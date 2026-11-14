#include "CryCommon/CrySystem/ISystem.h"

#include "Polynomial.h" // cubert_tpl
#include "TetrLattice.h"
#include "TriMesh.h"
#include "Utils.h"

struct STetrahedron
{
	int flags;
	float M, Minv;
	float Vinv;
	Matrix33 Iinv;
	Vec3 Pext, Lext;
	float area;
	int ivtx[4];
	int ibuddy[4];
	float fracFace[4];
	int idxface[4];
	int idx;
};

struct SCGTetr
{
	Vec3 dP, dL;
	float Minv;
	Matrix33 Iinv;
};

struct SCGFace
{
	int itet, iface;
	Vec3 rv, rw;
	Vec3 dv, dw;
	Vec3 dP, dL;
	Vec3 P, L;
	SCGTetr* pTet[2];
	Vec3 r0, r1;
	Matrix33 vKinv, wKinv;
	int flags;
};

CTetrLattice::CTetrLattice(IPhysicalWorld* pWorld)
{
	m_pWorld = pWorld;
	m_pVtx = 0;
	m_nVtx = 0;
	m_pTetr = 0;
	m_nTetr = 0;
	m_nRemovedTets = 0;
	m_pVtxFlags = 0;
	m_pGridTet0 = 0;
	m_pGrid = 0;
	m_idmat = 0;
	m_nMaxCracks = 4;
	m_crackWeaken = 0.4f;
	m_maxForcePush = 0.01f;
	m_maxForcePull = m_maxForceShift = 0.01f;
	m_maxTorqueTwist = m_maxTorqueBend = 0.01f;
	m_density = 1.0f;
	m_flags = 0;
	m_pVtxRemap = 0;
	m_maxTension = 0;
	m_imaxTension = LPush;
	m_lastImpulseTime = -1;
}

CTetrLattice::CTetrLattice(CTetrLattice* src, int bCopyData)
{
	m_pWorld = src->m_pWorld;
	m_idmat = src->m_idmat;
	m_nMaxCracks = src->m_nMaxCracks;
	m_crackWeaken = src->m_crackWeaken;
	m_maxForcePush = src->m_maxForcePush;
	m_maxForcePull = src->m_maxForcePull;
	m_maxForceShift = src->m_maxForceShift;
	m_maxTorqueTwist = src->m_maxTorqueTwist;
	m_maxTorqueBend = src->m_maxTorqueBend;
	m_RGrid = src->m_RGrid;
	m_posGrid = src->m_posGrid;
	m_stepGrid = src->m_stepGrid;
	m_rstepGrid = src->m_rstepGrid;
	m_szGrid = src->m_szGrid;
	m_strideGrid = src->m_strideGrid;
	m_density = src->m_density;
	m_flags = src->m_flags;
	m_pVtxRemap = 0;
	m_maxTension = 0;
	m_imaxTension = LPush;

	if (bCopyData)
	{
		memcpy(m_pVtx = new Vec3[m_nVtx = src->m_nVtx], src->m_pVtx, src->m_nVtx * sizeof(m_pVtx[0]));
		memcpy(m_pVtxFlags = new int[m_nVtx], src->m_pVtxFlags, m_nVtx * sizeof(m_pVtxFlags[0]));
		memcpy(m_pTetr = new STetrahedron[m_nTetr = src->m_nTetr], src->m_pTetr,
		       src->m_nTetr * sizeof(m_pTetr[0]));
		memcpy(m_pGridTet0 = new int[m_szGrid.GetVolume() + 1], src->m_pGridTet0,
		       (m_szGrid.GetVolume() + 1) * sizeof(m_pGridTet0[0]));
		memcpy(m_pGrid = new int[m_pGridTet0[m_szGrid.GetVolume()]], src->m_pGrid,
		       m_pGridTet0[m_szGrid.GetVolume()] * sizeof(m_pGrid[0]));
		m_nRemovedTets = src->m_nRemovedTets;
	}
	else
	{
		m_pVtx = 0;
		m_nVtx = 0;
		m_pTetr = 0;
		m_nTetr = 0;
		m_nRemovedTets = 0;
		m_pVtxFlags = 0;
		m_pGridTet0 = 0;
		m_pGrid = 0;
	}
}

CTetrLattice::~CTetrLattice()
{
	if (m_pVtx)
	{
		delete[] m_pVtx;
	}
	if (m_pVtxFlags)
	{
		delete[] m_pVtxFlags;
	}
	if (m_pTetr)
	{
		delete[] m_pTetr;
	}
	if (m_pGridTet0)
	{
		delete[] m_pGridTet0;
	}
	if (m_pGrid)
	{
		delete[] m_pGrid;
	}
	if (m_pVtxRemap)
	{
		delete[] m_pVtxRemap;
	}
}

