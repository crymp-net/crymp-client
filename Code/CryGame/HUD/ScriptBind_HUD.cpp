/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2006.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$

 -------------------------------------------------------------------------
  History:
  - 14:02:2006   11:29 : Created by AlexL
	- 04:04:2006	 17:30 : Extended by Jan Müller

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "ScriptBind_HUD.h"
#include "HUD.h"
#include "CryCommon/CryAction/IGameObject.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "HUDRadar.h"
#include "HUDPowerStruggle.h"
#include "HUDCrosshair.h"
#include "HUDSilhouettes.h"
#include "CryGame/Menus/FlashMenuObject.h"
#include "CryGame/MPTutorial.h"
#include "Library/StringTools.h"

//------------------------------------------------------------------------
CScriptBind_HUD::CScriptBind_HUD(ISystem* pSystem, IGameFramework* pGameFramework)
	: m_pSystem(pSystem),
	m_pSS(pSystem->GetIScriptSystem()),
	m_pGameFW(pGameFramework)
{
	Init(m_pSS, m_pSystem);
	SetGlobalName("HUD");

	RegisterMethods();
	RegisterGlobals();
}

//------------------------------------------------------------------------
CScriptBind_HUD::~CScriptBind_HUD()
{
}

//------------------------------------------------------------------------
void CScriptBind_HUD::RegisterGlobals()
{
	m_pSS->SetGlobalValue("MO_DEACTIVATED", CHUDMissionObjective::DEACTIVATED);
	m_pSS->SetGlobalValue("MO_COMPLETED", CHUDMissionObjective::COMPLETED);
	m_pSS->SetGlobalValue("MO_FAILED", CHUDMissionObjective::FAILED);
	m_pSS->SetGlobalValue("MO_ACTIVATED", CHUDMissionObjective::ACTIVATED);

	m_pSS->SetGlobalValue("eBLE_Information", eBLE_Information);
	m_pSS->SetGlobalValue("eBLE_Currency", eBLE_Currency);
	m_pSS->SetGlobalValue("eBLE_Warning", eBLE_Warning);
	m_pSS->SetGlobalValue("eBLE_System", eBLE_System);
}

//------------------------------------------------------------------------
void CScriptBind_HUD::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_HUD::

	SCRIPT_REG_TEMPLFUNC(SetObjectiveStatus, "objective,status,silent");
	SCRIPT_REG_TEMPLFUNC(GetObjectiveStatus, "");
	SCRIPT_REG_TEMPLFUNC(SetMainObjective, "objective");
	SCRIPT_REG_FUNC(GetMainObjective);
	SCRIPT_REG_TEMPLFUNC(SetObjectiveEntity, "objective,entity");
	SCRIPT_REG_TEMPLFUNC(DrawStatusText, "text");
	SCRIPT_REG_TEMPLFUNC(SetUsability, "objId, message");
	SCRIPT_REG_FUNC(ReloadLevel);
	SCRIPT_REG_FUNC(ReloadLevelSavegame);
	SCRIPT_REG_FUNC(TacWarning);
	SCRIPT_REG_FUNC(HitIndicator);
	SCRIPT_REG_TEMPLFUNC(DamageIndicator, "weaponId, shooterId, direction, onVehicle");
	SCRIPT_REG_TEMPLFUNC(EnteredBuyZone, "zoneId, entered");
	SCRIPT_REG_TEMPLFUNC(EnteredServiceZone, "zoneId, entered");
	SCRIPT_REG_TEMPLFUNC(UpdateBuyList, "");
	SCRIPT_REG_TEMPLFUNC(RadarShowVehicleReady, "vehicleId");
	SCRIPT_REG_TEMPLFUNC(AddEntityToRadar, "entityId");
	SCRIPT_REG_TEMPLFUNC(RemoveEntityFromRadar, "entityId");
	SCRIPT_REG_TEMPLFUNC(ShowKillZoneTime, "active, seconds");
	SCRIPT_REG_TEMPLFUNC(OnPlayerVehicleBuilt, "vehicleId, playerId");
	SCRIPT_REG_FUNC(StartPlayerFallAndPlay);
	SCRIPT_REG_TEMPLFUNC(ShowDeathFX, "type");
	SCRIPT_REG_TEMPLFUNC(BattleLogEvent, "type, msg, [p1], [p2], [p3], [p4]");
	SCRIPT_REG_TEMPLFUNC(OnItemBought, "success, itemName");
	SCRIPT_REG_FUNC(FakeDeath);
	SCRIPT_REG_TEMPLFUNC(ShowWarningMessage, "warning, text");
	SCRIPT_REG_TEMPLFUNC(GetMapGridCoord, "x, y");
	SCRIPT_REG_TEMPLFUNC(OpenPDA, "show, buymenu");

	SCRIPT_REG_TEMPLFUNC(ShowCaptureProgress, "show");
	SCRIPT_REG_TEMPLFUNC(SetCaptureProgress, "progress");
	SCRIPT_REG_TEMPLFUNC(SetCaptureContested, "contested");
	SCRIPT_REG_TEMPLFUNC(ShowConstructionProgress, "show, queued, constructionTime");
	SCRIPT_REG_TEMPLFUNC(ShowReviveCycle, "show");
	SCRIPT_REG_TEMPLFUNC(SpawnGroupInvalid, "");

	SCRIPT_REG_TEMPLFUNC(SetProgressBar, "show, percent, text");
	SCRIPT_REG_TEMPLFUNC(DisplayBigOverlayFlashMessage, "msg, duration, posX, posY, color");
	SCRIPT_REG_TEMPLFUNC(FadeOutBigOverlayFlashMessage, "");
	SCRIPT_REG_TEMPLFUNC(GetLastInGameSave, "");

	SCRIPT_REG_TEMPLFUNC(SetSilhouette, "entityId, r, g, b, duration");
	SCRIPT_REG_TEMPLFUNC(ResetSilhouette, "entityId");

	SCRIPT_REG_TEMPLFUNC(ShowTutorialText, "bShow, text");

