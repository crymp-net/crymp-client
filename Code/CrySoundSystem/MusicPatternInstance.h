#pragma once

#include "CryCommon/CryCore/smartptr.h"

class CMusicPattern;
struct IMusicPatternDecoderInstance;
struct SMusicMood;

class CMusicPatternInstance final
{
protected:
	int m_nRefs;
	CMusicPattern* m_pPattern;
	IMusicPatternDecoderInstance* m_pDecoderInstance;
	SMusicMood* m_pMood;

public:
	CMusicPatternInstance(CMusicPattern* pPattern);
	~CMusicPatternInstance();
	void AddRef() { m_nRefs++; }
	void Release()
	{
		m_nRefs--;
		if (m_nRefs <= 0)
			delete this;
	}
	CMusicPattern* GetPattern() { return m_pPattern; }

	void SetMood(SMusicMood* pMood) { m_pMood = pMood; }
	SMusicMood* GetMood() { return m_pMood; }

	//! Seek to beginning of pattern (if nDelay is set it will wait nDelay-samples before starting playback).
	bool Seek0(const int nDelay = 0);
	bool SeekPos(const int nSamplePos);
	int GetPos();
	bool GetPCMData(signed long* pDataOut, int nSamples, bool bLoop = true);
	int GetSamplesToNextFadePoint();
	int GetSamplesToLastFadePoint();
	int GetSamplesToEnd();

	bool m_bPlayFromStart;
	bool m_bPlaySingle;
	bool m_bPlayAtOnce;
};
