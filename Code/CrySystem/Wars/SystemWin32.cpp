
#include "StdAfx.h"
#include "System.h"
#include <time.h>

#include "CryCommon/CryNetwork/INetwork.h"
#include "CryCommon/Cry3DEngine/I3DEngine.h"
#include "CryCommon/CryAISystem/IAISystem.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CrySystem/ICryPak.h"
#include "CryCommon/CryMovie/IMovieSystem.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryCommon/CryInput/IInput.h"
#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/CrySoundSystem/ISound.h"
#include "CryCommon/CrySoundSystem/IMusicSystem.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryCommon/CryAnimation/ICryAnimation.h"
#include "CryLibrary.h"
#include "CryCommon/CryGame/IGame.h"
#include "CryCommon/CryAction/IGameFramework.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include <float.h>
#include <ShlObj.h>
#include <shellapi.h> // Needed for ShellExecute.
#include <Psapi.h>
#include <Aclapi.h>
#include <TlHelp32.h>

#include "DebugCallStack.h"
#endif

#include "XConsole.h"

#include "CrySizerStats.h"
#include "CrySizerImpl.h"
#include "StreamEngine.h"
#include "LocalizedStringManager.h"

#include "XML/XmlUtils.h"
#include "AutoDetectSpec.h"

extern CMTSafeHeap* g_pPakHeap;

// this is the list of modules that can be loaded into the game process
// Each array element contains 2 strings: the name of the module (case-insensitive)
// and the name of the group the module belongs to
//////////////////////////////////////////////////////////////////////////
const char g_szGroupCore[] = "CryEngine";
const char* g_szModuleGroups[][2] = {
	{"Editor.exe", g_szGroupCore},
	{"CrySystem.dll", g_szGroupCore},
	{"CryScriptSystem.dll", g_szGroupCore},
	{"CryNetwork.dll", g_szGroupCore},
	{"CryPhysics.dll", g_szGroupCore},
	{"CryMovie.dll", g_szGroupCore},
	{"CryInput.dll", g_szGroupCore},
	{"CrySoundSystem.dll", g_szGroupCore},
#ifdef WIN64
	{"crysound64.dll", g_szGroupCore},
#else
	{"crysound.dll", g_szGroupCore},
#endif
	{"CryFont.dll", g_szGroupCore},
	{"CryAISystem.dll", g_szGroupCore},
	{"CryEntitySystem.dll", g_szGroupCore},
	{"Cry3DEngine.dll", g_szGroupCore},
	{"CryGame.dll", g_szGroupCore},
	{"Game.dll", g_szGroupCore},
	{"CryAction.dll", g_szGroupCore},
	{"CryAnimation.dll", g_szGroupCore},
	{"CryRenderD3D9.dll", g_szGroupCore},
	{"CryRenderD3D10.dll", g_szGroupCore},
	{"CryRenderOGL.dll", g_szGroupCore},
	{"CryRenderNULL.dll", g_szGroupCore}
};

//////////////////////////////////////////////////////////////////////////
void CSystem::SetAffinity()
{
	// the following code is only for Windows
#ifdef WIN32
	// set the process affinity
	ICVar* pcvAffinityMask = GetIConsole()->GetCVar("sys_affinity");
	if (!pcvAffinityMask)
		pcvAffinityMask = GetIConsole()->RegisterInt("sys_affinity",0, 0);

	if (pcvAffinityMask)
	{
		unsigned nAffinity = pcvAffinityMask->GetIVal();
		if (nAffinity)
		{
			typedef BOOL (WINAPI *FnSetProcessAffinityMask)(IN HANDLE hProcess,IN DWORD_PTR dwProcessAffinityMask);
			HMODULE hKernel = CryLoadLibrary ("kernel32.dll");
			if (hKernel)
			{
				FnSetProcessAffinityMask SetProcessAffinityMask = (FnSetProcessAffinityMask)GetProcAddress(hKernel, "SetProcessAffinityMask");
				if (SetProcessAffinityMask && !SetProcessAffinityMask(GetCurrentProcess(), nAffinity))
					GetILog()->LogError("Error: Cannot set affinity mask %d, error code %d", nAffinity, GetLastError());
				FreeLibrary (hKernel);
			}
		}
	}
#endif
}





//! dumps the memory usage statistics to the log
//////////////////////////////////////////////////////////////////////////
void CSystem::DumpMemoryUsageStatistics(bool bUseKB)
{
//	CResourceCollector ResourceCollector;

//	TickMemStats(nMSP_ForDump,&ResourceCollector);
	TickMemStats(nMSP_ForDump);
	
	CrySizerStatsRenderer StatsRenderer (this, m_pMemStats, 10, 0);
	StatsRenderer.dump(bUseKB);

	int iSizeInM = m_env.p3DEngine->GetTerrainSize();

//	ResourceCollector.ComputeDependencyCnt();

//	int iTerrainSectorSize = m_env.p3DEngine->GetTerrainSectorSize();

//	if(iTerrainSectorSize)
//		ResourceCollector.LogData(*GetILog(),AABB(Vec3(0,0,0),Vec3(iSizeInM,iSizeInM,0)),iSizeInM/iTerrainSectorSize);
	
	// since we've recalculated this mem stats for dumping, we'll want to calculate it anew the next time it's rendered
	SAFE_DELETE(m_pMemStats);
}

// collects the whole memory statistics into the given sizer object
//////////////////////////////////////////////////////////////////////////

struct SmallModuleInfo
{
	string name;
	CryModuleMemoryInfo memInfo;


};

#if defined(WIN32) || defined(XENON)
#pragma pack(push,1)
const struct PEHeader_DLL
{
	DWORD signature;
	IMAGE_FILE_HEADER _head;
	IMAGE_OPTIONAL_HEADER opt_head;
	IMAGE_SECTION_HEADER *section_header;  // actual number in NumberOfSections
};
#pragma pack(pop)
#endif 

const SmallModuleInfo* FindModuleInfo(std::vector<SmallModuleInfo>& vec, const char * name) 
{
	for (size_t i = 0; i < vec.size(); ++i)	{
		if (!vec[i].name.compareNoCase(name)) 
			return &vec[i];
	}

	return 0;
}


