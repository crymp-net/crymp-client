#include "CryCommon/CryMath/GeomQuery.h"
#include "CryCommon/CryNetwork/ISerialize.h"

#include "ArticulatedEntity.h"
#include "BoxGeom.h"
#include "BVTree.h"
#include "ChangeRequest.h"
#include "ContactSolver.h"
#include "Geometry.h"
#include "LivingEntity.h"
#include "ParticleEntity.h"
#include "PhysicalEntity.h"
#include "PhysicalWorld.h"
#include "Quotient.h"
#include "RigidBody.h"
#include "RigidEntity.h"
#include "RopeEntity.h"
#include "SoftEntity.h"
#include "SphereGeom.h"
#include "TetrLattice.h"
#include "TriMesh.h"
#include "Utils.h"
#include "WheeledVehicleEntity.h"

namespace {

struct SPartHelper
{
	int idx;
	float Minv;
	Matrix33 Iinv;
	Vec3 v, w;
	Vec3 org;
	int bProcessed;
	int ijoint0;
	int isle;
	CPhysicalEntity* pent;
};

struct SStructuralJointHelper
{
	int idx;
	Vec3 r0, r1;
	Matrix33 vKinv, wKinv;
	Vec3 P, L;
	int bBroken;
};

struct SStructuralJointDebugHelper
{
	int itens;
	quotientf tension;
};

SPartHelper* g_parts = nullptr;
SStructuralJointHelper* g_joints = nullptr;
SStructuralJointDebugHelper* g_jointsDbg = nullptr;
int* g_jointidx = nullptr;
int g_nPartsAlloc = 0;
int g_nJointsAlloc = 0;

} // unnamed namespace

struct SExplosionInfo
{
	Vec3 center;
	Vec3 dir;
	float rmin, kr;
};

CPhysicalEntity g_StaticPhysicalEntity(nullptr);

CPhysicalEntity::CPhysicalEntity(CPhysicalWorld* pworld)
{
	m_pos.zero();
	m_qrot.SetIdentity();
	m_BBox[0].zero();
	m_BBox[1].zero();
	m_iSimClass = 0;
	m_iPrevSimClass = -1;
	m_iGThunk0 = 0;
	m_ig[0].x = m_ig[1].x = m_ig[0].y = m_ig[1].y = -2;
	m_prev = m_next = 0;
	m_bProcessed = 0;
	m_nRefCount = 0; // 1; 0 means that initially no other physical entity references this one
	m_nParts = 0;
	m_nPartsAlloc = 1;
	m_parts = &m_defpart;
	memset(&m_defpart, 0, sizeof(m_defpart));
	m_parts[0].pNewCoords = (coord_block_BBox*)&m_parts[0].pos;
	m_pNewCoords = (coord_block*)&m_pos;
	m_iLastIdx = 0;
	m_pWorld = pworld;
	m_pOuterEntity = 0;
	m_pBoundingGeometry = 0;
	m_bProcessed_aux = 0;
	m_nColliders = m_nCollidersAlloc = 0;
	m_pColliders = 0;
	m_iGroup = -1;
	m_bMoved = 0;
	m_id = -1;
	m_pForeignData = 0;
	m_iForeignData = m_iForeignFlags = 0;
	m_timeIdle = m_maxTimeIdle = 0;
	m_bPermanent = 1;
	m_pEntBuddy = 0;
	m_flags = pef_pushable_by_players | pef_traceable | pef_never_affect_triggers | pef_never_break;
	m_lockUpdate = 0;
	m_lockPartIdx = 0;
	m_nGroundPlanes = 0;
	m_ground = 0;
	m_timeStructUpdate = 0;
	m_updStage = 1;
	m_nUpdTicks = 0;
	m_iDeletionTime = 0;
	m_pStructure = 0;
	m_pExpl = 0;
	m_nRefCountPOD = 0;
	m_iLastPODUpdate = 0;
}

CPhysicalEntity::~CPhysicalEntity()
{
	for (int i = 0; i < m_nParts; i++)
	{
		if (m_parts[i].pPhysGeom)
		{
			if (m_parts[i].pMatMapping && m_parts[i].pMatMapping != m_parts[i].pPhysGeom->pMatMapping)
			{
				delete[] m_parts[i].pMatMapping;
			}
			m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeom);
			if (m_parts[i].pPhysGeomProxy != m_parts[i].pPhysGeom)
			{
				m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeomProxy);
			}
			if (m_parts[i].flags & geom_can_modify && m_parts[i].idmatBreakable >= 0 && m_parts[i].pLattice)
			{
				m_parts[i].pLattice->Release();
			}
		}
	}
	if (m_parts != &m_defpart)
	{
		delete[] m_parts;
	}
	if (m_pColliders)
	{
		delete[] m_pColliders;
	}
	if (m_ground)
	{
		delete[] m_ground;
	}
	if (m_pExpl)
	{
		delete[] m_pExpl;
	}
	if (m_pStructure)
	{
		delete[] m_pStructure->Pext;
		delete[] m_pStructure->Lext;
		delete[] m_pStructure->Fext;
		delete[] m_pStructure->Text;
		delete[] m_pStructure->pJoints;
		if (m_pStructure->Pexpl)
		{
			delete[] m_pStructure->Pexpl;
		}
		if (m_pStructure->Lexpl)
		{
			delete[] m_pStructure->Lexpl;
		}
		delete m_pStructure;
		m_pStructure = 0;
	}
}

int CPhysicalEntity::AddRef()
{
	if (m_pWorld->IsPodThread() && m_pWorld->m_lockPODGrid)
	{
		if (m_iLastPODUpdate == m_pWorld->m_iLastPODUpdate)
		{
			return m_nRefCount;
		}
		m_iLastPODUpdate = m_pWorld->m_iLastPODUpdate;
		++m_nRefCountPOD;
	}
	AtomicAdd(&m_nRefCount, 1);
	return m_nRefCount;
}

int CPhysicalEntity::Release()
{
	if (m_pWorld->IsPodThread() && m_pWorld->m_lockPODGrid)
	{
		if (m_iLastPODUpdate == m_pWorld->m_iLastPODUpdate)
		{
			return m_nRefCount;
		}
		m_iLastPODUpdate = m_pWorld->m_iLastPODUpdate;
		if (m_nRefCountPOD <= 0)
		{
			return m_nRefCount;
		}
		--m_nRefCountPOD;
	}
	AtomicAdd(&m_nRefCount, -1);
	return m_nRefCount;
}

void CPhysicalEntity::ComputeBBox(Vec3* BBox, int flags)
{
	if (m_nParts + (flags & part_added) != 0)
	{
		IGeometry* pGeom[3];
		Matrix33 R;
		int i, j;
		primitives::box abox;
		Vec3 sz, pos;
		BBox[0] = Vec3(VMAX);
		BBox[1] = Vec3(VMIN);

		for (i = 0; i < m_nParts + (flags & part_added); i++)
		{
			pGeom[0] = m_parts[i].pPhysGeomProxy->pGeom;
			pGeom[1] = pGeom[2] = m_parts[i].pPhysGeom->pGeom;
			if (flags & update_part_bboxes)
			{
				m_parts[i].pNewCoords->BBox[0] = Vec3(VMAX);
				m_parts[i].pNewCoords->BBox[1] = Vec3(VMIN);
			}
			j = 0;
			do
			{
				pGeom[j]->GetBBox(&abox);
				R = Matrix33(m_pNewCoords->q * m_parts[i].pNewCoords->q);
				abox.Basis *= R.T();
				sz = (abox.size * abox.Basis.Fabs()) * m_parts[i].pNewCoords->scale;
				pos = m_pNewCoords->pos + m_pNewCoords->q * (m_parts[i].pNewCoords->pos +
				                                             m_parts[i].pNewCoords->q * abox.center *
				                                                 m_parts[i].pNewCoords->scale);
				if (flags & update_part_bboxes)
				{
					m_parts[i].pNewCoords->BBox[0] =
					    min_safe(m_parts[i].pNewCoords->BBox[0], pos - sz);
					m_parts[i].pNewCoords->BBox[1] =
					    max_safe(m_parts[i].pNewCoords->BBox[1], pos + sz);
				}
				BBox[0] = min_safe(BBox[0], pos - sz);
				BBox[1] = max_safe(BBox[1], pos + sz);
				j++;
			}
			while (pGeom[j] != pGeom[j - 1]);
		}
	}
	else
	{
		BBox[0] = BBox[1] = m_pNewCoords->pos;
	}
	if (m_pEntBuddy && BBox == m_BBox)
	{
		m_pEntBuddy->m_BBox[0] = BBox[0], m_pEntBuddy->m_BBox[1] = BBox[1];
	}
}

int CPhysicalEntity::SetParams(pe_params* _params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this, m_pWorld, _params, bThreadSafe);
	if (req.IsQueued())
	{
		return 1;
	}

	if (_params->type == pe_params_pos::type_id)
	{
		pe_params_pos* params = (pe_params_pos*)_params;
		get_xqs_from_matrices(params->pMtx3x4, params->pMtx3x3, params->pos, params->q, params->scale);
		int i, j, bPosChanged = 0, bBBoxReady = 0;
		Vec3 BBox[2];
		coord_block cnew, *pcPrev;
		cnew.pos = m_pos;
		cnew.q = m_qrot;
		if (!is_unused(params->pos))
		{
			cnew.pos = params->pos;
			bPosChanged = 1;
		}
		if (!is_unused(params->q))
		{
			cnew.q = params->q;
			bPosChanged = 1;
		}
		if (!is_unused(params->iSimClass) && m_iSimClass >= 0 && m_iSimClass < 7)
		{
			m_iSimClass = params->iSimClass;
			m_pWorld->RepositionEntity(this, 2);
		}

		if (!is_unused(params->scale))
		{
			if (params->bRecalcBounds)
			{
				IGeometry* pGeom[3];
				Matrix33 R;
				primitives::box abox;
				BBox[0] = Vec3(VMAX);
				BBox[1] = Vec3(VMIN);
				if (m_nParts == 0)
				{
					BBox[0] = BBox[1] = cnew.pos;
				}
				else
				{
					for (i = 0; i < m_nParts; i++)
					{
						pGeom[0] = m_parts[i].pPhysGeomProxy->pGeom;
						pGeom[1] = pGeom[2] = m_parts[i].pPhysGeom->pGeom;
						j = 0;
						do
						{
							pGeom[j]->GetBBox(&abox);
							R = Matrix33(cnew.q * m_parts[i].q);
							abox.Basis *= R.T();
							Vec3 sz = (abox.size * abox.Basis.Fabs()) * params->scale;
							Vec3 pos = cnew.pos + cnew.q * (m_parts[i].pos +
							                                m_parts[i].q * abox.center *
							                                    params->scale);
							BBox[0] = min_safe(BBox[0], pos - sz);
							BBox[1] = max_safe(BBox[1], pos + sz);
							j++;
						}
						while (pGeom[j] != pGeom[j - 1]);
					}
				}
				bBBoxReady = 1;
			}
			bPosChanged = 1;
		}

		if (bPosChanged)
		{
			if (params->bRecalcBounds)
			{
				CPhysicalEntity** pentlist;
				// make triggers aware of the object's movements
				if (!(m_flags & pef_never_affect_triggers))
				{
					m_pWorld->GetEntitiesAround(m_BBox[0], m_BBox[1], pentlist, ent_triggers, this);
				}
				if (!bBBoxReady)
				{
					pcPrev = m_pNewCoords;
					m_pNewCoords = &cnew;
					ComputeBBox(BBox, 0);
					m_pNewCoords = pcPrev;
				}
				bPosChanged = m_pWorld->RepositionEntity(this, 1, BBox);
			}

			{
				WriteLock lock(m_lockUpdate);
				m_pos = m_pNewCoords->pos = cnew.pos;
				m_qrot = m_pNewCoords->q = cnew.q;
				if (!is_unused(params->scale))
				{
					for (i = 0; i < m_nParts; i++)
					{
						m_parts[i].pNewCoords->pos =
						    (m_parts[i].pos *= params->scale / m_parts[i].scale);
						m_parts[i].pNewCoords->scale = (m_parts[i].scale = params->scale);
					}
				}
				if (params->bRecalcBounds)
				{
					ComputeBBox(m_BBox);
					AtomicAdd(&m_pWorld->m_lockGrid, -bPosChanged);
				}
			}
			if (params->bRecalcBounds && !(m_flags & pef_never_affect_triggers))
			{
				CPhysicalEntity** pentlist;
				m_pWorld->GetEntitiesAround(m_BBox[0], m_BBox[1], pentlist, ent_triggers, this);
			}
		}

		return 1;
	}

	if (_params->type == pe_params_bbox::type_id)
	{
		pe_params_bbox* params = (pe_params_bbox*)_params;
		Vec3 BBox[2] = {m_BBox[0], m_BBox[1]};
		if (!is_unused(params->BBox[0]))
		{
			BBox[0] = params->BBox[0];
		}
		if (!is_unused(params->BBox[1]))
		{
			BBox[1] = params->BBox[1];
		}
		int bPosChanged = m_pWorld->RepositionEntity(this, 1, BBox);
		{
			WriteLock lock(m_lockUpdate);
			WriteBBox(BBox);
			AtomicAdd(&m_pWorld->m_lockGrid, -bPosChanged);
		}
		return 1;
	}

	if (_params->type == pe_params_part::type_id)
	{
		pe_params_part* params = (pe_params_part*)_params;
		int i = params->ipart;
		if (is_unused(params->ipart) && is_unused(params->partid))
		{
			for (i = 0; i < m_nParts; i++)
			{
				if (is_unused(params->flagsCond) || m_parts[i].flags & params->flagsCond)
				{
					m_parts[i].flags = (m_parts[i].flags & params->flagsAND) | params->flagsOR;
					m_parts[i].flagsCollider =
					    (m_parts[i].flagsCollider & params->flagsColliderAND) |
					    params->flagsColliderOR;
					if (!is_unused(params->mass))
					{
						m_parts[i].mass = params->mass;
					}
					if (!is_unused(params->idmatBreakable))
					{
						m_parts[i].idmatBreakable = -1;
					}
					if (!is_unused(params->pMatMapping))
					{
						if (m_parts[i].pMatMapping &&
						    m_parts[i].pMatMapping != m_parts[i].pPhysGeom->pMatMapping)
						{
							delete[] m_parts[i].pMatMapping;
						}
						if (params->pMatMapping)
						{
							memcpy(m_parts[i].pMatMapping = new int[params->nMats],
							       params->pMatMapping, params->nMats * sizeof(int));
						}
						else
						{
							m_parts[i].pMatMapping = 0;
						}
						m_parts[i].nMats = params->nMats;
					}
				}
			}
			return 1;
		}
		if (is_unused(params->ipart))
		{
			for (i = 0; i < m_nParts && m_parts[i].id != params->partid; i++)
				;
		}
		if (i >= m_nParts)
		{
			return 0;
		}
		get_xqs_from_matrices(params->pMtx3x4, params->pMtx3x3, params->pos, params->q, params->scale);
		coord_block_BBox newCoord, *prevCoord = m_parts[i].pNewCoords;
		Vec3 BBox[2] = {m_BBox[0], m_BBox[1]};
		newCoord.pos = newCoord.BBox[0] = newCoord.BBox[1] = m_parts[i].pos;
		newCoord.q = m_parts[i].q;
		newCoord.scale = m_parts[i].scale;
		int bPosChanged = 0;
		if (!is_unused(params->ipart) && !is_unused(params->partid))
		{
			m_parts[i].id = params->partid;
		}
		if (!is_unused(params->pos.x))
		{
			newCoord.pos = params->pos;
		}
		if (!is_unused(params->q))
		{
			newCoord.q = params->q;
		}
		if (!is_unused(params->scale))
		{
			newCoord.scale = params->scale;
		}
		if (!is_unused(params->mass))
		{
			m_parts[i].mass = params->mass;
		}
		if (!is_unused(params->density))
		{
			m_parts[i].mass = m_parts[i].pPhysGeom->V * cube(m_parts[i].scale) * params->density;
		}
		if (!is_unused(params->minContactDist))
		{
			m_parts[i].minContactDist = params->minContactDist;
		}
		if (!is_unused(params->pPhysGeom) && params->pPhysGeom != m_parts[i].pPhysGeom && params->pPhysGeom &&
		    params->pPhysGeom->pGeom)
		{
			WriteLock lock(m_lockUpdate);
			if (m_parts[i].pMatMapping == m_parts[i].pPhysGeom->pMatMapping)
			{
				m_parts[i].pMatMapping = params->pPhysGeom->pMatMapping;
				m_parts[i].nMats = params->pPhysGeom->nMats;
			}
			m_parts[i].surface_idx = params->pPhysGeom->surface_idx;
			if (m_parts[i].pPhysGeom == m_parts[i].pPhysGeomProxy)
			{
				m_parts[i].pPhysGeomProxy = params->pPhysGeom;
			}
			m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeom);
			m_parts[i].pPhysGeom = params->pPhysGeom;
			m_pWorld->GetGeomManager()->AddRefGeometry(params->pPhysGeom);
		}
		if (!is_unused(params->pPhysGeomProxy) && params->pPhysGeomProxy != m_parts[i].pPhysGeomProxy &&
		    params->pPhysGeomProxy && params->pPhysGeomProxy->pGeom)
		{
			WriteLock lock(m_lockUpdate);
			m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeomProxy);
			m_parts[i].pPhysGeomProxy = params->pPhysGeomProxy;
			m_pWorld->GetGeomManager()->AddRefGeometry(params->pPhysGeomProxy);
		}
		if (!is_unused(params->pLattice) && (ITetrLattice*)m_parts[i].pLattice != params->pLattice &&
		    m_parts[i].pPhysGeomProxy->pGeom->GetType() == GEOM_TRIMESH)
		{
			if (m_parts[i].flags & geom_can_modify && m_parts[i].pLattice)
			{
				m_parts[i].pLattice->Release();
			}
			(m_parts[i].pLattice = (CTetrLattice*)params->pLattice)
			    ->SetMesh((CTriMesh*)m_parts[i].pPhysGeomProxy->pGeom);
		}
		if (!is_unused(params->pMatMapping))
		{
			if (m_parts[i].pMatMapping && m_parts[i].pMatMapping != m_parts[i].pPhysGeom->pMatMapping)
			{
				delete[] m_parts[i].pMatMapping;
			}
			if (params->pMatMapping)
			{
				if (params->pMatMapping == m_parts[i].pPhysGeom->pMatMapping)
				{
					m_parts[i].pMatMapping = params->pMatMapping;
				}
				else
				{
					memcpy(m_parts[i].pMatMapping = new int[params->nMats], params->pMatMapping,
					       params->nMats * sizeof(int));
				}
			}
			else
			{
				m_parts[i].pMatMapping = 0;
			}
			m_parts[i].nMats = params->nMats;
		}
		if (!is_unused(params->idmatBreakable))
		{
			m_parts[i].idmatBreakable = params->idmatBreakable;
		}
		else if (!is_unused(params->pMatMapping) || !is_unused(params->pPhysGeom))
		{
			UpdatePartIdmatBreakable(i);
		}
		m_parts[i].flags = (m_parts[i].flags & params->flagsAND) | params->flagsOR;
		m_parts[i].flagsCollider =
		    (m_parts[i].flagsCollider & params->flagsColliderAND) | params->flagsColliderOR;
		if (m_parts[i].pLattice)
		{
			m_parts[i].flags |= geom_monitor_contacts;
		}
		if (params->bRecalcBBox)
		{
			m_parts[i].pNewCoords = &newCoord;
			ComputeBBox(BBox);
			bPosChanged = m_pWorld->RepositionEntity(this, 1, BBox);
		}
		{
			WriteLock lock(m_lockUpdate);
			m_parts[i].pNewCoords = prevCoord;
			m_parts[i].pos = m_parts[i].pNewCoords->pos = newCoord.pos;
			m_parts[i].q = m_parts[i].pNewCoords->q = newCoord.q;
			m_parts[i].scale = m_parts[i].pNewCoords->scale = newCoord.scale;
			if (params->bRecalcBBox)
			{
				WriteBBox(BBox);
				m_parts[i].BBox[0] = m_parts[i].pNewCoords->BBox[0] = newCoord.BBox[0];
				m_parts[i].BBox[1] = m_parts[i].pNewCoords->BBox[1] = newCoord.BBox[1];
				for (int j = 0; j < m_nParts; j++)
				{
					if (i != j && m_parts[j].pNewCoords != (coord_block_BBox*)&m_parts[j].pos)
					{
						m_parts[j].BBox[0] = m_parts[j].pNewCoords->BBox[0];
						m_parts[j].BBox[1] = m_parts[j].pNewCoords->BBox[1];
					}
				}
			}
			AtomicAdd(&m_pWorld->m_lockGrid, -bPosChanged);
		}
		return i + 1;
	}

	if (_params->type == pe_params_outer_entity::type_id)
	{
		m_pOuterEntity = (CPhysicalEntity*)((pe_params_outer_entity*)_params)->pOuterEntity;
		m_pBoundingGeometry = (CGeometry*)((pe_params_outer_entity*)_params)->pBoundingGeometry;
		return 1;
	}

	if (_params->type == pe_params_foreign_data::type_id)
	{
		// WriteLock lock(m_lockUpdate);
		pe_params_foreign_data* params = (pe_params_foreign_data*)_params;
		if (!is_unused(params->pForeignData))
		{
			m_pForeignData = 0;
		}
		if (!is_unused(params->iForeignData))
		{
			m_iForeignData = params->iForeignData;
		}
		if (!is_unused(params->pForeignData))
		{
			m_pForeignData = params->pForeignData;
		}
		if (!is_unused(params->iForeignFlags))
		{
			m_iForeignFlags = params->iForeignFlags;
		}
		m_iForeignFlags = (m_iForeignFlags & params->iForeignFlagsAND) | params->iForeignFlagsOR;
		if (m_pEntBuddy)
		{
			m_pEntBuddy->m_pForeignData = m_pForeignData;
			m_pEntBuddy->m_iForeignData = m_iForeignData;
			m_pEntBuddy->m_iForeignFlags = m_iForeignFlags;
		}
		return 1;
	}

	if (_params->type == pe_params_flags::type_id)
	{
		pe_params_flags* params = (pe_params_flags*)_params;
		int flags = m_flags;
		if (!is_unused(params->flags))
		{
			m_flags = params->flags;
		}
		if (!is_unused(params->flagsAND))
		{
			m_flags &= params->flagsAND;
		}
		if (!is_unused(params->flagsOR))
		{
			m_flags |= params->flagsOR;
		}

		if (m_flags & pef_traceable && m_ig[0].x == -3)
		{
			m_ig[0].x = m_ig[1].x = m_ig[0].y = m_ig[1].y = -2;
			if (m_pos.len2() > 0)
			{
				AtomicAdd(&m_pWorld->m_lockGrid, -m_pWorld->RepositionEntity(this, 1));
			}
		}
		if (!(m_flags & pef_traceable) && m_ig[0].x != -3)
		{
			WriteLock lock(m_pWorld->m_lockGrid);
			m_pWorld->DetachEntityGridThunks(this);
			m_ig[0].x = m_ig[1].x = m_ig[0].y = m_ig[1].y = -3;
		}

		return 1;
	}

	if (_params->type == pe_params_ground_plane::type_id)
	{
		pe_params_ground_plane* params = (pe_params_ground_plane*)_params;
		if (params->iPlane < 0)
		{
			m_nGroundPlanes = 0;
			if (m_ground)
			{
				delete[] m_ground;
				m_ground = 0;
			}
		}
		else if (params->iPlane >= m_nGroundPlanes)
		{
			ReallocateList(m_ground, m_nGroundPlanes, params->iPlane + 1, true);
			m_nGroundPlanes = params->iPlane + 1;
		}
		if (!is_unused(params->ground.origin))
		{
			m_ground[params->iPlane].origin = params->ground.origin;
		}
		if (!is_unused(params->ground.n))
		{
			m_ground[params->iPlane].n = params->ground.n;
		}
		return 1;
	}

	if (_params->type == pe_params_structural_joint::type_id)
	{
		pe_params_structural_joint* params = (pe_params_structural_joint*)_params;
		int i, j, iop;

		if (!is_unused(params->idx) && params->idx == -1)
		{
			if (m_pStructure)
			{
				if (!is_unused(params->partidEpicenter))
				{
					m_pStructure->idpartBreakOrg = params->partidEpicenter;
				}
				m_pWorld->MarkEntityAsDeforming(this);
			}
			return 1;
		}

		if (!m_pStructure)
		{
			memset(m_pStructure = new SStructureInfo, 0, sizeof(SStructureInfo));
			m_pStructure->idpartBreakOrg = -1;
			memset(m_pStructure->Pext = new Vec3[m_nParts], 0, m_nParts * sizeof(Vec3));
			memset(m_pStructure->Lext = new Vec3[m_nParts], 0, m_nParts * sizeof(Vec3));
			memset(m_pStructure->Fext = new Vec3[m_nParts], 0, m_nParts * sizeof(Vec3));
			memset(m_pStructure->Text = new Vec3[m_nParts], 0, m_nParts * sizeof(Vec3));
			m_pStructure->pJoints = 0;
			for (i = 0; i < m_nParts; i++)
			{
				m_parts[i].flags |= geom_monitor_contacts;
			}
		}
		if (m_pStructure->nPartsAlloc < m_nParts)
		{
			ReallocateList(m_pStructure->Pext, m_pStructure->nPartsAlloc, m_nParts, true);
			ReallocateList(m_pStructure->Lext, m_pStructure->nPartsAlloc, m_nParts, true);
			ReallocateList(m_pStructure->Fext, m_pStructure->nPartsAlloc, m_nParts, true);
			ReallocateList(m_pStructure->Text, m_pStructure->nPartsAlloc, m_nParts, true);
			m_pStructure->nPartsAlloc = m_nParts;
		}

		if (params->bReplaceExisting)
		{
			for (i = 0; i < m_pStructure->nJoints && m_pStructure->pJoints[i].id != params->id; i++)
				;
		}
		else
		{
			i = m_pStructure->nJoints;
		}
		if (i >= m_pStructure->nJointsAlloc)
		{
			ReallocateList(m_pStructure->pJoints, m_pStructure->nJointsAlloc,
			               m_pStructure->nJointsAlloc + 4, true),
			    m_pStructure->nJointsAlloc += 4;
		}

		for (iop = 0; iop < 2; iop++)
		{
			if (!is_unused(params->partid[iop]) || i == m_pStructure->nJoints)
			{
				for (j = m_nParts - 1; j >= 0 && m_parts[j].id != params->partid[iop]; j--)
					;
				if (j >= 0 && m_parts[j].mass == 0)
				{
					j = -1;
				}
				m_pStructure->pJoints[i].ipart[iop] = j;
			}
		}
		if (!is_unused(params->pt))
		{
			m_pStructure->pJoints[i].pt = params->pt;
		}
		if (!is_unused(params->n))
		{
			m_pStructure->pJoints[i].n = params->n;
		}
		if (!is_unused(params->maxForcePush))
		{
			m_pStructure->pJoints[i].maxForcePush = params->maxForcePush;
		}
		if (!is_unused(params->maxForcePull))
		{
			m_pStructure->pJoints[i].maxForcePull = params->maxForcePull;
		}
		if (!is_unused(params->maxForceShift))
		{
			m_pStructure->pJoints[i].maxForceShift = params->maxForceShift;
		}
		if (!is_unused(params->maxTorqueBend))
		{
			m_pStructure->pJoints[i].maxTorqueBend = params->maxTorqueBend;
		}
		if (!is_unused(params->maxTorqueTwist))
		{
			m_pStructure->pJoints[i].maxTorqueTwist = params->maxTorqueTwist;
		}
		if (!is_unused(params->bBreakable))
		{
			m_pStructure->pJoints[i].bBreakable = params->bBreakable;
		}
		else if (i == m_pStructure->nJoints)
		{
			m_pStructure->pJoints[i].bBreakable = 1;
		}
		if (!is_unused(params->id))
		{
			m_pStructure->pJoints[i].id = params->id;
		}
		else if (i == m_pStructure->nJoints)
		{
			m_pStructure->pJoints[i].id = i;
		}
		if (!is_unused(params->szSensor))
		{
			m_pStructure->pJoints[i].size = params->szSensor;
		}
		else if (i == m_pStructure->nJoints)
		{
			m_pStructure->pJoints[i].size = m_pWorld->m_vars.maxContactGap * 5;
		}
		m_pStructure->pJoints[i].tension = 0;
		m_pStructure->pJoints[i].itens = 0;
		m_pStructure->nJoints = max(i + 1, m_pStructure->nJoints);
		if (!is_unused(params->bBroken))
		{
			if (m_pStructure->pJoints[i].bBroken != params->bBroken &&
			    (m_pStructure->pJoints[i].bBroken = params->bBroken) && i < m_pStructure->nJoints)
			{
				if (is_unused(params->partidEpicenter))
				{
					if (i != m_pStructure->nJoints - 1)
					{
						m_pStructure->pJoints[i] =
						    m_pStructure->pJoints[m_pStructure->nJoints - 1];
					}
					--m_pStructure->nJoints;
				}
				else
				{
					m_pStructure->pJoints[i].bBroken = params->bBroken;
					m_pStructure->idpartBreakOrg = params->partidEpicenter;
				}
				m_pWorld->MarkEntityAsDeforming(this);
			}
		}
		else
		{
			m_pStructure->pJoints[i].bBroken = 0;
		}
		return 1;
	}

	if (_params->type == pe_params_timeout::type_id)
	{
		pe_params_timeout* params = (pe_params_timeout*)_params;
		if (!is_unused(params->timeIdle))
		{
			m_timeIdle = params->timeIdle;
		}
		if (!is_unused(params->maxTimeIdle))
		{
			m_maxTimeIdle = params->maxTimeIdle;
		}
		return 1;
	}

	return 0;
}

