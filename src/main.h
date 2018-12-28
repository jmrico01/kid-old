#pragma once

#include "km_math.h"
#include "opengl.h"
#include "opengl_base.h"
#include "text.h"
#include "particles.h"
#include "audio.h"
#include "load_obj.h"

#define NUM_FRAMEBUFFERS_COLOR  3
#define NUM_FRAMEBUFFERS_GRAY   2

struct GameState
{
    AudioState audioState;

    // Game data --------------------------
    Vec3 pos;
    float32 yaw;
    float32 pitch;

    Quat lightRot;

    float32 grainTime;

    MeshGL testMeshGL;
    // ------------------------------------

    RectGL rectGL;
    TexturedRectGL texturedRectGL;
    LineGL lineGL;
    TextGL textGL;

    FT_Library ftLibrary;
    FontFace fontFaceSmall;
    FontFace fontFaceMedium;

    GLuint framebuffersColor[NUM_FRAMEBUFFERS_COLOR];
    GLuint colorBuffersColor[NUM_FRAMEBUFFERS_COLOR];
    GLuint framebuffersGray[NUM_FRAMEBUFFERS_GRAY];
    GLuint colorBuffersGray[NUM_FRAMEBUFFERS_GRAY];
    GLuint depthBuffer;
    GLuint gBuffer;
    GLuint gPosition;
    GLuint gNormal;
    GLuint gColor;
    GLuint screenQuadVertexArray;
    GLuint screenQuadVertexBuffer;
    GLuint screenQuadUVBuffer;

    GLuint gbufferShader;
    GLuint deferredShader;
    GLuint screenShader;
    GLuint ssaoShader;
    GLuint ssaoBlurShader;
    GLuint ssaoNoiseTexture;
    GLuint fxaaShader;
    GLuint bloomExtractShader;
    GLuint bloomBlendShader;
    GLuint blurShader;
    GLuint grainShader;
    GLuint grainTexture;

    GLuint pTexBase;
    ParticleSystem ps;
};

inline float32 RandFloat32();
inline float32 RandFloat32(float32 min, float32 max);
