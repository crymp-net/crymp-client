#pragma once

#include "CryCommon/CryPhysics/IPhysics.h"

using intersection_check = int (*)(const primitives::primitive*, const primitives::primitive*, prim_inters*);

extern int box_ray_intersection(const primitives::box* pbox, const primitives::ray* pray, prim_inters* pinters);
extern int ray_tri_intersection(const primitives::ray* pray, const primitives::triangle* ptri, prim_inters* pinters);

class CIntersectionChecker
{
public:
	CIntersectionChecker();

	int Check(int type1, int type2, const primitives::primitive* prim1, const primitives::primitive* prim2,
	          prim_inters* pinters);
	int CheckExists(int type1, int type2);

	intersection_check table[NPRIMS][NPRIMS];
};

extern CIntersectionChecker g_Intersector;
