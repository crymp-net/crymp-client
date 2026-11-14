#include "OverlapChecker.h"
#include "Quotient.h"
#include "Utils.h"

namespace {

quotientf tri_sphere_dist2(const primitives::triangle* ptri, const primitives::sphere* psph, int& bFace)
{
	int i, bInside[2];
	float rvtx[3], elen2[2], denom;
	Vec3 edge[2], dp;
	bFace = 0;

	rvtx[0] = (ptri->pt[0] - psph->center).len2();
	rvtx[1] = (ptri->pt[1] - psph->center).len2();
	rvtx[2] = (ptri->pt[2] - psph->center).len2();

	i = idxmin3(rvtx);
	dp = psph->center - ptri->pt[i];

	edge[0] = ptri->pt[inc_mod3[i]] - ptri->pt[i];
	elen2[0] = edge[0].len2();
	edge[1] = ptri->pt[dec_mod3[i]] - ptri->pt[i];
	elen2[1] = edge[1].len2();
	bInside[0] = isneg((dp ^ edge[0]) * ptri->n);
	bInside[1] = isneg((edge[1] ^ dp) * ptri->n);
	rvtx[i] = (rvtx[i] * elen2[bInside[0]]) - (sqr(max(0.0f, dp * edge[bInside[0]])) * (bInside[0] | bInside[1]));
	denom = elen2[bInside[0]];

	if (bInside[0] & bInside[1])
	{
		if (edge[0] * edge[1] < 0)
		{
			edge[0] = ptri->pt[dec_mod3[i]] - ptri->pt[inc_mod3[i]];
			dp = psph->center - ptri->pt[inc_mod3[i]];
			if ((dp ^ edge[0]) * ptri->n > 0)
			{
				return quotientf((rvtx[inc_mod3[i]] * edge[0].len2()) - sqr(dp * edge[0]),
				                 edge[0].len2());
			}
		}
		bFace = 1;
		return quotientf(sqr((psph->center - ptri->pt[0]) * ptri->n), 1.0f);
	}
	return quotientf(rvtx[i], denom);
}

} // unnamed namespace

