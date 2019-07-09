#include "km_log.h"

#include "km_debug.h"

void LogState::PrintFormat(LogCategory logCategory,
	const char* file, int line, const char* function,
	const char* format, ...)
{
    uint64 eventIndex = eventFirst + eventCount;

    uint64 writeIndex;
    if (eventCount == 0) {
        writeIndex = 0;
    }
    else {
        uint64 prevEventIndex;
        if (eventIndex == 0) {
            prevEventIndex = LOG_EVENTS_MAX - 1;
        }
        else {
            prevEventIndex = eventIndex - 1;
        }

        const LogEvent& prevEvent = logEvents[eventIndex];
        writeIndex = prevEvent.logStart + prevEvent.logSize;
        if (writeIndex >= LOG_BUFFER_SIZE) {
            writeIndex -= LOG_BUFFER_SIZE;
        }
    }

	uint64 freeSpace1, freeSpace2;
    freeSpace1 = LOG_BUFFER_SIZE - writeIndex;
    freeSpace2 = writeIndex;

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

    LogEvent& event = logEvents[eventIndex];
    eventCount += 1;
    event.category = logCategory;
    uint64 fileStringLength = MinUInt64(StringLength(file), PATH_MAX_LENGTH - 1);
    MemCopy(event.file.fixedArray, file, fileStringLength);
    event.file[fileStringLength] = '\0';
    event.line = line;
    uint64 functionStringLength = MinUInt64(StringLength(function), FUNCTION_NAME_MAX_LENGTH - 1);
    MemCopy(event.function.fixedArray, function, functionStringLength);
    event.function[functionStringLength] = '\0';
    event.logStart = writeIndex;
    event.logSize = (uint64)logSize;
}