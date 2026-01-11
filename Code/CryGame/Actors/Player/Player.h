/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Implements the player.

 -------------------------------------------------------------------------
  History:
  - 29:9:2004: Created by Filippo De Luca

*************************************************************************/
#ifndef __PLAYER_H__
#define __PLAYER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "../Actor.h"
#include "NanoSuit.h"
#include "CryCommon/CryAction/IActionMapManager.h"
#include "CryCommon/CryAction/IViewSystem.h"
#include "PlayerView.h"


class CPlayerMovement;
class CPlayerRotation;
struct IPlayerInput;
class CHUD;
class CVehicleClient;
struct IDebugHistory;
struct IDebugHistoryManager;

enum class SpectatorTargetType
{
	NONE,
	FIRST_PERSON,
	THIRD_PERSON
};

struct SPlayerStats : public SActorStats
{
	float onGroundWBoots;

	float inMovement;//for how long the player is moving
	float inRest;//for how long the player is standing still

	bool landed;
	bool jumped;
	bool wasStuck;
	bool wasFlying;
	float stuckTimeout;

	float jumpLock;

	float leanAmount;

	float shakeAmount;

	Vec3 physCamOffset;
	float heightPivot;

	// falling things
	float fallSpeed;
	float downwardsImpactVelocity;

	bool isFiring;

	bool isThirdPerson;

	bool isWalkingOnWater;

	CCoherentValue<uint8> followCharacterHead;
	bool isStandingUp;

	CCoherentValue<uint8> firstPersonBody;

	//useful for smoothing out the view
	float smoothViewZ;
	uint8 smoothZType;

	bool animationControlled;

	//cheating stuff
	uint8 flyMode;//0 no fly, 1 fly mode, 2 fly mode + noclip
	EntityId spectatorTarget;	// which player we are watching
	EntityId deathCamTarget;
	int spectatorHealth;
	int spectatorZoom;
	uint8 spectatorMode;
	// 0=off, 
	// 1=on,no-move,
	// 2=on,move
	// 3=on,nomove,a fake spectatormode used by FlowPlayerStagingNode.cpp
	// SNH: added the CActor::EActorSpectatorMode enum for the above and more...

	Vec3 upVector;
	Vec3 groundNormal;

	Vec3 FPWeaponPos;
	Ang3 FPWeaponAngles;
	Vec3 FPWeaponPosOffset;
	Ang3 FPWeaponAnglesOffset;

	Vec3 FPSecWeaponPos;
	Ang3 FPSecWeaponAngles;
	Vec3 FPSecWeaponPosOffset;
	Ang3 FPSecWeaponAnglesOffset;

	bool FPWeaponSwayOn;

	Vec3 gBootsSpotNormal;

	//
	Ang3 angularVel;

	Ang3 angularImpulse;
	float angularImpulseDeceleration;
	float angularImpulseTime;
	float angularImpulseTimeMax;

	//
	float thrusters;
	float zgDashTimer;
	Vec3 zgDashWorldDir;

	//ladder
	CCoherentValue<bool> isOnLadder;
	bool isExitingLadder;

	Vec3	ladderTop;
	Vec3	ladderBottom;
	Vec3  ladderOrientation;
	Vec3  ladderUpDir;
	int		ladderAction;		// actually a ELadderActionType

	Quat  playerRotation;

	bool	bSprinting;
	bool  bIgnoreSprinting;
	bool  bLookingAtFriendlyAI;

	//Force crouch
	float forceCrouchTime;

	int  grabbedHeavyEntity; //Player is grabbing two handed object(1) or NPC(2)

	//CryMP FP Spec
	bool fpSpectator;
	SpectatorTargetType spectatorTargetType;

	SPlayerStats()
	{
		memset(this, 0, sizeof(SPlayerStats)); // This will zero everything, fine.
		new (this) SActorStats(); // But this will set certain Vec3 to QNAN, due to the new default constructors.

		mass = 80.0f;

		inAir = 1.0f;

		firstPersonBody = 2;

		upVector.Set(0, 0, 1);

		zeroGUp.Set(0, 0, 1);

		velocity.zero();
		velocityUnconstrained.zero();
		gravity.zero();
		forceUpVector.zero();
		forceLookVector.zero();

		thrusters = 0.0f;
		bSprinting = false;
		bIgnoreSprinting = false;

		groundMaterialIdx = -1;

		inFreefall = -1;

		fallSpeed = 0.0f;
		downwardsImpactVelocity = 0.0f;

		forceCrouchTime = 0;
		bLookingAtFriendlyAI = false;

		inWaterTimer = -1000.0f;
		relativeWaterLevel = 10.0f;
		worldWaterLevel = 0.0f;
		worldWaterLevelDelta = 0.0f;
		swimJumping = false;

		spectatorMode = 0;
		spectatorTarget = 0;
		deathCamTarget = 0;
		spectatorZoom = 2;

		grabbedHeavyEntity = 0;

		FPWeaponSwayOn = false;

		//CryMP FP spec
		fpSpectator = false,
		spectatorTargetType = SpectatorTargetType::NONE;
	}