void CSystem::CollectMemStats (CrySizerImpl* pSizer, MemStatsPurposeEnum nPurpose)
{

	std::vector<SmallModuleInfo> stats;
	//////////////////////////////////////////////////////////////////////////
	static const char* szModules[] = {
		"Editor.exe",
		"CrySystem.dll",
		"CryScriptSystem.dll",
		"CryNetwork.dll",
		"CryPhysics.dll",
		"CryMovie.dll",
		"CryInput.dll",
		"CrySoundSystem.dll",
		"CryFont.dll",
		"CryAISystem.dll",
		"CryEntitySystem.dll",
		"Cry3DEngine.dll",
		"CryAnimation.dll",
		"CryGame.dll",
		"CryAction.dll",
		"CryRenderD3D9.dll",
		"CryRenderD3D10.dll",
		"CryRenderNULL.dll",
	};

	int nRows = 0;
#ifdef WIN32
	for (int i = 0; i < sizeof(szModules)/sizeof(szModules[0]); i++)
	{
		const char *szModule = szModules[i];
		HMODULE hModule = GetModuleHandle( szModule );
		if (!hModule)
			continue;

		//totalStatic += me.modBaseSize;
		typedef void (*PFN_MODULEMEMORY)( CryModuleMemoryInfo* );
		PFN_MODULEMEMORY fpCryModuleGetAllocatedMemory = (PFN_MODULEMEMORY)::GetProcAddress( hModule,"CryModuleGetMemoryInfo" );
		if (!fpCryModuleGetAllocatedMemory)
			continue;

		PEHeader_DLL pe_header;
		PEHeader_DLL *header = &pe_header;

		/*
		#ifdef XENON
		FILE *file = fopen( string("d:\\")+szModule,"rb" );
		if (file)
		{
		IMAGE_DOS_HEADER dos_hdr;
		fread( &dos_hdr,sizeof(dos_hdr),1,file );
		SwapEndian( dos_hdr.e_lfanew );
		fseek( file,dos_hdr.e_lfanew,SEEK_SET );
		fread( &pe_header,sizeof(pe_header),1,file );
		fclose(file);
		SwapEndian(header->opt_head.SizeOfInitializedData);
		SwapEndian(header->opt_head.SizeOfUninitializedData);
		SwapEndian(header->opt_head.SizeOfCode);
		SwapEndian(header->opt_head.SizeOfHeaders);
		}
		#elif defined(WIN32)
		*/
		const IMAGE_DOS_HEADER *dos_head = (IMAGE_DOS_HEADER*)hModule;
		if (dos_head->e_magic != IMAGE_DOS_SIGNATURE)
		{
			// Wrong pointer, not to PE header.
			continue;
		}
		header = (PEHeader_DLL*)(const void *)((char *)dos_head + dos_head->e_lfanew);
		//#endif

		SmallModuleInfo moduleInfo;
		moduleInfo.name = szModule;
		fpCryModuleGetAllocatedMemory( &moduleInfo.memInfo );

		if (nMSP_ForCrashLog == nPurpose)
		{
			int usedInModule = moduleInfo.memInfo.allocated - moduleInfo.memInfo.freed;
			int numAllocations = moduleInfo.memInfo.num_allocations;
			CryLogAlways("%s Used in module:%d Allocations:%d", szModule, usedInModule, numAllocations);
		}
		else
		{
			stats.push_back(moduleInfo);
		}
		
	}
#endif

	if (nMSP_ForCrashLog == nPurpose)
	{
		return;
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "CrySystem");
		{
			{
				SIZER_COMPONENT_NAME (pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CrySystem.dll");
				if (info)
					pSizer->AddObject(info, info->memInfo.allocated - info->memInfo.requested );
			}

			{
				SIZER_COMPONENT_NAME(pSizer, "VFS");
				if (m_pStreamEngine)
				{
					SIZER_COMPONENT_NAME(pSizer, "Stream Engine");
					m_pStreamEngine->GetMemoryStatistics(pSizer);
				}
				if (m_env.pCryPak)
				{
					SIZER_COMPONENT_NAME(pSizer, "CryPak");
					// CryMP's ICryPak no longer exposes the legacy GetMemoryStatistics API.
					if (g_pPakHeap)
						g_pPakHeap->GetMemoryUsage(pSizer);
				}
			}
			{
				SIZER_COMPONENT_NAME(pSizer, "Localization Data");
				// CryMP LocalizationManager is externally owned and does not expose
				// the Wars-only GetMemoryUsage helper.
			}
			{
				SIZER_COMPONENT_NAME(pSizer, "XML");
				m_pXMLUtils->GetMemoryUsage(pSizer);
			}
			{
				SIZER_COMPONENT_NAME(pSizer, "Scaleform");
				GetFlashMemoryUsage(pSizer);
			}
			if (m_env.pConsole)
			{
				SIZER_COMPONENT_NAME (pSizer, "Console");
				m_env.pConsole->GetMemoryUsage (pSizer);
			}

		}
	}

	if (m_env.p3DEngine)
	{

		SIZER_COMPONENT_NAME(pSizer, "Cry3DEngine");
		{
			m_env.p3DEngine->GetMemoryUsage (pSizer);
			{
				SIZER_COMPONENT_NAME (pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "Cry3DEngine.dll");
				if (info)
					pSizer->AddObject(info, info->memInfo.allocated - info->memInfo.requested );
			}

		}

	}

	if (m_env.pCharacterManager)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryAnimation");
		{
			{
				SIZER_COMPONENT_NAME (pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryAnimation.dll");
				if (info)
					pSizer->AddObject(info, info->memInfo.allocated - info->memInfo.requested );
			}

			m_env.pCharacterManager->GetMemoryUsage(pSizer);
		}

	}

	if (m_env.pPhysicalWorld)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryPhysics");
		{
			{
				SIZER_COMPONENT_NAME (pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryPhysics.dll");
				if (info)
					pSizer->AddObject(info, info->memInfo.allocated - info->memInfo.requested );
			}
			m_env.pPhysicalWorld->GetMemoryStatistics (pSizer);
		}
		
	}
	
	if (m_env.pRenderer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryRenderer");
		{
			{
				SIZER_COMPONENT_NAME (pSizer, "$Allocations waste D3D9");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryRenderD3D9.dll");
				if (info)
					pSizer->AddObject(info, info->memInfo.allocated - info->memInfo.requested );
			}
			{
				SIZER_COMPONENT_NAME (pSizer, "$Allocations waste D3D10");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryRenderD3D10.dll");
				if (info)
					pSizer->AddObject(info, info->memInfo.allocated - info->memInfo.requested );
			}

		m_env.pRenderer->GetMemoryUsage (pSizer);
		}

	}

	if (m_env.pCryFont)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryFont");
		{
			{
				SIZER_COMPONENT_NAME (pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryFont.dll");
				if (info)
					pSizer->AddObject(info, info->memInfo.allocated - info->memInfo.requested );
			}

			m_env.pCryFont->GetMemoryUsage(pSizer);
		}
		
	}

	if (m_env.pSoundSystem)
	{
		SIZER_COMPONENT_NAME(pSizer, "CrySoundSystem");
		{
			{
				SIZER_COMPONENT_NAME (pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CrySiundSystem.dll");
				if (info)
					pSizer->AddObject(info, info->memInfo.allocated - info->memInfo.requested );
			}
			
			{
				SIZER_COMPONENT_NAME(pSizer, "Sound");
				m_env.pSoundSystem->GetMemoryUsage(pSizer);
			}
			if (m_env.pMusicSystem)
			{
				SIZER_COMPONENT_NAME(pSizer, "Music");
				m_env.pMusicSystem->GetMemoryUsage(pSizer);
			}
		}
	}

	if (m_env.pScriptSystem)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryScriptSystem");
		{
			{
				SIZER_COMPONENT_NAME (pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryScriptSystem.dll");
				if (info)
					pSizer->AddObject(info, info->memInfo.allocated - info->memInfo.requested );
			}
		m_env.pScriptSystem->GetMemoryStatistics(pSizer);
		}

	}

	if (m_env.pAISystem)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryAISystem");
		{
			{
				SIZER_COMPONENT_NAME (pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryAISystem.dll");
				if (info)
					pSizer->AddObject(info, info->memInfo.allocated - info->memInfo.requested );
			}
		m_env.pAISystem->GetMemoryStatistics (pSizer);
		}

	}

	if (m_env.pGame)
	{
		{
			SIZER_COMPONENT_NAME(pSizer, "Game");
			{
				SIZER_COMPONENT_NAME (pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryGame.dll");
				if (info)
					pSizer->AddObject(info, info->memInfo.allocated - info->memInfo.requested );
			}

			m_env.pGame->GetMemoryStatistics (pSizer);
			m_env.pGame->GetIGameFramework()->GetMemoryStatistics(pSizer);
		}
	}

	if (m_env.pNetwork)
	{
		SIZER_COMPONENT_NAME(pSizer, "Network");
		{
			SIZER_COMPONENT_NAME (pSizer, "$Allocations waste");
			const SmallModuleInfo* info = FindModuleInfo(stats, "CryNetwork.dll");
			if (info)
				pSizer->AddObject(info, info->memInfo.allocated - info->memInfo.requested );
		}

		m_env.pNetwork->GetMemoryStatistics(pSizer);
	}

	if (m_env.pEntitySystem)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryEntitySystem");
		{
			SIZER_COMPONENT_NAME (pSizer, "$Allocations waste");
			const SmallModuleInfo* info = FindModuleInfo(stats, "CryEntitySystem.dll");
			if (info)
				pSizer->AddObject(info, info->memInfo.allocated - info->memInfo.requested );
		}

		m_env.pEntitySystem->GetMemoryStatistics(pSizer);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "UserData");
		if(m_pUserCallback)
		{
			m_pUserCallback->GetMemoryUsage(pSizer);
		}
	}

#ifdef WIN32
	{
		SIZER_COMPONENT_NAME(pSizer, "Code");
		GetExeSizes (pSizer, nPurpose);
	}
#endif
 
	pSizer->end();
}

