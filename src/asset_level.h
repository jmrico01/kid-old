#pragma once

#include <km_common/km_defines.h>
#include <km_common/km_string.h>
#include <km_platform/main_platform.h>

#include "collision.h"
#include "load_psd.h"

const uint64 LINE_COLLIDERS_MAX = 16;
const uint64 LEVEL_SPRITES_MAX = 64;
const uint64 LEVEL_TRANSITIONS_MAX = 4;

enum class SpriteType
{
	BACKGROUND,
	OBJECT,
	LABEL
};

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

	FixedArray<LevelTransition, LEVEL_TRANSITIONS_MAX> levelTransitions;

	bool lockedCamera;
	Vec2 cameraCoords;
	bool bounded;
	Vec2 bounds;

	bool loaded;
};

bool LoadLevelData(LevelData* levelData, const_string name, float32 pixelsPerUnit, MemoryBlock transient);
void UnloadLevelData(LevelData* levelData);
