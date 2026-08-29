////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   FrameProfileSystem.cpp
//  Version:     v1.00
//  Created:     24/6/2003 by Timur,Sergey,Wouter.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FrameProfileSystem.h"

#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CryInput/IInput.h"
#include "CryCommon/CryCore/StlUtils.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/IHardwareMouse.h"

#include "Sampler.h"
#include "CryThread.h"
#include "Timer.h"

#if defined(LINUX)
#include "CryCommon/CryCore/platform.h"
#endif

#ifdef WIN32
#include <psapi.h>
static HMODULE hPsapiModule;
typedef BOOL (WINAPI *FUNC_GetProcessMemoryInfo)( HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD );
static FUNC_GetProcessMemoryInfo pfGetProcessMemoryInfo;
static bool m_bNoPsapiDll;
#endif

#ifdef USE_FRAME_PROFILER

#define MAX_PEAK_PROFILERS 20
#define MAX_ABSOLUTEPEAK_PROFILERS 100
//! Peak tolerance in milliseconds.
#define PEAK_TOLERANCE 10.0f

//////////////////////////////////////////////////////////////////////////
// CFrameProfilerTimer static variable.
//////////////////////////////////////////////////////////////////////////
CFrameProfileSystem* CFrameProfileSystem::s_pFrameProfileSystem = 0;
bool CFrameProfilerTimer::g_bThreadSafe = false;
int64 CFrameProfilerTimer::g_nTicksPerSecond = 0;
double CFrameProfilerTimer::g_fSecondsPerTick = 0.0;

//////////////////////////////////////////////////////////////////////////
// CFrameProfilerTimer implementation.
//////////////////////////////////////////////////////////////////////////
void CFrameProfilerTimer::Init( bool bThreadSafe ) // called once
{
	if (g_fSecondsPerTick != 0.f && g_bThreadSafe == bThreadSafe)
		return;

	g_bThreadSafe = bThreadSafe;

#if defined(WIN32) || defined(WIN64)
	if (g_bThreadSafe)
	{
		QueryPerformanceFrequency((LARGE_INTEGER*)&g_nTicksPerSecond);
	}
	else
	{
		HKEY hKey;
		unsigned nMhz = 0;
		DWORD dwSize = sizeof(nMhz);
		if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_QUERY_VALUE, &hKey)
			&&ERROR_SUCCESS == RegQueryValueEx (hKey, "~MHz", NULL, NULL, (LPBYTE)&nMhz, &dwSize))
		{
			g_nTicksPerSecond = nMhz * 1000000;
		}
	}
#else
  QueryPerformanceFrequency((LARGE_INTEGER*)&g_nTicksPerSecond);
#endif

	g_fSecondsPerTick = 1.0 / (double)g_nTicksPerSecond;
}

int64 CFrameProfilerTimer::GetTicks()
{
#if defined(WIN32) || defined(WIN64) || defined(XENON)
	if (g_bThreadSafe)
	{
		int64 nTicks;
		QueryPerformanceCounter((LARGE_INTEGER*)&nTicks);
		return nTicks;
	}
#endif
	return CryGetTicks();
}

//////////////////////////////////////////////////////////////////////////
// FrameProfilerSystem Implementation.
//////////////////////////////////////////////////////////////////////////

int CFrameProfileSystem::profile_callstack = 0;
int CFrameProfileSystem::profile_log = 0;

#if !defined(WIN32) && !defined(XENON)
	inline DWORD GetCurrentThreadId() { return 0; }
#endif //WIN32

//////////////////////////////////////////////////////////////////////////
CFrameProfileSystem::CFrameProfileSystem() 
: m_nCurSample(-1), m_ProfilerThreads(GetCurrentThreadId())
{
	s_pFrameProfileSystem = this;
#ifdef WIN32
	hPsapiModule = NULL;
	pfGetProcessMemoryInfo = NULL;
	m_bNoPsapiDll = false;
#endif

	// Allocate space for 1024 profilers.
	m_profilers.reserve( 1024 );
	m_pCurrentCustomSection = 0;
	m_bEnabled = false;
	m_totalProfileTime = 0;
	m_frameStartTime = 0;
	m_frameTime = 0;
	m_frameLostTime = 0;
	m_callOverheadTime = 0;
	m_pRenderer = 0;
	m_displayQuantity = SELF_TIME;

	m_bCollect = false;
	m_nThreadSupport = 0;
	m_bDisplay = false;
	m_bDisplayMemoryInfo = false;
	m_bLogMemoryInfo = false;

	m_peakTolerance = PEAK_TOLERANCE;

	m_pGraphProfiler = 0;
#if defined(PS3) && defined(SUPP_SPU_FRAME_STATS)
	m_timeGraphCurrentPosSPU = 0;
	m_bSPUMode = false;
#endif
	m_timeGraphCurrentPos = 0;
	m_bCollectionPaused = false;
	m_bDrawGraph = false;

	m_selectedRow = -1;
	m_selectedCol = -1;

	m_bEnableHistograms = false;
	m_histogramsMaxPos = 200;
	m_histogramsHeight = 16;
	m_histogramsCurrPos = 0;

	m_bSubsystemFilterEnabled = false;
	m_subsystemFilter = PROFILE_RENDERER;
	m_maxProfileCount = 999;
	m_histogramScale = 100;

	m_bDisplayedProfilersValid = false;
	m_bNetworkProfiling = false;
	//m_pProfilers = &m_netTrafficProfilers;
	m_pProfilers = &m_profilers;

	m_nLastPageFaultCount = 0;
	m_nPagesFaultsLastFrame = 0;
	m_bPageFaultsGraph = false;
	m_nPagesFaultsPerSec = 0;

	m_pSampler = new CSampler;

	//////////////////////////////////////////////////////////////////////////
	// Initialize subsystems list.
	memset( m_subsystems,0,sizeof(m_subsystems) );
	m_subsystems[PROFILE_RENDERER].name = "Renderer";
	m_subsystems[PROFILE_3DENGINE].name = "3DEngine";
	m_subsystems[PROFILE_PARTICLE].name = "Particle";
	m_subsystems[PROFILE_ANIMATION].name = "Animation";
	m_subsystems[PROFILE_AI].name = "AI";
	m_subsystems[PROFILE_ENTITY].name = "Entity";
	m_subsystems[PROFILE_PHYSICS].name = "Physics";
	m_subsystems[PROFILE_SOUND].name = "Sound";
	m_subsystems[PROFILE_MUSIC].name = "Music";
	m_subsystems[PROFILE_GAME].name = "Game";
	m_subsystems[PROFILE_EDITOR].name = "Editor";
	m_subsystems[PROFILE_NETWORK].name = "Network";
	m_subsystems[PROFILE_SYSTEM].name = "System";
	m_subsystems[PROFILE_INPUT].name = "Input";
	m_subsystems[PROFILE_SYNC].name = "Sync";
};

