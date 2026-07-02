#include "CryCommon/CryCore/CrySizer.h"
#include "CryCommon/CrySoundSystem/IReverbManager.h"

#include "AudioDeviceFmodEx400.h"
#include "Sound.h"
#include "SoundBufferFmodEx400Micro.h"
#include "SoundSystem.h"

CSoundBufferFmodEx400Micro::CSoundBufferFmodEx400Micro(const SSoundBufferProps& Props, FMOD::System* pCSEX)
    : CSoundBuffer(Props)
{
	m_pCSEX = pCSEX;
	m_ExResult = FMOD_OK;
	m_Props.nFlags = Props.nFlags;
	m_Props.nPrecacheFlags = Props.nPrecacheFlags;
	m_pMicrophoneStream = NULL;
	m_nLastRecordPos = 0;
}

CSoundBufferFmodEx400Micro::~CSoundBufferFmodEx400Micro()
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

void CSoundBufferFmodEx400Micro::FmodErrorOutput(const char* sDescription, ILog::ELogType LogType)
{
	switch (LogType)
	{
		case ILog::eWarning:
			gEnv->pLog->LogWarning("<Sound> SBM: %s (%d) %s\n", sDescription, m_ExResult,
			                       FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eError:
			gEnv->pLog->LogError("<Sound> SBM: %s (%d) %s\n", sDescription, m_ExResult,
			                     FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eMessage:
			gEnv->pLog->Log("<Sound> SBM: %s (%d) %s\n", sDescription, m_ExResult,
			                FMOD_ErrorString(m_ExResult));
			break;
		default:
			break;
	}
}

tAssetHandle CSoundBufferFmodEx400Micro::LoadAsMicro(IMicrophoneStream* pIMicrophoneStream)
{
	m_pMicrophoneStream = pIMicrophoneStream;

	FMOD::Sound* pEXSound = 0;
	FMOD_CREATESOUNDEXINFO exinfo{};

	if (m_Info.nBitsPerSample == 8)
		exinfo.format = FMOD_SOUND_FORMAT_PCM8;
	if (m_Info.nBitsPerSample == 16)
		exinfo.format = FMOD_SOUND_FORMAT_PCM16;
	if (!exinfo.format)
		return NULL; // unknown bitrate

	exinfo.numchannels = m_Info.nChannels;
	exinfo.defaultfrequency = m_Info.nBaseFreq;
	exinfo.length = (exinfo.numchannels * m_Info.nBaseFreq * m_Info.nBitsPerSample) / 8;

	m_ExResult = m_pCSEX->createSound(0, FMOD_2D | FMOD_SOFTWARE | FMOD_OPENUSER, &exinfo, &pEXSound);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("create microphone record buffer failed! ", ILog::eWarning);
		return (NULL);
	}

	if (pEXSound)
	{
		m_ExResult = pEXSound->getLength(&(m_Info.nLengthInSamples), FMOD_TIMEUNIT_PCM);

		bool bLooping = true;
		m_ExResult = m_pCSEX->recordStart(pEXSound, bLooping);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("start microphone recording failed! ", ILog::eWarning);
			// return (NULL);
		}

		bool rec = false;
		m_ExResult = m_pCSEX->isRecording(&rec);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get isRecording failed! ", ILog::eWarning);
			// return (NULL);
		}

		if (!rec)
			gEnv->pLog->Log("<Sound> Micro: Not recording");

		m_BufferData = pEXSound;
	}

	return (pEXSound);
}

