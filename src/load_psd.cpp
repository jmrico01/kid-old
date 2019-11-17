#include "load_psd.h"

#include <km_common/km_string.h>

#define PSD_COLOR_MODE_RGB 3

static const uint64 STRING_MAX_SIZE = 1024;

enum class PsdCompression
{
	RAW      = 0,
	PACKBITS = 1
};

enum class PsdImageResource
{
	LAYER_STATE             = 0x0400,
	LAYER_GROUPS            = 0x0402,
	GRID_AND_GUIDES         = 0x0408,
	SLICES                  = 0x041A,
	URL_LIST                = 0x041E,
	LAYER_COMPS             = 0x0429,
	LAYER_SELECTION_IDS     = 0x042D,
	LAYER_GROUPS_ENABLED_ID = 0x0430,
	MEASUREMENT_SCALE       = 0x0432,
	TIMELINE                = 0x0433,
	SHEET_DISCLOSURE        = 0x0434,
	ONION_SKINS             = 0x0436,
	COUNT_INFO              = 0x0438,
	PRINT_INFO              = 0x043A,
	PRINT_STYLE             = 0x043B,
	PATH_SELECTION_STATE    = 0x0440,
	ORIGIN_PATH_INFO        = 0x0BB8
};

internal int16 ReadBigEndianInt16(const uint8 bigEndian[2])
{
	return ((int16)bigEndian[0] << 8) + bigEndian[1];
}
internal int16 ReadBigEndianInt16(const char bigEndian[2])
{
	return ReadBigEndianInt16((const uint8*)bigEndian);
}

internal int32 ReadBigEndianInt32(const uint8 bigEndian[4])
{
	return ((int32)bigEndian[0] << 24) + (bigEndian[1] << 16) + (bigEndian[2] << 8) + bigEndian[3];
}
internal int32 ReadBigEndianInt32(const char bigEndian[4])
{
	return ReadBigEndianInt32((const uint8*)bigEndian);
}

template <uint64 S>
internal int ReadPascalString(const Array<char>& string, FixedArray<char, S>* outString)
{
	static_assert(S >= UINT8_MAX_VALUE, "pascal string buffer too small");

	uint8 length = string[0];
	if (string.size < length) {
		length = (uint8)string.size;
	}
	MemCopy(outString->array.data, &string[1], length);
	outString->array.size = length;
	return length + 1;
}

template <uint64 S>
internal int ReadUnicodeString(const Array<char>& string, FixedArray<char, S>* outString)
{
	int parsedBytes = 0;
	int32 stringLength = ReadBigEndianInt32(&string[parsedBytes]);
	parsedBytes += 4;
	if (stringLength > S) {
		LOG_ERROR("unicode string too long (%d, max %d)\n", stringLength, S);
		return -1;
	}
	outString->array.size = stringLength;
	for (uint64 c = 0; c < outString->array.size; c++) {
		int16 unicodeChar = ReadBigEndianInt16(&string[parsedBytes]);
		parsedBytes += 2;
		(*outString)[c] = (char)unicodeChar;
	}

	return parsedBytes;
}

struct PsdDescriptorItem
{
	FixedArray<char, 4> type;
	// stuff per type

	bool Load(const Array<char>& string, uint64* outParsedBytes);
};

struct PsdDescriptor
{
	static const uint64 MAX_ITEMS = 64;

	FixedArray<char, STRING_MAX_SIZE> name;
	FixedArray<char, STRING_MAX_SIZE> classId;
	int32 classIdInt;
	FixedArray<PsdDescriptorItem, MAX_ITEMS> items;

