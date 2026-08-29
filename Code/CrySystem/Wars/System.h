//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
// 
//	File: System.h	
// 
//	History:
//	-Jan 31,2001:Originally Created by Marco Corbetta
//	-: modified by all
//
//////////////////////////////////////////////////////////////////////

#ifndef SYSTEM_H
#define SYSTEM_H

#if _MSC_VER > 1000
# pragma once
#endif

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CryPhysics/IPhysics.h"
#include "CryThread.h"

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
#include <IPhysicsGPU.h>
#endif

#include "Timer.h"
#include "CryCommon/CrySystem/CryVersion.h"
#include "CmdLine.h"
#include "CryCommon/CryCore/CryName.h"
#include "CryCommon/CryMath/Cry_Camera.h"

#include "FrameProfileSystem.h"
#include "MTSafeAllocator.h"
#include "CPUDetect.h"
#include "PakVars.h"
#include "MemoryFragmentationProfiler.h"	// CMemoryFragmentationProfiler
#include "GlobalTaskScheduler.h"					// CGlobalTaskScheduler
#include "ThreadTask.h"


//////////////////////////////////////////////////////////////////////////
// Folders are loaded from the Game/Config/Folders.ini file.
//////////////////////////////////////////////////////////////////////////
enum ECryEngineFolders
{
	ENGINE_FOLDER_USER,  // UserFolder
	ENGINE_FOLDER_LAST
};


struct ISoundSystem;
struct IConsoleCmdArgs;
class CServerThrottle;

//#if defined(LINUX) || defined(WIN32)
// The console _should_ work on AMD64, but is untested, so I'll restrict the
// console to Win32 for now.
#if !defined(CRYMP_EMBEDDED_CRYSYSTEM) && (defined(LINUX) || (defined(WIN32) && defined(_CPU_X86)) || defined(WIN64))
#define USE_UNIXCONSOLE 1
#endif

#if !defined(_XBOX) && !defined(LINUX) && !defined(PS3)
#define DOWNLOAD_MANAGER
#endif

#ifdef DOWNLOAD_MANAGER
#include "DownloadManager.h"
#endif //DOWNLOAD_MANAGER

#if defined(LINUX)
	#include "CryLibrary.h"
#endif

#ifdef WIN32
typedef void* WIN_HMODULE;
#else
typedef void* WIN_HMODULE;
#endif

//forward declarations
class CScriptSink;
class CLUADbg;
struct IMusicSystem;
struct SDefaultValidator;
struct IDataProbe;
class CPhysRenderer;
struct IFileChangeMonitor;

#define PHSYICS_OBJECT_ENTITY 0

typedef void (__cdecl *VTuneFunction)(void);
extern VTuneFunction VTResume;
extern VTuneFunction VTPause;

struct SSystemCVars
{
	int sys_streaming_sleep;
	int sys_float_exceptions;
	int sys_no_crash_dialog;
	int sys_WER;
	int sys_ai;
	int sys_physics;
	int sys_entitysystem;
	int sys_trackview;
	int sys_vtune;
};
extern SSystemCVars g_cvars;

class CSystem;
class CPhysThread : public CryThread<CryRunnable>
{
public:
	CPhysThread() { m_bStopRequested=0; m_bIsActive=0; m_stepRequested=0; m_bProcessing=0; }
	virtual void Run();
	virtual void Cancel();
	virtual void Start(unsigned cpuMask = 0) { m_bStopRequested=0; m_bIsActive=1; CryThread<CryRunnable>::Start(cpuMask); }
	virtual void Start(CryRunnable &runnable, unsigned cpuMask=0) { m_bStopRequested=0; m_bIsActive=1; CryThread<CryRunnable>::Start(runnable,cpuMask); }

	int Pause();
	int Resume();
	int IsActive() { return m_bIsActive; }
	int RequestStep(float dt) { 
		//Lock(); 
		m_stepRequested += dt; 
		//Unlock();
		NotifySingle(); 
		return m_bProcessing; 
	}
	float GetRequestedStep() { return m_stepRequested; }

protected:
	volatile int m_bStopRequested;
	volatile int m_bIsActive; 
	volatile float m_stepRequested;
	volatile int m_bProcessing;
};