#undef SCRIPT_REG_CLASSNAME
}

//------------------------------------------------------------------------
int CScriptBind_HUD::SetObjectiveStatus(IFunctionHandler* pH, const char* pObjectiveID, int status, bool silent)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (!pHUD)
		return pH->EndFunction();
	CHUDMissionObjective* pObj = pHUD->GetMissionObjectiveSystem().GetMissionObjective(pObjectiveID);
	if (pObj)
	{
		pObj->SetSilent(silent);
		pObj->SetStatus((CHUDMissionObjective::HUDMissionStatus)status);
	}
	else
		CryLogWarning("CScriptBind_HUD::Tried to access non existing MissionObjective '%s'", pObjectiveID);
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_HUD::GetObjectiveStatus(IFunctionHandler* pH, const char* pObjectiveID)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (!pHUD)
		return pH->EndFunction();
	CHUDMissionObjective* pObj = pHUD->GetMissionObjectiveSystem().GetMissionObjective(pObjectiveID);
	if (pObj)
	{
		return pH->EndFunction(pObj->GetStatus());
	}
	else
		CryLogWarning("CScriptBind_HUD::Tried to access non existing MissionObjective '%s'", pObjectiveID);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_HUD::SetMainObjective(IFunctionHandler* pH, const char* pObjectiveID)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (!pHUD || !pObjectiveID)
		return pH->EndFunction();
	pHUD->SetMainObjective(pObjectiveID, true);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_HUD::GetMainObjective(IFunctionHandler* pH)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (!pHUD)
		return pH->EndFunction();

	return pH->EndFunction(pHUD->GetMainObjective());
}

//------------------------------------------------------------------------
int CScriptBind_HUD::SetObjectiveEntity(IFunctionHandler* pH, const char* pObjectiveID, ScriptHandle entityID)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (!pHUD)
		return pH->EndFunction();
	CHUDMissionObjective* pObj = pHUD->GetMissionObjectiveSystem().GetMissionObjective(pObjectiveID);
	if (pObj)
		pObj->SetTrackedEntity((EntityId)entityID.n);
	else
		CryLogWarning("CScriptBind_HUD::Tried to access non existing MissionObjective '%s'", pObjectiveID);
	return pH->EndFunction();
}

int CScriptBind_HUD::SetUsability(IFunctionHandler* pH, int objId, const char* message)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (!pHUD)
		return pH->EndFunction();

	pHUD->GetCrosshair()->UpdateUsabilityMessage(static_cast<EntityId>(objId), message);

	return pH->EndFunction();
}

