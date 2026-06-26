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
#include "Throw.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryGame/Game.h"
#include "../Projectile.h"
#include "../WeaponSystem.h"
#include "../OffHand.h"
#include "CryGame/GameCVars.h"

//------------------------------------------------------------------------
CThrow::CThrow()
{
}

//------------------------------------------------------------------------
CThrow::~CThrow()
{
}


//------------------------------------------------------------------------
void CThrow::Update(float frameTime, unsigned int frameId)
{
	CSingle::Update(frameTime, frameId);

	if (m_firing)
	{
		if (!m_pulling && !m_throwing && !m_thrown)
		{
			if (m_hold_timer > 0.0f)
			{
				m_hold_timer -= frameTime;
				if (m_hold_timer < 0.0f)
					m_hold_timer = 0.0f;
			}
		}
		else if (m_throwing && m_throw_time <= 0.0f)
		{
			float strengthScale = 1.0f;
			CActor* pOwner = m_pWeapon->GetOwnerActor();

			m_pWeapon->HideItem(true);

			IEntity* pEntity = GetHeldEntity();
			if (pEntity)
			{
				if (m_throwableAction)
				{
					m_throwableAction->execute(m_pWeapon);
					m_throwableAction = nullptr;
				}

				IPhysicalEntity* pPE = pEntity->GetPhysics();
				if (pPE && (pPE->GetType() == PE_RIGID || pPE->GetType() == PE_PARTICLE || pPE->GetType() == PE_WHEELEDVEHICLE)) //CryMP: support for vehicles too
				{
					ThrowObject(pEntity, pPE);
				}
				else if (pPE && (pPE->GetType() == PE_LIVING || pPE->GetType() == PE_ARTICULATED))
				{
					ThrowLivingEntity(pEntity, pPE);
				}
			}
			else if (!m_netfiring)
			{
				ThrowGrenade();
			}

			m_throwing = false;
		}
		else if (m_thrown && m_throw_time <= 0.0f)
		{
			m_pWeapon->SetBusy(false);

			m_pWeapon->HideItem(false);

			const int ammoCount = m_pWeapon->GetAmmoCount(m_fireparams.ammo_type_class);
			if (ammoCount > 0)
			{
				m_pWeapon->PlayAction(m_throwactions.next);
			}
			else if (m_throwparams.auto_select_last)
			{
				if (CPlayer *pPlayer = CPlayer::FromActor(m_pWeapon->GetOwnerActor()))
				{
					pPlayer->SelectLastItem(true);
				}
			}

			m_firing = false;
			m_throwing = false;
			m_thrown = false;
		}

		m_throw_time -= frameTime;
		if (m_throw_time < 0.0f)
			m_throw_time = 0.0f;

		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}
}

//------------------------------------------------------------------------
void CThrow::ResetParams(const struct IItemParamsNode* params)
{
	CSingle::ResetParams(params);

	const IItemParamsNode* throwp = params ? params->GetChild("throw") : 0;
	const IItemParamsNode* throwa = params ? params->GetChild("actions") : 0;

	m_throwparams.Reset(throwp);
	m_throwactions.Reset(throwa);
}

//------------------------------------------------------------------------
void CThrow::PatchParams(const struct IItemParamsNode* patch)
{
	CSingle::PatchParams(patch);

	const IItemParamsNode* throwp = patch->GetChild("throw");
	const IItemParamsNode* throwa = patch->GetChild("actions");

	m_throwparams.Reset(throwp, false);
	m_throwactions.Reset(throwa, false);
}

//------------------------------------------------------------------------
void CThrow::Activate(bool activate)
{
	CSingle::Activate(activate);


	m_thrown = false;
	m_pulling = false;
	m_throwing = false;
	m_firing = false;
	m_hold_timer = 0.0f;
	//m_netfiring = false;

	m_throwableId = 0;

	m_throwableAction = nullptr;

	CheckAmmo();
}

//------------------------------------------------------------------------
bool CThrow::CanFire(bool considerAmmo) const
{
	return GetThrowable() || CSingle::CanFire(considerAmmo);// cannot be changed. it's used in CSingle::Shoot()
}

//------------------------------------------------------------------------
bool CThrow::CanReload() const
{
	return CSingle::CanReload() && !m_throwing;
}

//------------------------------------------------------------------------
bool CThrow::IsReadyToFire() const
{
	return CanFire(true) && !m_firing && !m_throwing && !m_pulling && !m_thrown;
}


