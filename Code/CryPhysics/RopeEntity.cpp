#include "CryCommon/CryNetwork/ISerialize.h"
#include "CryCommon/CrySystem/ISystem.h"

#include "ChangeRequest.h"
#include "ContactSolver.h"
#include "CylinderGeom.h"
#include "OverlapChecker.h"
#include "PhysicalWorld.h"
#include "RayGeom.h"
#include "RigidBody.h"
#include "RopeEntity.h"
#include "TriMesh.h"
#include "Utils.h"

struct rope_vtx
{
	Vec3 pt;
	Vec3 vel;
	Vec3 dir;
	Vec3 ncontact;
	Vec3 vcontact;
	float dP;
	CPhysicalEntity* pContactEnt;
	int iContactPart;
};

struct rope_segment : rope_vtx
{
	//~rope_segment() { if (pContactEnt) pContactEnt->Release(); }
	Vec3 pt0;
	Vec3 vel_ext;
	Vec3 ptdst;

	int bRecheckContact;
	float tcontact;
	float vreq;
	float friction[2];
	int iPrim, iFeature;
	int bRecalcDir;
	int iVtx0;

	float r, P;
	float dv;
	float cosnext, kdP;
};

struct SRopeCheckPart
{
	Vec3 offset;
	Matrix33 R;
	float scale, rscale;
	primitives::box bbox;
	CPhysicalEntity* pent;
	int ipart;
	Vec3 pos0;
	quaternionf q0;
	CGeometry* pGeom;
	int bProcess;
};

CRopeEntity::CRopeEntity(CPhysicalWorld* pWorld) : CPhysicalEntity(pWorld)
{
	m_nSegs = -1;
	m_segs = 0;
	m_length = 0;
	m_pTiedTo[0] = m_pTiedTo[1] = 0;
	m_iTiedPart[0] = m_iTiedPart[1] = 0;
	m_ptTiedLoc[0].zero();
	m_ptTiedLoc[1].zero();
	m_idConstraint = 0;
	m_iConstraintClient = 0;
	if (pWorld)
	{
		m_gravity0 = m_gravity = pWorld->m_vars.gravity;
		m_stiffness = m_pWorld->m_vars.unprojVelScale;
	}
	else
	{
		m_gravity0 = m_gravity = Vec3(0, 0, 0);
		m_stiffness = 10.0f;
	}
	m_bAwake = m_nSlowFrames = 0;
	m_damping = 0.2f;
	m_iSimClass = 4;
	m_Emin = sqr(0.04f);
	m_maxAllowedStep = 0.05f;
	m_mass = 1.0f;
	m_collDist = 0.01f;
	m_flags = 0;
	m_surface_idx = 0;
	m_ig[0].x = m_ig[1].x = m_ig[0].y = m_ig[1].y = -3;
	m_defpart.id = 0;
	m_friction = 0.2f;
	m_stiffnessAnim = 70.0f;
	m_stiffnessDecayAnim = 0.75f;
	m_dampingAnim = 1.5f;
	m_bTargetPoseActive = 0;
	m_wind0 = m_wind1 = m_wind.zero();
	m_windVariance = m_windTimer = 0;
	m_airResistance = 0;
	m_waterResistance = 5.0f;
	m_rdensity = 1.0f / 500;
	m_jointLimit = 0;
	m_posBody[0][0] = m_posBody[0][1] = m_posBody[1][0] = m_posBody[1][1] = Vec3(0, 0, 0);
	m_qBody[0][0] = m_qBody[0][1] = m_qBody[1][0] = m_qBody[1][1] = quaternionf(IDENTITY);
	m_dir0dst.Set(0, 0, 1);
	m_szSensor = 0.05f;
	m_maxForce = 0;
	m_flagsCollider = geom_colltype0 | geom_colltype_player;
	m_vtx = 0;
	m_nVtx = m_nVtxAlloc = 0;
	m_pMesh = 0;
	m_vtxSolver = 0;
	m_vtx1 = 0;
	m_lastMeshOffs.zero();
	m_nMaxSubVtx = 3;
	m_idx = 0;
	m_bHasContacts = 0;
	m_bStrained = 0;
	m_frictionPull = 0;
	m_energy = 0;
	m_nFragments = 0;
	m_pContact = 0;
	m_lastTimeStep = 0;
	m_timeLastActive = 0;
	m_lockVtx = 0;
	MARK_UNUSED m_collBBox[0];
}

CRopeEntity::~CRopeEntity()
{
	if (m_segs)
	{
		delete[] m_segs;
	}
	if (m_vtx)
	{
		delete[] m_vtx;
	}
	if (m_vtx1)
	{
		delete[] m_vtx1;
	}
	if (m_vtxSolver)
	{
		delete[] m_vtxSolver;
	}
	if (m_pMesh)
	{
		m_pMesh->Release();
	}
	if (m_idx)
	{
		delete[] m_idx;
	}
	m_segs = 0;
	m_nSegs = -1;
}

void CRopeEntity::AlertNeighbourhoodND()
{
	pe_params_rope pr;
	pr.pEntTiedTo[0] = pr.pEntTiedTo[1] = 0;
	SetParams(&pr);
	int i;
	if (m_nSegs > 0)
	{
		for (i = 0; i <= m_nSegs; i++)
		{
			if (m_segs[i].pContactEnt)
			{
				if (!(m_flags & rope_subdivide_segs))
				{
					m_segs[i].pContactEnt->Release();
				}
				m_segs[i].pContactEnt = 0;
			}
		}
	}
	for (i = 0; i < m_nColliders; i++)
	{
		m_pColliders[i]->Release();
	}
	m_nColliders = 0;
}

int CRopeEntity::Awake(int bAwake, int iSource)
{
	m_bAwake = bAwake;
	m_nSlowFrames = 0;
	int i;
	if (m_nSegs > 0)
	{
		for (i = 0; i <= m_nSegs; i++)
		{
			if (m_segs[i].pContactEnt && (m_segs[i].pContactEnt->m_iDeletionTime > 0 ||
			                              m_segs[i].iContactPart >= m_segs[i].pContactEnt->m_nParts))
			{
				if (!(m_flags & rope_subdivide_segs))
				{
					m_segs[i].pContactEnt->Release();
				}
				m_segs[i].pContactEnt = 0;
				MARK_UNUSED m_segs[i].ncontact;
			}
		}
		if (m_vtx && m_bStrained)
		{
			for (i = 0; i < m_nVtx; i++)
			{
				if (m_vtx[i].pContactEnt && (m_vtx[i].pContactEnt->m_iDeletionTime > 0 ||
				                             m_vtx[i].iContactPart >= m_vtx[i].pContactEnt->m_nParts))
				{
					m_vtx[i].pContactEnt = 0, MARK_UNUSED m_vtx[i].ncontact;
				}
			}
		}
		for (i = 0; i < 2; i++)
		{
			if (m_pTiedTo[i] && m_pTiedTo[i]->m_iDeletionTime > 0)
			{
				if (m_pTiedTo[i]->GetType() == PE_ROPE)
				{
					m_pTiedTo[i]->RemoveCollider(this);
				}
				m_pTiedTo[i]->Release();
				m_pTiedTo[i] = 0;
			}
		}
		for (i = 0; i < m_nColliders; i++)
		{
			if (m_pColliders[i]->GetType() == PE_ROPE)
			{
				m_pColliders[i]->Awake();
			}
		}
		m_timeIdle = 0;
	}
	return 1;
}

void CRopeEntity::RecalcBBox()
{
	if (m_flags & pef_traceable)
	{
		m_BBox[0] = m_BBox[1] = m_nSegs > 0 ? m_segs[0].pt : m_pos;
		for (int i = 1; i <= m_nSegs; i++)
		{
			m_BBox[0] = min(m_BBox[0], m_segs[i].pt);
			m_BBox[1] = max(m_BBox[1], m_segs[i].pt);
		}
		const float val = m_collDist * 2;
		m_BBox[0] -= Vec3(val, val, val);
		m_BBox[1] += Vec3(val, val, val);
		AtomicAdd(&m_pWorld->m_lockGrid, -m_pWorld->RepositionEntity(this, 1, m_BBox));
	}
}

int CRopeEntity::SetParams(pe_params* _params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this, m_pWorld, _params, bThreadSafe);
	if (req.IsQueued())
	{
		return 1;
	}

	unsigned int flags0 = m_flags;
	Vec3 prevpos = m_pos;
	quaternionf prevq = m_qrot;
	int res = CPhysicalEntity::SetParams(_params, 1);
	if (res)
	{
		if (_params->type == pe_params_pos::type_id)
		{
			WriteLock lock(m_lockUpdate);
			Matrix33 R = Matrix33(m_qrot);
			if ((prevpos - m_pos).len2() > sqr(0.0001f) || (prevq.v - m_qrot.v).len2() > sqr(0.0001f))
			{
				m_nVtx = 0;
			}
			for (int i = 0; i <= m_nSegs; i++)
			{
				m_segs[i].pt = m_segs[i].pt0 = R * (m_segs[i].pt - prevpos) + m_pos;
				m_segs[i].vel = R * m_segs[i].vel;
				m_segs[i].dir = R * m_segs[i].dir;
			}
			m_qrot.SetIdentity();
		}
		if (m_nSegs > 0 && !(m_flags & (rope_collides | rope_collides_with_terrain)) &&
		    (flags0 & (rope_collides | rope_collides_with_terrain)))
		{
			for (int i = 0; i <= m_nSegs; i++)
			{
				m_segs[i].pContactEnt = 0;
			}
		}
		if (m_flags & pef_traceable && !(flags0 & pef_traceable))
		{
			RecalcBBox();
		}
		return res;
	}

	if (_params->type == pe_simulation_params::type_id)
	{
		pe_simulation_params* params = (pe_simulation_params*)_params;
		if (!is_unused(params->gravity))
		{
			m_gravity = m_gravity0 = params->gravity;
		}
		if (!is_unused(params->damping))
		{
			m_damping = params->damping;
		}
		if (!is_unused(params->maxTimeStep))
		{
			m_maxAllowedStep = params->maxTimeStep;
		}
		if (!is_unused(params->minEnergy))
		{
			m_Emin = params->minEnergy;
		}
		return 1;
	}

	if (_params->type == pe_params_rope::type_id)
	{
		pe_params_rope* params = (pe_params_rope*)_params;
		WriteLock lock(m_lockUpdate);
		int i;
		float diff = 0;
		if (!is_unused(params->length))
		{
			m_length = params->length;
		}
		else if (!is_unused(params->nSegments) && !is_unused(params->pPoints) && params->pPoints)
		{
			for (i = 0, m_length = 0; i < params->nSegments; i++)
			{
				m_length += (params->pPoints[i + 1] - params->pPoints[i]).len();
			}
		}
		if (!is_unused(params->mass))
		{
			m_mass = params->mass;
		}
		if (!is_unused(params->collDist))
		{
			m_collDist = params->collDist;
			RecalcBBox();
		}
		if (!is_unused(params->surface_idx))
		{
			m_surface_idx = params->surface_idx;
		}
		if (!is_unused(params->friction))
		{
			m_friction = params->friction;
		}
		if (!is_unused(params->stiffnessAnim))
		{
			diff = fabs_tpl(m_stiffnessAnim - params->stiffnessAnim),
			m_stiffnessAnim = params->stiffnessAnim;
		}
		if (!is_unused(params->stiffnessDecayAnim))
		{
			m_stiffnessDecayAnim = params->stiffnessDecayAnim;
		}
		if (!is_unused(params->dampingAnim))
		{
			m_dampingAnim = params->dampingAnim;
		}
		if (!is_unused(params->bTargetPoseActive))
		{
			diff += fabs_tpl(m_bTargetPoseActive - params->bTargetPoseActive),
			    m_bTargetPoseActive = params->bTargetPoseActive;
		}
		if (!is_unused(params->wind))
		{
			diff += (m_wind0 - params->wind).len2(), m_wind0 = m_wind1 = m_wind = params->wind;
		}
		if (!is_unused(params->windVariance))
		{
			m_windVariance = params->windVariance;
		}
		if (!is_unused(params->airResistance))
		{
			m_airResistance = params->airResistance;
		}
		if (!is_unused(params->waterResistance))
		{
			m_waterResistance = params->waterResistance;
		}
		if (!is_unused(params->density))
		{
			m_rdensity = 1.0f / params->density;
		}
		if (!is_unused(params->jointLimit))
		{
			m_jointLimit = params->jointLimit;
		}
		if (!is_unused(params->sensorRadius))
		{
			m_szSensor = params->sensorRadius;
		}
		if (!is_unused(params->maxForce))
		{
			m_maxForce = params->maxForce;
		}
		if (!is_unused(params->flagsCollider))
		{
			m_flagsCollider = params->flagsCollider;
		}
		if (!is_unused(params->nMaxSubVtx))
		{
			m_nMaxSubVtx = params->nMaxSubVtx;
		}
		if (!is_unused(params->frictionPull))
		{
			m_frictionPull = params->frictionPull;
		}
		if (!is_unused(params->stiffness))
		{
			m_stiffness = params->stiffness;
		}
		if (!is_unused(params->collisionBBox[0]))
		{
			if ((params->collisionBBox[1] - params->collisionBBox[0]).len2() > 0)
			{
				m_collBBox[0] = params->collisionBBox[0];
				m_collBBox[1] = params->collisionBBox[1];
			}
			else
			{
				MARK_UNUSED m_collBBox[0];
			}
		}
		if (!is_unused(params->nSegments))
		{
			diff++;
			Vec3 pt[2] = {m_pos, m_pos - Vec3(0, 0, m_length)};
			for (i = 0; i < 2; i++)
			{
				if (m_pTiedTo[i])
				{
					Vec3 pos;
					quaternionf q;
					float scale;
					m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], pos, q, scale);
					pt[i] = pos + q * m_ptTiedLoc[i];
				}
			}
			if (m_nSegs != params->nSegments)
			{
				if (m_segs)
				{
					delete[] m_segs;
				}
				m_segs = new rope_segment[params->nSegments + 1];
				memset(m_segs, 0, (params->nSegments + 1) * sizeof(rope_segment));
			}
			float rnSegs = 1.0f / params->nSegments;
			if (params->nSegments != m_nSegs || m_pTiedTo[0] && m_pTiedTo[1])
			{
				m_nSegs = params->nSegments;
				for (i = 0; i <= params->nSegments; i++)
				{
					m_segs[i].pt = m_segs[i].pt0 = pt[0] + (pt[1] - pt[0]) * (i * rnSegs);
				}
				if (m_flags & rope_subdivide_segs)
				{
					if (m_vtx)
					{
						delete[] m_vtx;
					}
					if (m_vtx1)
					{
						delete[] m_vtx1;
					}
					memset(m_vtx = new rope_vtx[m_nVtxAlloc = (params->nSegments + 1) * 2], 0,
					       (params->nSegments + 1) * sizeof(rope_vtx));
					m_vtx1 = new rope_vtx[m_nVtxAlloc];
					for (i = 0; i <= params->nSegments; i++)
					{
						m_vtx[i].pt = m_segs[i].pt;
					}
					m_nVtx = params->nSegments + 1;
				}
			}
		}
		if (!is_unused(params->pPoints) && params->pPoints)
		{
			for (i = 0; i <= m_nSegs; i++)
			{
				m_segs[i].pt = m_segs[i].pt0 = params->pPoints[i];
			}
			m_nVtx = 0;
			RecalcBBox();
			++diff;
		}
		if (!is_unused(params->pVelocities) && params->pVelocities)
		{
			for (i = 0; i <= m_nSegs; i++)
			{
				m_segs[i].vel = params->pVelocities[i];
			}
			++diff;
		}

		if (m_idConstraint && m_pTiedTo[m_iConstraintClient])
		{
			pe_action_update_constraint arc;
			arc.idConstraint = m_idConstraint;
			arc.bRemove = 1;
			m_pTiedTo[m_iConstraintClient]->Action(&arc);
			m_idConstraint = 0;
		}
		// for(i=0;i<2;i++) if (m_pTiedTo[i] && m_pTiedTo[i]->m_iSimClass==7)
		//	m_pTiedTo[i] = 0;

		if (m_segs)
		{
			for (i = 0; i < 2; i++)
			{
				if (!is_unused(params->pEntTiedTo[i]))
				{
					if (m_pTiedTo[i])
					{
						m_pTiedTo[i]->Release();
					}
					CPhysicalEntity* pPrevTied = m_pTiedTo[i];
					m_pTiedTo[i] =
					    params->pEntTiedTo[i] == WORLD_ENTITY
						? &g_StaticPhysicalEntity
						: (params->pEntTiedTo[i]
					               ? ((CPhysicalPlaceholder*)params->pEntTiedTo[i])->GetEntity()
					               : 0);
					if (m_pTiedTo[i] != pPrevTied && pPrevTied)
					{
						if (pPrevTied->GetType() == PE_ROPE)
						{
							pPrevTied->RemoveCollider(this);
						}
						if (!(m_flags & pef_disabled))
						{
							if ((unsigned int)pPrevTied->m_iSimClass < 7u)
							{
								pPrevTied->Awake();
							}
							if (m_pTiedTo[i ^ 1] &&
							    (unsigned int)m_pTiedTo[i ^ 1]->m_iSimClass < 7u)
							{
								m_pTiedTo[i ^ 1]->Awake();
							}
							diff++;
						}
					}
					// if (m_pTiedTo[i] && m_pTiedTo[i]->m_iDeletionTime>0)
					//	m_pTiedTo[i] = 0;
					if (m_pTiedTo[i])
					{
						if (!is_unused(params->idPartTiedTo[i]))
						{
							if (m_pTiedTo[i]->GetType() != PE_ROPE)
							{
								for (m_iTiedPart[i] = 0;
								     m_iTiedPart[i] < m_pTiedTo[i]->m_nParts &&
								     params->idPartTiedTo[i] !=
								         m_pTiedTo[i]->m_parts[m_iTiedPart[i]].id;
								     m_iTiedPart[i]++)
									;
								if (m_iTiedPart[i] >= m_pTiedTo[i]->m_nParts)
								{
									m_iTiedPart[i] = 0;
								}
							}
							else
							{
								m_iTiedPart[i] = params->idPartTiedTo[i];
							}
						}
						Vec3 pos;
						quaternionf q;
						float scale;
						m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], pos, q, scale);
						if (!is_unused(params->ptTiedTo[i]))
						{
							m_ptTiedLoc[i] = params->bLocalPtTied
							                     ? params->ptTiedTo[i]
							                     : (params->ptTiedTo[i] - pos) * q;
						}
						else if (m_nSegs > 0 && (!is_unused(params->pEntTiedTo[i]) ||
						                         !is_unused(params->idPartTiedTo[i])))
						{
							m_ptTiedLoc[i] = (m_segs[m_nSegs * i].pt - pos) * q;
						}
						m_pTiedTo[i]->AddRef();
						if (m_pTiedTo[i]->GetType() == PE_ROPE)
						{
							m_pTiedTo[i]->AddCollider(this);
						}
						if (m_nSegs > 0 && m_segs[i * m_nSegs].pContactEnt == m_pTiedTo[i])
						{
							m_pTiedTo[i]->Release();
							m_segs[i * m_nSegs].pContactEnt = 0;
						}
					}
				}
			}
		}
		if (diff > 1E-6f)
		{
			m_bAwake = 1;
			m_nSlowFrames = 0;
		}

		if (m_pTiedTo[0] && m_pTiedTo[1])
		{
			pe_action_add_constraint aac;
			Vec3 pos;
			quaternionf q;
			float scale;
			m_iConstraintClient = isneg(m_pTiedTo[0]->GetMassInv() - m_pTiedTo[1]->GetMassInv());
			for (i = 0; i < 2; i++)
			{
				m_pTiedTo[m_iConstraintClient ^ i]->GetLocTransform(
				    m_iTiedPart[m_iConstraintClient ^ i], pos, q, scale);
				aac.pt[i] = pos + q * m_ptTiedLoc[m_iConstraintClient ^ i];
				aac.partid[i] = m_pTiedTo[m_iConstraintClient ^ i]
				                    ->m_parts[m_iTiedPart[m_iConstraintClient ^ i]]
				                    .id;
			}
			aac.pBuddy = m_pTiedTo[m_iConstraintClient ^ 1];
			aac.pConstraintEntity = this;
			aac.maxPullForce = m_maxForce;
			m_idConstraint = m_pTiedTo[m_iConstraintClient]->Action(&aac);
		}

		return 1;
	}

	return 0;
}

