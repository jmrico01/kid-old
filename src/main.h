#pragma once

#include "animation.h"
#include "audio.h"
#include "gui.h"
#include "km_math.h"
#include "load_png.h"
#include "opengl.h"
#include "opengl_base.h"
#include "text.h"

#define NUM_FRAMEBUFFERS_COLOR_DEPTH  1
#define NUM_FRAMEBUFFERS_COLOR        2
#define NUM_FRAMEBUFFERS_GRAY         1

enum FramebufferState
{
    FBSTATE_NONE,        // No setup has been done yet
    FBSTATE_INITIALIZED, // Framebuffer object has been generated
    FBSTATE_COLOR,       // Color attachment created, status is complete
    FBSTATE_COLOR_DEPTH  // Color and depth attachments created, status is complete
};

struct Framebuffer
{
    GLuint framebuffer;
    GLuint color;
    GLuint depth;
    FramebufferState state = FBSTATE_NONE;
};

struct ObjectStatic
{
    Vec2Int pos;
    Vec2 anchor;
    TextureGL texture;
};

struct ObjectAnimated
{
    Vec2Int pos;
    Vec2 anchor;
    Animation animation;
    
#if GAME_INTERNAL
    ClickableBox box;
#endif
};

struct GameState
{
    AudioState audioState;

    // GAME DATA --------------------------------------------------------------
    Vec2Int cameraPos;
    Vec2Int playerPos;
    Vec2Int playerVel;
    bool32 falling;
    bool32 facingRight;

    float32 grainTime;

#if GAME_INTERNAL
    bool32 debugView;
    bool32 editor;
#endif
    // ------------------------------------------------------------------------

    RectGL rectGL;
    TexturedRectGL texturedRectGL;
    LineGL lineGL;
    TextGL textGL;

    FT_Library ftLibrary;
    FontFace fontFaceSmall;
    FontFace fontFaceMedium;

    Framebuffer framebuffersColorDepth[NUM_FRAMEBUFFERS_COLOR_DEPTH];
    Framebuffer framebuffersColor[NUM_FRAMEBUFFERS_COLOR];
    Framebuffer framebuffersGray[NUM_FRAMEBUFFERS_GRAY];
    // TODO make a vertex array struct probably
    GLuint screenQuadVertexArray;
    GLuint screenQuadVertexBuffer;
    GLuint screenQuadUVBuffer;

    GLuint screenShader;
    GLuint bloomExtractShader;
    GLuint bloomBlendShader;
    GLuint blurShader;
    GLuint grainShader;

    Animation animationKid;
    Animation animationMe;

    ObjectStatic background;
    ObjectStatic clouds;

    ObjectAnimated guys;
    ObjectAnimated bush;
};

inline float32 RandFloat32();
inline float32 RandFloat32(float32 min, float32 max);
