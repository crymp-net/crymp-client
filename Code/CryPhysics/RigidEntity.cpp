#include "CryCommon/CryNetwork/ISerialize.h"
#include "CryCommon/CrySystem/ISystem.h"

#include "BoxGeom.h"
#include "ChangeRequest.h"
#include "Geometry.h"
#include "PhysArea.h"
#include "PhysicalWorld.h"
#include "Qhull.h"
#include "RayGeom.h"
#include "RigidEntity.h"
#include "WaterMan.h"

CPhysicalEntity* g_CurColliders[128];

namespace {

int g_CurCollParts[128][2];
int g_idx0NoColl = -1;
int g_nLastContacts = 0;

Vec3 Loc2Glob(const entity_contact& cnt, int i)
{
	return cnt.pent[i]->m_pNewCoords->pos +
	       cnt.pent[i]->m_pNewCoords->q *
	           (cnt.pent[i]->m_parts[cnt.ipart[i]].q * cnt.ptloc[i] * cnt.pent[i]->m_parts[cnt.ipart[i]].scale +
	            cnt.pent[i]->m_parts[cnt.ipart[i]].pos);
}

Vec3 Glob2Loc(const entity_contact& cnt, int i)
{
	return ((cnt.pt[i] - cnt.pent[i]->m_pos) * cnt.pent[i]->m_qrot - cnt.pent[i]->m_parts[cnt.ipart[i]].pos) *
	       cnt.pent[i]->m_parts[cnt.ipart[i]].q * (1.0f / cnt.pent[i]->m_parts[cnt.ipart[i]].scale);
}

void swap(edgeitem* plist, int i1, int i2)
{
	int ti = plist[i1].idx;
	plist[i1].idx = plist[i2].idx;
	plist[i2].idx = ti;
}

void qsort(edgeitem* plist, int left, int right)
{
	if (left >= right)
	{
		return;
	}
	int i, last;
	swap(plist, left, (left + right) >> 1);
	for (last = left, i = left + 1; i <= right; i++)
	{
		if (plist[plist[i].idx].area < plist[plist[left].idx].area)
		{
			swap(plist, ++last, i);
		}
	}
	swap(plist, left, last);

	qsort(plist, left, last - 1);
	qsort(plist, last + 1, right);
}

} // unnamed namespace

void SRigidEntityNetSerialize::Serialize(TSerialize ser)
{
	// need to be carefull to avoid jittering (need to represent 0 somewhat accuratelly)
	ser.Value("pos", pos, 'wrld');
	ser.Value("rot", rot, 'ori1');
	ser.Value("vel", vel, 'pRVl');
	ser.Value("angvel", angvel, 'pRAV');
	ser.Value("simclass", simclass, 'bool');
}

CRigidEntity::CRigidEntity(CPhysicalWorld* pWorld) : CPhysicalEntity(pWorld)
{
	m_posNew.zero();
	m_qNew.SetIdentity();
	m_iSimClass = 2;
	if (pWorld)
	{
		m_gravity = pWorld->m_vars.gravity;
	}
	else
	{
		m_gravity.Set(0, 0, -9.81f);
	}
	m_gravityFreefall = m_gravity;
	m_maxAllowedStep = 0.02f;
	m_Pext[0].zero();
	m_Lext[0].zero();
	m_Pext[1].zero();
	m_Lext[1].zero();
	int i;
	for (i = 0; i < sizeof(m_vhist) / sizeof(m_vhist[0]); i++)
	{
		m_vhist[i].zero();
		m_whist[i].zero();
		m_Lhist[i].zero();
	}
	m_iDynHist = 0;
	m_timeStepPerformed = 0;
	m_timeStepFull = 0.01f;
	m_Emin = sqr(0.04f);
	m_pColliderContacts = 0;
	m_pColliderConstraints = 0;
	m_pContactStart = m_pContactEnd = CONTACT_END(m_pContactStart);
	m_nContacts = 0;
	m_pConstraints = 0;
	m_pConstraintInfos = 0;
	m_nConstraintsAlloc = 0;
	m_bAwake = 1;
	m_nSleepFrames = 0;
	m_prevUnprojDir.zero();
	m_bProhibitUnproj = 0;
	m_damping = 0;
	m_dampingFreefall = 0.1f;
	m_dampingEx = 0;
	m_kwaterDensity = m_kwaterResistance = 1.0f;
	m_waterDamping = 0;
	m_EminWater = sqr(0.01f);
	m_bFloating = 0;
	m_bProhibitUnprojection = m_bEnforceContacts = -1;
	m_bJustLoaded = 0;
	m_bCollisionCulling = 0;
	m_maxw = 20.0f;
	m_iLastChecksum = 0;
	m_softness[0] = 0.00015f;
	m_softness[1] = 0.001f;
	m_bStable = 0;
	m_bHadSeverePenetration = 0;
	m_nRestMask = 0;
	m_nPrevColliders = 0;
	m_lastTimeStep = 0;
	m_nStepBackCount = m_bSteppedBack = 0;
	m_bCanSweep = 1;
	m_minAwakeTime = 0;
	m_Psoft.zero();
	m_Lsoft.zero();
	m_E0 = m_Estep = 0;
	m_impulseTime = 0;
	m_pNewCoords = (coord_block*)&m_posNew;
	m_iLastConstraintIdx = 1;
	m_lockConstraintIdx = 0;
	m_lockDynState = 0;
	m_lockContacts = 0;
	m_Fcollision.zero();
	m_Tcollision.zero();
	m_iVarPart0 = 1000000;
	m_iLastLog = -2;
	m_pEvent = 0;
	m_iLastLogColl = -2;
	m_vcollMin = 1E10f;
	m_icollMin = 0;
	m_nEvents = m_nMaxEvents = 0;
	m_pEventsColl = 0;
	m_timeCanopyFallen = 0;
	m_nNonRigidNeighbours = 0;
	m_prevv.zero();
	m_prevw.zero();
	m_minFriction = 0.1f;
	m_nFutileUnprojFrames = 0;
	m_bCanopyContact = 0;
	m_nCanopyContactsLost = 0;
	m_vSleep.zero();
	m_wSleep.zero();
	m_pStableContact = 0;
	m_submergedFraction = 0;
	m_BBoxNew[0].zero();
	m_BBoxNew[1].zero();
	m_velFastDir = m_sizeFastDir = 0;
}

CRigidEntity::~CRigidEntity()
{
	if (m_pColliderContacts)
	{
		delete[] m_pColliderContacts;
	}
	entity_contact *pContact, *pContactNext;
	for (pContact = m_pContactStart; pContact != CONTACT_END(m_pContactStart); pContact = pContactNext)
	{
		pContactNext = pContact->next;
		m_pWorld->FreeContact(pContact);
	}
	if (m_pColliderConstraints)
	{
		delete[] m_pColliderConstraints;
	}
	if (m_pConstraints)
	{
		delete[] m_pConstraints;
	}
	if (m_pConstraintInfos)
	{
		delete[] m_pConstraintInfos;
	}
	if (m_pEventsColl)
	{
		delete[] m_pEventsColl;
	}
}

void CRigidEntity::AttachContact(entity_contact* pContact, int i)
{ // register new contact as the first contact for the collider i
	WriteLock lock(m_lockContacts);
	if (m_pColliderContacts[i] == 0)
	{
		m_pColliderContacts[i] = m_pContactStart;
		pContact->flags = contact_last;
	}
	else
	{
		pContact->flags = 0;
	}
	pContact->prev = m_pColliderContacts[i]->prev;
	pContact->next = m_pColliderContacts[i];
	m_pColliderContacts[i]->prev->next = pContact;
	m_pColliderContacts[i]->prev = pContact;
	m_pColliderContacts[i] = pContact;
	m_nContacts++;
}

void CRigidEntity::DetachContact(entity_contact* pContact, int i, int bCheckIfEmpty)
{ // detach contact from the contact list
	WriteLock lock(m_lockContacts);
	pContact->prev->next = pContact->next;
	pContact->next->prev = pContact->prev;
	if (i < 0)
	{
		for (i = 0; i < m_nColliders && m_pColliders[i] != pContact->pent[1]; i++)
			;
	}
	if (m_pColliderContacts[i] != pContact)
	{
		pContact->prev->flags |= pContact->flags & contact_last;
	}
	else
	{
		m_pColliderContacts[i] = (pContact->flags & contact_last) ? 0 : pContact->next;

		if (bCheckIfEmpty && !m_pColliderContacts[i] && !m_pColliderConstraints[i] &&
		    !m_pColliders[i]->HasContactsWith(this))
		{
			CPhysicalEntity* pCollider = m_pColliders[i];
			pCollider->RemoveCollider(this);
			RemoveCollider(pCollider);
		}
	}
	m_nContacts--;
}

void CRigidEntity::DetachAllContacts()
{
	entity_contact *pContact, *pContactNext;
	for (pContact = m_pContactStart; pContact != CONTACT_END(m_pContactStart); pContact = pContactNext)
	{
		pContactNext = pContact->next;
		m_pWorld->FreeContact(pContact);
	}
	m_nContacts = 0;
	m_pContactStart = m_pContactEnd = CONTACT_END(m_pContactStart);
	for (int i = m_nColliders - 1; i >= 0; i--)
	{
		m_pColliderContacts[i] = 0;
		if (!m_pColliderConstraints[i] && !m_pColliders[i]->HasConstraintContactsWith(this))
		{
			CPhysicalEntity* pCollider = m_pColliders[i];
			pCollider->RemoveCollider(this);
			RemoveCollider(pCollider);
		}
	}
}

void CRigidEntity::DetachPartContacts(int ipart, int iop0, CPhysicalEntity* pent, int iop1, int bCheckIfEmpty)
{
	entity_contact *pContact, *pContactNext;
	for (pContact = m_pContactStart; pContact != CONTACT_END(m_pContactStart); pContact = pContactNext)
	{
		pContactNext = pContact->next;
		if (pContact->ipart[iop0] == ipart && pContact->pent[iop1] == pent)
		{
			DetachContact(pContact, -1, bCheckIfEmpty), m_pWorld->FreeContact(pContact);
		}
	}
}

void CRigidEntity::MoveConstrainedObjects(const Vec3& dpos, const quaternionf& dq)
{
	pe_params_pos pp;
	for (int i = 0; i < m_nColliders; i++)
	{
		if ((unsigned int)m_pColliders[i]->m_iSimClass - 1u < 2u &&
		        HasConstraintContactsWith(m_pColliders[i], constraint_inactive | constraint_rope) ||
		    m_pColliders[i]->HasConstraintContactsWith(this, constraint_inactive | constraint_rope))
		{
			pp.pos = m_pos + dq * (m_pColliders[i]->m_pos - m_pos + dpos);
			pp.q = dq * m_pColliders[i]->m_qrot;
			pp.bRecalcBounds = 16 | 2;
			m_pColliders[i]->SetParams(&pp);
		}
	}
}

int CRigidEntity::AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id, int bThreadSafe)
{
	if (pgeom && fabs_tpl(params->mass) + fabs_tpl(params->density) != 0 &&
	    (pgeom->V <= 0 || pgeom->Ibody.x < 0 || pgeom->Ibody.y < 0 || pgeom->Ibody.z < 0))
	{
		CryLogError("CRigidEntity::AddGeometry: (%s at %.1f,%.1f,%.1f) Trying to add bad geometry",
		            m_pWorld->m_pRenderer
		                ? m_pWorld->m_pRenderer->GetForeignName(m_pForeignData, m_iForeignData, m_iForeignFlags)
		                : "",
		            m_pos.x, m_pos.y, m_pos.z);
		return -1;
	}

	if (!pgeom)
	{
		return -1;
	}
	ChangeRequest<pe_geomparams> req(this, m_pWorld, params, bThreadSafe, pgeom, id);
	if (req.IsQueued())
	{
		WriteLock lock(m_lockPartIdx);
		if (id < 0)
		{
			*((int*)req.GetQueuedStruct() - 1) = id = m_iLastIdx++;
		}
		else
		{
			m_iLastIdx = max(m_iLastIdx, id + 1);
		}
		return id;
	}

	int res;
	if ((res = CPhysicalEntity::AddGeometry(pgeom, params, id, 1)) < 0)
	{
		return res;
	}

	float V = pgeom->V * cube(params->scale), M = params->mass > 0 ? params->mass : params->density * V;
	Vec3 bodypos = m_pos + m_qrot * (params->pos + params->q * pgeom->origin * params->scale);
	quaternionf bodyq = m_qrot * params->q * pgeom->q;
	int i;
	for (i = 0; i < m_nParts && m_parts[i].id != res; i++)
		;
	m_parts[i].mass = M;

	if (M > 0)
	{
		if (m_body.M == 0 || m_nParts == 1)
		{
			m_body.Create(bodypos, pgeom->Ibody * sqr(params->scale) * cube(params->scale), bodyq, V, M,
			              m_qrot, m_pos);
			m_body.softness[0] = m_softness[0];
			m_body.softness[1] = m_softness[1];
		}
		else
		{
			m_body.Add(bodypos, pgeom->Ibody * sqr(params->scale) * cube(params->scale), bodyq, V, M);
		}
	}
	m_prevPos = m_body.pos;
	m_prevq = m_body.q;

	return res;
}

void CRigidEntity::RemoveGeometry(int id, int bThreadSafe)
{
	ChangeRequest<void> req(this, m_pWorld, 0, bThreadSafe, 0, id);
	if (req.IsQueued())
	{
		return;
	}

	int i, bRecalcMass = 0;
	for (i = 0; i < m_nParts && m_parts[i].id != id; i++)
		;
	if (i == m_nParts)
	{
		return;
	}
	phys_geometry* pgeom = m_parts[i].pPhysGeomProxy;

	if (m_parts[i].mass > 0 && m_body.M > 0)
	{
		Vec3 bodypos = m_pos + m_qrot * (m_parts[i].pos + m_parts[i].q * pgeom->origin * m_parts[i].scale);
		quaternionf bodyq = m_qrot * m_parts[i].q * pgeom->q;

		if (m_nParts > 1)
		{
			bRecalcMass = 1;
		}
		else
		{
			m_body.zero();
		}
		// m_body.Add(bodypos,-pgeom->Ibody*sqr(m_parts[i].scale)*cube(m_parts[i].scale), bodyq,
		//					 -pgeom->V*cube(m_parts[i].scale),-m_parts[i].mass);
	}
	DetachAllContacts();

	CPhysicalEntity::RemoveGeometry(id, 1);

	if (bRecalcMass)
	{
		RecomputeMassDistribution();
	}
}

void CRigidEntity::RecomputeMassDistribution(int ipart, int bMassChanged)
{
	float V;
	Vec3 bodypos, v = m_body.v, w = m_body.w;
	quaternionf bodyq, q = m_qrot.GetNormalized();

	m_body.zero();
	for (int i = 0; i < m_nParts; i++)
	{
		V = m_parts[i].pPhysGeom->V * cube(m_parts[i].scale);
		if (V <= 0)
		{
			m_parts[i].mass = 0;
		}
		bodypos = m_pos + q * (m_parts[i].pos + m_parts[i].q * m_parts[i].pPhysGeom->origin * m_parts[i].scale);
		bodyq = (q * m_parts[i].q * m_parts[i].pPhysGeom->q).GetNormalized();
		if (m_parts[i].mass > 0)
		{
			if (m_body.M == 0)
			{
				m_body.Create(bodypos,
				              m_parts[i].pPhysGeom->Ibody * cube(m_parts[i].scale) *
				                  sqr(m_parts[i].scale),
				              bodyq, V, m_parts[i].mass, q, m_pos);
				m_body.softness[0] = m_softness[0];
				m_body.softness[1] = m_softness[1];
			}
			else
			{
				m_body.Add(bodypos,
				           m_parts[i].pPhysGeom->Ibody * sqr(m_parts[i].scale) * cube(m_parts[i].scale),
				           bodyq, V, m_parts[i].mass);
			}
		}
	}
	if (min(min(min(m_body.M, m_body.Ibody_inv.x), m_body.Ibody_inv.y), m_body.Ibody_inv.z) < 0)
	{
		m_body.M = m_body.Minv = 0;
		m_body.Ibody_inv.zero();
		m_body.Ibody.zero();
	}
	else
	{
		m_body.P = (m_body.v = v) * m_body.M;
		m_body.L = m_body.q * (m_body.Ibody * (!m_body.q * (m_body.w = w)));
	}
	m_prevPos = m_body.pos;
	m_prevq = m_body.q;
}

int CRigidEntity::SetParams(pe_params* _params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this, m_pWorld, _params, bThreadSafe);
	if (req.IsQueued())
	{
		return 1;
	}

	int res, i, flags0 = m_flags;
	Vec3 BBox0[2] = {m_BBox[0], m_BBox[1]};

	res = CPhysicalEntity::SetParams(_params, 1);
	if (res)
	{
		if (_params->type == pe_params_pos::type_id)
		{
			pe_params_pos* params = (pe_params_pos*)_params;
			Vec3 pos0 = m_body.pos;
			quaternionf q0 = m_body.q;
			if (!is_unused(params->pos) || params->pMtx3x4)
			{
				m_body.pos = m_pos + m_qrot * m_body.offsfb;
			}
			if (!is_unused(params->q) || params->pMtx3x3 || params->pMtx3x4)
			{
				m_body.pos = m_pos + m_qrot * m_body.offsfb;
				m_body.q = m_qrot * !m_body.qfb;
			}
			if (!is_unused(params->iSimClass))
			{
				m_bAwake = isneg(1 - m_iSimClass);
			}
			int bRealMove = isneg(
			    min(sqr(0.003f) - (pos0 - m_body.pos).len2(), sqr(0.05f) - (q0.v - m_body.q.v).len2()));
			if (bRealMove && !(params->bRecalcBounds & 32))
			{
				if (m_pWorld->m_vars.lastTimeStep > 0)
				{
					for (i = 0; i < m_nColliders; i++)
					{
						m_pColliders[i]->Awake();
					}
				}
				DetachAllContacts();
				if (!(params->bRecalcBounds & 2))
				{
					if (m_body.M > 0)
					{
						m_body.v.zero();
						m_body.w.zero();
						m_body.P.zero();
						m_body.L.zero();
					}
					if (m_pWorld->m_vars.lastTimeStep > 0)
					{
						MoveConstrainedObjects(m_body.pos - pos0, m_body.q * !q0);
					}
				}
			}
			if (!is_unused(params->scale) && m_nParts && fabsf(params->scale - m_parts[0].scale) > 0.001f)
			{
				RecomputeMassDistribution();
			}
			if (m_nParts && m_pWorld->m_vars.lastTimeStep > 0 && !(m_flags & pef_disabled) &&
			    !(params->bRecalcBounds & 32))
			{
				int nEnts;
				CPhysicalEntity** pentlist;
				if (m_body.Minv == 0)
				{ // for animated 'static' rigid bodies, awake the environment
					nEnts = m_pWorld->GetEntitiesAround(
					    m_BBox[0], m_BBox[1], pentlist,
					    ent_sleeping_rigid | ent_living | ent_independent | ent_triggers, this);
					for (--nEnts; nEnts >= 0; nEnts--)
					{
						if (pentlist[nEnts] != this)
						{
							pentlist[nEnts]->Awake();
						}
					}
				}
				if (m_iLastLog < 0 && bRealMove)
				{
					const float val = m_pWorld->m_vars.maxContactGap * 4;
					nEnts = m_pWorld->GetEntitiesAround(
					    BBox0[0] - Vec3(val, val, val), BBox0[1] + Vec3(val, val, val), pentlist,
					    ent_sleeping_rigid | ent_living | ent_independent, this);
					for (--nEnts; nEnts >= 0; nEnts--)
					{
						if (pentlist[nEnts] != this)
						{
							pentlist[nEnts]->Awake();
						}
					}
				}
			}
			m_prevPos = m_body.pos;
			m_prevq = m_body.q;
		}
		else if (_params->type == pe_params_part::type_id)
		{
			pe_params_part* params = (pe_params_part*)_params;
			if (!is_unused(params->partid) || !is_unused(params->ipart))
			{
				if (!is_unused(params->mass) || !is_unused(params->density) ||
				    !is_unused(params->pPhysGeom) || !is_unused(params->pPhysGeomProxy))
				{
					DetachAllContacts();
					if (m_parts[res - 1].mass > 0 || !is_unused(params->mass) ||
					    !is_unused(params->density))
					{
						RecomputeMassDistribution(res - 1, 1);
					}
				}
				else if (params->bRecalcBBox)
				{
					RecomputeMassDistribution(res - 1, 0);
				}
			}
			else if (!is_unused(params->mass))
			{
				RecomputeMassDistribution();
			}
		}
		else if (_params->type == pe_params_flags::type_id)
		{
			if (flags0 & pef_invisible && !(m_flags & pef_invisible) && m_timeIdle > m_maxTimeIdle &&
			    m_submergedFraction > 0 && !m_bAwake)
			{
				Awake();
			}
		}
		return res;
	}

	if (_params->type == pe_simulation_params::type_id)
	{
		pe_simulation_params* params = (pe_simulation_params*)_params;
		bool bRecompute = false;
		if (!is_unused(params->gravity))
		{
			m_gravity = params->gravity;
			if (is_unused(params->gravityFreefall))
			{
				m_gravityFreefall = params->gravity;
			}
		}
		if (!is_unused(params->maxTimeStep))
		{
			m_maxAllowedStep = params->maxTimeStep;
		}
		if (!is_unused(params->minEnergy))
		{
			m_Emin = params->minEnergy;
		}
		if (!is_unused(params->damping))
		{
			m_damping = params->damping;
		}
		if (!is_unused(params->gravityFreefall))
		{
			m_gravityFreefall = params->gravityFreefall;
		}
		if (!is_unused(params->dampingFreefall))
		{
			m_dampingFreefall = params->dampingFreefall;
		}
		if (!is_unused(params->density) && params->density >= 0 && m_nParts > 0)
		{
			for (i = 0; i < m_nParts; i++)
			{
				m_parts[i].mass = m_parts[i].pPhysGeom->V * cube(m_parts[i].scale) * params->density;
			}
			bRecompute = true;
		}
		if (!is_unused(params->mass))
		{
			if (params->mass < 0)
			{
				m_body.zero();
				bRecompute = false;
			}
			else if (params->mass >= 0 && m_nParts > 0)
			{
				if (m_body.M == 0)
				{
					float density, V = 0;
					for (i = 0; i < m_nParts; i++)
					{
						V += m_parts[i].pPhysGeom->V * cube(m_parts[i].scale);
					}
					if (V > 0)
					{
						density = params->mass / V;
						for (i = 0; i < m_nParts; i++)
						{
							m_parts[i].mass =
							    m_parts[i].pPhysGeom->V * cube(m_parts[i].scale) * density;
						}
					}
				}
				else
				{
					float scaleM = params->mass / m_body.M;
					for (i = 0; i < m_nParts; i++)
					{
						m_parts[i].mass *= scaleM;
					}
				}
				bRecompute = true;
			}
		}
		if (!is_unused(params->softness))
		{
			m_softness[0] = m_body.softness[0] = params->softness;
			bRecompute = true;
		}
		if (!is_unused(params->softnessAngular))
		{
			m_softness[1] = m_body.softness[1] = params->softnessAngular;
			bRecompute = true;
		}
		if (bRecompute)
		{
			RecomputeMassDistribution();
		}
		if (!is_unused(params->iSimClass))
		{
			m_bAwake = isneg(1 - (m_iSimClass = params->iSimClass));
			m_pWorld->RepositionEntity(this, 2);
		}
		if (!is_unused(params->maxLoggedCollisions) && params->maxLoggedCollisions != m_nMaxEvents)
		{
			if (m_pEventsColl)
			{
				delete[] m_pEventsColl;
			}
			m_nMaxEvents = params->maxLoggedCollisions;
			if (m_nMaxEvents)
			{
				m_pEventsColl = new EventPhysCollision*[m_nMaxEvents];
			}
			m_nEvents = m_icollMin = 0;
			m_vcollMin = 1E10f;
		}
		return 1;
	}

	if (_params->type == pe_params_buoyancy::type_id)
	{
		pe_params_buoyancy* params = (pe_params_buoyancy*)_params;
		int bAwake = 0;
		float waterDensity0 = 1000.0f, waterResistance0 = 1000.0f;
		if (m_pWorld->m_pGlobalArea && !is_unused(m_pWorld->m_pGlobalArea->m_pb.waterPlane.origin))
		{
			waterDensity0 = m_pWorld->m_pGlobalArea->m_pb.waterDensity;
			waterResistance0 = m_pWorld->m_pGlobalArea->m_pb.waterResistance;
		}
		if (!is_unused(params->waterDensity))
		{
			m_kwaterDensity = params->waterDensity / waterDensity0;
		}
		if (!is_unused(params->kwaterDensity))
		{
			m_kwaterDensity = params->kwaterDensity;
		}
		if (!is_unused(params->waterResistance))
		{
			m_kwaterResistance = params->waterResistance / waterResistance0;
		}
		if (!is_unused(params->kwaterResistance))
		{
			m_kwaterResistance = params->kwaterResistance;
		}
		if (!is_unused(params->waterEmin))
		{
			m_EminWater = params->waterEmin;
		}
		if (!is_unused(params->waterDamping))
		{
			m_waterDamping = params->waterDamping;
		}
		return 1;
	}

	return res;
}

int CRigidEntity::GetParams(pe_params* _params)
{
	if (_params->type == pe_simulation_params::type_id)
	{
		pe_simulation_params* params = (pe_simulation_params*)_params;
		params->gravity = m_gravity;
		params->maxTimeStep = m_maxAllowedStep;
		params->minEnergy = m_Emin;
		params->damping = m_damping;
		params->dampingFreefall = m_dampingFreefall;
		params->gravityFreefall = m_gravityFreefall;
		params->iSimClass = m_iSimClass;
		params->density = m_body.V > 0 ? m_body.M / m_body.V : 0;
		params->mass = m_body.M;
		params->maxLoggedCollisions = m_nMaxEvents;
		return 1;
	}

	if (_params->type == pe_params_buoyancy::type_id)
	{
		pe_params_buoyancy* params = (pe_params_buoyancy*)_params;
		float waterDensity0 = 1000.0f, waterResistance0 = 1000.0f;
		if (m_pWorld->m_pGlobalArea && !is_unused(m_pWorld->m_pGlobalArea->m_pb.waterPlane.origin))
		{
			waterDensity0 = m_pWorld->m_pGlobalArea->m_pb.waterDensity;
			waterResistance0 = m_pWorld->m_pGlobalArea->m_pb.waterResistance;
		}
		params->waterDensity = m_kwaterDensity * waterDensity0;
		params->kwaterDensity = m_kwaterDensity;
		params->waterResistance = m_kwaterResistance * waterResistance0;
		params->kwaterResistance = m_kwaterResistance;
		params->waterDamping = m_waterDamping;
		params->waterEmin = m_EminWater;
		return 1;
	}

	return CPhysicalEntity::GetParams(_params);
}

