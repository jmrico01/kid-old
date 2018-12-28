#pragma once

#include <stdint.h>
#include <cstddef>

// STATIC REDEFINES
#define internal        static
#define local_persist	static
#define global_var		static

/*
GAME_INTERNAL:
0 - build for public release
1 - build for developer only

GAME_SLOW:
0 - slow code NOT allowed
1 - slow code allowed (asserts, etc)

GAME_WIN32:
defined if compiling for Win32 platform

GAME_PLATFORM_CODE:
defined on the platform-specific compilation unit
*/

#define ARRAY_COUNT(arrayName) (sizeof(arrayName) / sizeof((arrayName)[0]))

// Added (val - val) here to force integral promotion to the size of Value
#define ALIGN_POW2(val, alignment) \
    ((val + ((alignment) - 1)) & ~((val - val) + (alignment) - 1))
#define ALIGN4(val) ((val + 3) & ~3)
#define ALIGN8(val) ((val + 7) & ~7)
#define ALIGN16(val) ((val + 15) & ~15)

#define KILOBYTES(bytes)		((bytes) * 1024LL)
#define MEGABYTES(bytes)		(KILOBYTES(bytes) * 1024LL)
#define GIGABYTES(bytes)		(MEGABYTES(bytes) * 1024LL)
#define TERABYTES(bytes)		(GIGABYTES(bytes) * 1024LL)

// NUMERIC TYPES
typedef int8_t      int8;
typedef int16_t     int16;
typedef int32_t     int32;
typedef int64_t     int64;
typedef int32_t     bool32;

typedef uint8_t     uint8;
typedef uint16_t    uint16;
typedef uint32_t    uint32;
typedef uint64_t    uint64;

typedef float       float32;
typedef double      float64;

#define INT16_MINVAL -32768
#define INT16_MAXVAL 32767