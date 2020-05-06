#pragma once

#include <km_common/km_debug.h>
#include <km_common/km_log.h>
#include <km_common/km_math.h>

#include "alphabet.h"
#include "animation.h"
#include "audio.h"
#include "collision.h"
#include "framebuffer.h"
#include "load_level.h"
#include "load_png.h"
#include "opengl.h"
#include "opengl_base.h"
#include "text.h"

const uint64 NUM_FRAMEBUFFERS_COLOR_DEPTH = 1;
const uint64 NUM_FRAMEBUFFERS_COLOR = 2;
const uint64 NUM_FRAMEBUFFERS_GRAY = 1;

Vec2Int GetBorderSize(ScreenInfo screenInfo, float32 targetAspectRatio, float32 minBorderFrac);

enum class PlayerState
{
    GROUNDED,
    JUMPING,
    FALLING,
    
    COUNT
};

enum class TextureId
{
    ROCK,
    PIXEL,
    FRAME_CORNER,
    LUT_BASE,
    
    COUNT
};

enum class AnimatedSpriteId
{
    KID,
    PAPER,
    
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
    TextureWithPosition* spritePtr;
    Vec2 offset;
    float32 placementOffsetX;
    float32 coordYPrev;
};

struct GameAssets
{
    LevelData levels[LevelId::COUNT];
    
    TextureGL textures[TextureId::COUNT];
    AnimatedSprite animatedSprites[AnimatedSpriteId::COUNT];
    
#if 0
    TextureGL textureRock;
    
    TextureGL texturePixel;
    TextureGL textureFrameCorner;
    TextureGL textureLutBase;
    
    AnimatedSprite spriteKid;
    AnimatedSprite spritePaper;
#endif
    
    Alphabet alphabet;
    
    FontFace fontFaceSmall;
    FontFace fontFaceMedium;
    
    GLuint screenShader;
    GLuint bloomExtractShader;
    GLuint bloomBlendShader;
    GLuint blurShader;
    GLuint grainShader;
    GLuint lutShader;
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
