////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   frameprofilesystem.h
//  Version:     v1.00
//  Created:     24/6/2003 by Timur,Sergey,Wouter.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __frameprofilesystem_h__
#define __frameprofilesystem_h__
#pragma once

#include "CryCommon/CryCore/platform.h"
#include "CryCommon/CrySystem/FrameProfiler.h"

#ifdef USE_FRAME_PROFILER

#if defined(PS3)
	#include <IJobManSPU.h>
#endif

//////////////////////////////////////////////////////////////////////////
// Frame Profile Timer, provides precise timer for frame profiler.
//////////////////////////////////////////////////////////////////////////
class CFrameProfilerTimer
{
public:
	static void Init(bool bThreadSafe); // called once
	static int64 GetTicks();
	static float TicksToSeconds( int64 nTime )
	{
		return float(g_fSecondsPerTick * nTime);
	}
	static float TicksToMilliseconds( int64 nTime )
	{
		return float(g_fSecondsPerTick * 1000.0 * nTime);
	}

protected:
	static bool g_bThreadSafe;
	static int64 g_nTicksPerSecond;
	static double g_fSecondsPerTick;
};

//////////////////////////////////////////////////////////////////////////
//! the system which does the gathering of stats
class CFrameProfileSystem : public IFrameProfileSystem
{
public:
	int m_nCurSample;
	
	char *m_pPrefix;
	bool m_bEnabled;
	//! True when collection must be paused.
	bool m_bCollectionPaused;
	
	//! If set profiling data will be collected.
	bool m_bCollect;
	//! If >0, profiling will operate on all threads. If >=2, will use thread-safe QueryPerformanceCounter instead of CryTicks.
	int m_nThreadSupport;
	//! If set profiling data will be displayed.
	bool m_bDisplay;
	//! True if network profiling is enabled.
	bool m_bNetworkProfiling;
	//! If set memory info by modules will be displayed.
	bool m_bDisplayMemoryInfo;
	//! Put memory info also in the log.
	bool m_bLogMemoryInfo;

	ISystem *m_pSystem;
	IRenderer *m_pRenderer;

	static CFrameProfileSystem* s_pFrameProfileSystem;

	// Cvars.
	static int profile_callstack;
	static int profile_log;

	//struct SPeakRecord
	//{
	//	CFrameProfiler *pProfiler;
	//	float peakValue;
	//	float avarageValue;
	//	float variance;
	//	int pageFaults; // Number of page faults at this frame.
	//	int count;  // Number of times called for peak.
	//	float when; // when it added.
	//};
	struct SProfilerDisplayInfo
	{
		float x,y; // Position where this profiler rendered.
		int averageCount;
		int level; // child level.
		CFrameProfiler *pProfiler;
	};
	struct SSubSystemInfo
	{
		const char *name;
		float selfTime;
	};

	EDisplayQuantity m_displayQuantity;

	//! When profiling frame started.
	int64 m_frameStartTime;
	//! Total time of profiling.
	int64 m_totalProfileTime;
	//! Frame time from the last frame.
	int64 m_frameTime;
	//! Frame time not accounted by registered profilers.
	int64 m_frameLostTime;
	//! Ticks per profiling call, for adjustment.
	int64 m_callOverheadTime;

#if defined(PS3) && defined(SUPP_SPU_FRAME_STATS)
	//spu frame stats
	NPPU::SSPUFrameStats m_SPUFrameStats;
	std::vector<NPPU::SFrameProfileData> m_SPUJobFrameStats;
#endif

	//! Maintain separate profiler stacks for each thread.
	//! Disallow any post-init allocation, to avoid profiler/allocator conflicts
	typedef uint32 TThreadId;
	struct SProfilerThreads
	{
		SProfilerThreads( TThreadId nMainThreadId )
		{
			// First thread stack is for main thread.
			m_aThreadStacks[0].threadId = nMainThreadId;
			m_nThreadStacks = 1;
			m_pReservedProfilers = 0;
			m_lock = 0;
		}

		void Reset(ISystem* pSystem);

		ILINE TThreadId GetMainThreadId() const 
		{
			return m_aThreadStacks[0].threadId;
		}
		ILINE CFrameProfilerSection const* GetMainSection() const 
		{
			return m_aThreadStacks[0].pProfilerSection;
		}

		static ILINE void Push( CFrameProfilerSection*& parent, CFrameProfilerSection* child )
		{
			assert(child);
			child->m_pParent = parent;
			parent = child;
		}

