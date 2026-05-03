#include "Heightfield.h"
#include "IntersectData.h"
#include "IntersectionChecker.h"
#include "OverlapChecker.h"
#include "RayGeom.h"
#include "UnprojectionChecker.h"
#include "Utils.h"

namespace {

struct hf_cell_checker
{
	float dir2d_len, max_zcell, maxt;
	int idcontact, bNoCull;
	primitives::triangle hftri;
	primitives::ray hfray;
	primitives::heightfield* phf;
	prim_inters inters;
	vector2df org2d, dir2d;

	int check_cell(const vector2di& icell, int& ilastcell)
	{
		quotientf t((org2d + icell) * dir2d, dir2d_len * dir2d_len);
		if (t.x > maxt || !phf->inrange(icell.x, icell.y))
		{
			return 1;
		}
		float h[4], zlowest = (hfray.origin.z * t.y) + (hfray.dir.z * t.x) - max_zcell;
		int itype = phf->gettype(icell.x, icell.y);
		h[0] = phf->getheight(icell.x, icell.y);
		h[1] = phf->getheight(icell.x + 1, icell.y);
		h[2] = phf->getheight(icell.x, icell.y + 1);
		h[3] = phf->getheight(icell.x + 1, icell.y + 1);
		if (zlowest <= max(max(max(h[0], h[1]), h[2]), h[3]) * t.y && itype >= 0)
		{
			hftri.pt[0].x = hftri.pt[2].x = icell.x * phf->step.x;
			hftri.pt[1].x = hftri.pt[0].x + phf->step.x;
			hftri.pt[0].y = hftri.pt[1].y = icell.y * phf->step.y;
			hftri.pt[2].y = hftri.pt[0].y + phf->step.y;
			hftri.pt[0].z = h[0];
			hftri.pt[1].z = h[1];
			hftri.pt[2].z = h[2];
			hftri.n = hftri.pt[1] - hftri.pt[0] ^ hftri.pt[2] - hftri.pt[0];
			if (ray_tri_intersection(&hfray, &hftri, &inters) && hftri.n * hfray.dir < bNoCull)
			{
				idcontact = itype;
				return 1;
			}
			hftri.pt[0] = hftri.pt[2];
			hftri.pt[2].x += phf->step.x;
			hftri.pt[2].z = h[3];
			hftri.n = hftri.pt[1] - hftri.pt[0] ^ hftri.pt[2] - hftri.pt[0];
			if (ray_tri_intersection(&hfray, &hftri, &inters) && hftri.n * hfray.dir < bNoCull)
			{
				idcontact = itype;
				return 1;
			}
		}
		return 0;
	}
};

void CheckPtEdgeDist(const Vec3* pte, const Vec3& pt, Vec3& ptres, int& iFeature)
{
	Vec3 edge = pte[1] - pte[0], dp = pt - pte[0];
	float dp_edge = dp * edge, edge_len2 = edge.len2();
	if ((dp ^ edge).len2() < (pt - ptres).len2() * edge_len2 && dp_edge > 0 && (pt - pte[1]) * edge < 0)
	{
		ptres = pte[0] + edge * (dp_edge / edge_len2);
		iFeature = 0x20;
	}
}

void CheckPtTriDist(const primitives::triangle& tri, const Vec3& pt, Vec3& ptres, int& iFeature)
{
	float dist = (pt - tri.pt[0]) * tri.n;
	if (dist > 0 && sqr(dist) < (pt - ptres).len2() * tri.n.len2() &&
	    (tri.pt[1] - tri.pt[0] ^ pt - tri.pt[0]) * tri.n > 0 &&
	    (tri.pt[2] - tri.pt[1] ^ pt - tri.pt[1]) * tri.n > 0 &&
	    (tri.pt[0] - tri.pt[2] ^ pt - tri.pt[2]) * tri.n > 0)
	{
		Vec3 n = tri.n.normalized();
		ptres = pt - n * (n * (pt - tri.pt[0]));
		iFeature = 0x40;
	}
}

} // unnamed namespace

