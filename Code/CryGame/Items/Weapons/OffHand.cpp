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

#include "CryMP/Client/Client.h"
#include "CryMP/Client/HandGripRegistry.h"

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
		RemoveHeldEntityId(m_heldEntityId); //restore collisions immediately
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

	m_nextThrowTimer = -1.0f;
	m_lastFireModeId = 0;
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
			SetOffHandState(eOHS_PICKING);
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

				const EntityId backupId = m_heldEntityId;

				//If holding an object or NPC
				if (m_currentState & (eOHS_HOLDING_OBJECT | eOHS_PICKING | eOHS_THROWING_OBJECT | eOHS_MELEE))
				{
					//Do grabbing again
					SetOffHandState(eOHS_INIT_STATE); //CryMP: This will set m_heldEntityId to 0

					m_preHeldEntityId = backupId;
					PreExecuteAction(eOHA_USE, eAAM_OnPress, true);

					StartPickUpObject(m_preHeldEntityId, false);
				}
				else if (m_currentState & (eOHS_HOLDING_NPC | eOHS_GRABBING_NPC | eOHS_THROWING_NPC))
				{
					//Do grabbing again
					SetOffHandState(eOHS_INIT_STATE); //CryMP: This will set m_heldEntityId to 0

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
						m_preHeldEntityId = backupId;
						PreExecuteAction(eOHA_USE, eAAM_OnPress, true);

						StartPickUpObject(m_preHeldEntityId, true);
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
		CWeapon *pCurrentWeapon = GetOwnerActor()->GetCurrentWeapon(false);
		SetMainHandWeapon(pCurrentWeapon);

		m_mainHandIsDualWield = pCurrentWeapon ? pCurrentWeapon->IsDualWield() : false;
		
		SetOffHandState(eOHS_TRANSITIONING);
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
	}
}

//=======================================
void COffHand::OnEnterThirdPerson()
{
	//CryMP: Check 1st/3rd person transition
	CWeapon::OnEnterThirdPerson();

	if (m_heldEntityId)
	{
		if (m_stats.fp) //Only if we were in FP	(This function is also called from Select(true) in TP
		{
			AttachObjectToHand(true, m_heldEntityId, false);
			UpdateEntityRenderFlags(m_heldEntityId, EntityFpViewMode::ForceDisable);
		}
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
	//This has to be called for remote players when they're in thirdperson,
	//if we are spectating the remote player in firstperson, UpdateFPView will be called from CPlayer::UpdateFpSpectatorView
	//For local client, UpdateFPView is always called, even in thirdperson (from ItemSystem::Update)

	if (slot == eIUS_General)
	{
		if (gEnv->bClient && !m_stats.fp)
		{
			CActor* pActor = GetOwnerActor();
			if (pActor && pActor->IsRemote())
			{
				SharedUpdate(ctx.fFrameTime);

				if (m_heldEntityId && m_currentState & (eOHS_HOLDING_OBJECT | eOHS_PICKING | eOHS_THROWING_OBJECT | eOHS_PICKING_ITEM | eOHS_MELEE))
				{
					UpdateHeldObject();
				}
			}
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
void COffHand::SharedUpdate(float frameTime)
{
	CheckTimers(frameTime);

	//CryMP: Update held items in TP mode as well
	//Note: this check needs to be here, otherwise no grab anims in FP 
	if (!m_stats.fp && m_heldEntityId)
	{
		UpdateFPPosition(frameTime);
		UpdateFPCharacter(frameTime);
	}
}

//=============================================================
//CryMP: Called always on the client, even in ThirdPerson
//Called on other clients, if spectating them in FirstPerson

void COffHand::UpdateFPView(float frameTime)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	SharedUpdate(frameTime);

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
		{
			UpdateWeaponLowering(frameTime);
		}
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
	if (!gEnv->bClient)
		return;

	const bool enableThrowPitchRotation = true;

	CActor* pPlayer = GetOwnerActor();
	if (!pPlayer)
		return;

	const EntityId entityId = m_heldEntityId;

	IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);
	if (!pEntity)
	{
		FinishAction(eOHA_RESET);
		return;
	}

	AwakeEntityPhysics(pEntity);

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
					m_constraintStatus = ConstraintStatus::Broken;

					if (pPlayer->IsClient())
					{
						if (CHUD* pHUD = g_pGame->GetHUD())
						{
							pHUD->DisplayBigOverlayFlashMessage("@object_lost_destroyed", 2.0f, 400, 400, Col_Goldenrod);
						}
					}
				}
			}
		}
	}

	if (m_constraintStatus == ConstraintStatus::Broken)
	{
		if (m_currentState & (eOHS_HOLDING_OBJECT | eOHS_THROWING_OBJECT | eOHS_HOLDING_NPC | eOHS_THROWING_NPC)) //CryMP: don't release until we're actually holding it
		{
			if (pPlayer->IsClient())
			{
				if (GetMainHandWeapon() && GetMainHandWeapon()->IsBusy())
				{
					GetMainHandWeapon()->SetBusy(false);
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
			}
			return;
		}

		if (pPlayer->IsRemote())
		{
			FinishAction(eOHA_RESET);
			return;
		}
	}

	if (!m_stats.fp)
	{
		return;
	}

	const int id = eIGS_FirstPerson;

	const Matrix33& helperRot33 = GetSlotHelperRotation(id, "item_attachment", true);

	Matrix33 viewRot33 = helperRot33;
	Matrix33 base33 = helperRot33;

	if (enableThrowPitchRotation)
	{
		const bool isThrowState = (m_currentState & (eOHS_THROWING_OBJECT | eOHS_THROWING_NPC)) != 0;

		const float dt = gEnv->pTimer->GetFrameTime();
		const float targetBlend = isThrowState ? 1.0f : 0.0f;
		const float blendSpeed = 10.0f;

		m_throwPitchBlend_fp += (targetBlend - m_throwPitchBlend_fp) * min(1.0f, dt * blendSpeed);
		m_throwPitchBlend_fp = std::clamp(m_throwPitchBlend_fp, 0.0f, 1.0f);

		if (m_throwPitchBlend_fp > 0.001f)
		{
			IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(entityId);
			const float maxPitchRad = pVehicle ? DEG2RAD(10.0f) : DEG2RAD(20.0f);
			const float extraPitchRad = maxPitchRad * m_throwPitchBlend_fp;

			Matrix33 extraPitch = Matrix33::CreateRotationX(extraPitchRad);
			base33 = base33 * extraPitch;
		}
	}
	else
	{
		m_throwPitchBlend_fp = 0.0f;
	}

	Matrix34 baseRot(base33);

	Matrix34 finalMatrix = baseRot;
	finalMatrix.Scale(m_holdScale);

	Vec3 pos = GetSlotHelperPos(id, "item_attachment", true);

	Vec3 fpPosOffset(ZERO), tpPosOffset(ZERO);
	GetPredefinedPosOffset(pEntity, fpPosOffset, tpPosOffset);

	if (!fpPosOffset.IsZero(0.0001f))
	{
		const Vec3 viewRight = viewRot33.GetColumn0().GetNormalizedSafe(Vec3(1, 0, 0));
		const Vec3 viewFwd = viewRot33.GetColumn1().GetNormalizedSafe(Vec3(0, 1, 0));
		const Vec3 viewUp = viewRot33.GetColumn2().GetNormalizedSafe(Vec3(0, 0, 1));

		pos += viewRight * fpPosOffset.x
			+ viewFwd * fpPosOffset.y
			+ viewUp * fpPosOffset.z;
	}

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
		if (pClientChannel && m_pGameFramework->GetNetContext()->RemoteContextHasAuthority(pClientChannel, entityId))
		{
			hasAuthority = true;
		}*/
		if (pPlayer->GetHeldObjectId() == entityId)
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
			if (GetMainHandWeapon() && GetMainHandWeapon()->IsBusy())
			{
				GetMainHandWeapon()->SetBusy(false);
			}
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
			if (activationMode == eAAM_OnPress)
			{
				PerformThrowAction_Press(0, false);
			}
			else if (activationMode == eAAM_OnRelease)
			{
				PerformThrowAction_Release(0, false);
			}
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

		SetMainHandWeapon(pMainWeapon);
		m_mainHandIsDualWield = false;

		if (requestedAction == eOHA_THROW_GRENADE)
		{
			if (GetMainHandWeapon() && GetMainHandWeapon()->TwoHandMode() == 1)
			{
				GetOwnerActor()->HolsterItem(true);

				SetMainHandWeapon(nullptr);
			}
			else if (GetMainHandWeapon() && GetMainHandWeapon()->IsDualWield() && GetMainHandWeapon()->GetDualWieldSlave())
			{
				SetMainHandWeapon(static_cast<CWeapon*>(GetMainHandWeapon()->GetDualWieldSlave()->GetIWeapon()));

				m_mainHandIsDualWield = true;
				GetMainHandWeapon()->Select(false);
			}
		}
	}
	else if (requestedAction == eOHA_REINIT_WEAPON)
	{
		SetMainHandWeapon(pMainWeapon);
	}

	return exec;
}

//==================================================================
void COffHand::StartFire()
{
	CWeapon::StartFire();

	if (m_heldEntityId)
	{
		CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor());
		if (pPlayer)
		{
			pPlayer->StartThrowPrep();
		}
	}
}

