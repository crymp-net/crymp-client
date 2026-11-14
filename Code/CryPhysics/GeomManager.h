#pragma once

#include "CryCommon/CryPhysics/IPhysics.h"

class CTriMesh;
struct SCrackGeom;

class CGeomManager : public IGeomManager
{
public:
	CGeomManager() { InitGeoman(); }
	~CGeomManager() { ShutDownGeoman(); }

	void InitGeoman();
	void ShutDownGeoman();

	IGeometry* CreateMesh(strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices,
	                      char* pMats, int* pForeignIdx, int nTris, int flags, float approx_tolerance = 0.05f,
	                      int nMinTrisPerNode = 2, int nMaxTrisPerNode = 4, float favorAABB = 1.0f) override
	{
		SBVTreeParams params;
		params.nMinTrisPerNode = nMinTrisPerNode;
		params.nMaxTrisPerNode = nMaxTrisPerNode;
		params.favorAABB = favorAABB;
		return CreateMesh(pVertices, pIndices, pMats, pForeignIdx, nTris, flags & ~mesh_VoxelGrid,
		                  approx_tolerance, &params);
	}

	IGeometry* CreateMesh(strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices,
	                      char* pMats, int* pForeignIdx, int nTris, int flags, float approx_tolerance,
	                      SMeshBVParams* pParams) override;
	IGeometry* CreatePrimitive(int type, const primitives::primitive* pprim) override;
	void DestroyGeometry(IGeometry* pGeom) override;

	phys_geometry* RegisterGeometry(IGeometry* pGeom, int defSurfaceIdx = 0, int* pMatMapping = 0,
	                                int nMats = 0) override;
	int AddRefGeometry(phys_geometry* pgeom) override;
	int UnregisterGeometry(phys_geometry* pgeom) override;
	void SetGeomMatMapping(phys_geometry* pgeom, int* pMatMapping, int nMats) override;

	void SaveGeometry(CMemStream& stm, IGeometry* pGeom) override;
	IGeometry* LoadGeometry(CMemStream& stm, strided_pointer<const Vec3> pVertices,
	                        strided_pointer<unsigned short> pIndices, char* pIds) override;
	void SavePhysGeometry(CMemStream& stm, phys_geometry* pgeom) override;
	phys_geometry* LoadPhysGeometry(CMemStream& stm, strided_pointer<const Vec3> pVertices,
	                                strided_pointer<unsigned short> pIndices, char* pIds) override;
	IGeometry* CloneGeometry(IGeometry* pGeom) override;

	ITetrLattice* CreateTetrLattice(const Vec3* pt, int npt, const int* pTets, int nTets) override;
	int RegisterCrack(IGeometry* pGeom, Vec3* pVtx, int idmat) override;
	void UnregisterCrack(int id) override;
	IGeometry* GetCrackGeom(const Vec3* pt, int idmat, geom_world_data* pgwd) override;

	IBreakableGrid2d* GenerateBreakableGrid(vector2df* ptsrc, int npt, const vector2di& nCells, int bStaticBorder,
	                                        int seed = -1) override;

	phys_geometry* GetFreeGeomSlot();
	virtual IPhysicalWorld* GetIWorld() { return nullptr; }

	phys_geometry** m_pGeoms;
	int m_nGeomChunks, m_nGeomsInLastChunk;
	phys_geometry* m_pFreeGeom;
	int m_lockGeoman;

	SCrackGeom* m_pCracks;
	int m_nCracks;
	int m_idCrack;
	float m_kCrackScale, m_kCrackSkew;
	int m_sizeExtGeoms;
};
