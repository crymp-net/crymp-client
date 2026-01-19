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
#include "Rapid.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryCommon/CrySoundSystem/ISound.h"
#include "CryGame/Game.h"
#include "CryGame/GameCVars.h"
#include "CryCommon/CryAction/IViewSystem.h"

//------------------------------------------------------------------------
CRapid::CRapid()
{
}

//------------------------------------------------------------------------
CRapid::~CRapid()
{
}

//------------------------------------------------------------------------
void CRapid::ResetParams(const struct IItemParamsNode* params)
{
	CSingle::ResetParams(params);

	const IItemParamsNode* actions = params ? params->GetChild("actions") : 0;
	const IItemParamsNode* rapid = params ? params->GetChild("rapid") : 0;

	m_rapidactions.Reset(actions);
	m_rapidparams.Reset(rapid);

	m_barrelBaseTMValid = false;
}

//------------------------------------------------------------------------
void CRapid::PatchParams(const struct IItemParamsNode* patch)
{
	CSingle::PatchParams(patch);

	const IItemParamsNode* actions = patch->GetChild("actions");
	const IItemParamsNode* rapid = patch->GetChild("rapid");

	m_rapidactions.Reset(actions, false);
	m_rapidparams.Reset(rapid, false);

	m_barrelBaseTMValid = false;
}

//------------------------------------------------------------------------
void CRapid::Activate(bool activate)
{
	CSingle::Activate(activate);

	if (!activate)
	{
		if (m_soundId != INVALID_SOUNDID)
		{
			m_pWeapon->StopSound(m_soundId);
			m_soundId = INVALID_SOUNDID;
		}

		if (m_spinUpSoundId != INVALID_SOUNDID)
		{
			m_pWeapon->StopSound(m_spinUpSoundId);
			m_spinUpSoundId = INVALID_SOUNDID;
		}
	}

	m_rotation_angle = 0.0f;
	m_speed = 0.0f;
	m_accelerating = false;
	m_decelerating = false;
	m_acceleration = 0.0f;

	m_soundId = INVALID_SOUNDID;
	m_spinUpSoundId = INVALID_SOUNDID;

	Firing(false);
	m_startedToFire = false;

	m_hasBarrelAttachment = !m_rapidparams.barrel_attachment.empty();
	m_hasEngineAttachment = !m_rapidparams.engine_attachment.empty();

	UpdateTpBarrelVisibility();

	const auto slot = m_pWeapon->GetStats().fp ? CItem::eIGS_FirstPerson : CItem::eIGS_ThirdPerson;
	UpdateRotation(slot, 0.0f);
}