int CRopeEntity::GetParams(pe_params* _params)
{
	int res = CPhysicalEntity::GetParams(_params);
	if (res)
	{
		return res;
	}

	if (_params->type == pe_simulation_params::type_id)
	{
		pe_simulation_params* params = (pe_simulation_params*)_params;
		params->gravity = m_gravity;
		params->damping = m_damping;
		params->maxTimeStep = m_maxAllowedStep;
		params->minEnergy = m_Emin;
		return 1;
	}

	if (_params->type == pe_params_rope::type_id)
	{
		pe_params_rope* params = (pe_params_rope*)_params;
		ReadLock lock(m_lockUpdate);
		params->length = m_length;
		params->mass = m_mass;
		params->nSegments = m_nSegs;
		params->collDist = m_collDist;
		params->surface_idx = m_surface_idx;
		params->friction = m_friction;
		params->stiffnessAnim = m_stiffnessAnim;
		params->stiffnessDecayAnim = m_stiffnessDecayAnim;
		params->dampingAnim = m_dampingAnim;
		params->bTargetPoseActive = m_bTargetPoseActive;
		params->wind = m_wind;
		params->windVariance = m_windVariance;
		params->airResistance = m_airResistance;
		params->waterResistance = m_waterResistance;
		params->density = 1.0f / m_rdensity;
		params->jointLimit = m_jointLimit;
		params->sensorRadius = m_szSensor;
		params->maxForce = m_maxForce;
		params->flagsCollider = m_flagsCollider;
		params->nMaxSubVtx = m_nMaxSubVtx;
		params->frictionPull = m_frictionPull;
		params->stiffness = m_stiffness;
		params->pPoints = &m_segs[0].pt;
		params->pVelocities = &m_segs[0].vel;
		params->pPoints.iStride = params->pVelocities.iStride = sizeof(rope_segment);
		for (int i = 0; i < 2; i++)
		{
			params->pEntTiedTo[i] = m_pTiedTo[i];
			if (params->pEntTiedTo[i])
			{
				if (m_pTiedTo[i] == &g_StaticPhysicalEntity)
				{
					params->pEntTiedTo[i] = WORLD_ENTITY;
				}
				params->idPartTiedTo[i] = m_pTiedTo[i]->m_parts[m_iTiedPart[i]].id;
				Vec3 pos;
				quaternionf q;
				float scale;
				m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], pos, q, scale);
				params->ptTiedTo[i] = pos + q * m_ptTiedLoc[i];
			}
		}
		if (is_unused(m_collBBox[0]))
		{
			params->collisionBBox[0] = params->collisionBBox[1].zero();
		}
		else
		{
			params->collisionBBox[0] = m_collBBox[0];
			params->collisionBBox[1] = m_collBBox[1];
		}
		return 1;
	}

	return 0;
}

int CRopeEntity::GetStatus(pe_status* _status)
{
	int res = CPhysicalEntity::GetStatus(_status);
	if (res)
	{
		if (_status->type == pe_status_caps::type_id)
		{
			pe_status_caps* status = (pe_status_caps*)_status;
			status->bCanAlterOrientation = 1;
		}
		return res;
	}

	if (_status->type == pe_status_rope::type_id)
	{
		pe_status_rope* status = (pe_status_rope*)_status;
		ReadLock lock(m_lockUpdate);
		if (m_nSegs < 0)
		{
			return 0;
		}
		status->nSegments = m_nSegs;
		int i;
		if (!is_unused(status->pPoints) && status->pPoints)
		{
			for (i = 0; i <= m_nSegs; i++)
			{
				status->pPoints[i] = m_segs[i].pt0;
			}
		}
		if (!is_unused(status->pVelocities) && status->pVelocities)
		{
			for (i = 0; i <= m_nSegs; i++)
			{
				status->pVelocities[i] = m_segs[i].vel;
			}
		}
		if (!is_unused(status->pContactNorms) && status->pContactNorms)
		{
			for (i = 0; i < m_nSegs; i++)
			{
				if (m_segs[i].pContactEnt)
				{
					status->pContactNorms[i] = m_segs[i].ncontact;
				}
			}
		}
		int* nCollCount[2] = {&(status->nCollStat = 0), &(status->nCollDyn = 0)};
		volatile CPhysicalEntity* pContactEnt;
		for (i = 0; i < m_nSegs; i++)
		{
			pContactEnt = *(volatile CPhysicalEntity**)&m_segs[i].pContactEnt;
			if (pContactEnt)
			{
				(*nCollCount[-pContactEnt->m_iSimClass >> 31 & 1])++;
			}
		}
		status->bTargetPoseActive = m_bTargetPoseActive;
		status->stiffnessAnim = m_stiffnessAnim;
		status->pContactEnts = (IPhysicalEntity**)&m_segs[0].pContactEnt;
		status->pContactEnts.iStride = sizeof(rope_segment);
		if (m_flags & rope_subdivide_segs)
		{
			ReadLock lockVtx(m_lockVtx);
			status->nVtx = m_nVtx;
			if (status->pVtx)
			{
				for (i = 0; i < m_nVtx; i++)
				{
					status->pVtx[i] = m_vtx[i].pt;
				}
			}
		}
		else
		{
			status->nVtx = 0;
		}
		status->timeLastActive = m_timeLastActive;
		return 1;
	}

	return 0;
}

void CRopeEntity::EnforceConstraints(float seglen, const quaternionf& qtv, const Vec3& offstv, float scaletv)
{
	int i, iDir, iStart, iEnd, bHasContacts, bTargetPoseActive = m_bTargetPoseActive;
	float a, len, angleSrc, angleDst;
	Vec3 dir0src, dir1src, dir0dst, dir1dst, axis0src, axis1src, axis1dst;
	if (m_flags & rope_subdivide_segs && m_bHasContacts)
	{
		return;
	}

	if (m_flags & rope_no_stiffness_when_colliding)
	{
		iDir = m_pTiedTo[0] ? m_pTiedTo[0]->m_id : (m_pTiedTo[1] ? m_pTiedTo[1]->m_id : -2);
		for (i = bHasContacts = 0; i <= m_nSegs; i++)
		{
			if (m_segs[i].pContactEnt)
			{
				bHasContacts |= (m_segs[i].pContactEnt->m_id - iDir) >> 31 |
				                (iDir - m_segs[i].pContactEnt->m_id) >> 31;
			}
		}
		if (bHasContacts)
		{
			bTargetPoseActive = 0;
		}
	}

	iDir = m_pTiedTo[1] && !m_pTiedTo[0] ? -1 : 1;
	iStart = m_nSegs & iDir >> 31;
	iEnd = m_nSegs & -iDir >> 31;
	if (bTargetPoseActive == 0)
	{
		for (i = iStart; i != iEnd; i += iDir)
		{
			m_segs[i + iDir].pt =
			    m_segs[i].pt +
			    (m_segs[i + (iDir >> 31)].dir = (m_segs[i + iDir].pt - m_segs[i].pt).normalized()) * seglen;
		}
	}
	else if (m_jointLimit <= 0)
	{
		for (i = iStart, m_length = 0; i != iEnd; i += iDir)
		{
			len = (m_segs[i + iDir].ptdst - m_segs[i].ptdst).len() * scaletv;
			m_length += len;
			m_segs[i + iDir].pt =
			    m_segs[i].pt +
			    (m_segs[i + (iDir >> 31)].dir = (m_segs[i + iDir].pt - m_segs[i].pt).normalized()) * len;
		}
	}
	else
	{
		if (bTargetPoseActive != 2 && m_pTiedTo[0] && m_iTiedPart[0] < m_pTiedTo[0]->m_nParts)
		{
			dir0dst = m_pTiedTo[0]->m_qrot * (m_pTiedTo[0]->m_parts[m_iTiedPart[0]].q * m_dir0dst);
		}
		else
		{
			dir0dst = qtv * (m_segs[iStart + iDir].ptdst - m_segs[iStart].ptdst);
		}
		m_length = len = (m_segs[iStart + iDir].ptdst - m_segs[iStart].ptdst).len();
		dir0src = (m_segs[iStart + iDir].pt - m_segs[iStart].pt).normalized();
		axis0src = dir0src ^ dir0dst;
		angleSrc = atan2_tpl(a = axis0src.len(), dir0src * dir0dst);
		if (angleSrc > m_jointLimit && a > 1e-20f)
		{
			dir0src = dir0src.GetRotated(axis0src / a, angleSrc - m_jointLimit);
		}
		m_segs[iStart + iDir].pt =
		    m_segs[iStart].pt + (m_segs[iStart + (iDir >> 31)].dir = dir0src) * (len * scaletv);

		for (i = iStart + iDir; i != iEnd; i += iDir)
		{
			len = (m_segs[i + iDir].ptdst - m_segs[i].ptdst).len();
			m_length += len;
			m_segs[i + (iDir >> 31)].dir = (m_segs[i + iDir].pt - m_segs[i].pt).normalized();
			dir0src = m_segs[i].pt - m_segs[i - iDir].pt;
			dir0dst = m_segs[i].ptdst - m_segs[i - iDir].ptdst;
			dir1src = m_segs[i + iDir].pt - m_segs[i].pt;
			dir1dst = m_segs[i + iDir].ptdst - m_segs[i].ptdst;
			axis1src = dir0src ^ dir1src;
			axis1dst = dir0dst ^ dir1dst;
			angleSrc = atan2_tpl(a = axis1src.len(), dir0src * dir1src);
			angleDst = atan2_tpl(axis1dst.len(), dir0dst * dir1dst);
			if (fabs_tpl(angleSrc - angleDst) > m_jointLimit && a > 1e-20f)
			{
				m_segs[i + (iDir >> 31)].dir = m_segs[i + (iDir >> 31)].dir.GetRotated(
				    axis1src / a, angleDst - angleSrc - (m_jointLimit * sgnnz(angleDst - angleSrc)));
			}
			m_segs[i + iDir].pt = m_segs[i].pt + m_segs[i + (iDir >> 31)].dir * (len * scaletv);
		}
	}
}

int CRopeEntity::Action(pe_action* _action, int bThreadSafe)
{
	if (_action->type == pe_action_notify::type_id)
	{
		WriteLock lock(m_lockUpdate);
		pe_action_notify* action = (pe_action_notify*)_action;
		if (action->iCode == pe_action_notify::ParentChange)
		{
			int i;
			Vec3 pos;
			quaternionf q;
			float scale;
			if ((!m_pTiedTo[0] || !m_pTiedTo[1]) && m_nSegs > 0 && m_segs)
			{ // if the rope is tied with at most one end, do exact length enforcement
				for (i = 0; i < 2; i++)
				{
					if (m_pTiedTo[i])
					{
						m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], pos, q, scale);
						m_segs[m_nSegs * i].pt = m_segs[m_nSegs * i].pt0 =
						    pos + q * m_ptTiedLoc[i];
					}
				}
				q.SetIdentity();
				pos.zero();
				scale = 1.0f;
				if (m_flags & (rope_target_vtx_rel0 | rope_target_vtx_rel1))
				{
					i = (m_flags & rope_target_vtx_rel1) != 0;
					if (m_pTiedTo[i])
					{
						m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], pos, q, scale);
						if (m_bTargetPoseActive == 2)
						{
							m_ptTiedLoc[i] = m_segs[i * m_nSegs].ptdst;
						}
					}
				}
				EnforceConstraints(m_length / m_nSegs, q, pos, scale);
				if (!(m_flags & pef_disabled))
				{
					m_bAwake = 1;
					m_nSlowFrames = 0;
				}
			}
		}
		return 1;
	}

	ChangeRequest<pe_action> req(this, m_pWorld, _action, bThreadSafe);
	if (req.IsQueued())
	{
		return 1;
	}

	int i, j;
	float t = 0, rmass;
	if (_action->type == pe_action_impulse::type_id)
	{
		pe_action_impulse* action = (pe_action_impulse*)_action;
		if (!is_unused(action->ipart))
		{
			i = action->ipart;
		}
		else if (!is_unused(action->partid))
		{
			i = action->partid;
		}
		else if (!is_unused(action->point))
		{
			for (j = 1, i = 0; j <= m_nSegs; j++)
			{
				if ((m_segs[j].pt - action->point).len2() < (m_segs[i].pt - action->point).len2())
				{
					i = j;
				}
			}
			if ((m_segs[i].pt - action->point).len2() > sqr(m_collDist * 3) &&
			    (i == 0 || (m_segs[i].pt - m_segs[i - 1].pt ^ action->point - m_segs[i - 1].pt).len2() >
			                   sqr(m_collDist) * (m_segs[i].pt - m_segs[i - 1].pt).len2()))
			{
				return 0;
			}
			if (i < m_nSegs)
			{
				t = (action->point - m_segs[i].pt) * (m_segs[i + 1].pt - m_segs[i].pt).normalized();
			}
		}
		else
		{
			return 0;
		}
		if ((unsigned int)i > (unsigned int)m_nSegs)
		{
			return 0;
		}

		rmass = 1.0f / m_mass;
		m_segs[i].vel_ext += action->impulse * ((1.0f - t) * (m_nSegs + 1) * rmass);
		if (i < m_nSegs)
		{
			m_segs[i + 1].vel_ext += action->impulse * (t * (m_nSegs + 1) * rmass);
		}

		m_bAwake = 1;
		m_nSlowFrames = 0;
		m_timeIdle *= isneg(-action->iSource);
		for (i = 0; i < m_nColliders; i++)
		{
			m_pColliders[i]->Awake();
		}
		return 1;
	}

	if (_action->type == pe_action_target_vtx::type_id)
	{
		pe_action_target_vtx* action = (pe_action_target_vtx*)_action;
		if (!is_unused(action->points) && action->nPoints == m_nSegs + 1)
		{
			int bFar = (m_segs[0].pt - action->points[0]).len2() > sqr(m_length * 5);
			for (i = 0; i <= m_nSegs; i++)
			{
				m_segs[i].ptdst = action->points[i];
				if (!(m_flags & (rope_target_vtx_rel0 | rope_target_vtx_rel1)) &&
				    (m_flags & pef_disabled || bFar))
				{
					m_segs[i].pt = m_segs[i].pt0 = action->points[i];
				}
			}
			if (m_bTargetPoseActive == 1)
			{
				for (i = 0; i < 2; i++)
				{
					if (m_pTiedTo[i])
					{
						Vec3 pos;
						quaternionf q;
						float scale;
						m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], pos, q, scale);
						m_ptTiedLoc[i] = (action->points[m_nSegs * i] - pos) * q;
						if (i == 0 && m_iTiedPart[0] < m_pTiedTo[0]->m_nParts)
						{
							m_dir0dst = ((action->points[1] - action->points[0]) *
							             m_pTiedTo[0]->m_qrot) *
							            m_pTiedTo[0]->m_parts[m_iTiedPart[0]].q;
						}
					}
				}
			}
		}
		else if (m_flags & (rope_target_vtx_rel0 | rope_target_vtx_rel1))
		{
			int iParent = (m_flags & rope_target_vtx_rel1) != 0;
			Vec3 offsParent;
			quaternionf qParent;
			float scaleParent;
			if (m_pTiedTo[iParent])
			{
				m_pTiedTo[iParent]->GetLocTransform(m_iTiedPart[iParent], offsParent, qParent,
				                                    scaleParent);
				scaleParent = 1.0f / scaleParent;
				for (i = 0; i <= m_nSegs; i++)
				{
					m_segs[i].ptdst = (m_segs[i].pt - offsParent) * qParent * scaleParent;
				}
			}
		}
		else
		{
			for (i = 0; i <= m_nSegs; i++)
			{
				m_segs[i].ptdst = m_segs[i].pt;
			}
		}
		return 1;
	}

	if (_action->type == pe_action_reset::type_id)
	{
		if (!(m_flags & rope_subdivide_segs))
		{
			for (i = 0; i <= m_nSegs; i++)
			{
				m_segs[i].vel.zero();
				if (m_segs[i].pContactEnt)
				{
					m_segs[i].pContactEnt->Release();
					m_segs[i].pContactEnt = 0;
				}
			}
		}
		else
		{
			for (i = 0; i < 2; i++)
			{
				if (m_pTiedTo[i])
				{
					m_pTiedTo[i]->RemoveCollider(this);
				}
			}
			for (i = 1; i < m_nVtx - 1; i++)
			{
				if (m_vtx[i].pContactEnt)
				{
					m_vtx[i].pContactEnt->RemoveCollider(this);
					m_vtx[i].pContactEnt = 0;
				}
			}
			for (i = 0; i < m_nColliders; i++)
			{
				m_pColliders[i]->Release();
			}
			for (i = 0; i < m_nSegs; i++)
			{
				m_segs[i].pContactEnt = 0;
			}
			m_nColliders = 0;
		}
		m_bStrained = 0;
		return 1;
	}

	return CPhysicalEntity::Action(_action, 1);
}

