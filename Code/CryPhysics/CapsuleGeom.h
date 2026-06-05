#pragma once

#include "CylinderGeom.h"

class CCapsuleGeom final : public CCylinderGeom
{
public:
	CCapsuleGeom() = default;

	CCapsuleGeom* CreateCapsule(primitives::capsule* pcaps);

	int GetType() override { return GEOM_CAPSULE; }
	void SetData(const primitives::primitive* pcaps) override { CreateCapsule((primitives::capsule*)pcaps); }
	int PreparePrimitive(geom_world_data* pgwd, primitives::primitive*& pprim, int iCaller = 0) override
	{
		CCylinderGeom::PreparePrimitive(pgwd, pprim, iCaller);
		return primitives::capsule::type;
	}
	int CalcPhysicalProperties(phys_geometry* pgeom) override;
	int FindClosestPoint(geom_world_data* pgwd, int& iPrim, int& iFeature, const Vec3& ptdst0, const Vec3& ptdst1,
	                     Vec3* ptres, int nMaxIters) override;
	int PointInsideStatus(const Vec3& pt) override;
	float CalculateBuoyancy(const primitives::plane* pplane, const geom_world_data* pgwd,
	                        Vec3& massCenter) override;
	void CalculateMediumResistance(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& dPres,
	                               Vec3& dLres) override;
	int UnprojectSphere(Vec3 center, float r, float rsep, contact* pcontact) override;

	float GetVolume() override
	{
		return (sqr(m_cyl.r) * m_cyl.hh * (g_PI * 2)) + ((4.0f / 3 * g_PI) * cube(m_cyl.r));
	}

	int PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
	                               geometry_under_test* pGTestColl, bool bKeepPrevContacts = false) override;
	int GetUnprojectionCandidates(int iop, const contact* pcontact, primitives::primitive*& pprim, int*& piFeature,
	                              geometry_under_test* pGTest) override;
};
