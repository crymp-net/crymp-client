/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Simple Actor implementation

 -------------------------------------------------------------------------
  History:
  - 7:10:2004   14:46 : Created by Márcio Martins

*************************************************************************/
#ifndef __Actor_H__
#define __Actor_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "CryCommon/CryAction/IActorSystem.h"
#include "CryCommon/CryAction/IActionMapManager.h"
#include "CryCommon/CryGame/CoherentValue.h"
#include "CryCommon/CrySoundSystem/IMusicSystem.h"

#include "CryGame/Game.h" // for stance enum
#include "CryCommon/CryAISystem/IAgent.h" // for stance enum
#include "CryCommon/CryAction/IAnimationGraph.h"
#include "CryCommon/CryAction/IAnimatedCharacter.h"
#include "CryCommon/CryAction/IMovementController.h"
#include "ScreenEffects.h"
#include "GrabHandler.h"
#include "Player/WeaponAttachmentManager.h"

enum class ActorType
{
	CActor,
	CPlayer,
	CAlien,
	CShark,
};

struct SActorFrameMovementParams
{
	SActorFrameMovementParams() :
		desiredVelocity(ZERO),
		desiredLean(0.0f),
		deltaAngles(ZERO),
		lookTarget(ZERO),
		aimTarget(ZERO),
		lookIK(false),
		aimIK(false),
		jump(false),
		strengthJump(false),
		sprint(0.0f),
		stance(STANCE_NULL),
		allowStrafe(false)
	{
	}

	// desired velocity for this frame (meters/second) (in local space)
	Vec3 desiredVelocity;
	// desired lean
	float desiredLean;
	// desired turn
	Ang3 deltaAngles;
	Vec3 lookTarget;
	Vec3 aimTarget;
	// prediction
	SPredictedCharacterStates prediction;
	// look target
	bool lookIK;
	// aim target
	bool aimIK;
	bool jump;
	bool strengthJump;
	float sprint;
	// stance
	EStance stance;

	bool allowStrafe;
};

struct IActorMovementController : public IMovementController
{
	struct SStats
	{
		bool idle;
	};

	virtual void Reset() = 0;
	// return true if params was filled out, false if not
	virtual bool Update(float frameTime, SActorFrameMovementParams& params) = 0;
	virtual bool GetStats(SStats& stats) = 0;
	virtual void PostUpdate(float frameTime) = 0;
	virtual void Release() = 0;
	virtual void BindInputs(IAnimationGraphState*) {}
	virtual void Serialize(TSerialize& ser) = 0;
};

//FIXME:not sure to put this here
#define ZEROG_AREA_ID PHYS_FOREIGN_ID_USER+1

// Collision ray piercability to ignore leaves and other things
#define COLLISION_RAY_PIERCABILITY 10

enum EBonesID
{
	BONE_BIP01 = 0,
	BONE_SPINE,
	BONE_SPINE2,
	BONE_SPINE3,
	BONE_HEAD,
	BONE_EYE_R,
	BONE_EYE_L,
	BONE_WEAPON,
	BONE_FOOT_R,
	BONE_FOOT_L,
	BONE_ARM_R,
	BONE_ARM_L,
	BONE_CALF_R,
	BONE_CALF_L,
	BONE_ID_NUM
};

//For first person grabbing
enum EGrabbedCharacterType
{
	eGCT_UNKNOWN = 0,
	eGCT_HUMAN,
	eGCT_TROOPER,
	eGCT_ALIEN
};

//represent the key status of the actor
#define ACTION_JUMP						(1<<0)
#define ACTION_CROUCH					(1<<1)
#define ACTION_PRONE					(1<<2)
#define ACTION_SPRINT					(1<<3)
#define ACTION_LEANLEFT				(1<<4)
#define ACTION_LEANRIGHT			(1<<5)
#define ACTION_GYROSCOPE			(1<<6)
#define ACTION_ZEROGBACK			(1<<7)
#define ACTION_RELAXED				(1<<8)
#define ACTION_AIMING					(1<<9)
#define ACTION_STEALTH				(1<<10)
#define ACTION_MOVE						(1<<11)

struct IVehicle;
struct IInventory;
struct IInteractor;

struct SActorParams
{
	float maxGrabMass;
	float maxGrabVolume;
	bool nanoSuitActive;

	Vec3	viewPivot;
	float	viewDistance;
	float	viewHeightOffset;

	float	viewFoVScale;
	float   zoomFoV;
	float	viewSensitivity;

	Vec3	vLimitDir;
	float	vLimitRangeH;
	float	vLimitRangeV;
	float vLimitRangeVUp;
	float vLimitRangeVDown;

	float	weaponBobbingMultiplier;
	float	weaponInertiaMultiplier;

	float speedMultiplier;

	float timeImpulseRecover;

	SActorParams()
	{
		viewPivot.Set(0, 0, 0);
		viewDistance = 0;
		viewHeightOffset = 0;

		vLimitDir.Set(0, 0, 0);
		vLimitRangeH = 0.0f;
		vLimitRangeV = 0.0f;
		vLimitRangeVUp = 0.0f;
		vLimitRangeVDown = 0.0f;

		viewFoVScale = 1.0f;
		zoomFoV = 1.0f;
		viewSensitivity = 1.0f;

		weaponInertiaMultiplier = 1.0f;
		weaponBobbingMultiplier = 1.0f;

		speedMultiplier = 1.0f;

		timeImpulseRecover = 0.0f;
	}
};

struct SActorStats
{
	float inAir;//double purpose, tells if the actor is in air and for how long
	float onGround;//double purpose, tells if the actor is on ground and for how long

	float inWaterTimer;
	float relativeWaterLevel;
	float relativeBottomDepth;
	float worldWaterLevelDelta;
	bool swimJumping;

	float headUnderWaterTimer;
	float worldWaterLevel;

	float speed;
	float speedFlat;
	Vec3	angVelocity;

	Vec3 velocity;
	Vec3 velocityUnconstrained;

