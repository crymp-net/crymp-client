#pragma once

#include "Geometry.h"
#include "RayBV.h"

class CRayGeom final : public CGeometry
{
public:
	CRayGeom()
	{
		m_iCollPriority = 0;
		m_Tree.Build(this);
		m_Tree.SetRay(&m_ray);
		m_minVtxDist = 1.0f;
	}

	explicit CRayGeom(const primitives::ray* pray)
	{
		m_iCollPriority = 0;
		m_ray.origin = pray->origin;
		m_ray.dir = pray->dir;
		m_dirn = pray->dir.normalized();
		m_Tree.Build(this);
		m_Tree.SetRay(&m_ray);
		m_minVtxDist = 1.0f;
	}

	CRayGeom(const Vec3& origin, const Vec3& dir)
	{
		m_iCollPriority = 0;
		m_ray.origin = origin;
		m_ray.dir = dir;
		m_dirn = dir.normalized();
		m_Tree.Build(this);
		m_Tree.SetRay(&m_ray);
		m_minVtxDist = 1.0f;
	}

	CRayGeom* CreateRay(const Vec3& origin, const Vec3& dir, const Vec3* pdirn = 0)
	{
		m_ray.origin = origin;
		m_ray.dir = dir;
		m_dirn = pdirn ? *pdirn : dir.normalized();
		return this;
	}

	void PrepareRay(primitives::ray* pray, geometry_under_test* pGTest);

	int GetType() override { return GEOM_RAY; }
	int IsAPrimitive() { return 1; }
	void GetBBox(primitives::box* pbox) override;
	int PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
	                               geometry_under_test* pGTestColl, bool bKeepPrevContacts);
	int RegisterIntersection(primitives::primitive* pprim1, primitives::primitive* pprim2,
	                         geometry_under_test* pGTest1, geometry_under_test* pGTest2, prim_inters* pinters);
	int GetPrimitiveList(int iStart, int nPrims, int typeCollider, primitives::primitive* pCollider,
	                     int bColliderLocal, geometry_under_test* pGTest, geometry_under_test* pGTestOp,
	                     primitives::primitive* pRes, char* pResId);
	int GetUnprojectionCandidates(int iop, const contact* pcontact, primitives::primitive*& pprim, int*& piFeature,
	                              geometry_under_test* pGTest);
	int PreparePrimitive(geom_world_data* pgwd, primitives::primitive*& pprim, int iCaller = 0);

	CBVTree* GetBVTree() override { return &m_Tree; }

	int GetPrimitive(int iPrim, primitives::primitive* pprim) override
	{
		*(primitives::ray*)pprim = m_ray;
		return sizeof(primitives::ray);
	}

	void PrepareForRayTest(float raylen) override { m_dirn = m_ray.dir.normalized(); }

	const primitives::primitive* GetData() override { return &m_ray; }
	Vec3 GetCenter() override { return m_ray.origin + m_ray.dir * 0.5f; }

	int GetSizeFast() { return sizeof(*this); }

	primitives::ray m_ray;
	Vec3 m_dirn;
	CRayBV m_Tree;
};