//------------------------------------------------------------------------
void CRapid::Update(float frameTime, unsigned int frameId)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	CSingle::Update(frameTime, frameId);

	if (m_speed <= 0.0f && m_acceleration < 0.0001f)
	{
		FinishDeceleration();
		return;
	}

	m_pWeapon->RequireUpdate(eIUS_FireMode);

	m_speed = m_speed + m_acceleration * frameTime;

	if (m_speed > m_rapidparams.max_speed)
	{
		m_speed = m_rapidparams.max_speed;
		m_accelerating = false;
	}

	if ((m_speed >= m_rapidparams.min_speed) && (!m_decelerating))
	{
		float dt = 1.0f;
		if (cry_fabsf(m_speed) > 0.001f && cry_fabsf(m_rapidparams.max_speed) > 0.001f)
			dt = m_speed / m_rapidparams.max_speed;

		m_next_shot_dt = 60.0f / (m_fireparams.rate * dt);

		bool canShoot = CanFire(false);

		if (canShoot)
		{
			if (!OutOfAmmo())
			{
				if (m_netshooting)
					Firing(true);
				else
					Firing(Shoot(true, false));

				if (m_firing && !(m_rapidparams.camshake_rotate.IsZero() && m_rapidparams.camshake_shift.IsZero()))
				{
					CActor* act = m_pWeapon->GetOwnerActor();
					if (act && act->IsClient())
					{
						IView* pView = g_pGame->GetIGameFramework()->GetIViewSystem()->GetActiveView();
						if (pView)
						{
							pView->SetViewShake(
								Ang3(m_rapidparams.camshake_rotate),
								m_rapidparams.camshake_shift,
								m_next_shot_dt / m_rapidparams.camshake_perShot,
								m_next_shot_dt / m_rapidparams.camshake_perShot,
								0, 1);
						}
					}
				}
			}
			else
			{
				Firing(false);
				Accelerate(m_rapidparams.deceleration);
				if (m_pWeapon->GetOwnerActor() && m_pWeapon->GetOwnerActor()->IsPlayer())
				{
					SmokeEffect();
					m_pWeapon->Reload();
				}
			}
		}
	}
	else if (m_firing)
	{
		Firing(false);
		if (OutOfAmmo() && m_pWeapon->GetOwnerActor() && m_pWeapon->GetOwnerActor()->IsPlayer())
		{
			SmokeEffect();
			m_pWeapon->Reload();
		}
	}

	if ((m_speed < m_rapidparams.min_speed) && (m_acceleration < 0.0f) && (!m_decelerating))
		Accelerate(m_rapidparams.deceleration);

	const auto slot = m_pWeapon->GetStats().fp ? CItem::eIGS_FirstPerson : CItem::eIGS_ThirdPerson;
	UpdateRotation(slot, frameTime);

	UpdateSound(frameTime);
}

//------------------------------------------------------------------------
void CRapid::StartReload(int zoomed)
{
	if (IsFiring())
		Accelerate(m_rapidparams.deceleration);
	Firing(false);

	CSingle::StartReload(zoomed);
}

//------------------------------------------------------------------------
void CRapid::StartFire()
{
	if (m_pWeapon->IsBusy() || !CanFire(true))
	{
		//Clip empty sound
		if (!CanFire(true) && !m_reloading)
		{
			int ammoCount = m_pWeapon->GetAmmoCount(m_fireparams.ammo_type_class);

			if (m_fireparams.clip_size == 0)
				ammoCount = m_pWeapon->GetInventoryAmmoCount(m_fireparams.ammo_type_class);

			if (ammoCount <= 0)
			{
				m_pWeapon->PlayAction(m_actions.empty_clip);
				//Auto reload
				m_pWeapon->Reload();
			}
		}
		return;
	}
	else if (m_pWeapon->IsWeaponLowered())
	{
		m_pWeapon->PlayAction(m_actions.null_fire);
		return;
	}

	m_netshooting = false;

	m_pWeapon->EnableUpdate(true, eIUS_FireMode);

	//SpinUpEffect(true);
	Accelerate(m_rapidparams.acceleration);

	m_startedToFire = true;

	m_pWeapon->RequestStartFire();
}

//------------------------------------------------------------------------
void CRapid::StopFire()
{
	m_startedToFire = false;

	if (m_pWeapon->IsBusy() && !m_pWeapon->IsZooming())
		return;

	if (m_zoomtimeout > 0.0f)
	{
		CActor* pActor = m_pWeapon->GetOwnerActor();
		if (pActor && pActor->IsClient() && pActor->GetScreenEffects() != 0)
		{
			float speed = 1.0f / .1f;
			pActor->GetScreenEffects()->ClearBlendGroup(pActor->m_autoZoomInID);
			pActor->GetScreenEffects()->ClearBlendGroup(pActor->m_autoZoomOutID);
			CFOVEffect* fov = new CFOVEffect(pActor->GetEntityId(), 1.0f);
			CLinearBlend* blend = new CLinearBlend(1);
			pActor->GetScreenEffects()->StartBlend(fov, blend, speed, pActor->m_autoZoomOutID);
		}
		m_zoomtimeout = 0.0f;
	}

	if (m_acceleration >= 0.0f)
	{
		Accelerate(m_rapidparams.deceleration);

		if (m_pWeapon->IsDestroyed())
			FinishDeceleration();
	}

	SpinUpEffect(false);

	if (m_firing)
		SmokeEffect();

	m_pWeapon->RequestStopFire();
}

