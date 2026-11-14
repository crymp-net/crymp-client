#pragma once

#include "ReverbManager.h"

#include "eax.h"

struct IAudioDevice;
struct IReverbInstance;

class CReverbManagerDSP final : public CReverbManager
{
public:
	CReverbManagerDSP(void);
	~CReverbManagerDSP(void);

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
	bool UseHardwareVoices() { return false; }

	const char* GetTailName() { return m_sTailname.c_str(); }

private:
	// SWeightedEAXPreset					m_sTempEaxPreset;
	// bool												m_bInside;
	// string											m_sTailname;

	IReverbInstance* m_pReverbInstance;
};
