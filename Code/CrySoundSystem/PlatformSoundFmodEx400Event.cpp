#include "CryCommon/CryCore/CrySizer.h"
#include "CryCommon/CrySystem/ITimer.h"

#include "AudioDeviceFmodEx400.h"
#include "PlatformSoundFmodEx400Event.h"
#include "Sound.h"
#include "SoundBufferFmodEx400.h"
#include "SoundSystem.h"

tmapParameterNames CPlatformSoundFmodEx400Event::ParameterNames;

FMOD_RESULT F_CALLBACK CPlatformSoundFmodEx400Event::OnCallBack(FMOD_EVENT* event, FMOD_EVENT_CALLBACKTYPE type,
                                                                void* param1, void* param2, void* userdata)
{
	if (!CSound::m_pSSys)
		return FMOD_OK;

	tSoundID nSoundID = static_cast<tSoundID>(reinterpret_cast<uintptr_t>(userdata));
	_smart_ptr<CSound> pSound = (CSound*)CSound::m_pSSys->GetSound(nSoundID);

	CPlatformSoundFmodEx400Event* pPlatformSound = NULL;

	if (pSound)
		pPlatformSound = (CPlatformSoundFmodEx400Event*)pSound->GetPlatformSoundPtr();

	if (pPlatformSound)
	{
		switch (type)
		{
			// TODO: newer FMOD?
			case FMOD_EVENT_CALLBACKTYPE_SOUNDDEF_START:
				if (pPlatformSound->m_State == pssLOADING || pPlatformSound->m_State == pssREADY)
				{
					pPlatformSound->m_State = pssPLAYING;
					pSound->OnEvent(SOUND_EVENT_ON_PLAYBACK_STARTED);
				}
				break;
			case FMOD_EVENT_CALLBACKTYPE_SOUNDDEF_END:
				break;
			case FMOD_EVENT_CALLBACKTYPE_SYNCPOINT:
			{
				pSound->OnEvent(SOUND_EVENT_ON_SYNCHPOINT);
				if (pPlatformSound->m_State == pssWAITINGFORSYNC)
				{
					bool bPause = true;
					ptParamBOOL TempParam(bPause);
					pPlatformSound->SetParamByType(pspPAUSEMODE, &TempParam);
					pPlatformSound->m_State = pssSYNCARRIVED;
				}
				break;
			}
			case FMOD_EVENT_CALLBACKTYPE_NET_MODIFIED:
			{
				// reset Maxvolume that is used to normalize sound volume
				FMOD::Event* pEvent = (FMOD::Event*)pPlatformSound->GetSoundHandle();
				if (pEvent)
				{
					FMOD_RESULT ExResult = pEvent->getVolume(&(pPlatformSound->m_fMaxVolume));

					if (ExResult != FMOD_OK)
						return FMOD_OK;
				}
				break;
			}
			case FMOD_EVENT_CALLBACKTYPE_EVENTFINISHED:
			{
				if (!pSound->GetLoopMode())
					pSound->Stop();
				else
					pSound->Deactivate();

				pPlatformSound->m_State = pssSTOPPED;

				// gEnv->pLog->Log("----> EVENTFINISHED");
				break;
			}
			case FMOD_EVENT_CALLBACKTYPE_STOLEN:
			{
				pSound->Deactivate();
				pPlatformSound->m_State = pssSTOLEN;
				// gEnv->pLog->Log("----> EVENTFINISHED");
				break;
			}
			case FMOD_EVENT_CALLBACKTYPE_SOUNDDEF_CREATE:
			{
				if (pSound->GetSoundBufferPtr() &&
				    pSound->GetSoundBufferPtr()->GetProps()->eBufferType != btEVENT)
				{
					FMOD::Sound* sound =
					    (FMOD::Sound*)pSound->GetSoundBufferPtr()->GetAssetHandle();
					*((FMOD::Sound**)param2) = sound;
				}

				break;
			}
			case FMOD_EVENT_CALLBACKTYPE_SOUNDDEF_RELEASE:
			{
				// triggering the radio end squelch, need fadeout time for that to work
				if (pSound->GetFlags() & FLAG_SOUND_SQUELCH)
				{
					float fOldSquelch = 0.0f;
					pPlatformSound->GetParamByName("squelch", &fOldSquelch, false);
					pPlatformSound->SetParamByName("squelch", 0.0f, false);
					pPlatformSound->SetParamByName("squelch", fOldSquelch, false);
				}
				pSound->Stop();
				break;
			}
			default:
				break;
		}
	}

	return FMOD_OK;
}

//////////////////////////////////////////////////////////////////////////
CPlatformSoundFmodEx400Event::CPlatformSoundFmodEx400Event(CSound* pSound, FMOD::System* pCSEX, const char* sEventName)
{
	assert(pCSEX);
	m_pCSEX = pCSEX;
	m_pAudioDevice = (CAudioDeviceFmodEx400*)m_pSound->GetSoundSystemPtr()->GetIAudioDevice();
	Reset(pSound, sEventName);
}

//////////////////////////////////////////////////////////////////////////
CPlatformSoundFmodEx400Event::~CPlatformSoundFmodEx400Event(void)
{
	FreeSoundHandle();
}

void CPlatformSoundFmodEx400Event::Reset(CSound* pSound, const char* sEventName)
{
	assert(pSound);
	m_pSound = pSound;
	m_pEvent = NULL;
	m_pExChannel = NULL;
	m_nRefCount = 0;
	m_ExResult = FMOD_OK;
	m_fMaxRadius = 0.0f;
	m_fLoudness = 0.0f;
	m_fMaxVolume = -1.0f;
	m_sEventName = sEventName;
	m_State = pssNONE;
	m_nRefCount = 0;
	m_nLenghtInMs = 0;
	m_nMaxSpawns = 0;
	m_nDistanceIndex = -1;
	m_nCurrentSpawns = 0;
	m_QueuedParameter.clear();

	int nNum = ParameterNames.size();
	m_ParameterIndex.resize(nNum);
	for (int i = 0; i < nNum; ++i)
		m_ParameterIndex[i] = -1;
}

//////////////////////////////////////////////////////////////////////////
int32 CPlatformSoundFmodEx400Event::AddRef()
{
	return (++m_nRefCount);
}

//////////////////////////////////////////////////////////////////////////
int32 CPlatformSoundFmodEx400Event::Release()
{
	--m_nRefCount;
	if (m_nRefCount <= 0)
	{
		m_pAudioDevice->RemovePlatformSound(this);
		// delete this;
		return (0);
	}
	return m_nRefCount;
}

