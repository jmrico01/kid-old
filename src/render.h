#pragma once

#include "opengl.h"
#include "main_platform.h"

#define SPRITE_BATCH_SIZE 1024

struct RenderState
{
	GLuint vertexArray;
	GLuint vertexBuffer;
	GLuint uvBuffer;
	GLuint posBuffer;
	GLuint sizeBuffer;
	GLuint uvInfoBuffer;
	GLuint programID;
};

struct SpriteDataGL
{
	Vec3 pos[SPRITE_BATCH_SIZE];
	Vec2 size[SPRITE_BATCH_SIZE];
	Vec4 uvInfo[SPRITE_BATCH_SIZE];
};

bool InitRenderState(RenderState& renderState,
	const ThreadContext* thread,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);

void DrawSpriteBatch(const RenderState& renderState, const SpriteDataGL& spriteDataGL);