/*
===========================================
The System interface Class
===========================================
*/
class CXConsole;
//////////////////////////////////////////////////////////////////////
//!	ISystem implementation
class CSystem :public ISystem, public ILoadConfigurationEntrySink
{
public:
	CSystem();
	~CSystem();
	bool IsDedicated(){return m_bDedicatedServer;}
	bool IsEditor(){return m_bEditor;}
	bool IsEditorMode();

	// interface ILoadConfigurationEntrySink ----------------------------------

	virtual void OnLoadConfigurationEntry( const char *szKey, const char *szValue, const char *szGroup );

	///////////////////////////////////////////////////////////////////////////
	//! @name ISystem implementation
	//@{ 
	virtual bool Init(const SSystemInitParams &startupParams);
	virtual void Release();

	virtual SSystemGlobalEnvironment* GetGlobalEnvironment() { return &m_env; }

	const char* GetRootFolder() const { return m_root.c_str(); }

	// Release all resources.
	void	ShutDown();
	virtual bool Update( int updateFlags=0, int nPauseMode=0);

	//! Begin rendering frame.
	void	RenderBegin();
	//! Render subsystems.
	void	Render();
	//! End rendering frame and swap back buffer.
	void	RenderEnd();

	//! Update screen during loading.
	void UpdateLoadingScreen();

	//! Renders the statistics; this is called from RenderEnd, but if the 
	//! Host application (Editor) doesn't employ the Render cycle in ISystem,
	//! it may call this method to render the essencial statistics
	void RenderStatistics ();

	virtual void* AllocMem( void* oldptr, size_t newsize ) { return ModuleAlloc( oldptr, newsize); }

	uint32 GetUsedMemory();

	virtual void DumpMemoryUsageStatistics(bool bUseKB);
	virtual void DumpMemoryCoverage();


	void Relaunch( bool bRelaunch );
	bool IsRelaunch() const { return m_bRelaunch; };
	void SerializingFile( int mode ) { m_iLoadingMode = mode; }
	int  IsSerializingFile() const { return m_iLoadingMode; }
	void Quit();
	bool IsQuitting();
	void SetAffinity();
  const char *GetUserName();
	
	IGame						*GetIGame(){ return m_env.pGame; }
	INetwork				*GetINetwork(){ return m_env.pNetwork; }
	IRenderer				*GetIRenderer(){ return m_env.pRenderer; }
	IInput					*GetIInput(){ return m_env.pInput; }
	ITimer					*GetITimer(){ return m_env.pTimer; }
	ICryPak					*GetIPak() { return m_env.pCryPak; };
	IConsole				*GetIConsole() { return m_env.pConsole; };
	IGlobalTaskScheduler *GetIGlobalTaskScheduler() { return &m_GlobalTaskScheduler; }
	IScriptSystem		*GetIScriptSystem(){ return m_env.pScriptSystem; }
	I3DEngine				*GetI3DEngine(){ return m_env.p3DEngine; }
	ICharacterManager *GetIAnimationSystem() {return m_env.pCharacterManager;}
	ISoundSystem		*GetISoundSystem(){ return m_env.pSoundSystem; }
	IMusicSystem		*GetIMusicSystem(){ return m_env.pMusicSystem; }
  IPhysicalWorld	*GetIPhysicalWorld(){ return m_env.pPhysicalWorld;}
	IMovieSystem		*GetIMovieSystem() { return m_env.pMovieSystem; };
	IAISystem				*GetAISystem(){ return m_env.pAISystem;}
	IMemoryManager	*GetIMemoryManager(){ return m_pMemoryManager;}
	IEntitySystem		*GetIEntitySystem(){ return m_env.pEntitySystem;}
	ICryFont				*GetICryFont(){ return m_env.pCryFont; }
	ILog						*GetILog()
	{ 
#if defined(__SPU__)
		return GetISPULog();
#else
		return m_env.pLog; 
#endif
	}
	ICmdLine				*GetICmdLine(){ return m_pCmdLine; }
	IStreamEngine   *GetStreamEngine();
	IValidator			*GetIValidator() { return m_pValidator; };
	IFrameProfileSystem* GetIProfileSystem() { return &m_FrameProfileSystem; }
	const char			*GetGameMOD() { if (m_szGameMOD[0]) return (m_szGameMOD);return (NULL); }
	INameTable      *GetINameTable() { return m_env.pNameTable; };
	IBudgetingSystem* GetIBudgetingSystem()  { return( m_pIBudgetingSystem ); }
	IFlowSystem     *GetIFlowSystem() { return m_env.pFlowSystem; }
	IDialogSystem   *GetIDialogSystem() { return m_env.pDialogSystem; }
	IHardwareMouse	*GetIHardwareMouse() { return m_env.pHardwareMouse; }
	IAnimationGraphSystem *GetIAnimationGraphSystem() { return m_env.pAnimationGraphSystem; }
	ISystemEventDispatcher*GetISystemEventDispatcher() { return m_pSystemEventDispatcher; }
	ITestSystem			*GetITestSystem() { return m_pTestSystem; }
	IThreadTaskManager* GetIThreadTaskManager();
	ITextModeConsole *GetITextModeConsole();
	Crc32Gen				*GetCrc32Gen();
	IFileChangeMonitor	*GetIFileChangeMonitor() { return m_pFileChangeMonitor; }
	//////////////////////////////////////////////////////////////////////////
	// retrieves the perlin noise singleton instance
	CPNoise3* GetNoiseGen();
	virtual uint64 GetUpdateCounter() { return m_nUpdateCounter; };

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
	IGPUPhysicsManager *GetIGPUPhysicsManager() { return (IGPUPhysicsManager*)m_pGPUPhysics; }
#else
	// CryMP ISystem keeps this virtual even when GPU particle physics is excluded.
	IGPUPhysicsManager *GetIGPUPhysicsManager() { return nullptr; }
#endif

