#include <mimalloc.h>

#include "CryCommon/CryCore/CrySizer.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CrySoundSystem/IReverbManager.h"
#include "CryCommon/CrySoundSystem/ISound.h"
#include "CryCommon/CrySoundSystem/ISoundMoodManager.h"
#include "CryCommon/CrySystem/CryFile.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/ICryPak.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/ITimer.h"

#include "AudioDeviceFmodEx400.h"
#include "PlatformSoundFmodEx400.h"
#include "PlatformSoundFmodEx400Event.h"
#include "Sound.h"
#include "SoundBufferFmodEx400.h"
#include "SoundBufferFmodEx400Event.h"
#include "SoundBufferFmodEx400Micro.h"
#include "SoundBufferFmodEx400Network.h"
#include "SoundSystem.h"

static void* F_CALLBACK CrySound_Alloc(unsigned int size, FMOD_MEMORY_TYPE memtype)
{
	return mi_malloc(size);
}

static void F_CALLBACK CrySound_Free(void* ptr, FMOD_MEMORY_TYPE memtype)
{
	mi_free(ptr);
}

static void* F_CALLBACK CrySound_Realloc(void* ptr, unsigned int size, FMOD_MEMORY_TYPE memtype)
{
	return mi_realloc(ptr, size);
}

static FMOD_RESULT F_CALLBACK CrySoundEx_fopen(const char* name, int unicode, unsigned int* filesize, void** handle,
                                               void** userdata)
{
	FILE* file = gEnv->pCryPak->FOpen(name, "rbx");
	if (!file)
	{
		CryLogWarning("CrySoundEx_fopen: File %s open failed!", name);
		return FMOD_ERR_FILE_NOTFOUND;
	}

	uint32 nSize = gEnv->pCryPak->FGetSize(file);
	*handle = file;
	*filesize = nSize;

	return FMOD_OK;
}

static FMOD_RESULT F_CALLBACK CrySoundEx_fclose(void* nFile, void* userdata)
{
	if (nFile)
	{
		FILE* file = (FILE*)nFile;

		if (gEnv->pCryPak->FClose(file) == 0)
		{
			return FMOD_OK;
		}
	}

	return FMOD_ERR_FILE_NOTFOUND;
}

static FMOD_RESULT F_CALLBACK CrySoundEx_fread(void* nFile, void* buffer, unsigned int sizebytes,
                                               unsigned int* bytesread, void* userdata)
{
	FILE* file = (FILE*)nFile;

	uint32 nBytesRead = gEnv->pCryPak->FReadRaw(buffer, 1, sizebytes, file);
	*bytesread = nBytesRead;

	if (nBytesRead != sizebytes)
	{
		return FMOD_ERR_FILE_EOF;
	}

	return FMOD_OK;
}

static FMOD_RESULT F_CALLBACK CrySoundEx_fseek(void* nFile, unsigned int pos, void* userdata)
{
	FILE* file = (FILE*)nFile;

	// mode not supplied
	int mode = 0;

	if (gEnv->pCryPak->FSeek(file, pos, mode) == size_t(-1))
	{
		return FMOD_ERR_FILE_COULDNOTSEEK;
	}

	return FMOD_OK;
}

void CAudioDeviceFmodEx400::FmodErrorOutput(const char* sDescription, ILog::ELogType LogType)
{
	switch (LogType)
	{
		case ILog::eWarning:
			CryLog("[Warning] <Sound> Sound-FmodEx-AudioDevice: %s (%d) %s\n", sDescription, m_ExResult,
			       FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eError:
			CryLog("[Error] <Sound> Sound-FmodEx-AudioDevice: %s (%d) %s\n", sDescription, m_ExResult,
			       FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eMessage:
			CryLog("<Sound> Sound-FmodEx-AudioDevice: %s (%d) %s\n", sDescription, m_ExResult,
			       FMOD_ErrorString(m_ExResult));
			break;
	}
}

CAudioDeviceFmodEx400::CAudioDeviceFmodEx400(void* hWnd)
{
	// GUARD_HEAP;

	m_ExResult = FMOD_OK;
	m_nSpeakerMode = FMOD_SPEAKERMODE_RAW;
	m_pEventSystem = NULL;
	m_pSoundSystem = NULL;
	m_pAPISystem = NULL;
	m_pAPISystemDSound = NULL;
	m_tLastUpdate = gEnv->pTimer->GetFrameStartTime();

	m_ExResult = FMOD::Memory_Initialize(0, 0, CrySound_Alloc, CrySound_Realloc, CrySound_Free);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("memory initialization failed! ", ILog::eError);
	}

	m_InitSettings.nMaxHWchannels = 0;
	m_InitSettings.nMinHWChannels = 0;
	m_InitSettings.nSoftwareChannels = 0;
	m_InitSettings.nMPEGDecoders = 0;
	m_InitSettings.nXMADecoders = 0;
	m_InitSettings.nADPCMDecoders = 0;

	m_nCurrentMemAlloc = 0;
	m_nMaxMemAlloc = 0;
	// m_nEaxStatusFMOD = 0;

	m_nHardware2DChannels = 0;
	m_nHardware3DChannels = 0;
	m_nTotalHardwareChannelsAvail = 0;

	// 0 - 1, 1 being max volume
	m_fMasterVolume = 1.0f;
	m_nFMODDriverCaps = 0;
	m_nCountProject = 0;
	m_nCountEvent = 0;
	m_nCountGroup = 0;

	// store active window
	m_pHWnd = hWnd;

	m_ExResult = FMOD::EventSystem_Create(&m_pEventSystem);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("event system failed! ", ILog::eError);
		return;
	}

	// have to do it again
	FMOD_DEBUGLEVEL nLevel = FMOD_DEBUG_LEVEL_NONE;
	m_ExResult = FMOD::Debug_SetLevel(nLevel);

	// Get FMOD-Ex System Ptr from EventSystem
	m_ExResult = m_pEventSystem->getSystemObject(&m_pAPISystem);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("get system object failed! ", ILog::eError);
		return;
	}
}

CAudioDeviceFmodEx400::~CAudioDeviceFmodEx400()
{
	ShutDownDevice();

	int nEnd = m_UnusedPlatformSounds.size();
	for (int i = 0; i < nEnd; ++i)
		delete m_UnusedPlatformSounds[i];

	nEnd = m_UnusedPlatformSoundEvents.size();
	for (int i = 0; i < nEnd; ++i)
		delete m_UnusedPlatformSoundEvents[i];
}