int CPhysicalEntity::GetParams(pe_params* _params)
{
	if (_params->type == pe_params_bbox::type_id)
	{
		pe_params_bbox* params = (pe_params_bbox*)_params;
		ReadLock lock(m_lockUpdate);
		params->BBox[0] = m_BBox[0];
		params->BBox[1] = m_BBox[1];
		return 1;
	}

	if (_params->type == pe_params_outer_entity::type_id)
	{
		((pe_params_outer_entity*)_params)->pOuterEntity = m_pOuterEntity;
		((pe_params_outer_entity*)_params)->pBoundingGeometry = m_pBoundingGeometry;
		return 1;
	}

	if (_params->type == pe_params_foreign_data::type_id)
	{
		pe_params_foreign_data* params = (pe_params_foreign_data*)_params;
		ReadLock lock(m_lockUpdate);
		params->iForeignData = m_iForeignData;
		params->pForeignData = m_pForeignData;
		params->iForeignFlags = m_iForeignFlags;
		return 1;
	}

	if (_params->type == pe_params_part::type_id)
	{
		pe_params_part* params = (pe_params_part*)_params;
		ReadLock lock(m_lockUpdate);
		int i;
		if ((is_unused(params->ipart) || params->ipart == -1) && params->partid >= 0)
		{
			for (i = 0; i < m_nParts && m_parts[i].id != params->partid; i++)
				;
			if (i == m_nParts)
			{
				return 0;
			}
		}
		else if ((unsigned int)(i = params->ipart) >= (unsigned int)m_nParts)
		{
			return 0;
		}
		params->partid = m_parts[params->ipart = i].id;
		params->pos = m_parts[i].pos;
		params->q = m_parts[i].q;
		params->scale = m_parts[i].scale;
		if (params->pMtx3x4)
		{
			(*params->pMtx3x4 = Matrix33(m_parts[i].q) * m_parts[i].scale).SetTranslation(m_parts[i].pos);
		}
		if (params->pMtx3x3)
		{
			*params->pMtx3x3 = Matrix33(m_parts[i].q) * m_parts[i].scale;
		}
		params->flagsOR = params->flagsAND = m_parts[i].flags;
		params->flagsColliderOR = params->flagsColliderAND = m_parts[i].flagsCollider;
		params->mass = m_parts[i].mass;
		params->density = m_parts[i].pPhysGeomProxy->V > 0
		                      ? m_parts[i].mass / (m_parts[i].pPhysGeomProxy->V * cube(m_parts[i].scale))
		                      : 0;
		params->minContactDist = m_parts[i].minContactDist;
		params->pPhysGeom = m_parts[i].pPhysGeom;
		params->pPhysGeomProxy = m_parts[i].pPhysGeomProxy;
		params->idmatBreakable = m_parts[i].idmatBreakable;
		params->pLattice = m_parts[i].pLattice;
		params->pMatMapping = m_parts[i].pMatMapping;
		params->nMats = m_parts[i].nMats;
		if (params->bAddrefGeoms)
		{
			m_pWorld->GetGeomManager()->AddRefGeometry(params->pPhysGeom);
			m_pWorld->GetGeomManager()->AddRefGeometry(params->pPhysGeomProxy);
		}
		return 1;
	}

	if (_params->type == pe_params_flags::type_id)
	{
		((pe_params_flags*)_params)->flags = m_flags;
		return 1;
	}

	if (_params->type == pe_params_ground_plane::type_id)
	{
		pe_params_ground_plane* params = (pe_params_ground_plane*)_params;
		if ((unsigned int)params->iPlane >= (unsigned int)(sizeof(m_ground) / sizeof(m_ground[0])))
		{
			return 0;
		}
		params->ground.origin = m_ground[params->iPlane].origin;
		params->ground.n = m_ground[params->iPlane].n;
		return 1;
	}

	if (_params->type == pe_params_structural_joint::type_id)
	{
		pe_params_structural_joint* params = (pe_params_structural_joint*)_params;
		if (!m_pStructure)
		{
			return 0;
		}
		int i, iop;
		if (!is_unused(params->idx))
		{
			i = params->idx;
		}
		else
		{
			for (i = 0; i < m_pStructure->nJoints && m_pStructure->pJoints[i].id != params->id; i++)
				;
		}
		if ((unsigned int)i >= (unsigned int)m_pStructure->nJoints)
		{
			return 0;
		}
		for (iop = 0; iop < 2; iop++)
		{
			if ((params->partid[iop] = m_pStructure->pJoints[i].ipart[iop]) >= 0)
			{
				params->partid[iop] = m_parts[params->partid[iop]].id;
			}
		}
		params->pt = m_pStructure->pJoints[i].pt;
		params->n = m_pStructure->pJoints[i].n;
		params->maxForcePush = m_pStructure->pJoints[i].maxForcePush;
		params->maxForcePull = m_pStructure->pJoints[i].maxForcePull;
		params->maxForceShift = m_pStructure->pJoints[i].maxForceShift;
		params->maxTorqueBend = m_pStructure->pJoints[i].maxTorqueBend;
		params->maxTorqueTwist = m_pStructure->pJoints[i].maxTorqueTwist;
		params->bBreakable = m_pStructure->pJoints[i].bBreakable;
		params->bBroken = m_pStructure->pJoints[i].bBroken;
		return 1;
	}

	if (_params->type == pe_params_timeout::type_id)
	{
		pe_params_timeout* params = (pe_params_timeout*)_params;
		params->timeIdle = m_timeIdle;
		params->maxTimeIdle = m_maxTimeIdle;
		return 1;
	}

	return 0;
}

int CPhysicalEntity::GetStatus(pe_status* _status)
{
	if (_status->type == pe_status_pos::type_id)
	{
		pe_status_pos* status = (pe_status_pos*)_status;
		ReadLockCond lock(m_lockUpdate, (status->flags ^ status_thread_safe) & status_thread_safe &
		                                    -m_pWorld->m_vars.bLogStructureChanges);
		Vec3 respos;
		quaternionf resq;
		float resscale;
		int i;

		if (status->ipart == -1 && status->partid >= 0)
		{
			for (i = 0; i < m_nParts && m_parts[i].id != status->partid; i++)
				;
			if (i == m_nParts)
			{
				return 0;
			}
		}
		else
		{
			i = status->ipart;
		}

		if (i < 0)
		{
			respos = m_pos;
			resq = m_qrot;
			resscale = 1.0f;
			for (i = 0, status->flagsOR = 0, status->flagsAND = 0xFFFFFFFF; i < m_nParts; i++)
			{
				status->flagsOR |= m_parts[i].flags, status->flagsAND &= m_parts[i].flags;
			}
			if (m_nParts > 0)
			{
				status->pGeom = m_parts[0].pPhysGeom->pGeom;
				status->pGeomProxy = m_parts[0].pPhysGeomProxy->pGeom;
			}
			else
			{
				status->pGeom = status->pGeomProxy = 0;
			}
			status->BBox[0] = m_BBox[0] - m_pos;
			status->BBox[1] = m_BBox[1] - m_pos;
		}
		else if (i < m_nParts)
		{
			if (status->flags & status_local)
			{
				respos = m_parts[i].pos;
				resq = m_parts[i].q;
			}
			else
			{
				respos = m_pos + m_qrot * m_parts[i].pos;
				resq = m_qrot * m_parts[i].q;
			}
			resscale = m_parts[i].scale;
			status->partid = m_parts[i].id;
			status->flagsOR = status->flagsAND = m_parts[i].flags;
			status->pGeom = m_parts[i].pPhysGeom->pGeom;
			status->pGeomProxy = m_parts[i].pPhysGeomProxy->pGeom;
			if (status->flags & status_addref_geoms)
			{
				if (status->pGeom)
				{
					status->pGeom->AddRef();
				}
				if (status->pGeomProxy)
				{
					status->pGeom->AddRef();
				}
			}
			status->BBox[0] = m_parts[i].BBox[0] - respos;
			status->BBox[1] = m_parts[i].BBox[1] - respos;
		}
		else
		{
			return 0;
		}

		status->pos = respos;
		status->q = resq;
		status->scale = resscale;
		status->iSimClass = m_iSimClass;

		if (status->pMtx3x4)
		{
			(*status->pMtx3x4 = Matrix33(resq) * resscale).SetTranslation(respos);
		}
		if (status->pMtx3x3)
		{
			(Matrix33&)* status->pMtx3x3 = (Matrix33(resq) * resscale);
		}
		return 1;
	}

	if (_status->type == pe_status_id::type_id)
	{
		pe_status_id* status = (pe_status_id*)_status;
		ReadLock lock(m_lockUpdate);
		int ipart = status->ipart;
		if (ipart < 0)
		{
			for (ipart = 0; ipart < m_nParts - 1 && m_parts[ipart].id != status->partid; ipart++)
				;
		}
		if (ipart >= m_nParts)
		{
			return 0;
		}
		phys_geometry* pgeom = status->bUseProxy ? m_parts[ipart].pPhysGeomProxy : m_parts[ipart].pPhysGeom;
		if ((unsigned int)status->iPrim >= (unsigned int)pgeom->pGeom->GetPrimitiveCount() ||
		    pgeom->pGeom->GetType() == GEOM_TRIMESH && status->iFeature > 2)
		{
			return 0;
		}
		status->id = pgeom->pGeom->GetPrimitiveId(status->iPrim, status->iFeature);
		status->id = (status->id & ~(status->id >> 31)) | (m_parts[ipart].surface_idx & status->id >> 31);
		return 1;
	}

	if (_status->type == pe_status_nparts::type_id)
	{
		return m_nParts;
	}

	if (_status->type == pe_status_awake::type_id)
	{
		return IsAwake();
	}

	if (_status->type == pe_status_contains_point::type_id)
	{
		return IsPointInside(((pe_status_contains_point*)_status)->pt);
	}

	if (_status->type == pe_status_caps::type_id)
	{
		pe_status_caps* status = (pe_status_caps*)_status;
		status->bCanAlterOrientation = 0;
		return 1;
	}

	if (_status->type == pe_status_extent::type_id)
	{
		pe_status_extent* status = (pe_status_extent*)_status;
		assert(status->pGeo);
		status->pGeo->GetExtent(this, status->eForm);
		return 1;
	}

	if (_status->type == pe_status_random::type_id)
	{
		pe_status_random* status = (pe_status_random*)_status;
		assert(status->pGeo);
		GetRandomPos(status->ran, *status->pGeo, status->eForm);
		return 1;
	}

	return 0;
};