int box_box_overlap_check(const primitives::box* box1, const primitives::box* box2, COverlapChecker* pOverlapper)
{
	if ((box1->bOriented | box2->bOriented << 16) != pOverlapper->iPrevCode)
	{
		if (!box1->bOriented)
		{
			pOverlapper->Basis21 = box2->Basis.T();
		}
		else if (box2->bOriented)
		{
			pOverlapper->Basis21 = box1->Basis * box2->Basis.T();
		}
		else
		{
			pOverlapper->Basis21 = box1->Basis;
		}
		pOverlapper->Basis21abs = pOverlapper->Basis21.GetFabs();
		pOverlapper->iPrevCode = box1->bOriented | box2->bOriented << 16;
	}

	Vec3 center21 = box2->center - box1->center;
	if (box1->bOriented)
	{
		center21 = box1->Basis * center21;
	}
	const Vec3 &a = box1->size, &b = box2->size;
	float t1, t2, t3, e = (a.x + a.y + a.z) * 1e-4f;

	// node1 basis vectors
	if (fabsf(center21.x) > a.x + b * pOverlapper->Basis21abs.GetRow(0))
	{
		return 0;
	}
	if (fabsf(center21.y) > a.y + b * pOverlapper->Basis21abs.GetRow(1))
	{
		return 0;
	}
	if (fabsf(center21.z) > a.z + b * pOverlapper->Basis21abs.GetRow(2))
	{
		return 0;
	}

	// node2 basis vectors
	if (fabsf(center21 * pOverlapper->Basis21.GetColumn(0)) > a * pOverlapper->Basis21abs.GetColumn(0) + b.x)
	{
		return 0;
	}
	if (fabsf(center21 * pOverlapper->Basis21.GetColumn(1)) > a * pOverlapper->Basis21abs.GetColumn(1) + b.y)
	{
		return 0;
	}
	if (fabsf(center21 * pOverlapper->Basis21.GetColumn(2)) > a * pOverlapper->Basis21abs.GetColumn(2) + b.z)
	{
		return 0;
	}

	// node1->axes[0] x node2->axes[0]
	t1 = (a.y * pOverlapper->Basis21abs(2, 0)) + (a.z * pOverlapper->Basis21abs(1, 0));
	t2 = (b.y * pOverlapper->Basis21abs(0, 2)) + (b.z * pOverlapper->Basis21abs(0, 1));
	t3 = (center21.z * pOverlapper->Basis21(1, 0)) - (center21.y * pOverlapper->Basis21(2, 0));
	if (fabsf(t3) > t1 + t2 + e)
	{
		return 0;
	}

	// node1->axes[0] x node2->axes[1]
	t1 = (a.y * pOverlapper->Basis21abs(2, 1)) + (a.z * pOverlapper->Basis21abs(1, 1));
	t2 = (b.x * pOverlapper->Basis21abs(0, 2)) + (b.z * pOverlapper->Basis21abs(0, 0));
	t3 = (center21.z * pOverlapper->Basis21(1, 1)) - (center21.y * pOverlapper->Basis21(2, 1));
	if (fabsf(t3) > t1 + t2 + e)
	{
		return 0;
	}

	// node1->axes[0] x node2->axes[2]
	t1 = (a.y * pOverlapper->Basis21abs(2, 2)) + (a.z * pOverlapper->Basis21abs(1, 2));
	t2 = (b.x * pOverlapper->Basis21abs(0, 1)) + (b.y * pOverlapper->Basis21abs(0, 0));
	t3 = (center21.z * pOverlapper->Basis21(1, 2)) - (center21.y * pOverlapper->Basis21(2, 2));
	if (fabsf(t3) > t1 + t2 + e)
	{
		return 0;
	}

	// node1->axes[1] x node2->axes[0]
	t1 = (a.x * pOverlapper->Basis21abs(2, 0)) + (a.z * pOverlapper->Basis21abs(0, 0));
	t2 = (b.y * pOverlapper->Basis21abs(1, 2)) + (b.z * pOverlapper->Basis21abs(1, 1));
	t3 = (center21.x * pOverlapper->Basis21(2, 0)) - (center21.z * pOverlapper->Basis21(0, 0));
	if (fabsf(t3) > t1 + t2 + e)
	{
		return 0;
	}

	// node1->axes[1] x node2->axes[1]
	t1 = (a.x * pOverlapper->Basis21abs(2, 1)) + (a.z * pOverlapper->Basis21abs(0, 1));
	t2 = (b.x * pOverlapper->Basis21abs(1, 2)) + (b.z * pOverlapper->Basis21abs(1, 0));
	t3 = (center21.x * pOverlapper->Basis21(2, 1)) - (center21.z * pOverlapper->Basis21(0, 1));
	if (fabsf(t3) > t1 + t2 + e)
	{
		return 0;
	}

	// node1->axes[1] x node2->axes[2]
	t1 = (a.x * pOverlapper->Basis21abs(2, 2)) + (a.z * pOverlapper->Basis21abs(0, 2));
	t2 = (b.x * pOverlapper->Basis21abs(1, 1)) + (b.y * pOverlapper->Basis21abs(1, 0));
	t3 = (center21.x * pOverlapper->Basis21(2, 2)) - (center21.z * pOverlapper->Basis21(0, 2));
	if (fabsf(t3) > t1 + t2 + e)
	{
		return 0;
	}

	// node1->axes[2] x node2->axes[0]
	t1 = (a.x * pOverlapper->Basis21abs(1, 0)) + (a.y * pOverlapper->Basis21abs(0, 0));
	t2 = (b.y * pOverlapper->Basis21abs(2, 2)) + (b.z * pOverlapper->Basis21abs(2, 1));
	t3 = (center21.y * pOverlapper->Basis21(0, 0)) - (center21.x * pOverlapper->Basis21(1, 0));
	if (fabsf(t3) > t1 + t2 + e)
	{
		return 0;
	}

	// node1->axes[2] x node2->axes[1]
	t1 = (a.x * pOverlapper->Basis21abs(1, 1)) + (a.y * pOverlapper->Basis21abs(0, 1));
	t2 = (b.x * pOverlapper->Basis21abs(2, 2)) + (b.z * pOverlapper->Basis21abs(2, 0));
	t3 = (center21.y * pOverlapper->Basis21(0, 1)) - (center21.x * pOverlapper->Basis21(1, 1));
	if (fabsf(t3) > t1 + t2 + e)
	{
		return 0;
	}

	// node1->axes[2] x node2->axes[2]
	t1 = (a.x * pOverlapper->Basis21abs(1, 2)) + (a.y * pOverlapper->Basis21abs(0, 2));
	t2 = (b.x * pOverlapper->Basis21abs(2, 1)) + (b.y * pOverlapper->Basis21abs(2, 0));
	t3 = (center21.y * pOverlapper->Basis21(0, 2)) - (center21.x * pOverlapper->Basis21(1, 2));
	if (fabsf(t3) > t1 + t2 + e)
	{
		return 0;
	}

	return 1;
}