bool CAudioDeviceFmodEx400::InitDevice(CSoundSystem* pSoundSystem)
{
	// GUARD_HEAP;

	if (!pSoundSystem) // check for valid system
		return false;

	m_pSoundSystem = pSoundSystem;
	bool bTemp = true;
	m_nMemoryStatInc = 0;

	// system starts out not muted
	m_bMuteStatus = false;

	// make true when have new attributes for listener
	m_bHaveListenerAttributes = false;

	m_bSystemPaused = false; // starts out not paused

	// gets system stats when turned on
	m_bGetSystemStats = false;

	// memory debugging
	// m_ExResult = m_pEventSystem->release();
	// if (m_ExResult != FMOD_OK)
	//{
	//	FmodErrorOutput("event system release failed! ", ILog::eError);
	//	bTemp = false;
	//}

	// m_ExResult = m_pEventSystem->release();
	// if (m_ExResult != FMOD_OK)
	//{
	//	FmodErrorOutput("event system release failed! ", ILog::eError);
	//	bTemp = false;
	// }

	// if not already there, create again
	if (!m_pEventSystem)
	{
		m_ExResult = FMOD::EventSystem_Create(&m_pEventSystem);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("event system failed! ", ILog::eError);
			return false;
		}
	}

	// and again..
	FMOD_DEBUGLEVEL nLevel = FMOD_DEBUG_LEVEL_NONE;
	m_ExResult = FMOD::Debug_SetLevel(nLevel);

	if (pSoundSystem->g_nNetworkAudition)
	{
		m_ExResult = FMOD::NetEventSystem_Init(m_pEventSystem);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("event system network audition init failed! ", ILog::eError);
			return false;
		}
	}

	// Get FMOD-Ex System Ptr from EventSystem
	m_ExResult = m_pEventSystem->getSystemObject(&m_pAPISystem);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("get system object failed! ", ILog::eError);
		return false;
	}

	// changing number of Mpeg codec instances
	FMOD_ADVANCEDSETTINGS settings{};
	m_ExResult = m_pAPISystem->getAdvancedSettings(&settings);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("get system advanced settings failed! ", ILog::eError);
		return false;
	}

	settings.maxMPEGcodecs = m_InitSettings.nMPEGDecoders;
	settings.maxADPCMcodecs = m_InitSettings.nADPCMDecoders;
	settings.maxXMAcodecs = m_InitSettings.nXMADecoders;
	// settings.minHRTFAngle		= 90;
	////settings.maxHRTFAngle   = 0;
	// settings.minHRTFFreq    = 4000;
	// settings.maxHRTFFreq		= 22050;

	m_ExResult = m_pAPISystem->setAdvancedSettings(&settings);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("set system advanced settings failed! ", ILog::eError);
		return false;
	}

	// software format
	int nSampleRate = 0;
	FMOD_SOUND_FORMAT Format = FMOD_SOUND_FORMAT_NONE;
	int nOutputChannels = 0;
	int nInputChannels = 0;
	FMOD_DSP_RESAMPLER Resampler = FMOD_DSP_RESAMPLER_NOINTERP;
	int nBits = 0;
	m_ExResult = m_pAPISystem->getSoftwareFormat(&nSampleRate, &Format, &nOutputChannels, &nInputChannels,
	                                             &Resampler, &nBits);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("get software format failed! ", ILog::eError);
		return false;
	}

	if (pSoundSystem->g_nFormatSampleRate > 0 && pSoundSystem->g_nFormatSampleRate <= 96000)
		nSampleRate = pSoundSystem->g_nFormatSampleRate;

	if (pSoundSystem->g_nFormatType >= 0 && (FMOD_SOUND_FORMAT)pSoundSystem->g_nFormatType < FMOD_SOUND_FORMAT_MAX)
		Format = (FMOD_SOUND_FORMAT)pSoundSystem->g_nFormatType;

	if (pSoundSystem->g_nFormatResampler >= 0 &&
	    (FMOD_DSP_RESAMPLER)pSoundSystem->g_nFormatResampler < FMOD_DSP_RESAMPLER_MAX)
		Resampler = (FMOD_DSP_RESAMPLER)pSoundSystem->g_nFormatResampler;

	m_ExResult = m_pAPISystem->setSoftwareFormat(nSampleRate, Format, 0, 0, Resampler);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("set software format failed! ", ILog::eError);
		return false;
	}

	// output types
	FMOD_OUTPUTTYPE nOutput = FMOD_OUTPUTTYPE_AUTODETECT;

	if (pSoundSystem->g_nOutputConfig == 1)
	{
		nOutput = FMOD_OUTPUTTYPE_DSOUND;
		CryLogAlways("Sound - trying to initialize DirectSound output! \n");
	}
	if (pSoundSystem->g_nOutputConfig == 2)
	{
		nOutput = FMOD_OUTPUTTYPE_WAVWRITER;
		CryLogAlways("Sound - trying to initialize Wav-Writer output! \n");
	}
	if (pSoundSystem->g_nOutputConfig == 3)
	{
		nOutput = FMOD_OUTPUTTYPE_WAVWRITER_NRT;
		CryLogAlways("Sound - trying to initialize Wav-Writer_NRT output! \n");
	}

	m_ExResult = m_pAPISystem->setOutput(nOutput);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("set sound output failed! ", ILog::eError);
		return false;
	}

	// Create DSound System object on Vista for SoftDec audio support
	m_ExResult = m_pAPISystem->getOutput(&nOutput);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("get sound output failed! ", ILog::eError);
		return false;
	}

	switch (nOutput)
	{
		case FMOD_OUTPUTTYPE_DSOUND:
			CryLogAlways("Sound - starting to initialize DirectSound output! \n");
			break;
		case FMOD_OUTPUTTYPE_WASAPI:
			CryLogAlways("Sound - starting to initialize Windows Audio Session API output! \n");
			break;
		case FMOD_OUTPUTTYPE_WINMM:
			CryLogAlways("Sound - starting to initialize Windows Multimedia output! \n");
			break;
		case FMOD_OUTPUTTYPE_XBOX360:
			CryLogAlways("Sound - starting to initialize XBox360 output! \n");
			break;
		case FMOD_OUTPUTTYPE_PS3:
			CryLogAlways("Sound - starting to initialize PS3 output! \n");
			break;
		default:
			break;
	}

	if (nOutput != FMOD_OUTPUTTYPE_DSOUND)
	{
		m_ExResult = FMOD::System_Create(&m_pAPISystemDSound);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("2nd system object failed! ", ILog::eError);
			m_pAPISystemDSound = 0;
		}
		else
		{
			FMOD_DEBUGLEVEL nLevel = FMOD_DEBUG_LEVEL_NONE;
			m_ExResult = FMOD::Debug_SetLevel(nLevel);

			m_ExResult = m_pAPISystemDSound->setOutput(FMOD_OUTPUTTYPE_DSOUND);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("set DSound on 2nd system object failed! ", ILog::eError);
				m_pAPISystemDSound = 0;
			}
			else
			{
				if (m_pSoundSystem->g_nOutputConfig == 2 || m_pSoundSystem->g_nOutputConfig == 3)
					m_ExResult = m_pAPISystemDSound->init(5, FMOD_INIT_NORMAL,
					                                      (void*)"audio_capture_dsound.wav");
				else
					m_ExResult = m_pAPISystemDSound->init(
					    5, FMOD_INIT_NORMAL, m_pSoundSystem->g_nNetworkAudition ? NULL : m_pHWnd);

				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("init 2nd system object failed! ", ILog::eError);
					m_pAPISystemDSound = NULL;
				}
			}
		}
	}

	int nNumDrivers = 0;
	m_ExResult = m_pAPISystem->getNumDrivers(&nNumDrivers);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("get number of drivers failed! ", ILog::eError);
		// return false;

		// enforce DSound
		m_ExResult = m_pAPISystem->setOutput(FMOD_OUTPUTTYPE_DSOUND);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("set sound output (enforce DSound) failed! ", ILog::eError);
			return false;
		}

		m_ExResult = m_pAPISystem->getNumDrivers(&nNumDrivers);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("get number of drivers failed again! ", ILog::eError);
			return false;
		}
	}

	CryLogAlways("Sound - %d drivers found: \n", nNumDrivers);

	// prints out valid drivers
	for (int i = 0; m_ExResult == FMOD_OK; ++i)
	{
		char sDriverName[MAXCHARBUFFERSIZE];
		m_ExResult = m_pAPISystem->getDriverInfo(i, sDriverName, MAXCHARBUFFERSIZE, 0);

		if (m_ExResult == FMOD_OK)
			CryLogAlways("Sound - available drivers: %d %s !\n", i, sDriverName);
	}

	m_ExResult = m_pAPISystem->setDriver(0);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("set driver to default failed! ", ILog::eError);
		return false;
	}

	// querying which driver
	int nDriver = 0;
	m_ExResult = m_pAPISystem->getDriver(&nDriver);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("get driver failed! ", ILog::eError);
		return false;
	}

	// querying which driver
	char sDriverName[MAXCHARBUFFERSIZE];
	m_ExResult = m_pAPISystem->getDriverInfo(nDriver, sDriverName, MAXCHARBUFFERSIZE, 0);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("get driver name failed! ", ILog::eError);
		return false;
	}

	// Set HW and Softwarevoices
	m_ExResult = m_pAPISystem->setHardwareChannels(m_InitSettings.nMinHWChannels, m_InitSettings.nMaxHWchannels,
	                                               m_InitSettings.nMinHWChannels, m_InitSettings.nMaxHWchannels);
	// m_ExResult = m_pAPISystem->setHardwareChannels(0, 0, 0, 0);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("set hardware voices limit failed! ", ILog::eError);
		return false;
	}

	// overwrite file system
	m_ExResult =
	    m_pAPISystem->setFileSystem(CrySoundEx_fopen, CrySoundEx_fclose, CrySoundEx_fread, CrySoundEx_fseek, -1);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("set file system callbacks failed! ", ILog::eError);
		return false;
	}

	// need to call iseax before initt to set it up correctly (also speaker mode)
	IsEax();

	m_ExResult = m_pAPISystem->setSpeakerMode(m_nSpeakerMode);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("set speaker mode failed! ", ILog::eError);
		return false;
	}

	// TOMAS
	m_ExResult = m_pAPISystem->setSoftwareChannels(64);

	CryLogAlways("Sound - initializing FMOD-EX now!\n");

	FMOD_INITFLAGS InitFlags = FMOD_INIT_NORMAL;

	if (m_pSoundSystem->g_nObstruction == 1)
		InitFlags |= FMOD_INIT_SOFTWARE_OCCLUSION;

	if (m_pSoundSystem->g_nHRTF_DSP == 1)
		InitFlags |= FMOD_INIT_SOFTWARE_HRTF;

	if (m_pSoundSystem->g_nVol0TurnsVirtual == 1)
		InitFlags |= FMOD_INIT_VOL0_BECOMES_VIRTUAL;

	unsigned int nBlocksize = 0;
	int nNumblocks = 0;
	m_ExResult = m_pAPISystem->getDSPBufferSize(&nBlocksize, &nNumblocks);

	if (m_pSoundSystem->g_nOutputConfig == 2 || m_pSoundSystem->g_nOutputConfig == 3)
	{
		m_ExResult = m_pEventSystem->init(m_InitSettings.nSoftwareChannels, InitFlags,
		                                  (void*)"audio_capture.wav", FMOD_EVENT_INIT_NORMAL);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("event system init failed pipeing into audio_capture.wav! ", ILog::eError);
			return false;
		}
	}
	else
	{
		// test for windows advanced performance hardware acceleration slider
		if (m_nFMODDriverCaps & FMOD_CAPS_HARDWARE_EMULATED) // This is really bad for latency!.
		{                                                    /* You might want to warn the user about this. */
			m_ExResult = m_pAPISystem->setDSPBufferSize(
			    1024, 10); /* At 48khz, the latency between issuing an fmod command and hearing it will now
			                  be about 213ms. */
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("system set DSP Buffer Size failed! ", ILog::eError);
				// return false;
			}
		}
		m_ExResult =
		    m_pEventSystem->init(m_InitSettings.nSoftwareChannels, InitFlags,
		                         m_pSoundSystem->g_nNetworkAudition ? NULL : m_pHWnd, FMOD_EVENT_INIT_NORMAL);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("event system init failed! ", ILog::eError);
			if (m_ExResult == FMOD_ERR_OUTPUT_CREATEBUFFER)
			{
				// this might be an invalid speaker mode (5.0 or 7.1), so report an error if possible
				// and fallback to stereo
				m_ExResult = m_pAPISystem->setSpeakerMode(FMOD_SPEAKERMODE_STEREO);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("set fallback stereo speaker mode failed! ", ILog::eError);
					return false;
				}

				m_ExResult = m_pEventSystem->init(m_InitSettings.nSoftwareChannels, InitFlags,
				                                  m_pSoundSystem->g_nNetworkAudition ? NULL : m_pHWnd,
				                                  FMOD_EVENT_INIT_NORMAL);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("event system init with fallback stereo speaker mode failed! ",
					                ILog::eError);
					return false;
				}
				else
					CryLogAlways("Sound - initialized FMOD-EX with stereo fallback\n");
			}
			else
				return false;
		}
		else
			CryLogAlways("Sound - initialized FMOD-EX\n");
	}

	// Record driver query
	for (int i = 0; m_ExResult == FMOD_OK; ++i)
	{
		char sRecordDriverName[MAXCHARBUFFERSIZE];
		m_ExResult = m_pAPISystem->getRecordDriverInfo(i, sRecordDriverName, MAXCHARBUFFERSIZE, 0);

		if (m_ExResult == FMOD_OK)
			CryLogAlways("Sound - available record drivers: %d %s !\n", i, sRecordDriverName);
	}

	// set Record Driver for Microphone first
	m_ExResult = FMOD_OK;
	// m_ExResult = m_pAPISystem->setRecordDriver(0);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("init of microphone recording failed! ", ILog::eError);
	}

	uint32 nVersion;
	m_ExResult = m_pAPISystem->getVersion(&nVersion);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("Version number not available!", ILog::eError);
		return false;
	}
	CryLogAlways("Sound - using FMOD version: %08X", nVersion);

	m_ExResult = m_pEventSystem->setMediaPath("..\\Game\\Sounds");
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("invalid media path! ", ILog::eError);
	}

	// m_ExResult = m_pFMODEX->setOutput(FMOD_OUTPUTTYPE_AUTODETECT);
	// m_ExResult = m_pFMODEX->setDriver(0);

	/*
	Set the distance units. (meters/feet etc).
	*/
	const float DISTANCEFACTOR = 1.0f; // Units per meter.  I.e feet would = 3.28.  centimeters would = 100.
	// workaround for FMOD bug: distance attenuation was 1.0f, lowered to fix
	m_ExResult = m_pAPISystem->set3DSettings(1.0, DISTANCEFACTOR, 0.01f);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("distance factor failed! ", ILog::eError);
	}

	// Set memory system to crytek mem manager
	// CS_SetMemorySystem(NULL,NULL,CrySound_Alloc,CrySound_Realloc,CrySound_Free);

	// Assign file access callbacks to fmod to our pak file system.
	// CS_File_SetCallbacks( CrySound_fopen,CrySound_fclose,CrySound_fread,CrySound_fseek,CrySound_ftell );

	// store the amount of software channels
	// ptParamINT32 softwareChannels(softwareChan);
	// bTemp = SetParam(adpSOFTWARECHANNELS ,&softwareChannels);

	//******  Init fmod system setting mixing/playing speed (44100)
	// also requesting 256 software channels
	// flags are -0- for now

	// After init called this sets up hardware channel info for 3d and 2d channels
	GetNumberSoundsPlaying();

	// report info to log file
	// int32 nNuMDriver;
	// char sDriverName[MAXCHARBUFFERSIZE];

	// m_ExResult = m_pCSEX->getDriver(&nNuMDriver);
	// m_ExResult = m_pCSEX->getDriverName(nNuMDriver, sDriverName, MAXCHARBUFFERSIZE);

	int nHW2D = 0;
	int nHW3D = 0;

	CryLog("-------------CRYSOUND-EX-------------");
	m_ExResult = m_pAPISystem->getHardwareChannels(&nHW2D, &nHW3D, &m_nTotalHardwareChannelsAvail);
	CryLog("Total number of 2D hardware channels available: %d", nHW2D);
	CryLog("Total number of 3D hardware channels available: %d", nHW3D);
	CryLog("Total number of all hardware channels available: %d", m_nTotalHardwareChannelsAvail);

	// m_nHardware2DChannels = 5;
	// m_nHardware3DChannels = 5;
	// m_nTotalHardwareChannelsAvail = 10;

	IReverbManager* pReverbManager = m_pSoundSystem->GetIReverbManager();
	if (pReverbManager)
	{
		pReverbManager->Init(this, m_pSoundSystem->g_nReverbInstances);
		pReverbManager->SelectReverb(m_pSoundSystem->g_nReverbType);
	}

	/* After eventsystem create in your code */
	// FMOD::gDebugMode  = FMOD::DEBUG_STDOUT;
	// FMOD::gDebugLevel = FMOD::LOG_NONE;

	// DSP Debugging
	// char sDSPName[512];
	// int nNums = 0;
	// FMOD::DSP* pSoundCard = NULL;
	// m_ExResult = m_pAPISystem->getDSPHead(&pSoundCard);
	// m_ExResult = pSoundCard->getInfo(sDSPName, 0,0,0,0);
	// m_ExResult = pSoundCard->getNumInputs(&nNums);

	// FMOD::DSP* pTargetUnit = NULL;
	// m_ExResult = pSoundCard->getInput(0, &pTargetUnit);
	// m_ExResult = pTargetUnit->getInfo(sDSPName, 0,0,0,0);
	// m_ExResult = pTargetUnit->getNumInputs(&nNums);

	// FMOD::DSP* pFMODMaster = NULL;
	// FMOD::DSP* pMaster = NULL;
	// FMOD::DSP* pReverb = NULL;
	//

	// m_ExResult = pTargetUnit->getNumInputs(&nNums);

	// m_ExResult = pTargetUnit->getInput(0, &pFMODMaster);
	// if (pFMODMaster)
	//{
	//	m_ExResult = pFMODMaster->getNumInputs(&nNums);
	//	m_ExResult = pFMODMaster->getInfo(sDSPName, 0,0,0,0);
	// }

	// m_ExResult = pTargetUnit->getInput(1, &pMaster);
	// if (pMaster)
	//{
	//	FMOD::DSP* pMusic = NULL;
	//	m_ExResult = pMaster->getInfo(sDSPName, 0,0,0,0);
	//	m_ExResult = pMaster->getNumInputs(&nNums);
	//	m_ExResult = pMaster->getInput(0, &pMusic);

	//	if (pMusic)
	//	{
	//		m_ExResult = pMusic->getNumInputs(&nNums);
	//		m_ExResult = pMusic->getInfo(sDSPName, 0,0,0,0);
	//	}
	//}

	return bTemp;
}

