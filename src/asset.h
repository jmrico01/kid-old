#pragma once

#include "asset_animation.h"
#include "asset_level.h"
#include "asset_texture.h"

enum class LevelId
{
    NOTHING,
    OVERWORLD,

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

    OVERWORLD_AURORA,
    OVERWORLD_ICEBERG,

    COUNT
};

struct GameAssets
{
    TextureGL textures[TextureId::COUNT];
    AnimatedSprite animatedSprites[AnimatedSpriteId::COUNT];

    LevelData levels[LevelId::COUNT];

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

const_string GetLevelName(LevelId levelId);
const LevelData* GetLevelData(const GameAssets& assets, LevelId levelId);
LevelData* GetLevelData(GameAssets* assets, LevelId levelId);
const LevelData* LoadLevel(GameAssets* assets, LevelId levelId, float32 pixelsPerUnit, MemoryBlock transient);

const AnimatedSprite* GetAnimatedSprite(const GameAssets& assets, AnimatedSpriteId animatedSpriteId);
AnimatedSprite* GetAnimatedSprite(GameAssets* assets, AnimatedSpriteId animatedSpriteId);

const TextureGL* GetTexture(const GameAssets& assets, TextureId textureId);
TextureGL* GetTexture(GameAssets* assets, TextureId textureId);
