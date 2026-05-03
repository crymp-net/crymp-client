#include "AABBTree.h"
#include "IntersectData.h"
#include "TriMesh.h"
#include "Utils.h"

constexpr int AABB_VER = 1;

struct AABBnode
{
	unsigned int ichild;
	unsigned char minx;
	unsigned char maxx;
	unsigned char miny;
	unsigned char maxy;
	unsigned char minz;
	unsigned char maxz;
	unsigned char ntris;
	unsigned char bSingleColl;
};

CAABBTree::~CAABBTree()
{
	if (m_pNodes)
	{
		delete[] m_pNodes;
	}
	m_pNodes = nullptr;

	if (m_pTri2Node)
	{
		delete[] m_pTri2Node;
	}
	m_pTri2Node = nullptr;
}

void CAABBTree::SetParams(int nMinTrisPerNode, int nMaxTrisPerNode, float skipdim, const Matrix33& Basis)
{
	m_nMinTrisPerNode = nMinTrisPerNode;
	m_nMaxTrisPerNode = nMaxTrisPerNode;
	m_maxSkipDim = skipdim;
	m_Basis = Basis;
	m_bOriented = m_Basis.IsIdentity() ^ 1;
}

float CAABBTree::Build(CGeometry* pMesh)
{
	m_pMesh = (CTriMesh*)pMesh;
	m_pNodes = new AABBnode[m_nNodesAlloc = 32];
	m_pTri2Node = new int[m_pMesh->m_nTris];
	m_nMaxTrisInNode = 0;
	m_nNodes = 2;
	m_pNodes[0].ntris = m_pNodes[1].ntris = 0;

	Vec3 ptmin, ptmax, pt;
	ptmin = ptmax = m_Basis * m_pMesh->m_pVertices[m_pMesh->m_pIndices[0]];
	int i;
	for (i = 1; i < m_pMesh->m_nTris * 3; i++)
	{
		pt = m_Basis * m_pMesh->m_pVertices[m_pMesh->m_pIndices[i]];
		ptmin.x = min(ptmin.x, pt.x);
		ptmax.x = max(ptmax.x, pt.x);
		ptmin.y = min(ptmin.y, pt.y);
		ptmax.y = max(ptmax.y, pt.y);
		ptmin.z = min(ptmin.z, pt.z);
		ptmax.z = max(ptmax.z, pt.z);
	}
	m_size = (ptmax - ptmin) * 0.5f;
	m_center = ((ptmax + ptmin) * 0.5f) * m_Basis;
	float mindim = max(max(m_size.x, m_size.y), m_size.z);
	m_maxSkipDim *= mindim;
	mindim *= 0.001f;
	m_size.Set(max_safe(m_size.x, mindim), max_safe(m_size.y, mindim), max_safe(m_size.z, mindim));

	float volume = BuildNode(0, 0, m_pMesh->m_nTris, m_Basis * m_center, m_size, 0);

	if (m_nNodesAlloc > m_nNodes)
	{
		AABBnode* pNodes = m_pNodes;
		memcpy(m_pNodes = new AABBnode[m_nNodesAlloc = m_nNodes], pNodes, sizeof(AABBnode) * m_nNodes);
		delete[] pNodes;
	}
	m_nBitsLog = m_nNodes <= 256 ? 3 : 4;
	int* pNewTri2Node = new int[((m_pMesh->m_nTris - 1) >> (5 - m_nBitsLog)) + 1];
	memset(pNewTri2Node, 0, sizeof(int) * (((m_pMesh->m_nTris - 1) >> (5 - m_nBitsLog)) + 1));
	for (i = 0; i < m_pMesh->m_nTris; i++)
	{
		pNewTri2Node[i >> (5 - m_nBitsLog)] |= m_pTri2Node[i]
		                                       << ((i & (1 << (5 - m_nBitsLog)) - 1) << m_nBitsLog);
	}
	delete[] m_pTri2Node;
	m_pTri2Node = pNewTri2Node;

	return (volume * 8) + (isneg(63 - m_nMaxTrisInNode) * 1E10f);
}

