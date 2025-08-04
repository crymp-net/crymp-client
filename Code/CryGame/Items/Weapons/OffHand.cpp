/*************************************************************************
Crytek Source File
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 12:04:2006   17:22 : Created by Márcio Martins
- 18:02:2007	 13:30 : Refactored Offhand by Benito G.R.

*************************************************************************/

#include "CryCommon/CrySystem/ISystem.h"
#include "OffHand.h"
#include "CryGame/Actors/Actor.h"
#include "FireModes/Throw.h"
#include "CryGame/GameRules.h"
#include "CryCommon/CryAction/IWorldQuery.h"
#include "Fists.h"
#include "CryGame/GameActions.h"
#include "FireModes/Melee.h"

#include "CryGame/HUD/HUD.h"
#include "CryGame/HUD/HUDCrosshair.h"
#include "WeaponSystem.h"
#include "Projectile.h"
#include "CryGame/GameCVars.h"

//Sounds tables
namespace
{
	const char gChokeSoundsTable[MAX_CHOKE_SOUNDS][64] =
	{
		"Languages/dialog/ai_korean_soldier_1/choke_01.mp3",
		"Languages/dialog/ai_korean_soldier_2/choke_02.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_03.mp3",
		"Languages/dialog/ai_korean_soldier_3/choke_04.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_05.mp3"
	};
	const char gDeathSoundsTable[MAX_CHOKE_SOUNDS][64] =
	{
		"Languages/dialog/ai_korean_soldier_1/choke_grab_00.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_grab_01.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_grab_02.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_grab_03.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_grab_04.mp3"
	};
}

//========================Scheduled offhand actions =======================//
namespace
{
	//This class help us to select the correct action
	class FinishOffHandAction
	{
	public:
		FinishOffHandAction(EOffHandActions _eOHA, COffHand* _pOffHand)
		{
			eOHA = _eOHA;
			pOffHand = _pOffHand;
		}
		void execute(CItem* cItem)
		{
			pOffHand->FinishAction(eOHA);
		}

	private:

		EOffHandActions eOHA;
		COffHand* pOffHand;
	};

	//End finish grenade action (switch/throw)
	class FinishGrenadeAction
	{
	public:
		FinishGrenadeAction(COffHand* _pOffHand, CItem* _pMainHand)
		{
			pOffHand = _pOffHand;
			pMainHand = _pMainHand;
		}
		void execute(CItem* cItem)
		{
			//pOffHand->HideItem(true);
			float timeDelay = 0.1f;	//ms

			if (pMainHand && !pMainHand->IsDualWield())
			{
				pMainHand->ResetDualWield();		//I can reset, because if DualWield it's not possible to switch grenades (see PreExecuteAction())
				pMainHand->PlayAction(g_pItemStrings->offhand_off, 0, false, CItem::eIPAF_Default | CItem::eIPAF_NoBlend);
				timeDelay = (pMainHand->GetCurrentAnimationTime(CItem::eIGS_FirstPerson) + 50) * 0.001f;
			}
			else if (!pOffHand->GetOwnerActor()->ShouldSwim())
			{
				if (pMainHand && pMainHand->IsDualWield())
					pMainHand->Select(true);
				else
					pOffHand->GetOwnerActor()->HolsterItem(false);
			}

			if (pOffHand->GetOffHandState() == eOHS_SWITCHING_GRENADE)
			{
				int grenadeType = pOffHand->GetCurrentFireMode();
				pOffHand->AttachGrenadeToHand(grenadeType);
			}

			pOffHand->SetOffHandState(eOHS_TRANSITIONING);

			//Offhand goes back to initial state
			pOffHand->SetResetTimer(timeDelay);
			pOffHand->RequireUpdate(eIUS_General);
		}

	private:
		COffHand* pOffHand;
		CItem* pMainHand;
	};
}

//=====================~Scheduled offhand actions======================//

TActionHandler<COffHand> COffHand::s_actionHandler;

COffHand::COffHand()
{
	m_useFPCamSpacePP = false;

	m_holdOffset = Matrix34::CreateIdentity();
	m_lastNPCMatrix = Matrix34::CreateIdentity();
	m_intialBoidLocalMatrix = Matrix34::CreateIdentity();

	// Sound array
	for (int i = 0; i < eOHSound_LastSound; ++i)
		m_sounds[i] = INVALID_SOUNDID;

	RegisterActions();
}

//=============================================================
COffHand::~COffHand()
{
	if (m_heldEntityId)
	{
		SetHeldEntityId(0);
	}
}

//============================================================
void COffHand::RegisterActions()
{
	if (s_actionHandler.GetNumHandlers() == 0)
	{
#define ADD_HANDLER(action, func) s_actionHandler.AddHandler(actions.action, &COffHand::func)
		const CGameActions& actions = g_pGame->Actions();

		ADD_HANDLER(use, OnActionUse);
		ADD_HANDLER(xi_use, OnActionUse);
		ADD_HANDLER(attack1, OnActionAttack);
		ADD_HANDLER(zoom, OnActionDrop);
		ADD_HANDLER(xi_zoom, OnActionDrop);
		ADD_HANDLER(grenade, OnActionThrowGrenade);
		ADD_HANDLER(xi_grenade, OnActionXIThrowGrenade);
		ADD_HANDLER(handgrenade, OnActionSwitchGrenade);
		ADD_HANDLER(xi_handgrenade, OnActionXISwitchGrenade);
		ADD_HANDLER(special, OnActionSpecial);
#undef ADD_HANDLER
	}
}
//=============================================================
void COffHand::Reset()
{
	CWeapon::Reset();

	if (m_heldEntityId)
	{
		//Prevent editor-reset issues
		if (m_currentState & (eOHS_GRABBING_NPC | eOHS_HOLDING_NPC | eOHS_THROWING_NPC))
		{
			ThrowNPC(m_heldEntityId, false);
		}
	}

	SetHeldEntityId(0);

	m_nextThrowTimer = -1.0f;
	m_lastFireModeId = 0;
	m_pickingTimer = -1.0f;
	m_preHeldEntityId = 0;
	m_constraintId = 0;
	m_resetTimer = -1.0f;
	m_killTimeOut = -1.0f;
	m_killNPC = false;
	m_effectRunning = false;
	m_npcWasDead = false;
	m_grabbedNPCSpecies = eGCT_UNKNOWN;
	m_lastCHUpdate = 0.0f;
	m_heldEntityMass = 0.0f;
	m_prevMainHandId = 0;
	m_constraintStatus = ConstraintStatus::Inactive;
	m_pRockRN = nullptr;
	m_bCutscenePlaying = false;
	m_forceThrow = false;
	m_restoreStateAfterLoading = false;

	DrawSlot(eIGS_Aux0, false); //frag_grenade_tp.cgf	
	DrawSlot(eIGS_Owner, false); //emp_grenade.cgf
	DrawSlot(eIGS_OwnerLooped, false); //flashbang.cgf
	DrawSlot(eIGS_Aux1, false); //smoke_grenade_tp.cgf

	SetOffHandState(eOHS_INIT_STATE);

	for (int i = 0; i < eOHSound_LastSound; i++)
	{
		m_sounds[i] = INVALID_SOUNDID;
	}
}

//=============================================================
void COffHand::PostInit(IGameObject* pGameObject)
{
	CWeapon::PostInit(pGameObject);

	m_lastFireModeId = 0;
	SetCurrentFireMode(0);
	HideItem(true);
}

//============================================================
bool COffHand::ReadItemParams(const IItemParamsNode* root)
{
	if (!CWeapon::ReadItemParams(root))
		return false;

	m_grabTypes.clear();

	//Read offHand grab types
	if (const IItemParamsNode* pickabletypes = root->GetChild("pickabletypes"))
	{
		int n = pickabletypes->GetChildCount();
		for (int i = 0; i < n; ++i)
		{
			const IItemParamsNode* pt = pickabletypes->GetChild(i);

			SGrabType grabType;
			grabType.helper = pt->GetAttribute("helper");
			grabType.pickup = pt->GetAttribute("pickup");
			grabType.idle = pt->GetAttribute("idle");
			grabType.throwFM = pt->GetAttribute("throwFM");

			if (strcmp(pt->GetName(), "onehanded") == 0)
			{
				grabType.twoHanded = false;
				m_grabTypes.push_back(grabType);
			}
			else if (strcmp(pt->GetName(), "twohanded") == 0)
			{
				grabType.twoHanded = true;
				m_grabTypes.push_back(grabType);
			}
		}
	}

	return true;
}
//============================================================
void COffHand::FullSerialize(TSerialize ser)
{
	CWeapon::FullSerialize(ser);

	EntityId oldHeldId = m_heldEntityId;

	ser.Value("m_lastFireModeId", m_lastFireModeId);
	ser.Value("m_usable", m_usable);
	ser.Value("m_currentState", m_currentState);
	ser.Value("m_preHeldEntityId", m_preHeldEntityId);
	ser.Value("m_startPickUp", m_startPickUp);
	ser.Value("m_heldEntityId", m_heldEntityId);
	ser.Value("m_constraintId", m_constraintId);
	ser.Value("m_grabType", m_grabType);
	ser.Value("m_grabbedNPCSpecies", m_grabbedNPCSpecies);
	ser.Value("m_killTimeOut", m_killTimeOut);
	ser.Value("m_effectRunning", m_effectRunning);
	ser.Value("m_grabbedNPCInitialHealth", m_grabbedNPCInitialHealth);
	ser.Value("m_prevMainHandId", m_prevMainHandId);
	ser.Value("m_holdScale", m_holdScale);

	//PATCH ====================
	if (ser.IsReading())
	{
		m_mainHandIsDualWield = false;
	}

	ser.Value("m_mainHandIsDualWield", m_mainHandIsDualWield);

	//============================

	if (ser.IsReading() && m_heldEntityId != oldHeldId)
	{
		IActor* pActor = m_pActorSystem->GetActor(oldHeldId);
		if (pActor)
		{
			if (pActor->GetEntity()->GetCharacter(0))
				pActor->GetEntity()->GetCharacter(0)->SetFlags(pActor->GetEntity()->GetCharacter(0)->GetFlags() & (~ENTITY_SLOT_RENDER_NEAREST));
			if (IEntityRenderProxy* pProxy = (IEntityRenderProxy*)pActor->GetEntity()->GetProxy(ENTITY_PROXY_RENDER))
			{
				if (IRenderNode* pRenderNode = pProxy->GetRenderNode())
					pRenderNode->SetRndFlags(ERF_RENDER_ALWAYS, false);
			}
		}
		else
			DrawNear(false, oldHeldId);
	}
}

//============================================================
bool COffHand::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	return true;
}

//============================================================
void COffHand::PostSerialize()
{
	m_restoreStateAfterLoading = true; //Will be cleared on first update
}

//==========================================================
void COffHand::PostPostSerialize()
{
	RemoveAllAccessories();

	SetCurrentFireMode(m_lastFireModeId);

	bool needsReset = false;

	if (m_currentState & (eOHS_THROWING_OBJECT | eOHS_HOLDING_GRENADE | eOHS_THROWING_GRENADE | eOHS_TRANSITIONING))
	{
		needsReset = true;
	}
	else if (m_currentState == eOHS_SWITCHING_GRENADE)
	{
		m_currentState = eOHS_INIT_STATE;
		ProcessOffHandActions(eOHA_SWITCH_GRENADE, INPUT_DEF, eAAM_OnPress, 1.0f);
	}
	else if (m_currentState == eOHS_PICKING_ITEM)
	{
		//If picking an item...
		if (m_heldEntityId || (m_preHeldEntityId && m_startPickUp))
		{
			if (!m_heldEntityId)
			{
				SetHeldEntityId(m_preHeldEntityId);
			}

			SelectGrabType(m_pEntitySystem->GetEntity(m_heldEntityId));

			CActor* pPlayer = GetOwnerActor();
			if (pPlayer)
			{
				m_currentState = eOHS_PICKING_ITEM2;
				pPlayer->PickUpItem(m_heldEntityId, true);
				SetIgnoreCollisionsWithOwner(false, m_heldEntityId);
			}
		}
		SetOffHandState(eOHS_INIT_STATE);
	}
	else if ((m_heldEntityId || m_preHeldEntityId) && m_currentState != eOHS_TRANSITIONING)
	{
		if (m_preHeldEntityId && !m_heldEntityId && m_startPickUp)
		{
			m_currentState = eOHS_PICKING;
			SetHeldEntityId(m_preHeldEntityId);
		}

		if (m_heldEntityId)
		{
			IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
			if (!pEntity)
			{
				CryLogWarning("Offhand held entity did not exist anymore! Perhaps it was a boid ... ");
				needsReset = true;
			}
			else
			{
				SelectGrabType(m_pEntitySystem->GetEntity(m_heldEntityId));

				//If holding an object or NPC
				if (m_currentState & (eOHS_HOLDING_OBJECT | eOHS_PICKING | eOHS_THROWING_OBJECT | eOHS_MELEE))
				{
					//Do grabbing again
					m_currentState = eOHS_INIT_STATE;
					m_preHeldEntityId = m_heldEntityId;
					PreExecuteAction(eOHA_USE, eAAM_OnPress, true);

					StartPickUpObject(m_heldEntityId, false);
				}
				else if (m_currentState & (eOHS_HOLDING_NPC | eOHS_GRABBING_NPC | eOHS_THROWING_NPC))
				{
					//Do grabbing again
					m_currentState = eOHS_INIT_STATE;
					CActor* pActor = static_cast<CActor*>(m_pActorSystem->GetActor(m_heldEntityId));
					bool isDead = false;
					if (pActor && ((pActor->GetActorStats() && pActor->GetActorStats()->isRagDoll) || pActor->GetHealth() <= 0))
						isDead = true;
					if (isDead)//don't pickup ragdolls
					{
						pActor->Revive(); //will be ragdollized after serialization...
						pActor->SetAnimationInput("Action", "idle");
						pActor->CreateScriptEvent("kill", 0);
						needsReset = true;
					}
					else
					{
						m_preHeldEntityId = m_heldEntityId;
						PreExecuteAction(eOHA_USE, eAAM_OnPress, true);

						StartPickUpObject(m_heldEntityId, true);
						if (pActor)
						{
							pActor->GetAnimatedCharacter()->ForceRefreshPhysicalColliderMode();
						}
					}
				}
			}
		}

	}
	else if (m_currentState != eOHS_INIT_STATE)
	{
		needsReset = true;
	}

	if (needsReset)
	{
		m_mainHand = static_cast<CItem*>(GetOwnerActor()->GetCurrentItem());
		m_mainHandWeapon = m_mainHand ? static_cast<CWeapon*>(m_mainHand->GetIWeapon()) : NULL;
		m_mainHandIsDualWield = m_mainHand ? m_mainHand->IsDualWield() : false;
		m_currentState = eOHS_TRANSITIONING;
		FinishAction(eOHA_RESET);
	}

	if (m_ownerId == LOCAL_PLAYER_ENTITY_ID)
	{
		if (!m_stats.selected)
		{
			SetHand(eIH_Left);					//This will be only done once after loading
			Select(true);	//this can select the wrong crosshair, should be removed if save
			Select(false);
		}
	}

	m_restoreStateAfterLoading = false;
}

//=======================================
void COffHand::OnEnterFirstPerson()
{
	//CryMP: Check 1st/3rd person transition
	CWeapon::OnEnterFirstPerson();

	if (m_heldEntityId)
	{
		AttachObjectToHand(false, m_heldEntityId, false);
		UpdateEntityRenderFlags(m_heldEntityId, EntityFpViewMode::ForceActive);

		if (m_currentState == eOHS_HOLDING_OBJECT)
		{
			GetOwnerActor()->HolsterItem(true);
		}
	}
}

//=======================================
void COffHand::OnEnterThirdPerson()
{
	//CryMP: Check 1st/3rd person transition
	CWeapon::OnEnterThirdPerson();

	if (m_heldEntityId)
	{
		AttachObjectToHand(true, m_heldEntityId, false);
		UpdateEntityRenderFlags(m_heldEntityId, EntityFpViewMode::ForceDisable);
	}
}

//=============================================================
bool COffHand::CanSelect() const
{
	return false;
}

//=============================================================
void COffHand::Select(bool select)
{
	CWeapon::Select(select);
}

//=============================================================
void COffHand::Update(SEntityUpdateContext& ctx, int slot)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	CWeapon::Update(ctx, slot);

	//CryMP
	if (m_heldEntityId && !m_stats.fp)
	{
		CActor* pActor = GetOwnerActor();
		if (pActor && pActor->IsRemote())
		{
			UpdateHeldObject();
		}
	}
}

