#pragma once

#include "km_math.h"
#include "main_platform.h"
#include "opengl.h"

struct TextureGL
{
	Vec2Int size;
	GLuint textureID;
};

bool32 LoadPNGOpenGL(const ThreadContext* thread,
    const char* filePath, TextureGL& outTextureGL,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);