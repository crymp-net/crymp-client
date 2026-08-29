////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek Studios, 2001-2007.
// -------------------------------------------------------------------------
//  File name:   MemoryManager.h
//  Version:     v1.00
//  Created:     27/7/2007 by Timur.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MemoryManager_h__
#define __MemoryManager_h__
#pragma once

#include "CryCommon/CrySystem/ISystem.h"

//////////////////////////////////////////////////////////////////////////
// Class that implements IMemoryManager interface.
//////////////////////////////////////////////////////////////////////////
class CCryMemoryManager : public IMemoryManager
{
	virtual bool GetProcessMemInfo( SProcessMemInfo &minfo );
};

#endif // __MemoryManager_h__