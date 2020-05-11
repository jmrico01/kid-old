#pragma once

#include <km_common/km_lib.h>
#include <km_platform/main_platform.h>

#include "asset_texture.h"
#include "opengl.h"
#include "render.h"

const uint64 ANIMATION_MAX_FRAMES = 32;
const uint64 SPRITE_MAX_ANIMATIONS = 8;
const uint64 ANIMATION_QUEUE_MAX_LENGTH = 4;

enum class AnimatedSpriteId
{
    KID,
    PAPER,
    
    COUNT
};

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
    HashKey startAnimationKey;
    Vec2Int textureSize;
};

bool LoadAnimatedSprite(AnimatedSprite* sprite, const Array<char>& name, float32 pixelsPerUnit, MemoryBlock transient);
void UnloadAnimatedSprite(AnimatedSprite* sprite);