CTetrLattice* CTetrLattice::CreateLattice(const Vec3* pt, int npt, const int* pTets, int nTets)
{
	int i, j, *pTet0, *pVtxTets, pEdgeTets[64], pFaceTets[8];
	Vec3 vtx[4], vsum, n;
	Matrix33 I, mtx;

	memcpy(m_pVtx = new Vec3[m_nVtx = npt], pt, npt * sizeof(m_pVtx[0]));
	memset(m_pVtxFlags = new int[m_nVtx], 0, npt * sizeof(m_pVtxFlags[0]));
	m_pTetr = new STetrahedron[m_nTetr = nTets];

	for (i = 0; i < m_nTetr; i++)
	{
		m_pTetr[i].flags = 0;
		for (j = 0, vsum.zero(); j < 4; j++)
		{
			vsum += (vtx[j] = pt[m_pTetr[i].ivtx[j] = pTets[(i * 4) + j]]);
		}
		if ((vtx[1] - vtx[0] ^ vtx[2] - vtx[0]) * (vtx[3] - vtx[0]) > 0)
		{
			vtx[0] = pt[m_pTetr[i].ivtx[0] = pTets[(i * 4) + 1]];
			vtx[1] = pt[m_pTetr[i].ivtx[1] = pTets[i * 4]];
		}
		for (j = 0; j < 4; j++)
		{
			vtx[j] -= vsum * 0.25f;
		}
		n = vtx[3] - vtx[0] ^ vtx[2] - vtx[0];
		m_pTetr[i].M = ((vtx[1] - vtx[0]) * n) * (1.0f / 6);
		m_pTetr[i].Vinv = m_pTetr[i].Minv = 1 / m_pTetr[i].M;
		m_pTetr[i].area = n.len() * 0.5f;
		I.SetZero();
		for (j = 0; j < 4; j++)
		{
			I -= dotproduct_matrix(vtx[j], vtx[j], mtx);
		}
		vsum.Set(-I(0, 0), -I(1, 1), -I(2, 2));
		for (j = 0; j < 3; j++)
		{
			I(j, j) = vsum[inc_mod3[j]] + vsum[dec_mod3[j]];
		}
		(m_pTetr[i].Iinv = I * (m_pTetr[i].M * (1.0f / 20))).Invert();
		for (j = 0; j < 4; j++)
		{
			m_pTetr[i].fracFace[j] =
			    (vtx[(j + 2) & 3] - vtx[(j + 1) & 3] ^ vtx[(j + 3) & 3] - vtx[(j + 1) & 3]).len();
		}
		m_pTetr[i].idx = -1;
		m_pTetr[i].Pext.zero();
		m_pTetr[i].Lext.zero();
	}

	memset(pTet0 = new int[m_nVtx + 1], 0,
	       (m_nVtx + 1) * sizeof(int)); // for each used vtx, points to the corresponding tetr list start
	pVtxTets = new int[m_nTetr * 4];    // holds tetrahedra lists for each used vtx

	for (i = 0; i < m_nTetr; i++)
	{
		for (j = 0; j < 4; j++)
		{
			pTet0[m_pTetr[i].ivtx[j]]++;
		}
	}
	for (i = 0; i < m_nVtx; i++)
	{
		pTet0[i + 1] += pTet0[i];
	}
	for (i = m_nTetr - 1; i >= 0; i--)
	{
		for (j = 0; j < 4; j++)
		{
			pVtxTets[--pTet0[m_pTetr[i].ivtx[j]]] = i;
		}
	}
	for (i = 0; i < m_nTetr; i++)
	{
		for (j = 0; j < 4; j++)
		{
			nTets = intersect_lists(
			    pVtxTets + pTet0[m_pTetr[i].ivtx[(j + 1) & 3]],
			    pTet0[m_pTetr[i].ivtx[(j + 1) & 3] + 1] - pTet0[m_pTetr[i].ivtx[(j + 1) & 3]],
			    pVtxTets + pTet0[m_pTetr[i].ivtx[(j + 2) & 3]],
			    pTet0[m_pTetr[i].ivtx[(j + 2) & 3] + 1] - pTet0[m_pTetr[i].ivtx[(j + 2) & 3]], pEdgeTets);
			nTets = intersect_lists(pVtxTets + pTet0[m_pTetr[i].ivtx[(j + 3) & 3]],
			                        pTet0[m_pTetr[i].ivtx[(j + 3) & 3] + 1] -
			                            pTet0[m_pTetr[i].ivtx[(j + 3) & 3]],
			                        pEdgeTets, nTets, pFaceTets);
			// if (nTets>2) - error in topology
			m_pTetr[i].ibuddy[j] = pFaceTets[iszero(i - pFaceTets[0])] | (nTets - 2) >> 31;
		}
	}

	delete[] pVtxTets;
	delete[] pTet0;
	return this;
}

void CTetrLattice::SetMesh(CTriMesh* pMesh)
{
	m_pMesh = pMesh;

	if (!m_pGrid)
	{
		int i, j, k, i0, i1, i2, nSlots, *pTetCells = 0;
		float rstep, s, e;
		Vec3 sz, n, pt, vtx[4], BBox[2];
		Vec3i iBBox[2], ic;
		primitives::box bbox;

		m_pMesh->GetBBox(&bbox);
		m_RGrid = bbox.Basis.T();
		BBox[0] = BBox[1] = m_pVtx[0] * m_RGrid;
		for (i = 1; i < m_nVtx; i++)
		{
			if (!(m_pVtxFlags[i] & lvtx_removed))
			{
				BBox[0] = min(BBox[0], pt = m_pVtx[i] * m_RGrid);
				BBox[1] = max(BBox[1], pt);
			}
		}
		m_posGrid = m_RGrid * BBox[0];
		sz = BBox[1] - BBox[0];
		rstep = cubert_tpl(m_nTetr * 4 / sz.GetVolume());
		for (i = 0; i < 3; i++)
		{
			m_szGrid[i] = max(1, physics_float2int(sz[i] * rstep));
			m_stepGrid[i] = sz[i] / m_szGrid[i];
			m_rstepGrid[i] = m_szGrid[i] / sz[i];
		}
		m_strideGrid.Set(m_szGrid.z * m_szGrid.y, m_szGrid.z, 1);
		memset(m_pGridTet0 = new int[m_szGrid.GetVolume() + 1], 0,
		       (m_szGrid.GetVolume() + 1) * sizeof(m_pGridTet0[0]));
		;

		for (i = nSlots = 0; i < m_nTetr; i++)
		{
			m_pTetr[i].idxface[0] = nSlots;
			BBox[0] = BBox[1] = vtx[0] = (m_pVtx[m_pTetr[i].ivtx[0]] - m_posGrid) * m_RGrid;
			for (j = 1; j < 4; j++)
			{
				vtx[j] = (m_pVtx[m_pTetr[i].ivtx[j]] - m_posGrid) * m_RGrid;
				BBox[0] = min(BBox[0], vtx[j]);
				BBox[1] = max(BBox[1], vtx[j]);
			}
			for (j = 0; j < 3; j++)
			{
				iBBox[0][j] =
				    min(m_szGrid[j], max(0, physics_float2int((BBox[0][j] * m_rstepGrid[j]) - 0.5f)));
			}
			for (j = 0; j < 3; j++)
			{
				iBBox[1][j] =
				    min(m_szGrid[j], max(0, physics_float2int((BBox[1][j] * m_rstepGrid[j]) + 0.5f)));
			}
			for (ic.x = iBBox[0].x; ic.x < iBBox[1].x; ic.x++)
			{
				for (ic.y = iBBox[0].y; ic.y < iBBox[1].y; ic.y++)
				{
					for (ic.z = iBBox[0].z; ic.z < iBBox[1].z; ic.z++)
					{
						pt.Set((ic.x + 0.5f) * m_stepGrid.x, (ic.y + 0.5f) * m_stepGrid.y,
						       (ic.z + 0.5f) * m_stepGrid.z);
						for (j = 0; j < 4; j++)
						{
							n = vtx[(j + 2) & 3] - vtx[(j + 1) & 3] ^
							    vtx[(j + 3) & 3] - vtx[(j + 1) & 3];
							s = vtx[(j + 1) & 3] * n;
							e = vtx[j] * n;
							if (fabs_tpl(s + e - ((pt * n) * 2)) >
							    fabs_tpl(s - e) + m_stepGrid * n.abs())
							{
								goto separate;
							}
						}
						for (j = 0; j < 6; j++)
						{
							for (k = 0; k < 3; k++)
							{
								i2 = j >> 2;
								i0 = (j >> 1) + i2;
								i1 = (i0 + 1 + ((j & 1) << i2)) & 3;
								n = cross_with_ort(vtx[i0] - vtx[i1], k);
								s = min(min(min(vtx[0] * n, vtx[1] * n), vtx[2] * n),
								        vtx[3] * n);
								e = max(max(max(vtx[0] * n, vtx[1] * n), vtx[2] * n),
								        vtx[3] * n);
								if (fabs_tpl(s + e - ((pt * n) * 2)) >
								    e - s + m_stepGrid * n.abs())
								{
									goto separate;
								}
							}
						}
						m_pGridTet0[j = ic * m_strideGrid]++;
						if ((nSlots & 255) == 0)
						{
							ReallocateList(pTetCells, nSlots, nSlots + 256);
						}
						pTetCells[nSlots++] = j;
					separate:;
					}
				}
			}
			m_pTetr[i].idxface[1] = nSlots;
		}
		m_pGrid = new int[nSlots];
		for (i = 1, j = m_szGrid.GetVolume(); i <= j; i++)
		{
			m_pGridTet0[i] += m_pGridTet0[i - 1];
		}
		for (i = m_nTetr - 1; i >= 0; i--)
		{
			for (j = m_pTetr[i].idxface[0]; j < m_pTetr[i].idxface[1]; j++)
			{
				m_pGrid[--m_pGridTet0[pTetCells[j]]] = i;
			}
		}
		delete[] pTetCells;
	}
}

