#pragma once

#include "km_defines.h"
#include "km_math.h"
#include "km_input.h"
#include "opengl.h"

#define MIDI_IN_QUEUE_SIZE 256

struct ThreadContext
{
	int placeholder;
};

// ---------------------------- Platform functions ----------------------------
#if GAME_INTERNAL

struct DEBUGReadFileResult
{
	uint64 size;
	void* data;
};

#define DEBUG_PLATFORM_PRINT_FUNC(name) void name(const char* format, ...)
typedef DEBUG_PLATFORM_PRINT_FUNC(DEBUGPlatformPrintFunc);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY_FUNC(name) \
    void name(const ThreadContext* thread, DEBUGReadFileResult* file)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY_FUNC(DEBUGPlatformFreeFileMemoryFunc);

#define DEBUG_PLATFORM_READ_FILE_FUNC(name) \
    DEBUGReadFileResult name(const ThreadContext* thread, const char* fileName)
typedef DEBUG_PLATFORM_READ_FILE_FUNC(DEBUGPlatformReadFileFunc);

#define DEBUG_PLATFORM_WRITE_FILE_FUNC(name) \
    bool32 name(const ThreadContext* thread, const char* fileName, \
        uint32 memorySize, const void* memory)
typedef DEBUG_PLATFORM_WRITE_FILE_FUNC(DEBUGPlatformWriteFileFunc);

#endif

#define MAX_KEYS_PER_FRAME 256

struct ScreenInfo
{
    bool32 changed;

    Vec2Int size;

	int8 colorBits;
	int8 alphaBits;
	int8 depthBits;
	int8 stencilBits;
};

struct GameButtonState
{
	int transitions;
	bool32 isDown;
};

struct GameControllerInput
{
	bool32 isConnected;

	Vec2 leftStart;
	Vec2 leftEnd;
	Vec2 rightStart;
	Vec2 rightEnd;

	union
	{
		GameButtonState buttons[6];
		struct
		{
			GameButtonState a;
			GameButtonState b;
			GameButtonState x;
			GameButtonState y;
			GameButtonState lShoulder;
			GameButtonState rShoulder;
		};
	};
};

struct MidiMessage
{
    uint8 status;
    uint8 dataByte1;
    uint8 dataByte2;
};

struct MidiInput
{
    int numMessages;
    MidiMessage messages[MIDI_IN_QUEUE_SIZE];
};

struct GameInput
{
	GameButtonState mouseButtons[5];
    Vec2Int mousePos;
    Vec2Int mouseDelta;
	int mouseWheel;
    int mouseWheelDelta;

    GameButtonState keyboard[KM_KEY_LAST];
    char keyboardString[MAX_KEYS_PER_FRAME];
    uint32 keyboardStringLen;

	GameControllerInput controllers[4];

    MidiInput midiIn;
};

struct GameAudio
{
    int sampleRate;
    int channels;
    int bufferSizeSamples;
    float32* buffer;

    int sampleDelta; // Samples elapsed/written since last update
    int fillLength;
};

struct PlatformFunctions
{
#if GAME_INTERNAL
	DEBUGPlatformPrintFunc*			    DEBUGPlatformPrint;
	DEBUGPlatformFreeFileMemoryFunc*	DEBUGPlatformFreeFileMemory;
	DEBUGPlatformReadFileFunc*			DEBUGPlatformReadFile;
	DEBUGPlatformWriteFileFunc*			DEBUGPlatformWriteFile;
#endif

    OpenGLFunctions glFunctions;
};

struct MemoryBlock
{
    uint64 size;
    // Required to be cleared to zero at startup
    void* memory;
};

struct GameMemory
{
	bool32 isInitialized;

	//uint64 permanentStorageSize;
	// Required to be cleared to zero at startup
	//void* permanentStorage;

    MemoryBlock permanent;
    MemoryBlock transient;

	//uint64 transientStorageSize;
	//void* transientStorage;

#if GAME_INTERNAL
    bool32 DEBUGShouldInitGlobalFuncs;
#endif
};

// ------------------------------ Game functions ------------------------------
#define GAME_UPDATE_AND_RENDER_FUNC(name) void name( \
    const ThreadContext* thread, \
    const PlatformFunctions* platformFuncs, \
    const GameInput* input, ScreenInfo screenInfo, float32 deltaTime, \
	GameMemory* memory, GameAudio* audio)
typedef GAME_UPDATE_AND_RENDER_FUNC(GameUpdateAndRenderFunc);