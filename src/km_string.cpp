#include "km_string.h"

#include "km_debug.h"

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

bool32 StringToFloat32(const char* string, int n, float32* f)
{
    int dotIndex = n;
    for (int i = 0; i < n; i++) {
        if (string[i] == '.') {
            dotIndex = i;
            break;
        }
    }

    int whole = 0;
    if (dotIndex > 0) {
        if (!StringToIntBase10(string, dotIndex, &whole)) {
            return false;
        }
    }
    int frac = 0;
    int fracLength = n - dotIndex - 1;
    if (fracLength > 0) {
        if (!StringToIntBase10(string + dotIndex + 1, fracLength, &frac)) {
            return false;
        }
    }

    *f = (float32)whole;
    if (fracLength > 0) {
        float32 fractional = (float32)frac;
        for (int i = 0; i < fracLength; i++) {
            fractional /= 10.0f;
        }
        *f += fractional;
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

bool32 ReadElementInSplitString(const char* string, int stringLength, char separator,
    int* elementLength, const char** next)
{
    for (int i = 0; i < stringLength; i++) {
        if (string[i] == separator) {
            *elementLength = i;
            *next = string + i + 1;
            return true;
        }
    }

    *elementLength = stringLength;
    *next = string + stringLength;
    return true;
}

template <typename T>
bool32 StringToElementArray(const char* string, int length, char sep, bool trimElements,
    bool32 (*conversionFunction)(const char*, int, T*),
    int maxElements, T* array, int* numElements)
{
    int elementInd = 0;
    const char* parsingString = string;
    int parsingLength = length;
    while (parsingLength > 0) {
        int elementLength;
        const char* next;
        if (!ReadElementInSplitString(parsingString, parsingLength, sep, &elementLength, &next)) {
            break;
        }
        if (elementInd >= maxElements) {
            DEBUG_PRINT("String to array failed in %.*s (too many elements, max %d)\n",
                length, string, maxElements);
            return false;
        }
        const char* elementTrimmed = parsingString;
        int elementTrimmedLength = elementLength;
        if (trimElements) {
            TrimWhitespace(parsingString, elementLength,
                &elementTrimmed, &elementTrimmedLength);
        }
        if (!conversionFunction(elementTrimmed, elementTrimmedLength, array + elementInd)) {
            DEBUG_PRINT("String to array failed in %.*s for element %d\n",
                length, string, elementInd);
            return false;
        }

        elementInd++;
        parsingLength -= (int)(next - parsingString);
        parsingString = next;
    }

    *numElements = elementInd;
    return true;
}
