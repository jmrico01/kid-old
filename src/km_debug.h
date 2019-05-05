#pragma once

#include <stdlib.h>

#include "km_defines.h"
#include "km_log.h"
#include "main_platform.h"

#if GAME_SLOW
#define DEBUG_ASSERT(expression) if (!(expression)) { \
	LOG_ERROR("Assert failed\n", __FILE__, __LINE__, __func__); \
	flushLogs_(logState_); \
	abort(); }
#define DEBUG_PANIC(format, ...) \
	LOG_ERROR("PANIC!\n"); \
	LOG_ERROR(format, ##__VA_ARGS__); \
	flushLogs_(logState_); \
    abort();
#else
#define DEBUG_ASSERT(expression)
#define DEBUG_PANIC(format, ...)
#endif