// returns ptr to the output device if possible, else NULL
// this might be a pointer to DirectX LPDIRECTSOUND or a WINMM handle
void CAudioDeviceFmodEx400::GetOutputHandle(void** pHandle, EOutputHandle* HandleType)
{
	if (!pHandle || !HandleType || !m_pAPISystem)
		return;

	m_ExResult = m_pAPISystem->getOutputHandle(pHandle);

	FMOD_OUTPUTTYPE OutputType{};
	m_ExResult = m_pAPISystem->getOutput(&OutputType);

	switch (OutputType)
	{
		case FMOD_OUTPUTTYPE_DSOUND:
			*HandleType = eOUTPUT_DSOUND;

			// prevent CRI video codec from not play intro/tutorial videos, instead play them but without
			// any sound
			if (m_nFMODDriverCaps & FMOD_CAPS_HARDWARE_EMULATED)
				*HandleType = eOUTPUT_NOSOUND;
			break;
		case FMOD_OUTPUTTYPE_WINMM:
			*HandleType = eOUTPUT_WINMM;
			break;
		case FMOD_OUTPUTTYPE_WASAPI:
			*HandleType = eOUTPUT_WASAPI;
			break;
		case FMOD_OUTPUTTYPE_OPENAL:
			*HandleType = eOUTPUT_OPENAL;
			break;
		case FMOD_OUTPUTTYPE_ASIO:
			*HandleType = eOUTPUT_ASIO;
			break;
		case FMOD_OUTPUTTYPE_OSS:
			*HandleType = eOUTPUT_OSS;
			break;
		case FMOD_OUTPUTTYPE_ESD:
			*HandleType = eOUTPUT_ESD;
			break;
		case FMOD_OUTPUTTYPE_ALSA:
			*HandleType = eOUTPUT_ALSA;
			break;
		// case FMOD_OUTPUTTYPE_MAC:
		//	HandleType = eOUTPUT_MAC;
		//	break;
		case FMOD_OUTPUTTYPE_XBOX:
			*HandleType = eOUTPUT_Xbox;
			break;
		case FMOD_OUTPUTTYPE_PS2:
			*HandleType = eOUTPUT_PS2;
			break;
		case FMOD_OUTPUTTYPE_GC:
			*HandleType = eOUTPUT_GC;
			break;
		case FMOD_OUTPUTTYPE_NOSOUND:
			*HandleType = eOUTPUT_NOSOUND;
			break;
		case FMOD_OUTPUTTYPE_WAVWRITER:
			*HandleType = eOUTPUT_WAVWRITER;
			break;
		default:
			*HandleType = eOUTPUT_MAX;
	}

	// use secondary DSound system
	if (m_pAPISystemDSound)
	{
		*HandleType = eOUTPUT_DSOUND;
		m_ExResult = m_pAPISystemDSound->getOutputHandle(pHandle);
	}
}

void CAudioDeviceFmodEx400::GetInitSettings(AudioDeviceSettings* InitSettings)
{
	*InitSettings = m_InitSettings;
}

void CAudioDeviceFmodEx400::SetInitSettings(AudioDeviceSettings* InitSettings)
{
	m_InitSettings = *InitSettings;
}

bool CAudioDeviceFmodEx400::ShutDownDevice(void)
{
	bool bTemp = true;
	// this one dosn't need a param
	bTemp = SetParam(adpSTOPALL_CHANNELS, NULL);

	if (m_pEventSystem)
	{
		if (m_pSoundSystem->g_nNetworkAudition)
		{
			m_ExResult = FMOD::NetEventSystem_Shutdown();
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("event system network audition shutdown failed! ", ILog::eError);
				bTemp = false;
			}
		}

		m_ExResult = m_pEventSystem->unload();
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("event system unload failed! ", ILog::eWarning);
			bTemp = false;
		}

		m_ExResult = m_pEventSystem->release();
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("event system release failed! ", ILog::eError);
			bTemp = false;
		}

		m_pEventSystem = NULL;
		m_pAPISystem = NULL;
	}

	if (m_pAPISystemDSound)
	{
		m_ExResult = m_pAPISystemDSound->close();
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("2nd DSound system release failed! ", ILog::eError);
			bTemp = false;
		}

		m_pAPISystemDSound = NULL;
	}

	// if (m_pFMODEX)
	//{
	//	m_ExResult = m_pFMODEX->close();
	//	if (m_ExResult != FMOD_OK)
	//	{
	//		FmodErrorOutput("system object close failed! ", ILog::eError);
	//		bTemp = false;
	//	}

	//	m_ExResult = m_pFMODEX->release();
	//	if (m_ExResult != FMOD_OK)
	//	{
	//		FmodErrorOutput("system object release failed! ", ILog::eError);
	//		bTemp = false;
	//	}

	//	m_pFMODEX = NULL;
	//}

	// m_LoadedProjectFiles.clear();
	m_VecLoadedProjectFiles.clear();

	// refresh categories after unloading projects
	if (m_pSoundSystem->GetIMoodManager())
		m_pSoundSystem->GetIMoodManager()->RefreshCategories();

	return bTemp;
}

