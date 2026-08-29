#pragma once

// CryMP C++20 compatibility for the old CryEngine 2 CryLibrary helpers.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

inline HMODULE CryLoadLibrary(const char* name)
{
    return ::LoadLibraryA(name);
}

inline FARPROC CryGetProcAddress(HMODULE module, const char* name)
{
    return module ? ::GetProcAddress(module, name) : nullptr;
}

inline FARPROC CryGetProcAddress(void* module, const char* name)
{
    return module ? ::GetProcAddress(static_cast<HMODULE>(module), name) : nullptr;
}

inline bool CryCreateDirectory(const char* path, void*)
{
    if (!path || !*path)
        return false;
    return ::CreateDirectoryA(path, nullptr) != FALSE || ::GetLastError() == ERROR_ALREADY_EXISTS;
}

inline bool CryFreeLibrary(HMODULE module)
{
    return module ? (::FreeLibrary(module) != FALSE) : true;
}

inline int CryGetCurrentDirectory(unsigned int bufferLength, char* buffer)
{
    return static_cast<int>(::GetCurrentDirectoryA(bufferLength, buffer));
}

inline bool CryFreeLibrary(void* module)
{
    return module ? (::FreeLibrary(static_cast<HMODULE>(module)) != FALSE) : true;
}


inline unsigned int CryGetFileAttributes(const char* path)
{
    return path ? static_cast<unsigned int>(::GetFileAttributesA(path)) : static_cast<unsigned int>(-1);
}

inline bool CrySetFileAttributes(const char* path, unsigned int attributes)
{
    return path && ::SetFileAttributesA(path, static_cast<DWORD>(attributes)) != FALSE;
}

inline SHORT CryGetAsyncKeyState(int virtualKey)
{
    return ::GetAsyncKeyState(virtualKey);
}