//==================================================================
void COffHand::StopFire()
{
	CWeapon::StopFire();
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

	if (!gEnv->bClient)
		return;

	if (m_heldEntityId)
	{
		CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor());
		if (pPlayer)
		{
			pPlayer->StartThrowPrep();

			SetOffHandState(eOHS_THROWING_OBJECT);
		}
	}
	else
	{
		AttachGrenadeToHand(GetCurrentFireMode(), m_stats.fp, true);
	}

	//Handle FP Spectator
	if (m_stats.fp && GetOwnerActor())
	{
		CWeapon *pMainWeapon = GetOwnerActor()->GetCurrentWeapon(false);
		SetMainHandWeapon(pMainWeapon);
		
		if (pMainWeapon)
		{
			//if (!(m_currentState & (eOHS_THROWING_NPC | eOHS_THROWING_OBJECT)))
			{
				if (pMainWeapon->IsWeaponRaised())
				{
					pMainWeapon->RaiseWeapon(false, true);
					pMainWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
				}
				pMainWeapon->PlayAction(g_pItemStrings->offhand_on);
				pMainWeapon->SetActionSuffix("akimbo_");
			}
		}
		if (pMainWeapon && pMainWeapon->GetEntity()->GetClass() == CItem::sFistsClass)
		{
			CFists* pFists = static_cast<CFists*>(pMainWeapon);
			pFists->RequestAnimState(CFists::eFAS_FIGHT);
		}
	}
}

//=============================================================================
void COffHand::NetStopFire()
{
	CWeapon::NetStopFire();

	if (!gEnv->bClient)
	{
		CActor* pOwner = GetOwnerActor();
		if (pOwner && pOwner->GetHeldObjectId())
		{
			pOwner->OnObjectEvent(CActor::ObjectEvent::THROW, pOwner->GetHeldObjectId());
		}
		return;
	}

	AttachGrenadeToHand(GetCurrentFireMode(), m_stats.fp, false);

	if (m_heldEntityId)
	{
		CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor());
		if (pPlayer)
		{
			pPlayer->OnObjectEvent(CActor::ObjectEvent::THROW, m_heldEntityId);
		}

		if (GetMainHandWeapon() && !GetMainHandWeapon()->IsDualWield())
		{
			GetMainHandWeapon()->PlayAction(g_pItemStrings->offhand_off, 0, false, CItem::eIPAF_Default | CItem::eIPAF_NoBlend);
		}
	}
	else
	{
		GetScheduler()->TimerAction(
			GetCurrentAnimationTime(CItem::eIGS_FirstPerson),
			MakeAction([this, hand = GetMainHandWeapon()](CItem*) {
				this->FinishGrenadeAction(hand);
				}),
			/*persistent=*/false
		);
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
		GetScheduler()->TimerAction(
			300,
			MakeAction([this](CItem*) {
				this->FinishAction(eOHA_RESET);
				}),
			/*persistent=*/true
		);

		ThrowNPC(m_heldEntityId);

		RemoveHeldEntityId(m_heldEntityId, ConstraintReset::Delayed);

		SetOffHandState(eOHS_TRANSITIONING);
		break;

	case eOHA_PICK_OBJECT:
		SetOffHandState(eOHS_HOLDING_OBJECT);
		break;

	case eOHA_THROW_OBJECT:
	{
		CActor* pActor = GetOwnerActor();
		const EntityId playerId = pActor ? pActor->GetEntityId() : 0;
		const bool isTwoHand = IsTwoHandMode();

		GetScheduler()->TimerAction(
			150,
			MakeAction([this, playerId, isTwoHand](CItem*) {
				CPlayer* pPlayer = CPlayer::FromActorId(playerId);
				if (pPlayer)
				{
					if (pPlayer->IsThirdPerson())
					{
						if (isTwoHand)
						{
							pPlayer->SetExtension("c4");
							pPlayer->SetInput("plant", false);
						}
						else
						{
							pPlayer->SetExtension("nw");
							pPlayer->SetInput("holding_grenade");
							pPlayer->SetInput("throw_grenade");
						}
					}
				}
				}),
			/*persistent=*/true
		);

		GetScheduler()->TimerAction( //CryMP: Timer needed for FP animations
			500,
			MakeAction([this](CItem*) {
				this->FinishAction(eOHA_RESET);
				}),
			/*persistent=*/true
		);

		// after it's thrown, wait 500ms to enable collisions again
		RemoveHeldEntityId(m_heldEntityId, ConstraintReset::Delayed);

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
		// enable leg IK again
		EnableFootGroundAlignment(true);

		CActor* pActor = GetOwnerActor();
		if (pActor)
		{
			const EntityId entityId = m_heldEntityId;

			const EntityId playerId = pActor->GetEntityId();
			if (m_prevMainHandId && !pActor->IsSwimming())
			{
				pActor->SelectItem(m_prevMainHandId, false);
				CWeapon* pCurrentWeapon = pActor->GetCurrentWeapon(false);
				SetMainHandWeapon(pCurrentWeapon);
			}

			if (pActor->IsClient())
			{
				RequestFireMode(m_lastFireModeId); //Works for MP as well

				if (gEnv->bMultiplayer && entityId)
				{
					RequestStopFire();
				}
			}

			float timeDelay = 0.1f;

			CWeapon *pMainWeapon = GetMainHandWeapon();

			if (!pMainWeapon)
			{
				SActorStats* pStats = pActor->GetActorStats();
				if (!pActor->ShouldSwim() && !m_bCutscenePlaying && (pStats && !pStats->inFreefall.Value()))
				{
					pActor->HolsterItem(false);
				}
			}
			else if (!m_mainHandIsDualWield && !m_prevMainHandId)
			{
				pMainWeapon->ResetDualWield();
				pMainWeapon->PlayAction(g_pItemStrings->offhand_off, 0, false, CItem::eIPAF_Default | CItem::eIPAF_NoBlend);
				timeDelay = (pMainWeapon->GetCurrentAnimationTime(CItem::eIGS_FirstPerson) + 50) * 0.001f;
			}
			else if (m_mainHandIsDualWield)
			{
				pMainWeapon->Select(true);
			}

			//CryMP: Fixes instant change to relaxed state after throwing NPC 
			if (pMainWeapon && pMainWeapon->GetEntity()->GetClass() == CItem::sFistsClass)
			{
				CFists* pFists = static_cast<CFists*>(pMainWeapon);
				pFists->RequestAnimState(CFists::eFAS_FIGHT);
			}

			SetResetTimer(timeDelay);

			RequireUpdate(eIUS_General);
			m_prevMainHandId = 0;

			RemoveHeldEntityId(m_heldEntityId, static_cast<ConstraintReset>(SkipIfDelayTimerActive | Immediate));

			if (CPlayer* pPlayer = CPlayer::FromActor(pActor))
			{
				pPlayer->SetArmIKLocalInvalid();

				//CryMP: Checks if ReachState::ThrowPrep active, resets bend if so 
				pPlayer->CommitThrow();

				if (pPlayer->GetPlayerStats() && pPlayer->GetPlayerStats()->grabbedHeavyEntity)
				{
					pPlayer->NotifyObjectGrabbed(false, entityId, false);
				}
			}
		}
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
	}
}

//==============================================================================
void COffHand::LogOffHandState(EOffHandStates eOHS)
{
	// -- Current State Bitmask --
	string stateBits;
	static constexpr struct { int flag; const char* name; } FLAGS[] = {
		{ eOHS_INIT_STATE,        "Init" },
		{ eOHS_SWITCHING_GRENADE, "SwitchingGrenade" },
		{ eOHS_HOLDING_GRENADE,   "HoldingGrenade" },
		{ eOHS_THROWING_GRENADE,  "ThrowingGrenade" },
		{ eOHS_PICKING,           "Picking" },
		{ eOHS_PICKING_ITEM,      "PickingItem" },
		{ eOHS_PICKING_ITEM2,     "PickingItem2" },
		{ eOHS_HOLDING_OBJECT,    "HoldingObject" },
		{ eOHS_THROWING_OBJECT,   "ThrowingObject" },
		{ eOHS_GRABBING_NPC,      "GrabbingNPC" },
		{ eOHS_HOLDING_NPC,       "HoldingNPC" },
		{ eOHS_THROWING_NPC,      "ThrowingNPC" },
		{ eOHS_TRANSITIONING,     "Transitioning" },
		{ eOHS_MELEE,             "Melee" }
	};
	for (const auto& f : FLAGS)
	{
		if (eOHS & f.flag)
		{
			if (!stateBits.empty()) stateBits += "|";
			stateBits += f.name;
		}
	}

	CryLogAlways("%s OffHandState: $6%s", stateBits.c_str(), (GetOwnerActor() ? GetOwnerActor()->IsClient() : false) ? "$3[Client]$1" : "$8[Remote]$1");
}

//==============================================================================
void COffHand::SetOffHandState(EOffHandStates eOHS)
{
	m_currentState = eOHS;

	if (eOHS & eOHS_INIT_STATE)
	{
		SetMainHandWeapon(nullptr);

		m_preHeldEntityId = 0;
		
		RemoveHeldEntityId(m_heldEntityId, static_cast<ConstraintReset>(SkipIfDelayTimerActive | Immediate));

		m_mainHandIsDualWield = false;
		Select(false);
	}
}

//==============================================================================
void COffHand::SetMainHandWeapon(CWeapon* pWeapon)
{
	m_mainHandWeapon = pWeapon;
}