int CScriptBind_HUD::DrawStatusText(IFunctionHandler* pH, const char* pText)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (!pHUD)
		return pH->EndFunction();

	pHUD->TextMessage(pText);

	return pH->EndFunction();
}

//-------------------------------------------------------------------------------------------------
int CScriptBind_HUD::ReloadLevel(IFunctionHandler* pH)
{
	string command("map ");
	command.append(gEnv->pGame->GetIGameFramework()->GetLevelName());
	gEnv->pConsole->ExecuteString(command.c_str());
	return pH->EndFunction();
}

int CScriptBind_HUD::ReloadLevelSavegame(IFunctionHandler* pH)
{
	gEnv->pGame->InitMapReloading();
	return pH->EndFunction();
}

int CScriptBind_HUD::TacWarning(IFunctionHandler* pH)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (!pHUD)
		return pH->EndFunction();
	std::vector<CHUDRadar::RadarEntity> buildings = *(pHUD->GetRadar()->GetBuildings());
	for (int i = 0; i < buildings.size(); ++i)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(buildings[i].m_id);
		if (!_stricmp(pEntity->GetClass()->GetName(), "HQ"))
		{
			Vec3 pos = pEntity->GetWorldPos();
			pos.z += 10;
			ISound* pSound = gEnv->pSoundSystem->CreateSound("sounds/interface:multiplayer_interface:mp_tac_alarm_ambience", FLAG_SOUND_3D);
			if (pSound)
			{
				pSound->SetPosition(pos);
				pSound->Play();
			}
		}
	}
	return pH->EndFunction();
}

int CScriptBind_HUD::EnteredBuyZone(IFunctionHandler* pH, ScriptHandle zoneId, bool entered)
{
	// call the hud, tell him we entered a buy zone
	// the hud itself needs to keep track of which team owns the zone and enable/disable buying accordingly
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD && pHUD->GetPowerStruggleHUD())
		pHUD->GetPowerStruggleHUD()->UpdateBuyZone(entered, (EntityId)zoneId.n);

	return pH->EndFunction();
}

int CScriptBind_HUD::EnteredServiceZone(IFunctionHandler* pH, ScriptHandle zoneId, bool entered)
{
	// call the hud, tell him we entered a service zone
	// the hud itself needs to keep track of which team owns the zone and enable/disable buying accordingly
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD && pHUD->GetPowerStruggleHUD())
		pHUD->GetPowerStruggleHUD()->UpdateServiceZone(entered, (EntityId)zoneId.n);

	return pH->EndFunction();
}

int CScriptBind_HUD::UpdateBuyList(IFunctionHandler* pH)
{
	// something that might have changed item availability happened, so we better update our list
	CHUD* pHUD = g_pGame->GetHUD();

	if (pHUD && pHUD->GetPowerStruggleHUD())
	{
		pHUD->GetPowerStruggleHUD()->UpdateBuyList();
		pHUD->GetPowerStruggleHUD()->UpdateBuyZone(true, 0);
		pHUD->GetPowerStruggleHUD()->UpdateServiceZone(true, 0);
	}

	return pH->EndFunction();
}


int CScriptBind_HUD::DamageIndicator(IFunctionHandler* pH, ScriptHandle weaponId, ScriptHandle shooterId, Vec3 direction, bool onVehicle)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD)
	{
		pHUD->IndicateDamage((EntityId)weaponId.n, direction, onVehicle);
		pHUD->ShowTargettingAI((EntityId)shooterId.n);
	}
	return pH->EndFunction();
}

int CScriptBind_HUD::HitIndicator(IFunctionHandler* pH)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD)
		pHUD->IndicateHit();
	return pH->EndFunction();
}

int CScriptBind_HUD::RadarShowVehicleReady(IFunctionHandler* pH, ScriptHandle vehicleId)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD)
	{
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(vehicleId.n);
		if (pEntity)
		{
			pHUD->GetRadar()->ShowEntityTemporarily(pHUD->GetRadar()->ChooseRadarIcon(pEntity),
				MiniMapIcon::WayPoint,
				(EntityId)vehicleId.n,
				20.0f  //CryMP: Originally only 1 second, now shows up correctly in radar as well as a waypoint icon in minimap
			);
		}
	}

	return pH->EndFunction();
}

