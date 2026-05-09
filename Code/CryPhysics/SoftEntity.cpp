#include "CryCommon/CrySystem/ISystem.h"

#include "ChangeRequest.h"
#include "IntersectionChecker.h"
#include "OverlapChecker.h"
#include "PhysicalWorld.h"
#include "RayGeom.h"
#include "RigidBody.h"
#include "SingleBoxTree.h"
#include "SoftEntity.h"
#include "TriMesh.h"

namespace {

int GetCheckPart(CPhysicalEntity* pent, int ipart)
{
	int i, j;
	if (pent->m_nParts <= 24)
	{
		i = (pent->m_bProcessed_aux & 0xFFFFFF) & ((1 << ipart) - 1);
		return (pent->m_bProcessed_aux >> 24) + g_bitcount[i & 0xFF] + g_bitcount[i >> 8 & 0xFF] +
		       g_bitcount[i >> 16];
	}
	else
	{
		for (i = j = 0; i < ipart; i++)
		{
			j += iszero((int)pent->m_parts[i].flags & geom_removed) ^ 1;
		}
		return (pent->m_bProcessed_aux >> 24) + j;
	}
}

} // unnamed namespace

struct se_edge
{
	int ivtx[2];
	float len0;
	float len, rlen;
	float kd;
	float kmass;
};

struct se_vertex
{
	~se_vertex()
	{
		if (pContactEnt)
		{
			pContactEnt->Release();
		}
	}
	Vec3 pos, vel;
	Vec3 pos0, posorg, vel0;
	float massinv;
	float volume;
	Vec3 n, ncontact;
	int bSeparating;
	int iSorted, iSorted0;
	int idx;
	float area;
	int iStartEdge, iEndEdge, bFullFan;
	float rnEdges;
	CPhysicalEntity* pContactEnt;
	int iContactPart;
	int iContactNode;
	Vec3 vcontact;
	int surface_idx[2];
	int bAttached;
	Vec3 ptAttach;
	Vec3 P, dv, r, d;
};

CSoftEntity::CSoftEntity(CPhysicalWorld* pWorld) : CPhysicalEntity(pWorld)
{
	m_vtx = 0;
	m_edges = 0;
	m_pVtxEdges = 0;
	m_nVtx = m_nEdges = 0;
	m_maxAllowedStep = 0.1f;
	m_ks = 200.0f;
	m_thickness = 0.04f;
	m_maxSafeStep = 0.2f;
	m_kdRatio = 0.9f;
	m_airResistance = 0; // 10.0f;
	m_wind0 = m_wind1 = m_wind.zero();
	m_waterResistance = 0;
	m_gravity.Set(0, 0, -9.8f);
	m_damping = 0;
	m_Emin = sqr(0.01f);
	m_iSimClass = 4;
	m_accuracy = 0.01f;
	m_nMaxIters = 1024;
	m_prevTimeInterval = 0;
	m_bAwake = 1;
	m_nSlowFrames = 0;
	m_friction = 0;
	m_impulseScale = 0.05f;
	m_explosionScale = 0.001f;
	m_qrot0.SetIdentity();
	m_collImpulseScale = 1.0f;
	m_bMeshUpdated = 0;
	m_collTypes = ent_static | ent_sleeping_rigid | ent_rigid | ent_living;
	m_maxCollImpulse = 3000;
	m_windTimer = 0;
	m_windVariance = 0.2f;
	m_lockSoftBody = 0;
	m_flags |= se_skip_longest_edges;
	m_nConnectedVtx = 0;
	m_massDecay = 0;
}

CSoftEntity::~CSoftEntity()
{
	AlertNeighbourhoodND();
}

void CSoftEntity::AlertNeighbourhoodND()
{
	if (m_vtx)
	{
		delete[] m_vtx;
		m_vtx = 0;
	}
	if (m_edges)
	{
		delete[] m_edges;
		m_edges = 0;
	}
	if (m_pVtxEdges)
	{
		delete[] m_pVtxEdges;
		m_pVtxEdges = 0;
	}
	m_nVtx = m_nEdges = 0;
}

