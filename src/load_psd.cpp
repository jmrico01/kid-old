#include "load_psd.h"

#include <km_common/km_os.h>
#include <km_common/km_string.h>

#define PSD_COLOR_MODE_RGB 3

global_var const uint64 STRING_MAX_SIZE = 1024;

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
internal int ReadPascalString(const_string str, FixedArray<char, S>* outString)
{
	static_assert(S >= UINT8_MAX_VALUE, "pascal string buffer too small");

	uint8 length = str[0];
	if (str.size < length) {
		length = (uint8)str.size;
	}
	MemCopy(outString->data, &str[1], length);
	outString->size = length;
	return length + 1;
}

template <uint64 S>
internal int ReadUnicodeString(const_string str, FixedArray<char, S>* outString)
{
	int parsedBytes = 0;
	int32 stringLength = ReadBigEndianInt32(&str[parsedBytes]);
	parsedBytes += 4;
	if (stringLength > S) {
		LOG_ERROR("unicode string too long (%d, max %d)\n", stringLength, S);
		return -1;
	}
	outString->size = stringLength;
	for (uint64 c = 0; c < outString->size; c++) {
		int16 unicodeChar = ReadBigEndianInt16(&str[parsedBytes]);
		parsedBytes += 2;
		(*outString)[c] = (char)unicodeChar;
	}

	return parsedBytes;
}

enum class PsdDescriptorItemType
{
	DESCRIPTOR,
	INTEGER,
	OTHER
};

struct PsdDescriptor;

struct PsdDescriptorItem
{
	PsdDescriptorItemType type;
	union {
		int32 integer;
		PsdDescriptor* descriptorPtr;
	};

	bool Load(const_string str, uint64* outParsedBytes);
};

struct PsdDescriptor
{
	static const uint64 MAX_ITEMS = 64;

	FixedArray<char, STRING_MAX_SIZE> name;
	HashTable<PsdDescriptorItem> items;

	bool Load(const_string str, uint64* outParsedBytes)
	{
		uint64 parsedBytes = 0;

		int nameBytes = ReadUnicodeString(str.SliceFrom(parsedBytes), &name);
		if (nameBytes < 0) {
			LOG_ERROR("Failed to parse descriptor name\n");
			return false;
		}
		parsedBytes += nameBytes;

		int32 classIdLength = ReadBigEndianInt32(&str[parsedBytes]);
		parsedBytes += 4;
		if (classIdLength == 0) {
			classIdLength = 4;
		}
		parsedBytes += classIdLength;

		int32 descriptorItems = ReadBigEndianInt32(&str[parsedBytes]);
		parsedBytes += 4;
		for (int32 i = 0; i < descriptorItems; i++) {
			int32 keyLength = ReadBigEndianInt32(&str[parsedBytes]);
			parsedBytes += 4;
			HashKey keyString;
			if (keyLength == 0) {
				keyLength = 4;
			}
			if (!keyString.WriteString(str.Slice(parsedBytes, parsedBytes + keyLength))) {
				LOG_ERROR("key string too big (%d)\n", keyLength);
				return false;
			}
			parsedBytes += keyLength;

			PsdDescriptorItem item;
			uint64 itemBytes;
			if (!item.Load(str.SliceFrom(parsedBytes), &itemBytes)) {
				LOG_ERROR("Failed to load descriptor item %d\n", i);
				return false;
			}
			parsedBytes += itemBytes;

			items.Add(keyString, item);
		}

		*outParsedBytes = parsedBytes;
		return true;
	}

	template <typename T>
        const T* GetItemValue(const char* key) const;

	template <>
        const int32* GetItemValue<int32>(const char* key) const
	{
		const PsdDescriptorItem* item = items.GetValue(key);
		if (item == nullptr) {
			return nullptr;
		}
		if (item->type != PsdDescriptorItemType::INTEGER) {
			return nullptr;
		}
		return &item->integer;
	}

	template <>
        const PsdDescriptor* GetItemValue<PsdDescriptor>(const char* key) const
	{
		const PsdDescriptorItem* item = items.GetValue(key);
		if (item == nullptr) {
			return nullptr;
		}
		if (item->type != PsdDescriptorItemType::DESCRIPTOR) {
			return nullptr;
		}
		return item->descriptorPtr;
	}
};

