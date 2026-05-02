#pragma once

#include "Geometry.h"

class CBVTree;

enum tessvtx_flags
{
	contour_end = 4,
	vtx_processed = 8,
	vtx_instablept_log2 = 4,
	vtx_instable = 4 << vtx_instablept_log2
};

struct border_trace
{
	Vec3* pt;
	int (*itri)[2];
	float* seglen;
	int npt, szbuf;

	Vec3 pt_end;
	int itri_end;
	int iedge_end;
	float end_dist2;
	int bExactBorder;

	int iMark, nLoop;

	Vec3r n_sum[2];
	Vec3 n_best;
	int ntris[2];
};

struct tri_flags
{
	unsigned int inext : 16;
	unsigned int iprev : 15;
	unsigned int bFree : 1;
};

class CTriMesh : public CGeometry
{
public:
	CTriMesh();
	~CTriMesh();

	CTriMesh* CreateTriMesh(strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices,
	                        char* pMats, int* pForeignIdx, int nTris, int flags, int nMinTrisPerNode = 2,
	                        int nMaxTrisPerNode = 4, float favorAABB = 1.0f);
	CTriMesh* Clone(CTriMesh* src, int flags);

	int GetType() override { return GEOM_TRIMESH; }
	int Intersect(IGeometry* pCollider, geom_world_data* pdata1, geom_world_data* pdata2,
	              intersection_params* pparams, geom_contact*& pcontacts) override;
	int Subtract(IGeometry* pGeom, geom_world_data* pdata1, geom_world_data* pdata2, int bLogUpdates = 1) override;
	void GetBBox(primitives::box* pbox) override;
	void GetBBox(primitives::box* pbox, int bThreadSafe);
	int FindClosestPoint(geom_world_data* pgwd, int& iPrim, int& iFeature, const Vec3& ptdst0, const Vec3& ptdst1,
	                     Vec3* ptres, int nMaxIters = 10) override;
	int CalcPhysicalProperties(phys_geometry* pgeom) override;
	int PointInsideStatus(const Vec3& pt) override { return PointInsideStatusMesh(pt, 0); }
	int PointInsideStatusMesh(const Vec3& pt, primitives::indexed_triangle* pHitTri);
	void DrawWireframe(IPhysRenderer* pRenderer, geom_world_data* gwd, int iLevel, int idxColor) override;
	void CalcVolumetricPressure(geom_world_data* gwd, const Vec3& epicenter, float k, float rmin,
	                            const Vec3& centerOfMass, Vec3& P, Vec3& L) override;
	float CalculateBuoyancy(const primitives::plane* pplane, const geom_world_data* pgwd,
	                        Vec3& massCenter) override;
	void CalculateMediumResistance(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& dPres,
	                               Vec3& dLres) override;
	int GetPrimitiveId(int iPrim, int iFeature) override { return m_pIds ? m_pIds[iPrim] : -1; }
	int GetForeignIdx(int iPrim) override { return m_pForeignIdx ? m_pForeignIdx[iPrim] : -1; }
	Vec3 GetNormal(int iPrim, const Vec3& pt) override { return m_pNormals[iPrim]; }
	int IsConvex(float tolerance) override;
	void PrepareForRayTest(float raylen) override;
	virtual int DrawToOcclusionCubemap(const geom_world_data* pgwd, int iStartPrim, int nPrims, int iPass,
	                                   int* pGrid[6], int nRes, float rmin, float rmax, float zscale);
	virtual CBVTree* GetBVTree() { return m_pTree; }
	int GetPrimitiveCount() override { return m_nTris; }
	int GetPrimitive(int iPrim, primitives::primitive* pprim) override;
	int GetFeature(int iPrim, int iFeature, Vec3* pt) override;
	int PreparePrimitive(geom_world_data* pgwd, primitives::primitive*& pprim, int iCaller);
	int GetSubtractionsCount() override { return m_nSubtracts; }

