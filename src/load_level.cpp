#include "load_level.h"

void UnloadLevelData(LevelData* levelData)
{
	UnloadPSDOpenGL(levelData->psdData);
	levelData->loaded = false;
}

bool32 LoadLevelData(const ThreadContext* thread,
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