//------------------------------------------------------------------------
void CThrow::StartFire()
{
	m_netfiring = false;

	if (CanFire(true) && !m_firing && !m_throwing && !m_pulling)
	{
		m_firing = true;
		m_pulling = true;
		m_throwing = false;
		m_thrown = false;
		m_hold_timer = m_throwparams.hold_duration;

		m_pWeapon->SetBusy(true);

		if (IsHoldingEntity())
		{
			//CryMP
			const int flags = CItem::ePlayActionFlags::eIPAF_Default & ~CItem::ePlayActionFlags::eIPAF_Animation;
			m_pWeapon->PlayAction(m_throwactions.pull, 0, false, flags);
		}
		else
		{
			m_pWeapon->PlayAction(m_throwactions.pull);
		}

		m_pWeapon->GetScheduler()->TimerAction(m_pWeapon->GetCurrentAnimationTime(CItem::eIGS_FirstPerson) + 1,
			MakeAction([this](CItem*) {
				this->m_pulling = false;
				if (!this->IsHoldingEntity()) // CryMP
				{
					this->m_pWeapon->PlayAction(this->m_throwactions.hold, 0, true,
						CItem::eIPAF_Default | CItem::eIPAF_NoBlend);
				}
				}),
			/*persistent=*/false
		);

		m_pWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, m_throwactions.hold);

		m_pWeapon->RequestStartFire();

		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}
}

//------------------------------------------------------------------------
void CThrow::StopFire()
{
	if (m_firing && !m_throwing && !m_thrown)
	{
		DoThrow(false);

		m_pWeapon->RequestStopFire();

		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}
}

//------------------------------------------------------------------------
void CThrow::NetStartFire()
{
	m_firing = true;
	m_throwing = false;
	m_thrown = false;
	m_pulling = false; // false here to not override network orders
	m_netfiring = true;
	m_hold_timer = m_throwparams.hold_duration;

	int flags = CItem::ePlayActionFlags::eIPAF_Default;
	const bool isHoldingEntity = IsHoldingEntity();
	if (isHoldingEntity)
	{
		if (!m_pWeapon->GetStats().fp)
		{
			flags = flags & ~CItem::ePlayActionFlags::eIPAF_Animation;
		}
	}

	m_pWeapon->PlayAction(m_throwactions.pull, 0, false, flags);

	m_pulling = false;
	if (!isHoldingEntity) //CryMP
	{
		m_pWeapon->PlayAction(m_throwactions.hold, 0, true, CItem::eIPAF_Default | CItem::eIPAF_NoBlend);
	}

	m_pWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, m_throwactions.hold);

	m_pWeapon->RequireUpdate(eIUS_FireMode);
}

//------------------------------------------------------------------------
void CThrow::NetStopFire()
{
	if (m_netfiring)
	{
		m_pWeapon->RequireUpdate(eIUS_FireMode);
		DoThrow(true);
	}
}

//------------------------------------------------------------------------
void CThrow::SetThrowable(EntityId entityId, bool forceThrow, ISchedulerAction* action)
{
	m_throwableId = entityId;
	m_throwableAction = action;
	m_forceNextThrow = forceThrow;
}

//------------------------------------------------------------------------
EntityId CThrow::GetThrowable() const
{
	return m_throwableId;
}

//------------------------------------------------------------------------
void CThrow::CheckAmmo()
{
	const bool hide = (m_fireparams.ammo_type_class == nullptr || m_pWeapon->GetAmmoCount(m_fireparams.ammo_type_class) <= 0) && m_throwparams.hide_ammo;


	m_pWeapon->HideItem(hide);
}

