#pragma once

#include "km_debug.h"

#define STRING_KEY_MAX_LENGTH 32

template <typename T>
struct Array
{
	uint64 size;
	T* data;

	void Append(const T& element);
	void RemoveLast();

	// Slow, linear time
	void Remove(uint64 index);
	void AppendAfter(const T& element, uint64 index);
	
	inline T operator[](int index) const;
	inline T operator[](uint64 index) const;
	inline T& operator[](int index);
	inline T& operator[](uint64 index);
};

template <typename T, uint64 S>
struct FixedArray
{
	T fixedArray[S];
	Array<T> array;

	void Init(); // TODO ew, shouldn't need this

	void Append(const T& element);
	void RemoveLast();

	// Slow, linear time
	void Remove(uint64 index);
	void AppendAfter(const T& element, uint64 index);
	
	inline T operator[](int index) const;
	inline T operator[](uint64 index) const;
	inline T& operator[](int index);
	inline T& operator[](uint64 index);
};

template <typename T>
struct DynamicArray
{
	uint64 capacity;
	Array<T> array;

	void Allocate();
	void Allocate(uint64 capacity);
	void Free();

	void Append(const T& element);
	void RemoveLast();

	// Slow, linear time
	void Remove(uint64 index);
	void AppendAfter(const T& element, uint64 index);
	
	inline T operator[](int index) const;
	inline T operator[](uint64 index) const;
	inline T& operator[](int index);
	inline T& operator[](uint64 index);
};

struct HashKey
{
	char string[STRING_KEY_MAX_LENGTH];
	int length;

	void WriteString(const Array<char>& str);
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
	uint32 size;
	uint32 capacity;

	KeyValuePair<V>* pairs;

	void Init();
	void Init(int capacity);

	void Add(const HashKey& key, V value);
	V* GetValue(const HashKey& key) const; // const... sure, if it helps you sleep at night
	bool32 Remove(const HashKey& key);

	void Clear();
	void Free();

private:
	KeyValuePair<V>* GetPair(const HashKey& key) const;
	KeyValuePair<V>* GetFreeSlot(const HashKey& key);
};

bool32 KeyCompare(const HashKey& key1, const HashKey& key2);

void MemCopy(void* dst, const void* src, uint64 numBytes);
void MemMove(void* dst, const void* src, uint64 numBytes);
