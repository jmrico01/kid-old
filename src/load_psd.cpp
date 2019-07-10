#include "load_psd.h"

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

uint16 ReadBigEndianUInt16(const uint8 bigEndian[2])
{
    return ((uint16)bigEndian[0] << 8) + bigEndian[1];
}

uint32 ReadBigEndianUInt32(const uint8 bigEndian[4])
{
    return ((uint32)bigEndian[0] << 24) + (bigEndian[1] << 16) + (bigEndian[2] << 8) + bigEndian[3];
}

bool32 LoadPSD(const ThreadContext* thread, const char* filePath,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
    DEBUGReadFileResult psdFile = DEBUGPlatformReadFile(thread, filePath);
    if (!psdFile.data) {
        LOG_ERROR("Failed to open PSD file at: %s\n", filePath);
        return false;
    }

    const PSDHeader* header = (const PSDHeader*)psdFile.data;
    if (header->signature[0] != '8' || header->signature[1] != 'B'
    || header->signature[2] != 'P' || header->signature[3] != 'S') {
        LOG_ERROR("Invalid PSD signature (8BPS) on file %s\n", filePath);
        return false;
    }
    uint16 version = ReadBigEndianUInt16(header->beVersion);
    uint16 channels = ReadBigEndianUInt16(header->beChannels);
    uint16 depth = ReadBigEndianUInt16(header->beDepth);
    uint16 colorMode = ReadBigEndianUInt16(header->beColorMode);

    uint32 width = ReadBigEndianUInt32(header->beWidth);
    uint32 height = ReadBigEndianUInt32(header->beHeight);

    LOG_INFO("%d %d %d %d (%d x %d)\n", version, channels, depth, colorMode, width, height);

    return true;
}