int CRigidEntity::GetStatus(pe_status* _status)
{
	if (_status->type == pe_status_awake::type_id)
	{
		pe_status_awake* status = (pe_status_awake*)_status;
		return status->lag ? m_iLastLog < 0 || m_bAwake | (m_iLastLog >= m_pWorld->m_iLastLogPump - status->lag)
		                   : m_bAwake;
	}

	int res = CPhysicalEntity::GetStatus(_status);
	if (res)
	{
		if (_status->type == pe_status_pos::type_id)
		{
			pe_status_pos* status = (pe_status_pos*)_status;
			if (status->timeBack > 0)
			{
				status->q = m_prevq * m_body.qfb;
				status->pos = m_prevPos - status->q * m_body.offsfb;
			}
		}
		return res;
	}

	if (_status->type == pe_status_constraint::type_id)
	{
		pe_status_constraint* status = (pe_status_constraint*)_status;
		int i;
		masktype constraint_mask = 0;
		for (i = 0; i < m_nColliders; constraint_mask |= m_pColliderConstraints[i++])
			;
		for (i = 0; i < NMASKBITS && getmask(i) <= constraint_mask; i++)
		{
			if (constraint_mask & getmask(i) && m_pConstraintInfos[i].id == status->id)
			{
				status->pt[0] = m_pConstraints[i].pt[0];
				status->pt[1] = m_pConstraints[i].pt[1];
				status->n = m_pConstraints[i].n;
				status->flags = m_pConstraintInfos[i].flags;
				return 1;
			}
		}
		return 0;
	}

	if (_status->type == pe_status_dynamics::type_id)
	{
		pe_status_dynamics* status = (pe_status_dynamics*)_status;
		ReadLock lock(m_lockDynState);
		if (m_bAwake)
		{
			int i = (m_iDynHist - 1) & ((sizeof(m_vhist) / sizeof(m_vhist[0])) - 1);
			status->v = m_vhist[i];
			status->w = m_whist[i];
			status->a = m_gravity + m_Fcollision * m_body.Minv;
			status->wa = m_body.Iinv * (m_Tcollision - (m_whist[i] ^ m_Lhist[i]));
		}
		else
		{
			status->v.zero();
			status->w.zero();
			status->a.zero();
			status->wa.zero();
		}
		status->centerOfMass = m_body.pos;
		status->submergedFraction = m_submergedFraction;
		status->mass = m_body.M;
		return 1;
	}

	if (_status->type == pe_status_collisions::type_id)
	{
		return m_pContactStart != CONTACT_END(m_pContactStart);
	}

	if (_status->type == pe_status_sample_contact_area::type_id)
	{
		pe_status_sample_contact_area* status = (pe_status_sample_contact_area*)_status;
		Vec3 sz = m_BBox[1] - m_BBox[0];
		float dist, tol = (sz.x + sz.y + sz.z) * 0.1f;
		int nContacts;
		entity_contact* pRes;
		return CompactContactBlock(m_pContactStart, 0, tol, 0, nContacts, pRes, sz, dist, status->ptTest,
		                           status->dirTest);
	}

	return 0;
}

int CRigidEntity::Action(pe_action* _action, int bThreadSafe)
{
	ChangeRequest<pe_action> req(this, m_pWorld, _action, bThreadSafe);
	if (req.IsQueued())
	{
		if (_action->type == pe_action_add_constraint::type_id)
		{
			pe_action_add_constraint* action = (pe_action_add_constraint*)req.GetQueuedStruct();
			if (is_unused(action->id))
			{
				WriteLock lock(m_lockConstraintIdx);
				action->id = m_iLastConstraintIdx++;
			}
			return action->id;
		}
		return 1;
	}

	if (_action->type == pe_action_impulse::type_id)
	{
		pe_action_impulse* action = (pe_action_impulse*)_action;

		if (m_body.Minv > 0)
		{
			Vec3 P = action->impulse;
			Vec3 L;
			if (!is_unused(action->angImpulse))
			{
				L = action->angImpulse;
			}
			else if (!is_unused(action->point))
			{
				L = action->point - m_body.pos ^ action->impulse;
			}

			if (action->iSource != 1)
			{
				m_bAwake = 1;
				if (m_iSimClass == 1)
				{
					m_iSimClass = 2;
					m_pWorld->RepositionEntity(this, 2);
				}
				m_impulseTime = 0.2f;
				m_timeIdle *= isneg(-action->iSource);
			}
			else
			{
				float vres;
				if ((vres = P.len2() * sqr(m_body.Minv)) > sqr(5.0f))
				{
					P *= 5.0f / sqrt_tpl(vres);
				}
				if ((vres = (m_body.Iinv * L).len2()) > sqr(5.0f))
				{
					L *= 5.0f / sqrt_tpl(vres);
				}
			}

			if (action->iApplyTime == 0)
			{
				m_body.P += P;
				m_body.L += L;
				m_body.v = m_body.P * m_body.Minv;
				m_body.w = m_body.Iinv * m_body.L;
				CapBodyVel();
			}
			else
			{
				m_Pext[action->iApplyTime - 1] += P;
				m_Lext[action->iApplyTime - 1] += L;
			}
		}

		CPhysicalEntity::Action(_action, 1);

		return 1;
	}

	if (_action->type == pe_action_reset::type_id)
	{
		m_body.v.zero();
		m_body.w.zero();
		m_body.P.zero();
		m_body.L.zero();
		m_Pext[0].zero();
		m_Lext[0].zero();
		m_Pext[1].zero();
		m_Lext[1].zero();
		if (((pe_action_reset*)_action)->bClearContacts)
		{
			DetachAllContacts();
			pe_action_update_constraint auc;
			auc.bRemove = 1;
			Action(&auc);
		}
		{
			WriteLock lock(m_lockDynState);
			m_iDynHist = 1;
			m_vhist[0].zero();
			m_whist[0].zero();
			m_Lhist[0].zero();
			m_Fcollision.zero();
			m_Tcollision.zero();
		}
		m_Psoft.zero();
		m_Lsoft.zero();
		// m_nStickyContacts = m_nSlidingContacts = 0;
		m_iLastLog = m_iLastLogColl = -2;
		m_nRestMask = 0;
		m_nEvents = 0;
		return 1;
	}

	if (_action->type == pe_action_add_constraint::type_id)
	{
		pe_action_add_constraint* action = (pe_action_add_constraint*)_action;
		CPhysicalEntity* pBuddy = (CPhysicalEntity*)action->pBuddy;
		if (pBuddy == WORLD_ENTITY)
		{
			pBuddy = &g_StaticPhysicalEntity;
		}
		int i, res, ipart[2], id, flagsInfo, bBreakable = 0;
		Vec3 nloc, pt0, pt1;
		quaternionf qframe[2];
		float damping = 0;
		qframe[0].SetIdentity();
		qframe[1].SetIdentity();
		if (is_unused(action->pt[0]))
		{
			return 0;
		}
		if (is_unused(action->id))
		{
			WriteLock lock(m_lockConstraintIdx);
			id = m_iLastConstraintIdx++;
		}
		else
		{
			id = action->id;
		}
		pt0 = action->pt[0];
		pt1 = is_unused(action->pt[1]) ? action->pt[0] : action->pt[1];
		if (!is_unused(action->damping))
		{
			damping = action->damping;
		}
		flagsInfo = action->flags & (constraint_inactive | constraint_ignore_buddy);
		if (action->flags & local_frames)
		{
			pt0 = m_qrot * pt0 + m_pos;
			pt1 = pBuddy->m_qrot * pt1 + pBuddy->m_pos;
		}

		if (!is_unused(action->partid[0]))
		{
			for (ipart[0] = 0; ipart[0] < m_nParts && m_parts[ipart[0]].id != action->partid[0]; ipart[0]++)
				;
			if (ipart[0] >= m_nParts)
			{
				return 0;
			}
		}
		else if (m_nParts)
		{
			ipart[0] = 0;
		}
		else
		{
			return 0;
		}
		if (!is_unused(action->partid[1]))
		{
			for (ipart[1] = 0;
			     ipart[1] < pBuddy->m_nParts && pBuddy->m_parts[ipart[1]].id != action->partid[1];
			     ipart[1]++)
				;
			if (pBuddy->m_nParts > 0 && ipart[1] >= pBuddy->m_nParts)
			{
				return 0;
			}
		}
		else if (pBuddy->m_nParts || pBuddy == &g_StaticPhysicalEntity)
		{
			ipart[1] = 0;
		}
		else
		{
			return 0;
		}

		res = i = RegisterConstraint(pt0, pt1, ipart[0], pBuddy, ipart[1], contact_constraint_3dof, flagsInfo);
		m_pConstraintInfos[i].id = id;
		if (!is_unused(action->sensorRadius))
		{
			m_pConstraintInfos[i].sensorRadius = action->sensorRadius;
		}
		if (!is_unused(action->maxPullForce))
		{
			m_pConstraintInfos[i].limit = action->maxPullForce, bBreakable = 1;
		}

		if (!is_unused(action->qframe[0]))
		{
			qframe[0] = action->qframe[0];
		}
		if (!is_unused(action->qframe[1]))
		{
			qframe[1] = action->qframe[1];
		}
		if (action->flags & local_frames)
		{
			qframe[0] = m_qrot * qframe[0];
			qframe[1] = pBuddy->m_qrot * qframe[1];
		}
		qframe[0] = !m_pConstraints[i].pbody[0]->q * qframe[0];
		qframe[1] = !m_pConstraints[i].pbody[1]->q * qframe[1];
		m_pConstraintInfos[i].qprev[0] = m_pConstraints[i].pbody[0]->q;
		m_pConstraintInfos[i].qprev[1] = m_pConstraints[i].pbody[1]->q;
		m_pConstraintInfos[i].qframe_rel[0] = qframe[0];
		m_pConstraintInfos[i].qframe_rel[1] = qframe[1];
		nloc = qframe[0] * Vec3(1, 0, 0);

		if (!is_unused(action->pConstraintEntity))
		{
			m_pConstraintInfos[i].pConstraintEnt = (CPhysicalEntity*)action->pConstraintEntity;
			if (action->pConstraintEntity && action->pConstraintEntity->GetType() == PE_ROPE)
			{
				m_pConstraints[i].flags = 0; // act like frictionless contact when rope is strained
				m_pConstraintInfos[i].flags = constraint_rope;
			}
		}

		if (!(!is_unused(action->xlimits[0]) && action->xlimits[0] >= action->xlimits[1] &&
		      !is_unused(action->yzlimits[0]) && action->yzlimits[0] >= action->yzlimits[1]))
		{
			if (!is_unused(action->xlimits[0]) && action->xlimits[0] >= action->xlimits[1])
			{
				i = RegisterConstraint(pt0, pt1, ipart[0], pBuddy, ipart[1],
				                       contact_angular | contact_constraint_2dof, flagsInfo);
				m_pConstraints[i].nloc = nloc;
				m_pConstraintInfos[i].qframe_rel[0] = qframe[0];
				m_pConstraintInfos[i].qframe_rel[1] = qframe[1];
				m_pConstraintInfos[i].id = id;
				m_pConstraintInfos[i].damping = damping;
			}
			else if (!is_unused(action->yzlimits[0]) && action->yzlimits[0] >= action->yzlimits[1])
			{
				i = RegisterConstraint(pt0, pt1, ipart[0], pBuddy, ipart[1],
				                       contact_angular | contact_constraint_1dof, flagsInfo);
				m_pConstraints[i].nloc = nloc;
				m_pConstraintInfos[i].qframe_rel[0] = qframe[0];
				m_pConstraintInfos[i].qframe_rel[1] = qframe[1];
				m_pConstraintInfos[i].id = id;
				m_pConstraintInfos[i].damping = damping;
			}

			if (!is_unused(action->xlimits[0]) && action->xlimits[0] < action->xlimits[1])
			{
				i = RegisterConstraint(pt0, pt1, ipart[0], pBuddy, ipart[1], contact_angular,
				                       flagsInfo);
				m_pConstraints[i].nloc = nloc;
				m_pConstraintInfos[i].qframe_rel[0] = qframe[0];
				m_pConstraintInfos[i].qframe_rel[1] = qframe[1];
				m_pConstraintInfos[i].limits[0] = action->xlimits[0];
				m_pConstraintInfos[i].limits[1] = action->xlimits[1];
				m_pConstraintInfos[i].flags = constraint_limited_1axis;
				m_pConstraintInfos[i].id = id;
				if (!is_unused(action->maxBendTorque))
				{
					m_pConstraintInfos[i].limit = action->maxBendTorque, bBreakable = 2;
				}
			}
			if (!is_unused(action->yzlimits[0]) && action->yzlimits[0] < action->yzlimits[1])
			{
				i = RegisterConstraint(pt0, pt1, ipart[0], pBuddy, ipart[1], contact_angular,
				                       flagsInfo);
				m_pConstraints[i].nloc = nloc;
				m_pConstraintInfos[i].qframe_rel[0] = qframe[0];
				m_pConstraintInfos[i].qframe_rel[1] = qframe[1];
				m_pConstraintInfos[i].limits[0] = action->yzlimits[0];
				m_pConstraintInfos[i].limits[1] = sin_tpl(action->yzlimits[1]);
				m_pConstraintInfos[i].flags = constraint_limited_2axes;
				m_pConstraintInfos[i].id = id;
				if (!is_unused(action->maxBendTorque) && bBreakable < 2)
				{
					m_pConstraintInfos[i].limit = action->maxBendTorque, bBreakable = 1;
				}
			}
		}
		if (bBreakable)
		{
			BreakableConstraintsUpdated();
		}

		return id;
	}

	if (_action->type == pe_action_update_constraint::type_id)
	{
		pe_action_update_constraint* action = (pe_action_update_constraint*)_action;
		masktype constraint_mask;
		int i, j, n;
		for (i = 0, constraint_mask = 0; i < m_nColliders; constraint_mask |= m_pColliderConstraints[i++])
			;

		if (is_unused(action->idConstraint))
		{
			if (action->bRemove)
			{
				for (i = m_nColliders - 1; i >= 0; i--)
				{
					if (m_pColliderConstraints[i])
					{
						m_pColliderConstraints[i] = 0;
						if (!m_pColliderContacts[i] && !m_pColliders[i]->HasContactsWith(this))
						{
							CPhysicalEntity* pCollider = m_pColliders[i];
							pCollider->RemoveCollider(this);
							RemoveCollider(pCollider);
						}
					}
				}
				BreakableConstraintsUpdated();
			}
			else
			{
				for (i = NMASKBITS - 1, n = 0; i >= 0; i--)
				{
					if (constraint_mask & getmask(i))
					{
						m_pConstraintInfos[i].flags =
						    (m_pConstraintInfos[i].flags & action->flagsAND) | action->flagsOR;
					}
				}
			}
			return 1;
		}
		else
		{
			for (i = 0, constraint_mask = 0; i < m_nColliders;
			     constraint_mask |= m_pColliderConstraints[i++])
				;
			for (i = NMASKBITS - 1, n = 0; i >= 0; i--)
			{
				if (constraint_mask & getmask(i) && m_pConstraintInfos[i].id == action->idConstraint)
				{
					m_pConstraintInfos[i].flags =
					    (m_pConstraintInfos[i].flags & action->flagsAND) | action->flagsOR;
					for (j = 0; j < 2; j++)
					{
						if (!is_unused(action->pt[j]))
						{
							m_pConstraints[i].pt[j] =
							    (action->flags & world_frames)
								? action->pt[0]
								: m_pConstraints[i].pent[j]->m_qrot * action->pt[0] +
								      m_pConstraints[i].pent[j]->m_pos;
							m_pConstraints[i].ptloc[j] = Glob2Loc(m_pConstraints[i], j);
						}
					}
					if (action->bRemove)
					{
						n += RemoveConstraint(i);
					}
				}
			}
			if (action->bRemove)
			{
				BreakableConstraintsUpdated();
			}
			return n;
		}
	}

	if (_action->type == pe_action_set_velocity::type_id)
	{
		pe_action_set_velocity* action = (pe_action_set_velocity*)_action;
		{
			WriteLock lock(m_lockDynState);
			if (!is_unused(action->v))
			{
				m_body.v = action->v;
				m_body.P = action->v * m_body.M;
			}
			if (!is_unused(action->w))
			{
				m_body.w = action->w;
				m_body.L = m_body.q * (m_body.Ibody * (!m_body.q * m_body.w));
			}
			CapBodyVel();
			int i = (m_iDynHist - 1) & ((sizeof(m_vhist) / sizeof(m_vhist[0])) - 1);
			m_vhist[i] = m_body.v;
			m_whist[i] = m_body.w;
			m_Lhist[i] = m_body.L;
		}
		if (m_body.v.len2() + m_body.w.len2() > 0)
		{
			if (!m_bAwake)
			{
				Awake();
			}
			m_timeIdle = 0;
		}
		else if (m_body.Minv == 0)
		{
			Awake(0);
		}

		return 1;
	}

	if (_action->type == pe_action_register_coll_event::type_id)
	{
		pe_action_register_coll_event* action = (pe_action_register_coll_event*)_action;
		EventPhysCollision event;
		event.pEntity[0] = this;
		event.pForeignData[0] = m_pForeignData;
		event.iForeignData[0] = m_iForeignData;
		event.pEntity[1] = action->pCollider;
		event.pForeignData[1] = ((CPhysicalEntity*)action->pCollider)->m_pForeignData;
		event.iForeignData[1] = ((CPhysicalEntity*)action->pCollider)->m_iForeignData;
		event.pt = action->pt;
		event.n = action->n;
		event.idCollider = m_pWorld->GetPhysicalEntityId(action->pCollider);

		int i;
		for (i = 0; i < m_nParts && m_parts[i].id != action->partid[0]; i++)
			;
		RigidBody* pbody = GetRigidBody(i);
		event.vloc[0] =
		    is_unused(action->vSelf) ? pbody->v + (pbody->w ^ pbody->pos - action->pt) : action->vSelf;
		event.vloc[1] = action->v;
		event.mass[0] = pbody->M;
		event.mass[1] = action->collMass;
		event.partid[0] = action->partid[0];
		event.partid[1] = action->partid[1];
		event.idmat[0] = action->idmat[0];
		event.idmat[1] = action->idmat[1];
		event.penetration = event.radius = event.normImpulse = 0;
		m_pWorld->OnEvent(m_flags, &event);

		return 1;
	}

	return CPhysicalEntity::Action(_action, 1);
}

int CRigidEntity::RemoveCollider(CPhysicalEntity* pCollider, bool bRemoveAlways)
{
	int i;
	if (!bRemoveAlways)
	{
		for (i = 0; i < m_nColliders && m_pColliders[i] != pCollider; i++)
			;
		if (i < m_nColliders && (m_pColliderContacts[i] || m_pColliderConstraints[i]))
		{
			return i;
		}
	}
	i = CPhysicalEntity::RemoveCollider(pCollider, bRemoveAlways);
	if (i >= 0)
	{
		entity_contact *pContact, *pContactNext;
		if (m_pColliderContacts[i])
		{
			for (pContact = m_pColliderContacts[i];; pContact = pContactNext)
			{
				pContactNext = pContact->next;
				DetachContact(pContact, i, 0);
				m_pWorld->FreeContact(pContact);
				if (pContact->flags & contact_last)
				{
					break;
				}
			}
		}

		for (; i < m_nColliders; i++)
		{
			m_pColliderContacts[i] = m_pColliderContacts[i + 1];
			m_pColliderConstraints[i] = m_pColliderConstraints[i + 1];
		}
	}
	return i;
}

int CRigidEntity::AddCollider(CPhysicalEntity* pCollider)
{
	int nColliders = m_nColliders, nCollidersAlloc = m_nCollidersAlloc, i = CPhysicalEntity::AddCollider(pCollider);

	if (m_nCollidersAlloc > nCollidersAlloc)
	{
		entity_contact** pColliderContacts = m_pColliderContacts;
		memcpy(m_pColliderContacts = new entity_contact*[m_nCollidersAlloc], pColliderContacts,
		       nColliders * sizeof(m_pColliderContacts[0]));
		if (pColliderContacts)
		{
			delete[] pColliderContacts;
		}
		masktype* pColliderConstraints = m_pColliderConstraints;
		memcpy(m_pColliderConstraints = new masktype[m_nCollidersAlloc], pColliderConstraints,
		       nColliders * sizeof(m_pColliderConstraints[0]));
		if (pColliderConstraints)
		{
			delete[] pColliderConstraints;
		}
	}

	if (m_nColliders > nColliders)
	{
		for (int j = nColliders - 1; j >= i; j--)
		{
			m_pColliderContacts[j + 1] = m_pColliderContacts[j];
			m_pColliderConstraints[j + 1] = m_pColliderConstraints[j];
		}
		m_pColliderContacts[i] = 0;
		m_pColliderConstraints[i] = 0;
	}
	m_bJustLoaded = 0;

	return i;
}

int CRigidEntity::HasContactsWith(CPhysicalEntity* pent)
{
	int i;
	for (i = 0; i < m_nColliders && m_pColliders[i] != pent; i++)
		;
	return i == m_nColliders ? 0 : (m_pColliderContacts[i] != 0 || m_pColliderConstraints[i] != 0);
}

int CRigidEntity::HasCollisionContactsWith(CPhysicalEntity* pent)
{
	int i;
	for (i = 0; i < m_nColliders && m_pColliders[i] != pent; i++)
		;
	return i == m_nColliders ? 0 : m_pColliderContacts[i] != 0;
}

int CRigidEntity::HasConstraintContactsWith(CPhysicalEntity* pent, int flagsIgnore)
{
	int i;
	for (i = 0; i < m_nColliders && m_pColliders[i] != pent; i++)
		;
	if (i == m_nColliders)
	{
		return 0;
	}
	else if (!flagsIgnore)
	{
		return m_pColliderConstraints[i] != 0;
	}
	for (int j = 0; j < NMASKBITS && getmask(j) <= m_pColliderConstraints[i]; j++)
	{
		if (m_pColliderConstraints[i] & getmask(j) && !(m_pConstraintInfos[j].flags & flagsIgnore))
		{
			return 1;
		}
	}
	return 0;
}

int CRigidEntity::Awake(int bAwake, int iSource)
{
	if ((unsigned int)m_iSimClass > 6u)
	{
		CryLogError("Error: trying to awake deleted rigid entity");
		return -1;
	}
	int i;
	if (m_iSimClass <= 2)
	{
		for (i = m_nColliders - 1; i >= 0; i--)
		{
			if (m_pColliders[i]->m_iDeletionTime)
			{
				RemoveCollider(m_pColliders[i]);
			}
		}
		if ((m_iSimClass != bAwake + 1 || m_bAwake != bAwake && iSource == 5) &&
		    (m_body.Minv > 0 || m_body.v.len2() + m_body.w.len2() > 0 || bAwake == 0 || iSource == 1))
		{
			m_nSleepFrames = 0;
			m_bAwake = bAwake;
			m_iSimClass = m_bAwake + 1;
			m_pWorld->RepositionEntity(this, 2);
			// Exp1 integration from Main
			if (bAwake)
			{
				m_minAwakeTime = 0.1;
			}
		}
		if (m_body.Minv == 0)
		{
			for (i = 0; i < m_nColliders; i++)
			{
				if (m_pColliders[i] != this && m_pColliders[i]->GetMassInv() > 0)
				{
					m_pColliders[i]->Awake();
				}
			}
		}
	}
	else
	{
		m_bAwake = bAwake;
	}
	return m_iSimClass;
}

void CRigidEntity::AlertNeighbourhoodND()
{
	int i, iSimClass = m_iSimClass;
	masktype constraint_mask;
	m_iSimClass = 7; // notifies the others that we are being deleted

	for (i = 0, constraint_mask = 0; i < m_nColliders; constraint_mask |= m_pColliderConstraints[i++])
		;
	for (i = 0; i < NMASKBITS && getmask(i) <= constraint_mask; i++)
	{
		if (constraint_mask & getmask(i) && m_pConstraintInfos[i].pConstraintEnt &&
		    (unsigned int)m_pConstraintInfos[i].pConstraintEnt->m_iSimClass < 7u)
		{
			m_pConstraintInfos[i].pConstraintEnt->Awake();
		}
	}
	m_iSimClass = iSimClass;

	int flags = m_flags;
	if (m_iLastLog < 0 && m_pWorld->m_vars.lastTimeStep > 0)
	{
		m_flags |= pef_always_notify_on_deletion;
	}
	CPhysicalEntity::AlertNeighbourhoodND();
	m_flags = flags;
}

masktype CRigidEntity::MaskIgnoredColliders()
{
	int i;
	masktype constraint_mask;
	for (i = 0, constraint_mask = 0; i < m_nColliders; constraint_mask |= m_pColliderConstraints[i++])
	{
		if (m_pColliders[i]->IgnoreCollisionsWith(this) && !(m_pColliders[i]->m_bProcessed & 1))
		{
			AtomicAdd(&m_pColliders[i]->m_bProcessed, 1);
		}
	}
	for (i = 0; i < NMASKBITS && getmask(i) <= constraint_mask; i++)
	{
		if (constraint_mask & getmask(i) && m_pConstraintInfos[i].flags & constraint_ignore_buddy &&
		    !(m_pConstraints[i].pent[1]->m_bProcessed & 1))
		{
			AtomicAdd(&m_pConstraints[i].pent[1]->m_bProcessed, 1);
		}
	}
	return constraint_mask;
}

void CRigidEntity::UnmaskIgnoredColliders(masktype constraint_mask)
{
	int i;
	for (i = 0; i < m_nColliders; i++)
	{
		if (m_pColliders[i]->m_bProcessed & 1)
		{
			AtomicAdd(&m_pColliders[i]->m_bProcessed, -1);
		}
	}
	for (i = 0; i < NMASKBITS && getmask(i) <= constraint_mask; i++)
	{
		if (constraint_mask & getmask(i) && m_pConstraintInfos[i].flags & constraint_ignore_buddy)
		{
			AtomicAdd(&m_pConstraints[i].pent[1]->m_bProcessed,
			          -((int)m_pConstraints[i].pent[1]->m_bProcessed & 1));
		}
	}
}

