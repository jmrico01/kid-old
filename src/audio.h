#pragma once

#include <km_common/km_defines.h>

#include "asset_audio.h"

struct Sound
{
	bool play;
    bool playing;
	uint64 sampleIndex;
    
	AudioBuffer buffer;
};

struct AudioState
{
	Sound soundJump;
    
	bool globalMute;
    
#if GAME_INTERNAL
	bool debugView;
#endif
};

struct GameState;
template <typename Allocator>
bool InitAudioState(Allocator* allocator, AudioState* audioState, GameAudio* audio);
void OutputAudio(GameAudio* audio, GameState* gameState, const GameInput& input,
                 MemoryBlock transient);

#if GAME_INTERNAL
void DrawDebugAudioInfo(const GameAudio* audio, GameState* gameState,
                        const GameInput& input, ScreenInfo screenInfo, MemoryBlock transient, Vec4 debugFontColor);
#endif