	Vec3 gravity;

	bool inZeroG;
	Vec3 zeroGUp;
	float nextZeroGCheck;

	int movementDir;
	float inFiring;
	float mass;
	Vec3 forceUpVector;
	Vec3 forceLookVector;
	float	bobCycle;

	float grabbedTimer;
	bool isGrabbed;
	bool isRagDoll;

	int8 inFreefallLast;
	CCoherentValue<int8> inFreefall;//1 == freefalling, 2 == parachute open
	CCoherentValue<bool> isHidden;
	CCoherentValue<bool> isShattered;
	CCoherentValue<bool> isFrozen;

	float timeFallen;

	EntityId mountedWeaponID;

	int groundMaterialIdx;

	SActorStats()
	{
		memset(this, 0, sizeof(SActorStats));
	}

};

struct SStanceInfo
{
	//dimensions
	float heightCollider;
	float heightPivot;

	bool useCapsule;

	Vec3 size;

	//view
	Vec3 viewOffset;
	Vec3 leanLeftViewOffset;
	Vec3 leanRightViewOffset;

	//weapon
	Vec3 weaponOffset;
	Vec3 leanLeftWeaponOffset;
	Vec3 leanRightWeaponOffset;

	//movement
	float normalSpeed;
	float maxSpeed;

	//misc
	char name[32];

	Vec3 modelOffset;

	inline Vec3	GetViewOffsetWithLean(float lean) const
	{
		if (lean < -0.01f)
		{
			float a = CLAMP(-lean, 0.0f, 1.0f);
			return viewOffset + a * (leanLeftViewOffset - viewOffset);
		}
		else if (lean > 0.01f)
		{
			float a = CLAMP(lean, 0.0f, 1.0f);
			return viewOffset + a * (leanRightViewOffset - viewOffset);
		}
		return viewOffset;
	}

	inline Vec3	GetWeaponOffsetWithLean(float lean) const
	{
		if (lean < -0.01f)
		{
			float a = CLAMP(-lean, 0.0f, 1.0f);
			return weaponOffset + a * (leanLeftWeaponOffset - weaponOffset);
		}
		else if (lean > 0.01f)
		{
			float a = CLAMP(lean, 0.0f, 1.0f);
			return weaponOffset + a * (leanRightWeaponOffset - weaponOffset);
		}
		return weaponOffset;
	}

	static inline Vec3	GetOffsetWithLean(float lean, const Vec3& sOffset, const Vec3& sLeftLean, const Vec3& sRightLean)
	{
		if (lean < -0.01f)
		{
			float a = CLAMP(-lean, 0.0f, 1.0f);
			return sOffset + a * (sLeftLean - sOffset);
		}
		else if (lean > 0.01f)
		{
			float a = CLAMP(lean, 0.0f, 1.0f);
			return sOffset + a * (sRightLean - sOffset);
		}
		return sOffset;
	}


	// Returns the size of the stance including the collider and the ground location.
	inline AABB	GetStanceBounds() const
	{
		AABB	aabb(-size, size);
		// Compensate for capsules.
		if (useCapsule)
		{
			aabb.min.z -= max(size.x, size.y);
			aabb.max.z += max(size.x, size.y);
		}
		// Lift from ground.
		aabb.min.z += heightCollider;
		aabb.max.z += heightCollider;
		// Include ground position
		aabb.Add(Vec3(0, 0, 0));
		// Make relative to the entity
		aabb.min.z -= heightPivot;
		aabb.max.z -= heightPivot;
		return aabb;
	}

	// Returns the size of the collider of the stance.
	inline AABB	GetColliderBounds() const
	{
		AABB	aabb(-size, size);
		// Compensate for capsules.
		if (useCapsule)
		{
			aabb.min.z -= max(size.x, size.y);
			aabb.max.z += max(size.x, size.y);
		}
		// Lift from ground.
		aabb.min.z += heightCollider;
		aabb.max.z += heightCollider;
		// Make relative to the entity
		aabb.min.z -= heightPivot;
		aabb.max.z -= heightPivot;
		return aabb;
	}

	SStanceInfo()
	{
		heightCollider = 0;
		heightPivot = 0;

		useCapsule = false;

		size.Set(0.5f, 0.5f, 0.5f);
		modelOffset.Set(0, 0, 0);
		viewOffset.Set(0, 0, 0);
		leanLeftViewOffset.Set(0, 0, 0);
		leanRightViewOffset.Set(0, 0, 0);
		weaponOffset.Set(0, 0, 0);
		leanLeftWeaponOffset.Set(0, 0, 0);
		leanRightWeaponOffset.Set(0, 0, 0);

		maxSpeed = 0.001f;

		strcpy(name, "null");
	}
};

#define IKLIMB_RIGHTHAND (1<<0)
#define IKLIMB_LEFTHAND (1<<1)

struct SIKLimb
{
	int flags;

	int rootBoneID;
	int endBoneID;
	int middleBoneID;//optional for custom IK

	Vec3 goalWPos;
	Vec3 currentWPos;

	Vec3 goalNormal;

	//limb local position in the animation
	Vec3 lAnimPos;
	Vec3 lAnimPosLast;

	char name[64];

	float recoverTime;//the time to go back to the animation pose
	float recoverTimeMax;

	float blendTime;
	float blendTimeMax;

	int blendID;

	int characterSlot;

	bool keepGoalPos;

	SIKLimb()
	{
		memset(this, 0, sizeof(SIKLimb));
	}

	void SetLimb(int slot, const char* limbName, int rootID, int midID, int endID, int iFlags)
	{
		rootBoneID = rootID;
		endBoneID = endID;
		middleBoneID = midID;

		strncpy(name, limbName, 64);
		name[63] = 0;

		blendID = -1;

		flags = iFlags;

		characterSlot = slot;
	}

	void SetWPos(IEntity* pOwner, const Vec3& pos, const Vec3& normal, float blend, float recover, int requestID);
	void Update(IEntity* pOwner, float frameTime);
};

