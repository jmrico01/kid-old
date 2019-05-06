#include "km_lib.h"

#include <stdio.h>
#include <cstring> // memcpy is here... no idea why

#include "km_string.h"

#define DYNAMIC_ARRAY_START_CAPACITY 10

#define HASH_TABLE_START_CAPACITY 17
#define HASH_TABLE_MAX_SIZE_TO_CAPACITY 0.7

// Very simple string hash ( djb2 hash, source http://www.cse.yorku.ca/~oz/hash.html )
uint64 KeyHash(const HashKey& key)
{
	uint64 hash = 5381;

	for (uint64 i = 0; i < key.string.array.size; i++) {
		hash = ((hash << 5) + hash) + key.string[i];
	}

	return hash;
}
bool32 KeyCompare(const HashKey& key1, const HashKey& key2)
{
	if (key1.string.array.size != key2.string.array.size) {
		return false;
	}

	for (uint64 i = 0; i < key1.string.array.size; i++) {
		if (key1.string[i] != key2.string[i]) {
			return false;
		}
	}

	return true;
}

template <typename T>
void Array<T>::Append(const T& element)
{
	data[size++] = element;
}

template <typename T>
void Array<T>::RemoveLast()
{
	DEBUG_ASSERT(size > 0);
	size--;
}

template <typename T>
void Array<T>::Remove(uint64 index)
{
	DEBUG_ASSERT(size > 0);
	DEBUG_ASSERT(index < size);

	for (uint64 i = index + 1; i < size; i++) {
		data[i - 1] = data[i];
	}
	size--;
}

template <typename T>
void Array<T>::AppendAfter(const T& element, uint64 index)
{
	DEBUG_ASSERT(index < size);

	uint64 targetIndex = index + 1;
	for (uint64 i = size; i > targetIndex; i--) {
		data[i] = data[i - 1];
	}
	data[targetIndex] = element;
	size++;
}

template <typename T>
inline T Array<T>::operator[](int index) const
{
	DEBUG_ASSERT(0 <= index && (uint64)index < size);
	return data[index];
}

template <typename T>
inline T Array<T>::operator[](uint64 index) const
{
	DEBUG_ASSERT(0 <= index && index < size);
	return data[index];
}

template <typename T>
inline T& Array<T>::operator[](int index)
{
	DEBUG_ASSERT(0 <= index && (uint64)index < size);
	return data[index];
}

template <typename T>
inline T& Array<T>::operator[](uint64 index)
{
	DEBUG_ASSERT(0 <= index && index < size);
	return data[index];
}

template <typename T, uint64 S>
void FixedArray<T, S>::Init()
{
	array.data = fixedArray;
}

template <typename T, uint64 S>
void FixedArray<T, S>::Append(const T& element)
{
	DEBUG_ASSERT(array.size < S);
	array.Append(element);
}

template <typename T, uint64 S>
void FixedArray<T, S>::RemoveLast()
{
	array.RemoveLast()
}

template <typename T, uint64 S>
void FixedArray<T, S>::Remove(uint64 index)
{
	array.Remove(index);
}

template <typename T, uint64 S>
void FixedArray<T, S>::AppendAfter(const T& element, uint64 index)
{
	DEBUG_ASSERT(array.size < S);
	array.AppendAfter(element, index);
}

template <typename T, uint64 S>
inline T FixedArray<T, S>::operator[](int index) const
{
	return array[index];
}

template <typename T, uint64 S>
inline T FixedArray<T, S>::operator[](uint64 index) const
{
	return array[index];
}

template <typename T, uint64 S>
inline T& FixedArray<T, S>::operator[](int index)
{
	return array[index];
}

template <typename T, uint64 S>
inline T& FixedArray<T, S>::operator[](uint64 index)
{
	return array[index];
}

template <typename T, uint64 S>
inline void FixedArray<T, S>::operator=(const FixedArray<T, S>& other)
{
	array.size = other.array.size;
	array.data = fixedArray;
	for (uint64 i = 0; i < other.array.size; i++) {
		fixedArray[i] = other.fixedArray[i];
	}
}

template <typename T>
void DynamicArray<T>::Allocate()
{
	Allocate(DYNAMIC_ARRAY_START_CAPACITY);
}

template <typename T>
void DynamicArray<T>::Allocate(uint64 cap)
{
	this->capacity = capacity;
	array.data = (T*)malloc(sizeof(T) * capacity);
	if (!array.data) {
		DEBUG_PANIC("ERROR: not enough memory!\n");
	}
}

template <typename T>
void DynamicArray<T>::Free()
{
	free(data);

	capacity = 0;
	size = 0;
}

template <typename T>
void DynamicArray<T>::Append(const T& element)
{
	array.Append(element);
}