RigidBody* CRopeEntity::GetRigidBodyData(RigidBody* pbody, int ipart)
{
	pbody->pos = (m_segs[ipart].pt + m_segs[ipart + 1].pt) * 0.5f;
	Vec3 dir = m_segs[ipart + 1].pt - m_segs[ipart].pt;
	pbody->q = Quat::CreateRotationV0V1(Vec3(0, 0, -1), dir.normalized());
	pbody->v = (m_segs[ipart].vel + m_segs[ipart + 1].vel) * 0.5f;
	pbody->w = (dir ^ m_segs[ipart + 1].vel - m_segs[ipart].vel) / dir.len2();
	pbody->M = m_mass / m_nSegs;
	pbody->Minv = 1.0f / pbody->M;
	return pbody;
}

void CRopeEntity::GetLocTransform(int ipart, Vec3& offs, quaternionf& q, float& scale)
{
	scale = 1.0f;
	q = Quat::CreateRotationV0V1(Vec3(0, 0, -1), (m_segs[ipart + 1].pt - m_segs[ipart].pt).normalized());
	offs = m_segs[ipart].pt;
}

void CRopeEntity::MeshVtxUpdated()
{
	int i;
	primitives::box* pbox = &((CSingleBoxTree*)m_pMesh->m_pTree.get())->m_Box;
	Vec3 axisx, axisy, BBox[2];

	for (i = 0; i < 2; i++)
	{
		m_pMesh->m_pNormals[i + 2] =
		    -(m_pMesh->m_pNormals[i] = (m_pMesh->m_pVertices[m_pMesh->m_pIndices[(i * 3) + 1]] -
		                                    m_pMesh->m_pVertices[m_pMesh->m_pIndices[i * 3]] ^
		                                m_pMesh->m_pVertices[m_pMesh->m_pIndices[(i * 3) + 2]] -
		                                    m_pMesh->m_pVertices[m_pMesh->m_pIndices[i * 3]])
		                                   .normalized());
	}

	axisx = m_pMesh->m_pVertices[1].normalized();
	axisy = m_pMesh->m_pVertices[2] + m_pMesh->m_pVertices[3] - m_pMesh->m_pVertices[1];
	(axisy -= axisx * (axisy * axisx)).normalize();
	pbox->Basis.SetFromVectors(axisx, axisy, axisx ^ axisy);
	for (i = 1; i < 4; i++)
	{
		BBox[0] = min(BBox[0], m_pMesh->m_pVertices[i]);
		BBox[1] = max(BBox[1], m_pMesh->m_pVertices[i]);
	}
	pbox->center = (BBox[1] + BBox[0]) * 0.5f;
	pbox->size = (BBox[1] - BBox[0]) * 0.5f;
	float e = max(max(pbox->size.x, pbox->size.y), pbox->size.z) * 0.01f;
	for (i = 0; i < 3; i++)
	{
		pbox->size[i] = max(pbox->size[i], e);
	}
}

void CRopeEntity::AllocSubVtx()
{
	if (!m_vtx)
	{
		m_vtx = new rope_vtx[m_nVtxAlloc = (m_nSegs + 1) * 2];
		m_vtx1 = new rope_vtx[m_nVtxAlloc];
	}
	if (m_nVtx == m_nVtxAlloc)
	{
		rope_vtx* pVtx = m_vtx;
		memcpy(m_vtx = new rope_vtx[m_nVtxAlloc += 16], pVtx, m_nVtx * sizeof(rope_vtx));
		delete[] pVtx;
		pVtx = m_vtx1;
		memcpy(m_vtx1 = new rope_vtx[m_nVtxAlloc], pVtx, m_nVtx * sizeof(rope_vtx));
		delete[] pVtx;
		if (m_vtxSolver)
		{
			rope_solver_vtx* pVtxSolver = m_vtxSolver;
			memcpy(m_vtxSolver = new rope_solver_vtx[m_nVtxAlloc], pVtxSolver,
			       m_nVtx * sizeof(rope_solver_vtx));
			delete[] pVtxSolver;
		}
	}
}

void CRopeEntity::FillVtxContactData(rope_vtx* pvtx, int iseg, SRopeCheckPart& cp, geom_contact* pcontact)
{
	pvtx->pt = pcontact->pt - pcontact->dir * pcontact->t;
	if (iseg >= 0)
	{
		float t = (pvtx->pt - m_segs[iseg].pt).len();
		t = t / (t + (pvtx->pt - m_segs[iseg + 1].pt).len());
		pvtx->vel = m_segs[iseg].vel * (1 - t) + m_segs[iseg + 1].vel * t;
	}
	pvtx->ncontact = pcontact->n.normalized();
	pvtx->pt += pvtx->ncontact * m_collDist;
	RigidBody* pbody = cp.pent->GetRigidBody(cp.ipart);
	pvtx->vcontact = pbody->v + (pbody->w ^ pvtx->pt - pbody->pos);
	pvtx->pContactEnt = cp.pent;
	pvtx->iContactPart = cp.ipart;
}

void CRopeEntity::StartStep(float time_interval)
{
	m_timeStepPerformed = 0;
	m_timeStepFull = time_interval;
	m_posBody[0][0] = m_posBody[0][1];
	m_qBody[0][0] = m_qBody[0][1];
	m_posBody[1][0] = m_posBody[1][1];
	m_qBody[1][0] = m_qBody[1][1];
}

float CRopeEntity::GetMaxTimeStep(float time_interval)
{
	if (m_timeStepPerformed > m_timeStepFull - 0.001f)
	{
		return time_interval;
	}
	return min(min(m_timeStepFull - m_timeStepPerformed, m_maxAllowedStep), time_interval);
}

