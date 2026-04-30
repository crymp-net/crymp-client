/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$   All input weapon stuff here

-------------------------------------------------------------------------
History:
- 30.07.07   12:50 : Created by Benito G.R.

*************************************************************************/

#include "CryCommon/CrySystem/ISystem.h"
#include "Weapon.h"
#include "CryGame/GameActions.h"
#include "CryGame/Game.h"
#include "CryGame/GameCVars.h"
#include "CryGame/HUD/HUD.h"

#include "OffHand.h"
#include "CryGame/Actors/Player/IPlayerInput.h"
 

//===========AUX FUNCTIONS====================
namespace
{
	void GetOffHandInfo(CWeapon* pThis, bool &offHandSelected, COffHand** pOffHand)
	{
		CActor *pOwnerActor=pThis->GetOwnerActor();
		if (pOwnerActor)
		{
			(*pOffHand) = static_cast<COffHand*>(pOwnerActor->GetWeaponByClass(CItem::sOffHandClass));
			if((*pOffHand) && (*pOffHand)->IsSelected())
				offHandSelected = true;
		}
	}

	void GetDualWieldInfo(CWeapon* pThis, bool &isDualWield, CWeapon** pSlave)
	{
		if (pThis->IsDualWieldMaster())
		{
			IItem *slave = pThis->GetDualWieldSlave();
			if (slave && slave->GetIWeapon())
			{
				(*pSlave) = static_cast<CWeapon *>(slave);
				isDualWield = true;
			}
		}
	}
}

//=================================================================
TActionHandler<CWeapon>	CWeapon::s_actionHandler;

void CWeapon::RegisterActions()
{
	if (s_actionHandler.GetNumHandlers() == 0)
	{
		#define ADD_HANDLER(action, func) s_actionHandler.AddHandler(actions.action, &CWeapon::func)
		const CGameActions& actions = g_pGame->Actions();

		ADD_HANDLER(attack1,OnActionAttack);
		ADD_HANDLER(reload,OnActionReload);
		ADD_HANDLER(special,OnActionSpecial);
		ADD_HANDLER(modify,OnActionModify);
		ADD_HANDLER(firemode,OnActionFiremode);
		ADD_HANDLER(zoom_in,OnActionZoomIn);
		ADD_HANDLER(zoom_out,OnActionZoomOut);
		ADD_HANDLER(zoom,OnActionZoom);
		ADD_HANDLER(xi_zoom,OnActionZoomXI);

		#undef ADD_HANDLER
	}
}
//-----------------------------------------------------
void CWeapon::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	CItem::OnAction(actorId, actionId, activationMode, value);

	s_actionHandler.Dispatch(this,actorId,actionId,activationMode,value);
}

//------------------------------------------------------
void CWeapon::ForcePendingActions()
{
	CItem::ForcePendingActions();

	CActor* pOwner = GetOwnerActor();
	if(!pOwner || !pOwner->IsClient())
		return;

	//Force start firing, if needed and possible
	if(m_requestedFire)
	{
		if(!IsDualWield() && !IsWeaponRaised())
		{
			m_requestedFire = false;
			if(IsTargetOn() || (m_fm && !m_fm->AllowZoom()))
				return;
			
			OnAction(GetOwnerId(),ActionId("attack1"),eAAM_OnPress,0.0f);
		}
		else if(IsDualWield() && IsDualWieldMaster())
		{
			IItem *slave = GetDualWieldSlave();
			if(!IsWeaponRaised())
			{
				m_requestedFire = false;
				OnAction(GetOwnerId(),ActionId("attack1"),eAAM_OnPress,0.0f);
			}
			else if(slave && slave->GetIWeapon())
			{
				CWeapon* dualwield = static_cast<CWeapon*>(slave);
				if(!dualwield->IsWeaponRaised())
				{
					m_requestedFire = false;
					OnAction(GetOwnerId(),ActionId("attack1"),eAAM_OnPress,0.0f);
				}
			}
		}
	}
}