//==============================================================================
CWeapon* COffHand::GetMainHandWeapon() const
{
	return m_mainHandWeapon;
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

	CWeapon* pMainWeapon = GetMainHandWeapon();

	//Unzoom weapon if neccesary
	if (pMainWeapon && (pMainWeapon->IsZoomed() || pMainWeapon->IsZooming()))
	{
		pMainWeapon->ExitZoom();
		pMainWeapon->ExitViewmodes();
	}

	m_mainHandIsDualWield = false;

	//A new grenade type/fire mode was chosen
	if (pMainWeapon && (pMainWeapon->TwoHandMode() != 1) && !pMainWeapon->IsDualWield())
	{

		if (pMainWeapon->GetEntity()->GetClass() == CItem::sFistsClass)
		{
			CFists* pFists = static_cast<CFists*>(pMainWeapon);
			pFists->RequestAnimState(CFists::eFAS_FIGHT);
		}

		if (pMainWeapon->IsWeaponRaised())
		{
			pMainWeapon->RaiseWeapon(false, true);
			pMainWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
		}

		//Play deselect left hand on main item
		pMainWeapon->PlayAction(g_pItemStrings->offhand_on);
		pMainWeapon->SetActionSuffix("akimbo_");

		GetScheduler()->TimerAction( 
			pMainWeapon->GetCurrentAnimationTime(eIGS_FirstPerson),
			MakeAction([this](CItem*) {
				this->FinishAction(eOHA_SWITCH_GRENADE);
				}),
			/*persistent=*/false
		);

	}
	else
	{
		//No main item or holstered (wait 100ms)
		if (pMainWeapon && pMainWeapon->IsDualWield() && pMainWeapon->GetDualWieldSlave())
		{
			SetMainHandWeapon(static_cast<CWeapon*>(pMainWeapon->GetDualWieldSlave()->GetIWeapon()));

			pMainWeapon->Select(false);
			m_mainHandIsDualWield = true;
		}
		else
		{
			GetOwnerActor()->HolsterItem(true);

			SetMainHandWeapon(nullptr);
		}
		GetScheduler()->TimerAction(
			100,
			MakeAction([this](CItem*) {
				this->FinishAction(eOHA_SWITCH_GRENADE);
				}),
			/*persistent=*/false
		);
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

	GetScheduler()->TimerAction(
		GetCurrentAnimationTime(CItem::eIGS_FirstPerson),
		MakeAction([this, hand = GetMainHandWeapon()](CItem*) {
			this->FinishGrenadeAction(hand);
			}),
		/*persistent=*/false
	);
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
	GetScheduler()->TimerAction(
		2000,
		MakeAction([this](CItem*) {
			this->FinishAction(eOHA_FINISH_AI_THROW_GRENADE);
			}),
		/*persistent=*/false
	);
}

//===============================================================================
int COffHand::CanPerformPickUp(CActor* pActor, IPhysicalEntity* pPhysicalEntity /*=nullptr*/, bool getEntityInfo /*= false*/)
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
	const bool isFpSpectatorTarget = pActor->IsFpSpectatorTarget();

	if (!getEntityInfo)
	{
		//Prevent pick up message while can not pick up
		IItem* pItem = pActor->GetCurrentItem(false);
		CWeapon* pMainWeapon = pItem ? static_cast<CWeapon*>(pItem->GetIWeapon()) : nullptr;
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

			IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(entityId);

			if (getEntityInfo && !isFpSpectatorTarget)
			{
				m_preHeldEntityId = pEntity->GetId();
			}

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
				if (playerStance == STANCE_CROUCH && pVehicle)
				{
					// CryMP: Mass limit for pickups
					if (g_pGameCVars->mp_pickupMassLimit > 0.0f)
					{
						pe_status_dynamics dyn;
						if (pPhysicalEntity->GetStatus(&dyn))
						{
							if (dyn.mass > g_pGameCVars->mp_pickupMassLimit)
							{
								if (g_pGame->GetHUD() && gEnv->pTimer->GetCurrTime() - m_lastTooHeavyMessage > 2.0f)
								{
									g_pGame->GetHUD()->DisplayBigOverlayFlashMessage("@object_too_heavy", 2.0f, 400, 400, Col_Goldenrod);

									m_lastTooHeavyMessage = gEnv->pTimer->GetCurrTime();
								}

								return OH_NO_GRAB;
							}
						}
					}
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
					pClass == CItem::sFlagClass ||
					pClass == CItem::sGeomEntityClass)
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

			if (getEntityInfo && !isFpSpectatorTarget)
			{
				m_preHeldEntityId = 0;
			}

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
					if (!isFpSpectatorTarget)
					{
						m_preHeldEntityId = 0;
					}
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
		AwakeEntityPhysics(pEntity);
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
bool COffHand::PerformPickUp(EntityId entityId)
{
	if (!entityId && !m_pRockRN)
		return false;

	m_startPickUp = false;
	IEntity* pEntity = nullptr;

	if (m_pRockRN)
	{
		EntityId rockId = SpawnRockProjectile(m_pRockRN);
		if (!rockId)
			return false;

		SetHeldEntityId(rockId);

		m_pRockRN = nullptr;
		if (!m_heldEntityId)
			return false;

		pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
		SelectGrabType(pEntity);
		m_grabType = GRAB_TYPE_ONE_HANDED; //Force for now
	}
	else
	{
		SetHeldEntityId(entityId);
		pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
	}

	if (pEntity)
	{
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
		}

		SetDefaultIdleAnimation(eIGS_FirstPerson, m_grabTypes[m_grabType].idle);

		if (!g_pGameCVars->mp_netSerializeHolsteredItems && m_grabType == GRAB_TYPE_TWO_HANDED)
		{
			if (pActor && pActor->IsRemote())
			{
				pActor->HolsterItem(true);
			}
		}

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
		m_constraintStatus = ConstraintStatus::Inactive;
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
		m_constraintStatus = ConstraintStatus::WaitForPhysicsUpdate;
	}
	else
	{
		pe_action_update_constraint up;
		up.bRemove = true;
		up.idConstraint = m_constraintId;
		m_constraintId = 0;
		m_constraintStatus = ConstraintStatus::Inactive;
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
					std::string helper;
					if (j == 0)
					{
						helper += slotInfo.pStatObj->GetGeoName();
						helper += "_";
					}
					helper += grabType.helper.c_str();

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

//=========================================================================================
bool COffHand::IsGrabTypeTwoHanded(const EntityId entityId) const noexcept
{
	IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);
	if (!pEntity)
		return false;

	// Emulate original index-based mapping:
	// start at ONE_HANDED and ++ per entry in m_grabTypes
	uint32 grabType = GRAB_TYPE_ONE_HANDED;

	for (const auto& gt : m_grabTypes)
	{
		SEntitySlotInfo slotInfo{};
		const int slotCount = pEntity->GetSlotCount();

		for (int n = 0; n < slotCount; ++n)
		{
			if (!pEntity->IsSlotValid(n))
				continue;

			const bool ok = pEntity->GetSlotInfo(n, slotInfo) && (pEntity->GetSlotFlags(n) & ENTITY_SLOT_RENDER);
			if (!ok)
				continue;

			if (slotInfo.pStatObj)
			{
				const char* geoName = slotInfo.pStatObj->GetGeoName();

				for (int j = 0; j < 2; ++j)
				{
					string helper;
					if (j == 0)
					{
						if (geoName)
						{
							helper.append(geoName);
						}
						helper.push_back('_');
					}
					helper.append(gt.helper.c_str());

					if (slotInfo.pStatObj->GetParentObject())
					{
						if (IStatObj::SSubObject* pSubObj =
							slotInfo.pStatObj->GetParentObject()->FindSubObject(helper.c_str()))
						{
							return (grabType == GRAB_TYPE_TWO_HANDED);
						}
					}
					else
					{
						if (IStatObj::SSubObject* pSubObj =
							slotInfo.pStatObj->FindSubObject(helper.c_str()))
						{
							return (grabType == GRAB_TYPE_TWO_HANDED);
						}
					}
				}
			}
			else if (slotInfo.pCharacter)
			{
				if (IAttachmentManager* pAM = slotInfo.pCharacter->GetIAttachmentManager())
				{
					if (IAttachment* pAttachment = pAM->GetInterfaceByName(gt.helper.c_str()))
					{
						return (grabType == GRAB_TYPE_TWO_HANDED);
					}
				}
			}
		}

		++grabType;
	}

	// If no predefined helper matched, mirror original fallback: default to TWO_HANDED
	return true;
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

	SetIgnoreCollisionsWithOwner(true, m_preHeldEntityId);

	pPlayer->NeedToCrouch(pPreHeldEntity->GetWorldPos());

	CWeapon *pMainWeapon = GetMainHandWeapon();

	//Unzoom weapon if neccesary
	if (pMainWeapon && (pMainWeapon->IsZoomed() || pMainWeapon->IsZooming()))
	{
		pMainWeapon->ExitZoom();
		pMainWeapon->ExitViewmodes();
	}

	if (pMainWeapon && pMainWeapon->IsWeaponRaised())
	{
		pMainWeapon->RaiseWeapon(false, true);
		pMainWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
	}

	m_mainHandIsDualWield = false;

	if (pMainWeapon && (pMainWeapon->TwoHandMode() == 1 || drop_success))
	{
		GetOwnerActor()->HolsterItem(true);

		SetMainHandWeapon(nullptr);
	}
	else
	{
		if (pMainWeapon)
		{
			if (pMainWeapon->IsDualWield() && pMainWeapon->GetDualWieldSlave())
			{
				SetMainHandWeapon(static_cast<CWeapon*>(pMainWeapon->GetDualWieldSlave()->GetIWeapon()));

				pMainWeapon->Select(false);
				m_mainHandIsDualWield = true;
			}
			else
			{
				pMainWeapon->PlayAction(g_pItemStrings->offhand_on);
				pMainWeapon->SetActionSuffix("akimbo_");
			}
		}
	}

	//Everything seems ok, start the action...
	SetOffHandState(eOHS_PICKING_ITEM);

	GetScheduler()->TimerAction(
		300,
		MakeAction([this, id = m_preHeldEntityId](CItem*) {
			this->PerformPickUp(id);
			}),
		/*persistent=*/false
	);

	PlayAction(g_pItemStrings->pickup_weapon_left, 0, false, eIPAF_Default | eIPAF_RepeatLastFrame);

	GetScheduler()->TimerAction(
		GetCurrentAnimationTime(eIGS_FirstPerson) + 100,
		MakeAction([this](CItem*) {
			this->FinishAction(eOHA_PICK_ITEM);
			}),
		/*persistent=*/false
	);

	RequireUpdate(eIUS_General);
	m_startPickUp = true;
}

//=========================================================================================================
void COffHand::EndPickUpItem()
{
	IFireMode* pReloadFM = nullptr;
	EntityId prevWeaponId = 0;

	CWeapon* pMainWeapon = GetMainHandWeapon();

	if (pMainWeapon && IsServer())
	{
		prevWeaponId = pMainWeapon->GetEntityId();
		pReloadFM = pMainWeapon->GetFireMode(pMainWeapon->GetCurrentFireMode());
		if (pReloadFM)
		{
			int fmAmmo = pReloadFM->GetAmmoCount();
			int invAmmo = GetInventoryAmmoCount(pReloadFM->GetAmmoType());
			if (pReloadFM && (pReloadFM->GetAmmoCount() != 0 || GetInventoryAmmoCount(pReloadFM->GetAmmoType()) != 0))
			{
				pReloadFM = nullptr;
			}
		}
	}

	//Restore main weapon
	if (pMainWeapon)
	{
		if (m_mainHandIsDualWield)
		{
			pMainWeapon->Select(true);
		}
		else
		{
			pMainWeapon->ResetDualWield();
			pMainWeapon->PlayAction(g_pItemStrings->offhand_off, 0, false, eIPAF_Default | eIPAF_NoBlend);
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
void COffHand::StartPickUpObject(const EntityId entityId, bool isLivingEnt /* = false */, bool fromOnReachReadyCallback /* = false */)
{
	//Grab NPCs-----------------
	CActor* pHeldActor = nullptr;
	if (isLivingEnt)
	{
		pHeldActor = CanGrabNPC(entityId);
		if (!pHeldActor)
		{
			CancelAction();
			return;
		}
		else if (!m_stats.fp)
		{
			//CryMP: If we are in thirdperson, set grab target and wait for OnReachReady callback
			if (!fromOnReachReadyCallback)
			{
				if (CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor()))
				{
					if (!pPlayer->IsGrabTargetSet(entityId))
					{
						pPlayer->SetGrabTarget(entityId);
					}
					return;
				}
			}
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

		const CActor::ObjectHoldType armHoldType = DetermineObjectHoldType(entityId);

		pPlayer->SetHeldObjectId(entityId, armHoldType);
	}

	CWeapon *pMainHandWeapon = GetMainHandWeapon();

	//Unzoom weapon if neccesary
	if (pMainHandWeapon && (pMainHandWeapon->IsZoomed() || pMainHandWeapon->IsZooming()))
	{
		pMainHandWeapon->ExitZoom();
		pMainHandWeapon->ExitViewmodes();
	}

	// if two handed or dual wield we use the fists as the mainhand weapon
	if (pMainHandWeapon && (m_grabTypes[m_grabType].twoHanded || pMainHandWeapon->TwoHandMode() >= 1))
	{
		if (m_grabTypes[m_grabType].twoHanded)
		{
			GetOwnerActor()->HolsterItem(true);

			SetMainHandWeapon(nullptr);
			pMainHandWeapon = nullptr;
		}
		else
		{
			m_prevMainHandId = pMainHandWeapon->GetEntityId();
			GetOwnerActor()->SelectItemByName("Fists", false);

			CItem* pFists = GetActorItem(GetOwnerActor());
			SetMainHandWeapon(static_cast<CWeapon*>(pFists ? pFists->GetIWeapon() : nullptr));

			pMainHandWeapon = GetMainHandWeapon();
		}
	}

	if (pMainHandWeapon)
	{

		if (pMainHandWeapon && pMainHandWeapon->IsWeaponRaised())
		{
			pMainHandWeapon->RaiseWeapon(false, true);
			pMainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
		}

		if (pMainHandWeapon->IsDualWield() && pMainHandWeapon->GetDualWieldSlave())
		{
			SetMainHandWeapon(static_cast<CWeapon*>(pMainHandWeapon->GetDualWieldSlave()->GetIWeapon()));

			pMainHandWeapon->Select(false);
			m_mainHandIsDualWield = true;
		}
		else
		{
			pMainHandWeapon->PlayAction(g_pItemStrings->offhand_on);
			pMainHandWeapon->SetActionSuffix("akimbo_");
		}


		if (pMainHandWeapon && pPlayer->IsClient())
		{
			IFireMode* pFireMode = pMainHandWeapon->GetFireMode(pMainHandWeapon->GetCurrentFireMode());
			if (pFireMode)
			{
				pFireMode->SetRecoilMultiplier(1.5f);		//Increase recoil for the weapon
			}
		}
	}
	if (!isLivingEnt)
	{
		SetOffHandState(eOHS_PICKING);

		if (!m_preHeldEntityId)
		{
			m_preHeldEntityId = entityId;
		}


		SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, m_grabTypes[m_grabType].idle);
		PlayAction(m_grabTypes[m_grabType].pickup);

		GetScheduler()->TimerAction(
			300,
			MakeAction([this, id = entityId](CItem*) {
				this->PerformPickUp(id);
				}),
			/*persistent=*/false
		);

		GetScheduler()->TimerAction(
			GetCurrentAnimationTime(eIGS_FirstPerson),
			MakeAction([this](CItem*) {
				this->FinishAction(eOHA_PICK_OBJECT);
				}),
			/*persistent=*/false
		);

		m_startPickUp = true;
	}
	else
	{
		SetOffHandState(eOHS_GRABBING_NPC);

		m_grabType = GRAB_TYPE_NPC;

		SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, m_grabTypes[m_grabType].idle);
		PlayAction(m_grabTypes[m_grabType].pickup);

		PerformGrabNPC(pHeldActor);

		GetScheduler()->TimerAction(
			GetCurrentAnimationTime(eIGS_FirstPerson),
			MakeAction([this](CItem*) {
				this->FinishAction(eOHA_GRAB_NPC);
				}),
			/*persistent=*/false
		);
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

	if (activationMode == eAAM_OnPress)
	{
		PerformThrowAction_Press(entityId, isLivingEnt);
	}
	else if (activationMode == eAAM_OnRelease)
	{
		PerformThrowAction_Release(entityId, isLivingEnt);
	}

	CWeapon *pMainHandWeapon = GetMainHandWeapon();
	if (pMainHandWeapon)
	{
		IFireMode* pFireMode = pMainHandWeapon->GetFireMode(pMainHandWeapon->GetCurrentFireMode());
		if (pFireMode)
		{
			pFireMode->SetRecoilMultiplier(1.0f);		//Restore normal recoil for the weapon
		}
	}
}

//==========================================================================================
CActor* COffHand::CanGrabNPC(const EntityId grabActorId)
{
	if (!grabActorId)
		return nullptr;

	CActor* pActor = GetOwnerActor();

	//Do not grab in prone
	if (!pActor || pActor->GetStance() == STANCE_PRONE)
		return nullptr;

	//Get actor
	CActor* pHeldActor = static_cast<CActor*>(m_pActorSystem->GetActor(grabActorId));
	if (!pHeldActor)
		return nullptr;

	IEntity* pEntity = pHeldActor->GetEntity();
	if (!pEntity->GetCharacter(0))
		return nullptr;

	return pHeldActor;
}

//==========================================================================================
bool COffHand::PerformGrabNPC(CActor* pHeldActor)
{
	CActor* pActor = GetOwnerActor();
	if (!pActor || !pHeldActor)
		return false;

	IEntity* pHeldActorEnt = pHeldActor->GetEntity();

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
			if (ICharacterInstance* pCharacter = pHeldActorEnt->GetCharacter(0))
				if (ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim())
					pSkeletonAnim->StopAnimationsAllLayers();
		}

		// just in case clear the Signal
		pAGState->SetInput("Signal", "none");
		pAGState->SetInput(actionInputID, "grabStruggleFP");
	}
	if (SActorStats* pStats = pActor->GetActorStats())
	{
		pStats->grabbedTimer = 0.0f;
	}

	// this needs to be done before sending signal "OnFallAndPlay" to make sure
	// in case AG state was FallAndPlay we leave it before AI is disabled
	if (IAnimationGraphState* pAGState = pHeldActor->GetAnimationGraphState())
	{
		pAGState->ForceTeleportToQueriedState();
		pAGState->Update();
	}

	if (IAISystem* pAISystem = gEnv->pAISystem)
	{
		IAIActor* pAIActor = CastToIAIActorSafe(pHeldActorEnt->GetAI());
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

		GetScheduler()->TimerAction(
			GetCurrentAnimationTime(eIGS_FirstPerson) + 100,
			MakeAction([this](CItem*) {
				this->FinishAction(eOHA_FINISH_MELEE);
				}),
			/*persistent=*/false
		);
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
	if (!pStatObj)
		return 0;

	pRenderNode->SetRndFlags(ERF_HIDDEN, true);
	pRenderNode->Dephysicalize();

	float scale = statObjMtx.GetColumn(0).GetLength();

	IEntityClass* pClass = m_pEntitySystem->GetClassRegistry()->FindClass("rock");
	if (!pClass)
		return 0;

	CProjectile* pRock = g_pGame->GetWeaponSystem()->SpawnAmmo(pClass);
	if (!pRock)
		return 0;

	IEntity* pEntity = pRock->GetEntity();
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
				if (grenade == 1)
					slot = eIGS_Aux1;
				else if (grenade == 2)
					slot = eIGS_OwnerLooped;
				else if (grenade == 3)
					slot = eIGS_Owner;

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

		const bool bClientEntity = (pObject->GetFlags() & ENTITY_FLAG_CLIENT_ONLY);
		if (!bClientEntity)
		{
			RequestFireMode(GetFireModeIdx(m_grabTypes[m_grabType].throwFM.c_str())); //CryMP: added this here, early enough 

			pOwner->GetGameObject()->InvokeRMI(CPlayer::SvRequestPickUpItem(), 
				CPlayer::ItemIdParam(objectId), eRMI_ToServer);

			return true;
		}
	}
	return false;
}

//==============================================================
bool COffHand::PickUpObject_MP(CPlayer* pPlayer, const EntityId synchedObjectId) //Called from CPlayer.cpp
{
	if (!pPlayer)
		return false;

	IEntity* pObject = m_pEntitySystem->GetEntity(synchedObjectId);
	if (!pObject)
		return false;

	//Check if someone else is carrying the object already 
	const int count = m_pActorSystem->GetActorCount();
	if (count > 1)
	{
		IActorIteratorPtr pIter = m_pActorSystem->CreateActorIterator();
		while (IActor* pActor = pIter->Next())
		{
			CPlayer* pOwnerPlayer = CPlayer::FromIActor(pActor);
			if (pOwnerPlayer)
			{
				if (pOwnerPlayer->IsClient() && pOwnerPlayer == pPlayer)
				{
					continue;
				}
				else
				{
					if (pOwnerPlayer->GetHeldObjectId() == synchedObjectId)
					{
						COffHand* pOffHand = static_cast<COffHand*>(pOwnerPlayer->GetItemByClass(CItem::sOffHandClass));
						if (pOffHand)
						{
							//stolen object event
							pOffHand->ThrowObject_MP(pOwnerPlayer, synchedObjectId, true);
						}
					}
				}
			}
		}
	}

	if (!gEnv->bClient)
	{
		if (CActor* pActor = GetOwnerActor())
		{
			pActor->SetHeldObjectId(synchedObjectId);
		}
		return true;
	}

	StartPickUpObject(synchedObjectId, false); 

	if (gEnv->bClient && pPlayer->IsRemote())
	{
		if (!IsSelected())
		{
			Select(true); //CryMP: Starts updates
		}
	}

	return true;
}

//==============================================================
bool COffHand::ThrowObject_MP(CPlayer* pPlayer, const EntityId synchedObjectId, bool stealingObject) //Called from CPlayer.cpp
{
	if (!pPlayer)
		return false;

	if (!gEnv->bClient)
	{
		if (CActor* pActor = GetOwnerActor())
		{
			pActor->SetHeldObjectId(0);
		}
		return true;
	}

	const EntityId playerId = pPlayer->GetEntityId();

	if (stealingObject)
	{
		FinishAction(eOHA_RESET);
	}
	else
	{
		//pPlayer->PlayAnimation("combat_plantUB_c4_01", 1.0f, false, true, 1);
		if (pPlayer->IsRemote())
		{
			//CryMP: This will call eOHA_RESET on a timer
			FinishAction(eOHA_THROW_OBJECT);
		}
	}

	//CryMP: Small bump to the object in mp after 1 sec
	GetScheduler()->TimerAction(
		1000,
		MakeAction([this, eId = synchedObjectId](CItem*) {
			IEntity* pObject = m_pEntitySystem->GetEntity(eId);
			if (pObject)
			{
				AwakeEntityPhysics(pObject);
			}
			}),
		/*persistent=*/true
	);

	return true;
}


void COffHand::AttachObjectToHand(bool attach, EntityId objectId, bool throwObject)
{
	if (!gEnv->bClient)
		return;

	CActor* pOwner = GetOwnerActor();
	if (!pOwner)
		return;

	const EntityId ownerId = pOwner->GetEntityId();
	IEntity* pObject = m_pEntitySystem->GetEntity(objectId);

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
		if (pAttachment)
		{
			if (pAttachment->GetIAttachmentObject() && pAttachment->GetIAttachmentObject()->GetAttachmentType() == IAttachmentObject::eAttachment_Entity)
			{
				CEntityAttachment* pCurrent = static_cast<CEntityAttachment*>(pAttachment->GetIAttachmentObject());
				if (pCurrent && pCurrent->GetEntityId() == objectId)
				{
					return;
				}
			}

			IPhysicalEntity* pPhysEnt = pObject->GetPhysics();

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

			Matrix34 holdMatrix = GetHoldOffset(pObject);

			// ----------------------------
			// Rotation setup 
			// ----------------------------
			Quat rotationOffset = Quat(Matrix33(holdMatrix));
			if (ISkeletonPose* pSkeletonPose = pOwnerCharacter ? pOwnerCharacter->GetISkeletonPose() : nullptr)
			{
				Quat rollQuaternion = Quat::CreateRotationXYZ(Ang3(DEG2RAD(60), DEG2RAD(90), DEG2RAD(50)));
				rotationOffset = rollQuaternion * rotationOffset;

				if (isActor)
				{
					rotationOffset = Quat::CreateRotationX(DEG2RAD(180)) * rotationOffset;
				}

				rotationOffset.Normalize();
			}

			// ---------------------------------------------------------
			// POSITION OFFSET
			// ---------------------------------------------------------
			Vec3 positionOffset = Vec3(ZERO);

			if (isTwoHand)
			{
				Vec3 vOffset = Vec3(ZERO);

				AABB bbox;
				pObject->GetLocalBounds(bbox);

				const float lengthX = fabsf(bbox.max.x - bbox.min.x);
				const float lengthY = fabsf(bbox.max.y - bbox.min.y);
				// const float lengthZ = fabsf(bbox.max.z - bbox.min.z);

				vOffset.y = 0.5f + (std::max(lengthY, lengthX) * 0.2f);

				Vec3 P_local = 0.5f * (bbox.min + bbox.max); // visual center

				if (pPhysEnt)
				{
					pe_status_dynamics sd;
					if (pPhysEnt->GetStatus(&sd))
					{
						const Matrix34 objWMInv = pObject->GetWorldTM().GetInverted();
						P_local = objWMInv.TransformPoint(sd.centerOfMass);
					}
				}

				const Quat R = rotationOffset;

				const float kComHeightX = 0.0f;   
				const float kCenterSideZ = 0.0f;    

				const Vec3 desiredLocalPoint(kComHeightX, vOffset.y, kCenterSideZ);
				Vec3 newPosOffset = desiredLocalPoint - (R * P_local);

				positionOffset = newPosOffset;
			}
			else
			{
				if (m_grabbedNPCSpecies == eGCT_HUMAN)
				{
					positionOffset = Vec3(-0.2f, 0.7f, 0.0f);
				}
				else if (m_grabbedNPCSpecies == eGCT_TROOPER || m_grabbedNPCSpecies == eGCT_ALIEN)
				{
					positionOffset = Vec3(0.25f, 0.7f, 0.0f);
				}
				else
				{
					// One-hand default
					positionOffset = Vec3(-0.4f, 0.7f, 0.0f);
				}
			}

			Vec3 fpPosOffset = Vec3(ZERO);
			Vec3 tpPosOffset = Vec3(ZERO);
			GetPredefinedPosOffset(pObject, fpPosOffset, tpPosOffset);

			positionOffset += tpPosOffset;

			// Apply final (rotation + position)
			pAttachment->SetAttRelativeDefault(QuatT(rotationOffset, positionOffset));

			// ----------------
			// IK scheduling
			// ----------------

			if (CPlayer* pPlayer = CPlayer::FromActor(pOwner))
			{
				pPlayer->SetArmIKLocalInvalid();
			}

			Vec3 left = Vec3(ZERO);
			Vec3 right = Vec3(ZERO);

			if (!GetPredefinedGripHandPos(pObject, left, right))
			{
				GetScheduler()->TimerAction(
					200, //needs to be called with a delay because of raycasts
					MakeAction([this, obId = objectId, two = isTwoHand](CItem* /*unused*/) {

						CActor* pOwner = this->GetOwnerActor();
						IEntity* pObject = gEnv->pEntitySystem->GetEntity(obId);
						if (!pOwner || !pObject)
							return;

						SGripHitLocal rayGrips =
							ComputeGripHitsLocal(pOwner, pObject, two);

						if (!rayGrips.ok)
						{
							//CryLogAlways("[AttachGrip] Ray grips failed; keeping previous grips.");
							return;
						}

						Vec3 left = rayGrips.leftLocal;
						Vec3 right = rayGrips.rightLocal;

						if (CPlayer* pPlayer = CPlayer::FromActor(pOwner))
						{
							pPlayer->SetArmIKLocal(left, right);
						}
						}),
					false
				);
			}
			else
			{
				if (CPlayer* pPlayer = CPlayer::FromActor(pOwner))
				{
					pPlayer->SetArmIKLocal(left, right);
				}
			}
		}
	}
	else
	{
		if (pAttachment)
		{
			pAttachment->ClearBinding();
		}
	}
}

//==============================================================
void COffHand::UpdateEntityRenderFlags(const EntityId entityId, EntityFpViewMode mode)
{
	if (!gEnv->bClient)
		return;

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
				m_footAlignmentEnabled = enable;
			}
		}
	}
}