	void Serialize(TSerialize ser, unsigned aspects);
};

struct SPlayerParams : public SActorParams
{
	float sprintMultiplier;
	float strafeMultiplier;
	float backwardMultiplier;
	float grabMultiplier;
	float afterburnerMultiplier;

	float inertia;
	float inertiaAccel;

	float jumpHeight;

	float slopeSlowdown;//cant find a better name for this

	float leanShift;
	float leanAngle;

	float thrusterImpulse;
	float thrusterStabilizeImpulse;

	float gravityBootsMultipler;

	Vec3	hudOffset;
	Ang3	hudAngleOffset;

	//
	char animationAppendix[32];

	SPlayerParams()
	{
		memset(this, 0, sizeof(SPlayerParams));
		new (this) SActorParams();

		sprintMultiplier = 1.25f;
		strafeMultiplier = 0.9f;
		backwardMultiplier = 0.9f;
		grabMultiplier = 0.5f;
		afterburnerMultiplier = 1.5f;

		inertia = 7.0f;
		inertiaAccel = 11.0f;

		jumpHeight = 1.0f;

		slopeSlowdown = 2.1f;

		leanShift = 0.5f;
		leanAngle = 11.0f;

		thrusterImpulse = 5.0f;
		thrusterStabilizeImpulse = 1.0f;

		gravityBootsMultipler = 0.7f;

		strcpy(animationAppendix, "nw");
	}
};

struct IPlayerEventListener
{
	enum ESpecialMove
	{
		eSM_Jump = 0,
		eSM_SpeedSprint,
		eSM_StrengthJump,
	};

	virtual void OnEnterVehicle(IActor* pActor, const char* strVehicleClassName, const char* strSeatName, bool bThirdPerson) {};
	virtual void OnExitVehicle(IActor* pActor) {};
	virtual void OnToggleThirdPerson(IActor* pActor, bool bThirdPerson) {};
	virtual void OnItemDropped(IActor* pActor, EntityId itemId) {};
	virtual void OnItemPickedUp(IActor* pActor, EntityId itemId) {};
	virtual void OnItemUsed(IActor* pActor, EntityId itemId) {};
	virtual void OnStanceChanged(IActor* pActor, EStance stance) {};
	virtual void OnSpecialMove(IActor* pActor, ESpecialMove move) {};
	virtual void OnDeath(IActor* pActor, bool bIsGod) {};
	virtual void OnObjectGrabbed(IActor* pActor, bool bIsGrab, EntityId objectId, bool bIsNPC, bool bIsTwoHanded) {};
};

class PlayerView;

