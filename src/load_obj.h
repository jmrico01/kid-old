#pragma once

#include "main_platform.h"
#include "opengl.h"

struct MeshGL
{
    GLuint vertexArray;
    GLuint vertexBuffer;
    GLuint normalBuffer;
    GLuint programID;
    int vertexCount;
};

MeshGL LoadOBJOpenGL(const ThreadContext* thread,
    const char* filePath,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);