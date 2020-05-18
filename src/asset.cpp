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

bool LoadOrRefreshAssets(GameAssets* assets, float32 pixelsPerUnit, MemoryBlock transient)
{
    LinearAllocator allocator(transient.size, transient.memory);

    assets->screenShader = LoadShaders(&allocator, "shaders/screen.vert", "shaders/screen.frag");
    assets->bloomExtractShader = LoadShaders(&allocator, "shaders/screen.vert", "shaders/bloomExtract.frag");
    assets->bloomBlendShader = LoadShaders(&allocator, "shaders/screen.vert", "shaders/bloomBlend.frag");
    assets->blurShader = LoadShaders(&allocator, "shaders/screen.vert", "shaders/blur.frag");
    assets->grainShader = LoadShaders(&allocator, "shaders/screen.vert", "shaders/grain.frag");
    assets->lutShader = LoadShaders(&allocator, "shaders/screen.vert", "shaders/lut.frag");

    // Alphabet & fonts
    if (!LoadAlphabet(transient, &assets->alphabet)) {
        LOG_ERROR("Failed to load alphabet\n");
        return false;
    }

    FT_Error error = FT_Init_FreeType(&assets->ftLibrary);
    if (error) {
        LOG_ERROR("FreeType init error: %d\n", error);
        return false;
    }
    // TODO better error checking here
    assets->fontFaceSmall = LoadFontFace(&allocator, assets->ftLibrary, "data/fonts/ocr-a/regular.ttf", 18);
    assets->fontFaceMedium = LoadFontFace(&allocator, assets->ftLibrary, "data/fonts/ocr-a/regular.ttf", 24);

    // Animated sprites
    if (!LoadAnimatedSprite(GetAnimatedSprite(assets, AnimatedSpriteId::KID), ToString("kid"),
                            pixelsPerUnit, transient)) {
        LOG_ERROR("Failed to load kid animation sprite\n");
        return false;
    }
    // TODO priming file changed... hmm
    FileChangedSinceLastCall(ToString("data/kmkv/animations/kid.kmkv"));
    FileChangedSinceLastCall(ToString("data/psd/kid.psd"));

    if (!LoadAnimatedSprite(GetAnimatedSprite(assets, AnimatedSpriteId::PAPER), ToString("paper"),
                            pixelsPerUnit, transient)) {
        LOG_ERROR("Failed to load paper animation sprite\n");
        return false;
    }
    // TODO priming file changed... hmm
    FileChangedSinceLastCall(ToString("data/kmkv/animations/paper.kmkv"));
    FileChangedSinceLastCall(ToString("data/psd/paper.psd"));

    // Textures
    if (!LoadTextureFromPng(&allocator, ToString("data/sprites/rock.png"),
                            GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                            GetTexture(assets, TextureId::ROCK))) {
        LOG_ERROR("Failed to load rock\n");
        return false;
    }

    if (!LoadTextureFromPng(&allocator, ToString("data/sprites/frame-corner.png"),
                            GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                            GetTexture(assets, TextureId::FRAME_CORNER))) {
        LOG_ERROR("Failed to load frame corner texture\n");
        return false;
    }
    if (!LoadTextureFromPng(&allocator, ToString("data/sprites/pixel.png"),
                            GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                            GetTexture(assets, TextureId::PIXEL))) {
        LOG_ERROR("Failed to load pixel texture\n");
        return false;
    }

#if 0
    if (!LoadTextureFromPng(&allocator, ToString("data/luts/lutbase.png"),
                            GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, &assets->lutBase)) {
        LOG_ERROR("Failed to load base LUT\n");
        return false;
    }

    if (!LoadTextureFromPng(&allocator, ToString("data/luts/kodak5205.png"),
                            GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, &assets->lut1)) {
        LOG_ERROR("Failed to load LUT 1\n");
        return false;
    }
#endif

    return true;
}
