#pragma once

#include "km_debug.h"

#define STRING_KEY_MAX_LENGTH 32

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

struct HashKey
{
    char string[STRING_KEY_MAX_LENGTH];
    int length;

    void WriteString(const char* str, int n);
    void WriteString(const char* str);
};

template <typename V>
struct KeyValuePair
{
    HashKey key;
    V value;
};

template <typename V>
struct HashTable
{
	int size;
	int capacity;

    KeyValuePair<V>* pairs;

	void Init();
	void Init(int capacity);

	void Add(const HashKey& key, V value);
    V* GetValue(const HashKey& key) const; // const... sure, if it helps you sleep at night
	bool32 Remove(const HashKey& key);

private:
    KeyValuePair<V>* GetPair(const HashKey& key) const;
    KeyValuePair<V>* GetFreeSlot(const HashKey& key);
};

bool32 KeyCompare(const HashKey& key1, const HashKey& key2);

void MemCopy(void* dst, const void* src, uint64 numBytes);
void MemMove(void* dst, const void* src, uint64 numBytes);
