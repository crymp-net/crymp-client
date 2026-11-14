#pragma once

#include "CryCommon/CrySystem/ILog.h"

#include "FMOD.h"
#include "IReverbInstance.h"

struct IAudioDevice;

class CReverbInstance_Princeton2016 final : public IReverbInstance
{

public:
	CReverbInstance_Princeton2016(IAudioDevice* pAudioDevice);
	~CReverbInstance_Princeton2016(void);

	bool Activate();
	bool Deactivate();
	bool Update(CRYSOUND_REVERB_PROPERTIES* pProps);

private:
	FMOD::System* m_pCSEX;
	FMOD::DSP* m_DSPReverb;
	FMOD_RESULT m_ExResult;
	int m_nIndex;
	FMOD_PLUGINTYPE m_PlugInType;

	void FmodErrorOutput(const char* sDescription, ILog::ELogType LogType = ILog::eMessage);
};
