#pragma once

#include <km_common/km_defines.h>
#include <sys/types.h>

#include "main_platform.h"

#define LINUX_STATE_FILE_NAME_COUNT  512
#define BYTES_PER_PIXEL 4

struct LinuxWindowDimension
{
    uint32 Width;
    uint32 Height;
};

struct LinuxState
{
    uint64 gameMemorySize;
    void* gameMemoryBlock;

    int32 recordingHandle;
    int32 inputRecordingIndex;

    int32 playbackHandle;
    int32 inputPlayingIndex;

    char exeFilePath[LINUX_STATE_FILE_NAME_COUNT];
    char* exeOnePastLastSlash;
};


struct LinuxGameCode
{
    void* gameLibHandle;
    ino_t gameLibId;

    // NOTE: Callbacks can be 0!  You must check before calling
    GameUpdateAndRenderFunc* gameUpdateAndRender;

    bool32 isValid;
};