//////////////////////////////////////////////////////////////////////////
const char *CSystem::GetUserName()
{
#ifdef WIN32
	static char	szNameBuffer[1024];
	memset(szNameBuffer, 0, 1024);

	DWORD dwSize = 1024;
	::GetUserNameA(szNameBuffer, &dwSize);
	return szNameBuffer;
#else
	return "";
#endif
}

// refreshes the m_pMemStats if necessary; creates it if it's not created
//////////////////////////////////////////////////////////////////////////
void CSystem::TickMemStats(MemStatsPurposeEnum nPurpose, IResourceCollector *pResourceCollector )
{
	// gather the statistics, if required
	// if there's  no object, or if it's time to recalculate, or if it's for dump, then recalculate it
	if (!m_pMemStats || (m_env.pRenderer->GetFrameID(false)%m_cvMemStats->GetIVal())==0 || nPurpose == nMSP_ForDump)
	{
		if (!m_pMemStats)
		{
			if (m_cvMemStats->GetIVal() < 4 && m_cvMemStats->GetIVal())
				GetILog()->LogToConsole ("memstats is too small (%d). Performnce impact can be significant. Please set to a greater value.",m_cvMemStats->GetIVal());
			m_pMemStats = new CrySizerStats();
		}

		if (!m_pSizer)
			m_pSizer = new CrySizerImpl();

		m_pSizer->SetResourceCollector(pResourceCollector);

		m_pMemStats->startTimer(0,GetITimer());
		CollectMemStats (m_pSizer,nPurpose);
		m_pMemStats->stopTimer(0,GetITimer());

		m_pMemStats->startTimer(1,GetITimer());
		CrySizerStatsBuilder builder (m_pSizer);
		builder.build (m_pMemStats);
		m_pMemStats->stopTimer(1,GetITimer());

		m_pMemStats->startTimer(2,GetITimer());
		m_pSizer->clear();
		m_pMemStats->stopTimer(2,GetITimer());
	}
	else
		m_pMemStats->incAgeFrames();
}

//#define __HASXP

// these 2 functions are duplicated in System.cpp in editor
//////////////////////////////////////////////////////////////////////////
#if !defined(LINUX)
extern int CryStats(char *buf);
#endif
int CSystem::DumpMMStats(bool log)
{
#if defined(LINUX)
	return 0;
#else
	if(log)
	{
		char buf[1024];
		int n = CryStats(buf);
		GetILog()->Log(buf);
		return n;
	}
	else
	{
		return CryStats(NULL);
	};
#endif
};   

//////////////////////////////////////////////////////////////////////////
struct CryDbgModule
{
	HANDLE heap;
	WIN_HMODULE handle;
	string name;
	DWORD dwSize;
};

