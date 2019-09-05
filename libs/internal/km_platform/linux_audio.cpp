#include "linux_audio.h"

#include <km_common/km_debug.h>
#include <pthread.h>
#include <sys/mman.h>

internal void CopyAudioBuffer(int numSamples, int channels, const int16* src, int16* dst)
{
    for (int i = 0; i < numSamples; i++) {
        dst[i * channels] = src[i * channels];
        dst[i * channels + 1] = src[i * channels + 1];
    }
}

internal void* LinuxAudioRunner(void* data)
{
    LinuxAudio* linuxAudio = (LinuxAudio*)data;
    uint32 bufferSizeBytes = linuxAudio->bufferSize
        * linuxAudio->channels * sizeof(int16);
    int16* buffer = (int16*)mmap(NULL, bufferSizeBytes,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buffer == MAP_FAILED) {
        return nullptr;
    }

    while (linuxAudio->isPlaying) {
        pthread_testcancel();
        pthread_mutex_lock(&globalAudioMutex);
        /*uint32 regionSize1 = linuxAudio->writeIndex - linuxAudio->readIndex;
        if (linuxAudio->writeIndex < linuxAudio->readIndex) {
            regionSize1 = linuxAudio->writeIndex + linuxAudio->bufferSize
                - linuxAudio->readIndex;
        }
        if (regionSize1 > linuxAudio->periodSize) {
            regionSize1 = linuxAudio->periodSize;
        }*/
        uint32 regionSize1 = linuxAudio->periodSize;
        uint32 regionSize2 = 0;
        uint32 channels = linuxAudio->channels;
        if (linuxAudio->readIndex + regionSize1 >= linuxAudio->bufferSize) {
            uint32 diff = linuxAudio->bufferSize - linuxAudio->readIndex;
            regionSize2 = regionSize1 - diff;
            regionSize1 = diff;
            CopyAudioBuffer(regionSize1, channels,
                linuxAudio->buffer + linuxAudio->readIndex * channels,
                buffer);
            CopyAudioBuffer(regionSize2, channels,
                linuxAudio->buffer,
                buffer + regionSize1 * channels);
            linuxAudio->readIndex = regionSize2;
        }
        else {
            CopyAudioBuffer(regionSize1, channels,
                linuxAudio->buffer + linuxAudio->readIndex * channels,
                buffer);
            linuxAudio->readIndex += regionSize1;
        }
        pthread_mutex_unlock(&globalAudioMutex);

        snd_pcm_sframes_t ret = snd_pcm_writei(linuxAudio->pcmHandle,
            buffer, linuxAudio->periodSize);
        if (ret == -EPIPE) {
            snd_pcm_prepare(linuxAudio->pcmHandle);
            //LOG_INFO("Buffer underrun\n");
        }
        else if (ret < 0) {
            LOG_INFO("snd_pcm_writei error\n");
        }
    }

    free(buffer);
    pthread_exit(NULL);
}

bool LinuxInitAudio(LinuxAudio* audio,
    uint32 channels, uint32 sampleRate, uint32 periodSize, uint32 periods)
{
    int err;
    snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
    const char* pcmName = "plughw:0,0";
    err = snd_pcm_open(&audio->pcmHandle, pcmName, stream, 0);
    if (err < 0) {
        LOG_INFO("Error opening PCM device %s\n", pcmName);
        return false;
    }

    snd_pcm_hw_params_t* hwParams;
    snd_pcm_hw_params_alloca(&hwParams);
    err = snd_pcm_hw_params_any(audio->pcmHandle, hwParams);
    if (err < 0) {
        LOG_INFO("Failed to initialize hw params\n");
        return false;
    }

    // Set access
    err = snd_pcm_hw_params_set_access(audio->pcmHandle, hwParams,
        SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        LOG_INFO("Error setting audio access\n");
        return false;
    }

    // Set sample format
    err = snd_pcm_hw_params_set_format(audio->pcmHandle, hwParams,
        SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        LOG_INFO("Error setting audio format\n");
        return false;
    }

    // Set sample rate
    // If the rate is not supported by the hardware, use nearest one
    uint32 rate = sampleRate;
    err = snd_pcm_hw_params_set_rate_near(audio->pcmHandle, hwParams,
        &rate, 0);
    if (err < 0) {
        LOG_INFO("Error setting sample rate\n");
        return false;
    }
    if (rate != sampleRate) {
        LOG_INFO("Sample rate %d Hz not supported by your hardware\n"
            "    ==> Using %d Hz instead.\n", sampleRate, rate);
    }
    audio->sampleRate = rate;

    // Set number of channels
    err = snd_pcm_hw_params_set_channels(audio->pcmHandle, hwParams,
        channels);
    if (err < 0) {
        LOG_INFO("Error setting number of channels\n");
        return false;
    }
    audio->channels = channels;

    // Set number of periods
    uint32 p = periods;
    err = snd_pcm_hw_params_set_periods_near(
        audio->pcmHandle, hwParams, &p, 0);
    if (err < 0) {
        LOG_INFO("Error setting number of periods\n");
        return false;
    }

    // Set period size (in frames)
    snd_pcm_uframes_t ps = periodSize;
    err = snd_pcm_hw_params_set_period_size_near(
        audio->pcmHandle, hwParams,
        &ps, 0);
    if (err < 0) {
        LOG_INFO("Error setting period size\n");
        return false;
    }
    audio->periodSize = ps;

    // Set buffer size (in frames). The resulting latency is given by
    // latency = periodsize * periods / (rate * bytes_per_frame)
    snd_pcm_uframes_t bufferSize = periodSize * periods;
    err = snd_pcm_hw_params_set_buffer_size_near(
        audio->pcmHandle, hwParams,
        &bufferSize);
    if (err < 0) {
        LOG_INFO("Error setting buffer size\n");
        return false;
    }
    audio->bufferSize = (uint32)bufferSize;

    LOG_INFO("Sample rate: %d\n", audio->sampleRate);
    LOG_INFO("Period size (frames): %d\n", (int)audio->periodSize);
    LOG_INFO("Periods: %d\n", p);
    LOG_INFO("Buffer size (frames): %d\n", audio->bufferSize);
    LOG_INFO("Buffer size (secs): %f\n",
        (float)audio->bufferSize / audio->sampleRate);

    // Apply HW parameter settings to PCM device and prepare device
    err = snd_pcm_hw_params(audio->pcmHandle, hwParams);
    if (err < 0) {
        LOG_INFO("Error setting HW params\n");
        return false;
    }

    audio->readIndex = 0;
    audio->buffer = (int16*)malloc(bufferSize * channels * sizeof(int16));

    // Start audio thread
    pthread_mutex_init(&globalAudioMutex, NULL);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    audio->isPlaying = true;
    int result = pthread_create(&audio->thread, &attr,
        LinuxAudioRunner, audio);
    pthread_attr_destroy(&attr);
    if (result != 0) {
        LOG_INFO("Failed to create audio thread\n");
        return false;
    }

    return true;
}