bool CRigidEntity::IgnoreCollisionsWith(CPhysicalEntity* pent, int bCheckConstraints)
{
	int i;
	masktype constraint_mask;
	if (!m_pColliderConstraints)
	{
		return false;
	}
	for (i = 0, constraint_mask = 0; i < m_nColliders; constraint_mask |= m_pColliderConstraints[i++])
		;
	for (i = 0; i < NMASKBITS && getmask(i) <= constraint_mask; i++)
	{
		if (constraint_mask & getmask(i) && m_pConstraintInfos[i].flags & constraint_ignore_buddy &&
		    m_pConstraints[i].pent[1] == pent)
		{
			return true;
		}
	}
	if (bCheckConstraints)
	{
		for (i = 0; i < m_nColliders; i++)
		{
			if ((m_pColliderConstraints[i] || m_pColliders[i]->HasConstraintContactsWith(this)) &&
			    m_pColliders[i]->IgnoreCollisionsWith(pent, 0))
			{
				return true;
			}
		}
	}
	return false;
}

void CRigidEntity::FakeRayCollision(CPhysicalEntity* pent, float dt)
{
	ray_hit hit;
	if (m_pWorld->RayTraceEntity(pent, m_body.pos, m_body.v * dt, &hit))
	{
		EventPhysCollision epc;
		pe_action_impulse ai;
		epc.pEntity[0] = this;
		epc.pForeignData[0] = m_pForeignData;
		epc.iForeignData[0] = m_iForeignData;
		epc.pEntity[1] = pent;
		epc.pForeignData[1] = pent->m_pForeignData;
		epc.iForeignData[1] = pent->m_iForeignData;
		epc.idCollider = m_pWorld->GetPhysicalEntityId(pent);
		epc.pt = hit.pt;
		epc.n = hit.n;
		epc.vloc[0] = m_body.v;
		epc.mass[0] = m_body.M;
		epc.partid[0] = m_parts[0].id;
		epc.vloc[1].zero();
		epc.mass[1] = pent->GetMass(hit.ipart);
		epc.partid[1] = hit.partid;
		epc.idmat[0] = GetMatId(m_parts[0].pPhysGeom->pGeom->GetPrimitiveId(0, 0x40), 0);
		epc.idmat[1] = hit.surface_idx;
		epc.normImpulse = (hit.n * m_body.v) * -1.2f;
		epc.penetration = epc.radius = 0;
		epc.pEntContact = 0;
		m_pWorld->OnEvent(m_flags, &epc);

		ai.impulse = hit.n * epc.normImpulse;
		m_body.P = (m_body.v += ai.impulse) * m_body.M;
		ai.point = hit.pt;
		ai.ipart = hit.ipart;
		ai.impulse *= -m_body.M;
		ai.iApplyTime = 0;
		pent->Action(&ai, 1);
	}
}

int CRigidEntity::GetPotentialColliders(CPhysicalEntity**& pentlist, float dt)
{
	int i, j, nents, bSameGroup;
	masktype constraint_mask;
	if (m_body.Minv + m_body.v.len2() + m_body.w.len2() <= 0)
	{
		return 0;
	}
	const float inflatorValue = m_pWorld->m_vars.maxContactGap * 4 * isneg(m_iLastLog);
	Vec3 BBox[2] = {m_BBoxNew[0], m_BBoxNew[1]}, move = m_body.v * m_timeStepFull,
	     inflator = Vec3(inflatorValue, inflatorValue, inflatorValue);
	BBox[isneg(-move.x)].x += move.x;
	BBox[isneg(-move.y)].y += move.y;
	BBox[isneg(-move.z)].z += move.z;
	BBox[0] -= inflator;
	BBox[1] += inflator;

	// Only enable collisions for objects having mass >0 (in most cases animated objects)
	unsigned int collisionMask =
	    ent_sleeping_rigid | ent_rigid | ent_living | ent_independent | ent_sort_by_mass | ent_triggers;
	if (m_body.M > 0.0f)
	{
		collisionMask |= ent_terrain | ent_static;
	}
	nents = m_pWorld->GetEntitiesAround(BBox[0], BBox[1], pentlist, collisionMask, this);

	if (m_flags & ref_use_simple_solver)
	{
		for (i = 0; i < m_nColliders; i++)
		{
			entity_contact* pContact;
			for (j = 0, pContact = m_pColliderContacts[i]; pContact && j < 3; j++)
			{
				if (pContact->flags & contact_last)
				{
					break;
				}
			}
			AtomicAdd(
			    &m_pColliders[i]->m_bProcessed,
			    m_pWorld->m_vars.bSkipRedundantColldet &
				iszero((m_pColliders[i]->m_nParts * 2) + m_nParts - 3) &
				iszero(((CGeometry*)m_pColliders[i]->m_parts[0].pPhysGeomProxy->pGeom)->m_bIsConvex +
			               ((CGeometry*)m_parts[0].pPhysGeomProxy->pGeom)->m_bIsConvex - 2) &
				isneg(3 - j));
		}
	}
	constraint_mask = MaskIgnoredColliders();

	for (i = j = m_nNonRigidNeighbours = 0; i < nents; i++)
	{
		if (!(pentlist[i]->m_bProcessed & 1))
		{
			if (pentlist[i]->m_iSimClass > 2)
			{
				if (pentlist[i]->GetType() != PE_ARTICULATED)
				{
					pentlist[i]->Awake();
					if ((!m_pWorld->m_vars.bMultiplayer || pentlist[i]->m_iSimClass != 3))
					{
						m_pWorld->ScheduleForStep(pentlist[i]), m_nNonRigidNeighbours++;
					}
				}
				else if (dt > 0)
				{
					FakeRayCollision(pentlist[i], dt);
				}
			}
			else if (m_body.Minv <= 0)
			{
				if (pentlist[i] != this) // Exp1 integration from Main
				{
					pentlist[i]->Awake();
				}
			}
			else if (((pentlist[i]->m_BBox[1] - pentlist[i]->m_BBox[0]).len2() == 0 ||
			          AABB_overlap(BBox, pentlist[i]->m_BBox) || pentlist[i] == this) &&
			         pentlist[i] != this &&
			         ((bSameGroup = iszero(pentlist[i]->m_iGroup - m_iGroup)) & pentlist[i]->m_bMoved ||
			          pentlist[i]->m_iGroup == -1 ||
			          //! pentlist[i]->IsAwake() ||
			          !(pentlist[i]->IsAwake() |
			            ((m_flags & ref_use_simple_solver | pentlist[i]->m_flags & ref_use_simple_solver) &
			             -bSameGroup)) ||
			          m_pWorld->m_pGroupNums[pentlist[i]->m_iGroup] < m_pWorld->m_pGroupNums[m_iGroup]))
			{
				pentlist[j++] = pentlist[i];
				if (m_iLastLog < 0)
				{
					pentlist[i]->Awake();
				}
			}
		}
	}

	if (m_flags & ref_use_simple_solver)
	{
		for (i = 0; i < m_nColliders; i++)
		{
			AtomicAdd(&m_pColliders[i]->m_bProcessed, -((int)m_pColliders[i]->m_bProcessed & 1));
		}
	}
	UnmaskIgnoredColliders(constraint_mask);

	return j;
}

int CRigidEntity::CheckForNewContacts(geom_world_data* pgwd0, intersection_params* pip, int& itmax, Vec3 sweep,
                                      int iStartPart, int nParts)
{
	CPhysicalEntity** pentlist;
	geom_world_data gwd1;
	int ient, nents, i, j, icont, ncontacts, ipt, imask, nTotContacts = 0, nAreas = 0, nAreaPt = 0,
							     bHasMatSubst = 0, ient1, j1, bSquashy;
	RigidBody* pbody[2];
	geom_contact* pcontacts;
	IGeometry* pGeom;
	CRayGeom aray;
	float tsg = !pip->bSweepTest ? 1.0f : -1.0f, tmax = 1E10f * (1 - tsg), vrel_min = pip->vrel_min;
	bool bStopAtFirstTri = pip->bStopAtFirstTri, bCheckBBox;
	Vec3 sz, BBox[2], prevOutsidePivot = pip->ptOutsidePivot[0];
	EventPhysBBoxOverlap event;
	EventPhysCollision epc;
	int bPivotFilled = isneg(pip->ptOutsidePivot[0].x - 1E9f), bThreadSafe = pip->bThreadSafe;
	primitives::box bbox;
	itmax = -1;
	nParts = (m_nParts & nParts >> 31) | max(nParts, 0);
	event.pEntity[0] = this;
	event.pForeignData[0] = m_pForeignData;
	event.iForeignData[0] = m_iForeignData;

	nents = GetPotentialColliders(
	    pentlist, pip->time_interval * ((int)pip->bSweepTest & ~-iszero((int)m_flags & ref_small_and_fast)));
	pip->bKeepPrevContacts = false;

	for (i = iStartPart; i < iStartPart + nParts; i++)
	{
		if (m_parts[i].flagsCollider)
		{
			pgwd0->offset = m_pNewCoords->pos + m_pNewCoords->q * m_parts[i].pNewCoords->pos;
			pgwd0->R = Matrix33(m_pNewCoords->q * m_parts[i].pNewCoords->q);
			pgwd0->scale = m_parts[i].pNewCoords->scale;
			m_parts[i].pPhysGeomProxy->pGeom->GetBBox(&bbox);
			bbox.Basis *= pgwd0->R.T();
			sz = (bbox.size * bbox.Basis.Fabs()) * m_parts[i].pNewCoords->scale;
			BBox[0] = BBox[1] =
			    m_pNewCoords->pos +
			    m_pNewCoords->q * (m_parts[i].pNewCoords->pos +
			                       m_parts[i].pNewCoords->q * bbox.center * m_parts[i].pNewCoords->scale);
			BBox[0] -= sz;
			BBox[1] += sz;
			BBox[isneg(-sweep.x)].x += sweep.x;
			BBox[isneg(-sweep.y)].y += sweep.y;
			BBox[isneg(-sweep.z)].z += sweep.z;
			pGeom = m_parts[i].pPhysGeomProxy->pGeom;
			if (!(m_parts[i].flags & geom_squashy))
			{
				pip->vrel_min = vrel_min, bSquashy = 0;
			}
			else
			{
				if (!pip->bSweepTest)
				{
					pip->vrel_min = 1E10f;
				}
				else
				{
					continue;
				}
				if (m_timeCanopyFallen < 3.0f)
				{
					const Vec3 gravAbs = m_gravity.abs();
					ipt = idxmax3((const float*)&gravAbs);
					aray.m_dirn.zero()[ipt] = sgn(m_gravity[ipt]);
					aray.m_dirn = ((aray.m_dirn) * m_qNew) * m_parts[i].q;
					aray.m_ray.origin = m_parts[i].pPhysGeom->origin;
					Vec3 gloc = bbox.Basis.GetColumn(ipt);
					const Vec3 boxGloc(bbox.size.x * gloc.y * gloc.z, bbox.size.y * gloc.x * gloc.z,
					                   bbox.size.z * gloc.x * gloc.y);
					ipt = idxmin3((const float*)&boxGloc);
					aray.m_ray.dir =
					    aray.m_dirn * min((bbox.size.x + bbox.size.y + bbox.size.z) * (1.0f / 3),
					                      bbox.size[ipt] / max(0.001f, gloc[ipt]));
					aray.m_ray.origin -= aray.m_ray.dir;
					aray.m_ray.dir *= 2;
					pGeom = &aray;
					if (m_pWorld->m_vars.iDrawHelpers & 64 && m_pWorld->m_pRenderer)
					{
						m_pWorld->m_pRenderer->DrawLine(
						    pgwd0->R * aray.m_ray.origin * pgwd0->scale + pgwd0->offset,
						    pgwd0->R * (aray.m_ray.origin + aray.m_ray.dir) * pgwd0->scale +
							pgwd0->offset,
						    7, 1);
					}
				}
				bSquashy = 1;
			}

			for (ient = 0; ient < nents; ient++)
			{
				for (j = 0,
				    bCheckBBox = (pentlist[ient]->m_BBox[1] - pentlist[ient]->m_BBox[0]).len2() > 0;
				     j < (pentlist[ient]->m_nParts & ~(-pentlist[ient]->m_iSimClass >> 31 & -bSquashy));
				     j++)
				{
					if ((pentlist[ient]->m_parts[j].flags &
					     (m_parts[i].flagsCollider | geom_log_interactions)) &&
					    !(pentlist[ient] == this && !CheckSelfCollision(i, j)) &&
					    (m_nParts + pentlist[ient]->m_nParts == 2 ||
					     (IsAwake(i) || pentlist[ient]->IsAwake(j)) &&
					         (!bCheckBBox ||
					          AABB_overlap(BBox, pentlist[ient]->m_parts[j].pNewCoords->BBox))))
					{
						if (pentlist[ient]->m_parts[j].flags & geom_log_interactions)
						{
							event.pEntity[1] = pentlist[ient];
							event.pForeignData[1] = pentlist[ient]->m_pForeignData;
							event.iForeignData[1] = pentlist[ient]->m_iForeignData;
							m_pWorld->OnEvent(m_flags, &event);
							if (!(pentlist[ient]->m_parts[j].flags &
							      m_parts[i].flagsCollider))
							{
								continue;
							}
						}
						if (pentlist[ient]->m_parts[j].flags & geom_car_wheel &&
						    pentlist[ient]->GetMassInv() * 5 > m_body.Minv)
						{
							continue;
						}
						gwd1.offset = pentlist[ient]->m_pNewCoords->pos +
						              pentlist[ient]->m_pNewCoords->q *
						                  pentlist[ient]->m_parts[j].pNewCoords->pos;
						gwd1.R = Matrix33(pentlist[ient]->m_pNewCoords->q *
						                  pentlist[ient]->m_parts[j].pNewCoords->q);
						gwd1.scale = pentlist[ient]->m_parts[j].pNewCoords->scale;
						pbody[1] = pentlist[ient]->GetRigidBody(j);
						gwd1.v = pbody[1]->v;
						gwd1.w = pbody[1]->w;
						gwd1.centerOfMass = pbody[1]->pos;
						pip->ptOutsidePivot[0] =
						    m_parts[i].maxdim < pentlist[ient]->m_parts[j].maxdim
							? prevOutsidePivot
							: Vec3(1E11f, 1E11f, 1E11f);

						if (pGeom != &aray)
						{
							ncontacts = pGeom->Intersect(
							    pentlist[ient]->m_parts[j].pPhysGeomProxy->pGeom, pgwd0,
							    &gwd1, pip, pcontacts);
							for (icont = 0; icont < ncontacts; icont++)
							{
								pcontacts[icont].id[0] =
								    GetMatId(pcontacts[icont].id[0], i);
								pcontacts[icont].id[1] =
								    pentlist[ient]->GetMatId(pcontacts[icont].id[1], j);
								if (bHasMatSubst && pentlist[ient]->m_id == -1)
								{
									for (ient1 = 0; ient1 < nents; ient1++)
									{
										for (j1 = 0;
										     j1 < pentlist[ient1]->m_nParts;
										     j1++)
										{
											if (pentlist[ient1]
											            ->m_parts[j1]
											            .flags &
											        geom_mat_substitutor &&
											    pentlist[ient1]
											        ->m_parts[j1]
											        .pPhysGeom->pGeom
											        ->PointInsideStatus(
												    ((pcontacts[icont]
											                  .center -
											              pentlist[ient1]
											                  ->m_pos) *
											                 pentlist[ient1]
											                     ->m_qrot -
											             pentlist[ient1]
											                 ->m_parts[j1]
											                 .pos) *
												    pentlist[ient1]
													->m_parts[j1]
													.q))
											{
												pcontacts[icont].id[1] =
												    pentlist[ient1]->GetMatId(
													pentlist[ient1]
													    ->m_parts
														[j1]
													    .pPhysGeom
													    ->surface_idx,
													j1);
											}
										}
									}
								}
								g_CurColliders[nTotContacts] = pentlist[ient];
								g_CurCollParts[nTotContacts][0] = i;
								g_CurCollParts[nTotContacts][1] = j;
								if (pcontacts[icont].parea)
								{
									for (ipt = 0; ipt < pcontacts[icont].parea->npt;
									     ipt++)
									{
										imask = pcontacts[icont]
										            .parea->piPrim[0][ipt] >>
										        31;
										(pcontacts[icont]
										     .parea->piPrim[0][ipt] &=
										 ~imask) |=
										    pcontacts[icont].iPrim[0] & imask;
										(pcontacts[icont]
										     .parea->piFeature[0][ipt] &=
										 ~imask) |=
										    (pcontacts[icont].iFeature[0] &
										     imask) |
										    (imask & 1 << 31);
										imask = pcontacts[icont]
										            .parea->piPrim[1][ipt] >>
										        31;
										(pcontacts[icont]
										     .parea->piPrim[1][ipt] &=
										 ~imask) |=
										    pcontacts[icont].iPrim[1] & imask;
										(pcontacts[icont]
										     .parea->piFeature[1][ipt] &=
										 ~imask) |=
										    (pcontacts[icont].iFeature[1] &
										     imask) |
										    (imask & 1 << 31);
									}
								}
								if (pcontacts[icont].n * pcontacts[icont].dir > 0.2f)
								{
									pcontacts[icont].t = -1.0f;
								}
								if ((m_parts[i].flags |
								     pentlist[ient]->m_parts[j].flags) &
								        (geom_manually_breakable |
								         geom_no_coll_response) &&
								    m_flags & pef_log_collisions & (icont - 1) >> 31)
								{
									epc.pEntity[0] = this;
									epc.pForeignData[0] = m_pForeignData;
									epc.iForeignData[0] = m_iForeignData;
									epc.pEntity[1] = pentlist[ient];
									epc.pForeignData[1] =
									    pentlist[ient]->m_pForeignData;
									epc.iForeignData[1] =
									    pentlist[ient]->m_iForeignData;
									epc.pt = pcontacts[icont].pt;
									epc.n = -pcontacts[icont].n;
									epc.idCollider = m_pWorld->GetPhysicalEntityId(
									    epc.pEntity[1]);
									epc.partid[0] = m_parts[i].id;
									epc.partid[1] = pentlist[ient]->m_parts[j].id;
									pbody[0] = GetRigidBody(i);
									for (ipt = 0; ipt < 2; ipt++)
									{
										epc.idmat[ipt] =
										    pcontacts[icont].id[ipt];
										epc.vloc[ipt] =
										    pbody[ipt]->v +
										    (pbody[ipt]->w ^
										     epc.pt - pbody[ipt]->pos);
										epc.mass[ipt] = pbody[ipt]->M;
									}
									epc.penetration = epc.normImpulse = epc.radius =
									    0;
									if ((pentlist[ient]->m_parts[j].flags &
									     (geom_manually_breakable |
									      geom_no_coll_response)) ==
									        geom_manually_breakable &&
									    !m_pWorld->SignalEvent(&epc, 0))
									{
										pentlist[ient]->m_parts[j].flags |=
										    geom_no_coll_response;
									}
									if (pcontacts[icont].vel *
									        ((m_parts[i].flags |
									          pentlist[ient]->m_parts[j].flags) &
									         geom_no_coll_response) >
									    0)
									{
										pcontacts[icont].vel = 0;
										Vec3 vel = m_body.v.normalized();
										sz = (m_BBox[1] - m_BBox[0]) * 0.5f;
										const Vec3 velVec(
										    (vel.x * vel.y * sz.x * sz.y) +
											(vel.x * vel.z * sz.x * sz.z),
										    (vel.y * vel.x * sz.y * sz.x) +
											(vel.y * vel.z * sz.y * sz.z),
										    (vel.z * vel.x * sz.z * sz.x) +
											(vel.z * vel.y * sz.z * sz.y));
										sz[idxmax3((const float*)&velVec)] *=
										    -1;
										sz.x *= sgnnz(vel.x);
										sz.y *= sgnnz(vel.y);
										sz.z *= sgnnz(vel.z);
										epc.radius = (vel ^ sz).len();
										if (pentlist[ient]->m_parts[j].flags &
										        geom_no_coll_response &&
										    m_body.v.len2() >
										        sqr(epc.radius * 30.0f))
										{
											m_pWorld->OnEvent(m_flags,
											                  &epc);
										}
									}
								}
								if (pcontacts[icont].vel > 0 &&
								    pcontacts[icont].t * tsg > tmax * tsg)
								{
									tmax = pcontacts[icont].t;
									itmax = nTotContacts;
								}
								if (++nTotContacts ==
								    sizeof(g_CurColliders) / sizeof(g_CurColliders[0]))
								{
									goto CollidersNoMore;
								}
							}
						}
						else
						{
							ncontacts =
							    pentlist[ient]->m_parts[j].pPhysGeomProxy->pGeom->Intersect(
								pGeom, &gwd1, pgwd0, pip, pcontacts);
							for (icont = 0; icont < ncontacts; icont++)
							{
								int imat0 =
								    pentlist[ient]->GetMatId(pcontacts[icont].id[0], j);
								pcontacts[icont].id[0] =
								    GetMatId(pcontacts[icont].id[1], i);
								pcontacts[icont].id[1] = imat0;
								g_CurColliders[nTotContacts] = pentlist[ient];
								g_CurCollParts[nTotContacts][0] = i;
								g_CurCollParts[nTotContacts][1] = j;
								pcontacts[icont].t =
								    ((aray.m_ray.dir * aray.m_dirn) * pgwd0->scale -
								     pcontacts[icont].t) *
								    isneg(ncontacts - 2 - icont);
								pcontacts[icont].dir = pgwd0->R * -aray.m_dirn;
								if (++nTotContacts ==
								    sizeof(g_CurColliders) / sizeof(g_CurColliders[0]))
								{
									goto CollidersNoMore;
								}
							}
						}
						pip->bKeepPrevContacts = pip->bKeepPrevContacts || ncontacts > 0;
						pip->bThreadSafe |= isneg(-ncontacts);
					}
					else
					{
						bHasMatSubst |= pentlist[ient]->m_parts[j].flags & geom_mat_substitutor;
					}
				}
			}
		}
	}
CollidersNoMore:
	pip->bStopAtFirstTri = bStopAtFirstTri;
	pip->ptOutsidePivot[0] = prevOutsidePivot;
	pip->bThreadSafe = bThreadSafe;
	pip->vrel_min = vrel_min;
	g_nLastContacts = nTotContacts;

	return nTotContacts;
}

void CRigidEntity::CleanupAfterContactsCheck()
{
	for (int i = 0; i < g_nLastContacts; i++)
	{
		if ((g_CurColliders[i]->m_parts[g_CurCollParts[i][1]].flags &
		     (geom_manually_breakable | geom_no_coll_response)) ==
		    (geom_manually_breakable | geom_no_coll_response))
		{
			g_CurColliders[i]->m_parts[g_CurCollParts[i][1]].flags &= ~geom_no_coll_response;
		}
	}
}

int CRigidEntity::RemoveContactPoint(CPhysicalEntity* pCollider, const Vec3& pt, float mindist2)
{
	int i;
	for (i = 0; i < m_nColliders && m_pColliders[i] != pCollider; i++)
		;
	if (i < m_nColliders)
	{
		entity_contact* pContact;
		for (pContact = m_pColliderContacts[i];
		     pContact && pContact != CONTACT_END(m_pContactStart) && (pContact->pt[0] - pt).len2() > mindist2;
		     pContact = pContact->next)
			;
		if (pContact && pContact != CONTACT_END(m_pContactStart))
		{
			if (!(m_flags & ref_use_simple_solver))
			{
				DetachContact(pContact, i);
				m_pWorld->FreeContact(pContact);
			}
			return 1;
		}
	}
	return 0;
}