int CPhysicalEntity::Action(pe_action* _action, int bThreadSafe)
{
	ChangeRequest<pe_action> req(this, m_pWorld, _action, bThreadSafe);
	if (req.IsQueued())
	{
		return 1;
	}

	if (_action->type == pe_action_impulse::type_id)
	{
		pe_action_impulse* action = (pe_action_impulse*)_action;
		if (!is_unused(action->ipart) || !is_unused(action->partid))
		{
			int i;
			if (is_unused(action->ipart))
			{
				for (i = 0; i < m_nParts && m_parts[i].id != action->partid; i++)
					;
			}
			else
			{
				i = action->ipart;
			}
			if ((unsigned int)i >= (unsigned int)m_nParts)
			{
				return 0;
			}

			float scale = 1.0f + ((m_pWorld->m_vars.breakImpulseScale - 1.0f) *
			                      (iszero((int)m_flags & pef_override_impulse_scale) |
			                       m_pWorld->m_vars.bMultiplayer));
			if (m_pStructure)
			{
				m_pStructure->Pext[i] += action->impulse * scale;
				Vec3 Lext0 = m_pStructure->Lext[i];
				if (!is_unused(action->angImpulse))
				{
					m_pStructure->Lext[i] += action->angImpulse * scale;
				}
				else if (!is_unused(action->point))
				{
					Vec3 bodypos =
					    m_qrot * (m_parts[i].q * m_parts[i].pPhysGeom->origin * m_parts[i].scale +
					              m_parts[i].pos) +
					    m_pos;
					m_pStructure->Lext[i] += action->point - bodypos ^
					                         action->impulse * m_pWorld->m_vars.breakImpulseScale;
				}
				Vec3 sz = m_parts[i].BBox[1] - m_parts[i].BBox[0];
				if (max(m_pStructure->Pext[i].len2(),
				        m_pStructure->Lext[i].len2() * sqr(sz.x + sz.y + sz.z) * 0.1f) >
				    sqr(m_parts[i].mass * 0.01f) * m_pWorld->m_vars.gravity.len2())
				{
					m_pWorld->MarkEntityAsDeforming(this);
				}
				if (action->iSource == 2 && m_pWorld->m_vars.bDebugExplosions)
				{
					if (!m_pStructure->Pexpl)
					{
						m_pStructure->Pexpl = new Vec3[m_nParts];
					}
					if (!m_pStructure->Lexpl)
					{
						m_pStructure->Lexpl = new Vec3[m_nParts];
					}
					if ((m_pStructure->lastExplPos - m_pWorld->m_lastEpicenter).len2())
					{
						memset(m_pStructure->Pexpl, 0, m_nParts * sizeof(Vec3));
						memset(m_pStructure->Lexpl, 0, m_nParts * sizeof(Vec3));
					}
					m_pStructure->lastExplPos = m_pWorld->m_lastEpicenter;
					m_pStructure->Pexpl[i] += action->impulse * scale;
					m_pStructure->Lexpl[i] += m_pStructure->Lext[i] - Lext0;
				}
			}

			if (m_parts[i].pLattice && m_parts[i].idmatBreakable >= 0)
			{
				float rdensity = m_parts[i].pLattice->m_density * m_parts[i].pPhysGeom->V *
				                 cube(m_parts[i].scale) / m_parts[i].mass;
				Vec3 ptloc = ((action->point - m_pos) * m_qrot - m_parts[i].pos) * m_parts[i].q,
				     P(ZERO), L(ZERO);
				rdensity *= scale;
				if (m_parts[i].scale != 1.0f)
				{
					ptloc /= m_parts[i].scale;
				}
				if (!is_unused(action->impulse))
				{
					P = ((action->impulse * rdensity) * m_qrot) * m_parts[i].q;
				}
				if (!is_unused(action->angImpulse))
				{
					L = ((action->angImpulse * rdensity) * m_qrot) * m_parts[i].q;
				}
				if (m_parts[i].pLattice->AddImpulse(ptloc, P, L, m_pWorld->m_vars.gravity,
				                                    m_pWorld->m_updateTimes[0]) &&
				    !(m_flags & pef_deforming))
				{
					m_updStage = 0;
					m_pWorld->MarkEntityAsDeforming(this);
				}
			}
		}

		return 1;
	}

	if (_action->type == pe_action_awake::type_id && m_iSimClass >= 0 && m_iSimClass < 7)
	{
		Awake(((pe_action_awake*)_action)->bAwake, 1);
		return 1;
	}
	if (_action->type == pe_action_remove_all_parts::type_id)
	{
		for (int i = m_nParts - 1; i >= 0; i--)
		{
			RemoveGeometry(m_parts[i].id);
		}
		return 1;
	}

	if (_action->type == pe_action_reset_part_mtx::type_id)
	{
		pe_action_reset_part_mtx* action = (pe_action_reset_part_mtx*)_action;
		int i = 0;
		if (!is_unused(action->partid))
		{
			for (i = m_nParts - 1; i > 0 && m_parts[i].id != action->partid; i--)
				;
		}
		else if (!is_unused(action->ipart))
		{
			i = action->ipart;
		}

		pe_params_pos pp;
		pp.pos = m_pos + m_qrot * m_parts[i].pos;
		pp.q = m_qrot * m_parts[i].q;
		pp.bRecalcBounds = 0;
		SetParams(&pp, 1);

		pe_params_part ppt;
		ppt.ipart = i;
		ppt.pos.zero();
		ppt.q.SetIdentity();
		SetParams(&ppt, 1);
		return 1;
	}

	if (_action->type == pe_action_auto_part_detachment::type_id)
	{
		pe_action_auto_part_detachment* action = (pe_action_auto_part_detachment*)_action;
		if (!is_unused(action->threshold))
		{
			pe_params_structural_joint psj;
			pe_params_buoyancy pb;
			Vec3 gravity;
			float glen;
			if (!m_pWorld->CheckAreas(this, gravity, &pb) || is_unused(gravity))
			{
				gravity = m_pWorld->m_vars.gravity;
			}
			glen = gravity.len();
			psj.partid[1] = -1;
			psj.pt.zero();
			psj.n = gravity / (glen > 0 ? -glen : 1.0f);
			psj.maxTorqueBend = psj.maxTorqueTwist = 1E20f;
			for (int i = 0; i < m_nParts; i++)
			{
				psj.partid[0] = m_parts[i].id;
				psj.maxForcePush = psj.maxForcePull = psj.maxForceShift =
				    m_parts[i].mass * glen * action->threshold;
				SetParams(&psj);
			}
			m_flags |= aef_recorded_physics;
		}
		if (m_pStructure && m_flags & aef_recorded_physics)
		{
			m_pStructure->autoDetachmentDist = action->autoDetachmentDist;
		}
		return 1;
	}

	if (_action->type == pe_action_move_parts::type_id)
	{
		pe_action_move_parts* action = (pe_action_move_parts*)_action;
		int i, j, iop;
		pe_geomparams gp;
		pe_params_structural_joint psj;
		Matrix34 mtxPart;
		gp.pMtx3x4 = &mtxPart;
		if (action->pTarget)
		{
			for (i = m_nParts - 1; i >= 0; i--)
			{
				if (m_parts[i].id >= action->idStart && m_parts[i].id <= action->idEnd)
				{
					gp.mass = m_parts[i].mass;
					gp.flags = m_parts[i].flags;
					gp.flagsCollider = m_parts[i].flagsCollider;
					mtxPart = action->mtxRel *
					          Matrix34(Vec3(m_parts[i].scale), m_parts[i].q, m_parts[i].pos);
					gp.pLattice = m_parts[i].pLattice;
					gp.idmatBreakable = m_parts[i].idmatBreakable;
					gp.pMatMapping = m_parts[i].pMatMapping;
					gp.nMats = m_parts[i].nMats;
					action->pTarget->AddGeometry(m_parts[i].pPhysGeom, &gp,
					                             m_parts[i].id + action->idOffset, 1);

					if (m_pStructure)
					{
						for (j = 0; j < m_pStructure->nJoints; j++)
						{
							if (((iop = m_pStructure->pJoints[j].ipart[1] == i) ||
							     m_pStructure->pJoints[j].ipart[0] == i) &&
							    (m_pStructure->pJoints[j].ipart[iop ^ 1] == -1 ||
							     m_parts[m_pStructure->pJoints[j].ipart[iop ^ 1]].id >=
							             action->idStart &&
							         m_parts[m_pStructure->pJoints[j].ipart[iop ^ 1]].id <=
							             action->idEnd))
							{
								psj.id = m_pStructure->pJoints[j].id;
								psj.partid[iop] = m_parts[i].id + action->idOffset;
								if (m_pStructure->pJoints[j].ipart[iop ^ 1] >= 0)
								{
									psj.partid[iop ^ 1] =
									    m_parts[m_pStructure->pJoints[j]
									                .ipart[iop ^ 1]]
										.id +
									    action->idOffset;
								}
								else
								{
									psj.partid[iop ^ 1] = -1;
								}
								psj.pt = m_pStructure->pJoints[j].pt;
								psj.n = m_pStructure->pJoints[j].n;
								psj.maxForcePush =
								    m_pStructure->pJoints[j].maxForcePush;
								psj.maxForcePull =
								    m_pStructure->pJoints[j].maxForcePull;
								psj.maxForceShift =
								    m_pStructure->pJoints[j].maxForceShift;
								psj.maxTorqueBend =
								    m_pStructure->pJoints[j].maxTorqueBend;
								psj.maxTorqueTwist =
								    m_pStructure->pJoints[j].maxTorqueTwist;
								psj.bBreakable = m_pStructure->pJoints[j].bBreakable;
								psj.szSensor = m_pStructure->pJoints[j].size;
								action->pTarget->SetParams(&psj);
							}
						}
					}
				}
			}
		}
		for (i = m_nParts - 1; i >= 0; i--)
		{
			if (m_parts[i].id >= action->idStart && m_parts[i].id <= action->idEnd)
			{
				RemoveGeometry(m_parts[i].id);
			}
		}
		return 1;
	}

	return 0;
}

float CPhysicalEntity::ComputeExtent(GeomQuery& geo, EGeomForm eForm)
{
	// iterate parts, getting an extent and random point from each.
	float fExtent = 0.f;
	geo.AllocParts(m_nParts);
	for (int i = 0; i < m_nParts; i++)
	{
		if (m_parts[i].pPhysGeom && m_parts[i].pPhysGeom->pGeom)
		{
			fExtent += geo.GetPart(i).GetExtent(m_parts[i].pPhysGeom->pGeom, eForm);
		}
	}
	return fExtent;
}

void CPhysicalEntity::GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm)
{
	if (m_nParts == 0)
	{
		ran.vPos.zero();
		ran.vNorm.zero();
		return;
	}

	geo.GetExtent(this, eForm);
	int iPart = geo.GetRandomPart();
	assert(iPart >= 0);

	m_parts[iPart].pPhysGeom->pGeom->GetRandomPos(ran, geo.GetPart(iPart), eForm);

	// transform to world.
	Quat q = m_qrot * m_parts[iPart].q;
	ran.vNorm = q * ran.vNorm;
	ran.vPos = q * ran.vPos * (m_parts[iPart].scale) + m_parts[iPart].pos + m_pos;
}

void CPhysicalEntity::UpdatePartIdmatBreakable(int ipart, int nParts)
{
	int i, id, flags = 0;
	m_parts[ipart].idmatBreakable = -1;
	m_parts[ipart].flags &= ~geom_manually_breakable;
	nParts += (m_nParts + 1) & nParts >> 31;

	if (m_parts[ipart].pMatMapping)
	{
		for (i = 0; i < m_parts[ipart].nMats; i++)
		{
			id = m_pWorld
			         ->m_SurfaceFlagsTable[min(NSURFACETYPES - 1, max(0, m_parts[ipart].pMatMapping[i]))];
			m_parts[ipart].idmatBreakable =
			    max(m_parts[ipart].idmatBreakable, (id >> sf_matbreakable_bit) - 1);
			flags |= id;
		}
		if ((m_parts[ipart].idmatBreakable >= 0 || flags & sf_manually_breakable) && nParts > 1)
		{
			m_parts[ipart].idmatBreakable = -1;
			flags = 0;
			for (i = m_parts[ipart].pPhysGeom->pGeom->GetPrimitiveCount() - 1; i >= 0; i--)
			{
				id = m_parts[ipart].pPhysGeom->pGeom->GetPrimitiveId(i, 0x40);
				id += (m_parts[ipart].surface_idx + 1) & id >> 31;
				id = m_pWorld->m_SurfaceFlagsTable[m_parts[ipart]
				                                       .pMatMapping[min(m_parts[ipart].nMats - 1, id)]];
				m_parts[ipart].idmatBreakable =
				    max(m_parts[ipart].idmatBreakable, (id >> sf_matbreakable_bit) - 1);
				flags |= id;
			}
		}
	}
	else
	{
		for (i = m_parts[ipart].pPhysGeom->pGeom->GetPrimitiveCount() - 1; i >= 0; i--)
		{
			id = m_parts[ipart].pPhysGeom->pGeom->GetPrimitiveId(i, 0x40);
			id += (m_parts[ipart].surface_idx + 1) & id >> 31;
			id = m_pWorld->m_SurfaceFlagsTable[id];
			m_parts[ipart].idmatBreakable =
			    max(m_parts[ipart].idmatBreakable, (id >> sf_matbreakable_bit) - 1);
			flags |= id;
		}
	}
	if (m_parts[ipart].idmatBreakable >= 0 && m_parts[ipart].pPhysGeom->pGeom->GetType() != GEOM_TRIMESH)
	{
		m_parts[ipart].idmatBreakable = -1, flags &= ~sf_manually_breakable;
	}

	if (flags & sf_manually_breakable)
	{
		m_parts[ipart].flags |= geom_manually_breakable;
	}
}

int CPhysicalEntity::AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id, int bThreadSafe)
{
	if (!pgeom || !pgeom->pGeom)
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

	primitives::box abox;
	pgeom->pGeom->GetBBox(&abox);
	float mindim = min(min(abox.size.x, abox.size.y), abox.size.z), mindims = mindim * params->scale;
	float maxdim = max(max(abox.size.x, abox.size.y), abox.size.z), maxdims = maxdim * params->scale;

	if (id >= 0)
	{
		int i;
		for (i = 0; i < m_nParts && m_parts[i].id != id; i++)
			;
		if (i < m_nParts)
		{
			if (params->flags & geom_proxy)
			{
				if (pgeom)
				{
					if (m_parts[i].pPhysGeomProxy != pgeom)
					{
						m_pWorld->GetGeomManager()->AddRefGeometry(pgeom);
					}
					m_parts[i].pPhysGeomProxy = pgeom;
					if (params->bRecalcBBox)
					{
						Vec3 BBox[2];
						ComputeBBox(BBox);
						int bPosChanged = m_pWorld->RepositionEntity(this, 1, BBox);
						{
							WriteLock lock(m_lockUpdate);
							WriteBBox(BBox);
							AtomicAdd(&m_pWorld->m_lockGrid, -bPosChanged);
						}
					}
				}
			}
			return id;
		}
	}
	get_xqs_from_matrices(params->pMtx3x4, params->pMtx3x3, params->pos, params->q, params->scale);
	if (m_nParts == m_nPartsAlloc)
	{
		WriteLock lock(m_lockUpdate);
		geom* pparts = m_parts;
		memcpy(m_parts = new geom[m_nPartsAlloc = (m_nPartsAlloc + 4) & ~3], pparts, sizeof(geom) * m_nParts);
		for (int i = 0; i < m_nParts; i++)
		{
			if (m_parts[i].pNewCoords == (coord_block_BBox*)&pparts[i].pos)
			{
				m_parts[i].pNewCoords = (coord_block_BBox*)&m_parts[i].pos;
			}
		}
		if (pparts != &m_defpart)
		{
			delete[] pparts;
		}
	}
	{
		WriteLock lock(m_lockPartIdx);
		if (id < 0)
		{
			id = m_iLastIdx++;
		}
		else
		{
			m_iLastIdx = max(m_iLastIdx, id + 1);
		}
	}
	m_parts[m_nParts].id = id;
	m_parts[m_nParts].pPhysGeom = m_parts[m_nParts].pPhysGeomProxy = pgeom;
	m_pWorld->GetGeomManager()->AddRefGeometry(pgeom);
	m_parts[m_nParts].surface_idx = pgeom->surface_idx;
	if (!is_unused(params->surface_idx))
	{
		m_parts[m_nParts].surface_idx = params->surface_idx;
	}
	m_parts[m_nParts].flags = params->flags & ~geom_proxy;
	m_parts[m_nParts].flagsCollider = params->flagsCollider;
	m_parts[m_nParts].pos = params->pos;
	m_parts[m_nParts].q = params->q;
	m_parts[m_nParts].scale = params->scale;
	if (is_unused(params->minContactDist))
	{
		m_parts[m_nParts].minContactDist =
		    max(maxdims * 0.03f, mindims * (mindims < maxdims * 0.3f ? 0.4f : 0.1f));
	}
	else
	{
		m_parts[m_nParts].minContactDist = params->minContactDist;
	}
	m_parts[m_nParts].maxdim = maxdim;
	m_parts[m_nParts].pNewCoords = (coord_block_BBox*)&m_parts[m_nParts].pos;
	if (pgeom->pGeom->GetType() == GEOM_TRIMESH && params->pLattice)
	{
		(m_parts[m_nParts].pLattice = (CTetrLattice*)params->pLattice)->SetMesh((CTriMesh*)pgeom->pGeom);
	}
	else
	{
		m_parts[m_nParts].pLattice = 0;
	}
	if (params->pMatMapping)
	{
		memcpy(m_parts[m_nParts].pMatMapping = new int[params->nMats], params->pMatMapping,
		       (m_parts[m_nParts].nMats = params->nMats) * sizeof(int));
	}
	else
	{
		m_parts[m_nParts].pMatMapping = pgeom->pMatMapping;
		m_parts[m_nParts].nMats = pgeom->nMats;
	}
	if (!is_unused(params->idmatBreakable))
	{
		m_parts[m_nParts].idmatBreakable = params->idmatBreakable;
	}
	else if (m_parts[m_nParts].flags & geom_colltype_solid)
	{
		UpdatePartIdmatBreakable(m_nParts, m_nParts + 1);
		if (m_nParts == 1)
		{
			UpdatePartIdmatBreakable(0, 2);
		}
	}
	else
	{
		m_parts[m_nParts].idmatBreakable = -1;
	}
	if (m_parts[m_nParts].idmatBreakable >= 0 && m_parts[m_nParts].pPhysGeom->pGeom->GetType() == GEOM_TRIMESH)
	{
		CTriMesh* pMesh = (CTriMesh*)m_parts[m_nParts].pPhysGeom->pGeom;
		if (pMesh->m_pTree->GetMaxSkipDim() > 0.05f)
		{
			pMesh->Lock();
			pMesh->RebuildBVTree();
			pMesh->Unlock();
		}
	}
	m_parts[m_nParts].mass = params->mass > 0 ? params->mass : params->density * pgeom->V * cube(params->scale);
	if (m_pStructure || m_parts[m_nParts].pLattice)
	{
		m_parts[m_nParts].flags |= geom_monitor_contacts;
	}
	if (m_pStructure && m_pStructure->nPartsAlloc < m_nParts + 1)
	{
		ReallocateList(m_pStructure->Pext, m_nParts, m_nParts + 1, true);
		ReallocateList(m_pStructure->Lext, m_nParts, m_nParts + 1, true);
		ReallocateList(m_pStructure->Fext, m_nParts, m_nParts + 1, true);
		ReallocateList(m_pStructure->Text, m_nParts, m_nParts + 1, true);
		m_pStructure->nPartsAlloc = m_nParts + 1;
	}

	if (params->bRecalcBBox)
	{
		Vec3 BBox[2];
		ComputeBBox(BBox, update_part_bboxes | part_added);
		int bPosChanged = m_pWorld->RepositionEntity(this, 1, BBox);
		{
			WriteLock lock(m_lockUpdate);
			WriteBBox(BBox);
			m_nParts++;
			AtomicAdd(&m_pWorld->m_lockGrid, -bPosChanged);
		}
	}
	else
	{
		WriteLock lock(m_lockUpdate);
		m_nParts++;
	}

	return id;
}

