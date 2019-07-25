#pragma once

#include <km_lib.h>

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
    bool32 loop;
	TextureGL frameTextures[ANIMATION_MAX_FRAMES];
    int frameTiming[ANIMATION_MAX_FRAMES];
    HashTable<int> frameExitTo[ANIMATION_MAX_FRAMES];
    bool32 rootMotion;
    bool32 rootFollow;
    bool32 rootFollowEndLoop;
    Vec2 frameRootMotion[ANIMATION_MAX_FRAMES];
    Vec2 frameRootAnchor[ANIMATION_MAX_FRAMES];
};

struct AnimatedSprite
{
    HashTable<Animation> animations;
    HashKey startAnimation;
	Vec2Int textureSize;
};

struct AnimatedSpriteInstance
{
    const AnimatedSprite* animatedSprite;

    HashKey activeAnimation;
    int activeFrame;
    int activeFrameRepeat;
    float32 activeFrameTime;

    Vec2 Update(float32 deltaTime, const Array<HashKey>& nextAnimations);

    void Draw(SpriteDataGL* spriteDataGL, Vec2 pos, Vec2 size, Vec2 anchor, Quat rot,
        float32 alpha, bool32 flipHorizontal) const;
};

bool32 LoadAnimatedSprite(const ThreadContext* thread, const char* filePath,
	AnimatedSprite& outAnimatedSprite, MemoryBlock transient,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);