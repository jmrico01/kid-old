#include "km_string.h"

int StringLength(const char* string)
{
	int length = 0;
	while (*string++) {
		length++;
	}

	return length;
}

bool StringCompare(const char* str1, const char* str2, int n)
{
	for (int i = 0; i < n; i++) {
		if (str1[i] != str2[i]) {
			return false;
		}
	}

	return true;
}

void TrimWhitespace(const char* str, int n, const char** trimmedStr, int* trimmedN)
{
    int i = 0;
    while (i < n && IsWhitespace(str[i])) {
        i++;
    }
    while (i < n && IsWhitespace(str[n - 1])) {
        n--;
    }

    *trimmedStr = str + i;
    *trimmedN = n - i;
}

void CatStrings(
	size_t sourceACount, const char* sourceA,
	size_t sourceBCount, const char* sourceB,
	size_t destCount, char* dest)
{
	for (size_t i = 0; i < sourceACount; i++) {
		*dest++ = *sourceA++;
	}

	for (size_t i = 0; i < sourceBCount; i++) {
		*dest++ = *sourceB++;
	}

	*dest++ = '\0';
}

inline bool32 IsWhitespace(char c)
{
	return c == ' ' || c == '\t'
		|| c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

bool32 StringToIntBase10(const char* string, int n, int* intBase10)
{
	if (n <= 0) {
		return false;
	}

	bool32 negative = false;
	*intBase10 = 0;
	for (int i = 0; i < n; i++) {
		char c = string[i];
		if (i == 0 && c == '-') {
			negative = true;
			continue;
		}
		if (c < '0' || c > '9') {
			return false;
		}
		*intBase10 = (*intBase10) * 10 + (int)(c - '0');
	}

	if (negative) {
		*intBase10 = -(*intBase10);
	}
	return true;
}

int GetLastOccurrence(const char* string, int n, char c)
{
	for (int i = n - 1; i >= 0; i--) {
		if (string[i] == c) {
			return i;
		}
	}

	return -1;
}