//==============================================================
bool COffHand::SetHeldEntityId(const EntityId entityId)
{
	m_heldVehicleCollisions = 0;

	if (m_heldEntityId == entityId)
	{
		CryLogWarningAlways("Failed to set held entity ID %u: already holding that entity");
		return false;
	}

	if (m_heldEntityId && entityId)
	{
		RemoveHeldEntityId(m_heldEntityId, ConstraintReset::Immediate);
	}

	CActor* pActor = GetOwnerActor();
	if (!pActor)
		return false;

	m_heldEntityId = entityId;

	const bool isNewItem = (m_pItemSystem->GetItem(entityId) != nullptr);

	if (!isNewItem || !entityId)
	{
		const CActor::ObjectHoldType armHoldType = DetermineObjectHoldType(entityId);

		pActor->SetHeldObjectId(entityId, armHoldType);

		if (CPlayer *pPlayer = CPlayer::FromActor(pActor))
		{
			if (armHoldType == CActor::ObjectHoldType::TwoHanded || armHoldType == CActor::ObjectHoldType::Vehicle)
			{
				pPlayer->SetExtension("c4");
			}
			else
			{
				pPlayer->SetExtension("nw");
				pPlayer->SetInput("holding_grenade", true);
			}
		}
	}

	if (!gEnv->bClient)
		return true;

	if (!entityId || isNewItem)
	{
		return true;
	}

	HandleNewHeldEntity(entityId, isNewItem, pActor);

	return true;
}

