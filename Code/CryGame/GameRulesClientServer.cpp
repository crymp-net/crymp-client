/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 23:5:2006   9:27 : Created by Márcio Martins

*************************************************************************/
#include <cstdint>

#include "CryCommon/CrySystem/ISystem.h"
#include "ScriptBind_GameRules.h"
#include "GameRules.h"
#include "Game.h"
#include "GameCVars.h"
#include "Actors/Actor.h"
#include "Actors/Player/Player.h"
#include "HUD/HUD.h"
#include "HUD/HUDRadar.h"
#include "HUD/HUDCrosshair.h"
#include "HUD/HUDTagNames.h"

#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CryAction/IMaterialEffects.h"
#include "CryCommon/CryAction/IGameplayRecorder.h"
#include "CryCommon/CryEntitySystem/EntityId.h"
#include "CryCommon/CrySystem/IConsole.h"

#include "Items/Weapons/Weapon.h"
#include "Items/Weapons/WeaponSystem.h"
#include "Radio.h"
#include "SoundMoods.h"
#include "CryCommon/CryAction/IWorldQuery.h"
#include "ShotValidator.h"

#include "CryCommon/CryCore/StlUtils.h"
#include "Library/Util.h"

#include "CryMP/Server/SSM.h"
#include "Items/Weapons/Projectiles/Bullet.h"
#include "CryCommon/Cry3DEngine/IFoliage.h"

extern std::uintptr_t CRYACTION_BASE;

//------------------------------------------------------------------------
void CGameRules::ValidateShot(EntityId playerId, EntityId weaponId, uint16 seq, uint8 seqr)
{
	if (m_pShotValidator)
		m_pShotValidator->AddShot(playerId, weaponId, seq, seqr);
}

//------------------------------------------------------------------------
void CGameRules::ClientSimpleHit(const SimpleHitInfo& simpleHitInfo)
{
	if (!simpleHitInfo.remote)
	{
		if (!gEnv->bServer)
		{
			//CryMP prevent any spoofs caused by bugs etc..
			if (simpleHitInfo.shooterId == m_pGameFramework->GetClientActorId())
			{
				GetGameObject()->InvokeRMI(SvRequestSimpleHit(), simpleHitInfo, eRMI_ToServer);
			}
		}
		else
			ServerSimpleHit(simpleHitInfo);
	}
}

//------------------------------------------------------------------------
void CGameRules::ClientHit(const HitInfo& hitInfo)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	const EntityId pClientActorId = m_pGameFramework->GetClientActorId();
	const EntityId shooterId = hitInfo.shooterId;
	const EntityId targetId = hitInfo.targetId;
	IEntity* pTarget = m_pEntitySystem->GetEntity(hitInfo.targetId);
	if (!pTarget)
		return;
	IVehicle* pVehicle = m_pGameFramework->GetIVehicleSystem()->GetVehicle(targetId);
	CPlayer* pShooterPlayer = static_cast<CPlayer*>(m_pActorSystem->GetActor(shooterId));
	IActor* pTargetPlayer = m_pActorSystem->GetActor(targetId);
	bool dead = pTargetPlayer ? (pTargetPlayer->GetHealth() <= 0) : false;
	
	const bool showCrosshairHit((pClientActorId == shooterId) || (pShooterPlayer && pShooterPlayer->IsFpSpectatorTarget())); //CryMP for FP target we want to see crosshair hits

	if (showCrosshairHit && pTarget && (pVehicle || pTargetPlayer) && !dead)
	{
		SAFE_HUD_FUNC(GetCrosshair()->CrosshairHit());
		SAFE_HUD_FUNC(GetTagNames()->AddEnemyTagName(pTargetPlayer ? pTargetPlayer->GetEntityId() : pVehicle->GetEntityId()));
	}

	if (hitInfo.targetId == pClientActorId)
		if (gEnv->pInput) gEnv->pInput->ForceFeedbackEvent(SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.5f * hitInfo.damage * 0.01f, hitInfo.damage * 0.02f, 0.0f));

	/*	if (gEnv->pAISystem && !gEnv->bMultiplayer)
		{
			static int htMelee = GetHitTypeId("melee");
			if (pShooter && hitInfo.type != htMelee)
			{
				ISurfaceType *pSurfaceType = GetHitMaterial(hitInfo.material);
				const ISurfaceType::SSurfaceTypeAIParams* pParams = pSurfaceType ? pSurfaceType->GetAIParams() : 0;
				const float radius = pParams ? pParams->fImpactRadius : 5.0f;
				gEnv->pAISystem->BulletHitEvent(hitInfo.pos, radius, pShooter->GetAI());
			}
		}*/

	CreateScriptHitInfo(m_scriptHitInfo, hitInfo);
	CallScript(m_clientStateScript, "OnHit", m_scriptHitInfo);

	bool backface = hitInfo.dir.Dot(hitInfo.normal) > 0;
	if (!hitInfo.remote && targetId && !backface)
	{
		if (!gEnv->bServer)
		{
			//CryMP prevent any unexpected bugs etc..
			if (shooterId == pClientActorId)
			{
				GetGameObject()->InvokeRMI(SvRequestHit(), hitInfo, eRMI_ToServer);
			}
		}
		else
			ServerHit(hitInfo);
	}
}

//------------------------------------------------------------------------
void CGameRules::ServerSimpleHit(const SimpleHitInfo& simpleHitInfo)
{
	switch (simpleHitInfo.type)
	{
	case 0: // tag
	{
		if (!simpleHitInfo.targetId)
			return;

		// tagged entities are temporary in MP, not in SP.
		bool temp = gEnv->bMultiplayer;

		AddTaggedEntity(simpleHitInfo.shooterId, simpleHitInfo.targetId, temp);
	}
	break;
	case 1: // tac
	{
		if (!simpleHitInfo.targetId)
			return;

		CActor* pActor = (CActor*)gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(simpleHitInfo.targetId);

		if (pActor && pActor->CanSleep())
		{
			pActor->Fall(Vec3(0.0f, 0.0f, 0.0f), false, simpleHitInfo.value);
			//This is only used in SP by the player, so don't need further checks
			CPlayer* pPlayer = static_cast<CPlayer*>(m_pGameFramework->GetClientActor());
			if (pPlayer)
				pPlayer->PlaySound(CPlayer::ESound_TacBulletFeedBack, true);
		}
	}
	break;
	case 0xe: // freeze
	{
		if (!simpleHitInfo.targetId)
			return;

		CActor* pActor = GetActorByEntityId(simpleHitInfo.targetId);

		if (pActor && pActor->IsPlayer() && pActor->GetActorClass() == CPlayer::GetActorClassType())
		{
			CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
			if (CNanoSuit* pSuit = pPlayer->GetNanoSuit())
				if (pSuit->IsInvulnerable())
					return;
		}

		// call OnFreeze
		bool allow = true;
		if (m_serverStateScript.GetPtr() && m_serverStateScript->GetValueType("OnFreeze") == svtFunction)
		{
			HSCRIPTFUNCTION func = 0;
			m_serverStateScript->GetValue("OnFreeze", func);
			Script::CallReturn(m_serverStateScript->GetScriptSystem(), func, m_script, ScriptHandle(simpleHitInfo.targetId), ScriptHandle(simpleHitInfo.shooterId), ScriptHandle(simpleHitInfo.weaponId), simpleHitInfo.value, allow);
			gEnv->pScriptSystem->ReleaseFunc(func);
		}

		if (!allow)
			return;

		if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(simpleHitInfo.targetId))
		{
			IScriptTable* pScriptTable = pEntity->GetScriptTable();

			// call OnFrost
			if (pScriptTable && pScriptTable->GetValueType("OnFrost") == svtFunction)
			{
				HSCRIPTFUNCTION func = 0;
				pScriptTable->GetValue("OnFrost", func);
				Script::Call(pScriptTable->GetScriptSystem(), func, pScriptTable, ScriptHandle(simpleHitInfo.shooterId), ScriptHandle(simpleHitInfo.weaponId), simpleHitInfo.value);
				gEnv->pScriptSystem->ReleaseFunc(func);
			}

			FreezeEntity(simpleHitInfo.targetId, true, true, simpleHitInfo.value > 0.999f);
		}
	}
	break;
	default:
		assert(!"Unknown Simple Hit type!");
	}
}

//------------------------------------------------------------------------
void CGameRules::ServerHit(const HitInfo& hitInfo)
{
	HitInfo info(hitInfo);

	if (IItem* pItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(info.weaponId))
	{
		if (CWeapon* pWeapon = static_cast<CWeapon*>(pItem->GetIWeapon()))
		{
			/*			if (info.damage && !gEnv->bServer)
							CryLogAlways("WARNING: SERVER HIT WITH DAMAGE SET!! (dmg: %d   weapon: %s   fmId: %d)", info.damage, pWeapon->GetEntity()->GetClass()->GetName(), info.fmId);
			*/
			float distance = 0.0f;

			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(info.shooterId ? info.shooterId : info.weaponId))
			{
				distance = (pEntity->GetWorldPos() - info.pos).len2();
				if (distance > 0.0f)
					distance = cry_sqrtf_fast(distance);
			}

			info.damage = pWeapon->GetDamage(info.fmId, distance);

			if (info.type != GetHitTypeId(pWeapon->GetDamageType(info.fmId)))
			{
				//				CryLogAlways("WARNING: MISMATCHING DAMAGE TYPE!! (dmg: %d   weapon: %s   fmId: %d   type: %d)", info.damage, pWeapon->GetEntity()->GetClass()->GetName(), info.fmId, info.type);
				info.damage = 0;
			}
		}
	}

	if (m_processingHit)
	{
		m_queuedHits.push(info);
		return;
	}

	++m_processingHit;

	ProcessServerHit(info);

	while (!m_queuedHits.empty())
	{
		HitInfo qinfo(m_queuedHits.front());
		ProcessServerHit(qinfo);
		m_queuedHits.pop();
	}

	--m_processingHit;
}

