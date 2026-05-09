#include "CryCommon/CryNetwork/ISerialize.h"
#include "CryCommon/CrySystem/ISystem.h"

#include "ChangeRequest.h"
#include "IntersectionChecker.h"
#include "ParticleEntity.h"
#include "PhysicalWorld.h"
#include "RayGeom.h"
#include "RigidBody.h"
#include "WaterMan.h"

CParticleEntity::CParticleEntity(CPhysicalWorld* pWorld) : CPhysicalEntity(pWorld)
{
	m_gravity0 = m_gravity = pWorld ? pWorld->m_vars.gravity : Vec3(0, 0, -9.81f);
	m_mass = 0.2f;
	m_dim = m_dimLying = 0.05f;
	m_rdim = 20.0f;
	m_heading.Set(1, 0, 0);
	m_vel.zero();
	m_wspin.zero();
	m_waterGravity = m_gravity * 0.8f;
	m_kAirResistance = 0;
	m_kWaterResistance = 0.5f;
	m_accThrust = 0;
	m_kAccLift = 0;
	m_qspin.SetIdentity();
	m_surface_idx = 0;
	m_dirdown.Set(0, 0, -1);
	m_normal.Set(0, 0, 1);
	m_flags = 0;
	m_iSimClass = 4;
	m_bSliding = 0;
	m_slide_normal.Set(0, 0, 1);
	m_minBounceVel = 1.5f;
	m_pColliderToIgnore = 0;
	m_iPierceability = sf_max_pierceable;
	m_ig[0].x = m_ig[1].x = m_ig[0].y = m_ig[1].y = -3;
	m_timeSurplus = 0;

	m_defpart.flags = 0;
	m_defpart.id = 0;
	m_defpart.pos.zero();
	m_defpart.q.SetIdentity();
	m_defpart.scale = 1.0f;
	m_defpart.mass = 0;
	m_defpart.minContactDist = 0;
	m_depth = 0;
	m_velMedium.zero();
	m_bForceAwake = 2;
	m_timeForceAwake = 0;
	m_sleepTime = 0;
	m_bHadCollisions = 0;
	m_bRecentCollisions = 0;
	m_nStepCount = 0;
	m_areaCheckPeriod = 6;
	m_lockParticle = 0;
	m_timeStepPerformed = 0;
}

CParticleEntity::~CParticleEntity()
{
	if (m_nParts > 0 && m_pWorld)
	{
		m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[0].pPhysGeom);
	}
	if (m_pColliderToIgnore)
	{
		m_pColliderToIgnore->Release();
	}
	m_nParts = 0;
}