//////////////////////////////////////////////////////////////////////////
void CSystem::DebugStats(bool checkpoint, bool leaks)
{
#ifdef WIN32
	std::vector<CryDbgModule> dbgmodules;

/*
	{
		{ NULL, (WIN_HMODULE)GetModuleHandle("CrySystem.dll"), "SYSTEM"},
#ifdef _WIN32		
		{ NULL, (WIN_HMODULE)GetModuleHandle("Editor.exe"), "EDITOR"},
#endif
		{ NULL, m_dll.hNetwork,      "NETWORK" },
		{ NULL, m_dll.hGame,         "GAME" },
		{ NULL, m_dll.hAI,           "AI" },
		{ NULL, m_dll.hEntitySystem, "ENTITY" },
		{ NULL, m_dll.hRenderer,     "RENDERER" },
		{ NULL, m_dll.hInput,        "INPUT" },
		{ NULL, m_dll.hSound,        "SOUND" },
		{ NULL, m_dll.hPhysics,      "PHYSICS" },
		{ NULL, m_dll.hFont,         "FONT" },
		{ NULL, m_dll.hScript,       "SCRIPT" },
		{ NULL, m_dll.h3DEngine,     "3DENGINE" },
		//{ NULL, (WIN_HMODULE)GetModuleHandle("CrySystem.dll"), "SYSTEM"},
		{ NULL, m_dll.hAnimation,    "ANIMATION" },
#ifdef WIN64
		{ NULL, LoadDLL("crysound64.dll"),        "FMOD" }     // temp!
#else
		{ NULL, LoadDLL("crysound.dll"),        "FMOD" }     // temp!
#endif
		// missing: OPENGL
	}; 
*/

	//////////////////////////////////////////////////////////////////////////
	// Use windows Performance Monitoring API to enumerate all modules of current process.
	//////////////////////////////////////////////////////////////////////////
	HANDLE hSnapshot;
	hSnapshot = CreateToolhelp32Snapshot (TH32CS_SNAPMODULE, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 me;
		memset (&me, 0, sizeof(me));
		me.dwSize = sizeof(me);

		if (Module32First (hSnapshot, &me))
		{
			// the sizes of each module group
			do
			{
				CryDbgModule module;
				module.handle = me.hModule;
				module.name = me.szModule;
				module.dwSize = me.modBaseSize;
				dbgmodules.push_back( module );
			}
			while(Module32Next (hSnapshot, &me));
		}
		CloseHandle (hSnapshot);
	}
	//////////////////////////////////////////////////////////////////////////

	
	ILog *log = GetILog();
	int totalal = 0, totalbl = 0, nolib = 0;

#ifdef _DEBUG
	int extrastats[10];
#endif
	
	int totalUsedInModules = 0;
	int countedMemoryModules = 0;
	for(int i = 0; i < (int)(dbgmodules.size()); i++)
	{
		if(!dbgmodules[i].handle)
		{
			CryLogAlways( "WARNING: <CrySystem> CSystem::DebugStats: NULL handle for %s", dbgmodules[i].name.c_str() );
			nolib++;
			continue; 
		};

		typedef int (*PFN_MODULEMEMORY)();
		PFN_MODULEMEMORY fpCryModuleGetAllocatedMemory = (PFN_MODULEMEMORY)::GetProcAddress( (HMODULE)dbgmodules[i].handle, "CryModuleGetAllocatedMemory");
		if (fpCryModuleGetAllocatedMemory)
		{
			int allocatedMemory = fpCryModuleGetAllocatedMemory();
			totalUsedInModules += allocatedMemory;
			countedMemoryModules++;
			CryLogAlways("%8d K used in Module %s: ",allocatedMemory/1024,dbgmodules[i].name.c_str() );
		}
		
#ifdef _DEBUG
		typedef void (*PFNUSAGESUMMARY)(ILog *log, const char *, int *);
		typedef void (*PFNCHECKPOINT)();
		PFNUSAGESUMMARY fpu = (PFNUSAGESUMMARY)::GetProcAddress( (HMODULE)dbgmodules[i].handle, "UsageSummary");
		PFNCHECKPOINT fpc = (PFNCHECKPOINT)::GetProcAddress( (HMODULE)dbgmodules[i].handle, "CheckPoint");
		if(fpu && fpc)
		{
			if(checkpoint) fpc();
			else
			{
				extrastats[2] = (int)leaks;
				fpu(log, dbgmodules[i].name.c_str(), extrastats);
				totalal += extrastats[0];
				totalbl += extrastats[1];
			};

		}
		else
		{
			CryLogAlways( "WARNING: <CrySystem> CSystem::DebugStats: could not retrieve function from DLL %s", dbgmodules[i].name.c_str());
			nolib++;
		};
#endif

		typedef HANDLE(*PFNGETDLLHEAP)();
		PFNGETDLLHEAP fpg = (PFNGETDLLHEAP)::GetProcAddress( (HMODULE)dbgmodules[i].handle, "GetDLLHeap");
		if(fpg)
		{
			dbgmodules[i].heap = fpg();
		};
	};

	CryLogAlways("-------------------------------------------------------" );
	CryLogAlways("%8d K Total Memory Allocated in %d Modules",totalUsedInModules/1024,countedMemoryModules );
#ifdef _DEBUG
	CryLogAlways("$8GRAND TOTAL: %d k, %d blocks (%d dlls not included)", totalal/1024, totalbl, nolib);
	CryLogAlways("estimated debugalloc overhead: between %d k and %d k", totalbl*36/1024, totalbl*72/1024);
#endif

	//////////////////////////////////////////////////////////////////////////
	// Get HeapQueryInformation pointer if on windows XP.
	//////////////////////////////////////////////////////////////////////////
	typedef BOOL (WINAPI *FUNC_HeapQueryInformation)( HANDLE,HEAP_INFORMATION_CLASS,PVOID,SIZE_T,PSIZE_T );
	FUNC_HeapQueryInformation pFnHeapQueryInformation = NULL;
	HMODULE hKernelInstance = CryLoadLibrary( "Kernel32.dll" );
	if (hKernelInstance)
	{
		pFnHeapQueryInformation = (FUNC_HeapQueryInformation)(::GetProcAddress(hKernelInstance,"HeapQueryInformation" ));
	}
	//////////////////////////////////////////////////////////////////////////

	const int MAXHANDLES = 100;
	HANDLE handles[MAXHANDLES];
	int realnumh = GetProcessHeaps(MAXHANDLES, handles);
	char hinfo[1024];
	PROCESS_HEAP_ENTRY phe;
	CryLogAlways("$6--------------------- dump of windows heaps ---------------------");
	int nTotalC = 0, nTotalCP = 0, nTotalUC = 0, nTotalUCP = 0, totalo = 0;
	for(int i = 0; i<realnumh; i++)
	{
		HANDLE hHeap = handles[i];
		HeapCompact(hHeap, 0);
		hinfo[0] = 0;
		if (pFnHeapQueryInformation)
		{
			pFnHeapQueryInformation(hHeap, HeapCompatibilityInformation, hinfo, 1024, NULL);
		}
		else
		{
			for(int m = 0; m < (int)(dbgmodules.size()); m++)
			{
				if(dbgmodules[m].heap==handles[i]) strcpy(hinfo, dbgmodules[m].name.c_str());
			}
		}
		phe.lpData = NULL;
		int nCommitted = 0, nUncommitted = 0, nOverhead = 0;
		int nCommittedPieces = 0, nUncommittedPieces = 0;
		int nPrevRegionIndex = -1;
		while(HeapWalk(hHeap, &phe))
		{	
			if (phe.wFlags & PROCESS_HEAP_REGION)
			{
				assert (++nPrevRegionIndex == phe.iRegionIndex);
				nCommitted += phe.Region.dwCommittedSize;
				nUncommitted +=  phe.Region.dwUnCommittedSize;
				assert (phe.cbData == 0 || (phe.wFlags & PROCESS_HEAP_ENTRY_BUSY));
			}
			else
				if (phe.wFlags & PROCESS_HEAP_UNCOMMITTED_RANGE)
					nUncommittedPieces += phe.cbData;
				else
					//if (phe.wFlags & PROCESS_HEAP_ENTRY_BUSY)
					nCommittedPieces += phe.cbData;


			{
				/*
				MEMORY_BASIC_INFORMATION mbi;
				if (VirtualQuery(phe.lpData, &mbi,sizeof(mbi)) == sizeof(mbi))
				{
				if (mbi.State == MEM_COMMIT)
				nCommittedPieces += phe.cbData;//mbi.RegionSize;
				//else
				//	nUncommitted += mbi.RegionSize;
				}
				else
				nCommittedPieces += phe.cbData;
				*/
			}

			nOverhead += phe.cbOverhead;
		};
		int nCommittedMin = min(nCommitted, nCommittedPieces);
		int nCommittedMax = max(nCommitted, nCommittedPieces);
		CryLogAlways("* heap %8x: %6d (or ~%6d) K in use, %6d..%6d K uncommitted, %6d K overhead (%s)\n",
			handles[i], nCommittedPieces/1024, nCommitted/1024, nUncommittedPieces/1024, nUncommitted/1024, nOverhead/1024, hinfo);

		nTotalC += nCommitted;
		nTotalCP += nCommittedPieces;
		nTotalUC += nUncommitted;
		nTotalUCP += nUncommittedPieces;
		totalo += nOverhead;
	};
	CryLogAlways("$6----------------- total in heaps: %d megs committed (win stats shows ~%d) (%d..%d uncommitted, %d k overhead) ---------------------", nTotalCP/1024/1024, nTotalC/1024/1024, nTotalUCP/1024/1024, nTotalUC/1024/1024, totalo/1024);

#endif //WIN32
};

