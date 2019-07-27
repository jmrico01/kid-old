#include "macos_main.h"

#undef internal
#include <Cocoa/Cocoa.h>
#define internal static

#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>

#include <mach/mach_time.h>
#include <sys/mman.h>       // memory functions
#include <sys/stat.h>       // file stat functions
#include <fcntl.h>          // file open/close functions
#include <dlfcn.h>          // dynamic linking functions

#include <km_common/km_defines.h>
#include <km_common/km_debug.h>
#include <km_common/km_string.h>

#include "macos_audio.h"

// Let the command line override
//#ifndef HANDMADE_USE_VSYNC
//#define HANDMADE_USE_VSYNC 1
//#endif

global_var bool32 running_;
global_var NSOpenGLContext* glContext_;
global_var char pathToApp_[MACOS_STATE_FILE_NAME_COUNT];
global_var mach_timebase_info_data_t machTimebaseInfo_;

internal inline uint32 SafeTruncateUInt64(uint64 value)
{
	// TODO defines for maximum values
	DEBUG_ASSERT(value <= 0xFFFFFFFF);
	uint32 result = (uint32)value;
	return result;
}

// TODO maybe factor out all unix-style shared functions
// (e.g. library loading, file i/o)

// NOTE Explicit wrappers around dlsym, dlopen and dlclose
internal void* MacOSLoadFunction(void* libHandle, const char* name)
{
	void* symbol = dlsym(libHandle, name);
	if (!symbol) {
		LOG_INFO("dlsym failed: %s\n", dlerror());
	}
	// TODO(michiel): Check if lib with underscore exists?!
	return symbol;
}

internal void* MacOSLoadLibrary(const char* libName)
{
	void* handle = dlopen(libName, RTLD_NOW | RTLD_LOCAL);
	if (!handle) {
		LOG_INFO("dlopen failed: %s\n", dlerror());
	}
	return handle;
}

internal void MacOSUnloadLibrary(void* handle)
{
	if (handle != NULL) {
		dlclose(handle);
		handle = NULL;
	}
}

internal inline ino_t MacOSFileId(const char* fileName)
{
	struct stat attr = {};
	if (stat(fileName, &attr)) {
		attr.st_ino = 0;
	}

	return attr.st_ino;
}

//#if GAME_INTERNAL

DEBUG_PLATFORM_PRINT_FUNC(DEBUGPlatformPrint)
{
	NSString* formatStr = [NSString stringWithUTF8String:format];
	va_list args;
	va_start(args, format);
	NSLogv(formatStr, args);
	va_end(args);
}

DEBUG_PLATFORM_FLUSH_GL_FUNC(DEBUGPlatformFlushGl)
{
	[glContext_ flushBuffer];
}

DEBUG_PLATFORM_FREE_FILE_MEMORY_FUNC(DEBUGPlatformFreeFileMemory)
{
	if (file->data) {
		munmap(file->data, file->size);
		file->data = 0;
	}
	file->size = 0;
}

