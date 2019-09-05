#include "main.h"

#include <km_common/km_debug.h>
#include <km_common/km_defines.h>
#include <km_common/km_input.h>
#include <km_common/km_log.h>
#include <km_common/km_math.h>
#include <km_common/km_string.h>
#undef internal
#include <random>
#define internal static

#include "main_platform.h"
#include "load_psd.h"
#include "opengl.h"
#include "opengl_funcs.h"
#include "opengl_base.h"
#include "post.h"
#include "render.h"

#define CAMERA_HEIGHT_UNITS ((REF_PIXEL_SCREEN_HEIGHT) / (REF_PIXELS_PER_UNIT))
#define CAMERA_WIDTH_UNITS ((CAMERA_HEIGHT_UNITS) * TARGET_ASPECT_RATIO)
#define CAMERA_OFFSET_Y (-CAMERA_HEIGHT_UNITS / 2.8f)
#define CAMERA_OFFSET_VEC3 (Vec3 { 0.0f, CAMERA_OFFSET_Y, 0.0f })

#define PLAYER_RADIUS 0.4f
#define PLAYER_HEIGHT 1.3f

#define LINE_COLLIDER_MARGIN 0.05f

global_var const char* LEVEL_NAMES[] = {
	"overworld",
	"house",
	"house-attic",
	"idk",
	"shack",
	"dream",
	"castle"
};

internal uint64 LevelNameToId(const Array<char>& name)
{
	uint64 numNames = C_ARRAY_LENGTH(LEVEL_NAMES);
	LOG_INFO("numNames: %llu\n", numNames);
	for (uint64 i = 0; i < numNames; i++) {
		if (StringCompare(name, LEVEL_NAMES[i])) {
			return i;
		}
	}

	return numNames;
}

internal uint64 LevelNameToId(const char* name)
{
	Array<char> nameArray;
	nameArray.data = (char*)name;
	nameArray.size = StringLength(name);
	return LevelNameToId(nameArray);
}

inline float32 RandFloat32()
{
	return (float32)rand() / RAND_MAX;
}
inline float32 RandFloat32(float32 min, float32 max)
{
	DEBUG_ASSERT(max > min);
	return RandFloat32() * (max - min) + min;
}

inline int RandInt(int min, int max)
{
	DEBUG_ASSERT(max > min);
	return rand() % (max - min) + min;
}

#if GAME_INTERNAL
internal float32 ScaleExponentToWorldScale(float32 exponent)
{
	const float32 SCALE_MIN = 0.1f;
	const float32 SCALE_MAX = 10.0f;
	const float32 c = (SCALE_MAX * SCALE_MIN - 1.0f) / (SCALE_MAX + SCALE_MIN - 2.0f);
	const float32 a = SCALE_MIN - c;
	const float32 b = log2f((SCALE_MAX - c) / a);
	return a * powf(2.0f, b * exponent) + c;
}
#endif

int GetPillarboxWidth(ScreenInfo screenInfo)
{
	int targetWidth = (int)((float32)screenInfo.size.y * TARGET_ASPECT_RATIO);
	return (screenInfo.size.x - targetWidth) / 2;
}

internal Mat4 CalculateProjectionMatrix(ScreenInfo screenInfo)
{
	float32 aspectRatio = (float32)screenInfo.size.x / screenInfo.size.y;
	Vec3 scaleToNDC = {
		2.0f / (CAMERA_HEIGHT_UNITS * aspectRatio),
		2.0f / CAMERA_HEIGHT_UNITS,
		1.0f
	};

	return Scale(scaleToNDC);
}

internal Mat4 CalculateInverseViewMatrix(Vec2 cameraPos, Quat cameraRot)
{
	return Translate(ToVec3(cameraPos, 0.0f))
		* UnitQuatToMat4(cameraRot)
		* Translate(-CAMERA_OFFSET_VEC3);
}

internal Mat4 CalculateViewMatrix(Vec2 cameraPos, Quat cameraRot)
{
	return Translate(CAMERA_OFFSET_VEC3)
		* UnitQuatToMat4(Inverse(cameraRot))
		* Translate(ToVec3(-cameraPos, 0.0f));
}

internal Vec2Int WorldToScreen(Vec2 worldPos, ScreenInfo screenInfo,
	Vec2 cameraPos, Quat cameraRot, float32 editorWorldScale)
{
	Vec2Int screenCenter = screenInfo.size / 2;
	Vec4 afterView = Scale(editorWorldScale)
		* CalculateViewMatrix(cameraPos, cameraRot)
		* ToVec4(worldPos, 0.0f, 1.0f);
	Vec2 result = Vec2 { afterView.x, afterView.y } / CAMERA_HEIGHT_UNITS * (float32)screenInfo.size.y;
	return ToVec2Int(result) + screenCenter;
}

internal Vec2 ScreenToWorld(Vec2Int screenPos, ScreenInfo screenInfo,
	Vec2 cameraPos, Quat cameraRot, float32 editorWorldScale)
{
	Vec2Int screenCenter = screenInfo.size / 2;
	Vec2 beforeView = ToVec2(screenPos - screenCenter) / (float32)screenInfo.size.y
		* CAMERA_HEIGHT_UNITS;
	Vec4 result = CalculateInverseViewMatrix(cameraPos, cameraRot)
		* Scale(1.0f / editorWorldScale)
		* ToVec4(beforeView, 0.0f, 1.0f);
	return Vec2 { result.x, result.y };
}

internal bool32 SaveFloorVertices(const ThreadContext* thread,
	const FloorCollider* floor, const char* filePath,
	MemoryBlock* transient,
	DEBUGPlatformWriteFileFunc DEBUGPlatformWriteFile)
{
	uint64 stringSize = 0;
	uint64 stringCapacity = transient->size;
	char* string = (char*)transient->memory;
	for (uint64 i = 0; i < floor->line.array.size; i++) {
		uint64 n = snprintf(string + stringSize, stringCapacity - stringSize,
			"%.2f, %.2f\r\n",
			floor->line[i].x, floor->line[i].y);
		stringSize += n;
		DEBUG_ASSERT(stringSize < stringCapacity);
	}

	if (!DEBUGPlatformWriteFile(thread, filePath, (uint32)stringSize, string, true)) {
		LOG_ERROR("Failed to write vertices to file\n");
		return false;
	}

	LOG_INFO("Floor vertices written to file\n");
	return true;
}