	bool Load(const Array<char>& string, uint64* outParsedBytes)
	{
		uint64 parsedBytes = 0;

		name.Init();
		int nameBytes = ReadUnicodeString(string.SliceFrom(parsedBytes), &name);
		if (nameBytes < 0) {
			LOG_ERROR("Failed to parse descriptor name\n");
			return false;
		}
		parsedBytes += nameBytes;

		classId.Init();
		int classIdBytes = ReadUnicodeString(string.SliceFrom(parsedBytes), &classId);
		if (classIdBytes < 0) {
			LOG_ERROR("Failed to parse class ID string\n");
			return false;
		}
		parsedBytes += classIdBytes;
		if (classId.array.size == 0) {
			classIdInt = ReadBigEndianInt32(&string[parsedBytes]);
			parsedBytes += 4;
		}

		int32 descriptorItems = ReadBigEndianInt32(&string[parsedBytes]);
		parsedBytes += 4;
		items.Init();
		items.array.size = descriptorItems;
		for (uint64 i = 0; i < items.array.size; i++) {
			int32 keyLength = ReadBigEndianInt32(&string[parsedBytes]);
			parsedBytes += 4;
			if (keyLength > STRING_MAX_SIZE) {
				LOG_ERROR("key string too big (%d, max is %d)\n", keyLength, STRING_MAX_SIZE);
				return false;
			}
			FixedArray<char, STRING_MAX_SIZE> keyString;
			keyString.Init();
			keyString.array.size = keyLength;
			if (keyLength != 0) {
				MemCopy(keyString.array.data, &string[parsedBytes], keyLength);
				parsedBytes += keyLength;
				LOG_INFO("item key string %.*s\n", keyString.array.size, keyString.array.data);
			}
			else {
				const Array<char> shortKey = string.Slice(parsedBytes, parsedBytes + 4);
				parsedBytes += 4;
				LOG_INFO("item key %.*s\n", shortKey.size, shortKey.data);
			}

			uint64 itemBytes;
			if (!items[i].Load(string.SliceFrom(parsedBytes), &itemBytes)) {
				LOG_ERROR("Failed to load descriptor item %d\n", i);
				return false;
			}
			parsedBytes += itemBytes;
		}

		*outParsedBytes = parsedBytes;
		return true;
	}
};

bool PsdDescriptorItem::Load(const Array<char>& string, uint64* outParsedBytes)
{
	uint64 parsedBytes = 0;

	type.Init();
	type.array.size = 4;
	MemCopy(type.array.data, &string[parsedBytes], 4);
	parsedBytes += 4;
	// (StringCompare(type.array, "obj ")) {
	// }
	if (StringCompare(type.array, "Objc")) {
		PsdDescriptor descriptor;
		uint64 descriptorBytes;
		LOG_INFO("descriptor {\n");
		if (!descriptor.Load(string.SliceFrom(parsedBytes), &descriptorBytes)) {
			LOG_ERROR("failed to load descriptor in item\n");
			return false;
		}
		LOG_INFO("}\n");
		parsedBytes += descriptorBytes;
	}
	else if (StringCompare(type.array, "VlLs")) {
		int32 listLength = ReadBigEndianInt32(&string[parsedBytes]);
		parsedBytes += 4;
		for (int32 i = 0; i < listLength; i++) {
			PsdDescriptorItem listItem;
			uint64 listItemBytes;
			if (!listItem.Load(string.SliceFrom(parsedBytes), &listItemBytes)) {
				LOG_ERROR("failed to load item in list\n");
				return false;
			}
			parsedBytes += listItemBytes;
		}
	}
	else if (StringCompare(type.array, "doub")) {
		double doub = *((double*)&string[parsedBytes]);
		parsedBytes += 8;
		LOG_INFO("double %f\n", doub);
	}
	// else if (StringCompare(type.array, "UntF")) {
	// }
	else if (StringCompare(type.array, "TEXT")) {
		FixedArray<char, STRING_MAX_SIZE> stringData;
		stringData.Init();
		int stringBytes = ReadUnicodeString(string.SliceFrom(parsedBytes), &stringData);
		if (stringBytes < 0) {
			LOG_ERROR("Failed to parse string in descriptor\n");
			return false;
		}
		parsedBytes += stringBytes;
		LOG_INFO("string %.*s\n", stringData.array.size, stringData.array.data);
	}
	// else if (StringCompare(type.array, "enum")) {
	// }
	else if (StringCompare(type.array, "long")) {
		int32 integer = ReadBigEndianInt32(&string[parsedBytes]);
		parsedBytes += 4;
		LOG_INFO("integer %d\n", integer);
	}
	// else if (StringCompare(type.array, "comp")) {
	// }
	else if (StringCompare(type.array, "bool")) {
		uint8 boolean = string[parsedBytes++];
		LOG_INFO("boolean %d\n", boolean);
	}
	// else if (StringCompare(type.array, "GlbO")) {
	// }
	// else if (StringCompare(type.array, "type")) {
	// }
	// else if (StringCompare(type.array, "GlbC")) {
	// }
	// else if (StringCompare(type.array, "alis")) {
	// }
	// else if (StringCompare(type.array, "tdta")) {
	// }
	else {
		LOG_ERROR("Unhandled descriptor type %.*s\n", type.array.size, type.array.data);
		return false;
	}

	*outParsedBytes = parsedBytes;
	return true;
}

