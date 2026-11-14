#include "CryCommon/CryNetwork/ISerialize.h"

#include "PhysicalStub.h"

int CPhysicalStub::GetStateSnapshot(TSerialize ser, float time_back, int flags)
{
	return 0;
}

int CPhysicalStub::SetStateFromSnapshot(TSerialize ser, int flags)
{
	return 0;
}

int CPhysicalStub::SetStateFromTypedSnapshot(TSerialize ser, int type, int flags)
{
	return 0;
}
