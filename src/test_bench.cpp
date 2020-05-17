#include <intrin.h>
#include <stdio.h>
#include <stdlib.h>

#include "load_psd.h"

void LogString(const char* string, uint64 n)
{
    fprintf(stderr, "%.*s", (int)n, string);
}

void PlatformFlushLogs(LogState* logState)
{
    // TODO fix this
    for (uint64 i = 0; i < logState->eventCount; i++) {
        uint64 eventIndex = (logState->eventFirst + i) % LOG_EVENTS_MAX;
        const LogEvent& event = logState->logEvents[eventIndex];
        uint64 bufferStart = event.logStart;
        uint64 bufferEnd = event.logStart + event.logSize;
        if (bufferEnd >= LOG_BUFFER_SIZE) {
            bufferEnd -= LOG_BUFFER_SIZE;
        }
        if (bufferEnd >= bufferStart) {
            LogString(logState->buffer + bufferStart, event.logSize);
        }
        else {
            LogString(logState->buffer + bufferStart, LOG_BUFFER_SIZE - bufferStart);
            LogString(logState->buffer, bufferEnd);
        }
    }

    logState->eventFirst = (logState->eventFirst + logState->eventCount) % LOG_EVENTS_MAX;
    logState->eventCount = 0;
    // uint64 toRead1, toRead2;
    // if (logState->readIndex <= logState->writeIndex) {
    //  toRead1 = logState->writeIndex - logState->readIndex;
    //  toRead2 = 0;
    // }
    // else {
    //  toRead1 = LOG_BUFFER_SIZE - logState->readIndex;
    //  toRead2 = logState->writeIndex;
    // }
    // if (toRead1 != 0) {
    //  LogString(logState->buffer + logState->readIndex, toRead1);
    // }
    // if (toRead2 != 0) {
    //  LogString(logState->buffer, toRead2);
    // }
    // logState->readIndex += toRead1 + toRead2;
    // if (logState->readIndex >= LOG_BUFFER_SIZE) {
    //  logState->readIndex -= LOG_BUFFER_SIZE;
    // }
}

int main(int argc, char** argv)
{
    LogState* logState = (LogState*)malloc(sizeof(LogState));
    logState->eventFirst = 0;
    logState->eventCount = 0;
    logState_ = logState;

    const uint64 memorySize = GIGABYTES(1);
    void* memory = malloc(memorySize);

    const_string psdFilePath = ToString("data/levels/overworld/overworld.psd");
    PsdFile psdFile;
    if (!OpenPSD(&defaultAllocator_, psdFilePath, &psdFile)) {
        LOG_ERROR("Couldn't open overworld.psd file\n");
        LOG_FLUSH();
        return 1;
    }

    uint64 layerInd = psdFile.layers.size;
    for (uint64 i = 0; i < psdFile.layers.size; i++) {
        if (StringEquals(psdFile.layers[i].name.ToArray(), ToString("bg"))) {
            layerInd = i;
            break;
        }
    }
    if (layerInd == psdFile.layers.size) {
        LOG_ERROR("Couldn't find layer 'bg'\n");
        LOG_FLUSH();
        return 1;
    }

    LOG_INFO("Starting benchmark\n");

    const int ITERATIONS = 256;

    ImageData imageData;
    uint64 cyclesStart = __rdtsc();
    for (int i = 0; i < ITERATIONS; i++) {
        LinearAllocator allocator(memorySize, memory);
        if (!psdFile.LoadLayerImageData(layerInd, LayerChannelID::RED, &allocator, &imageData)) {
            LOG_ERROR("LoadLayerImageData failed!\n");
            LOG_FLUSH();
            return 1;
        }
    }
    uint64 cyclesEnd = __rdtsc();

    uint64 totalCycles = cyclesEnd - cyclesStart;
    double cyclesPerIteration = (double)totalCycles / (double)ITERATIONS;
    double totalMegacycles = (double)totalCycles / 1000000.;
    LOG_INFO("CYCLES\n");
    LOG_INFO("    total elapsed: %.03f Mcycles\n", totalMegacycles);
    LOG_INFO("    per iteration: %.0f cycles\n", cyclesPerIteration);
    LOG_FLUSH();

    return 0;
}

#include "load_psd.cpp"

#define STB_SPRINTF_IMPLEMENTATION
#include <stb_sprintf.h>
#undef STB_SPRINTF_IMPLEMENTATION

#include <km_common/km_input.cpp>
#include <km_common/km_kmkv.cpp>
#include <km_common/km_lib.cpp>
#include <km_common/km_log.cpp>
#include <km_common/km_memory.cpp>
#include <km_common/km_os.cpp>
#include <km_common/km_string.cpp>