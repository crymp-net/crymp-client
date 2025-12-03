/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 11:9:2005   15:00 : Created by Márcio Martins

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "Plant.h"
#include "../../Item.h"
#include "../Weapon.h"
#include "../Projectile.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/Game.h"
#include "../C4.h"


IEntityClass* CPlant::m_pAVMineClass = NULL;
IEntityClass* CPlant::m_pClaymoreClass = NULL;

//------------------------------------------------------------------------
CPlant::CPlant()
: m_projectileId(0)
{
	m_projectiles.clear();
	m_plantPos.zero();
	m_plantDir.zero();
	m_plantVel.zero();
}

//------------------------------------------------------------------------
CPlant::~CPlant()
{
	m_projectiles.clear();
}

//------------------------------------------------------------------------
void CPlant::Init(IWeapon *pWeapon, const struct IItemParamsNode *params)
{
	m_pWeapon = static_cast<CWeapon *>(pWeapon);

	if (params)
		ResetParams(params);
}


//------------------------------------------------------------------------
void CPlant::UpdateFPView(float frameTime)
{
}


//------------------------------------------------------------------------
void CPlant::Release()
{
	delete this;
}


//------------------------------------------------------------------------
void CPlant::ResetParams(const struct IItemParamsNode *params)
{
	const IItemParamsNode *plant = params?params->GetChild("plant"):0;
	const IItemParamsNode *actions = params?params->GetChild("actions"):0;
	m_plantparams.Reset(plant, true);
	m_plantactions.Reset(actions, true);
}

//------------------------------------------------------------------------
void CPlant::PatchParams(const struct IItemParamsNode *patch)
{
	const IItemParamsNode *plant = patch?patch->GetChild("plant"):0;
	const IItemParamsNode *actions = patch?patch->GetChild("actions"):0;
	m_plantparams.Reset(plant, false);
	m_plantactions.Reset(actions, false);
}

//------------------------------------------------------------------------
void CPlant::Activate(bool activate)
{
	m_planting = false;
	m_pressing = false;
	m_holding = false;
	m_timing = false;

	m_time = 0.0f;
	m_tickTimer = 0.0f;

	CheckAmmo();
}

//------------------------------------------------------------------------
bool CPlant::OutOfAmmo() const
{
	if (m_plantparams.clip_size!=0)
		return m_plantparams.ammo_type_class && m_plantparams.clip_size != -1 && m_pWeapon->GetAmmoCount(m_plantparams.ammo_type_class)<1;

	return m_plantparams.ammo_type_class && m_pWeapon->GetInventoryAmmoCount(m_plantparams.ammo_type_class)<1;
}

//------------------------------------------------------------------------
void CPlant::NetShoot(const Vec3 &hit, int ph)
{
	NetShootEx(ZERO, ZERO, ZERO, hit, 1.0f, ph);
}

//------------------------------------------------------------------------
void CPlant::NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int ph)
{
	Plant(pos, dir, vel, true, ph);
		
	CActor* pActor = m_pWeapon->GetOwnerActor();
	if (pActor)
	{
		const bool fpSpecTarget = pActor->IsFpSpectatorTarget();
		unsigned int flags = CItem::eIPAF_Default;
		if (!fpSpecTarget)
		{
			flags = flags & ~(CItem::eIPAF_Sound); //Currently no 3D sound for 3rd person planting
		}
		m_pWeapon->PlayAction(ItemString(m_plantactions.plant.c_str()), 0, false, flags, fpSpecTarget ? 2.0f : 1.0f);

		if (fpSpecTarget)
		{
			m_pWeapon->GetScheduler()->TimerAction(
				m_pWeapon->GetCurrentAnimationTime(CItem::eIGS_FirstPerson),
				MakeAction([this](CItem* /*unused*/) {
					m_pWeapon->PlayAction(g_pItemStrings->select, 0, false, CItem::eIPAF_Default | CItem::eIPAF_NoBlend | CItem::eIPAF_CleanBlending);
					m_pWeapon->HideItem(false);

					}),
				/*persistent=*/false
			);
		}
		else
		{
			m_pWeapon->HideItem(false);
		}
	}

}

