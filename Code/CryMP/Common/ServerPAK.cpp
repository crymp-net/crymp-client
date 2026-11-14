#include "CryCommon/CryAnimation/ICryAnimation.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CryNetwork/INetwork.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CryAction/IMaterialEffects.h"
#include "CryGame/Game.h"
#include "CryGame/Items/ItemSharedParams.h"
#include "CryGame/Items/Weapons/WeaponSystem.h"
#include "CryGame/Actors/Player/NanoSuit.h" 
#include "CrySystem/CryPak.h"

#include "ServerPAK.h"
#include "Utilities.h"

ServerPAK::ServerPAK()
{
}

ServerPAK::~ServerPAK()
{
}

bool ServerPAK::Load(const std::string& path)
{
	Unload();

	const bool opened = CryPak::GetInstance().LoadServerPak(path);
	if (opened)
	{
		CryLogAlways("$3[CryMP] [ServerPAK] Loaded $6%s", path.c_str());
		m_path = path;
	}
	else
	{
		CryLogAlways("$4[CryMP] [ServerPAK] Failed to load $8%s", path.c_str());
	}

	return opened;
}

bool ServerPAK::Unload()
{
	if (m_path.empty())
	{
		return false;
	}

	const bool closed = CryPak::GetInstance().UnloadServerPak(m_path);
	if (closed)
	{
		CryLogAlways("$3[CryMP] [ServerPAK] Unloaded $6%s", m_path.c_str());
		m_path.clear();
	}
	else
	{
		CryLogAlways("$4[CryMP] [ServerPAK] Failed to unload $8%s", m_path.c_str());
	}

	return closed;
}

void ServerPAK::OnFirstLoadingProgress()
{
	this->ReloadCgaCache();

	CNanoSuit::ResetCachedMaterials();

	if (ICVar* pReloadShaders = gEnv->pConsole->GetCVar("r_ReloadShaders"))
	{
		CryLogAlways("$3[CryMP] [ServerPAK] Reloading shaders");

		const int flags = pReloadShaders->GetFlags();
		pReloadShaders->SetFlags((flags & ~VF_CHEAT) | VF_NOT_NET_SYNCED);
		pReloadShaders->Set(0x10); // FRO_FORCERELOAD
		pReloadShaders->SetFlags(flags);
	}
}

void ServerPAK::OnDisconnect(int reason, const char* message)
{
	gEnv->pScriptSystem->ResetTimers();

	IGameFramework* pGameFrameWork = gEnv->pGame->GetIGameFramework();
	IItemSystem* pItemSystem = pGameFrameWork->GetIItemSystem();
	pItemSystem->ClearGeometryCache();
	pItemSystem->ClearSoundCache();

	Unload();
}

void ServerPAK::ResetSubSystems()
{
	IGameFramework* pGameFrameWork = gEnv->pGame->GetIGameFramework();

	//Reset a bunch of subsystems
	ResetGameObjectSystem();
	gEnv->p3DEngine->ResetPostEffects();
	gEnv->p3DEngine->ResetParticlesAndDecals();
	pGameFrameWork->ResetBrokenGameObjects();
	gEnv->pPhysicalWorld->ResetDynamicEntities();
	//gEnv->pFlowSystem->Reset();
	g_pGame->GetItemSharedParamsList()->Reset();
	g_pGame->GetIGameFramework()->GetIItemSystem()->Reload();
	g_pGame->GetWeaponSystem()->Reload();
	//gEnv->pDialogSystem->Reset();
	pGameFrameWork->GetIMaterialEffects()->Reset();
	pGameFrameWork->GetIVehicleSystem()->Reset();

	//Reset scripts to avoid disconnect bugs caused by possible new rmis in PAK
	IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();

	//Parent classes: BasicEntity and Chickens are parent classes and have to be loaded before others
	IEntityClass *pBasicEntityClass = pClassRegistry->FindClass("BasicEntity");
	if (pBasicEntityClass)
	{
		pBasicEntityClass->LoadScript(true);
	}
	IEntityClass* pChickensClass = pClassRegistry->FindClass("Chickens");
	if (pChickensClass)
	{
		pChickensClass->LoadScript(true);
	}

	IEntityClass* pGUI = pClassRegistry->FindClass("GUI");

	std::vector<IEntityClass*> classes;
	pClassRegistry->IteratorMoveFirst();
	IEntityClass* pEntityClass = nullptr;
	int counter = 0;
	while ((pEntityClass = pClassRegistry->IteratorNext()) != nullptr)
	{
		const char* file = pEntityClass->GetScriptFile();
		if (strlen(file) > 0)
		{
			if (pEntityClass == pBasicEntityClass || pEntityClass == pChickensClass)
				continue;

			//GUI corrupts Lua environment :S Hope there aren't any others..
			if (pEntityClass == pGUI)
				continue;

			SmartScriptTable entityTable;
			if (!gEnv->pScriptSystem->GetGlobalValue(pEntityClass->GetName(), entityTable))
				continue;

			classes.push_back(pEntityClass);
		}
	}

	for (IEntityClass* pClass : classes)
	{
		const bool bReloaded = pClass->LoadScript(true);
		if (bReloaded)
		{
			++counter;
		}
	}

	CryLogAlways("$3[CryMP] Reset subsystems and %d scripts", counter);
}