int CRopeEntity::Step(float time_interval)
{
	if (m_nSegs * m_length <= 0 || !m_bAwake)
	{
		return 1;
	}

	int i, j;
	if (m_flags & (rope_collides | rope_collides_with_terrain))
	{
		for (i = 0; i <= m_nSegs; i++)
		{
			if (m_segs[i].pContactEnt && (m_segs[i].pContactEnt->m_iSimClass == 7 ||
			                              m_segs[i].iContactPart >= m_segs[i].pContactEnt->m_nParts))
			{
				if (!(m_flags & rope_subdivide_segs))
				{
					m_segs[i].pContactEnt->Release();
				}
				m_segs[i].pContactEnt = 0;
				MARK_UNUSED m_segs[i].ncontact;
			}
		}
	}
	for (i = j = 0; i < 2; i++)
	{
		if (m_pTiedTo[i] &&
		    ((unsigned int)m_pTiedTo[i]->m_iSimClass >= 7u ||
		     (unsigned int)m_pTiedTo[i]->m_iSimClass - 1u < 2u && m_iTiedPart[i] >= m_pTiedTo[i]->m_nParts))
		{
			j |= 1 << i;
		}
	}
	if (j)
	{
		pe_params_rope pr;
		if (j & 1)
		{
			pr.pEntTiedTo[0] = 0;
		}
		if (j & 2)
		{
			pr.pEntTiedTo[1] = 0;
		}
		SetParams(&pr);
	}

	if (m_flags & pef_disabled)
	{
		if (m_bTargetPoseActive == 2 && m_pTiedTo[0])
		{
			Vec3 dir;
			quaternionf q;
			float scale;
			m_pTiedTo[0]->GetLocTransform(m_iTiedPart[0], dir, q, scale);
			m_segs[0].pt = dir + q * m_ptTiedLoc[0];
			m_BBox[0] = min(m_BBox[0], m_segs[0].pt);
			m_BBox[1] = max(m_BBox[1], m_segs[0].pt);
		}
		return 1;
	}
	if (m_pWorld->m_bWorldStep != 2)
	{
		if (m_timeStepPerformed > m_timeStepFull - 0.001f ||
		    m_timeStepPerformed > 0 && (m_flags & pef_invisible))
		{
			return 1;
		}
	}
	else if (time_interval > 0)
	{
		time_interval = max(0.001f, min(m_timeStepFull - m_timeStepPerformed, time_interval));
	}
	else
	{
		return 1;
	}
	m_timeStepPerformed += time_interval;
	m_lastTimeStep = time_interval;
	m_timeLastActive = m_pWorld->m_timePhysics;

	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	float seglen = m_length / m_nSegs, seglen2 = sqr(seglen), rseglen = m_nSegs / m_length, rseglen2 = sqr(rseglen),
	      Ebefore, scale{};
	int iDir, iStart, iEnd, iter, bStretched = 0, bTargetPoseActive = m_bTargetPoseActive, bGridLocked = 0,
				      bHasContacts = 0;
	int flags = m_flags;
	Vec3 pos, gravity, dir, ptend[2], sz, BBox[2], ptnew, dv, dw, vrel, dir0src, dir1src, dir0dst, dir1dst,
	    axis0src, axis1src, axis0dst, axis1dst, offstv;
	float len2, diff, a{}, b, r2, r2new, pAp, vmax, k, E, damping = max(0.0f, 1.0f - (m_damping * time_interval)),
							      rnSegs = 1.0f / m_nSegs, rcollDist = 0, angleSrc,
							      angleDst, scaletv = 1.0f;
	quaternionf dq, qtv(1, 0, 0, 0), q;
	RigidBody* pbody;
	pe_params_buoyancy pb[4];
	EventPhysPostStep event;

	if ((iEnd = m_pWorld->CheckAreas(this, dir, pb, 4)) && !is_unused(dir))
	{
		m_gravity = (dir - m_pWorld->m_vars.gravity).len2() < dir.len2() * sqr(0.01) ? m_gravity0 : dir;
	}
	gravity = m_gravity;

	event.pEntity = this;
	event.pForeignData = m_pForeignData;
	event.iForeignData = m_iForeignData;
	event.dt = time_interval;
	event.pos = m_pos;
	event.q = m_qrot;
	event.idStep = m_pWorld->m_idStep;
	m_pWorld->OnEvent(m_flags, &event);
	for (i = 0, dir = m_wind; i < iEnd; i++)
	{
		dir += pb[i].waterFlow * pb[i].iMedium;
	}
	if (m_flags & rope_no_stiffness_when_colliding)
	{
		iter = m_pTiedTo[0] ? m_pTiedTo[0]->m_id : (m_pTiedTo[1] ? m_pTiedTo[1]->m_id : -2);
		for (i = bHasContacts = 0; i <= m_nSegs; i++)
		{
			if (m_segs[i].pContactEnt)
			{
				bHasContacts |= (m_segs[i].pContactEnt->m_id - iter) >> 31 |
				                (iter - m_segs[i].pContactEnt->m_id) >> 31;
			}
		}
		if (bHasContacts)
		{
			bTargetPoseActive = 0;
		}
	}

	if ((m_windTimer += time_interval * 4) > 1.0f)
	{
		m_windTimer = 0;
		m_wind0 = m_wind1;
		a = m_windVariance * (fabs_tpl(dir.x) + fabs_tpl(dir.y) + fabs_tpl(dir.z));
		m_wind1 = dir + Vec3(physics_frand(a) - (a * 0.5f), physics_frand(a) - (a * 0.5f),
		                     physics_frand(a) - (a * 0.5f));
	}
	dw = m_wind0 * m_windTimer + m_wind1 * (1.0f - m_windTimer);
	if (bTargetPoseActive && m_segs[0].pContactEnt && m_segs[0].tcontact < 0.2f)
	{
		gravity.zero();
		dw.zero();
		bTargetPoseActive = 0;
		for (i = 0; i <= m_nSegs; i++)
		{
			m_segs[i].vel.zero();
		}
	}
	for (i = 0; i <= m_nSegs; i++)
	{
		m_segs[i].pt0 = m_segs[i].pt;
		ptnew = m_segs[i].pt + m_segs[i].vel * time_interval;
		if (bTargetPoseActive == 1 &&
		    (m_stiffnessAnim == 0 || (m_segs[i].pt - m_segs[i].ptdst) * (ptnew - m_segs[i].ptdst) < 0 &&
		                                 (ptnew - m_segs[i].ptdst).len2() < sqr(seglen * 0.08f)))
		{
			ptnew = m_segs[i].ptdst;
		}
		m_segs[i].pt = ptnew;
		m_segs[i].vel += gravity * time_interval;
		m_segs[i].vel += (dw - m_segs[i].vel) * min(1.0f, m_airResistance * time_interval);
		for (iter = 0; iter < iEnd; iter++)
		{
			if ((diff = ((m_segs[i].pt - pb[iter].waterPlane.origin) * pb[iter].waterPlane.n) *
			            ((pb[iter].iMedium - 1) >> 31)) > 0)
			{
				if (rcollDist == 0)
				{
					rcollDist = 1.0f / m_collDist;
				}
				m_segs[i].vel += (pb[iter].waterFlow - m_segs[i].vel) *
				                     min(1.0f, m_waterResistance * time_interval) -
				                 gravity * (pb[iter].waterDensity * m_rdensity *
				                            min(1.0f, diff * rcollDist) * time_interval);
			}
		}
	}

	if (m_flags & rope_subdivide_segs && m_nVtx > 0)
	{
		for (i = 0; i < m_nVtx; i++)
		{
			m_vtx[i].pt += m_vtx[i].vel * time_interval;
		}
		for (i = 0; i < m_nSegs; i++)
		{
			if (m_segs[i + 1].iVtx0 > m_segs[i].iVtx0 + 1)
			{
				dir0src = (m_vtx[m_segs[i + 1].iVtx0].pt - m_vtx[m_segs[i].iVtx0].pt).normalized();
				for (m_segs[i].ptdst.zero(), j = m_segs[i].iVtx0; j < m_segs[i + 1].iVtx0; j++)
				{
					dir = m_vtx[j].pt - m_vtx[m_segs[i].iVtx0].pt;
					m_segs[i].ptdst += dir - dir0src * (dir * dir0src);
				}
				m_segs[i].ptdst.normalize();
			}
			else
			{
				m_segs[i].ptdst.zero();
			}
		}
		if (m_bStrained)
		{
			for (i = 0; i <= m_nSegs; i++)
			{
				m_segs[i].pt = m_vtx[m_segs[i].iVtx0].pt;
			}
		}
	}

	if (m_flags & rope_findiff_attached_vel)
	{
		a = 1.0f / m_timeStepFull;
	}
	for (i = 1; i >= 0; i--)
	{
		if (m_pTiedTo[i])
		{
			m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], pos, q, scale);
			m_posBody[i][1] = pos;
			m_qBody[i][1] = q;
			m_segs[m_nSegs * i].pt = ptend[i] = pos + q * m_ptTiedLoc[i];
			pbody = m_pTiedTo[i]->GetRigidBody(m_iTiedPart[i]);
			if (!(m_flags & rope_findiff_attached_vel))
			{
				dv = pbody->v;
				dw = pbody->w;
			}
			else
			{
				dv = (m_posBody[i][1] - m_posBody[i][0]) * a;
				dq = m_qBody[i][1] * !m_qBody[i][0];
				dw = dq.v * (dq.w * 2 * a);
				if (dv.len2() > sqr(m_pWorld->m_vars.maxVel))
				{
					dv.zero();
					dw.zero();
				}
			}
			m_segs[m_nSegs * i].vel = dv + (dw ^ ptend[i] - pbody->pos);
			m_timeIdle *= m_pTiedTo[i]->IsAwake() ^ 1;
		}
	}
	if (m_flags & (rope_target_vtx_rel0 | rope_target_vtx_rel1))
	{
		i = (m_flags & rope_target_vtx_rel1) != 0;
		if (m_pTiedTo[i])
		{
			m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], offstv, qtv, scaletv);
		}
	}

	if (bTargetPoseActive == 1)
	{
		for (i = 0; i < m_nSegs; i++)
		{
			dv = (qtv * m_segs[i + 1].ptdst * scaletv + offstv - m_segs[i + 1].pt) * m_stiffnessAnim *
			     (1 - (m_stiffnessDecayAnim * (i + 1) * rnSegs));
			a = max(0.0f, 1 - (m_dampingAnim * time_interval));
			m_segs[i + 1].vel = m_segs[i + 1].vel * a + dv * (1 - a);
		}
	}
	else if (bTargetPoseActive == 2)
	{
		// dir0src, dir0dst - point to attached object's center
		// axis0src=axis0dst = some axis perpendicular to dir0src
		if (m_stiffnessAnim > 0)
		{
			if (m_pTiedTo[0])
			{
				m_pTiedTo[0]->GetLocTransform(m_iTiedPart[0], dir, q, scale);
				dir0dst = dir0src = (m_segs[0].pt - dir).normalized();
				axis0src = axis0dst = q * (!q * dir0src).GetOrthogonal().normalized();
			}
			else
			{
				dir0src.Set(0, 0, 1);
				dir0dst = dir0src;
				axis0src.Set(0, 1, 0);
				axis0dst = axis0src;
			}

			for (i = 0; i < m_nSegs;
			     i++, dir0src = dir1src, dir0dst = dir1dst, axis0src = axis1src, axis0dst = axis1dst)
			{
				dir1src = (m_segs[i + 1].pt - m_segs[i].pt).normalized();
				dir1dst = qtv * (m_segs[i + 1].ptdst - m_segs[i].ptdst).normalized();
				axis1src = dir0src ^ dir1src;
				axis1dst = dir0dst ^ dir1dst;
				angleSrc = atan2_tpl(a = axis1src.len(), dir0src * dir1src);
				angleDst = atan2_tpl(b = axis1dst.len(), dir0dst * dir1dst);
				// rotate around axis1src to bring angleSrc to angleDst
				axis1src = a > m_length * 0.001f ? axis1src / a : axis0src;
				axis1dst = b > m_length * 0.001f ? axis1dst / b : axis0dst;
				if (fabs_tpl(angleDst - angleSrc) > g_PI)
				{
					angleDst -= 2 * sgnnz(angleDst - angleSrc) * g_PI;
				}
				dw = axis1src * (angleDst - angleSrc);
				// rotate around dir0src to match (axis1src wrt axis0src) to (axis1dst wrt axis0dst)
				angleSrc =
				    atan2_tpl((axis1src ^ axis0src) * dir0src,
				              axis0src * axis1src - ((axis0src * dir0src) * (axis1src * dir0src)));
				angleDst =
				    atan2_tpl((axis1dst ^ axis0dst) * dir0dst,
				              axis0dst * axis1dst - ((axis0dst * dir0dst) * (axis1dst * dir0dst)));
				if (fabs_tpl(angleDst - angleSrc) > g_PI)
				{
					angleDst -= 2 * sgnnz(angleDst - angleSrc) * g_PI;
				}
				dw -= dir0src * (angleDst - angleSrc);
				// update m_segs[i+1].vel
				dv = dw * (m_stiffnessAnim * (1 - m_stiffnessDecayAnim * (i + 1) * rnSegs) *
				           time_interval) ^
				     m_segs[i + 1].pt - m_segs[i].pt;
				vrel = m_segs[i + 1].vel - m_segs[i].vel;
				m_segs[i + 1].vel =
				    m_segs[i].vel + vrel * max(0.0f, 1.0f - (m_dampingAnim * time_interval)) + dv;
			}
		}
		else
		{
			for (i = 0; i <= m_nSegs; i++)
			{
				m_segs[i].pt = qtv * m_segs[i].ptdst * scaletv + offstv;
			}
		}
	}

	if (!(m_flags & rope_subdivide_segs) && m_pTiedTo[0] && m_pTiedTo[1] &&
	    (ptend[1] - ptend[0]).len2() > sqr(m_length))
	{
		float newlen = (ptend[1] - ptend[0]).len(), newseglen = newlen / m_nSegs, rnSegs = 1.0f / (m_nSegs + 1);
		dir = (ptend[1] - ptend[0]).normalized();
		for (i = 1; i < m_nSegs; i++)
		{
			m_segs[i].dir = dir;
			m_segs[i].pt = ptend[0] + m_segs[i].dir * (newseglen * i);
			m_segs[i].vel =
			    m_segs[0].vel * ((m_nSegs + 1 - i) * rnSegs) + m_segs[m_nSegs].vel * (i * rnSegs);
		}
		m_segs[0].dir = m_segs[m_nSegs].dir = dir;
		pe_status_awake isawake;
		m_bAwake =
		    m_pTiedTo[0]->GetStatus(&isawake) | m_pTiedTo[1]->GetStatus(&isawake) | isneg(-bTargetPoseActive);

		pe_status_constraint sc;
		sc.id = m_idConstraint;
		if (m_idConstraint &&
		    (!m_pTiedTo[m_iConstraintClient]->GetStatus(&sc) || sc.flags & constraint_inactive))
		{
			EventPhysJointBroken epjb;
			epjb.idJoint = 0;
			epjb.bJoint = 0;
			MARK_UNUSED epjb.pNewEntity[0], epjb.pNewEntity[1];
			epjb.pEntity[0] = epjb.pEntity[1] = this;
			epjb.pForeignData[0] = epjb.pForeignData[1] = m_pForeignData;
			epjb.iForeignData[0] = epjb.iForeignData[1] = m_iForeignData;
			epjb.pt = m_segs[m_nSegs].pt;
			epjb.n = m_segs[m_nSegs - 1].dir;
			epjb.partid[0] = 1;
			epjb.partid[1] = m_pTiedTo[1]->m_parts[m_iTiedPart[1]].id;
			epjb.partmat[0] = epjb.partmat[1] = -1;
			m_pWorld->OnEvent(2, &epjb);

			pe_params_rope pr;
			pr.pEntTiedTo[1] = 0;
			SetParams(&pr);
		}

		BBox[0] = min(m_segs[0].pt, m_segs[m_nSegs].pt);
		BBox[1] = max(m_segs[0].pt, m_segs[m_nSegs].pt);
		a = (max(max(BBox[1].x - BBox[0].x, BBox[1].y - BBox[0].y), BBox[1].z - BBox[0].z) * 0.01f) +
		    (m_collDist * 2);
		BBox[0] -= Vec3(a, a, a);
		BBox[1] += Vec3(a, a, a);
		if (m_flags & pef_traceable)
		{
			bGridLocked = m_pWorld->RepositionEntity(this, 1, BBox);
		}
		{
			WriteLock lock(m_lockUpdate);
			m_pos = m_segs[0].pt;
			m_BBox[0] = BBox[0];
			m_BBox[1] = BBox[1];
			for (i = 0; i <= m_nSegs; i++)
			{
				m_segs[i].pt0 = m_segs[i].pt;
			}
			AtomicAdd(&m_pWorld->m_lockGrid, -bGridLocked);
		}
		return 1;
	}

	// first, ensure that all vertices have distance between them equal to seglen
	iter = 300;
	iDir = m_pTiedTo[1] && !m_pTiedTo[0] ? 1 : -1;
	if (!m_pTiedTo[0] || !m_pTiedTo[1]) // if the rope is tied with at most one end, do exact length enforcement
	{
		EnforceConstraints(seglen, qtv, offstv, scaletv);
	}
	else if (!(m_flags & rope_subdivide_segs))
	{
		do
		{
			iDir = -iDir;
			iStart = m_nSegs & iDir >> 31;
			iEnd = m_nSegs & -iDir >> 31;

			for (i = iStart; i != iEnd; i += iDir)
			{
				dir = m_segs[i + iDir].pt - m_segs[i].pt;
				len2 = dir.len2();
				diff = fabs_tpl(len2 - seglen2);
				bStretched = (diff > seglen2 * 0.01f);
				if (bStretched)
				{
					if (diff < seglen2 * 0.2f)
					{ // use 3 terms of 1/sqrt(x) Taylor series expansion
						k = len2 * rseglen2;
						k = 1.875f - ((1.25f - 0.375f * k) * k);
					}
					else
					{
						k = seglen / sqrt_tpl(len2);
					}
					m_segs[i + iDir].pt = m_segs[i].pt + dir * k;
					iter--;
				}
				else
				{
					k = 1.0f;
				}
				m_segs[i + (iDir >> 31)].dir = dir * (k * rseglen);
			}

			if (m_pTiedTo[0])
			{
				m_segs[0].pt = ptend[0];
			}
			if (m_pTiedTo[1])
			{
				m_segs[m_nSegs].pt = ptend[1];
			}
		}
		while (m_pTiedTo[(iDir + 1) >> 1] && bStretched && iter > 0);
	}

	BBox[0] = BBox[1] = m_segs[0].pt;
	for (i = 0; i <= m_nSegs; i++)
	{
		(m_segs[i].vel += m_segs[i].vel_ext) *= damping;
		m_segs[i].vel_ext.zero();
		m_segs[i].bRecalcDir = 0;
		BBox[0] = min(BBox[0], m_segs[i].pt);
		BBox[1] = max(BBox[1], m_segs[i].pt);
	}
	a = (max(max(BBox[1].x - BBox[0].x, BBox[1].y - BBox[0].y), BBox[1].z - BBox[0].z) * 0.01f) + (m_collDist * 2);
	BBox[0] -= Vec3(a, a, a);
	BBox[1] += Vec3(a, a, a);

	if (m_flags & rope_collides_with_terrain && m_pTiedTo[0] && m_pTiedTo[1] &&
	    m_pTiedTo[0]->m_iSimClass + m_pTiedTo[1]->m_iSimClass == 0 &&
	    sqr(m_length * 0.95f) < (m_segs[0].pt - m_segs[m_nSegs].pt).len2())
	{
		flags &= ~rope_collides_with_terrain;
	}

	if (m_flags & (rope_collides | rope_collides_with_terrain))
	{
		FRAME_PROFILER("Rope collision", GetISystem(), PROFILE_PHYSICS);

		CPhysicalEntity** pentlist;
		int iseg, nEnts, nCheckParts, nLocChecks, iend;
		primitives::box boxrope, boxpart;
		Vec3 center, ptres[2], rotax, n, collBBox[2] = {BBox[0], BBox[1]};
		float angle, dist2, t, cost, sint;
		SRopeCheckPart checkParts[20];
		CRayGeom aray;
		aray.m_iCollPriority = 10;
		intersection_params ip;
		geom_contact* pcontact;
		CPhysicalEntity* pent;
		geom_world_data gwd;
		COverlapChecker Overlapper;
		pe_status_pos sp;
		sp.timeBack = 1;
		m_bHasContacts = 0;

		ip.bStopAtFirstTri = true;
		ip.iUnprojectionMode = 1;
		if (!is_unused(m_collBBox[0]))
		{
			if (!m_pTiedTo[0])
			{
				collBBox[0] = m_collBBox[0], collBBox[1] = m_collBBox[1];
			}
			else
			{
				Matrix33 mtx = Matrix33(q);
				center = mtx * ((m_collBBox[0] + m_collBBox[1]) * 0.5f * scale) + pos;
				sz = mtx.Fabs() * ((m_collBBox[1] - m_collBBox[0]) * 0.5f * scale);
				collBBox[0] = center - sz;
				collBBox[1] = center + sz;
			}
		}
		boxrope.size = (BBox[1] - BBox[0]) * 0.5f;
		center = (BBox[0] + BBox[1]) * 0.5f;

		nEnts = m_pWorld->GetEntitiesAround(collBBox[0], collBBox[1], pentlist,
		                                    (flags & rope_collides_with_terrain ? ent_terrain : 0) |
		                                        ent_static | ent_sleeping_rigid | ent_rigid | ent_living |
		                                        ent_sort_by_mass | ent_ignore_noncolliding | ent_triggers,
		                                    this);

		for (i = j = nCheckParts = 0; i < nEnts; i++)
		{
			if (pentlist[i] != this && !(m_flags & rope_ignore_attachments &&
			                             (pentlist[i] == m_pTiedTo[0] ||
			                              m_pTiedTo[0] && m_pTiedTo[0]->m_pForeignData &&
			                                  m_pTiedTo[0]->m_pForeignData == pentlist[i]->m_pForeignData ||
			                              pentlist[i] == m_pTiedTo[1] ||
			                              m_pTiedTo[1] && m_pTiedTo[1]->m_pForeignData &&
			                                  m_pTiedTo[1]->m_pForeignData == pentlist[i]->m_pForeignData)))
			{
				for (j = 0; j < pentlist[i]->m_nParts; j++)
				{
					if (pentlist[i]->m_parts[j].flags & m_flagsCollider)
					{
						checkParts[nCheckParts].offset =
						    pentlist[i]->m_pos +
						    pentlist[i]->m_qrot * pentlist[i]->m_parts[j].pos;
						boxrope.Basis =
						    Matrix33(pentlist[i]->m_qrot * pentlist[i]->m_parts[j].q);
						boxrope.center =
						    (center - checkParts[nCheckParts].offset) * boxrope.Basis;
						if (pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom->GetType() !=
						    GEOM_HEIGHTFIELD)
						{
							pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom->GetBBox(
							    &checkParts[nCheckParts].bbox);
							checkParts[nCheckParts].bbox.center *=
							    pentlist[i]->m_parts[j].scale;
							checkParts[nCheckParts].bbox.size *=
							    pentlist[i]->m_parts[j].scale;
						}
						else
						{
							checkParts[nCheckParts].bbox = boxrope;
						}
						boxrope.bOriented++;
						if (box_box_overlap_check(&boxrope, &checkParts[nCheckParts].bbox,
						                          &Overlapper))
						{
							checkParts[nCheckParts].pent = pentlist[i];
							checkParts[nCheckParts].ipart = j;
							checkParts[nCheckParts].R = boxrope.Basis;
							checkParts[nCheckParts].scale = pentlist[i]->m_parts[j].scale;
							iend = pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom->GetType();
							if (pentlist[i] != m_pTiedTo[0] &&
							    pentlist[i] != m_pTiedTo[1] &&
							    (iend == GEOM_CYLINDER || iend == GEOM_CAPSULE ||
							     iend == GEOM_BOX))
							{
								n = (m_segs[iseg = (m_pTiedTo[1] == 0 ? 0 : m_nSegs)]
								         .pt -
								     checkParts[nCheckParts].offset) *
								    checkParts[nCheckParts].R;
								gwd.scale = checkParts[nCheckParts].scale;
								if (pentlist[i]
								            ->m_parts[j]
								            .pPhysGeomProxy->pGeom->FindClosestPoint(
										&gwd, iend, iend, n, n, ptres) >= 0 &&
								    ((a = (ptres[1] - ptres[0]) *
								          (ptres[0] -
								           checkParts[nCheckParts].bbox.center *
								               gwd.scale)) < 0 ||
								     (ptres[0] - ptres[1]).len2() <
								         sqr(m_collDist * 1.2f)))
								{
									n = ptres[0] +
									    (ptres[1] - ptres[0]).normalized() *
									        (m_collDist * 1.2f * sgnnz(a));
									m_segs[iseg].pt =
									    checkParts[nCheckParts].R * n +
									    checkParts[nCheckParts].offset;
									// continue;
								}
							}
							pentlist[i]
							    ->m_parts[j]
							    .pPhysGeomProxy->pGeom->PrepareForRayTest(
								pentlist[i]->m_parts[j].scale == 1.0f
								    ? seglen
								    : seglen / pentlist[i]->m_parts[j].scale);
							checkParts[nCheckParts].rscale =
							    checkParts[nCheckParts].scale == 1.0f
								? 1.0f
								: 1.0f / checkParts[nCheckParts].scale;
							if (m_flags & rope_subdivide_segs)
							{
								pentlist[i]->GetStatus(&sp);
								checkParts[nCheckParts].pos0 = sp.pos;
								checkParts[nCheckParts].q0 = sp.q;
							}
							else
							{
								checkParts[nCheckParts].pos0 = pentlist[i]->m_pos;
								checkParts[nCheckParts].q0 = pentlist[i]->m_qrot;
							}
							checkParts[nCheckParts].pGeom =
							    (CGeometry*)pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom;
							checkParts[nCheckParts].bProcess = 1;
							if (++nCheckParts == sizeof(checkParts) / sizeof(checkParts[0]))
							{
								goto enoughgeoms;
							}
						}
					}
				}
			}
		}
	enoughgeoms:

		if (!(m_flags & rope_subdivide_segs))
		{
			if (m_pTiedTo[0] && m_pTiedTo[1])
			{
				iDir = isneg(m_pTiedTo[1]->GetRigidBody(m_iTiedPart[1])->Minv -
				             m_pTiedTo[0]->GetRigidBody(m_iTiedPart[0])->Minv);
			}
			else
			{
				iDir = iszero((intptr_t)m_pTiedTo[0]);
			}
			iDir = 1 - (iDir * 2);
			iStart = m_nSegs & iDir >> 31;
			iEnd = m_nSegs & -iDir >> 31;
			for (i = iStart; i != iEnd; i += iDir)
			{
				iseg = i + (iDir >> 31);
				pent = m_segs[iseg].pContactEnt;
				if (pent)
				{
					gwd.R = Matrix33(pent->m_qrot * pent->m_parts[m_segs[iseg].iContactPart].q);
					gwd.offset =
					    pent->m_pos + pent->m_qrot * pent->m_parts[m_segs[iseg].iContactPart].pos;
					gwd.scale = pent->m_parts[m_segs[iseg].iContactPart].scale;
					if (!m_segs[iseg].bRecheckContact &&
					    pent->m_nParts > m_segs[iseg].iContactPart &&
					    pent->m_parts[m_segs[iseg].iContactPart]
					            .pPhysGeomProxy->pGeom->FindClosestPoint(
							&gwd, m_segs[iseg].iPrim, m_segs[iseg].iFeature,
							m_segs[i + iDir].pt, m_segs[i].pt, ptres) >= 0 &&
					    (dist2 = (n = ptres[1] - ptres[0]).len2()) <
					        sqr(m_collDist * 2) && // drop contact when gap becomes too big
					    sqr_signed(m_segs[iseg].vreq = n * m_segs[iseg].ncontact) >
					        n.len2() * sqr(0.75f)) // drop contact if normal changes abruptly
					{
						n.normalize();
						m_segs[i].tcontact = (ptres[1] - m_segs[iseg].pt).len() * rseglen;
						if (dist2 < sqr(m_collDist * 0.85f))
						{ // reinforce the distance
							t = (iDir >> 31 & 1) +
							    (m_segs[i].tcontact * iDir); // 1.0-t for iDir==-1
							j = isneg(t - 0.1f);
							t += j;
							j = -j & iDir;
							if ((unsigned int)i - j <= (unsigned int)m_nSegs)
							{
								rotax = (m_segs[i + iDir - j].pt - m_segs[i - j].pt ^ n)
								            .normalized();
								angle = (m_collDist - sqrt_tpl(dist2)) / (t * seglen);
								m_segs[i + iDir - j].pt =
								    m_segs[i + iDir - j].pt.GetRotated(
									m_segs[i - j].pt, rotax, cos_tpl(angle),
									sin_tpl(angle));
								m_segs[i + iDir - j].bRecalcDir =
								    m_segs[max(0, i + iDir - j - 1)].bRecalcDir = 1;
							}
							m_segs[iseg].vreq = 0;
						}
						else
						{
							m_segs[iseg].vreq = (m_collDist - m_segs[iseg].vreq) * 10.0f;
						}
						m_segs[i].ncontact = n;
						pbody =
						    m_segs[iseg].pContactEnt->GetRigidBody(m_segs[iseg].iContactPart);
						m_segs[iseg].vcontact = pbody->v + (pbody->w ^ ptres[0] - pbody->pos);
						m_bHasContacts = 1;
					}
					else
					{
						m_segs[iseg].pContactEnt->Release();
						m_segs[iseg].pContactEnt = 0;
					}
				}

				if (!m_segs[iseg].pContactEnt || m_segs[iseg].tcontact < 0.1f && m_nSegs <= 8)
				{
					for (j = nLocChecks = 0; j < nCheckParts && nLocChecks < 6; j++)
					{
						aray.m_ray.origin =
						    (m_segs[i].pt - checkParts[j].offset) * checkParts[j].R;
						aray.m_dirn = aray.m_ray.dir =
						    (m_segs[i + iDir].pt - checkParts[j].offset) * checkParts[j].R -
						    aray.m_ray.origin;
						if (box_ray_overlap_check(&checkParts[j].bbox, &aray.m_ray, nullptr))
						{
							ip.centerOfRotation = aray.m_ray.origin;
							ip.axisOfRotation.zero();
							gwd.offset.zero();
							gwd.R.SetIdentity();
							gwd.scale = checkParts[j].scale;
							if (checkParts[j]
							        .pent->m_parts[checkParts[j].ipart]
							        .pPhysGeomProxy->pGeom->Intersect(&aray, &gwd, 0, &ip,
							                                          pcontact))
							{
								WriteLockCond lockColl(*ip.plock, 0);
								lockColl.SetActive();
								if (pcontact->iUnprojMode == 1)
								{
									if (m_segs[iseg].pContactEnt)
									{
										m_segs[iseg].pContactEnt->Release();
									}
									m_segs[iseg].pContactEnt = 0;
									diff = m_segs[iseg].tcontact =
									    (pcontact->pt - aray.m_ray.origin).len();
									m_segs[iseg].tcontact =
									    (m_segs[iseg].tcontact * iDir) -
									    (seglen * (iDir >> 31));
									iend = -iseg >> 31 & 1;
									if (m_bTargetPoseActive == 0 &&
									    (unsigned int)(iseg - 1) >=
									        (unsigned int)(m_nSegs - 2) &&
									    m_pTiedTo[iend] &&
									    isneg((m_segs[iseg].tcontact * rseglen) -
									          0.2f - (0.6f * iend)) ^
									        iend)
									{
										continue;
									}
									(m_segs[iseg].pContactEnt = checkParts[j].pent)
									    ->AddRef();
									m_segs[iseg].iContactPart = checkParts[j].ipart;
									rotax = checkParts[j].R * -pcontact->dir;
									pcontact->t +=
									    min((float)pcontact->t, m_collDist / diff);
									if (pcontact->t > (real)0.5)
									{
										pcontact->t = (real)0.5;
										m_segs[iseg].bRecheckContact = 1;
									}
									else
									{
										m_segs[iseg].bRecheckContact = 0;
									}
									cost = cos_tpl(pcontact->t);
									sint = sin_tpl(pcontact->t);
									m_segs[iseg].ncontact =
									    checkParts[j].R *
									    (pcontact->n.normalized().GetRotated(
										pcontact->dir, cost, -sint));
									m_segs[iseg].tcontact *= rseglen;
									m_segs[i + iDir].pt =
									    m_segs[i + iDir].pt.GetRotated(
										m_segs[i].pt, rotax, cost, sint);
									m_segs[iseg].bRecheckContact = isnonneg(
									    (m_segs[i + iDir].pt - m_segs[i].pt) *
									    m_segs[iseg].ncontact);
									aray.m_ray.dir = (m_segs[i + iDir].pt -
									                  checkParts[j].offset) *
									                     checkParts[j].R -
									                 aray.m_ray.origin;
									m_segs[iseg].iPrim = pcontact->iPrim[0];
									m_segs[iseg].iFeature = pcontact->iFeature[0];
									m_segs[iseg].friction[0] =
									    m_segs[iseg].friction[1] = m_friction;
									pbody = m_segs[iseg].pContactEnt->GetRigidBody(
									    m_segs[iseg].iContactPart);
									m_segs[iseg].vcontact =
									    pbody->v +
									    (pbody->w ^ pcontact->pt - pbody->pos);
									m_segs[i + iDir].bRecalcDir =
									    m_segs[max(0, i + iDir - 1)].bRecalcDir = 1;
									m_segs[iseg].vreq = 0;
									m_bHasContacts = 1;
								}
							}
						}
					}
				}
			}
		}
		else
		{
			int i1, j1, iMoveEnd[2], ncont, bIncomplete[2], bStrainRechecked = 0;
			float tmax[2];
			Vec3 dirUnproj[2];
			rope_vtx vtxBest;
			geom_world_data gwd1;
			WriteLock lockVtx(m_lockVtx);

			if (m_idConstraint && m_pTiedTo[m_iConstraintClient])
			{
				pe_action_update_constraint arc;
				arc.idConstraint = m_idConstraint;
				arc.bRemove = 1;
				m_pTiedTo[m_iConstraintClient]->Action(&arc);
				m_idConstraint = 0;
			}

			for (i = 0; i < 2; i++)
			{
				if (m_pTiedTo[i])
				{
					m_pTiedTo[i]->RemoveCollider(this);
				}
			}
			if (m_bStrained)
			{
				for (i = 1; i < m_nVtx - 1; i++)
				{
					if (m_vtx[i].pContactEnt)
					{
						m_vtx[i].pContactEnt->RemoveCollider(this);
					}
				}
			}
			for (i = 0; i < m_nColliders; i++)
			{
				m_pColliders[i]->Release();
			}
			m_nColliders = 0;

			if (!m_idx)
			{
				m_idx = new int[m_nMaxSubVtx + 2];
			}

		RecheckAfterStrain:
			ip.bNoAreaContacts = true;
			ip.bStopAtFirstTri = false;
			ip.iUnprojectionMode = -1;
			ip.vrel_min = 1e-6f;
			aray.m_iCollPriority = 0;
			if (m_pTiedTo[0])
			{
				iStart = 1;
				MARK_UNUSED m_segs[0].ncontact;
				m_segs[0].pContactEnt = 0;
			}
			else
			{
				iStart = 0;
			}
			if (m_pTiedTo[1])
			{
				iEnd = m_nSegs - 1;
				MARK_UNUSED m_segs[m_nSegs].ncontact;
				m_segs[m_nSegs].pContactEnt = 0;
			}
			else
			{
				iEnd = m_nSegs;
			}

			for (i = iStart; i <= iEnd; i++)
			{
				if (m_segs[i].pContactEnt && !is_unused(m_segs[i].ncontact))
				{
					pbody = m_segs[i].pContactEnt->GetRigidBody(m_segs[i].iContactPart);
					dv = pbody->v + (pbody->w ^ m_segs[i].pt - pbody->pos);
					if ((m_segs[i].vel - dv) * m_segs[i].ncontact < 0.1f)
					{
						aray.m_ray.origin.zero();
						gwd1.offset = m_segs[i].pt;
						aray.m_ray.dir =
						    (aray.m_dirn = -m_segs[i].ncontact) * m_collDist * 1.5f;
						gwd.offset =
						    m_segs[i].pContactEnt->m_pos +
						    m_segs[i].pContactEnt->m_qrot *
							m_segs[i].pContactEnt->m_parts[m_segs[i].iContactPart].pos;
						gwd.R =
						    Matrix33(m_segs[i].pContactEnt->m_qrot *
						             m_segs[i].pContactEnt->m_parts[m_segs[i].iContactPart].q);
						gwd.scale =
						    m_segs[i].pContactEnt->m_parts[m_segs[i].iContactPart].scale;
						if (m_segs[i]
						        .pContactEnt->m_parts[m_segs[i].iContactPart]
						        .pPhysGeomProxy->pGeom->Intersect(&aray, &gwd, &gwd1, &ip,
						                                          pcontact))
						{
							WriteLockCond lockColl(*ip.plock, 0);
							lockColl.SetActive();
							m_segs[i].pt = pcontact->pt +
							               (m_segs[i].ncontact = pcontact->n) * m_collDist;
							m_segs[i].vcontact = dv;
						}
						else
						{
							MARK_UNUSED m_segs[i].ncontact;
						}
					}
					else
					{
						MARK_UNUSED m_segs[i].ncontact;
					}
				}
				else
				{
					MARK_UNUSED m_segs[i].ncontact;
				}

				for (j = 0; j < nCheckParts; j++)
				{
					if (checkParts[j].pent->m_iSimClass != 3)
					{
						center = (checkParts[j].pent->m_qrot * !checkParts[j].q0) *
						             (m_segs[i].pt0 - checkParts[j].pos0) +
						         checkParts[j].pent->m_pos;
						aray.m_ray.origin = (center - checkParts[j].offset) * checkParts[j].R;
						aray.m_ray.dir =
						    (m_segs[i].pt - checkParts[j].offset) * checkParts[j].R -
						    aray.m_ray.origin;
						aray.m_ray.dir +=
						    (aray.m_dirn = aray.m_ray.dir.normalized()) * m_collDist;
						if (box_ray_overlap_check(&checkParts[j].bbox, &aray.m_ray, nullptr))
						{
							gwd.offset.zero();
							gwd.R.SetIdentity();
							gwd.scale = checkParts[j].scale;
							ncont = checkParts[j].pGeom->Intersect(&aray, &gwd, 0, &ip,
							                                       pcontact);
							if (ncont)
							{
								WriteLockCond lockColl(*ip.plock, 0);
								lockColl.SetActive();
								for (i1 = ncont - 1;
								     i1 >= 0 && pcontact[i1].n * aray.m_dirn > 0; i1--)
									;
								if (i1 >= 0)
								{
									m_segs[i].pt = checkParts[j].R *
									                   (pcontact[i1].pt -
									                    aray.m_dirn * m_collDist) +
									               checkParts[j].offset;
									m_segs[i].ncontact =
									    checkParts[j].R * pcontact[i1].n;
									m_segs[i].pContactEnt = checkParts[j].pent;
									m_segs[i].iContactPart = checkParts[j].ipart;
									pbody = checkParts[j].pent->GetRigidBody(
									    checkParts[j].ipart);
									m_segs[i].vcontact =
									    pbody->v +
									    (pbody->w ^ m_segs[i].pt - pbody->pos);
								}
							}
						}
					}
					else
					{
						contact acontact;
						if (checkParts[j].pGeom->UnprojectSphere(
							((m_segs[i].pt - checkParts[j].offset) * checkParts[j].R) *
							    checkParts[j].rscale,
							m_collDist * checkParts[j].rscale,
							m_collDist * 1.3f * checkParts[j].rscale, &acontact))
						{
							m_segs[i].pt =
							    checkParts[j].R * (acontact.pt * checkParts[j].scale +
							                       acontact.n * m_collDist) +
							    checkParts[j].offset;
							m_segs[i].ncontact = checkParts[j].R * acontact.n;
							m_segs[i].pContactEnt = checkParts[j].pent;
							m_segs[i].iContactPart = checkParts[j].ipart;
							pbody = checkParts[j].pent->GetRigidBody(checkParts[j].ipart);
							m_segs[i].vcontact =
							    pbody->v + (pbody->w ^ m_segs[i].pt - pbody->pos);
						}
					}
				}
			}
			ip.iUnprojectionMode = 0;
			ip.vrel_min = 0;
			aray.m_iCollPriority = 10;

			for (i = m_nVtx = 0; i < m_nSegs; i++)
			{
				AllocSubVtx();
				m_vtx[m_segs[i].iVtx0 = m_nVtx++].pt = m_segs[i].pt;
				m_vtx[m_segs[i].iVtx0].vel = m_segs[i].vel;
				m_vtx[m_segs[i].iVtx0].ncontact = m_segs[i].ncontact;
				m_vtx[m_segs[i].iVtx0].vcontact = m_segs[i].vcontact;
				m_vtx[m_segs[i].iVtx0].pContactEnt = m_segs[i].pContactEnt;
				m_vtx[m_segs[i].iVtx0].iContactPart = m_segs[i].iContactPart;
				AllocSubVtx();
				m_nVtx++;
				AllocSubVtx();
				m_nVtx++;

				for (j = 0; j < nCheckParts; j++)
				{
					aray.m_ray.origin = (m_segs[i].pt - checkParts[j].offset) * checkParts[j].R;
					aray.m_ray.dir = (m_segs[i + 1].pt - checkParts[j].offset) * checkParts[j].R -
					                 aray.m_ray.origin;
					checkParts[j].bProcess =
					    box_ray_overlap_check(&checkParts[j].bbox, &aray.m_ray, nullptr);
				}

				aray.m_ray.origin.zero();
				gwd1.offset = m_segs[i].pt;
				m_segs[i].dir = aray.m_dirn =
				    (aray.m_ray.dir = m_segs[i + 1].pt - m_segs[i].pt).normalized();
				if (i == 0 && m_pTiedTo[0])
				{
					aray.m_ray.origin += (aray.m_ray.dir *= 0.5f);
				}
				else if (i == m_nSegs - 1 && m_pTiedTo[1])
				{
					aray.m_ray.dir *= 0.5f;
				}
				if (m_segs[i].ptdst.len2() > 0)
				{
					gwd1.v = -m_segs[i].ptdst;
				}
				else
				{
					gwd1.v = m_segs[max(0, i - 1)].pt - m_segs[i].pt;
					gwd1.v -= aray.m_dirn * (aray.m_dirn * gwd1.v);
					dir = m_segs[min(m_nSegs, i + 2)].pt - m_segs[i + 1].pt;
					dir -= aray.m_dirn * (aray.m_dirn * dir);
					gwd1.v += dir;
					if (gwd1.v.len2() > sqr(seglen * 0.1f))
					{
						gwd1.v.normalize();
					}
					else
					{
						gwd1.v.zero();
					}
				}
				ip.time_interval = ip.maxUnproj = seglen * 3;
				iMoveEnd[0] = iMoveEnd[1] = -1;

				for (i1 = 0, tmax[0] = tmax[1] = 0, bIncomplete[0] = bIncomplete[1] = 1; i1 < 2; i1++)
				{
					for (j = 0, dir.zero(); j < nCheckParts; j++)
					{
						if (checkParts[j].bProcess)
						{
							gwd.offset = checkParts[j].offset;
							gwd.R = checkParts[j].R;
							gwd.scale = checkParts[j].scale;
							ncont = checkParts[j].pGeom->Intersect(&aray, &gwd, &gwd1, &ip,
							                                       pcontact);
							if (ncont)
							{
								WriteLockCond lockColl(*ip.plock, 0);
								lockColl.SetActive();
								if (pcontact->iUnprojMode == 0 &&
								    pcontact->t > tmax[i1])
								{
									FillVtxContactData(m_vtx + m_nVtx - 2 + i1, i,
									                   checkParts[j], pcontact);
									tmax[i1] = pcontact->t;
									dirUnproj[i1] = pcontact->dir;
									if (!(pcontact->iFeature[1] & 0x20))
									{
										iMoveEnd[i1] = min(
										    1, pcontact->iFeature[1] & 0x1F);
									}
									bIncomplete[i1] =
									    pcontact->n * pcontact->dir > 0;
								}
							}
						}
					}
					if (gwd1.v.len2() == 0)
					{
						gwd1.v = dirUnproj[i1];
					}
					gwd1.v.Flip();
				}
				if (tmax[0] + tmax[1] == 0)
				{
					m_nVtx -= 2;
					continue;
				}
				if (tmax[1] > 0 && (tmax[1] < tmax[0] * 0.5f && bIncomplete[0] == bIncomplete[1] ||
				                    bIncomplete[0] && !bIncomplete[1]))
				{
					m_vtx[m_nVtx - 2] = m_vtx[m_nVtx - 1];
					iMoveEnd[0] = iMoveEnd[1];
					tmax[0] = tmax[1];
					dirUnproj[0] = dirUnproj[1];
				}
				if (iMoveEnd[0] >= 0)
				{
					m_segs[i + iMoveEnd[0]].pt -= dirUnproj[0] * tmax[0];
					m_segs[i].dir = (m_segs[i + 1].pt - m_segs[i].pt).normalized();
					m_vtx[m_nVtx - 3].pt = m_segs[i].pt;
				}
				m_vtx[m_nVtx - 1].pt = m_segs[i + 1].pt;
				dir = dirUnproj[0];

				for (iter = 0; m_nVtx - m_segs[i].iVtx0 - 2 < m_nMaxSubVtx && iter < 3; iter++)
				{
					for (i1 = 0; i1 < m_nVtx - m_segs[i].iVtx0 - 1; i1++)
					{
						m_idx[i1] = m_segs[i].iVtx0 + i1;
					}
					for (i1 = 0; i1 < m_nVtx - m_segs[i].iVtx0 - 2; i1++)
					{
						for (j1 = m_nVtx - m_segs[i].iVtx0 - 2; j1 > i1; j1--)
						{
							if ((m_vtx[m_idx[j1] + 1].pt - m_vtx[m_idx[j1]].pt).len2() >
							    (m_vtx[m_idx[j1 - 1] + 1].pt - m_vtx[m_idx[j1 - 1]].pt)
							        .len2())
							{
								j = m_idx[j1 - 1], m_idx[j1 - 1] = m_idx[j1],
								m_idx[j1] = j;
							}
						}
					}
					for (i1 = 0; i1 < m_nVtx - m_segs[i].iVtx0 - 1; i1++)
					{
						// unproject the next longest subseg until some unprojection is found
						aray.m_dirn =
						    (aray.m_ray.dir = m_vtx[m_idx[i1] + 1].pt - m_vtx[m_idx[i1]].pt)
							.normalized();
						gwd1.offset = m_vtx[m_idx[i1]].pt;
						gwd1.v = ((aray.m_dirn ^ dir) ^ aray.m_dirn).normalized();
						for (j = 0, tmax[0] = 0; j < nCheckParts; j++)
						{
							if (checkParts[j].bProcess)
							{
								gwd.offset = checkParts[j].offset;
								gwd.R = checkParts[j].R;
								gwd.scale = checkParts[j].scale;
								if (checkParts[j].pGeom->Intersect(&aray, &gwd, &gwd1,
								                                   &ip, pcontact))
								{
									WriteLockCond lockColl(*ip.plock, 0);
									lockColl.SetActive();
									float rng[2] = {
									    seglen * (1.5f * isneg(i - iStart) - 1),
									    seglen * (2 - 1.5f * isneg(iEnd - 1 - i))};
									if (pcontact->t > tmax[0] &&
									    inrange(m_segs[i].dir *
									                (pcontact->pt - m_segs[i].pt),
									            rng[0], rng[1]))
									{
										FillVtxContactData(&vtxBest, i,
										                   checkParts[j],
										                   pcontact),
										    tmax[0] = pcontact->t;
									}
								}
							}
						}
						if (tmax[0] > 0)
						{
							// if a new point is close to an existing one, replace the old
							// one (but if the old one is an end, skip the new)
							j1 = m_idx[i1] + 1;
							if ((m_vtx[m_idx[i1]].pt - vtxBest.pt).len2() <
							    sqr(seglen * 0.05f))
							{
								if (m_idx[i1] == m_segs[i].iVtx0)
								{
									continue;
								}
								j1 = m_idx[i1];
							}
							else if ((m_vtx[m_idx[i1] + 1].pt - vtxBest.pt).len2() >
							         sqr(seglen * 0.05f))
							{
								AllocSubVtx();
								m_nVtx++;
								iter = 0;
								memmove(m_vtx + m_idx[i1] + 2, m_vtx + m_idx[i1] + 1,
								        (m_nVtx - m_idx[i1] - 2) * sizeof(rope_vtx));
							}
							else if (m_idx[i1] == m_nVtx - 2)
							{
								continue;
							}
							m_vtx[j1] = vtxBest;
							break;
						}
					}
					if (i1 == m_nVtx - m_segs[i].iVtx0 - 1)
					{
						break;
					}
				}
				m_nVtx--;
			}
			m_vtx[m_segs[i].iVtx0 = m_nVtx++].pt = m_segs[i].pt;
			m_vtx[m_segs[i].iVtx0].vel = m_segs[i].vel;
			m_vtx[m_segs[i].iVtx0].ncontact = m_segs[i].ncontact;
			m_vtx[m_segs[i].iVtx0].vcontact = m_segs[i].vcontact;
			m_vtx[m_segs[i].iVtx0].pContactEnt = m_segs[i].pContactEnt;
			m_vtx[m_segs[i].iVtx0].iContactPart = m_segs[i].iContactPart;
			for (i = 0; i < m_nVtx - 1; i++)
			{
				m_vtx[i].dir = (m_vtx[i + 1].pt - m_vtx[i].pt).normalized();
			}
			m_vtx[i].dir = m_vtx[max(0, i - 1)].dir;
			for (i = 0; i < m_nVtx; i++)
			{
				m_bHasContacts |= !is_unused(m_vtx[i].ncontact);
			}
			for (i = 0; i < 2; i++)
			{
				if (m_pTiedTo[i])
				{
					m_vtx[i * (m_nVtx - 1)].pContactEnt = m_pTiedTo[i];
					m_vtx[i * (m_nVtx - 1)].iContactPart = m_iTiedPart[i];
					m_vtx[i * (m_nVtx - 1)].ncontact = m_vtx[i * (m_nVtx - 1)].dir * (1 - (i * 2));
				}
			}

			if (!m_pTiedTo[0] || !m_pTiedTo[1])
			{
				m_bStrained = 0;
			}
			else
			{
				for (i = 1; i < m_nVtx - 1; i++)
				{
					if (!is_unused(m_vtx[i].ncontact) &&
					    m_vtx[i].ncontact * (m_vtx[i - 1].dir - m_vtx[i].dir) < 0)
					{
						MARK_UNUSED m_vtx[i].ncontact;
						m_vtx[i].pContactEnt = 0;
					}
				}
				for (i = 0, m_bStrained = 1, len2 = 0; i < m_nVtx - 1;)
				{
					for (j = i++; i < m_nVtx - 1 && is_unused(m_vtx[i].ncontact); i++)
						;
					len2 += (m_vtx[i].pt - m_vtx[j].pt).len();
					dir = m_vtx[i - 1].dir - m_vtx[i].dir;
					if (dir.len2() > sqr(0.3f))
					{
						Vec3 axis = m_vtx[i - 1].dir ^ m_vtx[i].dir;
						n = m_vtx[i].ncontact;
						if ((a = axis.len2()) > sqr(0.2f))
						{
							n -= axis * (n * axis);
						}
						else
						{
							a = 1.0f;
						}
						if (i < m_nVtx - 1 && sqr_signed(n * dir) * (1 + sqr(m_friction)) <
						                          dir.len2() * n.len2() * sqr(a))
						{
							m_bStrained = 0;
						}
					}
				}
				m_bStrained &= isneg(m_length - len2);
				if (m_bStrained)
				{
					for (a = 0, i = j = ncont = 0, seglen = len2 / m_nSegs, iEnd = m_nVtx - 1,
					    m_nVtx = 0;
					     i < iEnd; ncont++)
					{
						for (iStart = i++; is_unused(m_vtx[i].ncontact) && i < iEnd; i++)
							;
						dir = m_vtx[i].pt - m_vtx[iStart].pt;
						dir /= (b = dir.len());
						if (m_nVtx > 0)
						{
							m_vtx1[m_nVtx - 1].dir = dir;
						}
						for (; a < b + (seglen * 0.01f) && j <= m_nSegs; m_nVtx++, a += seglen)
						{
							AllocSubVtx();
							m_vtx1[m_nVtx].pt =
							    m_vtx[iStart].pt + (m_vtx1[m_nVtx].dir = dir) * a;
							MARK_UNUSED m_vtx1[m_nVtx].ncontact;
							m_vtx1[m_nVtx].pContactEnt = 0;
							m_vtx1[m_nVtx].vel.zero();
							m_segs[j++].iVtx0 = m_nVtx;
						}
						m_nVtx -= isneg((seglen * 0.98f) - (a -= b));
						AllocSubVtx();
						m_vtx1[m_nVtx++] = m_vtx[i];
					}
					rope_vtx* pvtx = m_vtx;
					m_vtx = m_vtx1;
					m_vtx1 = pvtx;
					for (i = 1; i < m_nSegs; i++)
					{
						m_segs[i].ncontact = m_vtx[m_segs[i].iVtx0].ncontact;
						m_segs[i].pContactEnt = m_vtx[m_segs[i].iVtx0].pContactEnt;
						m_segs[i].iContactPart = m_vtx[m_segs[i].iVtx0].iContactPart;
					}
					if (ncont < m_nFragments && !bStrainRechecked)
					{
						m_nFragments = ncont;
						bStrainRechecked = 1;
						for (i = 0; i <= m_nSegs; i++)
						{
							m_segs[i].pt0 = m_segs[i].pt,
							m_segs[i].pt = m_vtx[m_segs[i].iVtx0].pt;
						}
						goto RecheckAfterStrain;
					}
					m_nFragments = ncont;
				}
			}
			if (m_bStrained)
			{
				m_pTiedTo[0]->AddCollider(this);
				AddCollider(m_pTiedTo[0]);
				m_pTiedTo[1]->AddCollider(this);
				AddCollider(m_pTiedTo[1]);
				for (i = 1; i < m_nVtx - 1; i++)
				{
					if (m_vtx[i].pContactEnt)
					{
						m_vtx[i].pContactEnt->AddCollider(this),
						    AddCollider(m_vtx[i].pContactEnt);
					}
				}
				if (m_pWorld->m_bWorldStep != 2)
				{
					int nIndep = 0, nRigid = 0;
					for (i = 0; i < m_nColliders; i++)
					{
						nIndep += (j = isneg(2 - m_pColliders[i]->m_iSimClass));
						nRigid += isneg(-m_pColliders[i]->m_iSimClass) - j;
					}
					if (nIndep)
					{
						m_pTiedTo[0]->Awake();
						m_pTiedTo[1]->Awake();
						if (!nRigid)
						{
							InitContactSolver(time_interval);
							RegisterContacts(time_interval, 1);
							InvokeContactSolver(time_interval, &m_pWorld->m_vars, 1E20f);
							for (entity_contact* pContact = m_pContact; pContact;
							     pContact = pContact->next)
							{
								for (j = 0; j < 2; j++)
								{
									if (pContact->pent[j]
									        ->m_parts[pContact->ipart[j]]
									        .flags &
									    geom_monitor_contacts)
									{
										pContact->pent[j]->OnContactResolved(
										    pContact, j, -1);
									}
								}
							}
							for (i = m_nColliders - 1; i >= 0; i--)
							{
								m_pColliders[i]->Update(time_interval, 1.0f);
							}
						}
					}
					Update(time_interval, 1.0f);
				}
			}
			else
			{
				for (i = 0; i <= m_nSegs; i++)
				{
					if (m_segs[i].pContactEnt)
					{
						m_segs[i].pContactEnt->m_flags |= pef_always_notify_on_deletion;
					}
				}
			}
			for (i = 0; i < m_nSegs; i++)
			{
				m_segs[i].bRecalcDir = 1;
			}
		}

		for (i = 0; i < m_nSegs; i++)
		{
			if (m_segs[i].bRecalcDir)
			{
				m_segs[i].dir = (m_segs[i + 1].pt - m_segs[i].pt).normalized();
			}
		}
	}
	else
	{
		for (i = 0; i <= m_nSegs; i++)
		{
			m_segs[i].pContactEnt = 0;
		}
	}

	for (i = 0, Ebefore = 0; i <= m_nSegs; i++)
	{
		Ebefore += m_segs[i].vel.len2();
	}

	if (!(m_flags & rope_no_solver))
	{
		if (m_bHasContacts || m_flags & rope_subdivide_segs)
		{
			FRAME_PROFILER("Rope solver MC", GetISystem(), PROFILE_PHYSICS);
			int bBounced;
			iter = 650;
			float vrel, dPtang;
			Vec3 dp;

			if (!(m_flags & rope_subdivide_segs))
			{
				for (i = 0; i < m_nSegs; i++)
				{
					m_segs[i].kdP = 0.5f, m_segs[i].dP = 0;
				}
				if (m_pTiedTo[0])
				{
					m_segs[0].kdP = 0.0f;
				}
				if (m_pTiedTo[1])
				{
					m_segs[m_nSegs - 1].kdP = 1.0f;
				}

				do
				{ // solve for velocities using relaxation solver with friction
					for (i = bBounced = 0; i < m_nSegs; i++, iter--)
					{
						if (fabsf(vrel = (m_segs[i + 1].vel - m_segs[i].vel) * m_segs[i].dir) >
						    seglen * 0.005f)
						{
							m_segs[i].vel += m_segs[i].dir * (vrel * m_segs[i].kdP);
							m_segs[i + 1].vel -=
							    m_segs[i].dir * (vrel * (1.0f - m_segs[i].kdP));
							bBounced++;
						}
						if (m_segs[i].pContactEnt)
						{
							dp = m_segs[i].vel * (1.0f - m_segs[i].tcontact) +
							     m_segs[i + 1].vel * m_segs[i].tcontact -
							     m_segs[i].vcontact;
							if ((vrel = dp * m_segs[i].ncontact - m_segs[i].vreq) <
							    -seglen * 0.005f)
							{
								if (m_segs[i].friction[0] > 0.01f)
								{
									m_segs[i].dP -= dPtang = sqrt_tpl(
									    max(0.0001f, dp.len2() - sqr(vrel)));
									if (m_segs[i].dP -
									        (vrel * m_segs[i].friction[0]) <
									    0)
									{ // friction cannot stop sliding
										dp +=
										    (dp - m_segs[i].ncontact * vrel) *
										    ((m_segs[i].dP -
										      vrel * m_segs[i].friction[1]) /
										     dPtang); // remove part of dp that
										              // friction cannot stop
										dp *=
										    vrel / (m_segs[i].ncontact *
										            dp); // apply impulse along
										                 // dp so that it stops
										                 // normal component
										m_segs[i].dP = 0;
									}
									else
									{
										m_segs[i].dP -=
										    vrel * m_segs[i].friction[0];
									}
								}
								else
								{
									dp = m_segs[i].ncontact * vrel;
								}
								m_segs[i].vel -=
								    dp * ((1.0f - m_segs[i].tcontact) * m_segs[i].kdP);
								m_segs[i + 1].vel -=
								    dp * (m_segs[i].tcontact * (1.0f - m_segs[i].kdP));
								bBounced += 4;
							}
						}
					}
				}
				while (bBounced && (iter -= bBounced) > 0);
			}
			else if (!m_bStrained)
			{
				float kdPs[3], e = m_pWorld->m_vars.accuracyMC, vreq;
				kdPs[0] = m_pTiedTo[0] ? 0.0f : 0.5f;
				kdPs[2] = m_pTiedTo[1] ? 1.0f : 0.5f;
				kdPs[1] = 0.5f;
				for (i = 0; i < m_nSegs; i++)
				{
					for (j = m_segs[i].iVtx0, a = 0; j < m_segs[i + 1].iVtx0; j++)
					{
						a += m_vtx[j].dir * (m_vtx[j + 1].pt - m_vtx[j].pt);
					}
					m_segs[i].vreq = m_stiffness * 0.5f * (m_length / (a * m_nSegs) - 1);
					Ebefore +=
					    sqr(m_segs[i].vreq) + (m_segs[i].vel.len() * fabs_tpl(m_segs[i].vreq) * 2);
				}
				for (i = 0; i < m_nVtx; i++)
				{
					m_vtx[i].dP = 0;
				}

				do
				{
					for (i = bBounced = iStart = 0; i < m_nVtx; i++, iter--)
					{
						if (i < m_nVtx - 1)
						{
							vreq = min(m_pWorld->m_vars.maxUnprojVel,
							           ((m_vtx[i + 1].pt - m_vtx[i].pt) * m_vtx[i].dir) *
							               m_segs[iStart].vreq);
							iStart += isneg(m_segs[iStart + 1].iVtx0 - 2 - i);
							if (fabs_tpl(vrel = (m_vtx[i + 1].vel - m_vtx[i].vel) *
							                        m_vtx[i].dir -
							                    vreq) *
							        isneg(i - m_nVtx + 1) >
							    e)
							{
								j = (i - 1) >> 31 | max(0, i - m_nVtx + 3);
								m_vtx[i].vel += m_vtx[i].dir * (vrel * kdPs[j + 1]);
								m_vtx[i + 1].vel -=
								    m_vtx[i].dir * (vrel * (1.0f - kdPs[j + 1]));
								bBounced++;
							}
						}
						if (!is_unused(m_vtx[i].ncontact))
						{
							dp = m_vtx[i].vel - m_vtx[i].vcontact;
							if ((vrel = dp * m_vtx[i].ncontact) < -e)
							{
								if (m_friction > 0.01f)
								{
									m_vtx[i].dP -= dPtang =
									    sqrt_tpl(max(0.0f, dp.len2() - sqr(vrel)));
									if (m_vtx[i].dP - (vrel * m_friction) < 0)
									{ // friction cannot stop sliding
										dp +=
										    (dp - m_vtx[i].ncontact * vrel) *
										    ((m_vtx[i].dP - vrel * m_friction) /
										     dPtang); // remove part of dp that
										              // friction cannot stop
										dp *=
										    vrel / (m_vtx[i].ncontact *
										            dp); // apply impulse along
										                 // dp so that it stops
										                 // normal component
										m_vtx[i].dP = 0;
									}
									else
									{
										m_vtx[i].dP -= vrel * m_friction;
									}
								}
								else
								{
									dp = m_vtx[i].ncontact * vrel;
								}
								m_vtx[i].vel -= dp;
								bBounced += 4;
							}
						}
					}
				}
				while (bBounced && (iter -= bBounced) > 0);

				if (m_vtx)
				{
					for (i = 0; i <= m_nSegs; i++)
					{
						m_segs[i].vel = m_vtx[m_segs[i].iVtx0].vel;
					}
				}
			}
		}
		else
		{
			FRAME_PROFILER("Rope solver CG", GetISystem(), PROFILE_PHYSICS);
			// solve for velocities using conjugate gradient
			for (i = 0, r2 = 0; i < m_nSegs; i++)
			{
				r2 += sqr(m_segs[i].dP = m_segs[i].r =
				              (m_segs[i + 1].vel - m_segs[i].vel) * m_segs[i].dir);
				m_segs[i].cosnext = m_segs[i].dir * m_segs[i + 1].dir;
				m_segs[i].kdP = 2.0f;
				m_segs[i].P = 0;
			}
			if (m_pTiedTo[0])
			{
				m_segs[0].kdP = 1.0f;
			}
			if (m_pTiedTo[1])
			{
				m_segs[m_nSegs - 1].kdP = 1.0f;
			}
			iter = m_nSegs * 3;

			do
			{
				m_segs[0].dv = m_segs[0].dP * m_segs[0].kdP;
				m_segs[1].dv = -m_segs[0].cosnext * m_segs[0].dP;
				for (i = 1; i < m_nSegs; i++)
				{
					m_segs[i - 1].dv -= m_segs[i - 1].cosnext * m_segs[i].dP;
					m_segs[i].dv += m_segs[i].dP * m_segs[i].kdP;
					m_segs[i + 1].dv = -m_segs[i].cosnext * m_segs[i].dP;
				}

				for (i = 0, pAp = 0; i < m_nSegs; i++)
				{
					pAp += m_segs[i].dv * m_segs[i].dP;
				}
				if (fabs_tpl(pAp) < 1E-10)
				{
					break;
				}
				a = r2 / pAp;
				for (i = 0, r2new = 0; i < m_nSegs; i++)
				{
					r2new += sqr(m_segs[i].r -= m_segs[i].dv * a);
					m_segs[i].P += m_segs[i].dP * a;
				}
				if (r2new > r2 * 500)
				{
					break;
				}
				b = r2new / r2;
				r2 = r2new;
				for (i = 0, vmax = 0; i < m_nSegs; i++)
				{
					(m_segs[i].dP *= b) += m_segs[i].r;
					vmax = max(vmax, sqr(m_segs[i].r));
				}
			}
			while (--iter && vmax > sqr(0.003f));

			if (!m_pTiedTo[0])
			{
				m_segs[0].vel += m_segs[0].dir * m_segs[0].P;
			}
			if (!m_pTiedTo[1])
			{
				m_segs[m_nSegs].vel -= m_segs[m_nSegs - 1].dir * m_segs[m_nSegs - 1].P;
			}
			m_segs[1].vel -= m_segs[0].dir * m_segs[0].P;
			m_segs[m_nSegs - 1].vel += m_segs[m_nSegs - 1].dir * m_segs[m_nSegs - 1].P;
			for (i = 1; i < m_nSegs - 1; i++)
			{
				m_segs[i].vel += m_segs[i].dir * m_segs[i].P;
				m_segs[i + 1].vel -= m_segs[i].dir * m_segs[i].P;
			}
		}
	}

	for (i = 0, E = 0; i <= m_nSegs; i++)
	{
		E += m_segs[i].vel.len2();
	}
	if (E > Ebefore && E > m_Emin)
	{
		k = sqrt_tpl(Ebefore / E);
		for (i = 0; i <= m_nSegs; i++)
		{
			m_segs[i].vel *= k;
		}
		E = Ebefore;
	}
	i = -isneg(E - (m_Emin * (m_nSegs + 1)));
	m_nSlowFrames = (m_nSlowFrames & i) - i;
	m_bAwake = isneg(m_nSlowFrames - 4) | (bTargetPoseActive & 1);
	i = isneg(0.0001f - m_maxTimeIdle); // forceful deactivation is turned on
	m_bAwake &= (i ^ 1) | isneg((m_timeIdle += time_interval * i) - m_maxTimeIdle);
	if (m_pTiedTo[0] && m_pTiedTo[0]->GetType() == PE_ROPE)
	{
		m_bAwake |= m_pTiedTo[0]->IsAwake();
	}
	if (!m_bAwake)
	{
		for (i = iszero((intptr_t)m_pTiedTo[0]) ^ 1; i < m_nSegs + iszero((intptr_t)m_pTiedTo[1]); i++)
		{
			m_segs[i].vel.zero();
		}
	}

	if (m_flags & pef_traceable)
	{
		bGridLocked = m_pWorld->RepositionEntity(this, 1, BBox);
	}
	{
		WriteLock lock(m_lockUpdate);
		m_BBox[0] = BBox[0];
		m_BBox[1] = BBox[1];
		m_pos = m_segs[0].pt;
		for (i = 0; i <= m_nSegs; i++)
		{
			m_segs[i].pt0 = m_segs[i].pt;
		}
		AtomicAdd(&m_pWorld->m_lockGrid, -bGridLocked);
	}

	return isneg(m_timeStepFull - m_timeStepPerformed - 0.001f);
}

