#pragma once

#include "Primitive.h"
#include "SingleBoxTree.h"

class CSphereGeom final : public CPrimitive
{
public:
	CSphereGeom()
	{
		m_iCollPriority = 3;
		m_minVtxDist = 0;
	}

	CSphereGeom* CreateSphere(primitives::sphere* pcyl);

	int GetType() override { return GEOM_SPHERE; }
	void GetBBox(primitives::box* pbox) override { m_Tree.GetBBox(pbox); }
	int CalcPhysicalProperties(phys_geometry* pgeom) override;
	int FindClosestPoint(geom_world_data* pgwd, int& iPrim, int& iFeature, const Vec3& ptdst0, const Vec3& ptdst1,
	                     Vec3* ptres, int nMaxIters) override;
	int PointInsideStatus(const Vec3& pt) override;
	float CalculateBuoyancy(const primitives::plane* pplane, const geom_world_data* pgwd,
	                        Vec3& massCenter) override;
	void CalculateMediumResistance(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& dPres,
	                               Vec3& dLres) override;
	void CalcVolumetricPressure(geom_world_data* gwd, const Vec3& epicenter, float k, float rmin,
	                            const Vec3& centerOfMass, Vec3& P, Vec3& L) override;
	int DrawToOcclusionCubemap(const geom_world_data* pgwd, int iStartPrim, int nPrims, int iPass, int* pGrid[6],
	                           int nRes, float rmin, float rmax, float zscale) override;
	CBVTree* GetBVTree() override { return &m_Tree; }
	int UnprojectSphere(Vec3 center, float r, float rsep, contact* pcontact);
	int GetPrimitive(int iPrim, primitives::primitive* pprim) override
	{
		*(primitives::sphere*)pprim = m_sphere;
		return sizeof(primitives::sphere);
	}
	void DrawWireframe(IPhysRenderer* pRenderer, geom_world_data* gwd, int iLevel, int idxColor) override;

	const primitives::primitive* GetData() override { return &m_sphere; }
	void SetData(const primitives::primitive* psph) override { CreateSphere((primitives::sphere*)psph); }
	float GetVolume() override { return (4.0f / 3) * g_PI * cube(m_sphere.r); }
	Vec3 GetCenter() override { return m_sphere.center; }
	float ComputeExtent(GeomQuery& geo, EGeomForm eForm) override;
	void GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm) override;

	int PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
	                               geometry_under_test* pGTestColl, bool bKeepPrevContacts = false);
	int PreparePrimitive(geom_world_data* pgwd, primitives::primitive*& pprim, int iCaller = 0);

	int GetPrimitiveList(int iStart, int nPrims, int typeCollider, primitives::primitive* pCollider,
	                     int bColliderLocal, geometry_under_test* pGTest, geometry_under_test* pGTestOp,
	                     primitives::primitive* pRes, char* pResId);
	int GetUnprojectionCandidates(int iop, const contact* pcontact, primitives::primitive*& pprim, int*& piFeature,
	                              geometry_under_test* pGTest);

	void GetMemoryStatistics(ICrySizer*) override {}
	void Save(CMemStream& stm) override;
	void Load(CMemStream& stm) override;

	primitives::sphere m_sphere;
	CSingleBoxTree m_Tree;
};
