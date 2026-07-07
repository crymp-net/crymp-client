#include "IAudioDevice.h"
#include "ReverbInstance_RoomMachine844.h"

void CReverbInstance_RoomMachine844::FmodErrorOutput(const char* sDescription, ILog::ELogType LogType)
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

CReverbInstance_RoomMachine844::CReverbInstance_RoomMachine844(IAudioDevice* pAudioDevice)
{
	m_pCSEX = 0;

	if (pAudioDevice)
		m_pCSEX = (FMOD::System*)pAudioDevice->GetSoundLibrary();

	m_DSPReverb = 0;
	m_ExResult = FMOD_OK;
	m_PlugInType = FMOD_PLUGINTYPE_MAX;
	m_nIndex = 0;
}

CReverbInstance_RoomMachine844::~CReverbInstance_RoomMachine844()
{
	Deactivate();
}

bool CReverbInstance_RoomMachine844::Activate()
{

	if (m_pCSEX && !m_DSPReverb)
	{
		bool bActive = false;
		const char* szPath = "Game/Libs/ReverbPlugins/VST_RoomMachine844.dll";
		m_ExResult = m_pCSEX->loadPlugin(szPath, &m_PlugInType, &m_nIndex);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("load /Libs/ReverbPlugins/VST_RoomMachine844.dll reverb failed! ",
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

bool CReverbInstance_RoomMachine844::Deactivate()
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

bool CReverbInstance_RoomMachine844::Update(CRYSOUND_REVERB_PROPERTIES* pProps)
{

	// 0 "Color" 0 - 1
	// 1 "Pan L" 0 - 1
	// 2 "Pan R" 0 - 1
	// 3 "Width" 0 -1
	// 4 "Distance" 0 -1
	m_ExResult = m_DSPReverb->setParameter(5, pProps->EnvSize / 50.0f); // 5 "Roomsize" 0 - 1
	// 6 "Dry" 0 - 1
	// 7 "Wet" 0 - 1
	m_ExResult = m_DSPReverb->setParameter(8, 1.0f - pProps->DecayTime / 20.0f); // 8 "Decay" 0- 1
	m_ExResult = m_DSPReverb->setParameter(9, 1.0f - pProps->Room / -10000.0f);  // 9 "Damping" 0 - 1

	return (m_ExResult == FMOD_OK);
}
