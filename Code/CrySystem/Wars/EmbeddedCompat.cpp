#include "StdAfx.h"
#include "System.h"
#include "AutoDetectSpec.h"

#include <algorithm>
#include <cstdio>
#include <limits>

#include <psapi.h>

// CryMP: CrySystem is part of the executable, so there is no CrySystem.dll
// DllMain to provide this legacy module handle.
HMODULE gDLLHandle = GetModuleHandleW(nullptr);

// CryMP: the custom memory manager remains authoritative.  The Wars callers
// only use these legacy helpers for budgeting/debug statistics, so provide
// process-memory based compatibility implementations instead of linking the
// old Wars CryMemoryManager.
int CryMemoryGetAllocatedSize()
{
    PROCESS_MEMORY_COUNTERS_EX counters = {};
    counters.cb = sizeof(counters);

    if (GetProcessMemoryInfo(GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
    {
        const SIZE_T bytes = counters.PrivateUsage;
        return static_cast<int>((std::min)(bytes, static_cast<SIZE_T>((std::numeric_limits<int>::max)())));
    }

    return 0;
}

int CryStats(char* buf)
{
    const int bytes = CryMemoryGetAllocatedSize();
    if (buf)
    {
        std::sprintf(buf, "Process private memory = %d K", bytes / 1024);
    }
    return bytes / 1024;
}

namespace Win32SysInspect
{
    void GetNumCPUCores(unsigned int& totAvailToSystem, unsigned int& totAvailToProcess)
    {
        SYSTEM_INFO info = {};
        GetSystemInfo(&info);
        totAvailToSystem = info.dwNumberOfProcessors ? info.dwNumberOfProcessors : 1;

        DWORD_PTR processMask = 0;
        DWORD_PTR systemMask = 0;
        if (GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask) && processMask)
        {
            unsigned int count = 0;
            for (DWORD_PTR mask = processMask; mask; mask >>= 1)
                count += static_cast<unsigned int>(mask & 1);
            totAvailToProcess = count ? count : totAvailToSystem;
        }
        else
        {
            totAvailToProcess = totAvailToSystem;
        }
    }

    // First embedded milestone still uses the stock DX9 renderer DLL.
    bool IsDX10Supported()
    {
        return false;
    }

    void GetGPUInfo(char* pName, size_t bufferSize, unsigned int& vendorID,
        unsigned int& deviceID, unsigned int& totLocalVidMem, bool& supportsSM20orAbove)
    {
        vendorID = 0;
        deviceID = 0;
        totLocalVidMem = 0;
        supportsSM20orAbove = true;

        if (pName && bufferSize)
        {
            DISPLAY_DEVICEA display = {};
            display.cb = sizeof(display);
            if (EnumDisplayDevicesA(nullptr, 0, &display, 0) && display.DeviceString[0])
            {
                strncpy_s(pName, bufferSize, display.DeviceString, _TRUNCATE);
            }
            else
            {
                strncpy_s(pName, bufferSize, "Unknown GPU", _TRUNCATE);
            }
        }
    }

    int GetGPURating(unsigned int, unsigned int)
    {
        // Non-negative means accepted by the old renderer startup gate.
        return 1;
    }

    void GetOS(SPlatformInfo::EWinVersion& ver, bool& is64Bit, char* pName, size_t bufferSize)
    {
        ver = SPlatformInfo::WinUndetected;
#if defined(_WIN64)
        is64Bit = true;
#else
        BOOL wow64 = FALSE;
        is64Bit = IsWow64Process(GetCurrentProcess(), &wow64) && wow64;
#endif

        if (pName && bufferSize)
            strncpy_s(pName, bufferSize, "Windows", _TRUNCATE);
    }

    bool IsVistaKB940105Required()
    {
        return false;
    }
}

// The original implementation lives in AutoDetectSpec.cpp, which is excluded
// because it pulls in the legacy DX10 SDK. Keep the interface satisfied for
// this DX9-first embedded milestone.
void CSystem::AutoDetectSpec()
{
    CryLog("AutoDetectSpec: using CryMP compatibility path");
}