entity_contact* CRigidEntity::RegisterContactPoint(int idx, const Vec3& pt, const geom_contact* pcontacts, int iPrim0,
                                                   int iFeature0, int iPrim1, int iFeature1, int flags,
                                                   float penetration)
{
	if (!(m_pWorld->m_vars.bUseDistanceContacts | (m_flags & ref_use_simple_solver)) && penetration == 0 &&
	    GetType() != PE_ARTICULATED)
	{
		return 0;
	}
	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	float min_dist2;
	bool bNoCollResponse = false;
	entity_contact* pContact;
	int j, bUseSimpleSolver = iszero((int)m_flags & ref_use_simple_solver) ^ 1;
	RigidBody* pbody1 = g_CurColliders[idx]->GetRigidBody(g_CurCollParts[idx][1], 1);

	if (!((m_parts[g_CurCollParts[idx][0]].flags | g_CurColliders[idx]->m_parts[g_CurCollParts[idx][1]].flags) &
	      geom_no_coll_response))
	{
		if (!(m_parts[g_CurCollParts[idx][0]].flags & geom_squashy))
		{
			min_dist2 = sqr(min(m_parts[g_CurCollParts[idx][0]].minContactDist,
			                    g_CurColliders[idx]->m_parts[g_CurCollParts[idx][1]].minContactDist));
		}
		else
		{
			Vec3 sz = m_BBox[1] - m_BBox[0];
			min_dist2 = sqr(min(min(sz.x, sz.y), sz.z) * 0.15f);
		}
		if (bUseSimpleSolver)
		{
			if (g_CurColliders[idx]->RemoveContactPoint(this, pt, min_dist2) == 1)
			{
				return 0;
			}
		}
		else if (m_pWorld->m_vars.bUseDistanceContacts)
		{
			g_CurColliders[idx]->RemoveContactPoint(this, pt, min_dist2);
		}

		for (pContact = m_pContactStart;
		     pContact != CONTACT_END(m_pContactStart) &&
		     !((pContact->flags & contact_new || pContact->penetration == 0) &&
		       pContact->pent[1] == g_CurColliders[idx] && pContact->pbody[1] == pbody1 &&
		       (((pContact->iPrim[0] - iPrim0) | (pContact->iFeature[0] - iFeature0) |
		         (pContact->iPrim[1] - iPrim1) | (pContact->iFeature[1] - iFeature1) |
		         (bUseSimpleSolver ^ 1)) == 0 ||
		        ((pContact->flags & contact_new)
		             ? (pContact->pt[0] - pt).len2()
		             : (pContact->pbody[0]->q * pContact->ptloc[0] + pContact->pbody[0]->pos - pt).len2()) <
		            min_dist2));
		     pContact = pContact->next)
			;
		if (pContact == CONTACT_END(m_pContactStart))
		{ // no existing point that is close enough to this one
			for (pContact = m_pContactStart, j = 0;
			     pContact != CONTACT_END(m_pContactStart) &&
			     (pContact->flags & contact_new || pContact->penetration == 0);
			     pContact = pContact->next, j++)
				;
			if (pContact != CONTACT_END(m_pContactStart))
			{
				DetachContact(pContact);
			}
			else
			{
				if (j >= m_pWorld->m_vars.nMaxEntityContacts)
				{
					return 0;
				}
				pContact = m_pWorld->AllocContact();
			}
			AttachContact(pContact, AddCollider(g_CurColliders[idx]));
			g_CurColliders[idx]->AddCollider(this);
		}
		else if (bUseSimpleSolver || (pContact->penetration == 0 || pContact->flags & contact_new) &&
		                                 (pContact->penetration >= penetration || flags & contact_2b_verified))
		{
			return 0;
		}
		//(!m_pWorld->m_vars.bUseDistanceContacts ?
		//(pContact->penetration==0 || pContact->flags&contact_new) :
		//(flags & contact_2b_verified && pContact->penetration==0))
	}
	else
	{
		bNoCollResponse = true;
		static entity_contact g_NoRespCnt;
		pContact = &g_NoRespCnt;
	}

	pContact->pt[0] = pContact->pt[1] = pt;
	pContact->n = -pcontacts[idx].n;
	pContact->dir = pcontacts[idx].dir;
	pContact->pent[0] = this;
	pContact->pent[1] = g_CurColliders[idx];
	pContact->ipart[0] = g_CurCollParts[idx][0];
	pContact->ipart[1] = g_CurCollParts[idx][1];
	pContact->pbody[0] = GetRigidBody(g_CurCollParts[idx][0]);
	pContact->pbody[1] = pbody1;
	pContact->iPrim[0] = max(0, iPrim0);
	pContact->iFeature[0] = iFeature0;
	pContact->iPrim[1] = max(0, iPrim1);
	pContact->iFeature[1] = iFeature1;
	pContact->penetration = penetration;

	pContact->vrel = pContact->pbody[0]->v + (pContact->pbody[0]->w ^ pContact->pt[0] - pContact->pbody[0]->pos) -
	                 pContact->pbody[1]->v - (pContact->pbody[1]->w ^ pContact->pt[0] - pContact->pbody[1]->pos);
	pContact->id[0] = pcontacts[idx].id[0];
	pContact->id[1] = pcontacts[idx].id[1];
	pContact->friction = m_pWorld->GetFriction(pContact->id[0], pContact->id[1],
	                                           pContact->vrel.len2() > sqr(m_pWorld->m_vars.maxContactGap * 5));
	pContact->friction *= iszero((int)pContact->pent[1]->m_parts[pContact->ipart[1]].flags & geom_car_wheel);
	pContact->friction = max(m_minFriction, pContact->friction);
	pContact->flags = (pContact->flags & ~contact_archived) | (flags & ~contact_last);

	if (bNoCollResponse)
	{
		int bLastInGroup = flags & contact_last &&
		                   (idx == g_nLastContacts - 1 || g_CurColliders[idx] != g_CurColliders[idx + 1] ||
		                    g_CurCollParts[idx][1] != g_CurCollParts[idx + 1][1]);
		float r = 0;
		if (g_idx0NoColl < 0)
		{
			g_idx0NoColl = idx;
		}
		if (bLastInGroup)
		{
			int i, npt;
			Vec3 center;
			for (i = g_idx0NoColl, center.zero(), npt = 0; i <= idx; npt += pcontacts[i++].nborderpt)
			{
				for (j = 0; j < pcontacts[i].nborderpt; j++)
				{
					center += pcontacts[i].ptborder[j];
				}
			}
			if (npt)
			{
				for (i = g_idx0NoColl, center /= npt; i <= idx; i++)
				{
					for (j = 0; j < pcontacts[i].nborderpt; j++)
					{
						r = max(r, (pcontacts[i].ptborder[j] - center).len());
					}
				}
				pContact->pt[0] = pContact->pt[1] = center;
			}
			g_idx0NoColl = -1;
		}
		ArchiveContact(pContact, 0, bLastInGroup, r);
		return 0;
	}

	if (bUseSimpleSolver)
	{
		Vec3 ptloc[2], unproj = pcontacts[idx].dir * pcontacts[idx].t;
		pContact->iNormal = isneg((pContact->iFeature[0] & 0xE0) - (pContact->iFeature[1] & 0xE0));
		pContact->nloc = pContact->n * pContact->pbody[pContact->iNormal]->q;
		ptloc[0] = (pt - pContact->pbody[0]->pos - unproj) * pContact->pbody[0]->q;
		ptloc[1] = (pt - pContact->pbody[1]->pos) * pContact->pbody[1]->q;
		if (!(flags & contact_inexact))
		{
			pContact->ptloc[0] = ptloc[0];
			pContact->ptloc[1] = ptloc[1];
		}
		else
		{
			Vec3 ptfeat[4];
			if (!(pContact->iFeature[0] & pContact->iFeature[1] & 0xE0))
			{
				j = pContact->iNormal; // vertex-face contact
				geom* ppart = pContact->pent[j]->m_parts + pContact->ipart[j];
				// get geometry-CS coordinates of features in ptloc
				ppart->pPhysGeomProxy->pGeom->GetFeature(pContact->iPrim[j], pContact->iFeature[j],
				                                         ptfeat);
				// store world-CS coords in pContact->ptloc
				pContact->ptloc[j] = pContact->pent[j]->m_pNewCoords->q *
				                         (ppart->pNewCoords->q * ptfeat[0] * ppart->pNewCoords->scale +
				                          ppart->pNewCoords->pos) +
				                     pContact->pent[j]->m_pNewCoords->pos;
				Vec3 nfeat = pContact->pent[j]->m_pNewCoords->q * ppart->pNewCoords->q *
				             (ptfeat[1] - ptfeat[0] ^ ptfeat[2] - ptfeat[0]);
				ppart = pContact->pent[j ^ 1]->m_parts + pContact->ipart[j ^ 1];
				ppart->pPhysGeomProxy->pGeom->GetFeature(pContact->iPrim[j ^ 1],
				                                         pContact->iFeature[j ^ 1], ptfeat);
				pContact->ptloc[j ^ 1] =
				    pContact->pent[j ^ 1]->m_pNewCoords->q *
					(ppart->pNewCoords->q * ptfeat[0] * ppart->pNewCoords->scale +
				         ppart->pNewCoords->pos) +
				    pContact->pent[j ^ 1]->m_pNewCoords->pos;
				pContact->ptloc[0] += unproj;
				pContact->ptloc[j] = pt - nfeat * ((nfeat * (pt - pContact->ptloc[j])) / nfeat.len2());
				pContact->ptloc[0] =
				    (pContact->ptloc[0] - pContact->pbody[0]->pos - unproj) * pContact->pbody[0]->q;
				pContact->ptloc[1] =
				    (pContact->ptloc[1] - pContact->pbody[1]->pos) * pContact->pbody[1]->q;
			}
			else
			{ // edge-edge contact
				for (j = 0; j < 2; j++)
				{
					geom* ppart = pContact->pent[j]->m_parts + pContact->ipart[j];
					float rscale = ppart->scale == 1.0f ? 1.0f : 1.0f / ppart->scale;
					// get edge in geom CS to ptloc[1]-ptloc[2]
					ppart->pPhysGeomProxy->pGeom->GetFeature(pContact->iPrim[j],
					                                         pContact->iFeature[j], ptfeat + 1);
					ptfeat[0] = (((pt - pContact->pent[j]->m_pNewCoords->pos - unproj * (j ^ 1)) *
					                  pContact->pent[j]->m_pNewCoords->q -
					              ppart->pNewCoords->pos) *
					             rscale) *
					            ppart->pNewCoords->q; // ptloc[0] <- contact point in geom CS
					ptfeat[0] =
					    ptfeat[1] + (ptfeat[2] - ptfeat[1]) *
							    (((ptfeat[0] - ptfeat[1]) * (ptfeat[2] - ptfeat[1])) /
					                     (ptfeat[2] - ptfeat[1]).len2());
					// transform geom CS->world CS
					ptfeat[0] = pContact->pent[j]->m_pNewCoords->q *
					                (ppart->pNewCoords->q * ptfeat[0] * ppart->pNewCoords->scale +
					                 ppart->pNewCoords->pos) +
					            pContact->pent[j]->m_pNewCoords->pos;
					pContact->ptloc[j] = (ptfeat[0] - pContact->pbody[j]->pos) *
					                     pContact->pbody[j]->q; // world CS->body CS
				}
			}
			for (j = 0; j < 2 &&
			            (pContact->ptloc[j] - ptloc[j]).len2() < sqr(m_pWorld->m_vars.maxContactGapSimple);
			     j++)
				;
			if (j < 2)
			{
				pContact->ptloc[0] = ptloc[0], pContact->ptloc[1] = ptloc[1];
			}
		}
		pContact->vreq.zero();
		pContact->r.zero();
		pContact->dP.zero();
		UpdatePenaltyContact(pContact, m_lastTimeStep);
	}
	else
	{
		pContact->Pspare = 0;
		pContact->K.SetZero();
		GetContactMatrix(pContact->pt[0], pContact->ipart[0], pContact->K);
		g_CurColliders[idx]->GetContactMatrix(pContact->pt[1], pContact->ipart[1], pContact->K);

		float e = (m_pWorld->m_BouncinessTable[pcontacts[idx].id[0] & (NSURFACETYPES - 1)] +
		           m_pWorld->m_BouncinessTable[pcontacts[idx].id[1] & (NSURFACETYPES - 1)]) *
		          0.5f;
		if ( // m_body.M<2 &&	// bounce only smalll objects
		     // m_nParts+g_CurColliders[idx]->m_nParts<=4 && // bounce only simple objects (bouncing is
		     // dangerous)
		    !bUseSimpleSolver && e > 0 && pContact->vrel * pContact->n < -m_pWorld->m_vars.minBounceSpeed &&
		    (g_CurColliders[idx]->m_parts[pContact->ipart[1]].mass > m_body.M ||
		     !(g_CurColliders[idx]->m_parts[pContact->ipart[1]].flags & geom_monitor_contacts)))
		{ // apply bounce impulse if needed
			pe_action_impulse ai;
			ai.impulse.x =
			    (pContact->vrel * pContact->n) * -(1 + e) / (pContact->n * pContact->K * pContact->n);
			if (sqr(ai.impulse.x * pContact->pbody[0]->Minv) <
			    max(pContact->pbody[0]->v.len2(), pContact->pbody[1]->v.len2()) * sqr(e + 1.1f))
			{
				ArchiveContact(pContact, ai.impulse.x);
				ai.impulse = pContact->n * ai.impulse.x;
				ai.point = pContact->pt[0];
				ai.partid = m_parts[pContact->ipart[0]].id;
				ai.iApplyTime = 0;
				ai.iSource = 4;
				Action(&ai);
				ai.impulse.Flip();
				ai.partid = g_CurColliders[idx]->m_parts[pContact->ipart[1]].id;
				g_CurColliders[idx]->Action(&ai, 1);
			}
		}

		if (penetration > 0)
		{
			Vec3 sz = (m_BBox[1] - m_BBox[0]);
			if (penetration > (sz.x + sz.y + sz.z) * 0.06f)
			{
				pContact->n = pcontacts[idx].dir;
			}
			pContact->vreq = penetration > m_pWorld->m_vars.maxContactGap
			                     ? pContact->n * min(m_pWorld->m_vars.maxUnprojVel,
			                                         (penetration - m_pWorld->m_vars.maxContactGap) *
			                                             m_pWorld->m_vars.unprojVelScale)
			                     : Vec3();
		}
		else
		{
			pContact->ptloc[0] = (pt - pContact->pbody[0]->pos) * pContact->pbody[0]->q;
			pContact->ptloc[1] = (pt - pContact->pbody[1]->pos) * pContact->pbody[1]->q;
			pContact->iNormal = isneg((pContact->iFeature[0] & 0xE0) - (pContact->iFeature[1] & 0xE0));
			pContact->nloc = pContact->n * pContact->pbody[pContact->iNormal]->q;
			pContact->vreq.zero();
		}
	}

	return pContact;
}

void CRigidEntity::UpdatePenaltyContacts(float time_interval)
{
	entity_contact *pContact, *pContactNext;

	m_bStable = max(m_bStable, isneg(2 - m_nContacts) * 2);
	for (pContact = m_pContactStart; pContact != CONTACT_END(m_pContactStart); pContact = pContactNext)
	{
		pContactNext = pContact->next;
		UpdatePenaltyContact(pContact, time_interval);
		if (m_bStable && pContact->pent[1]->m_flags & ref_use_simple_solver)
		{
			((CRigidEntity*)pContact->pent[1])->m_bStable = 2;
		}
	}
}

int CRigidEntity::UpdatePenaltyContact(entity_contact* pContact, float time_interval)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	static float g_timeInterval = 0.01f;
	static float g_rtimeInterval = 100.0f;

	int j, bRemoveContact, bCanPull;
	Vec3 dp, n, vrel, dP, r;
	float dpn, dptang2, dPn, dPtang2;
	Matrix33 rmtx, rmtx1;

	if (g_timeInterval != time_interval)
	{
		g_rtimeInterval = 1.0f / (g_timeInterval = time_interval);
	}

	for (j = 0; j < 2; j++)
	{
		pContact->pt[j] = pContact->pbody[j]->q * pContact->ptloc[j] + pContact->pbody[j]->pos;
	}
	pContact->n = n = pContact->pbody[pContact->iNormal]->q * pContact->nloc;
	dp = pContact->pt[0] - pContact->pt[1];
	dpn = dp * n;
	dptang2 = (dp - n * dpn).len2();
	bRemoveContact = 0;
	bCanPull = isneg(m_impulseTime - 1E-6f);

	if (dpn < m_pWorld->m_vars.maxContactGapSimple)
	{
		pContact->r = dp;
		dp *= g_rtimeInterval * m_pWorld->m_vars.penaltyScale;
		vrel = pContact->pbody[0]->v + (pContact->pbody[0]->w ^ pContact->pt[0] - pContact->pbody[0]->pos);
		vrel -= pContact->pbody[1]->v + (pContact->pbody[1]->w ^ pContact->pt[1] - pContact->pbody[1]->pos);
		pContact->vrel = vrel;
		dp += vrel;

		pContact->K.SetZero();
		for (j = 0; j < 2; j++)
		{
			r = pContact->pt[j] - pContact->pbody[j]->pos;
			((crossproduct_matrix(r, rmtx)) *= pContact->pbody[j]->Iinv) *= crossproduct_matrix(r, rmtx1);
			pContact->K -= rmtx;
			for (int idx = 0; idx < 3; idx++)
			{
				pContact->K(idx, idx) += pContact->pbody[j]->Minv;
			}
		}

		dP = dp * (-dp.len2() / (dp * pContact->K * dp));
		dPn = dP * n;
		dPtang2 = (dP - n * dPn).len2();
		if (dptang2 > sqr_signed(-dpn * pContact->friction) * bCanPull &&
		    dPtang2 > sqr_signed(dPn * pContact->friction))
		{
			dP = (dP - n * dPn).normalized() * (max(dPn, 0.0f) * pContact->friction) + n * dPn;
			bRemoveContact = isneg(sqr(m_pWorld->m_vars.maxContactGapSimple) - dptang2);
		}
		if (dP * n > min(0.0f, (pContact->dP * n) * -2.1f * bCanPull))
		{
			pContact->dP = dP;
			for (j = 0; j < 2; j++, dP.Flip())
			{
				r = pContact->pt[j] - pContact->pbody[j]->pos;
				pContact->pbody[j]->P += dP;
				pContact->pbody[j]->L += r ^ dP;
				pContact->pbody[j]->v = pContact->pbody[j]->P * pContact->pbody[j]->Minv;
				pContact->pbody[j]->w = pContact->pbody[j]->Iinv * pContact->pbody[j]->L;
			}
		}
		else
		{
			pContact->dP.zero();
		}
	}
	else
	{
		bRemoveContact = 1;
	}

	if (bRemoveContact)
	{
		DetachContact(pContact);
		m_pWorld->FreeContact(pContact);
	}

	return bRemoveContact;
}

int CRigidEntity::RegisterConstraint(const Vec3& pt0, const Vec3& pt1, int ipart0, CPhysicalEntity* pBuddy, int ipart1,
                                     int flags, int flagsInfo)
{
	int i;
	masktype constraint_mask;

	for (i = 0, constraint_mask = 0; i < m_nColliders; constraint_mask |= m_pColliderConstraints[i++])
		;
	for (i = 0; i < NMASKBITS && constraint_mask & getmask(i); i++)
		;
	if (i == NMASKBITS)
	{
		return -1;
	}
	if (!pBuddy)
	{
		pBuddy = &g_StaticPhysicalEntity;
	}

	if (i >= m_nConstraintsAlloc)
	{
		entity_contact* pConstraints = m_pConstraints;
		constraint_info* pInfos = m_pConstraintInfos;
		int nConstraints = m_nConstraintsAlloc;
		memcpy(m_pConstraints = new entity_contact[(m_nConstraintsAlloc = (i & ~7) + 8)], pConstraints,
		       nConstraints * sizeof(entity_contact));
		memcpy(m_pConstraintInfos = new constraint_info[m_nConstraintsAlloc], pInfos,
		       nConstraints * sizeof(constraint_info));
		if (pConstraints)
		{
			delete[] pConstraints;
		}
		if (pInfos)
		{
			delete[] pInfos;
		}
	}
	// michaelg: had to split the access to m_pColliderConstraints, was compiler bug in gcc
	const int buddyCollider = AddCollider(pBuddy);
	m_pColliderConstraints[buddyCollider] |= getmask(i);
	pBuddy->AddCollider(this);

	m_pConstraints[i].pt[0] = pt0;
	m_pConstraints[i].pt[1] = pt1;
	m_pConstraints[i].nloc = m_pConstraints[i].n.Set(0, 0, 1);
	m_pConstraints[i].iNormal = 0;
	m_pConstraints[i].dir.zero();
	m_pConstraints[i].pent[0] = this;
	m_pConstraints[i].pent[1] = pBuddy;
	m_pConstraints[i].ipart[0] = ipart0;
	m_pConstraints[i].ipart[1] = ipart1;
	m_pConstraints[i].pbody[0] = GetRigidBody(ipart0);
	m_pConstraints[i].pbody[1] = pBuddy->GetRigidBody(ipart1, iszero(flagsInfo & constraint_inactive));
	m_pConstraints[i].ptloc[0] = Glob2Loc(m_pConstraints[i], 0);
	m_pConstraints[i].ptloc[1] = Glob2Loc(m_pConstraints[i], 1);

	m_pConstraints[i].vrel.zero();
	m_pConstraints[i].friction = 0;
	m_pConstraints[i].flags = flags;
	m_pConstraints[i].iPrim[0] = -(i + 1);

	m_pConstraints[i].Pspare = 0;
	m_pConstraints[i].K.SetZero();
	GetContactMatrix(m_pConstraints[i].pt[0], m_pConstraints[i].ipart[0], m_pConstraints[i].K);
	pBuddy->GetContactMatrix(m_pConstraints[i].pt[1], m_pConstraints[i].ipart[1], m_pConstraints[i].K);
	m_pConstraints[i].vreq.zero();

	m_pConstraintInfos[i].flags = flagsInfo;
	m_pConstraintInfos[i].damping = 0;
	m_pConstraintInfos[i].pConstraintEnt = 0;
	m_pConstraintInfos[i].bActive = 0;
	m_pConstraintInfos[i].sensorRadius = 0.05f;
	m_pConstraintInfos[i].limit = 0;

	return i;
}

int CRigidEntity::RemoveConstraint(int iConstraint)
{
	int i;
	for (i = 0; i < m_nColliders && !(m_pColliderConstraints[i] & getmask(iConstraint)); i++)
		;
	if (i == m_nColliders)
	{
		return 0;
	}
	if (!((m_pColliderConstraints[i] &= ~getmask(iConstraint)) || m_pColliderContacts[i]) &&
	    !m_pColliders[i]->HasContactsWith(this))
	{
		m_pColliders[i]->RemoveCollider(this);
		RemoveCollider(m_pColliders[i]);
	}
	return 1;
}

void CRigidEntity::VerifyExistingContacts(float maxdist)
{
	int i, bConfirmed, nRemoved = 0;
	geom_world_data gwd;
	Vec3 ptres[2], n, ptclosest[2];
	entity_contact *pContact, *pContactNext;

	for (i = 0; i < m_nColliders; i++)
	{
		pContact = m_pColliderContacts[i];
		if (pContact)
		{
			for (;; pContact = pContact->next)
			{
				if ((pContact->flags & contact_new) == 0 &&
				    IsAwake(pContact->ipart[0]) + m_pColliders[i]->IsAwake(pContact->ipart[1]) > 0)
				{
					bConfirmed = 0;
					if (!bConfirmed)
					{
						pContact->flags |= contact_remove;
						nRemoved++;
					}
				}
				if ((pContact->flags &= ~(contact_new | contact_2b_verified | contact_archived)) &
				    contact_last)
				{
					break;
				}
			}
		}
	}

	if (nRemoved)
	{
		for (pContact = m_pContactStart; pContact != CONTACT_END(m_pContactStart); pContact = pContactNext)
		{
			pContactNext = pContact->next;
			if (pContact->flags & contact_remove)
			{
				DetachContact(pContact);
				m_pWorld->FreeContact(pContact);
			}
		}
	}
}

void CRigidEntity::OnContactResolved(entity_contact* pContact, int iop, int iGroupId)
{
	int i, j;
	if (iop == 0 && pContact->iPrim[0] < 0 && m_pConstraintInfos[j = -pContact->iPrim[0] - 1].limit > 0 &&
	    pContact->Pspare > m_pConstraintInfos[j].limit * max(0.01f, m_lastTimeStep))
	{
		masktype constraint_mask = 0;
		for (i = 0; i < m_nColliders; constraint_mask |= m_pColliderConstraints[i++])
			;
		for (i = 0; i < NMASKBITS && getmask(i) <= constraint_mask; i++)
		{
			if (constraint_mask & getmask(i) && m_pConstraintInfos[i].id == m_pConstraintInfos[j].id)
			{
				(m_pConstraintInfos[i].flags |= constraint_inactive | constraint_broken) &=
				    ~constraint_ignore_buddy;

				if (!(m_pConstraintInfos[i].flags & constraint_rope))
				{
					EventPhysJointBroken epjb;
					epjb.idJoint = m_pConstraintInfos[i].id;
					epjb.bJoint = 0;
					epjb.pNewEntity[0] = epjb.pNewEntity[1] = 0;
					for (int j = 0; j < 2; j++)
					{
						epjb.pEntity[j] = m_pConstraints[i].pent[j];
						epjb.pForeignData[j] = m_pConstraints[i].pent[j]->m_pForeignData;
						epjb.iForeignData[j] = m_pConstraints[i].pent[j]->m_iForeignData;
						epjb.partid[j] =
						    m_pConstraints[i].pent[j]->m_parts[m_pConstraints[i].ipart[j]].id;
					}
					epjb.pt = m_pConstraints[i].pt[0];
					epjb.n = m_pConstraints[i].n;
					epjb.partmat[0] = epjb.partmat[1] = -1;
					m_pWorld->OnEvent(2, &epjb);
				}
			}
		}
		BreakableConstraintsUpdated();
	}
	CPhysicalEntity::OnContactResolved(pContact, iop, iGroupId);
}

void CRigidEntity::BreakableConstraintsUpdated()
{
	int i;
	masktype constraint_mask = 0;
	for (i = 0; i < m_nColliders; constraint_mask |= m_pColliderConstraints[i++])
		;
	for (i = 0; i < m_nParts; i++)
	{
		if (!m_parts[i].pLattice && !m_pStructure)
		{
			m_parts[i].flags &= ~geom_monitor_contacts;
		}
	}
	for (i = 0; i < NMASKBITS && getmask(i) <= constraint_mask; i++)
	{
		if (constraint_mask & getmask(i) && m_pConstraintInfos[i].limit > 0)
		{
			m_parts[m_pConstraints[i].ipart[0]].flags |= geom_monitor_contacts;
		}
	}
}

void CRigidEntity::UpdateConstraints(float time_interval)
{
	int i;
	masktype constraint_mask = 0;
	Ang3 angles;
	Vec3 drift, dw, dL;
	quaternionf qframe0, qframe1;
	for (i = 0; i < m_nColliders; constraint_mask |= m_pColliderConstraints[i++])
		;

	for (i = 0; i < NMASKBITS && getmask(i) <= constraint_mask; i++)
	{
		if (constraint_mask & getmask(i))
		{
			if (!(m_pConstraintInfos[i].flags & constraint_inactive))
			{
				m_pConstraints[i].pt[0] = Loc2Glob(m_pConstraints[i], 0);
				m_pConstraints[i].pt[1] = Loc2Glob(m_pConstraints[i], 1);
				m_pConstraintInfos[i].qprev[0] = m_pConstraints[i].pbody[0]->q;
				m_pConstraintInfos[i].qprev[1] = m_pConstraints[i].pbody[1]->q;

				drift = m_pConstraints[i].pt[1] - m_pConstraints[i].pt[0];
				m_pConstraintInfos[i].bActive = 1;

				if (m_pConstraintInfos[i].flags & constraint_rope)
				{
					pe_params_rope pr;
					m_pConstraintInfos[i].pConstraintEnt->GetParams(&pr);
					if (drift.len2() > sqr(pr.length))
					{
						m_pConstraints[i].n = drift.normalized();
						if (drift.len2() > sqr(pr.length * 1.005f))
						{
							m_pConstraints[i].vreq =
							    (drift - m_pConstraints[i].n * (pr.length * 1.005f)) * 5.0f;
						}
						else
						{
							m_pConstraints[i].vreq.zero();
						}
					}
					else
					{
						m_pConstraintInfos[i].bActive = 0;
					}
					pe_simulation_params sp;
					m_pConstraintInfos[i].pConstraintEnt->GetParams(&sp);
					m_dampingEx = max(m_dampingEx, sp.damping);
					m_pConstraintInfos[i].pConstraintEnt->Awake();
				}
				else if (m_pConstraints[i].flags & contact_constraint_3dof)
				{
					m_pConstraints[i].n = drift.normalized();
					m_pConstraints[i].vreq = drift * m_pWorld->m_vars.unprojVelScale;
				}
				else if (m_pConstraints[i].flags & contact_angular)
				{
					m_pConstraints[i].n = m_pConstraints[i].pbody[0]->q * m_pConstraints[i].nloc;
					qframe0 = m_pConstraints[i].pbody[0]->q * m_pConstraintInfos[i].qframe_rel[0];
					qframe1 = m_pConstraints[i].pbody[1]->q * m_pConstraintInfos[i].qframe_rel[1];
					dw = m_pConstraints[i].pbody[0]->w - m_pConstraints[i].pbody[1]->w;

					if ((m_pConstraints[i].flags & contact_constraint_2dof) |
					    (m_pConstraintInfos[i].flags & constraint_limited_1axis))
					{
						angles = Ang3::GetAnglesXYZ(Matrix33(!qframe1 * qframe0));
						if (!(m_pConstraintInfos[i].flags &
						      (constraint_limited_1axis | constraint_limited_2axes)))
						{
							dw -= m_pConstraints[i].n * (m_pConstraints[i].n * dw);
							m_pConstraints[i].vreq =
							    m_pConstraints[i].n * (-angles.x * 10.0f);
						}
						else
						{
							dw = m_pConstraints[i].n * (m_pConstraints[i].n * dw);
							if (angles.x < m_pConstraintInfos[i].limits[0])
							{
								m_pConstraints[i].vreq =
								    m_pConstraints[i].n *
								    min(5.0f,
								        (m_pConstraintInfos[i].limits[0] - angles.x) *
								            10.0f);
							}
							else if (angles.x > m_pConstraintInfos[i].limits[1])
							{
								m_pConstraints[i].n.Flip();
								m_pConstraints[i].vreq =
								    m_pConstraints[i].n *
								    min(5.0f,
								        (angles.x - m_pConstraintInfos[i].limits[1]) *
								            10.0f);
							}
							else
							{
								m_pConstraintInfos[i].bActive = 0;
							}
						}
					}
					else if ((m_pConstraints[i].flags & contact_constraint_1dof) |
					         (m_pConstraintInfos[i].flags & constraint_limited_2axes))
					{
						drift = m_pConstraints[i].n ^ qframe1 * Vec3(1, 0, 0);
						if (!(m_pConstraintInfos[i].flags &
						      (constraint_limited_1axis | constraint_limited_2axes)))
						{
							if (drift.len2() > 1.0f)
							{
								drift.normalize();
							}
							m_pConstraints[i].vreq = drift * 10.0f;
						}
						else if (drift.len2() > sqr(m_pConstraintInfos[i].limits[1]))
						{
							m_pConstraints[i].vreq =
							    (m_pConstraints[i].n = drift.normalized()) *
							    min(10.0f, drift.len() - m_pConstraintInfos[i].limits[1]);
						}
						else
						{
							m_pConstraintInfos[i].bActive = 0;
						}
					}
				}

				m_pConstraints[i].K.SetZero();
				if (!(m_pConstraints[i].flags & contact_angular))
				{
					m_pConstraints[i].pent[0]->GetContactMatrix(
					    m_pConstraints[i].pt[0], m_pConstraints[i].ipart[0], m_pConstraints[i].K);
					m_pConstraints[i].pent[1]->GetContactMatrix(
					    m_pConstraints[i].pt[1], m_pConstraints[i].ipart[1], m_pConstraints[i].K);
				}
				else
				{
					m_pConstraints[i].K += m_pConstraints[i].pbody[0]->Iinv;
					m_pConstraints[i].K += m_pConstraints[i].pbody[1]->Iinv;
					if (m_pConstraintInfos[i].bActive && m_pConstraintInfos[i].damping > 0)
					{
						dL = m_pConstraints[i].K.GetInverted() * dw *
						     -min(0.9f, time_interval * m_pConstraintInfos[i].damping);
						m_pConstraints[i].pbody[0]->L += dL;
						m_pConstraints[i].pbody[1]->L -= dL;
						m_pConstraints[i].pbody[0]->w =
						    m_pConstraints[i].pbody[0]->Iinv * m_pConstraints[i].pbody[0]->L;
						m_pConstraints[i].pbody[1]->w =
						    m_pConstraints[i].pbody[1]->Iinv * m_pConstraints[i].pbody[1]->L;
					}
				}
			}
			else
			{
				m_pConstraintInfos[i].bActive = 0;
			}
		}
	}
}

