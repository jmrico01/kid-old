#include "load_level.h"

bool32 LevelData::Load(const ThreadContext* thread, const char* levelPath, MemoryBlock* transient)
{
	DEBUG_ASSERT(!loaded);

	sprites.Init();
	sprites.array.size = 0;

	levelTransitions.Init();
	levelTransitions.array.size = 0;

	lineColliders.Init();
	lineColliders.array.size = 0;

	lockedCamera = false;
	bounded = false;

	char filePath[PATH_MAX_LENGTH];

	LinearAllocator allocator(transient->size, transient->memory);
	StringCat(levelPath, "/level.psd", filePath, PATH_MAX_LENGTH);
	if (!LoadPSD(thread, &allocator, filePath,
	GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, &psdData)) {
		LOG_ERROR("Failed to load level PSD file %s\n", filePath);
		return false;
	}

	for (uint64 i = 0; i < psdData.layers.array.size; i++) {
		LayerInfo& layer = psdData.layers[i];
		if (!layer.visible) {
			continue;
		}

		SpriteType spriteType = SPRITE_BACKGROUND;
		if (StringContains(layer.name.array, "obj_")) {
			spriteType = SPRITE_OBJECT;
		}
		else if (StringContains(layer.name.array, "label_")) {
			spriteType = SPRITE_LABEL;
		}
		else if (StringContains(layer.name.array, "x_")) {
			continue;
		}

		sprites.array.size++;
		TextureWithPosition& sprite = sprites[sprites.array.size - 1];
		sprite.texture = layer.textureGL;
		sprite.type = spriteType;
		Vec2Int offset = Vec2Int {
			layer.left,
			psdData.size.y - layer.bottom
		};
		sprite.pos = ToVec2(offset) / REF_PIXELS_PER_UNIT;
		sprite.anchor = Vec2::zero;
		sprite.restAngle = 0.0f;
		sprite.flipped = false;
	}

	StringCat(levelPath, "/level.kmkv", filePath, PATH_MAX_LENGTH);
	PlatformReadFileResult levelFile = PlatformReadFile(thread, &allocator, filePath);
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
			Vec2 parsedBounds;
			int parsedElements;
			if (!StringToElementArray(value.array, ' ', true,
			StringToFloat32, 2, parsedBounds.e, &parsedElements)) {
				LOG_ERROR("Failed to parse level bounds %.*s (%s)\n",
					value.array.size, value.array.data, filePath);
				return false;
			}
			if (parsedElements != 2) {
				LOG_ERROR("Not enough coordinates in level bounds %.*s (%s)\n",
					value.array.size, value.array.data, filePath);
				return false;
			}

			bounded = true;
			bounds = parsedBounds;
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

			lockedCamera = true;
			cameraCoords = coords;
		}
		else if (StringCompare(keyword.array, "transition")) {
			DEBUG_ASSERT(levelTransitions.array.size < LEVEL_TRANSITIONS_MAX);
			LevelTransition* transition = &levelTransitions[levelTransitions.array.size++];

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
			DEBUG_ASSERT(lineColliders.array.size < LINE_COLLIDERS_MAX);

			LineCollider* lineCollider = &lineColliders[lineColliders.array.size++];
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
			floor.line.array.size = 0;
			floor.line.Init();

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

				floor.line.Append(pos);

				element = next;
			}

			floor.PrecomputeSampleVerticesFromLine();
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

	for (uint64 i = 0; i < sprites.array.size; i++) {
		TextureWithPosition* sprite = &sprites[i];
		if (sprite->type == SPRITE_OBJECT) {
			Vec2 worldSize = ToVec2(sprite->texture.size) / REF_PIXELS_PER_UNIT;
			Vec2 coords = floor.GetCoordsFromWorldPos(sprite->pos + worldSize / 2.0f);

			sprite->coords = coords;
			sprite->anchor = Vec2::one / 2.0f;

			Vec2 floorPos, floorNormal;
			floor.GetInfoFromCoordX(sprite->coords.x, &floorPos, &floorNormal);
			sprite->restAngle = acosf(Dot(Vec2::unitY, floorNormal));
			if (floorNormal.x > 0.0f) {
				sprite->restAngle = -sprite->restAngle;
			}
		}
	}

	loaded = true;

	LOG_INFO("Loaded level data from file %s\n", filePath);

	return true;
}

void LevelData::Unload()
{
	UnloadPSDOpenGL(psdData);
	loaded = false;
}