//=============================================================
void COffHand::CheckTimers(float frameTime)
{
	if (m_resetTimer >= 0.0f)
	{
		m_resetTimer -= frameTime;
		if (m_resetTimer < 0.0f)
		{
			SetOffHandState(eOHS_INIT_STATE);
			m_resetTimer = -1.0f;
		}
	}
	if (m_nextThrowTimer >= 0.0f)
	{
		//Throw fire rate (grenade, object, NPC)
		m_nextThrowTimer -= frameTime;
	}
	if (m_fGrenadeToggleTimer >= 0.0f)
	{
		m_fGrenadeToggleTimer += frameTime;
		if (m_fGrenadeToggleTimer > 1.0f)
		{
			StartSwitchGrenade(true);
			m_fGrenadeToggleTimer = 0.0f;
		}
	}

	if (m_fGrenadeThrowTimer >= 0.0f)
	{
		m_fGrenadeThrowTimer += frameTime;
		if (m_fGrenadeThrowTimer > 0.5f)
		{
			m_fGrenadeThrowTimer = -1.0f;
		}
	}

	if (m_pickingTimer >= 0.0f)
	{
		m_pickingTimer -= frameTime;

		if (m_pickingTimer < 0.0f)
		{
			PerformPickUp();
		}

	}
	if (m_killTimeOut >= 0.0f)
	{
		m_killTimeOut -= frameTime;
		if (m_killTimeOut < 0.0f)
		{
			m_killTimeOut = -1.0f;
			m_killNPC = true;
		}
	}
}

//=============================================================
//CryMP: Called always on the client, even in ThirdPerson
//Called on other clients, if spectating them in FirstPerson

void COffHand::UpdateFPView(float frameTime)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	CheckTimers(frameTime);

	//CryMP: Update held items in TP mode as well
	if (!m_stats.fp && m_heldEntityId) //CryMP: Note: this check needs to be here, otherwise no grab anims in FP 
	{
		UpdateFPPosition(frameTime);
		UpdateFPCharacter(frameTime);
	}

	if (m_stats.selected)
	{
		CItem::UpdateFPView(frameTime);

		if (m_fm)
			m_fm->UpdateFPView(frameTime);

	}
	else
	{
		//CryMP need to do this here, not checked in CItem::UpdateFPView if deselected
		CheckViewChange();
	}

	if (m_restoreStateAfterLoading)
		PostPostSerialize();

	m_lastCHUpdate += frameTime;

	if (m_currentState == eOHS_INIT_STATE)
	{
		if (!gEnv->bMultiplayer || (g_pGameCVars->mp_pickupObjects && m_pGameFramework->IsImmersiveMPEnabled()))
			UpdateCrosshairUsabilitySP();
		else
			UpdateCrosshairUsabilityMP();

		m_weaponLowered = false;

		//Fix offhand floating on spawn (not really nice fix...)
		if (m_stats.hand == 0)
		{
			SetHand(eIH_Left); //This will be only done once after loading

			Select(true); Select(false);
		}
		//=========================================================
	}
	else if (m_heldEntityId && m_currentState & (eOHS_HOLDING_OBJECT | eOHS_PICKING | eOHS_THROWING_OBJECT | eOHS_PICKING_ITEM | eOHS_MELEE))
	{

		if (m_usable)
		{
			if (g_pGame->GetHUD())
				g_pGame->GetHUD()->GetCrosshair()->SetUsability(0, "");
			m_usable = false;
		}

		UpdateHeldObject();

		if (m_grabType == GRAB_TYPE_TWO_HANDED)
			UpdateWeaponLowering(frameTime);
		else if (CActor* pActor = GetOwnerActor())
		{
			if (CWeapon* pWeapon = pActor->GetCurrentWeapon(false))
			{
				LowerWeapon(pWeapon->IsWeaponLowered());
			}
		}
	}
}
//============================================================

void COffHand::UpdateCrosshairUsabilitySP()
{

	//Only update a few times per second
	if (m_lastCHUpdate > TIME_TO_UPDATE_CH)
		m_lastCHUpdate = 0.0f;
	else
		return;

	CActor* pActor = GetOwnerActor();
	if (pActor)
	{
		CPlayer* pPlayer = CPlayer::FromActor(pActor);
		bool isLadder = pPlayer->IsLadderUsable();

		const bool onLadder = pPlayer->GetPlayerStats()->isOnLadder.Value();

		const int canGrab = CanPerformPickUp(pActor, NULL);

		if (canGrab || (isLadder && !onLadder))
		{
			if (CHUD* pHUD = g_pGame->GetHUD())
			{
				IItem* pItem = m_pItemSystem->GetItem(m_crosshairId);
				CHUDCrosshair *pHUDCrosshair = pHUD->GetCrosshair();
				if (IActor* pActor = m_pActorSystem->GetActor(m_crosshairId))
					pHUDCrosshair->SetUsability(1, "@grab_enemy");
				else if (isLadder)
					pHUDCrosshair->SetUsability(1, "@use_ladder");
				else if (!pItem)
				{
					pHUDCrosshair->SetUsability(1, "@grab_object");
				}
				else if (pItem)
				{
					CryFixedStringT<128> itemName("@");
					itemName.append(pItem->GetEntity()->GetClass()->GetName());
					if (!strcmp(pItem->GetEntity()->GetClass()->GetName(), "CustomAmmoPickup"))
					{
						SmartScriptTable props;
						if (pItem->GetEntity()->GetScriptTable() && pItem->GetEntity()->GetScriptTable()->GetValue("Properties", props))
						{
							const char* name = NULL;
							props->GetValue("AmmoName", name);
							itemName.assign("@");
							itemName.append(name);
						}
					}

					if (pItem->GetIWeapon())
					{
						IEntityClass* pItemClass = pItem->GetEntity()->GetClass();
						bool isSocom = strcmp(pItemClass->GetName(), "SOCOM") ? false : true;
						IItem* pCurrentItem = m_pItemSystem->GetItem(pPlayer->GetInventory()->GetItemByClass(pItemClass));
						if ((!isSocom && pCurrentItem) ||
							(isSocom && pCurrentItem && pCurrentItem->IsDualWield()))
						{
							if (pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
								pHUDCrosshair->SetUsability(1, "@game_take_ammo_from", itemName.c_str());
							else
								pHUDCrosshair->SetUsability(2, "@weapon_ammo_full", itemName.c_str());
						}
						else
						{
							int typ = CanPerformPickUp(GetOwnerActor(), NULL, true);
							if (m_preHeldEntityId && !pPlayer->CheckInventoryRestrictions(m_pEntitySystem->GetEntity(m_preHeldEntityId)->GetClass()->GetName()))
							{
								IItem* pExchangedItem = GetExchangeItem(pPlayer);
								if (pExchangedItem)
								{
									pHUDCrosshair->SetUsability(1, "@game_exchange_weapon",
										pExchangedItem->GetEntity()->GetClass()->GetName(), itemName.c_str());
								}
								else
									pHUDCrosshair->SetUsability(2, "@inventory_full");
							}
							else
							{
								pHUDCrosshair->SetUsability(1, "@pick_weapon", itemName.c_str());
							}
						}
					}
					else
					{
						if (pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
							pHUDCrosshair->SetUsability(1, "@pick_item", itemName.c_str());
						else
							pHUDCrosshair->SetUsability(2, "@weapon_ammo_full", itemName.c_str());
					}
				}
			}
			m_usable = true;
		}
		else if (m_usable)
		{
			if (g_pGame->GetHUD())
				g_pGame->GetHUD()->GetCrosshair()->SetUsability(false, "");
			m_usable = false;
		}
	}
}

//==========================================================
void COffHand::UpdateCrosshairUsabilityMP()
{
	//Only update a few times per second
	if (m_lastCHUpdate > TIME_TO_UPDATE_CH)
		m_lastCHUpdate = 0.0f;
	else
		return;

	CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor());
	if (pPlayer)
	{
		bool isLadder = pPlayer->IsLadderUsable();

		const bool onLadder = pPlayer->GetPlayerStats()->isOnLadder.Value();

		IMovementController* pMC = pPlayer->GetMovementController();
		if (!pMC)
			return;

		SMovementState info;
		pMC->GetMovementState(info);
		if (CheckItemsInProximity(info.eyePosition, info.eyeDirection, false) || (isLadder && !onLadder))
		{
			//Offhand pick ups are disabled in MP (check only for items)
			if (CHUD* pHUD = g_pGame->GetHUD())
			{
				if (isLadder)
				{
					pHUD->GetCrosshair()->SetUsability(1, "@use_ladder");
					m_usable = true;
					return;
				}

				if (CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(m_crosshairId)))
				{
					CryFixedStringT<128> itemName(pItem->GetEntity()->GetClass()->GetName());
					if (!strcmp(itemName.c_str(), "CustomAmmoPickup"))
					{
						SmartScriptTable props;
						if (pItem->GetEntity()->GetScriptTable() && pItem->GetEntity()->GetScriptTable()->GetValue("Properties", props))
						{
							const char* name = NULL;
							props->GetValue("AmmoName", name);
							itemName.assign(name);
						}
					}

					if (pItem->GetIWeapon())
					{
						IEntityClass* pItemClass = pItem->GetEntity()->GetClass();
						const bool isSocom = pItemClass == CItem::sSOCOMClass;
						IItem* pCurrentItem = m_pItemSystem->GetItem(pPlayer->GetInventory()->GetItemByClass(pItemClass));
						if ((!isSocom && pCurrentItem) ||
							(isSocom && pCurrentItem && pCurrentItem->IsDualWield()))
						{
							if (pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
								pHUD->GetCrosshair()->SetUsability(1, "@game_take_ammo_from", itemName.c_str());
							else
								pHUD->GetCrosshair()->SetUsability(2, "@weapon_ammo_full", itemName.c_str());
						}
						else
						{
							int typ = CanPerformPickUp(pPlayer, NULL, true);
							if (m_preHeldEntityId && !pPlayer->CheckInventoryRestrictions(m_pEntitySystem->GetEntity(m_preHeldEntityId)->GetClass()->GetName()))
							{
								IItem* pExchangedItem = GetExchangeItem(pPlayer);
								if (pExchangedItem)
								{
									pHUD->GetCrosshair()->SetUsability(1, "@game_exchange_weapon",
										pExchangedItem->GetEntity()->GetClass()->GetName(), itemName.c_str());
								}
								else
									pHUD->GetCrosshair()->SetUsability(2, "@inventory_full");
							}
							else
							{
								pHUD->GetCrosshair()->SetUsability(true, "@pick_weapon", itemName.c_str());
							}
						}
					}
					else
					{
						if (pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
							pHUD->GetCrosshair()->SetUsability(1, "@pick_item", itemName.c_str());
						else
							pHUD->GetCrosshair()->SetUsability(2, "@weapon_ammo_full", itemName.c_str());
					}
				}
			}
			m_usable = true;
		}
		else if (m_usable)
		{
			if (g_pGame->GetHUD())
				g_pGame->GetHUD()->GetCrosshair()->SetUsability(0, "");
			m_usable = false;
		}
	}
}

//=============================================================
void COffHand::UpdateHeldObject()
{
	CActor* pPlayer = GetOwnerActor();
	if (!pPlayer)
		return;

	IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
	if (!pEntity)
	{
		SetHeldEntityId(0);

		FinishAction(eOHA_RESET);

		return;
	}

	if (m_constraintStatus == ConstraintStatus::WaitForPhysicsUpdate || m_constraintStatus == ConstraintStatus::Active)
	{
		IPhysicalEntity* pPE = pEntity->GetPhysics();
		if ((pPE && m_constraintId) ||
			(pPE && !m_constraintId && pPE->GetType() == PE_ARTICULATED))
		{
			pe_status_constraint state;
			state.id = m_constraintId;
			if (m_constraintStatus == ConstraintStatus::WaitForPhysicsUpdate)
			{
				if (pPE->GetStatus(&state))
				{
					m_constraintStatus = ConstraintStatus::Active;
				}
				else
					return;
			}
			else if (m_constraintStatus == ConstraintStatus::Active)
			{
				//The constraint was removed (the object was broken/destroyed)
				//means a boid that died in player hands (and now it's a ragdoll,which could cause collision issues)
				if (!pPE->GetStatus(&state))
				{
					if (m_currentState & (eOHS_HOLDING_OBJECT | eOHS_THROWING_OBJECT | eOHS_HOLDING_NPC | eOHS_THROWING_NPC)) //CryMP: don't release untill we're actually holding it
					{
						if (pPlayer->IsClient())
						{
							if (m_mainHand && m_mainHand->IsBusy())
							{
								m_mainHand->SetBusy(false);
							}

							if (m_currentState != eOHS_THROWING_OBJECT)
							{
								if (m_currentState == eOHS_MELEE)
								{
									GetScheduler()->Reset();
								}

								SetOffHandState(eOHS_HOLDING_OBJECT);

								OnAction(GetOwnerId(), ActionId("use"), eAAM_OnPress, 0.0f);
								OnAction(GetOwnerId(), ActionId("use"), eAAM_OnRelease, 0.0f);
							}
							else
							{
								OnAction(GetOwnerId(), ActionId("use"), eAAM_OnRelease, 0.0f);
							}
							m_constraintId = 0;
							m_constraintStatus = ConstraintStatus::Broken;
						}
						return;
					}
				}
			}
		}
	}

	if (!m_stats.fp)
	{
		return;
	}

	//Update entity WorldTM 
	int id = eIGS_FirstPerson;

	Matrix34 finalMatrix(Matrix34(GetSlotHelperRotation(id, "item_attachment", true)));

	finalMatrix.Scale(m_holdScale);

	Vec3 pos = GetSlotHelperPos(id, "item_attachment", true);

	finalMatrix.SetTranslation(pos);

	finalMatrix = finalMatrix * m_holdOffset;

	//This is need it for breakable/joint-constraints stuff
	if (IPhysicalEntity* pPhys = pEntity->GetPhysics())
	{
		if (!pPlayer->IsRemote())
		{
			pe_action_set_velocity v;
			v.v = Vec3(0.01f, 0.01f, 0.01f);
			pPhys->Action(&v);

			//For boids
			if (pPhys->GetType() == PE_PARTICLE)
			{
				pEntity->SetSlotLocalTM(0, IDENTITY);
			}
		}
	}
	//====================================
	bool hasAuthority(gEnv->bServer);
	if (gEnv->bMultiplayer && !gEnv->bServer)
	{
		/*INetChannel* pClientChannel = m_pGameFramework->GetClientChannel();
		if (pClientChannel && m_pGameFramework->GetNetContext()->RemoteContextHasAuthority(pClientChannel, m_heldEntityId))
		{
			hasAuthority = true;
		}*/
		if (pPlayer->GetHeldObjectId() == m_heldEntityId)
		{
			hasAuthority = true;
		}

		//We haven't received authorization from server, check if it's a client entity e.g. chickens etc
		if (!hasAuthority)
		{
			//CryMP: Let us have fun with chickens in MP...
			const bool bClientEntity = (pEntity->GetFlags() & ENTITY_FLAG_CLIENT_ONLY);
			if (bClientEntity)
			{
				hasAuthority = true;
			}
		}
	}
	if (hasAuthority || pPlayer->IsRemote())
	{
		pEntity->SetWorldTM(finalMatrix);
	}
}

//===========================================================
void COffHand::UpdateGrabbedNPCState()
{
	//Get actor
	CActor* pActor = static_cast<CActor*>(m_pActorSystem->GetActor(m_heldEntityId));
	CActor* pPlayer = GetOwnerActor();
	SActorStats* pStats = pPlayer ? pPlayer->GetActorStats() : NULL;

	if (pActor && pStats)
	{
		RunEffectOnGrabbedNPC(pActor);

		// workaround for playing a different facial animation after 5 seconds
		pStats->grabbedTimer += gEnv->pTimer->GetFrameTime();
		if (m_grabbedNPCSpecies == eGCT_HUMAN && pStats->grabbedTimer > 5.2f)
			pActor->SetAnimationInput("Action", "grabStruggleFP2");
		else
			pActor->SetAnimationInput("Action", "grabStruggleFP");

		//Actor died while grabbed
		if ((pActor->GetHealth() <= 0 && !m_npcWasDead) || pActor->IsFallen() || (pStats->isRagDoll))
		{
			if (m_mainHand && m_mainHand->IsBusy())
				m_mainHand->SetBusy(false);
			//Already throwing (do nothing)
			if (m_currentState & (eOHS_GRABBING_NPC | eOHS_HOLDING_NPC))
			{
				//Drop NPC
				SetOffHandState(eOHS_HOLDING_NPC);

				OnAction(GetOwnerId(), ActionId("use"), eAAM_OnPress, 0.0f);
				OnAction(GetOwnerId(), ActionId("use"), eAAM_OnRelease, 0.0f);
			}
			else if (m_currentState & eOHS_THROWING_NPC)
			{
				OnAction(GetOwnerId(), ActionId("use"), eAAM_OnRelease, 0.0f);
			}

			if (!pStats->isRagDoll)
				m_npcWasDead = true;

		}
	}

}

//============================================================
void COffHand::PostFilterView(struct SViewParams& viewParams)
{
	//This should be only be called when grabbing/holding/throwing an NPC from CPlayer::PostUpdateView()
	IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);

	UpdateGrabbedNPCState();
	UpdateGrabbedNPCWorldPos(pEntity, &viewParams);
}

//===============================================
void COffHand::PostUpdate(float frameTime)
{
	//Update character position here when the game is paused, if I don't do so, character goes invisible!
	//IGameObject::PostUpdate() is updated when the game is paused.
	//PostUpdated are enabled/disabled when grabbing/throwing a character
	if (m_pGameFramework->IsGamePaused() && m_currentState & (eOHS_GRABBING_NPC | eOHS_HOLDING_NPC | eOHS_THROWING_NPC))
	{
		//This should be only be called when grabbing/holding/throwing an NPC from CPlayer::PostUpdateView()
		IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);

		//UpdateGrabbedNPCState();
		UpdateGrabbedNPCWorldPos(pEntity, NULL);
	}

}

