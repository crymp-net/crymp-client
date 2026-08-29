#include "StdAfx.h"
#include "CryCommon/CrySystem/ISystem.h"												// ISystem
#include "CryCommon/CryEntitySystem/IEntity.h"												// EntityId
#include "CryCommon/CrySystem/ITimer.h"													// ITimer
#include "CryCommon/CryEntitySystem/IEntitySystem.h"									// IEntityIt
#include "CryCommon/CrySystem/IConsole.h"												// IConsole
#include "CryCommon/Cry3DEngine/I3DEngine.h"											// I3DEngine
#include "CryCommon/CryGame/IGame.h"													// IGame
#include "CryCommon/CryAction/IGameFramework.h"									// IGameFramework
#include "TestSystem.h"
#include "CryCommon/CryAction/IGameRulesSystem.h"	// IGameRulesSystem
#include "DebugCallStack.h"									// DebugCallStack
#include "CryCommon/CryAction/ILevelSystem.h"			// ILevelSystemListener

extern int CryMemoryGetAllocatedSize();

CTestSystem::CTestSystem() : m_iRenderPause(0), m_fQuitInNSeconds(0.0f)
{
	m_bFirstUpdate = true;
	m_bApplicationTest = false;
	m_pTimeDemoInfo = 0;
	m_pLog = 0;
}

CTestSystem::~CTestSystem()
{
	if (m_pTimeDemoInfo)
	{
		delete []m_pTimeDemoInfo->pFrames;
		delete m_pTimeDemoInfo;
	}
}

void CTestSystem::QuitInNSeconds( const float fInNSeconds )
{
	if(fInNSeconds>0)
		gEnv->pLog->Log("QuitInNSeconds() requests quit in %f sec",fInNSeconds);

	m_fQuitInNSeconds=fInNSeconds;
}



class CLevelListener :public ILevelSystemListener
{
public:
	CLevelListener( CTestSystem &rTestSystem ) :m_rTestSystem(rTestSystem)
	{
		m_LevelStartTime = gEnv->pTimer->GetAsyncTime();
	}

	// interface ILevelSystemListener -------------------------------

	virtual void OnLevelNotFound(const char *levelName) 
	{ 
		m_rTestSystem.GetILog()->LogError("CLevelListener '%s' level not found",levelName);
		Quit(); 
	}
	virtual void OnLoadingStart(ILevelInfo *pLevel) {}
	virtual void OnLoadingComplete(ILevel *pLevel)
	{
		m_rTestSystem.LogLevelStats();
		Quit();
	}

	virtual void OnLoadingError(ILevelInfo *pLevel, const char *error)
	{ 
		m_rTestSystem.GetILog()->LogError("TestSystem '%s' loading error:'%s'",pLevel->GetName(),error);
		Quit(); 
	}
	virtual void OnLoadingProgress(ILevelInfo *pLevel, int progressAmount) {}

	// ------------------------------------------------------------------

	void Quit()
	{
		CTimeValue time = gEnv->pTimer->GetAsyncTime() - m_LevelStartTime;

		m_rTestSystem.GetILog()->Log("   Time since level start: %d min %d sec", ((uint32)time.GetSeconds())/60, ((uint32)time.GetSeconds())%60 );

		gEnv->pConsole->ExecuteString("quit");
	}

	// ------------------------------------------------------------------

	CTestSystem &						m_rTestSystem;							//
	CTimeValue							m_LevelStartTime;						// absolute time at level start
};


void CTestSystem::ApplicationTest( const char *szParam )
{
	m_sParameter=szParam;
	m_iRenderPause=0;

	m_pLog = new CLog( GetISystem() );
	m_pLog->SetFileName( "TestResults/TestLog.log" );
	if(!szParam)
	{
		gEnv->pLog->LogError("ApplicationTest() parameter missing");
		return;
	}

	IGame *pGame = gEnv->pGame;

	if(stricmp(m_sParameter.c_str(),"LevelStats")==0)
	if(pGame)
	{
		static CLevelListener listener(*this);

		if(m_bFirstUpdate)
		{
			DeactivateCrashDialog();
			pGame->GetIGameFramework()->GetILevelSystem()->AddListener(&listener);
		}
	}

	gEnv->pConsole->ShowConsole(false);


	m_bApplicationTest = true;

//	GetILog()->Log("ApplicationTest '%s'",szParam);
}

void CTestSystem::LogLevelStats()
{
	// get version
	const SFileVersion & ver = GetISystem()->GetFileVersion();
	char sVersion[128];
	ver.ToString(sVersion);

	GetILog()->Log("   LevelStats Level='%s'   Ver=%s",gEnv->pGame->GetIGameFramework()->GetLevelName(),sVersion);

	{
		// copied from CBudgetingSystem::MonitorSystemMemory
		int memUsageInMB_Engine( CryMemoryGetAllocatedSize() );
		int memUsageInMB_SysCopyMeshes( *( (int*) gEnv->pRenderer->EF_Query( EFQ_Alloc_APIMesh ) ) );
		int memUsageInMB_SysCopyTextures( *( (int*) gEnv->pRenderer->EF_Query( EFQ_Alloc_APITextures ) ) );

		int memUsageInMB( RoundToClosestMB( (size_t) memUsageInMB_Engine + 
			(size_t) memUsageInMB_SysCopyMeshes + (size_t) memUsageInMB_SysCopyTextures ) );

		memUsageInMB_Engine = RoundToClosestMB( memUsageInMB_Engine );
		memUsageInMB_SysCopyMeshes = RoundToClosestMB( memUsageInMB_SysCopyMeshes );
		memUsageInMB_SysCopyTextures = RoundToClosestMB( memUsageInMB_SysCopyTextures );

		GetILog()->Log( "   System memory: %d MB [%d MB (engine), %d MB (managed textures), %d MB (managed meshes)]",
			memUsageInMB,memUsageInMB_Engine,memUsageInMB_SysCopyTextures,memUsageInMB_SysCopyMeshes);
	}

	gEnv->pConsole->ExecuteString("SaveLevelStats");
}



