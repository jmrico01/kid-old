#pragma once

#include "km_defines.h"
#include "km_lib.h"

#define LOG_BUFFER_SIZE KILOBYTES(4)

enum LogCategory
{
	LOG_CATEGORY_ERROR = 0,
	LOG_CATEGORY_WARNING,
	LOG_CATEGORY_INFO
};

const char* LOG_CATEGORY_NAMES[] = {
	"ERROR",
	"WARN ",
	"INFO "
};

struct LogState
{
	uint64 readIndex;
	uint64 writeIndex;
	char buffer[LOG_BUFFER_SIZE];

	void PrintFormat(LogCategory logCategory,
		const char* file, int line, const char* function,
		const char* format, ...);
};

#define PLATFORM_FLUSH_LOGS_FUNC(name) void name(LogState* logState)
typedef PLATFORM_FLUSH_LOGS_FUNC(PlatformFlushLogsFunc);

global_var LogState* logState_;
global_var PlatformFlushLogsFunc* flushLogs_;

#define LOG_ERROR(format, ...) logState_->PrintFormat(LOG_CATEGORY_ERROR, \
	__FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) logState_->PrintFormat(LOG_CATEGORY_WARNING, \
	__FILE__, __LINE__, __func__, format, ##__VA_ARGS__);
#define LOG_INFO(format, ...) logState_->PrintFormat(LOG_CATEGORY_INFO, \
	__FILE__, __LINE__, __func__, format, ##__VA_ARGS__)