//==============================================================
bool COffHand::RemoveHeldEntityId(const EntityId oldId /* = 0*/, ConstraintReset constraintReset /* = ConstraintReset::Immediate*/)
{
	CActor* pActor = GetOwnerActor();
	if (pActor)
	{
		pActor->SetHeldObjectId(0);
	}

	const EntityId oldHeldEntityId = oldId ? oldId : m_heldEntityId;
	if (!oldHeldEntityId)
	{
		return false;
	}

	m_heldEntityId = 0;

	if (!gEnv->bClient)
		return true;

	const bool isOldItem = (m_pItemSystem->GetItem(oldHeldEntityId) != nullptr);

	HandleOldHeldEntity(oldHeldEntityId, isOldItem, constraintReset, pActor);

	return true;
}

//==============================================================
void COffHand::HandleNewHeldEntity(const EntityId entityId, const bool isNewItem, CActor* pActor)
{
	IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);
	if (!pEntity || !pActor)
		return;

	SelectGrabType(pEntity);

	EnableFootGroundAlignment(false);

	if (!pActor->IsThirdPerson())
	{
		UpdateEntityRenderFlags(entityId, EntityFpViewMode::ForceActive);
	}

	// Remote client in MP: mark client-only and zero mass while storing original mass on actor
	if (gEnv->bClient && gEnv->bMultiplayer && pActor->IsRemote())
	{
		//pEntity->SetFlags(pEntity->GetFlags() | ENTITY_FLAG_CLIENT_ONLY);

		if (IPhysicalEntity* pObjectPhys = pEntity->GetPhysics())
		{
			pe_status_dynamics dyn;
			if (pObjectPhys->GetStatus(&dyn))
			{
				pe_simulation_params simParams;
				simParams.mass = 0.0f;

				const int success = pObjectPhys->SetParams(&simParams);
				if (success)
				{
					m_heldEntityMassBackup = dyn.mass;
				}
				else
				{
					CryLogWarningAlways("Failed to set mass to 0 on object '%s'", pEntity->GetName());
				}
			}
		}
	}

	SetIgnoreCollisionsWithOwner(true, entityId);

	if (pActor->IsThirdPerson())
	{
		if (CPlayer *pPlayer = CPlayer::FromActor(pActor))
		{
			if (!pPlayer->IsGrabTargetSet(entityId))
			{
				pPlayer->SetGrabTarget(entityId);
			}
		}
	}
}

