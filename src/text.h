#pragma once

#undef internal // Required to build FreeType
#include <ft2build.h>
#include FT_FREETYPE_H
#define internal static
#include <km_common/km_math.h>
#include <km_platform/main_platform.h>

#include "opengl.h"
#include "opengl_funcs.h"

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

template <typename Allocator>
TextGL InitTextGL(const ThreadContext* thread, Allocator* allocator);
template <typename Allocator>
FontFace LoadFontFace(const ThreadContext* thread, Allocator* allocator,
	FT_Library library, const char* path, uint32 height);

int GetTextWidth(const FontFace& face, const char* text);
void DrawText(TextGL textGL, const FontFace& face, ScreenInfo screenInfo,
	const char* text,
	Vec2Int pos, Vec4 color,
	MemoryBlock transient);
void DrawText(TextGL textGL, const FontFace& face, ScreenInfo screenInfo,
	const char* text,
	Vec2Int pos, Vec2 anchor, Vec4 color,
	MemoryBlock transient);
