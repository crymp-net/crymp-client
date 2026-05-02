#include "IntersectData.h"
#include "SphereGeom.h"
#include "Utils.h"

CSphereGeom* CSphereGeom::CreateSphere(primitives::sphere* psphere)
{
	m_sphere.center = psphere->center;
	m_sphere.r = psphere->r;

	primitives::box bbox;
	bbox.Basis.SetIdentity();
	bbox.center = m_sphere.center;
	bbox.size.Set(m_sphere.r, m_sphere.r, m_sphere.r);
	m_Tree.SetBox(&bbox);
	m_Tree.Build(this);
	m_minVtxDist = m_sphere.r * 1E-4f;
	return this;
}

int CSphereGeom::CalcPhysicalProperties(phys_geometry* pgeom)
{
	pgeom->pGeom = this;
	pgeom->origin = m_sphere.center;
	pgeom->q.SetIdentity();
	pgeom->V = 4.0f / 3 * g_PI * cube(m_sphere.r);
	float x2 = sqr(m_sphere.r) * 0.4f * pgeom->V;
	pgeom->Ibody.Set(x2, x2, x2);
	return 1;
}

int CSphereGeom::PointInsideStatus(const Vec3& pt)
{
	return isneg((pt - m_sphere.center).len2() - sqr(m_sphere.r));
}

float CSphereGeom::ComputeExtent(GeomQuery& geo, EGeomForm eForm)
{
	switch (eForm)
	{
		default:
			assert(0);
		case GeomForm_Vertices:
		case GeomForm_Edges:
			return 0.f;
		case GeomForm_Surface:
			return gf_PI * 4.f * square(m_sphere.r);
		case GeomForm_Volume:
			return gf_PI * 4.f / 3.f * cube(m_sphere.r);
	}
}

void CSphereGeom::GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm)
{
	switch (eForm)
	{
		default:
			assert(0);
		case GeomForm_Vertices:
		case GeomForm_Edges:
			ran.vPos = m_sphere.center;
			ran.vNorm.zero();
			return;
		case GeomForm_Surface:
		case GeomForm_Volume:
		{
			float fLen2;
			do
			{
				ran.vPos = BiRandom(Vec3(1.f));
				fLen2 = ran.vPos.GetLengthSquared();
			}
			while (fLen2 > 1.f);
			ran.vNorm = ran.vPos.GetNormalized();
			if (eForm == GeomForm_Surface)
			{
				ran.vPos = ran.vNorm * m_sphere.r;
			}
			else
			{
				ran.vPos *= m_sphere.r;
			}
			break;
		}
	}
	ran.vPos += m_sphere.center;
}

int CSphereGeom::PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
                                            geometry_under_test* pGTestColl, bool bKeepPrevContacts)
{
	static char g_SphIdBuf[1];

	pGTest->pGeometry = this;
	pGTest->pBVtree = &m_Tree;
	m_Tree.PrepareForIntersectionTest(pGTest, pCollider, pGTestColl);

	pGTest->primbuf = pGTest->primbuf1 = g_idata[pGTest->iCaller].SphBuf + g_idata[pGTest->iCaller].SphBufPos++;
	pGTest->szprimbuf = 1;
	pGTest->typeprim = primitives::sphere::type;
	pGTest->szprim = sizeof(primitives::sphere);
	pGTest->idbuf = g_SphIdBuf;
	pGTest->surfaces = 0;
	pGTest->edges = 0;
	pGTest->minAreaEdge = 1E10f;
	return 1;
}

int CSphereGeom::PreparePrimitive(geom_world_data* pgwd, primitives::primitive*& pprim, int iCaller)
{
	primitives::sphere* psph = g_idata[iCaller].SphBuf + g_idata[iCaller].SphBufPos;
	g_idata[iCaller].SphBufPos = (g_idata[iCaller].SphBufPos + 1) &
	                             ((sizeof(g_idata[iCaller].SphBuf) / sizeof(g_idata[iCaller].SphBuf[0])) - 1);
	psph->center = pgwd->R * m_sphere.center * pgwd->scale + pgwd->offset;
	psph->r = m_sphere.r * pgwd->scale;
	pprim = psph;
	return primitives::sphere::type;
}