//------------------------------------------------------------------------
void CRapid::NetStartFire()
{
	m_netshooting = true;

	m_pWeapon->EnableUpdate(true, eIUS_FireMode);

	//SpinUpEffect(true);
	Accelerate(m_rapidparams.acceleration);
}

//------------------------------------------------------------------------
void CRapid::NetStopFire()
{
	if (m_acceleration >= 0.0f)
	{
		Accelerate(m_rapidparams.deceleration);

		if (m_pWeapon->IsDestroyed())
			FinishDeceleration();
	}

	SpinUpEffect(false);

	if (m_firing)
		SmokeEffect();
}

//------------------------------------------------------------------------
float CRapid::GetSpinUpTime() const
{
	return m_rapidparams.min_speed / m_rapidparams.acceleration;
}

//------------------------------------------------------------------------
float CRapid::GetSpinDownTime() const
{
	return m_rapidparams.max_speed / max(0.0001f, cry_fabsf(m_rapidparams.deceleration));
}

//------------------------------------------------------------------------
void CRapid::OnEnterFirstPerson()
{
	UpdateRotation(CItem::eIGS_FirstPerson, 0.0f);

	if (m_rapidparams.tp_barrel_enable && !m_rapidparams.tp_barrel_model.empty() && m_rapidparams.tp_barrel_hide_fp)
	{
		SetSlotRender(m_pWeapon->GetEntity(), m_rapidparams.tp_barrel_slot, false);
	}

	if (m_speed >= 0.00001f && m_soundId != INVALID_SOUNDID)
	{
		SetupEmitters(false);
		SetupEmitters(true);

		m_pWeapon->StopSound(m_soundId);

		//CryMP: We might not be first person untill next frame, so need to force it with flag
		m_soundId = m_pWeapon->PlayAction(
			m_rapidactions.rapid_fire, 0, true,
			CItem::eIPAF_ForceFirstPerson | (CItem::eIPAF_Default & (~CItem::eIPAF_Animation)));
	}
}

//------------------------------------------------------------------------
void CRapid::OnEnterThirdPerson()
{
	UpdateRotation(CItem::eIGS_ThirdPerson, 0.0f);

	if (m_rapidparams.tp_barrel_enable && !m_rapidparams.tp_barrel_model.empty())
	{
		SetSlotRender(m_pWeapon->GetEntity(), m_rapidparams.tp_barrel_slot, true);
	}

	if (m_speed >= 0.00001f && m_soundId != INVALID_SOUNDID)
	{
		SetupEmitters(false);
		SetupEmitters(true);

		m_pWeapon->StopSound(m_soundId);

		m_soundId = m_pWeapon->PlayAction(
			m_rapidactions.rapid_fire, 0, true,
			CItem::eIPAF_ForceThirdPerson | (CItem::eIPAF_Default & (~CItem::eIPAF_Animation)));
	}
}

//------------------------------------------------------------------------
void CRapid::Accelerate(float acc)
{
	m_acceleration = acc;

	if (acc > 0.0f)
	{
		if (!IsFiring())
			SpinUpEffect(true);

		m_accelerating = true;
		m_decelerating = false;
		m_spinUpSoundId = m_pWeapon->PlayAction(m_actions.spin_up, 0, false, CItem::eIPAF_Default | CItem::eIPAF_CleanBlending);
	}
	else
	{
		m_accelerating = false;
		m_decelerating = true;
		if (m_speed > 0.0f)
		{
			m_pWeapon->PlayAction(m_actions.spin_down, 0, false, CItem::eIPAF_Default | CItem::eIPAF_CleanBlending);
			if (m_firing)
				m_pWeapon->PlayAction(m_actions.spin_down_tail);
		}

		if (m_spinUpSoundId != INVALID_SOUNDID)
		{
			m_pWeapon->StopSound(m_spinUpSoundId);
			m_spinUpSoundId = INVALID_SOUNDID;
		}

		if (IsFiring())
			SpinUpEffect(false);
	}
}

