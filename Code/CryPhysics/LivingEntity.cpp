#include "CryCommon/CryNetwork/ISerialize.h"
#include "CryCommon/CrySystem/ISystem.h"

#include "CapsuleGeom.h"
#include "ChangeRequest.h"
#include "ContactSolver.h"
#include "LivingEntity.h"
#include "PhysicalWorld.h"
#include "RayGeom.h"
#include "RigidBody.h"
#include "RigidEntity.h" // ref_small_and_fast
#include "RopeEntity.h"
#include "Utils.h"
#include "WaterMan.h"

namespace {

constexpr int SZ_ACTIONS = 128;
constexpr int SZ_HISTORY = 128;

struct SLivingEntityNetSerialize
{
	Vec3 pos;
	Vec3 vel;
	Vec3 velRequested;

	void Serialize(TSerialize ser)
	{
		ser.Value("pos", pos, 'wrld');
		ser.Value("vel", vel, 'pLVl');
		ser.Value("velRequested", velRequested, 'pLVl');
	}
};

Vec3_tpl<short> EncodeVec6b(const Vec3& vec)
{
	return Vec3_tpl<short>(physics_float2int(min_safe(100.0f, max_safe(-100.0f, vec.x)) * (32767 / 100.0f)),
	                       physics_float2int(min_safe(100.0f, max_safe(-100.0f, vec.y)) * (32767 / 100.0f)),
	                       physics_float2int(min_safe(100.0f, max_safe(-100.0f, vec.z)) * (32767 / 100.0f)));
}

Vec3 DecodeVec6b(const Vec3_tpl<short>& vec)
{
	return Vec3(vec.x, vec.y, vec.z) * (100.0f / 32767);
}

struct vec4b
{
	short yaw;
	char pitch;
	unsigned char len;
};

vec4b EncodeVec4b(const Vec3& vec)
{
	vec4b res = {0, 0, 0};
	if (vec.len2() > sqr(1.0f / 512))
	{
		float len = vec.len();
		res.yaw = physics_float2int(atan2_tpl(vec.y, vec.x) * (32767 / g_PI));
		res.pitch = physics_float2int(asin_tpl(vec.z / len) * (127 / g_PI));
		res.len = physics_float2int(min(20.0f, len) * (255 / 20.0f));
	}
	return res;
}

Vec3 DecodeVec4b(const vec4b& vec)
{
	float yaw = vec.yaw * (g_PI / 32767), pitch = vec.pitch * (g_PI / 127), len = vec.len * (20.0f / 255);
	float t = cos_tpl(pitch);
	return Vec3(t * cos_tpl(yaw), t * sin_tpl(yaw), sin_tpl(pitch)) * len;
}

} // unnamed namespace

CLivingEntity::CLivingEntity(CPhysicalWorld* pWorld) : CPhysicalEntity(pWorld)
{
	m_vel.zero();
	m_velRequested.zero();
	if (pWorld)
	{
		m_gravity = pWorld->m_vars.gravity;
	}
	else
	{
		m_gravity.Set(0, 0, -9.81f);
	}
	m_bFlying = 1;
	m_bJumpRequested = 0;
	m_iSimClass = 3;
	m_dh = m_dhSpeed = m_dhAcc = 0;
	m_stablehTime = 1;
	m_hLatest = -1e-10f;
	m_timeFlying = m_minFlyTime = 0;
	m_nslope.Set(0, 0, 1);
	m_mass = 80;
	m_massinv = 2.0f / 80;
	m_surface_idx = 0;
	m_slopeSlide = cos_tpl(g_PI * 0.2f);
	m_slopeJump = m_slopeClimb = cos_tpl(g_PI * 0.3f);
	m_slopeFall = cos_tpl(g_PI * 0.39f);
	m_nodSpeed = 60.0f;

	m_size.Set(0.4f, 0.4f, 0.6f);
	m_hCyl = 1.1f;
	m_hEye = 1.7f;
	m_hPivot = 0;

	primitives::cylinder cyl;
	cyl.axis.Set(0, 0, 1);
	cyl.center.zero();
	cyl.hh = m_size.z;
	cyl.r = m_size.x;
	m_bUseCapsule = 0;
	if (m_bUseCapsule)
	{
		((CCapsuleGeom*)(m_pCylinderGeom = new CCapsuleGeom))->CreateCapsule((primitives::capsule*)&cyl);
	}
	else
	{
		(m_pCylinderGeom = new CCylinderGeom)->CreateCylinder(&cyl);
	}
	m_pCylinderGeom->CalcPhysicalProperties(&m_CylinderGeomPhys);
	m_CylinderGeomPhys.pMatMapping = 0;
	m_CylinderGeomPhys.nMats = 0;
	m_CylinderGeomPhys.nRefCount = -1;
	primitives::sphere sph;
	sph.center.Set(0, 0, -m_size.z);
	sph.r = m_size.x;
	m_SphereGeom.CreateSphere(&sph);
	sph.center.zero();
	sph.r = 0; // 0.2f;
	m_HeadGeom.CreateSphere(&sph);
	m_hHead = 1.9f;
	m_parts[0].pPhysGeom = m_parts[0].pPhysGeomProxy = &m_CylinderGeomPhys;
	m_parts[0].id = 100;
	m_parts[0].pos.Set(0, 0, m_hCyl - m_hPivot);
	m_parts[0].q.SetIdentity();
	m_parts[0].scale = 1.0f;
	m_parts[0].mass = m_mass;
	m_parts[0].surface_idx = m_surface_idx;
	m_parts[0].flags = geom_collides | geom_monitor_contacts;
	m_parts[0].flagsCollider = geom_colltype_player;
	m_parts[0].minContactDist = m_size.z * 0.1;
	m_parts[0].pNewCoords = (coord_block_BBox*)&m_parts[0].pos;
	m_parts[0].idmatBreakable = -1;
	m_parts[0].pLattice = 0;
	m_parts[0].pMatMapping = 0;
	m_parts[0].nMats = 0;
	m_nParts = 1;

	m_kInertia = 8;
	m_kInertiaAccel = 0;
	m_kAirControl = 0.1f;
	m_kAirResistance = 0;
	m_bSwimming = 0;
	m_nSensors = 0;
	m_pSensors = 0;
	m_pSensorsPoints = 0;
	m_pSensorsSlopes = 0;
	m_iSensorsActive = 0;
	m_pLastGroundCollider = 0;

	m_iHist = 0;
	int i;
	m_history = m_history_buf;
	m_szHistory = sizeof(m_history_buf) / sizeof(m_history_buf[0]);
	m_actions = m_actions_buf;
	m_szActions = sizeof(m_actions_buf) / sizeof(m_actions_buf[0]);
	for (i = 0; i < m_szHistory; i++)
	{
		m_history[i].dt = 1E10;
		m_history[i].pos.zero();
		m_history[i].v.zero();
		m_history[i].bFlying = 0;
		m_history[i].q.SetIdentity();
		m_history[i].timeFlying = 0;
		m_history[i].idCollider = -2;
		m_history[i].posColl.zero();
	}
	for (i = 0; i < m_szActions; i++)
	{
		m_actions[i].dt = 1E10;
		MARK_UNUSED m_actions[i].dir;
		m_actions[i].iJump = 0;
	}
	m_iAction = 0;
	m_bIgnoreCommands = 0;
	m_bStateReading = 0;
	m_flags = pef_pushable_by_players | pef_traceable | lef_push_players | lef_push_objects | pef_never_break;
	m_velGround.zero();
	m_timeUseLowCap = -1.0f;
	m_bActive = 1;
	m_timeSinceStanceChange = 0;
	m_bActiveEnvironment = 0;
	m_dhHist[0] = m_dhHist[1] = 0;
	m_timeOnStairs = 0;
	m_timeForceInertia = 0;
	m_deltaPos.zero();
	m_timeSmooth = 0;
	m_bUseSphere = 0;
	m_timeSinceImpulseContact = 10.0f;
	m_maxVelGround = 10.0f;
	m_bStuck = 0;
	m_nContacts = m_nContactsAlloc = 0;
	m_pContacts = 0;
	m_iSnapshot = 0;
	m_iTimeLastSend = -1;
	m_timeImpulseRecover = 5.0f;
	m_bHadCollisions = 0;
	m_lockLiving = 0;
	m_lastGroundSurfaceIdx = m_lastGroundSurfaceIdxAux = -1;
	m_pBody = 0;
	m_iCurTime = m_iRequestedTime = 0;
	m_lockStep = 0;
	m_collTypes = ent_terrain | ent_static | ent_sleeping_rigid | ent_rigid | ent_living;
	m_bSquashed = 0;

	m_forceFly = false;
}

CLivingEntity::~CLivingEntity()
{
	m_parts[0].pPhysGeom = m_parts[0].pPhysGeomProxy = 0;
	if (m_pSensors)
	{
		delete[] m_pSensors;
	}
	if (m_pSensorsPoints)
	{
		delete[] m_pSensorsPoints;
	}
	if (m_pSensorsSlopes)
	{
		delete[] m_pSensorsSlopes;
	}
	if (m_history != m_history_buf)
	{
		delete[] m_history;
	}
	if (m_actions != m_actions_buf)
	{
		delete[] m_actions;
	}
	if (m_pContacts)
	{
		delete[] m_pContacts;
	}
	if (m_pCylinderGeom)
	{
		delete m_pCylinderGeom;
	}
	if (m_pBody)
	{
		delete[] m_pBody;
	}
}

void CLivingEntity::ReleaseGroundCollider()
{
	if (m_pLastGroundCollider)
	{
		m_pLastGroundCollider->Release();
		m_pLastGroundCollider = 0;
	}
}

void CLivingEntity::SetGroundCollider(CPhysicalEntity* pCollider, int bAcceptStatic)
{
	ReleaseGroundCollider();
	if (pCollider && pCollider->m_iSimClass + bAcceptStatic > 0)
	{
		pCollider->AddRef();
		m_pLastGroundCollider = pCollider;
	}
}

int CLivingEntity::Awake(int bAwake, int iSource)
{
	if (m_pLastGroundCollider && m_pLastGroundCollider->m_iSimClass == 7)
	{
		ReleaseGroundCollider();
	}
	m_bActiveEnvironment = bAwake;
	return 1;
}

float CLivingEntity::UnprojectionNeeded(const Vec3& pos, const quaternionf& qrot, float hCollider, float hPivot,
                                        const Vec3& newdim, int bCapsule, Vec3& dirUnproj, int iCaller)
{
	int i, j, nents;
	float t;
	geom_world_data gwd[2];
	intersection_params ip;
	CPhysicalEntity** pentlist;
	geom_contact* pcontacts;

	primitives::cylinder newcyl;
	CCylinderGeom geomCyl;
	CCapsuleGeom geomCaps;
	CCylinderGeom* pgeom = bCapsule ? &geomCaps : &geomCyl;
	newcyl.r = newdim.x;
	newcyl.hh = newdim.z;
	newcyl.center.zero();
	newcyl.axis.Set(0, 0, 1);
	geomCyl.CreateCylinder(&newcyl);
	geomCaps.CreateCapsule((primitives::capsule*)&newcyl);
	Vec3 BBox[2];
	BBox[0].Set(-newdim.x, -newdim.x, -hPivot) += pos;
	BBox[1].Set(newdim.x, newdim.x, newdim.z + hCollider - hPivot) += pos;

	nents = m_pWorld->GetEntitiesAround(BBox[0], BBox[1], pentlist, m_collTypes | ent_no_ondemand_activation, 0, 0,
	                                    iCaller);
	gwd[0].R = Matrix33(qrot);
	gwd[0].offset = pos + gwd[0].R * Vec3(0, 0, hCollider - hPivot);
	gwd[0].v = -dirUnproj;
	ip.time_interval = m_size.z * 2;
	for (i = 0; i < nents; i++)
	{
		if (pentlist[i] != this && (!m_pForeignData || pentlist[i]->m_pForeignData != m_pForeignData) &&
		    !pentlist[i]->IgnoreCollisionsWith(this, 1))
		{
			if (pentlist[i]->GetType() != PE_LIVING)
			{
				ReadLock lock(pentlist[i]->m_lockUpdate);
				for (j = 0; j < pentlist[i]->m_nParts; j++)
				{
					if (pentlist[i]->m_parts[j].flags & geom_colltype_player)
					{
						gwd[1].R = Matrix33(pentlist[i]->m_qrot * pentlist[i]->m_parts[j].q);
						gwd[1].offset = pentlist[i]->m_pos +
						                pentlist[i]->m_qrot * pentlist[i]->m_parts[j].pos;
						gwd[1].scale = pentlist[i]->m_parts[j].scale;
						if (pgeom->Intersect(pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom, gwd,
						                     gwd + 1, &ip, pcontacts))
						{
							WriteLockCond lockColl(*ip.plock, 0);
							lockColl.SetActive();
							if (dirUnproj.len2() == 0)
							{
								dirUnproj = pcontacts[0].dir;
							}
							t = pcontacts[0].t; // lock should be released after reading t
							return t;
						}
					}
				}
			}
			else
			{
				CLivingEntity* pent = (CLivingEntity*)pentlist[i];
				if (fabs_tpl((pos.z + hCollider - hPivot) -
				             (pent->m_pos.z + pent->m_hCyl - pent->m_hPivot)) <
				        newdim.z + pent->m_size.z &&
				    len2(vector2df(pos) - vector2df(pent->m_pos)) < sqr(newdim.x + pent->m_size.x))
				{
					return 1E10;
				}
			}
		}
	}
	return 0;
}