void CPlatformSoundFmodEx400Event::FmodErrorOutput(const char* sDescription, ILog::ELogType LogType)
{
	if (m_ExResult == FMOD_ERR_INVALID_HANDLE)
	{
		if (CSound::m_pSSys->g_nDebugSound == SOUNDSYSTEM_DEBUG_DETAIL)
		{
			TFixedResourceName sTemp("Invalid Handle - Playback maximum reached on ");
			sTemp += m_pSound->GetName();
			sTemp += "?";
			gEnv->pLog->LogWarning("<Sound> PSE: %s \n", sTemp.c_str());
		}

		m_pEvent = NULL;
		m_State = pssERROR;
		m_pSound->Stop();
		return;
	}

	switch (LogType)
	{
		case ILog::eWarning:
			gEnv->pLog->LogWarning("<Sound> PSE: (%s) %s", m_pSound->GetName(), sDescription);
			gEnv->pLog->LogWarning("<Sound> PSE: (%d) %s", m_ExResult, FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eError:
			gEnv->pLog->LogError("<Sound> PSE: (%s) %s", m_pSound->GetName(), sDescription);
			gEnv->pLog->LogError("<Sound> PSE: (%d) %s", m_ExResult, FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eMessage:
			gEnv->pLog->Log("<Sound> PSE: (%s) %s", m_pSound->GetName(), sDescription);
			gEnv->pLog->Log("<Sound> PSE: (%d) %s", m_ExResult, FMOD_ErrorString(m_ExResult));
			break;
		default:
			break;
	}
}

//////////////////////////////////////////////////////////////////////////
tSoundHandle CPlatformSoundFmodEx400Event::GetSoundHandle() const
{
	// if (m_pExChannel == NULL)
	// return NULL;
	return ((tSoundHandle)m_pEvent);
}

//////////////////////////////////////////////////////////////////////////
bool CPlatformSoundFmodEx400Event::Set3DPosition(Vec3* pvPosition, Vec3* pvVelocity, Vec3* pvOrientation)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	FMOD_VECTOR vExPos;
	FMOD_VECTOR vExVel;
	FMOD_VECTOR vExOrient;

	CSoundSystem* pSys = m_pSound->GetSoundSystemPtr();
	if (!pSys)
		return false;

	// SListener *pListener = NULL;
	//  axis switch
	if (pvPosition)
	{
		vExPos.x = pvPosition->x;
		vExPos.y = pvPosition->z;
		vExPos.z = pvPosition->y;
	}

	if (pvVelocity)
	{
		// Doppler Value here does not have to be multiplied with the DopplerScale of the event
		// because FMOD will do that internally anyway

		vExVel.x = clamp(pvVelocity->x, -200.0f, 200.0f);
		vExVel.y = clamp(pvVelocity->z, -200.0f, 200.0f);
		vExVel.z = clamp(pvVelocity->y, -200.0f, 200.0f);
	}

	if (pvOrientation)
	{
		vExOrient.x = pvOrientation->x;
		vExOrient.y = pvOrientation->z;
		vExOrient.z = pvOrientation->y;
	}

	if (m_pEvent)
	{
		m_ExResult = m_pEvent->set3DAttributes(pvPosition ? &vExPos : NULL, pvVelocity ? &vExVel : NULL,
		                                       pvOrientation ? &vExOrient : NULL);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("set 3d event position failed! ", ILog::eMessage);
			return false;
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CPlatformSoundFmodEx400Event::SetObstruction(const SObstruction* pObstruction)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	assert(pObstruction);

	if (m_pEvent) // && m_State != pssINFOONLY)
	{
		float fDirect = pObstruction->GetDirect();
		float fReverb = pObstruction->GetReverb();

		assert(fDirect <= 1.0f && fDirect >= 0.0f);
		assert(fReverb <= 1.0f && fReverb >= 0.0f);

		m_ExResult = m_pEvent->set3DOcclusion(fDirect, fReverb);
		if (m_ExResult != FMOD_OK)
		{
			// FmodErrorOutput("set set3DOcclusion failed! ", ILog::eMessage);
			return;
		}
	}
}

// Gets and Sets Parameter defined in the enumSoundParam list
//////////////////////////////////////////////////////////////////////////
bool CPlatformSoundFmodEx400Event::GetParamByType(enumPlatformSoundParamSemantics eSemantics, ptParam* pParam)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	if (m_pEvent)
	{

		switch (eSemantics)
		{
			case pspNONE:
				break;
			case pspSOUNDTYPE:
				break;
			case pspSAMPLETYPE:
				break;
			case pspFREQUENCY:
			{
				int32 nTemp;
				if (!(pParam->GetValue(nTemp)))
					return false;
				// nTemp = CS_GetFrequency(m_nChannel);
				pParam->SetValue(nTemp);
				break;
			}
			case pspVOLUME:
			{
				float fTemp;
				if (!(pParam->GetValue(fTemp)))
					return false;

				m_ExResult = m_pEvent->getVolume(&fTemp);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event property <volume> failed! ", ILog::eMessage);
					return false;
				}

				if (fTemp != 1.0f)
					int a = 12;

				// volume needs to be normalized with allowed MaxVolume (which is -1.0 the first time)
				fTemp *= abs(m_fMaxVolume);

				pParam->SetValue(fTemp);
				break;
			}
			case pspLOUDNESSINDB:
			{
				float fTemp = 60.0f;
				if (!(pParam->GetValue(fTemp)))
					return false;

				m_ExResult = m_pEvent->getProperty("loudness", &fTemp);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event property <loudness> failed! ", ILog::eMessage);
					return false;
				}

				pParam->SetValue(fTemp);
				break;
			}
			case pspIS3D:
			{
				bool bTemp = false;
				if (!(pParam->GetValue(bTemp)))
					return false;
				FMOD_MODE Mode;
				m_ExResult = m_pEvent->getPropertyByIndex(FMOD_EVENTPROPERTY_MODE, &Mode);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event property <is3D/Mode> failed! ", ILog::eMessage);
					return false;
				}
				if (Mode & FMOD_3D)
					bTemp = true;
				pParam->SetValue(bTemp);
				break;
			}
			case pspISPLAYING:
			{
				bool bTemp = false;
				if (!(pParam->GetValue(bTemp)))
					return false;

				FMOD_EVENT_STATE EventState;
				m_ExResult = m_pEvent->getState(&EventState);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event state failed! ", ILog::eMessage);
					return false;
				}

				if (EventState & FMOD_EVENT_STATE_PLAYING ||
				    EventState & FMOD_EVENT_STATE_LOADING) // EVENT_STATE_CHANNELSACTIVE
					bTemp = true;
				else
					bTemp = false;

				pParam->SetValue(bTemp);
				break;
			}
			case pspLOOPMODE:
			{
				bool bTemp;
				if (!(pParam->GetValue(bTemp)))
					return false;

				int nOneShot = 0;
				m_ExResult = m_pEvent->getPropertyByIndex(FMOD_EVENTPROPERTY_ONESHOT, &nOneShot);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event property <oneshot> failed! ", ILog::eMessage);
					return false;
				}

				bTemp = (nOneShot == 0);

				// bTemp = (Mode != FMOD_LOOP_OFF);
				pParam->SetValue(bTemp);
				break;
			}
			case pspPAUSEMODE:
			{
				bool bTemp;
				if (!(pParam->GetValue(bTemp)))
					return false;
				m_ExResult = m_pEvent->getPaused(&bTemp);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event <paused> failed! ", ILog::eMessage);
					return false;
				}
				pParam->SetValue(bTemp);
				break;
			}
			case pspSPEAKERPAN:
				return false;
				break;
			case pspSPEAKER3DPAN:
				return false;
				break;
			case pspSAMPLEPOSITION:
			{
				int32 nTemp;
				if (!(pParam->GetValue(nTemp)))
					return false;
				// nTemp = CS_GetCurrentPosition(m_nChannel);
				pParam->SetValue(nTemp);
				break;
			}
				//		TIMEPOSITION
				// CS_Sample_GetDefaults((CS_SAMPLE*)m_pSoundBuffer->GetAssetHandle(), &nFreq, NULL,
				// NULL, NULL); return (unsigned
				// int)((float)CS_GetCurrentPosition(m_nChannel)/(float)nFreq*1000.0f);

			case psp3DPOSITION:
			{
				Vec3 VecTemp;
				if (!(pParam->GetValue(VecTemp)))
					return false;

				FMOD_VECTOR pos;
				m_ExResult = m_pEvent->get3DAttributes(&pos, 0);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event property <3d position> failed! ", ILog::eMessage);
					return false;
				}

				VecTemp.x = pos.x;
				VecTemp.y = pos.z;
				VecTemp.z = pos.y;
				pParam->SetValue(VecTemp);
				break;
			}
			case psp3DVELOCITY:
			{
				Vec3 VecTemp;
				if (!(pParam->GetValue(VecTemp)))
					return false;

				FMOD_VECTOR vel;
				m_ExResult = m_pEvent->get3DAttributes(0, &vel);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event property <3d velocity> failed! ", ILog::eMessage);
					return false;
				}

				VecTemp.x = vel.x;
				VecTemp.y = vel.z;
				VecTemp.z = vel.y;
				pParam->SetValue(VecTemp);
				break;
			}
			case psp3DDOPPLERSCALE:
			{
				float fTemp = 0.0f;
				if (!(pParam->GetValue(fTemp)))
					return false;

				m_ExResult = m_pEvent->getPropertyByIndex(FMOD_EVENTPROPERTY_3D_DOPPLERSCALE, &fTemp);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event property <doppler scale> failed! ", ILog::eMessage);
					return false;
				}

				pParam->SetValue(fTemp);
				break;
			}
			case pspPRIORITY:
			{
				int32 nTemp;
				if (!(pParam->GetValue(nTemp)))
					return false;

				m_ExResult = m_pEvent->getPropertyByIndex(FMOD_EVENTPROPERTY_PRIORITY, &nTemp);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event property <priority> failed! ", ILog::eMessage);
					return false;
				}

				pParam->SetValue(nTemp);
				break;
			}
			case pspLENGTHINMS:
			{
				int32 nTemp;
				if (!(pParam->GetValue(nTemp)))
					return false;

				FMOD_EVENT_INFO Info;
				memset(&Info, 0, sizeof(FMOD_EVENT_INFO));

				m_ExResult = m_pEvent->getInfo(0, 0, &Info);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event info <length ms> failed! ", ILog::eMessage);
					return false;
				}

				m_nLenghtInMs = Info.lengthms;

				if (Info.lengthms < 0)
					return false;

				pParam->SetValue(Info.lengthms);
				break;
			}
			case pspLENGTHINBYTES:
			{
				int32 nTemp;
				if (!(pParam->GetValue(nTemp)))
					return false;

				FMOD_EVENT_INFO Info;
				memset(&Info, 0, sizeof(FMOD_EVENT_INFO));

				m_ExResult = m_pEvent->getInfo(0, 0, &Info);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event info <length bytes> failed! ", ILog::eMessage);
					return false;
				}

				if (Info.memoryused < 0)
					return false;

				pParam->SetValue(Info.memoryused);
				break;
			}
			case pspFXEFFECT:
				break;
			case pspMAXRADIUS:
			{
				float fMin = 0.0f;
				float fMax = 1.0f;

				if (!(pParam->GetValue(fMax)))
					return false;

				// first test if there is a <distance> parameter
				FMOD::EventParameter* pParameter = 0;
				m_ExResult = m_pEvent->getParameter("distance", &pParameter);
				if (m_ExResult != FMOD_OK)
				{
					// FmodErrorOutput("get event parameter <distance> failed! ",
					// ILog::eMessage);
					return false;
				}

				// if there is one, then get the values through the properties because if INFOONLY
				// m_ExResult = m_pEvent->getPropertyByIndex(FMOD::EVENTPROPERTY_3D_MAXDISTANCE, &fMax);
				// if (m_ExResult != FMOD_OK)
				//{
				//	FmodErrorOutput("get 3D maxdistance property failed! ", ILog::eMessage);
				//	return false;
				//}

				if (pParameter)
				{
					m_ExResult = pParameter->getRange(&fMin, &fMax);
					if (m_ExResult != FMOD_OK)
					{
						FmodErrorOutput("get event parameter <distance> range failed! ",
						                ILog::eMessage);
						return false;
					}
				}

				pParam->SetValue(fMax);
				break;
			}
			case pspAMPLITUDE:
			{
				float fTemp;
				if (!(pParam->GetValue(fTemp)))
					return false;

				m_ExResult = m_pEvent->getVolume(&fTemp);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event property <amplitude> failed! ", ILog::eMessage);
					return false;
				}

				pParam->SetValue(fTemp);
				break;
			}
			case pspCONEINNERANGLE:
			{
				float fTemp;
				if (!(pParam->GetValue(fTemp)))
					return false;

				m_ExResult =
				    m_pEvent->getPropertyByIndex(FMOD_EVENTPROPERTY_3D_CONEINSIDEANGLE, &fTemp);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event property <cone inside angle> failed! ",
					                ILog::eMessage);
					return false;
				}

				pParam->SetValue(fTemp);
				break;
			}
			case pspCONEOUTERANGLE:
			{
				float fTemp;
				if (!(pParam->GetValue(fTemp)))
					return false;

				m_ExResult =
				    m_pEvent->getPropertyByIndex(FMOD_EVENTPROPERTY_3D_CONEOUTSIDEANGLE, &fTemp);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event property <cone outside angle> failed! ",
					                ILog::eMessage);
					return false;
				}

				pParam->SetValue(fTemp);
				break;
			}
			case pspREVERBWET:
			{
				float fTemp;
				if (!(pParam->GetValue(fTemp)))
					return false;

				m_ExResult = m_pEvent->getPropertyByIndex(FMOD_EVENTPROPERTY_REVERBWETLEVEL, &fTemp);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event property <reverb wet> failed! ", ILog::eMessage);
					return false;
				}
				// fTemp = 1 - (-fTemp/60.0f);

				pParam->SetValue(fTemp);
				break;
			}
			case pspREVERBDRY:
			{
				float fTemp;
				if (!(pParam->GetValue(fTemp)))
					return false;

				m_ExResult = m_pEvent->getPropertyByIndex(FMOD_EVENTPROPERTY_REVERBDRYLEVEL, &fTemp);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event property <reverb dry> failed! ", ILog::eMessage);
					return false;
				}
				// fTemp = 1 - (-fTemp/60.0f);

				pParam->SetValue(fTemp);
				break;
			}
			case pspSPREAD:
			{
				float fTemp;
				if (!(pParam->GetValue(fTemp)))
					return false;

				// first test if there is a <spread> parameter
				FMOD::EventParameter* pParameter = 0;
				m_ExResult = m_pEvent->getParameter("spread", &pParameter);
				if (m_ExResult != FMOD_OK)
				{
					// FmodErrorOutput("get event parameter <spread> failed! ", ILog::eMessage);
					return false;
				}

				if (pParameter)
					return true;

				break;
			}
			case pspSPAWNEDINSTANCES:
			{
				int32 nTemp;
				if (!(pParam->GetValue(nTemp)))
					return false;

				FMOD_EVENT_INFO Info;
				memset(&Info, 0, sizeof(FMOD_EVENT_INFO));

				m_ExResult = m_pEvent->getInfo(0, 0, &Info);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event info <spawned instances> failed! ", ILog::eMessage);
					return false;
				}

				pParam->SetValue(Info.instancesactive);
				break;
			}
			case pspCHANNELSUSED:
			{
				int32 nTemp;
				if (!(pParam->GetValue(nTemp)))
					return false;

				FMOD_EVENT_INFO Info;
				memset(&Info, 0, sizeof(FMOD_EVENT_INFO));

				m_ExResult = m_pEvent->getInfo(0, 0, &Info);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event info <channels used> failed! ", ILog::eMessage);
					return false;
				}

				pParam->SetValue(Info.channelsplaying);
				break;
			}
			default:
				break;
		}
	}
	else
	{
		if (eSemantics == pspLENGTHINMS)
		{
			pParam->SetValue(m_nLenghtInMs);
		}
	}

	if (eSemantics == pspEVENTNAME)
	{
		pParam->SetValue(m_sEventName);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CPlatformSoundFmodEx400Event::SetParamByType(enumPlatformSoundParamSemantics eSemantics, ptParam* pParam)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	if (!m_pEvent)
	{
		SQueuedParameter QParam;
		QParam.eSemantic = eSemantics;

		int32 nTemp = 0;
		if (pParam->GetValue(nTemp))
			QParam.nValue = nTemp;

		float fTemp = 0.0f;
		if (pParam->GetValue(fTemp))
			QParam.fValue = fTemp;

		bool bAlreadyFound = false;
		tvecQueuedParameterIter It2End = m_QueuedParameter.end();
		for (tvecQueuedParameterIter It2 = m_QueuedParameter.begin(); It2 != It2End; ++It2)
		{
			if ((*It2).eSemantic == QParam.eSemantic)
			{
				(*It2).fValue = QParam.fValue;
				(*It2).nValue = QParam.nValue;
				bAlreadyFound = true;
				break;
			}
		}

		if (!bAlreadyFound)
			m_QueuedParameter.push_back(QParam);

		return true;
	}

	switch (eSemantics)
	{
		case pspNONE:
			return false;
			break;
		case pspSOUNDTYPE:
			break;
		case pspSAMPLETYPE:
			break;
		case pspDISTANCE:
		{
			float fTemp = 0.0;
			if (!(pParam->GetValue(fTemp)))
				return false;

			FMOD::EventParameter* pParameter = 0;

			if (m_nDistanceIndex != -1)
			{
				m_ExResult = m_pEvent->getParameterByIndex(m_nDistanceIndex, &pParameter);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get event parameter <distance> failed! ", ILog::eMessage);
					return false;
				}

				m_ExResult = pParameter->setValue(fTemp);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("set event parameter value <distance> failed! ",
					                ILog::eMessage);
					return false;
				}
			}
			break;
		}
		case pspFREQUENCY:
			// TODO Check Pitch and stuff
			{
				int32 nTemp = 0;
				if (!(pParam->GetValue(nTemp)))
					return false;
				// m_pEvent->getParameter()
				// if (!CS_SetFrequency(m_nChannel,nTemp))
				return false;
				break;
			}
		case pspVOLUME:
		{
			// do not set the volume on infoonly events
			if (m_State == pssINFOONLY)
				return false;

			float fTemp = 0.0;
			if (!(pParam->GetValue(fTemp)))
				return false;

			fTemp *= abs(m_fMaxVolume);

			m_ExResult = m_pEvent->setVolume(fTemp);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("set event property <volume> failed! ", ILog::eMessage);
				return false;
			}
			break;
		}
		case pspISPLAYING:
			// dont support that
			break;
		case pspLOOPMODE:
		{
			// bool bTemp = false;
			// FMOD_MODE Mode = FMOD_LOOP_OFF;
			// if (!(pParam->GetValue(bTemp)))
			return false;
			// if (bTemp)
			//	Mode = FMOD_LOOP_NORMAL;
			// bool bPaused;
			// m_pExChannel->getPaused(&bPaused);
			// if (bPaused)
			//	m_pExChannel->setMode(Mode);
			// else
			//{
			//	m_pExChannel->setPaused(true);
			//	m_pExChannel->setMode(Mode);
			//	m_pExChannel->setPaused(false);
			// }
			break;
		}
		case pspPAUSEMODE:
		{
			// do not pause on infoonly events
			if (m_State == pssINFOONLY)
				return false;

			bool bTemp = false;
			if (!(pParam->GetValue(bTemp)))
				return false;

			m_ExResult = m_pEvent->setPaused(bTemp);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("set event property <paused> failed! ", ILog::eMessage);
				return false;
			}

			if (bTemp)
				m_State = pssPAUSED;

			break;
		}
		case pspSPEAKERPAN:
			return false;
			break;
		case pspSPEAKER3DPAN:
			return false;
			break;
		case pspSAMPLEPOSITION:
		{
			int32 nTemp = 0;
			if (!(pParam->GetValue(nTemp)))
				return false;
			// if (!CS_SetCurrentPosition(m_nChannel, nTemp))
			return false;
			break;
		}
		case psp3DPOSITION:
		{
			Vec3 VecTemp;
			if (!(pParam->GetValue(VecTemp)))
				return false;
			// axis swap!
			FMOD_VECTOR pos = {VecTemp.x, VecTemp.z, VecTemp.y};
			m_ExResult = m_pEvent->set3DAttributes(&pos, NULL);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("set event property <3d position> failed! ", ILog::eMessage);
				return false;
			}

			break;
		}
		case psp3DVELOCITY:
		{
			Vec3 VecTemp;
			if (!(pParam->GetValue(VecTemp)))
				return false;
			// axis swap!
			FMOD_VECTOR vel = {VecTemp.x, VecTemp.z, VecTemp.y};
			m_ExResult = m_pEvent->set3DAttributes(NULL, &vel);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("set event property <3d velocity> failed! ", ILog::eMessage);
				return false;
			}

			break;
		}
		case pspPRIORITY:
			break;
		case pspFXEFFECT:
			break;
		case pspAMPLITUDE:
			break;
		case pspREVERBWET:
		{
			// do not set the reverb on infoonly events
			if (m_State == pssINFOONLY)
				return false;

			float fTemp;
			if (!(pParam->GetValue(fTemp)))
				return false;

			m_ExResult = m_pEvent->setPropertyByIndex(FMOD_EVENTPROPERTY_REVERBWETLEVEL, &fTemp);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("set event property <reverb wet> failed! ", ILog::eMessage);
				return false;
			}
			break;
		}
		case pspREVERBDRY:
		{
			// do not set the reverb on infoonly events
			if (m_State == pssINFOONLY)
				return false;

			float fTemp;
			if (!(pParam->GetValue(fTemp)))
				return false;

			m_ExResult = m_pEvent->setPropertyByIndex(FMOD_EVENTPROPERTY_REVERBDRYLEVEL, &fTemp);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("set event property <reverb dry> failed! ", ILog::eMessage);
				return false;
			}
			break;
		}
		case pspSTEALBEHAVIOUR:
		{
			int32 nTemp = 0;
			if (!(pParam->GetValue(nTemp)))
				return false;

			m_ExResult = m_pEvent->setPropertyByIndex(FMOD_EVENTPROPERTY_MAX_PLAYBACKS_BEHAVIOR, &nTemp);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("set event property <steal behaviour> failed! ", ILog::eMessage);
				return false;
			}
			break;
		}
		default:
			break;
	}
	return true;
}

