#include "CryCommon/CryMath/GeomQuery.h"
#include "CryCommon/CrySystem/ISystem.h"

#include "BVTree.h"
#include "Geometry.h"
#include "GeomManager.h"
#include "IntersectData.h"
#include "IntersectionChecker.h"
#include "UnprojectionChecker.h"
#include "Utils.h"

namespace {

int IntersectBVs(geometry_under_test* pGTest, BV* pBV1, BV* pBV2)
{
	intptr_t mask;
	int iNode[2], *pUsedNodesMap[2], res = 0, nullmap = 0, bNodeUsed[2], *pnullmap = &nullmap;
	mask = iszero_mask(pGTest[0].pUsedNodesMap); // mask = -(pUsedNodeMap==0)
	pUsedNodesMap[0] = (int*)(((intptr_t)pGTest[0].pUsedNodesMap & ~mask) | ((intptr_t)pnullmap & mask));
	iNode[0] = pBV1->iNode & ~mask;
	mask = iszero_mask(pGTest[1].pUsedNodesMap); // mask = -(pUsedNodeMap==0)
	pUsedNodesMap[1] = (int*)(((intptr_t)pGTest[1].pUsedNodesMap & ~mask) | ((intptr_t)pnullmap & mask));
	iNode[1] = pBV2->iNode & ~mask;
	bNodeUsed[0] = pUsedNodesMap[0][iNode[0] >> 5] >> (iNode[0] & 31) & 1;
	bNodeUsed[1] = pUsedNodesMap[1][iNode[1] >> 5] >> (iNode[1] & 31) & 1;

	if (bNodeUsed[0] & bNodeUsed[1])
	{
		return 0; // skip check only if both nodes are marked used
	}

	if (!g_idata[pGTest->iCaller].Overlapper.Check(pBV1->type, pBV2->type, *pBV1, *pBV2))
	{
		return 0;
	}

	float split1, split2;
	split1 = pGTest[0].pBVtree->SplitPriority(pBV1);
	split2 = pGTest[1].pBVtree->SplitPriority(pBV2);

	BV *pBV_split1, *pBV_split2;

	if (split1 > split2 && split1 > 0)
	{ // split the first BV
		pGTest[0].pBVtree->GetNodeChildrenBVs(pBV1, pBV_split1, pBV_split2, pGTest->iCaller);
		res = IntersectBVs(pGTest, pBV_split1, pBV2);
		if (pGTest[0].bStopIntersection + pGTest[1].bStopIntersection > 0)
		{
			return res;
		}
		res += IntersectBVs(pGTest, pBV_split2, pBV2);
		pGTest[0].pBVtree->ReleaseLastBVs(pGTest->iCaller);
	}
	else if (split2 > 0)
	{ // split the second BV
		pGTest[1].pBVtree->GetNodeChildrenBVs(pGTest[1].R_rel, pGTest[1].offset_rel, pGTest[1].scale_rel, pBV2,
		                                      pBV_split1, pBV_split2, pGTest->iCaller);
		res = IntersectBVs(pGTest, pBV1, pBV_split1);
		if (pGTest[0].bStopIntersection + pGTest[1].bStopIntersection > 0)
		{
			return res;
		}
		res += IntersectBVs(pGTest, pBV1, pBV_split2);
		pGTest[1].pBVtree->ReleaseLastBVs(pGTest->iCaller);
	}
	else
	{
		int nPrims1, nPrims2, iClient, i, j;
		primitives::primitive* ptr[2];
		prim_inters inters;
		static Vec3 ptborder[16];
		inters.minPtDist2 = sqr(min(pGTest[0].pGeometry->m_minVtxDist, pGTest[1].pGeometry->m_minVtxDist));
		iClient = -((pGTest[1].pGeometry->m_iCollPriority - pGTest[0].pGeometry->m_iCollPriority) >> 31);
		inters.pt[1].zero();
		inters.n.zero();
		inters.ptborder = ptborder;
		inters.nborderpt = inters.nBestPtVal = 0;
		inters.nbordersz = (sizeof(ptborder) / sizeof(ptborder[0])) - 1;

		nPrims1 =
		    pGTest[0].pBVtree->GetNodeContents(pBV1->iNode, pBV2, bNodeUsed[1], 1, pGTest + 0, pGTest + 1);
		nPrims2 =
		    pGTest[1].pBVtree->GetNodeContents(pBV2->iNode, pBV1, bNodeUsed[0], 0, pGTest + 1, pGTest + 0);

		for (i = 0, ptr[0] = pGTest[0].primbuf; i < nPrims1;
		     i++, ptr[0] = (primitives::primitive*)((char*)ptr[0] + pGTest[0].szprim))
		{
			for (j = 0, ptr[1] = pGTest[1].primbuf; j < nPrims2;
			     j++, ptr[1] = (primitives::primitive*)((char*)ptr[1] + pGTest[1].szprim))
			{
				if (g_Intersector.Check(pGTest[iClient].typeprim, pGTest[iClient ^ 1].typeprim,
				                        ptr[iClient], ptr[iClient ^ 1], &inters))
				{
					inters.id[0] = pGTest[iClient].idbuf[(i & -(iClient ^ 1)) | (j & -iClient)];
					inters.id[1] = pGTest[iClient ^ 1].idbuf[(i & -iClient) | (j & -(iClient ^ 1))];
					inters.iNode[0] = pBV1->iNode;
					inters.iNode[1] = pBV2->iNode;
					pGTest[0].bCurNodeUsed = pGTest[1].bCurNodeUsed = 0;
					res += pGTest[iClient].pGeometry->RegisterIntersection(
					    ptr[iClient], ptr[iClient ^ 1], pGTest + iClient, pGTest + (iClient ^ 1),
					    &inters);
					if (pGTest[0].bStopIntersection + pGTest[1].bStopIntersection +
					        pGTest[0].bCurNodeUsed + pGTest[1].bCurNodeUsed >
					    0)
					{
						return res;
					}
					inters.nborderpt = inters.nBestPtVal = 0;
					inters.nbordersz = (sizeof(ptborder) / sizeof(ptborder[0])) - 1;
				}
			}
		}
	}

	return res;
}

int SweepTestBVs(geometry_under_test* pGTest, BV* pBV1, BV* pBV2)
{
	if (!g_idata[pGTest->iCaller].Overlapper.Check(pBV1->type, pBV2->type, *pBV1, *pBV2))
	{
		return 0;
	}

	int res = 0;
	float split1, split2;
	split1 = pGTest[0].pBVtree->SplitPriority(pBV1);
	split2 = pGTest[1].pBVtree->SplitPriority(pBV2);

	BV *pBV_split1, *pBV_split2;

	if (split1 > split2 && split1 > 0)
	{ // split the first BV
		pGTest[0].pBVtree->GetNodeChildrenBVs(pBV1, pGTest[0].sweepdir_loc, pGTest[0].sweepstep_loc, pBV_split1,
		                                      pBV_split2, pGTest->iCaller);
		res = SweepTestBVs(pGTest, pBV_split1, pBV2) + SweepTestBVs(pGTest, pBV_split2, pBV2);
		pGTest[0].pBVtree->ReleaseLastSweptBVs(pGTest->iCaller);
	}
	else if (split2 > 0)
	{ // split the second BV
		pGTest[1].pBVtree->GetNodeChildrenBVs(pGTest[1].R_rel, pGTest[1].offset_rel, pGTest[1].scale_rel, pBV2,
		                                      pBV_split1, pBV_split2, pGTest->iCaller);
		res = SweepTestBVs(pGTest, pBV1, pBV_split1) + SweepTestBVs(pGTest, pBV1, pBV_split2);
		pGTest[1].pBVtree->ReleaseLastBVs(pGTest->iCaller);
	}
	else
	{
		int nPrims1, nPrims2, i, j;
		primitives::primitive* ptr[2];
		contact contact_cur;
		geom_contact* pcontact = pGTest[0].contacts;
		unprojection_mode unproj;
		Vec3 startoffs = pGTest[0].offset;
		pGTest[0].offset += pGTest[0].sweepdir * pGTest[0].sweepstep;
		unproj.imode = 0;
		unproj.dir = -pGTest[0].sweepdir;
		unproj.tmax = pGTest[0].sweepstep;
		unproj.minPtDist = min(pGTest[0].pGeometry->m_minVtxDist, pGTest[1].pGeometry->m_minVtxDist);

		nPrims1 = pGTest[0].pBVtree->GetNodeContents(pBV1->iNode, pBV2, 0, -1, pGTest + 0, pGTest + 1);
		nPrims2 = pGTest[1].pBVtree->GetNodeContents(pBV2->iNode, pBV1, 0, 0, pGTest + 1, pGTest + 0);

		for (i = 0, ptr[0] = pGTest[0].primbuf; i < nPrims1;
		     i++, ptr[0] = (primitives::primitive*)((char*)ptr[0] + pGTest[0].szprim))
		{
			for (j = 0, ptr[1] = pGTest[1].primbuf; j < nPrims2;
			     j++, ptr[1] = (primitives::primitive*)((char*)ptr[1] + pGTest[1].szprim))
			{
				if (g_Unprojector.Check(&unproj, pGTest[0].typeprim, pGTest[1].typeprim, ptr[0], -1,
				                        ptr[1], -1, &contact_cur) &&
				    contact_cur.n * pGTest[0].sweepdir > 0 && contact_cur.t <= pGTest[0].sweepstep &&
				    pGTest[0].sweepstep - contact_cur.t < pcontact->t)
				{
					pcontact->t = pGTest[0].sweepstep - contact_cur.t;
					pcontact->pt = pcontact->center = contact_cur.pt;
					pcontact->n = contact_cur.n.normalized();
					pcontact->iPrim[0] = pGTest[0].typeprim != primitives::indexed_triangle::type
					                         ? 0
					                         : ((primitives::indexed_triangle*)ptr[0])->idx;
					pcontact->iPrim[1] = pGTest[1].typeprim == primitives::indexed_triangle::type
					                         ? ((primitives::indexed_triangle*)ptr[1])->idx
					                         : 0;
					pcontact->iFeature[0] = contact_cur.iFeature[0];
					pcontact->iFeature[1] = contact_cur.iFeature[1];
					pcontact->id[0] = pGTest[0].idbuf[i];
					pcontact->id[1] = pGTest[1].idbuf[j];
					pcontact->iNode[0] = pBV1->iNode;
					pcontact->iNode[1] = pBV2->iNode;
					res++;
					*(pGTest[0].pnContacts) = 1;
				}
			}
		}
		pGTest[0].offset = startoffs;
	}

	return res;
}

} // unnamed namespace

