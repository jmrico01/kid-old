#include "linux_main.h"

#include <sys/mman.h>       // memory functions
#include <sys/stat.h>       // file stat functions
#include <fcntl.h>          // file open/close functions
#include <unistd.h>         // symbolic links & other
#include <dlfcn.h>          // dynamic linking functions
#include <pthread.h>        // threading
//#include <dirent.h>
//#include <sys/sysinfo.h>  // get_nprocs
//#include <sys/wait.h>     // waitpid
//#include <unistd.h>       // usleep
//#include <time.h>         // CLOCK_MONOTONIC, clock_gettime
//#include <semaphore.h>    // sem_init, sem_wait, sem_post
//#include <alloca.h>       // alloca

#include <stdarg.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <GL/gl.h>
#include <GL/glx.h>

#include <alsa/asoundlib.h>

#include <km_common/km_debug.h>
#include <km_common/km_math.h>
#include <km_common/km_input.h>
#include <km_common/km_string.h>

#include "linux_audio.h"

global_var char pathToApp_[LINUX_STATE_FILE_NAME_COUNT];
global_var bool32 running_;

// Required GLX functions
typedef GLXContext  glXCreateContextAttribsARBFunc(
    Display* display, GLXFBConfig config, GLXContext shareContext,
    Bool direct, const int* attribList);
global_var glXCreateContextAttribsARBFunc* glXCreateContextAttribsARB;
typedef void        glXSwapIntervalEXTFunc(
    Display* display, GLXDrawable drawable, int interval);
global_var glXSwapIntervalEXTFunc* glXSwapIntervalEXT;

