#include "CryCommon/CryCore/CrySizer.h"
#include "CryCommon/CrySystem/ICryPak.h"
#include "CryCommon/CrySystem/ITimer.h"

#include "AudioDeviceFmodEx400.h"
#include "Sound.h"
#include "SoundBufferFmodEx400Event.h"
#include "SoundSystem.h"

CSoundBufferFmodEx400Event::CSoundBufferFmodEx400Event(const SSoundBufferProps& Props, FMOD::System* pCSEX)
    : CSoundBuffer(Props)
{
	m_pCSEX = pCSEX;
	m_ExResult = FMOD_OK;
	m_Props.nFlags = Props.nFlags;
	m_Props.nPrecacheFlags = Props.nPrecacheFlags;
}

CSoundBufferFmodEx400Event::~CSoundBufferFmodEx400Event()
{
	// if (m_pReadStream != NULL)
	//{
	//	m_pReadStream->Abort();
	//	m_pReadStream = NULL;
	// }
	UnloadData(sbUNLOAD_ALL_DATA);

	// Tell Sounds that their soundbuffers are invalid now
	for (TBufferLoadReqVecIt It = m_vecLoadReq.begin(); It != m_vecLoadReq.end(); ++It)
		(*It)->OnBufferDelete();
}

void CSoundBufferFmodEx400Event::FmodErrorOutput(const char* sDescription, ILog::ELogType LogType)
{
	switch (LogType)
	{
		case ILog::eWarning:
			// gEnv->pLog->Log("[Warning] <Sound> SBE: (%s) %s (%d) %s\n", m_Props.sName.c_str(),
			// sDescription, m_ExResult, FMOD_ErrorString(m_ExResult));
			gEnv->pLog->Log("[Warning] <Sound> SBE: (%s) %s (%d) %s\n", m_Props.sName.c_str(), sDescription,
			                m_ExResult, FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eError:
			// gEnv->pLog->Log("[Error] <Sound> SBE: (%s) %s (%d) %s\n", m_Props.sName.c_str(),
			// sDescription, m_ExResult, FMOD_ErrorString(m_ExResult));
			gEnv->pLog->Log("[Error] <Sound> SBE: (%s) %s (%d) %s\n", m_Props.sName.c_str(), sDescription,
			                m_ExResult, FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eMessage:
			gEnv->pLog->Log("<Sound> SBE: (%s) %s (%d) %s\n", m_Props.sName.c_str(), sDescription,
			                m_ExResult, FMOD_ErrorString(m_ExResult));
			// gEnv->pLog->Log("<Sound> SBE: (%s) %s (%d) %s\n", m_Props.sName.c_str(), sDescription,
			// m_ExResult, FMOD_ErrorString(m_ExResult));
			break;
		default:
			break;
	}
}

uint32 CSoundBufferFmodEx400Event::GetMemoryUsed()
{
	size_t nSize = sizeof(*this);

	if (!m_BufferData)
		return nSize;

	FMOD::EventGroup* pEventGroup = (FMOD::EventGroup*)m_BufferData;

	int nMemUsed = 0;
	int nNumEvents = 0;
	m_ExResult = pEventGroup->getNumEvents(&nNumEvents);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("get group numevents failed! ", ILog::eError);
		return nSize;
	}

	for (int i = 0; i < nNumEvents; ++i)
	{
		FMOD::Event* pEvent = 0;
		m_ExResult = pEventGroup->getEventByIndex(i, FMOD_EVENT_INFOONLY, &pEvent);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get event by index failed! ", ILog::eError);
			return nSize;
		}

		FMOD_EVENT_INFO EventInfo;
		memset(&EventInfo, 0, sizeof(FMOD_EVENT_INFO));

		m_ExResult = pEvent->getInfo(0, 0, &EventInfo);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get event info failed! ", ILog::eError);
			return nSize;
		}

		nMemUsed += EventInfo.memoryused;
	}

	return nMemUsed;
}

// compute memory-consumption, returns size in Bytes
uint32 CSoundBufferFmodEx400Event::GetMemoryUsage(class ICrySizer* pSizer)
{

	if (pSizer)
	{
		if (!pSizer->Add(*this))
			return 0;
	}

	return sizeof(*this);
}

