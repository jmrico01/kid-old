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

bool32 StringToIntBase10(const Array<char>& string, int* intBase10)
{
    if (string.size == 0) {
        return false;
    }

    bool32 negative = false;
    *intBase10 = 0;
    for (uint64 i = 0; i < string.size; i++) {
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

bool32 StringToFloat32(const Array<char>& string, float32* f)
{
    uint64 dotIndex = 0;
    while (dotIndex < string.size && string[dotIndex] != '.') {
        dotIndex++;
    }

    int whole = 0;
    float32 wholeNegative = false;
    if (dotIndex > 0) {
        if (!StringToIntBase10(string, &whole)) {
            return false;
        }
        wholeNegative = string[0] == '-';
    }
    int frac = 0;
    Array<char> fracString;
    fracString.size = string.size - dotIndex - 1;
    if (fracString.size > 0) {
        fracString.data = string.data + dotIndex + 1;
        if (!StringToIntBase10(fracString, &frac)) {
            return false;
        }
    }

    *f = (float32)whole;
    if (fracString.size > 0) {
        frac = wholeNegative ? -frac : frac;
        float32 fractional = (float32)frac;
        for (uint64 i = 0; i < fracString.size; i++) {
            fractional /= 10.0f;
        }
        *f += fractional;
    }
    return true;
}

/*bool32 StringToIntBase10(const char* string, int n, int* intBase10)
{
    Array<char> stringArray;
    stringArray.data = (char*)string;
    stringArray.size = n;
    return StringToIntBase10(stringArray, intBase10);
}

bool32 StringToFloat32(const char* string, int n, float32* f)
{
    Array<char> stringArray;
    stringArray.data = (char*)string;
    stringArray.size = n;
    return StringToFloat32(stringArray, f);
}*/

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
bool32 StringToElementArray(const Array<char>& string, char sep, bool trimElements,
    bool32 (*conversionFunction)(const char*, int, T*),
    int maxElements, T* array, int* numElements)
{
    int elementInd = 0;
    const char* parsingString = string.data;
    int parsingLength = (int)string.size;
    while (parsingLength > 0) {
        int elementLength;
        const char* next;
        if (!ReadElementInSplitString(parsingString, parsingLength, sep, &elementLength, &next)) {
            break;
        }
        if (elementInd >= maxElements) {
            DEBUG_PRINT("String to array failed in %.*s (too many elements, max %d)\n",
                string.size, string.data, maxElements);
            return false;
        }
        const char* elementTrimmed = parsingString;
        int elementTrimmedLength = elementLength;
        if (trimElements) {
            TrimWhitespace(parsingString, elementLength,
                &elementTrimmed, &elementTrimmedLength);
        }
        Array<char> trimmed;
        trimmed.data = elementTrimmed;
        trimmed.size = elementTrimmedLength;
        if (!conversionFunction(trimmed, array + elementInd)) {
            DEBUG_PRINT("String to array failed in %.*s for element %d\n",
                string.size, string.data, elementInd);
            return false;
        }

        elementInd++;
        parsingLength -= (int)(next - parsingString);
        parsingString = next;
    }

    *numElements = elementInd;
    return true;
}

template <uint64 KEYWORD_SIZE, uint64 VALUE_SIZE>
int ReadNextKeywordValue(Array<char> string,
    FixedArray<char, KEYWORD_SIZE>* keyword, FixedArray<char, VALUE_SIZE>* value)
{
    if (string.size == 0 || string[0] == '\0') {
        return 0;
    }

    int i = 0;

    keyword->size = 0;
    while (i < string.size && !IsWhitespace(string[i])) {
        if (keyword->size >= KEYWORD_SIZE) {
            DEBUG_PRINT("Keyword too long %.*s\n", keyword->size, keyword->data);
            return -1;
        }
        keyword->Append(string[i++]);
    }

    if (i < string.size && IsWhitespace(string[i])) {
        i++;
    }

    value->size = 0;
    bool bracketValue = false;
    while (i < string.size &&
    ((!bracketValue && string[i] != '\n' && string[i] != '\r')
    || (bracketValue && string[i] != '}'))) {
        if (value->size == 0 && string[i] == '{') {
            bracketValue = true;
            i++;
            continue;
        }
        if (value->size >= VALUE_SIZE) {
            DEBUG_PRINT("Value too long %.*s\n", value->size, value->data);
            return -1;
        }
        if (value->size == 0 && IsWhitespace(string[i])) {
            i++;
            continue;
        }
        value->Append(string[i++]);
    }

    while (value->size > 0 && IsWhitespace(value->data[value->size - 1])) {
        value->size--;
    }

    while (i < string.size && (IsWhitespace(string[i]) || string[i] == '}')) {
        i++;
    }

    return i;
}