//============================================================
void COffHand::UpdateGrabbedNPCWorldPos(IEntity* pEntity, struct SViewParams* viewParams)
{
	if (!pEntity || !m_stats.fp)
		return;

	Matrix34 neckFinal = Matrix34::CreateIdentity();

	if (viewParams)
	{
		CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor());
		if (!pPlayer)
			return;

		const SPlayerStats* stats = pPlayer->GetPlayerStats();
		Quat wQuat = (viewParams->rotation * Quat::CreateRotationXYZ(stats->FPWeaponAnglesOffset * gf_PI / 180.0f));
		wQuat *= Quat::CreateSlerp(viewParams->currentShakeQuat, IDENTITY, 0.5f);
		wQuat.Normalize();

		Vec3 itemAttachmentPos = GetSlotHelperPos(0, "item_attachment", false);
		itemAttachmentPos = stats->FPWeaponPos + wQuat * itemAttachmentPos;

		neckFinal.SetRotation33(Matrix33(viewParams->rotation * Quat::CreateRotationZ(gf_PI)));
		neckFinal.SetTranslation(itemAttachmentPos);

		ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
		if (!pCharacter)
			return;

		ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
		if (!pSkeletonPose)
			return;

		int neckId = 0;
		Vec3 specialOffset(0.0f, -0.07f, -0.09f);

		switch (m_grabbedNPCSpecies)
		{
		case eGCT_HUMAN:  neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
			specialOffset.Set(0.0f, 0.0f, 0.0f);
			break;

		case eGCT_ALIEN:  neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
			specialOffset.Set(0.0f, 0.0f, -0.09f);
			break;

		case eGCT_TROOPER: neckId = pSkeletonPose->GetJointIDByName("Bip01 Head");
			break;
		}

		Vec3 neckLOffset(pSkeletonPose->GetAbsJointByID(neckId).t);
		//Vec3 charOffset(pEntity->GetSlotLocalTM(0,false).GetTranslation());
		//if(m_grabbedNPCSpecies==eGCT_TROOPER)
		//charOffset.Set(0.0f,0.0f,0.0f);
		//Vec3 charOffset(0.0f,0.0f,0.0f);		//For some reason the above line didn't work with the trooper...

		//float white[4] = {1,1,1,1};
		//gEnv->pRenderer->Draw2dLabel( 100, 50, 2, white, false, "neck: %f %f %f", neckLOffset.x,neckLOffset.y,neckLOffset.z );
		//gEnv->pRenderer->Draw2dLabel( 100, 70, 2, white, false, "char: %f %f %f", charOffset.x,charOffset.y,charOffset.z );

		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(neckFinal.GetTranslation(),0.08f,ColorB(255,0,0));

		neckFinal.AddTranslation(Quat(neckFinal) * -(neckLOffset + specialOffset));
		m_lastNPCMatrix = neckFinal;
	}
	else
	{
		Vec3 itemAttachmentPos = GetSlotHelperPos(0, "item_attachment", true);
		neckFinal = m_lastNPCMatrix;
		neckFinal.SetTranslation(itemAttachmentPos);

		if (ICharacterInstance* pCharacter = pEntity->GetCharacter(0))
		{
			ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
			assert(pSkeletonPose && "COffHand::UpdateGrabbedNPCWorldPos --> Actor entity has no skeleton!!");
			if (!pSkeletonPose)
				return;

			int neckId = 0;
			Vec3 specialOffset(0.0f, -0.07f, -0.09f);

			switch (m_grabbedNPCSpecies)
			{
			case eGCT_HUMAN:  neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
				specialOffset.Set(0.0f, 0.0f, 0.0f);
				break;

			case eGCT_ALIEN:  neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
				specialOffset.Set(0.0f, 0.0f, -0.09f);
				break;

			case eGCT_TROOPER: neckId = pSkeletonPose->GetJointIDByName("Bip01 Head");
				break;
			}

			Vec3 neckLOffset(pSkeletonPose->GetAbsJointByID(neckId).t);
			neckFinal.AddTranslation(Quat(neckFinal) * -(neckLOffset + specialOffset));
		}

	}

	float EntRotZ = RAD2DEG(Quat(neckFinal).GetRotZ());

	pEntity->SetWorldTM(neckFinal);

	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(neckFinal.GetTranslation(),0.08f,ColorB(0,255,0));
}

//=============================================================
bool COffHand::GetGrabbedActorNeckWorldPos(IEntity* pEntity, Vec3& outNeckPos) const
{
	if (!pEntity)
		return false;

	ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
	if (!pCharacter)
		return false;

	ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
	if (!pSkeletonPose)
		return false;

	int neckId = 0;
	Vec3 specialOffset(0.0f, -0.07f, -0.09f);

	switch (m_grabbedNPCSpecies)
	{
	case eGCT_HUMAN:
		neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
		specialOffset.Set(0.0f, 0.0f, 0.0f);
		break;
	case eGCT_ALIEN:
		neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
		specialOffset.Set(0.0f, 0.0f, -0.09f);
		break;
	case eGCT_TROOPER:
		neckId = pSkeletonPose->GetJointIDByName("Bip01 Head");
		break;
	default:
		return false;
	}

	if (neckId < 0)
		return false;

	const QuatT neckJoint = pSkeletonPose->GetAbsJointByID(neckId);
	outNeckPos = pEntity->GetWorldTM().TransformPoint(neckJoint.t + specialOffset);
	return true;
}

//=============================================================
void COffHand::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	s_actionHandler.Dispatch(this, actorId, actionId, activationMode, value);
}

//-------------
bool COffHand::OnActionUse(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_USE, INPUT_USE, activationMode);
}

//------------
bool COffHand::OnActionAttack(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_USE, INPUT_LBM, activationMode);
}

//------------
bool COffHand::OnActionDrop(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_USE, INPUT_RBM, activationMode);
}

//------------
bool COffHand::OnActionThrowGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_THROW_GRENADE, INPUT_DEF, activationMode);
}

//------------
bool COffHand::OnActionXIThrowGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress)
	{
		m_fGrenadeThrowTimer = 0.0f;
		RequireUpdate(eIUS_General);
	}
	else if (activationMode == eAAM_OnRelease)
	{
		if (m_fGrenadeThrowTimer >= 0.0f)
		{
			ProcessOffHandActions(eOHA_THROW_GRENADE, INPUT_DEF, eAAM_OnPress);
			ProcessOffHandActions(eOHA_THROW_GRENADE, INPUT_DEF, eAAM_OnRelease);
			m_fGrenadeThrowTimer = -1.0f;
		}
	}
	return true;
}

//-----------
bool COffHand::OnActionSwitchGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_SWITCH_GRENADE, INPUT_DEF, activationMode);
}

//-----------
bool COffHand::OnActionXISwitchGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress)
	{
		m_fGrenadeToggleTimer = 0.0f;
		RequireUpdate(eIUS_General);
	}
	else if (activationMode == eAAM_OnRelease)
	{
		m_fGrenadeToggleTimer = -1.0f;
	}
	return true;
}

//-----------
bool COffHand::OnActionSpecial(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_MELEE_ATTACK, INPUT_DEF, activationMode);
}

//----------
bool COffHand::ProcessOffHandActions(EOffHandActions eOHA, int input, int activationMode, float value /*= 0.0f*/)
{
	//Test if action is possible
	if (!EvaluateStateTransition(eOHA, activationMode, input))
		return false;

	if (!PreExecuteAction(eOHA, activationMode))
		return false;

	if (eOHA == eOHA_SWITCH_GRENADE)
	{
		StartSwitchGrenade(false, (value != 0.0f) ? true : false);
	}
	else if (eOHA == eOHA_THROW_GRENADE)
	{
		if (!m_fm->OutOfAmmo() && GetFireModeIdx(m_fm->GetName()) < MAX_GRENADE_TYPES)
		{
			PerformThrow(activationMode, 0, GetCurrentFireMode());
		}
	}
	else if (eOHA == eOHA_MELEE_ATTACK)
	{
		MeleeAttack();
	}
	else if (eOHA == eOHA_USE)
	{
		if (m_currentState & eOHS_INIT_STATE)
		{
			const int typ = CanPerformPickUp(GetOwnerActor(), nullptr, true);
			if (typ == OH_GRAB_ITEM)
			{
				StartPickUpItem();
			}
			else if (typ == OH_GRAB_OBJECT)
			{
				if (gEnv->bMultiplayer && g_pGameCVars->mp_pickupObjects && m_pGameFramework->IsImmersiveMPEnabled())
				{
					if (Request_PickUpObject_MP())
					{
						return true;
					}
				}

				StartPickUpObject(m_preHeldEntityId, false);
			}
			else if (typ == OH_GRAB_NPC)
			{
				StartPickUpObject(m_preHeldEntityId, true);
			}
			else if (typ == OH_NO_GRAB)
			{
				CancelAction();
				return false;
			}

		}
		else if (m_currentState & (eOHS_HOLDING_OBJECT | eOHS_THROWING_OBJECT))
		{
			//CryMP throwing object called here
			StartThrowObject(m_heldEntityId, activationMode, false);
		}
		else if (m_currentState & (eOHS_HOLDING_NPC | eOHS_THROWING_NPC))
		{
			StartThrowObject(m_heldEntityId, activationMode, true);
		}
	}

	return true;
}

//==================================================================
bool COffHand::EvaluateStateTransition(int requestedAction, int activationMode, int	inputMethod)
{
	switch (requestedAction)
	{
	case eOHA_SWITCH_GRENADE:
		if (activationMode == eAAM_OnPress && m_currentState == eOHS_INIT_STATE)
		{
			return true;
		}
		break;

	case eOHA_THROW_GRENADE:
		if (activationMode == eAAM_OnPress && m_currentState == eOHS_INIT_STATE)
		{
			//Don't allow throwing grenades under water.
			if (CPlayer* pPlayer = GetOwnerPlayer())
			{
				const SPlayerStats* stats = pPlayer->GetPlayerStats();
				if ((stats->worldWaterLevel + 0.1f) > stats->FPWeaponPos.z)
					return false;
			}

			//Don't throw if there's no ammo (or not fm)
			if (m_fm && !m_fm->OutOfAmmo() && m_nextThrowTimer <= 0.0f)
				return true;
		}
		else if (activationMode == eAAM_OnRelease && m_currentState == eOHS_HOLDING_GRENADE)
		{
			return true;
		}
		break;

	case eOHA_USE:
		//Evaluate mouse inputs first
		if (inputMethod == INPUT_LBM)
		{
			//Two handed object throw/drop is handled with the mouse now...
			if (activationMode == eAAM_OnPress && m_currentState & eOHS_HOLDING_OBJECT && m_grabType == GRAB_TYPE_TWO_HANDED)
			{
				m_forceThrow = true;
				return true;
			}
			else if (activationMode == eAAM_OnRelease && m_currentState & eOHS_THROWING_OBJECT && m_grabType == GRAB_TYPE_TWO_HANDED)
			{
				return true;
			}
		}
		else if (inputMethod == INPUT_RBM)
		{
			//Two handed object throw/drop is handled with the mouse now...
			if (activationMode == eAAM_OnPress && m_currentState & eOHS_HOLDING_OBJECT)
			{
				m_forceThrow = false;
				return true;
			}
			else if (activationMode == eAAM_OnRelease && m_currentState & eOHS_THROWING_OBJECT)
			{
				return true;
			}
		}
		else if (activationMode == eAAM_OnPress && (m_currentState & (eOHS_INIT_STATE | eOHS_HOLDING_OBJECT | eOHS_HOLDING_NPC)))
		{
			m_forceThrow = true;
			return true;
		}
		else if (activationMode == eAAM_OnRelease && (m_currentState & (eOHS_THROWING_OBJECT | eOHS_THROWING_NPC)))
		{
			return true;
		}
		break;

	case eOHA_MELEE_ATTACK:
		if (activationMode == eAAM_OnPress && m_currentState == eOHS_HOLDING_OBJECT && m_grabType == GRAB_TYPE_ONE_HANDED)
		{
			return true;
		}
		break;
	}

	return false;
}

//==================================================================
bool COffHand::PreExecuteAction(int requestedAction, int activationMode, bool forceSelect)
{
	if (m_currentState != eOHS_INIT_STATE && requestedAction != eOHA_REINIT_WEAPON)
		return true;

	//We have to test the main weapon/item state, in some cases the offhand could not perform the action
	CActor* pActor = GetOwnerActor();
	if (!pActor || pActor->GetHealth() <= 0 || (pActor->IsSwimming() && requestedAction != eOHA_USE))
		return false;

	CItem* pMain = GetActorItem(pActor);
	CWeapon* pMainWeapon = static_cast<CWeapon*>(pMain ? pMain->GetIWeapon() : NULL);

	bool exec = true;

	if (pMain && pMain->IsMounted())
		return false;

	if (pMainWeapon)
		exec &= !pMainWeapon->IsModifying() && !pMainWeapon->IsReloading() && !pMainWeapon->IsSwitchingFireMode() && !pMainWeapon->IsFiring();


	if (pMainWeapon && (pMainWeapon->IsZoomed() || pMainWeapon->IsZooming()) && (requestedAction == eOHA_THROW_GRENADE))
	{
		pMainWeapon->ExitZoom();
		pMainWeapon->ExitViewmodes();
	}

	if (exec)
	{
		if ((!gEnv->bMultiplayer || g_pGameCVars->mp_animationGrenadeSwitch) || (requestedAction != eOHA_SWITCH_GRENADE))
		{
			SetHand(eIH_Left);		//Here??

			if ((GetEntity()->IsHidden() || forceSelect) && activationMode == eAAM_OnPress)
			{
				m_stats.fp = !m_stats.fp;

				GetScheduler()->Lock(true);
				Select(true);
				GetScheduler()->Lock(false);
				SetBusy(false);
			}
		}
		m_mainHand = pMain;
		m_mainHandWeapon = pMainWeapon;
		m_mainHandIsDualWield = false;

		if (requestedAction == eOHA_THROW_GRENADE)
		{
			if (m_mainHand && m_mainHand->TwoHandMode() == 1)
			{
				GetOwnerActor()->HolsterItem(true);
				m_mainHand = m_mainHandWeapon = NULL;
			}
			else if (m_mainHand && m_mainHand->IsDualWield() && m_mainHand->GetDualWieldSlave())
			{
				m_mainHand = static_cast<CItem*>(m_mainHand->GetDualWieldSlave());
				m_mainHandIsDualWield = true;
				m_mainHand->Select(false);
			}
		}
	}
	else if (requestedAction == eOHA_REINIT_WEAPON)
	{
		m_mainHand = pMain;
		m_mainHandWeapon = pMainWeapon;
	}

	return exec;
}

//==================================================================
void COffHand::NetStartFire()
{
	if (GetEntity()->IsHidden()) // this is need for network triggered grenade throws to trigger updates and what not..
	{
		m_stats.fp = !m_stats.fp;

		GetScheduler()->Lock(true);
		Select(true);
		GetScheduler()->Lock(false);
		SetBusy(false);
	}

	CWeapon::NetStartFire();

	AttachGrenadeToHand(GetCurrentFireMode(), m_stats.fp, true);

	//Handle FP Spectator
	if (m_stats.fp)
	{
		m_mainHand = static_cast<CItem*>(GetOwnerActor()->GetCurrentItem());
		if (m_mainHand)
		{
			//if (!(m_currentState & (eOHS_THROWING_NPC | eOHS_THROWING_OBJECT)))
			{
				if (m_mainHandWeapon && m_mainHandWeapon->IsWeaponRaised())
				{
					m_mainHandWeapon->RaiseWeapon(false, true);
					m_mainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
				}
				m_mainHand->PlayAction(g_pItemStrings->offhand_on);
				m_mainHand->SetActionSuffix("akimbo_");
			}
		}
		if (m_mainHandWeapon && m_mainHandWeapon->GetEntity()->GetClass() == CItem::sFistsClass)
		{
			CFists* pFists = static_cast<CFists*>(m_mainHandWeapon);
			pFists->RequestAnimState(CFists::eFAS_FIGHT);
		}
	}
}