class CPlayer :
	public CActor, public ISoundSystemEventListener
{
	friend class CPlayerMovement;
	friend class CPlayerRotation;
	friend class CPlayerInput;
	friend class PlayerView;
	friend class CNetPlayerInput;

public:

	enum EPlayerSounds //most of these sounds are supposed to be played, when the nanosuit is not in use
	{
		ESound_Player_First,
		ESound_Run,
		ESound_StopRun,
		ESound_Jump,
		ESound_Fall_Drop,
		ESound_Melee,
		ESound_Fear,
		ESound_Choking,
		ESound_Hit_Wall,
		ESound_UnderwaterBreathing,
		ESound_Drowning,
		ESound_DiveIn,
		ESound_DiveOut,
		ESound_Underwater,
		ESound_Thrusters,
		ESound_ThrustersDash,
		ESound_ThrustersDashFail,
		ESound_ThrustersDash02,
		ESound_ThrustersDashRecharged,
		ESound_ThrustersDashRecharged02,
		ESound_TacBulletFeedBack,
		ESound_ParachuteStart,
		ESound_ParachuteRun,
		ESound_ParachuteStop,
		ESound_ProneOn,
		ESound_ProneOff,
		ESound_CrouchOn,
		ESound_CrouchOff,
		ESound_Player_Last
	};

	enum EClientPostEffect //first person screen effects for the client player
	{
		EEffect_ChromaShift = 1,
		EEffect_AlienInterference,
		EEffect_WaterDroplets
	};

	enum ELadderState
	{
		eLS_Exit,
		eLS_ExitTop,
		eLS_Climb
	};

	enum ELadderDirection
	{
		eLDIR_Up,
		eLDIR_Down,
		eLDIR_Stationary
	};

	enum ELadderActionType
	{
		eLAT_None,
		eLAT_ReachedEnd,
		eLAT_ExitTop,
		eLAT_Jump,
		eLAT_StrafeLeft,
		eLAT_StrafeRight,
		eLAT_Use,
		eLAT_Die,
	};

	static const int ASPECT_NANO_SUIT_SETTING = eEA_GameClientDynamic; // needs to be the same as IPlayerInput::INPUT_ASPECT, so that both get serialized together
	static const int ASPECT_NANO_SUIT_ENERGY = eEA_GameServerDynamic;
	static const int ASPECT_NANO_SUIT_INVULNERABLE = eEA_GameServerDynamic;
	static const int ASPECT_NANO_SUIT_DEFENSE_HIT = eEA_GameServerDynamic;

	static const int ASPECT_HEALTH = eEA_GameServerStatic;
	static const int ASPECT_FROZEN = eEA_GameServerStatic;
	static const int ASPECT_CURRENT_ITEM = eEA_GameClientStatic;

	CPlayer();
	virtual ~CPlayer();

	virtual void Draw(bool draw);
	virtual void BindInputs(IAnimationGraphState* pAGState);
	//marcok: GC workaround
	virtual bool ShouldSwim();
	virtual bool IsSwimming() { return m_bSwimming; }
	virtual bool IsSprinting();
	virtual bool CanFire();
	virtual bool Init(IGameObject* pGameObject);
	virtual void PostInit(IGameObject* pGameObject);
	virtual void InitClient(int channelId);
	virtual void InitLocalPlayer();
	virtual void ProcessEvent(SEntityEvent& event);
	virtual void SetAuthority(bool auth);
	virtual void SerializeXML(XmlNodeRef& node, bool bLoading);
	virtual void Update(SEntityUpdateContext& ctx, int updateSlot);
	virtual void PrePhysicsUpdate();
	virtual void UpdateView(SViewParams& viewParams);
	virtual bool UpdateFpSpectatorView(SViewParams& viewParams);
	virtual void PostUpdateView(SViewParams& viewParams);
	virtual void UpdateFirstPersonEffects(float frameTime);
	virtual void GetMemoryStatistics(ICrySizer* s);
	virtual void UpdateFootSteps(float frameTime);
	virtual int32 GetArmor() const;
	virtual int32 GetMaxArmor() const;

	virtual IEntity* LinkToVehicle(EntityId vehicleId);
	virtual IEntity* LinkToEntity(EntityId entityId, bool bKeepTransformOnDetach = true);
	virtual void LinkToMountedWeapon(EntityId weaponId);

	virtual void SetViewInVehicle(Quat viewRotation);
	virtual Vec3 GetVehicleViewDir() const { return m_vehicleViewDir; }

	virtual void SupressViewBlending() { m_viewBlending = false; };

	Vec3 GetLastRequestedVelocity() const { return m_lastRequestedVelocity; }
	void SetDeathTimer() { m_fDeathTime = gEnv->pTimer->GetFrameStartTime().GetSeconds(); }
	float GetDeathTime() const { return m_fDeathTime; }

	bool ProcessProceduralLean(float frameTime);
	void SufferingHighLatency(bool highLatency);

	static ActorType GetActorClassType() { return ActorType::CPlayer; }
	virtual ActorType GetActorClass() const { return GetActorClassType(); }

protected:
	virtual IActorMovementController* CreateMovementController();
	virtual void SetIK(const SActorFrameMovementParams&);

public:
	struct SAlienInterferenceParams
	{
		SAlienInterferenceParams() : maxdist(0.f) {}
		SAlienInterferenceParams(float dist) : maxdist(dist) {}

		float maxdist;
	};

	struct UnfreezeParams
	{
		UnfreezeParams() {};
		UnfreezeParams(float dlt) : delta(dlt) {};

		float delta;
		void SerializeWith(TSerialize ser)
		{
			ser.Value("delta", delta, 'frzn');
		}
	};
	struct LadderParams
	{
		LadderParams() : topPos(ZERO), bottomPos(ZERO), ladderOrientation(ZERO), reason(eLAT_None) {};
		LadderParams(Vec3 _topPos, Vec3 _bottomPos, Vec3 _ladderOrientation, ELadderActionType _reason)
			: topPos(_topPos), bottomPos(_bottomPos), ladderOrientation(_ladderOrientation), reason(_reason) {};

		void SerializeWith(TSerialize ser)
		{
			ser.Value("topPos", topPos, /* 'wrld' */ 0x77726C64);
			ser.Value("bottomPos", bottomPos, /* 'wrld' */ 0x77726C64);
			ser.Value("ladderOrientation", ladderOrientation, 'dir0');
			ser.Value("reason", reason);
		}

		Vec3 topPos;
		Vec3 bottomPos;
		Vec3 ladderOrientation;
		int reason;
	};
	struct HitAssistanceParams
	{
		HitAssistanceParams() : assistance(false) {};
		HitAssistanceParams(bool _assistance) : assistance(_assistance) {};

		void SerializeWith(TSerialize ser)
		{
			ser.Value("assistance", assistance, 'bool');
		}

		bool assistance;
	};


	struct EMPParams
	{
		EMPParams() : time(1.0f) {};
		EMPParams(float _time) : time(_time) {};

		float time;

		void SerializeWith(TSerialize ser)
		{
			ser.Value("time", time);
		}
	};

	struct JumpParams
	{
		JumpParams() : strengthJump(false) {};
		JumpParams(bool _strengthJump) : strengthJump(_strengthJump) {};

		bool strengthJump;

		void SerializeWith(TSerialize ser)
		{
			ser.Value("strengthJump", strengthJump, 'bool');
		}
	};

	DECLARE_SERVER_RMI_NOATTACH_FAST(SvRequestGrabOnLadder, LadderParams, eNRT_ReliableOrdered);
	DECLARE_SERVER_RMI_NOATTACH_FAST(SvRequestLeaveLadder, LadderParams, eNRT_ReliableOrdered);

	DECLARE_CLIENT_RMI_NOATTACH_FAST(ClGrabOnLadder, LadderParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH_FAST(ClLeaveLadder, LadderParams, eNRT_ReliableOrdered);

	DECLARE_SERVER_RMI_NOATTACH_FAST(SvRequestUnfreeze, UnfreezeParams, eNRT_ReliableUnordered);

	DECLARE_SERVER_RMI_NOATTACH_FAST(SvRequestHitAssistance, HitAssistanceParams, eNRT_ReliableUnordered);

	DECLARE_CLIENT_RMI_NOATTACH(ClEMP, EMPParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH_FAST(ClJump, JumpParams, eNRT_ReliableUnordered);
	DECLARE_SERVER_RMI_NOATTACH_FAST(SvRequestJump, JumpParams, eNRT_ReliableUnordered);
	DECLARE_SERVER_RMI_PREATTACH(SvRequestParachute, NoParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_PREATTACH(ClParachute, NoParams, eNRT_ReliableOrdered);

	//set/get actor status
	virtual void SetStats(SmartScriptTable& rTable);
	virtual void UpdateScriptStats(SmartScriptTable& rTable);
	virtual void UpdateStats(float frameTime);
	virtual void UpdateSwimStats(float frameTime);
	virtual void UpdateUWBreathing(float frameTime, Vec3 worldBreathPos);
	virtual void UpdateWeaponRaising();
	virtual void SetParams(SmartScriptTable& rTable, bool resetFirst);
	void SetParamsMP(bool resetFirst);
	virtual bool GetParams(SmartScriptTable& rTable);

	virtual float CalculatePseudoSpeed(bool wantSprint) const;

	// Accessed via function to allow game based modifiers to stance speed without multiplying the number of stances.
	virtual float GetStanceMaxSpeed(EStance stance) const;
	// Accessed via function to allow game based modifiers to stance speed without multiplying the number of stances.
	virtual float GetStanceNormalSpeed(EStance stance) const;

	virtual void ToggleThirdPerson();
	void EnableThirdPerson(bool enable);

	virtual int  IsGod();

	virtual void Revive(ReasonForRevive reason = ReasonForRevive::NONE);
	virtual void Kill();

	//stances
	virtual Vec3	GetStanceViewOffset(EStance stance, float* pLeanAmt = NULL, bool withY = false) const;
	virtual bool IsThirdPerson() const;
	virtual void StanceChanged(EStance lastStance, EStance newStance);
	void StanceSound(EStance lastStance, EStance newStance);
	//virtual bool TrySetStance(EStance stance); // Moved to Actor, to be shared with Aliens.

	virtual void ResetAnimGraph();

	CNanoSuit* GetNanoSuit() const { return m_pNanoSuit; }
	void ActivateNanosuit(bool active);

	virtual void SetFlyMode(uint8 flyMode);
	virtual uint8 GetFlyMode() const { return m_stats.flyMode; };
	virtual void SetSpectatorMode(uint8 mode, EntityId targetId);
	virtual uint8 GetSpectatorMode() const { return m_stats.spectatorMode; };
	virtual void SetSpectatorTarget(EntityId targetId) { m_stats.spectatorTarget = targetId; };
	virtual void SetDeathCamTarget(EntityId targetId) { m_stats.deathCamTarget = targetId; };
	virtual EntityId GetSpectatorTarget() const { return m_stats.spectatorTarget; };
	virtual EntityId GetDeathCamTarget() const { return m_stats.deathCamTarget; };
	virtual void SetSpectatorHealth(int health) override;
	virtual int GetSpectatorHealth() const { return m_stats.spectatorHealth; };
	virtual void ChangeSpectatorZoom(int zoomChange) { m_stats.spectatorZoom = CLAMP(m_stats.spectatorZoom + zoomChange, 2, 8); }
	virtual int GetSpectatorZoom() const { return m_stats.spectatorZoom; }
	void MoveToSpectatorTargetPosition();

	virtual void SelectNextItem(int direction, bool keepHistory, const char* category);
	virtual void HolsterItem(bool holster);
	virtual void SelectLastItem(bool keepHistory, bool forceNext = false);
	virtual void SelectItemByName(const char* name, bool keepHistory);
	virtual void SelectItem(EntityId itemId, bool keepHistory);

	virtual void RagDollize(bool fallAndPlay);
	virtual void HandleEvent(const SGameObjectEvent& event);

	virtual void UpdateAnimGraph(IAnimationGraphState* pState);
	virtual void PostUpdate(float frameTime);
	virtual void PostRemoteSpawn();

	virtual void AnimationEvent(ICharacterInstance* pCharacter, const AnimEventInstance& event);

	virtual void SetViewRotation(const Quat& rotation);
	virtual Quat GetViewRotation() const;
	virtual void EnableTimeDemo(bool bTimeDemo);

	virtual void SetViewAngleOffset(const Vec3& offset);
	virtual Vec3 GetViewAngleOffset() { return (Vec3)m_viewAnglesOffset; };

	virtual bool SetAspectProfile(EEntityAspects aspect, uint8 profile);

	virtual void FullSerialize(TSerialize ser);
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	virtual void PostSerialize();
	//set/get actor params
	void OnHealthChanged(const int health);
	virtual void SetHealth(int health);
	virtual SActorStats* GetActorStats() { return &m_stats; };
	virtual const SActorStats* GetActorStats() const { return &m_stats; };
	SPlayerStats* GetPlayerStats() { return &m_stats; }
	const SPlayerStats* GetPlayerStats() const { return &m_stats; }
	virtual SActorParams* GetActorParams() { return &m_params; };
	virtual void PostPhysicalize();
	virtual void CameraShake(float angle, float shift, float duration, float frequency, Vec3 pos, int ID, const char* source = "");
	virtual bool CreateCodeEvent(SmartScriptTable& rTable);
	void SetExtension(const char* extension);
	void SetInput(const char* action, bool looping = false);
	ILINE virtual void VectorToLocal(Vec3& v) { v = m_clientViewMatrix.GetInverted() * v; }
	ILINE virtual Matrix34 GetViewMatrix() const { return m_clientViewMatrix; }
	virtual void AddAngularImpulse(const Ang3& angular, float deceleration, float duration);
	virtual void SetAngles(const Ang3& angles);
	virtual Ang3 GetAngles();
	virtual void PlayAction(const char* action, const char* extension, bool looping = false);
	virtual void UpdateGrab(float frameTime);
	virtual float GetActorStrength() const;
	virtual void Freeze(bool freeze);
	virtual void ProcessBonesRotation(ICharacterInstance* pCharacter, float frameTime);
	bool SetGrabTarget(EntityId targetId);
	bool IsGrabTargetSet(EntityId targetId) const
	{
		return !m_reachNotified && m_grabTargetId && m_grabTargetId != targetId;
	}
	bool StartThrowPrep();   
	void CommitThrow();       
	void CancelThrowPrep();   
	void CancelGrabTarget();
	float GetReachDesiredPitch() const;
	void UpdateReachBend(float frameTime);
	void ApplyReachToSpine(ICharacterInstance* pCharacter, float bendAngle, float amount);
	virtual void ProcessIKLegs(ICharacterInstance* pCharacter, float frameTime);
	virtual void Landed(float fallSpeed);

	//Player can grab north koreans
	virtual int GetActorSpecies() { return eGCT_HUMAN; }
	virtual bool IsCloaked() const;

	virtual EntityId	GetGrabbedEntityId() const;

	//Support sleep 
	virtual bool CanSleep() { return true; }

	void UpdateParachute(float frameTime);
	void UpdateParachuteIK();
	void UpdateHeldObjectIK();
	void UpdateParachuteMorph(float frameTime);
	void ChangeParachuteState(int8 newState);
	void DeployParachute(bool show, bool sound);
	void UpdateFreefallAnimationInputs(bool force = false, bool parachuteActive = false);

	void ProcessCharacterOffset(float frameTime);

	const Quat& GetBaseQuat() { return m_baseQuat; }
	const Quat& GetViewQuat() { return m_viewQuat; }
	const Quat& GetViewQuatFinal() { return m_viewQuatFinal; }
	virtual void ResetAnimations();
	float GetMassFactor() const;
	bool	IsFiringProne() const;
	virtual float GetFrozenAmount(bool stages = false) const;
	IPlayerInput* GetPlayerInput() const { return m_pPlayerInput.get(); }

	bool IsTimeDemo() const { return m_timedemo; }
	void ForceFreeFall() { m_stats.inFreefall = 1; }

	void StopLoopingSounds();

	void RegisterPlayerEventListener(IPlayerEventListener* pPlayerEventListener);
	void UnregisterPlayerEventListener(IPlayerEventListener* pPlayerEventListener);

	void ResetOpacity();

	ILINE bool GravityBootsOn() const
	{
		return false; //(m_actions & ACTION_GRAVITYBOOTS && InZeroG() )?true:false;
	}

	void Stabilize(bool stabilize);
	ILINE bool GetStabilize() const { return m_bStabilize; }
	ILINE float GetSpeedLean() const { return m_fSpeedLean; }
	ILINE void SetSpeedLean(float speedLean) { m_fSpeedLean = speedLean; }

	ILINE void EnableParachute(bool enable) { m_parachuteEnabled = enable; };
	ILINE bool IsParachuteEnabled() const { return m_parachuteEnabled; };

	virtual bool UseItem(EntityId itemId);
	virtual bool PickUpItem(EntityId itemId, bool sound);
	virtual bool DropItem(EntityId itemId, float impulseScale = 1.0f, bool slectNext = true, bool byDeath = false);

	ILINE const Vec3& GetEyeOffset() const { return m_eyeOffset; }
	ILINE const Vec3& GetWeaponOffset() const { return m_weaponOffset; }

	void UpdateUnfreezeInput(const Ang3& deltaRotation, const Vec3& deltaMovement, float mult);

	void SpawnParticleEffect(const char* effectName, const Vec3& pos, const Vec3& dir);
	void PlaySound(EPlayerSounds sound, bool play = true, bool param = false, const char* paramName = nullptr, float paramValue = 0.0f);
	virtual void SendMusicLogicEvent(EMusicLogicEvents event);

	//Ladders
	bool IsLadderUsable();
	void RequestGrabOnLadder(ELadderActionType reason);
	void GrabOnLadder(ELadderActionType reason);
	void RequestLeaveLadder(ELadderActionType reason);
	void LeaveLadder(ELadderActionType reason);

	bool UpdateLadderAnimation(ELadderState eLS, ELadderDirection eLDIR, float time = 0.0f);

	//Pick Up Items (pos parameter is worldspace position of item to pick up)
	bool NeedToCrouch(const Vec3& pos);

	// mines (and claymores)
	void RemoveAllExplosives(float timeDelay, uint8 typeId = 0xff);
	void RemoveExplosiveEntity(EntityId entityId);
	void RecordExplosivePlaced(EntityId entityId, uint8 typeId);
	void RecordExplosiveDestroyed(EntityId entityId, uint8 typeId);

	//First person fists/hands actions
	void EnterFirstPersonSwimming();
	void ExitFirstPersonSwimming();
	void UpdateFirstPersonSwimming();
	void UpdateFirstPersonSwimmingEffects(bool exitWater, float velSqr);
	void UpdateFirstPersonFists();

	void ResetScreenFX();
	void ResetFPView();

	//Hit assistance
	bool HasHitAssistance();

	void CreateVoiceListener();

	struct SStagingParams
	{
		SStagingParams() :
			bActive(false), bLocked(false), vLimitDir(ZERO), vLimitRangeH(0.0f), vLimitRangeV(0.0f), stance(STANCE_NULL)
		{
		}

		bool  bActive;
		bool  bLocked;
		Vec3  vLimitDir;
		float vLimitRangeH;
		float vLimitRangeV;
		EStance stance;
		void Serialize(TSerialize ser)
		{
			assert(ser.GetSerializationTarget() != eST_Network);
			ser.BeginGroup("SStagingParams");
			ser.Value("bActive", bActive);
			ser.Value("bLocked", bLocked);
			ser.Value("vLimitDir", vLimitDir);
			ser.Value("vLimitRangeH", vLimitRangeH);
			ser.Value("vLimitRangeV", vLimitRangeV);
			ser.EnumValue("stance", stance, STANCE_NULL, STANCE_LAST);
			ser.EndGroup();
		}
	};

	void StagePlayer(bool bStage, SStagingParams* pStagingParams = 0);

	void NotifyObjectGrabbed(bool bIsGrab, EntityId objectId, bool bIsNPC, bool bIsTwoHanded = false); // called from OffHand.cpp. bIsGrab always true atm

	virtual void OnSoundSystemEvent(ESoundSystemCallbackEvent event, ISound* pSound);

private:
	void AnimationControlled(bool activate);
	bool ShouldUsePhysicsMovement();
	void Debug();
	void SetPainEffect(float progress = 0.0f);

	void UpdateSounds(float fFrameTime);
	float m_fLowHealthSoundMood;
	float m_fConcentrationTimer;
	bool m_bConcentration;

	typedef std::list<IPlayerEventListener*> TPlayerEventListeners;
	TPlayerEventListeners m_playerEventListeners;

protected:
	Vec3		m_modelOffset;
	float		m_groundElevation;
	Ang3 		m_viewAnglesOffset;//used for recoil
	Ang3		m_headAngles;//head angles in local space, for bones rotation

	// bool IsMaterialBootable(int matId) const;
	ILINE bool IsMaterialBootable(int matId) const { return true; }

	void InitInterference();

	CNanoSuit* m_pNanoSuit;

	Ang3		m_angleOffset;	// Used only by the view system. (retained state)

	Quat	m_viewQuat;//view quaternion
	Quat	m_viewQuatFinal;//the final exact view quaternion (with recoil added)
	Quat	m_baseQuat;//base rotation quaternion, rotating on the Z axis

	Matrix34 m_clientViewMatrix;

	Vec3		m_eyeOffset;	// View system - used to interpolate to goal eye offset
												//the offset from the entity origin to eyes, its not the real offset vector but its referenced to player view direction.

	Vec3		m_eyeOffsetView; //this is exclusive for PlayerView to use, do not touch it outside PlayerView

	Vec3		m_weaponOffset;

	float		m_viewRoll;

	Vec3		m_upVector;//using gravity boots (or when prone) is the normal of the surface where the player is standing on, otherwise is the default (0,0,1)

	Vec3		m_bobOffset;

	Vec3		m_FPWeaponLastDirVec;
	Vec3		m_FPWeaponOffset;
	Ang3		m_FPWeaponAngleOffset;

	// updated by PlayerMovement for tracking time based acceleration
	Vec3		m_velocity;
	// updated by PlayerMovement to allow for turning faster than we can in one frame
	Quat    m_turnTarget;
	Vec3		m_lastRequestedVelocity;
	Vec3    m_lastKnownPosition;

	SPlayerStats		m_stats;
	SPlayerParams		m_params;

	std::unique_ptr<IPlayerInput> m_pPlayerInput;

	// compatibility with old code: which actions are set
	int m_actions;

	// TODO: wish i could figure out a way of removing this
	// was the rotation forced this frame?
	bool m_forcedRotation;
	bool m_viewBlending;
	bool m_ignoreRecoil;
	bool m_timedemo;

	bool m_bRagDollHead;

	// animation graph input ids
	IAnimationGraph::InputID m_inputAction;
	IAnimationGraph::InputID m_inputItem;
	IAnimationGraph::InputID m_inputUsingLookIK;
	IAnimationGraph::InputID m_inputAiming;
	IAnimationGraph::InputID m_inputVehicleName;
	IAnimationGraph::InputID m_inputVehicleSeat;
	IAnimationGraph::InputID m_inputPseudoSpeed;

	// probably temporary, feel free to figure out better place
	float m_lastAnimControlled;
	Quat m_lastAnimContRot;
	Vec3 m_lastAnimContPos;
	bool m_bSwimming;
	bool m_bSprinting;
	bool m_bUnderwater;
	bool m_bStabilize;
	float m_fSpeedLean;
	float m_fDeathTime;
	float m_drownEffectDelay;
	float m_underwaterBubblesDelay;
	float m_stickySurfaceTimer;

	// used by parachute. 
	bool m_ParachuteOpen = false;
	float m_fParachuteMorph = 0.0f; //0..1 to play morph targets
	bool		m_parachuteEnabled;
	float		m_openParachuteTimer;
	bool    m_openingParachute;

	bool		m_sufferingHighLatency;

	CVehicleClient* m_pVehicleClient;
	Vec3 m_vehicleViewDir;

	//sound loops that are played when we don't have a nanosuit
	float		 m_sprintTimer;
	tSoundID m_sounds[ESound_Player_Last];
	//client / localActor active first person effects
	std::vector<EClientPostEffect> m_clientPostEffects;

	typedef std::map<IEntityClass*, const SAlienInterferenceParams> TAlienInterferenceParams;
	static TAlienInterferenceParams m_interferenceParams;

	std::list<EntityId>			m_explosiveList[3];
	bool                    m_bSpeedSprint;
	bool										m_bHasAssistance;
	bool                    m_bVoiceSoundPlaying;
	bool                    m_bVoiceSoundRecursionFlag;
	IGameObjectExtension* m_pVoiceListener;
	IGameObjectExtension* m_pInteractor;
	IEntitySoundProxy* m_pSoundProxy;

	SStagingParams m_stagingParams;

	static unsigned int s_ladderMaterial;
public:
	IDebugHistoryManager* m_pDebugHistoryManager;
	void DebugGraph_AddValue(const char* id, float value) const;


//////////////////////////////////////////////////////////////////////////////////
//CryMP 
//////////////////////////////////////////////////////////////////////////////////

private:

	static constexpr std::string_view m_parachuteAttachmentName = "parachute_attach";

	float m_tpLeanOffset = 0.0f;
	float m_tpProneOffset = 0.0f;
	float m_prevFrozenAmount = 0.0f;
	Vec3 m_vehicleViewDirSmooth = Vec3(ZERO);
	Vec3 m_netAimDir = Vec3(ZERO);
	Vec3 m_netAimDirSmooth = Vec3(ZERO);
	bool m_bSlowCamera = false;
	bool GetAimTargetAdjusted(Vec3& aimTarget);

	SViewParams m_FirstPersonSpectatorParams = SViewParams();
	void UpdateSpectator(EntityId oldTargetId, EntityId newTargetId);

	PlayerView m_PlayerView = PlayerView(*this);

	void UpdateDraw();
	void UpdateCharacter(ICharacterInstance* pCharacter, bool characterLoad = false);
	void UpdateScreenFrost();
	void UpdateScreenEffects(float frameTime);
	void SetDofFxLimits(float focusmin, float focusmax, float focuslim, float speed = 0);
	void SetDofFxMask(const char* texName);
	void SetDofFxAmount(float amount, float speed = 0);
	void ResetDofFx(float speed = 0);
	void UpdateDofFx(float frameTime);
	void SetMotionFxAmount(float amount, float speed = 0);
	void SetMotionFxMask(const char* texName);
	void ResetMotionFx();
	void UpdateMotionFx(float frameTime);

	float DofInterpolate(float curr, float target, float speed, float frameTime);
	float MBlurInterpolate(float curr, float target, float speed, float frameTime);

	void UpdateModelChangeInVehicle();

	float m_viewBlur = 0.0f;
	float m_viewBlurAmt = 0.0f;

	int m_blurType = 0;

	// Depth of Field (DoF) related variables
	float m_dof_amount_speed = 0.0f;
	float m_dof_distance_speed = 0.0f;
	float m_target_dof_min = 0.0f;
	float m_target_dof_max = 2000.0f;
	float m_target_dof_lim = 2500.0f;
	float m_target_dof_amount = 0.0f;
	float m_current_dof_min = 0.0f;
	float m_current_dof_max = 2000.0f;
	float m_current_dof_lim = 2500.0f;
	float m_current_dof_amount = 0.0f;

	// Motion Blur related variables
	float m_mblur_amount_speed = 0.0f;
	float m_target_mblur_amount = 0.0f;
	float m_current_mblur_amount = 0.0f;

	int m_lastAttachmentCount = 0;

	Vec3 m_lefthandGrip = Vec3(ZERO);
	Vec3 m_righthandGrip = Vec3(ZERO);
	bool m_handGripsValid = false;

private:

	enum class ReachState : uint8
	{
		Idle = 0,
		Reaching,    
		ThrowPrep,   
		Throwing,   
		Returning
	};

	EntityId   m_grabTargetId = 0;
	ReachState m_reachState = ReachState::Idle;

	float m_reachAmount = 0.0f;  
	bool  m_reachNotified = false;            
	float m_reachCurrentPitch = 0.0f;

public:

	void SetArmIKLocal(const Vec3& leftHand, const Vec3& rightHand)
	{
		m_lefthandGrip = leftHand;
		m_righthandGrip = rightHand;
		m_handGripsValid = true;
	}

	void SetArmIKLocalInvalid()
	{
		m_handGripsValid = false;
	}

	bool IsLeftRightHandGripValid() const 
	{	
		return m_handGripsValid; 
	}

	// Member variables
	bool m_camoState = false;
	bool m_camoFading = false;

	virtual bool IsSpectatorTarget() const { return m_stats.spectatorTarget; }

	//First/Third Person Spectator
	virtual bool IsFpSpectator() const 
	{ 
		return m_stats.fpSpectator; 
	}
	virtual bool IsFpSpectatorTarget() const override
	{
		return m_stats.spectatorTargetType == SpectatorTargetType::FIRST_PERSON;
	}
	virtual bool IsTpSpectatorTarget() const override
	{
		return m_stats.spectatorTargetType == SpectatorTargetType::THIRD_PERSON;
	}
	void SetFpSpectator(bool activate) 
	{
		m_stats.fpSpectator = activate; 
	}
	void EnableFpSpectatorTarget(bool activate);
	SpectatorTargetType GetSpectatorTargetType() const
	{
		return m_stats.spectatorTargetType;
	}
	void SetSpectatorTargetType(SpectatorTargetType type);

	IActor* GetSpectatorTargetActor();

	void SetSlowCamera(bool on) { m_bSlowCamera = on; }
	bool IsSlowCamera() const { return m_bSlowCamera; }
	float m_fCameraMoveSpeedMult = 1.0f;
	float m_targetOpacity = 1.0f;
	float m_smoothedOpacity = 1.0f;

	bool IsParachuteActive()
	{
		return m_ParachuteOpen;
	}

	Vec3 GetVehicleViewDirSmooth() const { return m_vehicleViewDirSmooth; }
	Vec3 GetNetAimDir() const { return m_netAimDir; }
	Vec3 GetNetAimDirSmooth() const { return m_netAimDirSmooth; }


	void OnObjectEvent(ObjectEvent evnt, const EntityId objectId) override;
	void PlayAnimation(const char* animationName, float speed = 1.0f, bool loop = false, bool noBlend = false, int layerID = 0);

	bool IsStanceInputValid(int stance) const;

	static CPlayer* FromIActor(IActor* pActor)
	{
		if (!pActor)
			return nullptr;

		if (static_cast<CActor*>(pActor)->GetActorClass() == CPlayer::GetActorClassType())
			return static_cast<CPlayer*>(pActor);
		else
			return nullptr;
	}

	static CPlayer* FromActor(CActor* pActor)
	{
		if (!pActor)
			return nullptr;

		if (pActor->GetActorClass() == CPlayer::GetActorClassType())
			return static_cast<CPlayer*>(pActor);
		else
			return nullptr;
	}

	static CPlayer* FromActorId(const EntityId actorId)
	{
		if (!actorId)
			return nullptr;

		return FromIActor(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(actorId));
	}

	static CNanoSuit* GetNanoSuit(CActor* pActor)
	{
		if (!pActor)
			return nullptr;

		if (CPlayer* pPlayer = FromActor(pActor))
			return pPlayer->GetNanoSuit();
		else
			return nullptr;
	}

};


#endif //__PLAYER_H__
