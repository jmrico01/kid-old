#pragma once

#include "opengl.h"
#include "opengl_base.h"
#include "main_platform.h"

#define ANIMATION_MAX_FRAMES 32

struct Animation
{
	int fps;
	int frames;
	int currentFrame;
	float32 currentFrameTime;
	int numIdles;
	int idles[ANIMATION_MAX_FRAMES];

	GLuint frameTextures[ANIMATION_MAX_FRAMES];

	bool32 IsIdleFrame(int frame);

	void Update(float32 deltaTime, bool32 moving);

	void Draw(TexturedRectGL texturedRectGL, ScreenInfo screenInfo,
		Vec2Int pos, Vec2 anchor, Vec2Int size, bool32 flipHorizontal);
};

Animation LoadAnimation(const ThreadContext* thread,
	int fps, int frames, const char* path,
	int numIdles, int idles[],
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);