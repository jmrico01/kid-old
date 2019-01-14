#pragma once

#include "opengl.h"
#include "main_platform.h"

#define SPRITE_BATCH_SIZE 128

struct SpriteStateGL
{
	GLuint vertexArray;
	GLuint vertexBuffer;
	GLuint uvBuffer;
	GLuint programID;
};

struct RenderState
{
	SpriteStateGL spriteStateGL;
};

struct SpriteDataGL
{
	int numSprites;
	Vec3 pos[SPRITE_BATCH_SIZE];
	Vec2 size[SPRITE_BATCH_SIZE];
	Vec4 uvInfo[SPRITE_BATCH_SIZE];

	// TODO temporary, probably make a sprite sheet and put this in uvInfo
	GLuint texture[SPRITE_BATCH_SIZE];
};

bool InitRenderState(RenderState& renderState,
	const ThreadContext* thread,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);

void PushSpriteWorldSpace(SpriteDataGL* spriteDataGL,
	Vec2 pos, Vec2 size, Vec2 anchor, bool32 flipHorizontal, GLuint texture);

void DrawSprites(const RenderState& renderState,
	const SpriteDataGL& spriteDataGL, Mat4 view, Mat4 proj);