int CParticleEntity::SetParams(pe_params* _params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this, m_pWorld, _params, bThreadSafe);
	if (req.IsQueued())
	{
		return 1;
	}

	int res = CPhysicalEntity::SetParams(_params, 1);
	if (res)
	{
		if (_params->type == pe_params_flags::type_id)
		{
			pe_params_particle pp;
			SetParams(&pp, 1);
		}
		return res;
	}

	if (_params->type == pe_params_particle::type_id)
	{
		pe_params_particle* params = (pe_params_particle*)_params;
		WriteLock lock(m_lockParticle);
		if (!is_unused(params->mass))
		{
			m_mass = params->mass;
		}
		if (!is_unused(params->size))
		{
			m_rdim = 1.0f / (m_dim = params->size * 0.5f);
			m_dimLying = params->size * 0.5f;
		}
		if (!is_unused(params->thickness))
		{
			m_dimLying = params->thickness * 0.5f;
		}
		if (!is_unused(params->kAirResistance))
		{
			m_kAirResistance = params->kAirResistance;
		}
		if (!is_unused(params->kWaterResistance))
		{
			m_kWaterResistance = params->kWaterResistance;
		}
		if (!is_unused(params->accThrust))
		{
			m_accThrust = params->accThrust;
		}
		if (!is_unused(params->accLift) && !is_unused(params->velocity))
		{
			m_kAccLift = params->velocity != 0 ? params->accLift / fabs_tpl(params->velocity) : 0;
		}
		if (!is_unused(params->heading))
		{
			m_heading = params->heading;
		}
		if (!is_unused(params->velocity))
		{
			m_vel = m_heading * params->velocity;
		}
		if (!is_unused(params->wspin))
		{
			m_wspin = params->wspin;
		}
		if (!is_unused(params->gravity))
		{
			m_gravity0 = m_gravity = params->gravity;
			if (m_gravity.len2() > 0)
			{
				(m_dirdown = m_gravity).normalize();
			}
			else
			{
				m_dirdown.Set(0, 0, -1);
			}
		}
		if (!is_unused(params->waterGravity))
		{
			m_waterGravity = params->waterGravity;
		}
		if (!is_unused(params->normal))
		{
			m_normal = params->normal;
		}
		if (!is_unused(params->q0))
		{
			m_qspin = params->q0;
		}
		if (!is_unused(params->minBounceVel))
		{
			m_minBounceVel = params->minBounceVel;
		}
		if (!is_unused(params->surface_idx))
		{
			m_surface_idx = params->surface_idx;
		}
		if (!is_unused(params->flags))
		{
			m_flags = params->flags;
		}

		if (m_flags & particle_traceable)
		{
			if (m_ig[0].x == -3)
			{
				m_ig[0].x = m_ig[1].x = m_ig[0].y = m_ig[1].y = -2;
			}
			if (m_pos.len2() > 0)
			{
				AtomicAdd(&m_pWorld->m_lockGrid, -m_pWorld->RepositionEntity(this, 1));
			}
		}
		else
		{
			if (m_ig[0].x != -3)
			{
				m_pWorld->DetachEntityGridThunks(this);
				m_ig[0].x = m_ig[1].x = m_ig[0].y = m_ig[1].y = -3;
			}
		}

		if (!is_unused(params->pColliderToIgnore))
		{
			if (m_pColliderToIgnore)
			{
				m_pColliderToIgnore->Release();
			}
			m_pColliderToIgnore = (CPhysicalEntity*)params->pColliderToIgnore;
			if (m_pColliderToIgnore)
			{
				m_pColliderToIgnore->AddRef();
			}
		}
		if (!is_unused(params->iPierceability))
		{
			m_iPierceability = params->iPierceability;
		}
		if (!is_unused(params->areaCheckPeriod))
		{
			m_areaCheckPeriod = params->areaCheckPeriod;
			m_nStepCount = 0;
		}

		if (!(m_flags & particle_constant_orientation))
		{
			if (!(m_flags & particle_no_path_alignment))
			{
				Vec3 dirbuf[3];
				dirbuf[0] = m_dirdown ^ m_heading;
				dirbuf[1] = m_heading;
				//	if (dirbuf[0].len2()<0.01f) dirbuf[0] = m_heading.orthogonal();
				if (dirbuf[0].len2() < 0.01f)
				{
					dirbuf[0].SetOrthogonal(m_heading);
				}
				dirbuf[0].normalize();
				dirbuf[2] = dirbuf[0] ^ dirbuf[1];
				m_qrot = quaternionf(GetMtxFromBasisT(dirbuf)) * m_qspin;
			}
			else
			{
				m_qrot = m_qspin;
			}
		}

		{
			WriteLock lock1(m_lockUpdate);
			m_BBox[0] = m_pos - Vec3(m_dim, m_dim, m_dim);
			m_BBox[1] = m_pos + Vec3(m_dim, m_dim, m_dim);
		}
		return 1;
	}

	return 0;
}

int CParticleEntity::GetParams(pe_params* _params)
{
	if (CPhysicalEntity::GetParams(_params))
	{
		return 1;
	}

	if (_params->type == pe_params_particle::type_id)
	{
		pe_params_particle* params = (pe_params_particle*)_params;
		ReadLock lock(m_lockParticle);
		params->mass = m_mass;
		params->size = m_dim * 2.0f;
		params->thickness = m_dimLying * 2.0f;
		params->heading = m_heading;
		params->velocity = m_vel.len();
		params->wspin = m_wspin;
		params->gravity = m_gravity;
		params->normal = m_normal;
		params->kAirResistance = m_kAirResistance;
		params->accThrust = m_accThrust;
		params->accLift = params->velocity * m_kAccLift;
		params->q0 = m_qspin;
		params->surface_idx = m_surface_idx;
		params->flags = m_flags;
		params->pColliderToIgnore = m_pColliderToIgnore;
		params->iPierceability = m_iPierceability;
		return 1;
	}

	return 0;
}

