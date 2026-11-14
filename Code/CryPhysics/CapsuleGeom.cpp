#include "CapsuleGeom.h"
#include "IntersectData.h"
#include "SphereGeom.h"
#include "Utils.h"

CCapsuleGeom* CCapsuleGeom::CreateCapsule(primitives::capsule* pcaps)
{
	m_cyl.center = pcaps->center;
	m_cyl.axis = pcaps->axis;
	m_cyl.hh = pcaps->hh;
	m_cyl.r = pcaps->r;

	primitives::box bbox;
	bbox.Basis.SetRow(2, m_cyl.axis);
	bbox.Basis.SetRow(0, m_cyl.axis.GetOrthogonal().normalized());
	bbox.Basis.SetRow(1, m_cyl.axis ^ bbox.Basis.GetRow(0));
	bbox.bOriented = 1;
	bbox.center = m_cyl.center;
	bbox.size.z = m_cyl.hh + m_cyl.r;
	bbox.size.x = bbox.size.y = m_cyl.r;
	m_Tree.SetBox(&bbox);
	m_Tree.Build(this);
	m_minVtxDist = (m_cyl.r + m_cyl.hh) * 1E-4f;
	return this;
}

int CCapsuleGeom::CalcPhysicalProperties(phys_geometry* pgeom)
{
	pgeom->pGeom = this;
	pgeom->origin = m_cyl.center;
	pgeom->q.SetRotationV0V1(Vec3(0, 0, 1), m_cyl.axis);
	float Vcap = 4.0f / 3 * g_PI * cube(m_cyl.r);
	pgeom->V = (sqr(m_cyl.r) * m_cyl.hh * (g_PI * 2)) + Vcap;
	float r2 = sqr(m_cyl.r), x2 = g_PI * m_cyl.hh * sqr(r2) * 0.5f, z2 = g_PI * r2 * cube(m_cyl.hh) * (2.0 / 3);
	pgeom->Ibody.Set(x2 + z2 + (Vcap * (r2 * 0.4f + sqr(m_cyl.hh))), x2 + z2 + (Vcap * (r2 * 0.4f + sqr(m_cyl.hh))),
	                 (x2 * 2) + (Vcap * r2 * 0.4f));
	return 1;
}

int CCapsuleGeom::PointInsideStatus(const Vec3& pt)
{
	Vec3 ptr, ptc;
	float h;
	ptc = ptr = pt - m_cyl.center;
	h = m_cyl.axis * ptr;
	ptr -= m_cyl.axis * h;
	ptc -= m_cyl.axis * (m_cyl.hh * sgnnz(h));
	return isneg(min(ptc.len2() - sqr(m_cyl.r), max(ptr.len2() - sqr(m_cyl.r), fabs_tpl(h) - m_cyl.hh)));
}

int CCapsuleGeom::PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
                                             geometry_under_test* pGTestColl, bool bKeepPrevContacts)
{
	CCylinderGeom::PrepareForIntersectionTest(pGTest, pCollider, pGTestColl, bKeepPrevContacts);
	pGTest->typeprim = primitives::capsule::type;
	pGTest->szprim = sizeof(primitives::capsule);
	return 1;
}

int CCapsuleGeom::GetUnprojectionCandidates(int iop, const contact* pcontact, primitives::primitive*& pprim,
                                            int*& piFeature, geometry_under_test* pGTest)
{
	if (pGTest->bTransformUpdated)
	{
		pprim = pGTest->primbuf1;
		PrepareCylinder((primitives::cylinder*)pprim, pGTest);
	}
	primitives::cylinder* pcyl = (primitives::cylinder*)pprim;
	pGTest->idbuf[0] = (char)-1;

	int iFeature = pcontact->iFeature[iop];
	float r2, hh;
	r2 = (pcontact->pt - pcyl->center ^ pcyl->axis).len2();
	hh = (pcontact->pt - pcyl->center) * pcyl->axis;

	if (pcontact->iFeature[iop] == 0x40 ||
	    fabs_tpl(r2 - sqr(pcyl->r)) < sqr(m_minVtxDist) && fabs_tpl(fabs_tpl(hh) - pcyl->hh) < m_minVtxDist)
	{
		pGTest->edges[0].dir = pcyl->axis;
		pGTest->edges[0].n[0] = pcyl->center - pcontact->pt ^ pcyl->axis;
		pGTest->edges[0].n[1] = -pGTest->edges[0].n[0];
		pGTest->edges[0].idx = 0;
		pGTest->edges[0].iFeature = iFeature;
		pGTest->nEdges = 1;
	}
	else
	{
		pGTest->nEdges = 0;
	}
	pGTest->nSurfaces = 0;

	return 1;
}

