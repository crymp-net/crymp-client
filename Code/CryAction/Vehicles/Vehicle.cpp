#include "CryAction/GameFramework.h"
#include "CryAction/ScriptBind_Vehicle.h"
#include "CryAction/ScriptBind_VehicleSeat.h"
#include "CryCommon/CryAction/IGameRulesSystem.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CrySystem/gEnv.h"
#include "CrySystem/CryLog.h"
#include "Library/WinAPI.h"

#include "Vehicle.h"

extern std::uintptr_t CRYACTION_BASE;

Vehicle::Vehicle()
{
#ifdef BUILD_64BIT
	static_assert(sizeof(Vehicle) == 0x550);
	static_assert(offsetof(Vehicle, m_pVehicleSystem) == 0xb0);
	static_assert(offsetof(Vehicle, m_pInventory) == 0xb8);
	static_assert(offsetof(Vehicle, m_physicalizeParams) == 0xc0);
	static_assert(offsetof(Vehicle, m_pMovement) == 0x120);
	static_assert(offsetof(Vehicle, m_engineSlotBySpeed) == 0x15e);
	static_assert(offsetof(Vehicle, m_seats) == 0x168);
	static_assert(offsetof(Vehicle, m_seatGroups) == 0x180);
	static_assert(offsetof(Vehicle, m_components) == 0x1a0);
	static_assert(offsetof(Vehicle, m_parts) == 0x1b8);
	static_assert(offsetof(Vehicle, m_buoyancy) == 0x230);
	static_assert(offsetof(Vehicle, m_physics) == 0x278);
	static_assert(offsetof(Vehicle, m_physicsParams) == 0x2c4);
	static_assert(offsetof(Vehicle, m_isDestroyed) == 0x2d4);
	static_assert(offsetof(Vehicle, m_mass) == 0x2d8);
	static_assert(offsetof(Vehicle, m_animations) == 0x420);
	static_assert(offsetof(Vehicle, m_pSoundProxy) == 0x450);
	static_assert(offsetof(Vehicle, m_retainGravity) == 0x470);
	static_assert(offsetof(Vehicle, m_gravity) == 0x474);
	static_assert(offsetof(Vehicle, m_modName) == 0x498);
	static_assert(offsetof(Vehicle, m_paintName) == 0x4a0);
	static_assert(offsetof(Vehicle, m_totalMaxDamage) == 0x4ac);
	static_assert(offsetof(Vehicle, m_majorComponentMaxDamage) == 0x4b0);
	static_assert(offsetof(Vehicle, m_actionMapName) == 0x4b8);
#else
	static_assert(sizeof(Vehicle) == 0x37c);
	static_assert(offsetof(Vehicle, m_pVehicleSystem) == 0x68);
	static_assert(offsetof(Vehicle, m_pInventory) == 0x6c);
	static_assert(offsetof(Vehicle, m_physicalizeParams) == 0x70);
	static_assert(offsetof(Vehicle, m_pMovement) == 0xb0);
	static_assert(offsetof(Vehicle, m_engineSlotBySpeed) == 0xea);
	static_assert(offsetof(Vehicle, m_seats) == 0xf4);
	static_assert(offsetof(Vehicle, m_seatGroups) == 0x100);
	static_assert(offsetof(Vehicle, m_components) == 0x110);
	static_assert(offsetof(Vehicle, m_parts) == 0x11c);
	static_assert(offsetof(Vehicle, m_buoyancy) == 0x158);
	static_assert(offsetof(Vehicle, m_physics) == 0x1a0);
	static_assert(offsetof(Vehicle, m_physicsParams) == 0x1ec);
	static_assert(offsetof(Vehicle, m_isDestroyed) == 0x1fc);
	static_assert(offsetof(Vehicle, m_mass) == 0x200);
	static_assert(offsetof(Vehicle, m_animations) == 0x2cc);
	static_assert(offsetof(Vehicle, m_pSoundProxy) == 0x2e4);
	static_assert(offsetof(Vehicle, m_retainGravity) == 0x2f4);
	static_assert(offsetof(Vehicle, m_gravity) == 0x2f8);
	static_assert(offsetof(Vehicle, m_modName) == 0x318);
	static_assert(offsetof(Vehicle, m_paintName) == 0x31c);
	static_assert(offsetof(Vehicle, m_totalMaxDamage) == 0x324);
	static_assert(offsetof(Vehicle, m_majorComponentMaxDamage) == 0x328);
	static_assert(offsetof(Vehicle, m_actionMapName) == 0x330);
#endif

#ifdef BUILD_64BIT
	std::uintptr_t ctor = CRYACTION_BASE + 0x7f340;
#else
	std::uintptr_t ctor = CRYACTION_BASE + 0x591b0;
#endif

	(this->*reinterpret_cast<void(IVehicle::*&)()>(ctor))();

	m_self = this;
}