float CAABBTree::BuildNode(int iNode, int iTriStart, int nTris, Vec3 center, Vec3 size, int nDepth)
{
	int i, j;
	Vec3 ptmin(VMAX), ptmax(VMIN), pt, rsize;
	float mindim = max(max(size.x, size.y), size.z) * 0.001f;
	for (i = iTriStart * 3; i < (iTriStart + nTris) * 3; i++)
	{
		pt = m_Basis * m_pMesh->m_pVertices[m_pMesh->m_pIndices[i]];
		ptmin.x = min_safe(ptmin.x, pt.x);
		ptmax.x = max_safe(ptmax.x, pt.x);
		ptmin.y = min_safe(ptmin.y, pt.y);
		ptmax.y = max_safe(ptmax.y, pt.y);
		ptmin.z = min_safe(ptmin.z, pt.z);
		ptmax.z = max_safe(ptmax.z, pt.z);
	}
	ptmin += size - center;
	ptmax += size - center;

	if (size.x > mindim)
	{
		rsize.x = (0.5f * 128) / size.x;
		m_pNodes[iNode].minx = physics_float2int(min(max((ptmin.x * rsize.x) - 0.5f, 0.0f), 127.0f));
		m_pNodes[iNode].maxx = physics_float2int(min(max((ptmax.x * rsize.x) - 0.5f, 0.0f), 127.0f));
		m_pNodes[iNode].minx = min((int)m_pNodes[iNode].minx, (int)m_pNodes[iNode].maxx);
		m_pNodes[iNode].maxx = max((int)m_pNodes[iNode].minx, (int)m_pNodes[iNode].maxx);
		ptmin.x = m_pNodes[iNode].minx * size.x * (2.0f / 128);
		ptmax.x = (m_pNodes[iNode].maxx + 1) * size.x * (2.0f / 128);
		center.x += ((ptmax.x + ptmin.x) * 0.5f) - size.x;
		size.x = (ptmax.x - ptmin.x) * 0.5f;
	}
	else
	{
		m_pNodes[iNode].minx = 0;
		m_pNodes[iNode].maxx = 127;
	}
	if (size.y > mindim)
	{
		rsize.y = (0.5f * 128) / size.y;
		m_pNodes[iNode].miny = physics_float2int(min(max((ptmin.y * rsize.y) - 0.5f, 0.0f), 127.0f));
		m_pNodes[iNode].maxy = physics_float2int(min(max((ptmax.y * rsize.y) - 0.5f, 0.0f), 127.0f));
		m_pNodes[iNode].miny = min((int)m_pNodes[iNode].miny, (int)m_pNodes[iNode].maxy);
		m_pNodes[iNode].maxy = max((int)m_pNodes[iNode].miny, (int)m_pNodes[iNode].maxy);
		ptmin.y = m_pNodes[iNode].miny * size.y * (2.0f / 128);
		ptmax.y = (m_pNodes[iNode].maxy + 1) * size.y * (2.0f / 128);
		center.y += ((ptmax.y + ptmin.y) * 0.5f) - size.y;
		size.y = (ptmax.y - ptmin.y) * 0.5f;
	}
	else
	{
		m_pNodes[iNode].miny = 0;
		m_pNodes[iNode].maxy = 127;
	}
	if (size.z > mindim)
	{
		rsize.z = (0.5f * 128) / size.z;
		m_pNodes[iNode].minz = physics_float2int(min(max((ptmin.z * rsize.z) - 0.5f, 0.0f), 127.0f));
		m_pNodes[iNode].maxz = physics_float2int(min(max((ptmax.z * rsize.z) - 0.5f, 0.0f), 127.0f));
		m_pNodes[iNode].minz = min((int)m_pNodes[iNode].minz, (int)m_pNodes[iNode].maxz);
		m_pNodes[iNode].maxz = max((int)m_pNodes[iNode].minz, (int)m_pNodes[iNode].maxz);
		ptmin.z = m_pNodes[iNode].minz * size.z * (2.0f / 128);
		ptmax.z = (m_pNodes[iNode].maxz + 1) * size.z * (2.0f / 128);
		center.z += ((ptmax.z + ptmin.z) * 0.5f) - size.z;
		size.z = (ptmax.z - ptmin.z) * 0.5f;
	}
	else
	{
		m_pNodes[iNode].minz = 0;
		m_pNodes[iNode].maxz = 127;
	}
	// center += (ptmax+ptmin)*0.5f-size; size = (ptmax-ptmin)*0.5f;

	if (nTris <= m_nMaxTrisPerNode)
	{
		m_pNodes[iNode].ichild = iTriStart;
		m_pNodes[iNode].ntris = nTris;
		m_nMaxTrisInNode = max(m_nMaxTrisInNode, nTris);
		m_pNodes[iNode].bSingleColl = isneg(max(max(size.x, size.y), size.z) - m_maxSkipDim);
		for (i = iTriStart; i < iTriStart + nTris; i++)
		{
			m_pTri2Node[i] = iNode;
		}
		return size.GetVolume();
	}

	volatile int iAxis;
	int numtris[3], nTrisAx[3], iPart{}, iMode[3], idx;
	float x0, x1, x2, cx, xlim[2], bounds[3][2], diff[3], axdiff[3];
	Vec3 axis, c;

	for (iAxis = 0; iAxis < 3; iAxis++)
	{
		if (size[iAxis] < mindim)
		{
			axdiff[iAxis] = -1E10f;
			nTrisAx[iAxis] = 0;
			continue;
		}
		axis = m_Basis.GetRow(iAxis);
		cx = center[iAxis];
		bounds[0][0] = bounds[1][0] = bounds[2][0] = -size[iAxis];
		bounds[0][1] = bounds[1][1] = bounds[2][1] = size[iAxis];
		numtris[0] = numtris[1] = numtris[2] = 0;
		for (i = iTriStart; i < iTriStart + nTris; i++)
		{
			c = m_pMesh->m_pVertices[m_pMesh->m_pIndices[i * 3]] +
			    m_pMesh->m_pVertices[m_pMesh->m_pIndices[(i * 3) + 1]] +
			    m_pMesh->m_pVertices[m_pMesh->m_pIndices[(i * 3) + 2]];
			x0 = m_pMesh->m_pVertices[m_pMesh->m_pIndices[(i * 3) + 0]] * axis - cx;
			x1 = m_pMesh->m_pVertices[m_pMesh->m_pIndices[(i * 3) + 1]] * axis - cx;
			x2 = m_pMesh->m_pVertices[m_pMesh->m_pIndices[(i * 3) + 2]] * axis - cx;
			xlim[0] = min(min(x0, x1), x2);
			xlim[1] = max(max(x0, x1), x2);
			iPart = isneg(xlim[1]) ^ 1; // mode0: group all triangles that are entirely below center
			bounds[0][iPart] = minmax(bounds[0][iPart], xlim[iPart ^ 1], iPart ^ 1);
			numtris[0] += iPart;
			iPart = isnonneg(xlim[0]); // mode1: group all triangles that are entirely above center
			bounds[1][iPart] = minmax(bounds[1][iPart], xlim[iPart ^ 1], iPart ^ 1);
			numtris[1] += iPart;
			iPart = isnonneg(
			    (((c * axis) * (1.0f / 3)) - cx)); // mode2: sort triangles basing on centroids only
			bounds[2][iPart] = minmax(bounds[2][iPart], xlim[iPart ^ 1], iPart ^ 1);
			numtris[2] += iPart;
		}
		for (i = 0; i < 3; i++)
		{
			diff[i] = bounds[i][1] - bounds[i][0] -
			          (size[iAxis] * ((isneg(numtris[i] - m_nMinTrisPerNode) |
			                           isneg(nTris - numtris[i] - m_nMinTrisPerNode)) *
			                          8));
		}
		iMode[iAxis] = idxmax3(diff);
		nTrisAx[iAxis] = numtris[iMode[iAxis]];
		axdiff[iAxis] = diff[iMode[iAxis]] * size[dec_mod3[iAxis]] * size[inc_mod3[iAxis]];
	}

	iAxis = idxmax3(axdiff);
	axis = m_Basis.GetRow(iAxis);
	cx = center[iAxis];
	for (i = j = iTriStart; i < iTriStart + nTris; i++)
	{
		x0 = m_pMesh->m_pVertices[m_pMesh->m_pIndices[(i * 3) + 0]] * axis - cx;
		x1 = m_pMesh->m_pVertices[m_pMesh->m_pIndices[(i * 3) + 1]] * axis - cx;
		x2 = m_pMesh->m_pVertices[m_pMesh->m_pIndices[(i * 3) + 2]] * axis - cx;

		switch (iMode[iAxis])
		{
			case 0:
				iPart = isneg(max(max(x0, x1), x2)) ^ 1;
				break;
			case 1:
				iPart = isnonneg(min(min(x0, x1), x2));
				break;
			case 2:
				iPart = isnonneg(x0 + x1 + x2);
		}
		if (iPart == 0)
		{
			// swap triangles
			idx = m_pMesh->m_pIndices[(i * 3) + 0];
			m_pMesh->m_pIndices[(i * 3) + 0] = m_pMesh->m_pIndices[(j * 3) + 0];
			m_pMesh->m_pIndices[(j * 3) + 0] = idx;
			idx = m_pMesh->m_pIndices[(i * 3) + 1];
			m_pMesh->m_pIndices[(i * 3) + 1] = m_pMesh->m_pIndices[(j * 3) + 1];
			m_pMesh->m_pIndices[(j * 3) + 1] = idx;
			idx = m_pMesh->m_pIndices[(i * 3) + 2];
			m_pMesh->m_pIndices[(i * 3) + 2] = m_pMesh->m_pIndices[(j * 3) + 2];
			m_pMesh->m_pIndices[(j * 3) + 2] = idx;
			if (m_pMesh->m_pIds)
			{
				idx = m_pMesh->m_pIds[i];
				m_pMesh->m_pIds[i] = m_pMesh->m_pIds[j];
				m_pMesh->m_pIds[j] = idx;
			}
			if (m_pMesh->m_pForeignIdx)
			{
				idx = m_pMesh->m_pForeignIdx[i];
				m_pMesh->m_pForeignIdx[i] = m_pMesh->m_pForeignIdx[j];
				m_pMesh->m_pForeignIdx[j] = idx;
			}
			if (m_pMesh->m_pIdxNew2Old)
			{
				idx = m_pMesh->m_pIdxNew2Old[i];
				m_pMesh->m_pIdxNew2Old[i] = m_pMesh->m_pIdxNew2Old[j];
				m_pMesh->m_pIdxNew2Old[j] = idx;
			}
			j++;
		}
	}
	j -= iTriStart;

	if (j < m_nMinTrisPerNode || j > nTris - m_nMinTrisPerNode)
	{
		m_pNodes[iNode].ichild = iTriStart;
		m_pNodes[iNode].ntris = nTris;
		m_nMaxTrisInNode = max(m_nMaxTrisInNode, nTris);
		m_pNodes[iNode].bSingleColl = isneg(max(max(size.x, size.y), size.z) - m_maxSkipDim);
		for (i = iTriStart; i < iTriStart + nTris; i++)
		{
			m_pTri2Node[i] = iNode;
		}
		return size.GetVolume();
	}

	// proceed with the children
	if (m_nNodes + 2 > m_nNodesAlloc)
	{
		AABBnode* pNodes = m_pNodes;
		memcpy(m_pNodes = new AABBnode[m_nNodesAlloc += 32], pNodes, m_nNodes * sizeof(AABBnode));
		delete[] pNodes;
	}

	m_pNodes[iNode].ichild = m_nNodes;
	m_pNodes[m_nNodes].ntris = m_pNodes[m_nNodes + 1].ntris = 0;
	iNode = m_nNodes;
	m_nNodes += 2;
	float volume = 0;
	volume += BuildNode(iNode + 1, iTriStart + j, nTris - j, center, size, nDepth + 1);
	volume += BuildNode(iNode, iTriStart, j, center, size, nDepth + 1);
	return volume;
}

