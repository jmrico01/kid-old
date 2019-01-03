#pragma once

#undef internal // Required to build FreeType
#include <ft2build.h>
#include FT_FREETYPE_H
#define internal static

#include "opengl.h"
#include "opengl_funcs.h"
#include "km_math.h"
#include "main_platform.h"

#define MAX_GLYPHS 128

struct TextGL
{
	GLuint vertexArray;
	GLuint vertexBuffer;
	GLuint uvBuffer;
	GLuint posBuffer;
	GLuint sizeBuffer;
	GLuint uvInfoBuffer;
	GLuint programID;
};
struct GlyphInfo
{
	uint32 width;
	uint32 height;
	int offsetX;
	int offsetY;
	int advanceX;
	int advanceY;
	Vec2 uvOrigin;
	Vec2 uvSize;
};
struct FontFace
{
	GLuint atlasTexture;
	uint32 height;
	GlyphInfo glyphInfo[MAX_GLYPHS];
};

TextGL InitTextGL(const ThreadContext* thread,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);
FontFace LoadFontFace(const ThreadContext* thread,
	FT_Library library,
	const char* path, uint32 height,
	MemoryBlock transient,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);

int GetTextWidth(const FontFace& face, const char* text);
void DrawText(TextGL textGL, const FontFace& face, ScreenInfo screenInfo,
	const char* text,
	Vec2Int pos, Vec4 color,
	MemoryBlock transient);
void DrawText(TextGL textGL, const FontFace& face, ScreenInfo screenInfo,
	const char* text,
	Vec2Int pos, Vec2 anchor, Vec4 color,
	MemoryBlock transient);