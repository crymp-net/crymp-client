#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "CryCommon/Cry3DEngine/I3DEngine.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryCommon/CryInput/IInput.h"
#include "CryCommon/CryMovie/IMovieSystem.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryCommon/CrySoundSystem/IMusicSystem.h"
#include "CryCommon/CrySoundSystem/ISound.h"
#include "CryCommon/CrySystem/gEnv.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/ITextModeConsole.h"
#include "CryCommon/CrySystem/ITimer.h"
#include "CrySystem/CryLog.h"

#include "ActionMapManager.h"
#include "ActorSystem.h"
#include "AnimationGraphSystem.h"
#include "CallbackTimer.h"
#include "DebrisMgr.h"
#include "DebugHistoryManager.h"
#include "DevMode.h"
#include "DialogSystem.h"
#include "EffectSystem.h"
#include "FlowSystem.h"
#include "GameClientChannel.h"
#include "GameClientNub.h"
#include "GameContext.h"
#include "GameFramework.h"
#include "GameFrameworkCVars.h"
#include "GameObject.h"
#include "GameObjectSystem.h"
#include "GameplayAnalyst.h"
#include "GameplayRecorder.h"
#include "GameQueryListener.h"
#include "GameRulesSystem.h"
#include "GameSerialize.h"
#include "GameServerChannel.h"
#include "GameServerNub.h"
#include "GameStatsConfig.h"
#include "GameTokenSystem.h"
#include "ItemSystem.h"
#include "LevelSystem.h"
#include "MaterialEffects.h"
#include "MusicLogic.h"
#include "NetworkCVars.h"
#include "PersistantDebug.h"
#include "PlayerProfileManager.h"
#include "ScriptBind_ActionMapManager.h"
#include "ScriptBind_Actor.h"
#include "ScriptBind_AI.h"
#include "ScriptBind_CryAction.h"
#include "ScriptBind_DialogSystem.h"
#include "ScriptBind_Inventory.h"
#include "ScriptBind_ItemSystem.h"
#include "ScriptBind_MaterialEffects.h"
#include "ScriptBind_Network.h"
#include "ScriptBind_Vehicle.h"
#include "ScriptBind_VehicleSeat.h"
#include "ScriptBind_VehicleSystem.h"
#include "ScriptRMI.h"
#include "SubtitleManager.h"
#include "TimeDemoRecorder.h"
#include "TimeOfDayScheduler.h"
#include "UIDraw.h"
#include "Vehicles/VehicleSystem.h"
#include "ViewSystem.h"

extern std::uintptr_t CRYACTION_BASE;

static void RegisterInventoryFactory(IGameFramework* pGameFramework)
{
#ifdef BUILD_64BIT
	auto* pInventoryCreator = reinterpret_cast<IGameFramework::IGameObjectExtensionCreator*>(CRYACTION_BASE + 0x4651a8);
	auto* vtable = reinterpret_cast<void*>(CRYACTION_BASE + 0x38c968);
#else
	auto* pInventoryCreator = reinterpret_cast<IGameFramework::IGameObjectExtensionCreator*>(CRYACTION_BASE + 0x2db8dc);
	auto* vtable = reinterpret_cast<void*>(CRYACTION_BASE + 0x298bd4);
#endif

	// construct
	*reinterpret_cast<void**>(pInventoryCreator) = vtable;

	pGameFramework->RegisterFactory("Inventory", pInventoryCreator, false);
}

static void RegisterSomeExtensions(IGameFramework* pGameFramework)
{
#ifdef BUILD_64BIT
	std::uintptr_t func = CRYACTION_BASE + 0x2bb6d0;
#else
	std::uintptr_t func = CRYACTION_BASE + 0x1dbcb0;
#endif

	reinterpret_cast<void(*)(IGameFramework*)>(func)(pGameFramework);
}

GameFramework::GameFramework()
{
	// set global instance pointer inside CryAction DLL
#ifdef BUILD_64BIT
	*reinterpret_cast<GameFramework**>(CRYACTION_BASE + 0x464b40) = this;
#else
	*reinterpret_cast<GameFramework**>(CRYACTION_BASE + 0x2db0ac) = this;
#endif
}

GameFramework::~GameFramework()
{
	// TODO
}

GameFramework* GameFramework::GetInstance()
{
	static GameFramework* pInstance = new GameFramework();
	return pInstance;
}

////////////////////////////////////////////////////////////////////////////////
// IGameFramework
////////////////////////////////////////////////////////////////////////////////

void GameFramework::RegisterFactory(const char* name, IAnimationStateNodeFactory* (*pCreator)(), bool isAI)
{
#ifdef BUILD_64BIT
	std::uintptr_t func = CRYACTION_BASE + 0x31D250;
#else
	std::uintptr_t func = CRYACTION_BASE + 0x21F760;
#endif

	(this->*reinterpret_cast<void(GameFramework::*&)(const char*, IAnimationStateNodeFactory* (*)(), bool)>(func))
		(name, pCreator, isAI);
}

void GameFramework::RegisterFactory(const char* name, ISaveGame* (*pCreator)(), bool isAI)
{
	m_pGameSerialize->RegisterSaveGameFactory(name, pCreator);
}

void GameFramework::RegisterFactory(const char* name, ILoadGame* (*pCreator)(), bool isAI)
{
	m_pGameSerialize->RegisterLoadGameFactory(name, pCreator);
}

void GameFramework::RegisterFactory(const char* name, IActorCreator* pCreator, bool isAI)
{
	m_pActorSystem->RegisterActorClass(name, pCreator, isAI);
}

void GameFramework::RegisterFactory(const char* name, IItemCreator* pCreator, bool isAI)
{
	m_pItemSystem->RegisterItemFactory(name, pCreator);
}

