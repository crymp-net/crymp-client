#pragma once

#include "HeightfieldBV.h"
#include "TriMesh.h"

class CHeightfield final : public CTriMesh
{
public:
	CHeightfield() = default;

	CHeightfield* CreateHeightfield(primitives::heightfield* phf);
	int GetType() override { return GEOM_HEIGHTFIELD; }
	int Intersect(IGeometry* pCollider, geom_world_data* pdata1, geom_world_data* pdata2,
	              intersection_params* pparams, geom_contact*& pcontacts) override;
	int PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
	                               geometry_under_test* pGTestColl, bool bKeepPrevContacts = false) override;
	int PointInsideStatus(const Vec3& pt) override { return -1; }
	int FindClosestPoint(geom_world_data* pgwd, int& iPrim, int& iFeature, const Vec3& ptdst0, const Vec3& ptdst1,
	                     Vec3* ptres, int nMaxIters = 10) override;
	void CalcVolumetricPressure(geom_world_data* gwd, const Vec3& epicenter, float k, float rmin,
	                            const Vec3& centerOfMass, Vec3& P, Vec3& L) override;
	int IsConvex(float tolerance) override { return 0; }
	int DrawToOcclusionCubemap(const geom_world_data* pgwd, int iStartPrim, int nPrims, int iPass, int* pGrid[6],
	                           int nRes, float rmin, float rmax, float zscale) override;
	void PrepareForRayTest(float raylen) override {}
	CBVTree* GetBVTree() override { return &m_Tree; }

	const primitives::primitive* GetData() override { return &m_hf; }
	Vec3 GetCenter() override { return m_hf.origin + (Vec3(m_hf.size.x, m_hf.size.y, 0) * 0.5f) * m_hf.Basis; }

	int GetPrimitive(int iPrim, primitives::primitive* pprim) override
	{
		*(primitives::heightfield*)pprim = m_hf;
		return sizeof(primitives::heightfield);
	}
	void GetMemoryStatistics(ICrySizer*) override {}

	primitives::heightfield m_hf;
	CHeightfieldBV m_Tree;
	float m_minHeight, m_maxHeight;
	int m_nVerticesAlloc, m_nTrisAlloc;
};