CHeightfield* CHeightfield::CreateHeightfield(primitives::heightfield* phf)
{
	m_hf = *phf;
	m_hf.Basis.SetIdentity();
	m_hf.origin.zero();
	m_hf.stepr.x = 1.0f / phf->step.x;
	m_hf.stepr.y = 1.0f / phf->step.y;
	int i = ((phf->typemask ^ phf->typemask - 1) + 1) >> 1;
	for (m_hf.typepower = 0; !(i & 1); i >>= 1, m_hf.typepower++)
		;

	m_minHeight = -1000;
	m_maxHeight = 1000;
	m_Tree.m_phf = &m_hf;
	m_Tree.Build(this);
	m_pTree = &m_Tree;

	m_pVertices = new Vec3[m_nVerticesAlloc = 32];
	m_pNormals = new Vec3[m_nTrisAlloc = 64];
	m_pIndices = new index_t[m_nTrisAlloc * 3];
	m_pIds = new char[m_nTrisAlloc];
	m_pTopology = new trinfo[m_nTrisAlloc];
	m_Tree.m_pUsedTriMap = new unsigned int[((m_nTrisAlloc - 1) >> 5) + 1];
	m_minVtxDist = (m_hf.step.x + m_hf.step.y) * 1E-3f;
	m_nVertices = m_nTris = 0;
	m_flags = 3;
	return this;
}

