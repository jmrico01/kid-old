#include "load_psd.h"

#include <km_common/km_string.h>

#define PSD_COLOR_MODE_RGB 3

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
bool32 LoadPSD(const ThreadContext* thread, const char* filePath,
	GLint magFilter, GLint minFilter, GLint wrapS, GLint wrapT,
	MemoryBlock* transient, PsdData* outPsdData,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
	DEBUGReadFileResult psdFile = DEBUGPlatformReadFile(thread, filePath);
	if (!psdFile.data) {
		LOG_ERROR("Failed to open PSD file at: %s\n", filePath);
		return false;
	}
	const uint8* psdData = (uint8*)psdFile.data;
	uint64 parsedBytes = 0;

	//const PSDHeader* header = (const PSDHeader*)psdData;
	//parsedBytes += sizeof(PSDHeader);
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
		LOG_ERROR("Expected %d color channels (RGBA), found %d\n", PSD_CHANNELS, channels);
		return false;
	}

	int32 height = ReadBigEndianInt32(&psdData[parsedBytes]);
	parsedBytes += 4;
	int32 width = ReadBigEndianInt32(&psdData[parsedBytes]);
	parsedBytes += 4;
	outPsdData->size.x = width;
	outPsdData->size.y = height;

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
	outPsdData->layers.Init();
	outPsdData->layers.array.size = 0;
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
		outPsdData->layers.array.size = layerCount;

		for (int16 l = 0; l < layerCount; l++) {
			LayerInfo& layerInfo = outPsdData->layers[l];
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
			layerInfo.boundsMin.x = left;
			layerInfo.boundsMin.y = height - bottom;
			layerInfo.boundsMax.x = right;
			layerInfo.boundsMax.y = height - top;

			int16 layerChannels = ReadBigEndianInt16(&psdData[parsedBytes]);
			parsedBytes += 2;
			if (layerChannels != PSD_CHANNELS) {
				LOG_ERROR("Layer expected %d color channels (RGBA), found %d\n",
					PSD_CHANNELS, channels);
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
			layerInfo.flags = flags;
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
			// TODO something about layer data being odd and pad byte at the end of row
			LayerInfo& layerInfo = outPsdData->layers[l];
			int layerWidth = layerInfo.boundsMax.x - layerInfo.boundsMin.x;
			int layerHeight = layerInfo.boundsMax.y - layerInfo.boundsMin.y;
			uint64 layerChannels = layerInfo.channels.array.size;
			if (transient->size < layerHeight * sizeof(int16)
			+ layerWidth * layerHeight * layerChannels) {
				LOG_ERROR("Not enough memory to read layer data\n");
				return false;
			}
			uint16* layerRowLengths = (uint16*)transient->memory;
			uint8* layerDataGL = (uint8*)transient->memory + layerHeight * sizeof(int16);
			// TODO is this necessary?
			MemSet(layerDataGL, 0, layerWidth * layerHeight * layerChannels);

			for (int16 c = 0; c < layerChannels; c++) {
				LayerChannelInfo& layerChannelInfo = layerInfo.channels[c];
				int channelOffset = (int)layerChannelInfo.channelID;
				int16 compression = ReadBigEndianInt16(&psdData[parsedBytes]);
				parsedBytes += 2;
				if (compression != 1 && layerHeight > 0) {
					LOG_ERROR("Unhandled layer compression %d\n", compression);
					return false;
				}

				for (int r = 0; r < layerHeight; r++) {
					int16 rowLengthBytes = ReadBigEndianInt16(&psdData[parsedBytes]);
					parsedBytes += 2;
					layerRowLengths[r] = rowLengthBytes;
				}

				for (int r = 0; r < layerHeight; r++) {
					// Parse data in PackBits format
					// https://en.wikipedia.org/wiki/PackBits
					int16 parsedData = 0;
					uint64 pixelY = layerHeight - r - 1;
					uint64 pixelX = 0;
					while (true) {
						int8 header = psdData[parsedBytes + parsedData];
						if (++parsedData >= layerRowLengths[r]) {
							break;
						}
						if (header == -128) {
							continue;
						}
						else if (header < 0) {
							uint8 data = psdData[parsedBytes + parsedData];
							int repeats = 1 - header;
							for (int i = 0; i < repeats; i++) {
								uint64 ind = (pixelY * layerWidth + pixelX) * layerChannels
									+ channelOffset;
								layerDataGL[ind] = data;
								pixelX++;
							}
							if (++parsedData >= layerRowLengths[r]) {
								break;
							}
						}
						else if (header >= 0) {
							int dataLength = 1 + header;
							for (int i = 0; i < dataLength; i++) {
								uint8 data = psdData[parsedBytes + parsedData];
								uint64 ind = (pixelY * layerWidth + pixelX) * layerChannels
									+ channelOffset;
								layerDataGL[ind] = data;
								pixelX++;
								if (++parsedData >= layerRowLengths[r]) {
									if (i < dataLength - 1) {
										LOG_ERROR("data parse unexpected end %d, %d\n",
											parsedBytes, parsedData);
										return false;
									}
									break;
								}
							}
						}
					}

					DEBUG_ASSERTF(pixelX == layerWidth, "row %d, bytes %d\n", r, layerRowLengths[r]);

					parsedBytes += layerRowLengths[r];
				}
			}

			if ((layerInfo.flags & 0x2) != 0) {
				LOG_INFO("Skipping GL texture for hidden layer %.*s\n",
					layerInfo.name.array.size, layerInfo.name.array.data);
				continue;
			}
			GLuint textureID;
			glGenTextures(1, &textureID);
			glBindTexture(GL_TEXTURE_2D, textureID);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, layerWidth, layerHeight,
				0, GL_RGBA, GL_UNSIGNED_BYTE, (const GLvoid*)layerDataGL);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);

			layerInfo.textureGL.textureID = textureID;
			layerInfo.textureGL.size = Vec2Int { layerWidth, layerHeight };
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

	/*const uint8* imageDataSection = layerAndMaskInfoSection + 4 + layerAndMaskInfoLength;
	uint64 imageDataLength = psdFile.size - (uint64)(imageDataSection - psdData);
	LOG_INFO("image data length %zu\n", imageDataLength);*/

	DEBUGPlatformFreeFileMemory(thread, &psdFile);

	return true;
}