	float ComputeExtent(GeomQuery& geo, EGeomForm eForm) override;
	void GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm) override;

	void GetMemoryStatistics(ICrySizer*) override {}
	void Save(CMemStream& stm) override;
	void Load(CMemStream& stm) override { Load(stm, 0, 0, 0); }
	void Load(CMemStream& stm, strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices,
	          char* pIds) override;
	int GetErrorCount() override { return m_nErrors; }

	virtual int PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
	                                       geometry_under_test* pGTestColl, bool bKeepPrevContacts = false);
	int RegisterIntersection(primitives::primitive* pprim1, primitives::primitive* pprim2,
	                         geometry_under_test* pGTest1, geometry_under_test* pGTest2, prim_inters* pinters);
	void CleanupAfterIntersectionTest(geometry_under_test* pGTest);

	int GetPrimitiveList(int iStart, int nPrims, int typeCollider, primitives::primitive* pCollider,
	                     int bColliderLocal, geometry_under_test* pGTest, geometry_under_test* pGTestOp,
	                     primitives::primitive* pRes, char* pResId);
	int GetUnprojectionCandidates(int iop, const contact* pcontact, primitives::primitive*& pprim, int*& piFeature,
	                              geometry_under_test* pGTest);
	int PreparePolygon(primitives::coord_plane* psurface, int iPrim, int iFeature, geometry_under_test* pGTest,
	                   vector2df*& ptbuf, int*& pVtxIdBuf, int*& pEdgeIdBuf);
	int PreparePolyline(primitives::coord_plane* psurface, int iPrim, int iFeature, geometry_under_test* pGTest,
	                    vector2df*& ptbuf, int*& pVtxIdBuf, int*& pEdgeIdBuf);

	int GetEdgeByBuddy(int itri, int itri_buddy);
	int GetNeighbouringEdgeId(int vtxid, int ivtx);
	void PrepareTriangle(int itri, primitives::triangle* ptri, const geometry_under_test* pGTest);
	int TraceTriangleInters(int iop, primitives::primitive* pprims[], int idx_buddy, int type_buddy,
	                        prim_inters* pinters, geometry_under_test* pGTest, border_trace* pborder);
	void HashTrianglesToPlane(const primitives::coord_plane& hashplane, const vector2df& hashsize,
	                          primitives::grid& hashgrid, index_t*& pHashGrid, index_t*& pHashData,
	                          float cellsize = 0);
	int CalculateTopology(index_t* pIndices, int bCheckOnly = 0);
	int BuildIslandMap();
	void RebuildBVTree(CBVTree* pRefTree = 0);
	void Empty();
	float GetIslandDisk(int matid, const Vec3& ptref, Vec3& center, Vec3& normal, float& peakDist);

	CTriMesh* SplitIntoIslands(primitives::plane* pGround, int nPlanes, int bOriginallyMobile);
	int FilterMesh(float minlen, float minangle);

	void CompactTriangleList(int* pTriMap);
	void CollapseTriangleToLine(int itri, int ivtx, int* pTriMap, bop_meshupdate* pmu);
	void RecalcTriNormal(int i);

	const primitives::primitive* GetData() override { return (mesh_data*)&m_pIndices; }
	float GetVolume() override;
	Vec3 GetCenter() override;
	void* GetForeignData(int iForeignData = 0) override;
	void SetForeignData(void* pForeignData, int iForeignData) override;
	void RemapForeignIdx(int* pCurForeignIdx, int* pNewForeignIdx, int nTris) override;
	void AppendVertices(Vec3* pVtx, int* pVtxMap, int nVtx) override;
	void DestroyAuxilaryMeshData(int idata) override;

	CBVTree* m_pTree;
	index_t* m_pIndices;
	char* m_pIds;
	int* m_pForeignIdx;
	strided_pointer<Vec3> m_pVertices;
	Vec3* m_pNormals;
	int* m_pVtxMap;
	trinfo* m_pTopology;
	int m_nTris, m_nVertices;
	mesh_island* m_pIslands;
	int m_nIslands;
	tri_flags* m_pTri2Island;
	int* m_pIdxNew2Old;
	int m_nMaxVertexValency;
	int m_flags;
	index_t *m_pHashGrid[3], *m_pHashData[3];
	primitives::grid m_hashgrid[3];
	volatile int m_nHashPlanes;
	int m_bConvex[4];
	float m_ConvexityTolerance[4];
	int m_bMultipart;
	float m_V, m_A, m_L;
	Vec3 m_center;
	int m_nErrors;
	bop_meshupdate* m_pMeshUpdate;
	int m_iLastNewTriIdx;
	bop_meshupdate_thunk m_refmu;
	int m_nMessyCutCount;
	int m_nSubtracts;
};