void CAABBTree::SetGeomConvex()
{
	for (int i = 0; i < m_nNodes; i++)
	{
		m_pNodes[i].bSingleColl = 1;
	}
}

void CAABBTree::GetBBox(primitives::box* pbox)
{
	pbox->Basis = m_Basis;
	pbox->bOriented = m_bOriented;
	pbox->center = m_center;
	pbox->size = m_size;
}

void CAABBTree::GetNodeBV(BV*& pBV, int iNode, int iCaller)
{
	pBV = g_idata[iCaller].BBoxBuf + g_idata[iCaller].BBoxBufPos++;
	pBV->type = primitives::box::type;
	((BBox*)pBV)->iNode = 0;
	((BBox*)pBV)->abox.Basis = m_Basis;
	((BBox*)pBV)->abox.bOriented = m_bOriented;
	GetRootNodeDim(((BBox*)pBV)->abox.center, ((BBox*)pBV)->abox.size);
}

void CAABBTree::GetNodeBV(BV*& pBV, const Vec3& sweepdir, float sweepstep, int iNode, int iCaller)
{
	pBV = g_idata[iCaller].BBoxExtBuf;
	g_idata[iCaller].BBoxExtBufPos = 0;
	pBV->type = primitives::box::type;
	((BBoxExt*)pBV)->iNode = 0;
	primitives::box& boxstatic = ((BBoxExt*)pBV)->aboxStatic;
	boxstatic.Basis = m_Basis;
	boxstatic.bOriented = m_bOriented;
	GetRootNodeDim(boxstatic.center, boxstatic.size);
	ExtrudeBox(&boxstatic, sweepdir, sweepstep, &((BBoxExt*)pBV)->abox);
}

