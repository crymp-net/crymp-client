#pragma once

#include "BVTree.h"

class CTriMesh;

class CVoxelBV final : public CBVTree
{
public:
	CVoxelBV() = default;

	int GetType() override { return BVT_VOXEL; }

	float Build(CGeometry* pGeom);

	void GetBBox(primitives::box* pbox) override;

	int MaxPrimsInNode() override { return m_nTris; }

	int PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
	                               geometry_under_test* pGTestColl) override;
	void CleanupAfterIntersectionTest(geometry_under_test* pGTest) override;
	void GetNodeBV(BV*& pBV, int iNode = 0, int iCaller = 0) override;
	void GetNodeBV(BV*& pBV, const Vec3& sweepdir, float sweepstep, int iNode = 0, int iCaller = 0) override
	{
		GetNodeBV(pBV);
	}

	void GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, int iNode = 0,
	               int iCaller = 0) override;
	void GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, const Vec3& sweepdir,
	               float sweepstep, int iNode = 0, int iCaller = 0) override
	{
		GetNodeBV(Rw, offsw, scalew, pBV, 0, iCaller);
	}

	int GetNodeContents(int iNode, BV* pBVCollider, int bColliderUsed, int bColliderLocal,
	                    geometry_under_test* pGTest, geometry_under_test* pGTestOp) override;
	void MarkUsedTriangle(int itri, geometry_under_test* pGTest) override;

	void ResetCollisionArea() override
	{
		m_iBBox[0].zero();
		m_iBBox[1] = m_pgrid->size;
	}

	CTriMesh* m_pMesh{};
	primitives::voxelgrid* m_pgrid{};
	Vec3i m_iBBox[2]{};
	int m_nTris{};
};
