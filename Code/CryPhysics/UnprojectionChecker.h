#pragma once

#include "CryCommon/CryPhysics/IPhysics.h"

struct unprojection_mode
{
	unprojection_mode()
	{
		bCheckContact = 0;
		tmin = 0;
	}

	int imode;
	Vec3 dir;        // direction or rotation axis
	Vec3 center;     // center of rotation
	float vel;       // linear or angular velocity
	float tmax;      // maximum unprojection length (not time)
	float tmin;      // minimum unprojection length
	float minPtDist; // tolerance value
	int bCheckContact;

	Matrix33 R0;
	Vec3 offset0;
};

using unprojection_check = int (*)(unprojection_mode*, const primitives::primitive*, int, const primitives::primitive*,
                                   int, contact*, geom_contact_area*);

extern int box_sphere_lin_unprojection(unprojection_mode* pmode, const primitives::box* pbox, int iFeature1,
                                       const primitives::sphere* psph, int iFeature2, contact* pcontact,
                                       geom_contact_area* parea);
extern int cylinder_sphere_lin_unprojection(unprojection_mode* pmode, const primitives::cylinder* pcyl, int iFeature1,
                                            const primitives::sphere* psph, int iFeature2, contact* pcontact,
                                            geom_contact_area* parea);

class CUnprojectionChecker
{
public:
	CUnprojectionChecker();

	int Check(unprojection_mode* pmode, int type1, int type2, const primitives::primitive* prim1, int iFeature1,
	          const primitives::primitive* prim2, int iFeature2, contact* pcontact,
	          geom_contact_area* parea = nullptr);
	int CheckExists(int imode, int type1, int type2);

	unprojection_check table[2][NPRIMS][NPRIMS];
};

extern CUnprojectionChecker g_Unprojector;