int CLivingEntity::SetParams(pe_params* _params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this, m_pWorld, _params, bThreadSafe);
	if (req.IsQueued())
	{
		if (_params->type == pe_player_dimensions::type_id)
		{
			int iter = 0, iCaller = GetCaller();
			WriteLockCond lockCaller(m_pWorld->m_lockCaller[iCaller], iCaller);
			float hCyl, hPivot;
			Vec3 size, pos;
			quaternionf qrot;
			{
				ReadLock lock(m_lockUpdate);
				hCyl = m_hCyl;
				size = m_size;
				hPivot = m_hPivot;
				pos = m_pos;
				qrot = m_qrot;
			}
			pe_player_dimensions* params = (pe_player_dimensions*)_params;
			Vec3 sizeCollider = is_unused(params->sizeCollider) ? size : params->sizeCollider;
			float heightCollider = is_unused(params->heightCollider) ? hCyl : params->heightCollider;
			float unproj = 0, step;
			if ((heightCollider != hCyl || sizeCollider.x != size.x || sizeCollider.z != size.z) &&
			    m_pWorld && m_bActive)
			{
				do
				{
					unproj += (step = UnprojectionNeeded(
						       pos + params->dirUnproj * unproj, qrot, heightCollider, hPivot,
						       sizeCollider, m_bUseCapsule, params->dirUnproj, iCaller)) *
					          1.01f;
				}
				while (unproj < params->maxUnproj && ++iter < 8 && step > 0);
				if (unproj > params->maxUnproj || iter == 8)
				{
					return 0;
				}
			}
		}
		return 1;
	}

	int res;
	Vec3 prevpos = m_pos;
	quaternionf prevq = m_qrot;

	res = CPhysicalEntity::SetParams(_params, 1);
	if (res)
	{
		if (_params->type == pe_params_pos::type_id && !(((pe_params_pos*)_params)->bRecalcBounds & 32))
		{
			if ((prevpos - m_pos).len2() > sqr(m_size.z * 0.01))
			{
				WriteLock lock(m_lockLiving);
				ReleaseGroundCollider();
				m_bFlying = 1;
				m_vel.zero();
				m_dh = 0;
				for (int i = 0; i < m_szHistory; i++)
				{
					m_history[i].pos = m_pos;
				}
			}
			for (int i = 0; i < 3; i++)
			{
				if (fabs_tpl(m_qrot.v[i]) < 1e-10f)
				{
					m_qrot.v[i] = 0;
				}
			}
			if (fabs_tpl(m_qrot.w) < 1e-10f)
			{
				m_qrot.w = 0;
			}
			m_qrot.Normalize();
			// if ((m_qrot.v^prevq.v).len2() > m_qrot.v.len2()*prevq.v.len2()*sqr(0.001f)) {
			if (fabs_tpl(m_qrot.v.x) + fabs_tpl(m_qrot.v.y) + fabs_tpl(prevq.v.x) + fabs_tpl(prevq.v.y) > 0)
			{
				Vec3 dirUnproj(0, 0, 1);
				if (((pe_params_pos*)_params)->bRecalcBounds && m_bActive &&
				    UnprojectionNeeded(m_pos, m_qrot, m_hCyl, m_hPivot, m_size, m_bUseCapsule,
				                       dirUnproj) > 0)
				{
					pe_params_pos pp;
					pp.q = prevq;
					CPhysicalEntity::SetParams(&pp, 1);
					return 0;
				}
			}
		}
		return res;
	}

	if (_params->type == pe_player_dimensions::type_id)
	{
		pe_player_dimensions* params = (pe_player_dimensions*)_params;
		if (is_unused(params->sizeCollider))
		{
			params->sizeCollider = m_size;
		}
		if (is_unused(params->heightCollider))
		{
			params->heightCollider = m_hCyl;
		}
		if (is_unused(params->heightPivot))
		{
			params->heightPivot = m_hPivot;
		}
		if (is_unused(params->bUseCapsule))
		{
			params->bUseCapsule = m_bUseCapsule;
		}

		float unproj = 0, step;
		res = 0;
		if ((params->heightCollider != m_hCyl || params->heightPivot != m_hPivot ||
		     params->sizeCollider.x != m_size.x || params->sizeCollider.z != m_size.z ||
		     params->bUseCapsule != static_cast<int>(m_bUseCapsule)) &&
		    m_pWorld && m_bActive)
		{
			do
			{
				unproj += (step = UnprojectionNeeded(m_pos + params->dirUnproj * unproj, m_qrot,
				                                     params->heightCollider, params->heightPivot,
				                                     params->sizeCollider, params->bUseCapsule,
				                                     params->dirUnproj)) *
				          1.01f;
			}
			while (unproj < params->maxUnproj && ++res < 8 && step > 0);
			if (unproj > params->maxUnproj || res == 8)
			{
				return 0;
			}
		}

		{
			WriteLock lock(m_lockUpdate);
			m_pos += params->dirUnproj * unproj;

			if (params->heightCollider != m_hCyl || params->heightPivot != m_hPivot ||
			    params->sizeCollider.x != m_size.x || params->sizeCollider.z != m_size.z ||
			    params->bUseCapsule != static_cast<int>(m_bUseCapsule))
			{
				m_hPivot = params->heightPivot;
				primitives::cylinder newdim;
				newdim.r = params->sizeCollider.x;
				newdim.hh = params->sizeCollider.z;
				newdim.center.zero();
				newdim.axis.Set(0, 0, 1);
				if (static_cast<int>(m_bUseCapsule) != params->bUseCapsule)
				{
					if (m_pCylinderGeom)
					{
						delete m_pCylinderGeom;
					}
					m_CylinderGeomPhys.pGeom = m_pCylinderGeom =
					    params->bUseCapsule ? (new CCapsuleGeom) : (new CCylinderGeom);
				}
				m_bUseCapsule = params->bUseCapsule;
				if (m_bUseCapsule)
				{
					((CCapsuleGeom*)m_pCylinderGeom)->CreateCapsule((primitives::capsule*)&newdim);
				}
				else
				{
					m_pCylinderGeom->CreateCylinder(&newdim);
				}
				m_hCyl = params->heightCollider;
				m_parts[0].pos.Set(0, 0, m_hCyl - m_hPivot);
				m_size.Set(params->sizeCollider.x, params->sizeCollider.x, params->sizeCollider.z);
				primitives::sphere sph;
				sph.center.Set(0, 0, -m_size.z);
				sph.r = m_size.x;
				m_SphereGeom.CreateSphere(&sph);
				m_SphereGeom.m_Tree.m_Box.center = sph.center;
				m_SphereGeom.m_Tree.m_Box.size = Vec3(sph.r, sph.r, sph.r);
			}

			if (!is_unused(params->heightEye))
			{
				m_hEye = params->heightEye;
				m_timeSinceStanceChange = 0;
			}

			if (!is_unused(params->heightHead))
			{
				m_hHead = params->heightHead;
			}
			if (!is_unused(params->headRadius))
			{
				primitives::sphere sph;
				sph.center.zero();
				sph.r = params->headRadius;
				m_HeadGeom.CreateSphere(&sph);
			}
			ComputeBBox(m_BBox);
		}
		AtomicAdd(&m_pWorld->m_lockGrid, -m_pWorld->RepositionEntity(this, 1));

		return 1;
	}

	if (_params->type == pe_player_dynamics::type_id)
	{
		pe_player_dynamics* params = (pe_player_dynamics*)_params;
		WriteLock lock(m_lockLiving);
		if (!is_unused(params->kInertia))
		{
			m_kInertia = params->kInertia;
		}
		if (!is_unused(params->kInertiaAccel))
		{
			m_kInertiaAccel = params->kInertiaAccel;
		}
		if (!is_unused(params->kAirControl))
		{
			m_kAirControl = params->kAirControl;
		}
		if (!is_unused(params->kAirResistance))
		{
			m_kAirResistance = params->kAirResistance;
		}
		if (!is_unused(params->gravity))
		{
			m_gravity = params->gravity;
		}
		else if (!is_unused(params->gravity.z))
		{
			m_gravity.Set(0, 0, -params->gravity.z);
		}
		if (!is_unused(params->nodSpeed))
		{
			m_nodSpeed = params->nodSpeed;
		}
		if (!is_unused(params->bSwimming))
		{
			m_bSwimming = params->bSwimming;
		}
		if (!is_unused(params->mass))
		{
			m_mass = params->mass;
			m_massinv = m_mass > 0 ? 1.0f / m_mass : 0.0f;
		}
		if (!is_unused(params->surface_idx))
		{
			m_surface_idx = m_parts[0].surface_idx = params->surface_idx;
		}
		if (!is_unused(params->minSlideAngle))
		{
			m_slopeSlide = cos_tpl(params->minSlideAngle * (g_PI / 180.0f));
		}
		if (!is_unused(params->maxClimbAngle))
		{
			m_slopeClimb = cos_tpl(params->maxClimbAngle * (g_PI / 180.0f));
		}
		if (!is_unused(params->maxJumpAngle))
		{
			m_slopeJump = cos_tpl(params->maxJumpAngle * (g_PI / 180.0f));
		}
		if (!is_unused(params->minFallAngle))
		{
			m_slopeFall = cos_tpl(params->minFallAngle * (g_PI / 180.0f));
		}
		if (!is_unused(params->maxVelGround))
		{
			m_maxVelGround = params->maxVelGround;
		}
		if (!is_unused(params->timeImpulseRecover))
		{
			m_timeImpulseRecover = params->timeImpulseRecover;
		}
		if (!is_unused(params->collTypes))
		{
			m_collTypes = params->collTypes;
		}
		if (!is_unused(params->bActive))
		{
			if (m_bActive + (params->bActive * 2) == 2)
			{
				m_bFlying = 1;
			}
			m_bActive = params->bActive;
		}
		if (!is_unused(params->bNetwork) && params->bNetwork)
		{
			AllocateExtendedHistory();
		}
		if (!is_unused(params->iRequestedTime))
		{
			m_iRequestedTime = params->iRequestedTime;
		}
		return 1;
	}

	if (_params->type == pe_params_sensors::type_id)
	{
		pe_params_sensors* params = (pe_params_sensors*)_params;
		WriteLock lock(m_lockLiving);
		if (m_nSensors != params->nSensors)
		{
			m_bActiveEnvironment = 1;
		}
		if (m_pSensors && m_nSensors != params->nSensors)
		{
			delete[] m_pSensors;
			m_pSensors = 0;
			delete[] m_pSensorsPoints;
			m_pSensorsPoints = 0;
			delete[] m_pSensorsSlopes;
			m_pSensorsSlopes = 0;
		}
		m_nSensors = params->nSensors;
		if (!m_pSensors && m_nSensors > 0)
		{
			memset(m_pSensors = new Vec3[m_nSensors], 0, m_nSensors * sizeof(Vec3));
			m_pSensorsPoints = new Vec3[m_nSensors];
			m_pSensorsSlopes = new Vec3[m_nSensors];
		}
		for (int i = 0; i < m_nSensors; i++)
		{
			if ((m_pSensors[i] - params->pOrigins[i]).len2() > sqr(0.01f))
			{
				m_bActiveEnvironment = 1;
				(m_pSensorsPoints[i] = m_pos + m_qrot * m_pSensors[i]).z = m_pos.z;
				m_pSensorsSlopes[i].Set(0, 0, 1);
				m_iSensorsActive &= ~(1 << i);
			}
			m_pSensors[i] = params->pOrigins[i];
		}
		return 1;
	}

	return 0;
}

int CLivingEntity::GetParams(pe_params* _params)
{
	int res = CPhysicalEntity::GetParams(_params);
	if (res)
	{
		return res;
	}

	if (_params->type == pe_player_dimensions::type_id)
	{
		pe_player_dimensions* params = (pe_player_dimensions*)_params;
		ReadLock lock(m_lockLiving);
		params->heightPivot = m_hPivot;
		params->sizeCollider = m_size;
		params->heightCollider = m_hCyl;
		params->heightEye = m_hEye;
		params->headRadius = m_HeadGeom.m_sphere.r;
		params->heightHead = m_hHead;
		params->bUseCapsule = m_bUseCapsule;
		return 1;
	}

	if (_params->type == pe_player_dynamics::type_id)
	{
		pe_player_dynamics* params = (pe_player_dynamics*)_params;
		ReadLock lock(m_lockLiving);
		params->kInertia = m_kInertia;
		params->kInertiaAccel = m_kInertiaAccel;
		params->kAirControl = m_kAirControl;
		params->kAirResistance = m_kAirResistance;
		params->gravity = m_gravity;
		params->nodSpeed = m_nodSpeed;
		params->bSwimming = m_bSwimming;
		params->mass = m_mass;
		params->surface_idx = m_surface_idx;
		params->minSlideAngle = cry_acosf(m_slopeSlide) * (180.0f / g_PI);
		params->maxClimbAngle = cry_acosf(m_slopeClimb) * (180.0f / g_PI);
		params->maxJumpAngle = cry_acosf(m_slopeJump) * (180.0f / g_PI);
		params->minFallAngle = cry_acosf(m_slopeFall) * (180.0f / g_PI);
		params->maxVelGround = m_maxVelGround;
		params->timeImpulseRecover = m_timeImpulseRecover;
		params->collTypes = m_collTypes;
		params->bActive = m_bActive;
		params->bNetwork = m_szHistory == SZ_HISTORY;
		params->iRequestedTime = m_iRequestedTime;
		return 1;
	}

	if (_params->type == pe_params_sensors::type_id)
	{
		pe_params_sensors* params = (pe_params_sensors*)_params;
		ReadLock lock(m_lockLiving);
		if (m_pSensors && m_nSensors != params->nSensors)
		{
			delete[] m_pSensors;
			m_pSensors = 0;
			delete[] m_pSensorsPoints;
			m_pSensorsPoints = 0;
			delete[] m_pSensorsSlopes;
			m_pSensorsSlopes = 0;
		}
		m_nSensors = params->nSensors;
		if (!m_pSensors && m_nSensors > 0)
		{
			m_pSensors = new Vec3[m_nSensors];
			m_pSensorsPoints = new Vec3[m_nSensors];
			m_pSensorsSlopes = new Vec3[m_nSensors];
		}
		for (int i = 0; i < m_nSensors; i++)
		{
			m_pSensors[i] = params->pOrigins[i];
		}
		return 1;
	}
	return 0;
}

void CLivingEntity::AllocateExtendedHistory()
{
	int i;
	if (m_history == m_history_buf)
	{
		m_history = new le_history_item[SZ_HISTORY];
		for (i = 0; i < m_szHistory; i++)
		{
			m_history[i] = m_history_buf[i];
		}
		m_szHistory = SZ_HISTORY;
		for (; i < m_szHistory; i++)
		{
			m_history[i].dt = 1E10;
			m_history[i].pos.zero();
			m_history[i].v.zero();
			m_history[i].bFlying = 0;
			m_history[i].q.SetIdentity();
			m_history[i].timeFlying = 0;
			m_history[i].idCollider = -2;
			m_history[i].posColl.zero();
		}
	}

	if (m_actions == m_actions_buf)
	{
		m_actions = new pe_action_move[SZ_ACTIONS];
		for (i = 0; i < m_szActions; i++)
		{
			m_actions[i] = m_actions_buf[i];
		}
		m_szActions = SZ_ACTIONS;
		for (; i < m_szActions; i++)
		{
			m_actions[i].dt = 1E10;
			MARK_UNUSED m_actions[i].dir;
			m_actions[i].iJump = 0;
		}
	}
}

int CLivingEntity::AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id, int bThreadSafe)
{
	m_parts[0].flags &= ~(geom_colltype_ray | geom_colltype13);
	id = CPhysicalEntity::AddGeometry(pgeom, params, id);
	m_parts[m_nParts - 1].flags |= geom_monitor_contacts;
	return id;
}

void CLivingEntity::RemoveGeometry(int id, int bThreadSafe)
{
	CPhysicalEntity::RemoveGeometry(id);
	if (m_nParts == 1)
	{
		m_parts[0].flags |= geom_colltype_ray;
	}
}

