#include "Geometry.h"
#include "IntersectData.h"
#include "RayBV.h"

float CRayBV::Build(CGeometry* pGeom)
{
	m_pGeom = pGeom;
	return 0.0f;
}

void CRayBV::GetNodeBV(BV*& pBV, int iNode, int iCaller)
{
	pBV = &g_idata[iCaller].BVRay;
	g_idata[iCaller].BVRay.iNode = 0;
	g_idata[iCaller].BVRay.type = primitives::ray::type;
	g_idata[iCaller].BVRay.aray.origin = m_pray->origin;
	g_idata[iCaller].BVRay.aray.dir = m_pray->dir;
}

void CRayBV::GetNodeBV(BV*& pBV, const Vec3& sweepdir, float sweepstep, int iNode, int iCaller)
{
}

void CRayBV::GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, int iNode, int iCaller)
{
	pBV = &g_idata[iCaller].BVRay;
	g_idata[iCaller].BVRay.iNode = 0;
	g_idata[iCaller].BVRay.type = primitives::ray::type;
	g_idata[iCaller].BVRay.aray.origin = Rw * m_pray->origin * scalew + offsw;
	g_idata[iCaller].BVRay.aray.dir = Rw * m_pray->dir * scalew;
}

void CRayBV::GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, const Vec3& sweepdir,
                       float sweepstep, int iNode, int iCaller)
{
}

int CRayBV::GetNodeContents(int iNode, BV* pBVCollider, int bColliderUsed, int bColliderLocal,
                            geometry_under_test* pGTest, geometry_under_test* pGTestOp)
{
	return m_pGeom->GetPrimitiveList(0, 1, pBVCollider->type, *pBVCollider, bColliderLocal, pGTest, pGTestOp,
	                                 pGTest->primbuf, pGTest->idbuf);
}
