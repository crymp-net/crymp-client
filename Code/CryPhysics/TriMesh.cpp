#include "CryCommon/CryCore/MTPseudoRandom.h"
#include "CryCommon/CryMath/GeomQuery.h"
#include "CryCommon/CrySystem/ISystem.h"

#include "AABBTree.h"
#include "BVTree.h"
#include "OBBTree.h"
#include "SingleBoxTree.h"
#include "TriMesh.h"
#include "Utils.h"
#include "IntersectData.h"
#include "Qhull.h"
#include "RayGeom.h"
#include "UnprojectionChecker.h"
#include "OverlapChecker.h"
#include "IntersectionChecker.h"

namespace {

constexpr int MAX_LOOP = 10;
constexpr int SAFER_BOOLEANS = 1;

struct tessvtx
{
	int ivtx;
	int inext, inextBrd;
	int iprev;
	int icont; // -1 if source A, -2 if source B
	int itwin;
	int ipoly;
	float t;
	Vec3 pt;
	int flags;
};

struct tesspoly
{
	int itri;
	int ivtx;
	int ivtx0;
	int ivtxCont;
	int nvtx;
	Vec3 n;
	int id;
	float holeArea;
	float areaOrg;
	char mat;
};

template<class T>
struct SplitArray
{
	SplitArray()
	{
		sz[0] = sz[1] = 0;
		ptr[0] = ptr[1] = 0;
	}
	T* ptr[2];
	int sz[2];
	T& operator[](int idx) { return ptr[idx >> 27][idx & ~(1 << 27)]; }
	T MapSafe(int idx)
	{
		int iarr = idx >> 27, islot = idx & ~(1 << 27);
		int bValid = -inrange(islot, -1, sz[iarr]);
		int ival = ptr[iarr][islot & bValid] | ~bValid;
		return (idx & (ival >> 31)) | (ival & ~(ival >> 31));
	}
};

template<class ftype>
class vector2_w_enforcer
{
public:
	vector2_w_enforcer(Vec3_tpl<ftype>& _vec0, Vec3_tpl<ftype>& _vec1, ftype& _w) : vec0(_vec0), vec1(_vec1), w(_w)
	{
	}
	~vector2_w_enforcer()
	{
		if (w != (ftype)1.0 && fabs_tpl(w) > 1E-20)
		{
			float rw = 1.0f / w;
			vec0 *= rw;
			vec1 *= rw;
		}
	}
	Vec3_tpl<ftype>&vec0, &vec1;
	ftype& w;
};

void swap(int* v, int i1, int i2)
{
	int ti = v[i1];
	v[i1] = v[i2];
	v[i2] = ti;
}

void qsort(int* v, index_t* idx, int left, int right)
{
	if (left >= right)
	{
		return;
	}
	int i, last;
	swap(v, left, (left + right) >> 1);
	for (last = left, i = left + 1; i <= right; i++)
	{
		if (idx[v[i] * 3] < idx[v[left] * 3])
		{
			swap(v, ++last, i);
		}
	}
	swap(v, left, last);

	qsort(v, idx, left, last - 1);
	qsort(v, idx, last + 1, right);
}

template<class ftype>
void qsort(int* v, strided_pointer<ftype> pkey, int left, int right)
{
	if (left >= right)
	{
		return;
	}
	int i, last;
	swap(v, left, (left + right) >> 1);
	for (last = left, i = left + 1; i <= right; i++)
	{
		if (pkey[v[i]] < pkey[v[left]])
		{
			swap(v, ++last, i);
		}
	}
	swap(v, left, last);

	qsort(v, pkey, left, last - 1);
	qsort(v, pkey, last + 1, right);
}

void qsort(int* v, int left, int right)
{
	if (left >= right)
	{
		return;
	}
	int i, last;
	swap(v, left, (left + right) >> 1);
	for (last = left, i = left + 1; i <= right; i++)
	{
		if (v[i] < v[left])
		{
			swap(v, ++last, i);
		}
	}
	swap(v, left, last);

	qsort(v, left, last - 1);
	qsort(v, last + 1, right);
}

void update_unprojection(real t, unprojection_mode* pmode, geometry_under_test* pGTest)
{
	if (pmode->imode == 0)
	{
		pGTest->offset = pmode->offset0 + pmode->dir * t;
	}
	else
	{
		Matrix33 Rw;
		real cosa = cos_tpl(t), sina = sin_tpl(t);
		Rw.SetRotationAA(cosa, sina, pmode->dir);
		(pGTest->R = Rw) *= pmode->R0;
		pGTest->offset = Rw * (pmode->offset0 - pmode->center) + pmode->center;
	}
	pGTest->bTransformUpdated = 1;
}

float CalcPyramidVolume(const Vec3& pt0, const Vec3& pt1, const Vec3& pt2, const Vec3& pt3, Vec3& com)
{
	com = (pt0 + pt1 + pt2 + pt3) * 0.25f;
	return fabsf((pt1 - pt0 ^ pt2 - pt0) * (pt3 - pt0)) * (1.0f / 6);
}

void insertBorderVtx(tessvtx* pVtx, int ivtx0, int ivtx, const Vec3& n, int bEnd)
{
	int i;
	float t, e[4],
	    estart[4] = {0, (pVtx[ivtx0 + 1].pt - pVtx[ivtx0].pt).len2(),
	                 (pVtx[ivtx0 + 2].pt - pVtx[ivtx0 + 1].pt).len2(),
	                 (pVtx[ivtx0].pt - pVtx[ivtx0 + 2].pt).len2()};
	Vec3 ptc = (pVtx[ivtx0].pt + pVtx[ivtx0 + 1].pt + pVtx[ivtx0 + 2].pt) * (1.0f / 3), dpt = pVtx[ivtx].pt - ptc;
	e[0] = e[3] = min(estart[1], estart[3]);
	e[1] = min(estart[1], estart[2]);
	e[2] = min(estart[2], estart[3]);
	estart[3] += (estart[2] += estart[1]);

	for (i = 0;
	     i < 2 && !((pVtx[ivtx0 + i].pt - ptc ^ dpt) * n > 0 && (pVtx[ivtx0 + inc_mod3[i]].pt - ptc ^ dpt) * n < 0);
	     i++)
		;
	pVtx[ivtx].t = t =
	    (pVtx[ivtx].pt - pVtx[ivtx0 + i].pt) * (pVtx[ivtx0 + inc_mod3[i]].pt - pVtx[ivtx0 + i].pt) + estart[i];

	for (; (pVtx[ivtx0].t <= t) + (pVtx[pVtx[ivtx0].inextBrd].t > t) +
	           (pVtx[ivtx0].t > pVtx[pVtx[ivtx0].inextBrd].t) <
	       2;
	     ivtx0 = pVtx[ivtx0].inextBrd)
		;
	pVtx[ivtx].inextBrd = pVtx[ivtx0].inextBrd;
	pVtx[ivtx0].inextBrd = ivtx;
	pVtx[ivtx].flags |= (i + 1) & -bEnd;

	for (i = 0; i < 4 && fabs_tpl(t - estart[i]) > e[i] * sqr(0.005f); i++)
		;
	pVtx[ivtx].flags |= (i & 4 ^ 4 | i & -isneg(i - 3)) << vtx_instablept_log2;
}

} // unnamed namespace

CTriMesh::CTriMesh()
{
	m_nVertices = m_nTris = 0;
	m_iCollPriority = 1;
	m_pTree = 0;
	m_pTopology = 0;
	m_pIndices = 0;
	m_pIds = 0;
	m_pVertices = 0;
	m_pNormals = 0;
	m_pForeignIdx = 0;
	m_flags = 0;
	for (int i = 0; i < sizeof(m_bConvex) / sizeof(m_bConvex[0]); i++)
	{
		m_ConvexityTolerance[i] = -1;
	}
	m_nHashPlanes = 0;
	m_bMultipart = 0;
	m_nIslands = 0;
	m_pTri2Island = 0;
	m_pIslands = 0;
	MARK_UNUSED m_V, m_A, m_L, m_center;
	m_pMeshUpdate = 0;
	m_lockUpdate = 0;
	m_pIdxNew2Old = 0;
	m_pVtxMap = 0;
	m_nErrors = 0;
	m_iLastNewTriIdx = BOP_NEWIDX0;
	m_nMessyCutCount = m_nSubtracts = 0;
}

CTriMesh::~CTriMesh()
{
	if (m_pTree)
	{
		delete m_pTree;
	}
	if (m_pTopology && !(m_flags & mesh_shared_topology))
	{
		delete[] m_pTopology;
	}
	if (m_pIndices && !(m_flags & mesh_shared_idx))
	{
		delete[] m_pIndices;
	}
	if (m_pIds && !(m_flags & mesh_shared_mats))
	{
		delete[] m_pIds;
	}
	if (m_pForeignIdx && !(m_flags & mesh_shared_foreign_idx))
	{
		delete[] m_pForeignIdx;
	}
	if (m_pVertices && !(m_flags & mesh_shared_vtx))
	{
		delete[] m_pVertices.data;
	}
	if (m_pNormals && !(m_flags & mesh_shared_normals))
	{
		delete[] m_pNormals;
	}
	for (int i = 0; i < m_nHashPlanes; i++)
	{
		delete[] m_pHashGrid[i];
		delete[] m_pHashData[i];
	}
	if (m_pTri2Island)
	{
		delete[] m_pTri2Island;
	}
	if (m_pIslands)
	{
		delete[] m_pIslands;
	}
	if (m_pVtxMap)
	{
		delete[] m_pVtxMap;
	}
	if (m_pMeshUpdate)
	{
		m_pMeshUpdate->Release();
	}
}

CTriMesh* CTriMesh::CreateTriMesh(strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices,
                                  char* pIds, int* pForeignIdx, int nTris, int flags, int nMinTrisPerNode,
                                  int nMaxTrisPerNode, float favorAABB)
{
	int i;
	m_nTris = nTris;
	m_pNormals = new Vec3[m_nTris];
	m_nVertices = 0;
	for (i = 0; i < m_nTris; i++)
	{
		m_nVertices = max(m_nVertices, max(max(pIndices[i * 3], pIndices[(i * 3) + 1]), pIndices[(i * 3) + 2]));
	}
	m_nVertices++;
	m_flags = flags & (mesh_shared_vtx | mesh_shared_idx | mesh_shared_mats | mesh_shared_foreign_idx |
	                   mesh_keep_vtxmap | mesh_always_static);
	if (!(flags & mesh_shared_vtx))
	{
		m_pVertices = new Vec3[m_nVertices];
		for (i = 0; i < m_nVertices; i++)
		{
			m_pVertices[i] = pVertices[i];
		}
	}
	else
	{
		m_pVertices = strided_pointer<Vec3>((Vec3*)pVertices.data, pVertices.iStride); // removes const modifier
	}

	if (!(flags & mesh_shared_idx))
	{
		m_pIndices = new index_t[m_nTris * 3];
		for (i = 0; i < m_nTris * 3; i++)
		{
			m_pIndices[i] = pIndices[i];
		}
	}
	else
	{
		m_pIndices = (index_t*)pIndices.data;
	}

	if (!(flags & mesh_VoxelGrid))
	{
		unsigned char* pValency = new unsigned char[m_nVertices];
		memset(pValency, 0, m_nVertices);
		for (i = (nTris * 3) - 1; i >= 0; i--)
		{
			pValency[m_pIndices[i]]++;
		}
		for (m_nMaxVertexValency = i = 0; i < m_nVertices; i++)
		{
			m_nMaxVertexValency = max(m_nMaxVertexValency, (int)pValency[i]);
		}
		delete[] pValency;
	}
	else
	{
		m_nMaxVertexValency = 8;
	}

	if (!(flags & mesh_shared_mats) && pIds)
	{
		m_pIds = new char[m_nTris];
		for (i = 0; i < nTris; i++)
		{
			m_pIds[i] = pIds[i];
		}
	}
	else
	{
		m_pIds = pIds;
	}
	if (!(flags & mesh_shared_foreign_idx) && pForeignIdx)
	{
		memcpy(m_pForeignIdx = new int[m_nTris], pForeignIdx, m_nTris * sizeof(int));
	}
	else if ((flags & mesh_shared_foreign_idx) && !pForeignIdx)
	{
		m_pForeignIdx = new int[m_nTris];
		m_flags &= ~mesh_shared_foreign_idx;
		for (i = 0; i < m_nTris; i++)
		{
			m_pForeignIdx[i] = i;
		}
	}
	else
	{
		m_pForeignIdx = pForeignIdx;
	}

	int *vsort[3], j, k, *pList[4], nList[4];
	struct vtxinfo
	{
		int id;
		Vec3i isorted;
	};
	vtxinfo* pVtx;
	float coord;

	// for mesh connectivity calculations, temporarily assign same ids to the vertices that have the same
	// coordinates
	pVtx = new vtxinfo[m_nVertices];
	if (!(flags & mesh_no_vtx_merge))
	{
		for (i = 0; i < 3; i++)
		{
			vsort[i] = new int[max(m_nTris, m_nVertices)];
		}
		for (i = 0; i < 4; i++)
		{
			pList[i] = new int[m_nVertices];
		}
		for (i = 0; i < m_nVertices; i++)
		{
			vsort[0][i] = vsort[1][i] = vsort[2][i] = i;
		}
		for (j = 0; j < 3; j++)
		{
			qsort(vsort[j], strided_pointer<float>((float*)m_pVertices.data + j, m_pVertices.iStride), 0,
			      m_nVertices - 1);
			for (i = 0; i < m_nVertices; i++)
			{
				pVtx[vsort[j][i]].isorted[j] = i;
			}
		}
		for (i = 0; i < m_nVertices; i++)
		{
			pVtx[i].id = -1;
		}
		for (i = 0; i < m_nVertices; i++)
		{
			if (pVtx[i].id < 0)
			{
				for (j = 0; j < 3; j++)
				{
					for (k = pVtx[i].isorted[j], coord = m_pVertices[i][j];
					     k > 0 && m_pVertices[vsort[j][k - 1]][j] == coord; k--)
						;
					for (nList[j] = 0; k < m_nVertices && m_pVertices[vsort[j][k]][j] == coord; k++)
					{
						pList[j][nList[j]++] = vsort[j][k];
					}
					qsort(pList[j], 0, nList[j] - 1);
				}
				nList[3] = intersect_lists(pList[0], nList[0], pList[1], nList[1], pList[3]);
				nList[0] = intersect_lists(pList[3], nList[3], pList[2], nList[2], pList[0]);
				for (k = 0; k < nList[0]; k++)
				{
					pVtx[pList[0][k]].id = i;
				}
			}
		}
		for (i = 0; i < 4; i++)
		{
			delete[] pList[i];
		}
		for (i = 0; i < 3; i++)
		{
			delete[] vsort[i];
		}

		// move degenerate triangles to the end of the list
		for (i = 0, j = m_nTris; i < j;)
		{
			if (iszero(pVtx[m_pIndices[i * 3]].id ^ pVtx[m_pIndices[(i * 3) + 1]].id) |
			    iszero(pVtx[m_pIndices[(i * 3) + 1]].id ^ pVtx[m_pIndices[(i * 3) + 2]].id) |
			    iszero(pVtx[m_pIndices[(i * 3) + 2]].id ^ pVtx[m_pIndices[i * 3]].id))
			{
				j--;
				for (k = 0; k < 3; k++)
				{
					int itmp = m_pIndices[(j * 3) + k];
					m_pIndices[(j * 3) + k] = m_pIndices[(i * 3) + k];
					m_pIndices[(i * 3) + k] = itmp;
				}
				if (m_pIds)
				{
					char stmp = m_pIds[j];
					m_pIds[j] = m_pIds[i];
					m_pIds[i] = stmp;
				}
				if (m_pForeignIdx)
				{
					int itmp = m_pForeignIdx[j];
					m_pForeignIdx[j] = m_pForeignIdx[i];
					m_pForeignIdx[i] = itmp;
				}
			}
			else
			{
				i++;
			}
		}
		m_nTris = i;
	}
	else
	{
		for (i = 0; i < m_nVertices; i++)
		{
			pVtx[i].id = i;
		}
	}

	for (i = 0; i < m_nTris * 3; i++)
	{
		m_pIndices[i] = pVtx[m_pIndices[i]].id;
	}
	if (flags & (mesh_keep_vtxmap | mesh_keep_vtxmap_for_saving))
	{
		if (!(flags & mesh_keep_vtxmap))
		{
			for (i = j = 0; i < m_nVertices; i++)
			{
				j |= sqr(pVtx[i].id - i);
			}
		}
		else
		{
			j = 1;
		}
		if (j)
		{
			m_pVtxMap = new int[m_nVertices];
			for (i = 0; i < m_nVertices; i++)
			{
				m_pVtxMap[i] = pVtx[i].id;
			}
		}
	}
	delete[] pVtx;

	primitives::box bbox;
	Vec3r axes[3], center;
	if (flags & (mesh_AABB | mesh_SingleBB))
	{
		index_t* pHullTris = 0;
		int nHullTris;
		if (m_nTris <= 20 || !(nHullTris = qhull(m_pVertices, m_nVertices, pHullTris)))
		{
			nHullTris = m_nTris;
			pHullTris = m_pIndices;
		}
		ComputeMeshEigenBasis(m_pVertices, strided_pointer<index_t>(pHullTris), nHullTris, axes, center);
		if (pHullTris != m_pIndices)
		{
			delete[] pHullTris;
		}
	}

	if (flags & mesh_SingleBB)
	{
		CSingleBoxTree* pTree = new CSingleBoxTree;
		bbox.Basis = GetMtxFromBasis(axes);
		Vec3 pt, ptmin(VMAX), ptmax(VMIN);
		for (i = 0; i < m_nTris; i++)
		{
			for (j = 0; j < 3; j++)
			{
				pt = bbox.Basis * m_pVertices[m_pIndices[(i * 3) + j]];
				ptmin.x = min_safe(ptmin.x, pt.x);
				ptmin.y = min_safe(ptmin.y, pt.y);
				ptmin.z = min_safe(ptmin.z, pt.z);
				ptmax.x = max_safe(ptmax.x, pt.x);
				ptmax.y = max_safe(ptmax.y, pt.y);
				ptmax.z = max_safe(ptmax.z, pt.z);
			}
		}
		bbox.center = ((ptmin + ptmax) * bbox.Basis) * 0.5f;
		bbox.bOriented = 1;
		bbox.size = (ptmax - ptmin) * 0.5f;
		float mindim = max(max(bbox.size.x, bbox.size.y), bbox.size.z) * 0.001f;
		bbox.size.x = max(bbox.size.x, mindim);
		bbox.size.y = max(bbox.size.y, mindim);
		bbox.size.z = max(bbox.size.z, mindim);
		pTree->SetBox(&bbox);
		pTree->Build(this);
		pTree->m_nPrims = m_nTris;
		m_pTree = pTree;
	}
	else if (flags & (mesh_AABB | mesh_AABB_rotated | mesh_OBB))
	{
		CBVTree* pTrees[3];
		index_t* pTreeIndices[3];
		char* pTreeIds[3];
		int* pTreeFIdx[3];
		float volumes[3], skipdim;
		int nTrees = 0, iTreeBest;
		skipdim = flags & mesh_multicontact2 ? 0.0f : (flags & mesh_multicontact0 ? 10.0f : 0.1f);

		if (flags & mesh_AABB)
		{
			Matrix33 Basis;
			Basis.SetIdentity();
			CAABBTree* pTree = new CAABBTree;
			pTree->SetParams(nMinTrisPerNode, nMaxTrisPerNode, skipdim, Basis);
			volumes[nTrees] = (pTrees[nTrees] = pTree)->Build(this);
			++nTrees;
		}
		if (flags & mesh_AABB_rotated)
		{
			if (nTrees > 0)
			{
				memcpy(pTreeIndices[nTrees - 1] = new index_t[m_nTris * 3], m_pIndices,
				       sizeof(m_pIndices[0]) * m_nTris * 3);
				if (m_pIds)
				{
					memcpy(pTreeIds[nTrees - 1] = new char[m_nTris], m_pIds,
					       sizeof(m_pIds[0]) * m_nTris);
				}
				if (m_pForeignIdx)
				{
					memcpy(pTreeFIdx[nTrees - 1] = new int[m_nTris], m_pForeignIdx,
					       sizeof(m_pForeignIdx[0]) * m_nTris);
				}
			}
			CAABBTree* pTree = new CAABBTree;
			Matrix33 Basis = GetMtxFromBasis(axes);
			pTree->SetParams(nMinTrisPerNode, nMaxTrisPerNode, skipdim, Basis);
			volumes[nTrees] =
			    (pTrees[nTrees] = pTree)->Build(this) * 1.01f; // favor non-oriented AABBs slightly
			++nTrees;
		}
		if (flags & mesh_OBB)
		{
			if (nTrees > 0)
			{
				memcpy(pTreeIndices[nTrees - 1] = new index_t[m_nTris * 3], m_pIndices,
				       sizeof(m_pIndices[0]) * m_nTris * 3);
				if (m_pIds)
				{
					memcpy(pTreeIds[nTrees - 1] = new char[m_nTris], m_pIds,
					       sizeof(m_pIds[0]) * m_nTris);
				}
				if (m_pForeignIdx)
				{
					memcpy(pTreeFIdx[nTrees - 1] = new int[m_nTris], m_pForeignIdx,
					       sizeof(m_pForeignIdx[0]) * m_nTris);
				}
			}
			COBBTree* pTree = new COBBTree;
			pTree->SetParams(nMinTrisPerNode, nMaxTrisPerNode, skipdim);
			volumes[nTrees] = (pTrees[nTrees] = pTree)->Build(this) * favorAABB;
			++nTrees;
		}
		for (iTreeBest = 0, i = 1; i < nTrees; i++)
		{
			if (volumes[i] < volumes[iTreeBest])
			{
				iTreeBest = i;
			}
		}
		m_pTree = pTrees[iTreeBest];
		if (iTreeBest != nTrees - 1)
		{
			memcpy(m_pIndices, pTreeIndices[iTreeBest], sizeof(m_pIndices[0]) * m_nTris * 3);
			if (m_pIds)
			{
				memcpy(m_pIds, pTreeIds[iTreeBest], sizeof(m_pIds[0]) * m_nTris);
			}
			if (m_pForeignIdx)
			{
				memcpy(m_pForeignIdx, pTreeFIdx[iTreeBest], sizeof(m_pForeignIdx[0]) * m_nTris);
			}
		}
		for (i = 0; i < nTrees; i++)
		{
			if (i != iTreeBest)
			{
				delete pTrees[i];
			}
		}
		for (i = 0; i < nTrees - 1; i++)
		{
			delete[] pTreeIndices[i];
			if (m_pIds)
			{
				delete[] pTreeIds[i];
			}
			if (m_pForeignIdx)
			{
				delete[] pTreeFIdx[i];
			}
		}
	}
	m_flags &= ~(mesh_OBB | mesh_AABB | mesh_AABB_rotated | mesh_SingleBB);
	if (m_pTree)
	{
		switch (m_pTree->GetType())
		{
			case BVT_AABB:
				m_flags |= mesh_AABB;
				break;
			case BVT_OBB:
				m_flags |= mesh_OBB;
				break;
			case BVT_SINGLEBOX:
				m_flags |= mesh_SingleBB;
				break;
		}
	}

	// BVtree may have sorted indices, so calc normals and topology after the tree
	for (i = 0; i < m_nTris; i++) // precalculate normals
	{
		m_pNormals[i] = (m_pVertices[m_pIndices[(i * 3) + 1]] - m_pVertices[m_pIndices[i * 3]] ^
		                 m_pVertices[m_pIndices[(i * 3) + 2]] - m_pVertices[m_pIndices[i * 3]])
		                    .normalized();
	}

	// fill topology information - find 3 edge neighbours for every triangle
	m_pTopology = new trinfo[m_nTris];
	CalculateTopology(m_pIndices);

	if (!(flags & mesh_VoxelGrid))
	{
		// detect if we have unconnected regions (floodfill starting from the 1st tri)
		m_bMultipart = BuildIslandMap() > 1;

		if ((m_bIsConvex = IsConvex(0.02f)) && m_pTree)
		{
			m_pTree->SetGeomConvex();
		}
		if (m_pTree)
		{
			m_pTree->GetBBox(&bbox);
			m_minVtxDist = max(max(bbox.size.x, bbox.size.y), bbox.size.z) * 0.0002f;
		}
	}
	else
	{
		m_bMultipart = 1;
		m_bIsConvex = 0;
	}
	return this;
}

CTriMesh* CTriMesh::Clone(CTriMesh* src, int flags)
{
	m_flags =
	    (src->m_flags & ~(mesh_shared_vtx | mesh_shared_idx | mesh_shared_mats | mesh_shared_foreign_idx)) | flags;
	m_nTris = src->m_nTris;
	m_nVertices = src->m_nVertices;

	if (!(flags & mesh_shared_vtx))
	{
		m_pVertices = new Vec3[m_nVertices];
		for (int i = 0; i < m_nVertices; i++)
		{
			m_pVertices[i] = src->m_pVertices[i];
		}
	}
	else
	{
		m_pVertices = src->m_pVertices;
	}
	if (!(flags & mesh_shared_idx))
	{
		memcpy(m_pIndices = new index_t[m_nTris * 3], src->m_pIndices, m_nTris * 3 * sizeof(m_pIndices[0]));
	}
	else
	{
		m_pIndices = src->m_pIndices;
	}
	if (!(flags & mesh_shared_mats) && src->m_pIds)
	{
		memcpy(m_pIds = new char[m_nTris], src->m_pIds, m_nTris * sizeof(m_pIds[0]));
	}
	else
	{
		m_pIds = src->m_pIds;
	}
	if (!(flags & mesh_shared_foreign_idx) && src->m_pForeignIdx)
	{
		memcpy(m_pForeignIdx = new int[m_nTris], src->m_pForeignIdx, m_nTris * sizeof(m_pForeignIdx[0]));
	}
	else
	{
		m_pForeignIdx = src->m_pForeignIdx;
	}
	if (!(flags & mesh_shared_topology))
	{
		memcpy(m_pTopology = new trinfo[m_nTris], src->m_pTopology, m_nTris * sizeof(m_pTopology[0]));
	}
	else
	{
		m_pTopology = src->m_pTopology;
	}
	memcpy(m_pNormals = new Vec3[m_nTris], src->m_pNormals, m_nTris * sizeof(m_pNormals[0]));
	if (src->m_pVtxMap)
	{
		memcpy(m_pVtxMap = new int[m_nVertices], src->m_pVtxMap, m_nVertices * sizeof(m_pVtxMap[0]));
	}

	m_nMaxVertexValency = src->m_nMaxVertexValency;
	for (int i = 0; i < 4; i++)
	{
		m_bConvex[i] = src->m_bConvex[i];
		m_ConvexityTolerance[i] = src->m_ConvexityTolerance[i];
	}
	m_bMultipart = src->m_bMultipart;

	m_minVtxDist = src->m_minVtxDist;
	m_iCollPriority = src->m_iCollPriority;
	m_bIsConvex = src->m_bIsConvex;
	m_pForeignData = src->m_pForeignData;
	m_iForeignData = src->m_iForeignData;
	m_nSubtracts = src->m_nSubtracts;

	CMemStream stm(false);
	src->m_pTree->Save(stm);
	switch (src->m_pTree->GetType())
	{
		case BVT_OBB:
			m_pTree = new COBBTree;
			break;
		case BVT_AABB:
			m_pTree = new CAABBTree;
			break;
		case BVT_SINGLEBOX:
			m_pTree = new CSingleBoxTree;
			break;
	}
	stm.m_iPos = 0;
	if (m_pTree)
	{
		m_pTree->Load(stm, this);
	}

	return this;
}

int CTriMesh::CalculateTopology(index_t* pIndices, int bCheckOnly)
{
	int i, j, i1, j1, nTris, itri, *pTris, *pVtxTris, *pEdgeTris, nBadEdges[2] = {0, 0};
	Vec3 edge, v0, v1;
	float elen2;
	quotientf sina, minsina;
	trinfo dummy, *pTopology = !bCheckOnly ? m_pTopology : &dummy;
	memset(pTris = new int[m_nVertices + 1], 0,
	       (m_nVertices + 1) * sizeof(int)); // for each used vtx, points to the corresponding tri list start
	pVtxTris = new int[m_nTris * 4];         // holds tri lists for each used vtx
	pEdgeTris = pVtxTris + (m_nTris * 3);

	for (i = 0; i < m_nTris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			pTris[pIndices[(i * 3) + j]]++;
		}
	}
	for (i = 0; i < m_nVertices; i++)
	{
		pTris[i + 1] += pTris[i];
	}
	for (i = m_nTris - 1; i >= 0; i--)
	{
		for (j = 0; j < 3; j++)
		{
			pVtxTris[--pTris[pIndices[(i * 3) + j]]] = i;
		}
	}
	for (i = 0; i < m_nTris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			nTris = intersect_lists(pVtxTris + pTris[pIndices[(i * 3) + j]],
			                        pTris[pIndices[(i * 3) + j] + 1] - pTris[pIndices[(i * 3) + j]],
			                        pVtxTris + pTris[pIndices[(i * 3) + inc_mod3[j]]],
			                        pTris[pIndices[(i * 3) + inc_mod3[j]] + 1] -
			                            pTris[pIndices[(i * 3) + inc_mod3[j]]],
			                        pEdgeTris);
			// if (nTris!=2) - the mesh is not manifold
			if (nTris == 2)
			{
				pTopology[i & ~-bCheckOnly].ibuddy[j] = pEdgeTris[iszero(i - pEdgeTris[0])];
			}
			else
			{
				// among the other triangles sharing this edge, find the one that has it in reverse
				// order
				edge = m_pVertices[m_pIndices[(i * 3) + inc_mod3[j]]] -
				       m_pVertices[m_pIndices[(i * 3) + j]];
				v0 = m_pVertices[m_pIndices[(i * 3) + dec_mod3[j]]] -
				     m_pVertices[m_pIndices[(i * 3) + j]];
				v0 = v0 * (elen2 = edge.len2()) - edge * (edge * v0);
				itri = -1;
				minsina.set(3, 1);
				for (i1 = 0; i1 < nTris; i1++)
				{
					if (pEdgeTris[i1] != i)
					{
						for (j1 = 0; j1 < 3; j1++)
						{
							if (pIndices[(i * 3) + j] ==
							        pIndices[(pEdgeTris[i1] * 3) + inc_mod3[j1]] &&
							    pIndices[(i * 3) + inc_mod3[j]] ==
							        pIndices[(pEdgeTris[i1] * 3) + j1])
							{
								v1 = m_pVertices[m_pIndices[(pEdgeTris[i1] * 3) +
								                            dec_mod3[j1]]] -
								     m_pVertices[m_pIndices[(i * 3) + j]];
								v1 = v1 * elen2 - edge * (edge * v1);
								sina.set(sqr_signed((v0 ^ v1) * edge),
								         v0.len2() * v1.len2() * elen2);
								float denom = isneg(1e10f - sina.y) * 1e-15f;
								sina.x *= denom;
								sina.y *= denom;
								sina += isneg(sina) * 2;
								if (sina < minsina)
								{
									minsina = sina;
									itri = pEdgeTris[i1];
								}
							}
						}
					}
				}
				pTopology[i & ~-bCheckOnly].ibuddy[j] = itri;
				nBadEdges[min(nTris - 1, 1)]++;
			}
		}
	}

	delete[] pVtxTris;
	delete[] pTris;
	return !bCheckOnly ? ((m_nErrors = nBadEdges[0] + nBadEdges[1]) == 0) : nBadEdges[0] + nBadEdges[1];
}