	virtual void SetLoadingProgressListener(ILoadingProgressListener *pLoadingProgressListener)
	{
		m_pProgressListener = pLoadingProgressListener;
	};

	virtual ILoadingProgressListener *GetLoadingProgressListener() const
	{
		return m_pProgressListener;
	};

	void		SetIGame(IGame* pGame) {m_env.pGame = pGame;}
	void    SetIFlowSystem(IFlowSystem* pFlowSystem) { m_env.pFlowSystem = pFlowSystem; }
	void    SetIAnimationGraphSystem(IAnimationGraphSystem * pAnimationGraphSystem) { m_env.pAnimationGraphSystem = pAnimationGraphSystem; }
	void    SetIDialogSystem(IDialogSystem * pDialogSystem) { m_env.pDialogSystem = pDialogSystem; }
	void    SetIMaterialEffects(IMaterialEffects* pMaterialEffects) { m_env.pMaterialEffects = pMaterialEffects; }
	void    SetIFileChangeMonitor(IFileChangeMonitor * pFileChangeMonitor) { m_pFileChangeMonitor = pFileChangeMonitor; }
	void    ChangeUserPath( const char *sUserPath );
	void		DetectGameFolderAccessRights();

	virtual void ExecuteCommandLine();

	//////////////////////////////////////////////////////////////////////////
	virtual XmlNodeRef CreateXmlNode( const char *sNodeName="" );
	virtual XmlNodeRef LoadXmlFile( const char *sFilename );
	virtual XmlNodeRef LoadXmlFromString( const char *sXmlString );
	virtual IXmlUtils* GetXmlUtils();
	//////////////////////////////////////////////////////////////////////////

	void SetViewCamera(class CCamera &Camera){ m_ViewCamera = Camera; }
	CCamera& GetViewCamera() { return m_ViewCamera; }

  virtual int GetCPUFlags()
  {
    int Flags = 0;
    if (!m_pCpu)
      return Flags;
    if (m_pCpu->hasMMX())
      Flags |= CPUF_MMX;
    if (m_pCpu->hasSSE())
      Flags |= CPUF_SSE;
    if (m_pCpu->hasSSE2())
      Flags |= CPUF_SSE;
    if (m_pCpu->has3DNow())
      Flags |= CPUF_3DNOW;

    return Flags;
  }
  virtual double GetSecondsPerCycle()
  {
    if (!m_pCpu)
      return 0;
    else
      return m_pCpu->m_Cpu[0].m_SecondsPerCycle;
  }

	void IgnoreUpdates( bool bIgnore ) { m_bIgnoreUpdates = bIgnore; };
	void SetGCFrequency( const float fRate );

	void SetIProcess(IProcess *process);
	IProcess* GetIProcess(){ return m_pProcess; }

	bool IsTestMode() const { return m_bTestMode; }
	//@}

	void SleepIfNeeded();
	void SleepIfInactive();

