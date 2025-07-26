/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 18:10:2005   18:00 : Created by Márcio Martins

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryGame/Game.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryCommon/CrySystem/CryFind.h"
#include "CryCommon/CrySystem/CryPath.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryCommon/CryAction/IGameObject.h"
#include "CryGame/Actors/Actor.h"
#include "CryMP/Client/WeatherSystem.h"
#include "WeaponSystem.h"

#include "Projectile.h"
#include "Projectiles/Bullet.h"
#include "Projectiles/Rock.h"
#include "Projectiles/Rocket.h"
#include "Projectiles/HomingMissile.h"
#include "Projectiles/TacBullet.h"
#include "Projectiles/TagBullet.h"
#include "Projectiles/AVMine.h"
#include "Projectiles/Claymore.h"
//#include "Projectiles/EMPField.h"
#include "Projectiles/C4Projectile.h"

#include "FireModes/Single.h"
#include "FireModes/Automatic.h"
#include "FireModes/Burst.h"
#include "FireModes/Rapid.h"
#include "FireModes/Throw.h"
#include "FireModes/Plant.h"
#include "FireModes/Detonate.h"
#include "FireModes/FreezingBeam.h"
#include "FireModes/Charge.h"
#include "FireModes/Shotgun.h"
#include "FireModes/Melee.h"
#include "FireModes/WorkOnTarget.h"
#include "FireModes/Scan.h"
#include "FireModes/SingleTG.h"
#include "../ItemSharedParams.h"


#include "ZoomModes/IronSight.h"
#include "ZoomModes/Scope.h"
#include "CryGame/GameCVars.h"

template <typename T, typename R> R *CreateIt() { return new T(); };



