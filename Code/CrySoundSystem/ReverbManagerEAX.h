#pragma once

#include "ReverbManager.h"

#include "eax.h"

struct IAudioDevice;

class CReverbManagerEAX final : public CReverbManager
{

public:
	CReverbManagerEAX(void);
	~CReverbManagerEAX(void);

	//////////////////////////////////////////////////////////////////////////
	// Initialization
	//////////////////////////////////////////////////////////////////////////

	void Init(IAudioDevice* pAudioDevice, int nInstanceNumber);
	bool SelectReverb(int nReverbType);

	void Release();

	//////////////////////////////////////////////////////////////////////////
	// Information
	//////////////////////////////////////////////////////////////////////////

	// writes output to screen in debug
	void DrawInformation(IRenderer* pRenderer, float xpos, float ypos);

	//////////////////////////////////////////////////////////////////////////
	// Management
	//////////////////////////////////////////////////////////////////////////

	// needs to be called regularly
	bool Update(bool bInside);

	//! returns boolean if hardware reverb (EAX) is used or not
	bool UseHardwareVoices() { return m_bHardwareReverb; }

	const char* GetTailName() { return m_sTailname.c_str(); }

private:
	bool m_bHardwareReverb;
	// bool m_bInside;
	// bool m_bNeedUpdate;
	// string m_sTailname;
};
