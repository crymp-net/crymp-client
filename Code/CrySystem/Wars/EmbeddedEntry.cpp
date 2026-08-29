#include "StdAfx.h"
#include "System.h"

#include "CryCommon/CrySystem/gEnv.h"

// CryMP: CrySystem is linked directly into CryMP-Client.exe.
extern "C" ISystem* CreateSystemInterface(const SSystemInitParams& startupParams)
{
    CSystem* pSystem = new CSystem;
    ModuleInitISystem(pSystem);

    if (!pSystem->Init(startupParams))
    {
        delete pSystem;
        return nullptr;
    }

    return pSystem;
}
