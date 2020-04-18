#include "audio.h"

#include <km_common/km_debug.h>

#include "main.h"

template <typename Allocator>
internal bool SoundInit(Allocator* allocator, const GameAudio* audio, Sound* sound,
                        const char* filePath)
{
	sound->play = false;
	sound->playing = false;
	sound->sampleIndex = 0;
    
	return LoadWAV(allocator, filePath, audio, &sound->buffer);
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
	uint64 samplesToWrite = audio->fillLength;
	if (sound->sampleIndex + samplesToWrite > buffer->bufferSizeSamples) {
		samplesToWrite = buffer->bufferSizeSamples - sound->sampleIndex;
	}
	for (uint64 i = 0; i < samplesToWrite; i++) {
		uint64 sampleInd = sound->sampleIndex + i;
		float32 sample1 = amplitude
			* buffer->buffer[sampleInd * audio->channels];
		float32 sample2 = amplitude
			* buffer->buffer[sampleInd * audio->channels + 1];
        
		audio->buffer[i * audio->channels] += sample1;
		audio->buffer[i * audio->channels + 1] += sample2;
	}
}

template <typename Allocator>
bool InitAudioState(Allocator* allocator, AudioState* audioState, GameAudio* audio)
{
	// audioState->globalMute = false;
	audioState->globalMute = true;
    
	if (!SoundInit(allocator, audio, &audioState->soundJump, "data/audio/yow.wav")) {
		LOG_ERROR("Failed to init jump sound");
		return false;
	}
    
#if GAME_INTERNAL
	audioState->debugView = false;
#endif
    
	return true;
}

void OutputAudio(GameAudio* audio, GameState* gameState, const GameInput& input,
                 MemoryBlock transient)
{
	DEBUG_ASSERT(audio->sampleDelta >= 0);
	DEBUG_ASSERT(audio->channels == 2); // Stereo support only
	AudioState* audioState = &gameState->audioState;
    
	SoundUpdate(audio, &audioState->soundJump);
    
	for (int i = 0; i < audio->fillLength; i++) {
		audio->buffer[i * audio->channels] = 0.0f;
		audio->buffer[i * audio->channels + 1] = 0.0f;
	}
    
	if (audioState->globalMute) {
		return;
	}
    
	SoundWriteSamples(&audioState->soundJump, 1.0f, audio);
}

#if GAME_INTERNAL

internal void DrawAudioBuffer(
                              const GameState* gameState, const GameAudio* audio,
                              const float32* buffer, uint64 bufferSizeSamples, uint8 channel,
                              const int marks[], const Vec4 markColors[], int numMarks,
                              Vec3 origin, Vec2 size, Vec4 color,
                              MemoryBlock transient)
{
	DEBUG_ASSERT(transient.size >= sizeof(LineGLData));
	DEBUG_ASSERT(bufferSizeSamples <= MAX_LINE_POINTS);
    
	LineGLData* lineData = (LineGLData*)transient.memory;
	
	lineData->count = (int)bufferSizeSamples;
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
                        const GameInput& input, ScreenInfo screenInfo, MemoryBlock transient,
                        Vec4 debugFontColor)
{
	AudioState* audioState = &gameState->audioState;
    
	if (WasKeyPressed(input, KM_KEY_H)) {
		audioState->debugView = !audioState->debugView;
	}
    
	if (audioState->debugView) {
		LinearAllocator tempAllocator(transient.size, transient.memory);
        
		const int PILLARBOX_WIDTH = GetBorderSize(screenInfo, gameState->aspectRatio,
                                                  gameState->minBorderFrac).x;
		const Vec2Int MARGIN = { 30, 45 };
		const Vec2 TEXT_ANCHOR = { 1.0f, 0.0f };
        
		const int STR_BUF_LENGTH = 128;
		char strBuf[STR_BUF_LENGTH];
		Vec2Int audioInfoStride = {
			0,
			-((int)gameState->fontFaceSmall.height + 6)
		};
		Vec2Int audioInfoPos = {
			screenInfo.size.x - MARGIN.x - PILLARBOX_WIDTH,
			MARGIN.y + AbsInt(audioInfoStride.y) * 4
		};
		DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
                 ToString("Audio Engine"), audioInfoPos, TEXT_ANCHOR,
                 debugFontColor,
                 &tempAllocator
                 );
		const char* muteStr = audioState->globalMute ? "Global Mute: ON" : "Global Mute: OFF";
		audioInfoPos += audioInfoStride;
		DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
                 ToString(muteStr), audioInfoPos, TEXT_ANCHOR,
                 debugFontColor,
                 &tempAllocator
                 );
		stbsp_snprintf(strBuf, STR_BUF_LENGTH, "Sample Rate: %d", audio->sampleRate);
		audioInfoPos += audioInfoStride;
		DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
                 ToString(strBuf), audioInfoPos, TEXT_ANCHOR,
                 debugFontColor,
                 &tempAllocator
                 );
		stbsp_snprintf(strBuf, STR_BUF_LENGTH, "Channels: %d", audio->channels);
		audioInfoPos += audioInfoStride;
		DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
                 ToString(strBuf), audioInfoPos, TEXT_ANCHOR,
                 debugFontColor,
                 &tempAllocator
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