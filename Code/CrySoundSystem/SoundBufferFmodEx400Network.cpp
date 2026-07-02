#include "CryCommon/CryCore/CrySizer.h"
#include "CryCommon/CrySoundSystem/IReverbManager.h"

#include "AudioDeviceFmodEx400.h"
#include "Sound.h"
#include "SoundBufferFmodEx400Network.h"
#include "SoundSystem.h"

FMOD_RESULT F_CALLBACK NetworkReadCallback(FMOD_SOUND* sound, void* data, unsigned int datalen)
{
	void* nParam;
	FMOD::Sound* pExSound = (FMOD::Sound*)sound;
	pExSound->getUserData(&nParam);

	if (!nParam)
		return FMOD_ERR_INVALID_PARAM;

	CSoundBufferFmodEx400Network* pNetworkBuf = (CSoundBufferFmodEx400Network*)nParam;

	unsigned int nBitsPerSample = pNetworkBuf->GetInfo()->nBitsPerSample;
	unsigned int nSamplesPerSecond = pNetworkBuf->GetInfo()->nBaseFreq;
	unsigned int nNumSamples = (datalen * 8) / nBitsPerSample;

	if (pNetworkBuf->GetINetworkStreamListener()->FillDataBuffer(nBitsPerSample, nSamplesPerSecond, nNumSamples,
	                                                             data))
	{
		datalen = (nNumSamples * nBitsPerSample) / 8;
		return FMOD_OK;
	}
	else
		return FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALLBACK NetworkSetPosCallback(FMOD_SOUND* sound, int subsound, unsigned int position,
                                             FMOD_TIMEUNIT postype)
{
	/*
	This is useful if the user calls Sound::setPosition and you want to seek your data accordingly.
	*/
	return FMOD_OK;
}

CSoundBufferFmodEx400Network::CSoundBufferFmodEx400Network(const SSoundBufferProps& Props, FMOD::System* pCSEX)
    : CSoundBuffer(Props)
{
	m_pCSEX = pCSEX;
	m_ExResult = FMOD_OK;
	m_Props.nFlags = Props.nFlags;
	m_Props.nPrecacheFlags = Props.nPrecacheFlags;
	m_pINetworkSoundListener = NULL;
	m_nLastRecordPos = 0;
	m_TempBuffer = NULL;
}

CSoundBufferFmodEx400Network::~CSoundBufferFmodEx400Network()
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

void CSoundBufferFmodEx400Network::FmodErrorOutput(const char* sDescription, ILog::ELogType LogType)
{
	switch (LogType)
	{
		case ILog::eWarning:
			gEnv->pLog->LogWarning("<Sound> SBN: %s (%d) %s\n", sDescription, m_ExResult,
			                       FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eError:
			gEnv->pLog->LogError("<Sound> SBN: %s (%d) %s\n", sDescription, m_ExResult,
			                     FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eMessage:
			gEnv->pLog->Log("<Sound> SBN: %s (%d) %s\n", sDescription, m_ExResult,
			                FMOD_ErrorString(m_ExResult));
			break;
		default:
			break;
	}
}

tAssetHandle CSoundBufferFmodEx400Network::LoadAsNetworkStream(INetworkSoundListener* pINetworkSoundListener)
{
	m_pINetworkSoundListener = pINetworkSoundListener;

	FMOD::Sound* pEXSound = 0;
	FMOD_CREATESOUNDEXINFO exinfo{};

	if (m_Info.nBitsPerSample == 8)
		exinfo.format = FMOD_SOUND_FORMAT_PCM8;
	if (m_Info.nBitsPerSample == 16)
		exinfo.format = FMOD_SOUND_FORMAT_PCM16;
	if (!exinfo.format)
		return NULL; // unknown bitrate

	exinfo.decodebuffersize = m_Info.nBitsPerSample / 8 * (m_Info.nBaseFreq / 20);
	exinfo.numchannels = m_Info.nChannels;
	exinfo.defaultfrequency = m_Info.nBaseFreq;
	exinfo.length = (m_Info.nLengthInSamples * m_Info.nBitsPerSample) / 8;
	exinfo.pcmreadcallback = NetworkReadCallback;
	exinfo.pcmsetposcallback = NetworkSetPosCallback;
	exinfo.userdata = (void*)this;

	m_ExResult = m_pCSEX->createSound(
	    0, FMOD_3D | FMOD_SOFTWARE | FMOD_LOOP_NORMAL | FMOD_CREATESTREAM | FMOD_OPENUSER, &exinfo, &pEXSound);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("create network stream sound buffer failed! ", ILog::eWarning);
		return (NULL);
	}

	if (pEXSound)
	{
		// m_ExResult = pEXSound->getLength(&(m_Info.nLengthInSamples), FMOD_TIMEUNIT_PCM);

		// prepare temp buffer
		// m_TempBuffer = new unsigned int[exinfo.length];
		// memset(m_TempBuffer, 0, sizeof(exinfo.length));

		// bool bLooping = true;
		// m_ExResult = m_pCSEX->recordStart(pEXSound, bLooping);
		// FmodExErrorCheck("start microphone recording failed! ");

		// bool rec = false;
		// m_ExResult = m_pCSEX->isRecording(&rec);
		// if (!rec)
		//	gEnv->pLog->Log("Not recording");

		m_BufferData = pEXSound;
	}

	return (pEXSound);
}

void CSoundBufferFmodEx400Network::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	// GUARD_HEAP;
	// FUNCTION_PROFILER( m_pSoundSystem->GetSystem(),PROFILE_SOUND );

	// Dont need stream anymore.
	m_pReadStream = NULL;

	if (nError)
	{
		m_Info.bLoadFailure = true;
		LoadFailed();
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

// compute memory-consumption, returns size in Bytes
uint32 CSoundBufferFmodEx400Network::GetMemoryUsage(class ICrySizer* pSizer)
{

	if (pSizer)
	{
		if (!pSizer->Add(*this))
			return 0;
	}

	return sizeof(*this);
}

// Gets and Sets Parameter defined in the enumAssetParam list
bool CSoundBufferFmodEx400Network::GetParam(enumAssetParamSemantics eSemantics, ptParam* pParam) const
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

bool CSoundBufferFmodEx400Network::SetParam(enumAssetParamSemantics eSemantics, ptParam* pParam)
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

bool CSoundBufferFmodEx400Network::UnloadData(const eUnloadDataOptions UnloadOption)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	// GUARD_HEAP;
	bool bResult = true;

	if (m_BufferData)
	{
		// FUNCTION_PROFILER( m_pSoundSystem->GetSystem(),PROFILE_SOUND );

		// m_ExResult = m_pCSEX->recordStop();
		// FmodExErrorCheck("stop microphone recording failed! ");

		FMOD::Sound* pExSound = (FMOD::Sound*)m_BufferData;
		m_ExResult = pExSound->release();
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("release network stream sound buffer failed! ", ILog::eWarning);
			bResult = false;
		}
	}
	return bResult;
}