int box_ray_overlap_check(const primitives::box* pbox, const primitives::ray* pray, COverlapChecker*)
{
	Vec3 l, m, al;
	if (pbox->bOriented)
	{
		l = pbox->Basis * pray->dir * 0.5f;
		m = pbox->Basis * (pray->origin - pbox->center) + l;
	}
	else
	{
		l = pray->dir * 0.5f;
		m = pray->origin + l - pbox->center;
	}
	al.x = fabsf(l.x);
	al.y = fabsf(l.y);
	al.z = fabsf(l.z);

	// separating axis check for line and box
	if (fabsf(m.x) > pbox->size.x + al.x)
	{
		return 0;
	}
	if (fabsf(m.y) > pbox->size.y + al.y)
	{
		return 0;
	}
	if (fabsf(m.z) > pbox->size.z + al.z)
	{
		return 0;
	}

	if (fabsf((m.z * l.y) - (m.y * l.z)) > (pbox->size.y * al.z) + (pbox->size.z * al.y))
	{
		return 0;
	}
	if (fabsf((m.x * l.z) - (m.z * l.x)) > (pbox->size.x * al.z) + (pbox->size.z * al.x))
	{
		return 0;
	}
	if (fabsf((m.x * l.y) - (m.y * l.x)) > (pbox->size.x * al.y) + (pbox->size.y * al.x))
	{
		return 0;
	}

	return 1;
}

int box_sphere_overlap_check(const primitives::box* pbox, const primitives::sphere* psph, COverlapChecker*)
{
	Vec3 center = psph->center - pbox->center, dist;
	if (pbox->bOriented)
	{
		center = pbox->Basis * center;
	}
	dist.x = max(0.0f, fabsf(center.x) - pbox->size.x);
	dist.y = max(0.0f, fabsf(center.y) - pbox->size.y);
	dist.z = max(0.0f, fabsf(center.z) - pbox->size.z);
	return isneg(dist.len2() - sqr(psph->r));
}

int tri_sphere_overlap_check(const primitives::triangle* ptri, const primitives::sphere* psph, COverlapChecker*)
{
	int bFace;
	return isneg(tri_sphere_dist2(ptri, psph, bFace) - sqr(psph->r));
}