//==============================================================
void COffHand::HandleOldHeldEntity(const EntityId oldHeldEntityId, const bool isOldItem, ConstraintReset constraintReset, CActor* pActor)
{
	if (!isOldItem)
	{
		if (oldHeldEntityId)
		{
			UpdateEntityRenderFlags(oldHeldEntityId, EntityFpViewMode::ForceDisable);
			AttachObjectToHand(false, oldHeldEntityId, false);
		}
	}

	if (!oldHeldEntityId || isOldItem)
		return;

	if (IEntity* pOldEntity = m_pEntitySystem->GetEntity(oldHeldEntityId))
	{
		const bool skip = (constraintReset & ConstraintReset::SkipIfDelayTimerActive) && IsTimerEnableCollisionsActive();
		if (!skip)
		{
			if (constraintReset & ConstraintReset::Immediate)
			{
				SetIgnoreCollisionsWithOwner(false, oldHeldEntityId);
			}
			else if (constraintReset & ConstraintReset::Delayed)
			{
				m_timerEnableCollisions = GetScheduler()->TimerAction(
					500,
					MakeAction([this, id = oldHeldEntityId](CItem* /*cItem*/) {
						SetIgnoreCollisionsWithOwner(false, id);
						}),
					/*persistent=*/false
				);
			}
		}
	
		// Remote client in MP: restore mass, clear client-only, rephys vehicles, clear cached mass
		if (gEnv->bClient && gEnv->bMultiplayer && (!pActor || pActor->IsRemote())) //No actor: Actor disconnected
		{
			//pOldEntity->SetFlags(pOldEntity->GetFlags() & ~ENTITY_FLAG_CLIENT_ONLY);

			if (IPhysicalEntity* pObjectPhys = pOldEntity->GetPhysics())
			{
				pe_status_dynamics dyn;
				pObjectPhys->GetStatus(&dyn);

				const float originalMass = m_heldEntityMassBackup;
				if (originalMass > 0.0f && dyn.mass != originalMass)
				{
					pe_simulation_params simParams;
					simParams.mass = originalMass;
					
					const int success = pObjectPhys->SetParams(&simParams);
					if (success == 0)
					{
						CryLogWarningAlways("Failed to restore mass on object '%s'", pOldEntity->GetName());
					}
					m_heldEntityMassBackup = 0.0f;
				}
			}
			
			if (IVehicle* pVehicle = m_pGameFramework->GetIVehicleSystem()->GetVehicle(pOldEntity->GetId()))
			{
				// CryMP: trigger rephysicalization to avoid bugs caused by 0 mass
				reinterpret_cast<IGameObjectProfileManager*>(pVehicle + 1)->SetAspectProfile(eEA_Physics, 1);
			}
		}
	}
}

//==============================================================
void COffHand::AwakeEntityPhysics(IEntity* pEntity)
{
	if (!pEntity)
		return;

	IPhysicalEntity* pPhysics = pEntity->GetPhysics();
	if (pPhysics)
	{
		pe_action_awake actionAwake;
		actionAwake.bAwake = 1;
		pPhysics->Action(&actionAwake);
	}
}

//==============================================================
COffHand::SGripHitLocal COffHand::ComputeGripHitsLocal(CActor* pOwner, IEntity* pObject, bool isTwoHand)
{
	SGripHitLocal out;
	if (!pOwner || !pObject)
		return out;

	const float extraMargin = 0.25f;
	const float ttlSeconds = 2.0f;

	IPhysicalEntity* pPE = pObject->GetPhysics();
	if (!pPE)
	{
		return out;
	}

	AABB wbox; pObject->GetWorldBounds(wbox);
	Vec3 center = (wbox.min + wbox.max) * 0.5f;

	pe_status_dynamics sd;
	if (pPE->GetStatus(&sd))
		center = sd.centerOfMass;


	if (g_pGame->GetWeaponSystem()->GetProjectile(pObject->GetId()))
	{
		out.ok = true;
		return out;
	}

	const Vec3 ext = (wbox.max - wbox.min) * 0.5f;
	const float radius = max(ext.len(), 0.05f);
	const float d = radius + extraMargin;

	const Matrix34 playerW = pOwner->GetEntity()->GetWorldTM();
	Vec3 playerRight = playerW.GetColumn0(); playerRight.NormalizeSafe(Vec3(1, 0, 0));

	const Vec3 leftWS = center - playerRight * d;
	const Vec3 rightWS = center + playerRight * d;

	auto rayOne = [&](const Vec3& from, const Vec3& to, ray_hit& hit)->bool
		{
			const Vec3 rayVec = to - from;
			if (rayVec.GetLengthSquared() < 1e-6f)
				return false;

			const int result = gEnv->pPhysicalWorld->RayTraceEntity(pPE, from, rayVec, &hit, nullptr);
			return (result > 0);
		};

	ray_hit hitLR = {}, hitRL = {};
	const bool okLR = rayOne(leftWS, rightWS, hitLR);
	const bool okRL = isTwoHand ? rayOne(rightWS, leftWS, hitRL) : false;

	if (isTwoHand)
	{
		if (!(okLR && okRL))
		{
			return out;
		}
	}
	else
	{
		if (!okLR)
		{
			return out;
		}
	}

	const float inset = 0.01f;
	const Vec3 leftInsetWS = okLR ? (hitLR.pt + hitLR.n * (-inset)) : leftWS;
	const Vec3 rightInsetWS = (isTwoHand && okRL) ? (hitRL.pt + hitRL.n * (-inset)) : rightWS;

	const Matrix34 objW = pObject->GetWorldTM();
	const Matrix34 objInv = objW.GetInvertedFast();

	out.leftLocal = objInv.TransformPoint(leftInsetWS);
	if (isTwoHand)
		out.rightLocal = objInv.TransformPoint(rightInsetWS);

	out.ok = true;

	return out;
}

//==============================================================
bool COffHand::GetPredefinedGripHandPos(IEntity* pEnt, Vec3& outLeftEL, Vec3& outRightEL)
{
	if (!pEnt)
		return false;

	if (!gClient || !gClient->GetHandGripRegistry())
	{
		return false;
	}

	const HandGripInfo* info = gClient->GetHandGripRegistry()->GetGripByEntity(pEnt);
	if (!info)
		return false;

	bool any = false;

	if (info->hasLeft)
	{
		outLeftEL = info->leftEL;
		any = true;
	}

	if (info->hasRight)
	{
		outRightEL = info->rightEL;
		any = true;
	}

	return any; // true if at least one hand is defined
}

//==============================================================
void COffHand::GetPredefinedPosOffset(IEntity* pEnt, Vec3& fpPosOffset, Vec3& tpPosOffset)
{
	if (!pEnt)
		return;

	if (!gClient || !gClient->GetHandGripRegistry())
	{
		return;
	}

	const HandGripInfo* info = gClient->GetHandGripRegistry()->GetGripByEntity(pEnt);
	if (!info)
		return;

	fpPosOffset = info->posOffset_FP;
	tpPosOffset = info->posOffset_TP;
}