internal bool32 LoadLevelData(const ThreadContext* thread,
	const char* levelPath, LevelData* levelData, MemoryBlock* transient,
	DEBUGPlatformReadFileFunc DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc DEBUGPlatformFreeFileMemory)
{
	DEBUG_ASSERT(!levelData->loaded);

	levelData->sprites.Init();
	levelData->sprites.array.size = 0;

	levelData->levelTransitions.Init();
	levelData->levelTransitions.array.size = 0;

	levelData->lineColliders.Init();
	levelData->lineColliders.array.size = 0;

	levelData->lockedCamera = false;
	levelData->bounded = false;

	char filePath[PATH_MAX_LENGTH];

	StringCat(levelPath, "/level.psd", filePath, PATH_MAX_LENGTH);
	if (!LoadPSD(thread, filePath,
	GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
	transient, &levelData->psdData,
	DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory)) {
		LOG_ERROR("Failed to load level PSD file %s\n", filePath);
		return false;
	}

	for (uint64 i = 0; i < levelData->psdData.layers.array.size; i++) {
		LayerInfo& layer = levelData->psdData.layers[i];
		if (!layer.visible) {
			continue;
		}
		levelData->sprites.array.size++;
		TextureWithPosition& sprite = levelData->sprites[levelData->sprites.array.size - 1];
		sprite.texture = layer.textureGL;
		sprite.type = SPRITE_BACKGROUND;
		if (StringContains(layer.name.array, "obj_")) {
			sprite.type = SPRITE_OBJECT;
		}
		else if (StringContains(layer.name.array, "label_")) {
			sprite.type = SPRITE_LABEL;
		}
		Vec2Int offset = Vec2Int {
			layer.left,
			levelData->psdData.size.y - layer.bottom
		};
		sprite.pos = ToVec2(offset) / REF_PIXELS_PER_UNIT;
		sprite.anchor = Vec2::zero;
		sprite.restAngle = 0.0f;
		sprite.flipped = false;

	}

	StringCat(levelPath, "/level.kmkv", filePath, PATH_MAX_LENGTH);
	DEBUGReadFileResult levelFile = DEBUGPlatformReadFile(thread, filePath);
	if (!levelFile.data) {
		LOG_ERROR("Failed to load level data file %s\n", filePath);
		return false;
	}

	Array<char> fileString;
	fileString.size = levelFile.size;
	fileString.data = (char*)levelFile.data;
	while (true) {
		FixedArray<char, KEYWORD_MAX_LENGTH> keyword;
		keyword.Init();
		FixedArray<char, VALUE_MAX_LENGTH> value;
		value.Init();
		int read = ReadNextKeywordValue(fileString, &keyword, &value);
		if (read < 0) {
			LOG_ERROR("Sprite metadata file keyword/value error (%s)\n", filePath);
			return false;
		}
		else if (read == 0) {
			break;
		}
		fileString.size -= read;
		fileString.data += read;

		if (StringCompare(keyword.array, "bounds")) {
			Vec2 bounds;
			int parsedElements;
			if (!StringToElementArray(value.array, ' ', true,
			StringToFloat32, 2, bounds.e, &parsedElements)) {
				LOG_ERROR("Failed to parse level bounds %.*s (%s)\n",
					value.array.size, value.array.data, filePath);
				return false;
			}
			if (parsedElements != 2) {
				LOG_ERROR("Not enough coordinates in level bounds %.*s (%s)\n",
					value.array.size, value.array.data, filePath);
				return false;
			}

			levelData->bounded = true;
			levelData->bounds = bounds;
		}
		else if (StringCompare(keyword.array, "lockcamera")) {
			Vec2 coords;
			int parsedElements;
			if (!StringToElementArray(value.array, ' ', true,
			StringToFloat32, 2, coords.e, &parsedElements)) {
				LOG_ERROR("Failed to parse level lock camera coords %.*s (%s)\n",
					value.array.size, value.array.data, filePath);
				return false;
			}
			if (parsedElements != 2) {
				LOG_ERROR("Not enough coordinates in level lock camera coords %.*s (%s)\n",
					value.array.size, value.array.data, filePath);
				return false;
			}

			levelData->lockedCamera = true;
			levelData->cameraCoords = coords;
		}
		else if (StringCompare(keyword.array, "transition")) {
			DEBUG_ASSERT(levelData->levelTransitions.array.size < LEVEL_TRANSITIONS_MAX);
			LevelTransition* transition = &levelData->levelTransitions[
				levelData->levelTransitions.array.size++];

			Array<char> transitionString = value.array;
			while (true) {
				FixedArray<char, KEYWORD_MAX_LENGTH> keywordTransition;
				keywordTransition.Init();
				FixedArray<char, VALUE_MAX_LENGTH> valueTransition;
				valueTransition.Init();
				int readTransition = ReadNextKeywordValue(transitionString,
					&keywordTransition, &valueTransition);
				if (readTransition < 0) {
					LOG_ERROR("Sprite metadata file keyword/value error (%s)\n", filePath);
					return false;
				}
				else if (readTransition == 0) {
					break;
				}
				transitionString.size -= readTransition;
				transitionString.data += readTransition;

				if (StringCompare(keywordTransition.array, "coords")) {
					Vec2 coords;
					int parsedElements;
					if (!StringToElementArray(valueTransition.array, ' ', true,
					StringToFloat32, 2, coords.e, &parsedElements)) {
						LOG_ERROR("Failed to parse transition coords %.*s (%s)\n",
							valueTransition.array.size, valueTransition.array.data, filePath);
						return false;
					}
					if (parsedElements != 2) {
						LOG_ERROR("Not enough coordinates in transition coords %.*s (%s)\n",
							valueTransition.array.size, valueTransition.array.data, filePath);
						return false;
					}

					transition->coords = coords;
				}
				else if (StringCompare(keywordTransition.array, "range")) {
					Vec2 range;
					int parsedElements;
					if (!StringToElementArray(valueTransition.array, ' ', true,
					StringToFloat32, 2, range.e, &parsedElements)) {
						LOG_ERROR("Failed to parse transition range %.*s (%s)\n",
							valueTransition.array.size, valueTransition.array.data, filePath);
						return false;
					}
					if (parsedElements != 2) {
						LOG_ERROR("Not enough coordinates in transition range %.*s (%s)\n",
							valueTransition.array.size, valueTransition.array.data, filePath);
						return false;
					}

					transition->range = range;
				}
				else if (StringCompare(keywordTransition.array, "tolevel")) {
					uint64 toLevel = LevelNameToId(valueTransition.array);
					if (toLevel == C_ARRAY_LENGTH(LEVEL_NAMES)) {
						LOG_ERROR("Unrecognized level name on transition tolevel: %.*s (%s)\n",
							valueTransition.array.size, valueTransition.array.data, filePath);
						return false;
					}

					transition->toLevel = toLevel;
				}
				else if (StringCompare(keywordTransition.array, "tocoords")) {
					Vec2 toCoords;
					int parsedElements;
					if (!StringToElementArray(valueTransition.array, ' ', true,
					StringToFloat32, 2, toCoords.e, &parsedElements)) {
						LOG_ERROR("Failed to parse transition to-coords %.*s (%s)\n",
							valueTransition.array.size, valueTransition.array.data, filePath);
						return false;
					}
					if (parsedElements != 2) {
						LOG_ERROR("Not enough coordinates in transition to-coords %.*s (%s)\n",
							valueTransition.array.size, valueTransition.array.data, filePath);
						return false;
					}

					transition->toCoords = toCoords;
				}
				else {
					LOG_ERROR("Level file unsupported transition keyword %.*s (%s)\n",
						keywordTransition.array.size, &keywordTransition[0], filePath);
					return false;
				}
			}
		}
		else if (StringCompare(keyword.array, "line")) {
			DEBUG_ASSERT(levelData->lineColliders.array.size < LINE_COLLIDERS_MAX);

			LineCollider* lineCollider = &levelData->lineColliders[
				levelData->lineColliders.array.size++];
			lineCollider->line.array.size = 0;
			lineCollider->line.Init();

			Array<char> element = value.array;
			while (true) {
				Array<char> next;
				ReadElementInSplitString(&element, &next, '\n');

				Array<char> trimmed;
				TrimWhitespace(element, &trimmed);
				if (trimmed.size == 0) {
					break;
				}

				Vec2 pos;
				int parsedElements;
				if (!StringToElementArray(trimmed, ',', true,
				StringToFloat32, 2, pos.e, &parsedElements)) {
					LOG_ERROR("Failed to parse floor position %.*s (%s)\n",
						trimmed.size, trimmed.data, filePath);
					return false;
				}
				if (parsedElements != 2) {
					LOG_ERROR("Not enough coordinates in floor position %.*s (%s)\n",
						trimmed.size, trimmed.data, filePath);
					return false;
				}

				lineCollider->line.Append(pos);

				element = next;
			}
		}
		else if (StringCompare(keyword.array, "floor")) {
			levelData->floor.line.array.size = 0;
			levelData->floor.line.Init();

			Array<char> element = value.array;
			while (true) {
				Array<char> next;
				ReadElementInSplitString(&element, &next, '\n');

				Array<char> trimmed;
				TrimWhitespace(element, &trimmed);
				if (trimmed.size == 0) {
					break;
				}

				Vec2 pos;
				int parsedElements;
				if (!StringToElementArray(trimmed, ',', true,
				StringToFloat32, 2, pos.e, &parsedElements)) {
					LOG_ERROR("Failed to parse floor position %.*s (%s)\n",
						trimmed.size, trimmed.data, filePath);
					return false;
				}
				if (parsedElements != 2) {
					LOG_ERROR("Not enough coordinates in floor position %.*s (%s)\n",
						trimmed.size, trimmed.data, filePath);
					return false;
				}

				levelData->floor.line.Append(pos);

				element = next;
			}

			levelData->floor.PrecomputeSampleVerticesFromLine();
		}
		else if (StringCompare(keyword.array, "//")) {
			// comment, ignore
		}
		else {
			LOG_ERROR("Level file unsupported keyword %.*s (%s)\n",
				keyword.array.size, &keyword[0], filePath);
			return false;
		}
	}

	DEBUGPlatformFreeFileMemory(thread, &levelFile);

	for (uint64 i = 0; i < levelData->sprites.array.size; i++) {
		TextureWithPosition* sprite = &levelData->sprites[i];
		if (sprite->type == SPRITE_OBJECT) {
			Vec2 worldSize = ToVec2(sprite->texture.size) / REF_PIXELS_PER_UNIT;
			Vec2 coords = levelData->floor.GetCoordsFromWorldPos(sprite->pos + worldSize / 2.0f);

			sprite->coords = coords;
			sprite->anchor = Vec2::one / 2.0f;

			Vec2 floorPos, floorNormal;
			levelData->floor.GetInfoFromCoordX(sprite->coords.x, &floorPos, &floorNormal);
			sprite->restAngle = acosf(Dot(Vec2::unitY, floorNormal));
			if (floorNormal.x > 0.0f) {
				sprite->restAngle = -sprite->restAngle;
			}
		}
	}

	levelData->loaded = true;

	LOG_INFO("Loaded level data from file %s\n", filePath);

	return true;
}

internal bool32 SetActiveLevel(const ThreadContext* thread,
	GameState* gameState, const char* level, Vec2 startCoords, MemoryBlock* transient,
	DEBUGPlatformReadFileFunc DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc DEBUGPlatformFreeFileMemory,
	DEBUGPlatformWriteFileFunc DEBUGPlatformWriteFile)
{
	uint64 levelId = LevelNameToId(level);
	LevelData* levelData = &gameState->levels[levelId];
	if (!levelData->loaded) {
		char levelPath[PATH_MAX_LENGTH];
		snprintf(levelPath, PATH_MAX_LENGTH, "data/levels/%s", level);
		if (!LoadLevelData(thread, levelPath, levelData, transient,
		DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory)) {
			LOG_ERROR("Failed to load level data for level %llu\n", level);
			return false;
		}
	}

	gameState->activeLevel = levelId;

	gameState->playerCoords = startCoords;
	gameState->playerVel = Vec2::zero;
	if (startCoords.y > 0.0f) {
		gameState->playerState = PLAYER_STATE_FALLING;
	}
	else {
		gameState->playerState = PLAYER_STATE_GROUNDED;
		gameState->kid.activeAnimation.WriteString("Idle");
		gameState->kid.activeFrame = 0;
		gameState->kid.activeFrameRepeat = 0;
		gameState->kid.activeFrameTime = 0.0f;
	}
	if (levelData->lockedCamera) {
		gameState->cameraCoords = levelData->cameraCoords;
	}
	else {
		gameState->cameraCoords = startCoords;
	}

	char bakFilePath[PATH_MAX_LENGTH];
	snprintf(bakFilePath, PATH_MAX_LENGTH, "data/levels/%s/collision-bak.kml", level);
	if (!SaveFloorVertices(thread, &levelData->floor, bakFilePath, transient,
	DEBUGPlatformWriteFile)) {
		LOG_ERROR("Failed to save backup floor vertex data for level %llu\n", level);
		return false;
	}

	return true;
}