namespace {

int default_overlap_check(const primitives::primitive*, const primitives::primitive*, COverlapChecker*)
{
	return 1;
}

int box_tri_overlap_check(const primitives::box* pbox, const primitives::triangle* ptri, COverlapChecker*)
{
	Vec3 pt[3], n;
	float l1, l2, l3, l, c;
	if (pbox->bOriented)
	{
		pt[0] = pbox->Basis * (ptri->pt[0] - pbox->center);
		pt[1] = pbox->Basis * (ptri->pt[1] - pbox->center);
		pt[2] = pbox->Basis * (ptri->pt[2] - pbox->center);
		n = pbox->Basis * ptri->n;
	}
	else
	{
		pt[0] = ptri->pt[0] - pbox->center;
		pt[1] = ptri->pt[1] - pbox->center;
		pt[2] = ptri->pt[2] - pbox->center;
		n = ptri->n;
	}

	// check box normals
	l1 = fabsf(pt[0].x - pt[1].x);
	l2 = fabsf(pt[1].x - pt[2].x);
	l3 = fabsf(pt[2].x - pt[0].x);
	l = l1 + l2 + l3; // half length = l/4
	c = (l1 * (pt[0].x + pt[1].x) + l2 * (pt[1].x + pt[2].x) + l3 * (pt[2].x + pt[0].x)) * 0.5f; // center = c/l
	if (fabsf(c) > (pbox->size.x + l * 0.25f) * l)
	{
		return 0;
	}
	l1 = fabsf(pt[0].y - pt[1].y);
	l2 = fabsf(pt[1].y - pt[2].y);
	l3 = fabsf(pt[2].y - pt[0].y);
	l = l1 + l2 + l3;
	c = (l1 * (pt[0].y + pt[1].y) + l2 * (pt[1].y + pt[2].y) + l3 * (pt[2].y + pt[0].y)) * 0.5f;
	if (fabsf(c) > (pbox->size.y + l * 0.25f) * l)
	{
		return 0;
	}
	l1 = fabsf(pt[0].z - pt[1].z);
	l2 = fabsf(pt[1].z - pt[2].z);
	l3 = fabsf(pt[2].z - pt[0].z);
	l = l1 + l2 + l3;
	c = (l1 * (pt[0].z + pt[1].z) + l2 * (pt[1].z + pt[2].z) + l3 * (pt[2].z + pt[0].z)) * 0.5f;
	if (fabsf(c) > (pbox->size.z + l * 0.25f) * l)
	{
		return 0;
	}

	// check triangle normal
	if ((fabsf(n.x) * pbox->size.x) + (fabsf(n.y) * pbox->size.y) + (fabsf(n.z) * pbox->size.z) < fabsf(n * pt[0]))
	{
		return 0;
	}

	Vec3 edge, triproj1, triproj2;
	float boxproj;

	// check triangle edges - box edges cross products
	for (int i = 0; i < 3; i++)
	{
		edge = pt[inc_mod3[i]] - pt[i];
		triproj1 = edge ^ pt[i];
		triproj2 = edge ^ pt[dec_mod3[i]];
		boxproj = fabsf(pbox->size.y * edge.z) + fabsf(pbox->size.z * edge.y);
		if (fabsf((triproj1.x + triproj2.x) * 0.5f) > boxproj + (fabsf(triproj1.x - triproj2.x) * 0.5f))
		{
			return 0;
		}
		boxproj = fabsf(pbox->size.x * edge.z) + fabsf(pbox->size.z * edge.x);
		if (fabsf((triproj1.y + triproj2.y) * 0.5f) > boxproj + (fabsf(triproj1.y - triproj2.y) * 0.5f))
		{
			return 0;
		}
		boxproj = fabsf(pbox->size.x * edge.y) + fabsf(pbox->size.y * edge.x);
		if (fabsf((triproj1.z + triproj2.z) * 0.5f) > boxproj + (fabsf(triproj1.z - triproj2.z) * 0.5f))
		{
			return 0;
		}
	}

	return 1;
}

int box_heightfield_overlap_check(const primitives::box* pbox, const primitives::heightfield* phf,
                                  COverlapChecker* pOverlapper)
{
	primitives::box boxtr;
	Vec3 vtx[4];
	primitives::triangle hftri;
	vector2df sz, ptmin, ptmax;
	int ix, iy, ix0, iy0, ix1, iy1;
	float hmax;

	// find the 3 lowest vertices
	if (phf->bOriented)
	{
		if (pbox->bOriented)
		{
			boxtr.Basis = pbox->Basis * phf->Basis.T();
		}
		else
		{
			boxtr.Basis = phf->Basis.T();
		}
		boxtr.center = phf->Basis * (pbox->center - phf->origin);
	}
	else
	{
		boxtr.Basis = pbox->Basis;
		boxtr.center = pbox->center - phf->origin;
	}
	boxtr.bOriented = 1;
	boxtr.Basis.SetRow(0, boxtr.Basis.GetRow(0) * -sgnnz(boxtr.Basis(0, 2)));
	boxtr.Basis.SetRow(1, boxtr.Basis.GetRow(1) * -sgnnz(boxtr.Basis(1, 2)));
	boxtr.Basis.SetRow(2, boxtr.Basis.GetRow(2) * -sgnnz(boxtr.Basis(2, 2)));
	boxtr.size = pbox->size;
	vtx[0] = pbox->size * boxtr.Basis;
	boxtr.Basis.SetRow(0, -boxtr.Basis.GetRow(0));
	vtx[1] = pbox->size * boxtr.Basis;
	boxtr.Basis.SetRow(0, -boxtr.Basis.GetRow(0));
	boxtr.Basis.SetRow(1, -boxtr.Basis.GetRow(1));
	vtx[2] = pbox->size * boxtr.Basis;
	boxtr.Basis.SetRow(1, -boxtr.Basis.GetRow(1));
	boxtr.Basis.SetRow(2, -boxtr.Basis.GetRow(2));
	vtx[3] = pbox->size * boxtr.Basis;
	boxtr.Basis.SetRow(2, -boxtr.Basis.GetRow(2));

	// find the underlying grid rectangle
	sz.x = sz.y = 0;
	sz.x = max(sz.x, fabsf(vtx[1].x));
	sz.y = max(sz.y, fabsf(vtx[1].y));
	sz.x = max(sz.x, fabsf(vtx[2].x));
	sz.y = max(sz.y, fabsf(vtx[2].y));
	sz.x = max(sz.x, fabsf(vtx[3].x));
	sz.y = max(sz.y, fabsf(vtx[3].y));
	ptmin.x = (boxtr.center.x - sz.x) * phf->stepr.x;
	ptmin.y = (boxtr.center.y - sz.y) * phf->stepr.y;
	ptmax.x = (boxtr.center.x + sz.x) * phf->stepr.x;
	ptmax.y = (boxtr.center.y + sz.y) * phf->stepr.y;
	ix0 = physics_float2int(ptmin.x - 0.5f);
	iy0 = physics_float2int(ptmin.y - 0.5f);
	ix0 &= ~(ix0 >> 31);
	iy0 &= ~(iy0 >> 31);
	ix1 = min(physics_float2int(ptmax.x + 0.5f), phf->size.x);
	iy1 = min(physics_float2int(ptmax.y + 0.5f), phf->size.y);
	vtx[0] += boxtr.center;
	vtx[1] += boxtr.center;
	vtx[2] += boxtr.center;
	vtx[3] += boxtr.center;

	if ((ix1 - ix0) * (iy1 - iy0) <= 6)
	{
		// check if all heightfield points are below the lowest box point
		for (ix = ix0, hmax = 0; ix <= ix1; ix++)
		{
			for (iy = iy0; iy <= iy1; iy++)
			{
				hmax = max(hmax, phf->getheight(ix, iy));
			}
		}
		if (hmax < vtx[0].z)
		{
			return 0;
		}

		// check for intersection with each underlying triangle
		for (ix = ix0; ix < ix1; ix++)
		{
			for (iy = iy0; iy < iy1; iy++)
			{
				hftri.pt[0].x = hftri.pt[2].x = ix * phf->step.x;
				hftri.pt[0].y = hftri.pt[1].y = iy * phf->step.y;
				hftri.pt[1].x = hftri.pt[0].x + phf->step.x;
				hftri.pt[2].y = hftri.pt[0].y + phf->step.y;
				hftri.pt[0].z = phf->getheight(ix, iy);
				hftri.pt[1].z = phf->getheight(ix + 1, iy);
				hftri.pt[2].z = phf->getheight(ix, iy + 1);
				hftri.n = hftri.pt[1] - hftri.pt[0] ^ hftri.pt[2] - hftri.pt[0];
				if (box_tri_overlap_check(&boxtr, &hftri, pOverlapper))
				{
					return 1;
				}
				hftri.pt[0] = hftri.pt[2];
				hftri.pt[2].x += phf->step.x;
				hftri.pt[2].z = phf->getheight(ix + 1, iy + 1);
				hftri.n = hftri.pt[1] - hftri.pt[0] ^ hftri.pt[2] - hftri.pt[0];
				if (box_tri_overlap_check(&boxtr, &hftri, pOverlapper))
				{
					return 1;
				}
			}
		}
	}
	else
	{
		for (ix = ix0; ix <= ix1; ix++)
		{
			for (iy = iy0; iy <= iy1; iy++)
			{
				if (phf->getheight(ix, iy) > vtx[0].z)
				{
					return 1;
				}
			}
		}
	}

	return 0;
}

int heightfield_box_overlap_check(const primitives::heightfield* phf, const primitives::box* pbox,
                                  COverlapChecker* pOverlapper)
{
	return box_heightfield_overlap_check(pbox, phf, pOverlapper);
}

int box_voxgrid_overlap_check(const primitives::box* pbox, const primitives::voxelgrid* pgrid,
                              COverlapChecker* pOverlapper)
{
	int i, j, nTris = 0, nTrisDst, icell;
	primitives::box boxtr;
	Vec3 dim;
	Vec3i ic, iBBox[2];
	const int MAXTESTRIS = 8;
	int idxbuf[(MAXTESTRIS + 1) * 2], *plist = idxbuf, *plistDst = idxbuf + MAXTESTRIS + 1;
	primitives::triangle atri;

	if (pgrid->bOriented)
	{
		if (pbox->bOriented)
		{
			boxtr.Basis = pbox->Basis * pgrid->Basis.T();
		}
		else
		{
			boxtr.Basis = pgrid->Basis.T();
		}
		boxtr.center = pgrid->Basis * (pbox->center - pgrid->origin);
	}
	else
	{
		boxtr.Basis = pbox->Basis;
		boxtr.center = pbox->center - pgrid->origin;
	}
	boxtr.size = pbox->size;
	boxtr.bOriented = 1;

	dim = boxtr.size * boxtr.Basis.Fabs();
	for (i = 0; i < 3; i++)
	{
		iBBox[0][i] = max(0, physics_float2int(((boxtr.center[i] - dim[i]) * pgrid->stepr[i]) - 0.5f));
		iBBox[1][i] =
		    min(pgrid->size[i], physics_float2int(((boxtr.center[i] + dim[i]) * pgrid->stepr[i]) + 0.5f));
	}

	if ((iBBox[1] - iBBox[0]).GetVolume() > 18)
	{
		return 1;
	}

	boxtr.Basis = pbox->Basis * pgrid->R;
	boxtr.center = ((pbox->center - pgrid->offset) * pgrid->R) * pgrid->rscale;
	boxtr.size = pbox->size * pgrid->rscale;

	for (ic.z = iBBox[0].z; ic.z < iBBox[1].z; ic.z++)
	{
		for (ic.y = iBBox[0].y; ic.y < iBBox[1].y; ic.y++)
		{
			for (ic.x = iBBox[0].x; ic.x < iBBox[1].x; ic.x++)
			{
				icell = ic * pgrid->stride;
				nTrisDst = unite_lists(plist, nTris, pgrid->pTriBuf + pgrid->pCellTris[icell],
				                       pgrid->pCellTris[icell + 1] - pgrid->pCellTris[icell], plistDst,
				                       MAXTESTRIS + 1);
				if (nTrisDst > MAXTESTRIS)
				{
					return 1;
				}
				std::swap(nTris, nTrisDst);
				std::swap(plist, plistDst);
			}
		}
	}

	for (i = 0; i < nTris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			atri.pt[j] = pgrid->pVtx[pgrid->pIndices[(plist[i] * 3) + j]];
		}
		atri.n = pgrid->pNormals[plist[i]];
		if (box_tri_overlap_check(&boxtr, &atri, pOverlapper))
		{
			return 1;
		}
	}