#define REGISTER_PROJECTILE(name, T)	\
struct C##name##Creator : public IGameObjectExtensionCreatorBase	\
{ \
	T *Create() \
	{ \
		return new T(); \
	} \
	void GetGameObjectExtensionRMIData( void ** ppRMI, size_t * nCount ) \
	{ \
		T::GetGameObjectExtensionRMIData( ppRMI, nCount ); \
	} \
}; \
static C##name##Creator _##name##Creator; \
RegisterProjectile(#name, &_##name##Creator);

//------------------------------------------------------------------------
CWeaponSystem::CWeaponSystem(CGame *pGame, ISystem *pSystem)
: m_pGame(pGame),
	m_pSystem(pSystem),
	m_pItemSystem(pGame->GetIGameFramework()->GetIItemSystem()),
	m_pPrecache(0),
	m_reloading(false),
	m_frozenEnvironment(false),
	m_wetEnvironment(false),
	m_tokensUpdated(false)
{
	// register fire modes here
	RegisterFireMode("Single", &CreateIt<CSingle, IFireMode>);
	RegisterFireMode("Automatic", &CreateIt<CAutomatic, IFireMode>);
	RegisterFireMode("Burst", &CreateIt<CBurst, IFireMode>);
	RegisterFireMode("Rapid", &CreateIt<CRapid, IFireMode>);
	RegisterFireMode("Throw", &CreateIt<CThrow, IFireMode>);
	RegisterFireMode("Plant", &CreateIt<CPlant, IFireMode>);
	RegisterFireMode("Detonate", &CreateIt<CDetonate, IFireMode>);
	RegisterFireMode("FreezingBeam", &CreateIt<CFreezingBeam, IFireMode>);
	RegisterFireMode("Charge", &CreateIt<CCharge, IFireMode>);
	RegisterFireMode("Shotgun", &CreateIt<CShotgun, IFireMode>);	
	RegisterFireMode("Melee", &CreateIt<CMelee, IFireMode>);
	RegisterFireMode("WorkOnTarget", &CreateIt<CWorkOnTarget, IFireMode>);
	RegisterFireMode("Scan", &CreateIt<CScan, IFireMode>);
	RegisterFireMode("SingleTG", &CreateIt<CSingleTG, IFireMode>);
  
	// register zoom modes here
	RegisterZoomMode("IronSight", &CreateIt<CIronSight, IZoomMode>);
	RegisterZoomMode("Scope", &CreateIt<CScope, IZoomMode>);

	// register projectile classes here
	REGISTER_PROJECTILE(Projectile, CProjectile);
	REGISTER_PROJECTILE(Bullet, CBullet);
	REGISTER_PROJECTILE(Rock, CRock);
	REGISTER_PROJECTILE(Rocket, CRocket);
  REGISTER_PROJECTILE(HomingMissile, CHomingMissile);
	REGISTER_PROJECTILE(TacBullet, CTacBullet);
	REGISTER_PROJECTILE(TagBullet, CTagBullet);
	REGISTER_PROJECTILE(AVExplosive, CAVMine);
	REGISTER_PROJECTILE(ClaymoreExplosive, CClaymore);
	//REGISTER_PROJECTILE(EMPField, CEMPField);
	REGISTER_PROJECTILE(C4Projectile, CC4Projectile); 

	m_pPrecache = gEnv->pConsole->GetCVar("i_precache");

	CBullet::EntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Bullet");
	CBullet::SetWaterMaterialId();

	m_pGame->GetIGameFramework()->GetILevelSystem()->AddListener(this);

	gEnv->pConsole->AddCommand(
		"ws_dumpAmmoPools",
		[](IConsoleCmdArgs* pArgs)
		{
			if (g_pGame && g_pGame->GetWeaponSystem())
			{
				g_pGame->GetWeaponSystem()->DumpPoolSizes();
			}
		},
		0,
		"Dump current ammo pool sizes for debugging"
	);
}

//------------------------------------------------------------------------
CWeaponSystem::~CWeaponSystem()
{
	// cleanup current projectiles
	for (TProjectileMap::iterator pit = m_projectiles.begin(); pit != m_projectiles.end(); ++pit)
		gEnv->pEntitySystem->RemoveEntity(pit->first, true);

	for (TAmmoTypeParams::iterator it = m_ammoparams.begin(); it != m_ammoparams.end(); ++it)
	{
		SAmmoTypeDesc &desc=it->second;
		delete desc.params;

		if (!desc.configurations.empty())
		{
			for (std::map<string, const SAmmoParams *>::iterator ait=desc.configurations.begin(); ait!=desc.configurations.end(); ait++)
				delete ait->second;
		}
	}

	m_pGame->GetIGameFramework()->GetILevelSystem()->RemoveListener(this);
}

//------------------------------------------------------------------------
void CWeaponSystem::Update(float frameTime)
{
	if (gEnv->bClient)
	{
		m_tracerManager.Update(frameTime);
	}
	CheckEnvironmentChanges();
}

//------------------------------------------------------------------------
void CWeaponSystem::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CWeaponSystem::Reload()
{
	m_reloading = true;

	// cleanup current projectiles
	for (TProjectileMap::iterator pit = m_projectiles.begin(); pit != m_projectiles.end();)
	{
		//Bugfix: RemoveEntity removes projectile from map, thus invalidating iterator
		TProjectileMap::iterator next = pit;        
		next++;
		gEnv->pEntitySystem->RemoveEntity(pit->first, true);
		pit = next;
	}
	m_projectiles.clear();

	for (TAmmoTypeParams::iterator it = m_ammoparams.begin(); it != m_ammoparams.end(); ++it)
	{
		SAmmoTypeDesc &desc=it->second;
		delete desc.params;
		if (!desc.configurations.empty())
		{
			for (std::map<string, const SAmmoParams *>::iterator ait=desc.configurations.begin(); ait!=desc.configurations.end(); ait++)
				delete ait->second;
		}
	}

	m_ammoparams.clear();

	m_tracerManager.Reset();

	this->RegisterXMLData();

	m_reloading = false;
}

//------------------------------------------------------------------------
void CWeaponSystem::OnLoadingStart(ILevelInfo *pLevel)
{
	if (gEnv->bMultiplayer)
		SetConfiguration("mp");
	else
		SetConfiguration("");

	// force shared item params to be refreshed
	g_pGame->GetItemSharedParamsList()->Reset();
}

//------------------------------------------------------------------------
void CWeaponSystem::OnLoadingComplete(ILevel *pLevel)
{
	// marcio: precaching of items enabled by default for now
//	ICVar *sys_preload=m_pSystem->GetIConsole()->GetCVar("sys_preload");
//	if ((!sys_preload || sys_preload->GetIVal()) && m_pPrecache->GetIVal())
	{
		for (TAmmoTypeParams::iterator it=m_ammoparams.begin(); it!=m_ammoparams.end(); ++it)
		{
			const SAmmoParams *pParams=GetAmmoParams(it->first);
			const IItemParamsNode *params = pParams->pItemParams;
			const IItemParamsNode *geometry = params?params->GetChild("geometry"):0;

			m_pItemSystem->CacheGeometry(geometry);

			// Preload particle assets.
			for (int ch = params->GetChildCount()-1; ch >= 0; ch--)
			{
				const IItemParamsNode *child = params->GetChild(ch);
				if (child)
				{
					CItemParamReader reader(child);
					const char *effect = 0;
					reader.Read("effect", effect);
					if (effect && *effect)
						gEnv->p3DEngine->FindParticleEffect(effect, "WeaponSystem");
				}
			}
		}
	}	

	if(!m_tokensUpdated)
	{
		m_wetEnvironment = m_frozenEnvironment = false;
		ApplyEnvironmentChanges(); //Reset on loading new level
		CreateEnvironmentGameTokens(m_frozenEnvironment,m_wetEnvironment); //Do not force set/creation if exit
	}
	m_tokensUpdated = false;
	
}

//------------------------------------------------------------------------
IFireMode *CWeaponSystem::CreateFireMode(const char *name)
{
	TFireModeRegistry::iterator it = m_fmregistry.find(CONST_TEMP_STRING(name));
	if (it != m_fmregistry.end())
		return it->second();
	return 0;
}

//------------------------------------------------------------------------
void CWeaponSystem::RegisterFireMode(const char *name, IFireMode *(*CreateProc)())
{
	m_fmregistry.insert(TFireModeRegistry::value_type(name, CreateProc));
}

//------------------------------------------------------------------------
IZoomMode *CWeaponSystem::CreateZoomMode(const char *name)
{
	TZoomModeRegistry::iterator it = m_zmregistry.find(CONST_TEMP_STRING(name));
	if (it != m_zmregistry.end())
		return it->second();
	return 0;
}

//------------------------------------------------------------------------
void CWeaponSystem::RegisterZoomMode(const char *name, IZoomMode *(*CreateProc)())
{
	m_zmregistry.insert(TZoomModeRegistry::value_type(name, CreateProc));
}

//------------------------------------------------------------------------
CProjectile *CWeaponSystem::SpawnAmmo(IEntityClass* pAmmoType, bool isRemote, EntityId hostId)
{
	TAmmoTypeParams::const_iterator it = m_ammoparams.find(pAmmoType);
	if (it == m_ammoparams.end())
	{
		CryLogWarning("Failed to spawn ammo '%s'! Unknown class or entity class not registered...", pAmmoType?pAmmoType->GetName():"");
		return 0;
	}

	const SAmmoParams* pAmmoParams = it->second.params;
	if (!m_config.empty())
	{
		std::map<string, const SAmmoParams *>::const_iterator cit=it->second.configurations.find(m_config);
		if (cit != it->second.configurations.end())
			pAmmoParams=cit->second;
		else
			pAmmoParams=it->second.params;
	}

	m_lastHostId = hostId;

	const bool reusable = pAmmoParams->flags & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY);
	if (reusable && g_pGameCVars->mp_recycleProjectiles)
	{
		if (isRemote || (!pAmmoParams->serverSpawn && reusable))
		{
			return UseFromPool(pAmmoType, pAmmoParams);
		}
	}

	return DoSpawnAmmo(pAmmoType, isRemote, pAmmoParams);
}


