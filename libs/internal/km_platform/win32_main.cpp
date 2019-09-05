#include "win32_main.h"

#include <stdio.h>
#include <stdarg.h>
#include <Xinput.h>
#include <intrin.h> // __rdtsc

#include <app_info.h>
#include <km_common/km_debug.h>
#include <km_common/km_input.h>
#include <km_common/km_log.h>
#include <km_common/km_string.h>
#include <opengl.h>

#include "win32_audio.h"


// TODO
// - WINDOWS 7 VIRTUAL MACHINE
// 
// - Getting a handle to our own exe file
// - Asset loading path
// - Threading (launch a thread)
// - Sleep/timeBeginPeriod (don't melt the processor)
// - ClipCursor() (multimonitor support)
// - WM_SETCURSOR (control cursor visibility)
// - QueryCancelAutoplay
// - WM_ACTIVATEAPP (for when we are not the active app)


#define START_WIDTH 1280
#define START_HEIGHT 720

// TODO this is a global for now
global_var char pathToApp_[MAX_PATH];
global_var bool32 running_ = true;
global_var GameInput* input_ = nullptr;             // for WndProc WM_CHAR
global_var glViewportFunc* glViewport_ = nullptr;   // for WndProc WM_SIZE
global_var ScreenInfo* screenInfo_ = nullptr;       // for WndProc WM_SIZE
global_var FixedArray<char, MAX_PATH> logFilePath_;

global_var WINDOWPLACEMENT DEBUGwpPrev = { sizeof(DEBUGwpPrev) };

// XInput functions
#define XINPUT_GET_STATE_FUNC(name) DWORD WINAPI name(DWORD dwUserIndex, \
	XINPUT_STATE *pState)
typedef XINPUT_GET_STATE_FUNC(XInputGetStateFunc);
XINPUT_GET_STATE_FUNC(XInputGetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
internal XInputGetStateFunc *xInputGetState_ = XInputGetStateStub;
#define XInputGetState xInputGetState_

#define XINPUT_SET_STATE_FUNC(name) DWORD WINAPI name(DWORD dwUserIndex, \
	XINPUT_VIBRATION *pVibration)