int CTetrLattice::SetParams(pe_params* _params)
{
	if (_params->type == pe_tetrlattice_params::type_id)
	{
		pe_tetrlattice_params* params = (pe_tetrlattice_params*)_params;
		if (!is_unused(params->density) && params->density != m_density)
		{
			float diff = params->density / m_density, rdiff = 1 / diff;
			for (int i = 0; i < m_nTetr; i++)
			{
				m_pTetr[i].M *= diff;
				m_pTetr[i].Minv *= rdiff;
				m_pTetr[i].Iinv *= rdiff;
			}
			m_maxForcePush *= diff;
			m_maxForcePull *= diff;
			m_maxForceShift *= diff;
			m_maxTorqueTwist *= diff;
			m_maxTorqueBend *= diff;
			m_density = params->density;
		}
		if (!is_unused(params->nMaxCracks))
		{
			m_nMaxCracks = params->nMaxCracks;
		}
		if (!is_unused(params->maxForcePush))
		{
			m_maxForcePush = params->maxForcePush;
		}
		if (!is_unused(params->maxForcePull))
		{
			m_maxForcePull = params->maxForcePull;
		}
		if (!is_unused(params->maxForceShift))
		{
			m_maxForceShift = params->maxForceShift;
		}
		if (!is_unused(params->maxTorqueTwist))
		{
			m_maxTorqueTwist = params->maxTorqueTwist;
		}
		if (!is_unused(params->maxTorqueBend))
		{
			m_maxTorqueBend = params->maxTorqueBend;
		}
		if (!is_unused(params->crackWeaken))
		{
			m_crackWeaken = params->crackWeaken;
		}

		return 1;
	}
	return 0;
}

int CTetrLattice::GetParams(pe_params* _params)
{
	if (_params->type == pe_tetrlattice_params::type_id)
	{
		pe_tetrlattice_params* params = (pe_tetrlattice_params*)_params;
		params->density = m_density;
		params->nMaxCracks = m_nMaxCracks;
		params->maxForcePush = m_maxForcePush;
		params->maxForcePull = m_maxForcePull;
		params->maxForceShift = m_maxForceShift;
		params->maxTorqueTwist = m_maxTorqueTwist;
		params->maxTorqueBend = m_maxTorqueBend;
		params->crackWeaken = m_crackWeaken;
		return 1;
	}
	return 0;
}

void CTetrLattice::Subtract(IGeometry* pGeom, const geom_world_data* pgwd1, const geom_world_data* pgwd2)
{
	// only the lattice is updated here, not m_pMesh
	static float g_recip[] = {0, 1, 0.5f, 1.0f / 3, 0.25f};
	int i, idx, state0, state1, ivtx0 = -1, itet0 = -1;
	float frac, rfrac, rscale1 = pgwd1->scale == 1.0f ? 1.0f : 1.0f / pgwd1->scale,
			   rscale2 = pgwd2->scale == 1.0f ? 1.0f : 1.0f / pgwd2->scale;
	primitives::box bboxLoc;
	Vec3 pos, sz, center, n;
	Vec3i ic, iBBox[2];
	Matrix33 R;
	STetrahedron* ptet;

	pGeom->GetBBox(&bboxLoc);
	bboxLoc.Basis *= pgwd2->R.T() * pgwd1->R;
	R = bboxLoc.Basis * m_RGrid;
	bboxLoc.center =
	    ((pgwd2->offset + pgwd2->R * bboxLoc.center * pgwd2->scale - pgwd1->offset) * pgwd1->R) * rscale1;
	bboxLoc.size *= pgwd2->scale * rscale1;
	sz = bboxLoc.size * R.Fabs();
	pos = (bboxLoc.center - m_posGrid) * m_RGrid;
	for (i = 0; i < 3; i++)
	{
		iBBox[0][i] = min(m_szGrid[i], max(0, physics_float2int(((pos[i] - sz[i]) * m_rstepGrid[i]) - 0.5f)));
		iBBox[1][i] = min(m_szGrid[i], max(0, physics_float2int(((pos[i] + sz[i]) * m_rstepGrid[i]) + 0.5f)));
	}
	R = pgwd2->R.T() * pgwd1->R * (pgwd1->scale * rscale2);
	pos = ((pgwd1->offset - pgwd2->offset) * rscale2) * pgwd2->R;
	center = ((pgwd2->R * pGeom->GetCenter() * pgwd2->scale + pgwd2->offset - pgwd1->offset) * pgwd1->R) * rscale1;

	for (ic.x = iBBox[0].x; ic.x < iBBox[1].x; ic.x++)
	{
		for (ic.y = iBBox[0].y; ic.y < iBBox[1].y; ic.y++)
		{
			for (ic.z = iBBox[0].z; ic.z < iBBox[1].z; ic.z++)
			{
				for (idx = m_pGridTet0[ic * m_strideGrid]; idx < m_pGridTet0[ic * m_strideGrid + 1];
				     idx++)
				{
					if (!((ptet = m_pTetr + m_pGrid[idx])->flags & (ltet_removed | ltet_processed)))
					{
						for (ptet = m_pTetr + m_pGrid[idx], i = 0, state0 = 15; i < 4; i++)
						{
							state0 ^= (m_pVtxFlags[ptet->ivtx[i]] & lvtx_removed) << i;
						}

						for (i = 0, state1 = state0, frac = -1; i < 4; i++)
						{
							if (!(m_pVtxFlags[ptet->ivtx[i]] &
							      (lvtx_removed | lvtx_processed)))
							{ // check if the vertex is inside pGeom
								sz = bboxLoc.Basis *
								     (m_pVtx[ptet->ivtx[i]] - bboxLoc.center);
								if (max(max(fabs_tpl(sz.x) - bboxLoc.size.x,
								            fabs_tpl(sz.y) - bboxLoc.size.y),
								        fabs_tpl(sz.z) - bboxLoc.size.z) < 0)
								{
									m_pVtxFlags[ptet->ivtx[i]] |=
									    lvtx_removed_new &
									    -pGeom->PointInsideStatus(
										R * m_pVtx[ptet->ivtx[i]] + pos);
								}
								m_pVtxFlags[ptet->ivtx[i]] =
								    (m_pVtxFlags[ptet->ivtx[i]] &
								     ~(-1 << lvtx_inext_log2)) |
								    ivtx0 << lvtx_inext_log2 | lvtx_processed;
								ivtx0 = ptet->ivtx[i]; // maintain a linked list of
								                       // processed vertices
							}
							state1 ^= (m_pVtxFlags[ptet->ivtx[i]] >> 1 & lvtx_removed) << i;
							n = (m_pVtx[ptet->ivtx[(i + 2) & 3]] -
							         m_pVtx[ptet->ivtx[(i + 1) & 3]] ^
							     m_pVtx[ptet->ivtx[(i + 3) & 3]] -
							         m_pVtx[ptet->ivtx[(i + 1) & 3]]) *
							    ((i * 2 & 2) - 1);
							frac =
							    max(frac, n * (center - m_pVtx[ptet->ivtx[(i + 1) & 3]]));
						}

						if (frac <= 0)
						{ // weaken the tetrahedron if pGeom's center is inside it
							frac = 1.0f -
							       min(0.9f, max(0.1f, pGeom->GetVolume() *
							                               cube(pgwd2->scale * rscale1) *
							                               0.7f * ptet->Vinv));
							rfrac = 1.0f / frac;
							ptet->M *= frac;
							ptet->Minv *= rfrac;
							ptet->Vinv *= rfrac;
							ptet->Iinv *= rfrac;
							for (i = 0; i < 4; i++)
							{
								ptet->fracFace[i] *= frac;
								if (ptet->ibuddy[i] >= 0)
								{
									m_pTetr[ptet->ibuddy[i]]
									    .fracFace[GetFaceByBuddy(
										ptet->ibuddy[i], m_pGrid[idx])] *= frac;
								}
							}
						}

						if (state0 != state1)
						{
							for (i = 0; i < 4; i++) // weaken each face by the number of
							                        // removed vertices 2-66%, 1-33%, 0-0%
							{
								ptet->fracFace[i] *=
								    g_bitcount[state1 & ~(1 << i)] *
								    g_recip[g_bitcount[state0 & ~(1 << i)]];
							}
						}
						if (state1 == 0)
						{ // if all 4 vertices are removed, remove the entire tetrahedron
							ptet->flags |= ltet_removed;
							for (i = 0; i < 4; i++)
							{
								if (ptet->ibuddy[i] >= 0)
								{
									m_pTetr[ptet->ibuddy[i]].ibuddy[GetFaceByBuddy(
									    ptet->ibuddy[i], m_pGrid[idx])] = -1;
								}
							}
							m_nRemovedTets++;
						}
						ptet->flags = (ptet->flags & ~(-1 << ltet_inext_log2)) |
						              itet0 << ltet_inext_log2 |
						              ltet_processed; // link processed tets into a list
						itet0 = m_pGrid[idx];
					}
				}
			}
		}
	}

	// reset processed flag and set removed<-removed_new for the processed vertices
	for (; ivtx0 != -1; ivtx0 = m_pVtxFlags[ivtx0] >> lvtx_inext_log2)
	{
		m_pVtxFlags[ivtx0] = (m_pVtxFlags[ivtx0] & ~(lvtx_processed | lvtx_removed_new)) |
		                     (m_pVtxFlags[ivtx0] & lvtx_removed_new) >> 1;
	}
	for (; itet0 != -1; itet0 = m_pTetr[itet0].flags >> ltet_inext_log2)
	{
		m_pTetr[itet0].flags &= ~ltet_processed;
	}
}

