#pragma once

#include <AudioUnit/AudioUnit.h>
#include <km_common/km_defines.h>

#include "main_platform.h"

#define AUDIO_SAMPLERATE 48000
#define AUDIO_CHANNELS 2
#define AUDIO_BUFFER_SIZE_MILLISECONDS 1000

struct MacOSAudio
{
	AudioStreamBasicDescription audioDescriptor;
	AudioUnit audioUnit;

	int sampleRate;
	int channels;
	int bufferSizeSamples;
	int16* buffer;

	int readCursor;
	int writeCursor;

	int latency;

	bool32 midiInBusy;
	MidiInput midiIn;
};

void MacOSInitCoreAudio(MacOSAudio* macOSAudio,
	int sampleRate, int channels, int bufferSizeSamples);
void MacOSStopCoreAudio(MacOSAudio* macOSAudio);

void MacOSWriteSamples(MacOSAudio* macAudio,
	const GameAudio* gameAudio, int numSamples);