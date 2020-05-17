#pragma once

#include "asset_texture.h"

#include <km_common/km_lib.h>

#define PSD_CHANNELS 4
#define PSD_MAX_LAYERS 64
#define PSD_LAYER_NAME_MAX_LENGTH 256

enum class LayerBlendMode
{
	NORMAL,
	MULTIPLY
};

enum class LayerChannelID
{
	RED   = 0,
	GREEN = 1,
	BLUE  = 2,
	ALPHA = 3,

	ALL
};

struct LayerChannelInfo
{
	LayerChannelID channelID;
	uint32 dataSize;
};

struct ImageData
{
	Vec2Int size;
	uint8 channels;
	uint8* data;
};

struct PsdLayerInfo
{
	FixedArray<char, PSD_LAYER_NAME_MAX_LENGTH> name;
	int left, right, top, bottom;
	FixedArray<LayerChannelInfo, PSD_CHANNELS> channels;
	uint8 opacity;
	LayerBlendMode blendMode;
	bool visible;
	uint64 parentIndex;
	bool inTimeline;
	float32 timelineStart;
	float32 timelineDuration;
	uint64 dataStart;
};

struct PsdFile
{
	Vec2Int size;
	FixedArray<PsdLayerInfo, PSD_MAX_LAYERS> layers;
	Array<uint8> file;

	template <typename Allocator>
        bool LoadLayerImageData(uint64 layerIndex, LayerChannelID channel, Allocator* allocator,
                                ImageData* outImageData);
	template <typename Allocator>
        bool LoadLayerTextureGL(uint64 layerIndex, LayerChannelID channel, GLint magFilter,
                                GLint minFilter, GLint wrapS, GLint wrapT, Allocator* allocator, TextureGL* outTextureGL);

	// TODO temp?
	template <typename Allocator>
        bool LoadLayerAtPsdSizeTextureGL(uint64 layerIndex, LayerChannelID channel, GLint magFilter,
                                         GLint minFilter, GLint wrapS, GLint wrapT, Allocator* allocator, TextureGL* outTextureGL);
};

template <typename Allocator>
bool LoadPsd(PsdFile* psdFile, const_string filePath, Allocator* allocator);
template <typename Allocator>
void FreePsd(PsdFile* psdFile, Allocator* allocator);
