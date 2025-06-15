/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$

 -------------------------------------------------------------------------
  History:
  - 3:8:2004   11:26 : Created by Márcio Martins
  - 17:8:2005        : Modified - NickH: Factory registration moved to GameFactory.cpp

*************************************************************************/
#include <tracy/Tracy.hpp>

#include "CryCommon/CrySystem/ISystem.h"
#include "Game.h"
#include "GameCVars.h"
#include "GameActions.h"
#include "Menus/FlashMenuObject.h"
#include "Menus/OptionsManager.h"

#include "GameRules.h"
#include "BulletTime.h"
#include "SoundMoods.h"
#include "HUD/HUD.h"
#include "Items/Weapons/WeaponSystem.h"

#include "CryCommon/CrySystem/CryPath.h"
#include "CryCommon/CryAction/IActionMapManager.h"
#include "CryCommon/CryAction/IViewSystem.h"
#include "CryCommon/CryAction/ILevelSystem.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryMovie/IMovieSystem.h"
#include "CryCommon/CryAction/IPlayerProfiles.h"

#include "Actors/ScriptBind_Actor.h"
#include "Items/ScriptBind_Item.h"
#include "Items/Weapons/ScriptBind_Weapon.h"
#include "ScriptBind_GameRules.h"
#include "ScriptBind_Game.h"
#include "HUD/ScriptBind_HUD.h"

#include "GameFactory.h"

#include "Items/ItemSharedParams.h"

#include "Nodes/G2FlowBaseNode.h"

#include "ServerSynchedStorage.h"
#include "ClientSynchedStorage.h"

#include "SPAnalyst.h"

#include "CryCommon/CryAction/ISaveGame.h"
#include "CryCommon/CryAction/ILoadGame.h"

#include "CryMP/Client/WeatherSystem.h"
#include "CryMP/Client/Advertising.h"
#include "CryMP/Client/HealthManager.h"

#if defined(CRYSIS_BETA)
#define CRYSIS_GUID "{CDC82B4A-7540-45A5-B92E-9A7C7033DBF4}"
#elif defined(SP_DEMO)
#define CRYSIS_GUID "{CDC82B4A-7540-45A5-B92E-9A7C7033DBF3}"
#else
#define CRYSIS_GUID "{CDC82B4A-7540-45A5-B92E-9A7C7033DBF2}"
#endif

//FIXME: really horrible. Remove ASAP
int OnImpulse(const EventPhys* pEvent)
{
	//return 1;
	return 0;
}

//

// Needed for the Game02 specific flow node
CG2AutoRegFlowNodeBase* CG2AutoRegFlowNodeBase::m_pFirst = 0;
CG2AutoRegFlowNodeBase* CG2AutoRegFlowNodeBase::m_pLast = 0;

CGame* g_pGame = 0;
SCVars* g_pGameCVars = 0;
CGameActions* g_pGameActions = 0;

CGame::CGame()
	: m_pFramework(0),
	m_pConsole(0),
	m_pWeaponSystem(0),
	m_pFlashMenuObject(0),
	m_pOptionsManager(0),
	m_pScriptBindActor(0),
	m_pScriptBindGame(0),
	m_pPlayerProfileManager(0),
	m_pBulletTime(0),
	m_pSoundMoods(0),
	m_pHUD(0),
	m_pServerSynchedStorage(0),
	m_pClientSynchedStorage(0),
	m_uiPlayerID(-1),
	m_pSPAnalyst(0),
	m_pWeatherSystem(0)
{
	m_pCVars = new SCVars();
	g_pGameCVars = m_pCVars;
	m_pGameActions = new CGameActions();
	g_pGameActions = m_pGameActions;
	g_pGame = this;
	m_bReload = false;

	m_pDebugAM = 0;
	m_pDefaultAM = 0;
	m_pMultiplayerAM = 0;

	m_pWeatherSystem = new CWeatherSystem();
	m_pAdManager = new CAdManager();
	m_pHealthManager = new CHealthManager();

	GetISystem()->SetIGame(this);
}

CGame::~CGame()
{
	m_pFramework->EndGameContext();
	m_pFramework->UnregisterListener(this);
	ReleaseScriptBinds();
	ReleaseActionMaps();
	SAFE_DELETE(m_pFlashMenuObject);
	SAFE_DELETE(m_pOptionsManager);
	SAFE_DELETE(m_pBulletTime);
	SAFE_DELETE(m_pSoundMoods);
	SAFE_DELETE(m_pHUD);
	SAFE_DELETE(m_pSPAnalyst);
	m_pWeaponSystem->Release();
	m_pWeatherSystem->Reset();
	m_pAdManager->Reset();
	SAFE_DELETE(m_pHealthManager);
	SAFE_DELETE(m_pItemStrings);
	SAFE_DELETE(m_pItemSharedParamsList);
	SAFE_DELETE(m_pCVars);
	g_pGame = 0;
	g_pGameCVars = 0;
	g_pGameActions = 0;
}

