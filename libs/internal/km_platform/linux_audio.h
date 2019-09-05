#pragma once

#include <alsa/asoundlib.h>
#include <km_common/km_defines.h>

#define AUDIO_CHANNELS 2
#define AUDIO_SAMPLERATE 44100
#define AUDIO_NUM_PERIODS 32
#define AUDIO_PERIOD_SIZE 256

struct LinuxAudio
{
    snd_pcm_t* pcmHandle;
    snd_pcm_uframes_t periodSize;
    
    uint32 channels;
    uint32 sampleRate;

    int16* buffer;
    uint32 bufferSize;
    uint32 readIndex;

    pthread_t thread;
    bool32 isPlaying;
};

internal LinuxAudio globalAudio;
internal pthread_mutex_t globalAudioMutex;

bool LinuxInitAudio(LinuxAudio* audio,
    uint32 channels, uint32 sampleRate, uint32 periodSize, uint32 periods);