// Gets and Sets Parameter defined by string and float value
int CPlatformSoundFmodEx400Event::GetParamByName(const char* sParameter, float* fValue, bool bOutputWarning)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	int nIndex = -1;

	if (m_pEvent && sParameter)
	{
		FMOD::EventParameter* pParameter = 0;
		m_ExResult = m_pEvent->getParameter((char*)sParameter, &pParameter);
		if (m_ExResult != FMOD_OK)
		{
			if (bOutputWarning)
			{
				string sPotentialError = "get event parameter <";
				sPotentialError += sParameter;
				sPotentialError = sPotentialError + "> failed!";
				FmodErrorOutput(sPotentialError.c_str(), ILog::eMessage);
			}
			return nIndex;
		}

		if (pParameter)
		{
			m_ExResult = pParameter->getInfo(&nIndex, NULL);
			if (m_ExResult != FMOD_OK)
			{
				if (bOutputWarning)
				{
					string sPotentialError = "get event parameter <";
					sPotentialError += sParameter;
					sPotentialError = sPotentialError + "> info failed!";
					FmodErrorOutput(sPotentialError.c_str(), ILog::eMessage);
				}
			}

			m_ExResult = pParameter->getValue(fValue);
			if (m_ExResult != FMOD_OK)
			{
				if (bOutputWarning)
				{
					string sPotentialError = "get event parameter <";
					sPotentialError += sParameter;
					sPotentialError = sPotentialError + "> value failed!";
					FmodErrorOutput(sPotentialError.c_str(), ILog::eMessage);
				}
			}
		}
	}
	else
	{
		int nIndex = 0;
		tmapParameterNamesIterConst It = ParameterNames.find(CONST_TEMP_STRING(sParameter));
		if (It != ParameterNames.end())
		{
			*fValue = 0.0f;
			return It->second;
		}

		nIndex = -1;
		if (bOutputWarning)
		{
			string sPotentialError = "get event parameter <";
			sPotentialError += sParameter;
			sPotentialError = sPotentialError + "> value failed!";
			FmodErrorOutput(sPotentialError.c_str(), ILog::eMessage);
		}
	}

	return nIndex;
}