bool CGame::Init(IGameFramework* pFramework)
{
	LOADING_TIME_PROFILE_SECTION(GetISystem());

	m_pFramework = pFramework;
	assert(m_pFramework);

	m_pConsole = gEnv->pConsole;

	RegisterConsoleVars();
	RegisterConsoleCommands();
	RegisterGameObjectEvents();

	// Initialize static item strings
	m_pItemStrings = new SItemStrings();

	m_pItemSharedParamsList = new CItemSharedParamsList();

	LoadActionMaps();

	InitScriptBinds();

	//load user levelnames for ingame text and savegames
	XmlNodeRef lnames = GetISystem()->LoadXmlFile("Game/Scripts/GameRules/LevelNames.xml");
	if (lnames)
	{
		int num = lnames->getNumAttributes();
		const char* nameA, * nameB;
		for (int n = 0; n < num; ++n)
		{
			lnames->getAttributeByIndex(n, &nameA, &nameB);
			m_mapNames[string(nameA)] = string(nameB);
		}
	}

	// Register all the games factory classes e.g. maps "Player" to CPlayer
	InitGameFactory(m_pFramework);

	//FIXME: horrible, remove this ASAP
	//gEnv->pPhysicalWorld->AddEventClient( EventPhysImpulse::id,OnImpulse,0 );  

	m_pWeaponSystem = new CWeaponSystem(this, GetISystem());

	string itemFolder = "scripts/entities/items/xml";
	pFramework->GetIItemSystem()->Scan(itemFolder.c_str());
	m_pWeaponSystem->RegisterXMLData();

	m_pOptionsManager = COptionsManager::CreateOptionsManager();

	m_pSPAnalyst = new CSPAnalyst();

	gEnv->pConsole->CreateKeyBind("f12", "r_getscreenshot 2");

	//Ivo: initialites the Crysis conversion file.
	//this is a conversion solution for the Crysis game DLL. Other projects don't need it.
	// No need anymore
	//gEnv->pCharacterManager->LoadCharacterConversionFile("Objects/CrysisCharacterConversion.ccc");

	// set game GUID
	m_pFramework->SetGameGUID(CRYSIS_GUID);

	// TEMP
	// Load the action map beforehand (see above)
	// afterwards load the user's profile whose action maps get merged with default's action map
	m_pPlayerProfileManager = m_pFramework->GetIPlayerProfileManager();

	bool bIsFirstTime = false;
	const bool bResetProfile = gEnv->pSystem->GetICmdLine()->FindArg(eCLAT_Pre, "ResetProfile") != 0;
	if (m_pPlayerProfileManager)
	{
		const char* userName = gEnv->pSystem->GetUserName();

		bool ok = m_pPlayerProfileManager->LoginUser(userName, bIsFirstTime);
		if (ok)
		{

			// activate the always present profile "default"
			int profileCount = m_pPlayerProfileManager->GetProfileCount(userName);
			if (profileCount > 0)
			{
				bool handled = false;
				if (gEnv->pSystem->IsDedicated())
				{
					for (int i = 0; i < profileCount; ++i)
					{
						IPlayerProfileManager::SProfileDescription profDesc;
						bool ok = m_pPlayerProfileManager->GetProfileInfo(userName, i, profDesc);
						if (ok)
						{
							const IPlayerProfile* preview = m_pPlayerProfileManager->PreviewProfile(userName, profDesc.name);
							int iActive = 0;
							if (preview)
							{
								preview->GetAttribute("Activated", iActive);
							}
							if (iActive > 0)
							{
								m_pPlayerProfileManager->ActivateProfile(userName, profDesc.name);
								CryLogAlways("[GameProfiles]: Successfully activated profile '%s' for user '%s'", profDesc.name, userName);
								m_pFramework->GetILevelSystem()->LoadRotation();
								handled = true;
								break;
							}
						}
					}
					m_pPlayerProfileManager->PreviewProfile(userName, NULL);
				}

				if (!handled)
				{
					IPlayerProfileManager::SProfileDescription desc;
					ok = m_pPlayerProfileManager->GetProfileInfo(userName, 0, desc);
					if (ok)
					{
						IPlayerProfile* pProfile = m_pPlayerProfileManager->ActivateProfile(userName, desc.name);

						if (pProfile == 0)
						{
							CryLogWarningAlways("[GameProfiles]: Cannot activate profile '%s' for user '%s'. Trying to re-create.", desc.name, userName);
							IPlayerProfileManager::EProfileOperationResult profileResult;
							m_pPlayerProfileManager->CreateProfile(userName, desc.name, true, profileResult); // override if present!
							pProfile = m_pPlayerProfileManager->ActivateProfile(userName, desc.name);
							if (pProfile == 0)
								CryLogWarningAlways("[GameProfiles]: Cannot activate profile '%s' for user '%s'.", desc.name, userName);
							else
								CryLogWarningAlways("[GameProfiles]: Successfully re-created profile '%s' for user '%s'.", desc.name, userName);
						}

						if (pProfile)
						{
							if (bResetProfile)
							{
								bIsFirstTime = true;
								pProfile->Reset();
								gEnv->pCryPak->RemoveFile("%USER%/game.cfg");
								CryLogAlways("[GameProfiles]: Successfully reset and activated profile '%s' for user '%s'", desc.name, userName);
							}
							CryLogAlways("[GameProfiles]: Successfully activated profile '%s' for user '%s'", desc.name, userName);
							m_pFramework->GetILevelSystem()->LoadRotation();
						}
					}
					else
					{
						CryLogWarningAlways("[GameProfiles]: Cannot get profile info for user '%s'", userName);
					}
				}
			}
			else
			{
				CryLogWarningAlways("[GameProfiles]: User 'dude' has no profiles");
			}
		}
		else
			CryLogWarningAlways("[GameProfiles]: Cannot login user '%s'", userName);
	}
	else
		CryLogWarningAlways("[GameProfiles]: PlayerProfileManager not available. Running without.");

	m_pOptionsManager->SetProfileManager(m_pPlayerProfileManager);

	if (!gEnv->pSystem->IsDedicated())
	{
		m_pFlashMenuObject = new CFlashMenuObject;
		m_pFlashMenuObject->Load();
	}

	if (bIsFirstTime)
	{
		if (m_pOptionsManager->IsFirstStart() || bResetProfile)
		{
			CryLogAlways("[GameProfiles]: PlayerProfileManager reported first-time login. Running AutoDetectSpec.");
			// run autodetectspec
			gEnv->pSystem->AutoDetectSpec();
			m_pOptionsManager->SystemConfigChanged(true);
		}
		else
		{
			CryLogAlways("[GameProfiles]: PlayerProfileManager reported first-time login. AutoDetectSpec NOT running because g_startFirstTime=0.");
		}
	}


	if (!m_pServerSynchedStorage)
		m_pServerSynchedStorage = new CServerSynchedStorage(GetIGameFramework());

	if (!m_pBulletTime)
	{
		m_pBulletTime = new CBulletTime();
	}

	if (!m_pSoundMoods)
	{
		m_pSoundMoods = new CSoundMoods();
	}

	m_pFramework->RegisterListener(this, "Game", FRAMEWORKLISTENERPRIORITY_GAME);

	return true;
}

