#include "load_wav.h"

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

bool32 LoadWAV(const ThreadContext* thread, const char* filePath,
	const GameAudio* audio, AudioBuffer* audioBuffer,
	MemoryBlock* transient,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
	DEBUGReadFileResult wavFile = DEBUGPlatformReadFile(thread, filePath);
	if (!wavFile.data) {
		LOG_INFO("Failed to open WAV file at: %s\n", filePath);
		return false;
	}

	ChunkRIFF* riff = (ChunkRIFF*)wavFile.data;
	if (riff->header.c1 != 'R' || riff->header.c2 != 'I'
	|| riff->header.c3 != 'F' || riff->header.c4 != 'F') {
		LOG_INFO("Invalid RIFF header on file %s\n", filePath);
		return false;
	}
	if (riff->w != 'W' || riff->a != 'A'
	|| riff->v != 'V' || riff->e != 'E') {
		LOG_INFO("Not a WAVE file: %s\n", filePath);
		return false;
	}

	ChunkHeader* fmtHeader = (ChunkHeader*)(riff + 1);
	if (fmtHeader->c1 != 'f' || fmtHeader->c2 != 'm' || fmtHeader->c3 != 't') {
		LOG_INFO("Invalid fmt header on file: %s\n", filePath);
		return false;
	}
	WaveFormat* format = (WaveFormat*)(fmtHeader + 1);
	if (format->audioFormat != WAVE_FORMAT_IEEE_FLOAT) {
		LOG_INFO("WAV format isn't IEEE float (%d) for %s\n",
			format->audioFormat, filePath);
		return false;
	}
	/*if (format->sampleRate != audio->sampleRate) {
		LOG_INFO("WAV file sample rate mismatch: %d vs %d, for %s\n",
			format->sampleRate, audio->sampleRate, filePath);
		return false;
	}
	if (format->channels != audio->channels) {
		LOG_INFO("WAV file channels mismatch: %d vs %d, for %s\n",
			format->channels, audio->channels, filePath);
		return false;
	}*/

	int bytesRead = sizeof(ChunkRIFF) + sizeof(ChunkHeader)
		+ fmtHeader->dataSize;
	ChunkHeader* header = (ChunkHeader*)((char*)format + fmtHeader->dataSize);
	while (header->c1 != 'd' || header->c2 != 'a'
	|| header->c3 != 't' || header->c4 != 'a') {
		/*LOG_INFO("skipped chunk: %c%c%c%c\n",
			header->c1, header->c2, header->c3, header->c4);*/
		int bytesToSkip = sizeof(ChunkHeader) + header->dataSize;
		if (bytesRead + bytesToSkip >= wavFile.size) {
			LOG_INFO("WAV file has no data chunk: %s\n", filePath);
			return false;
		}
		header = (ChunkHeader*)((char*)header + bytesToSkip);
		bytesRead += bytesToSkip;
	}

	void* data = (void*)(header + 1);
	int bytesPerSample = format->bitsPerSample / 8;
	int lengthSamples = header->dataSize / bytesPerSample / format->channels;
	if (lengthSamples > AUDIO_MAX_SAMPLES) {
		LOG_INFO("WAV file too long: %s\n", filePath);
		return false;
	}

	if (format->sampleRate != audio->sampleRate) {
		DEBUG_ASSERT(transient->size >= sizeof(AudioBuffer));
		AudioBuffer* origBuffer = (AudioBuffer*)transient->memory;
		MemCopy(origBuffer->buffer, data, header->dataSize);
		int targetLengthSamples = (int)((float32)lengthSamples /
			format->sampleRate * audio->sampleRate);
		for (int i = 0; i < targetLengthSamples; i++) {
			float32 t = (float32)i / (targetLengthSamples - 1);
			float32 sample1 = LinearSample(audio,
				origBuffer->buffer, lengthSamples, 0, t);
			float32 sample2 = LinearSample(audio,
				origBuffer->buffer, lengthSamples, 1, t);
			audioBuffer->buffer[i * audio->channels] = sample1;
			audioBuffer->buffer[i * audio->channels + 1] = sample2;
		}
		audioBuffer->bufferSizeSamples = targetLengthSamples;
	}
	else {
		MemCopy(audioBuffer->buffer, data, header->dataSize);
		audioBuffer->bufferSizeSamples = lengthSamples;
	}

	audioBuffer->sampleRate = audio->sampleRate;
	audioBuffer->channels = audio->channels;

	// LOG_INFO("Loaded WAV file: %s\n", filePath);
	// LOG_INFO("Samples: %d\n", audioBuffer->bufferSizeSamples);

	DEBUGPlatformFreeFileMemory(thread, &wavFile);

	return true;
}

float32 LinearSample(const GameAudio* audio,
	const float32* buffer, int bufferLengthSamples,
	int channel, float32 t)
{
	float32 iFloat = t * bufferLengthSamples;
	int i1 = (int)floorf(iFloat);
	int i2 = (int)ceilf(iFloat);
	float32 val1 = buffer[i1 * audio->channels + channel];
	float32 val2 = buffer[i2 * audio->channels + channel];
	return Lerp(val1, val2, iFloat - i1);
}