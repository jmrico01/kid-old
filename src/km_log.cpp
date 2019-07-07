#include "km_log.h"

#include "km_debug.h"

// int LogEvent::PrintPrefix(char* buffer)
// {
// #if GAME_SLOW
//     const char* PREFIX_FORMAT = "";
// #elif GAME_INTERNAL
//     const char* PREFIX_FORMAT = "%s - %s:%d (%s)\n";
// #else
//     const char* PREFIX_FORMAT = "%s - %s:%d (%s)\n";
// #endif

//     return snprintf(buffer, )
//     int i = 0;

//     return i;
// }

void LogState::PrintFormat(LogCategory logCategory,
	const char* file, int line, const char* function,
	const char* format, ...)
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
    int logSize = vsnprintf(buffer + writeIndex, freeSpace1, format, args);
    if (logSize < 0 || (uint64)logSize >= freeSpace1) {
        logSize = vsnprintf(buffer, freeSpace2, format, args);
        if (logSize < 0 || (uint64)logSize >= freeSpace2) {
            // Not necessarily too big to write, freeSpace1 + freeSpace2 might be big enough
            // But this is easier and probably fine
            DEBUG_PANIC("Log too big!\n");
            return;
        }
        MemCopy(buffer + writeIndex, buffer, freeSpace1);
        MemMove(buffer, buffer + freeSpace1, freeSpace2);
    }
    va_end(args);

    LogEvent& event = logEvents[eventLast++];
    if (eventLast >= LOG_EVENTS_MAX) {
        eventLast = 0;
    }
    event.category = logCategory;
    uint64 fileLength = StringLength(file);
    MemCopy(event.file.fixedArray, file, MinUInt64(fileLength, PATH_MAX_LENGTH));
    event.line = line;
    uint64 functionLength = StringLength(function);
    MemCopy(event.function.fixedArray, function, MinUInt64(functionLength, FUNCTION_NAME_MAX_LENGTH));
    event.logStart = writeIndex;
    event.logSize = logSize;

	writeIndex += (uint64)logSize;
	if (writeIndex >= LOG_BUFFER_SIZE) {
		writeIndex -= LOG_BUFFER_SIZE;
	}
}