bool PsdDescriptorItem::Load(const_string str, uint64* outParsedBytes)
{
	uint64 parsedBytes = 0;

    const_string typeString = str.Slice(parsedBytes, parsedBytes + 4);
	parsedBytes += 4;
	if (StringEquals(typeString, ToString("Objc"))) {
		type = PsdDescriptorItemType::DESCRIPTOR;
		descriptorPtr = new PsdDescriptor(); // TODO hmmm
		uint64 descriptorBytes;
		if (!descriptorPtr->Load(str.SliceFrom(parsedBytes), &descriptorBytes)) {
			LOG_ERROR("failed to load descriptor in item\n");
			return false;
		}
		parsedBytes += descriptorBytes;
	}
	else if (StringEquals(typeString, ToString("VlLs"))) {
		type = PsdDescriptorItemType::OTHER;
		int32 listLength = ReadBigEndianInt32(&str[parsedBytes]);
		parsedBytes += 4;
		for (int32 i = 0; i < listLength; i++) {
			PsdDescriptorItem listItem;
			uint64 listItemBytes;
			if (!listItem.Load(str.SliceFrom(parsedBytes), &listItemBytes)) {
				LOG_ERROR("failed to load item in list\n");
				return false;
			}
			parsedBytes += listItemBytes;
		}
	}
	else if (StringEquals(typeString, ToString("doub"))) {
		type = PsdDescriptorItemType::OTHER;
		// float64 doub = *((float64*)&string[parsedBytes]);
		parsedBytes += 8;
	}
	else if (StringEquals(typeString, ToString("TEXT"))) {
		type = PsdDescriptorItemType::OTHER;
		FixedArray<char, STRING_MAX_SIZE> stringData;
		int stringBytes = ReadUnicodeString(str.SliceFrom(parsedBytes), &stringData);
		if (stringBytes < 0) {
			LOG_ERROR("Failed to parse string in descriptor\n");
			return false;
		}
		parsedBytes += stringBytes;
	}
	else if (StringEquals(typeString, ToString("long"))) {
		type = PsdDescriptorItemType::INTEGER;
		integer = ReadBigEndianInt32(&str[parsedBytes]);
		parsedBytes += 4;
	}
	else if (StringEquals(typeString, ToString("bool"))) {
		type = PsdDescriptorItemType::OTHER;
		parsedBytes++; // uint8 boolean = string[parsedBytes++];
	}
	else {
		type = PsdDescriptorItemType::OTHER;
		LOG_ERROR("Unhandled descriptor type %.*s\n", typeString.size, typeString.data);
		return false;
	}

	*outParsedBytes = parsedBytes;
	return true;
}

