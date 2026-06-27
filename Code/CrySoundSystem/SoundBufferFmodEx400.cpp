#include "CryCommon/CryCore/CrySizer.h"
#include "CryCommon/CrySoundSystem/IReverbManager.h"

#include "AudioDeviceFmodEx400.h"
#include "Sound.h"
#include "SoundBufferFmodEx400.h"
#include "SoundSystem.h"

CSoundBufferFmodEx400::CSoundBufferFmodEx400(const SSoundBufferProps& Props, FMOD::System* pCSEX) : CSoundBuffer(Props)
{
	m_pCSEX = pCSEX;
	m_ExResult = FMOD_OK;
	m_Props.nFlags = Props.nFlags;
	m_Props.nPrecacheFlags = Props.nPrecacheFlags;
}

CSoundBufferFmodEx400::~CSoundBufferFmodEx400()
{
	if (m_pReadStream != NULL)
	{
		m_pReadStream->Abort();
		m_pReadStream = NULL;
	}
	UnloadData(sbUNLOAD_ALL_DATA);

	// Tell Sounds that their soundbuffers are invalid now
	for (TBufferLoadReqVecIt It = m_vecLoadReq.begin(); It != m_vecLoadReq.end(); ++It)
		(*It)->OnBufferDelete();
}

void CSoundBufferFmodEx400::FmodErrorOutput(const char* sDescription, ILog::ELogType LogType)
{
	switch (LogType)
	{
		case ILog::eWarning:
			// CryWarning(VALIDATOR_MODULE_SOUNDSYSTEM,VALIDATOR_COMMENT,"[Warning] <Sound> SB: %s (%d)
			// %s\n", sDescription, m_ExResult, FMOD_ErrorString(m_ExResult));
			gEnv->pLog->Log("[Warning] <Sound> SB: %s (%d) %s\n", sDescription, m_ExResult,
			                FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eError:
			// CryWarning(VALIDATOR_MODULE_SOUNDSYSTEM,VALIDATOR_COMMENT,"[Error] <Sound> SB: %s (%d) %s\n",
			// sDescription, m_ExResult, FMOD_ErrorString(m_ExResult));
			gEnv->pLog->Log("[Error] <Sound> SB: %s (%d) %s\n", sDescription, m_ExResult,
			                FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eMessage:
			// CryWarning(VALIDATOR_MODULE_SOUNDSYSTEM,VALIDATOR_COMMENT,"<Sound> SB: %s (%d) %s\n",
			// sDescription, m_ExResult, FMOD_ErrorString(m_ExResult));
			gEnv->pLog->Log("<Sound> SB: %s (%d) %s\n", sDescription, m_ExResult,
			                FMOD_ErrorString(m_ExResult));
			break;
		default:
			break;
	}
}

uint32 CSoundBufferFmodEx400::GetMemoryUsed()
{
	if (Loaded())
		return m_Info.nLengthInBytes;
	else
		return sizeof(*this);
}

// compute memory-consumption, returns size in Bytes
uint32 CSoundBufferFmodEx400::GetMemoryUsage(class ICrySizer* pSizer)
{

	size_t nSize = sizeof(*this);

	if (pSizer)
	{
		if (!pSizer->Add(*this))
			return 0;

		if (Loaded())
		{
			SIZER_COMPONENT_NAME(pSizer, "SoundData");
			pSizer->AddObject(m_BufferData, m_Info.nLengthInBytes);
			nSize += m_Info.nLengthInBytes;
		}
	}
	return nSize;
}

tAssetHandle CSoundBufferFmodEx400::LoadAsSample(const char* AssetDataPtrOrName, int nLength)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SOUND);

	FMOD::Sound* pEXSound = 0;
	FMOD_CREATESOUNDEXINFO exinfo{};
	exinfo.length = nLength;

	FMOD_MODE Modes = 0;

	if (m_pSoundSystem->g_nCompressedDialog)
		Modes |= FMOD_CREATECOMPRESSEDSAMPLE;

	if (m_pSoundSystem->g_nMPEGCompression)
	{
		exinfo.suggestedsoundtype = FMOD_SOUND_TYPE_MPEG;
	}
	else
	{
		exinfo.suggestedsoundtype = FMOD_SOUND_TYPE_WAV;
	}

	m_Props.sResourceFile = m_Props.sName;

	IReverbManager* pReverb = m_pSoundSystem->GetIReverbManager();
	if (pReverb && pReverb->UseHardwareVoices())
		Modes |= FMOD_HARDWARE;
	else
		Modes |= FMOD_SOFTWARE;

	if (m_Props.nFlags & FLAG_SOUND_VOICE || !(m_Props.nFlags & FLAG_SOUND_LOOP))
		Modes |= FMOD_LOOP_OFF;

	if (m_Props.nFlags & FLAG_SOUND_3D)
		m_ExResult = m_pCSEX->createSound(AssetDataPtrOrName,
		                                  (FMOD_MODE)(FMOD_OPENMEMORY | FMOD_DEFAULT | FMOD_3D | Modes),
		                                  &exinfo, &pEXSound);
	else
		m_ExResult = m_pCSEX->createSound(
		    AssetDataPtrOrName, (FMOD_MODE)(FMOD_OPENMEMORY | FMOD_DEFAULT | Modes), &exinfo, &pEXSound);

	// if the suggested codec failed, just try the other one to make sure before we give up
	if (m_ExResult == FMOD_ERR_FORMAT)
	{
		TFixedResourceName sTemp = "suggested format not availible! " + m_Props.sName;
		FmodErrorOutput(sTemp.c_str(), ILog::eWarning);

		if (m_pSoundSystem->g_nMPEGCompression)
			exinfo.suggestedsoundtype = FMOD_SOUND_TYPE_WAV;
		else
			exinfo.suggestedsoundtype = FMOD_SOUND_TYPE_MPEG;

		if (m_Props.nFlags & FLAG_SOUND_3D)
			m_ExResult = m_pCSEX->createSound(AssetDataPtrOrName,
			                                  (FMOD_MODE)(FMOD_OPENMEMORY | FMOD_DEFAULT | FMOD_3D | Modes),
			                                  &exinfo, &pEXSound);
		else
			m_ExResult = m_pCSEX->createSound(AssetDataPtrOrName,
			                                  (FMOD_MODE)(FMOD_OPENMEMORY | FMOD_DEFAULT | Modes), &exinfo,
			                                  &pEXSound);
	}

	if (m_ExResult != FMOD_OK)
	{
		TFixedResourceName sTemp = "create sample sound failed! " + m_Props.sName;
		FmodErrorOutput(sTemp.c_str(), ILog::eWarning);
		return (NULL);
	}

	if (pEXSound)
	{
		// m_Info.nChannels = exinfo.numchannels;
		float fFreq;

		m_ExResult = pEXSound->getLength(&(m_Info.nLengthInMs), FMOD_TIMEUNIT_MS);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get length ms failed! ", ILog::eWarning);
			return (NULL);
		}

		m_ExResult = pEXSound->getLength(&(m_Info.nLengthInSamples), FMOD_TIMEUNIT_PCM);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get length pcm failed! ", ILog::eWarning);
			return (NULL);
		}

		m_ExResult = pEXSound->getDefaults(&fFreq, 0, 0, 0);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get defaults failed! ", ILog::eWarning);
			return (NULL);
		}

		m_Info.nBaseFreq = (uint32)fFreq;
		m_ExResult = pEXSound->getFormat(0, 0, &(m_Info.nChannels), &(m_Info.nBitsPerSample));
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get format failed! ", ILog::eWarning);
			return (NULL);
		}

		// m_ExResult = pEXSound->getMode(&SoundMode);
		// m_ExResult = pEXSound->setMode(FMOD_LOOP_NORMAL);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("set mode failed! ", ILog::eWarning);
			return (NULL);
		}

		// TODO review this, what will happen if compressed sound data is used?
		// m_Info.nLengthInBytes		= (int32)
		// m_Info.nLengthInSamples*m_Info.nChannels*(m_Info.nBitsPerSample/8.0f);
		m_Info.nLengthInBytes = exinfo.length;
	}

	return (pEXSound);
}

