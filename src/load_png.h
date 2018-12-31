#pragma once

#include "km_math.h"
#include "main_platform.h"
#include "opengl.h"

struct TextureGL
{
	Vec2Int size;
	GLuint textureID;
};

TextureGL LoadPNGOpenGL(const ThreadContext* thread,
    const char* fileName,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);