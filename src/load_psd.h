#pragma once

#include "load_png.h"

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
	double timelineStart;
	double timelineDuration;
	uint64 dataStart;
};

struct PsdFile
{
	Vec2Int size;
	FixedArray<PsdLayerInfo, PSD_MAX_LAYERS> layers;
	PlatformReadFileResult file;

	template <typename Allocator>
	bool LoadLayerImageData(uint64 layerIndex, Allocator* allocator, LayerChannelID channel,
		ImageData* outImageData);
	template <typename Allocator>
	bool LoadLayerTextureGL(uint64 layerIndex, Allocator* allocator, LayerChannelID channel,
		GLint magFilter, GLint minFilter, GLint wrapS, GLint wrapT, TextureGL* outTextureGL);
};

template <typename Allocator>
bool OpenPSD(const ThreadContext* thread, Allocator* allocator,
	const char* filePath, PsdFile* outPsdFile);

template <typename Allocator>
void ClosePSD(const ThreadContext* thread, Allocator* allocator, PsdFile* psdFile);
