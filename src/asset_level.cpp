#include "asset_level.h"

#include <km_common/km_kmkv.h>
#include <km_common/km_os.h>
#include <km_common/km_string.h>
#include <stb_sprintf.h>

internal int ToFlatIndex(Vec2Int index, Vec2Int size)
{
	return index.y * size.x + index.x;
}

internal bool PixelInBounds(Vec2Int pixel, Vec2Int size)
{
	return 0 <= pixel.x && pixel.x < size.x && 0 <= pixel.y && pixel.y < size.y;
}

template <typename Allocator, typename LoopAllocator>
internal bool GetLoop(const ImageData& imageAlpha, Allocator* allocator,
                      DynamicArray<Vec2Int, LoopAllocator>* outLoop)
{
	const int NUM_PIXELS = imageAlpha.size.x * imageAlpha.size.y;
	const Vec2Int PIXEL_NULL = Vec2Int { -1, -1 };

	FixedArray<Vec2Int, 4> neighborOffsets4;
	neighborOffsets4.size = 0;
	neighborOffsets4.Append(Vec2Int { -1,  0 });
	neighborOffsets4.Append(Vec2Int {  0,  1 });
	neighborOffsets4.Append(Vec2Int {  1,  0 });
	neighborOffsets4.Append(Vec2Int {  0, -1 });

	FixedArray<Vec2Int, 8> neighborOffsets8;
	neighborOffsets8.size = 0;
	neighborOffsets8.Append(Vec2Int { -1,  0 });
	neighborOffsets8.Append(Vec2Int { -1,  1 });
	neighborOffsets8.Append(Vec2Int {  0,  1 });
	neighborOffsets8.Append(Vec2Int {  1,  1 });
	neighborOffsets8.Append(Vec2Int {  1,  0 });
	neighborOffsets8.Append(Vec2Int {  1, -1 });
	neighborOffsets8.Append(Vec2Int {  0, -1 });
	neighborOffsets8.Append(Vec2Int { -1, -1 });

	bool* isEdge = (bool*)allocator->Allocate(NUM_PIXELS * sizeof(bool));
	if (isEdge == nullptr) {
		LOG_ERROR("Not enough memory for isEdge array\n");
		return false;
	}
	defer (allocator->Free(isEdge));
	uint8* neighborsVisited = (uint8*)allocator->Allocate(NUM_PIXELS * sizeof(uint8));
	if (neighborsVisited == nullptr) {
		LOG_ERROR("Not enough memory for neighborsVisited array\n");
		return false;
	}
	defer (allocator->Free(neighborsVisited));

	Vec2Int startPixel = PIXEL_NULL;
	for (int y = 0; y < imageAlpha.size.y; y++) {
		for (int x = 0; x < imageAlpha.size.x; x++) {
			Vec2Int pixel = { x, y };
			int pixelIndex = ToFlatIndex(pixel, imageAlpha.size);
			isEdge[pixelIndex] = false;
			neighborsVisited[pixelIndex] = 0;
			if (imageAlpha.data[pixelIndex] == 0) {
				continue;
			}
			bool hasZeroNeighbor = false;
			for (uint64 o = 0; o < neighborOffsets4.size; o++) {
				Vec2Int neighbor = pixel + neighborOffsets4[o];
				if (!PixelInBounds(neighbor, imageAlpha.size)) {
					hasZeroNeighbor = true;
					break;
				}
				int neighborIndex = ToFlatIndex(neighbor, imageAlpha.size);
				if (imageAlpha.data[neighborIndex] == 0) {
					hasZeroNeighbor = true;
					break;
				}
			}
			if (hasZeroNeighbor) {
				isEdge[pixelIndex] = true;
				startPixel = pixel;
			}
		}
	}

	if (startPixel == PIXEL_NULL) {
		LOG_ERROR("No edge pixels found\n");
		return false;
	}

	Vec2Int pixel = startPixel;
	outLoop->Append(startPixel);
	do {
		DEBUG_ASSERT(outLoop->size > 0);
		Vec2Int prev = outLoop->data[outLoop->size - 1];
		int index = ToFlatIndex(pixel, imageAlpha.size);
		Vec2Int next = PIXEL_NULL;
		while (true) {
			uint8 offsetIndex = neighborsVisited[index];
			if (offsetIndex >= neighborOffsets8.size) {
				break;
			}
			neighborsVisited[index]++;

			Vec2Int neighbor = pixel + neighborOffsets8[offsetIndex];
			if (!PixelInBounds(neighbor, imageAlpha.size) || neighbor == prev) {
				continue;
			}
			if (neighbor == startPixel) {
				// for some reason, we exit whenever this is the case
				// which is great, but idk why it's happening
			}
			int neighborIndex = ToFlatIndex(neighbor, imageAlpha.size);
			if (isEdge[neighborIndex]) {
				next = neighbor;
				break;
			}
		}
		if (next == PIXEL_NULL) {
			// Dead end, backtrack
			outLoop->RemoveLast();
			pixel = prev;
		}
		else {
			// Valid neighbor, advance
			outLoop->Append(pixel);
			pixel = next;
		}
	} while (pixel != startPixel);

	return true;
}