void GameFramework::RegisterFactory(const char* name, IVehicleCreator* pCreator, bool isAI)
{
	m_pVehicleSystem->RegisterVehicleFactory(name, pCreator);
}

void GameFramework::RegisterFactory(const char* name, IGameObjectExtensionCreator* pCreator, bool isAI)
{
	m_pGameObjectSystem->RegisterExtension(name, pCreator, nullptr);
}

bool GameFramework::Init(SSystemInitParams& startupParams)
{
	CryLogAlways("$3[CryMP] Initializing Game Framework");

	m_pSystem = gEnv->pSystem;

	// set gEnv inside CryAction DLL
#ifdef BUILD_64BIT
	*reinterpret_cast<SSystemGlobalEnvironment**>(CRYACTION_BASE + 0x464ae0) = m_pSystem->GetGlobalEnvironment();
#else
	*reinterpret_cast<SSystemGlobalEnvironment**>(CRYACTION_BASE + 0x2db06c) = m_pSystem->GetGlobalEnvironment();
#endif

	// register CryAction's global system event listener
#ifdef BUILD_64BIT
	m_pSystem->GetISystemEventDispatcher()->RegisterListener(reinterpret_cast<ISystemEventListener*>(CRYACTION_BASE + 0x420bb0));
#else
	m_pSystem->GetISystemEventDispatcher()->RegisterListener(reinterpret_cast<ISystemEventListener*>(CRYACTION_BASE + 0x2a15e4));
#endif

	m_pNetwork = m_pSystem->GetINetwork();
	m_p3DEngine = m_pSystem->GetI3DEngine();
	m_pScriptSystem = m_pSystem->GetIScriptSystem();
	m_pEntitySystem = m_pSystem->GetIEntitySystem();
	m_pTimer = m_pSystem->GetITimer();
	m_pLog = m_pSystem->GetILog();

	// original CryAction obtains Crysis CD key from Windows registry at this point
	// CryMP client sets a random CD key during its initialization
	// no need to obtain CD key here

	this->InitCVars();
	this->InitCommands();

	if (m_pSystem->IsDevMode())
	{
		m_pDevMode = new DevMode();
	}

	m_pTimeDemoRecorder = new TimeDemoRecorder(m_pSystem);

	ScriptRMI::RegisterCVars();
	GameObject::RegisterCVars();

	m_pScriptRMI = new ScriptRMI();
	m_pGameTokenSystem = new GameTokenSystem();
	m_pEffectSystem = new EffectSystem();
	m_pEffectSystem->Init();
	m_pUIDraw = new UIDraw();
	m_pLevelSystem = new LevelSystem(m_pSystem, "levels");
	m_pActorSystem = new ActorSystem(m_pSystem, m_pEntitySystem);
	m_pItemSystem = new ItemSystem(this);
	m_pActionMapManager = new ActionMapManager(m_pSystem->GetIInput());
	m_pViewSystem = new ViewSystem(m_pSystem);
	m_pGameplayRecorder = new GameplayRecorder(this);
	m_pGameplayAnalyst = new GameplayAnalyst();
	m_pGameRulesSystem = new GameRulesSystem(m_pSystem, this);
	m_pVehicleSystem = new VehicleSystem(this);
	m_pNetworkCVars = new NetworkCVars();
	m_pGameFrameworkCVars = new GameFrameworkCVars();

	m_pGameObjectSystem = new GameObjectSystem();
	m_pGameObjectSystem->Init();
	m_pGameObjectSystem->RegisterEvent(eGFE_PauseGame, "PauseGame");
	m_pGameObjectSystem->RegisterEvent(eGFE_ResumeGame, "ResumeGame");
	m_pGameObjectSystem->RegisterEvent(eGFE_OnCollision, "OnCollision");
	m_pGameObjectSystem->RegisterEvent(eGFE_OnPostStep, "OnPostStep");
	m_pGameObjectSystem->RegisterEvent(eGFE_OnStateChange, "OnStateChange");
	m_pGameObjectSystem->RegisterEvent(eGFE_ResetAnimationGraphs, "ResetAnimationGraphs");
	m_pGameObjectSystem->RegisterEvent(eGFE_OnBreakable2d, "OnBreakable2d");
	m_pGameObjectSystem->RegisterEvent(eGFE_OnBecomeVisible, "OnBecomeVisible");
	m_pGameObjectSystem->RegisterEvent(eGFE_PreFreeze, "PreFreeze");
	m_pGameObjectSystem->RegisterEvent(eGFE_PreShatter, "PreShatter");
	m_pGameObjectSystem->RegisterEvent(eGFE_BecomeLocalPlayer, "BecomeLocalPlayer");
	m_pGameObjectSystem->RegisterEvent(eGFE_DisablePhysics, "DisablePhysics");
	m_pGameObjectSystem->RegisterEvent(eGFE_EnablePhysics, "EnablePhysics");

	m_pAnimationGraphSystem = new AnimationGraphSystem();
	m_pGameSerialize = new GameSerialize();
	m_pCallbackTimer = new CallbackTimer();
	m_pPersistantDebug = new PersistantDebug();
	m_pPlayerProfileManager = new PlayerProfileManager();
	m_pDialogSystem = new DialogSystem();
	m_pDialogSystem->Init();
	m_pDebrisMgr = new DebrisMgr();
	m_pTimeOfDayScheduler = new TimeOfDayScheduler();
	m_pSubtitleManager = new SubtitleManager();

	IMovieSystem* pMovieSystem = m_pSystem->GetIMovieSystem();
	if (pMovieSystem)
	{
		pMovieSystem->SetUser(m_pViewSystem);
	}

	m_pVehicleSystem->Init();

	RegisterInventoryFactory(this);

	m_pLevelSystem->AddListener(m_pItemSystem);

	this->InitScriptBinds();

	m_pSomeStrings = new SomeStrings();

	m_pVehicleSystem->RegisterVehicles(this);
	m_pGameObjectSystem->RegisterFactories(this);
	m_pGameSerialize->RegisterFactories(this);

	RegisterSomeExtensions(this);

	m_pPlayerProfileManager->Initialize();

	// skip instantiating CDownloadTask for downloading maps and stuff (m_reserved_0x51c_0x5b8)
	// it is more harmful than useful and all accesses seem to be guarded by null checks making it optional

	m_pListeners = new StlportVector_CryAction<Listener>();
	m_pListenersCopy = new StlportVector_CryAction<Listener>();

	m_pNextFrameCommand = new CryStringT<char>();

	m_pGameStatsConfig = new GameStatsConfig();
	m_pGameStatsConfig->Init();

	return true;
}

