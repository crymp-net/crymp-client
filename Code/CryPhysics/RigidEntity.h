#pragma once

#include <stdint.h>

#include "ContactSolver.h"
#include "PhysicalEntity.h"
#include "RigidBody.h"

struct entity_contact;
struct coord_block_BBox;

using masktype = uint64_t;
constexpr int NMASKBITS = 64;

constexpr masktype getmask(int i)
{
	return masktype(1) << i;
}

enum rentity_flags_int
{
	ref_small_and_fast = 0x10,
};

enum constr_info_flags
{
	constraint_limited_1axis = 1,
	constraint_limited_2axes = 2,
	constraint_rope = 4,
	constraint_broken = 0x10000,
};

struct checksum_item
{
	int iPhysTime;
	unsigned int checksum;
};

struct constraint_info
{
	int id;
	quaternionf qframe_rel[2];
	float limits[2];
	unsigned int flags;
	float damping;
	float sensorRadius;
	CPhysicalEntity* pConstraintEnt;
	int bActive;
	quaternionf qprev[2];
	float limit;
};

struct SRigidEntityNetSerialize
{
	Vec3 pos;
	Quat rot;
	Vec3 vel;
	Vec3 angvel;
	bool simclass;

	void Serialize(TSerialize ser);
};

class CRigidEntity : public CPhysicalEntity
{
public:
	explicit CRigidEntity(CPhysicalWorld* pWorld);
	~CRigidEntity();

	pe_type GetType() override { return PE_RIGID; }

