#pragma once

#include "km_debug.h"

template <typename T>
struct DynamicArray
{
	uint32 size;
	uint32 capacity;
	T* data;

	void Init();
	void Init(uint32 cap);

	DynamicArray<T> Copy() const;
	void Append(T element);
	void RemoveLast();
	// Slow, linear time
	void Remove(uint32 idx);
	void Clear();
	void Free();

	inline T& operator[](int index) const;
};

void MemCopy(void* dst, const void* src, uint64 numBytes);
void MemMove(void* dst, const void* src, uint64 numBytes);