	virtual void Error( const char *format,... ) PRINTF_PARAMS(2, 3);
	// Validator Warning.
	void Warning( EValidatorModule module,EValidatorSeverity severity,int flags,const char *file,const char *format,... ) PRINTF_PARAMS(6, 7);
	bool CheckLogVerbosity( int verbosity );
		
	virtual void DebugStats(bool checkpoint, bool leaks);
	void DumpWinHeaps();
	
	// tries to log the call stack . for DEBUG purposes only
	void LogCallStack();

	virtual int DumpMMStats(bool log);

	//! Return pointer to user defined callback.
	ISystemUserCallback* GetUserCallback() const { return m_pUserCallback; };

	//////////////////////////////////////////////////////////////////////////
	virtual void SaveConfiguration();
	virtual void LoadConfiguration( const char *sFilename, ILoadConfigurationEntrySink *pSink=0 );
	virtual ESystemConfigSpec GetConfigSpec( bool bClient=true );
	virtual void SetConfigSpec( ESystemConfigSpec spec,bool bClient );
	virtual ESystemConfigSpec GetMaxConfigSpec() const;
	//////////////////////////////////////////////////////////////////////////

	void ReloadTexturesNextFrame() { m_bReloadTextures=true; }

	virtual int SetThreadState(ESubsystem subsys, bool bActive)
	{
		switch (subsys)
		{
			case ESubsys_Physics: return bActive ? m_PhysThread.Resume() : m_PhysThread.Pause();
		}
		return 0;
	}
	virtual ICrySizer* CreateSizer();
	virtual bool IsPaused() const { return m_bPaused; };
	virtual IFlashPlayer* CreateFlashPlayerInstance() const;
	virtual void SetFlashLoadMovieHandler(IFlashLoadMovieHandler* pHandler) const;

	//////////////////////////////////////////////////////////////////////////
	// Creates an instance of the AVI Reader class.
	virtual IAVI_Reader *CreateAVIReader();
	// Release the AVI reader
	virtual void ReleaseAVIReader(IAVI_Reader *pAVIReader);

	virtual ILocalizationManager* GetLocalizationManager();
	virtual void debug_GetCallStack( const char **pFunctions,int &nCount );
	virtual void debug_LogCallStack( int nMaxFuncs=32,int nFlags=0 );
	virtual void ApplicationTest( const char *szParam );


public:
		// this enumeration describes the purpose for which the statistics is gathered.
	// if it's gathered to be dumped, then some different rules may be applied
	enum MemStatsPurposeEnum {nMSP_ForDisplay, nMSP_ForDump, nMSP_ForCrashLog};
	// collects the whole memory statistics into the given sizer object
	void CollectMemStats (class CrySizerImpl* pSizer, MemStatsPurposeEnum nPurpose = nMSP_ForDisplay);
	void GetExeSizes (ICrySizer* pSizer, MemStatsPurposeEnum nPurpose = nMSP_ForDisplay);
	//! refreshes the m_pMemStats if necessary; creates it if it's not created
	void TickMemStats(MemStatsPurposeEnum nPurpose = nMSP_ForDisplay, IResourceCollector *pResourceCollector=0 );

private:
	//! @name Initialization routines
	//@{ 
	bool InitNetwork();
	bool InitInput(WIN_HWND hwnd);
	bool InitConsole();
	bool InitRenderer(WIN_HINSTANCE hinst,WIN_HWND hwnd,const char *szCmdLine);
	bool InitSound(WIN_HWND hwnd);
	bool InitPhysics();

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
	bool InitGPUPhysics();
#endif

	bool InitFont();
	bool InitFlash();
	bool InitAISystem();
	bool InitScriptSystem();
	bool InitFileSystem();
	bool InitStreamEngine();
	bool Init3DEngine();
	bool InitAnimationSystem();
	bool InitMovieSystem();
	bool InitEntitySystem(WIN_HINSTANCE hInstance, WIN_HWND hWnd);
	bool OpenRenderLibrary(int type);
	int AutoDetectRenderer(char *Vendor, char *Device);
  bool OpenRenderLibrary(const char *t_rend);
  bool CloseRenderLibrary();
	//@}
	void Strange();
	bool ParseSystemConfig(string &sFileName);