int CParticleEntity::GetStateSnapshot(TSerialize ser, float time_back, int flags)
{
	if (ser.GetSerializationTarget() == eST_Network)
	{
		if (ser.BeginOptionalGroup("ifawake", IsAwake() != 0))
		{
			ser.Value("vel", m_vel, 'pPVl');
			ser.EndGroup();
		}
		ser.Value("pos", m_pos, 'wrld');
	}
	else
	{
		ser.BeginGroup("ParticleEntity");
		CPhysicalEntity::GetStateSnapshot(ser, time_back, flags | ssf_from_child_class);
		ser.Value("vel", m_vel);
		ser.EndGroup();
	}

	return 1;
}

int CParticleEntity::SetStateFromSnapshot(TSerialize ser, int flags)
{
	if (ser.GetSerializationTarget() == eST_Network)
	{
		if (ser.BeginOptionalGroup("ifawake", IsAwake() != 0))
		{
			ser.Value("vel", m_vel, 'pPVl');
			ser.EndGroup();
			m_heading = m_vel.normalized();
		}

		ser.Value("pos", m_pos, 'wrld');
	}
	else
	{
		ser.BeginGroup("ParticleEntity");
		CPhysicalEntity::GetStateSnapshot(ser, flags | ssf_from_child_class);
		ser.Value("vel", m_vel);
		ser.EndGroup();
	}

	return 1;
}

int CParticleEntity::GetStatus(pe_status* _status)
{
	if (CPhysicalEntity::GetStatus(_status))
	{
		return 1;
	}

	if (_status->type == pe_status_collisions::type_id)
	{
		return m_bHadCollisions ? m_bHadCollisions-- : 0;
	}

	if (_status->type == pe_status_dynamics::type_id)
	{
		pe_status_dynamics* status = (pe_status_dynamics*)_status;
		ReadLock lock(m_lockParticle);
		Vec3 gravity;
		float kAirResistance;
		if (m_depth < 0)
		{
			gravity = m_waterGravity;
			kAirResistance = m_kWaterResistance;
		}
		else
		{
			gravity = m_gravity;
			kAirResistance = m_kAirResistance;
		}
		status->v = m_vel;
		status->a = gravity + m_heading * m_accThrust - m_vel * kAirResistance +
		            (m_heading ^ (m_heading ^ m_dirdown)).normalize() * m_kAccLift * m_vel.len();
		status->w = m_wspin;
		status->centerOfMass = m_pos;
		status->mass = m_mass;
		return 1;
	}

	return 0;
}

int CParticleEntity::Action(pe_action* _action, int bThreadSafe)
{
	ChangeRequest<pe_action> req(this, m_pWorld, _action, bThreadSafe);
	if (req.IsQueued())
	{
		return 1;
	}

	if (_action->type == pe_action_impulse::type_id)
	{
		pe_action_impulse* action = (pe_action_impulse*)_action;
		Vec3 P = action->impulse;
		Vec3 L;
		if (!is_unused(action->angImpulse))
		{
			L = action->angImpulse;
		}
		else if (!is_unused(action->point))
		{
			L = action->point - m_pos ^ action->impulse;
		}
		if (m_mass > 0.0f)
		{
			m_vel += P / m_mass;
		}
		if (m_mass * m_dim > 0 && !(m_flags & particle_constant_orientation))
		{
			m_wspin += L / (0.4f * m_mass * m_dim);
		}
		return 1;
	}

	if (_action->type == pe_action_set_velocity::type_id)
	{
		pe_action_set_velocity* action = (pe_action_set_velocity*)_action;
		if (!is_unused(action->v))
		{
			m_heading = (m_vel = action->v).normalized();
		}
		if (!is_unused(action->w))
		{
			m_wspin = action->w;
		}
		return 1;
	}

	if (_action->type == pe_action_reset::type_id)
	{
		m_vel.zero();
		m_wspin.zero();
		return 1;
	}

	return CPhysicalEntity::Action(_action, 1);
}

int CParticleEntity::Awake(int bAwake, int iSource)
{
	if (bAwake)
	{
		m_bForceAwake = 1;
	}
	else
	{
		m_bForceAwake = 0;
		m_vel.zero();
	}
	return m_iSimClass;
}

int CParticleEntity::IsAwake(int ipart)
{
	ReadLock lock(m_lockParticle);
	if (!m_pWorld)
	{
		return false;
	}
	Vec3 gravity = m_depth < 0 ? m_waterGravity : m_gravity;
	return m_pos.z > -1000.0f && m_bForceAwake != 0 &&
	       (m_bForceAwake == 1 || (m_vel.len2() > sqr(m_pWorld->m_vars.minSeparationSpeed) ||
	                               !m_bRecentCollisions && !m_bSliding && gravity.len2() > 0));
}

