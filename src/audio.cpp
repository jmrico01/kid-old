#include "audio.h"

#include "main.h"
#include "km_debug.h"

internal inline float32 MidiNoteToFreq(int midiNote)
{
    return 261.625565f * powf(2.0f, (midiNote - 60.0f) / 12.0f);
}

internal void DrawAudioBuffer(
    const GameState* gameState, const GameAudio* audio,
    const float32* buffer, int bufferSizeSamples, int channel,
    const int marks[], const Vec4 markColors[], int numMarks,
    Vec3 origin, Vec2 size, Vec4 color,
    MemoryBlock transient)
{
    DEBUG_ASSERT(transient.size >= sizeof(LineGLData));
    DEBUG_ASSERT(bufferSizeSamples <= MAX_LINE_POINTS);

    LineGLData* lineData = (LineGLData*)transient.memory;
    Mat4 proj = Mat4::one;
    Mat4 view = Mat4::one;
    
    lineData->count = bufferSizeSamples;
    for (int i = 0; i < bufferSizeSamples; i++) {
        float32 val = buffer[i * audio->channels + channel];
        float32 t = (float32)i / (bufferSizeSamples - 1);
        lineData->pos[i] = {
            origin.x + t * size.x,
            origin.y + size.y * val,
            origin.z
        };
    }
    DrawLine(gameState->lineGL, proj, view, lineData, color);

    lineData->count = 2;
    for (int i = 0; i < numMarks; i++) {
        float32 tMark = (float32)marks[i] / (bufferSizeSamples - 1);
        lineData->pos[0] = Vec3 {
            origin.x + tMark * size.x,
            origin.y,
            origin.z
        };
        lineData->pos[1] = Vec3 {
            origin.x + tMark * size.x,
            origin.y + size.y,
            origin.z
        };
        DrawLine(gameState->lineGL, proj, view, lineData, markColors[i]);
    }
}

internal void SoundInit(const ThreadContext* thread,
    const GameAudio* audio, Sound* sound,
    const char* filePath,
    MemoryBlock* transient,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
    sound->play = false;
    sound->playing = false;
    sound->sampleIndex = 0;

    LoadWAV(thread, filePath,
        audio, &sound->buffer,
        transient,
        DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory);
}

internal void SoundUpdate(const GameAudio* audio, Sound* sound)
{
    if (sound->playing) {
        sound->sampleIndex += audio->sampleDelta;
        if (sound->sampleIndex >= sound->buffer.bufferSizeSamples) {
            sound->playing = false;
        }
    }
    if (sound->play) {
        sound->play = false;
        sound->playing = true;
        sound->sampleIndex = 0;
    }
}

internal void SoundWriteSamples(const Sound* sound, float32 amplitude,
    GameAudio* audio)
{
    if (!sound->playing) {
        return;
    }

    const AudioBuffer* buffer = &sound->buffer;
    int samplesToWrite = audio->fillLength;
    if (sound->sampleIndex + samplesToWrite > buffer->bufferSizeSamples) {
        samplesToWrite = buffer->bufferSizeSamples - sound->sampleIndex;
    }
    for (int i = 0; i < samplesToWrite; i++) {
        int sampleInd = sound->sampleIndex + i;
        float32 sample1 = amplitude
            * buffer->buffer[sampleInd * audio->channels];
        float32 sample2 = amplitude
            * buffer->buffer[sampleInd * audio->channels + 1];

        audio->buffer[i * audio->channels] += sample1;
        audio->buffer[i * audio->channels + 1] += sample2;
    }
}

void InitAudioState(const ThreadContext* thread,
    AudioState* audioState, GameAudio* audio,
    MemoryBlock* transient,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
    audioState->globalMute = false;

    const int KICK_VARIATIONS = 1;
    const char* kickSoundFiles[KICK_VARIATIONS] = {
        "data/audio/kick.wav"
    };
    const int SNARE_VARIATIONS = 1;
    const char* snareSoundFiles[SNARE_VARIATIONS] = {
        "data/audio/snare.wav"
    };
    const int DEATH_VARIATIONS = 1;
    const char* deathSoundFiles[DEATH_VARIATIONS] = {
        "data/audio/death.wav"
    };

    SoundInit(thread, audio,
        &audioState->soundKick,
        kickSoundFiles[0],
        transient,
        DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory);
    SoundInit(thread, audio,
        &audioState->soundSnare,
        snareSoundFiles[0],
        transient,
        DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory);
    SoundInit(thread, audio,
        &audioState->soundDeath,
        deathSoundFiles[0],
        transient,
        DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory);

    for (int i = 0; i < 12; i++) {
        char buf[128];
        sprintf(buf, "data/audio/note%d.wav", i);
        SoundInit(thread, audio,
            &audioState->soundNotes[i],
            buf,
            transient,
            DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory);
    }

#if GAME_INTERNAL
    audioState->debugView = false;
#endif
}

