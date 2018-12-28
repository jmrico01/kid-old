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