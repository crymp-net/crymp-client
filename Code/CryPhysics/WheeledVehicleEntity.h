#pragma once

#include "RigidEntity.h"

struct suspension_point
{
	Vec3 pos;
	quaternionf q;
	float scale;
	Vec3 BBox[2];

	int bDriving; // if the corresponding wheel a driving wheel
	float Tscale;
	int iAxle;
	Vec3 pt;          // the uppermost suspension point in car frame
	float fullen;     // unconstrained length
	float kStiffness; // stiffness coefficient
	float kDamping;   // damping coefficient
	float kDamping0;  // damping coefficient
	float len0;       // initial length in model
	float Mpt;        // hull "mass" at suspension upper point along suspension direction
	quaternionf q0;   // used to calculate geometry transformation from wheel transformation
	Vec3 pos0;
	Vec3 ptc0;
	float Iinv;
	float minFriction;
	float maxFriction;
	float kLatFriction;
	int flags0;
	int flagsCollider0;
	int bCanBrake;
	int bBlocked;
	int iBuddy;
	float r;    // wheel radius
	float rinv; // 1.0/radius
	float width;
	int bRayCast;

	float curlen; // current length
	float steer;  // steering angle
	float rot;    // current wheel rotation angle
	float w;      // current rotation speed
	float wa;     // current angular acceleration
	float T;      // wheel's net torque
	float prevTdt;
	float prevw;
	EventPhysCollision* pCollEvent;

	Vec3 ncontact;
	Vec3 ptcontact;
	int bSlip;
	int bSlipPull;
	int bContact;
	int surface_idx[2];
	Vec3 vrel;
	Vec3 rworld;
	float vworld;
	float PN;
	RigidBody* pbody;
	CPhysicalEntity* pent;
	int ipart;
	float unproj;
	entity_contact contact;
};

class CWheeledVehicleEntity final : public CRigidEntity
{
public:
	explicit CWheeledVehicleEntity(CPhysicalWorld* pWorld);

	pe_type GetType() override { return PE_WHEELEDVEHICLE; }

	int SetParams(pe_params*, int bThreadSafe = 1) override;
	int GetParams(pe_params*) override;
	int Action(pe_action*, int bThreadSafe = 1) override;
	int GetStatus(pe_status*) override;

	enum snapver
	{
		SNAPSHOT_VERSION = 1
	};

	int GetSnapshotVersion() { return SNAPSHOT_VERSION; }
	int GetStateSnapshot(TSerialize ser, float time_back = 0, int flags = 0) override;
	int SetStateFromSnapshot(TSerialize ser, int flags = 0) override;

	int AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id = -1, int bThreadSafe = 1) override;
	void RemoveGeometry(int id, int bThreadSafe = 1) override;

	float GetMaxTimeStep(float time_interval);
	float GetDamping(float time_interval);
	void CheckAdditionalGeometry(float time_interval);
	int RegisterContacts(float time_interval, int nMaxPlaneContacts);
	int RemoveCollider(CPhysicalEntity* pCollider, bool bRemoveAlways = true);
	int HasContactsWith(CPhysicalEntity* pent);
	void AddAdditionalImpulses(float time_interval);
	int Update(float time_interval, float damping);
	void ComputeBBox(Vec3* BBox, int flags);
	void OnContactResolved(entity_contact* pcontact, int iop, int iGroupId);
	void DrawHelperInformation(IPhysRenderer* pRenderer, int flags) override;

	void GetMemoryStatistics(ICrySizer*) override {}

	void RecalcSuspStiffness();
	float ComputeDrivingTorque(float time_interval);

	suspension_point m_susp[NMAXWHEELS];
	float m_enginePower;
	float m_maxSteer;
	float m_engineMaxw;
	float m_engineMinw;
	float m_engineIdlew;
	float m_engineShiftUpw;
	float m_engineShiftDownw;
	float m_gearDirSwitchw;
	float m_engineStartw;
	float m_axleFriction;
	float m_brakeTorque;
	float m_clutchSpeed;
	float m_minBrakingFriction;
	float m_maxBrakingFriction;
	float m_kDynFriction;
	float m_slipThreshold;
	float m_kStabilizer;
	float m_enginePedal;
	float m_steer;
	float m_clutch;
	float m_wengine;
	float m_gears[12];
	int m_nGears;
	int m_iCurGear;
	int m_maxGear;
	int m_minGear;
	float m_kSteerToTrack;
	int m_bHandBrake;
	int m_nHullParts;
	int m_iIntegrationType;
	float m_EminRigid;
	float m_EminVehicle;
	float m_maxAllowedStepVehicle;
	float m_maxAllowedStepRigid;
	float m_dampingVehicle;
	Vec3 m_Ffriction;
	Vec3 m_Tfriction;
	float m_timeNoContacts;
	int m_nContacts;
	int m_bHasContacts;
	volatile int m_lockVehicle;
	float m_pullTilt;
	float m_drivingTorque;
};