int CLivingEntity::GetStatus(pe_status* _status)
{
	if (_status->type == pe_status_pos::type_id)
	{
		pe_status_pos* status = (pe_status_pos*)_status;
		int res, flags = status->flags;
		status->flags |= status_thread_safe;
		{
			WriteLock lock(m_lockUpdate);
			Vec3 prevPos = m_pos;
			if (m_bActive)
			{
				m_pos += m_deltaPos * (m_timeSmooth * (1 / 0.3f)) - m_qrot * Vec3(0, 0, m_dh);
			}
			res = CPhysicalEntity::GetStatus(_status);
			m_pos = prevPos;
		}
		status->flags = flags;
		return res;
	}

	if (CPhysicalEntity::GetStatus(_status))
	{
		return 1;
	}

	if (_status->type == pe_status_living::type_id)
	{
		pe_status_living* status = (pe_status_living*)_status;
		ReadLock lock(m_lockLiving);
		status->bFlying = m_bFlying & m_bActive;
		status->timeFlying = m_timeFlying;
		status->camOffset = m_qrot * Vec3(0, 0, -m_dh);

		int idx, len;
		float dt;
		for (idx = m_iHist, len = 0, dt = 0; len < 3 && m_history[(idx - 1) & (m_szHistory - 1)].dt < 1E9;
		     idx = (idx - 1) & (m_szHistory - 1), len++)
		{
			dt += m_history[idx].dt;
		}
		if (idx == m_iHist ||
		    (status->vel = (m_history[m_iHist].pos - m_history[idx].pos) / dt).len2() > sqr(6.0f))
		{
			status->vel = m_vel;
			if (m_pLastGroundCollider)
			{
				status->vel += m_velGround;
			}
		}

		status->velUnconstrained = m_vel;
		status->velRequested = m_velRequested;
		status->velGround = m_velGround;
		status->groundHeight = m_hLatest;
		status->groundSlope = m_nslope;
		status->groundSurfaceIdx = m_lastGroundSurfaceIdx;
		status->groundSurfaceIdxAux = m_lastGroundSurfaceIdxAux;
		status->pGroundCollider = m_pLastGroundCollider;
		status->iGroundColliderPart = m_iLastGroundColliderPart;
		status->timeSinceStanceChange = m_timeSinceStanceChange;
		status->bOnStairs = isneg(0.1f - m_timeOnStairs);
		status->bStuck = m_bStuck;
		status->pLockStep = &m_lockStep;
		status->iCurTime = m_iCurTime;
		status->bSquashed = m_bSquashed;
		return 1;
	}
	else

	    if (_status->type == pe_status_sensors::type_id)
	{
		pe_status_sensors* status = (pe_status_sensors*)_status;
		ReadLock lock(m_lockLiving);
		status->pPoints = m_pSensorsPoints;
		status->pNormals = m_pSensorsSlopes;
		status->flags = m_iSensorsActive;
		return m_nSensors;
	}

	if (_status->type == pe_status_dynamics::type_id)
	{
		pe_status_dynamics* status = (pe_status_dynamics*)_status;
		ReadLock lock(m_lockLiving);
		status->v = m_vel;
		if (m_pLastGroundCollider)
		{
			status->v += m_velGround;
		}
		float rdt = 1.0f / m_history[m_iHist].dt;
		quaternionf dq = m_history[m_iHist].q * !m_history[(m_iHist - 1) & (m_szHistory - 1)].q;
		status->a = (m_history[m_iHist].v - m_history[(m_iHist - 1) & (m_szHistory - 1)].v) * rdt;
		if (dq.v.len2() < sqr(0.05f))
		{
			status->w = dq.v * (2 * rdt);
		}
		else
		{
			status->w = dq.v.normalized() * (acos_tpl(dq.w) * 2 * rdt);
		}
		status->centerOfMass = (m_BBox[0] + m_BBox[1]) * 0.5f;
		status->mass = m_mass;
		return 1;
	}

	if (_status->type == pe_status_timeslices::type_id)
	{
		pe_status_timeslices* status = (pe_status_timeslices*)_status;
		ReadLock lock(m_lockLiving);
		if (!status->pTimeSlices)
		{
			return 0;
		}
		int i, nSlices = 0;
		float dt;
		if (is_unused(status->time_interval))
		{
			if (m_actions[m_iAction].dt > 1E9)
			{
				return 0;
			}
			status->time_interval = m_actions[m_iAction].dt;
		}

		for (i = m_iHist, dt = status->time_interval; dt > status->precision;
		     dt -= m_history[i].dt, i = (i - 1) & (m_szHistory - 1))
			;
		for (i = (i + 1) & (m_szHistory - 1); i != ((m_iHist + 1) & (m_szHistory - 1)) && nSlices < status->sz;
		     i = (i + 1) & (m_szHistory - 1))
		{
			status->pTimeSlices[nSlices++] = m_history[i].dt;
		}
		return nSlices;
	}

	if (_status->type == pe_status_collisions::type_id)
	{
		pe_status_collisions* status = (pe_status_collisions*)_status;
		return m_bHadCollisions--;
	}

	if (_status->type == pe_status_check_stance::type_id)
	{
		pe_status_check_stance* status = (pe_status_check_stance*)_status;
		Vec3 pos = m_pos, size = m_size;
		quaternionf qrot = m_qrot;
		float hCollider = m_hCyl;
		int bCapsule = m_bUseCapsule;
		if (!is_unused(status->pos))
		{
			pos = status->pos;
		}
		if (!is_unused(status->q))
		{
			qrot = status->q;
		}
		if (!is_unused(status->sizeCollider))
		{
			size = status->sizeCollider;
		}
		if (!is_unused(status->heightCollider))
		{
			hCollider = status->heightCollider;
		}
		if (!is_unused(status->bUseCapsule))
		{
			bCapsule = status->bUseCapsule;
		}

		return ((status->unproj = UnprojectionNeeded(pos, qrot, hCollider, m_hPivot, size, bCapsule,
		                                             status->dirUnproj)) == 0);
	}

	return 0;
}

int CLivingEntity::Action(pe_action* _action, int bThreadSafe)
{
	ChangeRequest<pe_action> req(this, m_pWorld, _action, bThreadSafe);
	if (req.IsQueued())
	{
		return 1;
	}

	if (_action->type == pe_action_move::type_id)
	{
		if (!m_bIgnoreCommands)
		{
			pe_action_move* action = (pe_action_move*)_action;
			bool bForceHistory = false;

			WriteLockCond lock(m_lockLiving, bThreadSafe ^ 1);
			if (action->dt == 0 && !is_unused(action->dir))
			{
				if (!action->iJump || action->iJump == 3)
				{
					m_velRequested = action->dir;
					if (action->iJump == 3)
					{
						m_forceFly = true;
					}
				}
				else
				{
					if (action->iJump == 2)
					{
						m_vel += action->dir;
						bForceHistory = true;
						if (action->dir.z > 1.0f)
						{
							m_minFlyTime = 0.2f;
							m_timeFlying = 0;
							m_bJumpRequested = 1;
						}
					}
					else
					{
						m_vel = action->dir;
						bForceHistory = true;
						m_minFlyTime = 0.2f;
						m_timeFlying = 0;
						m_bJumpRequested = 1;
					}
				}
				if (m_flags & lef_snap_velocities)
				{
					m_velRequested = DecodeVec4b(EncodeVec4b(m_velRequested));
					m_vel = DecodeVec6b(EncodeVec6b(m_vel));
				}
				if (bForceHistory)
				{
					m_history[m_iHist].v = m_vel;
				}
			}
			m_actions[++m_iAction &= m_szActions - 1] = *action;
			return 1;
		}
		return 0;
	}

	if (_action->type == pe_action_impulse::type_id)
	{
		pe_action_impulse* action = (pe_action_impulse*)_action;

		WriteLock lock(m_lockLiving);
		Vec3 impulse = action->impulse;
		if (action->iSource == 2) // explosion
		{
			impulse *= 0.3f;
		}
		if (action->iApplyTime == 0)
		{
			// if (!m_bFlying) m_velRequested.zero();
			m_vel += impulse * m_massinv;
			// m_velRequested = m_vel;
			if (m_flags & lef_snap_velocities)
			{
				m_vel = DecodeVec6b(EncodeVec6b(m_vel));
			}
			if (impulse.z * m_massinv > 1.0f)
			{
				m_bFlying = 1;
				m_minFlyTime = 0.2f;
				m_timeFlying = 0;
			}
		}
		else
		{
			pe_action_move am;
			am.dir = impulse * m_massinv;
			am.iJump = 2;
			Action(&am, 1);
		}
		if (m_kInertia == 0)
		{
			m_timeForceInertia = m_timeImpulseRecover;
		}
		return 1;
	}

	if (_action->type == pe_action_reset::type_id)
	{
		WriteLock lock(m_lockLiving);
		m_vel.zero();
		m_velRequested.zero();
		m_dh = m_dhSpeed = 0;
		m_stablehTime = 1;
		m_bFlying = 1;
		return 1;
	}

	if (_action->type == pe_action_set_velocity::type_id)
	{
		pe_action_set_velocity* action = (pe_action_set_velocity*)_action;
		WriteLock lock(m_lockLiving);

		if (!is_unused(action->v))
		{
			m_velRequested = m_vel = action->v;
			m_bJumpRequested = 1;
		}

		return 1;
	}

	return CPhysicalEntity::Action(_action, 1);
}

int CLivingEntity::GetStateSnapshot(TSerialize ser, float time_back, int flags)
{
	WriteLock lock0(m_lockUpdate), lock1(m_lockLiving);

	SLivingEntityNetSerialize helper = {
	    m_pos,
	    m_vel,
	    m_velRequested,
	};
	helper.Serialize(ser);

	return 1;
}

int CLivingEntity::SetStateFromSnapshot(TSerialize ser, int flags)
{
	SLivingEntityNetSerialize helper;
	helper.Serialize(ser);

	if ((ser.GetSerializationTarget() == eST_Network) && (helper.pos.len2() < 50.0f))
	{
		return 1;
	}

	// DO NOT CHECK IN
	// DO NOT CHECK IN
	// DO NOT CHECK IN
	// DO NOT CHECK IN
	//	helper.pos.x += rand() / float(RAND_MAX);
	//	helper.pos.y += rand() / float(RAND_MAX);
	// DO NOT CHECK IN
	// DO NOT CHECK IN
	// DO NOT CHECK IN
	// DO NOT CHECK IN

	if (ser.ShouldCommitValues() && m_pWorld)
	{
		pe_params_pos setpos;
		setpos.bRecalcBounds |= 16 | 32;
		pe_action_set_velocity velocity;

		Vec3 debugOnlyOriginalHelperPos = helper.pos;

		if (flags & ssf_compensate_time_diff)
		{
			float dtBack = (m_pWorld->m_iTimeSnapshot[1] - m_pWorld->m_iTimeSnapshot[0]) *
			               m_pWorld->m_vars.timeGranularity;
			helper.pos += helper.vel * dtBack;
		}

		const float MAX_DIFFERENCE = std::max(helper.vel.GetLength() * 0.1f, 0.1f);

		float distance = m_pos.GetDistance(helper.pos);
		if (ser.GetSerializationTarget() == eST_Network)
		{
			if (distance > MAX_DIFFERENCE)
			{
				setpos.pos = helper.pos + (m_pos - helper.pos).GetNormalized() * MAX_DIFFERENCE;
			}
			else
			{
				setpos.pos = m_pos + (helper.pos - m_pos) * distance / MAX_DIFFERENCE * 0.033f;
			}
		}
		else
		{
			setpos.pos = helper.pos;
		}

		SetParams(&setpos, 0);

		WriteLock lock1(m_lockLiving);
		m_vel = helper.vel;
		m_velRequested = helper.velRequested;
	}

	return 1;
}

float CLivingEntity::ShootRayDown(CPhysicalEntity** pentlist, int nents, const Vec3& pos, Vec3& nslope,
                                  float time_interval, bool bUseRotation, bool bUpdateGroundCollider,
                                  bool bIgnoreSmallObjects)
{
	int i, j, ibest = -1, jbest = 0, ncont = 0, idbest = 0, idbestAux = -1;
	Matrix33 R;
	Vec3 pt, axis = m_qrot * Vec3(0, 0, 1);
	float h = -1E10f, maxdim, maxarea, haux = -1E10f;
	primitives::box bbox;
	if (bUseRotation)
	{
		R = Matrix33(m_qrot);
	}
	else
	{
		maxdim = sqr(m_qrot.v.z) + sqr(m_qrot.w);
		if (maxdim > 0.0001f)
		{
			maxdim = 1 / sqrt_tpl(maxdim);
			R = Matrix33(m_qrot * Quat(m_qrot.w * maxdim, 0, 0, m_qrot.v.z * maxdim));
		}
		else
		{
			R = Matrix33(Quat(0, 1, 0, 0));
		}
	}
	const Vec3 dirp = (R * Vec3(0, 0, -1));
	CRayGeom aray;
	aray.CreateRay(pos + R * Vec3(0, 0, m_hCyl - m_size.z - m_hPivot), R * Vec3(0, 0, -1.5f * (m_hCyl - m_size.z)),
	               &dirp);
	geom_world_data gwd;
	geom_contact* pcontacts;
	intersection_params ip;
	CPhysicalEntity* pPrevCollider = m_pLastGroundCollider;
	nslope = m_nslope;

	for (i = nents - 1; i >= 0; i--)
	{
		if (pentlist[i] != this && !pentlist[i]->IgnoreCollisionsWith(this, 1))
		{
			for (j = 0; j < pentlist[i]->m_nColliders &&
			            (pentlist[i]->m_pColliders[j]->m_iSimClass != 3 ||
			             !pentlist[i]->HasConstraintContactsWith(pentlist[i]->m_pColliders[j]));
			     j++)
				;
			if (j < pentlist[i]->m_nColliders)
			{
				continue;
			}
			for (j = 0; j < pentlist[i]->m_nParts; j++)
			{
				if (pentlist[i]->m_parts[j].flags & geom_colltype_player)
				{
					if (bIgnoreSmallObjects &&
					    (ibest >= 0 || pentlist[i]->GetRigidBody(j)->v.len2() > 1))
					{
						pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom->GetBBox(&bbox);
						maxarea = max(max(bbox.size.x * bbox.size.y, bbox.size.x * bbox.size.z),
						              bbox.size.y * bbox.size.z) *
						          sqr(pentlist[i]->m_parts[j].scale) * 4;
						maxdim = max(max(bbox.size.x, bbox.size.y), bbox.size.z) *
						         pentlist[i]->m_parts[j].scale * 2;
						if (maxarea < sqr(m_size.x) * g_PI * 0.25f && maxdim < m_size.z * 1.4f)
						{
							continue;
						}
					}
					gwd.R = Matrix33(pentlist[i]->m_qrot * pentlist[i]->m_parts[j].q);
					gwd.offset =
					    pentlist[i]->m_pos + pentlist[i]->m_qrot * pentlist[i]->m_parts[j].pos;
					gwd.scale = pentlist[i]->m_parts[j].scale;
					ncont = pentlist[i]->m_parts[j].pPhysGeom->pGeom->Intersect(&aray, &gwd, 0, &ip,
					                                                            pcontacts);
					if (ncont)
					{
						WriteLockCond lockColl(*ip.plock, 0);
						lockColl.SetActive();
						if (pcontacts[ncont - 1].n * aray.m_ray.dir < 0)
						{
							if (pentlist[i]->m_parts[j].flags & geom_colltype0 &&
							    pcontacts[ncont - 1].pt * axis > haux)
							{
								haux = (pt = pcontacts[ncont - 1].pt) * axis;
								idbestAux = pentlist[i]->GetMatId(
								    pcontacts[ncont - 1].id[0], j);
							}
							if (pcontacts[ncont - 1].pt * axis > h)
							{
								h = (pt = pcontacts[ncont - 1].pt) * axis;
								ibest = i;
								jbest = j;
								nslope = pcontacts[ncont - 1].n;
								if (pentlist[i]->m_iSimClass == 3)
								{
									nslope = pcontacts[ncont - 1].pt -
									         pentlist[i]->m_pos;
									nslope -= axis * (axis * nslope);
									if (nslope.len2() > 1E-4f)
									{
										nslope.normalize();
									}
									else
									{
										nslope = m_qrot * Vec3(1, 0, 0);
									}
									// nslope = nslope*0.965925826f +
									// axis*0.2588190451f; // make it act like a 75
									// degrees slope
									nslope =
									    nslope * sqrt_tpl(max(
											 0.0f, 1.0f - sqr(m_slopeFall *
									                                  1.01f))) +
									    axis * (m_slopeFall * 1.01f);
								}
								idbest = pentlist[i]->GetMatId(
								    pcontacts[ncont - 1].id[0], j);
							}
						}
					}
				}
			}
		}
	}

	if (bUpdateGroundCollider)
	{
		ReleaseGroundCollider();
		m_lastGroundSurfaceIdx = m_lastGroundSurfaceIdxAux = -1;
		if (ibest >= 0)
		{
			SetGroundCollider(pentlist[ibest],
			                  (pentlist[ibest]->m_parts[jbest].flags & geom_manually_breakable) != 0);
			m_idLastGroundColliderPart = pentlist[ibest]->m_parts[m_iLastGroundColliderPart = jbest].id;
			Vec3 coll_origin =
			    pentlist[ibest]->m_pos + pentlist[ibest]->m_qrot * pentlist[ibest]->m_parts[jbest].pos;
			quaternionf coll_q = pentlist[ibest]->m_qrot * pentlist[ibest]->m_parts[jbest].q;
			float coll_scale = pentlist[ibest]->m_parts[jbest].scale;
			m_posLastGroundColl = ((pt - coll_origin) * coll_q) / coll_scale;
			m_lastGroundSurfaceIdx = idbest;
			m_lastGroundSurfaceIdxAux = idbestAux;

			if (pPrevCollider != pentlist[ibest])
			{
				AddLegsImpulse(m_gravity * time_interval + m_vel, nslope, true);
			}
			else
			{
				AddLegsImpulse(m_gravity * time_interval, nslope, false);
			}

			pe_status_dynamics sd;
			sd.ipart = jbest;
			pentlist[ibest]->GetStatus(&sd);
			m_velGround = sd.v + (sd.w ^ pt - sd.centerOfMass);
		}
	}
	return h;
}