int CSoftEntity::AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id, int bThreadSafe)
{
	if (!pgeom || pgeom->pGeom->GetType() != GEOM_TRIMESH || m_nParts > 0)
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
	if (params->mass == 0 && params->density == 0)
	{
		params->mass = 1;
	}

	int res = CPhysicalEntity::AddGeometry(pgeom, params, id, 1);
	WriteLock lock(m_lockUpdate);
	int i, j, i0, i1, bDegen, iedge, itri, itri0, ivtx, itrinew, nVtxEdges, (*pInfo)[3];
	float rvtxmass, vtxvol, len[3];

	CTriMesh* pMesh = (CTriMesh*)pgeom->pGeom;
	rvtxmass = pMesh->m_nVertices / params->mass;
	vtxvol = 1.0f / (rvtxmass * params->density);
	m_density = params->density;

	m_vtx = new se_vertex[m_nVtx = pMesh->m_nVertices];
	for (i = 0; i < m_nVtx; i++)
	{
		m_vtx[i].posorg = pMesh->m_pVertices[i];
		m_vtx[i].pos = m_qrot0 * (params->q * pMesh->m_pVertices[i] * params->scale + params->pos);
		m_vtx[i].massinv = rvtxmass;
		m_vtx[i].volume = vtxvol;
		m_vtx[i].vel.zero();
		m_vtx[i].bSeparating = m_vtx[i].iSorted = m_vtx[i].bAttached = 0;
		m_vtx[i].pContactEnt = 0;
		m_vtx[i].iContactNode = 0;
		m_vtx[i].ncontact.zero();
		m_vtx[i].n.zero();
		m_vtx[i].iStartEdge = 0;
		m_vtx[i].iEndEdge = -1;
	}
	m_offs0 = m_vtx[0].pos;
	for (i = 1; i < m_nVtx; i++)
	{
		m_vtx[i].pos -= m_vtx[0].pos;
	}
	m_vtx[0].pos.zero();
	memset(pInfo = new int[pMesh->m_nTris][3], -1, pMesh->m_nTris * sizeof(pInfo[0]));

	// count the number of edges - for each tri, mark each edge, unless buddy already did it
	for (i = m_nEdges = 0; i < pMesh->m_nTris; i++)
	{
		for (j = bDegen = 0; j < 3; j++)
		{
			len[j] = (pMesh->m_pVertices[pMesh->m_pIndices[(i * 3) + j]] -
			          pMesh->m_pVertices[pMesh->m_pIndices[(i * 3) + inc_mod3[j]]])
			             .len2();
			bDegen |= iszero(len[j]);
		}
		iedge = idxmax3(len);
		j = iedge & -bDegen;
		do
		{
			if (pInfo[i][j] < 0 && !(m_flags & se_skip_longest_edges && j == iedge && !bDegen))
			{
				if (pMesh->m_pTopology[i].ibuddy[j] >= 0)
				{
					pInfo[pMesh->m_pTopology[i].ibuddy[j]]
					     [pMesh->GetEdgeByBuddy(pMesh->m_pTopology[i].ibuddy[j], i)] = m_nEdges;
				}
				pInfo[i][j] = m_nEdges++;
			}
		}
		while (++j < 3 && !bDegen);
	}
	m_edges = new se_edge[m_nEdges];
	m_pVtxEdges = new int[m_nEdges * 2];
	for (i = 0; i < pMesh->m_nTris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			if ((iedge = pInfo[i][j]) >= 0)
			{
				i0 = m_edges[iedge].ivtx[0] = pMesh->m_pIndices[(i * 3) + j];
				i1 = m_edges[iedge].ivtx[1] = pMesh->m_pIndices[(i * 3) + inc_mod3[j]];
				m_edges[iedge].len0 =
				    (pMesh->m_pVertices[i0] - pMesh->m_pVertices[i1]).len() * params->scale;
				m_edges[pInfo[i][j]].kd =
				    m_vtx[i0].massinv + m_vtx[i1].massinv > 0
					? sqrt_tpl(m_ks / (m_vtx[i0].massinv + m_vtx[i1].massinv)) * 2.0f * m_kdRatio
					: 0;
			}
		}
	}

	// for each vertex, trace ccw fan around it and store in m_pVtxEdges
	m_BBox[0].zero();
	m_BBox[1].zero();
	for (i = nVtxEdges = 0; i < pMesh->m_nTris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			if (!m_vtx[ivtx = pMesh->m_pIndices[(i * 3) + j]].iSorted)
			{
				itri = i;
				iedge = j;
				m_vtx[ivtx].iStartEdge = nVtxEdges;
				m_vtx[ivtx].bFullFan = 1;
				do
				{ // first, trace cw fan until we find an open edge (if any)
					if ((itrinew = pMesh->m_pTopology[itri].ibuddy[iedge]) <= 0)
					{
						break;
					}
					iedge = inc_mod3[pMesh->GetEdgeByBuddy(itrinew, itri)];
				}
				while ((itri = itrinew) != i);
				itri0 = itri;
				do
				{ // now trace ccw fan
					if (pInfo[itri][iedge] >= 0)
					{
						m_pVtxEdges[nVtxEdges++] = pInfo[itri][iedge];
					}
					if ((itrinew = pMesh->m_pTopology[itri].ibuddy[dec_mod3[iedge]]) < 0)
					{
						if (pInfo[itri][dec_mod3[iedge]] >= 0)
						{
							m_pVtxEdges[nVtxEdges++] = pInfo[itri][dec_mod3[iedge]];
						}
						m_vtx[ivtx].bFullFan = 0;
						break;
					}
					iedge = pMesh->GetEdgeByBuddy(itrinew, itri);
				}
				while ((itri = itrinew) != itri0);
				m_vtx[ivtx].iEndEdge = nVtxEdges - 1;
				m_vtx[ivtx].rnEdges = 1.0f / (nVtxEdges - m_vtx[ivtx].iStartEdge);
				m_vtx[ivtx].iSorted = 1;
				m_vtx[ivtx].surface_idx[0] = pMesh->m_pIds ? pMesh->m_pIds[i] : -1;

				m_BBox[0] = min(m_BBox[0], m_vtx[ivtx].pos);
				m_BBox[1] = max(m_BBox[1], m_vtx[ivtx].pos);
			}
		}
	}
	delete[] pInfo;
	m_coverage = m_flags & se_skip_longest_edges ? 0.5f : 1.0f / 3;
	m_BBox[0] += m_pos + m_offs0;
	m_BBox[1] += m_pos + m_offs0;

	primitives::box bbox;
	bbox.Basis.SetIdentity();
	bbox.bOriented = 0;
	bbox.center = (m_BBox[0] + m_BBox[1]) * 0.5f - m_pos - m_parts[0].pos;
	bbox.size = (m_BBox[1] - m_BBox[0]) * (0.5f / params->scale);
	if (pMesh->m_pTree)
	{
		delete pMesh->m_pTree;
	}
	CSingleBoxTree* pTree = new CSingleBoxTree;
	pTree->SetBox(&bbox);
	pTree->Build(pMesh);
	pTree->m_nPrims = pMesh->m_nTris;
	pMesh->m_pTree = pTree;
	m_flags |= pef_use_geom_callbacks;
	m_bMeshUpdated = 0;

	return res;
}

void CSoftEntity::RemoveGeometry(int id, int bThreadSafe)
{
	ChangeRequest<void> req(this, m_pWorld, 0, bThreadSafe, 0, id);
	if (req.IsQueued())
	{
		return;
	}

	int i;
	for (i = 0; i < m_nParts && m_parts[i].id != id; i++)
		;
	if (i == m_nParts)
	{
		return;
	}

	if (m_vtx)
	{
		delete[] m_vtx;
		m_vtx = 0;
	}
	if (m_edges)
	{
		delete[] m_edges;
		m_edges = 0;
	}
	if (m_pVtxEdges)
	{
		delete[] m_pVtxEdges;
		m_pVtxEdges = 0;
	}
	m_nVtx = m_nEdges = 0;

	CPhysicalEntity::RemoveGeometry(id, 1);
}