tAssetHandle CSoundBufferFmodEx400::LoadAsStream(const char* AssetName, int nLength)
{
	FMOD::Sound* pEXStream = 0;
	FMOD_CREATESOUNDEXINFO exinfo{};
	exinfo.length = nLength;

	if (m_Props.nFlags & FLAG_SOUND_3D)
		m_ExResult = m_pCSEX->createStream(AssetName, (FMOD_MODE)(FMOD_CREATESTREAM | FMOD_DEFAULT | FMOD_3D),
		                                   0, &pEXStream);
	else
		m_ExResult =
		    m_pCSEX->createStream(AssetName, (FMOD_MODE)(FMOD_CREATESTREAM | FMOD_DEFAULT), 0, &pEXStream);

	if (m_ExResult != FMOD_OK)
	{
		TFixedResourceName sTemp = "create stream sound failed! " + m_Props.sName;
		FmodErrorOutput(sTemp.c_str(), ILog::eWarning);
		return (NULL);
	}

	if (pEXStream)
	{
		float fFreq;
		m_ExResult = pEXStream->getLength(&(m_Info.nLengthInMs), FMOD_TIMEUNIT_MS);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get length ms failed! ", ILog::eWarning);
			return (NULL);
		}

		m_ExResult = pEXStream->getLength(&(m_Info.nLengthInSamples), FMOD_TIMEUNIT_PCM);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get length pcm failed! ", ILog::eWarning);
			return (NULL);
		}

		m_ExResult = pEXStream->getDefaults(&fFreq, 0, 0, 0);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get defaults failed! ", ILog::eWarning);
			return (NULL);
		}
		m_Info.nBaseFreq = (uint32)fFreq;

		m_ExResult = pEXStream->getFormat(0, 0, &(m_Info.nChannels), &(m_Info.nBitsPerSample));
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get format failed! ", ILog::eWarning);
			return (NULL);
		}

		// m_ExResult = pEXSound->getMode(&SoundMode);
		// m_ExResult = pEXStream->setMode(FMOD_LOOP_NORMAL);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("set mode failed! ", ILog::eWarning);
			return (NULL);
		}
		m_Info.nLengthInBytes = exinfo.length;
	}

	return (pEXStream);
}

