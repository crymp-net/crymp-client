#pragma once

#include "CryCommon/CrySoundSystem/IReverbManager.h"

struct IReverbInstance
{
	virtual ~IReverbInstance() = default;

	virtual bool Activate() = 0;
	virtual bool Deactivate() = 0;

	virtual bool Update(CRYSOUND_REVERB_PROPERTIES* pProps) = 0;
};