void CLivingEntity::AddLegsImpulse(const Vec3& vel, const Vec3& nslope, bool bInstantChange)
{
	int ncoll;
	CPhysicalEntity **pColliders, *pPrevCollider = m_pLastGroundCollider;
	pe_status_sample_contact_area ssca;
	RigidBody* pbody;
	pe_action_impulse ai;
	ssca.ptTest = m_pos - m_qrot * Vec3(0, 0, m_hPivot);
	ssca.dirTest = m_gravity;

	if (m_pLastGroundCollider && m_flags & lef_push_objects &&
	    (unsigned int)m_pLastGroundCollider->m_iSimClass - 1u < 2u &&
	    m_pLastGroundCollider->m_flags & pef_pushable_by_players &&
	    ((pbody = m_pLastGroundCollider->GetRigidBody(m_iLastGroundColliderPart))->Minv < m_massinv * 10 ||
	     (ncoll = m_pLastGroundCollider->GetColliders(pColliders)) == 0 ||
	     (ncoll == 1 && pColliders[0] == m_pLastGroundCollider)) &&
	    !m_pLastGroundCollider->GetStatus(&ssca))
	{
		Vec3 vrel = vel;
		ai.point = ssca.ptTest;
		if (bInstantChange)
		{
			vrel -= pbody->v + (pbody->w ^ ai.point - pbody->pos);
		}
		Matrix33 K;
		K.SetZero();
		m_pLastGroundCollider->GetContactMatrix(ai.point, m_iLastGroundColliderPart, K);
		K(0, 0) += m_massinv * 0.5f;
		K(1, 1) += m_massinv * 0.5f;
		K(2, 2) += m_massinv * 0.5f;
		ai.impulse = nslope * (min(0.0f, nslope * vrel) / (nslope * K * nslope));
		m_pLastGroundCollider->Action(&ai, 1);
	}
}

void CLivingEntity::RegisterContact(const Vec3& posSelf, const Vec3& pt, const Vec3& n, CPhysicalEntity* pCollider,
                                    int ipart, int idmat, float imp, int bLegsContact)
{
	if (m_flags & (pef_monitor_collisions | pef_log_collisions) && !m_bStateReading)
	{
		EventPhysCollision epc;
		epc.pEntity[0] = this;
		epc.pForeignData[0] = m_pForeignData;
		epc.iForeignData[0] = m_iForeignData;
		epc.pEntity[1] = pCollider;
		epc.pForeignData[1] = pCollider->m_pForeignData;
		epc.iForeignData[1] = pCollider->m_iForeignData;
		RigidBody* pbody = pCollider->GetRigidBody(ipart);
		Vec3 v = pbody->v + (pbody->w ^ pt - pbody->pos);
		epc.n = n;
		if (bLegsContact)
		{
			if (!(pCollider->m_parts[ipart].flags & geom_manually_breakable))
			{
				if (!((v - m_vel - m_velGround).len2() > 3 && pbody->V < m_size.GetVolume() * 8))
				{
					return;
				}
				epc.n = v.normalized();
				imp = (epc.n * (v - m_vel - m_velGround)) * (m_mass * (m_mass * pbody->Minv + 1.0f));
			}
		}
		epc.pt = pt;
		epc.vloc[0] = m_vel + m_velGround;
		epc.vloc[1] = v;
		epc.mass[0] = m_mass;
		epc.mass[1] = pbody->M;
		epc.idCollider = m_pWorld->GetPhysicalEntityId(pCollider);
		epc.partid[0] = 100;
		epc.partid[1] = pCollider->m_parts[ipart].id;
		epc.idmat[0] = m_surface_idx;
		epc.idmat[1] = pCollider->GetMatId(idmat, ipart);
		epc.penetration = epc.radius = 0;
		epc.normImpulse = max(0.0f, imp);
		if (pCollider->m_parts[ipart].flags & geom_manually_breakable)
		{
			epc.pt = posSelf + m_qrot * m_parts[0].pos;
			epc.pt += n * (n * (pt - epc.pt));
			float cosa = n * (m_qrot * Vec3(0, 0, 1)), sina = sqrt_tpl(max(0.0f, 1.0f - (cosa * cosa)));
			epc.radius = (m_size.x * cosa + (m_size.z + m_size.x * m_bUseCapsule) * sina) * 1.05f;
		}
		m_pWorld->OnEvent(m_flags, &epc);
		m_bHadCollisions = 1;
	}
}

void CLivingEntity::RegisterUnprojContact(const le_contact& unproj)
{
	if (m_nContacts == m_nContactsAlloc)
	{
		le_contact* pContacts = m_pContacts;
		m_pContacts = new le_contact[m_nContactsAlloc += 4];
		memcpy(m_pContacts, pContacts, m_nContacts * sizeof(le_contact));
		if (pContacts)
		{
			delete[] pContacts;
		}
	}
	m_pContacts[m_nContacts] = unproj;
	m_pContacts[m_nContacts++].pSolverContact[0] = 0;
	AddCollider(unproj.pent);
	unproj.pent->AddCollider(this);
}

float CLivingEntity::GetMaxTimeStep(float time_interval)
{
	if (m_timeStepPerformed > m_timeStepFull - 0.001f)
	{
		return time_interval;
	}
	return min_safe(m_timeStepFull - m_timeStepPerformed, time_interval);
}

Vec3 CLivingEntity::SyncWithGroundCollider(float time_interval)
{
	int i;
	Vec3 newpos = m_pos;
	if (m_pLastGroundCollider &&
	    (m_pLastGroundCollider->m_iSimClass == 7 || m_iLastGroundColliderPart >= m_pLastGroundCollider->m_nParts ||
	     m_idLastGroundColliderPart != m_pLastGroundCollider->m_parts[m_iLastGroundColliderPart].id))
	{
		ReleaseGroundCollider();
	}

	WriteLockCond lock(m_lockLiving, m_bStateReading ^ 1);
	m_velGround.zero();

	if (m_pLastGroundCollider)
	{
		i = m_iLastGroundColliderPart;
		Vec3 coll_origin = m_pLastGroundCollider->m_pos +
		                   m_pLastGroundCollider->m_qrot * m_pLastGroundCollider->m_parts[i].pos;
		quaternionf coll_q = m_pLastGroundCollider->m_qrot * m_pLastGroundCollider->m_parts[i].q;
		float coll_scale = m_pLastGroundCollider->m_parts[i].scale;
		newpos = coll_q * m_posLastGroundColl * coll_scale + coll_origin + m_qrot * Vec3(0, 0, m_hPivot);
		if ((newpos - m_pos).len2() > sqr(time_interval * 0.01f))
		{
			if ((newpos - m_pos).len2() > sqr(time_interval * m_maxVelGround))
			{
				newpos = m_pos + (newpos - m_pos).normalized() * (m_maxVelGround * time_interval);
			}
		}
		pe_status_dynamics sd;
		sd.ipart = i;
		m_pLastGroundCollider->GetStatus(&sd);

		m_velGround = sd.v + (sd.w ^ newpos - sd.centerOfMass);
		if (m_velGround.len2() > sqr(m_maxVelGround))
		{
			m_velGround.normalize() *= m_maxVelGround;
		}
	}
	return newpos;
}

void CLivingEntity::ComputeBBox(Vec3* BBox, int flags)
{
	CPhysicalEntity::ComputeBBox(BBox, flags);
	Vec3 pt = m_pNewCoords->pos - m_pNewCoords->q * Vec3(0, 0, m_hPivot);
	BBox[0] = min(BBox[0], pt);
	BBox[1] = max(BBox[1], pt);
	if (m_HeadGeom.m_sphere.r > 0)
	{
		pt = m_pNewCoords->pos + m_pNewCoords->q * Vec3(0, 0, m_hHead + m_HeadGeom.m_sphere.r - m_hPivot);
		BBox[0] = min(BBox[0], pt);
		BBox[1] = max(BBox[1], pt);
	}
}

void CLivingEntity::ComputeBBoxLE(const Vec3& pos, Vec3* BBox, coord_block_BBox* partCoord)
{
	coord_block coord;
	coord.pos = pos;
	coord.q = m_qrot;
	m_pNewCoords = &coord;
	partCoord->pos = m_parts[0].pos;
	partCoord->q = m_parts[0].q;
	partCoord->scale = m_parts[0].scale;
	m_parts[0].pNewCoords = partCoord;
	ComputeBBox(BBox, update_part_bboxes & (m_nParts - 2) >> 31);
	m_pNewCoords = (coord_block*)&m_pos;
}

void CLivingEntity::UpdatePosition(const Vec3& pos, const Vec3* BBox, int bGridLocked)
{
	WriteLockCond lock(m_lockUpdate, m_bStateReading ^ 1);
	m_pos = pos;

	if (m_nParts == 1)
	{
		m_parts[0].BBox[0] = m_parts[0].pNewCoords->BBox[0];
		m_parts[0].BBox[1] = m_parts[0].pNewCoords->BBox[1];
		m_parts[0].pNewCoords = (coord_block_BBox*)&m_parts[0].pos;
		m_BBox[0] = BBox[0];
		m_BBox[1] = BBox[1];
	}
	else
	{
		for (int i = 0; i < m_nParts; i++)
		{
			m_parts[i].pNewCoords = (coord_block_BBox*)&m_parts[i].pos;
		}
		ComputeBBox(m_BBox);
	}
	AtomicAdd(&m_pWorld->m_lockGrid, -bGridLocked);
}

void CLivingEntity::StartStep(float time_interval)
{
	m_timeStepPerformed = 0;
	m_timeStepFull = time_interval;
	m_nContacts = 0;
}