int CScriptBind_HUD::AddEntityToRadar(IFunctionHandler* pH, ScriptHandle entityId)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD)
		pHUD->GetRadar()->AddEntityToRadar((EntityId)entityId.n);

	return pH->EndFunction();
}

int CScriptBind_HUD::RemoveEntityFromRadar(IFunctionHandler* pH, ScriptHandle entityId)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD)
		pHUD->GetRadar()->RemoveFromRadar((EntityId)entityId.n);

	return pH->EndFunction();
}

int CScriptBind_HUD::ShowKillZoneTime(IFunctionHandler* pH, bool active, int seconds)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD)
		pHUD->ShowKillAreaWarning(active, seconds);
	return pH->EndFunction();
}

int CScriptBind_HUD::StartPlayerFallAndPlay(IFunctionHandler* pH)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD)
		pHUD->StartPlayerFallAndPlay();
	return pH->EndFunction();
}

int CScriptBind_HUD::OnPlayerVehicleBuilt(IFunctionHandler* pH, ScriptHandle playerId, ScriptHandle vehicleId)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD)
		pHUD->OnPlayerVehicleBuilt((EntityId)playerId.n, (EntityId)vehicleId.n);
	return pH->EndFunction();
}

int CScriptBind_HUD::ShowDeathFX(IFunctionHandler* pH, int type)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD)
		pHUD->ShowDeathFX(type);
	return pH->EndFunction();
}

int CScriptBind_HUD::OnItemBought(IFunctionHandler* pH, bool success, const char* itemName)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD)
		pHUD->PlaySound(success ? ESound_BuyBeep : ESound_BuyError);

	if (success && itemName && pHUD->GetPowerStruggleHUD())
	{
		pHUD->GetPowerStruggleHUD()->SetLastPurchase(itemName);
	}

	return pH->EndFunction();
}

int CScriptBind_HUD::BattleLogEvent(IFunctionHandler* pH, int type, const char* msg)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD)
	{
		string p[4];
		for (int i = 0;i < pH->GetParamCount() - 2;i++)
		{
			switch (pH->GetParamType(3 + i))
			{
			case svtPointer:
			{
				ScriptHandle sh;
				pH->GetParam(3 + i, sh);

				if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)sh.n))
					p[i] = pEntity->GetName();
			}
			break;
			default:
			{
				ScriptAnyValue value;
				pH->GetParamAny(3 + i, value);
				switch (value.GetVarType())
				{
				case svtNumber:
					p[i].Format("%g", value.number);
					break;
				case svtString:
					p[i] = value.str;
					break;
				case svtBool:
					p[i] = value.b ? "true" : "false";
					break;
				default:
					break;
				}
			}
			break;
			}
		}

		pHUD->BattleLogEvent(type, msg, p[0].c_str(), p[1].c_str(), p[2].c_str(), p[3].c_str());
	}

	return pH->EndFunction();
}

int CScriptBind_HUD::ShowWarningMessage(IFunctionHandler* pH, int message, const char* text)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD)
		pHUD->ShowWarningMessage(EWarningMessages(message), text);

	return pH->EndFunction();
}

int CScriptBind_HUD::GetMapGridCoord(IFunctionHandler* pH, float x, float y)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD)
	{
		Vec2i grid = pHUD->GetRadar()->GetMapGridPosition(x, y);
		return pH->EndFunction(grid.x, grid.y);
	}

	return pH->EndFunction();
}

int CScriptBind_HUD::OpenPDA(IFunctionHandler* pH, bool show, bool buyMenu)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (!pHUD)
		return pH->EndFunction();

	pHUD->ShowPDA(show, buyMenu);

	return pH->EndFunction();
}

int CScriptBind_HUD::ShowCaptureProgress(IFunctionHandler* pH, bool show)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD && pHUD->GetPowerStruggleHUD())
		pHUD->GetPowerStruggleHUD()->ShowCaptureProgress(show);

	return pH->EndFunction();
}

int CScriptBind_HUD::SetCaptureProgress(IFunctionHandler* pH, float progress)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD && pHUD->GetPowerStruggleHUD())
		pHUD->GetPowerStruggleHUD()->SetCaptureProgress(progress);

	return pH->EndFunction();
}


int CScriptBind_HUD::SetCaptureContested(IFunctionHandler* pH, bool contested)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD && pHUD->GetPowerStruggleHUD())
		pHUD->GetPowerStruggleHUD()->SetCaptureContested(contested);

	return pH->EndFunction();
}