internal Vec2 WrappedWorldOffset(Vec2 fromCoords, Vec2 toCoords, float32 floorLength)
{
	Vec2 offset = toCoords - fromCoords;
	float32 distX = AbsFloat32(offset.x);

	float32 distXAlt = AbsFloat32(offset.x + floorLength);
	if (distXAlt < distX) {
		offset.x += floorLength;
		return offset;
	}

	distXAlt = AbsFloat32(offset.x - floorLength);
	if (distXAlt < distX) {
		offset.x -= floorLength;
		return offset;
	}

	return offset;
}

internal bool IsGrabbableObjectInRange(Vec2 playerCoords, GrabbedObjectInfo object,
	float32 floorLength)
{
	DEBUG_ASSERT(object.coordsPtr != nullptr);

	Vec2 toCoords = WrappedWorldOffset(playerCoords, *object.coordsPtr, floorLength);
	float32 distX = AbsFloat32(toCoords.x);
	float32 distY = AbsFloat32(toCoords.y);
	return object.rangeX.x <= distX && distX <= object.rangeX.y
		&& object.rangeY.x <= distY && distY <= object.rangeY.y;
}

internal void UpdateWorld(GameState* gameState, float32 deltaTime, const GameInput* input,
	MemoryBlock transient,
	DEBUGPlatformReadFileFunc DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc DEBUGPlatformFreeFileMemory,
	DEBUGPlatformWriteFileFunc DEBUGPlatformWriteFile)
{
	bool32 isInteractKeyPressed = IsKeyPressed(input, KM_KEY_E)
		|| (input->controllers[0].isConnected && input->controllers[0].b.isDown);
	bool32 wasInteractKeyPressed = WasKeyPressed(input, KM_KEY_E)
		|| (input->controllers[0].isConnected && input->controllers[0].b.isDown
		&& input->controllers[0].b.transitions == 1);

	if (wasInteractKeyPressed) {
		const LevelData& levelData = gameState->levels[gameState->activeLevel];
		for (uint64 i = 0; i < levelData.levelTransitions.array.size; i++) {
			Vec2 toPlayer = WrappedWorldOffset(
				gameState->playerCoords, levelData.levelTransitions[i].coords,
				levelData.floor.length); 
			if (AbsFloat32(toPlayer.x) <= levelData.levelTransitions[i].range.x
			&& AbsFloat32(toPlayer.y) <= levelData.levelTransitions[i].range.y) {
				uint64 newLevel = levelData.levelTransitions[i].toLevel;
				Vec2 startCoords = levelData.levelTransitions[i].toCoords;
				if (!SetActiveLevel(nullptr, gameState, LEVEL_NAMES[newLevel], startCoords,
				&transient, DEBUGPlatformReadFile,
				DEBUGPlatformFreeFileMemory, DEBUGPlatformWriteFile)) {
					DEBUG_PANIC("Failed to load level %llu\n", i);
				}
				break;
			}
		}
	}

	const LevelData& levelData = gameState->levels[gameState->activeLevel];

	HashKey ANIM_IDLE;
	ANIM_IDLE.WriteString("Idle");
	HashKey ANIM_WALK;
	ANIM_WALK.WriteString("Walk");
	HashKey ANIM_JUMP;
	ANIM_JUMP.WriteString("Jump");
	HashKey ANIM_FALL;
	ANIM_FALL.WriteString("Fall");
	HashKey ANIM_LAND;
	ANIM_LAND.WriteString("Land");

	const float32 PLAYER_WALK_SPEED = 3.6f;
	const float32 PLAYER_JUMP_HOLD_DURATION_MIN = 0.02f;
	const float32 PLAYER_JUMP_HOLD_DURATION_MAX = 0.3f;
	const float32 PLAYER_JUMP_MAG_MAX = 1.2f;
	const float32 PLAYER_JUMP_MAG_MIN = 0.4f;

	float32 speedMultiplier = 1.0f;

	gameState->playerVel.x = 0.0f;
	bool skipPlayerInput = false;
#if GAME_INTERNAL
	if (gameState->editor) {
		skipPlayerInput = true;
	}
#endif

	if (!skipPlayerInput) {
		float32 speed = PLAYER_WALK_SPEED * speedMultiplier;
		if (gameState->playerState == PLAYER_STATE_JUMPING) {
			speed /= Lerp(gameState->playerJumpMag, 1.0f, 0.3f);
		}

		if (IsKeyPressed(input, KM_KEY_A) || IsKeyPressed(input, KM_KEY_ARROW_LEFT)) {
			gameState->playerVel.x = -speed;
			gameState->facingRight = false;
		}
		if (IsKeyPressed(input, KM_KEY_D) || IsKeyPressed(input, KM_KEY_ARROW_RIGHT)) {
			gameState->playerVel.x = speed;
			gameState->facingRight = true;
		}
		if (input->controllers[0].isConnected) {
			float32 leftStickX = input->controllers[0].leftEnd.x;
			if (leftStickX < 0.0f) {
				gameState->playerVel.x = -speed;
				gameState->facingRight = false;
			}
			else if (leftStickX > 0.0f) {
				gameState->playerVel.x = speed;
				gameState->facingRight = true;
			}
		}

		bool fallPressed = IsKeyPressed(input, KM_KEY_S)
			|| IsKeyPressed(input, KM_KEY_ARROW_DOWN)
			|| (input->controllers[0].isConnected && input->controllers[0].leftEnd.y < 0.0f);
		if (gameState->playerState == PLAYER_STATE_GROUNDED && fallPressed
		&& gameState->currentPlatform != nullptr) {
			gameState->playerState = PLAYER_STATE_FALLING;
			gameState->currentPlatform = nullptr;
			gameState->playerCoords.y -= LINE_COLLIDER_MARGIN;
		}

		bool jumpPressed = IsKeyPressed(input, KM_KEY_SPACE)
			|| IsKeyPressed(input, KM_KEY_ARROW_UP)
			|| (input->controllers[0].isConnected && input->controllers[0].a.isDown);
		if (gameState->playerState == PLAYER_STATE_GROUNDED && jumpPressed
		&& !KeyCompare(gameState->kid.activeAnimation, ANIM_FALL) /* TODO fall anim + grounded state seems sketchy */) {
			gameState->playerState = PLAYER_STATE_JUMPING;
			gameState->currentPlatform = nullptr;
			gameState->playerJumpHolding = true;
			gameState->playerJumpHold = 0.0f;
			gameState->playerJumpMag = PLAYER_JUMP_MAG_MAX;
			gameState->audioState.soundKick.playing = true;
			gameState->audioState.soundKick.sampleIndex = 0;
		}

		if (gameState->playerJumpHolding) {
			gameState->playerJumpHold += deltaTime;
			if (gameState->playerState == PLAYER_STATE_JUMPING && !jumpPressed) {
				gameState->playerJumpHolding = false;
				float32 timeT = gameState->playerJumpHold - PLAYER_JUMP_HOLD_DURATION_MIN
					/ (PLAYER_JUMP_HOLD_DURATION_MAX - PLAYER_JUMP_HOLD_DURATION_MIN);
				timeT = ClampFloat32(timeT, 0.0f, 1.0f);
				timeT = sqrtf(timeT);
				gameState->playerJumpMag = Lerp(PLAYER_JUMP_MAG_MIN, PLAYER_JUMP_MAG_MAX, timeT);
			}
		}
	}

	FixedArray<HashKey, 4> nextAnimations;
	nextAnimations.Init();
	nextAnimations.array.size = 0;
	if (gameState->playerState == PLAYER_STATE_JUMPING) {
		if (!KeyCompare(gameState->kid.activeAnimation, ANIM_JUMP)) {
			nextAnimations.Append(ANIM_JUMP);
		}
		else {
			nextAnimations.Append(ANIM_FALL);
		}
	}
	else if (gameState->playerState == PLAYER_STATE_FALLING) {
		nextAnimations.Append(ANIM_FALL);
	}
	else if (gameState->playerState == PLAYER_STATE_GROUNDED) {
		if (gameState->playerVel.x != 0) {
			nextAnimations.Append(ANIM_WALK);
			nextAnimations.Append(ANIM_LAND);
		}
		else {
			nextAnimations.Append(ANIM_IDLE);
			nextAnimations.Append(ANIM_LAND);
		}
	}

	float32 animDeltaTime = deltaTime;
	if (gameState->playerState == PLAYER_STATE_GROUNDED) {
		animDeltaTime *= speedMultiplier;
	}
	if (gameState->playerState == PLAYER_STATE_JUMPING) {
		animDeltaTime /= Lerp(gameState->playerJumpMag, 1.0f, 0.5f);
	}
	Vec2 rootMotion = gameState->kid.Update(animDeltaTime, nextAnimations.array);
	if (gameState->playerState == PLAYER_STATE_JUMPING) {
		rootMotion *= gameState->playerJumpMag;
	}

	if (gameState->playerState == PLAYER_STATE_JUMPING
	&& KeyCompare(gameState->kid.activeAnimation, ANIM_FALL)) {
		gameState->playerState = PLAYER_STATE_FALLING;
	}

	const FloorCollider& floor = levelData.floor;

	Vec2 deltaCoords = gameState->playerVel * deltaTime + rootMotion;

	Vec2 playerPos = floor.GetWorldPosFromCoords(gameState->playerCoords);
	Vec2 playerPosNew = floor.GetWorldPosFromCoords(gameState->playerCoords + deltaCoords);
	Vec2 deltaPos = playerPosNew - playerPos;

	FixedArray<LineColliderIntersect, LINE_COLLIDERS_MAX> intersects;
	intersects.Init();
	GetLineColliderIntersections(levelData.lineColliders.array,
		playerPos, deltaPos, LINE_COLLIDER_MARGIN,
		&intersects.array);
	for (uint64 i = 0; i < intersects.array.size; i++) {
		if (gameState->currentPlatform == intersects[i].collider) {
			continue;
		}

		float32 newDeltaCoordX = deltaCoords.x;
		if (deltaPos.x != 0.0f) {
			float32 tX = (intersects[i].pos.x - playerPos.x) / deltaPos.x;
			newDeltaCoordX = deltaCoords.x * tX;
		}
		Vec2 newFloorPos, newFloorNormal;
		floor.GetInfoFromCoordX(gameState->playerCoords.x + newDeltaCoordX,
			&newFloorPos, &newFloorNormal);

		const float32 COS_WALK_ANGLE = cosf(PI_F / 4.0f);
		float32 dotCollisionFloorNormal = Dot(newFloorNormal, intersects[i].normal);
		if (AbsFloat32(dotCollisionFloorNormal) >= COS_WALK_ANGLE) {
			// Walkable floor
			if (gameState->playerState == PLAYER_STATE_FALLING) {
				gameState->currentPlatform = intersects[i].collider;
				deltaCoords.x = newDeltaCoordX;
			}
		}
		else {
			// Floor at steep angle (wall)
			deltaCoords = Vec2::zero;
		}
	}

	float32 floorHeightCoord = 0.0f;
	if (gameState->currentPlatform != nullptr) {
		float32 platformHeight;
		bool32 playerOverPlatform = GetLineColliderCoordYFromFloorCoordX(
			*gameState->currentPlatform, floor, gameState->playerCoords.x + deltaCoords.x,
			&platformHeight);
		if (playerOverPlatform) {
			floorHeightCoord = platformHeight;
		}
		else {
			gameState->currentPlatform = nullptr;
			gameState->playerState = PLAYER_STATE_FALLING;
		}
	}

	Vec2 playerCoordsNew = gameState->playerCoords + deltaCoords;
	if (playerCoordsNew.y < floorHeightCoord || gameState->currentPlatform != nullptr) {
		playerCoordsNew.y = floorHeightCoord;
		gameState->prevFloorCoordY = floorHeightCoord;
		gameState->playerState = PLAYER_STATE_GROUNDED;
	}

	{ // rock
		Vec2 size = ToVec2(gameState->rockTexture.size) / REF_PIXELS_PER_UNIT;
		float32 radius = size.y / 2.0f * 0.8f;
		gameState->rock.angle = -gameState->rock.coords.x / radius;
		gameState->rock.coords.y = radius;
	}

	const float32 GRAB_RANGE = 0.2f;

	if (isInteractKeyPressed && gameState->grabbedObject.coordsPtr == nullptr) {
		FixedArray<GrabbedObjectInfo, 10> candidates;
		candidates.Init();
		candidates.array.size = 0;
		Vec2 rockSize = ToVec2(gameState->rockTexture.size) / REF_PIXELS_PER_UNIT;
		float32 rockRadius = rockSize.y / 2.0f * 0.8f;
		candidates.Append({
			&gameState->rock.coords,
			Vec2 { rockRadius * 1.2f, rockRadius * 1.7f },
			Vec2 { 0.0f, rockRadius * 2.0f }
		});
		for (uint64 i = 0; i < candidates.array.size; i++) {
			if (IsGrabbableObjectInRange(gameState->playerCoords, candidates[i], floor.length)) {
				gameState->grabbedObject = candidates[i];
				break;
			}
		}
	}

	if (wasInteractKeyPressed) {
		if (gameState->liftedObject.spritePtr == nullptr) {
			const FixedArray<TextureWithPosition, LEVEL_SPRITES_MAX>& sprites =
				levelData.sprites;
			TextureWithPosition* newLiftedObject = nullptr;
			float32 minDist = 2.0f;
			for (uint64 i = 0; i < sprites.array.size; i++) {
				TextureWithPosition* sprite = &sprites.array.data[i];
				if (sprite->type != SPRITE_OBJECT) {
					continue;
				}

				Vec2 toCoords = sprite->coords - gameState->playerCoords;
				float32 coordDist = Mag(toCoords);
				if (coordDist < minDist) {
					newLiftedObject = sprite;
					minDist = coordDist;
				}
			}

			if (newLiftedObject != nullptr) {
				gameState->liftedObject.offset = Vec2 { -0.25f, 1.9f };
				gameState->liftedObject.placementOffsetX = 1.2f;
				gameState->liftedObject.coordYPrev = newLiftedObject->coords.y;
				gameState->liftedObject.spritePtr = newLiftedObject;
				//gameState->liftedObject.spritePtr->restAngle -= PI_F / 2.0f;
			}
		}
		else {
			TextureWithPosition* spritePtr = gameState->liftedObject.spritePtr;
			float32 placementOffsetX = gameState->liftedObject.placementOffsetX;
			if (spritePtr->flipped) {
				placementOffsetX = -placementOffsetX;
			}
			spritePtr->coords.x += placementOffsetX;
			spritePtr->coords.y = gameState->liftedObject.coordYPrev;
			//spritePtr->restAngle += PI_F / 2.0f;
			gameState->liftedObject.spritePtr = nullptr;
		}
	}

	TextureWithPosition* liftedSprite = gameState->liftedObject.spritePtr;
	if (liftedSprite != nullptr) {
		liftedSprite->coords = gameState->playerCoords;
		liftedSprite->flipped = !gameState->facingRight;
		Vec2 offset = gameState->liftedObject.offset;
		if (liftedSprite->flipped) {
			offset.x = -offset.x;
		}
		liftedSprite->coords += offset;
	}

	if (gameState->grabbedObject.coordsPtr != nullptr) {
		if (IsGrabbableObjectInRange(gameState->playerCoords, gameState->grabbedObject,
			floor.length)
		&& isInteractKeyPressed) {
			float32 deltaX = playerCoordsNew.x - gameState->playerCoords.x;
			Vec2* coordsPtr = gameState->grabbedObject.coordsPtr;
			coordsPtr->x += deltaX;
			if (coordsPtr->x < 0.0f) {
				coordsPtr->x += floor.length;
			}
			else if (coordsPtr->x > floor.length) {
				coordsPtr->x -= floor.length;
			}
		}
		else {
			gameState->grabbedObject.coordsPtr = nullptr;
		}
	}

	if (levelData.bounded) {
		if (gameState->playerCoords.x >= levelData.bounds.x
		&& playerCoordsNew.x < levelData.bounds.x) {
			playerCoordsNew.x = levelData.bounds.x;
		}
		if (gameState->playerCoords.x <= levelData.bounds.y
		&& playerCoordsNew.x > levelData.bounds.y) {
			playerCoordsNew.x = levelData.bounds.y;
		}
	}
	if (playerCoordsNew.x < 0.0f) {
		playerCoordsNew.x += floor.length;
	}
	else if (playerCoordsNew.x > floor.length) {
		playerCoordsNew.x -= floor.length;
	}
	gameState->playerCoords = playerCoordsNew;

	Array<HashKey> paperNextAnims;
	paperNextAnims.size = 0;
	gameState->paper.Update(deltaTime, paperNextAnims);

#if GAME_INTERNAL
	if (gameState->editor) {
		return;
	}
#endif

	if (!levelData.lockedCamera) {
		const float32 CAMERA_FOLLOW_ACCEL_DIST_MIN = 3.0f;
		const float32 CAMERA_FOLLOW_ACCEL_DIST_MAX = 10.0f;
		float32 cameraFollowLerpMag = 0.08f;
		Vec2 cameraCoordsTarget = gameState->playerCoords;
		if (cameraCoordsTarget.y > gameState->prevFloorCoordY) {
			cameraCoordsTarget.y = gameState->prevFloorCoordY;
		}

		// Wrap camera if necessary
		float32 dist = Mag(cameraCoordsTarget - gameState->cameraCoords);
		Vec2 cameraCoordsWrap = gameState->cameraCoords;
		cameraCoordsWrap.x += floor.length;
		float32 altDist = Mag(cameraCoordsTarget - cameraCoordsWrap);
		if (altDist < dist) {
			gameState->cameraCoords = cameraCoordsWrap;
			dist = altDist;
		}
		else {
			cameraCoordsWrap.x -= floor.length * 2.0f;
			altDist = Mag(cameraCoordsTarget - cameraCoordsWrap);
			if (altDist < dist) {
				gameState->cameraCoords = cameraCoordsWrap;
				dist = altDist;
			}
		}

		if (dist > CAMERA_FOLLOW_ACCEL_DIST_MIN) {
			float32 lerpMagAccelT = (dist - CAMERA_FOLLOW_ACCEL_DIST_MIN)
				/ (CAMERA_FOLLOW_ACCEL_DIST_MAX - CAMERA_FOLLOW_ACCEL_DIST_MIN);
			lerpMagAccelT = ClampFloat32(lerpMagAccelT, 0.0f, 1.0f);
			cameraFollowLerpMag += (1.0f - cameraFollowLerpMag) * lerpMagAccelT;
		}
		gameState->cameraCoords = Lerp(gameState->cameraCoords, cameraCoordsTarget,
			cameraFollowLerpMag);
	}

	Vec2 camFloorPos, camFloorNormal;
	floor.GetInfoFromCoordX(gameState->cameraCoords.x, &camFloorPos, &camFloorNormal);
	float32 angle = acosf(Dot(Vec2::unitY, camFloorNormal));
	if (camFloorNormal.x > 0.0f) {
		angle = -angle;
	}
	gameState->cameraPos = camFloorPos + camFloorNormal * gameState->cameraCoords.y;
	gameState->cameraRot = QuatFromAngleUnitAxis(angle, Vec3::unitZ);
}

