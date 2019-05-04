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

#define PLATFORM_FLUSH_LOGS_FUNC(name) void name(LogState* logState)
typedef PLATFORM_FLUSH_LOGS_FUNC(PlatformFlushLogsFunc);

global_var LogState* logState_;
global_var PlatformFlushLogsFunc* flushLogs_;

#define LOG_INFO(format, ...) logState_->PrintFormat(format, ##__VA_ARGS__)