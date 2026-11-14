#pragma once

#include "CryCommon/CryPhysics/IPhysics.h"

class CBVTree;
struct geometry_under_test;

class CGeometry : public IGeometry
{
public:
	CGeometry() = default;
	virtual ~CGeometry() = default;

	int GetType() override = 0;
	int AddRef() override { return ++m_nRefCount; }
	void Release() override;
	void GetBBox(primitives::box* pbox) override = 0;
	int Intersect(IGeometry* pCollider, geom_world_data* pdata1, geom_world_data* pdata2,
	              intersection_params* pparams, geom_contact*& pcontacts) override;
	int CalcPhysicalProperties(phys_geometry* pgeom) override { return 0; }
	int FindClosestPoint(geom_world_data* pgwd, int& iPrim, int& iFeature, const Vec3& ptdst0, const Vec3& ptdst1,
	                     Vec3* ptres, int nMaxIters = 10) override
	{
		return 0;
	}
	int PointInsideStatus(const Vec3& pt) override { return -1; }
	void DrawWireframe(IPhysRenderer* pRenderer, geom_world_data* gwd, int iLevel, int idxColor) override
	{
		pRenderer->DrawGeometry(this, gwd, idxColor);
	}
	void CalcVolumetricPressure(geom_world_data* gwd, const Vec3& epicenter, float k, float rmin,
	                            const Vec3& centerOfMass, Vec3& P, Vec3& L) override
	{
	}
	float CalculateBuoyancy(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& massCenter) override
	{
		massCenter.zero();
		return 0;
	}
	void CalculateMediumResistance(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& dPres,
	                               Vec3& dLres) override
	{
		dPres.zero();
		dLres.zero();
	}
	int GetPrimitiveId(int iPrim, int iFeature) override { return -1; }
	int GetForeignIdx(int iPrim) override { return -1; }
	Vec3 GetNormal(int iPrim, const Vec3& pt) override { return Vec3(0, 0, 1); }
	int IsConvex(float tolerance) override { return 1; }
	void PrepareForRayTest(float raylen) override {}
	virtual CBVTree* GetBVTree() { return nullptr; }
	int GetPrimitiveCount() override { return 1; }
	virtual int IsAPrimitive() { return 0; }
	virtual int PreparePrimitive(geom_world_data* pgwd, primitives::primitive*& pprim, int iCaller = 0)
	{
		return -1;
	}
	int GetFeature(int iPrim, int iFeature, Vec3* pt) override { return 0; }
	virtual int UnprojectSphere(Vec3 center, float r, float rsep, contact* pcontact) { return 0; }
	int Subtract(IGeometry* pGeom, geom_world_data* pdata1, geom_world_data* pdata2, int bLogUpdates = 1) override
	{
		return 0;
	}
	int GetSubtractionsCount() override { return 0; }
	void SetData(const primitives::primitive*) override {}

	float GetVolume() override { return 0; }
	Vec3 GetCenter() override { return {}; }
	void* GetForeignData(int iForeignData = 0) override
	{
		return iForeignData == m_iForeignData ? m_pForeignData : 0;
	}
	int GetiForeignData() override { return m_iForeignData; }
	void SetForeignData(void* pForeignData, int iForeignData) override
	{
		m_pForeignData = pForeignData;
		m_iForeignData = iForeignData;
	}

	float BuildOcclusionCubemap(geom_world_data* pgwd, int iMode, int* pGrid0[6], int* pGrid1[6], int nRes,
	                            float rmin, float rmax, int nGrow) override;
	virtual int DrawToOcclusionCubemap(const geom_world_data* pgwd, int iStartPrim, int nPrims, int iPass,
	                                   int* pGrid[6], int nRes, float rmin, float rmax, float zscale)
	{
		return 0;
	}

	void GetMemoryStatistics(ICrySizer*) override {}
	void Save(CMemStream& stm) override {}
	void Load(CMemStream& stm) override {}
	void Load(CMemStream& stm, strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices,
	          char* pIds) override
	{
		Load(stm);
	}

	int GetErrorCount() override { return 0; }

	virtual int GetSizeFast() { return 0; }

	virtual int PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
	                                       geometry_under_test* pGTestColl, bool bKeepPrevContacts = false)
	{
		return 1;
	}
	virtual int RegisterIntersection(primitives::primitive* pprim1, primitives::primitive* pprim2,
	                                 geometry_under_test* pGTest1, geometry_under_test* pGTest2,
	                                 prim_inters* pinters);
	virtual void CleanupAfterIntersectionTest(geometry_under_test* pGTest) {}

	virtual int GetPrimitiveList(int iStart, int nPrims, int typeCollider, primitives::primitive* pCollider,
	                             int bColliderLocal, geometry_under_test* pGTest, geometry_under_test* pGTestOp,
	                             primitives::primitive* pRes, char* pResId)
	{
		return 0;
	}
	virtual int GetUnprojectionCandidates(int iop, const contact* pcontact, primitives::primitive*& pprim,
	                                      int*& piFeature, geometry_under_test* pGTest);
	virtual int PreparePolygon(primitives::coord_plane* psurface, int iPrim, int iFeature,
	                           geometry_under_test* pGTest, vector2df*& ptbuf, int*& pVtxIdBuf, int*& pEdgeIdBuf)
	{
		return 0;
	}
	virtual int PreparePolyline(primitives::coord_plane* psurface, int iPrim, int iFeature,
	                            geometry_under_test* pGTest, vector2df*& ptbuf, int*& pVtxIdBuf, int*& pEdgeIdBuf)
	{
		return 0;
	}

	void Lock(int bWrite = 1) override;
	void Unlock(int bWrite = 1) override;

	void DestroyAuxilaryMeshData(int) override {}
	void RemapForeignIdx(int* pCurForeignIdx, int* pNewForeignIdx, int nTris) override {}
	void AppendVertices(Vec3* pVtx, int* pVtxMap, int nVtx) override {}
	float ComputeExtent(GeomQuery& geo, EGeomForm eForm) override;
	void GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm) override;

	volatile int m_lockUpdate{};
	int m_nRefCount = 1;
	float m_minVtxDist{};
	int m_iCollPriority{};
	int m_bIsConvex{};
	void* m_pForeignData{};
	int m_iForeignData{};
};
