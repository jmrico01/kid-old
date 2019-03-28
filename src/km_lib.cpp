#include "km_lib.h"

#include <stdio.h>
#include <cstring> // memcpy is here... no idea why

#include "km_string.h"

#define DYNAMIC_ARRAY_START_CAPACITY 10

#define HASH_TABLE_START_CAPACITY 17
#define HASH_TABLE_MAX_SIZE_TO_CAPACITY 0.7

// Very simple string hash ( djb2 hash, source http://www.cse.yorku.ca/~oz/hash.html )
uint32 KeyHash(const HashKey& key)
{
    uint32 hash = 5381;

    for (int i = 0; i < key.length; i++) {
        hash = ((hash << 5) + hash) + key.string[i];
    }

    return hash;
}
bool32 KeyCompare(const HashKey& key1, const HashKey& key2)
{
    if (key1.length != key2.length) {
        return false;
    }

    for (int i = 0; i < key1.length; i++) {
        if (key1.string[i] != key2.string[i]) {
            return false;
        }
    }

    return true;
}

template <typename T, uint32 S>
void FixedArray<T, S>::Append(T element)
{
    DEBUG_ASSERT(size < S);
    array[size++] = T;
}

template <typename T, uint32 S>
inline T& FixedArray<T, S>::operator[](int index) const
{
    DEBUG_ASSERT(index > 0 && index < size);
    return array[index];
}

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
	DEBUG_ASSERT(capacity > 0);

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

    capacity = 0;
    size = 0;
}

template <typename T>
inline T& DynamicArray<T>::operator[](int index) const
{
#if GAME_SLOW
	DEBUG_ASSERT(0 <= index && index < (int)size);
#endif
	return data[index];
}

void HashKey::WriteString(const char* str, int n)
{
    DEBUG_ASSERT(n <= STRING_KEY_MAX_LENGTH);

    for (int i = 0; i < n; i++) {
        string[i] = str[i];
    }

    length = n;
}

void HashKey::WriteString(const char* str)
{
    WriteString(str, StringLength(str));
}

template <typename V>
void HashTable<V>::Init()
{
	Init(HASH_TABLE_START_CAPACITY);
}

template <typename V>
void HashTable<V>::Init(int cap)
{
	size = 0;
	this->capacity = cap;
	pairs = (KeyValuePair<V>*)malloc(sizeof(KeyValuePair<V>) * cap);
	if (!pairs) {
		DEBUG_PANIC("ERROR: not enough memory!\n");
	}

    for (int i = 0; i < cap; i++) {
        pairs[i].key.length = 0;
    }
}

template <typename V>
void HashTable<V>::Add(const HashKey& key, V value)
{
    DEBUG_ASSERT(GetPair(key) == nullptr);

    if (size >= (uint32)((float32)capacity * HASH_TABLE_MAX_SIZE_TO_CAPACITY)) {
        uint32 newCapacity = NextPrime(capacity * 2);
        pairs = (KeyValuePair<V>*)realloc(pairs, sizeof(KeyValuePair<V>) * newCapacity);
        if (!pairs) {
            DEBUG_PANIC("ERROR: not enough memory!\n");
        }

        // TODO rehash etc
        capacity = newCapacity;
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
    uint32 hashInd = KeyHash(key) % capacity;
    for (uint32 i = 0; i < capacity; i++) {
        KeyValuePair<V>* pair = pairs + hashInd + i;
        if (KeyCompare(pair->key, key)) {
            return pair;
        }
        if (pair->key.length == 0) {
            return nullptr;
        }
    }

    return nullptr;
}

template <typename V>
KeyValuePair<V>* HashTable<V>::GetFreeSlot(const HashKey& key)
{
    uint32 hashInd = KeyHash(key) % capacity;
    for (uint32 i = 0; i < capacity; i++) {
        KeyValuePair<V>* pair = pairs + hashInd + i;
        if (pair->key.length == 0) {
            return pair;
        }
    }

    return nullptr;
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