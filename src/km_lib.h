#pragma once

#include "km_debug.h"

template <typename T>
struct DynamicArray
{
	uint32 size;
	uint32 capacity;
	T* data;

	void Init();
	void Init(uint32 capacity);

	DynamicArray<T> Copy() const;
	void Append(T element);
	void RemoveLast();
	// Slow, linear time
	void Remove(uint32 index);
	void Clear();
	void Free();

	inline T& operator[](int index) const;
};

template <typename K, typename V>
struct HashTable
{
	struct KeyValuePair
	{
		K key;
		V value;
	};

	uint32 size;
	uint32 capacity;

	KeyValuePair* kvPairs;

	void Init();
	void Init(uint32 capacity);

	void Add(K key, V value);
	V Get(K key);
	bool32 Remove(K key);
};

void MemCopy(void* dst, const void* src, uint64 numBytes);
void MemMove(void* dst, const void* src, uint64 numBytes);
