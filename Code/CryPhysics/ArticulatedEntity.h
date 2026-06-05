#pragma once

#include "Matrix.h"
#include "RigidEntity.h"
#include "Vector.h"

struct ae_joint;
struct ae_part_info;

class CArticulatedEntity final : public CRigidEntity
{
public:
	explicit CArticulatedEntity(CPhysicalWorld* pworld);
	~CArticulatedEntity();

	pe_type GetType() override { return PE_ARTICULATED; }

	void AlertNeighbourhoodND();

	int AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id = -1, int bThreadSafe = 1) override;
	void RemoveGeometry(int id, int bThreadSafe = 1) override;
	int SetParams(pe_params* _params, int bThreadSafe = 1) override;
	int GetParams(pe_params* _params) override;
	int GetStatus(pe_status* _status) override;
	int Action(pe_action*, int bThreadSafe = 1) override;

	RigidBody* GetRigidBody(int ipart = -1, int bWillModify = 0);
	void GetContactMatrix(const Vec3& pt, int ipart, Matrix33& K);
	void OnContactResolved(entity_contact* pcontact, int iop, int iGroupId);

	void GetMemoryStatistics(ICrySizer*) override {}

	enum snapver
	{
		SNAPSHOT_VERSION = 6
	};

	int GetStateSnapshot(TSerialize ser, float time_back = 0, int flags = 0) override;
	int SetStateFromSnapshot(TSerialize ser, int flags) override;

	float GetMaxTimeStep(float time_interval);
	int Step(float time_interval) override;
	void StepBack(float time_interval) override;
	int RegisterContacts(float time_interval, int nMaxPlaneContacts);
	int Update(float time_interval, float damping);
	float CalcEnergy(float time_interval);
	float GetDamping(float time_interval);
	void GetSleepSpeedChange(int ipart, Vec3& v, Vec3& w);
	int GetPotentialColliders(CPhysicalEntity**& pentlist, float dt = 0);
	int CheckSelfCollision(int ipart0, int ipart1);
	int IsAwake(int ipart = -1);
	void RecomputeMassDistribution(int ipart = -1, int bMassChanged = 1);
	void BreakableConstraintsUpdated();

	void SyncWithHost(int bRecalcJoints, float time_interval);
	void SyncBodyWithJoint(int idx, int flags = 3);
	void SyncJointWithBody(int idx, int flags = 1);
	void UpdateJointRotationAxes(int idx);
	void CheckForGimbalLock(int idx);
	int GetUnprojAxis(int idx, Vec3& axis);

	int StepJoint(int idx, float time_interval, int& bBounced, int bFlying);
	void JointListUpdated();
	int CalcBodyZa(int idx, float time_interval, vectornf& Za_change);
	int CalcBodyIa(int idx, matrixf& Ia_change);
	void CalcBodiesIinv(int bLockLimits);
	int CollectPendingImpulses(int idx, int& bNotZero);
	void PropagateImpulses(const Vec3& dv, int bLockLimits = 0);
	void CalcVelocityChanges(float time_interval, const Vec3& dv, const Vec3& dw);
	void GetJointTorqueResponseMatrix(int idx, Matrix33& K);
	void UpdatePosition(int bGridLocked);
	void UpdateJointDyn();
	int IsChildOf(int idx, int iParent);

	entity_contact* CreateConstraintContact(int idx);

	ae_part_info* m_infos;
	ae_joint* m_joints;
	int m_nJoints, m_nJointsAlloc;
	Vec3 m_posPivot, m_offsPivot;
	Vec3 m_acc, m_wacc;
	Matrix33 m_M0inv;
	Vec3 m_Ya_vec[2];
	float m_simTime, m_simTimeAux;
	float m_scaleBounceResponse;
	int m_bGrounded;
	int m_nRoots;
	int m_bInheritVel;
	CPhysicalEntity* m_pHost;
	Vec3 m_posHostPivot;
	quaternionf m_qHostPivot;
	int m_bCheckCollisions;
	int m_bCollisionResp;
	int m_bExertImpulse;
	int m_iSimType, m_iSimTypeLyingMode;
	int m_iSimTypeCur;
	int m_iSimTypeOverride;
	int m_bIaReady;
	int m_bPartPosForced;
	int m_bFastLimbs;
	int m_bExpandHinges;
	float m_maxPenetrationCur;
	int m_bUsingUnproj;
	Vec3 m_prev_pos, m_prev_vel;
	int m_bUpdateBodies;
	int m_nDynContacts, m_bInGroup;
	int m_bIgnoreCommands;

	int m_nCollLyingMode;
	Vec3 m_gravityLyingMode;
	float m_dampingLyingMode;
	float m_EminLyingMode;
	int m_nBodyContacts;

	volatile int m_lockJoints;

	CPhysicalEntity** m_pCollEntList;
	int m_nCollEnts;
};