internal void ReadRawData(const uint8* inData, int width, int height,
	uint8 numChannels, int channelOffset, uint8* outData)
{
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			uint64 inIndex = y * width + x;
			uint64 invertedY = height - y - 1; // NOTE y-axis is inverted by this procedure
			uint64 outIndex = (invertedY * width + x) * numChannels + channelOffset;
			outData[outIndex] = inData[inIndex];
		}
	}
}

template <typename Allocator>
internal bool ReadPackBitsData(const uint8* inData, Allocator* allocator, int width, int height,
	uint8 numChannels, int channelOffset, uint8* outData)
{
	uint64 sizeRowLengths = height * sizeof(int16);
	uint16* layerRowLengths = (uint16*)allocator->Allocate(sizeRowLengths);
	if (!layerRowLengths) {
		LOG_ERROR("Not enough memory for layer row lengths, need %d\n", layerRowLengths);
		return false;
	}
	defer (allocator->Free(layerRowLengths));

	uint64 dataIndex = 0;

	for (int r = 0; r < height; r++) {
		int16 rowLengthBytes = ReadBigEndianInt16(&inData[dataIndex]);
		dataIndex += 2;
		layerRowLengths[r] = rowLengthBytes;
	}

	for (int r = 0; r < height; r++) {
		// Parse data in PackBits format
		// https://en.wikipedia.org/wiki/PackBits
		int16 parsedData = 0;
		uint64 pixelY = height - r - 1; // NOTE y-axis is inverted by this procedure
		uint64 pixelX = 0;
		while (true) {
			int8 header = inData[dataIndex + parsedData];
			if (++parsedData >= layerRowLengths[r]) {
				break;
			}
			if (header == -128) {
				continue;
			}
			else if (header < 0) {
				uint8 data = inData[dataIndex + parsedData];
				int repeats = 1 - header;
				for (int i = 0; i < repeats; i++) {
					uint64 outIndex = (pixelY * width + pixelX) * numChannels + channelOffset;
					outData[outIndex] = data;
					pixelX++;
				}
				if (++parsedData >= layerRowLengths[r]) {
					break;
				}
			}
			else if (header >= 0) {
				int dataLength = 1 + header;
				for (int i = 0; i < dataLength; i++) {
					uint8 data = inData[dataIndex + parsedData];
					uint64 outIndex = (pixelY * width + pixelX) * numChannels + channelOffset;
					outData[outIndex] = data;
					pixelX++;
					if (++parsedData >= layerRowLengths[r]) {
						if (i < dataLength - 1) {
							LOG_ERROR("data parse unexpected end %d, %d\n", dataIndex, parsedData);
							return false;
						}
						break;
					}
				}
			}
		}

		if (pixelX != width) {
			LOG_ERROR("PackBits row length mismatch: %d vs %d\n", pixelX, width);
			return false;
		}

		dataIndex += layerRowLengths[r];
	}

	return true;
}

template <typename Allocator>
bool PsdFile::LoadLayerImageData(uint64 layerIndex, Allocator* allocator, LayerChannelID channel,
	ImageData* outImageData)
{
	const PsdLayerInfo& layerInfo = layers[layerIndex];
	uint8 numChannelsDest = (uint8)layerInfo.channels.array.size;
	if (channel != LayerChannelID::ALL) {
		numChannelsDest = 1;
	}
	int layerWidth = layerInfo.right - layerInfo.left;
	int layerHeight = layerInfo.bottom - layerInfo.top;

	uint64 sizeLayerData = layerWidth * layerHeight * numChannelsDest;
	uint8* layerData = (uint8*)allocator->Allocate(sizeLayerData);
	if (!layerData) {
		LOG_ERROR("Not enough memory for layer data, need %d\n", sizeLayerData);
		return false;
	}

	const Array<char> psdData = {
		.size = file.size,
		.data = (char*)file.data
	};
	uint64 psdDataIndex = layerInfo.dataStart;

	for (uint8 c = 0; c < layerInfo.channels.array.size; c++) {
		const LayerChannelInfo& layerChannelInfo = layerInfo.channels[c];
		int channelOffsetDest = (int)layerChannelInfo.channelID;
		if (channel != LayerChannelID::ALL) {
			if (layerChannelInfo.channelID != channel) {
				psdDataIndex += layerChannelInfo.dataSize;
				continue;
			}
			channelOffsetDest = 0;
		}

		int16 compression = ReadBigEndianInt16(&psdData[psdDataIndex]);
		// TODO hmm
		const uint8* layerImageData = (uint8*)&psdData[psdDataIndex + 2];
		psdDataIndex += layerChannelInfo.dataSize;

		switch (compression) {
			case PsdCompression::RAW: {
				ReadRawData(layerImageData, layerWidth, layerHeight,
					numChannelsDest, channelOffsetDest, layerData);
			} break;
			case PsdCompression::PACKBITS: {
				if (!ReadPackBitsData(layerImageData, allocator, layerWidth, layerHeight,
				numChannelsDest, channelOffsetDest, layerData)) {
					LOG_ERROR("Failed to read PackBits data\n");
					return false;
				}
			} break;
			default: {
				LOG_ERROR("Unhandled layer compression %d\n", compression);
				return false;
			} break;
		}
	}

	outImageData->size = { layerWidth, layerHeight };
	outImageData->channels = numChannelsDest;
	outImageData->data = layerData;
	return true;
}