bool GameFramework::CompleteInit()
{
	m_pAnimationGraphSystem->RegisterFactories(this);

	if (IAnimationGraphPtr pMusicGraph = m_pAnimationGraphSystem->LoadGraph("MusicLogic.xml", false, true))
	{
		m_pMusicGraphState = pMusicGraph->CreateState();
	}

	if (!m_pMusicGraphState)
	{
		CryLogWarningAlways("%s: Unable to load music logic graph", __FUNCTION__);
	}

	m_pMusicLogic = new MusicLogic(m_pMusicGraphState);
	m_pMusicLogic->Init();

	this->EndGameContext();

	m_pFlowSystem = new FlowSystem();

	m_pSystem->SetIFlowSystem(m_pFlowSystem);
	m_pSystem->SetIAnimationGraphSystem(m_pAnimationGraphSystem);
	m_pSystem->SetIDialogSystem(m_pDialogSystem);

	m_pGameplayRecorder->RegisterListener(m_pGameplayAnalyst);

	m_pMaterialEffects = new MaterialEffects();
	m_pScriptBind_MaterialEffects = new ScriptBind_MaterialEffects(m_pSystem, m_pMaterialEffects);
	m_pMaterialEffects->Load("MaterialEffects.xml");

	m_pSystem->SetIMaterialEffects(m_pMaterialEffects);

	if (!m_pSystem->IsEditor())
	{
		IEquipmentManager* pEquipmentManager = m_pItemSystem->GetIEquipmentManager();

		pEquipmentManager->DeleteAllEquipmentPacks();
		pEquipmentManager->LoadEquipmentPacksFromPath("Libs/EquipmentPacks");
	}

	IGame* pGame = gEnv->pGame;
	if (pGame)
	{
		pGame->CompleteInit();
	}

	m_pMaterialEffects->LoadFlowGraphLibs();

	m_pScriptSystem->ExecuteFile("scripts/main.lua");
	m_pScriptSystem->BeginCall("OnInit");
	m_pScriptSystem->EndCall();

	return true;
}

bool GameFramework::PreUpdate(bool haveFocus, unsigned int updateFlags)
{
	if (!m_pNextFrameCommand->empty())
	{
		gEnv->pConsole->ExecuteString(m_pNextFrameCommand->c_str());
		m_pNextFrameCommand->clear();
	}

	this->CheckEndLevelSchedule();

	ITextModeConsole* pTextModeConsole = m_pSystem->GetITextModeConsole();
	if (pTextModeConsole)
	{
		pTextModeConsole->BeginDraw();
	}

	bool isGameStarted = this->IsGameStarted();
	bool isGamePaused = !isGameStarted || this->IsGamePaused();

	if (!this->IsGamePaused())
	{
		m_pTimeDemoRecorder->PreUpdate();
	}

	if (!isGamePaused)
	{
		m_pCallbackTimer->UpdateTimer();
	}

	if (!(updateFlags & ESYSUPDATE_EDITOR))
	{
		gEnv->pFrameProfileSystem->StartFrame();
	}

	m_pSystem->RenderBegin();

	const float frameTime = gEnv->pTimer->GetFrameTime();
	bool status = true;

	if (!(updateFlags & ESYSUPDATE_EDITOR))
	{
		int pauseMode = (!isGameStarted || m_isPaused) ? 1 : 0;

		if (gEnv->bMultiplayer && !gEnv->bServer)
		{
			pauseMode = 0;
		}

		status = m_pSystem->Update(0, pauseMode);

		this->OnActionEvent(SActionEvent(eAE_earlyPreUpdate));

		isGameStarted = this->IsGameStarted();
		const bool wasGamePaused = isGamePaused;
		isGamePaused = !isGameStarted || this->IsGamePaused();

		if (!isGamePaused)
		{
			if (!wasGamePaused)
			{
				m_pViewSystem->Update(std::min(frameTime, 0.1f));
				m_pGameplayRecorder->Update(frameTime);
			}

			m_pFlowSystem->Update();
		}
	}

	m_pActionMapManager->Update();

	if (!isGamePaused)
	{
		m_pItemSystem->Update(frameTime);
		m_pMaterialEffects->Update(frameTime);
		m_pDialogSystem->Update(frameTime);
		m_pMusicLogic->Update();

		if (m_pMusicGraphState)
		{
			m_pMusicGraphState->Update();
		}

		m_pDebrisMgr->Update();
	}

	// skip updating CDownloadTask
	// skip updating RCon server
	// skip updating HTTP server

	return status;
}