void CGeometry::Release()
{
	if (--m_nRefCount <= 0)
	{
		if (gEnv && gEnv->pPhysicalWorld)
		{
			((CGeomManager*)gEnv->pPhysicalWorld)->m_sizeExtGeoms -= GetSizeFast();
		}
		delete this;
	}
}

int CGeometry::Intersect(IGeometry* pCollider, geom_world_data* pdata1, geom_world_data* pdata2,
                         intersection_params* pparams, geom_contact*& pcontacts)
{
	// FUNCTION_PROFILER( GetISystem(),PROFILE_PHYSICS );

	geometry_under_test gtest[2];
	geom_world_data* pdata[2] = {pdata1, pdata2};
	int i, j, jmax, mask, nContacts = 0, iStartNode[2], bActive = 0, iCaller;
	intersection_params defip;
	Vec3r offsetWorld;
	if (!pparams)
	{
		pparams = &defip;
		bActive = 1;
	}
	iCaller = gtest[0].iCaller = gtest[1].iCaller = GetCaller();
	ReadLockCond lock0(m_lockUpdate, pparams->bThreadSafeMesh ^ 1),
	    lock1(((CGeometry*)pCollider)->m_lockUpdate, !pparams->bThreadSafeMesh && pCollider != this);
	WriteLockCond lock(g_idata[iCaller].lockIntersect, pparams->bThreadSafe ^ 1);
	pparams->plock = &g_idata[iCaller].lockIntersect;

	// zero all global buffer pointers
	ResetGlobalPrimsBuffers(iCaller);
	g_idata[iCaller].Overlapper.Init();
	if (!pparams->bKeepPrevContacts)
	{
		g_idata[iCaller].nAreas = 0;
		g_idata[iCaller].nAreaPt = 0;
		g_idata[iCaller].nTotContacts = 0;
	}
	if (g_idata[iCaller].nTotContacts >= sizeof(g_idata[iCaller].Contacts) / sizeof(g_idata[iCaller].Contacts[0]))
	{
		return 0;
	}
	g_idata[iCaller].SurfaceDescBufPos = 0;
	g_idata[iCaller].EdgeDescBufPos = 0;
	g_idata[iCaller].IdBufPos = 0;
	g_idata[iCaller].iFeatureBufPos = 0;
	g_idata[iCaller].UsedNodesMapPos = 0;
	g_idata[iCaller].UsedNodesIdxPos = 0;
	pcontacts = g_idata[iCaller].Contacts + g_idata[iCaller].nTotContacts;
	pparams->pGlobalContacts = g_idata[iCaller].Contacts;

	if (!pparams->bNoAreaContacts &&
	    g_idata[iCaller].nAreas < sizeof(g_idata[iCaller].AreaBuf) / sizeof(g_idata[iCaller].AreaBuf[0]) &&
	    g_idata[iCaller].nAreaPt < sizeof(g_idata[iCaller].AreaPtBuf) / sizeof(g_idata[iCaller].AreaPtBuf[0]))
	{
		pcontacts[0].parea = g_idata[iCaller].AreaBuf + g_idata[iCaller].nAreas;
		pcontacts[0].parea->pt = g_idata[iCaller].AreaPtBuf + g_idata[iCaller].nAreaPt;
		pcontacts[0].parea->piPrim[0] = g_idata[iCaller].AreaPrimBuf0 + g_idata[iCaller].nAreaPt;
		pcontacts[0].parea->piFeature[0] = g_idata[iCaller].AreaFeatureBuf0 + g_idata[iCaller].nAreaPt;
		pcontacts[0].parea->piPrim[1] = g_idata[iCaller].AreaPrimBuf1 + g_idata[iCaller].nAreaPt;
		pcontacts[0].parea->piFeature[1] = g_idata[iCaller].AreaFeatureBuf1 + g_idata[iCaller].nAreaPt;
		pcontacts[0].parea->npt = 0;
		pcontacts[0].parea->nmaxpt =
		    (sizeof(g_idata[iCaller].AreaPtBuf) / sizeof(g_idata[iCaller].AreaPtBuf[0])) -
		    g_idata[iCaller].nAreaPt;
	}
	else
	{
		pcontacts[0].parea = 0;
	}
	if (pparams->bSweepTest && pdata1->v.len2() == 0)
	{
		pparams->bSweepTest = 0;
		pdata1->v.Set(0, 0, 1);
		pparams->vrel_min = 0;
	}

	for (i = 0; i < 2; i++)
	{
		if (pdata[i])
		{
			gtest[i].offset = pdata[i]->offset;
			gtest[i].R = pdata[i]->R;
			gtest[i].scale = pdata[i]->scale;
			gtest[i].rscale = 1.0f / pdata[i]->scale;
			gtest[i].v = pdata[i]->v;
			gtest[i].w = pdata[i]->w;
			gtest[i].centerOfMass = pdata[i]->centerOfMass;
			iStartNode[i] = pdata[i]->iStartNode;
		}
		else
		{
			gtest[i].offset.zero();
			gtest[i].R.SetIdentity();
			gtest[i].scale = gtest[i].rscale = 1.0f;
			gtest[i].v.zero();
			gtest[i].w.zero();
			gtest[i].centerOfMass.zero();
			iStartNode[i] = 0;
		}
		gtest[i].centerOfRotation = pparams->centerOfRotation;
		gtest[i].axisContactNormal = pparams->axisContactNormal * (1 - (i << 1));
		gtest[i].contacts = pcontacts;
		gtest[i].pnContacts = &nContacts;
		gtest[i].nMaxContacts = (sizeof(g_idata[iCaller].Contacts) / sizeof(g_idata[iCaller].Contacts[0])) -
		                        g_idata[iCaller].nTotContacts;
		gtest[i].bStopIntersection = 0;
		gtest[i].sweepstep = 0;
		gtest[i].ptOutsidePivot = pparams->ptOutsidePivot[i];
		gtest[i].pParams = pparams;
	}
	for (i = 0; i < 2; i++)
	{
		gtest[i].offset_rel = ((gtest[i].offset - gtest[i ^ 1].offset) * gtest[i ^ 1].R) * gtest[i ^ 1].rscale;
		(gtest[i].R_rel = gtest[i ^ 1].R.T()) *= gtest[i].R;
		gtest[i].scale_rel = gtest[i].scale * gtest[i ^ 1].rscale;
		gtest[i].rscale_rel = gtest[i ^ 1].scale * gtest[i].rscale;
	}
	pparams->bBothConvex = m_bIsConvex & ((CGeometry*)pCollider)->m_bIsConvex;

	gtest[0].offset -= (offsetWorld = gtest[1].offset);
	gtest[0].centerOfMass -= offsetWorld;
	gtest[0].centerOfRotation -= offsetWorld;
	gtest[0].ptOutsidePivot -= offsetWorld;
	gtest[1].centerOfMass -= offsetWorld;
	gtest[1].centerOfRotation -= offsetWorld;
	gtest[1].ptOutsidePivot -= offsetWorld;
	gtest[1].offset.zero(); // use relative offset in calculations to improve accuracy

	BV *pBV1, *pBV2;

	if (!pparams->bSweepTest)
	{
		if (!PrepareForIntersectionTest(gtest + 0, (CGeometry*)pCollider, gtest + 1,
		                                pparams->bKeepPrevContacts))
		{
			return 0;
		}
		if (!((CGeometry*)pCollider)
		         ->PrepareForIntersectionTest(gtest + 1, this, gtest + 0, pparams->bKeepPrevContacts))
		{
			CleanupAfterIntersectionTest(gtest + 0);
			return 0;
		}
		if (pcontacts[0].parea)
		{
			pcontacts[0].parea->minedge = min(gtest[0].minAreaEdge, gtest[1].minAreaEdge);
		}

		if (iStartNode[0] + iStartNode[1])
		{
			gtest[0].pBVtree->GetNodeBV(pBV1, iStartNode[0], iCaller);
			gtest[1].pBVtree->GetNodeBV(gtest[1].R_rel, gtest[1].offset_rel, gtest[1].scale_rel, pBV2,
			                            iStartNode[1], iCaller);
			IntersectBVs(gtest, pBV1, pBV2);
		}
		if (nContacts == 0)
		{
			gtest[0].pBVtree->GetNodeBV(pBV1, 0, iCaller);
			gtest[1].pBVtree->GetNodeBV(gtest[1].R_rel, gtest[1].offset_rel, gtest[1].scale_rel, pBV2, 0,
			                            iCaller);
			IntersectBVs(gtest, pBV1, pBV2);
		}

		int iClient = -((gtest[1].pGeometry->m_iCollPriority - gtest[0].pGeometry->m_iCollPriority) >> 31);
		if (iClient)
		{
			for (i = 0; i < nContacts; i++)
			{
				if (pcontacts[i].parea && pcontacts[i].parea->npt)
				{
					Vec3 ntmp = pcontacts[i].n;
					pcontacts[i].n = pcontacts[i].parea->n1;
					pcontacts[i].parea->n1 = ntmp;
					int* pprim = pcontacts[i].parea->piPrim[0];
					pcontacts[i].parea->piPrim[0] = pcontacts[i].parea->piPrim[1];
					pcontacts[i].parea->piPrim[1] = pprim;
					int* pfeat = pcontacts[i].parea->piFeature[0];
					pcontacts[i].parea->piFeature[0] = pcontacts[i].parea->piFeature[1];
					pcontacts[i].parea->piFeature[1] = pfeat;
					if (pcontacts[i].iUnprojMode)
					{
						pcontacts[i].n =
						    pcontacts[i].n.GetRotated(pcontacts[i].dir, -pcontacts[i].t);
						pcontacts[i].parea->n1 = pcontacts[i].parea->n1.GetRotated(
						    pcontacts[i].dir, -pcontacts[i].t);
					}
					if (pcontacts[i].iUnprojMode)
					{
						for (j = 0; j < pcontacts[i].parea->npt; j++)
						{
							pcontacts[i].parea->pt[j] =
							    pcontacts[i].parea->pt[j].GetRotated(
								gtest[0].centerOfRotation, pcontacts[i].dir,
								-pcontacts[i].t);
						}
					}
					else
					{
						for (j = 0; j < pcontacts[i].parea->npt; j++)
						{
							pcontacts[i].parea->pt[j] -= pcontacts[i].dir * pcontacts[i].t;
						}
					}
				}
				else
				{
					if (pcontacts[i].iUnprojMode)
					{
						pcontacts[i].n =
						    pcontacts[i].n.GetRotated(pcontacts[i].dir, -pcontacts[i].t);
					}
					pcontacts[i].n.Flip();
				}
				if (pcontacts[i].nborderpt > 1)
				{
					for (j = 0; j < pcontacts[i].nborderpt; j++)
					{
						mask = pcontacts[i].idxborder[j][0];
						pcontacts[i].idxborder[j][0] = pcontacts[i].idxborder[j][1];
						pcontacts[i].idxborder[j][1] = mask;
					}
				}
				if (pcontacts[i].iUnprojMode)
				{
					pcontacts[i].pt = pcontacts[i].pt.GetRotated(gtest[0].centerOfRotation,
					                                             pcontacts[i].dir, -pcontacts[i].t);
				}
				else
				{
					pcontacts[i].pt -= pcontacts[i].dir * pcontacts[i].t;
				}
				pcontacts[i].dir.Flip();
				char id = pcontacts[i].id[0];
				pcontacts[i].id[0] = pcontacts[i].id[1];
				pcontacts[i].id[1] = id;
				int iprim = pcontacts[i].iPrim[0];
				pcontacts[i].iPrim[0] = pcontacts[i].iPrim[1];
				pcontacts[i].iPrim[1] = iprim;
				int ifeature = pcontacts[i].iFeature[0];
				pcontacts[i].iFeature[0] = pcontacts[i].iFeature[1];
				pcontacts[i].iFeature[1] = ifeature;
			}
		}

		// sort contacts in descending t order
		geom_contact tmpcontact;
		for (i = 0; i < nContacts - 1; i++)
		{
			for (jmax = i, j = i + 1; j < nContacts; j++)
			{
				mask = -isneg(pcontacts[jmax].t - pcontacts[j].t);
				jmax = (jmax & ~mask) | (j & mask);
			}
			if (jmax != i)
			{
				if (pcontacts[i].ptborder == &pcontacts[i].center)
				{
					pcontacts[i].ptborder = &pcontacts[jmax].center;
				}
				if (pcontacts[i].ptborder == &pcontacts[i].pt)
				{
					pcontacts[i].ptborder = &pcontacts[jmax].pt;
				}
				if (pcontacts[jmax].ptborder == &pcontacts[jmax].center)
				{
					pcontacts[jmax].ptborder = &pcontacts[i].center;
				}
				if (pcontacts[jmax].ptborder == &pcontacts[jmax].pt)
				{
					pcontacts[jmax].ptborder = &pcontacts[i].pt;
				}
				tmpcontact = pcontacts[i];
				pcontacts[i] = pcontacts[jmax];
				pcontacts[jmax] = tmpcontact;
			}
		}
	}
	else
	{
		gtest[0].sweepstep = pcontacts[0].vel = gtest[0].v.len();
		gtest[0].sweepdir = gtest[0].v / gtest[0].sweepstep;
		gtest[0].sweepstep *= pparams->time_interval;
		gtest[0].sweepdir_loc = gtest[0].sweepdir * gtest[0].R;
		gtest[0].sweepstep_loc = gtest[0].sweepstep * gtest[0].rscale;

		if (!PrepareForIntersectionTest(gtest + 0, (CGeometry*)pCollider, gtest + 1,
		                                pparams->bKeepPrevContacts) ||
		    !((CGeometry*)pCollider)
		         ->PrepareForIntersectionTest(gtest + 1, this, gtest + 0, pparams->bKeepPrevContacts))
		{
			return 0;
		}

		pcontacts[0].ptborder = &g_idata[iCaller].Contacts[0].pt;
		pcontacts[0].nborderpt = 1;
		pcontacts[0].parea = 0;
		pcontacts[0].dir = -gtest[0].sweepdir;
		pcontacts[0].t = gtest[0].sweepstep;

		gtest[0].pBVtree->GetNodeBV(pBV1, gtest[0].sweepdir_loc, gtest[0].sweepstep_loc, 0, iCaller);
		gtest[1].pBVtree->GetNodeBV(gtest[1].R_rel, gtest[1].offset_rel, gtest[1].scale_rel, pBV2, 0, iCaller);
		SweepTestBVs(gtest, pBV1, pBV2);
	}

	for (i = 0; i < nContacts; i++)
	{
		pcontacts[i].pt += offsetWorld;
		pcontacts[i].center += offsetWorld;
		if (pcontacts[i].ptborder != &pcontacts[i].pt && pcontacts[i].ptborder != &pcontacts[i].center)
		{
			for (j = 0; j < pcontacts[i].nborderpt; j++)
			{
				pcontacts[i].ptborder[j] += offsetWorld;
			}
		}
		if (pcontacts[i].parea)
		{
			for (j = 0; j < pcontacts[i].parea->npt; j++)
			{
				pcontacts[i].parea->pt[j] += offsetWorld;
			}
		}
	}

	CleanupAfterIntersectionTest(gtest + 0);
	((CGeometry*)pCollider)->CleanupAfterIntersectionTest(gtest + 1);

	lock.SetActive((pparams->bThreadSafe ^ 1) & (bActive | iszero(nContacts)));
	g_idata[iCaller].nTotContacts += nContacts;
	return nContacts;
}