void ServerPAK::ReloadCgaCache()
{
	// Instantiate all vehicle CGAs to preload vehicle assets
	static constexpr const char* VEHICLE_CGAS[] = {
		//"objects/characters/alien/warrior/warrior_wb_v2.cga",
		"Objects/Library/Architecture/Aircraftcarrier/props/trolley/bigtrolley_useable.cga",
		"Objects/Vehicles/Asian_AAA/asian_aaa_damaged.cga",
		"Objects/Vehicles/Asian_AAA/asian_aaa.cga",
		"Objects/Vehicles/asian_apc/asian_apc_damaged.cga",
		"Objects/Vehicles/asian_apc/asian_apc.cga",
		//"Objects/Vehicles/asian_helicopter_low_budget/asian_helicopter_low_budget_flying.cga",
		"Objects/Vehicles/Asian_Helicopter/asian_helicopter_destroyed.cga",
		"Objects/Vehicles/Asian_Helicopter/asian_helicopter.cga",
		"Objects/Vehicles/Asian_patrolboat/asian_patrolboat_damaged.cga",
		"Objects/Vehicles/Asian_patrolboat/asian_patrolboat.cga",
		"objects/vehicles/asian_smallboat/asian_smallboat_damaged.cga",
		"objects/vehicles/asian_smallboat/asian_smallboat.cga",
		"Objects/Vehicles/asian_tank/asian_tank_damaged.cga",
		"Objects/Vehicles/asian_tank/asian_tank.cga",
		"Objects/Vehicles/Asian_Truck_B/Asian_Truck_b_damaged.cga",
		"Objects/Vehicles/Asian_Truck_B/Asian_Truck_b.cga",
		"Objects/Vehicles/Civ_car1/Civ_car_damaged.cga",
		"Objects/Vehicles/Civ_car1/Civ_car.cga",
		"Objects/Vehicles/ltv/ltv_damaged.cga",
		"Objects/Vehicles/ltv/ltv.cga",
		"Objects/Vehicles/speedboat/speedboat_asian_damaged.cga",
		"Objects/Vehicles/speedboat/speedboat_asian.cga",
		"Objects/Vehicles/speedboat/speedboat_damaged.cga",
		"Objects/Vehicles/speedboat/speedboat.cga",
		"Objects/Vehicles/us_apc/us_apc_damaged.cga",
		"Objects/Vehicles/us_apc/us_apc.cga",
		"Objects/Vehicles/US_Hovercraft_B/US_Hovercraft_B_destroyed.cga",
		"Objects/Vehicles/US_Hovercraft_B/US_Hovercraft_B.cga",
		"Objects/Vehicles/US_Smallboat/US_Smallboat_damaged.cga",
		"Objects/Vehicles/US_Smallboat/US_Smallboat.cga",
		"Objects/Vehicles/us_tank/us_tank_damaged.cga",
		"Objects/Vehicles/us_tank/us_tank.cga",
		"Objects/Vehicles/US_VTOL_Transport/US_VTOL_Transport_destroyed.cga",
		"Objects/Vehicles/US_VTOL_Transport/US_VTOL_Transport.cga",
		"Objects/Vehicles/US_Vtol/US_Vtol_destroyed.cga",
		"Objects/Vehicles/US_Vtol/US_Vtol.cga",
	};

	m_cgaCache.clear();
	m_cgaCache.reserve(std::size(VEHICLE_CGAS));

	for (const char* cga : VEHICLE_CGAS)
	{
		ICharacterInstance* pInstance = gEnv->pCharacterManager->CreateInstance(cga);
		if (pInstance)
		{
			pInstance->AddRef();
			m_cgaCache.emplace_back(pInstance);
		}
	}
}