	//////////////////////////////////////////////////////////////////////////
	// Helper functions.
	//////////////////////////////////////////////////////////////////////////
	void CreateRendererVars();
	void CreateSystemVars();
	void RenderStats();
	void RenderMemStats();
	void RenderFlashInfo();
	void GetFlashMemoryUsage(ICrySizer* pSizer);
	void MonitorBudget();
	WIN_HMODULE LoadDLL( const char *dllName, bool bQuitIfNotFound=true);
	void FreeLib( WIN_HMODULE hLibModule);
	void QueryVersionInfo();
	void LogVersion();
	void SetDevMode( bool bEnable );
	void InitScriptDebugger();

	void CreatePhysicsThread();
	void KillPhysicsThread();

	void ShareShaderCombinations();
	bool ReLaunchMediaCenter();
	void LogSystemInfo();
	void EnableFloatExceptions( int type );

	// recursive
	// Arguments:
	//   sPath - e.g. "Game/Config/CVarGroups"
	void AddCVarGroupDirectory( const string &sPath );

	wstring GetErrorStringUnsupportedGPU(const char* gpuName, unsigned int gpuVendorId, unsigned int gpuDeviceId);

public:

	// interface ISystem -------------------------------------------
	virtual IDataProbe* GetIDataProbe() { return m_pDataProbe; };
	virtual void SetForceNonDevMode( const bool bValue );
	virtual bool GetForceNonDevMode() const;
	virtual bool WasInDevMode() const { return m_bWasInDevMode; };
	virtual bool IsDevMode() const { return m_bInDevMode && !GetForceNonDevMode(); }
	virtual bool IsMODValid(const char *szMODName) const 
	{ 
		if (!szMODName || strstr(szMODName,".") || strstr(szMODName,"\\") || stricmp(szMODName,DATA_FOLDER)==0) 
			return (false);
		return (true);
	}
	virtual void AutoDetectSpec();

	// -------------------------------------------------------------

	//! attaches the given variable to the given container;
	//! recreates the variable if necessary
	ICVar* attachVariable (const char* szVarName, int* pContainer, const char *szComment,int dwFlags=0 );

	CCpuFeatures* GetCPUFeatures() { return m_pCpu; };

	string& GetDelayedScreeenshot() {return m_sDelayedScreeenshot;}

	const string& GetEngineUserPath() const {return m_engineFolders[ENGINE_FOLDER_USER];}

private: // ------------------------------------------------------

	// System environment.
	SSystemGlobalEnvironment m_env;

	CTimer								m_Time;								//!<
	CCamera								m_ViewCamera;					//!<
	bool									m_bQuit;							//!< if is true the system is quitting
	bool									m_bRelaunch;					//!< relaunching the app or not (true beforerelaunch)
	bool									m_bRelaunched;				//!< Application was started with the -RELAUNCH option (true after relaunch)
	int										m_iLoadingMode;				//!< Game is loading w/o changing context (0 not, 1 quickloading, 2 full loading)
	bool									m_bTestMode;					//!< If running in testing mode.
	bool									m_bEditor;						//!< If running in Editor.
	bool                  m_bPreviewMode;       //!< If running in Preview mode.
	bool									m_bDedicatedServer;		//!< If running as Dedicated server.
	bool									m_bIgnoreUpdates;			//!< When set to true will ignore Update and Render calls,
	IValidator *					m_pValidator;					//!< Pointer to validator interface.
	bool									m_bForceNonDevMode;		//!< true when running on a cheat protected server or a client that is connected to it (not used in singlplayer)
	bool									m_bWasInDevMode;			//!< Set to true if was in dev mode.
	bool									m_bInDevMode;					//!< Set to true if was in dev mode.
	bool                  m_bGameFolderWritable;//!< True when verified that current game folder have write access.
	SDefaultValidator *		m_pDefaultValidator;	//!<
	int										m_nStrangeRatio;			//!<
	string								m_sDelayedScreeenshot;//!< to delay a screenshot call for a frame
  CCpuFeatures *				m_pCpu;								//!< CPU features

