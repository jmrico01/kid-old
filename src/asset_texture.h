#pragma once

#include <km_common/km_math.h>
#include <km_common/km_string.h>
#include <km_platform/main_platform.h>

#include "opengl.h"

struct TextureGL
{
	Vec2Int size;
	GLuint textureID;
};

bool LoadTexture(const uint8* data, GLint width, GLint height, GLint format,
                 GLint magFilter, GLint minFilter, GLint wrapS, GLint wrapT, TextureGL* outTextureGL);
void UnloadTexture(const TextureGL& textureGL);

template <typename Allocator>
bool LoadTextureFromPng(Allocator* allocator, const_string& filePath,
                        GLint magFilter, GLint minFilter, GLint wrapS, GLint wrapT, TextureGL* outTextureGL);
