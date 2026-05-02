#pragma once

#include "BVTree.h"

class CTriMesh;

class CHeightfieldBV final : public CBVTree
{
public:
	CHeightfieldBV()
	{
		m_pUsedTriMap = 0;
		m_minHeight = 0.f;
		m_maxHeight = 0.f;
	}

	~CHeightfieldBV()
	{
		if (m_pUsedTriMap)
		{
			delete[] m_pUsedTriMap;
		}
	}

	int GetType() override { return BVT_HEIGHTFIELD; }

	float Build(CGeometry* pGeom) override;
	void SetHeightfield(primitives::heightfield* phf);
	void GetBBox(primitives::box* pbox) override;
	int MaxPrimsInNode() override { return m_PatchSize.x * m_PatchSize.y * 2; }

	void GetNodeBV(BV*& pBV, int iNode = 0, int iCaller = 0) override;
	void GetNodeBV(BV*& pBV, const Vec3& sweepdir, float sweepstep, int iNode = 0, int iCaller = 0) override {}
	void GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, int iNode = 0,
	               int iCaller = 0) override;
	void GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, const Vec3& sweepdir,
	               float sweepstep, int iNode = 0, int iCaller = 0) override
	{
	}
	int GetNodeContents(int iNode, BV* pBVCollider, int bColliderUsed, int bColliderLocal,
	                    geometry_under_test* pGTest, geometry_under_test* pGTestOp) override;
	void MarkUsedTriangle(int itri, geometry_under_test* pGTest) override;

	CTriMesh* m_pMesh;
	primitives::heightfield* m_phf;
	vector2di m_PatchStart;
	vector2di m_PatchSize;
	float m_minHeight, m_maxHeight;
	unsigned int* m_pUsedTriMap;
};

extern void project_box_on_grid(primitives::box* pbox, primitives::grid* pgrid, geometry_under_test* pGTest, int& ix,
                                int& iy, int& sx, int& sy, float& minz);