void CSoundBufferFmodEx400::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SOUND);

	// Dont need stream anymore.
	m_pReadStream = NULL;

	if (nError)
	{
		m_Info.bLoadFailure = true;
		// LoadFailed();
		return;
	}

	FMOD::Sound* pExSound = (FMOD::Sound*)LoadAsSample((const char*)pStream->GetBuffer(), pStream->GetBytesRead());

	if (!pExSound)
	{
		gEnv->pLog->LogToFile("<Sound> Error: Sound - Cannot load sample sound %s\n", m_Props.sName.c_str());
		m_Info.bLoadFailure = true;
		LoadFailed();
		return;
	}
	// set the sound source
	SetAssetHandle(pExSound, btSAMPLE);
	SoundBufferLoaded();
	// TRACE("Sound-Streaming for %s finished.", m_Props.sName.c_str());
}

// Gets and Sets Parameter defined in the enumAssetParam list
bool CSoundBufferFmodEx400::GetParam(enumAssetParamSemantics eSemantics, ptParam* pParam) const
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
			int32 nTemp;
			if (!(pParam->GetValue(nTemp)))
				return false;
			nTemp = m_Props.eBufferType;
			pParam->SetValue(nTemp);
			break;
		}
		case apASSETFREQUENCY:
		{
			int32 nTemp;
			if (!(pParam->GetValue(nTemp)))
				return false;
			nTemp = m_Info.nBaseFreq;
			pParam->SetValue(nTemp);
			break;
		}
		case apLENGTHINBYTES:
		{
			int32 nTemp;
			if (!(pParam->GetValue(nTemp)))
				return false;
			switch (m_Props.eBufferType)
			{
				case btSAMPLE:
					nTemp = m_Info.nLengthInBytes;
					pParam->SetValue(nTemp);
					break;
				case btSTREAM: // Stream does not support Byte Length
					nTemp = m_Info.nLengthInBytes;
					pParam->SetValue(nTemp);
					break;
				default:
					return false;
			}
			break;
		}
		case apLENGTHINMS:
		{
			int32 nTemp;
			if (!(pParam->GetValue(nTemp)))
				return false;
			nTemp = m_Info.nLengthInMs;
			pParam->SetValue(nTemp);
			break;
		}
		case apLENGTHINSAMPLES:
		{
			int32 nTemp;
			if (!(pParam->GetValue(nTemp)))
				return false;
			nTemp = m_Info.nLengthInSamples;
			pParam->SetValue(nTemp);
			break;
		}
		case apBYTEPOSITION:
		{
			int32 nTemp;
			switch (m_Props.eBufferType)
			{
				case btSAMPLE:
					return false;
					break;
				case btSTREAM:
					if (!(pParam->GetValue(nTemp)))
						return false;
					// nTemp = CS_Stream_GetPosition((CS_STREAM*)m_BufferData);
					pParam->SetValue(nTemp);
					break;
				default:
					return false;
			}
			return true;
		}
		case apTIMEPOSITION:
		{
			int32 nTemp;
			switch (m_Props.eBufferType)
			{
				case btSAMPLE:
					return false;
				case btSTREAM:
					if (!(pParam->GetValue(nTemp)))
						return false;
					// nTemp = CS_Stream_GetTime((CS_STREAM*)m_BufferData);
					pParam->SetValue(nTemp);
					break;
				default:
					return false;
			}
			return true;
		}
		case apLOADINGSTATE:
		{
			int32 nTemp;
			if (!(pParam->GetValue(nTemp)))
				return false;
			nTemp = m_Info.bLoadFailure;
			pParam->SetValue(nTemp);
			break;
		}
		case apNUMREFERENCEDSOUNDS:
		{
			int32 nTemp;
			if (!(pParam->GetValue(nTemp)))
				return false;
			nTemp = m_vecLoadReq.size();
			pParam->SetValue(nTemp);
			break;
		}
		default:
		{
			return false;
		}
	}
	return true;
}