void CAABBTree::GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, int iNode, int iCaller)
{
	pBV = g_idata[iCaller].BBoxBuf + g_idata[iCaller].BBoxBufPos++;
	pBV->type = primitives::box::type;
	pBV->iNode = 0;
	((BBox*)pBV)->abox.Basis = m_Basis * Rw.T();
	((BBox*)pBV)->abox.bOriented = 1;
	Vec3 center, size;
	GetRootNodeDim(center, size);
	((BBox*)pBV)->abox.center = Rw * center * scalew + offsw;
	((BBox*)pBV)->abox.size = size * scalew;
}

void CAABBTree::GetNodeBV(const Matrix33& Rw, const Vec3& offsw, float scalew, BV*& pBV, const Vec3& sweepdir,
                          float sweepstep, int iNode, int iCaller)
{
}

float CAABBTree::SplitPriority(const BV* pBV)
{
	BBox* pbox = (BBox*)pBV;
	return pbox->abox.size.GetVolume() * ((m_pNodes[pbox->iNode].ntris - 1) >> 31 & 1);
}

void CAABBTree::GetNodeChildrenBVs(const Matrix33& Rw, const Vec3& offsw, float scalew, const BV* pBV_parent,
                                   BV*& pBV_child1, BV*& pBV_child2, int iCaller)
{
	BBox *bbox_parent = (BBox*)pBV_parent, *&bbox_child1 = (BBox*&)pBV_child1, *&bbox_child2 = (BBox*&)pBV_child2;
	bbox_child1 = g_idata[iCaller].BBoxBuf + g_idata[iCaller].BBoxBufPos++;
	bbox_child2 = g_idata[iCaller].BBoxBuf + g_idata[iCaller].BBoxBufPos++;
	bbox_child2->iNode = (bbox_child1->iNode = m_pNodes[bbox_parent->iNode].ichild) + 1;
	bbox_child1->type = bbox_child2->type = primitives::box::type;
	bbox_child1->abox.Basis = bbox_parent->abox.Basis;
	bbox_child2->abox.Basis = bbox_parent->abox.Basis;
	bbox_child1->abox.bOriented = bbox_child2->abox.bOriented = 1;

	int ichild = bbox_child1->iNode;
	const Vec3 size = bbox_parent->abox.size, center = bbox_parent->abox.center;
	Vec3 ptmax, ptmin;
	ptmin.x = m_pNodes[ichild].minx * size.x * (2.0 / 128);
	ptmax.x = (m_pNodes[ichild].maxx + 1) * size.x * (2.0 / 128);
	ptmin.y = m_pNodes[ichild].miny * size.y * (2.0 / 128);
	ptmax.y = (m_pNodes[ichild].maxy + 1) * size.y * (2.0 / 128);
	ptmin.z = m_pNodes[ichild].minz * size.z * (2.0 / 128);
	ptmax.z = (m_pNodes[ichild].maxz + 1) * size.z * (2.0 / 128);
	bbox_child1->abox.center = center + ((ptmax + ptmin) * 0.5f - size) * bbox_parent->abox.Basis;
	bbox_child1->abox.size = (ptmax - ptmin) * 0.5f;

	ptmin.x = m_pNodes[ichild + 1].minx * size.x * (2.0 / 128);
	ptmax.x = (m_pNodes[ichild + 1].maxx + 1) * size.x * (2.0 / 128);
	ptmin.y = m_pNodes[ichild + 1].miny * size.y * (2.0 / 128);
	ptmax.y = (m_pNodes[ichild + 1].maxy + 1) * size.y * (2.0 / 128);
	ptmin.z = m_pNodes[ichild + 1].minz * size.z * (2.0 / 128);
	ptmax.z = (m_pNodes[ichild + 1].maxz + 1) * size.z * (2.0 / 128);
	bbox_child2->abox.center = center + ((ptmax + ptmin) * 0.5f - size) * bbox_parent->abox.Basis;
	bbox_child2->abox.size = (ptmax - ptmin) * 0.5f;
}