int CSoftEntity::SetParams(pe_params* _params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this, m_pWorld, _params, bThreadSafe);
	if (req.IsQueued())
	{
		return 1;
	}

	int res = CPhysicalEntity::SetParams(_params, 1);
	if (res)
	{
		if (_params->type == pe_params_pos::type_id && !is_unused(((pe_params_pos*)_params)->q))
		{
			if (m_nVtx > 0 && (m_qrot0 | m_qrot) < 0.998f)
			{
				CTriMesh* pMesh = (CTriMesh*)m_parts[0].pPhysGeom->pGeom;
				int i;
				quaternionf qrot = m_bMeshUpdated ? m_qrot * !m_qrot0 : m_qrot;
				for (i = 0; i < m_nVtx; i++)
				{
					m_vtx[i].pos = qrot * (m_parts[0].q * pMesh->m_pVertices[i] * m_parts[0].scale +
					                       m_parts[0].pos);
					if (m_vtx[i].bAttached == 2 && m_vtx[i].pContactEnt)
					{
						RigidBody* pbody =
						    m_vtx[i].pContactEnt->GetRigidBody(m_vtx[i].iContactPart);
						m_vtx[i].ptAttach = (m_vtx[i].pos + m_pos - pbody->pos) * pbody->q;
					}
				}
				m_offs0 = m_vtx[0].pos;
				for (i = 1; i < m_nVtx; i++)
				{
					m_vtx[i].pos -= m_vtx[0].pos;
				}
				m_vtx[0].pos.zero();
			}
			m_qrot0 = m_qrot;
			m_qrot.SetIdentity();
		}
		return res;
	}

	if (_params->type == pe_simulation_params::type_id)
	{
		pe_simulation_params* params = (pe_simulation_params*)_params;
		if (!is_unused(params->gravity))
		{
			m_gravity = params->gravity;
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
		if (!is_unused(params->density) && params->density >= 0 && m_nParts > 0)
		{
			for (int i = 0; i < m_nVtx; i++)
			{
				if (m_vtx[i].massinv > 0)
				{
					m_vtx[i].volume = 1.0f / (m_vtx[i].massinv * params->density);
				}
			}
			m_density = params->density;
		}
		if (!is_unused(params->mass) && params->mass >= 0 && m_nParts > 0)
		{
			int i;
			float dmass = m_parts[0].mass / params->mass;
			for (i = 0; i < m_nVtx; i++)
			{
				m_vtx[i].massinv *= dmass;
			}
			for (i = 0; i < m_nEdges; i++)
			{
				m_edges[i].kmass = 1.0f / max(1E-10f, m_vtx[m_edges[i].ivtx[0]].massinv +
				                                          m_vtx[m_edges[i].ivtx[1]].massinv);
			}
			m_parts[0].mass = params->mass;
		}
		if (!is_unused(params->iSimClass))
		{
			m_iSimClass = params->iSimClass;
			m_pWorld->RepositionEntity(this, 2);
		}
		return 1;
	}

	if (_params->type == pe_params_softbody::type_id)
	{
		pe_params_softbody* params = (pe_params_softbody*)_params;
		WriteLock lock(m_lockSoftBody);
		if (!is_unused(params->thickness))
		{
			m_thickness = params->thickness;
		}
		if (!is_unused(params->friction))
		{
			m_friction = params->friction;
		}
		if (!is_unused(params->ks))
		{
			m_ks = params->ks;
		}
		if (!is_unused(params->kdRatio))
		{
			m_kdRatio = params->kdRatio;
		}
		if (!is_unused(params->ks) || !is_unused(params->kdRatio))
		{
			for (int i = 0; i < m_nEdges; i++)
			{
				if (m_vtx[m_edges[i].ivtx[0]].massinv + m_vtx[m_edges[i].ivtx[1]].massinv > 0)
				{
					m_edges[i].kd = sqrt_tpl(m_ks / (m_vtx[m_edges[i].ivtx[0]].massinv +
					                                 m_vtx[m_edges[i].ivtx[1]].massinv)) *
					                2.0f * m_kdRatio;
				}
			}
		}
		if (!is_unused(params->airResistance))
		{
			m_airResistance = params->airResistance;
		}
		if (!is_unused(params->waterResistance))
		{
			m_waterResistance = params->waterResistance;
		}
		if (!is_unused(params->wind))
		{
			if ((m_wind0 = m_wind1 = m_wind = params->wind).len2() > 0)
			{
				Awake();
			}
		}
		if (!is_unused(params->windVariance))
		{
			m_windVariance = params->windVariance;
		}
		if (!is_unused(params->accuracy))
		{
			m_accuracy = params->accuracy;
		}
		if (!is_unused(params->nMaxIters))
		{
			m_nMaxIters = params->nMaxIters;
		}
		if (!is_unused(params->maxSafeStep))
		{
			m_maxSafeStep = params->maxSafeStep;
		}
		if (!is_unused(params->impulseScale))
		{
			m_impulseScale = params->impulseScale;
		}
		if (!is_unused(params->explosionScale))
		{
			m_explosionScale = params->explosionScale;
		}
		if (!is_unused(params->collisionImpulseScale))
		{
			m_collImpulseScale = params->collisionImpulseScale;
		}
		if (!is_unused(params->maxCollisionImpulse))
		{
			m_maxCollImpulse = params->maxCollisionImpulse;
		}
		if (!is_unused(params->collTypes))
		{
			m_collTypes = params->collTypes;
		}
		if (!is_unused(params->massDecay))
		{
			m_massDecay = params->massDecay;
		}
		return 1;
	}

	return 0;
}

int CSoftEntity::GetParams(pe_params* _params)
{
	if (_params->type == pe_simulation_params::type_id)
	{
		pe_simulation_params* params = (pe_simulation_params*)_params;
		params->gravity = params->gravityFreefall = m_gravity;
		params->maxTimeStep = m_maxAllowedStep;
		params->minEnergy = m_Emin;
		params->damping = params->dampingFreefall = m_damping;
		params->mass = m_parts[0].mass;
		params->density = m_density;
		params->iSimClass = m_iSimClass;
		return 1;
	}

	if (_params->type == pe_params_softbody::type_id)
	{
		pe_params_softbody* params = (pe_params_softbody*)_params;
		ReadLock lock(m_lockSoftBody);
		params->thickness = m_thickness;
		params->friction = m_friction;
		params->ks = m_ks;
		params->kdRatio = m_kdRatio;
		params->airResistance = m_airResistance;
		params->waterResistance = m_waterResistance;
		params->wind = m_wind;
		params->windVariance = m_windVariance;
		params->accuracy = m_accuracy;
		params->nMaxIters = m_nMaxIters;
		params->maxSafeStep = m_maxSafeStep;
		params->impulseScale = m_impulseScale;
		params->explosionScale = m_explosionScale;
		params->collisionImpulseScale = m_collImpulseScale;
		params->maxCollisionImpulse = m_maxCollImpulse;
		params->collTypes = m_collTypes;
		params->massDecay = m_massDecay;
		return 1;
	}

	return CPhysicalEntity::GetParams(_params);
}

int CSoftEntity::GetStatus(pe_status* _status)
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

	if (_status->type == pe_status_softvtx::type_id)
	{
		if (m_nVtx <= 0)
		{
			return 0;
		}
		pe_status_softvtx* status = (pe_status_softvtx*)_status;
		if (status->flags & eSSV_LockPos)
		{
			++m_lockUpdate;
			for (; *((char*)&m_lockUpdate + 2);)
				;
		}
		if (status->flags & eSSV_UnlockPos)
		{
			--m_lockUpdate;
		}
		status->nVtx = m_nVtx;
		status->pVtx = ((CTriMesh*)m_parts[0].pPhysGeomProxy->pGeom)->m_pVertices;
		status->pNormals.data = &m_vtx[0].n;
		status->pNormals.iStride = sizeof(m_vtx[0]);
		status->pVtxMap = ((CTriMesh*)m_parts[0].pPhysGeom->pGeom)->m_pVtxMap;
		return 1;
	}

	if (_status->type == pe_status_collisions::type_id)
	{
		pe_status_collisions* status = (pe_status_collisions*)_status;
		int i, j, n;
		if (status->len <= 0)
		{
			return 0;
		}

		for (i = n = 0; i < m_nVtx; i++)
		{
			if (!m_vtx[i].bAttached && m_vtx[i].pContactEnt)
			{
				if (n == status->len)
				{
					for (n = 1, j = 0; n < status->len; n++)
					{
						if ((status->pHistory[n].v[0] - status->pHistory[n].v[1]).len2() <
						    (status->pHistory[j].v[0] - status->pHistory[j].v[1]).len2())
						{
							j = n;
						}
					}
					if ((status->pHistory[j].v[0] - status->pHistory[j].v[1]).len2() >
					    (m_vtx[i].vel - m_vtx[i].vcontact).len2())
					{
						continue;
					}
				}
				else
				{
					j = n++;
				}
				status->pHistory[j].pt = m_vtx[i].pos;
				status->pHistory[j].n = m_vtx[i].ncontact;
				status->pHistory[j].v[0] = m_vtx[i].vel;
				status->pHistory[j].v[1] = m_vtx[i].vcontact;
				status->pHistory[j].mass[0] = m_parts[0].mass;
				status->pHistory[j].mass[1] = m_vtx[i].pContactEnt->GetMassInv();
				if (status->pHistory[j].mass[1] > 0)
				{
					status->pHistory[j].mass[1] = 1.0f / status->pHistory[j].mass[1];
				}
				status->pHistory[j].age = 0;
				status->pHistory[j].idCollider = m_pWorld->GetPhysicalEntityId(m_vtx[i].pContactEnt);
				status->pHistory[j].partid[0] = 0;
				status->pHistory[j].partid[1] = m_vtx[i].iContactPart;
				status->pHistory[j].idmat[0] = m_vtx[i].surface_idx[0];
				status->pHistory[j].idmat[1] = m_vtx[i].surface_idx[1];
			}
		}

		return status->len = n;
	}

	return 0;
}