	return 0;
}

int voxgrid_box_overlap_check(const primitives::voxelgrid* pgrid, const primitives::box* pbox,
                              COverlapChecker* pOverlapper)
{
	return box_voxgrid_overlap_check(pbox, pgrid, pOverlapper);
}

int heightfield_voxgrid_overlap_check(const primitives::heightfield* phf, const primitives::voxelgrid* pgrid,
                                      COverlapChecker*)
{
	return 0;
}

int voxgrid_heightfield_overlap_check(const primitives::voxelgrid* pgrid, const primitives::heightfield* phf,
                                      COverlapChecker*)
{
	return 0;
}

int tri_box_overlap_check(const primitives::triangle* ptri, const primitives::box* pbox, COverlapChecker* pOverlapper)
{
	return box_tri_overlap_check(pbox, ptri, pOverlapper);
}

int ray_box_overlap_check(const primitives::ray* pray, const primitives::box* pbox, COverlapChecker* pOverlapper)
{
	return box_ray_overlap_check(pbox, pray, pOverlapper);
}

int sphere_box_overlap_check(const primitives::sphere* psph, const primitives::box* pbox, COverlapChecker* pOverlapper)
{
	return box_sphere_overlap_check(pbox, psph, pOverlapper);
}

int sphere_tri_overlap_check(const primitives::sphere* psph, const primitives::triangle* ptri,
                             COverlapChecker* pOverlapper)
{
	return tri_sphere_overlap_check(ptri, psph, pOverlapper);
}