struct SLinkStats
{
#define LINKED_FREELOOK (1<<0)
#define LINKED_VEHICLE (1<<1)

	//which entity are we linked to?
	EntityId linkID;
	//
	uint32 flags;
	//when freelook is on, these are used to update the player rotation
	Quat	baseQuatLinked;
	Quat	viewQuatLinked;

	//
	SLinkStats() : linkID(0), flags(0), baseQuatLinked(IDENTITY), viewQuatLinked(IDENTITY)
	{
	}

	SLinkStats(EntityId eID, uint32 flgs = 0) : linkID(eID), flags(flgs), baseQuatLinked(IDENTITY), viewQuatLinked(IDENTITY)
	{
	}

	ILINE void UnLink()
	{
		linkID = 0;
		flags = 0;
	}

	ILINE bool CanRotate()
	{
		return (!linkID || flags & LINKED_FREELOOK);
	}

	ILINE bool CanMove()
	{
		return (!linkID);
	}

	ILINE bool CanDoIK()
	{
		return (!linkID);
	}

	ILINE bool CanMoveCharacter()
	{
		return (!linkID);
	}

	ILINE IEntity* GetLinked()
	{
		if (!linkID)
			return NULL;
		else
		{
			IEntity* pEnt = gEnv->pEntitySystem->GetEntity(linkID);
			//if the entity doesnt exist anymore forget about it.
			if (!pEnt)
				UnLink();

			return pEnt;
		}
	}

	IVehicle* GetLinkedVehicle();

	void Serialize(TSerialize ser);
};

class CItem;
class CWeapon;