int CSoftEntity::Action(pe_action* _action, int bThreadSafe)
{
	ChangeRequest<pe_action> req(this, m_pWorld, _action, bThreadSafe);
	if (req.IsQueued())
	{
		return 1;
	}

	if (_action->type == pe_action_impulse::type_id)
	{
		pe_action_impulse* action = (pe_action_impulse*)_action;

		if (m_nVtx > 0 && !is_unused(action->point) && !is_unused(action->impulse))
		{
			CTriMesh* pMesh = (CTriMesh*)m_parts[0].pPhysGeom->pGeom;
			Vec3 pt = action->point - (m_pos + m_offs0), impulse = action->impulse * m_impulseScale;
			int i, j, bBest;
			if (!is_unused(action->ipart))
			{
				i = action->ipart;
			}
			else
			{
				i = action->partid;
			}
			m_bAwake = 1;

			if ((unsigned int)i < (unsigned int)pMesh->m_nTris)
			{
				float rarea, k;
				int idx[3];
				for (j = 0; j < 3; j++)
				{
					idx[j] = pMesh->m_pIndices[(i * 3) + j];
				}
				rarea = (m_vtx[idx[1]].pos - m_vtx[idx[0]].pos ^ m_vtx[idx[2]].pos - m_vtx[idx[0]].pos)
				            .len();
				if (rarea > 1E-4f)
				{
					rarea = 1.0f / rarea;
					for (j = 0; j < 3; j++)
					{
						k = (m_vtx[idx[inc_mod3[j]]].pos - pt ^ m_vtx[idx[j]].pos - pt).len() *
						    rarea;
						m_vtx[idx[dec_mod3[j]]].vel +=
						    impulse * (m_vtx[idx[dec_mod3[j]]].massinv * k);
					}
				}
				else
				{
					m_vtx[idx[0]].vel += impulse * m_vtx[idx[0]].massinv;
				}
			}
			else
			{
				for (i = 1, j = 0; i < m_nVtx; i++)
				{
					bBest = -isneg((m_vtx[i].pos - pt).len2() - (m_vtx[j].pos - pt).len2());
					j = (i & bBest) | (j & ~bBest);
				}
				m_vtx[j].vel += impulse * m_vtx[j].massinv;
			}
		}
		return 1;
	}

	if (_action->type == pe_action_attach_points::type_id)
	{
		pe_action_attach_points* action = (pe_action_attach_points*)_action;
		CPhysicalEntity* pent = action->pEntity == WORLD_ENTITY ? &g_StaticPhysicalEntity
		                        : action->pEntity ? ((CPhysicalPlaceholder*)action->pEntity)->GetEntity()
		                                          : 0;
		int i, ipart = 0, bAttached = iszero((intptr_t)pent) ^ 1;
		if (bAttached && is_unused(action->points))
		{
			bAttached = 2;
		}
		float rvtxmass = pent ? 0 : m_nVtx / m_parts[0].mass;

		if (!pent && (is_unused(action->piVtx) || !action->piVtx))
		{
			for (i = 0; i < m_nVtx; i++)
			{
				if (m_vtx[i].bAttached && m_vtx[i].pContactEnt)
				{
					m_vtx[i].pContactEnt->Release();
					m_vtx[i].pContactEnt = 0;
				}
			}
			return 1;
		}

		if (!is_unused(action->partid))
		{
			for (ipart = 0; ipart < pent->m_nParts && pent->m_parts[ipart].id != action->partid; ipart++)
				;
			if (ipart >= pent->m_nParts)
			{
				return 0;
			}
		}
		if (pent && pent->m_iSimClass == 4)
		{
			(m_collTypes &= ent_living) |= ent_independent;
		}
		RigidBody* pbody = nullptr;
		if (bAttached)
		{
			pbody = pent->GetRigidBody(ipart);
		}

		for (i = 0; i < action->nPoints; i++)
		{
			if (m_vtx[action->piVtx[i]].pContactEnt)
			{
				m_vtx[action->piVtx[i]].pContactEnt->Release();
			}
			m_vtx[action->piVtx[i]].pContactEnt = pent;
			if (pent)
			{
				pent->AddRef();
			}
			m_vtx[action->piVtx[i]].massinv = rvtxmass;
			m_vtx[action->piVtx[i]].iContactPart = ipart;
			m_vtx[action->piVtx[i]].bAttached = bAttached;
			if (bAttached)
			{
				if (!is_unused(action->points))
				{
					m_vtx[action->piVtx[i]].ptAttach = action->points[i];
				}
				else
				{
					m_vtx[action->piVtx[i]].ptAttach =
					    m_vtx[action->piVtx[i]].pos + m_pos + m_offs0;
				}
				m_vtx[action->piVtx[i]].ptAttach =
				    (m_vtx[action->piVtx[i]].ptAttach - pbody->pos) * pbody->q;
			}
		}

		int ihead, itail, j, i1, imaxLvl = 0;
		for (i = ihead = 0; i < m_nVtx; i++)
		{
			if (m_vtx[i].bAttached)
			{
				m_vtx[ihead++].idx = i, m_vtx[i].iSorted = 0;
			}
			else
			{
				m_vtx[i].iSorted = -1;
			}
		}

		for (itail = 0; itail != ihead;)
		{
			for (j = m_vtx[i = m_vtx[itail++].idx].iStartEdge; j <= m_vtx[i].iEndEdge; j++)
			{
				if (m_vtx[i1 = m_edges[m_pVtxEdges[j]].ivtx[0] + m_edges[m_pVtxEdges[j]].ivtx[1] - i]
				        .iSorted < 0)
				{
					m_vtx[ihead++].idx = i1,
					imaxLvl = max(imaxLvl, m_vtx[i1].iSorted = m_vtx[i].iSorted + 1);
				}
			}
		}
		m_nConnectedVtx = ihead;

		if (m_massDecay > 0)
		{
			rvtxmass = m_nVtx / m_parts[0].mass;
			float kmass = (m_massDecay + 1) / (float)(imaxLvl + 1);
			for (i = 0; i < m_nVtx; i++)
			{
				m_vtx[i].massinv =
				    rvtxmass * -((m_vtx[i].bAttached - 1) >> 31) * (1.0f + m_vtx[i].iSorted * kmass);
			}
		}

		for (i = 0; i < m_nEdges; i++)
		{
			m_edges[i].kmass =
			    1.0f / max(1E-10f, m_vtx[m_edges[i].ivtx[0]].massinv + m_vtx[m_edges[i].ivtx[1]].massinv);
		}

		return 1;
	}

	if (_action->type == pe_action_reset::type_id)
	{
		if (m_nVtx > 0)
		{
			CTriMesh* pMesh = (CTriMesh*)m_parts[0].pPhysGeomProxy->pGeom;
			int i;
			for (i = 0; i < m_nVtx; i++)
			{
				if (m_vtx[i].pContactEnt)
				{
					m_vtx[i].pContactEnt->Release();
				}
				m_vtx[i].pContactEnt = 0;
				m_vtx[i].vel.zero();
				pMesh->m_pVertices[i] = m_vtx[i].posorg;
			}
			for (i = 0; i < pMesh->m_nTris; i++)
			{
				pMesh->m_pNormals[i] = (pMesh->m_pVertices[pMesh->m_pIndices[(i * 3) + 1]] -
				                            pMesh->m_pVertices[pMesh->m_pIndices[i * 3]] ^
				                        pMesh->m_pVertices[pMesh->m_pIndices[(i * 3) + 2]] -
				                            pMesh->m_pVertices[pMesh->m_pIndices[i * 3]])
				                           .normalized();
			}
		}
		return 1;
	}

	return CPhysicalEntity::Action(_action, 1);
}

