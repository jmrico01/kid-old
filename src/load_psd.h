#pragma once

bool32 LoadPSD(const ThreadContext* thread, const char* filePath, MemoryBlock* transient,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory);
