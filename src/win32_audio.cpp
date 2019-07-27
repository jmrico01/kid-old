#include "win32_audio.h"

#include <km_common/km_debug.h>
#include <km_common/km_lib.h>
#include <km_common/km_log.h>

// REFERENCE_TIME as defined by Windows API
#define REFERENCE_TIME_NANOSECONDS 100
// Careful when using this value: close to int overflow
#define REFERENCE_TIMES_PER_MILLISECOND \
    (1000000 / REFERENCE_TIME_NANOSECONDS)

void CALLBACK MidiInputCallback(
    HMIDIIN hMidiIn, UINT wMsg,
    DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    Win32Audio* winAudio = (Win32Audio*)dwInstance;
    while (winAudio->midiInBusy) {
        Sleep(0); // TODO use better thread sync thingy
    }
    if (winAudio->midiIn.numMessages >= MIDI_IN_QUEUE_SIZE) {
        return;
    }
    winAudio->midiInBusy = true;
    switch (wMsg) {
        case MIM_OPEN: {
            LOG_INFO("MIDI input opened\n");
        } break;
        case MIM_CLOSE: {
            LOG_INFO("MIDI input closed\n");
        } break;
        case MIM_DATA: {
            //LOG_INFO("MIDI input data message\n");
            MidiMessage msg;
            msg.status = (uint8)(dwParam1 & 0xff);
            msg.dataByte1 = (uint8)((dwParam1 >> 8) & 0xff);
            msg.dataByte2 = (uint8)((dwParam1 >> 16) & 0xff);
            winAudio->midiIn.messages[winAudio->midiIn.numMessages] = msg;
            winAudio->midiIn.numMessages++;
        } break;
    }
    winAudio->midiInBusy = false;
}

bool32 Win32InitAudio(Win32Audio* winAudio, int bufferSizeMilliseconds)
{
    // TODO release/CoTaskMemFree on failure
    // and in general
    HRESULT hr;

    IMMDeviceEnumerator* deviceEnumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&deviceEnumerator);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create device enumerator\n");
        return false;
    }

    IMMDevice* device;
    hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get default audio endpoint\n");
        return false;
    }

    IAudioClient* audioClient;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
        (void**)&audioClient);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to activate audio device\n");
        return false;
    }

    WAVEFORMATEX* format;
    hr = audioClient->GetMixFormat(&format);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get audio device format\n");
        return false;
    }

    // TODO do this differently: query for several formats with some priority
    //  e.g. 1st PCM float, 2nd PCM int16 ext, 3rd PCM int16
    LOG_INFO("---------- Audio device format ----------\n");
    LOG_INFO("Sample rate: %d\n", format->nSamplesPerSec);
    LOG_INFO("Channels: %d\n", format->nChannels);
    LOG_INFO("Bits per sample: %d\n", format->wBitsPerSample);
    LOG_INFO("Block align: %d\n", format->nBlockAlign);
    switch (format->wFormatTag) {
        case WAVE_FORMAT_PCM: {
            LOG_INFO("Format: PCM\n");
            winAudio->format = AUDIO_FORMAT_PCM_INT16;
            winAudio->bitsPerSample = (int)format->wBitsPerSample;
        } break;
        case WAVE_FORMAT_EXTENSIBLE: {
            if (sizeof(WAVEFORMATEX) + format->cbSize
            < sizeof(WAVEFORMATEXTENSIBLE)) {
                LOG_ERROR("Extended format, invalid structure size\n");
                return false;
            }

            WAVEFORMATEXTENSIBLE* formatExt = (WAVEFORMATEXTENSIBLE*)format;
            LOG_INFO("Valid bits per sample: %d\n",
                formatExt->Samples.wValidBitsPerSample);
            LOG_INFO("Channel mask: %d\n", formatExt->dwChannelMask);
            if (formatExt->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
                LOG_INFO("Format: PCM ext\n");
                winAudio->format = AUDIO_FORMAT_PCM_INT16;
                winAudio->bitsPerSample =
                    (int)formatExt->Samples.wValidBitsPerSample; // TODO wrong
            }
            else if (formatExt->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
                LOG_INFO("Format: IEEE float\n");
                winAudio->format = AUDIO_FORMAT_PCM_FLOAT32;
                winAudio->bitsPerSample =
                    (int)formatExt->Samples.wValidBitsPerSample; // TODO wrong
            }
            else {
                LOG_ERROR("Unrecognized audio device ext format: %d\n",
                    formatExt->SubFormat);
                return false;
            }
        } break;
        default: {
            LOG_ERROR("Unrecognized audio device format: %d\n",
                format->wFormatTag);
            return false;
        } break;
    }
    LOG_INFO("-----------------------------------------\n");

    REFERENCE_TIME bufferSizeRefTimes = REFERENCE_TIMES_PER_MILLISECOND
        * bufferSizeMilliseconds;
    hr = audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        bufferSizeRefTimes,
        0,
        format,
        NULL
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to initialize audio client\n");
        return false;
    }

    UINT32 bufferSizeFrames;
    hr = audioClient->GetBufferSize(&bufferSizeFrames);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get audio buffer size\n");
        return false;
    }

    IAudioRenderClient* renderClient;
    hr = audioClient->GetService(__uuidof(IAudioRenderClient),
        (void**)&renderClient);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get audio render client\n");
        return false;
    }

    IAudioClock* audioClock;
    hr = audioClient->GetService(__uuidof(IAudioClock),
        (void**)&audioClock);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get audio clock\n");
        return false;
    }

    // Dummy get/release calls for setup purposes
    BYTE* buffer;
    hr = renderClient->GetBuffer(0, &buffer);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get audio render client buffer\n");
        return false;
    }
    hr = renderClient->ReleaseBuffer(0, 0);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to release audio render client buffer\n");
        return false;
    }

    winAudio->sampleRate = (int)format->nSamplesPerSec;
    winAudio->channels = (int)format->nChannels;
    winAudio->bufferSizeSamples = (int)bufferSizeFrames;

    winAudio->audioClient = audioClient;
    winAudio->renderClient = renderClient;
    winAudio->audioClock = audioClock;

    hr = audioClient->Start();
    if (FAILED(hr)) {
        LOG_ERROR("Failed to start audio client\n");
        return false;
    }

    // Setup MIDI input
    winAudio->midiInBusy = false;
    UINT midiInDevs = midiInGetNumDevs();
    if (midiInDevs > 0) {
        int midiInDeviceID = 0;
        MMRESULT res;
        MIDIINCAPS midiInCaps;
        res = midiInGetDevCaps(midiInDeviceID,
            &midiInCaps, sizeof(MIDIINCAPS));
        if (res != MMSYSERR_NOERROR) {
            LOG_WARN("Couldn't get MIDI input device caps\n");
            return true;
        }

        LOG_INFO("MIDI input device: %s\n", midiInCaps.szPname);
        HMIDIIN midiInHandle;
        res = midiInOpen(&midiInHandle, midiInDeviceID,
            (DWORD_PTR)MidiInputCallback, (DWORD_PTR)winAudio,
            CALLBACK_FUNCTION);
        if (res != MMSYSERR_NOERROR) {
            LOG_WARN("Couldn't open MIDI input\n");
            return true;
        }

        MIDIHDR midiHeader;
        int midiBufferSizeBytes = MEGABYTES(1);
        midiHeader.lpData = (LPSTR)VirtualAlloc(0, (size_t)midiBufferSizeBytes,
            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        midiHeader.dwBufferLength = midiBufferSizeBytes;
        midiHeader.dwFlags = 0;
        res = midiInPrepareHeader(midiInHandle, &midiHeader, sizeof(MIDIHDR));
        if (res != MMSYSERR_NOERROR) {
            LOG_WARN("Couldn't prepare MIDI input header\n");
            return true;
        }

        res = midiInStart(midiInHandle);
        if (res != MMSYSERR_NOERROR) {
            LOG_WARN("Couldn't start MIDI input\n");
            return true;
        }
    }
    else {
        LOG_WARN("No MIDI input devices detected\n");
    }

    return true;
}