void CTestSystem::Update()
{
	if(m_fQuitInNSeconds>0.0f)
	{
		int iSec = (int)m_fQuitInNSeconds;

		m_fQuitInNSeconds -= gEnv->pTimer->GetFrameTime();

		if(m_fQuitInNSeconds<=0.0f)
			gEnv->pSystem->Quit();
		else
		{
			if(iSec!=(int)m_fQuitInNSeconds)
				gEnv->pLog->Log("quit in %d seconds ...",iSec);
		}
	}

	if (!m_bApplicationTest)
		return;

	if(m_sParameter.empty())
		return;

	IGame *pGame = gEnv->pGame;

	if (m_bFirstUpdate)
	{
		if(stricmp(m_sParameter.c_str(),"TimeDemo")==0)
		{
			gEnv->pConsole->ExecuteString( "exec TimeDemo.exc" );
		}
	}
	

	m_bFirstUpdate = false;
}

void CTestSystem::DeactivateCrashDialog()
{
#ifndef _DEBUG
#ifdef WIN32
		DebugCallStack::instance()->SetUserDialogEnable(false);
#endif
#endif
}


void CTestSystem::BeforeRender()
{
	const int iPauseTime=20;

	if(m_sParameter.empty())
		return;

	IGame *pGame = gEnv->pGame;	
	I3DEngine * p3DEngine = gEnv->p3DEngine;
	IRenderer * pRenderer = gEnv->pRenderer;

	if(stricmp(m_sParameter.c_str(), "PrecacheCameraScreenshots")==0)	// -----------------------------------------------------
	{
		assert(p3DEngine);
		assert(pRenderer);

		IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
		IEntityClass *pPrecacheCameraClass = pEntitySystem->GetClassRegistry()->FindClass( "PrecacheCamera" );
		static IEntityIt *pEntityIter=0;

		if(m_iRenderPause!=0)
			--m_iRenderPause;
		else
		{
			if(!pEntityIter)
			{
				pEntityIter = pEntitySystem->GetEntityIterator();
			}
			else pEntityIter->Next();

			m_iRenderPause=iPauseTime;																// wait 20 frames
		}

		IEntity *pEntity = pEntityIter->This();

		// find next of the right type
		while(pEntity && pEntity->GetClass()!=pPrecacheCameraClass)
		{
			pEntityIter->Next();pEntity = pEntityIter->This();
		}

		if(!pEntity)
		{
			m_sParameter="";																					// stop processing
			pEntityIter->Release();pEntityIter=0;m_iRenderPause=0;
			return;
		}

		const char *szDirectory="TestSystemOutput";

		if(m_iRenderPause==iPauseTime-1)
			ScreenShot(szDirectory,(string(pEntity->GetName())+"_immediate.bmp").c_str());

		if(m_iRenderPause==0)
			ScreenShot(szDirectory,(string(pEntity->GetName())+"_later.bmp").c_str());

		// setup camera 
		CCamera &rCam = GetISystem()->GetViewCamera();
		Matrix34 mat = pEntity->GetWorldTM();
		rCam.SetMatrix(mat);
		rCam.SetFrustum(pRenderer->GetWidth(), pRenderer->GetHeight(), gf_PI/2, rCam.GetNearPlane(), rCam.GetFarPlane());
	}
}

//////////////////////////////////////////////////////////////////////////
void CTestSystem::ScreenShot( const char *szDirectory, const char *szFilename )
{
	assert(szDirectory);
	assert(szFilename);

	CryCreateDirectory(szDirectory,0);	// ensure the directory is existing - might not be needed

	// directory is generated automatically
	gEnv->pRenderer->ScreenShot(string(szDirectory)+"/"+szFilename);

	GetILog()->Log("Generated screen shot '%s/%s'",szDirectory,szFilename);
}

//////////////////////////////////////////////////////////////////////////
void CTestSystem::SetTimeDemoInfo( STimeDemoInfo *pTimeDemoInfo )
{
	if (m_pTimeDemoInfo)
		delete []m_pTimeDemoInfo->pFrames;
	else
		m_pTimeDemoInfo = new STimeDemoInfo;

	*m_pTimeDemoInfo = *pTimeDemoInfo;

	m_pTimeDemoInfo->pFrames = new STimeDemoFrameInfo[pTimeDemoInfo->nFrameCount];
	// Copy per frame data.
	memcpy( m_pTimeDemoInfo->pFrames,pTimeDemoInfo->pFrames,sizeof(STimeDemoFrameInfo)*pTimeDemoInfo->nFrameCount );
}

//////////////////////////////////////////////////////////////////////////
STimeDemoInfo* CTestSystem::GetTimeDemoInfo()
{
	return m_pTimeDemoInfo;
}
