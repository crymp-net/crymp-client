#include "IntersectData.h"
#include "IntersectionChecker.h"
#include "OverlapChecker.h"
#include "RayGeom.h"
#include "UnprojectionChecker.h"
#include "Utils.h"
#include "VoxelGeom.h"

CVoxelGeom* CVoxelGeom::CreateVoxelGrid(primitives::grid3d* pgrid)
{
	int i, j, ipass, dummy, icell, ncells;
	Vec3i ic, iBBox[2];
	Vec3 c;
	float x;

	m_grid.step = pgrid->step;
	m_grid.stepr.Set(1 / m_grid.step.x, 1 / m_grid.step.y, 1 / m_grid.step.z);
	m_grid.size = pgrid->size;
	m_grid.stride.Set(1, m_grid.size.x, m_grid.size.x * m_grid.size.y);
	m_grid.origin = pgrid->origin;
	m_grid.Basis.SetIdentity();
	m_grid.bOriented = 0;
	m_grid.pVtx = m_pVertices;
	m_grid.pIndices = m_pIndices;
	m_grid.pNormals = m_pNormals;
	m_grid.pIds = m_pIds;
	m_grid.R.SetIdentity();
	m_grid.offset.zero();
	m_grid.scale = m_grid.rscale = 1;
	ncells = m_grid.size.GetVolume();
	memset(m_grid.pCellTris = new int[ncells + 1], 0, (ncells + 1) * sizeof(int));

	for (ipass = 0, m_grid.pTriBuf = &dummy; ipass < 2; ipass++)
	{
		for (i = m_nTris - 1; i >= 0; i--)
		{
			for (j = 0, c.zero(); j < 3; j++)
			{
				c += m_pVertices[m_pIndices[(i * 3) + j]] - m_grid.origin;
			}
			iBBox[0] = m_grid.size;
			iBBox[1].zero();
			c *= 1 / 3.0f;
			for (j = 0; j < 3; j++)
			{
				for (ic.x = 0; ic.x < 3; ic.x++)
				{
					x = (m_pVertices[m_pIndices[(i * 3) + j]][ic.x] - m_grid.origin[ic.x]) *
					    m_grid.stepr[ic.x];
					x += max(-0.001f,
					         min(0.001f, (c[ic.x] - m_pVertices[m_pIndices[(i * 3) + j]][ic.x]) *
					                         m_grid.stepr[ic.x]));
					icell = min(m_grid.size[ic.x] - 1, max(0, physics_float2int(x - 0.5f)));
					iBBox[0][ic.x] = min(iBBox[0][ic.x], icell);
					iBBox[1][ic.x] = max(iBBox[1][ic.x], icell);
				}
			}
			for (ic.z = iBBox[0].z; ic.z <= iBBox[1].z; ic.z++)
			{
				for (ic.y = iBBox[0].y; ic.y <= iBBox[1].y; ic.y++)
				{
					for (ic.x = iBBox[0].x; ic.x <= iBBox[1].x; ic.x++)
					{
						m_grid.pCellTris[icell = ic * m_grid.stride] -= ipass;
						m_grid.pTriBuf[m_grid.pCellTris[icell] & -ipass] = i;
						m_grid.pCellTris[icell] += ipass ^ 1;
					}
				}
			}
		}
		if (ipass == 0)
		{
			for (i = 1; i <= ncells; i++)
			{
				m_grid.pCellTris[i] += m_grid.pCellTris[i - 1];
			}
			m_grid.pTriBuf = new int[m_grid.pCellTris[ncells]];
		}
	}

	m_Tree.m_pgrid = &m_grid;
	m_Tree.Build(this);
	m_pTree = &m_Tree;
	m_minVtxDist = m_grid.step.x * 0.0001f;

	return this;
}

