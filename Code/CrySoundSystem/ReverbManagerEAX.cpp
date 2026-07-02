#include "CryCommon/CryRenderer/IRenderer.h"

#include "FMOD.h"
#include "ReverbManagerEAX.h"
#include "IAudioDevice.h"

//////////////////////////////////////////////////////////////////////////
// Initialization
//////////////////////////////////////////////////////////////////////////

CReverbManagerEAX::CReverbManagerEAX()
{
	m_pAudioDevice = NULL;
	m_bHardwareReverb = false;
}

CReverbManagerEAX::~CReverbManagerEAX()
{
}

void CReverbManagerEAX::Init(IAudioDevice* pAudioDevice, int nInstanceNumber)
{
	assert(pAudioDevice);
	m_pAudioDevice = pAudioDevice;
}

bool CReverbManagerEAX::SelectReverb(int nReverbType)
{
	if (!m_pAudioDevice)
		return false;

	m_bNeedUpdate = true;
	if (nReverbType == REVERB_TYPE_NONE)
	{
		m_ReverbEnable = false;
		m_vecActiveReverbPresetAreas.clear();
		Update(false);
	}
	else
		m_ReverbEnable = true;

	switch (nReverbType)
	{
		case REVERB_TYPE_HARDWARE:
		{
			m_bHardwareReverb = true;
		}
		break;
		case REVERB_TYPE_SOFTWARE:
		{
			m_bHardwareReverb = false;
		}
		break;
		default:
			return false;
	}

	return true;
}

void CReverbManagerEAX::Release()
{
	m_vecActiveReverbPresetAreas.clear();
}

//////////////////////////////////////////////////////////////////////////
// Information
//////////////////////////////////////////////////////////////////////////

// writes output to screen in debug
void CReverbManagerEAX::DrawInformation(IRenderer* pRenderer, float xpos, float ypos)
{
	// ActiveEaxPresetAreasIter It=m_vecActiveEaxPresetAreas.begin();
	// pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColorGreen, false, "EAX - %s",(*It).sPresetName);
	float fColorGreen[4] = {0.0f, 1.0f, 0.0f, 0.7f};
	float fColorGray[4] = {0.3f, 0.3f, 0.3f, 0.7f};
	float fColorTemp[4] = {0.0f, 0.0f, 0.0f, 0.7f};
	float fColorBlue[4] = {0.4f, 0.4f, 1.0f, 0.7f};

	if (!m_ReverbEnable)
	{
		pRenderer->Draw2dLabel(xpos, ypos, 1.35f, fColorGray, false, "Reverb disabled");
		ypos += 11;
		return;
	}

	pRenderer->Draw2dLabel(xpos, ypos, 1.35f, fColorBlue, false, "EAX - %s ",
	                       m_sTempEaxPreset.pReverbPreset->sPresetName.c_str(),
	                       m_sTempEaxPreset.pReverbPreset->sTailName.c_str());
	ypos += 11;

	ActiveReverbPresetAreasIterConst IterEnd = m_vecActiveReverbPresetAreas.end();
	for (ActiveReverbPresetAreasIterConst It = m_vecActiveReverbPresetAreas.begin(); It != IterEnd; ++It)
	{
		fColorTemp[0] = (*It).fWeight * fColorGreen[0] + (1.0f - (*It).fWeight) * fColorGray[0];
		fColorTemp[1] = (*It).fWeight * fColorGreen[1] + (1.0f - (*It).fWeight) * fColorGray[1];
		fColorTemp[2] = (*It).fWeight * fColorGreen[2] + (1.0f - (*It).fWeight) * fColorGray[2];

		// pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColorTemp, false, "EAX - %s Tail: %s",(*It).sPresetName,
		// (*It).EAX.sTailName);
		pRenderer->Draw2dLabel(xpos, ypos, 1.35f, fColorTemp, false, "Reverb - %s Tail: %s",
		                       (*It).pReverbPreset->sPresetName.c_str(),
		                       (*It).pReverbPreset->sTailName.c_str());
		ypos += 11;
	}
}

//////////////////////////////////////////////////////////////////////////
// Management
//////////////////////////////////////////////////////////////////////////

