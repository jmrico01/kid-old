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

void TrimWhitespace(const Array<char>& string, Array<char>* trimmed)
{
	uint64 start = 0;
	while (start < string.size && IsWhitespace(string[start])) {
		start++;
	}
	uint64 end = string.size;
	while (end > 0 && IsWhitespace(string[end - 1])) {
		end--;
	}

	trimmed->data = string.data + start;
	trimmed->size = end - start;
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
		Array<char> stringWhole;
		stringWhole.size = dotIndex;
		stringWhole.data = string.data;
		if (!StringToIntBase10(stringWhole, &whole)) {
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

uint64 GetLastOccurrence(const Array<char>& string, char c)
{
	for (uint64 i = string.size - 1; i >= 0; i--) {
		if (string[i] == c) {
			return i;
		}
	}

	return string.size;
}

void ReadElementInSplitString(Array<char>* element, Array<char>* next, char separator)
{
	for (uint64 i = 0; i < element->size; i++) {
		if ((*element)[i] == separator) {
			next->data = element->data + i + 1;
			next->size = element->size - i - 1;
			element->size = i;
			return;
		}
	}

	next->size = 0;
}

template <typename T>
bool32 StringToElementArray(const Array<char>& string, char sep, bool trimElements,
	bool32 (*conversionFunction)(const Array<char>&, T*),
	int maxElements, T* array, int* numElements)
{
	int elementInd = 0;
	Array<char> element = string;
	while (true) {
		Array<char> next;
		ReadElementInSplitString(&element, &next, sep);
		Array<char> trimmed;
		if (trimElements) {
			TrimWhitespace(element, &trimmed);
		}
		else {
			trimmed = element;
		}
		if (!conversionFunction(trimmed, array + elementInd)) {
			LOG_INFO("String to array failed for %.*s in element %d conversion\n",
				string.size, string.data, elementInd);
			return false;
		}

		if (next.size == 0) {
			break;
		}
		element = next;
		elementInd++;
		if (elementInd >= maxElements) {
			LOG_INFO("String to array failed in %.*s (too many elements, max %d)\n",
				string.size, string.data, maxElements);
			return false;
		}
	}

	*numElements = elementInd + 1;
	return true;
}

template <uint64 KEYWORD_SIZE, uint64 VALUE_SIZE>
int ReadNextKeywordValue(const Array<char>& string,
	FixedArray<char, KEYWORD_SIZE>* keyword, FixedArray<char, VALUE_SIZE>* value)
{
	if (string.size == 0 || string[0] == '\0') {
		return 0;
	}

	int i = 0;

	keyword->array.size = 0;
	while (i < string.size && !IsWhitespace(string[i])) {
		if (keyword->array.size >= KEYWORD_SIZE) {
			LOG_INFO("Keyword too long %.*s\n", keyword->array.size, keyword->array.data);
			return -1;
		}
		keyword->Append(string[i++]);
	}

	if (i < string.size && IsWhitespace(string[i])) {
		i++;
	}

	value->array.size = 0;
	bool bracketValue = false;
	while (i < string.size &&
	((!bracketValue && string[i] != '\n' && string[i] != '\r')
	|| (bracketValue && string[i] != '}'))) {
		if (value->array.size == 0 && string[i] == '{') {
			bracketValue = true;
			i++;
			continue;
		}
		if (value->array.size >= VALUE_SIZE) {
			LOG_INFO("Value too long %.*s\n", value->array.size, value->array.data);
			return -1;
		}
		if (value->array.size == 0 && IsWhitespace(string[i])) {
			i++;
			continue;
		}
		value->Append(string[i++]);
	}

	while (value->array.size > 0 && IsWhitespace(value->array.data[value->array.size - 1])) {
		value->array.size--;
	}

	while (i < string.size && (IsWhitespace(string[i]) || string[i] == '}')) {
		i++;
	}

	return i;
}