int CPlatformSoundFmodEx400Event::SetParamByName(const char* sParameter, float fValue, bool bOutputWarning)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	SQueuedParameter QParam;
	QParam.fValue = fValue;
	bool bFoundInName = false;

	assert(sParameter[0]);

	if (!sParameter[0])
		return QParam.nNameIndex;

	// get the NameIndex and the EventIndex
	tmapParameterNamesIterConst It = ParameterNames.find(CONST_TEMP_STRING(sParameter));
	if (It != ParameterNames.end())
	{
		// check if it is accessable by index
		if (It->second < m_ParameterIndex.size())
		{
			QParam.nEventIndex = m_ParameterIndex[It->second];
			QParam.nNameIndex = It->second;
			bFoundInName = true;
		}
	}

	// Queue it if no event is there
	if (!m_pEvent)
	{
		bool bAlreadyFound = false;
		tvecQueuedParameterIter It2End = m_QueuedParameter.end();
		for (tvecQueuedParameterIter It2 = m_QueuedParameter.begin(); It2 != It2End; ++It2)
		{
			if ((*It2).nNameIndex == QParam.nNameIndex)
			{
				(*It2).fValue = fValue;
				bAlreadyFound = true;
				break;
			}
		}

		if (!bAlreadyFound && bFoundInName)
			m_QueuedParameter.push_back(QParam);
	}
	else
	{
		FMOD::EventParameter* pParameter = 0;

		if (QParam.nEventIndex != -1)
			m_ExResult = m_pEvent->getParameterByIndex(QParam.nEventIndex, &pParameter);
		else
			m_ExResult = m_pEvent->getParameter((char*)sParameter, &pParameter);

		if (m_ExResult != FMOD_OK)
		{
			if (bOutputWarning)
			{
				string sPotentialError = "set event parameter <";
				sPotentialError += sParameter;
				sPotentialError = sPotentialError + "> failed!";
				FmodErrorOutput(sPotentialError.c_str(), ILog::eWarning);
			}

			return QParam.nNameIndex;
		}

		if (pParameter)
		{
			m_ExResult = pParameter->setValue(fValue);
			if (m_ExResult != FMOD_OK)
			{
				if (bOutputWarning)
				{
					string sPotentialError = "set event parameter <";
					sPotentialError += sParameter;
					sPotentialError = sPotentialError + "> value failed!";
					FmodErrorOutput(sPotentialError.c_str(), ILog::eMessage);
				}
			}

			// all go and set
		}
	}

	return QParam.nNameIndex;
}

