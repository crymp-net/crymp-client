#pragma once

#include "PhysicalEntity.h"

struct se_edge;
struct se_vertex;

class CSoftEntity final : public CPhysicalEntity
{
public:
	explicit CSoftEntity(CPhysicalWorld* pWorld);
	~CSoftEntity();

	pe_type GetType() override { return PE_SOFT; }

	int AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id = -1, int bThreadSafe = 1) override;
	void RemoveGeometry(int id, int bThreadSafe = 1) override;
	int SetParams(pe_params* _params, int bThreadSafe = 1) override;
	int GetParams(pe_params* _params) override;
	int Action(pe_action*, int bThreadSafe = 1) override;
	int GetStatus(pe_status*) override;

	int Awake(int bAwake = 1, int iSource = 0)
	{
		m_bAwake = bAwake;
		if (bAwake)
		{
			m_nSlowFrames = 0;
		}
		return 1;
	}

	int IsAwake(int ipart = -1) { return m_bAwake; }
	void AlertNeighbourhoodND();

	void StartStep(float time_interval) override;
	float GetMaxTimeStep(float time_interval);
	int Step(float time_interval) override;
	int RayTrace(CRayGeom* pRay, geom_contact*& pcontacts, volatile int*& plock);
	void ApplyVolumetricPressure(const Vec3& epicenter, float kr, float rmin);
	float GetMass(int ipart) { return m_parts[0].mass / m_nVtx; }

	enum snapver
	{
		SNAPSHOT_VERSION = 10
	};

	void DrawHelperInformation(IPhysRenderer* pRenderer, int flags) override;
	void GetMemoryStatistics(ICrySizer*) override {}

	se_vertex* m_vtx;
	se_edge* m_edges;
	int* m_pVtxEdges;
	int m_nVtx, m_nEdges;
	int m_nConnectedVtx;
	Vec3 m_offs0;
	quaternionf m_qrot0;
	int m_bMeshUpdated;

	float m_timeStepFull;
	float m_timeStepPerformed;

	Vec3 m_gravity;
	float m_Emin;
	float m_maxAllowedStep;
	int m_bAwake, m_nSlowFrames;
	float m_damping;
	float m_accuracy;
	int m_nMaxIters;
	float m_prevTimeInterval;

	float m_thickness;
	float m_ks, m_kdRatio;
	float m_maxSafeStep;
	float m_density;
	float m_coverage;
	float m_friction;
	float m_impulseScale;
	float m_explosionScale;
	float m_collImpulseScale;
	float m_maxCollImpulse;
	int m_collTypes;
	float m_massDecay;

	float m_waterResistance;
	float m_airResistance;
	Vec3 m_wind;
	Vec3 m_wind0, m_wind1;
	float m_windTimer;
	float m_windVariance;

	volatile int m_lockSoftBody;
};