	int AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id = -1, int bThreadSafe = 1) override;
	void RemoveGeometry(int id, int bThreadSafe = 1) override;
	int SetParams(pe_params* _params, int bThreadSafe = 1) override;
	int GetParams(pe_params* _params) override;
	int GetStatus(pe_status*) override;
	int Action(pe_action*, int bThreadSafe = 1) override;

	int AddCollider(CPhysicalEntity* pCollider);
	virtual int RemoveCollider(CPhysicalEntity* pCollider, bool bRemoveAlways = true);
	int RemoveContactPoint(CPhysicalEntity* pCollider, const Vec3& pt, float mindist2);
	virtual int HasContactsWith(CPhysicalEntity* pent);
	int HasCollisionContactsWith(CPhysicalEntity* pent);
	int HasConstraintContactsWith(CPhysicalEntity* pent, int flagsIgnore = 0);
	int Awake(int bAwake = 1, int iSource = 0);
	virtual int IsAwake(int ipart = -1) { return m_bAwake; }
	virtual void AlertNeighbourhoodND();
	virtual void OnContactResolved(entity_contact* pContact, int iop, int iGroupId);

	virtual RigidBody* GetRigidBody(int ipart = -1, int bWillModify = 0) { return &m_body; }
	virtual void GetContactMatrix(const Vec3& pt, int ipart, Matrix33& K)
	{
		m_body.GetContactMatrix(pt - m_body.pos, K);
	}
	float GetMassInv() { return m_flags & aef_recorded_physics ? 0 : m_body.Minv; }

	enum snapver
	{
		SNAPSHOT_VERSION = 9
	};

	virtual int GetSnapshotVersion() { return SNAPSHOT_VERSION; }
	int GetStateSnapshot(TSerialize ser, float time_back = 0, int flags = 0) override;
	int SetStateFromSnapshot(TSerialize ser, int flags) override;
	int PostSetStateFromSnapshot() override;
	unsigned int GetStateChecksum() override;

	void StartStep(float time_interval) override;
	virtual float GetMaxTimeStep(float time_interval);
	float GetLastTimeStep(float time_interval) { return m_lastTimeStep; }
	int Step(float time_interval) override;
	void StepBack(float time_interval) override;
	int GetContactCount(int nMaxPlaneContacts);
	virtual int RegisterContacts(float time_interval, int nMaxPlaneContacts);
	virtual int Update(float time_interval, float damping);
	virtual float CalcEnergy(float time_interval);
	virtual float GetDamping(float time_interval);
	virtual void GetSleepSpeedChange(int ipart, Vec3& v, Vec3& w)
	{
		v = m_vSleep;
		w = m_wSleep;
	}

	virtual void CheckAdditionalGeometry(float time_interval) {}
	virtual void AddAdditionalImpulses(float time_interval) {}
	virtual void RecomputeMassDistribution(int ipart = -1, int bMassChanged = 1);

	void DrawHelperInformation(IPhysRenderer* pRenderer, int flags) override;
	void GetMemoryStatistics(ICrySizer*) override {}

	int RegisterConstraint(const Vec3& pt0, const Vec3& pt1, int ipart0, CPhysicalEntity* pBuddy, int ipart1,
	                       int flags, int flagsInfo = 0);
	int RemoveConstraint(int iConstraint);
	virtual void BreakableConstraintsUpdated();
	entity_contact* RegisterContactPoint(int idx, const Vec3& pt, const geom_contact* pcontacts, int iPrim0,
	                                     int iFeature0, int iPrim1, int iFeature1, int flags = contact_new,
	                                     float penetration = 0);
	int CheckForNewContacts(geom_world_data* pgwd0, intersection_params* pip, int& itmax, Vec3 sweep = {},
	                        int iStartPart = 0, int nParts = -1);
	virtual int GetPotentialColliders(CPhysicalEntity**& pentlist, float dt = 0);
	virtual int CheckSelfCollision(int ipart0, int ipart1) { return 0; }
	void UpdatePenaltyContacts(float time_interval);
	int UpdatePenaltyContact(entity_contact* pContact, float time_interval);
	void VerifyExistingContacts(float maxdist);
	int EnforceConstraints(float time_interval);
	void UpdateConstraints(float time_interval);
	void UpdateContactsAfterStepBack(float time_interval);
	void ApplyBuoyancy(float time_interval, const Vec3& gravity, pe_params_buoyancy* pb, int nBuoys);
	void ArchiveContact(entity_contact* pContact, float imp = 0, int bLastInGroup = 1, float r = 0.0f);
	int CompactContactBlock(entity_contact* pContact, int endFlags, float maxPlaneDist, int nMaxContacts,
	                        int& nContacts, entity_contact*& pResContact, Vec3& n, float& maxDist,
	                        const Vec3& ptTest, const Vec3& dirTest);
	void ComputeBBoxRE(coord_block_BBox* partCoord);
	void UpdatePosition(int bGridLocked);
	int PostStepNotify(float time_interval, pe_params_buoyancy* pb, int nMaxBuoys);
	masktype MaskIgnoredColliders();
	void UnmaskIgnoredColliders(masktype constraint_mask);
	void FakeRayCollision(CPhysicalEntity* pent, float dt);
	int ExtractConstraintInfo(int i, masktype constraintMask, pe_action_add_constraint& aac);
	EventPhysJointBroken& ReportConstraintBreak(EventPhysJointBroken& epjb, int i);
	bool IgnoreCollisionsWith(CPhysicalEntity* pent, int bCheckConstraints = 0);
	void OnNeighbourSplit(CPhysicalEntity* pentOrig, CPhysicalEntity* pentNew);

	void AttachContact(entity_contact* pContact, int i);
	void DetachContact(entity_contact* pContact, int i = -1, int bCheckIfEmpty = 1);
	void DetachAllContacts();
	void MoveConstrainedObjects(const Vec3& dpos, const quaternionf& dq);
	void DetachPartContacts(int ipart, int iop0, CPhysicalEntity* pent, int iop1, int bCheckIfEmpty = 1);
	void CapBodyVel();
	void CleanupAfterContactsCheck();

	Vec3 m_posNew;
	quaternionf m_qNew;
	Vec3 m_BBoxNew[2];
	int m_iVarPart0;

	entity_contact** m_pColliderContacts;
	masktype* m_pColliderConstraints;
	entity_contact *m_pContactStart, *m_pContactEnd;
	int m_nContacts;
	entity_contact* m_pConstraints;
	constraint_info* m_pConstraintInfos;
	int m_nConstraintsAlloc;
	int m_bProhibitUnproj;
	int m_bProhibitUnprojection, m_bEnforceContacts;
	Vec3 m_prevUnprojDir;
	int m_bCollisionCulling;
	int m_bJustLoaded;
	int m_bStable;
	int m_bHadSeverePenetration;
	unsigned int m_nRestMask;
	int m_nPrevColliders;
	int m_bSteppedBack, m_nStepBackCount;
	float m_velFastDir, m_sizeFastDir;
	int m_bCanSweep;

	float m_timeStepFull;
	float m_timeStepPerformed;
	float m_lastTimeStep;
	float m_minAwakeTime;

	Vec3 m_gravity, m_gravityFreefall;
	float m_Emin;
	float m_maxAllowedStep;
	Vec3 m_vhist[4], m_whist[4], m_Lhist[4];
	int m_iDynHist;
	int m_bAwake;
	int m_nSleepFrames;
	float m_damping, m_dampingFreefall;
	float m_dampingEx;
	float m_maxw;
	float m_softness[4];
	int m_nNonRigidNeighbours;
	float m_minFriction;
	int m_nFutileUnprojFrames;
	Vec3 m_vSleep, m_wSleep;
	entity_contact* m_pStableContact;

	RigidBody m_body;
	Vec3 m_Pext[2], m_Lext[2];
	Vec3 m_prevPos, m_prevv, m_prevw;
	quaternionf m_prevq;
	float m_E0, m_Estep;
	float m_impulseTime;
	float m_timeCanopyFallen;
	int m_bCanopyContact : 8;
	int m_nCanopyContactsLost : 24;
	Vec3 m_Psoft, m_Lsoft;

	EventPhysCollision** m_pEventsColl;
	int m_nEvents, m_nMaxEvents;
	int m_iLastLogColl;
	int m_icollMin;
	float m_vcollMin;
	int m_iLastLog;
	EventPhysPostStep* m_pEvent;

	float m_waterDamping;
	float m_kwaterDensity, m_kwaterResistance;
	float m_EminWater;
	int m_bFloating;
	float m_submergedFraction;

	int m_iLastConstraintIdx;
	volatile int m_lockConstraintIdx;
	volatile int m_lockDynState;
	volatile int m_lockContacts;
	Vec3 m_Fcollision, m_Tcollision;

	checksum_item m_checksums[1];
	int m_iLastChecksum;
};

extern CPhysicalEntity* g_CurColliders[];