void CTriMesh::RebuildBVTree(CBVTree* pRefTree)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	int nMinTrisPerNode = 2, nMaxTrisPerNode = 4, i, j, jnext, *pIdxOld2New;
	Vec3 n, nnext, BBox[2];
	trinfo t, tnext;
	Matrix33 Basis;
	primitives::box bbox;
	CBVTree* pTree = pRefTree ? pRefTree : m_pTree;
	pIdxOld2New = new int[m_nTris * 2];
	m_pIdxNew2Old = pIdxOld2New + m_nTris;
	for (i = 0; i < m_nTris; i++)
	{
		m_pIdxNew2Old[i] = i;
	}
	bbox.Basis.SetIdentity();
	Basis.SetIdentity();

	if (m_nTris > 0)
	{
		if (m_flags & mesh_OBB)
		{
			if (pTree)
			{
				nMinTrisPerNode = ((COBBTree*)pTree)->m_nMinTrisPerNode;
				nMaxTrisPerNode = ((COBBTree*)pTree)->m_nMaxTrisPerNode;
			}
		}
		else if (m_flags & mesh_AABB)
		{
			if (pTree)
			{
				nMinTrisPerNode = ((CAABBTree*)pTree)->m_nMinTrisPerNode;
				nMaxTrisPerNode = ((CAABBTree*)pTree)->m_nMaxTrisPerNode;
				Basis = ((CAABBTree*)pTree)->m_Basis;
			}
		}
		else if (m_nTris < 10)
		{
			if (pTree)
			{
				bbox = ((CSingleBoxTree*)pTree)->m_Box;
			}
			BBox[0] = BBox[1] = bbox.Basis * m_pVertices[m_pIndices[0]];
			for (i = 0; i < m_nTris; i++)
			{
				for (j = 0; j < 3; j++)
				{
					n = bbox.Basis * m_pVertices[m_pIndices[(i * 3) + j]];
					BBox[0] = min(BBox[0], n);
					BBox[1] = max(BBox[1], n);
				}
			}
			bbox.size = (BBox[1] - BBox[0]) * 0.5f;
			bbox.center = ((BBox[1] + BBox[0]) * 0.5f) * bbox.Basis;
		}
		else
		{
			m_flags = (m_flags & ~mesh_SingleBB) | mesh_AABB;
		}
	}
	else
	{
		m_flags = ((m_flags & ~(mesh_AABB | mesh_OBB)) | mesh_SingleBB);
		bbox.Basis.SetIdentity();
		bbox.center.zero();
		bbox.size.Set(1E-10f, 1E-10f, 1E-10f);
	}
	if (pTree == m_pTree)
	{
		delete m_pTree;
	}
	if (m_flags & mesh_OBB)
	{
		((COBBTree*)(m_pTree = new COBBTree))->SetParams(nMinTrisPerNode, nMaxTrisPerNode, 0.01f);
	}
	else if (m_flags & mesh_AABB)
	{
		((CAABBTree*)(m_pTree = new CAABBTree))->SetParams(nMinTrisPerNode, nMaxTrisPerNode, 0.01f, Basis);
	}
	else
	{
		((CSingleBoxTree*)(m_pTree = new CSingleBoxTree))->SetBox(&bbox);
		((CSingleBoxTree*)m_pTree)->m_nPrims = m_nTris;
	}
	m_pTree->Build(this);

	m_bIsConvex = IsConvex(0.02f);
	if (m_bIsConvex)
	{
		m_pTree->SetGeomConvex();
	}

	for (i = 0; i < m_nTris; i++)
	{
		pIdxOld2New[m_pIdxNew2Old[i]] = i;
	}
	for (i = 0; i < m_nTris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			m_pTopology[i].ibuddy[j] =
			    pIdxOld2New[max(0, m_pTopology[i].ibuddy[j])] | m_pTopology[i].ibuddy[j] >> 31;
		}
	}
	for (i = 0; i < m_nTris; i++)
	{
		if (pIdxOld2New[i] != i)
		{
			jnext = pIdxOld2New[i];
			nnext = m_pNormals[i];
			tnext = m_pTopology[i];
			do
			{
				j = jnext;
				n = nnext;
				t = tnext;
				nnext = m_pNormals[j];
				m_pNormals[j] = n;
				tnext = m_pTopology[j];
				m_pTopology[j] = t;
				jnext = pIdxOld2New[j];
				pIdxOld2New[j] = j;
			}
			while (j != i);
		}
	}

	delete[] pIdxOld2New;
	m_pIdxNew2Old = 0;

	for (i = 0; i < m_nHashPlanes; i++)
	{
		delete[] m_pHashGrid[i];
		delete[] m_pHashData[i];
	}
	m_nHashPlanes = 0;
}

int CTriMesh::BuildIslandMap()
{
	if (m_pIslands)
	{
		return m_nIslands;
	}

	m_nIslands = 0;
	if (m_nTris > 0)
	{
		int i, iFreeTri, itri, imask, *pTris, nTris, nTrisAlloc;
		Vec3 vtx[3];
		m_pTri2Island = new tri_flags[m_nTris];
		for (i = 0; i < m_nTris; i++)
		{
			m_pTri2Island[i].inext = i + 1;
			m_pTri2Island[i].iprev = i - 1;
			m_pTri2Island[i].bFree = 1;
		}
		m_pTri2Island[0].iprev = m_nTris - 1;
		m_pTri2Island[m_nTris - 1].inext = 0;
		iFreeTri = 0;
		m_V = 0;
		m_center.zero();
		pTris = new int[nTrisAlloc = 256];

		do
		{
			if ((m_nIslands & 3) == 0)
			{
				ReallocateList(m_pIslands, m_nIslands, m_nIslands + 4);
			}
			m_pTri2Island[pTris[0] = iFreeTri].bFree = 0;
			nTris = 1;
			m_pIslands[m_nIslands].V = 0;
			m_pIslands[m_nIslands].center.zero();
			m_pIslands[m_nIslands].iParent = m_pIslands[m_nIslands].iChild = m_pIslands[m_nIslands].iNext =
			    -1;
			m_pIslands[m_nIslands].itri = 0x7FFF;
			m_pIslands[m_nIslands].bProcessed = 0;
			m_pIslands[m_nIslands].nTris = 0;

			while (nTris)
			{
				itri = pTris[--nTris];
				m_pTri2Island[m_pTri2Island[itri].iprev].inext = m_pTri2Island[itri].inext;
				m_pTri2Island[m_pTri2Island[itri].inext].iprev = m_pTri2Island[itri].iprev;
				imask = -iszero(iFreeTri - itri);
				iFreeTri = (iFreeTri & ~imask) | (m_pTri2Island[itri].inext & imask);
				m_pTri2Island[itri].iprev = m_nIslands;
				m_pTri2Island[itri].inext = m_pIslands[m_nIslands].itri;
				m_pIslands[m_nIslands].itri = itri;
				m_pIslands[m_nIslands].nTris++;
				for (i = 0; i < 3; i++)
				{
					vtx[i] = m_pVertices[m_pIndices[(itri * 3) + i]];
				}
				m_pIslands[m_nIslands].V +=
				    (vector2df(vtx[1]) - vector2df(vtx[0]) ^ vector2df(vtx[2]) - vector2df(vtx[0])) *
				    (vtx[0].z + vtx[1].z + vtx[2].z);
				m_pIslands[m_nIslands].center += vtx[0] + vtx[1] + vtx[2];
				for (i = 0; i < 3; i++)
				{
					if (m_pTopology[itri].ibuddy[i] >= 0 &&
					    m_pTri2Island[m_pTopology[itri].ibuddy[i]].bFree)
					{
						if (nTris == nTrisAlloc)
						{
							ReallocateList(pTris, nTris, nTrisAlloc += 256);
						}
						m_pTri2Island[pTris[nTris++] = m_pTopology[itri].ibuddy[i]].bFree = 0;
					}
				}
			}
			m_V += (m_pIslands[m_nIslands].V *= 1.0f / 6);
			m_center += (m_pIslands[m_nIslands].center /= (m_pIslands[m_nIslands].nTris * 3)) *
			            m_pIslands[m_nIslands].V;
			m_nIslands++;
		}
		while (m_pTri2Island[iFreeTri].bFree);
		if (m_V > 0)
		{
			m_center /= m_V;
		}

		delete[] pTris;
	}

	return m_nIslands;
}

CTriMesh* CTriMesh::SplitIntoIslands(primitives::plane* pGround, int nPlanes, int bOriginallyMobile)
{
	WriteLock lock0(m_lockUpdate);
	if (BuildIslandMap() < 2)
	{
		return 0;
	}

	int iCaller = GetCaller();
	WriteLock lock1(g_idata[iCaller].lockIntersect);
	int i, j, imask, isle, idx, itri, ivtx, nRemovedTri = 0, *pTriMap, *pVtxMap;
	CTriMesh *pMesh = 0, *pPrevMesh = 0;
	bop_meshupdate *pmu, *pmuPrev;
	geom_contact* pcontact;
	CRayGeom aray;
	geom_world_data gwd;
	intersection_params ip;
	primitives::box bbox;
	m_pTree->GetBBox(&bbox);
	const Vec3 dirn = Vec3(0, 0, 1);
	aray.CreateRay(bbox.center, Vec3(0, 0, (bbox.size.x + bbox.size.y + bbox.size.z) * 3), &dirn);
	ip.bThreadSafe = ip.bThreadSafeMesh = 1;

	memset(pVtxMap = new int[m_nVertices], -1, m_nVertices * sizeof(int));
	memset(pTriMap = new int[m_nTris], -1, m_nTris * sizeof(int));

	for (i = 0; i < m_nIslands; i++)
	{
		m_pIslands[i].bProcessed = 1;
	}
	for (i = 0; i < m_nTris; i++)
	{
		for (idx = 0; idx < nPlanes; idx++)
		{
			for (j = 0; j < 3; j++)
			{
				m_pIslands[m_pTri2Island[i].iprev].bProcessed &= isnonneg(
				    (m_pVertices[m_pIndices[(i * 3) + j]] - pGround[idx].origin) * pGround[idx].n);
			}
		}
	}

	// for islands with negative volumes, check inside what islands they are
	for (isle = 0; isle < m_nIslands; isle++)
	{
		if (m_pIslands[isle].V < 0)
		{
			i = m_pIndices[(itri = m_pIslands[isle].itri) * 3];
			for (; itri != 0x7FFF; itri = m_pTri2Island[itri].inext)
			{
				for (j = 0; j < 3; j++)
				{
					imask = -isneg(m_pVertices[i].z - m_pVertices[m_pIndices[(itri * 3) + j]].z);
					i = (i & ~imask) | (m_pIndices[(itri * 3) + j] & imask);
				}
			}
			aray.m_ray.origin = m_pVertices[i];
			if (Intersect(&aray, &gwd, 0, &ip, pcontact))
			{
				m_pIslands[isle].iParent = m_pTri2Island[pcontact->iPrim[0]].iprev;
				m_pIslands[isle].iNext = m_pIslands[m_pIslands[isle].iParent].iChild;
				m_pIslands[m_pIslands[isle].iParent].iChild = isle;
				m_pIslands[isle].bProcessed = m_pIslands[m_pIslands[isle].iParent].bProcessed;
			}
		}
	}

	if (bOriginallyMobile)
	{
		for (i = 0, idx = 1; i < m_nIslands; i++)
		{
			idx &= m_pIslands[i].bProcessed; // skip the 1st island (thus keep it in the mesh) if all
			                                 // islands are marked for splitting
		}
	}
	else
	{
		idx = 0;
	}
	for (; idx < m_nIslands; idx++)
	{
		if (m_pIslands[idx].bProcessed && m_pIslands[idx].iParent == -1)
		{
			pMesh = new CTriMesh();
			pMesh->SetForeignData(pPrevMesh, -2);
			pPrevMesh = pMesh;
			pMesh->m_flags =
			    m_flags & ~(mesh_shared_vtx | mesh_shared_idx | mesh_shared_mats | mesh_shared_foreign_idx);
			pMesh->m_nMaxVertexValency = m_nMaxVertexValency;
			pMesh->m_bMultipart = 1 + (m_pIslands[idx].iChild >> 31); // multipart if it has inside areas
			pMesh->m_minVtxDist = m_minVtxDist;
			pMesh->m_pMeshUpdate = pmu = new bop_meshupdate;
			pmu->nextRef = m_refmu.nextRef;
			pmu->prevRef = &m_refmu;
			m_refmu.nextRef->prevRef = pmu;
			m_refmu.nextRef = pmu;
			(pmu->pMesh[0] = pMesh)->AddRef();
			(pmu->pMesh[1] = this)->AddRef();
			pMesh->m_V = m_pIslands[idx].V;
			pMesh->m_center = m_pIslands[idx].center;

			isle = idx;
			do
			{
				for (itri = m_pIslands[isle].itri, i = pMesh->m_nVertices; itri != 0x7FFF;
				     itri = m_pTri2Island[itri].inext)
				{
					for (j = 0; j < 3; j++, i -= imask)
					{
						imask = pVtxMap[m_pIndices[(itri * 3) + j]] >> 31;
						pVtxMap[m_pIndices[(itri * 3) + j]] += (i + 1) & imask;
					}
				}

				ReallocateList(pMesh->m_pVertices.data, pMesh->m_nVertices, i);
				if (m_pVtxMap)
				{
					ReallocateList(pMesh->m_pVtxMap, pMesh->m_nVertices, i);
					for (j = 0; j < i; j++)
					{
						pMesh->m_pVtxMap[j] = j;
					}
				}
				ReallocateList(pMesh->m_pIndices, pMesh->m_nTris * 3,
				               (pMesh->m_nTris + m_pIslands[isle].nTris) * 3);
				ReallocateList(pMesh->m_pNormals, pMesh->m_nTris,
				               pMesh->m_nTris + m_pIslands[isle].nTris);
				ReallocateList(pMesh->m_pTopology, pMesh->m_nTris,
				               pMesh->m_nTris + m_pIslands[isle].nTris);
				if (m_pIds)
				{
					ReallocateList(pMesh->m_pIds, pMesh->m_nTris,
					               pMesh->m_nTris + m_pIslands[isle].nTris);
				}
				if (m_pForeignIdx)
				{
					ReallocateList(pMesh->m_pForeignIdx, pMesh->m_nTris,
					               pMesh->m_nTris + m_pIslands[isle].nTris);
				}
				ReallocateList(pmu->pNewVtx, pMesh->m_nVertices, i);
				ReallocateList(pmu->pNewTri, pMesh->m_nTris, pMesh->m_nTris + m_pIslands[isle].nTris);
				pMesh->m_nVertices = pmu->nNewVtx = i;

				for (itri = m_pIslands[isle].itri, i = pMesh->m_nTris; itri != 0x7FFF;
				     itri = m_pTri2Island[itri].inext, i++)
				{
					pmu->pNewTri[i].areaOrg =
					    m_pNormals[itri] * (m_pVertices[m_pIndices[(itri * 3) + 1]] -
					                            m_pVertices[m_pIndices[itri * 3]] ^
					                        m_pVertices[m_pIndices[(itri * 3) + 2]] -
					                            m_pVertices[m_pIndices[itri * 3]]);
					for (j = 0; j < 3; j++)
					{
						pMesh->m_pIndices[(i * 3) + j] = ivtx =
						    pVtxMap[m_pIndices[(itri * 3) + j]];
						pmu->pNewVtx[ivtx].idx = ivtx;
						pMesh->m_pVertices[ivtx] = m_pVertices[m_pIndices[(itri * 3) + j]];
						pmu->pNewTri[i].iVtx[j] = -(ivtx + 1);
						pmu->pNewVtx[ivtx].iBvtx = m_pIndices[(itri * 3) + j];
						pmu->pNewVtx[ivtx].idxTri[0] = pmu->pNewVtx[ivtx].idxTri[1] = 0;
						pmu->pNewTri[i].area[j].zero()[j] = pmu->pNewTri[i].areaOrg;
					}
					pMesh->m_pNormals[i] = m_pNormals[itri];
					if (m_pIds)
					{
						pMesh->m_pIds[i] = m_pIds[itri];
					}
					if (m_pForeignIdx)
					{
						pmu->pNewTri[i].idxOrg = m_pForeignIdx[itri];
						pMesh->m_pForeignIdx[i] = BOP_NEWIDX0 + i;
					}
					pmu->pNewTri[i].idxNew = BOP_NEWIDX0 + i;
					pmu->pNewTri[i].iop = 1;
					pTriMap[itri] = i;
				}

				for (itri = m_pIslands[isle].itri, i = pMesh->m_nTris; itri != 0x7FFF;
				     itri = m_pTri2Island[itri].inext, i++)
				{
					for (j = 0; j < 3; j++)
					{
						pMesh->m_pTopology[i].ibuddy[j] =
						    pTriMap[max(0, m_pTopology[itri].ibuddy[j])] |
						    m_pTopology[itri].ibuddy[j] >> 31;
					}
				}
				pMesh->m_iLastNewTriIdx = BOP_NEWIDX0 + (pMesh->m_nTris = pmu->nNewTri = i);

				for (itri = m_pIslands[isle].itri; itri != 0x7FFF; itri = m_pTri2Island[itri].inext)
				{
					for (j = 0; j < 3; j++)
					{
						pVtxMap[m_pIndices[(itri * 3) + j]] = -1;
					}
					pTriMap[itri] = -2;
					nRemovedTri++;
				}
			}
			while ((isle = max(m_pIslands[isle].iChild, m_pIslands[isle].iNext)) >= 0);

			pMesh->RebuildBVTree(m_pTree);
		}
	}

	if (m_pForeignIdx && nRemovedTri > 0)
	{
		for (i = j = 0; i < m_nTris; i++)
		{
			j += iszero(pTriMap[i] + 2);
		}
		if (j)
		{
			pmu = new bop_meshupdate;
			pmu->pMesh[0] = pmu->pMesh[1] = this;
			AddRef();
			AddRef();
			pmu->pRemovedTri = new int[(pmu->nRemovedTri = j) + 1];
			for (i = j = 0; i < m_nTris; i++)
			{
				pmu->pRemovedTri[j] = m_pForeignIdx[i];
				j += iszero(pTriMap[i] + 2);
			}
			if (m_pMeshUpdate)
			{
				for (pmuPrev = m_pMeshUpdate; pmuPrev->next; pmuPrev = pmuPrev->next)
					;
				pmuPrev->next = pmu;
			}
			else
			{
				m_pMeshUpdate = pmu;
			}
		}
	}

	if (nRemovedTri > 0)
	{
		CompactTriangleList(pTriMap);
	}

	delete[] pTriMap;
	delete[] pVtxMap;
	delete[] m_pIslands;
	m_pIslands = 0;

	if (nRemovedTri > 0)
	{
		MARK_UNUSED m_V, m_A, m_L, m_center;
		RebuildBVTree();
	}

	return pMesh;
}

void CTriMesh::Empty()
{
	WriteLock lock(m_lockUpdate);
	if (m_nTris > 0)
	{
		bop_meshupdate *pmuPrev, *pmu = new bop_meshupdate;
		pmu->pRemovedTri = new int[(pmu->nRemovedTri = m_nTris) + 1];
		pmu->pMesh[0] = pmu->pMesh[1] = this;
		AddRef();
		AddRef();
		for (int i = 0; i < m_nTris; i++)
		{
			pmu->pRemovedTri[i] = m_pForeignIdx[i];
		}

		if (m_pMeshUpdate)
		{
			for (pmuPrev = m_pMeshUpdate; pmuPrev->next; pmuPrev = pmuPrev->next)
				;
			pmuPrev->next = pmu;
		}
		else
		{
			m_pMeshUpdate = pmu;
		}
		MARK_UNUSED m_V, m_A, m_L, m_center;
		RebuildBVTree();

		m_nTris = 0;
	}
}

void CTriMesh::CollapseTriangleToLine(int itri, int ivtx, int* pTriMap, bop_meshupdate* pmu)
{
	// welds the ivtx+1'th vertex of itri to its ivtx'th vertex
	if (itri >= 0 && pTriMap[itri] != -2)
	{
		int i, j, iprev, iedge;
		int ibuddy[3] = {m_pTopology[itri].ibuddy[dec_mod3[ivtx]], m_pTopology[itri].ibuddy[inc_mod3[ivtx]],
		                 m_pTopology[itri].ibuddy[ivtx]};

		if (!(pmu->nWeldedVtx & 31))
		{
			ReallocateList(pmu->pWeldedVtx, pmu->nWeldedVtx, pmu->nWeldedVtx + 32);
		}
		pmu->pWeldedVtx[pmu->nWeldedVtx++].set(m_pIndices[(itri * 3) + ivtx],
		                                       m_pIndices[(itri * 3) + inc_mod3[ivtx]]);

		for (i = ibuddy[1], iprev = itri, j = 0; i >= 0 && i != ibuddy[2] && j < 32;
		     i = m_pTopology[i].ibuddy[inc_mod3[iedge]], j++)
		{
			iedge = GetEdgeByBuddy(i, iprev);
			m_pIndices[(i * 3) + inc_mod3[iedge]] = m_pIndices[(itri * 3) + ivtx];
			RecalcTriNormal(i);
			iprev = i;
		}
		for (i = 0; i < 2; i++)
		{
			if (ibuddy[i] >= 0)
			{
				m_pTopology[ibuddy[i]].ibuddy[GetEdgeByBuddy(ibuddy[i], itri)] = ibuddy[i ^ 1];
			}
		}
		pTriMap[itri] = -2;
		if (!(pmu->nRemovedTri & 31))
		{
			ReallocateList(pmu->pRemovedTri, pmu->nRemovedTri, pmu->nRemovedTri + 32);
		}
		pmu->pRemovedTri[pmu->nRemovedTri++] = pmu->pRemovedVtx[itri];

		if (ibuddy[2] >= 0 && pTriMap[ibuddy[2]] != -2)
		{
			ivtx = GetEdgeByBuddy(ibuddy[2], itri);
			itri = ibuddy[2];
			ibuddy[0] = m_pTopology[itri].ibuddy[inc_mod3[ivtx]];
			ibuddy[1] = m_pTopology[itri].ibuddy[dec_mod3[ivtx]];
			for (i = 0; i < 2; i++)
			{
				if (ibuddy[i] >= 0)
				{
					m_pTopology[ibuddy[i]].ibuddy[GetEdgeByBuddy(ibuddy[i], itri)] = ibuddy[i ^ 1];
				}
			}
			pTriMap[itri] = -2;
			if (!(pmu->nRemovedTri & 31))
			{
				ReallocateList(pmu->pRemovedTri, pmu->nRemovedTri, pmu->nRemovedTri + 32);
			}
			pmu->pRemovedTri[pmu->nRemovedTri++] = pmu->pRemovedVtx[itri];
		}
	}
}

void CTriMesh::RecalcTriNormal(int i)
{
	m_pNormals[i] = (m_pVertices[m_pIndices[(i * 3) + 1]] - m_pVertices[m_pIndices[i * 3]] ^
	                 m_pVertices[m_pIndices[(i * 3) + 2]] - m_pVertices[m_pIndices[i * 3]])
	                    .normalized();
}

int CTriMesh::FilterMesh(float minlen, float minangle)
{
	int i, j, iedge, ibuddy, ibuddy1, *pTriMap, nChanges, iter = 0;
	int idxmin, idxmax, imask, ivtx[3], *pVtxMap = m_pVtxMap;
	Vec3 vtx[4];
	float len0, len1, minlen2 = sqr(minlen), minangle2 = sqr(minangle);
	bop_meshupdate mu;
	memset(pTriMap = new int[m_nTris], -1, m_nTris * sizeof(int));
	if (!pVtxMap)
	{
		pVtxMap = new int[m_nVertices];
		for (i = 0; i < m_nVertices; i++)
		{
			pVtxMap[i] = i;
		}
	}
	mu.pRemovedVtx =
	    m_pForeignIdx ? m_pForeignIdx : pTriMap; // pRemovedVtx contains a valid pointer to foreign indices

	for (i = 0, imask = 1; i < m_nTris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			if ((m_pVertices[idxmin = m_pIndices[(i * 3) + j]] -
			     m_pVertices[idxmax = m_pIndices[(i * 3) + inc_mod3[j]]])
			        .len2() < minlen2)
			{
				imask = (idxmax - idxmin) >> 31;
				idxmin ^= idxmax & imask;
				idxmax ^= idxmin & imask;
				idxmin ^= idxmax & imask;
				pVtxMap[idxmax] = idxmin | 1 << 30;
			}
		}
	}

	if (imask != 1)
	{
		for (i = 0; i < m_nVertices; i++)
		{
			for (j = i, imask = 0; pVtxMap[j] != j; imask |= pVtxMap[j], j = pVtxMap[j] & ~(1 << 30))
				;
			if (j != i && imask & 1 << 30)
			{
				pVtxMap[i] = j;
				m_pVertices[i] = m_pVertices[j];
				if (!(mu.nWeldedVtx & 31))
				{
					ReallocateList(mu.pWeldedVtx, mu.nWeldedVtx, mu.nWeldedVtx + 32);
				}
				mu.pWeldedVtx[mu.nWeldedVtx++].set(j, i);
			}
		}
		for (i = 0; i < m_nTris; i++)
		{
			for (j = 0; j < 3; j++)
			{
				ivtx[j] = pVtxMap[m_pIndices[(i * 3) + j]];
			}
			if (iszero(ivtx[0] - ivtx[1]) | iszero(ivtx[0] - ivtx[2]) | iszero(ivtx[1] - ivtx[2]))
			{
				pTriMap[i] = -2;
				if (!(mu.nRemovedTri & 31))
				{
					ReallocateList(mu.pRemovedTri, mu.nRemovedTri, mu.nRemovedTri + 32);
				}
				mu.pRemovedTri[mu.nRemovedTri++] = mu.pRemovedVtx[i];
			}
			else if ((m_pIndices[i * 3] - ivtx[0]) | (m_pIndices[(i * 3) + 1] - ivtx[1]) |
			         (m_pIndices[(i * 3) + 2] - ivtx[2]))
			{
				for (j = 0; j < 3; j++)
				{
					m_pIndices[(i * 3) + j] = ivtx[j];
				}
				RecalcTriNormal(i);
			}
		}

		CompactTriangleList(pTriMap);
		mu.pRemovedVtx = m_pForeignIdx ? m_pForeignIdx : pTriMap;
		memset(pTriMap, -1, m_nTris * sizeof(int));
		CalculateTopology(m_pIndices);
	}

	for (i = nChanges = 0; i < m_nTris; i++)
	{
		if (pTriMap[i] != -2)
		{
			for (j = 0; j < 3 && (m_pTopology[i].ibuddy[j] == -1 ||
			                      m_pTopology[i].ibuddy[j] != m_pTopology[i].ibuddy[dec_mod3[j]]);
			     j++)
			{
				vtx[j] = m_pVertices[m_pIndices[(i * 3) + j]];
			}
			if (j < 3)
			{ // remove 2 triangles if they are 'glued' together, forming a 0-thickness fin
				ibuddy = m_pTopology[i].ibuddy[j];
				for (iedge = 0; iedge < 2 && m_pTopology[ibuddy].ibuddy[iedge] == i; iedge++)
					;

				pTriMap[i] = pTriMap[ibuddy] = -2;
				if (!(mu.nRemovedTri & 31))
				{
					ReallocateList(mu.pRemovedTri, mu.nRemovedTri, mu.nRemovedTri + 32);
				}
				mu.pRemovedTri[mu.nRemovedTri++] = mu.pRemovedVtx[i];
				if (!(mu.nRemovedTri & 31))
				{
					ReallocateList(mu.pRemovedTri, mu.nRemovedTri, mu.nRemovedTri + 32);
				}
				mu.pRemovedTri[mu.nRemovedTri++] = mu.pRemovedVtx[ibuddy];

				if ((ibuddy1 = m_pTopology[i].ibuddy[inc_mod3[j]]) >= 0)
				{
					m_pTopology[ibuddy1].ibuddy[GetEdgeByBuddy(ibuddy1, i)] =
					    m_pTopology[ibuddy].ibuddy[iedge];
				}
				if ((ibuddy1 = m_pTopology[ibuddy].ibuddy[iedge]) >= 0)
				{
					m_pTopology[ibuddy1].ibuddy[GetEdgeByBuddy(ibuddy1, ibuddy)] =
					    m_pTopology[i].ibuddy[inc_mod3[j]];
				}
				nChanges++;
				continue;
			}

			for (j = 0; j < 3 && (vtx[inc_mod3[j]] - vtx[j] ^ vtx[dec_mod3[j]] - vtx[j]).len2() >
			                         (len1 = (vtx[inc_mod3[j]] - vtx[j]).len2()) *
			                             (len0 = (vtx[dec_mod3[j]] - vtx[j]).len2()) * minangle2;
			     j++)
				;
			if (j < 3)
			{
				if (!inrange(len0, len1 * 0.95f, len1 * 1.05f))
				{
					// triangle is a T-junction, split the 'base' triangle in two
					j = len0 < len1 ? dec_mod3[j] : inc_mod3[j]; // j - the junction vertex
					if ((ibuddy = m_pTopology[i].ibuddy[inc_mod3[j]]) >= 0)
					{
						iedge = GetEdgeByBuddy(ibuddy, i);
						vtx[3] = m_pVertices[m_pIndices[(ibuddy * 3) + dec_mod3[iedge]]];
						if ((vtx[j] - vtx[3] ^ vtx[dec_mod3[j]] - vtx[3])
						            .len2() > // avoid looped switching
						        (vtx[j] - vtx[3]).len2() * (vtx[dec_mod3[j]] - vtx[3]).len2() *
						            minangle2 &&
						    (vtx[j] - vtx[3] ^ vtx[inc_mod3[j]] - vtx[3]).len2() >
						        (vtx[j] - vtx[3]).len2() * (vtx[inc_mod3[j]] - vtx[3]).len2() *
						            minangle2 &&
						    (m_pTopology[ibuddy].ibuddy[inc_mod3[iedge]] |
						     m_pTopology[i].ibuddy[dec_mod3[j]]) >= 0)
						{
							m_pIndices[(i * 3) + dec_mod3[j]] =
							    m_pIndices[(ibuddy * 3) + dec_mod3[iedge]];
							m_pIndices[(ibuddy * 3) + inc_mod3[iedge]] =
							    m_pIndices[(i * 3) + j];
							if (!(mu.nTJFixes & 31))
							{
								ReallocateList(mu.pTJFixes, mu.nTJFixes,
								               mu.nTJFixes + 32);
							}
							mu.pTJFixes[mu.nTJFixes++].set(mu.pRemovedVtx[i], inc_mod3[j],
							                               mu.pRemovedVtx[ibuddy], iedge,
							                               m_pIndices[(i * 3) + j]);

							ibuddy1 = m_pTopology[ibuddy].ibuddy[inc_mod3[iedge]];
							m_pTopology[ibuddy1].ibuddy[GetEdgeByBuddy(ibuddy1, ibuddy)] =
							    i;
							ibuddy1 = m_pTopology[i].ibuddy[dec_mod3[j]];
							m_pTopology[ibuddy1].ibuddy[GetEdgeByBuddy(ibuddy1, i)] =
							    ibuddy;
							m_pTopology[i].ibuddy[inc_mod3[j]] =
							    m_pTopology[ibuddy].ibuddy[inc_mod3[iedge]];
							m_pTopology[ibuddy].ibuddy[iedge] =
							    m_pTopology[i].ibuddy[dec_mod3[j]];
							m_pTopology[i].ibuddy[dec_mod3[j]] = ibuddy;
							m_pTopology[ibuddy].ibuddy[inc_mod3[iedge]] = i;
							RecalcTriNormal(i);
							RecalcTriNormal(ibuddy);
							nChanges++;
						}
					}
				}
			}
		}
	}

	mu.pRemovedVtx = 0;
	if (mu.nWeldedVtx + mu.nRemovedTri + mu.nTJFixes)
	{
		bop_meshupdate *pbop, *pUpdate = new bop_meshupdate(mu);
		pUpdate->pMesh[0] = pUpdate->pMesh[1] = this;
		AddRef();
		AddRef();
		if (m_pMeshUpdate)
		{
			for (pbop = m_pMeshUpdate; pbop->next; pbop = pbop->next)
				;
			pbop->next = pUpdate;
		}
		else
		{
			m_pMeshUpdate = pUpdate;
		}
	}
	mu.Reset();

	if (nChanges || iter > 0)
	{
		CompactTriangleList(pTriMap);
		CalculateTopology(m_pIndices);
		MARK_UNUSED m_V, m_A, m_L, m_center;
	}
	if (m_pVtxMap != pVtxMap)
	{
		delete[] pVtxMap;
	}
	delete[] pTriMap;

	return iter;
}

void CTriMesh::CompactTriangleList(int* pTriMap)
{
	int i, j, itri, idx, imask;

	for (i = 0, itri = m_nTris - 1; true; i++, itri--)
	{
		for (; i < itri && pTriMap[i] != -2; i++)
			;
		for (; i < itri && pTriMap[itri] == -2; itri--)
		{
			pTriMap[itri] = -1;
		}
		if (i >= itri)
		{
			break;
		}
		for (j = 0; j < 3; j++)
		{
			m_pIndices[(i * 3) + j] = m_pIndices[(itri * 3) + j];
			m_pTopology[i].ibuddy[j] = m_pTopology[itri].ibuddy[j];
		}
		if (m_pIds)
		{
			m_pIds[i] = m_pIds[itri];
		}
		if (m_pForeignIdx)
		{
			m_pForeignIdx[i] = m_pForeignIdx[itri];
		}
		m_pNormals[i] = m_pNormals[itri];
		pTriMap[itri] = i;
		pTriMap[i] = -1;
	}
	itri += pTriMap[itri] & 1;
	for (i = 0; i < itri; i++)
	{
		for (j = 0; j < 3; j++)
		{
			idx = pTriMap[max(0, m_pTopology[i].ibuddy[j])];
			imask = (idx | m_pTopology[i].ibuddy[j]) >> 31;
			m_pTopology[i].ibuddy[j] = (m_pTopology[i].ibuddy[j] & imask) | (idx & ~imask);
		}
	}

	ReallocateList(m_pIndices, itri * 3, itri * 3);
	ReallocateList(m_pNormals, itri, itri);
	ReallocateList(m_pTopology, itri, itri);
	if (m_pIds)
	{
		ReallocateList(m_pIds, itri, itri);
	}
	if (m_pForeignIdx)
	{
		ReallocateList(m_pForeignIdx, itri, itri);
	}
	m_nTris = itri;
}

float CTriMesh::ComputeExtent(GeomQuery& geo, EGeomForm eForm)
{
	int i, j;
	Vec3 vtx[3];

	switch (eForm)
	{
		case GeomForm_Vertices:
			return (float)m_nVertices;
		case GeomForm_Edges:
			if (is_unused(m_L))
			{
				m_L = 0;
				for (i = 0; i < m_nTris; i++)
				{
					for (j = 0; j < 3; j++)
					{
						vtx[j] = m_pVertices[m_pIndices[(i * 3) + j]];
					}
					m_L += (vtx[1] - vtx[0]).GetLength();
					m_L += (vtx[2] - vtx[1]).GetLength();
					m_L += (vtx[0] - vtx[2]).GetLength();
				}
				m_L *= 0.5f;
			}
			return m_L;
		case GeomForm_Surface:
			if (is_unused(m_A))
			{
				m_A = 0;
				for (i = 0; i < m_nTris; i++)
				{
					for (j = 0; j < 3; j++)
					{
						vtx[j] = m_pVertices[m_pIndices[(i * 3) + j]];
					}
					m_A += ((vtx[1] - vtx[0]) ^ (vtx[2] - vtx[0])).GetLength();
				}
				m_A *= 0.5f;
			}
			return m_A;
		case GeomForm_Volume:
			if (is_unused(m_V))
			{
				m_V = 0;
				for (i = 0; i < m_nTris; i++)
				{
					for (j = 0; j < 3; j++)
					{
						vtx[j] = m_pVertices[m_pIndices[(i * 3) + j]];
					}
					m_V += (vector2df(vtx[1]) - vector2df(vtx[0]) ^
					        vector2df(vtx[2]) - vector2df(vtx[0])) *
					       (vtx[0].z + vtx[1].z + vtx[2].z);
				}
				m_V *= 1.0f / 6;
			}
			return m_V;
	}

	return {};
}

