#pragma once

#include <stdlib.h>

#include "km_defines.h"
#include "main_platform.h"

#if GAME_SLOW
global_var DEBUGPlatformPrintFunc* debugPrint_;
#define DEBUG_ASSERT(expression) if (!(expression)) { \
	debugPrint_("Assert failed, file %s line %s, function %s\n", __FILE__, __LINE__, __func__); \
	abort(); }
#define DEBUG_PANIC(format, ...) debugPrint_(format, ##__VA_ARGS__); \
    abort();
#define DEBUG_PRINT(format, ...) debugPrint_(format, ##__VA_ARGS__)
#else
#define DEBUG_ASSERT(expression)
#define DEBUG_PANIC(format, ...)
#define DEBUG_PRINT(format, ...)
#endif
