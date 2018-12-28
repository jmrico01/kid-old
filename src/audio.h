#pragma once

#include "km_defines.h"
#include "load_wav.h"

#define WAVE_BUFFER_LENGTH_SECONDS 1
#define WAVE_BUFFER_MAX_SAMPLES (AUDIO_MAX_SAMPLERATE \
    * WAVE_BUFFER_LENGTH_SECONDS)

#define WAVETABLE_MAX_WAVES 16
#define WAVETABLE_MAX_VOICES 16
#define WAVETABLE_OSCILLATORS 16
#define WAVETABLE_ENVELOPES 4

struct Sound
{
    bool32 play;
    bool32 playing;
    int sampleIndex;

    AudioBuffer buffer;
};

struct Voice
{
    float32 baseFreq;
    float32 maxAmp;
    int midiNote;

    float32 time;
    float32 tWave;
    float32 freq;
    float32 amp;

    bool32 sustained;
    float32 releaseTime;
    float32 releaseAmp;

    int envelope;
};

struct Oscillator
{
    float32 tWave;
    float32 freq;
    float32 amp;
};

struct Wave
{
    float32 buffer[WAVE_BUFFER_MAX_SAMPLES * AUDIO_MAX_CHANNELS];
};

struct EnvelopeADSR
{
    float32 attack;
    float32 decay;
    float32 sustain;
    float32 release;
};

struct WaveTable
{
    float32 tWaveTable;
    int bufferLengthSamples;
    int numWaves;
    Wave waves[WAVETABLE_MAX_WAVES];

    int activeVoices;
    Voice voices[WAVETABLE_MAX_VOICES];

    Oscillator oscillators[WAVETABLE_OSCILLATORS];

    EnvelopeADSR envelopes[WAVETABLE_ENVELOPES];
};

struct AudioState
{
    Sound soundKick;
    Sound soundSnare;
    Sound soundDeath;

    Sound soundNotes[12];

    WaveTable waveTable;

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
