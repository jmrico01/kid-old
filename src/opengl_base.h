#pragma once

#include <km_common/km_math.h>
#include <km_platform/main_platform.h>

#include "opengl.h"

#define MAX_LINE_POINTS 100000

struct RectGL
{
    GLuint vertexArray;
    GLuint vertexBuffer;
    GLuint programID;
};

struct TexturedRectGL
{
    GLuint vertexArray;
    GLuint vertexBuffer;
    GLuint uvBuffer;
    GLuint programID;
};

struct LineGL
{
    GLuint vertexArray;
    GLuint vertexBuffer;
    GLuint programID;
};

struct PlaneGL
{
    GLuint vertexArray;
    GLuint vertexBuffer;
    GLuint programID;
};

struct BoxGL
{
    GLuint vertexArray;
    GLuint vertexBuffer;
    GLuint programID;
};

struct RectCoordsNDC
{
    Vec3 pos;
    Vec2 size;
};

struct LineGLData
{
    int count;
    Vec3 pos[MAX_LINE_POINTS];
};

RectCoordsNDC ToRectCoordsNDC(Vec2Int pos, Vec2Int size,
    ScreenInfo screenInfo);
RectCoordsNDC ToRectCoordsNDC(Vec2Int pos, Vec2Int size, Vec2 anchor,
    ScreenInfo screenInfo);

GLuint LoadShaders(const ThreadContext* thread,
    const char* vertFilePath, const char* fragFilePath,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);

RectGL InitRectGL(const ThreadContext* thread,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);
TexturedRectGL InitTexturedRectGL(const ThreadContext* thread,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);
LineGL InitLineGL(const ThreadContext* thread,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);
PlaneGL InitPlaneGL(const ThreadContext* thread,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);
BoxGL InitBoxGL(const ThreadContext* thread,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);

// TODO this API is unrealistic and dumb. I need a batch draw function?
// TODO actually, is it? this is pretty useful for debug stuff...
void DrawRect(RectGL rectGL, ScreenInfo screenInfo,
    Vec2Int pos, Vec2 anchor, Vec2Int size, Vec4 color);
void DrawTexturedRect(TexturedRectGL texturedRectGL, ScreenInfo screenInfo,
    Vec2Int pos, Vec2 anchor, Vec2Int size, bool32 flipHorizontal, GLuint texture);
void DrawPlane(PlaneGL,
    Mat4 vp, Vec3 point, Vec3 normal, Vec4 color);
void DrawBox(BoxGL boxGL,
    Mat4 vp, Vec3 min, Vec3 max, Vec4 color);

// Batch functions
void DrawLine(LineGL lineGL, Mat4 transform, const LineGLData* lineData, Vec4 color);