bool CAudioDeviceFmodEx400::ResetAudio(void)
{
	bool bTemp = true;
	// GUARD_HEAP;

	assert(m_pEventSystem);

	if (m_pSoundSystem->g_nUnloadData)
	{
		// unload Projects *important* all handles turn invalid now!
		m_ExResult = m_pEventSystem->unload();
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("projects couldnt be unloaded! ", ILog::eError);
			bTemp = false;
		}

		m_VecLoadedProjectFiles.clear();
	}

	// refresh categories after unloading projects
	if (m_pSoundSystem->GetIMoodManager())
		m_pSoundSystem->GetIMoodManager()->RefreshCategories();

	// m_nMemoryStatInc = 0;

	// ShutDownDevice();
	// m_ExResult = FMOD::System_Create(&m_pFMODEX);
	// if (m_ExResult != FMOD_OK)
	//{
	//	FmodErrorOutput("system object create failed! ", ILog::eError);
	//	bTemp = false;
	// }

	// InitAudio(m_pSoundSystem, m_nSoftwareChannels);

	// if (!CS_Init(m_nSystemFrequency , m_nSoftwareChannels, 0))
	// m_ExResult = m_pFMODEX->init(100, FMOD_INIT_NORMAL, m_pHWnd);

	// if (!InitDevice(m_pSoundSystem, m_nSoftwareChannels))
	//{
	//	CryLog("System re-init of CRYSOUND FAILED\n");
	//	ShutDownDevice();
	//	return false;
	// }
	// CryLog("--------------  RE-INIT  --------------------------CRYSOUND VERSION =
	// %f\n",CS_GetVersion()); int32 nSoftwareChannels = 0; m_pFMODEX->getSoftwareChannels(&nSoftwareChannels);
	// CryLog("Total number of channels available: %d\n", nSoftwareChannels);

	// bTemp = (CS_SetHWND(m_pHWnd) ? true : false); // re_set active window

	return bTemp;
}

// this is called every frame to update all listeners and must be called *before* SubSystemUpdate()
bool CAudioDeviceFmodEx400::UpdateListeners(void)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	if (!m_pAPISystem || !m_pEventSystem)
		return false;

	bool bResult = true;
	uint32 nNumActiveListeners = m_pSoundSystem->GetNumActiveListeners();
	FMOD_VECTOR vExPos;
	FMOD_VECTOR vExVel;
	FMOD_VECTOR vExForward;
	FMOD_VECTOR vExTop;

	CListener* pListener = NULL;
	ListenerID nListenerID = LISTENERID_INVALID;
	do
	{
		pListener = (CListener*)m_pSoundSystem->GetNextListener(nListenerID);
		if (pListener)
		{
			nListenerID = pListener->GetID();

			if (pListener->GetActive()) // && pListener->bRotated)
			{
				Vec3 vPos = pListener->GetPosition();
				Vec3 vForward = pListener->GetForward();
				Vec3 vTop = pListener->GetTop();
				vExPos.x = vPos.x;
				vExPos.y = vPos.z;
				vExPos.z = vPos.y;

				if (m_pSoundSystem->g_nDoppler)
				{
					vExVel.x = clamp(pListener->GetVelocity().x, -200.0f, 200.0f);
					vExVel.y = clamp(pListener->GetVelocity().z, -200.0f, 200.0f);
					vExVel.z = clamp(pListener->GetVelocity().y, -200.0f, 200.0f);
				}
				else
				{
					vExVel.x = 0;
					vExVel.y = 0;
					vExVel.z = 0;
				}

				vExForward.x = vForward.x;
				vExForward.y = vForward.z;
				vExForward.z = vForward.y;
				vExTop.x = vTop.x;
				vExTop.y = vTop.z;
				vExTop.z = vTop.y;

				// FMOD-EX
				// m_ExResult = m_pFMODEX->set3DNumListeners(nNumActiveListeners);
				// if (m_ExResult != FMOD_OK)
				//{
				//	FmodErrorOutput("invalid number of listeners! ", ILog::eWarning);
				//	bResult = false;
				//}

				// m_ExResult = m_pFMODEX->set3DListenerAttributes(nListenerID, &vExPos, &vExVel,
				// &vExForward, &vExTop);
				// if (m_ExResult != FMOD_OK)
				//{
				//	FmodErrorOutput("listener 3d update failed! ", ILog::eWarning);
				//	bResult = false;
				//}

				// Event System

				m_ExResult = m_pEventSystem->set3DNumListeners(nNumActiveListeners);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("event system invalid number of listeners! ", ILog::eWarning);
					bResult = false;
				}

				// CryLog("FMOD Update Listener Pos: %.2f,%.2f,%.2f", vPos.x, vPos.y, vPos.z);

				m_ExResult = m_pEventSystem->set3DListenerAttributes(nListenerID, &vExPos, &vExVel,
				                                                     &vExForward, &vExTop);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("event system listener 3d update failed! ", ILog::eWarning);
					bResult = false;
				}

				pListener->MarkAsSet();
			}
		}
	}
	while (pListener);

	return bResult;
}

// must be called every game frame
bool CAudioDeviceFmodEx400::Update(void)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	// float position[3];
	// float velocity[3];
	bool bResult = true;

	// CTimeValue tTimeDiff = gEnv->pTimer->GetAsyncTime() - m_pSoundSystem->m_tUpdateAudioDevice;
	// float fMilliSecs = tTimeDiff.GetMilliSeconds();
	// if (fMilliSecs == 0)
	//	return bResult;

	if (m_pSoundSystem)
	{
		int nDebugLevel = m_pSoundSystem->g_nDebugSound;

		FMOD_DEBUGLEVEL nLevel = FMOD_DEBUG_LEVEL_NONE;

		if (nDebugLevel == SOUNDSYSTEM_DEBUG_FMOD_SIMPLE)
			nLevel = (FMOD_DEBUG_TYPE_EVENT | FMOD_DEBUG_DISPLAY_TIMESTAMPS | FMOD_DEBUG_LEVEL_ERROR);

		if (nDebugLevel == SOUNDSYSTEM_DEBUG_FMOD_COMPLEX)
			nLevel = (FMOD_DEBUG_TYPE_EVENT | FMOD_DEBUG_DISPLAY_TIMESTAMPS | FMOD_DEBUG_LEVEL_ERROR |
			          FMOD_DEBUG_LEVEL_WARNING);

		if (nDebugLevel == SOUNDSYSTEM_DEBUG_FMOD_ALL)
			nLevel = (FMOD_DEBUG_TYPE_EVENT | FMOD_DEBUG_DISPLAY_TIMESTAMPS | FMOD_DEBUG_LEVEL_ERROR |
			          FMOD_DEBUG_LEVEL_WARNING | FMOD_DEBUG_LEVEL_HINT | FMOD_DEBUG_TYPE_MEMORY);

		m_ExResult = FMOD::Debug_SetLevel(nLevel);
	}

	int nUpdateLoops = 0;

	if (m_pSoundSystem->g_nOutputConfig == 3)
	{
		CTimeValue tCurrent = gEnv->pTimer->GetFrameStartTime();
		CTimeValue tTimeDiff = tCurrent - m_tLastUpdate;
		m_tLastUpdate = tCurrent;

		// CTimeValue tTimeDiff = (gEnv->pTimer->GetAsyncTime() - gEnv->pTimer->GetFrameStartTime());
		// //m_pSoundSystem->m_tUpdateAudioDevice;
		float fMS = tTimeDiff.GetMilliSeconds();

		if (fMS < 5000.0f) // prevent long pause (5 sec) of silence when activated at runtime
		{
			while (abs(fMS) > 21.33f)
			{
				++nUpdateLoops;
				fMS -= 21.33f;
			}
			tTimeDiff.SetMilliSeconds((int64)fMS);
			m_tLastUpdate -= tTimeDiff;
		}
	}
	else
		nUpdateLoops = 1;

	// update crysound must be called every frame
	while (m_pEventSystem && nUpdateLoops > 0)
	{
		--nUpdateLoops;

		{
			FRAME_PROFILER("FMOD-EventUpdate", GetISystem(), PROFILE_SOUND);
			m_ExResult = m_pEventSystem->update();
		}

		if (m_ExResult != FMOD_OK)
		{
			// disable output for VS2
			FmodErrorOutput("event system update failed! ", ILog::eWarning);
			bResult = false;
		}

		if (m_pSoundSystem->g_nNetworkAudition)
		{
			m_ExResult = FMOD::NetEventSystem_Update();
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("event system network audition update failed! ", ILog::eWarning);
				bResult = false;
			}
		}

		if (m_pSoundSystem->g_nProfiling > 0)
		{
			FMOD_EVENT_SYSTEMINFO SystemInfo;
			memset(&SystemInfo, 0, sizeof(FMOD_EVENT_SYSTEMINFO));

			m_ExResult = m_pEventSystem->getInfo(&SystemInfo);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("event system get system info failed! ", ILog::eWarning);
				bResult = false;
			}

			// nSizeInMB += Info.instancememory;

			for (int i = 0; i < SystemInfo.numwavebanks; ++i)
			{
				IWavebank* pWavebank = nullptr;
				{
					const char* name = SystemInfo.wavebankinfo[i].name;
					const auto it = m_wavebanks.find(name);
					if (it == m_wavebanks.end())
					{
						const auto [newIt, added] = m_wavebanks.emplace(
						    name, std::make_unique<CWavebankFmodEx400>(name));
						pWavebank = newIt->second.get();
					}
					else
					{
						pWavebank = it->second.get();
					}
				}

				IWavebank::SWavebankInfo BankInfo;
				BankInfo.nMemCurrentlyInByte =
				    SystemInfo.wavebankinfo[i].streammemory + SystemInfo.wavebankinfo[i].samplememory;
				BankInfo.nMemPeakInByte =
				    SystemInfo.wavebankinfo[i].streammemory + SystemInfo.wavebankinfo[i].samplememory;

				if (*pWavebank->GetPath())
				{
					CryFile file;

					m_sFullWaveBankName = "";

					int nPathLength = strlen(pWavebank->GetPath());
					int nNameLength = strlen(pWavebank->GetName());
					if (nPathLength + nNameLength + 4 < 512)
					{
						m_sFullWaveBankName = pWavebank->GetPath();
						m_sFullWaveBankName += pWavebank->GetName();
						m_sFullWaveBankName += ".fsb";

						if (file.Open(m_sFullWaveBankName.c_str(), "rb"))
							BankInfo.nFileSize = file.GetSize();
					}
				}
				pWavebank->AddInfo(BankInfo);

				// nSizeInMB += Info.wavebankinfo[i].streammemory + Info.wavebankinfo[i].samplememory;
			}

			// nSizeInMB = nSizeInMB/(1024*1024);
		}
		FindLostEvent(); // call to find and stop MP-looping-sound-bug
	}

	return bResult;
}