bool CGame::CompleteInit()
{
	// Initialize Game02 flow nodes

	if (IFlowSystem* pFlow = m_pFramework->GetIFlowSystem())
	{
		CG2AutoRegFlowNodeBase* pFactory = CG2AutoRegFlowNodeBase::m_pFirst;

		while (pFactory)
		{
			pFlow->RegisterType(pFactory->m_sClassName, pFactory);
			pFactory = pFactory->m_pNext;
		}
	}

	return true;
}

int CGame::Update(bool haveFocus, unsigned int updateFlags)
{
	ZoneScoped;

	bool bRun = m_pFramework->PreUpdate(true, updateFlags);
	float frameTime = gEnv->pTimer->GetFrameTime();

	if (m_pFramework->IsGamePaused() == false)
	{
		m_pWeaponSystem->Update(frameTime);

		m_pBulletTime->Update();
		m_pSoundMoods->Update();
	}

	m_pFramework->PostUpdate(true, updateFlags);
	m_pWeatherSystem->Update(frameTime);
	m_pAdManager->Update(frameTime);

	CheckReloadLevel();

	return bRun ? 1 : 0;
}

void CGame::ConfigureGameChannel(bool isServer, IProtocolBuilder* pBuilder)
{
	if (isServer)
		m_pServerSynchedStorage->DefineProtocol(pBuilder);
	else
	{
		m_pClientSynchedStorage = new CClientSynchedStorage(GetIGameFramework());
		m_pClientSynchedStorage->DefineProtocol(pBuilder);
	}
}