//------------------------------------------------------------------------
CProjectile *CWeaponSystem::DoSpawnAmmo(IEntityClass* pAmmoType, bool isRemote, const SAmmoParams *pAmmoParams)
{
	bool isServer=gEnv->bServer;
	bool isClient=gEnv->bClient;

	if ( pAmmoParams->serverSpawn && (!isServer || IsDemoPlayback()) )
	{
		if (!pAmmoParams->predictSpawn || isRemote)
			return 0;
	}

	SEntitySpawnParams spawnParams;
	spawnParams.pClass = pAmmoType;
	spawnParams.sName = "ammo";
	spawnParams.nFlags = pAmmoParams->flags | ENTITY_FLAG_NO_PROXIMITY; // No proximity for this entity.
	
	IEntity *pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams);
	if (!pEntity)
	{
		CryLogWarning("Failed to spawn ammo '%s'! Entity creation failed...", pAmmoType->GetName());
		return 0;
	}
	
	m_lastHostId = 0;

	CProjectile *pProjectile = GetProjectile(pEntity->GetId());

	if (pProjectile && !isServer && !isRemote && pAmmoParams->predictSpawn)
	{
		pProjectile->GetGameObject()->RegisterAsPredicted();
	}
	
	return pProjectile;
}

//------------------------------------------------------------------------
bool CWeaponSystem::IsServerSpawn(IEntityClass* pAmmoType) const
{
	if (!pAmmoType)
		return false;

	if (const SAmmoParams *pAmmoParams=GetAmmoParams(pAmmoType))
		return pAmmoParams->serverSpawn!=0;
	return false;
}