int CAudioDeviceFmodEx400::GetMemoryStats(void)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	if (m_pAPISystem)
	{
		m_ExResult = FMOD::Memory_GetStats(&m_nCurrentMemAlloc, &m_nMaxMemAlloc);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("system get memory stats failed! ", ILog::eWarning);
			return 0;
		}
	}

	return m_nCurrentMemAlloc;
}

bool CAudioDeviceFmodEx400::IsEax(void)
{
	if (m_pAPISystem && !m_nFMODDriverCaps)
	{
		// int nDriver = 0;

		if (m_nSpeakerMode != FMOD_SPEAKERMODE_MAX)
			// m_ExResult = m_pFMODEX->getDriver(&nDriver);
			m_ExResult = m_pAPISystem->getDriverCaps(0, &m_nFMODDriverCaps, 0, 0, &m_nSpeakerMode);
		else
			m_ExResult = m_pAPISystem->getDriverCaps(0, &m_nFMODDriverCaps, 0, 0, 0);

		if (m_nFMODDriverCaps & FMOD_CAPS_HARDWARE_EMULATED)
			CryLog(" *WARNING* Hardware acceleration turned off sound performance settings!");

		if (m_nFMODDriverCaps & FMOD_CAPS_HARDWARE)
			CryLog(" Driver supports hardware 3D sound");

		if (m_nFMODDriverCaps & FMOD_CAPS_REVERB_EAX2)
			CryLog(" Driver supports EAX 2.0 reverb");

		if (m_nFMODDriverCaps & FMOD_CAPS_REVERB_EAX3)
			CryLog(" Driver supports EAX 3.0 reverb");

		if (m_nFMODDriverCaps & FMOD_CAPS_REVERB_EAX4)
			CryLog(" Driver supports EAX 4.0 reverb");
	}

	return (m_nFMODDriverCaps & (FMOD_CAPS_REVERB_EAX2 | FMOD_CAPS_REVERB_EAX3 | FMOD_CAPS_REVERB_EAX4)) ? true
	                                                                                                     : false;
}

int CAudioDeviceFmodEx400::GetNumberSoundsPlaying(void)
{
	int nTemp = 0;
	ptParamINT32 Param(nTemp);
	GetParam(adpCHANNELS_PLAYING, &Param);
	Param.GetValue(nTemp);

	return nTemp;
}

// percent of cpu usage by CrySound mixer,dsound, and streams
float CAudioDeviceFmodEx400::GetCpuUsage(void)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	float fDSP = 0.0f;
	float fStream = 0.0f;
	float fUpdate = 0.0f;
	float fTotal = 0.0f;

	if (m_pAPISystem)
	{
		m_ExResult = m_pAPISystem->getCPUUsage(&fDSP, &fStream, &fUpdate, &fTotal);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("system get CPU usage failed! ", ILog::eWarning);
			return 0.0f;
		}
	}

	return fTotal;
}

// sets whole sound system's frequency
bool CAudioDeviceFmodEx400::SetFrequency(int newFreq)
{
	bool bTemp = true;

	m_nSystemFrequency = newFreq;

	// bTemp = (CS_SetFrequency(CS_ALL,newFreq) ? true : false);
	return bTemp;
}

// compute memory-consumption, returns rough estimate in MB
int CAudioDeviceFmodEx400::GetMemoryUsage(class ICrySizer* pSizer)
{
	// TODO MAJOR REVIEW HERE !

	int nSizeInByte = 0;
	int nWavebank = 0;

	if (pSizer)
	{
		if (!pSizer->Add(*this))
			return 0;

		// SIZER_COMPONENT_NAME(pSizer, "FMOD");
	}

	FMOD_EVENT_SYSTEMINFO SystemInfo;
	memset(&SystemInfo, 0, sizeof(FMOD_EVENT_SYSTEMINFO));

	m_ExResult = m_pEventSystem->getInfo(&SystemInfo);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("event system get system info failed! ", ILog::eWarning);
		return 0;
	}

	// CSoundBuffer *pBuffer=m_pSoundBuffer;
	int nFMODCurrent = 0;
	int nMax = 0;
	m_ExResult = FMOD::Memory_GetStats(&nFMODCurrent, &nMax);
	if (m_ExResult != FMOD_OK)
	{
		FmodErrorOutput("system get memory stats failed! ", ILog::eWarning);
		return 0;
	}

	nSizeInByte += nFMODCurrent;

	int nTempMemory = 0;

	if (pSizer)
	{
		pSizer->AddObject(m_pAPISystem, nFMODCurrent);
	}

	nTempMemory = SystemInfo.eventmemory + SystemInfo.instancememory + SystemInfo.dspmemory;

	for (int i = 0; i < SystemInfo.numwavebanks; ++i)
	{
		nWavebank += SystemInfo.wavebankinfo[i].streammemory + SystemInfo.wavebankinfo[i].samplememory;
		nTempMemory += SystemInfo.wavebankinfo[i].streammemory + SystemInfo.wavebankinfo[i].samplememory;
	}

	return nSizeInByte;
}

// accesses wavebanks
IWavebank* CAudioDeviceFmodEx400::GetWavebank(int nIndex)
{
	if (nIndex < 0 || nIndex >= static_cast<int>(m_wavebanks.size()))
	{
		return nullptr;
	}

	return std::next(m_wavebanks.begin(), nIndex)->second.get();
}

// accesses wavebanks
IWavebank* CAudioDeviceFmodEx400::GetWavebank(const char* sWavebankName)
{
	if (!sWavebankName)
	{
		return nullptr;
	}

	const auto it = m_wavebanks.find(sWavebankName);
	if (it == m_wavebanks.end())
	{
		return nullptr;
	}

	return it->second.get();
}

// MVD
//  creates a new platform dependent SoundBuffer
CSoundBuffer* CAudioDeviceFmodEx400::CreateSoundBuffer(const SSoundBufferProps& BufferProps)
{
	CSoundBuffer* pBuf = NULL;

	if (BufferProps.eBufferType == btEVENT)
		pBuf = new CSoundBufferFmodEx400Event(BufferProps, m_pAPISystem);

	if (BufferProps.eBufferType == btMICRO)
		pBuf = new CSoundBufferFmodEx400Micro(BufferProps, m_pAPISystem);

	if (BufferProps.eBufferType == btNETWORK)
		pBuf = new CSoundBufferFmodEx400Network(BufferProps, m_pAPISystem);

	if (!pBuf)
		pBuf = new CSoundBufferFmodEx400(BufferProps, m_pAPISystem);

	return pBuf;
}

// creates a new platform dependent PlatformSound
IPlatformSound* CAudioDeviceFmodEx400::CreatePlatformSound(CSound* pSound, const char* sEventName)
{
	IPlatformSound* pPlatform = NULL;

	if (sEventName[0])
	{
		if (m_UnusedPlatformSoundEvents.empty())
			pPlatform = new CPlatformSoundFmodEx400Event(pSound, m_pAPISystem, sEventName);
		else
		{
			pPlatform = m_UnusedPlatformSoundEvents[m_UnusedPlatformSoundEvents.size() - 1];
			pPlatform->Reset(pSound, sEventName);
			m_UnusedPlatformSoundEvents.pop_back();
		}
	}
	else
	{
		if (m_UnusedPlatformSounds.empty())
			pPlatform = new CPlatformSoundFmodEx400(pSound, m_pAPISystem);
		else
		{
			pPlatform = m_UnusedPlatformSounds[m_UnusedPlatformSounds.size() - 1];
			pPlatform->Reset(pSound, "");
			m_UnusedPlatformSounds.pop_back();
		}
	}

	return pPlatform;
}

bool CAudioDeviceFmodEx400::RemovePlatformSound(IPlatformSound* pPlatformSound)
{
	if (pPlatformSound)
	{
		if (pPlatformSound->GetClass() == pscEVENT)
			m_UnusedPlatformSoundEvents.push_back((CPlatformSoundFmodEx400Event*)pPlatformSound);
		else
			m_UnusedPlatformSounds.push_back((CPlatformSoundFmodEx400*)pPlatformSound);
	}

	return true;
}

// check for multiple record devices and expose them, write name to sName pointer
bool CAudioDeviceFmodEx400::GetRecordDeviceInfo(const int nRecordDevice, char* sName, const int nNameLength)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	if (m_pAPISystem)
	{
		m_ExResult = m_pAPISystem->getRecordDriverInfo(nRecordDevice, sName, nNameLength, 0);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("retrieve microphone record information failed! ", ILog::eWarning);
			return false;
		}
	}
	return true;
}

