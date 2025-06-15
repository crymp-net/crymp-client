/*************************************************************************
CryMP client
-------------------------------------------------------------------------
History:
- 04/2020   : Created by ctaoistrach
				

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "HUDVehicleInterface.h"
#include "HUDCrosshair.h"
#include "HUD.h"
#include "GameFlashAnimation.h"
#include "CryGame/Menus/FlashMenuObject.h"
#include "CryGame/Game.h"
#include "CryGame/GameCVars.h"
#include "CryGame/GameRules.h"
#include "CryGame/Items/Weapons/Weapon.h"


//-----------------------------------------------------------------------------------------------------

void CHUD::CheckSpectatorTarget(CPlayer* pActor, float deltaTime)
{
	if (!pActor)
		return;

	const float pTargetHealth = m_pClientActor->GetSpectatorHealth();

	if (pActor && pTargetHealth > 0)
	{
		const auto pSuit = pActor->GetNanoSuit();

		if (!m_animPlayerStats.GetVisible())
			m_animPlayerStats.SetVisible(true);

		SetFireMode(NULL, NULL);

		const auto fPlayer = m_animPlayerStats.GetFlashPlayer();
		fPlayer->Advance(deltaTime);
		fPlayer->Render();
	}
	else if (m_animPlayerStats.GetVisible())
	{
		m_animPlayerStats.SetVisible(false);
	}
}
//-----------------------------------------------------------------------------------------------------

void CHUD::UpdateHealth()
{
	auto* pTarget = GetSpectatorTarget();
	CActor* pActor = static_cast<CActor*>(pTarget ? pTarget : m_pClientActor);
	if (pActor)
	{
		auto pHealth = pTarget ? static_cast<CActor*>(m_pClientActor)->GetSpectatorHealth() : pActor->GetHealth(); //hax: GetHealth works only for client actor
		float fHealth = (pHealth / (float)pActor->GetMaxHealth()) * 100.0f + 1.0f;

		if (m_fHealth != fHealth || m_bFirstFrame)
		{
			m_animPlayerStats.Invoke("setHealth", (int)fHealth);
		}

		if (m_bFirstFrame)
			m_fHealth = fHealth;

		m_fHealth = fHealth;
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::EnergyChanged(float energy)
{
	m_animPlayerStats.Invoke("setEnergy", energy * 0.5f + 1.0f);

	// Crytek FIXME: this should be moved to ::EnergyChanged | CTAO : Done

	CPlayer* pTargetPlayer = CPlayer::FromIActor(GetSpectatorTarget());
	CNanoSuit* pTargetSuit = pTargetPlayer ? pTargetPlayer->GetNanoSuit() : nullptr;
	const float nanoEnergy = pTargetSuit ? pTargetSuit->GetSuitEnergy() : m_pNanoSuit->GetSuitEnergy();

	if ((m_fSuitEnergy > (NANOSUIT_ENERGY * 0.25f)) && nanoEnergy < (NANOSUIT_ENERGY * 0.25f))
	{
		CTimeValue now = gEnv->pTimer->GetFrameStartTime();
		if (now.GetMilliSeconds() - m_fLastSoundPlayedCritical > 30000)
		{
			m_fLastSoundPlayedCritical = now.GetMilliSeconds();
			PlayStatusSound("energy_critical");
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetFireMode(IItem* pItem, IFireMode* pFM, bool forceUpdate)
{
	if (forceUpdate)
	{
		m_curFireMode = 0;
		m_pHUDVehicleInterface->ShowVehicleInterface(m_pHUDVehicleInterface->GetHUDType(), forceUpdate);
	}

	if (!pItem || !pFM)
	{
		if (!pItem)
		{
			IActor* pTarget = GetSpectatorTarget();
			CActor* pActor = static_cast<CActor*>(pTarget ? pTarget : m_pClientActor);
			if (pActor)
			{
				pItem = pActor->GetCurrentItem(true);
			}
			if (!pItem)
				return;
		}
	}

	IZoomMode* pZM = nullptr;
	IWeapon* pWeapon = pItem->GetIWeapon();
	if (pWeapon)
	{
		pFM = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
		pZM = pWeapon->GetZoomMode(pWeapon->GetCurrentZoomMode());
	}

	int iFireMode = stl::find_in_map(m_hudFireModes, CONST_TEMP_STRING(pItem->GetEntity()->GetClass()->GetName()), 0);

	if (iFireMode == 0 || iFireMode == 9) //Shotgun has the firemode "Shotgun"
	{
		if (pFM)
		{
			const char* name = pFM->GetName();
			iFireMode = stl::find_in_map(m_hudFireModes, CONST_TEMP_STRING(name), 0);

			if (pFM->GetAmmoType() && !strcmp(pFM->GetAmmoType()->GetName(), "incendiarybullet"))
			{
				if (iFireMode == 1)
					iFireMode = 18;
				else if (iFireMode == 3)
					iFireMode = 19;
			}

			if (pFM->GetClipSize() == -1 && iFireMode != 6)
				iFireMode = 8;
		}
	}

	if (m_curFireMode != iFireMode)
	{
		m_animPlayerStats.Invoke("setFireMode", iFireMode);

		m_curFireMode = iFireMode;
		UpdateCrosshair(pItem);

		int heat = 0;
		m_animPlayerStats.Invoke("setOverheatBar", heat);
		m_pHUDVehicleInterface->m_iLastReloadBarValue = heat;

		if (m_animProgressLocking.GetVisible())
			ShowProgress(-1);
	}
	
	if (pWeapon && pWeapon->IsZoomed() &&
		(pZM && pZM->IsToggle())) // CryMP: old code: !g_pGameCVars->g_enableAlternateIronSight
	{
		if (m_pHUDCrosshair->GetCrosshairType() != 0)
		{
			m_pHUDCrosshair->SetCrosshair(0);
		}
	}
	else if (m_pHUDCrosshair->GetCrosshairType() == 0 && (g_pGameCVars->g_difficultyLevel < 4 || gEnv->bMultiplayer))
	{
		m_pHUDCrosshair->SelectCrosshair(pItem);
	}

	if (iFireMode == 6)
	{
		float fFireRate = 60.0f / pFM->GetFireRate();
		float fNextShotTime = pFM->GetNextShotTime();
		float fDuration = 100.0f - (((fFireRate - fNextShotTime) / fFireRate) * 100.0f + 1.0f);
		m_animPlayerStats.Invoke("setOverheatBar", fDuration);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ModeChanged(ENanoMode mode, bool suitModeChanged)
{
	IAISignalExtraData* pData = nullptr;
	CPlayer* pPlayer = nullptr;
	switch (mode)
	{
	case NANOMODE_SPEED:
		m_animPlayerStats.Invoke("setMode", "Speed");
		if (suitModeChanged)
		{
			m_fSpeedTimer = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
			m_fSuitChangeSoundTimer = m_fSpeedTimer;
		}
		break;
	case NANOMODE_STRENGTH:
		m_animPlayerStats.Invoke("setMode", "Strength");
		if (suitModeChanged)
		{
			m_fStrengthTimer = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
			m_fSuitChangeSoundTimer = m_fStrengthTimer;
		}
		break;
	case NANOMODE_DEFENSE:
		m_animPlayerStats.Invoke("setMode", "Armor");
		if (suitModeChanged)
		{
			m_fDefenseTimer = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
			m_fSuitChangeSoundTimer = m_fDefenseTimer;
		}
		break;
	case NANOMODE_CLOAK:
		m_animPlayerStats.Invoke("setMode", "Cloak");

		if (suitModeChanged)
		{
			PlaySound(ESound_PresetNavigationBeep);
			if (m_pNanoSuit->GetSlotValue(NANOSLOT_ARMOR, true) != 50 || m_pNanoSuit->GetSlotValue(NANOSLOT_SPEED, true) != 50 ||
				m_pNanoSuit->GetSlotValue(NANOSLOT_STRENGTH, true) != 50 || m_pNanoSuit->GetSlotValue(NANOSLOT_MEDICAL, true) != 50)
			{
				TextMessage("suit_modification_engaged");
			}

			m_fSuitChangeSoundTimer = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
		}

		if (gEnv->pAISystem)
		{
			pData = gEnv->pAISystem->CreateSignalExtraData();//AI System will be the owner of this data
			pData->iValue = NANOMODE_CLOAK;
			pPlayer = static_cast<CPlayer*>(m_pClientActor);
			if (pPlayer && pPlayer->GetEntity() && pPlayer->GetEntity()->GetAI())
				gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 1, "OnNanoSuitMode", pPlayer->GetEntity()->GetAI(), pData);
		}
		break;
	default:
		break;
	}

	if (gEnv->pSystem->IsSerializingFile()) //don't play sounds etc. when it's loaded
		m_fSuitChangeSoundTimer = 0.0f;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UpdatePlayerAmmo()
{
	if (!m_animPlayerStats.GetVisible())
	{
		m_playerAmmo = m_playerRestAmmo = -1; //forces update next time loaded
		return;
	}

	if (!(m_pHUDVehicleInterface && m_pHUDVehicleInterface->GetHUDType() != CHUDVehicleInterface::EHUD_NONE))
	{
		int ammo = 0;
		int clipSize = 0;
		int restAmmo = 0;
		CryFixedStringT<128> grenadeType;
		int grenades = 0;

		auto* Target = GetSpectatorTarget();
		CPlayer* pPlayer = static_cast<CPlayer*>(Target ? Target : m_pClientActor);
		if (pPlayer)
		{
			IItem* pItem = pPlayer->GetCurrentItem(false);
			if (pItem)
			{
				if (CWeapon* pWeapon = static_cast<CWeapon*>(pItem->GetIWeapon()))
				{
					int fm = pWeapon->GetCurrentFireMode();
					if (IFireMode* pFM = pWeapon->GetFireMode(fm))
					{
						ammo = pFM->GetAmmoCount();
						if (IItem* pSlave = pWeapon->GetDualWieldSlave())
						{
							if (IWeapon* pSlaveWeapon = pSlave->GetIWeapon())
								if (IFireMode* pSlaveFM = pSlaveWeapon->GetFireMode(pSlaveWeapon->GetCurrentFireMode()))
									ammo += pSlaveFM->GetAmmoCount();
						}
						clipSize = pFM->GetClipSize();
						restAmmo = pPlayer->GetInventory()->GetAmmoCount(pFM->GetAmmoType());

						if (pFM->CanOverheat())
						{
							int heat = int(pFM->GetHeat() * 100.0f);
							m_animPlayerStats.Invoke("setOverheatBar", heat);
						}
					}
				}
			}

			IItem* pOffhand = m_pItemSystem->GetItem(pPlayer->GetInventory()->GetItemByClass(CItem::sOffHandClass));
			if (pOffhand)
			{
				int firemode = pOffhand->GetIWeapon()->GetCurrentFireMode();
				if (IFireMode* pFm = pOffhand->GetIWeapon()->GetFireMode(firemode))
				{
					if (pFm->GetAmmoType())
					{
						grenadeType.assign(pFm->GetAmmoType()->GetName());
						if (pPlayer)
							grenades = pPlayer->GetInventory()->GetAmmoCount(pFm->GetAmmoType());
					}
					else //can happen during object pickup/throw
					{
						grenadeType.assign(m_sGrenadeType.c_str(), m_sGrenadeType.length());
						grenades = m_iGrenadeAmmo;
					}
				}
			}
		}

		if (m_playerAmmo == -1) //probably good to update fireMode
		{
			SetFireMode(NULL, NULL);
			m_animPlayerStats.Invoke("setAmmoMode", 0);
		}

		if (m_playerAmmo != ammo || m_playerClipSize != clipSize || m_playerRestAmmo != restAmmo ||
			m_iGrenadeAmmo != grenades || grenadeType.compare(m_sGrenadeType.c_str()) != 0)
		{
			SFlashVarValue args[7] = { 0, ammo, clipSize, restAmmo, grenades, grenadeType.c_str(), true };
			m_animPlayerStats.Invoke("setAmmo", args, 7);
			m_playerAmmo = ammo;
			m_playerClipSize = clipSize;
			m_playerRestAmmo = restAmmo;
			m_iGrenadeAmmo = grenades;
			m_sGrenadeType.assign(grenadeType.c_str()); // will alloc
		}
	}
	else
		m_playerAmmo = m_playerRestAmmo = -1; //forces update next time outside vehicle
}