		ILINE void PushSection( CFrameProfilerSection* pSection, TThreadId threadId )
		{
			if (m_aThreadStacks[0].threadId == threadId)
				Push(m_aThreadStacks[0].pProfilerSection, pSection);
			else
				PushThreadedSection( pSection, threadId );
		}
		void PushThreadedSection( CFrameProfilerSection* pSection, TThreadId threadId )
		{
			assert(pSection);
			{
				ReadLock lock(m_lock);

				// Look for thread slot.
				for (int i = 1; i < m_nThreadStacks; ++i)
					if (m_aThreadStacks[i].threadId == threadId)
					{
						Push(m_aThreadStacks[i].pProfilerSection, pSection);
						return;
					}
			}

			// Look for unused slot.
			WriteLock lock(m_lock);

			int i;
			for (i = 1; i < m_nThreadStacks; ++i)
				if (m_aThreadStacks[i].pProfilerSection == 0)
					break;

			// Allocate new slot.
			if (i == m_nThreadStacks)
			{
				if (m_nThreadStacks == nMAX_THREADS)
					CryError("Profiled thread count of %d exceeded!", nMAX_THREADS);
				m_nThreadStacks++;
			}

			m_aThreadStacks[i].threadId = threadId;
			Push(m_aThreadStacks[i].pProfilerSection, pSection);
		}

		ILINE void PopSection( CFrameProfilerSection* pSection, TThreadId threadId )
		{
			// Thread-safe without locking.
			for (int i = 0; i < m_nThreadStacks; ++i)
				if (m_aThreadStacks[i].threadId == threadId)
				{
					assert(m_aThreadStacks[i].pProfilerSection == pSection || m_aThreadStacks[i].pProfilerSection == 0);
					m_aThreadStacks[i].pProfilerSection = pSection->m_pParent;
					return;
				}
			assert(0);
		}

		ILINE CFrameProfiler* GetThreadProfiler( CFrameProfiler* pMainProfiler, TThreadId nThreadId )
		{
			// Check main threads, or existing linked threads.
			if (nThreadId == GetMainThreadId())
				return pMainProfiler;
			for (CFrameProfiler* pProfiler = pMainProfiler->m_pNextThread; pProfiler; pProfiler = pProfiler->m_pNextThread)
				if (pProfiler->m_threadId == nThreadId)
					return pProfiler;
			return NewThreadProfiler( pMainProfiler, nThreadId );
		}

		CFrameProfiler* NewThreadProfiler( CFrameProfiler* pMainProfiler, TThreadId nThreadId );


	protected:
		struct SThreadStack
		{
			TThreadId								threadId;
			CFrameProfilerSection*	pProfilerSection;

			SThreadStack(TThreadId id = 0)
				: threadId(id), pProfilerSection(0)
			{}
		};
		static const int					nMAX_THREADS = 128;
		int												m_nThreadStacks;
		SThreadStack							m_aThreadStacks[nMAX_THREADS];
		CFrameProfiler*						m_pReservedProfilers;
		volatile int							m_lock;
	};
	SProfilerThreads m_ProfilerThreads;
	CCustomProfilerSection *m_pCurrentCustomSection;

	typedef std::vector<CFrameProfiler*> Profilers;
	//! Array of all registered profilers.
	Profilers m_profilers;
	//! Network profilers, they are not in regular list.
	Profilers m_netTrafficProfilers;
	//! Currently active profilers array.
	Profilers *m_pProfilers;

	float m_peakTolerance;

	//! List of several latest peaks.
	std::vector<SPeakRecord> m_peaks;
	std::vector<SPeakRecord> m_absolutepeaks;
	std::vector<SProfilerDisplayInfo> m_displayedProfilers;
	bool m_bDisplayedProfilersValid;
	EProfiledSubsystem m_subsystemFilter;
	bool m_bSubsystemFilterEnabled;
	int m_maxProfileCount;

	//////////////////////////////////////////////////////////////////////////
	//! Smooth frame time in milliseconds.
	CFrameProfilerSamplesHistory<float,32> m_frameTimeHistory;
	CFrameProfilerSamplesHistory<float,32> m_frameTimeLostHistory;

	//////////////////////////////////////////////////////////////////////////
	// Graphs.
	//////////////////////////////////////////////////////////////////////////
	bool m_bDrawGraph;
	std::vector<unsigned char> m_timeGraph;
	std::vector<unsigned char> m_timeGraph2;
#if defined(PS3) && defined(SUPP_SPU_FRAME_STATS)
	std::vector<unsigned char> m_timeGraphSPU[NPPU::scMaxSPU];//one time graph per SPU
	int m_timeGraphCurrentPosSPU;
	std::vector<SPeakRecord> m_peaksSPU;
	bool m_bSPUMode;
#endif
	int m_timeGraphCurrentPos;
	CFrameProfiler* m_pGraphProfiler;

	//////////////////////////////////////////////////////////////////////////
	// Histograms.
	//////////////////////////////////////////////////////////////////////////
	bool m_bEnableHistograms;
	int m_histogramsCurrPos;
	int m_histogramsMaxPos;
	int m_histogramsHeight;
	float m_histogramScale;