int CSphereGeom::GetPrimitiveList(int iStart, int nPrims, int typeCollider, primitives::primitive* pCollider,
                                  int bColliderLocal, geometry_under_test* pGTest, geometry_under_test* pGTestOp,
                                  primitives::primitive* pRes, char* pResId)
{
	((primitives::sphere*)pRes)->center = pGTest->R * m_sphere.center * pGTest->scale + pGTest->offset;
	((primitives::sphere*)pRes)->r = m_sphere.r * pGTest->scale;
	pGTest->bTransformUpdated = 0;
	*pResId = (char)-1;
	return 1;
}

int CSphereGeom::GetUnprojectionCandidates(int iop, const contact* pcontact, primitives::primitive*& pprim,
                                           int*& piFeature, geometry_under_test* pGTest)
{
	pprim = pGTest->primbuf1;
	((primitives::sphere*)pprim)->center = pGTest->R * m_sphere.center * pGTest->scale + pGTest->offset;
	((primitives::sphere*)pprim)->r = m_sphere.r * pGTest->scale;

	pGTest->idbuf[0] = (char)-1;
	pGTest->nSurfaces = 0;
	pGTest->nEdges = 0;

	return 1;
}

int CSphereGeom::FindClosestPoint(geom_world_data* pgwd, int& iPrim, int& iFeature, const Vec3& ptdst0,
                                  const Vec3& ptdst1, Vec3* ptres, int nMaxIters)
{
	Vec3 center = pgwd->R * m_sphere.center * pgwd->scale + pgwd->offset;
	float r = m_sphere.r * pgwd->scale;
	ptres[1] = ptdst0;
	if ((ptdst0 - center).len2() < r * r)
	{
		ptres[0].Set(1E10f, 1E10f, 1E10f);
		return -1;
	}
	ptres[0] = center + (ptdst0 - center).normalized() * r;
	return 1;
}

int CSphereGeom::UnprojectSphere(Vec3 center, float r, float rsep, contact* pcontact)
{
	Vec3 dc = center - m_sphere.center;
	if (dc.len2() > sqr(m_sphere.r + rsep))
	{
		return 0;
	}
	pcontact->n = dc.normalized();
	pcontact->t = dc * pcontact->n - m_sphere.r - r;
	pcontact->pt = m_sphere.center + pcontact->n * m_sphere.r;
	return 1;
}

float CSphereGeom::CalculateBuoyancy(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& massCenter)
{
	float r = m_sphere.r * pgwd->scale, x;
	Vec3 n = pplane->n, center = pgwd->R * m_sphere.center + pgwd->offset;
	massCenter = center;
	x = (pplane->origin - center) * n;

	if (x > r)
	{
		return 0;
	}
	if (x < -r)
	{
		return (4.0f / 3) * g_PI * cube(r);
	}
	massCenter += n * ((g_PI * 0.5f * (x * x * (r * r - x * x * 0.5f) - r * r * r * r * 0.5f)) - x);
	return g_PI * ((2.0f / 3) * cube(r) + x * (r * r - x * x * (1.0f / 3)));
}

