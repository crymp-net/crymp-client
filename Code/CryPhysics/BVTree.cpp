#include "BVTree.h"
#include "IntersectData.h"
#include "Utils.h"

int CBVTree::PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
                                        geometry_under_test* pGTestColl)
{
	pGTest->pUsedNodesMap = 0;
	pGTest->pUsedNodesIdx = 0;
	pGTest->nMaxUsedNodes = 0;
	pGTest->nUsedNodes = -1;
	return 1;
}

void DrawBBox(IPhysRenderer* pRenderer, int idxColor, geom_world_data* gwd, CBVTree* pTree, BBox* pbbox, int maxlevel,
              int level, int iCaller)
{
	if (level < maxlevel && pTree->SplitPriority(pbbox) > 0)
	{
		BV *pbbox1, *pbbox2;
		pTree->GetNodeChildrenBVs(gwd->R, gwd->offset, gwd->scale, pbbox, pbbox1, pbbox2, iCaller);
		DrawBBox(pRenderer, idxColor, gwd, pTree, (BBox*)pbbox1, maxlevel, level + 1, iCaller);
		DrawBBox(pRenderer, idxColor, gwd, pTree, (BBox*)pbbox2, maxlevel, level + 1, iCaller);
		pTree->ReleaseLastBVs(iCaller);
		return;
	}

	Vec3 pts[8], sz;
	int i, j;

	for (i = 0; i < 8; i++)
	{
		for (j = 0; j < 3; j++)
		{
			sz[j] = pbbox->abox.size[j] * (((i >> j & 1) << 1) - 1);
		}
		pts[i] = pbbox->abox.Basis.T() * sz + pbbox->abox.center;
	}
	for (i = 0; i < 8; i++)
	{
		for (j = 0; j < 3; j++)
		{
			if (i & 1 << j)
			{
				pRenderer->DrawLine(pts[i], pts[i ^ 1 << j], idxColor);
			}
		}
	}
}
