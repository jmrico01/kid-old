#pragma once

#include <km_common/km_defines.h>
#include <sys/types.h>

#include "main_platform.h"

#define MACOS_STATE_FILE_NAME_COUNT  512

struct MacOSState
{
	uint64 gameMemorySize;
	void* gameMemoryBlock;
};

struct MacOSGameCode
{
	void* gameLibHandle;
	ino_t gameLibId;

	// NOTE: Callbacks can be 0!  You must check before calling
	GameUpdateAndRenderFunc* gameUpdateAndRender;

	bool32 isValid;
};