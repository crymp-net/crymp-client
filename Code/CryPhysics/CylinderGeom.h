#pragma once

#include "Primitive.h"
#include "SingleBoxTree.h"

class CCylinderGeom : public CPrimitive
{
public:
	CCylinderGeom()
	{
		m_iCollPriority = 3;
		m_nTesselation = 18;
		m_minVtxDist = 0;
	}

	CCylinderGeom* CreateCylinder(primitives::cylinder* pcyl);

	void PrepareCylinder(primitives::cylinder* pcyl, geometry_under_test* pGTest);
	virtual int PreparePrimitive(geom_world_data* pgwd, primitives::primitive*& pprim, int iCaller = 0);

	int GetType() override { return GEOM_CYLINDER; }
	void GetBBox(primitives::box* pbox) override { m_Tree.GetBBox(pbox); }
	int CalcPhysicalProperties(phys_geometry* pgeom) override;
	int FindClosestPoint(geom_world_data* pgwd, int& iPrim, int& iFeature, const Vec3& ptdst0, const Vec3& ptdst1,
	                     Vec3* ptres, int nMaxIters) override;
	int PointInsideStatus(const Vec3& pt) override;
	void CalcVolumetricPressure(geom_world_data* gwd, const Vec3& epicenter, float k, float rmin,
	                            const Vec3& centerOfMass, Vec3& P, Vec3& L) override;
	float CalculateBuoyancy(const primitives::plane* pplane, const geom_world_data* pgwd,
	                        Vec3& massCenter) override;
	void CalculateMediumResistance(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& dPres,
	                               Vec3& dLres) override;
	int DrawToOcclusionCubemap(const geom_world_data* pgwd, int iStartPrim, int nPrims, int iPass, int* pGrid[6],
	                           int nRes, float rmin, float rmax, float zscale) override;
	CBVTree* GetBVTree() override { return &m_Tree; }
	virtual int UnprojectSphere(Vec3 center, float r, float rsep, contact* pcontact);
	int GetPrimitive(int iPrim, primitives::primitive* pprim) override
	{
		*(primitives::cylinder*)pprim = m_cyl;
		return sizeof(primitives::cylinder);
	}
	void DrawWireframe(IPhysRenderer* pRenderer, geom_world_data* gwd, int iLevel, int idxColor) override;

	const primitives::primitive* GetData() override { return &m_cyl; }
	void SetData(const primitives::primitive* pcyl) override { CreateCylinder((primitives::cylinder*)pcyl); }
	float GetVolume() override { return sqr(m_cyl.r) * m_cyl.hh * (g_PI * 2); }
	Vec3 GetCenter() override { return m_cyl.center; }
	float ComputeExtent(GeomQuery& geo, EGeomForm eForm) override;
	void GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm) override;

	virtual int PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
	                                       geometry_under_test* pGTestColl, bool bKeepPrevContacts = false);

	int GetPrimitiveList(int iStart, int nPrims, int typeCollider, primitives::primitive* pCollider,
	                     int bColliderLocal, geometry_under_test* pGTest, geometry_under_test* pGTestOp,
	                     primitives::primitive* pRes, char* pResId);
	virtual int GetUnprojectionCandidates(int iop, const contact* pcontact, primitives::primitive*& pprim,
	                                      int*& piFeature, geometry_under_test* pGTest);
	int PreparePolygon(primitives::coord_plane* psurface, int iPrim, int iFeature, geometry_under_test* pGTest,
	                   vector2df*& ptbuf, int*& pVtxIdBuf, int*& pEdgeIdBuf);
	int PreparePolyline(primitives::coord_plane* psurface, int iPrim, int iFeature, geometry_under_test* pGTest,
	                    vector2df*& ptbuf, int*& pVtxIdBuf, int*& pEdgeIdBuf);

	void GetMemoryStatistics(ICrySizer*) override {}
	void Save(CMemStream& stm) override;
	void Load(CMemStream& stm) override;
	int GetSizeFast() { return sizeof(*this); }

	primitives::cylinder m_cyl;
	int m_nTesselation;
	CSingleBoxTree m_Tree;
};