void CSoftEntity::StartStep(float time_interval)
{
	m_timeStepPerformed = 0;
	m_timeStepFull = time_interval;
}

float CSoftEntity::GetMaxTimeStep(float time_interval)
{
	if (m_timeStepPerformed > m_timeStepFull - 0.001f)
	{
		return time_interval;
	}
	return min(min(m_timeStepFull - m_timeStepPerformed, m_maxAllowedStep), time_interval);
}

int CSoftEntity::Step(float time_interval)
{
	if (m_nVtx <= 0 || !m_bAwake)
	{
		return 1;
	}

	if (m_flags & pef_invisible)
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
		return 1;
	}

	int i, j, j1, nEnts, i0, i1, nContactVtx, nContactVtx0, nCheckParts, iter, imask, bPosEnforcement,
	    iFullIter = 4, bGridLocked = 0; //,bUnstable
	Vec3 d, l, llTd, v, F, w, center, BBox0[2], BBox[2];
	float l0, rmax, vreq, windage, kr, rsep, friction = max(0.5f, 1.0f - (m_friction * time_interval)), ks = m_ks,
						 kdScale = 1.0f; //,rl
	// real r2,r2new,a,b,dAd;
	struct check_part
	{
		Vec3 offset;
		Matrix33 R;
		float scale, rscale;
		primitives::box bbox;
		CPhysicalEntity* pent;
		int ipart;
		RigidBody* pbody;
		CGeometry* pGeom;
		int bPrimitive;
		int surface_idx;
		Vec3 P, L;
	};
	primitives::box boxent;
	check_part checkParts[20];
	CRayGeom aray;
	intersection_params ip;
	geom_contact* pcontacts;
	CPhysicalEntity **pentlist, *pent;
	geom_world_data gwd;
	RigidBody* pbody;
	COverlapChecker Overlapper;

	Overlapper.Init();
	boxent.size = (m_BBox[1] - m_BBox[0]) * 0.5f + Vec3(m_thickness, m_thickness, m_thickness) * 2;
	center = (m_BBox[0] + m_BBox[1]) * 0.5f;
	boxent.bOriented = 1;
	ip.bStopAtFirstTri = true;

	for (i = 0; i < m_nVtx; i++)
	{
		if (m_vtx[i].pContactEnt && m_vtx[i].pContactEnt->m_iSimClass == 7)
		{
			m_vtx[i].pContactEnt = 0;
			m_vtx[i].bAttached = 0;
		}
	}
	if (m_timeStepPerformed > m_timeStepFull - 0.001f)
	{
		return 1;
	}
	m_timeStepPerformed += time_interval;

	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	nEnts =
	    m_pWorld->GetEntitiesAround(m_BBox[0] - Vec3(m_thickness, m_thickness, m_thickness) * 2,
	                                m_BBox[1] + Vec3(m_thickness, m_thickness, m_thickness) * 2, pentlist,
	                                m_collTypes | ent_sort_by_mass | ent_ignore_noncolliding | ent_triggers, this);

	for (i = j = nCheckParts = 0; i < nEnts; i++)
	{
		if (pentlist[i] != this)
		{
			for (j = 0, pentlist[i]->m_bProcessed_aux = nCheckParts << 24; j < pentlist[i]->m_nParts; j++)
			{
				if (pentlist[i]->m_parts[j].flags & m_parts[0].flagsCollider)
				{
					checkParts[nCheckParts].offset =
					    pentlist[i]->m_pos + pentlist[i]->m_qrot * pentlist[i]->m_parts[j].pos;
					boxent.Basis = Matrix33(pentlist[i]->m_qrot * pentlist[i]->m_parts[j].q);
					boxent.center = (center - checkParts[nCheckParts].offset) * boxent.Basis;
					if (pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom->GetType() !=
					    GEOM_HEIGHTFIELD)
					{
						pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom->GetBBox(
						    &checkParts[nCheckParts].bbox);
						checkParts[nCheckParts].bbox.center *= pentlist[i]->m_parts[j].scale;
						checkParts[nCheckParts].bbox.size *= pentlist[i]->m_parts[j].scale;
					}
					else
					{
						checkParts[nCheckParts].bbox = boxent;
					}
					boxent.bOriented++;
					if (box_box_overlap_check(&boxent, &checkParts[nCheckParts].bbox, &Overlapper))
					{
						checkParts[nCheckParts].pent = pentlist[i];
						checkParts[nCheckParts].ipart = j;
						checkParts[nCheckParts].R = boxent.Basis;
						pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom->PrepareForRayTest(
						    pentlist[i]->m_parts[j].scale == 1.0f
							? m_thickness * 2
							: m_thickness * 2 / pentlist[i]->m_parts[j].scale);
						checkParts[nCheckParts].offset -= m_pos + m_offs0;
						checkParts[nCheckParts].scale = pentlist[i]->m_parts[j].scale;
						checkParts[nCheckParts].rscale =
						    checkParts[nCheckParts].scale == 1.0f
							? 1.0f
							: 1.0f / checkParts[nCheckParts].scale;
						checkParts[nCheckParts].pbody = pentlist[i]->GetRigidBody(j);
						checkParts[nCheckParts].pGeom =
						    (CGeometry*)pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom;
						checkParts[nCheckParts].bPrimitive =
						    isneg(checkParts[nCheckParts].pGeom->GetPrimitiveCount() - 2);
						checkParts[nCheckParts].surface_idx =
						    pentlist[i]->m_parts[j].surface_idx;
						if (pentlist[i]->m_nParts <= 24)
						{
							pentlist[i]->m_bProcessed_aux |= 1u << j;
						}
						else
						{
							pentlist[i]->m_parts[i].flags |= geom_removed;
						}
						AtomicAdd(&pentlist[i]->m_bProcessed,
						          (pentlist[i]->m_bProcessed & 1) ^ 1);
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

	for (i0 = nContactVtx = bPosEnforcement = 0; i0 < m_nVtx; i0++)
	{
		i = m_vtx[i0].idx;
		// calculate normal
		for (j = m_vtx[i].iStartEdge, m_vtx[i].n.zero(); j < m_vtx[i].iEndEdge + m_vtx[i].bFullFan; j++)
		{
			imask = (j - m_vtx[i].iEndEdge) >> 31;
			j1 = ((j + 1) & imask) | (m_vtx[i].iStartEdge & ~imask);
			m_vtx[i].n +=
			    (m_vtx[m_edges[m_pVtxEdges[j]].ivtx[1]].pos - m_vtx[m_edges[m_pVtxEdges[j]].ivtx[0]].pos) *
				((iszero(i ^ m_edges[m_pVtxEdges[j]].ivtx[0]) * 2) - 1) ^
			    (m_vtx[m_edges[m_pVtxEdges[j1]].ivtx[1]].pos -
			     m_vtx[m_edges[m_pVtxEdges[j1]].ivtx[0]].pos) *
				((iszero(i ^ m_edges[m_pVtxEdges[j1]].ivtx[0]) * 2) - 1);
		}
		if ((m_vtx[i].area = m_vtx[i].n.len2()) > 0)
		{
			m_vtx[i].area = sqrt_tpl(m_vtx[i].area);
			m_vtx[i].n /= m_vtx[i].area;
			m_vtx[i].area *= m_coverage * 0.5f;
		}
		m_vtx[i].pos0 = m_vtx[i].pos;
		m_vtx[i].pos += m_vtx[i].vel * time_interval;

		if (!m_vtx[i].bAttached)
		{
			// detect collisions for free vertices
			rsep = m_thickness;
			pent = m_vtx[i].pContactEnt;
			if (pent)
			{
				pbody = m_vtx[i].pContactEnt->GetRigidBody(m_vtx[i].iContactPart);
				m_vtx[i].vcontact = pbody->v + (pbody->w ^ m_vtx[i].pos + m_pos + m_offs0 - pbody->pos);
				if (pent->m_bProcessed & 1 &&
				    (!m_vtx[i].bSeparating ||
				     (m_vtx[i].vel - m_vtx[i].vcontact) * m_vtx[i].ncontact < 0.1f))
				{
					if (checkParts[GetCheckPart(m_vtx[i].pContactEnt, m_vtx[i].iContactPart)]
					        .bPrimitive)
					{
						rsep = m_thickness * 1.5f;
						m_vtx[i].pContactEnt = 0;
					}
					else
					{
						j = m_vtx[i].iContactPart;
						gwd.offset =
						    pent->m_pos - m_pos - m_offs0 + pent->m_qrot * pent->m_parts[j].pos;
						gwd.R = Matrix33(pent->m_qrot * pent->m_parts[j].q);
						gwd.scale = pent->m_parts[j].scale;
						aray.m_dirn = -m_vtx[i].ncontact;
						aray.m_ray.origin = m_vtx[i].pos;
						aray.m_ray.dir = aray.m_dirn * (m_thickness * 1.5f);
						gwd.iStartNode = m_vtx[i].iContactNode;
						if (pent->m_parts[j].pPhysGeomProxy->pGeom->Intersect(&aray, &gwd, 0,
						                                                      &ip, pcontacts))
						{
							WriteLockCond lockColl(*ip.plock, 0);
							lockColl.SetActive();
							m_vtx[i].pos = pcontacts->pt + pcontacts->n * m_thickness;
							m_vtx[i].ncontact = pcontacts->n;
							m_vtx[i].iContactNode = pcontacts->iNode[0];
						}
						else
						{
							m_vtx[i].pContactEnt = 0;
						}
					}
				}
				else
				{
					m_vtx[i].pContactEnt = 0;
				}
			}

			for (j = 0; j < nCheckParts; j++)
			{
				v = checkParts[j].pbody->v + (checkParts[j].pbody->w ^ m_vtx[i].pos + m_pos + m_offs0 -
				                                                           checkParts[j].pbody->pos);
				if (checkParts[j].bPrimitive)
				{
					contact acontact;
					if (checkParts[j].pGeom->UnprojectSphere(
						((m_vtx[i].pos - checkParts[j].offset) * checkParts[j].R) *
						    checkParts[j].rscale,
						m_thickness * checkParts[j].rscale, rsep * checkParts[j].rscale,
						&acontact))
					{
						m_vtx[i].pos = checkParts[j].R * (acontact.pt * checkParts[j].scale +
						                                  acontact.n * m_thickness) +
						               checkParts[j].offset;
						m_vtx[i].ncontact = checkParts[j].R * acontact.n;
						m_vtx[i].pContactEnt = checkParts[j].pent;
						m_vtx[i].iContactPart = checkParts[j].ipart;
						m_vtx[i].vcontact = v;
						m_vtx[i].surface_idx[1] = checkParts[j].surface_idx;
					}
				}
				else
				{
					aray.m_ray.dir = ((v - m_vtx[i].vel) * checkParts[j].R) * m_prevTimeInterval;
					aray.m_dirn = aray.m_ray.dir.normalized();
					aray.m_ray.origin = (m_vtx[i].pos - checkParts[j].offset) * checkParts[j].R -
					                    aray.m_dirn * m_thickness;
					aray.m_ray.dir += aray.m_dirn * m_thickness;
					if (box_ray_overlap_check(&checkParts[j].bbox, &aray.m_ray, nullptr))
					{
						gwd.scale =
						    checkParts[j].scale; // pent->m_parts[checkParts[j].ipart].scale;
						gwd.offset.zero();
						gwd.R.SetIdentity();
						if (checkParts[j]
						        .pent->m_parts[checkParts[j].ipart]
						        .pPhysGeomProxy->pGeom->Intersect(&aray, &gwd, 0, &ip,
						                                          pcontacts))
						{
							WriteLockCond lockColl(*ip.plock, 0);
							lockColl.SetActive();
							if (pcontacts->n * aray.m_dirn > 0)
							{
								m_vtx[i].pos =
								    checkParts[j].R *
									(pcontacts->pt * checkParts[j].scale +
								         aray.m_dirn * m_thickness) +
								    checkParts[j].offset;
								m_vtx[i].ncontact = checkParts[j].R * pcontacts->n;
								m_vtx[i].pContactEnt = checkParts[j].pent;
								m_vtx[i].iContactPart = checkParts[j].ipart;
								m_vtx[i].iContactNode = pcontacts->iNode[0];
								m_vtx[i].vcontact = v;
								imask = pcontacts->id[0] >> 31;
								m_vtx[i].surface_idx[1] =
								    (checkParts[j].surface_idx & imask) |
								    (pcontacts->id[0] & ~imask);
							}
						}
					}
				}
			}

			if (pent != m_vtx[i].pContactEnt)
			{
				if (pent)
				{
					pent->Release();
				}
				if (m_vtx[i].pContactEnt)
				{
					m_vtx[i].pContactEnt->AddRef();
				}
			}
			if (m_vtx[i].pContactEnt)
			{
				m_vtx[nContactVtx++].iSorted = i;
				// if ((m_vtx[i].pos0-m_vtx[i].pos).len2()>sqr(m_maxSafeStep*0.75f))
				//	m_vtx[i].pos =
				// m_vtx[i].pos0+(m_vtx[i].pos-m_vtx[i].pos0).normalized()*(m_maxSafeStep*0.75f);
			}
		}
		else
		{ // synchronize attached vertices w/ hosts
			pbody = m_vtx[i].pContactEnt->GetRigidBody(m_vtx[i].iContactPart);
			m_vtx[i].pos = pbody->pos + pbody->q * m_vtx[i].ptAttach;
			m_vtx[i].vcontact = m_vtx[i].vel = pbody->v + (pbody->w ^ m_vtx[i].pos - pbody->pos);
			m_vtx[i].pos -= m_pos + m_offs0;
			bPosEnforcement |= isneg(sqr(m_maxSafeStep) - (m_vtx[i].pos - m_vtx[i].pos0).len2());
		}

		if (bPosEnforcement)
		{
			for (j = m_vtx[i].iStartEdge; j <= m_vtx[i].iEndEdge; j++)
			{
				if (m_edges[m_pVtxEdges[j]].len >= 0)
				{
					i1 = m_edges[m_pVtxEdges[j]].ivtx[0] + m_edges[m_pVtxEdges[j]].ivtx[1] - i;
					m_vtx[i1].pos = m_vtx[i].pos + (m_vtx[i1].pos - m_vtx[i].pos).normalized() *
					                                   m_edges[m_pVtxEdges[j]].len0;
					m_edges[m_pVtxEdges[j]].len = -1;
				}
			}
		}
	}
	for (i = 0; i < nCheckParts; i++)
	{
		if (checkParts[i].pent->m_bProcessed & 1)
		{
			AtomicAdd(&checkParts[i].pent->m_bProcessed, -1);
			if (checkParts[i].pent->m_nParts > 24)
			{
				for (j = 0; j < checkParts[i].pent->m_nParts; j++)
				{
					checkParts[i].pent->m_parts[j].flags &= ~geom_removed;
				}
			}
		}
	}

	for (i = 0; i < m_nEdges; i++)
	{ // calculate edge lengths
		l = m_vtx[m_edges[i].ivtx[0]].pos - m_vtx[m_edges[i].ivtx[1]].pos;
		m_edges[i].rlen = 1.0f / max(1E-4f, m_edges[i].len = sqrt_tpl(l.len2()));
	}

	pe_params_buoyancy pb[4];
	primitives::plane waterPlane;
	Vec3 waterFlow;
	float waterDensity = 0, rthickness = 1.0f / m_thickness;
	waterPlane.origin.Set(0, 0, -1000);
	waterPlane.n.Set(0, 0, 1);
	if ((nEnts = m_pWorld->CheckAreas(this, w, pb, 4)) && !is_unused(w) &&
	    (w - m_pWorld->m_vars.gravity).len2() > w.len2() * 1E-4f)
	{
		m_gravity = w;
	}
	for (i = 0, w = m_wind; i < nEnts; i++)
	{
		if (pb[i].iMedium)
		{
			w += pb[i].waterFlow;
		}
		else
		{
			waterPlane.origin = pb[i].waterPlane.origin;
			waterPlane.n = pb[i].waterPlane.n;
			waterFlow = pb[i].waterFlow;
			waterDensity = pb[i].waterDensity;
		}
	}

	if ((m_windTimer += time_interval * 4) > 1.0f)
	{
		m_windTimer = 0;
		m_wind0 = m_wind1;
		windage = m_windVariance * (fabs_tpl(w.x) + fabs_tpl(w.y) + fabs_tpl(w.z));
		m_wind1 = w + Vec3(physics_frand(windage), physics_frand(windage), physics_frand(windage));
	}

	for (i = 0; i < m_nVtx; i++)
	{ // apply gravity, buoyancy, wind (or water resistance)
		if (!m_vtx[i].bAttached)
		{
			if ((l0 = (m_vtx[i].pos + m_pos - waterPlane.origin) * waterPlane.n) < 0)
			{
				m_vtx[i].vel -= m_gravity * (waterDensity * m_vtx[i].volume * time_interval *
				                             min(1.0f, -l0 * rthickness));
				kr = m_waterResistance;
				w = waterFlow;
			}
			else
			{
				kr = m_airResistance;
				w = m_wind0 * m_windTimer + m_wind1 * (1.0f - m_windTimer);
			}
			for (j = m_vtx[i].iStartEdge, windage = 0; j <= m_vtx[i].iEndEdge; j++)
			{
				windage += ((m_vtx[m_edges[m_pVtxEdges[j]].ivtx[1]].pos -
				             m_vtx[m_edges[m_pVtxEdges[j]].ivtx[0]].pos) *
				            m_vtx[i].n) *
				           (iszero(i ^ m_edges[m_pVtxEdges[j]].ivtx[0]) * 2 - 1) *
				           m_edges[m_pVtxEdges[j]].rlen;
			}
			m_vtx[i].vel += m_vtx[i].n * ((m_vtx[i].n * (w - m_vtx[i].vel)) * m_vtx[i].area *
			                              (windage * m_vtx[i].rnEdges + 1) * kr * time_interval);
			m_vtx[i].vel += m_gravity * time_interval;
		}
		// if (m_vtx[i].pContactEnt)
		//	m_vtx[i].vel -= m_vtx[i].ncontact*(m_vtx[i].ncontact*(m_vtx[i].vel-m_vtx[i].vcontact));
		m_vtx[i].r.zero();
		m_vtx[i].bSeparating = 0;
		m_vtx[i].pos0 = m_vtx[i].pos;
		m_vtx[i].vel0 = m_vtx[i].vel;
		m_vtx[i].iSorted0 = m_vtx[i].iSorted;
	}
	nContactVtx0 = nContactVtx;
	BBox0[0] = m_BBox[0];
	BBox0[1] = m_BBox[1];

	// reiterate:
	for (i = 0; i < nCheckParts; i++)
	{
		checkParts[i].P = checkParts[i].L.zero();
	}

	iter = 0;
	do
	{
		for (i = 0, rmax = 0; i < m_nConnectedVtx; i++)
		{
			i0 = m_vtx[i].idx;
			for (j = m_vtx[i0].iStartEdge; j <= m_vtx[i0].iEndEdge; j++)
			{
				if ((vreq = (m_edges[m_pVtxEdges[j]].len - m_edges[m_pVtxEdges[j]].len0) * m_ks) > 0)
				{
					i1 = m_edges[m_pVtxEdges[j]].ivtx[0] + m_edges[m_pVtxEdges[j]].ivtx[1] - i0;
					l = (m_vtx[i1].pos - m_vtx[i0].pos) * m_edges[m_pVtxEdges[j]].rlen;
					F = l * -min(0.0f, vreq = (m_vtx[i0].vel - m_vtx[i1].vel) * l - vreq);
					m_vtx[i0].vel += F * (m_vtx[i0].massinv * m_edges[m_pVtxEdges[j]].kmass);
					m_vtx[i1].vel -= F * (m_vtx[i1].massinv * m_edges[m_pVtxEdges[j]].kmass);
					rmax = max(rmax, -vreq);
				}
			}
			if (m_vtx[i0].pContactEnt && !m_vtx[i0].bAttached)
			{
				m_vtx[i0].vel -=
				    m_vtx[i0].ncontact *
				    min(0.0f, vreq = m_vtx[i0].ncontact * (m_vtx[i0].vel - m_vtx[i0].vcontact));
				rmax = max(rmax, -vreq);
				if (m_friction * vreq < 0)
				{
					v = m_vtx[i0].vel - m_vtx[i0].vcontact;
					v -= m_vtx[i0].ncontact * (m_vtx[i0].ncontact * v);
					if (v.len2() > sqr(m_friction * vreq))
					{
						m_vtx[i0].vel += v.normalized() * (m_friction * vreq);
					}
					else
					{
						m_vtx[i0].vel = m_vtx[i0].vcontact;
					}
				}
			}
		}
	}
	while (rmax > m_accuracy && ++iter < m_nMaxIters);

	BBox[0].zero();
	BBox[1].zero();
	rmax = 0;
	m_vtx[0].vel *= (kr = 1.0f - min(1.0f, m_damping * time_interval));
	for (i = 1; i < m_nVtx; i++)
	{
		m_vtx[i].vel *= kr;
		m_vtx[i].pos -= m_vtx[0].pos;
		rmax = max(rmax, m_vtx[i].vel.len2());
		BBox[0] = min(BBox[0], m_vtx[i].pos);
		BBox[1] = max(BBox[1], m_vtx[i].pos);
	}

	BBox[0] += m_pos + m_vtx[0].pos + m_offs0;
	BBox[1] += m_pos + m_vtx[0].pos + m_offs0;
	if (m_flags & pef_traceable)
	{
		bGridLocked = m_pWorld->RepositionEntity(this, 1, BBox);
	}

	{
		WriteLock lock(m_lockUpdate);
		m_BBox[0] = BBox[0];
		m_BBox[1] = BBox[1];
		m_pos += m_vtx[0].pos;
		m_vtx[0].pos.zero();
		m_prevTimeInterval = time_interval;
		if (m_wind.len2() * m_airResistance > 0 || rmax > m_Emin)
		{
			m_nSlowFrames = 0;
			m_bAwake = 1;
		}
		else if (++m_nSlowFrames >= 3)
		{
			m_bAwake = 0;
		}

		float rscale = m_parts[0].scale == 1.0f ? 1.0f : 1.0f / m_parts[0].scale;
		primitives::box bbox;
		bbox.Basis = Matrix33(!m_parts[0].q);
		bbox.center = (m_BBox[0] + m_BBox[1]) * 0.5f - m_pos - m_parts[0].pos;
		bbox.size = (m_BBox[1] - m_BBox[0]) * (0.5f * rscale);
		CTriMesh* pMesh = (CTriMesh*)m_parts[0].pPhysGeomProxy->pGeom;
		((CSingleBoxTree*)pMesh->m_pTree)->SetBox(&bbox);
		d = m_offs0 - m_parts[0].pos;
		if (m_parts[0].scale == 1.0f && m_parts[0].q.w == 1.0f)
		{
			for (i = 0; i < m_nVtx; i++)
			{
				pMesh->m_pVertices[i] = m_vtx[i].pos + d;
			}
		}
		else
		{
			for (i = 0; i < m_nVtx; i++)
			{
				pMesh->m_pVertices[i] = ((m_vtx[i].pos + d) * rscale) * m_parts[0].q;
			}
		}
		m_bMeshUpdated = 1;
		AtomicAdd(&m_pWorld->m_lockGrid, -bGridLocked);
	}

	EventPhysPostStep epps;
	epps.pEntity = this;
	epps.pForeignData = m_pForeignData;
	epps.iForeignData = m_iForeignData;
	epps.dt = time_interval;
	epps.pos = m_pos;
	epps.q = m_qrot;
	epps.idStep = m_pWorld->m_idStep;
	m_pWorld->OnEvent(m_flags, &epps);

	return isneg(m_timeStepFull - m_timeStepPerformed - 0.001f);
}

int CSoftEntity::RayTrace(CRayGeom* pRay, geom_contact*& pcontacts, volatile int*& plock)
{
	static geom_contact g_SoftContact;
	static volatile int g_lock = 0;

	if (m_nVtx > 0)
	{
		CTriMesh* pMesh = (CTriMesh*)m_parts[0].pPhysGeom->pGeom;
		prim_inters inters;
		primitives::triangle atri;
		int i, j;

		for (i = 0; i < pMesh->m_nTris; i++)
		{
			for (j = 0; j < 3; j++)
			{
				atri.pt[j] = m_vtx[pMesh->m_pIndices[(i * 3) + j]].pos + m_pos + m_offs0;
			}
			atri.n = atri.pt[1] - atri.pt[0] ^ atri.pt[2] - atri.pt[0];
			if (ray_tri_intersection(&pRay->m_ray, &atri, &inters))
			{
				WriteLockCond lock(g_lock, 1);
				lock.SetActive(0);
				plock = &g_lock;
				pcontacts = &g_SoftContact;
				pcontacts->pt = inters.pt[0];
				pcontacts->t = (inters.pt[0] - pRay->m_ray.origin) * pRay->m_dirn;
				pcontacts->id[0] = pMesh->m_pIds ? pMesh->m_pIds[i] : m_parts[0].surface_idx;
				pcontacts->iNode[0] = i;
				pcontacts->n = atri.n.normalized() * -sgnnz(atri.n * pRay->m_dirn);
				return 1;
			}
		}
	}

	return 0;
}

void CSoftEntity::ApplyVolumetricPressure(const Vec3& epicenter, float kr, float rmin)
{
	if (m_nVtx > 0)
	{
		CTriMesh* pMesh = (CTriMesh*)m_parts[0].pPhysGeom->pGeom;
		Vec3 ptc, r, n, pt[3], dP;
		int i, j, idx[3];
		float r2, rmin2 = sqr(rmin);
		kr *= m_explosionScale;

		for (i = 0; i < pMesh->m_nTris; i++)
		{
			for (j = 0; j < 3; j++)
			{
				pt[j] = m_vtx[idx[j] = pMesh->m_pIndices[(i * 3) + j]].pos + m_pos + m_offs0;
			}
			ptc = (pt[0] + pt[1] + pt[2]) * (1.0f / 3);
			r = ptc - epicenter;
			r2 = r.len2();
			n = pt[1] - pt[0] ^ pt[2] - pt[0];
			n *= sgnnz(r * n);
			dP = n * ((n * r) * 0.5f * kr / (sqrt_tpl(n.len2() * r2) * max(rmin2, r2)));
			for (j = 0; j < 3; j++)
			{
				m_vtx[idx[j]].vel += dP * m_vtx[idx[j]].massinv;
			}
		}
		m_bAwake = 1;
	}
}

void CSoftEntity::DrawHelperInformation(IPhysRenderer* pRenderer, int flags)
{
	CPhysicalEntity::DrawHelperInformation(pRenderer, flags);

	int i;
	Vec3 offs = m_pos + m_offs0;
	if (flags & pe_helper_geometry)
	{
		for (i = 0; i < m_nEdges; i++)
		{
			pRenderer->DrawLine(m_vtx[m_edges[i].ivtx[0]].pos + offs, m_vtx[m_edges[i].ivtx[1]].pos + offs,
			                    m_iSimClass);
		}
	}
	if (flags & pe_helper_collisions)
	{
		for (i = 0; i < m_nVtx; i++)
		{
			if (m_vtx[i].pContactEnt && !m_vtx[i].bSeparating)
			{
				pRenderer->DrawLine(m_vtx[i].pos + offs - m_vtx[i].ncontact * m_thickness,
				                    m_vtx[i].pos + offs, m_iSimClass);
			}
		}
	}
}