bool CAudioDeviceFmodEx400::GetParam(enumAudioDeviceParamSemantics enumtype, ptParam* pParam)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	int nTemp = 0;
	float fTemp = 0.0f;
	bool bTemp = false;
	;
	Vec3 vTemp;
	string sTemp;
	void* vpTemp = NULL;
	float tempVect[3];
	bool result = true;

	switch (enumtype)
	{
		case adpDOPPLER: // turn doppler on or off with a bool
			if (!(pParam->GetValue(bTemp)))
				return false;
			bTemp = m_bDopplerStatus;
			pParam->SetValue(bTemp);
			break;
		case adpHARDWARECHANNELS: // number of hardware channels available
			if (!(pParam->GetValue(nTemp)))
				return false;
			GetNumberSoundsPlaying();
			nTemp = m_nTotalHardwareChannelsAvail;
			pParam->SetValue(nTemp);
			break;
		case adpTOTALCHANNELS: // total number of channels available
			if (!(pParam->GetValue(nTemp)))
				return false;
			GetNumberSoundsPlaying();
			nTemp = m_nTotalHardwareChannelsAvail + m_InitSettings.nSoftwareChannels;
			pParam->SetValue(nTemp);
			break;
		case adpEAX_STATUS: // is eax available on this machine
			if (!(pParam->GetValue(bTemp)))
				return false;
			bTemp = IsEax();
			pParam->SetValue(bTemp);
			break;
		case adpMUTE_STATUS: // returns bool telling caller if system wide mute on=true or off=false
			if (!(pParam->GetValue(bTemp)))
				return false;
			bTemp = m_bMuteStatus;
			pParam->SetValue(bTemp);
			break;
		case adpLISTENER_POSITION: // gets active Listeners position
			if (!(pParam->GetValue(vTemp)))
				return false;
			tempVect[0] = tempVect[1] = tempVect[2] = 0.0f;
			// CS_3D_Listener_GetAttributes(tempVect, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
			vTemp(tempVect[0], tempVect[2], tempVect[1]);
			pParam->SetValue(vTemp);
			break;
		case adpLISTENER_VELOCITY: // gets active Listeners velocity
			if (!(pParam->GetValue(vTemp)))
				return false;
			tempVect[0] = tempVect[1] = tempVect[2] = 0.0f;
			// CS_3D_Listener_GetAttributes(NULL, tempVect, NULL, NULL, NULL, NULL, NULL, NULL);
			vTemp(tempVect[0], tempVect[2], tempVect[1]);
			pParam->SetValue(vTemp);
			break;
		case adpLISTENER_FORWARD: // gets active Listeners direction or forward
			if (!(pParam->GetValue(vTemp)))
				return false;
			tempVect[0] = tempVect[1] = tempVect[2] = 0.0f;
			// CS_3D_Listener_GetAttributes(NULL, NULL, &tempVect[0], &tempVect[1], &tempVect[2], NULL,
			// NULL, NULL);
			vTemp(tempVect[0], tempVect[2], tempVect[1]);
			pParam->SetValue(vTemp);
			break;
		case adpLISTENER_TOP: // gets active Listeners top
			if (!(pParam->GetValue(vTemp)))
				return false;
			tempVect[0] = tempVect[1] = tempVect[2] = 0.0f;
			// CS_3D_Listener_GetAttributes(NULL, NULL, NULL, NULL, NULL, &tempVect[0], &tempVect[1],
			// &tempVect[2]);
			vTemp(tempVect[0], tempVect[2], tempVect[1]);
			pParam->SetValue(vTemp);
			break;
		case adpWINDOW_HANDLE: // returns handle of active window which is void * type
			if (!(pParam->GetValue(vpTemp)))
				return false;
			vpTemp = m_pHWnd;
			pParam->SetValue(vpTemp);
			break;
		case adpSYSTEM_FREQUENCY: // gets the mixing speed
			if (!(pParam->GetValue(nTemp)))
				return false;
			// nTemp = CS_GetFrequency(CS_ALL);
			pParam->SetValue(nTemp);
			break;
		case adpMASTER_VOLUME: // value from 0-1, 1 being max fmod master volume
		{
			if (!(pParam->GetValue(fTemp)))
				return false;

			FMOD::ChannelGroup* MasterChannelGroup = 0;
			m_ExResult = m_pAPISystem->getMasterChannelGroup(&MasterChannelGroup);
			if (m_ExResult == FMOD_OK)
			{
				m_ExResult = MasterChannelGroup->getVolume(&fTemp);
			}
			pParam->SetValue(nTemp);
			break;
		}
		case adpMASTER_PAUSE: // value true/false
		{
			if (!(pParam->GetValue(bTemp)))
				return false;

			FMOD::ChannelGroup* MasterChannelGroup = 0;
			m_ExResult = m_pAPISystem->getMasterChannelGroup(&MasterChannelGroup);
			if (m_ExResult == FMOD_OK)
			{
				m_ExResult = MasterChannelGroup->getPaused(&bTemp);
			}
			pParam->SetValue(nTemp);
			break;
		}
		case adpSPEAKER_MODE: // gets active speaker mode
			if (!(pParam->GetValue(nTemp)))
				return false;
			switch (m_nSpeakerMode)
			{
				case FMOD_SPEAKERMODE_MONO:
					nTemp = 1;
					break;
				case FMOD_SPEAKERMODE_STEREO:
					nTemp = 2;
					break;
				case FMOD_SPEAKERMODE_QUAD:
					nTemp = 4;
					break;
				case FMOD_SPEAKERMODE_5POINT1:
					nTemp = 5;
					break;
				case FMOD_SPEAKERMODE_7POINT1:
					nTemp = 7;
					break;
				case FMOD_SPEAKERMODE_PROLOGIC:
					nTemp = 9;
					break;
			}
			pParam->SetValue(nTemp);
			break;
		case adpPAUSED_STATUS: // is fmod paused or not
			if (!(pParam->GetValue(bTemp)))
				return false;
			bTemp = m_bSystemPaused;
			pParam->SetValue(bTemp);
			break;
		case adpSNDSYSTEM_MEM_USAGE_NOW: // returns how much memory fmod is using RIGHT NOW
			if (!(pParam->GetValue(nTemp)))
				return false;
			GetMemoryStats();
			nTemp = m_nCurrentMemAlloc;
			pParam->SetValue(nTemp);
			break;
		case adpSNDSYSTEM_MEM_USAGE_HIGHEST: // returns the amount of memory fmod used at its PEEK
			if (!(pParam->GetValue(nTemp)))
				return false;
			GetMemoryStats();
			nTemp = m_nMaxMemAlloc;
			pParam->SetValue(nTemp);
			break;
		case adpSNDSYSTEM_OUTPUTTYPE: // returns a enum in int form of output type
			/*
			CS_OUTPUT_NOSOUND,     NoSound driver, all calls to this succeed but do nothing.
			CS_OUTPUT_WINMM,     Windows Multimedia driver.
			CS_OUTPUT_DSOUND,    DirectSound driver.  You need this to get EAX2 or EAX3 support, or FX api
			support. CS_OUTPUT_A3D,       A3D driver.

			CS_OUTPUT_OSS,        Linux/Unix OSS (Open Sound System) driver, i.e. the kernel sound drivers.
			CS_OUTPUT_ESD,        Linux/Unix ESD (Enlightment Sound Daemon) driver.
			CS_OUTPUT_ALSA,       Linux Alsa driver.

			CS_OUTPUT_ASIO,       Low latency ASIO driver
			CS_OUTPUT_XBOX,       Xbox driver
			CS_OUTPUT_PS2,        PlayStation 2 driver
			CS_OUTPUT_MAC,        Mac SoundManager driver
			CS_OUTPUT_GC,         Gamecube driver
			CS_OUTPUT_NOSOUND_NONREALTIME   This is the same as nosound, but the sound generation is driven
			by CS_Update
			*/
			if (!(pParam->GetValue(nTemp)))
				return false;
			// nTemp = CS_GetOutput();
			pParam->SetValue(nTemp);
			break;
		case adpSNDSYSTEM_DRIVER: // sets a string name of driver
			if (!(pParam->GetValue(sTemp)))
				return false;
			// sTemp = CS_GetDriverName(CS_GetOutput());
			pParam->SetValue(sTemp);
			break;
		case adpMIXING_BUFFER_SIZE: // ****** OPTIONAL ****** if set returns mixing buffer size in milliseconds
		                            // (fmod does automaticly)
			if (!(pParam->GetValue(nTemp)))
				return false;
			nTemp = m_nMixingBuffSize;
			pParam->SetValue(nTemp);
			break;
		case adpCHANNELS_PLAYING: // returns number of channels being used
		{
			if (!(pParam->GetValue(nTemp)) || !m_pAPISystem)
				return false;

			m_ExResult = m_pAPISystem->getChannelsPlaying(&nTemp);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("system get channels playing failed! ", ILog::eWarning);
				return false;
			}

			pParam->SetValue(nTemp);
			return false; // disable FMOD-EX polling because event system is ignored (BUG)
			break;
		}
		case adpSTOPALL_CHANNELS: // ******* NOT ACTIVE !!!!!
			break;
		case adpEVENTCOUNT:
			if (!(pParam->GetValue(nTemp)))
				return false;
			nTemp = m_nCountEvent;
			pParam->SetValue(nTemp);
			break;
		case adpGROUPCOUNT:
			if (!(pParam->GetValue(nTemp)))
				return false;
			nTemp = m_nCountGroup;
			pParam->SetValue(nTemp);
			break;

		default:
			break;
	}
	return result;
}