void CSoundBufferFmodEx400Event::LogDependencies()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	if (!m_BufferData)
		return;

	FMOD::EventGroup* pEventGroup = (FMOD::EventGroup*)m_BufferData;
	int nNumEvents = 0;

	m_ExResult = pEventGroup->getNumEvents(&nNumEvents);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("create group num. events failed! ", ILog::eWarning);
		return;
	}

	for (int i = 0; i < nNumEvents; ++i)
	{
		FMOD::Event* pEvent = 0;
		m_ExResult = pEventGroup->getEventByIndex(i, FMOD_EVENT_INFOONLY, &pEvent);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get event by index failed! ", ILog::eError);
			break;
		}

		FMOD_EVENT_INFO EventInfo;
		memset(&EventInfo, 0, sizeof(FMOD_EVENT_INFO));

		m_ExResult = pEvent->getInfo(0, 0, &EventInfo);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get event info failed! ", ILog::eError);
			break;
		}

		int w = 0;
		while (EventInfo.wavebanknames[w])
		{
			TFixedResourceName sWavebankName;
			sWavebankName.assign(EventInfo.wavebanknames[w], strlen(EventInfo.wavebanknames[w]));
			sWavebankName += ".fsb";
			sWavebankName = m_Props.sFilePath + sWavebankName;

			if (m_pSoundSystem->g_nDebugSound > 1)
				CryLog(" <Dependency> %s ", sWavebankName.c_str());

			FILE* pFile = gEnv->pCryPak->FOpen(sWavebankName.c_str(), "rbx");
			if (pFile)
				gEnv->pCryPak->FClose(pFile);

			++w;
		}
	}
}

// loads a event sound
tAssetHandle CSoundBufferFmodEx400Event::LoadAsEvent(const char* AssetName)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SOUND);

	CSoundSystem::g_sFileName = AssetName;

	m_pSoundSystem->ParseGroupString(CSoundSystem::g_sFileName, CSoundSystem::g_sProjectPath,
	                                 CSoundSystem::g_sProjectName, CSoundSystem::g_sGroupName,
	                                 CSoundSystem::g_sEventName, CSoundSystem::g_sKeyName);

	m_Props.sFilePath = "Game/";
	m_Props.sFilePath += CSoundSystem::g_sProjectPath;

	CSoundSystem::g_sProjectName += ".fev";

	m_Props.sResourceFile = m_Props.sFilePath;
	m_Props.sResourceFile += CSoundSystem::g_sProjectName;

	FMOD::EventGroup* pEventGroup = 0;
	CAudioDeviceFmodEx400* pAudioDeviceFmodEx400 = (CAudioDeviceFmodEx400*)m_pSoundSystem->GetIAudioDevice();
	FMOD::EventSystem* pEventSystem = (FMOD::EventSystem*)pAudioDeviceFmodEx400->GetEventSystem();
	FMOD::EventProject* pProject = NULL;

	if (m_pSoundSystem->g_nProfiling > 1)
	{
		float f0 = gEnv->pTimer->GetAsyncCurTime();
		{
			FRAME_PROFILER("FMOD-LoadProject", GetISystem(), PROFILE_SOUND);
			pProject = pAudioDeviceFmodEx400->LoadProjectFile(CSoundSystem::g_sProjectPath.c_str(),
			                                                  CSoundSystem::g_sProjectName.c_str());
		}

		float f1 = gEnv->pTimer->GetAsyncCurTime();
		CryLog(" <Sound Profile Loading Project File> %s %.2f", m_Props.sName.c_str(), (f1 - f0) * 1000.0f);
	}
	else
	{
		pProject = pAudioDeviceFmodEx400->LoadProjectFile(CSoundSystem::g_sProjectPath.c_str(),
		                                                  CSoundSystem::g_sProjectName.c_str());
	}

	if (pProject && pEventSystem)
	{
		bool bLoadGroup = false;
		if (m_Props.nPrecacheFlags & FLAG_SOUND_PRECACHE_LOAD_GROUP)
			bLoadGroup = true;
		else
			bLoadGroup = false;

		// bPreload = true;
		bool bCacheMemory = false;

		// try to fix bug
		// bLoadGroup = true;

		if (m_pSoundSystem->g_nProfiling > 1)
		{
			float f0 = gEnv->pTimer->GetAsyncCurTime();
			m_ExResult =
			    pProject->getGroup((char*)CSoundSystem::g_sGroupName.c_str(), bCacheMemory, &pEventGroup);

			float f1 = gEnv->pTimer->GetAsyncCurTime();
			CryLog("<Sound Profile Getting Group> %s %.2f", m_Props.sName.c_str(), (f1 - f0) * 1000.0f);
		}
		else
		{
			m_ExResult =
			    pProject->getGroup((char*)CSoundSystem::g_sGroupName.c_str(), bCacheMemory, &pEventGroup);
		}

		// precache flag has the same result as using
		// pEventGroup->LoadEventData(EVENT_RESOURCE_STREAMS_AND_SAMPLES);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("create event group failed! ", ILog::eWarning);
			return (NULL);
		}

		int nNumEvents = 0;
		m_ExResult = pEventGroup->getNumEvents(&nNumEvents);

		m_EventTimeouts.clear();
		m_EventTimeouts.resize(nNumEvents);

		if (bLoadGroup)
		{
			m_ExResult = pEventGroup->loadEventData(FMOD_EVENT_RESOURCE_SAMPLES, FMOD_EVENT_DEFAULT);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("event group loading failed! ", ILog::eWarning);
				return (NULL);
			}
		}
		// m_ExResult = pEventGroup->loadEventData(FMOD::EVENT_RESOURCE_SAMPLES);

		// else
		// m_ExResult = pEventGroup->loadEventData(FMOD::EVENT_RESOURCE_STREAMS);

		// else gEnv->pLog->Log("Error: Sound-FmodEx-SoundBuffer-Event: %s %s \n", sEventName, "created
		// succesfully.");

		// debugging getInfo()
		// int nIndex = 0;
		// char *sName = 0;
		// pEventGroup->getInfo( &nIndex,&sName );
	}

	return (pEventGroup);
}