//=============================================================================
void COffHand::NetStopFire()
{
	CWeapon::NetStopFire();

	AttachGrenadeToHand(GetCurrentFireMode(), m_stats.fp, false);

	//Handle FP Spectator
	if (m_stats.fp)
	{
		GetScheduler()->TimerAction(GetCurrentAnimationTime(CItem::eIGS_FirstPerson), CSchedulerAction<FinishGrenadeAction>::Create(FinishGrenadeAction(this, m_mainHand)), false);
	}
}


//=============================================================================
//This function seems redundant...
void COffHand::CancelAction()
{
	SetOffHandState(eOHS_INIT_STATE);
}

//=============================================================================
void COffHand::FinishAction(EOffHandActions eOHA)
{
	switch (eOHA)
	{
	case eOHA_SWITCH_GRENADE:
		EndSwitchGrenade();
		break;

	case eOHA_PICK_ITEM:
		EndPickUpItem();
		break;

	case eOHA_GRAB_NPC:
		SetOffHandState(eOHS_HOLDING_NPC);
		break;

	case eOHA_THROW_NPC:
		GetScheduler()->TimerAction(300, CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_RESET, this)), true);
		ThrowNPC(m_heldEntityId);

		SetHeldEntityId(0);

		SetOffHandState(eOHS_TRANSITIONING);
		break;

	case eOHA_PICK_OBJECT:
		SetOffHandState(eOHS_HOLDING_OBJECT);
		break;

	case eOHA_THROW_OBJECT:
	{
		// after it's thrown, wait 500ms to enable collisions again
		GetScheduler()->TimerAction(500, CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_RESET, this)), true);

		SetHeldEntityId(0);

		IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
		if (pEntity)
		{
			IPhysicalEntity* pPhys = pEntity->GetPhysics();
			if (pPhys && pPhys->GetType() == PE_PARTICLE)
				pEntity->SetSlotLocalTM(0, m_intialBoidLocalMatrix);

		}

		SetOffHandState(eOHS_TRANSITIONING);
	}
	break;

	case eOHA_RESET:
		//Reset main weapon status and reset offhand
	{
		CActor* pActor = GetOwnerActor();
		if (m_prevMainHandId && !pActor->IsSwimming())
		{
			pActor->SelectItem(m_prevMainHandId, false);
			m_mainHand = static_cast<CItem*>(pActor->GetCurrentItem());
			m_mainHandWeapon = static_cast<CWeapon*>(m_mainHand ? m_mainHand->GetIWeapon() : NULL);
		}

		if (pActor->IsClient())
		{
			RequestFireMode(m_lastFireModeId); //Works for MP as well
		}

		float timeDelay = 0.1f;
		if (!m_mainHand)
		{
			SActorStats* pStats = pActor->GetActorStats();
			if (!pActor->ShouldSwim() && !m_bCutscenePlaying && (pStats && !pStats->inFreefall.Value()))
				pActor->HolsterItem(false);
		}
		else if (!m_mainHandIsDualWield && !m_prevMainHandId)
		{
			m_mainHand->ResetDualWield();
			m_mainHand->PlayAction(g_pItemStrings->offhand_off, 0, false, CItem::eIPAF_Default | CItem::eIPAF_NoBlend);
			timeDelay = (m_mainHand->GetCurrentAnimationTime(CItem::eIGS_FirstPerson) + 50) * 0.001f;
		}
		else if (m_mainHandIsDualWield)
		{
			m_mainHand->Select(true);
		}
		SetResetTimer(timeDelay);
		RequireUpdate(eIUS_General);
		m_prevMainHandId = 0;

		//turn off collision with thrown objects
		//if (m_heldEntityId)
		//	SetIgnoreCollisionsWithOwner(false, m_heldEntityId);

		SetHeldEntityId(0);

	}

	break;

	case	eOHA_FINISH_MELEE:	
	{
		if (m_heldEntityId)
		{
			SetOffHandState(eOHS_HOLDING_OBJECT);
		}
	}
	break;

	case eOHA_FINISH_AI_THROW_GRENADE:
	{
		// Reset the main weapon after a grenade throw.
		CActor* pActor = GetOwnerActor();
		if (pActor)
		{
			CItem* pMain = GetActorItem(pActor);
			if (pMain)
				pMain->PlayAction(g_pItemStrings->idle, 0, false, CItem::eIPAF_Default | CItem::eIPAF_NoBlend);
		}
	}
	break;
	}
}

//==============================================================================
void COffHand::Freeze(bool freeze)
{
	CWeapon::Freeze(freeze);

	if (!freeze && m_currentState == eOHS_HOLDING_GRENADE)
	{
		FinishAction(eOHA_RESET);
		CancelAction();
	}
}

//==============================================================================
void COffHand::SetOffHandState(EOffHandStates eOHS)
{
	m_currentState = eOHS;

	if (eOHS & eOHS_INIT_STATE)
	{
		m_mainHand = m_mainHandWeapon = nullptr;
		m_preHeldEntityId = 0;
		SetHeldEntityId(0);
		m_mainHandIsDualWield = false;
		Select(false);
	}
}

//==============================================================================
void COffHand::StartSwitchGrenade(bool xi_switch, bool fakeSwitch)
{
	//Iterate different firemodes 
	int firstMode = GetCurrentFireMode();
	int newMode = GetNextFireMode(GetCurrentFireMode());

	while (newMode != firstMode)
	{
		//Fire mode idx>2 means its a throw object/npc firemode
		if (GetFireMode(newMode)->OutOfAmmo() || newMode >= MAX_GRENADE_TYPES)
			newMode = GetNextFireMode(newMode);
		else
		{
			m_lastFireModeId = newMode;
			break;
		}
	}

	//We didn't find a fire mode with ammo
	if (newMode == firstMode)
	{
		CancelAction();

		if (m_ownerId == LOCAL_PLAYER_ENTITY_ID && g_pGame->GetHUD())
			g_pGame->GetHUD()->FireModeSwitch(true);

		return;
	}
	else if (!fakeSwitch)
		RequestFireMode(newMode);

	//No animation in multiplayer or when using the gamepad
	//CryMP: Optional animations
	if ((gEnv->bMultiplayer && (!g_pGameCVars->mp_animationGrenadeSwitch || !m_stats.fp)) || xi_switch)
	{
		SetOffHandState(eOHS_SWITCHING_GRENADE);

		SetResetTimer(0.3f); //Avoid spamming keyboard issues
		RequireUpdate(eIUS_General);
		return;
	}

	//Unzoom weapon if neccesary
	if (m_mainHandWeapon && (m_mainHandWeapon->IsZoomed() || m_mainHandWeapon->IsZooming()))
	{
		m_mainHandWeapon->ExitZoom();
		m_mainHandWeapon->ExitViewmodes();
	}

	m_mainHandIsDualWield = false;

	//A new grenade type/fire mode was chosen
	if (m_mainHand && (m_mainHand->TwoHandMode() != 1) && !m_mainHand->IsDualWield())
	{

		if (m_mainHandWeapon && m_mainHandWeapon->GetEntity()->GetClass() == CItem::sFistsClass)
		{
			CFists* pFists = static_cast<CFists*>(m_mainHandWeapon);
			pFists->RequestAnimState(CFists::eFAS_FIGHT);
		}

		if (m_mainHandWeapon && m_mainHandWeapon->IsWeaponRaised())
		{
			m_mainHandWeapon->RaiseWeapon(false, true);
			m_mainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
		}

		//Play deselect left hand on main item
		m_mainHand->PlayAction(g_pItemStrings->offhand_on);
		m_mainHand->SetActionSuffix("akimbo_");

		GetScheduler()->TimerAction(m_mainHand->GetCurrentAnimationTime(eIGS_FirstPerson),
			CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_SWITCH_GRENADE, this)), false);

	}
	else
	{
		//No main item or holstered (wait 100ms)
		if (m_mainHand && m_mainHand->IsDualWield() && m_mainHand->GetDualWieldSlave())
		{
			m_mainHand = static_cast<CItem*>(m_mainHand->GetDualWieldSlave());
			m_mainHand->Select(false);
			m_mainHandIsDualWield = true;
		}
		else
		{
			GetOwnerActor()->HolsterItem(true);
			m_mainHand = m_mainHandWeapon = NULL;
		}
		GetScheduler()->TimerAction(100, CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_SWITCH_GRENADE, this)), false);
	}

	//Change offhand state
	SetOffHandState(eOHS_SWITCHING_GRENADE);

	if (fakeSwitch)
		AttachGrenadeToHand(firstMode);
	else
		AttachGrenadeToHand(newMode);
}

//==============================================================================
void COffHand::EndSwitchGrenade()
{
	//Play select grenade animation (and un-hide grenade geometry)
	PlayAction(g_pItemStrings->select_grenade);
	//HideItem(false);
	GetScheduler()->TimerAction(GetCurrentAnimationTime(CItem::eIGS_FirstPerson),
		CSchedulerAction<FinishGrenadeAction>::Create(FinishGrenadeAction(this, m_mainHand)), false);
}

//==============================================================================
void COffHand::PerformThrow(float speedScale)
{
	if (!m_fm)
		return;

	m_fm->StartFire();

	CThrow* pThrow = static_cast<CThrow*>(m_fm);
	pThrow->SetSpeedScale(speedScale);

	m_fm->StopFire();
	pThrow->ThrowingGrenade(true);

	// Schedule to revert back to main weapon.
	GetScheduler()->TimerAction(2000,
		CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_FINISH_AI_THROW_GRENADE, this)), false);
}

//===============================================================================
void COffHand::PerformThrow(int activationMode, EntityId throwableId, int oldFMId /* = 0 */, bool isLivingEnt /*=false*/)
{
	if (!m_fm)
		return;

	if (activationMode == eAAM_OnPress)
	{
		SetOffHandState(eOHS_HOLDING_GRENADE);
	}

	//Throw objects...
	if (throwableId && activationMode == eAAM_OnPress)
	{
		if (!isLivingEnt)
		{
			CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor());
			if (pPlayer)
			{
				pPlayer->NotifyObjectGrabbed(false, throwableId, false);
			}

			SetOffHandState(eOHS_THROWING_OBJECT);

			CThrow* pThrow = static_cast<CThrow*>(m_fm);
			pThrow->SetThrowable(throwableId, m_forceThrow, 
				CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_THROW_OBJECT, this))
			);
		}
		else
		{
			SetOffHandState(eOHS_THROWING_NPC);

			CThrow* pThrow = static_cast<CThrow*>(m_fm);
			pThrow->SetThrowable(throwableId, true, 
				CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_THROW_NPC, this))
			);
		}
		m_forceThrow = false;

		// enable leg IK again
		EnableFootGroundAlignment(true);
	}
	//--------------------------

	if (activationMode == eAAM_OnPress)
	{
		if (!m_fm->IsFiring() && m_nextThrowTimer < 0.0f)
		{
			if (m_currentState == eOHS_HOLDING_GRENADE)
			{
				AttachGrenadeToHand(GetCurrentFireMode(), m_stats.fp);
			}

			m_fm->StartFire();

			SetBusy(false);

			if (m_mainHand && m_fm->IsFiring())
			{
				if (!(m_currentState & (eOHS_THROWING_NPC | eOHS_THROWING_OBJECT)))
				{
					if (m_mainHandWeapon && m_mainHandWeapon->IsWeaponRaised())
					{
						m_mainHandWeapon->RaiseWeapon(false, true);
						m_mainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
					}

					m_mainHand->PlayAction(g_pItemStrings->offhand_on);
					m_mainHand->SetActionSuffix("akimbo_");
				}
			}
			if (!throwableId)
			{
				if (m_mainHandWeapon && m_mainHandWeapon->GetEntity()->GetClass() == CItem::sFistsClass)
				{
					CFists* pFists = static_cast<CFists*>(m_mainHandWeapon);
					pFists->RequestAnimState(CFists::eFAS_FIGHT);
				}
			}
		}

	}
	else if (activationMode == eAAM_OnRelease && m_nextThrowTimer <= 0.0f)
	{
		CThrow* pThrow = static_cast<CThrow*>(m_fm);
		if (m_currentState != eOHS_HOLDING_GRENADE)
		{
			pThrow->ThrowingGrenade(false);
		}
		else if (m_currentState == eOHS_HOLDING_GRENADE)
		{
			SetOffHandState(eOHS_THROWING_GRENADE);
		}
		else
		{
			CancelAction();
			return;
		}

		m_nextThrowTimer = 60.0f / m_fm->GetFireRate();

		m_fm->StopFire();
		pThrow->ThrowingGrenade(true);

		if (m_fm->IsFiring() && m_currentState == eOHS_THROWING_GRENADE)
		{
			GetScheduler()->TimerAction(GetCurrentAnimationTime(CItem::eIGS_FirstPerson), CSchedulerAction<FinishGrenadeAction>::Create(FinishGrenadeAction(this, m_mainHand)), false);
		}
	}
}

