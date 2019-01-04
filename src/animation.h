#pragma once

#include "opengl.h"
#include "opengl_base.h"
#include "main_platform.h"
#include "load_png.h"

#define ANIMATION_MAX_FRAMES 32
#define SPRITE_MAX_ANIMATIONS 4
#define ANIMATION_QUEUE_MAX_LENGTH 4

struct Animation
{
	int fps;
	int numFrames;
	TextureGL frameTextures[ANIMATION_MAX_FRAMES];
	int frameExitTo[ANIMATION_MAX_FRAMES][SPRITE_MAX_ANIMATIONS];
};

struct AnimatedSprite
{
	int activeAnimation;
	int activeFrame;
	float32 activeFrameTime;

	int numAnimations;
	Animation animations[SPRITE_MAX_ANIMATIONS];

	Vec2Int textureSize;

	void Update(float32 deltaTime, int numNextAnimations, const int nextAnimations[]);

	void Draw(TexturedRectGL texturedRectGL, ScreenInfo screenInfo,
		Vec2Int pos, Vec2 anchor, Vec2Int size, bool32 flipHorizontal) const;
};

bool32 LoadAnimatedSprite(const ThreadContext* thread, const char* filePath,
	AnimatedSprite& outAnimatedSprite,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);