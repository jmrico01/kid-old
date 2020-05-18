#pragma once

#include <km_common/km_debug.h>
#include <km_common/km_log.h>
#include <km_common/km_math.h>

#include "alphabet.h"
#include "asset.h"
#include "asset_animation.h"
#include "asset_level.h"
#include "asset_texture.h"
#include "audio.h"
#include "collision.h"
#include "framebuffer.h"
#include "opengl.h"
#include "opengl_base.h"
#include "text.h"

const uint64 NUM_FRAMEBUFFERS_COLOR_DEPTH = 1;
const uint64 NUM_FRAMEBUFFERS_COLOR = 2;
const uint64 NUM_FRAMEBUFFERS_GRAY = 1;

enum class PlayerState
{
    GROUNDED,
    JUMPING,
    FALLING,

    COUNT
};

struct Rock
{
    Vec2 coords;
    float32 angle;
};

struct GrabbedObjectInfo
{
    Vec2* coordsPtr;
    Vec2 rangeX, rangeY;
};

struct LiftedObjectInfo
{
    SpriteMetadata* spritePtr;
    Vec2 offset;
    float32 placementOffsetX;
    float32 coordYPrev;
};

struct AnimatedSpriteInstance
{
    AnimatedSpriteId animatedSpriteId;

    HashKey activeAnimationKey;
    int activeFrame;
    int activeFrameRepeat;
    float32 activeFrameTime;
};

struct LevelState
{
    LevelId activeLevelId;

    Vec2 cameraPos;
    Quat cameraRot;
    Vec2 cameraCoords;

    float32 prevFloorCoordY;
    Vec2 playerCoords;
    Vec2 playerVel;
    PlayerState playerState;
    const LineCollider* currentPlatform;
    bool facingRight;
    float32 playerJumpMag;
    bool playerJumpHolding;
    float32 playerJumpHold;

    AnimatedSpriteInstance kid;

    GrabbedObjectInfo grabbedObject;
    LiftedObjectInfo liftedObject;
};

struct GameState
{
    GameAssets assets;

    AudioState audioState;

    AnimatedSpriteInstance paper;
    Rock rock;

    float32 aspectRatio;
    int refPixelScreenHeight;
    float32 refPixelsPerUnit;
    float32 minBorderFrac;
    int borderRadius;
    float32 cameraOffsetFracY;
    float32 grainTime;

    LevelState levelState;

    bool kmKey;
    bool debugView;
    float32 editorScaleExponent;
    int floorVertexSelected;

    RenderState renderState;
    RectGL rectGL;
    TexturedRectGL texturedRectGL;
    LineGL lineGL;
    TextGL textGL;

    Framebuffer framebuffersColorDepth[NUM_FRAMEBUFFERS_COLOR_DEPTH];
    Framebuffer framebuffersColor[NUM_FRAMEBUFFERS_COLOR];
    Framebuffer framebuffersGray[NUM_FRAMEBUFFERS_GRAY];
    // TODO make a vertex array struct probably
    GLuint screenQuadVertexArray;
    GLuint screenQuadVertexBuffer;
    GLuint screenQuadUVBuffer;
};

Vec2Int GetBorderSize(ScreenInfo screenInfo, float32 targetAspectRatio, float32 minBorderFrac);

Vec2 UpdateAnimatedSprite(AnimatedSpriteInstance* sprite, const GameAssets& assets, float32 deltaTime,
                          const Array<HashKey>& nextAnimations);
void DrawAnimatedSprite(const AnimatedSpriteInstance& sprite, const GameAssets& assets, SpriteDataGL* spriteDataGL,
                        Vec2 pos, Vec2 size, Vec2 anchor, Quat rot, float32 alpha, bool flipHorizontal);
