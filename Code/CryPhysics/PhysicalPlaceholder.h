#pragma once

#include "CryCommon/CryPhysics/IPhysics.h"

class CPhysicalEntity;

class CPhysicalPlaceholder : public IPhysicalEntity
{
public:
	CPhysicalPlaceholder()
	{
		m_lockUpdate = 0;
		m_iGThunk0 = 0;
		m_pEntBuddy = 0;
		m_bProcessed = 0;
		m_ig[0].x = m_ig[1].x = m_ig[0].y = m_ig[1].y = -2;
		m_pForeignData = 0;
		m_iForeignData = m_iForeignFlags = 0;
	}

	virtual ~CPhysicalPlaceholder() = default;
	virtual CPhysicalEntity* GetEntity();
	virtual CPhysicalEntity* GetEntityFast() { return (CPhysicalEntity*)m_pEntBuddy; }

	int AddRef() override { return 0; }
	int Release() override { return 0; }

	pe_type GetType() override;
	int SetParams(pe_params* params, int bThreadSafe = 1) override;
	int GetParams(pe_params* params) override;
	int GetStatus(pe_status* status) override;
	int Action(pe_action* action, int bThreadSafe = 1) override;

	int AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id = -1, int bThreadSafe = 1) override;
	void RemoveGeometry(int id, int bThreadSafe = 1) override;

	void* GetForeignData(int itype = 0) override { return m_iForeignData == itype ? m_pForeignData : 0; }
	int GetiForeignData() override { return m_iForeignData; }

	int GetStateSnapshot(TSerialize ser, float time_back = 0, int flags = 0) override;
	int SetStateFromSnapshot(TSerialize ser, int flags = 0) override;
	int SetStateFromTypedSnapshot(TSerialize ser, int type, int flags = 0) override;
	int PostSetStateFromSnapshot() override;
	unsigned int GetStateChecksum() override;

	virtual void StartStep(float time_interval);
	virtual int Step(float time_interval);
	int DoStep(float time_interval, int iCaller) override { return 1; }
	void StepBack(float time_interval) override;
	IPhysicalWorld* GetWorld() override;

	void GetMemoryStatistics(ICrySizer*) override {}

	Vec3 m_BBox[2];

	void* m_pForeignData;
	int m_iForeignData : 16;
	int m_iForeignFlags : 16;

	struct vec2dpacked
	{
		int x : 16;
		int y : 16;
	};
	vec2dpacked m_ig[2];
	int m_iGThunk0;

	CPhysicalPlaceholder* m_pEntBuddy;
	volatile unsigned int m_bProcessed;
	int m_id : 24;
	int m_iSimClass : 8;
	volatile int m_lockUpdate;
};