Vehicle::~Vehicle()
{
}

////////////////////////////////////////////////////////////////////////////////
// IGameObjectExtension
////////////////////////////////////////////////////////////////////////////////

bool Vehicle::Init(IGameObject* pGameObject)
{
	this->SetGameObject(pGameObject);

	if (!this->GetGameObject()->CaptureProfileManager(this))
	{
		CryLogErrorAlways("[Vehicle] Profile manager capture failed for %s", this->GetEntity()->GetName());
		return false;
	}

	m_pSoundProxy = static_cast<IEntitySoundProxy*>(this->GetEntity()->CreateProxy(ENTITY_PROXY_SOUND));

	m_pVehicleSystem = GameFramework::GetInstance()->GetIVehicleSystem();
	m_engineSlotBySpeed = true;

	m_pVehicleSystem->AddVehicle(this->GetEntityId(), this);
	GameFramework::GetInstance()->GetScriptBind_Vehicle()->AttachTo(this);

	m_pInventory = static_cast<IInventory*>(this->GetGameObject()->AcquireExtension("Inventory"));

	this->InitRespawn();

	if (m_modName.empty())
	{
		this->InitModName();
	}

	if (m_paintName.empty())
	{
		this->InitPaintName();
	}

	this->GetEntity()->AddFlags(ENTITY_FLAG_CASTSHADOW | ENTITY_FLAG_CUSTOM_VIEWDIST_RATIO | ENTITY_FLAG_ON_RADAR);

	IEntityProxy* pRenderProxy = this->GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
	if (pRenderProxy)
	{
		static_cast<IEntityRenderProxy*>(pRenderProxy)->GetRenderNode()->SetViewDistRatio(50);
	}

	return true;
}

void Vehicle::PostInit(IGameObject* pGameObject)
{
}

void Vehicle::InitClient(int channelId)
{
}

void Vehicle::PostInitClient(int channelId)
{
}

void Vehicle::Release()
{
}

void Vehicle::FullSerialize(TSerialize ser)
{
}

bool Vehicle::NetSerialize(TSerialize ser, EEntityAspects aspect, std::uint8_t profile, int flags)
{
	return {};
}

void Vehicle::PostSerialize()
{
}

void Vehicle::SerializeSpawnInfo(TSerialize ser)
{
}

ISerializableInfoPtr Vehicle::GetSpawnInfo()
{
	return {};
}

void Vehicle::Update(SEntityUpdateContext& context, int updateSlot)
{
}

void Vehicle::HandleEvent(const SGameObjectEvent& event)
{
}

void Vehicle::ProcessEvent(SEntityEvent& event)
{
}

void Vehicle::GetMemoryStatistics(ICrySizer* s)
{
}

void Vehicle::SetChannelId(std::uint16_t id)
{
}

void Vehicle::SetAuthority(bool auth)
{
}

const void* Vehicle::GetRMIBase() const
{
	return {};
}

void Vehicle::PostUpdate(float frameTime)
{
}

void Vehicle::PostRemoteSpawn()
{
}

////////////////////////////////////////////////////////////////////////////////
// IVehicle
////////////////////////////////////////////////////////////////////////////////

void Vehicle::Reset(bool enterGame)
{
}

SEntityPhysicalizeParams& Vehicle::GetPhysicsParams()
{
	static SEntityPhysicalizeParams params{};
	return params;
}

const SVehicleStatus& Vehicle::GetStatus()
{
	static const SVehicleStatus status{};
	return status;
}

void Vehicle::UpdateView(SViewParams& viewParams, EntityId playerId)
{
}

void Vehicle::OnAction(const TVehicleActionId actionId, int activationMode, float value, EntityId callerId)
{
}

void Vehicle::OnHit(EntityId targetId, EntityId shooterId, float damage, Vec3 position, float radius, const char* pHitClass, bool explosion)
{
}

float Vehicle::GetDamageRatio(bool onlyMajorComponents) const
{
	return {};
}