void CGame::EditorResetGame(bool bStart)
{
	//Editor not supported 
	/*
	CRY_ASSERT(gEnv->pSystem->IsEditor());

	if(bStart)
	{
		IActionMapManager* pAM = m_pFramework->GetIActionMapManager();
		if (pAM)
		{
			pAM->EnableActionMap(0, true); // enable all action maps
			pAM->EnableFilter(0, false); // disable all filters
		}
		m_pHUD = new CHUD;
		m_pHUD->Init();
		m_pHUD->PlayerIdSet(m_uiPlayerID);
	}
	else
	{
		SAFE_DELETE(m_pHUD);
	}*/
}

void CGame::PlayerIdSet(EntityId playerId)
{
	if (!gEnv->pSystem->IsEditor() && playerId != 0 && !gEnv->pSystem->IsDedicated())
	{
		GetMenu()->DestroyIngameMenu();
		GetMenu()->DestroyStartMenu();
	}

	if (m_pHUD)
	{
		//m_pHUD->PlayerIdSet(playerId);
	}
	else
	{
		m_uiPlayerID = playerId;
	}
}

string CGame::InitMapReloading()
{
	string levelFileName = GetIGameFramework()->GetLevelName();
	levelFileName = CryPath::GetFileName(levelFileName);
	if (const char* visibleName = GetMappedLevelName(levelFileName.c_str()))
		levelFileName = visibleName;
	//levelFileName.append("_levelstart.crysisjmsf"); //because of the french law we can't do this ...
	levelFileName.append("_crysis.crysisjmsf");
	bool foundSaveGame = false;
	if (m_pPlayerProfileManager)
	{
		const char* userName = GetISystem()->GetUserName();
		IPlayerProfile* pProfile = m_pPlayerProfileManager->GetCurrentProfile(userName);
		if (pProfile)
		{
			const char* sharedSaveGameFolder = m_pPlayerProfileManager->GetSharedSaveGameFolder();
			if (sharedSaveGameFolder && *sharedSaveGameFolder)
			{
				string prefix = pProfile->GetName();
				prefix += "_";
				levelFileName = prefix + levelFileName;
			}
			ISaveGameEnumeratorPtr pSGE = pProfile->CreateSaveGameEnumerator();
			ISaveGameEnumerator::SGameDescription desc;
			const int nSaveGames = pSGE->GetCount();
			for (int i = 0; i < nSaveGames; ++i)
			{
				if (pSGE->GetDescription(i, desc))
				{
					if (!_stricmp(desc.name, levelFileName.c_str()))
					{
						m_bReload = true;
						return levelFileName;
					}
				}
			}
		}
	}
	m_bReload = false;
	levelFileName.clear();
	return levelFileName;
}

void CGame::Shutdown()
{
	if (m_pPlayerProfileManager)
	{
		m_pPlayerProfileManager->LogoutUser(m_pPlayerProfileManager->GetCurrentUser());
	}

	delete m_pServerSynchedStorage;
	m_pServerSynchedStorage = 0;

	this->~CGame();
}

const char* CGame::GetLongName()
{
	return "Crysis";
}

const char* CGame::GetName()
{
	return "Crysis";
}

void CGame::OnPostUpdate(float fDeltaTime)
{
}

void CGame::OnSaveGame(ISaveGame* pSaveGame)
{
	CPlayer* pPlayer = static_cast<CPlayer*>(GetIGameFramework()->GetClientActor());
	GetGameRules()->PlayerPosForRespawn(pPlayer, true);

	//save difficulty
	pSaveGame->AddMetadata("sp_difficulty", g_pGameCVars->g_difficultyLevel);

	//save mod info
	SModInfo info;
	if (GetIGameFramework()->GetModInfo(&info))
	{
		pSaveGame->AddMetadata("ModName", info.m_name);
		pSaveGame->AddMetadata("ModVersion", info.m_version);
	}

	//write file to profile
	if (m_pPlayerProfileManager)
	{
		const char* saveGameFolder = m_pPlayerProfileManager->GetSharedSaveGameFolder();
		const bool bSaveGameFolderShared = saveGameFolder && *saveGameFolder;
		const char* user = m_pPlayerProfileManager->GetCurrentUser();
		if (IPlayerProfile* pProfile = m_pPlayerProfileManager->GetCurrentProfile(user))
		{
			string filename(pSaveGame->GetFileName());
			CryFixedStringT<128> profilename(pProfile->GetName());
			profilename += '_';
			filename = filename.substr(filename.rfind('/') + 1);
			// strip profileName_ prefix
			if (bSaveGameFolderShared)
			{
				if (_strnicmp(filename.c_str(), profilename.c_str(), profilename.length()) == 0)
					filename = filename.substr(profilename.length());
			}
			pProfile->SetAttribute("Singleplayer.LastSavedGame", filename);
		}
	}

	pSaveGame->AddMetadata("v_altitudeLimit", g_pGameCVars->pAltitudeLimitCVar->GetString());
}

