#include "BVTree.h"
#include "IntersectData.h"
#include "IntersectionChecker.h"
#include "Primitive.h"
#include "UnprojectionChecker.h"

int CPrimitive::Intersect(IGeometry* _pCollider, geom_world_data* pdata1, geom_world_data* pdata2,
                          intersection_params* pparams, geom_contact*& pcontacts)
{
	CGeometry* pCollider = (CGeometry*)_pCollider;
	if (!pCollider->IsAPrimitive())
	{
		return CGeometry::Intersect(pCollider, pdata1, pdata2, pparams, pcontacts);
	}

	// FUNCTION_PROFILER( GetISystem(),PROFILE_PHYSICS );

	static geom_world_data defgwd;
	static intersection_params defip;
	pdata1 = (geom_world_data*)((intptr_t)pdata1 | (-iszero((intptr_t)pdata1) & (intptr_t)&defgwd));
	pdata2 = (geom_world_data*)((intptr_t)pdata2 | (-iszero((intptr_t)pdata2) & (intptr_t)&defgwd));
	// pparams = (intersection_params*)((intptr_t)pparams | -iszero((intptr_t)pparams) & (intptr_t)&defip);
	int bActive;
	if (pparams)
	{
		bActive = 0;
	}
	else
	{
		pparams = &defip;
		bActive = 1;
	}
	int iCaller = GetCaller();
	WriteLockCond lock(g_idata[iCaller].lockIntersect, pparams->bThreadSafe ^ 1);
	pparams->plock = &g_idata[iCaller].lockIntersect;

	if (!pparams->bKeepPrevContacts)
	{
		g_idata[iCaller].nAreas = g_idata[iCaller].nAreaPt = g_idata[iCaller].nTotContacts =
		    g_idata[iCaller].BrdPtBufPos = 0;
	}
	if (max(g_idata[iCaller].BrdPtBufPos + 8 -
	            (int)(sizeof(g_idata[iCaller].BrdPtBuf) / sizeof(g_idata[iCaller].BrdPtBuf[0])),
	        g_idata[iCaller].nTotContacts -
	            (int)(sizeof(g_idata[iCaller].Contacts) / sizeof(g_idata[iCaller].Contacts[0]))) >= 0)
	{
		return 0;
	}
	pcontacts = g_idata[iCaller].Contacts + g_idata[iCaller].nTotContacts;
	pcontacts->ptborder = g_idata[iCaller].BrdPtBuf + g_idata[iCaller].BrdPtBufPos;
	pcontacts->nborderpt = 0;
	pparams->pGlobalContacts = g_idata[iCaller].Contacts;

	BV *pBV1, *pBV2;
	prim_inters inters;
	Vec3 sweepdir;
	float sweepstep = 0;
	ResetGlobalPrimsBuffers(iCaller);
	g_idata[iCaller].Overlapper.Init();
	if (!pparams->bSweepTest)
	{
		GetBVTree()->GetNodeBV(pdata1->R, pdata1->offset, pdata1->scale, pBV1, 0, iCaller);
	}
	else
	{
		sweepstep = pdata1->v.len();
		sweepdir = pdata1->v / sweepstep;
		sweepstep *= pparams->time_interval;
		GetBVTree()->GetNodeBV(pdata1->R, pdata1->offset, pdata1->scale, pBV1, sweepdir, sweepstep, 0, iCaller);
	}
	pCollider->GetBVTree()->GetNodeBV(pdata2->R, pdata2->offset, pdata2->scale, pBV2, 0, iCaller);
	if (!g_idata[iCaller].Overlapper.Check(pBV1->type, pBV2->type, *pBV1, *pBV2))
	{
		return 0;
	}

	// get primitives in world space
	int i, itype[2], bUnprojected = 0;
	primitives::primitive* pprim[2];
	pdata1->offset += sweepdir * sweepstep;
	itype[0] = PreparePrimitive(pdata1, pprim[0], iCaller);
	itype[1] = pCollider->PreparePrimitive(pdata2, pprim[1], iCaller);
	pdata1->offset -= sweepdir * sweepstep;
	inters.n.zero();

	if (pCollider->m_iCollPriority == 0)
	{ // probably the other geometry is a ray
		if (!g_Intersector.Check(itype[0], itype[1], pprim[0], pprim[1], &inters))
		{
			return 0;
		}
		geometry_under_test gtest;
		gtest.contacts = g_idata[iCaller].Contacts + g_idata[iCaller].nTotContacts;
		gtest.pnContacts = &g_idata[iCaller].nTotContacts;
		gtest.pParams = pparams;
		gtest.R = pdata2->R;
		gtest.offset = pdata2->offset;
		gtest.scale = pdata2->scale;
		pCollider->RegisterIntersection(pprim[0], pprim[1], &gtest, 0, &inters);
		pcontacts->n.Flip();
		lock.SetActive((pparams->bThreadSafe ^ 1) & bActive);
		return 1;
	}

	unprojection_mode unproj;
	contact contact_best;
	contact_best.t = 0;
	geom_contact_area* parea;
	unproj.minPtDist = min(m_minVtxDist, pCollider->m_minVtxDist);

	if (!pparams->bNoAreaContacts &&
	    g_idata[iCaller].nAreas < sizeof(g_idata[iCaller].AreaBuf) / sizeof(g_idata[iCaller].AreaBuf[0]) &&
	    g_idata[iCaller].nAreaPt < sizeof(g_idata[iCaller].AreaPtBuf) / sizeof(g_idata[iCaller].AreaPtBuf[0]))
	{
		parea = g_idata[iCaller].AreaBuf + g_idata[iCaller].nAreas;
		parea->pt = g_idata[iCaller].AreaPtBuf + g_idata[iCaller].nAreaPt;
		parea->piPrim[0] = g_idata[iCaller].AreaPrimBuf0 + g_idata[iCaller].nAreaPt;
		parea->piFeature[0] = g_idata[iCaller].AreaFeatureBuf0 + g_idata[iCaller].nAreaPt;
		parea->piPrim[1] = g_idata[iCaller].AreaPrimBuf1 + g_idata[iCaller].nAreaPt;
		parea->piFeature[1] = g_idata[iCaller].AreaFeatureBuf1 + g_idata[iCaller].nAreaPt;
		parea->npt = 0;
		parea->nmaxpt = (sizeof(g_idata[iCaller].AreaPtBuf) / sizeof(g_idata[iCaller].AreaPtBuf[0])) -
		                g_idata[iCaller].nAreaPt;
		parea->minedge = 0;
		parea->n1.zero();
	}
	else
	{
		parea = 0;
	}

	if (!pparams->bSweepTest)
	{
		Vec3 ptm;
		if (!pparams->bNoIntersection)
		{
			if (g_Intersector.CheckExists(itype[0], itype[1]))
			{
				inters.ptborder = pcontacts->ptborder;
				inters.nborderpt = 0;
				if (!g_Intersector.Check(itype[0], itype[1], pprim[0], pprim[1], &inters))
				{
					return 0;
				}
				pcontacts->nborderpt = inters.nborderpt;
				pcontacts->center.zero();
				if (inters.nborderpt > 0)
				{
					ptm = inters.ptborder[0];
					for (i = 0; i < inters.nborderpt; i++)
					{
						pcontacts->center += pcontacts->ptborder[i];
					}
					pcontacts->center /= inters.nborderpt;
				}
			}
		}
		else if (!g_idata[iCaller].Overlapper.Check(itype[0], itype[1], pprim[0], pprim[1]))
		{
			return 0;
		}

		if (pparams->iUnprojectionMode == 0)
		{
			if (pparams->vrel_min < 1E9f)
			{
				Vec3 vrel;
				if (!pcontacts->nborderpt && pdata1->w.len2() + pdata2->w.len2() > 0)
				{
					Vec3 center[2], pt[3];
					primitives::box bbox;
					int iPrim, iFeature; // dummy parameters
					if (pBV1->type == primitives::box::type)
					{
						center[0] = ((primitives::box*)(primitives::primitive*)*pBV1)->center;
					}
					else
					{
						GetBBox(&bbox);
						center[0] = pdata1->R * bbox.center + pdata1->offset;
					}
					if (pBV1->type == primitives::box::type)
					{
						center[1] = ((primitives::box*)(primitives::primitive*)*pBV2)->center;
					}
					else
					{
						pCollider->GetBBox(&bbox);
						center[1] = pdata2->R * bbox.center + pdata2->offset;
					}
					FindClosestPoint(pdata1, iPrim, iFeature, center[1], center[1], pt + 0);
					pCollider->FindClosestPoint(pdata2, iPrim, iFeature, center[0], center[0],
					                            pt + 1);
					if (pCollider->PointInsideStatus(
						((pt[0] - pdata2->offset) * pdata2->R) *
						(pdata2->scale == 1.0f ? 1.0f : 1.0f / pdata2->scale)))
					{
						ptm = pt[0];
					}
					else if (PointInsideStatus(
						     ((pt[1] - pdata1->offset) * pdata1->R) *
						     (pdata1->scale == 1.0f ? 1.0f : 1.0f / pdata1->scale)))
					{
						ptm = pt[1];
					}
					else
					{
						ptm = (pt[0] + pt[1]) * 0.5f;
					}
				}
				vrel = pdata1->v + (pdata1->w ^ ptm - pdata1->centerOfMass) - pdata2->v -
				       (pdata2->w ^ ptm - pdata2->centerOfMass);
				if (vrel.len2() == 0 && itype[1] == primitives::ray::type)
				{
					Vec3 raydir = ((primitives::ray*)pprim[1])->dir;
					vrel = (raydir ^ (inters.n ^ raydir)).normalized();
				}
				if (vrel.len2() > sqr(pparams->vrel_min))
				{
					unproj.imode = 0; // unproject along vrel
					(unproj.dir = -vrel) /= (unproj.vel = vrel.len());
					unproj.tmax = pparams->time_interval * unproj.vel;
					bUnprojected = g_Unprojector.Check(&unproj, itype[0], itype[1], pprim[0], -1,
					                                   pprim[1], -1, &contact_best, parea);
					bUnprojected &= isneg(contact_best.t - unproj.tmax);
				}
			}
		}
		else
		{
			unproj.imode = 1;
			unproj.center = pparams->centerOfRotation;
			if (pparams->axisOfRotation.len2() == 0)
			{
				unproj.dir = inters.n ^ inters.pt[0] - unproj.center;
				if (unproj.dir.len2() < 1E-6f)
				{
					unproj.dir = inters.n.GetOrthogonal();
				}
				unproj.dir.normalize();
			}
			else
			{
				unproj.dir = pparams->axisOfRotation;
			}
			bUnprojected = g_Unprojector.Check(&unproj, itype[0], itype[1], pprim[0], -1, pprim[1], -1,
			                                   &contact_best, parea);
			if (bUnprojected)
			{
				contact_best.t = atan2(contact_best.t, contact_best.taux);
			}
		}

		if (!bUnprojected)
		{
			unproj.imode = 0;
			unproj.dir.zero(); // zero requested direction - means minimum direction will be found
			unproj.vel = 0;
			unproj.tmax = pparams->maxUnproj;
			bUnprojected = g_Unprojector.Check(&unproj, itype[0], itype[1], pprim[0], -1, pprim[1], -1,
			                                   &contact_best, parea);
		}
	}
	else
	{
		unproj.imode = 0;
		unproj.dir = -sweepdir;
		unproj.tmax = unproj.vel = sweepstep;
		unproj.bCheckContact = 1;
		contact_best.t = 0;
		bUnprojected =
		    g_Unprojector.Check(&unproj, itype[0], itype[1], pprim[0], -1, pprim[1], -1, &contact_best, parea);
		bUnprojected &= isneg(contact_best.t - unproj.tmax);
		if (bUnprojected && contact_best.n * unproj.dir > 0)
		{
			// if we hit something with the back side, 1st primitive probably just passed through the 2nd
			// one, so move a little deeper than the contact and unproject again (some primitives check for
			// minimal separating unprojection, and some - for maximum contacting; the former can cause
			// this)
			real t = contact_best.t;
			primitives::box bbox;
			pCollider->GetBBox(&bbox);
			Vec3 dirloc, dirsz;
			dirloc = bbox.Basis * (sweepdir * pdata2->R);
			dirsz(fabs_tpl(dirloc.x) * bbox.size.y * bbox.size.z,
			      fabs_tpl(dirloc.y) * bbox.size.x * bbox.size.z,
			      fabs_tpl(dirloc.z) * bbox.size.x * bbox.size.y);
			i = idxmax3((float*)&dirsz);
			t += bbox.size[i] / fabs_tpl(dirloc[i]) * 0.1f;
			unproj.tmax = sweepstep - t;
			pdata1->offset += sweepdir * unproj.tmax;
			itype[0] = PreparePrimitive(pdata1, pprim[0], iCaller);
			pdata1->offset -= sweepdir * unproj.tmax;
			bUnprojected = g_Unprojector.Check(&unproj, itype[0], itype[1], pprim[0], -1, pprim[1], -1,
			                                   &contact_best, parea);
			bUnprojected &= isneg(contact_best.t - unproj.tmax) & isneg(contact_best.n * unproj.dir);
			contact_best.t += t;
			unproj.tmax = sweepstep;
		}
	}

	if (bUnprojected)
	{
		pcontacts->t = contact_best.t;
		if (pparams->bSweepTest)
		{
			pcontacts->t = unproj.tmax - pcontacts->t;
		}
		pcontacts->pt = contact_best.pt;
		pcontacts->n = contact_best.n.normalized();
		pcontacts->dir = unproj.dir;
		pcontacts->iUnprojMode = unproj.imode;
		pcontacts->vel = unproj.vel;
		pcontacts->id[0] = pcontacts->id[1] = -1;
		pcontacts->iPrim[0] = pcontacts->iPrim[1] = 0;
		pcontacts->iFeature[0] = contact_best.iFeature[0];
		pcontacts->iFeature[1] = contact_best.iFeature[1];
		pcontacts->iNode[0] = pcontacts->iNode[1] = 0;
		if (!parea || parea->npt == 0)
		{
			pcontacts->parea = 0;
		}
		else
		{
			pcontacts->parea = parea;
			g_idata[iCaller].nAreas++;
			g_idata[iCaller].nAreaPt += parea->npt;
			if (pcontacts->nborderpt < parea->npt &&
			    (pcontacts->nborderpt == 0 || (iszero(itype[0] - primitives::cylinder::type) |
			                                   iszero(itype[1] - primitives::cylinder::type))))
			{
				pcontacts->ptborder = parea->pt;
				pcontacts->nborderpt = parea->npt;
				for (i = 0, pcontacts->center.zero(); i < parea->npt; i++)
				{
					pcontacts->center += parea->pt[i];
				}
				pcontacts->center /= parea->npt;
			}
		}
		if (pcontacts->nborderpt == 0)
		{
			pcontacts->ptborder[0] = pcontacts->center = pcontacts->pt;
			pcontacts->nborderpt = 1;
		}
		pcontacts->bBorderConsecutive = false;
		g_idata[iCaller].nTotContacts++;
		g_idata[iCaller].BrdPtBufPos += pcontacts->nborderpt;
	}
	lock.SetActive((pparams->bThreadSafe ^ 1) & (bActive | (bUnprojected ^ 1)));

	return bUnprojected;
}
