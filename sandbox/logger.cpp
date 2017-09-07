#include "logger.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <strsafe.h>

void log(char* format, ...) {
	va_list args;
	va_start(args, format);
	char buffer[1024];
	memset(buffer, 0, sizeof(buffer));
	int written = vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
	OutputDebugString(buffer);
	OutputDebugString("\n");
	va_end(args);
}