void CGame::OnLoadGame(ILoadGame* pLoadGame)
{
	int difficulty = g_pGameCVars->g_difficultyLevel;
	pLoadGame->GetMetadata("sp_difficulty", difficulty);
	if (difficulty != g_pGameCVars->g_difficultyLevel)
	{
		m_pFlashMenuObject->SetDifficulty(difficulty);
		//ICVar *diff = gEnv->pConsole->GetCVar("g_difficultyLevel");
		//if(diff)
		//{
			//string diffVal = "Option.";
			//diffVal.append(diff->GetName());
			//GetOptions()->SaveCVarToProfile(diffVal.c_str(), diff->GetString());
		IPlayerProfile* pProfile = m_pPlayerProfileManager->GetCurrentProfile(m_pPlayerProfileManager->GetCurrentUser());
		if (pProfile)
		{
			pProfile->SetAttribute("Singleplayer.LastSelectedDifficulty", difficulty);
			pProfile->SetAttribute("Option.g_difficultyLevel", difficulty);
			IPlayerProfileManager::EProfileOperationResult result;
			m_pPlayerProfileManager->SaveProfile(m_pPlayerProfileManager->GetCurrentUser(), result);
		}
		//}
	}

	// altitude limit
	const char* v_altitudeLimit = pLoadGame->GetMetadata("v_altitudeLimit");
	if (v_altitudeLimit && *v_altitudeLimit)
		g_pGameCVars->pAltitudeLimitCVar->ForceSet(v_altitudeLimit);
	else
	{
		CryFixedStringT<128> buf;
		buf.FormatFast("%g", g_pGameCVars->v_altitudeLimitDefault());
		g_pGameCVars->pAltitudeLimitCVar->ForceSet(buf.c_str());
	}
}

void CGame::OnActionEvent(const SActionEvent& event)
{
	//SAFE_MENU_FUNC(OnActionEvent(event)); - FlashMenuObject is already a registered CryAction listener - Lin
	switch (event.m_event)
	{
	case  eAE_channelDestroyed:
		GameChannelDestroyed(event.m_value == 1);
		break;
	case eAE_serverIp:
		if (gEnv->bServer && GetServerSynchedStorage())
		{
			GetServerSynchedStorage()->SetGlobalValue(GLOBAL_SERVER_IP_KEY, string(event.m_description));
			GetServerSynchedStorage()->SetGlobalValue(GLOBAL_SERVER_PUBLIC_PORT_KEY, event.m_value);
		}
		break;
	case eAE_serverName:
		if (gEnv->bServer && GetServerSynchedStorage())
			GetServerSynchedStorage()->SetGlobalValue(GLOBAL_SERVER_NAME_KEY, string(event.m_description));
		break;
	}
}

void CGame::GameChannelDestroyed(bool isServer)
{
	if (!isServer)
	{
		delete m_pClientSynchedStorage;
		m_pClientSynchedStorage = 0;
		if (m_pHUD)
			m_pHUD->PlayerIdSet(0);

		if (!gEnv->pSystem->IsSerializingFile())
		{
			CryFixedStringT<128> buf;
			buf.FormatFast("%g", g_pGameCVars->v_altitudeLimitDefault());
			g_pGameCVars->pAltitudeLimitCVar->ForceSet(buf.c_str());
		}
		//the hud continues existing when the player got diconnected - it's part of the game
		/*if(!gEnv->pSystem->IsEditor())
		{
		SAFE_DELETE(m_pHUD);
		}*/
	}
}

void CGame::InitHUD(IActor* pActor)
{
	if (pActor && m_pHUD == 0)
	{
		m_pHUD = new CHUD();
		m_pHUD->Init(pActor);
	}
}

void CGame::DestroyHUD()
{
	SAFE_DELETE(m_pHUD);
}