void CAABBTree::GetNodeChildrenBVs(const BV* pBV_parent, BV*& pBV_child1, BV*& pBV_child2, int iCaller)
{
	BBox *bbox_parent = (BBox*)pBV_parent, *&bbox_child1 = (BBox*&)pBV_child1, *&bbox_child2 = (BBox*&)pBV_child2;
	bbox_child1 = g_idata[iCaller].BBoxBuf + g_idata[iCaller].BBoxBufPos++;
	bbox_child2 = g_idata[iCaller].BBoxBuf + g_idata[iCaller].BBoxBufPos++;
	bbox_child2->iNode = (bbox_child1->iNode = m_pNodes[bbox_parent->iNode].ichild) + 1;
	bbox_child1->type = bbox_child2->type = primitives::box::type;
	bbox_child1->abox.bOriented = bbox_parent->abox.bOriented;
	bbox_child2->abox.bOriented = bbox_parent->abox.bOriented;
	if (bbox_parent->abox.bOriented)
	{
		bbox_child1->abox.Basis = bbox_parent->abox.Basis;
		bbox_child2->abox.Basis = bbox_parent->abox.Basis;
	}

	int ichild = bbox_child1->iNode;
	const Vec3 size = bbox_parent->abox.size, center = bbox_parent->abox.center;
	Vec3 ptmax, ptmin;
	ptmin.x = m_pNodes[ichild].minx * size.x * (2.0 / 128);
	ptmax.x = (m_pNodes[ichild].maxx + 1) * size.x * (2.0 / 128);
	ptmin.y = m_pNodes[ichild].miny * size.y * (2.0 / 128);
	ptmax.y = (m_pNodes[ichild].maxy + 1) * size.y * (2.0 / 128);
	ptmin.z = m_pNodes[ichild].minz * size.z * (2.0 / 128);
	ptmax.z = (m_pNodes[ichild].maxz + 1) * size.z * (2.0 / 128);
	if (bbox_parent->abox.bOriented)
	{
		bbox_child1->abox.center = center + ((ptmax + ptmin) * 0.5f - size) * bbox_parent->abox.Basis;
	}
	else
	{
		bbox_child1->abox.center = center + ((ptmax + ptmin) * 0.5f - size);
	}
	bbox_child1->abox.size = (ptmax - ptmin) * 0.5f;

	ptmin.x = m_pNodes[ichild + 1].minx * size.x * (2.0 / 128);
	ptmax.x = (m_pNodes[ichild + 1].maxx + 1) * size.x * (2.0 / 128);
	ptmin.y = m_pNodes[ichild + 1].miny * size.y * (2.0 / 128);
	ptmax.y = (m_pNodes[ichild + 1].maxy + 1) * size.y * (2.0 / 128);
	ptmin.z = m_pNodes[ichild + 1].minz * size.z * (2.0 / 128);
	ptmax.z = (m_pNodes[ichild + 1].maxz + 1) * size.z * (2.0 / 128);
	if (bbox_parent->abox.bOriented)
	{
		bbox_child2->abox.center = center + ((ptmax + ptmin) * 0.5f - size) * bbox_parent->abox.Basis;
	}
	else
	{
		bbox_child2->abox.center = center + ((ptmax + ptmin) * 0.5f - size);
	}
	bbox_child2->abox.size = (ptmax - ptmin) * 0.5f;
}