// Gets and Sets Parameter defined by index and float value
bool CPlatformSoundFmodEx400Event::GetParamByIndex(int nIndex, float* fValue, bool bOutputWarning)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	if (m_pEvent)
	{
		FMOD::EventParameter* pParameter = 0;
		m_ExResult = m_pEvent->getParameterByIndex(nIndex, &pParameter);
		if (m_ExResult != FMOD_OK)
		{
			if (bOutputWarning)
			{
				string sPotentialError = "get event parameter <index:" + nIndex;
				// sPotentialError += sParameter;
				sPotentialError = sPotentialError + "> failed!";
				FmodErrorOutput(sPotentialError.c_str(), ILog::eMessage);
			}
			return false;
		}

		if (pParameter)
		{
			m_ExResult = pParameter->getValue(fValue);
			if (m_ExResult != FMOD_OK)
			{
				if (bOutputWarning)
				{
					string sPotentialError = "set event parameter <index:" + nIndex;
					// sPotentialError += sParameter;
					sPotentialError = sPotentialError + "> value failed!";
					FmodErrorOutput(sPotentialError.c_str(), ILog::eMessage);
				}
				return false;
			}
			return true;
		}
	}
	else
	{
		if (nIndex >= 0 && nIndex < ParameterNames.size())
		{
			*fValue = 0.0f;
			return true;
		}

		if (bOutputWarning)
		{
			string sPotentialError = "get event parameter < Index:";
			sPotentialError += nIndex;
			sPotentialError = sPotentialError + "> value failed!";
			FmodErrorOutput(sPotentialError.c_str(), ILog::eMessage);
		}
	}
	return false;
}