int CRigidEntity::EnforceConstraints(float time_interval)
{
	int i, i1, bEnforced = 0;
	Vec3 pt[2], v1;
	Ang3 ang;
	quaternionf qframe0, qframe1;
	masktype constraintMask = 0;
	for (i = 0; i < m_nColliders; constraintMask |= m_pColliderConstraints[i++])
		;

	for (i = 0; i < NMASKBITS && getmask(i) <= constraintMask; i++)
	{
		if (constraintMask & getmask(i))
		{
			if (!(m_pConstraintInfos[i].flags & (constraint_inactive | constraint_rope)) &&
			    m_pConstraints[i].pbody[0]->Minv > m_pConstraints[i].pbody[1]->Minv * 10)
			{
				pt[0] = Loc2Glob(m_pConstraints[i], 0);
				pt[1] = Loc2Glob(m_pConstraints[i], 1);
				v1 = m_pConstraints[i].pbody[1]->v +
				     (m_pConstraints[i].pbody[1]->w ^ pt[1] - m_pConstraints[i].pbody[1]->pos);
				if (v1.len() * time_interval < m_pWorld->m_vars.maxContactGap * 15)
				{
					continue;
				}

				m_posNew += pt[1] - pt[0];
				qframe0 = m_pConstraints[i].pbody[0]->q * m_pConstraintInfos[i].qframe_rel[0];
				qframe1 = m_pConstraints[i].pbody[1]->q * m_pConstraintInfos[i].qframe_rel[1];
				ang = Ang3::GetAnglesXYZ(Matrix33(!qframe1 * qframe0));

				i1 = i + 1;
				if (i1 < NMASKBITS && constraintMask & getmask(i1) &&
				    m_pConstraintInfos[i + 1].id == m_pConstraintInfos[i].id &&
				    m_pConstraints[i1].flags & (contact_constraint_2dof | contact_constraint_1dof))
				{
					if (m_pConstraints[i1++].flags & contact_constraint_2dof)
					{
						ang.y = ang.z = 0;
					}
				}
				if (i1 < NMASKBITS && constraintMask & getmask(i1) &&
				    m_pConstraintInfos[i + 1].id == m_pConstraintInfos[i].id)
				{
					if (m_pConstraintInfos[i1++].flags & constraint_limited_1axis)
					{
						ang.x = max(m_pConstraintInfos[i1 - 1].limits[0],
						            min(m_pConstraintInfos[i1 - 1].limits[1], ang.x));
					}
				}
				m_qNew = qframe1 * quaternionf(ang) * !qframe0 * m_qNew;
				i = i1 - 1;
				bEnforced++;
			}
		}
	}

	return bEnforced;
}

float CRigidEntity::CalcEnergy(float time_interval)
{
	Vec3 v;
	float Emax = m_body.M * sqr(m_pWorld->m_vars.maxVel) * 2, Econstr = 0;
	int i;
	masktype constraint_mask;
	entity_contact* pContact;

	if (time_interval > 0)
	{
		if (m_flags & ref_use_simple_solver & -m_pWorld->m_vars.bLimitSimpleSolverEnergy)
		{
			return m_E0;
		}

		for (i = 0, constraint_mask = 0; i < m_nColliders; i++)
		{
			constraint_mask |= m_pColliderConstraints[i];
		}

		if (!(m_flags & ref_use_simple_solver))
		{
			for (pContact = m_pContactStart; pContact != CONTACT_END(m_pContactStart);
			     pContact = pContact->next)
			{
				v += pContact->n * max(0.0f, max(0.0f, pContact->vreq * pContact->n -
				                                           max(0.0f, pContact->vrel * pContact->n)) -
				                                 pContact->n * v);
				if (pContact->pbody[1]->Minv == 0 && pContact->pent[1]->m_iSimClass > 1)
				{
					RigidBody* pbody = pContact->pbody[1];
					Emax = m_body.M * sqr(10.0f) * 2;
				}
			}
		}

		for (i = 0; i < NMASKBITS && getmask(i) <= constraint_mask; i++)
		{
			if (constraint_mask & getmask(i) && (m_pConstraints[i].flags & contact_constraint_3dof ||
			                                     m_pConstraintInfos[i].flags & constraint_rope))
			{
				Econstr += max(m_pConstraints[i].pbody[0]->M *
				                   sqr(fabs_tpl(m_pConstraints[i].n * m_pConstraints[i].pbody[0]->v) +
				                       fabs_tpl(m_pConstraints[i].n * m_pConstraints[i].vreq)),
				               m_pConstraints[i].pbody[1]->M *
				                   sqr(fabs_tpl(m_pConstraints[i].n * m_pConstraints[i].pbody[1]->v) +
				                       fabs_tpl(m_pConstraints[i].n * m_pConstraints[i].vreq)));
			}
		}
		return min_safe((m_body.M * max((m_body.v + v).len2(), m_body.v.len2() + v.len2())) +
		                    m_body.L * m_body.w + Econstr,
		                Emax);
	}

	return (m_body.M * max((m_body.v + v).len2(), m_body.v.len2() + v.len2())) + m_body.L * m_body.w;
}

void CRigidEntity::ComputeBBoxRE(coord_block_BBox* partCoord)
{
	for (int i = 0; i < min(2, min(m_nParts, m_iVarPart0)); i++)
	{
		partCoord[i].pos = m_parts[i].pNewCoords->pos;
		partCoord[i].q = m_parts[i].pNewCoords->q;
		partCoord[i].scale = m_parts[i].pNewCoords->scale;
		m_parts[i].pNewCoords = &partCoord[i];
	}
	ComputeBBox(m_BBoxNew, update_part_bboxes & (min(m_nParts, m_iVarPart0) - 3) >> 31);
}

void CRigidEntity::UpdatePosition(int bGridLocked)
{
	int i;
	WriteLock lock(m_lockUpdate);

	m_pos = m_pNewCoords->pos;
	m_qrot = m_pNewCoords->q;
	for (i = 0; i < m_nParts; i++)
	{
		m_parts[i].pos = m_parts[i].pNewCoords->pos;
		m_parts[i].q = m_parts[i].pNewCoords->q;
	}

	if (min(m_nParts, m_iVarPart0) <= 2)
	{
		for (i = 0; i < m_nParts; i++)
		{
			m_parts[i].BBox[0] = m_parts[i].pNewCoords->BBox[0];
			m_parts[i].BBox[1] = m_parts[i].pNewCoords->BBox[1];
		}
		for (i = 0; i < min(2, min(m_nParts, m_iVarPart0)); i++)
		{
			m_parts[i].pNewCoords = (coord_block_BBox*)&m_parts[i].pos;
		}
		m_BBox[0] = m_BBoxNew[0];
		m_BBox[1] = m_BBoxNew[1];
	}
	else
	{
		for (i = 0; i < min(m_nParts, m_iVarPart0); i++)
		{
			m_parts[i].pNewCoords = (coord_block_BBox*)&m_parts[i].pos;
		}
		ComputeBBox(m_BBox);
		for (i = 0; i < m_nParts; i++)
		{
			m_parts[i].BBox[0] = m_parts[i].pNewCoords->BBox[0];
			m_parts[i].BBox[1] = m_parts[i].pNewCoords->BBox[1];
		}
	}
	AtomicAdd(&m_pWorld->m_lockGrid, -bGridLocked);
}

void CRigidEntity::CapBodyVel()
{
	if (m_body.v.len2() > sqr(m_pWorld->m_vars.maxVel))
	{
		m_body.v.normalize() *= m_pWorld->m_vars.maxVel;
		m_body.P = m_body.v * m_body.M;
	}
	if (m_body.w.len2() > sqr(m_maxw))
	{
		m_body.w.normalize() *= m_maxw;
		m_body.L = m_body.q * (m_body.Ibody * (!m_body.q * m_body.w));
	}
}

void CRigidEntity::StartStep(float time_interval)
{
	m_timeStepPerformed = 0;
	m_timeStepFull = time_interval;
}

int CRigidEntity::Step(float time_interval)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	geom_contact* pcontacts;
	int i, j, itmax, bRecheckPenetrations, ncontacts, nAwakeColliders, bHasContacts, bNoForce,
	    bSeverePenetration = 0, bNoUnproj = 0, bWasUnproj = 0, bCanopyContact;
	int bUseSimpleSolver = isneg(-((int)m_flags & ref_use_simple_solver)),
	    bUseDistanceContacts = m_pWorld->m_vars.bUseDistanceContacts & (bUseSimpleSolver ^ 1);
	real tmax, e;
	float r;
	quaternionf qrot;
	Vec3 axis, pos;
	geom_world_data gwd;
	intersection_params ip;
	coord_block_BBox partCoordTmp[2];
	entity_contact* pContact;

	for (i = m_nColliders - 1, nAwakeColliders = 0, bHasContacts = isneg(-m_nContacts); i >= 0; i--)
	{
		if (m_pColliders[i]->m_iSimClass == 7)
		{
			RemoveCollider(m_pColliders[i]);
		}
		else
		{
			bHasContacts |= m_pColliders[i]->HasCollisionContactsWith(this);
			nAwakeColliders += isneg(-m_pColliders[i]->m_iSimClass);
		}
	}
	m_prevPos = m_body.pos;
	m_prevq = m_body.q;
	m_prevv = m_body.v;
	m_prevw = m_body.w;
	m_vSleep.zero();
	m_wSleep.zero();
	m_pStableContact = 0;

	m_dampingEx = 0;
	if (m_timeStepPerformed > m_timeStepFull - 0.001f || m_nParts == 0)
	{
		return 1;
	}
	m_timeStepPerformed += time_interval;
	m_lastTimeStep = time_interval;
	m_impulseTime = max(0.0f, m_impulseTime - time_interval);
	m_minFriction = 0.1f;
	bNoForce = iszero(m_body.Fcollision.len2());
	bNoUnproj = isneg(4 - (m_nFutileUnprojFrames * bNoForce));
	m_flags &= ~ref_small_and_fast;

	// if not sleeping, check for new contacts with all surrounding entities
	if (m_bAwake | bUseSimpleSolver)
	{
		m_body.P += m_Pext[0];
		m_Pext[0].zero();
		m_body.L += m_Lext[0];
		m_Lext[0].zero();
		m_Psoft.zero();
		m_Lsoft.zero();
		m_body.Step(time_interval);
		CapBodyVel();
		m_qNew = m_body.q * m_body.qfb;
		m_posNew = m_body.pos - m_qNew * m_body.offsfb;

		ip.maxSurfaceGapAngle = 1.0f * (g_PI / 180);
		ip.axisContactNormal = -m_gravity.normalized();
		// ip.bNoAreaContacts = !m_pWorld->m_vars.bUseDistanceContacts;
		gwd.v = m_body.v;
		gwd.w = m_body.w;
		gwd.centerOfMass = m_body.pos;

		if (m_bCollisionCulling || m_nParts == 1 && m_parts[0].pPhysGeomProxy->pGeom->IsConvex(0.02f))
		{
			ip.ptOutsidePivot[0] = (m_BBox[0] + m_BBox[1]) * 0.5f;
		}
		Vec3 sz = m_BBox[1] - m_BBox[0];
		ip.maxUnproj = max(max(sz.x, sz.y), sz.z);
		e = m_pWorld->m_vars.maxContactGap * (0.5f - 0.4f * bUseSimpleSolver);
		ip.iUnprojectionMode = 0;
		ip.bNoIntersection = bUseSimpleSolver;
		m_qNew.Normalize();

		if (!EnforceConstraints(time_interval) &&
		    m_velFastDir * (time_interval - bNoUnproj) > m_sizeFastDir * 0.71f)
		{
			pos = m_posNew;
			m_posNew = m_pos;
			qrot = m_qNew;
			m_qNew = m_qrot;
			ComputeBBox(m_BBoxNew, 0);
			ip.bSweepTest = true;
			ip.time_interval = time_interval;
			m_flags |= ref_small_and_fast & -isneg(ip.maxUnproj - 1.0f);
			ncontacts = CheckForNewContacts(&gwd, &ip, itmax, gwd.v * time_interval);
			{
				WriteLockCond lock(*ip.plock, 0);
				lock.SetActive(isneg(-ncontacts));
				pcontacts = ip.pGlobalContacts;
				if (itmax >= 0 && !m_pWorld->GetPhysVars()->bProhibitUnprojection)
				{
					m_posNew -= pcontacts[itmax].dir * (pcontacts[itmax].t + e);
					m_body.pos = m_posNew + m_qNew * m_body.offsfb;
					m_posNew = m_body.pos - qrot * m_body.offsfb;
					bWasUnproj = 1;
					if (m_nColliders)
					{
						m_minFriction = 3.0f;
					}
				}
				else
				{
					m_posNew = pos;
				}
			}
			ray_hit hit;
			for (i = 0; i < m_nColliders; i++)
			{
				if (m_pColliders[i]->m_iSimClass <= 2 && m_pColliders[i] != this &&
				    (m_pColliders[i]->m_iGroup != m_iGroup || m_pColliders[i]->m_bMoved) &&
				    m_pWorld->RayTraceEntity(m_pColliders[i], m_prevPos, m_posNew - m_pos, &hit) &&
				    !m_pWorld->GetPhysVars()->bProhibitUnprojection)
				{
					m_body.v *= 0.5f;
					m_body.w *= 0.5f;
					m_body.P *= 0.5f;
					m_body.L *= 0.5f;
					m_posNew = m_pos;
					m_body.pos = m_prevPos;
					break;
				}
			}
			m_qNew = qrot;
			ip.bSweepTest = false;
			ip.time_interval = time_interval * 3;
		}
		else
		{
			ip.time_interval = time_interval;
		}

		ComputeBBox(m_BBoxNew, 0);
		CheckAdditionalGeometry(time_interval);

		ip.vrel_min = ip.maxUnproj / ip.time_interval * (bHasContacts ? 0.2f : 0.05f);
		ip.vrel_min += 1E10f * (bUseSimpleSolver | bWasUnproj);
		ncontacts = CheckForNewContacts(&gwd, &ip, itmax);
		pcontacts = ip.pGlobalContacts;
		WriteLockCond lock(*ip.plock, 0);
		lock.SetActive(isneg(-ncontacts));

		if (ncontacts <= 8)
		{
			// 141833	2007/09/21 !O another tweak for small-and-fast rigid bodies
			if (ncontacts == 0 && bWasUnproj && !m_pWorld->GetPhysVars()->bProhibitUnprojection)
			{
				m_body.v *= 0.1f;
				m_body.w *= 0.1f;
				m_body.P *= 0.1f;
				m_body.L *= 0.1f;
			}
			for (i = 0; i < ncontacts - 1; i++)
			{
				for (j = i + 1; j < ncontacts; j++)
				{
					if ((g_CurColliders[i] == g_CurColliders[j] &&
					         g_CurCollParts[i][1] == g_CurCollParts[j][1] ||
					     g_CurColliders[i]->m_iSimClass + g_CurColliders[j]->m_iSimClass == 0) &&
					    max(pcontacts[i].vel, 0.0f) + max(pcontacts[j].vel, 0.0f) <= 0 &&
					    pcontacts[i].dir * pcontacts[j].dir < -0.85f)
					{
						if (g_CurColliders[i]->m_iSimClass)
						{
							pcontacts[(pcontacts[i].center - m_body.pos) *
							                      pcontacts[i].dir >
							                  0
							              ? i
							              : j]
							    .t = -1;
						}
						else
						{
							Vec3 thinDim;
							if (m_nParts == 1)
							{
								primitives::box bbox;
								m_parts[0].pPhysGeomProxy->pGeom->GetBBox(&bbox);
								int idim = idxmin3((float*)bbox.size);
								if (bbox.size[idim] < max(bbox.size[inc_mod3[idim]],
								                          bbox.size[dec_mod3[idim]]) *
								                          0.1f)
								{
									thinDim = m_qrot * m_parts[0].q *
									          bbox.Basis.GetRow(idim);
								}
							}
							if ((fabs_tpl(pcontacts[i].n * thinDim) > 0.94f ||
							     (pcontacts[i].center - pcontacts[j].center) *
							                 pcontacts[i].n <
							             0 && // **||**, but not |****|
							         (pcontacts[j].center - pcontacts[i].center) *
							                 pcontacts[j].n <
							             0))
							{
								Vec3 axisx, axisy,
								    dci =
									pcontacts[i].center -
									m_body
									    .pos; //,
								                  // dcj=pcontacts[j].center-m_body.pos;
								Vec2 pt2d, BBoxi[2], BBoxj[2], ci, cj, szi, szj;
								int ipt;
								axisx = pcontacts[i].dir.GetOrthogonal().normalized();
								axisy = pcontacts[i].dir ^ axisx;

								BBoxi[0] = BBoxi[1].set(
								    (pcontacts[i].ptborder[0] - pcontacts[i].center) *
									axisx,
								    (pcontacts[i].ptborder[0] - pcontacts[i].center) *
									axisy);
								for (ipt = 1; ipt < pcontacts[i].nborderpt; ipt++)
								{
									pt2d.set((pcontacts[i].ptborder[ipt] -
									          pcontacts[i].center) *
									             axisx,
									         (pcontacts[i].ptborder[ipt] -
									          pcontacts[i].center) *
									             axisy);
									BBoxi[0] = min(BBoxi[0], pt2d);
									BBoxi[1] = max(BBoxi[1], pt2d);
								}

								BBoxj[0] = BBoxj[1].set(
								    (pcontacts[j].ptborder[0] - pcontacts[i].center) *
									axisx,
								    (pcontacts[j].ptborder[0] - pcontacts[i].center) *
									axisy);
								for (ipt = 1; ipt < pcontacts[j].nborderpt; ipt++)
								{
									pt2d.set((pcontacts[j].ptborder[ipt] -
									          pcontacts[i].center) *
									             axisx,
									         (pcontacts[j].ptborder[ipt] -
									          pcontacts[i].center) *
									             axisy);
									BBoxj[0] = min(BBoxj[0], pt2d);
									BBoxj[1] = max(BBoxj[1], pt2d);
								}

								ci = (BBoxi[1] + BBoxi[0]) * 0.5f;
								szi = (BBoxi[1] - BBoxi[0]) * 0.5f;
								cj = (BBoxj[1] + BBoxj[0]) * 0.5f;
								szj = (BBoxj[1] - BBoxj[0]) * 0.5f;
								if (max(fabs_tpl(ci.x - cj.x) - szi.x - szj.x,
								        fabs_tpl(ci.y - cj.y) - szi.y - szj.y) < 0)
								{
									pcontacts[dci * pcontacts[i].dir > 0 ? i : j]
									    .t = -1;
								}
								//	pcontacts[szi.x*szi.y<szj.x*szj.y ? i:j].t = -1;
								// if
								// (sqr_signed(dci*dcj)>sqr(0.5f)*dci.len2()*dcj.len2())
								//	pcontacts[dci*pcontacts[i].dir>0 ? i:j].t = -1;
							}
						}
					}
				}
			}
		}

		tmax = itmax >= 0 ? pcontacts[itmax].t : 0;
		i = itmax; // if (ncontacts==1) i = 0;
		bRecheckPenetrations = 0;
		if (i >= 0)
		{ // first unproject tmax contact to exact collision time (to get ptloc fields in entity_contact filled
		  // correctly)
			if (pcontacts[i].iUnprojMode == 0) // check if unprojection conflicts with existing contacts
			{
				for (pContact = m_pContactStart; pContact != CONTACT_END(m_pContactStart) &&
				                                 pContact->n * pcontacts[i].dir > -0.001f;
				     pContact = pContact->next)
					;
			}
			else
			{
				for (pContact = m_pContactStart; pContact != CONTACT_END(m_pContactStart);
				     pContact = pContact->next)
				{
					axis = pcontacts[i].dir ^ pContact->pt[0] - ip.centerOfRotation;
					r = axis.len2();
					if (r > sqr(ip.minAxisDist) && sqr_signed(axis * pContact->n) < -0.001f * r)
					{
						break;
					}
				}
			}
			if (pContact != CONTACT_END(m_pContactStart))
			{
				e = 0;
				bRecheckPenetrations = 1;
			}
			else if (pcontacts[i].iUnprojMode == 0 &&
			         (!bHasContacts || pcontacts[i].t > ip.maxUnproj * 0.1f) &&
			         pcontacts[i].t < pcontacts[i].vel * time_interval * 1.1f &&
			         !m_pWorld->GetPhysVars()->bProhibitUnprojection)
			{
				m_posNew += pcontacts[i].dir * pcontacts[i].t;
				m_body.pos += pcontacts[i].dir * pcontacts[i].t;
				bWasUnproj = 1;
			}
			else
			{
				e = 0;
				bRecheckPenetrations = 1;
			}
			if (pcontacts[i].dir * m_prevUnprojDir < -0.5f)
			{
				m_bProhibitUnproj += 2;
			}
			m_prevUnprojDir = pcontacts[i].dir;
		}

		if (bUseSimpleSolver)
		{
			m_body.P += m_Pext[1];
			m_Pext[1].zero();
			m_body.L += m_Lext[1];
			m_Lext[1].zero();
			m_Estep = (m_body.v.len2() + (m_body.L * m_body.w) * m_body.Minv) * 0.5f;
			m_body.P += m_gravity * (m_body.M * time_interval);
			m_body.v = m_body.P * m_body.Minv;
			m_body.w = m_body.Iinv * m_body.L;
			m_E0 = (m_body.M * m_body.v.len2()) + m_body.L * m_body.w;
			UpdatePenaltyContacts(time_interval);
		}

		if (!bRecheckPenetrations)
		{
			if (bUseDistanceContacts)
			{
				for (i = 0; i < ncontacts; i++)
				{
					if (pcontacts[i].vel > 0 && pcontacts[i].t > tmax - e)
					{
						if (pcontacts[i].parea && pcontacts[i].parea->npt > 1)
						{
							for (j = 0; j < pcontacts[i].parea->npt; j++)
							{
								RegisterContactPoint(
								    i, pcontacts[i].parea->pt[j], pcontacts,
								    pcontacts[i].parea->piPrim[0][j],
								    pcontacts[i].parea->piFeature[0][j],
								    pcontacts[i].parea->piPrim[1][j],
								    pcontacts[i].parea->piFeature[1][j],
								    contact_inexact);
							}
						}
						else
						{
							RegisterContactPoint(
							    i, pcontacts[i].pt, pcontacts, pcontacts[i].iPrim[0],
							    pcontacts[i].iFeature[0], pcontacts[i].iPrim[1],
							    pcontacts[i].iFeature[1]);
						}
					}
					else
					{
						bRecheckPenetrations |= pcontacts[i].vel <= 0 ? 1 : 0;
					}
				}
			}

			// if we don't generate distance contacts, leave some penetration during unprojection instead of
			// a gap
			e *= (bUseDistanceContacts * 2) - 1;
			if (itmax >= 0)
			{
				if (pcontacts[itmax].iUnprojMode == 0)
				{
					m_posNew += pcontacts[itmax].dir * e;
					m_body.pos += pcontacts[itmax].dir * e;
				}
				else
				{
					qrot.SetRotationAA(e, pcontacts[itmax].dir);
					m_qNew = qrot * m_qNew;
					m_posNew = ip.centerOfRotation + qrot * (m_posNew - ip.centerOfRotation);
					m_body.pos = m_posNew + m_qNew * m_body.offsfb;
					m_body.q = m_qNew * !m_body.qfb;
					m_body.UpdateState();
				}
			}
		}
		ComputeBBoxRE(partCoordTmp);
		bRecheckPenetrations |= bUseDistanceContacts ^ 1;

		if (bRecheckPenetrations)
		{
			if (itmax >= 0)
			{
				ip.iUnprojectionMode = 0;
				ip.vrel_min = 1E10;
				ip.maxSurfaceGapAngle = 0;
				// ip.bThreadSafe = 1;
				lock.Release();
				ncontacts = CheckForNewContacts(&gwd, &ip, itmax);
				lock.SetActive(isneg(-ncontacts));
			}
			bCanopyContact = m_bCanopyContact;
			m_bCanopyContact = 0;
			for (i = 0; i < ncontacts; i++)
			{
				if (pcontacts[i].t >= 0)
				{ // penetration contacts - register points and add additional penalty impulse in solver
					if (!(m_parts[g_CurCollParts[i][0]].flags & geom_squashy))
					{
						Vec3 ntilt;
						axis.zero();
						r = 0;
						if (pcontacts[i].parea && pcontacts[i].parea->npt > 2)
						{
							for (j = 0; (j < pcontacts[i].parea->npt) & -bUseSimpleSolver;
							     j++)
							{
								RegisterContactPoint(
								    i, pcontacts[i].parea->pt[j], pcontacts,
								    pcontacts[i].parea->piPrim[0][j],
								    pcontacts[i].parea->piFeature[0][j],
								    pcontacts[i].parea->piPrim[1][j],
								    pcontacts[i].parea->piFeature[1][j],
								    contact_2b_verified | contact_new |
									contact_inexact);
							}
							if (pcontacts[i].parea->type == geom_contact_area::polygon)
							{
								if (pcontacts[i].parea->n1 * pcontacts[i].n < -0.999f &&
								    fabs_tpl(pcontacts[i].parea->n1 * m_gravity) >
								        fabs_tpl(pcontacts[i].n * m_gravity))
								{
									ntilt = pcontacts[i].n;
									pcontacts[i].n = -pcontacts[i].parea->n1;
									pcontacts[i].parea->n1 = -ntilt;
								}
								axis =
								    (ntilt = pcontacts[i].parea->n1 ^ pcontacts[i].n) ^
								    pcontacts[i].n;
								for (j = 1, r = pcontacts[i].ptborder[0] * axis;
								     j < pcontacts[i].nborderpt; j++)
								{
									r = max(r, pcontacts[i].ptborder[j] * axis);
								}
							}
						}
						else if (bUseSimpleSolver)
						{
							RegisterContactPoint(
							    i, pcontacts[i].pt, pcontacts, pcontacts[i].iPrim[0],
							    pcontacts[i].iFeature[0], pcontacts[i].iPrim[1],
							    pcontacts[i].iFeature[1],
							    contact_2b_verified | contact_new);
						}
						if (!bUseSimpleSolver)
						{
							pContact = RegisterContactPoint(
							    i, pcontacts[i].center, pcontacts, pcontacts[i].iPrim[0],
							    pcontacts[i].iFeature[0], pcontacts[i].iPrim[1],
							    pcontacts[i].iFeature[1], contact_new | contact_last,
							    max(0.001f, (float)pcontacts[i].t +
							                    axis * pcontacts[i].center - r));
							if (pContact)
							{
								pContact->nloc = ntilt;
							}
							for (j = 0; j < pcontacts[i].nborderpt; j++)
							{
								pContact = RegisterContactPoint(
								    i, pcontacts[i].ptborder[j], pcontacts,
								    pcontacts[i].iPrim[0], pcontacts[i].iFeature[0],
								    pcontacts[i].iPrim[1], pcontacts[i].iFeature[1],
								    contact_new,
								    max(0.001f, (float)pcontacts[i].t +
								                    axis * pcontacts[i].ptborder[j] -
								                    r));
								if (pContact)
								{
									pContact->nloc = ntilt;
								}
							}
						}
						bSeverePenetration |=
						    isneg(min(0.4f, ip.maxUnproj * 0.4f) - pcontacts[i].t);
					}
					else
					{
						primitives::box bbox;
						Vec3 dP, center, dv;
						float ks, kd, Mpt, g = m_pWorld->m_vars.gravity.len();

						m_parts[j = g_CurCollParts[i][0]].pPhysGeom->pGeom->GetBBox(&bbox);
						center = m_qNew * (m_parts[j].q * m_parts[j].pPhysGeom->origin *
						                       m_parts[j].scale +
						                   m_parts[j].pos) +
						         m_posNew - m_body.pos;
						Mpt = 1 / (m_body.Minv +
						           (m_body.Iinv * (center ^ pcontacts[i].dir) ^ center) *
						               pcontacts[i].dir);

						ks = Mpt * m_parts[g_CurCollParts[i][0]].minContactDist * g /
						     ((bbox.size.x + bbox.size.y + bbox.size.z) * m_parts[j].scale);
						kd = 3.5f;

						dP = pcontacts[i].dir * (ks * pcontacts[i].t);
						m_Psoft += dP;
						m_Lsoft += center ^ dP;
						dv = m_body.v + (m_body.w ^ center);
						if (m_timeCanopyFallen > 3.0f)
						{
							for (j = 0; j < pcontacts[i].nborderpt; j++)
							{
								pcontacts[i].n = -pcontacts[i].dir;
								pContact = RegisterContactPoint(
								    i, pcontacts[i].ptborder[j], pcontacts,
								    pcontacts[i].iPrim[0], pcontacts[i].iFeature[0],
								    pcontacts[i].iPrim[1], pcontacts[i].iFeature[1],
								    contact_new, 0.001f);
								if (pContact)
								{
									pContact->nloc.zero();
								}
								m_Psoft.zero();
								m_Lsoft.zero();
							}
						}
						else
						{
							dP = dv * (-2.0f * sqrt_tpl(ks * Mpt) * time_interval);
							m_Pext[1] += dP;
							m_Lext[1] += (center ^ dP) - m_body.L * (time_interval * kd);
						}
						m_timeCanopyFallen +=
						    time_interval *
						    sgn(pcontacts[i].t -
						        ((bbox.size.x + bbox.size.y + bbox.size.z) * 0.02f));
						m_timeCanopyFallen =
						    min(5.0f * isneg(-m_nColliders), max(0.0f, m_timeCanopyFallen));
						m_bCanopyContact = 1;

						if (m_flags & pef_log_collisions && m_nMaxEvents > 0 ||
						    m_parts[g_CurCollParts[i][0]].flags & geom_log_interactions)
						{
							EventPhysCollision event;
							event.pEntity[0] = this;
							event.pEntity[1] = g_CurColliders[i];
							event.pForeignData[0] = m_pForeignData;
							event.iForeignData[0] = m_iForeignData;
							event.pForeignData[1] = g_CurColliders[i]->m_pForeignData;
							event.iForeignData[1] = g_CurColliders[i]->m_iForeignData;
							event.pt = pcontacts[i].center;
							event.n = pcontacts[i].n;
							event.idCollider =
							    m_pWorld->GetPhysicalEntityId(event.pEntity[1]);
							event.partid[0] = m_parts[g_CurCollParts[i][0]].id;
							event.partid[1] =
							    g_CurColliders[i]->m_parts[g_CurCollParts[i][1]].id;
							event.idmat[0] = pcontacts[i].id[0];
							event.idmat[1] = pcontacts[i].id[1];
							event.mass[0] = m_body.M;
							event.mass[1] =
							    g_CurColliders[i]->GetMass(g_CurCollParts[i][1]);
							event.vloc[0] =
							    m_body.v + (m_body.w ^ pcontacts[i].center - m_body.pos);
							event.vloc[1].zero();
							event.penetration = pcontacts[i].t;
							event.radius = event.normImpulse = 0;
							m_pWorld->OnEvent(pef_log_collisions, &event);
						}
					}
				}
			}
			m_nCanopyContactsLost += bCanopyContact & (m_bCanopyContact ^ 1);
			m_timeCanopyFallen += 3.0f * iszero((m_nCanopyContactsLost & 3) - 3);
		}
		else
		{
			m_bProhibitUnproj = max(m_bProhibitUnproj - 1, 0);
		}

		CleanupAfterContactsCheck();
	}
	else
	{
		CPhysicalEntity** pentlist;
		if (m_nNonRigidNeighbours)
		{
			GetPotentialColliders(pentlist);
		}
		CheckAdditionalGeometry(time_interval);
		m_BBoxNew[0] = m_BBox[0];
		m_BBoxNew[1] = m_BBox[1];
	}

	if (!bUseSimpleSolver)
	{
		VerifyExistingContacts(m_pWorld->m_vars.maxContactGap);
	}

	UpdatePosition(m_pWorld->RepositionEntity(this, 1, m_BBoxNew));
	pe_params_buoyancy pb[4];
	int nBuoys = PostStepNotify(time_interval, pb, 4);

	Vec3 gravity = m_nColliders ? m_gravity : m_gravityFreefall;
	ApplyBuoyancy(time_interval, gravity, pb, nBuoys);
	if (!bUseSimpleSolver)
	{
		m_body.P += m_Pext[1];
		m_Pext[1].zero();
		m_body.L += m_Lext[1];
		m_Lext[1].zero();
		m_body.P += gravity * (m_body.M * time_interval);
		m_body.P += m_Psoft * time_interval;
		m_body.L += m_Lsoft * time_interval;
		m_body.UpdateState();
	}
	AddAdditionalImpulses(time_interval);
	m_body.Fcollision.zero();
	m_body.Tcollision.zero();
	m_body.UpdateState();
	CapBodyVel();

	bSeverePenetration &= isneg((m_pWorld->m_curGroupMass * 0.001f) - m_body.M);
	if (bSeverePenetration & m_bHadSeverePenetration)
	{
		if ((m_body.v.len2() + m_body.w.len2() * sqr(ip.maxUnproj * 0.5f)) * sqr(time_interval) <
		    sqr(ip.maxUnproj * 0.1f))
		{
			bSeverePenetration = 0; // apparently we cannot resolve the situation by stepping back
		}
		else
		{
			m_body.P.zero();
			m_body.L.zero();
			m_body.v.zero();
			m_body.w.zero();
		}
	}
	m_bHadSeverePenetration = bSeverePenetration;
	m_bSteppedBack = 0;
	m_nFutileUnprojFrames = (m_nFutileUnprojFrames + bWasUnproj) & -bNoForce;

	return (m_bHadSeverePenetration ^ 1) | isneg(3 - m_nStepBackCount); // 1;
}