void GameFramework::PostUpdate(bool haveFocus, unsigned int updateFlags)
{
	if (m_pLanBrowserCVar->GetIVal())
	{
		if (!m_isLanBrowserRunning)
		{
			m_isLanBrowserRunning = true;
			this->BeginLanQuery();
		}
	}
	else
	{
		if (m_isLanBrowserRunning)
		{
			m_isLanBrowserRunning = false;
			this->EndCurrentQuery();
		}
	}

	const float frameTime = gEnv->pTimer->GetFrameTime();

	if (!this->IsGamePaused())
	{
		m_pEffectSystem->Update(frameTime);

		if (m_lastSaveLoad > 0)
		{
			m_lastSaveLoad -= frameTime;

			if (m_lastSaveLoad < 0)
			{
				m_lastSaveLoad = 0;
			}
		}
	}

	m_pSystem->Render();

	this->CallListeners([frameTime](IGameFrameworkListener* pListener) { pListener->OnPostUpdate(frameTime); });

	m_pPersistantDebug->PostUpdate(frameTime);
	m_pGameObjectSystem->PostUpdate(frameTime);

	m_pSystem->RenderEnd();

	if (m_pActionGame)
	{
		if (m_pActionGame->Update())
		{
			m_pActionGame = nullptr;
		}
	}

	GameServerNub* pGameServerNub = this->GetGameServerNub();
	if (pGameServerNub)
	{
		pGameServerNub->Update();
	}

	if (!this->IsGamePaused())
	{
		m_pTimeDemoRecorder->Update();
	}

	if (!(updateFlags & ESYSUPDATE_EDITOR))
	{
		gEnv->pFrameProfileSystem->EndFrame();
	}

	if (m_delayedSaveGameMethod)
	{
		const char* const path = m_pSomeStrings->delayedSaveGameName.c_str();
		const char* const checkPoint = m_pSomeStrings->checkPointName.c_str();
		const ESaveGameReason reason = m_delayedSaveGameReason;
		const bool quick = m_delayedSaveGameMethod == 1;
		const bool forceImmediate = true;
		const bool ignoreDelay = false;

		this->SaveGame(path, quick, forceImmediate, reason, ignoreDelay, checkPoint);

		m_delayedSaveGameMethod = 0;
		m_pSomeStrings->delayedSaveGameName.clear();
	}

	ITextModeConsole* pTextModeConsole = m_pSystem->GetITextModeConsole();
	if (pTextModeConsole)
	{
		pTextModeConsole->EndDraw();
	}

	GameObject::UpdateSchedulingProfiles();
}

void GameFramework::Shutdown()
{
	CryLogErrorAlways("%s: Not implemented!", __FUNCTION__);
}

void GameFramework::Reset(bool clients)
{
	GameContext* pGameContext = m_pActionGame ? m_pActionGame->GetGameContext() : nullptr;
	if (pGameContext && pGameContext->GetFlags() & eGSF_Server)
	{
		pGameContext->ResetMap(true);

		if (gEnv->bServer)
		{
			GameServerNub* pGameServerNub = this->GetGameServerNub();
			if (pGameServerNub)
			{
				pGameServerNub->ResetOnHoldChannels();
			}
		}
	}

	if (m_pGameplayRecorder)
	{
		m_pGameplayRecorder->Event(nullptr, GameplayEvent(eGE_GameReset));
	}

	if (m_pMusicLogic)
	{
		m_pMusicLogic->Stop();
	}
}

void GameFramework::PauseGame(bool pause, bool force)
{
	if (!force && m_isPaused && m_isForcedPaused)
	{
		return;
	}

	if (m_isPaused == pause && m_isForcedPaused == force)
	{
		return;
	}

	gEnv->pTimer->PauseTimer(ITimer::ETIMER_GAME, pause);

	m_pActionMapManager->Enable(!pause);

	ISoundSystem* pSoundSystem = gEnv->pSoundSystem;
	if (pSoundSystem)
	{
		pSoundSystem->Pause(pause, false);
	}

	if (pause)
	{
		IInput* pInput = gEnv->pInput;
		if (pInput)
		{
			pInput->ForceFeedbackEvent(SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.0f, 0.0f, 0.0f));
		}
	}

	gEnv->p3DEngine->GetTimeOfDay()->SetPaused(pause);

	gEnv->pEntitySystem->PauseTimers(pause, false);

	m_isPaused = pause;
	m_isForcedPaused = force;

	IMovieSystem* pMovieSystem = gEnv->pMovieSystem;
	if (pMovieSystem)
	{
		if (pause)
		{
			pMovieSystem->Pause();
		}
		else
		{
			pMovieSystem->Resume();
		}
	}

	m_pGameObjectSystem->BroadcastEvent(SGameObjectEvent(pause ? eGFE_PauseGame : eGFE_ResumeGame, eGOEF_ToAll));
}

bool GameFramework::IsGamePaused()
{
	return m_isPaused || !gEnv->pTimer->IsTimerEnabled();
}

bool GameFramework::IsGameStarted()
{
	if (!m_pActionGame)
	{
		return false;
	}

	GameContext* pGameContext = m_pActionGame->GetGameContext();
	if (!pGameContext)
	{
		return false;
	}

	return pGameContext->IsGameStarted();
}

ISystem* GameFramework::GetISystem()
{
	return m_pSystem;
}

ILanQueryListener* GameFramework::GetILanQueryListener()
{
	return m_pLanQueryListener;
}

IUIDraw* GameFramework::GetIUIDraw()
{
	return m_pUIDraw;
}

IGameObjectSystem* GameFramework::GetIGameObjectSystem()
{
	return m_pGameObjectSystem;
}

ILevelSystem* GameFramework::GetILevelSystem()
{
	return m_pLevelSystem;
}

IActorSystem* GameFramework::GetIActorSystem()
{
	return m_pActorSystem;
}

IItemSystem* GameFramework::GetIItemSystem()
{
	return m_pItemSystem;
}

IActionMapManager* GameFramework::GetIActionMapManager()
{
	return m_pActionMapManager;
}

IViewSystem* GameFramework::GetIViewSystem()
{
	return m_pViewSystem;
}