#ifdef WIN32
struct DumpHeap32Stats
{
	DumpHeap32Stats():dwFree(0),dwMoveable(0),dwFixed(0),dwUnknown(0)
	{}
	void operator += (const DumpHeap32Stats& right)
	{
		dwFree += right.dwFree;
		dwMoveable += right.dwMoveable;
		dwFixed += right.dwFixed;
		dwUnknown += right.dwUnknown;
	}
	DWORD dwFree;
	DWORD dwMoveable;
	DWORD dwFixed;
	DWORD dwUnknown;
};
static void DumpHeap32 (const HEAPLIST32& hl, DumpHeap32Stats& stats)
{
	HEAPENTRY32 he;
	memset (&he,0, sizeof(he));
	he.dwSize = sizeof(he);

	if (Heap32First (&he, hl.th32ProcessID, hl.th32HeapID))
	{
		DumpHeap32Stats heap;
		do {
			if (he.dwFlags & LF32_FREE)
				heap.dwFree += he.dwBlockSize;
			else
			if (he.dwFlags & LF32_MOVEABLE)
				heap.dwMoveable += he.dwBlockSize;
			else
			if (he.dwFlags & LF32_FIXED)
			{
				heap.dwFixed += he.dwBlockSize;
			}
			else
				heap.dwUnknown += he.dwBlockSize;
		} while(Heap32Next (&he));

		CryLogAlways ("%08X  %6d %6d %6d (%d)", hl.th32HeapID, heap.dwFixed/0x400, heap.dwFree/0x400, heap.dwMoveable/0x400, heap.dwUnknown/0x400);
		stats += heap;
	}
	else
		CryLogAlways ("%08X  empty or invalid");
}

//////////////////////////////////////////////////////////////////////////
class CStringOrder
{
public:
	bool operator () (const char*szLeft, const char* szRight)const {return stricmp(szLeft, szRight) < 0;}
};
typedef std::map<const char*,unsigned,CStringOrder> StringToSizeMap;
void AddSize (StringToSizeMap& mapSS, const char* szString, unsigned nSize)
{
	StringToSizeMap::iterator it = mapSS.find (szString);
	if (it == mapSS.end())
		mapSS.insert (StringToSizeMap::value_type(szString, nSize));
	else
		it->second += nSize;
}

//////////////////////////////////////////////////////////////////////////
const char* GetModuleGroup (const char* szString)
{
	for (unsigned i = 0; i < sizeof(g_szModuleGroups)/sizeof(g_szModuleGroups[0]); ++i)
		if (stricmp(szString, g_szModuleGroups[i][0]) == 0)
			return g_szModuleGroups[i][1];
	return "Other";
}

