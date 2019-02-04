#pragma once

#include "opengl.h"
#include "main_platform.h"
#include "load_png.h"
#include "render.h"

#define ANIMATION_MAX_FRAMES 32
#define SPRITE_MAX_ANIMATIONS 8
#define ANIMATION_QUEUE_MAX_LENGTH 4

struct Animation
{
	int fps;
	int numFrames;
	TextureGL frameTextures[ANIMATION_MAX_FRAMES];
    int frameTiming[ANIMATION_MAX_FRAMES];
	int frameExitTo[ANIMATION_MAX_FRAMES][SPRITE_MAX_ANIMATIONS];
};

struct AnimatedSprite
{
	int activeAnimation;
	int activeFrame;
    int activeFrameRepeat;
	float32 activeFrameTime;

	int numAnimations;
	Animation animations[SPRITE_MAX_ANIMATIONS];

	Vec2Int textureSize;

	void Update(float32 deltaTime, int numNextAnimations, const int nextAnimations[]);

    void Draw(SpriteDataGL* spriteDataGL,
        Vec2 pos, Vec2 size, Vec2 anchor, bool32 flipHorizontal) const;
};

bool32 LoadAnimatedSprite(const ThreadContext* thread, const char* filePath,
	AnimatedSprite& outAnimatedSprite,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);