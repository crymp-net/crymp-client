/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
	G02 HUD using Flash player
	Code specific to G02 should go here
	For code needed by all games, see CHUDCommon


-------------------------------------------------------------------------
History:
- 07:11:2005: Created by Julien Darre
- 01:02:2006: Modified by Jan Müller
- 22:02:2006: Refactored for G04 by Matthew Jack
- 2007: Refactored by Jan Müller

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryCore/StlUtils.h"
#include <ctype.h>

#include "CryGame/Game.h"
#include "CryGame/GameActions.h"
#include "CryGame/GameCVars.h"
#include "CryGame/MPTutorial.h"

#include "HUD.h"
#include "HUDObituary.h"
#include "HUDRadar.h"
#include "HUDScore.h"
#include "HUDTextArea.h"
#include "HUDTextChat.h"

#include "GameFlashAnimation.h"
#include "GameFlashLogic.h"
#include "CryGame/Menus/FlashMenuObject.h"
#include "CryCommon/CryAction/IPlayerProfiles.h"

#include "CryCommon/CryAction/IUIDraw.h"
#include "CryCommon/CrySoundSystem/ISound.h"
#include "CryGame/Actors/Player/IPlayerInput.h"
#include "CryCommon/CryAction/IWorldQuery.h"
#include "CryCommon/CryInput/IInput.h"
#include "CryCommon/CryAction/IMaterialEffects.h"
#include "CryCommon/CryAction/ISaveGame.h"
#include "CryCommon/CryAction/ILoadGame.h"
#include "CryCommon/CrySystem/ICryPak.h"
#include "CryCommon/CryMovie/IMovieSystem.h"

#include "CryGame/GameRules.h"
#include "CryGame/Items/Item.h"
#include "CryGame/Items/Weapons/Weapon.h"
#include "CryGame/Items/Weapons/OffHand.h"

#include "HUDVehicleInterface.h"
#include "HUDPowerStruggle.h"
#include "HUDTeamInstantAction.h"
#include "HUDScopes.h"
#include "HUDCrosshair.h"
#include "HUDTagNames.h"
#include "HUDSilhouettes.h"

#include "CryGame/Menus/OptionsManager.h"
#include "CryGame/Items/Weapons/WeaponSystem.h"
#include "CryGame/Radio.h"

#include "CryMP/Client/Client.h"
#include "CryMP/Client/ServerConnector.h"
#include "Library/StringTools.h"

static const float NIGHT_VISION_ENERGY = 30.0f;

//-----------------------------------------------------------------------------------------------------

#undef HUD_CALL_LISTENERS
#define HUD_CALL_LISTENERS(func) \
{ \
	if (m_hudListeners.empty() == false) \
	{ \
		m_hudTempListeners = m_hudListeners; \
		for (std::vector<IHUDListener*>::iterator tmpIter = m_hudTempListeners.begin(); tmpIter != m_hudTempListeners.end(); ++tmpIter) \
		   (*tmpIter)->func; \
	} \
}

//-----------------------------------------------------------------------------------------------------
void CHUD::OnSubtitleCVarChanged(ICVar* pCVar)
{
	int mode = pCVar->GetIVal();
	HUDSubtitleMode eMode = mode == 1 ? eHSM_All : (mode == 2 ? eHSM_CutSceneOnly : eHSM_Off);
	SAFE_HUD_FUNC(SetSubtitleMode(eMode));
}

//-----------------------------------------------------------------------------------------------------
void CHUD::OnCrosshairCVarChanged(ICVar* pCVar)
{
	SAFE_HUD_FUNC(m_pHUDCrosshair->SetCrosshair(pCVar->GetIVal()));
}

//-----------------------------------------------------------------------------------------------------
void CHUD::OnSubtitlePanoramicHeightCVarChanged(ICVar* pCVar)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD)
	{
		if (pHUD->m_cineState == eHCS_Fading)
			SAFE_HUD_FUNC(FadeCinematicBars(pCVar->GetIVal()));
	}
}

//-----------------------------------------------------------------------------------------------------

CHUD::CHUD()
{
	CFlashMenuObject::GetFlashMenuObject()->SetColorChanged();
	// CHUDCommon constructor runs first
	m_pHUDRadar = NULL;
	m_pHUDScore = NULL;
	m_pHUDTextChat = NULL;
	m_pRenderer = NULL;
	m_pUIDraw = NULL;
	m_pHUDVehicleInterface = NULL;
	m_pHUDPowerStruggle = NULL;
	m_pHUDScopes = NULL;
	m_pHUDCrosshair = NULL;
	m_pHUDTagNames = NULL;
	m_pHUDSilhouettes = NULL;

	m_forceScores = false;

	m_bLaunchWS = false;
	m_bIgnoreMiddleClick = true;
	m_pModalHUD = NULL;
	m_pSwitchScoreboardHUD = NULL;
	m_bScoreboardCursor = false;

	m_iVoiceMode = 0;
	m_lastPlayerPPSet = -1;
	m_deathFxId = InvalidEffectId;

	m_pNanoSuit = NULL;
	m_eNanoSlotMax = NANOSLOT_ARMOR;
	m_bFirstFrame = true;
	m_bAutosnap = false;
	m_bHideCrosshair = false;
	m_fLastSoundPlayedCritical = 0;
	m_bInMenu = false;
	m_bDestroyInitializePending = false;

	m_friendlyTrackerStatus = 0;
	m_hostileTrackerStatus = 0;

	m_bThirdPerson = false;
	m_bNightVisionActive = false;
	m_bMiniMapZooming = false;
	m_bTacLock = false;
	m_missionObjectiveNumEntries = 0;
	m_missionObjectiveValues.clear();
	m_bAirStrikeAvailable = false;
	m_fAirStrikeStarted = 0.0f;
	m_fNightVisionTimer = 0;
	m_fPlayerFallAndPlayTimer = 0.0f;
	m_bExclusiveListener = false;
	m_iBreakHUD = 0;
	m_iPlayerOwnedVehicle = 0;
	m_iOnScreenObjective = 0;
	m_bShowAllOnScreenObjectives = (g_pGameCVars && g_pGameCVars->hud_showAllObjectives) ? true : false;
	m_fMiddleTextLineTimeout = 0.0f;
	m_fOverlayTextLineTimeout = 0.0f;
	m_fBigOverlayTextLineTimeout = 0.0f;
	m_bigOverlayTextColor = Col_White;
	m_bigOverlayTextX = 1024 / 2;
	m_bigOverlayTextY = 768 / 2;
	m_curFireMode = 0;

	m_fPlayerRespawnTimer = 0.0f;
	m_fLastPlayerRespawnEffect = 0.0f;
	m_bRespawningFromFakeDeath = false;
	m_fPlayerDeathTime = 0.0f;
	m_bNoMiniMap = false;
	m_hasTACWeapon = false;
	m_fNightVisionEnergy = 30.0f;
	m_lastSpawnUpdate = 0.0f;
	m_fBackPressedTime = 0.0f;
	m_fXINightVisionActivateTimer = -1.0f;

	m_changedSpawnGroup = false;

	m_uiWeapondID = 0;
	m_iProgressBar = 0;
	m_bProgressLocking = false;
	m_playerAmmo = m_playerRestAmmo = -1;
	m_playerClipSize = 0;

	m_fSuitChangeSoundTimer = 0.0f;
	m_fLastReboot = 0.0f;
	m_fRemainingReviveCycleTime = -1.0f;

	ResetQuickMenu();

	m_pSCAR = gEnv->pEntitySystem->GetClassRegistry()->FindClass("SCAR");
	m_pSCARTut = gEnv->pEntitySystem->GetClassRegistry()->FindClass("SCARTutorial");
	m_pFY71 = gEnv->pEntitySystem->GetClassRegistry()->FindClass("FY71");
	m_pSMG = gEnv->pEntitySystem->GetClassRegistry()->FindClass("SMG");
	m_pDSG1 = gEnv->pEntitySystem->GetClassRegistry()->FindClass("DSG1");
	m_pShotgun = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Shotgun");
	m_pLAW = gEnv->pEntitySystem->GetClassRegistry()->FindClass("LAW");
	m_pGauss = gEnv->pEntitySystem->GetClassRegistry()->FindClass("GaussRifle");
	m_pClaymore = gEnv->pEntitySystem->GetClassRegistry()->FindClass("claymoreexplosive");
	m_pAVMine = gEnv->pEntitySystem->GetClassRegistry()->FindClass("avexplosive");
	m_pC4 = gEnv->pEntitySystem->GetClassRegistry()->FindClass("c4explosive");


	m_fDefenseTimer = m_fStrengthTimer = m_fSpeedTimer = 0;

	XmlNodeRef statusMessages = GetISystem()->LoadXmlFile("Libs/UI/StatusMessages.xml");

	if (statusMessages != NULL)
	{
		for (int j = 0; j < statusMessages->getNumAttributes(); ++j)
		{
			const char* attrib, * value;
			if (statusMessages->getAttributeByIndex(j, &attrib, &value))
				m_statusMessagesMap[string(attrib)] = string(value);
		}
	}

	LoadWeaponsAccessories();

	if (gEnv->pSoundSystem)
	{
		// this does not iterate through all hud events correctly yet
		gEnv->pSoundSystem->Precache("Sounds/interface:suit:generic_beep", 0, FLAG_SOUND_PRECACHE_EVENT_DEFAULT);
	}

	m_entityTargetAutoaimId = 0;

	m_trackedProjectiles.clear();

	m_fAutosnapCursorRelativeX = 0.0f;
	m_fAutosnapCursorRelativeY = 0.0f;
	m_fAutosnapCursorControllerX = 0.0f;
	m_fAutosnapCursorControllerY = 0.0f;
	m_bOnCircle = false;

	m_bNoMouse = false;
	m_bNoMove = false;
	m_bSuitMenuFilter = false;

	//update current mission objectives
	m_missionObjectiveSystem.LoadLevelObjectives(true);
	m_fBattleStatus = 0;
	m_fBattleStatusDelay = 0;

	m_pDefaultFont = NULL;

	m_bSubtitlesNeedUpdate = false;
	m_hudSubTitleMode = eHSM_Off;
	m_cineState = eHCS_None;
	m_cineHideHUD = false;
	m_bCutscenePlaying = false;
	m_bStopCutsceneNextUpdate = false;
	m_bCutsceneAbortPressed = false;
	m_bCutsceneCanBeAborted = true;
	m_fCutsceneSkipTimer = 0.0f;

	//just a small enough value to have no restrictions at startup
	m_lastNonAssistedInput = -3600.0f;
	m_hitAssistanceAvailable = false;
	m_quietMode = false;

	m_buyMenuKeyLog.Clear();

	m_currentGameRules = EHUD_SINGLEPLAYER;

	m_prevSpectatorMode = -1;
	m_prevSpectatorHealth = -1;
	m_prevSpectatorTarget = 0;

	//the hud exists as long as the game exists
	gEnv->pGame->GetIGameFramework()->RegisterListener(this, "hud", FRAMEWORKLISTENERPRIORITY_HUD);

	//CryMP: Record already placed explosives (fixes missing icons on explosives)
	IEntitySystem* pES = gEnv->pEntitySystem;
	IEntityItPtr pIt = pES->GetEntityIterator();
	while (!pIt->IsEnd())
	{
		if (IEntity* pEnt = pIt->Next())
		{
			if (pEnt->GetClass() == CItem::sClaymoreExplosiveClass
				|| pEnt->GetClass() == CItem::sAVExplosiveClass
				|| pEnt->GetClass() == CItem::sC4ExplosiveClass)
			{
				RecordExplosivePlaced(pEnt->GetId());
			}
		}
	}

}

//-----------------------------------------------------------------------------------------------------

CHUD::~CHUD()
{
	ShowDeathFX(0);

	if (m_bCutscenePlaying)
	{
		// we may not get the callback if moviesystem is reset after CHUD is destroyed
		// also, I don't want to call gEnv->pMovieSystem->StopAllCutscenes here
		// because it will call us back and call into our virtual function OnEndCutscene...
		g_pGameActions->FilterCutscene()->Enable(false);
		g_pGameActions->FilterCutsceneNoPlayer()->Enable(false);
		g_pGameActions->FilterInVehicleSuitMenu()->Enable(false);
	}

	//stop looping sounds
	for (int i = (int)ESound_Hud_First + 1; i < (int)ESound_Hud_Last;++i)
		PlaySound((ESound)i, false);

	HUD_CALL_LISTENERS(HUDDestroyed());

	IGameFramework* pGF = gEnv->pGame->GetIGameFramework();
	pGF->GetIItemSystem()->UnregisterListener(this);
	pGF->GetIItemSystem()->GetIEquipmentManager()->UnregisterListener(this);
	pGF->UnregisterListener(this);
	pGF->GetIViewSystem()->RemoveListener(this);
	CPlayer* pPlayer = static_cast<CPlayer*>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if (pPlayer)
	{
		pPlayer->UnregisterPlayerEventListener(this);
		if (CNanoSuit* pSuit = pPlayer->GetNanoSuit())
			pSuit->RemoveListener(this);
	}

	CGameRules* pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		pGameRules->RemoveGameRulesListener(this);
	}

	ISubtitleManager* pSubtitleManager = pGF->GetISubtitleManager();
	if (pSubtitleManager != 0)
	{
		pSubtitleManager->SetHandler(0);
	}

	OnSetActorItem(NULL, NULL);

	// call OnHUDDestroyed on hud objects. we own them, so delete afterwards
	for (CHUDObject *pObject : m_hudObjectsList)
	{
		pObject->OnHUDToBeDestroyed();
		delete pObject;
	}

	m_hudObjectsList.clear();

	// call OnHUDDestroyed on external hud objects. we don't own them, so don't delete
	for (CHUDObject *pObject : m_externalHUDObjectList)
	{
		pObject->OnHUDToBeDestroyed();
	}

	PlayerIdSet(0);	//unregister from game / player

	SAFE_DELETE(m_pHUDVehicleInterface);
	SAFE_DELETE(m_pHUDScopes);
	SAFE_DELETE(m_pHUDTagNames);
	SAFE_DELETE(m_pHUDSilhouettes);
	if (gEnv->pInput) gEnv->pInput->RemoveEventListener(this);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::MP_ResetBegin()
{
	if (m_pHUDPowerStruggle)
		m_pHUDPowerStruggle->Reset();
	if (m_pHUDTeamInstantAction)
		m_pHUDTeamInstantAction->Reset();
	//if (m_pHUDInstantAction)
	//	m_pHUDInstantAction->Reset();

	// Close everything that could be opened at this time

	OnAction(g_pGame->Actions().hud_hide_multiplayer_scoreboard, eIS_Released, 1);
	OnAction(g_pGame->Actions().hud_suit_menu, eIS_Released, 1);

	if (m_pClientActor)
	{
		m_pHUDVehicleInterface->OnExitVehicle(m_pClientActor);
	}

	if (m_pModalHUD == &m_animWeaponAccessories)
	{
		ShowWeaponAccessories(false);
	}

	// Remind which action filters were enabled (they are disabled by gamerules on restart)
	m_bNoMouse = g_pGameActions->FilterNoMouse()->Enabled();
	m_bNoMove = g_pGameActions->FilterNoMove()->Enabled();
	m_bSuitMenuFilter = g_pGameActions->FilterSuitMenu()->Enabled();
}

//-----------------------------------------------------------------------------------------------------

void CHUD::MP_ResetEnd()
{
	g_pGameActions->FilterNoMouse()->Enable(m_bNoMouse);
	g_pGameActions->FilterNoMove()->Enable(m_bNoMove);
	g_pGameActions->FilterSuitMenu()->Enable(m_bSuitMenuFilter);

	//resets most of the hud
	ResetPostSerElements();

	//CryMP for Radar after sv_restart
	ILevelSystem* pLevelSystem = g_pGame->GetIGameFramework()->GetILevelSystem();
	ILevel* pLevel = pLevelSystem->GetCurrentLevel();
	if (pLevel)
	{
		OnLoadingComplete(pLevel);
	}
}

//-----------------------------------------------------------------------------------------------------

CWeapon* CHUD::GetCurrentWeapon()
{
	if (IItem* pItem = m_pItemSystem->GetItem(m_uiWeapondID))
	{
		if (IWeapon* pWeapon = pItem->GetIWeapon())
		{
			return (CWeapon*)pWeapon;
		}
	}

	return 0;
}