//==============================================================
bool COffHand::IsTimerEnableCollisionsActive()
{
	return m_timerEnableCollisions && GetScheduler()->IsTimerActive(m_timerEnableCollisions);
}

//==============================================================
CActor::ObjectHoldType COffHand::DetermineObjectHoldType(const EntityId entityId) const
{
	if (m_pActorSystem->GetActor(entityId))
	{
		return CActor::ObjectHoldType::Actor;
	}
	else if (m_pGameFramework->GetIVehicleSystem()->GetVehicle(entityId))
	{
		return CActor::ObjectHoldType::Vehicle;
	}
	else if (g_pGame->GetWeaponSystem()->GetProjectile(entityId))
	{
		return CActor::ObjectHoldType::Projectile;
	}
	else
	{
		if (IsGrabTypeTwoHanded(entityId))
		{
			return CActor::ObjectHoldType::TwoHanded;
		}
		else
		{
			return CActor::ObjectHoldType::OneHanded;
		}
	}
	return CActor::ObjectHoldType::None;
}

//==============================================================
void COffHand::OnThirdPersonBendReady(const EntityId targetId, bool reaching)
{
	if (m_stats.fp)
		return;

	if (!reaching)
		return;

	if (m_pActorSystem->GetActor(targetId))
	{
		StartPickUpObject(targetId, true, true);
	}

	AttachObjectToHand(true, targetId, false);
}

//==============================================================
void COffHand::OnHeldObjectCollision(CPlayer* pClientActor, const EventPhysCollision* pCollision, IEntity* pTargetEnt)
{
	if (!pClientActor || !m_heldEntityId)
		return;

	const int maxCollisions = g_pGameCVars->mp_pickupMaxVehicleCollisions;
	if (!maxCollisions)
		return;

	IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(m_heldEntityId);
	if (pVehicle)
	{
		++m_heldVehicleCollisions;
		if (m_heldVehicleCollisions > maxCollisions)
		{
			if (CHUD *pHUD = g_pGame->GetHUD())
			{
				pHUD->DisplayBigOverlayFlashMessage("@object_collision_vehicle_drop", 2.0f, 400, 400, Col_Goldenrod);
			}

			IVehicle* pCarriedVehicle = m_pVehicleSystem->GetVehicle(m_heldEntityId);
			Vec3 pos = ZERO;
			if (pCarriedVehicle && pCarriedVehicle->GetExitPositionForActor(pClientActor, pos, true))
			{
				const Ang3 angles = pCarriedVehicle->GetEntity()->GetWorldAngles(); // face same direction as vehicle.

				if (pos.GetDistance(pClientActor->GetEntity()->GetWorldPos()) < 30.f)
				{
					//CryMP: Teleport to safe exit pos near vehicle, to avoid vehicle falling on top of client
					pClientActor->GetEntity()->SetWorldTM(Matrix34::Create(Vec3(1, 1, 1), Quat(angles), pos));
				}
			}

			FinishAction(eOHA_RESET);
		}
	}
}

//==============================================================
void COffHand::OnPlayerRevive(CPlayer* pPlayer)
{
	FinishAction(eOHA_RESET);
}

//==============================================================
void COffHand::OnPlayerDied(CPlayer* pPlayer)
{
	FinishAction(eOHA_RESET);
}

//==============================================================
void COffHand::ReAttachObjectToHand()
{
	if (m_heldEntityId && !m_stats.fp)
	{
		AttachObjectToHand(false, m_heldEntityId, false);
		AttachObjectToHand(true, m_heldEntityId, false);
	}
}

//==============================================================================
void COffHand::FinishGrenadeAction(CWeapon* pMainHand)
{
	//HideItem(true);
	float timeDelay = 0.1f;	//ms

	if (pMainHand && !pMainHand->IsDualWield())
	{
		pMainHand->ResetDualWield();		//I can reset, because if DualWield it's not possible to switch grenades (see PreExecuteAction())
		pMainHand->PlayAction(g_pItemStrings->offhand_off, 0, false, CItem::eIPAF_Default | CItem::eIPAF_NoBlend);
		timeDelay = (pMainHand->GetCurrentAnimationTime(CItem::eIGS_FirstPerson) + 50) * 0.001f;
	}
	else if (GetOwnerActor() && !GetOwnerActor()->ShouldSwim())
	{
		if (pMainHand && pMainHand->IsDualWield())
		{
			pMainHand->Select(true);
		}
		else
		{
			GetOwnerActor()->HolsterItem(false);
		}
	}

	if (GetOffHandState() == eOHS_SWITCHING_GRENADE)
	{
		const int grenadeType = GetCurrentFireMode();
		AttachGrenadeToHand(grenadeType);
	}

	SetOffHandState(eOHS_TRANSITIONING);

	//Offhand goes back to initial state
	SetResetTimer(timeDelay);
	RequireUpdate(eIUS_General);
}

//==============================================================================
void COffHand::PerformThrowAction_Press(EntityId throwableId, bool isLivingEnt /*=false*/)
{
	if (!m_fm)
		return;

	if (!throwableId)
	{
		SetOffHandState(eOHS_HOLDING_GRENADE);
	}

	if (throwableId)
	{
		if (!isLivingEnt)
		{
			if (CPlayer* pClientActor = CPlayer::FromActor(GetOwnerActor()))
			{
				pClientActor->NotifyObjectGrabbed(false, throwableId, false);

				//if (!m_forceThrow)
				//{
				//	IVehicle* pCarriedVehicle = m_pVehicleSystem->GetVehicle(throwableId);
				//	Vec3 pos = ZERO;
				//	if (pCarriedVehicle && pCarriedVehicle->GetExitPositionForActor(pClientActor, pos, true))
				//	{
				//		const Ang3 angles = pCarriedVehicle->GetEntity()->GetWorldAngles(); // face same direction as vehicle.

				//		if (pos.GetDistance(pClientActor->GetEntity()->GetWorldPos()) < 30.f)
				//		{
				//			CryLogAlways("$3Teleporting to safe loc");
				//			pClientActor->GetEntity()->SetWorldTM(Matrix34::Create(Vec3(1, 1, 1), Quat(angles), pos));
				//		}
				//	}
				//}
			}

			SetOffHandState(eOHS_THROWING_OBJECT);

			CThrow* pThrow = static_cast<CThrow*>(m_fm);
			pThrow->SetThrowable(
				throwableId,
				m_forceThrow,
				MakeAction([this](CItem*) {
					this->FinishAction(eOHA_THROW_OBJECT);
					})
			);
		}
		else
		{
			SetOffHandState(eOHS_THROWING_NPC);

			CThrow* pThrow = static_cast<CThrow*>(m_fm);
			pThrow->SetThrowable(
				throwableId,
				true,
				MakeAction([this](CItem*) {
					this->FinishAction(eOHA_THROW_NPC);
					})
			);
		}

		m_forceThrow = false;
	}

	if (!m_fm->IsFiring() && m_nextThrowTimer <= 0.0f)
	{
		if (m_currentState == eOHS_HOLDING_GRENADE)
		{
			AttachGrenadeToHand(GetCurrentFireMode(), m_stats.fp);
		}

		StartFire();
		SetBusy(false);

		CWeapon *pMainHandWeapon = GetMainHandWeapon();

		if (pMainHandWeapon && m_fm->IsFiring())
		{
			if (!(m_currentState & (eOHS_THROWING_NPC | eOHS_THROWING_OBJECT)))
			{
				if (pMainHandWeapon->IsWeaponRaised())
				{
					pMainHandWeapon->RaiseWeapon(false, true);
					pMainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
				}

				pMainHandWeapon->PlayAction(g_pItemStrings->offhand_on);
				pMainHandWeapon->SetActionSuffix("akimbo_");
			}
		}

		if (!throwableId)
		{
			if (pMainHandWeapon && pMainHandWeapon->GetEntity()->GetClass() == CItem::sFistsClass)
			{
				CFists* pFists = static_cast<CFists*>(pMainHandWeapon);
				pFists->RequestAnimState(CFists::eFAS_FIGHT);
			}
		}
	}
}

//===============================================================================
void COffHand::PerformThrowAction_Release(EntityId throwableId, bool isLivingEnt /*=false*/)
{
	if (!m_fm)
		return;

	if (m_nextThrowTimer > 0.0f)
		return;

	CThrow* pThrow = static_cast<CThrow*>(m_fm);

	if (m_currentState != eOHS_HOLDING_GRENADE)
	{
		if (gEnv->bMultiplayer)
		{
			IEntity* pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
			if (pEntity && pEntity->GetPhysics())
			{
				Vec3 hit = pThrow->GetProbableHit(WEAPON_HIT_RANGE);
				Vec3 pos = pThrow->GetFiringPos(hit);
				Vec3 dir = pThrow->GetFiringDir(hit, pos);

				if (pThrow->CheckForIntersections(pEntity->GetPhysics(), dir))
				{
					if (isLivingEnt)
						SetOffHandState(eOHS_HOLDING_NPC);
					else
						SetOffHandState(eOHS_HOLDING_OBJECT);

					if (CHUD* pHUD = g_pGame->GetHUD())
					{
						pHUD->DisplayBigOverlayFlashMessage("@object_cant_throw_here", 2.0f, 400, 400, Col_Goldenrod);
					}
					return;
				}
			}
		}

		pThrow->ThrowingGrenade(false);
	}
	else if (m_currentState == eOHS_HOLDING_GRENADE)
	{
		SetOffHandState(eOHS_THROWING_GRENADE);

		pThrow->ThrowingGrenade(true);
	}
	else
	{
		CancelAction();
		return;
	}

	if (gEnv->bMultiplayer)
	{
		if (throwableId)
		{
			//CryMP: Small bump to the object in mp after 1 sec
			GetScheduler()->TimerAction(
				1000,
				MakeAction([this, eId = throwableId](CItem* /*unused*/) {
					IEntity* pObject = m_pEntitySystem->GetEntity(eId);
					if (pObject)
					{
						AwakeEntityPhysics(pObject);
					}
					}),
				/*persistent=*/true
			);
		}
	}

	m_nextThrowTimer = 60.0f / m_fm->GetFireRate();

	StopFire();

	if (m_fm->IsFiring() && m_currentState == eOHS_THROWING_GRENADE)
	{
		GetScheduler()->TimerAction(
			GetCurrentAnimationTime(CItem::eIGS_FirstPerson),
			MakeAction([this, hand = GetMainHandWeapon()](CItem*) {
				this->FinishGrenadeAction(hand);
				}),
			/*persistent=*/false
		);
	}
}

