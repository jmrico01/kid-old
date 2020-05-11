#pragma once

#include "asset_animation.h"
#include "asset_level.h"
#include "asset_texture.h"

enum class TextureId
{
    ROCK,
    PIXEL,
    FRAME_CORNER,
    LUT_BASE,
    
    COUNT
};

struct GameAssets
{
    LevelData levels[LevelId::COUNT];
    
    TextureGL textures[TextureId::COUNT];
    AnimatedSprite animatedSprites[AnimatedSpriteId::COUNT];
    
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

const LevelData* GetLevelData(const GameAssets& assets, LevelId levelId);
LevelData* GetLevelData(GameAssets* assets, LevelId levelId);

const AnimatedSprite* GetAnimatedSprite(const GameAssets& assets, AnimatedSpriteId animatedSpriteId);
AnimatedSprite* GetAnimatedSprite(GameAssets* assets, AnimatedSpriteId animatedSpriteId);

const TextureGL* GetTexture(const GameAssets& assets, TextureId textureId);
TextureGL* GetTexture(GameAssets* assets, TextureId textureId);
