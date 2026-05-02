#pragma once

#include "PhysicalEntity.h"

class CTriMesh;
class RigidBody;
struct entity_contact;
struct rope_vtx;
struct rope_segment;
struct SRopeCheckPart;

struct rope_solver_vtx
{
	Vec3 r, v, P;
	RigidBody* pbody;
	int iBody;
	int ivtx;
};

class CRopeEntity final : public CPhysicalEntity
{
public:
	explicit CRopeEntity(CPhysicalWorld* pWorld);
	~CRopeEntity();

	pe_type GetType() override { return PE_ROPE; }

	int SetParams(pe_params*, int bThreadSafe = 1) override;
	int GetParams(pe_params*) override;
	int GetStatus(pe_status*) override;
	int Action(pe_action*, int bThreadSafe = 1) override;

	void StartStep(float time_interval) override;
	float GetMaxTimeStep(float time_interval);
	int Step(float time_interval) override;
	int Awake(int bAwake = 1, int iSource = 0);
	int IsAwake(int ipart = -1) { return m_bAwake; }
	void AlertNeighbourhoodND();
	int RayTrace(CRayGeom* pRay, geom_contact*& pcontacts, volatile int*& plock);
	float GetMass(int ipart) { return m_mass / m_nSegs; }
	float GetMassInv() { return 1E26f; }
	RigidBody* GetRigidBodyData(RigidBody* pbody, int ipart = -1);
	void GetLocTransform(int ipart, Vec3& offs, quaternionf& q, float& scale);
	void EnforceConstraints(float seglen, const quaternionf& qtv, const Vec3& offstv, float scaletv);
	void OnNeighbourSplit(CPhysicalEntity* pentOrig, CPhysicalEntity* pentNew);
	int RegisterContacts(float time_interval, int nMaxPlaneContacts);
	int Update(float time_interval, float damping);
	float GetDamping(float time_interval) { return max(0.0f, 1.0f - (m_damping * time_interval)); }
	float CalcEnergy(float time_interval) { return time_interval > 0 ? m_energy : 0.0f; }
	float GetLastTimeStep(float time_interval) { return m_lastTimeStep; }
	void ApplyVolumetricPressure(const Vec3& epicenter, float kr, float rmin);
	void RecalcBBox();

	enum snapver
	{
		SNAPSHOT_VERSION = 8
	};

	int GetStateSnapshot(TSerialize ser, float time_back = 0, int flags = 0) override;
	int SetStateFromSnapshot(TSerialize ser, int flags) override;

	void DrawHelperInformation(IPhysRenderer* pRenderer, int flags) override;
	void GetMemoryStatistics(ICrySizer*) override {}

	void MeshVtxUpdated();
	void AllocSubVtx();
	void FillVtxContactData(rope_vtx* pvtx, int iseg, SRopeCheckPart& cp, geom_contact* pcontact);

	Vec3 m_gravity, m_gravity0;
	float m_damping;
	float m_maxAllowedStep;
	float m_Emin;
	int m_bAwake;
	float m_timeStepPerformed, m_timeStepFull;
	int m_nSlowFrames;
	float m_lastTimeStep;
	float m_timeLastActive;
	volatile int m_lockVtx;

	float m_length;
	int m_nSegs;
	float m_mass;
	float m_collDist;
	int m_surface_idx;
	float m_friction;
	float m_stiffness;
	float m_stiffnessAnim, m_dampingAnim, m_stiffnessDecayAnim;
	int m_bTargetPoseActive;
	Vec3 m_wind, m_wind0, m_wind1;
	float m_airResistance, m_windVariance, m_windTimer;
	float m_waterResistance, m_rdensity;
	float m_jointLimit;
	float m_szSensor;
	float m_maxForce;
	int m_flagsCollider;
	int m_bHasContacts;
	rope_segment* m_segs;

	CPhysicalEntity* m_pTiedTo[2];
	Vec3 m_ptTiedLoc[2];
	int m_iTiedPart[2];
	int m_idConstraint;
	int m_iConstraintClient;
	Vec3 m_posBody[2][2];
	quaternionf m_qBody[2][2];
	Vec3 m_dir0dst;
	Vec3 m_collBBox[2];

	rope_vtx* m_vtx;
	rope_vtx* m_vtx1;
	rope_solver_vtx* m_vtxSolver;
	int m_nVtx, m_nVtxAlloc;
	int m_nFragments;
	CTriMesh* m_pMesh;
	Vec3 m_lastMeshOffs;
	int m_nMaxSubVtx;
	int* m_idx;
	int m_bStrained;
	float m_frictionPull;
	float m_energy;
	entity_contact* m_pContact;
};
