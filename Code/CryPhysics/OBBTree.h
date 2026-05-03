#pragma once

#include "BVTree.h"

class CTriMesh;
struct OBBnode;

class COBBTree final : public CBVTree
{
public:
	COBBTree();
	~COBBTree();

	int GetType() override { return BVT_OBB; }

	float Build(CGeometry* pMesh) override;
	void SetGeomConvex() override;

	void SetParams(int nMinTrisPerNode, int nMaxTrisPerNode, float skipdim);
	float BuildNode(int iNode, int iTriStart, int nTris, int nDepth);

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
	void ReleaseLastBVs(int iCaller = 0) override;
	void ReleaseLastSweptBVs(int iCaller = 0) override;
	int GetNodeContents(int iNode, BV* pBVCollider, int bColliderUsed, int bColliderLocal,
	                    geometry_under_test* pGTest, geometry_under_test* pGTestOp) override;
	int GetNodeContentsIdx(int iNode, int& iStartPrim) override;
	void MarkUsedTriangle(int itri, geometry_under_test* pGTest) override;
	float GetMaxSkipDim() override;

	void GetMemoryStatistics(ICrySizer*) override {}
	void Save(CMemStream& stm) override;
	void Load(CMemStream& stm, CGeometry* pGeom) override;

	CTriMesh* m_pMesh;
	OBBnode* m_pNodes;
	int m_nNodes;
	int m_nNodesAlloc;
	index_t* m_pTri2Node;
	int m_nMaxTrisInNode;

	int m_nMinTrisPerNode;
	int m_nMaxTrisPerNode;
	float m_maxSkipDim;
	int* m_pMapVtxUsed;
	Vec3* m_pVtxUsed;
};