//------------------------------------------------------------------------
void CRapid::FinishDeceleration()
{
	m_decelerating = false;
	m_speed = 0.0f;
	m_acceleration = 0.0f;

	if (m_soundId != INVALID_SOUNDID)
	{
		m_pWeapon->StopSound(m_soundId);
		m_soundId = INVALID_SOUNDID;
	}

	m_pWeapon->EnableUpdate(false, eIUS_FireMode);

	MuzzleFlashEffect(false);
}

//------------------------------------------------------------------------
void CRapid::Firing(bool firing)
{
	SpinUpEffect(false);

	if (m_firing != firing)
	{
		if (!m_firing)
			m_pWeapon->PlayAction(m_rapidactions.blast);
	}
	m_firing = firing;
}

//------------------------------------------------------------------------
void CRapid::UpdateRotation(CItem::eGeometrySlot slot, float frameTime)
{
	const bool wantsTpSlotSpin =
		(slot == CItem::eIGS_ThirdPerson) &&
		m_rapidparams.tp_barrel_enable &&
		!m_rapidparams.tp_barrel_model.empty();

	const bool wantsChrSpin = (m_hasBarrelAttachment || m_hasEngineAttachment);

	if (!wantsTpSlotSpin && !wantsChrSpin)
		return;

	const float spinMult = std::max(0.0f, m_rapidparams.barrel_spin_mult);
	m_rotation_angle -= (m_speed * spinMult) * frameTime * 2.0f * gf_PI;

	// --------------------------
	// Third-person: optional geometry slot spinner (CGF)
	// --------------------------
	if (wantsTpSlotSpin)
	{
		const int barrelSlot = m_rapidparams.tp_barrel_slot;

		if (!m_barrelBaseTMValid)
		{
			const int ok = m_pWeapon->GetEntity()->LoadGeometry(barrelSlot, m_rapidparams.tp_barrel_model.c_str());
			SetSlotRender(m_pWeapon->GetEntity(), barrelSlot, true);

			const Vec3 rDeg = m_rapidparams.tp_barrel_rotation;
			Ang3 aRad(DEG2RAD(rDeg.x), DEG2RAD(rDeg.y), DEG2RAD(rDeg.z));
			Matrix33 rot = Matrix33::CreateRotationXYZ(aRad);

			if (fabsf(m_rapidparams.tp_barrel_scale - 1.0f) > 0.0001f)
				rot *= m_rapidparams.tp_barrel_scale;

			m_barrelBaseTM = Matrix34(rot, m_rapidparams.tp_barrel_offset);
			m_barrelBaseTMValid = true;

			m_pWeapon->GetEntity()->SetSlotLocalTM(barrelSlot, m_barrelBaseTM);

			//CryLogAlways("$3Rapid TP barrel model loaded (%s) slot=%d ok=%d",
			//	m_rapidparams.tp_barrel_model.c_str(), barrelSlot, ok);
		}

		Matrix33 spin = Matrix33::CreateRotationY(m_rotation_angle);
		Matrix34 spinTM(spin, Vec3(ZERO));

		Matrix34 finalTM = m_barrelBaseTM * spinTM;
		m_pWeapon->GetEntity()->SetSlotLocalTM(barrelSlot, finalTM);
		return;
	}

	// --------------------------
	// CHR attachments rotation
	// --------------------------
	Ang3 angles(0, m_rotation_angle, 0);
	Matrix34 tm = Matrix33::CreateRotationXYZ(angles);

	if (m_hasBarrelAttachment)
		m_pWeapon->SetCharacterAttachmentLocalTM(slot, m_rapidparams.barrel_attachment.c_str(), tm);

	if (m_hasEngineAttachment)
		m_pWeapon->SetCharacterAttachmentLocalTM(slot, m_rapidparams.engine_attachment.c_str(), tm);
}

