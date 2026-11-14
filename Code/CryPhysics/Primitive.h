#pragma once

#include "Geometry.h"

class CPrimitive : public CGeometry
{
public:
	CPrimitive() { m_bIsConvex = 1; }

	int Intersect(IGeometry* pCollider, geom_world_data* pdata1, geom_world_data* pdata2,
	              intersection_params* pparams, geom_contact*& pcontacts) override;

	int IsAPrimitive() override { return 1; }
};
