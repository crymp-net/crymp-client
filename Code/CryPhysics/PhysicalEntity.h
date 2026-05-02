#pragma once

#include "PhysicalPlaceholder.h"

class CPhysicalWorld;
class CRayGeom;
class CGeometry;
class CTetrLattice;
class RigidBody;
struct entity_contact;
struct SExplosionInfo;

enum cbbx_flags
{
	part_added = 1,
	update_part_bboxes = 2
};

struct coord_block
{
	Vec3 pos;
	quaternionf q;
};

struct coord_block_BBox
{
	Vec3 pos;
	quaternionf q;
	float scale;
	Vec3 BBox[2];
};

struct geom
{
	Vec3 pos;
	quaternionf q;
	float scale;
	Vec3 BBox[2];
	phys_geometry *pPhysGeom, *pPhysGeomProxy;
	int id;
	float mass;
	unsigned int flags, flagsCollider;
	float maxdim;
	float minContactDist;
	int surface_idx : 10;
	int idmatBreakable : 14;
	int nMats : 8;
	CTetrLattice* pLattice;
	int* pMatMapping;
	coord_block_BBox* pNewCoords;
};

struct SStructuralJoint
{
	int id;
	int ipart[2];
	Vec3 pt;
	Vec3 n;
	float maxForcePush, maxForcePull, maxForceShift;
	float maxTorqueBend, maxTorqueTwist;
	int bBreakable, bBroken;
	float size;
	float tension;
	int itens;
};

struct SStructureInfo
{
	Vec3 *Pext, *Lext;
	Vec3 *Fext, *Text;
	SStructuralJoint* pJoints;
	int nJoints, nJointsAlloc;
	int nLastBrokenJoints;
	int bModified;
	int idpartBreakOrg;
	int nPartsAlloc;
	float timeLastUpdate;
	int nPrevJoints;
	float prevdt;
	float minSnapshotTime;
	float autoDetachmentDist;
	Vec3 lastExplPos;
	Vec3 *Pexpl, *Lexpl;
};

class CPhysicalEntity : public CPhysicalPlaceholder
{
public:
	CPhysicalEntity(CPhysicalWorld* pworld);
	~CPhysicalEntity();
	pe_type GetType() override { return PE_STATIC; }

	int AddRef() override;
	int Release() override;