int CHeightfield::Intersect(IGeometry* pCollider, geom_world_data* pdata1, geom_world_data* pdata2,
                            intersection_params* pparams, geom_contact*& pcontacts)
{
	if (pCollider->GetType() == GEOM_RAY)
	{
		geometry_under_test GRay;
		bool bKeepPrevContacts;
		int bThreadSafe, bActive, iCaller = GetCaller();
		hf_cell_checker hcc;
		hcc.bNoCull = 2;
		GRay.iCaller = iCaller;
		if (pparams)
		{
			bKeepPrevContacts = pparams->bKeepPrevContacts;
			bThreadSafe = pparams->bThreadSafe;
			bActive = 0;
			if (pparams->iUnprojectionMode == 1 && pparams->axisOfRotation.len2() == 0)
			{
				hcc.bNoCull = 0;
			}
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
		primitives::ray* pray = (primitives::ray*)GRay.primbuf;
		float rscale;
		Vec3 dirn = GRay.R * ((CRayGeom*)pCollider)->m_dirn, origin, dir;
		unprojection_mode unproj;
		contact contact_best;
		hcc.phf = &m_hf;
		if (!bKeepPrevContacts)
		{
			g_idata[iCaller].nTotContacts = 0;
		}

		// transform ray to grid coordinates
		rscale = 1.0f / pdata1->scale;
		hcc.hfray.origin = origin = ((pray->origin - pdata1->offset) * pdata1->R) * rscale;
		hcc.hfray.dir = dir = (pray->dir * pdata1->R) * rscale;
		hcc.inters.minPtDist2 = sqr(m_minVtxDist);
		hcc.org2d.set(0.5f - (hcc.hfray.origin.x * m_hf.stepr.x), 0.5f - (hcc.hfray.origin.y * m_hf.stepr.y));
		hcc.dir2d.set(hcc.hfray.dir.x * m_hf.stepr.x, hcc.hfray.dir.y * m_hf.stepr.y);
		hcc.dir2d_len = len(hcc.dir2d);
		hcc.max_zcell = fabs_tpl(hcc.hfray.dir.z) * hcc.dir2d_len * (sqrt2 * 0.5f);
		hcc.maxt = (hcc.dir2d_len * (hcc.dir2d_len + sqrt2)) + 0.0001f;
		hcc.idcontact = -2;

		if (DrawRayOnGrid(&m_hf, origin, dir, hcc) && hcc.idcontact != -2)
		{
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].ptborder =
			    &g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].center;
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].center =
			    (pdata1->R * hcc.inters.pt[0]) * pdata1->scale + pdata1->offset;
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].nborderpt = 1;
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].parea = 0;

			if (((CGeometry*)pCollider)->m_iCollPriority < m_iCollPriority || !pparams)
			{
				unproj.imode = -1;
			}
			else
			{
				unproj.imode = pparams->iUnprojectionMode;
				if (unproj.imode)
				{
					if ((unproj.dir = pparams->axisOfRotation).len2() == 0)
					{
						unproj.dir = pdata1->R * hcc.inters.n ^ pray->dir;
						// if (unproj.dir.len2()<0.002) unproj.dir = aray.dir.orthogonal();
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
				for (int i = 0; i < 3; i++)
				{
					hcc.hftri.pt[i] = pdata1->R * hcc.hftri.pt[i] * pdata1->scale + pdata1->offset;
				}
				hcc.hftri.n = pdata1->R * hcc.hftri.n.normalized();
			}

			if (unproj.imode < 0 ||
			    !g_Unprojector.Check(&unproj, primitives::triangle::type, primitives::ray::type, &hcc.hftri,
			                         -1, pray, -1, &contact_best))
			{
				g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].t =
				    ((pdata1->R * (hcc.inters.pt[0] - hcc.hfray.origin)) * dirn) * pdata1->scale;
				g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].pt =
				    g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].center;
				g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].n =
				    pdata1->R * hcc.inters.n.normalized();
				g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].dir.zero();
				g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].iUnprojMode = -1;
				g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].iFeature[0] = 0x40;
				g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].iFeature[1] = 0x20;
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
				g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].iFeature[0] =
				    contact_best.iFeature[0];
				g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].iFeature[1] =
				    contact_best.iFeature[1];
			}
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].vel = 0;
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].id[0] = hcc.idcontact;
			g_idata[iCaller].Contacts[g_idata[iCaller].nTotContacts].id[1] = -1;
			pcontacts = g_idata[iCaller].Contacts + g_idata[iCaller].nTotContacts++;
			if (pparams)
			{
				pparams->pGlobalContacts = g_idata[iCaller].Contacts;
			}
			lock.SetActive((bThreadSafe ^ 1) & bActive);
			return 1;
		}
		return 0;
	}
	return CTriMesh::Intersect(pCollider, pdata1, pdata2, pparams, pcontacts);
}

