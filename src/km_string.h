#pragma once

int StringLength(const char* str);
bool StringCompare(const char* str1, const char* str2, int n);
void TrimWhitespace(const char* str, int n, const char** trimmedStr, int* trimmedN);
void CatStrings(
	size_t sourceACount, const char* sourceA,
	size_t sourceBCount, const char* sourceB,
	size_t destCount, char* dest);
inline bool32 IsWhitespace(char c);
bool32 StringToIntBase10(const char* string, int n, int* intBase10);
int GetLastOccurrence(const char* string, int n, char c);