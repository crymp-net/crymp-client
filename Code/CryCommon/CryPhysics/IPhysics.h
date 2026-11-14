// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------

#pragma once

#include "physinterface.h"

//////////////////////////////////////////////////////////////////////////
// IDs that can be used for foreign id.
//////////////////////////////////////////////////////////////////////////
enum EPhysicsForeignIds
{
	PHYS_FOREIGN_ID_TERRAIN = 0,
	PHYS_FOREIGN_ID_STATIC = 1,
	PHYS_FOREIGN_ID_ENTITY = 2,
	PHYS_FOREIGN_ID_FOLIAGE = 3,
	PHYS_FOREIGN_ID_ROPE = 4,
	PHYS_FOREIGN_ID_SOUND_OBSTRUCTION = 5,
	PHYS_FOREIGN_ID_WATERVOLUME = 6,

	PHYS_FOREIGN_ID_USER = 100, // All user defined foreign ids should start from this enum.
};
