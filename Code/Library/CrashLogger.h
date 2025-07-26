#pragma once

#include <cstdio>

namespace CrashLogger
{
	using LogFileProvider = std::FILE* (*)();
	using HeapInfoProvider = void (*)(std::FILE* file, void* address);

	void OnEngineError(const char* format, va_list args);

	void Enable(LogFileProvider logFileProvider, HeapInfoProvider heapInfoProvider, const char* banner);
}