//-------------------------------------------	-----------------------------
void CWeaponSystem::RegisterProjectile(const char *name, IGameObjectExtensionCreatorBase *pCreator)
{
	m_projectileregistry.insert(TProjectileRegistry::value_type(name, pCreator));
}

//------------------------------------------------------------------------
const SAmmoParams* CWeaponSystem::GetAmmoParams(IEntityClass* pAmmoType) const
{
	TAmmoTypeParams::const_iterator it=m_ammoparams.find(pAmmoType);
	if (it==m_ammoparams.end())
		return 0;

	if (m_config.empty())
		return it->second.params;

	std::map<string, const SAmmoParams *>::const_iterator cit=it->second.configurations.find(m_config);
	if (cit != it->second.configurations.end())
		return cit->second;

	return it->second.params;
}

//------------------------------------------------------------------------
void CWeaponSystem::AddProjectile(IEntity *pEntity, CProjectile *pProjectile)
{
	m_projectiles.insert(TProjectileMap::value_type(pEntity->GetId(), pProjectile));
}

//------------------------------------------------------------------------
void CWeaponSystem::RemoveProjectile(CProjectile *pProjectile)
{
	m_projectiles.erase(pProjectile->GetEntity()->GetId());

	RemoveFromPool(pProjectile);
}

//------------------------------------------------------------------------
CProjectile *CWeaponSystem::GetProjectile(EntityId entityId)
{
	TProjectileMap::iterator it = m_projectiles.find(entityId);
	if (it != m_projectiles.end())
		return it->second;
	return 0;
}

//------------------------------------------------------------------------
int  CWeaponSystem::QueryProjectiles(SProjectileQuery& q)
{
    IEntityClass* pClass = q.ammoName?gEnv->pEntitySystem->GetClassRegistry()->FindClass(q.ammoName):0;
    m_queryResults.resize(0);
    if(q.box.IsEmpty())
    {
        for(TProjectileMap::iterator it = m_projectiles.begin();it!=m_projectiles.end();++it)
        {
            IEntity *pEntity = it->second->GetEntity();
            if(pClass == 0 || pEntity->GetClass() == pClass)
            m_queryResults.push_back(pEntity);
        }
    }
    else
    {
        for(TProjectileMap::iterator it = m_projectiles.begin();it!=m_projectiles.end();++it)
        {
            IEntity *pEntity = it->second->GetEntity();
            if(q.box.IsContainPoint(pEntity->GetWorldPos()))
            {
                m_queryResults.push_back(pEntity);
            }
        }
    }
    
    q.nCount = int(m_queryResults.size());
    if(q.nCount)
        q.pResults = &m_queryResults[0];
    return q.nCount;
}

