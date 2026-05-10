#pragma once

#include "CryCommon/CryPhysics/IPhysics.h"

class CGeometry;
struct geometry_under_test;
struct BV;
struct BBox;

enum BVtreetypes
{
	BVT_OBB = 0,
	BVT_AABB = 1,
	BVT_SINGLEBOX = 2,
	BVT_RAY = 3,
	BVT_HEIGHTFIELD = 4,
	BVT_VOXEL = 5
};

class CBVTree
{
public:
	virtual ~CBVTree() = default;
	virtual int GetType() = 0;
	virtual void GetBBox(primitives::box* pbox) {}
	virtual int MaxPrimsInNode() { return 1; }
	virtual float Build(CGeometry* pGeom) = 0;
	virtual void SetGeomConvex() {}

	virtual int PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
	                                       geometry_under_test* pGTestColl);

	virtual void CleanupAfterIntersectionTest(geometry_under_test* pGTest) {}
	virtual void GetNodeBV(BV*& pBV, int iNode = 0, int iCaller = 0) = 0;
	virtual void GetNodeBV(BV*& pBV, const Vec3& sweepdir, float sweepstep, int iNode = 0, int iCaller = 0) = 0;
	virtual void GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, int iNode = 0,
	                       int iCaller = 0) = 0;
	virtual void GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, const Vec3& sweepdir,
	                       float sweepstep, int iNode = 0, int iCaller = 0) = 0;
	virtual float SplitPriority(const BV* pBV) { return 0.0f; }
	virtual void GetNodeChildrenBVs(const Matrix33& Rw, const Vec3& offsw, float scalew, const BV* pBV_parent,
	                                BV*& pBV_child1, BV*& pBV_child2, int iCaller = 0)
	{
	}
	virtual void GetNodeChildrenBVs(const BV* pBV_parent, BV*& pBV_child1, BV*& pBV_child2, int iCaller = 0) {}
	virtual void GetNodeChildrenBVs(const BV* pBV_parent, const Vec3& sweepdir, float sweepstep, BV*& pBV_child1,
	                                BV*& pBV_child2, int iCaller = 0)
	{
	}
	virtual void ReleaseLastBVs(int iCaller = 0) {}
	virtual void ReleaseLastSweptBVs(int iCaller = 0) {}
	virtual void ResetCollisionArea() {}
	virtual float GetMaxSkipDim() { return 0; }

	virtual void GetMemoryStatistics(ICrySizer*) {}
	virtual void Save(CMemStream& stm) {}
	virtual void Load(CMemStream& stm, CGeometry* pGeom) {}

	virtual int GetNodeContents(int iNode, BV* pBVCollider, int bColliderUsed, int bColliderLocal,
	                            geometry_under_test* pGTest, geometry_under_test* pGTestOp) = 0;
	virtual int GetNodeContentsIdx(int iNode, int& iStartPrim)
	{
		iStartPrim = 0;
		return 1;
	}
	virtual void MarkUsedTriangle(int itri, geometry_under_test* pGTest) {}
};

void DrawBBox(IPhysRenderer* pRenderer, int idxColor, geom_world_data* gwd, CBVTree* pTree, BBox* pbbox, int maxlevel,
              int level, int iCaller);
