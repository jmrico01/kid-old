#include "load_wav.h"

#include <km_common/km_lib.h>

#define WAVE_FORMAT_PCM         0x0001
#define WAVE_FORMAT_IEEE_FLOAT  0x0003

struct ChunkHeader
{
	char c1, c2, c3, c4;
	int32 dataSize;
};

struct ChunkRIFF
{
	ChunkHeader header;
	char w, a, v, e;
};

struct WaveFormat
{
	int16 audioFormat;
	int16 channels;
	int32 sampleRate;
	int32 byteRate;
	int16 blockAlign;
	int16 bitsPerSample;
	// there might be additional data here
};

template <typename Allocator>
bool32 LoadWAV(const ThreadContext* thread, Allocator* allocator, const char* filePath,
	const GameAudio* gameAudio, AudioBuffer* audioBuffer)
{
	const auto& allocatorState = allocator->SaveState();
	defer (allocator->LoadState(allocatorState));

	PlatformReadFileResult wavFile = PlatformReadFile(thread, allocator, filePath);
	if (!wavFile.data) {
		LOG_ERROR("Failed to open WAV file at: %s\n", filePath);
		return false;
	}

	ChunkRIFF* riff = (ChunkRIFF*)wavFile.data;
	if (riff->header.c1 != 'R' || riff->header.c2 != 'I'
	|| riff->header.c3 != 'F' || riff->header.c4 != 'F') {
		LOG_ERROR("Invalid RIFF header on file %s\n", filePath);
		return false;
	}
	if (riff->w != 'W' || riff->a != 'A'
	|| riff->v != 'V' || riff->e != 'E') {
		LOG_ERROR("Not a WAVE file: %s\n", filePath);
		return false;
	}

	ChunkHeader* fmtHeader = (ChunkHeader*)(riff + 1);
	if (fmtHeader->c1 != 'f' || fmtHeader->c2 != 'm' || fmtHeader->c3 != 't') {
		LOG_ERROR("Invalid fmt header on file: %s\n", filePath);
		return false;
	}
	WaveFormat* format = (WaveFormat*)(fmtHeader + 1);
	if (format->audioFormat != WAVE_FORMAT_IEEE_FLOAT) {
		LOG_ERROR("WAV format isn't IEEE float (%d) for %s\n",
			format->audioFormat, filePath);
		return false;
	}
	/*if (format->sampleRate != audio->sampleRate) {
		LOG_ERROR("WAV file sample rate mismatch: %d vs %d, for %s\n",
			format->sampleRate, audio->sampleRate, filePath);
		return false;
	}
	if (format->channels != audio->channels) {
		LOG_ERROR("WAV file channels mismatch: %d vs %d, for %s\n",
			format->channels, audio->channels, filePath);
		return false;
	}*/

	int bytesRead = sizeof(ChunkRIFF) + sizeof(ChunkHeader)
		+ fmtHeader->dataSize;
	ChunkHeader* header = (ChunkHeader*)((char*)format + fmtHeader->dataSize);
	while (header->c1 != 'd' || header->c2 != 'a'
	|| header->c3 != 't' || header->c4 != 'a') {
		int bytesToSkip = sizeof(ChunkHeader) + header->dataSize;
		if (bytesRead + bytesToSkip >= wavFile.size) {
			LOG_ERROR("WAV file has no data chunk: %s\n", filePath);
			return false;
		}
		header = (ChunkHeader*)((char*)header + bytesToSkip);
		bytesRead += bytesToSkip;
	}

	void* data = (void*)(header + 1);
	int bytesPerSample = format->bitsPerSample / 8;
	int lengthSamples = header->dataSize / bytesPerSample / format->channels;
	if (lengthSamples > AUDIO_MAX_SAMPLES) {
		LOG_ERROR("WAV file too long: %s\n", filePath);
		return false;
	}

	if ((uint32)format->sampleRate != gameAudio->sampleRate) {
		float32* floatData = (float32*)data;
		int targetLengthSamples = (int)((float32)lengthSamples /
			format->sampleRate * gameAudio->sampleRate);
		for (int i = 0; i < targetLengthSamples; i++) {
			float32 t = (float32)i / (targetLengthSamples - 1);
			float32 sample1 = LinearSample(gameAudio, floatData, lengthSamples, 0, t);
			float32 sample2 = LinearSample(gameAudio, floatData, lengthSamples, 1, t);
			audioBuffer->buffer[i * gameAudio->channels] = sample1;
			audioBuffer->buffer[i * gameAudio->channels + 1] = sample2;
		}
		audioBuffer->bufferSizeSamples = targetLengthSamples;
	}
	else {
		MemCopy(audioBuffer->buffer, data, header->dataSize);
		audioBuffer->bufferSizeSamples = lengthSamples;
	}

	audioBuffer->sampleRate = gameAudio->sampleRate;
	audioBuffer->channels = gameAudio->channels;

	return true;
}

float32 LinearSample(const GameAudio* gameAudio,
	const float32* buffer, int bufferLengthSamples,
	int channel, float32 t)
{
	float32 iFloat = t * bufferLengthSamples;
	int i1 = (int)floorf(iFloat);
	int i2 = (int)ceilf(iFloat);
	float32 val1 = buffer[i1 * gameAudio->channels + channel];
	float32 val2 = buffer[i2 * gameAudio->channels + channel];
	return Lerp(val1, val2, iFloat - i1);
}