//------------------------------------------------------------------------
void CPlant::NetStartFire()
{
}

//------------------------------------------------------------------------
void CPlant::NetStopFire()
{
}

//------------------------------------------------------------------------
const char *CPlant::GetType() const
{
	return "Plant";
}

//------------------------------------------------------------------------
IEntityClass* CPlant::GetAmmoType() const
{
	return m_plantparams.ammo_type_class;
}

//------------------------------------------------------------------------
int CPlant::GetDamage(float distance) const
{
	return m_plantparams.damage;
}

//------------------------------------------------------------------------
void CPlant::Update(float frameTime, unsigned int frameId)
{
	
}

// returns true if firing is allowed
bool CPlant::GetPlantingParameters(Vec3& pos, Vec3& dir, Vec3& vel) const
{
	CActor *pActor = m_pWeapon->GetOwnerActor();
	IMovementController *pMC = pActor?pActor->GetMovementController():0;
	SMovementState info;
	if (pMC)
		pMC->GetMovementState(info);

	if (m_pWeapon->GetStats().fp)
		pos = m_pWeapon->GetSlotHelperPos(CItem::eIGS_FirstPerson, m_plantparams.helper.c_str(), true);
	else if (pMC)
		pos = info.eyePosition+info.eyeDirection*0.25f;
	else if (pActor)
		pos = pActor->GetEntity()->GetWorldPos();

	if (pActor)
	{
		if (pMC)
			dir = info.eyeDirection;
		else
			dir = pActor->GetEntity()->GetWorldRotation().GetColumn1();

		if (IPhysicalEntity* pPE = pActor->GetEntity()->GetPhysics())
		{
			pe_status_dynamics sv;
			if (pPE->GetStatus(&sv))
			{
				if (sv.v.len2() > 0.01f)
				{
					float dot = sv.v.GetNormalized().Dot(dir);
					if (dot < 0.0f)
						dot = 0.0f;
					vel = sv.v * dot;
				}
			}
		}
	}

	// if the ammo should be placed (claymore/mine) rather than thrown forward (c4), check if we can do so...
	if(m_plantparams.place_on_ground)
	{
		ray_hit hit;
		static const int objTypes = ent_static | ent_terrain;
		static const unsigned int flags = rwi_stop_at_pierceable|rwi_colltype_any;
		static IPhysicalEntity* pSkipEnts[10];
		int nskip = CSingle::GetSkipEntities(m_pWeapon, pSkipEnts, 10);
		int res = gEnv->pPhysicalWorld->RayWorldIntersection(pos, 2.0f * dir, objTypes, flags, &hit, 1, pSkipEnts, nskip);
		if(!res)
			return false;
		else
		{
			// check surface normal - must be close to up
			if(hit.n.z < 0.8f)
				return false;

 			// special case to stop stacking of claymores/mines (they are static so are hit by the ray)
 			if(hit.pCollider && hit.pCollider->GetiForeignData() == PHYS_FOREIGN_ID_ENTITY)
			{
				IEntity * pEntity = (IEntity*)hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
				if(pEntity)
				{
					if(!m_pClaymoreClass)
						m_pClaymoreClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("claymoreexplosive");
					if(!m_pAVMineClass)
						m_pAVMineClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("avexplosive");

					if(pEntity->GetClass() == m_pClaymoreClass || pEntity->GetClass() == m_pAVMineClass)
						return false;
				}
 			}

			// second check to see if there is another object in the way
			float hitDist = hit.dist;
			int res = gEnv->pPhysicalWorld->RayWorldIntersection(pos, 2.0f * dir, ent_all, flags, &hit, 1, pSkipEnts, nskip);
			if(res && hit.dist < hitDist-0.1f)
			{
				return false;
			}

			pos = hit.pt;
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CPlant::CanFire(bool considerAmmo/* =true */) const
{
	return !m_planting && !m_pressing && !m_pWeapon->IsBusy() && (!considerAmmo || !OutOfAmmo()) && PlayerStanceOK();
}

//------------------------------------------------------------------------
struct CPlant::StartPlantAction
{
	StartPlantAction(CPlant *_plant): pPlant(_plant) {};
	CPlant *pPlant;

	void execute(CItem *_this)
	{
		pPlant->m_pWeapon->SetBusy(false);
		pPlant->m_planting = false;
		
		if(pPlant->m_pWeapon->OutOfAmmo(false))
		{
			if(!pPlant->m_plantparams.simple)
				pPlant->SelectDetonator();
			else
			{
				if(pPlant->m_pWeapon->GetOwnerActor())
					pPlant->m_pWeapon->GetOwnerActor()->SelectNextItem(1, false);
			}
		}
		else
		{
			pPlant->m_pWeapon->PlayAction(g_pItemStrings->select,0,false,CItem::eIPAF_Default|CItem::eIPAF_NoBlend|CItem::eIPAF_CleanBlending);
			pPlant->m_pWeapon->HideItem(false);
		}
	}
};

//------------------------------------------------------------------------
struct CPlant::PlantTimer
{
	PlantTimer(CPlant* _plant) : pPlant(_plant) {};
	CPlant* pPlant;

	void execute(CItem* _this)
	{
		if (pPlant->m_planting)
		{
			Vec3 pos;
			Vec3 dir(FORWARD_DIRECTION);
			Vec3 vel(0, 0, 0);

			// placed weapons have already stored the position etc to place at.
			if (pPlant->m_plantparams.place_on_ground)
			{
				pPlant->Plant(pPlant->m_plantPos, pPlant->m_plantDir, pPlant->m_plantVel);
			}
			else if (pPlant->GetPlantingParameters(pos, dir, vel))
			{
				pPlant->Plant(pos, dir, vel);
			}

			pPlant->m_plantPos.zero();
			pPlant->m_plantDir.zero();
			pPlant->m_plantVel.zero();
		}
	}
};

//------------------------------------------------------------------------
void CPlant::StartFire()
{
	if (!CanFire(true))
		return;

	if(m_pWeapon->IsWeaponLowered())
		return;

	//m_pWeapon->RequireUpdate(eIUS_FireMode); //CryMP: Don't think this is needed anymore?

	GetPlantingParameters(m_plantPos, m_plantDir, m_plantVel);

	m_planting = true;
	m_pWeapon->SetBusy(true);

	CActor* pActor = m_pWeapon->GetOwnerActor();
	if (pActor && pActor->IsClient())
	{
		m_pWeapon->PlayAction(ItemString(m_plantactions.plant.c_str()));
	}

	m_pWeapon->GetScheduler()->TimerAction(m_pWeapon->GetCurrentAnimationTime(CItem::eIGS_FirstPerson), 
		CSchedulerAction<StartPlantAction>::Create(this), false);

	m_pWeapon->GetScheduler()->TimerAction(static_cast<uint32_t>(m_plantparams.delay * 1000.f), //CryMP added
		CSchedulerAction<PlantTimer>::Create(this), false);
}

//------------------------------------------------------------------------
void CPlant::StopFire()
{
	m_timing=false;
	m_pressing=false;
}

//------------------------------------------------------------------------
void CPlant::Serialize(TSerialize ser)
{
	if (ser.GetSerializationTarget()!=eST_Network)
	{
		ser.Value("projectileId", m_projectileId);
		ser.Value("projectiles", m_projectiles);
	}
}

//------------------------------------------------------------------------
void CPlant::Plant(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, bool net, int ph)
{
	IEntityClass* ammo = m_plantparams.ammo_type_class;
	int ammoCount = m_pWeapon->GetAmmoCount(ammo);
	if (m_plantparams.clip_size==0)
		ammoCount = m_pWeapon->GetInventoryAmmoCount(ammo);

	CProjectile* pAmmo = m_pWeapon->SpawnAmmo(ammo, net);
	CActor *pActor=m_pWeapon->GetOwnerActor();
	if (pAmmo)
	{
		pAmmo->SetParams(m_pWeapon->GetOwnerId(), m_pWeapon->GetHostId(), m_pWeapon->GetEntityId(), 
			m_pWeapon->GetFireModeIdx(GetName()), m_plantparams.damage, 0);

		if (!net)
			pAmmo->SetSequence(m_pWeapon->GenerateShootSeqN());
		pAmmo->SetDestination(m_pWeapon->GetDestination());
		pAmmo->Launch(pos, dir, vel);

		m_projectileId = pAmmo->GetEntity()->GetId();

		if (m_projectileId && !m_plantparams.simple && m_pWeapon->IsServer())
		{
			if (pActor)
			{
				SetProjectileId(m_projectileId);
				m_pWeapon->GetGameObject()->InvokeRMIWithDependentObject(CC4::ClSetProjectileId(), CC4::SetProjectileIdParams(m_projectileId, m_pWeapon->GetCurrentFireMode()), eRMI_ToClientChannel, m_projectileId, pActor->GetChannelId());
			}
		}
	}

	m_pWeapon->OnShoot(m_pWeapon->GetOwnerId(), pAmmo?pAmmo->GetEntity()->GetId():0, GetAmmoType(), pos, dir, vel);

	ammoCount--;
	if (m_plantparams.clip_size != -1)
	{
		if (m_plantparams.clip_size!=0)
			m_pWeapon->SetAmmoCount(ammo, ammoCount);
		else
			m_pWeapon->SetInventoryAmmoCount(ammo, ammoCount);
	}

	if (pAmmo && ph && pActor)
	{
		pAmmo->GetGameObject()->RegisterAsValidated(pActor->GetGameObject(), ph);
		pAmmo->GetGameObject()->BindToNetwork();
	}
	else if (pAmmo && pAmmo->IsPredicted() && gEnv->bClient && gEnv->bServer && pActor && pActor->IsClient())
	{
		pAmmo->GetGameObject()->BindToNetwork();
	}

	if (!net)
		m_pWeapon->RequestShoot(ammo, pos, dir, vel, ZERO, 1.0f, pAmmo? pAmmo->GetGameObject()->GetPredictionHandle() : 0, m_pWeapon->GetShootSeqN(), 0, true);

	m_pWeapon->HideItem(true);
}

//=======================================================================
void CPlant::SelectDetonator()
{
	if (CActor *pOwner=m_pWeapon->GetOwnerActor())
	{
		EntityId detonatorId = pOwner->GetInventory()->GetItemByClass(CItem::sDetonatorClass);
		if (detonatorId)
			pOwner->SelectItemByName("Detonator", false);
	}
}

//========================================================================
void CPlant::SetProjectileId(EntityId id)
{
	stl::push_back_unique(m_projectiles,id);
}

//====================================================================
EntityId CPlant::GetProjectileId() const
{
	if(!m_projectiles.empty())
		return m_projectiles.back();

	return 0;
}
//=====================================================================
EntityId CPlant::RemoveProjectileId() 
{
	EntityId id = 0;
	if(!m_projectiles.empty())
	{
		id= m_projectiles.back();
		m_projectiles.pop_back();
	}
	return id;
}

//------------------------------------------------------------------------
void CPlant::CheckAmmo()
{
	m_pWeapon->HideItem(OutOfAmmo());
}

void CPlant::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->Add(m_name);
	m_plantparams.GetMemoryStatistics(s);
	m_plantactions.GetMemoryStatistics(s);
}

bool CPlant::PlayerStanceOK() const
{
	if (m_plantparams.need_to_crouch)
	{
		CActor* pActor = m_pWeapon->GetOwnerActor();
		if (pActor)
		{
			bool ok = (pActor->GetStance() == STANCE_CROUCH);
			if (ok)
			{
				ok &= (pActor->GetActorStats()->velocity.GetLengthSquared() < 0.1f);

				if (ok)
				{
					// also fire a ray forwards to check the placement position is nearby
					Vec3 pos;
					Vec3 dir(FORWARD_DIRECTION);
					Vec3 vel(0, 0, 0);
					ok &= GetPlantingParameters(pos, dir, vel);
				}
			}
			return ok;
		}
	}
	
	return true;
}