//------------------------------------------------------------------------
void CWeaponSystem::RegisterAmmo(const char* name, const char* className, const char* script, const char* config, IItemParamsNode* params)
{
	auto it = m_projectileregistry.find(CONST_TEMP_STRING(className));
	if (it == m_projectileregistry.end())
	{
		CryLogWarningAlways("Unknown ammo class '%s'! Skipping...", className);
	}

	IEntityClassRegistry::SEntityClassDesc classDesc;
	classDesc.sName = name;
	classDesc.sScriptFile = script;
	classDesc.flags |= ECLF_INVISIBLE;

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);

	if (!m_reloading && !pClass)
	{
		m_pGame->GetIGameFramework()->GetIGameObjectSystem()->RegisterExtension(name, it->second, &classDesc);
		pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
		assert(pClass);
	}

	auto ait = m_ammoparams.find(pClass);
	if (ait == m_ammoparams.end())
	{
		auto result = m_ammoparams.insert(TAmmoTypeParams::value_type(pClass, SAmmoTypeDesc()));
		ait = result.first;
	}

	SAmmoParams* pAmmoParams = new SAmmoParams(params, pClass);
	SAmmoTypeDesc& desc = ait->second;

	if (!config || !config[0])
	{
		if (desc.params)
		{
			delete desc.params;
		}

		desc.params=pAmmoParams;
	}
	else
	{
		desc.configurations.insert(std::make_pair<string, const SAmmoParams*>(config, static_cast<const SAmmoParams*>(pAmmoParams)));
	}
}

//------------------------------------------------------------------------
void CWeaponSystem::DebugGun(IConsoleCmdArgs *args)
{
  IGameFramework* pGF = gEnv->pGame->GetIGameFramework();  
  IItemSystem* pItemSystem = pGF->GetIItemSystem();
 
  IActor* pActor = pGF->GetClientActor();
  if (!pActor || !pActor->IsPlayer())
    return;

  IInventory *pInventory = pActor->GetInventory();
  if (!pInventory)
    return;  
  
  // give & select the debuggun 
	EntityId itemId = pInventory->GetItemByClass(CItem::sDebugGunClass);
  if (0 == itemId)        
  {
    // if actor doesn't have it, only give it in editor
    if (!GetISystem()->IsEditor())
      return;

		itemId = pItemSystem->GiveItem(pActor, CItem::sDebugGunClass->GetName(), false, true, true);
  }
  pItemSystem->SetActorItem(pActor, itemId, true);      
}

//------------------------------------------------------------------------
void CWeaponSystem::RefGun(IConsoleCmdArgs *args)
{
	IGameFramework* pGF = gEnv->pGame->GetIGameFramework();  
	IItemSystem* pItemSystem = pGF->GetIItemSystem();

	IActor* pActor = pGF->GetClientActor();
	if (!pActor || !pActor->IsPlayer())
		return;

	IInventory *pInventory = pActor->GetInventory();
	if (!pInventory)
		return;

	// give & select the refgun 
	EntityId itemId = pInventory->GetItemByClass(CItem::sRefWeaponClass);
	if (0 == itemId)        
	{
		// if actor doesn't have it, only give it in editor
		if (!GetISystem()->IsEditor())
			return;

		itemId = pItemSystem->GiveItem(pActor, CItem::sRefWeaponClass->GetName(), false, true, true);
	}
	pItemSystem->SetActorItem(pActor, itemId, true);   

}

//---------------------------------------------------------------
void CWeaponSystem::CreateEnvironmentGameTokens(bool frozenEnvironment, bool wetEnvironment)
{
	if(gEnv->bMultiplayer)
		return;

	IGameTokenSystem *pGameTokenSystem = gEnv->pGame->GetIGameFramework()->GetIGameTokenSystem();

	// create the game tokens if not present
	if(pGameTokenSystem)
	{
		pGameTokenSystem->SetOrCreateToken("weapon.effects.ice", TFlowInputData(frozenEnvironment));
		pGameTokenSystem->SetOrCreateToken("weapon.effects.wet", TFlowInputData(wetEnvironment));
	}
}