void CAABBTree::GetNodeChildrenBVs(const BV* pBV_parent, const Vec3& sweepdir, float sweepstep, BV*& pBV_child1,
                                   BV*& pBV_child2, int iCaller)
{
	BBoxExt *bbox_parent = (BBoxExt*)pBV_parent, *&bbox_child1 = (BBoxExt*&)pBV_child1,
		*&bbox_child2 = (BBoxExt*&)pBV_child2;
	bbox_child1 = g_idata[iCaller].BBoxExtBuf + g_idata[iCaller].BBoxExtBufPos++;
	bbox_child2 = g_idata[iCaller].BBoxExtBuf + g_idata[iCaller].BBoxExtBufPos++;
	bbox_child2->iNode = (bbox_child1->iNode = m_pNodes[bbox_parent->iNode].ichild) + 1;
	bbox_child1->type = bbox_child2->type = primitives::box::type;
	bbox_child1->aboxStatic.bOriented = bbox_child2->aboxStatic.bOriented = bbox_parent->aboxStatic.bOriented;
	bbox_child1->aboxStatic.Basis = bbox_parent->aboxStatic.Basis;
	bbox_child2->aboxStatic.Basis = bbox_parent->aboxStatic.Basis;

	int ichild = bbox_child1->iNode;
	const Vec3 size = bbox_parent->aboxStatic.size, center = bbox_parent->aboxStatic.center;
	Vec3 ptmax, ptmin;
	ptmin.x = m_pNodes[ichild].minx * size.x * (2.0 / 128);
	ptmax.x = (m_pNodes[ichild].maxx + 1) * size.x * (2.0 / 128);
	ptmin.y = m_pNodes[ichild].miny * size.y * (2.0 / 128);
	ptmax.y = (m_pNodes[ichild].maxy + 1) * size.y * (2.0 / 128);
	ptmin.z = m_pNodes[ichild].minz * size.z * (2.0 / 128);
	ptmax.z = (m_pNodes[ichild].maxz + 1) * size.z * (2.0 / 128);
	if (bbox_parent->aboxStatic.bOriented)
	{
		bbox_child1->aboxStatic.center =
		    center + ((ptmax + ptmin) * 0.5f - size) * bbox_parent->aboxStatic.Basis;
	}
	else
	{
		bbox_child1->aboxStatic.center = center + ((ptmax + ptmin) * 0.5f - size);
	}
	bbox_child1->aboxStatic.size = (ptmax - ptmin) * 0.5f;
	ExtrudeBox(&bbox_child1->aboxStatic, sweepdir, sweepstep, &bbox_child1->abox);
	bbox_child1->abox.bOriented = 1 + bbox_child1->iNode;

	ptmin.x = m_pNodes[ichild + 1].minx * size.x * (2.0 / 128);
	ptmax.x = (m_pNodes[ichild + 1].maxx + 1) * size.x * (2.0 / 128);
	ptmin.y = m_pNodes[ichild + 1].miny * size.y * (2.0 / 128);
	ptmax.y = (m_pNodes[ichild + 1].maxy + 1) * size.y * (2.0 / 128);
	ptmin.z = m_pNodes[ichild + 1].minz * size.z * (2.0 / 128);
	ptmax.z = (m_pNodes[ichild + 1].maxz + 1) * size.z * (2.0 / 128);
	if (bbox_parent->abox.bOriented)
	{
		bbox_child2->aboxStatic.center =
		    center + ((ptmax + ptmin) * 0.5f - size) * bbox_parent->aboxStatic.Basis;
	}
	else
	{
		bbox_child2->aboxStatic.center = center + ((ptmax + ptmin) * 0.5f - size);
	}
	bbox_child2->aboxStatic.size = (ptmax - ptmin) * 0.5f;
	ExtrudeBox(&bbox_child2->aboxStatic, sweepdir, sweepstep, &bbox_child2->abox);
	bbox_child2->abox.bOriented = 1 + bbox_child2->iNode;
}