internal void DrawWorld(const GameState* gameState, SpriteDataGL* spriteDataGL,
	Mat4 projection, ScreenInfo screenInfo)
{
	const LevelData& levelData = gameState->levels[gameState->activeLevel];
	const FloorCollider& floor = levelData.floor;

	spriteDataGL->numSprites = 0;

	//Vec2 pos = gameState->floor.GetWorldPosFromCoords(gameState->playerCoords);
	Vec2 playerFloorPos, playerFloorNormal;
	floor.GetInfoFromCoordX(gameState->playerCoords.x,
		&playerFloorPos, &playerFloorNormal);
	Vec2 playerPos = playerFloorPos + playerFloorNormal * gameState->playerCoords.y;
	float32 playerAngle = acosf(Dot(Vec2::unitY, playerFloorNormal));
	if (playerFloorNormal.x > 0.0f) {
		playerAngle = -playerAngle;
	}
	Quat playerRot = QuatFromAngleUnitAxis(playerAngle, Vec3::unitZ);
	Vec2 playerSize = ToVec2(gameState->kid.animatedSprite->textureSize) / REF_PIXELS_PER_UNIT;
	Vec2 anchorUnused = Vec2::zero;
	gameState->kid.Draw(spriteDataGL, playerPos, playerSize, anchorUnused, playerRot,
		1.0f, !gameState->facingRight);

	{ // level sprites
		for (uint64 i = 0; i < levelData.sprites.array.size; i++) {
			TextureWithPosition* sprite = &levelData.sprites.array.data[i];
			Vec2 pos;
			Quat baseRot;
			Quat rot;
			if (sprite->type == SPRITE_OBJECT) {
				baseRot = QuatFromAngleUnitAxis(-sprite->restAngle, Vec3::unitZ);

				Vec2 floorPos, floorNormal;
				floor.GetInfoFromCoordX(sprite->coords.x, &floorPos, &floorNormal);
				pos = floorPos + floorNormal * sprite->coords.y;
				if (sprite == gameState->liftedObject.spritePtr) {
					rot = playerRot;
				}
				else {
					float32 angle = acosf(Dot(Vec2::unitY, floorNormal));
					if (floorNormal.x > 0.0f) {
						angle = -angle;
					}
					rot = QuatFromAngleUnitAxis(angle, Vec3::unitZ);
				}
			}
			else {
				pos = sprite->pos;
				baseRot = Quat::one;
				rot = Quat::one;
			}
			if (sprite->type == SPRITE_LABEL) {
				Vec2 offset = WrappedWorldOffset(playerPos, pos, floor.length);
				if (AbsFloat32(offset.x) > 1.0f || offset.y < 0.0f || offset.y > 2.5f) {
					continue;
				}
			}
			Vec2 size = ToVec2(sprite->texture.size) / REF_PIXELS_PER_UNIT;
			Mat4 transform = CalculateTransform(pos, size, sprite->anchor,
				baseRot, rot, sprite->flipped);
			PushSprite(spriteDataGL, transform, 1.0f, sprite->texture.textureID);
		}
	}

	{ // rock
		Vec2 pos = floor.GetWorldPosFromCoords(gameState->rock.coords);
		Vec2 size = ToVec2(gameState->rockTexture.size) / REF_PIXELS_PER_UNIT;
		Quat rot = QuatFromAngleUnitAxis(gameState->rock.angle, Vec3::unitZ);
		Mat4 transform = CalculateTransform(pos, size, Vec2::one / 2.0f, rot, false);
		PushSprite(spriteDataGL, transform, 1.0f, gameState->rockTexture.textureID);
	}

	Mat4 view = CalculateViewMatrix(gameState->cameraPos, gameState->cameraRot);
	DrawSprites(gameState->renderState, *spriteDataGL, projection * view);

	spriteDataGL->numSprites = 0;

	const float32 aspectRatio = (float32)screenInfo.size.x / screenInfo.size.y;
	const Vec2 screenSizeWorld = { CAMERA_HEIGHT_UNITS * aspectRatio, CAMERA_HEIGHT_UNITS };
	const float32 marginX = (screenSizeWorld.x - CAMERA_WIDTH_UNITS) / 2.0f;

#if GAME_INTERNAL
	if (gameState->editor) {
		DrawSprites(gameState->renderState, *spriteDataGL, projection);
		return;
	}
#endif

	gameState->paper.Draw(spriteDataGL,
		Vec2::zero, screenSizeWorld, Vec2::one / 2.0f, Quat::one, 0.5f,
		false);

	{ // frame
		// TODO this sounds like I need another batch transform matrix
		float32 scale = (float32)REF_PIXEL_SCREEN_HEIGHT / gameState->frame.size.y;
		Vec2 size = scale * ToVec2(gameState->frame.size) / REF_PIXELS_PER_UNIT;
		Mat4 transform = CalculateTransform(Vec2::zero, size, Vec2::one / 2.0f, Quat::one, false);
		PushSprite(spriteDataGL, transform, 1.0f, gameState->frame.textureID);
	}

	DrawSprites(gameState->renderState, *spriteDataGL, projection);
}

