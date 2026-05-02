#pragma once

#include "CryCommon/CryPhysics/IPhysics.h"

class CBreakableGrid2d final : public IBreakableGrid2d
{
public:
	CBreakableGrid2d()
	{
		m_pt = 0;
		m_pTris = 0;
		m_pCellDiv = 0;
		m_nTris = 0;
	}

	~CBreakableGrid2d()
	{
		if (m_pt)
		{
			delete[] m_pt;
		}
		if (m_pTris)
		{
			delete[] m_pTris;
		}
		if (m_pCellDiv)
		{
			delete[] m_pCellDiv;
		}
	}

	void Generate(vector2df* ptsrc, int npt, const vector2di& nCells, int bStaticBorder, int seed = -1);

	int* BreakIntoChunks(const vector2df& pt, float r, vector2df*& ptout, int maxPatchTris, float jointhresh,
	                     int seed = -1) override;

	primitives::grid* GetGridData() override { return &m_coord; }
	bool IsEmpty() override { return m_nTris == 0; }
	void Release() override { delete this; }

	void MarkCellInterior(int i);
	int get_neighb(int iTri, int iEdge);
	void get_edge_ends(int iTri, int iEdge, int& iend0, int& iend1);

	enum tritypes
	{
		TRI_AVAILABLE = 1 << 31,
		TRI_FIXED = 1 << 29,
		TRI_EMPTY = 1 << 28,
		TRI_STABLE = 1 << 27,
		TRI_PROCESSED = 1 << 26
	};

	primitives::grid m_coord;
	vector2df* m_pt;
	int* m_pTris;
	char* m_pCellDiv;
	int m_nTris;
};
