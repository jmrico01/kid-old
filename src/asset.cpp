#include "asset.h"

static const_string LEVEL_NAMES[] = {
    ToString("nothing"),
    ToString("overworld")
};

const_string GetLevelName(LevelId levelId)
{
    DEBUG_ASSERT(C_ARRAY_LENGTH(LEVEL_NAMES) == (int)LevelId::COUNT);
    DEBUG_ASSERT(levelId < LevelId::COUNT);

    return LEVEL_NAMES[(int)levelId];
}

const LevelData* GetLevelData(const GameAssets& assets, LevelId levelId)
{
    DEBUG_ASSERT(levelId < LevelId::COUNT);
    const LevelData* levelData = &assets.levels[(int)levelId];
	if (!levelData->loaded) {
        return nullptr;
	}
    return levelData;
}

LevelData* GetLevelData(GameAssets* assets, LevelId levelId)
{
    return (LevelData*)GetLevelData(*assets, levelId);
}

const LevelData* LoadLevel(GameAssets* assets, LevelId levelId, float32 pixelsPerUnit, MemoryBlock transient)
{
    LevelData* levelData = &assets->levels[(int)levelId];
    if (!LoadLevelData(levelData, GetLevelName(levelId), pixelsPerUnit, transient)) {
        return nullptr;
    }
    return levelData;
}

const AnimatedSprite* GetAnimatedSprite(const GameAssets& assets, AnimatedSpriteId animatedSpriteId)
{
    DEBUG_ASSERT(animatedSpriteId < AnimatedSpriteId::COUNT);
    return &assets.animatedSprites[(int)animatedSpriteId];
}

AnimatedSprite* GetAnimatedSprite(GameAssets* assets, AnimatedSpriteId animatedSpriteId)
{
    return (AnimatedSprite*)GetAnimatedSprite(*assets, animatedSpriteId);
}

const TextureGL* GetTexture(const GameAssets& assets, TextureId textureId)
{
    DEBUG_ASSERT(textureId < TextureId::COUNT);
    return &assets.textures[(int)textureId];
}

TextureGL* GetTexture(GameAssets* assets, TextureId textureId)
{
    return (TextureGL*)GetTexture(*assets, textureId);
}
