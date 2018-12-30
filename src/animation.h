#pragma once

#include "opengl.h"
#include "opengl_base.h"
#include "main_platform.h"

#define ANIMATION_MAX_FRAMES 32
#define ANIMATION_FPS 24

struct Animation
{
	int frames;
	int currentFrame;
	float32 currentFrameTime;

	GLuint frameTextures[ANIMATION_MAX_FRAMES];

	void Update(float32 deltaTime, bool32 moving);

	void Draw(TexturedRectGL texturedRectGL, ScreenInfo screenInfo,
		Vec2Int pos, Vec2 anchor, Vec2Int size, bool32 flipHorizontal);
};

Animation LoadAnimation(const ThreadContext* thread,
	int frames, const char* path,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);