// Gets and Sets Parameter defined in the enumAssetParam list
bool CSoundBufferFmodEx400Event::GetParam(enumAssetParamSemantics eSemantics, ptParam* pParam) const
{
	switch (eSemantics)
	{
		case apASSETNAME:
		{
			string sTemp;
			if (!(pParam->GetValue(sTemp)))
				return false;
			sTemp = m_Props.sName.c_str();
			pParam->SetValue(sTemp);
		}
		break;
		case apASSETTYPE:
		{
			int32 nTemp = 0;
			if (!(pParam->GetValue(nTemp)))
				return false;
			nTemp = m_Props.eBufferType;
			pParam->SetValue(nTemp);
			break;
		}
		// case apASSETFREQUENCY:
		//	{
		//		int32 nTemp = 0;
		//		if (!(pParam->GetValue(nTemp)))
		//			return false;
		//			nTemp = m_Info.nBaseFreq;
		//			pParam->SetValue(nTemp);
		//		break;
		//	}
		// case apLENGTHINBYTES:
		//	{
		//		int32 nTemp = 0;
		//		if (!(pParam->GetValue(nTemp)))
		//			return false;
		//		switch (m_Props.eBufferType)
		//		{
		//		case btSAMPLE:
		//			nTemp = m_Info.nLengthInBytes;
		//			pParam->SetValue(nTemp);
		//			break;
		//		case btSTREAM: // Stream does not support Byte Length
		//			nTemp = m_Info.nLengthInBytes;
		//			pParam->SetValue(nTemp);
		//			break;
		//		default:
		//			return false;
		//		}
		//		break;
		//	}
		// case apLENGTHINMS:
		//	{
		//		int32 nTemp = 1; // better not return 0
		//		if (!(pParam->GetValue(nTemp)))
		//			return false;
		//		nTemp = m_Info.nLengthInMs;
		//		pParam->SetValue(nTemp);
		//		break;
		//	}
		// case apLENGTHINSAMPLES:
		//	{
		//		int32 nTemp = 0;
		//		if (!(pParam->GetValue(nTemp)))
		//			return false;
		//		nTemp = m_Info.nLengthInSamples;
		//		pParam->SetValue(nTemp);
		//		break;
		//	}
		// case apBYTEPOSITION:
		//	{
		//		int32 nTemp = 0;
		//		switch (m_Props.eBufferType)
		//		{
		//		case btSAMPLE:
		//			return false;
		//			break;
		//		case btSTREAM:
		//			if (!(pParam->GetValue(nTemp)))
		//				return false;
		//			//nTemp = CS_Stream_GetPosition((CS_STREAM*)m_BufferData);
		//			pParam->SetValue(nTemp);
		//			break;
		//		default:
		//			return false;
		//		}
		//		return true;
		//	}
		// case apTIMEPOSITION:
		//	{
		//		int32 nTemp = 0;
		//		switch (m_Props.eBufferType)
		//		{
		//		case btSAMPLE:
		//			return false;
		//			break;
		//		case btSTREAM:
		//			if (!(pParam->GetValue(nTemp)))
		//				return false;
		//			//nTemp = CS_Stream_GetTime((CS_STREAM*)m_BufferData);
		//			pParam->SetValue(nTemp);
		//			break;
		//		default:
		//			return false;
		//		}
		//		return true;
		//	}
		case apLOADINGSTATE:
		{
			int32 nTemp = 0;
			if (!(pParam->GetValue(nTemp)))
				return false;
			nTemp = m_Info.bLoadFailure;
			pParam->SetValue(nTemp);
			break;
		}
		case apNUMREFERENCEDSOUNDS:
		{
			int32 nTemp = 0;
			if (!(pParam->GetValue(nTemp)))
				return false;
			nTemp = m_vecLoadReq.size();
			pParam->SetValue(nTemp);
			break;
		}
		default:
			return false;
			break;
	}
	return true;
}

