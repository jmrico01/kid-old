#pragma once

#include <km_common/km_defines.h>
#include <km_platform/main_platform.h>

#include "collision.h"
#include "load_psd.h"

#define LEVEL_SPRITES_MAX 16

#define LINE_COLLIDERS_MAX 16

#define LEVELS_MAX 8
#define LEVEL_TRANSITIONS_MAX 4

enum SpriteType
{
	SPRITE_BACKGROUND,
    SPRITE_OBJECT,
    SPRITE_LABEL
};

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
	bool32 flipped;
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

    FixedArray<TextureWithPosition, LEVEL_SPRITES_MAX> sprites;

    FixedArray<LevelTransition, LEVEL_TRANSITIONS_MAX> levelTransitions;

    bool32 lockedCamera;
    Vec2 cameraCoords;
    bool32 bounded;
    Vec2 bounds;

    PsdData psdData;

    bool32 loaded;
};

void UnloadLevelData(LevelData* levelData);

bool32 LoadLevelData(const ThreadContext* thread,
	const char* levelPath, LevelData* levelData, MemoryBlock* transient,
	DEBUGPlatformReadFileFunc DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc DEBUGPlatformFreeFileMemory);