template <typename T>
void DynamicArray<T>::RemoveLast()
{
	array.RemoveLast();
}

template <typename T>
void DynamicArray<T>::Remove(uint64 index)
{
	array.Remove(index);
}

template <typename T>
inline T DynamicArray<T>::operator[](int index) const
{
	return array[index];
}

template <typename T>
inline T DynamicArray<T>::operator[](uint64 index) const
{
	return array[index];
}

template <typename T>
inline T& DynamicArray<T>::operator[](int index)
{
	return array[index];
}

template <typename T>
inline T& DynamicArray<T>::operator[](uint64 index)
{
	return array[index];
}

void HashKey::WriteString(const Array<char>& str)
{
	DEBUG_ASSERT(str.size <= STRING_KEY_MAX_LENGTH);
	MemCopy(string.fixedArray, str.data, str.size * sizeof(char));
	string.Init();
	string.array.size = str.size;
}

void HashKey::WriteString(const char* str)
{
	Array<char> stringArray;
	stringArray.data = (char*)str;
	stringArray.size = StringLength(str);
	WriteString(stringArray);
}

template <typename V>
void HashTable<V>::Init()
{
	Init(HASH_TABLE_START_CAPACITY);
}

template <typename V>
void HashTable<V>::Init(uint64 cap)
{
	size = 0;
	capacity = cap;
	pairs = (KeyValuePair<V>*)malloc(sizeof(KeyValuePair<V>) * cap);
	if (!pairs) {
		DEBUG_PANIC("ERROR: not enough memory!\n");
	}

	for (uint64 i = 0; i < cap; i++) {
		pairs[i].key.string.Init();
		pairs[i].key.string.array.size = 0;
	}
}

template <typename V>
void HashTable<V>::Add(const HashKey& key, V value)
{
	DEBUG_ASSERT(GetPair(key) == nullptr);

	if (size >= (uint64)((float32)capacity * HASH_TABLE_MAX_SIZE_TO_CAPACITY)) {
		uint64 newCapacity = NextPrime(capacity * 2);
		pairs = (KeyValuePair<V>*)realloc(pairs, sizeof(KeyValuePair<V>) * newCapacity);
		if (!pairs) {
			DEBUG_PANIC("ERROR: not enough memory!\n");
		}

		// TODO revisit this / just take in custom allocators already
		KeyValuePair<V>* oldPairs = (KeyValuePair<V>*)malloc(sizeof(KeyValuePair<V>) * capacity);
		MemCopy(oldPairs, pairs, sizeof(KeyValuePair<V>) * capacity);
		for (uint64 i = 0; i < capacity; i++) {
			// TODO bleh
		}
		capacity = newCapacity;
		free(oldPairs);
	}

	KeyValuePair<V>* pair = GetFreeSlot(key);
	DEBUG_ASSERT(pair != nullptr);

	pair->key = key;
	pair->value = value;
	size++;
}

template <typename V>
V* HashTable<V>::GetValue(const HashKey& key) const
{
	KeyValuePair<V>* pair = GetPair(key);
	if (pair == nullptr) {
		return nullptr;
	}

	return &pair->value;
}

template <typename V>
void HashTable<V>::Clear()
{
	for (int i = 0; i < capacity; i++) {
		pairs[i].key.length = 0;
	}
}

template <typename V>
void HashTable<V>::Free()
{
	free(pairs);

	capacity = 0;
	size = 0;
}

template <typename V>
KeyValuePair<V>* HashTable<V>::GetPair(const HashKey& key) const
{
	uint64 hashInd = KeyHash(key) % capacity;
	for (uint64 i = 0; i < capacity; i++) {
		KeyValuePair<V>* pair = pairs + hashInd + i;
		if (KeyCompare(pair->key, key)) {
			return pair;
		}
		if (pair->key.string.array.size == 0) {
			return nullptr;
		}
	}

	return nullptr;
}

template <typename V>
KeyValuePair<V>* HashTable<V>::GetFreeSlot(const HashKey& key)
{
	uint64 hashInd = KeyHash(key) % capacity;
	for (uint64 i = 0; i < capacity; i++) {
		KeyValuePair<V>* pair = pairs + hashInd + i;
		if (pair->key.string.array.size == 0) {
			return pair;
		}
	}

	return nullptr;
}

void MemCopy(void* dst, const void* src, uint64 numBytes)
{
	DEBUG_ASSERT(((const char*)dst + numBytes <= src)
		|| (dst >= (const char*)src + numBytes));
	// TODO maybe see about reimplementing this? would be informative
	memcpy(dst, src, numBytes);
}

void MemMove(void* dst, const void* src, uint64 numBytes)
{
	memmove(dst, src, numBytes);
}

void MemSet(void* dst, char value, uint64 numBytes)
{
	memset(dst, value, numBytes);
}