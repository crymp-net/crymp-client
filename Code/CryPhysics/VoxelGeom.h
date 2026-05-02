#pragma once

#include "TriMesh.h"
#include "VoxelBV.h"

class CVoxelGeom final : public CTriMesh
{
public:
	CVoxelGeom()
	{
		m_grid.pCellTris = nullptr;
		m_grid.pTriBuf = nullptr;
	}

	~CVoxelGeom()
	{
		if (m_grid.pCellTris)
		{
			delete[] m_grid.pCellTris;
		}
		if (m_grid.pTriBuf)
		{
			delete[] m_grid.pTriBuf;
		}
		m_pTree = nullptr;
	}

	CVoxelGeom* CreateVoxelGrid(primitives::grid3d* pgrid);
	int GetType() override { return GEOM_VOXELGRID; }
	int Intersect(IGeometry* pCollider, geom_world_data* pdata1, geom_world_data* pdata2,
	              intersection_params* pparams, geom_contact*& pcontacts) override;
	int PointInsideStatus(const Vec3& pt) override { return -1; }
	void CalcVolumetricPressure(geom_world_data* gwd, const Vec3& epicenter, float k, float rmin,
	                            const Vec3& centerOfMass, Vec3& P, Vec3& L) override
	{
	}
	int IsConvex(float tolerance) override { return 0; }
	int DrawToOcclusionCubemap(const geom_world_data* pgwd, int iStartPrim, int nPrims, int iPass, int* pGrid[6],
	                           int nRes, float rmin, float rmax, float zscale) override;
	void PrepareForRayTest(float raylen) override {}
	CBVTree* GetBVTree() override { return &m_Tree; }
	void GetMemoryStatistics(ICrySizer*) override {}

	primitives::voxelgrid m_grid;
	CVoxelBV m_Tree;
};
