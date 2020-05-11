#include "asset.h"

const LevelData* GetLevelData(const GameAssets& assets, LevelId levelId)
{
    DEBUG_ASSERT(levelId < LevelId::COUNT);
    return &assets.levels[(int)levelId];
}
LevelData* GetLevelData(GameAssets* assets, LevelId levelId)
{
    DEBUG_ASSERT(levelId < LevelId::COUNT);
    return &assets->levels[(int)levelId];
}

const AnimatedSprite* GetAnimatedSprite(const GameAssets& assets, AnimatedSpriteId animatedSpriteId)
{
    DEBUG_ASSERT(animatedSpriteId < AnimatedSpriteId::COUNT);
    return &assets.animatedSprites[(int)animatedSpriteId];
}
AnimatedSprite* GetAnimatedSprite(GameAssets* assets, AnimatedSpriteId animatedSpriteId)
{
    DEBUG_ASSERT(animatedSpriteId < AnimatedSpriteId::COUNT);
    return &assets->animatedSprites[(int)animatedSpriteId];
}

const TextureGL* GetTexture(const GameAssets& assets, TextureId textureId)
{
    DEBUG_ASSERT(textureId < TextureId::COUNT);
    return &assets.textures[(int)textureId];
}
TextureGL* GetTexture(GameAssets* assets, TextureId textureId)
{
    DEBUG_ASSERT(textureId < TextureId::COUNT);
    return &assets->textures[(int)textureId];
}