//--------------
int COffHand::CanPerformPickUp(CActor* pActor, IPhysicalEntity* pPhysicalEntity /*=NULL*/, bool getEntityInfo /*= false*/)
{
	if (!pActor || (!pActor->IsClient() && !pActor->IsFpSpectatorTarget())) //CryMP Fp Spec enable pickup HUD 
		return OH_NO_GRAB;

	IMovementController* pMC = pActor->GetMovementController();
	if (!pMC)
		return OH_NO_GRAB;

	SMovementState info;
	pMC->GetMovementState(info);

	if (gEnv->bMultiplayer && !g_pGameCVars->mp_pickupObjects)
	{
		return CheckItemsInProximity(info.eyePosition, info.eyeDirection, getEntityInfo);
	}

	EStance playerStance = pActor->GetStance();

	if (!getEntityInfo)
	{
		//Prevent pick up message while can not pick up
		IItem* pItem = pActor->GetCurrentItem(false);
		CWeapon* pMainWeapon = pItem ? static_cast<CWeapon*>(pItem->GetIWeapon()) : NULL;
		if (pMainWeapon)
		{
			if (pMainWeapon->IsBusy() || pMainWeapon->IsModifying() || pMainWeapon->IsReloading())
				return OH_NO_GRAB;
		}
	}

	if (!pPhysicalEntity)
	{
		const ray_hit* pRay = pActor->GetGameObject()->GetWorldQuery()->GetLookAtPoint(m_range);
		if (pRay)
		{
			pPhysicalEntity = pRay->pCollider;
		}
		else
			return CheckItemsInProximity(info.eyePosition, info.eyeDirection, getEntityInfo);
	}

	IEntity* pEntity = m_pEntitySystem->GetEntityFromPhysics(pPhysicalEntity);

	m_crosshairId = 0;

	Vec3 pos = info.eyePosition;
	float lenSqr = 0.0f;
	bool  breakable = false;
	pe_params_part pPart;
		
	//Check if entity is in range
	if (pEntity)
	{
		const bool bDefaultClass = pEntity->GetClass() == gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
			
		lenSqr = (pos - pEntity->GetWorldPos()).len2();
		if (pPhysicalEntity->GetType() == PE_RIGID && !bDefaultClass)
		{
			//Procedurally breakable object (most likely...)
			//I need to adjust the distance, since the pivot of the entity could be anywhere
			pPart.ipart = 0;
			if (pPhysicalEntity->GetParams(&pPart) && pPart.pPhysGeom)
			{
				lenSqr -= pPart.pPhysGeom->origin.len2();
				breakable = true;
			}
		}

		const EntityId lookAtId = pEntity->GetId();
		if (lookAtId != m_lastLookAtEntityId)
		{
			OnLookAtEntityChanged(pEntity);

			m_lastLookAtEntityId = lookAtId;
		}
	}

	if (lenSqr < m_range * m_range)
	{
		if (pEntity)
		{
			const EntityId entityId = pEntity->GetId();
			// check if we have to pickup with two hands or just on hand
			SelectGrabType(pEntity);
			m_crosshairId = pEntity->GetId();

			if (getEntityInfo)
				m_preHeldEntityId = pEntity->GetId();

			//1.- Player can grab some NPCs
			//Let the actor decide if it can be grabbed
			if (CActor* pActorAI = static_cast<CActor*>(m_pActorSystem->GetActor(entityId)))
			{
				if (((playerStance != STANCE_STAND) && (playerStance != STANCE_ZEROG)) || pActor->IsSwimming())
					return OH_NO_GRAB;

				//Check Player position vs AI position
				if (pActorAI->GetActorSpecies() == eGCT_HUMAN)
				{
					float playerZ = pActor->GetEntity()->GetWorldPos().z;
					Vec3 aiPos = pActorAI->GetEntity()->GetWorldPos();
					if (aiPos.z - playerZ > 1.0f)
						return OH_NO_GRAB;

					Line aim = Line(info.eyePosition, info.eyeDirection);

					float dst = LinePointDistanceSqr(aim, aiPos, 0.75f);
					if (dst < 0.6f)
						return OH_NO_GRAB;

					CPlayer* pPlayerAI = CPlayer::FromActor(pActorAI);
					if (pPlayerAI && pPlayerAI->GetPlayerStats()->isStandingUp)
						return OH_NO_GRAB;
				}

				if (pEntity->GetAI() && pActor->GetEntity() && !pActorAI->GetLinkedVehicle())
				{
					//Check script table (maybe is not possible to grab)
					SmartScriptTable props;
					SmartScriptTable propsDamage;
					IScriptTable* pScriptTable = pEntity->GetScriptTable();
					if (pScriptTable && pScriptTable->GetValue("Properties", props))
					{
						if (props->GetValue("Damage", propsDamage))
						{
							int noGrab = 0;
							if (propsDamage->GetValue("bNoGrab", noGrab) && noGrab != 0)
								return OH_NO_GRAB;

							float customGrabDistance;
							if (propsDamage->GetValue("customGrabDistance", customGrabDistance))
							{
								if (lenSqr > customGrabDistance * customGrabDistance)
									return OH_NO_GRAB;
							}
						}
					}

					if (pActorAI->GetActorSpecies() != eGCT_UNKNOWN && pActorAI->GetHealth() > 0 && !pActorAI->IsFallen() && pEntity->GetAI()->IsHostile(pActor->GetEntity()->GetAI(), false))
						return OH_GRAB_NPC;
					else
						return OH_NO_GRAB;
				}
				return OH_NO_GRAB;
			}

			//2. -if it's an item, let the item decide if it can be picked up or not
			if (CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(pEntity->GetId())))
			{
				if (pItem->CanPickUp(pActor->GetEntityId()))
					return OH_GRAB_ITEM;
				else
					return OH_NO_GRAB;
			}

			//Items have priority over the rest of pickables
			if (CheckItemsInProximity(info.eyePosition, info.eyeDirection, getEntityInfo) == OH_GRAB_ITEM)
				return OH_GRAB_ITEM;

			if (pActor->IsSwimming() || playerStance == STANCE_PRONE)
				return OH_NO_GRAB;

			//2.5. -CryMP Custom pickups 
			if (g_pGameCVars->mp_pickupVehicles || !gEnv->bMultiplayer)
			{
				//CryMP: Crouch to pickup vehicles :D
				if (playerStance == STANCE_CROUCH && m_pVehicleSystem->GetVehicle(entityId))
				{
					//Don't allow to pickup while in vehicle, or in air
					if (!pActor->GetLinkedVehicle() && pActor->GetActorStats() && pActor->GetActorStats()->onGround > 0.0f)
					{
						return OH_GRAB_OBJECT;
					}
				}
			}

			const bool bPICK_UP_OBJECTS_MP = gEnv->bMultiplayer && g_pGameCVars->mp_pickupObjects;
			if (bPICK_UP_OBJECTS_MP)
			{
				//CryMP: Objects that are not bound to network, cannot be picked up in MP (except chickens etc)
				const bool bClientEntity = (pEntity->GetFlags() & ENTITY_FLAG_CLIENT_ONLY);
				const bool bIsBound = m_pGameFramework->GetNetContext()->IsBound(entityId);
				const IEntityClass* pClass = pEntity->GetClass();
				if (!bIsBound && !bClientEntity)
				{
					return OH_NO_GRAB;
				}
				if (pClass == CItem::sDoorClass ||
					pClass == CItem::sElevatorSwitchClass ||
					pClass == CItem::sFlagClass)
				{
					return OH_NO_GRAB;
				}
			}

			if (bPICK_UP_OBJECTS_MP || !gEnv->bMultiplayer)
			{
				//CryMP: Allow picking up projectiles 
				if (g_pGame->GetWeaponSystem()->GetProjectile(entityId))
				{
					const pe_type physicsType = pEntity->GetPhysics() ? pEntity->GetPhysics()->GetType() : PE_NONE;
					//Only allow PE_RIGID
					if (physicsType != PE_RIGID && physicsType != PE_PARTICLE)
					{
						return OH_NO_GRAB;
					}
					m_grabType = GRAB_TYPE_ONE_HANDED;
					return OH_GRAB_OBJECT;
				}
			}

			//3. -If we found a helper, it has to be pickable
			if (m_hasHelper && pPhysicalEntity->GetType() == PE_RIGID)
			{
				SmartScriptTable props;
				IScriptTable* pEntityScript = pEntity->GetScriptTable();
				if (pEntityScript && pEntityScript->GetValue("Properties", props))
				{
					//If it's not pickable, ignore helper
					int pickable = 0;
					if (props->GetValue("bPickable", pickable) && !pickable)
						return OH_NO_GRAB;
				}
				return OH_GRAB_OBJECT;
			}

			//4. Pick boid object
			if ((pPhysicalEntity->GetType() == PE_PARTICLE || (pPhysicalEntity->GetType() == PE_ARTICULATED && m_grabType == GRAB_TYPE_TWO_HANDED)) && m_hasHelper)
				return OH_GRAB_OBJECT;

			//5. -Procedurally breakable object (most likely...)
			if (breakable)
			{
				//Set "hold" matrix
				if (pPart.pPhysGeom->V < 0.35f && pPart.pPhysGeom->Ibody.len() < 0.1)
				{
					m_holdOffset.SetTranslation(pPart.pPhysGeom->origin + Vec3(0.0f, -0.15f, 0.0f));
					m_holdOffset.InvertFast();
					return OH_GRAB_OBJECT;
				}

			}

			//6.- Temp? solution for spawned rocks (while they don't have helpers)
			if (pPhysicalEntity->GetType() == PE_RIGID && !strcmp(pEntity->GetClass()->GetName(), "rock"))
			{
				m_grabType = GRAB_TYPE_ONE_HANDED;
				return OH_GRAB_OBJECT;
			}

			//7.- Legacy system...
			SmartScriptTable props;
			IScriptTable* pEntityScript = pEntity->GetScriptTable();
			if (pPhysicalEntity->GetType() == PE_RIGID && pEntityScript && pEntityScript->GetValue("Properties", props))
			{
				int pickable = 0;
				int usable = 0;
				if (props->GetValue("bPickable", pickable) && !pickable) 
					return false;
				else if (pickable)
					if (props->GetValue("bUsable", usable) && !usable)
						return OH_GRAB_OBJECT;

				return false;
			}

			if (getEntityInfo)
				m_preHeldEntityId = 0;

			return OH_NO_GRAB;//CheckItemsInProximity(info.eyePosition, info.eyeDirection, getEntityInfo);
		}
		else if (pPhysicalEntity->GetType() == PE_STATIC)
		{
			//Rocks and small static vegetation marked as pickable
			IRenderNode* pRenderNode = 0;
			pe_params_foreign_data pfd;
			if (pPhysicalEntity->GetParams(&pfd) && pfd.iForeignData == PHYS_FOREIGN_ID_STATIC)
				pRenderNode = static_cast<IRenderNode*>(pfd.pForeignData);

			if (pRenderNode && pRenderNode->GetRndFlags() & ERF_PICKABLE)
			{
				if (getEntityInfo)
				{
					m_grabType = GRAB_TYPE_ONE_HANDED;
					m_pRockRN = pRenderNode;
					m_preHeldEntityId = 0;
				}
				return OH_GRAB_OBJECT;
			}
		}
	}
	return CheckItemsInProximity(info.eyePosition, info.eyeDirection, getEntityInfo);
}

//========================================================================================
void COffHand::OnLookAtEntityChanged(IEntity* pEntity)
{
	if (pEntity && m_pVehicleSystem->GetVehicle(pEntity->GetId()))
	{
		//CryMP: Fix missing Enter vehicle HUD display
		//Usually a punch solves it, the following awake code fixes it
		//The bugged vehicles have submergedFraction 1.0
		IPhysicalEntity* pPhysics = pEntity->GetPhysics();
		if (pPhysics)
		{
			pe_action_awake actionAwake;
			actionAwake.bAwake = 1;
			pPhysics->Action(&actionAwake);
		}
	}
}

//========================================================================================
int COffHand::CheckItemsInProximity(Vec3 pos, Vec3 dir, bool getEntityInfo)
{
	float sizeX = 1.2f;
	if (gEnv->bMultiplayer)
		sizeX = 1.75;
	float sizeY = sizeX;
	float sizeZup = 0.5f;
	float sizeZdown = 1.75f;

	SEntityProximityQuery query;
	query.box = AABB(Vec3(pos.x - sizeX, pos.y - sizeY, pos.z - sizeZdown),
		Vec3(pos.x + sizeX, pos.y + sizeY, pos.z + sizeZup));
	query.nEntityFlags = ~0; // Filter by entity flag.

	float minDstSqr = 0.2f;
	EntityId nearItemId = 0;
	int count = m_pEntitySystem->QueryProximity(query);
	for (int i = 0; i < query.nCount; i++)
	{
		EntityId id = query.pEntities[i]->GetId();

		if (CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(id)))
		{
			if (pItem->GetOwnerId() != GetOwnerId())
			{
				Line line(pos, dir);
				AABB bbox;
				pItem->GetEntity()->GetWorldBounds(bbox);
				Vec3 itemPos = bbox.GetCenter();
				if (dir.Dot(itemPos - pos) > 0.0f)
				{
					float dstSqr = LinePointDistanceSqr(line, itemPos);
					if (dstSqr < 0.2f)
					{
						if ((dstSqr < minDstSqr) && pItem->CanPickUp(GetOwnerId()))
						{
							minDstSqr = dstSqr;
							nearItemId = id;
						}
					}
				}
			}
		}
	}

	if (IEntity* pEntity = m_pEntitySystem->GetEntity(nearItemId))
	{
		AABB bbox;
		pEntity->GetWorldBounds(bbox);
		Vec3 itemPos = bbox.GetCenter();
		Vec3 newDir = (pos - itemPos);

		IPhysicalEntity* phys = pEntity->GetPhysics();

		ray_hit hit;
		if (!gEnv->pPhysicalWorld->RayWorldIntersection(itemPos, newDir, ent_static | ent_rigid | ent_sleeping_rigid,
			rwi_stop_at_pierceable | rwi_ignore_back_faces, &hit, 1, phys ? &phys : NULL, phys ? 1 : 0))
		{
			//If nothing in between...
			m_crosshairId = nearItemId;

			SelectGrabType(pEntity);
			if (getEntityInfo)
				m_preHeldEntityId = m_crosshairId;

			return OH_GRAB_ITEM;
		}
	}

	return OH_NO_GRAB;
}

//==========================================================================================
bool COffHand::PerformPickUp()
{
	bool setHeld = false;

	//If we are here, we must have the entity ID
	if (m_preHeldEntityId)
	{
		//CryMP: Remote players don't use m_preHeldEntityId
		m_heldEntityId = 0; //forces SetHeldEntityId 
		SetHeldEntityId(m_preHeldEntityId);
		m_preHeldEntityId = 0;
		setHeld = true;
	}
	m_startPickUp = false;
	IEntity* pEntity = NULL;

	if (m_heldEntityId)
	{
		pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
	}
	else if (m_pRockRN)
	{
		SetHeldEntityId(SpawnRockProjectile(m_pRockRN));
		setHeld = true;

		m_pRockRN = NULL;
		if (!m_heldEntityId)
			return false;

		pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
		SelectGrabType(pEntity);
		m_grabType = GRAB_TYPE_ONE_HANDED; //Force for now
	}

	if (pEntity)
	{

		if (!setHeld)
		{
			m_heldEntityId = 0;
			SetHeldEntityId(pEntity->GetId());
		}

		CActor* pActor = GetOwnerActor();
		// Send event to entity.
		SEntityEvent entityEvent;
		entityEvent.event = ENTITY_EVENT_PICKUP;
		entityEvent.nParam[0] = 1;
		if (pActor)
			entityEvent.nParam[1] = pActor->GetEntityId();
		pEntity->SendEvent(entityEvent);

		m_holdScale = pEntity->GetScale();

		//if (m_pVehicleSystem->GetVehicle(pEntity->GetId()))
		//{
		//	m_holdScale *= 0.3f;
		//}

		if (pActor && pActor->IsPlayer())
		{
			m_heldEntityMass = 1.0f;
			if (IPhysicalEntity* pPhy = pEntity->GetPhysics())
			{
				pe_status_dynamics dynStat;
				if (pPhy->GetStatus(&dynStat))
					m_heldEntityMass = dynStat.mass;
				if (pPhy->GetType() == PE_PARTICLE)
					m_intialBoidLocalMatrix = pEntity->GetSlotLocalTM(0, false);
			}
			// only if we're picking up a normal (non Item) object
			if (m_currentState & eOHS_PICKING)
			{
				if (CPlayer* pPlayer = CPlayer::FromActor(pActor))
				{
					pPlayer->NotifyObjectGrabbed(true, m_heldEntityId, false, (m_grabType == GRAB_TYPE_TWO_HANDED));
				}
			}

			//CryMP
			if (pActor && pActor->IsRemote())
			{
				if (!IsSelected())
				{
					Select(true);
				}
			}
		}

		SetDefaultIdleAnimation(eIGS_FirstPerson, m_grabTypes[m_grabType].idle);

		m_constraintStatus = ConstraintStatus::WaitForPhysicsUpdate;

		return true;
	}

	return false;
}

//===========================================================================================
void COffHand::SetIgnoreCollisionsWithOwner(bool activate, EntityId entityId /*=0*/)
{
	if (!m_heldEntityId && !entityId)
		return;

	IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId ? m_heldEntityId : entityId);
	if (!pEntity)
		return;

	IPhysicalEntity* pPE = pEntity->GetPhysics();
	if (!pPE)
		return;

	if (pPE->GetType() == PE_PARTICLE)
	{
		m_constraintId = 0;
		return;
	}

	if (activate)
	{
		if (pEntity->IsHidden())
			return;

		//The constraint doesn't work with Items physicalized as static
		if (pPE->GetType() == PE_STATIC)
		{
			IItem* pItem = m_pItemSystem->GetItem(pEntity->GetId());
			if (pItem)
			{
				pItem->Physicalize(true, true);
				pPE = pEntity->GetPhysics();
				assert(pPE);
			}
		}

		CActor* pActor = GetOwnerActor();
		if (!pActor)
			return;

		pe_action_add_constraint ic;
		ic.flags = constraint_inactive | constraint_ignore_buddy;
		ic.pBuddy = pActor->GetEntity()->GetPhysics();
		ic.pt[0].Set(0, 0, 0);
		m_constraintId = pPE->Action(&ic);
	}
	else
	{
		pe_action_update_constraint up;
		up.bRemove = true;
		up.idConstraint = m_constraintId;
		m_constraintId = 0;
		pPE->Action(&up);
	}
}

//==========================================================================================
void COffHand::DrawNear(bool drawNear, EntityId entityId /*=0*/)
{
	IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId ? m_heldEntityId : entityId);
	if (!pEntity)
		return;

	int nslots = pEntity->GetSlotCount();
	for (int i = 0;i < nslots;i++)
	{
		if (pEntity->GetSlotFlags(i) & ENTITY_SLOT_RENDER)
		{
			if (drawNear)
			{
				pEntity->SetSlotFlags(i, pEntity->GetSlotFlags(i) | ENTITY_SLOT_RENDER_NEAREST);
				if (IEntityRenderProxy* pProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER))
				{
					if (IRenderNode* pRenderNode = pProxy->GetRenderNode())
						pRenderNode->SetRndFlags(ERF_REGISTER_BY_POSITION, true);
				}
				if (IEntityPhysicalProxy* pPhysicsProxy = static_cast<IEntityPhysicalProxy*>(pEntity->GetProxy(ENTITY_PROXY_PHYSICS)))
					pPhysicsProxy->DephysicalizeFoliage(i);

			}
			else
			{
				pEntity->SetSlotFlags(i, pEntity->GetSlotFlags(i) & (~ENTITY_SLOT_RENDER_NEAREST));
				if (IEntityRenderProxy* pProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER))
				{
					if (IRenderNode* pRenderNode = pProxy->GetRenderNode())
						pRenderNode->SetRndFlags(ERF_REGISTER_BY_POSITION, false);
				}
				if (IEntityPhysicalProxy* pPhysicsProxy = static_cast<IEntityPhysicalProxy*>(pEntity->GetProxy(ENTITY_PROXY_PHYSICS)))
					pPhysicsProxy->PhysicalizeFoliage(i);

			}
		}
	}
}