void CPhysicalEntity::RemoveGeometry(int id, int bThreadSafe)
{
	ChangeRequest<void> req(this, m_pWorld, 0, bThreadSafe, 0, id);
	if (req.IsQueued())
	{
		return;
	}
	int i, j;

	for (i = 0; i < m_nParts; i++)
	{
		if (m_parts[i].id == id)
		{
			if (m_pStructure)
			{
				// ReallocateList(m_pStructure->Pext,0,max(1,m_nParts-1),true);
				// ReallocateList(m_pStructure->Lext,0,max(1,m_nParts-1),true);
				for (j = 0; j < m_pStructure->nJoints; j++)
				{
					if (m_pStructure->pJoints[j].ipart[0] == i ||
					    m_pStructure->pJoints[j].ipart[1] == i)
					{
						m_pStructure->pJoints[j] =
						    m_pStructure->pJoints[--m_pStructure->nJoints];
						--j;
						m_pStructure->bModified++;
					}
				}
				for (j = 0; j < m_pStructure->nJoints; j++)
				{
					m_pStructure->pJoints[j].ipart[0] -=
					    isneg(i - m_pStructure->pJoints[j].ipart[0]);
					m_pStructure->pJoints[j].ipart[1] -=
					    isneg(i - m_pStructure->pJoints[j].ipart[1]);
				}
				if (m_pStructure->Pexpl)
				{
					memmove(m_pStructure->Pexpl + i, m_pStructure->Pexpl + i + 1,
					        (m_nParts - 1 - i) * sizeof(Vec3));
				}
				if (m_pStructure->Lexpl)
				{
					memmove(m_pStructure->Lexpl + i, m_pStructure->Lexpl + i + 1,
					        (m_nParts - 1 - i) * sizeof(Vec3));
				}
			}
			{
				WriteLock lock(m_lockUpdate);
				if (m_parts[i].pMatMapping &&
				    m_parts[i].pMatMapping != m_parts[i].pPhysGeom->pMatMapping)
				{
					delete[] m_parts[i].pMatMapping;
				}
				m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeom);
				if (m_parts[i].pPhysGeomProxy != m_parts[i].pPhysGeom)
				{
					m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeomProxy);
				}
				for (; i < m_nParts - 1; i++)
				{
					m_parts[i] = m_parts[i + 1];
					if (m_parts[i].pNewCoords == (coord_block_BBox*)&m_parts[i + 1].pos)
					{
						m_parts[i].pNewCoords = (coord_block_BBox*)&m_parts[i].pos;
					}
				}
				m_nParts--;
				ComputeBBox(m_BBox);
				for (m_iLastIdx = i = 0; i < m_nParts; i++)
				{
					m_iLastIdx = max(m_iLastIdx, m_parts[i].id + 1);
				}
			}
			AtomicAdd(&m_pWorld->m_lockGrid, -m_pWorld->RepositionEntity(this, 1));
			return;
		}
	}
}

RigidBody* CPhysicalEntity::GetRigidBody(int ipart, int bWillModify)
{
	g_StaticRigidBody.P.zero();
	g_StaticRigidBody.v.zero();
	g_StaticRigidBody.L.zero();
	g_StaticRigidBody.w.zero();
	return &g_StaticRigidBody;
}

int CPhysicalEntity::IsPointInside(Vec3 pt)
{
	pt = (pt - m_pos) * m_qrot;
	if (m_pBoundingGeometry)
	{
		return m_pBoundingGeometry->PointInsideStatus(pt);
	}
	ReadLock lock(m_lockUpdate);
	for (int i = 0; i < m_nParts; i++)
	{
		if ((m_parts[i].flags & geom_collides) &&
		    m_parts[i].pPhysGeom->pGeom->PointInsideStatus(((pt - m_parts[i].pos) * m_parts[i].q) /
		                                                   m_parts[i].scale))
		{
			return 1;
		}
	}
	return 0;
}

void CPhysicalEntity::AlertNeighbourhoodND()
{
	int i;
	if (m_iSimClass > 3)
	{
		return;
	}

	for (i = 0; i < m_nColliders; i++)
	{
		if (m_pColliders[i] != this)
		{
			m_pColliders[i]->RemoveCollider(this);
			m_pColliders[i]->Awake();
			m_pColliders[i]->Release();
		}
	}
	m_nColliders = 0;

	if (!(m_flags & pef_disabled) &&
	    (m_nRefCount > 0 && m_iSimClass < 3 || m_flags & pef_always_notify_on_deletion))
	{
		CPhysicalEntity** pentlist;
		Vec3 inflator = (m_BBox[1] - m_BBox[0]) * 1E-3f + Vec3(4, 4, 4) * m_pWorld->m_vars.maxContactGap;
		for (i = m_pWorld->GetEntitiesAround(m_BBox[0] - inflator, m_BBox[1] + inflator, pentlist,
		                                     ent_sleeping_rigid | ent_rigid | ent_living | ent_independent |
		                                         ent_triggers) -
		         1;
		     i >= 0; i--)
		{
			pentlist[i]->Awake();
		}
	}
}

int CPhysicalEntity::RemoveCollider(CPhysicalEntity* pCollider, bool bRemoveAlways)
{
	if (m_pColliders && m_iSimClass != 0)
	{
		int i, islot;
		for (i = 0; i < m_nColliders && m_pColliders[i] != pCollider; i++)
			;
		if (i < m_nColliders)
		{
			for (islot = i; i < m_nColliders - 1; i++)
			{
				m_pColliders[i] = m_pColliders[i + 1];
			}
			if (pCollider != this)
			{
				pCollider->Release();
			}
			m_nColliders--;
			return islot;
		}
	}
	return -1;
}

int CPhysicalEntity::AddCollider(CPhysicalEntity* pCollider)
{
	if (m_iSimClass == 0)
	{
		return 1;
	}
	int i, j;
	for (i = 0; i < m_nColliders && m_pColliders[i] != pCollider; i++)
		;
	if (i == m_nColliders)
	{
		if (m_nColliders == m_nCollidersAlloc)
		{
			CPhysicalEntity** pColliders = m_pColliders;
			memcpy(m_pColliders = new CPhysicalEntity*[m_nCollidersAlloc += 8], pColliders,
			       sizeof(CPhysicalEntity*) * m_nColliders);
			if (pColliders)
			{
				delete[] pColliders;
			}
		}
		for (i = 0; i < m_nColliders && pCollider->GetMassInv() > m_pColliders[i]->GetMassInv(); i++)
			;
		for (j = m_nColliders - 1; j >= i; j--)
		{
			m_pColliders[j + 1] = m_pColliders[j];
		}
		m_pColliders[i] = pCollider;
		m_nColliders++;
		if (pCollider != this)
		{
			pCollider->AddRef();
		}
	}
	return i;
}