internal void DownsampleLoop(Array<Vec2Int>* loop, uint64 factor)
{
	// TODO smoothing
	uint64 i = 0;
	while (i * factor < loop->size) {
		loop->data[i] = loop->data[i * factor];
		i++;
	}
	loop->size = i;
}

internal bool IsLoopClockwise(const Array<Vec2Int>& loop)
{
	int sumAngleMetric = 0;
	Vec2Int prevVector = loop[1] - loop[0];
	for (uint64 i = 2; i < loop.size; i++) {
		Vec2Int vector = loop[i] - loop[i - 1];
		// determinant is proportinal to sine of angle
		int determinant = vector.x * prevVector.y - vector.y * prevVector.x;
		sumAngleMetric += determinant;
		prevVector = vector;
	}

	return sumAngleMetric >= 0;
}

internal void InvertLoop(Array<Vec2Int>* loop)
{
	for (uint64 low = 0, high = loop->size - 1; low < high; low++, high--) {
		Vec2Int temp = loop->data[low];
		loop->data[low] = loop->data[high];
		loop->data[high] = temp;
	}
}

bool LoadLevelData(LevelData* levelData, const_string name, float32 pixelsPerUnit, MemoryBlock transient)
{
	LinearAllocator allocator(transient.size, transient.memory);

	levelData->sprites.Clear();
	levelData->levelTransitions.Clear();
	levelData->lineColliders.Clear();
	levelData->floor.line.Clear();

	levelData->lockedCamera = false;
	levelData->bounded = false;

	FixedArray<char, PATH_MAX_LENGTH> filePath;
	filePath.Clear();
	filePath.Append(ToString("data/kmkv/levels/"));
	filePath.Append(name);
	filePath.Append(ToString(".kmkv"));
	Array<uint8> levelFile = LoadEntireFile(filePath.ToArray(), &allocator);
	if (!levelFile.data) {
		LOG_ERROR("Failed to load level data file %.*s\n", filePath.size, filePath.data);
		return false;
	}

    string groundLayerName = {};
    string fileString = {
        .size = levelFile.size,
        .data = (char*)levelFile.data
    };
    string keyword, value;
	while (true) {
		int read = ReadNextKeywordValue(fileString, &keyword, &value);
		if (read < 0) {
			LOG_ERROR("Sprite metadata file keyword/value error (%.*s)\n",
                      filePath.size, filePath.data);
			return false;
		}
		else if (read == 0) {
			break;
		}
		fileString.size -= read;
		fileString.data += read;

        if (StringEquals(keyword, ToString("ground"))) {
            groundLayerName = value;
        }
        else if (StringEquals(keyword, ToString("anim"))) {
            // TODO use these
        }
		else if (StringEquals(keyword, ToString("bounds"))) {
			Vec2 parsedBounds;
			int parsedElements;
			if (!StringToElementArray(value, ' ', true, StringToFloat32, 2, parsedBounds.e, &parsedElements)) {
				LOG_ERROR("Failed to parse level bounds %.*s (%.*s)\n",
                          value.size, value.data, filePath.size, filePath.data);
				return false;
			}
			if (parsedElements != 2) {
				LOG_ERROR("Not enough coordinates in level bounds %.*s (%.*s)\n",
                          value.size, value.data, filePath.size, filePath.data);
				return false;
			}

			levelData->bounded = true;
			levelData->bounds = parsedBounds;
		}
		else if (StringEquals(keyword, ToString("lockcamera"))) {
			Vec2 coords;
			int parsedElements;
			if (!StringToElementArray(value, ' ', true, StringToFloat32, 2, coords.e, &parsedElements)) {
				LOG_ERROR("Failed to parse level lock camera coords %.*s (%.*s)\n",
                          value.size, value.data, filePath.size, filePath.data);
				return false;
			}
			if (parsedElements != 2) {
				LOG_ERROR("Not enough coordinates in level lock camera coords %.*s (%.*s)\n",
                          value.size, value.data, filePath.size, filePath.data);
				return false;
			}

			levelData->lockedCamera = true;
			levelData->cameraCoords = coords;
		}
		else if (StringEquals(keyword, ToString("transition"))) {
			DEBUG_ASSERT(levelData->levelTransitions.size < LEVEL_TRANSITIONS_MAX);
            LevelTransition* transition = levelData->levelTransitions.Append();

            string keywordTransition, valueTransition;
			while (true) {
				int readTransition = ReadNextKeywordValue(value, &keywordTransition, &valueTransition);
				if (readTransition < 0) {
					LOG_ERROR("Sprite metadata file keyword/value error (%.*s)\n",
                              filePath.size, filePath.data);
					return false;
				}
				else if (readTransition == 0) {
					break;
				}
				value.size -= readTransition;
				value.data += readTransition;

				if (StringEquals(keywordTransition, ToString("coords"))) {
					Vec2 coords;
					int parsedElements;
					if (!StringToElementArray(valueTransition, ' ', true, StringToFloat32, 2, coords.e, &parsedElements)) {
						LOG_ERROR("Failed to parse transition coords %.*s (%.*s)\n",
                                  valueTransition.size, valueTransition.data,
                                  filePath.size, filePath.data);
						return false;
					}
					if (parsedElements != 2) {
						LOG_ERROR("Not enough coordinates in transition coords %.*s (%.*s)\n",
                                  valueTransition.size, valueTransition.data,
                                  filePath.size, filePath.data);
						return false;
					}

					transition->coords = coords;
				}
				else if (StringEquals(keywordTransition, ToString("range"))) {
					Vec2 range;
					int parsedElements;
					if (!StringToElementArray(valueTransition, ' ', true, StringToFloat32, 2, range.e, &parsedElements)) {
						LOG_ERROR("Failed to parse transition range %.*s (%.*s)\n",
                                  valueTransition.size, valueTransition.data,
                                  filePath.size, filePath.data);
						return false;
					}
					if (parsedElements != 2) {
						LOG_ERROR("Not enough coordinates in transition range %.*s (%.*s)\n",
                                  valueTransition.size, valueTransition.data,
                                  filePath.size, filePath.data);
						return false;
					}

					transition->range = range;
				}
				else if (StringEquals(keywordTransition, ToString("tolevel"))) {
                    // TODO fix this translation
					uint64 toLevel = 0; // LevelNameToId(valueTransition.ToArray());
					if (toLevel == C_ARRAY_LENGTH(LEVEL_NAMES)) {
						LOG_ERROR("Unrecognized level name on transition tolevel: %.*s (%.*s)\n",
                                  valueTransition.size, valueTransition.data,
                                  filePath.size, filePath.data);
						return false;
					}

					transition->toLevel = toLevel;
				}
				else if (StringEquals(keywordTransition, ToString("tocoords"))) {
					Vec2 toCoords;
					int parsedElements;
					if (!StringToElementArray(valueTransition, ' ', true,
                                              StringToFloat32, 2, toCoords.e, &parsedElements)) {
						LOG_ERROR("Failed to parse transition to-coords %.*s (%.*s)\n",
                                  valueTransition.size, valueTransition.data,
                                  filePath.size, filePath.data);
						return false;
					}
					if (parsedElements != 2) {
						LOG_ERROR("Not enough coordinates in transition to-coords %.*s (%.*s)\n",
                                  valueTransition.size, valueTransition.data,
                                  filePath.size, filePath.data);
						return false;
					}

					transition->toCoords = toCoords;
				}
				else {
					LOG_ERROR("Level file unsupported transition keyword %.*s (%.*s)\n",
                              keywordTransition.size, keywordTransition.data,
                              filePath.size, filePath.data);
					return false;
				}
			}
		}
		else if (StringEquals(keyword, ToString("line"))) {
			DEBUG_ASSERT(levelData->lineColliders.size < LINE_COLLIDERS_MAX);

			LineCollider* lineCollider = levelData->lineColliders.Append();
			lineCollider->line.size = 0;

			while (true) {
                string next = NextSplitElement(&value, '\n');

                string trimmed = TrimWhitespace(value);
				if (trimmed.size == 0) {
					break;
				}

				Vec2 pos;
				int parsedElements;
				if (!StringToElementArray(trimmed, ',', true,
                                          StringToFloat32, 2, pos.e, &parsedElements)) {
					LOG_ERROR("Failed to parse floor position %.*s (%.*s)\n",
                              trimmed.size, trimmed.data, filePath.size, filePath.data);
					return false;
				}
				if (parsedElements != 2) {
					LOG_ERROR("Not enough coordinates in floor position %.*s (%.*s)\n",
                              trimmed.size, trimmed.data, filePath.size, filePath.data);
					return false;
				}

				lineCollider->line.Append(pos);

                value = next;
			}
		}
		else if (StringEquals(keyword, ToString("//"))) {
			// comment, ignore
		}
		else {
			LOG_ERROR("Level file unsupported keyword %.*s (%.*s)\n",
                      keyword.size, keyword.data, filePath.size, filePath.data);
			return false;
		}
	}

    if (groundLayerName.size == 0) {
        LOG_ERROR("Level file missing ground layer name (%.*s\n", filePath.size, filePath.data);
        return false;
    }

	filePath.Clear();
	filePath.Append(ToString("data/psd/"));
	filePath.Append(name);
	filePath.Append(ToString(".psd"));
	PsdFile psdFile;
	if (!LoadPsd(&psdFile, filePath.ToArray(), &allocator)) {
		LOG_ERROR("Failed to open and parse level PSD file %.*s\n", filePath.size, filePath.data);
		return false;
	}

	for (uint64 i = 0; i < psdFile.layers.size; i++) {
		PsdLayerInfo& layer = psdFile.layers[i];

		const auto& allocatorState = allocator.SaveState();
		defer (allocator.LoadState(allocatorState));

		if (StringEquals(layer.name.ToArray(), groundLayerName)) {
			if (levelData->floor.line.size > 0) {
				LOG_ERROR("Found more than 1 ground_ layer: %.*s for %.*s\n",
                          layer.name.size, layer.name.data, filePath.size, filePath.data);
				return false;
			}
			ImageData imageAlpha;
			if (!psdFile.LoadLayerImageData(i, LayerChannelID::ALPHA, &allocator, &imageAlpha)) {
				LOG_ERROR("Failed to load ground layer %.*s image data for %.*s\n",
                          layer.name.size, layer.name.data, filePath.size, filePath.data);
				return false;
			}

			DynamicArray<Vec2Int, LinearAllocator> loop(&allocator);
			if (!GetLoop(imageAlpha, &allocator, &loop)) {
				LOG_ERROR("Failed to get loop from ground edges\n");
				return false;
			}

			DownsampleLoop(&loop.ToArray(), 10);
			if (!IsLoopClockwise(loop.ToArray())) {
				InvertLoop(&loop.ToArray());
			}

			Vec2Int origin = Vec2Int {
				layer.left,
				psdFile.size.y - layer.bottom
			};
			for (uint64 v = 0; v < loop.size; v++) {
				Vec2 pos = ToVec2(loop[v] + origin) / pixelsPerUnit;
				levelData->floor.line.Append(pos);
			}
			levelData->floor.PrecomputeSampleVerticesFromLine();
		}

		if (!layer.visible) {
			continue;
		}

		SpriteType spriteType = SpriteType::BACKGROUND;
		if (StringContains(layer.name.ToArray(), ToString("obj_"))) {
			spriteType = SpriteType::OBJECT;
		}
		else if (StringContains(layer.name.ToArray(), ToString("label_"))) {
			spriteType = SpriteType::LABEL;
		}
		else if (StringContains(layer.name.ToArray(), ToString("x_"))) {
			continue;
		}

		TextureGL* sprite = levelData->sprites.Append();
		if (!psdFile.LoadLayerTextureGL(i, LayerChannelID::ALL, GL_LINEAR, GL_LINEAR,
                                        GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, &allocator, sprite)) {
			LOG_ERROR("Failed to load layer %.*s to OpenGL for %.*s\n",
                      layer.name.size, layer.name.data, filePath.size, filePath.data);
			return false;
		}

        SpriteMetadata* spriteMetadata = levelData->spriteMetadata.Append();
		spriteMetadata->type = spriteType;
		Vec2Int offset = Vec2Int {
			layer.left,
			psdFile.size.y - layer.bottom
		};
		spriteMetadata->pos = ToVec2(offset) / pixelsPerUnit;
		spriteMetadata->anchor = Vec2::zero;
		spriteMetadata->restAngle = 0.0f;
		spriteMetadata->flipped = false;
	}

    if (levelData->floor.line.size == 0) {
        LOG_ERROR("Level ground collision not initialized (%.*s)\n", filePath.size, filePath.data);
        return false;
    }

	for (uint64 i = 0; i < levelData->sprites.size; i++) {
        const TextureGL* sprite = &levelData->sprites[i];
		SpriteMetadata* spriteMetadata = &levelData->spriteMetadata[i];
		if (spriteMetadata->type == SpriteType::OBJECT) {
			Vec2 worldSize = ToVec2(sprite->size) / pixelsPerUnit;
			Vec2 coords = levelData->floor.GetCoordsFromWorldPos(spriteMetadata->pos + worldSize / 2.0f);

			spriteMetadata->coords = coords;
			spriteMetadata->anchor = Vec2::one / 2.0f;

			Vec2 floorPos, floorNormal;
			levelData->floor.GetInfoFromCoordX(spriteMetadata->coords.x, &floorPos, &floorNormal);
			spriteMetadata->restAngle = acosf(Dot(Vec2::unitY, floorNormal));
			if (floorNormal.x > 0.0f) {
				spriteMetadata->restAngle = -spriteMetadata->restAngle;
			}
		}
	}

	levelData->loaded = true;

	LOG_INFO("Loaded level data from file %.*s\n", filePath.size, filePath.data);

	return true;
}

void UnloadLevelData(LevelData* levelData)
{
	for (uint64 i = 0; i < levelData->sprites.size; i++) {
		UnloadTexture(levelData->sprites[i]);
	}

	levelData->loaded = false;
}