int CScriptBind_HUD::ShowConstructionProgress(IFunctionHandler* pH, bool show, bool queued, float constructionTime)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD && pHUD->GetPowerStruggleHUD())
		pHUD->GetPowerStruggleHUD()->ShowConstructionProgress(show, queued, constructionTime);

	return pH->EndFunction();
}

int CScriptBind_HUD::ShowReviveCycle(IFunctionHandler* pH, bool show)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD && pHUD->GetPowerStruggleHUD())
		pHUD->ShowReviveCycle(show);

	return pH->EndFunction();
}

int CScriptBind_HUD::SpawnGroupInvalid(IFunctionHandler* pH)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD && pHUD->GetPowerStruggleHUD())
		pHUD->SpawnPointInvalid();

	return pH->EndFunction();
}


int CScriptBind_HUD::FakeDeath(IFunctionHandler* pH)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (pHUD && !pHUD->IsFakeDead())
		pHUD->FakeDeath();

	return pH->EndFunction();
}

int CScriptBind_HUD::SetProgressBar(IFunctionHandler* pH, bool show, int progress, const char* text)
{
	if (CHUD* pHUD = g_pGame->GetHUD())
	{
		if (show)
		{
			const bool init = progress == 0; //CryMP: Call init code only by progress 0
			pHUD->ShowProgress(CLAMP(progress, 0, 100), init, 400, 200, text);
		}
		else
			pHUD->ShowProgress();
	}

	return pH->EndFunction();
}

int CScriptBind_HUD::DisplayBigOverlayFlashMessage(IFunctionHandler* pH, const char* msg, float duration, int posX, int posY, Vec3 color)
{
	if (CHUD* pHUD = g_pGame->GetHUD())
	{
		pHUD->DisplayBigOverlayFlashMessage(msg, duration, posX, posY, color);
	}
	return pH->EndFunction();
}

int CScriptBind_HUD::FadeOutBigOverlayFlashMessage(IFunctionHandler* pH)
{
	if (CHUD* pHUD = g_pGame->GetHUD())
	{
		pHUD->FadeOutBigOverlayFlashMessage();
	}
	return pH->EndFunction();
}

int CScriptBind_HUD::GetLastInGameSave(IFunctionHandler* pH)
{
	string* lastSave = g_pGame->GetMenu()->GetLastInGameSave();

	if (lastSave)
	{
		return pH->EndFunction(lastSave->c_str());
	}
	return pH->EndFunction();
}

int CScriptBind_HUD::SetSilhouette(IFunctionHandler* pH, ScriptHandle entityId, float r, float g, float b, float a, float fDuration)
{
	if (CHUD* pHUD = g_pGame->GetHUD())
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId.n);
		if (pEntity && pHUD->GetSilhouettes())
		{
			pHUD->GetSilhouettes()->SetSilhouette(pEntity, r, g, b, a, fDuration);
		}
	}
	return pH->EndFunction();
}

int CScriptBind_HUD::ResetSilhouette(IFunctionHandler* pH, ScriptHandle entityId)
{
	if (CHUD* pHUD = g_pGame->GetHUD())
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId.n);
		if (pEntity && pHUD->GetSilhouettes())
		{
			pHUD->GetSilhouettes()->ResetSilhouette(entityId.n);
		}
	}
	return pH->EndFunction();
}

int CScriptBind_HUD::ShowTutorialText(IFunctionHandler* pH, bool show)
{
	CHUD* pHUD = g_pGame->GetHUD();
	if (!pHUD)
		return pH->EndFunction();

	if (CMPTutorial* pTutorial = g_pGame->GetGameRules() ? g_pGame->GetGameRules()->GetMPTutorial() : 0)
	{
		if (pTutorial->IsEnabled())
		{
			return pH->EndFunction();
		}
	}

	if (pH->GetParamCount() == 1)
	{
		pHUD->ShowTutorialText(nullptr, show);

		return pH->EndFunction();
	}

	const char* text = "";
	if (pH->GetParamType(2) == svtString)
	{
		if (pH->GetParam(2, text))
		{
			std::wstring widetext;
			StringTools::AppendTo(widetext, text);
			pHUD->ShowTutorialText(widetext.c_str(), true);
		}
	}
	return pH->EndFunction();
}