int CRigidEntity::PostStepNotify(float time_interval, pe_params_buoyancy* pb, int nMaxBuoys)
{
	Vec3 gravity;
	int nBuoys = m_pWorld->CheckAreas(this, gravity, pb, nMaxBuoys, m_body.v);
	if (nBuoys)
	{
		if (!is_unused(gravity))
		{
			if ((m_gravityFreefall - gravity).len2())
			{
				m_minAwakeTime = 0.2f;
			}
			m_gravity = m_gravityFreefall = gravity;
		}
		// SetParams(&pb);
	}

	if (m_flags & (pef_monitor_poststep | pef_log_poststep))
	{
		EventPhysPostStep event;
		event.pEntity = this;
		event.pForeignData = m_pForeignData;
		event.iForeignData = m_iForeignData;
		event.dt = time_interval;
		event.pos = m_posNew;
		event.q = m_qNew;
		event.idStep = m_pWorld->m_idStep;
		m_pWorld->OnEvent(m_flags & pef_monitor_poststep, &event);

		if (m_pWorld->m_iLastLogPump == m_iLastLog && m_pEvent && !m_pWorld->m_vars.bMultithreaded)
		{
			// if (m_flags & pef_monitor_poststep)
			//	m_pWorld->SignalEvent(&event,0);
			WriteLock lock(m_pWorld->m_lockEventsQueue);
			m_pEvent->dt += time_interval;
			m_pEvent->idStep = m_pWorld->m_idStep;
			m_pEvent->pos = m_posNew;
			m_pEvent->q = m_qNew;
			if (m_bProcessed & PENT_SETPOSED)
			{
				AtomicAdd(&m_bProcessed, -PENT_SETPOSED);
			}
		}
		else
		{
			m_pEvent = 0;
			m_pWorld->OnEvent(m_flags & pef_log_poststep, &event, &m_pEvent);
			m_iLastLog = m_pWorld->m_iLastLogPump;
		}
	}
	else
	{
		m_iLastLog = 1;
	}

	return nBuoys;
}

float CRigidEntity::GetMaxTimeStep(float time_interval)
{
	if (m_timeStepPerformed > m_timeStepFull - 0.001f)
	{
		return time_interval;
	}

	primitives::box bbox;
	float bestvol = 0;
	int i, j, iBest = -1;
	unsigned int flagsCollider = 0;
	for (i = 0; i < m_nParts; i++)
	{
		if (m_parts[i].flags & geom_collides)
		{
			m_parts[i].pPhysGeomProxy->pGeom->GetBBox(&bbox);
			if (bbox.size.GetVolume() > bestvol)
			{
				bestvol = bbox.size.GetVolume();
				iBest = i;
			}
			flagsCollider |= m_parts[i].flagsCollider;
		}
	}
	if (iBest == -1)
	{
		return time_interval;
	}
	for (i = j = 0; i < m_nColliders; i++)
	{
		if (m_pColliderContacts[i])
		{
			j |= m_pColliders[i]->m_iSimClass - 1;
		}
	}
	j &= -isneg(m_body.M - (m_pWorld->m_curGroupMass * 0.001f));
	m_parts[iBest].pPhysGeomProxy->pGeom->GetBBox(&bbox);
	Vec3 vloc, vsz, size;
	vloc = bbox.Basis * (m_body.v * (m_qrot * m_parts[iBest].q));
	size = bbox.size * m_parts[iBest].scale;
	vsz(fabsf(vloc.x) * size.y * size.z, fabsf(vloc.y) * size.x * size.z, fabsf(vloc.z) * size.x * size.y);
	i = idxmax3((float*)&vsz);
	m_velFastDir = fabsf(vloc[i]);
	m_sizeFastDir = size[i];

	bool bSkipSpeedCheck = false;
	// if (!m_nColliders || m_nColliders==1 && m_pColliders[0]==this) {
	if (j >= 0)
	{ // if there are no collisions with static objects *or* there are collisions with very heavy rigid objects
		if (m_bCanSweep)
		{
			bSkipSpeedCheck = true;
		}
		else if (min(m_maxAllowedStep, time_interval) * m_velFastDir > m_sizeFastDir * 0.7f)
		{
			int ient, nents;
			Vec3 BBox[2] = {m_BBox[0], m_BBox[1]}, delta = m_body.v * m_maxAllowedStep;
			geom_contact* pcontacts;
			intersection_params ip;
			geom_world_data gwd0, gwd1;
			ip.bStopAtFirstTri = true;
			ip.time_interval = m_maxAllowedStep;

			BBox[isneg(-delta.x)].x += delta.x;
			BBox[isneg(-delta.y)].y += delta.y;
			BBox[isneg(-delta.z)].z += delta.z;
			CPhysicalEntity** pentlist;
			masktype constraint_mask = MaskIgnoredColliders();
			nents = m_pWorld->GetEntitiesAround(BBox[0], BBox[1], pentlist,
			                                    ent_terrain | ent_static | ent_sleeping_rigid | ent_rigid,
			                                    this);
			UnmaskIgnoredColliders(constraint_mask);

			CBoxGeom boxgeom;
			primitives::box abox;
			abox.Basis.SetIdentity();
			abox.center.zero();
			abox.size = (m_BBox[1] - m_BBox[0]) * 0.5f;
			boxgeom.CreateBox(&abox);
			gwd0.offset = (m_BBox[0] + m_BBox[1]) * 0.5f;
			gwd0.R.SetIdentity();
			gwd0.v = m_body.v;

			for (ient = 0; ient < nents; ient++)
			{
				if (pentlist[ient] != this)
				{
					for (j = 0; j < pentlist[ient]->m_nParts; j++)
					{
						if (pentlist[ient]->m_parts[j].flags & flagsCollider)
						{
							gwd1.offset =
							    pentlist[ient]->m_pos +
							    pentlist[ient]->m_qrot * pentlist[ient]->m_parts[j].pos;
							gwd1.R = Matrix33(pentlist[ient]->m_qrot *
							                  pentlist[ient]->m_parts[j].q);
							gwd1.scale = pentlist[ient]->m_parts[j].scale;
							ip.bSweepTest = true;
							if (boxgeom.Intersect(
								pentlist[ient]->m_parts[j].pPhysGeomProxy->pGeom, &gwd0,
								&gwd1, &ip, pcontacts))
							{
								{
									WriteLockCond lockColl(*ip.plock, 0);
									lockColl.SetActive();
								}
								goto hitsmth;
							}
							// note that sweep check can return nothing if bbox is initially
							// intersecting the obstacle
							ip.bSweepTest = false;
							if (boxgeom.Intersect(
								pentlist[ient]->m_parts[j].pPhysGeomProxy->pGeom, &gwd0,
								&gwd1, &ip, pcontacts))
							{
								{
									WriteLockCond lockColl(*ip.plock, 0);
									lockColl.SetActive();
								}
								goto hitsmth;
							}
						}
					}
				}
			}
			bSkipSpeedCheck = true;
		hitsmth:;
		}
	}

	if (!bSkipSpeedCheck && time_interval * fabsf(vloc[i]) > size[i] * 0.7f)
	{
		time_interval = max(0.001f, size[i] * 0.7f / fabsf(vloc[i]));
	}

	int bDegradeQuality = -m_pWorld->m_bCurGroupInvisible & ~((1 - m_nColliders) >> 31);
	return min(min(m_timeStepFull - m_timeStepPerformed, m_maxAllowedStep * (1 - bDegradeQuality * 0.5f)),
	           time_interval);
}

void CRigidEntity::ArchiveContact(entity_contact* pContact, float imp, int bLastInGroup, float r)
{
	if (m_flags & (pef_monitor_collisions | pef_log_collisions) && !(pContact->flags & contact_archived))
	{
		int i;
		float vrel;
		EventPhysCollision event;
		for (i = 0; i < 2; i++)
		{
			event.pEntity[i] = pContact->pent[i];
			event.pForeignData[i] = pContact->pent[i]->m_pForeignData;
			event.iForeignData[i] = pContact->pent[i]->m_iForeignData;
		}
		event.pt = pContact->pt[0];
		event.n = pContact->n;
		event.idCollider = m_pWorld->GetPhysicalEntityId(pContact->pent[1]);
		for (i = 0; i < 2; i++)
		{
			event.vloc[i] =
			    pContact->pbody[i]->v + (pContact->pbody[i]->w ^ pContact->pt[0] - pContact->pbody[i]->pos);
			event.mass[i] = pContact->pbody[i]->M;
			if ((event.partid[i] = pContact->pent[i]->m_parts[pContact->ipart[i]].id) <= -2)
			{
				event.partid[i] = -2 - event.partid[i];
			}
			event.idmat[i] = pContact->id[i];
		}
		event.penetration = pContact->penetration;
		event.pEntContact = pContact;
		event.normImpulse = imp;
		event.radius = r * (1 + isneg(7 - m_nParts) * 1.5f);
		pContact->flags |= contact_archived;

		if (m_flags & pef_monitor_collisions)
		{
			m_pWorld->SignalEvent(&event, 0);
		}

		if (m_flags & pef_log_collisions)
		{
			i = iszero(m_pWorld->m_iLastLogPump - m_iLastLogColl);
			m_nEvents &= -i;
			m_vcollMin += (1e10f - m_vcollMin) * (i ^ 1);
			m_iLastLogColl = m_pWorld->m_iLastLogPump;
			if ((pContact->pent[0]->m_parts[pContact->ipart[0]].flags |
			     pContact->pent[1]->m_parts[pContact->ipart[1]].flags) &
			    geom_no_coll_response)
			{
				if (bLastInGroup)
				{
					m_pWorld->OnEvent(pef_log_collisions, &event);
				}
			}
			else if (m_nMaxEvents > 0)
			{
				vrel = (event.vloc[0] - event.vloc[1]).len2();
				if (m_nEvents == m_nMaxEvents)
				{
					if (vrel > m_vcollMin && vrel < 1E10f)
					{
						memcpy(&m_pEventsColl[m_icollMin]->idCollider, &event.idCollider,
						       sizeof(event) - ((INT_PTR)&event.idCollider - (INT_PTR)&event));
						for (m_vcollMin = 1E10f, i = 0, m_icollMin = -1; i < m_nEvents; i++)
						{
							if ((vrel =
							         (m_pEventsColl[i]->vloc[0] - m_pEventsColl[i]->vloc[1])
							             .len2()) < m_vcollMin)
							{
								m_vcollMin = vrel;
								m_icollMin = i;
							}
						}
					}
				}
				else
				{
					if (vrel < m_vcollMin)
					{
						m_vcollMin = vrel;
						m_icollMin = m_nEvents;
					}
					m_pWorld->OnEvent(pef_log_collisions, &event, &m_pEventsColl[m_nEvents++]);
				}
			}
		}
	}
}

int CRigidEntity::GetContactCount(int nMaxPlaneContacts)
{
	if (m_flags & ref_use_simple_solver)
	{
		return 0;
	}

	int i, nContacts, nTotContacts;
	entity_contact* pContact;

	for (i = nTotContacts = nContacts = 0; i < m_nColliders; i++, nTotContacts += min(nContacts, nMaxPlaneContacts))
	{
		pContact = m_pColliderContacts[i];
		if (pContact)
		{
			for (nContacts = 0;; pContact = pContact->next)
			{
				nContacts += (pContact->flags >> contact_2b_verified_log2 ^ 1) & 1;
				if (pContact->flags & contact_last)
				{
					break;
				}
			}
		}
	}
	return nTotContacts;
}

int CRigidEntity::RegisterContacts(float time_interval, int nMaxPlaneContacts)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	int i, j, bComplexCollider, bStable, nContacts, bUseAreaContact;
	Vec3 sz = m_BBox[1] - m_BBox[0], n;
	float mindim = min(sz.x, min(sz.y, sz.z)), mindim1, dist, friction, penetration;
	entity_contact *pContact = nullptr, *pContact0 = nullptr, *pContactLin = nullptr, *pContactAng = nullptr;
	RigidBody* pbody;
	m_body.Fcollision.zero();
	m_body.Tcollision.zero();
	m_bStable &= -((int)m_flags & ref_use_simple_solver) >> 31;
	if (m_nPrevColliders != m_nColliders)
	{
		m_nRestMask = 0;
	}
	m_nPrevColliders = m_nColliders;

	UpdateConstraints(time_interval);

	for (i = 0; i < m_nColliders; i++)
	{
		if (!(m_flags & ref_use_simple_solver) && m_pColliderContacts[i])
		{
			pbody = 0;
			bComplexCollider = bStable = bUseAreaContact = 0;
			friction = penetration = 0;
			for (pContact = m_pColliderContacts[i], nContacts = 0;; pContact = pContact->next, nContacts++)
			{
				if (!(pContact->flags & contact_2b_verified))
				{
					if (pbody && pContact->pbody[1] != pbody)
					{
						bComplexCollider = 1;
					}
					pContact0 = pContact;
					pbody = pContact->pbody[1];
					pContact->flags &= ~contact_maintain_count;
					friction = max(friction, pContact->friction);
					penetration = max(penetration, pContact->penetration);
				}
				pContact->nextAux = pContact->next;
				pContact->prevAux = pContact->prev;
				if (pContact->flags & contact_last)
				{
					break;
				}
			}
			pContact = m_pColliderContacts[i];
			if (!bComplexCollider)
			{
				sz = m_pColliders[i]->m_BBox[1] - m_pColliders[i]->m_BBox[0];
				mindim1 = min(sz.x, min(sz.y, sz.z));
				mindim1 += iszero(mindim1) * mindim;
				bStable = CompactContactBlock(m_pColliderContacts[i], contact_last,
				                              min(mindim, mindim1) * 0.15f, nMaxPlaneContacts,
				                              nContacts, pContact, n, dist, m_body.pos, m_gravity);
				if (bStable && dist < min(mindim, mindim1) * 0.05f)
				{
					pContactLin = (entity_contact*)AllocSolverTmpBuf(sizeof(entity_contact));
					pContactAng = (entity_contact*)AllocSolverTmpBuf(sizeof(entity_contact));
					if (pContactLin && pContactAng)
					{
						bUseAreaContact = 1;
						pContactLin->pent[0] = pContactAng->pent[0] = this;
						pContactLin->pent[1] = pContactAng->pent[1] = m_pColliders[i];
						pContactLin->pbody[0] = pContactAng->pbody[0] = &m_body;
						pContactLin->pbody[1] = pContactAng->pbody[1] = pbody;
						pContactLin->ipart[0] = pContactLin->ipart[1] = -1;
						pContactAng->ipart[0] = pContactAng->ipart[1] = -1;
						pContactLin->flags =
						    contact_maintain_count + 3; // max(3,(nContacts>>1)+1);
						pContactLin->pBounceCount = &pContactAng->iCount;
						pContactAng->flags = contact_angular | contact_constraint_1dof | 1;
						pContactLin->friction = friction;
						pContactLin->pt[0] = m_body.pos;
						pContactLin->n = pContactAng->n = pContact0->n;
						pContactLin->vreq =
						    pContactLin->n *
						    min(m_pWorld->m_vars.maxUnprojVel,
						        max(0.0f, (penetration - m_pWorld->m_vars.maxContactGap)) *
						            m_pWorld->m_vars.unprojVelScale);
						pContactAng->vreq =
						    pContact0->penetration > 0 ? pContact0->nloc * 10.0f : Vec3();
						pContactLin->K.SetZero();
						if ((m_BBox[1] - m_BBox[0]).GetVolume() >
						    (m_pColliders[i]->m_BBox[1] - m_pColliders[i]->m_BBox[0])
						            .GetVolume() *
						        0.1f)
						{
							pContactLin->pt[1] = pbody->pos;
							pContactLin->K(0, 0) = pContactLin->K(1, 1) =
							    pContactLin->K(2, 2) = m_body.Minv + pbody->Minv;
						}
						else
						{
							pContactLin->K(0, 0) = pContactLin->K(1, 1) =
							    pContactLin->K(2, 2) = m_body.Minv;
							m_pColliders[i]->GetContactMatrix(
							    pContactLin->pt[1] = m_body.pos, pContact0->ipart[1],
							    pContactLin->K);
						}
						pContactAng->K = m_body.Iinv + pbody->Iinv;
						m_pStableContact = pContact0;
					}
				}
				bStable += bStable & (m_pColliders[i]->IsAwake() ^ 1);
			}
			m_bStable = max(m_bStable, bStable);
			for (j = 0; j < nContacts; j++, pContact = pContact->nextAux)
			{
				pContact->pBounceCount = &pContactLin->iCount;
				pContact->flags |= contact_maintain_count & -bUseAreaContact;
				RegisterContact(pContact);
				ArchiveContact(pContact);
			}
			if (bUseAreaContact)
			{
				RegisterContact(pContactLin);
				RegisterContact(pContactAng);
			}
		}
		for (j = 0; j < NMASKBITS && getmask(j) <= m_pColliderConstraints[i]; j++)
		{
			if (m_pColliderConstraints[i] & getmask(j) && m_pConstraintInfos[j].bActive)
			{
				RegisterContact(m_pConstraints + j);
			}
		}
	}
	if (m_submergedFraction > 0)
	{
		g_bUsePreCG = false;
	}
	return 1;
}