int CLivingEntity::Step(float time_interval)
{
	if (time_interval <= 0)
	{
		return 1;
	}
	if (m_timeStepPerformed > m_timeStepFull - 0.001f && m_pWorld->m_vars.bMultiplayer + m_bStateReading == 0)
	{
		time_interval = 0.001f;
	}
	int i, j, imin, jmin = 0, iter, nents, ncont, bFlying, bWasFlying, bPushOther, bUnprojected, idmat = 0,
			bFastPhys = 0, bPush, bHasFastPhys, iCyl, icnt, nUnproj, bStaticUnproj, bDynUnproj, bMoving = 0,
			bCheckBBox;
	Vec3 pos, vel, pos0, vel0, newpos, move(ZERO), nslope, ncontact, ptcontact, ncontactHist[4], ncontactSum,
	    BBoxInner[2], BBoxOuter[2], velGround, axis;
	float movelen, tmin, h, hcur, dh = 0, tfirst, vrel, move0, movesum, kInertia, imp;
	coord_block_BBox partCoord;
	le_contact unproj[8];
	CCylinderGeom CylinderGeomOuter, *pCyl[2];
	geom_world_data gwd[2];
	intersection_params ip;
	CPhysicalEntity** pentlist;
	geom_contact* pcontacts;
	pe_player_dimensions pd;
	pe_action_impulse ai;
	pe_status_dynamics sd;
	pe_player_dynamics pdyn;
	EventPhysBBoxOverlap event;
	Matrix33 K;
	WriteLockCond lockMain(m_lockStep, m_bStateReading ^ 1);
	ip.bNoAreaContacts = true;

	if (m_iRequestedTime && m_iCurTime > m_iRequestedTime)
	{
		int iaction, nActions, nSteps, bChange;
		float dt, dtState, dtAction;
		m_bStateReading = 1;

		dtState = (m_iCurTime - m_iRequestedTime) * m_pWorld->m_vars.timeGranularity;
		m_iRequestedTime = 0;
		StepBackEx(dtState, false);

		for (dtAction = dtState, nActions = 0; dtAction > 0.00001f && nActions < m_szActions;
		     dtAction -= m_actions[m_iAction].dt, --m_iAction &= m_szActions - 1, nActions++)
			;
		for (dt = dtState, nSteps = 0; dt > 0.00001f && nSteps < m_szHistory && m_history[m_iHist].dt < 1E9;
		     dt -= m_history[m_iHist].dt, --m_iHist &= m_szHistory - 1, nSteps++)
			;
		if (m_szActions == 0 || dtAction < -3.0f)
		{
			dtAction = 1E10f;
		}

		do
		{
			bChange = 0;
			for (; dtAction < 0.00001f && nActions > 0; nActions--)
			{
				iaction = (m_iAction + 1) & (m_szActions - 1);
				if (fabs_tpl(dtAction) < 0.00001f)
				{
					dtAction = 0;
				}
				dtAction += m_actions[iaction].dt;
				m_actions[iaction].dt = 0;
				Action(m_actions + iaction);
				bChange = 1;
			}
			for (; dtAction >= 0.00001f && nSteps > 0; nSteps--)
			{
				if (fabs_tpl(dt) < 0.00001f)
				{
					dt = 0;
				}
				dt += m_history[(m_iHist + 1) & (m_szHistory - 1)].dt;
				Step(dt);
				dtAction -= dt;
				dt = 0;
				bChange = 1;
			}
		}
		while (nActions + nSteps > 0 && bChange);
		m_bStateReading = 0;
	}
	m_iRequestedTime = 0;

	if (m_timeForceInertia > 0.0001f)
	{
		kInertia = 6.0f;
	}
	else if (m_kInertiaAccel && m_velRequested.len2() > 0.1f)
	{
		kInertia = m_kInertiaAccel;
	}
	else
	{
		kInertia = m_kInertia;
	}

	pos = SyncWithGroundCollider(time_interval);
	vel0 = vel = m_vel;
	m_actions[m_iAction].dt += time_interval;
	bWasFlying = bFlying = m_bFlying;
	if (!m_bStateReading)
	{
		m_timeUseLowCap -= time_interval;
		m_timeSinceStanceChange += time_interval;
		m_timeSinceImpulseContact += time_interval;
		m_timeForceInertia = max(0.0f, m_timeForceInertia - time_interval);
		m_timeStepPerformed += time_interval;
		m_iCurTime = m_pWorld->m_iTimePhysics - physics_float2int((m_timeStepPerformed - m_timeStepFull) *
		                                                          m_pWorld->m_vars.rtimeGranularity);
	}
	m_bUseSphere = 0;
	event.pEntity[0] = this;
	event.pForeignData[0] = m_pForeignData;
	event.iForeignData[0] = m_iForeignData;

	if (m_bActive &&
	    (!(vel.len2() == 0 && m_velRequested.len2() == 0 && (!bFlying || m_gravity.len2() == 0) && m_dhSpeed == 0 &&
	       m_dhAcc == 0 && m_qrot.w == m_history[(m_iHist - 1) & (m_szHistory - 1)].q.w &&
	       (m_qrot.v - m_history[(m_iHist - 1) & (m_szHistory - 1)].q.v).len2() == 0) ||
	     m_nslope.z < m_slopeSlide || m_velGround.len2() > 0 || m_bActiveEnvironment))
	{
		FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

		m_bActiveEnvironment = 0;
		// m_nslope.Set(0,0,1);
		if (kInertia == 0 && !bFlying && !m_bJumpRequested || m_pWorld->m_vars.bFlyMode)
		{
			vel = m_velRequested;
			vel -= m_nslope * (m_nslope * vel);
		}
		if (m_velRequested.len2() > sqr(m_pWorld->m_vars.maxVelPlayers))
		{
			m_velRequested.normalize() *= m_pWorld->m_vars.maxVelPlayers;
		}
		if (!m_pWorld->m_vars.bFlyMode && bFlying && m_kAirControl > 0)
		{
			if (m_kInertia > 0)
			{
				if (vel * m_velRequested < m_velRequested.len2() * m_kAirControl)
				{
					vel += m_velRequested * (m_kInertia * time_interval);
				}
			}
			else if (m_gravity.len2() > 0)
			{
				vel = m_gravity * (vel * m_gravity - m_velRequested * m_gravity) / m_gravity.len2() +
				      m_velRequested;
			}
			else
			{
				vel = m_velRequested;
			}
			// vel += (m_velRequested-vel)*(m_kAirControl*time_interval*3.33f);
		}
		// filippo:m_forceFly is to let the game set a velocity no matter what is the status of the entity.
		if (m_forceFly)
		{
			vel = m_velRequested;
		}
		if (vel.len2() > sqr(m_pWorld->m_vars.maxVelPlayers))
		{
			vel.normalize() *= m_pWorld->m_vars.maxVelPlayers;
		}
		move += vel * time_interval;
		if (bFlying && !m_bSwimming && !m_pWorld->m_vars.bFlyMode && !m_forceFly)
		{
			move += m_gravity * sqr(time_interval) * 0.5f;
		}
		m_forceFly = false;
		bUnprojected = 0;
		axis = m_qrot * Vec3(0, 0, 1);
		if (_isnan(move.len2()))
		{
			return 1;
		}
		--m_bSquashed;
		m_bSquashed -= m_bSquashed >> 31;

		if (m_pWorld->m_vars.iCollisionMode != 0 && m_pWorld->m_vars.bFlyMode)
		{
			pos += move;
			bFlying = 1;
			m_hLatest = 0;
			ReleaseGroundCollider();
		}
		else
		{
			movelen = move.len();
			h = (movelen + m_pWorld->m_vars.maxContactGapPlayer) * 1.5f;
			BBoxInner[0] = m_BBox[0] + (pos - m_pos) - Vec3(h, h, h + ((m_hCyl - m_size.z) * 0.5f));
			BBoxInner[1] = m_BBox[1] + (pos - m_pos) + Vec3(h, h, h);
			h = max(10.0f * time_interval, h); // adds a safety margin of m_size.x width
			BBoxOuter[0] = m_BBox[0] + (pos - m_pos) - Vec3(h, h, h + ((m_hCyl - m_size.z) * 0.5f));
			BBoxOuter[1] = m_BBox[1] + (pos - m_pos) + Vec3(h, h, h);
			nents = m_pWorld->GetEntitiesAround(
			    BBoxOuter[0], BBoxOuter[1], pentlist,
			    m_collTypes | ent_independent | ent_triggers | ent_sort_by_mass, this);

			for (i = j = bHasFastPhys = 0, vrel = 0; i < nents; i++)
			{
				if (!m_pForeignData || pentlist[i]->m_pForeignData != m_pForeignData)
				{
					Vec3 sz = pentlist[i]->m_BBox[1] - pentlist[i]->m_BBox[0];
					if (pentlist[i]->m_iSimClass == 2)
					{
						if (pentlist[i]->m_flags & ref_small_and_fast)
						{
							continue;
						}
						else if (pentlist[i]->GetMassInv() * 0.4f < m_massinv)
						{
							pentlist[i]->GetStatus(&sd);
							vrel = max(vrel, sd.v.len() +
							                     (sd.w.len() * max(max(sz.x, sz.y), sz.z)));
							;
							bHasFastPhys |= (bFastPhys = isneg((m_size.x * 0.2f) -
							                                   (vrel * time_interval)));
						}
						else
						{
							bFastPhys = 0;
						}
					}
					if (!bFastPhys && !AABB_overlap(pentlist[i]->m_BBox, BBoxInner) &&
					    sz.len2() > 0)
					{
						continue;
					}
					idmat = pentlist[i]->GetType();
					if (idmat == PE_SOFT || idmat == PE_ROPE)
					{
						//|| idmat==PE_LIVING && m_parts[0].flags & geom_colltype_player &&
						//!(m_parts[0].flagsCollider & geom_colltype_player))
						pentlist[i]->Awake();
					}
					else if (pentlist[i]->m_iSimClass < 4 &&
					         (idmat != PE_LIVING && !pentlist[i]->IgnoreCollisionsWith(this, 1) ||
					          idmat == PE_LIVING &&
					              pentlist[i]->m_parts[0].flags & geom_colltype_player &&
					              pentlist[i] !=
					                  this)) // && pentlist[i]->GetParams(&pdyn) && pdyn.bActive))
					{
						if (pentlist[i]->m_iSimClass == 1 && m_timeSinceImpulseContact < 0.2f &&
						    pentlist[i]->GetMassInv() > 0)
						{
							int ipart;
							unsigned int flags;
							for (ipart = 0, flags = 0; ipart < pentlist[i]->m_nParts;
							     ipart++)
							{
								flags |= pentlist[i]->m_parts[ipart].flags;
							}
							if (flags & geom_colltype_player)
							{
								pentlist[i]->Awake();
							}
						}
						pentlist[j++] = pentlist[i];
						if (idmat == PE_LIVING)
						{
							((CLivingEntity*)pentlist[i])
							    ->SyncWithGroundCollider(time_interval);
						}
					}
				}
			}
			nents = j;
			pos0 = pos;
			bStaticUnproj = bDynUnproj = 0;
			newpos = pos + move;
			h = ShootRayDown(pentlist, nents, newpos, nslope);

			// first, check if we need unprojection in the initial position
			if (sqr(m_qrot.v.x) + sqr(m_qrot.v.y) < sqr(0.05f))
			{
				gwd[0].R.SetIdentity();
			}
			else
			{
				gwd[0].R = Matrix33(m_qrot);
			}
			gwd[0].centerOfMass = gwd[0].offset = pos + gwd[0].R * m_parts[0].pos;
			gwd[0].v.zero();
			gwd[0]
			    .w.zero(); // since we check a static character against potentially moving environment here
			ip.vrel_min = m_size.x;
			ip.time_interval = time_interval * 2;
			ip.maxUnproj = m_size.x * 2.5f;
			ip.ptOutsidePivot[0] = gwd[0].offset;
			bFastPhys = 0;
			pCyl[0] = m_pCylinderGeom;
			if (bHasFastPhys)
			{
				primitives::cylinder cylOuter;
				cylOuter.r = m_size.x + min(m_size.x * 1.5f, vrel * time_interval);
				cylOuter.hh = m_size.z + min(m_size.z, vrel * time_interval);
				cylOuter.center.zero();
				cylOuter.axis.Set(0, 0, 1);
				CylinderGeomOuter.CreateCylinder(&cylOuter);
				pCyl[1] = &CylinderGeomOuter;
			}

		retry_without_ground_sync:
			if (m_parts[0].flagsCollider & geom_colltype_player)
			{
				for (i = nUnproj = 0, ncontactSum.zero(); i < nents; i++)
				{
					if (pentlist[i]->GetType() != PE_LIVING)
					{
						float minv = pentlist[i]->GetMassInv();
						int bHeavy = isneg((minv * 0.4f) - m_massinv);
						int bVeryHeavy = -isneg((minv * 2) - m_massinv);
						if (bHasFastPhys && pentlist[i]->m_iSimClass == 2 && bHeavy)
						{
							pentlist[i]->GetStatus(&sd);
							Vec3 sz = pentlist[i]->m_BBox[1] - pentlist[i]->m_BBox[0];
							vrel = max(vrel, sd.v.len() +
							                     (sd.w.len() * max(max(sz.x, sz.y), sz.z)));
							bFastPhys = isneg((m_size.x * 0.2f) - (vrel * time_interval));
							gwd[1].v = sd.v;
							gwd[1].w = sd.w;
							gwd[1].centerOfMass = sd.centerOfMass;
						}
						else
						{
							gwd[1].v.zero();
							gwd[1].w.zero();
							bFastPhys = 0;
						}

						for (iCyl = 0; iCyl <= bFastPhys; iCyl++)
						{
							for (j = 0, bCheckBBox = (pentlist[i]->m_BBox[1] -
							                          pentlist[i]->m_BBox[0])
							                             .len2() > 0;
							     j < pentlist[i]->m_nParts; j++)
							{
								if (pentlist[i]->m_parts[j].flags &
								        geom_colltype_player &&
								    (!bCheckBBox ||
								     AABB_overlap(BBoxInner,
								                  pentlist[i]->m_parts[j].BBox)))
								{
									gwd[1].R = Matrix33(pentlist[i]->m_qrot *
									                    pentlist[i]->m_parts[j].q);
									gwd[1].offset = pentlist[i]->m_pos +
									                pentlist[i]->m_qrot *
									                    pentlist[i]->m_parts[j].pos;
									gwd[1].scale = pentlist[i]->m_parts[j].scale;

									icnt = pCyl[iCyl]->Intersect(
									    pentlist[i]
										->m_parts[j]
										.pPhysGeomProxy->pGeom,
									    gwd, gwd + 1, &ip, pcontacts);
									if (icnt)
									{
										WriteLockCond lockColl(*ip.plock, 0);
										lockColl.SetActive();
										for (ncont = 0;
										     ncont < icnt &&
										     pcontacts[ncont].dir *
										             (pcontacts[ncont].pt -
										              gwd[0].offset) >
										         0;
										     ncont++)
											;
										if (m_velGround.len2() < 0.001f)
										{
											if ((pos0 - m_pos).len2() >
											    sqr(0.001f))
											{
												gwd[0].offset +=
												    m_pos - pos;
												move -= m_pos - pos;
												pos = m_pos;
												pos0 = m_pos;
												goto retry_without_ground_sync;
											}
										}
										if (iCyl == 0 && bHeavy)
										{
											if (m_pWorld->m_bWorldStep == 2)
											{
												bPushOther =
												    pentlist[i]
													    ->m_iSimClass >
													0 &&
												    pentlist[i]
													    ->GetMassInv() >
													0;
												if (bPushOther)
												{
													nUnproj = min(
													    nUnproj + 1,
													    sizeof(
														unproj) /
														sizeof(
														    unproj
															[0]));
													unproj[nUnproj -
													       1]
													    .pent =
													    pentlist[i];
													unproj[nUnproj -
													       1]
													    .ipart = j;
													unproj[nUnproj -
													       1]
													    .pt =
													    pcontacts
														[ncont]
														    .pt;
													unproj[nUnproj -
													       1]
													    .n =
													    pcontacts
														[ncont]
														    .n;
													unproj[nUnproj -
													       1]
													    .center =
													    gwd[0]
														.offset;
													unproj[nUnproj -
													       1]
													    .penetration =
													    pcontacts
														[ncont]
														    .t;
												}
												else if (
												    !((pentlist[i]
												           ->m_flags |
												       ~bVeryHeavy) &
												      pef_cannot_squash_players))
												{
													m_bSquashed = min(
													    5,
													    m_bSquashed +
														(5 *
													         isneg(
														     ncontactSum *
															 pcontacts
															     [ncont]
																 .n +
														     0.99f)));
													if (pentlist[i]
													        ->m_iSimClass >
													    0)
													{
														ncontactSum =
														    pcontacts
															[ncont]
															    .n;
													}
												}

												for (icnt = 0;
												     icnt <
												     nUnproj -
												         bPushOther;
												     icnt++)
												{
													if (unproj[icnt]
													            .n *
													        pcontacts
													            [ncont]
													                .n <
													    0)
													{
														RegisterUnprojContact(
														    unproj
															[icnt]);
														if (bPushOther)
														{
															RegisterUnprojContact(
															    unproj
																[nUnproj -
															         1]);
														}
														if (!((pentlist[i]
														           ->m_flags |
														       unproj[icnt]
														           .pent
														           ->m_flags |
														       ~bVeryHeavy) &
														      pef_cannot_squash_players))
														{
															m_bSquashed = min(
															    5,
															    m_bSquashed +
																(5 *
															         isneg(max(
																     (5 *
															              pentlist[i]
															                  ->GetMassInv()) -
																	 m_massinv,
																     unproj[icnt].n *
																	     pcontacts
																		 [ncont]
																		     .n +
																	 0.99f))));
														}
													}
												}
											}
											else if (
											    !((pentlist[i]->m_flags |
											       ~bVeryHeavy) &
											      pef_cannot_squash_players))
											{
												m_bSquashed = min(
												    5,
												    m_bSquashed +
													(5 *
												         isneg(
													     ncontactSum *
														 pcontacts
														     [ncont]
															 .n +
													     0.99f)));
												if (pentlist[i]
												        ->m_iSimClass >
												    0)
												{
													ncontactSum =
													    pcontacts
														[ncont]
														    .n;
												}
											}

											if (m_flags &
											    pef_pushable_by_players)
											{
												(pentlist[i]->GetMassInv() ==
												         0
												     ? bStaticUnproj
												     : bDynUnproj)++;
												Vec3 offs =
												    pcontacts[ncont]
													.dir *
												    (pcontacts[ncont]
												         .t +
												     m_pWorld->m_vars
												         .maxContactGapPlayer);
												pos += offs;
												gwd[0].offset += offs;
												bUnprojected = 1;
												if (pcontacts[ncont].t >
												    m_size.x)
												{
													ip
													    .ptOutsidePivot
														[0]
													    .Set(1E11f,
													         1E11f,
													         1E11f);
												}
											}
										}

										if (pentlist[i]->m_iSimClass == 2)
										{
											K.SetZero();
											if (m_flags &
											    pef_pushable_by_players)
											{
												GetContactMatrix(
												    pcontacts[ncont]
													.center,
												    0, K);
											}
											bPushOther =
											    (m_flags &
											     lef_push_objects) &&
											    (pentlist[i]->m_flags &
											     pef_pushable_by_players);
											bPushOther |= iszero(
											    pentlist[i]->m_iSimClass -
											    2);
											bPushOther &=
											    iszero(
												pentlist[i] -
												m_pLastGroundCollider) ^
											    1;
											if (bPushOther)
											{
												pentlist[i]
												    ->GetContactMatrix(
													pcontacts[ncont]
													    .center,
													j, K);
											}
											else if (
											    !(m_flags &
											      pef_pushable_by_players))
											{
												continue;
											}
											pcontacts[ncont].center -=
											    pcontacts[ncont].dir *
											    pcontacts[ncont].t;
											ncontact = -pcontacts[ncont].n;
											if (fabs_tpl(ncontact.z) < 0.5f)
											{
												ncontact.z = 0;
												ncontact.normalize();
											}
											RigidBody* pbody =
											    pentlist[i]->GetRigidBody(
												j);
											vrel =
											    ncontact *
											    (pbody->v +
											     (pbody->w ^
											      pcontacts[ncont].center -
											          pbody->pos) -
											     vel - m_velGround);
											if (iCyl == 0 ||
											    fabs_tpl(vrel) *
											            time_interval >
											        m_size.x * 0.2f)
											{
												vrel = max(
												    0.0f,
												    vrel -
													ncontact *
													    (vel +
												             m_velGround));
												ai.impulse =
												    ncontact *
												    (imp =
												         vrel /
												         max(1e-6f,
												             ncontact *
												                 K *
												                 ncontact));
												ai.point =
												    pcontacts[ncont]
													.center;
												ai.ipart = 0;
												if (ai.impulse.len2() *
												        sqr(m_massinv) >
												    max(1.0f,
												        sqr(vrel)))
												{
													ai.impulse
													    .normalize() *=
													    fabs_tpl(
														vrel) *
													    m_mass *
													    0.5f;
												}
												if (m_flags &
												    pef_pushable_by_players)
												{
													vel +=
													    ai.impulse *
													    m_massinv;
												}
												bFlying = 1;
												m_minFlyTime = 1.0f;
												m_timeFlying = 0;
												if (m_kInertia == 0)
												{
													m_timeForceInertia =
													    m_timeImpulseRecover;
												}
												if (bPushOther)
												{
													if (fabs_tpl(
														pcontacts
														    [ncont]
															.dir *
														axis) <
													    0.7f)
													{
														ai.impulse -=
														    axis *
														    (axis *
														     ai.impulse);
													}
													ai.impulse
													    .Flip();
													ai.ipart = j;
													if (pentlist[i]
													        ->GetType() <
													    PE_LIVING)
													{
														ai.impulse *=
														    0.2f;
													}
													pentlist[i]
													    ->Action(
														&ai, 1);
													m_timeSinceImpulseContact =
													    0;
												}
												idmat =
												    (pentlist[i]
												         ->m_parts[j]
												         .surface_idx &
												     pcontacts[ncont]
												             .id[1] >>
												         31) |
												    max(pcontacts[ncont]
												            .id[1],
												        0);
												RegisterContact(
												    pos,
												    pcontacts[ncont].pt,
												    ncontact,
												    pentlist[i], j,
												    idmat, imp);
											}
										}
									}
								}
							}
						}
					}
				}
			}
			if (bStaticUnproj && bDynUnproj && m_bSquashed)
			{
				pos = pos0;
				bStaticUnproj = bDynUnproj = 0;
			}

			if (bStaticUnproj + bDynUnproj > 0)
			{
				newpos = pos + move;
				h = ShootRayDown(pentlist, nents, newpos, nslope);
			}

			hcur = newpos * axis - m_hPivot;
			if (axis * nslope > m_slopeFall &&
			    (hcur < h && hcur > h - ((m_hCyl - m_size.z) * 1.01f) && !bFlying ||
			     hcur > h && sqr_signed(hcur - h) < vel.len2() * sqr(time_interval) && !bFlying &&
			         !m_bJumpRequested && !m_bSwimming))
			{
				if (h > hcur && m_nslope * axis < m_slopeSlide && m_nslope * nslope < m_slopeSlide &&
				    nslope * axis < m_slopeSlide && m_velRequested.len2() == 0)
				{
					newpos = pos;
					vel.zero();
				}
				else
				{
					newpos += axis * (h + m_hPivot - newpos * axis);
				}
				move = newpos - pos;
				movelen = move.len();
			}
			pos0 = pos;
			if (m_bJumpRequested)
			{
				AddLegsImpulse(-vel, m_nslope, true);
			}
			m_bJumpRequested = 0;
			m_bStuck = 0;
			ncontactSum.zero();

			if (movelen > m_size.x * 1E-4f && m_parts[0].flagsCollider & geom_colltype_player)
			{
				ip.bSweepTest = true;
				gwd[0].v = move / movelen;
				iter = 0;
				move0 = movelen;
				movesum = 0;
				m_bUseSphere = 0;

				do
				{
					gwd[0].offset = pos + gwd[0].R * m_parts[0].pos;
					ip.time_interval = movelen + m_pWorld->m_vars.maxContactGapPlayer;
					tmin = ip.time_interval * 2;
					imin = -1;
					for (i = 0; i < nents; i++)
					{
						if (pentlist[i]->GetType() != PE_LIVING ||
						    pentlist[i]->m_nParts -
						            iszero((int)pentlist[i]
						                       ->m_parts[pentlist[i]->m_nParts - 1]
						                       .flags &
						                   geom_colltype_player) >
						        1 ||
						    max(sqr(m_qrot.v.x) + sqr(m_qrot.v.y),
						        sqr(pentlist[i]->m_qrot.v.x) + sqr(pentlist[i]->m_qrot.v.y)) >
						        0.001f)
						{
							for (j = 0, bCheckBBox = (pentlist[i]->m_BBox[1] -
							                          pentlist[i]->m_BBox[0])
							                             .len2() > 0;
							     j < pentlist[i]->m_nParts; j++)
							{
								if (pentlist[i]->m_parts[j].flags &
								        (geom_colltype_player |
								         geom_log_interactions) &&
								    (!bCheckBBox ||
								     AABB_overlap(BBoxInner,
								                  pentlist[i]->m_parts[j].BBox)))
								{
									if (pentlist[i]->m_parts[j].flags &
									    geom_log_interactions)
									{
										event.pEntity[1] = pentlist[i];
										event.pForeignData[1] =
										    pentlist[i]->m_pForeignData;
										event.iForeignData[1] =
										    pentlist[i]->m_iForeignData;
										m_pWorld->OnEvent(m_flags, &event);
										if (!(pentlist[i]->m_parts[j].flags &
										      geom_colltype_player))
										{
											continue;
										}
									}
									gwd[1].R = Matrix33(pentlist[i]->m_qrot *
									                    pentlist[i]->m_parts[j].q);
									gwd[1].offset = pentlist[i]->m_pos +
									                pentlist[i]->m_qrot *
									                    pentlist[i]->m_parts[j].pos;
									gwd[1].scale = pentlist[i]->m_parts[j].scale;
									if (m_bUseSphere &&
									    (ncont = m_SphereGeom.Intersect(
										 pentlist[i]
										     ->m_parts[j]
										     .pPhysGeomProxy->pGeom,
										 gwd, gwd + 1, &ip, pcontacts)))
									{
										WriteLockCond lockColl(*ip.plock, 0);
										lockColl.SetActive();
										if (pcontacts[ncont - 1].t < tmin &&
										    pcontacts[ncont - 1].n * gwd[0].v >
										        0)
										{
											tmin = pcontacts[ncont - 1].t;
											ncontact =
											    -pcontacts[ncont - 1].n;
											ptcontact =
											    pcontacts[ncont - 1].pt;
											imin = i;
											jmin = j;
											idmat = (pentlist[i]
											             ->m_parts[j]
											             .surface_idx &
											         pcontacts[ncont - 1]
											                 .id[1] >>
											             31) |
											        max(pcontacts[ncont - 1]
											                .id[1],
											            0);
										}
									}
									if ((ncont = m_pCylinderGeom->Intersect(
										 pentlist[i]
										     ->m_parts[j]
										     .pPhysGeomProxy->pGeom,
										 gwd, gwd + 1, &ip, pcontacts)))
									{
										WriteLockCond lockColl(*ip.plock, 0);
										lockColl.SetActive();
										if (pcontacts[ncont - 1].t < tmin &&
										    pcontacts[ncont - 1].n * gwd[0].v >
										        0 &&
										    (!m_bUseSphere ||
										     m_timeUseLowCap > 0 ||
										     pcontacts[ncont - 1].n.z > -0.5f ||
										     pcontacts[ncont - 1].iFeature[0] !=
										             0x20 &&
										         pcontacts[ncont - 1]
										                 .iFeature[0] != 0x41))
										{
											tmin = pcontacts[ncont - 1].t;
											ncontact =
											    -pcontacts[ncont - 1].n;
											ptcontact =
											    pcontacts[ncont - 1].pt;
											imin = i;
											jmin = j;
											idmat = (pentlist[i]
											             ->m_parts[j]
											             .surface_idx &
											         pcontacts[ncont - 1]
											                 .id[1] >>
											             31) |
											        max(pcontacts[ncont - 1]
											                .id[1],
											            0);
										}
									}
								}
							}
						}
						else
						{
							pentlist[i]->GetParams(&pd);
							vector2df dorigin, ncoll, move2d = (vector2df)gwd[0].v;
							dorigin = vector2df(newpos - pentlist[i]->m_pos);
							float kb = dorigin * move2d,
							      kc = len2(dorigin) - sqr(m_size.x + pd.sizeCollider.x),
							      kd = (kb * kb) - kc, zup0, zdown0, zup1, zdown1;
							if (kd >= 0)
							{
								zup0 = (zdown0 = pos.z - m_hPivot) + m_hCyl + m_size.z;
								zdown0 = max(
								    zdown0,
								    zup0 - ((m_size.x * m_bUseCapsule + m_size.z) * 2));
								zup1 =
								    (zdown1 = pentlist[i]->m_pos.z - pd.heightPivot) +
								    pd.heightCollider + pd.sizeCollider.z;
								zdown1 =
								    max(zdown1,
								        zup1 - ((pd.sizeCollider.x * pd.bUseCapsule +
								                 pd.sizeCollider.z) *
								                2));
								kd = sqrt_tpl(kd);
								tfirst = -kb + kd;
								ncoll = vector2df(pos + gwd[0].v * tfirst -
								                  pentlist[i]->m_pos);
								if (tfirst > -m_size.x && tfirst < tmin &&
								    ncoll * move2d >= 0 &&
								    min(zup0 + (gwd[0].v.z * tfirst), zup1) <
								        max(zdown0 + (gwd[0].v.z * tfirst), zdown1))
								{
									continue; // if entities separate during this
									          // timestep, ignore other collisions
								}
								tfirst = -kb - kd;
								ncoll = vector2df(pos + gwd[0].v * tfirst -
								                  pentlist[i]->m_pos);
								if (tfirst < -m_size.x ||
								    min(zup0 + (gwd[0].v.z * tfirst), zup1) <
								        max(zdown0 + (gwd[0].v.z * tfirst), zdown1) ||
								    ncoll * move2d >= 0)
								{
									tfirst = -kb + kd;
									ncoll = vector2df(pos + gwd[0].v * tfirst -
									                  pentlist[i]->m_pos);
								}
								if (tfirst > -m_size.x && tfirst < tmin &&
								    ncoll * move2d < 0 &&
								    min(zup0 + (gwd[0].v.z * tfirst), zup1) >
								        max(zdown0 + (gwd[0].v.z * tfirst), zdown1))
								{
									tmin = tfirst;
									ncontact.Set(ncoll.x, ncoll.y, 0).normalize();
									ptcontact = pos + gwd[0].v * tfirst -
									            ncontact * m_size.x;
									imin = i;
									jmin = 0;
									idmat = m_surface_idx;
								}
								// also check for cap-cap contact
								if (fabs_tpl(gwd[0].v.z) > m_size.z * 1E-5f)
								{
									j = sgnnz(gwd[0].v.z);
									zup0 =
									    pos.z - m_hPivot + m_hCyl + (m_size.z * j);
									zdown1 = pentlist[i]->m_pos.z - pd.heightPivot +
									         pd.heightCollider -
									         (pd.sizeCollider.z * j);
									tfirst = zdown1 - zup0;
									if (inrange(tfirst, -m_size.x * gwd[0].v.z,
									            tmin * gwd[0].v.z) &&
									    len2(dorigin * gwd[0].v.z +
									         move2d * tfirst) <
									        sqr((m_size.x + pd.sizeCollider.x) *
									            gwd[0].v.z))
									{
										tmin = tfirst / gwd[0].v.z;
										ncontact.Set(0, 0, -j);
										(ptcontact = pos + gwd[0].v * tfirst)
										    .z += m_size.z * j;
										ptcontact.x +=
										    (pentlist[i]->m_pos.x - pos.x) *
										    0.1f;
										ptcontact.y +=
										    (pentlist[i]->m_pos.y - pos.y) *
										    0.1f;
										imin = i;
										jmin = -1;
										idmat = m_surface_idx;
									}
								}
							}
						}
					}

					if (tmin <= ip.time_interval)
					{
						tmin = max(0.0f, tmin - m_pWorld->m_vars.maxContactGapPlayer);
						pos += gwd[0].v * tmin;
						static const float g_cosSlide = cos_tpl(0.3f),
								   g_sinSlide = sin_tpl(0.3f);
						if (bFlying)
						{
							if ((ncontact * axis) * (1 - m_bSwimming) > g_cosSlide)
							{
								ncontact = axis * g_cosSlide +
								           (pos - ptcontact -
								            axis * (axis * (pos - ptcontact)))
								                   .normalized() *
								               g_sinSlide;
							}
						}
						else if (inrange(ncontact * axis, 0.85f, -0.1f) &&
						         (unsigned int)pentlist[imin]->m_iSimClass - 1u < 2u &&
						         pentlist[imin]->GetMassInv() > m_massinv * 0.25f)
						{
							ncontact.z = 0, ncontact.normalize();
						}
						bPush = pentlist[imin]->m_iSimClass > 0 ||
						        isneg(min(min(m_slopeClimb - ncontact * axis,
						                      ncontact * axis + m_slopeFall),
						                  vel * axis + 1.0f));
						int bUnmovable =
						    isneg(-pentlist[imin]->m_iSimClass >> 31 &
						          ~(-((int)m_flags & pef_pushable_by_players) >> 31));
						bPush &= bUnmovable ^ 1;
						{
							K.SetZero();
							bPushOther =
							    (m_flags &
							     (pentlist[imin]->m_iSimClass == 3 ? lef_push_players
							                                       : lef_push_objects)) &&
							    (pentlist[imin]->m_flags & pef_pushable_by_players) &&
							    (pentlist[imin]->m_iSimClass |
							     m_pWorld->m_vars.bPlayersCanBreak |
							     pentlist[imin]->m_flags & pef_players_can_break);
							bPushOther &=
							    iszero(pentlist[imin] - m_pLastGroundCollider) ^ 1;
							bPushOther |= bUnmovable;
							if (bPushOther)
							{
								pentlist[imin]->GetContactMatrix(ptcontact, jmin, K);
							}
							if (!bPushOther || m_flags & pef_pushable_by_players)
							{
								GetContactMatrix(ptcontact, -1, K);
							}
							else
							{
								bPush = 0;
							}
							vrel = min(0.0f, ncontact * (vel + m_velGround));
							ai.impulse = ncontact;
							if (pentlist[imin]->m_iSimClass == 3)
							{
								vrel -=
								    ncontact * ((CLivingEntity*)pentlist[imin])->m_vel;
								if (ncontact * axis > 0.95f)
								{
									ai.impulse +=
									    (pos - pentlist[imin]->m_pos -
									     axis *
									         (axis * (pos - pentlist[imin]->m_pos)))
										.normalized();
									if (inrange(vrel, -1.0f, 1.0f))
									{
										vrel = -1.0f;
									}
								}
							}
							else
							{
								RigidBody* pbody = pentlist[imin]->GetRigidBody(jmin);
								vrel -=
								    ncontact *
								    (pbody->v + (pbody->w ^ ptcontact - pbody->pos));
							}
							imp = -vrel * 1.01f / (ncontact * K * ncontact);
							jmin -= jmin >> 31;
							if (bPush || m_flags & lef_report_sliding_contacts ||
							    pentlist[imin]->m_parts[jmin].flags &
							        geom_manually_breakable)
							{
								RegisterContact(pos, ptcontact, ncontact,
								                pentlist[imin], jmin, idmat,
								                imp * bPush);
							}
							ai.impulse *= imp;
							ai.point = ptcontact;
							ai.ipart = 0;
							if (ai.impulse.len2() * sqr(m_massinv) > max(1.0f, sqr(vrel)))
							{
								ai.impulse.normalize() *=
								    fabs_tpl(vrel) * m_mass * 0.5f;
							}
							if (bPush)
							{
								vel += ai.impulse * m_massinv;
								if (m_kInertia == 0 &&
								    (pentlist[imin]->m_iSimClass - 3 |
								     m_flags & pef_pushable_by_players))
								{
									m_timeForceInertia = m_timeImpulseRecover;
								}
							}
							if (bPushOther)
							{
								ai.impulse.Flip();
								ai.ipart = jmin;
								if (fabs_tpl(ncontact * axis) < 0.7f)
								{
									ai.impulse -= axis * (axis * ai.impulse);
								}
								ai.iApplyTime = isneg(pentlist[imin]->m_iSimClass - 3)
								                << 1;
								if (pentlist[imin]->GetType() < PE_LIVING)
								{
									ai.impulse *= 0.2f;
								}
								pentlist[imin]->Action(&ai);
								if (pentlist[imin]->m_iSimClass < 3)
								{
									m_timeSinceImpulseContact = 0;
								}
							}
						}
						movelen -= tmin;
						movesum += tmin;
						for (i = 0; i < iter && ncontactHist[i] * ncontact < 0.95f; i++)
							;
						if (i == iter)
						{
							ncontactSum += ncontact;
						}
						ncontactHist[iter] = ncontact;
						if (iter == 1 && movesum == 0.0f)
						{
							ncontact = (ncontactHist[0] ^ ncontactHist[1]).normalized();
							gwd[0].v = ncontact * (gwd[0].v * ncontact);
						}
						else
						{
							gwd[0].v -= ncontact * ((gwd[0].v * ncontact) - 0.002f);
						}
						tmin = gwd[0].v.len();
						movelen *= tmin;
						if (tmin > 0)
						{
							gwd[0].v /= tmin;
						}
						if (gwd[0].v * move < 0)
						{
							movelen = 0;
						}
					}
					else
					{
						pos += gwd[0].v * movelen;
						movesum += movelen;
						break;
					}
				}
				while (movelen > m_pWorld->m_vars.maxContactGapPlayer * 0.1f && ++iter < 3);
				if (movesum < move0 * 0.001f &&
				    (sqr_signed(ncontactSum.z) > sqr(0.4f) * ncontactSum.len2() ||
				     ncontactSum.len2() < 0.6f))
				{
					m_bStuck = 1;
				}

				if (m_parts[0].flagsCollider & geom_colltype_player &&
				    (bUnprojected || !(m_flags & lef_loosen_stuck_checks)))
				{
					ip.bSweepTest = false;
					gwd[0].offset = pos + gwd[0].R * m_parts[0].pos;
					gwd[0].v = -axis;
					ip.bStopAtFirstTri = true;
					ip.bNoBorder = true;
					ip.time_interval = m_size.z * 10;
					for (i = 0; i < nents; i++)
					{
						if (pentlist[i]->m_iSimClass == 0)
						{
							for (j = 0; j < pentlist[i]->m_nParts; j++)
							{
								if (pentlist[i]->m_parts[j].flags &
								    geom_colltype_player)
								{
									gwd[1].R = Matrix33(pentlist[i]->m_qrot *
									                    pentlist[i]->m_parts[j].q);
									gwd[1].offset = pentlist[i]->m_pos +
									                pentlist[i]->m_qrot *
									                    pentlist[i]->m_parts[j].pos;
									gwd[1].scale = pentlist[i]->m_parts[j].scale;
									if (m_pCylinderGeom->Intersect(
										pentlist[i]
										    ->m_parts[j]
										    .pPhysGeomProxy->pGeom,
										gwd, gwd + 1, &ip, pcontacts))
									{
										{
											WriteLockCond lockColl(
											    *ip.plock, 0);
											lockColl.SetActive();
											if (pcontacts->t >
											    m_pWorld->m_vars
											        .maxContactGapPlayer)
											{
												vel.zero(),
												    m_bStuck = 1;
											}
										}
										pos = pos0;
										m_timeUseLowCap = 1.0f;
										goto nomove;
									}
								}
							}
						}
					}
				nomove:;
				}
			}
			else
			{
				pos += move;
			}

			if (!m_pLastGroundCollider)
			{
				velGround.zero();
			}
			else
			{
				velGround = m_velGround;
			}
			m_hLatest = h = ShootRayDown(pentlist, nents, pos, nslope, time_interval, false, true);
			if (nslope * axis < 0.087f)
			{
				nslope = m_nslope;
			}
			else
			{
				WriteLockCond lock(m_lockLiving, m_bStateReading ^ 1);
				m_nslope = nslope;
			}
			if (m_pLastGroundCollider)
			{
				RegisterContact(newpos, newpos, m_qrot * Vec3(0, 0, 1), m_pLastGroundCollider,
				                m_iLastGroundColliderPart, m_lastGroundSurfaceIdx, 0, 1);
				if (m_pLastGroundCollider->m_iSimClass == 0)
				{
					ReleaseGroundCollider();
				}
			}

			if (bFlying)
			{
				m_timeFlying += time_interval;
			}
			int bGroundContact = isneg(max(pos * axis - m_hPivot - (h + max(0.004f, m_size.z * 0.01f)),
			                               m_slopeFall - nslope * axis));
			if (!bGroundContact)
			{
				ReleaseGroundCollider();
			}
			bFlying = m_pWorld->m_vars.bFlyMode || m_gravity * axis > 0 || m_bSwimming ||
			          ((bGroundContact | m_bStuck) ^ 1);
			m_bActiveEnvironment = m_bStuck;

			Vec3 last_force(ZERO);
			if (bFlying)
			{
				if (!bWasFlying)
				{
					vel += velGround;
				}
				if (!m_pWorld->m_vars.bFlyMode)
				{
					last_force = m_gravity;
					if (m_bSwimming)
					{
						last_force += (m_velRequested - vel) * kInertia;
					}
					if ((vel + last_force * time_interval) * vel < 0)
					{
						vel.zero();
					}
					else
					{
						vel += last_force * time_interval;
					}
					last_force = -vel * m_kAirResistance * time_interval;
					if (last_force.len2() < vel.len2() * 4.0f)
					{
						vel += last_force;
					}
				}
				if (vel.len2() < 1E-10f)
				{
					vel.zero();
				}
				ReleaseGroundCollider();
			}
			else
			{
				if (bWasFlying && !m_bSwimming)
				{
					if (vel * axis < -4.0f && !m_bStateReading)
					{
						m_dhSpeed = vel * axis;
						m_dhAcc = max(sqr(m_dhSpeed) / (m_size.z * 2), m_nodSpeed);
					}
					vel -= m_velGround;
					if (m_nslope * axis < m_slopeFall && (vel * axis < -4) && !m_bStuck)
					{
						vel -= m_nslope * (vel * m_nslope) * 1.0f;
						if (vel * axis > 0)
						{
							vel -= axis * (axis * vel);
						}
						bFlying = 1;
					}
					else if (m_nslope * axis < m_slopeFall ||
					         (!bGroundContact || m_velRequested.len2() > 0) &&
					             (m_nslope * axis > m_slopeClimb || vel * axis < 0))
					{
						vel -= m_nslope * (vel * m_nslope);
					}
					else
					{
						vel.zero();
					}
				}
				Vec3 velReq = m_velRequested, g;
				if (!m_bSwimming)
				{
					velReq -= m_nslope * (velReq * m_nslope);
				}
				last_force = (velReq - vel) * kInertia;
				if (m_nslope * axis < m_slopeSlide && !m_bSwimming)
				{
					g = m_gravity;
					last_force += g - m_nslope * (g * m_nslope);
				}
				if ((vel + last_force * time_interval) * vel < 0 &&
				    (vel + last_force * time_interval) * m_velRequested <= 0)
				{
					vel.zero();
				}
				else
				{
					vel += last_force * time_interval;
				}
				if (m_nslope * axis < m_slopeClimb)
				{
					if (vel * axis > 0 && last_force * axis > 0)
					{
						vel -= axis * (axis * vel);
					}
					if ((pos - pos0) * axis > m_size.z * 0.001f)
					{
						vel -= axis * (axis * vel);
					}
				}
				if (m_nslope * axis < m_slopeFall && !m_bStuck)
				{
					bFlying = 1;
					m_minFlyTime = 0.5f;
					vel += m_nslope - axis * (axis * m_nslope);
				}
				if (m_velRequested.len2() == 0 && vel.len2() < 0.001f || vel.len2() < 0.0001f)
				{
					vel.zero();
				}
			}

			if (!bFlying)
			{
				m_timeFlying = m_minFlyTime = 0;
			}
			else
			{
				m_timeFlying = physics_float2int(min(m_timeFlying, 9.9f) * 6553.6f) * (10.0f / 65536);
			}
			if (m_flags & lef_snap_velocities)
			{
				vel = DecodeVec6b(EncodeVec6b(vel));
			}

			if (!m_bStateReading)
			{
				if (!bFlying && (dh = (pos - pos0) * axis) > m_size.z * 0.01f)
				{
					m_dhSpeed = max(m_dhSpeed, dh / m_stablehTime);
					m_dh += dh;
					if (m_dh > m_size.z)
					{
						m_dh = m_size.z;
					}
					m_stablehTime = 0;
					if (dh > (m_dhHist[0] + m_dhHist[1]) * 3)
					{
						m_timeOnStairs = 0.5f;
					}
				}
				m_dhHist[0] = m_dhHist[1];
				m_dhHist[1] = dh;
				m_stablehTime = min(m_stablehTime + time_interval, 0.5f);
				m_timeOnStairs = max(m_timeOnStairs - time_interval, 0.0f);

				m_dhSpeed += m_dhAcc * time_interval;
				if (m_dhAcc == 0 && m_dh * m_dhSpeed < 0 || m_dh * m_dhAcc < 0 ||
				    m_dh * (m_dh - m_dhSpeed * time_interval) < 0)
				{
					m_dh = m_dhSpeed = m_dhAcc = 0;
				}
				else
				{
					m_dh -= m_dhSpeed * time_interval;
				}
			}

			if (m_HeadGeom.m_sphere.r > 0)
			{
				ip.bSweepTest = true;
				gwd[0].offset = pos + gwd[0].R * m_parts[0].pos;
				gwd[0].v = axis;
				tmin = ip.time_interval = m_hHead - m_hCyl - min(m_dh, 0.0f);
				for (i = 0; i < nents; i++)
				{
					if (pentlist[i]->m_iSimClass == 0)
					{
						for (j = 0; j < pentlist[i]->m_nParts; j++)
						{
							if (pentlist[i]->m_parts[j].flags & geom_colltype_player)
							{
								gwd[1].R = Matrix33(pentlist[i]->m_qrot *
								                    pentlist[i]->m_parts[j].q);
								gwd[1].offset =
								    pentlist[i]->m_pos +
								    pentlist[i]->m_qrot * pentlist[i]->m_parts[j].pos;
								gwd[1].scale = pentlist[i]->m_parts[j].scale;
								if (m_HeadGeom.Intersect(
									pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom,
									gwd, gwd + 1, &ip, pcontacts))
								{
									WriteLockCond lockColl(*ip.plock, 0);
									lockColl.SetActive();
									tmin = min(tmin, (float)pcontacts[0].t);
								}
							}
						}
					}
				}
				if (m_dh < ip.time_interval + min(m_dh, 0.0f) - tmin ||
				    fabs_tpl(m_dhSpeed) + fabs_tpl(m_dhAcc) == 0)
				{
					m_dh = ip.time_interval + min(m_dh, 0.0f) - tmin;
				}
			}

			m_iSensorsActive = 0;
			for (i = 0; i < m_nSensors; i++)
			{
				Vec3 pt = pos + m_qrot * m_pSensors[i];
				m_pSensorsPoints[i] = pt + axis * (axis * (pos - pt) - m_hPivot);
				h = ShootRayDown(pentlist, nents, pt, m_pSensorsSlopes[i], 0, true, false, false);
				int bActive = isneg(axis * pt - h);
				if (bActive)
				{
					m_pSensorsPoints[i] += axis * (h - axis * m_pSensorsPoints[i]);
				}
				m_iSensorsActive |= bActive << i;
				m_pSensorsPoints[i] = (m_pSensorsPoints[i] - pos) * m_qrot;
				m_pSensorsSlopes[i] = m_pSensorsSlopes[i] * m_qrot;
			}
		}

		ComputeBBoxLE(pos, BBoxInner, &partCoord);
		UpdatePosition(pos, BBoxInner, m_pWorld->RepositionEntity(this, 1, BBoxInner));
		bMoving = 1;
	}
	else if (!m_bActive)
	{
		if (m_velRequested.len2() > 0)
		{
			m_pos += m_velRequested * time_interval;
			m_BBox[0] += m_velRequested * time_interval;
			m_BBox[1] += m_velRequested * time_interval;
			AtomicAdd(&m_pWorld->m_lockGrid, -m_pWorld->RepositionEntity(this, 1));
			bMoving = 1;
		}
		ReleaseGroundCollider();
		m_iSensorsActive = 0;
	}

	{
		WriteLockCond lock(m_lockLiving, m_bStateReading ^ 1);
		m_vel = vel + m_vel - vel0;
		m_bFlying = bFlying;
		m_iHist = (m_iHist + 1) & (m_szHistory - 1);
		m_history[m_iHist].dt = time_interval;
		m_history[m_iHist].v = m_vel;
		m_history[m_iHist].pos = m_pos;
		m_history[m_iHist].q = m_qrot;
		m_history[m_iHist].bFlying = m_bFlying;
		m_history[m_iHist].timeFlying = m_timeFlying;
		m_history[m_iHist].minFlyTime = m_minFlyTime;
		m_history[m_iHist].timeUseLowCap = m_timeUseLowCap;
		m_history[m_iHist].nslope = m_nslope;
		m_history[m_iHist].idCollider =
		    m_pLastGroundCollider ? m_pWorld->GetPhysicalEntityId(m_pLastGroundCollider) : -2;
		m_history[m_iHist].iColliderPart = m_iLastGroundColliderPart;
		m_history[m_iHist].posColl = m_posLastGroundColl;

		m_timeSmooth = max(0.0f, m_timeSmooth - time_interval);
		if (m_pWorld->m_bUpdateOnlyFlagged)
		{
			m_deltaPos = m_posLocal - m_pos;
			if (m_deltaPos.len2() < sqr(0.01f) || m_deltaPos.len2() > sqr(2.0f))
			{
				m_deltaPos.zero();
			}
			m_timeSmooth = 0.3f;
		}
		if (m_pBody)
		{
			if (!m_nColliders)
			{
				delete m_pBody;
				m_pBody = 0;
			}
			else
			{
				m_pBody->pos = m_pos + m_qrot * Vec3(0, 0, m_hCyl);
				m_pBody->q = m_qrot;
				m_pBody->P = (m_pBody->v = m_vel) * (m_pBody->M = m_mass);
				m_pBody->Minv = m_massinv;
				m_pBody->w.zero();
				m_pBody->L.zero();
			}
		}
	}

	if (bMoving && !m_bStateReading)
	{
		Vec3 gravity;
		pe_params_buoyancy pb;
		if (m_pWorld->CheckAreas(this, gravity, &pb) && !is_unused(gravity))
		{
			m_gravity = gravity;
		}

		if (m_pWorld->m_pWaterMan)
		{
			m_pWorld->m_pWaterMan->OnWaterInteraction(this);
		}

		if (m_flags & (pef_monitor_poststep | pef_log_poststep))
		{
			EventPhysPostStep epps;
			epps.pEntity = this;
			epps.pForeignData = m_pForeignData;
			epps.iForeignData = m_iForeignData;
			epps.dt = time_interval;
			epps.pos = m_pos;
			epps.q = m_qrot;
			epps.idStep = m_pWorld->m_idStep;
			epps.pos -= m_qrot * Vec3(0, 0, m_dh);
			m_pWorld->OnEvent(m_flags, &epps);
		}
	}

	return 1;
}

