#pragma once

#include <cstdio>
#include <cstdarg>

namespace CrashLogger
{
    using LogFileProvider = std::FILE* (*)();
    using HeapInfoProvider = void (*)(std::FILE* file, void* address);

    class ExtraProvider
    {
    public:
        virtual ~ExtraProvider() = default;
        virtual void OnCrash(std::FILE* file) = 0;
    };

    void AddExtraProvider(ExtraProvider* provider);

    void OnEngineError(const char* format, va_list args);

    void Enable(LogFileProvider logFileProvider,
                HeapInfoProvider heapInfoProvider,
                const char* banner);
}