//=========================================================================================
void COffHand::SelectGrabType(IEntity* pEntity)
{
	if (!pEntity)
		return;

	CActor* pActor = GetOwnerActor();
	if (!pActor)
		return;

	if (g_pGame->GetWeaponSystem()->GetProjectile(pEntity->GetId()))
	{
		//If it's a projectile, we can only grab it with one hand
		m_grabType = GRAB_TYPE_ONE_HANDED;
		m_holdOffset.SetIdentity();
		m_hasHelper = false;
		return;
	}

	// iterate over the grab types and see if this object supports one
	m_grabType = GRAB_TYPE_ONE_HANDED;
	const TGrabTypes::const_iterator end = m_grabTypes.end();
	for (TGrabTypes::const_iterator i = m_grabTypes.begin(); i != end; ++i, ++m_grabType)
	{
		SEntitySlotInfo slotInfo;
		for (int n = 0; n < pEntity->GetSlotCount(); n++)
		{
			if (!pEntity->IsSlotValid(n))
				continue;

			bool ok = pEntity->GetSlotInfo(n, slotInfo) && (pEntity->GetSlotFlags(n) & ENTITY_SLOT_RENDER);
			if (ok && slotInfo.pStatObj)
			{
				//Iterate two times (normal helper name, and composed one)
				for (int j = 0; j < 2; j++)
				{
					string helper;
					helper.clear();
					if (j == 0)
					{
						helper.append(slotInfo.pStatObj->GetGeoName()); helper.append("_"); helper.append((*i).helper.c_str());
					}
					else
						helper.append((*i).helper.c_str());

					//It is already a subobject, we have to search in the parent
					if (slotInfo.pStatObj->GetParentObject())
					{
						IStatObj::SSubObject* pSubObj = slotInfo.pStatObj->GetParentObject()->FindSubObject(helper.c_str());
						if (pSubObj)
						{
							m_holdOffset = pSubObj->tm;
							m_holdOffset.OrthonormalizeFast();
							m_holdOffset.InvertFast();
							m_hasHelper = true;
							return;
						}
					}
					else {
						IStatObj::SSubObject* pSubObj = slotInfo.pStatObj->FindSubObject(helper.c_str());
						if (pSubObj)
						{
							m_holdOffset = pSubObj->tm;
							m_holdOffset.OrthonormalizeFast();
							m_holdOffset.InvertFast();
							m_hasHelper = true;
							return;
						}
					}
				}
			}
			else if (ok && slotInfo.pCharacter)
			{
				//Grabbing helpers for boids/animals
				IAttachmentManager* pAM = slotInfo.pCharacter->GetIAttachmentManager();
				if (pAM)
				{
					IAttachment* pAttachment = pAM->GetInterfaceByName((*i).helper.c_str());
					if (pAttachment)
					{
						m_holdOffset = Matrix34(pAttachment->GetAttAbsoluteDefault().q);
						m_holdOffset.SetTranslation(pAttachment->GetAttAbsoluteDefault().t);
						m_holdOffset.OrthonormalizeFast();
						m_holdOffset.InvertFast();
						if (m_grabType == GRAB_TYPE_TWO_HANDED)
							m_holdOffset.AddTranslation(Vec3(0.1f, 0.0f, -0.12f));
						m_hasHelper = true;
						return;
					}
				}
			}
		}
	}

	// when we come here, we haven't matched any of the predefined helpers ... so try to make a 
	// smart decision based on how large the object is
	//float volume(0),heavyness(0);
	//pActor->CanPickUpObject(pEntity, heavyness, volume);

	// grabtype 0 is onehanded and 1 is twohanded
	//m_grabType = (volume>0.08f) ? 1 : 0;
	m_holdOffset.SetIdentity();
	m_hasHelper = false;
	m_grabType = GRAB_TYPE_TWO_HANDED;
}

//=========================================================================================
Matrix34 COffHand::GetHoldOffset(IEntity* pEntity)
{
	Matrix34 holdOffset(IDENTITY);

	CActor* pActor = GetOwnerActor();
	if (!pActor || !pEntity)
		return holdOffset;

	for (const auto& grabType : m_grabTypes)
	{
		for (int n = 0; n < pEntity->GetSlotCount(); ++n)
		{
			if (!pEntity->IsSlotValid(n))
				continue;

			SEntitySlotInfo slotInfo;
			if (!pEntity->GetSlotInfo(n, slotInfo) || !(pEntity->GetSlotFlags(n) & ENTITY_SLOT_RENDER))
				continue;

			if (slotInfo.pStatObj)
			{
				for (int j = 0; j < 2; ++j)
				{
					std::string helper = (j == 0) ?
						std::string(slotInfo.pStatObj->GetGeoName()) + "_" + std::string(grabType.helper.c_str()) :
						std::string(grabType.helper.c_str());

					IStatObj* parentObj = slotInfo.pStatObj->GetParentObject();
					IStatObj::SSubObject* pSubObj = parentObj ?
						parentObj->FindSubObject(helper.c_str()) :
						slotInfo.pStatObj->FindSubObject(helper.c_str());

					if (pSubObj)
					{
						//CryLogAlways("GetHoldOffset: Found helper %s", helper.c_str());
						holdOffset = pSubObj->tm;
						holdOffset.OrthonormalizeFast();
						holdOffset.InvertFast();
						return holdOffset;
					}
				}
			}
			else if (slotInfo.pCharacter)
			{
				IAttachmentManager* pAM = slotInfo.pCharacter->GetIAttachmentManager();
				if (pAM)
				{
					IAttachment* pAttachment = pAM->GetInterfaceByName(grabType.helper.c_str());
					if (pAttachment)
					{
						holdOffset = Matrix34(pAttachment->GetAttAbsoluteDefault().q);
						holdOffset.SetTranslation(pAttachment->GetAttAbsoluteDefault().t);
						holdOffset.OrthonormalizeFast();
						holdOffset.InvertFast();
						return holdOffset;
					}
				}
			}
		}
	}

	holdOffset.SetIdentity();
	return holdOffset;
}

//========================================================================================================
void COffHand::StartPickUpItem()
{
	CPlayer* pPlayer = static_cast<CPlayer*>(GetOwnerActor());
	assert(pPlayer && "COffHand::StartPickUpItem -> No player found!!");

	bool drop_success = false;

	IEntity* pPreHeldEntity = m_pEntitySystem->GetEntity(m_preHeldEntityId);

	// determine position of entity which will be picked up
	if (!pPreHeldEntity)
		return;

	if (!pPlayer->CheckInventoryRestrictions(pPreHeldEntity->GetClass()->GetName()))
	{
		//Can not carry more heavy/medium weapons
		//Drop existing weapon and pick up the other

		IItem* pItem = GetExchangeItem(pPlayer);
		if (pItem)
		{
			if (pPlayer->DropItem(pItem->GetEntityId(), 8.0f, true))
				drop_success = true;
		}

		if (!drop_success)
		{
			g_pGame->GetGameRules()->OnTextMessage(eTextMessageError, "@mp_CannotCarryMore");
			CancelAction();
			return;
		}
	}
	else if (IItem* pItem = m_pItemSystem->GetItem(m_preHeldEntityId))
	{
		if (!pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
		{
			CryFixedStringT<128> itemName(pItem->GetEntity()->GetClass()->GetName());
			if (!_stricmp(itemName.c_str(), "CustomAmmoPickup"))
			{
				SmartScriptTable props;
				if (pItem->GetEntity()->GetScriptTable() && pItem->GetEntity()->GetScriptTable()->GetValue("Properties", props))
				{
					const char* name = NULL;
					if (props->GetValue("AmmoName", name))
						itemName.assign(name);
				}
			}
			if (g_pGame->GetHUD())
			{
				CryFixedStringT<128> temp("@");
				temp.append(itemName.c_str());
				g_pGame->GetHUD()->DisplayFlashMessage("@ammo_maxed_out", 2, ColorF(1.0f, 0, 0), true, temp.c_str());
			}
			CancelAction();
			return;
		}
	}

	//No animation in MP
	if (gEnv->bMultiplayer)
	{
		SetOffHandState(eOHS_PICKING_ITEM2);

		pPlayer->PickUpItem(m_preHeldEntityId, true);
		CancelAction();
		return;
	}

	//Unzoom weapon if neccesary
	if (m_mainHandWeapon && (m_mainHandWeapon->IsZoomed() || m_mainHandWeapon->IsZooming()))
	{
		m_mainHandWeapon->ExitZoom();
		m_mainHandWeapon->ExitViewmodes();
	}

	if (m_mainHandWeapon && m_mainHandWeapon->IsWeaponRaised())
	{
		m_mainHandWeapon->RaiseWeapon(false, true);
		m_mainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
	}

	//Everything seems ok, start the action...
	SetOffHandState(eOHS_PICKING_ITEM);

	m_pickingTimer = 0.3f;
	m_mainHandIsDualWield = false;
	pPlayer->NeedToCrouch(pPreHeldEntity->GetWorldPos());

	if (m_mainHand && (m_mainHand->TwoHandMode() == 1 || drop_success))
	{
		GetOwnerActor()->HolsterItem(true);
		m_mainHand = m_mainHandWeapon = NULL;
	}
	else
	{
		if (m_mainHand)
		{
			if (m_mainHand->IsDualWield() && m_mainHand->GetDualWieldSlave())
			{
				m_mainHand = static_cast<CItem*>(m_mainHand->GetDualWieldSlave());
				m_mainHand->Select(false);
				m_mainHandIsDualWield = true;
			}
			else
			{
				m_mainHand->PlayAction(g_pItemStrings->offhand_on);
				m_mainHand->SetActionSuffix("akimbo_");
			}
		}
	}

	PlayAction(g_pItemStrings->pickup_weapon_left, 0, false, eIPAF_Default | eIPAF_RepeatLastFrame);
	GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson) + 100, CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_PICK_ITEM, this)), false);
	RequireUpdate(eIUS_General);
	m_startPickUp = true;
}

//=========================================================================================================
void COffHand::EndPickUpItem()
{
	IFireMode* pReloadFM = 0;
	EntityId prevWeaponId = 0;
	if (m_mainHandWeapon && IsServer() && m_mainHand)
	{
		prevWeaponId = m_mainHand->GetEntityId();
		pReloadFM = m_mainHandWeapon->GetFireMode(m_mainHandWeapon->GetCurrentFireMode());
		if (pReloadFM)
		{
			int fmAmmo = pReloadFM->GetAmmoCount();
			int invAmmo = GetInventoryAmmoCount(pReloadFM->GetAmmoType());
			if (pReloadFM && (pReloadFM->GetAmmoCount() != 0 || GetInventoryAmmoCount(pReloadFM->GetAmmoType()) != 0))
			{
				pReloadFM = 0;
			}
		}
	}

	//Restore main weapon
	if (m_mainHand)
	{
		if (m_mainHandIsDualWield)
		{
			m_mainHand->Select(true);
		}
		else
		{
			m_mainHand->ResetDualWield();
			m_mainHand->PlayAction(g_pItemStrings->offhand_off, 0, false, eIPAF_Default | eIPAF_NoBlend);
		}
	}
	else
	{
		GetOwnerActor()->HolsterItem(false);
	}

	//Pick-up the item, and reset offhand
	SetOffHandState(eOHS_PICKING_ITEM2);

	GetOwnerActor()->PickUpItem(m_heldEntityId, true);
	SetIgnoreCollisionsWithOwner(false, m_heldEntityId);

	SetOffHandState(eOHS_INIT_STATE);

	// if we were in a reload position and weapon hasn't changed, try to reload it
	// this will fail automatically, if we didn't get any new ammo
	// it will only occur on the server due to checks at the start of this function
	if (pReloadFM)
	{
		IItem* pItem = GetOwnerActor()->GetCurrentItem();
		if (pItem && pItem->GetEntityId() == prevWeaponId)
		{
			if (IWeapon* pWeapon = pItem->GetIWeapon())
			{
				pWeapon->Reload(false);
			}
		}
	}
}

//=======================================================================================
void COffHand::StartPickUpObject(const EntityId entityId, bool isLivingEnt /* = false */)
{
	//Grab NPCs-----------------
	if (isLivingEnt)
	{
		if (!GrabNPC())
		{
			CancelAction();
		}
	}
	//-----------------------

	//Don't pick up in prone
	CPlayer* pPlayer = static_cast<CPlayer*>(GetOwnerActor());
	if (!pPlayer)
		return;

	if (!gEnv->bMultiplayer)
	{
		if (pPlayer->IsClient() && pPlayer->GetStance() == STANCE_PRONE)
		{
			CancelAction();
			return;
		}

		pPlayer->SetHeldObjectId(entityId); //For ik arms
	}

	//Unzoom weapon if neccesary
	if (m_mainHandWeapon && (m_mainHandWeapon->IsZoomed() || m_mainHandWeapon->IsZooming()))
	{
		m_mainHandWeapon->ExitZoom();
		m_mainHandWeapon->ExitViewmodes();
	}

	// if two handed or dual wield we use the fists as the mainhand weapon
	if (m_mainHand && (m_grabTypes[m_grabType].twoHanded || m_mainHand->TwoHandMode() >= 1))
	{
		if (m_grabTypes[m_grabType].twoHanded)
		{
			GetOwnerActor()->HolsterItem(true);
			m_mainHand = m_mainHandWeapon = NULL;
		}
		else
		{
			m_prevMainHandId = m_mainHand->GetEntityId();
			GetOwnerActor()->SelectItemByName("Fists", false);
			m_mainHand = GetActorItem(GetOwnerActor());
			m_mainHandWeapon = static_cast<CWeapon*>(m_mainHand ? m_mainHand->GetIWeapon() : NULL);
		}
	}

	if (m_mainHand)
	{

		if (m_mainHandWeapon && m_mainHandWeapon->IsWeaponRaised())
		{
			m_mainHandWeapon->RaiseWeapon(false, true);
			m_mainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
		}

		if (m_mainHand->IsDualWield() && m_mainHand->GetDualWieldSlave())
		{
			m_mainHand = static_cast<CItem*>(m_mainHand->GetDualWieldSlave());
			m_mainHand->Select(false);
			m_mainHandIsDualWield = true;
		}
		else
		{
			m_mainHand->PlayAction(g_pItemStrings->offhand_on);
			m_mainHand->SetActionSuffix("akimbo_");
		}


		if (m_mainHandWeapon && pPlayer->IsClient())
		{
			IFireMode* pFireMode = m_mainHandWeapon->GetFireMode(m_mainHandWeapon->GetCurrentFireMode());
			if (pFireMode)
			{
				pFireMode->SetRecoilMultiplier(1.5f);		//Increase recoil for the weapon
			}
		}
	}
	if (!isLivingEnt)
	{
		SetOffHandState(eOHS_PICKING);

		m_pickingTimer = 0.3f;

		//PerformPickUp();

		SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, m_grabTypes[m_grabType].idle);
		PlayAction(m_grabTypes[m_grabType].pickup);

		GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson),
			CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_PICK_OBJECT, this)), false);

		m_startPickUp = true;
	}
	else
	{
		SetOffHandState(eOHS_GRABBING_NPC);

		m_grabType = GRAB_TYPE_NPC;
		SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, m_grabTypes[m_grabType].idle);
		PlayAction(m_grabTypes[m_grabType].pickup);
		GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson),
			CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_GRAB_NPC, this)), false);
	}
	RequireUpdate(eIUS_General);
}

//=========================================================================================
void COffHand::StartThrowObject(const EntityId entityId, int activationMode, bool isLivingEnt /*= false*/)
{
	if (!entityId)
		return;

	CActor* pActor = GetOwnerActor();
	if (!pActor)
		return;

	if (!gEnv->bMultiplayer)
	{
		pActor->SetHeldObjectId(0); //Disable ik arms
	}

	if (!gEnv->bMultiplayer && activationMode == eAAM_OnPress) //CryMP: moved this to request pickup action for MultiPlayer
	{
		if (GetCurrentFireMode() < MAX_GRENADE_TYPES)
		{
			m_lastFireModeId = GetCurrentFireMode();
		}
		if (entityId)
		{
			RequestFireMode(GetFireModeIdx(m_grabTypes[m_grabType].throwFM.c_str()));
		}
	}

	PerformThrow(activationMode, entityId, m_lastFireModeId, isLivingEnt);

	if (m_mainHandWeapon)
	{
		IFireMode* pFireMode = m_mainHandWeapon->GetFireMode(m_mainHandWeapon->GetCurrentFireMode());
		if (pFireMode)
		{
			pFireMode->SetRecoilMultiplier(1.0f);		//Restore normal recoil for the weapon
		}
	}
}