DEBUG_PLATFORM_READ_FILE_FUNC(DEBUGPlatformReadFile)
{
	DEBUGReadFileResult result = {};

	char fullPath[MACOS_STATE_FILE_NAME_COUNT];
	CatStrings(StringLength(pathToApp_), pathToApp_,
		StringLength(fileName), fileName,
		MACOS_STATE_FILE_NAME_COUNT, fullPath);
	int32 fileHandle = open(fullPath, O_RDONLY);
	if (fileHandle >= 0) {
		off_t fileSize64 = lseek(fileHandle, 0, SEEK_END);
		lseek(fileHandle, 0, SEEK_SET);

		if (fileSize64 > 0) {
			uint32 fileSize32 = SafeTruncateUInt64(fileSize64);
			result.data = mmap(NULL, fileSize32, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			if (result.data) {
				ssize_t bytesRead = read(fileHandle,
					result.data, fileSize64);
				if ((ssize_t)fileSize32 == bytesRead) {
					// NOTE(casey): File read successfully
					result.size = fileSize32;
				}
				else {
					// TODO(michiel): Logging
					DEBUGPlatformFreeFileMemory(thread, &result);
				}
			}
		}
		else {
			// TODO(michiel): Logging
		}

		close(fileHandle);
	}
	else {
		// TODO(casey): Logging
	}

	return result;
}


DEBUG_PLATFORM_WRITE_FILE_FUNC(DEBUGPlatformWriteFile)
{
	bool32 result = false;

	char fullPath[MACOS_STATE_FILE_NAME_COUNT];
	CatStrings(StringLength(pathToApp_), pathToApp_,
		StringLength(fileName), fileName,
		MACOS_STATE_FILE_NAME_COUNT, fullPath);
	int32 fileHandle = open(fullPath, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (fileHandle >= 0) {
		ssize_t bytesWritten = write(fileHandle, memory, memorySize);
		if (fsync(fileHandle) >= 0) {
			result = (bytesWritten == (ssize_t)memorySize);
		}
		else {
			// TODO(casey): Logging
		}

		close(fileHandle);
	}
	else {
		// TODO(casey): Logging
	}

	return result;
}

//#endif

#define LOAD_GL_FUNCTION(name) \
	glFuncs->name = (name##Func*)MacOSLoadFunction(glLib, #name); \
	if (!glFuncs->name) { \
		DEBUG_PANIC("OpenGL function load failed: %s\n", #name); \
	}

internal bool32 MacOSInitOpenGL(
	OpenGLFunctions* glFuncs,
	int width, int height)
{
	void* glLib = MacOSLoadLibrary("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL");
	if (!glLib) {
		LOG_INFO("Failed to load OpenGL library\n");
		return false;
	}

	// Generate function loading code
#define FUNC(returntype, name, ...) LOAD_GL_FUNCTION(name);
	GL_FUNCTIONS_BASE
#undef FUNC

	const GLubyte* versionString = glFuncs->glGetString(GL_VERSION);
	LOG_INFO("GL_VERSION: %s\n", versionString);
	LOG_INFO("GL_VENDOR: %s\n", glFuncs->glGetString(GL_VENDOR));
	LOG_INFO("GL_RENDERER: %s\n", glFuncs->glGetString(GL_RENDERER));

	int32 majorVersion = versionString[0] - '0';
	int32 minorVersion = versionString[2] - '0';

	if (majorVersion < 3 || (majorVersion == 3 && minorVersion < 3)) {
		// TODO logging. opengl version is less than 3.3
		return false;
	}

	// Generate function loading code
#define FUNC(returntype, name, ...) LOAD_GL_FUNCTION(name);
	GL_FUNCTIONS_ALL
#undef FUNC

	LOG_INFO("GL_SHADING_LANGUAGE_VERSION: %s\n", 
		glFuncs->glGetString(GL_SHADING_LANGUAGE_VERSION));

	return true;
}

// Dynamic code loading
internal bool32 MacOSLoadGameCode(
	MacOSGameCode* gameCode, const char* libName, ino_t fileId)
{
	if (gameCode->gameLibId != fileId) {
		MacOSUnloadLibrary(gameCode->gameLibHandle);
		gameCode->gameLibId = fileId;
		gameCode->isValid = false;

		gameCode->gameLibHandle = MacOSLoadLibrary(libName);
		if (gameCode->gameLibHandle) {
			*(void**)(&gameCode->gameUpdateAndRender) = MacOSLoadFunction(
				gameCode->gameLibHandle, "GameUpdateAndRender");
			if (gameCode->gameUpdateAndRender) {
				gameCode->isValid = true;
			}
		}
	}

	if (!gameCode->isValid) {
		MacOSUnloadLibrary(gameCode->gameLibHandle);
		gameCode->gameLibId = 0;
		gameCode->gameUpdateAndRender = 0;
	}

	return gameCode->isValid;
}

/*internal void MacOSUnloadGameCode(MacOSGameCode *gameCode)
{
	MacOSUnloadLibrary(gameCode->gameLibHandle);
	gameCode->gameLibId = 0;
	gameCode->isValid = false;
	gameCode->gameUpdateAndRender = 0;
}*/


@interface KMAppDelegate : NSObject<NSApplicationDelegate, NSWindowDelegate>
@end

@implementation KMAppDelegate

- (void)applicationDidFinishLaunching:(id)sender
{
	#pragma unused(sender)
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
	#pragma unused(sender)
	return YES;
}

- (void)applicationWillTerminate:(NSApplication*)sender
{
	#pragma unused(sender)
}

- (void)windowWillClose:(id)sender
{
	running_ = false;
}

#if 0
- (void)windowDidResize:(NSNotification*)notification
{
	NSWindow* window = [notification object];

	//Assert(GlobalGLContext == [NSOpenGLContext currentContext]);
	//[GlobalGLContext update];
	[GlobalGLContext makeCurrentContext];
	[GlobalGLContext update];

	// OpenGL reshape. Typically done in the view
	//glLoadIdentity();

	NSRect ContentViewFrame = [[window contentView] frame];
	glViewport(0, 0, ContentViewFrame.size.width, ContentViewFrame.size.height);
	[GlobalGLContext update];
}
#endif

@end

void MacOSCreateMainMenu()
{
	// Create the Menu. Two options right now:
	//	Toggle Full Screen
	//  Quit
	NSMenu* menu = [NSMenu new];

	NSMenuItem* appMenuItem = [NSMenuItem new];
	[menu addItem:appMenuItem];

	[NSApp setMainMenu:menu];

	NSMenu* appMenu = [NSMenu new];

	//NSString* appName = [[NSProcessInfo processInfo] processName];
	NSString* appName = @"kid";

	NSString* toggleFullScreenTitle = @"Toggle Full Screen";
	NSMenuItem* toggleFullScreenMenuItem = [[NSMenuItem alloc]
		initWithTitle:toggleFullScreenTitle
		action:@selector(toggleFullScreen:)
		keyEquivalent:@"f"];
	[appMenu addItem:toggleFullScreenMenuItem];

	NSString* quitTitle = [@"Quit " stringByAppendingString:appName];
	NSMenuItem* quitMenuItem = [[NSMenuItem alloc] initWithTitle:quitTitle
		action:@selector(terminate:)
		keyEquivalent:@"q"];
	[appMenu addItem:quitMenuItem];
	[appMenuItem setSubmenu:appMenu];
}

internal KeyInputCode MacOSKeyCodeToKM(int keyCode)
{
	// Numbers, letters, text
	if (keyCode >= '0' && keyCode <= '9') {
		return (KeyInputCode)(keyCode - '0' + KM_KEY_0);
	}
	else if (keyCode >= 'A' && keyCode <= 'Z') {
		return (KeyInputCode)(keyCode - 'A' + KM_KEY_A);
	}
	else if (keyCode >= 'a' && keyCode <= 'z') {
		return (KeyInputCode)(keyCode - 'a' + KM_KEY_A);
	}
	else if (keyCode == ' ') {
		return KM_KEY_SPACE;
	}
	else if (keyCode == 0x8) {
		// TODO this isn't right
		return KM_KEY_BACKSPACE;
	}
	// Arrow keys
	else if (keyCode == 63232) {
		return KM_KEY_ARROW_UP;
	}
	else if (keyCode == 63233) {
		return KM_KEY_ARROW_DOWN;
	}
	else if (keyCode == 63234) {
		return KM_KEY_ARROW_LEFT;
	}
	else if (keyCode == 63235) {
		return KM_KEY_ARROW_RIGHT;
	}
	// Special keys
	else if (keyCode == 0x1b) {
		return KM_KEY_ESCAPE;
	}
	/*else if (keyCode == VK_SHIFT) {
		return KM_KEY_SHIFT;
	}
	else if (keyCode == VK_CONTROL) {
		return KM_KEY_CTRL;
	}
	else if (keyCode == VK_TAB) {
	   return KM_KEY_TAB;
	}
	else if (keyCode == VK_RETURN) {
		return KM_KEY_ENTER;
	}
	else if (keyCode >= 0x60 && keyCode <= 0x69) {
		return keyCode - 0x60 + KM_KEY_NUMPAD_0;
	}*/
	else {
		LOG_INFO("Unrecognized key: %d\n", keyCode);
		return KM_KEY_LAST;
	}
}

void MacOSKeyProcessing(bool32 isDown, uint32 key,
	int commandKeyFlag, int ctrlKeyFlag, int altKeyFlag,
	GameInput* input, NSWindow* window)
{
	KeyInputCode kmCode = MacOSKeyCodeToKM((int)key);
	if (kmCode != KM_KEY_LAST) {
		input->keyboard[kmCode].isDown = isDown;
		input->keyboard[kmCode].transitions = 1;

		if (kmCode == KM_KEY_ESCAPE ||
		(kmCode == KM_KEY_Q && isDown && commandKeyFlag)) {
			running_ = false;
		}
		if (kmCode == KM_KEY_F && isDown && commandKeyFlag) {
			[window toggleFullScreen:0];
			/*[self performSelector:@selector(toggleFullScreen:) withObject:myString];*/
		}
	}
}

void MacOSProcessPendingMessages(GameInput* input, NSWindow* window)
{
	NSEvent* event;

	do {
		event = [NSApp nextEventMatchingMask:NSEventMaskAny
			untilDate:nil
			inMode:NSDefaultRunLoopMode
			dequeue:YES];

		switch ([event type]) {
			case NSEventTypeKeyDown: {
				unichar ch = [[event charactersIgnoringModifiers] characterAtIndex:0];
				int modifierFlags = [event modifierFlags];
				int commandKeyFlag = modifierFlags
					& NSEventModifierFlagCommand;
				int ctrlKeyFlag = modifierFlags
					& NSEventModifierFlagControl;
				int altKeyFlag = modifierFlags
					& NSEventModifierFlagOption;

				int keyDownFlag = 1;

				MacOSKeyProcessing(keyDownFlag, ch,
					commandKeyFlag, ctrlKeyFlag, altKeyFlag,
					input, window);
			} break;

			case NSEventTypeKeyUp: {
				unichar ch = [[event charactersIgnoringModifiers] characterAtIndex:0];
				int modifierFlags = [event modifierFlags];
				int commandKeyFlag = modifierFlags
					& NSEventModifierFlagCommand;
				int ctrlKeyFlag = modifierFlags
					& NSEventModifierFlagControl;
				int altKeyFlag = modifierFlags
					& NSEventModifierFlagOption;

				int keyDownFlag = 0;

				MacOSKeyProcessing(keyDownFlag, ch,
					commandKeyFlag, ctrlKeyFlag, altKeyFlag,
					input, window);
			} break;

			case NSEventTypeFlagsChanged: {
				int modifierFlags = [event modifierFlags];
				int shiftKeyFlag = modifierFlags
					& NSEventModifierFlagShift;

				input->keyboard[KM_KEY_SHIFT].isDown = shiftKeyFlag;
				input->keyboard[KM_KEY_SHIFT].transitions = 1;
			} break;

			default: {
				[NSApp sendEvent:event];
			} break;
		}
	} while (event != nil);
}

@interface KMOpenGLView : NSOpenGLView
@end

@implementation KMOpenGLView

- (id)init
{
	self = [super init];
	return self;
}


- (void)prepareOpenGL
{
	[super prepareOpenGL];
	[[self openGLContext] makeCurrentContext];
}

- (void)reshape
{
	[super reshape];

	NSRect bounds = [self bounds];
	[glContext_ makeCurrentContext];
	[glContext_ update];
	glViewport(0, 0, bounds.size.width, bounds.size.height);
}


@end


///////////////////////////////////////////////////////////////////////
// Startup

int main(int argc, const char* argv[])
{
	#pragma unused(argc)
	#pragma unused(argv)

	//return NSApplicationMain(argc, argv); // ??
	@autoreleasepool
	{

#if GAME_SLOW
	debugPrint_ = DEBUGPlatformPrint;
#endif

	NSApplication* app = [NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

	NSString* path = [[NSFileManager defaultManager] currentDirectoryPath];
	path = [NSString stringWithFormat:@"%@/",
		[[NSBundle mainBundle] bundlePath]];
	const char* pathStr = [path UTF8String];
	strncpy(pathToApp_, pathStr, [path length]);
	LOG_INFO("Path to application: %s", pathToApp_);

	/*NSFileManager* fileManager = [NSFileManager defaultManager];
	NSString* appPath = [NSString stringWithFormat:@"%@/Contents/Resources",
		[[NSBundle mainBundle] bundlePath]];
	if ([fileManager changeCurrentDirectoryPath:appPath] == NO) {
		// TODO fix this, it's crashing
		//DEBUG_PANIC("Failed to change application data path\n");
	}*/

	MacOSCreateMainMenu();

	KMAppDelegate* appDelegate = [[KMAppDelegate alloc] init];
	[app setDelegate:appDelegate];
	[NSApp finishLaunching];
	LOG_INFO("Finished launching NSApp\n");

	NSOpenGLPixelFormatAttribute openGLAttributesAccel[] = {
		NSOpenGLPFAAccelerated,
		NSOpenGLPFADoubleBuffer, // Enable for vsync
		NSOpenGLPFAColorSize, 24,
		NSOpenGLPFAAlphaSize, 8,
		NSOpenGLPFADepthSize, 24,
		NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
		0
	};
	NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc]
		initWithAttributes:openGLAttributesAccel];
	glContext_ = [[NSOpenGLContext alloc] initWithFormat:pixelFormat
		shareContext:NULL];
	if (glContext_ == NULL) {
		LOG_INFO("No hardware-accelerated GL renderer\n");
		NSOpenGLPixelFormatAttribute openGLAttributesNoAccel[] = {
			NSOpenGLPFADoubleBuffer, // Enable for vsync
			NSOpenGLPFAColorSize, 24,
			NSOpenGLPFAAlphaSize, 8,
			NSOpenGLPFADepthSize, 24,
			NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
			0
		};
		NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc]
			initWithAttributes:openGLAttributesNoAccel];
		glContext_ = [[NSOpenGLContext alloc] initWithFormat:pixelFormat
			shareContext:NULL];
		if (glContext_ == NULL) {
			LOG_INFO("Failed to create GL context\n");
			return 1;
		}
	}
	LOG_INFO("Initialized GL context\n");

	// Create the main window and the content view
	NSRect screenRect = [[NSScreen mainScreen] frame];
	ScreenInfo screenInfo;
	screenInfo.changed = true;
	screenInfo.size.x = 1280;
	screenInfo.size.y = 720;
	// TODO set these values (they aren't used for now)
	/*screenInfo.colorBits = 32;
	screenInfo.alphaBits = 8;
	screenInfo.depthBits = 24;
	screenInfo.stencilBits = 0;*/

	NSRect initialFrame = NSMakeRect(
		(screenRect.size.width - screenInfo.size.x) * 0.5,
		(screenRect.size.height - screenInfo.size.y) * 0.5,
		screenInfo.size.x, screenInfo.size.y
	);

	NSWindow* window = [[NSWindow alloc] initWithContentRect:initialFrame
		styleMask:NSWindowStyleMaskTitled
			| NSWindowStyleMaskClosable
			| NSWindowStyleMaskMiniaturizable
			| NSWindowStyleMaskResizable
		backing:NSBackingStoreBuffered
		defer:NO];

	[window setBackgroundColor:NSColor.blackColor];
	[window setDelegate:appDelegate];

	NSView* cv = [window contentView];
	[cv setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	[cv setAutoresizesSubviews:YES];

	KMOpenGLView* glView = [[KMOpenGLView alloc] init];
	[glView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	[glView setPixelFormat:pixelFormat];
	[glView setOpenGLContext:glContext_];
	[glView setFrame:[cv bounds]];

	[cv addSubview:glView];

	[pixelFormat release];

	[window setMinSize:NSMakeSize(160, 90)];
	[window setTitle:@"kid"];
	[window makeKeyAndOrderFront:nil];
	LOG_INFO("Initialized window and OpenGL view\n");

	GLint swapInt = 1;
	//GLint swapInt = 0;
	[glContext_ setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
	[glContext_ setView:[window contentView]];
	[glContext_ makeCurrentContext];

	PlatformFunctions platformFuncs = {};
	platformFuncs.DEBUGPlatformPrint = DEBUGPlatformPrint;
	platformFuncs.DEBUGPlatformFlushGl = DEBUGPlatformFlushGl;
	platformFuncs.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
	platformFuncs.DEBUGPlatformReadFile = DEBUGPlatformReadFile;
	platformFuncs.DEBUGPlatformWriteFile = DEBUGPlatformWriteFile;
	if (!MacOSInitOpenGL(&platformFuncs.glFunctions,
	screenInfo.size.x, screenInfo.size.y)) {
		return 1;
	}
	LOG_INFO("Initialized MacOS OpenGL\n");

#if 0
	// Default to full screen mode at startup...
	NSDictionary* fullScreenOptions =
		[NSDictionary dictionaryWithObject:[NSNumber numberWithBool:YES] forKey:NSFullScreenModeSetting];

	[[window contentView] enterFullScreenMode:[NSScreen mainScreen] withOptions:fullScreenOptions];
#endif

	MacOSAudio macAudio;
	int bufferSizeSamples = AUDIO_BUFFER_SIZE_MILLISECONDS
		* AUDIO_SAMPLERATE / 1000;
	MacOSInitCoreAudio(&macAudio, AUDIO_SAMPLERATE,
		AUDIO_CHANNELS, bufferSizeSamples);
	macAudio.latency = AUDIO_SAMPLERATE / 30 * 4; // TODO use refresh rate

	GameAudio gameAudio = {};
	gameAudio.sampleRate = macAudio.sampleRate;
	gameAudio.channels = macAudio.channels;
	gameAudio.bufferSizeSamples = macAudio.bufferSizeSamples;
	int bufferSizeBytes = gameAudio.bufferSizeSamples
		* gameAudio.channels * sizeof(float32);
	gameAudio.buffer = (float32*)mmap(0, (size_t)bufferSizeBytes,
		PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	LOG_INFO("Initialized MacOS CoreAudio\n");

#if GAME_INTERNAL
	void* baseAddress = (void*)TERABYTES((uint64)2);;
#else
	void* baseAddress = 0;
#endif
	
	GameMemory gameMemory = {};
	gameMemory.shouldInitGlobalVariables = true;
	gameMemory.permanent.size = MEGABYTES(64);
	gameMemory.transient.size = GIGABYTES(2);

	// TODO Look into using large virtual pages for this
	// potentially big allocation
	uint64 totalSize = gameMemory.permanent.size + gameMemory.transient.size;
	// TODO check allocation fail?
	gameMemory.permanent.memory = mmap(baseAddress, (size_t)totalSize,
		PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	gameMemory.transient.memory = ((uint8*)gameMemory.permanent.memory +
		gameMemory.permanent.size);
	LOG_INFO("Initialized game memory\n");

	GameInput input[2];
	GameInput* newInput = &input[0];
	GameInput* oldInput = &input[1];
	ClearInput(newInput);
	ClearInput(oldInput);

	const char* gameCodeLibName = "kid_game.dylib";
	char gameCodeLibPath[MACOS_STATE_FILE_NAME_COUNT];
	CatStrings(StringLength(pathToApp_), pathToApp_,
		StringLength(gameCodeLibName), gameCodeLibName,
		MACOS_STATE_FILE_NAME_COUNT, gameCodeLibPath);

	MacOSGameCode gameCode = {};
	MacOSLoadGameCode(&gameCode,
		gameCodeLibPath, MacOSFileId(gameCodeLibPath));

	uint64 startTime = mach_absolute_time();
	mach_timebase_info(&machTimebaseInfo_);
	running_ = true;

	while (running_) {
		MacOSProcessPendingMessages(newInput, window);
		
		[glContext_ makeCurrentContext];

		CGRect windowFrame = [window frame];
		CGPoint mousePosInScreen = [NSEvent mouseLocation];
		BOOL mouseInWindowFlag = NSPointInRect(mousePosInScreen, windowFrame);
		// NOTE Use this instead of convertRectFromScreen
		// if you want to support Snow Leopard
		// NSPoint PointInWindow = [[self window]
		//	convertScreenToBase:[NSEvent mouseLocation]];
		// CGAssociateMouseAndMouseCursorPosition(false);

		// We don't actually care what the mouse screen coordinates are,
		// we just want the coordinates relative to the content view
		NSRect rectInWindow = [window convertRectFromScreen:NSMakeRect(mousePosInScreen.x, mousePosInScreen.y, 1, 1)];
		NSPoint pointInWindow = rectInWindow.origin;

		newInput->mousePos.x = (int)pointInWindow.x;
		newInput->mousePos.y = (int)pointInWindow.y;
		int mouseDeltaX, mouseDeltaY;
		CGGetLastMouseDelta(&mouseDeltaX, &mouseDeltaY);
		newInput->mouseDelta.x = mouseDeltaX;
		newInput->mouseDelta.y = -mouseDeltaY;
		if (!mouseInWindowFlag) {
			for (int i = 0; i < 5; i++) {
				int transitions = newInput->mouseButtons[i].isDown ? 1 : 0;
				newInput->mouseButtons[i].isDown = false;
				newInput->mouseButtons[i].transitions = transitions;
			}
		}

		uint32 mouseButtonMask = [NSEvent pressedMouseButtons];
		for (int buttonIndex = 0; buttonIndex < 3; buttonIndex++) {
			bool32 isDown = (mouseButtonMask >> buttonIndex) & 0x0001;
			bool32 wasDown = oldInput->mouseButtons[buttonIndex].isDown;
			int transitions = wasDown == isDown ? 0 : 1;
			newInput->mouseButtons[buttonIndex].isDown = isDown;
			newInput->mouseButtons[buttonIndex].transitions = transitions;
		}

		// MIDI input
		if (!macAudio.midiInBusy) {
			macAudio.midiInBusy = true;
			newInput->midiIn = macAudio.midiIn;
			macAudio.midiIn.numMessages = 0;
			macAudio.midiInBusy = false;
		}

		uint64 endTime = mach_absolute_time();
		uint64 elapsed = endTime - startTime;
		startTime = endTime;
		uint64 elapsedNanos = elapsed
			* machTimebaseInfo_.numer / machTimebaseInfo_.denom;
		float deltaTime = (float)elapsedNanos * 1e-9;

		if (gameCode.gameUpdateAndRender) {
			ThreadContext thread = {};
			CGRect contentViewFrame = [[window contentView] frame];
			if (screenInfo.size.x != (int)contentViewFrame.size.width
			|| screenInfo.size.y != (int)contentViewFrame.size.height) {
				screenInfo.changed = true;
				screenInfo.size.x = (int)contentViewFrame.size.width;
				screenInfo.size.y = (int)contentViewFrame.size.height;
			}
			gameAudio.fillLength = macAudio.latency;

			gameCode.gameUpdateAndRender(&thread, &platformFuncs,
				newInput, screenInfo, deltaTime,
				&gameMemory, &gameAudio
			);
			screenInfo.changed = false;
		}
		else {
			gameAudio.fillLength = 0;
		}

		int samplesQueued = macAudio.writeCursor - macAudio.readCursor;
		if (macAudio.writeCursor < macAudio.readCursor) {
			samplesQueued = macAudio.bufferSizeSamples - macAudio.readCursor
				+ macAudio.writeCursor;
		}
		if (samplesQueued < macAudio.latency) {
			int samplesToWrite = macAudio.latency - samplesQueued;
			if (samplesToWrite > gameAudio.fillLength) {
				samplesToWrite = gameAudio.fillLength;
			}

			MacOSWriteSamples(&macAudio, &gameAudio, samplesToWrite);
			gameAudio.sampleDelta = samplesToWrite;
		}

		// flushes and forces vsync
		[glContext_ flushBuffer];
		//glFlush(); // no vsync

		GameInput* temp = newInput;
		newInput = oldInput;
		oldInput = temp;
		ClearInput(newInput, oldInput);
	}


	MacOSStopCoreAudio(&macAudio);

	} // @autoreleasepool
}

#include <km_common/km_input.cpp>

#include "macos_audio.cpp"