int CGeometry::RegisterIntersection(primitives::primitive* pprim1, primitives::primitive* pprim2,
                                    geometry_under_test* pGTest1, geometry_under_test* pGTest2, prim_inters* pinters)
{
	unprojection_mode unproj;
	contact contact_cur;
	geom_contact* pres = pGTest1->contacts + *pGTest1->pnContacts;
	const int iCaller = pGTest1->iCaller;
	primitives::primitive* pprims[2] = {pprim1, pprim2};
	pres->ptborder = &pres->pt;
	pres->nborderpt = 1;

	if (pGTest1->pParams->iUnprojectionMode == 0)
	{
		// for linear unprojection, find point with maximum relative normal velocity
		unproj.imode = 0;
		unproj.dir = pGTest2->v + (pGTest2->w ^ pinters->pt[1] - pGTest2->centerOfMass) - pGTest1->v -
		             (pGTest1->w ^ pinters->pt[1] - pGTest1->centerOfMass);
		unproj.vel = unproj.dir.len();
		if (unproj.vel < pGTest1->pParams->vrel_min)
		{
			goto use_normal;
		}
		unproj.dir /= unproj.vel;
	}
	else if (pGTest1->pParams->iUnprojectionMode == 1)
	{
		unproj.imode = 1;
		unproj.dir = pGTest2->w - pGTest1->w;
		unproj.center = pGTest1->centerOfRotation.len2() > pGTest2->centerOfRotation.len2()
		                    ? pGTest1->centerOfRotation
		                    : pGTest2->centerOfRotation;
		unproj.vel = unproj.dir.len();
		unproj.dir /= unproj.vel;
	}
	unproj.tmax = pGTest1->pParams->time_interval * unproj.vel;

	if (!g_Unprojector.Check(&unproj, pGTest1->typeprim, pGTest2->typeprim, pprims[0], -1, pprims[1], -1,
	                         &contact_cur, pres->parea))
	{
		return 0;
	}

	if (contact_cur.t > pGTest1->pParams->time_interval * unproj.vel)
	{
	use_normal:
		unproj.imode = 0;
		unproj.dir = pinters->n.normalize();
		unproj.vel = 0;

		if (!g_Unprojector.Check(&unproj, pGTest1->typeprim, pGTest2->typeprim, pprims[0], -1, pprims[1], -1,
		                         &contact_cur, pres->parea))
		{
			return 0;
		}
	}

	pres->t = contact_cur.t;
	pres->pt = contact_cur.pt;
	pres->n = contact_cur.n;
	pres->dir = unproj.dir;
	pres->vel = unproj.vel;
	pres->iUnprojMode = unproj.imode;
	pres->id[0] = pinters->id[0];
	pres->id[1] = pinters->id[1];
	pres->iNode[0] = pinters->iNode[0];
	pres->iNode[1] = pinters->iNode[1];

	if (pres->parea->npt > 0)
	{
		g_idata[iCaller].nAreaPt += pres->parea->npt;
		g_idata[iCaller].nAreas++;
	}
	else
	{
		pres->parea = 0;
	}

	// allocate slots for the next intersection
	(*pGTest1->pnContacts)++;
	pres = pGTest1->contacts + *pGTest1->pnContacts;
	if (g_idata[iCaller].nAreas < sizeof(g_idata[iCaller].AreaBuf) / sizeof(g_idata[iCaller].AreaBuf[0]) &&
	    g_idata[iCaller].nAreaPt < sizeof(g_idata[iCaller].AreaPtBuf) / sizeof(g_idata[iCaller].AreaPtBuf[0]))
	{
		pres->parea = g_idata[iCaller].AreaBuf + g_idata[iCaller].nAreas;
		pres->parea->pt = g_idata[iCaller].AreaPtBuf + g_idata[iCaller].nAreaPt;
		pres->parea->piPrim[0] = g_idata[iCaller].AreaPrimBuf0 + g_idata[iCaller].nAreaPt;
		pres->parea->piFeature[0] = g_idata[iCaller].AreaFeatureBuf0 + g_idata[iCaller].nAreaPt;
		pres->parea->piPrim[1] = g_idata[iCaller].AreaPrimBuf1 + g_idata[iCaller].nAreaPt;
		pres->parea->piFeature[1] = g_idata[iCaller].AreaFeatureBuf1 + g_idata[iCaller].nAreaPt;
		pres->parea->npt = 0;
		pres->parea->nmaxpt = (sizeof(g_idata[iCaller].AreaPtBuf) / sizeof(g_idata[iCaller].AreaPtBuf[0])) -
		                      g_idata[iCaller].nAreaPt;
		pres->parea->minedge = min(pGTest1->minAreaEdge, pGTest2->minAreaEdge);
	}
	else
	{
		pres->parea = 0;
	}

	if (*pGTest1->pnContacts >= pGTest1->nMaxContacts)
	{
		pGTest1->bStopIntersection = 1;
	}

	return 0;
}

