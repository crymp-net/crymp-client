#include "HeightfieldBV.h"
#include "IntersectData.h"
#include "TriMesh.h"
#include "Utils.h"

float CHeightfieldBV::Build(CGeometry* pGeom)
{
	m_pMesh = (CTriMesh*)pGeom;
	return (m_phf->size.x * m_phf->step.x * m_phf->size.y * m_phf->step.y * (m_maxHeight - m_minHeight));
}

void CHeightfieldBV::SetHeightfield(primitives::heightfield* phf)
{
	m_phf = phf;
	m_PatchStart.set(0, 0);
	m_PatchSize = m_phf->size;
	m_minHeight = -1;
	m_maxHeight = 1;
}

void CHeightfieldBV::GetBBox(primitives::box* pbox)
{
	pbox->size.x = m_PatchSize.x * m_phf->step.x * 0.5f;
	pbox->size.y = m_PatchSize.y * m_phf->step.y * 0.5f;
	pbox->size.z = (m_maxHeight - m_minHeight) * 0.5f;
	pbox->center.x = (m_PatchStart.x * m_phf->step.x) + pbox->size.x;
	pbox->center.y = (m_PatchStart.y * m_phf->step.y) + pbox->size.y;
	pbox->center.z = m_minHeight + pbox->size.z;
	pbox->Basis.SetIdentity();
	pbox->bOriented = 0;
}

void CHeightfieldBV::GetNodeBV(BV*& pBV, int iNode, int iCaller)
{
	pBV = &g_idata[iCaller].BVhf;
	g_idata[iCaller].BVhf.hf = *m_phf;
	g_idata[iCaller].BVhf.hf.Basis.SetIdentity();
	g_idata[iCaller].BVhf.hf.bOriented = 0;
	g_idata[iCaller].BVhf.hf.origin.zero();
}

void CHeightfieldBV::GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, int iNode, int iCaller)
{
	pBV = &g_idata[iCaller].BVhf;
	g_idata[iCaller].BVhf.hf = *m_phf;
	g_idata[iCaller].BVhf.hf.Basis = Rw.T();
	g_idata[iCaller].BVhf.hf.bOriented = 1;
	g_idata[iCaller].BVhf.hf.origin = offsw;
	g_idata[iCaller].BVhf.hf.step = m_phf->step * scalew;
	g_idata[iCaller].BVhf.hf.stepr = m_phf->stepr * (scalew == 1.0f ? 1 : 1 / scalew);
	g_idata[iCaller].BVhf.hf.heightscale *= scalew;
}

void CHeightfieldBV::MarkUsedTriangle(int itri, geometry_under_test* pGTest)
{
	m_pUsedTriMap[itri >> 5] |= 1u << (itri & 31);
}

int CHeightfieldBV::GetNodeContents(int iNode, BV* pBVCollider, int bColliderUsed, int bColliderLocal,
                                    geometry_under_test* pGTest, geometry_under_test* pGTestOp)
{
	int iStart, nCols, nRows, nPrims, ix, iy, ix0, iy0, i, j;
	float minz;

	if (pBVCollider->type == primitives::box::type)
	{
		project_box_on_grid((primitives::box*)(primitives::primitive*)*pBVCollider, m_phf,
		                    (geometry_under_test*)((intptr_t)pGTest & -((intptr_t)bColliderLocal ^ 1)), ix0,
		                    iy0, nCols, nRows, minz);
		ix = ix0 - m_PatchStart.x;
		iy = iy0 - m_PatchStart.y;
		ix &= ~(ix >> 31);
		iy &= ~(iy >> 31);
		nCols = min(ix0 + nCols, m_PatchStart.x + m_PatchSize.x) - ix -
		        m_PatchStart.x; // don't forget to crop values to the current patch
		nRows = min(iy0 + nRows, m_PatchStart.y + m_PatchSize.y) - iy - m_PatchStart.y;
	}
	else
	{
		nCols = m_PatchSize.x;
		nRows = m_PatchSize.y;
		ix = iy = 0;
	}

	// if (m_phf->gettype(ix,iy)==-1)
	//	return 0;

	iStart = ix + (iy * m_PatchSize.x);
	if (!bColliderUsed)
	{
		for (i = nPrims = 0; i < nRows; i++, iStart += m_PatchSize.x)
		{
			nPrims += m_pMesh->GetPrimitiveList(
			    iStart * 2, nCols * 2, pBVCollider->type, *pBVCollider, bColliderLocal, pGTest, pGTestOp,
			    (primitives::primitive*)((char*)pGTest->primbuf + (nPrims * pGTest->szprim)),
			    pGTest->idbuf + nPrims);
		}
	}
	else
	{
		for (i = nPrims = 0; i < nRows; i++, iStart += m_PatchSize.x)
		{
			for (j = 0; j < nCols * 2; j++)
			{
				int itri = (iStart * 2) + j;
				if (!(m_pUsedTriMap[itri >> 5] >> (itri & 31) & 1))
				{
					nPrims += m_pMesh->GetPrimitiveList(
					    itri, 1, pBVCollider->type, *pBVCollider, bColliderLocal, pGTest, pGTestOp,
					    (primitives::primitive*)((char*)pGTest->primbuf +
					                             (nPrims * pGTest->szprim)),
					    pGTest->idbuf + nPrims);
				}
			}
		}
	}

	return nPrims;
}

void project_box_on_grid(primitives::box* pbox, primitives::grid* pgrid, geometry_under_test* pGTest, int& ix, int& iy,
                         int& sx, int& sy, float& minz)
{
	Vec3 center;
	Vec3 dim;
	if (!pGTest)
	{
		Matrix33 Basis = pbox->Basis;
		dim = pbox->size * Basis.Fabs();
		center = pbox->center;
	}
	else
	{
		Matrix33 Basis;
		if (pbox->bOriented)
		{
			Basis = pbox->Basis * pGTest->R_rel;
		}
		else
		{
			Basis = pGTest->R_rel;
		}
		dim = (pbox->size * pGTest->rscale_rel) * Basis.Fabs();
		center = ((pbox->center - pGTest->offset_rel) * pGTest->R_rel) * pGTest->rscale_rel;
	}
	ix = physics_float2int(((center.x - dim.x) * pgrid->stepr.x) - 0.5f);
	ix &= ~(ix >> 31);
	iy = physics_float2int(((center.y - dim.y) * pgrid->stepr.y) - 0.5f);
	iy &= ~(iy >> 31);
	sx = min(physics_float2int(((center.x + dim.x) * pgrid->stepr.x) + 0.5f), pgrid->size.x) - ix;
	sy = min(physics_float2int(((center.y + dim.y) * pgrid->stepr.y) + 0.5f), pgrid->size.y) - iy;
	minz = center.z - dim.z;
}