int CVoxelGeom::Intersect(IGeometry* pCollider, geom_world_data* pdata1, geom_world_data* pdata2,
                          intersection_params* pparams, geom_contact*& pcontacts)
{
	if (pCollider->GetType() == GEOM_RAY)
	{
		geometry_under_test GRay;
		bool bKeepPrevContacts;
		int bThreadSafe, bActive, iCaller;
		iCaller = GRay.iCaller = GetCaller();
		if (pparams)
		{
			bKeepPrevContacts = pparams->bKeepPrevContacts;
			bThreadSafe = pparams->bThreadSafe;
			bActive = 0;
			pparams->plock = &g_idata[iCaller].lockIntersect;
		}
		else
		{
			bKeepPrevContacts = false;
			bThreadSafe = 0;
			bActive = 1;
		}
		WriteLockCond lock(g_idata[iCaller].lockIntersect, bThreadSafe ^ 1);
		if (!pdata2)
		{
			GRay.R.SetIdentity();
			GRay.offset.zero();
			GRay.scale = 1.0f;
		}
		else
		{
			GRay.R = pdata2->R;
			GRay.offset = pdata2->offset;
			GRay.scale = pdata2->scale;
		}
		((CGeometry*)pCollider)->PrepareForIntersectionTest(&GRay, this, 0, bKeepPrevContacts);
		primitives::ray *pray = (primitives::ray*)GRay.primbuf, rayloc;
		int i, j, idxcell, itri, bHasInters = 0;
		float rscale, len;
		Vec3 dirn = ((CRayGeom*)pCollider)->m_dirn, origin, pthit;
		quotientf t[3];
		Vec3i istep, icell, itest;
		primitives::triangle atri;
		prim_inters inters, curinters;
		unprojection_mode unproj;
		contact contact_best;
		if (!bKeepPrevContacts)
		{
			g_idata[iCaller].nTotContacts = 0;
		}

		rscale = pdata1->scale == 1.0f ? 1 : 1 / pdata1->scale;
		rayloc.origin = ((pray->origin - pdata1->offset) * pdata1->R) * rscale;
		origin = rayloc.origin - m_grid.origin;
		rayloc.dir = (pray->dir * pdata1->R) * rscale;
		len = (pray->dir * dirn) * rscale;
		inters.minPtDist2 = sqr(m_minVtxDist);
		for (i = 0, j = 1; i < 3; i++)
		{
			icell[i] = physics_float2int((origin[i] * m_grid.stepr[i]) - 0.5f);
			j &= inrange(icell[i], -1, m_grid.size[i]);
			itest[i] = (istep[i] = sgnnz(rayloc.dir[i])) + 1 >> 1;
			t[i].y = fabs_tpl(rayloc.dir[i]);
		}
		if (!j)
		{
			for (i = 0; i < 3; i++)
			{
				icell[i] = (1 - istep[i] >> 1) * m_grid.size[i];
				t[i].x = (icell[i] * m_grid.step[i] - origin[i]) * istep[i];
			}
			i = idxmax3(t);
			pthit = origin + rayloc.dir * t[i].val();
			icell[i] += min(0, istep[i]);
			icell[inc_mod3[i]] =
			    min(m_grid.size[inc_mod3[i]] - 1,
			        max(0, physics_float2int((pthit[inc_mod3[i]] * m_grid.stepr[inc_mod3[i]]) - 0.5f)));
			icell[dec_mod3[i]] =
			    min(m_grid.size[dec_mod3[i]] - 1,
			        max(0, physics_float2int((pthit[dec_mod3[i]] * m_grid.stepr[dec_mod3[i]]) - 0.5f)));
		}
		inters.pt[0] = rayloc.origin + rayloc.dir * 1.1f;

		do
		{
			idxcell = icell * m_grid.stride;
			for (i = m_grid.pCellTris[idxcell]; i < m_grid.pCellTris[idxcell + 1]; i++)
			{
				for (j = 0; j < 3; j++)
				{
					atri.pt[j] = m_pVertices[m_pIndices[(m_grid.pTriBuf[i] * 3) + j]];
				}
				atri.n = m_pNormals[itri = m_grid.pTriBuf[i]];
				if (ray_tri_intersection(&rayloc, &atri, &curinters) &&
				    (curinters.pt[0] - rayloc.origin) * rayloc.dir <
				        (inters.pt[0] - rayloc.origin) * rayloc.dir)
				{
					bHasInters = 1;
					inters = curinters;
				}
			}
			if (bHasInters)
			{
				goto haveinters;
			}
			for (i = 0; i < 3; i++)
			{
				t[i].x = ((icell[i] + itest[i]) * m_grid.step[i] - origin[i]) * istep[i];
			}
			i = idxmin3(t);
			icell[i] += istep[i];
		}
		while (t[i] < len && inrange(icell[i], -1, m_grid.size[i]));
		return 0;

	haveinters:
		g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].ptborder =
		    &g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].center;
		g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].center =
		    (pdata1->R * inters.pt[0]) * pdata1->scale + pdata1->offset;
		g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].nborderpt = 1;
		g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].parea = 0;

		if (((CGeometry*)pCollider)->m_iCollPriority < m_iCollPriority || !pparams)
		{
			unproj.imode = -1;
		}
		else
		{
			if (unproj.imode = pparams->iUnprojectionMode)
			{
				if ((unproj.dir = pparams->axisOfRotation).len2() == 0)
				{
					unproj.dir =
					    g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].n ^ pray->dir;
					if (unproj.dir.len2() < 0.002)
					{
						unproj.dir.SetOrthogonal(pray->dir);
					}
					unproj.dir.normalize();
				}
				unproj.center = pparams->centerOfRotation;
				unproj.tmax = g_PI * 0.5;
			}
			else
			{
				unproj.dir = pdata2->v - pdata1->v;
				unproj.tmax = pparams->time_interval;
				if (fabsf(unproj.dir.len2() - 1.0f) > 0.001f)
				{
					unproj.dir.normalize();
				}
			}
			unproj.minPtDist = m_minVtxDist;
			for (i = 0; i < 3; i++)
			{
				atri.pt[i] = pdata1->R * atri.pt[i] * pdata1->scale + pdata1->offset;
			}
			atri.n = pdata1->R * atri.n;
		}

		if (unproj.imode < 0 || !g_Unprojector.Check(&unproj, primitives::triangle::type, primitives::ray::type,
		                                             &atri, -1, pray, -1, &contact_best))
		{
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].t =
			    ((pdata1->R * (inters.pt[0] - rayloc.origin)) * dirn) * pdata1->scale;
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].pt =
			    g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].center;
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].n = pdata1->R * inters.n.normalized();
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].dir.zero();
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].iUnprojMode = -1;
		}
		else
		{
			if (unproj.imode)
			{
				contact_best.t = atan2(contact_best.t, contact_best.taux);
			}
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].t = contact_best.t;
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].pt = contact_best.pt;
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].n = contact_best.n;
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].dir = unproj.dir;
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].iUnprojMode = unproj.imode;
		}
		g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].vel = 0;
		g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].id[0] = m_pIds ? m_pIds[itri] : -1;
		g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].id[1] = -1;
		pcontacts = g_idata[iCaller].Contacts + g_idata[iCaller].nTotContacts++;
		if (pparams)
		{
			pparams->pGlobalContacts = g_idata[iCaller].Contacts;
		}
		lock.SetActive((bThreadSafe ^ 1) & bActive);

		return 1;
	}

	return CTriMesh::Intersect(pCollider, pdata1, pdata2, pparams, pcontacts);
}