void Vehicle::SetAmmoCount(IEntityClass* pAmmoType, int amount)
{
}

int Vehicle::GetAmmoCount(IEntityClass* pAmmoType) const
{
	return {};
}

int Vehicle::GetAmmoCapacity(IEntityClass* pAmmoType) const
{
	return {};
}

void Vehicle::SetOwnerId(EntityId ownerId)
{
}

EntityId Vehicle::GetOwnerId() const
{
	return {};
}

int Vehicle::IsUsable(EntityId userId)
{
	return {};
}

bool Vehicle::OnUsed(EntityId userId, int index)
{
	return {};
}

bool Vehicle::AddSeat(const SmartScriptTable& seatParams)
{
	return {};
}

bool Vehicle::AddHelper(const char* name, Vec3 position, Vec3 direction, IVehiclePart* pPart)
{
	return {};
}

int Vehicle::GetPartCount()
{
	return {};
}

IVehiclePart* Vehicle::GetPart(unsigned int index)
{
	return {};
}

void Vehicle::GetParts(IVehiclePart** parts, int nMax)
{
}

int Vehicle::GetComponentCount() const
{
	return {};
}

IVehicleComponent* Vehicle::GetComponent(int index)
{
	return {};
}

IVehicleComponent* Vehicle::GetComponent(const char* name)
{
	return {};
}

IVehicleAnimation* Vehicle::GetAnimation(const char* name)
{
	return {};
}

int Vehicle::GetActionCount()
{
	return {};
}

IVehicleAction* Vehicle::GetAction(int index)
{
	return {};
}

IMovementController* Vehicle::GetMovementController()
{
	return {};
}

IFireController* Vehicle::GetFireController()
{
	return {};
}

TVehicleSeatId Vehicle::GetLastSeatId()
{
	return {};
}

IVehicleSeat* Vehicle::GetSeatById(const TVehicleSeatId seatId)
{
	return {};
}

unsigned int Vehicle::GetSeatCount()
{
	return {};
}

IVehicleSeat* Vehicle::GetSeatForPassenger(EntityId passengerId)
{
	return {};
}

TVehicleSeatId Vehicle::GetSeatId(const char* pSeatName)
{
	return {};
}

bool Vehicle::IsPlayerDriving(bool clientOnly)
{
	return {};
}

bool Vehicle::HasFriendlyPassenger(IEntity* pPlayer)
{
	return {};
}

bool Vehicle::IsPlayerPassenger()
{
	return {};
}

int Vehicle::GetWeaponCount() const
{
	return {};
}

EntityId Vehicle::GetWeaponId(int index) const
{
	return {};
}

EntityId Vehicle::GetCurrentWeaponId(EntityId passengerId, bool secondary)
{
	return {};
}

bool Vehicle::HasHelper(const char* pName)
{
	return {};
}

IVehiclePart* Vehicle::GetPart(const char* name)
{
	return {};
}

IVehiclePart* Vehicle::GetWeaponParentPart(EntityId weaponId)
{
	return {};
}

IVehicleSeat* Vehicle::GetWeaponParentSeat(EntityId weaponId)
{
	return {};
}

IVehicleMovement* Vehicle::GetMovement()
{
	return {};
}

int Vehicle::GetWheelCount()
{
	return {};
}

IVehiclePart* Vehicle::GetWheelPart(int idx)
{
	return {};
}

float Vehicle::GetMass()
{
	return {};
}

float Vehicle::GetAltitude()
{
	return {};
}

IMovementController* Vehicle::GetPassengerMovementController(EntityId passenger)
{
	return {};
}

void Vehicle::BroadcastVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
}

void Vehicle::RegisterVehicleEventListener(IVehicleEventListener* pEvenListener, const char* name)
{
}

void Vehicle::UnregisterVehicleEventListener(IVehicleEventListener* pEvenListener)
{
}

SParticleParams* Vehicle::GetParticleParams()
{
	return {};
}

const SVehicleDamageParams& Vehicle::GetDamageParams() const
{
	static const SVehicleDamageParams params{};
	return params;
}

void Vehicle::SetObjectUpdate(IVehicleObject* pObject, EVehicleObjectUpdate updatePolicy)
{
}

IVehicleHelper* Vehicle::GetHelper(const char* pName)
{
	return {};
}

bool Vehicle::IsDestroyed()
{
	return {};
}