void CGame::BlockingProcess(BlockingConditionFunction f)
{
	INetwork* pNetwork = gEnv->pNetwork;

	bool ok = false;

	ITimer* pTimer = gEnv->pTimer;
	CTimeValue startTime = pTimer->GetAsyncTime();

	while (!ok)
	{
		pNetwork->SyncWithGame(eNGS_FrameStart);
		pNetwork->SyncWithGame(eNGS_FrameEnd);
		gEnv->pTimer->UpdateOnFrameStart();
		ok |= (*f)();
	}
}

CGameRules* CGame::GetGameRules() const
{
	return static_cast<CGameRules*>(m_pFramework->GetIGameRulesSystem()->GetCurrentGameRules());
}

CBulletTime* CGame::GetBulletTime() const
{
	return m_pBulletTime;
}

CSoundMoods* CGame::GetSoundMoods() const
{
	return m_pSoundMoods;
}

CHUD* CGame::GetHUD() const
{
	return m_pHUD;
}

CFlashMenuObject* CGame::GetMenu() const
{
	return m_pFlashMenuObject;
}

COptionsManager* CGame::GetOptions() const
{
	return m_pOptionsManager;
}

void CGame::LoadActionMaps(const char* filename)
{
	if (g_pGame->GetIGameFramework()->IsGameStarted())
	{
		CryLogAlways("Can't change configuration while game is running (yet)");
		return;
	}

	IActionMapManager* pActionMapMan = m_pFramework->GetIActionMapManager();

	// make sure that they are also added to the GameActions.actions file!
	XmlNodeRef rootNode = m_pFramework->GetISystem()->LoadXmlFile(filename);
	if (rootNode)
	{
		pActionMapMan->Clear();
		pActionMapMan->LoadFromXML(rootNode);
		m_pDefaultAM = pActionMapMan->GetActionMap("default");
		m_pDebugAM = pActionMapMan->GetActionMap("debug");
		m_pMultiplayerAM = pActionMapMan->GetActionMap("multiplayer");

		// enable defaults
		pActionMapMan->EnableActionMap("default", true);

		// enable debug
		pActionMapMan->EnableActionMap("debug", true);

		// enable player action map
		pActionMapMan->EnableActionMap("player", true);
	}
	else
		CryLogAlways("Could not open configuration file");

	m_pGameActions->Init();
}

void CGame::ReleaseActionMaps()
{
	SAFE_RELEASE(m_pDebugAM);
	SAFE_RELEASE(m_pDefaultAM);
	SAFE_RELEASE(m_pMultiplayerAM);
	SAFE_DELETE(m_pGameActions);
	g_pGameActions = NULL;
}

void CGame::InitScriptBinds()
{
	m_pScriptBindActor = new CScriptBind_Actor(m_pFramework->GetISystem());
	m_pScriptBindItem = new CScriptBind_Item(m_pFramework->GetISystem(), m_pFramework);
	m_pScriptBindWeapon = new CScriptBind_Weapon(m_pFramework->GetISystem(), m_pFramework);
	m_pScriptBindHUD = new CScriptBind_HUD(m_pFramework->GetISystem(), m_pFramework);
	m_pScriptBindGameRules = new CScriptBind_GameRules(m_pFramework->GetISystem(), m_pFramework);
	m_pScriptBindGame = new CScriptBind_Game(m_pFramework->GetISystem(), m_pFramework);
}

void CGame::ReleaseScriptBinds()
{
	SAFE_DELETE(m_pScriptBindActor);
	SAFE_DELETE(m_pScriptBindItem);
	SAFE_DELETE(m_pScriptBindWeapon);
	SAFE_DELETE(m_pScriptBindHUD);
	SAFE_DELETE(m_pScriptBindGameRules);
	SAFE_DELETE(m_pScriptBindGame);
}

void CGame::CheckReloadLevel()
{
	if (!m_bReload)
		return;

	if (GetISystem()->IsEditor() || gEnv->bMultiplayer)
	{
		if (m_bReload)
			m_bReload = false;
		return;
	}

	// Restart interrupts cutscenes
	gEnv->pMovieSystem->StopAllCutScenes();

	GetISystem()->SerializingFile(1);

	//load levelstart
	ILevelSystem* pLevelSystem = m_pFramework->GetILevelSystem();
	ILevel* pLevel = pLevelSystem->GetCurrentLevel();
	ILevelInfo* pLevelInfo = pLevelSystem->GetLevelInfo(m_pFramework->GetLevelName());
	//**********
	EntityId playerID = GetIGameFramework()->GetClientActorId();
	pLevelSystem->OnLoadingStart(pLevelInfo);
	PlayerIdSet(playerID);
	string levelstart(GetIGameFramework()->GetLevelName());
	if (const char* visibleName = GetMappedLevelName(levelstart.c_str()))
		levelstart = visibleName;
	//levelstart.append("_levelstart.crysisjmsf"); //because of the french law we can't do this ...
	levelstart.append("_crysis.crysisjmsf");
	GetIGameFramework()->LoadGame(levelstart.c_str(), true, true);
	//**********
	IActor* pPlayerActor = this->GetIGameFramework()->GetIActorSystem()->GetActor(playerID);
	this->InitHUD(pPlayerActor);
	pLevelSystem->OnLoadingComplete(pLevel);
	GetMenu()->OnActionEvent(SActionEvent(eAE_inGame));	//reset the menu
	m_bReload = false;	//if m_bReload is true - load at levelstart

	//if paused - start game
	m_pFramework->PauseGame(false, true);

	GetISystem()->SerializingFile(0);
}

