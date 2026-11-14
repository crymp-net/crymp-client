#pragma once

#include "CryCommon/CryMath/Cry_Math.h"
#include "CryCommon/CryPhysics/IPhysics.h" // index_t

struct ptitem2d
{
	vector2df pt;
	ptitem2d *next, *prev;
	int iContact;
};

struct edgeitem
{
	ptitem2d* pvtx;
	ptitem2d* plist;
	edgeitem *next, *prev;
	float area, areanorm2;
	edgeitem *next1, *prev1;
	int idx;
};

int qhull(strided_pointer<Vec3> pts, int npts, index_t*& pTris);
int qhull2d(ptitem2d* pts, int nVtx, edgeitem* edges);
