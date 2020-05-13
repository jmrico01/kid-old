#pragma once

#include <km_common/km_defines.h>
#include <km_platform/main_platform.h>

#include "collision.h"
#include "load_psd.h"

const uint64 LINE_COLLIDERS_MAX = 16;
const uint64 LEVEL_SPRITES_MAX = 32;
const uint64 LEVEL_TRANSITIONS_MAX = 4;

enum class LevelId
{
    NOTHING,
    OVERWORLD,
    
    COUNT
};

enum class SpriteType
{
	BACKGROUND,
	OBJECT,
	LABEL
};

#if 0
struct TextureWithPosition
{
	union {
		Vec2 pos;
		Vec2 coords;
	};
	Vec2 anchor;
	float32 restAngle;
	TextureGL texture;
	SpriteType type;
    bool flipped;
};
#endif

struct SpriteMetadata
{
	union {
		Vec2 pos;
		Vec2 coords;
	};
	Vec2 anchor;
	float32 restAngle;
	SpriteType type;
    bool flipped;
};

struct LevelTransition
{
	Vec2 coords;
	Vec2 range;
	uint64 toLevel;
	Vec2 toCoords;
};

struct LevelData
{
	FloorCollider floor;
	FixedArray<LineCollider, LINE_COLLIDERS_MAX> lineColliders;
    
    FixedArray<TextureGL, LEVEL_SPRITES_MAX> sprites;
    FixedArray<SpriteMetadata, LEVEL_SPRITES_MAX> spriteMetadata;
	// FixedArray<TextureWithPosition, LEVEL_SPRITES_MAX> sprites;
    
	FixedArray<LevelTransition, LEVEL_TRANSITIONS_MAX> levelTransitions;
    
	bool lockedCamera;
	Vec2 cameraCoords;
	bool bounded;
	Vec2 bounds;
    
	bool loaded;
};

const Array<char> GetLevelName(LevelId levelId);

bool LoadLevelData(LevelData* levelData, LevelId levelId, float32 pixelsPerUnit, MemoryBlock transient);
void UnloadLevelData(LevelData* levelData);