#define LOAD_GLX_FUNCTION(name) \
    name = (name##Func*)glXGetProcAddress((const GLubyte*)#name)

global_var int linuxOpenGLAttribs[] =
{
    GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
    GLX_CONTEXT_MINOR_VERSION_ARB, 3,
    GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
#if GAME_INTERNAL
        | GLX_CONTEXT_DEBUG_BIT_ARB
#endif
        ,
#if 0
    GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
#else
    GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
#endif
    0,
};

enum
{
    _NET_WM_STATE_REMOVE = 0,
    _NET_WM_STATE_ADD = 1,
    _NET_WM_STATE_TOGGLE = 2
};

// NOTE(michiel): Explicit wrappers around dlsym, dlopen and dlclose
internal void* LinuxLoadFunction(void* libHandle, const char* name)
{
    void* symbol = dlsym(libHandle, name);
    if (!symbol) {
        fprintf(stderr, "dlsym failed: %s\n", dlerror());
    }
    // TODO(michiel): Check if lib with underscore exists?!
    return symbol;
}

internal void* LinuxLoadLibrary(const char* libName)
{
    void* handle = dlopen(libName, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
    }
    return handle;
}

internal void LinuxUnloadLibrary(void* handle)
{
    if (handle != NULL) {
        dlclose(handle);
        handle = NULL;
    }
}

internal inline uint32 SafeTruncateUInt64(uint64 value)
{
	// TODO defines for maximum values
	DEBUG_ASSERT(value <= 0xFFFFFFFF);
	uint32 result = (uint32)value;
	return result;
}

internal void RemoveFileNameFromPath(
    const char* filePath, char* dest, uint64 destLength)
{
    unsigned int lastSlash = 0;
	// TODO confused... some cross-platform code inside a win32 file
	//	maybe I meant to pull this out sometime?
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

internal void LinuxGetEXEFileName(LinuxState *state)
{
    ssize_t numRead = readlink("/proc/self/exe",
        state->exeFilePath, ARRAY_COUNT(state->exeFilePath) - 1);
    state->exeFilePath[numRead] = '\0';

    if (numRead > 0) {
        state->exeOnePastLastSlash = state->exeFilePath;
        for (char* scan = state->exeFilePath; *scan != '\0'; scan++) {
            if (*scan == '/') {
                state->exeOnePastLastSlash = scan + 1;
            }
        }
    }
}

internal void LinuxBuildEXEPathFileName(
    LinuxState *state, const char *fileName,
    int dstLen, char *dst)
{
    CatStrings(
        state->exeOnePastLastSlash - state->exeFilePath, state->exeFilePath,
        StringLength(fileName), fileName,
        dstLen, dst);
}

internal inline ino_t LinuxFileId(const char* fileName)
{
    struct stat attr = {};
    if (stat(fileName, &attr)) {
        attr.st_ino = 0;
    }

    return attr.st_ino;
}

#if GAME_INTERNAL

DEBUG_PLATFORM_PRINT_FUNC(DEBUGPlatformPrint)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

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

    char fullPath[LINUX_STATE_FILE_NAME_COUNT];
    CatStrings(StringLength(pathToApp_), pathToApp_,
        StringLength(fileName), fileName, LINUX_STATE_FILE_NAME_COUNT, fullPath);
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

    char fullPath[LINUX_STATE_FILE_NAME_COUNT];
    CatStrings(StringLength(pathToApp_), pathToApp_,
        StringLength(fileName), fileName, LINUX_STATE_FILE_NAME_COUNT, fullPath);
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

#endif

// Dynamic code loading
internal bool32 LinuxLoadGameCode(
    LinuxGameCode* gameCode, const char* libName, ino_t fileId)
{
    if (gameCode->gameLibId != fileId) {
        LinuxUnloadLibrary(gameCode->gameLibHandle);
        gameCode->gameLibId = fileId;
        gameCode->isValid = false;

        gameCode->gameLibHandle = LinuxLoadLibrary(libName);
        if (gameCode->gameLibHandle) {
            *(void**)(&gameCode->gameUpdateAndRender) = LinuxLoadFunction(
                gameCode->gameLibHandle, "GameUpdateAndRender");
            if (gameCode->gameUpdateAndRender) {
                gameCode->isValid = true;
            }
        }
    }

    if (!gameCode->isValid) {
        LinuxUnloadLibrary(gameCode->gameLibHandle);
        gameCode->gameLibId = 0;
        gameCode->gameUpdateAndRender = 0;
    }

    return gameCode->isValid;
}

internal void LinuxUnloadGameCode(LinuxGameCode *gameCode)
{
    LinuxUnloadLibrary(gameCode->gameLibHandle);
    gameCode->gameLibId = 0;
    gameCode->isValid = false;
    gameCode->gameUpdateAndRender = 0;
}

internal bool32 LinuxLoadGLXExtensions()
{
    Display* tempDisplay = XOpenDisplay(NULL);
    if (!tempDisplay) {
        LOG_INFO("Failed to open display on GLX extension load\n");
        return false;
    }

    int dummy;
    if (!glXQueryExtension(tempDisplay, &dummy, &dummy)) {
        LOG_INFO("GLX extension query failed\n");
        return false;
    }

    int displayBufferAttribs[] = {
        GLX_RGBA,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_DOUBLEBUFFER,
        None
    };
    XVisualInfo* visuals = glXChooseVisual(tempDisplay,
        DefaultScreen(tempDisplay), displayBufferAttribs);
    if (!visuals) {
        LOG_INFO("Failed to choose GLX visual\n");
        return false;
    }

    visuals->screen = DefaultScreen(tempDisplay);

    XSetWindowAttributes attribs = {};
    Window root = RootWindow(tempDisplay, visuals->screen);
    attribs.colormap = XCreateColormap(tempDisplay, root,
        visuals->visual, AllocNone);

    Window glWindow = XCreateWindow(tempDisplay, root,
        0, 0,
        10, 10,
        0, visuals->depth, InputOutput, visuals->visual,
        CWColormap, &attribs);
    if (!glWindow) {
        LOG_INFO("Failed to create dummy GL window\n");
        return false;
    }

    GLXContext context = glXCreateContext(tempDisplay,
        visuals, NULL, true);
    if (!glXMakeCurrent(tempDisplay, glWindow, context)) {
        LOG_INFO("Failed to make GLX context current\n");
        return false;
    }

    char* extensions = (char*)glXQueryExtensionsString(tempDisplay,
        visuals->screen);
    //LOG_INFO("Supported extensions: %s\n", extensions);
    char* at = extensions;
    while (*at) {
        while (IsWhitespace(*at)) {
            at++;
        }
        char* end = at;
        while (*end && !IsWhitespace(*end)) {
            end++;
        }
        //size_t count = end - at;

        /*const char* srgbFramebufferExtName =
            "GLX_EXT_framebuffer_sRGB";
        const char* srgbFramebufferArbName =
            "GLX_ARB_framebuffer_sRGB";
        if (0) {
        }
        else if (StringsAreEqual(at, count,
        srgbFramebufferExtName, StringLength(srgbFramebufferExtName))) {
            //OpenGL.SupportsSRGBFramebuffer = true;
        }
        else if (StringsAreEqual(at, count,
        srgbFramebufferArbName, StringLength(srgbFramebufferArbName))) {
            //OpenGL.SupportsSRGBFramebuffer = true;
        }*/

        at = end;
    }

    glXMakeCurrent(tempDisplay, None, NULL);

    glXDestroyContext(tempDisplay, context);
    XDestroyWindow(tempDisplay, glWindow);

    XFreeColormap(tempDisplay, attribs.colormap);
    XFree(visuals);

    XCloseDisplay(tempDisplay);

    return true;
}

internal GLXFBConfig* LinuxGetOpenGLFramebufferConfig(Display *display)
{
    int visualAttribs[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_DOUBLEBUFFER, True,
        GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB, True,
        //GLX_SAMPLE_BUFFERS  , 1,
        //GLX_SAMPLES         , 4,
        None
    };

    /*if (!OpenGL.SupportsSRGBFramebuffer) {
        visualAttribs[22] = None;
    }*/

    int framebufferCount;
    GLXFBConfig* framebufferConfig = glXChooseFBConfig(
        display, DefaultScreen(display), visualAttribs, &framebufferCount);
    DEBUG_ASSERT(framebufferCount >= 1);

    return framebufferConfig;
}

#define LOAD_GL_FUNCTION(name) \
    glFuncs->name = (name##Func*)glXGetProcAddress((const GLubyte*)#name); \
    if (!glFuncs->name) { \
        DEBUG_PANIC("OpenGL function load failed: %s\n", #name); \
    }

internal bool32 LinuxLoadBaseGLFunctions(OpenGLFunctions* glFuncs)
{
	// Generate function loading code
#define FUNC(returntype, name, ...) LOAD_GL_FUNCTION(name);
	GL_FUNCTIONS_BASE
#undef FUNC

    return true;
}

internal bool32 LinuxLoadAllGLFunctions(OpenGLFunctions* glFuncs)
{
	// Generate function loading code
#define FUNC(returntype, name, ...) LOAD_GL_FUNCTION(name);
	GL_FUNCTIONS_ALL
#undef FUNC

    return true;
}

internal bool32 LinuxInitOpenGL(
    OpenGLFunctions* glFuncs,
    Display* display, GLXDrawable glWindow,
    int width, int height)
{
    if (!LinuxLoadBaseGLFunctions(glFuncs)) {
        LOG_INFO("Failed to load base GL functions\n");
        return false;
    }

	glFuncs->glViewport(0, 0, width, height);

	// Set v-sync
    LOAD_GLX_FUNCTION(glXSwapIntervalEXT);
    if (glXSwapIntervalEXT) {
        glXSwapIntervalEXT(display, glWindow, 1);
    }
    else {
        // TODO no vsync. logging? just exit? just exit for now
        LOG_INFO("Failed to load glXSwapIntervalEXT (no vsync)\n");
        //return false;
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
		LOG_ERROR("Unsupported OpenGL version (less than 3.3)\n");
		return false;
	}

	if (!LinuxLoadAllGLFunctions(glFuncs)) {
        LOG_INFO("Failed to load all GL functions\n");
		return false;
	}

	const GLubyte* glslString =
        glFuncs->glGetString(GL_SHADING_LANGUAGE_VERSION);
    LOG_INFO("GL_SHADING_LANGUAGE_VERSION: %s\n", glslString);

    return true;
}

#if 0
internal void LinuxGetInputFileLocation(
    LinuxState* state, bool32 inputStream,
    int32 slotInd, int32 dstCount, char *dst)
{
    char temp[64];
    snprintf(temp, sizeof(temp), "loop_edit_%d_%s.hmi", slotInd, inputStream ? "input" : "state");
    LinuxBuildEXEPathFileName(state, temp, dstCount, dst);
}
#endif

internal inline struct timespec LinuxGetWallClock()
{
    struct timespec clock;
    clock_gettime(CLOCK_MONOTONIC, &clock);
    return clock;
}

internal inline float32 LinuxGetSecondsElapsed(
    struct timespec start, struct timespec end)
{
    return (float32)(end.tv_sec - start.tv_sec)
        + ((float32)(end.tv_nsec - start.tv_nsec) * 1e-9f);
}

// TODO change this to an array or something
internal int LinuxKeyCodeToKM(unsigned int keycode)
{
    // Numbers, letters, text
    // Arrow keys
    // Special keys
    if (keycode == 9) {
        return KM_KEY_ESCAPE;
    }
    else if (keycode == 65) {
        return KM_KEY_SPACE;
    }
    else if (keycode == 22) {
        return KM_KEY_BACKSPACE;
    }
    else if (keycode == 25) {
        return KM_KEY_W;
    }
    else if (keycode == 38) {
        return KM_KEY_A;
    }
    else if (keycode == 39) {
        return KM_KEY_S;
    }
    else if (keycode == 40) {
        return KM_KEY_D;
    }
    else {
        return -1;
    }
}

internal void LinuxProcessPendingMessages(
    LinuxState *state, Display *display, Window window, Atom wmDeleteWindow,
    GameInput* input, Vec2* mousePos, ScreenInfo* screenInfo)
{
    while (running_ && XPending(display)) {
        XEvent event;
        XNextEvent(display, &event);

        // Don't skip the scroll key events
        if (event.type == ButtonRelease) {
            if ((event.xbutton.button != 4) &&
            (event.xbutton.button != 5) &&
            XEventsQueued(display, QueuedAfterReading)) {
                // Skip the auto repeat key
                XEvent nextEvent;
                XPeekEvent(display, &nextEvent);
                if ((nextEvent.type == ButtonPress) &&
                (nextEvent.xbutton.time == event.xbutton.time) &&
                (nextEvent.xbutton.button == event.xbutton.button)) {
                    continue;
                }
            }
        }
        // Skip the keyboard
        if (event.type == KeyRelease
        && XEventsQueued(display, QueuedAfterReading)) {
            XEvent nextEvent;
            XPeekEvent(display, &nextEvent);
            if ((nextEvent.type == KeyPress) &&
            (nextEvent.xbutton.time == event.xbutton.time) &&
            (nextEvent.xbutton.button == event.xbutton.button)) {
                continue;
            }
        }

        switch (event.type) {
            case ConfigureNotify: {
                int w = event.xconfigure.width;
                int h = event.xconfigure.height;
                if ((screenInfo->size.x != w) || (screenInfo->size.y != h)) {
                    screenInfo->size.x = w;
                    screenInfo->size.y = h;
                    glViewport(0, 0, w, h);
                }
            } break;
            case DestroyNotify: {
                running_ = false;
            } break;
            case ClientMessage: {
                if ((Atom)event.xclient.data.l[0] == wmDeleteWindow) {
                    running_ = false;
                }
            } break;
            case MotionNotify: {
                mousePos->x = (float32)event.xmotion.x;
                mousePos->y = (float32)(screenInfo->size.x - event.xmotion.y);
            } break;

            case ButtonRelease:
            case ButtonPress: {
                if (event.xbutton.button == 1) {
                    //LOG_INFO("left click something\n");
                }
                else if (event.xbutton.button == 2) {
                    //LOG_INFO("middle click something\n");
                }
                else if (event.xbutton.button == 3) {
                    //LOG_INFO("right click something\n");
                }
            } break;

            case KeyRelease:
            case KeyPress: {
                //bool32 altWasDown = event.xkey.state & KEYCODE_ALT_MASK;
                //bool32 shiftWasDown = event.xkey.state & KEYCODE_SHIFT_MASK;
                bool32 isDown = event.type == KeyPress;
                int transitions = 1;

                //LOG_INFO("key code: %d\n", event.xkey.keycode);
                int kmKeyCode = LinuxKeyCodeToKM(event.xkey.keycode);
                if (kmKeyCode != -1) {
                    input->keyboard[kmKeyCode].isDown = isDown;
                    input->keyboard[kmKeyCode].transitions = transitions;
                }
                
                if (input->keyboard[KM_KEY_ESCAPE].isDown) {
                    running_ = false;
                }
            } break;

            default: {
            } break;
        }
    }
}

#if 0
internal void ToggleFullscreen(Display *display, Window window)
{
    GlobalFullscreen = !GlobalFullscreen;
    Atom FullscreenAtom = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
    Atom WindowState = XInternAtom(display, "_NET_WM_STATE", False);
    int32 Mask = SubstructureNotifyMask | SubstructureRedirectMask;
    // int32 Mask = StructureNotifyMask | ResizeRedirectMask;
    XEvent event = {};
    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    // event.xclient.display = display;
    event.xclient.window = window;
    event.xclient.message_type = WindowState;
    event.xclient.format = 32;
    event.xclient.data.l[0] = (GlobalFullscreen ?
        _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE); // set (2 is toggle)
    event.xclient.data.l[1] = (long)FullscreenAtom;
    event.xclient.data.l[2] = 0;

    XSendEvent(display, DefaultRootWindow(display), False, Mask, &event);
    // XFlush(display);
}
#endif

//
// Replays
//

#if 0
internal void LinuxBeginRecordingInput(
    LinuxState *state, int InputRecordingIndex)
{
    // TODO(michiel): mmap to file?
    char FileName[LINUX_STATE_FILE_NAME_COUNT];
    LinuxGetInputFileLocation(state, true, InputRecordingIndex, sizeof(FileName), FileName);
    state->RecordingHandle = open(FileName, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (state->RecordingHandle >= 0)
    {
        //ssize_t BytesWritten = write(FileHandle, Memory, MemorySize);
        ssize_t BytesWritten;

        state->InputRecordingIndex = InputRecordingIndex;
        LinuxMemoryBlock *Sentinel = state->MemorySentinel;

        BeginTicketMutex(state->MemoryMutex);
        for (LinuxMemoryBlock *SourceBlock = Sentinel->Next;
            SourceBlock != Sentinel;
            SourceBlock = SourceBlock->Next)
        {
            if (!(SourceBlock->Block.Flags & PlatformMemory_NotRestored))
            {
                linux_saved_memory_block DestBlock;
                void *BasePointer = SourceBlock->Block.Base;
                DestBlock.BasePointer = (u64)BasePointer;
                DestBlock.Size = SourceBlock->Block.Size;
                BytesWritten = write(state->RecordingHandle, &DestBlock, sizeof(DestBlock));
                DEBUG_ASSERT(DestBlock.Size <= U32Max);
                BytesWritten = write(state->RecordingHandle, BasePointer, (uint32)DestBlock.Size);
            }
        }
        EndTicketMutex(state->MemoryMutex);

        linux_saved_memory_block DestBlock = {};
        BytesWritten = write(state->RecordingHandle, &DestBlock, sizeof(DestBlock));
    }
}

internal void LinuxEndRecordingInput(LinuxState *state)
{
    fsync(state->RecordingHandle);
    close(state->RecordingHandle);
    state->InputRecordingIndex = 0;
}

internal void LinuxBeginInputPlayBack(
    LinuxState *state, int InputPlayingIndex)
{
    //LinuxClearBlocksByMask(state, LinuxMem_AllocatedDuringLooping);

    char FileName[LINUX_STATE_FILE_NAME_COUNT];
    LinuxGetInputFileLocation(state, true, InputPlayingIndex, sizeof(FileName), FileName);
    state->PlaybackHandle = open(FileName, O_RDONLY);
    if (state->PlaybackHandle >= 0) {
        state->InputPlayingIndex = InputPlayingIndex;

        for (;;) {
            linux_saved_memory_block Block = {};
            ssize_t BytesRead = read(state->PlaybackHandle, &Block, sizeof(Block));
            if (Block.BasePointer != 0) {
                void *BasePointer = (void *)Block.BasePointer;
                DEBUG_ASSERT(Block.Size <= UINT32_MAX);
                BytesRead = read(state->PlaybackHandle, BasePointer, (uint32)Block.Size);
            }
            else {
                break;
            }
        }
        // TODO(casey): Stream memory in from the file!
    }
}

internal void LinuxEndInputPlayBack(LinuxState *state)
{
    LinuxClearBlocksByMask(state, LinuxMem_FreedDuringLooping);
    close(state->PlaybackHandle);
    state->InputPlayingIndex = 0;
}

internal void LinuxRecordInput(LinuxState *state, game_input *NewInput)
{
    write(state->RecordingHandle, NewInput, sizeof(*NewInput));
}

internal void
LinuxPlayBackInput(linux_state *State, game_input *NewInput)
{
    ssize_t BytesRead = read(State->PlaybackHandle, NewInput, sizeof(*NewInput));
    if (BytesRead == 0) {
        // NOTE(casey): We've hit the end of the stream, go back to the beginning
        int32 PlayingIndex = State->InputPlayingIndex;
        LinuxEndInputPlayBack(State);
        LinuxBeginInputPlayBack(State, PlayingIndex);
        read(State->PlaybackHandle, NewInput, sizeof(*NewInput));
    }
}
#endif

int main(int argc, char **argv)
{
    #if GAME_SLOW
    debugPrint_ = DEBUGPlatformPrint;
    #endif

    LinuxState linuxState = {};
    LinuxGetEXEFileName(&linuxState);

    RemoveFileNameFromPath(linuxState.exeFilePath, pathToApp_, LINUX_STATE_FILE_NAME_COUNT);
    LOG_INFO("Path to application: %s\n", pathToApp_);
    
    ScreenInfo screenInfo;
    screenInfo.size.x = 800;
    screenInfo.size.y = 600;
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        LOG_INFO("Failed to open display\n");
        return 1;
    }

    if (!LinuxLoadGLXExtensions()) {
        return 1;
    }

    GLXFBConfig* framebufferConfigs = LinuxGetOpenGLFramebufferConfig(display);
    GLXFBConfig framebufferConfig = framebufferConfigs[0];
    XFree(framebufferConfigs);
    XVisualInfo* visualInfo = glXGetVisualFromFBConfig(display,
        framebufferConfig);
    if (!visualInfo) {
        LOG_INFO("Failed to get framebuffer visual info\n");
        return 1;
    }

    visualInfo->screen = DefaultScreen(display);

    XSetWindowAttributes windowAttribs = {};
    Window root = RootWindow(display, visualInfo->screen);
    windowAttribs.colormap = XCreateColormap(display, root, visualInfo->visual, AllocNone);
    windowAttribs.border_pixel = 0;
    // This in combination with CWOverrideRedirect produces a borderless window
    //windowAttribs.override_redirect = 1;
    windowAttribs.event_mask = (StructureNotifyMask | PropertyChangeMask |
                                PointerMotionMask | ButtonPressMask | ButtonReleaseMask |
                                KeyPressMask | KeyReleaseMask);

    Window glWindow = XCreateWindow(display, root,
        100, 100, screenInfo.size.x, screenInfo.size.y,
        0, visualInfo->depth, InputOutput, visualInfo->visual,
        CWBorderPixel | CWColormap | CWEventMask /*| CWOverrideRedirect*/,
        &windowAttribs);
    if (!glWindow) {
        LOG_INFO("Failed to create X11 window\n");
        return 1;
    }
    
    XSizeHints sizeHints = {};
    sizeHints.x = 100;
    sizeHints.y = 100;
    sizeHints.width  = screenInfo.size.x;
    sizeHints.height = screenInfo.size.y;
    sizeHints.flags = USSize | USPosition; // US vs PS?

    XSetNormalHints(display, glWindow, &sizeHints);
    XSetStandardProperties(display, glWindow, "kid", "glsync text",
        None, NULL, 0, &sizeHints);

    Atom wmDeleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, glWindow, &wmDeleteWindow, 1);
    XMapRaised(display, glWindow);
    LOG_INFO("Created X11 window\n");

    GLXContext glxContext = 0;
    LOAD_GLX_FUNCTION(glXCreateContextAttribsARB);
    if (glXCreateContextAttribsARB) {
        glxContext = glXCreateContextAttribsARB(display, framebufferConfig,
            0, true, linuxOpenGLAttribs);
    }
    if (!glxContext) {
        LOG_INFO("Failed to create OpenGL 3+ context\n");
        return 1;
    }

    if (!glXMakeCurrent(display, glWindow, glxContext)) {
        LOG_INFO("Failed to make GLX context current\n");
        return 1;
    }
    LOG_INFO("Created GLX context\n");

    PlatformFunctions platformFuncs = {};
	platformFuncs.DEBUGPlatformPrint = DEBUGPlatformPrint;
	platformFuncs.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
	platformFuncs.DEBUGPlatformReadFile = DEBUGPlatformReadFile;
	platformFuncs.DEBUGPlatformWriteFile = DEBUGPlatformWriteFile;
    if (!LinuxInitOpenGL(&platformFuncs.glFunctions, display, glWindow,
    screenInfo.size.x, screenInfo.size.y)) {
        return 1;
    }
    LOG_INFO("Initialized Linux OpenGL\n");

    GameAudio gameAudio_;
    if (!LinuxInitAudio(&globalAudio,
    AUDIO_CHANNELS, AUDIO_SAMPLERATE, AUDIO_PERIOD_SIZE, AUDIO_NUM_PERIODS)) {
        return 1;
    }
    LOG_INFO("Initialized Linux audio\n");

#if GAME_INTERNAL
	void* baseAddress = (void*)TERABYTES((uint64)2);;
#else
	void* baseAddress = 0;
#endif
    
    GameMemory gameMemory = {};
    gameMemory.shouldInitGlobalVariables = true;
	gameMemory.permanentStorageSize = MEGABYTES(64);
	gameMemory.transientStorageSize = GIGABYTES(1);


	// TODO Look into using large virtual pages for this
    // potentially big allocation
	uint64 totalSize = gameMemory.permanentStorageSize
        + gameMemory.transientStorageSize;
	// TODO check allocation fail?
	gameMemory.permanentStorage = mmap(baseAddress, (size_t)totalSize,
		PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (!gameMemory.permanentStorage) {
		LOG_ERROR("Linux memory allocation (mmap) failed\n");
		return 1;
	}
	gameMemory.transientStorage = ((uint8*)gameMemory.permanentStorage +
		gameMemory.permanentStorageSize);

	linuxState.gameMemorySize = totalSize;
	linuxState.gameMemoryBlock = gameMemory.permanentStorage;
	LOG_INFO("Initialized game memory\n");

	GameInput input[2] = {};
	GameInput *newInput = &input[0];
	GameInput *oldInput = &input[1];

    struct timespec lastCounter = LinuxGetWallClock();

    char gameCodeLibPath[LINUX_STATE_FILE_NAME_COUNT];
    LinuxBuildEXEPathFileName(&linuxState, "kid_game.so",
        sizeof(gameCodeLibPath), gameCodeLibPath);
    
    LinuxGameCode gameCode = {};
    LinuxLoadGameCode(&gameCode,
        gameCodeLibPath, LinuxFileId(gameCodeLibPath));

    // IMPORTANT LEFTOVER NOTES:
    // Querying X11 every frame can cause hickups because of a sync of X11 state

    // TODO This is actually game-specific code
    /*uint32 runningSampleIndex = 0;
    float32 tSine1 = 0.0f;
    float32 tSine2 = 0.0f;*/

    float toneHz = 260.0f;
    for (uint32 i = 0; i < globalAudio.bufferSize; i++) {
        float t = 2.0f * PI_F * toneHz * i / globalAudio.sampleRate; 
        int16 s1 = (int16)(INT16_MAXVAL * sinf(t));
        int16 s2 = (int16)(INT16_MAXVAL * sinf(t));
        globalAudio.buffer[i * globalAudio.channels]        = s1;
        globalAudio.buffer[i * globalAudio.channels + 1]    = s2;
    }

    running_ = true;
    while (running_) {
        Vec2 mousePos;
        LinuxProcessPendingMessages(&linuxState, display,
            glWindow, wmDeleteWindow,
            newInput, &mousePos, &screenInfo);

        struct timespec endCounter = LinuxGetWallClock();
        float32 deltaTime = LinuxGetSecondsElapsed(
            lastCounter, endCounter);
        lastCounter = endCounter;

        if (gameCode.gameUpdateAndRender) {
			ThreadContext thread = {};
            gameCode.gameUpdateAndRender(&thread, &platformFuncs,
                newInput, screenInfo, deltaTime,
                &gameMemory, &gameAudio_);
        }

        pthread_mutex_lock(&globalAudioMutex);
#if 0
        uint32 playMark = globalAudio.readIndex;
        uint32 fillTarget = (playMark - 1) % globalAudio.bufferSize;
        uint32 writeTo = runningSampleIndex % globalAudio.bufferSize;
        uint32 writeLen;
        if (writeTo == fillTarget) {
            writeLen = globalAudio.bufferSize;
        }
        else if (writeTo > fillTarget) {
            writeLen = globalAudio.bufferSize - (writeTo - fillTarget);
        }
        else {
            writeLen = fillTarget - writeTo;
        }
        /*uint32 writeLen;
        if (playMark < fillTarget) {
            if (playMark <= writeTo && writeTo < fillTarget) {
                writeLen = fillTarget - writeTo;
            }
            else {
                writeLen = 0;
            }
        }
        else if (playMark > fillTarget) {
            if (fillTarget <= writeTo && writeTo < playMark) {
                writeLen = 0;
            }
            else {
                if (writeTo > fillTarget) {
                    writeLen = globalAudio.bufferSize + fillTarget - writeTo;
                }
                else {
                    writeLen = fillTarget - writeTo;
                }
            }
        }
        else {
            // hm... strange. but remotely possible I suppose
            writeLen = 0;
        }*/

        LOG_INFO("play mark: %d, target: %d\n", playMark, fillTarget);
        LOG_INFO("writeTo: %d\n", writeTo);
        LOG_INFO("writeLen: %d / %d\n", writeLen, globalAudio.bufferSize);
        if (newInput->keyboard[KM_KEY_SPACE].isDown
        && newInput->keyboard[KM_KEY_SPACE].transitions > 0) {
            LOG_INFO("Paused on terminal...\n");
            getchar();
        }

        float tone1Hz = 440.6f;
        float tone2Hz = tone1Hz * 3.0f / 2.0f;
        for (uint32 i = 0; i < writeLen; i++) {
            uint32 ind = (writeTo + i) % globalAudio.bufferSize;
            int16 s1 = (int16)(INT16_MAXVAL * sinf(tSine1));
            int16 s2 = (int16)(INT16_MAXVAL * sinf(tSine2));
            globalAudio.buffer[ind * globalAudio.channels]        = s1;
            globalAudio.buffer[ind * globalAudio.channels + 1]    = s2;

            tSine1 += 2.0f * PI_F * tone1Hz * 1.0f
                / (float32)globalAudio.sampleRate;
            tSine2 += 2.0f * PI_F * tone2Hz * 1.0f
                / (float32)globalAudio.sampleRate;

            runningSampleIndex++;
        }
#endif
        if (WasKeyPressed(newInput, KM_KEY_SPACE)) {
            toneHz *= 3.0f / 2.0f;
            for (uint32 i = 0; i < globalAudio.bufferSize; i++) {
                float t = 2.0f * PI_F * toneHz * i / globalAudio.sampleRate; 
                int16 s1 = (int16)(INT16_MAXVAL * sinf(t));
                int16 s2 = (int16)(INT16_MAXVAL * sinf(t));
                globalAudio.buffer[i * globalAudio.channels]        = s1;
                globalAudio.buffer[i * globalAudio.channels + 1]    = s2;
            }
        }
        if (WasKeyPressed(newInput, KM_KEY_BACKSPACE)) {
            toneHz /= 3.0f / 2.0f;
            for (uint32 i = 0; i < globalAudio.bufferSize; i++) {
                float t = 2.0f * PI_F * toneHz * i / globalAudio.sampleRate; 
                int16 s1 = (int16)(INT16_MAXVAL * sinf(t));
                int16 s2 = (int16)(INT16_MAXVAL * sinf(t));
                globalAudio.buffer[i * globalAudio.channels]        = s1;
                globalAudio.buffer[i * globalAudio.channels + 1]    = s2;
            }
        }

        pthread_mutex_unlock(&globalAudioMutex);
        
        glXSwapBuffers(display, glWindow);

		GameInput *temp = newInput;
		newInput = oldInput;
		oldInput = temp;
        ClearInput(newInput, oldInput);
    }

    LinuxUnloadGameCode(&gameCode);
    //LinuxStopPlayingSound();

#if 0
    int DebugTimeMarkerIndex = 0;
    linux_debug_time_marker DebugTimeMarkers[30] = {0};

    uint32 AudioLatencyBytes = 0;
    float32 AudioLatencySeconds = 0.0f;
    bool32 SoundIsValid = false;

    uint32 ExpectedFramesPerUpdate = 1;
    float32 TargetSecondsPerFrame = (float32)ExpectedFramesPerUpdate / (float32)GameUpdateHz;
    
    while (GlobalRunning)
    {
        if(state->InputRecordingIndex)
        {
            LinuxRecordInput(state, NewInput);
        }

        if(state->InputPlayingIndex)
        {
            game_input Temp = *NewInput;
            LinuxPlayBackInput(state, NewInput);
            for (uint32 MouseButtonIndex = 0;
                MouseButtonIndex < PlatformMouseButton_Count;
                ++MouseButtonIndex)
            {
                NewInput->MouseButtons[MouseButtonIndex] = Temp.MouseButtons[MouseButtonIndex];
            }
            NewInput->MouseX = Temp.MouseX;
            NewInput->MouseY = Temp.MouseY;
            NewInput->MouseZ = Temp.MouseZ;
        }

        BEGIN_BLOCK("Audio Update");

        /* NOTE(casey):
            Here is how sound output computation works.
            We define a safety value that is the number
            of samples we think our game update loop
            may vary by (let's say up to 2ms)
            When we wake up to write audio, we will look
            and see what the play cursor position is and we
            will forecast ahead where we think the play
            cursor will be on the next frame boundary.
            We will then look to see if the write cursor is
            before that by at least our safety value.  If
            it is, the target fill position is that frame
            boundary plus one frame.  This gives us perfect
            audio sync in the case of a card that has low
            enough latency.
            If the write cursor is _after_ that safety
            margin, then we assume we can never sync the
            audio perfectly, so we will write one frame's
            worth of audio plus the safety margin's worth
            of guard samples.
        */
        uint32 PlayCursor = SoundOutput.Buffer.ReadIndex;
        uint32 WriteCursor = PlayCursor + AUDIO_WRITE_SAFE_SAMPLES * SoundOutput.BytesPerSample;
        if (!SoundIsValid)
        {
            SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
            SoundIsValid = true;
        }

        uint32 ByteToLock = ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) %
                            SoundOutput.Buffer.Size);

        uint32 ExpectedSoundBytesPerFrame =
            (uint32)((float32)(SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample) /
                    GameUpdateHz);

        uint32 ExpectedFrameBoundaryByte = PlayCursor + ExpectedSoundBytesPerFrame;

        uint32 SafeWriteCursor = WriteCursor + SoundOutput.SafetyBytes;
        bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

        uint32 TargetCursor = 0;
        if(AudioCardIsLowLatency)
        {
            TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
        }
        else
        {
            TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame +
                            SoundOutput.SafetyBytes);
        }
        TargetCursor = (TargetCursor % SoundOutput.Buffer.Size);

        uint32 BytesToWrite = 0;
        if(ByteToLock > TargetCursor)
        {
            BytesToWrite = (SoundOutput.Buffer.Size - ByteToLock);
            BytesToWrite += TargetCursor;
        }
        else
        {
            BytesToWrite = TargetCursor - ByteToLock;
        }

        game_sound_output_buffer SoundBuffer = {};
        SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
        SoundBuffer.SampleCount = Align8(BytesToWrite / SoundOutput.BytesPerSample);
        BytesToWrite = SoundBuffer.SampleCount*SoundOutput.BytesPerSample;
        SoundBuffer.Samples = Samples;
        if(Game.GetSoundSamples)
        {
            Game.GetSoundSamples(&GameMemory, &SoundBuffer);
        }

#if GAME_INTERNAL
        linux_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
        Marker->OutputPlayCursor = PlayCursor;
        Marker->OutputWriteCursor = WriteCursor;
        Marker->OutputLocation = ByteToLock;
        Marker->OutputByteCount = BytesToWrite;
        Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;

        uint32 UnwrappedWriteCursor = WriteCursor;
        if(UnwrappedWriteCursor < PlayCursor)
        {
            UnwrappedWriteCursor += SoundOutput.Buffer.Size;
        }
        AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
        AudioLatencySeconds =
            (((float32)AudioLatencyBytes / (float32)SoundOutput.BytesPerSample) / (float32)SoundOutput.SamplesPerSecond);
#endif
        LinuxFillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);

        END_BLOCK();

        //
        //
        //

#if GAME_INTERNAL
        BEGIN_BLOCK("Debug Collation");

        // Reload code if necessary
        ino_t GameLibId = LinuxFileId(SourceGameCodeDLLFullPath);
        bool32 ExecutableNeedsToBeReloaded = (GameLibId != Game.GameLibID);

        GameMemory.ExecutableReloaded = false;
        if(ExecutableNeedsToBeReloaded)
        {
            LinuxCompleteAllWork(&HighPriorityQueue);
            LinuxCompleteAllWork(&LowPriorityQueue);
            DEBUGSetEventRecording(false);
        }

        if(Game.DEBUGFrameEnd)
        {
            Game.DEBUGFrameEnd(&GameMemory, NewInput, &RenderCommands);
        }

        if(ExecutableNeedsToBeReloaded)
        {
            bool32 IsValid = false;
            for(uint32 LoadTryIndex = 0;
                !IsValid && (LoadTryIndex < 100);
                ++LoadTryIndex)
            {
                IsValid = LinuxLoadGameCode(&Game, SourceGameCodeDLLFullPath, GameLibId);
                usleep(100000);
            }

            GameMemory.ExecutableReloaded = true;
            DEBUGSetEventRecording(Game.IsValid);
        }

        END_BLOCK();
#endif

        //
        //
        //

#if 0
        BEGIN_BLOCK("Framerate Wait");

        if (!GlobalPause)
        {
            struct timespec WorkCounter = LinuxGetWallClock();
            float32 WorkSecondsElapsed = LinuxGetSecondsElapsed(LastCounter, WorkCounter);

            float32 SecondsElapsedForFrame = WorkSecondsElapsed;
            if (SecondsElapsedForFrame < TargetSecondsPerFrame)
            {
                uint32 SleepUs = (uint32)(0.99e6f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
                usleep(SleepUs);
                while (SecondsElapsedForFrame < TargetSecondsPerFrame)
                {
                    SecondsElapsedForFrame = LinuxGetSecondsElapsed(LastCounter, LinuxGetWallClock());
                }
            }
            else
            {
                // Missed frame rate
            }
        }
        END_BLOCK();
#endif
    }
#endif

    return 0;
}

#include "linux_audio.cpp"

// TODO temporary! this is a bad idea! already compiled in main.cpp
#include <km_common/km_input.cpp>