void CTriMesh::GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm)
{
	// Inaccuracy: Points distributed equally by triangle, not extent.
	int i = physics_rand(m_nTris);

	float t[3];
	TriRandomPoint(t, eForm);

	ran.vPos = m_pVertices[m_pIndices[(i * 3) + 0]] * t[0] + m_pVertices[m_pIndices[(i * 3) + 1]] * t[1] +
	           m_pVertices[m_pIndices[(i * 3) + 2]] * t[2];
	ran.vNorm = m_pNormals[i];
}

float CTriMesh::GetVolume()
{
	GeomQuery geo;
	return geo.GetExtent(this, GeomForm_Volume);
}

Vec3 CTriMesh::GetCenter()
{
	if (is_unused(m_center))
	{
		int i, j;
		m_center.zero();
		for (i = 0; i < m_nTris; i++)
		{
			for (j = 0; j < 3; j++)
			{
				m_center += m_pVertices[m_pIndices[(i * 3) + j]];
			}
		}
		if (m_nTris > 0)
		{
			m_center /= m_nTris * 3;
		}
	}
	return m_center;
}

int CTriMesh::IsConvex(float tolerance)
{
	int idx, i, j;
	for (idx = 0; idx < (sizeof(m_bConvex) / sizeof(m_bConvex[0])) - 1 && m_ConvexityTolerance[idx] >= 0 &&
	              m_ConvexityTolerance[idx] != tolerance;
	     idx++)
		;
	if (m_ConvexityTolerance[idx] == tolerance)
	{
		m_ConvexityTolerance[idx] = m_ConvexityTolerance[0];
		m_ConvexityTolerance[0] = tolerance;
		i = m_bConvex[idx];
		m_bConvex[idx] = m_bConvex[0];
		m_bConvex[0] = i;
		return i;
	}

	if (m_bMultipart)
	{
		return m_bConvex[idx] = 0;
	}

	m_ConvexityTolerance[idx] = tolerance;
	tolerance = sqr(tolerance);
	for (i = 0; i < m_nTris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			if (m_pTopology[i].ibuddy[j] >= 0)
			{
				Vec3 cross = m_pNormals[i] ^ m_pNormals[m_pTopology[i].ibuddy[j]];
				if (cross.len2() > tolerance &&
				    cross * (m_pVertices[m_pIndices[(i * 3) + inc_mod3[j]]] -
				             m_pVertices[m_pIndices[(i * 3) + j]]) <
				        0)
				{
					return m_bConvex[idx] = 0;
				}
			}
			else
			{
				return m_bConvex[idx] = 0;
			}
		}
	}
	return m_bConvex[idx] = 1;
}

int CTriMesh::CalcPhysicalProperties(phys_geometry* pgeom)
{
	pgeom->pGeom = this;

	if (!(m_flags & mesh_always_static))
	{
		ReadLock lock(m_lockUpdate);
		Vec3r center, Ibody;
		Matrix33r I;
		real Irm[9], Rframe2body[9];
		pgeom->V = ComputeMassProperties(m_pVertices, m_pIndices, m_nTris, center, I);
		if (is_unused(m_V))
		{
			m_V = pgeom->V;
		}

		matrix eigenBasis(3, 3, 0, Rframe2body);
		SetMtxStrided<real, 3, 1>(I, Irm);
		matrix(3, 3, mtx_symmetric, Irm).jacobi_transformation(eigenBasis, static_cast<double*>(Ibody));
		pgeom->origin = center;
		pgeom->Ibody = Ibody;
		pgeom->q = quaternionf((Matrix33)GetMtxStrided<real, 3, 1>(Rframe2body).T());
	}
	else
	{
		pgeom->V = 0;
		pgeom->origin.zero();
		pgeom->Ibody.zero();
		pgeom->q.SetIdentity();
	}

	return 1;
}

int CTriMesh::PrepareForIntersectionTest(geometry_under_test* pGTest, CGeometry* pCollider,
                                         geometry_under_test* pGTestColl, bool bKeepPrevContacts)
{
	const int iCaller = pGTest->iCaller;
	pGTest->pGeometry = this;
	pGTest->pBVtree = m_pTree;
	m_pTree->PrepareForIntersectionTest(pGTest, pCollider, pGTestColl);

	pGTest->typeprim = primitives::indexed_triangle::type;
	int nNodeTris = m_pTree->MaxPrimsInNode();
	if (nNodeTris <= (int)(sizeof(g_idata[iCaller].IdxTriBuf) / sizeof(g_idata[iCaller].IdxTriBuf[0])) -
	                     g_idata[iCaller].IdxTriBufPos)
	{
		pGTest->primbuf = g_idata[iCaller].IdxTriBuf + g_idata[iCaller].IdxTriBufPos;
		g_idata[iCaller].IdxTriBufPos += nNodeTris;
		pGTest->szprimbuf = nNodeTris;
	}
	else
	{
		pGTest->primbuf = new primitives::indexed_triangle[nNodeTris];
	}
	if (m_nMaxVertexValency <= (int)(sizeof(g_idata[iCaller].IdxTriBuf) / sizeof(g_idata[iCaller].IdxTriBuf[0])) -
	                               g_idata[iCaller].IdxTriBufPos)
	{
		pGTest->primbuf1 = g_idata[iCaller].IdxTriBuf + g_idata[iCaller].IdxTriBufPos;
		g_idata[iCaller].IdxTriBufPos += m_nMaxVertexValency;
		pGTest->szprimbuf1 = m_nMaxVertexValency;
	}
	else
	{
		pGTest->primbuf1 = new primitives::indexed_triangle[m_nMaxVertexValency];
	}
	if (m_nMaxVertexValency <=
	    (int)(sizeof(g_idata[pGTest->iCaller].iFeatureBuf) / sizeof(g_idata[pGTest->iCaller].iFeatureBuf[0])) -
	        g_idata[pGTest->iCaller].iFeatureBufPos)
	{
		pGTest->iFeature_buf = g_idata[pGTest->iCaller].iFeatureBuf + g_idata[pGTest->iCaller].iFeatureBufPos;
		g_idata[pGTest->iCaller].iFeatureBufPos += m_nMaxVertexValency;
	}
	else
	{
		pGTest->iFeature_buf = new int[m_nMaxVertexValency];
	}
	int szbuf = max(nNodeTris, m_nMaxVertexValency);
	if (szbuf <= (int)(sizeof(g_idata[pGTest->iCaller].IdBuf) / sizeof(g_idata[pGTest->iCaller].IdBuf[0])) -
	                 g_idata[pGTest->iCaller].IdBufPos)
	{
		pGTest->idbuf = g_idata[pGTest->iCaller].IdBuf + g_idata[pGTest->iCaller].IdBufPos;
		g_idata[pGTest->iCaller].IdBufPos += szbuf;
	}
	else
	{
		pGTest->idbuf = new char[szbuf];
	}
	pGTest->szprim = sizeof(primitives::indexed_triangle);

	if (m_nMaxVertexValency <= (int)(sizeof(g_idata[pGTest->iCaller].SurfaceDescBuf) /
	                                 sizeof(g_idata[pGTest->iCaller].SurfaceDescBuf[0])) -
	                               g_idata[pGTest->iCaller].SurfaceDescBufPos)
	{
		pGTest->surfaces = g_idata[pGTest->iCaller].SurfaceDescBuf + g_idata[pGTest->iCaller].SurfaceDescBufPos;
		g_idata[pGTest->iCaller].SurfaceDescBufPos += m_nMaxVertexValency;
	}
	else
	{
		pGTest->surfaces = new surface_desc[m_nMaxVertexValency];
	}
	if (m_nMaxVertexValency <=
	    (int)(sizeof(g_idata[pGTest->iCaller].EdgeDescBuf) / sizeof(g_idata[pGTest->iCaller].EdgeDescBuf[0])) -
	        g_idata[pGTest->iCaller].EdgeDescBufPos)
	{
		pGTest->edges = g_idata[pGTest->iCaller].EdgeDescBuf + g_idata[pGTest->iCaller].EdgeDescBufPos;
		g_idata[pGTest->iCaller].EdgeDescBufPos += m_nMaxVertexValency;
	}
	else
	{
		pGTest->edges = new edge_desc[m_nMaxVertexValency];
	}

	pGTest->minAreaEdge = 0;
	if (!bKeepPrevContacts)
	{
		g_idata[iCaller].BrdPtBufPos = g_idata[iCaller].BrdPtBufStart = 0;
	}
	g_idata[iCaller].BrdPtBufStart = g_idata[iCaller].BrdPtBufPos;

	return 1;
}

void CTriMesh::CleanupAfterIntersectionTest(geometry_under_test* pGTest)
{
	const int iCaller = pGTest->iCaller;
	m_pTree->CleanupAfterIntersectionTest(pGTest);
	if ((unsigned int)((primitives::indexed_triangle*)pGTest->primbuf - g_idata[iCaller].IdxTriBuf) >
	    (unsigned int)(sizeof(g_idata[iCaller].IdxTriBuf) / sizeof(g_idata[iCaller].IdxTriBuf[0])))
	{
		delete[] pGTest->primbuf;
	}
	if ((unsigned int)((primitives::indexed_triangle*)pGTest->primbuf1 - g_idata[iCaller].IdxTriBuf) >
	    (unsigned int)(sizeof(g_idata[iCaller].IdxTriBuf) / sizeof(g_idata[iCaller].IdxTriBuf[0])))
	{
		delete[] pGTest->primbuf1;
	}
	if ((unsigned int)(pGTest->iFeature_buf - g_idata[pGTest->iCaller].iFeatureBuf) >
	    (unsigned int)(sizeof(g_idata[pGTest->iCaller].iFeatureBuf) /
	                   sizeof(g_idata[pGTest->iCaller].iFeatureBuf[0])))
	{
		delete[] pGTest->iFeature_buf;
	}
	if ((unsigned int)(pGTest->idbuf - g_idata[pGTest->iCaller].IdBuf) >
	    (unsigned int)(sizeof(g_idata[pGTest->iCaller].IdBuf) / sizeof(g_idata[pGTest->iCaller].IdBuf[0])))
	{
		delete[] pGTest->idbuf;
	}
	if ((unsigned int)(pGTest->surfaces - g_idata[pGTest->iCaller].SurfaceDescBuf) >
	    (unsigned int)(sizeof(g_idata[pGTest->iCaller].SurfaceDescBuf) /
	                   sizeof(g_idata[pGTest->iCaller].SurfaceDescBuf[0])))
	{
		delete[] pGTest->surfaces;
	}
	if ((unsigned int)(pGTest->edges - g_idata[pGTest->iCaller].EdgeDescBuf) >
	    (unsigned int)(sizeof(g_idata[pGTest->iCaller].EdgeDescBuf) /
	                   sizeof(g_idata[pGTest->iCaller].EdgeDescBuf[0])))
	{
		delete[] pGTest->edges;
	}
}

int CTriMesh::GetPrimitive(int iPrim, primitives::primitive* pprim)
{
	if ((unsigned int)iPrim >= (unsigned int)m_nTris)
	{
		return 0;
	}
	((primitives::triangle*)pprim)->pt[0] = m_pVertices[m_pIndices[(iPrim * 3) + 0]];
	((primitives::triangle*)pprim)->pt[1] = m_pVertices[m_pIndices[(iPrim * 3) + 1]];
	((primitives::triangle*)pprim)->pt[2] = m_pVertices[m_pIndices[(iPrim * 3) + 2]];
	((primitives::triangle*)pprim)->n = m_pNormals[iPrim];
	return sizeof(primitives::triangle);
}

int CTriMesh::GetFeature(int iPrim, int iFeature, Vec3* pt)
{
	int npt = 0;
	switch (iFeature & 0x60)
	{
		case 0x40:
			pt[2] = m_pVertices[m_pIndices[(iPrim * 3) + dec_mod3[(iFeature & 0x1F)]]];
			npt++;
			[[fallthrough]];
		case 0x20:
			pt[1] = m_pVertices[m_pIndices[(iPrim * 3) + inc_mod3[(iFeature & 0x1F)]]];
			npt++;
			[[fallthrough]];
		case 0:
			pt[0] = m_pVertices[m_pIndices[(iPrim * 3) + (iFeature & 0x1F)]];
			npt++;
	}
	return npt;
}

void CTriMesh::PrepareTriangle(int itri, primitives::triangle* ptri, const geometry_under_test* pGTest)
{
	int idx = itri * 3;
	ptri->pt[0] = pGTest->R * m_pVertices[m_pIndices[idx]] * pGTest->scale + pGTest->offset;
	ptri->pt[1] = pGTest->R * m_pVertices[m_pIndices[idx + 1]] * pGTest->scale + pGTest->offset;
	ptri->pt[2] = pGTest->R * m_pVertices[m_pIndices[idx + 2]] * pGTest->scale + pGTest->offset;
	ptri->n = pGTest->R * m_pNormals[itri];
}

int CTriMesh::PreparePrimitive(geom_world_data* pgwd, primitives::primitive*& pprim, int iCaller)
{
	primitives::indexed_triangle* ptri;
	pprim = ptri = g_idata[iCaller].IdxTriBuf;
	ptri->pt[0] = pgwd->R * m_pVertices[m_pIndices[0]] * pgwd->scale + pgwd->offset;
	ptri->pt[1] = pgwd->R * m_pVertices[m_pIndices[1]] * pgwd->scale + pgwd->offset;
	ptri->pt[2] = pgwd->R * m_pVertices[m_pIndices[2]] * pgwd->scale + pgwd->offset;
	ptri->n = pgwd->R * m_pNormals[0];
	return primitives::indexed_triangle::type;
}

int CTriMesh::TraceTriangleInters(int iop, primitives::primitive* pprims[], int idx_buddy, int type_buddy,
                                  prim_inters* pinters, geometry_under_test* pGTest, border_trace* pborder)
{
	primitives::indexed_triangle* ptri = (primitives::indexed_triangle*)pprims[iop];
	int itri, itri0 = ptri->idx, itri_end, itri_prev, itri_cur, iedge, itypes[2], iter;
	Vec3 dir0, dir1;
	itypes[iop] = primitives::triangle::type;
	itypes[iop ^ 1] = type_buddy;

	do
	{
		itri = m_pTopology[itri0].ibuddy[pinters->iFeature[1][iop] & 0x1F];
		if ((itri | iop << 31) == pborder->itri_end && GetEdgeByBuddy(itri, itri0) == pborder->iedge_end ||
		    ((pborder->npt & ~-pborder->bExactBorder) > 3 &&
		     (pinters->pt[1] - pborder->pt_end).len2() < pborder->end_dist2) ||
		    itri < 0)
		{
			return 2;
		}
		if (pborder->npt >= pborder->szbuf)
		{
			return 0;
		}
		PrepareTriangle(itri, ptri, pGTest);
		// m_pTree->MarkUsedTriangle(itri, pGTest);

		if (!g_Intersector.Check(itypes[0], itypes[1], pprims[0], pprims[1], pinters))
		{
			// if we start from a point close to vertex, check a triangle fan around the vertex rather than
			// just edge owner return 0 if we fail to close the intersection region
			iedge = GetEdgeByBuddy(itri, itri0);
			itri_end = itri;
			if ((pinters->pt[1] - ptri->pt[iedge]).len2() < sqr(m_minVtxDist))
			{ // start edge vertex
				itri_end = itri0;
				itri_prev = itri;
				itri = m_pTopology[itri].ibuddy[dec_mod3[iedge]];
			}
			else if ((pinters->pt[1] - ptri->pt[inc_mod3[iedge]]).len2() < sqr(m_minVtxDist))
			{ // end edge
				itri_end = itri;
				itri_prev = itri0;
				itri = m_pTopology[itri0].ibuddy[dec_mod3[pinters->iFeature[1][iop] & 0x1F]];
			}
			else
			{
				return 0;
			}
			if (itri < 0)
			{
				return 0;
			}

			iter = 0;
			do
			{
				PrepareTriangle(itri, ptri, pGTest);
				// m_pTree->MarkUsedTriangle(itri, pGTest);
				if (g_Intersector.Check(itypes[0], itypes[1], pprims[0], pprims[1], pinters))
				{
					goto have_inters;
				}
				itri_cur = itri;
				itri = m_pTopology[itri].ibuddy[dec_mod3[GetEdgeByBuddy(itri, itri_prev)]];
				itri_prev = itri_cur;
			}
			while (itri >= 0 && itri != itri_end && ++iter < 30);

			return 0;
		}
	have_inters:
		// store information about the last intersection segment
		pborder->itri[pborder->npt][iop] = itri | pinters->iFeature[1][iop] << IFEAT_LOG2;
		pborder->itri[pborder->npt][iop ^ 1] = idx_buddy | pinters->iFeature[1][iop ^ 1] << IFEAT_LOG2;
		pborder->pt[pborder->npt] = pinters->pt[1];
		pborder->n_sum[iop] += ptri->n;
		if (fabs_tpl(pborder->n_best.z) < fabs_tpl(ptri->n.z))
		{
			pborder->n_best = ptri->n * (1 - (iop * 2));
		}
		pborder->ntris[iop]++;
		// do checks against looping in degenerate cases
		if (pborder->npt > 30 &&
		    (pborder->pt[pborder->npt] - pborder->pt[pborder->iMark]).len2() < pborder->end_dist2 * 0.01f)
		{
			return 0;
		}
		--pborder->nLoop;
		pborder->iMark += (pborder->npt - pborder->iMark) & pborder->nLoop >> 31;
		pborder->nLoop += (MAX_LOOP + 1) & pborder->nLoop >> 31;
		pborder->npt++;

		ptri->idx = itri0 = itri;
	}
	while (!(pinters->iFeature[1][iop ^ 1] & 0x80)); // iterate while triangle from another mesh is not ended

	return 1;
}

int CTriMesh::GetUnprojectionCandidates(int iop, const contact* pcontact, primitives::primitive*& pprim,
                                        int*& piFeature, geometry_under_test* pGTest)
{
	int i, itri, iFeature = pcontact->iFeature[iop];
	primitives::indexed_triangle* ptri = (primitives::indexed_triangle*)pGTest->primbuf1;
	intptr_t idmask = ~iszero_mask(m_pIds);
	char idnull = (char)-1, *ptrnull = &idnull,
	     *pIds = (char*)(((intptr_t)m_pIds & idmask) | ((intptr_t)ptrnull & ~idmask));
	itri = ((primitives::indexed_triangle*)pprim)->idx;
	PrepareTriangle(itri, ptri, pGTest);
	ptri->idx = itri;
	pprim = ptri;
	piFeature = pGTest->iFeature_buf;
	pGTest->bTransformUpdated = 0;

	if ((iFeature & 0x60) != 0)
	{ // if feature is not vertex, but is very close to one, change it to vertex
		for (i = 0; i < 3 && (ptri->pt[i] - pcontact->pt).len2() > sqr(m_minVtxDist); i++)
			;
		if (i < 3)
		{
			iFeature = 0x80 | i;
		}
	}

	switch (iFeature & 0x60)
	{
		case 0x00:
		{ // vertex - return triangle fan around it (except the previous contacting triangle)
			int itri_prev, itri0, iedge, ntris = 0;
			itri_prev = itri0 = itri;

			pGTest->surfaces[0].n = ptri->n;
			pGTest->surfaces[0].idx = itri;
			pGTest->surfaces[0].iFeature = 0x40;

			iedge = iFeature & 0x1F;
			pGTest->edges[0].dir = ptri->pt[inc_mod3[iedge]] - ptri->pt[iedge];
			pGTest->edges[0].n[0] = ptri->n ^ pGTest->edges[0].dir;
			pGTest->edges[0].idx = itri;
			pGTest->edges[0].iFeature = 0x20 | iedge;

			itri = m_pTopology[itri].ibuddy[dec_mod3[iFeature & 0x1F]];
			if (itri >= 0)
			{
				do
				{
					PrepareTriangle(itri, ptri + ntris, pGTest);
					ptri[ntris].idx = itri;
					pGTest->idbuf[ntris] = pIds[itri & idmask];

					iedge = GetEdgeByBuddy(itri, itri_prev);
					piFeature[ntris] = 0x80 | iedge;
					pGTest->edges[ntris + 1].dir =
					    ptri[ntris].pt[inc_mod3[iedge]] - ptri[ntris].pt[iedge];
					pGTest->edges[ntris + 1].n[0] =
					    pGTest->edges[ntris + 1].dir ^ pGTest->surfaces[ntris].n;
					pGTest->edges[ntris + 1].n[1] = ptri[ntris].n ^ pGTest->edges[ntris + 1].dir;
					pGTest->edges[ntris + 1].idx = itri;
					pGTest->edges[ntris + 1].iFeature = 0x20 | iedge;

					pGTest->surfaces[ntris + 1].n = ptri[ntris].n;
					pGTest->surfaces[ntris + 1].idx = itri;
					pGTest->surfaces[ntris + 1].iFeature = 0x40;

					itri_prev = itri;
					itri = m_pTopology[itri].ibuddy[dec_mod3[iedge]];
					ntris++;
				}
				while (itri >= 0 && itri != itri0 && ntris < pGTest->szprimbuf1 - 1);
			}

			pGTest->edges[0].n[1] = pGTest->edges[0].dir ^ ptri[max(0, ntris - 1)].n;
			pGTest->nSurfaces = ntris + 1;
			pGTest->nEdges = ntris + 1;
			pprim = ptri;
			return ntris;
		}

		case 0x20: // edge - switch to the corresponding buddy
			pGTest->edges[0].dir = ptri->pt[inc_mod3[iFeature & 0x1F]] - ptri->pt[iFeature & 0x1F];
			pGTest->edges[0].n[0] = ptri->n ^ pGTest->edges[0].dir;
			pGTest->edges[0].idx = itri;
			pGTest->edges[0].iFeature = iFeature;
			pGTest->nEdges = 1;

			pGTest->surfaces[0].n = ptri->n;
			pGTest->surfaces[0].idx = itri;
			pGTest->surfaces[0].iFeature = 0x40;

			i = itri;
			itri = m_pTopology[itri].ibuddy[iFeature & 0x1F];
			pGTest->nSurfaces = 1;
			if (itri < 0)
			{
				pGTest->edges[0].n[1] = pGTest->edges[0].n[0];
				return 0;
			}

			piFeature[0] = GetEdgeByBuddy(itri, i) | 0xA0;
			PrepareTriangle(itri, ptri, pGTest);
			pGTest->idbuf[0] = pIds[itri & idmask];
			ptri->idx = itri;

			pGTest->edges[0].n[1] = pGTest->edges[0].dir ^ ptri->n;
			pGTest->surfaces[1].n = ptri->n;
			pGTest->surfaces[1].idx = itri;
			pGTest->surfaces[1].iFeature = 0x40;
			pGTest->nSurfaces = 2;
			return 1;

		case 0x40: // face - keep the previous contacting triangle
			pGTest->surfaces[0].n = ptri->n;
			pGTest->surfaces[0].idx = itri;
			pGTest->surfaces[0].iFeature = 0x40;
			pGTest->nSurfaces = 1;
			pGTest->nEdges = 0;
			piFeature[0] = 0x40;
			return 1;
	}
	return 0;
}

int CTriMesh::GetPrimitiveList(int iStart, int nPrims, int typeCollider, primitives::primitive* pCollider,
                               int bColliderLocal, geometry_under_test* pGTest, geometry_under_test* pGTestOp,
                               primitives::primitive* pRes, char* pResId)
{
	intptr_t mask;
	int i, j, iEnd = min(m_nTris, iStart + nPrims), bContact, bPivotCulling;
	char *pIds, noid = (char)-2, *pnoid = &noid;
	mask = ~iszero_mask(m_pIds);
	intptr_t inv_mask = ~mask;
	pIds = (char*)(((intptr_t)m_pIds & mask) | ((intptr_t)pnoid & inv_mask));
	bPivotCulling = isneg(pGTestOp->ptOutsidePivot.x - 1E10);

	static COverlapChecker g_Overlapper;

	if (bColliderLocal == 1) // collider is in local coordinate frame
	{
		for (i = iStart, j = 0; i < iEnd; i++, j += bContact)
		{
			primitives::indexed_triangle* ptri = (primitives::indexed_triangle*)pRes + j;
			ptri->pt[0] = m_pVertices[m_pIndices[(i * 3) + 0]];
			ptri->pt[1] = m_pVertices[m_pIndices[(i * 3) + 1]];
			ptri->pt[2] = m_pVertices[m_pIndices[(i * 3) + 2]];
			ptri->n = m_pNormals[i];
			pResId[j] = pIds[i & mask]; // material -1 signals that the triangle should be skipped
			bContact = g_Overlapper.Check(typeCollider, primitives::triangle::type, pCollider, ptri) &
			           (iszero(pResId[j] + 1) ^ 1);
			PrepareTriangle(i, ptri, pGTest);
			if (bPivotCulling)
			{
				bContact &= isneg((ptri->pt[0] - pGTestOp->ptOutsidePivot) * ptri->n);
			}
			ptri->idx = i;
		}
	}
	else if (bColliderLocal == 0) // collider is in his own coordinate frame
	{
		for (i = iStart, j = 0; i < iEnd; i++, j += bContact)
		{
			primitives::indexed_triangle* ptri = (primitives::indexed_triangle*)pRes + j;
			ptri->pt[0] = pGTest->R_rel * m_pVertices[m_pIndices[(i * 3) + 0]] * pGTest->scale_rel +
			              pGTest->offset_rel;
			ptri->pt[1] = pGTest->R_rel * m_pVertices[m_pIndices[(i * 3) + 1]] * pGTest->scale_rel +
			              pGTest->offset_rel;
			ptri->pt[2] = pGTest->R_rel * m_pVertices[m_pIndices[(i * 3) + 2]] * pGTest->scale_rel +
			              pGTest->offset_rel;
			ptri->n = pGTest->R_rel * m_pNormals[i];
			pResId[j] = pIds[i & mask];
			bContact = g_Overlapper.Check(typeCollider, primitives::triangle::type, pCollider, ptri) &
			           (iszero(pResId[j] + 1) ^ 1);
			PrepareTriangle(i, ptri, pGTest);
			if (bPivotCulling)
			{
				bContact &= isneg((ptri->pt[0] - pGTestOp->ptOutsidePivot) * ptri->n);
			}
			ptri->idx = i;
		}
	}
	else // no checks for collider, get all tris unconditionally (used in sweep check)
	{
		for (i = iStart, j = 0; i < iEnd; i++, j++)
		{
			primitives::indexed_triangle* ptri = (primitives::indexed_triangle*)pRes + j;
			PrepareTriangle(i, ptri, pGTest);
			ptri->idx = i;
		}
	}
	pGTest->bTransformUpdated = 0;
	return j;
}

