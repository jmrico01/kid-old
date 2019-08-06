#include "load_psd.h"

#include <km_common/km_string.h>

#include "load_png.h"

#define PSD_COLOR_MODE_RGB 3

#define PSD_MAX_LAYERS 64
#define PSD_LAYER_NAME_MAX_LENGTH 256

enum LayerBlendMode
{
	LAYER_BLEND_MODE_NORMAL,
	LAYER_BLEND_MODE_MULTIPLY
};

struct PSDHeader
{
	uint8 signature[4];
	uint8 beVersion[2];
	uint8 reserved[6];
	uint8 beChannels[2];
	uint8 beHeight[4];
	uint8 beWidth[4];
	uint8 beDepth[2];
	uint8 beColorMode[2];
};

struct AdditionalLayerInfo
{
	uint8 signature[4];
	uint8 key[4];
	uint8 beLength[4];
};

struct LayerInfo
{
	TextureGL textureGL;
	FixedArray<char, PSD_LAYER_NAME_MAX_LENGTH> name;
	Vec2Int boundsMin, boundsMax;
	uint16 channels;
	uint8 opacity;
	LayerBlendMode blendMode;
	bool visible;
};

int16 ReadBigEndianInt16(const uint8 bigEndian[2])
{
	return ((int16)bigEndian[0] << 8) + bigEndian[1];
}

int32 ReadBigEndianInt32(const uint8 bigEndian[4])
{
	return ((int32)bigEndian[0] << 24) + (bigEndian[1] << 16) + (bigEndian[2] << 8) + bigEndian[3];
}