//--------------------------------------------------------------------
bool CWeapon::PreActionAttack(bool startFire)
{
	// Melee while pressing SHIFT for SP
	CPlayer* pPlayer = GetOwnerPlayer();
	if(!pPlayer)
		return false;

	//if(gEnv->bMultiplayer)
	{
		SPlayerStats* stats = pPlayer->GetPlayerStats();

		if(startFire && pPlayer->IsSprinting())
		{
			//Stop sprinting, start firing
			stats->bSprinting = false;
			stats->bIgnoreSprinting = true;
		
		}
		else if(!startFire)
		{
			//Stop firing, continue sprinting
			stats->bIgnoreSprinting = false;

		}
	}

	return false;
}

//--------------------------------------------------------------------
bool CWeapon::OnActionAttack(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(!m_modifying)
	{
		if(IsTwoHand())
		{
			COffHand * offHandWeapon = NULL;
			bool isOffHandSelected = false;
			GetOffHandInfo(this,isOffHandSelected,&offHandWeapon);

			if(offHandWeapon && 
				(offHandWeapon->GetOffHandState()&(eOHS_HOLDING_GRENADE|eOHS_SWITCHING_GRENADE|eOHS_PICKING_ITEM)))
				return false;
		}

		if (activationMode == eAAM_OnPress)
		{

			if(PreActionAttack(true))
				return true;

			bool isDualWield = false;
			CWeapon *dualWield = NULL;
			GetDualWieldInfo(this,isDualWield,&dualWield);

			if (isDualWield)
			{
				m_fire_alternation = !m_fire_alternation;
				m_requestedFire = true;

				if (!m_fire_alternation && dualWield->OutOfAmmo(false) && dualWield->CanReload())
				{
					dualWield->Reload();
					return true;
				}
				else if(m_fire_alternation && OutOfAmmo(false) && CanReload())
				{
					Reload();
					return true;
				}

				if (m_fire_alternation || (!dualWield->CanFire() || !dualWield->IsSelected()))
				{
					if(!IsWeaponRaised() && CanFire())
						StartFire();
					else if(!dualWield->IsWeaponRaised() && dualWield->IsSelected())
						dualWield->StartFire();
				}
				else if (dualWield->CanFire())
				{
					if(!dualWield->IsWeaponRaised() && dualWield->CanFire())
						dualWield->StartFire();
					else if(!IsWeaponRaised())
						StartFire();
				}

			}
			else
			{
				if(!m_weaponRaised)
					StartFire();

				m_requestedFire = true;
			}
		}
		else if (activationMode == eAAM_OnRelease)
		{
			PreActionAttack(false);

			StopFire();
			m_requestedFire = false;
		}
	}

	return true;
}

//---------------------------------------------------------
bool CWeapon::OnActionReload(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(activationMode==eAAM_OnPress)
	{
		COffHand * offHandWeapon = NULL;
		bool isOffHandSelected = false;
		GetOffHandInfo(this,isOffHandSelected,&offHandWeapon);

		if (!IsBusy() && !m_modifying && !isOffHandSelected)
		{
			bool isDualWield = false;
			CWeapon *dualWield = NULL;
			GetDualWieldInfo(this,isDualWield,&dualWield);

			if(IsWeaponRaised() && m_fm && m_fm->CanReload())
				RaiseWeapon(false);

			Reload();

			if (isDualWield)
			{
				if(dualWield->IsWeaponRaised() && dualWield->CanReload())
					dualWield->RaiseWeapon(false);
				dualWield->Reload();
			}
		}
	}

	return true;
}

//---------------------------------------------------------------------------------
bool CWeapon::OnActionFiremode(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode==eAAM_OnPress)
	{
		bool isDualWield = false;
		CWeapon *dualWield = NULL;
		GetDualWieldInfo(this,isDualWield,&dualWield);

		if (isDualWield)
		{
			if(IsWeaponRaised())
				RaiseWeapon(false,true);

			if(dualWield->IsWeaponRaised())
				dualWield->RaiseWeapon(false,true);

			StartChangeFireMode();
		}
		else
		{
			if(m_weaponRaised)
				RaiseWeapon(false,true);

			StartChangeFireMode();
		}
	}

	return true;
}