int CTriMesh::RegisterIntersection(primitives::primitive* pprim1, primitives::primitive* pprim2,
                                   geometry_under_test* pGTest1, geometry_under_test* pGTest2, prim_inters* pinters)
{
	// FUNCTION_PROFILER( GetISystem(),PROFILE_PHYSICS );

	geometry_under_test* pGTest[2] = {pGTest1, pGTest2};
	primitives::indexed_triangle tri = *(primitives::indexed_triangle*)pprim1, tri1;
	primitives::primitive *pprims[2] = {&tri, pprim2}, *ptr[2];
	int iFeature_dummy = -1, *piFeature[2] = {&iFeature_dummy, &iFeature_dummy};
	int idx_prim[2] = {((primitives::indexed_triangle*)pprim1)->idx, -1};
	int bNoUnprojection = 0, bSurfaceSurfaceContact = 0, bSurfaceEdgeContact = 0, bUseLSNormal = 0;
	int i = 0, j = 0, res = 0, ipt, ibest, jbest, iop, nprims1, nprims2, nSmallSteps;
	int primitives::indexed_triangle::* pidxoffs = 0;
	const int iCaller = pGTest1->iCaller;
	float len, maxlen;
	real t{}, tmax{};
	border_trace border;
	geom_contact* pres = pGTest[0]->contacts + *pGTest[0]->pnContacts;
	Vec3r n_avg;
	Vec3 unprojPlaneNorm;
	if (pGTest[1]->typeprim == primitives::indexed_triangle::type)
	{
		idx_prim[1] = ((primitives::indexed_triangle*)pprims[1])->idx;
		pidxoffs = &primitives::indexed_triangle::idx;
	}

	if (!pGTest1->pParams->bExactBorder)
	{
		border.end_dist2 = sqr(m_minVtxDist * 2);
		for (i = g_idata[iCaller].BrdPtBufStart;
		     i < g_idata[iCaller].BrdPtBufPos &&
		     (g_idata[iCaller].BrdPtBuf[i] - pinters->pt[1]).len2() > border.end_dist2;
		     i++)
			;
		if (i < g_idata[iCaller].BrdPtBufPos)
		{
			return 0;
		}
	}
	else
	{
		border.end_dist2 = 0;
		for (i = g_idata[iCaller].BrdPtBufStart;
		     i < g_idata[iCaller].BrdPtBufPos && ((g_idata[iCaller].BrdiTriBuf[i][0] & IDXMASK) - idx_prim[0] |
		                                          (g_idata[iCaller].BrdiTriBuf[i][1] & IDXMASK) - idx_prim[1]);
		     i++)
			;
		if (i < g_idata[iCaller].BrdPtBufPos)
		{
			return 0;
		}
	}
	border.bExactBorder = pGTest1->pParams->bExactBorder;

	pres->iPrim[0] = ((primitives::indexed_triangle*)pprims[0])->idx;
	pres->iPrim[1] = ((primitives::indexed_triangle*)pprims[1])->*pidxoffs;
	pres->bBorderConsecutive = true;

	float maxcos = 1.0f, mincos2 = 0;
	if (pres->parea)
	{
		if (pGTest2->pParams->maxSurfaceGapAngle < 0.2f)
		{
			maxcos = 1.0f - (sqr(pGTest2->pParams->maxSurfaceGapAngle) * 2);
			mincos2 = sqr(pGTest2->pParams->maxSurfaceGapAngle);
		}
		else
		{
			maxcos = cos_tpl(pGTest2->pParams->maxSurfaceGapAngle);
			mincos2 = sqr(cos_tpl((g_PI * 0.5f) - pGTest2->pParams->maxSurfaceGapAngle));
		}
	}
	pres->id[0] = pinters->id[0];
	pres->id[1] = pinters->id[1];
	pres->iNode[0] = pinters->iNode[0];
	pres->iNode[1] = pinters->iNode[2];

	border.itri = g_idata[iCaller].BrdiTriBuf + g_idata[iCaller].BrdPtBufPos;
	border.seglen = g_idata[iCaller].BrdSeglenBuf;
	border.pt = g_idata[iCaller].BrdPtBuf + g_idata[iCaller].BrdPtBufPos;
	border.szbuf = min(100 + (pGTest1->pParams->bExactBorder * 900),
	                   (sizeof(g_idata[iCaller].BrdPtBuf) / sizeof(g_idata[iCaller].BrdPtBuf[0])) -
	                       g_idata[iCaller].BrdPtBufPos - 10);
	if (border.szbuf < 0)
	{
		return 0;
	}
	pres->ptborder = border.pt;
	pres->idxborder = border.itri;
	border.n_sum[0].zero();
	border.n_sum[1].zero();
	border.ntris[0] = border.ntris[1] = 0;
	border.n_best.zero();

	iop = pinters->iFeature[0][1] >> 7; // index of mesh whose primitive has boundary on intersection start
	border.pt_end = pinters->pt[0];
	border.itri_end = ((primitives::indexed_triangle*)pprims[iop])->idx | iop << 31;
	border.iedge_end = pinters->iFeature[0][iop] & 0x1F;

	pres->center = border.pt[0] = pinters->pt[0];
	border.pt[1] = pinters->pt[1];
	border.itri[0][iop] = idx_prim[iop] | pinters->iFeature[0][iop] << IFEAT_LOG2;
	border.itri[1][iop] = idx_prim[iop] | pinters->iFeature[1][iop] << IFEAT_LOG2;
	border.itri[0][iop ^ 1] = idx_prim[iop ^ 1] | pinters->iFeature[0][iop ^ 1] << IFEAT_LOG2;
	border.itri[1][iop ^ 1] = idx_prim[iop ^ 1] | pinters->iFeature[1][iop ^ 1] << IFEAT_LOG2;
	border.npt = 2;
	border.iMark = 1;
	border.nLoop = MAX_LOOP;
	iop = pinters->iFeature[1][1] >> 7; // index of mesh whose primitive has boundary on intersection end
	if (pGTest[iop ^ 1]->typeprim == primitives::indexed_triangle::type)
	{
		border.n_sum[iop ^ 1] = ((primitives::indexed_triangle*)pprims[iop ^ 1])->n;
		border.n_best = border.n_sum[iop ^ 1] * ((iop * 2) - 1);
		border.ntris[iop ^ 1] = 1;
	}

	if ((pinters->iFeature[1][0] | pinters->iFeature[1][1]) & 0x80 && !pGTest[0]->pParams->bNoBorder)
	{ // if at least one triangle ended
		// && (!pGTest1->bStopAtFirstTri || (pinters->pt[0]-pinters->pt[1]).len2()<sqr(m_minVtxDist*10))) {
		do
		{
			if (iop == 1 && pprims[1] == pprim2)
			{ // since we are about to change triangle being traced, use a local copy
				tri1 = *(primitives::indexed_triangle*)pprim2;
				pprims[1] = &tri1;
			}
			border.n_sum[iop] += ((primitives::indexed_triangle*)pprims[iop])->n;
			border.ntris[iop]++;
			if (fabs_tpl(border.n_best.z) < fabs_tpl(((primitives::indexed_triangle*)pprims[iop])->n.z))
			{
				border.n_best = ((primitives::indexed_triangle*)pprims[iop])->n * (1 - (iop * 2));
			}
			res = ((CTriMesh*)pGTest[iop]->pGeometry)
			          ->TraceTriangleInters(iop, pprims, idx_prim[iop ^ 1], pGTest[iop ^ 1]->typeprim,
			                                pinters, pGTest[iop], &border);
			pGTest1->pParams->bBothConvex &=
			    res >> 1; // if failed to close a contour - don't use optiomizations for convex objects
			idx_prim[iop] = ((primitives::indexed_triangle*)pprims[iop])->idx;
			iop ^= 1;
		}
		while (res == 1);

		if (res == 2)
		{
			for (i = 1; i < border.npt; i++)
			{
				pGTest[0]->pBVtree->MarkUsedTriangle(border.itri[i][0] & IDXMASK, pGTest[0]);
				pGTest[1]->pBVtree->MarkUsedTriangle(border.itri[i][1] & IDXMASK, pGTest[1]);
			}
		}
	}
	else
	{
		border.ntris[0] = 1;
		border.n_sum[0] = tri.n;
	};
	pres->bClosed = res >> 1;
	pres->nborderpt = border.npt;
	g_idata[iCaller].BrdPtBufPos += border.npt;
	border.n_sum[1].Flip();
	n_avg = (border.ntris[0] < border.ntris[1] && border.ntris[0] || !border.ntris[1] ? border.n_sum[0]
	                                                                                  : border.n_sum[1])
	            .normalized();
	if (fabs_tpl(border.n_best.z) < 0.95f)
	{
		border.n_best = n_avg;
	}

	unprojection_mode unproj;
	contact contact_cur, contact_best;

	Vec3 vrel_loc, vrel = pGTest[0]->v - pGTest[1]->v, pthit = border.pt[0], pthit1 = border.pt[0];
	float vrel_loc2, vrel2 = 0, dist, maxdist = 0, maxdist1 = 0;
	for (i = 0; i < border.npt; i++)
	{
		vrel_loc = pGTest[0]->v - pGTest[1]->v + (pGTest[0]->w ^ border.pt[i] - pGTest[0]->centerOfMass) -
		           (pGTest[1]->w ^ border.pt[i] - pGTest[1]->centerOfMass);
		if (vrel_loc * n_avg > 0 && (vrel_loc2 = vrel_loc.len2()) > vrel2)
		{
			vrel2 = vrel_loc2;
			vrel = vrel_loc;
		}
		if ((dist = (border.pt[i] - pGTest[0]->centerOfRotation ^ pGTest[0]->pParams->axisOfRotation).len2()) >
		    maxdist)
		{
			maxdist = dist;
			pthit = border.pt[i];
		}
		if ((dist = (border.pt[i] - pGTest[0]->centerOfRotation).len2()) > maxdist1)
		{
			maxdist1 = dist;
			pthit1 = border.pt[i];
		}
	}

	if (pGTest[0]->pParams->iUnprojectionMode <= 0)
	{
		unproj.imode = 0;
		unproj.dir = -vrel;
		if (unproj.dir.len2() < sqr(pGTest[0]->pParams->vrel_min))
		{
			unproj.R0 = pGTest[0]->R;
			unproj.offset0 = pGTest[0]->offset;
			bUseLSNormal = 1;
			goto end_unproj;
		}
		if (unproj.dir.len2() == 0 && pGTest2->pGeometry->GetType() == GEOM_RAY)
		{
			Vec3 dir = pGTest2->R * ((CRayGeom*)pGTest2->pGeometry)->m_ray.dir;
			if (n_avg * dir > 0)
			{
				return 0;
			}
			unproj.dir = ((n_avg ^ dir) ^ dir).normalized();
			unproj.vel = 1.0f;
		}
		else
		{
			unproj.dir /= (unproj.vel = unproj.dir.len());
		}
		tmax = unproj.tmax = pGTest[0]->pParams->time_interval * unproj.vel;
	}
	else
	{
		if (pGTest[0]->pParams->axisOfRotation.len2() == 0)
		{
			maxdist = maxdist1;
			pthit = pthit1;
		}
		if (maxdist < sqr(pGTest[0]->pParams->minAxisDist))
		{
			unproj.R0 = pGTest[0]->R;
			unproj.offset0 = pGTest[0]->offset;
			bUseLSNormal = 1;
			goto end_unproj;
		}
		unproj.imode = 1;
		unproj.center = pGTest[0]->centerOfRotation;
		unproj.dir = pGTest[0]->pParams->axisOfRotation;
		if (unproj.dir.len2() > 0)
		{
			unproj.dir *= -sgnnz(n_avg * (unproj.dir ^ pthit - unproj.center));
		}
		else
		{
			// unproj.dir = (pthit-unproj.center^vrel).normalized();
			// unproj.dir *= -sgnnz(n_avg*(unproj.dir^pthit-unproj.center));
			if (n_avg * (pthit - unproj.center) > 0)
			{
				return 0;
			}
			unproj.dir = (n_avg ^ pthit - unproj.center).normalized();
			pGTest[0]->pParams->axisOfRotation = unproj.dir;
		}
		unproj.vel = unproj.dir.len();
		unproj.dir /= unproj.vel;
		tmax = pGTest[0]->pParams->time_interval * unproj.vel;
		unproj.tmax = sin_tpl(min(g_PI * 0.5f, tmax));
	}
	unproj.minPtDist = min(m_minVtxDist, pGTest2->pGeometry->m_minVtxDist);
	unprojPlaneNorm = pGTest[0]->pParams->unprojectionPlaneNormal;

	do
	{
		unproj.R0 = pGTest[0]->R;
		unproj.offset0 = pGTest[0]->offset;
		bSurfaceSurfaceContact = 0;
		bSurfaceEdgeContact = 0;
		if (unproj.imode == 0 && unprojPlaneNorm.len2() > 0 && fabs_tpl(unproj.dir * unprojPlaneNorm) < 0.707f)
		{
			(unproj.dir -= unprojPlaneNorm * (unproj.dir * unprojPlaneNorm))
			    .normalize(); // do not restrict direction to requested plane if angle>pi/4
		}

		// select the intersection segment with maximum length as unprojection starting point
		ibest = border.npt - 1;
		maxlen = (border.pt[0] - border.pt[border.npt - 1]).len2();
		for (i = 0; i < border.npt - 1; i++)
		{
			if ((len = (border.pt[i] - border.pt[i + 1]).len2()) > maxlen)
			{
				maxlen = len;
				ibest = i;
			}
		}

		PrepareTriangle(border.itri[ibest][0] & IDXMASK, (primitives::triangle*)pprims[0], pGTest[0]);
		((primitives::indexed_triangle*)pprims[0])->idx = border.itri[ibest][0] & IDXMASK;
		if (border.itri[ibest][1] >= 0)
		{
			((CTriMesh*)pGTest[1]->pGeometry)
			    ->PrepareTriangle(border.itri[ibest][1] & IDXMASK, (primitives::triangle*)pprims[1],
			                      pGTest[1]);
			((primitives::indexed_triangle*)pprims[1])->idx = border.itri[ibest][1] & IDXMASK;
		}

		t = 0;
		contact_best.pt = unproj.center;
		contact_best.n = pGTest[isneg(pGTest[1]->w.len2() - pGTest[0]->w.len2())]->axisContactNormal;
		contact_best.taux = 1;
		if (!g_Unprojector.Check(&unproj, pGTest[0]->typeprim, pGTest[1]->typeprim, pprims[0], -1, pprims[1],
		                         -1, &contact_best) ||
		    !(contact_best.t > 0))
		{
			if ((border.npt > 3 || pinters->nBestPtVal ||
			     !g_Unprojector.CheckExists(unproj.imode, pGTest[0]->typeprim, pGTest[1]->typeprim)) &&
			    bUseLSNormal == 0)
			{
				bUseLSNormal = 1;
			}
			else
			{
				bNoUnprojection = 1;
			}
			goto end_unproj;
		}
		if (unproj.imode)
		{
			contact_best.t = atan2(contact_best.t, contact_best.taux);
		}

		for (nSmallSteps = 0; contact_best.t > 0 && nSmallSteps < 3;)
		{
			// modify pGTest[0] in accordance with contact_best.t
			t += contact_best.t;
			update_unprojection(t, &unproj, pGTest[0]);
			if (t > tmax)
			{
				bNoUnprojection = 1;
				goto end_unproj;
			}

			pres->iPrim[0] = ((primitives::indexed_triangle*)pprims[0])->idx;
			pres->iPrim[1] = ((primitives::indexed_triangle*)pprims[1])->*pidxoffs;
			nprims1 = pGTest[0]->pGeometry->GetUnprojectionCandidates(0, &contact_best, pprims[0],
			                                                          piFeature[0], pGTest[0]);
			nprims2 = pGTest[1]->pGeometry->GetUnprojectionCandidates(1, &contact_best, pprims[1],
			                                                          piFeature[1], pGTest[1]);

			ibest = jbest = 0;
			contact_best.t = 0;
			for (i = 0, ptr[0] = pprims[0]; i < nprims1;
			     i++, ptr[0] = (primitives::primitive*)((char*)ptr[0] + pGTest[0]->szprim))
			{
				for (j = 0, ptr[1] = pprims[1]; j < nprims2;
				     j++, ptr[1] = (primitives::primitive*)((char*)ptr[1] + pGTest[1]->szprim))
				{
					if (g_Unprojector.Check(&unproj, pGTest[0]->typeprim, pGTest[1]->typeprim,
					                        ptr[0], piFeature[0][i], ptr[1], piFeature[1][j],
					                        &contact_cur) &&
					    contact_cur.t > contact_best.t)
					{
						ibest = i;
						jbest = j;
						contact_best = contact_cur;
						pres->id[0] = pGTest[0]->idbuf[i];
						pres->id[1] = pGTest[1]->idbuf[j];
					}
				}
			}
			pprims[0] = (primitives::primitive*)((char*)pprims[0] + (pGTest[0]->szprim * ibest));
			pprims[1] = (primitives::primitive*)((char*)pprims[1] + (pGTest[1]->szprim * jbest));

			if (unproj.imode)
			{
				contact_best.t = atan2(contact_best.t, contact_best.taux);
				i = isneg(contact_best.t - (real)1E-3);
			}
			else
			{
				i = isneg(contact_best.t - m_minVtxDist);
			}
			nSmallSteps = (nSmallSteps & -i) + i;
		}

		// check if we have surface-surface or edge-surface contact (in the latter case test if both edge
		// buddies are outside the surface)
		if (pres->parea)
		{
			for (i = 0; i < pGTest[0]->nSurfaces; i++)
			{
				for (j = 0; j < pGTest[1]->nSurfaces; j++)
				{
					if (pGTest[0]->surfaces[i].n * pGTest[1]->surfaces[j].n < -maxcos)
					{
						bSurfaceSurfaceContact = 1;
						goto end_unproj;
					} // surface - surface contact
				}
			}
			for (iop = 0; iop < 2; iop++)
			{
				for (i = 0; i < pGTest[iop]->nSurfaces; i++)
				{
					for (j = 0; j < pGTest[iop ^ 1]->nEdges; j++)
					{
						if (sqr(pGTest[iop]->surfaces[i].n * pGTest[iop ^ 1]->edges[j].dir) <
						        mincos2 * pGTest[iop ^ 1]->edges[j].dir.len2() &&
						    sqr_signed(pGTest[iop]->surfaces[i].n *
						               pGTest[iop ^ 1]->edges[j].n[0]) >
						        -mincos2 * pGTest[iop ^ 1]->edges[j].n[0].len2() &&
						    sqr_signed(pGTest[iop]->surfaces[i].n *
						               pGTest[iop ^ 1]->edges[j].n[1]) >
						        -mincos2 * pGTest[iop ^ 1]->edges[j].n[1].len2())
						{
							bSurfaceEdgeContact = 1;
							goto end_unproj;
						} // surface - edge contact
					}
				}
			}
		}
	end_unproj:

		if (!bUseLSNormal && unproj.imode == 1 &&
		    (contact_best.pt - unproj.center ^ unproj.dir).len2() < sqr(pGTest[0]->pParams->minAxisDist))
		{
			unproj.imode = 0;
			unproj.dir = -contact_best.n;
			unproj.vel =
			    unproj.dir * (pGTest[1]->v + (pGTest[1]->w ^ contact_best.pt - pGTest[1]->centerOfMass) -
			                  pGTest[0]->v - (pGTest[0]->w ^ contact_best.pt - pGTest[0]->centerOfMass));
			tmax = unproj.tmax = pGTest[0]->pParams->maxUnproj;
			pGTest[0]->R = unproj.R0;
			pGTest[0]->offset = unproj.offset0;
			bUseLSNormal = -1;
			bNoUnprojection = 0;
		}
		else if ((bUseLSNormal == 1 || t > tmax) && bUseLSNormal != -1)
		{ // if length is out of bounds (relative to vel)
			//   calc least squares normal for border
			//   request calculation of linear unprojection along this normal
			Vec3r inters_center, p0, p1, ntest;
			Matrix33r inters_C;
			real inters_len, det, maxdet;
			Vec3 n;

			pGTest[0]->R = unproj.R0;
			pGTest[0]->offset = unproj.offset0;

			if (pinters->nBestPtVal > border.npt)
			{
				inters_center = pinters->ptbest;
				n = n_avg = pinters->n;
			}
			else if (border.npt < 3)
			{ // not enough points to calc least squares normal, use average normal
				n = border.n_best;
				inters_center = (border.pt[0] + border.pt[1]) * 0.5f;
			}
			else
			{
				border.pt[border.npt] = border.pt[0];
				for (ipt = 0, inters_len = 0, inters_center.zero(); ipt < border.npt; ipt++)
				{
					inters_len += border.seglen[ipt] = (border.pt[ipt + 1] - border.pt[ipt]).len();
					inters_center += (border.pt[ipt + 1] + border.pt[ipt]) * border.seglen[ipt];
				}
				if (inters_len > 0)
				{
					inters_center /= inters_len * 2;
				}
				else
				{
					inters_center = border.pt[0];
				}

				for (ipt = 0, inters_C.SetZero(), ntest.zero(); ipt < border.npt; ipt++)
				{ // form inters_C matrix for least squares normal calculation
					p0 = border.pt[ipt] - inters_center;
					p1 = border.pt[ipt + 1] - inters_center;
					ntest += p0 ^ p1;
					for (i = 0; i < 3; i++)
					{
						for (j = 0; j < 3; j++)
						{
							inters_C(i, j) +=
							    border.seglen[ipt] * (2 * (p0[i] * p0[j] + p1[i] * p1[j]) +
							                          p0[i] * p1[j] + p0[j] * p1[i]);
						}
					}
				}
				if (ntest.len2() < sqr(m_minVtxDist))
				{
					n = border.n_best;
				}
				else
				{
					// since inter_C matrix is (it least it should be) singular, we try to set one
					// coordinate to 1 and find which one provides most stable solution
					for (i = 0, maxdet = 0; i < 3; i++)
					{
						det = (inters_C(inc_mod3[i], inc_mod3[i]) *
						       inters_C(dec_mod3[i], dec_mod3[i])) -
						      (inters_C(dec_mod3[i], inc_mod3[i]) *
						       inters_C(inc_mod3[i], dec_mod3[i]));
						if (fabs(det) > fabs(maxdet))
						{
							maxdet = det;
							j = i;
						}
					}
					if (fabs(maxdet) < 1E-20) // normal is ill defined
					{
						n = n_avg;
					}
					else
					{
						det = 1.0 / maxdet;
						n[j] = 1;
						n[inc_mod3[j]] =
						    -((inters_C(inc_mod3[j], j) * inters_C(dec_mod3[j], dec_mod3[j])) -
						      (inters_C(dec_mod3[j], j) * inters_C(inc_mod3[j], dec_mod3[j]))) *
						    det;
						n[dec_mod3[j]] =
						    -((inters_C(inc_mod3[j], inc_mod3[j]) * inters_C(dec_mod3[j], j)) -
						      (inters_C(dec_mod3[j], inc_mod3[j]) * inters_C(inc_mod3[j], j))) *
						    det;
						n.normalize();
						if (ntest.len2() < sqr(sqr(m_minVtxDist * 10)))
						{
							ntest = n_avg;
						}
						if (n * ntest < 0)
						{
							n.Flip();
						}
					}
				}
			}

			unproj.imode = 0;
			unproj.dir = -n;
			unproj.vel = 0;
			tmax = unproj.tmax = pGTest[0]->pParams->maxUnproj;
			bUseLSNormal = -1;
			bNoUnprojection = 0;
			unprojPlaneNorm.zero();
			pres->center = inters_center;
		}
		else
		{
			break;
		}
	}
	while (true);

	// pres->iPrim[0] = pGTest[0]->typeprim==indexed_triangle::type ? ((indexed_triangle*)pprims[0])->idx : 0;
	// pres->iPrim[1] = pGTest[1]->typeprim==indexed_triangle::type ? ((indexed_triangle*)pprims[1])->idx : 0;
	if (pGTest[1]->typeprim != primitives::indexed_triangle::type)
	{
		pres->iPrim[1] = 0;
	}

	if (bNoUnprojection)
	{
		pres->t = 0.0001;
		pres->pt = border.pt[0];
		pres->n = n_avg;
		pres->iFeature[0] = pinters->iFeature[0][0];
		pres->iFeature[1] = pinters->iFeature[0][1];
		pres->dir = -n_avg;
		pres->vel = -1;
		pres->parea = 0;
		pres->iUnprojMode = 0;
	}
	else
	{
		pres->t = t;
		pres->pt = contact_best.pt;
		pres->iFeature[0] = contact_best.iFeature[0];
		pres->iFeature[1] = contact_best.iFeature[1];
		pres->n = contact_best.n.normalized();
		pres->dir = unproj.dir;
		pres->iUnprojMode = unproj.imode;
		pres->vel = unproj.vel;

		if (bSurfaceSurfaceContact || bSurfaceEdgeContact)
		{
			primitives::coord_plane surface;
			surface.n.zero();
			vector2df *ptbuf1, *ptbuf2, *ptbuf;
			int *idbuf1[2], *idbuf2[2], *idbuf, id, idmask, bEdgeEdge;
			int npt1, npt2, npt;
			g_idata[iCaller].PolyPtBufPos = 0;
			static volatile int g_lockBool2d = 0;
			WriteLock lock(g_lockBool2d);

			if (bSurfaceSurfaceContact)
			{
				pres->parea->type = geom_contact_area::polygon;
				pres->n = pGTest[0]->surfaces[i].n;
				iop = 0;
				pres->parea->n1 = pGTest[1]->surfaces[j].n;
				npt1 = pGTest[0]->pGeometry->PreparePolygon(&surface, pGTest[0]->surfaces[i].idx,
				                                            pGTest[0]->surfaces[i].iFeature, pGTest[0],
				                                            ptbuf1, idbuf1[0], idbuf1[1]);
				npt2 = pGTest[1]->pGeometry->PreparePolygon(&surface, pGTest[1]->surfaces[j].idx,
				                                            pGTest[1]->surfaces[j].iFeature, pGTest[1],
				                                            ptbuf2, idbuf2[0], idbuf2[1]);
				npt = boolean2d(bool_intersect, ptbuf1, npt1, ptbuf2, npt2, 1, ptbuf, idbuf);
			}
			else
			{
				pres->parea->type = geom_contact_area::polyline;
				pres->n = pGTest[iop]->surfaces[i].n;
				pres->parea->n1 = (pGTest[iop ^ 1]->edges[j].dir ^
				                   (pGTest[iop]->surfaces[i].n ^ pGTest[iop ^ 1]->edges[j].dir))
				                      .normalize();
				pres->parea->n1 *= -sgnnz(pres->parea->n1 * pres->n);
				if (iop)
				{
					Vec3 ntmp = pres->n;
					pres->n = pres->parea->n1;
					pres->parea->n1 = ntmp;
				}
				npt1 = pGTest[iop]->pGeometry->PreparePolygon(
				    &surface, pGTest[iop]->surfaces[i].idx, pGTest[iop]->surfaces[i].iFeature,
				    pGTest[iop], ptbuf1, idbuf1[0], idbuf1[1]);
				npt2 = pGTest[iop ^ 1]->pGeometry->PreparePolyline(
				    &surface, pGTest[iop ^ 1]->edges[j].idx, pGTest[iop ^ 1]->edges[j].iFeature,
				    pGTest[iop ^ 1], ptbuf2, idbuf2[0], idbuf2[1]);
				npt = boolean2d(bool_intersect, ptbuf1, npt1, ptbuf2, npt2, 0, ptbuf, idbuf);
			}

			if (npt)
			{
				for (i = 0; i < min(npt, pres->parea->nmaxpt); i++)
				{
					pres->parea->pt[i] = surface.origin + surface.axes[0] * ptbuf[i].x +
					                     surface.axes[1] * ptbuf[i].y;
					bEdgeEdge =
					    -(((idbuf[i] >> 16) - 1) >> 31 | ((idbuf[i] & 0xFFFF) - 1) >> 31) ^ 1;
					id = (idbuf[i] & 0xFFFF) - 1;
					idmask = id >> 31;
					id -= idmask;
					pres->parea->piPrim[iop][i] = idbuf1[bEdgeEdge][id] >> 8 | idmask;
					pres->parea->piFeature[iop][i] = (idbuf1[bEdgeEdge][id] & 0xFF) | idmask;
					id = (idbuf[i] >> 16) - 1;
					idmask = id >> 31;
					id -= idmask;
					pres->parea->piPrim[iop ^ 1][i] = idbuf2[bEdgeEdge][id] >> 8 | idmask;
					pres->parea->piFeature[iop ^ 1][i] = (idbuf2[bEdgeEdge][id] & 0xFF) | idmask;
				}
				pres->parea->npt = i;
			}
			else
			{
				pres->parea->npt = 0;
			}

			g_idata[iCaller].nAreaPt += pres->parea->npt;
			g_idata[iCaller].nAreas++;
		}
		else
		{
			pres->parea = 0;
		}
	}

	if (pinters->nborderpt)
	{
		for (i = 0; i < pinters->nborderpt; i++)
		{
			pres->ptborder[pres->nborderpt++] = pinters->ptborder[i];
		}
		g_idata[iCaller].BrdPtBufPos += pinters->nborderpt;
		pres->bBorderConsecutive = false;
	}
	else if (pinters->nBestPtVal > 100 && !pGTest[0]->pParams->bExactBorder)
	{
		pres->nborderpt = 1;
		pres->ptborder = &pres->center;
	}

	pGTest[0]->R = unproj.R0;
	pGTest[0]->offset = unproj.offset0;

	// allocate slots for the next intersection
	(*pGTest[0]->pnContacts)++;
	if (*pGTest[0]->pnContacts >= pGTest[0]->nMaxContacts || pGTest[0]->pParams->bStopAtFirstTri ||
	    pGTest[0]->pParams->bBothConvex)
	{
		pGTest[0]->bStopIntersection = 1;
	}
	else
	{
		pres = pGTest[0]->contacts + *pGTest[0]->pnContacts;
		if (!pGTest[0]->pParams->bNoAreaContacts &&
		    g_idata[iCaller].nAreas < sizeof(g_idata[iCaller].AreaBuf) / sizeof(g_idata[iCaller].AreaBuf[0]) &&
		    g_idata[iCaller].nAreaPt <
		        sizeof(g_idata[iCaller].AreaPtBuf) / sizeof(g_idata[iCaller].AreaPtBuf[0]))
		{
			pres->parea = g_idata[iCaller].AreaBuf + g_idata[iCaller].nAreas;
			pres->parea->pt = g_idata[iCaller].AreaPtBuf + g_idata[iCaller].nAreaPt;
			pres->parea->piPrim[0] = g_idata[iCaller].AreaPrimBuf0 + g_idata[iCaller].nAreaPt;
			pres->parea->piFeature[0] = g_idata[iCaller].AreaFeatureBuf0 + g_idata[iCaller].nAreaPt;
			pres->parea->piPrim[1] = g_idata[iCaller].AreaPrimBuf1 + g_idata[iCaller].nAreaPt;
			pres->parea->piFeature[1] = g_idata[iCaller].AreaFeatureBuf1 + g_idata[iCaller].nAreaPt;
			pres->parea->npt = 0;
			pres->parea->nmaxpt =
			    (sizeof(g_idata[iCaller].AreaPtBuf) / sizeof(g_idata[iCaller].AreaPtBuf[0])) -
			    g_idata[iCaller].nAreaPt;
			pres->parea->minedge = min(pGTest[0]->minAreaEdge, pGTest[1]->minAreaEdge);
		}
		else
		{
			pres->parea = 0;
		}
	}

	return 1;
}

int CTriMesh::GetEdgeByBuddy(int itri, int itri_buddy)
{
	int iedge = 0, imask;
	imask = m_pTopology[itri].ibuddy[1] - itri_buddy;
	imask = (imask - 1) >> 31 ^ imask >> 31;
	iedge = 1 & imask;
	imask = m_pTopology[itri].ibuddy[2] - itri_buddy;
	imask = (imask - 1) >> 31 ^ imask >> 31;
	iedge = (iedge & ~imask) | (2 & imask);
	return iedge;
}

int CTriMesh::GetNeighbouringEdgeId(int vtxid, int ivtx)
{
	int iedge, itri, itrinew, itri0, iter = 50;
	itri0 = itri = vtxid >> 8;
	iedge = dec_mod3[vtxid & 31];
	do
	{
		if (m_pIndices[(itri * 3) + iedge] == ivtx) // check leaving edge
		{
			return itri << 8 | iedge | 0x20;
		}
		if ((itrinew = m_pTopology[itri].ibuddy[iedge]) < 0)
		{
			return -1;
		}
		iedge = GetEdgeByBuddy(itrinew, itri);
		itri = itrinew;
		if (m_pIndices[(itri * 3) + inc_mod3[iedge]] == ivtx) // check entering edge
		{
			return itri << 8 | iedge | 0x20;
		}
		iedge = dec_mod3[iedge];
	}
	while (itri != itri0 && --iter > 0);
	return -1;
}