int CTetrLattice::CheckStructure(float time_interval, const Vec3& gravity, const primitives::plane* pGround,
                                 int nPlanes, pe_explosion* pexpl, int maxIters, int bLogTension)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PHYSICS);

	int i, j, nTets = 0, nFaces = 0, iter;
	Vec3 pt, dw0, dw1, n;
	real pAp, a, b, r2 = 0, r2new;
	float e, vmax, t2 = sqr(time_interval), area2, Pn, Ln, nlen2;
	Matrix33 rmtx;
	quotientf tens, tensMax;
	SCGTetr *pTet0, *pTet1;
	e = gravity.len() * time_interval * 0.05f;

	for (i = 0; i < m_nTetr; i++)
	{
		if (!(m_pTetr[i].flags & ltet_removed))
		{
			if (nTets == g_nTetsAlloc)
			{
				ReallocateList(g_Tets, nTets, g_nTetsAlloc += 32);
			}
			for (j = 0, vmax = e; j < 4; j++)
			{
				for (iter = 0; iter < nPlanes; iter++)
				{
					vmax = min(vmax, pGround[iter].n *
					                     (m_pVtx[m_pTetr[i].ivtx[j]] - pGround[iter].origin));
				}
			}
			if (vmax >= 0)
			{
				g_Tets[nTets].Minv = m_pTetr[i].Minv;
				g_Tets[nTets].Iinv = m_pTetr[i].Iinv;
				g_Tets[nTets].dP = gravity * time_interval;
				if (pexpl)
				{ // add velocity from explosion
					n = GetTetrCenter(i) - pexpl->epicenter;
					nlen2 = n.len();
					g_Tets[nTets].dP += n * (pexpl->impulsivePressureAtR * sqr(pexpl->r) /
					                         (max(1E-5f, nlen2) * sqr(max(pexpl->rmin, nlen2))) *
					                         m_pTetr[i].area * 0.3f);
				}
			}
			else
			{
				g_Tets[nTets].Minv = 0;
				g_Tets[nTets].Iinv.SetZero();
				g_Tets[nTets].dP.zero();
			}
			g_Tets[nTets].dL.zero();
			m_pTetr[i].idx = nTets++;
		}
		for (j = 0; j < 4; j++)
		{
			m_pTetr[i].idxface[j] = -1;
		}
	}
	for (i = 0, vmax = 0; i < m_nTetr; i++)
	{
		if (!(m_pTetr[i].flags & ltet_removed))
		{
			for (j = 0; j < 4; j++)
			{
				if (m_pTetr[i].ibuddy[j] > i && !(m_pTetr[m_pTetr[i].ibuddy[j]].flags & ltet_removed) &&
				    max(g_Tets[m_pTetr[i].idx].Minv, g_Tets[m_pTetr[m_pTetr[i].ibuddy[j]].idx].Minv) >
				        0 &&
				    m_pTetr[i].fracFace[j] > 0)
				{
					if (nFaces == g_nFacesAlloc)
					{
						ReallocateList(g_Faces, nFaces, g_nFacesAlloc += 32);
					}
					g_Faces[nFaces].itet = i;
					g_Faces[nFaces].iface = j;
					m_pTetr[i].idxface[j] = m_pTetr[m_pTetr[i].ibuddy[j]]
					                            .idxface[GetFaceByBuddy(m_pTetr[i].ibuddy[j], i)] =
					    nFaces;
					pTet0 = g_Faces[nFaces].pTet[0] = g_Tets + m_pTetr[i].idx;
					pTet1 = g_Faces[nFaces].pTet[1] = g_Tets + m_pTetr[m_pTetr[i].ibuddy[j]].idx;
					pt = (m_pVtx[m_pTetr[i].ivtx[(j + 1) & 3]] +
					      m_pVtx[m_pTetr[i].ivtx[(j + 2) & 3]] +
					      m_pVtx[m_pTetr[i].ivtx[(j + 3) & 3]]) *
					     (1.0f / 3);
					g_Faces[nFaces].r0 = pt - GetTetrCenter(i);
					g_Faces[nFaces].r1 = pt - GetTetrCenter(m_pTetr[i].ibuddy[j]);
					Pn = pTet0->Minv + pTet1->Minv;
					g_Faces[nFaces].vKinv.SetZero();
					g_Faces[nFaces].vKinv(0, 0) = Pn;
					g_Faces[nFaces].vKinv(1, 1) = Pn;
					g_Faces[nFaces].vKinv(2, 2) = Pn;
					crossproduct_matrix(g_Faces[nFaces].r0, rmtx);
					g_Faces[nFaces].vKinv -= rmtx * pTet0->Iinv * rmtx;
					crossproduct_matrix(g_Faces[nFaces].r1, rmtx);
					g_Faces[nFaces].vKinv -= rmtx * pTet1->Iinv * rmtx;
					g_Faces[nFaces].vKinv.Invert();
					(g_Faces[nFaces].wKinv = pTet0->Iinv + pTet1->Iinv).Invert();
					g_Faces[nFaces].rv = pTet1->dP + (pTet1->dL ^ g_Faces[nFaces].r1) - pTet0->dP -
					                     (pTet0->dL ^ g_Faces[nFaces].r0);
					g_Faces[nFaces].rw = g_Faces[nFaces].pTet[1]->dL - g_Faces[nFaces].pTet[0]->dL;
					g_Faces[nFaces].dP = g_Faces[nFaces].vKinv * g_Faces[nFaces].rv;
					g_Faces[nFaces].dL = g_Faces[nFaces].wKinv * g_Faces[nFaces].rw;
					r2 += g_Faces[nFaces].dP * g_Faces[nFaces].rv +
					      g_Faces[nFaces].dL * g_Faces[nFaces].rw;
					vmax = max(vmax, max(g_Faces[nFaces].rv.len2(),
					                     g_Faces[nFaces].rw.len2() * g_Faces[nFaces].r0.len2()));
					g_Faces[nFaces].P.zero();
					g_Faces[nFaces].L.zero();
					g_Faces[nFaces++].flags = 0;
				}
			}
		}
	}
	iter = min(maxIters / max(1, nFaces), nFaces * 6);

	if (vmax > sqr(e))
	{
		do
		{
			for (i = 0; i < nTets; i++)
			{
				g_Tets[i].dP.zero();
				g_Tets[i].dL.zero();
			}
			for (i = 0; i < nFaces; i++)
			{
				g_Faces[i].pTet[0]->dP += g_Faces[i].dP;
				g_Faces[i].pTet[0]->dL += (g_Faces[i].r0 ^ g_Faces[i].dP) + g_Faces[i].dL;
				g_Faces[i].pTet[1]->dP -= g_Faces[i].dP;
				g_Faces[i].pTet[1]->dL -= (g_Faces[i].r1 ^ g_Faces[i].dP) + g_Faces[i].dL;
			}
			for (i = 0; i < nFaces; i++)
			{
				pTet0 = g_Faces[i].pTet[0];
				pTet1 = g_Faces[i].pTet[1];
				dw0 = pTet0->Iinv * pTet0->dL;
				dw1 = pTet1->Iinv * pTet1->dL;
				g_Faces[i].dw = dw0 - dw1;
				g_Faces[i].dv = pTet0->dP * pTet0->Minv + (dw0 ^ g_Faces[i].r0);
				g_Faces[i].dv -= pTet1->dP * pTet1->Minv + (dw1 ^ g_Faces[i].r1);
			}

			pAp = 0;
			for (i = 0; i < nFaces; i++)
			{
				pAp += g_Faces[i].dw * g_Faces[i].dL + g_Faces[i].dv * g_Faces[i].dP;
			}

			a = min((real)50.0, r2 / max((real)1E-10, pAp));
			r2new = 0;
			for (i = 0; i < nFaces; i++)
			{
				g_Faces[i].dv = g_Faces[i].vKinv * (g_Faces[i].rv -= g_Faces[i].dv * a);
				g_Faces[i].dw = g_Faces[i].wKinv * (g_Faces[i].rw -= g_Faces[i].dw * a);
				r2new += g_Faces[i].dv * g_Faces[i].rv + g_Faces[i].dw * g_Faces[i].rw;
				g_Faces[i].P += g_Faces[i].dP * a;
				g_Faces[i].L += g_Faces[i].dL * a;
			}
			b = r2new / r2;
			r2 = r2new;
			for (i = 0, vmax = 0; i < nFaces; i++)
			{
				(g_Faces[i].dP *= b) += g_Faces[i].dv;
				(g_Faces[i].dL *= b) += g_Faces[i].dw;
				vmax =
				    max(vmax, max(g_Faces[i].rv.len2(), g_Faces[i].rw.len2() * g_Faces[i].r0.len2()));
			}
		}
		while (--iter && vmax > sqr(e));
	}

	for (j = 0, i = -1, tensMax.set(1 - bLogTension, 1); j < nFaces; j++)
	{
		n = (m_pVtx[m_pTetr[g_Faces[j].itet].ivtx[(g_Faces[j].iface + 2) & 3]] -
		         m_pVtx[m_pTetr[g_Faces[j].itet].ivtx[(g_Faces[j].iface + 1) & 3]] ^
		     m_pVtx[m_pTetr[g_Faces[j].itet].ivtx[(g_Faces[j].iface + 3) & 3]] -
		         m_pVtx[m_pTetr[g_Faces[j].itet].ivtx[(g_Faces[j].iface + 1) & 3]]) *
		    ((g_Faces[j].iface * 2 & 2) - 1);
		Pn = g_Faces[j].P * n;
		Ln = g_Faces[j].L * n;
		area2 = sqr(m_pTetr[g_Faces[j].itet].fracFace[g_Faces[j].iface]) * (nlen2 = n.len2()) * t2;
		tens = max(
		    max(max(max(quotientf(sqr_signed(Pn), area2 * sqr(m_maxForcePull)),
		                quotientf(sqr_signed(-Pn), area2 * sqr(m_maxForcePush))),
		            quotientf((g_Faces[j].P * nlen2 - n * Pn).len2(), area2 * nlen2 * sqr(m_maxForceShift))),
		        quotientf(sqr(Ln), area2 * sqr(m_maxTorqueTwist))),
		    quotientf((g_Faces[j].L * nlen2 - n * Ln).len2(), area2 * nlen2 * sqr(m_maxTorqueBend)));
		if (tens > tensMax)
		{
			tensMax = tens;
			i = j;
			pt = n;
		}
	}
	for (j = 0; j < m_nTetr; j++)
	{
		m_pTetr[j].idx = -1;
	}
	if (bLogTension && i >= 0)
	{
		n = pt;
		Pn = g_Faces[i].P * n;
		Ln = g_Faces[i].L * n;
		area2 = sqr(m_pTetr[g_Faces[i].itet].fracFace[g_Faces[i].iface]) * (nlen2 = n.len2()) * t2;
		if (quotientf(sqr_signed(Pn), area2 * sqr(m_maxForcePull)) >= tensMax)
		{
			m_maxTension = Pn / sqrt_tpl(area2);
			m_imaxTension = LPull;
		}
		else if (quotientf(sqr_signed(-Pn), area2 * sqr(m_maxForcePush)) >= tensMax)
		{
			m_maxTension = -Pn / sqrt_tpl(area2);
			m_imaxTension = LPush;
		}
		else if (quotientf((g_Faces[i].P * nlen2 - n * Pn).len2(), area2 * nlen2 * sqr(m_maxForceShift)) >=
		         tensMax)
		{
			m_maxTension = (g_Faces[i].P * nlen2 - n * Pn).len() / sqrt_tpl(area2 * nlen2);
			m_imaxTension = LShift;
		}
		else if (quotientf(sqr(Ln), area2 * sqr(m_maxTorqueTwist)) >= tensMax)
		{
			m_maxTension = Ln / sqrt_tpl(area2);
			m_imaxTension = LTwist;
		}
		else
		{
			m_maxTension = (g_Faces[i].L * nlen2 - n * Ln).len() / sqrt_tpl(area2 * nlen2);
			m_imaxTension = LBend;
		}
		if (tensMax < 1)
		{
			i = -1;
		}
	}
	if (i < 0)
	{
		return 0;
	}

	int itet, itet0, itet1, iface, iface0, iface1, idxsum, idxedge, idxvtx, idxface, ihead, itail,
	    nCracks = 0, tqueue[32], fqueue[32];
	geom_world_data gwd[2];
	Vec3 n0, vtx[3];
	IGeometry* pCrack;
	g_Faces[i].flags |= lface_processed;
	tqueue[0] = g_Faces[i].itet;
	fqueue[0] = g_Faces[i].iface;
	ihead = -1;
	itail = 0;

	do
	{
		// get a face from the queue and generate a crack for it
		ihead = (ihead + 1) & ((sizeof(tqueue) / sizeof(tqueue[0])) - 1);
		itet0 = tqueue[ihead];
		iface0 = fqueue[ihead];
		itet1 = m_pTetr[itet0].ibuddy[iface0];

		for (j = 0; j < 3; j++)
		{
			vtx[j] = m_pVtx[m_pTetr[itet0].ivtx[(iface0 + 1 + j) & 3]];
		}
		pCrack = m_pWorld->GetGeomManager()->GetCrackGeom(vtx, m_idmat, gwd + 1);
		if (pCrack)
		{
			if (m_pMesh->Subtract(pCrack, gwd, gwd + 1))
			{
				// Subtract(pCrack, gwd,gwd+1);
				m_pTetr[itet0].fracFace[iface0] = 0;
				itet1 = m_pTetr[itet0].ibuddy[iface0];
				m_pTetr[itet1].fracFace[GetFaceByBuddy(itet1, itet0)] = 0;
			}
			else
			{
				m_pTetr[itet0].fracFace[iface0] *= 1.5f;
				m_pTetr[itet1].fracFace[GetFaceByBuddy(m_pTetr[itet0].ibuddy[iface0], itet0)] *= 1.5f;
				continue;
			}
			pCrack->Release();
		}
		else
		{
			continue;
		}

		n0 = (vtx[1] - vtx[0] ^ vtx[2] - vtx[0]) * ((iface0 * 2 & 2) - 1);
		for (j = idxsum = 0; j < 3; j++)
		{
			idxsum += m_pTetr[itet0].ivtx[(iface0 + 1 + j) & 3];
		}
		// do not quit now if the crack limit is reached, for we still want to weaken the neighbourhood

		for (j = 0; j < 3; j++)
		{ // trace fins around the face's edges
			itet = itet0;
			iface = (iface0 + 1 + j) & 3;
			idxedge = idxsum - m_pTetr[itet0].ivtx[iface];
			do
			{
				itet1 = itet;
				iface1 = iface;
				if ((itet = m_pTetr[itet].ibuddy[iface]) < 0 || m_pTetr[itet].flags & ltet_processed)
				{
					break;
				}
				iface = GetFaceByBuddy(itet, itet1);
				idxface = m_pTetr[itet].idxface[iface];
				if (idxface < 0 || g_Faces[idxface].flags & lface_processed)
				{
					break;
				}
				g_Faces[idxface].flags |= lface_processed;
				n = (m_pVtx[m_pTetr[itet].ivtx[(iface + 2) & 3]] -
				         m_pVtx[m_pTetr[itet].ivtx[(iface + 1) & 3]] ^
				     m_pVtx[m_pTetr[itet].ivtx[(iface + 3) & 3]] -
				         m_pVtx[m_pTetr[itet].ivtx[(iface + 1) & 3]]) *
				    ((iface * 2 & 2) - 1);
				if (sqr_signed(n * n0) > sqr(0.75f) * n.len2() * n0.len2())
				{
					m_pTetr[itet].fracFace[iface] *= m_crackWeaken;
					m_pTetr[itet1].fracFace[iface1] *= m_crackWeaken;
					Pn = g_Faces[idxface].P * n;
					Ln = g_Faces[idxface].L * n;
					area2 = sqr(m_pTetr[itet].fracFace[iface]);
					if (sqr_signed(Pn) * t2 > area2 * sqr(m_maxForcePull) ||
					    sqr_signed(-Pn) * t2 > area2 * sqr(m_maxForcePush) ||
					    (g_Faces[idxface].P - n * Pn).len2() * t2 > area2 * sqr(m_maxForcePush) ||
					    sqr(Ln) * t2 > area2 * sqr(m_maxTorqueTwist) ||
					    (g_Faces[idxface].L - n * Ln).len2() * t2 > area2 * sqr(m_maxTorqueBend))
					{ // put the to-be-cracked face into the queue
						itail = (itail + 1) & ((sizeof(tqueue) / sizeof(tqueue[0])) - 1);
						tqueue[itail] = itet;
						fqueue[itail] = iface;
						break;
					}
				}
				idxvtx = m_pTetr[itet].ivtx[(iface + 1) & 3] + m_pTetr[itet].ivtx[(iface + 2) & 3] +
				         m_pTetr[itet].ivtx[(iface + 3) & 3] - idxedge;
				idxvtx = iszero(idxvtx - m_pTetr[itet].ivtx[(iface + 2) & 3]) +
				         (2 & -iszero(idxvtx - m_pTetr[itet].ivtx[(iface + 3) & 3]));
				iface = (iface + 1 + idxvtx) & 3;
			}
			while (true);
		}
	}
	while (ihead != itail && ++nCracks < m_nMaxCracks);

	return 1;
}