namespace {

struct radius_check_data
{
	CGeometry* pGeom;
	CBVTree* pBVtree;
	geom_world_data* pgwd;
	primitives::sphere sph;
	float rmin, rmax;
	float zscale;
	int** pGrid;
	int nRes;
	int nGrow;
	int iPass;
	int iCaller;
};

int RadiusCheckBVs(radius_check_data* prcd, BV* pBV)
{
	if (!g_idata[prcd->iCaller].Overlapper.Check(pBV->type, primitives::sphere::type, *pBV, &prcd->sph))
	{
		return 0;
	}

	if (prcd->pBVtree->SplitPriority(pBV) > 0)
	{
		BV *pBV_split1, *pBV_split2;
		prcd->pBVtree->GetNodeChildrenBVs(pBV, pBV_split1, pBV_split2, prcd->iCaller);
		int res = RadiusCheckBVs(prcd, pBV_split1) + RadiusCheckBVs(prcd, pBV_split2);
		prcd->pBVtree->ReleaseLastBVs(prcd->iCaller);
		return res;
	}
	else
	{
		int iStartPrim, nPrims;
		nPrims = prcd->pBVtree->GetNodeContentsIdx(pBV->iNode, iStartPrim);
		return prcd->pGeom->DrawToOcclusionCubemap(prcd->pgwd, iStartPrim, nPrims, prcd->iPass, prcd->pGrid,
		                                           prcd->nRes, prcd->rmin, prcd->rmax, prcd->zscale);
	}
}

} // unnamed namespace

