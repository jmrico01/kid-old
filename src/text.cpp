#include "text.h"

#include <km_common/km_debug.h>
#include <km_common/km_string.h>

#include "main.h"

#define ATLAS_DIM_MIN 128
#define ATLAS_DIM_MAX 2048

#define GLYPH_BATCH_SIZE 1024

struct AtlasData
{
    uint8 data[ATLAS_DIM_MAX * ATLAS_DIM_MAX * sizeof(uint8)];
};

struct TextDataGL
{
    Vec3 posBottomLeft[GLYPH_BATCH_SIZE];
    Vec2 size[GLYPH_BATCH_SIZE];
    Vec4 uvInfo[GLYPH_BATCH_SIZE];
};

template <typename Allocator>
TextGL InitTextGL(const ThreadContext* thread, Allocator* allocator)
{
    TextGL textGL;

    glGenVertexArrays(1, &textGL.vertexArray);
    glBindVertexArray(textGL.vertexArray);

    const GLfloat vertices[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };
    glGenBuffers(1, &textGL.vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, textGL.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, // match shader layout location
        2, // size (vec2)
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        0, // stride
        (void*)0 // array buffer offset
    );
    glVertexAttribDivisor(0, 0);

    const GLfloat uvs[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };
    glGenBuffers(1, &textGL.uvBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, textGL.uvBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, // match shader layout location
        2, // size (vec2)
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        0, // stride
        (void*)0 // array buffer offset
    );
    glVertexAttribDivisor(1, 0);

    glGenBuffers(1, &textGL.posBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, textGL.posBuffer);
    glBufferData(GL_ARRAY_BUFFER, GLYPH_BATCH_SIZE * sizeof(Vec3), NULL,
        GL_STREAM_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, // match shader layout location
        3, // size (vec3)
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        0, // stride
        (void*)0 // array buffer offset
    );
    glVertexAttribDivisor(2, 1);

    glGenBuffers(1, &textGL.sizeBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, textGL.sizeBuffer);
    glBufferData(GL_ARRAY_BUFFER, GLYPH_BATCH_SIZE * sizeof(Vec2), NULL,
        GL_STREAM_DRAW);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(
        3, // match shader layout location
        2, // size (vec2)
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        0, // stride
        (void*)0 // array buffer offset
    );
    glVertexAttribDivisor(3, 1);

    glGenBuffers(1, &textGL.uvInfoBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, textGL.uvInfoBuffer);
    glBufferData(GL_ARRAY_BUFFER, GLYPH_BATCH_SIZE * sizeof(Vec4), NULL,
        GL_STREAM_DRAW);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(
        4, // match shader layout location
        4, // size (vec4)
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        0, // stride
        (void*)0 // array buffer offset
    );
    glVertexAttribDivisor(4, 1);

    glBindVertexArray(0);

    textGL.programID = LoadShaders(thread, allocator, "shaders/text.vert", "shaders/text.frag");

    return textGL;
}