int CRopeEntity::RegisterContacts(float time_interval, int nMaxPlaneContacts)
{
	m_energy = 0;
	if (!m_bStrained)
	{
		return 1;
	}
	int i, j, iStart, iEnd, jStart, sg, nUniqueBodies;
	float friction, cos2a, cosa, sina, cosb, sinb, kP, len, rlen, sag, maxM;
	Vec3 n;
	RigidBody* pbody[3];
	entity_contact* pContact;
	g_bUsePreCG = false;

	if (!m_vtxSolver)
	{
		m_vtxSolver = new rope_solver_vtx[m_nVtxAlloc];
	}
	cosb = 1 / sqrt_tpl(1 + sqr(m_frictionPull));
	for (i = 0, len = 0; i < m_nVtx - 1; i++)
	{
		len += (m_vtx[i + 1].pt - m_vtx[i].pt) * m_vtx[i].dir;
	}
	rlen = 1.0f / len;
	sag = (m_length * 1.005f) - len;

	for (iStart = j = 0; iStart < m_nVtx - 1; iStart = iEnd)
	{
		pContact = (entity_contact*)AllocSolverTmpBuf(sizeof(entity_contact));

		for (i = iStart + 1, kP = 1, len = 0, sg = 0; i < m_nVtx - 1; i++)
		{
			len += m_vtx[i - 1].dir * (m_vtx[i].pt - m_vtx[i - 1].pt);
			if (!is_unused(m_vtx[i].ncontact))
			{
				if ((cos2a = -(m_vtx[i].dir * m_vtx[i - 1].dir)) > -0.99f)
				{
					pbody[2] = m_vtx[i].pContactEnt->GetRigidBody(m_vtx[i].iContactPart, 1);
					pbody[2]->Fcollision.zero();
					pbody[2]->Tcollision.zero();
					m_vtx[i].dP = 1;
					if ((1 - cos2a) < sqr(m_frictionPull) * (1 + cos2a))
					{
						break;
					}
					sg -=
					    sgn((m_vtx[i - 1].dir + m_vtx[i].dir) * (m_vtx[i].vel - m_vtx[i].vcontact));
				}
				else
				{
					m_vtx[i].dP = 0;
				}
			}
		}
		// TODO: switch to an infinite friction mdoe if we have too many high-friction contacts
		// with a non-static geom (or maybe not just non-static)
		friction = m_frictionPull * sgn(sg);
		len += m_vtx[i - 1].dir * (m_vtx[i].pt - m_vtx[i - 1].pt);
		iEnd = i;
		pContact->pbody[0] = pbody[0] =
		    iStart == 0
			? (pContact->pent[0] = m_pTiedTo[0])->GetRigidBody(pContact->ipart[0] = m_iTiedPart[0], 1)
			: (pContact->pent[0] = m_vtx[iStart].pContactEnt)
			      ->GetRigidBody(pContact->ipart[0] = m_vtx[iStart].iContactPart, 1);
		pContact->pbody[1] = pbody[1] =
		    iEnd == m_nVtx - 1
			? (pContact->pent[1] = m_pTiedTo[1])->GetRigidBody(pContact->ipart[1] = m_iTiedPart[1], 1)
			: (pContact->pent[1] = m_vtx[iEnd].pContactEnt)
			      ->GetRigidBody(pContact->ipart[1] = m_vtx[iEnd].iContactPart, 1);
		for (i = 0; i < 2; i++)
		{
			pbody[i]->Fcollision.zero(), pbody[i]->Tcollision.zero();
		}
		pContact->pt[0] = m_vtx[iStart].pt;
		pContact->pt[1] = m_vtx[iEnd].pt;
		pContact->K.SetZero();
		sinb = friction * cosb;
		pbody[0]->Fcollision += m_vtx[iStart].dir;
		pbody[0]->Tcollision += m_vtx[iStart].pt - pbody[0]->pos ^ m_vtx[iStart].dir;
		maxM = max(pbody[0]->M, pbody[1]->M);
		nUniqueBodies = (pbody[0] != pbody[1]) + 1;

		for (i = iStart + 1, kP = 1, jStart = j; i < iEnd; i++)
		{
			if (!is_unused(m_vtx[i].ncontact) && m_vtx[i].dP > 0)
			{
				cos2a = -(m_vtx[i].dir * m_vtx[i - 1].dir);
				cosa = sqrt_tpl((1 + cos2a) * 0.5f);
				sina = sqrt_tpl((1 - cos2a) * 0.5f);
				m_vtxSolver[j].ivtx = i;
				m_vtxSolver[j].v = m_vtx[i].dir - m_vtx[i - 1].dir;
				n = m_vtxSolver[j].v / -(2 * cosa);
				n += (m_vtx[i - 1].dir + m_vtx[i].dir) * (friction / (2 * sina));
				kP /= (sina * cosb) + (cosa * sinb);
				m_vtxSolver[j].P = n * (kP * -2 * sina * cosa * cosb);
				m_vtxSolver[j].pbody = pbody[2] =
				    m_vtx[i].pContactEnt->GetRigidBody(m_vtx[i].iContactPart, 1);
				maxM = max(maxM, pbody[2]->M);
				pbody[2]->Fcollision += m_vtxSolver[j].P;
				pbody[2]->Tcollision +=
				    (m_vtxSolver[j].r = m_vtx[i].pt - pbody[2]->pos) ^ m_vtxSolver[j].P;
				kP *= (sina * cosb) - (cosa * sinb);
				nUniqueBodies += pbody[2] != pbody[0] && pbody[2] != pbody[1];
				j++;
			}
		}
		pContact->K(0, 0) = kP;
		pbody[1]->Fcollision -= m_vtx[iEnd - 1].dir * kP;
		pbody[1]->Tcollision -= m_vtx[iEnd - 1].pt - pbody[1]->pos ^ m_vtx[iEnd - 1].dir;

		kP = -m_vtx[iEnd - 1].dir * (pbody[1]->Fcollision * pbody[1]->Minv +
		                             (pbody[1]->Iinv * pbody[1]->Tcollision ^ m_vtx[iEnd].pt - pbody[1]->pos));
		kP += m_vtx[iStart].dir * (pbody[0]->Fcollision * pbody[0]->Minv +
		                           (pbody[0]->Iinv * pbody[0]->Tcollision ^ m_vtx[iStart].pt - pbody[0]->pos));
		for (i = iStart + 1, j = jStart; i < iEnd; i++)
		{
			if (!is_unused(m_vtx[i].ncontact) && m_vtx[i].dP > 0 &&
			    (pbody[2] = m_vtx[i].pContactEnt->GetRigidBody(m_vtx[i].iContactPart, 1)))
			{
				kP += m_vtxSolver[j++].v *
				      (pbody[2]->Fcollision * pbody[2]->Minv +
				       (pbody[2]->Iinv * pbody[2]->Tcollision ^ m_vtx[i].pt - pbody[2]->pos));
			}
		}
		pContact->K(0, 1) = 1 / kP;

		pContact->flags = contact_rope;
		pContact->n = m_vtx[iStart].dir;
		pContact->vreq = -m_vtx[iEnd - 1].dir;
		pContact->friction = len * rlen *
		                     max(0.0f, min(m_pWorld->m_vars.maxUnprojVel,
		                                   -sag * len * rlen * m_pWorld->m_vars.unprojVelScale * 2));
		pContact->Pspare = 0;
		pContact->pBounceCount = (int*)(m_vtxSolver + jStart);
		pContact->iCount = j - jStart;
		if (nUniqueBodies > 1 && maxM > 0)
		{
			::RegisterContact(pContact);
			m_energy += maxM * sqr(pContact->friction);
			pContact->next = m_pContact;
			m_pContact = pContact;
		}
	}

	return 1;
}

