#pragma once

#include <km_platform/main_platform.h>

#include "opengl.h"

#define SPRITE_BATCH_SIZE 128

struct SpriteStateGL
{
	GLuint vertexArray;
	GLuint vertexBuffer;
	GLuint uvBuffer;
    GLuint multiplyProgramID;
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
    // otherwise it's very inefficient, prevents batching
	GLuint texture[SPRITE_BATCH_SIZE];
};

Mat4 CalculateTransform(Vec2 pos, Vec2 size, Vec2 anchor,
    Quat baseRot, Quat rot, bool32 flip);
Mat4 CalculateTransform(Vec2 pos, Vec2 size, Vec2 anchor, Quat rot, bool32 flip);

template <typename Allocator>
bool InitRenderState(Allocator* allocator, RenderState& renderState);

void PushSprite(SpriteDataGL* spriteDataGL, Mat4 transform, float32 alpha, GLuint texture);

void DrawSprites(const RenderState& renderState,
	const SpriteDataGL& spriteDataGL, Mat4 transform);
