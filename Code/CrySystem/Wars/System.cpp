//////////////////////////////////////////////////////////////////////
//
//	Crytek Source code
// 
//	File: System.cpp
//  Description: CryENGINE system core-handle all subsystems
// 
//	History:
//	-Jan 31,2001: Originally Created by Marco Corbetta
//	-: modified by all
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "../LocalizationManager.h"
#include "System.h"
#include <time.h>
//#include "ini_vars.h"
#include "CryLibrary.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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
#include "CryCommon/CrySoundSystem/IReverbManager.h"
#include "CryCommon/CrySoundSystem/IMusicSystem.h"
#include "CryCommon/CryAnimation/ICryAnimation.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryCommon/CrySystem/IProcess.h"
#include "CryCommon/CrySystem/IBudgetingSystem.h"

#include "CryPak.h"
#include "XConsole.h"
#include "Log.h"
#include "CrySizerStats.h"
#include "CrySizerImpl.h"

#include "XML/xml.h"
#include "XML/ReadWriteXMLSink.h"
#include "DataProbe.h"

#include "StreamEngine.h"
#include "PhysRenderer.h"

#include "LocalizedStringManager.h"
#include "XML/XmlUtils.h"
#include "ThreadProfiler.h"
#include "SystemEventDispatcher.h"
#include "HardwareMouse.h"
#include "ServerThrottle.h"

#include "CryCommon/CryCore/crc32.h"
#include <PNoise3.h>

#include "CryWaterMark.h"
WATERMARKDATA(_m);

#ifdef WIN32
#define  PROFILE_WITH_VTUNE
#include <process.h>
#include <malloc.h>
#endif

// profilers api.
VTuneFunction VTResume = NULL;
VTuneFunction VTPause = NULL;

// Define global cvars.
SSystemCVars g_cvars;

#if defined(USE_UNIXCONSOLE)
#include "UnixConsole.h"
extern CUNIXConsole g_UnixConsole;
#endif

extern int CryMemoryGetAllocatedSize();

// these heaps are used by underlying System structures
// to allocate, accordingly, small (like elements of std::set<..*>) and big (like memory for reading files) objects
// hopefully someday we'll have standard MT-safe heap
//CMTSafeHeap g_pakHeap;
CMTSafeHeap* g_pPakHeap = 0;// = &g_pakHeap;

#ifdef WIN32
extern HMODULE gDLLHandle;
#pragma comment(lib, "WINMM.lib")
#pragma comment(lib, "dxguid.lib")
#endif

#define DLL_GAME_ENTRANCE_FUNCTION	"CreateGameInstance"

//////////////////////////////////////////////////////////////////////////
#include "Validator.h"