	//////////////////////////////////////////////////////////////////////////
	// Selection/Render.
	//////////////////////////////////////////////////////////////////////////
	int m_selectedRow,m_selectedCol;
	float ROW_SIZE,COL_SIZE;
	float m_baseY;
	int m_textModeBaseExtra;
	float m_mouseX,m_mouseY;

	int m_nPagesFaultsLastFrame;
	int m_nPagesFaultsPerSec;
	int64 m_nLastPageFaultCount;
	bool m_bPageFaultsGraph;

	//////////////////////////////////////////////////////////////////////////
	// Subsystems.
	//////////////////////////////////////////////////////////////////////////
	SSubSystemInfo m_subsystems[PROFILE_LAST_SUBSYSTEM];
	
	CFrameProfilerOfflineHistory m_frameTimeOfflineHistory;

	//////////////////////////////////////////////////////////////////////////
	// Peak callbacks.
	//////////////////////////////////////////////////////////////////////////
	std::vector<IFrameProfilePeakCallback*> m_peakCallbacks;

	class CSampler *m_pSampler;

public:
	//////////////////////////////////////////////////////////////////////////
	// Methods.
	//////////////////////////////////////////////////////////////////////////
	CFrameProfileSystem();
	~CFrameProfileSystem();
	void Init( ISystem *pSystem );
	void Done();

	void SetProfiling(bool on, bool display, char *prefix, ISystem *pSystem);

	//////////////////////////////////////////////////////////////////////////
	// IFrameProfileSystem interface implementation.
	//////////////////////////////////////////////////////////////////////////
	//! Reset all profiling data.
	void Reset();
	//! Add new frame profiler.
	//! Profile System will not delete those pointers, client must take care of memory managment issues.
	void AddFrameProfiler( CFrameProfiler *pProfiler );
	//! Must be called at the start of the frame.
	void StartFrame();
	//! Must be called at the end of the frame.
	void EndFrame();
	//! Get number of registered frame profilers.
	int GetProfilerCount() const { return (int)m_profilers.size(); };

	virtual int GetPeaksCount() const
	{
		return (int)m_absolutepeaks.size();
	}

	virtual const SPeakRecord* GetPeak(int index) const
	{
		if (index >= 0 && index < m_absolutepeaks.size())
			return &m_absolutepeaks[index];
		return 0;
	}

	//! Get frame profiler at specified index.
	//! @param index must be 0 <= index < GetProfileCount() 
	CFrameProfiler* GetProfiler( int index ) const;
	//! helper 
	virtual float TicksToSeconds (int64 nTime);
	inline TThreadId GetMainThreadId() const 
	{
		return m_ProfilerThreads.GetMainThreadId();
	}

	//////////////////////////////////////////////////////////////////////////
	// Sampling related.
	//////////////////////////////////////////////////////////////////////////
	void StartSampling( int nMaxSamples );

	//////////////////////////////////////////////////////////////////////////
	// Adds a value to profiler.
	virtual void StartCustomSection( CCustomProfilerSection *pSection );
	virtual void EndCustomSection( CCustomProfilerSection *pSection );

	//////////////////////////////////////////////////////////////////////////
	// Peak callbacks.
	//////////////////////////////////////////////////////////////////////////
	virtual void AddPeaksListener( IFrameProfilePeakCallback *pPeakCallback );
	virtual void RemovePeaksListener( IFrameProfilePeakCallback *pPeakCallback );
	
	//////////////////////////////////////////////////////////////////////////
	//! Starts profiling a new section.
	static void StartProfilerSection( CFrameProfilerSection *pSection );
	//! Ends profiling a section.
	static void EndProfilerSection( CFrameProfilerSection *pSection );
	//! Gets the bottom active section.
	virtual CFrameProfilerSection const* GetCurrentProfilerSection();

	//! Enable/Disable profile samples gathering.
	void Enable( bool bCollect,bool bDisplay );
	void EnableMemoryProfile( bool bEnable );
	void SetSubsystemFilter( bool bFilterSubsystem,EProfiledSubsystem subsystem );
	void EnableHistograms( bool bEnableHistograms );
	bool IsEnabled() const { return m_bEnabled; }
	virtual bool IsVisible() const { return m_bDisplay; }
	bool IsProfiling() const { return m_bCollect; }
	void SetDisplayQuantity( EDisplayQuantity quantity );
	void AddPeak( SPeakRecord &peak );
	void SetHistogramScale( float fScale ) { m_histogramScale = fScale; }
	void SetDrawGraph( bool bDrawGraph ) { m_bDrawGraph = bDrawGraph; };
#if defined(PS3) && defined(SUPP_SPU_FRAME_STATS)
	void SetSPUMode( bool bSPUMode ) { m_bSPUMode = bSPUMode; };
#endif
	void SetThreadSupport( int nThreading ) { m_nThreadSupport = nThreading; }
	void SetNetworkProfiler( bool bNet ) { m_bNetworkProfiling = bNet; };
	void SetPeakTolerance( float fPeakTimeMillis ) { m_peakTolerance = fPeakTimeMillis; }
	void SetPageFaultsGraph( bool bEnabled ) { m_bPageFaultsGraph = bEnabled; }