// Reference: Official Adobe File Formats specification document
// https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
bool32 LoadPSD(const ThreadContext* thread, const char* filePath, MemoryBlock* transient,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
	DEBUGReadFileResult psdFile = DEBUGPlatformReadFile(thread, filePath);
	if (!psdFile.data) {
		LOG_ERROR("Failed to open PSD file at: %s\n", filePath);
		return false;
	}
	const uint8* psdData = (uint8*)psdFile.data;

	const PSDHeader* header = (const PSDHeader*)psdData;
	if (header->signature[0] != '8' || header->signature[1] != 'B'
	|| header->signature[2] != 'P' || header->signature[3] != 'S') {
		LOG_ERROR("Invalid PSD signature (8BPS) on file %s\n", filePath);
		return false;
	}
	int16 version = ReadBigEndianInt16(header->beVersion);
	if (version != 1) {
		LOG_ERROR("Expected PSD version 1, found %d\n", version);
	}
	int16 channels = ReadBigEndianInt16(header->beChannels);
	if (channels != 4) {
		LOG_ERROR("Expected 4 color channels (RGBA), found %d\n", channels);
		return false;
	}
	int16 depth = ReadBigEndianInt16(header->beDepth);
	if (depth != 8) {
		LOG_ERROR("Unsupported color depth (not 8): %d\n", depth);
		return false;
	}
	int16 colorMode = ReadBigEndianInt16(header->beColorMode);
	if (colorMode != PSD_COLOR_MODE_RGB) {
		LOG_ERROR("Unsupported PSD color mode (non-RGB): %d\n", colorMode);
		return false;
	}

	int32 width = ReadBigEndianInt32(header->beWidth);
	int32 height = ReadBigEndianInt32(header->beHeight);

	LOG_INFO("PSD header %d %d %d %d (%d x %d)\n",
		version, channels, depth, colorMode, width, height);

	// TODO unsafe file data traversal yada yada, have to check lengths against psdFile.size
	const uint8* colorModeDataSection = psdData + sizeof(PSDHeader);
	int32 colorModeDataLength = ReadBigEndianInt32(colorModeDataSection);
	// TODO nothing here for now

	const uint8* imageResourcesSection = colorModeDataSection + 4 + colorModeDataLength;
	int32 imageResourcesLength = ReadBigEndianInt32(imageResourcesSection);
	LOG_INFO("img resources length %d\n", imageResourcesLength);

	const uint8* layerAndMaskInfoSection = imageResourcesSection + 4 + imageResourcesLength;
	int32 layerAndMaskInfoLength = ReadBigEndianInt32(layerAndMaskInfoSection);
	LOG_INFO("layer and mask info length %d\n", layerAndMaskInfoLength);
	FixedArray<LayerInfo, PSD_MAX_LAYERS> layers;
	layers.Init();
	layers.array.size = 0;
	if (layerAndMaskInfoLength > 0) {
		const uint8* layerInfoData = layerAndMaskInfoSection + 4;
		int32 layerInfoLength = ReadBigEndianInt32(layerInfoData);
		int16 layerCount = ReadBigEndianInt16(layerInfoData + 4);
		if (layerCount < 0) {
			// TODO spec says 1st alpha channel contains transparency data for merged result
			layerCount = -layerCount;
		}
		if (layerCount > PSD_MAX_LAYERS) {
			LOG_ERROR("PSD too many layers: %d, max %d\n", layerCount, PSD_MAX_LAYERS);
			return false;
		}
		layers.array.size = layerCount;
		LOG_INFO("layer info length %d, total %d layers\n", layerInfoLength, layerCount);

		const uint8* layerRecord = layerInfoData + 4 + 2;
		for (int16 l = 0; l < layerCount; l++) {
			LayerInfo& layerInfo = layers[l];
			const uint8* dataPtr = layerRecord;

			int32 top = ReadBigEndianInt32(dataPtr);
			int32 left = ReadBigEndianInt32(dataPtr + 4);
			int32 bottom = ReadBigEndianInt32(dataPtr + 4 + 4);
			int32 right = ReadBigEndianInt32(dataPtr + 4 + 4 + 4);
			dataPtr += 4 * 4;
			layerInfo.boundsMin.x = left;
			layerInfo.boundsMin.y = bottom;
			layerInfo.boundsMax.x = right;
			layerInfo.boundsMax.y = top;

			int16 numChannels = ReadBigEndianInt16(dataPtr);
			dataPtr += 2;
			layerInfo.channels = numChannels;

			// TODO maybe look at this data
			dataPtr += 6 * numChannels;

			uint8 signature[4];
			MemCopy(signature, dataPtr, 4);
			if (signature[0] != '8' || signature[1] != 'B'
			|| signature[2] != 'I' || signature[3] != 'M') {
				LOG_ERROR("Blend mode signature invalid (not 8BIM): %c%c%c%c\n",
					signature[0], signature[1], signature[2], signature[3]);
				return false;
			}
			dataPtr += 4;

			uint8 blendModeKey[4];
			MemCopy(blendModeKey, dataPtr, 4);
			dataPtr += 4;
			if (StringCompare((const char*)blendModeKey, "norm", 4)) {
				layerInfo.blendMode = LAYER_BLEND_MODE_NORMAL;
			}
			else if (StringCompare((const char*)blendModeKey, "mul", 3)) {
				layerInfo.blendMode = LAYER_BLEND_MODE_MULTIPLY;
			}
			else {
				LOG_ERROR("Unsupported blend mode %c%c%c%c\n",
					blendModeKey[0], blendModeKey[1], blendModeKey[2], blendModeKey[3]);
				return false;
			}

			uint8 opacity = *dataPtr++;
			layerInfo.opacity = opacity;
			dataPtr++; // uint8 clipping = *dataPtr++;
			uint8 flags = *dataPtr++;
			layerInfo.visible = (flags & 0x2) != 0;
			dataPtr++; // filler

			int32 extraDataLength = ReadBigEndianInt32(dataPtr);
			dataPtr += 4;
			const uint8* extraDataPtr = dataPtr;

			int32 layerMaskDataLength = ReadBigEndianInt32(dataPtr);
			dataPtr += 4;
			// TODO maybe look at this data
			dataPtr += layerMaskDataLength;

			int32 layerBlendingRangesLength = ReadBigEndianInt32(dataPtr);
			dataPtr += 4;
			// TODO maybe look at this data
			dataPtr += layerBlendingRangesLength;

			uint8 nameLength = *dataPtr++;
			layerInfo.name.Init();
			MemCopy(layerInfo.name.array.data, dataPtr, nameLength);
			layerInfo.name.array.size = nameLength;

			layerRecord = extraDataPtr + extraDataLength;
		}

		const uint8* layerChannelImageData = layerRecord;
		for (int16 l = 0; l < layerCount; l++) {
			const uint8* dataPtr = layerChannelImageData;
			int16 compression = ReadBigEndianInt16(dataPtr);
			dataPtr += 2;

			LOG_INFO("layer %.*s compression %d\n",
				layers[l].name.array.size, layers[l].name.array.data, compression);
		}

		const uint8* globalLayerMaskInfo = layerInfoData + 4 + layerInfoLength;
		int32 globalLayerMaskLength = ReadBigEndianInt32(globalLayerMaskInfo);
		LOG_INFO("global layer mask length %d\n", globalLayerMaskLength);
		// TODO nothing here for now

		const uint8* additionalLayerInfo = globalLayerMaskInfo + 4 + globalLayerMaskLength;
		uint8 signature[4];
		MemCopy(signature, additionalLayerInfo, 4);
		if ((signature[0] != '8' || signature[1] != 'B'
		|| signature[2] != 'I' || signature[3] != 'M') &&
		(signature[0] != '8' || signature[1] != 'B'
		|| signature[2] != '6' || signature[3] != '4')) {
			LOG_ERROR("Additional layer info signature invalid (not 8BIM or 8B64): %c%c%c%c\n",
				signature[0], signature[1], signature[2], signature[3]);
			return false;
		}
		uint8 key[4];
		MemCopy(key, additionalLayerInfo + 4, 4);
		LOG_INFO("additional layer info key: %c%c%c%c\n", key[0], key[1], key[2], key[3]);
		int32 additionalLayerInfoLength = ReadBigEndianInt32(additionalLayerInfo + 4 + 4);
		LOG_INFO("additional layer info length %d\n", additionalLayerInfoLength);
	}

	const uint8* imageDataSection = layerAndMaskInfoSection + 4 + layerAndMaskInfoLength;
	uint64 imageDataLength = psdFile.size - (uint64)(imageDataSection - psdData);
	LOG_INFO("image data length %zu\n", imageDataLength);

	return true;
}