bool CPlatformSoundFmodEx400Event::SetParamByIndex(int nIndex, float fValue, bool bOutputWarning)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	if (!m_pEvent)
	{
		SQueuedParameter QParam;
		QParam.nNameIndex = nIndex;
		QParam.fValue = fValue;

		bool bAlreadyFound = false;
		tvecQueuedParameterIter It2End = m_QueuedParameter.end();
		for (tvecQueuedParameterIter It2 = m_QueuedParameter.begin(); It2 != It2End; ++It2)
		{
			if ((*It2).nNameIndex == QParam.nNameIndex)
			{
				(*It2).fValue = fValue;
				bAlreadyFound = true;
				break;
			}
		}

		if (!bAlreadyFound)
			m_QueuedParameter.push_back(QParam);

		return true;
	}

	if (m_pEvent)
	{
		FMOD::EventParameter* pParameter = 0;
		int nEventIndex = m_ParameterIndex[nIndex];
		m_ExResult = m_pEvent->getParameterByIndex(nEventIndex, &pParameter);
		if (m_ExResult != FMOD_OK)
		{
			string sPotentialError = "set event parameter <index:" + nIndex;
			// sPotentialError += sParameter;
			sPotentialError = sPotentialError + "> failed!";
			FmodErrorOutput(sPotentialError.c_str(), ILog::eWarning);
			return false;
		}

		if (pParameter)
		{
			m_ExResult = pParameter->setValue(fValue);
			if (m_ExResult != FMOD_OK)
			{
				string sPotentialError = "set event parameter <index:" + nIndex;
				// sPotentialError += sParameter;
				sPotentialError = sPotentialError + "> value failed!";
				FmodErrorOutput(sPotentialError.c_str(), ILog::eMessage);
			}
			return false;
		}
		return true;
	}
	return false;
}

bool CPlatformSoundFmodEx400Event::IsInCategory(const char* sCategory)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	if (!m_pEvent)
		return false;

	FMOD::EventCategory* pCat = NULL;
	m_ExResult = m_pEvent->getCategory(&pCat);
	if (m_ExResult == FMOD_OK && pCat)
	{
		int nIndex = 0;
		char* sName = 0;

		pCat->getInfo(&nIndex, &sName);

		if (strcmp(sCategory, sName) == 0)
			return true;
	}

	return false;
}

void CPlatformSoundFmodEx400Event::GetMemoryUsage(ICrySizer* pSizer)
{
	size_t nSize = sizeof(*this);

	if (m_pEvent)
	{
		int nIndex;
		char* name;
		// char **soundbanks;
		TFixedResourceName sBankName;
		FMOD_EVENT_INFO Info;
		memset(&Info, 0, sizeof(FMOD_EVENT_INFO));

		m_ExResult = m_pEvent->getInfo(&nIndex, &name, &Info);
		if (m_ExResult == FMOD_OK)
		{
			int i = 0;
			while (Info.wavebanknames[i])
			{
				sBankName = Info.wavebanknames[i];

				if (m_pSound->GetSoundBufferPtr())
					sBankName =
					    m_pSound->GetSoundBufferPtr()->GetProps()->sFilePath + sBankName + ".fsb";

				pSizer->GetResourceCollector().AddResource(sBankName.c_str(), 0xffffffff);
				++i;
			}
		}
	}
	// nSize += m_pSoundAssetManager->GetMemUsage();
	pSizer->AddObject(this, nSize);
}