void CAABBTree::ReleaseLastBVs(int iCaller)
{
	g_idata[iCaller].BBoxBufPos -= 2;
}

void CAABBTree::ReleaseLastSweptBVs(int iCaller)
{
	g_idata[iCaller].BBoxExtBufPos -= 2;
}

int CAABBTree::GetNodeContents(int iNode, BV* pBVCollider, int bColliderUsed, int bColliderLocal,
                               geometry_under_test* pGTest, geometry_under_test* pGTestOp)
{
	return m_pMesh->GetPrimitiveList(m_pNodes[iNode].ichild, m_pNodes[iNode].ntris, pBVCollider->type, *pBVCollider,
	                                 bColliderLocal, pGTest, pGTestOp, pGTest->primbuf, pGTest->idbuf);
}

int CAABBTree::GetNodeContentsIdx(int iNode, int& iStartPrim)
{
	iStartPrim = m_pNodes[iNode].ichild;
	return m_pNodes[iNode].ntris;
}

void CAABBTree::MarkUsedTriangle(int itri, geometry_under_test* pGTest)
{
	if (!pGTest->pUsedNodesMap)
	{
		return;
	}
	int iNode = m_pTri2Node[itri >> (5 - m_nBitsLog)] >> ((itri & (1 << (5 - m_nBitsLog)) - 1) << m_nBitsLog) &
	            ((1 << (1 << m_nBitsLog)) - 1);
	if (m_pNodes[iNode].bSingleColl)
	{
		pGTest->pUsedNodesMap[iNode >> 5] |= 1 << (iNode & 31);
		pGTest->pUsedNodesIdx[pGTest->nUsedNodes = min(pGTest->nUsedNodes + 1, pGTest->nMaxUsedNodes - 1)] =
		    iNode;
		pGTest->bCurNodeUsed = 1;
	}
}

float CAABBTree::GetMaxSkipDim()
{
	return m_maxSkipDim / max(max(m_size.x, m_size.y), m_size.z);
}

void CAABBTree::GetRootNodeDim(Vec3& center, Vec3& size)
{
	Vec3 ptmin, ptmax;
	ptmin.x = m_pNodes[0].minx * m_size.x * (2.0f / 128);
	ptmax.x = (m_pNodes[0].maxx + 1) * m_size.x * (2.0f / 128);
	ptmin.y = m_pNodes[0].miny * m_size.y * (2.0f / 128);
	ptmax.y = (m_pNodes[0].maxy + 1) * m_size.y * (2.0f / 128);
	ptmin.z = m_pNodes[0].minz * m_size.z * (2.0f / 128);
	ptmax.z = (m_pNodes[0].maxz + 1) * m_size.z * (2.0f / 128);
	center = m_center + ((ptmax + ptmin) * 0.5f - m_size) * m_Basis;
	size = (ptmax - ptmin) * 0.5f;
}

int CAABBTree::PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
                                          geometry_under_test* pGTestColl)
{
	if (m_maxSkipDim <= 0 || m_pMesh->m_nTris <= 16)
	{
		pGTest->pUsedNodesMap = 0;
		pGTest->pUsedNodesIdx = 0;
		pGTest->nMaxUsedNodes = 0;
	}
	else
	{
		int mapsz = ((m_nNodes - 1) >> 5) + 1;
		if (mapsz <= (int)(sizeof(g_idata[pGTest->iCaller].UsedNodesMap) /
		                   sizeof(g_idata[pGTest->iCaller].UsedNodesMap[0])) -
		                 g_idata[pGTest->iCaller].UsedNodesMapPos)
		{
			pGTest->pUsedNodesMap =
			    g_idata[pGTest->iCaller].UsedNodesMap + g_idata[pGTest->iCaller].UsedNodesMapPos;
			g_idata[pGTest->iCaller].UsedNodesMapPos += mapsz;
		}
		else
		{
			pGTest->pUsedNodesMap = new int[mapsz];
		}
		pGTest->pUsedNodesIdx =
		    g_idata[pGTest->iCaller].UsedNodesIdx + g_idata[pGTest->iCaller].UsedNodesIdxPos;
		pGTest->nMaxUsedNodes = min(32, (sizeof(g_idata[pGTest->iCaller].UsedNodesIdx) /
		                                 sizeof(g_idata[pGTest->iCaller].UsedNodesIdx[0])) -
		                                    g_idata[pGTest->iCaller].UsedNodesIdxPos);
		g_idata[pGTest->iCaller].UsedNodesIdxPos += pGTest->nMaxUsedNodes;
	}
	pGTest->nUsedNodes = -1;
	pGTest->pUsedNodesMap = 0;
	return 1;
}