void CTetrLattice::Split(CTriMesh** pChunks, int nChunks, CTetrLattice** pLattices)
{
	int ichunk, i, j, ibuddy, idx, idx1, nNewTet, nNewVtx, ivtx0, itet0, itet, nCells;
	Vec3 pos, sz;
	primitives::box bboxLoc;
	Matrix33 R;
	Vec3i iBBox[2], ic;
	STetrahedron* ptet;
	CTetrLattice* plat;
	if (!m_pVtxRemap)
	{
		m_pVtxRemap = new int[m_nVtx];
	}

	for (ichunk = 0; ichunk < nChunks; ichunk++)
	{
		pChunks[ichunk]->GetBBox(&bboxLoc);
		R = bboxLoc.Basis * m_RGrid;
		sz = bboxLoc.size * R.Fabs();
		pos = (bboxLoc.center - m_posGrid) * m_RGrid;
		for (i = 0; i < 3; i++)
		{
			iBBox[0][i] =
			    min(m_szGrid[i], max(0, physics_float2int(((pos[i] - sz[i]) * m_rstepGrid[i]) - 0.5f)));
			iBBox[1][i] =
			    min(m_szGrid[i], max(0, physics_float2int(((pos[i] + sz[i]) * m_rstepGrid[i]) + 0.5f)));
		}
		nNewVtx = nNewTet = 0;
		itet0 = ivtx0 = -1;

		for (ic.x = iBBox[0].x; ic.x < iBBox[1].x; ic.x++)
		{
			for (ic.y = iBBox[0].y; ic.y < iBBox[1].y; ic.y++)
			{
				for (ic.z = iBBox[0].z; ic.z < iBBox[1].z; ic.z++)
				{
					for (idx = m_pGridTet0[ic * m_strideGrid];
					     idx < m_pGridTet0[ic * m_strideGrid + 1]; idx++)
					{
						if (!((ptet = m_pTetr + m_pGrid[idx])->flags &
						      (ltet_removed | ltet_processed)))
						{
							sz = bboxLoc.Basis *
							     (GetTetrCenter(m_pGrid[idx]) - bboxLoc.center);
							if (max(max(fabs_tpl(sz.x) - bboxLoc.size.x,
							            fabs_tpl(sz.y) - bboxLoc.size.y),
							        fabs_tpl(sz.z) - bboxLoc.size.z) < 0 &&
							    pChunks[ichunk]->PointInsideStatus(
								GetTetrCenter(m_pGrid[idx])))
							{ // move the tets that have their centers inside the chunk geom
							  // to that chunk's lattice
								ptet->idx = nNewTet++;
								ptet->flags |= ltet_removed_new;
								for (i = 0; i < 4; i++)
								{
									if (!(m_pVtxFlags[ptet->ivtx[i]] &
									      lvtx_processed))
									{
										m_pVtxFlags[ptet->ivtx[i]] =
										    (m_pVtxFlags[ptet->ivtx[i]] &
										     ~(-1 << lvtx_inext_log2)) |
										    ivtx0 << lvtx_inext_log2 |
										    lvtx_processed;
										ivtx0 =
										    ptet->ivtx[i]; // maintain a linked
										                   // list of processed
										                   // vertices
										m_pVtxRemap[ptet->ivtx[i]] = nNewVtx++;
									}
								}
							}
							ptet->flags = (ptet->flags & ~(-1 << ltet_inext_log2)) |
							              itet0 << ltet_inext_log2 |
							              ltet_processed; // link processed tets into a list
							itet0 = m_pGrid[idx];
						}
					}
				}
			}
		}

		if (nNewTet > 1 && nNewVtx)
		{
			pLattices[ichunk] = plat = new CTetrLattice(this, 0);
			plat->m_pMesh = pChunks[ichunk];
			plat->m_pVtx = new Vec3[plat->m_nVtx = nNewVtx];
			plat->m_pVtxFlags = new int[plat->m_nVtx];
			plat->m_pTetr = new STetrahedron[plat->m_nTetr = nNewTet];

			for (i = 0, idx = ivtx0; idx != -1; idx = m_pVtxFlags[idx] >> lvtx_inext_log2, i++)
			{
				plat->m_pVtx[i] = m_pVtx[idx];
				plat->m_pVtxFlags[i] = m_pVtxFlags[idx] & ~lvtx_processed;
			}
			for (i = 0, itet = itet0; itet != -1; itet = m_pTetr[itet].flags >> ltet_inext_log2)
			{
				if (m_pTetr[itet].flags & ltet_removed_new)
				{
					plat->m_pTetr[i].flags = 0;
					plat->m_pTetr[i].idx = -1;
					plat->m_pTetr[i].M = m_pTetr[itet].M;
					plat->m_pTetr[i].Minv = m_pTetr[itet].Minv;
					plat->m_pTetr[i].Vinv = m_pTetr[itet].Vinv;
					plat->m_pTetr[i].Iinv = m_pTetr[itet].Iinv;
					plat->m_pTetr[i].area = m_pTetr[itet].area;
					for (j = 0; j < 4; j++)
					{
						plat->m_pTetr[i].fracFace[j] = m_pTetr[itet].fracFace[j];
						plat->m_pTetr[i].ivtx[j] =
						    nNewVtx - 1 - m_pVtxRemap[m_pTetr[itet].ivtx[j]];
						plat->m_pTetr[i].ibuddy[j] = -1;
						if ((ibuddy = m_pTetr[itet].ibuddy[j]) >= 0)
						{
							if (m_pTetr[ibuddy].idx >= 0)
							{
								plat->m_pTetr[i].ibuddy[j] =
								    nNewTet - 1 - m_pTetr[ibuddy].idx;
							}
							else
							{
								m_pTetr[ibuddy].ibuddy[GetFaceByBuddy(ibuddy, itet)] =
								    -1;
							}
						}
					}
					i++;
				}
			}
			if (i != nNewTet)
			{
				pLattices[ichunk] = 0;
			}
			else
			{
				plat->m_szGrid = iBBox[1] - iBBox[0];
				plat->m_strideGrid.Set(plat->m_szGrid.z * plat->m_szGrid.y, plat->m_szGrid.z, 1);
				plat->m_posGrid += m_RGrid * Vec3(iBBox[0].x * m_stepGrid.x, iBBox[0].y * m_stepGrid.y,
				                                  iBBox[0].z * m_stepGrid.z);
				plat->m_pGridTet0 = new int[plat->m_szGrid.GetVolume() + 1];
				idx1 = nCells = 0;
				for (ic.x = iBBox[0].x; ic.x < iBBox[1].x; ic.x++)
				{
					for (ic.y = iBBox[0].y; ic.y < iBBox[1].y; ic.y++)
					{
						for (ic.z = iBBox[0].z; ic.z < iBBox[1].z; ic.z++)
						{
							for (idx = m_pGridTet0[ic * m_strideGrid];
							     idx < m_pGridTet0[ic * m_strideGrid + 1]; idx++)
							{
								nCells +=
								    m_pTetr[m_pGrid[idx]].flags >> 1 & ltet_removed;
							}
						}
					}
				}
				plat->m_pGrid = new int[nCells];
				for (ic.x = iBBox[0].x; ic.x < iBBox[1].x; ic.x++)
				{
					for (ic.y = iBBox[0].y; ic.y < iBBox[1].y; ic.y++)
					{
						for (ic.z = iBBox[0].z; ic.z < iBBox[1].z; ic.z++)
						{
							for (idx = m_pGridTet0[ic * m_strideGrid],
							    plat->m_pGridTet0[(ic - iBBox[0]) * plat->m_strideGrid] =
							         idx1;
							     idx < m_pGridTet0[ic * m_strideGrid + 1]; idx++)
							{
								if (m_pTetr[m_pGrid[idx]].flags & ltet_removed_new)
								{
									plat->m_pGrid[idx1++] =
									    nNewTet - 1 - m_pTetr[m_pGrid[idx]].idx;
								}
							}
						}
					}
				}
				plat->m_pGridTet0[plat->m_szGrid.GetVolume()] = idx1;
			}
		}
		else
		{
			pLattices[ichunk] = 0;
		}

		for (; itet0 != -1; itet0 = m_pTetr[itet0].flags >> ltet_inext_log2)
		{
			m_pTetr[itet0].flags = (m_pTetr[itet0].flags & ~(ltet_processed | ltet_removed_new)) |
			                       (m_pTetr[itet0].flags >> 1 & ltet_removed);
			m_pTetr[itet0].idx = -1;
		}
		for (; ivtx0 != -1; ivtx0 = m_pVtxFlags[ivtx0] >> lvtx_inext_log2)
		{
			m_pVtxFlags[ivtx0] &= ~lvtx_processed;
		}
	}

	Defragment();
}