int CRopeEntity::Update(float time_interval, float damping)
{
	if (m_bStrained)
	{
		int i, j, i0;
		float vrope[2], len, rlen = 1 / m_length, rseglen, t;
		Vec3 norm, v, tang, dir;
		RigidBody* pbody;

		for (i = 0; i < 2; i++)
		{
			pbody = m_pTiedTo[i]->GetRigidBody(m_iTiedPart[i]);
			vrope[i] = m_vtx[i * (m_nVtx - 2)].dir *
			           (m_vtx[i * (m_nVtx - 1)].vel =
			                pbody->v + (pbody->w ^ m_vtx[i * (m_nVtx - 1)].pt - pbody->pos));
		}
		for (i = 0; i < m_nVtx - 1;)
		{
			for (j = i + 1; j < m_nVtx - 1 && is_unused(m_vtx[j].ncontact); j++)
				;
			len = (dir = m_vtx[j].pt - m_vtx[i].pt).len();
			dir *= (rseglen = 1 / len);
			if (j < m_nVtx - 1)
			{
				pbody = m_vtx[j].pContactEnt->GetRigidBody(m_vtx[j].iContactPart);
				v = pbody->v + (pbody->w ^ m_vtx[j].pt - pbody->pos);
				norm = (m_vtx[j - 1].dir - m_vtx[j].dir).normalized();
				tang = (m_vtx[j - 1].dir + m_vtx[j].dir).normalized();
				m_vtx[j].vel = norm * (v * norm) +
				               tang * ((vrope[0] * (1 - len * rlen)) + (vrope[1] * (len * rlen)));
			}
			for (i0 = i++; i < j; i++)
			{
				t = (dir * (m_vtx[i].pt - m_vtx[i0].pt)) * rseglen;
				m_vtx[i].vel = m_vtx[i0].vel * (1 - t) + m_vtx[j].vel * t;
			}
		}
		for (i = 0; i <= m_nSegs; i++)
		{
			m_segs[i].vel = m_vtx[m_segs[i].iVtx0].vel;
		}

		for (len = 0; m_pContact; m_pContact = m_pContact->next)
		{
			len = max(len, m_pContact->Pspare);
			for (i = 0; i < m_pContact->iCount; i++)
			{
				j = ((rope_solver_vtx*)m_pContact->pBounceCount)[i].ivtx;
				if (m_vtx[j].pContactEnt &&
				    m_vtx[j].pContactEnt->m_parts[m_vtx[j].iContactPart].flags & geom_monitor_contacts)
				{
					m_vtx[j].pContactEnt->OnContactResolved(m_pContact, i + 2, -1);
				}
			}
		}
		if (m_maxForce > 0 && len > m_maxForce * max(0.01f, time_interval))
		{
			EventPhysJointBroken epjb;
			epjb.idJoint = 0;
			epjb.bJoint = 0;
			MARK_UNUSED epjb.pNewEntity[0], epjb.pNewEntity[1];
			epjb.pEntity[0] = epjb.pEntity[1] = this;
			epjb.pForeignData[0] = epjb.pForeignData[1] = m_pForeignData;
			epjb.iForeignData[0] = epjb.iForeignData[1] = m_iForeignData;
			epjb.pt = m_segs[m_nSegs].pt;
			epjb.n = m_segs[m_nSegs - 1].dir;
			epjb.partid[0] = 1;
			epjb.partid[1] = m_pTiedTo[1]->m_parts[m_iTiedPart[1]].id;
			epjb.partmat[0] = epjb.partmat[1] = -1;
			m_pWorld->OnEvent(2, &epjb);

			pe_params_rope pr;
			pr.pEntTiedTo[1] = 0;
			SetParams(&pr);
		}
	}
	m_pContact = 0;
	return 1;
}

