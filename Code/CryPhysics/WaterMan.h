#pragma once

#include "CryCommon/CryPhysics/IPhysics.h"

class CPhysicalEntity;
class CPhysicalWorld;
struct SWaterTile;

class CWaterMan
{
public:
	explicit CWaterMan(CPhysicalWorld* pWorld);
	~CWaterMan();

	int SetParams(pe_params* _params);
	int GetParams(pe_params* _params);
	int GetStatus(pe_status* _status);
	void OnWaterInteraction(CPhysicalEntity* pent);
	void OnWaterHit(const Vec3& pthit, const Vec3& vel);
	void SetViewerPos(const Vec3& newpos, int iCaller = 0);
	void TimeStep(float dt);
	void DrawHelpers(IPhysRenderer* pRenderer);

	CPhysicalWorld* m_pWorld;
	int m_bActive;
	float m_timeSurplus, m_dt;
	int m_nTiles, m_nCells;
	float m_tileSz, m_rtileSz;
	float m_cellSz, m_rcellSz;
	float m_waveSpeed;
	float m_dampingCenter, m_dampingRim;
	float m_minhSpread, m_minVel;
	Vec3 m_origin;
	Vec3 m_waterOrigin, m_posViewer;
	int m_ix, m_iy;
	Matrix33 m_R;
	SWaterTile **m_pTiles, **m_pTilesTmp;
	char* m_pCellMask;
	vector2df* m_pCellNorm;
	int *m_pCellQueue, m_szCellQueue;
};