//==========================================================================================
bool COffHand::GrabNPC()
{
	CActor* pActor = GetOwnerActor();

	if (!pActor)
		return false;

	//Do not grab in prone
	if (pActor->GetStance() == STANCE_PRONE)
		return false;

	//Get actor
	CActor* pHeldActor = static_cast<CActor*>(m_pActorSystem->GetActor(m_preHeldEntityId));

	if (!pHeldActor)
		return false;

	IEntity* pEntity = pHeldActor->GetEntity();
	if (!pEntity->GetCharacter(0))
		return false;

	//The NPC holster his weapon
	bool mounted = false;
	CItem* currentItem = static_cast<CItem*>(pHeldActor->GetCurrentItem());
	if (currentItem)
	{
		if (currentItem->IsMounted() && currentItem->IsUsed())
		{
			currentItem->StopUse(pHeldActor->GetEntityId());
			mounted = true;
		}
	}

	if (!mounted)
	{
		pHeldActor->HolsterItem(false); //AI sometimes has holstered a weapon and selected a different one...
		pHeldActor->HolsterItem(true);
	}

	if (IAnimationGraphState* pAGState = pHeldActor->GetAnimationGraphState())
	{
		char value[256];
		IAnimationGraphState::InputID actionInputID = pAGState->GetInputId("Action");
		pAGState->GetInput(actionInputID, value);
		if (strcmp(value, "grabStruggleFP") != 0)
		{
			// this is needed to make sure the transition is super fast
			if (ICharacterInstance* pCharacter = pEntity->GetCharacter(0))
				if (ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim())
					pSkeletonAnim->StopAnimationsAllLayers();
		}

		// just in case clear the Signal
		pAGState->SetInput("Signal", "none");
		pAGState->SetInput(actionInputID, "grabStruggleFP");
	}
	if (SActorStats* pStats = pActor->GetActorStats())
		pStats->grabbedTimer = 0.0f;

	// this needs to be done before sending signal "OnFallAndPlay" to make sure
	// in case AG state was FallAndPlay we leave it before AI is disabled
	if (IAnimationGraphState* pAGState = pHeldActor->GetAnimationGraphState())
	{
		pAGState->ForceTeleportToQueriedState();
		pAGState->Update();
	}

	if (IAISystem* pAISystem = gEnv->pAISystem)
	{
		IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
		if (pAIActor)
		{
			IAISignalExtraData* pEData = pAISystem->CreateSignalExtraData();
			pEData->point = Vec3(0, 0, 0);
			pAIActor->SetSignal(1, "OnFallAndPlay", 0, pEData);
		}
	}

	//Disable IK
	SActorStats* pStats = pHeldActor->GetActorStats();
	if (pStats)
	{
		pStats->isGrabbed = true;
	}

	SetHeldEntityId(pHeldActor->GetEntityId());

	m_preHeldEntityId = 0;
	m_grabbedNPCSpecies = pHeldActor->GetActorSpecies();
	m_grabType = GRAB_TYPE_NPC;
	m_killTimeOut = KILL_NPC_TIMEOUT;
	m_killNPC = m_effectRunning = m_npcWasDead = false;
	m_grabbedNPCInitialHealth = pHeldActor->GetHealth();

	if (CPlayer* pHeldPlayer = CPlayer::FromActor(pHeldActor))
	{
		pHeldPlayer->NotifyObjectGrabbed(true, m_heldEntityId, true);
	}

	//Hide attachments on the back
	if (CWeaponAttachmentManager* pWAM = pHeldActor->GetWeaponAttachmentManager())
	{
		pWAM->HideAllAttachments(true);
	}

	if (m_grabbedNPCSpecies == eGCT_HUMAN)
	{
		PlaySound(eOHSound_Choking_Human, true);
	}

	RequireUpdate(eIUS_General);
	GetGameObject()->EnablePostUpdates(this); //needed, if I pause the game before throwing the NPC

	return true;
}

//=============================================================================================
void COffHand::ThrowNPC(const EntityId entityId, bool kill /*= true*/)
{
	//Get actor
	CActor* pHeldActor = static_cast<CActor*>(m_pActorSystem->GetActor(entityId));
	if (!pHeldActor)
		return;

	IEntity* pEntity = pHeldActor->GetEntity();

	SActorStats* pStats = pHeldActor->GetActorStats();
	if (pStats)
	{
		pStats->isGrabbed = false;
	}

	//Un-Hide attachments on the back
	if (CWeaponAttachmentManager* pWAM = pHeldActor->GetWeaponAttachmentManager())
	{
		pWAM->HideAllAttachments(false);
	}

	CPlayer* pPlayer = static_cast<CPlayer*>(GetOwnerActor());

	if (kill)
	{
		UpdateGrabbedNPCWorldPos(pEntity, NULL);
		pHeldActor->HolsterItem(false);
		IItem* currentItem = pHeldActor->GetCurrentItem();

		{
			int prevHealth = pHeldActor->GetHealth();
			int health = prevHealth - 100;

			//In strenght mode, always kill
			if (pPlayer && pPlayer->GetNanoSuit() && pPlayer->GetNanoSuit()->GetMode() == NANOMODE_STRENGTH)
			{
				health = 0;
			}
			if (health <= 0 || (m_grabbedNPCSpecies != eGCT_HUMAN) || m_bCutscenePlaying)
			{
				pHeldActor->SetHealth(0);
				if (currentItem && m_grabbedNPCSpecies == eGCT_HUMAN)
					pHeldActor->DropItem(currentItem->GetEntityId(), 0.5f, false, true);

				//Don't kill if it was already dead
				if ((pStats && !pStats->isRagDoll) || prevHealth > 0 || m_grabbedNPCSpecies == eGCT_HUMAN)
				{
					pHeldActor->SetAnimationInput("Action", "idle");
					pHeldActor->CreateScriptEvent("kill", 0);
				}
			}
			else
			{
				if (pEntity->GetAI())
				{
					pHeldActor->SetHealth(health);
					pHeldActor->SetAnimationInput("Action", "idle");
					//pEntity->GetAI()->Event(AIEVENT_ENABLE,0);
					pHeldActor->Fall();
					PlaySound(eOHSound_Kill_Human, true);
				}
			}
		}
	}
	else
	{
		pHeldActor->SetAnimationInput("Action", "idle");
	}

	/*if(m_grabbedNPCSpecies==eGCT_TROOPER)
	{
		PlaySound(eOHSound_Choking_Trooper,false);
	}
	else*/
	if (m_grabbedNPCSpecies == eGCT_HUMAN)
	{
		PlaySound(eOHSound_Choking_Human, false);
	}

	m_killTimeOut = -1.0f;
	m_killNPC = m_effectRunning = m_npcWasDead = false;
	m_grabbedNPCInitialHealth = 0;

	if (CPlayer* pHeldPlayer = CPlayer::FromActor(pHeldActor))
	{
		pHeldPlayer->NotifyObjectGrabbed(false, pEntity->GetId(), true);
	}

	GetGameObject()->DisablePostUpdates(this); //Disable again
}


//==============================================================================
void COffHand::RunEffectOnGrabbedNPC(CActor* pNPC)
{
	//Under certain conditions, different things could happen to the grabbed NPC (die, auto-destruct...)
	if (m_grabbedNPCSpecies == eGCT_TROOPER)
	{
		if (m_killNPC && m_effectRunning)
		{
			pNPC->SetHealth(0);
			//PlaySound(eOHSound_Choking_Trooper,false);
			m_killNPC = false;
		}
		else if (m_killNPC || (pNPC->GetHealth() < m_grabbedNPCInitialHealth && !m_effectRunning))
		{
			SGameObjectEvent event(eCGE_InitiateAutoDestruction, eGOEF_ToScriptSystem);
			pNPC->GetGameObject()->SendEvent(event);
			m_effectRunning = true;
			m_killTimeOut = KILL_NPC_TIMEOUT - 0.75f;
			m_killNPC = false;
		}
	}
	else if (m_grabbedNPCSpecies == eGCT_HUMAN && (pNPC->GetHealth() < m_grabbedNPCInitialHealth))
	{
		//Release the guy at first hit/damage... 8/
		if (m_currentState & (eOHS_HOLDING_NPC | eOHS_THROWING_NPC))
		{
			pNPC->SetHealth(0);
		}
	}
}

//========================================================================================
void COffHand::PlaySound(EOffHandSounds sound, bool play)
{
	if (!gEnv->pSoundSystem)
		return;

	bool repeating = false;
	unsigned int idx = 0;
	const char* soundName = NULL;

	switch (sound)
	{
	case eOHSound_Choking_Trooper:
		soundName = "Sounds/alien:trooper:choke";
		repeating = true;
		break;

	case eOHSound_Choking_Human:
		idx = Random(MAX_CHOKE_SOUNDS);
		idx = CLAMP(idx, 0, MAX_CHOKE_SOUNDS - 1);
		if (idx >= 0 && idx < MAX_CHOKE_SOUNDS)
			soundName = gChokeSoundsTable[idx];
		//repeating = true;
		break;

	case eOHSound_Kill_Human:
		idx = Random(MAX_CHOKE_SOUNDS);
		idx = CLAMP(idx, 0, MAX_CHOKE_SOUNDS - 1);
		if (idx >= 0 && idx < MAX_CHOKE_SOUNDS)
			soundName = gDeathSoundsTable[idx];
		break;

	default:
		break;
	}

	if (!soundName)
		return;

	if (play)
	{
		ISound* pSound = NULL;
		if (repeating && m_sounds[sound])
		{
			pSound = gEnv->pSoundSystem->GetSound(m_sounds[sound]);
			if (pSound && pSound->IsPlaying())
			{
				return;
			}
		}

		if (!pSound)
		{
			pSound = gEnv->pSoundSystem->CreateSound(soundName, 0);
		}

		if (pSound)
		{
			pSound->SetSemantic(eSoundSemantic_Player_Foley);
			if (repeating)
			{
				m_sounds[sound] = pSound->GetId();
			}
			pSound->Play();
		}
	}
	else if (repeating && m_sounds[sound])
	{
		ISound* pSound = gEnv->pSoundSystem->GetSound(m_sounds[sound]);
		if (pSound)
		{
			pSound->Stop();
		}
		m_sounds[sound] = 0;
	}


}
//==========================================================================================
void COffHand::MeleeAttack()
{
	if (m_melee)
	{
		if (CPlayer* pOwner = GetOwnerPlayer())
		{
			if (pOwner->GetPlayerStats()->bLookingAtFriendlyAI)
				return;
		}

		CMelee* melee = static_cast<CMelee*>(m_melee);

		SetOffHandState(eOHS_MELEE);

		m_melee->Activate(true);
		if (melee)
		{
			melee->IgnoreEntity(m_heldEntityId);
			//Most 1-handed objects have a mass between 1.0f and 10.0f
			//Scale melee damage/impulse based on mass of the held object
			float massScale = m_heldEntityMass * 0.1f;
			if (massScale < 0.2f)
				massScale *= 0.1f; //Scale down even more for small object...
			melee->MeleeScale(min(massScale, 1.0f));
		}
		m_melee->StartFire();
		m_melee->StopFire();

		GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson) + 100, CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_FINISH_MELEE, this)), false);
	}
}

//=======================================================================================
float COffHand::GetObjectMassScale()
{
	if (m_currentState & eOHS_HOLDING_NPC)
		return 0.65f;
	if (m_currentState & eOHS_HOLDING_OBJECT)
	{
		if (m_grabType == GRAB_TYPE_TWO_HANDED)
			return 0.65f;
		else
		{
			float mass = CLAMP(m_heldEntityMass, 1.0f, 10.0f);
			return (1.0f - (0.025f * mass));
		}
	}

	return 1.0f;
}

//=========================================================================================
bool COffHand::IsHoldingEntity()
{
	bool ret = false;
	if (m_currentState & (eOHS_GRABBING_NPC | eOHS_HOLDING_NPC | eOHS_THROWING_NPC | eOHS_PICKING | eOHS_HOLDING_OBJECT | eOHS_THROWING_OBJECT | eOHS_MELEE))
		ret = true;

	return ret;
}

//==============================================================
void COffHand::GetAvailableGrenades(std::vector<string>& grenades)
{
	if (!GetFireMode(0)->OutOfAmmo())
	{
		grenades.push_back(GetFireMode(0)->GetName());
	}

	if (!GetFireMode(1)->OutOfAmmo())
	{
		grenades.push_back(GetFireMode(1)->GetName());
	}

	if (!GetFireMode(2)->OutOfAmmo())
	{
		grenades.push_back(GetFireMode(2)->GetName());
	}

	if (!GetFireMode(3)->OutOfAmmo())
	{
		grenades.push_back(GetFireMode(3)->GetName());
	}
}

//==============================================================
int COffHand::CanExchangeWeapons(IItem* pItem, IItem** pExchangeItem)
{
	CPlayer* pPlayer = static_cast<CPlayer*>(GetOwnerActor());

	if (!pPlayer)
		return ITEM_NO_EXCHANGE;

	if (!pPlayer->CheckInventoryRestrictions(pItem->GetEntity()->GetClass()->GetName()))
	{
		//Can not carry more heavy/medium weapons
		IItem* pNewItem = GetExchangeItem(pPlayer);
		if (pNewItem)
		{
			*pExchangeItem = pNewItem;
			//can replace medium or heavy weapon
			return ITEM_CAN_EXCHANGE;
		}
		else
			return ITEM_NO_EXCHANGE;
	}

	return ITEM_CAN_PICKUP;
}

//==========================================================================
IItem* COffHand::GetExchangeItem(CPlayer* pPlayer)
{
	if (!pPlayer)
		return NULL;

	IItem* pItem = pPlayer->GetCurrentItem();
	const char* itemCategory = NULL;
	if (pItem)
	{
		itemCategory = m_pItemSystem->GetItemCategory(pItem->GetEntity()->GetClass()->GetName());
		if (!strcmp(itemCategory, "medium") || !strcmp(itemCategory, "heavy"))
			return pItem;
		else
		{
			int i = pPlayer->GetInventory()->GetCount();
			for (int w = i - 1; w > -1; --w)
			{
				pItem = m_pItemSystem->GetItem(pPlayer->GetInventory()->GetItem(w));
				itemCategory = m_pItemSystem->GetItemCategory(pItem->GetEntity()->GetClass()->GetName());
				if (!strcmp(itemCategory, "medium") || !strcmp(itemCategory, "heavy"))
					return pItem;
			}
		}
	}
	return NULL;
}

//========================================================
EntityId COffHand::SpawnRockProjectile(IRenderNode* pRenderNode)
{
	Matrix34 statObjMtx;
	IStatObj* pStatObj = pRenderNode->GetEntityStatObj(0, 0, &statObjMtx);
	assert(pStatObj);
	if (!pStatObj)
		return 0;

	pRenderNode->SetRndFlags(ERF_HIDDEN, true);
	pRenderNode->Dephysicalize();

	float scale = statObjMtx.GetColumn(0).GetLength();

	IEntityClass* pClass = m_pEntitySystem->GetClassRegistry()->FindClass("rock");
	if (!pClass)
		return 0;
	CProjectile* pRock = g_pGame->GetWeaponSystem()->SpawnAmmo(pClass);
	assert(pRock);
	if (!pRock)
		return 0;
	IEntity* pEntity = pRock->GetEntity();
	assert(pEntity);
	if (!pEntity)
		return 0;

	pEntity->SetStatObj(pStatObj, 0, true);
	pEntity->SetSlotFlags(0, pEntity->GetSlotFlags(0) | ENTITY_SLOT_RENDER);

	IEntityRenderProxy* pRenderProxy = static_cast<IEntityRenderProxy*>(pEntity->GetProxy(ENTITY_PROXY_RENDER));
	if (pRenderProxy)
	{
		pRenderProxy->GetRenderNode()->SetLodRatio(255);
		pRenderProxy->GetRenderNode()->SetViewDistRatio(255);
		pRenderProxy->GetRenderNode()->SetRndFlags(pRenderProxy->GetRenderNode()->GetRndFlags() | ERF_PICKABLE);
	}

	pRock->SetParams(GetOwnerId(), GetEntityId(), GetEntityId(), GetCurrentFireMode(), 75, 0);
	pRock->SetSequence(GenerateShootSeqN());

	pRock->Launch(statObjMtx.GetTranslation(), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), 1.0f);


	SEntityPhysicalizeParams params;
	params.type = PE_RIGID;
	params.nSlot = 0;
	params.mass = 2.0f;

	pe_params_buoyancy buoyancy;
	buoyancy.waterDamping = 1.5;
	buoyancy.waterResistance = 10;
	buoyancy.waterDensity = 0;
	params.pBuoyancy = &buoyancy;

	pEntity->Physicalize(params);

	return pEntity->GetId();
}