typedef XINPUT_SET_STATE_FUNC(XInputSetStateFunc);
XINPUT_SET_STATE_FUNC(XInputSetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
internal XInputSetStateFunc *xInputSetState_ = XInputSetStateStub;
#define XInputSetState xInputSetState_

// WGL functions
typedef BOOL WINAPI wglSwapIntervalEXTFunc(int interval);
global_var wglSwapIntervalEXTFunc* wglSwapInterval_ = NULL;
#define wglSwapInterval wglSwapInterval_

internal void Win32ToggleFullscreen(HWND hWnd, OpenGLFunctions* glFuncs)
{
	// This follows Raymond Chen's perscription for fullscreen toggling. See:
	// https://blogs.msdn.microsoft.com/oldnewthing/20100412-00/?p=14353

	DWORD dwStyle = GetWindowLong(hWnd, GWL_STYLE);
	if (dwStyle & WS_OVERLAPPEDWINDOW) {
		// Switch to fullscreen
		MONITORINFO monitorInfo = { sizeof(monitorInfo) };
		HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
		if (GetWindowPlacement(hWnd, &DEBUGwpPrev)
		&& GetMonitorInfo(hMonitor, &monitorInfo)) {
			SetWindowLong(hWnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(hWnd, HWND_TOP,
				monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
				monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			glFuncs->glViewport(
				(GLint)monitorInfo.rcMonitor.left,
				(GLint)monitorInfo.rcMonitor.top,
				(GLsizei)(monitorInfo.rcMonitor.right
					- monitorInfo.rcMonitor.left),
				(GLsizei)(monitorInfo.rcMonitor.bottom
					- monitorInfo.rcMonitor.top));
		}
	}
	else {
		// Switch to windowed
		SetWindowLong(hWnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(hWnd, &DEBUGwpPrev);
		SetWindowPos(hWnd, NULL, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
			| SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		RECT clientRect;
		GetClientRect(hWnd, &clientRect);
		glFuncs->glViewport(
			(GLint)clientRect.left, (GLint)clientRect.top,
			(GLsizei)(clientRect.right - clientRect.left),
			(GLsizei)(clientRect.bottom - clientRect.top));
	}
}

internal void RemoveFileNameFromPath(
	const char* filePath, char* dest, uint64 destLength)
{
	unsigned int lastSlash = 0;
	// TODO confused... some cross-platform code inside a win32 file
	//  maybe I meant to pull this out sometime?
#ifdef _WIN32
	char pathSep = '\\';
#else
	char pathSep = '/';
#endif
	while (filePath[lastSlash] != '\0') {
		lastSlash++;
	}
	// TODO unsafe!
	while (filePath[lastSlash] != pathSep) {
		lastSlash--;
	}
	if (lastSlash + 2 > destLength) {
		return;
	}
	for (unsigned int i = 0; i < lastSlash + 1; i++) {
		dest[i] = filePath[i];
	}
	dest[lastSlash + 1] = '\0';
}

internal void Win32GetExeFilePath(Win32State* state)
{
	DWORD size = GetModuleFileName(NULL,
		state->exeFilePath, sizeof(state->exeFilePath));
	if (size == 0) {
		LOG_ERROR("Failed to get EXE file path\n");
	}
	state->exeOnePastLastSlash = state->exeFilePath;
	for (char* scan = state->exeFilePath; *scan; scan++) {
		if (*scan == '\\') {
			state->exeOnePastLastSlash = scan + 1;
		}
	}
}
internal void Win32BuildExePathFileName(Win32State* state,
	const char* fileName,
	int destCount, char* dest)
{
	CatStrings(state->exeOnePastLastSlash - state->exeFilePath,
		state->exeFilePath, StringLength(fileName), fileName,
		destCount, dest);
}

internal void Win32GetInputFileLocation(
	Win32State* state, bool32 inputStream,
	int slotIndex, int destCount, char* dest)
{
	char temp[64];
	wsprintf(temp, "recording_%d_%s.kmi",
		slotIndex, inputStream ? "input" : "state");
	Win32BuildExePathFileName(state, temp, destCount, dest);
}

inline FILETIME Win32GetLastWriteTime(char* fileName)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesEx(fileName, GetFileExInfoStandard, &data)) {
		// TODO log?
		FILETIME zero = {};
		return zero;
	}
	return data.ftLastWriteTime;
}

internal Win32GameCode Win32LoadGameCode(
	char* gameCodeDLLPath, char* tempCodeDLLPath)
{
	Win32GameCode result = {};
	result.isValid = false;

	result.lastDLLWriteTime = Win32GetLastWriteTime(gameCodeDLLPath);
	CopyFile(gameCodeDLLPath, tempCodeDLLPath, FALSE);
	result.gameCodeDLL = LoadLibrary(tempCodeDLLPath);
	if (result.gameCodeDLL) {
		result.gameUpdateAndRender =
			(GameUpdateAndRenderFunc*)GetProcAddress(result.gameCodeDLL, "GameUpdateAndRender");

		result.isValid = result.gameUpdateAndRender != 0;
	}

	if (!result.isValid) {
		result.gameUpdateAndRender = 0;
	}

	return result;
}

internal void Win32UnloadGameCode(Win32GameCode* gameCode)
{
	if (gameCode->gameCodeDLL) {
		FreeLibrary(gameCode->gameCodeDLL);
	}

	gameCode->gameCodeDLL = NULL;
	gameCode->isValid = false;
	gameCode->gameUpdateAndRender = 0;
}

internal inline uint32 SafeTruncateUInt64(uint64 value)
{
	// TODO defines for maximum values
	DEBUG_ASSERT(value <= 0xFFFFFFFF);
	uint32 result = (uint32)value;
	return result;
}

//#if GAME_INTERNAL

DEBUG_PLATFORM_FREE_FILE_MEMORY_FUNC(DEBUGPlatformFreeFileMemory)
{
	if (file->data) {
		VirtualFree(file->data, 0, MEM_RELEASE);
	}
}

DEBUG_PLATFORM_READ_FILE_FUNC(DEBUGPlatformReadFile)
{
	DEBUGReadFileResult result = {};
	
	char fullPath[MAX_PATH];
	CatStrings(StringLength(pathToApp_), pathToApp_,
		StringLength(fileName), fileName, MAX_PATH, fullPath);

	HANDLE hFile = CreateFile(fullPath, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, NULL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		// TODO log
		return result;
	}

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(hFile, &fileSize)) {
		// TODO log
		return result;
	}

	uint32 fileSize32 = SafeTruncateUInt64(fileSize.QuadPart);
	result.data = VirtualAlloc(0, fileSize32,
		MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!result.data) {
		// TODO log
		return result;
	}

	DWORD bytesRead;
	if (!ReadFile(hFile, result.data, fileSize32, &bytesRead, NULL) ||
	fileSize32 != bytesRead) {
		// TODO log
		DEBUGPlatformFreeFileMemory(thread, &result);
		return result;
	}

	result.size = fileSize32;
	CloseHandle(hFile);
	return result;
}

DEBUG_PLATFORM_WRITE_FILE_FUNC(DEBUGPlatformWriteFile)
{
	HANDLE hFile = CreateFile(fileName, GENERIC_WRITE, NULL,
		NULL, OPEN_ALWAYS, NULL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		// TODO log
		return false;
	}

	if (!overwrite) {
		DWORD dwPos = SetFilePointer(hFile, 0, NULL, FILE_END);
		if (dwPos == INVALID_SET_FILE_POINTER) {
			// TODO GetLastError to make sure it's an error... ugh
		}
	}

	DWORD bytesWritten;
	if (!WriteFile(hFile, memory, (DWORD)memorySize, &bytesWritten, NULL)) {
		// TODO log
		return false;
	}

	CloseHandle(hFile);
	return bytesWritten == memorySize;
}

//#endif

void LogString(const char* string, uint64 n)
{
#if GAME_SLOW
	const int LOG_STRING_BUFFER_SIZE = 1024;
	char buffer[LOG_STRING_BUFFER_SIZE];
	uint64 remaining = n;
	while (remaining > 0) {
		uint64 copySize = remaining;
		if (copySize > LOG_STRING_BUFFER_SIZE - 1) {
			copySize = LOG_STRING_BUFFER_SIZE - 1;
		}
		MemCopy(buffer, string, copySize);
		buffer[copySize] = '\0';
		OutputDebugString(buffer);
		remaining -= copySize;
	}
#endif

	if (!DEBUGPlatformWriteFile(nullptr, logFilePath_.array.data, n, string, false)) {
		DEBUG_PANIC("failed to write to log file");
	}
}

PLATFORM_FLUSH_LOGS_FUNC(FlushLogs)
{
	// TODO fix this
	for (uint64 i = 0; i < logState->eventCount; i++) {
		uint64 eventIndex = (logState->eventFirst + i) % LOG_EVENTS_MAX;
		const LogEvent& event = logState->logEvents[eventIndex];
        uint64 bufferStart = event.logStart;
        uint64 bufferEnd = event.logStart + event.logSize;
        if (bufferEnd >= LOG_BUFFER_SIZE) {
            bufferEnd -= LOG_BUFFER_SIZE;
        }
        if (bufferEnd >= bufferStart) {
            LogString(logState->buffer + bufferStart, event.logSize);
        }
        else {
            LogString(logState->buffer + bufferStart, LOG_BUFFER_SIZE - bufferStart);
            LogString(logState->buffer, bufferEnd);
        }
	}

    logState->eventFirst = (logState->eventFirst + logState->eventCount) % LOG_EVENTS_MAX;
    logState->eventCount = 0;
	// uint64 toRead1, toRead2;
	// if (logState->readIndex <= logState->writeIndex) {
	// 	toRead1 = logState->writeIndex - logState->readIndex;
	// 	toRead2 = 0;
	// }
	// else {
	// 	toRead1 = LOG_BUFFER_SIZE - logState->readIndex;
	// 	toRead2 = logState->writeIndex;
	// }
	// if (toRead1 != 0) {
	// 	LogString(logState->buffer + logState->readIndex, toRead1);
	// }
	// if (toRead2 != 0) {
	// 	LogString(logState->buffer, toRead2);
	// }
	// logState->readIndex += toRead1 + toRead2;
	// if (logState->readIndex >= LOG_BUFFER_SIZE) {
	// 	logState->readIndex -= LOG_BUFFER_SIZE;
	// }
}

internal void Win32LoadXInput()
{
	HMODULE xInputLib = LoadLibrary("xinput1_4.dll");
	if (!xInputLib) {
		xInputLib = LoadLibrary("xinput1_3.dll");
	}
	if (!xInputLib) {
		// TODO warning message?
		xInputLib = LoadLibrary("xinput9_1_0.dll");
	}

	if (xInputLib) {
		XInputGetState = (XInputGetStateFunc*)GetProcAddress(xInputLib,
			"XInputGetState");
		XInputSetState = (XInputSetStateFunc*)GetProcAddress(xInputLib,
			"XInputSetState");
	}
}

LRESULT CALLBACK WndProc(
	HWND hWnd, UINT message,
	WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;

	switch (message) {
		case WM_ACTIVATEAPP: {
			// TODO handle
		} break;
		case WM_CLOSE: {
			// TODO handle this with a message?
			running_ = false;
		} break;
		case WM_DESTROY: {
			// TODO handle this as an error?
			running_ = false;
		} break;

		case WM_SIZE: {
			int width = LOWORD(lParam);
			int height = HIWORD(lParam);
			if (glViewport_) {
				glViewport_(0, 0, width, height);
			}
			if (screenInfo_) {
				screenInfo_->size.x = width;
				screenInfo_->size.y = height;
				screenInfo_->changed = true;
			}
		} break;

		case WM_SYSKEYDOWN: {
			DEBUG_PANIC("WM_SYSKEYDOWN in WndProc");
		} break;
		case WM_SYSKEYUP: {
			DEBUG_PANIC("WM_SYSKEYUP in WndProc");
		} break;
		case WM_KEYDOWN: {
		} break;
		case WM_KEYUP: {
		} break;

		case WM_CHAR: {
			char c = (char)wParam;
			input_->keyboardString[input_->keyboardStringLen++] = c;
			input_->keyboardString[input_->keyboardStringLen] = '\0';
		} break;

		default: {
			result = DefWindowProc(hWnd, message, wParam, lParam);
		} break;
	}

	return result;
}

internal int Win32KeyCodeToKM(int vkCode)
{
	// Numbers, letters, text
	if (vkCode >= 0x30 && vkCode <= 0x39) {
		return vkCode - 0x30 + KM_KEY_0;
	}
	else if (vkCode >= 0x41 && vkCode <= 0x5a) {
		return vkCode - 0x41 + KM_KEY_A;
	}
	else if (vkCode == VK_SPACE) {
		return KM_KEY_SPACE;
	}
	else if (vkCode == VK_BACK) {
		return KM_KEY_BACKSPACE;
	}
	// Arrow keys
	else if (vkCode == VK_UP) {
		return KM_KEY_ARROW_UP;
	}
	else if (vkCode == VK_DOWN) {
		return KM_KEY_ARROW_DOWN;
	}
	else if (vkCode == VK_LEFT) {
		return KM_KEY_ARROW_LEFT;
	}
	else if (vkCode == VK_RIGHT) {
		return KM_KEY_ARROW_RIGHT;
	}
	// Special keys
	else if (vkCode == VK_ESCAPE) {
		return KM_KEY_ESCAPE;
	}
	else if (vkCode == VK_SHIFT) {
		return KM_KEY_SHIFT;
	}
	else if (vkCode == VK_CONTROL) {
		return KM_KEY_CTRL;
	}
	else if (vkCode == VK_TAB) {
	   return KM_KEY_TAB;
	}
	else if (vkCode == VK_RETURN) {
		return KM_KEY_ENTER;
	}
	else if (vkCode >= 0x60 && vkCode <= 0x69) {
		return vkCode - 0x60 + KM_KEY_NUMPAD_0;
	}
	else {
		return -1;
	}
}

internal void Win32ProcessMessages(
	HWND hWnd, GameInput* gameInput,
	OpenGLFunctions* glFuncs)
{
	MSG msg;
	while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
		switch (msg.message) {
		case WM_QUIT: {
			running_ = false;
		} break;

		case WM_SYSKEYDOWN: {
			uint32 vkCode = (uint32)msg.wParam;
			bool32 altDown = (msg.lParam & (1 << 29));

			if (vkCode == VK_F4 && altDown) {
				running_ = false;
			}
		} break;
		case WM_SYSKEYUP: {
		} break;
		case WM_KEYDOWN: {
			uint32 vkCode = (uint32)msg.wParam;
			bool32 wasDown = ((msg.lParam & (1 << 30)) != 0);
			bool32 isDown = ((msg.lParam & (1 << 31)) == 0);
			int transitions = (wasDown != isDown) ? 1 : 0;
			DEBUG_ASSERT(isDown);

			int kmKeyCode = Win32KeyCodeToKM(vkCode);
			if (kmKeyCode != -1) {
				gameInput->keyboard[kmKeyCode].isDown = isDown;
				gameInput->keyboard[kmKeyCode].transitions = transitions;
			}

			if (vkCode == VK_ESCAPE) {
				// TODO eventually handle this in the game layer
				running_ = false;
			}
			if (vkCode == VK_F11) {
				Win32ToggleFullscreen(hWnd, glFuncs);
			}

			// Pass over to WndProc for WM_CHAR messages (string input)
			input_ = gameInput;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} break;
		case WM_KEYUP: {
			uint32 vkCode = (uint32)msg.wParam;
			bool32 wasDown = ((msg.lParam & (1 << 30)) != 0);
			bool32 isDown = ((msg.lParam & (1 << 31)) == 0);
			int transitions = (wasDown != isDown) ? 1 : 0;
			DEBUG_ASSERT(!isDown);

			int kmKeyCode = Win32KeyCodeToKM(vkCode);
			if (kmKeyCode != -1) {
				gameInput->keyboard[kmKeyCode].isDown = isDown;
				gameInput->keyboard[kmKeyCode].transitions = transitions;
			}
		} break;

		case WM_LBUTTONDOWN: {
			gameInput->mouseButtons[0].isDown = true;
			gameInput->mouseButtons[0].transitions = 1;
		} break;
		case WM_LBUTTONUP: {
			gameInput->mouseButtons[0].isDown = false;
			gameInput->mouseButtons[0].transitions = 1;
		} break;
		case WM_RBUTTONDOWN: {
			gameInput->mouseButtons[1].isDown = true;
			gameInput->mouseButtons[1].transitions = 1;
		} break;
		case WM_RBUTTONUP: {
			gameInput->mouseButtons[1].isDown = false;
			gameInput->mouseButtons[1].transitions = 1;
		} break;
		case WM_MBUTTONDOWN: {
			gameInput->mouseButtons[2].isDown = true;
			gameInput->mouseButtons[2].transitions = 1;
		} break;
		case WM_MBUTTONUP: {
			gameInput->mouseButtons[2].isDown = false;
			gameInput->mouseButtons[2].transitions = 1;
		} break;
		case WM_XBUTTONDOWN: {
			if ((msg.wParam & MK_XBUTTON1) != 0) {
				gameInput->mouseButtons[3].isDown = true;
				gameInput->mouseButtons[3].transitions = 1;
			}
			else if ((msg.wParam & MK_XBUTTON2) != 0) {
				gameInput->mouseButtons[4].isDown = true;
				gameInput->mouseButtons[4].transitions = 1;
			}
		} break;
		case WM_XBUTTONUP: {
			if ((msg.wParam & MK_XBUTTON1) != 0) {
				gameInput->mouseButtons[3].isDown = false;
				gameInput->mouseButtons[3].transitions = 1;
			}
			else if ((msg.wParam & MK_XBUTTON2) != 0) {
				gameInput->mouseButtons[4].isDown = false;
				gameInput->mouseButtons[4].transitions = 1;
			}
		} break;

		case WM_MOUSEWHEEL: {
			// TODO standardize these units
			gameInput->mouseWheel += GET_WHEEL_DELTA_WPARAM(msg.wParam);
		} break;

		default: {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} break;
		}
	}
}

internal void Win32ProcessXInputButton(
	DWORD xInputButtonState,
	GameButtonState *oldState,
	DWORD buttonBit,
	GameButtonState *newState)
{
	newState->isDown = ((xInputButtonState & buttonBit) == buttonBit);
	newState->transitions = (oldState->isDown != newState->isDown) ? 1 : 0;
}

internal float32 Win32ProcessXInputStickValue(SHORT value, SHORT deadZone)
{
	if (value < -deadZone) {
		return (float32)(value + deadZone) / (32768.0f - deadZone);
	}
	else if (value > deadZone) {
		return (float32)(value - deadZone) / (32767.0f - deadZone);
	}
	else {
		return 0.0f;
	}
}

internal void Win32RecordInputBegin(Win32State* state, int inputRecordingIndex)
{
	DEBUG_ASSERT(inputRecordingIndex < ARRAY_COUNT(state->replayBuffers));

	Win32ReplayBuffer* replayBuffer =
		&state->replayBuffers[inputRecordingIndex];
	if (replayBuffer->gameMemoryBlock) {
		state->inputRecordingIndex = inputRecordingIndex;

		char filePath[MAX_PATH];
		Win32GetInputFileLocation(state, true, inputRecordingIndex,
			sizeof(filePath), filePath);
		state->recordingHandle = CreateFile(filePath, GENERIC_WRITE,
			0, NULL, CREATE_ALWAYS, 0, NULL);

		MemCopy(replayBuffer->gameMemoryBlock,
			state->gameMemoryBlock, state->gameMemorySize);
	}
}
internal void Win32RecordInputEnd(Win32State* state)
{
	state->inputRecordingIndex = 0;
	CloseHandle(state->recordingHandle);
}
internal void Win32RecordInput(Win32State* state, GameInput* input)
{
	DWORD bytesWritten;
	WriteFile(state->recordingHandle,
		input, sizeof(*input), &bytesWritten, NULL);
}

internal void Win32PlaybackBegin(Win32State* state, int inputPlayingIndex)
{
	DEBUG_ASSERT(inputPlayingIndex < ARRAY_COUNT(state->replayBuffers));
	Win32ReplayBuffer* replayBuffer = &state->replayBuffers[inputPlayingIndex];

	if (replayBuffer->gameMemoryBlock)
	{
		state->inputPlayingIndex = inputPlayingIndex;

		char filePath[MAX_PATH];
		Win32GetInputFileLocation(state, true, inputPlayingIndex,
			sizeof(filePath), filePath);
		state->playbackHandle = CreateFile(filePath, GENERIC_READ,
			0, NULL, OPEN_EXISTING, 0, NULL);

		MemCopy(state->gameMemoryBlock, replayBuffer->gameMemoryBlock, state->gameMemorySize);
	}
}
internal void Win32PlaybackEnd(Win32State* state)
{
	state->inputPlayingIndex = 0;
	CloseHandle(state->playbackHandle);
}
internal void Win32PlayInput(Win32State* state, GameInput* input)
{
	DWORD bytesRead;
	if (ReadFile(state->playbackHandle,
	input, sizeof(*input), &bytesRead, NULL)) {
		if (bytesRead == 0) {
			int playingIndex = state->inputPlayingIndex;
			Win32PlaybackEnd(state);
			Win32PlaybackBegin(state, playingIndex);
		}
	}
}

#define LOAD_GL_FUNCTION(name) \
	glFuncs->name = (name##Func*)wglGetProcAddress(#name); \
	if (!glFuncs->name) { \
		glFuncs->name = (name##Func*)GetProcAddress(oglLib, #name); \
		if (!glFuncs->name) { \
			LOG_ERROR("OpenGL function load failed: %s", #name); \
		} \
	}

internal bool Win32LoadBaseGLFunctions(
	OpenGLFunctions* glFuncs, const HMODULE& oglLib)
{
	// Generate function loading code
#define FUNC(returntype, name, ...) LOAD_GL_FUNCTION(name);
	GL_FUNCTIONS_BASE
#undef FUNC

	return true;
}

internal bool Win32LoadAllGLFunctions(
	OpenGLFunctions* glFuncs, const HMODULE& oglLib)
{
	// Generate function loading code
#define FUNC(returntype, name, ...) LOAD_GL_FUNCTION(name);
	GL_FUNCTIONS_ALL
#undef FUNC

	return true;
}

internal bool Win32InitOpenGL(OpenGLFunctions* glFuncs,
	int width, int height)
{
	HMODULE oglLib = LoadLibrary("opengl32.dll");
	if (!oglLib) {
		LOG_ERROR("Failed to load opengl32.dll\n");
		return false;
	}

	if (!Win32LoadBaseGLFunctions(glFuncs, oglLib)) {
		// TODO logging (base GL loading failed, but context exists. weirdness)
		return false;
	}
	glViewport_ = glFuncs->glViewport;

	glFuncs->glViewport(0, 0, width, height);

	// Set v-sync
	// NOTE this isn't technically complete. we have to ask Windows
	// if the extensions are loaded, and THEN look for the
	// extension function name
	wglSwapInterval = (wglSwapIntervalEXTFunc*)
		wglGetProcAddress("wglSwapIntervalEXT");
	if (wglSwapInterval) {
		wglSwapInterval(1);
	}
	else {
		// TODO no vsync. logging? just exit? just exit for now
		return false;
	}

	const GLubyte* vendorString = glFuncs->glGetString(GL_VENDOR);
	LOG_INFO("GL_VENDOR: %s\n", vendorString);
	const GLubyte* rendererString = glFuncs->glGetString(GL_RENDERER);
	LOG_INFO("GL_RENDERER: %s\n", rendererString);
	const GLubyte* versionString = glFuncs->glGetString(GL_VERSION);
	LOG_INFO("GL_VERSION: %s\n", versionString);

	int32 majorVersion = versionString[0] - '0';
	int32 minorVersion = versionString[2] - '0';

	if (majorVersion < 3 || (majorVersion == 3 && minorVersion < 3)) {
		// TODO logging. opengl version is less than 3.3
		return false;
	}

	if (!Win32LoadAllGLFunctions(glFuncs, oglLib)) {
		// TODO logging (couldn't load all functions, but version is 3.3+)
		return false;
	}

	const GLubyte* glslString =
		glFuncs->glGetString(GL_SHADING_LANGUAGE_VERSION);
	LOG_INFO("GL_SHADING_LANGUAGE_VERSION: %s\n", glslString);

	return true;
}

internal bool Win32CreateRC(HWND hWnd,
	BYTE colorBits, BYTE alphaBits, BYTE depthBits, BYTE stencilBits)
{
	// TODO these calls in total add about 40MB to program memory
	//      ...why?

	HDC hDC = GetDC(hWnd);
	if (!hDC) {
		// TODO log
		return false;
	}

	// Define and set pixel format
	PIXELFORMATDESCRIPTOR desiredPFD = { sizeof(desiredPFD) };
	desiredPFD.nVersion = 1;
	desiredPFD.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW
		| PFD_DOUBLEBUFFER;
	desiredPFD.iPixelType = PFD_TYPE_RGBA;
	desiredPFD.cColorBits = colorBits;
	desiredPFD.cAlphaBits = alphaBits;
	desiredPFD.cDepthBits = depthBits;
	desiredPFD.cStencilBits = stencilBits;
	desiredPFD.iLayerType = PFD_MAIN_PLANE;
	int pixelFormat = ChoosePixelFormat(hDC, &desiredPFD);
	if (!pixelFormat) {
		DWORD error = GetLastError();
		LOG_ERROR("ChoosePixelFormat error, code %d\n", error);
		return false;
	}

	PIXELFORMATDESCRIPTOR suggestedPFD = {};
	DescribePixelFormat(hDC, pixelFormat, sizeof(suggestedPFD), &suggestedPFD);
	if (!SetPixelFormat(hDC, pixelFormat, &suggestedPFD)) {
		DWORD error = GetLastError();
		LOG_ERROR("SetPixelFormat error, code %d\n", error);
		return false;
	}

	// Create and attach OpenGL rendering context to this thread
	HGLRC hGLRC = wglCreateContext(hDC);
	if (!hGLRC) {
		DWORD error = GetLastError();
		LOG_ERROR("wglCreateContext error, code %d\n", error);
		return false;
	}
	if (!wglMakeCurrent(hDC, hGLRC)) {
		DWORD error = GetLastError();
		LOG_ERROR("wglMakeCurrent error, code %d\n", error);
		return false;
	}

	ReleaseDC(hWnd, hDC);
	return true;
}

internal HWND Win32CreateWindow(
	HINSTANCE hInstance,
	const char* className, const char* windowName,
	int x, int y, int clientWidth, int clientHeight)
{
	WNDCLASSEX wndClass = { sizeof(wndClass) };
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = WndProc;
	wndClass.hInstance = hInstance;
	//wndClass.hIcon = NULL;
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.lpszClassName = className;

	if (!RegisterClassEx(&wndClass)) {
		// TODO log
		return NULL;
	}

	RECT windowRect     = {};
	windowRect.left     = x;
	windowRect.top      = y;
	windowRect.right    = x + clientWidth;
	windowRect.bottom   = y + clientHeight;

	if (!AdjustWindowRectEx(&windowRect, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
	FALSE, 0)) {
		// TODO log
		GetLastError();
		return NULL;
	}

	HWND hWindow = CreateWindowEx(
		0,
		className,
		windowName,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		windowRect.left, windowRect.top,
		windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
		0,
		0,
		hInstance,
		0);

	if (!hWindow) {
		// TODO log
		return NULL;
	}

	return hWindow;
}

int CALLBACK WinMain(
	HINSTANCE hInstance, HINSTANCE hPrevInst,
	LPSTR cmdline, int cmd_show)
{
	logFilePath_.Init();
	SYSTEMTIME systemTime;
	GetLocalTime(&systemTime);
	int n = snprintf(logFilePath_.array.data, MAX_PATH,
		"logs/log%04d-%02d-%02d_%02d-%02d-%02d.txt",
		systemTime.wYear, systemTime.wMonth, systemTime.wDay,
		systemTime.wHour, systemTime.wMinute, systemTime.wSecond);
	logFilePath_.array.size += n;

	LogState* logState = (LogState*)VirtualAlloc(0, sizeof(LogState),
		MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!logState) {
		LOG_ERROR("Log state memory allocation failed\n");
		FlushLogs(logState);
		return 1;
	}
	logState->eventFirst = 0;
	logState->eventCount = 0;
	logState_ = logState;

	Win32State state = {};
	Win32GetExeFilePath(&state);
	RemoveFileNameFromPath(state.exeFilePath, pathToApp_, MAX_PATH);
	LOG_INFO("Path to executable: %s\n", pathToApp_);

	Win32LoadXInput();

	// Create window
	HWND hWnd = Win32CreateWindow(hInstance,
		"OpenGLWindowClass", APP_NAME,
		100, 100, START_WIDTH, START_HEIGHT);
	if (!hWnd) {
		LOG_ERROR("Win32 create window failed\n");
		FlushLogs(logState);
		return 1;
	}
	LOG_INFO("Created Win32 window\n");

	RECT clientRect;
	GetClientRect(hWnd, &clientRect);
	ScreenInfo screenInfo = {};
	screenInfo.changed = true;
	screenInfo.size.x = clientRect.right - clientRect.left;
	screenInfo.size.y = clientRect.bottom - clientRect.top;
	// TODO is cColorBits ACTUALLY excluding alpha bits? Doesn't seem like it
	screenInfo.colorBits = 32;
	screenInfo.alphaBits = 8;
	screenInfo.depthBits = 24;
	screenInfo.stencilBits = 0;
	screenInfo_ = &screenInfo;

	// Create and attach rendering context for OpenGL
	if (!Win32CreateRC(hWnd, screenInfo.colorBits, screenInfo.alphaBits,
	screenInfo.depthBits, screenInfo.stencilBits)) {
		LOG_ERROR("Win32 create RC failed\n");
		FlushLogs(logState);
		return 1;
	}
	LOG_INFO("Created Win32 OpenGL rendering context\n");

	PlatformFunctions platformFuncs = {};
	platformFuncs.flushLogs = FlushLogs;
	platformFuncs.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
	platformFuncs.DEBUGPlatformReadFile = DEBUGPlatformReadFile;
	platformFuncs.DEBUGPlatformWriteFile = DEBUGPlatformWriteFile;

	// Initialize OpenGL
	if (!Win32InitOpenGL(&platformFuncs.glFunctions,
	screenInfo.size.x, screenInfo.size.y)) {
		LOG_ERROR("Win32 OpenGL init failed\n");
		FlushLogs(logState);
		return 1;
	}
	LOG_INFO("Initialized Win32 OpenGL\n");

	// Try to get monitor refresh rate
	// TODO test how reliable this is
	DEVMODE devmode;
	devmode.dmSize = sizeof(DEVMODE);
	devmode.dmDriverExtra = 0;
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode);
	int monitorRefreshHz = (int)devmode.dmDisplayFrequency;
	LOG_INFO("Refresh rate: %d\n", monitorRefreshHz);

	// Initialize audio
	Win32Audio winAudio = {};
	if (!Win32InitAudio(&winAudio, AUDIO_DEFAULT_BUFFER_SIZE_MILLISECONDS)) {
		LOG_ERROR("Win32 audio init failed\n");
		FlushLogs(logState);
		return 1;
	}
	winAudio.latency = winAudio.sampleRate / monitorRefreshHz * 2;

	GameAudio gameAudio = {};
	gameAudio.sampleRate = winAudio.sampleRate;
	//gameAudio.channels = winAudio.channels;
	gameAudio.channels = 2;
	gameAudio.bufferSizeSamples = winAudio.bufferSizeSamples;
	int bufferSizeBytes = gameAudio.bufferSizeSamples
		* gameAudio.channels * sizeof(float32);
	gameAudio.buffer = (float32*)VirtualAlloc(0, (size_t)bufferSizeBytes,
		MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!gameAudio.buffer) {
		LOG_ERROR("Win32 audio memory allocation failed\n");
		FlushLogs(logState);
		return 1;
	}
	gameAudio.sampleDelta = 0; // TODO revise this
	LOG_INFO("Initialized Win32 audio\n");

#if GAME_INTERNAL
	LPVOID baseAddress = (LPVOID)TERABYTES((uint64)2);;
#else
	LPVOID baseAddress = 0;
#endif

	GameMemory gameMemory = {};
	gameMemory.shouldInitGlobalVariables = true;

	gameMemory.permanent.size = MEGABYTES(64);
	gameMemory.transient.size = GIGABYTES(2);

	// TODO Look into using large virtual pages for this
	// potentially big allocation
	uint64 totalSize = gameMemory.permanent.size + gameMemory.transient.size;
	// TODO check allocation fail?
	gameMemory.permanent.memory = VirtualAlloc(baseAddress, (size_t)totalSize,
		MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!gameMemory.permanent.memory) {
		LOG_ERROR("Win32 memory allocation failed\n");
		FlushLogs(logState);
		return 1;
	}
	gameMemory.transient.memory = ((uint8*)gameMemory.permanent.memory +
		gameMemory.permanent.size);

	state.gameMemorySize = totalSize;
	state.gameMemoryBlock = gameMemory.permanent.memory;
	LOG_INFO("Initialized game memory\n");

#if 0
#if GAME_INTERNAL
	for (int replayIndex = 0;
	replayIndex < ARRAY_COUNT(state.replayBuffers);
	replayIndex++) {
		Win32ReplayBuffer* replayBuffer = &state.replayBuffers[replayIndex];

		Win32GetInputFileLocation(&state, false, replayIndex,
			sizeof(replayBuffer->filePath), replayBuffer->filePath);

		replayBuffer->fileHandle = CreateFile(replayBuffer->filePath,
			GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, NULL);

		LARGE_INTEGER maxSize;
		maxSize.QuadPart = state.gameMemorySize;
		replayBuffer->memoryMap = CreateFileMapping(
			replayBuffer->fileHandle, 0, PAGE_READWRITE,
			maxSize.HighPart, maxSize.LowPart, NULL);
		replayBuffer->gameMemoryBlock = MapViewOfFile(
			replayBuffer->memoryMap, FILE_MAP_ALL_ACCESS, 0, 0,
			state.gameMemorySize);

		// TODO possibly check if this memory block "allocation" fails
		// Debug, so not really that important
	}
	LOG_INFO("Initialized input replay system\n");
#endif
#endif

	char dllName[MAX_PATH];
	CatStrings(StringLength(APP_NAME), APP_NAME, StringLength("_game.dll"), "_game.dll",
		MAX_PATH, dllName);
	char dllTempName[MAX_PATH];
	CatStrings(StringLength(APP_NAME), APP_NAME, StringLength("_game_temp.dll"), "_game_temp.dll",
		MAX_PATH, dllTempName);

	char gameCodeDLLPath[MAX_PATH];
	Win32BuildExePathFileName(&state, dllName,
		MAX_PATH, gameCodeDLLPath);
	char tempCodeDLLPath[MAX_PATH];
	Win32BuildExePathFileName(&state, dllTempName,
		MAX_PATH, tempCodeDLLPath);

	GameInput input[2] = {};
	GameInput *newInput = &input[0];
	GameInput *oldInput = &input[1];

	// Initialize timing information
	int64 timerFreq;
	{
		LARGE_INTEGER timerFreqResult;
		QueryPerformanceFrequency(&timerFreqResult);
		timerFreq = timerFreqResult.QuadPart;
	}

	/*LARGE_INTEGER timerBegin;
	QueryPerformanceCounter(&timerBegin);
	float64 soundBegin = 0.0f;*/

	LARGE_INTEGER timerLast;
	QueryPerformanceCounter(&timerLast);
	uint64 cyclesLast = __rdtsc();

	Win32GameCode gameCode =
		Win32LoadGameCode(gameCodeDLLPath, tempCodeDLLPath);

	FlushLogs(logState);
	running_ = true;
	while (running_) {
		// TODO this gets called twice very quickly in succession
		FILETIME newDLLWriteTime = Win32GetLastWriteTime(gameCodeDLLPath);
		if (CompareFileTime(&newDLLWriteTime,
		&gameCode.lastDLLWriteTime) > 0) {
			Win32UnloadGameCode(&gameCode);
			gameCode = Win32LoadGameCode(gameCodeDLLPath, tempCodeDLLPath);
			gameMemory.shouldInitGlobalVariables = true;
		}

		// Process keyboard input & other messages
		int mouseWheelPrev = newInput->mouseWheel;
		Win32ProcessMessages(hWnd, newInput, &platformFuncs.glFunctions);
		newInput->mouseWheelDelta = newInput->mouseWheel - mouseWheelPrev;

		POINT mousePos;
		GetCursorPos(&mousePos);
		ScreenToClient(hWnd, &mousePos);
		Vec2Int mousePosPrev = newInput->mousePos;
		newInput->mousePos.x = mousePos.x;
		newInput->mousePos.y = screenInfo_->size.y - mousePos.y;
		newInput->mouseDelta = newInput->mousePos - mousePosPrev;
		if (mousePos.x < 0 || mousePos.x > screenInfo_->size.x
		|| mousePos.y < 0 || mousePos.y > screenInfo_->size.y) {
			for (int i = 0; i < 5; i++) {
				int transitions = newInput->mouseButtons[i].isDown ? 1 : 0;
				newInput->mouseButtons[i].isDown = false;
				newInput->mouseButtons[i].transitions = transitions;
			}
		}

		// MIDI input
		if (!winAudio.midiInBusy) {
			winAudio.midiInBusy = true;
			newInput->midiIn = winAudio.midiIn;
			winAudio.midiIn.numMessages = 0;
			winAudio.midiInBusy = false;
		}

		DWORD maxControllerCount = XUSER_MAX_COUNT;
		if (maxControllerCount > ARRAY_COUNT(newInput->controllers)) {
			maxControllerCount = ARRAY_COUNT(newInput->controllers);
		}
		// TODO should we poll this more frequently?
		for (DWORD controllerInd = 0;
		controllerInd < maxControllerCount;
		controllerInd++) {
			GameControllerInput *oldController =
				&oldInput->controllers[controllerInd];
			GameControllerInput *newController =
				&newInput->controllers[controllerInd];

			XINPUT_STATE controllerState;
			// TODO the get state function has a really bad performance bug
			// which causes it to stall for a couple ms if a controller
			// isn't connected
			if (XInputGetState(controllerInd, &controllerState) == ERROR_SUCCESS) {
				newController->isConnected = true;

				// TODO check controller_state.dwPacketNumber
				XINPUT_GAMEPAD *pad = &controllerState.Gamepad;

				Win32ProcessXInputButton(pad->wButtons,
					&oldController->a, XINPUT_GAMEPAD_A,
					&newController->a);
				Win32ProcessXInputButton(pad->wButtons,
					&oldController->b, XINPUT_GAMEPAD_B,
					&newController->b);
				Win32ProcessXInputButton(pad->wButtons,
					&oldController->x, XINPUT_GAMEPAD_X,
					&newController->x);
				Win32ProcessXInputButton(pad->wButtons,
					&oldController->y, XINPUT_GAMEPAD_Y,
					&newController->y);
				Win32ProcessXInputButton(pad->wButtons,
					&oldController->lShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER,
					&newController->lShoulder);
				Win32ProcessXInputButton(pad->wButtons,
					&oldController->rShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER,
					&newController->rShoulder);

				newController->leftStart = oldController->leftEnd;
				newController->rightStart = oldController->rightEnd;

				// TODO check if the deadzone is round
				newController->leftEnd.x = Win32ProcessXInputStickValue(
					pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
				newController->leftEnd.y = Win32ProcessXInputStickValue(
					pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
				newController->rightEnd.x = Win32ProcessXInputStickValue(
					pad->sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
				newController->rightEnd.y = Win32ProcessXInputStickValue(
					pad->sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

#if 0
				XINPUT_VIBRATION vibration;
				vibration.wLeftMotorSpeed = 4000;
				vibration.wRightMotorSpeed = 10000;
				XInputSetState(controllerInd, &vibration);
#endif
			}
			else {
				newController->isConnected = false;
			}
		}

#if 0
#if GAME_INTERNAL
		// Recording game input
		if (newInput->controllers[0].lShoulder.isDown
		&& newInput->controllers[0].lShoulder.transitions > 0
		&& state.inputPlayingIndex == 0) {
			if (state.inputRecordingIndex == 0) {
				Win32RecordInputBegin(&state, 1);
				LOG_INFO("RECORDING - START\n");
			}
			else {
				Win32RecordInputEnd(&state);
				LOG_INFO("RECORDING - STOP\n");
			}
		}
		if (newInput->controllers[0].rShoulder.isDown
		&& newInput->controllers[0].rShoulder.transitions > 0
		&& state.inputRecordingIndex == 0) {
			if (state.inputPlayingIndex == 0) {
				Win32PlaybackBegin(&state, 1);
				LOG_INFO("-> PLAYING - START\n");
			}
			else {
				Win32PlaybackEnd(&state);
				LOG_INFO("-> PLAYING - STOP\n");
			}
		}

		if (state.inputRecordingIndex) {
			Win32RecordInput(&state, newInput);
		}
		if (state.inputPlayingIndex) {
			Win32PlayInput(&state, newInput);
		}

		if (newInput->controllers[0].y.isDown
		&& newInput->controllers[0].y.transitions > 0) {
			gameMemory.isInitialized = false;
		}
#endif
#endif

		/*HRESULT hr;
		UINT64 audioClockFreq;
		UINT64 audioClockPos;
		hr = winAudio.audioClock->GetFrequency(&audioClockFreq);
		hr = winAudio.audioClock->GetPosition(&audioClockPos, NULL);
		float64 secondsPlayed = (float64)audioClockPos / audioClockFreq;*/

		LARGE_INTEGER timerEnd;
		QueryPerformanceCounter(&timerEnd);
		uint64 cyclesEnd = __rdtsc();
		int64 timerElapsed = timerEnd.QuadPart - timerLast.QuadPart;
		// NOTE this is an estimate for next frame, based on last frame time
		float32 elapsed = (float32)timerElapsed / timerFreq;
		//int64 cyclesElapsed = cyclesEnd - cyclesLast;
		//int mCyclesPerFrame = (int)(cyclesElapsed / (1000 * 1000));
		timerLast = timerEnd;
		cyclesLast = cyclesEnd;

		/*float64 totalElapsedSound = secondsPlayed - soundBegin;
		int64 timerTotal = timerEnd.QuadPart - timerBegin.QuadPart;
		float64 totalElapsedTime = (float64)timerTotal / timerFreq;
		LOG_INFO("sound time sync offset (real minus sound): %f\n",
			totalElapsedTime - totalElapsedSound);*/

		if (gameCode.gameUpdateAndRender) {
			ThreadContext thread = {};
			gameAudio.fillLength = winAudio.latency;
			gameCode.gameUpdateAndRender(&thread, &platformFuncs,
				newInput, screenInfo, elapsed,
				&gameMemory, &gameAudio, logState);
			screenInfo.changed = false;
		}
		else {
			gameAudio.fillLength = 0;
		}

		UINT32 audioPadding;
		HRESULT hr = winAudio.audioClient->GetCurrentPadding(&audioPadding);
		// TODO check for invalid device and stuff
		if (SUCCEEDED(hr)) {
			int samplesQueued = (int)audioPadding;
			if (samplesQueued < winAudio.latency) {
				// Write enough samples so that the number of queued samples
				// is enough for one latency interval
				int samplesToWrite = winAudio.latency - samplesQueued;
				if (samplesToWrite > gameAudio.fillLength) {
					// Don't write more samples than the game generated
					samplesToWrite = gameAudio.fillLength;
				}

				Win32WriteAudioSamples(&winAudio, &gameAudio, samplesToWrite);
				gameAudio.sampleDelta = samplesToWrite;
			}
		}

		FlushLogs(logState);

		// NOTE
		// SwapBuffers seems to effectively stall for vsync target time
		// It's probably more complicated than that under the hood,
		// but it does (I believe) effectively sleep your thread and yield
		// CPU time to other processes until the vsync target time is hit
		HDC hDC = GetDC(hWnd);
		SwapBuffers(hDC);
		ReleaseDC(hWnd, hDC);

		GameInput *temp = newInput;
		newInput = oldInput;
		oldInput = temp;
		ClearInput(newInput, oldInput);
	}

	Win32StopAudio(&winAudio);

	FlushLogs(logState);

	return 0;
}

#include "win32_audio.cpp"

// TODO temporary! this is a bad idea! already compiled in main.cpp
#include <km_common/km_input.cpp>
#include <km_common/km_string.cpp>
#include <km_common/km_lib.cpp>
#include <km_common/km_log.cpp>
