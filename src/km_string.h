#pragma once

#include "km_lib.h"
#include "km_math.h"

// TODO strings should probably be KMArray or FixedArray objects

int StringLength(const char* str);
bool StringCompare(const char* str1, const char* str2, int n);
void TrimWhitespace(const char* str, int n, const char** trimmedStr, int* trimmedN);
void CatStrings(
	size_t sourceACount, const char* sourceA,
	size_t sourceBCount, const char* sourceB,
	size_t destCount, char* dest);
inline bool32 IsWhitespace(char c);
bool32 StringToIntBase10(const char* string, int n, int* intBase10);
bool32 StringToFloat32(const char* string, int n, float32* f);
int GetLastOccurrence(const char* string, int n, char c);
bool32 ReadElementInSplitString(const char* string, int stringLength, char separator,
    int* elementLength, const char** next);

template <typename T>
bool32 StringToElementArray(const char* string, int length, char sep, bool trimElements,
    bool32 (*conversionFunction)(const char*, int, T*),
    int maxElements, T* array, int* numElements);

template <uint64 KEYWORD_SIZE, uint64 VALUE_SIZE>
bool32 ReadNextKeywordValue(Array<char> string,
    FixedArray<char, KEYWORD_SIZE>* keyword, FixedArray<char, VALUE_SIZE>* value);