extern "C" GAME_UPDATE_AND_RENDER_FUNC(GameUpdateAndRender)
{
	// NOTE: for clarity
	// A call to this function means the following has happened, in order:
	//  1. A frame has been displayed to the user
	//  2. The latest user input has been processed by the platform layer
	//
	// This function is expected to update the state of the game
	// and draw the frame that will be displayed, ideally, some constant
	// amount of time in the future.
	DEBUG_ASSERT(sizeof(GameState) <= memory->permanent.size);

	GameState *gameState = (GameState*)memory->permanent.memory;
	if (memory->shouldInitGlobalVariables) {
		// Initialize global function names
		logState_ = logState;
		flushLogs_ = platformFuncs->flushLogs;
		
		#define FUNC(returntype, name, ...) name = \
		platformFuncs->glFunctions.name;
			GL_FUNCTIONS_BASE
			GL_FUNCTIONS_ALL
		#undef FUNC

		memory->shouldInitGlobalVariables = false;
		LOG_INFO("Initialized global variables\n");
	}

	if (memory->isInitialized && WasKeyPressed(input, KM_KEY_R)) {
		for (int i = 0; i < LEVELS_MAX; i++) {
			if (gameState->levels[i].loaded) {
				UnloadPSDOpenGL(gameState->levels[i].psdData);
				gameState->levels[i].loaded = false;
			}
		}

		if (!SetActiveLevel(thread, gameState, LEVEL_NAMES[gameState->activeLevel],
		gameState->playerCoords, &memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory,
		platformFuncs->DEBUGPlatformWriteFile)) {
			DEBUG_PANIC("Failed to reload level %s\n", LEVEL_NAMES[gameState->activeLevel]);
		}
	}

	if (!memory->isInitialized) {
		// Very explicit depth testing setup (DEFAULT VALUES)
		// NDC is left-handed with this setup
		// (subtle left-handedness definition:
		//  front objects have z = -1, far objects have z = 1)
		// Nearer objects have less z than farther objects
		glDepthFunc(GL_LEQUAL);
		// Depth buffer clears to farthest z-value (1)
		glClearDepth(1.0);
		// Depth buffer transforms -1 to 1 range to 0 to 1 range
		glDepthRange(0.0, 1.0);

		glDisable(GL_CULL_FACE);
		//glFrontFace(GL_CCW);
		//glCullFace(GL_BACK);

		glLineWidth(1.0f);

		if (!InitAudioState(thread, &gameState->audioState, audio,
			&memory->transient,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to init audio state\n");
		}

		// Game data
		gameState->playerVel = Vec2::zero;
		gameState->facingRight = true;
		gameState->currentPlatform = nullptr;

		gameState->grabbedObject.coordsPtr = nullptr;
		gameState->liftedObject.spritePtr = nullptr;

		for (int i = 0; i < LEVELS_MAX; i++) {
			gameState->levels[i].loaded = false;
		}
		const char* FIRST_LEVEL = "dream";
		if (!SetActiveLevel(thread, gameState, FIRST_LEVEL, Vec2::zero, &memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory,
		platformFuncs->DEBUGPlatformWriteFile)) {
			DEBUG_PANIC("Failed to load level %s\n", FIRST_LEVEL);
		}

		gameState->grainTime = 0.0f;

#if GAME_INTERNAL
		gameState->debugView = false;
		gameState->editor = false;
		gameState->editorScaleExponent = 0.5f;

		gameState->floorVertexSelected = -1;
#endif

		// Rendering stuff
		InitRenderState(gameState->renderState, thread,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);

		gameState->rectGL = InitRectGL(thread,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->texturedRectGL = InitTexturedRectGL(thread,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->lineGL = InitLineGL(thread,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->textGL = InitTextGL(thread,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);

		FT_Error error = FT_Init_FreeType(&gameState->ftLibrary);
		if (error) {
			LOG_ERROR("FreeType init error: %d\n", error);
		}
		gameState->fontFaceSmall = LoadFontFace(thread, gameState->ftLibrary,
			"data/fonts/ocr-a/regular.ttf", 18,
			memory->transient,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->fontFaceMedium = LoadFontFace(thread, gameState->ftLibrary,
			"data/fonts/ocr-a/regular.ttf", 24,
			memory->transient,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);

		InitializeFramebuffers(NUM_FRAMEBUFFERS_COLOR_DEPTH, gameState->framebuffersColorDepth);
		InitializeFramebuffers(NUM_FRAMEBUFFERS_COLOR, gameState->framebuffersColor);
		InitializeFramebuffers(NUM_FRAMEBUFFERS_GRAY, gameState->framebuffersGray);

		glGenVertexArrays(1, &gameState->screenQuadVertexArray);
		glBindVertexArray(gameState->screenQuadVertexArray);

		glGenBuffers(1, &gameState->screenQuadVertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, gameState->screenQuadVertexBuffer);
		const GLfloat vertices[] = {
			-1.0f, -1.0f,
			1.0f, -1.0f,
			1.0f, 1.0f,
			1.0f, 1.0f,
			-1.0f, 1.0f,
			-1.0f, -1.0f
		};
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
			GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(
			0, // match shader layout location
			2, // size (vec2)
			GL_FLOAT, // type
			GL_FALSE, // normalized?
			0, // stride
			(void*)0 // array buffer offset
		);

		glGenBuffers(1, &gameState->screenQuadUVBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, gameState->screenQuadUVBuffer);
		const GLfloat uvs[] = {
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,
			1.0f, 1.0f,
			0.0f, 1.0f,
			0.0f, 0.0f
		};
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

		glBindVertexArray(0);

		gameState->screenShader = LoadShaders(thread,
			"shaders/screen.vert", "shaders/screen.frag",
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->bloomExtractShader = LoadShaders(thread,
			"shaders/screen.vert", "shaders/bloomExtract.frag",
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->bloomBlendShader = LoadShaders(thread,
			"shaders/screen.vert", "shaders/bloomBlend.frag",
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->blurShader = LoadShaders(thread,
			"shaders/screen.vert", "shaders/blur.frag",
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->grainShader = LoadShaders(thread,
			"shaders/screen.vert", "shaders/grain.frag",
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->lutShader = LoadShaders(thread,
			"shaders/screen.vert", "shaders/lut.frag",
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);

		gameState->rock.coords = {
			gameState->levels[gameState->activeLevel].floor.length - 10.0f,
			0.0f
		};
		gameState->rock.angle = 0.0f;
		if (!LoadPNGOpenGL(thread,
		"data/sprites/rock.png",
		GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
		gameState->rockTexture, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load rock\n");
		}

		if (!LoadAnimatedSprite(thread,
		"data/animations/kid/kid.kma",
		gameState->spriteKid, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load kid animation sprite\n");
		}

		gameState->kid.animatedSprite = &gameState->spriteKid;
		gameState->kid.activeAnimation = gameState->spriteKid.startAnimation;
		gameState->kid.activeFrame = 0;
		gameState->kid.activeFrameRepeat = 0;
		gameState->kid.activeFrameTime = 0.0f;

		if (!LoadAnimatedSprite(thread,
		"data/animations/paper/paper.kma",
		gameState->spritePaper, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load paper animation sprite\n");
		}

		gameState->paper.animatedSprite = &gameState->spritePaper;
		gameState->paper.activeAnimation = gameState->spritePaper.startAnimation;
		gameState->paper.activeFrame = 0;
		gameState->paper.activeFrameRepeat = 0;
		gameState->paper.activeFrameTime = 0.0f;

		if (!LoadPNGOpenGL(thread,
		"data/sprites/frame.png",
		GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
		gameState->frame, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load frame\n");
		}
		if (!LoadPNGOpenGL(thread,
		"data/sprites/pixel.png",
		GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
		gameState->pixelTexture, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load pixel texture\n");
		}

		if (!LoadPNGOpenGL(thread,
		"data/luts/lutbase.png",
		GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
		gameState->lutBase, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load base LUT\n");
		}

		if (!LoadPNGOpenGL(thread,
		"data/luts/kodak5205.png",
		GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
		gameState->lut1, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load base LUT\n");
		}

		memory->isInitialized = true;
	}
	if (screenInfo.changed) {
		// TODO not ideal to check for changed screen every frame
		// probably not that big of a deal, but might also be easy to avoid
		// later on with a more callback-y mechanism?

		UpdateFramebufferColorAttachments(NUM_FRAMEBUFFERS_COLOR_DEPTH,
			gameState->framebuffersColorDepth,
			GL_RGB, screenInfo.size.x, screenInfo.size.y, GL_RGB, GL_UNSIGNED_BYTE);
		UpdateFramebufferColorAttachments(NUM_FRAMEBUFFERS_COLOR, gameState->framebuffersColor,
			GL_RGB, screenInfo.size.x, screenInfo.size.y, GL_RGB, GL_UNSIGNED_BYTE);
		UpdateFramebufferColorAttachments(NUM_FRAMEBUFFERS_GRAY, gameState->framebuffersGray,
			GL_RED, screenInfo.size.x, screenInfo.size.y, GL_RED, GL_FLOAT);

		UpdateFramebufferDepthAttachments(NUM_FRAMEBUFFERS_COLOR_DEPTH,
			gameState->framebuffersColorDepth,
			GL_DEPTH24_STENCIL8, screenInfo.size.x, screenInfo.size.y,
			GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);

		LOG_INFO("Updated screen-size-dependent info\n");
	}

	// gameState->grainTime = fmod(gameState->grainTime + deltaTime, 5.0f);

	// Toggle global mute
	if (WasKeyPressed(input, KM_KEY_M)) {
		gameState->audioState.globalMute = !gameState->audioState.globalMute;
	}

	UpdateWorld(gameState, deltaTime, input, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory,
		platformFuncs->DEBUGPlatformWriteFile);

	// ---------------------------- Begin Rendering ---------------------------
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_ZERO);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glBindFramebuffer(GL_FRAMEBUFFER, gameState->framebuffersColorDepth[0].framebuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Mat4 projection = CalculateProjectionMatrix(screenInfo);
#if GAME_INTERNAL
	if (gameState->editor) {
		projection = projection * Scale(ScaleExponentToWorldScale(gameState->editorScaleExponent));
	}
#endif

	DEBUG_ASSERT(memory->transient.size >= sizeof(SpriteDataGL));
	SpriteDataGL* spriteDataGL = (SpriteDataGL*)memory->transient.memory;

	DrawWorld(gameState, spriteDataGL, projection, screenInfo);

	// ------------------------ Post processing passes ------------------------
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	// Apply filters
	// PostProcessGrain(gameState->framebuffersColorDepth[0], gameState->framebuffersColor[0],
	//     gameState->screenQuadVertexArray,
	//     gameState->grainShader, gameState->grainTime);

	// PostProcessLUT(gameState->framebuffersColorDepth[0],
	// 	gameState->framebuffersColor[0],
	// 	gameState->screenQuadVertexArray,
	// 	gameState->lutShader, gameState->lutBase);

	// Render to screen
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBindVertexArray(gameState->screenQuadVertexArray);
	glUseProgram(gameState->screenShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gameState->framebuffersColorDepth[0].color);
	GLint loc = glGetUniformLocation(gameState->screenShader, "framebufferTexture");
	glUniform1i(loc, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	// ---------------------------- End Rendering -----------------------------
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	OutputAudio(audio, gameState, input, memory->transient);

#if GAME_INTERNAL
	Mat4 view = CalculateViewMatrix(gameState->cameraPos, gameState->cameraRot);
	int pillarboxWidth = GetPillarboxWidth(screenInfo);

	bool32 wasDebugKeyPressed = WasKeyPressed(input, KM_KEY_G)
		|| (input->controllers[0].isConnected
		&& input->controllers[0].x.isDown && input->controllers[0].x.transitions == 1);
	if (wasDebugKeyPressed) {
		gameState->debugView = !gameState->debugView;
	}
	if (WasKeyPressed(input, KM_KEY_H)) {
		gameState->editor = !gameState->editor;
		gameState->editorScaleExponent = 0.5f;
	}

	const Vec4 DEBUG_FONT_COLOR = { 0.05f, 0.05f, 0.05f, 1.0f };
	const Vec2Int MARGIN = { 30, 45 };

	if (gameState->debugView) {
		const Mat4 viewProjection = projection * view;
		const LevelData& levelData = gameState->levels[gameState->activeLevel];
		const FloorCollider& floor = levelData.floor;

		FontFace& textFont = gameState->fontFaceSmall;
		FontFace& textFontSmall = gameState->fontFaceSmall;
		char textStr[128];
		Vec2Int textPosLeft = {
			pillarboxWidth + MARGIN.x,
			screenInfo.size.y - MARGIN.y,
		};
		Vec2Int textPosRight = {
			screenInfo.size.x - pillarboxWidth - MARGIN.x,
			screenInfo.size.y - MARGIN.y,
		};

		sprintf(textStr, "[G] to toggle debug view");
		DrawText(gameState->textGL, textFontSmall, screenInfo,
			textStr, textPosLeft, Vec2 { 0.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosLeft.y -= textFontSmall.height;
		sprintf(textStr, "[H] to toggle editor");
		DrawText(gameState->textGL, textFontSmall, screenInfo,
			textStr, textPosLeft, Vec2 { 0.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosLeft.y -= textFontSmall.height;
		textPosLeft.y -= textFontSmall.height;
		sprintf(textStr, "[K] to toggle debug audio view");
		DrawText(gameState->textGL, textFontSmall, screenInfo,
			textStr, textPosLeft, Vec2 { 0.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosLeft.y -= textFontSmall.height;
		sprintf(textStr, "[M] to toggle global audio mute");
		DrawText(gameState->textGL, textFontSmall, screenInfo,
			textStr, textPosLeft, Vec2 { 0.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosLeft.y -= textFontSmall.height;
		textPosLeft.y -= textFontSmall.height;
		sprintf(textStr, "[F11] to toggle fullscreen");
		DrawText(gameState->textGL, textFontSmall, screenInfo,
			textStr, textPosLeft, Vec2 { 0.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		float32 fps = 1.0f / deltaTime;
		sprintf(textStr, "%.2f --- FPS", fps);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		textPosRight.y -= textFont.height;
		sprintf(textStr, "%.2f|%.2f -- CRDS", gameState->playerCoords.x, gameState->playerCoords.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		sprintf(textStr, "%.2f|%.2f - CMCRD", gameState->cameraCoords.x, gameState->cameraCoords.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		Vec2 playerPosWorld = floor.GetWorldPosFromCoords(gameState->playerCoords);
		sprintf(textStr, "%.2f|%.2f --- POS", playerPosWorld.x, playerPosWorld.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		sprintf(textStr, "%d - STATE", gameState->playerState);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		HashKey& kidActiveAnim = gameState->kid.activeAnimation;
		sprintf(textStr, "%.*s -- ANIM",
			(int)kidActiveAnim.string.array.size, kidActiveAnim.string.array.data);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		textPosRight.y -= textFont.height;
		sprintf(textStr, "%.2f|%.2f --- CAM", gameState->cameraPos.x, gameState->cameraPos.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		textPosRight.y -= textFont.height;
		Vec2 mouseWorld = ScreenToWorld(input->mousePos, screenInfo,
			gameState->cameraPos, gameState->cameraRot,
			ScaleExponentToWorldScale(gameState->editorScaleExponent));
		sprintf(textStr, "%.2f|%.2f - MOUSE", mouseWorld.x, mouseWorld.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		Vec2 mouseCoords = floor.GetCoordsFromWorldPos(mouseWorld);
		textPosRight.y -= textFont.height;
		sprintf(textStr, "%.2f|%.2f - MSCRD", mouseCoords.x, mouseCoords.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		DEBUG_ASSERT(memory->transient.size >= sizeof(LineGLData));
		LineGLData* lineData = (LineGLData*)memory->transient.memory;

		{ // mouse
			Vec2 floorPos, floorNormal;
			floor.GetInfoFromCoordX(mouseCoords.x, &floorPos, &floorNormal);
			lineData->count = 2;
			lineData->pos[0] = ToVec3(floorPos, 0.0f);
			lineData->pos[1] = ToVec3(floorPos + floorNormal * mouseCoords.y, 0.0f);
			DrawLine(gameState->lineGL, viewProjection, lineData,
				Vec4 { 0.5f, 0.4f, 0.0f, 0.25f });
		}

		// sprites
		for (uint64 i = 0; i < levelData.sprites.array.size; i++) {
			if (levelData.sprites[i].type != SPRITE_OBJECT) {
				continue;
			}
			const float32 POINT_CROSS_OFFSET = 0.05f;
			Vec4 centerColor = Vec4 { 0.0f, 1.0f, 1.0f, 1.0f };
			Vec4 boundsColor = Vec4 { 1.0f, 0.0f, 1.0f, 1.0f };
			const TextureWithPosition& sprite = levelData.sprites[i];
			lineData->count = 2;
			Vec2 worldPos = floor.GetWorldPosFromCoords(sprite.pos);
			Vec3 pos = ToVec3(worldPos, 0.0f);
			lineData->pos[0] = pos - Vec3::unitX * POINT_CROSS_OFFSET;
			lineData->pos[1] = pos + Vec3::unitX * POINT_CROSS_OFFSET;
			DrawLine(gameState->lineGL, viewProjection, lineData, centerColor);
			lineData->pos[0] = pos - Vec3::unitY * POINT_CROSS_OFFSET;
			lineData->pos[1] = pos + Vec3::unitY * POINT_CROSS_OFFSET;
			DrawLine(gameState->lineGL, viewProjection, lineData, centerColor);
			Vec2 worldSize = ToVec2(sprite.texture.size) / REF_PIXELS_PER_UNIT;
			Vec2 anchorOffset = Vec2 {
				sprite.anchor.x * worldSize.x,
				sprite.anchor.y * worldSize.y
			};
			Vec3 origin = ToVec3(worldPos - anchorOffset, 0.0f);
			// TODO rotate sprite box
			lineData->count = 5;
			lineData->pos[0] = origin;
			lineData->pos[1] = origin;
			lineData->pos[1].x += worldSize.x;
			lineData->pos[2] = origin + ToVec3(worldSize, 0.0f);
			lineData->pos[3] = origin;
			lineData->pos[3].y += worldSize.y;
			lineData->pos[4] = lineData->pos[0];
			DrawLine(gameState->lineGL, viewProjection, lineData, boundsColor);
		}

		{ // floor
			Vec4 floorSmoothColorMax = { 0.4f, 0.4f, 0.5f, 1.0f };
			Vec4 floorSmoothColorMin = { 0.4f, 0.4f, 0.5f, 0.2f };
			const float32 FLOOR_SMOOTH_STEPS = 0.2f;
			const int FLOOR_HEIGHT_NUM_STEPS = 10;
			const float32 FLOOR_HEIGHT_STEP = 0.5f;
			const float32 FLOOR_NORMAL_LENGTH = FLOOR_HEIGHT_STEP;
			const float32 FLOOR_LENGTH = floor.length;
			for (int i = 0; i < FLOOR_HEIGHT_NUM_STEPS; i++) {
				float32 height = i * FLOOR_HEIGHT_STEP;
				lineData->count = 0;
				for (float32 floorX = 0.0f; floorX < FLOOR_LENGTH; floorX += FLOOR_SMOOTH_STEPS) {
					Vec2 fPos, fNormal;
					floor.GetInfoFromCoordX(floorX, &fPos, &fNormal);
					Vec2 pos = fPos + fNormal * height;
					lineData->pos[lineData->count++] = ToVec3(pos, 0.0f);
					lineData->pos[lineData->count++] = ToVec3(
						pos + fNormal * FLOOR_NORMAL_LENGTH, 0.0f);
					lineData->pos[lineData->count++] = ToVec3(pos, 0.0f);
				}
				DrawLine(gameState->lineGL, viewProjection, lineData,
					Lerp(floorSmoothColorMax, floorSmoothColorMin,
						(float32)i / (FLOOR_HEIGHT_NUM_STEPS - 1)));
			}
		}

		{ // line colliders
			Vec4 lineColliderColor = { 0.0f, 0.6f, 0.6f, 1.0f };
			const FixedArray<LineCollider, LINE_COLLIDERS_MAX>& lineColliders =
				levelData.lineColliders;
			for (uint64 i = 0; i < lineColliders.array.size; i++) {
				const LineCollider& lineCollider = lineColliders[i];
				lineData->count = (int)lineCollider.line.array.size;
				for (uint64 v = 0; v < lineCollider.line.array.size; v++) {
					lineData->pos[v] = ToVec3(lineCollider.line[v], 0.0f);
				}
				DrawLine(gameState->lineGL, viewProjection, lineData, lineColliderColor);
			}
		}

		{ // level transitions
			Vec4 levelTransitionColor = { 0.1f, 0.3f, 1.0f, 1.0f };
			const FixedArray<LevelTransition, LEVEL_TRANSITIONS_MAX>& transitions =
				levelData.levelTransitions;
			lineData->count = 5;
			for (uint64 i = 0; i < transitions.array.size; i++) {
				Vec2 coords = transitions[i].coords;
				Vec2 range = transitions[i].range;
				lineData->pos[0] = ToVec3(coords - range, 0.0f);
				lineData->pos[1] = lineData->pos[0];
				lineData->pos[1].x += range.x * 2.0f;
				lineData->pos[2] = ToVec3(coords + range, 0.0f);
				lineData->pos[3] = lineData->pos[0];
				lineData->pos[3].y += range.y * 2.0f;
				lineData->pos[4] = lineData->pos[0];
				for (int p = 0; p < 5; p++) {
					Vec2 worldPos = floor.GetWorldPosFromCoords(ToVec2(lineData->pos[p]));
					lineData->pos[p] = ToVec3(worldPos, 0.0f);
				}
				DrawLine(gameState->lineGL, viewProjection, lineData, levelTransitionColor);
			}
		}

		{ // bounds
			Vec4 boundsColor = { 1.0f, 0.2f, 0.3f, 1.0f };
			if (levelData.bounded) {
				lineData->count = 2;

				Vec2 boundLeftPos, boundLeftNormal;
				floor.GetInfoFromCoordX(levelData.bounds.x, &boundLeftPos, &boundLeftNormal);
				Vec2 boundLeft = floor.GetWorldPosFromCoords(Vec2 { levelData.bounds.x, 0.0f });
				lineData->pos[0] = ToVec3(boundLeftPos, 0.0f);
				lineData->pos[1] = ToVec3(boundLeftPos + boundLeftNormal * CAMERA_HEIGHT_UNITS, 0.0f);
				DrawLine(gameState->lineGL, viewProjection, lineData, boundsColor);

				Vec2 boundRightPos, boundRightNormal;
				floor.GetInfoFromCoordX(levelData.bounds.y, &boundRightPos, &boundRightNormal);
				lineData->pos[0] = ToVec3(boundRightPos, 0.0f);
				lineData->pos[1] = ToVec3(boundRightPos + boundRightNormal * CAMERA_HEIGHT_UNITS, 0.0f);
				DrawLine(gameState->lineGL, viewProjection, lineData, boundsColor);
			}
		}

		{ // player
			const float32 CROSS_RADIUS = 0.2f;
			Vec4 playerColor = { 1.0f, 0.2f, 0.2f, 1.0f };
			Vec3 playerPosWorld3 = ToVec3(playerPosWorld, 0.0f);
			lineData->count = 2;
			lineData->pos[0] = playerPosWorld3 - Vec3::unitX * CROSS_RADIUS;
			lineData->pos[1] = playerPosWorld3 + Vec3::unitX * CROSS_RADIUS;
			DrawLine(gameState->lineGL, viewProjection, lineData, playerColor);
			lineData->pos[0] = playerPosWorld3 - Vec3::unitY * CROSS_RADIUS;
			lineData->pos[1] = playerPosWorld3 + Vec3::unitY * CROSS_RADIUS;
			DrawLine(gameState->lineGL, viewProjection, lineData, playerColor);
		}
	}
	if (gameState->editor) {
		FloorCollider& floor = gameState->levels[gameState->activeLevel].floor;

		Vec2Int editorStrPos = {
			pillarboxWidth + MARGIN.x,
			MARGIN.y,
		};
		Vec4 editorFontColor = { 1.0f, 0.1f, 1.0f, 1.0f };
		DrawText(gameState->textGL, gameState->fontFaceMedium, screenInfo,
			"EDITOR", editorStrPos, Vec2 { 0.0f, 0.0f }, editorFontColor, memory->transient);

		bool32 newVertexPressed = false;
		{
			const Vec4 BOX_COLOR_BASE = Vec4 { 0.1f, 0.5f, 1.0f, 1.0f };
			const float32 IDLE_ALPHA = 0.5f;
			const float32 HOVER_ALPHA = 0.8f;
			const float32 SELECTED_ALPHA = 1.0f;
			const Vec2Int BOX_SIZE = Vec2Int { 20, 20 };
			const Vec2 BOX_ANCHOR = Vec2::one / 2.0f;
			const Vec2Int ANCHOR_OFFSET = Vec2Int {
				(int)(BOX_SIZE.x * BOX_ANCHOR.x),
				(int)(BOX_SIZE.y * BOX_ANCHOR.y)
			};
			Vec2Int mousePosPlusAnchor = input->mousePos + ANCHOR_OFFSET;
			for (uint64 i = 0; i < floor.line.array.size; i++) {
				Vec2Int boxPos = WorldToScreen(floor.line[i], screenInfo,
					gameState->cameraPos, gameState->cameraRot,
					ScaleExponentToWorldScale(gameState->editorScaleExponent));

				Vec4 boxColor = BOX_COLOR_BASE;
				boxColor.a = IDLE_ALPHA;
				if ((mousePosPlusAnchor.x >= boxPos.x
				&& mousePosPlusAnchor.x <= boxPos.x + BOX_SIZE.x) &&
				(mousePosPlusAnchor.y >= boxPos.y
				&& mousePosPlusAnchor.y <= boxPos.y + BOX_SIZE.y)) {
					if (input->mouseButtons[0].isDown
					&& input->mouseButtons[0].transitions == 1) {
						newVertexPressed = true;
						gameState->floorVertexSelected = (int)i;
					}
					else {
						boxColor.a = HOVER_ALPHA;
					}
				}
				if ((int)i == gameState->floorVertexSelected) {
					boxColor.a = SELECTED_ALPHA;
				}

				DrawRect(gameState->rectGL, screenInfo,
					boxPos, BOX_ANCHOR, BOX_SIZE, boxColor);
			}
		}

		if (input->mouseButtons[0].isDown && input->mouseButtons[0].transitions == 1
		&& !newVertexPressed) {
			gameState->floorVertexSelected = -1;
		}

		Vec2 mouseWorldPosStart = ScreenToWorld(input->mousePos, screenInfo,
			gameState->cameraPos, gameState->cameraRot,
			ScaleExponentToWorldScale(gameState->editorScaleExponent));
		Vec2 mouseWorldPosEnd = ScreenToWorld(input->mousePos + input->mouseDelta, screenInfo,
			gameState->cameraPos, gameState->cameraRot,
			ScaleExponentToWorldScale(gameState->editorScaleExponent));
		Vec2 mouseWorldDelta = mouseWorldPosEnd - mouseWorldPosStart;

		if (input->mouseButtons[0].isDown) {
			if (gameState->floorVertexSelected == -1) {
				gameState->cameraPos -= mouseWorldDelta;
			}
			else {
				floor.line[gameState->floorVertexSelected] += mouseWorldDelta;
				floor.PrecomputeSampleVerticesFromLine();
			}
		}

		if (gameState->floorVertexSelected != -1) {
			if (WasKeyPressed(input, KM_KEY_R)) {
				floor.line.Remove(gameState->floorVertexSelected);
				floor.PrecomputeSampleVerticesFromLine();
				gameState->floorVertexSelected = -1;
			}
		}

		if (input->mouseButtons[1].isDown && input->mouseButtons[1].transitions == 1) {
			if (gameState->floorVertexSelected == -1) {
				floor.line.Append(mouseWorldPosEnd);
				gameState->floorVertexSelected = (int)(floor.line.array.size - 1);
			}
			else {
				floor.line.AppendAfter(mouseWorldPosEnd,
					gameState->floorVertexSelected);
				gameState->floorVertexSelected += 1;
			}
			floor.PrecomputeSampleVerticesFromLine();
		}

		if (input->mouseButtons[2].isDown) {
			Vec2Int screenCenterToMouse = input->mousePos - screenInfo.size / 2;
			// TODO rotate camera
		}

		if (WasKeyPressed(input, KM_KEY_P)) {
			char saveFilePath[PATH_MAX_LENGTH];
			snprintf(saveFilePath, PATH_MAX_LENGTH,
				"data/levels/level%llu/collision.kml", gameState->activeLevel);
			if (!SaveFloorVertices(thread, &floor, saveFilePath,
			&memory->transient, platformFuncs->DEBUGPlatformWriteFile)) {
				LOG_ERROR("Level save failed!\n");
			}
		}

		float32 editorScaleExponentDelta = input->mouseWheelDelta * 0.0002f;
		gameState->editorScaleExponent = ClampFloat32(
			gameState->editorScaleExponent + editorScaleExponentDelta, 0.0f, 1.0f);
	}

	DrawDebugAudioInfo(audio, gameState, input, screenInfo, memory->transient, DEBUG_FONT_COLOR);
#endif

#if GAME_SLOW
	// Catch-all site for OpenGL errors
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		LOG_ERROR("OpenGL error: 0x%x\n", err);
	}
#endif
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>

#include <km_common/km_debug.cpp>
#include <km_common/km_input.cpp>
#include <km_common/km_lib.cpp>
#include <km_common/km_log.cpp>
#include <km_common/km_string.cpp>

#include "animation.cpp"
#include "audio.cpp"
#include "collision.cpp"
#include "framebuffer.cpp"
#include "load_png.cpp"
#include "load_psd.cpp"
#include "load_wav.cpp"
#include "opengl_base.cpp"
#include "particles.cpp"
#include "post.cpp"
#include "render.cpp"
#include "text.cpp"