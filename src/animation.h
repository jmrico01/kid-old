#pragma once

#include <km_common/km_lib.h>
#include <km_platform/main_platform.h>

#include "opengl.h"
#include "load_png.h"
#include "render.h"

#define ANIMATION_MAX_FRAMES 32
#define SPRITE_MAX_ANIMATIONS 8
#define ANIMATION_QUEUE_MAX_LENGTH 4

struct Animation
{
    int fps;
    int numFrames;
    bool loop;
    TextureGL frameTextures[ANIMATION_MAX_FRAMES];
    int frameTiming[ANIMATION_MAX_FRAMES];
    float32 frameTime[ANIMATION_MAX_FRAMES];
    HashTable<int> frameExitTo[ANIMATION_MAX_FRAMES];
    bool rootMotion;
    bool rootFollow;
    bool rootFollowEndLoop;
    Vec2 frameRootMotion[ANIMATION_MAX_FRAMES];
    Vec2 frameRootAnchor[ANIMATION_MAX_FRAMES];
};

struct AnimatedSprite
{
    HashTable<Animation> animations;
    HashKey startAnimation;
    Vec2Int textureSize;
    
    bool Load(const Array<char>& name, float32 pixelsPerUnit, const MemoryBlock& transient);
    void Unload();
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
              float32 alpha, bool flipHorizontal) const;
};