	int										m_iTraceAllocations;
	//! DLLs handles.
	struct SDllHandles
	{
		WIN_HMODULE hRenderer;
		WIN_HMODULE hInput;
    WIN_HMODULE hFlash;
		WIN_HMODULE hSound;
		WIN_HMODULE hEntitySystem;
		WIN_HMODULE hNetwork;
		WIN_HMODULE hAI;
		WIN_HMODULE	hMovie;
		WIN_HMODULE	hPhysics;
		WIN_HMODULE	hFont;
		WIN_HMODULE hScript;
		WIN_HMODULE h3DEngine;
		WIN_HMODULE hAnimation;
		WIN_HMODULE hIndoor;
		WIN_HMODULE hGame;
#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
		WIN_HMODULE	hPhysicsGPU;
#endif
	};
	SDllHandles m_dll;

	//! THe streaming engine
	IStreamEngine* m_pStreamEngine;
	
	//! current active process
	IProcess *m_pProcess;

	IMemoryManager *m_pMemoryManager;

	CPhysRenderer *m_pPhysRenderer;
	CCamera m_PhysRendererCamera;
	ICVar *m_p_draw_helpers_str;
	int m_iJumpToPhysProfileEnt;

	// system to schedule tasks to balance the workload over multiple frame
	// (central system to avoid peaks if multiple tasks fall together)
	CGlobalTaskScheduler m_GlobalTaskScheduler;

	//! system event dispatcher
	ISystemEventDispatcher * m_pSystemEventDispatcher;

	//! The default font
	IFFont*	m_pIFont;

	//! System to monitor given budget.
	IBudgetingSystem* m_pIBudgetingSystem;

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
	// Function pointers from the dll
	CreateGPUPhysicsProc pfnCreateGPUPhysics;
	//	DeleteGPUPhysicsProc pfnDeleteGPUPhysics;
	//	SceneSetupGPUPhysicsProc pfnSceneSetupGPUPhysics;
	//	RenderGPUPhysicsProc pfnRenderGPUPhysics;
	//	RenderGPUPhysicsMessagesProc pfnRenderGPUPhysicsMessages;
	// GPU physics system
	IGPUPhysicsManager *m_pGPUPhysics;
#endif

	// XML Utils interface.
	class CXmlUtils *m_pXMLUtils;

	//! game path folder
	char	m_szGameMOD[256];

	//! global root folder
	string m_root;

	//! to hold the values stored in system.cfg
	//! because editor uses it's own values,
	//! and then saves them to file, overwriting the user's resolution.
	int m_iHeight;
	int m_iWidth;
	int m_iColorBits;
	
	// System console variables.
	//////////////////////////////////////////////////////////////////////////
	ICVar *m_cvAIUpdate;
	ICVar *m_rWidth;
	ICVar *m_rHeight;
	ICVar *m_rColorBits;
	ICVar *m_rDepthBits;
	ICVar *m_rStencilBits;
	ICVar *m_rFullscreen;
	ICVar *m_rDriver;
	ICVar *m_rDisplayInfo;
	ICVar *m_sysNoUpdate;
	ICVar *m_cvEntitySuppressionLevel;
	ICVar *m_pCVarQuit;
	ICVar *m_cvMemStats;
	ICVar *m_cvMemStatsThreshold;
	ICVar *m_cvMemStatsMaxDepth;
	ICVar *m_sysWarnings;										//!< might be 0, "sys_warnings" - Treat warning as errors.
	ICVar *m_cvSSInfo;											//!< might be 0, "sys_SSInfo" 0/1 - get file sourcesafe info
	ICVar *m_svDedicatedMaxRate;
	ICVar *m_sysAISystem;
	ICVar *m_sys_profile;
	ICVar *m_sys_profile_graph;
	ICVar *m_sys_profile_graphScale;
	ICVar *m_sys_profile_pagefaultsgraph;
	ICVar *m_sys_profile_filter;
	ICVar *m_sys_profile_allThreads;
	ICVar *m_sys_profile_network;
	ICVar *m_sys_profile_peak;
	ICVar *m_sys_profile_memory;
	ICVar *m_sys_profile_sampler;
	ICVar *m_sys_profile_sampler_max_samples;
#if defined(PS3)
	ICVar *m_sys_spu_profile;
	ICVar *m_sys_spu_enable;
	ICVar *m_sys_spu_debug;
	ICVar *m_sys_spu_dump_stats;
#endif
	ICVar *m_sys_spec;
	ICVar *m_sys_firstlaunch;
	ICVar *m_sys_StreamCallbackTimeBudget;
	ICVar *m_sys_SaveCVars;
	ICVar *m_sys_physics_CPU;
	ICVar *m_sys_min_step;
	ICVar *m_sys_max_step;
	ICVar *m_sys_enable_budgetmonitoring;
	ICVar *m_sys_memory_debug;
	ICVar *m_sys_preload;
	ICVar *m_sys_crashtest;
//	ICVar *m_sys_filecache;
	ICVar *m_gpu_particle_physics;
	
