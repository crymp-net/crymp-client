
#include "StdAfx.h"
#include "MemoryManager.h"
#include "CryCommon/CryCore/platform.h"

#if defined(WIN32) && !defined(XENON)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <Psapi.h>
#endif

#if defined(PS3)
	#include <sys/memory.h>
#endif

//////////////////////////////////////////////////////////////////////////
bool CCryMemoryManager::GetProcessMemInfo( SProcessMemInfo &minfo )
{
	minfo = SProcessMemInfo{};

#if defined(WIN32) && !defined(XENON)

	//////////////////////////////////////////////////////////////////////////
	typedef BOOL (WINAPI *GetProcessMemoryInfoProc)( HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD );

	PROCESS_MEMORY_COUNTERS pc{};
	pc.cb = sizeof(pc);
	HMODULE hPSAPI = LoadLibrary("psapi.dll");
	if (hPSAPI)
	{
		GetProcessMemoryInfoProc pGetProcessMemoryInfo = (GetProcessMemoryInfoProc)GetProcAddress(hPSAPI, "GetProcessMemoryInfo");
		if (pGetProcessMemoryInfo)
		{
			if (pGetProcessMemoryInfo( GetCurrentProcess(), &pc, sizeof(pc) ))
			{
				minfo.PageFaultCount = pc.PageFaultCount;
				minfo.PeakWorkingSetSize = pc.PeakWorkingSetSize;
				minfo.WorkingSetSize = pc.WorkingSetSize;
				minfo.QuotaPeakPagedPoolUsage = pc.QuotaPeakPagedPoolUsage;
				minfo.QuotaPagedPoolUsage = pc.QuotaPagedPoolUsage;
				minfo.QuotaPeakNonPagedPoolUsage = pc.QuotaPeakNonPagedPoolUsage;
				minfo.QuotaNonPagedPoolUsage = pc.QuotaNonPagedPoolUsage;
				minfo.PagefileUsage = pc.PagefileUsage;
				minfo.PeakPagefileUsage = pc.PeakPagefileUsage;
				return true;
			}
		}
	}
#elif defined(PS3)
	sys_memory_info_t ps3MemInfo;
	if(sys_memory_get_user_memory_size(&ps3MemInfo) == CELL_OK)
	{
		//currently only PagefileUsage seems to be used
		minfo.PagefileUsage = ps3MemInfo.total_user_memory - ps3MemInfo.available_user_memory;
		return true;
	}
#endif
	return false;
}