//////////////////////////////////////////////////////////////////////////
void CSystem::GetExeSizes (ICrySizer* pSizer, MemStatsPurposeEnum nPurpose)
{
	HANDLE hSnapshot;
	hSnapshot = CreateToolhelp32Snapshot (TH32CS_SNAPMODULE, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
	{
		CryLogAlways ("Cannot get the module snapshot, error code %d", GetLastError());
		return;
	}

	DWORD dwProcessID = GetCurrentProcessId();

	MODULEENTRY32 me;
	memset (&me, 0, sizeof(me));
	me.dwSize = sizeof(me);

	if (Module32First (hSnapshot, &me))
	{
		// the sizes of each module group
		StringToSizeMap mapGroupSize; 
		DWORD dwTotalModuleSize = 0;
		do
		{
			dwProcessID = me.th32ProcessID;
			const char* szGroup = GetModuleGroup (me.szModule);
			SIZER_COMPONENT_NAME(pSizer, szGroup);
			if (nPurpose == nMSP_ForDump)
			{
				SIZER_COMPONENT_NAME(pSizer, me.szModule);
				pSizer->AddObject(me.modBaseAddr, me.modBaseSize);
			}
			else
				pSizer->AddObject(me.modBaseAddr, me.modBaseSize);
		}
		while(Module32Next (hSnapshot, &me));
	}
	else
		CryLogAlways ("No modules to dump");

	CloseHandle (hSnapshot);
}

#endif

//////////////////////////////////////////////////////////////////////////
void CSystem::DumpWinHeaps()
{
#ifdef WIN32
	//
	// Retrieve modules and log them; remember the process id

	HANDLE hSnapshot;
	hSnapshot = CreateToolhelp32Snapshot (TH32CS_SNAPMODULE, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
	{
		CryLogAlways ("Cannot get the module snapshot, error code %d", GetLastError());
		return;
	}

	DWORD dwProcessID = GetCurrentProcessId();

	MODULEENTRY32 me;
	memset (&me, 0, sizeof(me));
	me.dwSize = sizeof(me);

	if (Module32First (hSnapshot, &me))
	{
		// the sizes of each module group
		StringToSizeMap mapGroupSize; 
		DWORD dwTotalModuleSize = 0;
		CryLogAlways ("base        size  module");
		do
		{
			dwProcessID = me.th32ProcessID;
			const char* szGroup = GetModuleGroup (me.szModule);
			CryLogAlways ("%08X %8X  %25s   - %s", me.modBaseAddr, me.modBaseSize, me.szModule, stricmp(szGroup,"Other")?szGroup:"");
			dwTotalModuleSize += me.modBaseSize;
			AddSize (mapGroupSize, szGroup, me.modBaseSize);
		}
		while(Module32Next (hSnapshot, &me));

		CryLogAlways ("------------------------------------");
		for (StringToSizeMap::iterator it = mapGroupSize.begin(); it != mapGroupSize.end(); ++it)
			CryLogAlways ("         %6.3f Mbytes  - %s", double(it->second)/0x100000, it->first);
		CryLogAlways ("------------------------------------");
		CryLogAlways ("         %6.3f Mbytes  - TOTAL", double(dwTotalModuleSize)/0x100000);
		CryLogAlways ("------------------------------------");
	}
	else
		CryLogAlways ("No modules to dump");

	CloseHandle (hSnapshot);

	//
	// Retrieve the heaps and dump each of them with a special function

	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
	{
		CryLogAlways ("Cannot get the heap LIST snapshot, error code %d", GetLastError());
		return;
	}

	HEAPLIST32 hl;
	memset (&hl, 0, sizeof(hl));
	hl.dwSize = sizeof(hl);

	CryLogAlways ("__Heap__   fixed   free   move (unknown)");
	if (Heap32ListFirst (hSnapshot, &hl))
	{
		DumpHeap32Stats stats;
		do {
			DumpHeap32 (hl, stats);
		} while(Heap32ListNext (hSnapshot,&hl));

		CryLogAlways ("-------------------------------------------------");
		CryLogAlways ("$6          %6.3f %6.3f %6.3f (%.3f) Mbytes", double(stats.dwFixed)/0x100000, double(stats.dwFree)/0x100000, double(stats.dwMoveable)/0x100000, double(stats.dwUnknown)/0x100000);
		CryLogAlways ("-------------------------------------------------");
	}
	else
		CryLogAlways ("No heaps to dump");

	CloseHandle(hSnapshot);
#endif
}

// Make system error message string
//////////////////////////////////////////////////////////////////////////
//! \return pointer to the null terminated error string or 0
static const char* GetLastSystemErrorMessage()
{
#ifdef WIN32
  DWORD dwError = GetLastError();

	static char szBuffer[512]; // function will return pointer to this buffer

  if(dwError)
  {
//#ifdef _XBOX

    LPVOID lpMsgBuf=0;
    
		if(FormatMessage( 
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM | 
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        (LPTSTR) &lpMsgBuf,
        0,
        NULL))
		{
	    strncpy(szBuffer, (char*)lpMsgBuf, sizeof(szBuffer));
		  LocalFree(lpMsgBuf);
		}
		else return 0;

//#else

	  //sprintf(szBuffer, "Win32 ERROR: %i", dwError);
    //OutputDebugString(szBuffer);

//#endif

    return szBuffer;
  }
#else
	return 0;

#endif //WIN32

  return 0;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::Error( const char *format,... )
{
	// format message
	va_list	ArgList;
	char szBuffer[MAX_WARNING_LENGTH];
	const char *sPrefix = "";
	strcpy( szBuffer,sPrefix );
	va_start(ArgList, format);
	_vsnprintf(szBuffer+strlen(sPrefix), MAX_WARNING_LENGTH-strlen(sPrefix), format, ArgList);
	va_end(ArgList);

	// get system error message before any attempt to write into log
  const char * szSysErrorMessage = GetLastSystemErrorMessage();

	CryLogAlways( "=============================================================================" );
	CryLogAlways( "*ERROR" );
	CryLogAlways( "=============================================================================" );
	// write both messages into log
	CryLogAlways( szBuffer );

	if (szSysErrorMessage)
		CryLogAlways( "<CrySystem> Last System Error: %s",szSysErrorMessage );

	bool bHandled = false;
	if (GetUserCallback())
		bHandled = GetUserCallback()->OnError( szBuffer );

	assert(szBuffer[0]>=' ');
//	strcpy(szBuffer,szBuffer+1);	// remove verbosity tag since it is not supported by ::MessageBox

	LogSystemInfo();

	CollectMemStats(0, nMSP_ForCrashLog);

#ifdef WIN32
	if (!bHandled && !g_cvars.sys_no_crash_dialog)
	{
		ICVar *pFullscreen = (gEnv && gEnv->pConsole) ? gEnv->pConsole->GetCVar("r_Fullscreen") : 0;
		if (pFullscreen && pFullscreen->GetIVal() != 0 && gEnv->pRenderer && gEnv->pRenderer->GetHWND())
		{
			::ShowWindow((HWND)gEnv->pRenderer->GetHWND(),SW_MINIMIZE);
		}
		::MessageBox( NULL,szBuffer,"CryEngine Error",MB_OK|MB_ICONERROR|MB_SYSTEMMODAL );
	}
	// Dump callstack.
	DebugCallStack::instance()->LogCallstack();
#endif
#ifndef PS2
  ::OutputDebugString(szBuffer);
#endif	//PS2

	// try to shutdown renderer (if we crash here - error message will already stay in the log)
	//if(m_env.pRenderer)
		//m_env.pRenderer->ShutDown();

	// app can not continue
#ifdef _DEBUG
#if defined(WIN32) && !defined(WIN64)
	DEBUG_BREAK;
#endif
#else
	exit(1);
#endif
}

// tries to log the call stack . for DEBUG purposes only
//////////////////////////////////////////////////////////////////////////
void CSystem::LogCallStack()
{
#ifdef WIN32
	DebugCallStack::instance()->LogCallstack();
#endif
}

//////////////////////////////////////////////////////////////////////////
void CSystem::debug_GetCallStack( const char **pFunctions,int &nCount )
{
	int nMaxCount = nCount;
	nCount = 0;
#ifdef WIN32
	DebugCallStack::instance()->CollectCurrentCallStack();
	static std::vector<string> funcs;
	funcs.clear();
	DebugCallStack::instance()->getCallStack( funcs );

	nCount = ((int)funcs.size()) - 1; // Remove this function from the call stack.
	if (nCount < 0)
		nCount = 0;
	for (int i = 0; i < nMaxCount && i < nCount; i++)
	{
		pFunctions[i] = funcs[i+1].c_str();
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CSystem::debug_LogCallStack( int nMaxFuncs,int nFlags )
{
	if (nMaxFuncs > 32)
		nMaxFuncs = 32;
	// Print call stack for each find.
	const char *funcs[32];
	int nCount = nMaxFuncs;
	int nCurFrame = 0;
	if (m_env.pRenderer)
		nCurFrame = (int)m_env.pRenderer->GetFrameID(false);
	CryLogAlways( "    ----- CallStack (Frame: %d) -----",nCurFrame );
	GetISystem()->debug_GetCallStack( funcs,nCount );
	for (int i = 1; i < nCount; i++) // start from 1 to skip this function.
	{
		CryLogAlways( "    %02d) %s",i,funcs[i] );
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ShareShaderCombinations()
{

#ifdef WIN32

	// Make Sure shader combinations file is closed.
	if (m_env.pRenderer)
		m_env.pRenderer->EF_Query(EFQ_CloseShaderCombinations);

	STARTUPINFO si;
	memset( &si,0, sizeof(si) );
	si.cb = sizeof(si);
	si.dwX = 0;
	si.dwY = 0;
	si.wShowWindow = SW_HIDE;
	si.dwFlags =  STARTF_USESHOWWINDOW;

	char sShadersPath[MAX_PATH];
	const char *sShadersList = gEnv->pCryPak->AdjustFileName( "%USER%/Shaders/Cache/ShaderList.txt",sShadersPath,0 );

	char szCmdLine[_MAX_PATH];	
	strcpy(szCmdLine,"Tools\\sc_share.exe \"");
	strcat(szCmdLine,sShadersList);
	strcat(szCmdLine,"\"");

	PROCESS_INFORMATION pi;
	memset( &pi,0,sizeof(pi) );
	BOOL bRes = CreateProcess( NULL, // No module name (use command line). 
		szCmdLine,        // Command line. 
		NULL,             // Process handle not inheritable. 
		NULL,             // Thread handle not inheritable. 
		TRUE,             // Set handle inheritance to TRUE. 
		0, // No creation flags. 
		NULL,             // Use parent's environment block. 
		NULL/*szFolderName*/,     // Set starting directory. 
		&si,              // Pointer to STARTUPINFO structure.
		&pi );            // Pointer to PROCESS_INFORMATION structure.



#endif //WIN32
}

//////////////////////////////////////////////////////////////////////////
// Support relaunching for windows media center edition.
//////////////////////////////////////////////////////////////////////////
#if defined(WIN32) && !defined(XENON)
#if(_WIN32_WINNT < 0x0501)
#define SM_MEDIACENTER          87
#endif
bool CSystem::ReLaunchMediaCenter()
{
	// Skip if not running on a Media Center
	if( GetSystemMetrics( SM_MEDIACENTER ) == 0 ) 
		return false;

	// Get the path to Media Center
	char szExpandedPath[MAX_PATH];
	if( !ExpandEnvironmentStrings( "%SystemRoot%\\ehome\\ehshell.exe", szExpandedPath, MAX_PATH) )
		return false;

	// Skip if ehshell.exe doesn't exist
	if( GetFileAttributes( szExpandedPath ) == 0xFFFFFFFF )
		return false;

	// Launch ehshell.exe 
	INT_PTR result = (INT_PTR)ShellExecute( NULL, TEXT("open"), szExpandedPath, NULL, NULL, SW_SHOWNORMAL);
	return (result > 32);
}
#else
bool CSystem::ReLaunchMediaCenter()
{
	return false;
}
#endif //defined(WIN32) && !defined(XENON)

//////////////////////////////////////////////////////////////////////////
#if defined(WIN32) && !defined(XENON)
void CSystem::LogSystemInfo()
{
	//////////////////////////////////////////////////////////////////////
	// Write the system informations to the log
	//////////////////////////////////////////////////////////////////////

	char szBuffer[1024];
	char szProfileBuffer[128];
	char szLanguageBuffer[64];
	//char szCPUModel[64];
	char *pChar = 0;

	MEMORYSTATUSEX MemoryStatus;
	MemoryStatus.dwLength = sizeof(MemoryStatus);

	DEVMODE DisplayConfig;
	OSVERSIONINFO OSVerInfo;
	OSVerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	// log Windows type
	Win32SysInspect::GetOS(m_env.pi.winVer, m_env.pi.win64Bit, szBuffer, sizeof(szBuffer));
	CryLogAlways(szBuffer);

	// log user name
	CryLog("User name: \"%s\"",GetUserName());

	// log system language
	GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_SENGLANGUAGE, szLanguageBuffer, sizeof(szLanguageBuffer));
	sprintf(szBuffer, "System language: %s", szLanguageBuffer);
	CryLogAlways(szBuffer);

	// log Windows directory	
	GetWindowsDirectory(szBuffer, sizeof(szBuffer));
	string str = "Windows Directory: \"";
	str += szBuffer;
	str += "\"";
	CryLogAlways("%s", str.c_str());

	// prerequisites
	CryLogAlways("Prerequisites...");
	m_env.pi.vistaKB940105Required = Win32SysInspect::IsVistaKB940105Required();
	CryLogAlways("* Installation of KB940105 hotfix required: %s", m_env.pi.vistaKB940105Required ? "yes!" : "no! (either not needed or already installed)");

	//////////////////////////////////////////////////////////////////////
	// Send system time & date
	//////////////////////////////////////////////////////////////////////

	str = "Local time is ";
	_strtime(szBuffer);
	str += szBuffer;
	str += " ";
	_strdate(szBuffer);
	str += szBuffer;
	sprintf(szBuffer, ", system running for %d minutes", GetTickCount() / 60000);
	str += szBuffer;
	CryLogAlways("%s", str.c_str());

	//////////////////////////////////////////////////////////////////////
	// Send system CPU informations
	//////////////////////////////////////////////////////////////////////

	/*
	GetCPUModel(szCPUModel);
	#ifdef _DEBUG
	sprintf(szBuffer, "System CPU is an %s running at %d Mhz", 
	szCPUModel, GetCPUSpeed());
	#else
	sprintf(szBuffer, "System CPU is an %s, frequency only measured in debug versions", 
	szCPUModel);
	#endif
	LogLine(szBuffer);
	*/

	//////////////////////////////////////////////////////////////////////
	// Send system memory status
	//////////////////////////////////////////////////////////////////////

	GlobalMemoryStatusEx(&MemoryStatus);
	sprintf(szBuffer, "%I64dMB physical memory installed, %I64dMB available, %I64dMB virtual memory installed, %ld percent of memory in use",
		MemoryStatus.ullTotalPhys  / 1048576 + 1,
		MemoryStatus.ullAvailPhys / 1048576,
		MemoryStatus.ullTotalVirtual / 1048576,
		MemoryStatus.dwMemoryLoad );
	CryLogAlways(szBuffer);

	IMemoryManager::SProcessMemInfo memCounters;
	GetISystem()->GetIMemoryManager()->GetProcessMemInfo( memCounters );

	uint64 PagefileUsage = memCounters.PagefileUsage;
	uint64 PeakPagefileUsage = memCounters.PeakPagefileUsage;
	uint64 WorkingSetSize = memCounters.WorkingSetSize;
	sprintf(szBuffer, "PageFile usage: %I64dMB, Working Set: %I64dMB, Peak PageFile usage: %I64dMB,",
		(uint64)PagefileUsage / (1024*1024),
		(uint64)WorkingSetSize / (1024*1024),
		(uint64)PeakPagefileUsage / (1024*1024) );
	CryLogAlways(szBuffer);

	//////////////////////////////////////////////////////////////////////
	// Send display settings
	//////////////////////////////////////////////////////////////////////

	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &DisplayConfig);
	GetPrivateProfileString("boot.description", "display.drv", 
		"(Unknown graphics card)", szProfileBuffer, sizeof(szProfileBuffer), 
		"system.ini");
	sprintf(szBuffer, "Current display mode is %dx%dx%d, %s",
		DisplayConfig.dmPelsWidth, DisplayConfig.dmPelsHeight,
		DisplayConfig.dmBitsPerPel, szProfileBuffer);
	CryLogAlways(szBuffer);

	//////////////////////////////////////////////////////////////////////
	// Send input device configuration
	//////////////////////////////////////////////////////////////////////

	str = "";
	// Detect the keyboard type
	switch (GetKeyboardType(0))
	{
	case 1:
		str = "IBM PC/XT (83-key)";
		break;
	case 2:
		str = "ICO (102-key)";
		break;
	case 3:
		str = "IBM PC/AT (84-key)";
		break;
	case 4:
		str = "IBM enhanced (101/102-key)";
		break;
	case 5:
		str = "Nokia 1050";
		break;
	case 6:
		str = "Nokia 9140";
		break;
	case 7:
		str = "Japanese";
		break;
	default:
		str = "Unknown";
		break;
	}

	// Any mouse attached ?
	if (!GetSystemMetrics(SM_MOUSEPRESENT))
		CryLogAlways("%s", (str + " keyboard and no mouse installed").c_str());
	else
	{
		sprintf(szBuffer, " keyboard and %i+ button mouse installed", 
			GetSystemMetrics(SM_CMOUSEBUTTONS));
		CryLogAlways("%s", (str + szBuffer).c_str());
	}

	CryLogAlways("--------------------------------------------------------------------------------");
}
#else
void CSystem::LogSystemInfo()
{
}
#endif

//////////////////////////////////////////////////////////////////////////
void CSystem::ChangeUserPath( const char *sUserPath )
{
#if defined(WIN32) && !defined(XENON)
	m_engineFolders[ENGINE_FOLDER_USER] = sUserPath;
	char szMyDocumentsPath[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPath(NULL,CSIDL_PERSONAL|CSIDL_FLAG_CREATE,NULL,0,szMyDocumentsPath))) 
	{
		string mydocs = string(szMyDocumentsPath) + "\\" + m_engineFolders[ENGINE_FOLDER_USER];
		mydocs = PathUtil::RemoveSlash(mydocs);
		mydocs = PathUtil::ToDosPath(mydocs);
		m_env.pCryPak->MakeDir(mydocs.c_str());
		m_env.pCryPak->SetAlias("%USER%",mydocs.c_str(),true);
	}
	else
	{
		// Add %USER% alias.
		m_env.pCryPak->SetAlias("%USER%",".",true);
	}
#endif
#if defined(PS3)
	m_engineFolders[ENGINE_FOLDER_USER] = sUserPath;
	m_env.pCryPak->SetAlias("%USER%",sUserPath,true);
#endif
#if defined(LINUX)
	const char *chop = "My Games/";
	const size_t choplen = strlen(chop);
	if (strncmp(chop, sUserPath, choplen) == 0)
		sUserPath += choplen;
	m_engineFolders[ENGINE_FOLDER_USER] = sUserPath;
	const char *home = getenv("HOME");
	if (home != NULL)
	{
		string mydocs = string(home) + "/." + m_engineFolders[ENGINE_FOLDER_USER];
		mydocs = PathUtil::RemoveSlash(mydocs);
		m_env.pCryPak->MakeDir(mydocs.c_str());
		m_env.pCryPak->SetAlias("%USER%",mydocs.c_str(),true);
		m_engineFolders[ENGINE_FOLDER_USER] = mydocs.c_str();
	}
	else
	{
		// Add %USER% alias.
		m_env.pCryPak->SetAlias("%USER%",".",true);
		m_engineFolders[ENGINE_FOLDER_USER] = ".";
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CSystem::DetectGameFolderAccessRights()
{
	// This code is trying to figure out if the current folder we are now running under have write access.
	// By default assume folder is not writable.
	// If folder is writable game.log is saved there, otherwise it is saved in user documents folder.

#if defined(WIN32) && !defined(XENON)

	DWORD DesiredAccess = FILE_GENERIC_WRITE;
	DWORD GrantedAccess = 0;
	DWORD dwRes = 0;
	PACL pDACL = NULL;
	PSECURITY_DESCRIPTOR pSD = NULL;
	HANDLE hClientToken = 0;
	PRIVILEGE_SET PrivilegeSet;
	DWORD PrivilegeSetLength = sizeof(PrivilegeSet);
	BOOL bAccessStatus = FALSE;

	// Get a pointer to the existing DACL.
	dwRes = GetNamedSecurityInfo(".", SE_FILE_OBJECT,
		DACL_SECURITY_INFORMATION|OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION,
		NULL, NULL, &pDACL, NULL, &pSD);
	
	if (ERROR_SUCCESS != dwRes)
	{
		// 
		assert(0);
	}

	if (!ImpersonateSelf(SecurityIdentification))
		return;
	if (!OpenThreadToken( GetCurrentThread(),TOKEN_QUERY,TRUE,&hClientToken ) && hClientToken != 0)
		return;

	GENERIC_MAPPING GenMap;
	GenMap.GenericRead = FILE_GENERIC_READ;
	GenMap.GenericWrite = FILE_GENERIC_WRITE;
	GenMap.GenericExecute = FILE_GENERIC_EXECUTE;
	GenMap.GenericAll = FILE_ALL_ACCESS;

	MapGenericMask( &DesiredAccess,&GenMap );
	if (!AccessCheck( pSD,hClientToken,DesiredAccess,&GenMap,&PrivilegeSet,&PrivilegeSetLength,&GrantedAccess,&bAccessStatus ))
	{
		RevertToSelf();
		CloseHandle(hClientToken);
		return;
	}
	CloseHandle(hClientToken);
	RevertToSelf();

	if (bAccessStatus)
	{
		m_bGameFolderWritable = true;
	}
#endif //WIN32,XENON
}

//////////////////////////////////////////////////////////////////////////
void CSystem::EnableFloatExceptions( int type )
{
#if defined(WIN32)

#if defined(WIN32) && !defined(XENON) && !defined(WIN64)
	
	// Optimization
	// Enable DAZ/FZ
	// Denormals Are Zeros
	// Flush-to-Zero
	_controlfp(_DN_FLUSH, _MCW_DN);

#endif //#if defined(WIN32) && !defined(XENON) && !defined(WIN64)

	if (g_cvars.sys_float_exceptions == 0) // mask all floating exceptions off.
		_controlfp( _EM_INEXACT|_EM_UNDERFLOW|_EM_OVERFLOW|_EM_INVALID|_EM_DENORMAL|_EM_ZERODIVIDE,_MCW_EM );

	// enable just the most important fp-exceptions.
	if (g_cvars.sys_float_exceptions == 1)
	{
		_controlfp( _EM_INEXACT|_EM_UNDERFLOW|_EM_OVERFLOW,_MCW_EM ); // Enable floating point exceptions.
	}

	// enable ALL floating point exceptions.
	if (g_cvars.sys_float_exceptions == 2)
		_controlfp( _EM_INEXACT,_MCW_EM ); 

#endif //WIN32
}
