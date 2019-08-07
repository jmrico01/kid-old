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
    uint8 flags;
};

struct PsdData
{
    Vec2Int size;
    FixedArray<LayerInfo, PSD_MAX_LAYERS> layers;
};

bool32 LoadPSD(const ThreadContext* thread, const char* filePath,
    GLint magFilter, GLint minFilter, GLint wrapS, GLint wrapT,
    MemoryBlock* transient, PsdData* outPsdData,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);
