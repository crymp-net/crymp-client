/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 12:04:2006   17:22 : Created by Márcio Martins
- 18:02:2007	 13:30 : Refactored Offhand by Benito G.R.

*************************************************************************/

#ifndef __OFFHAND_H__
#define __OFFHAND_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "CryCommon/CryAction/IItemSystem.h"
#include "Weapon.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryCommon/CryAction/IViewSystem.h"

//================ Grab States ================
#define OH_NO_GRAB           0
#define OH_GRAB_OBJECT       1
#define OH_GRAB_ITEM         2
#define OH_GRAB_NPC          3

//================ Grab Types ================
#define GRAB_TYPE_ONE_HANDED   0
#define GRAB_TYPE_TWO_HANDED   1
#define GRAB_TYPE_NPC          2

//================ Item Exchange =============
#define ITEM_NO_EXCHANGE     0
#define ITEM_CAN_PICKUP      1
#define ITEM_CAN_EXCHANGE    2

//================ Inputs ====================
#define INPUT_DEF            0
#define INPUT_USE            1
#define INPUT_LBM            2
#define INPUT_RBM            3

//================ Timers & Limits ===========
#define KILL_NPC_TIMEOUT     7.25f
#define TIME_TO_UPDATE_CH    0.25f
#define MAX_CHOKE_SOUNDS     5
#define MAX_GRENADE_TYPES    4
#define OFFHAND_RANGE        2.5f

enum EOffHandActions
{
	eOHA_NO_ACTION = 0,
	eOHA_SWITCH_GRENADE = 1,
	eOHA_THROW_GRENADE = 2,
	eOHA_USE = 3,
	eOHA_PICK_ITEM = 4,
	eOHA_PICK_OBJECT = 5,
	eOHA_THROW_OBJECT = 6,
	eOHA_GRAB_NPC = 7,
	eOHA_THROW_NPC = 8,
	eOHA_RESET = 9,
	eOHA_FINISH_AI_THROW_GRENADE = 10,
	eOHA_MELEE_ATTACK = 11,
	eOHA_FINISH_MELEE = 12,
	eOHA_REINIT_WEAPON = 13,
};

enum EOffHandStates
{
	eOHS_INIT_STATE = 0x00000001,
	eOHS_SWITCHING_GRENADE = 0x00000002,
	eOHS_HOLDING_GRENADE = 0x00000004,
	eOHS_THROWING_GRENADE = 0x00000008,
	eOHS_PICKING = 0x00000010,
	eOHS_PICKING_ITEM = 0x00000020,
	eOHS_PICKING_ITEM2 = 0x00000040,
	eOHS_HOLDING_OBJECT = 0x00000080,
	eOHS_THROWING_OBJECT = 0x00000100,
	eOHS_GRABBING_NPC = 0x00000200,
	eOHS_HOLDING_NPC = 0x00000400,
	eOHS_THROWING_NPC = 0x00000800,
	eOHS_TRANSITIONING = 0x00001000,
	eOHS_MELEE = 0x00002000
};

enum EOffHandSounds
{
	eOHSound_Choking_Trooper = 0,
	eOHSound_Choking_Human = 1,
	eOHSound_Kill_Human = 2,
	eOHSound_LastSound = 3
};


class COffHand : public CWeapon
{

	struct SGrabType
	{
		ItemString	helper;
		ItemString	pickup;
		ItemString	idle;
		ItemString	throwFM;
		bool		twoHanded;
	};

	typedef std::vector<SGrabType>				TGrabTypes;

public:

	COffHand();
	virtual ~COffHand();

	virtual void Update(SEntityUpdateContext& ctx, int slot);
	void CheckTimers(float frameTime);
	virtual void PostUpdate(float frameTime);
	virtual void PostInit(IGameObject* pGameObject);
	virtual void Reset();

	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);

	virtual bool CanSelect() const;
	virtual void Select(bool select);
	virtual void FullSerialize(TSerialize ser);
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	virtual void PostSerialize();

	virtual void MeleeAttack();

	virtual void PostFilterView(struct SViewParams& viewParams);

	//Only needed because is used in CFists
	virtual void EnterWater(bool enter) {}

	virtual void UpdateFPView(float frameTime);

	//AIGrenades (for AI)
	virtual void PerformThrow(float speedScale);

	//Memory Statistics
	virtual void GetMemoryStatistics(ICrySizer* s) { s->Add(*this); CWeapon::GetMemoryStatistics(s); }

	void  SetOffHandState(EOffHandStates eOHS);
	void SetMainHand(CItem* pItem);
	void SetMainHandWeapon(CWeapon* pWeapon);
	ILINE int  GetOffHandState() { return m_currentState; }
	void  FinishAction(EOffHandActions eOHA);
	virtual void Freeze(bool freeze);

	bool IsHoldingEntity();

	void OnBeginCutScene();
	void OnEndCutScene();

	void GetAvailableGrenades(std::vector<string>& grenades);

	ILINE void SetResetTimer(float t) { m_resetTimer = t; }
	ILINE int32 GetGrabType() const { return m_grabType; }

	float GetObjectMassScale();  //Scale for mouse sensitivity

	virtual bool ReadItemParams(const IItemParamsNode* root);

	void	SelectGrabType(IEntity* pEntity);

	Matrix34 GetHoldOffset(IEntity* pEntity);

	bool EvaluateStateTransition(int requestedAction, int activationMode, int inputMethod);
	bool PreExecuteAction(int requestedAction, int activationMode, bool forceSelect = false);
	void CancelAction();

	void SetIgnoreCollisionsWithOwner(bool activate, EntityId entityId = 0);
	void DrawNear(bool drawNear, EntityId entityId = 0);
	bool PerformPickUp();
	int  CanPerformPickUp(CActor* pActor, IPhysicalEntity* pPhysicalEntity = NULL, bool getEntityInfo = false);
	void OnLookAtEntityChanged(IEntity* pEntity);
	int  CheckItemsInProximity(Vec3 pos, Vec3 dir, bool getEntityInfo);

	void UpdateCrosshairUsabilitySP();
	void UpdateCrosshairUsabilityMP();
	void UpdateHeldObject();
	void UpdateGrabbedNPCState();
	void UpdateGrabbedNPCWorldPos(IEntity* pEntity, struct SViewParams* viewParams);
	bool GetGrabbedActorNeckWorldPos(IEntity* pEntity, Vec3& outNeckPos) const;

	void StartSwitchGrenade(bool xi_switch = false, bool fakeSwitch = false);
	void EndSwitchGrenade();

	//Offhand (for Player)
	void PerformThrow(int activationMode, EntityId throwableId, int oldFMId = -1, bool isLivingEnt = false);

	void StartPickUpItem();
	void EndPickUpItem();

	void StartPickUpObject(const EntityId entityId, bool isLivingEnt /* = false */);
	void StartThrowObject(const EntityId entityId, int activationMode, bool isLivingEnt /*= false*/);

	void NetStartFire();
	void NetStopFire();

	bool GrabNPC();
	void ThrowNPC(const EntityId entityId, bool kill = true/*= true*/);

	//Special stuff for grabbed NPCs
	void RunEffectOnGrabbedNPC(CActor* pNPC);
	void PlaySound(EOffHandSounds sound, bool play);

	EntityId	GetHeldEntityId() const;

	void AttachGrenadeToHand(int grenade, bool fp = true, bool attach = true);

	virtual void ForcePendingActions() {}

	virtual void OnEnterFirstPerson() override;
	virtual void OnEnterThirdPerson() override;