float CGeometry::BuildOcclusionCubemap(geom_world_data* pgwd, int iMode, int* pGrid0[6], int* pGrid1[6], int nRes,
                                       float rmin, float rmax, int nGrow)
{
	radius_check_data rcd;
	float rscale = pgwd->scale == 1.0f ? 1.0f : 1.0f / pgwd->scale;
	int i, j, irmin;
	rcd.iCaller = GetCaller();
	ReadLock lock0(m_lockUpdate);
	WriteLock lock(g_idata[rcd.iCaller].lockIntersect);

	ResetGlobalPrimsBuffers(rcd.iCaller);
	rcd.pgwd = pgwd;
	rcd.sph.center = (-pgwd->offset * rscale) * pgwd->R;
	rcd.sph.r = rmax * rscale;
	rcd.rmin = rmin;
	rcd.rmax = rmax;
	rcd.zscale = 65535.0f / rmax;
	rcd.nRes = nRes;
	rcd.nGrow = nGrow;
	rcd.iPass = 0;
	if (iMode == 0)
	{
		rcd.pGrid = pGrid0;
	}
	else
	{
		rcd.pGrid = pGrid1;
		for (i = 0; i < 6; i++)
		{
			for (j = (nRes * nRes) - 1; j >= 0; j--)
			{
				pGrid1[i][j] = (1u << 31) - 1;
			}
		}
	}
	rcd.pGeom = this;
	rcd.pBVtree = GetBVTree();

	BV* pBV;
	rcd.pBVtree->GetNodeBV(pBV, 0, rcd.iCaller);

	if (RadiusCheckBVs(&rcd, pBV))
	{ // && iMode==0) {
		rcd.iPass = 1;
		ResetGlobalPrimsBuffers(rcd.iCaller);
		RadiusCheckBVs(&rcd, pBV);
	}

	irmin = physics_float2int(rmin * rcd.zscale);
	for (i = 0; i < 6; i++)
	{
		for (j = (nRes * nRes) - 1; j >= 0; j--) // if the highest bit is set, change cell z to irmin
		{
			rcd.pGrid[i][j] =
			    (irmin & rcd.pGrid[i][j] >> 31) | (rcd.pGrid[i][j] & ~(rcd.pGrid[i][j] >> 31));
		}
	}

	if (iMode)
	{
		int nCells, nOccludedCells;
		GrowAndCompareCubemaps(pGrid0, pGrid1, nRes, nGrow, nCells, nOccludedCells);
		return nCells > 0 ? (float)(nCells - nOccludedCells) / nCells : 0.0f;
	}
	return 0.0f;
}

int CGeometry::GetUnprojectionCandidates(int iop, const contact* pcontact, primitives::primitive*& pprim,
                                         int*& piFeature, geometry_under_test* pGTest)
{
	pGTest->nSurfaces = 0;
	pGTest->nEdges = 0;
	return 0;
}

void CGeometry::Lock(int bWrite)
{
	SpinLock(&m_lockUpdate, 0, 1 + ((WRITE_LOCK_VAL - 1) & -bWrite));
}

void CGeometry::Unlock(int bWrite)
{
	AtomicAdd(&m_lockUpdate, -1 - ((WRITE_LOCK_VAL - 1) & -bWrite));
}

float CGeometry::ComputeExtent(GeomQuery& geo, EGeomForm eForm)
{
	primitives::box box;
	GetBBox(&box);
	return BoxExtent(eForm, box.size);
}

void CGeometry::GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm)
{
	primitives::box box;
	GetBBox(&box);
	BoxRandomPos(ran, eForm, box.size);
	if (box.bOriented)
	{
		ran.vPos = box.Basis * ran.vPos;
		ran.vNorm = box.Basis * ran.vNorm;
	}
	ran.vPos += box.center;
}