int CTriMesh::PreparePolygon(primitives::coord_plane* psurface, int iPrim, int iFeature, geometry_under_test* pGTest,
                             vector2df*& ptbuf, int*& pVtxIdBuf, int*& pEdgeIdBuf)
{
	int *pUsedVtxMap, UsedVtxIdx[16], nUsedVtx, *pUsedTriMap, UsedTriIdx[16], nUsedTri;
	int i, ihead, itail, ntri, nvtx, itri, itri_parent, ivtx0, ivtx, itri_new, iedge, iedge_open{}, ivtx0_new,
	    ivtx_start, ivtx_end{}, ivtx_end1, ivtx_head, iorder, nSides;
	Vec3 n0, n, edge, pt0;

	if (psurface->n.len2() == 0)
	{
		psurface->n = pGTest->R * m_pNormals[iPrim];
		psurface->axes[0] =
		    pGTest->R *
		    (m_pVertices[m_pIndices[(iPrim * 3) + 1]] - m_pVertices[m_pIndices[iPrim * 3]]).normalized();
		psurface->axes[1] = psurface->n ^ psurface->axes[0];
		psurface->origin = pGTest->R * m_pVertices[m_pIndices[iPrim * 3]] * pGTest->scale + pGTest->offset;
	}
	if (((m_nVertices - 1) >> 5) + 1 <
	    sizeof(g_idata[pGTest->iCaller].UsedVtxMap) / sizeof(g_idata[pGTest->iCaller].UsedVtxMap[0]))
	{
		pUsedVtxMap = g_idata[pGTest->iCaller].UsedVtxMap;
	}
	else
	{
		memset(pUsedVtxMap = new int[((m_nVertices - 1) >> 5) + 1], 0, (((m_nVertices - 1) >> 5) + 1) << 2);
	}
	if (((m_nTris - 1) >> 5) + 1 <
	    sizeof(g_idata[pGTest->iCaller].UsedTriMap) / sizeof(g_idata[pGTest->iCaller].UsedTriMap[0]))
	{
		pUsedTriMap = g_idata[pGTest->iCaller].UsedTriMap;
	}
	else
	{
		memset(pUsedTriMap = new int[((m_nTris - 1) >> 5) + 1], 0, (((m_nTris - 1) >> 5) + 1) << 2);
	}
	nUsedVtx = nUsedTri = 0;

	const auto enqueue_tri = [&]() {
		if (itri_new >= 0 && !(pUsedTriMap[itri_new >> 5] & 1 << (itri_new & 31)) &&
		    m_pNormals[itri_new] * n0 > 1.0f - 2E-4f)
		{
			ihead = (ihead + 1) & ((int)(sizeof(g_idata[pGTest->iCaller].TriQueue) /
			                             sizeof(g_idata[pGTest->iCaller].TriQueue[0])) -
			                       1);
			ntri = min(ntri + 1, (int)(sizeof(g_idata[pGTest->iCaller].TriQueue) /
			                           sizeof(g_idata[pGTest->iCaller].TriQueue[0])));
			g_idata[pGTest->iCaller].TriQueue[ihead].itri = itri_new;
			g_idata[pGTest->iCaller].TriQueue[ihead].itri_parent = itri;
			g_idata[pGTest->iCaller].TriQueue[ihead].ivtx0 = ivtx0_new;
			pUsedTriMap[itri_new >> 5] |= 1 << (itri_new & 31);
			UsedTriIdx[nUsedTri] = itri_new;
			nUsedTri = min(nUsedTri + 1, 15);
			m_pTree->MarkUsedTriangle(itri_new, pGTest);
		}
	};

	ihead = -1;
	itail = 0;
	n0 = m_pNormals[iPrim];
	itri = iPrim;
	ntri = 0;
	nvtx = 3;
	g_idata[pGTest->iCaller].VtxList[0].ivtx = m_pIndices[(itri * 3) + 0];
	g_idata[pGTest->iCaller].VtxList[0].ibuddy[1] = 1;
	g_idata[pGTest->iCaller].VtxList[0].ibuddy[0] = 2;
	g_idata[pGTest->iCaller].VtxList[0].id = iPrim << 8;
	g_idata[pGTest->iCaller].VtxList[1].ivtx = m_pIndices[(itri * 3) + 1];
	g_idata[pGTest->iCaller].VtxList[1].ibuddy[1] = 2;
	g_idata[pGTest->iCaller].VtxList[1].ibuddy[0] = 0;
	g_idata[pGTest->iCaller].VtxList[1].id = iPrim << 8 | 1;
	g_idata[pGTest->iCaller].VtxList[2].ivtx = m_pIndices[(itri * 3) + 2];
	g_idata[pGTest->iCaller].VtxList[2].ibuddy[1] = 0;
	g_idata[pGTest->iCaller].VtxList[2].ibuddy[0] = 1;
	g_idata[pGTest->iCaller].VtxList[2].id = iPrim << 8 | 2;
	itri_new = m_pTopology[itri].ibuddy[0];
	ivtx0_new = 0;
	enqueue_tri();
	itri_new = m_pTopology[itri].ibuddy[1];
	ivtx0_new = 1;
	enqueue_tri();
	itri_new = m_pTopology[itri].ibuddy[2];
	ivtx0_new = 2;
	enqueue_tri();
	ivtx_head = 0;

	while (ntri > 0)
	{
		itri = g_idata[pGTest->iCaller].TriQueue[itail].itri;
		itri_parent = g_idata[pGTest->iCaller].TriQueue[itail].itri_parent;
		ivtx_head = ivtx0 = g_idata[pGTest->iCaller].TriQueue[itail].ivtx0;
		itail =
		    (itail + 1) &
		    ((sizeof(g_idata[pGTest->iCaller].TriQueue) / sizeof(g_idata[pGTest->iCaller].TriQueue[0])) - 1);
		ntri--;
		iedge = GetEdgeByBuddy(itri, itri_parent);
		ivtx = m_pIndices[(itri * 3) + dec_mod3[iedge]];
		nSides = 0;

		if (pUsedVtxMap[ivtx >> 5] & 1 << (ivtx & 31))
		{
			// triangle's free vertex is already in the list
			if (g_idata[pGTest->iCaller]
			        .VtxList[ivtx_start = g_idata[pGTest->iCaller].VtxList[ivtx0].ibuddy[0]]
			        .ivtx == ivtx)
			{
				ivtx_end = g_idata[pGTest->iCaller]
				               .VtxList[g_idata[pGTest->iCaller].VtxList[ivtx_start].ibuddy[1]]
				               .ibuddy[1]; // tringle touches previous buddy
				iedge_open = dec_mod3[iedge];
				ivtx0_new = ivtx_head = ivtx_start;
				nSides++;
			}
			if (g_idata[pGTest->iCaller]
			        .VtxList[ivtx_end1 = g_idata[pGTest->iCaller]
			                                 .VtxList[g_idata[pGTest->iCaller].VtxList[ivtx0].ibuddy[1]]
			                                 .ibuddy[1]]
			        .ivtx == ivtx)
			{
				ivtx_end = ivtx_end1;
				ivtx_start = ivtx0_new = ivtx0; // triangle touches next buddy
				iedge_open = inc_mod3[iedge];
				nSides++;
			}
			if (nSides == 2)
			{ // if triangle touches both neighbours, it must have filled interior hole, trace it and "glue"
			  // sides
				ivtx_start = g_idata[pGTest->iCaller].VtxList[ivtx0].ibuddy[0];
				ivtx_end = g_idata[pGTest->iCaller]
				               .VtxList[g_idata[pGTest->iCaller].VtxList[ivtx0].ibuddy[1]]
				               .ibuddy[1];
				for (; g_idata[pGTest->iCaller].VtxList[ivtx_start].ivtx ==
				       g_idata[pGTest->iCaller].VtxList[ivtx_end].ivtx;
				     ivtx_start = g_idata[pGTest->iCaller].VtxList[ivtx_start].ibuddy[0],
				     ivtx_end = g_idata[pGTest->iCaller].VtxList[ivtx_end].ibuddy[1])
					;
				ivtx_start = g_idata[pGTest->iCaller].VtxList[ivtx_start].ibuddy[1];
			}
			if (nSides > 0)
			{ // remove one vertex if triangle touches only one neighbour
				g_idata[pGTest->iCaller].VtxList[ivtx_start].ibuddy[1] = ivtx_end;
				g_idata[pGTest->iCaller].VtxList[ivtx_end].ibuddy[0] = ivtx_start;
			}
		}
		if (nSides == 0)
		{ // add new vertex to the list if triangle does not touch neighbours
			if (nvtx ==
			    sizeof(g_idata[pGTest->iCaller].VtxList) / sizeof(g_idata[pGTest->iCaller].VtxList[0]))
			{
				break;
			}
			g_idata[pGTest->iCaller].VtxList[nvtx].ivtx = ivtx;
			g_idata[pGTest->iCaller].VtxList[nvtx].id = itri << 8 | dec_mod3[iedge];
			g_idata[pGTest->iCaller].VtxList[nvtx].ibuddy[1] =
			    g_idata[pGTest->iCaller].VtxList[ivtx0].ibuddy[1];
			g_idata[pGTest->iCaller].VtxList[nvtx].ibuddy[0] = ivtx0;
			g_idata[pGTest->iCaller].VtxList[ivtx0].ibuddy[1] =
			    g_idata[pGTest->iCaller]
				.VtxList[g_idata[pGTest->iCaller].VtxList[ivtx0].ibuddy[1]]
				.ibuddy[0] = nvtx++;
			pUsedVtxMap[ivtx >> 5] |= 1 << (ivtx & 31);
			UsedVtxIdx[nUsedVtx] = ivtx;
			nUsedVtx = min(nUsedVtx + 1, 15);

			itri_new = m_pTopology[itri].ibuddy[inc_mod3[iedge]];
			ivtx0_new = ivtx0;
			enqueue_tri();
			iedge_open = dec_mod3[iedge];
			ivtx0_new = g_idata[pGTest->iCaller].VtxList[ivtx0].ibuddy[1];
		}
		if (nSides < 2)
		{
			itri_new = m_pTopology[itri].ibuddy[iedge_open];
			enqueue_tri();
		}
	}

	if (pUsedVtxMap != g_idata[pGTest->iCaller].UsedVtxMap)
	{
		delete[] pUsedVtxMap;
	}
	else if (nUsedVtx >= 15)
	{
		memset(g_idata[pGTest->iCaller].UsedVtxMap, 0, (((m_nVertices - 1) >> 5) + 1) << 2);
	}
	else
	{
		for (i = 0; i < nUsedVtx; i++)
		{
			g_idata[pGTest->iCaller].UsedVtxMap[UsedVtxIdx[i] >> 5] &= ~(1 << (UsedVtxIdx[i] & 31));
		}
	}
	if (pUsedTriMap != g_idata[pGTest->iCaller].UsedTriMap)
	{
		delete[] pUsedTriMap;
	}
	else if (nUsedTri >= 15)
	{
		memset(g_idata[pGTest->iCaller].UsedTriMap, 0, (((m_nTris - 1) >> 5) + 1) << 2);
	}
	else
	{
		for (i = 0; i < nUsedTri; i++)
		{
			g_idata[pGTest->iCaller].UsedTriMap[UsedTriIdx[i] >> 5] &= ~(1 << (UsedTriIdx[i] & 31));
		}
	}

	ivtx_start = ivtx_head;
	nvtx = 0;
	ptbuf = g_idata[pGTest->iCaller].PolyPtBuf + g_idata[pGTest->iCaller].PolyPtBufPos;
	pVtxIdBuf = g_idata[pGTest->iCaller].PolyVtxIdBuf + g_idata[pGTest->iCaller].PolyPtBufPos;
	pEdgeIdBuf = g_idata[pGTest->iCaller].PolyEdgeIdBuf + g_idata[pGTest->iCaller].PolyPtBufPos;
	iorder = isnonneg((pGTest->R * n0) * psurface->n);
	do
	{
		pt0 = pGTest->R * m_pVertices[g_idata[pGTest->iCaller].VtxList[ivtx_head].ivtx] * pGTest->scale +
		      pGTest->offset - psurface->origin;
		ptbuf[nvtx].set(psurface->axes[0] * pt0, psurface->axes[1] * pt0);
		pVtxIdBuf[nvtx] = g_idata[pGTest->iCaller].VtxList[ivtx_head].id;
		if ((unsigned int)(g_idata[pGTest->iCaller].VtxList[ivtx_head].id >> 8) >= (unsigned int)m_nTris)
		{
			break;
		}
		pEdgeIdBuf[nvtx++] = GetNeighbouringEdgeId(
		    g_idata[pGTest->iCaller].VtxList[ivtx_head].id,
		    g_idata[pGTest->iCaller].VtxList[g_idata[pGTest->iCaller].VtxList[ivtx_head].ibuddy[iorder]].ivtx);
		ivtx_head = g_idata[pGTest->iCaller].VtxList[ivtx_head].ibuddy[iorder];
	}
	while (ivtx_head != ivtx_start &&
	       nvtx < sizeof(g_idata[pGTest->iCaller].VtxList) / sizeof(g_idata[pGTest->iCaller].VtxList[0]));
	ptbuf[nvtx] = ptbuf[0];
	pVtxIdBuf[nvtx] = pVtxIdBuf[0];
	pEdgeIdBuf[nvtx] = pEdgeIdBuf[0];
	g_idata[pGTest->iCaller].PolyPtBufPos += nvtx + 1;

	return nvtx;
}

int CTriMesh::PreparePolyline(primitives::coord_plane* psurface, int iPrim, int iFeature, geometry_under_test* pGTest,
                              vector2df*& ptbuf, int*& pVtxIdBuf, int*& pEdgeIdBuf)
{
	int *pUsedVtxMap, UsedVtxIdx[16], nUsedVtx = 0;
	int nvtx, iedge, itri, itri_prev, ivtx, itri0, iorder, i, iprev, iter = 0;
	Vec3 n0, edge, pt;

	if (((m_nVertices - 1) >> 5) + 1 <
	    sizeof(g_idata[pGTest->iCaller].UsedVtxMap) / sizeof(g_idata[pGTest->iCaller].UsedVtxMap[0]))
	{
		pUsedVtxMap = g_idata[pGTest->iCaller].UsedVtxMap;
	}
	else
	{
		memset(pUsedVtxMap = new int[((m_nVertices - 1) >> 5) + 1], 0, (((m_nVertices - 1) >> 5) + 1) << 2);
	}

	iedge = iFeature & 0x1F;
	itri_prev = itri0 = iPrim;
	nvtx = 2;
	g_idata[pGTest->iCaller].VtxList[0].ivtx = m_pIndices[(itri0 * 3) + iedge];
	g_idata[pGTest->iCaller].VtxList[1].ivtx = m_pIndices[(itri0 * 3) + inc_mod3[iedge]];
	g_idata[pGTest->iCaller].VtxList[0].ibuddy[0] = g_idata[pGTest->iCaller].VtxList[1].ibuddy[1] = -1;
	g_idata[pGTest->iCaller].VtxList[0].ibuddy[1] = 1;
	g_idata[pGTest->iCaller].VtxList[1].ibuddy[0] = 0;
	g_idata[pGTest->iCaller].VtxList[0].id = itri0 << 8 | iedge;
	g_idata[pGTest->iCaller].VtxList[1].id = itri0 << 8 | inc_mod3[iedge];
	edge = m_pVertices[m_pIndices[(itri0 * 3) + inc_mod3[iedge]]] - m_pVertices[m_pIndices[(itri0 * 3) + iedge]];
	ivtx = m_pIndices[(itri0 * 3) + iedge];
	pUsedVtxMap[ivtx >> 5] |= 1 << (ivtx & 31);
	UsedVtxIdx[nUsedVtx++] = ivtx;
	ivtx = m_pIndices[(itri0 * 3) + inc_mod3[iedge]];
	pUsedVtxMap[ivtx >> 5] |= 1 << (ivtx & 31);
	UsedVtxIdx[nUsedVtx++] = ivtx;
	n0 = psurface->n * pGTest->R;
	n0 = (edge ^ (edge ^ n0)).normalized();

	for (iorder = 1, iprev = nvtx - 1; iorder >= 0; iorder--)
	{
		do
		{
			itri = m_pTopology[itri_prev].ibuddy[iedge];
			if (itri < 0 || itri == itri0)
			{
				break;
			}
			iedge = dec_mod3[GetEdgeByBuddy(itri, itri_prev)];
			edge = m_pVertices[m_pIndices[(itri * 3) + inc_mod3[iedge]]] -
			       m_pVertices[m_pIndices[(itri * 3) + iedge]];
			if (fabsf(edge * n0) < 1E-2f &&
			    sqr((m_pVertices[m_pIndices[(itri * 3) + iedge]] + edge * 0.5f - psurface->origin) *
			        psurface->n) < edge.len2() * sqr(0.02f))
			{
				itri0 = itri;
				iedge = dec_mod3[iedge];
				ivtx = m_pIndices[(itri * 3) + inc_mod3[iedge]];
				if (pUsedVtxMap[ivtx >> 5] & 1 << (ivtx & 31))
				{
					break;
				}
				// add edge to list
				if (nvtx == sizeof(g_idata[pGTest->iCaller].VtxList) /
				                sizeof(g_idata[pGTest->iCaller].VtxList[0]))
				{
					break;
				}
				g_idata[pGTest->iCaller].VtxList[nvtx].ivtx = ivtx;
				g_idata[pGTest->iCaller].VtxList[nvtx].id = itri << 8 | inc_mod3[iedge];
				g_idata[pGTest->iCaller].VtxList[nvtx].ibuddy[iorder] = -1;
				g_idata[pGTest->iCaller].VtxList[nvtx].ibuddy[iorder ^ 1] = iprev;
				g_idata[pGTest->iCaller].VtxList[iprev].ibuddy[iorder] = nvtx;
				iprev = nvtx++;
				pUsedVtxMap[ivtx >> 5] |= 1 << (ivtx & 31);
				UsedVtxIdx[nUsedVtx] = ivtx;
				nUsedVtx = min(nUsedVtx + 1, 15);
			}
			itri_prev = itri;
		}
		while (++iter < 100);
		itri_prev = itri0 = iPrim;
		iedge = dec_mod3[iFeature & 0x1F];
		iprev = 0;
	}

	if (pUsedVtxMap != g_idata[pGTest->iCaller].UsedVtxMap)
	{
		delete[] pUsedVtxMap;
	}
	else if (nUsedVtx >= 15)
	{
		memset(g_idata[pGTest->iCaller].UsedVtxMap, 0, (((m_nVertices - 1) >> 5) + 1) << 2);
	}
	else
	{
		for (i = 0; i < nUsedVtx; i++)
		{
			g_idata[pGTest->iCaller].UsedVtxMap[UsedVtxIdx[i] >> 5] &= ~(1 << (UsedVtxIdx[i] & 31));
		}
	}

	ptbuf = g_idata[pGTest->iCaller].PolyPtBuf + g_idata[pGTest->iCaller].PolyPtBufPos;
	pVtxIdBuf = g_idata[pGTest->iCaller].PolyVtxIdBuf + g_idata[pGTest->iCaller].PolyPtBufPos;
	pEdgeIdBuf = g_idata[pGTest->iCaller].PolyEdgeIdBuf + g_idata[pGTest->iCaller].PolyPtBufPos;
	for (nvtx = 0, ivtx = iprev;
	     ivtx >= 0 && nvtx < sizeof(g_idata[pGTest->iCaller].VtxList) / sizeof(g_idata[pGTest->iCaller].VtxList[0]);
	     ivtx = g_idata[pGTest->iCaller].VtxList[ivtx].ibuddy[1], nvtx++)
	{
		pt = pGTest->R * m_pVertices[g_idata[pGTest->iCaller].VtxList[ivtx].ivtx] * pGTest->scale +
		     pGTest->offset - psurface->origin;
		ptbuf[nvtx].set(psurface->axes[0] * pt, psurface->axes[1] * pt);
		pVtxIdBuf[nvtx] = g_idata[pGTest->iCaller].VtxList[ivtx].id;
		pEdgeIdBuf[nvtx] =
		    g_idata[pGTest->iCaller].VtxList[ivtx].ibuddy[1] >= 0
			? GetNeighbouringEdgeId(
			      g_idata[pGTest->iCaller].VtxList[ivtx].id,
			      g_idata[pGTest->iCaller].VtxList[g_idata[pGTest->iCaller].VtxList[ivtx].ibuddy[1]].ivtx)
			: -1;
	}
	ptbuf[nvtx] = ptbuf[0];
	pVtxIdBuf[nvtx] = pVtxIdBuf[0];
	pEdgeIdBuf[nvtx] = pEdgeIdBuf[0];
	g_idata[pGTest->iCaller].PolyPtBufPos += nvtx + 1;

	return nvtx;
}

int CTriMesh::FindClosestPoint(geom_world_data* pgwd, int& iPrim, int& iFeature, const Vec3& ptdst0, const Vec3& ptdst1,
                               Vec3* ptres, int nMaxIters)
{
	Vec3 pt[3], n, nprev, edge, dp, seg, cross, ptdst, ptseg[3];
	int i, itri, ivtx, iedge, bEdgeOut[4], bOut, itri0, inewtri, itrimax, bBest, iter;
	float t, tmax, dist[3], ptdstw = 1.0f;
	int iPrim_hist[4] = {-1, -1, -1, -1}, iFeature_hist[4] = {-1, -1, -1, -1}, ihist = 0, bLooped;
	int bLine = isneg(1E-4f - (seg = ptdst1 - ptdst0).len2());
	ptseg[0] = ptdst0;
	ptseg[1] = ptdst1;
	ptres[1] = ptdst0;
	ptres[0].Set(1E10f, 1E10f, 1E10f);
	vector2_w_enforcer<float> wdiv(ptres[1], ptres[0], ptdstw);

	if ((unsigned int)iPrim < (unsigned int)m_nTris && (iFeature & 0x1F) < 3)
	{
		do
		{
			if ((iFeature & 0x60) == 0x40)
			{ // face
				pt[0] = pgwd->R * m_pVertices[m_pIndices[(iPrim * 3) + 0]] * pgwd->scale + pgwd->offset;
				pt[1] = pgwd->R * m_pVertices[m_pIndices[(iPrim * 3) + 1]] * pgwd->scale + pgwd->offset;
				pt[2] = pgwd->R * m_pVertices[m_pIndices[(iPrim * 3) + 2]] * pgwd->scale + pgwd->offset;
				n = pgwd->R * m_pNormals[iPrim];

				if (bLine)
				{
					dist[0] = (ptseg[0] - pt[0]) * n;
					dist[1] = (ptseg[1] - pt[0]) * n;
					if (dist[0] * dist[1] < 0 && fabsf(ptdstw = seg * n) > 1E-4f)
					{
						ptres[1] = (ptseg[0] * ptdstw + seg * ((pt[0] - ptseg[0]) * n)) *
						           sgnnz(ptdstw);
						ptdstw = fabsf(ptdstw);
						pt[0] *= ptdstw;
						pt[1] *= ptdstw;
						pt[2] *= ptdstw;
					}
					else
					{
						ptres[1] = ptseg[isneg(dist[1] - dist[0])];
						ptdstw = 1.0f;
					}
				}
				else
				{
					ptres[1] = ptdst0;
					ptdstw = 1.0f;
				}

				bEdgeOut[0] = isneg((pt[1] - pt[0] ^ ptres[1] - pt[0]) * n);
				bEdgeOut[1] = isneg((pt[2] - pt[1] ^ ptres[1] - pt[1]) * n);
				bEdgeOut[2] = isneg((pt[0] - pt[2] ^ ptres[1] - pt[2]) * n);
				ptres[0] = ptres[1] - n * ((ptres[1] - pt[0]) * n);
				if (bEdgeOut[0] + bEdgeOut[1] + bEdgeOut[2] == 0)
				{
					return nMaxIters; // ptres[1] is in triangle's voronoi region
				}

				i = (bEdgeOut[0] ^ 1) +
				    ((bEdgeOut[0] | bEdgeOut[1]) ^ 1); // i = index of the 1st non-zero (unity) bEdgeOut
				if (m_pTopology[iPrim].ibuddy[i] >= 0)
				{
					iFeature = GetEdgeByBuddy(m_pTopology[iPrim].ibuddy[i], iPrim) | 0x20;
					iPrim = m_pTopology[iPrim].ibuddy[i];
				}
				else
				{
					edge = pt[inc_mod3[i]] - pt[i];
					t = min(ptdstw = edge.len2(), max(0.0f, (ptdst0 - pt[i]) * edge));
					ptres[0] = pt[i] * ptdstw + edge * t;
					ptres[1] *= ptdstw;
					iFeature = 0x20 | i;
					return nMaxIters;
				}
			}
			else if ((iFeature & 0x60) == 0x20)
			{ // edge
				iedge = iFeature & 0x1F;
				n = pgwd->R * m_pNormals[iPrim];
				pt[0] =
				    pgwd->R * m_pVertices[m_pIndices[(iPrim * 3) + iedge]] * pgwd->scale + pgwd->offset;
				pt[1] = pgwd->R * m_pVertices[m_pIndices[(iPrim * 3) + inc_mod3[iedge]]] * pgwd->scale +
				        pgwd->offset;
				edge = pt[1] - pt[0];

				if (bLine)
				{
					cross = seg ^ edge;
					t = (pt[0] - ptseg[0] ^ edge) * cross;
					ptdstw = cross.len2();
					i = isneg(1E-6f - ptdstw);
					ptdstw =
					    (ptdstw * i) +
					    (i ^ 1); // set ptdstw to 1 if it's too small - point is ill defined anyway
					t = min(ptdstw, max(0.0f, t));
					dist[0] = (ptseg[0] - pt[0] ^ edge).len2() * ptdstw;
					dist[1] = (ptseg[1] - pt[0] ^ edge).len2() * ptdstw;
					ptseg[2] = ptseg[0] * ptdstw + seg * t;
					dist[2] = sqr((ptseg[0] - pt[0]) * cross) * edge.len2();
					i = idxmin3(dist);
					ptres[1] = ptseg[i];
					ptdstw =
					    (ptdstw * (i >> 1)) + (i >> 1 ^ 1); // reset ptdstw to 1.0 if i is not 2
					pt[0] *= ptdstw;
					pt[1] *= ptdstw;
					edge *= ptdstw;
				}
				else
				{
					ptres[1] = ptdst0;
					ptdstw = 1.0f;
				}

				if (m_pTopology[iPrim].ibuddy[iedge] < 0)
				{
					iFeature = 0x40;
				}
				else
				{
					nprev = pgwd->R * m_pNormals[m_pTopology[iPrim].ibuddy[iedge]];
					dp = ptres[1] - pt[0];
					bEdgeOut[0] = isnonneg((n ^ edge) * dp);  // point leans to the 1st triangle
					bEdgeOut[1] = isneg((nprev ^ edge) * dp); // point leans to the 2nd triangle
					bEdgeOut[2] = isneg((ptres[1] - pt[0]) * edge); // point leans to start vertex
					bEdgeOut[3] = isnonneg((ptres[1] - pt[1]) * edge); // point leans to end vertex
					// ptres[0] = pt[0]+edge*((edge*dp)/edge.len2());
					ptres[0] = pt[0] * (t = edge.len2()) + edge * ((edge * (ptres[1] - pt[0])));
					ptdstw *= t;
					ptres[1] *= t;

					if (bEdgeOut[0] + bEdgeOut[1] + bEdgeOut[2] + bEdgeOut[3] == 0)
					{
						return nMaxIters;
					}
					if (bEdgeOut[0] + bEdgeOut[1])
					{
						if (bEdgeOut[1])
						{
							iPrim = m_pTopology[iPrim].ibuddy[iedge];
						}
						iFeature = 0x40;
					}
					else
					{
						iFeature = (inc_mod3[iedge] & -bEdgeOut[3]) | (iedge & ~-bEdgeOut[3]);
					}
				}
			}
			else if ((iFeature & 0x60) == 0)
			{ // vertex
				pt[0] =
				    pgwd->R * m_pVertices[m_pIndices[(iPrim * 3) + (iFeature & 0x1F)]] * pgwd->scale +
				    pgwd->offset;

				if (bLine)
				{
					dist[0] = (ptseg[0] - pt[0]).len2();
					dist[1] = (ptseg[1] - pt[0]).len2();
					dist[2] = (pt[0] - ptseg[0] ^ seg).len2();
					ptdstw = seg.len2();
					dist[0] = sqr(dist[0]) * ptdstw;
					dist[1] = sqr(dist[1]) * ptdstw;
					ptseg[2] = ptseg[0] * ptdstw + seg * (seg * (pt[0] - ptseg[0]));
					i = idxmin3(dist);
					ptres[1] = ptseg[i];
					ptdstw =
					    (ptdstw * (i >> 1)) + (i >> 1 ^ 1); // reset ptdstw to 1.0 if i is not 2
					pt[0] *= ptdstw;
				}
				else
				{
					ptres[1] = ptdst0;
					ptdstw = 1.0f;
				}

				dp = ptres[1] - pt[0];
				itri = itrimax = itri0 = iPrim;
				ivtx = iFeature & 0x1F;
				tmax = 0;
				bOut = 0;
				if (m_pTopology[itri].ibuddy[ivtx] >= 0)
				{
					nprev = pgwd->R * m_pNormals[m_pTopology[itri].ibuddy[ivtx]];
				}
				else
				{
					nprev.zero();
				}
				iter = 35;
				do
				{
					n = pgwd->R * m_pNormals[itri];
					bOut |= isneg((nprev ^ n) * dp + 1E-7f);
					bBest = isneg(tmax - (t = n * dp));
					tmax = (tmax * (bBest ^ 1)) + (t * bBest);
					itrimax = (itrimax & ~-bBest) | (itri & -bBest);
					if ((inewtri = m_pTopology[itri].ibuddy[dec_mod3[ivtx]]) < 0)
					{
						break;
					}
					ivtx = GetEdgeByBuddy(inewtri, itri);
					itri = inewtri;
					nprev = n;
				}
				while (itri != itri0 && --iter);
				ptres[0] = pt[0];
				if (!bOut)
				{
					return nMaxIters;
				}
				iPrim = itrimax;
				iFeature = 0x40;
			}
			else
			{
				return -1;
			}

			bLooped = iszero((iPrim - iPrim_hist[0]) | (iFeature - iFeature_hist[0])) |
			          iszero((iPrim - iPrim_hist[1]) | (iFeature - iFeature_hist[1])) |
			          iszero((iPrim - iPrim_hist[2]) | (iFeature - iFeature_hist[2])) |
			          iszero((iPrim - iPrim_hist[3]) | (iFeature - iFeature_hist[3]));
			iPrim_hist[ihist] = iPrim;
			iFeature_hist[ihist] = iFeature;
			ihist = (ihist + 1) & 3;
			// if line mode and looped, don't allow final iteration to be face one (face doesn't compute
			// ptres[1] properly)
		}
		while (!bLooped && --nMaxIters > 0 || bLine && iFeature_hist[(ihist - 2) & 3] == 0x40);
	}
	else
	{
		return -1;
	}

	return max(nMaxIters, 0);
}

int CTriMesh::PointInsideStatusMesh(const Vec3& pt, primitives::indexed_triangle* pHitTri)
{
	int res = 0;
	float mindist = 1E20f;
	primitives::triangle atri;
	primitives::ray aray;
	int icell, i;
	Vec3 ptgrid;
	prim_inters inters;

	// if (m_nTris<4)
	//	return 0;
	PrepareForRayTest(0);

	ptgrid = m_hashgrid[0].Basis * (pt - m_hashgrid[0].origin);
	icell = (min(m_hashgrid[0].size.x - 1, max(0, physics_float2int((m_hashgrid[0].stepr.x * ptgrid.x) - 0.5f))) *
	         m_hashgrid[0].stride.x) +
	        (min(m_hashgrid[0].size.y - 1, max(0, physics_float2int((m_hashgrid[0].stepr.y * ptgrid.y) - 0.5f))) *
	         m_hashgrid[0].stride.y);
	aray.origin = pt;
	aray.dir = m_hashgrid[0].Basis.GetRow(2) *
	           ((m_hashgrid[0].step.x * m_hashgrid[0].size.x) + (m_hashgrid[0].step.y * m_hashgrid[0].size.y));
	for (i = m_pHashGrid[0][icell]; i < m_pHashGrid[0][icell + 1]; i++)
	{
		atri.pt[0] = m_pVertices[m_pIndices[(m_pHashData[0][i] * 3) + 0]];
		atri.pt[1] = m_pVertices[m_pIndices[(m_pHashData[0][i] * 3) + 1]];
		atri.pt[2] = m_pVertices[m_pIndices[(m_pHashData[0][i] * 3) + 2]];
		atri.n = m_pNormals[m_pHashData[0][i]];
		if (ray_tri_intersection(&aray, &atri, &inters) && (inters.pt[0] - aray.origin) * aray.dir < mindist)
		{
			mindist = (inters.pt[0] - aray.origin) * aray.dir;
			res = isnonneg(aray.dir * atri.n);
			if (pHitTri)
			{
				pHitTri->pt[0] = atri.pt[0];
				pHitTri->pt[1] = atri.pt[1];
				pHitTri->pt[2] = atri.pt[2];
				pHitTri->n = atri.n;
				pHitTri->idx = m_pHashData[0][i];
			}
		}
	}

	return res;
}

void CTriMesh::PrepareForRayTest(float raylen)
{
	{
		ReadLock lock(m_lockUpdate);
		if (m_nHashPlanes > 0)
		{
			return;
		}
	}
	WriteLock lock(m_lockUpdate);
	if (m_nHashPlanes == 0)
	{
		primitives::coord_plane hashplane;
		primitives::box bbox;
		m_pTree->GetBBox(&bbox);
		float rcellsize = 0;
		Vec3i isz;
		int i, iPlane, b2Planes;
		Vec3 szOrg = bbox.size;
		bbox.size.x = max(bbox.size.x, raylen * 0.5f);
		bbox.size.y = max(bbox.size.y, raylen * 0.5f);
		bbox.size.z = max(bbox.size.z, raylen * 0.5f);
		for (i = 1, iPlane = 0; i < 3; i++)
		{
			iPlane += (i - iPlane) & -isneg(fabs_tpl(bbox.Basis.GetRow(iPlane) * m_pNormals[0]) -
			                                fabs_tpl(bbox.Basis.GetRow(i) * m_pNormals[0]));
		}
		i = sgnnz(bbox.Basis.GetRow(iPlane) * m_pNormals[0]);
		bbox.Basis.SetRow(iPlane, bbox.Basis.GetRow(iPlane) * i);
		bbox.Basis.SetRow(inc_mod3[iPlane], bbox.Basis.GetRow(inc_mod3[iPlane]) * i);

		if (raylen > 0)
		{
			rcellsize = 1.0f / raylen;
			for (i = 0; i < 3; i++)
			{
				isz[i] = physics_float2int((bbox.size[i] * 2 * rcellsize) + 0.5f);
			}
			for (i = 0; i < 3; i++)
			{
				if (isz[inc_mod3[i]] * isz[dec_mod3[i]] > 4096)
				{
					isz[inc_mod3[i]] = min(64, isz[inc_mod3[i]]);
					isz[dec_mod3[i]] = min(64, isz[dec_mod3[i]]);
				}
			}
			if (m_nTris > (isz.x * isz.y + isz.x * isz.z + isz.y * isz.z) * 6)
			{
				return; // hash will be too inefficient
			}
		}

		iPlane = isneg(
		    (bbox.size.y * bbox.size.z) -
		    (bbox.size.x * bbox.size.z)); // initially set iPlane correspond to the bbox axis w/ max. face area
		iPlane |=
		    isneg((bbox.size[inc_mod3[iPlane]] * bbox.size[dec_mod3[iPlane]]) - (bbox.size.x * bbox.size.y))
		    << 1;
		iPlane &= ~(iPlane >> 1);
		b2Planes = isneg(raylen - (szOrg[iPlane] * 2));
		for (i = 0; i < 1 + b2Planes; i++, iPlane = inc_mod3[iPlane])
		{
			hashplane.n = bbox.Basis.GetRow(iPlane);
			hashplane.axes[0] = bbox.Basis.GetRow(inc_mod3[iPlane]);
			hashplane.axes[1] = bbox.Basis.GetRow(dec_mod3[iPlane]);
			hashplane.origin = bbox.center;
			HashTrianglesToPlane(
			    hashplane, vector2df(bbox.size[inc_mod3[iPlane]] * 2, bbox.size[dec_mod3[iPlane]] * 2),
			    m_hashgrid[i], m_pHashGrid[i], m_pHashData[i], rcellsize);
		}
		m_nHashPlanes = i;
	}
}