void CParticleEntity::StartStep(float time_interval)
{
	m_timeStepPerformed = 0;
	m_timeStepFull = time_interval;
}

float CParticleEntity::GetMaxTimeStep(float time_interval)
{
	if (m_timeStepPerformed > m_timeStepFull - 0.001f)
	{
		return time_interval;
	}
	return min_safe(m_timeStepFull - m_timeStepPerformed, time_interval);
}

int CParticleEntity::DoStep(float time_interval, int iCaller)
{
	ray_hit hits[8];
	pe_action_impulse ai;
	// pe_action_register_coll_event arce;
	pe_status_dynamics sd;
	Vec3 pos, vel, heading, slide_normal, pos0, vel0, heading0, vtang, vel_next, gravity;
	float vn, vtang_len, rvtang_len, e, k, friction, kAirResistance;
	IPhysicalEntity* pIgnoredColliders[2] = {m_pColliderToIgnore, this};
	quaternionf qrot = m_qrot;
	int i, j, nhits, bHit, flags, bHasIgnore = (m_pColliderToIgnore != 0);
	EventPhysCollision event;
	event.penetration = event.radius = 0;

	if (m_depth < 0)
	{
		gravity = m_waterGravity * min(1.0f, -m_depth * m_rdim);
		kAirResistance = m_kWaterResistance;
	}
	else
	{
		gravity = m_gravity;
		kAirResistance = m_kAirResistance;
	}
	if (m_timeStepPerformed > m_timeStepFull - 0.0001f)
	{
		time_interval = 0.0001f;
	}
	m_timeStepPerformed += time_interval;

	if (m_pColliderToIgnore && m_pColliderToIgnore->m_iSimClass == 7)
	{
		m_pColliderToIgnore = 0;
	}

	if (IsAwake())
	{
		FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

		pos = pos0 = m_pos;
		vel = vel0 = m_vel;
		heading = m_heading;
		slide_normal = m_slide_normal;
		if (!m_bSliding)
		{
			vel0 += gravity * time_interval * 0.5f;
		}
		flags = m_flags;
		pos += vel0 * time_interval;
		m_bRecentCollisions = max(0, (int)m_bRecentCollisions - 1);

		if (m_bSliding && m_iPierceability >= 0)
		{
			if (m_pWorld->RayWorldIntersection(pos, slide_normal * (m_dim * -1.1f), ent_all,
			                                   m_iPierceability | geom_colltype_ray << rwi_colltype_bit,
			                                   hits, 1, pIgnoredColliders + 1 - bHasIgnore, 1 + bHasIgnore,
			                                   0, 0, "RayWorldIntersection(PhysParticles)", 0, iCaller))
			{
				slide_normal = hits[0].n;
				pos = hits[0].pt + slide_normal * m_dimLying;
				// if (m_dimLying!=m_dim)
				//	pos0 = m_pos;
				if (((CPhysicalEntity*)hits[0].pCollider)->m_iSimClass > 1)
				{
					sd.ipart = hits[0].ipart;
					hits[0].pCollider->GetStatus(&sd);
					vel0 -= (sd.v += (sd.w ^ pos - sd.centerOfMass));
				}
				else
				{
					sd.v.zero();
				}
				if (m_flags & particle_no_roll || slide_normal.z < 0.5f)
				{ // always slide if the slope is steep enough
					friction = m_pWorld->GetFriction(m_surface_idx, hits[0].surface_idx);
					if (slide_normal.z < 0.5f)
					{
						friction = min(1.0f, friction); // limit sliding friction on slopes
					}
					vn = hits[0].n * vel0;
					vtang = vel0 - hits[0].n * vn;
					vtang_len = vtang.len();
					rvtang_len = vtang_len > 1e-4 ? 1.0f / vtang_len : 0;
					vel = vel0 =
					    sd.v + hits[0].n * max(0.0f, vn) +
					    vtang * (max(0.0f, vtang_len - (max(0.0f, -(vn + ((m_gravity * hits[0].n) *
					                                                      time_interval))) *
					                                    friction)) *
					             rvtang_len);
					m_wspin.zero();
					m_qspin.SetIdentity();
					if (!(m_flags & particle_constant_orientation))
					{
						(qrot = Quat::CreateRotationV0V1(qrot * m_normal, hits[0].n) * qrot)
						    .Normalize();
					}
					// m_qrot.SetRotationV0V1(m_normal,hits[0].n); //m_qrot =
					// quaternionf(m_normal,hits[0].n);
					flags |= particle_constant_orientation;
				}
				else
				{
					friction = m_pWorld->m_FrictionTable[m_surface_idx & (NSURFACETYPES - 1)];
					vel0 = vel = sd.v + (vel0 - slide_normal * (vel0 * slide_normal)) *
					                        max(0.0f, 1.0f - (time_interval * friction));
					m_wspin = slide_normal ^ (vel - sd.v) * m_rdim;
				}
				if (m_flags & particle_single_contact)
				{
					gravity.zero();
				}
				else
				{
					gravity -= slide_normal * (slide_normal * gravity);
				}
				m_bForceAwake =
				    ((CPhysicalEntity*)hits[0].pCollider)->m_iSimClass <= 2 || m_timeForceAwake > 40.0f
					? 2
					: 1;
			}
			else
			{
				m_bSliding = 0;
				if (!(m_flags & particle_constant_orientation))
				{
					m_wspin = (heading ^ m_gravity).normalized() *
					          ((m_gravity * m_dirdown) * 0.5f * m_rdim);
				}
			}
		}

		vel += (gravity + heading * m_accThrust + (m_velMedium - vel) * kAirResistance +
		        (heading ^ (heading ^ m_dirdown)).normalize() * (m_kAccLift * vel.len())) *
		       time_interval;
		(heading = vel).normalize();

		if (!(flags & particle_constant_orientation))
		{
			if (!(m_flags & particle_no_spin))
			{
				if (m_wspin.len2() * sqr(time_interval) < 0.1f * 0.1f)
				{
					m_qspin.w -= (m_wspin * m_qspin.v) * time_interval * 0.5f;
					m_qspin.v +=
					    ((m_wspin ^ m_qspin.v) + m_wspin * m_qspin.w) * (time_interval * 0.5f);
					// m_qspin += quaternionf(0,m_wspin*0.5f)*m_qspin*time_interval;
				}
				else
				{
					float wlen = m_wspin.len();
					m_qspin =
					    Quat::CreateRotationAA(wlen * time_interval, m_wspin / wlen) * m_qspin;
				}
				m_qspin.Normalize();
			}
			else
			{
				m_wspin.zero();
			}
			if (!(m_flags & particle_no_path_alignment))
			{
				Vec3 dirbuf[3];
				dirbuf[0] = m_dirdown ^ m_heading;
				dirbuf[1] = m_heading;
				if (dirbuf[0].len2() < 0.01f)
				{
					dirbuf[0].SetOrthogonal(m_heading);
				}
				dirbuf[0].normalize();
				dirbuf[2] = dirbuf[0] ^ dirbuf[1];
				qrot = m_qspin * quaternionf(GetMtxFromBasisT(dirbuf));
			}
			else
			{
				qrot = m_qspin;
			}
		}

		bHit = nhits = 0;
		if (iCaller == 0 && m_pWorld->m_bWorldStep == 2 && m_iPierceability >= 0)
		{
			CPhysicalEntity** pentlist;
			pe_status_pos sp;
			sp.timeBack = 1; // time_interval;
			Vec3 posFixed;
			int nents = m_pWorld->GetEntitiesAround(pos0 - Vec3(m_dim, m_dim, m_dim),
			                                        pos0 + Vec3(m_dim, m_dim, m_dim), pentlist, ent_rigid);

			hits[0].dist = hits[1].dist = 1E10f;
			for (i = 0; i < nents; i++)
			{
				pentlist[i]->GetStatus(&sp);
				posFixed = (pentlist[i]->m_qrot * !sp.q) * (pos0 - sp.pos) + pentlist[i]->m_pos;
				bHit = (m_pWorld->RayTraceEntity(pentlist[i], posFixed,
				                                 pos0 - posFixed +
				                                     (pos0 - posFixed).normalized() * (m_dim * 0.97f),
				                                 hits + 1) &&
				        pentlist[i]->m_parts[hits[1].ipart].flags & geom_colltype_ray &&
				        hits[1].dist < hits[0].dist);
				if (bHit)
				{
					hits[0] = hits[1];
				}
			}
			if (bHit)
			{ // ignore collisions with moving bodies if they push us through statics
				heading0 = (hits[0].pt - pos0).normalized();
				bHit ^= m_pWorld->RayWorldIntersection(
				    pos0, hits[0].pt - pos0 + heading0 * m_dim, ent_terrain | ent_static,
				    m_iPierceability | geom_colltype_ray << rwi_colltype_bit | rwi_ignore_back_faces,
				    hits + 2, 1, pIgnoredColliders + 1 - bHasIgnore, 1 + bHasIgnore, 0, 0,
				    "RayWorldIntersection(PhysParticles)");
			}
			nhits = bHit;
			if (nhits)
			{
				pos0 = posFixed;
			}
		}

		if (!bHit && m_iPierceability >= 0)
		{
			heading0 = (pos - pos0).normalized();
			nhits = m_pWorld->RayWorldIntersection(
			    pos0, pos - pos0 + heading0 * m_dim, ent_all | ent_water,
			    m_iPierceability | (geom_colltype_ray | geom_colltype13) << rwi_colltype_bit |
				rwi_colltype_any | rwi_force_pierceable_noncoll | rwi_ignore_solid_back_faces,
			    hits, 8, pIgnoredColliders + 1 - bHasIgnore, 1 + bHasIgnore, 0, 0,
			    "RayWorldIntersection(PhysParticles)", 0, iCaller);
			bHit = isneg(-nhits) & (isneg(hits[0].dist + 0.5f) ^ 1);
		}

		event.pEntity[0] = this;
		event.pForeignData[0] = m_pForeignData;
		event.iForeignData[0] = m_iForeignData;
		if (iCaller == 0)
		{
			for (i = 0; i < nhits; i++)
			{
				j = (i + 1) & (i - (nhits - bHit)) >> 31;
				event.pt = hits[j].pt;
				event.n = hits[j].n;
				event.normImpulse = 0;
				if (hits[j].pCollider && ((CPhysicalPlaceholder*)hits[j].pCollider)->m_iSimClass != 5)
				{
					CPhysicalEntity* pCollider = (CPhysicalEntity*)hits[j].pCollider;
					event.pEntity[1] = hits[j].pCollider;
					event.pForeignData[1] = pCollider->m_pForeignData;
					event.iForeignData[1] = pCollider->m_iForeignData;
					RigidBody* pbody = pCollider->GetRigidBody(hits[j].ipart);
					event.vloc[1] = pbody->v + (pbody->w ^ event.pt - pbody->pos);
					event.mass[1] = pbody->M;
					event.idCollider = m_pWorld->GetPhysicalEntityId(hits[j].pCollider);

					if ((pCollider->m_iSimClass > 0 ||
					     pCollider->m_parts[hits[j].ipart].flags & geom_monitor_contacts) &&
					    pCollider->GetType() != PE_PARTICLE)
					{
						Vec3 vrel = vel0 - pbody->v - (pbody->w ^ hits[j].pt - pbody->pos);
						float M = max(pbody->M, pCollider->GetMass(hits[j].ipart));
						if (j > 0)
						{
							vrel *=
							    0.5f *
							    (1 -
							     (1.0f / 15) *
							         (int)((m_pWorld
							                    ->m_SurfaceFlagsTable[hits[j].surface_idx] &
							                sf_pierceable_mask) -
							               m_iPierceability));
						}
						if ((vrel * hits[j].n) * M < 0 && vel0.len2() > sqr(m_minBounceVel))
						{
							ai.point = hits[j].pt;
							ai.impulse = vrel * (m_mass * M / (m_mass + M));
							ai.ipart = hits[j].ipart;
							event.normImpulse = -(ai.impulse * event.n);
							pCollider->Action(&ai, 1);
						}
					}
				}
				else
				{
					event.pEntity[1] =
					    hits[j].pCollider ? hits[j].pCollider : &g_StaticPhysicalEntity;
					hits[j].pCollider = 0;
					event.pForeignData[1] = 0;
					event.iForeignData[1] = -1;
					event.vloc[1].zero();
					event.mass[1] = 0;
					event.idCollider = -1;
				}
				event.vloc[0] = vel0;
				event.mass[0] = m_mass;
				event.partid[0] = m_iPierceability;
				event.partid[1] = hits[j].partid;
				event.idmat[0] = m_surface_idx;
				event.idmat[1] = hits[j].surface_idx;
				m_pWorld->OnEvent(m_flags, &event);
				if (hits[j].surface_idx == m_pWorld->m_matWater)
				{
					if (m_pWorld->m_pWaterMan)
					{
						m_pWorld->m_pWaterMan->OnWaterHit(hits[j].pt, vel0);
					}
				}
				else
				{
					m_bHadCollisions = 1;
					m_bRecentCollisions +=
					    (3 - m_bRecentCollisions) * isneg(hits[j].n * gravity - 0.001f);
				}
			}
		}

		if (bHit && hits[0].pCollider && ((CPhysicalPlaceholder*)hits[0].pCollider)->m_iSimClass != 5)
		{
			e = max(min((m_pWorld->m_BouncinessTable[m_surface_idx & (NSURFACETYPES - 1)] +
			             m_pWorld->m_BouncinessTable[hits[0].surface_idx & (NSURFACETYPES - 1)]) *
			                0.5f,
			            1.0f),
			        0.0f);
			friction = m_pWorld->GetFriction(m_surface_idx, hits[0].surface_idx);
			float minv = ((CPhysicalEntity*)hits[0].pCollider)->GetMassInv();
			k = m_mass * minv / (1 + m_mass * minv);
			if (sqr(m_dim) < (hits[0].pt - pos0).len2())
			{
				pos = hits[0].pt - heading0 * m_dim;
			}
			else
			{
				pos = pos0;
			}
			if (((CPhysicalEntity*)hits[0].pCollider)->m_iSimClass > 1)
			{
				hits[0].pCollider->GetStatus(&sd);
				vel0 -= (sd.v += (sd.w ^ pos - sd.centerOfMass));
			}
			else
			{
				sd.v.zero();
			}

			vn = hits[0].n * vel0;
			vtang = vel0 - hits[0].n * vn;
			if (vn > -m_minBounceVel || m_dimLying < m_dim * 0.3f)
			{
				e = 0;
			}
			vel = vel0 - hits[0].n * (vn * (1 - k) * (1 + e));
			if (sqr_signed(vn * (k - 1) * friction) < vtang.len2())
			{
				vel += vtang.normalized() * min(0.0f, vn * (1 - k)) * friction;
			}
			else
			{
				vel -= vtang;
			}

			j = isneg(hits[0].n * gravity - 0.001f);
			if (vel.len2() < sqr(m_pWorld->m_vars.minSeparationSpeed) * j ||
			    m_flags & particle_single_contact)
			{
				vel.zero();
				m_wspin.zero();
				m_qspin.SetIdentity();
				if (m_dim != m_dimLying)
				{
					pos = hits[0].pt + hits[0].n * m_dimLying;
				}
				m_bSliding = 1;
				slide_normal = hits[0].n;
				if (!(flags & particle_constant_orientation))
				{
					qrot = Quat::CreateRotationV0V1(qrot * m_normal, hits[0].n) * qrot;
				}
			}
			else
			{
				vel_next =
				    vel + (gravity + heading * m_accThrust - vel * kAirResistance +
				           (heading ^ (heading ^ m_dirdown)).normalize() * (m_kAccLift * vel.len())) *
					      time_interval;
				if ((vel_next * hits[0].n) * j < m_pWorld->m_vars.minSeparationSpeed * j)
				{
					if (m_dim != m_dimLying)
					{
						pos = hits[0].pt + hits[0].n * m_dimLying;
					}
					m_bSliding = 1;
					slide_normal = hits[0].n;
					if (m_flags & particle_no_roll && !(m_flags & particle_constant_orientation))
					{
						qrot = Quat::CreateRotationV0V1(qrot * m_normal, hits[0].n) * qrot;
					}
					// m_qrot = quaternionf(m_normal,hits[0].n);
				}

				if (m_flags & particle_no_roll)
				{
					m_wspin.zero();
				}
				else
				{
					m_wspin = (hits[0].n ^ vtang) * m_rdim;
				}
				hits[0].n.z = 1.0f;
			}
			vel += sd.v;

			m_bForceAwake =
			    hits[0].n.z > 0.7f && (((CPhysicalEntity*)hits[0].pCollider)->m_iSimClass <= 2 ||
			                           m_timeForceAwake > 40.0f)
				? 2
				: 1;
		}
		i = m_bForceAwake & 1;
		m_timeForceAwake = (m_timeForceAwake * i) + (time_interval * i);
		m_sleepTime = 0;

		Vec3 BBox[2];
		int bGridLocked = 0;
		BBox[0] = pos - Vec3(m_dim, m_dim, m_dim);
		BBox[1] = pos + Vec3(m_dim, m_dim, m_dim);
		if (m_flags & particle_traceable)
		{
			bGridLocked = m_pWorld->RepositionEntity(this, 1, BBox);
		}
		{
			WriteLock lock(m_lockUpdate);
			m_pos = pos;
			m_qrot = qrot;
			m_BBox[0] = BBox[0];
			m_BBox[1] = BBox[1];
			AtomicAdd(&m_pWorld->m_lockGrid, -bGridLocked);
		}
		{
			WriteLock lock(m_lockParticle);
			m_vel = vel;
			m_slide_normal = slide_normal;
			m_heading = heading;
		}

		if ((m_nStepCount | (int)(char)m_areaCheckPeriod >> 31) == 0)
		{
			pe_params_buoyancy pb[4];
			Vec3 vmedium[2];
			float depth;

			nhits = m_pWorld->CheckAreas(this, gravity, pb, 4, Vec3(), iCaller);
			if (nhits)
			{
				if (!is_unused(gravity))
				{
					m_waterGravity = m_gravity =
					    (gravity - m_pWorld->m_vars.gravity).len2() < gravity.len2() * sqr(0.01)
						? m_gravity0
						: gravity;
				}
				for (i = 0, m_depth = depth = 0; i < nhits; i++)
				{
					vmedium[pb[i].iMedium] += pb[i].waterFlow;
					if (pb[i].iMedium == 0 &&
					    (depth = (m_pos - pb[i].waterPlane.origin) * pb[i].waterPlane.n) < 0)
					{
						m_depth = max(m_depth, depth);
					}
				}
				m_velMedium = vmedium[isnonneg(m_depth)];
			}
			m_nStepCount = m_areaCheckPeriod;
		}
		m_nStepCount -= 1 + ((int)(char)m_nStepCount >> 31);

		if (iCaller == 0)
		{
			EventPhysPostStep event;
			event.pEntity = this;
			event.pForeignData = m_pForeignData;
			event.iForeignData = m_iForeignData;
			event.dt = time_interval;
			event.pos = m_pos;
			event.q = m_qrot;
			event.idStep = m_pWorld->m_idStep;
			m_pWorld->OnEvent(m_flags, &event);
		}
	}
	else
	{
		m_sleepTime += time_interval;
	}

	return 1;
}