//---------------------------------------------------------------------
bool CWeapon::OnActionSpecial(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress)
	{
		if(m_weaponRaised)
			RaiseWeapon(false,true);

		CActor* pOwnerActor = GetOwnerActor();
		if (pOwnerActor && g_pGameCVars->dt_enable)
		{
			CPlayer *pPlayer = static_cast<CPlayer*>(pOwnerActor);
			pPlayer->GetNanoSuit()->Tap(eNA_Melee);
		}

		COffHand * offHandWeapon = NULL;
		bool isOffHandSelected = false;
		GetOffHandInfo(this,isOffHandSelected,&offHandWeapon);

		if (CanMeleeAttack() && (!isOffHandSelected || (offHandWeapon->GetOffHandState()&(eOHS_HOLDING_NPC|eOHS_TRANSITIONING))))
			MeleeAttack();
	}

	return true;
}

//-------------------------------------------------------------------------
bool CWeapon::OnActionModify(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	COffHand * offHandWeapon = NULL;
	bool isOffHandSelected = false;
	GetOffHandInfo(this,isOffHandSelected,&offHandWeapon);

	if (!IsBusy() && !isOffHandSelected)
	{
		if (m_fm)
			m_fm->StopFire();

		if(m_zm && m_zm->IsZoomed())
			StopZoom(actorId);

		if(m_weaponRaised)
			RaiseWeapon(false,true);

		if (m_modifying)
		{
			//close interface

			StopLayer(g_pItemStrings->modify_layer, eIPAF_Default, false);
			StopLayer(g_pItemStrings->enter_modify, eIPAF_Default, false); //CryMP

			const bool overrideBlend = m_timerLayerEnterId > 0;
			if (m_timerLayerEnterId)
			{
				GetScheduler()->KillTimer(m_timerLayerEnterId);
				m_timerLayerEnterId = 0;
			}

			PlayAction(g_pItemStrings->leave_modify, 0, false, eIPAF_Default, -1.0f, overrideBlend ? 0.3f : -1.0f);

			GetScheduler()->TimerAction(
				GetCurrentAnimationTime(eIGS_FirstPerson),
				MakeAction([this](CItem*)
					{
						this->m_transitioning = false;

						CPlayer* pPlayer = this->GetOwnerPlayer();
						if (pPlayer && pPlayer->IsClient())
						{
							pPlayer->ResetDofFx(0.0f);
						}
					}),
				false
			);

			CPlayer* pPlayer = GetOwnerPlayer();
			if (pPlayer && pPlayer->IsClient())
			{
				const float animTime = max(0.001f, (float)GetCurrentAnimationTime(eIGS_FirstPerson) / 1000.0f);
				const float dofSpeed = 1.0f / animTime;

				// Keep modify focus while fading blur out
				pPlayer->SetDofFxLimits(0.0f, 5.0f, 20.0f, 0.0f);
				pPlayer->SetDofFxAmount(0.0f, dofSpeed);
			}

			m_transitioning = true;

			SAFE_HUD_FUNC(WeaponAccessoriesInterface(false));
			m_modifying = false;

			GetGameObject()->InvokeRMI(CItem::SvRequestLeaveModify(), CItem::EmptyParams(), eRMI_ToServer);
		}
		else 
		{
			//open interface
			PlayAction(g_pItemStrings->enter_modify, 0, false, eIPAF_Default | eIPAF_RepeatLastFrame);

			CPlayer* pPlayer = GetOwnerPlayer();
			if (pPlayer && pPlayer->IsClient())
			{
				pPlayer->SetDofFxMask(nullptr);
				pPlayer->SetDofFxLimits(0.0f, 5.0f, 20.0f, 0.0f);
				pPlayer->SetDofFxAmount(1.0f, 1.0f / ((float)GetCurrentAnimationTime(eIGS_FirstPerson) / 1000.0f));
			}

			m_transitioning = true;

			m_timerLayerEnterId = GetScheduler()->TimerAction(
				GetCurrentAnimationTime(eIGS_FirstPerson),
				MakeAction([this](CItem*)
					{
						SAFE_HUD_FUNC(WeaponAccessoriesInterface(true));

						this->m_transitioning = false;
						this->m_timerLayerEnterId = 0;
					}),
				false
			);

			m_modifying = true;

			GetGameObject()->InvokeRMI(CItem::SvRequestEnterModify(), CItem::EmptyParams(), eRMI_ToServer);
		}
	}

	return true;
}