int CRopeEntity::RayTrace(CRayGeom* pRay, geom_contact*& pcontacts, volatile int*& plock)
{
	static geom_contact g_RopeContact;
	static volatile int g_lock = 0;
	ReadLock lock(m_lockUpdate);
	Vec3 dp, dir, l, pt;
	strided_pointer<Vec3> vtx;
	float t, t1, llen2, raylen = pRay->m_ray.dir * pRay->m_dirn;
	int i, nSegs;
	if (m_nVtx == 0)
	{
		vtx = strided_pointer<Vec3>(&m_segs[0].pt0, sizeof(rope_segment));
		nSegs = m_nSegs;
	}
	else
	{
		vtx = strided_pointer<Vec3>(&m_vtx[0].pt, sizeof(rope_vtx));
		nSegs = m_nVtx - 1;
	}

	for (i = 0; i < nSegs; i++)
	{
		dp = pRay->m_ray.origin - vtx[i];
		if ((dp ^ pRay->m_dirn).len2() < sqr(m_collDist) &&
		    inrange((vtx[i] - pRay->m_ray.origin) * pRay->m_dirn, 0.0f, raylen))
		{
			pt = vtx[i];
			break;
		}
		dir = vtx[i + 1] - vtx[i];
		l = dir ^ pRay->m_dirn;
		llen2 = l.len2();
		t = (dp ^ pRay->m_dirn) * l;
		t1 = (dp ^ dir) * l;
		if (sqr(dp * l) < sqr(m_collDist) * llen2 && inrange(t, 0.0f, llen2) &&
		    inrange(t1, 0.0f, llen2 * raylen))
		{
			pt = vtx[i] + dir * (t / llen2);
			break;
		}
	}

	if (i < m_nSegs)
	{
		WriteLockCond lock(g_lock, 1);
		lock.SetActive(0);
		plock = &g_lock;
		pcontacts = &g_RopeContact;
		pcontacts->pt = pt;
		pcontacts->t = (pt - pRay->m_ray.origin) * pRay->m_dirn;
		pcontacts->id[0] = m_surface_idx;
		pcontacts->iNode[0] = i;
		pcontacts->n = -pRay->m_dirn;
		return 1;
	}
	return 0;
}