void CRigidEntity::UpdateContactsAfterStepBack(float time_interval)
{
	entity_contact* pContact;
	Vec3 diff;

	for (pContact = m_pContactStart; pContact != CONTACT_END(m_pContactStart); pContact = pContact->next)
	{
		diff = pContact->pbody[0]->q * pContact->ptloc[0] + pContact->pbody[0]->pos -
		       pContact->pbody[1]->q * pContact->ptloc[1] - pContact->pbody[1]->pos;
		// leave contact point and ptloc unchanged, but update penetration and vreq
		// pContact->ptloc[0] = (pContact->pt[0]-pContact->pbody[0]->pos)*pContact->pbody[0]->q;
		// pContact->ptloc[1] = (pContact->pt[0]-pContact->pbody[1]->pos)*pContact->pbody[1]->q;
		if (pContact->penetration > 0)
		{
			pContact->penetration = max(0.0f, pContact->penetration - min(0.0f, pContact->n * diff));
			pContact->vreq = pContact->n * min(1.2f, pContact->penetration * 10.0f);
		}
	}
}

void CRigidEntity::StepBack(float time_interval)
{
	if (time_interval > 0)
	{
		m_body.pos = m_prevPos;
		m_body.q = m_prevq;

		Matrix33 R = Matrix33(m_body.q);
		m_body.Iinv = R * m_body.Ibody_inv * R.T();
		m_qNew = m_body.q * m_body.qfb;
		m_posNew = m_body.pos - m_qNew * m_body.offsfb;
		m_bSteppedBack = 1;

		coord_block_BBox partCoordTmp[2];
		ComputeBBoxRE(partCoordTmp);
		UpdatePosition(m_pWorld->RepositionEntity(this, 1, m_BBoxNew));
	}

	m_body.v = m_prevv;
	m_body.w = m_prevw;
	m_body.P = m_body.v * m_body.M;
	m_body.L = m_body.q * (m_body.Ibody * (!m_body.q * m_body.w));

	if (m_pEventsColl && m_pWorld->m_iLastLogPump == m_iLastLogColl)
	{
		for (int i = 0; i < m_nEvents; i++)
		{
			if (m_pEventsColl[i] &&
			    ((CPhysicalEntity*)m_pEventsColl[i]->pEntity[1])->m_flags & pef_deforming)
			{
				m_pEventsColl[i]->mass[1] = 0.01f;
			}
		}
	}
}

float CRigidEntity::GetDamping(float time_interval)
{
	float damping = max(m_nColliders ? m_damping : m_dampingFreefall, m_dampingEx);
	// if (!(m_flags & pef_fixed_damping) && m_nGroupSize>=m_pWorld->m_vars.nGroupDamping)
	//	damping = max(damping,m_pWorld->m_vars.groupDamping);

	return max(0.0f, 1.0f - (damping * time_interval));
}

int CRigidEntity::Update(float time_interval, float damping)
{
	int i;
	float dt, E, E_accum, Emin = m_bFloating && m_nColliders + m_nPrevColliders == 0 ? m_EminWater : m_Emin;
	Vec3 v_accum, w_accum, L_accum, pt[4];
	coord_block_BBox partCoordTmp[2];
	// m_nStickyContacts = m_nSlidingContacts = 0;
	m_nStepBackCount = (m_nStepBackCount & -m_bSteppedBack) + m_bSteppedBack;
	if (m_flags & ref_use_simple_solver)
	{
		damping = max(0.9f, damping);
	}
	CapBodyVel();

	m_body.v *= damping;
	m_body.w *= damping;
	m_body.P *= damping;
	m_body.L *= damping;

	if (m_body.Eunproj > 0)
	{ // if the solver changed body position
		m_qNew = m_body.q * m_body.qfb;
		m_posNew = m_body.pos - m_qrot * m_body.offsfb;
		ComputeBBoxRE(partCoordTmp);
		UpdatePosition(m_pWorld->RepositionEntity(this, 1, m_BBoxNew));
	}

	if (m_flags & pef_log_collisions && m_nMaxEvents > 0)
	{
		i = iszero(m_pWorld->m_iLastLogPump - m_iLastLogColl);
		m_nEvents &= -i;
		m_vcollMin += (1e10f - m_vcollMin) * (i ^ 1);
		m_iLastLogColl = m_pWorld->m_iLastLogPump;
		for (i = 0; i < m_nEvents; i++)
		{
			if (m_pEventsColl[i]->pEntContact)
			{
				m_pEventsColl[i]->normImpulse +=
				    ((entity_contact*)m_pEventsColl[i]->pEntContact)->Pspare;
				m_pEventsColl[i]->pEntContact = 0;
			}
		}
	}

	pe_action_update_constraint auc;
	auc.bRemove = 1;
	masktype constraint_mask = 0;
	for (i = 0; i < m_nColliders; constraint_mask |= m_pColliderConstraints[i++])
		;
	for (i = 0; i < NMASKBITS && getmask(i) <= constraint_mask; i++)
		;
	for (--i; i >= 0; i--)
	{
		if (constraint_mask & getmask(i) && m_pConstraintInfos[i].flags & constraint_broken)
		{
			for (; i > 0 && m_pConstraintInfos[i].id == m_pConstraintInfos[i - 1].id; i--)
				;
			auc.idConstraint = m_pConstraintInfos[i].id;
			Action(&auc);
		}
	}

	entity_contact* pContact = m_pStableContact;
	Vec3 v0 = m_body.v, w0 = m_body.w;
	if (m_bStable != 2)
	{
		for (i = 0, pContact = 0; i < m_nColliders && m_pColliders[i]->m_iSimClass == 0; i++)
			;
		if (i < m_nColliders)
		{
			pContact = m_pColliderContacts[i];
		}
	}
	if (pContact && pContact->pent[1]->m_iSimClass > 0)
	{
		Vec3 vSleep, wSleep;
		pContact->pent[1]->GetSleepSpeedChange(pContact->ipart[1], vSleep, wSleep);
		if (vSleep.len2() + wSleep.len2())
		{
			m_body.w -= wSleep;
			m_body.v -= vSleep + (wSleep ^ pContact->pt[0] - pContact->pbody[1]->pos);
			m_body.P = m_body.v * m_body.M;
			m_body.L = m_body.q * (m_body.Ibody * (!m_body.q * m_body.w));
		}
	}

	{
		WriteLock lock(m_lockDynState);
		m_vhist[m_iDynHist] = m_body.v;
		m_whist[m_iDynHist] = m_body.w;
		m_Lhist[m_iDynHist] = m_body.L;
		m_iDynHist = (m_iDynHist + 1) & ((sizeof(m_vhist) / sizeof(m_vhist[0])) - 1);
		m_Fcollision = m_body.Fcollision;
		m_Tcollision = m_body.Tcollision;
	}

	m_minAwakeTime = max(m_minAwakeTime, 0.0f) - time_interval;
	if (m_body.Minv > 0)
	{
		for (i = 0, v_accum.zero(), w_accum.zero(), L_accum.zero(); i < sizeof(m_vhist) / sizeof(m_vhist[0]);
		     i++)
		{
			v_accum += m_vhist[i];
			w_accum += m_whist[i];
			L_accum += m_Lhist[i];
		}
		E = ((m_body.v.len2() + (m_body.L * m_body.w) * m_body.Minv) * 0.5f) + (m_body.Eunproj * 1000.0f);
		if (m_flags & ref_use_simple_solver)
		{
			E = min(E, m_Estep);
		}
		if (m_bStable < 2 && g_bitcount[m_nRestMask & 0xFF] + g_bitcount[m_nRestMask >> 8 & 0xFF] +
		                             g_bitcount[m_nRestMask >> 16 & 0xFF] +
		                             g_bitcount[m_nRestMask >> 24 & 0xFF] <
		                         20)
		{
			E_accum =
			    ((v_accum.len2() + (w_accum * L_accum) * m_body.Minv) * 0.5f) + (m_body.Eunproj * 1000.0f);
		}
		else
		{
			E_accum = E;
		}
		dt = time_interval + ((m_timeStepFull - time_interval) * m_pWorld->m_bCurGroupInvisible);
		m_timeIdle = max(0.0f, m_timeIdle - ((iszero(m_nColliders + m_nPrevColliders + m_submergedFraction) &
		                                      isneg(-200.0f - m_pos.z)) *
		                                     dt * 3));
		m_timeIdle *= m_pWorld->m_bCurGroupInvisible;
		int i = isneg(0.0001f - m_maxTimeIdle); // forceful deactivation is turned on
		i = (i ^ 1) | isneg((m_timeIdle += dt * i) - m_maxTimeIdle);

		if (m_bAwake &= i)
		{
			if ((E < Emin || E_accum < Emin) &&
			    (m_nColliders + m_nPrevColliders + m_bFloating || m_gravity.len2() == 0) &&
			    m_minAwakeTime <= 0)
			{
				m_bAwake = 0;
			}
			else
			{
				m_nSleepFrames = 0;
			}
		}
		else if (i)
		{
			if (E_accum > Emin)
			{
				m_bAwake = 1;
				m_nSleepFrames = 0;
				if (m_iSimClass == 1)
				{
					m_iSimClass = 2;
					m_pWorld->RepositionEntity(this, 2);
				}
			}
			else
			{
				if (!(m_bFloating | (m_flags & ref_use_simple_solver)))
				{
					m_vSleep = v0;
					m_wSleep = w0;
					m_body.P.zero();
					m_body.L.zero();
					m_body.v.zero();
					m_body.w.zero();
				}
				if (++m_nSleepFrames >= 4 && m_iSimClass == 2)
				{
					m_iSimClass = 1;
					m_pWorld->RepositionEntity(this, 2);
					m_nSleepFrames = 0;
					m_timeCanopyFallen = max(0.0f, m_timeCanopyFallen - 1.5f);
				}
			}
		}
		else if (m_iSimClass == 2)
		{
			m_iSimClass = 1;
			m_pWorld->RepositionEntity(this, 2);
		}
		m_nRestMask = (m_nRestMask << 1) | (m_bAwake ^ 1);
	}
	m_bStable = 0;

	return (m_bAwake ^ 1) | isneg(m_timeStepFull - m_timeStepPerformed - 0.001f) | m_pWorld->m_bCurGroupInvisible;
}

int CRigidEntity::GetStateSnapshot(TSerialize ser, float time_back, int flags)
{
	CPhysicalEntity::GetStateSnapshot(ser, time_back, flags | ssf_from_child_class);

	SRigidEntityNetSerialize helper = {m_pos, m_qrot, m_body.v, m_body.w, m_bAwake != 0};
	if (!m_bAwake)
	{
		helper.vel = {};
		helper.angvel = {};
	}
	helper.Serialize(ser);

	if (ser.GetSerializationTarget() != eST_Network)
	{
		int i, count;
		bool bVal;
		masktype constraintMask = 0;
		for (i = 0; i < m_nColliders; constraintMask |= m_pColliderConstraints[i++])
			;

		if (ser.BeginOptionalGroup("constraints", constraintMask != 0))
		{
			for (i = count = 0; i < NMASKBITS && getmask(i) <= constraintMask;)
			{
				if (constraintMask & getmask(i) && !(m_pConstraintInfos[i].flags & constraint_rope))
				{
					ser.BeginOptionalGroup("constraint", true);
					pe_action_add_constraint aac;
					aac.id = m_pConstraintInfos[i].id;
					i += ExtractConstraintInfo(i, constraintMask, aac);
					ser.Value("id", aac.id);
					m_pWorld->SavePhysicalEntityPtr(ser, (CPhysicalEntity*)aac.pBuddy);
					ser.Value("pt0", aac.pt[0]);
					ser.Value("pt1", aac.pt[1]);
					ser.Value("partid0", aac.partid[0]);
					ser.Value("partid1", aac.partid[1]);
					if (!is_unused(aac.qframe[0]))
					{
						ser.Value("qf0used", bVal = true);
						ser.Value("qframe0", aac.qframe[0]);
					}
					else
					{
						ser.Value("qf0used", bVal = false);
					}
					if (!is_unused(aac.qframe[1]))
					{
						ser.Value("qf1used", bVal = true);
						ser.Value("qframe1", aac.qframe[1]);
					}
					else
					{
						ser.Value("qf1used", bVal = false);
					}
					if (!is_unused(aac.xlimits[0]))
					{
						ser.Value("xlim", bVal = true);
						ser.Value("xlim0", aac.xlimits[0]);
						ser.Value("xlim1", aac.xlimits[1]);
					}
					else
					{
						ser.Value("xlim", bVal = false);
					}
					if (!is_unused(aac.yzlimits[0]))
					{
						ser.Value("yzlim", bVal = true);
						ser.Value("yzlim0", aac.yzlimits[0]);
						ser.Value("yzlim1", aac.yzlimits[1]);
					}
					else
					{
						ser.Value("yzlim", bVal = false);
					}
					ser.Value("flags", aac.flags);
					ser.Value("damping", aac.damping);
					ser.Value("radius", aac.sensorRadius);
					if (!is_unused(aac.maxPullForce))
					{
						ser.Value("breakableLin", bVal = true);
						ser.Value("maxPullForce", aac.maxPullForce);
					}
					else
					{
						ser.Value("breakableLin", bVal = false);
					}
					if (!is_unused(aac.maxBendTorque))
					{
						ser.Value("breakableAng", bVal = true);
						ser.Value("maxBendTorque", aac.maxBendTorque);
					}
					else
					{
						ser.Value("breakableAng", bVal = false);
					}
					ser.EndGroup();
					count++;
				}
				else
				{
					i++;
				}
			}
			ser.Value("count", count);
			ser.EndGroup();
		}
	}

	return 1;
}

int CRigidEntity::SetStateFromSnapshot(TSerialize ser, int flags)
{
	CPhysicalEntity::SetStateFromSnapshot(ser, flags | ssf_from_child_class);

	SRigidEntityNetSerialize helper = {m_pos, m_qrot, m_body.v, m_body.w, m_iSimClass > 1};
	helper.Serialize(ser);

	if ((ser.GetSerializationTarget() == eST_Network) && (helper.pos.len2() < 50.0f))
	{
		return 1;
	}

	if (m_pWorld && ser.ShouldCommitValues())
	{
		pe_params_pos setpos;
		setpos.bRecalcBounds = 16 | 32;
		pe_action_set_velocity velocity;

		Vec3 debugOnlyOriginalHelperPos = helper.pos;

		if ((flags & ssf_compensate_time_diff) && helper.simclass)
		{
			float dtBack = (m_pWorld->m_iTimeSnapshot[1] - m_pWorld->m_iTimeSnapshot[0]) *
			               m_pWorld->m_vars.timeGranularity;
			helper.pos += helper.vel * dtBack;

			if (helper.angvel.len2() * sqr(dtBack) < sqr(0.003f))
			{
				// quaternionf q(0.0f, helper.angvel*0.5f); //q.Normalize();
				// helper.rot += q*helper.rot*dtBack;
				helper.rot.w -= (helper.angvel * helper.rot.v) * dtBack * 0.5f;
				helper.rot.v +=
				    ((helper.angvel ^ helper.rot.v) + helper.angvel * helper.rot.w) * (dtBack * 0.5f);
			}
			else
			{
				float wlen = helper.angvel.len();
				// q = quaternionf(wlen*dt,w/wlen)*q;
				helper.rot = Quat::CreateRotationAA(wlen * dtBack, helper.angvel / wlen) * helper.rot;
			}
			helper.rot.Normalize();
		}

		float distance = m_pos.GetDistance(helper.pos);

		//CryMP
		const bool ignoreNetState = ser.GetSerializationTarget() == eST_Network &&
			(m_flags & pef_ignore_network_state);

		if (!ignoreNetState && (m_body.Minv || ser.GetSerializationTarget() != eST_Network))
		{
			if (helper.simclass && ser.GetSerializationTarget() == eST_Network)
			{
				const float MAX_DIFFERENCE = m_pWorld->m_vars.netMinSnapDist +
				                             (m_pWorld->m_vars.netVelSnapMul * helper.vel.GetLength());
				if (distance > MAX_DIFFERENCE)
				{
					setpos.pos = helper.pos;
				}
				else
				{
					setpos.pos =
					    m_pos + (helper.pos - m_pos) *
							clamp(distance / MAX_DIFFERENCE /
					                          std::max(m_pWorld->m_vars.netSmoothTime, 0.01f),
					                      0.0f, 1.0f);
				}
				if (helper.vel.GetLength() < 0.1f && distance > 0.05f)
				{
					ser.FlagPartialRead();
				}

				const float MAX_DOT =
				    std::max(0.01f, 1.0f - m_pWorld->m_vars.netMinSnapDot -
				                        (m_pWorld->m_vars.netAngSnapMul * helper.angvel.GetLength()));
				float quatDiff = 1.0f - fabsf(m_qrot | helper.rot);
				if (quatDiff > MAX_DOT)
				{
					setpos.q = helper.rot;
				}
				else
				{
					setpos.q = Quat::CreateSlerp(
					    m_qrot, helper.rot,
					    clamp(quatDiff / MAX_DOT / std::max(m_pWorld->m_vars.netSmoothTime, 0.01f),
					          0.0f, 1.0f));
				}
				if (helper.angvel.GetLength() < 0.1f && quatDiff > 0.05f)
				{
					ser.FlagPartialRead();
				}
			}
			else
			{
				setpos.pos = helper.pos;
				setpos.q = helper.rot;
			}

			setpos.iSimClass = helper.simclass ? 2 : 1;
			SetParams(&setpos, 0);

			velocity.v = helper.vel;
			velocity.w = helper.angvel;
			Action(&velocity, 0);

			int i, j;
			masktype constraintMask = 0;
			pe_action_update_constraint auc;
			auc.bRemove = 1;
			for (i = 0; i < m_nColliders; constraintMask |= m_pColliderConstraints[i++])
				;
			for (i = NMASKBITS - 1; i >= 0;)
			{
				if (constraintMask & getmask(i) && !(m_pConstraintInfos[i].flags & constraint_rope))
				{
					auc.idConstraint = m_pConstraintInfos[i].id;
					for (j = i; i >= 0 && m_pConstraintInfos[i].id == m_pConstraintInfos[j].id; i--)
						;
					Action(&auc, 0);
				}
				else
				{
					i--;
				}
			}
		}
	}

	if (ser.GetSerializationTarget() != eST_Network)
	{
		if (ser.BeginOptionalGroup("constraints", true))
		{
			int count = 0;
			ser.Value("count", count);
			while (--count >= 0)
			{
				if (ser.BeginOptionalGroup("constraint", true))
				{
					bool bVal;
					pe_action_add_constraint aac;
					ser.Value("id", aac.id);
					aac.pBuddy = m_pWorld->LoadPhysicalEntityPtr(ser);
					if (aac.pBuddy)
					{
						ser.Value("pt0", aac.pt[0]);
						ser.Value("pt1", aac.pt[1]);
						ser.Value("partid0", aac.partid[0]);
						ser.Value("partid1", aac.partid[1]);
						ser.Value("qf0used", bVal);
						if (bVal)
						{
							ser.Value("qframe0", aac.qframe[0]);
						}
						ser.Value("qf1used", bVal);
						if (bVal)
						{
							ser.Value("qframe1", aac.qframe[1]);
						}
						ser.Value("xlim", bVal);
						if (bVal)
						{
							ser.Value("xlim0", aac.xlimits[0]);
							ser.Value("xlim1", aac.xlimits[1]);
						}
						ser.Value("yzlim", bVal);
						if (bVal)
						{
							ser.Value("yzlim0", aac.yzlimits[0]);
							ser.Value("yzlim1", aac.yzlimits[1]);
						}
						ser.Value("flags", aac.flags);
						ser.Value("damping", aac.damping);
						ser.Value("radius", aac.sensorRadius);
						ser.Value("breakableLin", bVal);
						if (bVal)
						{
							ser.Value("maxPullForce", aac.maxPullForce);
						}
						ser.Value("breakableAng", bVal);
						if (bVal)
						{
							ser.Value("maxBendTorque", aac.maxBendTorque);
						}
						if (ser.ShouldCommitValues())
						{
							Action(&aac);
						}
					}
					ser.EndGroup(); // constrain
				}
			}
			ser.EndGroup(); // constrains
		}
		m_nEvents = 0;
	}

	return 1;
}

int CRigidEntity::PostSetStateFromSnapshot()
{
	if (m_bJustLoaded)
	{
		pe_status_id si;
		int i, j;
		entity_contact *pContact, *pContactNext;
		m_nColliders = m_bJustLoaded;
		m_bJustLoaded = 0;

		for (pContact = m_pContactStart; pContact != CONTACT_END(m_pContactStart); pContact = pContactNext)
		{
			pContactNext = pContact;
			pContact->pent[1] =
			    (CPhysicalEntity*)m_pWorld->GetPhysicalEntityById((INT_PTR)pContact->pent[1]);
			if (pContact->pent[1] &&
			    (unsigned int)pContact->ipart[0] < (unsigned int)pContact->pent[0]->m_nParts &&
			    (unsigned int)pContact->ipart[1] < (unsigned int)pContact->pent[1]->m_nParts)
			{
				pContact->pbody[1] = pContact->pent[1]->GetRigidBody(pContact->ipart[1], 1);
				si.ipart = pContact->ipart[1];
				si.iPrim = pContact->iPrim[1];
				si.iFeature = pContact->iFeature[1];
				if (pContact->pent[1]->GetStatus(&si))
				{
					pContact->id[1] = si.id;
					pContact->n = pContact->pbody[pContact->iNormal]->q * pContact->nloc;
					pContact->vreq = pContact->n * min(1.2f, pContact->penetration * 10.0f);
					pContact->K.SetZero();
					pContact->pent[0]->GetContactMatrix(pContact->pt[0], pContact->ipart[0],
					                                    pContact->K);
					pContact->pent[1]->GetContactMatrix(pContact->pt[1], pContact->ipart[1],
					                                    pContact->K);
					// assume that the contact is static here, for if it's dynamic, VerifyContacts
					// will update it before passing to the solver
					pContact->vrel.zero();
					pContact->friction = m_pWorld->GetFriction(pContact->id[0], pContact->id[1]);
				}
				else
				{
					DetachContact(pContact, -1, 0);
					m_pWorld->FreeContact(pContact);
				}
			}
			else
			{
				DetachContact(pContact, -1, 0);
				m_pWorld->FreeContact(pContact);
			}
		}

		for (i = m_nColliders - 1; i >= 0; i--)
		{
			if (!(m_pColliders[i] =
			          (CPhysicalEntity*)m_pWorld->GetPhysicalEntityById((INT_PTR)m_pColliders[i])) ||
			    !m_pColliderContacts[i] && !m_pColliderConstraints[i])
			{
				for (j = i; j < m_nColliders; j++)
				{
					m_pColliders[j] = m_pColliders[j + 1];
					m_pColliderContacts[j] = m_pColliderContacts[j + 1];
					m_pColliderConstraints[j] = m_pColliderConstraints[j + 1];
				}
				m_nColliders--;
			}
		}
		for (i = 0; i < m_nColliders; i++)
		{
			m_pColliders[i]->AddRef();
		}
	}
	return 1;
}

unsigned int CRigidEntity::GetStateChecksum()
{
	return physics_float2int(m_pos.x * 1024) ^ physics_float2int(m_pos.y * 1024) ^
	       physics_float2int(m_pos.z * 1024) ^ physics_float2int(m_qrot.v.x * 1024) ^
	       physics_float2int(m_qrot.v.y * 1024) ^ physics_float2int(m_qrot.v.z * 1024) ^
	       physics_float2int(m_body.v.x * 1024) ^ physics_float2int(m_body.v.y * 1024) ^
	       physics_float2int(m_body.v.z * 1024) ^ physics_float2int(m_body.w.x * 1024) ^
	       physics_float2int(m_body.w.y * 1024) ^ physics_float2int(m_body.w.z * 1024);
}