bool Vehicle::IsFlipped(float maxSpeed)
{
	return {};
}

void Vehicle::TriggerEngineSlotBySpeed(bool trigger)
{
}

void Vehicle::NeedsUpdate(int flags, bool threadSafe)
{
}

int Vehicle::SetTimer(int timerId, int ms, IVehicleObject* pObject)
{
	return {};
}

int Vehicle::KillTimer(int timerId)
{
	return {};
}

IActor* Vehicle::GetDriver()
{
	return {};
}

void Vehicle::SetVehicleGlassSurfaceType(ISurfaceType* pSurface)
{
}

bool Vehicle::GetExitPositionForActor(IActor* pActor, Vec3& pos, bool extended)
{
	return {};
}

int Vehicle::GetSkipEntities(IPhysicalEntity** pSkipEnts, int nMaxSkip)
{
	return {};
}

const char* Vehicle::GetModification() const
{
	return {};
}

////////////////////////////////////////////////////////////////////////////////
// IGameObjectProfileManager
////////////////////////////////////////////////////////////////////////////////

bool Vehicle::SetAspectProfile(EEntityAspects aspect, std::uint8_t profile)
{
	if (aspect != eEA_Physics)
	{
		return false;
	}

	if (profile == 0)
	{
		m_physicalizeParams.type = PE_NONE;
		m_physicalizeParams.nSlot = -1;

		this->GetEntity()->Physicalize(m_physicalizeParams);

		return true;
	}
	else if (profile == 1)
	{
		m_physicalizeParams.type = PE_RIGID;
		m_physicalizeParams.nSlot = -1;
		m_physicalizeParams.mass = m_mass;
		m_physicalizeParams.pCar = nullptr;

		if (!m_pMovement || m_pMovement->GetPhysicalizationType() != PE_WHEELEDVEHICLE)
		{
			this->GetEntity()->Physicalize(m_physicalizeParams);
		}

		if (m_pMovement)
		{
			m_pMovement->Physicalize();
		}

		IPhysicalEntity* pPhysicalEntity = this->GetEntity()->GetPhysics();
		if (!pPhysicalEntity)
		{
			return false;
		}

		pe_params_buoyancy buoyancy(m_buoyancy);
		pPhysicalEntity->SetParams(&buoyancy);

		pe_simulation_params physics(m_physics);
		pPhysicalEntity->SetParams(&physics);

		pe_params_flags physicsParams(m_physicsParams);
		pPhysicalEntity->SetParams(&physicsParams);

		for (auto& [partName, pPart] : m_parts)
		{
			pPart->Physicalize();
		}

		this->GetGameObject()->EnablePhysicsEvent(true, eEPE_OnPostStepImmediate);

		{
			pe_params_flags newPhysicsParams;
			newPhysicsParams.flagsOR = pef_fixed_damping | pef_custom_poststep | pef_log_collisions;
			pPhysicalEntity->SetParams(&newPhysicsParams);
		}

		if (pPhysicalEntity->GetParams(&physics))
		{
			m_gravity = physics.gravity;
		}

		if (m_retainGravity)
		{
			pe_params_flags newPhysicsParams;
			newPhysicsParams.flagsOR = pef_ignore_areas;
			pPhysicalEntity->SetParams(&newPhysicsParams);
		}

		if (m_pMovement)
		{
			m_pMovement->PostPhysicalize();
		}

		if (!GameFramework::GetInstance()->IsEditing())
		{
			this->NeedsUpdate(eVUF_AwakePhysics, false);
		}

		return true;
	}

	return false;
}

