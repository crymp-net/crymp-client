#include "CryPhysics.h"
#include "PhysicalWorld.h"

IPhysicalWorld* CreatePhysicalWorld(ISystem* pSystem)
{
	return new CPhysicalWorld;
}

void bop_meshupdate::Release()
{
	delete[] pRemovedVtx;
	delete[] pRemovedTri;
	delete[] pNewVtx;
	delete[] pNewTri;
	delete[] pWeldedVtx;
	delete[] pTJFixes;
	delete[] pMovedBoxes;
	delete next;

	prevRef->nextRef = nextRef;
	nextRef->prevRef = prevRef;
	prevRef = this;
	nextRef = this;

	if (pMesh[0])
	{
		pMesh[0]->Release();
	}

	if (pMesh[1])
	{
		pMesh[1]->Release();
	}
}