void CPhysicalEntity::DrawHelperInformation(IPhysRenderer* pRenderer, int flags)
{
	// ReadLock lock(m_lockUpdate);
	int i, j;
	geom_world_data gwd;

	if (flags & pe_helper_bbox)
	{
		Vec3 sz, center, pt[8];
		center = (m_BBox[1] + m_BBox[0]) * 0.5f;
		sz = (m_BBox[1] - m_BBox[0]) * 0.5f;
		for (i = 0; i < 8; i++)
		{
			pt[i] =
			    Vec3(sz.x * ((i << 1 & 2) - 1), sz.y * ((i & 2) - 1), sz.z * ((i >> 1 & 2) - 1)) + center;
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

	if (flags & (pe_helper_geometry | pe_helper_lattice))
	{
		int iLevel = flags >> 16, mask = 0,
		    bTransp = m_pStructure && (flags & 16 || m_pWorld->m_vars.bLogLatticeTension);
		if (iLevel & 1 << 11)
		{
			mask = iLevel & 0xF7FF, iLevel = 0;
		}
		for (i = 0; i < m_nParts; i++)
		{
			if ((m_parts[i].flags & mask) == static_cast<unsigned int>(mask) &&
			    (m_parts[i].flags || m_parts[i].flagsCollider || m_parts[i].mass > 0))
			{
				gwd.R = Matrix33(m_qrot * m_parts[i].q);
				gwd.offset = m_pos + m_qrot * m_parts[i].pos;
				gwd.scale = m_parts[i].scale;
				if (flags & pe_helper_geometry)
				{
					if (m_parts[i].pPhysGeomProxy == m_parts[i].pPhysGeom ||
					    mask != geom_colltype_ray)
					{
						m_parts[i].pPhysGeomProxy->pGeom->DrawWireframe(
						    pRenderer, &gwd, iLevel,
						    m_iSimClass | (m_parts[i].flags & 1 ^ 1 | bTransp) << 8);
					}
					if (m_parts[i].pPhysGeomProxy != m_parts[i].pPhysGeom &&
					    (mask & geom_colltype_ray) == geom_colltype_ray)
					{
						m_parts[i].pPhysGeom->pGeom->DrawWireframe(pRenderer, &gwd, iLevel,
						                                           m_iSimClass | 1 << 8);
					}
				}
				if ((flags & pe_helper_lattice) && m_parts[i].pLattice)
				{
					m_parts[i].pLattice->DrawWireframe(pRenderer, &gwd, m_iSimClass);
				}
			}
		}

		if (m_pWorld->m_vars.bLogLatticeTension && !m_pStructure && GetMassInv() > 0)
		{
			char txt[32];
			sprintf(txt, "%.2fkg", 1.0f / GetMassInv());
			pRenderer->DrawText(m_pos, txt, 1);
		}
	}

	if ((m_pWorld->m_vars.bLogLatticeTension || flags & 16) && m_pStructure)
	{
		CTriMesh mesh;
		CBoxGeom boxGeom;
		primitives::box abox;
		Vec3 norms[4],
		    vtx[] = {Vec3(0, 0.2f, 0), Vec3(-0.1732f, -0.1f, 0), Vec3(0.1732f, -0.1f, 0), Vec3(0, 0, 1)};
		int ipart, idx[] = {0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3};
		trinfo top[4];
		primitives::box bbox;
		for (i = 0; i < 4; i++)
		{
			norms[i] = (vtx[idx[(i * 3) + 1]] - vtx[idx[i * 3]] ^ vtx[idx[(i * 3) + 2]] - vtx[idx[i * 3]])
			               .normalized();
		}
		memset(top, -1, 4 * sizeof(top[0]));
		mesh.m_flags =
		    mesh_shared_vtx | mesh_shared_idx | mesh_shared_normals | mesh_shared_topology | mesh_SingleBB;
		mesh.m_pVertices = vtx;
		mesh.m_pIndices = idx;
		mesh.m_pNormals = norms;
		mesh.m_pTopology = top;
		mesh.m_nTris = mesh.m_nVertices = 4;
		mesh.m_ConvexityTolerance[0] = 0.02f;
		mesh.m_bConvex[0] = 1;
		mesh.RebuildBVTree();
		abox.Basis.SetIdentity();
		abox.center.zero();
		abox.size.Set(0.5f, 0.5f, 0.5f);
		boxGeom.CreateBox(&abox);
		for (i = 0; i < m_pStructure->nJoints; i++)
		{
			gwd.offset = m_qrot * m_pStructure->pJoints[i].pt + m_pos;
			gwd.R = Matrix33(m_qrot * Quat::CreateRotationV0V1(Vec3(0, 0, 1), m_pStructure->pJoints[i].n));
			gwd.scale = m_pStructure->pJoints[i].size * 2;
			j = 4 + (2 * isneg(m_pStructure->pJoints[i].ipart[0] | m_pStructure->pJoints[i].ipart[1]));
			pRenderer->DrawGeometry(&mesh, &gwd, j);
			gwd.scale = m_pStructure->pJoints[i].size;
			pRenderer->DrawGeometry(&boxGeom, &gwd, j);
			for (j = 0; j < 2; j++)
			{
				if ((unsigned int)(ipart = m_pStructure->pJoints[i].ipart[j]) < (unsigned int)m_nParts)
				{
					m_parts[ipart].pPhysGeom->pGeom->GetBBox(&bbox);
					pRenderer->DrawLine(
					    gwd.offset,
					    m_qrot * (m_parts[ipart].q * bbox.center * m_parts[ipart].scale +
					              m_parts[ipart].pos) +
						m_pos,
					    m_iSimClass);
				}
			}
		}
		if (m_pWorld->m_vars.bLogLatticeTension)
		{
			char txt[32];
			if (m_nParts != 0)
			{
				for (i = 0; i < m_nParts; i++)
				{
					m_parts[i].pPhysGeom->pGeom->GetBBox(&bbox);
					sprintf(txt, "%.2fkg", m_parts[i].mass);
					pRenderer->DrawText(
					    m_qrot * (m_parts[i].q * bbox.center * m_parts[i].scale + m_parts[i].pos) +
						m_pos,
					    txt, 1);
				}
			}
			if (m_pStructure->nPrevJoints)
			{
				float rdt = 1.0f / m_pStructure->prevdt, tens, maxtens{};
				for (i = 0; i < m_pStructure->nPrevJoints; i++)
				{
					if (m_pStructure->pJoints[i].tension > 0)
					{
						tens = sqrt_tpl(m_pStructure->pJoints[i].tension) * rdt;
						switch (m_pStructure->pJoints[i].itens)
						{
							case 0:
								sprintf(txt, "twist: %.1f/%.1f", tens,
								        maxtens =
								            m_pStructure->pJoints[i].maxTorqueTwist);
								break;
							case 1:
								sprintf(txt, "bend: %.1f/%.1f", tens,
								        maxtens =
								            m_pStructure->pJoints[i].maxTorqueBend);
								break;
							case 2:
								sprintf(txt, "push: %.1f/%.1f", tens,
								        maxtens =
								            m_pStructure->pJoints[i].maxForcePush);
								break;
							case 3:
								sprintf(txt, "pull: %.1f/%.1f", tens,
								        maxtens =
								            m_pStructure->pJoints[i].maxForcePull);
								break;
							case 4:
								sprintf(txt, "shift: %.1f/%.1f", tens,
								        maxtens =
								            m_pStructure->pJoints[i].maxForceShift);
								break;
						}
						pRenderer->DrawText(m_qrot * m_pStructure->pJoints[i].pt + m_pos, txt,
						                    0, tens / max(tens, maxtens));
					}
				}
			}
		}
	}

	if (m_pWorld->m_vars.bDebugExplosions)
	{
		Vec3 imp;
		char str[128];
		primitives::box bbox;
		if (!m_pStructure || !m_pStructure->Pexpl)
		{
			if (m_pWorld->IsAffectedByExplosion(this, &imp))
			{
				sprintf(str, "%.1f", imp.len());
				pRenderer->DrawText(imp = (m_BBox[0] + m_BBox[1]) * 0.5f, str, 1, 0.5f);
				pRenderer->DrawLine(m_pWorld->m_lastEpicenter, imp, 5);
			}
		}
		else
		{
			for (i = 0; i < m_nParts; i++)
			{
				if (m_pStructure->Pexpl[i].len2() + m_pStructure->Lexpl[i].len2() > 0)
				{
					sprintf(str, "%.1f/%.1f", m_pStructure->Pexpl[i].len(),
					        m_pStructure->Lexpl[i].len());
					m_parts[i].pPhysGeom->pGeom->GetBBox(&bbox);
					pRenderer->DrawText(
					    imp = m_qrot *
					              (m_parts[i].q * bbox.center * m_parts[i].scale + m_parts[i].pos) +
					          m_pos,
					    str, 1, 0.5f);
					pRenderer->DrawLine(m_pWorld->m_lastEpicenter, imp, 5);
				}
			}
		}
	}
}

namespace {

void FillGeomParams(pe_geomparams& dst, geom& src)
{
	dst.flags = (src.flags | geom_can_modify) & ~(geom_invalid | geom_removed | geom_constraint_on_break);
	dst.flagsCollider = src.flagsCollider;
	dst.mass = src.mass;
	dst.pos = src.pos;
	dst.q = src.q;
	dst.scale = src.scale;
	dst.idmatBreakable = src.idmatBreakable;
	dst.minContactDist = src.minContactDist;
	dst.pMatMapping = src.pMatMapping != src.pPhysGeom->pMatMapping ? src.pMatMapping : 0;
	dst.nMats = src.nMats;
}

} // unnamed namespace

int CPhysicalEntity::UpdateStructure(float time_interval, pe_explosion* pexpl, int iCaller, Vec3 gravity)
{
	int i, j, i0, i1, flags = 0, nPlanes, ihead, itail, nIsles, iter, bBroken, nJoints, nJoints0, nMeshSplits = 0,
			  iLastIdx = m_iLastIdx;
	EventPhysUpdateMesh epum;
	EventPhysCreateEntityPart epcep;
	EventPhysRemoveEntityParts eprep;
	EventPhysJointBroken epjb;
	pe_geomparams gp, gpAux;
	pe_params_pos pp;
	pe_params_part ppt;
	CTriMesh *pMeshNew, *pMeshNext, *pMesh, *pChunkMeshesBuf[16], **pChunkMeshes = pChunkMeshesBuf;
	IGeometry *pMeshIsland, *pMeshIslandPrev;
	CTetrLattice *pChunkLatticesBuf[16], **pChunkLattices = pChunkLatticesBuf;
	CPhysicalEntity** pents;
	phys_geometry *pgeom, *pgeom0, *pgeom1;
	geom_world_data gwd, gwdAux;
	pe_action_impulse shockwave;
	pe_action_set_velocity asv;
	pe_explosion expl;
	primitives::plane ground[8];
	primitives::box bbox, bbox1;
	Vec3 org, sz, n, P, L, dw, dv, Ptang, Pmax, Lmax, BBoxNew[2];
	Matrix33 R;
	CSphereGeom sphGeom;
	// CBoxGeom boxGeom;
	intersection_params ip;
	geom_contact* pcontacts;
	bop_meshupdate* pmu;
	pe_params_buoyancy pb;
	pe_params_structural_joint psj;
	float t, Pn, vmax, M, V0, dt = time_interval, impTot;
	quotientf jtens;

	static int idRemoveGeoms[256];
	static phys_geometry *pRemoveGeoms[256], *pAddGeoms[16];
	static pe_geomparams gpAddGeoms[16];
	int nRemoveGeoms = 0, nAddGeoms = 0, bGeomOverflow = 0;

	epum.pEntity = epcep.pEntity = eprep.pEntity = epjb.pEntity[0] = epjb.pEntity[1] = this;
	epum.pForeignData = epcep.pForeignData = eprep.pForeignData = epjb.pForeignData[0] = epjb.pForeignData[1] =
	    m_pForeignData;
	epum.iForeignData = epcep.iForeignData = eprep.iForeignData = epjb.iForeignData[0] = epjb.iForeignData[1] =
	    m_iForeignData;
	epum.iReason = EventPhysUpdateMesh::ReasonFracture;
	pp.pos = m_pos;
	pp.q = m_qrot;
	nPlanes = min(m_nGroundPlanes, sizeof(ground) / sizeof(ground[0]));
	if (iCaller >= 0 && (!m_pWorld->CheckAreas(this, gravity, &pb, 1, Vec3(ZERO), iCaller) || is_unused(gravity)))
	{
		gravity = m_pWorld->m_vars.gravity;
	}
	static volatile int g_lockUpdateStructure = 0;
	WriteLock lock(g_lockUpdateStructure);
	epcep.breakImpulse.zero();
	epcep.breakAngImpulse.zero();
	epcep.cutRadius = 0;
	epcep.breakSize = 0;
	epjb.bJoint = 1;
	MARK_UNUSED epjb.pNewEntity[0], epjb.pNewEntity[1];
	if ((unsigned int)m_iSimClass >= 7u)
	{
		return 0;
	}

	{
		ReadLock lockUpd(m_lockUpdate);
		if (pexpl)
		{
			if (!m_pExpl)
			{
				m_pExpl = new SExplosionInfo;
			}
			m_pExpl->center = pexpl->epicenterImp;
			m_pExpl->dir = pexpl->explDir;
			m_pExpl->kr = pexpl->impulsivePressureAtR * sqr(pexpl->r);
			m_pExpl->rmin = pexpl->rmin;
			m_timeStructUpdate = m_pWorld->m_vars.tickBreakable;
			m_updStage = 1;
			m_nUpdTicks = 0;
			return 1;
		}

		for (i = m_nParts - 1; i >= 0; i--)
		{
			if (m_parts[i].idmatBreakable <= -2)
			{
				for (j = 0; j < i; j++)
				{
					if (-2 - m_parts[j].id == m_parts[i].idmatBreakable)
					{
						m_parts[j].flags |= m_parts[i].flags;
						m_parts[j].flagsCollider |= m_parts[i].flagsCollider;
					}
				}
				m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeom);
				--m_nParts;
			}
			else
			{
				flags |= m_parts[i].flags & geom_structure_changes;
			}
		}

		if (m_pStructure && m_nParts > 0)
		{
			if (g_nPartsAlloc < m_nParts + 2)
			{
				if (g_parts)
				{
					delete[] (g_parts - 1);
				}
				g_parts = (new SPartHelper[g_nPartsAlloc = m_nParts + 2]) + 1;
			}
			if (g_nJointsAlloc < m_pStructure->nJoints + 1)
			{
				if (g_joints)
				{
					delete[] g_joints;
				}
				if (g_jointidx)
				{
					delete[] g_jointidx;
				}
				g_joints = new SStructuralJointHelper[g_nJointsAlloc = m_pStructure->nJoints + 1];
				g_jointidx = new int[g_nJointsAlloc * 2];
				if (g_jointsDbg)
				{
					delete[] g_jointsDbg;
				}
				g_jointsDbg = new SStructuralJointDebugHelper[g_nJointsAlloc];
			}
			nJoints0 = m_pStructure->nJoints;

			// prepare part infos
			for (i = 0, vmax = -1, M = impTot = 0, g_parts[0].idx = -1, Pmax.zero(), Lmax.zero();
			     i < m_nParts; i++)
			{
				g_parts[i].org = m_parts[i].q * m_parts[i].pPhysGeom->origin + m_parts[i].pos;
				if (m_parts[i].mass > 0)
				{
					g_parts[i].Minv = 1.0f / m_parts[i].mass;
					Diag33 invIbody = m_parts[i].pPhysGeom->Ibody *
					                  (m_parts[i].mass / m_parts[i].pPhysGeom->V) *
					                  sqr(m_parts[i].scale);
					g_parts[i].Iinv =
					    Matrix33(m_parts[i].q) * invIbody.invert() * Matrix33(!m_parts[i].q);
				}
				else
				{
					g_parts[i].Minv = 0;
					g_parts[i].Iinv.SetZero();
				}
				P = m_pStructure->Pext[i] * dt * 100 + m_pStructure->Fext[i];
				L = m_pStructure->Lext[i] * dt * 100 + m_pStructure->Text[i];
				j = isneg(Pmax.len() + Lmax.len2() - P.len() - L.len2());
				Pmax = Pmax * (1 - j) + P * j;
				Lmax = Lmax * (1 - j) + L * j;
				g_parts[i].v =
				    (m_pStructure->Pext[i] * dt * 100 + m_pStructure->Fext[i]) * g_parts[i].Minv;
				g_parts[i].w =
				    g_parts[i].Iinv * (m_pStructure->Lext[i] * dt * 100 + m_pStructure->Text[i]);
				g_parts[i].v += gravity * (iszero((int)m_flags & aef_recorded_physics) * time_interval);
				n = m_parts[i].BBox[1] - m_parts[i].BBox[0];
				if ((Pn = g_parts[i].v.len2() + (g_parts[i].w.len2() * sqr(n.x + n.y + n.z) * 0.01f)) >
				    vmax)
				{
					vmax = Pn, g_parts[0].idx = i;
				}
				g_parts[i].bProcessed = 0;
				g_parts[i].ijoint0 = 0;
				M += m_parts[i].mass;
				impTot += m_pStructure->Pext[i].len2() + m_pStructure->Lext[i].len2() +
				          m_pStructure->Fext[i].len2() + m_pStructure->Text[i].len2();
				g_parts[i].pent = this;
			}
			g_parts[-1].org.zero();
			g_parts[-1].v.zero();
			g_parts[-1].w.zero();
			g_parts[-1].Minv = 0;
			g_parts[-1].Iinv.SetZero();
			g_parts[-1].bProcessed = 0;
			g_parts[-1].ijoint0 = g_parts[m_nParts].ijoint0 = 0;
			eprep.massOrg = M;
			if (M > 0)
			{
				M = 1.0f / M;
			}
			if (m_pStructure->idpartBreakOrg >= 0)
			{
				for (g_parts[0].idx = m_nParts - 1;
				     g_parts[0].idx >= 0 && m_parts[g_parts[0].idx].id != m_pStructure->idpartBreakOrg;
				     g_parts[0].idx--)
					;
			}
			epjb.partidEpicenter = g_parts[0].idx >= 0 ? m_parts[g_parts[0].idx].id : -1;

			// for each part, build the list of related joints
			for (i = 0; i < m_pStructure->nJoints; i++)
			{
				g_parts[m_pStructure->pJoints[i].ipart[0]].ijoint0++;
				g_parts[m_pStructure->pJoints[i].ipart[1]].ijoint0++;
			}
			for (i = 0; i < m_nParts; i++)
			{
				g_parts[i].ijoint0 += g_parts[i - 1].ijoint0;
			}
			for (i = 0; i < m_pStructure->nJoints; i++)
			{
				g_jointidx[--g_parts[m_pStructure->pJoints[i].ipart[0]].ijoint0] = i;
				g_jointidx[--g_parts[m_pStructure->pJoints[i].ipart[1]].ijoint0] = i;
			}
			g_parts[m_nParts].ijoint0 = m_pStructure->nJoints * 2;

			// iterate parts around the fastest moving part breadth-wise, add connecting joints as we add
			// next part to the queue
			ihead = nJoints = 0;
			if (g_parts[0].idx >= 0)
			{
				do
				{
					for (g_parts[g_parts[ihead].idx].bProcessed = 1, itail = ihead + 1;
					     ihead < itail; ihead++)
					{
						for (i = g_parts[g_parts[ihead].idx].ijoint0;
						     i < g_parts[g_parts[ihead].idx + 1].ijoint0; i++)
						{
							j = m_pStructure->pJoints[g_jointidx[i]].ipart[0] +
							    m_pStructure->pJoints[g_jointidx[i]].ipart[1] -
							    g_parts[ihead].idx;
							if (!g_parts[j].bProcessed)
							{
								g_parts[itail].idx = j;
								itail += g_parts[j].Minv > 0;
								g_parts[j].bProcessed = 1;
								j += g_parts[ihead].idx;
								for (i1 = i;
								     i1 < g_parts[g_parts[ihead].idx + 1].ijoint0; i1++)
								{
									g_joints[nJoints].idx = g_jointidx[i1];
									nJoints +=
									    iszero(m_pStructure->pJoints[g_jointidx[i1]]
									               .ipart[0] +
									           m_pStructure->pJoints[g_jointidx[i1]]
									               .ipart[1] -
									           j) &
									    m_pStructure->pJoints[g_jointidx[i1]]
										.bBreakable;
								}
							}
						}
						g_parts[-1].bProcessed = 0;
					}
					if (ihead < m_nParts)
					{
						for (i = 0; i < m_nParts &&
						            (g_parts[i].bProcessed ||
						             m_parts[i].mass * (m_pStructure->Pext[i].len2() +
						                                m_pStructure->Lext[i].len2() +
						                                m_pStructure->Fext[i].len2() +
						                                m_pStructure->Text[i].len2()) ==
						                 0);
						     i++)
							;
						if (i == m_nParts)
						{
							break;
						}
						g_parts[ihead].idx = i;
					}
					else
					{
						break;
					}
				}
				while (true);
			}

			// prepare joint infos
			for (i = 0; i < nJoints; i++)
			{
				j = g_joints[i].idx;
				i0 = m_pStructure->pJoints[j].ipart[0];
				i1 = m_pStructure->pJoints[j].ipart[1];
				g_joints[i].r0 = g_parts[i0].org - m_pStructure->pJoints[j].pt;
				g_joints[i].r1 = g_parts[i1].org - m_pStructure->pJoints[j].pt;
				t = g_parts[i0].Minv + g_parts[i1].Minv;
				g_joints[i].vKinv.SetZero();
				g_joints[i].vKinv(0, 0) = t;
				g_joints[i].vKinv(1, 1) = t;
				g_joints[i].vKinv(2, 2) = t;
				crossproduct_matrix(g_joints[i].r0, R);
				g_joints[i].vKinv -= R * g_parts[i0].Iinv * R;
				crossproduct_matrix(g_joints[i].r1, R);
				g_joints[i].vKinv -= R * g_parts[i1].Iinv * R;
				g_joints[i].vKinv.Invert();
				(g_joints[i].wKinv = g_parts[i0].Iinv + g_parts[i1].Iinv).Invert();
				g_joints[i].P.zero();
				g_joints[i].L.zero();
				g_joints[i].bBroken = 0;
				g_jointsDbg[i].tension.set(-1, 0);
			}

			// resolve joints, alternating order each iteration; track saturated joints
			iter = 0;
			if (m_pStructure->idpartBreakOrg < 0)
			{
				do
				{
					ihead = (nJoints - 1) & -(iter & 1);
					itail = nJoints - (nJoints + 1 & -(iter & 1));
					for (i = ihead, bBroken = 0; i != itail; i += 1 - (iter << 1 & 2))
					{
						j = g_joints[i].idx;
						i0 = m_pStructure->pJoints[j].ipart[0];
						i1 = m_pStructure->pJoints[j].ipart[1];
						P = g_joints[i].P;
						L = g_joints[i].L;
						n = m_pStructure->pJoints[j].n;
						m_pStructure->pJoints[j].bBroken = 0;

						dw = g_parts[i0].w - g_parts[i1].w;
						g_joints[i].L -= g_joints[i].wKinv * dw;
						Pn = g_joints[i].L * n;
						if (m_pWorld->m_vars.bLogLatticeTension &&
						    jtens.set(Pn * Pn, sqr(m_pStructure->pJoints[j].maxTorqueTwist *
						                           dt)) >= g_jointsDbg[i].tension)
						{
							g_jointsDbg[i].tension = jtens, g_jointsDbg[i].itens = 0;
						}
						if (fabs_tpl(Pn) > m_pStructure->pJoints[j].maxTorqueTwist * dt)
						{
							g_joints[i].L -=
							    n * (Pn + (sgnnz(Pn) *
							               m_pStructure->pJoints[j].maxTorqueTwist * dt));
							g_joints[i].bBroken = m_pStructure->pJoints[j].bBroken = 1;
						}
						Ptang = g_joints[i].L - n * (Pn = n * g_joints[i].L);
						if (m_pWorld->m_vars.bLogLatticeTension &&
						    jtens.set(Ptang.len2(), sqr(m_pStructure->pJoints[j].maxTorqueBend *
						                                dt)) >= g_jointsDbg[i].tension)
						{
							g_jointsDbg[i].tension = jtens, g_jointsDbg[i].itens = 1;
						}
						if (Ptang.len2() > sqr(m_pStructure->pJoints[j].maxTorqueBend * dt))
						{
							g_joints[i].L =
							    Ptang.normalized() *
								(m_pStructure->pJoints[j].maxTorqueBend * dt) +
							    n * Pn;
							g_joints[i].bBroken = m_pStructure->pJoints[j].bBroken = 1;
						}
						L = g_joints[i].L - L;
						g_parts[i0].w += g_parts[i0].Iinv * L;
						g_parts[i1].w -= g_parts[i1].Iinv * L;

						dv = g_parts[i0].v + (g_parts[i0].w ^ g_joints[i].r0) - g_parts[i1].v -
						     (g_parts[i1].w ^ g_joints[i].r1);
						g_joints[i].P -= g_joints[i].vKinv * dv;
						Pn = g_joints[i].P * (n = m_pStructure->pJoints[j].n);
						if (m_pWorld->m_vars.bLogLatticeTension)
						{
							if (jtens.set(sqr_signed(Pn),
							              sqr(m_pStructure->pJoints[j].maxForcePush *
							                  dt)) >= g_jointsDbg[i].tension)
							{
								g_jointsDbg[i].tension = jtens,
								g_jointsDbg[i].itens = 2;
							}
							else if (jtens.set(-sqr_signed(Pn),
							                   sqr(m_pStructure->pJoints[j].maxForcePull *
							                       dt)) >= g_jointsDbg[i].tension)
							{
								g_jointsDbg[i].tension = jtens,
								g_jointsDbg[i].itens = 3;
							}
						}
						if (Pn > m_pStructure->pJoints[j].maxForcePush * dt)
						{
							g_joints[i].P -=
							    n * (Pn - (m_pStructure->pJoints[j].maxForcePush * dt));
							g_joints[i].bBroken = m_pStructure->pJoints[j].bBroken = 1;
						}
						else if (Pn < -m_pStructure->pJoints[j].maxForcePull * dt)
						{
							g_joints[i].P -=
							    n * (Pn + (m_pStructure->pJoints[j].maxForcePull * dt));
							g_joints[i].bBroken = m_pStructure->pJoints[j].bBroken = 1;
						}
						Ptang = g_joints[i].P - n * (Pn = n * g_joints[i].P);
						if (m_pWorld->m_vars.bLogLatticeTension &&
						    jtens.set(Ptang.len2(), sqr(m_pStructure->pJoints[j].maxForceShift *
						                                dt)) >= g_jointsDbg[i].tension)
						{
							g_jointsDbg[i].tension = jtens, g_jointsDbg[i].itens = 4;
						}
						if (Ptang.len2() > sqr(m_pStructure->pJoints[j].maxForceShift * dt))
						{
							g_joints[i].P =
							    Ptang.normalized() *
								(m_pStructure->pJoints[j].maxForceShift * dt) +
							    n * Pn;
							g_joints[i].bBroken = m_pStructure->pJoints[j].bBroken = 1;
						}
						P = g_joints[i].P - P;
						g_parts[i0].v += P * g_parts[i0].Minv;
						g_parts[i0].w += g_parts[i0].Iinv * (g_joints[i].r0 ^ P);
						g_parts[i1].v -= P * g_parts[i1].Minv;
						g_parts[i1].w -= g_parts[i1].Iinv * (g_joints[i].r1 ^ P);

						bBroken += m_pStructure->pJoints[j].bBroken;
					}
					++iter;
				}
				while ((iter < 3 || bBroken) && iter < 23);
			}

			for (i = 0; i < nJoints; i++)
			{
				for (j = 0; j < 2; j++)
				{
					if ((i0 = m_pStructure->pJoints[g_joints[i].idx].ipart[j]) >= 0 &&
					    m_parts[i0].pLattice && m_parts[i0].idmatBreakable >= 0)
					{
						t = m_parts[i0].pLattice->m_density * m_parts[i0].pPhysGeom->V *
						    cube(m_parts[i0].scale) / m_parts[i0].mass;
						org = (m_pStructure->pJoints[g_joints[i].idx].pt - m_parts[i0].pos) *
						      m_parts[i0].q;
						if (m_parts[i0].scale != 1.0f)
						{
							org /= m_parts[i0].scale;
						}
						m_parts[i0].pLattice->AddImpulse(org,
						                                 (g_joints[i].P * m_parts[i0].q) * t,
						                                 (g_joints[i].L * m_parts[i0].q) * t,
						                                 gravity, m_pWorld->m_updateTimes[0]);
					}
				}
			}

			// check connectivity among parts, assign island number to each part (iterate starting from the
			// 'world' part)
			for (i = 0; i < m_nParts; i++)
			{
				g_parts[i].bProcessed = 0;
			}
			g_parts[-1].bProcessed = 1;
			g_parts[ihead = 0].idx = g_parts[0].ijoint0 > 0 ? -1 : 0;
			nIsles = 0;
			bBroken = 0;
			iter = 0;
			do
			{
				for (itail = ihead + 1; ihead < itail; ihead++)
				{
					g_parts[g_parts[ihead].idx].isle = nIsles & iter;
					for (i = g_parts[g_parts[ihead].idx].ijoint0;
					     i < g_parts[g_parts[ihead].idx + 1].ijoint0; i++)
					{
						if (!m_pStructure->pJoints[g_jointidx[i]].bBroken)
						{
							j = m_pStructure->pJoints[g_jointidx[i]].ipart[0] +
							    m_pStructure->pJoints[g_jointidx[i]].ipart[1] -
							    g_parts[ihead].idx;
							if (!g_parts[j].bProcessed)
							{
								g_parts[itail++].idx = j;
								g_parts[j].bProcessed = 1;
							}
						}
						else
						{
							bBroken -= (nIsles - 1) >> 31;
						}
					}
				}
				nIsles++;
				if (ihead >= m_nParts + 1)
				{
					break;
				}
				for (i = 0; i < m_nParts && (g_parts[i].bProcessed || g_parts[i].Minv > 0); i++)
					;
				if (i == m_nParts)
				{
					for (i = 0; i < m_nParts && g_parts[i].bProcessed; i++)
						;
					iter = -1;
				}
				g_parts[g_parts[ihead].idx = i].bProcessed = 1;
			}
			while (true);

			for (i = m_nParts - 1, j = 0; i >= 0; i--)
			{
				if (g_parts[i].isle != 0)
				{
					if (nRemoveGeoms + j < sizeof(idRemoveGeoms) / sizeof(idRemoveGeoms[0]))
					{
						j++;
					}
					else
					{
						g_parts[i].isle = 0;
					}
				}
			}

			epcep.pMeshNew = 0;
			epcep.pLastUpdate = 0;
			epcep.bInvalid = 0;
			epcep.iReason = EventPhysCreateEntityPart::ReasonJointsBroken;
			pents = 0;

			if (m_pWorld->m_vars.breakImpulseScale ||
			    m_flags & pef_override_impulse_scale && !m_pWorld->m_vars.bMultiplayer || !pexpl ||
			    (pexpl && pexpl->forceDeformEntities))
			{
				for (i = 1; i < nIsles; i++)
				{
					for (j = 0, epcep.nTotParts = 0; j < m_nParts; j++)
					{
						epcep.nTotParts += iszero(g_parts[j].isle - i);
					}
					if (!epcep.nTotParts)
					{
						continue;
					}
					epcep.pEntNew = m_pWorld->CreatePhysicalEntity(PE_RIGID, &pp, 0, 0x5AFE);
					asv.v.zero();
					asv.w.zero();
					t = 0;

					for (j = i1 = 0, BBoxNew[0] = m_BBox[1], BBoxNew[1] = m_BBox[0]; j < m_nParts;
					     j++)
					{
						if (g_parts[j].isle == i)
						{
							gpAux.flags = m_parts[j].flags;
							gpAux.flagsCollider = m_parts[j].flagsCollider;
							gpAux.mass = m_parts[j].mass;
							gpAux.pos = m_parts[j].pos;
							gpAux.q = m_parts[j].q;
							gpAux.scale = m_parts[j].scale;
							gpAux.idmatBreakable = m_parts[j].idmatBreakable;
							gpAux.minContactDist = m_parts[j].minContactDist;
							gpAux.pMatMapping =
							    m_parts[j].pMatMapping != m_parts[j].pPhysGeom->pMatMapping
								? m_parts[j].pMatMapping
								: 0;
							gpAux.nMats = m_parts[j].nMats;
							if (gpAux.mass <= m_pWorld->m_vars.massLimitDebris)
							{
								if (!is_unused(m_pWorld->m_vars.flagsColliderDebris))
								{
									gpAux.flagsCollider =
									    m_pWorld->m_vars.flagsColliderDebris;
								}
								gpAux.flags &= m_pWorld->m_vars.flagsANDDebris;
							}
							if (!(m_parts[j].flags & geom_will_be_destroyed) &&
							    m_parts[j].pPhysGeom->pGeom->GetType() == GEOM_VOXELGRID)
							{
								(pMeshNew = new CTriMesh())
								    ->Clone((CTriMesh*)m_parts[j].pPhysGeom->pGeom, 0);
								delete pMeshNew->m_pTree;
								pMeshNew->m_pTree = 0;
								pMeshNew->m_flags |= mesh_AABB;
								pMeshNew->RebuildBVTree();
								m_parts[j].pPhysGeom->pGeom->Release();
								m_parts[j].pPhysGeom->pGeom = pMeshNew;
							}
							epcep.pEntNew->AddGeometry(m_parts[j].pPhysGeom, &gpAux, i1, 1);
							if (m_parts[j].pPhysGeomProxy != m_parts[j].pPhysGeom)
							{
								gpAux.flags |= geom_proxy;
								epcep.pEntNew->AddGeometry(m_parts[j].pPhysGeomProxy,
								                           &gpAux, i1, 1);
								gpAux.flags &= ~geom_proxy;
							}
							if (m_iSimClass > 0)
							{
								pe_status_dynamics sd;
								sd.ipart = j;
								GetStatus(&sd);
								asv.v += sd.v * m_parts[j].mass;
								asv.w += sd.w * m_parts[j].mass;
								t += m_parts[j].mass;
							}
							shockwave.ipart = g_parts[j].idx = i1++;
							shockwave.impulse = ((m_pStructure->Pext[j] * dt * 100 +
							                      m_pStructure->Fext[j]) *
							                         0.6f +
							                     Pmax * 0.4f) *
							                    max(0.1f, m_parts[j].mass * M);
							shockwave.angImpulse = ((m_pStructure->Lext[j] * dt * 100 +
							                         m_pStructure->Text[j]) *
							                            0.6f +
							                        Lmax * 0.4f) *
							                       max(0.1f, m_parts[j].mass * M);
							if (shockwave.impulse.len2() > sqr(m_parts[j].mass * 4.0f))
							{
								shockwave.impulse *= (vmax = m_parts[j].mass * 4.0f /
								                             shockwave.impulse.len()),
								    shockwave.angImpulse *= vmax;
							}
							epcep.breakImpulse = shockwave.impulse;
							epcep.breakAngImpulse = shockwave.angImpulse;
							if (!(m_flags & aef_recorded_physics))
							{
								epcep.pEntNew->Action(&shockwave, 1);
							}
							epcep.partidSrc = m_parts[j].id;
							epcep.partidNew = shockwave.ipart;
							m_pWorld->OnEvent(m_pWorld->m_vars.bLogStructureChanges + 1,
							                  &epcep);
							g_parts[j].pent = (CPhysicalEntity*)epcep.pEntNew;
							BBoxNew[0] = min(BBoxNew[0], m_parts[j].BBox[0]);
							BBoxNew[1] = max(BBoxNew[1], m_parts[j].BBox[1]);
						}
					}
					for (j = 0; j < m_pStructure->nJoints; j++)
					{
						if (!m_pStructure->pJoints[j].bBroken &&
						    g_parts[m_pStructure->pJoints[j].ipart[0]].isle == i)
						{
							psj.id = m_pStructure->pJoints[j].id;
							psj.partid[0] = g_parts[m_pStructure->pJoints[j].ipart[0]].idx;
							psj.partid[1] = g_parts[m_pStructure->pJoints[j].ipart[1]].idx;
							psj.pt = m_pStructure->pJoints[j].pt;
							psj.n = m_pStructure->pJoints[j].n;
							psj.maxForcePush = m_pStructure->pJoints[j].maxForcePush;
							psj.maxForcePull = m_pStructure->pJoints[j].maxForcePull;
							psj.maxForceShift = m_pStructure->pJoints[j].maxForceShift;
							psj.maxTorqueBend = m_pStructure->pJoints[j].maxTorqueBend;
							psj.maxTorqueTwist = m_pStructure->pJoints[j].maxTorqueTwist;
							psj.bBreakable = m_pStructure->pJoints[j].bBreakable;
							psj.szSensor = m_pStructure->pJoints[j].size;
							epcep.pEntNew->SetParams(&psj, 1);
							((CPhysicalEntity*)epcep.pEntNew)->m_pStructure->bModified = 1;
						}
					}
					if (m_iSimClass * t > 0)
					{
						asv.v *= (t = 1.0f / t);
						asv.w *= t;
						epcep.pEntNew->Action(&asv, 1);
					}
					if (!(m_flags & aef_recorded_physics))
					{
						if (!pents)
						{
							iter = m_pWorld->GetEntitiesAround(
							    m_BBox[0] - Vec3(0.1f), m_BBox[1] + Vec3(0.1f), pents,
							    ent_rigid | ent_sleeping_rigid | ent_independent);
						}
						BBoxNew[0] -= Vec3(m_pWorld->m_vars.maxContactGap * 5);
						BBoxNew[1] += Vec3(m_pWorld->m_vars.maxContactGap * 5);
						for (j = iter - 1; j >= 0; j--)
						{
							if (AABB_overlap(pents[j]->m_BBox, BBoxNew))
							{
								pents[j]->OnNeighbourSplit(
								    this, (CPhysicalEntity*)epcep.pEntNew);
							}
						}
					}
				}
			}

			for (i = 0; i < m_pStructure->nJoints; i++)
			{
				if (m_pStructure->pJoints[i].bBroken)
				{
					for (j = 0; j < 2; j++)
					{
						if ((i0 = m_pStructure->pJoints[i].ipart[j]) >= 0 &&
						    m_parts[i0].flags & geom_manually_breakable)
						{
							EventPhysCollision epc;
							epc.pEntity[0] = &g_StaticPhysicalEntity;
							epc.pForeignData[0] = 0;
							epc.iForeignData[0] = 0;
							epc.vloc[1].zero();
							epc.mass[0] = 1E10f;
							epc.partid[0] = 0;
							epc.penetration = epc.normImpulse = 0;
							epc.pt = (m_parts[i0].BBox[0] + m_parts[i0].BBox[1]) * 0.5f;
							epc.pEntity[1] = g_parts[i0].pent;
							epc.pForeignData[1] = g_parts[i0].pent->m_pForeignData;
							epc.iForeignData[1] = g_parts[i0].pent->m_iForeignData;
							m_parts[i0].pPhysGeom->pGeom->GetBBox(&bbox);
							epc.n.zero()[idxmin3((float*)&bbox.size)] = 1;
							epc.n = m_qrot * (m_parts[i0].q * (epc.n * bbox.Basis));
							epc.vloc[0] = epc.n * -4.0f;
							epc.mass[0] = epc.mass[1] = m_parts[i0].mass;
							epc.radius = (m_BBox[1].x + m_BBox[1].y + m_BBox[1].z -
							              m_BBox[0].x - m_BBox[0].y - m_BBox[0].z) *
							             2;
							epc.partid[1] =
							    g_parts[i0].pent == this ? m_parts[i0].id : g_parts[i0].idx;
							epc.idmat[0] = -2;
							for (i1 = m_parts[i0].pPhysGeom->pGeom->GetPrimitiveCount() - 1;
							     i1 >= 0; i1--)
							{
								epc.idmat[1] = GetMatId(
								    m_parts[i0].pPhysGeom->pGeom->GetPrimitiveId(i1,
								                                                 0x40),
								    i0);
								if (m_pWorld->m_SurfaceFlagsTable[epc.idmat[1]] &
								    sf_manually_breakable)
								{
									break;
								}
							}
							m_pWorld->OnEvent(pef_log_collisions, &epc);
						}
					}
				}
			}

			if (m_pWorld->m_vars.bLogLatticeTension &&
			    (bBroken || impTot > 0 && m_pWorld->m_timePhysics > m_pStructure->minSnapshotTime))
			{
				if (bBroken)
				{
					m_pStructure->minSnapshotTime = m_pWorld->m_timePhysics + 2.0f;
				}
				m_pStructure->nPrevJoints = nJoints0;
				m_pStructure->prevdt = dt;
				for (i = 0; i < nJoints; i++)
				{
					m_pStructure->pJoints[g_joints[i].idx].tension = g_jointsDbg[i].tension.x;
					m_pStructure->pJoints[g_joints[i].idx].itens = g_jointsDbg[i].itens;
				}
			}

			// move broken or detached joints to the end of the list
			for (i = iter = 0; i < m_pStructure->nJoints; i++)
			{
				if (m_pStructure->pJoints[i].bBroken + g_parts[m_pStructure->pJoints[i].ipart[0]].isle >
				    0)
				{
					if (m_pStructure->pJoints[i].bBroken)
					{
						epjb.idJoint = m_pStructure->pJoints[i].id;
						epjb.pt = m_qrot * m_pStructure->pJoints[i].pt + m_pos;
						epjb.n = m_qrot * m_pStructure->pJoints[i].n;
						i0 = (m_pStructure->pJoints[i].ipart[0] |
						      m_pStructure->pJoints[i].ipart[1]) >= 0 &&
						     m_parts[m_pStructure->pJoints[i].ipart[0]].pPhysGeom->V *
						             cube(m_parts[m_pStructure->pJoints[i].ipart[0]].scale) <
						         m_parts[m_pStructure->pJoints[i].ipart[1]].pPhysGeom->V *
						             cube(m_parts[m_pStructure->pJoints[i].ipart[1]].scale);
						for (j = 0; j < 2; j++)
						{
							if (m_pStructure->pJoints[i].ipart[j] >= 0)
							{
								epjb.partid[j ^ i0] =
								    m_parts[m_pStructure->pJoints[i].ipart[j]].id;
								epjb.partmat[j ^ i0] = GetMatId(
								    m_parts[m_pStructure->pJoints[i].ipart[j]]
									.pPhysGeom->pGeom->GetPrimitiveId(0, 0x40),
								    m_pStructure->pJoints[i].ipart[j]);
							}
							else
							{
								epjb.partid[j ^ i0] = epjb.partmat[j ^ i0] = -1;
							}
						}
						m_pWorld->OnEvent(m_pWorld->m_vars.bLogStructureChanges + 1, &epjb);
						++iter;
					}
					SStructuralJoint sj = m_pStructure->pJoints[i];
					m_pStructure->pJoints[i] = m_pStructure->pJoints[--m_pStructure->nJoints];
					--i;
					m_pStructure->pJoints[m_pStructure->nJoints] = sj;
				}
			}
			for (i = 0; i < sizeof(eprep.partIds) / sizeof(eprep.partIds[0]); i++)
			{
				eprep.partIds[i] = 0;
			}
			for (i = m_nParts - 1, j = 0; i >= 0; i--)
			{
				m_pStructure->Pext[i].zero();
				m_pStructure->Lext[i].zero();
				m_pStructure->Fext[i].zero();
				m_pStructure->Text[i].zero();
				if (g_parts[i].isle != 0 &&
				    nRemoveGeoms < sizeof(idRemoveGeoms) / sizeof(idRemoveGeoms[0]))
				{
					if (m_parts[i].id < sizeof(eprep.partIds) * 8)
					{
						eprep.partIds[m_parts[i].id >> 5] |= 1 << (m_parts[i].id & 31);
					}
					j++; // RemoveGeometry(m_parts[i].id);
					idRemoveGeoms[nRemoveGeoms] = m_parts[i].id;
					pRemoveGeoms[nRemoveGeoms++] = m_parts[i].pPhysGeom;
				}
			}
			if (j | iter)
			{
				m_pWorld->OnEvent(m_pWorld->m_vars.bLogStructureChanges + 1, &eprep);
			}

			m_pStructure->bModified += bBroken;
			if (m_nParts - nRemoveGeoms == 0)
			{
				bBroken = 0;
			}
			if (bBroken != m_pStructure->nLastBrokenJoints)
			{
				flags = geom_structure_changes;
			}
			else if (flags & geom_structure_changes)
			{
				m_pStructure->nLastBrokenJoints = bBroken;
			}
			if (m_pStructure->idpartBreakOrg >= 0)
			{
				flags &= ~geom_structure_changes;
				m_pStructure->idpartBreakOrg = -1;
				goto SkipMeshUpdates;
			}
		}

		if ((m_timeStructUpdate += time_interval) >= m_pWorld->m_vars.tickBreakable)
		{
			if (m_pStructure)
			{
				for (m_iLastIdx = i = 0; i < m_nParts; i++)
				{
					m_iLastIdx = max(m_iLastIdx, m_parts[i].id + 1);
				}
			}
			epcep.iReason = EventPhysCreateEntityPart::ReasonMeshSplit;
			for (i = 0; i < m_nParts; i++)
			{
				if (m_parts[i].flags & geom_structure_changes)
				{
					epum.partid = m_parts[i].id;
					epum.pMesh = m_parts[i].pPhysGeomProxy->pGeom;
					for (j = 0; j < nPlanes; j++)
					{
						ground[j].origin = (m_ground[j].origin - m_parts[i].pos) * m_parts[i].q;
						if (m_parts[i].scale != 1.0f)
						{
							ground[j].origin /= m_parts[i].scale;
						}
						ground[j].n = m_ground[j].n * m_parts[i].q;
					}

					if (m_updStage == 0)
					{
						if (m_parts[i].pLattice && m_pExpl)
						{
							expl.epicenter = expl.epicenterImp =
							    ((m_pExpl->center - m_pos) * m_qrot - m_parts[i].pos) *
							    m_parts[i].q;
							expl.impulsivePressureAtR =
							    max(0.0f, m_pExpl->kr) * m_parts[i].pLattice->m_density *
							    m_parts[i].pPhysGeom->V * cube(m_parts[i].scale) /
							    m_parts[i].mass;
							expl.r = 1.0f;
							expl.rmin = m_pExpl->rmin;
							expl.rmax = 1E10f;
							if (m_parts[i].scale != 1.0f)
							{
								t = 1.0f / m_parts[i].scale;
								expl.rmin *= t;
								expl.r *= t;
								expl.epicenter *= t;
								expl.epicenterImp *= t;
							}
						}
						if (m_parts[i].pLattice &&
						    m_parts[i].pLattice->CheckStructure(
							time_interval, (gravity * m_qrot) * m_parts[i].q, ground,
							nPlanes, m_nUpdTicks < 2 && m_pExpl ? &expl : 0,
							m_pWorld->m_vars.nMaxLatticeIters,
							m_pWorld->m_vars.bLogLatticeTension))
						{
							epum.pMesh->Lock();
							epum.pLastUpdate = (bop_meshupdate*)epum.pMesh->GetForeignData(
							    DATA_MESHUPDATE);
							for (; epum.pLastUpdate && epum.pLastUpdate->next;
							     epum.pLastUpdate = epum.pLastUpdate->next)
								;
							epum.pMesh->Unlock();
							m_pWorld->OnEvent(m_pWorld->m_vars.bLogStructureChanges + 1,
							                  &epum);
						}
						else
						{
							m_parts[i].flags &= ~geom_structure_changes;
						}
						if (m_parts[i].pLattice && m_pWorld->m_vars.bLogLatticeTension)
						{
							t = m_parts[i].pLattice->GetLastTension(j);
							CryLog("Lattice tension: %f (%s)", t,
							       j == LPush
							           ? "push"
							           : (j == LPull
							                  ? "pull"
							                  : (j == LShift
							                         ? "shift"
							                         : (j == LBend ? "bend" : "twist"))));
						}
					}
					else
					{
						pMeshNew =
						    ((CTriMesh*)m_parts[i].pPhysGeomProxy->pGeom)
							->SplitIntoIslands(ground, nPlanes, GetType() == PE_RIGID);
						if (pMeshNew)
						{
							nMeshSplits++;
							if (m_parts[i].pLattice)
							{
								for (j = 0, pMesh = pMeshNew; pMesh;
								     pMesh = (CTriMesh*)pMesh->GetForeignData(-2), j++)
									;
								if (j > sizeof(pChunkMeshes) / sizeof(pChunkMeshes[0]))
								{
									pChunkMeshes = new CTriMesh*[j];
									pChunkLattices = new CTetrLattice*[j];
								}
								for (j = 0, pMesh = pMeshNew; pMesh;
								     pMesh = (CTriMesh*)pMesh->GetForeignData(-2), j++)
								{
									pChunkMeshes[j] = pMesh;
								}
								m_parts[i].pLattice->Split(pChunkMeshes, j,
								                           pChunkLattices);
							}

							gp.pos = m_parts[i].pos;
							gp.q = m_parts[i].q;
							gp.scale = m_parts[i].scale;
							gp.pLattice = m_parts[i].pLattice;
							gp.pMatMapping =
							    m_parts[i].pMatMapping != m_parts[i].pPhysGeom->pMatMapping
								? m_parts[i].pMatMapping
								: 0;
							gp.nMats = m_parts[i].nMats;
							pgeom0 = m_parts[i].pPhysGeomProxy;
							// t = m_parts[i].mass/((V0=pgeom0->V)*cube(m_parts[i].scale));
							// ppt.ipart=i; ppt.density=0; SetParams(&ppt);
							V0 = pgeom0->V;
							pgeom0->pGeom->CalcPhysicalProperties(pgeom0);
							t = max(max(pgeom0->Ibody.x, pgeom0->Ibody.y),
							        pgeom0->Ibody.z) *
							    0.01f;
							pgeom0->Ibody.x = max(pgeom0->Ibody.x, t);
							pgeom0->Ibody.y = max(pgeom0->Ibody.y, t);
							pgeom0->Ibody.z = max(pgeom0->Ibody.z, t);
							pgeom0->pGeom->GetBBox(&bbox);
							org = bbox.Basis * (pgeom0->origin - bbox.center);
							if (m_iSimClass > 0 &&
							    (max(max(fabs_tpl(org.x) - bbox.size.x,
							             fabs_tpl(org.y) - bbox.size.y),
							         fabs_tpl(org.z) - bbox.size.z) > 0 ||
							     min(min(min(pgeom0->V, pgeom0->Ibody.x), pgeom0->Ibody.y),
							         pgeom0->Ibody.z) < 0 ||
							     pgeom0->V < V0 * 0.002f))
							{
								//((CTriMesh*)pgeom0->pGeom)->Empty();
								pgeom0->V = 0;
								pgeom0->Ibody.zero();
								m_parts[i].flags |= geom_invalid;
								epum.bInvalid = 1;
							}
							else
							{
								epum.bInvalid = 0;
							}
							// ppt.density=t; SetParams(&ppt);

							for (j = 0; pMeshNew; pMeshNew = pMeshNext, j++)
							{
								pMeshNext = (CTriMesh*)pMeshNew->GetForeignData(-2);
								pMeshNew->SetForeignData(0, 0);
								pgeom = m_pWorld->RegisterGeometry(
								    pMeshNew, pgeom0->surface_idx, pgeom0->pMatMapping,
								    pgeom0->nMats);
								pMeshNew->GetBBox(&bbox);
								org = bbox.Basis * (pgeom->origin - bbox.center);
								if (max(max(fabs_tpl(org.x) - bbox.size.x,
								            fabs_tpl(org.y) - bbox.size.y),
								        fabs_tpl(org.z) - bbox.size.z) > 0 ||
								    min(min(min(pgeom->V, pgeom->Ibody.x),
								            pgeom->Ibody.y),
								        pgeom->Ibody.z) < 0 ||
								    pgeom->V < V0 * 0.002f)
								{
									gp.flags = gp.flagsCollider = 0;
									gp.idmatBreakable = -1;
									gp.density = 0, epcep.bInvalid = 1;
								}
								else
								{
									gp.flags =
									    m_parts[i].flags & ~geom_structure_changes;
									gp.flagsCollider = m_parts[i].flagsCollider;
									gp.idmatBreakable = m_parts[i].idmatBreakable;
									gp.density = m_parts[i].mass /
									             (V0 * cube(m_parts[i].scale));
									epcep.bInvalid = 0;
								}
								gp.pLattice =
								    m_parts[i].pLattice ? pChunkLattices[j] : 0;
								if (!m_pStructure)
								{
									epcep.pEntNew = m_pWorld->CreatePhysicalEntity(
									    PE_RIGID, &pp, 0, 0x5AFE);
									epcep.pEntNew->AddGeometry(pgeom, &gp, -1, 1);
									pgeom->nRefCount--;
									epcep.partidNew = 0;
								}
								else
								{
									epcep.pEntNew = this;
									pAddGeoms[nAddGeoms] = pgeom;
									gpAddGeoms[nAddGeoms] = gp;
									bGeomOverflow |=
									    isneg((int)(sizeof(gpAddGeoms) /
									                sizeof(gpAddGeoms[0])) -
									          nAddGeoms - 1);
									nAddGeoms += 1 - bGeomOverflow;
									// nAddGeoms=min(nAddGeoms+1,
									// sizeof(gpAddGeoms)/sizeof(gpAddGeoms[0]));
									epcep.partidNew = iLastIdx + j;
								}

								gwd.R = Matrix33(m_parts[i].q);
								gwd.offset = m_parts[i].pos;
								gwd.scale = m_parts[i].scale;
								ip.bStopAtFirstTri = ip.bNoAreaContacts = ip.bNoBorder =
								    true;

								for (i1 = 0; i1 < m_nParts; i1++)
								{
									if (m_parts[i1].idmatBreakable < 0)
									{
										m_parts[i1].maxdim =
										    m_parts[i1].pPhysGeom->V;
										if (m_parts[i1]
										            .pPhysGeom->pGeom
										            ->GetType() ==
										        GEOM_TRIMESH &&
										    ((CTriMesh*)m_parts[i1]
										         .pPhysGeom->pGeom)
										            ->BuildIslandMap() > 1 &&
										    !m_parts[i1].pLattice)
										{ // separate non-breakable meshes into
										  // islands; store island list in
										  // part's pLattice
											if (!(m_parts[i1].flags &
											      geom_can_modify))
											{
												m_pWorld->ClonePhysGeomInEntity(
												    this, i1,
												    (new CTriMesh())
													->Clone(
													    (CTriMesh*)
														m_parts[i1]
														    .pPhysGeom
														    ->pGeom,
													    0));
											}
											m_parts[i1].pLattice =
											    (CTetrLattice*)((CTriMesh*)
											                        m_parts[i1]
											                            .pPhysGeom
											                            ->pGeom)
												->SplitIntoIslands(0, 0,
											                           1);
											m_parts[i1]
											    .pPhysGeom->pGeom
											    ->CalcPhysicalProperties(
												m_parts[i1].pPhysGeom);
										}
									}
								}

								if (!m_pStructure)
								{
									// find non-breakable parts that should be moved
									// to the new entity
									for (i1 = 0; i1 < m_nParts; i1++)
									{
										if (i1 != i)
										{
											pMeshIsland =
											    !(m_parts[i1].flags &
											      geom_removed)
												? m_parts[i1]
												      .pPhysGeom->pGeom
												: (m_parts[i1].idmatBreakable <
											                   0
											               ? (CTriMesh*)
											                     m_parts[i1]
											                         .pLattice
											               : 0);
											pMeshIslandPrev = 0;
											gwdAux.R =
											    Matrix33(m_parts[i1].q);
											gwdAux.offset = m_parts[i1].pos;
											gwdAux.scale =
											    m_parts[i1].scale;
											if (pMeshIsland)
											{
												do
												{
													pMeshIsland
													    ->GetBBox(
														&bbox);
													t = m_parts[i].scale ==
													            1.0f
													        ? 1.0f
													        : 1.0f /
													              m_parts[i]
													                  .scale;
													bbox.center =
													    ((m_parts[i1]
													              .q *
													          bbox.center *
													          m_parts[i1]
													              .scale +
													      m_parts[i1]
													          .pos -
													      m_parts[i]
													          .pos) *
													     m_parts[i]
													         .q) *
													    t;
													// bbox.Basis *=
													// Matrix33(!m_parts[i1].q*m_parts[i].q);
													// bbox.size *=
													// m_parts[i1].scale*t;
													// boxGeom.CreateBox(&bbox);
													pMeshNew
													    ->GetBBox(
														&bbox1);
													org =
													    bbox.size -
													    (bbox.Basis *
													     (gwd.R *
													          bbox1
													              .center *
													          gwd.scale +
													      gwd.offset -
													      bbox.center))
														.abs();
													// bBroken =
													// 0;//min(min(org.x,org.y),org.z)>0;
													bBroken =
													    pMeshIsland->PointInsideStatus(
														((gwdAux.R *
													              bbox1
													                  .center *
													              gwdAux
													                  .scale +
													          gwdAux
													              .offset -
													          gwd.offset) *
													         gwd.R) *
														t);
													if (pMeshNew->Intersect(
														pMeshIsland,
														&gwd,
														&gwdAux,
														&ip,
														pcontacts))
													{
														CryInterlockedAdd(
														    ip.plock,
														    -WRITE_LOCK_VAL),
														    bBroken =
															1;
													}
													if (bBroken)
													{
														FillGeomParams(
														    gpAux,
														    m_parts
															[i1]);
														gpAux
														    .mass *=
														    pMeshIsland
															->GetVolume() /
														    m_parts[i1]
															.maxdim;
														if (pMeshIsland ==
														    m_parts[i1]
														        .pPhysGeom
														        ->pGeom)
														{
															if (epcep
															        .pEntNew &&
															    !epcep
															         .bInvalid)
															{
																epcep
																    .pEntNew
																    ->AddGeometry(
																	m_parts[i1]
																	    .pPhysGeom,
																	&gpAux,
																	-1,
																	1);
															}
															m_parts[i1]
															    .flags |=
															    geom_removed; // m_parts[i1].idmatBreakable = -10;
														}
														else
														{
															pgeom1 =
															    m_pWorld
																->RegisterGeometry(
																    pMeshIsland,
																    m_parts[i1]
																	.pPhysGeom
																	->surface_idx,
																    m_parts[i1]
																	.pPhysGeom
																	->pMatMapping,
																    m_parts[i1]
																	.pPhysGeom
																	->nMats);
															if (epcep
															        .pEntNew &&
															    !epcep
															         .bInvalid)
															{
																epcep
																    .pEntNew
																    ->AddGeometry(
																	pgeom1,
																	&gpAux,
																	-1,
																	1);
															}
															pgeom1
															    ->nRefCount--;
															if (pMeshIslandPrev)
															{
																pMeshIslandPrev
																    ->SetForeignData(
																	pMeshIsland
																	    ->GetForeignData(),
																	-2);
															}
															else
															{
																m_parts[i1]
																    .pLattice =
																    (CTetrLattice*)pMeshIsland
																	->GetForeignData(
																	    -2);
															}
														}
														pmu =
														    (bop_meshupdate*)pMeshNew
															->GetForeignData(
															    DATA_MESHUPDATE);
														if (pmu)
														{
															ReallocateList(
															    pmu->pMovedBoxes,
															    pmu->nMovedBoxes,
															    pmu->nMovedBoxes +
																1);
															pmu->pMovedBoxes
															    [pmu->nMovedBoxes++] =
															    bbox;
														}
													}
													if (pMeshIsland ==
													    m_parts[i1]
													        .pPhysGeom
													        ->pGeom)
													{
														pMeshIsland =
														    m_parts[i1].idmatBreakable <
															    0
															? (CTriMesh*)m_parts
															      [i1]
																  .pLattice
															: 0;
													}
													else
													{
														pMeshIslandPrev =
														    pMeshIsland;
														pMeshIsland =
														    (CTriMesh*)pMeshIsland
															->GetForeignData(
															    -2);
													}
												}
												while (pMeshIsland);
											}
											if (m_parts[i1].idmatBreakable <
											    0)
											{
												m_parts[i1].mass *=
												    m_parts[i1]
													.pPhysGeom->V /
												    m_parts[i1].maxdim;
											}
										}
									}
								}
								else if (!bGeomOverflow)
								{
									// find joints that must be deleted or
									// re-assigned to the new part
									for (i1 = 0; i1 < m_pStructure->nJoints; i1++)
									{
										if (m_pStructure->pJoints[i1]
										            .ipart[0] == i ||
										    m_pStructure->pJoints[i1]
										            .ipart[1] == i)
										{
											primitives::sphere sph;
											sph.center =
											    m_pStructure->pJoints[i1]
												.pt;
											sph.r =
											    m_pStructure->pJoints[i1]
												.size *
											    0.5f;
											sphGeom.CreateSphere(&sph);
											// aray.m_ray.dir = (aray.m_dirn
											// =
											// m_pStructure->pJoints[i1].n)*t;
											// aray.m_ray.origin =
											// m_pStructure->pJoints[i1].pt-aray.m_ray.dir*0.5f;
											if (pMeshNew->Intersect(
												&sphGeom, &gwd, 0, 0,
												pcontacts))
											{ // joint touches the new mesh
												m_pStructure
												    ->pJoints[i1]
												    .ipart[iszero(
													m_pStructure
													    ->pJoints
														[i1]
													    .ipart[1] -
													i)] =
												    m_nParts + j;
												m_pStructure
												    ->bModified++;
											}
											// else if
											// (!m_parts[i].pPhysGeomProxy->pGeom->Intersect(&aray,&gwd,0,0,pcontacts))
											// // joint doesn't touch
											// neither new nor old
											//	m_pStructure->pJoints[i1]
											//=
											// m_pStructure->pJoints[--m_pStructure->nJoints],
											// i1--;
										}
									}
								}

								if (epcep.pEntNew && !epcep.bInvalid)
								{
									org = m_pos +
									      m_qrot * (m_parts[i].pos +
									                m_parts[i].q * pgeom->origin);
									if (m_iSimClass > 0)
									{
										pe_status_dynamics sd;
										sd.ipart = i;
										GetStatus(&sd);
										asv.v = sd.v +
										        (sd.w ^ org - sd.centerOfMass);
										asv.w = sd.w;
										epcep.pEntNew->Action(&asv, 1);
									}

									// add impulse from the explosion
									if (m_nUpdTicks == 0 && m_pExpl)
									{
										gwd.R = Matrix33(m_qrot * m_parts[i].q);
										gwd.offset =
										    m_pos + m_qrot * m_parts[i].pos;
										gwd.scale = m_parts[i].scale;
										shockwave.impulse.zero();
										shockwave.angImpulse.zero();
										if (m_pExpl->kr > 0)
										{
											pgeom->pGeom
											    ->CalcVolumetricPressure(
												&gwd, m_pExpl->center,
												m_pExpl->kr,
												m_pExpl->rmin,
												gwd.R * pgeom->origin *
													gwd.scale +
												    gwd.offset,
												shockwave.impulse,
												shockwave.angImpulse);
										}
										else if (m_pExpl->kr < 0)
										{
											MARK_UNUSED shockwave.point;
											shockwave.impulse =
											    m_pExpl->dir *
											    (pgeom->V *
											     cube(gwd.scale) *
											     gp.density);
											n = m_pExpl->center - org;
											if (n.len2() > 0)
											{
												shockwave.impulse -=
												    n *
												    ((n *
												      shockwave
												          .impulse) /
												     n.len2());
												quaternionf q =
												    m_qrot *
												    m_parts[i].q *
												    pgeom->q;
												shockwave.angImpulse =
												    q *
												    (Diag33(
													 pgeom->Ibody) *
												     (!q *
												      (m_pExpl->dir ^
												       n))) *
												    (cube(gwd.scale) *
												     gp.density /
												     n.len2());
											}
										}
										shockwave.ipart = 0;
										epcep.breakImpulse = shockwave.impulse;
										epcep.breakAngImpulse =
										    shockwave.angImpulse;
										shockwave.iApplyTime =
										    (epcep.pEntNew != this) * 2;
										epcep.pEntNew->Action(
										    &shockwave,
										    1); // shockwave.iApplyTime>>1^1);
										epcep.breakSize = m_pExpl->rmin;
									}

									// if (m_parts[i].flags &
									// geom_break_approximation &&
									// ((CPhysicalEntity*)epcep.pEntNew)->m_nParts)
									//	((CPhysicalEntity*)epcep.pEntNew)->CapsulizePart(0);
									if (m_parts[i].flags &
									        geom_break_approximation &&
									    ((CPhysicalEntity*)epcep.pEntNew)
									        ->m_nParts &&
									    (nJoints = ((CPhysicalEntity*)epcep.pEntNew)
									                   ->CapsulizePart(0)))
									{
										CPhysicalEntity* pent =
										    (CPhysicalEntity*)epcep.pEntNew;
										Vec3 ptEnd[2];
										primitives::capsule* pcaps =
										    (primitives::capsule*)pent
											->m_parts[pent->m_nParts -
										                  nJoints]
											.pPhysGeom->pGeom->GetData();
										epcep.cutRadius =
										    pcaps->r *
										    pent->m_parts[pent->m_nParts -
										                  nJoints]
											.scale;
										ptEnd[0] = ptEnd[1] = epcep
										                          .cutPtLoc[0] =
										    epcep.cutPtLoc[1] =
											pent->m_parts[pent->m_nParts -
										                      nJoints]
											    .pos -
											pent->m_parts[pent->m_nParts -
										                      nJoints]
												.q *
											    Vec3(0, 0,
										                 pcaps->hh + pcaps->r);
										epcep.cutDirLoc[1] =
										    -(epcep.cutDirLoc[0] =
										          pent->m_parts[pent->m_nParts -
										                        nJoints]
										              .q *
										          (n = pcaps->axis));
										t = ((CTriMesh*)m_parts[i]
										         .pPhysGeom->pGeom)
										        ->GetIslandDisk(
											    100,
											    ((epcep.cutPtLoc[0] -
										              m_parts[i].pos) *
										             m_parts[i].q) /
												m_parts[i].scale,
											    epcep.cutPtLoc[0], n, vmax);
										if (t)
										{
											epcep.cutRadius =
											    t * m_parts[i].scale;
											epcep.cutPtLoc[0] =
											    m_parts[i].q *
												epcep.cutPtLoc[0] *
												m_parts[i].scale +
											    m_parts[i].pos;
											epcep.cutDirLoc[0] =
											    m_parts[i].q * n;
											ptEnd[0] =
											    epcep.cutPtLoc[0] +
											    epcep.cutDirLoc[0] *
												(vmax *
											         m_parts[i].scale);
										}
										t = pMeshNew->GetIslandDisk(
										    100,
										    ((epcep.cutPtLoc[1] -
										      m_parts[i].pos) *
										     m_parts[i].q) /
											m_parts[i].scale,
										    epcep.cutPtLoc[1], n, vmax);
										if (t)
										{
											epcep.cutRadius =
											    min(epcep.cutRadius,
											        t * m_parts[i].scale);
											epcep.cutPtLoc[1] =
											    m_parts[i].q *
												epcep.cutPtLoc[1] *
												m_parts[i].scale +
											    m_parts[i].pos;
											epcep.cutDirLoc[1] =
											    m_parts[i].q * n;
											ptEnd[1] =
											    epcep.cutPtLoc[1] +
											    epcep.cutDirLoc[1] *
												(vmax *
											         m_parts[i].scale);
										}

										if (m_iSimClass == 0 &&
										    !m_pWorld->m_vars.bMultiplayer &&
										    (m_parts[i].flags &
										     (geom_constraint_on_break |
										      geom_colltype_vehicle)) ==
										        (geom_constraint_on_break |
										         geom_colltype_vehicle))
										{
											pe_action_add_constraint aac;
											aac.pBuddy = this;
											aac.id = 1000000;
											aac.pt[0] = ptEnd[1];
											aac.pt[1] = ptEnd[0];
											// aac.pt[1] =
											// aac.pt[0]-pent->m_qrot*epcep.cutDirLoc[0]*(pcaps->r*pent->m_parts[pent->m_nParts-nJoints].scale);
											aac.qframe[0] = aac.qframe[1] =
											    pent->m_qrot *
											    pent->m_parts
												[pent->m_nParts -
											         nJoints]
												    .q *
											    Quat::CreateRotationV0V1(
												Vec3(1, 0, 0),
												pcaps->axis);
											aac.partid[0] =
											    pent->m_parts
												[pent->m_nParts -
											         nJoints]
												    .id;
											aac.partid[1] = m_parts[i].id;
											aac.xlimits[0] =
											    aac.xlimits[1] = 0;
											aac.yzlimits[0] = 0;
											aac.yzlimits[1] = 1.1f;
											aac.damping = 1;
											aac.maxBendTorque =
											    m_parts[i].mass * 0.001f;
											// aac.maxPullForce =
											// m_parts[i].mass*10.0f;
											aac.flags =
											    constraint_ignore_buddy |
											    local_frames;
											aac.sensorRadius =
											    0.5f + pcaps->r;
											pent->Action(&aac);
											m_parts[i].flags &=
											    ~geom_constraint_on_break;
										}
										else
										{
											epcep.cutDirLoc[1].zero();
										}
									}

									epcep.partidSrc = m_parts[i].id;
									epcep.pMeshNew = pMeshNew;
									epcep.nTotParts = 1;
									epcep.pLastUpdate =
									    (bop_meshupdate*)epcep.pMeshNew
										->GetForeignData(DATA_MESHUPDATE);
									for (; epcep.pLastUpdate &&
									       epcep.pLastUpdate->next;
									     epcep.pLastUpdate =
									         epcep.pLastUpdate->next)
										;
									m_pWorld->OnEvent(
									    m_pWorld->m_vars.bLogStructureChanges + 1,
									    &epcep);

									pMeshNew->GetBBox(&bbox1);
									bbox.Basis *= Matrix33(!m_parts[i].q * !m_qrot);
									sz = (bbox1.size * bbox1.Basis.Fabs()) *
									         m_parts[i].scale +
									     Vec3(m_pWorld->m_vars.maxContactGap * 5);
									BBoxNew[0] = BBoxNew[1] =
									    m_pos +
									    m_qrot * (m_parts[i].pos +
									              m_parts[i].q * bbox1.center *
									                  m_parts[i].scale);
									BBoxNew[0] -= sz;
									BBoxNew[1] += sz;
									for (i1 = m_pWorld->GetEntitiesAround(
										      BBoxNew[0], BBoxNew[1], pents,
										      ent_rigid | ent_sleeping_rigid |
											  ent_independent) -
									          1;
									     i1 >= 0; i1--)
									{
										pents[i1]->OnNeighbourSplit(
										    this,
										    (CPhysicalEntity*)epcep.pEntNew);
									}
								}
							}
							if (V0 > 0)
							{
								m_parts[i].mass *= pgeom0->V / V0;
							}
							// if (pOrgMapping) delete[] pOrgMapping;

							epum.pMesh->Lock();
							epum.pLastUpdate = (bop_meshupdate*)epum.pMesh->GetForeignData(
							    DATA_MESHUPDATE);
							for (; epum.pLastUpdate && epum.pLastUpdate->next;
							     epum.pLastUpdate = epum.pLastUpdate->next)
								;
							epum.pMesh->Unlock();
							m_pWorld->OnEvent(m_pWorld->m_vars.bLogStructureChanges + 1,
							                  &epum);
						}
					}
					flags |= m_parts[i].flags;
					iLastIdx = m_iLastIdx + nAddGeoms;
				}
			}

			if (nMeshSplits)
			{
				// attach the islands of non-breakable geoms to the original entity if they are not
				// moved to any chunk
				for (i = m_nParts - 1; i >= 0; i--)
				{
					if (m_parts[i].idmatBreakable < 0)
					{
						for (pMeshIsland = (CTriMesh*)m_parts[i].pLattice; pMeshIsland;
						     pMeshIsland = (CTriMesh*)pMeshIsland->GetForeignData(-2))
						{
							FillGeomParams(gpAux, m_parts[i]);
							gpAux.mass *= pMeshIsland->GetVolume() / m_parts[i].maxdim;
							pgeom1 = m_pWorld->RegisterGeometry(
							    pMeshIsland, m_parts[i].pPhysGeom->surface_idx,
							    m_parts[i].pPhysGeom->pMatMapping,
							    m_parts[i].pPhysGeom->nMats);
							// AddGeometry(pgeom1, &gpAux,-1,1); pgeom1->nRefCount--;
							pAddGeoms[nAddGeoms] = pgeom1;
							gpAddGeoms[nAddGeoms] = gpAux;
							nAddGeoms = min(nAddGeoms + 1,
							                sizeof(gpAddGeoms) / sizeof(gpAddGeoms[0]));
						}
						m_parts[i].pPhysGeom->pGeom->GetBBox(&bbox);
						m_parts[i].maxdim =
						    max(max(bbox.size.x, bbox.size.y), bbox.size.z) * m_parts[i].scale;
						m_parts[i].pLattice = 0;
					}
				}
				for (i = m_nParts - 1; i >= 0; i--)
				{
					if (m_parts[i].flags & geom_removed)
					{
						//(pgeom=m_parts[i].pPhysGeom)->nRefCount++;
						// RemoveGeometry(m_parts[i].id); pgeom->nRefCount--;
						idRemoveGeoms[nRemoveGeoms] = m_parts[i].id;
						pRemoveGeoms[nRemoveGeoms] = m_parts[i].pPhysGeom;
						nRemoveGeoms =
						    min(nRemoveGeoms + 1,
						        (sizeof(idRemoveGeoms) / sizeof(idRemoveGeoms[0])) - 1);
					}
					else if (m_parts[i].flags & geom_invalid)
					{
						m_parts[i].flags = m_parts[i].flagsCollider = 0;
						m_parts[i].idmatBreakable = -1;
					}
				}
				if (pChunkMeshes != pChunkMeshesBuf)
				{
					delete[] pChunkMeshes;
				}
				if (pChunkLattices != pChunkLatticesBuf)
				{
					delete[] pChunkLattices;
				}
				RecomputeMassDistribution();
				// pe_action_reset ar; ar.bClearContacts=0;
				// Action(&ar,1);
				for (j = m_pWorld->GetEntitiesAround(m_BBox[0] - Vec3(0.1f), m_BBox[1] + Vec3(0.1f),
				                                     pents,
				                                     ent_rigid | ent_sleeping_rigid | ent_independent) -
				         1;
				     j >= 0; j--)
				{
					pents[j]->OnNeighbourSplit(this, 0);
				}
			}

			m_timeStructUpdate = m_updStage * m_pWorld->m_vars.tickBreakable;
			m_updStage ^= 1;
			m_nUpdTicks++;
		}
	}

SkipMeshUpdates:
	for (i = 0; i < nAddGeoms; i++)
	{
		AddGeometry(pAddGeoms[i], gpAddGeoms + i, -1, 1);
		pAddGeoms[i]->nRefCount--;
	}
	for (i = 0; i < nRemoveGeoms; i++)
	{
		pRemoveGeoms[i]->nRefCount++;
		RemoveGeometry(idRemoveGeoms[i]);
		pRemoveGeoms[i]->nRefCount--;
	}
	if (m_iSimClass && m_nParts && m_parts[0].flags & geom_break_approximation)
	{
		CapsulizePart(0);
	}
	if (!(m_flags & aef_recorded_physics) && (nRemoveGeoms || nMeshSplits))
	{
		for (i1 = m_pWorld->GetEntitiesAround(m_BBox[0] - Vec3(0.1f), m_BBox[1] + Vec3(0.1f), pents,
		                                      ent_rigid | ent_sleeping_rigid | ent_independent) -
		          1;
		     i1 >= 0; i1--)
		{
			pents[i1]->OnNeighbourSplit(this, 0);
		}
	}

	if (flags & geom_structure_changes)
	{
		// WriteLock lock(m_lockUpdate);
		if (GetType() != PE_STATIC)
		{
			for (i = 0; i < m_nColliders; i++)
			{
				if (m_pColliders[i] != this)
				{
					m_pColliders[i]->Awake();
				}
			}
		}

		Vec3 BBox[2];
		ComputeBBox(BBox, 0);
		i = m_pWorld->RepositionEntity(this, 1, BBox);
		{
			WriteLock lock(m_lockUpdate);
			ComputeBBox(m_BBox);
			AtomicAdd(&m_pWorld->m_lockGrid, -i);
		}
	}

	return m_updStage == 0 || flags & geom_structure_changes;
}

void CPhysicalEntity::OnContactResolved(entity_contact* pContact, int iop, int iGroupId)
{
	int i;
	if (iop < 2 && (unsigned int)pContact->ipart[iop] < (unsigned int)m_nParts &&
	    (m_flags & aef_recorded_physics || !(pContact->pent[iop ^ 1]->m_flags & pef_never_break) ||
	     m_iSimClass > 0 && GetRigidBody()->v.len2() > sqr(3.0f)) &&
	    !(pContact->flags & contact_angular))
	{
		Vec3 n, pt = pContact->pt[iop & 1];
		if (!(pContact->flags & contact_rope) || iop == 0)
		{
			n = pContact->n * (1 - (iop * 2));
		}
		else if (iop == 1)
		{
			n = pContact->vreq * pContact->K(0, 0);
		}
		else
		{
			rope_solver_vtx& svtx = ((rope_solver_vtx*)pContact->pBounceCount)[iop - 2];
			n = svtx.P;
			pt = svtx.r + svtx.pbody->pos;
		}
		float scale =
		    1.0f + ((m_pWorld->m_vars.breakImpulseScale - 1.0f) *
		            (iszero((int)m_flags & pef_override_impulse_scale) | m_pWorld->m_vars.bMultiplayer));
		if (m_pStructure)
		{
			Vec3 dP, r, sz;
			float Plen2, Llen2;
			i = pContact->ipart[iop];
			dP = n * (pContact->Pspare * scale);
			r = m_qrot * (m_parts[i].q * m_parts[i].pPhysGeom->origin * m_parts[i].scale + m_parts[i].pos) +
			    m_pos - pt;
			if (pContact->flags & contact_rope || pContact->pent[iop ^ 1]->GetType() != PE_RIGID ||
			    (((CRigidEntity*)pContact->pent[iop ^ 1])->m_prevv - pContact->pbody[iop ^ 1]->v).len2() <
			        4)
			{
				Plen2 = (m_pStructure->Fext[i] += dP).len2();
				Llen2 = (m_pStructure->Text[i] += r ^ dP).len2();
			}
			else
			{
				Plen2 = (m_pStructure->Pext[i] += dP).len2();
				Llen2 = (m_pStructure->Lext[i] += r ^ dP).len2();
			}
			sz = m_parts[i].BBox[1] - m_parts[i].BBox[0];
			if (max(Plen2, Llen2 * sqr(sz.x + sz.y + sz.z) * 0.1f) >
			    sqr(m_parts[i].mass * 0.01f) * m_pWorld->m_vars.gravity.len2() *
			        (0.01f + 0.99f * iszero((int)m_flags & aef_recorded_physics)))
			{
				m_pWorld->MarkEntityAsDeforming(this), m_iGroup = iGroupId;
			}
		}

		if (m_parts[i = pContact->ipart[iop]].pLattice && m_parts[i].idmatBreakable >= 0)
		{
			float rdensity = m_parts[i].pLattice->m_density * m_parts[i].pPhysGeom->V *
			                 cube(m_parts[i].scale) / m_parts[i].mass;
			Vec3 ptloc = ((pContact->pt[iop] - m_pos) * m_qrot - m_parts[i].pos) * m_parts[i].q, dP;
			if (m_parts[i].scale != 1.0f)
			{
				ptloc /= m_parts[i].scale;
			}
			dP = n * (pContact->Pspare * rdensity * scale);

			if (m_parts[i].pLattice->AddImpulse(ptloc, (dP * m_qrot) * m_parts[i].q, Vec3(0, 0, 0),
			                                    m_pWorld->m_vars.gravity, m_pWorld->m_updateTimes[0]) &&
			    !(m_flags & pef_deforming))
			{
				m_updStage = 0, m_iGroup = iGroupId, m_pWorld->MarkEntityAsDeforming(this);
			}
		}
	}
}

int CPhysicalEntity::GetStateSnapshot(TSerialize ser, float time_back, int flags)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		bool bVal;
		int i, id;
		ser.Value("hasJoints", bVal = (m_pStructure != 0 && m_pStructure->bModified > 0), 'bool');
		if (bVal)
		{
			ser.Value("nJoints", m_pStructure->nJoints);
			for (i = 0; i < m_pStructure->nJoints; i++)
			{
				ser.BeginGroup("joint");
				ser.Value("id", m_pStructure->pJoints[i].id);
				ser.Value("ipart0", id = m_pStructure->pJoints[i].ipart[0] >= 0
				                             ? m_parts[m_pStructure->pJoints[i].ipart[0]].id
				                             : -1);
				ser.Value("ipart1", id = m_pStructure->pJoints[i].ipart[1] >= 0
				                             ? m_parts[m_pStructure->pJoints[i].ipart[1]].id
				                             : -1);
				ser.Value("pt", m_pStructure->pJoints[i].pt);
				ser.Value("n", m_pStructure->pJoints[i].n);
				ser.Value("maxForcePush", m_pStructure->pJoints[i].maxForcePush);
				ser.Value("maxForcePull", m_pStructure->pJoints[i].maxForcePull);
				ser.Value("maxForceShift", m_pStructure->pJoints[i].maxForceShift);
				ser.Value("maxTorqueBend", m_pStructure->pJoints[i].maxTorqueBend);
				ser.Value("maxTorqueTwist", m_pStructure->pJoints[i].maxTorqueTwist);
				ser.Value("bBreakable", m_pStructure->pJoints[i].bBreakable);
				ser.Value("szHelper", m_pStructure->pJoints[i].size);
				ser.EndGroup();
			}
		}
	}
	else if ((flags & ssf_from_child_class) == 0)
	{
		SRigidEntityNetSerialize helper;
		helper.angvel = ZERO;
		helper.pos = m_pos;
		helper.rot = m_qrot;
		helper.simclass = 0;
		helper.vel = ZERO;
		helper.Serialize(ser);
	}

	return 1;
}