void CTriMesh::HashTrianglesToPlane(const primitives::coord_plane& hashplane, const vector2df& hashsize,
                                    primitives::grid& hashgrid, index_t*& pHashGrid, index_t*& pHashData,
                                    float rcellsize)
{
	float maxsz, edge2[3], tsx, tex, tsy, tey, ts, te;
	vector2df sz, pt[3], edge[3], step, rstep, ptc, sg;
	vector2di isz, irect[2], ipt;
	int i, itri, ipass, bFilled, imax, ix, iy, idx;
	index_t dummy, *pgrid, iHashMax = 0;
	Vec3 ptcur, origin;

	maxsz = max(hashsize.x, hashsize.y) * 1E-5f;
	sz.set(hashsize.x + maxsz, hashsize.y + maxsz);
	imax = isneg(sz.x - sz.y);
	if (rcellsize == 0)
	{
		isz[imax] = physics_float2int(sqrt_tpl(m_nTris * 2 * sz[imax] / sz[imax ^ 1]));
		isz[imax ^ 1] = max(1, m_nTris * 2 / isz[imax]);
	}
	else
	{
		isz.x = physics_float2int((sz.x * rcellsize) + 0.5f);
		isz.y = physics_float2int((sz.y * rcellsize) + 0.5f);
	}
	if ((unsigned int)(isz.x * isz.y) > 4096u)
	{
		isz.x = min(64, max(1, isz.x));
		isz.y = min(64, max(1, isz.y));
	}
	else if (isz.x * isz.y == 0)
	{
		return;
	}
	memset(pHashGrid = new index_t[(isz.x * isz.y) + 1], 0, (isz.x * isz.y + 1) * sizeof(index_t));
	step.set(sz.x / isz.x, sz.y / isz.y);
	rstep.set(isz.x / sz.x, isz.y / sz.y);
	origin = hashplane.origin - hashplane.axes[0] * (step.x * isz.x * 0.5f) -
	         hashplane.axes[1] * (step.y * isz.y * 0.5f);
	pHashData = &dummy;

	for (ipass = 0; ipass < 2; ipass++)
	{
		for (itri = m_nTris - 1; itri >= 0; itri--)
		{ // iterate tris in reversed order to get them in accending order in grid (since the algorithm reverses
		  // order)
			i = isneg(m_pNormals[itri] * hashplane.n) << 1;
			ptcur = m_pVertices[m_pIndices[(itri * 3) + i]] - origin;
			pt[0].set(ptcur * hashplane.axes[0], ptcur * hashplane.axes[1]);
			ptcur = m_pVertices[m_pIndices[(itri * 3) + 1]] - origin;
			pt[1].set(ptcur * hashplane.axes[0], ptcur * hashplane.axes[1]);
			ptcur = m_pVertices[m_pIndices[(itri * 3) + (i ^ 2)]] - origin;
			pt[2].set(ptcur * hashplane.axes[0], ptcur * hashplane.axes[1]);
			edge2[0] = len2(edge[0] = pt[1] - pt[0]);
			edge2[1] = len2(edge[1] = pt[2] - pt[1]);
			edge2[2] = len2(edge[2] = pt[0] - pt[2]);
			irect[1] = irect[0].set(physics_float2int((pt[0].x * rstep.x) - 0.5f),
			                        physics_float2int((pt[0].y * rstep.y) - 0.5f));
			ipt.set(physics_float2int((pt[1].x * rstep.x) - 0.5f),
			        physics_float2int((pt[1].y * rstep.y) - 0.5f));
			irect[0].set(min(irect[0].x, ipt.x), min(irect[0].y, ipt.y));
			irect[1].set(max(irect[1].x, ipt.x), max(irect[1].y, ipt.y));
			ipt.set(physics_float2int((pt[2].x * rstep.x) - 0.5f),
			        physics_float2int((pt[2].y * rstep.y) - 0.5f));
			irect[0].set(max(0, min(irect[0].x, ipt.x)), max(0, min(irect[0].y, ipt.y)));
			irect[1].set(min(isz.x - 1, max(irect[1].x, ipt.x)), min(isz.y - 1, max(irect[1].y, ipt.y)));

			for (iy = irect[0].y; iy <= irect[1].y; iy++)
			{
				for (pgrid = pHashGrid + (iy * isz.x) + (ix = irect[0].x),
				    ptc.set((ix + 0.5f) * step.x, (iy + 0.5f) * step.y);
				     ix <= irect[1].x; ix++, pgrid++, ptc.x += step.x)
				{
					bFilled =
					    isneg(ptc - pt[0] ^ edge[0]) & isneg(ptc - pt[1] ^ edge[1]) &
					    isneg(ptc - pt[2] ^ edge[2]); // check if cell center is inside triangle
					for (i = 0; i < 3; i++)
					{ // for each edge, find intersections with cell x and y bounds, then check
					  // intersection of x and y ranges
						sg.x = sgn(edge[i].x);
						sg.y = sgn(edge[i].y);
						tsx = max(0.0f, (ptc.x - step.x * sg.x * 0.5f - pt[i].x) * sg.x);
						tex = min(fabsf(edge[i].x),
						          (ptc.x + step.x * sg.x * 0.5f - pt[i].x) * sg.x);
						tsy = max(0.0f, (ptc.y - step.y * sg.y * 0.5f - pt[i].y) * sg.y);
						tey = min(fabsf(edge[i].y),
						          (ptc.y + step.y * sg.y * 0.5f - pt[i].y) * sg.y);
						ts = (tsx * fabsf(edge[i].y)) + (tsy * fabsf(edge[i].x)) +
						     fabsf((tsx * fabsf(edge[i].y)) - (tsy * fabsf(edge[i].x)));
						te = (tex * fabsf(edge[i].y)) + (tey * fabsf(edge[i].x)) -
						     fabsf((tex * fabsf(edge[i].y)) - (tey * fabsf(edge[i].x)));
						// test tsx,tex and tsy,tey as well, since ts,te might be indefinite
						// (0,0)
						bFilled |= isneg(tsx - tex - 1E-10f) & isneg(tsy - tey - 1E-10f) &
						           isneg(ts - te - 1E-10f);
					}
					*pgrid -= ipass & -bFilled; // during the 2nd pass if bFilled, decrement *pgrid
					                            // prior to storing
					idx = max(0, min(iHashMax, *pgrid));
					(pHashData[idx] &= ~-bFilled) |= itri & -bFilled; // store itri if bFilled is 1
					*pgrid +=
					    (ipass ^ 1) &
					    bFilled; // during the 1st pass if bFilled, increment *pgrid after storing
				}
			}
		}
		if (ipass == 0)
		{
			for (i = 1; i <= isz.x * isz.y; i++)
			{
				pHashGrid[i] += pHashGrid[i - 1];
			}
			pHashData = new index_t[iHashMax = pHashGrid[isz.x * isz.y]];
			memset(pHashData, 0, iHashMax * sizeof(index_t));
			iHashMax--;
		}
	}

	hashgrid.Basis.SetRow(0, hashplane.axes[0]);
	hashgrid.Basis.SetRow(1, hashplane.axes[1]);
	hashgrid.Basis.SetRow(2, hashplane.n);
	hashgrid.origin = origin;
	hashgrid.step = step;
	hashgrid.stepr = rstep;
	hashgrid.size = isz;
	hashgrid.stride.set(1, isz.x);
}

int CTriMesh::Intersect(IGeometry* pCollider, geom_world_data* pdata1, geom_world_data* pdata2,
                        intersection_params* pparams, geom_contact*& pcontacts)
{
	if (pCollider->GetType() == GEOM_RAY && m_nHashPlanes > 0)
	{
		primitives::ray aray;
		int trilist[3][1024], nTris[3];
		int iPlane, iListPlane, iListRes{}, iListTmp, ix, iy, iCell;
		vector2df pt[2];
		vector2di ipt[2], irect[2];
		float rscale = pdata1->scale == 1.0f ? 1.0f : 1.0f / pdata1->scale;
		primitives::indexed_triangle atri;
		prim_inters inters;
		unprojection_mode unproj;
		int i{}, j, i1, jmax, nSmallSteps, iEdge, bActive, bThreadSafe, bThreadSafeMesh, nContacts = 0;
		int iCaller = GetCaller();
		intptr_t idmask = ~iszero_mask(m_pIds);
		char idnull = (char)-1, *pidnull = &idnull,
		     *pIds = (char*)(((intptr_t)m_pIds & idmask) | ((intptr_t)pidnull & ~idmask));
		bool bStopAtFirstTri;
		real t;
		contact contact_best, contact_cur;
		CRayGeom* pRay = (CRayGeom*)pCollider;
		primitives::ray* pray = (primitives::ray*)&pRay->m_ray;

		aray.origin = pray->origin;
		aray.dir = pray->dir;
		if (pdata2)
		{
			aray.origin = pdata2->offset + pray->origin * pdata2->scale;
			aray.dir = pdata2->R * pray->dir * pdata2->scale;
		}
		aray.origin = ((aray.origin - pdata1->offset) * pdata1->R) * rscale;
		aray.dir = (aray.dir * pdata1->R) * rscale;
		unproj.dir.zero();

		if (pparams)
		{
			if (((CGeometry*)pCollider)->m_iCollPriority < m_iCollPriority)
			{
				unproj.imode = -1;
			}
			else
			{
				unproj.imode = pparams->iUnprojectionMode;
				unproj.center = ((pparams->centerOfRotation - pdata1->offset) * pdata1->R) * rscale;
				if (unproj.imode)
				{
					unproj.dir = pparams->axisOfRotation;
					unproj.tmax = g_PI * 0.5;
				}
				else
				{
					unproj.dir = -pdata1->v + (pdata2 ? pdata2->v : Vec3());
					if (unproj.dir.len2() > 0)
					{
						unproj.dir.normalize();
					}
					unproj.tmax = pparams->maxUnproj;
				}
				unproj.dir = unproj.dir * pdata1->R;
				unproj.minPtDist = m_minVtxDist;
			}
			bStopAtFirstTri = pparams->bStopAtFirstTri;
			pparams->pGlobalContacts = g_idata[iCaller].Contacts;
			pparams->plock = &g_idata[iCaller].lockIntersect;
			bThreadSafe = pparams->bThreadSafe;
			bThreadSafeMesh = pparams->bThreadSafeMesh;
			j = pparams->bKeepPrevContacts;
			bActive = 0;
		}
		else
		{
			unproj.imode = -1;
			bStopAtFirstTri = false;
			bThreadSafe = bThreadSafeMesh = 0;
			j = 0;
			bActive = 1;
		}

		WriteLockCond lock(g_idata[iCaller].lockIntersect, bThreadSafe ^ 1);
		ReadLockCond lock0(m_lockUpdate, bThreadSafeMesh ^ 1);
		if (m_nHashPlanes <= 0) // can be changed by now due to mt
		{
			goto skiphashes;
		}
		pcontacts = g_idata[iCaller].Contacts + g_idata[iCaller].nTotContacts;
		if (!j)
		{
			g_idata[iCaller].nTotContacts = 0;
		}

		if ((unsigned int)pdata1->iStartNode - 1u < (unsigned int)m_nTris)
		{
			atri.n = m_pNormals[pdata1->iStartNode - 1];
			atri.pt[0] = m_pVertices[m_pIndices[((pdata1->iStartNode - 1) * 3) + 0]];
			atri.pt[1] = m_pVertices[m_pIndices[((pdata1->iStartNode - 1) * 3) + 1]];
			atri.pt[2] = m_pVertices[m_pIndices[((pdata1->iStartNode - 1) * 3) + 2]];
			atri.idx = pdata1->iStartNode - 1;
			if (ray_tri_intersection(&aray, &atri, &inters))
			{
				bStopAtFirstTri = true;
				goto cached_inters;
			}
		}

		for (iPlane = iListRes = 0; iPlane < m_nHashPlanes; iPlane++)
		{
			pt[0] = vector2df(m_hashgrid[iPlane].Basis * (aray.origin - m_hashgrid[iPlane].origin));
			pt[1] = pt[0] + vector2df(m_hashgrid[iPlane].Basis * aray.dir);
			ipt[0].set(physics_float2int((pt[0].x * m_hashgrid[iPlane].stepr.x) - 0.5f),
			           physics_float2int((pt[0].y * m_hashgrid[iPlane].stepr.y) - 0.5f));
			ipt[1].set(physics_float2int((pt[1].x * m_hashgrid[iPlane].stepr.x) - 0.5f),
			           physics_float2int((pt[1].y * m_hashgrid[iPlane].stepr.y) - 0.5f));
			irect[0].set(max(0, min(ipt[0].x, ipt[1].x)), max(0, min(ipt[0].y, ipt[1].y)));
			irect[1].set(min(m_hashgrid[iPlane].size.x - 1, max(ipt[0].x, ipt[1].x)),
			             min(m_hashgrid[iPlane].size.y - 1, max(ipt[0].y, ipt[1].y)));
			if ((irect[0].x + 1 - irect[1].x) >> 31 |
			    (irect[0].x + 1 - irect[1].x) >> 31) // can be ineffective for long rays
			{
				goto skiphashes;
			}

			nTris[iListPlane = inc_mod3[iListRes]] = 0;
			iListTmp = inc_mod3[iListPlane];
			for (iy = irect[0].y; iy <= irect[1].y; iy++)
			{
				for (iCell = (iy * m_hashgrid[iPlane].stride.y) +
				             ((ix = irect[0].x) * m_hashgrid[iPlane].stride.x);
				     ix <= irect[1].x; ix++, iCell += m_hashgrid[iPlane].stride.x)
				{
					iListPlane ^= iListTmp;
					iListTmp ^= iListPlane;
					iListPlane ^= iListTmp;
					nTris[iListPlane] =
					    unite_lists(m_pHashData[iPlane] + m_pHashGrid[iPlane][iCell],
					                m_pHashGrid[iPlane][iCell + 1] - m_pHashGrid[iPlane][iCell],
					                trilist[iListTmp], nTris[iListTmp], trilist[iListPlane],
					                sizeof(trilist[0]) / sizeof(trilist[0][0]));
				}
			}
			if (iPlane > 0)
			{
				nTris[iListTmp] =
				    intersect_lists(trilist[iListRes], nTris[iListRes], trilist[iListPlane],
				                    nTris[iListPlane], trilist[iListTmp]);
				iListRes = iListTmp;
			}
			else
			{
				iListRes = iListPlane;
			}
		}

		if (nTris[iListRes])
		{
			for (i = 0; i < nTris[iListRes] &&
			            g_idata[iCaller].nTotContacts + nContacts <
			                sizeof(g_idata[iCaller].Contacts) / sizeof(g_idata[iCaller].Contacts[0]);
			     i++)
			{
				atri.n = m_pNormals[trilist[iListRes][i]];
				atri.pt[0] = m_pVertices[m_pIndices[(trilist[iListRes][i] * 3) + 0]];
				atri.pt[1] = m_pVertices[m_pIndices[(trilist[iListRes][i] * 3) + 1]];
				atri.pt[2] = m_pVertices[m_pIndices[(trilist[iListRes][i] * 3) + 2]];
				atri.idx = trilist[iListRes][i];
				if (ray_tri_intersection(&aray, &atri, &inters))
				{
				cached_inters:
					if (unproj.imode >= 0)
					{
						// here goes a simplified version of RegisterIntersection
						if (unproj.imode >= 0 && unproj.dir.len2() == 0)
						{
							if (atri.n * aray.dir > 0)
							{
								continue;
							}
							unproj.dir = atri.n ^ aray.dir;
							if (unproj.imode == 0)
							{
								unproj.dir = unproj.dir ^ aray.dir;
							}
							// if (unproj.dir.len2()<0.002) unproj.dir =
							// aray.dir.orthogonal();
							if (unproj.dir.len2() < 0.002)
							{
								unproj.dir.SetOrthogonal(aray.dir);
							}
							unproj.dir.normalize();
						}

						if (!g_Unprojector.Check(&unproj, primitives::triangle::type,
						                         primitives::ray::type, &atri, -1, &aray, -1,
						                         &contact_best))
						{
							continue;
						}
						if (unproj.imode)
						{
							contact_best.t = atan2(contact_best.t, contact_best.taux);
						}
						pcontacts[nContacts].iPrim[0] = atri.idx;

						for (nSmallSteps = 0, t = 0; contact_best.t > 0 && nSmallSteps < 3 &&
						                             (contact_best.iFeature[0] & 0x80);)
						{
							if (unproj.imode)
							{
								contact_best.t =
								    atan2(contact_best.t, contact_best.taux);
							}
							t += contact_best.t;
							if (t > unproj.tmax)
							{
								break;
							}

							if ((j = m_pTopology[atri.idx]
							             .ibuddy[contact_best.iFeature[0] & 0x1F]) < 0)
							{
								break;
							}
							atri.n = m_pNormals[j];
							if (unproj.imode)
							{
								real cost = cos_tpl(t), sint = sin_tpl(t);
								for (i1 = 0; i1 < 3; i1++)
								{
									atri.pt[i1] =
									    m_pVertices[m_pIndices[(j * 3) + i1]]
										.GetRotated(unproj.center, unproj.dir,
									                    cost, sint);
								}
								atri.n = atri.n.GetRotated(unproj.dir, cost, sint);
							}
							else
							{
								for (i1 = 0; i1 < 3; i1++)
								{
									atri.pt[i1] =
									    m_pVertices[m_pIndices[(j * 3) + i1]] +
									    unproj.dir * t;
								}
							}
							iEdge = GetEdgeByBuddy(j, atri.idx);
							atri.idx = j;

							contact_best.t = 0;
							if (g_Unprojector.Check(&unproj, primitives::triangle::type,
							                        primitives::ray::type, &atri,
							                        iEdge | 0xA0, &aray, 0xA0,
							                        &contact_cur))
							{
								contact_best = contact_cur;
								pcontacts[nContacts].iPrim[0] = atri.idx;
								if (unproj.imode)
								{
									contact_best.t =
									    atan2(contact_best.t, contact_best.taux);
									j = isneg(contact_best.t - (real)1E-3);
								}
								else
								{
									j = isneg(contact_best.t - m_minVtxDist);
								}
								nSmallSteps = (nSmallSteps & -j) + j;
							}
						}

						pcontacts[nContacts].t =
						    (t + contact_best.t) *
						    (unproj.imode + (1 - unproj.imode) * pdata1->scale);
						pcontacts[nContacts].pt =
						    pdata1->R * contact_best.pt * pdata1->scale + pdata1->offset;
						pcontacts[nContacts].n = pdata1->R * contact_best.n; // not normalized!
						pcontacts[nContacts].dir = pdata1->R * unproj.dir;
						pcontacts[nContacts].iFeature[0] = contact_best.iFeature[0];
						pcontacts[nContacts].iFeature[1] = contact_best.iFeature[1];
						pcontacts[nContacts].iUnprojMode = unproj.imode;
					}
					else
					{
						pcontacts[nContacts].pt =
						    pdata1->R * inters.pt[0] * pdata1->scale + pdata1->offset;
						pcontacts[nContacts].t =
						    (pdata1->R * (inters.pt[0] - aray.origin)) *
						    (pdata2 ? pdata2->R * pRay->m_dirn : pRay->m_dirn);
						pcontacts[nContacts].n = pdata1->R * inters.n;
						pcontacts[nContacts].dir.zero();
						pcontacts[nContacts].iPrim[0] = atri.idx;
						pcontacts[nContacts].iFeature[0] = 0x40;
						pcontacts[nContacts].iFeature[1] = 0x20;
						pcontacts[nContacts].iUnprojMode = 0;
					}

					pcontacts[nContacts].vel = 0;
					pcontacts[nContacts].ptborder = &pcontacts[nContacts].pt;
					pcontacts[nContacts].nborderpt = 1;
					pcontacts[nContacts].parea = 0;
					pcontacts[nContacts].id[0] = pIds[atri.idx & idmask];
					pcontacts[nContacts].id[1] = -1;
					pcontacts[nContacts].iPrim[1] = 0;
					pcontacts[nContacts].iNode[0] = pcontacts[nContacts].iPrim[0] + 1;
					pcontacts[nContacts].iNode[1] = 0;
					nContacts++;
					if (bStopAtFirstTri)
					{
						break;
					}
				}
			}

			// sort contacts in descending t order
			geom_contact tmpcontact;
			for (i = 0; i < nContacts - 1; i++)
			{
				for (jmax = i, j = i + 1; j < nContacts; j++)
				{
					idmask = -isneg(pcontacts[jmax].t - pcontacts[j].t);
					jmax = (jmax & ~idmask) | (j & idmask);
				}
				if (jmax != i)
				{
					tmpcontact = pcontacts[i];
					pcontacts[i] = pcontacts[jmax];
					pcontacts[jmax] = tmpcontact;
				}
			}

			lock.SetActive((bThreadSafe ^ 1) & (bActive | iszero(nContacts)));
			return nContacts;
		}
		return 0;
	}
skiphashes:
	return CGeometry::Intersect(pCollider, pdata1, pdata2, pparams, pcontacts);
}

