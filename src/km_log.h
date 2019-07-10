#pragma once

#include "km_defines.h"
#include "km_lib.h"
#include "km_string.h"

#define LOG_BUFFER_SIZE KILOBYTES(128)
#define LOG_EVENTS_MAX ((LOG_BUFFER_SIZE) / 32)

#define FUNCTION_NAME_MAX_LENGTH 128

enum LogCategory
{
	LOG_CATEGORY_ERROR = 0,
	LOG_CATEGORY_WARNING,
	LOG_CATEGORY_INFO,
    LOG_CATEGORY_DEBUG
};

const char* LOG_CATEGORY_NAMES[] = {
	"ERROR",
	"WARN",
	"INFO",
    "DEBUG"
};

struct LogEvent
{
    LogCategory category;
    char file[PATH_MAX_LENGTH];
    int line;
    char function[FUNCTION_NAME_MAX_LENGTH];
    uint64 logStart;
    uint64 logSize;
};

struct LogState
{
    uint64 eventFirst, eventCount;
    LogEvent logEvents[LOG_EVENTS_MAX];
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
	__FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) logState_->PrintFormat(LOG_CATEGORY_INFO, \
	__FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#if GAME_SLOW
#define LOG_DEBUG(format, ...) logState_->PrintFormat(LOG_CATEGORY_DEBUG, \
    __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#else
#define LOG_DEBUG(format, ...)
#endif