	void SetSubsystemFilter( const char *sFilterName );
	void UpdateOfflineHistory( CFrameProfiler *pProfiler );

	//////////////////////////////////////////////////////////////////////////
	// Rendering.
	//////////////////////////////////////////////////////////////////////////
	void Render();
	void RenderMemoryInfo();
	void RenderProfiler( CFrameProfiler *pProfiler,int level,float col,float row,bool bExtended,bool bSelected );
	void RenderProfilerHeader( float col,float row,bool bExtended );
	void RenderProfilers( float col,float row,bool bExtended );
	void RenderPeaks();
	void RenderSubSystems( float col,float row );
	void RenderHistograms();
	void CalcDisplayedProfilers();
	void DrawGraph();
	void DrawLabel( float raw,float column, float* fColor,float glow,const char* szText,float fScale=1.0f);
	void DrawRect( float x1,float y1,float x2,float y2,float *fColor );
	CFrameProfiler* GetSelectedProfiler();
	// Recursively add frame profiler and childs to displayed list.
	void AddDisplayedProfiler( CFrameProfiler *pProfiler,int level );

	//////////////////////////////////////////////////////////////////////////
	float TranslateToDisplayValue( int64 val );
	const char* GetFullName( CFrameProfiler *pProfiler );
	const char* GetModuleName( CFrameProfiler *pProfiler );
};

#else

// Dummy Frame profile Timer interface.
class CFrameProfilerTimer
{
public:
	
	static float TicksToSeconds( int64 nTime ){return 0.0f;}
};

// Dummy Frame profile system interface.
struct CFrameProfileSystem : public IFrameProfileSystem
{
	//! Reset all profiling data.
	virtual void Reset() {};
	//! Add new frame profiler.
	//! Profile System will not delete those pointers, client must take care of memory managment issues.
	virtual void AddFrameProfiler( class CFrameProfiler *pProfiler ) {};
	//! Must be called at the start of the frame.
	virtual void StartFrame() {};
	//! Must be called at the end of the frame.
	virtual void EndFrame() {};

	//! Here the new methods needed to enable profiling to go off.

	virtual int GetPeaksCount(void) const {return 0;}
	virtual const SPeakRecord* GetPeak( int index ) const  {return 0;}
	virtual int GetProfilerCount() const {return 0;}

	virtual CFrameProfiler* GetProfiler( int index ) const {return NULL;}

	virtual void Enable( bool bCollect,bool bDisplay ){}

	virtual void SetSubsystemFilter( bool bFilterSubsystem,EProfiledSubsystem subsystem ){}
	virtual void SetSubsystemFilter( const char *sFilterName ){}

	virtual bool IsEnabled() const {return 0;}
	
	virtual bool IsProfiling() const {return 0;}

	virtual void SetDisplayQuantity( EDisplayQuantity quantity ){}

	virtual void StartCustomSection( CCustomProfilerSection *pSection ){}
	virtual void EndCustomSection( CCustomProfilerSection *pSection ){}

	virtual void StartSampling( int ){}

	virtual void AddPeaksListener( IFrameProfilePeakCallback *pPeakCallback ){}
	virtual void RemovePeaksListener( IFrameProfilePeakCallback *pPeakCallback ){}

	void Init( ISystem *pSystem ){}
	void Done(){}
	void Render(){}

	void SetHistogramScale( float fScale ){}
	void SetDrawGraph( bool bDrawGraph ){}
	void SetNetworkProfiler( bool bNet ){}
	void SetPeakTolerance( float fPeakTimeMillis ){}
	void SetSmoothingTime( float fSmoothTime ){}
	void SetPageFaultsGraph( bool bEnabled ){}
	void SetThreadSupport( int ) {}

	void EnableMemoryProfile( bool bEnable ){}
	virtual bool IsVisible() const { return true; }
	virtual float TicksToSeconds (int64 ){return 0.f;}
	virtual const CFrameProfilerSection* GetCurrentProfilerSection(){return NULL;}

	const char* GetModuleName( CFrameProfiler *pProfiler ) { return 0; }
};

#endif // USE_FRAME_PROFILER

#endif // __frameprofilesystem_h__