bool CSoundBufferFmodEx400Event::SetParam(enumAssetParamSemantics eSemantics, ptParam* pParam)
{
	switch (eSemantics)
	{
		case apASSETNAME:     // TODO Review this, Reset the Asset?
			return false; // Setting this parameter not supported
			break;
		case apASSETTYPE:     // TODO Review this
			return false; // Setting this parameter not supported
			break;
		// case apASSETFREQUENCY:
		//	{
		//		int32 nTemp;
		//		if (!(pParam->GetValue(nTemp))) return false;
		//		switch (m_Props.eBufferType)
		//		{
		//		case btSAMPLE:
		//			//if (!CS_Sample_SetDefaults((CS_SAMPLE*)m_BufferData, nTemp, NULL, NULL, NULL))
		//			return false;
		//			//m_Info.nBaseFreq = nTemp;
		//			break;
		//		case btSTREAM: // Stream does not support Frequency change
		//			return false;
		//			break;
		//		default:
		//			return false;
		//			break;
		//		}
		//	}
		//	break;
		// case apLENGTHINBYTES:
		//	return false; // Setting this parameter not supported		break;
		// case apLENGTHINMS:
		//	return false; // Setting this parameter not supported		break;
		// case apBYTEPOSITION:
		//	{
		//		int32 nTemp;
		//		if (!(pParam->GetValue(nTemp))) return false;
		//		switch (m_Props.eBufferType)
		//		{
		//		case btSAMPLE:
		//			return false; // Setting this parameter not supported
		//			break;
		//		case btSTREAM:
		//			//if (!CS_Stream_SetPosition((CS_STREAM*)m_BufferData, nTemp))
		//			return false;
		//			break;
		//		default:
		//			return false;
		//			break;
		//		}
		//	}
		//	break;
		// case apTIMEPOSITION:	// TODO Review if seconds (float) or milliseconds (int) should be used
		//	{
		//		int32 nTemp;
		//		if (!(pParam->GetValue(nTemp)))
		//			return false;
		//		switch (m_Props.eBufferType)
		//		{
		//		case btSAMPLE:
		//			return false; // Setting this parameter not supported
		//			break;
		//		case btSTREAM:
		//			//if (!CS_Stream_SetTime((CS_STREAM*)m_BufferData, nTemp))
		//			return false;
		//			break;
		//		default:
		//			return false;
		//			break;
		//		}
		//	}
		//	break;
		case apLOADINGSTATE:
			return false; // Setting this parameter not supported
			break;
		default:
			return false;
			break;
	}
}