int CPhysicalEntity::SetStateFromSnapshot(TSerialize ser, int flags)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		bool bVal;
		ser.Value("hasJoints", bVal, 'bool');
		if (bVal)
		{
			int i, nJoints;
			pe_params_structural_joint psj;
			if (m_pStructure)
			{
				m_pStructure->nJoints = 0;
			}
			ser.Value("nJoints", nJoints);
			for (i = 0; i < nJoints; i++)
			{
				ser.BeginGroup("joint");
				ser.Value("id", psj.id);
				ser.Value("ipart0", psj.partid[0]);
				ser.Value("ipart1", psj.partid[1]);
				ser.Value("pt", psj.pt);
				ser.Value("n", psj.n);
				ser.Value("maxForcePush", psj.maxForcePush);
				ser.Value("maxForcePull", psj.maxForcePull);
				ser.Value("maxForceShift", psj.maxForceShift);
				ser.Value("maxTorqueBend", psj.maxTorqueBend);
				ser.Value("maxTorqueTwist", psj.maxTorqueTwist);
				ser.Value("bBreakable", psj.bBreakable);
				ser.Value("szHelper", psj.szSensor);
				SetParams(&psj);
				ser.EndGroup();
			}
			if (m_pStructure)
			{
				m_pStructure->bModified = 1;
			}
		}
	}
	else if ((flags & ssf_from_child_class) == 0)
	{
		SRigidEntityNetSerialize helper;
		helper.Serialize(ser);
	}
	return 1;
}