template <typename Allocator>
FontFace LoadFontFace(const ThreadContext* thread, Allocator* allocator,
    FT_Library library, const char* path, uint32 height)
{
    const auto& allocatorState = allocator->SaveState();
    defer (allocator->LoadState(allocatorState));

    FontFace face = {};
    face.height = height;

    // Load font face using FreeType.
    FT_Face ftFace;
    PlatformReadFileResult fontFile = PlatformReadFile(thread, allocator, path);
    FT_Open_Args openArgs = {};
    openArgs.flags = FT_OPEN_MEMORY;
    openArgs.memory_base = (const FT_Byte*)fontFile.data;
    openArgs.memory_size = (FT_Long)fontFile.size;
    FT_Error error = FT_Open_Face(library, &openArgs, 0, &ftFace);
    if (error == FT_Err_Unknown_File_Format) {
        LOG_ERROR("Unsupported file format for %s\n", path);
        return face;
    }
    else if (error) {
        LOG_ERROR("Font file couldn't be read: %s\n", path);
        return face;
    }

    error = FT_Set_Pixel_Sizes(ftFace, 0, height);
    if (error) {
        LOG_ERROR("Failed to set font pixel size\n");
        return face;
    }

    // Fill in the non-UV parameters of GlyphInfo struct array.
    for (int ch = 0; ch < MAX_GLYPHS; ch++) {
        error = FT_Load_Char(ftFace, ch, FT_LOAD_RENDER);
        if (error) {
            face.glyphInfo[ch].width = 0;
            face.glyphInfo[ch].height = 0;
            face.glyphInfo[ch].offsetX = 0;
            face.glyphInfo[ch].offsetY = 0;
            face.glyphInfo[ch].advanceX = 0;
            face.glyphInfo[ch].advanceY = 0;
            continue;
        }
        FT_GlyphSlot glyph = ftFace->glyph;

        face.glyphInfo[ch].width = glyph->bitmap.width;
        face.glyphInfo[ch].height = glyph->bitmap.rows;
        face.glyphInfo[ch].offsetX = glyph->bitmap_left;
        face.glyphInfo[ch].offsetY =
            glyph->bitmap_top - (int)glyph->bitmap.rows;
        face.glyphInfo[ch].advanceX = glyph->advance.x;
        face.glyphInfo[ch].advanceY = glyph->advance.y;
    }

    const uint32 pad = 2;
    // Find the lowest dimension atlas that fits all characters to be loaded.
    // Atlas dimension is always square and power-of-two.
    uint32 atlasWidth = 0;
    uint32 atlasHeight = 0;
    for (uint32 dim = ATLAS_DIM_MIN; dim <= ATLAS_DIM_MAX; dim *= 2) {
        uint32 originI = pad;
        uint32 originJ = pad;
        bool fits = true;
        for (int ch = 0; ch < MAX_GLYPHS; ch++) {
            uint32 glyphWidth = face.glyphInfo[ch].width;
            if (originI + glyphWidth + pad >= dim) {
                originI = pad;
                originJ += height + pad;
            }
            originI += glyphWidth + pad;

            if (originJ + pad >= dim) {
                fits = false;
                break;
            }
        }
        if (fits) {
            atlasWidth = dim;
            atlasHeight = dim;
            break;
        }
    }

    if (atlasWidth == 0 || atlasHeight == 0) {
        DEBUG_PANIC("Atlas not big enough\n");
    }

    // Allocate and initialize atlas texture data.
    AtlasData* atlasData = (AtlasData*)allocator->Allocate(sizeof(AtlasData));
    if (!atlasData) {
        LOG_ERROR("Not enough memory for AtlasData, font %s\n", path);
        return face;
    }
    for (uint32 j = 0; j < atlasHeight; j++) {
        for (uint32 i = 0; i < atlasWidth; i++) {
            atlasData->data[j * atlasWidth + i] = 0;
        }
    }

    uint32 originI = pad;
    uint32 originJ = pad;
    for (int ch = 0; ch < MAX_GLYPHS; ch++) {
        error = FT_Load_Char(ftFace, ch, FT_LOAD_RENDER);
        if (error) {
            continue;
        }
        FT_GlyphSlot glyph = ftFace->glyph;

        uint32 glyphWidth = glyph->bitmap.width;
        uint32 glyphHeight = glyph->bitmap.rows;
        if (originI + glyphWidth + pad >= atlasWidth) {
            originI = pad;
            originJ += height + pad;
        }

        // Write glyph bitmap into atlas.
        for (uint32 j = 0; j < glyphHeight; j++) {
            for (uint32 i = 0; i < glyphWidth; i++) {
                int indAtlas = (originJ + j) * atlasWidth + originI + i;
                int indBuffer = (glyphHeight - 1 - j) * glyphWidth + i;
                atlasData->data[indAtlas] = glyph->bitmap.buffer[indBuffer];
            }
        }
        // Save UV coordinate data.
        face.glyphInfo[ch].uvOrigin = {
            (float32)originI / atlasWidth,
            (float32)originJ / atlasHeight
        };
        face.glyphInfo[ch].uvSize = {
            (float32)glyphWidth / atlasWidth,
            (float32)glyphHeight / atlasHeight
        };

        originI += glyphWidth + pad;
    }

    glGenTextures(1, &face.atlasTexture);
    glBindTexture(GL_TEXTURE_2D, face.atlasTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RED,
        atlasWidth,
        atlasHeight,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        atlasData->data
    );
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    return face;
}

