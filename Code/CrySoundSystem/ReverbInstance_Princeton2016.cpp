////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ReverbInstance_Princeton2016.cpp
//  Version:     v1.00
//  Created:     12/10/2005 by Tomas.
//  Compilers:   Visual Studio.NET
//  Description: Princeton2016 implementation of IReverbInstance
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "ReverbInstance_Princeton2016.h"
#include "IAudioDevice.h"
#include "FMOD.h"

void CReverbInstance_Princeton2016::FmodErrorOutput(const char* sDescription, ILog::ELogType LogType)
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

//////////////////////////////////////////////////////////////////////////
// Initialization
//////////////////////////////////////////////////////////////////////////

CReverbInstance_Princeton2016::CReverbInstance_Princeton2016(IAudioDevice* pAudioDevice)
{
	m_pCSEX = 0;

	if (pAudioDevice)
		m_pCSEX = (FMOD::System*)pAudioDevice->GetSoundLibrary();

	m_DSPReverb = 0;
	m_ExResult = FMOD_OK;
	m_PlugInType = FMOD_PLUGINTYPE_MAX;
	m_nIndex = 0;
}

CReverbInstance_Princeton2016::~CReverbInstance_Princeton2016()
{
	Deactivate();
}

//////////////////////////////////////////////////////////////////////////
// Usage
//////////////////////////////////////////////////////////////////////////
bool CReverbInstance_Princeton2016::Activate()
{

	if (m_pCSEX && !m_DSPReverb)
	{
		bool bActive = false;
		const char* szPath = "Game/Libs/ReverbPlugins/VST_Princeton2016.dll";
		m_ExResult = m_pCSEX->loadPlugin(szPath, &m_PlugInType, &m_nIndex);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("load /Libs/ReverbPlugins/VST_Princeton2016.dll reverb failed! ", ILog::eError);
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

bool CReverbInstance_Princeton2016::Deactivate()
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

//////////////////////////////////////////////////////////////////////////
// Management
//////////////////////////////////////////////////////////////////////////

// needs to be called regularly
bool CReverbInstance_Princeton2016::Update(CRYSOUND_REVERB_PROPERTIES* pProps)
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

//////////////////////////////////////////////////////////////////////////
// Information
//////////////////////////////////////////////////////////////////////////

// writes output to screen in debug
// void CReverbManagerDSP::DrawInformation(IRenderer* pRenderer, float xpos, float ypos)
//{
// ActiveEaxPresetAreasIter It=m_vecActiveEaxPresetAreas.begin();
// pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColorGreen, false, "EAX - %s",(*It).sPresetName);
//	float fColorGreen[4]	={0.0f, 1.0f, 0.0f, 0.7f};
//	for (ActiveEaxPresetAreasIter It=m_vecActiveEaxPresetAreas.begin(); It!=m_vecActiveEaxPresetAreas.end(); ++It)
//	{
//		pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColorGreen, false, "EAX - %s",(*It).sPresetName);
//		ypos+=15;
//	}
//}