//--------------------------------------------------------------
void CWeaponSystem::ApplyEnvironmentChanges()
{
	IGameFramework* pGF = gEnv->pGame->GetIGameFramework();
	IActorSystem* pAS = pGF->GetIActorSystem();

	int count = pAS->GetActorCount();
	if (count)
	{
		IActorIteratorPtr it = pAS->CreateActorIterator();
		while (CActor* pActor = (CActor*)it->Next())
		{
			CPlayer* pPlayer = CPlayer::FromActor(pActor);
			if (pPlayer && pPlayer->GetNanoSuit())
			{
				// go through all players and turn their ice effects on/off
				IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)pPlayer->GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
				if (pRenderProxy)
				{
					uint8 mask = pRenderProxy->GetMaterialLayersMask();
					uint32 blend = pRenderProxy->GetMaterialLayersBlend();
					mask = IsFrozenEnvironment() ? mask|MTL_LAYER_DYNAMICFROZEN : mask&~MTL_LAYER_DYNAMICFROZEN;
					mask = IsWetEnvironment() ? mask|MTL_LAYER_WET : mask&~MTL_LAYER_WET;
					pRenderProxy->SetMaterialLayersMask(mask);
					pRenderProxy->SetMaterialLayersBlend(blend);
					if (CItem* pItem = static_cast<CItem*>(pPlayer->GetCurrentItem(true)))
					{
						pItem->FrostSync(true);
						pItem->WetSync(true);
					}
					if (COffHand* pOffHand = static_cast<COffHand*>(pPlayer->GetItemByClass(CItem::sOffHandClass)))
					{
						pOffHand->FrostSync(pOffHand->GetOffHandState() != eOHS_INIT_STATE);
						pOffHand->WetSync(pOffHand->GetOffHandState() != eOHS_INIT_STATE);
					}
				}
			}
		}
	}
}

//--------------------------------------------------------------
void CWeaponSystem::CheckEnvironmentChanges()	
{
	if(gEnv->bMultiplayer)
		return;

	IGameTokenSystem *pGameTokenSystem = gEnv->pGame->GetIGameFramework()->GetIGameTokenSystem();

	bool frozenEnvironment = m_frozenEnvironment;
	bool wetEnvironment = m_wetEnvironment;
	m_tokensUpdated = false;

	if(pGameTokenSystem)
	{
		// next call will leave value unchanged if not found
		pGameTokenSystem->GetTokenValueAs("weapon.effects.ice", frozenEnvironment);
		pGameTokenSystem->GetTokenValueAs("weapon.effects.wet", wetEnvironment);
	}

	CWeatherSystem* pWS = g_pGame->GetWeatherSystem();

	if (pWS) {
		frozenEnvironment = pWS->IsFrozen();
		wetEnvironment = pWS->IsWet();
	}

	if(m_frozenEnvironment!=frozenEnvironment)
	{
		m_frozenEnvironment = frozenEnvironment;
		ApplyEnvironmentChanges();
	}
	if(m_wetEnvironment!=wetEnvironment)
	{
		m_wetEnvironment = wetEnvironment;
		ApplyEnvironmentChanges();
	}

}

//------------------------------------------------------------------------
CProjectile* CWeaponSystem::UseFromPool(IEntityClass* pClass, const SAmmoParams* pAmmoParams)
{
	ProjectilePool& pool = m_projectilePools[pClass];
	if (pool.freeProjectiles.empty())
	{
		CProjectile* pProjectile = DoSpawnAmmo(pClass, false, pAmmoParams);
		pool.totalCount++;

		return pProjectile;
	}

	// use hot projectiles (last inserted) first to potentially reduce CPU cache misses
	CProjectile* pProjectile = pool.freeProjectiles.back().release();
	pool.freeProjectiles.pop_back();

	pProjectile->GetEntity()->Hide(false);
	pProjectile->ReInitFromPool();

	m_projectilesRecycled++;

	return pProjectile;
}