int GetTextWidth(const FontFace& face, const char* text)
{
    float x = 0.0f;
    float y = 0.0f;
    for (const char* p = text; *p != 0; p++) {
        GlyphInfo glyphInfo = face.glyphInfo[*p];
        x += (float)glyphInfo.advanceX / 64.0f;
        y += (float)glyphInfo.advanceY / 64.0f;
    }

    return (int)x;
}

void DrawText(TextGL textGL, const FontFace& face, ScreenInfo screenInfo,
    const char* text,
    Vec2Int pos, Vec4 color,
    MemoryBlock transient)
{
    DEBUG_ASSERT(StringLength(text) <= GLYPH_BATCH_SIZE);
    DEBUG_ASSERT(transient.size >= sizeof(TextDataGL));

    TextDataGL* dataGL = (TextDataGL*)transient.memory;

    GLint loc;
    glUseProgram(textGL.programID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, face.atlasTexture);
    loc = glGetUniformLocation(textGL.programID, "textureSampler");
    glUniform1i(loc, 0);

    loc = glGetUniformLocation(textGL.programID, "color");
    glUniform4fv(loc, 1, &color.e[0]);

    glBindVertexArray(textGL.vertexArray);

    int x = 0, y = 0;
    int count = 0;
    for (const char* p = text; *p != '\0'; p++) {
        GlyphInfo glyphInfo = face.glyphInfo[*p];
        Vec2Int glyphPos = pos;
        glyphPos.x += x + glyphInfo.offsetX;
        glyphPos.y += y + glyphInfo.offsetY;
        Vec2Int glyphSize = { (int)glyphInfo.width, (int)glyphInfo.height };
        RectCoordsNDC ndc = ToRectCoordsNDC(glyphPos, glyphSize, screenInfo);
        dataGL->posBottomLeft[count] = ndc.pos;
        dataGL->size[count] = ndc.size;
        dataGL->uvInfo[count] = Vec4 {
            glyphInfo.uvOrigin.x, glyphInfo.uvOrigin.y,
            glyphInfo.uvSize.x, glyphInfo.uvSize.y
        };

        x += glyphInfo.advanceX / 64;
        y += glyphInfo.advanceY / 64;
        count++;
    }

    glBindBuffer(GL_ARRAY_BUFFER, textGL.posBuffer);
    glBufferData(GL_ARRAY_BUFFER, GLYPH_BATCH_SIZE * sizeof(Vec3), NULL,
        GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(Vec3),
        dataGL->posBottomLeft);
    glBindBuffer(GL_ARRAY_BUFFER, textGL.sizeBuffer);
    glBufferData(GL_ARRAY_BUFFER, GLYPH_BATCH_SIZE * sizeof(Vec2), NULL,
        GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(Vec2),
        dataGL->size);
    glBindBuffer(GL_ARRAY_BUFFER, textGL.uvInfoBuffer);
    glBufferData(GL_ARRAY_BUFFER, GLYPH_BATCH_SIZE * sizeof(Vec4), NULL,
        GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(Vec4),
        dataGL->uvInfo);

    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count);

    glBindVertexArray(0);
}

// Anchor is in range (0-1, 0-1).
void DrawText(TextGL textGL, const FontFace& face, ScreenInfo screenInfo,
    const char* text,
    Vec2Int pos, Vec2 anchor, Vec4 color,
    MemoryBlock transient)
{
    int textWidth = GetTextWidth(face, text);
    pos.x -= (int)(anchor.x * textWidth);
    pos.y -= (int)(anchor.y * face.height);

    DrawText(textGL, face, screenInfo, text, pos, color, transient);
}