void Win32StopAudio(Win32Audio* winAudio)
{
    winAudio->audioClient->Stop();
}

void Win32WriteAudioSamples(const Win32Audio* winAudio,
    const GameAudio* gameAudio, int numSamples)
{
    DEBUG_ASSERT(numSamples <= winAudio->bufferSizeSamples);

    BYTE* audioBuffer;
    HRESULT hr = winAudio->renderClient->GetBuffer((UINT32)numSamples,
        &audioBuffer);
    if (SUCCEEDED(hr)) {
        switch (winAudio->format) {
            case AUDIO_FORMAT_PCM_FLOAT32: {
                float32* winBuffer = (float32*)audioBuffer;
                for (int i = 0; i < numSamples; i++) {
                    winBuffer[i * winAudio->channels] =
                        gameAudio->buffer[i * gameAudio->channels];
                    winBuffer[i * winAudio->channels + 1] =
                        gameAudio->buffer[i * gameAudio->channels + 1];
                }
                // Probably this is more efficient & worth checking for?
                /*if (gameAudio->sampleRate == winAudio->sampleRate
                && gameAudio->channels == winAudio->channels) {
                    int bytesToWrite = numSamples * winAudio->channels
                        * winAudio->bitsPerSample / 8;
                    MemCopy(audioBuffer, gameAudio->buffer, bytesToWrite);
                }*/
            } break;
            case AUDIO_FORMAT_PCM_INT16: {
                int16* winBuffer = (int16*)audioBuffer;
                for (int i = 0; i < numSamples; i++) {
                    winBuffer[i * winAudio->channels] = (int16)(
                        INT16_MAXVAL *
                        gameAudio->buffer[i * gameAudio->channels]);
                    winBuffer[i * winAudio->channels + 1] = (int16)(
                        INT16_MAXVAL *
                        gameAudio->buffer[i * gameAudio->channels + 1]);
                }
            } break;
        }
        winAudio->renderClient->ReleaseBuffer((UINT32)numSamples, 0);
    }
}