std::uint8_t Vehicle::GetDefaultProfile(EEntityAspects aspect)
{
	if (aspect == eEA_Physics)
	{
		return 1;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////

void Vehicle::GetGameObjectExtensionRMIData(void** ppRMI, size_t* count)
{
	// SvRequestUse
	// SvRequestChangeSeat
	// SvRequestLeave
	// ClSetupWeapons
	// ClAmmoCounts
	// ClSetAmmo
	// ClAbandonWarning

#ifdef BUILD_64BIT
	void* pVehicleMessages = reinterpret_cast<void*>(CRYACTION_BASE + 0x422fa0);
#else
	void* pVehicleMessages = reinterpret_cast<void*>(CRYACTION_BASE + 0x2a47b8);
#endif

	*ppRMI = pVehicleMessages;
	*count = 7;
}

bool Vehicle::PatchVTable()
{
	std::uintptr_t vtable[101] = {
#ifdef BUILD_64BIT
		CRYACTION_BASE + 0x84BC0, CRYACTION_BASE + 0x6BFC0, CRYACTION_BASE + 0x22A50, CRYACTION_BASE + 0x63A00, CRYACTION_BASE + 0x81180, CRYACTION_BASE + 0x7B5C0, CRYACTION_BASE + 0x7E6C0, CRYACTION_BASE + 0x67BB0,
		CRYACTION_BASE + 0x72910, CRYACTION_BASE + 0x6BD70, CRYACTION_BASE + 0x712A0, CRYACTION_BASE + 0x74A80, CRYACTION_BASE + 0x74A10, CRYACTION_BASE + 0x6D850, CRYACTION_BASE + 0x22A50, CRYACTION_BASE + 0x7F950,
		CRYACTION_BASE + 0x66FA0, CRYACTION_BASE + 0x22A50, CRYACTION_BASE + 0x22A50, CRYACTION_BASE + 0x7CF20, CRYACTION_BASE + 0x7F940, CRYACTION_BASE + 0x63AB0, CRYACTION_BASE + 0x64400, CRYACTION_BASE + 0x716E0,
		CRYACTION_BASE + 0x753B0, CRYACTION_BASE + 0x648F0, CRYACTION_BASE + 0x72D40, CRYACTION_BASE + 0x63A70, CRYACTION_BASE + 0x63A90, CRYACTION_BASE + 0x7F900, CRYACTION_BASE + 0x7F910, CRYACTION_BASE + 0x68730,
		CRYACTION_BASE + 0x718E0, CRYACTION_BASE + 0x64600, CRYACTION_BASE + 0x793C0, CRYACTION_BASE + 0x7FA10, CRYACTION_BASE + 0x64680, CRYACTION_BASE + 0x65360, CRYACTION_BASE + 0x65390, CRYACTION_BASE + 0x654B0,
		CRYACTION_BASE + 0x64860, CRYACTION_BASE + 0x654D0, CRYACTION_BASE + 0x646E0, CRYACTION_BASE + 0x7FA30, CRYACTION_BASE + 0x65470, CRYACTION_BASE + 0x644A0, CRYACTION_BASE + 0x644D0, CRYACTION_BASE + 0x64FE0,
		CRYACTION_BASE + 0x64FA0, CRYACTION_BASE + 0x7F9F0, CRYACTION_BASE + 0x64F20, CRYACTION_BASE + 0x69870, CRYACTION_BASE + 0x65130, CRYACTION_BASE + 0x65210, CRYACTION_BASE + 0x65320, CRYACTION_BASE + 0x661E0,
		CRYACTION_BASE + 0x66280, CRYACTION_BASE + 0x65010, CRYACTION_BASE + 0x6FC20, CRYACTION_BASE + 0x64A10, CRYACTION_BASE + 0x64AE0, CRYACTION_BASE + 0x7F980, CRYACTION_BASE + 0x7F990, CRYACTION_BASE + 0x653F0,
		CRYACTION_BASE + 0x7F9A0, CRYACTION_BASE + 0x6A480, CRYACTION_BASE + 0x63AC0, CRYACTION_BASE + 0x6D1A0, CRYACTION_BASE + 0x7C3A0, CRYACTION_BASE + 0x76630, CRYACTION_BASE + 0x7F9B0, CRYACTION_BASE + 0x0CA60,
		CRYACTION_BASE + 0x77BA0, CRYACTION_BASE + 0x7A040, CRYACTION_BASE + 0x63AF0, CRYACTION_BASE + 0x65D00, CRYACTION_BASE + 0x7F9C0, CRYACTION_BASE + 0x64360, CRYACTION_BASE + 0x78CC0, CRYACTION_BASE + 0x73030,
		CRYACTION_BASE + 0x64E80, CRYACTION_BASE + 0x7F9D0, CRYACTION_BASE + 0x6DBF0, CRYACTION_BASE + 0x66660, CRYACTION_BASE + 0x66780, CRYACTION_BASE + 0x6D670, CRYACTION_BASE + 0x63A40, CRYACTION_BASE + 0x7F920,
		CRYACTION_BASE + 0x7F930, CRYACTION_BASE + 0x88060, CRYACTION_BASE + 0x7E0A0, CRYACTION_BASE + 0x7C080, CRYACTION_BASE + 0x6F4F0, CRYACTION_BASE + 0x64BA0, CRYACTION_BASE + 0x63B40, CRYACTION_BASE + 0x66360,
		CRYACTION_BASE + 0x657C0, CRYACTION_BASE + 0x65B90, CRYACTION_BASE + 0x65BE0, CRYACTION_BASE + 0x71B00, CRYACTION_BASE + 0x73130,
#else
		CRYACTION_BASE + 0x5B4E0, CRYACTION_BASE + 0x50A90, CRYACTION_BASE + 0xF9730, CRYACTION_BASE + 0x4B470, CRYACTION_BASE + 0x62D00, CRYACTION_BASE + 0x581E0, CRYACTION_BASE + 0x58D30, CRYACTION_BASE + 0x4E050,
		CRYACTION_BASE + 0x540D0, CRYACTION_BASE + 0x50A10, CRYACTION_BASE + 0x53C60, CRYACTION_BASE + 0x549F0, CRYACTION_BASE + 0x54990, CRYACTION_BASE + 0x51DC0, CRYACTION_BASE + 0xF9730, CRYACTION_BASE + 0x629D0,
		CRYACTION_BASE + 0x5E300, CRYACTION_BASE + 0x4B500, CRYACTION_BASE + 0x203E60, CRYACTION_BASE + 0x588F0, CRYACTION_BASE + 0x629C0, CRYACTION_BASE + 0x4B530, CRYACTION_BASE + 0x4BBB0, CRYACTION_BASE + 0x54500,
		CRYACTION_BASE + 0x551A0, CRYACTION_BASE + 0x4BF40, CRYACTION_BASE + 0x54140, CRYACTION_BASE + 0x4B4C0, CRYACTION_BASE + 0x4B4E0, CRYACTION_BASE + 0x62980, CRYACTION_BASE + 0x62990, CRYACTION_BASE + 0x4EA80,
		CRYACTION_BASE + 0x53F20, CRYACTION_BASE + 0x4BDB0, CRYACTION_BASE + 0x568D0, CRYACTION_BASE + 0x62A80, CRYACTION_BASE + 0x4BE00, CRYACTION_BASE + 0x4C630, CRYACTION_BASE + 0x4C660, CRYACTION_BASE + 0x4C740,
		CRYACTION_BASE + 0x4BEE0, CRYACTION_BASE + 0x4C750, CRYACTION_BASE + 0x4BE70, CRYACTION_BASE + 0x62A90, CRYACTION_BASE + 0x4C710, CRYACTION_BASE + 0x4BC10, CRYACTION_BASE + 0x4BC30, CRYACTION_BASE + 0x4C380,
		CRYACTION_BASE + 0x4C350, CRYACTION_BASE + 0x62A70, CRYACTION_BASE + 0x4C2F0, CRYACTION_BASE + 0x4FAA0, CRYACTION_BASE + 0x4C4B0, CRYACTION_BASE + 0x4C510, CRYACTION_BASE + 0x4C600, CRYACTION_BASE + 0x4D080,
		CRYACTION_BASE + 0x4D0F0, CRYACTION_BASE + 0x4C3B0, CRYACTION_BASE + 0x53410, CRYACTION_BASE + 0x4C060, CRYACTION_BASE + 0x4C100, CRYACTION_BASE + 0x629F0, CRYACTION_BASE + 0x62A00, CRYACTION_BASE + 0x4C6B0,
		CRYACTION_BASE + 0x62A10, CRYACTION_BASE + 0x50360, CRYACTION_BASE + 0x4B540, CRYACTION_BASE + 0x518F0, CRYACTION_BASE + 0x58740, CRYACTION_BASE + 0x56320, CRYACTION_BASE + 0x62A20, CRYACTION_BASE + 0x62A30,
		CRYACTION_BASE + 0x56460, CRYACTION_BASE + 0x57520, CRYACTION_BASE + 0x4B590, CRYACTION_BASE + 0x4CD80, CRYACTION_BASE + 0x62A40, CRYACTION_BASE + 0x4BB30, CRYACTION_BASE + 0x564D0, CRYACTION_BASE + 0x54380,
		CRYACTION_BASE + 0x4C2C0, CRYACTION_BASE + 0x62A50, CRYACTION_BASE + 0x52090, CRYACTION_BASE + 0x4D370, CRYACTION_BASE + 0x4D480, CRYACTION_BASE + 0x51D00, CRYACTION_BASE + 0x4B4A0, CRYACTION_BASE + 0x629A0,
		CRYACTION_BASE + 0x629B0, CRYACTION_BASE + 0x5CDC0, CRYACTION_BASE + 0x58B80, CRYACTION_BASE + 0x58510, CRYACTION_BASE + 0x530D0, CRYACTION_BASE + 0x4C1A0, CRYACTION_BASE + 0x4B5E0, CRYACTION_BASE + 0x4D1B0,
		CRYACTION_BASE + 0x4CA40, CRYACTION_BASE + 0x4CCC0, CRYACTION_BASE + 0x4CD00, CRYACTION_BASE + 0x54760, CRYACTION_BASE + 0x543C0,
#endif
	};

	std::uintptr_t* current_vtable = *reinterpret_cast<std::uintptr_t**>(this);

	// use original IGameObjectExtension and IVehicle functions from CryAction DLL except the following
	vtable[0] = current_vtable[0];  // Vehicle::Init

	WinAPI::FillMem(current_vtable, vtable, sizeof(vtable), false);

	return true;
}

void Vehicle::InitActions(const SmartScriptTable& table)
{
#ifdef BUILD_64BIT
	std::uintptr_t func = CRYACTION_BASE + 0x841f0;
#else
	std::uintptr_t func = CRYACTION_BASE + 0x5b000;
#endif

	(this->*reinterpret_cast<void(IVehicle::*&)(const SmartScriptTable&)>(func))(table);
}

void Vehicle::InitDamages(const SmartScriptTable& table)
{
#ifdef BUILD_64BIT
	std::uintptr_t func = CRYACTION_BASE + 0xa8dd0;
#else
	std::uintptr_t func = CRYACTION_BASE + 0x7b1e0;
#endif

#ifdef BUILD_64BIT
	const unsigned int thisOffset = 0x28;
#else
	const unsigned int thisOffset = 0x14;
#endif

	Vehicle* pThis = reinterpret_cast<Vehicle*>(reinterpret_cast<unsigned char*>(this) + thisOffset);

	(pThis->*reinterpret_cast<void(IVehicle::*&)(Vehicle*, const SmartScriptTable&)>(func))(this, table);
}

void Vehicle::InitParticles(const SmartScriptTable& table)
{
#ifdef BUILD_64BIT
	std::uintptr_t func = CRYACTION_BASE + 0x81430;
#else
	std::uintptr_t func = CRYACTION_BASE + 0x5a040;
#endif

	(this->*reinterpret_cast<void(IVehicle::*&)(const SmartScriptTable&)>(func))(table);
}

void Vehicle::InitHelpers()
{
#ifdef BUILD_64BIT
	std::uintptr_t func = CRYACTION_BASE + 0x6f4f0;
#else
	std::uintptr_t func = CRYACTION_BASE + 0x530d0;
#endif

	(this->*reinterpret_cast<void(IVehicle::*&)()>(func))();
}

void Vehicle::InitMaxDamage()
{
	m_totalMaxDamage = 0;
	m_majorComponentMaxDamage = 0;

	for (VehicleComponent* pComponent : m_components)
	{
		m_totalMaxDamage += pComponent->GetMaxDamage();

		if (pComponent->IsMajorComponent())
		{
			m_majorComponentMaxDamage += pComponent->GetMaxDamage();
		}
	}

	if (m_majorComponentMaxDamage > 0)
	{
		for (VehicleComponent* pComponent : m_components)
		{
			if (pComponent->IsMajorComponent())
			{
				pComponent->SetVehicleHealthProportion(
					pComponent->GetMaxDamage() / m_majorComponentMaxDamage
				);
			}
		}
	}
}

void Vehicle::InitSeatTransitions()
{
	const int seatCount = static_cast<int>(m_seats.size());

	m_seatTransitions.resize(seatCount);

	for (int i = 0; i < seatCount; i++)
	{
		IVehicleHelper* pExitHelper = m_seats[i].second->GetExitHelper();

		for (int j = 0; j < seatCount; j++)
		{
			if (i != j && m_seats[j].second->GetExitHelper() == pExitHelper && pExitHelper)
			{
				m_seatTransitions[i].push_back(j);
			}
		}
	}
}

void Vehicle::LoadParts(const SmartScriptTable& parts, IVehiclePart* parent, ComponentParts& componentParts)
{
#ifdef BUILD_64BIT
	std::uintptr_t func = CRYACTION_BASE + 0x63c80;
#else
	std::uintptr_t func = CRYACTION_BASE + 0x4b6c0;
#endif

	(this->*reinterpret_cast<void(IVehicle::*&)(const SmartScriptTable&, IVehiclePart*, ComponentParts&)>(func))(
		parts, parent, componentParts
	);
}

void Vehicle::RegisterComponentParts(const ComponentParts& componentParts)
{
	for (const auto& [pPart, componentName] : componentParts)
	{
		IVehicleComponent* pComponent = this->GetComponent(componentName.c_str());
		if (pComponent)
		{
			static_cast<VehicleComponent*>(pComponent)->AddPart(pPart);
		}
	}
}

bool Vehicle::SetMovement(const char* name, const SmartScriptTable& movement)
{
#ifdef BUILD_64BIT
	std::uintptr_t func = CRYACTION_BASE + 0x64ba0;
#else
	std::uintptr_t func = CRYACTION_BASE + 0x4c1a0;
#endif

	return (this->*reinterpret_cast<bool(IVehicle::*&)(const CryStringT<char>&, const SmartScriptTable&)>(func))(
		name, movement
	);
}

void Vehicle::SetPaintMaterial(const char* name)
{
	const bool create = false;

	IMaterial* pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(name, create);
	if (!pMaterial)
	{
		CryLogErrorAlways("[Vehicle::SetPaintMaterial] Cannot find material %s", name);
		return;
	}

	this->GetEntity()->SetMaterial(pMaterial);
}

void Vehicle::SetAmmoCapacity(const char* name, int capacity)
{
	IEntityClass* pAmmoClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
	if (!pAmmoClass)
	{
		CryLogErrorAlways("[Vehicle::SetAmmoCapacity] Cannot find ammo class %s", name);
		return;
	}

	m_pInventory->SetAmmoCapacity(pAmmoClass, capacity);
}

void Vehicle::AttachScriptBindToSeats()
{
	ScriptBind_VehicleSeat* pScriptBind_VehicleSeat = GameFramework::GetInstance()->GetScriptBind_VehicleSeat();

	int seatId = 1;
	for (const auto& [name, pSeat] : m_seats)
	{
		pScriptBind_VehicleSeat->AttachTo(this, seatId);
		seatId++;
	}
}

bool Vehicle::BindVehicleToNetwork()
{
	if (this->GetEntity()->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY))
	{
		return true;
	}

	if (!this->GetGameObject()->BindToNetwork())
	{
		CryLogErrorAlways("[Vehicle::BindToNetwork] Failed for %s", this->GetEntity()->GetName());
		return false;
	}

	return true;
}

void Vehicle::InitRespawn()
{
	if (!gEnv->bServer)
	{
		return;
	}

	IScriptTable* pScriptTable = this->GetEntity()->GetScriptTable();
	SmartScriptTable propertiesTable;
	SmartScriptTable respawnTable;
	int respawn = 0;
	if (!pScriptTable
	 || !pScriptTable->GetValue("Properties", propertiesTable)
	 || !propertiesTable->GetValue("Respawn", respawnTable)
	 || !respawnTable->GetValue("bRespawn", respawn)
	 || !respawn)
	{
		return;
	}

	IGameRules* pGameRules = GameFramework::GetInstance()->GetIGameRulesSystem()->GetCurrentGameRules();
	if (!pGameRules)
	{
		return;
	}

	pGameRules->CreateEntityRespawnData(this->GetEntityId());
}

void Vehicle::InitModName()
{
	IScriptTable* pScriptTable = this->GetEntity()->GetScriptTable();
	SmartScriptTable propertiesTable;
	const char* mod = nullptr;
	if (!pScriptTable
	 || !pScriptTable->GetValue("Properties", propertiesTable)
	 || !propertiesTable->GetValue("Modification", mod))
	{
		return;
	}

	m_modName = mod;
}

void Vehicle::InitPaintName()
{
	IScriptTable* pScriptTable = this->GetEntity()->GetScriptTable();
	SmartScriptTable propertiesTable;
	const char* paint = nullptr;
	if (!pScriptTable
	 || !pScriptTable->GetValue("Properties", propertiesTable)
	 || !propertiesTable->GetValue("Paint", paint))
	{
		return;
	}

	m_paintName = paint;
}
