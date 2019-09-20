#include "load_level.h"

#undef STB_SPRINTF_IMPLEMENTATION
#include <stb_sprintf.h>

#include <queue> // TODO implement in km_lib?

internal int ToFlatIndex(Vec2Int index, Vec2Int size)
{
	return index.y * size.x + index.x;
}

internal bool PixelInBounds(Vec2Int pixel, Vec2Int size)
{
	return 0 <= pixel.x && pixel.x < size.x && 0 <= pixel.y && pixel.y < size.y;
}

template <typename Allocator>
internal bool GetLoop(const ImageData& imageAlpha, Allocator* allocator,
	DynamicArray<Vec2Int>* outLoop)
{
	const int NUM_PIXELS = imageAlpha.size.x * imageAlpha.size.y;
	const Vec2Int PIXEL_NULL = Vec2Int { -1, -1 };

	FixedArray<Vec2Int, 4> neighborOffsets4;
	neighborOffsets4.array.size = 0;
	neighborOffsets4.Init();
	neighborOffsets4.Append(Vec2Int { -1,  0 });
	neighborOffsets4.Append(Vec2Int {  0,  1 });
	neighborOffsets4.Append(Vec2Int {  1,  0 });
	neighborOffsets4.Append(Vec2Int {  0, -1 });

	FixedArray<Vec2Int, 8> neighborOffsets8;
	neighborOffsets8.array.size = 0;
	neighborOffsets8.Init();
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
	uint8* neighborsVisited = (uint8*)allocator->Allocate(NUM_PIXELS * sizeof(uint8));
	if (neighborsVisited == nullptr) {
		LOG_ERROR("Not enough memory for neighborsVisited array\n");
		return false;
	}

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
			for (uint64 o = 0; o < neighborOffsets4.array.size; o++) {
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

	Vec2Int pixel = startPixel;
	outLoop->Append(startPixel);
	do {
		DEBUG_ASSERT(outLoop->array.size > 0);
		Vec2Int prev = outLoop->array.data[outLoop->array.size - 1];
		int index = ToFlatIndex(pixel, imageAlpha.size);
		Vec2Int next = PIXEL_NULL;
		while (true) {
			uint8 offsetIndex = neighborsVisited[index];
			if (offsetIndex >= neighborOffsets8.array.size) {
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

bool32 LevelData::Load(const ThreadContext* thread, const char* levelName, MemoryBlock* transient)
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

	LinearAllocator allocator(transient->size, transient->memory);

	PsdFile psdFile;
	char filePath[PATH_MAX_LENGTH];
	stbsp_snprintf(filePath, PATH_MAX_LENGTH, "data/levels/%s/%s.psd", levelName, levelName);
	if (!OpenPSD(thread, &allocator, filePath, &psdFile)) {
		LOG_ERROR("Failed to open and parse level PSD file %s\n", filePath);
		return false;
	}

	for (uint64 i = 0; i < psdFile.layers.array.size; i++) {
		PsdLayerInfo& layer = psdFile.layers[i];
		if (!layer.visible) {
			continue;
		}

		const auto& allocatorState = allocator.SaveState();
		defer (allocator.LoadState(allocatorState));

		if (StringContains(layer.name.array, "ground_")) {
			ImageData imageAlpha;
			if (!psdFile.LoadLayerImageData(i, &allocator, LAYER_CHANNEL_ALPHA, &imageAlpha)) {
				LOG_ERROR("Failed to load ground layer %.*s image data for %s\n",
					layer.name.array.size, layer.name.array.data, filePath);
				return false;
			}

			DynamicArray<Vec2Int> loop;
			if (!GetLoop(imageAlpha, &allocator, &loop)) {
				LOG_ERROR("Failed to get loop from ground edges\n");
				return false;
			}

			DownsampleLoop(&loop.array, 10);
			if (!IsLoopClockwise(loop.array)) {
				InvertLoop(&loop.array);
			}

			Vec2Int origin = Vec2Int {
				layer.left,
				psdFile.size.y - layer.bottom
			};
			floor.line.array.size = 0;
			floor.line.Init();
			for (uint64 v = 0; v < loop.array.size; v++) {
				Vec2 pos = ToVec2(loop[v] + origin) / REF_PIXELS_PER_UNIT;
				floor.line.Append(pos);
			}
			floor.PrecomputeSampleVerticesFromLine();
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

		if (!psdFile.LoadLayerTextureGL(i, &allocator, LAYER_CHANNEL_ALL,
		GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, &sprite.texture)) {
			LOG_ERROR("Failed to load layer %.*s to OpenGL for %s\n",
				layer.name.array.size, layer.name.array.data, filePath);
			return false;
		}

		sprite.type = spriteType;
		Vec2Int offset = Vec2Int {
			layer.left,
			psdFile.size.y - layer.bottom
		};
		sprite.pos = ToVec2(offset) / REF_PIXELS_PER_UNIT;
		sprite.anchor = Vec2::zero;
		sprite.restAngle = 0.0f;
		sprite.flipped = false;
	}

	stbsp_snprintf(filePath, PATH_MAX_LENGTH, "data/levels/%s/%s.kmkv", levelName, levelName);
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
			/*floor.line.array.size = 0;
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

			floor.PrecomputeSampleVerticesFromLine();*/
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
	for (uint64 i = 0; i < sprites.array.size; i++) {
		UnloadTextureGL(sprites[i].texture);
	}
	loaded = false;
}
