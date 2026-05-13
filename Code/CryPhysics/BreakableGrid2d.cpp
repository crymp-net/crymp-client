#include "BreakableGrid2d.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "Utils.h"

int CBreakableGrid2d::get_neighb(int iTri, int iEdge)
{
	static int offsx[8] = {-1, 0, 1, 0, 0, 1, 0, -1};
	static int offsy[8] = {0, -1, 0, 1, -1, 0, 1, 0};
	static int buddytri[16] = {1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 0};

	if (iEdge == 2)
	{
		return iTri ^ 1;
	}
	int i = m_pCellDiv[iTri >> 1] << 2 | (iTri & 1) << 1 | (iEdge & 1);
	int iBuddyCell = (iTri >> 1) + offsx[i] + (offsy[i] * m_coord.size.x);
	return (iBuddyCell * 2) + buddytri[m_pCellDiv[iBuddyCell] << 3 | i];
}

void CBreakableGrid2d::get_edge_ends(int iTri, int iEdge, int& iend0, int& iend1)
{
	int i = (3 + m_pCellDiv[iTri >> 1] + ((iTri & 1) * 2) + iEdge) & 3;
	iend0 = (iTri >> 1) + ((i >> 1) * m_coord.size.x) + (i & 1 ^ i >> 1);
	i = (3 + m_pCellDiv[iTri >> 1] + ((iTri & 1) * 2) + inc_mod3[iEdge]) & 3;
	iend1 = (iTri >> 1) + ((i >> 1) * m_coord.size.x) + (i & 1 ^ i >> 1);
}