bool CSoundBufferFmodEx400::SetParam(enumAssetParamSemantics eSemantics, ptParam* pParam)
{
	switch (eSemantics)
	{
		case apASSETNAME:     // TODO Review this, Reset the Asset?
			return false; // Setting this parameter not supported
			break;
		case apASSETTYPE:     // TODO Review this
			return false; // Setting this parameter not supported
			break;
		case apASSETFREQUENCY:
		{
			int32 nTemp;
			if (!(pParam->GetValue(nTemp)))
				return false;
			switch (m_Props.eBufferType)
			{
				case btSAMPLE:
					// if (!CS_Sample_SetDefaults((CS_SAMPLE*)m_BufferData, nTemp, NULL, NULL,
					// NULL))
					return false;
					// m_Info.nBaseFreq = nTemp;
					break;
				case btSTREAM: // Stream does not support Frequency change
					return false;
					break;
				default:
					return false;
					break;
			}
		}
		break;
		case apLENGTHINBYTES:
			return false; // Setting this parameter not supported		break;
		case apLENGTHINMS:
			return false; // Setting this parameter not supported		break;
		case apBYTEPOSITION:
		{
			int32 nTemp;
			if (!(pParam->GetValue(nTemp)))
				return false;
			switch (m_Props.eBufferType)
			{
				case btSAMPLE:
					return false; // Setting this parameter not supported
					break;
				case btSTREAM:
					// if (!CS_Stream_SetPosition((CS_STREAM*)m_BufferData, nTemp))
					return false;
					break;
				default:
					return false;
					break;
			}
		}
		break;
		case apTIMEPOSITION: // TODO Review if seconds (float) or milliseconds (int) should be used
		{
			int32 nTemp;
			if (!(pParam->GetValue(nTemp)))
				return false;
			switch (m_Props.eBufferType)
			{
				case btSAMPLE:
					return false; // Setting this parameter not supported
					break;
				case btSTREAM:
					// if (!CS_Stream_SetTime((CS_STREAM*)m_BufferData, nTemp))
					return false;
					break;
				default:
					return false;
					break;
			}
		}
		break;
		case apLOADINGSTATE:
			return false; // Setting this parameter not supported
			break;
		default:
			return false;
			break;
	}
}

bool CSoundBufferFmodEx400::UnloadData(const eUnloadDataOptions UnloadOption)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	// GUARD_HEAP;
	bool bResult = true;

	if (m_BufferData) // && UnloadOption == sbUNLOAD_ALL_DATA)
	{
		// FUNCTION_PROFILER( m_pSoundSystem->GetSystem(),PROFILE_SOUND );

		// Tell Sounds that their soundbuffers are unloaded now
		for (TBufferLoadReqVecIt It = m_vecLoadReq.begin(); It != m_vecLoadReq.end(); ++It)
			(*It)->OnAssetUnloaded();
		for (TBufferLoadReqVecIt It = m_vecLoadReqToAdd.begin(); It != m_vecLoadReqToAdd.end(); ++It)
			(*It)->OnAssetUnloaded();

		FMOD::Sound* pExSound = (FMOD::Sound*)m_BufferData;
		m_ExResult = pExSound->release();
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("release sound failed! ", ILog::eWarning);
			bResult = false;
		}

		switch (m_Props.eBufferType)
		{
			case btSAMPLE:
				SetAssetHandle(NULL, btSAMPLE);
				break;
			case btSTREAM:
				SetAssetHandle(NULL, btSTREAM);
				break;
		}
		return bResult;
	}

	return false;
}