int CHeightfield::PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
                                             geometry_under_test* pGTestColl, bool bKeepPrevContacts)
{
	primitives::box abox, aboxext, *pbox;
	pCollider->GetBBox(&abox);
	if (pGTestColl->sweepstep > 0)
	{
		ExtrudeBox(&abox, pGTestColl->sweepdir_loc, pGTestColl->sweepstep_loc, &aboxext);
		pbox = &aboxext;
	}
	else
	{
		pbox = &abox;
	}
	int ix, iy, sx, sy, i, j, itri, icol, irow;
	float curx, cury, maxh, minz;
	index_t* pIdx;
	trinfo* pTop;
	Vec3* pVtx;

	project_box_on_grid(pbox, &m_hf, pGTest, ix, iy, sx, sy, minz);
	if (((sx - 1) | (sy - 1) | (1000000 - (sx * sy))) < 0)
	{
		return 0;
	}
	assert(sx >= 0);
	assert(sy >= 0);
	for (i = ix, maxh = m_hf.getheight(ix, iy); i <= ix + sx; i++)
	{
		for (j = iy; j <= iy + sy; j++)
		{
			maxh = max(maxh, m_hf.getheight(i, j));
		}
	}
	if (minz > maxh)
	{
		return 0;
	}
	m_Tree.m_PatchStart.set(ix, iy);
	m_Tree.m_PatchSize.set(sx, sy);

	m_nVertices = (sx + 1) * (sy + 1);
	m_nTris = sx * sy * 2;
	m_nMaxVertexValency = 6;

	if (m_nVerticesAlloc < m_nVertices)
	{
		delete[] m_pVertices.data;
		m_pVertices = new Vec3[m_nVerticesAlloc = (m_nVertices - 1 & ~15) + 16];
	}
	if (m_nTrisAlloc < m_nTris)
	{
		delete[] m_pIndices;
		delete[] m_pNormals;
		delete[] m_pIds;
		delete[] m_pTopology;
		delete[] m_Tree.m_pUsedTriMap;
		m_pNormals = new Vec3[m_nTrisAlloc = (m_nTris - 1 & ~15) + 16];
		m_pIndices = new index_t[m_nTrisAlloc * 3];
		m_pIds = new char[m_nTrisAlloc];
		m_pTopology = new trinfo[m_nTrisAlloc];
		m_Tree.m_pUsedTriMap = new unsigned int[((m_nTrisAlloc - 1) >> 5) + 1];
	}

	m_Tree.m_minHeight = m_Tree.m_maxHeight = m_hf.getheight(ix, iy);
	for (j = 0, pVtx = m_pVertices.data; j <= sy; j++)
	{
		curx = m_hf.step.x * ix;
		cury = m_hf.step.y * (iy + j);
		for (i = 0; i <= sx; i++, pVtx++, curx += m_hf.step.x)
		{
			pVtx->Set(curx, cury, m_hf.getheight(ix + i, iy + j));
			m_Tree.m_minHeight = min(m_Tree.m_minHeight, pVtx->z);
			m_Tree.m_maxHeight = max(m_Tree.m_maxHeight, pVtx->z);
		}
	}

	for (irow = i = itri = 0, pIdx = m_pIndices, pTop = m_pTopology; irow < sy; irow++, i++)
	{
		for (icol = 0; icol < sx; icol++, itri += 2, i++)
		{
			*pIdx++ = i;
			*pIdx++ = i + 1;
			*pIdx++ = i + sx + 1;
			m_pNormals[itri] = (m_pVertices[pIdx[1 - 3]] - m_pVertices[pIdx[0 - 3]] ^
			                    m_pVertices[pIdx[2 - 3]] - m_pVertices[pIdx[0 - 3]])
			                       .normalized();
			*pIdx++ = i + sx + 1;
			*pIdx++ = i + 1;
			*pIdx++ = i + sx + 2;
			m_pNormals[itri + 1] = (m_pVertices[pIdx[1 - 3]] - m_pVertices[pIdx[0 - 3]] ^
			                        m_pVertices[pIdx[2 - 3]] - m_pVertices[pIdx[0 - 3]])
			                           .normalized();
			pTop->ibuddy[0] = (itri - (sx * 2) + 1) | (irow - 1) >> 31;
			pTop->ibuddy[1] = itri + 1;
			pTop++->ibuddy[2] = (itri - 1) | (icol - 1) >> 31;
			pTop->ibuddy[0] = itri;
			pTop->ibuddy[1] = (itri + 2) | (sx - 2 - icol) >> 31;
			pTop++->ibuddy[2] = (itri + (sx * 2)) | (sy - 2 - irow) >> 31;
			m_pIds[itri] = m_pIds[itri + 1] = m_hf.gettype(ix + icol, iy + irow);
		}
	}
	for (i = (m_nTris - 1) >> 5; i >= 0; i--)
	{
		m_Tree.m_pUsedTriMap[i] = 0;
	}

	for (itri = 0; itri < m_nTris; itri++)
	{
		if (m_pIds[itri] == (char)-1)
		{
			for (j = 0; j < 3; j++)
			{
				if ((i = m_pTopology[itri].ibuddy[0]) >= 0) // remove connectivity for 'hole' triangles
				{
					m_pTopology[i].ibuddy[GetEdgeByBuddy(i, itri)] = -1;
				}
			}
		}
	}

	return CTriMesh::PrepareForIntersectionTest(pGTest, pCollider, pGTestColl, bKeepPrevContacts);
}