int heightfield_sphere_overlap_check(const primitives::heightfield* phf, const primitives::sphere* psph,
                                     COverlapChecker*)
{
	Vec3 center;
	vector2di irect[2];
	int ix, iy, bContact = 0;

	center = phf->Basis * (psph->center - phf->origin);
	irect[0].x = min(phf->size.x, max(0, physics_float2int(((center.x - psph->r) * phf->stepr.x) - 0.5f)));
	irect[0].y = min(phf->size.y, max(0, physics_float2int(((center.y - psph->r) * phf->stepr.y) - 0.5f)));
	irect[1].x = min(phf->size.x, max(0, physics_float2int(((center.x + psph->r) * phf->stepr.x) + 0.5f)));
	irect[1].y = min(phf->size.y, max(0, physics_float2int(((center.y + psph->r) * phf->stepr.y) + 0.5f)));

	for (ix = irect[0].x; ix < irect[1].x; ix++)
	{
		for (iy = irect[0].y; iy < irect[1].y; iy++)
		{
			bContact |= isneg(center.z - psph->r - phf->getheight(ix, iy));
		}
	}

	return bContact;
}

int sphere_heightfield_overlap_check(const primitives::sphere* psph, const primitives::heightfield* phf,
                                     COverlapChecker* pOverlapper)
{
	return heightfield_sphere_overlap_check(phf, psph, pOverlapper);
}