//------------------------------------------------------------------------
void CGameRules::ProcessServerHit(HitInfo& hitInfo)
{
	if (m_pShotValidator && !m_pShotValidator->ProcessHit(hitInfo))
		return;

	bool ok = true;
	// check if shooter is alive
	CActor* pShooter = GetActorByEntityId(hitInfo.shooterId);

	if (hitInfo.shooterId)
	{
		if (pShooter && pShooter->GetHealth() <= 0)
			ok = false;
	}

	if (hitInfo.targetId)
	{
		CActor* pTarget = GetActorByEntityId(hitInfo.targetId);
		if (pTarget && pTarget->GetSpectatorMode())
			ok = false;
	}

	if (ok)
	{
		CreateScriptHitInfo(m_scriptHitInfo, hitInfo);
		CallScript(m_serverStateScript, "OnHit", m_scriptHitInfo);

		// call hit listeners if any
		if (m_hitListeners.empty() == false)
		{
			m_hitListenersCopy = m_hitListeners;
			for(auto& iter : m_hitListenersCopy)
			{
				iter->OnHit(hitInfo);
			}
		}

		if (pShooter && hitInfo.shooterId != hitInfo.targetId && hitInfo.weaponId != hitInfo.shooterId && hitInfo.weaponId != hitInfo.targetId && hitInfo.damage >= 0)
		{
			EntityId params[2];
			params[0] = hitInfo.weaponId;
			params[1] = hitInfo.targetId;
			m_pGameplayRecorder->Event(pShooter->GetEntity(), GameplayEvent(eGE_WeaponHit, 0, 0, (void*)params));
		}

		if (pShooter)
		{
			void* extra = reinterpret_cast<void*>(static_cast<uintptr_t>(hitInfo.weaponId));
			m_pGameplayRecorder->Event(pShooter->GetEntity(), GameplayEvent(eGE_Hit, 0, 0, extra));
		}

		if (pShooter)
		{
			void* extra = reinterpret_cast<void*>(static_cast<uintptr_t>(hitInfo.weaponId));
			m_pGameplayRecorder->Event(pShooter->GetEntity(), GameplayEvent(eGE_Damage, 0, hitInfo.damage, extra));
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::ServerExplosion(const ExplosionInfo& explosionInfo)
{
	m_queuedExplosions.push(explosionInfo);
}

//------------------------------------------------------------------------
void CGameRules::ProcessServerExplosion(const ExplosionInfo& explosionInfo)
{
	//CryLog("[ProcessServerExplosion] (frame %i) shooter %i, damage %.0f, radius %.1f", gEnv->pRenderer->GetFrameID(), explosionInfo.shooterId, explosionInfo.damage, explosionInfo.radius);

	GetGameObject()->InvokeRMI(ClExplosion(), explosionInfo, eRMI_ToRemoteClients);
	ClientExplosion(explosionInfo);
}

//------------------------------------------------------------------------
void CGameRules::ProcessQueuedExplosions()
{
	const static uint8 nMaxExp = 3;

	for (uint8 exp = 0; !m_queuedExplosions.empty() && exp < nMaxExp; ++exp)
	{
		ExplosionInfo info(m_queuedExplosions.front());
		ProcessServerExplosion(info);
		m_queuedExplosions.pop();
	}
}


//------------------------------------------------------------------------
void CGameRules::CullEntitiesInExplosion(const ExplosionInfo& explosionInfo)
{
	if (!g_pGameCVars->g_ec_enable || explosionInfo.damage <= 0.1f)
		return;

	IPhysicalEntity** pents;
	float radiusScale = g_pGameCVars->g_ec_radiusScale;
	float minVolume = g_pGameCVars->g_ec_volume;
	float minExtent = g_pGameCVars->g_ec_extent;
	int   removeThreshold = max(1, g_pGameCVars->g_ec_removeThreshold);

	IActor* pClientActor = m_pGameFramework->GetClientActor();

	Vec3 radiusVec(radiusScale * explosionInfo.physRadius);
	int i = gEnv->pPhysicalWorld->GetEntitiesInBox(explosionInfo.pos - radiusVec, explosionInfo.pos + radiusVec, pents, ent_rigid | ent_sleeping_rigid);
	int removedCount = 0;

	static IEntityClass* s_pInteractiveEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("InteractiveEntity");
	static IEntityClass* s_pDeadBodyClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("DeadBody");

	if (i > removeThreshold)
	{
		int entitiesToRemove = i - removeThreshold;
		for (--i;i >= 0;i--)
		{
			if (removedCount >= entitiesToRemove)
				break;

			IEntity* pEntity = (IEntity*)pents[i]->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
			if (pEntity)
			{
				// don't remove if entity is held by the player
				if (pClientActor && pEntity->GetId() == pClientActor->GetGrabbedEntityId())
					continue;

				// don't remove items/pickups
				if (IItem* pItem = m_pGameFramework->GetIItemSystem()->GetItem(pEntity->GetId()))
				{
					continue;
				}
				// don't remove enemies/ragdolls
				if (IActor* pActor = m_pActorSystem->GetActor(pEntity->GetId()))
				{
					continue;
				}

				// if there is a flowgraph attached, never remove!
				if (pEntity->GetProxy(ENTITY_PROXY_FLOWGRAPH) != 0)
					continue;

				IEntityClass* pClass = pEntity->GetClass();
				if (pClass == s_pInteractiveEntityClass || pClass == s_pDeadBodyClass)
					continue;

				// get bounding box
				if (IEntityPhysicalProxy* pPhysProxy = (IEntityPhysicalProxy*)pEntity->GetProxy(ENTITY_PROXY_PHYSICS))
				{
					AABB aabb;
					pPhysProxy->GetWorldBounds(aabb);

					// don't remove objects which are larger than a predefined minimum volume
					if (aabb.GetVolume() > minVolume)
						continue;

					// don't remove objects which are larger than a predefined minimum volume
					Vec3 size(aabb.GetSize().abs());
					if (size.x > minExtent || size.y > minExtent || size.z > minExtent)
						continue;
				}

				// marcok: somehow editor doesn't handle deleting non-dynamic entities very well
				// but craig says, hiding is not synchronized for DX10 breakable MP, so we remove entities only when playing pure game
				// alexl: in SinglePlayer, we also currently only hide the object because it could be part of flowgraph logic
				//        which would break if Entity was removed and could not propagate events anymore
				if (gEnv->bMultiplayer == false || gEnv->pSystem->IsEditor())
				{
					pEntity->Hide(true);
				}
				else
				{
					gEnv->pEntitySystem->RemoveEntity(pEntity->GetId());
				}
				removedCount++;
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::ClientExplosion(const ExplosionInfo& explosionInfo)
{
	// let 3D engine know about explosion (will create holes and remove vegetation)
	//if (explosionInfo.hole_size > 0.0f)
	//{
		//gEnv->p3DEngine->OnExplosion(explosionInfo.pos, explosionInfo.hole_size, true);
	//}

	TExplosionAffectedEntities affectedEntities;

	if (gEnv->bServer)
	{
		CullEntitiesInExplosion(explosionInfo);
		pe_explosion explosion;
		explosion.epicenter = explosionInfo.pos;
		explosion.rmin = explosionInfo.minRadius;
		explosion.rmax = explosionInfo.radius;
		if (explosion.rmax == 0)
			explosion.rmax = 0.0001f;
		explosion.r = explosion.rmin;
		explosion.impulsivePressureAtR = explosionInfo.pressure;
		explosion.epicenterImp = explosionInfo.pos;
		explosion.explDir = explosionInfo.dir;
		explosion.nGrow = 2;
		explosion.rminOcc = 0.07f;

		// we separate the calls to SimulateExplosion so that we can define different radii for AI and physics bodies
		explosion.holeSize = 0.0f;
		explosion.nOccRes = explosion.rmax > 50.0f ? 0 : 32;
		gEnv->pPhysicalWorld->SimulateExplosion(&explosion, 0, 0, ent_living);

		CreateScriptExplosionInfo(m_scriptExplosionInfo, explosionInfo);
		UpdateAffectedEntitiesSet(affectedEntities, &explosion);
	
		// check vehicles
		IVehicleSystem* pVehicleSystem = m_pGameFramework->GetIVehicleSystem();
		uint32 vcount = pVehicleSystem->GetVehicleCount();
		if (vcount > 0)
		{
			IVehicleIteratorPtr iter = m_pGameFramework->GetIVehicleSystem()->CreateVehicleIterator();
			while (IVehicle* pVehicle = iter->Next())
			{
				if (IEntity* pEntity = pVehicle->GetEntity())
				{
					AABB aabb;
					pEntity->GetWorldBounds(aabb);
					IPhysicalEntity* pEnt = pEntity->GetPhysics();
					if (pEnt && aabb.GetDistanceSqr(explosionInfo.pos) <= explosionInfo.radius * explosionInfo.radius)
					{
						float affected = gEnv->pPhysicalWorld->CalculateExplosionExposure(&explosion, pEnt);
						AddOrUpdateAffectedEntity(affectedEntities, pEntity, affected);
					}
				}
			}
		}

		explosion.rmin = explosionInfo.minPhysRadius;
		explosion.rmax = explosionInfo.physRadius;
		if (explosion.rmax == 0)
			explosion.rmax = 0.0001f;
		explosion.r = explosion.rmin;
		explosion.holeSize = explosionInfo.hole_size;
		explosion.nOccRes = -1;	// makes second call re-use occlusion info
		gEnv->pPhysicalWorld->SimulateExplosion(&explosion, 0, 0, ent_rigid | ent_sleeping_rigid | ent_independent | ent_static);

		UpdateAffectedEntitiesSet(affectedEntities, &explosion);
		CommitAffectedEntitiesSet(m_scriptExplosionInfo, affectedEntities);

		float fSuitEnergyBeforeExplosion = 0.0f;
		float fHealthBeforeExplosion = 0.0f;
		IActor* pClientActor = m_pGameFramework->GetClientActor();
		if (pClientActor)
		{
			fSuitEnergyBeforeExplosion = static_cast<CPlayer*>(pClientActor)->GetNanoSuit()->GetSuitEnergy();
			fHealthBeforeExplosion = pClientActor->GetHealth();
		}

		CallScript(m_serverStateScript, "OnExplosion", m_scriptExplosionInfo);

		if (pClientActor)
		{
			float fDeltaSuitEnergy = fSuitEnergyBeforeExplosion - static_cast<CPlayer*>(pClientActor)->GetNanoSuit()->GetSuitEnergy();
			float fDeltaHealth = fHealthBeforeExplosion - pClientActor->GetHealth();

			if (fDeltaSuitEnergy >= 50.0f || fDeltaHealth >= 20.0f)
			{
				SAFE_SOUNDMOODS_FUNC(AddSoundMood(SOUNDMOOD_EXPLOSION, MIN(fDeltaSuitEnergy + fDeltaHealth, 100.0f)));
			}
		}

		if (!gEnv->bMultiplayer && g_pGameCVars->g_spRecordGameplay)
		{
			float distance = (explosion.epicenter - pClientActor->GetEntity()->GetWorldPos()).len();
			m_pGameplayRecorder->Event(pClientActor->GetEntity(), GameplayEvent(eGE_Explosion, 0, distance, 0));
		}

		// call hit listeners if any
		if (m_hitListeners.empty() == false)
		{
			m_hitListenersCopy = m_hitListeners;
			for (auto& iter : m_hitListenersCopy)
			{
				iter->OnServerExplosion(explosionInfo);
			}
		}
	}

	if (gEnv->bClient)
	{
		if (explosionInfo.pParticleEffect)
			explosionInfo.pParticleEffect->Spawn(true, IParticleEffect::ParticleLoc(explosionInfo.pos, explosionInfo.dir, explosionInfo.effect_scale));

		if (!gEnv->bServer)
		{
			CreateScriptExplosionInfo(m_scriptExplosionInfo, explosionInfo);
		}
		else
		{
			affectedEntities.clear();
			CommitAffectedEntitiesSet(m_scriptExplosionInfo, affectedEntities);
		}
		CallScript(m_clientStateScript, "OnExplosion", m_scriptExplosionInfo);

		// call hit listeners if any
		if (m_hitListeners.empty() == false)
		{
			m_hitListenersCopy = m_hitListeners;
			for (auto& iter : m_hitListenersCopy)
			{
				iter->OnExplosion(explosionInfo);
			}
		}
	}

	if (gEnv->bClient)
	{
		ProcessClientExplosionScreenFX(explosionInfo);

		bool mfxPlayed = false;

		if (gEnv->bMultiplayer && !gEnv->bServer)
		{
			if (!explosionInfo.effect_class.empty())
			{
				if (IEntityClassRegistry* cr = gEnv->pEntitySystem->GetClassRegistry())
				{
					if (IEntityClass *pProjectileClass = cr->FindClass(explosionInfo.effect_class.c_str()))
					{
						const SAmmoParams* pAmmo = g_pGame->GetWeaponSystem()->GetAmmoParams(pProjectileClass);
						if (pAmmo)
						{
							const bool serverCustomParticleValid = (explosionInfo.pParticleEffect != nullptr && 
								pAmmo->pExplosion && pAmmo->pExplosion->pParticleEffect != explosionInfo.pParticleEffect);

							if (!serverCustomParticleValid)
							{
								//CryMP: If server sets a custom valid explosion effect, don't play the original effect on top of it
								if (!serverCustomParticleValid)
								{
									//CryMP: In multiplayer, play explosion FX from ClExplosion if ammo supports it
									mfxPlayed = PlayMFXFromExplosionInfo(explosionInfo, pAmmo);
								}
								//else
								//{
									//CryLogAlways("Skipping MFX from explosion because server is using custom particle effect! (%s)", explosionInfo.pParticleEffect ? explosionInfo.pParticleEffect->GetName() : "nullptr");
								//}
							}
						}
					}
				}
			}
		}

		if (!mfxPlayed)
		{
			ProcessExplosionMaterialFX(explosionInfo);
		}
	}

	IEntity* pShooter = m_pEntitySystem->GetEntity(explosionInfo.shooterId);
	if (gEnv->pAISystem && !gEnv->bMultiplayer)
	{
		IAIObject* pShooterAI(pShooter != NULL ? pShooter->GetAI() : NULL);
		gEnv->pAISystem->ExplosionEvent(explosionInfo.pos, explosionInfo.radius, pShooterAI);
	}

}

//-------------------------------------------
void CGameRules::ProcessClientExplosionScreenFX(const ExplosionInfo& explosionInfo)
{
	IActor* pClientActor = m_pGameFramework->GetClientActor();
	if (pClientActor)
	{
		//Distance
		float dist = (pClientActor->GetEntity()->GetWorldPos() - explosionInfo.pos).len();

		//Is the explosion in Player's FOV (let's suppose the FOV a bit higher, like 80)
		CActor* pActor = (CActor*)pClientActor;
		SMovementState state;
		if (IMovementController* pMV = pActor->GetMovementController())
		{
			pMV->GetMovementState(state);
		}

		Vec3 eyeToExplosion = explosionInfo.pos - state.eyePosition;
		eyeToExplosion.Normalize();
		bool inFOV = (state.eyeDirection.Dot(eyeToExplosion) > 0.68f);

		// if in a vehicle eyeDirection is wrong
		if (pActor && pActor->GetLinkedVehicle())
		{
			Vec3 eyeDir = static_cast<CPlayer*>(pActor)->GetVehicleViewDir();
			inFOV = (eyeDir.Dot(eyeToExplosion) > 0.68f);
		}

		//All explosions have radial blur (default 30m radius, to make Sean happy =))
		float maxBlurDistance = (explosionInfo.maxblurdistance > 0.0f) ? explosionInfo.maxblurdistance : 30.0f;
		if (maxBlurDistance > 0.0f && g_pGameCVars->g_radialBlur > 0.0f && m_explosionScreenFX && explosionInfo.radius > 0.5f)
		{
			if (inFOV && dist < maxBlurDistance)
			{
				ray_hit hit;
				int col = gEnv->pPhysicalWorld->RayWorldIntersection(explosionInfo.pos, -eyeToExplosion * dist, ent_static | ent_terrain, rwi_stop_at_pierceable | rwi_colltype_any, &hit, 1);

				//If there was no obstacle between flashbang grenade and player
				if (!col)
				{
					float blurRadius = (-1.0f / maxBlurDistance) * dist + 1.0f;

					gEnv->p3DEngine->SetPostEffectParam("FilterRadialBlurring_Radius", blurRadius);
					gEnv->p3DEngine->SetPostEffectParam("FilterRadialBlurring_Amount", 1.0f);

					//CActor *pActor = (CActor *)pClientActor;
					if (pActor->GetScreenEffects() != 0)
					{
						CPostProcessEffect* pBlur = new CPostProcessEffect(pClientActor->GetEntityId(), "FilterRadialBlurring_Amount", 0.0f);
						CLinearBlend* pLinear = new CLinearBlend(1.0f);
						pActor->GetScreenEffects()->StartBlend(pBlur, pLinear, 1.0f, 98);
						pActor->GetScreenEffects()->SetUpdateCoords("FilterRadialBlurring_ScreenPosX", "FilterRadialBlurring_ScreenPosY", explosionInfo.pos);
					}

					float distAmp = 1.0f - (dist / maxBlurDistance);
					if (gEnv->pInput) gEnv->pInput->ForceFeedbackEvent(SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.5f, distAmp * 3.0f, 0.0f));
				}
			}
		}

		//Flashbang effect 
		if (dist < explosionInfo.radius && inFOV &&
			(explosionInfo.effect_class == "flashbang" || explosionInfo.effect_class == "FlashbangAI"))
		{
			ray_hit hit;
			int col = gEnv->pPhysicalWorld->RayWorldIntersection(explosionInfo.pos, -eyeToExplosion * dist, ent_static | ent_terrain, rwi_stop_at_pierceable | rwi_colltype_any, &hit, 1);

			//If there was no obstacle between flashbang grenade and player
			if (!col)
			{
				float power = explosionInfo.flashbangScale;
				power *= max(0.0f, 1 - (dist / explosionInfo.radius));
				float lookingAt = (eyeToExplosion.Dot(state.eyeDirection.normalize()) + 1) * 0.5f;
				power *= lookingAt;

				SAFE_SOUNDMOODS_FUNC(AddSoundMood(SOUNDMOOD_EXPLOSION, MIN(power * 40.0f, 100.0f)));

				gEnv->p3DEngine->SetPostEffectParam("Flashbang_Time", 1.0f + (power * 4));
				gEnv->p3DEngine->SetPostEffectParam("FlashBang_BlindAmount", explosionInfo.blindAmount);
				gEnv->p3DEngine->SetPostEffectParam("Flashbang_DifractionAmount", (power * 2));
				gEnv->p3DEngine->SetPostEffectParam("Flashbang_Active", 1);
			}
		}
		else if (inFOV && (dist < explosionInfo.radius))
		{
			if (explosionInfo.damage > 10.0f || explosionInfo.pressure > 100.0f)
			{
				//Add some angular impulse to the client actor depending on distance, direction...
				float dt = (1.0f - dist / explosionInfo.radius);
				dt = dt * dt;
				float angleZ = g_PI * 0.15f * dt;
				float angleX = g_PI * 0.15f * dt;

				pActor->AddAngularImpulse(Ang3(Random(-angleX * 0.5f, angleX), 0.0f, Random(-angleZ, angleZ)), 0.0f, dt * 2.0f);
			}
		}


		float fDist2 = (pClientActor->GetEntity()->GetWorldPos() - explosionInfo.pos).len2();
		if (fDist2 < 250.0f * 250.0f)
		{
			SAFE_HUD_FUNC(ShowSoundOnRadar(explosionInfo.pos, explosionInfo.hole_size));
			if (fDist2 < sqr(SAFE_HUD_FUNC_RET(GetBattleRange())))
				SAFE_HUD_FUNC(TickBattleStatus(1.0f));
		}
	}

}

//---------------------------------------------------
void CGameRules::ProcessExplosionMaterialFX(const ExplosionInfo& explosionInfo)
{
	// if an effect was specified, don't use MFX
	if (explosionInfo.pParticleEffect)
		return;

	// impact stuff here
	SMFXRunTimeEffectParams params;
	params.soundSemantic = eSoundSemantic_Explosion;
	params.pos = params.decalPos = explosionInfo.pos;
	params.trg = 0;
	params.trgRenderNode = 0;

	Vec3 gravity;
	pe_params_buoyancy buoyancy;
	gEnv->pPhysicalWorld->CheckAreas(params.pos, gravity, &buoyancy);

	// 0 for water, 1 for air
	Vec3 pos = params.pos;
	params.inWater = (buoyancy.waterPlane.origin.z > params.pos.z) && (gEnv->p3DEngine->GetWaterLevel(&pos) >= params.pos.z);
	params.inZeroG = (gravity.len2() < 0.0001f);
	params.trgSurfaceId = 0;

	static const int objTypes = ent_all;
	static const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;

	ray_hit ray;

	if (explosionInfo.impact)
	{
		params.dir[0] = explosionInfo.impact_velocity.normalized();
		params.normal = explosionInfo.impact_normal;

		if (gEnv->pPhysicalWorld->RayWorldIntersection(params.pos - params.dir[0] * 0.0125f, params.dir[0] * 0.25f, objTypes, flags, &ray, 1))
		{
			params.trgSurfaceId = ray.surface_idx;
			if (ray.pCollider->GetiForeignData() == PHYS_FOREIGN_ID_STATIC)
				params.trgRenderNode = (IRenderNode*)ray.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);
		}
	}
	else
	{
		params.dir[0] = gravity;
		params.normal = -gravity.normalized();

		if (gEnv->pPhysicalWorld->RayWorldIntersection(params.pos, gravity, objTypes, flags, &ray, 1))
		{
			params.trgSurfaceId = ray.surface_idx;
			if (ray.pCollider->GetiForeignData() == PHYS_FOREIGN_ID_STATIC)
				params.trgRenderNode = (IRenderNode*)ray.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);
		}
	}

	string effectClass = explosionInfo.effect_class;
	if (effectClass.empty())
		effectClass = "generic";

	string query = effectClass + "_explode";
	if (gEnv->p3DEngine->GetWaterLevel(&explosionInfo.pos) > explosionInfo.pos.z)
	{
		query = query + "_underwater";
	}

	IMaterialEffects* pMaterialEffects = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects();
	TMFXEffectId effectId = pMaterialEffects->GetEffectId(query.c_str(), params.trgSurfaceId);

	if (effectId == InvalidEffectId)
		effectId = pMaterialEffects->GetEffectId(query.c_str(), pMaterialEffects->GetDefaultSurfaceIndex());

	if (effectId != InvalidEffectId)
	{
		pMaterialEffects->ExecuteEffect(effectId, params);
	}
	return;
}

//---------------------------------------------------
// Helper: segment-plane (z = planeZ) intersection
//---------------------------------------------------
bool CGameRules::IntersectSegWithZPlane(const Vec3& p, const Vec3& seg, float planeZ, float& tOut, Vec3& hitOut) const
{
	const float denom = seg.z;
	if (fabs_tpl(denom) < 1e-6f)
		return false;
	const float t = (planeZ - p.z) / denom;
	if (t < 0.0f || t > 1.0f)
		return false;
	tOut = t;
	hitOut = p + seg * t;
	return true;
}

//---------------------------------------------------
bool CGameRules::PlayMFXFromExplosionInfo(const ExplosionInfo& info, const SAmmoParams* pAmmo) const
{
	if (!pAmmo || pAmmo->clexplosion_mfx == 0)
	{
		//CryLogAlways("$8PlayMFXFromExplosionInfo: %s doesnt use mfx from cl_explosion", info.effect_class.c_str());
		return false;
	}

	// Ammo params
	const IEntityClass* pProjectileClass = pAmmo->pEntityClass;

	bool useOriginalMFX = true;
	const Vec3 pos = info.pos;

	Vec3 intoSurf = info.dir.GetNormalizedSafe(Vec3(0, 0, 1));

	IEntity* pSrcEnt = gEnv->pEntitySystem->GetEntity(info.weaponId);
	IPhysicalEntity* peSrc = pSrcEnt ? pSrcEnt->GetPhysics() : nullptr;

	// Single forward ray probe
	ray_hit rh; memset(&rh, 0, sizeof(rh));
	const int objTypes = ent_all;
	const int flags = rwi_colltype_any | rwi_stop_at_pierceable;

	IPhysicalEntity* skip[1] = { peSrc };
	const int nSkip = peSrc ? 1 : 0;

	const Vec3 fwdStart = pos - intoSurf * 0.25f;
	const Vec3 fwdSeg = intoSurf * 10.0f;

	const int hits = gEnv->pPhysicalWorld->RayWorldIntersection(fwdStart, fwdSeg, objTypes, flags, &rh, 1, skip, nSkip);

	// Prepare EventPhysCollision to forward to OnCollisionLogged_MaterialFX
	EventPhysCollision c;
	c.idCollider = 0;

	// Resolve contact point surface normal 
	const float waterLevelAcc = gEnv->p3DEngine->GetWaterLevel(&pos);
	const bool insideWater = (pos.z < waterLevelAcc);
	const bool waterNearby = fabs_tpl(pos.z - waterLevelAcc) < 2.0f;

	Vec3 pt = pos;
	Vec3 nrm = intoSurf; 
	bool usedWater = false;

	if (hits > 0 && rh.pCollider)
	{
		pt = rh.pt;

		// If water is in front of the solid along the same probe, prefer it
		if (waterNearby)
		{
			float tW; Vec3 hitW;
			if (IntersectSegWithZPlane(fwdStart, fwdSeg, waterLevelAcc, tW, hitW))
			{
				const float segLen = fwdSeg.GetLength();
				const float tSolid = (segLen > 1e-6f) ? (rh.dist / segLen) : 2.0f;
				if (tW <= tSolid + 1e-4f)
				{
					pt = hitW; pt.z = waterLevelAcc - 0.02f;
					nrm = Vec3(0, 0, 1);
					usedWater = true;
				}
			}
		}

		if (!usedWater)
		{
			nrm = rh.n; // outward surface normal
		}
	}
	else
	{
		// No solid hit; if in/near water, use water plane; else leave defaults
		if (insideWater || waterNearby)
		{
			float tW; Vec3 hitW;
			if (IntersectSegWithZPlane(fwdStart, fwdSeg, waterLevelAcc, tW, hitW))
			{
				pt = hitW; pt.z = waterLevelAcc - 0.02f;
				nrm = Vec3(0, 0, 1);
				usedWater = true;
			}
			else
			{
				pt = pos; pt.z = waterLevelAcc - 0.02f;
				nrm = Vec3(0, 0, 1);
				usedWater = true;
			}
		}
	}

	// Pass point and surface normal
	c.pt = pt;
	c.n = nrm;

	// Set velocities for backface test: vloc[0] should be the projectile world-velocity
	if (info.impact && info.impact_velocity.len2() > 1e-6f)
	{
		c.vloc[0] = info.impact_velocity; // world-space projectile velocity
	}
	else if (peSrc)
	{
		pe_status_dynamics sd;
		c.vloc[0] = (peSrc->GetStatus(&sd) ? sd.v : info.dir * 120.0f);
	}
	else
	{
		c.vloc[0] = info.dir * 120.0f;
	}

	// Target velocity (if any) 
	c.vloc[1] = Vec3(ZERO);
	if (rh.pCollider)
	{
		pe_status_dynamics sdT;
		if (rh.pCollider->GetStatus(&sdT))
			c.vloc[1] = sdT.v;
	}

	// Masses & part ids
	c.mass[0] = (pAmmo && pAmmo->mass > 0.f) ? pAmmo->mass : 0.05f;
	c.mass[1] = 0.0f;
	c.partid[0] = 0;
	c.partid[1] = rh.partid;

	// Materials
	IMaterialEffects* pMFX = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects();
	const int defaultSurf = pMFX->GetDefaultSurfaceIndex();

	int srcSurf = defaultSurf;
	if (pAmmo && pAmmo->pSurfaceType)
	{
		const int sfid = pAmmo->pSurfaceType->GetId();
		if (sfid > 0) srcSurf = sfid;
	}
	c.idmat[0] = srcSurf;

	const int waterId = CBullet::GetWaterMaterialId();

	// Bind target material & foreign data so decals orient correctly
	if (usedWater)
	{
		c.idmat[1] = (waterId > 0) ? waterId : defaultSurf;
		c.pEntity[1] = gEnv->pPhysicalWorld->AddGlobalArea();
		c.iForeignData[1] = 0;
		c.pForeignData[1] = nullptr;
	}
	else if (rh.pCollider)
	{
		c.idmat[1] = rh.surface_idx;
		c.pEntity[1] = rh.pCollider;

		if (IEntity* hitEnt = gEnv->pEntitySystem->GetEntityFromPhysics(rh.pCollider))
		{
			c.iForeignData[1] = PHYS_FOREIGN_ID_ENTITY;
			c.pForeignData[1] = hitEnt;

		   // --- ACTOR-FACING NORMAL FIX ---
			if (IActor* pHitActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(hitEnt->GetId()))
			{
				useOriginalMFX = false;
				c.mass[0] = std::max(c.mass[0], 10.f);

				if (nrm.Dot(intoSurf) > 0.0f)
				{
					nrm = -nrm;
					c.n = nrm;
				}
			}
			// --- END FIX ---
		}
		else
		{
			c.iForeignData[1] = PHYS_FOREIGN_ID_STATIC;
			c.pForeignData[1] = GetRenderNodeFromCollider(rh.pCollider);
		}
	}
	else
	{
		// Nothing solid and no water → use default
		c.idmat[1] = defaultSurf;
		c.pEntity[1] = nullptr;
		c.iForeignData[1] = 0;
		c.pForeignData[1] = nullptr;
	}

	// Source binding
	c.pEntity[0] = peSrc;
	c.iForeignData[0] = pSrcEnt ? PHYS_FOREIGN_ID_ENTITY : 0;
	c.pForeignData[0] = pSrcEnt;

	c.penetration = 0.f;
	c.normImpulse = 0.f;
	c.radius = 0.f;

	//CryMP: If pSrcEnt, call original function
	if (pSrcEnt && useOriginalMFX)
	{
		// CActionGame::OnCollisionLogged_MaterialFX in CryAction.dll
#ifdef BUILD_64BIT
		reinterpret_cast<void(*)(const EventPhysCollision*)>(CRYACTION_BASE + 0x30CE20)(&c);
#else
		reinterpret_cast<void(*)(const EventPhysCollision*)>(CRYACTION_BASE + 0x211A70)(&c);
#endif
	}
	else if (pProjectileClass)
	{
		//CryMP: Projectile got destroyed or we hit an actor, call function with class name info to call appropriate effects 
		OnCollisionLogged_MaterialFX((const EventPhys*)&c, const_cast<IEntityClass*>(pProjectileClass));
	}

	return true;
}

//---------------------------------------------------
IRenderNode* CGameRules::GetRenderNodeFromCollider(IPhysicalEntity* pCollider) const
{
	if (!pCollider)
		return nullptr;

	pe_params_foreign_data fd;
	if (!pCollider->GetParams(&fd))
		return nullptr;

	switch (fd.iForeignData)
	{
	case PHYS_FOREIGN_ID_ENTITY:
	{
		IEntity* pEntity = static_cast<IEntity*>(pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY));
		if (!pEntity)
		{
			pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pCollider);
		}
		if (pEntity)
		{
			if (IEntityRenderProxy* pRenderProxy = static_cast<IEntityRenderProxy*>(pEntity->GetProxy(ENTITY_PROXY_RENDER)))
			{
				return pRenderProxy->GetRenderNode();
			}
		}
		break;
	}
	case PHYS_FOREIGN_ID_STATIC:
	case PHYS_FOREIGN_ID_TERRAIN:
		return static_cast<IRenderNode*>(pCollider->GetForeignData(fd.iForeignData));

	case PHYS_FOREIGN_ID_FOLIAGE:
	{
		IFoliage* pFoliage = static_cast<IFoliage*>(fd.pForeignData);
		return pFoliage ? pFoliage->GetIRenderNode() : nullptr;
	}
	default:
		break;
	}

	return nullptr;
}

//---------------------------------------------------
bool CGameRules::OnCollisionLogged_MaterialFX(const EventPhys* pEvent, IEntityClass *pProjectileClass) const
{
	const EventPhysCollision* pCEvent = (const EventPhysCollision*)pEvent;

	if ((CBullet::GetWaterMaterialId() && pCEvent->idmat[1] == CBullet::GetWaterMaterialId()) &&
		(pCEvent->pEntity[1] == gEnv->pPhysicalWorld->AddGlobalArea() && gEnv->p3DEngine->GetVisAreaFromPos(pCEvent->pt)))
		return false;

	const auto GetEnt = [](int i, void* p) -> IEntity*
		{
			return (i == PHYS_FOREIGN_ID_ENTITY) ? static_cast<IEntity*>(p) : nullptr;
		};

	Vec3 dir = ZERO;
	if (pCEvent->vloc[0].GetLengthSquared() > 1e-6f)
	{
		dir = pCEvent->vloc[0].GetNormalized();
	}

	bool backface = (pCEvent->n.Dot(dir) >= 0);

	// track contacts info for physics sounds generation
	//CryMP: Commented this out, we dont have SEntityCollHist
	/*
	Vec3 vrel, r;
	float velImpact, velSlide2, velRoll2;
	pe_status_dynamics sd;
	int iop, id, i;
	SEntityCollHist* pech = 0;
	std::map<int, SEntityCollHist*>::iterator iter;

	iop = inrange(pCEvent->mass[1], 0.0f, pCEvent->mass[0]);
	id = gEnv->pPhysicalWorld->GetPhysicalEntityId(pCEvent->pEntity[iop]);
	if ((iter = s_this->m_mapECH.find(id)) != s_this->m_mapECH.end())
		pech = iter->second;
	else if (s_this->m_pFreeCHSlot0->pnext != s_this->m_pFreeCHSlot0)
	{
		pech = s_this->m_pFreeCHSlot0->pnext;
		s_this->m_pFreeCHSlot0->pnext = pech->pnext;
		pech->pnext = 0;
		pech->timeRolling = pech->timeNotRolling = pech->rollTimeout = pech->slideTimeout = 0;
		pech->velImpact = pech->velSlide2 = pech->velRoll2 = 0;
		pech->imatImpact[0] = pech->imatImpact[1] = pech->imatSlide[0] = pech->imatSlide[1] = pech->imatRoll[0] = pech->imatRoll[1] = 0;
		pech->mass = 0;
		s_this->m_mapECH.insert(std::pair<int, SEntityCollHist*>(id, pech));
	}

	if (pech && pCEvent->pEntity[iop]->GetStatus(&sd))
	{
		vrel = pCEvent->vloc[iop ^ 1] - pCEvent->vloc[iop];
		r = pCEvent->pt - sd.centerOfMass;
		if (sd.w.len2() > 0.01f)
			r -= sd.w * ((r * sd.w) / sd.w.len2());
		velImpact = fabs_tpl(vrel * pCEvent->n);
		velSlide2 = (vrel - pCEvent->n * velImpact).len2();
		velRoll2 = (sd.w ^ r).len2();
		pech->mass = pCEvent->mass[iop];

		i = isneg(pech->velImpact - velImpact);
		pech->imatImpact[0] += pCEvent->idmat[iop] - pech->imatImpact[0] & -i;
		pech->imatImpact[1] += pCEvent->idmat[iop ^ 1] - pech->imatImpact[1] & -i;
		pech->velImpact = max(pech->velImpact, velImpact);

		i = isneg(pech->velSlide2 - velSlide2);
		pech->imatSlide[0] += pCEvent->idmat[iop] - pech->imatSlide[0] & -i;
		pech->imatSlide[1] += pCEvent->idmat[iop ^ 1] - pech->imatSlide[1] & -i;
		pech->velSlide2 = max(pech->velSlide2, velSlide2);

		i = isneg(max(pech->velRoll2 - velRoll2, r.len2() * sqr(0.97f) - sqr(r * pCEvent->n)));
		pech->imatRoll[0] += pCEvent->idmat[iop] - pech->imatRoll[0] & -i;
		pech->imatSlide[1] += pCEvent->idmat[iop ^ 1] - pech->imatRoll[1] & -i;
		pech->velRoll2 += (velRoll2 - pech->velRoll2) * i;
	}
	*/
	// --- Begin Material Effects Code ---
	// Relative velocity, adjusted to be between 0 and 1 for sound effect parameters.
	static ICVar* mfx_Debug = gEnv->pConsole->GetCVar("mfx_Debug");
	const int debug = mfx_Debug->GetIVal() > 0;

	float adjustedRelativeVelocity = 0.0f;
	float impactVelSquared = (pCEvent->vloc[0] - pCEvent->vloc[1]).GetLengthSquared();

	// Anything faster than 15 m/s is fast enough to consider maximum speed
	adjustedRelativeVelocity = (float)min(1.0f, impactVelSquared / (15.0f * 15.0f));

	// Relative mass, also adjusted to fit into sound effect parameters.
	// 100.0 is very heavy, the top end for the mass parameter.
	float adjustedRelativeMass = (float)min(1.0f, fabsf(pCEvent->mass[0] - pCEvent->mass[1]) / 100.0f);

	static ICVar* mfx_ParticleImpactThresh = gEnv->pConsole->GetCVar("mfx_ParticleImpactThresh");
	const float particleImpactThresh = mfx_ParticleImpactThresh->GetFVal();
	float partImpThresh = particleImpactThresh;
	Vec3 vloc0Dir = pCEvent->vloc[0];
	vloc0Dir = vloc0Dir.normalize();
	float testSpeed = (pCEvent->vloc[0] * vloc0Dir.Dot(pCEvent->n)).GetLengthSquared();

	// prevent slow objects from making too many collision events by only considering the velocity towards
	//  the surface (prevents sliding creating tons of effects)
	if (impactVelSquared < (25.0f * 25.0f) && testSpeed < (partImpThresh * partImpThresh))
	{
		impactVelSquared = 0.0f;
	}

	if (!backface && impactVelSquared > (partImpThresh * partImpThresh))
	{
		IEntity* pEntitySrc = GetEnt(pCEvent->iForeignData[0], pCEvent->pForeignData[0]);
		IEntity* pEntityTrg = GetEnt(pCEvent->iForeignData[1], pCEvent->pForeignData[1]);

		IMaterialEffects* pMaterialEffects = gEnv->pMaterialEffects;
		TMFXEffectId effectId = InvalidEffectId;
		const int defaultSurfaceIndex = pMaterialEffects->GetDefaultSurfaceIndex();

		SMFXRunTimeEffectParams params;
		params.src = pEntitySrc ? pEntitySrc->GetId() : 0;
		params.trg = pEntityTrg ? pEntityTrg->GetId() : 0;
		params.srcSurfaceId = pCEvent->idmat[0];
		params.trgSurfaceId = pCEvent->idmat[1];
		params.soundSemantic = eSoundSemantic_Physics_Collision;

		if (pCEvent->iForeignData[0] == PHYS_FOREIGN_ID_STATIC)
		{
			params.srcRenderNode = (IRenderNode*)pCEvent->pForeignData[0];
		}
		if (pCEvent->iForeignData[1] == PHYS_FOREIGN_ID_STATIC)
		{
			params.trgRenderNode = (IRenderNode*)pCEvent->pForeignData[1];
		}
		if (pEntitySrc && pCEvent->idmat[0] == pMaterialEffects->GetDefaultCanopyIndex())
		{
			/*  //CryMP: We dont have s_this->m_treeStatus
			SVegCollisionStatus* test = s_this->m_treeStatus[params.src];
			if (!test)
			{
				IEntityRenderProxy* rp = (IEntityRenderProxy*)pEntitySrc->GetProxy(ENTITY_PROXY_RENDER);
				if (rp)
				{
					IRenderNode* rn = rp->GetRenderNode();
					if (rn)
					{
						effectId = pMaterialEffects->GetEffectIdByName("vegetation", "tree_impact");
						s_this->m_treeStatus[params.src] = new SVegCollisionStatus();
					}
				}
			}*/
		}

		//Prevent the same FX to be played more than once in mfx_Timeout time interval
		/*
		static ICVar* mfx_Timeout = gEnv->pConsole->GetCVar("mfx_Timeout");
		float fTimeOut = mfx_Timeout->GetFVal();
		for (int k = 0; k < MAX_CACHED_EFFECTS; k++)
		{
			SMFXRunTimeEffectParams& cachedParams = s_this->m_lstCachedEffects[k];
			if (cachedParams.src == params.src && cachedParams.trg == params.trg &&
				cachedParams.srcSurfaceId == params.srcSurfaceId && cachedParams.trgSurfaceId == params.trgSurfaceId &&
				cachedParams.srcRenderNode == params.srcRenderNode && cachedParams.trgRenderNode == params.trgRenderNode)
			{
				if (GetISystem()->GetITimer()->GetCurrTime() - cachedParams.fLastTime <= fTimeOut)
					return; // didnt timeout yet
			}
		}

		// add it overwriting the oldest one
		s_this->m_nEffectCounter = (s_this->m_nEffectCounter + 1) & (MAX_CACHED_EFFECTS - 1);
		*/


		//SMFXRunTimeEffectParams& cachedParams = s_this->m_lstCachedEffects[s_this->m_nEffectCounter]; 
		SMFXRunTimeEffectParams cachedParams;
		cachedParams.src = params.src;
		cachedParams.trg = params.trg;
		cachedParams.srcSurfaceId = params.srcSurfaceId;
		cachedParams.trgSurfaceId = params.trgSurfaceId;
		cachedParams.soundSemantic = params.soundSemantic;
		cachedParams.srcRenderNode = params.srcRenderNode;
		cachedParams.trgRenderNode = params.trgRenderNode;
		cachedParams.fLastTime = GetISystem()->GetITimer()->GetCurrTime();

		if (effectId == InvalidEffectId)
		{
			const char* pSrcArchetype = (pEntitySrc && pEntitySrc->GetArchetype()) ? pEntitySrc->GetArchetype()->GetName() : 0;
			const char* pTrgArchetype = (pEntityTrg && pEntityTrg->GetArchetype()) ? pEntityTrg->GetArchetype()->GetName() : 0;

			if (pEntitySrc)
			{
				if (pSrcArchetype)
					effectId = pMaterialEffects->GetEffectId(pSrcArchetype, pCEvent->idmat[1]);
				if (effectId == InvalidEffectId)
				{
					effectId = pMaterialEffects->GetEffectId(pEntitySrc->GetClass(), pCEvent->idmat[1]);
				}
			}
			//CryMP: projectile got removed by server, use class info if provided
			else if (pProjectileClass)
			{
				effectId = pMaterialEffects->GetEffectId(pProjectileClass, pCEvent->idmat[1]);
			}
			if (effectId == InvalidEffectId && pEntityTrg)
			{
				if (pTrgArchetype)
					effectId = pMaterialEffects->GetEffectId(pTrgArchetype, pCEvent->idmat[0]);

				if (effectId == InvalidEffectId)
				{
					effectId = pMaterialEffects->GetEffectId(pEntityTrg->GetClass(), pCEvent->idmat[0]);
				}
			}
		}

		if (effectId != InvalidEffectId)
		{
			//It's a bullet if it is a particle, has small mass and flies at high speed (>100m/s)
			bool isBullet = pCEvent->pEntity[0] ? (pCEvent->pEntity[0]->GetType() == PE_PARTICLE && pCEvent->vloc[0].len2() > 10000.0f && pCEvent->mass[0] < 1.0f) : false;

			IActor* pActor = nullptr;
			if (isBullet)
				pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(params.trg);
			params.pos = pCEvent->pt;

			if (isBullet && pActor && pActor->IsClient())
			{
				Vec3 proxyOffset(ZERO);
				Matrix34 tm = pActor->GetEntity()->GetWorldTM();
				tm.Invert();

				IMovementController* pMV = pActor->GetMovementController();
				if (pMV)
				{
					SMovementState state;
					pMV->GetMovementState(state);
					params.pos = state.eyePosition + (state.eyeDirection.normalize() * 1.0f);
					params.soundProxyEntityId = params.trg;
					params.soundProxyOffset = tm.TransformVector((state.eyePosition + (state.eyeDirection * 1.0f)) - state.pos);
					//Do not play FX in FP
					params.playflags = MFX_PLAY_ALL & ~MFX_PLAY_PARTICLES;
				}
			}

			static ICVar* g_blood = gEnv->pConsole->GetCVar("g_blood");

			// further, prevent ALL particle effects from playing if g_blood = 0
			if (pActor && g_blood->GetIVal() == 0)
			{
				params.playflags = MFX_PLAY_ALL & ~MFX_PLAY_PARTICLES;
			}

			// Check entity links for a 'Shooter'
			// if we find one and the local player is the shooter, we don't
			// need a raycast for sound obstruction/occlusion
			IEntityLink* pEntityLink = pEntitySrc ? pEntitySrc->GetEntityLinks() : 0;
			if (pEntityLink)
			{
				EntityId clientActorId = g_pGame->GetIGameFramework()->GetClientActorId();
				while (pEntityLink)
				{
					//Entity link is created in CProjectile::SetParams(), do we really need to check for the name (only the id perhaps)?
					if (strcmp("Shooter", pEntityLink->name) == 0)
					{
						if (clientActorId == pEntityLink->entityId)
							params.soundNoObstruction = true;

						// in any case, we're done
						break;
					}
					pEntityLink = pEntityLink->next;
				}
			}

			params.decalPos = pCEvent->pt;
			params.normal = pCEvent->n;
			Vec3 dir0 = pCEvent->vloc[0];
			Vec3 dir1 = pCEvent->vloc[1];

			//Water, ZeroG, ... parameters

			bool inWater = false, zeroG = false;
			float waterLevel = 0.0f;
			Vec3 gravity;
			pe_params_buoyancy pb[4];
			int nBuoys = gEnv->pPhysicalWorld->CheckAreas(pCEvent->pt, gravity, pb, 4);
			for (int i = 0; i < nBuoys; i++) if (pb[i].iMedium == 0 && (pCEvent->pt - pb[i].waterPlane.origin) * pb[i].waterPlane.n < 0)
			{
				waterLevel = pb[i].waterPlane.origin.z;
				break;
			}
			if (gravity.GetLength() < 0.0001f)
				zeroG = true;

			Vec3 pos = params.pos;
			if (waterLevel > 0.0f)
				inWater = (gEnv->p3DEngine->GetWaterLevel(&pos) > params.pos.z);

			params.inWater = inWater;
			params.inZeroG = zeroG;
			params.dir[0] = dir0.normalize();
			params.dir[1] = dir1.normalize();
			params.src = pEntitySrc ? pEntitySrc->GetId() : 0;
			params.trg = pEntityTrg ? pEntityTrg->GetId() : 0;
			params.partID = pCEvent->partid[1];

			float massMin = 0.0f;
			float massMax = 500.0f;
			float paramMin = 0.0f;
			float paramMax = 1.0f / 3.0f;

			// tiny - bullets
			if ((pCEvent->mass[0] <= 0.1f) && pCEvent->pEntity[0] && pCEvent->pEntity[0]->GetType() == PE_PARTICLE)
			{
				// small
				massMin = 0.0f;
				massMax = 0.1f;
				paramMin = 0.0f;
				paramMax = 1.0f;
			}
			else if (pCEvent->mass[0] < 20.0f)
			{
				// small
				massMin = 0.0f;
				massMax = 20.0f;
				paramMin = 0.0f;
				paramMax = 1.5f / 3.0f;
			}
			else if (pCEvent->mass[0] < 200.0f)
			{
				// medium
				massMin = 20.0f;
				massMax = 200.0f;
				paramMin = 1.0f / 3.0f;
				paramMax = 2.0f / 3.0f;
			}
			else
			{
				// ultra large
				massMin = 200.0f;
				massMax = 2000.0f;
				paramMin = 2.0f / 3.0f;
				paramMax = 1.0f;
			}

			float p = min(1.0f, (pCEvent->mass[0] - massMin) / (massMax - massMin));
			float finalparam = paramMin + (p * (paramMax - paramMin));

			// need to hear bullet impacts
			params.soundDistanceMult = pCEvent->mass[0] > 1.0f ? (finalparam * finalparam) + ((1.0f - finalparam) * .05f) : 1.0f;

			params.AddSoundParam("mass", finalparam);
			params.AddSoundParam("speed", adjustedRelativeVelocity);

			pMaterialEffects->ExecuteEffect(effectId, params);

			if (debug != 0)
			{
				pEntitySrc = GetEnt(pCEvent->iForeignData[0], pCEvent->pForeignData[0]);
				pEntityTrg = GetEnt(pCEvent->iForeignData[1], pCEvent->pForeignData[1]);

				ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
				CryLogAlways("[$3MFX$1] Running effect for:");
				if (pEntitySrc)
				{
					const char* pSrcName = pEntitySrc->GetName();
					const char* pSrcClass = pEntitySrc->GetClass()->GetName();
					const char* pSrcArchetype = pEntitySrc->GetArchetype() ? pEntitySrc->GetArchetype()->GetName() : "<none>";
					CryLogAlways("      : SrcClass=%s SrcName=%s Arch=%s", pSrcClass, pSrcName, pSrcArchetype);
				}
				if (pEntityTrg)
				{
					const char* pTrgName = pEntityTrg->GetName();
					const char* pTrgClass = pEntityTrg->GetClass()->GetName();
					const char* pTrgArchetype = pEntityTrg->GetArchetype() ? pEntityTrg->GetArchetype()->GetName() : "<none>";
					CryLogAlways("      : TrgClass=%s TrgName=%s Arch=%s", pTrgClass, pTrgName, pTrgArchetype);
				}
				CryLogAlways("      : Mat0=%s", pSurfaceTypeManager->GetSurfaceType(pCEvent->idmat[0])->GetName());
				CryLogAlways("      : Mat1=%s", pSurfaceTypeManager->GetSurfaceType(pCEvent->idmat[1])->GetName());
				CryLogAlways("impact-speed=%f fx-threshold=%f mass=%f speed=%f", sqrtf(impactVelSquared), partImpThresh, finalparam, adjustedRelativeVelocity);
			}

			return true;
		}
		else
		{
			if (debug != 0)
			{
				pEntitySrc = GetEnt(pCEvent->iForeignData[0], pCEvent->pForeignData[0]);
				pEntityTrg = GetEnt(pCEvent->iForeignData[1], pCEvent->pForeignData[1]);

				ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
				CryLogAlways("[$8MFX$1] Couldn't find effect for any combination of:");
				if (pEntitySrc)
				{
					const char* pSrcName = pEntitySrc->GetName();
					const char* pSrcClass = pEntitySrc->GetClass()->GetName();
					const char* pSrcArchetype = pEntitySrc->GetArchetype() ? pEntitySrc->GetArchetype()->GetName() : "<none>";
					CryLogAlways("      : SrcClass=%s SrcName=%s Arch=%s", pSrcClass, pSrcName, pSrcArchetype);
				}
				if (pEntityTrg)
				{
					const char* pTrgName = pEntityTrg->GetName();
					const char* pTrgClass = pEntityTrg->GetClass()->GetName();
					const char* pTrgArchetype = pEntityTrg->GetArchetype() ? pEntityTrg->GetArchetype()->GetName() : "<none>";
					CryLogAlways("      : TrgClass=%s TrgName=%s Arch=%s", pTrgClass, pTrgName, pTrgArchetype);
				}
				CryLogAlways("      : Mat0=%s", pSurfaceTypeManager->GetSurfaceType(pCEvent->idmat[0])->GetName());
				CryLogAlways("      : Mat1=%s", pSurfaceTypeManager->GetSurfaceType(pCEvent->idmat[1])->GetName());
			}
		}
	}

	return false;
	// --- End Material Effects Code ---
}

//------------------------------------------------------------------------
// RMI
//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestRename)
{
	if (ISSM* pSSM = g_pGame->GetSSM(); pSSM && !pSSM->IsRMILegitimate(pNetChannel, params.entityId)) {
		return true;
	}

	CActor* pActor = GetActorByEntityId(params.entityId);
	if (!pActor)
		return true;

	RenamePlayer(pActor, params.name.c_str());

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClRenameEntity)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(params.entityId);
	if (pEntity)
	{
		std::string old = pEntity->GetName();
		pEntity->SetName(params.name.c_str());

		CryLogAlways("$8%s$o renamed to $8%s", old.c_str(), params.name.c_str());

		CActor* pActor = GetActorByEntityId(params.entityId);
		if (pActor)
		{
			pActor->SaveNick(params.name.c_str());
		}

		// if this was a remote player, check we're not spectating them.
		//	If we are, we need to trigger a spectator hud update for the new name
		EntityId clientId = m_pGameFramework->GetClientActorId();
		if (gEnv->bMultiplayer && params.entityId != clientId)
		{
			CActor* pClientActor = static_cast<CActor*>(m_pGameFramework->GetClientActor());
			if (pClientActor && pClientActor->GetSpectatorMode() == CActor::eASM_Follow && pClientActor->GetSpectatorTarget() == params.entityId && g_pGame->GetHUD())
			{
				g_pGame->GetHUD()->RefreshSpectatorHUDText();
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestChatMessage)
{
	if (ISSM* pSSM = g_pGame->GetSSM(); pSSM && !pSSM->IsRMILegitimate(pNetChannel, params.sourceId)) {
		return true;
	}
	SendChatMessage((EChatMessageType)params.type, params.sourceId, params.targetId, params.msg.c_str());

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClChatMessage)
{
	OnChatMessage((EChatMessageType)params.type, params.sourceId, params.targetId, params.msg.c_str(), params.onlyTeam);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClForbiddenAreaWarning)
{
	SAFE_HUD_FUNC(ShowKillAreaWarning(params.active, params.timer));

	return true;
}


//------------------------------------------------------------------------

IMPLEMENT_RMI(CGameRules, SvRequestRadioMessage)
{
	SendRadioMessage(params.sourceId, params.msg);

	return true;
}

//------------------------------------------------------------------------

IMPLEMENT_RMI(CGameRules, ClRadioMessage)
{
	OnRadioMessage(SRadioMessageParams{
		.id = params.msg,
		.sourceId = params.sourceId
	});
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestChangeTeam)
{
	if (ISSM* pSSM = g_pGame->GetSSM(); pSSM && !pSSM->IsRMILegitimate(pNetChannel, params.entityId)) {
		return true;
	}

	CActor* pActor = GetActorByEntityId(params.entityId);
	if (!pActor)
		return true;

	ChangeTeam(pActor, params.teamId);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestSpectatorMode)
{
	if (ISSM* pSSM = g_pGame->GetSSM(); pSSM && !pSSM->IsRMILegitimate(pNetChannel, params.entityId)) {
		return true;
	}

	CActor* pActor = GetActorByEntityId(params.entityId);
	if (!pActor)
		return true;

	ChangeSpectatorMode(pActor, params.mode, params.targetId, params.resetAll);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetTeam)
{
	if (!params.entityId) // ignore these for now
		return true;

	int oldTeam = GetTeam(params.entityId);
	if (oldTeam == params.teamId)
		return true;

	TEntityTeamIdMap::iterator it = m_entityteams.find(params.entityId);
	if (it != m_entityteams.end())
		m_entityteams.erase(it);

	CActor* pActor = static_cast<CActor*>(m_pActorSystem->GetActor(params.entityId));
	const bool isplayer = pActor != nullptr;
	if (isplayer && oldTeam)
	{
		TPlayerTeamIdMap::iterator pit = m_playerteams.find(oldTeam);
		assert(pit != m_playerteams.end());
		stl::find_and_erase(pit->second, params.entityId);
	}

	if (isplayer)
	{
		//CryMP: Update m_teamId so that SetActorModel uses latest info
		pActor->NetSetTeamId(params.teamId);
	}

	if (params.teamId)
	{
		m_entityteams.insert(TEntityTeamIdMap::value_type(params.entityId, params.teamId));
		if (isplayer)
		{
			TPlayerTeamIdMap::iterator pit = m_playerteams.find(params.teamId);
			assert(pit != m_playerteams.end());
			pit->second.push_back(params.entityId);
		}
	}

	if (isplayer)
	{
		ReconfigureVoiceGroups(params.entityId, oldTeam, params.teamId);

		if (pActor->IsClient())
			m_pRadio->SetTeam(GetTeamName(params.teamId));
	}

	ScriptHandle handle(params.entityId);
	CallScript(m_clientStateScript, "OnSetTeam", handle, params.teamId);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClTextMessage)
{
	OnTextMessage((ETextMessageType)params.type, params.msg.c_str(),
		params.params[0].empty() ? 0 : params.params[0].c_str(),
		params.params[1].empty() ? 0 : params.params[1].c_str(),
		params.params[2].empty() ? 0 : params.params[2].c_str(),
		params.params[3].empty() ? 0 : params.params[3].c_str()
	);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestSimpleHit)
{
	ServerSimpleHit(params);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestHit)
{
	HitInfo info(params);
	info.remote = true;

	if (ISSM* pSSM = g_pGame->GetSSM(); pSSM && !pSSM->IsHitRMILegitimate(pNetChannel, info.shooterId, info.weaponId)) {
		return true;
	}

	ServerHit(info);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClExplosion)
{
	ClientExplosion(params);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClFreezeEntity)
{
	//IEntity *pEntity=gEnv->pEntitySystem->GetEntity(params.entityId);

	//CryLogAlways("ClFreezeEntity: %s %s", pEntity?pEntity->GetName():"<<null>>", params.freeze?"true":"false");

	FreezeEntity(params.entityId, params.freeze, 0);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClShatterEntity)
{
	ShatterEntity(params.entityId, params.pos, params.impulse);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetGameTime)
{
	m_endTime = params.endTime;

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetRoundTime)
{
	m_roundEndTime = params.endTime;

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetPreRoundTime)
{
	m_preRoundEndTime = params.endTime;

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetReviveCycleTime)
{
	m_reviveCycleEndTime = params.endTime;

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetGameStartTimer)
{
	m_gameStartTime = params.endTime;

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClTaggedEntity)
{
	if (!params.entityId)
		return true;

	//SAFE_HUD_FUNC(GetRadar()->AddTaggedEntity(params.entityId)); //we have no tagging anymore, just temp and non-temp adding
	SAFE_HUD_FUNC(GetRadar()->AddEntityToRadar(params.entityId));

	SEntityEvent scriptEvent(ENTITY_EVENT_SCRIPT_EVENT);
	scriptEvent.nParam[0] = (INT_PTR)"OnGPSTagged";
	scriptEvent.nParam[1] = IEntityClass::EVT_BOOL;
	bool bValue = true;
	scriptEvent.nParam[2] = (INT_PTR)&bValue;

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(params.entityId);
	if (pEntity)
		pEntity->SendEvent(scriptEvent);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClTempRadarEntity)
{
	SAFE_HUD_FUNC(GetRadar()->AddEntityTemporarily(params.entityId, 15.0f));

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClAddSpawnGroup)
{
	AddSpawnGroup(params.entityId);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClRemoveSpawnGroup)
{
	RemoveSpawnGroup(params.entityId);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClAddMinimapEntity)
{
	AddMinimapEntity(params.entityId, params.type, params.lifetime);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClRemoveMinimapEntity)
{
	RemoveMinimapEntity(params.entityId);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClResetMinimap)
{
	ResetMinimap();

	return true;
}


//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetObjective)
{
	CHUDMissionObjective* pObjective = SAFE_HUD_FUNC_RET(GetMissionObjectiveSystem().GetMissionObjective(params.name.c_str()));
	if (pObjective)
	{
		pObjective->SetStatus((CHUDMissionObjective::HUDMissionStatus)params.status);
		pObjective->SetTrackedEntity(params.entityId);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetObjectiveStatus)
{
	CHUDMissionObjective* pObjective = SAFE_HUD_FUNC_RET(GetMissionObjectiveSystem().GetMissionObjective(params.name.c_str()));
	if (pObjective)
		pObjective->SetStatus((CHUDMissionObjective::HUDMissionStatus)params.status);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetObjectiveEntity)
{
	CHUDMissionObjective* pObjective = SAFE_HUD_FUNC_RET(GetMissionObjectiveSystem().GetMissionObjective(params.name.c_str()));
	if (pObjective)
		pObjective->SetTrackedEntity(params.entityId);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClResetObjectives)
{
	SAFE_HUD_FUNC(GetMissionObjectiveSystem().DeactivateObjectives(false));

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClHitIndicator)
{
	SAFE_HUD_FUNC(IndicateHit(false, NULL, params.success));

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClDamageIndicator)
{
	Vec3 dir(ZERO);
	bool vehicle = false;

	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(params.shooterId))
	{
		if (IActor* pLocal = m_pGameFramework->GetClientActor())
		{
			dir = (pLocal->GetEntity()->GetWorldPos() - pEntity->GetWorldPos());
			dir.NormalizeSafe();

			vehicle = (pLocal->GetLinkedVehicle() != 0);
		}
	}

	SAFE_HUD_FUNC(IndicateDamage(params.weaponId, dir, vehicle));
	SAFE_HUD_FUNC(ShowTargettingAI(params.shooterId));
	return true;
}

//------------------------------------------------------------------------

IMPLEMENT_RMI(CGameRules, SvVote)
{
	CActor* pActor = GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel));
	if (pActor)
		Vote(pActor, true);
	return true;
}

IMPLEMENT_RMI(CGameRules, SvVoteNo)
{
	CActor* pActor = GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel));
	if (pActor)
		Vote(pActor, false);
	return true;
}

IMPLEMENT_RMI(CGameRules, SvStartVoting)
{
	CActor* pActor = GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel));
	if (pActor)
		StartVoting(pActor, params.vote_type, params.entityId, params.param.c_str());
	return true;
}

IMPLEMENT_RMI(CGameRules, ClVotingStatus)
{
	SAFE_HUD_FUNC(SetVotingState(params.state, params.timeout, params.entityId, params.description.c_str()));
	return true;
}


IMPLEMENT_RMI(CGameRules, ClEnteredGame)
{
	if (!gEnv->bServer && m_pGameFramework->GetClientActor())
	{
		CActor* pActor = GetActorByChannelId(m_pGameFramework->GetClientActor()->GetChannelId());
		if (pActor)
		{
			int status[2];
			status[0] = GetTeam(pActor->GetEntityId());
			status[1] = pActor->GetSpectatorMode();
			m_pGameplayRecorder->Event(pActor->GetEntity(), GameplayEvent(eGE_Connected, 0, 0, (void*)status));
		}
	}

	return true;
}