int CLivingEntity::StepBackEx(float time_interval, bool bRollbackHistory)
{
	float dt;
	int idx, nSteps;
	for (dt = 0, idx = m_iHist, nSteps = 0; dt + 0.00001f < time_interval && nSteps < m_szHistory;
	     dt += m_history[idx].dt, idx = (idx - 1) & (m_szHistory - 1), nSteps++)
		;
	if (nSteps >= m_szHistory || dt > 1E9)
	{
		return m_iHist;
	}

	float a1 = dt - time_interval, a0 = m_history[(idx + 1) & (m_szHistory - 1)].dt - a1, rdt;
	if (fabsf(a1) < 0.00001f || m_history[(idx + 1) & (m_szHistory - 1)].dt == 0)
	{
		a1 = 0;
		a0 = 1.0f;
		dt = 0;
	}
	else
	{
		rdt = 1.0f / m_history[(idx + 1) & (m_szHistory - 1)].dt;
		a0 *= rdt;
		a1 *= rdt;
		dt -= time_interval;
	}

	m_pos = m_history[idx].pos * a0 + m_history[(idx + 1) & (m_szHistory - 1)].pos * a1;
	m_vel = m_history[idx].v * a0 + m_history[(idx + 1) & (m_szHistory - 1)].v * a1;
	m_bFlying = m_history[idx].bFlying;
	m_nslope = m_history[idx].nslope;
	m_timeFlying = (m_history[idx].timeFlying * a0) + (m_history[(idx + 1) & (m_szHistory - 1)].timeFlying * a1);
	m_minFlyTime = (m_history[idx].minFlyTime * a0) + (m_history[(idx + 1) & (m_szHistory - 1)].minFlyTime * a1);
	m_timeUseLowCap =
	    (m_history[idx].timeUseLowCap * a0) + (m_history[(idx + 1) & (m_szHistory - 1)].timeUseLowCap * a1);
	SetGroundCollider(m_history[idx].idCollider > -2
	                      ? (CPhysicalEntity*)m_pWorld->GetPhysicalEntityById(m_history[idx].idCollider)
	                      : 0);
	m_iLastGroundColliderPart = m_history[idx].iColliderPart;
	if (m_pLastGroundCollider)
	{
		m_idLastGroundColliderPart = m_pLastGroundCollider->m_parts[m_iLastGroundColliderPart].id;
	}
	m_posLastGroundColl = m_history[idx].posColl;

	if (bRollbackHistory)
	{
		m_history[idx].dt += dt;
		m_history[(idx + 1) & (m_szHistory - 1)].dt -= dt;
		m_history[idx].pos = m_pos;
		m_history[idx].v = m_vel;
		// m_history[idx].q = m_qrot;
		m_history[idx].bFlying = m_bFlying;
		m_history[idx].nslope = m_nslope;
		m_history[idx].timeFlying = m_timeFlying;
		m_history[idx].minFlyTime = m_minFlyTime;
		m_history[idx].timeUseLowCap = m_timeUseLowCap;
		m_iHist = idx;
	}
	return idx;
}