void CSoundBufferFmodEx400Micro::Update()
{
	std::lock_guard lock(m_mutex);

	if (m_BufferData)
	{
		unsigned int nRecordPos = 0;
		FMOD::Sound* pEXSound = (FMOD::Sound*)m_BufferData;

		m_ExResult = m_pCSEX->getRecordPosition(&nRecordPos);
		if (m_ExResult != FMOD_OK)
		{
			// FmodErrorOutput("get Record Position failed! ", ILog::eWarning);
			m_pMicrophoneStream->OnError("get Record Position failed!");
			return;
		}

		if (nRecordPos != m_nLastRecordPos)
		{
			void *ptr1, *ptr2;
			int blocklength = 0;
			unsigned int len1 = 0;
			unsigned int len2 = 0;
			unsigned int nBufferLength = 0;
			pEXSound->getLength(&nBufferLength, FMOD_TIMEUNIT_PCMBYTES);

			blocklength = (int)nRecordPos - (int)m_nLastRecordPos;
			if (blocklength < 0)
			{
				blocklength += m_Info.nLengthInSamples;
			}

			// Lock the sound to get access to the raw data.
			// m_ExResult = pEXSound->lock(m_nLastRecordPos * m_Info.nChannels * 2, blocklength *
			// m_Info.nChannels * 2, &ptr1, &ptr2, &len1, &len2);   /* * exinfo.numchannels * 2 = stereo
			// 16bit.  1 sample = 4 bytes. */ m_ExResult = pEXSound->lock(0, nBufferLength, &ptr1, &ptr2,
			// &len1, &len2);   /* * exinfo.numchannels * 2 = stereo 16bit.  1 sample = 4 bytes. */
			m_ExResult =
			    pEXSound->lock(m_nLastRecordPos * m_Info.nBitsPerSample / 8,
			                   blocklength * m_Info.nBitsPerSample / 8, &ptr1, &ptr2, &len1,
			                   &len2); /* * exinfo.numchannels * 2 = stereo 16bit.  1 sample = 4 bytes. */

			if (m_ExResult != FMOD_OK)
			{
				// FmodErrorOutput("lock micro sound failed! ", ILog::eWarning);
				m_pMicrophoneStream->OnError("lock micro sound failed!");
				return;
			}

			/*
			//Write it to temp buffer
			if (ptr1 && len1)
			{
			        memcpy(m_TempBuffer, ptr1, len1);
			        //datalength += fwrite(ptr1, 1, len1, fp);
			}
			if (ptr2 && len2)
			{
			        memcpy((void*)((uint8*)m_TempBuffer+len1), ptr2, len2);
			        //datalength += fwrite(ptr2, 1, len2, fp);
			}
			*/
			m_pMicrophoneStream->ReadDataBuffer(ptr1, len1, ptr2, len2);

			/*
			Unlock the sound to allow FMOD to use it again.
			*/
			m_ExResult = pEXSound->unlock(ptr1, ptr2, len1, len2);
			if (m_ExResult != FMOD_OK)
			{
				// FmodErrorOutput("unlock micro sound failed! ", ILog::eWarning);
				m_pMicrophoneStream->OnError("unlock micro sound failed!");
				return;
			}
		}
		else
			m_pMicrophoneStream->ReadDataBuffer(0, 0, 0, 0);

		m_nLastRecordPos = nRecordPos;
	}
	else
		m_pMicrophoneStream->ReadDataBuffer(0, 0, 0, 0);
}

void CSoundBufferFmodEx400Micro::StreamOnComplete(IReadStream* pStream, unsigned nError)
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
uint32 CSoundBufferFmodEx400Micro::GetMemoryUsage(class ICrySizer* pSizer)
{

	if (pSizer)
	{
		if (!pSizer->Add(*this))
			return 0;
	}

	return sizeof(*this);
}

// Gets and Sets Parameter defined in the enumAssetParam list
bool CSoundBufferFmodEx400Micro::GetParam(enumAssetParamSemantics eSemantics, ptParam* pParam) const
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

bool CSoundBufferFmodEx400Micro::SetParam(enumAssetParamSemantics eSemantics, ptParam* pParam)
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

bool CSoundBufferFmodEx400Micro::UnloadData(const eUnloadDataOptions UnloadOption)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	std::lock_guard lock(m_mutex);
	// GUARD_HEAP;
	bool bResult = true;

	if (m_BufferData)
	{
		// FUNCTION_PROFILER( m_pSoundSystem->GetSystem(),PROFILE_SOUND );

		m_ExResult = m_pCSEX->recordStop();
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("stop microphone recording failed! ", ILog::eWarning);
			bResult = false;
		}

		FMOD::Sound* pExSound = (FMOD::Sound*)m_BufferData;
		m_ExResult = pExSound->release();
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("release microphone record buffer failed! ", ILog::eWarning);
			bResult = false;
		}
	}
	return bResult;
}