int CCapsuleGeom::FindClosestPoint(geom_world_data* pgwd, int& iPrim, int& iFeature, const Vec3& ptdst0,
                                   const Vec3& ptdst1, Vec3* ptres, int nMaxIters)
{
	Vec3r axis, center, pt, l, n, ptdst[] = {ptdst0, ptdst1}, ptresi[2];
	real r, hh, r2, n2, t0, t1;
	int i, bLine;
	axis = pgwd->R * m_cyl.axis;
	center = pgwd->R * m_cyl.center * pgwd->scale + pgwd->offset;
	r = m_cyl.r * pgwd->scale;
	r2 = r * r;
	hh = m_cyl.hh * pgwd->scale;
	pt = ptdst0 - center;
	ptres[1] = ptdst0;

	bLine = isneg((r2 * 1E-6f) - (l = ptdst1 - ptdst0).len2());
	if (bLine)
	{
		n = l ^ axis;
		n2 = n.len2();
		if (isneg((n2 * r2) - sqr(pt * n)) & inrange(t0 = (-pt ^ axis) * n, (real)0, n2) &
		    inrange(t1 = (-pt ^ l) * n, -n2 * hh, n2 * hh))
		{
			ptres[1] = ptdst0 + l * (t0 / n2); // line-capsule side distance
			ptres[0] = center + axis * (axis * (ptres[1] - center));
			ptres[0] += (ptres[1] - ptres[0]).normalized() * r;
			return 1;
		}
		for (i = -1; i <= 1; i += 2)
		{
			if (inrange(t0 = l * (center + axis * (hh * i) - ptdst0), (real)0, l.len2()) &&
			    (((ptdst0 - center) * axis) * l.len2() + (l * axis) * t0) * i > hh * l.len2())
			{
				ptres[1] = ptdst0 + l * (t0 / l.len2());
				ptres[0] = center + axis * (hh * i); // line-spherical cap
				ptres[0] += (ptres[1] - ptres[0]).normalized() * r;
				return 1;
			}
		}
	}

	ptresi[1].zero();
	for (i = 0; i <= bLine; i++)
	{
		pt = ptdst[i] - center;
		if (fabsf(pt * axis) < hh)
		{ // the closest point lies on capsule side
			pt -= axis * (axis * pt);
			ptresi[i] = ptdst[i] - pt + pt.normalized() * r;
			continue;
		}
		ptresi[i] = center + axis * (hh * sgnnz(pt * axis)); // ..cap
		ptresi[i] += (ptdst[i] - ptresi[i]).normalized() * r;
	}
	i = bLine & isneg((ptresi[1] - ptdst[1]).len2() - (ptresi[0] - ptdst[0]).len2());
	ptres[0] = ptresi[i];
	ptres[1] = ptdst[i];
	return 1;
}

int CCapsuleGeom::UnprojectSphere(Vec3 center, float r, float rsep, contact* pcontact)
{
	float hh = m_cyl.axis * (center - m_cyl.center);
	if (fabs_tpl(hh) < m_cyl.hh)
	{
		pcontact->n = center - m_cyl.center;
		pcontact->n -= m_cyl.axis * (m_cyl.axis * pcontact->n);
		if (pcontact->n.len2() > sqr(m_cyl.r + r))
		{
			return 0;
		}
		pcontact->pt = m_cyl.center + m_cyl.axis * hh + pcontact->n.normalize() * m_cyl.r;
		pcontact->iFeature[0] = 0x40;
		return 1;
	}

	Vec3 ccap = m_cyl.center + m_cyl.axis * (m_cyl.hh * sgnnz(hh));
	if ((ccap - center).len2() > sqr(m_cyl.r + r))
	{
		return 0;
	}
	pcontact->n = (center - ccap).normalized();
	pcontact->pt = ccap + pcontact->n * m_cyl.r;
	pcontact->iFeature[0] = 0x42 - isneg(hh);
	return 1;
}

float CCapsuleGeom::CalculateBuoyancy(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& massCenter)
{
	return CCylinderGeom::CalculateBuoyancy(pplane, pgwd, massCenter);
}

void CCapsuleGeom::CalculateMediumResistance(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& dPres,
                                             Vec3& dLres)
{
	Vec3 n, rotax, center, dPcap, dLcap, ptside[4];
	float sina, r, hh, x0, y0, x1, y1, dx;
	int i;
	CSphereGeom sph;
	dPres.zero();
	dLres.zero();

	r = m_cyl.r * pgwd->scale;
	hh = m_cyl.hh * pgwd->scale;
	n = pgwd->R * -m_cyl.axis;
	rotax = n ^ Vec3(0, 0, 1);
	sina = rotax.len();
	if (sina > 0.001f)
	{
		rotax /= sina;
	}
	else
	{
		rotax.Set(1, 0, 0);
	}
	center = pgwd->R * m_cyl.center * pgwd->scale + pgwd->offset + n * hh;

	x1 = 0.965925826f;
	y1 = 0.258819045f; // 15 degrees sin/cos
	ptside[0] = Vec3(x0 = r, y0 = 0, 0).GetRotated(rotax, n.z, -sina) + center;
	for (i = 0; i < 12; i++)
	{
		ptside[1] = ptside[0];
		dx = x0;
		x0 = (x0 * sqrt3 - y0) * 0.5f;
		y0 = (y0 * sqrt3 + dx) * 0.5f;
		ptside[0] = Vec3(x0, y0, 0).GetRotated(rotax, n.z, -sina) + center;
		ptside[2] = ptside[1] - n * (hh * 2);
		ptside[3] = ptside[0] - n * (hh * 2);
		CalcMediumResistance(ptside, 4, Vec3(x1, y1, 0).GetRotated(rotax, n.z, -sina), *pplane, pgwd->v,
		                     pgwd->w, pgwd->centerOfMass, dPres, dLres);
		dx = x1;
		x1 = (x1 * sqrt3 - y1) * 0.5f;
		y1 = (y1 * sqrt3 + dx) * 0.5f;
	}

	sph.m_sphere.r = m_cyl.r;
	sph.m_sphere.center = m_cyl.center + m_cyl.axis * m_cyl.hh;
	sph.CalculateMediumResistance(pplane, pgwd, dPcap, dLcap);
	dPres += dPcap;
	dLres += dLcap;
	sph.m_sphere.center = m_cyl.center - m_cyl.axis * m_cyl.hh;
	sph.CalculateMediumResistance(pplane, pgwd, dPcap, dLcap);
	dPres += dPcap;
	dLres += dLcap;
}
