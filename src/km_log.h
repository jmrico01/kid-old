#pragma once

#include "km_defines.h"
#include "km_lib.h"

#define LOG_BUFFER_SIZE KILOBYTES(4)

struct LogState
{
	uint64 readIndex;
	uint64 writeIndex;
	char buffer[LOG_BUFFER_SIZE];

	void PrintFormat(const char* format, ...);
};

global_var LogState* logState_;

#define LOG_INFO(format, ...) logState_->PrintFormat(format, ##__VA_ARGS__)