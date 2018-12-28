#pragma once

#include "main_platform.h"
#include "opengl.h"

GLuint LoadPNGOpenGL(const ThreadContext* thread,
    const char* fileName,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);