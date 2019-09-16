#pragma once

#include <km_common/km_debug.h>
#include <km_platform/main_platform.h>

#define AUDIO_MAX_SAMPLERATE 48000
#define AUDIO_MAX_CHANNELS 2
#define AUDIO_MAX_LENGTH_SECS 2
#define AUDIO_MAX_SAMPLES (AUDIO_MAX_SAMPLERATE * AUDIO_MAX_LENGTH_SECS)

struct AudioBuffer
{
	uint32 sampleRate;
	uint8 channels;
	uint64 bufferSizeSamples;
	float32 buffer[AUDIO_MAX_SAMPLES * AUDIO_MAX_CHANNELS];
};

template <typename Allocator>
bool32 LoadWAV(const ThreadContext* thread, Allocator* allocator, const char* filePath,
	const GameAudio* gameAudio, AudioBuffer* audioBuffer);

float32 LinearSample(const GameAudio* gameAudio,
	const float32* buffer, int bufferLengthSamples,
	int channel, float32 t);
