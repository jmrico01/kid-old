#include "km_log.h"

#include "km_debug.h"

void LogState::PrintFormat(const char* format, ...)
{
	uint64 freeSpace1, freeSpace2;
	if (writeIndex >= readIndex) {
		freeSpace1 = LOG_BUFFER_SIZE - writeIndex;
		freeSpace2 = readIndex;
	}
	else {
		freeSpace1 = readIndex - writeIndex;
		freeSpace2 = 0;
	}

	DEBUG_ASSERT(freeSpace1 != 0 || freeSpace2 != 0);

	va_list args;
	va_start(args, format);
	int n = vsnprintf(buffer + writeIndex, freeSpace1, format, args);
	if (n < 0 || (uint64)n >= freeSpace1) {
		n = vsnprintf(buffer, freeSpace2, format, args);
		if (n < 0 || (uint64)n >= freeSpace2) {
			DEBUG_PANIC("log too big!");
			return;
		}
	}
	va_end(args);

	writeIndex += (uint64)n;
	if (writeIndex >= LOG_BUFFER_SIZE) {
		writeIndex -= LOG_BUFFER_SIZE;
	}
}