bool CAudioDeviceFmodEx400::SetParam(enumAudioDeviceParamSemantics enumtype, ptParam* pParam)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	int nTemp = 0;
	float fTemp = 0.0f;
	bool bTemp = false;
	Vec3 vTemp;
	string sTemp;
	void* vpTemp = NULL;
	bool result = true;

	switch (enumtype)
	{
		case adpHARDWARECHANNELS:
		case adpTOTALCHANNELS:
		case adpEAX_STATUS: // these values are not used at this time
		case adpCHANNELS_PLAYING:
		case adpSNDSYSTEM_MEM_USAGE_NOW:
		case adpSNDSYSTEM_MEM_USAGE_HIGHEST:
			result = false;
			break;
		case adpMIXING_BUFFER_SIZE: // ****OPTIONAL**** set the size of the mixing buffer in milliSeconds
			if (!(pParam->GetValue(nTemp)))
				return false;
			m_nMixingBuffSize = nTemp;
			// result = (CS_SetBufferSize(nTemp) ? true : false);
			break;
		case adpMUTE_STATUS: // changes the system mute status ,bTemp true mutes the system false reactivates
		                     // sound system
			if (!(pParam->GetValue(bTemp)))
				return false;
			m_bMuteStatus = bTemp;
			// result = (CS_SetMute(CS_ALL, (signed char)bTemp) ? true : false);
			break;

		case adpLISTENER_POSITION: // sets listener position
			return false;

			break;
		case adpLISTENER_VELOCITY: // sets 3d listener velocity
			return false;

			break;
		case adpLISTENER_FORWARD: // sets 3d listener forward value
			return false;

			break;
		case adpLISTENER_TOP: // sets 3d listener top value
			return false;

			break;
		case adpWINDOW_HANDLE: // sets fmod window output handle
			if (!(pParam->GetValue(vpTemp)))
				return false;
			// result = (CS_SetHWND(vpTemp) ? true : false);
			m_pHWnd = vpTemp;
			break;
		case adpSYSTEM_FREQUENCY: // sets fmod global mixing speed
			if (!(pParam->GetValue(nTemp)))
				return false;
			m_nSystemFrequency = nTemp;
			// result = (CS_SetFrequency(CS_ALL, nTemp) ? true : false);
			break;
		case adpMASTER_VOLUME: // sets the fmod master volume 0 - 1, 1 being highest volume
		{
			if (!(pParam->GetValue(fTemp)) || !m_pAPISystem)
				return false;

			if (fTemp < 0.0f)
				fTemp = 0.0f;
			else if (fTemp > 1.0f)
				fTemp = 1.0f;

			FMOD::ChannelGroup* pChannelGroup = 0;
			m_ExResult = m_pAPISystem->getMasterChannelGroup(&pChannelGroup);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("system get master channel group failed! ", ILog::eWarning);
				return false;
			}

			m_ExResult = pChannelGroup->setVolume(fTemp);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("master channel group set volume failed! ", ILog::eWarning);
				return false;
			}

			break;
		}
		case adpMASTER_PAUSE: // value true/false
		{
			if (!(pParam->GetValue(bTemp)))
				return false;

			FMOD::ChannelGroup* MasterChannelGroup = 0;
			m_ExResult = m_pAPISystem->getMasterChannelGroup(&MasterChannelGroup);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("system get master channel group failed! ", ILog::eWarning);
				return false;
			}

			m_ExResult = MasterChannelGroup->setPaused(bTemp);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("master channel group set pause failed! ", ILog::eWarning);
				return false;
			}

			if (m_pEventSystem)
			{
				FMOD::EventCategory* pMasterCategory = NULL;
				m_ExResult = m_pEventSystem->getCategory("master", &pMasterCategory);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("get master category failed! ", ILog::eWarning);
					return false;
				}

				m_ExResult = pMasterCategory->setPaused(bTemp);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("master category set pause failed! ", ILog::eWarning);
					return false;
				}
			}

			break;
		}
		case adpPITCH:
		{
			if (!(pParam->GetValue(fTemp)) || !m_pEventSystem)
				return false;

			if (fTemp < -4.0f)
				fTemp = -4.0f;
			else if (fTemp > 4.0f)
				fTemp = 4.0f;

			FMOD::EventCategory* pPlatformCategory = NULL;
			m_ExResult = m_pEventSystem->getCategory("master", &pPlatformCategory);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("system get master category failed! ", ILog::eWarning);
				return false;
			}

			m_ExResult = pPlatformCategory->setPitch(fTemp);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("master category set pitch failed! ", ILog::eWarning);
				return false;
			}

			break;
		}

		case adpSPEAKER_MODE: // sets fmod speaker enum

			//	typedef enum
			//	{
			//	FMOD_SPEAKERMODE_RAW,              /* There is no specific speakermode.  Sound channels
			// are mapped in order of input to output.  See remarks for more information. */
			//	FMOD_SPEAKERMODE_MONO,             /* The speakers are monaural. */
			//	FMOD_SPEAKERMODE_STEREO,           /* The speakers are stereo (default value). */
			//	FMOD_SPEAKERMODE_4POINT1,          /* 4.1 speaker setup.  This includes front, center,
			// left, rear and a subwoofer. Also known as a "quad" speaker configuration. */
			//	FMOD_SPEAKERMODE_5POINT1,          /* 5.1 speaker setup.  This includes front, center,
			// left, rear left, rear right and a subwoofer. */ 	FMOD_SPEAKERMODE_7POINT1, /* 7.1 speaker
			// setup.  This includes front, center, left, rear left, rear right, side left, side right and a
			// subwoofer. */ 	FMOD_SPEAKERMODE_PROLOGIC,         /* Stereo output, but data is encoded
			// in a way that is picked up by a Prologic/Prologic2 decoder and split into a 5.1 speaker
			// setup. */

			//	FMOD_SPEAKERMODE_MAX,              /* Maximum number of speaker modes supported. */
			//	FMOD_SPEAKERMODE_FORCEINT = 65536  /* Makes sure this enum is signed 32bit. */
			//} FMOD_SPEAKERMODE;

			//" 0: Control Panel Settings\n"
			//"	1: Mono\n"
			//"	2: Stereo\n"
			//"	3: Headphone\n"
			//"	4: 4Point1\n"
			//"	5: 5Point1\n"
			////"	6: Surround\n"
			//"	7: 7Point1\n"
			//"	9: Prologic\n"

			// ***** Setspeakermode also sets the PAN separation setting to -0- if set to mono and -1- if
			// stereo
			if (!(pParam->GetValue(nTemp)))
				return false;
			switch (nTemp)
			{
				case 0:
					IsEax(); // polls the state of the control panel;
					break;
				case 1:
					m_nSpeakerMode = FMOD_SPEAKERMODE_MONO;
					break;
				case 2:
					m_nSpeakerMode = FMOD_SPEAKERMODE_STEREO;
					break;
				case 3:
					m_nSpeakerMode = FMOD_SPEAKERMODE_STEREO;
					break;
				case 4:
					m_nSpeakerMode = FMOD_SPEAKERMODE_QUAD;
					break;
				case 5:
					m_nSpeakerMode = FMOD_SPEAKERMODE_5POINT1;
					break;
				case 6:
					m_nSpeakerMode = FMOD_SPEAKERMODE_PROLOGIC;
					break;
				case 7:
					m_nSpeakerMode = FMOD_SPEAKERMODE_7POINT1;
					break;
			}
			// ResetAudio();
			// m_ExResult = m_pFMODEX->setSpeakerMode(m_nSpeakerMode);
			break;
		case adpPAUSED_STATUS: // pauses or un pauses system
			if (!(pParam->GetValue(bTemp)))
				return false;
			m_bSystemPaused = bTemp;
			// result = (CS_SetPaused(CS_ALL, bTemp) ? true : false);
			break;
			/*
			      CS_OUTPUT_NOSOUND,     NoSound driver, all calls to this succeed but do nothing.
			        CS_OUTPUT_WINMM,     Windows Multimedia driver.
			        CS_OUTPUT_DSOUND,    DirectSound driver.  You need this to get EAX2 or EAX3 support, or
			   FX api support. CS_OUTPUT_A3D,       A3D driver.

			        CS_OUTPUT_OSS,        Linux/Unix OSS (Open Sound System) driver, i.e. the kernel sound
			   drivers. CS_OUTPUT_ESD,        Linux/Unix ESD (Enlightment Sound Daemon) driver.
			        CS_OUTPUT_ALSA,       Linux Alsa driver.

			        CS_OUTPUT_ASIO,       Low latency ASIO driver
			        CS_OUTPUT_XBOX,       Xbox driver
			        CS_OUTPUT_PS2,        PlayStation 2 driver
			        CS_OUTPUT_MAC,        Mac SoundManager driver
			        CS_OUTPUT_GC,         Gamecube driver
			        CS_OUTPUT_NOSOUND_NONREALTIME   This is the same as nosound, but the sound generation is
			   driven by CS_Update
			        */
		case adpSNDSYSTEM_OUTPUTTYPE: // **** OPTIONAL ***** (-1 lets fmod pick best one) sets the fmod mixer
		                              // driver type
		case adpSNDSYSTEM_DRIVER:
			if (!(pParam->GetValue(nTemp)))
				return false;

			// -1 tells the fmod system to pick the best one -- these are only valid choices
			if ((nTemp == -1) || (nTemp == FMOD_OUTPUTTYPE_NOSOUND) || (nTemp == FMOD_OUTPUTTYPE_WINMM) ||
			    (nTemp == FMOD_OUTPUTTYPE_DSOUND))
				// result = (CS_SetOutput(nTemp) ? true : false);
				break;

		case adpSTOPALL_CHANNELS: // stops all channels
		                          // m_pFMODEX->
			// result = (CS_StopSound(CS_ALL) ? true : false);
			break;

		case adpDOPPLER: // turn doppler on or off with a bool
			if (!(pParam->GetValue(bTemp)))
				return false;
			m_bDopplerStatus = bTemp;
			break;

		default:
			break;
	}
	return result;
}