class CActor :
	public CGameObjectExtensionHelper<CActor, IActor>,
	public IGameObjectView,
	public IGameObjectProfileManager
{
public:
	struct ItemIdParam
	{
		ItemIdParam() : itemId(0) {};
		ItemIdParam(EntityId item) : itemId(item) {};
		void SerializeWith(TSerialize ser)
		{
			ser.Value("itemId", itemId, /* 'eid' */0x00656964);
		}
		EntityId itemId;
	};

	struct DropItemParams
	{
		DropItemParams() : itemId(0), impulseScale(1.0f), selectNext(true), byDeath(false) {};
		DropItemParams(EntityId item, float scale, bool next = true, bool death = false) : itemId(item), impulseScale(scale), selectNext(next), byDeath(death) {};

		void SerializeWith(TSerialize ser)
		{
			ser.Value("itemId", itemId, /* 'eid' */0x00656964);
			ser.Value("impulseScale", impulseScale, 'iScl');
			ser.Value("selectNext", selectNext, 'bool');
			ser.Value("byDeath", byDeath, 'bool');
		}

		float impulseScale;
		EntityId itemId;
		bool selectNext;
		bool byDeath;
	};

	struct ReviveParams
	{
		ReviveParams() : pos(0, 0, 0), rot(0, 0, 0, 1.0f), teamId(0) {};
		ReviveParams(const Vec3& p, const Ang3& a, int tId) : pos(p), rot(Quat::CreateRotationXYZ(a)), teamId(tId) {};
		ReviveParams(const Vec3& p, const Quat& q, int tId) : pos(p), rot(q), teamId(tId) {};
		void SerializeWith(TSerialize ser)
		{
			ser.Value("teamId", teamId, 'team');
			ser.Value("pos", pos, /* 'wrld' */ 0x77726C64);
			ser.Value("rot", rot, 'ori1');
		};

		int	 teamId;
		Vec3 pos;
		Quat rot;
	};

	struct ReviveInVehicleParams
	{
		ReviveInVehicleParams() : vehicleId(0), seatId(0), teamId(0) {};
		ReviveInVehicleParams(EntityId vId, int sId, int tId) : vehicleId(vId), seatId(sId), teamId(tId) {};
		void SerializeWith(TSerialize ser)
		{
			ser.Value("teamId", teamId, 'team');
			ser.Value("vehicleId", vehicleId, /* 'eid' */0x00656964);
			ser.Value("seatId", seatId, 'seat');
		};

		int	 teamId;
		EntityId vehicleId;
		int seatId;
	};

	struct KillParams
	{
		KillParams() {};
		KillParams(EntityId _shooterId, int _weaponClassId, float _damage, int _material, int _hit_type, const Vec3& _impulse)
			: shooterId(_shooterId),
			weaponClassId(_weaponClassId),
			damage(_damage),
			material(_material),
			hit_type(_hit_type),
			impulse(_impulse)
		{};

		EntityId shooterId;
		int weaponClassId;
		float damage;
		int material;
		int hit_type;
		Vec3 impulse;

		void SerializeWith(TSerialize ser)
		{
			ser.Value("shooterId", shooterId, /* 'eid' */0x00656964);
			ser.Value("weaponClassId", weaponClassId, /* 'ui16' */ 0x75693136);
			ser.Value("damage", damage, /* 'dmg' */ 0x00646D67);
			ser.Value("material", material, /* 'mat' */ 0x006D6174);
			ser.Value("hit_type", hit_type, /* 'hTyp' */ 0x68547970);
			ser.Value("impulse", impulse, /* 'kImp' */ 0x6B496D70);
		};
	};
	struct MoveParams
	{
		MoveParams() {};
		MoveParams(const Vec3& p, const Quat& q) : pos(p), rot(q) {};
		void SerializeWith(TSerialize ser)
		{
			ser.Value("pos", pos, /* 'wrld' */ 0x77726C64);
			ser.Value("rot", rot, 'ori1');
		}
		Vec3 pos;
		Quat rot;
	};
	struct AmmoParams
	{
		AmmoParams() {};
		AmmoParams(const char* name, int amount) : ammo(name), count(amount) {};
		void SerializeWith(TSerialize ser)
		{
			ser.Value("ammo", ammo);
			ser.Value("amount", count);
		}
		string ammo;
		int	count;
	};

	struct PickItemParams
	{
		PickItemParams() : itemId(0), select(false), sound(false) {};
		PickItemParams(EntityId item, bool slct, bool snd) : itemId(item), select(slct), sound(snd) {};
		void SerializeWith(TSerialize ser)
		{
			ser.Value("itemId", itemId, /* 'eid' */0x00656964);
			ser.Value("select", select, 'bool');
			ser.Value("sound", sound, 'bool');
		}

		EntityId	itemId;
		bool			select;
		bool			sound;
	};

	struct SelectItemParams
	{
		SelectItemParams() {};
		SelectItemParams(const string& _itemName) : itemName(_itemName) {};
		void SerializeWith(TSerialize ser)
		{
			ser.Value("itemName", itemName);
		}

		string	itemName;
	};

	struct SetSpectatorModeParams
	{
		SetSpectatorModeParams() : mode(0), targetId(0) {};
		SetSpectatorModeParams(uint8 _mode, EntityId _target) : mode(_mode), targetId(_target) {};
		uint8 mode;
		EntityId targetId;
		void SerializeWith(TSerialize ser)
		{
			ser.Value("mode", mode, 'spec');
			ser.Value("target", targetId, /* 'eid' */0x00656964);
		}
	};
	struct SetSpectatorHealthParams
	{
		SetSpectatorHealthParams() : health(0) {};
		SetSpectatorHealthParams(int _health) : health(_health) {};
		int health;
		void SerializeWith(TSerialize ser)
		{
			ser.Value("health", health, /* 'i8' */ 0x00006938);
		}
	};

	struct NoParams
	{
		void SerializeWith(TSerialize ser) {};
	};

	enum EActorSpectatorMode
	{
		eASM_None = 0,												// normal, non-spectating

		eASM_FirstMPMode,
		eASM_Fixed = eASM_FirstMPMode,				// fixed position camera
		eASM_Free,														// free roaming, no collisions
		eASM_Follow,													// follows an entity in 3rd person
		eASM_LastMPMode = eASM_Follow,

		eASM_Cutscene,												// HUDInterfaceEffects.cpp sets this
	};

	enum class ReasonForRevive
	{
		NONE, FROM_INIT, START_SPECTATING, SPAWN, SCRIPT_BIND
	};

	DECLARE_SERVER_RMI_NOATTACH_FAST(SvRequestDropItem, DropItemParams, eNRT_ReliableOrdered);
	DECLARE_SERVER_RMI_NOATTACH_FAST(SvRequestPickUpItem, ItemIdParam, eNRT_ReliableOrdered);
	DECLARE_SERVER_RMI_NOATTACH_FAST(SvRequestUseItem, ItemIdParam, eNRT_ReliableOrdered);
	// cannot be _FAST - see comment on InvokeRMIWithDependentObject
	DECLARE_CLIENT_RMI_NOATTACH(ClPickUp, PickItemParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH(ClClearInventory, NoParams, eNRT_ReliableOrdered);

	DECLARE_CLIENT_RMI_NOATTACH(ClSelectItemByName, SelectItemParams, eNRT_ReliableOrdered);

	DECLARE_CLIENT_RMI_NOATTACH_FAST(ClDrop, DropItemParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH_FAST(ClStartUse, ItemIdParam, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH_FAST(ClStopUse, ItemIdParam, eNRT_ReliableOrdered);

	DECLARE_CLIENT_RMI_PREATTACH(ClSetSpectatorMode, SetSpectatorModeParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_PREATTACH(ClSetSpectatorHealth, SetSpectatorHealthParams, eNRT_ReliableOrdered);

	DECLARE_CLIENT_RMI_PREATTACH(ClRevive, ReviveParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_PREATTACH(ClReviveInVehicle, ReviveInVehicleParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_POSTATTACH(ClSimpleKill, NoParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH_FAST(ClKill, KillParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH_FAST(ClMoveTo, MoveParams, eNRT_ReliableOrdered);

	DECLARE_CLIENT_RMI_NOATTACH(ClSetAmmo, AmmoParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH(ClAddAmmo, AmmoParams, eNRT_ReliableOrdered);

	CItem* GetItem(EntityId itemId) const;
	CItem* GetItemByClass(IEntityClass* pClass) const;
	CWeapon* GetWeapon(EntityId itemId) const;
	CWeapon* GetWeaponByClass(IEntityClass* pClass) const;

	virtual void SelectNextItem(int direction, bool keepHistory, const char* category = 0);
	virtual void HolsterItem(bool holster);
	virtual void SelectLastItem(bool keepHistory, bool forceNext = false);
	virtual void SelectItemByName(const char* name, bool keepHistory);
	virtual void SelectItem(EntityId itemId, bool keepHistory);

	virtual bool UseItem(EntityId itemId);
	virtual bool PickUpItem(EntityId itemId, bool sound);
	virtual bool DropItem(EntityId itemId, float impulseScale = 1.0f, bool selectNext = true, bool byDeath = false);
	virtual void DropAttachedItems();

	virtual void NetReviveAt(const Vec3& pos, const Quat& rot, int teamId);
	virtual void NetReviveInVehicle(EntityId vehicleId, int seatId, int teamId);
	virtual void NetSimpleKill();
	virtual void NetKill(EntityId shooterId, uint16 weaponClassId, int damage, int material, int hit_type);

	virtual void SetSleepTimer(float timer);
	Vec3 GetWeaponOffsetWithLean(EStance stance, float lean, const Vec3& eyeOffset);

	virtual bool CanRagDollize() const;

public:
	CActor();
	virtual ~CActor();

	// IActor
	virtual void ProcessEvent(SEntityEvent& event);
	virtual void Release() { delete this; };
	virtual void ResetAnimGraph();
	virtual void FullSerialize(TSerialize ser);
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	virtual void PostSerialize();
	virtual void SerializeSpawnInfo(TSerialize ser);
	virtual ISerializableInfoPtr GetSpawnInfo();
	virtual void SetChannelId(uint16 id);
	virtual void  SerializeLevelToLevel(TSerialize& ser);
	virtual IInventory* GetInventory() const;

	bool IsClient() const override;
	bool IsRemote() const;

	virtual IMaterial* GetReplacementMaterial() { return m_pReplacementMaterial; };

	virtual bool Init(IGameObject* pGameObject);
	virtual void InitClient(int channelId);
	virtual void PostInit(IGameObject* pGameObject);
	virtual void PostInitClient(int channelId) {};
	virtual void Update(SEntityUpdateContext& ctx, int updateSlot);
	virtual void UpdateView(SViewParams& viewParams) {};
	virtual void PostUpdateView(SViewParams& viewParams) {};

	virtual void InitLocalPlayer() {};

	virtual void InitiateCombat();
	virtual void ExtendCombat();

	virtual void SetIKPos(const char* pLimbName, const Vec3& goalPos, int priority);
	void SetIKPos(const char* pLimbName, const Vec3& goalPos, int priority, float blendIn, float recover);
	void ClearIKPosBlending(const char* pLimbName);

	virtual void HandleEvent(const SGameObjectEvent& event);
	virtual void PostUpdate(float frameTime);
	virtual void PostRemoteSpawn() {};

	virtual bool IsThirdPerson() const { return true; };
	virtual void ToggleThirdPerson() {}


	virtual void RequestFacialExpression(const char* pExpressionName /* = NULL */);
	virtual void PrecacheFacialExpression(const char* pExpressionName);

	virtual void NotifyInventoryAmmoChange(IEntityClass* pAmmoClass, int amount);
	virtual EntityId	GetGrabbedEntityId() const { return 0; }

	virtual void HideAllAttachments(bool isHiding);

	// ~IActor

	// IGameObjectProfileManager
	virtual bool SetAspectProfile(EEntityAspects aspect, uint8 profile);
	virtual uint8 GetDefaultProfile(EEntityAspects aspect) { return aspect == eEA_Physics ? eAP_NotPhysicalized : 0; }
	// ~IGameObjectProfileManager

	// IActionListener
	virtual void OnAction(const ActionId& actionId, int activationMode, float value);
	// ~IActionListener

	virtual void ProfileChanged(uint8 newProfile);

	//------------------------------------------------------------------------
	float GetAirControl() const { return m_airControl; };
	float GetAirResistance() const { return m_airResistance; };
	float GetTimeImpulseRecover() const { return m_timeImpulseRecover; };

	virtual void SetViewRotation(const Quat& rotation) {};
	virtual Quat GetViewRotation() const { return GetEntity()->GetRotation(); };
	virtual void EnableTimeDemo(bool bTimeDemo) {};

	// offset to add to the computed camera angles every frame
	virtual void SetViewAngleOffset(const Vec3& offset) {};
	virtual Vec3 GetViewAngleOffset() { return Vec3(0, 0, 0); };

	//------------------------------------------------------------------------
	virtual void Revive(ReasonForRevive reason = ReasonForRevive::NONE);
	virtual void Reset(bool toGame) {};
	//physicalization
	virtual void Physicalize(EStance stance = STANCE_NULL);
	virtual void PostPhysicalize();
	virtual void RagDollize(bool fallAndPlay);
	//
	virtual int IsGod() { return 0; }

	virtual void SetSpectatorMode(uint8 mode, EntityId targetId) {};
	virtual uint8 GetSpectatorMode() const { return 0; };
	virtual void SetSpectatorTarget(EntityId targetId) {};
	virtual EntityId GetSpectatorTarget() const { return 0; };
	virtual void SetDeathCamTarget(EntityId targetId) {};
	virtual EntityId GetDeathCamTarget() const { return 0; };
	virtual void SetSpectatorHealth(int health) {};
	virtual int GetSpectatorHealth() const { return 0; };

	//get actor status
	virtual SActorStats* GetActorStats() { return 0; };
	virtual const SActorStats* GetActorStats() const { return 0; };
	virtual SActorParams* GetActorParams() { return 0; };

	virtual void SetStats(SmartScriptTable& rTable);
	virtual void UpdateScriptStats(SmartScriptTable& rTable);
	virtual ICharacterInstance* GetFPArms(int i) const { return GetEntity()->GetCharacter(3 + i); };
	//set/get actor params
	virtual void SetParams(SmartScriptTable& rTable, bool resetFirst = false);
	virtual bool GetParams(SmartScriptTable& rTable) { return false; };
	//
	virtual void Freeze(bool freeze) {};
	virtual void Fall(Vec3 hitPos = Vec3(0, 0, 0), bool forceFall = false, float time = 0.0f);
	//throw away the actors helmet (if available) [Jan Müller]
	virtual bool LooseHelmet(Vec3 hitDir = Vec3(0, 0, 0), Vec3 hitPos = Vec3(0, 0, 0), bool simulate = false);
	virtual void GoLimp();
	virtual void StandUp();
	virtual void NotifyLeaveFallAndPlay();
	virtual bool IsFallen() const;
	virtual int  GetFallenTime() const;
	//
	virtual IEntity* LinkToVehicle(EntityId vehicleId);
	virtual IEntity* LinkToVehicleRemotely(EntityId vehicleId);
	virtual void LinkToMountedWeapon(EntityId weaponId) {};
	virtual IEntity* LinkToEntity(EntityId entityId, bool bKeepTransformOnDetach = true);

	ILINE virtual IEntity* GetLinkedEntity() const
	{
		return m_linkStats.GetLinked();
	}

	ILINE virtual IVehicle* GetLinkedVehicle() const
	{
		return m_linkStats.GetLinkedVehicle();
	}

	EntityId m_lastVehicleId;

	virtual void SetViewInVehicle(Quat viewRotation) {};

	virtual void SupressViewBlending() {};

	ILINE Vec3 GetLBodyCenter()
	{
		const SStanceInfo* pStance(GetStanceInfo(GetStance()));
		return Vec3(0, 0, (pStance->viewOffset.z - pStance->heightPivot) * 0.5f);
	}

	ILINE Vec3 GetWBodyCenter()
	{
		return GetEntity()->GetWorldTM() * GetLBodyCenter();
	}

	//for animations
	virtual void PlayAction(const char* action, const char* extension, bool looping = false) {};
	//
	virtual void SetMovementTarget(const Vec3& position, const Vec3& looktarget, const Vec3& up, float speed) {};
	//
	virtual void CreateScriptEvent(const char* event, float value, const char* str = NULL);
	virtual bool CreateCodeEvent(SmartScriptTable& rTable);
	virtual void AnimationEvent(ICharacterInstance* pCharacter, const AnimEventInstance& event);
	//
	virtual void CameraShake(float angle, float shift, float duration, float frequency, Vec3 pos, int ID, const char* source = "") {};
	//
	virtual void VectorToLocal(Vec3& v) {};
	//
	virtual void SetAngles(const Ang3& angles) {};
	virtual Ang3 GetAngles() { return Ang3(0, 0, 0); };
	virtual void AddAngularImpulse(const Ang3& angular, float deceleration = 0.0f, float duration = 0.0f) {}
	//
	virtual void SetViewLimits(Vec3 dir, float rangeH, float rangeV) {};
	virtual void SetZoomSpeedMultiplier(float m);
	virtual float GetZoomSpeedMultiplier() const;
	virtual void SetHealth(int health);
	virtual void DamageInfo(EntityId shooterID, EntityId weaponID, float damage, const char* damageType);
	virtual IAnimatedCharacter* GetAnimatedCharacter() { return m_pAnimatedCharacter; }
	void SetMaxHealth(int maxHealth);
	virtual int32 GetHealth() const { return int32(m_health); }
	virtual int32 GetMaxHealth() const { return int32(m_maxHealth); }
	virtual int32 GetArmor() const { return 0; }
	virtual int32 GetMaxArmor() const { return 0; }
	virtual void Kill();

	void ResetHelmetAttachment();

	virtual float GetFrozenAmount(bool stages = false) const { return m_frozenAmount; }
	void SetFrozenAmount(float amount);

	bool IsFrozen();
	virtual void AddFrost(float frost);

	virtual void BindInputs(IAnimationGraphState* pAGState);
	//marcok: GC workaround
	virtual bool IsSwimming() { return false; }
	virtual bool ShouldSwim() { return false; };

	virtual bool IsSprinting() { return false; }
	virtual bool CanFire() { return true; }

	//stances
	ILINE virtual EStance GetStance() const
	{
		/*
				if(!m_pAnimatedCharacter)
					return STANCE_NULL;
				int stance = m_pAnimatedCharacter?m_pAnimatedCharacter->GetCurrentStance():STANCE_NULL;

				if (stance < 0 || stance > STANCE_LAST)
					return STANCE_NULL;
				return (EStance)stance;
		*/
		return m_stance;
	}
	ILINE const SStanceInfo* GetStanceInfo(EStance stance) const
	{
		if (stance < 0 || stance > STANCE_LAST)
		{
			return &m_defaultStance;
		}
		return &m_stances[stance];
	}
	virtual void	SetupStance(EStance stance, SStanceInfo* info);
	//

	// forces the animation graph to select a state
	void QueueAnimationState(const char* state);
	void ChangeAnimGraph(const char* graph, int layer);
	virtual bool SetAnimationInput(const char* inputID, const char* value)
	{
		// Handle action and signal inputs via AIproxy, since the AI system and
		// the AI agent behavior depend on those inputs.
		if (IEntity* pEntity = GetEntity())
			if (IAIObject* pAI = pEntity->GetAI())
				if (IUnknownProxy* pProxy = pAI->GetProxy())
				{
					if (pProxy->IsEnabled())
					{
						bool	bSignal = strcmp(inputID, "Signal") == 0;
						bool	bAction = strcmp(inputID, "Action") == 0;
						if (bSignal)
						{
							return pProxy->SetAGInput(AIAG_SIGNAL, value);
						}
						else if (bAction)
						{
							// Dejan: actions should not go through the ai proxy anymore!
							/*
							if(__stricmp(value, "idle") == 0)
								return pProxy->ResetAGInput( AIAG_ACTION );
							else
							{
								return pProxy->SetAGInput( AIAG_ACTION, value );
							}
							*/
						}
					}
				}

		if (IAnimationGraphState* pState = GetAnimationGraphState())
		{
			pState->SetInput(pState->GetInputId(inputID), value);
			return true;
		}

		return false;
	}

	//
	virtual int GetBoneID(int ID, int slot = 0) const;
	virtual Vec3 GetLocalEyePos(int slot = 0) const;
	virtual Vec3 GetLocalEyePos2(int slot = 0) const;

	ILINE /* virtual */ bool InZeroG() const
	{
		if (const SActorStats* pActorStats = GetActorStats())
			return pActorStats->inZeroG;
		return false;
	}

	void UpdateZeroG(float frameTime);

	//
	virtual void ProcessBonesRotation(ICharacterInstance* pCharacter, float frameTime);

	virtual void OnPhysicsPreStep(float frameTime) {};

	//grabbing
	virtual IGrabHandler* CreateGrabHanlder();
	virtual void UpdateGrab(float frameTime);

	virtual bool CheckInventoryRestrictions(const char* itemClassName);
	virtual bool CheckVirtualInventoryRestrictions(const std::vector<string>& inventory, const char* itemClassName);

	virtual bool CanPickUpObject(IEntity* obj, float& heavyness, float& volume);
	virtual bool CanPickUpObject(float mass, float volume);
	virtual float GetActorStrength() const;

	//
	virtual void ProcessIKLimbs(ICharacterInstance* pCharacter, float frameTime);

	//IK limbs
	int GetIKLimbIndex(const char* limbName);
	ILINE SIKLimb* GetIKLimb(int limbIndex)
	{
		return &m_IKLimbs[limbIndex];
	}
	void CreateIKLimb(int characterSlot, const char* limbName, const char* rootBone, const char* midBone, const char* endBone, int flags = 0);

	//
	virtual IMovementController* GetMovementController()
	{
		return m_pMovementController;
	}

	//stances
	virtual void	SetStance(EStance stance);
	virtual void	StanceChanged(EStance lastStance, EStance newStance) {};
	virtual bool	TrySetStance(EStance stance); // Shared between humans and aliens.
	//

	IAnimationGraphState* GetAnimationGraphState();
	IAnimatedCharacter* GetAnimatedCharacter() const { return m_pAnimatedCharacter; };
	void SetFacialAlertnessLevel(int alertness);

	//weapons
	virtual IItem* GetCurrentItem(bool includeVehicle = false) const;
	CWeapon* GetCurrentWeapon(bool includeVehicle/*=false*/) const;
	EntityId GetCurrentItemId(bool includeVehicle = false) const;
	virtual IItem* GetHolsteredItem() const;
	EntityId GetHolsteredItemId() const;

	IInteractor* GetInteractor() const;

	//Net
	EntityId NetGetCurrentItem() const;
	void NetSetCurrentItem(EntityId id, bool hasWeapon);

	//AI
	Vec3 GetAIAttentionPos();

	virtual void UpdateFootSteps(float frameTime);
	CScreenEffects* GetScreenEffects() { return m_screenEffects; };
	CWeaponAttachmentManager* GetWeaponAttachmentManager() { return m_pWeaponAM; }
	void InitActorAttachments();

	virtual void SwitchDemoModeSpectator(bool activate) {};	//this is a player only function

	//misc
	virtual void ReplaceMaterial(const char* strMaterial);
	virtual void SendMusicLogicEvent(EMusicLogicEvents event) {};

	//Crytek ugly: replace by real class system
	//CryMP: Here you go
	static ActorType GetActorClassType() { return ActorType::CActor; }
	virtual ActorType GetActorClass() const { return GetActorClassType(); }

	//For sleeping bullets
	virtual bool  CanSleep() { return false; }

	//Grabbing
	virtual int	GetActorSpecies() { return eGCT_UNKNOWN; }

	virtual void SetAnimTentacleParams(pe_params_rope& rope, float animBlend) {};

	virtual bool IsCloaked() const { return false; }

	virtual void DumpActorInfo();

	virtual bool IsAlien() { return false; }


protected:

	virtual void SetMaterialRecursive(ICharacterInstance* charInst, bool undo, IMaterial* newMat = 0);

	virtual void UpdateAnimGraph(IAnimationGraphState* pState);

	//movement
	virtual IActorMovementController* CreateMovementController() = 0;
	//

	virtual Vec3 GetModelOffset() const { return GetStanceInfo(GetStance())->modelOffset; }

private:
	mutable IInventory* m_pInventory;
	mutable IInteractor* m_pInteractor;
	void ClearExtensionCache();

protected:
	void RequestServerResync()
	{
		if (!IsClient())
			GetGameObject()->RequestRemoteUpdate(eEA_Physics | eEA_GameClientDynamic | eEA_GameServerDynamic | eEA_GameClientStatic | eEA_GameServerStatic);
	}

	void GetActorMemoryStatistics(ICrySizer* pSizer);

	//
	typedef std::vector<SIKLimb> TIKLimbs;

	//
	virtual void SetActorModel();
	virtual bool UpdateStance();
	virtual void OnCloaked(bool cloaked) {};

	mutable int16 m_boneIDs[BONE_ID_NUM];

	bool	m_isClient;
	float m_health;
	int32 m_maxHealth;


	EStance m_stance;
	EStance m_desiredStance;

	static SStanceInfo m_defaultStance;
	SStanceInfo m_stances[STANCE_LAST];

	float m_zoomSpeedMultiplier;

	SmartScriptTable m_actorStats;

	IAnimatedCharacter* m_pAnimatedCharacter;
	IActorMovementController* m_pMovementController;

	static IItemSystem* m_pItemSystem;
	static IGameFramework* m_pGameFramework;
	static IGameplayRecorder* m_pGameplayRecorder;

	//
	IAnimationGraph::InputID m_inputHealth;
	IAnimationGraph::InputID m_inputStance;
	IAnimationGraph::InputID m_inputFiring;
	IAnimationGraph::InputID m_inputWaterLevel;

	IGrabHandler* m_pGrabHandler;

	mutable SLinkStats m_linkStats;

	TIKLimbs m_IKLimbs;

	float m_frozenAmount; // internal amount. used to leave authority over frozen state at game

	// ScreenEffects-related variables
	CScreenEffects* m_screenEffects;

	// Weapon Attachment manager
	CWeaponAttachmentManager* m_pWeaponAM;

	uint8 m_currentPhysProfile;

	EBonesID m_currentFootID;

	float m_fallAndPlayTimer;

	float m_combatTimer;
	bool m_enterCombat;
	bool m_inCombat;
	bool m_rightFoot;
	bool m_bHasHUD;

	float m_airControl;
	float m_airResistance;
	float m_timeImpulseRecover;

	EntityId m_netLastSelectablePickedUp;

	//helmet serialization
	EntityId	m_lostHelmet, m_serializeLostHelmet;
	string		m_lostHelmetObj, m_serializelostHelmetObj;
	string		m_lostHelmetMaterial;
	string		m_lostHelmetPos;

	_smart_ptr<IMaterial> m_pReplacementMaterial;

	//
	std::map< ICharacterInstance*, _smart_ptr<IMaterial> > m_testOldMats;
	std::map< IAttachmentObject*, _smart_ptr<IMaterial> > m_attchObjMats;
	//std::map< EntityId, IMaterial* > m_wepAttchMats;

	float			m_sleepTimer, m_sleepTimerOrg;

	int				m_teamId;
	EntityId	m_lastItemId;

public:
	// Can occur only when we're not zooming out
	int m_autoZoomInID;
	// Can occur at any time
	int m_autoZoomOutID;
	// Can occur at any time
	int m_saturationID;

	int m_hitReactionID;

	uint8 GetPhysicsProfile() const
	{
		return m_currentPhysProfile;
	}

	void NetSetTeamId(const int teamId)
	{
		m_teamId = teamId;
	}

//////////////////////////////////////////////////////////////////////////////////
//CryMP
//////////////////////////////////////////////////////////////////////////////////
public:

	enum class ObjectHoldType
	{
		None,
		Actor,
		Projectile,
		OneHanded,
		TwoHanded,
		Vehicle
	};

	bool IsPlayerClass() const
	{
		return m_isPlayerClass;
	}

	bool ShouldUseMPParams();

	//First Person Spectators
	virtual bool IsFpSpectator() const { return false; }
	virtual bool IsFpSpectatorTarget() const { return false; }
	virtual bool IsTpSpectatorTarget() const { return false; }

	EntityId GetHeldObjectId() const { return m_heldObjectId; }
	ObjectHoldType GetHeldObjectType() const { return m_heldObjectType; }

	void SetHeldObjectId(EntityId objectId, ObjectHoldType type = ObjectHoldType::None);

	void SaveNick(const std::string_view& name);

	bool IsGhostPit();

	IAttachment* CreateBoneAttachment(int characterSlot, const char* boneName, const char* attachmentName,
		const Vec3& offsetPosition = Vec3(ZERO),
		const Quat& offsetRotation = Quat(IDENTITY));

	void HideAllAttachments(int characterSlot, bool hide, bool hideShadow);
	void HideAttachment(int characterSlot, const char* attachmentName, bool hide, bool hideShadow);

	void DrawSlot(int nSlot, int nEnable);
	bool IsFp3pModel() const;

	void CacheIKLimbs();
	void CacheFileModels();
	void CallCreateAttachments();

	std::string GetCleanNick()
	{
		if (m_playerNameClean.empty())
		{
			SaveNick(GetEntity()->GetName());
		}
		return m_playerNameClean;
	}

	CActor* GetActor(EntityId actorId)
	{
		return static_cast<CActor*>(m_pGameFramework->GetIActorSystem()->GetActor(actorId));
	}

	CActor* GetActorByChannel(uint16 actorId)
	{
		return static_cast<CActor*>(m_pGameFramework->GetIActorSystem()->GetActorByChannelId(actorId));
	}

	bool m_hideActor = false;
	bool m_hideMaster = false;

	void SetFileModel(std::string_view model)
	{
		m_fileModel = model;
	}
	void SetFrozenModel(std::string_view model)
	{
		m_frozenModel = model;
	}
	void SetFpItemHandsModel(std::string_view model)
	{
		m_fpItemHandsModel = model;
	}

	void SetVehicleRelinkUpdateId(EntityId vehicleId)
	{
		m_vehicleRelinkUpdateId = vehicleId;
	}
	EntityId GetVehicleRelinkUpdateId() const
	{
		return m_vehicleRelinkUpdateId;
	}

	enum class ObjectEvent
	{
		GRAB,
		THROW
	};

	virtual void OnObjectEvent(ObjectEvent evnt, const EntityId objectId) {};

private:

	ObjectHoldType m_heldObjectType = ObjectHoldType::None;
	EntityId m_heldObjectId = (EntityId)0;

	struct IKLimb
	{
		int slot = 0;
		std::string limbName;
		std::string rootBone;
		std::string midBone;
		std::string endBone;
		int flags = 0;
	};

	std::vector<IKLimb> m_cachedIKLimbs;

	std::string m_playerNameClean;
	bool m_isPlayerClass = false;

	std::string m_currFileModel;
	std::string m_fileModel;
	std::string m_currfpItemHandsModel;
	std::string m_frozenModel;
	std::string m_fpItemHandsModel;

	EntityId m_vehicleRelinkUpdateId = 0;
	bool m_netItemReceived = false;

	enum EntitySlot
	{
		CHARACTER,
		FROZEN,
		UNUSED,
		FPITEMHANDS,
		FPITEMHANDS_SECONDARY
	};

	enum Timers			          //Converted from BasicActor.lua
	{
		DEAD = 1,                 // DEAD_TIMER (lua)
		DEADANIM = 2,             // DEADANIM_TIMER
		THROWOBJECT = 3,          // THROWOBJECT_TIMER
		EQUIP = 4,                // EQUIP_TIMER
		ACTOREFFECTS = 5,         // ACTOREFFECTS_TIMER
		BLEED = 9,                // BLEED_TIMER
		PAIN = 10,                // PAIN_TIMER
		NANOSUIT_MODE = 11,       // NANOSUIT_MODE_TIMER
		NANOSUIT_CLOAK = 12,      // NANOSUIT_CLOAK_TIMER
		NANOSUIT_UNCLOAK = 13,    // NANOSUIT_UNCLOAK_TIMER
		DEATH_REIMPULSE = 14,     // DEATH_REIMPULSE_TIMER
		COLLISION = 15,           // COLLISION_TIMER
		UNRAGDOLL = 16,           // UNRAGDOLL_TIMER 
		HOSTAGE_COWER = 17,       // HOSTAGE_COWER_TIMER
		SWITCH_WEAPON = 18,       // SWITCH_WEAPON_TIMER
		BLOOD_POOL = 19,          // BLOOD_POOL_TIMER
		ITEM_RESELECT = 20         // new	
	};


public:

	template<class... Args>
	void DrawLog(const char* msg, Args... args)
	{
		IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
		IActorIteratorPtr pActorIterator = pActorSystem->CreateActorIterator();
		IActor* pActor = nullptr;
		int k = 1;

		while (pActor = pActorIterator->Next())
		{
			if (pActor == this)
			{
				break;
			}
			++k;
		}
		f32 fColor[4] = { 1,1,0,1 };
		f32 g_YLine = 60.0f + (k * 20.f);
		gEnv->pRenderer->Draw2dLabel(1, g_YLine, 1.3f, fColor, false, msg, args...);
	}

};

#endif //__Actor_H__