	string	m_sSavedRDriver;								//!< to restore the driver when quitting the dedicated server

	ICVar* m_cvPakPriority;
	ICVar* m_cvPakReadSlice;
	ICVar* m_cvPakLogMissingFiles;
	// the contents of the pak priority file
	PakVars m_PakVar;
	ICVar *m_cvEarlyMovieUpdate;						//!< 0 (default) needed for game, 1 better for having artifact free movie playback (camera pos is valid during entity update)

	//////////////////////////////////////////////////////////////////////////
	//! User define callback for system events.
	ISystemUserCallback *m_pUserCallback;

	WIN_HWND		m_hWnd;
	WIN_HINSTANCE	m_hInst;

	// this is the memory statistics that is retained in memory between frames
	// in which it's not gathered
	class CrySizerStats* m_pMemStats;
	class CrySizerImpl* m_pSizer;
	
	CFrameProfileSystem m_FrameProfileSystem;
	int m_profile_old;
	class CThreadProfiler *m_pThreadProfiler;

	//int m_nCurrentLogVerbosity;

	SFileVersion m_fileVersion;
	SFileVersion m_productVersion;
	IDataProbe *m_pDataProbe;

	class CLocalizedStringsManager *m_pLocalizationManager;

	// Name table.
	IFileChangeMonitor* m_pFileChangeMonitor = nullptr;
	CNameTable m_nameTable;

	CPhysThread m_PhysThread;

	ESystemConfigSpec m_nServerConfigSpec;
	ESystemConfigSpec m_nMaxConfigSpec;

	std::unique_ptr<CServerThrottle> m_pServerThrottle;

	// Pause mode.
	bool m_bPaused;
	bool m_bReloadTextures;

	uint64 m_nUpdateCounter;

public:
	//! Pointer to the download manager
	class CDownloadManager	*m_pDownloadManager;

#ifdef USE_FRAME_PROFILER
	void SetFrameProfiler(bool on, bool display, char *prefix) { m_FrameProfileSystem.SetProfiling(on, display, prefix, this); };
#else
	void SetFrameProfiler(bool on, bool display, char *prefix) {};
#endif

	//////////////////////////////////////////////////////////////////////////
	// VTune.
	virtual void VTuneResume();
	virtual void VTunePause();
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// File version.
	//////////////////////////////////////////////////////////////////////////
	virtual const SFileVersion& GetFileVersion();
	virtual const SFileVersion& GetProductVersion();

	bool WriteCompressedFile(const char *filename, void *data, unsigned int bitlen);
	unsigned int ReadCompressedFile(const char *filename, void *data, unsigned int maxbitlen);
	unsigned int GetCompressedFileSize(const char *filename);
	bool CompressDataBlock(const void * input, size_t inputSize, void * output, size_t& outputSize, int level);
	bool DecompressDataBlock( const void * input, size_t inputSize, void * output, size_t& outputSize );
	void InitVTuneProfiler();

	void OpenBasicPaks();
	void OpenLanguagePak( const char *sLanguage );

	void	Deltree(const char *szFolder, bool bRecurse);
	void UpdateMovieSystem( const int updateFlags, const float fFrameTime );

public:
	void InitLocalization();

protected: // -------------------------------------------------------------

	ILoadingProgressListener *		m_pProgressListener;
  CCmdLine *										m_pCmdLine;
	ITestSystem	*									m_pTestSystem;				// needed for external test application (0 if not activated yet)
	CThreadTaskManager*           m_pThreadTaskManager;
	
	//////////////////////////////////////////////////////////////////////////
	// Special paths.
	//////////////////////////////////////////////////////////////////////////
	string                        m_engineFolders[ENGINE_FOLDER_LAST];
	//////////////////////////////////////////////////////////////////////////

	CMemoryFragmentationProfiler	m_MemoryFragmentationProfiler;

	friend struct SDefaultValidator;
	friend struct SCryEngineFoldersLoader;
//	friend void ScreenshotCmd( IConsoleCmdArgs *pParams );
};

#endif // SYSTEM_H