//---------------------------------------------------------
bool CWeapon::OnActionZoomIn(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(m_zm && m_zm->IsZoomed())
	{
		//int numSteps = m_zm->GetMaxZoomSteps();
		//if((numSteps>1) && (m_zm->GetCurrentStep()<numSteps))
			m_zm->ZoomIn();
			//StartZoom(actorId,1);	
	}

	return true;
}

//----------------------------------------------------------
bool CWeapon::OnActionZoomOut(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(m_zm && m_zm->IsZoomed())
	{
		//int numSteps = m_zm->GetMaxZoomSteps();
		//if((numSteps>1) && (m_zm->GetCurrentStep()>1))
			m_zm->ZoomOut();
	}

	return true;
}

//----------------------------------------------------------
bool CWeapon::OnActionZoom(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	COffHand * offHandWeapon = NULL;
	bool isOffHandSelected = false;
	GetOffHandInfo(this,isOffHandSelected,&offHandWeapon);

	if (!m_modifying && (!isOffHandSelected || (offHandWeapon->GetOffHandState()&eOHS_TRANSITIONING)))
	{
		if (activationMode == eAAM_OnPress && m_useViewMode)
		{
			IncrementViewmode();
		}
		else
		{
			bool isDualWield = false;
			CWeapon *dualWield = NULL;
			GetDualWieldInfo(this,isDualWield,&dualWield);

			if (!isDualWield)
			{
				if (m_fm && !m_fm->IsReloading())
				{
					if (activationMode == eAAM_OnPress)
					{
						if(!m_fm->AllowZoom())
						{
							if(!IsTargetOn()) //Allow zoom-in, when using aiming helper
								return false;
						}

						if(m_weaponRaised)
							RaiseWeapon(false,true);

						//Use mouse wheel for scopes with several steps/stages
						if (m_zm && m_zm->IsZoomed() && m_zm->GetMaxZoomSteps()>1)
							StopZoom(actorId);
						else
							StartZoom(actorId,1);
					}
					else if (activationMode == eAAM_OnRelease)
					{
						if (m_zm && !m_zm->IsToggle())
							StopZoom(actorId);
					}
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------------
bool CWeapon::OnActionZoomXI(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	COffHand * offHandWeapon = NULL;
	bool isOffHandSelected = false;
	GetOffHandInfo(this,isOffHandSelected,&offHandWeapon);

	if (!m_modifying && !isOffHandSelected && !IsWeaponRaised())
	{
		bool isDualWield = false;
		CWeapon *dualWield = NULL;
		GetDualWieldInfo(this,isDualWield,&dualWield);

		if (m_useViewMode)
		{
			if (activationMode == eAAM_OnPress)
				IncrementViewmode();
		}
		else if (g_pGameCVars->hud_ctrlZoomMode)
		{
			if (activationMode == eAAM_OnPress)
			{
				if (!isDualWield)
				{
					if(m_fm && !m_fm->IsReloading())
					{
						// The zoom code includes the aim assistance
						if (m_fm->AllowZoom())
							StartZoom(actorId,1);
						else
							m_fm->Cancel();
					}
				}
				else
				{
					// If the view does not zoom, we need to force aim assistance
					AssistAiming(1, true);
				}
			}
			else if (activationMode == eAAM_OnRelease)
			{
				if (!isDualWield)
				{
					if(m_fm && !m_fm->IsReloading())
						StopZoom(actorId);
				}
			}
		}
		else
		{
			if (activationMode == eAAM_OnPress && m_fm && !m_fm->IsReloading())
			{
				if (!isDualWield)
				{
					if (m_fm->AllowZoom())
						StartZoom(actorId,1);		
					else
						m_fm->Cancel();
				}
				else
				{
					// If the view does not zoom, we need to force aim assistance
					AssistAiming(1, true);
				}
			}
		}
	}

	return true;
}
