#include "km_lib.h"

#include <stdio.h>
#include <cstring> // memcpy is here... no idea why

#define DYNAMIC_ARRAY_START_CAPACITY 10

template <typename T>
void DynamicArray<T>::Init()
{
    Init(DYNAMIC_ARRAY_START_CAPACITY);
}

template <typename T>
void DynamicArray<T>::Init(uint32 cap)
{
    size = 0;
    this->capacity = cap;
    data = (T*)malloc(sizeof(T) * cap);
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
void DynamicArray<T>::Remove(uint32 idx)
{
    DEBUG_ASSERT(idx < size);

    for (uint32 i = idx + 1; i < size; i++) {
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