//------------------------------------------------------------------------
bool CWeaponSystem::ReturnToPool(CProjectile* pProjectile)
{
	IEntityClass* pClass = pProjectile->GetEntity()->GetClass();

	const auto it = m_projectilePools.find(pClass);
	if (it == m_projectilePools.end())
	{
		const char* name = pClass ? pClass->GetName() : "NULL";
		CryLogWarningAlways("%s: Missing pool for projectile class %s", __FUNCTION__, name);
		return false;
	}

	it->second.freeProjectiles.emplace_back(SmartProjectile(pProjectile));

	pProjectile->GetEntity()->Hide(true);
	pProjectile->GetEntity()->SetWorldTM(IDENTITY);

	return true;
}

//------------------------------------------------------------------------
void CWeaponSystem::RemoveFromPool(CProjectile* pProjectile)
{
	IEntityClass* pClass = pProjectile->GetEntity()->GetClass();

	const auto it = m_projectilePools.find(pClass);
	if (it == m_projectilePools.end())
	{
		return;
	}

	std::erase_if(it->second.freeProjectiles, [&](const SmartProjectile& x) { return x.get() == pProjectile; });
	it->second.totalCount--;
}

//------------------------------------------------------------------------
void CWeaponSystem::DumpPoolSizes()
{
	CryLogAlways("------------------------ Projectile Recycler Statistics ------------------------");

	CryLogAlways("Recycling %s", g_pGameCVars->mp_recycleProjectiles ? "enabled" : "disabled");
	CryLogAlways("Projectiles recycled: %zu", m_projectilesRecycled);

	for (const auto& [pClass, pool] : m_projectilePools)
	{
		const char* name = pClass ? pClass->GetName() : "NULL";
		CryLogAlways("%s: totalCount=%zu, freeCount=%zu", name, pool.totalCount, pool.freeProjectiles.size());
	}

	CryLogAlways("--------------------------------------------------------------------------------");
}

//----------------------------------------
void CWeaponSystem::Serialize(TSerialize ser)
{
	ser.Value("m_wetEnvironment",m_wetEnvironment);
	ser.Value("m_frozenEnvironment",m_frozenEnvironment);
	
	if(ser.IsReading())
	{
		CreateEnvironmentGameTokens(m_frozenEnvironment,m_wetEnvironment);
		m_tokensUpdated = true;
	}
}

void CWeaponSystem::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "WeaponSystem");
	int nSize = sizeof(*this);
	s->AddObject(this,nSize);

	m_tracerManager.GetMemoryStatistics(s);
	s->AddContainer(m_fmregistry);
	s->AddContainer(m_zmregistry);
	s->AddContainer(m_projectileregistry);
	s->AddContainer(m_queryResults);
	s->AddContainer(m_config);

	{
		SIZER_SUBCOMPONENT_NAME(s, "AmmoParams");
		int nSize = m_ammoparams.size() * sizeof(TAmmoTypeParams::value_type);
		for (TAmmoTypeParams::iterator iter = m_ammoparams.begin(); iter != m_ammoparams.end(); ++iter)
		{
			nSize += iter->second.params->GetMemorySize();
			nSize += iter->second.configurations.size()*sizeof(std::map<string, SAmmoTypeDesc>::value_type);

			for (std::map<string, const SAmmoParams *>::iterator it=iter->second.configurations.begin(); it!=iter->second.configurations.end(); it++)
			{
				nSize+=it->first.size();
				nSize+=it->second->GetMemorySize();
			}
		}
		s->AddObject(&m_ammoparams,nSize);
	}
	
	{
		SIZER_SUBCOMPONENT_NAME(s, "Projectiles");
		int nSize = m_projectiles.size() * sizeof(TProjectileMap::value_type);
		for (TProjectileMap::iterator iter = m_projectiles.begin(); iter != m_projectiles.end(); ++iter)
		{
			nSize += iter->second->GetMemorySize();
		}
		s->AddObject(&m_projectiles,nSize);
	}
}

void CWeaponSystem::ProjectileDeleter::operator()(CProjectile* pProjectile) const
{
	gEnv->pEntitySystem->RemoveEntity(pProjectile->GetEntityId(), true);
}