IGameplayRecorder* GameFramework::GetIGameplayRecorder()
{
	return m_pGameplayRecorder;
}

IVehicleSystem* GameFramework::GetIVehicleSystem()
{
	return m_pVehicleSystem;
}

IGameRulesSystem* GameFramework::GetIGameRulesSystem()
{
	return m_pGameRulesSystem;
}

IFlowSystem* GameFramework::GetIFlowSystem()
{
	return m_pFlowSystem;
}

IGameTokenSystem* GameFramework::GetIGameTokenSystem()
{
	return m_pGameTokenSystem;
}

IEffectSystem* GameFramework::GetIEffectSystem()
{
	return m_pEffectSystem;
}

IMaterialEffects* GameFramework::GetIMaterialEffects()
{
	return m_pMaterialEffects;
}

IDialogSystem* GameFramework::GetIDialogSystem()
{
	return m_pDialogSystem;
}

IPlayerProfileManager* GameFramework::GetIPlayerProfileManager()
{
	return m_pPlayerProfileManager;
}

IDebrisMgr* GameFramework::GetDebrisMgr()
{
	return m_pDebrisMgr;
}

ISubtitleManager* GameFramework::GetISubtitleManager()
{
	return m_pSubtitleManager;
}

bool GameFramework::StartGameContext(const SGameStartParams* pGameStartParams)
{
	if (gEnv->bEditor)
	{
		if (!m_pActionGame)
		{
			m_pSystem->Error("Must have game around always for editor");
		}
	}
	else
	{
		this->EndGameContext();
		m_pActionGame = new ActionGame(m_pScriptRMI);
	}

	if (!m_pActionGame->Init(pGameStartParams))
	{
		this->EndGameContext();
		CryLogWarningAlways("%s: Failed initializing game", __FUNCTION__);
		return false;
	}

	return true;
}

bool GameFramework::ChangeGameContext(const SGameContextParams* pGameContextParams)
{
	return m_pActionGame && m_pActionGame->ChangeGameContext(pGameContextParams);
}

void GameFramework::EndGameContext()
{
	_smart_ptr<ActionGame> pActionGame = m_pActionGame;
	m_pActionGame = 0;
	pActionGame = 0;

	if (gEnv->bEditor)
	{
		m_pActionGame = new ActionGame(m_pScriptRMI);
	}

	if (m_pMusicLogic)
	{
		m_pMusicLogic->Stop();
	}
}

bool GameFramework::StartedGameContext() const
{
	return static_cast<bool>(m_pActionGame);
}

bool GameFramework::BlockingSpawnPlayer()
{
	return m_pActionGame && m_pActionGame->BlockingSpawnPlayer();
}

void GameFramework::ResetBrokenGameObjects()
{
	if (m_pActionGame)
	{
		m_pActionGame->FixBrokenObjects();
	}
}

void GameFramework::SetEditorLevel(const char* levelName, const char* levelFolder)
{
	std::strncpy(m_editorLevelName, levelName, sizeof(m_editorLevelName) - 1);
	m_editorLevelName[sizeof(m_editorLevelName) - 1] = '\0';

	std::strncpy(m_editorLevelFolder, levelFolder, sizeof(m_editorLevelFolder) - 1);
	m_editorLevelFolder[sizeof(m_editorLevelFolder) - 1] = '\0';
}

void GameFramework::GetEditorLevel(char** levelName, char** levelFolder)
{
	if (levelName)
	{
		*levelName = &m_editorLevelName[0];
	}

	if (levelFolder)
	{
		*levelFolder = &m_editorLevelFolder[0];
	}
}

void GameFramework::BeginLanQuery()
{
	GameQueryListener* pGameQueryListener = new GameQueryListener();
	m_pLanQueryListener = m_pNetwork->CreateLanQueryListener(pGameQueryListener);
	pGameQueryListener->SetNetListener(m_pLanQueryListener);

	this->EndCurrentQuery();

	m_pGameQueryListener = pGameQueryListener;
}

void GameFramework::EndCurrentQuery()
{
	if (m_pGameQueryListener)
	{
		m_pGameQueryListener->Complete();
		m_pGameQueryListener = nullptr;
	}
}

IActor* GameFramework::GetClientActor() const
{
	return m_pActionGame ? m_pActionGame->GetClientActor() : nullptr;
}

EntityId GameFramework::GetClientActorId() const
{
	IActor* pActor = this->GetClientActor();
	if (!pActor)
	{
		return 0;
	}

	return pActor->GetEntityId();
}

INetChannel* GameFramework::GetClientChannel() const
{
	if (!m_pActionGame)
	{
		return nullptr;
	}

	GameClientNub* pGameClientNub = m_pActionGame->GetGameClientNub();
	if (!pGameClientNub)
	{
		return nullptr;
	}

	GameClientChannel* pGameClientChannel = pGameClientNub->GetGameClientChannel();
	if (!pGameClientChannel)
	{
		return nullptr;
	}

	return pGameClientChannel->GetNetChannel();
}

CTimeValue GameFramework::GetServerTime()
{
	if (gEnv->bServer)
	{
		return gEnv->pTimer->GetFrameStartTime();
	}

	INetChannel* pNetChannel = this->GetClientChannel();
	if (!pNetChannel)
	{
		return CTimeValue();
	}

	return pNetChannel->GetRemoteTime();
}

uint16 GameFramework::GetGameChannelId(INetChannel* pNetChannel)
{
	if (!gEnv->bServer)
	{
		return 0;
	}

	GameServerNub* pGameServerNub = this->GetGameServerNub();
	if (!pGameServerNub)
	{
		return 0;
	}

	return pGameServerNub->GetChannelId(pNetChannel);
}