float CLivingEntity::CalcEnergy(float time_interval)
{
	float E = 0;
	for (int i = 0; i < m_nContacts; i++)
	{
		E += sqr(min(3.0f, (m_pContacts[i].penetration * 10.0f))) *
		     m_pContacts[i].pent->GetRigidBody(m_pContacts[i].ipart)->M;
	}
	return E;
}

int CLivingEntity::RegisterContacts(float time_interval, int nMaxPlaneContacts)
{
	int i, j;
	Vec3 pt[2], axis = m_qrot * Vec3(0, 0, 1);
	float h;
	entity_contact* pcontact;

	if (m_iSimClass != 7)
	{
		for (i = 0; i < m_nContacts; i++)
		{
			h = (m_pContacts[i].pt - m_pContacts[i].center) * axis;
			pt[0] = pt[1] = m_pContacts[i].pt;
			if (fabs_tpl(m_pContacts[i].n * axis) > 0.7f && fabs_tpl(h) > m_size.z * 0.9f)
			{
				pt[1] += (m_pContacts[i].center - pt[0]) * 2;
				pt[1].z = pt[0].z;
			}
			else
			{
				pt[0] -= axis * (h - m_size.z);
				pt[1] -= axis * (h + m_size.z);
			}

			for (j = 0; j < 2; j++)
			{
				if (!(pcontact = (entity_contact*)AllocSolverTmpBuf(sizeof(entity_contact))))
				{
					return 0;
				}
				pcontact->flags = 0;
				pcontact->pent[0] = m_pContacts[i].pent;
				pcontact->ipart[0] = m_pContacts[i].ipart;
				pcontact->pbody[0] = m_pContacts[i].pent->GetRigidBody(m_pContacts[i].ipart);
				pcontact->pent[1] = this;
				pcontact->ipart[1] = 0;
				pcontact->pbody[1] = GetRigidBody();
				pcontact->friction = 0.2f;
				pcontact->vreq = m_pContacts[i].n * min(3.0f, (m_pContacts[i].penetration * 10.0f));
				pcontact->pt[0] = pcontact->pt[1] = pt[j];
				pcontact->n = m_pContacts[i].n;
				pcontact->K.SetZero();
				m_pContacts[i].pent->GetContactMatrix(m_pContacts[i].pt, m_pContacts[i].ipart,
				                                      pcontact->K);
				::RegisterContact(pcontact);
				m_pContacts[i].pSolverContact[j] = pcontact;
			}
			g_bUsePreCG = 0;
		}
	}

	if (m_pBody && !m_bFlying && (pcontact = (entity_contact*)AllocSolverTmpBuf(sizeof(entity_contact))))
	{
		pcontact->pent[0] = this;
		pcontact->ipart[0] = -1;
		pcontact->pbody[0] = m_pBody;
		if (m_pLastGroundCollider)
		{
			pcontact->pent[1] = m_pLastGroundCollider;
			pcontact->ipart[1] = m_iLastGroundColliderPart;
		}
		else
		{
			pcontact->pent[1] = &g_StaticPhysicalEntity;
			pcontact->ipart[1] = -1;
		}
		pcontact->pbody[1] = pcontact->pent[1]->GetRigidBody(pcontact->ipart[1]);
		pcontact->friction = 0.3f;
		pcontact->pt[0] = pcontact->pt[1] = m_pos;
		pcontact->n = m_nslope;
		pcontact->K.SetZero();
		pcontact->K(0, 0) = pcontact->K(1, 1) = pcontact->K(2, 2) = m_massinv;
		pcontact->pent[1]->GetContactMatrix(pcontact->pt[1], pcontact->ipart[1], pcontact->K);
		::RegisterContact(pcontact);
		g_bUsePreCG = 0;
	}

	return 1;
}

