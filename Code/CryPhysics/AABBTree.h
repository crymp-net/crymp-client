#pragma once

#include "BVTree.h"

class CTriMesh;
struct AABBnode;

class CAABBTree final : public CBVTree
{
public:
	CAABBTree() = default;
	~CAABBTree();

	int GetType() override { return BVT_AABB; }

	float Build(CGeometry* pMesh) override;
	void SetGeomConvex() override;

	void SetParams(int nMinTrisPerNode, int nMaxTrisPerNode, float skipdim, const Matrix33& Basis);
	float BuildNode(int iNode, int iTriStart, int nTris, Vec3 center, Vec3 size, int nDepth);

	int PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
	                               geometry_under_test* pGTestColl) override;
	void CleanupAfterIntersectionTest(geometry_under_test* pGTest) override;
	void GetBBox(primitives::box* pbox) override;
	int MaxPrimsInNode() override { return m_nMaxTrisInNode; }
	void GetNodeBV(BV*& pBV, int iNode = 0, int iCaller = 0) override;
	void GetNodeBV(BV*& pBV, const Vec3& sweepdir, float sweepstep, int iNode = 0, int iCaller = 0) override;
	void GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, int iNode = 0,
	               int iCaller = 0) override;
	void GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, const Vec3& sweepdir,
	               float sweepstep, int iNode = 0, int iCaller = 0) override;
	float SplitPriority(const BV* pBV) override;
	void GetNodeChildrenBVs(const Matrix33& Rw, const Vec3& offsw, float scalew, const BV* pBV_parent,
	                        BV*& pBV_child1, BV*& pBV_child2, int iCaller = 0) override;
	void GetNodeChildrenBVs(const BV* pBV_parent, BV*& pBV_child1, BV*& pBV_child2, int iCaller = 0) override;
	void GetNodeChildrenBVs(const BV* pBV_parent, const Vec3& sweepdir, float sweepstep, BV*& pBV_child1,
	                        BV*& pBV_child2, int iCaller = 0) override;
	void ReleaseLastBVs(int iCaller) override;
	void ReleaseLastSweptBVs(int iCaller) override;
	int GetNodeContents(int iNode, BV* pBVCollider, int bColliderUsed, int bColliderLocal,
	                    geometry_under_test* pGTest, geometry_under_test* pGTestOp) override;
	int GetNodeContentsIdx(int iNode, int& iStartPrim) override;

	void MarkUsedTriangle(int itri, geometry_under_test* pGTest) override;
	float GetMaxSkipDim() override;

	void GetRootNodeDim(Vec3& center, Vec3& size);

	void GetMemoryStatistics(ICrySizer*) override {}
	void Save(CMemStream& stm) override;
	void Load(CMemStream& stm, CGeometry* pGeom) override;

	CTriMesh* m_pMesh;
	AABBnode* m_pNodes;
	int m_nNodes, m_nNodesAlloc;
	Vec3 m_center;
	Vec3 m_size;
	Matrix33 m_Basis;
	int m_bOriented;
	int *m_pTri2Node, m_nBitsLog;
	int m_nMaxTrisPerNode, m_nMinTrisPerNode;
	int m_nMaxTrisInNode;
	float m_maxSkipDim;
};
