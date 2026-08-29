#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define eCryModule eCryM_System

#include <windows.h>
#include <tlhelp32.h>
#undef GetCharWidth
#undef GetUserName

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "CryCommon/CryCore/platform.h"
#include "CryCommon/CryMath/Cry_Math.h"
#include "CryCommon/CryMath/Cry_Camera.h"
#include "CryCommon/CrySystem/CryFile.h"
#include "CryCommon/CrySystem/CryPath.h"
#include "CryCommon/CryCore/smartptr.h"
#include "CryCommon/CryMath/Range.h"
#include "CryCommon/CryCore/CrySizer.h"
#include "CryCommon/CryCore/CryMalloc.h"
#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryThread.h"
#include "CryLibrary.h"
#include "CryCommon/CryCore/StlUtils.h"
#include <TlHelp32.h>

// Legacy per-module memory statistics POD used by Wars diagnostics.
// This is data-only compatibility; CryMP keeps its own allocator implementation.
#ifndef CRYMP_CRYMODULEMEMORYINFO_DEFINED
#define CRYMP_CRYMODULEMEMORYINFO_DEFINED
struct CryModuleMemoryInfo
{
    uint64 requested = 0;
    uint64 allocated = 0;
    uint64 freed = 0;
    int num_allocations = 0;
    uint64 CryString_allocated = 0;
    uint64 STL_allocated = 0;
    uint64 STL_wasted = 0;
};
#endif

static inline int RoundToClosestMB(size_t memSize)
{
    return static_cast<int>((memSize + (1 << 19)) >> 20);
}

class ITexture;
struct IRenderer;
struct ISystem;
struct IScriptSystem;
struct ITimer;
struct IFFont;
struct IInput;
struct IKeyboard;
struct ICVar;
struct IConsole;
struct IGame;
struct IEntitySystem;
struct IProcess;
struct ICryPak;
struct ICryFont;
struct I3DEngine;
struct IMovieSystem;
struct ISoundSystem;
struct IPhysicalWorld;

// Compatibility constants/helpers present in Wars CryCommon/Cry_XOptimise.h.
#ifndef CPUF_SSE
#define CPUF_SSE 1
#define CPUF_SSE2 2
#define CPUF_3DNOW 4
#define CPUF_MMX 8
#endif
#ifndef DATA_FOLDER
#define DATA_FOLDER "Game"
#endif
inline int FtoI(float value) { return static_cast<int>(value); }


// Wars used PathUtil; CryMP's current header exposes the same helpers as CryPath.
namespace PathUtil = CryPath;

// Single-function allocator expected by Wars CrySystem. Route through CryMP's
// exported allocator rather than CRT malloc/realloc so ownership stays consistent.
inline void* ModuleAlloc(void* ptr, std::size_t size)
{
    if (size == 0)
    {
        if (ptr)
            CryFree(ptr);
        return nullptr;
    }

    std::size_t allocatedSize = 0;
    return ptr ? CryRealloc(ptr, size, allocatedSize) : CryMalloc(size, allocatedSize);
}

// Legacy CryPak-aware fopen helper from CryCommon/ICryPak.h.
inline FILE* fxopen(const char* file, const char* mode)
{
    if (!file || !mode || !gEnv || !gEnv->pCryPak)
        return nullptr;

    bool writeAccess = false;
    for (const char* s = mode; *s; ++s)
    {
        if (*s == 'w' || *s == 'W' || *s == 'a' || *s == 'A' || *s == '+')
        {
            writeAccess = true;
            break;
        }
    }

    int flags = ICryPak::FLAGS_NO_MASTER_FOLDER_MAPPING;
    if (writeAccess)
        flags |= ICryPak::FLAGS_FOR_WRITING;

    char adjusted[_MAX_PATH] = {};
    const char* path = gEnv->pCryPak->AdjustFileName(file, adjusted, flags);
    return std::fopen(path, mode);
}

inline void CryError(const char* format, ...)
{
    if (!gEnv || !gEnv->pSystem || !format)
        return;
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';
    gEnv->pSystem->Error("%s", buffer);
}

// Legacy CrySystem allocation tracing flag; CryMP allocator does not export the old global.
inline bool g_bTraceAllocations = false;
