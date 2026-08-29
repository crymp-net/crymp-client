////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   dllmain.cpp
//  Version:     v1.00
//  Created:     1/10/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "System.h"
#include <platform_impl.h>
#include "DebugCallStack.h"

// For lua debugger
//#include <malloc.h>

HMODULE gDLLHandle = NULL;


#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)
BOOL APIENTRY DllMain( HANDLE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
											)
{
	gDLLHandle = (HMODULE)hModule;
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
	
		
		break;
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH: 
		break;
	}
	int sbh = _set_sbh_threshold(1016);
	return TRUE;
}
#endif

//////////////////////////////////////////////////////////////////////////
struct CSystemEventListner_System : public ISystemEventListener
{
public:
	virtual void OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
	{
		switch (event)
		{
		case ESYSTEM_EVENT_LEVEL_RELOAD:
			{
				CryCleanup();
				STLALLOCATOR_CLEANUP;
				break;
			}
		}
	}
};

static CSystemEventListner_System g_system_event_listener_system;

extern "C"
{
	CRYSYSTEM_API ISystem* CreateSystemInterface(const SSystemInitParams &startupParams)
	{
		CSystem *pSystem = NULL;

		pSystem = new CSystem;
		ModuleInitISystem(pSystem);

		
#ifndef _DEBUG
#ifdef WIN32
		// Install exception handler in Release modes.
		DebugCallStack::instance()->installErrorHandler( pSystem );
#endif
#endif

		if (!pSystem->Init(startupParams))
		{
			delete pSystem;

			return 0;
		}

		pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_system);

		return pSystem;
	}

	CRYSYSTEM_API unsigned int CryRandom()
	{
		return g_random_generator.Generate();
	}

	CRYSYSTEM_API void CryRandomSeed( unsigned int seed )
	{
		g_random_generator.seed(seed);
	}
};