//------------------------------------------------------------------------
void CRapid::UpdateSound(float frameTime)
{
	if (m_speed >= 0.00001f)
	{
		if (m_soundId == INVALID_SOUNDID)
		{
			m_soundId = m_pWeapon->PlayAction(m_rapidactions.rapid_fire, 0, true, CItem::eIPAF_Default & (~CItem::eIPAF_Animation));
		}

		if (m_soundId != INVALID_SOUNDID)
		{
			float rpm_scale = m_speed / m_rapidparams.min_speed;
			float ammo = 0;

			if (!OutOfAmmo())
				ammo = 1.0f;

			ISound* pSound = m_pWeapon->GetISound(m_soundId);
			if (pSound)
			{
				if (g_pGameCVars->i_debug_sounds)
				{
					float color[] = { 1,1,1,0.5f };
					gEnv->pRenderer->Draw2dLabel(150, 500, 1.3f, color, false, "%s rpm_scale: %.2f", m_pWeapon->GetEntity()->GetName(), rpm_scale);
				}

				pSound->SetParam("rpm_scale", rpm_scale, false);
				pSound->SetParam("ammo", ammo, false);

				//Sound variations for FY71 (AI most common weapon)
				if (m_fireparams.sound_variation && !m_pWeapon->IsOwnerFP())
				{
					pSound->SetParam("variations", m_soundVariationParam, true);
				}
			}

			if (m_speed >= m_rapidparams.min_speed)
				m_spinUpSoundId = INVALID_SOUNDID;
		}
	}
	else if (m_soundId != INVALID_SOUNDID)
	{
		m_pWeapon->StopSound(m_soundId);
		m_soundId = INVALID_SOUNDID;
	}
}

//------------------------------------------------------------------------
void CRapid::SetSlotRender(IEntity* pEnt, int slot, bool render)
{
	if (!pEnt || slot < 0)
		return;

	int flags = pEnt->GetSlotFlags(slot);
	if (render) 
		flags |= ENTITY_SLOT_RENDER;
	else        
		flags &= ~ENTITY_SLOT_RENDER;

	pEnt->SetSlotFlags(slot, flags);
}

//------------------------------------------------------------------------
bool CRapid::ShouldShowTpBarrel() const
{
	const CWeapon::SStats& stats = m_pWeapon->GetStats();

	if (stats.dropped)
		return true;

	if (m_rapidparams.tp_barrel_hide_fp && stats.fp)
		return false;

	const EntityId ownerId = m_pWeapon->GetOwnerId();
	const bool hasOwner = (ownerId != 0);

	if (hasOwner && !m_pWeapon->IsSelected())
		return false;

	return true;
}

//------------------------------------------------------------------------
void CRapid::UpdateTpBarrelVisibility()
{
	if (!(m_rapidparams.tp_barrel_enable && !m_rapidparams.tp_barrel_model.empty()))
		return;

	const int slot = m_rapidparams.tp_barrel_slot;
	const bool show = ShouldShowTpBarrel();

	SetSlotRender(m_pWeapon->GetEntity(), slot, show);
}

//------------------------------------------------------------------------
bool CRapid::AllowZoom() const
{
	return !m_firing && !m_startedToFire;
}

const char* CRapid::GetType() const
{
	return "Rapid";
}

void CRapid::GetMemoryStatistics(ICrySizer* s)
{
	s->Add(*this);
	CSingle::GetMemoryStatistics(s);
	m_rapidparams.GetMemoryStatistics(s);
	m_rapidactions.GetMemoryStatistics(s);
}
