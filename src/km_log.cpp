#include "km_log.h"

#include "km_debug.h"

void LogState::PrintFormat(LogCategory logCategory,
	const char* file, int line, const char* function,
	const char* format, ...)
{
#if GAME_INTERNAL
	const char* PREFIX_FORMAT = "";
#else
	const char* PREFIX_FORMAT = "%s - %s:%d (%s)\n";
#endif

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

	int n;

	va_list args;
	va_start(args, format);
	n = snprintf(buffer + writeIndex, freeSpace1, PREFIX_FORMAT,
		LOG_CATEGORY_NAMES[logCategory], file, line, function);
	n += vsnprintf(buffer + writeIndex + n, freeSpace1 - n, format, args);
	if (n < 0 || (uint64)n >= freeSpace1) {
		n = snprintf(buffer, freeSpace2, PREFIX_FORMAT,
			LOG_CATEGORY_NAMES[logCategory], file, line, function);
		n += vsnprintf(buffer + n, freeSpace2 - n, format, args);
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