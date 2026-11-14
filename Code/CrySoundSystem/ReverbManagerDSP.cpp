#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CrySystem/ISystem.h"

#include "IReverbInstance.h"
#include "ReverbInstance_Classic_Reverb.h"
#include "ReverbInstance_FreeVerb.h"
#include "ReverbInstance_Princeton2016.h"
#include "ReverbInstance_RoomMachine844.h"
#include "ReverbManagerDSP.h"

//////////////////////////////////////////////////////////////////////////
// Initialization
//////////////////////////////////////////////////////////////////////////

CReverbManagerDSP::CReverbManagerDSP()
{
	m_pReverbInstance = 0;
	m_pAudioDevice = 0;
}

CReverbManagerDSP::~CReverbManagerDSP()
{
}

void CReverbManagerDSP::Init(IAudioDevice* pAudioDevice, int nInstanceNumber)
{
	assert(pAudioDevice);
	m_pAudioDevice = pAudioDevice;
	SelectReverb(REVERB_TYPE_FREEVERB);
}

bool CReverbManagerDSP::SelectReverb(int nReverbType)
{
	if (m_pReverbInstance)
		m_pReverbInstance->Deactivate();

	if (!m_pAudioDevice)
		return false;

	switch (nReverbType)
	{
		case REVERB_TYPE_FREEVERB:
		{
			m_pReverbInstance = new CReverbInstance_FreeVerb(m_pAudioDevice);
			m_ReverbEnable = m_pReverbInstance->Activate();
		}
		break;
		case REVERB_TYPE_VST_CLASSIC_REVERB:
		{
			m_pReverbInstance = new CReverbInstance_Classic_Reverb(m_pAudioDevice);
			m_ReverbEnable = m_pReverbInstance->Activate();
		}
		break;
		case REVERB_TYPE_VST_PRINCETON2016:
		{
			m_pReverbInstance = new CReverbInstance_Princeton2016(m_pAudioDevice);
			m_ReverbEnable = m_pReverbInstance->Activate();
		}
		break;
		case REVERB_TYPE_VST_ROOMMACHINE844:
		{
			m_pReverbInstance = new CReverbInstance_RoomMachine844(m_pAudioDevice);
			m_ReverbEnable = m_pReverbInstance->Activate();
		}
		break;
		default:
			return false;
	}

	return true;
}

void CReverbManagerDSP::Release()
{
	if (m_pReverbInstance)
	{
		m_pReverbInstance->Deactivate();
		delete m_pReverbInstance;
		m_pReverbInstance = NULL;
	}

	m_vecActiveReverbPresetAreas.clear();
}

//////////////////////////////////////////////////////////////////////////
// Information
//////////////////////////////////////////////////////////////////////////

// writes output to screen in debug
void CReverbManagerDSP::DrawInformation(IRenderer* pRenderer, float xpos, float ypos)
{
	// ActiveEaxPresetAreasIter It=m_vecActiveEaxPresetAreas.begin();
	// pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColorGreen, false, "EAX - %s",(*It).sPresetName);
	float fColorGreen[4] = {0.0f, 1.0f, 0.0f, 0.7f};
	float fColorGray[4] = {0.3f, 0.3f, 0.3f, 0.7f};
	float fColorTemp[4] = {0.0f, 0.0f, 0.0f, 0.7f};
	float fColorBlue[4] = {0.4f, 0.4f, 1.0f, 0.7f};

	if (!m_ReverbEnable)
	{
		pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColorGray, false, "Reverb disabled");
		ypos += 15;
		return;
	}

	pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColorBlue, false, "DSP - %s ",
	                       m_sTempEaxPreset.pReverbPreset->sPresetName.c_str(),
	                       m_sTempEaxPreset.pReverbPreset->sTailName.c_str());
	ypos += 15;

	ActiveReverbPresetAreasIterConst IterEnd = m_vecActiveReverbPresetAreas.end();
	for (ActiveReverbPresetAreasIterConst It = m_vecActiveReverbPresetAreas.begin(); It != IterEnd; ++It)
	{
		fColorTemp[0] = (*It).fWeight * fColorGreen[0] + (1.0f - (*It).fWeight) * fColorGray[0];
		fColorTemp[1] = (*It).fWeight * fColorGreen[1] + (1.0f - (*It).fWeight) * fColorGray[1];
		fColorTemp[2] = (*It).fWeight * fColorGreen[2] + (1.0f - (*It).fWeight) * fColorGray[2];

		// pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColorTemp, false, "EAX - %s Tail: %s",(*It).sPresetName,
		// (*It).EAX.sTailName);
		pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColorTemp, false, "Reverb - %s Tail: %s",
		                       (*It).pReverbPreset->sPresetName.c_str(),
		                       (*It).pReverbPreset->sTailName.c_str());
		ypos += 15;
	}
}