void CRigidEntity::ApplyBuoyancy(float time_interval, const Vec3& gravity, pe_params_buoyancy* pb, int nBuoys)
{
	if (m_kwaterDensity == 0 || m_body.Minv == 0)
	{
		m_submergedFraction = 0;
		return;
	}
	int i = 0, ibuoy, ibuoySplash = -1;
	Vec3 sz, center, dP, totResistance, vel0 = m_body.v;
	float r, dist, V, Vsubmerged, Vfull, submergedFraction, waterFraction = 0;
	geom_world_data gwd;
	pe_action_impulse ai;
	RigidBody* pbody;
	primitives::box bbox;
	ai.iSource = 1;
	ai.iApplyTime = 0;
	m_bFloating = 0;

	for (ibuoy = 0; ibuoy < nBuoys; ibuoy++)
	{
		if ((unsigned int)pb[ibuoy].iMedium < 2u)
		{
			sz = (m_BBoxNew[1] - m_BBoxNew[0]) * 0.5f;
			center = (m_BBoxNew[1] + m_BBoxNew[0]) * 0.5f;
			r = fabsf(pb[ibuoy].waterPlane.n.x * sz.x) + fabsf(pb[ibuoy].waterPlane.n.y * sz.y) +
			    fabsf(pb[ibuoy].waterPlane.n.z * sz.z);
			dist = (center - pb[ibuoy].waterPlane.origin) * pb[ibuoy].waterPlane.n;
			if (dist > r)
			{
				continue;
			}

			for (i = 0, Vsubmerged = Vfull = 0; i < m_nParts; i++)
			{
				if (m_parts[i].flags & geom_floats &&
				    (m_parts[i].pPhysGeomProxy->V * cube(m_parts[i].scale) * pb[ibuoy].waterDensity *
				             m_kwaterDensity >
				         m_parts[i].mass * 0.1f ||
				     (m_parts[i].pPhysGeomProxy->pGeom->GetBBox(&bbox),
				      max(max(bbox.size.x * bbox.size.y, bbox.size.y * bbox.size.z),
				          bbox.size.z * bbox.size.x) *
				              sqr(m_parts[i].scale) * pb[ibuoy].waterResistance * m_kwaterResistance >
				          m_parts[i].mass * 0.0125f) ||
				     pb[ibuoy].waterDamping > 0))
				{
					gwd.R = Matrix33(m_qNew * m_parts[i].pNewCoords->q);
					gwd.offset = m_posNew + m_qNew * m_parts[i].pNewCoords->pos;
					gwd.scale = m_parts[i].pNewCoords->scale;
					pbody = GetRigidBody(i);
					gwd.v = pbody->v - pb[ibuoy].waterFlow;
					gwd.w = pbody->w;
					gwd.centerOfMass = pbody->pos;

					if (pb[ibuoy].waterResistance > 0)
					{
						m_parts[i].pPhysGeomProxy->pGeom->CalculateMediumResistance(
						    &pb[ibuoy].waterPlane, &gwd, ai.impulse, ai.angImpulse);
						ai.impulse *= pb[ibuoy].waterResistance * m_kwaterResistance;
						totResistance += ai.impulse * -((pb[ibuoy].iMedium - 1) >> 31);
						ibuoySplash += (ibuoy - ibuoySplash) & (pb[ibuoy].iMedium - 1) >> 31;
						ai.impulse *= time_interval;
						ai.angImpulse *=
						    pb[ibuoy].waterResistance * m_kwaterResistance * time_interval;
					}
					else
					{
						if (pb[ibuoy].waterFlow.len2() > 0)
						{
							ai.impulse = (pb[ibuoy].waterFlow - pbody->v) *
							             ((sz.x * sz.y + sz.x * sz.z + sz.y * sz.z) *
							              pb[ibuoy].waterDensity * m_kwaterDensity);
						}
						else
						{
							ai.impulse.zero();
						}
						ai.angImpulse.zero();
					}

					if (dist > -r)
					{
						V = m_parts[i].pPhysGeomProxy->pGeom->CalculateBuoyancy(
						    &pb[ibuoy].waterPlane, &gwd, center);
						dP = gravity *
						     (-pb[ibuoy].waterDensity * m_kwaterDensity * V * time_interval);
						ai.impulse += dP;
						ai.angImpulse += center - pbody->pos ^ dP;
					}
					else
					{
						V = m_parts[i].pPhysGeomProxy->V * cube(m_parts[i].scale);
						ai.impulse -= gravity * (pb[ibuoy].waterDensity * m_kwaterDensity * V *
						                         time_interval);
					}

					ai.ipart = i;
					if (ai.impulse.len2() + ai.angImpulse.len2() > 0)
					{
						Action(&ai, 1);
					}
					Vsubmerged += V;
					Vfull += m_parts[i].pPhysGeomProxy->V;
				}
			}
			if (Vfull * Vsubmerged > 0)
			{
				submergedFraction = Vsubmerged < Vfull ? Vsubmerged / Vfull : 1.0f;
				m_dampingEx = max(
				    m_dampingEx, (m_damping * (1 - submergedFraction)) +
						     (max(m_waterDamping, pb[ibuoy].waterDamping) * submergedFraction));
				if (pb[ibuoy].iMedium == 0)
				{
					waterFraction = max(waterFraction, submergedFraction);
					if (m_body.M < pb[ibuoy].waterDensity * m_kwaterDensity * m_body.V)
					{
						m_bFloating = 1;
					}
				}
			}
		}
	}
	m_submergedFraction = waterFraction;

	if (m_pWorld->m_pWaterMan && inrange(m_submergedFraction, 0.001f, 0.999f))
	{
		m_pWorld->m_pWaterMan->OnWaterInteraction(this);
	}

	float R0 = m_pWorld->m_vars.minSplashForce0, R1 = m_pWorld->m_vars.minSplashForce1,
	      v0 = m_pWorld->m_vars.minSplashVel0, v1 = m_pWorld->m_vars.minSplashVel1,
	      d0 = m_pWorld->m_vars.splashDist0, d1 = m_pWorld->m_vars.splashDist1,
	      rd = isqrt_tpl(max((m_body.pos - m_pWorld->m_posViewer).len2(), d0 * d0));
	if (m_pWorld->m_vars.maxSplashesPerObj * (ibuoySplash + 1) > 0 && !(m_flags & ref_no_splashes) &&
	    m_submergedFraction < 0.9f && 1.0f < d1 * rd &&
	    max((totResistance.len2() * rd * rd * sqr(d1 - d0)) - sqr(R1 - R0 + ((R0 * d1 - R1 * d0) * rd)),
	        ((pb[ibuoySplash].waterPlane.n * vel0) * rd * (d0 - d1)) - (v1 - v0 + (v0 * d1 - v1 * d0) * rd)) > 0)
	{
		int icont, icirc, nSplashes = 0;
		intersection_params ip;
		geom_contact* pcont;
		CBoxGeom gbox;
		primitives::box abox;
		vector2df* centers;
		float* radii;
		EventPhysCollision epc;

		abox.Basis.SetIdentity();
		abox.bOriented = 0;
		abox.center = (m_BBoxNew[1] + m_BBoxNew[0]) * 0.5f;
		abox.size = m_BBoxNew[1] - m_BBoxNew[0];
		abox.center.z = pb[ibuoySplash].waterPlane.origin.z - abox.size.z;
		gbox.CreateBox(&abox);
		gwd.v.zero();
		gwd.w.zero();
		ip.bNoAreaContacts = 1;
		ip.vrel_min = 1E10f;
		epc.pEntity[0] = this;
		epc.pForeignData[0] = m_pForeignData;
		epc.iForeignData[0] = m_iForeignData;
		epc.pEntity[1] = &g_StaticPhysicalEntity;
		epc.pForeignData[1] = 0;
		epc.iForeignData[1] = 0;
		epc.idCollider = -2;
		epc.n = pb[ibuoySplash].waterPlane.n;
		epc.vloc[1].zero();
		epc.mass[1] = 0;
		epc.partid[1] = 0;
		epc.idmat[1] = m_pWorld->m_matWater;
		epc.penetration = m_submergedFraction;
		epc.normImpulse = totResistance * epc.n;
		epc.mass[0] = m_body.M;
		epc.radius = max(max(abox.size.x, abox.size.y), abox.size.z);
		if (epc.radius < 0.5f)
		{
			epc.pt = abox.center;
			epc.vloc[0] = m_body.v;
			epc.partid[0] = m_parts[0].id;
			epc.idmat[0] = GetMatId(m_parts[0].pPhysGeom->pGeom->GetPrimitiveId(0, 0), i);
			m_pWorld->OnEvent(pef_log_collisions, &epc);
		}
		else
		{
			for (i = 0; i < m_nParts; i++)
			{
				if (m_parts[i].flags & geom_floats)
				{
					gwd.R = Matrix33(m_qNew * m_parts[i].pNewCoords->q);
					gwd.offset = m_posNew + m_qNew * m_parts[i].pNewCoords->pos;
					gwd.scale = m_parts[i].pNewCoords->scale;
					icont = m_parts[i].pPhysGeomProxy->pGeom->Intersect(&gbox, &gwd, 0, &ip, pcont);
					if (icont)
					{
						WriteLockCond lock(*ip.plock, 0);
						lock.SetActive(1);
						for (--icont; icont >= 0; icont--)
						{
							for (icirc = CoverPolygonWithCircles(
									 strided_pointer<vector2df>(
									     (vector2df*)pcont[icont].ptborder,
									     sizeof(Vec3)),
									 pcont[icont].nborderpt,
									 pcont[icont].bBorderConsecutive,
									 vector2df(pcont[icont].center), centers, radii,
									 0.5f) -
							             1;
							     icirc >= 0; icirc--)
							{
								*(vector2df*)&epc.pt = centers[icirc];
								epc.pt.z = pcont[icont].center.z;
								epc.vloc[0] =
								    m_body.v + (m_body.w ^ epc.pt - m_body.pos);
								epc.partid[0] = m_parts[i].id;
								epc.idmat[0] = GetMatId(pcont[icont].id[0], i);
								epc.radius = radii[icirc];
								m_pWorld->OnEvent(pef_log_collisions, &epc);
								if (++nSplashes == m_pWorld->m_vars.maxSplashesPerObj)
								{
									goto SplashesNoMore;
								}
							}
						}
					}
				}
			}
		}
	SplashesNoMore:;
	}
}

int CRigidEntity::CompactContactBlock(entity_contact* pContact, int endFlags, float maxPlaneDist, int nMaxContacts,
                                      int& nContacts, entity_contact*& pResContact, Vec3& n, float& maxDist,
                                      const Vec3& ptTest, const Vec3& dirTest)
{
	int i, j, nEdges;
	Matrix33 C, Ctmp;
	Vec3 pt, p_avg, center, axes[2];
	float detabs[3], det;
	const int NMAXCONTACTS = 256;
	ptitem2d pts[NMAXCONTACTS];
	edgeitem edges[NMAXCONTACTS], *pedge, *pminedge, *pnewedge;
	entity_contact* pContacts[NMAXCONTACTS];

	nContacts = 0;
	p_avg.zero();
	if (pContact != CONTACT_END(m_pContactStart))
	{
		do
		{
			if (!(pContact->flags & contact_2b_verified))
			{
				p_avg += pContact->pt[0];
				pts[nContacts].iContact = nContacts;
				pContacts[nContacts++] = pContact;
			}
		}
		while (nContacts < NMAXCONTACTS && !(pContact->flags & endFlags) &&
		       (pContact = pContact->next) != CONTACT_END(m_pContactStart));
	}
	if (nContacts <= nMaxContacts && dirTest.len2() == 0 || nContacts < 3)
	{
		return 0;
	}

	center = p_avg / nContacts;
	for (i = 0, C.SetZero(); i < nContacts;
	     i++) // while it's possible to calculate C and center in one pass, it will lose precision
	{
		C += dotproduct_matrix(pContacts[i]->pt[0] - center, pContacts[i]->pt[0] - center, Ctmp);
	}
	detabs[0] = fabs_tpl((C(1, 1) * C(2, 2)) - (C(2, 1) * C(1, 2)));
	detabs[1] = fabs_tpl((C(2, 2) * C(0, 0)) - (C(0, 2) * C(2, 0)));
	detabs[2] = fabs_tpl((C(0, 0) * C(1, 1)) - (C(1, 0) * C(0, 1)));
	i = idxmax3(detabs);
	if (detabs[i] <= fabs_tpl(C(inc_mod3[i], inc_mod3[i]) * C(dec_mod3[i], dec_mod3[i])) * 0.001f)
	{
		return 0;
	}
	det = 1.0f / (C(inc_mod3[i], inc_mod3[i]) * C(dec_mod3[i], dec_mod3[i]) -
	              C(dec_mod3[i], inc_mod3[i]) * C(inc_mod3[i], dec_mod3[i]));
	n[i] = 1;
	n[inc_mod3[i]] =
	    -((C(inc_mod3[i], i) * C(dec_mod3[i], dec_mod3[i])) - (C(dec_mod3[i], i) * C(inc_mod3[i], dec_mod3[i]))) *
	    det;
	n[dec_mod3[i]] =
	    -((C(inc_mod3[i], inc_mod3[i]) * C(dec_mod3[i], i)) - (C(dec_mod3[i], inc_mod3[i]) * C(inc_mod3[i], i))) *
	    det;
	n.normalize();
	// axes[0]=n.orthogonal(); axes[1]=n^axes[0];
	axes[0].SetOrthogonal(n);
	axes[1] = n ^ axes[0];
	for (i = 0, maxDist = 0; i < nContacts; i++)
	{
		pt = pContacts[i]->pt[0] - center;
		maxDist = max(maxDist, fabsf(pt * n));
		pts[i].pt.set(pt * axes[0], pt * axes[1]);
	}
	if (maxDist > maxPlaneDist)
	{
		return 0;
	}

	nEdges = qhull2d(pts, nContacts, edges);

	if (nMaxContacts)
	{ // chop off vertices until we have only the required amount left
		vector2df edge[2];
		for (i = 0; i < nEdges; i++)
		{
			edge[0] = edges[i].prev->pvtx->pt - edges[i].pvtx->pt;
			edge[1] = edges[i].next->pvtx->pt - edges[i].pvtx->pt;
			edges[i].area = edge[1] ^ edge[0];
			edges[i].areanorm2 = len2(edge[0]) * len2(edge[1]);
			edges[i].idx = i;
		}
		// sort edges by the area of triangles
		qsort(edges, 0, nEdges - 1);
		// transform sorted array into a linked list
		pminedge = edges + edges[0].idx;
		pminedge->next1 = pminedge->prev1 = edges + edges[0].idx;
		for (pedge = pminedge, i = 1; i < nEdges; i++)
		{
			edges[edges[i].idx].prev1 = pedge;
			edges[edges[i].idx].next1 = pedge->next1;
			pedge->next1->prev1 = edges + edges[i].idx;
			pedge->next1 = edges + edges[i].idx;
			pedge = edges + edges[i].idx;
		}

		while (nEdges > 2 && (nEdges > nMaxContacts || sqr(pminedge->area) < sqr(0.15f) * pminedge->areanorm2))
		{
			edgeitem* pedge[2];
			// delete the ith vertex with the minimum area triangle
			pminedge->next->prev = pminedge->prev;
			pminedge->prev->next = pminedge->next;
			pminedge->next1->prev1 = pminedge->prev1;
			pminedge->prev1->next1 = pminedge->next1;
			pedge[0] = pminedge->prev;
			pedge[1] = pminedge->next;
			pminedge = pminedge->next1;
			nEdges--;
			for (i = 0; i < 2; i++)
			{
				edge[0] = pedge[i]->prev->pvtx->pt - pedge[i]->pvtx->pt;
				edge[1] = pedge[i]->next->pvtx->pt - pedge[i]->pvtx->pt;
				pedge[i]->area = edge[1] ^ edge[0];
				pedge[i]->areanorm2 = len2(edge[0]) * len2(edge[1]); // update area
				for (pnewedge = pedge[i]->next1, j = 0;
				     pnewedge != pminedge && pnewedge->area < pedge[i]->area && j < nEdges + 1;
				     pnewedge = pnewedge->next1, j++)
					;
				if (pedge[i]->next1 != pnewedge)
				{ // re-insert pedge[i] before pnewedge
					if (pedge[i] == pminedge)
					{
						pminedge = pedge[i]->next1;
					}
					if (pedge[i] != pnewedge)
					{
						pedge[i]->next1->prev1 = pedge[i]->prev1;
						pedge[i]->prev1->next1 = pedge[i]->next1;
						pedge[i]->prev1 = pnewedge->prev1;
						pedge[i]->next1 = pnewedge;
						pnewedge->prev1->next1 = pedge[i];
						pnewedge->prev1 = pedge[i];
					}
				}
				else
				{
					for (pnewedge = pedge[i]->prev1, j = 0;
					     pnewedge->next1 != pminedge && pnewedge->area > pedge[i]->area &&
					     j < nEdges + 1;
					     pnewedge = pnewedge->prev1, j++)
						;
					if (pedge[i]->prev1 != pnewedge)
					{ // re-insert pedge[i] after pnewedge
						if (pnewedge->next1 == pminedge)
						{
							pminedge = pedge[i];
						}
						if (pedge[i] != pnewedge)
						{
							pedge[i]->next1->prev1 = pedge[i]->prev1;
							pedge[i]->prev1->next1 = pedge[i]->next1;
							pedge[i]->prev1 = pnewedge;
							pedge[i]->next1 = pnewedge->next1;
							pnewedge->next1->prev1 = pedge[i];
							pnewedge->next1 = pedge[i];
						}
					}
				}
			}
		}
	}
	else
	{
		pminedge = edges;
	}

	for (i = nContacts = 0, pedge = pminedge; i < nEdges; i++, nContacts++, pedge = pedge->next)
	{
		pContacts[pedge->pvtx->iContact]->nextAux = pContacts[pedge->next->pvtx->iContact];
		pContacts[pedge->pvtx->iContact]->prevAux = pContacts[pedge->prev->pvtx->iContact];
	}
	pResContact = pContacts[pminedge->pvtx->iContact];

	if (dirTest.len2() > 0)
	{
		if (nEdges < 3 || sqr(det = dirTest * n) < dirTest.len2() * 0.001f)
		{
			return 0;
		}
		pt = ptTest - center + dirTest * (((center - ptTest) * n) / det);
		vector2df pt2d(axes[0] * pt, axes[1] * pt);
		for (i = 0; i < nEdges; i++, pminedge = pminedge->next)
		{
			if ((pt2d - pminedge->pvtx->pt ^ pminedge->next->pvtx->pt - pminedge->pvtx->pt) > 0)
			{
				return 0;
			}
		}
	}

	return 1;
}

int CRigidEntity::ExtractConstraintInfo(int i, masktype constraintMask, pe_action_add_constraint& aac)
{
	int i1, j;
	aac.pBuddy = m_pConstraints[i].pent[1];
	aac.pt[0] = m_pConstraints[i].pt[0];
	aac.pt[1] = m_pConstraints[i].pt[1];
	aac.id = m_pConstraintInfos[i].id;
	if (m_pConstraintInfos[i].limit > 0)
	{
		aac.maxPullForce = m_pConstraintInfos[i].limit;
	}
	aac.damping = m_pConstraintInfos[i].damping;
	for (j = 0; j < 2; j++)
	{
		aac.partid[j] = m_pConstraints[i].pent[j]->m_parts[m_pConstraints[i].ipart[j]].id;
	}
	if (i + 1 < NMASKBITS && constraintMask & getmask(i + 1) &&
	    m_pConstraintInfos[i + 1].id == m_pConstraintInfos[i].id &&
	    m_pConstraints[i + 1].flags & (contact_constraint_2dof | contact_constraint_1dof))
	{
		for (j = 0; j < 2; j++)
		{
			aac.qframe[j] = m_pConstraints[i + 1].pbody[j]->q * m_pConstraintInfos[i + 1].qframe_rel[j];
		}
		if (m_pConstraints[i + 1].flags & contact_constraint_2dof)
		{
			aac.xlimits[0] = aac.xlimits[1] = 0;
		}
		else
		{
			aac.yzlimits[0] = aac.yzlimits[1] = 0;
		}
		aac.damping = max(aac.damping, m_pConstraintInfos[i + 1].damping);
		i1 = 2;
	}
	else
	{
		i1 = 1;
	}
	if (i1 < NMASKBITS && constraintMask & getmask(i1) && m_pConstraintInfos[i1].id == m_pConstraintInfos[i].id &&
	    m_pConstraintInfos[i1].flags & constraint_limited_1axis)
	{
		for (j = 0; j < 2; j++)
		{
			aac.xlimits[j] = m_pConstraintInfos[i1].limits[j];
		}
		if (m_pConstraintInfos[i1].limit > 0)
		{
			aac.maxBendTorque = m_pConstraintInfos[i1].limit;
		}
		i1++;
	}
	if (i1 < NMASKBITS && constraintMask & getmask(i1) && m_pConstraintInfos[i1].id == m_pConstraintInfos[i].id &&
	    m_pConstraintInfos[i1].flags & constraint_limited_2axes)
	{
		for (j = 0; j < 2; j++)
		{
			aac.yzlimits[j] = asin_tpl(m_pConstraintInfos[i1].limits[j]);
		}
		if (m_pConstraintInfos[i1].limit > 0)
		{
			aac.maxBendTorque = m_pConstraintInfos[i1].limit;
		}
		i1++;
	}
	aac.flags = world_frames | (m_pConstraintInfos[i].flags & (constraint_inactive | constraint_ignore_buddy));
	aac.sensorRadius = m_pConstraintInfos[i].sensorRadius;
	return i1;
}

EventPhysJointBroken& CRigidEntity::ReportConstraintBreak(EventPhysJointBroken& epjb, int i)
{
	epjb.idJoint = m_pConstraintInfos[i].id;
	epjb.bJoint = 0;
	MARK_UNUSED epjb.pNewEntity[0], epjb.pNewEntity[1];
	for (int iop = 0; iop < 2; iop++)
	{
		epjb.pEntity[iop] = m_pConstraints[i].pent[iop];
		epjb.pForeignData[iop] = m_pConstraints[i].pent[iop]->m_pForeignData;
		epjb.iForeignData[iop] = m_pConstraints[i].pent[iop]->m_iForeignData;
		epjb.partid[iop] = m_pConstraints[i].pent[iop]->m_parts[m_pConstraints[i].ipart[iop]].id;
	}
	epjb.pt = m_pConstraints[i].pt[0];
	epjb.n = m_pConstraints[i].n;
	epjb.partmat[0] = epjb.partmat[1] = -1;
	return epjb;
}

void CRigidEntity::OnNeighbourSplit(CPhysicalEntity* pentOrig, CPhysicalEntity* pentNew)
{
	if (this == pentNew)
	{
		return;
	}
	int i, j, i1, ipart;
	masktype constraintMask = 0, removeMask = 0, cmask;
	EventPhysJointBroken epjb;
	pe_action_update_constraint auc;
	auc.bRemove = 1;
	for (i = 0; i < m_nColliders; constraintMask |= m_pColliderConstraints[i++])
		;

	if (pentNew)
	{
		for (i = 0; i < NMASKBITS && getmask(i) <= constraintMask;)
		{
			if (constraintMask & getmask(i) && !(m_pConstraintInfos[i].flags & constraint_rope))
			{
				if (m_pConstraints[i].pent[0] == pentOrig &&
				    (ipart = pentNew->TouchesSphere(m_pConstraints[i].pt[0],
				                                    m_pConstraintInfos[i].sensorRadius)) >= 0)
				{
					ReportConstraintBreak(epjb, i).pNewEntity[0] = pentNew;
					m_pWorld->OnEvent(2, &epjb);
					pe_action_add_constraint aac;
					removeMask |= getmask(i);
					i += ExtractConstraintInfo(i, constraintMask, aac);
					aac.partid[0] = pentNew->m_parts[ipart].id;
					pentNew->Action(&aac);
				}
				else if (m_pConstraints[i].pent[1] == pentOrig &&
				         (ipart = pentNew->TouchesSphere(m_pConstraints[i].pt[1],
				                                         m_pConstraintInfos[i].sensorRadius)) >= 0)
				{
					ReportConstraintBreak(epjb, i).pNewEntity[1] = pentNew;
					m_pWorld->OnEvent(2, &epjb);
					for (j = i, cmask = 0; j < NMASKBITS && constraintMask & getmask(j) &&
					                       m_pConstraintInfos[j].id == m_pConstraintInfos[i].id;
					     j++)
					{
						cmask |= getmask(j);
					}
					for (j = 0; j < m_nColliders && m_pColliders[j] != m_pConstraints[i].pent[1];
					     j++)
						;
					m_pColliderConstraints[j] &= ~cmask;
					if (!m_pColliderContacts[j] && !m_pColliderConstraints[j] &&
					    !m_pConstraints[i].pent[1]->HasContactsWith(this))
					{
						m_pConstraints[i].pent[1]->RemoveCollider(this);
						RemoveCollider(m_pConstraints[i].pent[1]);
					}
					m_pColliderConstraints[AddCollider(pentNew)] |= cmask;
					pentNew->AddCollider(this);
					for (j = i; j < NMASKBITS && constraintMask & getmask(j) &&
					            m_pConstraintInfos[j].id == m_pConstraintInfos[i].id;
					     j++)
					{
						m_pConstraints[j].pent[1] = pentNew;
						m_pConstraints[j].ipart[1] = ipart;
						quaternionf q0 = m_pConstraints[j].pbody[1]->q;
						m_pConstraints[j].pbody[1] = pentNew->GetRigidBody(
						    ipart,
						    iszero((int)m_pConstraintInfos[j].flags & constraint_inactive));
						m_pConstraints[j].ptloc[1] = Glob2Loc(m_pConstraints[j], 1);
						//! m_pConstraints[j].pbody[1]->q*(m_pConstraints[j].pt[1]-m_pConstraints[j].pbody[1]->pos);
						m_pConstraintInfos[j].qframe_rel[1] =
						    !m_pConstraints[j].pbody[1]->q * q0 *
						    m_pConstraintInfos[j].qframe_rel[1];
					}
					i = j;
				}
				else
				{
					for (j = m_pConstraintInfos[i].id;
					     i < NMASKBITS && getmask(i) <= constraintMask &&
					     m_pConstraintInfos[i].id == j;
					     i++)
						;
				}
			}
			else
			{
				i++;
			}
		}
	}
	else
	{
		for (i = 0; i < NMASKBITS && getmask(i) <= constraintMask; i++)
		{
			if (constraintMask & getmask(i) && !(m_pConstraintInfos[i].flags & constraint_rope))
			{
				if (m_pConstraints[i].pent[0] == pentOrig || m_pConstraints[i].pent[1] == pentOrig)
				{
					j = m_pConstraints[i].pent[1] == pentOrig;
					for (i1 = i; i1 < NMASKBITS && getmask(i1) <= constraintMask &&
					             m_pConstraintInfos[i1].id == m_pConstraintInfos[i].id;
					     i1++)
						;
					if ((ipart = pentOrig->TouchesSphere(m_pConstraints[i].pt[0],
					                                     m_pConstraintInfos[i].sensorRadius)) >= 0)
					{
						quaternionf q0 = m_pConstraintInfos[i].qprev[j];
						for (; i < i1; i++)
						{
							m_pConstraints[i].ipart[j] = ipart;
							m_pConstraints[i].pbody[j] = pentOrig->GetRigidBody(
							    ipart, iszero((int)m_pConstraintInfos[i].flags &
							                  constraint_inactive));
							m_pConstraints[i].ptloc[j] = Glob2Loc(m_pConstraints[i], j);
							m_pConstraintInfos[i].qframe_rel[j] =
							    !m_pConstraints[i].pbody[j]->q * q0 *
							    m_pConstraintInfos[i].qframe_rel[j];
							if (j == 0)
							{
								m_pConstraints[i].nloc =
								    !m_pConstraints[i].pbody[0]->q *
								    (q0 * m_pConstraints[i].nloc);
							}
						}
					}
					else
					{
						ReportConstraintBreak(epjb, i).pNewEntity[j] = 0;
						m_pWorld->OnEvent(2, &epjb);
						removeMask |= getmask(i);
					}
					i = i1 - 1;
				}
			}
		}
	}

	for (i = NMASKBITS - 1; i >= 0; i--)
	{
		if (removeMask & getmask(i))
		{
			auc.idConstraint = m_pConstraintInfos[i].id;
			m_pConstraints[i].pent[0]->Action(&auc);
		}
	}

	if (pentNew)
	{
		Awake(1, 5);
	}

	EventPhysEnvChange epec;
	epec.pEntity = this;
	epec.pForeignData = m_pForeignData;
	epec.iForeignData = m_iForeignData;
	epec.iCode = EventPhysEnvChange::EntStructureChange;
	epec.pentSrc = pentOrig;
	epec.pentNew = pentNew;
	m_pWorld->OnEvent(m_flags, &epec);
}

void CRigidEntity::DrawHelperInformation(IPhysRenderer* pRenderer, int flags)
{
	CPhysicalEntity::DrawHelperInformation(pRenderer, flags);

	if (flags & pe_helper_collisions)
	{
		ReadLock lock(m_lockContacts);
		for (entity_contact* pContact = m_pContactStart; pContact != CONTACT_END(m_pContactStart);
		     pContact = pContact->next)
		{
			pRenderer->DrawLine(pContact->pt[0],
			                    pContact->pt[0] + pContact->n * m_pWorld->m_vars.maxContactGap * 30,
			                    m_iSimClass);
		}
	}
}
