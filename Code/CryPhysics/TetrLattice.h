#pragma once

#include "CryCommon/CryPhysics/IPhysics.h"

class CTriMesh;
struct STetrahedron;
struct SCGFace;
struct SCGTetr;

enum ltension_type
{
	LPull,
	LPush,
	LShift,
	LTwist,
	LBend
};

enum lvtx_flags
{
	lvtx_removed = 1,
	lvtx_removed_new = 2,
	lvtx_processed = 4,
	lvtx_inext_log2 = 8,
};

enum ltet_flags
{
	ltet_removed = 1,
	ltet_removed_new = 2,
	ltet_processed = 4,
	ltet_inext_log2 = 8,
};

enum lface_flags
{
	lface_processed = 1
};

class CTetrLattice final : public ITetrLattice
{
public:
	explicit CTetrLattice(IPhysicalWorld* pWorld);
	CTetrLattice(CTetrLattice* src, int bCopyData);
	~CTetrLattice();

	void Release() override { delete this; }

	CTetrLattice* CreateLattice(const Vec3* pVtx, int nVtx, const int* pTets, int nTets);
	void SetMesh(CTriMesh* pMesh);
	void SetIdMat(int id) { m_idmat = id; }

	int SetParams(pe_params* _params) override;
	int GetParams(pe_params* _params) override;

	void Subtract(IGeometry* pGeonm, const geom_world_data* pgwd1, const geom_world_data* pgwd2);
	int CheckStructure(float time_interval, const Vec3& gravity, const primitives::plane* pGround, int nPlanes,
	                   pe_explosion* pexpl, int maxIters = 100000, int bLogTension = 0);
	void Split(CTriMesh** pChunks, int nChunks, CTetrLattice** pLattices);
	int Defragment();
	void DrawWireframe(IPhysRenderer* pRenderer, geom_world_data* gwd, int idxColor) override;
	float GetLastTension(int& itype)
	{
		itype = m_imaxTension;
		return m_maxTension;
	}
	int AddImpulse(const Vec3& pt, const Vec3& impulse, const Vec3& momentum, const Vec3& gravity, float worldTime);
	int GetFaceByBuddy(int itet, int itetBuddy);
	Vec3 GetTetrCenter(int i);

	IPhysicalWorld* m_pWorld;

	CTriMesh* m_pMesh;
	Vec3* m_pVtx;
	int m_nVtx;
	STetrahedron* m_pTetr;
	int m_nTetr;
	int* m_pVtxFlags;
	int m_nMaxCracks;
	int m_idmat;
	float m_maxForcePush, m_maxForcePull, m_maxForceShift;
	float m_maxTorqueTwist, m_maxTorqueBend;
	float m_crackWeaken;
	float m_density;
	int m_nRemovedTets;
	int* m_pVtxRemap;
	int m_flags;
	float m_maxTension;
	int m_imaxTension;
	float m_lastImpulseTime;

	Matrix33 m_RGrid;
	Vec3 m_posGrid;
	Vec3 m_stepGrid, m_rstepGrid;
	Vec3i m_szGrid, m_strideGrid;
	int *m_pGridTet0, *m_pGrid;

	inline static SCGFace* g_Faces = nullptr;
	inline static SCGTetr* g_Tets = nullptr;
	inline static int g_nFacesAlloc = 0;
	inline static int g_nTetsAlloc = 0;
};