int CTetrLattice::AddImpulse(const Vec3& pt, const Vec3& impulse, const Vec3& momentum, const Vec3& gravity,
                             float worldTime)
{
	int i, bCheckStructure = 0;
	Vec3 ptloc;
	Vec3i ic;
	float vgravity = gravity.len2() * sqr(0.01);
	STetrahedron* ptet;

	if (worldTime != m_lastImpulseTime)
	{
		for (i = 0; i < m_nTetr; i++)
		{
			m_pTetr[i].Pext.zero(), m_pTetr[i].Lext.zero();
		}
		m_lastImpulseTime = worldTime;
	}

	ptloc = (pt - m_posGrid) * m_RGrid;
	for (i = 0; i < 3; i++)
	{
		ic[i] = min(m_szGrid[i] - 1, max(0, physics_float2int((ptloc[i] * m_rstepGrid[i]) - 0.5f)));
	}

	for (i = m_pGridTet0[ic * m_strideGrid]; i < m_pGridTet0[ic * m_strideGrid + 1]; i++)
	{
		if (!((ptet = m_pTetr + m_pGrid[i])->flags & ltet_removed))
		{
			ptet->Pext += impulse;
			ptet->Lext += momentum + (pt - GetTetrCenter(m_pGrid[i]) ^ impulse);
			bCheckStructure |= isneg(vgravity - max((ptet->Pext * ptet->Minv).len2(),
			                                        (ptet->Iinv * ptet->Lext).len2() * ptet->area));
		}
	}

	return bCheckStructure;
}

