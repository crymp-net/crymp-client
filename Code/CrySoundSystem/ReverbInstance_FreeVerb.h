#pragma once

#include "CryCommon/CrySystem/ILog.h"

#include "FMOD.h"
#include "IReverbInstance.h"

struct IAudioDevice;

class CReverbInstance_FreeVerb final : public IReverbInstance
{

public:
	CReverbInstance_FreeVerb(IAudioDevice* pAudioDevice);
	~CReverbInstance_FreeVerb(void);

	bool Activate();
	bool Deactivate();
	bool Update(CRYSOUND_REVERB_PROPERTIES* pProps);

private:
	FMOD::System* m_pCSEX;
	FMOD::DSP* m_DSPReverb;
	FMOD_RESULT m_ExResult;

	void FmodErrorOutput(const char* sDescription, ILog::ELogType LogType = ILog::eMessage);
};
