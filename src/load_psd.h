#pragma once

#include "load_png.h"

#define PSD_CHANNELS 4
#define PSD_MAX_LAYERS 64
#define PSD_LAYER_NAME_MAX_LENGTH 256

enum LayerBlendMode
{
    LAYER_BLEND_MODE_NORMAL,
    LAYER_BLEND_MODE_MULTIPLY
};

enum LayerChannelID
{
    LAYER_CHANNEL_RED = 0,
    LAYER_CHANNEL_GREEN,
    LAYER_CHANNEL_BLUE,
    LAYER_CHANNEL_ALPHA
};

struct LayerChannelInfo
{
    LayerChannelID channelID;
    uint32 dataSize;
};

struct LayerInfo
{
    TextureGL textureGL;
    FixedArray<char, PSD_LAYER_NAME_MAX_LENGTH> name;
    int left, right, top, bottom;
    FixedArray<LayerChannelInfo, PSD_CHANNELS> channels;
    uint8 opacity;
    LayerBlendMode blendMode;
    bool visible;
};

struct PsdData
{
    Vec2Int size;
    FixedArray<LayerInfo, PSD_MAX_LAYERS> layers;
};

template <typename Allocator>
bool LoadPSD(const ThreadContext* thread, Allocator* allocator, const char* filePath,
    GLint magFilter, GLint minFilter, GLint wrapS, GLint wrapT, PsdData* outPsdData);

void UnloadPSDOpenGL(const PsdData& psdData);