int CHeightfield::FindClosestPoint(geom_world_data* pgwd, int& iPrim, int& iFeature, const Vec3& ptdst0,
                                   const Vec3& ptdst1, Vec3* ptres, int nMaxIters)
{
	primitives::triangle tri;
	float h[4];
	Vec3 pt, ptres_loc;
	int ix, iy, sx, sy;
	ptres[1] = ptdst0;

	pt = ((ptdst0 - pgwd->offset) * pgwd->R);
	if (pgwd->scale != 1.0f)
	{
		pt /= pgwd->scale;
	}
	ix = physics_float2int((pt.x * m_hf.stepr.x) - 0.5f);
	iy = physics_float2int((pt.y * m_hf.stepr.y) - 0.5f);
	if ((unsigned int)ix > (unsigned int)m_hf.size.x - 2 || (unsigned int)iy > (unsigned int)m_hf.size.y - 2 ||
	    m_hf.gettype(ix, iy) < 0)
	{
		ptres[0].Set(1E10, 1E10, 1E10);
		return -1;
	}

	h[0] = m_hf.getheight(ix, iy);
	h[1] = m_hf.getheight(ix + 1, iy);
	h[2] = m_hf.getheight(ix, iy + 1);
	h[3] = m_hf.getheight(ix + 1, iy + 1);

	sx = isneg((m_hf.step.x * ix) + 0.5f - pt.x);
	sy = isneg((m_hf.step.y * iy) + 0.5f - pt.y);
	ptres_loc.Set((ix + sx) * m_hf.step.x, (iy + sy) * m_hf.step.y, h[sx + (sy * 2)]);
	iFeature = 0;

	tri.pt[0].x = tri.pt[2].x = ix * m_hf.step.x;
	tri.pt[1].x = tri.pt[0].x + m_hf.step.x;
	tri.pt[0].y = tri.pt[1].y = iy * m_hf.step.y;
	tri.pt[2].y = tri.pt[0].y + m_hf.step.y;
	tri.pt[0].z = h[0];
	tri.pt[1].z = h[1];
	tri.pt[2].z = h[2];
	tri.n = tri.pt[1] - tri.pt[0] ^ tri.pt[2] - tri.pt[0];
	CheckPtTriDist(tri, pt, ptres_loc, iFeature);
	tri.pt[0] = tri.pt[2];
	tri.pt[2].x += m_hf.step.x;
	tri.pt[2].z = h[3];
	tri.n = tri.pt[1] - tri.pt[0] ^ tri.pt[2] - tri.pt[0];
	CheckPtTriDist(tri, pt, ptres_loc, iFeature);

	tri.pt[1].x = tri.pt[0].x = (ix + sx) * m_hf.step.x;
	tri.pt[1].y = (tri.pt[0].y = iy * m_hf.step.y) + m_hf.step.y;
	tri.pt[0].z = h[sx];
	tri.pt[1].z = h[sx + 2];
	CheckPtEdgeDist(tri.pt, pt, ptres_loc, iFeature);

	tri.pt[1].y = tri.pt[0].y = (iy + sy) * m_hf.step.y;
	tri.pt[1].x = (tri.pt[0].x = ix * m_hf.step.x) + m_hf.step.x;
	tri.pt[0].z = h[sy * 2];
	tri.pt[1].z = h[(sy * 2) + 1];
	CheckPtEdgeDist(tri.pt, pt, ptres_loc, iFeature);

	tri.pt[0].Set((ix + 1) * m_hf.step.x, iy * m_hf.step.y, h[1]);
	tri.pt[1].Set(ix * m_hf.step.x, (iy + 1) * m_hf.step.y, h[2]);
	CheckPtEdgeDist(tri.pt, pt, ptres_loc, iFeature);

	iPrim = 0;
	ptres[0] = pgwd->R * ptres_loc * pgwd->scale + pgwd->offset;
	return 1;
}