private:

	EntityId SpawnRockProjectile(IRenderNode* pRenderNode);

	int		CanExchangeWeapons(IItem* pItem, IItem** pExchangeItem);
	IItem* GetExchangeItem(CPlayer* pPlayer);

	void	PostPostSerialize();

	// Grenade info
	int m_lastFireModeId = 0;

	float m_nextThrowTimer = -1.0f;
	float m_lastCHUpdate = 0.0f;

	// Grabbing system
	TGrabTypes m_grabTypes;
	uint32 m_grabType = GRAB_TYPE_TWO_HANDED;
	EntityId m_heldEntityId = 0;
	EntityId m_preHeldEntityId = 0;
	EntityId m_crosshairId = 0;
	EntityId m_lastLookAtEntityId = 0;
	Matrix34 m_holdOffset;
	Vec3 m_holdScale = Vec3(1.0f, 1.0f, 1.0f);
	int m_constraintId = 0;
	bool m_hasHelper = false;
	int m_grabbedNPCSpecies = eGCT_UNKNOWN;
	float m_heldEntityMass = 0.0f;

	float m_killTimeOut = -1.0f;
	bool m_killNPC = false;
	bool m_effectRunning = false;
	bool m_npcWasDead = false;
	bool m_startPickUp = false;
	int m_grabbedNPCInitialHealth = 0;
	bool m_forceThrow = false;

	// Sound
	tSoundID m_sounds[eOHSound_LastSound]; 

	// Usage state
	float m_range = OFFHAND_RANGE;
	float m_pickingTimer = -1.0f;
	float m_resetTimer = -1.0f;
	int m_usable = false;

	bool m_bCutscenePlaying = false;

	float m_fGrenadeToggleTimer = -1.0f;
	float m_fGrenadeThrowTimer = -1.0f;

	// Weapon/main hand state
	int m_currentState = eOHS_INIT_STATE;
	CItem* m_mainHand = nullptr;
	CWeapon* m_mainHandWeapon = nullptr;
	EntityId m_prevMainHandId = 0;
	bool m_mainHandIsDualWield = false;
	bool m_restoreStateAfterLoading = false;

	IRenderNode* m_pRockRN = nullptr;

	Matrix34 m_lastNPCMatrix; 
	Matrix34 m_intialBoidLocalMatrix;

	bool m_useFPCamSpacePP = false;

	//Input actions
	static TActionHandler<COffHand> s_actionHandler;

	bool ProcessOffHandActions(EOffHandActions eOHA, int input, int activationMode, float value = 0.0f);
	bool OnActionUse(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionAttack(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionDrop(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionThrowGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionXIThrowGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionSwitchGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionXISwitchGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionSpecial(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	void RegisterActions();

private:

	enum class EntityFpViewMode
	{
		Default,
		ForceActive,
		ForceDisable,
		ForceUpdate,
	};

	bool m_objectFpMode = false;

	enum class ConstraintStatus
	{
		Inactive,
		WaitForPhysicsUpdate,
		Active,
		Broken
	};

	ConstraintStatus m_constraintStatus = ConstraintStatus::Inactive;

public:

	bool IsTwoHandMode() const
	{
		return m_grabType == 1; //GRAB_TYPE_TWO_HANDED;
	}
	bool Request_PickUpObject_MP();
	bool PickUpObject_MP(CPlayer* pPlayer, const EntityId synchedObjectId);
	bool ThrowObject_MP(CPlayer* pPlayer, const EntityId synchedObjectId, bool stealingObject = false);
	void AttachObjectToHand(bool attach, EntityId objectId, bool throwObject);
	void UpdateEntityRenderFlags(const EntityId entityId, EntityFpViewMode mode = EntityFpViewMode::Default);
	void EnableFootGroundAlignment(bool enable);
	void SetHeldEntityId(const EntityId id, const EntityId oldId = 0);
};

#endif
