#pragma once

#include "opengl.h"
#include "main_platform.h"
#include "load_png.h"
#include "km_lib.h"
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
    Vec2 frameRootMotion[ANIMATION_MAX_FRAMES];
    Vec2 frameRootAnchor[ANIMATION_MAX_FRAMES];
    HashTable<int> frameExitTo[ANIMATION_MAX_FRAMES];
	//int frameExitTo[ANIMATION_MAX_FRAMES][SPRITE_MAX_ANIMATIONS];
};

struct AnimatedSprite
{
	HashKey activeAnimation;
	int activeFrame;
    int activeFrameRepeat;
	float32 activeFrameTime;

    HashTable<Animation> animations;

	Vec2Int textureSize;

	Vec2 Update(float32 deltaTime, int numNextAnimations, const HashKey* nextAnimations);

    void Draw(SpriteDataGL* spriteDataGL,
        Vec2 pos, Vec2 size, Vec2 anchor, bool32 flipHorizontal) const;
};

bool32 LoadAnimatedSprite(const ThreadContext* thread, const char* filePath,
	AnimatedSprite& outAnimatedSprite,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);