bool GameFramework::IsChannelOnHold(uint16 channelId)
{
	GameServerNub* pGameServerNub = this->GetGameServerNub();
	if (!pGameServerNub)
	{
		return false;
	}

	GameServerChannel* pChannel = pGameServerNub->GetChannel(channelId);
	if (!pChannel)
	{
		return false;
	}

	return pChannel->IsOnHold();
}

INetChannel* GameFramework::GetNetChannel(uint16 channelId)
{
	GameServerNub* pGameServerNub = this->GetGameServerNub();
	if (!pGameServerNub)
	{
		return nullptr;
	}

	GameServerChannel* pChannel = pGameServerNub->GetChannel(channelId);
	if (!pChannel)
	{
		return nullptr;
	}

	return pChannel->GetNetChannel();
}

IGameObject* GameFramework::GetGameObject(EntityId id)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (!pEntity)
	{
		return nullptr;
	}

	GameObject* pGameObject = static_cast<GameObject*>(pEntity->GetProxy(ENTITY_PROXY_USER));
	if (!pGameObject)
	{
		return nullptr;
	}

	return pGameObject;
}

bool GameFramework::GetNetworkSafeClassId(uint16& id, const char* className)
{
	if (!m_pActionGame)
	{
		return false;
	}

	GameContext* pGameContext = m_pActionGame->GetGameContext();
	if (!pGameContext)
	{
		return false;
	}

	return pGameContext->ClassIdFromName(id, className);
}

bool GameFramework::GetNetworkSafeClassName(char* className, size_t maxn, uint16 id)
{
	if (!m_pActionGame)
	{
		return false;
	}

	GameContext* pGameContext = m_pActionGame->GetGameContext();
	if (!pGameContext)
	{
		return false;
	}

	CryStringT<char> name;
	if (!pGameContext->ClassNameFromId(name, id))
	{
		return false;
	}

	std::strncpy(className, name.c_str(), maxn);

	return true;
}

IGameObjectExtension* GameFramework::QueryGameObjectExtension(EntityId id, const char* name)
{
	IGameObject* pGameObject = this->GetGameObject(id);
	if (!pGameObject)
	{
		return nullptr;
	}

	return pGameObject->QueryExtension(name);
}

bool GameFramework::SaveGame(const char* path, bool quick, bool forceImmediate, ESaveGameReason reason, bool ignoreDelay, const char* checkPoint)
{
	if (gEnv->bMultiplayer)
	{
		return false;
	}

	IActor* pClientActor = this->GetClientActor();
	if (!pClientActor)
	{
		return false;
	}

	if (pClientActor->GetHealth() <= 0)
	{
		CryLogErrorAlways("%s: Player is dead!", __FUNCTION__);
		return false;
	}

	if (!this->CanSave())
	{
		CryLogErrorAlways("%s: Suppressing QS", __FUNCTION__);
		return false;
	}

	if (m_lastSaveLoad > 0.0f)
	{
		if (ignoreDelay)
		{
			m_lastSaveLoad = 0.0f;
		}
		else
		{
			return false;
		}
	}

	if (!forceImmediate)
	{
		m_delayedSaveGameMethod = quick ? 1 : 2;
		m_delayedSaveGameReason = reason;
		m_pSomeStrings->delayedSaveGameName = path;

		if (checkPoint)
		{
			m_pSomeStrings->checkPointName = checkPoint;
		}
		else
		{
			m_pSomeStrings->checkPointName.clear();
		}

		this->OnActionEvent(SActionEvent(eAE_preSaveGame, static_cast<int>(reason)));

		return true;
	}

	if (m_pSomeStrings->delayedSaveGameName.empty())
	{
		this->OnActionEvent(SActionEvent(eAE_preSaveGame, static_cast<int>(reason)));
	}

	CTimeValue elapsed = -gEnv->pTimer->GetAsyncTime();

	gEnv->pSystem->SerializingFile(quick ? 1 : 2);
	const bool result = m_pGameSerialize->SaveGame(this, "xml", path, reason, checkPoint);
	gEnv->pSystem->SerializingFile(0);

	this->OnActionEvent(SActionEvent(eAE_postSaveGame, static_cast<int>(reason), result ? "" : nullptr));

	m_lastSaveLoad = 0.5f;
	elapsed += gEnv->pTimer->GetAsyncTime();

	CryLogAlways("%s: '%s' %s in %.4f seconds", __FUNCTION__, path, result ? "done" : "failed", elapsed.GetSeconds());

	return result;
}

bool GameFramework::LoadGame(const char* path, bool quick, bool ignoreDelay)
{
	if (gEnv->bMultiplayer)
	{
		return false;
	}

	if (m_lastSaveLoad > 0.0f)
	{
		if (ignoreDelay)
		{
			m_lastSaveLoad = 0.0f;
		}
		else
		{
			return false;
		}
	}

	if (!this->CanLoad())
	{
		CryLogErrorAlways("%s: Suppressing QL", __FUNCTION__);
		return false;
	}

	CTimeValue elapsed = -gEnv->pTimer->GetAsyncTime();

	gEnv->pSystem->SerializingFile(quick ? 1 : 2);

	SGameStartParams params;
	params.flags = eGSF_Client | eGSF_Server;
	params.hostname = "localhost";
	params.maxPlayers = 1;
	params.port = gEnv->pConsole->GetCVar("sv_port")->GetIVal();

	gEnv->pEntitySystem->PauseTimers(true, false);

	CryLogAlways("%s: '%s'", __FUNCTION__, path);

	const GameSerialize::LoadResult result = m_pGameSerialize->LoadGame(this, "xml", path, params, quick, true);
	if (result != GameSerialize::LoadResult::OK)
	{
		if (result == GameSerialize::LoadResult::FAILED)
		{
			CryLogErrorAlways("%s: '%s' failed", __FUNCTION__, path);
		}
		else if (result == GameSerialize::LoadResult::FAILED_AND_DESTROYED_STATE)
		{
			CryLogErrorAlways("%s: '%s' failed and destroyed state", __FUNCTION__, path);
			this->EndGameContext();
		}
		else
		{
			CryLogErrorAlways("%s: '%s' unknown error %d", __FUNCTION__, path, static_cast<int>(result));
			gEnv->pEntitySystem->PauseTimers(false, false);
			this->EndGameContext();
		}

		gEnv->pSystem->SerializingFile(0);

		return false;
	}

	gEnv->pEntitySystem->PauseTimers(false, false);
	gEnv->pSystem->SerializingFile(0);

	this->AllowSave(true);

	m_lastSaveLoad = 0.5f;
	elapsed += gEnv->pTimer->GetAsyncTime();

	CryLogAlways("%s: '%s' done in %.4f seconds", __FUNCTION__, path, elapsed.GetSeconds());

	return true;
}

