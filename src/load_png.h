#pragma once

#include <km_common/km_math.h>
#include <km_platform/main_platform.h>

#include "opengl.h"

struct TextureGL
{
	Vec2Int size;
	GLuint textureID;
};

void UnloadTextureGL(const TextureGL& textureGL);

// TODO change ref out to pointer
bool LoadPNGOpenGL(const ThreadContext* thread, const char* filePath,
	GLint magFilter, GLint minFilter, GLint wrapS, GLint wrapT,
	TextureGL& outTextureGL, MemoryBlock transient,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);
