#pragma once

#include <atomic>
#include <thread>

#include "CryCommon/CryPhysics/IPhysics.h"

#include "OverlapChecker.h"

class CBVTree;
class CGeometry;

struct surface_desc
{
	Vec3 n{};
	int idx{};
	int iFeature{};
};

struct edge_desc
{
	Vec3 dir{};
	Vec3 n[2]{};
	int idx{};
	int iFeature{};
};

struct tritem
{
	int itri{};
	int itri_parent{};
	int ivtx0{};
};

struct vtxitem
{
	int ivtx{};
	int id{};
	int ibuddy[2]{};
};

struct geometry_under_test
{
	CGeometry* pGeometry;
	CBVTree* pBVtree;
	int* pUsedNodesMap;
	int* pUsedNodesIdx;
	int nUsedNodes;
	int nMaxUsedNodes;
	int bStopIntersection;
	int bCurNodeUsed;

	Matrix33 R, R_rel;
	Vec3 offset, offset_rel;
	float scale, rscale, scale_rel, rscale_rel;
	int bTransformUpdated;

	Vec3 v;
	Vec3 w, centerOfMass;
	Vec3 centerOfRotation;
	intersection_params* pParams;

	Vec3 axisContactNormal;
	Vec3 sweepdir, sweepdir_loc;
	float sweepstep, sweepstep_loc;
	Vec3 ptOutsidePivot;

	int typeprim;
	primitives::primitive* primbuf;  // used to get node contents
	primitives::primitive* primbuf1; // used to get unprojection candidates
	int szprimbuf, szprimbuf1;
	int* iFeature_buf; // feature that led to this primitive
	char* idbuf;       // ids of unprojection candidates
	int szprim;

	surface_desc* surfaces; // the last potentially surfaces
	edge_desc* edges;       // the last potentially contacting edges
	int nSurfaces, nEdges;
	float minAreaEdge;

	geom_contact* contacts;
	int* pnContacts;
	int nMaxContacts;

	int iCaller;
};

struct BV
{
	int type{};
	int iNode{};

	operator primitives::primitive*()
	{
		return (primitives::primitive*)((char*)this + sizeof(type) + sizeof(iNode));
	}
};

struct BBox : BV
{
	primitives::box abox{};

	BBox() { this->type = primitives::box::type; }
};

struct BBoxExt : BBox
{
	primitives::box aboxStatic{};

	BBoxExt() { this->type = primitives::box::type; }
};

struct BVheightfield : BV
{
	primitives::heightfield hf{};

	BVheightfield() { this->type = primitives::heightfield::type; }
};

struct BVvoxelgrid : BV
{
	primitives::voxelgrid voxgrid{};

	BVvoxelgrid() { this->type = primitives::voxelgrid::type; }
};

struct BVray : BV
{
	primitives::ray aray{};

	BVray() { this->type = primitives::ray::type; }
};

struct IntersectData
{
	primitives::indexed_triangle IdxTriBuf[256]{};
	int IdxTriBufPos{};
	primitives::cylinder CylBuf[2]{};
	int CylBufPos{};
	primitives::sphere SphBuf[2]{};
	int SphBufPos{};
	primitives::box BoxBuf[2]{};
	int BoxBufPos{};
	primitives::ray RayBuf[2]{};

	surface_desc SurfaceDescBuf[64]{};
	int SurfaceDescBufPos{};

	edge_desc EdgeDescBuf[64]{};
	int EdgeDescBufPos{};

	int iFeatureBuf[64]{};
	int iFeatureBufPos{};

	char IdBuf[256]{};
	int IdBufPos{};

	int UsedNodesMap[8192]{};
	int UsedNodesMapPos{};

	int UsedNodesIdx[64]{};
	int UsedNodesIdxPos{};

	geom_contact Contacts[64]{};
	int nTotContacts{};

	geom_contact_area AreaBuf[32]{};
	int nAreas{};
	Vec3 AreaPtBuf[256]{};
	int AreaPrimBuf0[256]{};
	int AreaFeatureBuf0[256]{};
	int AreaPrimBuf1[256]{};
	int AreaFeatureBuf1[256]{};
	int nAreaPt{};

	Vec3 BrdPtBuf[2048]{};
	int BrdPtBufPos, BrdPtBufStart{};
	int BrdiTriBuf[2048][2]{};
	float BrdSeglenBuf[2048]{};
	int UsedVtxMap[4096]{};
	int UsedTriMap[4096]{};
	vector2df PolyPtBuf[1024]{};
	int PolyVtxIdBuf[1024]{};
	int PolyEdgeIdBuf[1024]{};
	int PolyPtBufPos{};

	tritem TriQueue[512]{};
	vtxitem VtxList[512]{};

	vector2df BoxCont[8]{};
	int BoxVtxId[8], BoxEdgeId[8]{};
	char BoxIdBuf[3]{};
	surface_desc BoxSurfaceBuf[3]{};
	edge_desc BoxEdgeBuf[3]{};

	vector2df CylCont[64]{};
	int CylContId[64]{};
	char CylIdBuf[1]{};
	surface_desc CylSurfaceBuf[1]{};
	edge_desc CylEdgeBuf[1]{};

	BBox BBoxBuf[128]{};
	int BBoxBufPos{};
	BBoxExt BBoxExtBuf[64]{};
	int BBoxExtBufPos{};

	BVheightfield BVhf{};
	BVvoxelgrid BVvox{};
	BVray BVRay{};

	COverlapChecker Overlapper{};

	volatile int lockIntersect{};
};

extern std::atomic<std::thread::id> g_physicsThreadId;
extern IntersectData g_idata[2];

int GetCaller();

void ResetGlobalPrimsBuffers(int iCaller = 0);
