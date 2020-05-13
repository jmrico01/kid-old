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

struct GameState
{
    AudioState audioState;
    
    float32 aspectRatio;
    int refPixelScreenHeight;
    float32 refPixelsPerUnit;
    float32 minBorderFrac;
    int borderRadius;
    float32 cameraOffsetFracY;
    
    Vec2 cameraPos;
    Quat cameraRot; // TODO Come on dude, who needs Quaternions in a 2D game
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
    
    GrabbedObjectInfo grabbedObject;
    LiftedObjectInfo liftedObject;
    
    AnimatedSpriteInstance kid;
    AnimatedSpriteInstance paper;
    Rock rock;
    
    LevelId activeLevelId;
    
    float32 grainTime;
    
#if GAME_INTERNAL
    bool kmKey;
    bool debugView;
    float32 editorScaleExponent;
    
    // Editor
    int floorVertexSelected;
#endif
    
    FT_Library ftLibrary;
    
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
    
    GameAssets assets;
};

Vec2Int GetBorderSize(ScreenInfo screenInfo, float32 targetAspectRatio, float32 minBorderFrac);

Vec2 UpdateAnimatedSprite(AnimatedSpriteInstance* sprite, const GameAssets& assets, float32 deltaTime,
                          const Array<HashKey>& nextAnimations);
void DrawAnimatedSprite(const AnimatedSpriteInstance& sprite, const GameAssets& assets, SpriteDataGL* spriteDataGL,
                        Vec2 pos, Vec2 size, Vec2 anchor, Quat rot, float32 alpha, bool flipHorizontal);