//////////////////////////////////////////////////////////////////////////
bool CPlatformSoundFmodEx400Event::PlaySound(bool bStartPaused)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	// m_ExResult = m_pEvent->set3DOcclusion(1.0f, 0.0f);

	if (m_pEvent && m_State != pssINFOONLY)
	{
		// m_ExResult = m_pEvent->setVolume(0.0f);

		if (m_State != pssPAUSED && m_pSound->m_pSSys->g_nProfiling > 1)
		{
			float f0 = gEnv->pTimer->GetAsyncCurTime();
			{
				FRAME_PROFILER("FMOD-StartEvent", GetISystem(), PROFILE_SOUND);
				m_ExResult = m_pEvent->start();
			}

			float f1 = gEnv->pTimer->GetAsyncCurTime();
			CryLog(" <Sound Profile starting> %s %.2f", m_pSound->GetName(), (f1 - f0) * 1000.0f);
		}
		else
		{
			FRAME_PROFILER("FMOD-StartEvent", GetISystem(), PROFILE_SOUND);
			// SetParam("distance", 0.0f);
			m_ExResult = m_pEvent->start();
		}

		if (m_ExResult != FMOD_OK || !m_pEvent)
		{
			// Callbacks set m_pEvent to zero or something else went wrong
			m_ExResult = FMOD_ERR_EVENT_FAILED; // set error so output works
			FmodErrorOutput("start event failed! ", ILog::eError);
			return false;
		}

		FMOD_EVENT_STATE EventState;
		m_ExResult = m_pEvent->getState(&EventState);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get event state failed! ", ILog::eError);
			return false;
		}

		if (EventState & FMOD_EVENT_STATE_PLAYING)
			m_State = pssPLAYING;

		if (EventState & FMOD_EVENT_STATE_LOADING)
			m_State = pssLOADING;

		// if (bStartPaused)
		//{
		//	m_ExResult = m_pEvent->setPaused(true);
		//	m_State = pssPAUSED;
		// }
		// else
		//{
		//	m_ExResult = m_pEvent->setPaused(false);
		//	m_State = pssPLAYING;
		// }

		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPlatformSoundFmodEx400Event::StopSound()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	if (m_pEvent && m_State != pssINFOONLY && m_State != pssSTOPPED)
	{
		// m_ExResult = m_pEvent->setVolume(0.0f);
		bool bAtOnce =
		    m_pSound->m_pSSys->g_nStopSoundsImmediately != 0 || m_pSound->m_eStopMode == ESoundStopMode_AtOnce;
		if (bAtOnce || m_pSound->m_eStopMode != ESoundStopMode_OnSyncPoint)
		{
			m_ExResult = m_pEvent->setPaused(false);
			m_ExResult = m_pEvent->stop(bAtOnce);
		}
		else
		{
			m_State = pssWAITINGFORSYNC;
			return true;
		}

		// FMOD::EVENT_INFO Info;
		// m_ExResult = m_pEvent->getInfo(0, 0, &Info);

		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("stop event failed! ", ILog::eError);
			return false;
		}
		m_State = pssNONE;
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPlatformSoundFmodEx400Event::FreeSoundHandle()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	if (m_pEvent)
	{
		// deactivate callback for now
		if (m_State != pssINFOONLY)
			m_ExResult = m_pEvent->setCallback(NULL, INVALID_SOUNDID); // OnCallBack

		// TODO Generic approach to Playmodes (Play/Pause/Stop)
		// Stopping a Stream does not stop the streaming itself !

		// To actually free a channel FreeSoundHandle has to be called.
		//  if it was a stream, or if it was the only ref to the Asset, the Asset will stop.
		StopSound();
		// m_pEvent->release();
		m_pEvent = NULL;
	}

	m_State = pssNONE;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CPlatformSoundFmodEx400Event::CreateSound(tAssetHandle AssetHandle, SSoundSettings SoundSettings)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	// TODO Verify that we dont need any StopChannel or (m_nChannel != CS_FREE) checks at all
	CSoundBuffer* SoundBuf = NULL;
	SoundBuf = m_pSound->GetSoundBufferPtr();

	ptParamF32 fParam(0.0f);
	ptParamINT32 nParam(0);

	if (!SoundBuf)
		return false;

	bool bSoundBufLoaded = (SoundBuf->Loaded() || m_pSound->m_pEventGroupBuffer);
	if (AssetHandle && SoundBuf && bSoundBufLoaded)
	{
		switch (SoundBuf->GetProps()->eBufferType)
		{
			case btSTREAM:
			{
				return false;
				break;
			}
			case btNETWORK:
			{

				return false;
				break;
			}
			case btEVENT:
			case btSAMPLE:
			{
				if (m_pEvent && m_State == pssINFOONLY && !SoundSettings.bLoadData)
					return true;

				if (m_pEvent && m_State != pssINFOONLY && SoundSettings.bLoadData)
					return true;

				m_pEvent = NULL;
				m_State = pssNONE;
				FMOD::EventGroup* pEventGroup = NULL;

				if (SoundBuf->GetProps()->eBufferType == btSAMPLE)
					pEventGroup =
					    (FMOD::EventGroup*)m_pSound->m_pEventGroupBuffer->GetAssetHandle();
				else
					pEventGroup = (FMOD::EventGroup*)AssetHandle;

				if (!pEventGroup)
				{
					int nReallyBad = 1;
					return false;
				}

				// first get the INFOONLY event retrieve info
				if (!m_pEvent && m_State != pssINFOONLY)
				{
					m_ExResult = pEventGroup->getEvent((char*)m_sEventName.c_str(),
					                                   FMOD_EVENT_INFOONLY, &m_pEvent);
					if (m_ExResult != FMOD_OK)
					{
						FmodErrorOutput("get info event failed! (Event does not exist!) ",
						                ILog::eMessage);
						m_State = pssNONE;
						return false;
					}

					m_State = pssINFOONLY;

					// r�trieve all availible parameters and store their name indicies
					FMOD::EventParameter* pParameter = 0;
					int nIndex = 0;

					do
					{
						char* name;
						pParameter = NULL;
						m_ExResult = m_pEvent->getParameterByIndex(nIndex, &pParameter);
						if (pParameter)
						{
							m_ExResult = pParameter->getInfo(0, &name);
							if (m_ExResult != FMOD_OK)
							{
								FmodErrorOutput("get event info failed! ",
								                ILog::eMessage);
								return false;
							}

							tmapParameterNamesIterConst It =
							    ParameterNames.find(CONST_TEMP_STRING(name));
							if (It == ParameterNames.end())
							{
								int nNum = ParameterNames.size();
								ParameterNames[name] = nNum;

								// make sure the Parameterindex has the right size
								m_ParameterIndex.resize(nNum + 1);

								// enter new index
								m_ParameterIndex[nNum] = nIndex;
							}
							else
							{
								// enter new index
								m_ParameterIndex[It->second] = nIndex;
							}

							// shortcut for distance parameter
							if (strcmp(name, "distance") == 0)
								m_nDistanceIndex = nIndex;
						}

						++nIndex;
					}
					while (m_ExResult == FMOD_OK);

					// more info stuff

					fParam.SetValue(m_fMaxRadius);
					GetParamByType(pspMAXRADIUS, &fParam);
					fParam.GetValue(m_fMaxRadius);

					float fTemp = 0.0f;
					GetParamByType(pspVOLUME, &fParam);
					fParam.GetValue(fTemp);
					if (m_fMaxVolume < 0)
						m_fMaxVolume = fTemp;
					assert(m_fMaxVolume != 0.0f);

					FMOD_EVENT_INFO Info;
					memset(&Info, 0, sizeof(FMOD_EVENT_INFO));

					m_ExResult = m_pEvent->getInfo(0, 0, &Info);
					if (m_ExResult != FMOD_OK)
					{
						FmodErrorOutput("get event info <length ms> failed! ", ILog::eMessage);
						return false;
					}
					m_nCurrentSpawns = Info.instancesactive;
					m_nLenghtInMs = Info.lengthms;

					m_ExResult = m_pEvent->getPropertyByIndex(FMOD_EVENTPROPERTY_MAX_PLAYBACKS,
					                                          &m_nMaxSpawns);
					if (m_ExResult != FMOD_OK)
					{
						FmodErrorOutput("get event property <max playbacks> failed! ",
						                ILog::eMessage);
						return false;
					}

					// EVENTPROPERTY_MAX_PLAYBACKS
					FMOD_MODE Mode;
					m_ExResult = m_pEvent->getPropertyByIndex(FMOD_EVENTPROPERTY_MODE, &Mode);
					if (m_ExResult != FMOD_OK)
					{
						FmodErrorOutput("get Property Mode failed!", ILog::eError);
						return false;
					}

					int nPriority = 0;
					m_ExResult =
					    m_pEvent->getPropertyByIndex(FMOD_EVENTPROPERTY_PRIORITY, &nPriority);
					if (m_ExResult != FMOD_OK)
					{
						FmodErrorOutput("get Property Priority failed!", ILog::eError);
						return false;
					}
				}

				// now set all parameters to the info only event
				if (m_pEvent && m_State == pssINFOONLY)
				{

					// set queued Parameters
					int nNum = m_QueuedParameter.size();
					for (int i = 0; i < nNum; ++i)
					{
						if (m_QueuedParameter[i].nNameIndex != -1)
						{
							SetParamByIndex(m_QueuedParameter[i].nNameIndex,
							                m_QueuedParameter[i].fValue, false);
							continue;
						}

						if (m_QueuedParameter[i].eSemantic != pspNONE)
						{
							nParam.SetValue(m_QueuedParameter[i].nValue);
							fParam.SetValue(m_QueuedParameter[i].fValue);
							SetParamByType(m_QueuedParameter[i].eSemantic, &nParam);
							SetParamByType(m_QueuedParameter[i].eSemantic, &fParam);
							continue;
						}

						// should never go here
						if (m_QueuedParameter[i].nNameIndex > -1)
						{
							SetParamByIndex(m_QueuedParameter[i].nNameIndex,
							                m_QueuedParameter[i].fValue, false);
							continue;
						}
					}
				}

				// now spawn the real event if needed
				if (SoundSettings.bLoadData)
				{
					m_pEvent = NULL;
					m_State = pssNONE;

					FMOD_EVENT_MODE EventMode = FMOD_EVENT_DEFAULT;

					if (SoundSettings.bLoadNonBlocking &&
					    m_pSound->GetSoundSystemPtr()->g_nLoadNonBlocking > 0)
						EventMode = FMOD_EVENT_NONBLOCKING;

					assert(pEventGroup);
					if (pEventGroup)
					{
						{
							CSoundSystem* pSys = m_pSound->GetSoundSystemPtr();
							if (pSys && (pSys->g_nProfiling > 1))
							{
								float f0 = gEnv->pTimer->GetAsyncCurTime();
								{
									m_ExResult = pEventGroup->getEvent(
									    (char*)m_sEventName.c_str(), EventMode,
									    &m_pEvent);
								}

								float f1 = gEnv->pTimer->GetAsyncCurTime();
								CryLog("<Sound Profile Getting Event> %s %.2f",
								       m_sEventName.c_str(), (f1 - f0) * 1000.0f);
							}
							else
							{
								m_ExResult = pEventGroup->getEvent(
								    (char*)m_sEventName.c_str(), EventMode, &m_pEvent);
							}
						}
					}
					else
					{
						m_pEvent = NULL;
						return false;
					}

					if (m_ExResult != FMOD_OK)
					{
						if (m_ExResult == FMOD_ERR_EVENT_FAILED)
						{
							m_State = pssNONE;
							return false;
						}

						FmodErrorOutput(
						    "create event failed! (Typo in event name or event not available?)",
						    ILog::eError);
						return false;
					}

					ptParamINT32 nParam(0);
					ptParamF32 fParam(0.0f);

					// set queued Parameters again TODO check if that is needed
					for (int i = 0; i < m_QueuedParameter.size(); ++i)
					{
						if (m_QueuedParameter[i].nNameIndex != -1)
						{
							SetParamByIndex(m_QueuedParameter[i].nNameIndex,
							                m_QueuedParameter[i].fValue, false);
							continue;
						}

						if (m_QueuedParameter[i].eSemantic != pspNONE)
						{
							nParam.SetValue(m_QueuedParameter[i].nValue);
							fParam.SetValue(m_QueuedParameter[i].fValue);
							SetParamByType(m_QueuedParameter[i].eSemantic, &nParam);
							SetParamByType(m_QueuedParameter[i].eSemantic, &fParam);
							continue;
						}

						// should never go here
						if (m_QueuedParameter[i].nNameIndex > -1)
						{
							SetParamByIndex(m_QueuedParameter[i].nNameIndex,
							                m_QueuedParameter[i].fValue, false);
							continue;
						}
					}
					////

					FMOD_EVENT_STATE EventState;
					m_ExResult = m_pEvent->getState(&EventState);
					if (m_ExResult != FMOD_OK)
					{
						FmodErrorOutput("get event state failed! ", ILog::eError);
						return false;
					}

					if (EventState & FMOD_EVENT_STATE_READY)
						m_State = pssREADY;

					if (EventState & FMOD_EVENT_STATE_LOADING)
						m_State = pssLOADING;

					if (EventState & FMOD_EVENT_STATE_PLAYING)
						m_State = pssPLAYING;

					if (EventState & FMOD_EVENT_STATE_INFOONLY)
						m_State = pssINFOONLY;

					if (m_State != pssINFOONLY)
					{
						// activate callback for now
						m_ExResult = m_pEvent->setCallback(
						    OnCallBack,
						    reinterpret_cast<void*>(static_cast<uintptr_t>(m_pSound->GetId())));
					}

					if (m_ExResult != FMOD_OK)
					{
						FmodErrorOutput("set callback on event failed!", ILog::eError);
					}

					break;
				}
			}
		}
	}
	else
	{
		return false;
	}

	if (m_pSound->GetSoundSystemPtr()->g_nProfiling > 0)
	{
		FMOD_EVENT_INFO Info;
		memset(&Info, 0, sizeof(FMOD_EVENT_INFO));

		m_ExResult = m_pEvent->getInfo(0, 0, &Info);
		if (m_ExResult == FMOD_OK)
		{
			int i = 0;
			while (Info.wavebanknames[i])
			{
				IWavebank* pWavebank = m_pSound->GetSoundSystemPtr()->GetIAudioDevice()->GetWavebank(
				    Info.wavebanknames[i]);
				if (pWavebank && strcmp(pWavebank->GetPath(), "") == 0)
					pWavebank->SetPath(
					    m_pSound->GetSoundBufferPtr()->GetProps()->sFilePath.c_str());
				++i;
			}
		}
	}

	return true;
}