internal bool GetFractionDescriptorValue(const PsdDescriptor& descriptor, float32* outValue)
{
	const int32* numerator = descriptor.GetItemValue<int32>("numerator");
	const int32* denominator = descriptor.GetItemValue<int32>("denominator");
	if (numerator == nullptr || denominator == nullptr || *denominator == 0) {
		return false;
	}
	*outValue = (float32)(*numerator) / (float32)(*denominator);
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

#if 1
	for (int r = 0; r < height; r++) {
		// Parse data in PackBits format
		// https://en.wikipedia.org/wiki/PackBits
        const uint8* in = inData + dataIndex;
        uint16 rowParsed = 0;
        int pixelX = 0;
        int pixelY = height - r - 1; // TODO flipping here for now - slow memory traversal!
		while (true) {
			int8 header = *(in++);
			if (++rowParsed >= layerRowLengths[r]) {
				break;
			}
			if (header == -128) {
				continue;
			}
			else if (header < 0) {
				// TODO this is the slowest part of the code, and it matters for iteration speed
				uint8 data = *(in++);
				int repeats = 1 - header;
                uint8* out = outData + (pixelY * width + pixelX) * numChannels + channelOffset;
				for (int i = 0; i < repeats; i++) {
					*out = data;
					out += numChannels;
				}
				pixelX += repeats;
				if (++rowParsed >= layerRowLengths[r]) {
					break;
				}
			}
			else if (header >= 0) {
				int dataLength = 1 + header;
				for (int i = 0; i < dataLength; i++) {
					uint8 data = *(in++);
                    uint8* out = outData + (pixelY * width + pixelX) * numChannels + channelOffset;
					*out = data;
					pixelX++;
					if (++rowParsed >= layerRowLengths[r]) {
						if (i < dataLength - 1) {
							LOG_ERROR("data parse unexpected end %d, %d\n", dataIndex, rowParsed);
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
#else
	for (int r = 0; r < height; r++) {
		// Parse data in PackBits format
		// https://en.wikipedia.org/wiki/PackBits
		int16 parsedData = 0;
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
				// TODO this is the slowest part of the code, and it matters for iteration speed
				uint8 data = inData[dataIndex + parsedData];
				int repeats = 1 - header;
				uint64 outIndex = (r * width + pixelX) * numChannels + channelOffset;
				for (int i = 0; i < repeats; i++) {
					outData[outIndex] = data;
					outIndex += numChannels;
				}
				pixelX += repeats;
				if (++parsedData >= layerRowLengths[r]) {
					break;
				}
			}
			else if (header >= 0) {
				int dataLength = 1 + header;
				for (int i = 0; i < dataLength; i++) {
					uint8 data = inData[dataIndex + parsedData];
					uint64 outIndex = (r * width + pixelX) * numChannels + channelOffset;
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
#endif

	return true;
}

template <typename Allocator>
bool PsdFile::LoadLayerImageData(uint64 layerIndex, LayerChannelID channel, Allocator* allocator,
                                 ImageData* outImageData)
{
	const PsdLayerInfo& layerInfo = layers[layerIndex];
	uint8 numChannelsDest = (uint8)layerInfo.channels.size;
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

    const_string psdData = {
		.size = file.size,
		.data = (const char*)file.data
	};
	uint64 psdDataIndex = layerInfo.dataStart;

	for (uint8 c = 0; c < layerInfo.channels.size; c++) {
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
bool PsdFile::LoadLayerTextureGL(uint64 layerIndex, LayerChannelID channel, GLint magFilter,
                                 GLint minFilter, GLint wrapS, GLint wrapT, Allocator* allocator, TextureGL* outTextureGL)
{
	const auto& allocatorState = allocator->SaveState();
	defer (allocator->LoadState(allocatorState));

	ImageData imageData;
	if (!LoadLayerImageData(layerIndex, channel, allocator, &imageData)) {
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

template <typename Allocator>
bool PsdFile::LoadLayerAtPsdSizeTextureGL(uint64 layerIndex, LayerChannelID channel, GLint magFilter,
                                          GLint minFilter, GLint wrapS, GLint wrapT, Allocator* allocator, TextureGL* outTextureGL)
{
	const auto& allocatorState = allocator->SaveState();
	defer (allocator->LoadState(allocatorState));

	ImageData imageDataSmall;
	if (!LoadLayerImageData(layerIndex, channel, allocator, &imageDataSmall)) {
		LOG_ERROR("Failed to load layer image data");
		return false;
	}

	ImageData imageData;
	imageData.size = size;
	imageData.channels = imageDataSmall.channels;
	uint64 sizeLayerData = size.x * size.y * imageDataSmall.channels;
	imageData.data = (uint8*)allocator->Allocate(sizeLayerData);
	if (!imageData.data) {
		LOG_ERROR("Failed to allocate memory for full-size image data\n");
		return false;
	}
	const PsdLayerInfo& layerInfo = layers[layerIndex];
	for (int y = 0; y < size.y; y++) {
		for (int x = 0; x < size.x; x++) {
			int outPixelIndex = (y * size.x + x) * imageData.channels;
			int layerBottom = size.y - layerInfo.bottom;
			int layerTop = size.y - layerInfo.top;
			if (x < layerInfo.left || x >= layerInfo.right || y < layerBottom || y >= layerTop) {
				MemSet(&imageData.data[outPixelIndex], 0, imageData.channels);
				continue;
			}

			int inPixelIndex = ((y - layerBottom) * (layerInfo.right - layerInfo.left) + x - layerInfo.left) * imageData.channels;
			MemCopy(&imageData.data[outPixelIndex], &imageDataSmall.data[inPixelIndex], imageData.channels);
		}
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
bool LoadPsd(PsdFile* psdFile, const_string filePath, Allocator* allocator)
{
	psdFile->file = LoadEntireFile(filePath, allocator);
	if (psdFile->file.data == nullptr) {
		LOG_ERROR("Failed to open PSD file at: %.*s\n", filePath.size, filePath.data);
		return false;
	}
    const_string psdData = {
		.size = psdFile->file.size,
		.data = (const char*)psdFile->file.data
	};
	uint64 parsedBytes = 0;

    const_string psdSignature = psdData.Slice(parsedBytes, parsedBytes + 4);
	parsedBytes += 4;
	if (!StringEquals(psdSignature, ToString("8BPS"))) {
		LOG_ERROR("Invalid PSD signature %.*s, expected 8BPS, on file %.*s\n",
                  psdSignature.size, psdSignature.data, filePath.size, filePath.data);
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
	psdFile->size.x = width;
	psdFile->size.y = height;

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
	// Ignored data here
	parsedBytes += colorModeDataLength;

	int32 imageResourcesLength = ReadBigEndianInt32(&psdData[parsedBytes]);
	parsedBytes += 4;
	uint64 imageResourcesStart = parsedBytes;
	while (parsedBytes < imageResourcesStart + imageResourcesLength) {
        const_string signature = psdData.Slice(parsedBytes, parsedBytes + 4);
		parsedBytes += 4;
		if (!StringEquals(signature, ToString("8BIM"))) {
			LOG_ERROR("Invalid image resource signature %.*s, expected 8BIM, on file %.*s\n",
                      signature.size, signature.data, filePath.size, filePath.data);
			return false;
		}
		uint16 resourceId = ReadBigEndianInt16(&psdData[parsedBytes]);
		parsedBytes += 2;

		FixedArray<char, 255> resourceName;
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
			case PsdImageResource::LAYER_COMPS:
			case PsdImageResource::MEASUREMENT_SCALE:
			case PsdImageResource::TIMELINE:
			case PsdImageResource::SHEET_DISCLOSURE:
			case PsdImageResource::ONION_SKINS:
			case PsdImageResource::COUNT_INFO:
			case PsdImageResource::PATH_SELECTION_STATE:
			case PsdImageResource::ORIGIN_PATH_INFO: {
				int32 descriptorVersion = ReadBigEndianInt32(&psdData[parsedBytes]);
				parsedBytes += 4;
				if (descriptorVersion != 16) {
					LOG_ERROR("descriptorVersion for %x not 16 %.*s\n", resourceId,
                              filePath.size, filePath.data);
					return false;
				}

				PsdDescriptor descriptor;
				uint64 descriptorBytes;
				if (!descriptor.Load(psdData.SliceFrom(parsedBytes), &descriptorBytes)) {
					LOG_ERROR("Failed to parse descriptor for %.*s\n", filePath.size, filePath.data);
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
	psdFile->layers.size = 0;
	if (layerAndMaskInfoLength > 0) {
		int32 layerInfoLength = ReadBigEndianInt32(&psdData[parsedBytes]);
		parsedBytes += 4;
		uint64 layerInfoStart = parsedBytes;

		int16 layerCount = ReadBigEndianInt16(&psdData[parsedBytes]);
		parsedBytes += 2;
		if (layerCount < 0) {
			// NOTE spec says 1st alpha channel contains transparency data for merged result
			layerCount = -layerCount;
		}
		if (layerCount > PSD_MAX_LAYERS) {
			LOG_ERROR("PSD too many layers: %d, max %d\n", layerCount, PSD_MAX_LAYERS);
			return false;
		}
		psdFile->layers.size = layerCount;
		uint64 currentGroupEnd = psdFile->layers.size;

		for (uint64 layerIndex = 0; layerIndex < psdFile->layers.size; layerIndex++) {
			PsdLayerInfo& layerInfo = psdFile->layers[layerIndex];
			layerInfo.inTimeline = false;

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
			layerInfo.channels.size = layerChannels;

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

            const_string signature = psdData.Slice(parsedBytes, parsedBytes + 4);
			parsedBytes += 4;
			if (!StringEquals(signature, ToString("8BIM"))) {
				LOG_ERROR("Invalid blend mode signature %.*s, expected 8BIM, on file %.*s\n",
                          signature.size, signature.data, filePath.size, filePath.data);
				return false;
			}

            const_string blendModeKey = psdData.Slice(parsedBytes, parsedBytes + 4);
			parsedBytes += 4;
			if (StringEquals(blendModeKey, ToString("norm"))) {
				layerInfo.blendMode = LayerBlendMode::NORMAL;
			}
			else if (StringEquals(blendModeKey, ToString("mul "))) { // TODO trailing space is eh...
				layerInfo.blendMode = LayerBlendMode::MULTIPLY;
			}
			else {
				LOG_ERROR("Unsupported blend mode %.*s\n", blendModeKey.size, blendModeKey.data);
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
			// Ignored data here
			parsedBytes += layerMaskDataLength;

			int32 layerBlendingRangesLength = ReadBigEndianInt32(&psdData[parsedBytes]);
			parsedBytes += 4;
			// Ignored data here
			parsedBytes += layerBlendingRangesLength;

			int layerNameBytes = ReadPascalString(psdData.SliceFrom(parsedBytes), &layerInfo.name);
			layerNameBytes = RoundToPowerOfTwo(layerNameBytes, 4);
			parsedBytes += layerNameBytes;

			while (parsedBytes < extraDataStart + extraDataLength) {
				const_string signature2 = psdData.Slice(parsedBytes, parsedBytes + 4);
				parsedBytes += 4;
				if (!StringEquals(signature2, ToString("8BIM")) && !StringEquals(signature2, ToString("8B64"))) {
					LOG_ERROR("Invalid additional layer info, signature %.*s, expected 8BIM or 8B64, layer %.*s, file %.*s\n",
                              signature2.size, signature2.data,
                              layerInfo.name.size, layerInfo.name.data,
                              filePath.size, filePath.data);
					return false;
				}
                const_string key = psdData.Slice(parsedBytes, parsedBytes + 4);
				parsedBytes += 4;

				int32 addLayerInfoLength = ReadBigEndianInt32(&psdData[parsedBytes]);
				parsedBytes += 4;
				// Ignored data here
				if (IsOdd(addLayerInfoLength)) {
					addLayerInfoLength++;
				}
				uint64 addLayerInfoStart = parsedBytes;

				if (StringEquals(key, ToString("lyid"))) {
					if (addLayerInfoLength != 4) {
						LOG_ERROR("layer \"%.*s\" ID length %d, expected 4\n",
                                  layerInfo.name.size, layerInfo.name.data,
                                  addLayerInfoLength);
						return false;
					}
					// int32 layerId = ReadBigEndianInt32(&psdData[parsedBytes]);
					parsedBytes += 4;
				}
				else if (StringEquals(key, ToString("lsct"))) {
					int32 sectionDividerType = ReadBigEndianInt32(&psdData[parsedBytes]);
					parsedBytes += 4;
					switch (sectionDividerType) {
						case 1:
						case 2: {
							if (currentGroupEnd == psdFile->layers.size) {
								LOG_ERROR("Layer group start without end for file %s\n", filePath);
								return false;
							}
							for (uint64 l = 0; l < layerIndex; l++) {
								if (psdFile->layers[l].parentIndex == currentGroupEnd) {
									psdFile->layers[l].parentIndex = layerIndex;
								}
							}
							currentGroupEnd = psdFile->layers.size;
						} break;
						case 3: {
							currentGroupEnd = layerIndex;
						} break;
					}
					// Ignored data here
				}
				else if (StringEquals(key, ToString("shmd"))) {
					int32 itemCount = ReadBigEndianInt32(&psdData[parsedBytes]);
					parsedBytes += 4;
					for (int32 i = 0; i < itemCount; i++) {
						const_string signature3 = psdData.Slice(parsedBytes, parsedBytes + 4);
						parsedBytes += 4;
                        const_string metadataKey = psdData.Slice(parsedBytes, parsedBytes + 4);
						parsedBytes += 4;
						parsedBytes++; // uint8 copyOnSheetDuplication = psdData[parsedBytes++];
						parsedBytes += 3; // padding
						int32 metadataLength = ReadBigEndianInt32(&psdData[parsedBytes]);
						parsedBytes += 4;

						uint64 metadataStart = parsedBytes;
						if (!StringEquals(signature3, ToString("8BIM"))) {
							// idk, spooky? skip!
						}
						else if (StringEquals(metadataKey, ToString("tmln"))) {
							int32 descriptorVersion = ReadBigEndianInt32(&psdData[parsedBytes]);
							parsedBytes += 4;
							if (descriptorVersion != 16) {
								LOG_ERROR("tmln descriptor version %d, expected 16, file %.*s\n",
                                          descriptorVersion, filePath.size, filePath.data);
								return false;
							}
							PsdDescriptor descriptor;
							uint64 descriptorBytes;
							if (!descriptor.Load(psdData.SliceFrom(parsedBytes), &descriptorBytes)) {
								LOG_ERROR("Failed to parse tmln as descriptor\n");
								return false;
							}
							parsedBytes += descriptorBytes;

							const PsdDescriptor* timeScope = descriptor.GetItemValue<PsdDescriptor>("timeScope");
							if (timeScope == nullptr) {
								LOG_ERROR("Failed to get timeScope in tmln descriptor\n");
								return false;
							}

							const PsdDescriptor* timeStart = timeScope->GetItemValue<PsdDescriptor>("Strt");
							if (timeStart == nullptr) {
								LOG_ERROR("Failed to get Strt in timeScope for tmln descriptor\n");
								return false;
							}
							float32 startValue;
							if (!GetFractionDescriptorValue(*timeStart, &startValue)) {
								LOG_ERROR("Bad Strt value in timeScope for tmln descriptor\n");
								return false;
							}
							layerInfo.timelineStart = startValue;

							const PsdDescriptor* inTime = timeScope->GetItemValue<PsdDescriptor>("inTime");
							if (inTime == nullptr) {
								LOG_ERROR("Failed to get inTime in timeScope for tmln descriptor\n");
								return false;
							}
							float32 inTimeValue;
							if (!GetFractionDescriptorValue(*inTime, &inTimeValue)) {
								LOG_ERROR("Bad Strt value in timeScope for tmln descriptor\n");
								return false;
							}
							const PsdDescriptor* outTime = timeScope->GetItemValue<PsdDescriptor>("outTime");
							if (outTime == nullptr) {
								LOG_ERROR("Failed to get outTime in timeScope for tmln descriptor\n");
								return false;
							}
							float32 outTimeValue;
							if (!GetFractionDescriptorValue(*outTime, &outTimeValue)) {
								LOG_ERROR("Bad Strt value in timeScope for tmln descriptor\n");
								return false;
							}

							if (inTimeValue > outTimeValue) {
								LOG_ERROR("inTime > outTime in tmln descriptor\n");
								return false;
							}
							layerInfo.timelineDuration = outTimeValue - inTimeValue;

							layerInfo.inTimeline = true;
						}
						parsedBytes = metadataStart + metadataLength;
					}
				}
				else {
					// All other keywords, print if you're curious
				}
				parsedBytes = addLayerInfoStart + addLayerInfoLength;
			}

			parsedBytes = extraDataStart + extraDataLength;

			// Temporarily point layer parent to the end of the group.
			// Will be patched up when the start of the group is parsed.
			if (currentGroupEnd != layerIndex) {
				layerInfo.parentIndex = currentGroupEnd;
			}
			else {
				layerInfo.parentIndex = psdFile->layers.size;
			}
		}

		for (uint64 l = 0; l < psdFile->layers.size; l++) {
			PsdLayerInfo& layerInfo = psdFile->layers[l];
			layerInfo.dataStart = parsedBytes;
			for (uint64 c = 0; c < layerInfo.channels.size; c++) {
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
void FreePsd(PsdFile* psdFile, Allocator* allocator)
{
	FreeFile(psdFile->file, allocator);
}