// return handle if Project is available
FMOD::EventProject* CAudioDeviceFmodEx400::LoadProjectFile(const string& sProjectPath, const string& sProjectName)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	int nNum = m_VecLoadedProjectFiles.size();

	if (m_pEventSystem)
	{
		string sFull = sProjectPath + "/" + sProjectName;

		VecProjectFilesIter IterEnd = m_VecLoadedProjectFiles.end();
		for (VecProjectFilesIter Iter = m_VecLoadedProjectFiles.begin(); Iter != IterEnd; ++Iter)
		{
			if (sFull == (*Iter).sProjectName)
				return (*Iter).ProjectHandle;
		}

		if (true)
		{
			SProjectFile NewProject;
			NewProject.sProjectName = sFull;
			NewProject.ProjectHandle = NULL;

			m_ExResult = m_pEventSystem->setMediaPath((char*)sProjectPath.c_str());
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("set event media path failed! ", ILog::eWarning);
				return NULL;
			}

			FMOD_EVENT_LOADINFO Info;
			Info.size = sizeof(Info);
			Info.encryptionkey = 0;
			Info.loadfrommemory_length = 0;
			Info.sounddefentrylimit = m_pSoundSystem->g_fVariationLimiter;

			m_ExResult =
			    m_pEventSystem->load((char*)sProjectName.c_str(), &Info, &NewProject.ProjectHandle);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput(("load event project failed! " + sProjectName).c_str(), ILog::eError);
				return NULL;
			}

			m_VecLoadedProjectFiles.push_back(NewProject);

			// refresh categories after loading a new project
			if (m_pSoundSystem->GetIMoodManager())
				m_pSoundSystem->GetIMoodManager()->RefreshCategories();

			// update group and event counting
			UpdateGroupEventCount();
			return NewProject.ProjectHandle;
		}
	}

	return NULL;
}

void CAudioDeviceFmodEx400::UpdateGroupEventCount()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	m_nCountProject = 0;
	m_nCountGroup = 0;
	m_nCountEvent = 0;
	int nProjectIndex = 0;
	int nGroupIndex = 0;
	m_ExResult = FMOD_OK;
	FMOD::EventProject* pProject = NULL;
	FMOD::EventGroup* pGroup = NULL;

	while (m_ExResult == FMOD_OK)
	{
		m_ExResult = m_pEventSystem->getProjectByIndex(nProjectIndex, &pProject);
		if (m_ExResult != FMOD_OK)
		{
			// FmodErrorOutput("event system get project by index failed! ", ILog::eWarning);
		}

		if (pProject && (m_ExResult == FMOD_OK))
		{

			while (m_ExResult == FMOD_OK)
			{
				m_ExResult = pProject->getGroupByIndex(nGroupIndex, false, &pGroup);
				if (m_ExResult != FMOD_OK)
				{
					// FmodErrorOutput("project get group by index failed! ", ILog::eWarning);
				}

				if (pGroup && (m_ExResult == FMOD_OK))
				{
					++nGroupIndex;
					++m_nCountGroup;
					GroupEventCount(pGroup, pProject);
				}
			}
			nGroupIndex = 0;
			m_ExResult = FMOD_OK;
			++nProjectIndex;
			++m_nCountProject;
		}
	}
}

void CAudioDeviceFmodEx400::GroupEventCount(FMOD::EventGroup* pParentGroup, FMOD::EventProject* pProject)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SOUND);

	int nGroupIndex = 0;
	int nEventIndex = 0;
	m_ExResult = FMOD_OK;
	FMOD::EventGroup* pGroup = NULL;
	FMOD::Event* pEvent = NULL;

	char* sGroupName = 0;
	char* sProjectName = 0;
	char* sEventtName = 0;

	if (m_pSoundSystem->g_nDumpEventStructure)
	{
		pProject->getInfo(0, &sProjectName);
	}

	// counting Groups
	while (m_ExResult == FMOD_OK)
	{
		m_ExResult = pParentGroup->getGroupByIndex(nGroupIndex, false, &pGroup);
		if (m_ExResult != FMOD_OK)
		{
			// FmodErrorOutput("group get group by index failed! ", ILog::eWarning);
		}

		if (pGroup && (m_ExResult == FMOD_OK))
		{
			++nGroupIndex;
			++m_nCountGroup;
			GroupEventCount(pGroup, pProject);
		}
	}

	// counting Events
	int nNumEvents = 0;
	m_ExResult = pParentGroup->getNumEvents(&nNumEvents);
	if (m_ExResult != FMOD_OK)
	{
		// FmodErrorOutput("group get num. events failed! ", ILog::eWarning);
	}

	if (m_pSoundSystem->g_nDumpEventStructure)
	{
		pParentGroup->getInfo(0, &sGroupName);
		for (int i = 0; i < nNumEvents; ++i)
		{
			m_ExResult = pParentGroup->getEventByIndex(i, FMOD_EVENT_INFOONLY, &pEvent);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("group get event by index failed! ", ILog::eWarning);
			}

			if (pEvent)
			{
				m_ExResult = pEvent->getInfo(0, &sEventtName, 0);
				if (m_ExResult == FMOD_OK && m_pSoundSystem->g_nDumpEventStructure)
				{
					CryLogAlways("Sounds/%s:%s:%s", sProjectName, sGroupName, sEventtName);
				}
			}
		}
	}

	if (m_ExResult == FMOD_OK)
	{
		m_nCountEvent += nNumEvents;
	}
}

// writes output to screen in debug
void CAudioDeviceFmodEx400::DrawInformation(IRenderer* pRenderer, float xpos, float ypos, int nSoundInfo)
{
	float fColor[4] = {1.0f, 1.0f, 1.0f, 0.7f};
	float fWhite[4] = {1.0f, 1.0f, 1.0f, 0.7f};
	float fBlue[4] = {0.0f, 0.0f, 1.0f, 0.7f};
	float fCyan[4] = {0.0f, 1.0f, 1.0f, 0.7f};

	if (nSoundInfo == 6)
	{
		uint32 nUsedMem = 0;

		IWavebank* pWavebank = NULL;

		ypos += 10;

		for (const auto& [name, pWavebank] : m_wavebanks)
		{
			nUsedMem += pWavebank->GetInfo()->nMemCurrentlyInByte;
			if (pWavebank->GetInfo()->nMemCurrentlyInByte > 0)
			{
				pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColor, false, "%s Mem: %dKB File: %dKB",
				                       pWavebank->GetName(),
				                       pWavebank->GetInfo()->nMemCurrentlyInByte / 1024,
				                       pWavebank->GetInfo()->nFileSize / 1024);
				ypos += 10;
			}
		}

		ypos += 10;
		pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColor, false, "Total Wavebank Memory: %.1f MB",
		                       (nUsedMem / 1024) / 1024.0f);
		ypos += 10;
	}

	if (nSoundInfo == 1)
	{
		pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColor, false, "Channel sounds: %d",
		                       m_UnusedPlatformSounds.size());
		ypos += 10;
		pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColor, false, "Event sounds: %d",
		                       m_UnusedPlatformSoundEvents.size());
		ypos += 10;

		if (m_pEventSystem)
		{
			int nNum = 64;
			FMOD_EVENT_SYSTEMINFO SystemInfo;
			memset(&SystemInfo, 0, sizeof(FMOD_EVENT_SYSTEMINFO));

			SystemInfo.numplayingevents = nNum;
			FMOD_EVENT* PlayingEvents[64];
			SystemInfo.playingevents = PlayingEvents;

			m_ExResult = m_pEventSystem->getInfo(&SystemInfo);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("event system get system info failed! ", ILog::eWarning);
				return;
			}

			pRenderer->Draw2dLabel(xpos, ypos, 1, fColor, false, "----- Event Names %d -----",
			                       SystemInfo.numplayingevents);
			// pRenderer->Draw2dLabel(xpos, ypos, 1.5, fColor, false, "Playing Events: %d", );
			ypos += 10;

			for (int i = 0; i < SystemInfo.numplayingevents; ++i)
			{
				char* sName;
				FMOD::Event* pEvent = (FMOD::Event*)SystemInfo.playingevents[i];
				FMOD_EVENT_INFO EventInfo;
				memset(&EventInfo, 0, sizeof(FMOD_EVENT_INFO));

				m_ExResult = pEvent->getInfo(0, &sName, &EventInfo);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("event get info failed! ", ILog::eWarning);
				}

				pRenderer->Draw2dLabel(xpos, ypos, 1.2f, fColor, false, "%s in %s", sName,
				                       EventInfo.wavebanknames[0]);
				ypos += 10;
			}
		}
	}
}

// hack for looping weapon sound MP bug
void CAudioDeviceFmodEx400::FindLostEvent()
{
	// Go through all FMOD events and check if their handle is known to the soundsystem
	// in all active looping sounds. Oneshots will end eventually anyways.

	if (m_pEventSystem)
	{
		int nNum = 64;
		FMOD_EVENT_SYSTEMINFO SystemInfo = {0};

		SystemInfo.numplayingevents = nNum;
		FMOD_EVENT* PlayingEvents[64];
		SystemInfo.playingevents = PlayingEvents;

		m_ExResult = m_pEventSystem->getInfo(&SystemInfo);
		if (m_ExResult != FMOD_OK)
		{
			FmodErrorOutput("event system get system info failed! ", ILog::eWarning);
			return;
		}

		for (int i = 0; i < SystemInfo.numplayingevents; ++i)
		{
			FMOD::Event* pEvent = (FMOD::Event*)SystemInfo.playingevents[i];
			FMOD_EVENT_INFO EventInfo = {0};

			m_ExResult = pEvent->getInfo(0, 0, &EventInfo);
			if (m_ExResult != FMOD_OK)
			{
				FmodErrorOutput("event get info failed! ", ILog::eWarning);
			}
			else
			{
				// oneshot sounds will end eventually,
				// so only check for looping sounds which are not controlled by the sound system
				bool bLost = (EventInfo.lengthms == -1) &&
				             !(m_pSoundSystem->IsEventUsedInPlatformSound((tSoundHandle)pEvent));

				if (bLost)
				{
					// CryLog("[Warning] <Sound> Lost event %s found and stopped. \n",
					// sName );
					m_ExResult = pEvent->stop(false);
				}
			}
		}
	}
}