int CLivingEntity::Update(float time_interval, float damping)
{
	int i, j;
	for (i = 0; i < m_nContacts; i++)
	{
		if (m_pContacts[i].pSolverContact[0])
		{
			RegisterContact(m_pos, m_pContacts[i].pt, m_pContacts[i].n, m_pContacts[i].pent,
			                m_pContacts[i].ipart, m_surface_idx,
			                m_pContacts[i].pSolverContact[0]->Pspare +
			                    m_pContacts[i].pSolverContact[1]->Pspare);
		}
	}
	for (i = j = 0; i < m_nColliders; i++)
	{
		if (!m_pColliders[i]->HasConstraintContactsWith(this))
		{
			m_pColliders[i]->RemoveCollider(this);
			m_pColliders[i]->Release();
		}
		else
		{
			m_pColliders[j++] = m_pColliders[i];
		}
	}
	m_nColliders = j;
	m_nContacts = 0;
	if (m_pBody)
	{
		m_vel = m_pBody->v;
		if (m_pBody->w.len2() > 0)
		{
			m_qrot *= Quat::CreateRotationAA(m_pBody->w.len() * time_interval,
			                                 Vec3(0, 0, sgnnz((m_pBody->w * m_qrot).z)));
			m_qrot.Normalize();
			m_pBody->w.zero();
			m_pBody->L.zero();
		}
	}
	return 1;
}

RigidBody* CLivingEntity::GetRigidBody(int ipart, int bWillModify)
{
	if (!bWillModify)
	{
		return CPhysicalEntity::GetRigidBody(ipart);
	}
	else
	{
		if (!m_pBody)
		{
			m_pBody = new RigidBody;
			m_pBody->Create(m_pos, Vec3(1), m_qrot, m_size.GetVolume() * 8, m_mass, m_qrot, m_pos);
		}
		Vec3 axis = m_qrot * Vec3(0, 0, 1);
		m_pBody->pos = m_pos + axis * m_hCyl;
		m_pBody->q = m_qrot;
		m_pBody->P = (m_pBody->v = m_vel) * (m_pBody->M = m_mass);
		m_pBody->Minv = m_massinv;
		m_pBody->Ibody_inv.zero();
		m_pBody->Iinv.SetZero();
		// m_pBody->Ibody_inv.z = m_massinv/(g_PI*m_size.z*sqr(sqr(m_size.x)));
		// dotproduct_matrix(axis,axis*m_pBody->Ibody_inv.z, m_pBody->Iinv);
		m_pBody->w.zero();
		m_pBody->L.zero();
		if (m_kInertia == 0)
		{
			m_timeForceInertia = m_timeImpulseRecover;
		}
		return m_pBody;
	}
}

RigidBody* CLivingEntity::GetRigidBodyData(RigidBody* pbody, int ipart)
{
	pbody->zero();
	pbody->M = m_mass;
	pbody->Minv = m_massinv;
	pbody->v = m_vel;
	pbody->pos = m_pos;
	return pbody;
}

void CLivingEntity::OnContactResolved(entity_contact* pContact, int iop, int iGroupId)
{
	if (!(pContact->flags & contact_angular))
	{
		Vec3 n;
		if (!(pContact->flags & contact_rope) || iop == 0)
		{
			n = pContact->n;
		}
		else
		{
			n = iop == 1 ? pContact->vreq
			             : ((rope_solver_vtx*)pContact->pBounceCount)[iop - 2].v.normalized();
		}
		m_velRequested -= n * min(0.0f, m_velRequested * n);
		m_vel -= n * min(0.0f, m_vel * n);
	}
}

void CLivingEntity::DrawHelperInformation(IPhysRenderer* pRenderer, int flags)
{
	if (m_bActive || m_nParts > 0 && m_parts[0].flags & geom_colltype_player)
	{
		CPhysicalEntity::DrawHelperInformation(pRenderer, flags);

		geom_world_data gwd;
		gwd.R = Matrix33(m_qrot);
		gwd.offset = m_pos + gwd.R * m_parts[0].pos;
		if (m_bUseSphere)
		{
			m_SphereGeom.DrawWireframe(pRenderer, &gwd, 0, m_iSimClass);
		}
		if (m_HeadGeom.m_sphere.r > 0)
		{
			gwd.offset += gwd.R * Vec3(0, 0, m_hHead - m_hCyl - m_dh);
			m_HeadGeom.DrawWireframe(pRenderer, &gwd, 0, m_iSimClass);
		}
	}
}