int CVoxelGeom::DrawToOcclusionCubemap(const geom_world_data* pgwd, int iStartPrim, int nPrims, int iPass,
                                       int* pGrid[6], int nRes, float rmin, float rmax, float zscale)
{
	int nTris = 0, i, j, icell;
	float rscale = pgwd->scale == 1.0f ? 1.0f : 1.0f / pgwd->scale;
	Vec3i iBBox[2], ic;
	primitives::triangle tri;
	primitives::sphere sph;
	sph.center = (-pgwd->offset * rscale) * pgwd->R;
	sph.r = rmax * rscale;
	for (i = 0; i < 3; i++)
	{
		iBBox[0][i] = max(
		    0, min(m_grid.size[i] - 1, physics_float2int(((sph.center[i] - sph.r) * m_grid.stepr[i]) - 0.5f)));
		iBBox[1][i] = max(
		    0, min(m_grid.size[i] - 1, physics_float2int(((sph.center[i] + sph.r) * m_grid.stepr[i]) + 0.5f)));
	}
	sph.center.zero();
	sph.r = rmax;

	for (ic.z = iBBox[0].x; ic.z < iBBox[1].z; ic.z++)
	{
		for (ic.y = iBBox[0].y; ic.y < iBBox[1].y; ic.y++)
		{
			for (ic.x = iBBox[0].x; ic.x < iBBox[1].x; ic.x++)
			{
				for (i = m_grid.pCellTris[icell = ic * m_grid.stride]; i < m_grid.pCellTris[icell + 1];
				     i++)
				{
					for (j = 0; j < 3; j++)
					{
						tri.pt[j] = pgwd->R *
						                m_pVertices[m_pIndices[(m_grid.pTriBuf[i] * 3) + j]] *
						                pgwd->scale +
						            pgwd->offset;
					}
					tri.n = pgwd->R * m_pNormals[m_grid.pTriBuf[i]];
					if (tri_sphere_overlap_check(&tri, &sph, nullptr))
					{
						RasterizePolygonIntoCubemap(tri.pt, 3, iPass, pGrid, nRes, rmin, rmax,
						                            zscale);
						nTris++;
					}
				}
			}
		}
	}

	return nTris;
}
