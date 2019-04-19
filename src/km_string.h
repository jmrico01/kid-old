#pragma once

#include "km_lib.h"
#include "km_math.h"

// TODO strings should be Array or FixedArray objects

int StringLength(const char* str);
bool StringCompare(const char* str1, const char* str2, int n);
void TrimWhitespace(const char* str, int n, const char** trimmedStr, int* trimmedN);
void CatStrings(
	size_t sourceACount, const char* sourceA,
	size_t sourceBCount, const char* sourceB,
	size_t destCount, char* dest);

inline bool32 IsWhitespace(char c);
void TrimWhitespace(const Array<char>& string, Array<char>* trimmed);
bool32 StringToIntBase10(const Array<char>& string, int* intBase10);
bool32 StringToFloat32(const Array<char>& string, float32* f);
uint64 GetLastOccurrence(const Array<char>& string, char c);
void ReadElementInSplitString(Array<char>* element, Array<char>* next, char separator);

template <typename T>
bool32 StringToElementArray(const Array<char>& string, char sep, bool trimElements,
    bool32 (*conversionFunction)(const Array<char>&, T*),
    int maxElements, T* array, int* numElements);

// TODO this fits more into a km-file-format module. not really a general string lib function
template <uint64 KEYWORD_SIZE, uint64 VALUE_SIZE>
bool32 ReadNextKeywordValue(Array<char> string,
    FixedArray<char, KEYWORD_SIZE>* keyword, FixedArray<char, VALUE_SIZE>* value);