void CRopeEntity::ApplyVolumetricPressure(const Vec3& epicenter, float kr, float rmin)
{
	if (m_maxForce > 0 && m_pTiedTo[0] && m_pTiedTo[1])
	{
		int i;
		float dP;
		Vec3 r;
		for (i = 0, dP = 0; i < m_nSegs; i++)
		{
			r = (m_segs[i].pt + m_segs[i + 1].pt) * 0.5f - epicenter;
			dP += (m_segs[i].dir ^ r).len() / (r.len() * max(r.len2(), rmin * rmin));
		}
		if (dP * kr * m_length * m_collDist * 2 > m_maxForce * 0.01f)
		{
			EventPhysJointBroken epjb;
			epjb.idJoint = 0;
			epjb.bJoint = 0;
			MARK_UNUSED epjb.pNewEntity[0], epjb.pNewEntity[1];
			epjb.pEntity[0] = epjb.pEntity[1] = this;
			epjb.pForeignData[0] = epjb.pForeignData[1] = m_pForeignData;
			epjb.iForeignData[0] = epjb.iForeignData[1] = m_iForeignData;
			epjb.pt = m_segs[m_nSegs].pt;
			epjb.n = m_segs[m_nSegs - 1].dir;
			epjb.partid[0] =
			    isneg((m_segs[m_nSegs].pt - epicenter).len2() - (m_segs[0].pt - epicenter).len2());
			epjb.partid[1] = m_pTiedTo[epjb.partid[0]]->m_parts[m_iTiedPart[epjb.partid[0]]].id;
			epjb.partmat[0] = epjb.partmat[1] = -1;
			m_pWorld->OnEvent(2, &epjb);

			pe_params_rope pr;
			pr.pEntTiedTo[epjb.partid[0]] = 0;
			SetParams(&pr);
		}
		m_bAwake = 1;
	}
}

void CRopeEntity::OnNeighbourSplit(CPhysicalEntity* pentOrig, CPhysicalEntity* pentNew)
{
	int i, ipart;
	CPhysicalEntity* pent;
	for (i = 0; i < 2; i++)
	{
		if (m_pTiedTo[i] == pentOrig)
		{
			pe_params_rope pr;
			pent = pentNew ? pentNew : pentOrig;
			ipart = pent->TouchesSphere(m_segs[i * m_nSegs].pt, m_szSensor);
			if (ipart >= 0)
			{
				pr.pEntTiedTo[i] = pent;
				pr.idPartTiedTo[i] = pent->m_parts[ipart].id;
			}
			else if (!pentNew)
			{
				pr.pEntTiedTo[i] = 0;
			}
			else
			{
				continue;
			}
			SetParams(&pr);
			Awake();
		}
	}
}

int CRopeEntity::GetStateSnapshot(TSerialize ser, float time_back, int flags)
{
	int i;
	bool bAwake;
	ser.Value("nsegs", m_nSegs);

	if (m_nSegs <= 0)
	{
		bAwake = true;
		ser.Value("strained", bAwake);
		ser.Value("pos_start", m_pos);
		ser.Value("pos_end", m_pos);
	}
	else if (!m_bAwake && (m_segs[m_nSegs].pt - m_segs[0].pt).len2() > sqr(m_length))
	{
		bAwake = true;
		ser.Value("strained", bAwake);
		ser.Value("pos_start", m_segs[0].pt);
		ser.Value("pos_end", m_segs[m_nSegs].pt);
	}
	else
	{
		bAwake = false;
		ser.Value("strained", bAwake);
		bAwake = m_bAwake != 0;
		ser.Value("awake", bAwake);

		ser.BeginGroup("Segs");
		for (i = 0; i <= m_nSegs; i++)
		{
			ser.Value(numbered_tag("pos", i), m_segs[i].pt);
			if (m_bAwake)
			{
				ser.Value(numbered_tag("vel", i), m_segs[i].vel);
			}
		}
		ser.EndGroup();

		ser.Value("nvtx", m_nVtx);
		if (m_nVtx > 0)
		{
			ser.BeginGroup("Verts");
			for (i = 0; i < m_nVtx; i++)
			{
				ser.Value(numbered_tag("pos", i), m_vtx[i].pt);
				if (m_bAwake)
				{
					ser.Value(numbered_tag("vel", i), m_vtx[i].vel);
				}
			}
			ser.EndGroup();
		}

		ser.Value("pt_tied0", m_ptTiedLoc[0]);
		ser.Value("pt_tied1", m_ptTiedLoc[1]);
	}

	if (!m_bTargetPoseActive && (m_pTiedTo[0] && m_pTiedTo[1] ||
	                             m_pTiedTo[0] && m_pTiedTo[0]->m_iSimClass < 3 &&
	                                 (!m_pForeignData || m_pForeignData != m_pTiedTo[0]->m_pForeignData) ||
	                             m_pTiedTo[1] && m_pTiedTo[1]->m_iSimClass < 3 &&
	                                 (!m_pForeignData || m_pForeignData != m_pTiedTo[1]->m_pForeignData)))
	{
		bAwake = true;
		ser.Value("saveties", bAwake);
		for (i = 0; i < 2; i++)
		{
			ser.BeginGroup("link");
			if (m_pTiedTo[i])
			{
				bAwake = true;
				ser.Value("tied", bAwake);
				m_pWorld->SavePhysicalEntityPtr(ser, m_pTiedTo[i]);
				ser.Value("ipart", m_iTiedPart[i]);
			}
			else
			{
				bAwake = false;
				ser.Value("tied", bAwake);
			}
			ser.EndGroup();
		}
	}
	else
	{
		bAwake = false;
		ser.Value("saveties", bAwake);
	}

	return 1;
}

int CRopeEntity::SetStateFromSnapshot(TSerialize ser, int flags)
{
	int i;
	bool bAwake;
	int nSegs;

	ser.Value("nsegs", nSegs);
	if ((unsigned int)nSegs > 300u)
	{
		return 0;
	}

	if (m_nSegs != nSegs)
	{
		if (m_segs)
		{
			delete[] m_segs;
		}
		memset(m_segs = new rope_segment[nSegs + 1], 0, (nSegs + 1) * sizeof(rope_segment));
		m_nSegs = nSegs;
	}

	if (m_vtx)
	{
		delete[] m_vtx;
	}
	m_vtx = 0;
	if (m_vtx1)
	{
		delete[] m_vtx1;
	}
	m_vtx1 = 0;
	if (m_vtxSolver)
	{
		delete[] m_vtxSolver;
	}
	m_vtxSolver = 0;

	ser.Value("strained", bAwake);
	if (bAwake)
	{
		ser.Value("pos_start", m_segs[0].pt);
		ser.Value("pos_end", m_segs[m_nSegs].pt);
		Vec3 step = (m_segs[m_nSegs].pt - m_segs[0].pt) / m_nSegs;
		for (i = 1; i < m_nSegs; i++)
		{
			m_segs[i].pt0 = m_segs[i].pt = m_segs[0].pt + step * i, m_segs[i].vel.zero();
		}
		m_segs[0].vel.zero();
		m_segs[m_nSegs].vel.zero();

		if (m_flags & rope_subdivide_segs)
		{
			memset(m_vtx = new rope_vtx[m_nVtxAlloc = (m_nSegs + 1) * 2], 0,
			       (m_nSegs + 1) * sizeof(rope_vtx));
			m_vtx1 = new rope_vtx[m_nVtxAlloc];
			for (i = 0; i <= m_nSegs; i++)
			{
				m_vtx[i].pt = m_segs[i].pt, MARK_UNUSED m_vtx[i].ncontact;
			}
			m_nVtx = m_nSegs + 1;
		}
	}
	else
	{
		ser.Value("awake", bAwake);
		m_bAwake = bAwake;

		ser.BeginGroup("Segs");
		for (i = 0; i <= m_nSegs; i++)
		{
			ser.Value(numbered_tag("pos", i), m_segs[i].pt);
			m_segs[i].pt0 = m_segs[i].pt;
			if (m_bAwake)
			{
				ser.Value(numbered_tag("vel", i), m_segs[i].vel);
			}
			else
			{
				m_segs[i].vel.zero();
			}
			if (m_segs[i].pContactEnt)
			{
				if (!(m_flags & rope_subdivide_segs))
				{
					m_segs[i].pContactEnt->Release();
				}
				m_segs[i].pContactEnt = 0;
			}
			MARK_UNUSED m_segs[i].ncontact;
		}
		ser.EndGroup();

		ser.Value("nvtx", m_nVtx);
		if (m_nVtx > 0)
		{
			memset(m_vtx = new rope_vtx[m_nVtxAlloc = (m_nSegs + 1) * 2], 0, m_nVtx * sizeof(rope_vtx));
			memset(m_vtx1 = new rope_vtx[m_nVtxAlloc], 0, m_nVtx * sizeof(rope_vtx));
			m_vtxSolver = new rope_solver_vtx[m_nVtxAlloc];

			ser.BeginGroup("Verts");
			for (i = 0; i < m_nVtx; i++)
			{
				ser.Value(numbered_tag("pos", i), m_vtx[i].pt);
				if (m_bAwake)
				{
					ser.Value(numbered_tag("vel", i), m_vtx[i].vel);
				}
				MARK_UNUSED m_vtx[i].ncontact;
			}
			ser.EndGroup();
		}

		ser.Value("pt_tied0", m_ptTiedLoc[0]);
		ser.Value("pt_tied1", m_ptTiedLoc[1]);
	}

	ser.Value("saveties", bAwake);
	if (bAwake)
	{
		pe_params_rope pr;
		for (i = 0; i < 2; i++)
		{
			ser.BeginGroup("link");
			ser.Value("tied", bAwake);
			if (bAwake)
			{
				pr.pEntTiedTo[i] = m_pWorld->LoadPhysicalEntityPtr(ser);
				ser.Value("ipart", pr.idPartTiedTo[i]);
				if (pr.pEntTiedTo[i] && pr.pEntTiedTo[i]->GetType() != PE_ROPE)
				{
					pr.idPartTiedTo[i] =
					    ((CPhysicalEntity*)pr.pEntTiedTo[i])->m_parts[pr.idPartTiedTo[i]].id;
				}
			}
			else
			{
				pr.pEntTiedTo[i] = 0;
			}
			ser.EndGroup();
		}
		SetParams(&pr);
	}

	m_BBox[0] = m_BBox[1] = m_segs[m_nSegs].pt;
	for (i = 0; i < m_nSegs; i++)
	{
		m_segs[i].dir = (m_segs[i + 1].pt - m_segs[i].pt).normalized();
		m_BBox[0] = min(m_BBox[0], m_segs[i].pt);
		m_BBox[1] = max(m_BBox[1], m_segs[i].pt);
	}
	m_pos = m_segs[0].pt;
	if (m_flags & pef_traceable)
	{
		AtomicAdd(&m_pWorld->m_lockGrid, -m_pWorld->RepositionEntity(this, 1));
	}

	return 1;
}

void CRopeEntity::DrawHelperInformation(IPhysRenderer* pRenderer, int flags)
{
	CPhysicalEntity::DrawHelperInformation(pRenderer, flags);

	int i;
	flags |= 16;
	if (flags & pe_helper_geometry)
	{
		int nSegs;
		strided_pointer<Vec3> vtx;
		if (m_flags & rope_subdivide_segs && m_nVtx > 0)
		{
			vtx = strided_pointer<Vec3>(&m_vtx[0].pt, sizeof(rope_vtx));
			nSegs = m_nVtx - 1;
		}
		else
		{
			vtx = strided_pointer<Vec3>(&m_segs[0].pt0, sizeof(rope_segment));
			nSegs = m_nSegs;
		}
		if (flags & 16)
		{
			primitives::cylinder cyl;
			cyl.r = m_collDist;
			CCylinderGeom cylGeom;
			for (i = 0; i < nSegs; i++)
			{
				cyl.center = (vtx[i] + vtx[i + 1]) * 0.5f;
				cyl.axis = vtx[i + 1] - vtx[i];
				cyl.hh = cyl.axis.len();
				cyl.axis /= cyl.hh;
				cyl.hh *= 0.5f;
				cylGeom.CreateCylinder(&cyl);
				pRenderer->DrawGeometry(&cylGeom, 0, m_iSimClass);
			}
		}
		else
		{
			for (i = 0; i < nSegs; i++)
			{
				pRenderer->DrawLine(vtx[i], vtx[i + 1], m_iSimClass);
			}
		}
		if (m_flags & rope_subdivide_segs && flags & 16)
		{
			for (i = 0; i < m_nSegs; i++) // if (m_segs[i+1].iVtx0>m_segs[i].iVtx0+1)
			{
				pRenderer->DrawLine(m_segs[i].pt, m_segs[i + 1].pt, m_iSimClass);
			}
		}
	}
	if (flags & pe_helper_collisions)
	{
		if (!(m_flags & rope_subdivide_segs))
		{
			for (i = 0; i < m_nSegs; i++)
			{
				if (m_segs[i].pContactEnt)
				{
					Vec3 pt = m_segs[i].pt + (m_segs[i + 1].pt - m_segs[i].pt) * m_segs[i].tcontact;
					pRenderer->DrawLine(
					    pt, pt + m_segs[i].ncontact * m_pWorld->m_vars.maxContactGap * 30,
					    m_iSimClass);
				}
			}
		}
		else
		{
			for (i = 0; i < m_nVtx; i++)
			{
				if (!is_unused(m_vtx[i].ncontact))
				{
					pRenderer->DrawLine(m_vtx[i].pt,
					                    m_vtx[i].pt +
					                        m_vtx[i].ncontact * m_pWorld->m_vars.maxContactGap * 30,
					                    m_iSimClass);
				}
			}
			if (m_pMesh)
			{
				geom_world_data gwd;
				gwd.offset = m_lastMeshOffs;
				m_pMesh->DrawWireframe(pRenderer, &gwd, 0, m_iSimClass | 256);
			}
		}
	}
}
