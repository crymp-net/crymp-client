#pragma once

#include <cstddef>

// CryMalloc functions exported by the EXE are automatically used instead of CrySystem.dll ones
#ifdef _MSC_VER
#define CRYMALLOC_API extern "C" __declspec(dllexport)
#else
#define CRYMALLOC_API
#endif

// Implemented in Code/CrySystem/CryMemoryManager.cpp
CRYMALLOC_API void* CryMalloc(std::size_t size, std::size_t& allocatedSize);
CRYMALLOC_API void* CryRealloc(void* oldPtr, std::size_t newSize, std::size_t& allocatedSize);
CRYMALLOC_API std::size_t CryFree(void* ptr);
