#pragma once

#include "km_defines.h"
#include "load_wav.h"

struct Sound
{
    bool32 play;
    bool32 playing;
    int sampleIndex;

    AudioBuffer buffer;
};

struct AudioState
{
    Sound soundKick;
    Sound soundSnare;
    Sound soundDeath;

    Sound soundNotes[12];

    bool32 globalMute;

#if GAME_INTERNAL
    bool32 debugView;
#endif
};

struct GameState;
void InitAudioState(const ThreadContext* thread,
    AudioState* audioState, GameAudio* audio,
    MemoryBlock* transient,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);
void OutputAudio(GameAudio* audio, GameState* gameState,
    const GameInput* input, MemoryBlock transient);

#if GAME_INTERNAL
void DrawDebugAudioInfo(const GameAudio* audio, GameState* gameState,
    const GameInput* input, ScreenInfo screenInfo, MemoryBlock transient);
#endif