/////////////////////////////////////////////////////////////////////////////////
// System Implementation.
//////////////////////////////////////////////////////////////////////////
CSystem::CSystem()
{
	m_iHeight = 0;
	m_iWidth = 0;
	m_iColorBits = 0;
	// CRT ALLOCATION threshold

	m_pSystemEventDispatcher = new CSystemEventDispatcher(); // Must be first.

	//#ifndef _XBOX
#ifdef WIN32	
	m_hInst = NULL;
	m_hWnd = NULL;
#if defined(_M_IX86)
	_set_sbh_threshold(1016);
#endif
#endif

	//////////////////////////////////////////////////////////////////////////
	// Clear environment.
	//////////////////////////////////////////////////////////////////////////
	memset( &m_env,0,sizeof(m_env) );

	//////////////////////////////////////////////////////////////////////////
	// Reset handles.
	memset( &m_dll,0,sizeof(m_dll) );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Initialize global environment interface pointers.
	m_env.pSystem = this;
	m_env.pTimer = &m_Time;
	m_env.pNameTable = &m_nameTable;
	m_env.pFrameProfileSystem = &m_FrameProfileSystem;
	m_env.bClient = false;
	m_env.bServer = false;
	m_env.bMultiplayer = false;
	m_env.bProfilerEnabled = false;
	m_env.callbackStartSection = 0;
	m_env.callbackEndSection = 0;
	m_env.bIgnoreAllAsserts = false;
	//////////////////////////////////////////////////////////////////////////
	

	m_pStreamEngine = NULL;

	m_pIFont = NULL;
	m_pTestSystem = NULL;
	m_rWidth=NULL;
	m_rHeight=NULL;
	m_rColorBits=NULL;
	m_rDepthBits=NULL;
	m_cvSSInfo=NULL;
	m_rStencilBits=NULL;
	m_rFullscreen=NULL;
	m_rDriver=NULL;
	m_sysNoUpdate=NULL;
	m_pMemoryManager = NULL;
	m_pProcess = NULL;
	
	m_pValidator = NULL;
	m_pCmdLine = NULL;
	m_pDefaultValidator = NULL;
	m_pIBudgetingSystem = NULL;
	m_pLocalizationManager = NULL;
	m_sys_SaveCVars=0;
	m_sys_StreamCallbackTimeBudget=0;
	m_sys_physics_CPU=0;
	m_sys_min_step=0;
	m_sys_max_step=0;

	m_cvAIUpdate = NULL;

	m_pUserCallback = NULL;
	m_sys_memory_debug=NULL;
	m_sysWarnings = NULL;
	m_sys_profile = NULL;
	m_sys_profile_graphScale = NULL;
	m_sys_profile_pagefaultsgraph = NULL;
	m_sys_profile_graph = NULL;
	m_sys_profile_filter = NULL;
	m_sys_profile_allThreads = NULL;
	m_sys_profile_network = NULL;
	m_sys_profile_peak = NULL;
	m_sys_profile_memory = NULL;
	m_sys_profile_sampler = NULL;
	m_sys_profile_sampler_max_samples = NULL;
#if defined(PS3)
	m_sys_spu_profile = NULL;
	m_sys_spu_enable = NULL;
	m_sys_spu_debug = NULL;
	m_sys_spu_dump_stats = NULL;
#endif
	m_sys_spec = NULL;
	m_sys_firstlaunch = NULL;
	m_sys_enable_budgetmonitoring = NULL;
	m_sys_preload = NULL;
	m_sys_crashtest = NULL;
//	m_sys_filecache = NULL;
	m_gpu_particle_physics = NULL;
	m_pCpu = NULL;

	m_profile_old = 0;

	m_bQuit = false;
	m_bRelaunch = false;
	m_bRelaunched = false;
	m_iLoadingMode = 0;
	m_bTestMode = false;
	m_bEditor = false;
	m_bPreviewMode = false;
	m_bIgnoreUpdates = false;

	m_nStrangeRatio = 1000;
	// no mem stats at the moment
	m_pMemStats = NULL;
	m_pSizer = NULL;

	m_pCVarQuit = NULL;

	m_pDownloadManager = 0;
	// default game MOD is root
	memset(m_szGameMOD,0,256);
	m_pDataProbe = 0;
#if defined( _DATAPROBE )
	m_pDataProbe = new CDataProbe;
#endif
	m_bForceNonDevMode=false;
	m_bWasInDevMode = false;
	m_bInDevMode = false;
	m_bGameFolderWritable = false;

	m_nServerConfigSpec = CONFIG_VERYHIGH_SPEC;
	m_nMaxConfigSpec = CONFIG_VERYHIGH_SPEC;

	//m_hPhysicsThread = INVALID_HANDLE_VALUE;
	//m_hPhysicsActive = INVALID_HANDLE_VALUE;
	//m_bStopPhysics = 0;
	//m_bPhysicsActive = 0;

	m_pProgressListener = 0;

	m_bPaused = false;
	m_bReloadTextures = false;
	m_nUpdateCounter = 0;

	m_pPhysRenderer = 0;

	m_pXMLUtils = new CXmlUtils(this);
	// CryMP: legacy CTestSystem is not part of the embedded boot path.
	m_pTestSystem = nullptr;
	m_pThreadTaskManager = new CThreadTaskManager;
	m_pThreadProfiler = 0;

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
	m_pGPUPhysics = NULL;
	pfnCreateGPUPhysics = NULL;
#endif

	g_pPakHeap = new CMTSafeHeap;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
CSystem::~CSystem()
{
	ShutDown();

	FreeLib(m_dll.hNetwork);
	FreeLib(m_dll.hAI);
	FreeLib(m_dll.hInput);
	FreeLib(m_dll.hScript);
	FreeLib(m_dll.hPhysics);
	FreeLib(m_dll.hEntitySystem);
	FreeLib(m_dll.hRenderer);
	FreeLib(m_dll.hFlash);
	FreeLib(m_dll.hFont);
	FreeLib(m_dll.hMovie);
	FreeLib(m_dll.hIndoor);
	FreeLib(m_dll.h3DEngine);
	FreeLib(m_dll.hAnimation);
	FreeLib(m_dll.hGame);
	FreeLib(m_dll.hSound);
#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
	FreeLib(m_dll.hPhysicsGPU);
#endif
	SAFE_DELETE(m_pThreadProfiler);
	SAFE_DELETE(m_pDataProbe);
	SAFE_DELETE(m_pXMLUtils);
	SAFE_DELETE(m_pThreadTaskManager);
	SAFE_DELETE(m_pSystemEventDispatcher);

	SAFE_DELETE(g_pPakHeap);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::Release()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::FreeLib( WIN_HMODULE hLibModule)
{
	if (hLibModule) 
	{ 
		CryFreeLibrary(hLibModule);
		(hLibModule)=NULL; 
	} 
}

//////////////////////////////////////////////////////////////////////////
IStreamEngine* CSystem::GetStreamEngine()
{
	return m_pStreamEngine;
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)
WIN_HMODULE CSystem::LoadDLL( const char *dllName,bool bQuitIfNotFound)
{ 
	WIN_HMODULE handle = CryLoadLibrary( dllName ); 

 	if (!handle)      
	{
#if defined(LINUX)
		printf ("Error loading DLL: %s, error :  %s\n", dllName, dlerror());
		if (bQuitIfNotFound)
			Quit();
		else
			return 0;
#else
		if (bQuitIfNotFound)
		{		
			Error( "Error loading DLL: %s, error code %d",dllName, GetLastError());
			Quit();
		}
		return 0;
	#endif //LINUX
	}
#if defined(_DATAPROBE) && !defined(LINUX) && !defined(XENON) && !defined(PS3)
	IDataProbe::SModuleInfo module;
	module.filename = dllName;
	module.handle = gDLLHandle;
	m_pDataProbe->AddModule(module);
#endif
	return handle;
}
#endif

//////////////////////////////////////////////////////////////////////////
void CSystem::SetForceNonDevMode( const bool bValue )
{
	m_bForceNonDevMode=bValue;
	if (bValue)
		SetDevMode(false);
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::GetForceNonDevMode() const
{
	return m_bForceNonDevMode;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetDevMode( bool bEnable )
{
	/*
	//////////////////////////////////////////////////////////////////////////
	// Timur
	//////////////////////////////////////////////////////////////////////////
	// For now always in dev mode.
	m_bWasInDevMode = true;
	m_bInDevMode = true; 
	return;
	*/
	
	if (!bEnable)
	{
		// set pak priority to files inside the pak to avoid
		// trying to open unnecessary files and to avoid cheating
		ICVar *cVar = m_env.pConsole->GetCVar("lua_debugger");
		if (cVar) 
			cVar->ForceSet("0");
		if (m_cvPakPriority)
			m_cvPakPriority->ForceSet("1");
	}	
	else
	{
		if (m_cvPakPriority)
			m_cvPakPriority->ForceSet("0");
	}	
	if (bEnable)
		m_bWasInDevMode = true;
	m_bInDevMode = bEnable;
	
	if(m_bInDevMode)
		CryLogAlways("Running in devmode");
}


void LvlRes_export( IConsoleCmdArgs *pParams );

///////////////////////////////////////////////////
void CSystem::ShutDown()
{		
	CryLogAlways("System Shutdown");

	if (m_pUserCallback)
		m_pUserCallback->OnShutdown();

	ShareShaderCombinations();

	KillPhysicsThread();
	
	if (m_env.pSoundSystem && m_env.pSoundSystem->GetIReverbManager())
	{
		// turn EAX off otherwise it affects all Windows sounds!
		m_env.pSoundSystem->GetIReverbManager()->SetListenerReverb(REVERB_PRESET_OFF);
	}

	m_FrameProfileSystem.Done();

	if (m_sys_firstlaunch)
		m_sys_firstlaunch->Set( "0" );

	if (m_bEditor)
	{
		// restore the old saved cvars
		if(m_env.pConsole->GetCVar("r_Width"))
			m_env.pConsole->GetCVar("r_Width")->Set(m_iWidth);
		if(m_env.pConsole->GetCVar("r_Height"))
			m_env.pConsole->GetCVar("r_Height")->Set(m_iHeight);
		if(m_env.pConsole->GetCVar("r_ColorBits"))
			m_env.pConsole->GetCVar("r_ColorBits")->Set(m_iColorBits);
	}

	if (m_bEditor && !m_bRelaunch)
	{
		SaveConfiguration();
	}

	//if (!m_bEditor && !bRelaunch)
	if (!m_bEditor)
	{	 		
		if (m_pCVarQuit && m_pCVarQuit->GetIVal())		
		{
			SaveConfiguration();
			m_env.pNetwork->FastShutdown();

			//@TODO: release game.
			//SAFE_RELEASE(m_env.pGame);
			//FreeLib(m_dll.hGame);

			SAFE_RELEASE(m_env.pRenderer);
      FreeLib(m_dll.hRenderer);
	
			SAFE_RELEASE(m_env.pLog);		// creates log backup

#if defined(LINUX) || defined(PS3)
			return;//safe clean return
#else
			// Commit files changes to the disk.
			_flushall();
			_exit(EXIT_SUCCESS);	
#endif
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Release Game.
	//////////////////////////////////////////////////////////////////////////	
	if (m_env.pEntitySystem)
	  m_env.pEntitySystem->Reset();
	//@TODO: Release game.
	//SAFE_RELEASE(m_env.pGame); 
	if (m_env.pPhysicalWorld)
	{
		m_env.pPhysicalWorld->SetPhysicsStreamer(0);
		m_env.pPhysicalWorld->SetPhysicsEventClient(0);
	}

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
	// release GPU physics
	if ( m_pGPUPhysics )
	{
		//pfnDeleteGPUPhysics( m_pGPUPhysics );
		delete m_pGPUPhysics;
		m_pGPUPhysics = NULL;
	}
#endif

	//////////////////////////////////////////////////////////////////////////
	// Clear 3D Engine resources.
	if (m_env.p3DEngine)
		m_env.p3DEngine->ClearRenderResources();
	//////////////////////////////////////////////////////////////////////////

	SAFE_RELEASE(m_env.pHardwareMouse);
	SAFE_RELEASE(m_env.pMovieSystem);
	SAFE_RELEASE(m_env.pEntitySystem);
	SAFE_RELEASE(m_env.pAISystem);
	SAFE_RELEASE(m_env.pCryFont);
	SAFE_RELEASE(m_env.pMusicSystem);
	SAFE_RELEASE(m_env.pNetwork);
//	SAFE_RELEASE(m_env.pCharacterManager);
	SAFE_RELEASE(m_env.p3DEngine);
	SAFE_RELEASE(m_env.pPhysicalWorld);
  if (m_env.pConsole)
    ((CXConsole*)m_env.pConsole)->FreeRenderResources();
	SAFE_RELEASE(m_pIBudgetingSystem);
	SAFE_RELEASE(m_env.pRenderer);
	SAFE_RELEASE(m_env.pSoundSystem);

	if(m_env.pLog)
		m_env.pLog->UnregisterConsoleVariables();

	// Release console variables.
	
	SAFE_RELEASE(m_pCVarQuit);
	SAFE_RELEASE(m_rWidth);
	SAFE_RELEASE(m_rHeight);
	SAFE_RELEASE(m_rColorBits);
	SAFE_RELEASE(m_rDepthBits);
	SAFE_RELEASE(m_cvSSInfo);
	SAFE_RELEASE(m_rStencilBits);
	SAFE_RELEASE(m_rFullscreen);
	SAFE_RELEASE(m_rDriver);

	SAFE_RELEASE(m_sysWarnings);
	SAFE_RELEASE(m_sys_profile);
	SAFE_RELEASE(m_sys_profile_graph);
	SAFE_RELEASE(m_sys_profile_pagefaultsgraph);
	SAFE_RELEASE(m_sys_profile_graphScale);
	SAFE_RELEASE(m_sys_profile_filter);
	SAFE_RELEASE(m_sys_profile_allThreads);
	SAFE_RELEASE(m_sys_profile_network);
	SAFE_RELEASE(m_sys_profile_peak);
	SAFE_RELEASE(m_sys_profile_memory);
	SAFE_RELEASE(m_sys_profile_sampler);
	SAFE_RELEASE(m_sys_profile_sampler_max_samples);
#if defined(PS3)
	SAFE_RELEASE(m_sys_spu_profile);
	SAFE_RELEASE(m_sys_spu_enable);
	SAFE_RELEASE(m_sys_spu_debug);
	SAFE_RELEASE(m_sys_spu_dump_stats);
#endif
	SAFE_RELEASE(m_sys_spec);
	SAFE_RELEASE(m_sys_firstlaunch);
	SAFE_RELEASE(m_sys_enable_budgetmonitoring);
	SAFE_RELEASE(m_sys_SaveCVars);
	SAFE_RELEASE(m_sys_StreamCallbackTimeBudget);
	SAFE_RELEASE(m_sys_physics_CPU);
	SAFE_RELEASE(m_sys_min_step);
	SAFE_RELEASE(m_sys_max_step);

	if (m_env.pInput)
	{
		m_env.pInput->ShutDown();
		m_env.pInput = NULL;
	}

	SAFE_RELEASE(m_env.pConsole);
	SAFE_RELEASE(m_env.pScriptSystem);

	SAFE_DELETE(m_pMemStats);
	SAFE_DELETE(m_pSizer);
	m_pStreamEngine = nullptr;
	SAFE_DELETE(m_env.pCryPak);
	SAFE_DELETE(m_pDefaultValidator);

	SAFE_DELETE(m_pPhysRenderer);

#ifdef DOWNLOAD_MANAGER
	SAFE_RELEASE(m_pDownloadManager);
#endif //DOWNLOAD_MANAGER

	// CryMP: LocalizationManager is a process-global singleton owned by CryMP.
	m_pLocalizationManager = NULL;

	//DebugStats(false, false);//true);
	//CryLogAlways(" ");
	//CryLogAlways("release mode memory manager stats:");
	//DumpMMStats(true);

	// Log must be last thing released.
	SAFE_RELEASE(m_env.pLog);		// creates log backup
	SAFE_DELETE(m_pCpu);

	delete m_pCmdLine;
	m_pCmdLine = 0;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
void CSystem::Quit()
{
	m_bQuit=true;

	if(gEnv->pCryPak->GetLvlResStatus())
		LvlRes_export(0);			// executable was started with -LvlRes so it should export lvlres file on quit

#ifdef WIN32
	// Fast Quit.

	if ((m_pCVarQuit && m_pCVarQuit->GetIVal() != 0) || m_bTestMode)
	{
		GetIRenderer()->RestoreGamma();
    GetIRenderer()->ShutDownFast();

		ShareShaderCombinations();

		// HACK! to save cvars on quit.
		SaveConfiguration();

		if (m_env.pNetwork)
			m_env.pNetwork->FastShutdown();

		CryLogAlways( "System:Quit" );
		// Commit files changes to the disk.
		_flushall();

		//////////////////////////////////////////////////////////////////////////
		// Support relaunching for windows media center edition.
		//////////////////////////////////////////////////////////////////////////
#if defined(WIN32) && !defined(XENON)
		if (m_pCmdLine && strstr(m_pCmdLine->GetCommandLine(),"ReLaunchMediaCenter") != 0)
		{
			ReLaunchMediaCenter();
		}
#endif
		//////////////////////////////////////////////////////////////////////////

		// [marco] in test mode, kill the process and quit without performing full C libs cleanup
		// (for faster closing of application)
		_exit(0);
	}

	PostQuitMessage(0);
#endif
}
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
bool CSystem::IsQuitting()
{
	return (m_bQuit);
}
 
//////////////////////////////////////////////////////////////////////////
void CSystem::SetIProcess(IProcess *process)
{
	m_pProcess = process; 
	//if (m_pProcess)
		//m_pProcess->SetPMessage("");
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SleepIfInactive()
{
	// ProcessSleep()
	if (m_bDedicatedServer || m_bEditor || gEnv->bMultiplayer)
		return;
	
#if defined(WIN32) && !defined(XENON)
	WIN_HWND hRendWnd = GetIRenderer()->GetHWND();
	if(!hRendWnd)
		return;

	// Loop here waiting for window to be activated.
	for (int nLoops = 0; nLoops < 10; nLoops++)
	{
		WIN_HWND hActiveWnd = ::GetActiveWindow();
		if (hActiveWnd == hRendWnd)
			break;

		if (m_hWnd && ::IsWindow((HWND)m_hWnd))
		{
			// Peek message.
			MSG msg;
			while (PeekMessage(&msg, (HWND)m_hWnd, 0, 0, PM_REMOVE))       
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		Sleep(100);
	}
#endif      
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SleepIfNeeded()
{ 
	ITimer *const pTimer = GetISystem()->GetITimer();
	static CTimeValue prevTime = 0.f;
	static bool firstCall = true;

	typedef MiniQueue<CTimeValue, 32> PrevNow;
	static PrevNow prevNow;
	if (firstCall)
	{
		prevTime = pTimer->GetAsyncTime();
		prevNow.Push(prevTime);
		firstCall = false;
		return;
	}

	const float maxRate = m_svDedicatedMaxRate->GetFVal();
	const float minTime = 1.0f / maxRate;
	CTimeValue now = pTimer->GetAsyncTime();
	float elapsed = (now - prevTime).GetSeconds();

	if (prevNow.Full())
		prevNow.Pop();
	prevNow.Push(now);

	static bool allowStallCatchup = true;
	if (elapsed > minTime && allowStallCatchup)
	{
		allowStallCatchup = false;
		prevTime = pTimer->GetAsyncTime();
		return;
	}
	allowStallCatchup = true;

	float totalElapsed = (now - prevNow.Front()).GetSeconds();
	float wantSleepTime = CLAMP(minTime*(prevNow.Size()-1) - totalElapsed, 0, (minTime - elapsed)*0.9f);
	static float sleepTime = 0;
	sleepTime = (15*sleepTime + wantSleepTime)/16;
	int sleepMS = (int)(1000.0f*sleepTime + 0.5f);
	if (sleepMS > 0)
		Sleep(sleepMS);

	prevTime = pTimer->GetAsyncTime();
}

//////////////////////////////////////////////////////////////////////
bool CSystem::Update( int updateFlags, int nPauseMode )
{
	// do the dedicated sleep earlier than the frame profiler to avoid having it counted
	if (IsDedicated())
	{
		SleepIfNeeded();
	}

	FUNCTION_PROFILER_FAST( GetISystem(),PROFILE_SYSTEM,g_bProfilerEnabled );

	m_nUpdateCounter++;

	if(!m_sDelayedScreeenshot.empty())
	{
		gEnv->pRenderer->ScreenShot(m_sDelayedScreeenshot.c_str());
		m_sDelayedScreeenshot.clear();
	}

	if (m_sys_crashtest->GetIVal() != 0)
	{
		int *p = 0;
		*p = 0xABCD;
	}

	// Check if game needs to be sleeping when not active.
	SleepIfInactive();

	if (m_pUserCallback)
		m_pUserCallback->OnUpdate();

	//////////////////////////////////////////////////////////////////////////
	// Enable/Disable floating exceptions.
	//////////////////////////////////////////////////////////////////////////
#if defined(WIN32)
	static int prev_sys_float_exceptions = g_cvars.sys_float_exceptions;
	if (prev_sys_float_exceptions != g_cvars.sys_float_exceptions)
	{
		prev_sys_float_exceptions = g_cvars.sys_float_exceptions;

		EnableFloatExceptions( g_cvars.sys_float_exceptions );
	}
#endif
	//////////////////////////////////////////////////////////////////////////

	if(!IsEditor())
	{
		// if aspect ratio changes or is different from default we need to update camera
		float fCurrentProjRatio = GetViewCamera().GetProjRatio();
		float fNewProjRatio = fCurrentProjRatio;

		uint32 dwWidth = m_rWidth->GetIVal();
		uint32 dwHeight = m_rHeight->GetIVal();
		
		if(dwHeight)
			fNewProjRatio = (float)dwWidth / (float)dwHeight;

		if(fNewProjRatio!=fCurrentProjRatio)
			GetViewCamera().SetFrustum(m_rWidth->GetIVal(),m_rHeight->GetIVal(),GetViewCamera().GetFov(),GetViewCamera().GetNearPlane(),GetViewCamera().GetFarPlane());
	}

	// CryMP: legacy automated test system is disabled in the embedded build.

	if (nPauseMode != 0)
		m_bPaused = true;
	else
		m_bPaused = false;

	if (m_env.pGame)
	{
		//bool bDevMode = m_env.pGame->GetModuleState( EGameDevMode );
		//if (bDevMode != m_bInDevMode)
			//SetDevMode(bDevMode);
	}
#ifdef PROFILE_WITH_VTUNE
	if (m_bInDevMode)
	{	
		if (VTPause != NULL && VTResume != NULL)
		{
			static bool bVtunePaused = true;
			
			bool bPaused = false;
			
			if (GetISystem()->GetIInput())
			{
				bPaused = !(GetKeyState(VK_SCROLL) & 1);
			}

			{
				if (bVtunePaused && !bPaused)
				{
					VTuneResume();
				}
				if (!bVtunePaused && bPaused)
				{
					VTunePause();
				}
				bVtunePaused = bPaused;
			}
		}
	}
#endif //PROFILE_WITH_VTUNE

#ifndef LINUX
	if (m_pStreamEngine)
	{
		FRAME_PROFILER( "IStreamEngine::Update()", this, PROFILE_SYSTEM );
		m_pStreamEngine->Update(0);
		// CryMP StreamEngine has no mutable callback-time budget; quota is managed internally.
	}
#endif	

	if (m_bIgnoreUpdates)
		return true;

	if (m_env.pCharacterManager)
		m_env.pCharacterManager->Update();

  //static bool sbPause = false; 
	//bool bPause = false;

	//check what is the current process 
	IProcess *pProcess=GetIProcess();
	if (!pProcess)
		return (true); //should never happen

	bool bNoUpdate = false;
	if (m_sysNoUpdate && m_sysNoUpdate->GetIVal())
	{
		bNoUpdate = true;
		updateFlags = ESYSUPDATE_IGNORE_AI | ESYSUPDATE_IGNORE_PHYSICS;
	}

	//if ((pProcess->GetFlags() & PROC_MENU) || (m_sysNoUpdate && m_sysNoUpdate->GetIVal()))
	//		bPause = true;

	//check if we are quitting from the game
	if (IsQuitting())
		return (false);
	

#ifndef _XBOX
#ifdef WIN32
  // process window messages
	{
		FRAME_PROFILER( "SysUpdate:PeekMessage",this,PROFILE_SYSTEM );

		if (m_hWnd && ::IsWindow((HWND)m_hWnd))
		{
			MSG msg;
			while (PeekMessage(&msg, (HWND)m_hWnd, 0, 0, PM_REMOVE))       
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
  }
#endif
#endif

	//////////////////////////////////////////////////////////////////////
	//update time subsystem	
	m_Time.UpdateOnFrameStart();

	if (m_bReloadTextures && gEnv->pRenderer)
	{
		gEnv->pRenderer->EF_ReloadTextures();
		m_bReloadTextures=false;
	}
	
	if (m_env.p3DEngine)
		m_env.p3DEngine->OnFrameStart();

	//////////////////////////////////////////////////////////////////////
	// update rate limiter for dedicated server
	if (m_pServerThrottle.get())
		m_pServerThrottle->Update();

	float fFrameTime = m_Time.GetFrameTime();

	//////////////////////////////////////////////////////////////////////
	// initial network update
	if (m_env.pNetwork)
	{
		m_env.pNetwork->SyncWithGame(eNGS_FrameStart);
	}

	//////////////////////////////////////////////////////////////////////////
	// Update script system.
	if (m_env.pScriptSystem)
	{
		FRAME_PROFILER( "SysUpdate:ScriptSystem",this,PROFILE_SYSTEM );
		m_env.pScriptSystem->Update();
	}
	
	if (m_env.pInput)
	{
		if (!(updateFlags&ESYSUPDATE_EDITOR))
		{
			//////////////////////////////////////////////////////////////////////
			//update input system
#ifndef WIN32
			m_env.pInput->Update(true);
#else
			bool bFocus = (GetFocus()==m_hWnd) || m_bEditor;
			m_env.pInput->Update(bFocus);
#endif
		}
	}

	//////////////////////////////////////////////////////////////////////
	//update console system
	if (m_env.pConsole)
	{
		FRAME_PROFILER( "SysUpdate:Console",this,PROFILE_SYSTEM );

		if (!(updateFlags&ESYSUPDATE_EDITOR))
			m_env.pConsole->Update();
	}

	//////////////////////////////////////////////////////////////////////
	//update sound system Part 1
	if ((nPauseMode!=1) && m_env.pSoundSystem && !bNoUpdate)
	{
		// updating the Listener Position in a first separate step
		m_env.pSoundSystem->SetListener(LISTENERID_STANDARD, m_ViewCamera.GetMatrix(), Vec3(0,0,0), true, 1.0);
		m_env.pSoundSystem->Update(eSoundUpdateMode_Listeners);
	}
	else
	{
		int a = 0;
	}


	//////////////////////////////////////////////////////////////////////	
	// update entity system (a little bit) before physics
	if(nPauseMode!=1)
	{
		//////////////////////////////////////////////////////////////////////
		//update entity system	
		if (m_env.pEntitySystem && !bNoUpdate && g_cvars.sys_entitysystem)
			m_env.pEntitySystem->PrePhysicsUpdate();
	}

	//////////////////////////////////////////////////////////////////////////
	// Update Threads Task Manager.
	//////////////////////////////////////////////////////////////////////////
	m_pThreadTaskManager->OnUpdate();

	//////////////////////////////////////////////////////////////////////	
	// update physic system	
	//static float time_zero = 0;

#if !defined(CRYSIS_BETA)
	if (m_sys_physics_CPU->GetIVal()>0 && !IsDedicated())
		CreatePhysicsThread();
	else
#endif
		KillPhysicsThread();
	static int g_iPausedPhys = 0;
	PhysicsVars *pVars = m_env.pPhysicalWorld->GetPhysVars();
	pVars->threadLag = 0;

	if (!m_PhysThread.IsRunning())
	{
		FRAME_PROFILER( "SysUpdate:AllAIAndPhysics",this,PROFILE_SYSTEM );
		// intermingle physics/AI updates so that if we get a big timestep (frame rate glitch etc) the
		// AI gets to steer entities before they travel over cliffs etc.
		float maxTimeStep = 0.0f;
		if (m_env.pAISystem)
			maxTimeStep = m_env.pAISystem->GetUpdateInterval();
		else
			maxTimeStep = 0.25f;
		int maxSteps = 1;
		float fCurTime = m_Time.GetCurrTime();
		float fPrevTime = m_env.pPhysicalWorld->GetPhysicsTime();
		float timeToDo = m_Time.GetFrameTime();//fCurTime - fPrevTime;
		if (m_env.bMultiplayer)
			timeToDo = m_Time.GetRealFrameTime();
		m_env.pPhysicalWorld->TracePendingRays();
		while (timeToDo > 0.0001f && maxSteps-- > 0)
		{
			float thisStep = min(maxTimeStep, timeToDo);
			timeToDo -= thisStep;

			if ((nPauseMode!=1) && !(updateFlags&ESYSUPDATE_IGNORE_PHYSICS) && g_cvars.sys_physics && !bNoUpdate)
			{
				FRAME_PROFILER( "SysUpdate:physics",this,PROFILE_SYSTEM );

				int iPrevTime=m_env.pPhysicalWorld->GetiPhysicsTime();
				float fPrevTime=m_env.pPhysicalWorld->GetPhysicsTime();
				pVars->bMultithreaded = 0;
				pVars->timeScalePlayers = 1.0f;
				if (!(updateFlags&ESYSUPDATE_MULTIPLAYER))
					m_env.pPhysicalWorld->TimeStep(thisStep);
				else
				{
					//@TODO: fixed step in game.
					/*
					if (m_env.pGame->UseFixedStep())
					{
						m_env.pPhysicalWorld->TimeStep(fCurTime-fPrevTime, 0);
						int iCurTime = m_env.pPhysicalWorld->GetiPhysicsTime();

						m_env.pPhysicalWorld->SetiPhysicsTime(m_env.pGame->SnapTime(iPrevTime));
						int i, iStep=m_env.pGame->GetiFixedStep();
						float fFixedStep = m_env.pGame->GetFixedStep();
						for(i=min(20*iStep,m_env.pGame->SnapTime(iCurTime)-m_pGame->SnapTime(iPrevTime)); i>0; i-=iStep)
						{
							m_env.pGame->ExecuteScheduledEvents();
							m_env.pPhysicalWorld->TimeStep(fFixedStep, ent_rigid|ent_skip_flagged);
						}

						m_env.pPhysicalWorld->SetiPhysicsTime(iPrevTime);
						m_env.pPhysicalWorld->TimeStep(fCurTime-fPrevTime, ent_rigid|ent_flagged_only);

						m_env.pPhysicalWorld->SetiPhysicsTime(iPrevTime);
						m_env.pPhysicalWorld->TimeStep(fCurTime-fPrevTime, ent_living|ent_independent|ent_deleted);
					}
					else
					*/
						m_env.pPhysicalWorld->TimeStep(thisStep);

				}
				g_iPausedPhys = 0;
			}	else if (!(g_iPausedPhys++ & 31))
				m_env.pPhysicalWorld->TimeStep(0);	// make sure objects get all notifications; flush deleted ents

			{ FRAME_PROFILER( "SysUpdate:PumpLoggedEvents",this,PROFILE_SYSTEM );
				m_env.pPhysicalWorld->PumpLoggedEvents();
			}

			// now AI
			if ((nPauseMode==0) && !(updateFlags&ESYSUPDATE_IGNORE_AI) && g_cvars.sys_ai && !bNoUpdate)
			{
				FRAME_PROFILER( "SysUpdate:AI",this,PROFILE_SYSTEM );
				//////////////////////////////////////////////////////////////////////
				//update AI system - match physics
				if (m_env.pAISystem && !m_cvAIUpdate->GetIVal() && g_cvars.sys_ai)
					m_env.pAISystem->Update(gEnv->pTimer->GetFrameStartTime(), gEnv->pTimer->GetFrameTime());
			}
		}

		// Make sure we don't lag too far behind
		if ((nPauseMode!=1) && !(updateFlags&ESYSUPDATE_IGNORE_PHYSICS))
		{
			if (fabsf(m_env.pPhysicalWorld->GetPhysicsTime()-fCurTime)>0.01f)
			{
				//GetILog()->LogToConsole("Adjusting physical world clock by %.5f", fCurTime-m_env.pPhysicalWorld->GetPhysicsTime());
				m_env.pPhysicalWorld->SetPhysicsTime(fCurTime);
			}
		}
	}
	else
	{
		{ FRAME_PROFILER( "SysUpdate:PumpLoggedEvents",this,PROFILE_SYSTEM );
			m_env.pPhysicalWorld->PumpLoggedEvents();
		}

		if ((nPauseMode!=1) && !(updateFlags&ESYSUPDATE_IGNORE_PHYSICS))
		{
			m_PhysThread.Resume();
			float lag = m_PhysThread.GetRequestedStep();
			if (m_PhysThread.RequestStep(m_Time.GetFrameTime()))
			{
				pVars->threadLag = lag+m_Time.GetFrameTime();
				//GetILog()->Log("Physics thread lags behind; accum time %.3f", pVars->threadLag);
			}
		} else
		{
			m_PhysThread.Pause();
			m_env.pPhysicalWorld->TracePendingRays();
			m_env.pPhysicalWorld->TimeStep(0);
		}

		if ((nPauseMode==0) && !(updateFlags&ESYSUPDATE_IGNORE_AI) && g_cvars.sys_ai && !bNoUpdate)
		{
			FRAME_PROFILER( "SysUpdate:AI",this,PROFILE_SYSTEM );
			//////////////////////////////////////////////////////////////////////
			//update AI system
			if (m_env.pAISystem && !m_cvAIUpdate->GetIVal())
				m_env.pAISystem->Update(gEnv->pTimer->GetFrameStartTime(), gEnv->pTimer->GetFrameTime());
		}
	}
	pe_params_waterman pwm;
	pwm.posViewer = GetViewCamera().GetPosition();
	m_env.pPhysicalWorld->SetWaterManagerParams(&pwm);

	//////////////////////////////////////////////////////////////////////////
	// fix to solve a flaw in the system
	// Update movie system (before entity update is better for having artifact free movie playback)
	//////////////////////////////////////////////////////////////////////////	
	if(m_cvEarlyMovieUpdate->GetIVal()!=0)
	{
		if(!bNoUpdate)
			UpdateMovieSystem(updateFlags,fFrameTime);
	}

	if(nPauseMode!=1)
	{
		//////////////////////////////////////////////////////////////////////
		// reset volumetric fog modifiers before (fog) entities get updated
		m_env.p3DEngine->SetVolumetricFogModifiers(0, 0, true);

		//////////////////////////////////////////////////////////////////////
		//update entity system	
		if (m_env.pEntitySystem && !bNoUpdate && g_cvars.sys_entitysystem)
			m_env.pEntitySystem->Update();
	}

	//////////////////////////////////////////////////////////////////////////
	// Update movie system (Must be after updating EntitySystem and AI.
	//////////////////////////////////////////////////////////////////////////	
	// the movie system already disables AI physics etc.
	if(m_cvEarlyMovieUpdate->GetIVal()==0)
	{
		if(!bNoUpdate)
			UpdateMovieSystem(updateFlags,fFrameTime);
	}

	//////////////////////////////////////////////////////////////////////
	//update process (3D engine)
	if (!(updateFlags&ESYSUPDATE_EDITOR) && !bNoUpdate)
	{
		if (ITimeOfDay * pTOD = m_env.p3DEngine->GetTimeOfDay())
			pTOD->Tick();

		if (m_pProcess && (m_pProcess->GetFlags() & PROC_3DENGINE))
		{
			if ((nPauseMode!=1))
			if (!IsEquivalent(m_ViewCamera.GetPosition(),Vec3(0,0,0),VEC_EPSILON))
			{			
				if (m_env.p3DEngine)
				{
//					m_env.p3DEngine->SetCamera(m_ViewCamera);
					m_pProcess->Update();

					//////////////////////////////////////////////////////////////////////////
					// Strange, !do not remove... ask Timur for the meaning of this.
					//////////////////////////////////////////////////////////////////////////
					if (m_nStrangeRatio > 32767)
					{
						gEnv->pScriptSystem->SetGCFrequency(-1); // lets get nasty.
					}
					//////////////////////////////////////////////////////////////////////////
					// Strange, !do not remove... ask Timur for the meaning of this.
					//////////////////////////////////////////////////////////////////////////
					if (m_nStrangeRatio > 1000)
					{
						if (m_pProcess && (m_pProcess->GetFlags() & PROC_3DENGINE))
							m_nStrangeRatio += (1 + (10*rand())/RAND_MAX);
					}
					//////////////////////////////////////////////////////////////////////////
				}
			}
		}
		else
		{
			if (m_pProcess)
				m_pProcess->Update();
		}
	}

	//////////////////////////////////////////////////////////////////////
	//update sound system part 2
  if (m_env.pSoundSystem && !bNoUpdate)
	{
		FRAME_PROFILER( "SysUpdate:Sound",this,PROFILE_SYSTEM );

		// Listener position was already updated above, now we just call the update function
		ESoundUpdateMode UpdateMode = (ESoundUpdateMode)(eSoundUpdateMode_All & ~eSoundUpdateMode_Listeners);
    m_env.pSoundSystem->Update(UpdateMode);
	}
	else
	{
		int b = 0;
	}

	if (m_env.pMusicSystem && !bNoUpdate)
	{
		FRAME_PROFILER( "SysUpdate:Music",this,PROFILE_SYSTEM );

		m_env.pMusicSystem->Update();
	}

	//////////////////////////////////////////////////////////////////////
	// final network update
	if (m_env.pNetwork)
	{
		m_env.pNetwork->SyncWithGame(eNGS_FrameEnd);
	}

#ifdef DOWNLOAD_MANAGER
	if (m_pDownloadManager && !bNoUpdate)
	{
		m_pDownloadManager->Update();
	}
#endif

	return !m_bQuit;	
}


int CPhysThread::Pause()
{
	if (m_bIsActive) 
	{
		Lock();
		gEnv->pPhysicalWorld->GetPhysVars()->lastTimeStep = 0;
		m_bIsActive = 0;
		return 1;
	}
	gEnv->pPhysicalWorld->GetPhysVars()->lastTimeStep = 0;
	return 0;
}

int CPhysThread::Resume()
{
	if (!m_bIsActive && IsRunning())
	{
		Unlock();
		m_bIsActive = 1;
		return 1;
	}
	return 0;
}


void CSystem::CreatePhysicsThread()
{
	if (!m_PhysThread.IsRunning() && (unsigned int)m_sys_physics_CPU->GetIVal()<m_pCpu->GetPhysCPUCount())
		m_PhysThread.Start(m_pCpu->GetPhysCPUAffinityMask(m_sys_physics_CPU->GetIVal()));
}

void CPhysThread::Cancel()
{
	Pause();
	m_bStopRequested = 1;
	NotifySingle();
	Resume();
	m_bIsActive = 0;
}

void CSystem::KillPhysicsThread()
{
	if (m_PhysThread.IsRunning())
	{
		m_PhysThread.Cancel();
		while(m_PhysThread.IsRunning());
		m_PhysThread.Stop();
	}
}


void CPhysThread::Run()
{
	CryThreadSetName( -1,"Physics" );
	Lock();
	float step,timeTaken,kSlowdown=1.0f;
	int nSlowFrames=0;
	CTimeValue timeStart;
#ifdef WIN32
	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
#endif 

	while(true)
	{
		Wait();
		if (m_bStopRequested)	{
			Unlock();
			return;
		}
		while ((step=m_stepRequested)>0)
		{
			m_stepRequested=0; m_bProcessing=1;
			PhysicsVars *pVars = gEnv->pPhysicalWorld->GetPhysVars();
			pVars->bMultithreaded = 1;
			gEnv->pPhysicalWorld->TracePendingRays();
			if (kSlowdown!=1.0f) 
			{
				step = max(1,FtoI(step*kSlowdown*50-0.5f))*0.02f;
				pVars->timeScalePlayers = 1.0f/max(kSlowdown,0.2f);
			}	else
				pVars->timeScalePlayers = 1.0f;
			step = min(step, pVars->maxWorldStep);
			timeStart = gEnv->pTimer->GetAsyncTime();
			gEnv->pPhysicalWorld->TimeStep(step);
			timeTaken = (gEnv->pTimer->GetAsyncTime()-timeStart).GetSeconds();
			if (timeTaken>step*0.9f)
			{
				if (++nSlowFrames>5)
					kSlowdown = step*0.9f/timeTaken;
			} else 
				kSlowdown=1.0f, nSlowFrames=0;
			m_bProcessing=0;
			//int timeSleep = (int)((m_timeTarget-gEnv->pTimer->GetAsyncTime()).GetMilliSeconds()*0.9f);
			//Sleep(max(0,timeSleep));
		}
	}
}


void CSystem::UpdateMovieSystem( const int updateFlags, const float fFrameTime )
{
	if (m_env.pMovieSystem && !(updateFlags&ESYSUPDATE_EDITOR) && g_cvars.sys_trackview)
	{
		float fMovieFrameTime = fFrameTime;
		
		if (fMovieFrameTime > 0.1f) // Slow frame rate fix.
			fMovieFrameTime = 0.1f;

		m_env.pMovieSystem->Update(fMovieFrameTime);
	}
}

//////////////////////////////////////////////////////////////////////////
// XML stuff
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CSystem::CreateXmlNode( const char *sNodeName )
{
	return new CXmlNode( sNodeName );
}

//////////////////////////////////////////////////////////////////////////
IXmlUtils* CSystem::GetXmlUtils()
{
	return m_pXMLUtils;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CSystem::LoadXmlFile( const char *sFilename )
{
	return m_pXMLUtils->LoadXmlFile(sFilename);
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CSystem::LoadXmlFromString( const char *sXmlString )
{
	return m_pXMLUtils->LoadXmlFromString(sXmlString);
}

//////////////////////////////////////////////////////////////////////////
bool CSystem::CheckLogVerbosity( int verbosity )
{
	if (verbosity <= m_env.pLog->GetVerbosityLevel())
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::Warning( EValidatorModule module,EValidatorSeverity severity,int flags,const char *file,const char *format,... )
{
	va_list	ArgList;
	char		szBuffer[MAX_WARNING_LENGTH];
	va_start(ArgList, format);
	vsnprintf(szBuffer, MAX_WARNING_LENGTH, format, ArgList);

	ILog::ELogType ltype = (severity==VALIDATOR_ERROR) ? ILog::eError : ILog::eWarning;

	if (file && *file)
	{
		char dest[ICryPak::g_nMaxPath];
		dest[0] = '\0';
		const char* adjustedFilename = gEnv->pCryPak->AdjustFileName(file, dest, 0);

		CryFixedStringT<MAX_WARNING_LENGTH> fmt(format);
		fmt += " [File=";
		fmt += adjustedFilename;
		fmt += "]";

		m_env.pLog->LogV( ltype, fmt.c_str(), ArgList);
	}
	else
		m_env.pLog->LogV( ltype, format, ArgList);

	va_end(ArgList);

	//if(file)
		//m_env.pLog->LogWithType( ltype, "  ... caused by file '%s'",file);

	SValidatorRecord record;
	record.file = file;
	record.text = szBuffer;
	record.module = module;
	record.severity = severity;
	record.flags = flags;
	m_pValidator->Report( record );
}

//////////////////////////////////////////////////////////////////////////
void CSystem::VTuneResume()
{
#ifdef PROFILE_WITH_VTUNE
	if (VTResume)
	{
		CryLogAlways("VTune Resume");
		VTResume();
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CSystem::VTunePause()
{
#ifdef PROFILE_WITH_VTUNE
	if (VTPause)
	{
		VTPause();
		CryLogAlways("VTune Pause");
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CSystem::Deltree(const char *szFolder, bool bRecurse)
{
	__finddata64_t fd;
	string filespec = szFolder;
	filespec += "*.*";

	intptr_t hfil = 0;
	if ((hfil = _findfirst64(filespec.c_str(), &fd)) == -1)
	{
		return;
	}

	do
	{
		if (fd.attrib & _A_SUBDIR)
		{
			string name = fd.name;

			if ((name != ".") && (name != ".."))
			{
				if (bRecurse)
				{
					name = szFolder;
					name += fd.name;
					name += "/";

					Deltree(name.c_str(), bRecurse);
				}
			}
		}
		else
		{
			string name = szFolder;
			
			name += fd.name;

			DeleteFile(name.c_str());
		}

	} while(!_findnext64(hfil, &fd));

	_findclose(hfil);

	RemoveDirectory(szFolder);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OpenBasicPaks()
{
	ICVar *pCVar = m_env.pConsole->GetCVar( "g_language" );
	//////////////////////////////////////////////////////////////////////////
	// load language pak
	if (!pCVar)
	{
		// if the language value cannot be found, let's default to the english pak
		OpenLanguagePak("english"); 
	}
	else
	{ 
		OpenLanguagePak( pCVar->GetString() );
	}

	string paksFolder = string(DATA_FOLDER)+"/*.pak";
	// Open all *.pak files in root folder.
	//m_env.pCryPak->OpenPacks( "*.pak" );
	m_env.pCryPak->OpenPacks( paksFolder.c_str() ); 

	if (const ICmdLineArg *pModArg = GetICmdLine()->FindArg(eCLAT_Pre,"MOD"))
	{
		if (IsMODValid(pModArg->GetValue()))
		{		
			string paksModFolder = string("Mods\\") + string(pModArg->GetValue())+ string("\\") + string(DATA_FOLDER)+"\\*.pak";
			GetIPak()->OpenPacks("", paksModFolder.c_str());
		}
	}

	// Open all *.pak files in FCData folder.
	// m_env.pCryPak->OpenPacks( string(DATA_FOLDER)+"/","FCData/*.pak" ); 
}

//////////////////////////////////////////////////////////////////////////
void CSystem::OpenLanguagePak( const char *sLanguage )
{	
	// Initialize languages.

	string sLocalizedPathBase = string(DATA_FOLDER)+"/Localized/";

	// load localized pak.
	string sLocalizedPath = sLocalizedPathBase + sLanguage + ".pak";
	string bindRoot = string("./") + DATA_FOLDER;
	if (!m_env.pCryPak->OpenPacks( bindRoot.c_str(), sLocalizedPath.c_str() ))
	{
		// make sure the localized language is found - not really necessary, for TC		
		CryLogAlways("Localized language content(%s) not available or modified from the original installation.",sLanguage );
	}

	// Try to open Englis1.pak, Englis2.pak, Englis3.pak, Englis4.pak ...
	sLocalizedPath = sLocalizedPathBase + sLanguage + "1.pak";
	m_env.pCryPak->OpenPacks( bindRoot.c_str(), sLocalizedPath.c_str() );
	sLocalizedPath = sLocalizedPathBase + sLanguage + "2.pak";
	m_env.pCryPak->OpenPacks( bindRoot.c_str(), sLocalizedPath.c_str() );
	sLocalizedPath = sLocalizedPathBase + sLanguage + "3.pak";
	m_env.pCryPak->OpenPacks( bindRoot.c_str(), sLocalizedPath.c_str() );
	sLocalizedPath = sLocalizedPathBase + sLanguage + "4.pak";
	m_env.pCryPak->OpenPacks( bindRoot.c_str(), sLocalizedPath.c_str() );
	sLocalizedPath = sLocalizedPathBase + sLanguage + "5.pak";
	m_env.pCryPak->OpenPacks( bindRoot.c_str(), sLocalizedPath.c_str() );
	sLocalizedPath = sLocalizedPathBase + sLanguage + "6.pak";
	m_env.pCryPak->OpenPacks( bindRoot.c_str(), sLocalizedPath.c_str() );

	if (!m_bPreviewMode)
	{
		// Load localization xml.
		// bad, we should load any XML files in Languages
		GetLocalizationManager()->LoadExcelXmlSpreadsheet( "Languages/dialog_recording_list.xml" );
		GetLocalizationManager()->LoadExcelXmlSpreadsheet( "Languages/ai_dialog_recording_list.xml" );
		GetLocalizationManager()->LoadExcelXmlSpreadsheet( "Languages/ui_dialog_recording_list.xml" );
		// CryMP/Crysis 1: these two files are used by the menu and multiplayer UI.
		GetLocalizationManager()->LoadExcelXmlSpreadsheet( "Languages/ui_text_messages.xml" );
		GetLocalizationManager()->LoadExcelXmlSpreadsheet( "Languages/mp_text_messages.xml" );
		GetLocalizationManager()->LoadExcelXmlSpreadsheet( "Languages/game_text_messages.xml" );
		GetLocalizationManager()->LoadExcelXmlSpreadsheet( "Languages/game_controls.xml" );
		GetLocalizationManager()->LoadExcelXmlSpreadsheet( "Languages/ps_basic_tutorial_subtitles.xml" );
		GetLocalizationManager()->LoadExcelXmlSpreadsheet( "Languages/ui_credit_list.xml" );
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystem::Strange()
{
	m_nStrangeRatio += (1 + (100*rand())/RAND_MAX);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::Relaunch(bool bRelaunch)
{
	if (m_sys_firstlaunch)
		m_sys_firstlaunch->Set( "0" );

	m_bRelaunch = bRelaunch;
	SaveConfiguration();
}

//////////////////////////////////////////////////////////////////////////
ICrySizer* CSystem::CreateSizer()
{
	return new CrySizerImpl;
}

//////////////////////////////////////////////////////////////////////////
uint32 CSystem::GetUsedMemory()
{
	return CryMemoryGetAllocatedSize();
}

//////////////////////////////////////////////////////////////////////////
ILocalizationManager* CSystem::GetLocalizationManager()
{
	// CryMP: keep the Crysis 1 localization manager authoritative.
	return &LocalizationManager::GetInstance();
}

//////////////////////////////////////////////////////////////////////////
IThreadTaskManager* CSystem::GetIThreadTaskManager()
{
	return m_pThreadTaskManager;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::ApplicationTest( const char *szParam )
{
	assert(szParam);

	(void)szParam;
	CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING,
		"ApplicationTest is unavailable in the embedded CrySystem build");
}

void CSystem::ExecuteCommandLine()
{
	// 
	if(gEnv->pCryPak->GetRecordFileOpenList()==ICryPak::RFOM_EngineStartup)
	{
//		gEnv->pCryPak->GetRecorderdResourceList(ICryPak::RFOM_Level)->Clear();
		gEnv->pCryPak->RecordFileOpen(ICryPak::RFOM_Level);
	}

	// auto detect system spec (overrides profile settings)
	if (m_pCmdLine->FindArg(eCLAT_Pre,"autodetect"))
		AutoDetectSpec();

	// execute command line arguments e.g. +g_gametype ASSAULT +map "testy"

	ICmdLine *pCmdLine = GetICmdLine();			assert(pCmdLine);

	const int iCnt = pCmdLine->GetArgCount();

	for(int i=0;i<iCnt;++i)
	{
		const ICmdLineArg *pCmd=pCmdLine->GetArg(i);

		if(pCmd->GetType()==eCLAT_Post)
		{
			string sLine = pCmd->GetName();

			if(pCmd->GetValue())
				sLine += string(" ") + pCmd->GetValue();

			GetILog()->Log("Executing command from command line: %s",sLine.c_str());  // - the actual command might be executed much later (e.g. level load pause)
			GetIConsole()->ExecuteString(sLine.c_str());
		}
	}

	gEnv->pConsole->ExecuteString("sys_RestoreSpec test*"); // to get useful debugging information about current spec settings to the log file
}

void CSystem::DumpMemoryCoverage()
{
	m_MemoryFragmentationProfiler.DumpMemoryCoverage();
}

ITextModeConsole * CSystem::GetITextModeConsole()
{
#if defined(USE_UNIXCONSOLE)
	if (m_bDedicatedServer)
		return &g_UnixConsole;
#endif
	return 0;
}

//////////////////////////////////////////////////////////////////////////
ESystemConfigSpec CSystem::GetConfigSpec( bool bClient )
{
	if (bClient)
	{
		if (m_sys_spec)
			return (ESystemConfigSpec)m_sys_spec->GetIVal();
		return CONFIG_VERYHIGH_SPEC; // highest spec.
	}
	else
		return m_nServerConfigSpec;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::SetConfigSpec( ESystemConfigSpec spec,bool bClient )
{
	if (bClient)
	{
		if (m_sys_spec)
			m_sys_spec->Set( (int)spec );
	}
	else
	{
		m_nServerConfigSpec = spec;
	}
}

//////////////////////////////////////////////////////////////////////////
ESystemConfigSpec CSystem::GetMaxConfigSpec() const
{ 
	return m_nMaxConfigSpec; 
}

//////////////////////////////////////////////////////////////////////////
Crc32Gen* CSystem::GetCrc32Gen()
{
	static Crc32Gen m_crcGen;
	return &m_crcGen;
}

//////////////////////////////////////////////////////////////////////////
CPNoise3* CSystem::GetNoiseGen()
{
	static CPNoise3 m_pNoiseGen;	
	return &m_pNoiseGen;
}