int sphere_sphere_overlap_check(const primitives::sphere* psph1, const primitives::sphere* psph2, COverlapChecker*)
{
	return isneg((psph1->center - psph2->center).len2() - sqr(psph1->r + psph2->r));
}

} // unnamed namespace

COverlapChecker::COverlapChecker()
{
	for (int i = 0; i < NPRIMS; i++)
	{
		for (int j = 0; j < NPRIMS; j++)
		{
			table[i][j] = default_overlap_check;
		}
	}

	using namespace primitives;

	table[box::type][box::type] = (overlap_check)box_box_overlap_check;
	table[box::type][triangle::type] = (overlap_check)box_tri_overlap_check;
	table[triangle::type][box::type] = (overlap_check)tri_box_overlap_check;
	table[box::type][heightfield::type] = (overlap_check)box_heightfield_overlap_check;
	table[heightfield::type][box::type] = (overlap_check)heightfield_box_overlap_check;
	table[box::type][voxelgrid::type] = (overlap_check)box_voxgrid_overlap_check;
	table[voxelgrid::type][box::type] = (overlap_check)voxgrid_box_overlap_check;
	table[heightfield::type][voxelgrid::type] = (overlap_check)heightfield_voxgrid_overlap_check;
	table[voxelgrid::type][heightfield::type] = (overlap_check)voxgrid_heightfield_overlap_check;
	table[box::type][ray::type] = (overlap_check)box_ray_overlap_check;
	table[ray::type][box::type] = (overlap_check)ray_box_overlap_check;
	table[box::type][sphere::type] = (overlap_check)box_sphere_overlap_check;
	table[sphere::type][box::type] = (overlap_check)sphere_box_overlap_check;
	table[triangle::type][sphere::type] = (overlap_check)tri_sphere_overlap_check;
	table[sphere::type][triangle::type] = (overlap_check)sphere_tri_overlap_check;
	table[sphere::type][sphere::type] = (overlap_check)sphere_sphere_overlap_check;
	table[heightfield::type][sphere::type] = (overlap_check)heightfield_sphere_overlap_check;
	table[sphere::type][heightfield::type] = (overlap_check)sphere_heightfield_overlap_check;
}

int COverlapChecker::Check(int type1, int type2, primitives::primitive* prim1, primitives::primitive* prim2)
{
	return table[type1][type2](prim1, prim2, this);
}

int COverlapChecker::CheckExists(int type1, int type2)
{
	return table[type1][type2] != default_overlap_check;
}