// needs to be called regularly
bool CReverbManagerEAX::Update(bool bInside)
{
	if (!m_bNeedUpdate)
		return true;

	m_bNeedUpdate = false;

	if (!m_ReverbEnable)
	{
		m_sTempEaxPreset.pReverbPreset->EAX = m_EAXOffPreset;
		m_sTempEaxPreset.pReverbPreset->sPresetName = "Off";
		return false;
	}
	else
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
			CRYSOUND_REVERB_PROPERTIES pPropsUnderWater = CRYSOUND_REVERB_UNDERWATER;
			m_sTempEaxPreset.pReverbPreset->EAX = pPropsUnderWater;
			m_sTempEaxPreset.pReverbPreset->sPresetName = "UnderWater";
		}
		else
		{

			// Verifying Reverb Values enabled/disables
			bool Checkresults = true;
			float SumOfWeight = 0;
			m_bInside = bInside;

			// if there is only one Preset just interpolate directly with its weight
			if (m_vecActiveReverbPresetAreas.size() == 1)
			{
				ActiveReverbPresetAreasIterConst It = m_vecActiveReverbPresetAreas.begin();
				SumOfWeight = (*It).fWeight;
				m_sTempEaxPreset.fWeight = (*It).fWeight;
				m_sTempEaxPreset.pReverbPreset->EAX = (*It).pReverbPreset->EAX;
				m_sTempEaxPreset.pReverbPreset->sPresetName = (*It).pReverbPreset->sPresetName;
			}
			else
			{
				ActiveReverbPresetAreasIterConst ItEnd = m_vecActiveReverbPresetAreas.end();
				for (ActiveReverbPresetAreasIterConst It = m_vecActiveReverbPresetAreas.begin();
				     It != ItEnd; ++It)
				{
					// Copy first element to the temp preset
					if (It == m_vecActiveReverbPresetAreas.begin())
					{
						m_sTempEaxPreset.pReverbPreset->EAX = (*It).pReverbPreset->EAX;
						m_sTempEaxPreset.pReverbPreset->sPresetName = "Reverb_Mix";
						m_sTempEaxPreset.fWeight = (*It).fWeight;
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
							EAX3ListenerInterpolate(&m_sTempEaxPreset.pReverbPreset->EAX,
							                        &((*It).pReverbPreset->EAX), BlendRatio,
							                        &m_sTempEaxPreset.pReverbPreset->EAX,
							                        Checkresults);
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
				{
					EAX3ListenerInterpolate(&m_EAXOutdoorPreset,
					                        &m_sTempEaxPreset.pReverbPreset->EAX, SumOfWeight,
					                        &m_sTempEaxPreset.pReverbPreset->EAX, Checkresults);
					// EAX3ListenerInterpolate(&m_EAXOutdoorPreset, &m_sTempEaxPreset.EAX,
					// SumOfWeight, &m_sTempEaxPreset.EAX, Checkresults);
					m_sTempEaxPreset.pReverbPreset->sPresetName = m_sOutdoorMix;
				}
				else
				{
					EAX3ListenerInterpolate(&m_EAXIndoorPreset,
					                        &m_sTempEaxPreset.pReverbPreset->EAX, SumOfWeight,
					                        &m_sTempEaxPreset.pReverbPreset->EAX, Checkresults);
					// EAX3ListenerInterpolate(&m_EAXIndoorPreset, &m_sTempEaxPreset.EAX,
					// SumOfWeight, &m_sTempEaxPreset.EAX, Checkresults);
					m_sTempEaxPreset.pReverbPreset->sPresetName = m_sIndoorMix;
				}
			}
		}
	}

	/*
	if (m_nLastEax!=EAX_PRESET_UNDERWATER)
	{
	// if not underwater, check if we now went to outside
	if (bWasInside!=m_bInside)
	{
	if (!m_bInside)
	{
	// if outside,set the global EAX outdoor environment
	// disabled by Tomas
	SetEaxListenerEnvironment(m_EAXOutdoor.nPreset, (m_EAXOutdoor.nPreset==-1) ? &m_EAXOutdoor.EAX : NULL);
	m_sTempEaxPreset.EAX = m_EAXOutdoor.EAX;
	}
	else m_sTempEaxPreset.EAX = m_EAXIndoor.EAX;
	}
	else m_sTempEaxPreset.EAX=m_EAXOutdoor.EAX;
	}

	*/

	// TODO reactivate weather airabsorption
	// Add the weather effect
	// m_sTempEaxPreset.EAX.AirAbsorptionHF =
	// m_fWeatherAirAbsorptionMultiplyer*m_sTempEaxPreset.EAX.AirAbsorptionHF;
	// TODO take the Weather Inversion into effect

	// TODO Reverb_Properties are different in FmodEX
	if (m_pAudioDevice)
	{
		m_sTempEaxPreset.pReverbPreset->EAX.Instance = 0;
		FMOD::System* pEx = (FMOD::System*)m_pAudioDevice->GetSoundLibrary();
		// FMOD_REVERB_PROPERTIES *pProp = (FMOD_REVERB_PROPERTIES*)&(m_sTempEaxPreset.EAX);
		FMOD_REVERB_PROPERTIES* pProp = (FMOD_REVERB_PROPERTIES*)&(m_sTempEaxPreset.pReverbPreset->EAX);
		pProp->Instance = 0;
		pProp->Flags = FMOD_REVERB_FLAGS_DEFAULT;
		FMOD_RESULT Result = pEx->setReverbProperties(pProp);

		// if (Result != FMOD_OK)
		// int a = 0;
	}

	m_bNeedUpdate = false;
	return true;
}
