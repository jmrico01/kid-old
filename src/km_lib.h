#pragma once

#include "km_debug.h"

template <typename T>
struct DynamicArray
{
    uint32 size;
#if GAME_SLOW
    uint32 capacity = 0;
#else
    uint32 capacity;
#endif
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

    inline T& operator[](int index) const {
#if GAME_SLOW
        DEBUG_ASSERT(0 <= index && index < (int)size);
#endif
        return data[index];
    }
};

void MemCopy(void* dst, const void* src, uint64 numBytes);
void MemMove(void* dst, const void* src, uint64 numBytes);

/*
template <typename K, typename V>
struct HashNode
{
    K key;
    V value;
    HashNode* next;
}

template <typename K>
struct KeyHash
{
    unsigned long operator()(const K& key) const;
}

template <typename K, typename V, typename F = KeyHash<K>>
struct HashMap
{
    uint32 size;
    HashNode<K, V>* table;
}
*/