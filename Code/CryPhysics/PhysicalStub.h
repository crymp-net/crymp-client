#pragma once

#include "CryCommon/CryNetwork/SerializeFwd.h"
#include "CryCommon/CryPhysics/IPhysics.h"

class CPhysicalStub final : public IPhysicalEntity
{
public:
	CPhysicalStub() = default;

	pe_type GetType() override { return PE_STATIC; }
	int AddRef() override { return 0; }
	int Release() override { return 0; }
	int SetParams(pe_params* params, int bThreadSafe = 1) override { return 0; }
	int GetParams(pe_params* params) override { return 0; }
	int GetStatus(pe_status* status) override { return 0; }
	int Action(pe_action* action, int bThreadSafe = 1) override { return 0; }

	int AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id = -1, int bThreadSafe = 1) override
	{
		return -1;
	}

	void RemoveGeometry(int id, int bThreadSafe = 1) override {}
	float ComputeExtent(GeomQuery& geo, EGeomForm eForm) { return 0.f; }
	void GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm)
	{
		ran.vPos.zero();
		ran.vNorm.zero();
	}

	void* GetForeignData(int itype = 0) override { return nullptr; }
	int GetiForeignData() override { return 0; }

	int GetStateSnapshot(TSerialize ser, float time_back = 0, int flags = 0) override;
	int SetStateFromSnapshot(TSerialize ser, int flags = 0) override;
	int SetStateFromTypedSnapshot(TSerialize ser, int type, int flags) override;
	int PostSetStateFromSnapshot() override { return 0; }
	unsigned int GetStateChecksum() override { return 0; }

	void StartStep(float time_interval) {}
	int Step(float time_interval) { return 0; }
	int DoStep(float time_interval, int iCaller) override { return 0; }
	void StepBack(float time_interval) override {}
	IPhysicalWorld* GetWorld() override { return nullptr; }

	void GetMemoryStatistics(ICrySizer*) override {}
};