void CHeightfield::CalcVolumetricPressure(geom_world_data* gwd, const Vec3& epicenter, float k, float rmin,
                                          const Vec3& centerOfMass, Vec3& P, Vec3& L)
{
}

int CHeightfield::DrawToOcclusionCubemap(const geom_world_data* pgwd, int iStartPrim, int nPrims, int iPass,
                                         int* pGrid[6], int nRes, float rmin, float rmax, float zscale)
{
	int nTris, ix, iy;
	float rscale = pgwd->scale == 1.0f ? 1.0f : 1.0f / pgwd->scale, h[4];
	vector2di irect[2];
	primitives::triangle tri;
	primitives::sphere sph;
	sph.center = (-pgwd->offset * rscale) * pgwd->R;
	sph.r = rmax * rscale;
	irect[0].x = max(0, min(m_hf.size.x, physics_float2int(((sph.center.x - sph.r) * m_hf.stepr.x) - 0.5f)));
	irect[0].y = max(0, min(m_hf.size.y, physics_float2int(((sph.center.y - sph.r) * m_hf.stepr.y) - 0.5f)));
	irect[1].x = max(0, min(m_hf.size.x, physics_float2int(((sph.center.x + sph.r) * m_hf.stepr.x) + 0.5f)));
	irect[1].y = max(0, min(m_hf.size.y, physics_float2int(((sph.center.y + sph.r) * m_hf.stepr.y) + 0.5f)));
	sph.center.zero();
	sph.r = rmax;

	for (ix = irect[0].x, nTris = 0; ix < irect[1].x; ix++)
	{
		for (iy = irect[0].y; iy < irect[1].y; iy++)
		{
			if (m_hf.gettype(ix, iy) >= 0)
			{
				h[0] = m_hf.getheight(ix, iy);
				h[1] = m_hf.getheight(ix + 1, iy);
				h[2] = m_hf.getheight(ix, iy + 1);
				h[3] = m_hf.getheight(ix + 1, iy + 1);

				tri.pt[0].x = tri.pt[2].x = ix * m_hf.step.x;
				tri.pt[1].x = tri.pt[0].x + m_hf.step.x;
				tri.pt[0].y = tri.pt[1].y = iy * m_hf.step.y;
				tri.pt[2].y = tri.pt[0].y + m_hf.step.y;
				tri.pt[0].z = h[0];
				tri.pt[1].z = h[1];
				tri.pt[2].z = h[2];
				tri.pt[0] = pgwd->R * tri.pt[0] * pgwd->scale + pgwd->offset;
				tri.pt[1] = pgwd->R * tri.pt[1] * pgwd->scale + pgwd->offset;
				tri.pt[2] = pgwd->R * tri.pt[2] * pgwd->scale + pgwd->offset;
				tri.n = tri.pt[1] - tri.pt[0] ^ tri.pt[2] - tri.pt[0];
				if (tri_sphere_overlap_check(&tri, &sph, nullptr))
				{
					RasterizePolygonIntoCubemap(tri.pt, 3, iPass, pGrid, nRes, rmin, rmax, zscale);
					nTris++;
				}

				tri.pt[0] = tri.pt[2];
				tri.pt[2] += pgwd->R * Vec3(pgwd->scale * m_hf.step.x, 0, 0);
				tri.pt[2] += pgwd->R * Vec3(0, 0, pgwd->scale * (h[3] - h[2]));
				tri.n = tri.pt[1] - tri.pt[0] ^ tri.pt[2] - tri.pt[0];
				if (tri_sphere_overlap_check(&tri, &sph, nullptr))
				{
					RasterizePolygonIntoCubemap(tri.pt, 3, iPass, pGrid, nRes, rmin, rmax, zscale);
					nTris++;
				}
			}
		}
	}

	return nTris;
}