int CParticleEntity::RayTrace(CRayGeom* pRay, geom_contact*& pcontacts, volatile int*& plock)
{
	static geom_contact g_ParticleContact;
	static volatile int g_lock = 0;
	prim_inters inters;
	primitives::box abox;
	abox.Basis.SetRow(2, m_qrot * m_normal);
	abox.Basis.SetRow(0, m_qrot * m_normal.GetOrthogonal().normalized());
	abox.Basis.SetRow(1, abox.Basis.GetRow(2) ^ abox.Basis.GetRow(0));
	abox.size(m_dim, m_dim, m_dimLying);
	abox.center = m_pos;

	if (box_ray_intersection(&abox, &pRay->m_ray, &inters))
	{
		WriteLockCond lock(g_lock, 1);
		lock.SetActive(0);
		plock = &g_lock;
		pcontacts = &g_ParticleContact;
		pcontacts->pt = inters.pt[0];
		pcontacts->t = (inters.pt[0] - pRay->m_ray.origin) * pRay->m_dirn;
		pcontacts->id[0] = m_surface_idx;
		pcontacts->iNode[0] = 0;
		pcontacts->n = inters.n;
		return 1;
	}
	return 0;
}

void CParticleEntity::DrawHelperInformation(IPhysRenderer* pRenderer, int flags)
{
	CPhysicalEntity::DrawHelperInformation(pRenderer, flags);

	if (flags & pe_helper_geometry)
	{
		primitives::box abox;
		abox.Basis.SetRow(2, m_qrot * m_normal);
		abox.Basis.SetRow(0, m_qrot * m_normal.GetOrthogonal());
		abox.Basis.SetRow(1, abox.Basis.GetRow(2) ^ abox.Basis.GetRow(0));
		abox.center = m_pos;
		int i, j;
		Vec3 pt[8];
		for (i = 0; i < 8; i++)
		{
			pt[i] =
			    Vec3(m_dim * ((i << 1 & 2) - 1), m_dim * ((i & 2) - 1), m_dimLying * ((i >> 1 & 2) - 1)) *
				abox.Basis +
			    abox.center;
		}
		for (i = 0; i < 8; i++)
		{
			for (j = 0; j < 3; j++)
			{
				if (i & 1 << j)
				{
					pRenderer->DrawLine(pt[i], pt[i ^ 1 << j], m_iSimClass);
				}
			}
		}
	}
}