void CThrow::DoThrow(bool netPlayer)
{
	m_throw_time = m_throwparams.delay;
	m_throwing = true;
	m_thrown = false;

	bool drop = false;

	if ((!m_pWeapon->IsWeaponLowered() && m_forceNextThrow) || m_usingGrenade)
	{
		const bool isHoldingEntity = IsHoldingEntity();
		if (isHoldingEntity)
		{
			//CryMP:
			int flags = CItem::ePlayActionFlags::eIPAF_Default;
			if (!m_pWeapon->GetStats().fp)
			{
				flags = flags & ~CItem::ePlayActionFlags::eIPAF_Animation;
			}
			m_pWeapon->PlayAction(m_throwactions.throwit, 0, false, flags);
		}
		else
		{
			m_pWeapon->PlayAction(m_throwactions.throwit);
		}
	}
	else
	{
		m_pWeapon->PlayAction(m_throwactions.dropit);
		m_throwing = false;

		DoDrop(netPlayer);
		
		drop = true;
	}
	m_forceNextThrow = false;

	if (CPlayer* pPlayer = CPlayer::FromActor(m_pWeapon->GetOwnerActor()))
	{
		if (CNanoSuit* pSuit = pPlayer->GetNanoSuit())
		{
			ENanoMode curMode = pSuit->GetMode();
			if (curMode == NANOMODE_STRENGTH)
			{
				IEntity* pThrowable = GetHeldEntity();
				if (pThrowable)	//set sound intensity by item mass (sound request)
				{
					IPhysicalEntity* pEnt(pThrowable->GetPhysics());
					float mass(0);
					float massFactor = 0.3f;
					if (pEnt)
					{
						pe_status_dynamics dynStat;
						if (pEnt->GetStatus(&dynStat))
							mass = dynStat.mass;
						if (mass > pPlayer->GetActorParams()->maxGrabMass)
							massFactor = 1.0f;
						else if (mass > 30)
							massFactor = 0.6f;
					}
					if (!drop)
					{
						pSuit->PlaySound(STRENGTH_THROW_SOUND, massFactor);

						if (gEnv->bServer)
							pSuit->SetSuitEnergy(pSuit->GetSuitEnergy() - (40.0f * massFactor));
					}
				}
				else if (!drop)
				{
					pSuit->PlaySound(STRENGTH_THROW_SOUND, (pSuit->GetSlotValue(NANOSLOT_STRENGTH)) * 0.01f);
				}
			}
			if (curMode == NANOMODE_CLOAK)
			{
				if (gEnv->bServer)
					pSuit->SetSuitEnergy(pSuit->GetSuitEnergy() - 100.0f);
			}
		}
	}

	m_pWeapon->GetScheduler()->TimerAction(m_pWeapon->GetCurrentAnimationTime(CItem::eIGS_FirstPerson),
		MakeAction([this](CItem*) {
			this->m_thrown = true;

			}),
		/*persistent=*/false
	);

	m_pWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
}

//--------------------------------------
void CThrow::DoDrop(bool netPlayer)
{
	m_pWeapon->HideItem(true);

	IEntity* pEntity = GetHeldEntity();
	if (pEntity)
	{
		IPhysicalEntity* pPE = pEntity->GetPhysics();
		if (pPE && (pPE->GetType() == PE_RIGID || pPE->GetType() == PE_PARTICLE))
		{
			Vec3 hit = GetProbableHit(WEAPON_HIT_RANGE);
			Vec3 pos = GetFiringPos(hit);

			CActor* pActor = m_pWeapon->GetOwnerActor();
			IMovementController* pMC = pActor ? pActor->GetMovementController() : 0;
			if (pMC)
			{
				SMovementState info;
				pMC->GetMovementState(info);
				float speed = 2.5f;

				CPlayer* pPlayer = m_pWeapon->GetOwnerPlayer();
				if (pPlayer && info.aimDirection.z < -0.1f)
				{
					if (pPlayer->GetPlayerStats()->grabbedHeavyEntity)
					{
						speed = 4.0f;
					}
				}

				if (CheckForIntersections(pPE, info.eyeDirection))
				{
					Matrix34 newTM = pEntity->GetWorldTM();
					newTM.SetTranslation(newTM.GetTranslation() - (info.eyeDirection * 0.4f));
					pEntity->SetWorldTM(newTM, ENTITY_XFORM_POS);

					pe_action_set_velocity asv;
					asv.v = (-info.eyeDirection * speed);
					pPE->Action(&asv);
				}
				else
				{
					pe_action_set_velocity asv;
					asv.v = (info.eyeDirection * speed);
					pPE->Action(&asv);
				}

				SEntityEvent entityEvent;
				entityEvent.event = ENTITY_EVENT_PICKUP;
				entityEvent.nParam[0] = 0;
				if (pPlayer)
					entityEvent.nParam[1] = pPlayer->GetEntityId();
				entityEvent.fParam[0] = speed;
				pEntity->SendEvent(entityEvent);
			}
		}
	}
	if (m_throwableAction)
	{
		m_throwableAction->execute(m_pWeapon);
		m_throwableAction = nullptr;
	}
}