void CGame::RegisterGameObjectEvents()
{
	IGameObjectSystem* pGOS = m_pFramework->GetIGameObjectSystem();

	pGOS->RegisterEvent(eCGE_PostFreeze, "PostFreeze");
	pGOS->RegisterEvent(eCGE_PostShatter, "PostShatter");
	pGOS->RegisterEvent(eCGE_OnShoot, "OnShoot");
	pGOS->RegisterEvent(eCGE_Recoil, "Recoil");
	pGOS->RegisterEvent(eCGE_BeginReloadLoop, "BeginReloadLoop");
	pGOS->RegisterEvent(eCGE_EndReloadLoop, "EndReloadLoop");
	pGOS->RegisterEvent(eCGE_ActorRevive, "ActorRevive");
	pGOS->RegisterEvent(eCGE_VehicleDestroyed, "VehicleDestroyed");
	pGOS->RegisterEvent(eCGE_TurnRagdoll, "TurnRagdoll");
	pGOS->RegisterEvent(eCGE_EnableFallAndPlay, "EnableFallAndPlay");
	pGOS->RegisterEvent(eCGE_DisableFallAndPlay, "DisableFallAndPlay");
	pGOS->RegisterEvent(eCGE_VehicleTransitionEnter, "VehicleTransitionEnter");
	pGOS->RegisterEvent(eCGE_VehicleTransitionExit, "VehicleTransitionExit");
	pGOS->RegisterEvent(eCGE_HUD_PDAMessage, "HUD_PDAMessage");
	pGOS->RegisterEvent(eCGE_HUD_TextMessage, "HUD_TextMessage");
	pGOS->RegisterEvent(eCGE_TextArea, "TextArea");
	pGOS->RegisterEvent(eCGE_HUD_Break, "HUD_Break");
	pGOS->RegisterEvent(eCGE_HUD_Reboot, "HUD_Reboot");
	pGOS->RegisterEvent(eCGE_InitiateAutoDestruction, "InitiateAutoDestruction");
	pGOS->RegisterEvent(eCGE_Event_Collapsing, "Event_Collapsing");
	pGOS->RegisterEvent(eCGE_Event_Collapsed, "Event_Collapsed");
	pGOS->RegisterEvent(eCGE_MultiplayerChatMessage, "MultiplayerChatMessage");
	pGOS->RegisterEvent(eCGE_ResetMovementController, "ResetMovementController");
	pGOS->RegisterEvent(eCGE_AnimateHands, "AnimateHands");
	pGOS->RegisterEvent(eCGE_Ragdoll, "Ragdoll");
	pGOS->RegisterEvent(eCGE_EnablePhysicalCollider, "EnablePhysicalCollider");
	pGOS->RegisterEvent(eCGE_DisablePhysicalCollider, "DisablePhysicalCollider");
	pGOS->RegisterEvent(eCGE_RebindAnimGraphInputs, "RebindAnimGraphInputs");
	pGOS->RegisterEvent(eCGE_OpenParachute, "OpenParachute");

}

void CGame::GetMemoryStatistics(ICrySizer* s)
{
	s->Add(*this);
	m_pWeaponSystem->GetMemoryStatistics(s);

	s->Add(*m_pScriptBindActor);
	s->Add(*m_pScriptBindItem);
	s->Add(*m_pScriptBindWeapon);
	s->Add(*m_pScriptBindGameRules);
	s->Add(*m_pScriptBindGame);
	s->Add(*m_pScriptBindHUD);

	SAFE_MENU_FUNC(GetMemoryStatistics(s));

	/* handled by actionmapmanager
	m_pDefaultAM->GetMemoryStatistics(s);
	m_pDefaultAM->GetMemoryStatistics(s);
	m_pDebugAM->GetMemoryStatistics(s);
	m_pMultiplayerAM->GetMemoryStatistics(s);
	m_pNoMoveAF->GetMemoryStatistics(s);
	m_pNoMouseAF->GetMemoryStatistics(s);
	m_pInVehicleSuitMenu->GetMemoryStatistics(s);
	*/

	s->Add(*m_pGameActions);

	m_pItemSharedParamsList->GetMemoryStatistics(s);

	if (m_pPlayerProfileManager)
		m_pPlayerProfileManager->GetMemoryStatistics(s);

	if (m_pHUD)
		m_pHUD->GetMemoryStatistics(s);
}