void GameFramework::OnEditorSetGameMode(int mode)
{
	if (mode < 2)
	{
		const bool isGameMode = mode != 0;

		if (m_pActionGame)
		{
			m_pActionGame->OnEditorSetGameMode(isGameMode);
		}

		if (m_pMaterialEffects)
		{
			if (isGameMode && m_isEditing)
			{
				m_pMaterialEffects->PreLoadAssets();
			}

			m_pMaterialEffects->SetUpdateMode(isGameMode);
		}

		m_isEditing = !isGameMode;
	}
	else if (m_pActionGame)
	{
		m_pActionGame->FixBrokenObjects();
		m_pActionGame->ClearBreakHistory();
	}
}

bool GameFramework::IsEditing()
{
	return m_isEditing;
}

bool GameFramework::IsInLevelLoad()
{
	if (!m_pActionGame)
	{
		return false;
	}

	GameContext* pGameContext = m_pActionGame->GetGameContext();
	if (!pGameContext)
	{
		return false;
	}

	return pGameContext->IsInLevelLoad();
}

bool GameFramework::IsLoadingSaveGame()
{
	if (!m_pActionGame)
	{
		return false;
	}

	GameContext* pGameContext = m_pActionGame->GetGameContext();
	if (!pGameContext)
	{
		return false;
	}

	return pGameContext->IsLoadingSaveGame();
}

bool GameFramework::IsInTimeDemo()
{
	return m_pTimeDemoRecorder->IsPlaying() || m_pTimeDemoRecorder->IsRecording();
}

void GameFramework::AllowSave(bool allow)
{
	m_isSaveAllowed = allow;
}

void GameFramework::AllowLoad(bool allow)
{
	m_isLoadAllowed = allow;
}

bool GameFramework::CanSave()
{
	return m_isSaveAllowed && !m_pViewSystem->IsPlayingCutScene() && !this->IsInTimeDemo();
}

bool GameFramework::CanLoad()
{
	return m_isLoadAllowed;
}

bool GameFramework::CanCheat()
{
	return !gEnv->bMultiplayer || gEnv->pSystem->IsDevMode();
}

const char* GameFramework::GetLevelName()
{
	if (m_pSystem->IsEditor())
	{
		// does this ever contain anything?
		return m_editorLevelName;
	}

	if (!this->StartedGameContext())
	{
		return nullptr;
	}

	ILevel* pLevel = m_pLevelSystem->GetCurrentLevel();
	if (!pLevel)
	{
		return nullptr;
	}

	ILevelInfo* pLevelInfo = pLevel->GetLevelInfo();
	if (!pLevelInfo)
	{
		return nullptr;
	}

	return pLevelInfo->GetName();
}

const char* GameFramework::GetAbsLevelPath()
{
	static char buffer[256];

	if (m_pSystem->IsEditor())
	{
		// does this ever contain anything?
		std::strncpy(buffer, m_editorLevelFolder, sizeof(buffer) - 1);
	}
	else
	{
		if (!this->StartedGameContext())
		{
			return nullptr;
		}

		ILevel* pLevel = m_pLevelSystem->GetCurrentLevel();
		if (!pLevel)
		{
			return nullptr;
		}

		ILevelInfo* pLevelInfo = pLevel->GetLevelInfo();
		if (!pLevelInfo)
		{
			return nullptr;
		}

		std::snprintf(buffer, sizeof(buffer), "Game/%s", pLevelInfo->GetPath());
	}

	return buffer;
}

IPersistantDebug* GameFramework::GetIPersistantDebug()
{
	return m_pPersistantDebug;
}

IGameStatsConfig* GameFramework::GetIGameStatsConfig()
{
	return m_pGameStatsConfig;
}

IAnimationGraphState* GameFramework::GetMusicGraphState()
{
	return m_pMusicGraphState;
}

IMusicLogic* GameFramework::GetMusicLogic()
{
	return m_pMusicLogic;
}

void GameFramework::RegisterListener(IGameFrameworkListener* pListener, const char* name, EFRAMEWORKLISTENERPRIORITY priority)
{
	for (Listener& listener : *m_pListeners)
	{
		if (listener.pListener == pListener)
		{
			return;
		}
	}

	auto it = m_pListeners->begin();
	for (; it != m_pListeners->end(); ++it)
	{
		if (it->priority > priority)
		{
			break;
		}
	}

	m_pListeners->insert(it, Listener{pListener, name, priority});
}

void GameFramework::UnregisterListener(IGameFrameworkListener* pListener)
{
	for (auto it = m_pListeners->begin(); it != m_pListeners->end(); ++it)
	{
		if (it->pListener == pListener)
		{
			m_pListeners->erase(it);
			return;
		}
	}
}

INetNub* GameFramework::GetServerNetNub()
{
	if (!m_pActionGame)
	{
		return nullptr;
	}

	return m_pActionGame->GetServerNetNub();
}

