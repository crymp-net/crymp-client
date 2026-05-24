#pragma once

#include "BVTree.h"

class CSingleBoxTree final : public CBVTree
{
public:
	CSingleBoxTree() { m_nPrims = 1; }
	int GetType() override { return BVT_SINGLEBOX; }
	float Build(CGeometry* pGeom) override;
	void SetBox(primitives::box* pbox);
	int PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
	                               geometry_under_test* pGTestColl) override;
	void GetBBox(primitives::box* pbox) override;
	int MaxPrimsInNode() override { return m_nPrims; }
	void GetNodeBV(BV*& pBV, int iNode = 0, int iCaller = 0) override;
	void GetNodeBV(BV*& pBV, const Vec3& sweepdir, float sweepstep, int iNode = 0, int iCaller = 0) override;
	void GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, int iNode = 0,
	               int iCaller = 0) override;
	void GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, const Vec3& sweepdir,
	               float sweepstep, int iNode = 0, int iCaller = 0) override;
	int GetNodeContents(int iNode, BV* pBVCollider, int bColliderUsed, int bColliderLocal,
	                    geometry_under_test* pGTest, geometry_under_test* pGTestOp) override;

	int GetNodeContentsIdx(int iNode, int& iStartPrim) override
	{
		iStartPrim = 0;
		return m_nPrims;
	}

	void MarkUsedTriangle(int itri, geometry_under_test* pGTest) override;

	void GetMemoryStatistics(ICrySizer*) override {}
	void Save(CMemStream& stm) override;
	void Load(CMemStream& stm, CGeometry* pGeom) override;

	CGeometry* m_pGeom;
	primitives::box m_Box;
	int m_nPrims;
};