int CTetrLattice::GetFaceByBuddy(int itet, int itetBuddy)
{
	int i, ibuddy = 0, imask;
	for (i = 1; i < 4; i++)
	{
		imask = -iszero(m_pTetr[itet].ibuddy[i] - itetBuddy);
		ibuddy = (ibuddy & ~imask) | (i & imask);
	}
	return ibuddy;
}

Vec3 CTetrLattice::GetTetrCenter(int i)
{
	return (m_pVtx[m_pTetr[i].ivtx[0]] + m_pVtx[m_pTetr[i].ivtx[1]] + m_pVtx[m_pTetr[i].ivtx[2]] +
	        m_pVtx[m_pTetr[i].ivtx[3]]) *
	       0.25f;
}

int CTetrLattice::Defragment()
{
	if (m_nRemovedTets * 10 > m_nTetr * 7)
	{
		int i, j, nTetr0 = m_nTetr, ngrid, ngrid0;
		for (i = ngrid = 0; i < m_szGrid.GetVolume(); i++)
		{
			for (j = m_pGridTet0[i], ngrid0 = ngrid; j < m_pGridTet0[i + 1]; j++)
			{
				if (!(m_pTetr[m_pGrid[j]].flags & ltet_removed))
				{
					m_pGrid[ngrid++] = m_pGrid[j];
				}
			}
			m_pGridTet0[i] = ngrid0;
		}
		m_pGridTet0[i] = ngrid;

		for (i = j = 0; i < m_nTetr; i++)
		{
			if (!(m_pTetr[i].flags & ltet_removed))
			{
				m_pTetr[i].idx = j++;
			}
		}
		for (i = 0; i < m_nTetr; i++)
		{
			for (j = 0; j < 4; j++)
			{
				if (m_pTetr[i].ibuddy[j] >= 0)
				{
					m_pTetr[i].ibuddy[j] = m_pTetr[m_pTetr[i].ibuddy[j]].idx;
				}
			}
		}
		for (i = m_pGridTet0[m_szGrid.GetVolume()] - 1; i >= 0; i--)
		{
			m_pGrid[i] = m_pTetr[m_pGrid[i]].idx;
		}
		for (i = j = 0; i < m_nTetr; i++)
		{
			if (!(m_pTetr[i].flags & ltet_removed))
			{
				if (i != j)
				{
					m_pTetr[j] = m_pTetr[i];
				}
				j++;
			}
		}
		m_nTetr = j;
		for (i = 0; i < m_nTetr; i++)
		{
			m_pTetr[i].idx = -1;
		}

		ReallocateList(m_pTetr, nTetr0, m_nTetr);
		m_nRemovedTets = 0;
		return 1;
	}
	return 0;
}