//-----------------------------------------------------
void CThrow::ThrowGrenade()
{
	//Grenade speed scale is always one (for player)
	if (CPlayer* pPlayer = static_cast<CPlayer*>(m_pWeapon->GetOwnerActor()))
	{
		if (pPlayer->IsPlayer())
		{
			m_speed_scale = 1.0f;
			if (pPlayer->GetNanoSuit() && (pPlayer->GetNanoSuit()->GetMode() == NANOMODE_STRENGTH))
				m_speed_scale = m_throwparams.strenght_scale;
		}
		else if (pPlayer->GetHealth() <= 0 || pPlayer->GetGameObject()->GetAspectProfile(eEA_Physics) == eAP_Sleep)
			return; //Do not throw grenade is player is death (AI "ghost grenades")

		//Hide grenade in hand (FP)
		if ((pPlayer->IsClient() || pPlayer->IsFpSpectatorTarget()) && m_pWeapon->GetEntity()->GetClass() == CItem::sOffHandClass)
		{
			if (COffHand* pOffHand = static_cast<COffHand*>(m_pWeapon))
			{
				pOffHand->AttachGrenadeToHand(pOffHand->GetCurrentFireMode(), m_pWeapon->GetStats().fp, false);
			}
		}
	}

	m_pWeapon->SetBusy(false);
	Shoot(true);
	m_pWeapon->SetBusy(true);
}
//-----------------------------------------------------
void CThrow::ThrowObject(IEntity* pEntity, IPhysicalEntity* pPE)
{
	if (!pEntity || !pPE)
		return;

	bool strengthMode = false;

	CPlayer* pPlayer = static_cast<CPlayer*>(m_pWeapon->GetOwnerActor());
	if (pPlayer && pPlayer->GetNanoSuit())
	{
		strengthMode = pPlayer->GetNanoSuit()->GetMode() == NANOMODE_STRENGTH;
		// Report throw to AI system.
		if (pPlayer->GetEntity() && pPlayer->GetEntity()->GetAI())
		{
			SAIEVENT AIevent;
			AIevent.targetId = pEntity->GetId();
			pPlayer->GetEntity()->GetAI()->Event(strengthMode ? AIEVENT_PLAYER_STUNT_THROW : AIEVENT_PLAYER_THROW, &AIevent);
		}
	}

	Vec3 hit = GetProbableHit(WEAPON_HIT_RANGE);
	Vec3 pos = GetFiringPos(hit);
	Vec3 dir = ApplySpread(GetFiringDir(hit, pos), GetSpread());
	Vec3 vel = GetFiringVelocity(dir);

	float speed = 12.0f;
	if (strengthMode)
		speed *= m_throwparams.strenght_scale;

	speed = std::max(2.0f, speed);

	pe_params_pos ppos;
	ppos.pos = pEntity->GetWorldPos();
	pPE->SetParams(&ppos);

	if (CheckForIntersections(pPE, dir))
	{
		Matrix34 newTM = pEntity->GetWorldTM();
		newTM.SetTranslation(newTM.GetTranslation() - (dir * 0.4f));
		pEntity->SetWorldTM(newTM, ENTITY_XFORM_POS);

		if (gEnv->bMultiplayer)
		{
			IEntityPhysicalProxy* pPhysicalProxy = (IEntityPhysicalProxy*)pEntity->GetProxy(ENTITY_PROXY_PHYSICS);
			if (pPhysicalProxy)
			{
				pPhysicalProxy->AddImpulse(-1, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f) * 1.5f, false, 1.0f);
			}
		}
	}
	else
	{
		pe_action_set_velocity asv;
		asv.v = (dir * speed * g_pGameCVars->mp_pickupThrowMult) + vel;
		AABB box;
		pEntity->GetWorldBounds(box);

		Vec3 dir(gEnv->pSystem->GetViewCamera().GetMatrix().GetColumn0());
		Vec3 finalW = -dir * (8.0f / max(0.1f, box.GetRadius()));
		finalW.x *= Random(0.5f, 1.3f);
		finalW.y *= Random(0.5f, 1.3f);
		finalW.z *= Random(0.5f, 1.3f);
		asv.w = finalW;
		//asv.w = Vec3(Random(-4.5f,3.5f),Random(-1.75f,2.5f),Random(-1.5f,2.2f));
		pPE->Action(&asv);
	}

	SEntityEvent entityEvent;
	entityEvent.event = ENTITY_EVENT_PICKUP;
	entityEvent.nParam[0] = 0;
	if (pPlayer)
		entityEvent.nParam[1] = pPlayer->GetEntityId();
	entityEvent.fParam[0] = speed;
	pEntity->SendEvent(entityEvent);

	//CryMP reset...
	m_throwableId = 0;
}