//===============================================================================
void COffHand::DebugLogInfo()
{
	IEntity* pEntity = GetEntity();
	CActor* pOwnerActor = GetOwnerActor();
	if (!pOwnerActor)
		return;

	const char* itemName = pEntity ? pEntity->GetName() : "<no entity>";

	// -- Owner --
	const char* ownerName = "<no owner>";
	if (pOwnerActor)
	{
		if (IEntity* pOwnerEntity = pOwnerActor->GetEntity())
			ownerName = pOwnerEntity->GetName();
	}

	// -- Grab Type --
	const char* grabStr = "Unknown";
	switch (m_grabType)
	{
	case GRAB_TYPE_ONE_HANDED: grabStr = "1-Hand"; break;
	case GRAB_TYPE_TWO_HANDED: grabStr = "2-Hand"; break;
	case GRAB_TYPE_NPC:        grabStr = "NPC"; break;
	default: break;
	}

	// -- Held Entity --
	EntityId heldId = m_heldEntityId;
	if (!heldId)
	{
		if (CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor()))
		{
			heldId = pPlayer->GetHeldObjectId();
		}
	}

	IEntity* pHeld = gEnv->pEntitySystem->GetEntity(heldId);

	float mass = -1.f;
	if (pHeld)
	{
		if (IPhysicalEntity* pPhys = pHeld->GetPhysics())
		{
			pe_status_dynamics dyn;
			if (pPhys->GetStatus(&dyn))
			{
				mass = dyn.mass;
			}
		}
	}

	// -- Current State Bitmask --
	string stateBits;
	static constexpr struct { int flag; const char* name; } FLAGS[] = {
		{ eOHS_INIT_STATE,        "Init" },
		{ eOHS_SWITCHING_GRENADE, "SwitchingGrenade" },
		{ eOHS_HOLDING_GRENADE,   "HoldingGrenade" },
		{ eOHS_THROWING_GRENADE,  "ThrowingGrenade" },
		{ eOHS_PICKING,           "Picking" },
		{ eOHS_PICKING_ITEM,      "PickingItem" },
		{ eOHS_PICKING_ITEM2,     "PickingItem2" },
		{ eOHS_HOLDING_OBJECT,    "HoldingObject" },
		{ eOHS_THROWING_OBJECT,   "ThrowingObject" },
		{ eOHS_GRABBING_NPC,      "GrabbingNPC" },
		{ eOHS_HOLDING_NPC,       "HoldingNPC" },
		{ eOHS_THROWING_NPC,      "ThrowingNPC" },
		{ eOHS_TRANSITIONING,     "Transitioning" },
		{ eOHS_MELEE,             "Melee" }
	};

	for (const auto& f : FLAGS)
	{
		if (m_currentState & f.flag)
		{
			if (!stateBits.empty()) stateBits += "|";
			stateBits += f.name;
		}
	}

	// -- Game Object Update Status --
	IGameObject* pGameObject = GetGameObject();
	uint8_t slotEnables = pGameObject->GetUpdateSlotEnables(this, eIUS_General);
	uint8_t slotEnables2 = pGameObject->GetUpdateSlotEnables(this, eIUS_Scheduler);

	// -- Fire Mode Index --
	int fireMode = GetCurrentFireMode();

	IGameObject* pActorObject = pOwnerActor->GetGameObject();
	uint8_t playerUpdating = pActorObject->GetUpdateSlotEnables(this, eIUS_General);

	// -- Final Output --
	CryLogAlways("$9[OffHand] Owner=$5%s$9 (Updating:%s%s$9), Item=%s, Grab=%s, Mode=%d, Timer=%s%.2f$9, Update=%s%s",
		ownerName, playerUpdating ? "$3" : "$9", playerUpdating ? "Yes" : "No", itemName, grabStr, fireMode,
		m_nextThrowTimer > 0.0f ? "$3" : "$9", m_nextThrowTimer,
		slotEnables ? "$3" : "$9", slotEnables ? "Yes" : "No");

	CryLogAlways("$9           StateFlags: $6%s", stateBits.c_str());

	CryLogAlways("$9           FireMode Handle: $3%p", GetActiveFireMode());

	if (m_resetTimer > 0.0f)
		CryLogAlways("$9           ResetTimer: %f", m_resetTimer);

	CryLogAlways("$9           OffHand selected: %s", IsSelected() ? "$5Yes" : "$8No");

	if (GetScheduler()->GetTimersCount() > 0 || GetScheduler()->GetActivesCount() > 0)
		CryLogAlways("$9           Scheduler: Actives %d - Timers %d", GetScheduler()->GetActivesCount(), GetScheduler()->GetTimersCount());

	CryLogAlways("$9           Scheduler Update: $1%d", slotEnables2);

	if (pHeld)
	{
		CryLogAlways("$9           Holding: %s (%d), Mass=$1%.2f", pHeld->GetName(), heldId, mass);

		if (IPhysicalEntity* pPhys = pHeld->GetPhysics())
		{
			pe_status_nparts snp;
			const int nParts = pPhys->GetStatus(&snp); // CE2: returns number of parts directly
			CryLogAlways("$9           Physics parts: $6%d", nParts);

			for (int i = 0; i < nParts; ++i)
			{
				pe_params_part pp;
				pp.ipart = i;
				if (!pPhys->GetParams(&pp))
				{
					CryLogAlways("$9           Part %d: $4<GetParams failed>", i);
					continue;
				}

				const phys_geometry* pPG = pp.pPhysGeomProxy ? pp.pPhysGeomProxy : pp.pPhysGeom;
				const char* label = pp.pPhysGeomProxy ? "proxy" : "geom";

				const int gtype = (pPG && pPG->pGeom) ? pPG->pGeom->GetType() : -1;
				const char* typeStr = "unknown";

				switch (gtype)
				{
				case GEOM_TRIMESH:    typeStr = "trimesh";    break;
				case GEOM_HEIGHTFIELD:typeStr = "heightfield"; break;
				case GEOM_CYLINDER:   typeStr = "cylinder";   break;
				case GEOM_CAPSULE:    typeStr = "capsule";    break;
				case GEOM_RAY:        typeStr = "ray";        break;
				case GEOM_SPHERE:     typeStr = "sphere";     break;
				case GEOM_BOX:        typeStr = "box";        break;
				case GEOM_VOXELGRID:  typeStr = "voxelgrid";  break;
				default:              typeStr = "unknown";    break;
				}

				CryLogAlways("$9           Part %d: %s: $6%s", i, label, typeStr);
			}
		}
		else
		{
			CryLogAlways("$9           Physics: None");
		}
	}
	else
	{
		CryLogAlways("$9           Holding: None");
	}

	auto* pHolster = pOwnerActor->GetHolsteredItem();
	if (pHolster)
	{
		CryLogAlways("$9           Holstered: $5%s$9",
			pHolster->GetEntity()->GetName());
	}
	auto* pCurr = pOwnerActor->GetCurrentItem();
	if (pCurr)
	{
		CryLogAlways("$9           Item: $5%s$9",
			pCurr->GetEntity()->GetName());
	}
	CryLogAlways("$9           %s", GetEntity()->IsHidden() ? "OffHand Entity Hidden" : "OffHand Entity $3Visible");
	CryLogAlways("$9--------------------------------------------");
	//CryLogAlways("$9           Character Master: %s", IsFirstPersonCharacterMasterHidden() ? "Hidden" : "$3Visible$9");
	//CryLogAlways("$9           Hands: %d", m_stats.hand);
	//const ItemString& attachment = GetParams().attachment[m_stats.hand];
	//CryLogAlways("$9           Attachment Name: %s (%s)", attachment.c_str(), IsCharacterAttachmentHidden(eIGS_FirstPerson, attachment.c_str()) ? "Hidden" : "$3Visible$9");
	//CryLogAlways("$9           %s", IsArmsHidden() ? "FP Arms Hidden" : "FP Arms $3Visible");

	// -- Constraint Status --
	const char* constraintStr = "Inactive";
	const char* constraintClr = "$9"; // gray

	switch (m_constraintStatus)
	{
	case ConstraintStatus::Inactive:
		constraintStr = "Inactive";
		constraintClr = "$9"; // gray
		break;
	case ConstraintStatus::WaitForPhysicsUpdate:
		constraintStr = "WaitingForPhys";
		constraintClr = "$3"; // yellow/orange
		break;
	case ConstraintStatus::Active:
		constraintStr = "Active";
		constraintClr = "$5"; // green
		break;
	case ConstraintStatus::Broken:
		constraintStr = "Broken";
		constraintClr = "$4"; // red
		break;
	default:
		break;
	}

	CryLogAlways("$9           Constraint: %s%s$9", constraintClr, constraintStr);

	CryLogAlways("$9           Foot Alignment: %s", m_footAlignmentEnabled ? "$1Enabled" : "Disabled");
}