//////////////////////////////////////////////////////////////////////////
// Management
//////////////////////////////////////////////////////////////////////////

// needs to be called regularly
bool CReverbManagerDSP::Update(bool bInside)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	//////////////////////////////////////////////////////////////////////////
	// new code by Tomas
	// go through the vector of active EAX Presets, calculate the resulting Interpolation and set it
	// CRYSOUND_REVERB_PROPERTIES GenericEAX = CS_PRESET_GENERIC;

	/*
	if (bWasInside!=m_bInside)
	{
	if (m_bInside)
	SetEaxListenerEnvironment(m_EAXIndoor.nPreset, (m_EAXIndoor.nPreset==-1) ? &m_EAXIndoor.EAX : NULL,
	FLAG_SOUND_INDOOR); else SetEaxListenerEnvironment(m_EAXOutdoor.nPreset, (m_EAXOutdoor.nPreset==-1) ?
	&m_EAXOutdoor.EAX : NULL, FLAG_SOUND_OUTDOOR);
	}
	*/
	if (m_bForceUnderwater)
	{
		CRYSOUND_REVERB_PROPERTIES pPropsOut = CRYSOUND_REVERB_UNDERWATER;
		m_sTempEaxPreset.pReverbPreset->EAX = pPropsOut;
	}
	else
	{

		// Verifying EAX Values enabled/disables
		bool Checkresults = true;
		float SumOfWeight = 0;
		m_bInside = bInside;

		// if there is only one Preset just interpolate directly with its weight
		if (m_vecActiveReverbPresetAreas.size() == 1)
		{
			ActiveReverbPresetAreasIter It = m_vecActiveReverbPresetAreas.begin();
			SumOfWeight = (*It).fWeight;
			m_sTempEaxPreset = (*It);
		}
		else
		{
			for (ActiveReverbPresetAreasIter It = m_vecActiveReverbPresetAreas.begin();
			     It != m_vecActiveReverbPresetAreas.end(); ++It)
			{
				// Copy first element to the temp preset
				if (It == m_vecActiveReverbPresetAreas.begin())
				{
					m_sTempEaxPreset = (*It);
				}
				else // start interpolating at the 2nd element
				{
					// calculate the sum of the weight
					SumOfWeight = m_sTempEaxPreset.fWeight + (*It).fWeight;
					if (SumOfWeight > 0)
					{
						float BlendRatio = 0;
						// float iteratorweight=(*It).fWeight;
						BlendRatio = (*It).fWeight / SumOfWeight;
						// EAX3ListenerInterpolate(&m_sTempEaxPreset.EAX, &((*It).EAX),
						// BlendRatio, &m_sTempEaxPreset.EAX, Checkresults);
						EAX3ListenerInterpolate(
						    &m_sTempEaxPreset.pReverbPreset->EAX, &((*It).pReverbPreset->EAX),
						    BlendRatio, &m_sTempEaxPreset.pReverbPreset->EAX, Checkresults);
						m_sTempEaxPreset.fWeight = BlendRatio;
						// gEnv->pLog->LogToConsole("setting Weight %f Interpolate",
						// BlendRatio);
					}
				}
			}
		}

		// if the weighted sum of effect has not reached 100% then "fill up" with basic preset
		if (SumOfWeight < 1)
		{
			if (!m_bInside)
				// EAX3ListenerInterpolate(&m_EAXOutdoorPreset, &m_sTempEaxPreset.EAX, SumOfWeight,
				// &m_sTempEaxPreset.EAX, Checkresults);
				EAX3ListenerInterpolate(&m_EAXOutdoorPreset, &m_sTempEaxPreset.pReverbPreset->EAX,
				                        SumOfWeight, &m_sTempEaxPreset.pReverbPreset->EAX,
				                        Checkresults);
			else
				// EAX3ListenerInterpolate(&m_EAXIndoorPreset, &m_sTempEaxPreset.EAX, SumOfWeight,
				// &m_sTempEaxPreset.EAX, Checkresults);
				EAX3ListenerInterpolate(&m_EAXIndoorPreset, &m_sTempEaxPreset.pReverbPreset->EAX,
				                        SumOfWeight, &m_sTempEaxPreset.pReverbPreset->EAX,
				                        Checkresults);
		}
	}

	// TODO Reverb_Properties are different in FmodEX
	if (m_pReverbInstance)
	{
		m_sTempEaxPreset.pReverbPreset->EAX.Instance = 0;
		m_pReverbInstance->Update(&(m_sTempEaxPreset.pReverbPreset->EAX));
	}
	return true;
}