//////////////////////////////////////////////////////////////////////////
CFrameProfileSystem::~CFrameProfileSystem()
{
	g_bProfilerEnabled = false;
	delete m_pSampler;

	// Delete graphs for all frame profilers.
#ifdef WIN32
	if (hPsapiModule)
		::FreeLibrary( hPsapiModule );
	hPsapiModule = NULL;
#endif
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::Init( ISystem *pSystem )
{
	m_pSystem = pSystem;

	CFrameProfilerTimer::Init(m_nThreadSupport >= 2);

	m_pSystem->GetIConsole()->Register( "profile_callstack",&profile_callstack,0,0,"Logs all Call Stacks of the selected profiler function for one frame" );
	m_pSystem->GetIConsole()->Register( "profile_log",&profile_log,0,0,"Logs profiler output" );
}

//////////////////////////////////////////////////////////////////////////
float CFrameProfileSystem::TicksToSeconds (int64 nTime)
{
	return CFrameProfilerTimer::TicksToSeconds(nTime);
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::Done()
{
	for (int i = 0; i < (int)m_profilers.size(); i++)
	{
		if (m_profilers[i])
		{
			SAFE_DELETE( m_profilers[i]->m_pGraph );
			SAFE_DELETE( m_profilers[i]->m_pOfflineHistory );
		}
	}
	for (int i = 0; i < (int)m_netTrafficProfilers.size(); i++)
	{
		if (m_netTrafficProfilers[i])
		{
			SAFE_DELETE( m_netTrafficProfilers[i]->m_pGraph );
			SAFE_DELETE( m_netTrafficProfilers[i]->m_pOfflineHistory );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::SetProfiling(bool on, bool display, char *prefix, ISystem *pSystem)
{
	Enable( on,display );
	m_pPrefix = prefix;
	if(on && m_nCurSample<0)
	{
		m_nCurSample = 0;
		CryLogAlways("Profiling data started (%s), prefix = \"%s\"", display ? "display only" : "tracing", prefix);

		m_frameTimeOfflineHistory.m_selfTime.reserve(1000);
		m_frameTimeOfflineHistory.m_count.reserve(1000);
	}
	else if(!on && m_nCurSample>=0)
	{
		CryLogAlways("Profiling data finished");
		{
#ifdef WIN32
			// find the "frameprofileXX" filename for the file
			char outfilename[32] = "frameprofile.dat";
			// while there is such file already
			for (int i = 0; (GetFileAttributes (outfilename) != INVALID_FILE_ATTRIBUTES) && i < 1000; ++i)
				sprintf (outfilename, "frameprofile%02d.dat", i);

			FILE *f = fopen(outfilename, "wb");
			if(!f)
			{
				CryError("Could not write profiling data to file!");
			}
			else
			{
				int i;
				// Find out how many profilers was active.
				int numProf = 0;

				for (i = 0; i < (int)m_pProfilers->size(); i++)
				{
					CFrameProfiler *pProfiler = (*m_pProfilers)[i];
					if (pProfiler->m_pOfflineHistory)
						numProf++;
				}

				fwrite("FPROFDAT", 8, 1, f);                // magic header, for what its worth
				int version = 2;                            // bump this if any of the format below changes
				fwrite(&version, sizeof(int), 1, f); 

				int numSamples = m_nCurSample;
				fwrite(&numSamples, sizeof(int), 1, f);   // number of samples per group (everything little endian)
				int mapsize = numProf+1; // Plus 1 global.
				fwrite(&mapsize, sizeof(int), 1, f);

				// Write global profiler.
				fwrite( "__frametime",strlen("__frametime")+1,1, f);
				int len = (int)m_frameTimeOfflineHistory.m_selfTime.size();
				assert( len == numSamples );
				for(i = 0; i<len; i++)
				{
					fwrite( &m_frameTimeOfflineHistory.m_selfTime[i], 1, sizeof(int),   f);
					fwrite( &m_frameTimeOfflineHistory.m_count[i], 1, sizeof(short), f);
				};

				// Write other profilers.
				for (i = 0; i < (int)m_pProfilers->size(); i++)
				{
					CFrameProfiler *pProfiler = (*m_pProfilers)[i];
					if (!pProfiler->m_pOfflineHistory)
						continue;

					const char *name = GetFullName(pProfiler);
					//int slen = strlen(name)+1;
					fwrite(name, strlen(name)+1,1,f);
					
					len = (int)pProfiler->m_pOfflineHistory->m_selfTime.size();
					assert( len == numSamples );
					for(int i = 0; i<len; i++)
					{
						fwrite( &pProfiler->m_pOfflineHistory->m_selfTime[i], 1, sizeof(int),   f);
						fwrite( &pProfiler->m_pOfflineHistory->m_count[i], 1, sizeof(short), f);
					};

					// Delete offline data, from profiler.
					SAFE_DELETE( pProfiler->m_pOfflineHistory );
				};
				fclose(f);
				CryLogAlways("Profiling data saved to file '%s'",outfilename);
			};
#endif
		};
		m_frameTimeOfflineHistory.m_selfTime.clear();
		m_frameTimeOfflineHistory.m_count.clear();
		m_nCurSample = -1;
	};
};

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::Enable( bool bCollect,bool bDisplay )
{
	if (m_bEnabled != bCollect)
	{
#ifdef WIN32
		if (bCollect)
		{
			CryLogAlways( "SetThreadAffinityMask to %d",(int)1 );
			if (SetThreadAffinityMask( GetCurrentThread(),1 ) == 0)
			{
				CryLogAlways( "SetThreadAffinityMask Failed" );
			}
		}
		else
		{
			DWORD_PTR mask1,mask2;
			GetProcessAffinityMask( GetCurrentProcess(),&mask1,&mask2 );
			CryLogAlways( "SetThreadAffinityMask to %d",(int)mask1 );
			if (SetThreadAffinityMask( GetCurrentThread(),mask1) == 0)
			{
				CryLogAlways( "SetThreadAffinityMask Failed" );
			}
			if(m_bCollectionPaused)
			{
				if(gEnv->pHardwareMouse)
				{
					gEnv->pHardwareMouse->DecrementCounter();
				}
			}
		}
#endif


		Reset();
	}

	m_bEnabled = bCollect;
	m_bDisplay = bDisplay;
	m_bDisplayedProfilersValid = false;

	gEnv->bProfilerEnabled = bCollect;
}

void CFrameProfileSystem::EnableHistograms( bool bEnableHistograms )
{
	if (m_bEnableHistograms != bEnableHistograms)
	{
		
	}
	m_bEnableHistograms = bEnableHistograms;
	m_bDisplayedProfilersValid = false;
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::StartSampling( int nMaxSamples )
{
	if (m_pSampler)
	{
		m_pSampler->SetMaxSamples( nMaxSamples );
		m_pSampler->Start();
	}
}

//////////////////////////////////////////////////////////////////////////
CFrameProfiler* CFrameProfileSystem::GetProfiler( int index ) const
{
	assert( index >= 0 && index < (int)m_profilers.size() );
	return m_profilers[index];
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::Reset()
{
	gEnv->callbackStartSection = &StartProfilerSection;
	gEnv->callbackEndSection = &EndProfilerSection;

	m_absolutepeaks.clear();
	m_ProfilerThreads.Reset(m_pSystem);
	m_pCurrentCustomSection = 0;
	m_totalProfileTime = 0;
	m_frameStartTime = 0;
	m_frameTime = 0;
	m_frameLostTime = 0;
	m_bCollectionPaused = false;

	int i;
	// Iterate over all profilers update thier history and reset them.
	for (i = 0; i < (int)m_profilers.size(); i++)
	{
		CFrameProfiler *pProfiler = m_profilers[i];
		// Reset profiler.
		pProfiler->m_totalTimeHistory.Clear();
		pProfiler->m_selfTimeHistory.Clear();
		pProfiler->m_countHistory.Clear();
		pProfiler->m_sumTotalTime = 0;
		pProfiler->m_sumSelfTime = 0;
		pProfiler->m_totalTime = 0;
		pProfiler->m_selfTime = 0;
		pProfiler->m_count = 0;
		pProfiler->m_displayedValue = 0;
		pProfiler->m_displayedCurrentValue = 0;
		pProfiler->m_variance = 0;
	}
	// Iterate over all profilers update thier history and reset them.
	for (i = 0; i < (int)m_netTrafficProfilers.size(); i++)
	{
		CFrameProfiler *pProfiler = m_netTrafficProfilers[i];
		// Reset profiler.
		pProfiler->m_totalTimeHistory.Clear();
		pProfiler->m_selfTimeHistory.Clear();
		pProfiler->m_countHistory.Clear();
		pProfiler->m_sumTotalTime = 0;
		pProfiler->m_sumSelfTime = 0;
		pProfiler->m_totalTime = 0;
		pProfiler->m_selfTime = 0;
		pProfiler->m_count = 0;
	}

	{
#ifdef WIN32
		int threadPriority = GetThreadPriority( GetCurrentThread() );
		SetThreadPriority( GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL );
#endif
		// Compute call overhead.
		static CFrameProfiler staticFrameProfiler( GetISystem(),"CallOverhead",PROFILE_SYSTEM );

		bool bPrevState = gEnv->bProfilerEnabled;
		gEnv->bProfilerEnabled = true;
		m_callOverheadTime = 0;
		int64 overheadTime = CFrameProfilerTimer::GetTicks();
		for (int i = 0; i < 1000; i++)
		{
			CFrameProfilerSection frameProfilerSection( &staticFrameProfiler );
		}
		overheadTime = CFrameProfilerTimer::GetTicks() - overheadTime;
		m_callOverheadTime = overheadTime / 1000;
		if (m_callOverheadTime < 0)
			m_callOverheadTime = 0;
		gEnv->bProfilerEnabled = bPrevState;

#ifdef WIN32
		SetThreadPriority( GetCurrentThread(),threadPriority );
#endif
		CryLogAlways( "Call overhead: %ld",(long)m_callOverheadTime );
	}
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::AddFrameProfiler( CFrameProfiler *pProfiler )
{
	// Set default thread id.
	pProfiler->m_threadId = GetMainThreadId();
	if (pProfiler->m_subsystem == PROFILE_NETWORK_TRAFFIC)
	{
		m_netTrafficProfilers.push_back( pProfiler );
	}
	else
	{
		m_profilers.push_back( pProfiler );
	}
}

static int nMAX_THREADED_PROFILERS = 256;

void CFrameProfileSystem::SProfilerThreads::Reset(ISystem* pSystem)
{
	m_aThreadStacks[0].pProfilerSection = 0;
	if (!m_pReservedProfilers)
	{
		// Allocate reserved profilers;
		for (int i = 0; i < nMAX_THREADED_PROFILERS; i++)
		{
			CFrameProfiler* pProf = new CFrameProfiler(pSystem, "");
			pProf->m_pNextThread = m_pReservedProfilers;
			m_pReservedProfilers = pProf;
		}
	}
}

CFrameProfiler* CFrameProfileSystem::SProfilerThreads::NewThreadProfiler(CFrameProfiler* pMainProfiler, TThreadId nThreadId)
{
	// Create new profiler for thread.
	WriteLock lock(m_lock);
	if (!m_pReservedProfilers)
		return 0;
	CFrameProfiler* pProfiler = m_pReservedProfilers;
	m_pReservedProfilers = pProfiler->m_pNextThread;

	// Init.
	memset(pProfiler, 0, sizeof(*pProfiler));
	pProfiler->m_name = pMainProfiler->m_name;
	pProfiler->m_subsystem = pMainProfiler->m_subsystem;
	pProfiler->m_pISystem = pMainProfiler->m_pISystem;
	pProfiler->m_threadId = nThreadId;

	// Insert in frame profiler list.
	pProfiler->m_pNextThread = pMainProfiler->m_pNextThread;
	pMainProfiler->m_pNextThread = pProfiler;

	return pProfiler;
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::StartProfilerSection( CFrameProfilerSection *pSection )
{
	// Find thread instance to collect profiles in.
	TThreadId nThreadId = GetCurrentThreadId();
	if (nThreadId != s_pFrameProfileSystem->GetMainThreadId())
	{
		if (!s_pFrameProfileSystem->m_nThreadSupport)
			return;
		pSection->m_pFrameProfiler = s_pFrameProfileSystem->m_ProfilerThreads.GetThreadProfiler( pSection->m_pFrameProfiler, nThreadId );
		if (!pSection->m_pFrameProfiler)
			return;
	}

	// Push section on stack for current thread.
	s_pFrameProfileSystem->m_ProfilerThreads.PushSection( pSection, nThreadId );
	pSection->m_excludeTime = 0;
	pSection->m_startTime = CFrameProfilerTimer::GetTicks();
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::EndProfilerSection( CFrameProfilerSection *pSection )
{
	int64 endTime = CFrameProfilerTimer::GetTicks();

	if (!s_pFrameProfileSystem->m_nThreadSupport)
	{
		if (GetCurrentThreadId() != s_pFrameProfileSystem->GetMainThreadId())
			return;
	}

	CFrameProfiler *pProfiler = pSection->m_pFrameProfiler;
	if (!pProfiler)
		return;

	assert(GetCurrentThreadId() == pProfiler->m_threadId);

	int64 totalTime = endTime - pSection->m_startTime;
	int64 selfTime = totalTime - pSection->m_excludeTime;
	if (totalTime < 0)
		totalTime = 0;
	if (selfTime < 0)
		selfTime = 0;

/*
	if(pProfiler->m_count!=0 && (pProfiler->m_count%4000)==0)
	if(strcmp(pProfiler->m_name,"CryFree")==0 || strcmp(pProfiler->m_name,"CryMalloc")==0)
	{
		float fVal = TranslateToDisplayValue(pProfiler->m_totalTime);

		char str[256];

		sprintf(str,"EndProfilerSection %d %s %f\n",pProfiler->m_count,pProfiler->m_name,fVal);
		OutputDebugString(str);

		if(pProfiler->m_count>=8000)
		{
			OutputDebugString("-------------------------------------- >8000\n");
		}
	}
*/
	pProfiler->m_count++;
	pProfiler->m_selfTime += selfTime;
	pProfiler->m_totalTime += totalTime;

	s_pFrameProfileSystem->m_ProfilerThreads.PopSection( pSection, pProfiler->m_threadId );
	if (pSection->m_pParent)
	{
		// If we have parent, add this counter total time to parent exclude time.
		pSection->m_pParent->m_excludeTime += totalTime + s_pFrameProfileSystem->m_callOverheadTime;
		if (!pProfiler->m_pParent && pSection->m_pParent->m_pFrameProfiler)
		{
			pSection->m_pParent->m_pFrameProfiler->m_bHaveChildren = true;
			pProfiler->m_pParent = pSection->m_pParent->m_pFrameProfiler;
		}
	}
	else
		pProfiler->m_pParent = 0;

	if (profile_callstack)
	{
		if (pProfiler == s_pFrameProfileSystem->m_pGraphProfiler)
		{
			float fMillis = CFrameProfilerTimer::TicksToMilliseconds(totalTime);
			CryLogAlways( "Function Profiler: %s  (time=%.2fms)",(const char*)s_pFrameProfileSystem->GetFullName(pSection->m_pFrameProfiler), fMillis );
			GetISystem()->debug_LogCallStack();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CFrameProfilerSection const* CFrameProfileSystem::GetCurrentProfilerSection()
{
	// Return current (main-thread) profile section.
	return m_ProfilerThreads.GetMainSection();
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::StartCustomSection( CCustomProfilerSection *pSection )
{
	if (!m_bNetworkProfiling)
		return;

	pSection->m_excludeValue = 0;
	pSection->m_pParent = m_pCurrentCustomSection;
	m_pCurrentCustomSection = pSection;
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::EndCustomSection( CCustomProfilerSection *pSection )
{
	if (!m_bNetworkProfiling || m_bCollectionPaused)
		return;

	int total = *pSection->m_pValue;
	int self = total - pSection->m_excludeValue;

	CFrameProfiler *pProfiler = pSection->m_pFrameProfiler;
	pProfiler->m_count++;
	pProfiler->m_selfTime += self;
	pProfiler->m_totalTime += total;

	m_pCurrentCustomSection = pSection->m_pParent;
	if (m_pCurrentCustomSection)
	{
		// If we have parent, add this counter total time to parent exclude time.
		m_pCurrentCustomSection->m_pFrameProfiler->m_bHaveChildren = true;
		m_pCurrentCustomSection->m_excludeValue += total;
		pProfiler->m_pParent = m_pCurrentCustomSection->m_pFrameProfiler;
	}
	else
		pProfiler->m_pParent = 0;
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::StartFrame()
{
	m_bCollect = m_bEnabled && !m_bCollectionPaused;
	
	if (m_bCollect)
	{
		CFrameProfilerTimer::Init(m_nThreadSupport >= 2);
		m_ProfilerThreads.Reset(m_pSystem);
		m_pCurrentCustomSection = 0;
		m_frameStartTime = CFrameProfilerTimer::GetTicks();
	}
	g_bProfilerEnabled = m_bCollect;
	/*
	if (m_displayQuantity == SUBSYSTEM_INFO)
	{
		for (int i = 0; i < PROFILE_LAST_SUBSYSTEM; i++)
		{
			//m_subsystems[i].selfTime = 0;
		}
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
float CFrameProfileSystem::TranslateToDisplayValue( int64 val )
{
	if (m_bNetworkProfiling)
		return (float)val;
	else
		return CFrameProfilerTimer::TicksToMilliseconds(val);
}

const char* CFrameProfileSystem::GetFullName( CFrameProfiler *pProfiler )
{
	if (pProfiler->m_threadId == GetMainThreadId())
		return pProfiler->m_name.c_str();

	// Add thread name.
	static char sFullName[256];
	const char* sThreadName = CryThreadGetName(pProfiler->m_threadId);
	if (sThreadName)
	{
		_snprintf(sFullName,sizeof(sFullName),"%s@%s", pProfiler->m_name, sThreadName);
		sFullName[sizeof(sFullName)-1] = 0;
	}
	else
	{
		_snprintf(sFullName,sizeof(sFullName),"%s@%d", pProfiler->m_name, pProfiler->m_threadId);
		sFullName[sizeof(sFullName)-1] = 0;
	}
	return sFullName;
}

//////////////////////////////////////////////////////////////////////////
const char* CFrameProfileSystem::GetModuleName( CFrameProfiler *pProfiler )
{
	return m_subsystems[pProfiler->m_subsystem].name.c_str();
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::EndFrame()
{
	if (m_pSampler)
		m_pSampler->Update();

	if (!m_bEnabled)
		m_totalProfileTime = 0;

	if (!m_bEnabled && !m_bNetworkProfiling)
		return;

#if defined(WIN32) || defined(PS3)

	bool bPaused = false;

	bPaused = (GetKeyState(VK_SCROLL) & 1);
	/*
	if (GetISystem()->GetIInput())
	{
		static TKeyName scrollKey("scrolllock");
		bPaused = (GetISystem()->GetIInput()->GetKeyState(scrollKey) & 1);
	}
	*/
	// Will pause or resume collection.
	if (bPaused != m_bCollectionPaused)
	{
		if (bPaused)
		{
			// Must be paused.
			if(gEnv->pHardwareMouse)
			{
				gEnv->pHardwareMouse->IncrementCounter();
			}
		}
		else
		{
			// Must be resumed.
			if(gEnv->pHardwareMouse)
			{
				gEnv->pHardwareMouse->DecrementCounter();
			}
		}
	}
	if (m_bCollectionPaused != bPaused)
	{
		m_bDisplayedProfilersValid = false;
	}
	m_bCollectionPaused = bPaused;
#endif

	if (m_bCollectionPaused || (!m_bCollect && !m_bNetworkProfiling))
		return;

	FUNCTION_PROFILER_FAST( GetISystem(),PROFILE_SYSTEM,g_bProfilerEnabled );

	float smoothTime = CFrameProfilerTimer::TicksToSeconds(m_totalProfileTime);
	float smoothFactor = 1.f - m_pSystem->GetITimer()->GetProfileFrameBlending(&smoothTime);

	int64 endTime = CFrameProfilerTimer::GetTicks();
	m_frameTime = endTime - m_frameStartTime;
	m_totalProfileTime += m_frameTime;

	//////////////////////////////////////////////////////////////////////////
	// Lets see how many page faults we got.
	//////////////////////////////////////////////////////////////////////////
#if defined(WIN32) && !defined(WIN64)

	// PSAPI is not supported on window9x
	// so, don't use it
	if (!m_bNoPsapiDll)
	{
		// Load psapi dll.
		if (!pfGetProcessMemoryInfo)
		{
			hPsapiModule = ::LoadLibrary( "psapi.dll" );
			if (hPsapiModule)
			{
				pfGetProcessMemoryInfo = (FUNC_GetProcessMemoryInfo)(::GetProcAddress(hPsapiModule,"GetProcessMemoryInfo" ));
			}
			else
				m_bNoPsapiDll = true;
		}
		if (pfGetProcessMemoryInfo)
		{
			PROCESS_MEMORY_COUNTERS pc;
			pfGetProcessMemoryInfo( GetCurrentProcess(),&pc,sizeof(pc) );
			m_nPagesFaultsLastFrame = (int)(pc.PageFaultCount - m_nLastPageFaultCount);
			m_nLastPageFaultCount = pc.PageFaultCount;
			static float fLastPFTime = 0;
			static int nPFCounter = 0;
			nPFCounter += m_nPagesFaultsLastFrame;
			float fCurr = CFrameProfilerTimer::TicksToMilliseconds(endTime);
			if ((fCurr - fLastPFTime) >= 1000)
			{
				fLastPFTime = fCurr;
				m_nPagesFaultsPerSec = nPFCounter;
				nPFCounter = 0;
			}
		}
	}
#endif
	//////////////////////////////////////////////////////////////////////////


	static ICVar* pVarMode = m_pSystem->GetIConsole()->GetCVar("profile_weighting");
	int nWeightMode = pVarMode ? pVarMode->GetIVal() : 0;

	int64 selfAccountedTime = 0;

	m_frameTimeHistory.Add( CFrameProfilerTimer::TicksToMilliseconds(m_frameTime) );
	m_frameTimeLostHistory.Add( CFrameProfilerTimer::TicksToMilliseconds(m_frameLostTime) );

#if defined(PS3) && defined(SUPP_SPU_FRAME_STATS)
	//add SPU peaks
	const float cPeakConvFactor = 1.f/1000.f;
	const float cSPUPeakTolerance = 0.1f;//100 usecs
	const float cWhen = CFrameProfilerTimer::TicksToSeconds(m_totalProfileTime);
	const std::vector<NPPU::SFrameProfileData>::const_iterator cSPUEnd = m_SPUJobFrameStats.end();
	for(std::vector<NPPU::SFrameProfileData>::const_iterator it=m_SPUJobFrameStats.begin();it!=cSPUEnd;++it)
	{
		const NPPU::SFrameProfileData& crProfData = *it;
		//peaks are measured in ms
		float prevValue = crProfData.usecLast * cPeakConvFactor;
		float peakValue = crProfData.usec * cPeakConvFactor;
		if ((peakValue-prevValue) > cSPUPeakTolerance)
		{
			SPeakRecord peak;
			peak.pProfiler = (CFrameProfiler*)crProfData.cpName;//encode name there
			peak.peakValue = peakValue;
			peak.avarageValue = (peakValue + prevValue) * 0.5f;
			peak.count = 1;
			peak.pageFaults = 0;
			peak.when = cWhen;
			if(m_peaksSPU.size() > MAX_PEAK_PROFILERS)
				m_peaksSPU.pop_back();
			m_peaksSPU.insert(m_peaksSPU.begin(),peak);
		}
	}
#endif

	// Iterate over all profilers update thier history and reset them.
	for (int i = 0; i < (int)m_pProfilers->size(); i++)
	{
		CFrameProfiler *pProfiler = (*m_pProfilers)[i];

		// Skip this profiler if its filtered out.
		if (m_bSubsystemFilterEnabled && pProfiler->m_subsystem != m_subsystemFilter)
			continue;
    
		if (pProfiler->m_threadId == GetMainThreadId())
		{
			selfAccountedTime += pProfiler->m_selfTime;
			pProfiler->m_sumTotalTime += pProfiler->m_totalTime;
			pProfiler->m_sumSelfTime += pProfiler->m_selfTime;
		}

		bool bEnablePeaks = nWeightMode == 0;
		float aveValue;
		float currentValue;
		float variance;
		switch ((int)m_displayQuantity)
		{
		case SELF_TIME:
		case PEAK_TIME:
		case COUNT_INFO:
			currentValue = TranslateToDisplayValue(pProfiler->m_selfTime);
			aveValue = pProfiler->m_selfTimeHistory.GetAverage();
			variance = (currentValue - aveValue) * (currentValue - aveValue);
			break;
		case TOTAL_TIME:
			currentValue = TranslateToDisplayValue(pProfiler->m_totalTime);
			aveValue = pProfiler->m_totalTimeHistory.GetAverage();
			variance = (currentValue - aveValue) * (currentValue - aveValue);
			break;
		case SELF_TIME_EXTENDED:
			currentValue = TranslateToDisplayValue(pProfiler->m_selfTime);
			aveValue = pProfiler->m_selfTimeHistory.GetAverage();
			variance = (currentValue - aveValue) * (currentValue - aveValue);
			bEnablePeaks = false;
			break;
		case TOTAL_TIME_EXTENDED:
			currentValue = TranslateToDisplayValue(pProfiler->m_totalTime);
			aveValue = pProfiler->m_totalTimeHistory.GetAverage();
			variance = (currentValue - aveValue) * (currentValue - aveValue);
			bEnablePeaks = false;
			break;
		case SUBSYSTEM_INFO:
			currentValue = (float)pProfiler->m_count;
			aveValue = pProfiler->m_selfTimeHistory.GetAverage();
			variance = (currentValue - aveValue) * (currentValue - aveValue);
			if (pProfiler->m_subsystem < PROFILE_LAST_SUBSYSTEM)
				m_subsystems[pProfiler->m_subsystem].selfTime += aveValue;
			break;
		case COUNT_INFO+1:
			// Standart Deviation.
			aveValue = pProfiler->m_selfTimeHistory.GetStdDeviation();
			aveValue *= 100.0f;
			currentValue = aveValue;
			variance = 0;
			break;
		};

		//////////////////////////////////////////////////////////////////////////
		// Records Peaks.
		uint64 frameID = gEnv->pRenderer->GetFrameID(false);
		if (bEnablePeaks)
		{
			float prevValue = pProfiler->m_selfTimeHistory.GetLast();
			float peakValue = TranslateToDisplayValue(pProfiler->m_selfTime);

			if (pProfiler->m_latestFrame != frameID - 1) {
				prevValue = 0.0f;
			}

			if ((peakValue-prevValue) > m_peakTolerance)
			{
				SPeakRecord peak;
				peak.pProfiler = pProfiler;
				peak.peakValue = peakValue;
				peak.avarageValue = pProfiler->m_selfTimeHistory.GetAverage();
				peak.count = pProfiler->m_count;
				peak.pageFaults = m_nPagesFaultsLastFrame;
				peak.when = CFrameProfilerTimer::TicksToSeconds(m_totalProfileTime);
				AddPeak( peak );

				// Call peak callbacks.
				if (!m_peakCallbacks.empty())
				{
					for (int i = 0; i < (int)m_peakCallbacks.size(); i++)
					{
						m_peakCallbacks[i]->OnFrameProfilerPeak( pProfiler,peakValue );
					}
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////

		pProfiler->m_latestFrame = frameID;
		pProfiler->m_totalTimeHistory.Add( TranslateToDisplayValue(pProfiler->m_totalTime) );
		pProfiler->m_selfTimeHistory.Add( TranslateToDisplayValue(pProfiler->m_selfTime) );
		pProfiler->m_countHistory.Add( pProfiler->m_count );

		pProfiler->m_displayedCurrentValue = nWeightMode > 0 ? currentValue : aveValue;
		pProfiler->m_displayedValue = pProfiler->m_displayedValue*smoothFactor + currentValue*(1.0f - smoothFactor);
		pProfiler->m_variance = pProfiler->m_variance*smoothFactor + variance*(1.0f - smoothFactor);

		if (m_bEnableHistograms)
		{
			if (!pProfiler->m_pGraph)
			{
				// Create graph.
				pProfiler->m_pGraph = new CFrameProfilerGraph;
			}
			// Update values in histogram graph.
			if (m_histogramsMaxPos != pProfiler->m_pGraph->m_data.size())
			{
				pProfiler->m_pGraph->m_width = m_histogramsMaxPos;
				pProfiler->m_pGraph->m_height = m_histogramsHeight;
				pProfiler->m_pGraph->m_data.resize( m_histogramsMaxPos );
			}
			float millis;
			if (m_displayQuantity == TOTAL_TIME || m_displayQuantity == TOTAL_TIME_EXTENDED)
				millis = m_histogramScale * pProfiler->m_totalTimeHistory.GetLast();
			else
				millis = m_histogramScale * pProfiler->m_selfTimeHistory.GetLast();
			if (millis < 0) millis = 0;
			if (millis > 255) millis = 255;
			pProfiler->m_pGraph->m_data[m_histogramsCurrPos] = 255-FtoI(millis); // must use ftoi.
		}

		if (m_nCurSample >= 0)
		{
			UpdateOfflineHistory( pProfiler );
		}

		// Reset profiler.
		pProfiler->m_totalTime = 0;
		pProfiler->m_selfTime = 0;
		pProfiler->m_count = 0;
	}

	m_frameLostTime = m_frameTime - selfAccountedTime;

	if (m_nCurSample >= 0)
	{
		// Keep offline global time history.
		m_frameTimeOfflineHistory.m_selfTime.push_back( FtoI(CFrameProfilerTimer::TicksToMilliseconds(m_frameTime)*1000) );
		m_frameTimeOfflineHistory.m_count.push_back(1);
		m_nCurSample++;
	}
	//AdvanceFrame( m_pSystem );

	// Reset profile callstack var.
	profile_callstack = 0;
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::UpdateOfflineHistory( CFrameProfiler *pProfiler )
{
	if (!pProfiler->m_pOfflineHistory)
	{
		pProfiler->m_pOfflineHistory = new CFrameProfilerOfflineHistory;
		pProfiler->m_pOfflineHistory->m_count.reserve( 1000+m_nCurSample*2 );
		pProfiler->m_pOfflineHistory->m_selfTime.reserve( 1000+m_nCurSample*2 );
	}
	int prevCont = (int)pProfiler->m_pOfflineHistory->m_selfTime.size();
	int newCount = m_nCurSample+1;
	pProfiler->m_pOfflineHistory->m_selfTime.resize( newCount );
	pProfiler->m_pOfflineHistory->m_count.resize( newCount );

	unsigned int micros = FtoI(CFrameProfilerTimer::TicksToMilliseconds(pProfiler->m_selfTime)*1000);
	unsigned short count = pProfiler->m_count;
	for (int i = prevCont; i < newCount; i++)
	{
		pProfiler->m_pOfflineHistory->m_selfTime[i] = micros;
		pProfiler->m_pOfflineHistory->m_count[i] = count;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::AddPeak( SPeakRecord &peak )
{
	// Add peak.
	if (m_peaks.size() > MAX_PEAK_PROFILERS)
		m_peaks.pop_back();

	if(m_pSystem->IsDedicated())
		m_pSystem->GetILog()->Log("Peak: name:'%s' val:%.2f avg:%.2f cnt:%d",
			(const char*)GetFullName(peak.pProfiler),
			peak.peakValue,
			peak.avarageValue,
			peak.count);

	/*
	// Check to see if this function is already a peak.
	for (int i = 0; i < (int)m_peaks.size(); i++)
	{
		if (m_peaks[i].pProfiler == peak.pProfiler)
		{
			m_peaks.erase( m_peaks.begin()+i );
			break;
		}
	}
	*/
	m_peaks.insert( m_peaks.begin(),peak );

	// Add to absolute value

	for (int i = 0; i < (int)m_absolutepeaks.size(); i++)
	{
		if (m_absolutepeaks[i].pProfiler == peak.pProfiler)
			if (m_absolutepeaks[i].peakValue < peak.peakValue)
			{
				m_absolutepeaks.erase( m_absolutepeaks.begin()+i );
				break;
			}
			else
				return;

	}

	bool bInserted = false;
	for (size_t i = 0; i < m_absolutepeaks.size(); ++i) {
		if (m_absolutepeaks[i].peakValue < peak.peakValue) {
			m_absolutepeaks[i] = peak;
			bInserted = true;
			break;
		}
	}

	if (!bInserted && m_absolutepeaks.size() < MAX_ABSOLUTEPEAK_PROFILERS) {
		m_absolutepeaks.push_back(peak);
	}

}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::SetDisplayQuantity( EDisplayQuantity quantity )
{
	m_displayQuantity = quantity;
	m_bDisplayedProfilersValid = false;
	if (m_displayQuantity == SELF_TIME_EXTENDED || m_displayQuantity == TOTAL_TIME_EXTENDED)
		EnableHistograms(true);
	else
		EnableHistograms(false);
};

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::SetSubsystemFilter( bool bFilterSubsystem,EProfiledSubsystem subsystem )
{
	m_bSubsystemFilterEnabled = bFilterSubsystem;
	m_subsystemFilter = subsystem;
	m_bDisplayedProfilersValid = false;
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::SetSubsystemFilter( const char *szFilterName )
{
	bool bFound = false;
	for (int i = 0; i < PROFILE_LAST_SUBSYSTEM; i++)
	{
		if (!m_subsystems[i].name)
			continue;
		if (stricmp(m_subsystems[i].name,szFilterName) == 0)
		{
			SetSubsystemFilter( true,(EProfiledSubsystem)i );
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		// Check for count limit.
		int nCount = atoi(szFilterName);
		if (nCount > 0)
			m_maxProfileCount = nCount;
		else
			SetSubsystemFilter( false,PROFILE_ANY );
	}
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::AddPeaksListener( IFrameProfilePeakCallback *pPeakCallback )
{
	// Only add one time.
	stl::push_back_unique( m_peakCallbacks,pPeakCallback );
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::RemovePeaksListener( IFrameProfilePeakCallback *pPeakCallback )
{
	stl::find_and_erase( m_peakCallbacks,pPeakCallback );
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::EnableMemoryProfile( bool bEnable )
{
	if (bEnable != m_bDisplayMemoryInfo)
		m_bLogMemoryInfo = true;
	m_bDisplayMemoryInfo = bEnable;
}

#endif