void CBreakableGrid2d::Generate(vector2df* ptsrc, int npt, const vector2di& nCells, int bStaticBorder, int seed)
{
	m_disabled = false;

	int i, j, ix, iy, ncells;
	vector2df ptmin, ptmax, step, v0, v1;
	vector2di sz;
	jgrid_checker jgc;
	jgc.ppt = 0;
	jgc.pnorms = 0;
	jgc.bMarkCenters = 0;

	if (seed != -1)
	{
		srand((unsigned int)seed);
	}

	sz.set(nCells.x + 2, nCells.y + 3);

	PhysicsVars* pVars = gEnv->pPhysicalWorld->GetPhysVars();

	const int maxBreakableGridCells = max(1, pVars->nMaxBreakableGridCells);
	const int generatedCells = sz.x * sz.y;

	// CryMP: some custom maps generate extremely large breakable grids that can
	// cause severe stalls during flood fill and chunk generation.
	if (generatedCells > maxBreakableGridCells)
	{
		CryLog("[BreakGrid] disabled huge grid: cells=%d max=%d size=(%d,%d) requested=(%d,%d)",
			generatedCells,
			maxBreakableGridCells,
			sz.x,
			sz.y,
			nCells.x,
			nCells.y
		);

		m_disabled = true;
		m_nTris = 0;
		return;
	}

	for (i = 1, ptmin = ptmax = ptsrc[0]; i < npt; i++)
	{
		ptmin = min(ptmin, ptsrc[i]);
		ptmax = max(ptmax, ptsrc[i]);
	}
	// m_coord.step = step.set((ptmax.x-ptmin.x)*(1+0.02f*rncells.x)*rncells.x,
	// (ptmax.y-ptmin.y)*(1+0.02f*rncells.y)*rncells.y);
	m_coord.step = step.set((ptmax.x - ptmin.x) / (nCells.x - 0.02f), (ptmax.y - ptmin.y) / (nCells.y - 0.02f));
	m_coord.stepr.set(1.0f / step.x, 1.0f / step.y);
	ptmin.x -= step.x * 1.51f; //(ptmax.x-ptmin.x)*rncells.x*0.01f+step.x*1.5f;
	ptmin.y -= step.y * 1.51f; //(ptmax.y-ptmin.y)*rncells.y*0.01f+step.y*1.5f;

	m_coord.Basis.SetIdentity();
	m_coord.bOriented = 0;
	*(vector2df*)&m_coord.origin.zero() = ptmin;
	m_coord.size = sz;
	m_coord.stride.set(1, sz.x);

	ncells = sz.x * sz.y;
	m_pt = new vector2df[ncells];
	m_pTris = new int[ncells * 2];
	memset(m_pCellDiv = new char[ncells], 28, ncells);
	for (i = 0; i < sz.x; i++)
	{
		m_pCellDiv[i] = m_pCellDiv[(sz.x * (sz.y - 1)) + i] = 2;
	}
	for (i = 0; i < sz.y; i++)
	{
		m_pCellDiv[i * sz.x] = 2;
	}
	jgc.pgrid = &m_coord;
	jgc.pCellMask = m_pCellDiv;
	jgc.ppt = m_pt;

	for (ix = 0; ix < sz.x; ix++)
	{
		for (iy = 0; iy < sz.y; iy++) // generate jittered grid points
		{
			m_pt[ix + (iy * sz.x)].set(((ix + physics_frand(0.8f) + 0.1f) * step.x) + ptmin.x,
			                           ((iy + physics_frand(0.8f) + 0.1f) * step.y) + ptmin.y);
		}
	}

	jgc.iedge[0] = jgc.iedge[1] = jgc.iprevcell = jgc.iedgeExit0 = -1;
	j = 2;
	for (i = 0; i < npt; i++)
	{
		jgc.org = ptsrc[i] - ptmin;
		jgc.dirn = norm(jgc.dir = ptsrc[(i + 1) & (i + 1 - npt) >> 31] - ptsrc[i]);
		Vec3 origin(jgc.org.x, jgc.org.y, 0);
		Vec3 direction(jgc.dir.x, jgc.dir.y, 0);
		DrawRayOnGrid(&m_coord, origin, direction, jgc);
	}
	if (jgc.iedge[0] != jgc.iedgeExit0)
	{
		for (i = (jgc.iedgeExit0 + 1) & 3; i != jgc.iedge[0]; i = (i + 1) & 3)
		{
			j |= 4 << i;
		}
	}
	m_pCellDiv[vector2di(jgc.iprevcell & 0xFFFF, jgc.iprevcell >> 16) * m_coord.stride] &= j;
	for (i = 0; i < ncells; i++)
	{
		if (m_pCellDiv[i] & 2 && m_pCellDiv[i] & 28)
		{
			for (j = 0; j < 4; j++)
			{
				if (m_pCellDiv[i] & 4 << j &&
				    !(m_pCellDiv[ix = i +
				                      vector2di((1 - (j & 2)) & -(j & 1), ((j & 2) - 1) & ~-(j & 1)) *
				                          m_coord.stride] &
				      2))
				{
					jgc.MarkCellInterior(ix);
				}
			}
		}
	}
	for (i = 0; i < sz.x; i++)
	{
		m_pCellDiv[i] = m_pCellDiv[(sz.x * (sz.y - 1)) + i] = 0;
	}
	for (i = 0; i < sz.y; i++)
	{
		m_pCellDiv[i * sz.x] = 0;
	}

	for (i = 0; i < ncells - sz.x - 1; i++)
	{
		j = (m_pCellDiv[i] >> 1 & 1) + ((m_pCellDiv[i + 1] >> 1 & 1) * 2) +
		    ((m_pCellDiv[i + sz.x + 1] >> 1 & 1) * 4) + ((m_pCellDiv[i + sz.x] >> 1 & 1) * 8);
		if (j == 15)
		{
			m_pCellDiv[i] = physics_rand() & 1; // randomly choose the way the cell is split
			// check is this triangulation creates valid triangles
			v0 = m_pt[i + 1 + (m_pCellDiv[i] * sz.x)];
			v1 = m_pt[i + ((m_pCellDiv[i] ^ 1) * sz.x)] - m_pt[i + m_pCellDiv[i]];
			m_pCellDiv[i] ^= isneg(sqr_signed(v0 ^ v1) - (sqr(0.2f) * len2(v0) * len2(v1)));
			v0 = m_pt[i + ((m_pCellDiv[i] ^ 1) * sz.x)] - m_pt[i + (m_pCellDiv[i] ^ 1) + sz.x];
			v1 = m_pt[i + 1 + (m_pCellDiv[i] * sz.x)] - m_pt[i + (m_pCellDiv[i] ^ 1) + sz.x];
			m_pCellDiv[i] ^= isneg(sqr_signed(v0 ^ v1) - (sqr(0.2f) * len2(v0) * len2(v1)));
			m_pTris[i * 2] = m_pTris[(i * 2) + 1] = TRI_AVAILABLE;
		}
		else if (g_bitcount[j] >= 3)
		{
			ix = 9 - (j & 3) - (3 & -(j >> 2 & 1)) - (j >> 1 & 4);
			m_pCellDiv[i] = ix & 1;
			m_pTris[(i * 2) + (ix >> 1 ^ 1)] = TRI_AVAILABLE;
			m_pTris[(i * 2) + (ix >> 1)] = TRI_FIXED;
		}
		else
		{
			m_pCellDiv[i] = 0;
			m_pTris[i * 2] = m_pTris[(i * 2) + 1] = TRI_FIXED;
		}
	}

	// mark borders
	for (ix = 0; ix < sz.x; ix++)
	{
		m_pTris[ix * 2] = m_pTris[(ix * 2) + 1] = m_pTris[(ix + sz.x * (sz.y - 1)) * 2] =
		    m_pTris[((ix + sz.x * (sz.y - 1)) * 2) + 1] = TRI_FIXED;
	}
	for (iy = 0; iy < sz.y; iy++)
	{
		m_pTris[iy * sz.x * 2] = m_pTris[(iy * sz.x * 2) + 1] = m_pTris[(iy * sz.x + sz.x - 1) * 2] =
		    m_pTris[((iy * sz.x + sz.x - 1) * 2) + 1] = TRI_FIXED;
	}
	for (i = m_nTris = 0; i < ncells * 2; i++)
	{
		m_nTris -= m_pTris[i] >> 31;
	}

	for (i = 0; i < ncells; i++)
	{
		m_pCellDiv[i] &= 1;
	}
}