template <typename Allocator>
bool PsdFile::LoadLayerTextureGL(uint64 layerIndex, Allocator* allocator, LayerChannelID channel,
	GLint magFilter, GLint minFilter, GLint wrapS, GLint wrapT, TextureGL* outTextureGL)
{
	ImageData imageData;
	if (!LoadLayerImageData(layerIndex, allocator, channel, &imageData)) {
		LOG_ERROR("Failed to load layer image data");
		return false;
	}

	GLenum formatGL;
	if (imageData.channels == 4) {
		formatGL = GL_RGBA;
	}
	else if (imageData.channels == 3) {
		formatGL = GL_RGB;
	}
	else {
		LOG_ERROR("Unsupported layer channel number for GL: %d\n", imageData.channels);
		return false;
	}
	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, formatGL, imageData.size.x, imageData.size.y,
		0, formatGL, GL_UNSIGNED_BYTE, (const GLvoid*)imageData.data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);

	outTextureGL->textureID = textureID;
	outTextureGL->size = imageData.size;
	return true;
}

// Reference: Official Adobe File Formats specification document
// https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
template <typename Allocator>
bool OpenPSD(const ThreadContext* thread, Allocator* allocator,
	const char* filePath, PsdFile* outPsdFile)
{
	outPsdFile->file = PlatformReadFile(thread, allocator, filePath);
	if (!outPsdFile->file.data) {
		LOG_ERROR("Failed to open PSD file at: %s\n", filePath);
		return false;
	}
	const Array<char> psdData = {
		.size = outPsdFile->file.size,
		.data = (char*)outPsdFile->file.data
	};
	uint64 parsedBytes = 0;

	const Array<char> psdSignature = psdData.Slice(parsedBytes, parsedBytes + 4);
	parsedBytes += 4;
	if (!StringCompare(psdSignature, "8BPS")) {
		LOG_ERROR("Invalid PSD signature %.*s, expected 8BPS, on file %s\n",
			psdSignature.size, psdSignature.data, filePath);
		return false;
	}

	int16 version = ReadBigEndianInt16(&psdData[parsedBytes]);
	parsedBytes += 2;
	if (version != 1) {
		LOG_ERROR("Expected PSD version 1, found %d\n", version);
	}

	parsedBytes += 6; // reserved

	int16 channels = ReadBigEndianInt16(&psdData[parsedBytes]);
	parsedBytes += 2;
	if (channels != 3 && channels != 4) {
		LOG_ERROR("Expected 3 or 4 color channels (RGB or RGBA), found %d\n", channels);
		return false;
	}

	int32 height = ReadBigEndianInt32(&psdData[parsedBytes]);
	parsedBytes += 4;
	int32 width = ReadBigEndianInt32(&psdData[parsedBytes]);
	parsedBytes += 4;
	outPsdFile->size.x = width;
	outPsdFile->size.y = height;

	int16 depth = ReadBigEndianInt16(&psdData[parsedBytes]);
	parsedBytes += 2;
	if (depth != 8) {
		LOG_ERROR("Unsupported color depth (not 8): %d\n", depth);
		return false;
	}

	int16 colorMode = ReadBigEndianInt16(&psdData[parsedBytes]);
	parsedBytes += 2;
	if (colorMode != PSD_COLOR_MODE_RGB) {
		LOG_ERROR("Unsupported PSD color mode (non-RGB): %d\n", colorMode);
		return false;
	}

	// TODO unsafe file data traversal yada yada, have to check lengths against psdFile.size
	int32 colorModeDataLength = ReadBigEndianInt32(&psdData[parsedBytes]);
	parsedBytes += 4;
	// TODO nothing here for now
	parsedBytes += colorModeDataLength;

	int32 imageResourcesLength = ReadBigEndianInt32(&psdData[parsedBytes]);
	parsedBytes += 4;
	uint64 imageResourcesStart = parsedBytes;
	while (parsedBytes < imageResourcesStart + imageResourcesLength) {
		const Array<char> signature = psdData.Slice(parsedBytes, parsedBytes + 4);
		parsedBytes += 4;
		if (!StringCompare(signature, "8BIM")) {
			LOG_ERROR("Invalid image resource signature %.*s, expected 8BIM, on file %s\n",
				signature.size, signature.data, filePath);
			return false;
		}
		uint16 resourceId = ReadBigEndianInt16(&psdData[parsedBytes]);
		parsedBytes += 2;

		FixedArray<char, 255> resourceName;
		resourceName.Init();
		int nameBytes = ReadPascalString(psdData.SliceFrom(parsedBytes), &resourceName);
		if (nameBytes == 0) {
			nameBytes = 2;
		}
		else if (IsOdd(nameBytes)) {
			nameBytes++;
		}
		parsedBytes += nameBytes;

		int32 dataSize = ReadBigEndianInt32(&psdData[parsedBytes]);
		parsedBytes += 4;

		uint64 resourceStart = parsedBytes;
		switch (resourceId) {
			case PsdImageResource::LAYER_GROUPS: {
			} break;
			case PsdImageResource::SLICES: {
				int32 slicesVersion = ReadBigEndianInt32(&psdData[parsedBytes]);
				parsedBytes += 4;
				if (slicesVersion == 6) {
					parsedBytes += 4 * 4;
					FixedArray<char, STRING_MAX_SIZE> slicesName;
					slicesName.Init();
					int slicesNameBytes = ReadUnicodeString(psdData.SliceFrom(parsedBytes), &slicesName);
					if (slicesNameBytes < 0) {
						LOG_ERROR("Failed to read slices name\n");
						return false;
					}
					parsedBytes += slicesNameBytes;
					int32 numSlices = ReadBigEndianInt32(&psdData[parsedBytes]);
					parsedBytes += 4;
					for (int32 s = 0; s < numSlices; s++) {
						// int32 sliceId = ReadBigEndianInt32(&psdData[parsedBytes]);
						parsedBytes += 4;
						// int32 groupId = ReadBigEndianInt32(&psdData[parsedBytes]);
						parsedBytes += 4;
						int32 sliceOrigin = ReadBigEndianInt32(&psdData[parsedBytes]);
						parsedBytes += 4;
						if (sliceOrigin == 1) {
							//int32 layerId = ReadBigEndianInt32(&psdData[parsedBytes]);
							parsedBytes += 4;
						}
						FixedArray<char, STRING_MAX_SIZE> sliceString;
						sliceString.Init();
						int sliceStringBytes = ReadUnicodeString(psdData.SliceFrom(parsedBytes), &sliceString);
						if (sliceStringBytes < 0) {
							LOG_ERROR("Failed to read slice name\n");
							return false;
						}
						parsedBytes += sliceStringBytes;
						// int32 sliceType = ReadBigEndianInt32(&psdData[parsedBytes]);
						parsedBytes += 4;
						parsedBytes += 4 * 4;

						sliceStringBytes = ReadUnicodeString(psdData.SliceFrom(parsedBytes), &sliceString);
						if (sliceStringBytes < 0) {
							LOG_ERROR("Failed to read slice URL\n");
							return false;
						}
						parsedBytes += sliceStringBytes;
						sliceStringBytes = ReadUnicodeString(psdData.SliceFrom(parsedBytes), &sliceString);
						if (sliceStringBytes < 0) {
							LOG_ERROR("Failed to read slice target\n");
							return false;
						}
						parsedBytes += sliceStringBytes;
						sliceStringBytes = ReadUnicodeString(psdData.SliceFrom(parsedBytes), &sliceString);
						if (sliceStringBytes < 0) {
							LOG_ERROR("Failed to read slice message\n");
							return false;
						}
						parsedBytes += sliceStringBytes;
						sliceStringBytes = ReadUnicodeString(psdData.SliceFrom(parsedBytes), &sliceString);
						if (sliceStringBytes < 0) {
							LOG_ERROR("Failed to read slice alt tag\n");
							return false;
						}
						parsedBytes += sliceStringBytes;

						uint8 isHtml = psdData[parsedBytes++];
						if (isHtml) {
							// LOG_INFO("text is HTML\n");
						}
						sliceStringBytes = ReadUnicodeString(psdData.SliceFrom(parsedBytes), &sliceString);
						if (sliceStringBytes < 0) {
							LOG_ERROR("Failed to read slice text\n");
							return false;
						}
						parsedBytes += sliceStringBytes;
					}
					break;
				}
				else if (slicesVersion == 7 || slicesVersion == 8) {
					// fall-through to cases below
				}
				else {
					LOG_ERROR("Invalid slices version %d in %s\n", slicesVersion, filePath);
					return false;
				}
			} // intentional fall-through
			case PsdImageResource::LAYER_COMPS:
			case PsdImageResource::MEASUREMENT_SCALE:
			case PsdImageResource::TIMELINE:
			case PsdImageResource::SHEET_DISCLOSURE:
			case PsdImageResource::ONION_SKINS:
			case PsdImageResource::COUNT_INFO:
			// case PsdImageResource::PRINT_INFO:
			// case PsdImageResource::PRINT_STYLE:
			case PsdImageResource::PATH_SELECTION_STATE:
			case PsdImageResource::ORIGIN_PATH_INFO: {
				int32 descriptorVersion = ReadBigEndianInt32(&psdData[parsedBytes]);
				parsedBytes += 4;
				if (descriptorVersion != 16) {
					LOG_ERROR("descriptorVersion for %x not 16 %s\n", resourceId, filePath);
					return false;
				}

				PsdDescriptor descriptor;
				uint64 descriptorBytes;
				if (!descriptor.Load(psdData.SliceFrom(parsedBytes), &descriptorBytes)) {
					LOG_ERROR("Failed to parse descriptor for %s\n", filePath);
					return false;
				}
			} break;
			default: {
				// Uncomment for reverse engineering purposes
				// LOG_INFO("Unhandled resource ID %x\n", resourceId);
			} break;
		}

		if (IsOdd(dataSize)) {
			dataSize += 1;
		}
		parsedBytes = resourceStart + dataSize;
	}

	int32 layerAndMaskInfoLength = ReadBigEndianInt32(&psdData[parsedBytes]);
	parsedBytes += 4;
	// uint64 layerAndMaskInfoStart = parsedBytes;
	outPsdFile->layers.Init();
	outPsdFile->layers.array.size = 0;
	if (layerAndMaskInfoLength > 0) {
		int32 layerInfoLength = ReadBigEndianInt32(&psdData[parsedBytes]);
		parsedBytes += 4;
		uint64 layerInfoStart = parsedBytes;

		int16 layerCount = ReadBigEndianInt16(&psdData[parsedBytes]);
		parsedBytes += 2;
		if (layerCount < 0) {
			// TODO spec says 1st alpha channel contains transparency data for merged result
			layerCount = -layerCount;
		}
		if (layerCount > PSD_MAX_LAYERS) {
			LOG_ERROR("PSD too many layers: %d, max %d\n", layerCount, PSD_MAX_LAYERS);
			return false;
		}
		outPsdFile->layers.array.size = layerCount;

		for (int16 l = 0; l < layerCount; l++) {
			PsdLayerInfo& layerInfo = outPsdFile->layers[l];
			layerInfo.name.Init();
			layerInfo.channels.Init();

			int32 top = ReadBigEndianInt32(&psdData[parsedBytes]);
			parsedBytes += 4;
			int32 left = ReadBigEndianInt32(&psdData[parsedBytes]);
			parsedBytes += 4;
			int32 bottom = ReadBigEndianInt32(&psdData[parsedBytes]);
			parsedBytes += 4;
			int32 right = ReadBigEndianInt32(&psdData[parsedBytes]);
			parsedBytes += 4;
			layerInfo.left = left;
			layerInfo.right = right;
			layerInfo.top = top;
			layerInfo.bottom = bottom;

			int16 layerChannels = ReadBigEndianInt16(&psdData[parsedBytes]);
			parsedBytes += 2;
			if (layerChannels != 3 && layerChannels != 4) {
				LOG_ERROR("Layer expected 3 or 4 color channels (RGB or RGBA), found %d\n",
					layerChannels);
				return false;
			}
			layerInfo.channels.array.size = layerChannels;

			for (int16 c = 0; c < layerChannels; c++) {
				LayerChannelInfo& layerChannelInfo = layerInfo.channels[c];
				int16 channelID = ReadBigEndianInt16(&psdData[parsedBytes]);
				parsedBytes += 2;
				switch (channelID) {
					case 0: {
						layerChannelInfo.channelID = LayerChannelID::RED;
					} break;
					case 1: {
						layerChannelInfo.channelID = LayerChannelID::GREEN;
					} break;
					case 2: {
						layerChannelInfo.channelID = LayerChannelID::BLUE;
					} break;
					case -1: {
						layerChannelInfo.channelID = LayerChannelID::ALPHA;
					} break;
					default: {
						LOG_ERROR("Unsupported layer channel ID %d\n", channelID);
						return false;
					} break;
				}
				int32 channelDataSize = ReadBigEndianInt32(&psdData[parsedBytes]);
				parsedBytes += 4;
				layerChannelInfo.dataSize = channelDataSize;
			}

			Array<char> signature = psdData.Slice(parsedBytes, parsedBytes + 4);
			parsedBytes += 4;
			if (!StringCompare(signature, "8BIM")) {
				LOG_ERROR("Invalid blend mode signature %.*s, expected 8BIM, on file %s\n",
					signature.size, signature.data, filePath);
				return false;
			}

			uint8 blendModeKey[4];
			MemCopy(blendModeKey, &psdData[parsedBytes], 4);
			parsedBytes += 4;
			if (StringCompare((const char*)blendModeKey, "norm", 4)) {
				layerInfo.blendMode = LayerBlendMode::NORMAL;
			}
			else if (StringCompare((const char*)blendModeKey, "mul", 3)) {
				layerInfo.blendMode = LayerBlendMode::MULTIPLY;
			}
			else {
				LOG_ERROR("Unsupported blend mode %c%c%c%c\n",
					blendModeKey[0], blendModeKey[1], blendModeKey[2], blendModeKey[3]);
				return false;
			}

			uint8 opacity = psdData[parsedBytes++];
			layerInfo.opacity = opacity;
			parsedBytes++; // uint8 clipping
			uint8 flags = psdData[parsedBytes++];
			layerInfo.visible = (flags & 0x02) == 0;
			parsedBytes++; // filler

			int32 extraDataLength = ReadBigEndianInt32(&psdData[parsedBytes]);
			parsedBytes += 4;
			uint64 extraDataStart = parsedBytes;

			int32 layerMaskDataLength = ReadBigEndianInt32(&psdData[parsedBytes]);
			parsedBytes += 4;
			// TODO maybe look at this data
			parsedBytes += layerMaskDataLength;

			int32 layerBlendingRangesLength = ReadBigEndianInt32(&psdData[parsedBytes]);
			parsedBytes += 4;
			// TODO maybe look at this data
			parsedBytes += layerBlendingRangesLength;

			int layerNameBytes = ReadPascalString(psdData.SliceFrom(parsedBytes), &layerInfo.name);
			layerNameBytes = RoundToPowerOfTwo(layerNameBytes, 4);
			parsedBytes += layerNameBytes;

			while (parsedBytes < extraDataStart + extraDataLength) {
				signature = psdData.Slice(parsedBytes, parsedBytes + 4);
				parsedBytes += 4;
				if (!StringCompare(signature, "8BIM") && !StringCompare(signature, "8B64")) {
					LOG_ERROR("Invalid additional layer info, signature %.*s, expected 8BIM or 8B64, layer %.*s, file %.*s\n",
						signature.size, signature.data,
						layerInfo.name.array.size, layerInfo.name.array.data, filePath);
					return false;
				}
				FixedArray<char, 4> key;
				key.Init();
				key.array.size = 4;
				MemCopy(key.array.data, &psdData[parsedBytes], 4);
				parsedBytes += 4;

				int32 addLayerInfoLength = ReadBigEndianInt32(&psdData[parsedBytes]);
				parsedBytes += 4;
				// TODO nothing here for now
				if (IsOdd(addLayerInfoLength)) {
					addLayerInfoLength++;
				}
				uint64 addLayerInfoStart = parsedBytes;

				if (StringCompare(key.array, "lyid")) {
					if (addLayerInfoLength != 4) {
						LOG_ERROR("layer \"%.*s\" ID length %d, expected 4\n",
							layerInfo.name.array.size, layerInfo.name.array.data,
							addLayerInfoLength);
						return false;
					}
					int32 layerId = ReadBigEndianInt32(&psdData[parsedBytes]);
					parsedBytes += 4;
				}
				else if (StringCompare(key.array, "shmd")) {
					int32 itemCount = ReadBigEndianInt32(&psdData[parsedBytes]);
					parsedBytes += 4;
					LOG_INFO("layer \"%.*s\" shmd, %d items:\n",
						layerInfo.name.array.size, layerInfo.name.array.data, itemCount);
					for (int32 i = 0; i < itemCount; i++) {
						signature = psdData.Slice(parsedBytes, parsedBytes + 4);
						parsedBytes += 4;
						const Array<char> metadataKey = psdData.Slice(parsedBytes, parsedBytes + 4);
						parsedBytes += 4;
						uint8 copyOnSheetDuplication = psdData[parsedBytes++];
						parsedBytes += 3; // padding
						int32 metadataLength = ReadBigEndianInt32(&psdData[parsedBytes]);
						parsedBytes += 4;
						LOG_INFO("%.*s key %.*s cosd %d length %d, at %x\n",
							signature.size, signature.data, metadataKey.size, metadataKey.data,
							copyOnSheetDuplication, metadataLength, parsedBytes);

						if (!StringCompare(signature, "8BIM")) {
							// idk, spooky? skip!
						}
						else if (StringCompare(metadataKey, "tmln")) {
							LOG_INFO("taking a big guess here. tmln descriptor:\n");
							parsedBytes += 4; // there's a small number here, ~16 or something, not sure what it means yet
							PsdDescriptor descriptor;
							uint64 descriptorBytes;
							if (!descriptor.Load(psdData.SliceFrom(parsedBytes), &descriptorBytes)) {
								LOG_ERROR("Failed to parse tmln as descriptor\n");
								return false;
							}
							parsedBytes += descriptorBytes;
							LOG_INFO("parsed tmln as descriptor! WOOHOO! at %x\n", parsedBytes);
						}
						// NOTE undocumented data here
						parsedBytes += metadataLength; // TODO shouldn't do this anymore
					}
				}
				else {
					// LOG_INFO("layer %.*s unhandled additional info %c%c%c%c\n",
					// 	layerInfo.name.array.size, layerInfo.name.array.data,
					// 	key[0], key[1], key[2], key[3]);
				}
/*
luni (32 | 108/776)
layer </Layer group> additional info lclr (8 | 192/776)
layer </Layer group> additional info shmd (544 | 748/776)
layer </Layer group> additional info fxrp (16 | 776/776)
*/
				parsedBytes = addLayerInfoStart + addLayerInfoLength;
			}

			parsedBytes = extraDataStart + extraDataLength;
		}

		for (int16 l = 0; l < layerCount; l++) {
			PsdLayerInfo& layerInfo = outPsdFile->layers[l];
			layerInfo.dataStart = parsedBytes;
			for (uint64 c = 0; c < layerInfo.channels.array.size; c++) {
				parsedBytes += layerInfo.channels[c].dataSize;
			}
		}

		if (IsOdd(layerInfoLength)) {
			layerInfoLength++;
		}
		parsedBytes = layerInfoStart + layerInfoLength;

		int32 globalLayerMaskLength = ReadBigEndianInt32(&psdData[parsedBytes]);
		parsedBytes += 4;
		// NOTE nothing here for now
		parsedBytes += globalLayerMaskLength;

		// NOTE there are global additional layer info chunks here
	}

	/*parsedBytes = layerAndMaskInfoStart + layerAndMaskInfoLength;
	uint64 imageDataLength = psdFile.size - parsedBytes;
	int16 compression = ReadBigEndianInt16(&psdData[parsedBytes]);
	parsedBytes += 2;

	for (int r = 0; r < height; r++) {
		int16 rowLengthBytes = ReadBigEndianInt16(&psdData[parsedBytes]);
		parsedBytes += 2;
		layerRowLengths[r] = rowLengthBytes;
	}
	LOG_INFO("image data length %d compression %d\n", imageDataLength, compression);*/

	return true;
}

template <typename Allocator>
void ClosePSD(const ThreadContext* thread, Allocator* allocator, PsdFile* psdFile)
{
	PlatformFreeFile(thread, allocator, &psdFile->file);
}