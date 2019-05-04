#pragma once

#include <stdlib.h>

#include "km_defines.h"
#include "km_log.h"
#include "main_platform.h"

#if GAME_SLOW
//global_var DEBUGPlatformPrintFunc* debugPrint_;
#define DEBUG_ASSERT(expression) if (!(expression)) { \
	LOG_INFO("Assert failed, file %s line %d function %s\n", __FILE__, __LINE__, __func__); \
	flushLogs_(logState_); \
	abort(); }
#define DEBUG_PANIC(format, ...) \
	LOG_INFO("Panic at file %s line %d function %s\n", __FILE__, __LINE__, __func__); \
	LOG_INFO(format, ##__VA_ARGS__); \
	flushLogs_(logState_); \
    abort();
//#define DEBUG_PRINT(format, ...) debugPrint_(format, ##__VA_ARGS__)
#else
#define DEBUG_ASSERT(expression)
#define DEBUG_PANIC(format, ...)
//#define DEBUG_PRINT(format, ...)
#endif
