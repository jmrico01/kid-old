#include "audio.h"

#include <km_debug.h>

#include "main.h"

internal bool32 SoundInit(const ThreadContext* thread,
    const GameAudio* audio, Sound* sound,
    const char* filePath,
    MemoryBlock* transient,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
    sound->play = false;
    sound->playing = false;
    sound->sampleIndex = 0;

    return LoadWAV(thread, filePath,
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

bool32 InitAudioState(const ThreadContext* thread,
    AudioState* audioState, GameAudio* audio,
    MemoryBlock* transient,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
    // audioState->globalMute = false;
    audioState->globalMute = true;

    const int KICK_VARIATIONS = 1;
    const char* kickSoundFiles[KICK_VARIATIONS] = {
        "data/audio/yow.wav"
    };
    const int SNARE_VARIATIONS = 1;
    const char* snareSoundFiles[SNARE_VARIATIONS] = {
        "data/audio/snare.wav"
    };
    const int DEATH_VARIATIONS = 1;
    const char* deathSoundFiles[DEATH_VARIATIONS] = {
        "data/audio/death.wav"
    };

    if (!SoundInit(thread, audio,
        &audioState->soundKick,
        kickSoundFiles[0],
        transient,
        DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory)) {
        LOG_ERROR("Failed to init kick sound");
        return false;
    }
    if (!SoundInit(thread, audio,
        &audioState->soundSnare,
        snareSoundFiles[0],
        transient,
        DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory)) {
        LOG_ERROR("Failed to init snare sound");
        return false;
    }
    if (!SoundInit(thread, audio,
        &audioState->soundDeath,
        deathSoundFiles[0],
        transient,
        DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory)) {
        LOG_ERROR("Failed to init death sound");
        return false;
    }

#if GAME_INTERNAL
    audioState->debugView = false;
#endif

    return true;
}

void OutputAudio(GameAudio* audio, GameState* gameState,
    const GameInput* input, MemoryBlock transient)
{
    DEBUG_ASSERT(audio->sampleDelta >= 0);
    DEBUG_ASSERT(audio->channels == 2); // Stereo support only
    AudioState* audioState = &gameState->audioState;

    SoundUpdate(audio, &audioState->soundKick);
    SoundUpdate(audio, &audioState->soundSnare);
    SoundUpdate(audio, &audioState->soundDeath);

    for (int i = 0; i < audio->fillLength; i++) {
        audio->buffer[i * audio->channels] = 0.0f;
        audio->buffer[i * audio->channels + 1] = 0.0f;
    }

    if (audioState->globalMute) {
        return;
    }

    SoundWriteSamples(&audioState->soundKick, 1.0f, audio);
    SoundWriteSamples(&audioState->soundSnare, 0.7f, audio);
    SoundWriteSamples(&audioState->soundDeath, 0.5f, audio);
}

#if GAME_INTERNAL

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
    DrawLine(gameState->lineGL, Mat4::one, lineData, color);

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
        DrawLine(gameState->lineGL, Mat4::one, lineData, markColors[i]);
    }
}

void DrawDebugAudioInfo(const GameAudio* audio, GameState* gameState,
    const GameInput* input, ScreenInfo screenInfo, MemoryBlock transient,
    Vec4 debugFontColor)
{
    AudioState* audioState = &gameState->audioState;

    if (WasKeyPressed(input, KM_KEY_K)) {
        audioState->debugView = !audioState->debugView;
    }

    if (audioState->debugView) {
        const int PILLARBOX_WIDTH = GetPillarboxWidth(screenInfo);
        const Vec2Int MARGIN = { 30, 45 };
        const Vec2 TEXT_ANCHOR = { 1.0f, 0.0f };

        char strBuf[128];
        Vec2Int audioInfoStride = {
            0,
            -((int)gameState->fontFaceSmall.height + 6)
        };
        Vec2Int audioInfoPos = {
            screenInfo.size.x - MARGIN.x - PILLARBOX_WIDTH,
            MARGIN.y + AbsInt(audioInfoStride.y) * 4
        };
        DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
            "Audio Engine", audioInfoPos, TEXT_ANCHOR,
            debugFontColor,
            transient
        );
        const char* muteStr = audioState->globalMute ? "Global Mute: ON" : "Global Mute: OFF";
        audioInfoPos += audioInfoStride;
        DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
            muteStr, audioInfoPos, TEXT_ANCHOR,
            debugFontColor,
            transient
        );
        sprintf(strBuf, "Sample Rate: %d", audio->sampleRate);
        audioInfoPos += audioInfoStride;
        DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
            strBuf, audioInfoPos, TEXT_ANCHOR,
            debugFontColor,
            transient
        );
        sprintf(strBuf, "Channels: %d", audio->channels);
        audioInfoPos += audioInfoStride;
        DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
            strBuf, audioInfoPos, TEXT_ANCHOR,
            debugFontColor,
            transient
        );

        DrawAudioBuffer(gameState, audio,
            audio->buffer, audio->fillLength, 0,
            nullptr, nullptr, 0,
            Vec3 { -1.0f, 0.5f, 0.0f }, Vec2 { 2.0f, 1.0f },
            debugFontColor,
            transient
        );
        DrawAudioBuffer(gameState, audio,
            audio->buffer, audio->fillLength, 1,
            nullptr, nullptr, 0,
            Vec3 { -1.0f, -0.5f, 0.0f }, Vec2 { 2.0f, 1.0f },
            debugFontColor,
            transient
        );
    }
}

#endif