void CTetrLattice::DrawWireframe(IPhysRenderer* pRenderer, geom_world_data* gwd, int idxColor)
{
	Vec3 pt[5];
	int i, j, i0, i1, i2;

	for (i = 0; i < m_nTetr; i++)
	{
		if (!(m_pTetr[i].flags & ltet_removed))
		{
			for (j = 0; j < 4; j++)
			{
				pt[j] = gwd->offset + gwd->R * m_pVtx[m_pTetr[i].ivtx[j]] * gwd->scale;
			}
			for (j = 0; j < 6; j++)
			{
				i2 = j >> 2;
				i0 = (j >> 1) + i2;
				i1 = (i0 + 1 + ((j & 1) << i2)) & 3;
				pRenderer->DrawLine(pt[i0], pt[i1], idxColor);
			}
			for (j = 0; j < 4; j++)
			{
				if (m_pTetr[i].fracFace[j] <= 0)
				{
					pt[4] = (pt[(j + 1) & 3] + pt[(j + 2) & 3] + pt[(j + 3) & 3]) * (1.0f / 3);
					for (i0 = 1; i0 < 4; i0++)
					{
						pRenderer->DrawLine(pt[(j + i0) & 3],
						                    pt[4] * 0.85f + pt[(j + i0) & 3] * 0.15f, idxColor);
					}
				}
			}
		}
	}
}
