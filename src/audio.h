#pragma once

#include <km_common/km_defines.h>

#include "load_wav.h"

struct Sound
{
	bool32 play;
	bool32 playing;
	uint64 sampleIndex;

	AudioBuffer buffer;
};

struct AudioState
{
	Sound soundJump;

	bool32 globalMute;

#if GAME_INTERNAL
	bool32 debugView;
#endif
};

struct GameState;
template <typename Allocator>
bool32 InitAudioState(const ThreadContext* thread, Allocator* allocator,
	AudioState* audioState, GameAudio* audio);
void OutputAudio(GameAudio* audio, GameState* gameState,
	const GameInput* input, MemoryBlock transient);

#if GAME_INTERNAL
void DrawDebugAudioInfo(const GameAudio* audio, GameState* gameState,
	const GameInput* input, ScreenInfo screenInfo, MemoryBlock transient,
	Vec4 debugFontColor);
#endif