//-----------------------------------------------------
void CThrow::ThrowLivingEntity(IEntity* pEntity, IPhysicalEntity* pPE)
{
	Vec3 hit = GetProbableHit(WEAPON_HIT_RANGE);
	Vec3 pos = GetFiringPos(hit);
	Vec3 dir = ApplySpread(GetFiringDir(hit, pos), GetSpread());
	Vec3 vel = GetFiringVelocity(dir);

	CPlayer* pPlayer = static_cast<CPlayer*>(m_pWeapon->GetOwnerActor());
	if (pPlayer && pPlayer->GetNanoSuit())
	{
		float speed = 8.0f;
		dir.Normalize();

		if (pPlayer->GetNanoSuit()->GetMode() == NANOMODE_STRENGTH)
			speed *= m_throwparams.strenght_scale;

		if (CheckForIntersections(pPE, dir))
		{
			Matrix34 newTM = pEntity->GetWorldTM();
			newTM.SetTranslation(newTM.GetTranslation() - (dir * 0.6f));
			pEntity->SetWorldTM(newTM, ENTITY_XFORM_POS);

		}

		{
			pe_action_set_velocity asv;
			asv.v = (dir * speed * g_pGameCVars->mp_pickupThrowMult) + vel;
			pPE->Action(&asv);
			// [anton] use thread safe=1 (immediate) if the character is still a living entity at this stage, 
			//   but will be ragdollized during the same frame
		}

		// Report throw to AI system.
		if (pPlayer->GetEntity() && pPlayer->GetEntity()->GetAI())
		{
			SAIEVENT AIevent;
			AIevent.targetId = pEntity->GetId();
			pPlayer->GetEntity()->GetAI()->Event(AIEVENT_PLAYER_STUNT_THROW_NPC, &AIevent);
		}
	}
}

//----------------------------------------------------
bool CThrow::CheckForIntersections(IPhysicalEntity* heldEntity, Vec3& dir)
{
	Vec3 checkPos = m_pWeapon->GetEntity()->GetWorldPos();

	if (CActor* pActor = m_pWeapon->GetOwnerActor())
	{
		if (IMovementController* pMC = pActor->GetMovementController())
		{
			SMovementState movementState;
			pMC->GetMovementState(movementState);
			checkPos = movementState.eyePosition;
		}
	}

	if (m_pWeapon->GetStats().fp)
	{
		Vec3 fpPos = m_pWeapon->GetSlotHelperPos(CItem::eIGS_FirstPerson, "item_attachment", true);
		checkPos = fpPos - dir * 0.4f;
	}

	ray_hit hit;
	if (gEnv->pPhysicalWorld->RayWorldIntersection(
		checkPos, dir,
		ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid,
		rwi_stop_at_pierceable | 14,
		&hit, 1, heldEntity))
	{
		return true;
	}

	return false;
}

//----------------------------------------------------
bool CThrow::IsHoldingEntity() const
{
	//if (m_pWeapon->GetEntity()->GetClass() == CItem::sOffHandClass)
	//{
	//	if (COffHand* pOffHand = static_cast<COffHand*>(m_pWeapon))
	//	{
	//		return pOffHand->GetHeldEntityId() != 0;
	//	}
	//}
	//
	//return false;
	return GetThrowable() != 0;
}

//----------------------------------------------------
IEntity* CThrow::GetHeldEntity()
{
	//if (m_pWeapon->GetEntity()->GetClass() == CItem::sOffHandClass)
	//{
	//	if (COffHand* pOffHand = static_cast<COffHand*>(m_pWeapon))
	//	{
	//		return gEnv->pEntitySystem->GetEntity(pOffHand->GetHeldEntityId());
	//	}
	//}

	//return nullptr;
	return gEnv->pEntitySystem->GetEntity(GetThrowable());
}

//-----------------------------------------------------
void CThrow::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	CSingle::GetMemoryStatistics(s);
	m_throwactions.GetMemoryStatistics(s);
}

//-----------------------------------------------------
void CThrow::Serialize(TSerialize ser)
{
	CSingle::Serialize(ser);
	if(ser.GetSerializationTarget() != eST_Network)
	{
	}
}