int CTriMesh::Subtract(IGeometry* pGeom, geom_world_data* pdata1, geom_world_data* pdata2, int bLogUpdates)
{
	if (pGeom->GetType() != GEOM_TRIMESH)
	{
		return 0;
	}

	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	int i, j, iop, icont, ncont, ipt, idx, ivtx, ivtx0, ivtx1, itri, itri1, imask, ipoly, flags, ivtxStart, ivtxEnd;
	int nVtx, nPolies, nTriSlots, nTriSlotsAlloc, nVtxSlots, nVtxSlotsAlloc, nBTris, nBTrisAlloc, nTris, nTrisAlloc,
	    nTris0 = m_nTris, nVtx0 = m_nVertices, nErrors0 = m_nErrors, nMaxPolyVtx, nNewVtx, nNewTris, bVtxMap,
	    nVtxAlloc = m_nVertices, nTriAlloc = m_nTris, nNewVtxAlloc = 0, nNewTrisAlloc = 0, nIsles, nTries = 1,
	    bCheckVolumes = 0;
	int iCurTri[2], iCurVtx[2], iCurVtx0[2], iCurPoly[2], iVtxCntStart[2], iCurTwin[2], iTwin0[2], bIsolatedCnt[2],
	    idmask[2], matmask[2];
	int *pTriSlots, *pVtxSlots, *pTris, *pBTris, *pBVtxMap, *pBVtxMapNew, *pVtxMap;
	int iCaller = GetCaller();
	unsigned int *pTriMask[2], *pVtxMask[2];
	Vec3 n, edge, edge1, *pdata0 = m_pVertices.data, *pBackupVertices{}, *pBackupNormals{};
	int *pIds[2], dummyId, *pIds0 = m_pForeignIdx, *pBackupIds{}, *pBackupVtxMap{};
	char *pMats[2], dummyMat, *pMats0 = m_pIds, *pBackupMats{};
	index_t *pIndices0 = m_pIndices, *pBackupIndices{};
	trinfo* pTopology0 = m_pTopology;
	CRayGeom rayGeom;
	primitives::box bbox;
	float e = sqr(m_minVtxDist * 2), e1, e2, denom, rscale = 1 / pdata1->scale,
	      rscale2[2] = {1, pdata1->scale / pdata2->scale}, V[3];
	tessvtx* pVtx;
	tesspoly* pPolies;
	CTriMesh* pMesh[2] = {this, (CTriMesh*)pGeom};
	geom_world_data gwd[2];
	intersection_params ip;
	geom_contact *pcont, *pcont1;
	bop_meshupdate* pRes = 0;
	ip.bExactBorder = 1;
	ip.bThreadSafe = ip.bThreadSafeMesh = 1;

	{
		ReadLock lock0(m_lockUpdate);
		Matrix33 R;
		pMesh[0]->GetBBox(&bbox);
		n = pdata1->R * bbox.center * pdata1->scale + pdata1->offset;
		edge = ((R = pdata1->R * bbox.Basis.T()).Fabs() * bbox.size) * pdata1->scale;
		pMesh[1]->GetBBox(&bbox);
		n -= pdata2->R * bbox.center * pdata2->scale + pdata2->offset;
		edge += ((R = pdata2->R * bbox.Basis.T()).Fabs() * bbox.size) * pdata2->scale;
		if (max(max(fabs_tpl(n.x) - edge.x, fabs_tpl(n.y) - edge.y), fabs_tpl(n.z) - edge.z) >= 0)
		{
			return 0;
		}
	}
	g_random_generator.seed(12102012);

	{
		WriteLock lock1(g_idata[iCaller].lockIntersect);
		WriteLock lock0(m_lockUpdate);

		if (m_pVertices.iStride != sizeof(Vec3))
		{
			m_pVertices.data = new Vec3[m_nVertices];
			for (i = 0; i < m_nVertices; i++)
			{
				m_pVertices.data[i] = strided_pointer<Vec3>(pdata0, m_pVertices.iStride)[i];
			}
			m_pVertices.iStride = sizeof(Vec3);
		}
		for (i = 0; i < 2; i++)
		{
			if (pMesh[i]->m_pForeignIdx)
			{
				pIds[i] = pMesh[i]->m_pForeignIdx;
				idmask[i] = -1;
			}
			else
			{
				pIds[i] = &dummyId;
				idmask[i] = 0;
			}
			if (pMesh[i]->m_pIds)
			{
				pMats[i] = pMesh[i]->m_pIds;
				matmask[i] = -1;
			}
			else
			{
				pMats[i] = &dummyMat;
				matmask[i] = 0;
			}
		}
		if (m_pVtxMap)
		{
			pVtxMap = m_pVtxMap;
			bVtxMap = -1;
		}
		else
		{
			pVtxMap = &dummyId;
			bVtxMap = 0;
		}

		nIsles = pMesh[1]->BuildIslandMap();
		for (i = 0; i < nIsles; i++)
		{
			pMesh[1]->m_pIslands[i].bProcessed = 0;
		}

		gwd[1].scale = pdata2->scale * rscale;
		e1 = (bbox.size.x + bbox.size.y + bbox.size.z) * gwd[1].scale * 0.003f;
	RetryWithJitter:
		gwd[1].R = pdata1->R.T() * pdata2->R;
		gwd[1].offset = ((pdata2->offset - pdata1->offset) * pdata1->R) * rscale;
		if (nTries > 1)
		{
			if (nTries >= 5)
			{
				return 0;
			}
			edge.Set(physics_frand(1.0f), physics_frand(1.0f), physics_frand(1.0f)).normalize();
			gwd[1].R *= Matrix33::CreateRotationAA(0.01f, edge);
			edge.Set(e1 * physics_frand(1.0f), e1 * physics_frand(1.0f), e1 * physics_frand(1.0f));
			gwd[1].offset += edge;
		}
		ip.bKeepPrevContacts = false;

		ncont = Intersect(pGeom, gwd, gwd + 1, &ip, pcont);

		for (i = nVtx = 0; i < ncont; i++)
		{
			for (j = 1; j < pcont[i].nborderpt && (pcont[i].ptborder[j] - pcont[i].ptborder[0]).len2() < e;
			     j++)
				;
			if (j == pcont[i].nborderpt)
			{
				pcont[i].nborderpt = 0;
			}
			else
			{
				if (!pcont[i].bClosed)
				{
					++nTries;
					goto RetryWithJitter;
				}
				nVtx += pcont[i].nborderpt;
				pMesh[1]
				    ->m_pIslands[pMesh[1]->m_pTri2Island[pcont[i].idxborder[0][1] & IDXMASK].iprev]
				    .bProcessed = 1;
			}
		}

		nMaxPolyVtx = 0;
		nNewVtx = 0;
		nNewTris = 0;
		pPolies = new tesspoly[(nVtx * 2) + 1];
		nPolies = 0;
		pVtx = new tessvtx[(nVtx * 8) + 1];
		nVtx = 0;
		for (iop = 0; iop < 2; iop++)
		{
			i = ((pMesh[iop]->m_nVertices - 1) >> 5) + 1;
			memset(pVtxMask[iop] = new unsigned int[i], 0, i * 4);
			i = (pMesh[iop]->m_nTris >> 5) + 1;
			memset(pTriMask[iop] = new unsigned int[i], 0, i * 4);
		}
		pBVtxMap = new int[pMesh[1]->m_nVertices];
		pBVtxMapNew = new int[pMesh[1]->m_nVertices];
		pTriSlots = new int[nTriSlotsAlloc = 32];
		nTriSlots = 0;
		pVtxSlots = new int[nVtxSlotsAlloc = 32];
		nVtxSlots = 0;
		pBTris = new int[nBTrisAlloc = 32];
		nBTris = 0;
		pTris = new int[nTrisAlloc = 32];
		if (bLogUpdates)
		{
			pRes = new bop_meshupdate;
		}
		dummyId = 0;
		g_nTriangulationErrors = 0;

		for (icont = 0; icont < ncont; icont++)
		{
			iCurTri[0] = iCurTri[1] = iCurVtx[0] = iCurVtx[1] = iVtxCntStart[0] = iVtxCntStart[1] =
			    iCurTwin[0] = iCurTwin[1] = iTwin0[0] = iTwin0[1] = -1;
			bIsolatedCnt[0] = bIsolatedCnt[1] = 1;
			for (ipt = 1; ipt < pcont[icont].nborderpt; ipt++)
			{
				for (iop = 0; iop < 2; iop++)
				{
					itri = pcont[icont].idxborder[ipt][iop] & IDXMASK;
					if (itri == iCurTri[iop])
					{
						ivtx = iCurVtx[iop];
						ipoly = iCurPoly[iop];
					}
					else
					{
						if (check_mask(pTriMask[iop],
						               itri)) // if triangle is 'processed', lookup its ipoly
						{
							for (ipoly = 0; ipoly < nPolies &&
							                pPolies[ipoly].itri != (iop << 30 | itri);
							     ipoly++)
								;
						}
						else
						{ // triangle wasn't encountered before, initialize a poly for it
							set_mask(pTriMask[iop], itri);
							if (iop == 0)
							{
								if (nTriSlots >= nTriSlotsAlloc - 1)
								{
									ReallocateList(pTriSlots, nTriSlots,
									               nTriSlotsAlloc += 16);
								}
								pTriSlots[nTriSlots++] = itri;
							}
							pPolies[ipoly = nPolies++].itri = iop << 30 | itri;
							for (j = 0; j < 3; j++)
							{
								pVtx[nVtx + j].inextBrd = nVtx + inc_mod3[j];
								pVtx[nVtx + j].inext = -1;
								pVtx[nVtx + j].icont = -(iop + 1);
								idx = 2 - j; // idx = (iop-1&3)+(j^~-iop); // for
								             // boolean addition, use 2-j for A, j for B
								pVtx[nVtx + j].ivtx =
								    pMesh[iop]->m_pIndices[(itri * 3) + idx];
								pVtx[nVtx + j].pt =
								    gwd[iop].R *
									pMesh[iop]->m_pVertices[pVtx[nVtx + j].ivtx] *
									gwd[iop].scale +
								    gwd[iop].offset;
								pVtx[nVtx + j].flags = (idx + 1) | vtx_processed;
								set_mask(
								    pVtxMask[iop],
								    pVtx[nVtx + j]
									.ivtx); // we need all 3 vertices marked
								                // (instead of just those that end up
								// in the polygon) since intersection points can be
								// substituted with original vertices
							}
							pVtx[nVtx].t = 0;
							pVtx[nVtx + 1].t = (pVtx[nVtx + 1].pt - pVtx[nVtx].pt).len2();
							pVtx[nVtx + 2].t =
							    (pVtx[nVtx + 2].pt - pVtx[nVtx + 1].pt).len2() +
							    pVtx[nVtx + 1].t;
							pVtx[nVtx].ipoly = pVtx[nVtx + 1].ipoly = pVtx[nVtx + 2].ipoly =
							    ipoly;
							pVtx[nVtx].itwin = pVtx[nVtx + 1].itwin = pVtx[nVtx + 2].itwin =
							    -1;
							pPolies[ipoly].ivtx = pPolies[ipoly].ivtx0 = nVtx;
							pPolies[ipoly].ivtxCont = -1;
							pPolies[ipoly].nvtx = 3;
							pPolies[ipoly].n =
							    gwd[iop].R * pMesh[iop]->m_pNormals[itri] * (1 - (iop * 2));
							pPolies[ipoly].id = pIds[iop][itri & idmask[iop]];
							pPolies[ipoly].mat = pMats[iop][itri & matmask[iop]];
							pPolies[ipoly].holeArea = 0;
							pPolies[ipoly].areaOrg =
							    (1 - iop * 2) *
							    (pPolies[ipoly].n * (pVtx[nVtx + 2].pt - pVtx[nVtx].pt ^
							                         pVtx[nVtx + 1].pt - pVtx[nVtx].pt));
							nVtx += 3;
						}
						// add the previous vertex to this triangle	as stripe start
						pVtx[nVtx].icont = icont;
						pVtx[nVtx].ivtx = ipt - 1;
						pVtx[nVtx].itwin = iCurTwin[iop];
						iTwin0[iop] = (iTwin0[iop] & ~((ipt - 2) >> 31)) |
						              (nVtx & ((ipt - 2) >> 31)); // set iTwin0 for ipt==1
						pVtx[nVtx].ipoly = ipoly;
						pVtx[nVtx].pt = pcont[icont].ptborder[ipt - 1];
						pVtx[nVtx].flags = 0;
						ivtx = nVtx++;
						pPolies[ipoly].nvtx++;
						iCurTri[iop] = itri;
						iCurPoly[iop] = ipoly;
						iCurVtx0[iop] = ivtx;
						iVtxCntStart[iop] += (iCurVtx0[iop] + 1) & iVtxCntStart[iop] >> 31;
					}
					pVtx[nVtx].inext = -1;
					pVtx[ivtx].inext = nVtx;
					pVtx[nVtx].icont = icont;
					// pVtx[nVtx].ivtx = ipt & ipt+1-pcont[icont].nborderpt>>31;	// replace
					// nborderpt-1 with 0
					if (ipt < pcont[icont].nborderpt - 1)
					{
						pVtx[nVtx].ivtx = ipt;
						pVtx[nVtx].itwin = -1;
					}
					else
					{
						pVtx[nVtx].ivtx = 0;
						pVtx[nVtx].itwin = iTwin0[iop];
					}
					iCurTwin[iop] = nVtx;
					pVtx[nVtx].ipoly = ipoly;
					pVtx[nVtx].pt = pcont[icont].ptborder[pVtx[nVtx].ivtx];
					pVtx[nVtx].flags = 0;
					pVtx[nVtx].inextBrd = -1;
					if (pcont[icont].idxborder[ipt][iop] & TRIEND)
					{
						if (pVtx[iCurVtx0[iop]].ivtx > 0 ||
						    (pcont[icont].idxborder[pcont[icont].nborderpt - 1][iop] & TRIEND))
						{
							insertBorderVtx(pVtx, pPolies[ipoly].ivtx, iCurVtx0[iop],
							                pPolies[ipoly].n * ((iop * 2) - 1), 0);
						}
						insertBorderVtx(pVtx, pPolies[ipoly].ivtx, nVtx,
						                pPolies[ipoly].n * ((iop * 2) - 1), 1);
						pPolies[ipoly].holeArea =
						    -1E10f; // never remove border based on hole area if there are
						            // border intersections
						iCurVtx[iop] = -1;
						bIsolatedCnt[iop] = 0;
						nMaxPolyVtx = max(
						    nMaxPolyVtx,
						    ++pPolies[ipoly]
							  .nvtx); // account for a dummy vertex slot for each contour
					}
					else
					{
						iCurVtx[iop] = nVtx;
					}
					nMaxPolyVtx = max(nMaxPolyVtx, ++pPolies[ipoly].nvtx);
					nVtx++;
				}
			}

			for (iop = 0; iop < 2; iop++)
			{
				if (iCurVtx[iop] >= 0)
				{ // if the last triangle's contour didn't end, stitch its pending ends
					pVtx[iCurVtx[iop]].inext = iVtxCntStart[iop];
					nMaxPolyVtx =
					    max(nMaxPolyVtx,
					        ++pPolies[iCurPoly[iop]]
					              .nvtx); // account for a dummy vertex slot for each contour
					if (!bIsolatedCnt[iop])
					{
						insertBorderVtx(pVtx, pPolies[iCurPoly[iop]].ivtx, iCurVtx0[iop],
						                pPolies[iCurPoly[iop]].n * ((iop * 2) - 1), 0);
						pPolies[iCurPoly[iop]].holeArea =
						    -1E10f; // never remove border based on hole area if there are
						            // border intersections
					}
					else
					{ // we have a closed contour fully inside one triangle
						pVtx[iCurVtx[iop]].flags |= contour_end;
						pVtx[iCurVtx0[iop]].flags |= contour_end;
						pVtx[iCurVtx[iop]].inext = pPolies[iCurPoly[iop]].ivtxCont;
						pPolies[iCurPoly[iop]].ivtxCont = iCurVtx0[iop];
						n = pPolies[iCurPoly[iop]].n * ((iop * 2) - 1);
						for (ivtx = iCurVtx0[iop], edge.zero(); ivtx != iCurVtx[iop];
						     ivtx = pVtx[ivtx].inext)
						{
							edge += pVtx[ivtx].pt - n * (n * pVtx[ivtx].pt) ^
							        pVtx[pVtx[ivtx].inext].pt -
							            n * (n * pVtx[pVtx[ivtx].inext].pt);
						}
						edge += pVtx[ivtx].pt - n * (n * pVtx[ivtx].pt) ^
						        pVtx[iCurVtx0[iop]].pt - n * (n * pVtx[iCurVtx0[iop]].pt);
						pPolies[iCurPoly[iop]].holeArea += edge * n;
					}
				}
			}
		}

		// if a vtx is instable and has a twin
		// find twin's triangle, find edge that has this tri as buddy, remove vtx from its pos, move it before
		// or after the vtx
		for (i = 0; i < nVtx; i++)
		{
			pVtx[i].iprev = -1;
		}
		for (i = 0; i < nVtx; i++)
		{
			if (pVtx[i].itwin >= 0)
			{
				pVtx[pVtx[i].itwin].itwin = i;
			}
			if (pVtx[i].inext >= 0)
			{
				pVtx[pVtx[i].inext].iprev = i;
			}
		}
		for (i = 0; i < nVtx; i++)
		{
			if (pVtx[i].flags & vtx_instable && pVtx[i].itwin >= 0)
			{
				ivtx0 = pPolies[pVtx[i].ipoly].ivtx;
				imask = pVtx[i].flags >> vtx_instablept_log2 & 7;
				for (ivtx = ivtx0;
				     ((pVtx[ivtx].flags >> vtx_instablept_log2 & 7) != imask || ivtx == i) &&
				     pVtx[ivtx].inextBrd != ivtx0;
				     ivtx = pVtx[ivtx].inextBrd)
					;
				if ((pVtx[ivtx].flags >> vtx_instablept_log2 & 7) != imask || ivtx == i)
				{
					continue; // continue only if we have at least 2 instable points near one vertex
				}
				for (ivtx = ivtx0; pVtx[ivtx].inextBrd != i && pVtx[ivtx].inextBrd != ivtx0;
				     ivtx = pVtx[ivtx].inextBrd)
					;
				if (pVtx[ivtx].inextBrd != i)
				{
					continue;
				}
				iop = pPolies[pVtx[i].ipoly].itri >> 30;
				itri = pPolies[pVtx[i].ipoly].itri & ((1 << 30) - 1);
				itri1 = pPolies[pVtx[pVtx[i].itwin].ipoly].itri & ((1 << 30) - 1);
				ipt = pMesh[iop]->GetEdgeByBuddy(itri, itri1);
				if (pMesh[iop]->m_pTopology[itri].ibuddy[ipt] == itri1)
				{
					j = (pVtx[i].flags >> vtx_instablept_log2) & 3;
					ipt = dec_mod3[2 - ipt];
					if (j == ipt)
					{ // re-insert the vertex after ipt
						pVtx[ivtx].inextBrd = pVtx[i].inextBrd;
						pVtx[i].inextBrd = pVtx[ivtx0 + ipt].inextBrd;
						pVtx[ivtx0 + ipt].inextBrd = i;
					}
					else if (j == inc_mod3[ipt])
					{ // re-insert the vertex before ipt+1
						pVtx[ivtx].inextBrd = pVtx[i].inextBrd;
						for (ivtx = ivtx0; pVtx[ivtx].inextBrd != ivtx0 + j;
						     ivtx = pVtx[ivtx].inextBrd)
							;
						pVtx[i].inextBrd = ivtx0 + j;
						pVtx[ivtx].inextBrd = i;
					}
				}
			}
		}

		// detect if any 2 points on a tri border are very close to each other
		// and their outgoing edges are criss-crossing. swap them if so
		for (i = 0; i < nPolies; i++)
		{
			iop = pPolies[i].itri >> 30;
			ivtx0 = pPolies[i].ivtx0;
			e2 = ((pVtx[ivtx0 + 1].pt - pVtx[ivtx0].pt).len2() +
			      (pVtx[ivtx0 + 2].pt - pVtx[ivtx0 + 1].pt).len2() +
			      (pVtx[ivtx0].pt - pVtx[ivtx0 + 2].pt).len2()) *
			     sqr(0.001f);
			for (ivtx1 = pPolies[i].ivtx; pVtx[ivtx1].inextBrd != pPolies[i].ivtx;
			     ivtx1 = pVtx[ivtx1].inextBrd)
				;
			for (ivtx0 = pPolies[i].ivtx, ivtx = -1; ivtx != pPolies[i].ivtx; ivtx1 = ivtx0, ivtx0 = ivtx)
			{
				if (sqr(pVtx[ivtx = pVtx[ivtx0].inextBrd].t - pVtx[ivtx0].t) < e2 &&
				    min(pVtx[ivtx].icont, pVtx[ivtx0].icont) >= 0 &&
				    max(pVtx[ivtx0].inext * pVtx[ivtx].inext, pVtx[ivtx0].iprev * pVtx[ivtx].iprev) < 0)
				{
					edge = pVtx[max(pVtx[ivtx].inext, pVtx[ivtx].iprev)].pt - pVtx[ivtx].pt;
					edge1 = pVtx[max(pVtx[ivtx0].inext, pVtx[ivtx0].iprev)].pt - pVtx[ivtx0].pt;
					denom = (edge ^ edge1) * pPolies[i].n;
					if (denom * (iop * 2 - 1) > 0 &&
					    inrange((pVtx[ivtx].pt - pVtx[ivtx0].pt ^ edge1) * pPolies[i].n,
					            denom * -0.1f, denom * 0.8f) &&
					    inrange((pVtx[ivtx].pt - pVtx[ivtx0].pt ^ edge) * pPolies[i].n,
					            denom * -0.1f, denom * 0.8f))
					{ // swap ivtx and ivtx0
						pVtx[ivtx1].inextBrd = ivtx;
						pVtx[ivtx0].inextBrd = pVtx[ivtx].inextBrd;
						pVtx[ivtx].inextBrd = ivtx0;
						ivtx1 = ivtx0;
						ivtx0 = ivtx;
						ivtx = ivtx1;
					}
				}
			}
		}

		// arrange contours inside polygons
		for (i = 0; i < nPolies; i++)
		{
			ivtxEnd = ivtxStart = -1;
			flags = 0;
			ipt = 0;
			do
			{
				for (ivtx0 = pPolies[i].ivtx;
				     pVtx[ivtx0].flags & vtx_processed && pVtx[ivtx0].inextBrd != pPolies[i].ivtx;
				     ivtx0 = pVtx[ivtx0].inextBrd)
					;
				if (pVtx[ivtx0].flags & vtx_processed)
				{
					break;
				}
				if (ivtxEnd < 0)
				{
					ivtxStart = ivtx0;
				}
				else
				{
					pVtx[ivtxEnd].inext = ivtx0;
				}
				pVtx[ivtx0].flags |= flags;
				ivtx = ivtx0;
				do
				{
					pVtx[ivtxEnd = ivtx].flags |= vtx_processed;
					pVtx[ivtx].inext += (pVtx[ivtx].inextBrd + 1) & pVtx[ivtx].inext >> 31;
				}
				while ((ivtx = pVtx[ivtx].inext) != ivtx0 && ++ipt < pPolies[i].nvtx * 2);
				pVtx[ivtxEnd].flags |= flags;
				flags = contour_end;
			}
			while (++ipt < pPolies[i].nvtx * 2);
			if (ipt >= pPolies[i].nvtx * 2)
			{
			BadContours:
				delete[] pBVtxMap;
				delete[] pBVtxMapNew;
				delete[] pTriSlots;
				delete[] pVtxSlots;
				delete[] pBTris;
				delete[] pTris;
				nTries++;
				goto RetryWithJitter;
			}
			if (ivtxStart >= 0)
			{
				pPolies[i].ivtx = ivtxStart;
			}
			else
			{
				for (j = 0; j < 3; j++)
				{
					pVtx[pPolies[i].ivtx + j].inext = pPolies[i].ivtx + inc_mod3[j];
				}
				ivtxEnd = (ivtxStart = pPolies[i].ivtx) + 2;
			}
			if (pPolies[i].ivtxCont >= 0)
			{
				pVtx[ivtxEnd].inext = pPolies[i].ivtxCont;
				for (ivtxEnd = pPolies[i].ivtxCont; pVtx[ivtxEnd].inext >= 0;
				     ivtxEnd = pVtx[ivtxEnd].inext)
					;
				if (pPolies[i].holeArea >
				    0) // we have a "positive" hole - remove triangle border from contour
				{
					pPolies[i].ivtx = ivtxStart = pPolies[i].ivtxCont;
				}
			}
			pVtx[ivtxEnd].inext = ivtxStart;
		}

		// if an A polygon does not have a full edge, flood fill A triangles starting from it and remove them
		// from A
		for (i = 0; i < nPolies; i++)
		{
			if (!(pPolies[i].itri & 1 << 30))
			{
				itri = pPolies[i].itri;
				ivtx = pPolies[i].ivtx;
				imask = 0;
				ipt = 0;
				do
				{
					imask |= 1 << (pVtx[ivtx].flags - 1 & 3);
					if (pVtx[ivtx].icont < 0) // mark A vertices used by polygons
					{
						set_mask(pVtxMask[0], pVtx[ivtx].ivtx);
					}
				}
				while ((ivtx = pVtx[ivtx].inext) != pPolies[i].ivtx && ++ipt < pPolies[i].nvtx * 2);
				if (ipt >= pPolies[i].nvtx * 2)
				{
					goto BadContours;
				}
				imask = (imask >> 1 & 3) | imask << 2; // since vertex order for A is reversed

				for (j = nTris = 0; j < 3; j++)
				{
					if (m_pTopology[itri].ibuddy[j] >= 0 &&
					    ((imask >> j & 1) | check_mask(pTriMask[0], m_pTopology[itri].ibuddy[j])) ==
					        0)
					{
						if (nTriSlots >= nTriSlotsAlloc - 1)
						{
							ReallocateList(pTriSlots, nTriSlots, nTriSlotsAlloc += 16);
						}
						pTris[nTris++] = m_pTopology[itri].ibuddy[j];
						pTriSlots[nTriSlots++] = m_pTopology[itri].ibuddy[j];
						set_mask(pTriMask[0], m_pTopology[itri].ibuddy[j]);
					}
				}
				while (nTris)
				{
					itri = pTris[--nTris];
					for (j = 0; j < 3; j++)
					{
						if (m_pTopology[itri].ibuddy[j] >= 0 &&
						    !check_mask(pTriMask[0], m_pTopology[itri].ibuddy[j]))
						{
							if (nTriSlots >= nTriSlotsAlloc - 1)
							{
								ReallocateList(pTriSlots, nTriSlots,
								               nTriSlotsAlloc += 16);
							}
							pTris[nTris++] = m_pTopology[itri].ibuddy[j];
							pTriSlots[nTriSlots++] = m_pTopology[itri].ibuddy[j];
							set_mask(pTriMask[0], m_pTopology[itri].ibuddy[j]);
						}
					}
					if (nTris > nTrisAlloc - 3)
					{
						ReallocateList(pTris, nTris, nTrisAlloc += 16);
					}
				}
			}
		}
		pTriSlots[nTriSlots] = m_nTris;

		if (nTriSlots * 2 > m_nTris || SAFER_BOOLEANS && !m_nErrors)
		{
			bCheckVolumes = 1;
			V[0] = GetVolume();
			V[1] = pMesh[1]->GetVolume() * cube(gwd[1].scale);
			memcpy(pBackupVertices = new Vec3[m_nVertices], m_pVertices,
			       m_nVertices * sizeof(m_pVertices[0]));
			memcpy(pBackupIndices = new index_t[m_nTris * 3], m_pIndices,
			       m_nTris * 3 * sizeof(m_pIndices[0]));
			memcpy(pBackupNormals = new Vec3[m_nTris], m_pNormals, m_nTris * sizeof(m_pNormals[0]));
			if (m_pIds)
			{
				memcpy(pBackupMats = new char[m_nTris], m_pIds, m_nTris * sizeof(m_pIds[0]));
			}
			if (m_pForeignIdx)
			{
				memcpy(pBackupIds = new int[m_nTris], m_pForeignIdx,
				       m_nTris * sizeof(m_pForeignIdx[0]));
			}
			if (m_pVtxMap)
			{
				memcpy(pBackupVtxMap = new int[m_nVertices], m_pVtxMap,
				       m_nVertices * sizeof(m_pVtxMap[0]));
			}
		}

		for (i = 0; i < m_nTris; i++)
		{
			if (!check_mask(pTriMask[0], i))
			{
				for (j = 0; j < 3; j++)
				{
					set_mask(pVtxMask[0],
					         m_pIndices[(i * 3) + j]); // mark A vertices used by remaining tris
				}
			}
		}
		for (i = 0; i < m_nVertices; i++)
		{ //	build a list of free vertex slots
			pVtxSlots[nVtxSlots] = i;
			nVtxSlots += (check_mask(pVtxMask[0], pVtxMap[i & bVtxMap] | (i & ~bVtxMap)) |
			              check_mask(pVtxMask[0], i)) ^
			             1;
			if (nVtxSlots >= nVtxSlotsAlloc - 1)
			{
				ReallocateList(pVtxSlots, nVtxSlots, nVtxSlotsAlloc += 16);
			}
		}
		pVtxSlots[nVtxSlots] = m_nVertices;

		if (pRes)
		{
			pRes->pMesh[0] = pMesh[0];
			pRes->pMesh[1] = pMesh[1];
			pRes->pRemovedVtx = pVtxSlots;
			pRes->nRemovedVtx = nVtxSlots;
			pRes->pRemovedTri = new int[nTriSlots + 1];
			for (i = 0; i < nTriSlots; i++)
			{
				pRes->pRemovedTri[i] = pIds[0][pTriSlots[i] & idmask[0]];
			}
			pRes->nRemovedTri = nTriSlots;
		}

		// if a B polygon has a full edge, flood fill B triangles starting from it and make a list of B's full
		// triangles used
		for (i = 0; i < nPolies; i++)
		{
			if (pPolies[i].itri & 1 << 30)
			{
				itri = pPolies[i].itri & ((1 << 30) - 1);
				ivtx = ivtx0 = pPolies[i].ivtx;
				imask = 0;
				nTris = ipt = 0;
				do
				{
					if (pVtx[ivtx].icont == -2)
					{
						set_mask(pVtxMask[1],
						         pVtx[ivtx].ivtx); // mark B vertices used by polygons
						idx = ivtx - pPolies[i].ivtx0;
						idx = 3 - idx - inc_mod3[idx];
						if (pVtx[pVtx[ivtx].inext].icont == -2 &&
						    (j = pMesh[1]
						             ->m_pTopology[itri]
						             .ibuddy[dec_mod3[(pVtx[ivtx].flags & 3) - 1]]) >= 0 &&
						    !check_mask(pTriMask[1], j) &&
						    (!(pVtx[ivtx0].flags & pVtx[pVtx[pVtx[ivtx].inext].inext].flags &
						       vtx_instable) ||
						     (pVtx[ivtx0].flags >> vtx_instablept_log2 & 3) == idx ||
						     (pVtx[pVtx[pVtx[ivtx].inext].inext].flags >> vtx_instablept_log2 &
						      3) == idx)) // don't floodfill from instable edges
						{
							if (nBTris == nBTrisAlloc)
							{
								ReallocateList(pBTris, nBTris, nBTrisAlloc += 16);
							}
							pTris[nTris++] = j;
							pBTris[nBTris++] = j;
							set_mask(pTriMask[1], j);
						}
					}
					ivtx0 = ivtx;
				}
				while ((ivtx = pVtx[ivtx].inext) != pPolies[i].ivtx && ++ipt < pPolies[i].nvtx * 2);
				if (ipt == pPolies[i].nvtx * 2)
				{
					goto BadContours;
				}
				while (nTris)
				{
					itri = pTris[--nTris];
					for (j = 0; j < 3; j++)
					{
						if (pMesh[1]->m_pTopology[itri].ibuddy[j] >= 0 &&
						    !check_mask(pTriMask[1], pMesh[1]->m_pTopology[itri].ibuddy[j]))
						{
							itri1 = pTris[nTris++] = pMesh[1]->m_pTopology[itri].ibuddy[j];
							if (nBTris == nBTrisAlloc)
							{
								ReallocateList(pBTris, nBTris, nBTrisAlloc += 16);
							}
							pBTris[nBTris++] = itri1;
							set_mask(pTriMask[1], itri1);
						}
					}
					if (nTris > nTrisAlloc - 3)
					{
						ReallocateList(pTris, nTris, nTrisAlloc += 16);
					}
				}
			}
		}

		// add B triangles that belong to B islands that are fully inside A
		ip.bKeepPrevContacts = true;
		GetBBox(&bbox, 1);
		for (i = 0; i < nIsles; i++)
		{
			if (!pMesh[1]->m_pIslands[i].bProcessed)
			{
				const Vec3 dirn(0, 0, 1);
				rayGeom.CreateRay(
				    gwd[1].R *
					    pMesh[1]
						->m_pVertices[pMesh[1]->m_pIndices[pMesh[1]->m_pIslands[i].itri * 3]] *
					    gwd[1].scale +
					gwd[1].offset,
				    Vec3(0, 0, bbox.size.z * 2.5f), &dirn);
				if ((ipt = Intersect(&rayGeom, gwd, 0, &ip, pcont1)) && pcont1[ipt - 1].n.z > 0)
				{
					for (itri = pMesh[1]->m_pIslands[i].itri; itri != 0x7FFF;
					     itri = pMesh[1]->m_pTri2Island[itri].inext)
					{
						if (nBTris == nBTrisAlloc)
						{
							ReallocateList(pBTris, nBTris, nBTrisAlloc += 16);
						}
						pBTris[nBTris++] = itri;
						set_mask(pTriMask[1], itri);
					}
				}
			}
			else
			{
				pMesh[1]->m_pIslands[i].bProcessed = 0;
			}
		}

		// mark B's vertices that B's full triangles use
		for (i = 0; i < nBTris; i++)
		{
			for (j = 0; j < 3; j++)
			{
				set_mask(pVtxMask[1], pMesh[1]->m_pIndices[(pBTris[i] * 3) + j]);
			}
		}
		// add used B vertices to A (using free slots), build a mapping table B index -> new A index
		for (i = 0; i < pMesh[1]->m_nVertices; i++)
		{
			if (check_mask(pVtxMask[1], i))
			{
				if ((pBVtxMap[i] = ivtx = pVtxSlots[min(nVtxSlots, nNewVtx++)]) >= nVtxAlloc)
				{
					ReallocateList(m_pVertices.data, m_nVertices,
					               nVtxAlloc = (m_nVertices + 32) & ~31);
					if (m_pVtxMap)
					{
						pVtxMap = ReallocateList(m_pVtxMap, m_nVertices, nVtxAlloc);
					}
				}
				pVtxSlots[nVtxSlots] = (m_nVertices += isneg(m_nVertices - 1 - ivtx));
				m_pVertices[ivtx] = gwd[1].R * pMesh[1]->m_pVertices[i] * gwd[1].scale + gwd[1].offset;
				pVtxMap[ivtx & bVtxMap] = ivtx;
			}
		}
		if (pRes)
		{ // register the new vertices
			pRes->pNewVtx = new bop_newvtx[nNewVtxAlloc = max(1, pRes->nNewVtx = nNewVtx)];
			for (i = j = 0; i < pMesh[1]->m_nVertices; i++)
			{
				if (check_mask(pVtxMask[1], i))
				{
					pBVtxMapNew[i] = j;
					pRes->pNewVtx[j].idx = pBVtxMap[i];
					pRes->pNewVtx[j].idxTri[0] = pRes->pNewVtx[j].idxTri[1] = 0;
					pRes->pNewVtx[j++].iBvtx = i;
				}
			}
		}

		// add all intersection points to A as new vertices (unless they are close enough to the corresponding
		// tri vertices)
		for (icont = 0; icont < ncont; icont++)
		{
			for (ipt = 0; ipt < pcont[icont].nborderpt - 1; ipt++)
			{
				// check if each point is close enough to either A vertex or B vertex
				// stores: pcont[icont].idxborder[ipt][0] <- corresponding new vertex index in A
				//				 pcont[icont].idxborder[ipt][1]	<- (vertex index for
				// bop_meshupdate.pNewTri.iVtx)*2 + (vertex modified flag)
				itri = pcont[icont].idxborder[ipt][0] & IDXMASK;
				for (j = 0;
				     j < 3 &&
				     (pcont[icont].ptborder[ipt] - m_pVertices[m_pIndices[(itri * 3) + j]]).len2() > e;
				     j++)
					;
				if (j == 3)
				{
					itri = pcont[icont].idxborder[ipt][1] & IDXMASK;
					for (j = 0;
					     j < 3 && (pcont[icont].ptborder[ipt] -
					               m_pVertices[pBVtxMap[pMesh[1]->m_pIndices[(itri * 3) + j]]])
					                      .len2() > e;
					     j++)
						;
					if (j == 3)
					{
						if ((ivtx = pVtxSlots[min(nVtxSlots, nNewVtx)]) >= nVtxAlloc)
						{
							ReallocateList(m_pVertices.data, m_nVertices,
							               nVtxAlloc = (m_nVertices + 32) & ~31);
							if (m_pVtxMap)
							{
								pVtxMap =
								    ReallocateList(m_pVtxMap, m_nVertices, nVtxAlloc);
							}
						}
						pVtxSlots[nVtxSlots] = (m_nVertices += isneg(m_nVertices - 1 - ivtx));
						if (pRes)
						{
							if (nNewVtx == nNewVtxAlloc)
							{
								ReallocateList(pRes->pNewVtx, nNewVtx,
								               nNewVtxAlloc = (nNewVtx + 32) & ~31);
							}
							pRes->pNewVtx[nNewVtx].idx = ivtx;
							pRes->pNewVtx[nNewVtx].iBvtx = -1;
							for (iop = 0; iop < 2; iop++)
							{
								pRes->pNewVtx[nNewVtx].idxTri[iop] =
								    pIds[iop][(itri = pcont[icont].idxborder[ipt][iop] &
								                      IDXMASK) &
								              idmask[iop]];
							}
						}
						pcont[icont].idxborder[ipt][1] = -(nNewVtx++ + 1) * 2;
						m_pVertices[ivtx] = pcont[icont].ptborder[ipt];
						pVtxMap[ivtx & bVtxMap] = ivtx;
					}
					else
					{
						ivtx = pBVtxMap[pMesh[1]->m_pIndices[(itri * 3) + j]];
						pcont[icont].idxborder[ipt][1] =
						    (-(pBVtxMapNew[pMesh[1]->m_pIndices[(itri * 3) + j]] + 1) * 2) + 1;
					}
				}
				else
				{
					pcont[icont].idxborder[ipt][1] = ((ivtx = m_pIndices[(itri * 3) + j]) * 2) + 1;
				}
				pcont[icont].idxborder[ipt][0] = ivtx;
			}
		}

		// add full B triangles to A
		for (i = 0, nTris = m_nTris; i < nBTris; i++)
		{
			if ((itri = pTriSlots[min(nTriSlots, nNewTris++)]) >= nTriAlloc)
			{
				ReallocateList(m_pIndices, m_nTris * 3, (nTriAlloc = (m_nTris + 32) & ~31) * 3);
				ReallocateList(m_pNormals, m_nTris, nTriAlloc);
				if (m_pForeignIdx)
				{
					pIds[0] = ReallocateList(m_pForeignIdx, m_nTris, nTriAlloc);
				}
				if (m_pIds)
				{
					pMats[0] = ReallocateList(m_pIds, m_nTris, nTriAlloc);
				}
			}
			clear_mask(pTriMask[0], min(nTris0, itri));
			pTriSlots[nTriSlots] = (m_nTris += isneg(m_nTris - 1 - itri));
			for (j = 0; j < 3; j++)
			{
				m_pIndices[(itri * 3) + j] = pBVtxMap[pMesh[1]->m_pIndices[(pBTris[i] * 3) + 2 - j]];
			}
			m_pNormals[itri] = gwd[1].R * -pMesh[1]->m_pNormals[pBTris[i]];
			pIds[0][itri & idmask[0]] = m_iLastNewTriIdx + i;
			pMats[0][itri & matmask[0]] = pMats[1][pBTris[i] & matmask[1]];
		}
		if (pRes)
		{
			pRes->pNewTri = new bop_newtri[nNewTrisAlloc = max(1, pRes->nNewTri = nNewTris)];
			for (i = 0; i < nBTris; i++)
			{
				pRes->pNewTri[i].idxNew = m_iLastNewTriIdx + i;
				pRes->pNewTri[i].iop = 1;
				pRes->pNewTri[i].idxOrg = pIds[1][pBTris[i] & idmask[1]];
				pRes->pNewTri[i].areaOrg =
				    pMesh[1]->m_pNormals[pBTris[i]] *
				    (pMesh[1]->m_pVertices[pMesh[1]->m_pIndices[(pBTris[i] * 3) + 1]] -
				         pMesh[1]->m_pVertices[pMesh[1]->m_pIndices[pBTris[i] * 3]] ^
				     pMesh[1]->m_pVertices[pMesh[1]->m_pIndices[(pBTris[i] * 3) + 2]] -
				         pMesh[1]->m_pVertices[pMesh[1]->m_pIndices[pBTris[i] * 3]]);
				for (j = 0; j < 3; j++)
				{
					pRes->pNewTri[i].iVtx[j] =
					    -(pBVtxMapNew[pMesh[1]->m_pIndices[(pBTris[i] * 3) + 2 - j]] + 1);
					pRes->pNewTri[i].area[j].zero()[j] = pRes->pNewTri[i].areaOrg;
				}
			}
		}

		// triangulate the polygons, add results to A
		int npt2d, bHasConts, dstflags, *ipt2d, *ipt2dnew, *pResTris, nResTrisAlloc;
		Vec3 axisx, axisy;
		vector2df* pt2d;
		pt2d = new vector2df[nMaxPolyVtx += 4];
		ipt2d = new int[nMaxPolyVtx];
		ipt2dnew = new int[nMaxPolyVtx];
		pResTris = new int[nResTrisAlloc = 64];

		for (i = 0; i < nPolies; i++)
		{
			iop = pPolies[i].itri >> 30;
			itri = pPolies[i].itri & ((1 << 30) - 1);
			n = pPolies[i].n * ((iop * 2) - 1);
			for (j = 0, axisx = n.GetOrthogonal(); j < nTries; j++)
			{
				axisx = axisx.GetRotated(n, 0.6f, 0.8f);
			}
			axisy = n ^ axisx;
			MARK_UNUSED pt2d[0].x;
			ipt2d[0] = -1;
			npt2d = 1;

			for (dstflags = 0, ncont = 1;; dstflags += contour_end)
			{
				flags = 0;
				bHasConts = 0;
				ivtx = pPolies[i].ivtx;
				while ((pVtx[ivtx].flags & 3) == 0 && (ivtx = pVtx[ivtx].inext) != pPolies[i].ivtx)
					;
				ivtx0 = ivtx;
				idx = -1;
				ipt = -1;
				do
				{
					flags ^= pVtx[ivtx].flags;
					if (((flags | pVtx[ivtx].flags) & contour_end) == dstflags)
					{
						if (pVtx[ivtx].icont >= 0)
						{
							ipt2d[npt2d] =
							    pcont[pVtx[ivtx].icont].idxborder[pVtx[ivtx].ivtx][0];
							ipt2dnew[npt2d] =
							    pcont[pVtx[ivtx].icont].idxborder[pVtx[ivtx].ivtx][1];
						}
						else if (pVtx[ivtx].icont == -2)
						{
							ipt2d[npt2d] = pBVtxMap[pVtx[ivtx].ivtx];
							ipt2dnew[npt2d] = -(pBVtxMapNew[pVtx[ivtx].ivtx] + 1) * 2;
						}
						else
						{
							ipt2dnew[npt2d] = (ipt2d[npt2d] = pVtx[ivtx].ivtx) * 2;
						}
						pt2d[npt2d].set(axisx * m_pVertices[ipt2d[npt2d]],
						                axisy * m_pVertices[ipt2d[npt2d]]);
						j = (iszero(ipt2d[npt2d - 1] - ipt2d[npt2d]) |
						     iszero(ipt2d[npt2d] - idx)) ^
						    1;
						idx += (ipt2d[npt2d] + 1) &
						       idx >> 31; // stores the first vertex idx for a contour
						npt2d += j;
						ipt = pVtx[ivtx].icont;
					}
					else
					{
						bHasConts = 1;
					}
					if (dstflags & (flags ^ contour_end) & pVtx[ivtx].flags & contour_end)
					{
						MARK_UNUSED pt2d[npt2d].x;
						ipt2d[npt2d] = -1;
						npt2d++;
						ncont++;
						idx = -1;
					}
				}
				while ((ivtx = pVtx[ivtx].inext) != ivtx0);
				if (dstflags == 0)
				{
					MARK_UNUSED pt2d[npt2d].x;
					ipt2d[npt2d++] = -1;
				}
				if (!bHasConts || dstflags == contour_end || npt2d >= nMaxPolyVtx)
				{
					break;
				}
			}

			if ((nTris = (npt2d + ncont * 2) * 3) > nResTrisAlloc)
			{
				ReallocateList(pResTris, 0, nResTrisAlloc = (nTris + 48) & ~15);
			}
			nTris = TriangulatePoly(pt2d + 1, npt2d - 1, pResTris, nResTrisAlloc);
			for (icont = 0; icont < nTris; icont++)
			{
				if (!(iszero(ipt2d[pResTris[(icont * 3) + 0] + 1] -
				             ipt2d[pResTris[(icont * 3) + 1] + 1]) | // skip degenerate tris
				      iszero(ipt2d[pResTris[(icont * 3) + 0] + 1] -
				             ipt2d[pResTris[(icont * 3) + 2] + 1]) |
				      iszero(ipt2d[pResTris[(icont * 3) + 1] + 1] -
				             ipt2d[pResTris[(icont * 3) + 2] + 1])))
				{
					if ((itri = pTriSlots[min(nTriSlots, nNewTris)]) >= nTriAlloc)
					{
						ReallocateList(m_pIndices, m_nTris * 3,
						               (nTriAlloc = (m_nTris + 33) & ~31) * 3);
						ReallocateList(m_pNormals, m_nTris, nTriAlloc);
						if (m_pForeignIdx)
						{
							pIds[0] = ReallocateList(m_pForeignIdx, m_nTris, nTriAlloc);
						}
						if (m_pIds)
						{
							pMats[0] = ReallocateList(m_pIds, m_nTris, nTriAlloc);
						}
					}
					clear_mask(pTriMask[0], min(nTris0, itri));
					for (j = 0; j < 3; j++)
					{
						idx = (iop - 1 & 3) + (j ^ ~-iop); // 2-j for A, j for B
						m_pIndices[(itri * 3) + j] = ipt2d[pResTris[(icont * 3) + idx] + 1];
					}
					n = (m_pVertices[m_pIndices[(itri * 3) + 1]] -
					         m_pVertices[m_pIndices[itri * 3]] ^
					     m_pVertices[m_pIndices[(itri * 3) + 2]] -
					         m_pVertices[m_pIndices[itri * 3]]);
					if (!((ipt2dnew[pResTris[icont * 3] + 1] |
					       ipt2dnew[pResTris[(icont * 3) + 1] + 1] |
					       ipt2dnew[pResTris[(icont * 3) + 2] + 1]) &
					      1))
					{
						m_pNormals[itri] = pPolies[i].n;
						if (sqr_signed(m_pNormals[itri] * n) < n.len2() * 0.999f)
						{
							m_pNormals[itri] = n.normalized();
						}
					}
					else
					{
						m_pNormals[itri] = n.normalized();
					}
					pIds[0][itri & idmask[0]] = m_iLastNewTriIdx + nNewTris;
					pMats[0][itri & matmask[0]] = pPolies[i].mat;
					pTriSlots[nTriSlots] = (m_nTris += isneg(m_nTris - 1 - itri));
					if (pRes)
					{
						if (nNewTris >= nNewTrisAlloc)
						{
							ReallocateList(pRes->pNewTri, nNewTris,
							               nNewTrisAlloc = (nNewTris + 32) & ~31);
						}
						pRes->pNewTri[nNewTris].idxNew = m_iLastNewTriIdx + nNewTris;
						pRes->pNewTri[nNewTris].iop = iop;
						pRes->pNewTri[nNewTris].idxOrg = pPolies[i].id;
						pRes->pNewTri[nNewTris].areaOrg = pPolies[i].areaOrg;
						for (j = 0; j < 3; j++)
						{
							idx = (iop - 1 & 3) + (j ^ ~-iop);
							pRes->pNewTri[nNewTris].iVtx[j] =
							    ipt2dnew[pResTris[(icont * 3) + idx] + 1] >> 1;
							for (ipt = 0; ipt < 3; ipt++)
							{
								pRes->pNewTri[nNewTris].area[j][ipt] =
								    (1 - iop * 2) *
								    (pPolies[i].n *
								     (pVtx[pPolies[i].ivtx0 + 2 - inc_mod3[ipt]].pt -
								          m_pVertices[m_pIndices[(itri * 3) + j]] ^
								      pVtx[pPolies[i].ivtx0 + 2 - dec_mod3[ipt]].pt -
								          m_pVertices[m_pIndices[(itri * 3) + j]]));
							}
						}
					}
					nNewTris++;
				}
			}
		}

		if (nNewTris < nTriSlots)
		{
			// compact triangle list if more triangles were deleted than added
			for (i = 0, itri = m_nTris - 1; true; i++, itri--)
			{
				for (; i < itri && !check_mask(pTriMask[0], i); i++)
					;
				for (; i < itri && check_mask(pTriMask[0], itri); itri--)
					;
				if (i >= itri)
				{
					break;
				}
				for (j = 0; j < 3; j++)
				{
					m_pIndices[(i * 3) + j] = m_pIndices[(itri * 3) + j];
				}
				if (m_pIds)
				{
					m_pIds[i] = m_pIds[itri];
				}
				if (m_pForeignIdx)
				{
					m_pForeignIdx[i] = m_pForeignIdx[itri];
				}
				m_pNormals[i] = m_pNormals[itri];
			}
			nTris = m_nTris - nTriSlots + nNewTris;
			ReallocateList(m_pIndices, nTris * 3, nTris * 3);
			ReallocateList(m_pNormals, nTris, nTris);
			if (m_pIds)
			{
				ReallocateList(m_pIds, nTris, nTris);
			}
			if (m_pForeignIdx)
			{
				ReallocateList(m_pForeignIdx, nTris, nTris);
			}
			m_nTris = nTris;
		}
		// set pVtxMap of all empty vtx slots that are left unused to themselves (to avoid dangling unused
		// vertices)
		for (i = nNewVtx; i < nVtxSlots; i++)
		{
			pVtxMap[pVtxSlots[i] & bVtxMap] = pVtxSlots[i];
		}
		if (pRes)
		{
			pRes->nNewTri = nNewTris;
			pRes->nNewVtx = nNewVtx;
		}

		delete[] pResTris;
		delete[] ipt2dnew;
		delete[] ipt2d;
		delete[] pt2d;
		delete[] pTris;
		delete[] pBTris;
		if (!pRes)
		{
			delete[] pVtxSlots;
		}
		delete[] pTriSlots;
		delete[] pBVtxMapNew;
		delete[] pBVtxMap;
		delete[] pTriMask[1];
		delete[] pVtxMask[1];
		delete[] pTriMask[0];
		delete[] pVtxMask[0];
		delete[] pVtx;
		delete[] pPolies;

		if (pdata0 != m_pVertices.data)
		{
			m_flags &= ~mesh_shared_vtx;
		}
		if (pIndices0 != m_pIndices)
		{
			m_flags &= ~mesh_shared_idx;
		}
		if (pMats0 != m_pIds)
		{
			m_flags &= ~mesh_shared_mats;
		}
		if (pIds0 != m_pForeignIdx)
		{
			m_flags &= ~mesh_shared_foreign_idx;
		}
		if (pTopology0 != m_pTopology)
		{
			m_flags &= ~mesh_shared_topology;
		}
		MARK_UNUSED m_V, m_center;

		if (bCheckVolumes)
		{
			V[2] = GetVolume();
			if (!inrange(V[2], V[0] + (V[1] * 0.1f), max(0.0f, V[0] - (V[1] * 1.1f))) ||
			    !m_nErrors && nTries < 4 && SAFER_BOOLEANS && CalculateTopology(m_pIndices, 1) ||
			    g_nTriangulationErrors > 0)
			{
				MARK_UNUSED m_V, m_center;
				m_nErrors = nErrors0;
				if (!(m_flags & mesh_shared_vtx))
				{
					delete[] m_pVertices.data;
				}
				if (!(m_flags & mesh_shared_idx))
				{
					delete[] m_pIndices;
				}
				if (m_pIds && !(m_flags & mesh_shared_mats))
				{
					delete[] m_pIds;
				}
				if (m_pForeignIdx && !(m_flags & mesh_shared_foreign_idx))
				{
					delete[] m_pForeignIdx;
				}
				if (m_pVtxMap)
				{
					delete[] m_pVtxMap;
				}
				delete[] m_pNormals;
				nVtxAlloc = m_nVertices = nVtx0;
				nTriAlloc = m_nTris = nTris0;
				m_pVertices.data = pBackupVertices;
				m_pIndices = pBackupIndices;
				m_pNormals = pBackupNormals;
				if (m_pIds)
				{
					pMats[0] = m_pIds = pBackupMats;
				}
				if (m_pForeignIdx)
				{
					pIds[0] = m_pForeignIdx = pBackupIds;
				}
				if (m_pVtxMap)
				{
					pVtxMap = m_pVtxMap = pBackupVtxMap;
				}
				bCheckVolumes = 0;
				if (pRes)
				{
					pRes->pMesh[0] = pRes->pMesh[1] = 0;
					delete pRes;
					pRes = 0;
				}
				nTries++;
				goto RetryWithJitter;
			}
			delete[] pBackupVertices;
			delete[] pBackupIndices;
			delete[] pBackupNormals;
			if (m_pIds)
			{
				delete[] pBackupMats;
			}
			if (m_pForeignIdx)
			{
				delete[] pBackupIds;
			}
			if (m_pVtxMap)
			{
				delete[] pBackupVtxMap;
			}
		}

		pRes->relScale = gwd[1].scale;
		pRes->pMesh[0]->AddRef();
		pRes->pMesh[1]->AddRef();
		if (!m_pMeshUpdate)
		{
			m_pMeshUpdate = pRes;
		}
		else
		{
			bop_meshupdate* pbop;
			for (pbop = m_pMeshUpdate; pbop->next; pbop = pbop->next)
				;
			pbop->next = pRes;
		}
		m_iLastNewTriIdx += nNewTris;
		m_nSubtracts++;

		// reallocate (if necessary) and recalculate connectivity information
		if (nNewTris > nTriSlots)
		{
			ReallocateList(m_pTopology, 0, m_nTris);
		}
		CalculateTopology(m_pIndices);
		FilterMesh(m_minVtxDist * 2, 0.01f);

		if (m_pIslands)
		{
			delete[] m_pIslands;
			m_pIslands = 0;
		}
		if (m_pTri2Island)
		{
			delete[] m_pTri2Island;
			m_pTri2Island = 0;
		}
		for (i = 1; i < sizeof(m_bConvex) / sizeof(m_bConvex[0]); i++)
		{
			m_ConvexityTolerance[i] = -1;
		}
		m_ConvexityTolerance[0] = 0.02f;
		m_bConvex[0] = 0;

		i = m_nTris;
		m_nTris = 0;
	} // lock intersect

	{
		WriteLock lock0(m_lockUpdate);
		m_nTris = i;
		RebuildBVTree();
	}

	return nTries;
}