void CAABBTree::CleanupAfterIntersectionTest(geometry_under_test* pGTest)
{
	if (!pGTest->pUsedNodesMap)
	{
		return;
	}
	if ((unsigned int)(pGTest->pUsedNodesMap - g_idata[pGTest->iCaller].UsedNodesMap) >
	    (unsigned int)sizeof(g_idata[pGTest->iCaller].UsedNodesMap) /
	        sizeof(g_idata[pGTest->iCaller].UsedNodesMap[0]))
	{
		delete[] pGTest->pUsedNodesMap;
		return;
	}
	if (pGTest->nUsedNodes < pGTest->nMaxUsedNodes - 1)
	{
		for (int i = 0; i <= pGTest->nUsedNodes; i++)
		{
			pGTest->pUsedNodesMap[pGTest->pUsedNodesIdx[i] >> 5] &= ~(1 << (pGTest->pUsedNodesIdx[i] & 31));
		}
	}
	else
	{
		memset(pGTest->pUsedNodesMap, 0, (((m_nNodes - 1) >> 5) + 1) * 4);
	}
}

void CAABBTree::Save(CMemStream& stm)
{
	stm.Write(-AABB_VER);
	stm.Write(m_nNodes);
	stm.Write(m_pNodes, m_nNodes * sizeof(m_pNodes[0]));
	stm.Write(m_center);
	stm.Write(m_size);
	stm.Write(m_Basis);
	stm.Write(m_nMaxTrisInNode);
	stm.Write(m_nMinTrisPerNode);
	stm.Write(m_nMaxTrisPerNode);
	stm.Write(m_maxSkipDim);
}

void CAABBTree::Load(CMemStream& stm, CGeometry* pGeom)
{
	struct AABBnodeV0
	{
		unsigned int minx : 7;
		unsigned int maxx : 7;
		unsigned int miny : 7;
		unsigned int maxy : 7;
		unsigned int minz : 7;
		unsigned int maxz : 7;
		unsigned int ichild : 15;
		unsigned int ntris : 6;
		unsigned int bSingleColl : 1;
	};

	m_pMesh = (CTriMesh*)pGeom;
	int i, ver;
	stm.Read(ver);
	if (ver >= 0)
	{
		m_nNodes = ver;
	}
	else
	{
		stm.Read(m_nNodes);
	}
	m_nBitsLog = m_nNodes <= 256 ? 3 : 4;
	m_pNodes = new AABBnode[m_nNodesAlloc = m_nNodes];
	if (ver == -AABB_VER)
	{
		stm.ReadType(m_pNodes, m_nNodes);
	}
	else if (ver >= 0)
	{
		AABBnodeV0* pNodes = new AABBnodeV0[m_nNodes];
		stm.ReadType(pNodes, m_nNodes);
		for (i = 0; i < m_nNodes; i++)
		{
			m_pNodes[i].ichild = pNodes[i].ichild;
			m_pNodes[i].minx = pNodes[i].minx;
			m_pNodes[i].maxx = pNodes[i].maxx;
			m_pNodes[i].miny = pNodes[i].miny;
			m_pNodes[i].maxy = pNodes[i].maxy;
			m_pNodes[i].minz = pNodes[i].minz;
			m_pNodes[i].maxz = pNodes[i].maxz;
			m_pNodes[i].ntris = pNodes[i].ntris;
			m_pNodes[i].bSingleColl = pNodes[i].bSingleColl;
		}
		delete[] pNodes;
	}
	m_pTri2Node = new int[((m_pMesh->m_nTris - 1) >> (5 - m_nBitsLog)) + 1];
	memset(m_pTri2Node, 0, sizeof(int) * (((m_pMesh->m_nTris - 1) >> (5 - m_nBitsLog)) + 1));
	for (i = 0; i < m_nNodes; i++)
	{
		for (int j = 0; j < (int)m_pNodes[i].ntris; j++)
		{
			m_pTri2Node[(m_pNodes[i].ichild + j) >> (5 - m_nBitsLog)] |=
			    i << ((m_pNodes[i].ichild + j & (1 << (5 - m_nBitsLog)) - 1) << m_nBitsLog);
		}
	}

	stm.Read(m_center);
	stm.Read(m_size);
	stm.Read(m_Basis);
	m_bOriented = m_Basis.IsIdentity() ^ 1;

	stm.Read(m_nMaxTrisInNode);
	stm.Read(m_nMinTrisPerNode);
	stm.Read(m_nMaxTrisPerNode);
	stm.Read(m_maxSkipDim);
}