//==============================================================
//Handle entering cinematic (called from HUD)

void COffHand::OnBeginCutScene()
{
	if (IsHoldingEntity())
	{
		if (m_currentState & (eOHS_THROWING_NPC | eOHS_THROWING_OBJECT))
		{
			OnAction(GetOwnerId(), ActionId("use"), eAAM_OnRelease, 0.0f);
		}
		else
		{
			OnAction(GetOwnerId(), ActionId("use"), eAAM_OnPress, 0.0f);
			OnAction(GetOwnerId(), ActionId("use"), eAAM_OnRelease, 0.0f);
		}
	}

	m_bCutscenePlaying = true;
}

void COffHand::OnEndCutScene()
{
	m_bCutscenePlaying = false;
}

//==============================================================
void COffHand::AttachGrenadeToHand(int grenade, bool fp /*=true*/, bool attach /*=true*/)
{
	//Attach grenade to hand
	if (fp)
	{
		if (grenade == 0)
			DoSwitchAccessory(ItemString("OffhandGrenade"));
		else if (grenade == 1)
			DoSwitchAccessory(ItemString("OffhandSmoke"));
		else if (grenade == 2)
			DoSwitchAccessory(ItemString("OffhandFlashbang"));
		else if (grenade == 3)
			DoSwitchAccessory(ItemString("OffhandNanoDisruptor"));
	}
	else
	{
		ICharacterInstance* pOwnerCharacter = GetOwnerActor() ? GetOwnerActor()->GetEntity()->GetCharacter(0) : NULL;
		if (!pOwnerCharacter)
			return;

		IAttachmentManager* pAttachmentManager = pOwnerCharacter->GetIAttachmentManager();
		IAttachment* pAttachment = pAttachmentManager ? pAttachmentManager->GetInterfaceByName(m_params.attachment[eIH_Left].c_str()) : NULL;

		if (pAttachment)
		{
			//If there's an attachment, clear it
			if (!attach)
			{
				pAttachment->ClearBinding();
			}
			else
			{
				//If not it means we need to attach
				int slot = eIGS_Aux0;
				if ((grenade == 1) || (grenade == 2))
					slot = eIGS_Aux1;

				if (IStatObj* pStatObj = GetEntity()->GetStatObj(slot))
				{
					CCGFAttachment* pCGFAttachment = new CCGFAttachment();
					pCGFAttachment->pObj = pStatObj;

					pAttachment->AddBinding(pCGFAttachment);
				}
			}

		}
	}
}

//==============================================================
EntityId	COffHand::GetHeldEntityId() const
{
	if (m_currentState & (eOHS_HOLDING_NPC | eOHS_HOLDING_OBJECT))
		return m_heldEntityId;

	return 0;
}

//==============================================================
bool COffHand::Request_PickUpObject_MP()
{
	const EntityId objectId = m_crosshairId;
	IEntity* pObject = m_pEntitySystem->GetEntity(objectId);
	if (!pObject)
		return false;

	//CryMP notify server
	CActor* pOwner = GetOwnerActor();
	if (pOwner && pOwner->IsClient())
	{
		//Don't pick up in prone
		if (pOwner->GetStance() == STANCE_PRONE)
		{
			return false;
		}

		//Check if someone else is carrying the object already
		const int count = m_pActorSystem->GetActorCount();
		if (count > 1)
		{
			IActorIteratorPtr pIter = m_pActorSystem->CreateActorIterator();
			while (IActor* pActor = pIter->Next())
			{
				if (pActor == pOwner)
					continue;

				CPlayer* pOwnerPlayer = CPlayer::FromIActor(pActor);

				if (pOwnerPlayer && pOwnerPlayer->GetHeldObjectId() == objectId)
				{
					COffHand* pOffHand = static_cast<COffHand*>(pOwnerPlayer->GetItemByClass(CItem::sOffHandClass));
					if (pOffHand)
					{
						//stolen object event
						pOffHand->ThrowObject_MP(pOwnerPlayer, objectId, true);
					}
					break;
				}
			}
		}

		const bool bClientEntity = (pObject->GetFlags() & ENTITY_FLAG_CLIENT_ONLY);
		if (!bClientEntity)
		{
			//Start throw animation for all clients
			/*if (m_fm)
			{
				m_fm->StartFire();
			}
			else
			{
				CryLogWarningAlways("COffHand: owner %s has no firemode", pOwner->GetEntity()->GetName());
			}*/

			RequestFireMode(GetFireModeIdx(m_grabTypes[m_grabType].throwFM.c_str())); //CryMP: added this here, early enough 

			//CryMP: Might need a small timer to guarantee firemode is accepted before pickupitem rmi
			pOwner->GetGameObject()->InvokeRMI(CPlayer::SvRequestPickUpItem(), CPlayer::ItemIdParam(objectId), eRMI_ToServer);
			return true;
		}
	}
	return false;
}

//==============================================================
bool COffHand::PickUpObject_MP(CPlayer* pPlayer, const EntityId synchedObjectId) //Called from CPlayer.cpp
{
	IEntity* pObject = m_pEntitySystem->GetEntity(synchedObjectId);
	if (!pObject)
		return false;

	//CryMP: Remote player stole our object
	CPlayer* pClientPlayer = static_cast<CPlayer*>(m_pGameFramework->GetClientActor());
	if (pClientPlayer && pClientPlayer != pPlayer && pClientPlayer->GetHeldObjectId() == synchedObjectId)
	{
		COffHand* pClientOffHand = static_cast<COffHand*>(pClientPlayer->GetWeaponByClass(CItem::sOffHandClass));
		if (pClientOffHand)
		{
			//Will take care of resets 
			pClientOffHand->ThrowObject_MP(pClientPlayer, synchedObjectId, true);
		}
	}

	SetHeldEntityId(synchedObjectId);

	StartPickUpObject(synchedObjectId, false); //fixme , false);

	if (pPlayer->IsRemote())
	{
		EnableUpdate(true, eIUS_General);
	}

	return true;
}

//==============================================================
bool COffHand::ThrowObject_MP(CPlayer* pPlayer, const EntityId synchedObjectId, bool stealingObject) //Called from CPlayer.cpp
{
	IEntity* pObject = m_pEntitySystem->GetEntity(synchedObjectId);
	if (!pObject)
		return false;

	SetHeldEntityId(0);

	if (stealingObject)
	{
		FinishAction(eOHA_RESET);
	}
	else
	{
		pPlayer->PlayAnimation("combat_plantUB_c4_01", 1.0f, false, true, 1);
	}

	if (pPlayer->IsRemote())
	{
		EnableUpdate(false, eIUS_General);
	}

	return true;
}

//==============================================================
void COffHand::AttachObjectToHand(bool attach, EntityId objectId, bool throwObject)
{
	CActor* pOwner = GetOwnerActor();
	if (!pOwner)
		return;

	IEntity* pObject = m_pEntitySystem->GetEntity(objectId);

	const bool fpMode = !pOwner->IsThirdPerson() && !attach && !throwObject;

	//CryLogAlways("AttachObjectToHand: m_stats.fp %d, throwObject %d, %s, objectId=%d", m_stats.fp, throwObject, attach ? "attached" : "de-tached", objectId);

	ICharacterInstance* pOwnerCharacter = pOwner->GetEntity()->GetCharacter(0);
	IAttachmentManager* pAttachmentManager = pOwnerCharacter ? pOwnerCharacter->GetIAttachmentManager() : nullptr;
	if (!pAttachmentManager)
	{
		return;
	}

	const char* attachmentName = "held_object_attachment";
	IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(attachmentName);

	if (attach && pObject)
	{
		if (!pAttachment)
		{
			pAttachment = pAttachmentManager->CreateAttachment(attachmentName, CA_BONE, "Bip01 Head");
		}

		if (pAttachment)
		{
			// Attach the entity to the left hand
			CEntityAttachment* pEntityAttachment = new CEntityAttachment();
			pEntityAttachment->SetEntityId(objectId);

			pAttachment->ClearBinding();
			pAttachment->AddBinding(pEntityAttachment);

			bool isTwoHand = IsTwoHandMode();

			QuatT offset;
			offset.t = Vec3(ZERO);
			const bool isVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->IsVehicleClass(pObject->GetClass()->GetName());
			const bool isActor = !isVehicle && g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(objectId) != nullptr;

			if (isActor)
			{
				isTwoHand = false;
			}

			if (isTwoHand && !isVehicle && !isActor)
			{
				Vec3 vOffset = Vec3(ZERO);
				AABB bbox;
				pObject->GetLocalBounds(bbox);

				const float lengthX = fabs(bbox.max.x - bbox.min.x);
				const float lengthY = fabs(bbox.max.y - bbox.min.y);
				const float lengthZ = fabs(bbox.max.z - bbox.min.z);

				vOffset.y = 0.5f + (std::max(lengthY, lengthX) * 0.2f);
				vOffset.x -= lengthZ * 0.25f;

				offset.t = vOffset;  // Offset position
			}
			else if (!isTwoHand)
			{
				offset.t = Vec3(-0.4f, 0.7f, 0.0f);
			}

			Matrix34 holdMatrix = GetHoldOffset(pObject);

			QuatT holdOffset;
			holdOffset.q = Quat(Matrix33(holdMatrix));
			holdOffset.t = offset.t;

			Quat worldRotation = holdOffset.q;
			Vec3 worldPosition = holdOffset.t;

			ISkeletonPose* pSkeletonPose = pOwnerCharacter->GetISkeletonPose();
			if (pSkeletonPose)
			{
				const int headJointId = pSkeletonPose->GetJointIDByName("eye_left_bone");
				if (headJointId > -1)
				{
					Quat headRotation = pSkeletonPose->GetAbsJointByID(headJointId).q;
					Vec3 eyeDirection = headRotation.GetColumn1().GetNormalized();
					Quat rollQuaternion = Quat::CreateRotationAA(DEG2RAD(95), eyeDirection);

					worldRotation = rollQuaternion * worldRotation;

					if (isActor)
					{
						worldRotation = Quat::CreateRotationX(DEG2RAD(180)) * worldRotation;
					}
				}
			}

			Matrix34 worldMatrix = Matrix34(worldRotation);
			worldMatrix.SetTranslation(worldPosition);

			pAttachment->SetAttRelativeDefault(QuatT(worldRotation, worldPosition));

			IAnimationGraphState* pGraphState = pOwner->GetAnimationGraphState();
			if (pGraphState)
			{
				const auto inputId = pGraphState->GetInputId("PseudoSpeed");
				pGraphState->SetInput(inputId, 0.0f);
				pGraphState->Update();
			}
		}
	}
	else
	{
		if (pAttachment)
		{
			pAttachment->ClearBinding();
			pAttachmentManager->RemoveAttachmentByName(attachmentName); //Need to remove always, or will be misaligned
		}
	}
}

//==============================================================
void COffHand::UpdateEntityRenderFlags(const EntityId entityId, EntityFpViewMode mode)
{
	IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);
	if (!pEntity)
		return;

	CActor* pOwner = GetOwnerActor();

	bool enable = pOwner && !pOwner->IsThirdPerson();

	if (mode == EntityFpViewMode::ForceActive)
		enable = true;
	else if (mode == EntityFpViewMode::ForceDisable)
		enable = false;

	if (mode == EntityFpViewMode::Default && enable == m_objectFpMode)
		return;

	m_objectFpMode = enable;

	ICharacterInstance* pCharacter = pEntity->GetCharacter(0);

	if (enable)
	{
		//FP Mode
		if (pCharacter)
		{
			pCharacter->SetFlags(pCharacter->GetFlags() | ENTITY_SLOT_RENDER_NEAREST);
			if (IEntityRenderProxy* pProxy = static_cast<IEntityRenderProxy*>(pEntity->GetProxy(ENTITY_PROXY_RENDER)))
			{
				if (IRenderNode* pRenderNode = pProxy->GetRenderNode())
					pRenderNode->SetRndFlags(ERF_RENDER_ALWAYS, true);
			}
		}
		else
		{
			DrawNear(true, entityId);
		}
	}
	else
	{
		if (pCharacter)
		{
			pCharacter->SetFlags(pCharacter->GetFlags() & (~ENTITY_SLOT_RENDER_NEAREST));
			if (IEntityRenderProxy* pProxy = static_cast<IEntityRenderProxy*>(pEntity->GetProxy(ENTITY_PROXY_RENDER)))
			{
				if (IRenderNode* pRenderNode = pProxy->GetRenderNode())
					pRenderNode->SetRndFlags(ERF_RENDER_ALWAYS, false);
			}
		}
		else
		{
			DrawNear(false, entityId);
		}
	}
}

//==============================================================
void COffHand::EnableFootGroundAlignment(bool enable)
{
	CActor* pActor = GetOwnerActor();
	if (pActor && (enable || m_grabType == GRAB_TYPE_TWO_HANDED))
	{
		if (ICharacterInstance* pCharacter = pActor->GetEntity()->GetCharacter(0))
		{
			if (ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose())
			{
				pSkeletonPose->EnableFootGroundAlignment(enable);
			}
		}
	}
}

//==============================================================
void COffHand::SetHeldEntityId(const EntityId entityId, const EntityId oldId /* = 0*/)
{
	if (m_heldEntityId == entityId)
		return;

	const EntityId oldHeldEntityId = oldId ? oldId : m_heldEntityId;

	m_heldEntityId = entityId;

	const bool isOldItem = m_pItemSystem->GetItem(oldHeldEntityId) != nullptr;
	const bool isNewItem = m_pItemSystem->GetItem(entityId) != nullptr;

	if (!isOldItem)
	{
		if (oldHeldEntityId)
		{
			UpdateEntityRenderFlags(oldHeldEntityId, EntityFpViewMode::ForceDisable);

			AttachObjectToHand(false, oldHeldEntityId, false);
		}

		EnableFootGroundAlignment(true);
	}

	CActor* pActor = GetOwnerActor();
	if (!pActor)
		return;

	if (!isNewItem || !entityId)
	{
		pActor->SetHeldObjectId(entityId);
	}

	if (oldHeldEntityId && !isOldItem)
	{
		IEntity* pOldEntity = m_pEntitySystem->GetEntity(oldHeldEntityId);
		if (pOldEntity)
		{
			SetIgnoreCollisionsWithOwner(false, oldHeldEntityId);

			if (gEnv->bMultiplayer && pActor->IsRemote())
			{
				pOldEntity->SetFlags(pOldEntity->GetFlags() & ~ENTITY_FLAG_CLIENT_ONLY);

				if (IPhysicalEntity* pObjectPhys = pOldEntity->GetPhysics())
				{
					pe_status_dynamics dyn;
					pObjectPhys->GetStatus(&dyn);
					const float originalMass = pActor->GetHeldObjectMass();
					if (originalMass && dyn.mass != originalMass)
					{
						pe_simulation_params simParams;
						simParams.mass = originalMass;
						if (pObjectPhys->SetParams(&simParams))
						{
							pe_status_dynamics dyn;
							pObjectPhys->GetStatus(&dyn);
						}
					}
				}

				if (IVehicle* pVehicle = m_pGameFramework->GetIVehicleSystem()->GetVehicle(pOldEntity->GetId()))
				{
					//CryMP: Triggers a rephysicalization to avoid bugs caused by 0 mass
					reinterpret_cast<IGameObjectProfileManager*>(pVehicle + 1)->SetAspectProfile(eEA_Physics, 1);
				}

				pActor->SetHeldObjectMass(0.0f);
			}
		}
	}

	if (!entityId || isNewItem)
		return;

	IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);
	if (!pEntity)
		return;

	SelectGrabType(pEntity);

	EnableFootGroundAlignment(false);

	if (!pActor->IsThirdPerson())
	{
		UpdateEntityRenderFlags(entityId, EntityFpViewMode::ForceActive);
	}

	if (gEnv->bMultiplayer && pActor->IsRemote()) 
	{
		pEntity->SetFlags(pEntity->GetFlags() | ENTITY_FLAG_CLIENT_ONLY);

		if (IPhysicalEntity* pObjectPhys = pEntity->GetPhysics())
		{
			pe_status_dynamics dyn;
			if (pObjectPhys->GetStatus(&dyn))
			{
				pe_simulation_params simParams;
				simParams.mass = 0.0f;

				if (pObjectPhys->SetParams(&simParams))
				{
					pActor->SetHeldObjectMass(dyn.mass);
				}
			}
		}
	}

	SetIgnoreCollisionsWithOwner(true, entityId);

	if (pActor->IsThirdPerson())
	{
		AttachObjectToHand(true, entityId, false);
	}
}