void OutputAudio(GameAudio* audio, GameState* gameState,
    const GameInput* input, MemoryBlock transient)
{
    DEBUG_ASSERT(audio->sampleDelta >= 0);
    DEBUG_ASSERT(audio->channels == 2); // Stereo support only
    AudioState* audioState = &gameState->audioState;

#if 0
    SoundUpdate(audio, &audioState->soundKick);
    SoundUpdate(audio, &audioState->soundSnare);
    SoundUpdate(audio, &audioState->soundDeath);
    for (int i = 0; i < 12; i++) {
        SoundUpdate(audio, &audioState->soundNotes[i]);
    }
#endif

    for (int i = 0; i < audio->fillLength; i++) {
        audio->buffer[i * audio->channels] = 0.0f;
        audio->buffer[i * audio->channels + 1] = 0.0f;
    }

    if (audioState->globalMute) {
        return;
    }

#if 0
    SoundWriteSamples(&audioState->soundKick, 1.0f, audio);
    SoundWriteSamples(&audioState->soundSnare, 0.7f, audio);
    SoundWriteSamples(&audioState->soundDeath, 0.5f, audio);
    for (int i = 0; i < 12; i++) {
        SoundWriteSamples(&audioState->soundNotes[i], 0.2f, audio);
    }
#endif
}

void DrawDebugAudioInfo(const GameAudio* audio, GameState* gameState,
    const GameInput* input, ScreenInfo screenInfo, MemoryBlock transient)
{
    AudioState* audioState = &gameState->audioState;

    if (WasKeyPressed(input, KM_KEY_K)) {
        audioState->debugView = !audioState->debugView;
    }

    if (audioState->debugView) {
        const Vec4 COLOR = { 0.1f, 0.1f, 0.1f, 1.0f };
        const int MARGIN = 10;
        const Vec2 TEXT_ANCHOR = { 1.0f, 0.0f };

        char strBuf[128];
        Vec2Int audioInfoStride = {
            0,
            -((int)gameState->fontFaceSmall.height + 6)
        };
        Vec2Int audioInfoPos = {
            screenInfo.size.x - MARGIN,
            MARGIN + AbsInt(audioInfoStride.y) * 4
        };
        DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
            "Audio Engine", audioInfoPos, TEXT_ANCHOR,
            COLOR,
            transient
        );
        const char* muteStr = audioState->globalMute ? "Global Mute: ON" : "Global Mute: OFF";
        audioInfoPos += audioInfoStride;
        DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
            muteStr, audioInfoPos, TEXT_ANCHOR,
            COLOR,
            transient
        );
        sprintf(strBuf, "Sample Rate: %d", audio->sampleRate);
        audioInfoPos += audioInfoStride;
        DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
            strBuf, audioInfoPos, TEXT_ANCHOR,
            COLOR,
            transient
        );
        sprintf(strBuf, "Channels: %d", audio->channels);
        audioInfoPos += audioInfoStride;
        DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
            strBuf, audioInfoPos, TEXT_ANCHOR,
            COLOR,
            transient
        );

        DrawAudioBuffer(gameState, audio,
            audio->buffer, audio->fillLength, 0,
            nullptr, nullptr, 0,
            Vec3 { -1.0f, 0.5f, 0.0f }, Vec2 { 2.0f, 1.0f },
            COLOR,
            transient
        );
        DrawAudioBuffer(gameState, audio,
            audio->buffer, audio->fillLength, 1,
            nullptr, nullptr, 0,
            Vec3 { -1.0f, -0.5f, 0.0f }, Vec2 { 2.0f, 1.0f },
            COLOR,
            transient
        );
    }
}