	int SetParams(pe_params*, int bThreadSafe = 1) override;
	int GetParams(pe_params*) override;
	int GetStatus(pe_status*) override;
	int Action(pe_action*, int bThreadSafe = 1) override;
	int AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id = -1, int bThreadSafe = 1) override;
	void RemoveGeometry(int id, int bThreadSafe = 1) override;
	float ComputeExtent(GeomQuery& geo, EGeomForm eForm);
	void GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm);
	void* GetForeignData(int itype = 0) override { return itype == m_iForeignData ? m_pForeignData : 0; }
	int GetiForeignData() override { return m_iForeignData; }
	IPhysicalWorld* GetWorld() override { return (IPhysicalWorld*)m_pWorld; }
	CPhysicalEntity* GetEntity() { return this; }
	CPhysicalEntity* GetEntityFast() { return this; }

	void StartStep(float time_interval) override {}
	virtual float GetMaxTimeStep(float time_interval) { return time_interval; }
	virtual float GetLastTimeStep(float time_interval) { return time_interval; }
	int Step(float time_interval) override { return 1; }
	int DoStep(float time_interval, int iCaller = 0) override { return 1; }
	void StepBack(float time_interval) override {}
	virtual int GetContactCount(int nMaxPlaneContacts) { return 0; }
	virtual int RegisterContacts(float time_interval, int nMaxPlaneContacts) { return 0; }
	virtual int Update(float time_interval, float damping) { return 1; }
	virtual float CalcEnergy(float time_interval) { return 0; }
	virtual float GetDamping(float time_interval) { return 1.0f; }
	virtual bool IgnoreCollisionsWith(CPhysicalEntity* pent, int bCheckConstraints = 0) { return false; }
	virtual void GetSleepSpeedChange(int ipart, Vec3& v, Vec3& w)
	{
		v.zero();
		w.zero();
	}

	virtual int AddCollider(CPhysicalEntity* pCollider);
	virtual int RemoveCollider(CPhysicalEntity* pCollider, bool bAlwaysRemove = true);
	virtual int RemoveContactPoint(CPhysicalEntity* pCollider, const Vec3& pt, float mindist2) { return -1; }
	virtual int HasContactsWith(CPhysicalEntity* pent) { return 0; }
	virtual int HasCollisionContactsWith(CPhysicalEntity* pent) { return 0; }
	virtual int HasConstraintContactsWith(CPhysicalEntity* pent, int flagsIgnore = 0) { return 0; }
	virtual void AlertNeighbourhoodND();
	virtual int Awake(int bAwake = 1, int iSource = 0) { return 0; }
	virtual int IsAwake(int ipart = -1) { return 0; }
	int GetColliders(CPhysicalEntity**& pentlist)
	{
		pentlist = m_pColliders;
		return m_nColliders;
	}
	virtual int RayTrace(CRayGeom* pRay, geom_contact*& pcontacts, volatile int*& plock) { return 0; }
	virtual void ApplyVolumetricPressure(const Vec3& epicenter, float kr, float rmin) {}
	virtual void OnContactResolved(entity_contact* pContact, int iop, int iGroupId);

	virtual void OnNeighbourSplit(CPhysicalEntity* pentOrig, CPhysicalEntity* pentNew) {}

	virtual RigidBody* GetRigidBody(int ipart = -1, int bWillModify = 0);
	virtual RigidBody* GetRigidBodyData(RigidBody* pbody, int ipart = -1) { return GetRigidBody(ipart); }
	virtual float GetMass(int ipart) { return m_parts[ipart].mass; }
	virtual void GetContactMatrix(const Vec3& pt, int ipart, Matrix33& K) {}
	virtual void GetSpatialContactMatrix(const Vec3& pt, int ipart, float Ibuf[][6]) {}
	virtual float GetMassInv() { return 0; }
	int IsPointInside(Vec3 pt);
	virtual void GetLocTransform(int ipart, Vec3& offs, quaternionf& q, float& scale);
	virtual void DetachPartContacts(int ipart, int iop0, CPhysicalEntity* pent, int iop1, int bCheckIfEmpty = 1) {}
	int TouchesSphere(const Vec3& center, float r);

	virtual void DrawHelperInformation(IPhysRenderer* pRenderer, int flags);
	void GetMemoryStatistics(ICrySizer*) override {}

	int GetStateSnapshot(TSerialize ser, float time_back = 0, int flags = 0) override;
	int SetStateFromSnapshot(TSerialize ser, int flags = 0) override;
	int SetStateFromTypedSnapshot(TSerialize ser, int iSnapshotType, int flags = 0) override;
	int PostSetStateFromSnapshot() override { return 1; }
	unsigned int GetStateChecksum() override { return 0; }

	int UpdateStructure(float time_interval, pe_explosion* pexpl, int iCaller = 0, Vec3 gravity = Vec3(0));
	virtual void RecomputeMassDistribution(int ipart = -1, int bMassChanged = 1) {}

	virtual void ComputeBBox(Vec3* BBox, int flags = update_part_bboxes);
	void WriteBBox(Vec3* BBox)
	{
		m_BBox[0] = BBox[0];
		m_BBox[1] = BBox[1];
		if (m_pEntBuddy)
		{
			m_pEntBuddy->m_BBox[0] = BBox[0];
			m_pEntBuddy->m_BBox[1] = BBox[1];
		}
	}

	void UpdatePartIdmatBreakable(int ipart, int nParts = -1);
	int CapsulizePart(int ipart);
	int GetMatId(int id, int ipart);

	int m_iDeletionTime;
	volatile int m_nRefCount;
	unsigned int m_flags;
	CPhysicalEntity *m_next, *m_prev;
	CPhysicalWorld* m_pWorld;
	int m_nRefCountPOD : 16;
	int m_iLastPODUpdate : 16;

	int m_iPrevSimClass : 24;
	int m_bMoved : 8;
	int m_iGroup;
	CPhysicalEntity *m_next_coll, *m_next_coll1, *m_next_coll2;

	Vec3 m_pos;
	quaternionf m_qrot;
	coord_block* m_pNewCoords;

	CPhysicalEntity** m_pColliders;
	int m_nColliders, m_nCollidersAlloc;

	CPhysicalEntity *m_next_aux, *m_prev_aux;
	CPhysicalEntity* m_pOuterEntity;
	CGeometry* m_pBoundingGeometry;
	int m_bProcessed_aux;

	float m_timeIdle, m_maxTimeIdle;
	int m_bPermanent;

	float m_timeStructUpdate;
	int m_updStage, m_nUpdTicks;
	SExplosionInfo* m_pExpl;

	geom *m_parts, m_defpart;
	int m_nParts, m_nPartsAlloc;
	int m_iLastIdx;
	volatile int m_lockPartIdx;

	primitives::plane* m_ground;
	int m_nGroundPlanes;

	SStructureInfo* m_pStructure;
};

extern CPhysicalEntity g_StaticPhysicalEntity;
