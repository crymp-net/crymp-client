#pragma once

#include "BVTree.h"

class CGeometry;

class CRayBV final : public CBVTree
{
public:
	CRayBV() = default;

	int GetType() override { return BVT_RAY; }

	float Build(CGeometry* pGeom) override;
	void GetNodeBV(BV*& pBV, int iNode = 0, int iCaller = 0) override;
	void GetNodeBV(BV*& pBV, const Vec3& sweepdir, float sweepstep, int iNode = 0, int iCaller = 0) override;
	void GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, int iNode = 0,
	               int iCaller = 0) override;
	void GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, const Vec3& sweepdir,
	               float sweepstep, int iNode = 0, int iCaller = 0) override;
	int GetNodeContents(int iNode, BV* pBVCollider, int bColliderUsed, int bColliderLocal,
	                    geometry_under_test* pGTest, geometry_under_test* pGTestOp) override;

	void SetRay(primitives::ray* pray) { m_pray = pray; }

	CGeometry* m_pGeom;
	primitives::ray* m_pray;
};
