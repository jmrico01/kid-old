#include "load_psd.h"

#include <km_common/km_string.h>

#define PSD_COLOR_MODE_RGB 3

enum PsdImageDataFormat
{
	PSD_IMAGE_DATA_RAW      = 0,
	PSD_IMAGE_DATA_PACKBITS = 1
};

internal int16 ReadBigEndianInt16(const uint8 bigEndian[2])
{
	return ((int16)bigEndian[0] << 8) + bigEndian[1];
}

internal int32 ReadBigEndianInt32(const uint8 bigEndian[4])
{
	return ((int32)bigEndian[0] << 24) + (bigEndian[1] << 16) + (bigEndian[2] << 8) + bigEndian[3];
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
	if (channel != LAYER_CHANNEL_ALL) {
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

	const uint8* psdData = (uint8*)file.data;
	uint64 psdDataIndex = layerInfo.dataStart;

	for (uint8 c = 0; c < layerInfo.channels.array.size; c++) {
		const LayerChannelInfo& layerChannelInfo = layerInfo.channels[c];
		int channelOffsetDest = (int)layerChannelInfo.channelID;
		if (channel != LAYER_CHANNEL_ALL) {
			if (layerChannelInfo.channelID != channel) {
				psdDataIndex += layerChannelInfo.dataSize;
				continue;
			}
			channelOffsetDest = 0;
		}

		int16 compression = ReadBigEndianInt16(&psdData[psdDataIndex]);
		const uint8* layerImageData = &psdData[psdDataIndex + 2];
		psdDataIndex += layerChannelInfo.dataSize;

		if (compression == PSD_IMAGE_DATA_RAW) {
			ReadRawData(layerImageData, layerWidth, layerHeight,
				numChannelsDest, channelOffsetDest, layerData);
		}
		else if (compression == PSD_IMAGE_DATA_PACKBITS) {
			if (!ReadPackBitsData(layerImageData, allocator, layerWidth, layerHeight,
			numChannelsDest, channelOffsetDest, layerData)) {
				LOG_ERROR("Failed to read PackBits data\n");
				return false;
			}
		}
		else {
			LOG_ERROR("Unhandled layer compression %d\n", compression);
			return false;
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
	const uint8* psdData = (uint8*)outPsdFile->file.data;
	uint64 parsedBytes = 0;

	uint8 signature[4];
	MemCopy(signature, &psdData[parsedBytes], 4);
	parsedBytes += 4;
	if (signature[0] != '8' || signature[1] != 'B'
	|| signature[2] != 'P' || signature[3] != 'S') {
		LOG_ERROR("Invalid PSD signature (8BPS) on file %s\n", filePath);
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
	// TODO nothing here for now
	parsedBytes += imageResourcesLength;

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
				if (channelID == 0) {
					layerChannelInfo.channelID = LAYER_CHANNEL_RED;
				}
				else if (channelID == 1) {
					layerChannelInfo.channelID = LAYER_CHANNEL_GREEN;
				}
				else if (channelID == 2) {
					layerChannelInfo.channelID = LAYER_CHANNEL_BLUE;
				}
				else if (channelID == -1) {
					layerChannelInfo.channelID = LAYER_CHANNEL_ALPHA;
				}
				else {
					LOG_ERROR("Unsupported layer channel ID %d\n", channelID);
					return false;
				}
				int32 channelDataSize = ReadBigEndianInt32(&psdData[parsedBytes]);
				parsedBytes += 4;
				layerChannelInfo.dataSize = channelDataSize;
			}

			uint8 blendModeSignature[4];
			MemCopy(blendModeSignature, &psdData[parsedBytes], 4);
			parsedBytes += 4;
			if (blendModeSignature[0] != '8' || blendModeSignature[1] != 'B'
			|| blendModeSignature[2] != 'I' || blendModeSignature[3] != 'M') {
				LOG_ERROR("Blend mode signature invalid (not 8BIM): %c%c%c%c\n",
					blendModeSignature[0], blendModeSignature[1],
					blendModeSignature[2], blendModeSignature[3]);
				return false;
			}

			uint8 blendModeKey[4];
			MemCopy(blendModeKey, &psdData[parsedBytes], 4);
			parsedBytes += 4;
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

			uint8 nameLength = psdData[parsedBytes++];
			MemCopy(layerInfo.name.array.data, &psdData[parsedBytes], nameLength);
			layerInfo.name.array.size = nameLength;

			parsedBytes = extraDataStart + extraDataLength;
		}

		for (int16 l = 0; l < layerCount; l++) {
			PsdLayerInfo& layerInfo = outPsdFile->layers[l];
			layerInfo.dataStart = parsedBytes;
			for (uint64 c = 0; c < layerInfo.channels.array.size; c++) {
				parsedBytes += layerInfo.channels[c].dataSize;
			}
		}

		parsedBytes = layerInfoStart + layerInfoLength;

		int32 globalLayerMaskLength = ReadBigEndianInt32(&psdData[parsedBytes]);
		parsedBytes += 4;
		// TODO nothing here for now
		parsedBytes += globalLayerMaskLength;

		uint8 additionalLayerInfoSignature[4];
		MemCopy(additionalLayerInfoSignature, &psdData[parsedBytes], 4);
		parsedBytes += 4;
		if ((additionalLayerInfoSignature[0] != '8' || additionalLayerInfoSignature[1] != 'B'
		|| additionalLayerInfoSignature[2] != 'I' || additionalLayerInfoSignature[3] != 'M') &&
		(additionalLayerInfoSignature[0] != '8' || additionalLayerInfoSignature[1] != 'B'
		|| additionalLayerInfoSignature[2] != '6' || additionalLayerInfoSignature[3] != '4')) {
			LOG_ERROR("Additional layer info signature invalid (not 8BIM or 8B64): %c%c%c%c\n",
				additionalLayerInfoSignature[0], additionalLayerInfoSignature[1],
				additionalLayerInfoSignature[2], additionalLayerInfoSignature[3]);
			return false;
		}
		uint8 key[4];
		MemCopy(key, &psdData[parsedBytes], 4);
		parsedBytes += 4;

		int32 additionalLayerInfoLength = ReadBigEndianInt32(&psdData[parsedBytes]);
		parsedBytes += 4;
		// TODO nothing here for now
		parsedBytes += additionalLayerInfoLength;
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