void GameFramework::SetGameGUID(const char* gameGUID)
{
	std::strncpy(m_guid, gameGUID, sizeof(m_guid) - 1);
	m_guid[sizeof(m_guid) - 1] = '\0';
}

const char* GameFramework::GetGameGUID()
{
	return m_guid;
}

INetContext* GameFramework::GetNetContext()
{
	if (!m_pActionGame)
	{
		return nullptr;
	}

	GameContext* pGameContext = m_pActionGame->GetGameContext();
	if (!pGameContext)
	{
		return nullptr;
	}

	return pGameContext->GetNetContext();
}

void GameFramework::GetMemoryStatistics(ICrySizer*)
{
	// not implemented
}

void GameFramework::EnableVoiceRecording(const bool enable)
{
	m_voiceRecording = enable ? 1 : 0;
}

void GameFramework::MutePlayerById(EntityId mutePlayer)
{
#ifdef BUILD_64BIT
	std::uintptr_t func = CRYACTION_BASE + 0x321FC0;
#else
	std::uintptr_t func = CRYACTION_BASE + 0x22FC30;
#endif

	(this->*reinterpret_cast<void(GameFramework::*&)(EntityId)>(func))(mutePlayer);
}

IDebugHistoryManager* GameFramework::CreateDebugHistoryManager()
{
	return new DebugHistoryManager();
}

void GameFramework::DumpMemInfo(const char* format, ...)
{
	CryLogErrorAlways("%s: Not implemented!", __FUNCTION__);
}

bool GameFramework::IsVoiceRecordingEnabled()
{
	return m_voiceRecording != 0;
}

bool GameFramework::IsImmersiveMPEnabled()
{
	if (!m_pActionGame)
	{
		return false;
	}

	GameContext* pGameContext = m_pActionGame->GetGameContext();
	if (!pGameContext)
	{
		return false;
	}

	return pGameContext->GetFlags() & eGSF_ImmersiveMultiplayer;
}

void GameFramework::ExecuteCommandNextFrame(const char* command)
{
	m_pNextFrameCommand->assign(command);
}

void GameFramework::ShowPageInBrowser(const char* url)
{
	CryLogErrorAlways("%s(%s): Not implemented!", __FUNCTION__, url);
}

bool GameFramework::StartProcess(const char* cmdLine)
{
	CryLogErrorAlways("%s(%s): Not implemented!", __FUNCTION__, cmdLine);
	return false;
}

bool GameFramework::SaveServerConfig(const char* path)
{
#ifdef BUILD_64BIT
	std::uintptr_t func = CRYACTION_BASE + 0x31B580;
#else
	std::uintptr_t func = CRYACTION_BASE + 0x21FA90;
#endif

	return (this->*reinterpret_cast<bool(GameFramework::*&)(const char*)>(func))(path);
}

void GameFramework::PrefetchLevelAssets(const bool enforceAll)
{
	m_pItemSystem->PrecacheLevel();
}

bool GameFramework::GetModInfo(SModInfo* modInfo, const char* modPath)
{
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void GameFramework::ScheduleEndLevel(const char* nextLevel)
{
	m_isLevelEndScheduled = true;
	m_pSomeStrings->nextLevelToLoad = nextLevel;
}

GameServerNub* GameFramework::GetGameServerNub()
{
	if (!m_pActionGame)
	{
		return nullptr;
	}

	return m_pActionGame->GetGameServerNub();
}

void GameFramework::OnActionEvent(const SActionEvent& event)
{
	this->CallListeners([&event](IGameFrameworkListener* pListener) { pListener->OnActionEvent(event); });
}

void GameFramework::InitCVars()
{
#ifdef BUILD_64BIT
	std::uintptr_t func = CRYACTION_BASE + 0x3212a0;
#else
	std::uintptr_t func = CRYACTION_BASE + 0x221ab0;
#endif

	(this->*reinterpret_cast<void(GameFramework::*&)()>(func))();
}

void GameFramework::InitCommands()
{
#ifdef BUILD_64BIT
	std::uintptr_t func = CRYACTION_BASE + 0x3277a0;
#else
	std::uintptr_t func = CRYACTION_BASE + 0x243600;
#endif

	(this->*reinterpret_cast<void(GameFramework::*&)()>(func))();
}

void GameFramework::InitScriptBinds()
{
	m_pScriptBind_Network = new ScriptBind_Network(m_pSystem, this);
	m_pScriptBind_CryAction = new ScriptBind_CryAction(this);
	m_pScriptBind_ItemSystem = new ScriptBind_ItemSystem(m_pSystem, m_pItemSystem, this);
	m_pScriptBind_Actor = new ScriptBind_Actor(m_pSystem, this);
	m_pScriptBind_AI = new ScriptBind_AI(m_pSystem);
	m_pScriptBind_ActionMapManager = new ScriptBind_ActionMapManager(m_pSystem, m_pActionMapManager);
	m_pScriptBind_VehicleSystem = new ScriptBind_VehicleSystem(m_pVehicleSystem);
	m_pScriptBind_Vehicle = new ScriptBind_Vehicle(m_pSystem, this);
	m_pScriptBind_VehicleSeat = new ScriptBind_VehicleSeat(m_pSystem, this);
	m_pScriptBind_Inventory = new ScriptBind_Inventory(m_pSystem, this);
	m_pScriptBind_DialogSystem = new ScriptBind_DialogSystem(m_pSystem, m_pDialogSystem);
}

void GameFramework::CheckEndLevelSchedule()
{
#ifdef BUILD_64BIT
	std::uintptr_t func = CRYACTION_BASE + 0x324a50;
#else
	std::uintptr_t func = CRYACTION_BASE + 0x2418e0;
#endif

	(this->*reinterpret_cast<void(GameFramework::*&)()>(func))();
}
