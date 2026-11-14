#pragma once

#include "SphereGeom.h"
#include "PhysicalEntity.h"

class CCylinderGeom;
class RigidBody;
struct entity_contact;

struct le_contact
{
	CPhysicalEntity* pent;
	int ipart;
	Vec3 pt, ptloc;
	Vec3 n;
	float penetration;
	Vec3 center;
	entity_contact* pSolverContact[2];
};

struct le_history_item
{
	Vec3 pos;
	quaternionf q;
	Vec3 v;
	int bFlying;
	Vec3 nslope;
	float timeFlying;
	float minFlyTime;
	float timeUseLowCap;
	int idCollider;
	int iColliderPart;
	Vec3 posColl;
	float dt;
};

class CLivingEntity final : public CPhysicalEntity
{
public:
	explicit CLivingEntity(CPhysicalWorld* pWorld);
	~CLivingEntity();

	pe_type GetType() override { return PE_LIVING; }

	int SetParams(pe_params*, int bThreadSafe = 1) override;
	int GetParams(pe_params*) override;
	int GetStatus(pe_status*) override;
	int Action(pe_action*, int bThreadSafe = 1) override;
	void StartStep(float time_interval) override;
	float GetMaxTimeStep(float time_interval);
	int Step(float time_interval) override;
	int StepBackEx(float time_interval, bool bRollbackHistory = true);
	void StepBack(float time_interval) override { StepBackEx(time_interval); }
	float CalcEnergy(float time_interval);
	int RegisterContacts(float time_interval, int nMaxPlaneContacts);
	int Update(float time_interval, float damping);
	int Awake(int bAwake = 1, int iSource = 0);

	void AlertNeighbourhoodND()
	{
		ReleaseGroundCollider();
		CPhysicalEntity::AlertNeighbourhoodND();
	}

	void ComputeBBox(Vec3* BBox, int flags = update_part_bboxes);
	RigidBody* GetRigidBody(int ipart = -1, int bWillModify = 0);
	RigidBody* GetRigidBodyData(RigidBody* pbody, int ipart = -1);

	void OnContactResolved(entity_contact* pContact, int iop, int iGroupId);

	int AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id = -1, int bThreadSafe = 1) override;
	void RemoveGeometry(int id, int bThreadSafe = 1) override;

	void DrawHelperInformation(IPhysRenderer* pRenderer, int flags) override;

	enum snapver
	{
		SNAPSHOT_VERSION = 2
	};

	int GetStateSnapshot(TSerialize ser, float time_back = 0, int flags = 0) override;
	int SetStateFromSnapshot(TSerialize ser, int flags = 0) override;

	void GetMemoryStatistics(ICrySizer*) override {}

	float GetMassInv() { return m_massinv; }

	void GetContactMatrix(const Vec3& pt, int ipart, Matrix33& K)
	{
		K(0, 0) += m_massinv;
		K(1, 1) += m_massinv;
		K(2, 2) += m_massinv;
	}

	void GetSpatialContactMatrix(const Vec3& pt, int ipart, float Ibuf[][6])
	{
		Ibuf[3][0] += m_massinv;
		Ibuf[4][1] += m_massinv;
		Ibuf[5][2] += m_massinv;
	}

	float ShootRayDown(CPhysicalEntity** pentlist, int nents, const Vec3& pos, Vec3& nslope,
	                   float time_interval = 0, bool bUseRotation = false, bool bUpdateGroundCollider = false,
	                   bool bIgnoreSmallObjects = true);
	void AddLegsImpulse(const Vec3& vel, const Vec3& nslope, bool bInstantChange);
	void ReleaseGroundCollider();
	void SetGroundCollider(CPhysicalEntity* pCollider, int bAcceptStatic = 0);
	Vec3 SyncWithGroundCollider(float time_interval);
	void RegisterContact(const Vec3& posSelf, const Vec3& pt, const Vec3& n, CPhysicalEntity* pCollider, int ipart,
	                     int idmat, float imp = 0, int bLegsContact = 0);
	void RegisterUnprojContact(const le_contact& unproj);
	float UnprojectionNeeded(const Vec3& pos, const quaternionf& qrot, float hCollider, float hPivot,
	                         const Vec3& newdim, int bCapsule, Vec3& dirUnproj, int iCaller = 0);

	void AllocateExtendedHistory();

	void ComputeBBoxLE(const Vec3& pos, Vec3* BBox, coord_block_BBox* partCoord);
	void UpdatePosition(const Vec3& pos, const Vec3* BBox, int bGridLocked);

	Vec3 m_vel, m_velRequested, m_gravity, m_nslope;
	float m_kInertia, m_kInertiaAccel, m_kAirControl, m_kAirResistance, m_hCyl, m_hEye, m_hPivot, m_hHead;
	Vec3 m_size;
	float m_dh, m_dhSpeed, m_dhAcc, m_stablehTime, m_hLatest, m_nodSpeed;
	float m_mass, m_massinv;
	int m_surface_idx;
	int m_lastGroundSurfaceIdx, m_lastGroundSurfaceIdxAux;
	float m_timeFlying, m_minFlyTime, m_timeForceInertia;
	float m_slopeSlide, m_slopeClimb, m_slopeJump, m_slopeFall;
	float m_maxVelGround;
	float m_timeImpulseRecover;
	CCylinderGeom* m_pCylinderGeom;
	CSphereGeom m_SphereGeom, m_HeadGeom;
	phys_geometry m_CylinderGeomPhys;
	float m_timeUseLowCap;
	float m_timeSinceStanceChange;
	float m_timeSinceImpulseContact;
	float m_dhHist[2], m_timeOnStairs;
	float m_timeStepFull, m_timeStepPerformed;
	int m_iSnapshot;
	int m_iTimeLastSend;
	int m_collTypes;

	unsigned int m_bFlying : 1;
	unsigned int m_bJumpRequested : 1;
	unsigned int m_bSwimming : 1;
	unsigned int m_bUseCapsule : 1;
	unsigned int m_bIgnoreCommands : 1;
	unsigned int m_bStateReading : 1;
	unsigned int m_bActive : 1;
	unsigned int m_bActiveEnvironment : 1;
	unsigned int m_bStuck : 1;
	unsigned int m_bHadCollisions : 1;
	unsigned int m_bUseSphere : 1;
	unsigned int m_iLastGroundColliderPart : 16;
	int m_bSquashed;

	CPhysicalEntity* m_pLastGroundCollider;
	int m_idLastGroundColliderPart;
	Vec3 m_posLastGroundColl;
	Vec3 m_velGround;
	Vec3 m_deltaPos, m_posLocal;
	float m_timeSmooth;

	le_history_item *m_history, m_history_buf[4];
	int m_szHistory, m_iHist;
	pe_action_move *m_actions, m_actions_buf[16];
	int m_szActions, m_iAction;
	int m_iCurTime, m_iRequestedTime;

	le_contact* m_pContacts;
	int m_nContacts, m_nContactsAlloc;
	RigidBody* m_pBody;

	int m_nSensors;
	Vec3 *m_pSensors, *m_pSensorsPoints, *m_pSensorsSlopes;
	int m_iSensorsActive;

	volatile int m_lockLiving;
	volatile int m_lockStep;

	bool m_forceFly;
};
