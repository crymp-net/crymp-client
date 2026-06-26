#pragma once

#include "CryCommon/CryPhysics/IPhysics.h"

class COverlapChecker;
using overlap_check = int (*)(const primitives::primitive*, const primitives::primitive*, COverlapChecker*);

extern int box_box_overlap_check(const primitives::box* box1, const primitives::box* box2, COverlapChecker*);
extern int box_ray_overlap_check(const primitives::box* pbox, const primitives::ray* pray, COverlapChecker*);
extern int box_sphere_overlap_check(const primitives::box* pbox, const primitives::sphere* psph, COverlapChecker*);
extern int tri_sphere_overlap_check(const primitives::triangle* ptri, const primitives::sphere* psph, COverlapChecker*);

class COverlapChecker
{
public:
	COverlapChecker();

	void Init() { iPrevCode = -1; }

	int Check(int type1, int type2, primitives::primitive* prim1, primitives::primitive* prim2);
	int CheckExists(int type1, int type2);

	overlap_check table[NPRIMS][NPRIMS];

	int iPrevCode = -1;
	Matrix33 Basis21;
	Matrix33 Basis21abs;
};
