#include "IAudioDevice.h"
#include "ReverbInstance_Classic_Reverb.h"

void CReverbInstance_Classic_Reverb::FmodErrorOutput(const char* sDescription, ILog::ELogType LogType)
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

CReverbInstance_Classic_Reverb::CReverbInstance_Classic_Reverb(IAudioDevice* pAudioDevice)
{
	m_pCSEX = 0;

	if (pAudioDevice)
		m_pCSEX = (FMOD::System*)pAudioDevice->GetSoundLibrary();

	m_DSPReverb = 0;
	m_ExResult = FMOD_OK;
	m_PlugInType = FMOD_PLUGINTYPE_MAX;
	m_nIndex = 0;
}

CReverbInstance_Classic_Reverb::~CReverbInstance_Classic_Reverb()
{
	Deactivate();
}

bool CReverbInstance_Classic_Reverb::Activate()
{

	if (m_pCSEX && !m_DSPReverb)
	{
		bool bActive = false;
		const char* szPath = "Game/Libs/ReverbPlugins/VST_Classic_Reverb.dll";
		m_ExResult = m_pCSEX->loadPlugin(szPath, &m_PlugInType, &m_nIndex);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("load /Libs/ReverbPlugins/VST_Classic_Reverb.dll reverb failed! ",
			                ILog::eError);
			return false;
		}

		m_ExResult = m_pCSEX->createDSPByIndex(m_nIndex, &m_DSPReverb);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("create DSP by index failed! ", ILog::eError);
			return false;
		}

		if (m_DSPReverb)
		{
			m_ExResult = m_pCSEX->addDSP(m_DSPReverb, 0);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("add reverb to DSP chain failed! ", ILog::eError);
				return false;
			}

			m_ExResult = m_DSPReverb->getActive(&bActive);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("activate reverb failed! ", ILog::eError);
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

bool CReverbInstance_Classic_Reverb::Deactivate()
{

	if (m_DSPReverb)
	{
		m_ExResult = m_DSPReverb->remove();
		m_pCSEX->unloadPlugin(m_PlugInType, m_nIndex);
		m_DSPReverb = 0;
		m_nIndex = 0;
		m_PlugInType = FMOD_PLUGINTYPE_MAX;
	}

	return (m_ExResult == FMOD_OK);
}

bool CReverbInstance_Classic_Reverb::Update(CRYSOUND_REVERB_PROPERTIES* pProps)
{
	m_ExResult = m_DSPReverb->setParameter(0, pProps->EnvSize / 50.0f);                // "Size" 0 - 1
	m_ExResult = m_DSPReverb->setParameter(1, 1.0f - pProps->DecayTime / 20.0f);       // "Damping"
	                                                                                   // 0 - 1
	m_ExResult = m_DSPReverb->setParameter(2, 0.5f + pProps->ReflectionsDelay / 0.6f); // "Predelay"
	                                                                                   // 0 - 1
	m_ExResult = m_DSPReverb->setParameter(3, pProps->RoomHF / -20000.0f +
	                                              (0.5f - pProps->DecayHFRatio / 4.0f)); // "Hi Damp" 0 - 1
	m_ExResult = m_DSPReverb->setParameter(4, pProps->RoomLF / -10000.0f);               // "Lo Cut" 0 - 1
	m_ExResult =
	    m_DSPReverb->setParameter(5, 1.0f - (pProps->Reflections - 1000.0f) / -11000.0f); // "Early Ref." 0 - 1
	m_ExResult = m_DSPReverb->setParameter(6, 0.5f);                                      // "Mix" 0 - 1
	m_ExResult = m_DSPReverb->setParameter(7, 0.5f);                                      // "Level" 0 - 1

	return (m_ExResult == FMOD_OK);
}