void CPhysicalEntity::GetLocTransform(int ipart, Vec3& offs, quaternionf& q, float& scale)
{
	if ((unsigned int)ipart < (unsigned int)m_nParts)
	{
		q = m_qrot * m_parts[ipart].q;
		offs = m_qrot * m_parts[ipart].pos + m_pos;
		scale = m_parts[ipart].scale;
	}
	else
	{
		q.SetIdentity();
		offs.zero();
		scale = 1.0f;
	}
}

int CPhysicalEntity::TouchesSphere(const Vec3& center, float r)
{
	ReadLock lock(m_lockUpdate);
	int i;
	geom_world_data gwd[2];
	intersection_params ip;
	primitives::sphere sph;
	geom_contact* pcont;
	CSphereGeom sphGeom;
	sph.center.zero();
	sph.r = r;
	sphGeom.CreateSphere(&sph);
	gwd[0].offset = center;
	ip.bStopAtFirstTri = ip.bNoBorder = ip.bNoAreaContacts = true;

	for (i = 0; i < m_nParts; i++)
	{
		if (m_parts[i].flags & (geom_colltype_solid | geom_colltype_ray))
		{
			gwd[1].offset = m_pos + m_qrot * m_parts[i].pos;
			gwd[1].R = Matrix33(m_qrot * m_parts[i].q);
			gwd[1].scale = m_parts[i].scale;
			if (sphGeom.Intersect(m_parts[i].pPhysGeomProxy->pGeom, gwd, gwd + 1, &ip, pcont))
			{
				CryInterlockedAdd(ip.plock, -WRITE_LOCK_VAL);
				return i;
			}
		}
	}
	return -1;
}

