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
	Mat4 transform[SPRITE_BATCH_SIZE];
	Vec4 uvInfo[SPRITE_BATCH_SIZE];
    float32 alpha[SPRITE_BATCH_SIZE];

	// TODO temporary, probably make a sprite sheet and put this in uvInfo
    // otherwise I think it's very inefficient, and prevents batching
	GLuint texture[SPRITE_BATCH_SIZE];
};

Mat4 CalculateTransform(Vec2 pos, Vec2 size, Vec2 anchor, Quat rot);

bool InitRenderState(RenderState& renderState,
	const ThreadContext* thread,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);

void PushSprite(SpriteDataGL* spriteDataGL,
	Mat4 transform, float32 alpha, bool32 flipHorizontal, GLuint texture);

void DrawSprites(const RenderState& renderState,
	const SpriteDataGL& spriteDataGL, Mat4 view, Mat4 proj);