void OnChangeChatResolution(ICVar* p)
{
	const int mode = p->GetIVal();

	if (g_pGame->GetHUD())
	{
		g_pGame->GetHUD()->EnableChatGfx(mode > 0);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::EnableChatGfx(bool highResolution)
{
	if (m_animChat.IsLoaded())
	{
		m_animChat.Unload();
	}

	const char* path = highResolution ? "Libs/UI/HUD_ChatSystem_HR.gfx" : "Libs/UI/HUD_ChatSystem.gfx";
	m_animChat.Load(path, eFD_Left);
	if (m_pHUDTextChat)
		m_pHUDTextChat->Init(&m_animChat);
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::Init(IActor* pActor)
{
	//no HUD without client..
	m_pClientActor = static_cast<CPlayer*>(pActor);
	m_pCurrentSpecTarget = 0;
	m_pGameRules = g_pGame->GetGameRules();
	m_pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();

	PlayerIdSet(pActor->GetEntityId());

	m_pRenderer = gEnv->pRenderer;
	m_pUIDraw = gEnv->pGame->GetIGameFramework()->GetIUIDraw();
	m_pDefaultFont = gEnv->pCryFont->GetFont("default");
	assert(m_pDefaultFont);

	bool loadEverything = gEnv->pCryPak->GetLvlResStatus();

	IScriptSystem* pScriptSystem = gEnv->pScriptSystem;

	m_pHUDScopes = new CHUDScopes(this);
	m_pHUDRadar = new CHUDRadar(this);
	m_pHUDObituary = new CHUDObituary;
	m_pHUDTextArea = new CHUDTextArea;
	m_pHUDTextArea->SetFadeTime(2.0f);
	m_pHUDTextArea->SetPos(Vec2(200.0f, 450.0f));

	m_pHUDCrosshair = new CHUDCrosshair(this);
	m_pHUDTagNames = new CHUDTagNames(this);
	m_pHUDSilhouettes = new CHUDSilhouettes;

	if (gEnv->bMultiplayer)
	{
		m_pHUDTextChat = new CHUDTextChat(this);
		m_pHUDScore = new CHUDScore(this);
		m_hudObjectsList.push_back(m_pHUDTextChat);
		m_hudObjectsList.push_back(m_pHUDScore);
	}

	m_hudObjectsList.push_back(m_pHUDRadar);
	m_hudObjectsList.push_back(m_pHUDObituary);
	m_hudObjectsList.push_back(m_pHUDTextArea);
	m_hudObjectsList.push_back(m_pHUDCrosshair);

	if (gEnv->bMultiplayer || loadEverything)
	{
		m_animKillLog.Load("Libs/UI/HUD_KillLog.gfx", eFD_Left, eFAF_Visible);
	}

	m_animPlayerStats.Load("Libs/UI/HUD_AmmoHealthEnergySuit.gfx", eFD_Right, eFAF_Visible | eFAF_ThisHandler);
	m_animAmmoPickup.Load("Libs/UI/HUD_AmmoPickup.gfx", eFD_Right, eFAF_Visible);
	m_animFriendlyProjectileTracker.Load("Libs/UI/HUD_GrenadeDetect_Friendly.gfx", eFD_Center, eFAF_Visible);
	if (!m_animFriendlyProjectileTracker.IsLoaded()) //asset missing so far ..
		m_animFriendlyProjectileTracker.Load("Libs/UI/HUD_GrenadeDetect.gfx", eFD_Center, eFAF_Visible);
	m_animHostileProjectileTracker.Load("Libs/UI/HUD_GrenadeDetect.gfx", eFD_Center, eFAF_Visible);
	m_animMissionObjective.Load("Libs/UI/HUD_MissionObjective_Icon.gfx", eFD_Center, eFAF_Visible);
	m_animQuickMenu.Load("Libs/UI/HUD_QuickMenu.gfx");
	m_animRadarCompassStealth.Load("Libs/UI/HUD_RadarCompassStealth.gfx", eFD_Left, eFAF_Visible);

	m_animNetworkConnection.Load("Libs/UI/HUD_Network_Icon.gfx", eFD_Center, eFAF_ThisHandler);

	m_pHUDRadar->SetFlashRadar(&m_animRadarCompassStealth);
	if (gEnv->bMultiplayer)
		m_animHexIcons.Load("Libs/UI/HUD_HexIcons.gfx", eFD_Left, eFAF_Visible);
	else
		m_animHexIcons.Load("Libs/UI/HUD_SP_HexIcons.gfx", eFD_Left, eFAF_Visible);

	m_animTacLock.Load("Libs/UI/HUD_Tac_Lock.gfx", eFD_Center, eFAF_ThisHandler);
	m_animWarningMessages.Load("Libs/UI/HUD_ErrorMessages.gfx", eFD_Center, eFAF_ThisHandler | eFAF_ManualRender);
	m_animOverlayMessages.Load("Libs/UI/HUD_OverlayMessage.gfx", eFD_Center, eFAF_ThisHandler | eFAF_ManualRender);
	m_animBigOverlayMessages.Load("Libs/UI/HUD_OverlayMessageFading.gfx", eFD_Center, eFAF_ThisHandler | eFAF_ManualRender);
	m_animHUDCornerLeft.Load("Libs/UI/HUD_Corner_Left.gfx", eFD_Left, eFAF_Visible);
	m_animHUDCornerRight.Load("Libs/UI/HUD_Corner_Right.gfx", eFD_Right, eFAF_Visible);

	//load the map
	m_animWeaponSelection.Load("Libs/UI/HUD_WeaponSelection.gfx", eFD_Right, eFAF_Visible);
	m_animPDA.Load("Libs/UI/HUD_PDA_Map.gfx", eFD_Right, eFAF_ThisHandler);
	m_animDownloadEntities.Load("Libs/UI/HUD_DownloadEntities.gfx");

	//CryMP
	m_animHitIndicatorPlayer.Load("Libs/UI/HUD_HitIndicatorPlayer.gfx", eFD_Center, eFAF_Visible);
	m_animHitIndicatorVehicle.Load("Libs/UI/HUD_HitIndicatorVehicle.gfx", eFD_Center, eFAF_Visible);

	m_animTrackedRadioMessage.Load("Libs/UI/HUD_GrenadeDetect_Friendly.gfx", eFD_Center, eFAF_Visible);
	if (!m_animTrackedRadioMessage.IsLoaded()) //asset missing so far ..
		m_animTrackedRadioMessage.Load("Libs/UI/HUD_GrenadeDetect.gfx", eFD_Center, eFAF_Visible);

	// these are delay-loaded elsewhere!!!
	if (loadEverything)
	{
		m_animKillAreaWarning.Load("Libs/UI/HUD_Area_Warning.gfx", eFD_Center, true);
		m_animDeathMessage.Load("Libs/UI/HUD_KillEvents.gfx", eFD_Center, true);
		m_animGamepadConnected.Load("Libs/UI/HUD_GamePad_Stats.gfx");
		m_animBreakHUD.Load("Libs/UI/HUD_Lost.gfx", eFD_Center, eFAF_ManualRender | eFAF_Visible);
		m_animRebootHUD.Load("Libs/UI/HUD_Reboot.gfx", eFD_Center, eFAF_Visible | eFAF_ThisHandler);
	}
	else
	{
		m_animKillAreaWarning.Init("Libs/UI/HUD_Area_Warning.gfx", eFD_Center, true);
		m_animDeathMessage.Init("Libs/UI/HUD_KillEvents.gfx", eFD_Center, true);
		m_animGamepadConnected.Init("Libs/UI/HUD_GamePad_Stats.gfx");
		m_animBreakHUD.Init("Libs/UI/HUD_Lost.gfx", eFD_Center, eFAF_Visible);
		m_animRebootHUD.Init("Libs/UI/HUD_Reboot.gfx", eFD_Center, eFAF_Visible | eFAF_ThisHandler);
	}

	m_animWeaponAccessories.Load("Libs/UI/HUD_WeaponAccessories.gfx", eFD_Center, eFAF_ThisHandler);
	if (loadEverything)
	{
		if (!gEnv->bMultiplayer)
		{
			m_animCinematicBar.Load("Libs/UI/HUD_CineBar.gfx", eFD_Center, eFAF_ThisHandler | eFAF_ManualRender | eFAF_Visible);
		}
		m_animSubtitles.Load("Libs/UI/HUD_Subtitle.gfx", eFD_Center, eFAF_ThisHandler | eFAF_ManualRender | eFAF_Visible);
	}
	else
	{
		if (!gEnv->bMultiplayer)
		{
			m_animCinematicBar.Init("Libs/UI/HUD_CineBar.gfx", eFD_Center, eFAF_ThisHandler | eFAF_ManualRender | eFAF_Visible);
		}
		m_animSubtitles.Init("Libs/UI/HUD_Subtitle.gfx", eFD_Center, eFAF_ThisHandler | eFAF_ManualRender | eFAF_Visible);
	}

	m_pHUDRadar->SetFlashPDA(&m_animPDA);

	for (int i = 0; i < ESound_Hud_Last; ++i)
		m_soundIDs[i] = INVALID_SOUNDID;

	//reload mission objectives
	m_missionObjectiveSystem.LoadLevelObjectives(true);

	m_pItemSystem->RegisterListener(this);
	m_pItemSystem->GetIEquipmentManager()->RegisterListener(this);
	g_pGame->GetIGameFramework()->GetIViewSystem()->AddListener(this);

	// apply subtitle mode
	SetSubtitleMode((HUDSubtitleMode)g_pGameCVars->hud_subtitles);

	// in Editor mode, the LoadingComplete is not called because the HUD does not exist yet/not initialized yet
	if (gEnv->pSystem->IsEditor())
		m_pHUDRadar->OnLoadingComplete(0);

	m_pHUDVehicleInterface = new CHUDVehicleInterface(this, &m_animPlayerStats);

	GameRulesSet();

	if (gEnv->bMultiplayer)
	{
		m_pGameRules->AddGameRulesListener(this);

		if (m_currentGameRules == EHUD_POWERSTRUGGLE)
		{
			//CryMP: Let's load PowerStruggle only when needed..
			m_pHUDPowerStruggle = new CHUDPowerStruggle(this, m_pGameRules, &m_animBuyMenu, &m_animHexIcons);
			m_hudObjectsList.push_back(m_pHUDPowerStruggle);
		}
		else if (m_currentGameRules == EHUD_TEAMINSTANTACTION)
		{
			m_pHUDTeamInstantAction = new CHUDTeamInstantAction(this);
			m_hudObjectsList.push_back(m_pHUDTeamInstantAction);

			if (m_pHUDTeamInstantAction)
				m_pHUDTeamInstantAction->Show(true);
		}
		else if (m_currentGameRules == EHUD_INSTANTACTION)
		{
			//m_pHUDInstantAction = new CHUDInstantAction(this);
			//m_hudObjectsList.push_back(m_pHUDInstantAction);
		}

		if (m_pGameRules->GetTeamCount() > 1)
		{
			m_animTeamSelection.Load("Libs/UI/HUD_TeamSelection.gfx", eFD_Center, eFAF_ManualRender | eFAF_ThisHandler);
			m_animTeamSelection.GetFlashPlayer()->SetVisible(false);
		}
	}

	m_lastPlayerPPSet = -1;
	m_buyMenuKeyLog.Clear();

	//if wanted, load everything that will be loaded later on
	if (loadEverything)
	{
		m_pHUDVehicleInterface->LoadVehicleHUDs(true);
		m_pHUDScopes->LoadFlashFiles(true);
	}

	// This one is on top of others because it displays important
	// messages, so let's put it at the end of the rendering list
	m_animMessages.Load("Libs/UI/HUD_Messages.gfx");

	UpdateHUDElements();

	//firemodes
	m_hudFireModes["Single"] = 1;
	m_hudFireModes["Burst"] = 2;
	m_hudFireModes["Rapid"] = 3;
	m_hudFireModes["Automatic"] = 3;
	m_hudFireModes["GrenadeLauncher"] = 4;
	m_hudFireModes["Incendiary"] = 5;
	m_hudFireModes["Tac Sleep"] = 6;
	m_hudFireModes["LAW"] = 7;
	m_hudFireModes["infinity"] = 8;
	m_hudFireModes["Fists"] = 8;
	m_hudFireModes["Shotgun"] = 9;
	m_hudFireModes["Narrow"] = 10;
	m_hudFireModes["Spread"] = 9;
	m_hudFireModes["AVMine"] = 12;
	m_hudFireModes["Claymore"] = 25;
	m_hudFireModes["RepairKit"] = 13;
	m_hudFireModes["AlienMount"] = 14; //Moac, Moar
	m_hudFireModes["VehicleMOACMounted"] = 15;
	m_hudFireModes["VehicleMOARMounted"] = 15;
	m_hudFireModes["VehicleSingularity"] = 15;
	m_hudFireModes["VehicleGaussMounted"] = 15;
	m_hudFireModes["TACGun"] = 16;
	m_hudFireModes["C4"] = 17;
	m_hudFireModes["SingleInc"] = 18;
	m_hudFireModes["RapidInc"] = 19;
	m_hudFireModes["AvengerCannon"] = 22;
	m_hudFireModes["Asian50Cal"] = 22;
	m_hudFireModes["ShiTen"] = 24;
	m_hudFireModes["VehicleShiTenV2"] = 24;
	m_hudFireModes["VehicleUSMachinegun"] = 24;
	m_hudFireModes["Detonator"] = 17;

	//ammunitions (1 is default bullet)
	m_hudAmmunition["lightbullet"] = 2;
	m_hudAmmunition["shotgunshell"] = 3;
	m_hudAmmunition["scargrenade"] = 4;
	m_hudAmmunition["rocket"] = 5;
	m_hudAmmunition["explosivegrenade"] = 7;
	m_hudAmmunition["flashbang"] = 8;
	m_hudAmmunition["smokegrenade"] = 9;
	m_hudAmmunition["C4"] = 10;
	m_hudAmmunition["c4explosive"] = 10;
	m_hudAmmunition["empgrenade"] = 11;
	m_hudAmmunition["gaussbullet"] = 12;
	m_hudAmmunition["tacgunprojectile"] = 13;
	m_hudAmmunition["claymoreexplosive"] = 14;
	m_hudAmmunition["incendiarybullet"] = 15;
	m_hudAmmunition["tacbullet"] = 16;
	m_hudAmmunition["avexplosive"] = 17;

	if (ICVar* pChatResolutionCVar = gEnv->pConsole->GetCVar("mp_chatHighResolution"))
	{
		pChatResolutionCVar->SetOnChangeCallback(OnChangeChatResolution);
	}

	return true;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnAmmoChanged(CActor *pActor)
{
	if (pActor && pActor->IsClient() && IsBuyMenuActive() && GetPowerStruggleHUD())
	{
		GetPowerStruggleHUD()->UpdateBuyList(nullptr, false);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ShowBootSequence()
{
	if (m_pHUDScopes->m_animBinoculars.IsLoaded())
	{
		if (m_pHUDScopes->IsBinocularsShown())
		{
			m_pClientActor->SelectLastItem(false);
		}
		m_pHUDScopes->ShowBinoculars(false, false, true);
	}

	if (m_iBreakHUD)
		BreakHUD(0);

	for (CGameFlashAnimation *pAnim : m_gameFlashAnimationsList)
	{
		if (!(pAnim->GetFlags() & eFAF_ManualRender))
			pAnim->SetVariable("SkipSequence", SFlashVarValue(false));
	}

	SetHUDColor();
	m_animInitialize.Load("Libs/UI/HUD_Initialize.gfx", eFD_Center, eFAF_ThisHandler | eFAF_ManualRender);
	m_animInitialize.SetVisible(true);
	PlaySound(ESound_Reboot);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ShowDownloadSequence()
{
	m_animDownloadEntities.Invoke("startDownload");
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UpdateHUDElements()
{
	UpdateCrosshair();

	//Color
	bool bUpdate = CFlashMenuObject::GetFlashMenuObject()->ColorChanged();

	if (bUpdate)
	{
		SetHUDColor();
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetHUDColor()
{
	CFlashMenuObject::GetFlashMenuObject()->UpdateMenuColor();

	TGameFlashAnimationsList::const_iterator iter = m_gameFlashAnimationsList.begin();
	TGameFlashAnimationsList::const_iterator end = m_gameFlashAnimationsList.end();
	for (; iter != end; ++iter)
	{
		SetFlashColor(*iter);
	}

	if (m_pHUDTeamInstantAction)
		m_pHUDTeamInstantAction->SetHUDColor();
	//if (m_pHUDInstantAction)
	//	m_pHUDInstantAction->SetHUDColor();

	//necessary in new hud design only
	m_fHealth = -1.0f;
	UpdateHealth();

	EnergyChanged(m_pClientActor->GetNanoSuit()->GetSuitEnergy());
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetFlashColor(CGameFlashAnimation* pAnim)
{
	if (pAnim)
	{
		//re-setting colors
		pAnim->CheckedInvoke("setLineColor", SFlashVarValue(g_pGameCVars->hud_colorLine));
		pAnim->CheckedInvoke("setOverColor", SFlashVarValue(g_pGameCVars->hud_colorOver));
		pAnim->CheckedInvoke("setTextColor", SFlashVarValue(g_pGameCVars->hud_colorText));
		if (pAnim->IsAvailable("resetColor"))
			pAnim->Invoke("resetColor");
		else
			pAnim->CheckedInvoke("Root.gotoAndPlay", "restart");

		pAnim->GetFlashPlayer()->Advance(0.2f);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetTACWeapon(bool hasTACWeapon)
{
	m_hasTACWeapon = hasTACWeapon;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::PlayerIdSet(EntityId playerId)
{
	if (m_pClientActor && playerId != 0)
	{
		m_pNanoSuit = m_pClientActor->GetNanoSuit();
		assert(m_pNanoSuit); //the player requires to have a nanosuit!

		if (m_pNanoSuit)
		{
			m_fSuitEnergy = m_pNanoSuit->GetSuitEnergy();

			m_pClientActor->RegisterPlayerEventListener(this);
			m_pNanoSuit->AddListener(this);

			switch (m_pNanoSuit->GetMode())
			{
			case NANOMODE_DEFENSE:
				m_animPlayerStats.Invoke("setMode", "Armor");
				break;
			case NANOMODE_SPEED:
				m_animPlayerStats.Invoke("setMode", "Speed");
				break;
			case NANOMODE_STRENGTH:
				m_animPlayerStats.Invoke("setMode", "Strength");
				break;
			default:
				m_animPlayerStats.Invoke("setMode", "Cloak");
				break;
			}
		}

		if (gEnv->pInput) gEnv->pInput->AddEventListener(this);

		GetMissionObjectiveSystem().DeactivateObjectives(true); //this should remove all "old" objectives
	}
	else
	{
		m_pClientActor->UnregisterPlayerEventListener(this);
		if (CNanoSuit* pSuit = m_pClientActor->GetNanoSuit())
			pSuit->RemoveListener(this);

		if (gEnv->pInput) gEnv->pInput->RemoveEventListener(this);
	}
}

//-----------------------------------------------------------------------------------------------------

IActor* CHUD::GetSpectatorTarget()
{
	return m_pClientActor->GetSpectatorTargetActor();
}

//-----------------------------------------------------------------------------------------------------

void CHUD::GameRulesSet()
{
	EHUDGAMERULES gameRules = EHUD_SINGLEPLAYER;

	std::string name = m_pGameRules->GetEntity()->GetClass()->GetName();

	if (gEnv->bMultiplayer)
	{
		if (!stricmp(name.c_str(), "InstantAction"))
			gameRules = EHUD_INSTANTACTION;
		else if (!stricmp(name.c_str(), "PowerStruggle"))
			gameRules = EHUD_POWERSTRUGGLE;
		else if (!stricmp(name.c_str(), "TeamAction"))
			gameRules = EHUD_TEAMACTION;
		else if (!stricmp(name.c_str(), "TeamInstantAction"))
			gameRules = EHUD_TEAMINSTANTACTION;
	}

	if (m_currentGameRules != gameRules)//unload stuff
	{
		LoadGameRulesHUD(false);
		m_currentGameRules = gameRules;
	}

	LoadGameRulesHUD(true);
}

//-----------------------------------------------------------------------------------------------------
void CHUD::SwitchToModalHUD(CGameFlashAnimation* pModalHUD, bool bNeedMouse)
{
	if (pModalHUD)
	{
		if (bNeedMouse)
		{
			this->ShowMouseCursor(true);
		}
	}
	else
	{
		this->ShowMouseCursor(false);
	}
	m_pModalHUD = pModalHUD;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::Serialize(TSerialize ser)
{
	CHUDCommon::Serialize(ser);

	if (ser.IsReading())
	{
		//stop looping sounds
		for (int i = (int)ESound_Hud_First + 1; i < (int)ESound_Hud_Last;++i)
			PlaySound((ESound)i, false);
	}

	ser.Value("hudShow", m_bShow);
	ser.Value("hudBroken", m_iBreakHUD);
	ser.EnumValue("hudCineState", m_cineState, eHCS_None, eHCS_Fading);
	ser.Value("thirdPerson", m_bThirdPerson);
	ser.Value("hudGodMode", m_godMode);
	ser.Value("hudBattleStatus", m_fBattleStatus);
	ser.Value("hudBattleStatusDelay", m_fBattleStatusDelay);
	ser.Value("hudExclusiveInputListener", m_bExclusiveListener);
	ser.Value("hudAirStrikeAvailable", m_bAirStrikeAvailable);
	ser.Value("hudAirStrikeStarted", m_fAirStrikeStarted);
	ser.Value("hudAirStrikeTarget", m_iAirStrikeTarget);
	ser.Value("hudPlayerOwnedVehicle", m_iPlayerOwnedVehicle);
	ser.Value("hudShowAllOnScreenObjectives", m_bShowAllOnScreenObjectives);
	int iSize = m_possibleAirStrikeTargets.size();
	ser.Value("hudPossibleTargetsLength", iSize);
	ser.Value("hudGodDeaths", m_iDeaths);

	ser.Value("playerRespawnTimer", m_fPlayerRespawnTimer);
	ser.Value("playerFakeDeath", m_bRespawningFromFakeDeath);
	bool nightVisionActive = m_bNightVisionActive;
	ser.Value("nightVisionActive", nightVisionActive);
	ser.Value("mainObjective", m_currentMainObjective);
	ser.Value("Goal", m_currentGoal);
	ser.Value("NightVisionEnergy", m_fNightVisionEnergy);
	ser.Value("hudWeaponId", m_uiWeapondID);
	ser.Value("m_iProgressBar", m_iProgressBar);
	ser.Value("m_curFireMode", m_curFireMode);

	ser.Value("trackedProjectiles", m_trackedProjectiles);

	m_pHUDScopes->Serialize(ser);

	if (m_iProgressBar)
	{
		ser.Value("m_iProgressBarX", m_iProgressBarX);
		ser.Value("m_iProgressBarY", m_iProgressBarY);
		ser.Value("m_sProgressBarText", m_sProgressBarText);
		ser.Value("m_bProgressBarTextPos", m_bProgressBarTextPos);
		ser.Value("m_bProgressLocking", m_bProgressLocking);
	}
	else if (ser.IsReading())
	{
		m_iProgressBarX = m_iProgressBarY = 0;
		m_sProgressBarText.clear();
		m_bProgressBarTextPos = false;
		m_bProgressLocking = false;
	}

	//only for vector serialize
	char szID[64];
	if (ser.IsReading())
	{
		for (int i = 0; i < iSize; i++)
		{
			sprintf(szID, "hudPossibleTargetsID%d", i);
			EntityId id = 0;
			ser.Value(szID, id);
			m_possibleAirStrikeTargets.push_back(id);
		}
	}
	else
	{
		for (int i = 0; i < iSize; i++)
		{
			sprintf(szID, "hudPossibleTargetsID%d", i);
			ser.Value(szID, m_possibleAirStrikeTargets[i]);
		}
	}

	m_pHUDVehicleInterface->Serialize(ser);
	m_pHUDSilhouettes->Serialize(ser);

	if (ser.IsReading())
	{
		m_hudObjectivesList.clear();
		UpdateObjective(NULL);

		if (m_currentGoal.size())
			SetMainObjective(m_currentGoal.c_str(), true);
		if (m_currentMainObjective.size())
			SetMainObjective(m_currentMainObjective.c_str(), false);

		//remove the weapon accessory screen (can show up after loading if not removed explicitly)
		if (GetModalHUD() == &m_animWeaponAccessories)
			OnAction(g_pGame->Actions().hud_suit_menu, 1, 1.0f);

		SetAirStrikeEnabled(m_bAirStrikeAvailable);

		BreakHUD(m_iBreakHUD);
		m_pHUDCrosshair->GetFlashAnim()->Reload(true); // only to make sure damage indicator won't be shown at next weapon switch
		m_pHUDCrosshair->Reset(); //reset crosshair
		m_pHUDCrosshair->SetUsability(0); //sometimes the scripts don't update the usability after loading from mounted gun for example

		m_bMiniMapZooming = false;

		SetGODMode(m_godMode, true);	//reset god mode - only necessary when reading

		// QuickMenu is a push holder thing, when we load, we don't hold
		// the middle click so we have to disable this menu !

		// This will show/hide vehicles/scopes interfaces
		if (m_pClientActor->GetLinkedVehicle())
		{
			if (m_pHUDVehicleInterface->IsParachute())
				OnEnterVehicle(m_pClientActor, "Parachute", "Closed", m_bThirdPerson);
			else
				m_pHUDVehicleInterface->OnEnterVehicle(m_pClientActor);
		}
		OnToggleThirdPerson(m_pClientActor, m_bThirdPerson); //problematic, because it doesn't know whether there is actually an vehicle

		// Reset cursor
		m_bScoreboardCursor = false;
		m_pSwitchScoreboardHUD = NULL;
		this->ShowMouseCursor(false);
		if (IPlayerInput* pInput = m_pClientActor->GetPlayerInput())
			pInput->DisableXI(false);
		g_pGameActions->FilterNoMouse()->Enable(false);
		g_pGameActions->FilterSuitMenu()->Enable(false);

		m_fNightVisionTimer = 0.0f; //reset timer
		if (m_bNightVisionActive) //turn always off (safest way)
			OnAction(g_pGame->Actions().hud_night_vision, 1, 1.0f);
		m_bNightVisionActive = nightVisionActive;

		m_animWeaponAccessories.ReInitVariables();
	}

	//**********************************Radar serialization
	m_pHUDRadar->Serialize(ser);

	//weapon setup should be serialized too?!

	//serialize objectives after removing them from the hud
	m_missionObjectiveSystem.Serialize(ser);
	/*if(ser.GetSerializationTarget() != eST_Network && ser.IsReading())
	{
		m_onScreenMessageBuffer.clear();	//clear old/serialization messages
		m_onScreenMessage.clear();
	}*/

	ser.Value("m_bigOverlayText", m_bigOverlayText);
	ser.Value("m_fBigOverlayTextLineTimeout", m_fBigOverlayTextLineTimeout);

	// color needs some special serialization (ColorF -> Vec3, Vec3 -> ColorF)
	Vec3 tmpColor(m_bigOverlayTextColor.r, m_bigOverlayTextColor.g, m_bigOverlayTextColor.b);
	ser.Value("m_bigOverlayTextColor", tmpColor);
	if (ser.IsReading())
		m_bigOverlayTextColor = tmpColor;
	ser.Value("m_bigOverlayTextX", m_bigOverlayTextX);
	ser.Value("m_bigOverlayTextY", m_bigOverlayTextY);

	if (m_externalHUDObjectList.empty() == false)
	{
		for (THUDObjectsList::iterator iter = m_externalHUDObjectList.begin(); iter != m_externalHUDObjectList.end(); ++iter)
		{
			(*iter)->Serialize(ser);
		}
	}
}
//-----------------------------------------------------------------------------------------------------

void CHUD::PostSerialize()
{
	m_animMessages.Reload(true); //reload message queue to delete old messages (reset unfortunately didn't work)

	ResetPostSerElements();
	m_delayedMessage.resize(0);
	DisplayOverlayFlashMessage("@game_loaded");
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ResetPostSerElements()
{
	if (m_animPDA.GetVisible())
		ShowPDA(false);
	SwitchToModalHUD(NULL, false);

	m_pHUDCrosshair->Reset();

	if (m_bNightVisionActive)
	{
		m_bNightVisionActive = false;
		m_fNightVisionTimer = 0.0f; //reset timer
		OnAction(g_pGame->Actions().hud_night_vision, 1, 1.0f);	//turn on
	}

	if (m_iProgressBar)
		ShowProgress(m_iProgressBar, true, m_iProgressBarX, m_iProgressBarY, m_sProgressBarText, m_bProgressBarTextPos, m_bProgressLocking);
	else
	{
		m_animProgress.Unload();
		m_animProgressLocking.Unload();
	}

	if (CHUDVehicleInterface::EVehicleHud hudType = m_pHUDVehicleInterface->GetHUDType())
	{
		m_pHUDVehicleInterface->UpdateVehicleHUDDisplay();
		m_pHUDVehicleInterface->UpdateDamages(m_pHUDVehicleInterface->GetHUDType(), m_pHUDVehicleInterface->GetVehicle());
		m_pHUDVehicleInterface->UpdateSeats();
		m_pHUDVehicleInterface->ShowVehicleInterface(hudType);
	}

	m_bFirstFrame = true;
	m_fHealth = 0.0f;
	m_fPlayerDeathTime = 0.0f;
	m_pHUDVehicleInterface->ChooseVehicleHUD(m_pHUDVehicleInterface->GetVehicle());
	UpdateHealth();
	m_fMiddleTextLineTimeout = 0.0f; //has to be reset sometimes after loading
	m_fOverlayTextLineTimeout = 0.0f; //has to be reset sometimes after loading
	DisplayFlashMessage("", 2);

	m_animWarningMessages.SetVisible(false);
	m_subtitleEntries.clear();
	m_bSubtitlesNeedUpdate = true;
	m_playerAmmo = m_playerRestAmmo = -1;
	m_playerClipSize = 0;

	//force firemode update
	m_curFireMode = -1;
	SetFireMode(NULL, NULL);

	float duration = m_fBigOverlayTextLineTimeout - gEnv->pTimer->GetFrameStartTime().GetSeconds();
	if (duration < 0.0f)
	{
		m_bigOverlayText.clear();
		duration = 0.0f;
		m_fBigOverlayTextLineTimeout = 0.0f;
	}
	DisplayBigOverlayFlashMessage(m_bigOverlayText.c_str(), duration, m_bigOverlayTextX, m_bigOverlayTextY, m_bigOverlayTextColor);
	m_cineHideHUD = false;
	m_cineState = eHCS_None;

	ShowDeathFX(-1); //stops running deathFX
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnSetActorItem(IActor* pActor, IItem* pItem)
{
	if (pActor && !pActor->IsClient())
	{
		return;
	}

	IItem* pCurrentItem = NULL;
	if (pActor && (pCurrentItem = pActor->GetCurrentItem()) && pCurrentItem != pItem)
	{
		// If new item is different than the current item and is not a weapon,
		// it's certainly an attachment like a grenade launcher
		// In that case, we don't unregister from the weapon, we just ignore
	}
	else
	{
		if (m_uiWeapondID)
		{
			// We can't use GetCurrentWeapon()->RemoveEventListener because sometimes the IItem is destroyed before
			// we reach that point. So let's try to retrieve the weapon by its ID with the IItemSystem
			IItem* pLocalItem = m_pItemSystem->GetItem(m_uiWeapondID);
			if (pLocalItem)
			{
				IWeapon* pWeapon = pLocalItem->GetIWeapon();
				if (pWeapon)
				{
					pWeapon->RemoveEventListener(this);
				}
			}
			m_uiWeapondID = 0;
		}
		if (pItem)
		{
			if (pItem->GetIWeapon())
			{
				m_uiWeapondID = pItem->GetEntity()->GetId();
				GetCurrentWeapon()->AddEventListener(this, __FUNCTION__);

				const char* curClass = nullptr;

				IEntityClass* pItemClass = pItem->GetEntity()->GetClass();
				if (pItemClass == CItem::sDetonatorClass)
				{
					curClass = "C4";
				}
				else
				{
					curClass = pItemClass->GetName();
				}
				const char* curCategory = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItemCategory(curClass);

				if (curCategory && curClass)
				{
					ShowInventoryOverview(curCategory, curClass);
				}
			}
		}
	}

	//notify the buymenu of the item change
	if (m_pHUDPowerStruggle)
		m_pHUDPowerStruggle->PopulateBuyList(false);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnPickupAttachment(IActor* pActor, IItem* pItem)
{
	if (!g_pGameCVars->mp_attachBoughtEquipment)
		return;

	//CryMP: Auto-attach bought equipment as in Wars

	CGameRules* pGameRules = g_pGame->GetGameRules();
	if (pItem && m_pHUDPowerStruggle && pGameRules)
	{
		bool bIsInList = false;
		for (std::list<string>::iterator iter = m_listBoughtItems.begin(); iter != m_listBoughtItems.end(); ++iter)
		{
			if (*iter == pItem->GetEntity()->GetClass()->GetName())
			{
				bIsInList = true;
				break;
			}
		}

		CWeapon* pItemWeapon = static_cast<CWeapon*>(pItem->GetIWeapon());
		if (pItemWeapon)
		{
			//we got a weapon
		}
		else if (m_uiWeapondID && (bIsInList || IsBuyMenuActive()))
		{
			//we got an accessory
			IItem* pLastWeaponItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_uiWeapondID);
			CWeapon* pLastWeapon = pLastWeaponItem ? static_cast<CWeapon*>(pLastWeaponItem) : nullptr;

			const ItemString szName = (ItemString)pItem->GetEntity()->GetClass()->GetName();

			if (pLastWeapon && pLastWeapon->GetAccessory(szName) == nullptr)
			{
				pLastWeapon->SwitchAccessory(szName);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnDropActorItem(IActor* pActor, IItem* pItem)
{
	//notify the buymenu of the item change
	if (m_pHUDPowerStruggle)
		m_pHUDPowerStruggle->PopulateBuyList();
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnSetActorAccessory(IActor* pActor, IItem* pItem)
{
	//notify the buymenu of the item change
	if (m_pHUDPowerStruggle)
		m_pHUDPowerStruggle->PopulateBuyList();
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnDropActorAccessory(IActor* pActor, IItem* pItem)
{
	//notify the buymenu of the item change
	if (m_pHUDPowerStruggle)
		m_pHUDPowerStruggle->PopulateBuyList();
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnStartTargetting(IWeapon* pWeapon)
{
	m_animTacLock.SetVisible(true);
	m_bTacLock = true;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnStopTargetting(IWeapon* pWeapon)
{
	m_animTacLock.SetVisible(false);
	m_bTacLock = false;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::HideInventoryOverview()
{
	m_animWeaponSelection.Invoke("clearLogs");
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ShowInventoryOverview(const char* curCategory, const char* curItem, bool grenades)
{
	if (!curCategory || !curItem)
		return;

	if (!m_pClientActor)
		return;

	IInventory* pInventory = m_pClientActor->GetInventory();
	if (!pInventory)
		return;

	if (m_pModalHUD == &m_animBuyMenu || m_pModalHUD == &m_animPDA)
		return;

	HideInventoryOverview();

	std::vector<IEntityClass*> classes;

	if (grenades)
	{
		COffHand* pOffHand = static_cast<COffHand*>(m_pClientActor->GetWeaponByClass(CItem::sOffHandClass));
		if (pOffHand)
		{
			std::vector<string> grenadeNames;
			pOffHand->GetAvailableGrenades(grenadeNames);
			for (const auto& grenadeName : grenadeNames)
			{
				SFlashVarValue args[2] = { grenadeName.c_str(), grenadeName == curItem };
				m_animWeaponSelection.Invoke("addLog", args, 2);
			}
		}

		m_animPlayerStats.Invoke("switchGrenades");
	}
	else
	{
		int count = pInventory->GetCount();
		for (int i = 0; i < count; ++i)
		{
			IEntity* pItemEntity = gEnv->pEntitySystem->GetEntity(pInventory->GetItem(i));
			IItem* pItemItem = m_pItemSystem->GetItem(pInventory->GetItem(i));
			if (pItemEntity && pItemEntity->GetClass() && pItemItem && pItemItem->CanSelect())
			{
				const char* className = pItemEntity->GetClass()->GetName();
				const char* categoryName = m_pItemSystem->GetItemCategory(className);

				if (!strcmp(categoryName, curCategory))
				{
					if (!stl::find(classes, pItemEntity->GetClass()))
					{
						SFlashVarValue args[2] = { className, !strcmp(className, curItem) };
						m_animWeaponSelection.Invoke("addLog", args, 2);
						classes.push_back(pItemEntity->GetClass());
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnLoadingStart(ILevelInfo* pLevel)
{
	if (g_pGameCVars->hud_enableAlienInterference)
		g_pGameCVars->hud_enableAlienInterference = 0;

	m_quietMode = true;

	m_pHUDCrosshair->GetFlashAnim()->Invoke("clearDamageDirection");
	m_pHUDCrosshair->GetFlashAnim()->GetFlashPlayer()->Advance(0.1f);
	/*m_pHUDScopes->ShowBinoculars(false);

	ShowKillAreaWarning(false, 0);
	BreakHUD(0);

	GetMissionObjectiveSystem().DeactivateObjectives(false); //deactivate old objectives
	m_animMessages.Invoke("reset");	//reset update texts

	if(m_pHUDVehicleInterface && m_pHUDVehicleInterface->GetHUDType() != CHUDVehicleInterface::EHUD_NONE)
		m_pHUDVehicleInterface->OnExitVehicle(NULL);

	m_currentGoal.clear();
	m_currentMainObjective.clear();

	//disable alien fear effect
	m_fSetAgressorIcon = 0.0f;
	m_animTargetter.SetVisible(false);
	m_bThirdPerson = false;

	m_uiWeapondID = 0;*/
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnLoadingComplete(ILevel* pLevel)
{
	m_quietMode = false;
	if (m_pHUDRadar)
		m_pHUDRadar->OnLoadingComplete(pLevel);
}

//-----------------------------------------------------------------------------------------------------

//the vehicle i paid for, was built
void CHUD::OnPlayerVehicleBuilt(EntityId playerId, EntityId vehicleId)
{
	if (!playerId || !vehicleId) return;
	EntityId localActor = m_pClientActor->GetEntityId();
	if (playerId == localActor)
	{
		m_iPlayerOwnedVehicle = vehicleId;
	}
}
//-----------------------------------------------------------------------------------------------------

//handles all kinds of mouse clicks and flash callbacks
void CHUD::HandleFSCommand(const char* szCommand, const char* szArgs)
{
	if (g_pGameCVars->g_debug_fscommand)
		CryLog("HandleFSCommand : %s %s\n", szCommand, szArgs);
	HandleFSCommandPDA(szCommand, szArgs);

	if (!strcmp(szCommand, "WeaponAccessory"))
	{
		CWeapon* pCurrentWeapon = GetCurrentWeapon();
		if (pCurrentWeapon)
		{
			string s(szArgs);
			int sep = s.find("+");

			if (sep == -1) return;

			string helper(s.substr(0, sep));
			string attachment(s.substr(sep + 1, s.length() - (sep + 1)));
			PlaySound(ESound_WeaponModification);
			if (!strcmp(attachment.c_str(), "NoAttachment"))
			{
				const char* curAttach = pCurrentWeapon->CurrentAttachment(ItemString(helper.c_str()));
				szArgs = curAttach ? curAttach : "";
			}
			else
			{
				szArgs = attachment.c_str();
			}

			const ItemString accessory(szArgs);
			const bool bAddAccessory = pCurrentWeapon->GetAccessory(accessory) == 0;
			pCurrentWeapon->SwitchAccessory(accessory);
			HUD_CALL_LISTENERS(WeaponAccessoryChanged(pCurrentWeapon, szArgs, bAddAccessory));

			// player's squadmates mimicking weapon switch accessory
			if (gEnv->pAISystem)
			{
				IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();//AI System will be the owner of this data
				pData->SetObjectName(szArgs);
				pData->iValue = bAddAccessory; // unmount/mount

				if (m_pClientActor && m_pClientActor->GetEntity()->GetAI())
					gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 10, "OnSwitchWeaponAccessory", m_pClientActor->GetEntity()->GetAI(), pData);
			}
		}
		return;
	}
	else if (!_stricmp(szCommand, "Suicide"))
	{
		if (m_pClientActor && m_pClientActor->GetHealth() > 0)
			ShowWarningMessage(EHUD_SUICIDE);
	}
	else if (!strcmp(szCommand, "FlashGetKeyFocus"))
	{
		if (GetMPChat() && m_animChat.GetVisible())
		{
			GetMPChat()->ShutDown();
		}
		m_bExclusiveListener = true;
		GetISystem()->GetIInput()->ClearKeyState();
		GetISystem()->GetIInput()->SetExclusiveListener(this);
	}
	else if (!strcmp(szCommand, "FlashLostKeyFocus"))
	{
		if (m_bExclusiveListener)
		{
			GetISystem()->GetIInput()->ClearKeyState();
			GetISystem()->GetIInput()->SetExclusiveListener(NULL);
		}
	}
	if (!_stricmp(szCommand, "menu_highlight"))
	{
		PlaySound(ESound_Highlight);
	}
	else if (!_stricmp(szCommand, "menu_select"))
	{
		PlaySound(ESound_Select);
	}
	else if (!strcmp(szCommand, "MapOpened"))
	{
		m_pHUDRadar->InitMap();
	}
	else if (!strcmp(szCommand, "RepeatLastPurchase"))
	{
		if (m_pHUDPowerStruggle)
			m_pHUDPowerStruggle->BuyPackage(-1);
	}

	else if (!strcmp(szCommand, "JoinGame"))
	{
		gEnv->pConsole->ExecuteString("join_game");
		ShowPDA(false);
	}
	else if (!strcmp(szCommand, "Autojoin"))
	{

		int lt = 0;
		int ltn = 0;
		int nteams = m_pGameRules->GetTeamCount();
		for (int i = 1;i <= nteams;i++)
		{
			int n = m_pGameRules->GetTeamPlayerCount(i);
			if (!lt || ltn > n)
			{
				lt = i;
				ltn = n;
			}
		}

		if (lt == 0)
			lt = 1;

		CryFixedStringT<64> cmd("team ");
		cmd.append(m_pGameRules->GetTeamName(lt));
		gEnv->pConsole->ExecuteString(cmd.c_str());

		if (GetModalHUD() == &m_animTeamSelection)
		{
			m_animTeamSelection.SetVisible(false);
			SwitchToModalHUD(NULL, false);
		}
		
		ShowPDA(false);
	}
	else if (!strcmp(szCommand, "Spectate"))
	{
		if (m_pGameRules)
		{
			if (m_pGameRules->GetCurrentStateId() != 2 && GetModalHUD() != &m_animTeamSelection)
				ShowWarningMessage(EHUD_SPECTATOR);
			else
			{
				HandleWarningAnswer("spectate");
			}
		}
	}
	else if (!strcmp(szCommand, "SwitchTeam"))
	{
		if (m_pGameRules)
		{
			if (m_pGameRules->GetCurrentStateId() != 2)
			{
				if (m_pGameRules->GetTeamId("black") == m_pGameRules->GetTeam(m_pClientActor->GetEntityId()))
					ShowWarningMessage(EHUD_SWITCHTOTAN);
				else
					ShowWarningMessage(EHUD_SWITCHTOBLACK);
			}
			else
			{
				ShowPDA(false);
				HandleWarningAnswer("switchTeam");
			}
		}
	}
	else if (!strcmp(szCommand, "WarningBox"))
	{
		HandleWarningAnswer(szArgs);
	}
	else if (!strcmp(szCommand, "ChangeTeam"))
	{
		if (m_pGameRules && m_pGameRules->GetTeamCount() > 1)
		{
			if (m_pGameRules->GetTeamId(szArgs) != m_pGameRules->GetTeam(m_pClientActor->GetEntityId()))
			{
				string command("team ");
				command.append(szArgs);
				gEnv->pConsole->ExecuteString(command.c_str());
				ShowPDA(false);

				if (GetModalHUD() == &m_animTeamSelection)
				{
					m_animTeamSelection.SetVisible(false);
					SwitchToModalHUD(NULL, false);
				}
			}
		}
	}
	else if (!strcmp(szCommand, "StopInitialize"))
	{
		m_bDestroyInitializePending = true;
		PlaySound(ESound_ReActivate);
	}
	else if (!strcmp(szCommand, "QuickMenuSpeedPreset"))
	{
		OnQuickMenuSpeedPreset();
		QuickMenuSnapToMode(NANOMODE_SPEED);
		return;
	}
	else if (0 == strcmp(szCommand, "QuickMenuStrengthPreset"))
	{
		OnQuickMenuStrengthPreset();
		QuickMenuSnapToMode(NANOMODE_STRENGTH);
		return;
	}
	else if (0 == strcmp(szCommand, "QuickMenuDefensePreset"))
	{
		OnQuickMenuDefensePreset();
		QuickMenuSnapToMode(NANOMODE_DEFENSE);
		return;
	}
	else if (0 == strcmp(szCommand, "QuickMenuDefault")) //this is cloak now
	{
		OnCloak();
		QuickMenuSnapToMode(NANOMODE_CLOAK);
		return;
	}
	else if (!strcmp(szCommand, "QuickMenuSwitchWeaponAccessories"))
	{
		if (ShowWeaponAccessories(true))
			m_bLaunchWS = true;
		return;
	}
	else if (!strcmp(szCommand, "Cloak"))
	{
		OnCloak();
		return;
	}
	else if (!_stricmp(szCommand, "MP_TeamMateSelected"))
	{
		EntityId id = static_cast<EntityId>(atoi(szArgs));
		m_pHUDRadar->SelectTeamMate(id, true);

		PlaySound(ESound_Select);
	}
	else if (!_stricmp(szCommand, "MP_KickPlayer"))
	{
		EntityId id = static_cast<EntityId>(atoi(szArgs));
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
		if (pEntity)
		{
			string kick("kick ");
			kick.append(pEntity->GetName());
			gEnv->pConsole->ExecuteString(kick.c_str());
		}
	}
	else if (!_stricmp(szCommand, "MuteMember"))
	{
		if (szArgs)
		{
			const EntityId muteId = atoi(szArgs);

			IVoiceContext* pVoiceContext = gEnv->pGame->GetIGameFramework()->GetNetContext()->GetVoiceContext();
			const bool muted = pVoiceContext->IsMuted(m_pClientActor->GetEntityId(), muteId);

			gEnv->pGame->GetIGameFramework()->MutePlayerById(muteId);

			PlaySound(muted ? ESound_DownloadStop : ESound_DownloadStart);
		}
	}
	else if (!_stricmp(szCommand, "MP_TeamMateDeselected"))
	{
		EntityId id = static_cast<EntityId>(atoi(szArgs));
		m_pHUDRadar->SelectTeamMate(id, false);

		PlaySound(ESound_Select);
	}
	else if (!_stricmp(szCommand, "soundstart_malfunction"))
	{
		PlaySound(ESound_Malfunction);
	}
	/*	else if(!_stricmp(szCommand,"soundstop_malfunction"))
		{
			note: hud_malfunction has been changed to oneshot instead of looping sound
			PlaySound(ESound_Malfunction,false);
		}*/
	else if (!_stricmp(szCommand, "vehicle_init"))
	{
		PlaySound(ESound_VehicleIn);
	}
	else if (!_stricmp(szCommand, "law_locking"))
	{
		PlaySound(ESound_LawLocking);
	}
	else if (!_stricmp(szCommand, "hud_download_start"))
	{
		PlaySound(ESound_DownloadStart);
	}
	else if (!_stricmp(szCommand, "hud_download_loop"))
	{
		//PlaySound(ESound_DownloadLoop);
	}
	else if (!_stricmp(szCommand, "hud_download_stop"))
	{
		//PlaySound(ESound_DownloadLoop,false);
		PlaySound(ESound_DownloadStop);
	}
	else if (!_stricmp(szCommand, "closeBinoculars"))
	{
		m_pHUDScopes->DestroyBinocularsAtNextFrame();
	}
	//multiplayer map functions
	if (gEnv->bMultiplayer)
	{
		if (!strcmp(szCommand, "MPMap_SelectObjective"))
		{
			EntityId id = 0;
			if (szArgs)
				id = EntityId(atoi(szArgs));
			SetOnScreenObjective(id);
		}
		else if (!strcmp(szCommand, "MPMap_SelectSpawnPoint"))
		{
			EntityId id = 0;
			if (szArgs)
				id = EntityId(atoi(szArgs));

			EntityId iCurrentSpawnPoint = 0;
			if (m_pGameRules)
				iCurrentSpawnPoint = m_pGameRules->GetPlayerSpawnGroup(m_pClientActor);

			if (iCurrentSpawnPoint && iCurrentSpawnPoint == id)
			{
				SetOnScreenObjective(id);
			}
			else if (m_pGameRules)
			{
				m_pGameRules->RequestSpawnGroup(id);
				m_changedSpawnGroup = true;
			}
		}
		else if (!strcmp(szCommand, "HoverBuyItem"))
		{
			if (szArgs)
			{
				HUD_CALL_LISTENERS(OnBuyMenuItemHover(szArgs));
			}
		}
		else if (!strcmp(szCommand, "RequestNewLoadoutName"))
		{
			if (m_pModalHUD == &m_animBuyMenu)
			{
				string name;
				if (m_pHUDPowerStruggle)
					m_pHUDPowerStruggle->RequestNewLoadoutName(name, "");
				m_animBuyMenu.SetVariable("_root.POPUP.POPUP_NewPackage.m_modifyPackageName", SFlashVarValue(name.c_str()));
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SpawnPointInvalid()
{
	if (m_animSpawnCycle.IsLoaded())
	{
		m_animSpawnCycle.Invoke("spawnPointLost", true);
	}
}
//-----------------------------------------------------------------------------------------------------

void CHUD::OnQuickMenuSpeedPreset()
{
	if (m_pNanoSuit->GetMode() != NANOMODE_SPEED)
	{
		m_pNanoSuit->SetMode(NANOMODE_SPEED);

		m_fAutosnapCursorRelativeX = 0.0f;
		m_fAutosnapCursorRelativeY = -30.0f;
	}
	else
		PlaySound(ESound_TemperatureBeep);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnQuickMenuStrengthPreset()
{
	if (m_pNanoSuit->GetMode() != NANOMODE_STRENGTH)
	{
		m_pNanoSuit->SetMode(NANOMODE_STRENGTH);

		m_fAutosnapCursorRelativeX = 30.0f;
		m_fAutosnapCursorRelativeY = 0.0f;
	}
	else
		PlaySound(ESound_TemperatureBeep);
}
//-----------------------------------------------------------------------------------------------------


void CHUD::OnQuickMenuDefensePreset()
{
	if (m_pNanoSuit->GetMode() != NANOMODE_DEFENSE)
	{
		m_pNanoSuit->SetMode(NANOMODE_DEFENSE);

		m_fAutosnapCursorRelativeX = -30.0f;
		m_fAutosnapCursorRelativeY = 0.0f;
	}
	else
		PlaySound(ESound_TemperatureBeep);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnCloak()
{
	char cloakState(m_pNanoSuit->GetCloak()->GetState());
	if (!cloakState)	//if cloak is getting activated
	{
		if (m_pNanoSuit->GetMode() != NANOMODE_CLOAK)
		{
			m_pNanoSuit->SetMode(NANOMODE_CLOAK);

			m_fAutosnapCursorRelativeX = 20.0f;
			m_fAutosnapCursorRelativeY = 30.0f;
		}
		else
			PlaySound(ESound_TemperatureBeep);
	}
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::OnInputEventUI(const SInputEvent& rInputEvent)
{
	if (gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated())
		return false;


	if (m_buyMenuKeyLog.m_state == SBuyMenuKeyLog::eBMKL_NoInput)
	{
		if (eDI_Keyboard == rInputEvent.deviceId)
		{
			IFlashPlayer* pFlashPlayer = NULL;
			if (m_pModalHUD == &m_animPDA)
			{
				pFlashPlayer = m_animPDA.GetFlashPlayer();
			}
			else if (m_pModalHUD == &m_animBuyMenu)
			{
				pFlashPlayer = m_animBuyMenu.GetFlashPlayer();
			}
			if (pFlashPlayer)
			{
				SFlashKeyEvent keyEvent(CFlashMenuObject::MapToFlashKeyEvent(rInputEvent));

				pFlashPlayer->SendKeyEvent(keyEvent);
				keyEvent.m_state = SFlashKeyEvent::eKeyUp;
				pFlashPlayer->SendKeyEvent(keyEvent);
				keyEvent.m_state = SFlashKeyEvent::eKeyDown;
				return true;
			}
		}
		return false;
	}
	return false;
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::OnInputEvent(const SInputEvent& rInputEvent)
{
	if ((gEnv->bEditor || g_pGame->GetMenu()->IsActive() == false) && m_bCutscenePlaying && m_fCutsceneSkipTimer <= 0.0f)
	{
		//skip cut scene
		if ((rInputEvent.keyId == eKI_Space && rInputEvent.deviceId == eDI_Keyboard) || (rInputEvent.keyId == eKI_XI_Start && rInputEvent.deviceId == eDI_XI))
		{
			if (m_bCutsceneAbortPressed == true && rInputEvent.state == eIS_Released)
			{
				m_bStopCutsceneNextUpdate = true;
				m_bCutsceneAbortPressed = false;
#ifdef USER_alexl
				CryLogAlways("[CX] Frame=%d Space released -> scheduling abort", gEnv->pRenderer->GetFrameID(false));
#endif
				return true;
			}
			else if (rInputEvent.state == eIS_Pressed)
			{
#ifdef USER_alexl
				CryLogAlways("[CX] Frame=%d Space pressed", gEnv->pRenderer->GetFrameID(false));
#endif
				m_bCutsceneAbortPressed = true;
				return true;
			}
		}
		if (rInputEvent.state == eIS_Pressed)
		{
			if ((rInputEvent.keyId == eKI_Escape && rInputEvent.deviceId == eDI_Keyboard) || (rInputEvent.keyId == eKI_XI_Back && rInputEvent.deviceId == eDI_XI))
			{
				g_pGame->GetMenu()->ShowInGameMenu(true);
				return true;
			}
		}
	}

	if (!g_pGame->GetMenu()->IsActive())
	{
		if (rInputEvent.state == eIS_Pressed)
		{
			if (rInputEvent.keyId == eKI_Escape ||
				rInputEvent.keyId == eKI_PS3_Select)
			{
				if (!ReturnFromLastModalScreen())
				{
					g_pGame->GetMenu()->ShowInGameMenu(true);
					return true;
				}
			}
		}
	}

	if (gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated()) return false;

	// Prevent cheating with using mouse and assisted controller at the same time
	if (rInputEvent.deviceId == eDI_Mouse)
		m_lastNonAssistedInput = gEnv->pTimer->GetCurrTime();

	bool assistance = IsInputAssisted();
	if (m_hitAssistanceAvailable != assistance)
	{
		// Notify server on the change
		m_pClientActor->GetGameObject()->InvokeRMI(CPlayer::SvRequestHitAssistance(), CPlayer::HitAssistanceParams(assistance), eRMI_ToServer);

		m_hitAssistanceAvailable = assistance;
	}

	bool sittingInVehicle = false;

	if (m_pClientActor->GetLinkedVehicle())
	{
		sittingInVehicle = true;
	}

	if (m_buyMenuKeyLog.m_state == SBuyMenuKeyLog::eBMKL_NoInput || sittingInVehicle)
	{
		if (eDI_Keyboard == rInputEvent.deviceId)
		{
			IFlashPlayer* pFlashPlayer = NULL;
			if (m_pModalHUD == &m_animPDA)
			{
				pFlashPlayer = m_animPDA.GetFlashPlayer();
			}
			else if (m_pModalHUD == &m_animBuyMenu)
			{
				pFlashPlayer = m_animBuyMenu.GetFlashPlayer();
			}
			if (pFlashPlayer && (eIS_Pressed == rInputEvent.state || eIS_Released == rInputEvent.state))
			{
				SFlashKeyEvent keyEvent(CFlashMenuObject::MapToFlashKeyEvent(rInputEvent));
				pFlashPlayer->SendKeyEvent(keyEvent);
				return true;
			}
		}
		return false;
	}

	if (m_animRadioButtons.GetVisible())
		return false;

	if (!m_animBuyMenu.IsLoaded())
		return false;

	if (rInputEvent.deviceId != eDI_Keyboard)
		return false;

	if (rInputEvent.state != eIS_Released)
		return false;

	if (gEnv->pConsole->GetStatus())
		return false;

	if (!gEnv->bMultiplayer)
		return false;

	const char* sKey = rInputEvent.keyName;
	// nasty check, but fastest early out
	if (sKey && sKey[0] && !sKey[1])
	{
		int iKey = atoi(sKey);
		if (iKey == 0 && sKey[0] != '0')
			iKey = -1;

		if (iKey >= 0)
		{
			if (m_buyMenuKeyLog.m_state == SBuyMenuKeyLog::eBMKL_Tab)
			{
				m_animBuyMenu.Invoke("inputTab", SFlashVarValue(sKey));
				m_buyMenuKeyLog.m_state = SBuyMenuKeyLog::eBMKL_Frame;
			}
			else if (m_buyMenuKeyLog.m_state == SBuyMenuKeyLog::eBMKL_Frame)
			{
				m_animBuyMenu.Invoke("inputFrame", SFlashVarValue(sKey));
				m_buyMenuKeyLog.m_state = SBuyMenuKeyLog::eBMKL_Button;
			}
			else if (m_buyMenuKeyLog.m_state == SBuyMenuKeyLog::eBMKL_Button)
			{
				m_animBuyMenu.Invoke("inputButton", SFlashVarValue(sKey));
				m_buyMenuKeyLog.m_state = SBuyMenuKeyLog::eBMKL_Tab;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::BuyViaFlash(int iItem)
{
	m_animBuyMenu.Invoke("Root.PDAArea.buyItem", iItem);
	m_buyMenuKeyLog.Clear();
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::OnAction(const ActionId& action, int activationMode, float value)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);
	const CGameActions& rGameActions = g_pGame->Actions();
	bool filterOut = true;

	if (action == rGameActions.toggle_airstrike)
	{
		SetAirStrikeEnabled(!IsAirStrikeAvailable());
	}
	else if (action == rGameActions.hud_mousex)
	{
		if (m_pHUDRadar->GetDrag())
		{
			m_pHUDRadar->DragMap(Vec2(value, 0));
		}
		else if (m_bAutosnap)
		{
			m_fAutosnapCursorRelativeX += value;
		}
		filterOut = false;
	}
	else if (action == rGameActions.hud_mousey)
	{
		if (m_pHUDRadar->GetDrag())
		{
			m_pHUDRadar->DragMap(Vec2(0, value));
		}
		if (m_bMiniMapZooming && m_pModalHUD == &m_animPDA)
		{
			m_pHUDRadar->ZoomChangePDA(value);
		}
		else if (m_bAutosnap)
		{
			m_fAutosnapCursorRelativeY += value;
		}
		filterOut = false;
	}
	else if (action == rGameActions.xi_hud_back)
	{
		float now = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		if (activationMode == eIS_Pressed)
		{
			m_fBackPressedTime = now;
		}
		else if (activationMode == eIS_Released)
		{
			if (m_fBackPressedTime && (now - m_fBackPressedTime) < 0.2f)
			{
				m_fBackPressedTime = 0.0f;
				if (!ReturnFromLastModalScreen())
				{
					if (!g_pGame->GetMenu()->IsActive())
					{
						g_pGame->GetMenu()->ShowInGameMenu(true);
						filterOut = true;
					}
				}
			}
			else
			{
				OnAction(rGameActions.hud_hide_multiplayer_scoreboard, eIS_Released, 1);
			}
		}
	}


	else if (action == rGameActions.xi_rotatepitch || action == rGameActions.xi_v_rotatepitch)
	{
		if (m_isMouseCursorVisible)
		{
			m_fAutosnapCursorControllerY = value * value * (-value);
		}
		else if (m_bAutosnap)
		{
			if (fabsf(value) > 0.5f || (fabsf(m_fAutosnapCursorControllerX) + fabsf(value)) > 0.5)
				m_fAutosnapCursorControllerY = -value;
		}
		filterOut = false;
	}
	else if (action == rGameActions.xi_rotateyaw || action == rGameActions.xi_v_rotateyaw)
	{
		if (m_isMouseCursorVisible)
		{
			m_fAutosnapCursorControllerX = value * value * value;
		}
		else if (m_bAutosnap)
		{
			if (fabsf(value) > 0.5f || (fabsf(m_fAutosnapCursorControllerY) + fabsf(value)) > 0.5)
				m_fAutosnapCursorControllerX = value;
		}
		filterOut = false;
	}
	/*else if(action != rGameActions.rotateyaw && action != rGameActions.rotatepitch && action != rGameActions.binoculars && action != rGameActions.hud_night_vision)
	{
		if(m_fPlayerDeathTime && gEnv->pTimer->GetFrameStartTime().GetSeconds() - m_fPlayerDeathTime > 3.0f)
		{
			if(!g_pGame->GetMenu()->IsActive())
			{
				m_fPlayerDeathTime = m_fPlayerDeathTime -  20.0f; //0.0f;  //load immediately
				DisplayFlashMessage("", 2);
				DisplayOverlayFlashMessage("");
				return true;
			}
		}
	}*/

	if (action == rGameActions.attack1 && activationMode == eIS_Pressed)
	{
		//restart
		if (m_fPlayerDeathTime && gEnv->pTimer->GetFrameStartTime().GetSeconds() - m_fPlayerDeathTime > 3.0f)
		{
			if (!g_pGame->GetMenu()->IsActive())
			{
				m_fPlayerDeathTime = m_fPlayerDeathTime - 20.0f; //0.0f;  //load immediately
				DisplayFlashMessage("", 2);
				return true;
			}
		}
		//revive
		if (m_fPlayerRespawnTimer && !m_bRespawningFromFakeDeath)
		{
			m_bRespawningFromFakeDeath = true;
			m_fPlayerRespawnTimer = gEnv->pTimer->GetFrameStartTime().GetSeconds() + 4.0f;
		}
		//AirStrike
		if (IsAirStrikeAvailable() && m_pHUDScopes->IsBinocularsShown())
		{
			if (StartAirStrike())
			{
				m_fAirStrikeStarted = gEnv->pTimer->GetFrameStartTime().GetSeconds();
			}
		}
	}

	if (action == rGameActions.xi_hud_mouseclick)
	{
		if (m_pModalHUD)
		{
			float x, y = 0.0f;
			gEnv->pHardwareMouse->GetHardwareMouseClientPosition(&x, &y);

			SFlashCursorEvent::ECursorState eCursorState = SFlashCursorEvent::eCursorMoved;
			if (activationMode == eIS_Pressed)
			{
				eCursorState = SFlashCursorEvent::eCursorPressed;
			}
			else if (activationMode == eIS_Released)
			{
				eCursorState = SFlashCursorEvent::eCursorReleased;
			}

			int iX((int)x), iY((int)y);
			m_pModalHUD->GetFlashPlayer()->ScreenToClient(iX, iY);
			m_pModalHUD->GetFlashPlayer()->SendCursorEvent(SFlashCursorEvent(eCursorState, iX, iY));
		}
	}

	//workaround for Bernd to allow shooting while in quick menu :-(
	if (action == rGameActions.hud_mouseclick)
	{
		if (m_pModalHUD == &m_animQuickMenu)
		{
			if (m_pClientActor && !m_pClientActor->GetLinkedVehicle() && !m_pClientActor->GetActorStats()->inFreefall.Value())
			{
				IItem* pItem = m_pClientActor->GetCurrentItem();
				if (pItem && pItem->GetIWeapon())
				{
					pItem->GetIWeapon()->OnAction(m_pClientActor->GetEntityId(), rGameActions.attack1, activationMode, 1.0f);
				}
			}
		}
	}

	if (action == rGameActions.hud_buy_weapons)
	{
		// don't show buy menu if spectating.
		if (m_pClientActor && m_pGameRules && m_pGameRules->IsPlayerActivelyPlaying(m_pClientActor->GetEntityId()))
		{
			if (m_pModalHUD == &m_animBuyMenu)
				ShowBuyMenu(false);
			else
				ShowBuyMenu(true);
		}
		return false;
	}

	if (action == rGameActions.hud_suit_menu && activationMode == eIS_Pressed)
	{
		if (!m_bShow || gEnv->pConsole->GetStatus())	//don't process while hud is disabled or console is opened
			return false;

		if (m_pModalHUD == &m_animPDA)
		{
			m_bMiniMapZooming = true;
		}
		else if (IsModalHUDAvailable())
		{
			if (m_pClientActor->GetHealth() > 0 && !m_pClientActor->GetSpectatorMode())
			{
				if (m_pClientActor->GetNanoSuit() && !m_pClientActor->GetNanoSuit()->IsActive())
					return false;
				/*				if(!m_animQuickMenu.IsLoaded())
									m_animQuickMenu.Reload();*/

				// CryMP: detect radio message tagging
				if (!m_nanosuitMenuOpenTime) {
					m_nanosuitMenuOpenTime = gEnv->pTimer->GetCurrTime();
					m_nanosuitOpenMode = m_pClientActor->GetNanoSuit()->GetMode();
				}

				m_animQuickMenu.Invoke("showQuickMenu");
				m_animQuickMenu.SetVariable("_alpha", 100);

				QuickMenuSnapToMode(m_pClientActor->GetNanoSuit()->GetMode());
				m_pClientActor->GetPlayerInput()->DisableXI(true);

				PlaySound(ESound_SuitMenuAppear);
				m_pClientActor->GetPlayerInput()->DisableXI(true);
				g_pGameActions->FilterSuitMenu()->Enable(true);
				g_pGameActions->FilterInVehicleSuitMenu()->Enable(true);
				m_bAutosnap = true;
				//UpdateCrosshairVisibility();
				SwitchToModalHUD(&m_animQuickMenu, false);
				m_animQuickMenu.CheckedInvoke("destroy", m_iBreakHUD);
			}
			filterOut = false;
		}
		else if (m_pModalHUD == &m_animWeaponAccessories)
		{
			if (!m_bIgnoreMiddleClick)
			{
				ShowWeaponAccessories(false);
			}
		}
		filterOut = false;
	}
	else if (action == rGameActions.hud_suit_menu && activationMode == eIS_Released)
	{
		if (m_pModalHUD == &m_animPDA)
		{
			m_bMiniMapZooming = false;
		}
		else if (&m_animQuickMenu == m_pModalHUD || &m_animQuickMenu == m_pSwitchScoreboardHUD)
		{
			if (m_pClientActor->GetPlayerInput())
				m_pClientActor->GetPlayerInput()->DisableXI(false);

			g_pGameActions->FilterSuitMenu()->Enable(false);
			g_pGameActions->FilterInVehicleSuitMenu()->Enable(false);
			m_bAutosnap = false;
			//UpdateCrosshairVisibility();
			if (&m_animQuickMenu == m_pModalHUD)
			{
				SwitchToModalHUD(NULL, false);
			}
			m_animQuickMenu.CheckedInvoke("Root.QuickMenu.setAutosnapFunction");
			m_fAutosnapCursorRelativeX = 0;
			m_fAutosnapCursorRelativeY = 0;
			m_fAutosnapCursorControllerX = 0;
			m_fAutosnapCursorControllerY = 0;
			m_bOnCircle = false;

			m_animQuickMenu.Invoke("hideQuickMenu");

			// While holding down mouse3, we went in scoreboard
			// Then while in scoreboard, we release mouse3
			// We must close the quickmenu even if it is not visible
			if (&m_animQuickMenu == m_pSwitchScoreboardHUD)
			{
				m_animQuickMenu.GetFlashPlayer()->Advance(0.5f);
				m_pSwitchScoreboardHUD = NULL;
			}

			if (!m_bLaunchWS)
				PlaySound(ESound_SuitMenuDisappear);

			// CryMP: detect radio message tagging
			if (m_nanosuitMenuOpenTime) {
				float dur = gEnv->pTimer->GetCurrTime() - *m_nanosuitMenuOpenTime;
				m_nanosuitMenuOpenTime.reset();
				if (
					dur < 0.25f &&
					gEnv->bMultiplayer && gEnv->bClient &&
					!m_bLaunchWS &&
					m_pClientActor && m_pClientActor->GetNanoSuit() && m_pClientActor->GetNanoSuit()->GetMode() == m_nanosuitOpenMode
					) {
					m_pGameRules->RequestTrackedRadio(m_pClientActor, 15);
				}
			}

			m_bLaunchWS = false;
		}
		filterOut = false;
	}
	else if (action == rGameActions.hud_openchat || action == rGameActions.hud_openteamchat)
	{
		//CryMP direction action..
		if (m_pGameRules)
		{
			auto mode = (action == rGameActions.hud_openchat) ? 1 : 2;
			if (mode == 2) //don't team chat while not having joined a team...
			{
				if (!m_pGameRules->IsPlayerActivelyPlaying(m_pClientActor->GetEntityId()))
					mode = 0;
			}
			if (GetMPChat())
				GetMPChat()->OpenChat(mode);
		}
		return true;
	}
	else if (action == rGameActions.hud_toggle_scoreboard_cursor)
	{
		if (m_pModalHUD == &m_animScoreBoard && gEnv->bMultiplayer)
		{
			if (activationMode == eIS_Pressed)
			{
				m_bScoreboardCursor = true;
				this->ShowMouseCursor(true);
				g_pGameActions->FilterNoMove()->Enable(true);

				m_pClientActor->GetPlayerInput()->DisableXI(true);
			}
			else if (activationMode == eIS_Released)
			{
				if (m_bScoreboardCursor)
				{
					m_bScoreboardCursor = false;
					this->ShowMouseCursor(false);
					g_pGameActions->FilterNoMove()->Enable(false);

					m_pClientActor->GetPlayerInput()->DisableXI(false);
				}
			}
		}
		filterOut = false;
	}
	else if (action == rGameActions.hud_show_multiplayer_scoreboard && activationMode == eIS_Pressed)
	{
		if (gEnv->bMultiplayer)
		{
			if (m_pGameRules && m_animScoreBoard.IsLoaded() && m_pHUDScore && !m_pHUDScore->m_bShow && GetModalHUD() != &m_animWarningMessages)
			{
				m_pGameRules->ShowScores(true);
				m_pSwitchScoreboardHUD = m_pModalHUD;
				if (m_pSwitchScoreboardHUD && m_pSwitchScoreboardHUD != &m_animQuickMenu)
				{
					// We don't want to have the cursor already active in the scoreboard
					// The player HAS to hit the spacebar to activate the mouse!
					this->ShowMouseCursor(false);
				}
				SwitchToModalHUD(&m_animScoreBoard, false);
				PlaySound(ESound_MapOpen);
				m_animScoreBoard.Invoke("Root.setVisible", 1);
				m_animScoreBoard.Invoke("setOwnTeam", m_pGameRules->GetTeam(m_pClientActor->GetEntityId()));
				m_pHUDScore->SetVisible(true, &m_animScoreBoard);
				m_bShow = false;

				HUD_CALL_LISTENERS(OnShowScoreBoard());
			}
		}
		else if (m_pModalHUD == NULL)
		{
			if (m_animObjectivesTab.GetVisible())
				ShowObjectives(false); //with XBOX controller "back" works different: one press: on, second press: off
			else
				ShowObjectives(true);
		}
		filterOut = false;
	}
	else if (!m_forceScores &&
		((action == rGameActions.hud_show_multiplayer_scoreboard && activationMode == eIS_Released) ||
			action == rGameActions.hud_hide_multiplayer_scoreboard))
	{
		if (gEnv->bMultiplayer)
		{
			if (m_pGameRules && m_animScoreBoard.IsLoaded() && m_pHUDScore && m_pHUDScore->m_bShow)
			{
				m_pGameRules->ShowScores(false);
				if (m_bScoreboardCursor)
				{
					m_bScoreboardCursor = false;
					this->ShowMouseCursor(false);
					g_pGameActions->FilterNoMove()->Enable(false);

					m_pClientActor->GetPlayerInput()->DisableXI(false);
				}
				if (m_pSwitchScoreboardHUD && m_pSwitchScoreboardHUD != &m_animQuickMenu)
				{
					// Restore the mouse if we were coming from a modal hud
					// Seems that only the quick menu does not use the mouse
					this->ShowMouseCursor(true);
				}
				SwitchToModalHUD(m_pSwitchScoreboardHUD, (m_pSwitchScoreboardHUD != &m_animQuickMenu) ? true : false);
				PlaySound(ESound_MapClose);
				m_animScoreBoard.Invoke("Root.setVisible", 0);
				m_pHUDScore->SetVisible(false);
				m_bShow = true;
			}
		}
		else
		{
			ShowObjectives(false);
		}
	}
	else if (action == rGameActions.hud_show_pda_map)
	{
		bool show = m_pModalHUD != &m_animPDA;
		ShowPDA(show);

		if (!show && m_changedSpawnGroup) // are we closing
		{
			RequestRevive();
			m_changedSpawnGroup = false;
		}

		filterOut = false;
	}
	else if (action == rGameActions.xi_hud_show_pda_map)
	{
		if (activationMode == eAAM_OnPress)
		{
			m_fXINightVisionActivateTimer = 0.0f;
		}
		else if (activationMode == eAAM_OnRelease)
		{
			if (m_fXINightVisionActivateTimer >= 0.0f)
			{
				m_fXINightVisionActivateTimer = -1.0;
				bool show = m_pModalHUD != &m_animPDA;
				ShowPDA(show);

				if (!show && m_changedSpawnGroup) // are we closing
				{
					RequestRevive();
					m_changedSpawnGroup = false;
				}

				filterOut = false;
			}
		}
	}
	else if (action == rGameActions.hud_mousewheelup)
	{
		if (m_pModalHUD == &m_animPDA)
			m_pHUDRadar->ZoomPDA(true);
		else if (m_pModalHUD == &m_animBuyMenu)
			m_pHUDPowerStruggle->Scroll(-1);
		filterOut = false;
	}
	else if (action == rGameActions.hud_mousewheeldown)
	{
		if (m_pModalHUD == &m_animPDA)
			m_pHUDRadar->ZoomPDA(false);
		else if (m_pModalHUD == &m_animBuyMenu)
			m_pHUDPowerStruggle->Scroll(1);
		filterOut = false;
	}
	else if (action == rGameActions.hud_mouserightbtndown)
	{
		if (m_pModalHUD == &m_animPDA)
			m_pHUDRadar->SetDrag(true);
		filterOut = false;
	}
	else if (action == rGameActions.hud_mouserightbtnup)
	{
		if (m_pModalHUD == &m_animPDA)
			m_pHUDRadar->SetDrag(false);
		else if (m_pModalHUD == &m_animWeaponAccessories)
		{
			ShowWeaponAccessories(false);
		}
		filterOut = false;
	}
	else if (action == rGameActions.hud_night_vision)
	{
		float now = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		if (now - m_fNightVisionTimer > 0.2f)	//strange bug - action is produces twice "onPress" (even when set to onRelease)
		{
			CNanoSuit* pSuit = m_pClientActor->GetNanoSuit();

			if ((!(m_pClientActor->GetHealth() < 0 && !m_bNightVisionActive)) && (m_bNightVisionActive || (pSuit && pSuit->IsNightVisionEnabled())))
			{
				gEnv->p3DEngine->SetPostEffectParam("NightVision_Active", float(!m_bNightVisionActive));
				m_bNightVisionActive = !m_bNightVisionActive;

				if (m_bNightVisionActive && !m_animNightVisionBattery.IsLoaded())
				{
					PlaySound(ESound_NightVisionSelect);
					PlaySound(ESound_NightVisionAmbience);
					m_animNightVisionBattery.Load("Libs/UI/HUD_NightVision.gfx", eFD_Right);
				}
				else
				{
					PlaySound(ESound_NightVisionAmbience, false);
					PlaySound(ESound_NightVisionDeselect);
				}
				HUD_CALL_LISTENERS(OnNightVision(m_bNightVisionActive));
				m_fNightVisionTimer = now;
			}
		}
		filterOut = false;
	}
	else if (action == rGameActions.hud_weapon_mod)
	{
		if (NULL == m_pModalHUD)
		{
			ShowWeaponAccessories(true);
		}
		else if (m_pModalHUD == &m_animWeaponAccessories)
		{
			ShowWeaponAccessories(false);
		}
		filterOut = false;
	}
	else if (action == rGameActions.objectives)
	{
		if (activationMode == eIS_Pressed)
		{
			if (g_pGameCVars->hud_showAllObjectives == 1)
				m_bShowAllOnScreenObjectives = !m_bShowAllOnScreenObjectives;
			else
				m_bShowAllOnScreenObjectives = true;
		}
		else if (activationMode == eIS_Released)
		{
			if (g_pGameCVars->hud_showAllObjectives != 1)
				m_bShowAllOnScreenObjectives = false;
		}
		filterOut = false;
	}
	else if (action == rGameActions.hud_suit_mod)
	{
		if (activationMode == 1)
			OnAction(rGameActions.hud_suit_menu, eIS_Pressed, 1);
		else if (activationMode == 2)
			OnAction(rGameActions.hud_suit_menu, eIS_Released, 1);
		filterOut = false;
	}
	else if (action == rGameActions.hud_select1)
	{
		if (m_animRadioButtons.GetVisible()) return false;

		if (m_pClientActor->GetFrozenAmount(true) > 0.0f) return false;
		if (m_pModalHUD == &m_animWeaponAccessories)
			m_animWeaponAccessories.Invoke("toggleAttachment", 1);
		else if (m_pModalHUD == &m_animQuickMenu && !m_iBreakHUD && IsQuickMenuButtonActive(EQM_ARMOR) && !IsQuickMenuButtonDefect(EQM_ARMOR))
		{
			OnQuickMenuDefensePreset();
			m_animQuickMenu.Invoke("makeBlink", EQM_ARMOR);
		}
		filterOut = false;
	}
	else if (action == rGameActions.hud_select2)
	{
		if (m_animRadioButtons.GetVisible()) return false;

		if (m_pClientActor->GetFrozenAmount(true) > 0.0f) return false;
		if (m_pModalHUD == &m_animWeaponAccessories)
			m_animWeaponAccessories.Invoke("toggleAttachment", 2);
		else if (m_pModalHUD == &m_animQuickMenu && !m_iBreakHUD && IsQuickMenuButtonActive(EQM_SPEED) && !IsQuickMenuButtonDefect(EQM_SPEED))
		{
			OnQuickMenuSpeedPreset();
			m_animQuickMenu.Invoke("makeBlink", EQM_SPEED);
		}
		filterOut = false;
	}
	else if (action == rGameActions.hud_select3)
	{
		if (m_animRadioButtons.GetVisible()) return false;

		if (m_pClientActor->GetFrozenAmount(true) > 0.0f) return false;
		if (m_pModalHUD == &m_animWeaponAccessories)
			m_animWeaponAccessories.Invoke("toggleAttachment", 3);
		else if (m_pModalHUD == &m_animQuickMenu && !m_iBreakHUD && IsQuickMenuButtonActive(EQM_STRENGTH) && !IsQuickMenuButtonDefect(EQM_STRENGTH))
		{
			OnQuickMenuStrengthPreset();
			m_animQuickMenu.Invoke("makeBlink", EQM_STRENGTH);
		}
		filterOut = false;
	}
	else if (action == rGameActions.hud_select4)
	{
		if (m_animRadioButtons.GetVisible()) return false;

		if (m_pClientActor->GetFrozenAmount(true) > 0.0f) return false;
		if (m_pModalHUD == &m_animWeaponAccessories)
			m_animWeaponAccessories.Invoke("toggleAttachment", 4);
		else if (m_pModalHUD == &m_animQuickMenu && !m_iBreakHUD && IsQuickMenuButtonActive(EQM_CLOAK) && !IsQuickMenuButtonDefect(EQM_CLOAK))
		{
			OnCloak();
			m_animQuickMenu.Invoke("makeBlink", EQM_CLOAK);
		}
		filterOut = false;
	}
	else if (action == rGameActions.hud_select5)
	{
		if (m_animRadioButtons.GetVisible()) return false;

		if (m_pClientActor->GetFrozenAmount(true) > 0.0f) return false;
		if (m_pModalHUD == &m_animWeaponAccessories)
			m_animWeaponAccessories.Invoke("toggleAttachment", 5);
		else if (m_pModalHUD == &m_animQuickMenu && !m_iBreakHUD && IsQuickMenuButtonActive(EQM_WEAPON) && !IsQuickMenuButtonDefect(EQM_WEAPON))
		{
			OnAction(rGameActions.hud_suit_menu, eIS_Released, 1);
			HandleFSCommand("QuickMenuSwitchWeaponAccessories", NULL);
			m_animQuickMenu.Invoke("makeBlink", EQM_WEAPON);
		}
		filterOut = false;
	}
	else if (action == rGameActions.hud_mptutorial_disable)
	{
		if (CMPTutorial* pTutorial = m_pGameRules ? m_pGameRules->GetMPTutorial() : 0)
		{
			if (pTutorial->IsEnabled())
			{
				gEnv->pConsole->ExecuteString("g_PSTutorial_Enabled 0");
				g_pGame->GetOptions()->SaveCVarToProfile("OptionCfg.g_PSTutorial_Enabled", "0");
			}
		}
		//CryMP
		if (m_animTutorial.IsLoaded() && m_animTutorial.GetVisible())
		{
			m_animTutorial.Invoke("showTutorial", false);
		}
	}

	return filterOut;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ShowObjectives(bool bShow)
{
	if (gEnv->pGame->GetIGameFramework()->GetIViewSystem()->IsPlayingCutScene())
		return;

	m_animObjectivesTab.SetVisible(bShow);
	if (bShow)
	{
		m_animObjectivesTab.Invoke("updateContent");
	}
	HUD_CALL_LISTENERS(OnShowObjectives(bShow));

	if (gEnv->bMultiplayer && m_currentGameRules == EHUD_POWERSTRUGGLE)
	{
		m_animBattleLog.SetVisible(!bShow);
	}

	if (!gEnv->bMultiplayer)
		ShowPDA(bShow, false);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ShowReviveCycle(bool show)
{
	if (show)
	{
		if (!m_animSpawnCycle.IsLoaded())
		{
			m_animSpawnCycle.Load("Libs/UI/HUD_MP_SpawnCircle.gfx", eFD_Center, eFAF_ManualRender | eFAF_Visible);
			SetFlashColor(&m_animSpawnCycle);
		}
		m_fRemainingReviveCycleTime = -1.0f;
	}
	else if (!show && m_animSpawnCycle.IsLoaded())
	{
		m_animSpawnCycle.Unload();
	}
	bool wuff = false;
	m_animSpawnCycle.Invoke("spawnPointLost", wuff);
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::ShowPDA(bool show, bool buyMenu)
{
	if (show && !m_bShow) //don't display map if hud is disabled
		return false;

	if (show && gEnv->pGame->GetIGameFramework()->GetIViewSystem()->IsPlayingCutScene())
		return false;

	if (buyMenu && !m_pHUDPowerStruggle)
		return false;

	if (!m_pGameRules)
		return false;

	if (gEnv->bMultiplayer && m_pGameRules->GetTeamCount() > 1)
	{
		if (m_pGameRules->GetTeam(m_pClientActor->GetEntityId()) == 0) //show team selection
		{
			if (buyMenu)
				return false;

			if (show && (!GetModalHUD() || GetModalHUD() != &m_animTeamSelection))
			{
				m_animTeamSelection.GetFlashPlayer()->SetVisible(true);
				SwitchToModalHUD(&m_animTeamSelection, true);
			}
			else if (GetModalHUD() == &m_animTeamSelection)
			{
				m_animTeamSelection.SetVisible(false);
				SwitchToModalHUD(NULL, false);
			}

			return false;
		}

		if (m_currentGameRules == EHUD_POWERSTRUGGLE ||
			m_currentGameRules == EHUD_TEAMINSTANTACTION)
		{
			if (m_pHUDTeamInstantAction)
			{
				m_pHUDTeamInstantAction->Show(!show);
			}

			if (!buyMenu && show && m_pModalHUD == NULL)
			{
				if (m_pGameRules->GetTeam(m_pClientActor->GetEntityId()) != 0)
					ShowObjectives(true);
			}
			else if (!show)
				ShowObjectives(false);
		}
	}

	if (buyMenu)
	{
		if (!m_animBuyMenu.IsLoaded())
			return false;
		if (m_pModalHUD == &m_animPDA)
			ShowPDA(false, false);

		// call listeners
		FlashRadarType type = EFirstType;
		if (m_pHUDPowerStruggle->m_currentBuyZones.size() > 0)
		{
			IEntity* pFactory = gEnv->pEntitySystem->GetEntity(m_pHUDPowerStruggle->m_currentBuyZones[0]);
			if (pFactory)
			{
				type = m_pHUDRadar->ChooseType(pFactory);
			}
		}

		HUD_CALL_LISTENERS(OnBuyMenuOpen(show, type));
	}
	else
	{
		if (!m_animPDA.IsLoaded())
			return false;
		if (m_pModalHUD == &m_animBuyMenu)
		{
			ShowPDA(false, true);
		}

		HUD_CALL_LISTENERS(OnMapOpen(show));
	}

	if (GetCurrentWeapon() && show)
		GetCurrentWeapon()->StopFire();

	if (show && m_pModalHUD == NULL)
	{
		if (gEnv->bMultiplayer && m_animRadioButtons.GetVisible())
		{
			m_pGameRules->GetRadio()->CancelRadio();
			SetRadioButtons(false);
		}

		if (m_pHUDPowerStruggle)
			m_pHUDPowerStruggle->HideSOM(true);

		if (m_pClientActor->GetHealth() <= 0 && !gEnv->bMultiplayer)
			return false;

		if (IsModalHUDAvailable())
			SwitchToModalHUD(buyMenu ? &m_animBuyMenu : &m_animPDA, true);

		HideInventoryOverview();

		CGameFlashAnimation* anim = buyMenu ? &m_animBuyMenu : &m_animPDA;
		anim->SetVisible(true);

		if (!buyMenu)
		{
			m_pHUDRadar->SetRenderMapOverlay(true);
			anim->Invoke("setDisconnect", (m_bNoMiniMap || m_pHUDRadar->GetJamming() > 0.5f) ? true : false);
			if (m_pGameRules->GetTeamId("black") == m_pGameRules->GetTeam(m_pClientActor->GetEntityId()))
				anim->SetVariable("PlayerTeam", SFlashVarValue("US"));
			else
				anim->SetVariable("PlayerTeam", SFlashVarValue("KOREAN"));

			anim->SetVariable("SpectatorMode", SFlashVarValue(m_pClientActor->GetSpectatorMode() != CActor::eASM_None));
			anim->SetVariable("GameRules", SFlashVarValue(m_pGameRules->GetEntity()->GetClass()->GetName()));

			if (!m_pClientActor || m_pGameRules->GetTeam(m_pClientActor->GetEntityId()) != 0)
				ShowObjectives(true);

		}
		else
		{
			//m_animBuyZoneIcon.SetVisible(m_pHUDPowerStruggle->m_bInBuyZone&&m_pModalHUD!=&m_animBuyMenu);
			m_buyMenuKeyLog.m_state = SBuyMenuKeyLog::eBMKL_Tab;
		}

		SetFlashColor(anim);
		anim->Invoke("showPDA", gEnv->bMultiplayer);
		if (buyMenu)
		{
			anim->GetFlashPlayer()->Advance(0.1f);
			m_pHUDPowerStruggle->DetermineCurrentBuyZone(true);
		}
		anim->CheckedInvoke("destroy", m_iBreakHUD);

		if (m_pClientActor->GetPlayerInput())
			m_pClientActor->GetPlayerInput()->DisableXI(true);

		PlaySound(ESound_MapOpen);
		HUD_CALL_LISTENERS(PDAOpened());

		return true;
	}
	else if (!show && ((buyMenu) ? m_pModalHUD == &m_animBuyMenu : m_pModalHUD == &m_animPDA))
	{
		SwitchToModalHUD(NULL, false);

		if (IPlayerInput* pPlayerInput = m_pClientActor->GetPlayerInput())
			pPlayerInput->DisableXI(false);

		if (buyMenu)
		{
			const SFlashVarValue cVal(gEnv->bMultiplayer ? "MP" : "SP");
			m_animBuyMenu.Invoke("hidePDA", &cVal, 1);
			m_buyMenuKeyLog.Clear();
			m_pHUDPowerStruggle->UpdateLastPurchase();
		}
		else
		{
			m_animPDA.SetVisible(false);
			m_pHUDRadar->SetRenderMapOverlay(false);
			m_pHUDRadar->SetDrag(false);
			m_bMiniMapZooming = false;
		}

		if (m_pHUDPowerStruggle)
			m_pHUDPowerStruggle->HideSOM(false);

		PlaySound(ESound_MapClose);
		HUD_CALL_LISTENERS(PDAClosed());

		if (gEnv->bMultiplayer && m_currentGameRules == EHUD_POWERSTRUGGLE)
			m_animBattleLog.SetVisible(true);

		return false;
	}
	return false;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnHardwareMouseEvent(int iX, int iY, EHARDWAREMOUSEEVENT eHardwareMouseEvent)
{
	if (!m_isMouseCursorVisible)
	{
		return;
	}

	SFlashCursorEvent::ECursorState eCursorState = SFlashCursorEvent::eCursorMoved;
	if (HARDWAREMOUSEEVENT_LBUTTONDOUBLECLICK == eHardwareMouseEvent)
	{
		if (m_pModalHUD)
		{
			m_pModalHUD->CheckedInvoke("DoubleClick");
		}

		//CryMP:
		eCursorState = SFlashCursorEvent::eCursorPressed;
	}
	else if (HARDWAREMOUSEEVENT_LBUTTONDOWN == eHardwareMouseEvent)
	{
		eCursorState = SFlashCursorEvent::eCursorPressed;
	}
	else if (HARDWAREMOUSEEVENT_LBUTTONUP == eHardwareMouseEvent)
	{
		eCursorState = SFlashCursorEvent::eCursorReleased;
	}

	if (m_pModalHUD)
	{
		int x(iX), y(iY);
		m_pModalHUD->GetFlashPlayer()->ScreenToClient(x, y);
		m_pModalHUD->GetFlashPlayer()->SendCursorEvent(SFlashCursorEvent(eCursorState, x, y));
	}

	// Note: this one is special, it overrides all the others
	if (m_animScoreBoard.IsLoaded())
	{
		int x(iX), y(iY);
		m_animScoreBoard.GetFlashPlayer()->ScreenToClient(x, y);
		m_animScoreBoard.GetFlashPlayer()->SendCursorEvent(SFlashCursorEvent(eCursorState, x, y));
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::GetGPSPosition(wchar_t* szN, wchar_t* szW)
{
	const float fMultiplier = 10.0f;

	// Easter egg: Coburg GPS as the origin!

	Vec3 vPos = gEnv->pSystem->GetViewCamera().GetPosition();

	int iN = 50155927 + (int)(vPos.x * fMultiplier);
	int iW = 10581721 + (int)(vPos.y * fMultiplier);

	// FIXME: I don't remember which one, but I'm pretty sure there is a function to do that properly

	int iN1 = iN / 1000000;
	int iN2 = (iN - iN1 * 1000000) / 10000;
	int iN3 = (iN - iN1 * 1000000 - iN2 * 10000) / 100;
	int iN4 = (iN - iN1 * 1000000 - iN2 * 10000 - iN3 * 100);

	int iW1 = iW / 1000000;
	int iW2 = (iW - iW1 * 1000000) / 10000;
	int iW3 = (iW - iW1 * 1000000 - iW2 * 10000) / 100;
	int iW4 = (iW - iW1 * 1000000 - iW2 * 10000 - iW3 * 100);

	wstring strNorth = LocalizeWithParams("@ui_N");
	wstring strWest = LocalizeWithParams("@ui_W");

	CrySwprintf(szN, 32, L"%.2d\"%.2d'%.2d.%.2d %s", iN1, iN2, iN3, iN4, strNorth.c_str());
	CrySwprintf(szW, 32, L"%.2d\"%.2d'%.2d.%.2d %s", iW1, iW2, iW3, iW4, strWest.c_str());
}

//-----------------------------------------------------------------------------------------------------

void CHUD::TickBattleStatus(float fValue)
{
	m_fBattleStatus += fValue;
	m_fBattleStatusDelay = g_pGameCVars->g_combatFadeTimeDelay;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UpdateBattleStatus()
{
	float delta = gEnv->pTimer->GetFrameTime();
	m_fBattleStatusDelay -= delta;
	if (m_fBattleStatusDelay <= 0)
	{
		m_fBattleStatus -= delta / (g_pGameCVars->g_combatFadeTime + 1.0f);
		m_fBattleStatusDelay = 0;
	}
	m_fBattleStatus = CLAMP(m_fBattleStatus, 0.0f, 1.0f);
}

void CHUD::UpdateBuyMenuPages()
{
	if (m_pHUDPowerStruggle && m_pModalHUD == &m_animBuyMenu)
	{
		m_pHUDPowerStruggle->DetermineCurrentBuyZone(true);
	}
}

//-----------------------------------------------------------------------------------------------------

float CHUD::GetBattleRange()
{
	return g_pGameCVars->g_battleRange;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::GetProjectionScale(CGameFlashAnimation* pGameFlashAnimation, float* pfScaleX, float* pfScaleY, float* pfHalfUselessSize)
{
	float fMovieWidth = (float)pGameFlashAnimation->GetFlashPlayer()->GetWidth();
	float fMovieHeight = (float)pGameFlashAnimation->GetFlashPlayer()->GetHeight();

	float fRendererWidth = (float)m_pRenderer->GetWidth();
	float fRendererHeight = (float)m_pRenderer->GetHeight();

	*pfScaleX = (fMovieHeight / 100.0f) * fRendererWidth / fRendererHeight;
	*pfScaleY = (fMovieHeight / 100.0f);

	float fScale = fMovieHeight / fRendererHeight;
	float fUselessSize = fMovieWidth - fRendererWidth * fScale;

	*pfHalfUselessSize = fUselessSize * 0.5f;
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::WeaponHasAttachments()
{
	CWeapon* pCurrentWeapon = GetCurrentWeapon();
	if (!pCurrentWeapon)
		return false;

	const auto& helpers = pCurrentWeapon->GetAttachmentHelpers();
	for (const auto& helper : helpers)
	{
		if (helper.slot != CItem::eIGS_FirstPerson)
			continue;

		if (!pCurrentWeapon->HasAttachmentAtHelper(helper.name.c_str()))
			continue;

		const std::vector<std::string> attachments = pCurrentWeapon->GetAttachmentsAtHelper(helper.name.c_str());
		if (attachments.empty())
			continue;

		const bool isSpecial = helper.name == "magazine" ||
			helper.name == "attachment_front" ||
			helper.name == "energy_source_helper" ||
			helper.name == "shell_grenade";

		int potentialButtons = 0;

		if (!isSpecial)
		{
			++potentialButtons; // NoAttachment
		}

		for (const auto& attachment : attachments)
		{
			if (attachment != "GrenadeShell")
			{
				++potentialButtons;
			}
		}

		if (potentialButtons >= 2)
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnPostUpdate(float frameTime)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if (m_bStopCutsceneNextUpdate)
	{
#ifdef USER_alexl
		CryLogAlways("[CX] Frame=%d Aborting Cutscenes", gEnv->pRenderer->GetFrameID(false));
#endif
		if (gEnv->pMovieSystem)
		{
			ISequenceIt* pSeqIt = gEnv->pMovieSystem->GetSequences(true, true);
			IAnimSequence* pSeq = pSeqIt->first();
			while (pSeq)
			{
#ifdef USER_alexl
				CryLogAlways("[CX] Frame=%d Aborting Cutscene 0x%p '%s'", gEnv->pRenderer->GetFrameID(false), pSeq, pSeq->GetName());
#endif
				gEnv->pMovieSystem->AbortSequence(pSeq, false);
				pSeq = pSeqIt->next();
			}
			pSeqIt->Release();
		}
		m_bStopCutsceneNextUpdate = false;
		m_fCutsceneSkipTimer = 0.0f;
	}
	if (m_bCutscenePlaying && m_fCutsceneSkipTimer > 0.0f)
	{
		m_fCutsceneSkipTimer -= frameTime;
	}

	int width = gEnv->pRenderer->GetWidth();
	int height = gEnv->pRenderer->GetHeight();
	if (width != m_width || height != m_height)
	{
		UpdateRatio();
	}

	if (gEnv->bMultiplayer)
	{
		//UpdateTeamActionHUD(); //No TeamAction supported.. yet? :D

		//MP team selection is always rendered if available
		if (!m_bInMenu && !IsScoreboardActive() && m_animTeamSelection.GetVisible())
		{
			m_animTeamSelection.GetFlashPlayer()->Advance(frameTime);
			m_animTeamSelection.GetFlashPlayer()->Render();
		}
	}

	if (g_pGameCVars->cl_hud <= 0 || m_cineHideHUD)
	{
		// possibility to show the binoculars during cutscene
		if (!m_bInMenu && m_cineHideHUD && m_pHUDScopes->IsBinocularsShown() && m_pHUDScopes->m_bShowBinocularsNoHUD && m_pHUDScopes->m_animBinoculars.GetVisible())
		{
			m_pHUDScopes->DisplayBinoculars(m_pClientActor);
			m_pHUDScopes->m_animBinoculars.GetFlashPlayer()->Advance(frameTime);
			m_pHUDScopes->m_animBinoculars.GetFlashPlayer()->Render();
		}

		// Even if HUD is off Fader must be able to function if cl_hud is 0.
		if (!m_externalHUDObjectList.empty() && (g_pGameCVars->cl_hud == 0 || m_cineHideHUD))
		{
			m_pUIDraw->PreRender();
			for (THUDObjectsList::iterator iter = m_externalHUDObjectList.begin(); iter != m_externalHUDObjectList.end(); ++iter)
			{
				(*iter)->Update(frameTime);
			}
			m_pHUDRadar->Update(frameTime);
			m_pUIDraw->PostRender();
		}

		UpdateCinematicAnim(frameTime);
		if (g_pGame->GetIGameFramework()->IsGamePaused() == false)
			UpdateSubtitlesAnim(frameTime);

		if (m_delayedMessage.empty() == false)
		{
			DisplayOverlayFlashMessage(m_delayedMessage.c_str());
			UpdateWarningMessages(gEnv->pTimer->GetFrameTime());
			m_delayedMessage.resize(0);
		}
		return;
	}

	if (gEnv->bMultiplayer)
	{
		bool noConnectivity = !gEnv->pNetwork->HasNetworkConnectivity();
		bool highLatency = false;

		INetChannel* pNetChannel = g_pGame->GetIGameFramework()->GetClientChannel();
		if (pNetChannel)
		{
			highLatency = pNetChannel->IsSufferingHighLatency(gEnv->pTimer->GetAsyncTime());
		}

		if (noConnectivity || highLatency)
		{
			if (!m_animNetworkConnection.GetVisible())
			{
				m_animNetworkConnection.SetVisible(true);

				if (!gEnv->bServer)
					m_pClientActor->SufferingHighLatency(true);
			}

			/*
			if (pNetChannel)
			{
				const char* descr = noConnectivity ? "No Connectivity" : "High Latency";
				const float up = pNetChannel->GetStatistics().bandwidthUp / 1000.f;
				const float down = pNetChannel->GetStatistics().bandwidthDown / 1000.f;

				const float y = 580.f;
				const float x = 250.f;
				const float sy = gEnv->pRenderer->ScaleCoordY(y);
				const float sx = gEnv->pRenderer->ScaleCoordX(x + x * 0.5f);
				const auto ct = g_pGameCVars->hud_colorOver;
				const float r = ((ct >> 16) & 0xFF) / 255.0f;
				const float g = ((ct >> 8) & 0xFF) / 255.0f;
				const float b = ((ct >> 0) & 0xFF) / 255.0f;

				float color[] = { r, g, b, 1.0 };
				const float size = 1.0f + (width / 800.f) * 0.3f;

				gEnv->pRenderer->Draw2dLabel(sx, sy, size, color, false, "%s: Up %4.1fk/s  Down %4.1fk/s", descr, up, down);
			}
			*/
		}
		else
		{
			if (m_animNetworkConnection.GetVisible())
			{
				m_animNetworkConnection.SetVisible(false);

				if (!gEnv->bServer)
					m_pClientActor->SufferingHighLatency(false);
			}
		}
	}
	else if (m_animNetworkConnection.GetVisible())
		m_animNetworkConnection.SetVisible(false);

	if (!gEnv->pGame->GetIGameFramework()->IsGameStarted())
	{
		// Modal dialog box must be always rendered last
		UpdateWarningMessages(frameTime);
		return;
	}

	//Updates all timer-based triggers
	bool loadedGame = UpdateTimers(frameTime);
	if (loadedGame)
		return;

	//updates ammo display
	UpdatePlayerAmmo();

	if (m_pModalHUD == &m_animQuickMenu)
	{
		const SPlayerStats* stats = m_pClientActor->GetPlayerStats();
		if ((stats->bSprinting || (m_pClientActor->GetStance() == STANCE_PRONE && stats->speedFlat > 0.01f)) || (GetCurrentWeapon() && GetCurrentWeapon()->IsBusy()))
		{
			if (IsQuickMenuButtonActive(EQM_WEAPON))	//we are sprinting, but the weapon button is not disabled yet
				ActivateQuickMenuButton(EQM_WEAPON, false);
		}
		else //we disabled the weapon button, but stopped sprinting
			ActivateQuickMenuButton(EQM_WEAPON, WeaponHasAttachments());
	}

	if (m_pModalHUD == &m_animPDA)
	{
		if (m_pGameRules)
		{
			if (g_pGame && g_pGame->GetIGameFramework())
			{
				bool isDead = m_pGameRules->IsDead(g_pGame->GetIGameFramework()->GetClientActorId());
				m_animPDA.Invoke("setPlayerIsDead", isDead);
			}
		}
	}

	if (!m_bInMenu)
	{
		//////////////////////////////////////////////////////////////////////////
		// update internals
		UpdateBattleStatus();
		//////////////////////////////////////////////////////////////////////////

		if (m_pGameRules)
		{
			if (m_animSpawnCycle.IsLoaded())
			{
				//get players that will spawn with you
				static std::vector<string> players;
				players.resize(0);

				int clientTeam = m_pGameRules->GetTeam(m_pClientActor->GetEntityId());
				EntityId spawnID = m_pGameRules->GetPlayerSpawnGroup(m_pClientActor);

				if (spawnID)
				{
					static std::vector<EntityId> teamMates;
					teamMates.resize(0);

					m_pGameRules->GetTeamPlayers(clientTeam, teamMates);

					std::vector<EntityId>::const_iterator it = teamMates.begin();
					std::vector<EntityId>::const_iterator end = teamMates.end();

					IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();

					for (;it != end;++it)
					{
						CActor* pTempActor = static_cast<CActor*>(pActorSystem->GetActor(*it));
						if (pTempActor && pTempActor != m_pClientActor)
						{
							if ((pTempActor->GetHealth() <= 0) && (m_pGameRules->GetPlayerSpawnGroup(pTempActor) == spawnID))
								players.push_back(pTempActor->GetEntity()->GetName());
						}
					}
				}

				int size = players.size();

				std::vector<const char*> pushArray;
				pushArray.reserve(size);
				for (int i(0); i < size; ++i)
				{
					pushArray.push_back(players[i].c_str());
				}
				m_animSpawnCycle.GetFlashPlayer()->SetVariableArray(FVAT_ConstStrPtr, "m_players", 0, &pushArray[0], size);

				float remaining = m_pGameRules->GetRemainingReviveCycleTime();
				// Because of net lag, it can happens that the time value is reset from 0 g_revivetime
				// To prevent this, just ignore these values (it should be a matter of a second)
				if (-1.0f == m_fRemainingReviveCycleTime)
					m_fRemainingReviveCycleTime = remaining;
				if (remaining > m_fRemainingReviveCycleTime)
					remaining = 0.0f;
				else
					m_fRemainingReviveCycleTime = remaining;
				SFlashVarValue args[2] = { g_pGameCVars->g_revivetime, remaining };
				m_animSpawnCycle.Invoke("setSeconds", args, 2);
			}
		}
	}

	if (m_bFirstFrame)
	{
		// FIXME: remove setalpha(0) from all files and remove this block
		for (CGameFlashAnimation *pAnim : m_gameFlashAnimationsList)
		{
			if (!(pAnim->GetFlags() & eFAF_ManualRender))
				pAnim->CheckedInvoke("setAlpha", 1.0f);
		}
	}

	if (m_bDestroyInitializePending)
	{
		m_animInitialize.Unload();
		m_bDestroyInitializePending = false;
	}
	else if (m_animInitialize.IsLoaded())
	{
		if (!m_bInMenu && m_animInitialize.GetVisible())
		{
			m_animInitialize.GetFlashPlayer()->Advance(frameTime);
			m_animInitialize.GetFlashPlayer()->Render();
		}
	}

	//Scopes moved

	if (m_bShow && (m_cineState == eHCS_None || gEnv->bMultiplayer) && m_pClientActor && !m_bInMenu)
	{
		// Weapon Modification
		UpdateWeaponModify();

		UpdateVoiceChat();

		UpdateCrosshairVisibility();

		UpdateProgressBar(frameTime);

		// Target autoaim and locking
		Targetting(0, false);

		UpdateHealth();

		// Grenade detector
		TrackProjectiles(m_pClientActor);
		TrackRadioMessages(m_pClientActor);

		// Binoculars and Scope
		if (m_pHUDScopes->IsBinocularsShown())
			m_pHUDScopes->DisplayBinoculars(m_pClientActor);
		else if (m_pHUDScopes->GetCurrentScope() >= 0)
			m_pHUDScopes->DisplayScope(m_pClientActor);

		DrawAirstrikeTargetMarkers();

		if (m_pHUDVehicleInterface->GetHUDType() != CHUDVehicleInterface::EHUD_NONE)
			m_pHUDVehicleInterface->ShowVehicleInterface(m_pHUDVehicleInterface->GetHUDType());

		if (!m_bThirdPerson)
		{
			// Scopes should be the very first thing to be drawn
			m_pHUDScopes->Update(frameTime);
		}

		m_pHUDVehicleInterface->Update(frameTime);

		m_pHUDTagNames->Update();

		m_pHUDSilhouettes->Update(frameTime);

		//*****************************************************
		//render flash animation

		if (m_animSpawnCycle.IsLoaded() && !GetModalHUD() && m_pClientActor->GetHealth() <= 0)
		{
			m_animSpawnCycle.GetFlashPlayer()->Advance(frameTime);
			m_animSpawnCycle.GetFlashPlayer()->Render();
		}

		if (m_animAirStrike.IsLoaded() && !GetModalHUD())
		{
			m_animAirStrike.GetFlashPlayer()->Advance(frameTime);
			m_animAirStrike.GetFlashPlayer()->Render();
		}

		if (gEnv->bMultiplayer && m_pClientActor->GetSpectatorMode()) //SPECTATOR Mode
		{
			if (!m_animSpectate.IsLoaded())
			{
				m_animSpectate.Load("Libs/UI/HUD_Spectate.gfx", eFD_Center, eFAF_Visible | eFAF_ManualRender);

				FadeCinematicBars(3);

				// SNH: moved text setting to further down (with player name display)
				//	as text changes based on current spectator mode.
			}

			const uint8 specMode = m_pClientActor->GetSpectatorMode();
			if (specMode >= CActor::eASM_FirstMPMode && specMode <= CActor::eASM_LastMPMode)
			{
				CPlayer* pSpectatorTarget = CPlayer::FromIActor(m_pClientActor->GetSpectatorTargetActor());
				CheckSpectatorTarget(pSpectatorTarget, frameTime);

				m_animSpectate.GetFlashPlayer()->Advance(frameTime);
				m_animSpectate.GetFlashPlayer()->Render();

				if (m_animNetworkConnection.GetVisible())
				{
					m_animNetworkConnection.GetFlashPlayer()->Advance(frameTime);
					m_animNetworkConnection.GetFlashPlayer()->Render();
				}

				UpdateSpectator(pSpectatorTarget, frameTime);
			}

			if (m_animPDA.GetVisible())
			{
				m_animPDA.GetFlashPlayer()->Advance(frameTime);
				m_animPDA.GetFlashPlayer()->Render();
			}
			if (m_animChat.GetVisible())
			{
				m_animChat.GetFlashPlayer()->Advance(frameTime);
				m_animChat.GetFlashPlayer()->Render();
			}
			if (m_animScoreBoard.GetVisible())
			{
				m_animScoreBoard.GetFlashPlayer()->Advance(frameTime);
				m_animScoreBoard.GetFlashPlayer()->Render();
			}
			if (m_animBuyMenu.GetVisible())
			{
				m_animBuyMenu.GetFlashPlayer()->Advance(frameTime);
				m_animBuyMenu.GetFlashPlayer()->Render();
			}
			if (m_animBattleLog.GetVisible()) //CryMP: battlelog in spectatemode
			{
				m_animBattleLog.GetFlashPlayer()->Advance(frameTime);
				m_animBattleLog.GetFlashPlayer()->Render();
			}
			if (m_animKillLog.GetVisible()) //CryMP: killLog in spectatemode
			{
				m_animKillLog.GetFlashPlayer()->Advance(frameTime);
				m_animKillLog.GetFlashPlayer()->Render();
			}
			if (m_pHUDPowerStruggle && !m_pGameRules->IsNeutral(m_pClientActor->GetEntityId()))
			{
				if (m_animPlayerPP.GetVisible()) //CryMP: playerPP while dead
				{
					m_animPlayerPP.GetFlashPlayer()->Advance(frameTime);
					m_animPlayerPP.GetFlashPlayer()->Render();
				}
			}
			if (m_animFriendlyProjectileTracker.GetVisible()) //CryMP: grenade indicator in spectator mode
			{
				m_animFriendlyProjectileTracker.GetFlashPlayer()->Advance(frameTime);
				m_animFriendlyProjectileTracker.GetFlashPlayer()->Render();
			}
			if (m_animHostileProjectileTracker.GetVisible())
			{
				m_animHostileProjectileTracker.GetFlashPlayer()->Advance(frameTime); //CryMP: grenade indicator in spectator mode
				m_animHostileProjectileTracker.GetFlashPlayer()->Render();
			}
			if (m_animMissionObjective.GetVisible() && m_bShowAllOnScreenObjectives) //CryMP: for names in mp spectatemode, update mission objective!
			{
				m_animMissionObjective.GetFlashPlayer()->Advance(frameTime);
				m_animMissionObjective.GetFlashPlayer()->Render();
			}
			if (m_animTrackedRadioMessage.GetVisible()) //CryMP: tracked radio messages
			{
				m_animTrackedRadioMessage.GetFlashPlayer()->Advance(frameTime);
				m_animTrackedRadioMessage.GetFlashPlayer()->Render();
			}
		}
		else
		{
			m_prevSpectatorHealth = 0;
			m_prevSpectatorMode = 0;
			m_prevSpectatorTarget = 0;

			if (m_animSpectate.IsLoaded())
			{
				m_animSpectate.Unload();
				FadeCinematicBars(0);
			}

			CreateInterference();

			for (CGameFlashAnimation *pAnim : m_gameFlashAnimationsList)
			{
				if (pAnim->GetFlags() & eFAF_ManualRender)
					continue;

				if (pAnim->GetVisible())
				{
					pAnim->GetFlashPlayer()->Advance(frameTime);
					pAnim->GetFlashPlayer()->Render();
				}
			}
		}

		// Autosnap
		if (m_bAutosnap)
			AutoSnap();

		// Non flash stuff

		for (THUDObjectsList::iterator iter = m_hudObjectsList.begin(); iter != m_hudObjectsList.end(); ++iter)
		{
			(*iter)->PreUpdate();
		}

		m_pUIDraw->PreRender();

		for (THUDObjectsList::iterator iter = m_hudObjectsList.begin(); iter != m_hudObjectsList.end(); ++iter)
		{
			(*iter)->Update(frameTime);
		}

		if (m_bShowGODMode && strcmp(m_strGODMode, ""))
		{
			int fading = g_pGameCVars->hud_godFadeTime;
			float time = gEnv->pTimer->GetAsyncTime().GetSeconds();
			if (fading == -1 || (time - m_fLastGodModeUpdate < fading))
			{
				float alpha = 0.75f;
				if (fading >= 2)
				{
					if (time - m_fLastGodModeUpdate < 0.75f)		//fade in
						alpha = (time - m_fLastGodModeUpdate);
					else if (time - m_fLastGodModeUpdate > (float(fading) - 0.75f))		//fade out
						alpha -= ((time - m_fLastGodModeUpdate) - (float(fading) - 0.75f));
				}
				m_pUIDraw->DrawText(m_pDefaultFont, 10, 60, 0, 0, m_strGODMode, alpha, 1, 1, 1, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP);
				//debugging : render number of deaths in God mode ...
				if (!strcmp(m_strGODMode, "GOD") || !strcmp(m_strGODMode, "Team GOD"))
				{
					string died("You died ");
					char aNumber[5];
					_itoa(m_iDeaths, aNumber, 10);
					died.append(aNumber);
					died.append(" times.");
					m_pUIDraw->DrawText(m_pDefaultFont, 10, 80, 0, 0, died.c_str(), alpha, 1, 1, 1, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP);
				}
			}
		}

		m_pUIDraw->PostRender();
	}
	else if (!m_bInMenu && m_pClientActor)
	{
		// TODO: add to list and use GetVisible()
		if (m_animScoreBoard.IsLoaded())
		{
			if (!m_bShow && (GetModalHUD() == &m_animScoreBoard))	//render sniper scope also when in scoreboard mode
			{
				if (m_pHUDScopes->GetCurrentScope() == CHUDScopes::ESCOPE_SNIPER)
				{
					m_pHUDScopes->DisplayScope(m_pClientActor);
					m_pHUDScopes->Update(frameTime);
				}
				//CryMP: also render chat in scoreboard
				if (m_animChat.GetVisible())
				{
					m_animChat.GetFlashPlayer()->Advance(frameTime);
					m_animChat.GetFlashPlayer()->Render();

					if (m_pHUDTextChat)
						m_pHUDTextChat->Update(frameTime);
				}
			}

			if (gEnv->bMultiplayer)
			{
				if (m_pGameRules->GetCurrentStateId() == 3/*InGame*/ &&
					m_pGameRules->IsTimeLimited())
				{
					const int time = static_cast<int>(m_pGameRules->GetRemainingGameTime());
					const int minutes = time / 60;
					const int seconds = time - (minutes * 60);
					std::string msg = StringTools::Format("%02d:%02d", minutes, seconds);
					m_animScoreBoard.Invoke("setCountdown", msg.c_str());
				}
				else
				{
					m_animScoreBoard.Invoke("setCountdown", "");
				}
			}
			m_animScoreBoard.GetFlashPlayer()->Advance(frameTime);
			m_animScoreBoard.GetFlashPlayer()->Render();
			m_pHUDScore->Render();
		}
	}

	if (!m_bInMenu && !m_externalHUDObjectList.empty())
	{
		m_pUIDraw->PreRender();
		for (THUDObjectsList::iterator iter = m_externalHUDObjectList.begin(); iter != m_externalHUDObjectList.end(); ++iter)
		{
			(*iter)->Update(frameTime);
		}
		m_pUIDraw->PostRender();
	}

	if (m_bInMenu && m_pClientActor)
	{
		if (m_pHUDScopes->GetCurrentScope() == CHUDScopes::ESCOPE_SNIPER)
		{
			m_pHUDScopes->DisplayScope(m_pClientActor);
			m_pHUDScopes->Update(frameTime);
		}
	}

	// update cinematic bars
	UpdateCinematicAnim(frameTime);

	// update subtitles
	UpdateSubtitlesAnim(frameTime);

	if (m_bFirstFrame)
	{
		EnergyChanged(m_pNanoSuit->GetSuitEnergy());
		m_bFirstFrame = false;
	}
	m_fSuitEnergy = m_pNanoSuit->GetSuitEnergy();
	m_iVoiceMode = g_pGameCVars->hud_voicemode;

	if (m_delayedMessage.empty() == false)
	{
		DisplayOverlayFlashMessage(m_delayedMessage.c_str());
		m_delayedMessage.resize(0);
	}

	// Modal dialog box must be always rendered last
	UpdateWarningMessages(frameTime);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UpdateSpectator(CPlayer* pSpectatorTarget, float frameTime)
{
	const uint8 specMode = m_pClientActor->GetSpectatorMode();
	bool changedTarget = false;
	if (m_prevSpectatorMode != specMode || m_prevSpectatorTarget != m_pClientActor->GetSpectatorTarget())
	{
		m_prevSpectatorMode = specMode;
		m_prevSpectatorTarget = m_pClientActor->GetSpectatorTarget();

		wstring mapText, functionalityText;
		// don't want the 'press m to...' text if waiting to respawn
		if (!m_pGameRules->IsPlayerActivelyPlaying(m_pClientActor->GetEntityId()))
		{
			if (m_currentGameRules == EHUD_POWERSTRUGGLE)
			{
				mapText = LocalizeWithParams("@ui_open_map");
			}
			else
			{
				mapText = LocalizeWithParams("@ui_open_map_dm");
			}

			// second line of text depends on current spectator mode
			if (m_pClientActor->GetSpectatorMode() == CActor::eASM_Follow)
			{
				functionalityText = LocalizeWithParams("@ui_spectate_functionality_crymp");
			}
			else
			{
				functionalityText = LocalizeWithParams("@ui_spectate_functionality");
			}
		}
		else
		{
			//CryMP: Fix team selection still showing while actively playing
			if (GetModalHUD() == &m_animTeamSelection)
			{
				m_animTeamSelection.SetVisible(false);
				SwitchToModalHUD(NULL, false);
			}

			// waiting to respawn - must be in 3rd person mode. Just show 'press left/right to switch player'
			mapText = L"";
			functionalityText = LocalizeWithParams("@ui_spectate_functionality_dead");
		}
		SFlashVarValue textArgs[3] = { mapText.c_str(), functionalityText.c_str(), true };
		m_animSpectate.Invoke("setText", textArgs, 3);

		if (specMode == CActor::eASM_Follow && m_pClientActor->GetSpectatorTarget() != 0)
		{
			IActor* pTarget = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_pClientActor->GetSpectatorTarget());
			if (pTarget)
			{
				SFlashVarValue args[2] = { pTarget->GetEntity()->GetName(),m_pGameRules->GetTeam(pTarget->GetEntityId()) };
				m_animSpectate.Invoke("setPlayer", args, 2);

				changedTarget = true;
			}

			m_animSpectate.Invoke("clearServerInfo");
		}
		else
		{
			// reset player name / flag when going back to free camera / fixed camera
			SFlashVarValue args[2] = { "", 0 };
			m_animSpectate.Invoke("setPlayer", args, 2);
			m_animSpectate.Invoke("clearPlayerInfo");

			//CryMP begin
			const ServerInfo& server = gClient->GetServerConnector()->GetLastServer();

			if (!server.public_host.empty() && server.public_port)
			{
				const std::string text = StringTools::Format("%s (%s:%hu)",
					server.name.c_str(),
					server.public_host.c_str(),
					server.public_port
				);

				SFlashVarValue sArgs[1] = { text.c_str() };
				m_animSpectate.Invoke("setServerInfo", sArgs, 1);
			}
			//CryMP end
		}
	}

	if (pSpectatorTarget)
	{
		ILocalizationManager* pLoc = gEnv->pSystem->GetLocalizationManager();
		if (pLoc)
		{
			const int KEY_PING = 103;
			int ping = 0;
			m_pGameRules->GetSynchedEntityValue(m_pClientActor->GetSpectatorTarget(), KEY_PING, ping);

			wstring tmp, tmp2;
			gEnv->pSystem->GetLocalizationManager()->LocalizeLabel("@ui_mp_PING", tmp);
			tmp2.Format(L": %d", ping);
			tmp += tmp2;

			SFlashVarValue sArgs[1] = { tmp.c_str() };
			m_animSpectate.Invoke("setPing", sArgs, 1);

			//These values never change
			if (changedTarget)
			{
				const int channelId = pSpectatorTarget->GetChannelId();
				const std::string channelInfo = StringTools::Format("CHANNEL: %d", channelId);
				SFlashVarValue sArgs2[1] = { channelInfo.c_str() };
				m_animSpectate.Invoke("setChannel", sArgs2, 1);

				if (IScriptTable * pScriptTable = pSpectatorTarget->GetEntity()->GetScriptTable())
				{
					const char* info;
					if (pScriptTable->GetValue("SPECTATOR_INFO", info))
					{
						if (strlen(info))
						{
							SFlashVarValue sArgs3[1] = { info };
							m_animSpectate.Invoke("setCountry", sArgs3, 1);
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnSaveGame(ISaveGame* pSaveGame)
{
}


//-----------------------------------------------------------------------------------------------------

void CHUD::OnActionEvent(const SActionEvent& event)
{
	if (event.m_event == eAE_preSaveGame)
	{
		if (gEnv->bEditor || g_pGame->GetMenu()->WaitingForStart() == false)
		{
			// this will make sure we actually display it BEFORE the short stall due to saving
			const ESaveGameReason reason = (ESaveGameReason)event.m_value;
			if (reason == eSGR_FlowGraph)
				m_delayedMessage = "@checkpoint";
			else
				m_delayedMessage = "@game_saving";
		}
	}
	if (event.m_event == eAE_postSaveGame)
	{
		if (gEnv->bEditor || g_pGame->GetMenu()->WaitingForStart() == false)
		{
			m_delayedMessage.resize(0);
			const ESaveGameReason reason = (ESaveGameReason)event.m_value;
			bool bSuccess = event.m_description != 0;
			if (bSuccess)
			{
				if (reason == eSGR_FlowGraph)
					DisplayOverlayFlashMessage("@game_saved");
				else
					DisplayOverlayFlashMessage("@game_saved");
				PlaySound(ESound_GameSaved);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::BreakHUD(int state)
{
	m_iBreakHUD = state;
	m_pHUDCrosshair->Break(state ? true : false);

	if (state == 1)
	{
		if (!m_animBreakHUD.IsLoaded())
			m_animBreakHUD.Reload();
		m_animBreakHUD.Invoke("gotoAndPlay", 1);
	}
	else
	{
		if (m_animBreakHUD.IsLoaded())
			m_animBreakHUD.Unload();
	}

	if (!m_iBreakHUD)		//remove some obsolete stuff
	{
		m_animKillAreaWarning.Unload();
		m_animDeathMessage.Unload();
	}

	for (CGameFlashAnimation *pAnim : m_gameFlashAnimationsList)
	{
		if (pAnim->GetFlags() & eFAF_ManualRender || pAnim->GetFlags() & eFAF_SkipBreak)
			continue;

		pAnim->CheckedInvoke("destroy", state);
	}

	if (m_pHUDVehicleInterface->m_animStats.IsLoaded())
	{
		m_pHUDVehicleInterface->m_animStats.Invoke("destroy", state);
		if (m_pHUDVehicleInterface->m_animMainWindow.IsLoaded())
			m_pHUDVehicleInterface->m_animMainWindow.Invoke("destroy", state);
	}

	if (m_pHUDPowerStruggle)
		m_pHUDPowerStruggle->m_animSwingOMeter.CheckedInvoke("destroy", state);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::RebootHUD()
{
	if (m_iBreakHUD)
		BreakHUD(false);

	for (CGameFlashAnimation *pAnim : m_gameFlashAnimationsList)
	{
		if (pAnim->GetFlags() & eFAF_ManualRender)
			continue;

		pAnim->CheckedInvoke("reboot");
	}
	m_animRebootHUD.SetVisible(true);
	m_animRebootHUD.Invoke("gotoAndPlay", 1);
	m_fLastReboot = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	m_bShow = true; // make sure we show the HUD
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetTeamDisplay(std::string team)
{
	//CryMP: unused
	/*
	if (m_animHexIcons.IsLoaded())
	{
		if (m_currentGameRules == EHUD_POWERSTRUGGLE)
		{
			m_animHexIcons.Invoke("setBackground", team.c_str());
			m_animHexIcons.Invoke("setFlagIcon", "");
		}
		else if (m_currentGameRules != EHUD_SINGLEPLAYER)
		{
			m_animHexIcons.Invoke("setFlagIcon", team.c_str());
			m_animHexIcons.Invoke("setBackground", "");
		}
	}*/
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::UpdateTimers(float frameTime)
{
	if (g_pGame->GetIGameFramework()->IsGamePaused())
		return false;

	CTimeValue now = gEnv->pTimer->GetFrameStartTime();

	if (m_fPlayerDeathTime && m_animWarningMessages.IsLoaded())
	{
		if (g_pGame->GetMenu()->IsActive())
			m_fPlayerDeathTime += frameTime;
		else
		{
			float diff = now.GetSeconds() - m_fPlayerDeathTime;
			if (diff > 14.0f)
			{
				m_fPlayerDeathTime = 0.0f;
				g_pGame->GetMenu()->ShowInGameMenu(false);
				m_animWarningMessages.GetFlashPlayer()->SetVisible(false);
				m_fPlayerDeathTime = now.GetSeconds(); //just in case loading fails

				string* lastSave = g_pGame->GetMenu()->GetLastInGameSave();
				if (lastSave && lastSave->size())
				{
					if (!g_pGame->GetIGameFramework()->LoadGame(lastSave->c_str()))
						g_pGame->GetIGameFramework()->LoadGame(g_pGame->GetLastSaveGame().c_str());
				}
				else
					g_pGame->GetIGameFramework()->LoadGame(g_pGame->GetLastSaveGame().c_str());

				return true;
			}
			else if (diff > 3.0f)
			{
				char seconds[4];
				sprintf(seconds, "%i", (int)(fabsf(14.0f - diff)));
				DisplayOverlayFlashMessage("@ui_load_last_save", ColorF(0, 1.0, 0), true, seconds);
			}
		}
	}

	if (m_fBackPressedTime > 0.0f)
	{
		if ((now.GetSeconds() - m_fBackPressedTime) > 0.2f)
		{
			m_fBackPressedTime = 0.0f;
			OnAction(g_pGame->Actions().hud_show_multiplayer_scoreboard, eIS_Pressed, 1);
		}
	}

	if (m_fXINightVisionActivateTimer >= 0.0f)
	{
		m_fXINightVisionActivateTimer += frameTime;
		if (m_fXINightVisionActivateTimer > 0.5f)
		{
			m_fXINightVisionActivateTimer = -1.0f;
			OnAction(g_pGame->Actions().hud_night_vision, eAAM_OnPress, 1.0f);	//turn on
		}
	}

	if (m_bNightVisionActive)
	{
		m_fNightVisionEnergy -= frameTime * g_pGameCVars->hud_nightVisionConsumption;
		m_animNightVisionBattery.Invoke("setNightVisionBar", m_fNightVisionEnergy * (100.0f / NIGHT_VISION_ENERGY));
		if (m_fNightVisionEnergy <= 0.0f || (m_pClientActor->GetNanoSuit() && !m_pClientActor->GetNanoSuit()->IsNightVisionEnabled()))
			OnAction(g_pGame->Actions().hud_night_vision, 1, 1.0f);	//turn off
	}
	else if (m_fNightVisionEnergy < NIGHT_VISION_ENERGY)
	{
		m_fNightVisionEnergy += frameTime * g_pGameCVars->hud_nightVisionRecharge;
		if (m_fNightVisionEnergy > NIGHT_VISION_ENERGY)
			m_fNightVisionEnergy = NIGHT_VISION_ENERGY;
		m_animNightVisionBattery.Invoke("setNightVisionBar", m_fNightVisionEnergy * (100.0f / NIGHT_VISION_ENERGY));
	}

	if (m_fNightVisionEnergy >= NIGHT_VISION_ENERGY && m_animNightVisionBattery.IsLoaded())
		m_animNightVisionBattery.Unload();

	if (m_fLastReboot)
	{
		if (gEnv->pTimer->GetFrameStartTime().GetSeconds() - m_fLastReboot > 5.0f)
		{
			m_animRebootHUD.Unload();
			m_animBreakHUD.Unload();
			m_fLastReboot = 0.0f;
		}
	}

	//timer missing
	ShowWeaponsOnGround();

	if (m_fPlayerRespawnTimer)
		FakeDeath(true);

	if (m_fMiddleTextLineTimeout != 0.0f)
	{
		if (now.GetSeconds() - m_fMiddleTextLineTimeout > 0)
		{
			m_fMiddleTextLineTimeout = 0.0f;
			m_animMessages.CheckedInvoke("fadeOutMiddleLine");
		}
	}

	if (m_fOverlayTextLineTimeout != 0.0f)
	{
		if (now.GetSeconds() - m_fOverlayTextLineTimeout > 0)
		{
			m_fOverlayTextLineTimeout = 0.0f;
			m_animOverlayMessages.CheckedInvoke("fadeOutMiddleLine");
		}
	}

	if (m_fBigOverlayTextLineTimeout != 0.0f)
	{
		if (now.GetSeconds() - m_fBigOverlayTextLineTimeout > 0)
		{
			m_fBigOverlayTextLineTimeout = 0.0f;
			m_animBigOverlayMessages.CheckedInvoke("fadeOutMiddleLine");
		}
	}

	if (m_fSuitChangeSoundTimer != 0)
	{
		if (now.GetMilliSeconds() - m_fSuitChangeSoundTimer > 500)
		{
			if (m_pClientActor->GetNanoSuit() && m_pClientActor->GetSpectatorMode() == CActor::eASM_None)
			{
				ENanoMode mode = m_pClientActor->GetNanoSuit()->GetMode();
				switch (mode)
				{
				case NANOMODE_SPEED:
					PlayStatusSound("maximum_speed", true);
					break;
				case NANOMODE_STRENGTH:
					PlayStatusSound("maximum_strength", true);
					break;
				case NANOMODE_DEFENSE:
					PlayStatusSound("maximum_armor", true);
					break;
				case NANOMODE_CLOAK:
					PlayStatusSound("normal_cloak_on", true);
					break;
				default:
					break;
				}
			}
			m_fSuitChangeSoundTimer = 0;
		}
	}

	if (m_fSpeedTimer != 0)
	{
		if (now.GetMilliSeconds() - m_fSpeedTimer > 500)
		{
			m_fSpeedTimer = 0;
			TextMessage("maximum_speed");
		}
	}

	if (m_fStrengthTimer != 0)
	{
		if (now.GetMilliSeconds() - m_fStrengthTimer > 500)
		{
			m_fStrengthTimer = 0;
			TextMessage("maximum_strength");
		}
	}

	if (m_fDefenseTimer != 0)
	{
		if (now.GetMilliSeconds() - m_fDefenseTimer > 500)
		{
			m_fDefenseTimer = 0;
			TextMessage("maximum_armor");
		}
	}

	//check if airstrike has been done
	if (m_fAirStrikeStarted > 0.0f && now.GetSeconds() - m_fAirStrikeStarted > 5.0f)
	{
		m_fAirStrikeStarted = 0.0f;
		SetAirStrikeEnabled(false);
	}

	if (m_fPlayerFallAndPlayTimer)
	{
		if (now.GetSeconds() - m_fPlayerFallAndPlayTimer > 12.5f)
		{
			if (m_pClientActor)
				m_pClientActor->StandUp();
			m_fPlayerFallAndPlayTimer = 0.0f;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UpdateWarningMessages(float frameTime)
{
	if (!m_bInMenu && m_animBigOverlayMessages.GetVisible())
	{
		m_animBigOverlayMessages.GetFlashPlayer()->Advance(frameTime);
		m_animBigOverlayMessages.GetFlashPlayer()->Render();
	}
	if (!m_bInMenu && m_animOverlayMessages.GetVisible())
	{
		m_animOverlayMessages.GetFlashPlayer()->Advance(frameTime);
		m_animOverlayMessages.GetFlashPlayer()->Render();
	}
	if (!m_bInMenu && m_animWarningMessages.GetVisible())
	{
		m_animWarningMessages.GetFlashPlayer()->Advance(frameTime);
		m_animWarningMessages.GetFlashPlayer()->Render();
	}
}

//-----------------------------------------------------------------------------------------------------
// TODO: HUD is no more a GameObjectExtension, there is no meaning to use HandleEvent() anymore
//-----------------------------------------------------------------------------------------------------

void CHUD::HandleEvent(const SGameObjectEvent& rGameObjectEvent)
{
	if (rGameObjectEvent.event == eGFE_PauseGame)
	{
		// We hide the HUD during the menu
		SetInMenu(true);
	}
	else if (rGameObjectEvent.event == eGFE_ResumeGame)
	{
		// We show the HUD during the game
		SetInMenu(false);
		UpdateHUDElements();
	}
	else if (rGameObjectEvent.event == eCGE_TextArea)
	{
		m_pHUDTextArea->AddMessage((const char*)rGameObjectEvent.param);
	}
	else if (rGameObjectEvent.event == eCGE_HUD_Break)
	{
		BreakHUD();
	}
	else if (rGameObjectEvent.event == eCGE_HUD_Reboot)
	{
		RebootHUD();
	}
	else if (rGameObjectEvent.event == eCGE_HUD_TextMessage)
	{
		TextMessage((const char*)rGameObjectEvent.param);
	}
}

//------------------------------------------------------------------------
void CHUD::AddTrackedProjectile(EntityId id)
{
	stl::push_back_unique(m_trackedProjectiles, id);
}

//------------------------------------------------------------------------
void CHUD::RemoveTrackedProjectile(EntityId id)
{
	stl::find_and_erase(m_trackedProjectiles, id);
}

// CryMP -----------------------------------------------------------------
void CHUD::AddTrackedRadioMessage(const SRadioMessageParams& params, float expiry)
{
	std::erase_if(m_trackedRadioMessages, [&params](const STrackedRadioMessage& msg) -> bool {
		return msg.params.sourceId == params.sourceId;
	});
	m_trackedRadioMessages.emplace_back(STrackedRadioMessage{
		.params = params,
		.expiresAt = gEnv->pTimer->GetCurrTime() + expiry
	});
}

//------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------------

void CHUD::AutoAimLocking(EntityId id)
{
	LockTarget(id, eLT_Locking);
	PlaySound(ESound_BinocularsLock);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::AutoAimNoText(EntityId id)
{
	LockTarget(id, eLT_Locking, false);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::AutoAimLocked(EntityId id)
{
	m_bHideCrosshair = true;
	//UpdateCrosshairVisibility();
	LockTarget(id, eLT_Locked);
	PlaySound(ESound_BinocularsLock);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::AutoAimUnlock(EntityId id)
{
	m_bHideCrosshair = false;
	//UpdateCrosshairVisibility();
	UnlockTarget(id);
	PlaySound(ESound_BinocularsLock);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ActorDeath(IActor* pActor)
{
	if (!pActor)
		return;

	if (pActor->IsGod())
		return;

	m_pHUDRadar->RemoveFromRadar(pActor->GetEntityId());

	// for MP and for SP local player
	// remove any progress bar and close suit menu if it was open

	if (pActor->IsClient())
	{
		ShowProgress(); // hide any leftover progress bar
		if (m_pModalHUD == &m_animQuickMenu)
			OnAction(g_pGame->Actions().hud_suit_menu, eIS_Released, 1);

		if (m_currentGameRules == EHUD_SINGLEPLAYER)
		{
			ShowPDA(false);
			ShowBuyMenu(false);
			m_fPlayerDeathTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		}
		else if (m_currentGameRules == EHUD_POWERSTRUGGLE)
		{
			ShowPDA(true);
		}

		m_pHUDCrosshair->SetUsability(0);

		if (m_animRadioButtons.GetVisible())
		{
			if (m_pGameRules)
				m_pGameRules->GetRadio()->CancelRadio();
			SetRadioButtons(false);
		}

		if (m_bNightVisionActive)
		{
			OnAction(g_pGame->Actions().hud_night_vision, eIS_Pressed, 1);
			m_fNightVisionTimer = 0.0f; //reset timer
			m_fNightVisionEnergy = NIGHT_VISION_ENERGY;
		}

		// remove forbidden area warning on death
		ShowKillAreaWarning(false, 0);

		m_listBoughtItems.clear();
	}
	else if (static_cast<CActor*>(pActor)->IsFpSpectatorTarget())
	{
		m_pHUDCrosshair->SetUsability(0); //CryMP reset crosshair for FP spec
	}
}


void CHUD::ActorRevive(IActor* pActor, EntityId vehicleId)
{
	if (!pActor)
		return;

	if (pActor->IsGod())
		return;

	if (pActor->IsClient())
	{
		//CryMP: Force hide team selection on revive
		if (GetModalHUD() == &m_animTeamSelection)
		{
			m_animTeamSelection.SetVisible(false);
			SwitchToModalHUD(NULL, false);
		}

		if (m_bNightVisionActive)
		{
			gEnv->p3DEngine->SetPostEffectParam("NightVision_Active", 0.0f);
			m_bNightVisionActive = false;
		}

		if (m_pHUDPowerStruggle)
		{
			m_animHexIcons.Invoke("setHexIcon", m_pHUDPowerStruggle->GetCurrentIconState());
		}

		//CryMP make sure hud hasn't been hidden by spectator
		if (!m_animPlayerStats.GetVisible())
			m_animPlayerStats.SetVisible(true);

		m_bFirstFrame = true; //Update HUD
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::VehicleDestroyed(EntityId id)
{
	m_pHUDRadar->RemoveFromRadar(id);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::TextMessage(const char* message)
{
	if (!message)
		return;

	//DEBUG : used for balancing
	if (!strcmp(message, "GodMode:died!"))
	{
		m_fLastGodModeUpdate = gEnv->pTimer->GetAsyncTime().GetSeconds();

		m_iDeaths++;
		m_pNanoSuit->ResetEnergy();
		return;
	}

	//find a vocal/sound to a text string ...
	const char* textMessage = 0;
	string temp;
	string statusMsg = stl::find_in_map(m_statusMessagesMap, string(message), string(""));
	if (statusMsg.size())
		textMessage = statusMsg.c_str();
	if (textMessage && strcmp(textMessage, ""))	//if known status message
	{
		PlayStatusSound(message);  //trigger vocals
		message = textMessage; //change to localized message for text rendering
	}

	if (strlen(message) > 1)
		DisplayFlashMessage(message, 3);

	//display message
	//m_onScreenText[string(message)] = gEnv->pTimer->GetCurrTime();
}
//-----------------------------------------------------------------------------------------------------

void CHUD::SetSpectatorMode(int mode, EntityId oldTargetId, EntityId newTargetid)
{
	if (oldTargetId != 0)
	{
		//Leaving spectator target
		CPlayer* pOldTarget = CPlayer::FromIActor(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(oldTargetId));
		if (pOldTarget)
		{
			CNanoSuit* pOldSuit = pOldTarget->GetNanoSuit();
			if (pOldSuit)
			{
				pOldSuit->RemoveListener(this);
			}

			m_pHUDCrosshair->SetUsability(0); //CryMP reset crosshair for FP spec
		}
	}
	if (newTargetid)
	{
		CPlayer* pNewTarget = CPlayer::FromIActor(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(newTargetid));
		if (pNewTarget)
		{
			CNanoSuit* pNewSuit = pNewTarget->GetNanoSuit();
			if (pNewSuit)
			{
				pNewSuit->AddListener(this);
				EnergyChanged(pNewSuit->GetSuitEnergy());
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UpdateObjective(CHUDMissionObjective* pObjective)
{
	const char* status = "";
	int colorStatus = 0;
	bool active = false;

	if (pObjective)
	{
		switch (pObjective->GetStatus())
		{
		case CHUDMissionObjective::ACTIVATED:
			status = "@activated";
			active = true;
			colorStatus = 1; //green
			if (pObjective->IsSecondary())
				colorStatus = 2; //yellow

			break;
		case CHUDMissionObjective::DEACTIVATED:
			status = "@deactivated";
			break;
		case CHUDMissionObjective::COMPLETED:
			status = "@completed";
			colorStatus = 4; //grey
			break;
		case CHUDMissionObjective::FAILED:
			status = "@failed";
			colorStatus = 3; //red
			break;
		}
		string message = pObjective->GetMessage();
		string description = pObjective->GetShortDescription();

		if (description.size() || message.size())
		{
			if (!pObjective->IsSilent() /*&& pObjective->GetStatus() != CHUDMissionObjective::DEACTIVATED*/)
			{
				const wchar_t* localizedText = LocalizeWithParams(description.c_str());
				wstring text = localizedText;
				localizedText = LocalizeWithParams(status);
				text.append(L" ");
				text.append(localizedText);
				SFlashVarValue args[3] = { text.c_str(), 1, Col_White.pack_rgb888() };
				m_animMessages.Invoke("setMessageText", args, 3);
				if (pObjective->GetStatus() == CHUDMissionObjective::COMPLETED)
					PlaySound(ESound_ObjectiveComplete);
				else
					PlaySound(ESound_ObjectiveUpdate);
			}

			if (pObjective->GetStatus() == CHUDMissionObjective::DEACTIVATED)
			{
				for (std::map<string, SHudObjective>::iterator it = m_hudObjectivesList.begin(); it != m_hudObjectivesList.end(); ++it)
				{
					if (!strcmp(it->first.c_str(), pObjective->GetID()))
					{
						m_hudObjectivesList.erase(it);
						break;
					}
				}
			}
			else
				m_hudObjectivesList[pObjective->GetID()] = SHudObjective(message, description, colorStatus);
		}

		//this gives the objective's (tracked) id to the Radar ...
		m_pHUDRadar->UpdateMissionObjective(pObjective->GetTrackedEntity(), active, pObjective->GetMapLabel(), pObjective->IsSecondary());
	}

	if (!gEnv->bMultiplayer || m_currentGameRules == EHUD_POWERSTRUGGLE) //in multiplayer the objectives are set in the miniMap only
	{
		m_animObjectivesTab.Invoke("resetObjectives");
		THUDObjectiveList::iterator it = m_hudObjectivesList.begin();
		for (; it != m_hudObjectivesList.end(); ++it)
		{
			SFlashVarValue args[3] = { it->second.description.c_str(), it->second.status, it->second.message.c_str() };
			m_animObjectivesTab.Invoke("setObjective", args, 3);
		}
		m_animObjectivesTab.Invoke("updateContent");
		m_animObjectivesTab.GetFlashPlayer()->Advance(0.1f);
	}
}
//-----------------------------------------------------------------------------------------------------

void CHUD::SetMainObjective(const char* objectiveKey, bool isGoal)
{
	CHUDMissionObjective* pObjective = m_missionObjectiveSystem.GetMissionObjective(objectiveKey);
	if (pObjective)
	{
		SFlashVarValue args[2] = { pObjective->GetShortDescription(), pObjective->GetMessage() };
		m_animObjectivesTab.Invoke((isGoal) ? "setGoal" : "setMainObjective", args, 2);

		if (!isGoal)
			m_currentMainObjective = objectiveKey;
		else
			m_currentGoal = objectiveKey;
	}
}

//-----------------------------------------------------------------------------------------------------

const char* CHUD::GetMainObjective()
{
	if (!m_currentMainObjective.empty())
		return m_currentMainObjective.c_str();

	return m_currentGoal.c_str();
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetOnScreenObjective(EntityId pObjectiveID)
{
	if (m_iOnScreenObjective && m_iOnScreenObjective != pObjectiveID)
		m_pHUDRadar->UpdateMissionObjective(m_iOnScreenObjective, false, " ", false);

	if (m_iOnScreenObjective != pObjectiveID)
	{
		m_iOnScreenObjective = pObjectiveID;
		if (m_iOnScreenObjective)
			m_pHUDRadar->UpdateMissionObjective(m_iOnScreenObjective, true, "", false);
	}
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::ReturnFromLastModalScreen()
{
	if (m_pModalHUD)
	{
		if (m_pModalHUD == &m_animBuyMenu)
		{
			ShowBuyMenu(false);
			return true;
		}
		else if (m_pModalHUD == &m_animPDA)
		{
			ShowPDA(false, false);
			return true;
		}
		else if (m_pModalHUD == &m_animWeaponAccessories)
		{
			ShowWeaponAccessories(false);
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ResetScoreBoard()
{
	if (m_pHUDScore)
		m_pHUDScore->Reset();
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetVotingState(EVotingState state, int timeout, EntityId id, const char* descr)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
	CryLog("%d (%d seconds left) Entity(id %x name %s) description %s", state, timeout, id, pEntity ? pEntity->GetName() : "", descr);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::AddToScoreBoard(EntityId player, int kills, int deaths, int ping)
{
	if (m_pHUDScore)
		m_pHUDScore->AddEntry(player, kills, deaths, ping);
}

//------------------------------------------------------------------------

void CHUD::ForceScoreBoard(bool force)
{
	if (m_animScoreBoard.IsLoaded())
	{
		m_forceScores = force;

		if (force)
		{
			PlaySound(ESound_OpenPopup);
			m_pModalHUD = &m_animScoreBoard;
			m_animScoreBoard.Invoke("Root.setVisible", 1);

		}
		else
		{
			PlaySound(ESound_ClosePopup);
			m_animScoreBoard.Invoke("Root.setVisible", 0);
		}

		if (m_pHUDScore)
			m_pHUDScore->SetVisible(force, &m_animScoreBoard);
		m_bShow = !force;
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::AddToRadar(EntityId entityId) const
{
	m_pHUDRadar->AddEntityToRadar(entityId);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ShowSoundOnRadar(const Vec3& pos, float intensity) const
{
	m_pHUDRadar->ShowSoundOnRadar(pos, intensity);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetRadarScanningEffect(bool show)
{
	if (m_bRadarScanningEffect == show)
		return;

	if (!m_animRadarCompassStealth.IsLoaded()) return;
	if (show)
	{
		m_animRadarCompassStealth.Invoke("setScanningEffect", true);
	}
	else
	{
		m_animRadarCompassStealth.Invoke("setScanningEffect", false);
	}
	m_bRadarScanningEffect = show;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UpdateRatio()
{
	if (m_pHUDRadar)
		m_pHUDRadar->ComputeMiniMapResolution();

	CHUDCommon::UpdateRatio();
}

//-----------------------------------------------------------------------------------------------------

void CHUD::RegisterHUDObject(CHUDObject* pObject)
{
	stl::push_back_unique(m_externalHUDObjectList, pObject);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::DeregisterHUDObject(CHUDObject* pObject)
{
	stl::find_and_erase(m_externalHUDObjectList, pObject);
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::RegisterListener(IHUDListener* pListener)
{
	return stl::push_back_unique(m_hudListeners, pListener);
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::UnRegisterListener(IHUDListener* pListener)
{
	return stl::find_and_erase(m_hudListeners, pListener);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::GetMemoryStatistics(ICrySizer* s)
{
	SIZER_SUBCOMPONENT_NAME(s, "HUD");
	s->Add(*this);

#define CHILD_STATISTICS(x) if (x) (x)->GetMemoryStatistics(s)

	CHILD_STATISTICS(m_pHUDRadar);
	CHILD_STATISTICS(m_pHUDScore);
	//CHILD_STATISTICS(m_pHUDTextChat);
	CHILD_STATISTICS(m_pHUDObituary);
	CHILD_STATISTICS(m_pHUDTextArea);

	TGameFlashAnimationsList::const_iterator iter = m_gameFlashAnimationsList.begin();
	TGameFlashAnimationsList::const_iterator end = m_gameFlashAnimationsList.end();
	for (; iter != end; ++iter)
	{
		CHILD_STATISTICS(*iter);
	}

	//s->Add(m_onScreenMessage);
	//s->AddContainer(m_onScreenMessageBuffer);
	//s->AddContainer(m_onScreenText);
	m_missionObjectiveSystem.GetMemoryStatistics(s);

	for (THUDObjectsList::iterator iter = m_hudObjectsList.begin(); iter != m_hudObjectsList.end(); ++iter)
		(*iter)->GetHUDObjectMemoryStatistics(s);
	for (THUDObjectsList::iterator iter = m_externalHUDObjectList.begin(); iter != m_externalHUDObjectList.end(); ++iter)
		(*iter)->GetHUDObjectMemoryStatistics(s);
	s->AddContainer(m_hudListeners);
	s->AddContainer(m_hudTempListeners);
	s->AddContainer(m_missionObjectiveValues);
	s->AddContainer(m_hudFireModes);
	for (THUDObjectiveList::iterator iter = m_hudObjectivesList.begin(); iter != m_hudObjectivesList.end(); ++iter)
	{
		s->Add(iter->first);
		s->Add(iter->second);
	}

	// TODO: properly handle the strings in these containers
	s->AddContainer(m_possibleAirStrikeTargets);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::StartPlayerFallAndPlay()
{
	if (!m_fPlayerFallAndPlayTimer)
	{
		m_fPlayerFallAndPlayTimer = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		m_pClientActor->Fall();
	}
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::IsInputAssisted()
{
	return false;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnEntityAddedToRadar(EntityId entityId)
{
	HUD_CALL_LISTENERS(OnEntityAddedToRadar(entityId));
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnEnterVehicle(IActor* pActor, const char* strVehicleClassName, const char* strSeatName, bool bThirdPerson)
{
	if (m_pHUDVehicleInterface)
		m_pHUDVehicleInterface->OnEnterVehicle(pActor, strVehicleClassName, strSeatName);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnExitVehicle(IActor* pActor)
{
	if (!pActor->IsClient() || !m_pHUDVehicleInterface)
		return;

	if (IVehicle* pVehicle = m_pHUDVehicleInterface->GetVehicle())
	{
		if (pVehicle->GetEntity()->GetClass() == GetRadar()->m_pAAA)
			m_animRadarCompassStealth.Invoke("setDamage", 2.0f);
	}

	m_pHUDVehicleInterface->OnExitVehicle(pActor);

	/*	SFlashVarValue args[6] = {m_iWeaponAmmo,m_iWeaponInvAmmo,m_iWeaponClipSize, 0, m_iGrenadeAmmo, (const char *)m_sGrenadeType};
		m_animPlayerStats.Invoke("setAmmo", args, 6);
		m_animPlayerStats.Invoke("setAmmoMode", 0);
		if(m_pHUDVehicleInterface)
			m_pHUDVehicleInterface->AmmoForceNextUpdate();
		if(m_iWeaponAmmo == -1)
			m_animPlayerStats.Invoke("setFireMode", 8);*/
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnItemDropped(IActor* pActor, EntityId itemId)
{
	if (m_pHUDPowerStruggle)
	{
		m_pHUDPowerStruggle->InitEquipmentPacks();
		m_pHUDPowerStruggle->UpdatePackageList();
		m_pHUDPowerStruggle->UpdateCurrentPackage();
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnItemUsed(IActor* pActor, EntityId itemId)
{
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnItemPickedUp(IActor* pActor, EntityId itemId)
{
	if (m_pHUDPowerStruggle)
	{
		m_pHUDPowerStruggle->InitEquipmentPacks();
		m_pHUDPowerStruggle->UpdatePackageList();
		m_pHUDPowerStruggle->UpdateCurrentPackage();
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UnloadVehicleHUD(bool bShow)
{
	if (m_pHUDVehicleInterface)
		m_pHUDVehicleInterface->UnloadVehicleHUD(bShow);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ResetQuickMenu()
{
	m_activeButtons = 31;	//011111
	m_defectButtons = 0;	//011111
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UnloadSimpleHUDElements(bool unload)
{
	//unloads simple assets to save memory (smaller peaks in memory pool)

	if (!unload)
	{
		m_animTrackedRadioMessage.Reload(); // CryMP: tracked radio messages
		m_animFriendlyProjectileTracker.Reload();
		m_animHostileProjectileTracker.Reload();
		m_animMissionObjective.Reload();
		m_animQuickMenu.Reload();
		m_animTacLock.Reload();
		//		m_animTargetter.Reload();
		m_animDownloadEntities.Reload();

		ResetQuickMenu();
	}
	else
	{
		m_animTrackedRadioMessage.Reload(); // CryMP: tracked radio messages
		m_animFriendlyProjectileTracker.Unload();
		m_animHostileProjectileTracker.Unload();
		m_animMissionObjective.Unload();
		m_animQuickMenu.Unload();
		m_animTacLock.Unload();
		//		m_animTargetter.Unload();
		m_animDownloadEntities.Unload();

		m_friendlyTrackerStatus = 0;
		m_hostileTrackerStatus = 0;
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::LoadGameRulesHUD(bool load)
{
	//LOAD if neccessary

	switch (m_currentGameRules)
	{
	case EHUD_SINGLEPLAYER:
		if (load)
		{
			if (!m_animObjectivesTab.IsLoaded())
			{
				m_animObjectivesTab.Load("Libs/UI/HUD_MissionObjectives.gfx", eFD_Left, eFAF_Visible);
				m_animObjectivesTab.Invoke("showObjectives", "noAnim");
				m_animObjectivesTab.SetVisible(false);
			}
			//      if(!m_animTargetter.IsLoaded())
			//				m_animTargetter.Load("Libs/UI/HUD_Binoculars_EnemyIndicator.gfx", eFD_Center, eFAF_ThisHandler);
					//m_animTargetter.Load("Libs/UI/HUD_EnemyShootingIndicator.gfx", eFD_Center, eFAF_ThisHandler);
		}
		else
		{
			m_animObjectivesTab.Unload();
			//      m_animTargetter.Unload();
		}
		break;
	case EHUD_INSTANTACTION:
		if (load)
		{
			if (!m_animScoreBoard.IsLoaded())
			{
				m_animScoreBoard.Load("Libs/UI/HUD_MultiplayerScoreboard_DM.gfx");
				SetFlashColor(&m_animScoreBoard);
			}
			if (!m_animChat.IsLoaded())
			{
				EnableChatGfx(g_pGameCVars->mp_chatHighResolution > 0);
			}
			if (!m_animVoiceChat.IsLoaded())
				m_animVoiceChat.Load("Libs/UI/HUD_MultiPlayer_VoiceChat.gfx", eFD_Right, eFAF_ThisHandler);
			if (!m_animBattleLog.IsLoaded())
				m_animBattleLog.Load("Libs/UI/HUD_MP_Log.gfx", eFD_Left);
		}
		else
		{
			m_animScoreBoard.Unload();
			if (m_pHUDTextChat)
				m_pHUDTextChat->Init(0);
			m_animChat.Unload();
			m_animVoiceChat.Unload();
			m_animBattleLog.Unload();
		}
		break;
	case EHUD_POWERSTRUGGLE:
		if (load)
		{
			if (!m_animScoreBoard.IsLoaded())
			{
				m_animScoreBoard.Load("Libs/UI/HUD_MultiplayerScoreboard.gfx");
				SetFlashColor(&m_animScoreBoard);
			}
			if (!m_animChat.IsLoaded())
			{
				/*const char* path = g_pGameCVars->mp_chatHighResolution ? "Libs/UI/HUD_ChatSystem_HR.gfx" : "Libs/UI/HUD_ChatSystem.gfx";
				m_animChat.Load(path, eFD_Left);
				if (m_pHUDTextChat)
					m_pHUDTextChat->Init(&m_animChat);*/

				EnableChatGfx(g_pGameCVars->mp_chatHighResolution > 0);
			}
			if (!m_animObjectivesTab.IsLoaded())
			{
				m_animObjectivesTab.Load("Libs/UI/HUD_MissionObjectives.gfx", eFD_Left, eFAF_Visible);
				m_animObjectivesTab.Invoke("showObjectives", "noAnim");
				m_animObjectivesTab.SetVisible(false);
			}
			if (!m_animVoiceChat.IsLoaded())
				m_animVoiceChat.Load("Libs/UI/HUD_MultiPlayer_VoiceChat.gfx", eFD_Right, eFAF_ThisHandler);
			if (!m_animBattleLog.IsLoaded())
				m_animBattleLog.Load("Libs/UI/HUD_MP_Log.gfx", eFD_Left, eFAF_Default | eFAF_SkipBreak);

			if (!m_animRadioButtons.IsLoaded())
				m_animRadioButtons.Load("Libs/UI/HUD_MP_Radio_Buttons.gfx", eFD_Center, eFAF_ThisHandler | eFAF_SkipBreak);

			if (!m_animBuyMenu.IsLoaded())
				m_animBuyMenu.Load("Libs/UI/HUD_PDA_Buy.gfx", eFD_Right, eFAF_ThisHandler);
			if (!m_animPlayerPP.IsLoaded())
				m_animPlayerPP.Load("Libs/UI/HUD_MP_PPoints.gfx", eFD_Right, eFAF_Default | eFAF_SkipBreak);
			if (!m_animTutorial.IsLoaded())
				m_animTutorial.Load("Libs/UI/HUD_Tutorial.gfx");
		}
		else
		{
			m_animScoreBoard.Unload();
			if (m_pHUDTextChat)
				m_pHUDTextChat->Init(0);
			m_animChat.Unload();
			m_animVoiceChat.Unload();
			m_animBattleLog.Unload();
			m_animBuyMenu.Unload();
			m_animPlayerPP.Unload();
			m_animTutorial.Unload();
			m_animObjectivesTab.Unload();
			m_animHexIcons.Unload();
			m_animSpawnCycle.Unload();
		}
		break;
	case EHUD_TEAMINSTANTACTION:
		if (load)
		{
			if (!m_animScoreBoard.IsLoaded())
			{
				m_animScoreBoard.Load("Libs/UI/HUD_MultiplayerScoreboard_TDM.gfx");
				SetFlashColor(&m_animScoreBoard);
			}
			if (!m_animChat.IsLoaded())
			{
				EnableChatGfx(g_pGameCVars->mp_chatHighResolution > 0);
			}
			if (!m_animVoiceChat.IsLoaded())
				m_animVoiceChat.Load("Libs/UI/HUD_MultiPlayer_VoiceChat.gfx", eFD_Right, eFAF_ThisHandler);
			if (!m_animBattleLog.IsLoaded())
				m_animBattleLog.Load("Libs/UI/HUD_MP_Log.gfx", eFD_Left);
			// This one is on top of others because it displays important
			// messages, so let's put it at the end of the rendering list
			if (!m_animMessages.IsLoaded())
				m_animMessages.Load("Libs/UI/HUD_Messages.gfx");
			//if (!m_animMPMessages.IsLoaded())
			//	m_animMPMessages.Load("Libs/UI/HUD_MP_Messages.gfx", eFD_Center, eFAF_Visible);

			if (!m_animRadioButtons.IsLoaded())
				m_animRadioButtons.Load("Libs/UI/HUD_MP_Radio_Buttons.gfx", eFD_Center, eFAF_ThisHandler | eFAF_SkipBreak);
		}
		else
		{
			m_animScoreBoard.Unload();
			if (m_pHUDTextChat)
				m_pHUDTextChat->Init(0);
			m_animChat.Unload();
			m_animVoiceChat.Unload();
			m_animBattleLog.Unload();
			m_animMessages.Unload();
			//m_animMPMessages.Unload();
			m_pHUDTeamInstantAction->Show(false);
			//m_pHUDInstantAction->Show(false);
		}
		break;
	case EHUD_TEAMACTION:
		if (load)
		{
			if (!m_animScoreBoard.IsLoaded())
			{
				m_animScoreBoard.Load("Libs/UI/HUD_MultiplayerScoreboard_TDM.gfx");
				SetFlashColor(&m_animScoreBoard);
			}
			if (!m_animChat.IsLoaded())
			{
				m_animChat.Load("Libs/UI/HUD_ChatSystem.gfx", eFD_Left);
				if (m_pHUDTextChat)
					m_pHUDTextChat->Init(&m_animChat);
			}
			if (!m_animObjectivesTab.IsLoaded())
			{
				m_animObjectivesTab.Load("Libs/UI/HUD_MissionObjectives.gfx", eFD_Left, eFAF_Visible);
				m_animObjectivesTab.Invoke("showObjectives", "noAnim");
				m_animObjectivesTab.SetVisible(false);
			}
			if (!m_animVoiceChat.IsLoaded())
				m_animVoiceChat.Load("Libs/UI/HUD_MultiPlayer_VoiceChat.gfx", eFD_Right, eFAF_ThisHandler);
			if (!m_animBattleLog.IsLoaded())
				m_animBattleLog.Load("Libs/UI/HUD_MP_Log.gfx", eFD_Left);

			if (!m_animRadioButtons.IsLoaded())
				m_animRadioButtons.Load("Libs/UI/HUD_MP_Radio_Buttons.gfx", eFD_Center, eFAF_ThisHandler);

			if (!m_animBuyMenu.IsLoaded())
				m_animBuyMenu.Load("Libs/UI/HUD_PDA_Buy.gfx", eFD_Right, eFAF_ThisHandler);
			if (!m_animPlayerPP.IsLoaded())
				m_animPlayerPP.Load("Libs/UI/HUD_MP_PPoints.gfx", eFD_Right);
		}
		else
		{
			m_animScoreBoard.Unload();
			if (m_pHUDTextChat)
				m_pHUDTextChat->Init(0);
			m_animChat.Unload();
			m_animVoiceChat.Unload();
			m_animBattleLog.Unload();
			m_animBuyMenu.Unload();
			m_animPlayerPP.Unload();
			m_animObjectivesTab.Unload();
		}
		break;
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetInMenu(bool m)
{
	m_bInMenu = m;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetStealthExposure(float exposure)
{
	m_animHexIcons.Invoke("setStealthExposure", exposure);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::RecordExplosivePlaced(EntityId eid)
{
	stl::push_back_unique(m_explosiveList, eid);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::RecordExplosiveDestroyed(EntityId eid)
{
	std::erase(m_explosiveList, eid);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UpdateTeamActionHUD()
{
	if (CGameRules* pGameRules = g_pGame->GetGameRules())
	{
		if (pGameRules->IsRoundTimeLimited() && !_stricmp(pGameRules->GetEntity()->GetClass()->GetName(), "TeamAction"))
		{
			IEntityScriptProxy* pScriptProxy = static_cast<IEntityScriptProxy*>(pGameRules->GetEntity()->GetProxy(ENTITY_PROXY_SCRIPT));
			if (pScriptProxy)
			{
				bool preround = false;
				int remainingTime = -1;
				if (!_stricmp(pScriptProxy->GetState(), "InGame"))
				{
					remainingTime = (int)(pGameRules->GetRemainingRoundTime());
				}
				else if (!_stricmp(pScriptProxy->GetState(), "PreRound"))
				{
					preround = true;
					remainingTime = (int)(pGameRules->GetRemainingPreRoundTime());
				}

				if (remainingTime > -1)
				{
					int minutes = (remainingTime) / 60;
					int seconds = (remainingTime - minutes * 60);
					string msg;
					msg.Format("%02d:%02d", minutes, seconds);

					Vec3 color = Vec3(1.0f, 1.0f, 1.0f);

					if (remainingTime < g_pGameCVars->g_suddendeathtime || preround)
					{
						float t = fabsf(cry_sinf(gEnv->pTimer->GetCurrTime() * 2.5f));
						Vec3 red = Vec3(0.85f, 0.0f, 0.0f);

						color = color * (1.0f - t) + red * t;
					}

					m_pUIDraw->DrawText(m_pDefaultFont, 0, 12, 22, 22, msg.c_str(), 0.85f, color.x, color.y, color.z, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP);
				}
				int key0;
				int nkScore = 0;
				int usScore = 0;
				IScriptTable* pGameRulesScript = pGameRules->GetEntity()->GetScriptTable();
				if (pGameRulesScript && pGameRulesScript->GetValue("TEAMSCORE_TEAM0_KEY", key0))
				{
					pGameRules->GetSynchedGlobalValue(key0 + 1, nkScore);
					pGameRules->GetSynchedGlobalValue(key0 + 2, usScore);
				}
				if (pGameRules->IsRoundTimeLimited() && !_stricmp(pGameRules->GetEntity()->GetClass()->GetName(), "TeamAction"))
				{
					int clientTeam = pGameRules->GetTeam(m_pClientActor->GetEntityId());

					CGameRules::TPlayers nkPlayers, usPlayers;
					pGameRules->GetTeamPlayers(1, nkPlayers);
					pGameRules->GetTeamPlayers(2, usPlayers);

					int numNK = 0;
					int numUS = 0;
					for (int i = 0; i < nkPlayers.size(); ++i)
					{
						IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(nkPlayers[i]);
						if (pActor && pActor->GetHealth() > 0)
							++numNK;
					}
					for (int i = 0; i < usPlayers.size(); ++i)
					{
						IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(usPlayers[i]);
						if (pActor && pActor->GetHealth() > 0)
							++numUS;
					}

					if (clientTeam != 0)
					{
						string nkPlayers;
						string usPlayers;
						string nkScoreText;
						string usScoreText;
						for (int i = 0; i < nkScore; ++i)
							nkScoreText += "*";
						for (int i = 0; i < usScore; ++i)
							usScoreText += "*";

						if (clientTeam == 1)
						{
							nkPlayers.Format("NK: %d", numNK);
							usPlayers.Format("%d :US", numUS);
							m_pUIDraw->DrawText(m_pDefaultFont, -40, 5, 22, 22, nkScoreText.c_str(), 0.85f, 1.0f, 1.0f, 1.0f, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP, UIDRAWHORIZONTAL_RIGHT, UIDRAWVERTICAL_TOP);
							m_pUIDraw->DrawText(m_pDefaultFont, 40, 5, 22, 22, usScoreText.c_str(), 0.85f, 1.0f, 1.0f, 1.0f, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP);
							m_pUIDraw->DrawText(m_pDefaultFont, -40, 12, 22, 22, nkPlayers.c_str(), 0.85f, 1.0f, 1.0f, 1.0f, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP, UIDRAWHORIZONTAL_RIGHT, UIDRAWVERTICAL_TOP);
							m_pUIDraw->DrawText(m_pDefaultFont, 40, 12, 22, 22, usPlayers.c_str(), 0.85f, 1.0f, 1.0f, 1.0f, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP);
						}
						else
						{
							nkPlayers.Format("%d :NK", numNK);
							usPlayers.Format("US: %d", numUS);
							m_pUIDraw->DrawText(m_pDefaultFont, -40, 5, 22, 22, usScoreText.c_str(), 0.85f, 1.0f, 1.0f, 1.0f, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP, UIDRAWHORIZONTAL_RIGHT, UIDRAWVERTICAL_TOP);
							m_pUIDraw->DrawText(m_pDefaultFont, 40, 5, 22, 22, nkScoreText.c_str(), 0.85f, 1.0f, 1.0f, 1.0f, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP);
							m_pUIDraw->DrawText(m_pDefaultFont, -40, 12, 22, 22, usPlayers.c_str(), 0.85f, 1.0f, 1.0f, 1.0f, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP, UIDRAWHORIZONTAL_RIGHT, UIDRAWVERTICAL_TOP);
							m_pUIDraw->DrawText(m_pDefaultFont, 40, 12, 22, 22, nkPlayers.c_str(), 0.85f, 1.0f, 1.0f, 1.0f, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP);
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::RequestRevive()
{
	if (m_pClientActor->GetHealth() <= 0)
		m_pClientActor->GetPlayerInput()->OnAction(g_pGame->Actions().attack1, eAAM_OnPress, 1.0f);
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::IsScoreboardActive() const
{
	return (m_pHUDScore && m_pHUDScore->m_bShow);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::GameOver(int localWinner, int winnerTeam, EntityId id)
{
	if (m_currentGameRules != EHUD_TEAMINSTANTACTION) //only TIA for now
		return;

	if (m_pHUDTeamInstantAction)
		m_pHUDTeamInstantAction->UpdateStats();
	//if (m_pHUDInstantAction)
	//	m_pHUDInstantAction->UpdateStats();

	const char *message = "@ui_msg_draw_0";
	const char* param = nullptr;

	EntityId checkId = 0;
	if (m_pGameRules->IsPlayerActivelyPlaying(m_pClientActor->GetEntityId()))
		checkId = m_pClientActor->GetEntityId();

	if (m_pGameRules->GetTeamCount() > 1)
	{
		if (checkId == 0)	//player is spectating in freelook
		{
			if (winnerTeam == 0)
			{
				//do nothing, message already set to draw
			}
			else
			{
				message = "@mp_GameOverWinner";
				if (winnerTeam == 1)
				{
					param = "NK";
				}
				else if (winnerTeam == 2)
				{
					param = "US";
				}
			}
		}
		else
		{
			int checkTeamId = m_pGameRules->GetTeam(checkId);
			if (localWinner == 0)
			{
				//do nothing, message already set to draw
			}
			else if (checkTeamId == winnerTeam)
			{
				message = "@ui_msg_tia_win_0";
			}
			else
			{
				message = "@ui_msg_tia_lose_0";
			}
		}
	}
	else
	{
		if (id)
		{
			if (id == checkId)
			{
				message = "@ui_msg_ia_win_0";
			}
			else
			{
				message = "@ui_msg_ia_lose_0";
				IActor* pWinActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(id);
				if (pWinActor && pWinActor->GetEntity())
					param = pWinActor->GetEntity()->GetName();
			}
		}
	}

	const wchar_t* localizedText = L"";
	localizedText = LocalizeWithParams(message, false, param);
	m_animScoreBoard.Invoke("setWinText", localizedText);

	//SFlashVarValue args[2] = { localizedText, false };
	//m_animMPMessages.Invoke("addKillLog", args, 2);
}

//-----------------------------------------------------------------------------------------------------
