#pragma once

#include <km_common/km_lib.h>
#include <km_platform/main_platform.h>

#include "load_png.h"

const uint64 MAX_LETTERS = 512;

using LetterFlags = uint8;
namespace LetterFlag
{
    constexpr LetterFlags IGNORE = 1 << 0;
}

struct Letter
{
    // Computed at alphabet extraction time
    uint16 minX, minY, maxX, maxY;
    uint32 numPixels;
    int letterInd;
    
    // Manually specified by user
    LetterFlags flags;
    uint8 ascii;
    uint16 offsetX, offsetY, parentIndex;
    uint16 kernings[MAX_LETTERS];
};

struct Alphabet
{
    FixedArray<Letter, MAX_LETTERS> letters;
    FixedArray<TextureGL, MAX_LETTERS> letterTextures;
    TextureGL lettersTexture;
    TextureGL letterIdsTexture;
};

bool LoadAlphabet(MemoryBlock memory, Alphabet* outAlphabet);

template <typename Allocator>
Array<Letter> LoadSavedLetters(Allocator* allocator);
bool SaveLetters(Array<Letter> letters);
