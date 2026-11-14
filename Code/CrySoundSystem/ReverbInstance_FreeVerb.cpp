#include "IAudioDevice.h"
#include "ReverbInstance_FreeVerb.h"

void CReverbInstance_FreeVerb::FmodErrorOutput(const char* sDescription, ILog::ELogType LogType)
{
	switch (LogType)
	{
		case ILog::eWarning:
			gEnv->pLog->LogWarning("<Sound> Sound-FmodEx-Reverb: %s (%d) %s\n", sDescription, m_ExResult,
			                       FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eError:
			gEnv->pLog->LogError("<Sound> Sound-FmodEx-Reverb: %s (%d) %s\n", sDescription, m_ExResult,
			                     FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eMessage:
			gEnv->pLog->Log("<Sound> Sound-FmodEx-Reverb: %s (%d) %s\n", sDescription, m_ExResult,
			                FMOD_ErrorString(m_ExResult));
			break;
		default:
			break;
	}
}

CReverbInstance_FreeVerb::CReverbInstance_FreeVerb(IAudioDevice* pAudioDevice)
{
	m_pCSEX = 0;

	if (pAudioDevice)
		m_pCSEX = (FMOD::System*)pAudioDevice->GetSoundLibrary();

	m_DSPReverb = 0;
	m_ExResult = FMOD_OK;
}

CReverbInstance_FreeVerb::~CReverbInstance_FreeVerb()
{
	Deactivate();
}

bool CReverbInstance_FreeVerb::Activate()
{

	if (m_pCSEX && !m_DSPReverb)
	{
		bool bActive = false;
		m_ExResult = m_pCSEX->createDSPByType(FMOD_DSP_TYPE_REVERB, &m_DSPReverb);

		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("create freeverb reverb failed! ", ILog::eError);
			return false;
		}

		if (m_DSPReverb)
		{
			m_ExResult = m_pCSEX->addDSP(m_DSPReverb, 0);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("add freeverb to DSP chain failed! ", ILog::eError);
				return false;
			}

			m_ExResult = m_DSPReverb->getActive(&bActive);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("activate freeverb failed! ", ILog::eError);
				return false;
			}

			bActive = bActive;
		}
	}

	int i = 0;
	while (m_ExResult == FMOD_OK && m_DSPReverb)
	{
		char sName[MAXCHARBUFFERSIZE];
		char sLabel[MAXCHARBUFFERSIZE];
		char sDescription[MAXCHARBUFFERSIZE];
		int nDescLen = MAXCHARBUFFERSIZE;

		float fMin = 0.0f;
		float fMax = 0.0f;

		m_ExResult = m_DSPReverb->getParameterInfo(i, sName, sLabel, sDescription, nDescLen, &fMin, &fMax);
		++i;
	}

	return true;
}

bool CReverbInstance_FreeVerb::Deactivate()
{

	if (m_DSPReverb)
		m_ExResult = m_DSPReverb->remove();

	m_DSPReverb = 0;
	// unload Plugin

	return (m_ExResult == FMOD_OK);
}

bool CReverbInstance_FreeVerb::Update(CRYSOUND_REVERB_PROPERTIES* pProps)
{
	// FMOD_DSP_REVERB_ROOMSIZE
	//	Roomsize. 0.0 to 1.0. Default = 0.5
	// FMOD_DSP_REVERB_DAMP
	//	Damp. 0.0 to 1.0. Default = 0.5
	// FMOD_DSP_REVERB_WETMIX
	//	Wet mix. 0.0 to 1.0. Default = 0.33
	// FMOD_DSP_REVERB_DRYMIX
	//	Dry mix. 0.0 to 1.0. Default = 0.66
	// FMOD_DSP_REVERB_WIDTH
	//	Width. 0.0 to 1.0. Default = 1.0
	// FMOD_DSP_REVERB_MODE
	//	Mode. 0 (normal), 1 (freeze). Default = 0

	m_ExResult = m_DSPReverb->setParameter(FMOD_DSP_REVERB_ROOMSIZE, pProps->EnvSize / 50.0f);
	if (m_ExResult != FMOD_OK)
		m_ExResult = m_ExResult;

	m_ExResult = m_DSPReverb->setParameter(FMOD_DSP_REVERB_DAMP, pProps->DecayTime / 20.0f);
	if (m_ExResult != FMOD_OK)
		m_ExResult = m_ExResult;

	return (m_ExResult == FMOD_OK);
}