void CTriMesh::GetBBox(primitives::box* pbox)
{
	ReadLock lock(m_lockUpdate);
	m_pTree->GetBBox(pbox);
}

void CTriMesh::GetBBox(primitives::box* pbox, int bThreadSafe)
{
	ReadLockCond lock(m_lockUpdate, bThreadSafe ^ 1);
	m_pTree->GetBBox(pbox);
}

void CTriMesh::CalcVolumetricPressure(geom_world_data* gwd, const Vec3& epicenter, float k, float rmin,
                                      const Vec3& centerOfMass, Vec3& P, Vec3& L)
{
	int i, j, npt, iter;
	Vec3 pt[64], ptc, dP, Pres, Lres, c, cm;
	float r2, t1, t2, rmin2, rscale;
	c = (epicenter - gwd->offset) * gwd->R;
	cm = (centerOfMass - gwd->offset) * gwd->R;
	rscale = 1.0f / gwd->scale;
	rmin2 = sqr(rmin * rscale);
	k *= rscale * rscale;

	for (i = 0; i < m_nTris; i++)
	{
		if (m_pNormals[i] * (m_pVertices[m_pIndices[i * 3]] - c) > 0)
		{
			pt[0] = m_pVertices[m_pIndices[i * 3]];
			pt[1] = m_pVertices[m_pIndices[(i * 3) + 1]];
			pt[2] = m_pVertices[m_pIndices[(i * 3) + 2]];
			iter = 0;
			npt = 3;
			do
			{
				npt -= 3;
				for (j = 0; j < 3 && (pt[npt + inc_mod3[j]] - pt[npt + j] ^ pt[npt + inc_mod3[j]] - c) *
				                             m_pNormals[i] >
				                         1E-4;
				     j++)
					;
				if (j == 3 && npt <= (sizeof(pt) / sizeof(Vec3)) - 9)
				{ // closest to the epicenter point lies inside triangle, split on this point
					ptc = c - m_pNormals[i] * (m_pNormals[i] * (c - pt[npt]));
					pt[npt + 3] = pt[npt];
					pt[npt + 5] = pt[npt + 2];
					pt[npt + 6] = pt[npt];
					pt[npt + 7] = pt[npt + 1];
					for (j = 0; j < 3; j++)
					{
						pt[npt + (j * 4)] = ptc;
					}
					npt += 9;
				}
				else
				{
					for (j = 0; j < 3; j++)
					{
						t1 = (pt[npt + j] - c).len2();
						t2 = (pt[npt + inc_mod3[j]] - c).len2();
						if ((t1 > rmin || t2 > rmin) &&
						    (t1 < (0.7f * 0.7f) * t2 || t1 > (1.0f / 0.7f / 0.7f) * t2))
						{
							break;
						}
					}
					if (j < 3 && npt <= (sizeof(pt) / sizeof(Vec3)) - 12)
					{
						for (j = 0; j < 3; j++)
						{ // split triangle on into 4 if difference between distances from any 2
						  // vertices to epicenter is too big
							pt[npt + (j * 3) + 3] = pt[npt + j];
							pt[npt + (j * 3) + 4] =
							    (pt[npt + j] + pt[npt + inc_mod3[j]]) * 0.5f;
							pt[npt + (j * 3) + 5] =
							    (pt[npt + j] + pt[npt + dec_mod3[j]]) * 0.5f;
						}
						for (j = 0; j < 3; j++)
						{
							pt[npt + j] =
							    (pt[npt + (j * 3) + 3] + pt[npt + (inc_mod3[j] * 3) + 3]) *
							    0.5f;
						}
						npt += 12;
					}
					else
					{
						ptc = (pt[npt] + pt[npt + 1] + pt[npt + 2]) * (1.0f / 3);
						r2 = (ptc - c).len2();
						dP = m_pNormals[i] *
						     (((ptc - c) * m_pNormals[i]) *
						      (pt[npt + 1] - pt[npt] ^ pt[npt + 2] - pt[npt]).len() * 0.5f * k /
						      (sqrt_tpl(r2) * max(r2, rmin2)));
						Pres += dP;
						Lres += ptc - cm ^ dP;
					}
				}
			}
			while (npt > 0 && ++iter < 300);
		}
	}
	P += gwd->R * Pres;
	L += gwd->R * Lres;
}

float CTriMesh::CalculateBuoyancy(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& massCenter)
{
	geometry_under_test gtest;
	primitives::triangle tri, tri0;

	// GetRotationV0V1(Vec3(0,0,1),pplane->n).getmatrix(gtest.R.T());	//Q2M_IVO
	gtest.R = Matrix33(Quat::CreateRotationV0V1(pplane->n, Vec3(0, 0, 1)));
	gtest.offset = gtest.R * (pgwd->offset - pplane->origin);
	gtest.R *= pgwd->R;
	gtest.scale = pgwd->scale;
	int itri, i, j, imask, nAbove, iLow, iHigh, sign[3], nPieces;
	float t, V[4], Vaccum = 0;
	Vec3 com[4], com_accum, pt[3];

	for (itri = 0; itri < m_nTris; itri++)
	{
		PrepareTriangle(itri, &tri, &gtest);
		for (i = iLow = iHigh = 0, nAbove = 3; i < 3; i++)
		{
			nAbove -= (sign[i] = isneg(tri.pt[i].z));
			imask = -isneg(tri.pt[i].z - tri.pt[iLow].z);
			iLow = (i & imask) | (iLow & ~imask);
			imask = -isneg(tri.pt[iHigh].z - tri.pt[i].z);
			iHigh = (i & imask) | (iHigh & ~imask);
		}
		for (i = j = 0; i < 3; i++)
		{
			if (sign[i] ^ sign[inc_mod3[i]])
			{
				t = -tri.pt[i].z / (tri.pt[inc_mod3[i]].z - tri.pt[i].z);
				pt[j++] = tri.pt[i] * (1.0f - t) + tri.pt[inc_mod3[i]] * t;
			}
		}

		if (nAbove < 2)
		{
			(tri0.pt[0] = tri.pt[0]).z = tri.pt[iHigh].z;
			(tri0.pt[1] = tri.pt[1]).z = tri.pt[iHigh].z;
			(tri0.pt[2] = tri.pt[2]).z = tri.pt[iHigh].z;
			V[0] = CalcPyramidVolume(tri.pt[iHigh], tri0.pt[inc_mod3[iHigh]], tri.pt[inc_mod3[iHigh]],
			                         tri0.pt[dec_mod3[iHigh]], com[0]);
			V[1] = CalcPyramidVolume(tri.pt[iHigh], tri0.pt[dec_mod3[iHigh]], tri.pt[dec_mod3[iHigh]],
			                         tri.pt[inc_mod3[iHigh]], com[1]);
			V[2] = fabsf((tri0.pt[1] - tri0.pt[0] ^ tri0.pt[2] - tri0.pt[0]).z * 0.5f * tri0.pt[0].z);
			(com[2] = (tri0.pt[0] + tri0.pt[1] + tri0.pt[2]) * (1.0f / 3)).z = tri0.pt[0].z * 0.5f;
			if (nAbove == 1)
			{
				V[2] *= -1.0f;
				tri0.pt[iHigh].z = 0;
				nPieces = 4;
				V[3] = CalcPyramidVolume(tri.pt[iHigh], tri0.pt[iHigh], pt[0], pt[1], com[3]);
			}
			else
			{
				nPieces = 3;
			}
		}
		else if (nAbove == 2)
		{
			(tri0.pt[0] = tri.pt[iLow]).z = 0;
			nPieces = 1;
			V[0] = CalcPyramidVolume(tri.pt[iLow], tri0.pt[0], pt[0], pt[1], com[0]);
		}
		else
		{
			nPieces = 0;
		}

		sign[0] = -sgn(tri.n.z);
		for (i = 0; i < nPieces; i++)
		{
			V[i] *= sign[0];
			Vaccum += V[i];
			com_accum += com[i] * V[i];
		}
	}

	if (Vaccum > 0)
	{
		massCenter = com_accum / Vaccum + pplane->origin;
	}
	else
	{
		massCenter.zero();
	}

	return Vaccum;
}

void CTriMesh::CalculateMediumResistance(const primitives::plane* pplane, const geom_world_data* pgwd, Vec3& dPres,
                                         Vec3& dLres)
{
	geometry_under_test gtest;
	primitives::triangle tri;
	gtest.R = pgwd->R;
	gtest.offset = pgwd->offset;
	gtest.scale = pgwd->scale;
	int itri;

	dPres.zero();
	dLres.zero();
	for (itri = 0; itri < m_nTris; itri++)
	{
		PrepareTriangle(itri, &tri, &gtest);
		CalcMediumResistance(tri.pt, 3, tri.n, *pplane, pgwd->v, pgwd->w, pgwd->centerOfMass, dPres, dLres);
	}
}

int CTriMesh::DrawToOcclusionCubemap(const geom_world_data* pgwd, int iStartPrim, int nPrims, int iPass, int* pGrid[6],
                                     int nRes, float rmin, float rmax, float zscale)
{
	Vec3 pt[3];
	for (int i = iStartPrim; i < iStartPrim + nPrims; i++)
	{
		pt[0] = pgwd->R * m_pVertices[m_pIndices[i * 3]] * pgwd->scale + pgwd->offset;
		pt[1] = pgwd->R * m_pVertices[m_pIndices[(i * 3) + 1]] * pgwd->scale + pgwd->offset;
		pt[2] = pgwd->R * m_pVertices[m_pIndices[(i * 3) + 2]] * pgwd->scale + pgwd->offset;
		RasterizePolygonIntoCubemap(pt, 3, iPass, pGrid, nRes, rmin, rmax, zscale);
	}
	return nPrims;
}

void* CTriMesh::GetForeignData(int iForeignData)
{
	return iForeignData == DATA_MESHUPDATE ? m_pMeshUpdate : CGeometry::GetForeignData(iForeignData);
}

void CTriMesh::SetForeignData(void* pForeignData, int iForeignData)
{
	if (iForeignData == DATA_MESHUPDATE)
	{
		m_pMeshUpdate = (bop_meshupdate*)pForeignData;
		if (!pForeignData)
		{
			m_iLastNewTriIdx = BOP_NEWIDX0;
		}
	}
	else
	{
		CGeometry::SetForeignData(pForeignData, iForeignData);
	}
}

void CTriMesh::RemapForeignIdx(int* pCurForeignIdx, int* pNewForeignIdx, int nTris)
{
	if (m_pForeignIdx)
	{
		int i, iop;
		SplitArray<int> pIdxMap;
		bop_meshupdate* pmu;

		for (i = 0; i < nTris; i++)
		{
			pIdxMap.sz[pCurForeignIdx[i] >> 27] =
			    max(pIdxMap.sz[pCurForeignIdx[i] >> 27], pCurForeignIdx[i] & ~BOP_NEWIDX0);
		}
		++pIdxMap.sz[0];
		++pIdxMap.sz[1];
		pIdxMap.ptr[1] = (pIdxMap.ptr[0] = new int[pIdxMap.sz[0] + pIdxMap.sz[1]]) + pIdxMap.sz[0];
		for (i = 0; i < pIdxMap.sz[0] + pIdxMap.sz[1]; i++)
		{
			pIdxMap.ptr[0][i] = -1;
		}

		for (i = 0; i < nTris; i++)
		{
			pIdxMap[pCurForeignIdx[i]] = pNewForeignIdx[i];
		}
		for (i = 0; i < m_nTris; i++)
		{
			m_pForeignIdx[i] = pIdxMap.MapSafe(m_pForeignIdx[i]);
		}

		for (pmu = m_pMeshUpdate; pmu; pmu = pmu->next)
		{
			for (i = 0; i < pmu->nNewVtx; i++)
			{
				pmu->pNewVtx[i].idxTri[0] = pIdxMap.MapSafe(pmu->pNewVtx[i].idxTri[0]);
			}
			for (i = 0; i < pmu->nNewTri; i++)
			{
				if (pmu->pNewTri[i].iop == 0)
				{
					pmu->pNewTri[i].idxOrg = pIdxMap.MapSafe(pmu->pNewTri[i].idxOrg);
				}
			}
			for (i = 0; i < pmu->nRemovedTri; i++)
			{
				pmu->pRemovedTri[i] = pIdxMap.MapSafe(pmu->pRemovedTri[i]);
			}
			for (i = 0; i < pmu->nTJFixes; i++)
			{
				pmu->pTJFixes[i].iABC = pIdxMap.MapSafe(pmu->pTJFixes[i].iABC);
				pmu->pTJFixes[i].iACJ = pIdxMap.MapSafe(pmu->pTJFixes[i].iACJ);
			}
		}
		for (pmu = (bop_meshupdate*)m_refmu.nextRef; pmu != &m_refmu; pmu = (bop_meshupdate*)pmu->nextRef)
		{
			for (i = 0; i < pmu->nNewVtx; i++)
			{
				pmu->pNewVtx[i].idxTri[1] = pIdxMap.MapSafe(pmu->pNewVtx[i].idxTri[1]);
			}
			iop = this != pmu->pMesh[0];
			for (i = 0; i < pmu->nNewTri; i++)
			{
				if (pmu->pNewTri[i].iop == iop)
				{
					pmu->pNewTri[i].idxOrg = pIdxMap.MapSafe(pmu->pNewTri[i].idxOrg);
				}
			}
		}

		delete[] pIdxMap.ptr[0];
	}
}

void CTriMesh::AppendVertices(Vec3* pVtx, int* pVtxMap, int nVtx)
{
	if (nVtx > 0)
	{
		ReallocateList(m_pVertices.data, m_nVertices, m_nVertices + nVtx);
		if (m_pVtxMap)
		{
			ReallocateList(m_pVtxMap, m_nVertices, m_nVertices + nVtx);
		}
		m_pVertices.iStride = sizeof(Vec3);
		m_flags &= ~mesh_shared_vtx;

		int i;
		for (i = 0; i < nVtx; i++)
		{
			m_pVertices[m_nVertices + i] = pVtx[i];
		}
		if (m_pVtxMap && pVtxMap)
		{
			for (i = 0; i < nVtx; i++)
			{
				m_pVtxMap[m_nVertices + i] = pVtxMap[i];
			}
		}
		m_nVertices += nVtx;
	}
}

float CTriMesh::GetIslandDisk(int matid, const Vec3& ptref, Vec3& center, Vec3& normal, float& peakDist)
{
	if (!m_pIds)
	{
		return 0;
	}
	ReadLock lock(m_lockUpdate);
	const int szq = 256;
	int i, j, itri, itri1 = -1, ihead, itail, queue[szq];
	Vec3 cnt, cnt0(1E10f, 1E10f, 1E10f), ntot;
	float area, totArea, det, maxdet, r;
	Matrix33 C, Ctmp;
	primitives::box bbox;

	for (itri = 0; itri < m_nTris; itri++)
	{
		if (m_pIds[itri] == matid)
		{
			cnt = (m_pVertices[m_pIndices[itri * 3]] + m_pVertices[m_pIndices[(itri * 3) + 1]] +
			       m_pVertices[m_pIndices[(itri * 3) + 2]]) *
			      (1.0f / 3);
			if ((cnt - ptref).len2() < (cnt0 - ptref).len2())
			{
				cnt0 = cnt;
				itri1 = itri;
			}
		}
	}
	if (itri1 < 0)
	{
		return 0;
	}

	queue[itail = 0] = itri1;
	ihead = 1;
	cnt.zero();
	totArea = 0;
	C.SetZero();
	ntot.zero();
	do
	{
		itri = queue[itail];
		++itail &= szq - 1;
		const Vec3 &pt0 = m_pVertices[m_pIndices[itri * 3]], &pt1 = m_pVertices[m_pIndices[(itri * 3) + 1]],
			   &pt2 = m_pVertices[m_pIndices[(itri * 3) + 2]];
		totArea += (area = (pt1 - pt0 ^ pt2 - pt0) * normal);
		cnt += (pt0 + pt1 + pt2) * (area * (1.0f / 3));
		ntot += m_pNormals[itri];
		for (i = 0; i < 3; i++)
		{
			if ((itri1 = m_pTopology[itri].ibuddy[i]) >= 0)
			{
				if (m_pIds[itri1] == matid)
				{
					if (ihead != ((itail - 1) & (szq - 1)))
					{
						m_pIds[queue[ihead] = itri1] = static_cast<char>(255);
						++ihead &= szq - 1;
					}
				}
				else if (m_pIds[itri1] != static_cast<char>(255))
				{
					C += dotproduct_matrix(m_pNormals[itri1], m_pNormals[itri1], Ctmp);
				}
			}
		}
	}
	while (ihead != itail);
	if (totArea != 0)
	{
		cnt /= totArea;
	}

	for (i = 0, j = -1, maxdet = 0; i < 3; i++)
	{
		det = (C(inc_mod3[i], inc_mod3[i]) * C(dec_mod3[i], dec_mod3[i])) -
		      (C(dec_mod3[i], inc_mod3[i]) * C(inc_mod3[i], dec_mod3[i]));
		if (fabs_tpl(det) > fabs_tpl(maxdet))
		{
			maxdet = det, j = i;
		}
	}
	if (j >= 0)
	{
		det = 1.0 / maxdet;
		normal[j] = 1;
		normal[inc_mod3[j]] = -((C(inc_mod3[j], j) * C(dec_mod3[j], dec_mod3[j])) -
		                        (C(dec_mod3[j], j) * C(inc_mod3[j], dec_mod3[j]))) *
		                      det;
		normal[dec_mod3[j]] = -((C(inc_mod3[j], inc_mod3[j]) * C(dec_mod3[j], j)) -
		                        (C(dec_mod3[j], inc_mod3[j]) * C(inc_mod3[j], j))) *
		                      det;
		normal.normalize() *= sgnnz(ntot * normal);
	}
	else
	{
		return 0;
	}

	m_pTree->GetBBox(&bbox);
	r = max(max(bbox.size.x, bbox.size.y), bbox.size.z) * 2;
	peakDist = 0;
	for (itri = 0; itri < m_nTris; itri++)
	{
		if (m_pIds[itri] == static_cast<char>(255))
		{
			for (i = 0; i < 3; i++)
			{
				if ((itri1 = m_pTopology[itri].ibuddy[i]) < 0 ||
				    m_pIds[itri1] != static_cast<char>(255) && m_pIds[itri1] != matid)
				{
					r = min(r, (cnt - m_pVertices[m_pIndices[(itri * 3) + i]]).len2());
					r = min(r, (cnt - m_pVertices[m_pIndices[(itri * 3) + inc_mod3[i]]]).len2());
				}
				peakDist = max(peakDist, (m_pVertices[m_pIndices[(itri * 3) + i]] - cnt) * normal);
			}
			m_pIds[itri] = matid;
		}
	}

	r = sqrt_tpl(r);
	center = cnt;
	return r;
}

void CTriMesh::DrawWireframe(IPhysRenderer* pRenderer, geom_world_data* gwd, int iLevel, int idxColor)
{
	int i, ix, iy;
	if (iLevel == 0)
	{
		pRenderer->DrawGeometry(this, gwd, idxColor);
	}
	else
	{
		int iCaller = GetCaller();
		BV* pbbox;
		ResetGlobalPrimsBuffers(iCaller);
		m_pTree->ResetCollisionArea();
		m_pTree->GetNodeBV(gwd->R, gwd->offset, gwd->scale, pbbox, 0, iCaller);
		if (pbbox->type == primitives::box::type)
		{
			DrawBBox(pRenderer, idxColor, gwd, m_pTree, (BBox*)pbbox, iLevel - 1, 0, iCaller);
		}
		else if (pbbox->type == primitives::voxelgrid::type)
		{
			primitives::voxelgrid* pgrid = (primitives::voxelgrid*)(primitives::primitive*)*pbbox;
			Vec3 pt0, pt1;
			for (i = 0; i < 3; i++)
			{
				for (ix = 0; ix <= pgrid->size[inc_mod3[i]]; ix++)
				{
					for (iy = 0; iy <= pgrid->size[dec_mod3[i]]; iy++)
					{
						pt0[inc_mod3[i]] = ix * pgrid->step[inc_mod3[i]];
						pt0[dec_mod3[i]] = iy * pgrid->step[dec_mod3[i]];
						pt0[i] = 0;
						(pt1 = pt0)[i] = pgrid->size[i] * pgrid->step[i];
						pRenderer->DrawLine(pgrid->origin + pt0 * pgrid->Basis,
						                    pgrid->origin + pt1 * pgrid->Basis, idxColor);
					}
				}
			}
		}
	}
}

void CTriMesh::Save(CMemStream& stm)
{
	int i, dummy[4];

	stm.Write(m_nVertices);
	stm.Write(m_nTris);
	stm.Write(m_nMaxVertexValency);
	stm.Write(m_flags);
	if (m_pVtxMap)
	{
		stm.Write(true);
		for (i = 0; i < m_nVertices; i++)
		{
			stm.Write(*(unsigned short*)(m_pVtxMap + i));
		}
	}
	else
	{
		stm.Write(false);
	}

	if (m_pForeignIdx)
	{
		stm.Write(true);
		for (i = 0; i < m_nTris; i++)
		{
			stm.Write(*(unsigned short*)(m_pForeignIdx + i));
		}
	}
	else
	{
		stm.Write(false);
		if (!(m_flags & mesh_shared_vtx))
		{
			for (i = 0; i < m_nVertices; i++)
			{
				stm.Write(m_pVertices[i]);
			}
		}
		for (i = 0; i < m_nTris * 3; i++)
		{
			stm.Write(*(short*)(m_pIndices + i));
		}
		if (m_pIds)
		{
			stm.Write(true);
			stm.Write(m_pIds, sizeof(m_pIds[0]) * m_nTris);
		}
		else
		{
			stm.Write(false);
		}
	}

	stm.Write(dummy);
	stm.Write((int)0);

	for (i = 0; i < 4; i++)
	{
		stm.Write(m_bConvex[i]);
		stm.Write(m_ConvexityTolerance[i]);
	}

	stm.Write(m_pTree->GetType());
	m_pTree->Save(stm);
}

void CTriMesh::Load(CMemStream& stm, strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices,
                    char* pIds)
{
	int i, j, dummy[4];
	bool bForeignIdx, bVtxMap, bIds;
	primitives::box bbox;

	stm.Read(m_nVertices);
	stm.Read(m_nTris);
	stm.Read(m_nMaxVertexValency);
	stm.Read(m_flags);
	m_flags |= mesh_keep_vtxmap;

	m_pIndices = new index_t[m_nTris * 3];

	stm.Read(bVtxMap);
	if (bVtxMap)
	{
		m_pVtxMap = new int[m_nVertices];
		for (i = 0; i < m_nVertices; i++)
		{
			m_pVtxMap[i] = stm.Read<uint16>();
		}
	}

	stm.Read(bForeignIdx);
	if (bForeignIdx)
	{
		if (!(m_flags & mesh_shared_vtx))
		{
			m_pVertices = new Vec3[m_nVertices];
			for (i = 0; i < m_nVertices; i++)
			{
				m_pVertices[i] = pVertices[i];
			}
		}
		else
		{
			m_pVertices = strided_pointer<Vec3>((Vec3*)pVertices.data, pVertices.iStride);
		}
		if (pIds)
		{
			m_pIds = new char[m_nTris];
			for (i = 0; i < m_nTris; i++)
			{
				m_pIds[i] = pIds[i];
			}
		}
		else
		{
			m_pIds = 0;
		}

		m_pForeignIdx = new int[m_nTris];
		for (i = 0; i < m_nTris; i++)
		{
			m_pForeignIdx[i] = stm.Read<uint16>();
			for (j = 0; j < 3; j++)
			{
				m_pIndices[(i * 3) + j] = pIndices[(m_pForeignIdx[i] * 3) + j];
			}
			if (m_pIds)
			{
				m_pIds[i] = pIds[m_pForeignIdx[i]];
			}
		}
	}
	else
	{
		if (!(m_flags & mesh_shared_vtx))
		{
			m_pVertices = new Vec3[m_nVertices];
			for (i = 0; i < m_nVertices; i++)
			{
				stm.Read(m_pVertices[i]);
			}
		}
		else
		{
			m_pVertices = strided_pointer<Vec3>((Vec3*)pVertices.data, pVertices.iStride);
		}
		for (i = 0; i < m_nTris * 3; i++)
		{
			m_pIndices[i] = stm.Read<uint16>();
		}
		stm.Read(bIds);
		if (bIds)
		{
			if (!m_pIds)
			{
				m_pIds = new char[m_nTris];
			}
			stm.ReadRaw(m_pIds, m_nTris);
		}
		else if (m_pIds)
		{
			delete[] m_pIds;
			m_pIds = 0;
		}
	}
	if (bVtxMap)
	{
		for (i = 0; i < m_nTris * 3; i++)
		{
			m_pIndices[i] = m_pVtxMap[m_pIndices[i]];
		}
		if (!(m_flags & mesh_keep_vtxmap))
		{
			delete[] m_pVtxMap;
			m_pVtxMap = 0;
		}
	}
	m_pNormals = new Vec3[m_nTris];
	for (i = 0; i < m_nTris; i++)
	{
		RecalcTriNormal(i);
	}

	m_pTopology = new trinfo[m_nTris];
	CalculateTopology(m_pIndices);

	stm.ReadRaw(dummy, sizeof dummy);

	stm.Read(i);
	if (i)
	{
		m_nTris = 0;
		CSingleBoxTree* pTree = new CSingleBoxTree;
		bbox.Basis.SetIdentity();
		bbox.center.zero();
		bbox.size.zero();
		pTree->SetBox(&bbox);
		pTree->Build(this);
		pTree->m_nPrims = m_nTris;
		m_pTree = pTree;
		return;
	}

	m_bMultipart = BuildIslandMap() > 1;
	for (i = 0; i < 4; i++)
	{
		stm.Read(m_bConvex[i]);
		m_bConvex[i] &= m_bMultipart ^ 1;
		stm.Read(m_ConvexityTolerance[i]);
	}
	m_bIsConvex = IsConvex(0.02f);

	stm.Read(i);
	switch (i)
	{
		case BVT_OBB:
			m_pTree = new COBBTree;
			break;
		case BVT_AABB:
			m_pTree = new CAABBTree;
			break;
		case BVT_SINGLEBOX:
			m_pTree = new CSingleBoxTree;
			break;
			// default: m_nTris=0; return;
	}
	m_pTree->Load(stm, this);

	m_pTree->GetBBox(&bbox);
	m_minVtxDist = max(max(bbox.size.x, bbox.size.y), bbox.size.z) * 0.0002f;
}

void CTriMesh::DestroyAuxilaryMeshData(int idata)
{
	if (idata & mesh_data_materials && m_pIds)
	{
		if (!(m_flags & mesh_shared_mats))
		{
			delete[] m_pIds;
		}
		m_pIds = 0;
	}
	if (idata & mesh_data_foreign_idx && m_pForeignIdx)
	{
		if (!(m_flags & mesh_shared_foreign_idx))
		{
			delete[] m_pForeignIdx;
		}
		m_pForeignIdx = 0;
	}
	if (idata & mesh_data_vtxmap && m_pVtxMap)
	{
		delete[] m_pVtxMap;
		m_pVtxMap = 0;
	}
}