bool CSoundBufferFmodEx400Event::UnloadData(const eUnloadDataOptions UnloadOption)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	// GUARD_HEAP;
	bool bResult = true;

	CAudioDeviceFmodEx400* pAudioDeviceFmodEx400 = (CAudioDeviceFmodEx400*)m_pSoundSystem->GetIAudioDevice();

	if (m_BufferData && UnloadOption == sbUNLOAD_ALL_DATA)
	{
		// FUNCTION_PROFILER( m_pSoundSystem->GetSystem(),PROFILE_SOUND );

		// Tell Sounds that their soundbuffers are unloaded now
		for (TBufferLoadReqVecIt It = m_vecLoadReq.begin(); It != m_vecLoadReq.end(); ++It)
			(*It)->OnAssetUnloaded();
		for (TBufferLoadReqVecIt It = m_vecLoadReqToAdd.begin(); It != m_vecLoadReqToAdd.end(); ++It)
			(*It)->OnAssetUnloaded();

		if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_DETAIL)
		{
			gEnv->pLog->Log("<Sound> Destroying sound buffer %s \n", m_Props.sName.c_str());
		}

		FMOD::EventGroup* pEventGroup = (FMOD::EventGroup*)m_BufferData;
		m_ExResult = pEventGroup->freeEventData();
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("release event group failed! ", ILog::eError);
			bResult = false;
		}
		// else gEnv->pLog->Log("Error: Sound-FmodEx-SoundBuffer-Event: %s %s \n", m_Props.sName.c_str(),
		// "released succesfully.");

		switch (m_Props.eBufferType)
		{
			case btSAMPLE:
				SetAssetHandle(NULL, btSAMPLE);
				break;
			case btSTREAM:
				SetAssetHandle(NULL, btSTREAM);
				break;
			case btEVENT:
				SetAssetHandle(NULL, btEVENT);
				break;
			default:
			{
				SetAssetHandle(NULL, btNONE);
				break;
			}
		}
	}

	bool bAllEventsUnloaded = true;

	if (m_BufferData && UnloadOption == sbUNLOAD_UNNEEDED_DATA)
	{
		FMOD::EventGroup* pEventGroup = (FMOD::EventGroup*)m_BufferData;
		CTimeValue tNow = gEnv->pTimer->GetFrameStartTime();

		for (int i = 0; i < m_EventTimeouts.size(); ++i)
		{
			FMOD::Event* pEvent = 0;
			m_ExResult = pEventGroup->getEventByIndex(i, FMOD_EVENT_INFOONLY, &pEvent);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("get event by index failed! ", ILog::eError);
				bResult = false;
			}

			FMOD_EVENT_INFO EventInfo;
			memset(&EventInfo, 0, sizeof(FMOD_EVENT_INFO));

			m_ExResult = pEvent->getInfo(0, 0, &EventInfo);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("get event info failed! ", ILog::eError);
				bResult = false;
			}

			if (EventInfo.instancesactive == 0)
			{
				CTimeValue tDiff = tNow - m_EventTimeouts[i];
				int nSeconds = tDiff.GetSeconds();
				if (nSeconds > 15)
				{
					FMOD_EVENT_STATE EventState;
					m_ExResult = pEvent->getState(&EventState);
					if (m_ExResult != FMOD_OK)
					{
						FmodErrorOutput("get event state failed! ", ILog::eError);
						return false;
					}

					if (!(EventState & FMOD_EVENT_STATE_LOADING))
					{
						m_ExResult = pEventGroup->freeEventData(pEvent);
						if (m_ExResult != FMOD_OK)
						{
							FmodErrorOutput("release event data failed! ", ILog::eError);
							bResult = false;
						}
						m_EventTimeouts[i] = tNow;
					}
				}
			}
			else
			{
				m_EventTimeouts[i] = tNow;
				bAllEventsUnloaded = false;
			}
		}
		if (bAllEventsUnloaded)
			UnloadData(sbUNLOAD_ALL_DATA);
	}

	return bResult;
}
