#pragma once

#include "PhysicalPlaceholder.h"

class CPhysicalWorld;

class CPhysArea final : public CPhysicalPlaceholder
{
public:
	explicit CPhysArea(CPhysicalWorld* pWorld);
	~CPhysArea();

	pe_type GetType() override { return PE_AREA; }

	CPhysicalEntity* GetEntity();
	CPhysicalEntity* GetEntityFast() { return GetEntity(); }

	int AddRef() override;
	int Release() override;

	int SetParams(pe_params* params, int bThreadSafe = 1) override;
	int GetParams(pe_params* params) override;
	int GetStatus(pe_status* status) override;
	IPhysicalWorld* GetWorld() override;

	int CheckPoint(const Vec3& pttest);
	int ApplyParams(const Vec3& pt, Vec3& gravity, const Vec3& vel, pe_params_buoyancy* pbdst, int nBuoys,
	                int nMaxBuoys, int& iMedium0, IPhysicalEntity* pent);
	float FindSplineClosestPt(const Vec3& ptloc, int& iClosestSeg, float& tClosest);
	int FindSplineClosestPt(const Vec3& org, const Vec3& dir, Vec3& ptray, Vec3& ptspline);
	void DrawHelperInformation(IPhysRenderer* pRenderer, int flags);
	int RayTrace(const Vec3& org, const Vec3& dir, ray_hit* pHit, pe_params_pos* pp = 0);

	float ComputeExtent(GeomQuery& geo, EGeomForm eForm);
	void GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm);

	void GetMemoryStatistics(ICrySizer*) override {}

	void SignalEventAreaChange(CPhysicalWorld* pWorld);

	int m_bDeleted;
	CPhysArea *m_next, *m_nextBig;
	CPhysicalWorld* m_pWorld;

	Vec3 m_offset;
	Matrix33 m_R;
	float m_scale, m_rscale;

	Vec3 m_offset0;
	Matrix33 m_R0;

	IGeometry* m_pGeom;
	Vec2* m_pt;
	int m_npt;
	float m_zlim[2];
	int* m_idxSort[2];
	Vec3* m_pFlows;
	unsigned int* m_pMask;
	Vec3 m_size0;
	Vec3* m_ptSpline;
	Vec3 m_ptLastCheck;
	int m_iClosestSeg;
	float m_tClosest, m_mindist;
	primitives::indexed_triangle m_trihit;

	pe_params_buoyancy m_pb;
	Vec3 m_gravity;
	Vec3 m_size, m_rsize;
	int m_bUniform;
	float m_falloff0, m_rfalloff0;
	float m_damping;
	int m_bUseCallback;
	volatile int m_lockRef;
};