int* CBreakableGrid2d::BreakIntoChunks(const vector2df& pt, float r, vector2df*& ptout, int maxPatchTris,
                                       float jointhresh, int seed)
{
	ptout = 0;

	if (m_disabled)
		return 0;

	int i, j, nPatches, nSeedTris, nPatchTris, iTri, iTriNew, nCells, ihead, itail, szQueue, iEdge, iCurPatch, nVtx,
		nStaticTris, nUsedTris, ivtx[3], nTris, bStable;
	// int iMotherPatch,bHasInclusions,iPatch;
	vector2di sz = m_coord.size;
	vector2df c, v0, v1;
	int *queue, *pSeedTris, *pPatchSeeds, *pPatchMothers, *pVtx = 0;
	ptout = m_pt;

	if (seed != -1)
	{
		srand((unsigned int)seed);
	}

	nCells = m_coord.size.x * m_coord.size.y;
	for (szQueue = 8; szQueue < nCells >> 2; szQueue <<= 1)
		;
	queue = new int[szQueue];
	pSeedTris = new int[nCells * 2];
	pPatchSeeds = new int[nCells * 2];
	pPatchMothers = new int[nCells * 2];

	// mark tris that are outside of the shattering region as static
	j = maxPatchTris > 0 ? TRI_AVAILABLE : TRI_EMPTY;
	for (i = nStaticTris = 0; i < nCells * 2; i++)
	{
		if ((m_pTris[i] & (TRI_EMPTY | TRI_FIXED)) == 0)
		{
			get_edge_ends(i, 0, ivtx[0], ivtx[1]);
			get_edge_ends(i, 1, ivtx[1], ivtx[2]);
			c = (m_pt[ivtx[0]] + m_pt[ivtx[1]] + m_pt[ivtx[2]]) * (1.0f / 3);
			if (len2(c - pt) > sqr(r))
			{
				m_pTris[i] = TRI_STABLE;
				nStaticTris++;
			}
			else
			{
				m_pTris[i] = j;
			}
		}
	}

	// unite grid triangles into patches by randomly growing them
	nUsedTris = nPatches = 0;
	if (maxPatchTris > 0)
	{
		for (; nUsedTris + nStaticTris < m_nTris;)
		{
			for (i = 0; i < nCells * 2 && m_pTris[i] >= 0; i++)
				;
			m_pTris[pPatchSeeds[nPatches] = queue[0] = i] = nPatches;
			itail = 0;
			ihead = 1;
			nUsedTris++;
			nSeedTris = 0;

			do
			{
				nPatchTris =
				    physics_float2int(physics_frand(maxPatchTris - 1) -
				                      0.5f); // request random number of triangles for this patch
				for (i = 0; i < 3; i++)
				{
					if (m_pTris[iTri = get_neighb(queue[itail], i)] == TRI_AVAILABLE)
					{
						m_pTris[pSeedTris[nSeedTris++] = iTri] |=
						    TRI_PROCESSED; // immediately queue neighbours as potential
						                   // subsequent patch seeds
					}
				}
				if (nPatchTris > 0)
				{
					do
					{ // grow iCurPatch around a seed triangle
						iTri = queue[itail];
						itail = (itail + 1) & (szQueue - 1);
						if (m_pTris[iTri] & TRI_AVAILABLE)
						{
							if (physics_frand(1) > jointhresh)
							{
								m_pTris[iTri] = nPatches;
								nPatchTris--;
								nUsedTris++;
								pPatchSeeds[nPatches] = iTri;
							}
							else if (m_pTris[iTri] == TRI_AVAILABLE)
							{
								m_pTris[pSeedTris[nSeedTris++] = iTri] |= TRI_PROCESSED;
							}
						}
						if (m_pTris[iTri] == nPatches)
						{
							for (i = 0; i < 3; i++)
							{
								if (m_pTris[j = get_neighb(iTri, i)] & TRI_AVAILABLE)
								{
									queue[ihead] = j;
									ihead = (ihead + 1) & (szQueue - 1);
								}
							}
						}
					}
					while (nPatchTris > 0 && ihead != itail);
				}

				pPatchMothers[nPatches++] = -1;
				for (i = j = 0; i < nSeedTris; i++)
				{
					if (m_pTris[pSeedTris[i]] & TRI_AVAILABLE)
					{
						pSeedTris[j++] = pSeedTris[i];
					}
				}
				for (; ihead != itail; itail = (itail + 1) & (szQueue - 1))
				{
					if (m_pTris[queue[itail]] == TRI_AVAILABLE)
					{
						m_pTris[pSeedTris[j++] = queue[itail]] |= TRI_PROCESSED;
					}
				}
				if ((nSeedTris = j) == 0) // all triangles are assigned to patches
				{
					break;
				}
				m_pTris[pPatchSeeds[nPatches] = queue[0] = pSeedTris[0]] = nPatches;
				itail = 0;
				ihead = 1;
				nUsedTris++;
			}
			while (true);
		}
	}

	// find isolated island in the static part and register them as patches
	for (i = 0; i < nCells * 2; i++)
	{
		if (m_pTris[i] == TRI_STABLE)
		{
			m_pTris[pSeedTris[0] = queue[0] = i] |= TRI_PROCESSED;
			itail = 0;
			ihead = 1;
			nTris = 1;
			bStable = 0;
			do
			{
				iTri = queue[itail];
				itail = (itail + 1) & (szQueue - 1);
				for (j = 0; j < 3; j++)
				{
					if (m_pTris[iTriNew = get_neighb(iTri, j)] == TRI_STABLE)
					{
						m_pTris[pSeedTris[nTris++] = queue[ihead] = iTriNew] |= TRI_PROCESSED;
						ihead = (ihead + 1) & (szQueue - 1);
					}
					bStable |= m_pTris[iTriNew] & TRI_FIXED;
				}
			}
			while (ihead != itail);
			if (!bStable)
			{
				for (j = 0; j < nTris; j++)
				{
					m_pTris[pSeedTris[j]] = nPatches;
				}
				pPatchSeeds[nPatches] = i;
				pPatchMothers[nPatches++] = -2;
				nUsedTris += nTris;
			}
		}
	}

	// iteratively chop off thin ears from the patches
	for (iCurPatch = nVtx = 0; iCurPatch < nPatches; iCurPatch++)
	{
		m_pTris[queue[0] = pPatchSeeds[iCurPatch]] |= TRI_PROCESSED;
		itail = 0;
		ihead = 1;
		do
		{
			iTri = queue[itail];
			itail = (itail + 1) & (szQueue - 1);
			if (m_pTris[iTri] != (iCurPatch | TRI_PROCESSED))
			{
				continue;
			}
			for (i = 0; i < 3; i++)
			{
				if (m_pTris[j = get_neighb(iTri, i)] == iCurPatch)
				{
					queue[ihead] = j;
					ihead = (ihead + 1) & (szQueue - 1);
					m_pTris[j] |= TRI_PROCESSED;
				}
			}
			do
			{
				for (i = iEdge = 0; i < 3; i++)
				{
					iEdge |= iszero((m_pTris[get_neighb(iTri, i)] & ~TRI_PROCESSED) - iCurPatch)
					         << i;
				}
				if (g_bitcount[iEdge] == 1)
				{
					i = (iEdge & 7) - 1 - (iEdge >> 2 & 1);
					get_edge_ends(iTri, i, ivtx[0], ivtx[1]);
					get_edge_ends(iTri, inc_mod3[i], ivtx[1], ivtx[2]);
					v0 = m_pt[ivtx[0]] - m_pt[ivtx[2]];
					v1 = m_pt[ivtx[1]] - m_pt[ivtx[2]];
					if (sqr_signed(v0 ^ v1) < len2(v0) * len2(v1) * sqr(0.5f))
					{
						m_pTris[iTri] = TRI_EMPTY;
						iTri = pPatchSeeds[iCurPatch] = get_neighb(iTri, i);
					}
					else
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
			while (true);
		}
		while (ihead != itail);
	}

	// output all generated patches
	for (iCurPatch = nVtx = 0; iCurPatch < nPatches; iCurPatch++)
	{
		if (m_pTris[pPatchSeeds[iCurPatch]] != (iCurPatch | TRI_PROCESSED))
		{
			continue;
		}
		m_pTris[queue[0] = pPatchSeeds[iCurPatch]] &= ~TRI_PROCESSED;
		itail = 0;
		ihead = 1;
		do
		{ // grow iCurPatch around a seed triangle
			iTri = queue[itail];
			itail = (itail + 1) & (szQueue - 1);
			if ((nVtx - 1 & 31) + 4 >= 32)
			{
				ReallocateList(pVtx, nVtx, (nVtx + 3 & ~31) + 32);
			}
			get_edge_ends(iTri, 0, pVtx[nVtx], pVtx[nVtx + 1]); // output the points
			get_edge_ends(iTri, 1, pVtx[nVtx + 1], pVtx[nVtx + 2]);
			for (i = iEdge = 0; i < 3; i++)
			{
				if ((m_pTris[j = get_neighb(iTri, i)] & ~TRI_PROCESSED) == iCurPatch)
				{
					iEdge |= 1 << i;
					if (m_pTris[j] & TRI_PROCESSED)
					{
						queue[ihead] = j;
						ihead = (ihead + 1) & (szQueue - 1);
						m_pTris[j] &= ~TRI_PROCESSED;
					}
				}
				else
				{
					iEdge |= iszero(m_pTris[j] - TRI_EMPTY) << (i + 3);
				}
			}
			if (iEdge == 0)
			{
				v0 = m_pt[pVtx[nVtx]] - m_pt[pVtx[nVtx + 2]];
				v1 = m_pt[pVtx[nVtx + 1]] - m_pt[pVtx[nVtx + 2]];
				if ((v0 ^ v1) < sqr(m_coord.step.x * 0.9f))
				{
					m_pTris[iTri] = TRI_EMPTY;
					continue;
				}
			}
			pVtx[nVtx + 3] = iEdge;
			nVtx += 4;
		}
		while (ihead != itail);
		if (!(nVtx & 31))
		{
			ReallocateList(pVtx, nVtx, nVtx + 32);
		}
		pVtx[nVtx++] = -1;
	}
	m_nTris -= nUsedTris;

	// output the remains of this grid
	for (i = 0; i < nCells * 2; i++)
	{
		if (m_pTris[i] == (TRI_STABLE | TRI_PROCESSED))
		{
			if ((nVtx - 1 & 31) + 4 >= 32)
			{
				ReallocateList(pVtx, nVtx, (nVtx + 3 & ~31) + 32);
			}
			get_edge_ends(i, 0, pVtx[nVtx], pVtx[nVtx + 1]); // output the points
			get_edge_ends(i, 1, pVtx[nVtx + 1], pVtx[nVtx + 2]);
			for (j = iEdge = 0; j < 3; j++)
			{
				iEdge |= iszero(m_pTris[get_neighb(i, j)] - (TRI_STABLE | TRI_PROCESSED)) << j;
			}
			pVtx[nVtx + 3] = iEdge;
			nVtx += 4;
		}
		else
		{
			m_pTris[i] = max(m_pTris[i], TRI_EMPTY);
		}
	}
	if (!(nVtx & 31))
	{
		ReallocateList(pVtx, nVtx, nVtx + 32);
	}
	pVtx[nVtx++] = -2;

	return pVtx;
}