void CSphereGeom::CalculateMediumResistance(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& dPres,
                                            Vec3& dLres)
{
	Vec3 center, v, vn, axisx, axisy, n;
	float r, x, vxn, vxninv, nv, cx, ry, l, lx, ly, circ_S, circ_x, ell_S, ell_x, S;
	center = pgwd->R * m_sphere.center * pgwd->scale + pgwd->offset;
	r = m_sphere.r * pgwd->scale;
	n = pplane->n;
	v = pgwd->v + (pgwd->w ^ center - pgwd->centerOfMass);
	x = (pplane->origin - center) * n;

	if (fabsf(x) > r)
	{
		dLres.zero();
		dPres.zero();
		if (x > r)
		{
			dPres = v * (-g_PI * r * r);
		}
		return;
	}
	vn = v.normalized();
	axisy = vn ^ n;
	vxn = axisy.len();
	if (vxn < 0.01f)
	{
		// axisy = vn.orthogonal().normalized(); l = sgnnz(x)*r;
		axisy = vn.GetOrthogonal().normalized();
		l = sgnnz(x) * r;
	}
	else
	{ // l - water plane - v+ plane intersection line along x
		axisy *= (vxninv = 1.0f / vxn);
		l = x * vxninv;
	}
	axisx = axisy ^ vn;
	nv = n * v;
	cx = x * vxn;                                  // ellipse center along x
	ry = max(0.001f, sqrt_tpl((r * r) - (x * x))); // ellipse radius along y
	lx = max(-r * 0.999f, min(r * 0.999f, l));
	ly = sqrt_tpl((r * r) - (lx * lx));
	circ_S = (lx * ly + r * r * (g_PI * 0.5f + asin_tpl(lx / r))) * 0.5f;
	circ_x = cube(ly) * (-1.0f / 3);
	lx = max(-ry * 0.999f, min(ry * 0.999f, (l - cx) / max(0.001f, fabs_tpl(nv))));
	ly = sqrt_tpl((ry * ry) - (lx * lx));
	ell_S = (lx * ly + ry * ry * (g_PI * 0.5f + asin_tpl(lx / ry) * sgnnz(nv))) * fabs_tpl(nv) * 0.5f;
	ell_x = (cube(ly) * (-1.0f / 3) * nv) + cx;
	S = circ_S + ell_S;
	x = (circ_x * circ_S + ell_x * ell_S) / S;
	dPres = -v * S;
	center += axisx * x + vn * sqrt_tpl(max(0.0f, (r * r) - (x * x)));
	dLres = center - pgwd->centerOfMass ^ dPres;
}

void CSphereGeom::CalcVolumetricPressure(geom_world_data* gwd, const Vec3& epicenter, float k, float rmin,
                                         const Vec3& centerOfMass, Vec3& P, Vec3& L)
{
	Vec3 dc = gwd->R * m_sphere.center * gwd->scale + gwd->offset - epicenter;
	P += dc * (k / (dc.len() * max(dc.len2(), sqr(rmin))));
}

int CSphereGeom::DrawToOcclusionCubemap(const geom_world_data* pgwd, int iStartPrim, int nPrims, int iPass,
                                        int* pGrid[6], int nRes, float rmin, float rmax, float zscale)
{
	Vec3 center = pgwd->R * m_sphere.center * pgwd->scale + pgwd->offset, axisx, axisy, pt[8];
	float x = m_sphere.r * pgwd->scale, y = 0;
	const float step = 1.0f / sqrt2;
	axisx = center.GetOrthogonal().normalized();
	axisy = (axisx ^ center).normalized();
	for (int i = 0; i < 8; i++)
	{
		pt[i] = center + axisx * x + axisy * y;
		x = (x - y) * (1.0f / sqrt2);
		y = x + (y * sqrt2);
	}
	RasterizePolygonIntoCubemap(pt, 8, iPass, pGrid, nRes, rmin, rmax, zscale);
	return 1;
}

void CSphereGeom::DrawWireframe(IPhysRenderer* pRenderer, geom_world_data* gwd, int iLevel, int idxColor)
{
	if (iLevel == 0)
	{
		pRenderer->DrawGeometry(this, gwd, idxColor);
	}
	else
	{
		int iCaller = GetCaller();
		BV* pbbox;
		ResetGlobalPrimsBuffers(iCaller);
		m_Tree.GetNodeBV(gwd->R, gwd->offset, gwd->scale, pbbox, 0, iCaller);
		DrawBBox(pRenderer, idxColor, gwd, &m_Tree, (BBox*)pbbox, iLevel - 1, 0, iCaller);
	}
}

void CSphereGeom::Save(CMemStream& stm)
{
	stm.Write(m_sphere);
	m_Tree.Save(stm);
}

void CSphereGeom::Load(CMemStream& stm)
{
	stm.Read(m_sphere);
	m_Tree.Load(stm, this);
}