int CPhysicalEntity::CapsulizePart(int ipart)
{
	struct caps_piece
	{
		primitives::capsule caps;
		Vec3 pos;
		quaternionf q;
	};

	IGeometry* pGeom;
	caps_piece pieceBuf[16], *pieces = pieceBuf;
	pe_geomparams gp;
	int i, j, nSlices, nParts = 0;

	{
		ReadLock lock(m_lockUpdate);
		if (ipart >= m_nParts || (pGeom = m_parts[ipart].pPhysGeom->pGeom)->GetPrimitiveCount() < 50 ||
		    m_pWorld->m_vars.approxCapsLen <= 0)
		{
			return 0;
		}

		CTriMesh* g_pSliceMesh = 0;
		if (!g_pSliceMesh)
		{
			static Vec3 vtx[] = {Vec3(-1, -1, 0), Vec3(1, -1, 0), Vec3(1, 1, 0), Vec3(-1, 1, 0)};
			static int indices[] = {0, 1, 2, 0, 2, 3};
			g_pSliceMesh = (CTriMesh*)m_pWorld->CreateMesh(
			    vtx, strided_pointer<unsigned short>((unsigned short*)indices, sizeof(int)), 0, 0, 2,
			    mesh_shared_idx | mesh_shared_vtx | mesh_no_vtx_merge | mesh_SingleBB, 0);
		}
		int iax, icnt, nLastParts = 0, nNewParts, nPartsAlloc = sizeof(pieceBuf) / sizeof(pieceBuf[0]);
		Vec3 axis, axcaps, ptc, ptcPrev, ptcCur;
		quotientf t, dist, mindist;
		float step, r0;
		primitives::box bbox;
		primitives::capsule caps;
		geom_world_data gwd;
		intersection_params ip;

		ip.bNoAreaContacts = true;
		ip.maxUnproj = 0.01f;
		geom_contact* pcont;
		pGeom->GetBBox(&bbox);
		axis = bbox.Basis.GetRow(iax = idxmax3((float*)&bbox.size));
		nSlices = min(m_pWorld->m_vars.nMaxApproxCaps + 1,
		              physics_float2int((bbox.size[iax] * 2 / m_pWorld->m_vars.approxCapsLen) - 0.5f) + 2);
		if (nSlices < 4)
		{
			return 0;
		}
		axis *= sgnnz(axis[idxmax3((float*)axis.abs())]);
		axcaps.zero()[iax] = 1;
		gwd.R = bbox.Basis.T() * Matrix33::CreateRotationV0V1(Vec3(0, 0, 1), axcaps);
		gwd.scale = max(bbox.size[inc_mod3[iax]], bbox.size[dec_mod3[iax]]) * 1.1f;
		gwd.offset = bbox.center - axis * (bbox.size[iax] * 0.9f);
		caps.center.zero();
		caps.axis.Set(0, 0, 1);
		gp.flagsCollider = m_parts[ipart].flagsCollider;
		gp.flags = m_parts[ipart].flags & geom_colltype_solid;
		gp.density = gp.mass = 0;
		gp.surface_idx = m_parts[ipart].surface_idx;
		if (m_parts[ipart].pMatMapping)
		{
			gp.surface_idx = m_parts[ipart].pMatMapping[gp.surface_idx];
		}
		gp.idmatBreakable = -2 - m_parts[ipart].id;
		// gp.scale = m_parts[ipart].scale;

		for (i = 0;; i++)
		{
			if (pGeom->Intersect(g_pSliceMesh, 0, &gwd, &ip, pcont))
			{
				WriteLockCond lockColl(*ip.plock, 0);
				lockColl.SetActive();
				if (!pcont[0].nborderpt)
				{
					return 0;
				}
				float r, rmin;
				for (j = 0, r0 = 0, rmin = bbox.size[iax]; j < pcont[0].nborderpt; j++)
				{
					r0 += (r = (pcont[0].ptborder[j] - pcont[0].center).len());
					rmin = min(rmin, r);
				}
				r0 /= pcont[0].nborderpt;
				if (r0 - rmin > r0 * 0.4f && i == 0)
				{
					gwd.offset += axis * (bbox.size[iax] * 0.9f);
				}
				else
				{
					gwd.offset = bbox.center - axis * (bbox.size[iax] - r0);
					step = (bbox.size[iax] * 2 - r0 - m_pWorld->m_vars.maxContactGap * 5) /
					       (nSlices - 1);
					r0 *= m_parts[ipart].scale;
					break;
				}
			}
			else
			{
				return 0;
			}
		}

		for (i = 0; i < nSlices; i++, gwd.offset += axis * step)
		{
			if (!(icnt = pGeom->Intersect(g_pSliceMesh, 0, &gwd, &ip, pcont)))
			{
				break;
			}
			nNewParts = 0;
			WriteLockCond lockColl(*ip.plock, 0);
			lockColl.SetActive();
			if (!i)
			{
				for (--icnt, ptcPrev.zero(); icnt >= 0; i += pcont[icnt--].nborderpt)
				{
					for (j = 0; j < pcont[icnt].nborderpt; j++)
					{
						ptcPrev += pcont[icnt].ptborder[j];
					}
				}
				if (i > 0)
				{
					ptcPrev /= i;
				}
				else
				{
					ptcPrev = pcont[0].center;
				}
				ptcPrev = m_parts[ipart].q * ptcPrev * m_parts[ipart].scale;
				i = 0;
			}
			else
			{
				for (--icnt, nNewParts = 0; icnt >= 0; icnt--)
				{
					if (!pcont[icnt].bClosed)
					{
						continue; // failure
					}
					ptcCur = m_parts[ipart].q * pcont[icnt].center * m_parts[ipart].scale;
					for (j = nParts - nNewParts - nLastParts, mindist.set(1, 0);
					     j < nParts - nNewParts; j++)
					{
						axcaps = pieces[j].q * Vec3(0, 0, 1);
						ptc = pieces[j].pos + axcaps * (pieces[j].caps.hh * gp.scale);
						t.set((m_parts[ipart].pos +
						       m_parts[ipart].q * gwd.offset * m_parts[ipart].scale - ptc) *
						          axcaps,
						      axcaps * axis);
						dist.set(((ptc - ptcCur) * t.y + axcaps * t.x).len2(), t.y * t.y);
						if (dist < mindist)
						{
							mindist = dist;
							ptcPrev = ptc;
						}
					}
					for (j = 0, caps.r = 0; j < pcont[icnt].nborderpt; j++)
					{
						caps.r += (pcont[icnt].ptborder[j] - pcont[icnt].center).len();
					}
					caps.r = caps.r / pcont[icnt].nborderpt * m_parts[ipart].scale;
					if (i == 1 && fabs_tpl(caps.r - r0) < min(caps.r, r0) * 0.5f)
					{
						caps.r = r0;
					}
					caps.hh = (ptcCur - ptcPrev).len() * 0.5f;
					if (caps.hh == 0)
					{
						continue;
					}
					gp.pos = (ptcPrev + ptcCur) * 0.5f;
					axcaps = (ptcCur - ptcPrev) / (caps.hh * 2);
					gp.q = Quat::CreateRotationV0V1(Vec3(0, 0, 1), axcaps);
					if (nParts == nPartsAlloc)
					{
						if (pieces == pieceBuf)
						{
							memcpy(pieces = new caps_piece[nPartsAlloc += 4], pieceBuf,
							       nParts);
						}
						else
						{
							ReallocateList(pieces, nParts, nPartsAlloc += 4);
						}
					}
					pieces[nParts].caps = caps;
					pieces[nParts].pos = gp.pos;
					pieces[nParts].q = gp.q;
					nParts++;
					nNewParts++;
				}
			}
			nLastParts = nNewParts;
		}
	}

	if (i < nSlices)
	{
		return 0;
	}
	float V0 = m_parts[ipart].pPhysGeom->V * cube(m_parts[ipart].scale), V1 = 0;
	for (j = 0; j < nParts; j++)
	{
		V1 += g_PI * sqr(pieces[j].caps.r) * pieces[j].caps.hh * 2 * cube(gp.scale);
	}
	if (fabs_tpl(V0 - V1) > max(V0, V1) * 0.2f)
	{
		return 0;
	}
	for (j = 0; j < nParts; j++)
	{
		gp.pos = pieces[j].pos;
		gp.q = pieces[j].q;
		pieces[j].caps.r = max(0.03f, pieces[j].caps.r);
		phys_geometry* pgeom =
		    m_pWorld->RegisterGeometry(m_pWorld->CreatePrimitive(primitives::capsule::type, &pieces[j].caps));
		if (AddGeometry(pgeom, &gp) < 0)
		{
			goto exitcp;
		}
		pgeom->nRefCount--;
	}
	m_parts[ipart].flags &= ~geom_colltype_solid;
	m_parts[ipart].flagsCollider = 0;
exitcp:
	if (pieces != pieceBuf)
	{
		delete[] pieces;
	}
	return j;
}

int CPhysicalEntity::GetMatId(int id, int ipart)
{
	if (ipart < m_nParts)
	{
		id += (m_parts[ipart].surface_idx - id) & id >> 31;
		intptr_t mask = iszero_mask(m_parts[ipart].pMatMapping);
		int idummy = 0,
		    *pMatMapping = (int*)(((intptr_t)m_parts[ipart].pMatMapping & ~mask) | ((intptr_t)&idummy & mask)),
		    nMats;
		nMats = m_parts[ipart].nMats + (65536 & (int)mask);
		return (id & (int)mask) | pMatMapping[id & ~(int)mask & ((id & ~(int)mask) - nMats) >> 31];
	}
	else
	{
		return id;
	}
}

int CPhysicalEntity::SetStateFromTypedSnapshot(TSerialize ser, int iSnapshotType, int flags)
{
	static CPhysicalEntity g_entStatic(0);
	static CRigidEntity g_entRigid(0);
	static CWheeledVehicleEntity g_entWheeled(0);
	static CLivingEntity g_entLiving(0);
	static CParticleEntity g_entParticle(0);
	static CArticulatedEntity g_entArticulated(0);
	static CRopeEntity g_entRope(0);
	static CSoftEntity g_entSoft(0);
	static CPhysicalEntity* g_pTypedEnts[] = {&g_entStatic,      &g_entStatic, &g_entRigid,
	                                          &g_entWheeled,     &g_entLiving, &g_entParticle,
	                                          &g_entArticulated, &g_entRope,   &g_entSoft};

	if (GetType() == iSnapshotType)
	{
		return SetStateFromSnapshot(ser, flags);
	}
	return g_pTypedEnts[min(sizeof(g_pTypedEnts) / sizeof(g_pTypedEnts[0]), max(0, iSnapshotType))]
	    ->SetStateFromSnapshot(ser, flags);
}
