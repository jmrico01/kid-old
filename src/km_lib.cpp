#include "km_lib.h"

#include <stdio.h>
#include <cstring> // memcpy is here... no idea why

#define DYNAMIC_ARRAY_START_CAPACITY 10

#define HASH_TABLE_START_CAPACITY 17

template <typename T>
void DynamicArray<T>::Init()
{
	Init(DYNAMIC_ARRAY_START_CAPACITY);
}

template <typename T>
void DynamicArray<T>::Init(uint32 capacity)
{
	size = 0;
	this->capacity = capacity;
	data = (T*)malloc(sizeof(T) * capacity);
	if (!data) {
		DEBUG_PANIC("ERROR: not enough memory!\n");
	}
}

template <typename T>
DynamicArray<T> DynamicArray<T>::Copy() const
{
	DynamicArray<T> array(capacity);

	array.size = size;
	for (uint32 i = 0; i < array.size; i++) {
		array.data[i] = data[i];
	}

	return array;
}

template <typename T>
void DynamicArray<T>::Append(T element)
{
#if GAME_SLOW
	DEBUG_ASSERT(capacity > 0);
#endif

	if (size >= capacity) {
		capacity *= 2;
		data = (T*)realloc(data, sizeof(T) * capacity);
		if (!data) {
			DEBUG_PANIC("ERROR: not enough memory!\n");
		}
	}
	data[size++] = element;
}

template <typename T>
void DynamicArray<T>::RemoveLast()
{
	size--;
}

template <typename T>
void DynamicArray<T>::Remove(uint32 index)
{
	DEBUG_ASSERT(index < size);

	for (uint32 i = index + 1; i < size; i++) {
		data[i - 1] = data[i];
	}
	size--;
}

template <typename T>
void DynamicArray<T>::Clear()
{
	size = 0;
}

template <typename T>
void DynamicArray<T>::Free()
{
	free(data);
}

template <typename T>
inline T& DynamicArray<T>::operator[](int index) const
{
#if GAME_SLOW
	DEBUG_ASSERT(0 <= index && index < (int)size);
#endif
	return data[index];
}

template <typename K, typename V>
void HashTable<K, V>::Init()
{
	Init(HASH_TABLE_START_CAPACITY);
}

template <typename K, typename V>
void HashTable<K, V>::Init(uint32 capacity)
{
	size = 0;
	this->capacity = capacity;
	kvPairs = (KeyValuePair*)malloc(sizeof(KeyValuePair) * capacity);
	if (!kvPairs) {
		DEBUG_PANIC("ERROR: not enough memory!\n");
	}
}

template <typename K, typename V>
void HashTable<K, V>::Add(K key, V value)
{
}

void MemCopy(void* dst, const void* src, uint64 numBytes)
{
	DEBUG_ASSERT(((const char*)dst + numBytes <= (const char*)src)
		|| (dst >= (const char*)src + numBytes));
	// TODO maybe see about reimplementing this? would be informative
	memcpy(dst, src, numBytes);
}

void MemMove(void* dst, const void* src, uint64 numBytes)
{
	memmove(dst, src, numBytes);
}