void CGame::OnClearPlayerIds()
{
	if (IActor* pClient = GetIGameFramework()->GetClientActor())
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pClient);
		if (pPlayer->GetNanoSuit())
			pPlayer->GetNanoSuit()->RemoveListener(GetHUD());
	}
}


const string& CGame::GetLastSaveGame(string& levelName)
{
	if (m_pPlayerProfileManager)
	{
		const char* userName = GetISystem()->GetUserName();
		IPlayerProfile* pProfile = m_pPlayerProfileManager->GetCurrentProfile(userName);
		if (pProfile)
		{
			ISaveGameEnumeratorPtr pSGE = pProfile->CreateSaveGameEnumerator();
			ISaveGameEnumerator::SGameDescription desc;
			time_t curLatestTime = (time_t)0;
			const char* lastSaveGame = "";
			const int nSaveGames = pSGE->GetCount();
			for (int i = 0; i < nSaveGames; ++i)
			{
				if (pSGE->GetDescription(i, desc))
				{
					if (desc.metaData.saveTime > curLatestTime)
					{
						lastSaveGame = desc.name;
						curLatestTime = desc.metaData.saveTime;
						levelName = desc.metaData.levelName;
					}
				}
			}
			m_lastSaveGame = lastSaveGame;
		}
	}

	return m_lastSaveGame;
}

ILINE void expandSeconds(int secs, int& days, int& hours, int& minutes, int& seconds)
{
	days = secs / 86400;
	secs -= days * 86400;
	hours = secs / 3600;
	secs -= hours * 3600;
	minutes = secs / 60;
	seconds = secs - minutes * 60;
	hours += days * 24;
	days = 0;
}

void secondsToString(int secs, string& outString)
{
	int d, h, m, s;
	expandSeconds(secs, d, h, m, s);
	if (h > 0)
		outString.Format("%02dh_%02dm_%02ds", h, m, s);
	else
		outString.Format("%02dm_%02ds", m, s);
}

const char* CGame::CreateSaveGameName()
{
	//design wants to have different, more readable names for the savegames generated
	char buffer[16];
	int id = 0;

	//saves a running savegame id which is displayed with the savegame name
	if (IPlayerProfileManager* m_pPlayerProfileManager = gEnv->pGame->GetIGameFramework()->GetIPlayerProfileManager())
	{
		const char* user = m_pPlayerProfileManager->GetCurrentUser();
		if (IPlayerProfile* pProfile = m_pPlayerProfileManager->GetCurrentProfile(user))
		{
			pProfile->GetAttribute("Singleplayer.SaveRunningID", id);
			pProfile->SetAttribute("Singleplayer.SaveRunningID", id + 1);
			IPlayerProfileManager::EProfileOperationResult result;
			m_pPlayerProfileManager->SaveProfile(user, result);
		}
	}

	_itoa(id, buffer, 10);
	m_newSaveGame.clear();
	if (id < 10)
		m_newSaveGame += "0";
	m_newSaveGame += buffer;
	m_newSaveGame += "_";

	const char* levelName = GetIGameFramework()->GetLevelName();
	const char* mappedName = GetMappedLevelName(levelName);
	m_newSaveGame += mappedName;

	m_newSaveGame += "_";
	string timeString;
	secondsToString(m_pSPAnalyst->GetTimePlayed(), timeString);
	m_newSaveGame += timeString;

	SModInfo info;
	if (GetIGameFramework()->GetModInfo(&info))
	{
		m_newSaveGame += "_";
		m_newSaveGame += info.m_name;
	}

	m_newSaveGame += ".CRYSISJMSF";

	return m_newSaveGame.c_str();
}

const char* CGame::GetMappedLevelName(const char* levelName) const
{
	TLevelMapMap::const_iterator iter = m_mapNames.find(CONST_TEMP_STRING(levelName));
	return (iter == m_mapNames.end()) ? levelName : iter->second.c_str();
}
