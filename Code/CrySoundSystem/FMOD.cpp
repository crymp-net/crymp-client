#include "CryCommon/CrySystem/ISystem.h"
#include "Library/WinAPI.h"

#include "FMOD.h"

// reduce machine code size by preventing the compiler from inlining certain functions
#ifdef _MSC_VER
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

// FMOD DLLs
extern void* g_pFmodEx;
extern void* g_pFmodEvent;
extern void* g_pFmodEventNet;

static NOINLINE void* GetFmodExSymbol(const char* name)
{
	void* symbol = WinAPI::DLL::GetSymbol(g_pFmodEx, name);
	if (!symbol)
	{
		gEnv->pSystem->Error("FMOD: Cannot find fmodex (%p) symbol %s", g_pFmodEx, name);
	}

	return symbol;
}

static NOINLINE void* GetFmodEventSymbol(const char* name)
{
	void* symbol = WinAPI::DLL::GetSymbol(g_pFmodEvent, name);
	if (!symbol)
	{
		gEnv->pSystem->Error("FMOD: Cannot find fmod_event (%p) symbol %s", g_pFmodEvent, name);
	}

	return symbol;
}

static NOINLINE void* GetFmodEventNetSymbol(const char* name)
{
	void* symbol = WinAPI::DLL::GetSymbol(g_pFmodEventNet, name);
	if (!symbol)
	{
		gEnv->pSystem->Error("FMOD: Cannot find fmod_event_net (%p) symbol %s", g_pFmodEventNet, name);
	}

	return symbol;
}

const char* FMOD_ErrorString(FMOD_RESULT errcode)
{
	switch (errcode)
	{
		case FMOD_ERR_ALREADYLOCKED:
			return "Tried to call lock a second time before unlock was called. ";
		case FMOD_ERR_BADCOMMAND:
			return "Tried to call a function on a data type that does not allow this type of functionality "
			       "(ie calling Sound::lock on a streaming sound). ";
		case FMOD_ERR_CDDA_DRIVERS:
			return "Neither NTSCSI nor ASPI could be initialised. ";
		case FMOD_ERR_CDDA_INIT:
			return "An error occurred while initialising the CDDA subsystem. ";
		case FMOD_ERR_CDDA_INVALID_DEVICE:
			return "Couldn't find the specified device. ";
		case FMOD_ERR_CDDA_NOAUDIO:
			return "No audio tracks on the specified disc. ";
		case FMOD_ERR_CDDA_NODEVICES:
			return "No CD/DVD devices were found. ";
		case FMOD_ERR_CDDA_NODISC:
			return "No disc present in the specified drive. ";
		case FMOD_ERR_CDDA_READ:
			return "A CDDA read error occurred. ";
		case FMOD_ERR_CHANNEL_ALLOC:
			return "Error trying to allocate a channel. ";
		case FMOD_ERR_CHANNEL_STOLEN:
			return "The specified channel has been reused to play another sound. ";
		case FMOD_ERR_COM:
			return "A Win32 COM related error occured. COM failed to initialize or a QueryInterface failed "
			       "meaning a Windows codec or driver was not installed properly. ";
		case FMOD_ERR_DMA:
			return "DMA Failure.  See debug output for more information. ";
		case FMOD_ERR_DSP_CONNECTION:
			return "DSP connection error.  Connection possibly caused a cyclic dependancy. ";
		case FMOD_ERR_DSP_FORMAT:
			return "DSP Format error.  A DSP unit may have attempted to connect to this network with the "
			       "wrong format. ";
		case FMOD_ERR_DSP_NOTFOUND:
			return "DSP connection error.  Couldn't find the DSP unit specified. ";
		case FMOD_ERR_DSP_RUNNING:
			return "DSP error.  Cannot perform this operation while the network is in the middle of "
			       "running.  This will most likely happen if a connection or disconnection is attempted "
			       "in a DSP callback. ";
		case FMOD_ERR_DSP_TOOMANYCONNECTIONS:
			return "DSP connection error.  The unit being connected to or disconnected should only have 1 "
			       "input or output. ";
		case FMOD_ERR_EVENT_FAILED:
			return "An Event failed to be retrieved, most likely due to 'just fail' being specified as the "
			       "max playbacks behavior. ";
		case FMOD_ERR_EVENT_INFOONLY:
			return "Can't execute this command on an EVENT_INFOONLY event. ";
		case FMOD_ERR_EVENT_INTERNAL:
			return "An error occured that wasn't supposed to.  See debug log for reason. ";
		case FMOD_ERR_EVENT_MAXSTREAMS:
			return "Event failed because 'Max streams' was hit when FMOD_INIT_FAIL_ON_MAXSTREAMS was "
			       "specified. ";
		case FMOD_ERR_EVENT_MISMATCH:
			return "FSB mismatches the FEV it was compiled with or FEV was built for a different "
			       "platform. ";
		case FMOD_ERR_EVENT_NAMECONFLICT:
			return "A category with the same name already exists. ";
		case FMOD_ERR_EVENT_NOTFOUND:
			return "The requested event, event group, event category or event property could not be "
			       "found. ";
		case FMOD_ERR_FILE_BAD:
			return "Error loading file. ";
		case FMOD_ERR_FILE_COULDNOTSEEK:
			return "Couldn't perform seek operation.  This is a limitation of the medium (ie netstreams) "
			       "or the file format. ";
		case FMOD_ERR_FILE_DISKEJECTED:
			return "Media was ejected while reading. ";
		case FMOD_ERR_FILE_EOF:
			return "End of file unexpectedly reached while trying to read essential data (truncated "
			       "data?). ";
		case FMOD_ERR_FILE_NOTFOUND:
			return "File not found. ";
		case FMOD_ERR_FILE_UNWANTED:
			return "Unwanted file access occured. ";
		case FMOD_ERR_FORMAT:
			return "Unsupported file or audio format. ";
		case FMOD_ERR_HTTP:
			return "A HTTP error occurred. This is a catch-all for HTTP errors not listed elsewhere. ";
		case FMOD_ERR_HTTP_ACCESS:
			return "The specified resource requires authentication or is forbidden. ";
		case FMOD_ERR_HTTP_PROXY_AUTH:
			return "Proxy authentication is required to access the specified resource. ";
		case FMOD_ERR_HTTP_SERVER_ERROR:
			return "A HTTP server error occurred. ";
		case FMOD_ERR_HTTP_TIMEOUT:
			return "The HTTP request timed out. ";
		case FMOD_ERR_INITIALIZATION:
			return "FMOD was not initialized correctly to support this function. ";
		case FMOD_ERR_INITIALIZED:
			return "Cannot call this command after System::init. ";
		case FMOD_ERR_INTERNAL:
			return "An error occured that wasn't supposed to.  Contact support. ";
		case FMOD_ERR_INVALID_ADDRESS:
			return "On Xbox 360, this memory address passed to FMOD must be physical, (ie allocated with "
			       "XPhysicalAlloc.) ";
		case FMOD_ERR_INVALID_FLOAT:
			return "Value passed in was a NaN, Inf or denormalized float. ";
		case FMOD_ERR_INVALID_HANDLE:
			return "An invalid object handle was used. ";
		case FMOD_ERR_INVALID_PARAM:
			return "An invalid parameter was passed to this function. ";
		case FMOD_ERR_INVALID_SPEAKER:
			return "An invalid speaker was passed to this function based on the current speaker mode. ";
		case FMOD_ERR_INVALID_VECTOR:
			return "The vectors passed in are not unit length, or perpendicular. ";
		case FMOD_ERR_IRX:
			return "PS2 only.  fmodex.irx failed to initialize.  This is most likely because you forgot to "
			       "load it. ";
		case FMOD_ERR_MAXAUDIBLE:
			return "Reached maximum audible playback count for this sound's soundgroup. ";
		case FMOD_ERR_MEMORY:
			return "Not enough memory or resources. ";
		case FMOD_ERR_MEMORY_CANTPOINT:
			return "Can't use FMOD_OPENMEMORY_POINT on non PCM source data, or non mp3/xma/adpcm data if "
			       "FMOD_CREATECOMPRESSEDSAMPLE was used. ";
		case FMOD_ERR_MEMORY_IOP:
			return "PS2 only.  Not enough memory or resources on PlayStation 2 IOP ram. ";
		case FMOD_ERR_MEMORY_SRAM:
			return "Not enough memory or resources on console sound ram. ";
		case FMOD_ERR_NEEDS2D:
			return "Tried to call a command on a 3d sound when the command was meant for 2d sound. ";
		case FMOD_ERR_NEEDS3D:
			return "Tried to call a command on a 2d sound when the command was meant for 3d sound. ";
		case FMOD_ERR_NEEDSHARDWARE:
			return "Tried to use a feature that requires hardware support.  (ie trying to play a VAG "
			       "compressed sound in software on PS2). ";
		case FMOD_ERR_NEEDSSOFTWARE:
			return "Tried to use a feature that requires the software engine.  Software engine has either "
			       "been turned off, or command was executed on a hardware channel which does not support "
			       "this feature. ";
		case FMOD_ERR_NET_CONNECT:
			return "Couldn't connect to the specified host. ";
		case FMOD_ERR_NET_SOCKET_ERROR:
			return "A socket error occurred.  This is a catch-all for socket-related errors not listed "
			       "elsewhere. ";
		case FMOD_ERR_NET_URL:
			return "The specified URL couldn't be resolved. ";
		case FMOD_ERR_NET_WOULD_BLOCK:
			return "Operation on a non-blocking socket could not complete immediately. ";
		case FMOD_ERR_NOTREADY:
			return "Operation could not be performed because specified sound is not ready. ";
		case FMOD_ERR_OUTPUT_ALLOCATED:
			return "Error initializing output device, but more specifically, the output device is already "
			       "in use and cannot be reused. ";
		case FMOD_ERR_OUTPUT_CREATEBUFFER:
			return "Error creating hardware sound buffer. ";
		case FMOD_ERR_OUTPUT_DRIVERCALL:
			return "A call to a standard soundcard driver failed, which could possibly mean a bug in the "
			       "driver or resources were missing or exhausted. ";
		case FMOD_ERR_OUTPUT_ENUMERATION:
			return "Error enumerating the available driver list. List may be inconsistent due to a recent "
			       "device addition or removal. ";
		case FMOD_ERR_OUTPUT_FORMAT:
			return "Soundcard does not support the minimum features needed for this soundsystem (16bit "
			       "stereo output). ";
		case FMOD_ERR_OUTPUT_INIT:
			return "Error initializing output device. ";
		case FMOD_ERR_OUTPUT_NOHARDWARE:
			return "FMOD_HARDWARE was specified but the sound card does not have the resources nescessary "
			       "to play it. ";
		case FMOD_ERR_OUTPUT_NOSOFTWARE:
			return "Attempted to create a software sound but no software channels were specified in "
			       "System::init. ";
		case FMOD_ERR_PAN:
			return "Panning only works with mono or stereo sound sources. ";
		case FMOD_ERR_PLUGIN:
			return "An unspecified error has been returned from a 3rd party plugin. ";
		case FMOD_ERR_PLUGIN_INSTANCES:
			return "The number of allowed instances of a plugin has been exceeded. ";
		case FMOD_ERR_PLUGIN_MISSING:
			return "A requested output, dsp unit type or codec was not available. ";
		case FMOD_ERR_PLUGIN_RESOURCE:
			return "A resource that the plugin requires cannot be found. (ie the DLS file for MIDI "
			       "playback) ";
		case FMOD_ERR_RECORD:
			return "An error occured trying to initialize the recording device. ";
		case FMOD_ERR_REVERB_INSTANCE:
			return "Specified Instance in FMOD_REVERB_PROPERTIES couldn't be set. Most likely because it "
			       "is an invalid instance number, or another application has locked the EAX4 FX slot. ";
		case FMOD_ERR_SUBSOUNDS:
			return "The error occured because the sound referenced contains subsounds.  The operation "
			       "cannot be performed on a parent sound, or a parent sound was played without setting up "
			       "a sentence first. ";
		case FMOD_ERR_SUBSOUND_ALLOCATED:
			return "This subsound is already being used by another sound, you cannot have more than one "
			       "parent to a sound.  Null out the other parent's entry first. ";
		case FMOD_ERR_SUBSOUND_CANTMOVE:
			return "Shared subsounds cannot be replaced or moved from their parent stream, such as when "
			       "the parent stream is an FSB file. ";
		case FMOD_ERR_SUBSOUND_MODE:
			return "The subsound's mode bits do not match with the parent sound's mode bits.  See "
			       "documentation for function that it was called with. ";
		case FMOD_ERR_TAGNOTFOUND:
			return "The specified tag could not be found or there are no tags. ";
		case FMOD_ERR_TOOMANYCHANNELS:
			return "The sound created exceeds the allowable input channel count.  This can be increased "
			       "using the maxinputchannels parameter in System::setSoftwareFormat. ";
		case FMOD_ERR_UNIMPLEMENTED:
			return "Something in FMOD hasn't been implemented when it should be! contact support! ";
		case FMOD_ERR_UNINITIALIZED:
			return "This command failed because System::init or System::setDriver was not called. ";
		case FMOD_ERR_UNSUPPORTED:
			return "A command issued was not supported by this object.  Possibly a plugin without certain "
			       "callbacks specified. ";
		case FMOD_ERR_UPDATE:
			return "An error caused by System::update occured. ";
		case FMOD_ERR_VERSION:
			return "The version number of this file format is not supported. ";
		case FMOD_OK:
			return "No errors.";
		default:
			return "Unknown error.";
	};
}

static void* g_sym_FMOD_Memory_Initialize;
static void* g_sym_FMOD_Memory_GetStats;
static void* g_sym_FMOD_Debug_SetLevel;
static void* g_sym_FMOD_Debug_GetLevel;
static void* g_sym_FMOD_File_SetDiskBusy;
static void* g_sym_FMOD_File_GetDiskBusy;
static void* g_sym_FMOD_System_Create;
static void* g_sym_FMOD_EventSystem_Create;
static void* g_sym_FMOD_NetEventSystem_Init;
static void* g_sym_FMOD_NetEventSystem_Update;
static void* g_sym_FMOD_NetEventSystem_Shutdown;
static void* g_sym_FMOD_NetEventSystem_GetVersion;
static void* g_sym_FMOD_System_release;
static void* g_sym_FMOD_System_setOutput;
static void* g_sym_FMOD_System_getOutput;
static void* g_sym_FMOD_System_getNumDrivers;
static void* g_sym_FMOD_System_getDriverInfo;
static void* g_sym_FMOD_System_getDriverCaps;
static void* g_sym_FMOD_System_setDriver;
static void* g_sym_FMOD_System_getDriver;
static void* g_sym_FMOD_System_setHardwareChannels;
static void* g_sym_FMOD_System_setSoftwareChannels;
static void* g_sym_FMOD_System_getSoftwareChannels;
static void* g_sym_FMOD_System_setSoftwareFormat;
static void* g_sym_FMOD_System_getSoftwareFormat;
static void* g_sym_FMOD_System_setDSPBufferSize;
static void* g_sym_FMOD_System_getDSPBufferSize;
static void* g_sym_FMOD_System_setFileSystem;
static void* g_sym_FMOD_System_attachFileSystem;
static void* g_sym_FMOD_System_setAdvancedSettings;
static void* g_sym_FMOD_System_getAdvancedSettings;
static void* g_sym_FMOD_System_setSpeakerMode;
static void* g_sym_FMOD_System_getSpeakerMode;
static void* g_sym_FMOD_System_setCallback;
static void* g_sym_FMOD_System_setPluginPath;
static void* g_sym_FMOD_System_loadPlugin;
static void* g_sym_FMOD_System_getNumPlugins;
static void* g_sym_FMOD_System_getPluginInfo;
static void* g_sym_FMOD_System_unloadPlugin;
static void* g_sym_FMOD_System_setOutputByPlugin;
static void* g_sym_FMOD_System_getOutputByPlugin;
static void* g_sym_FMOD_System_createCodec;
static void* g_sym_FMOD_System_init;
static void* g_sym_FMOD_System_close;
static void* g_sym_FMOD_System_update;
static void* g_sym_FMOD_System_set3DSettings;
static void* g_sym_FMOD_System_get3DSettings;
static void* g_sym_FMOD_System_set3DNumListeners;
static void* g_sym_FMOD_System_get3DNumListeners;
static void* g_sym_FMOD_System_set3DListenerAttributes;
static void* g_sym_FMOD_System_get3DListenerAttributes;
static void* g_sym_FMOD_System_set3DRolloffCallback;
static void* g_sym_FMOD_System_set3DSpeakerPosition;
static void* g_sym_FMOD_System_get3DSpeakerPosition;
static void* g_sym_FMOD_System_setStreamBufferSize;
static void* g_sym_FMOD_System_getStreamBufferSize;
static void* g_sym_FMOD_System_getVersion;
static void* g_sym_FMOD_System_getOutputHandle;
static void* g_sym_FMOD_System_getChannelsPlaying;
static void* g_sym_FMOD_System_getHardwareChannels;
static void* g_sym_FMOD_System_getCPUUsage;
static void* g_sym_FMOD_System_getSoundRAM;
static void* g_sym_FMOD_System_getNumCDROMDrives;
static void* g_sym_FMOD_System_getCDROMDriveName;
static void* g_sym_FMOD_System_getSpectrum;
static void* g_sym_FMOD_System_getWaveData;
static void* g_sym_FMOD_System_createSound;
static void* g_sym_FMOD_System_createStream;
static void* g_sym_FMOD_System_createDSP;
static void* g_sym_FMOD_System_createDSPByType;
static void* g_sym_FMOD_System_createDSPByIndex;
static void* g_sym_FMOD_System_createChannelGroup;
static void* g_sym_FMOD_System_createSoundGroup;
static void* g_sym_FMOD_System_createReverb;
static void* g_sym_FMOD_System_playSound;
static void* g_sym_FMOD_System_playDSP;
static void* g_sym_FMOD_System_getChannel;
static void* g_sym_FMOD_System_getMasterChannelGroup;
static void* g_sym_FMOD_System_getMasterSoundGroup;
static void* g_sym_FMOD_System_setReverbProperties;
static void* g_sym_FMOD_System_getReverbProperties;
static void* g_sym_FMOD_System_setReverbAmbientProperties;
static void* g_sym_FMOD_System_getReverbAmbientProperties;
static void* g_sym_FMOD_System_getDSPHead;
static void* g_sym_FMOD_System_addDSP;
static void* g_sym_FMOD_System_lockDSP;
static void* g_sym_FMOD_System_unlockDSP;
static void* g_sym_FMOD_System_getDSPClock;
static void* g_sym_FMOD_System_setRecordDriver;
static void* g_sym_FMOD_System_getRecordDriver;
static void* g_sym_FMOD_System_getRecordNumDrivers;
static void* g_sym_FMOD_System_getRecordDriverInfo;
static void* g_sym_FMOD_System_getRecordDriverCaps;
static void* g_sym_FMOD_System_getRecordPosition;
static void* g_sym_FMOD_System_recordStart;
static void* g_sym_FMOD_System_recordStop;
static void* g_sym_FMOD_System_isRecording;
static void* g_sym_FMOD_System_createGeometry;
static void* g_sym_FMOD_System_setGeometrySettings;
static void* g_sym_FMOD_System_getGeometrySettings;
static void* g_sym_FMOD_System_loadGeometry;
static void* g_sym_FMOD_System_setNetworkProxy;
static void* g_sym_FMOD_System_getNetworkProxy;
static void* g_sym_FMOD_System_setNetworkTimeout;
static void* g_sym_FMOD_System_getNetworkTimeout;
static void* g_sym_FMOD_System_setUserData;
static void* g_sym_FMOD_System_getUserData;
static void* g_sym_FMOD_Sound_release;
static void* g_sym_FMOD_Sound_getSystemObject;
static void* g_sym_FMOD_Sound_lock;
static void* g_sym_FMOD_Sound_unlock;
static void* g_sym_FMOD_Sound_setDefaults;
static void* g_sym_FMOD_Sound_getDefaults;
static void* g_sym_FMOD_Sound_setVariations;
static void* g_sym_FMOD_Sound_getVariations;
static void* g_sym_FMOD_Sound_set3DMinMaxDistance;
static void* g_sym_FMOD_Sound_get3DMinMaxDistance;
static void* g_sym_FMOD_Sound_set3DConeSettings;
static void* g_sym_FMOD_Sound_get3DConeSettings;
static void* g_sym_FMOD_Sound_set3DCustomRolloff;
static void* g_sym_FMOD_Sound_get3DCustomRolloff;
static void* g_sym_FMOD_Sound_setSubSound;
static void* g_sym_FMOD_Sound_getSubSound;
static void* g_sym_FMOD_Sound_setSubSoundSentence;
static void* g_sym_FMOD_Sound_getName;
static void* g_sym_FMOD_Sound_getLength;
static void* g_sym_FMOD_Sound_getFormat;
static void* g_sym_FMOD_Sound_getNumSubSounds;
static void* g_sym_FMOD_Sound_getNumTags;
static void* g_sym_FMOD_Sound_getTag;
static void* g_sym_FMOD_Sound_getOpenState;
static void* g_sym_FMOD_Sound_readData;
static void* g_sym_FMOD_Sound_seekData;
static void* g_sym_FMOD_Sound_setSoundGroup;
static void* g_sym_FMOD_Sound_getSoundGroup;
static void* g_sym_FMOD_Sound_getNumSyncPoints;
static void* g_sym_FMOD_Sound_getSyncPoint;
static void* g_sym_FMOD_Sound_getSyncPointInfo;
static void* g_sym_FMOD_Sound_addSyncPoint;
static void* g_sym_FMOD_Sound_deleteSyncPoint;
static void* g_sym_FMOD_Sound_setMode;
static void* g_sym_FMOD_Sound_getMode;
static void* g_sym_FMOD_Sound_setLoopCount;
static void* g_sym_FMOD_Sound_getLoopCount;
static void* g_sym_FMOD_Sound_setLoopPoints;
static void* g_sym_FMOD_Sound_getLoopPoints;
static void* g_sym_FMOD_Sound_getMusicNumChannels;
static void* g_sym_FMOD_Sound_setMusicChannelVolume;
static void* g_sym_FMOD_Sound_getMusicChannelVolume;
static void* g_sym_FMOD_Sound_setUserData;
static void* g_sym_FMOD_Sound_getUserData;
static void* g_sym_FMOD_Channel_getSystemObject;
static void* g_sym_FMOD_Channel_stop;
static void* g_sym_FMOD_Channel_setPaused;
static void* g_sym_FMOD_Channel_getPaused;
static void* g_sym_FMOD_Channel_setVolume;
static void* g_sym_FMOD_Channel_getVolume;
static void* g_sym_FMOD_Channel_setFrequency;
static void* g_sym_FMOD_Channel_getFrequency;
static void* g_sym_FMOD_Channel_setPan;
static void* g_sym_FMOD_Channel_getPan;
static void* g_sym_FMOD_Channel_setDelay;
static void* g_sym_FMOD_Channel_getDelay;
static void* g_sym_FMOD_Channel_setSpeakerMix;
static void* g_sym_FMOD_Channel_getSpeakerMix;
static void* g_sym_FMOD_Channel_setSpeakerLevels;
static void* g_sym_FMOD_Channel_getSpeakerLevels;
static void* g_sym_FMOD_Channel_setInputChannelMix;
static void* g_sym_FMOD_Channel_getInputChannelMix;
static void* g_sym_FMOD_Channel_setMute;
static void* g_sym_FMOD_Channel_getMute;
static void* g_sym_FMOD_Channel_setPriority;
static void* g_sym_FMOD_Channel_getPriority;
static void* g_sym_FMOD_Channel_setPosition;
static void* g_sym_FMOD_Channel_getPosition;
static void* g_sym_FMOD_Channel_setReverbProperties;
static void* g_sym_FMOD_Channel_getReverbProperties;
static void* g_sym_FMOD_Channel_setChannelGroup;
static void* g_sym_FMOD_Channel_getChannelGroup;
static void* g_sym_FMOD_Channel_setCallback;
static void* g_sym_FMOD_Channel_set3DAttributes;
static void* g_sym_FMOD_Channel_get3DAttributes;
static void* g_sym_FMOD_Channel_set3DMinMaxDistance;
static void* g_sym_FMOD_Channel_get3DMinMaxDistance;
static void* g_sym_FMOD_Channel_set3DConeSettings;
static void* g_sym_FMOD_Channel_get3DConeSettings;
static void* g_sym_FMOD_Channel_set3DConeOrientation;
static void* g_sym_FMOD_Channel_get3DConeOrientation;
static void* g_sym_FMOD_Channel_set3DCustomRolloff;
static void* g_sym_FMOD_Channel_get3DCustomRolloff;
static void* g_sym_FMOD_Channel_set3DOcclusion;
static void* g_sym_FMOD_Channel_get3DOcclusion;
static void* g_sym_FMOD_Channel_set3DSpread;
static void* g_sym_FMOD_Channel_get3DSpread;
static void* g_sym_FMOD_Channel_set3DPanLevel;
static void* g_sym_FMOD_Channel_get3DPanLevel;
static void* g_sym_FMOD_Channel_set3DDopplerLevel;
static void* g_sym_FMOD_Channel_get3DDopplerLevel;
static void* g_sym_FMOD_Channel_getDSPHead;
static void* g_sym_FMOD_Channel_addDSP;
static void* g_sym_FMOD_Channel_isPlaying;
static void* g_sym_FMOD_Channel_isVirtual;
static void* g_sym_FMOD_Channel_getAudibility;
static void* g_sym_FMOD_Channel_getCurrentSound;
static void* g_sym_FMOD_Channel_getSpectrum;
static void* g_sym_FMOD_Channel_getWaveData;
static void* g_sym_FMOD_Channel_getIndex;
static void* g_sym_FMOD_Channel_setMode;
static void* g_sym_FMOD_Channel_getMode;
static void* g_sym_FMOD_Channel_setLoopCount;
static void* g_sym_FMOD_Channel_getLoopCount;
static void* g_sym_FMOD_Channel_setLoopPoints;
static void* g_sym_FMOD_Channel_getLoopPoints;
static void* g_sym_FMOD_Channel_setUserData;
static void* g_sym_FMOD_Channel_getUserData;
static void* g_sym_FMOD_ChannelGroup_release;
static void* g_sym_FMOD_ChannelGroup_getSystemObject;
static void* g_sym_FMOD_ChannelGroup_setVolume;
static void* g_sym_FMOD_ChannelGroup_getVolume;
static void* g_sym_FMOD_ChannelGroup_setPitch;
static void* g_sym_FMOD_ChannelGroup_getPitch;
static void* g_sym_FMOD_ChannelGroup_set3DOcclusion;
static void* g_sym_FMOD_ChannelGroup_get3DOcclusion;
static void* g_sym_FMOD_ChannelGroup_setPaused;
static void* g_sym_FMOD_ChannelGroup_getPaused;
static void* g_sym_FMOD_ChannelGroup_setMute;
static void* g_sym_FMOD_ChannelGroup_getMute;
static void* g_sym_FMOD_ChannelGroup_stop;
static void* g_sym_FMOD_ChannelGroup_overrideVolume;
static void* g_sym_FMOD_ChannelGroup_overrideFrequency;
static void* g_sym_FMOD_ChannelGroup_overridePan;
static void* g_sym_FMOD_ChannelGroup_overrideReverbProperties;
static void* g_sym_FMOD_ChannelGroup_override3DAttributes;
static void* g_sym_FMOD_ChannelGroup_overrideSpeakerMix;
static void* g_sym_FMOD_ChannelGroup_addGroup;
static void* g_sym_FMOD_ChannelGroup_getNumGroups;
static void* g_sym_FMOD_ChannelGroup_getGroup;
static void* g_sym_FMOD_ChannelGroup_getParentGroup;
static void* g_sym_FMOD_ChannelGroup_getDSPHead;
static void* g_sym_FMOD_ChannelGroup_addDSP;
static void* g_sym_FMOD_ChannelGroup_getName;
static void* g_sym_FMOD_ChannelGroup_getNumChannels;
static void* g_sym_FMOD_ChannelGroup_getChannel;
static void* g_sym_FMOD_ChannelGroup_getSpectrum;
static void* g_sym_FMOD_ChannelGroup_getWaveData;
static void* g_sym_FMOD_ChannelGroup_setUserData;
static void* g_sym_FMOD_ChannelGroup_getUserData;
static void* g_sym_FMOD_SoundGroup_release;
static void* g_sym_FMOD_SoundGroup_getSystemObject;
static void* g_sym_FMOD_SoundGroup_setMaxAudible;
static void* g_sym_FMOD_SoundGroup_getMaxAudible;
static void* g_sym_FMOD_SoundGroup_setMaxAudibleBehavior;
static void* g_sym_FMOD_SoundGroup_getMaxAudibleBehavior;
static void* g_sym_FMOD_SoundGroup_setMuteFadeSpeed;
static void* g_sym_FMOD_SoundGroup_getMuteFadeSpeed;
static void* g_sym_FMOD_SoundGroup_setVolume;
static void* g_sym_FMOD_SoundGroup_getVolume;
static void* g_sym_FMOD_SoundGroup_stop;
static void* g_sym_FMOD_SoundGroup_getName;
static void* g_sym_FMOD_SoundGroup_getNumSounds;
static void* g_sym_FMOD_SoundGroup_getSound;
static void* g_sym_FMOD_SoundGroup_getNumPlaying;
static void* g_sym_FMOD_SoundGroup_setUserData;
static void* g_sym_FMOD_SoundGroup_getUserData;
static void* g_sym_FMOD_DSP_release;
static void* g_sym_FMOD_DSP_getSystemObject;
static void* g_sym_FMOD_DSP_addInput;
static void* g_sym_FMOD_DSP_disconnectFrom;
static void* g_sym_FMOD_DSP_disconnectAll;
static void* g_sym_FMOD_DSP_remove;
static void* g_sym_FMOD_DSP_getNumInputs;
static void* g_sym_FMOD_DSP_getNumOutputs;
static void* g_sym_FMOD_DSP_getInput;
static void* g_sym_FMOD_DSP_getOutput;
static void* g_sym_FMOD_DSP_setActive;
static void* g_sym_FMOD_DSP_getActive;
static void* g_sym_FMOD_DSP_setBypass;
static void* g_sym_FMOD_DSP_getBypass;
static void* g_sym_FMOD_DSP_reset;
static void* g_sym_FMOD_DSP_setParameter;
static void* g_sym_FMOD_DSP_getParameter;
static void* g_sym_FMOD_DSP_getNumParameters;
static void* g_sym_FMOD_DSP_getParameterInfo;
static void* g_sym_FMOD_DSP_showConfigDialog;
static void* g_sym_FMOD_DSP_getInfo;
static void* g_sym_FMOD_DSP_getType;
static void* g_sym_FMOD_DSP_setDefaults;
static void* g_sym_FMOD_DSP_getDefaults;
static void* g_sym_FMOD_DSP_setUserData;
static void* g_sym_FMOD_DSP_getUserData;
static void* g_sym_FMOD_DSPConnection_getInput;
static void* g_sym_FMOD_DSPConnection_getOutput;
static void* g_sym_FMOD_DSPConnection_setMix;
static void* g_sym_FMOD_DSPConnection_getMix;
static void* g_sym_FMOD_DSPConnection_setLevels;
static void* g_sym_FMOD_DSPConnection_getLevels;
static void* g_sym_FMOD_DSPConnection_setUserData;
static void* g_sym_FMOD_DSPConnection_getUserData;
static void* g_sym_FMOD_Geometry_release;
static void* g_sym_FMOD_Geometry_addPolygon;
static void* g_sym_FMOD_Geometry_getNumPolygons;
static void* g_sym_FMOD_Geometry_getMaxPolygons;
static void* g_sym_FMOD_Geometry_getPolygonNumVertices;
static void* g_sym_FMOD_Geometry_setPolygonVertex;
static void* g_sym_FMOD_Geometry_getPolygonVertex;
static void* g_sym_FMOD_Geometry_setPolygonAttributes;
static void* g_sym_FMOD_Geometry_getPolygonAttributes;
static void* g_sym_FMOD_Geometry_setActive;
static void* g_sym_FMOD_Geometry_getActive;
static void* g_sym_FMOD_Geometry_setRotation;
static void* g_sym_FMOD_Geometry_getRotation;
static void* g_sym_FMOD_Geometry_setPosition;
static void* g_sym_FMOD_Geometry_getPosition;
static void* g_sym_FMOD_Geometry_setScale;
static void* g_sym_FMOD_Geometry_getScale;
static void* g_sym_FMOD_Geometry_save;
static void* g_sym_FMOD_Geometry_setUserData;
static void* g_sym_FMOD_Geometry_getUserData;
static void* g_sym_FMOD_Reverb_release;
static void* g_sym_FMOD_Reverb_set3DAttributes;
static void* g_sym_FMOD_Reverb_get3DAttributes;
static void* g_sym_FMOD_Reverb_setProperties;
static void* g_sym_FMOD_Reverb_getProperties;
static void* g_sym_FMOD_Reverb_setActive;
static void* g_sym_FMOD_Reverb_getActive;
static void* g_sym_FMOD_Reverb_setUserData;
static void* g_sym_FMOD_Reverb_getUserData;
static void* g_sym_FMOD_Event_start;
static void* g_sym_FMOD_Event_stop;
static void* g_sym_FMOD_Event_getInfo;
static void* g_sym_FMOD_Event_getState;
static void* g_sym_FMOD_Event_getParentGroup;
static void* g_sym_FMOD_Event_getChannelGroup;
static void* g_sym_FMOD_Event_setCallback;
static void* g_sym_FMOD_Event_getParameter;
static void* g_sym_FMOD_Event_getParameterByIndex;
static void* g_sym_FMOD_Event_getNumParameters;
static void* g_sym_FMOD_Event_getProperty;
static void* g_sym_FMOD_Event_getPropertyByIndex;
static void* g_sym_FMOD_Event_setProperty;
static void* g_sym_FMOD_Event_setPropertyByIndex;
static void* g_sym_FMOD_Event_getNumProperties;
static void* g_sym_FMOD_Event_getCategory;
static void* g_sym_FMOD_Event_setVolume;
static void* g_sym_FMOD_Event_getVolume;
static void* g_sym_FMOD_Event_setPitch;
static void* g_sym_FMOD_Event_getPitch;
static void* g_sym_FMOD_Event_setPaused;
static void* g_sym_FMOD_Event_getPaused;
static void* g_sym_FMOD_Event_setMute;
static void* g_sym_FMOD_Event_getMute;
static void* g_sym_FMOD_Event_set3DAttributes;
static void* g_sym_FMOD_Event_get3DAttributes;
static void* g_sym_FMOD_Event_set3DOcclusion;
static void* g_sym_FMOD_Event_get3DOcclusion;
static void* g_sym_FMOD_Event_setReverbProperties;
static void* g_sym_FMOD_Event_getReverbProperties;
static void* g_sym_FMOD_Event_setUserData;
static void* g_sym_FMOD_Event_getUserData;
static void* g_sym_FMOD_EventParameter_getInfo;
static void* g_sym_FMOD_EventParameter_getRange;
static void* g_sym_FMOD_EventParameter_setValue;
static void* g_sym_FMOD_EventParameter_getValue;
static void* g_sym_FMOD_EventParameter_setVelocity;
static void* g_sym_FMOD_EventParameter_getVelocity;
static void* g_sym_FMOD_EventParameter_setSeekSpeed;
static void* g_sym_FMOD_EventParameter_getSeekSpeed;
static void* g_sym_FMOD_EventParameter_setUserData;
static void* g_sym_FMOD_EventParameter_getUserData;
static void* g_sym_FMOD_EventParameter_keyOff;

FMOD_RESULT FMOD::Memory_Initialize(void* poolmem, int poollen, FMOD_MEMORY_ALLOCCALLBACK useralloc,
                                    FMOD_MEMORY_REALLOCCALLBACK userrealloc, FMOD_MEMORY_FREECALLBACK userfree)
{
	if (!g_sym_FMOD_Memory_Initialize)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Memory_Initialize = GetFmodExSymbol("FMOD_Memory_Initialize");
#else
		g_sym_FMOD_Memory_Initialize = GetFmodExSymbol("_FMOD_Memory_Initialize@20");
#endif
	}

	return reinterpret_cast<FMOD_RESULT(F_API*)(void*, int, FMOD_MEMORY_ALLOCCALLBACK, FMOD_MEMORY_REALLOCCALLBACK,
	                                            FMOD_MEMORY_FREECALLBACK)>(g_sym_FMOD_Memory_Initialize)(
	    poolmem, poollen, useralloc, userrealloc, userfree);
}

FMOD_RESULT FMOD::Memory_GetStats(int* currentalloced, int* maxalloced)
{
	if (!g_sym_FMOD_Memory_GetStats)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Memory_GetStats = GetFmodExSymbol("FMOD_Memory_GetStats");
#else
		g_sym_FMOD_Memory_GetStats = GetFmodExSymbol("_FMOD_Memory_GetStats@8");
#endif
	}

	return reinterpret_cast<FMOD_RESULT(F_API*)(int*, int*)>(g_sym_FMOD_Memory_GetStats)(currentalloced,
	                                                                                     maxalloced);
}

FMOD_RESULT FMOD::Debug_SetLevel(FMOD_DEBUGLEVEL level)
{
	if (!g_sym_FMOD_Debug_SetLevel)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Debug_SetLevel = GetFmodExSymbol("FMOD_Debug_SetLevel");
#else
		g_sym_FMOD_Debug_SetLevel = GetFmodExSymbol("_FMOD_Debug_SetLevel@4");
#endif
	}

	return reinterpret_cast<FMOD_RESULT(F_API*)(FMOD_DEBUGLEVEL)>(g_sym_FMOD_Debug_SetLevel)(level);
}

FMOD_RESULT FMOD::Debug_GetLevel(FMOD_DEBUGLEVEL* level)
{
	if (!g_sym_FMOD_Debug_GetLevel)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Debug_GetLevel = GetFmodExSymbol("FMOD_Debug_GetLevel");
#else
		g_sym_FMOD_Debug_GetLevel = GetFmodExSymbol("_FMOD_Debug_GetLevel@4");
#endif
	}

	return reinterpret_cast<FMOD_RESULT(F_API*)(FMOD_DEBUGLEVEL*)>(g_sym_FMOD_Debug_GetLevel)(level);
}

FMOD_RESULT FMOD::File_SetDiskBusy(int busy)
{
	if (!g_sym_FMOD_File_SetDiskBusy)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_File_SetDiskBusy = GetFmodExSymbol("FMOD_File_SetDiskBusy");
#else
		g_sym_FMOD_File_SetDiskBusy = GetFmodExSymbol("_FMOD_File_SetDiskBusy@4");
#endif
	}

	return reinterpret_cast<FMOD_RESULT(F_API*)(int)>(g_sym_FMOD_File_SetDiskBusy)(busy);
}

FMOD_RESULT FMOD::File_GetDiskBusy(int* busy)
{
	if (!g_sym_FMOD_File_GetDiskBusy)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_File_GetDiskBusy = GetFmodExSymbol("FMOD_File_GetDiskBusy");
#else
		g_sym_FMOD_File_GetDiskBusy = GetFmodExSymbol("_FMOD_File_GetDiskBusy@4");
#endif
	}

	return reinterpret_cast<FMOD_RESULT(F_API*)(int*)>(g_sym_FMOD_File_GetDiskBusy)(busy);
}

FMOD_RESULT FMOD::System_Create(System** system)
{
	if (!g_sym_FMOD_System_Create)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_Create = GetFmodExSymbol("FMOD_System_Create");
#else
		g_sym_FMOD_System_Create = GetFmodExSymbol("_FMOD_System_Create@4");
#endif
	}

	return reinterpret_cast<FMOD_RESULT(F_API*)(System**)>(g_sym_FMOD_System_Create)(system);
}

FMOD_RESULT FMOD::EventSystem_Create(EventSystem** eventsystem)
{
	if (!g_sym_FMOD_EventSystem_Create)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_EventSystem_Create = GetFmodEventSymbol("FMOD_EventSystem_Create");
#else
		g_sym_FMOD_EventSystem_Create = GetFmodEventSymbol("_FMOD_EventSystem_Create@4");
#endif
	}

	return reinterpret_cast<FMOD_RESULT(F_API*)(EventSystem**)>(g_sym_FMOD_EventSystem_Create)(eventsystem);
}

FMOD_RESULT F_API FMOD::NetEventSystem_Init(EventSystem* eventsystem, unsigned short port)
{
	if (!g_sym_FMOD_NetEventSystem_Init)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_NetEventSystem_Init =
		    GetFmodEventNetSymbol("?NetEventSystem_Init@FMOD@@YA?AW4FMOD_RESULT@@PEAVEventSystem@1@G@Z");
#else
		g_sym_FMOD_NetEventSystem_Init =
		    GetFmodEventNetSymbol("?NetEventSystem_Init@FMOD@@YG?AW4FMOD_RESULT@@PAVEventSystem@1@G@Z");
#endif
	}

	return reinterpret_cast<FMOD_RESULT(F_API*)(EventSystem*, unsigned short)>(g_sym_FMOD_NetEventSystem_Init)(
	    eventsystem, port);
}

FMOD_RESULT F_API FMOD::NetEventSystem_Update()
{
	if (!g_sym_FMOD_NetEventSystem_Update)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_NetEventSystem_Update =
		    GetFmodEventNetSymbol("?NetEventSystem_Update@FMOD@@YA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_NetEventSystem_Update =
		    GetFmodEventNetSymbol("?NetEventSystem_Update@FMOD@@YG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return reinterpret_cast<FMOD_RESULT(F_API*)()>(g_sym_FMOD_NetEventSystem_Update)();
}

FMOD_RESULT F_API FMOD::NetEventSystem_Shutdown()
{
	if (!g_sym_FMOD_NetEventSystem_Shutdown)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_NetEventSystem_Shutdown =
		    GetFmodEventNetSymbol("?NetEventSystem_Shutdown@FMOD@@YA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_NetEventSystem_Shutdown =
		    GetFmodEventNetSymbol("?NetEventSystem_Shutdown@FMOD@@YG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return reinterpret_cast<FMOD_RESULT(F_API*)()>(g_sym_FMOD_NetEventSystem_Shutdown)();
}

FMOD_RESULT F_API FMOD::NetEventSystem_GetVersion(unsigned int* version)
{
	if (!g_sym_FMOD_NetEventSystem_GetVersion)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_NetEventSystem_GetVersion =
		    GetFmodEventNetSymbol("?NetEventSystem_GetVersion@FMOD@@YA?AW4FMOD_RESULT@@PEAI@Z");
#else
		g_sym_FMOD_NetEventSystem_GetVersion =
		    GetFmodEventNetSymbol("?NetEventSystem_GetVersion@FMOD@@YG?AW4FMOD_RESULT@@PAI@Z");
#endif
	}

	return reinterpret_cast<FMOD_RESULT(F_API*)(unsigned int*)>(g_sym_FMOD_NetEventSystem_GetVersion)(version);
}

FMOD_RESULT F_API FMOD::System::release()
{
	if (!g_sym_FMOD_System_release)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_release = GetFmodExSymbol("?release@System@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_System_release = GetFmodExSymbol("?release@System@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)()>(g_sym_FMOD_System_release))();
}

FMOD_RESULT F_API FMOD::System::setOutput(FMOD_OUTPUTTYPE output)
{
	if (!g_sym_FMOD_System_setOutput)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setOutput =
		    GetFmodExSymbol("?setOutput@System@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_OUTPUTTYPE@@@Z");
#else
		g_sym_FMOD_System_setOutput =
		    GetFmodExSymbol("?setOutput@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_OUTPUTTYPE@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_OUTPUTTYPE)>(
			   g_sym_FMOD_System_setOutput))(output);
}

FMOD_RESULT F_API FMOD::System::getOutput(FMOD_OUTPUTTYPE* output)
{
	if (!g_sym_FMOD_System_getOutput)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getOutput =
		    GetFmodExSymbol("?getOutput@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAW4FMOD_OUTPUTTYPE@@@Z");
#else
		g_sym_FMOD_System_getOutput =
		    GetFmodExSymbol("?getOutput@System@FMOD@@QAG?AW4FMOD_RESULT@@PAW4FMOD_OUTPUTTYPE@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_OUTPUTTYPE*)>(
			   g_sym_FMOD_System_getOutput))(output);
}

FMOD_RESULT F_API FMOD::System::getNumDrivers(int* numdrivers)
{
	if (!g_sym_FMOD_System_getNumDrivers)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getNumDrivers =
		    GetFmodExSymbol("?getNumDrivers@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_System_getNumDrivers =
		    GetFmodExSymbol("?getNumDrivers@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int*)>(g_sym_FMOD_System_getNumDrivers))(
	    numdrivers);
}

FMOD_RESULT F_API FMOD::System::getDriverInfo(int id, char* name, int namelen, FMOD_GUID* guid)
{
	if (g_hasC1Fmod)
	{
		if (!g_sym_FMOD_System_getDriverInfo)
		{
#ifdef BUILD_64BIT
			g_sym_FMOD_System_getDriverInfo =
			    GetFmodExSymbol("?getDriverName@System@FMOD@@QEAA?AW4FMOD_RESULT@@HPEADH@Z");
#else
			g_sym_FMOD_System_getDriverInfo =
			    GetFmodExSymbol("?getDriverName@System@FMOD@@QAG?AW4FMOD_RESULT@@HPADH@Z");
#endif
		}

		// no guid parameter
		return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int, char*, int)>(
				   g_sym_FMOD_System_getDriverInfo))(id, name, namelen);
	}

	if (!g_sym_FMOD_System_getDriverInfo)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getDriverInfo =
		    GetFmodExSymbol("?getDriverInfo@System@FMOD@@QEAA?AW4FMOD_RESULT@@HPEADHPEAUFMOD_GUID@@@Z");
#else
		g_sym_FMOD_System_getDriverInfo =
		    GetFmodExSymbol("?getDriverInfo@System@FMOD@@QAG?AW4FMOD_RESULT@@HPADHPAUFMOD_GUID@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int, char*, int, FMOD_GUID*)>(
			   g_sym_FMOD_System_getDriverInfo))(id, name, namelen, guid);
}

FMOD_RESULT F_API FMOD::System::getDriverCaps(int id, FMOD_CAPS* caps, int* minfrequency, int* maxfrequency,
                                              FMOD_SPEAKERMODE* controlpanelspeakermode)
{
	if (!g_sym_FMOD_System_getDriverCaps)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getDriverCaps = GetFmodExSymbol(
		    "?getDriverCaps@System@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAIPEAH1PEAW4FMOD_SPEAKERMODE@@@Z");
#else
		g_sym_FMOD_System_getDriverCaps =
		    GetFmodExSymbol("?getDriverCaps@System@FMOD@@QAG?AW4FMOD_RESULT@@HPAIPAH1PAW4FMOD_SPEAKERMODE@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(
			   int, FMOD_CAPS*, int*, int*, FMOD_SPEAKERMODE*)>(g_sym_FMOD_System_getDriverCaps))(
	    id, caps, minfrequency, maxfrequency, controlpanelspeakermode);
}

FMOD_RESULT F_API FMOD::System::setDriver(int driver)
{
	if (!g_sym_FMOD_System_setDriver)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setDriver = GetFmodExSymbol("?setDriver@System@FMOD@@QEAA?AW4FMOD_RESULT@@H@Z");
#else
		g_sym_FMOD_System_setDriver = GetFmodExSymbol("?setDriver@System@FMOD@@QAG?AW4FMOD_RESULT@@H@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int)>(g_sym_FMOD_System_setDriver))(
	    driver);
}

FMOD_RESULT F_API FMOD::System::getDriver(int* driver)
{
	if (!g_sym_FMOD_System_getDriver)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getDriver = GetFmodExSymbol("?getDriver@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_System_getDriver = GetFmodExSymbol("?getDriver@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int*)>(g_sym_FMOD_System_getDriver))(
	    driver);
}

FMOD_RESULT F_API FMOD::System::setHardwareChannels(int min2d, int max2d, int min3d, int max3d)
{
	if (!g_sym_FMOD_System_setHardwareChannels)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setHardwareChannels =
		    GetFmodExSymbol("?setHardwareChannels@System@FMOD@@QEAA?AW4FMOD_RESULT@@HHHH@Z");
#else
		g_sym_FMOD_System_setHardwareChannels =
		    GetFmodExSymbol("?setHardwareChannels@System@FMOD@@QAG?AW4FMOD_RESULT@@HHHH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int, int, int, int)>(
			   g_sym_FMOD_System_setHardwareChannels))(min2d, max2d, min3d, max3d);
}

FMOD_RESULT F_API FMOD::System::setSoftwareChannels(int numsoftwarechannels)
{
	if (!g_sym_FMOD_System_setSoftwareChannels)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setSoftwareChannels =
		    GetFmodExSymbol("?setSoftwareChannels@System@FMOD@@QEAA?AW4FMOD_RESULT@@H@Z");
#else
		g_sym_FMOD_System_setSoftwareChannels =
		    GetFmodExSymbol("?setSoftwareChannels@System@FMOD@@QAG?AW4FMOD_RESULT@@H@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int)>(
			   g_sym_FMOD_System_setSoftwareChannels))(numsoftwarechannels);
}

FMOD_RESULT F_API FMOD::System::getSoftwareChannels(int* numsoftwarechannels)
{
	if (!g_sym_FMOD_System_getSoftwareChannels)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getSoftwareChannels =
		    GetFmodExSymbol("?getSoftwareChannels@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_System_getSoftwareChannels =
		    GetFmodExSymbol("?getSoftwareChannels@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int*)>(
			   g_sym_FMOD_System_getSoftwareChannels))(numsoftwarechannels);
}

FMOD_RESULT F_API FMOD::System::setSoftwareFormat(int samplerate, FMOD_SOUND_FORMAT format, int numoutputchannels,
                                                  int maxinputchannels, FMOD_DSP_RESAMPLER resamplemethod)
{
	if (!g_sym_FMOD_System_setSoftwareFormat)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setSoftwareFormat =
		    GetFmodExSymbol("?setSoftwareFormat@System@FMOD@@QEAA?AW4FMOD_RESULT@@HW4FMOD_SOUND_FORMAT@@"
		                    "HHW4FMOD_DSP_RESAMPLER@@@Z");
#else
		g_sym_FMOD_System_setSoftwareFormat =
		    GetFmodExSymbol("?setSoftwareFormat@System@FMOD@@QAG?AW4FMOD_RESULT@@HW4FMOD_SOUND_FORMAT@@"
		                    "HHW4FMOD_DSP_RESAMPLER@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(
			   int, FMOD_SOUND_FORMAT, int, int, FMOD_DSP_RESAMPLER)>(g_sym_FMOD_System_setSoftwareFormat))(
	    samplerate, format, numoutputchannels, maxinputchannels, resamplemethod);
}

FMOD_RESULT F_API FMOD::System::getSoftwareFormat(int* samplerate, FMOD_SOUND_FORMAT* format, int* numoutputchannels,
                                                  int* maxinputchannels, FMOD_DSP_RESAMPLER* resamplemethod, int* bits)
{
	if (!g_sym_FMOD_System_getSoftwareFormat)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getSoftwareFormat =
		    GetFmodExSymbol("?getSoftwareFormat@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAHPEAW4FMOD_SOUND_FORMAT@@"
		                    "00PEAW4FMOD_DSP_RESAMPLER@@0@Z");
#else
		g_sym_FMOD_System_getSoftwareFormat =
		    GetFmodExSymbol("?getSoftwareFormat@System@FMOD@@QAG?AW4FMOD_RESULT@@PAHPAW4FMOD_SOUND_FORMAT@@"
		                    "00PAW4FMOD_DSP_RESAMPLER@@0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int*, FMOD_SOUND_FORMAT*, int*, int*,
	                                                                     FMOD_DSP_RESAMPLER*, int*)>(
			   g_sym_FMOD_System_getSoftwareFormat))(samplerate, format, numoutputchannels,
	                                                         maxinputchannels, resamplemethod, bits);
}

FMOD_RESULT F_API FMOD::System::setDSPBufferSize(unsigned int bufferlength, int numbuffers)
{
	if (!g_sym_FMOD_System_setDSPBufferSize)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setDSPBufferSize =
		    GetFmodExSymbol("?setDSPBufferSize@System@FMOD@@QEAA?AW4FMOD_RESULT@@IH@Z");
#else
		g_sym_FMOD_System_setDSPBufferSize =
		    GetFmodExSymbol("?setDSPBufferSize@System@FMOD@@QAG?AW4FMOD_RESULT@@IH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(unsigned int, int)>(
			   g_sym_FMOD_System_setDSPBufferSize))(bufferlength, numbuffers);
}

FMOD_RESULT F_API FMOD::System::getDSPBufferSize(unsigned int* bufferlength, int* numbuffers)
{
	if (!g_sym_FMOD_System_getDSPBufferSize)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getDSPBufferSize =
		    GetFmodExSymbol("?getDSPBufferSize@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAIPEAH@Z");
#else
		g_sym_FMOD_System_getDSPBufferSize =
		    GetFmodExSymbol("?getDSPBufferSize@System@FMOD@@QAG?AW4FMOD_RESULT@@PAIPAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(unsigned int*, int*)>(
			   g_sym_FMOD_System_getDSPBufferSize))(bufferlength, numbuffers);
}

FMOD_RESULT F_API FMOD::System::setFileSystem(FMOD_FILE_OPENCALLBACK useropen, FMOD_FILE_CLOSECALLBACK userclose,
                                              FMOD_FILE_READCALLBACK userread, FMOD_FILE_SEEKCALLBACK userseek,
                                              int blockalign)
{
	if (!g_sym_FMOD_System_setFileSystem)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setFileSystem =
		    GetFmodExSymbol("?setFileSystem@System@FMOD@@QEAA?AW4FMOD_RESULT@@P6A?AW43@PEBDHPEAIPEAPEAX2@ZP6A?"
		                    "AW43@PEAX4@ZP6A?AW43@44I14@ZP6A?AW43@4I4@ZH@Z");
#else
		g_sym_FMOD_System_setFileSystem =
		    GetFmodExSymbol("?setFileSystem@System@FMOD@@QAG?AW4FMOD_RESULT@@P6G?AW43@PBDHPAIPAPAX2@ZP6G?AW43@"
		                    "PAX4@ZP6G?AW43@44I14@ZP6G?AW43@4I4@ZH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(
			   FMOD_FILE_OPENCALLBACK, FMOD_FILE_CLOSECALLBACK, FMOD_FILE_READCALLBACK,
			   FMOD_FILE_SEEKCALLBACK, int)>(g_sym_FMOD_System_setFileSystem))(
	    useropen, userclose, userread, userseek, blockalign);
}

FMOD_RESULT F_API FMOD::System::attachFileSystem(FMOD_FILE_OPENCALLBACK useropen, FMOD_FILE_CLOSECALLBACK userclose,
                                                 FMOD_FILE_READCALLBACK userread, FMOD_FILE_SEEKCALLBACK userseek)
{
	if (!g_sym_FMOD_System_attachFileSystem)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_attachFileSystem =
		    GetFmodExSymbol("?attachFileSystem@System@FMOD@@QEAA?AW4FMOD_RESULT@@P6A?AW43@PEBDHPEAIPEAPEAX2@"
		                    "ZP6A?AW43@PEAX4@ZP6A?AW43@44I14@ZP6A?AW43@4I4@Z@Z");
#else
		g_sym_FMOD_System_attachFileSystem =
		    GetFmodExSymbol("?attachFileSystem@System@FMOD@@QAG?AW4FMOD_RESULT@@P6G?AW43@PBDHPAIPAPAX2@ZP6G?"
		                    "AW43@PAX4@ZP6G?AW43@44I14@ZP6G?AW43@4I4@Z@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(
			   FMOD_FILE_OPENCALLBACK, FMOD_FILE_CLOSECALLBACK, FMOD_FILE_READCALLBACK,
			   FMOD_FILE_SEEKCALLBACK)>(g_sym_FMOD_System_attachFileSystem))(useropen, userclose, userread,
	                                                                                 userseek);
}

FMOD_RESULT F_API FMOD::System::setAdvancedSettings(FMOD_ADVANCEDSETTINGS* settings)
{
	if (!g_sym_FMOD_System_setAdvancedSettings)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setAdvancedSettings = GetFmodExSymbol(
		    "?setAdvancedSettings@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_ADVANCEDSETTINGS@@@Z");
#else
		g_sym_FMOD_System_setAdvancedSettings = GetFmodExSymbol(
		    "?setAdvancedSettings@System@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_ADVANCEDSETTINGS@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_ADVANCEDSETTINGS*)>(
			   g_sym_FMOD_System_setAdvancedSettings))(settings);
}

FMOD_RESULT F_API FMOD::System::getAdvancedSettings(FMOD_ADVANCEDSETTINGS* settings)
{
	if (!g_sym_FMOD_System_getAdvancedSettings)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getAdvancedSettings = GetFmodExSymbol(
		    "?getAdvancedSettings@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_ADVANCEDSETTINGS@@@Z");
#else
		g_sym_FMOD_System_getAdvancedSettings = GetFmodExSymbol(
		    "?getAdvancedSettings@System@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_ADVANCEDSETTINGS@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_ADVANCEDSETTINGS*)>(
			   g_sym_FMOD_System_getAdvancedSettings))(settings);
}

FMOD_RESULT F_API FMOD::System::setSpeakerMode(FMOD_SPEAKERMODE speakermode)
{
	if (!g_sym_FMOD_System_setSpeakerMode)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setSpeakerMode =
		    GetFmodExSymbol("?setSpeakerMode@System@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_SPEAKERMODE@@@Z");
#else
		g_sym_FMOD_System_setSpeakerMode =
		    GetFmodExSymbol("?setSpeakerMode@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_SPEAKERMODE@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_SPEAKERMODE)>(
			   g_sym_FMOD_System_setSpeakerMode))(speakermode);
}

FMOD_RESULT F_API FMOD::System::getSpeakerMode(FMOD_SPEAKERMODE* speakermode)
{
	if (!g_sym_FMOD_System_getSpeakerMode)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getSpeakerMode =
		    GetFmodExSymbol("?getSpeakerMode@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAW4FMOD_SPEAKERMODE@@@Z");
#else
		g_sym_FMOD_System_getSpeakerMode =
		    GetFmodExSymbol("?getSpeakerMode@System@FMOD@@QAG?AW4FMOD_RESULT@@PAW4FMOD_SPEAKERMODE@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_SPEAKERMODE*)>(
			   g_sym_FMOD_System_getSpeakerMode))(speakermode);
}

FMOD_RESULT F_API FMOD::System::setCallback(FMOD_SYSTEM_CALLBACKTYPE type, FMOD_SYSTEM_CALLBACK callback)
{
	if (!g_sym_FMOD_System_setCallback)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setCallback =
		    GetFmodExSymbol("?setCallback@System@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_SYSTEM_CALLBACKTYPE@@P6A?"
		                    "AW43@PEAUFMOD_SYSTEM@@0PEAX2@Z@Z");
#else
		g_sym_FMOD_System_setCallback =
		    GetFmodExSymbol("?setCallback@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_SYSTEM_CALLBACKTYPE@@P6G?"
		                    "AW43@PAUFMOD_SYSTEM@@0PAX2@Z@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(
			   FMOD_SYSTEM_CALLBACKTYPE, FMOD_SYSTEM_CALLBACK)>(g_sym_FMOD_System_setCallback))(type,
	                                                                                                    callback);
}

FMOD_RESULT F_API FMOD::System::setPluginPath(const char* path)
{
	if (!g_sym_FMOD_System_setPluginPath)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setPluginPath =
		    GetFmodExSymbol("?setPluginPath@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEBD@Z");
#else
		g_sym_FMOD_System_setPluginPath =
		    GetFmodExSymbol("?setPluginPath@System@FMOD@@QAG?AW4FMOD_RESULT@@PBD@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(const char*)>(
			   g_sym_FMOD_System_setPluginPath))(path);
}

FMOD_RESULT F_API FMOD::System::loadPlugin(const char* filename, FMOD_PLUGINTYPE* plugintype, int* index)
{
	if (!g_sym_FMOD_System_loadPlugin)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_loadPlugin =
		    GetFmodExSymbol("?loadPlugin@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEBDPEAW4FMOD_PLUGINTYPE@@PEAH@Z");
#else
		g_sym_FMOD_System_loadPlugin =
		    GetFmodExSymbol("?loadPlugin@System@FMOD@@QAG?AW4FMOD_RESULT@@PBDPAW4FMOD_PLUGINTYPE@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(const char*, FMOD_PLUGINTYPE*, int*)>(
			   g_sym_FMOD_System_loadPlugin))(filename, plugintype, index);
}

FMOD_RESULT F_API FMOD::System::getNumPlugins(FMOD_PLUGINTYPE plugintype, int* numplugins)
{
	if (!g_sym_FMOD_System_getNumPlugins)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getNumPlugins =
		    GetFmodExSymbol("?getNumPlugins@System@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_PLUGINTYPE@@PEAH@Z");
#else
		g_sym_FMOD_System_getNumPlugins =
		    GetFmodExSymbol("?getNumPlugins@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_PLUGINTYPE@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_PLUGINTYPE, int*)>(
			   g_sym_FMOD_System_getNumPlugins))(plugintype, numplugins);
}

FMOD_RESULT F_API FMOD::System::getPluginInfo(FMOD_PLUGINTYPE plugintype, int index, char* name, int namelen,
                                              unsigned int* version)
{
	if (!g_sym_FMOD_System_getPluginInfo)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getPluginInfo =
		    GetFmodExSymbol("?getPluginInfo@System@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_PLUGINTYPE@@HPEADHPEAI@Z");
#else
		g_sym_FMOD_System_getPluginInfo =
		    GetFmodExSymbol("?getPluginInfo@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_PLUGINTYPE@@HPADHPAI@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(
			   FMOD_PLUGINTYPE, int, char*, int, unsigned int*)>(g_sym_FMOD_System_getPluginInfo))(
	    plugintype, index, name, namelen, version);
}

FMOD_RESULT F_API FMOD::System::unloadPlugin(FMOD_PLUGINTYPE plugintype, int index)
{
	if (!g_sym_FMOD_System_unloadPlugin)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_unloadPlugin =
		    GetFmodExSymbol("?unloadPlugin@System@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_PLUGINTYPE@@H@Z");
#else
		g_sym_FMOD_System_unloadPlugin =
		    GetFmodExSymbol("?unloadPlugin@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_PLUGINTYPE@@H@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_PLUGINTYPE, int)>(
			   g_sym_FMOD_System_unloadPlugin))(plugintype, index);
}

FMOD_RESULT F_API FMOD::System::setOutputByPlugin(int index)
{
	if (!g_sym_FMOD_System_setOutputByPlugin)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setOutputByPlugin =
		    GetFmodExSymbol("?setOutputByPlugin@System@FMOD@@QEAA?AW4FMOD_RESULT@@H@Z");
#else
		g_sym_FMOD_System_setOutputByPlugin =
		    GetFmodExSymbol("?setOutputByPlugin@System@FMOD@@QAG?AW4FMOD_RESULT@@H@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int)>(
			   g_sym_FMOD_System_setOutputByPlugin))(index);
}

FMOD_RESULT F_API FMOD::System::getOutputByPlugin(int* index)
{
	if (!g_sym_FMOD_System_getOutputByPlugin)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getOutputByPlugin =
		    GetFmodExSymbol("?getOutputByPlugin@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_System_getOutputByPlugin =
		    GetFmodExSymbol("?getOutputByPlugin@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int*)>(
			   g_sym_FMOD_System_getOutputByPlugin))(index);
}

FMOD_RESULT F_API FMOD::System::createCodec(FMOD_CODEC_DESCRIPTION* description)
{
	if (!g_sym_FMOD_System_createCodec)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_createCodec =
		    GetFmodExSymbol("?createCodec@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_CODEC_DESCRIPTION@@@Z");
#else
		g_sym_FMOD_System_createCodec =
		    GetFmodExSymbol("?createCodec@System@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_CODEC_DESCRIPTION@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_CODEC_DESCRIPTION*)>(
			   g_sym_FMOD_System_createCodec))(description);
}

FMOD_RESULT F_API FMOD::System::init(int maxchannels, FMOD_INITFLAGS flags, void* extradriverdata)
{
	if (!g_sym_FMOD_System_init)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_init = GetFmodExSymbol("?init@System@FMOD@@QEAA?AW4FMOD_RESULT@@HIPEAX@Z");
#else
		g_sym_FMOD_System_init = GetFmodExSymbol("?init@System@FMOD@@QAG?AW4FMOD_RESULT@@HIPAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int, FMOD_INITFLAGS, void*)>(
			   g_sym_FMOD_System_init))(maxchannels, flags, extradriverdata);
}

FMOD_RESULT F_API FMOD::System::close()
{
	if (!g_sym_FMOD_System_close)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_close = GetFmodExSymbol("?close@System@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_System_close = GetFmodExSymbol("?close@System@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)()>(g_sym_FMOD_System_close))();
}

FMOD_RESULT F_API FMOD::System::update()
{
	if (!g_sym_FMOD_System_update)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_update = GetFmodExSymbol("?update@System@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_System_update = GetFmodExSymbol("?update@System@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)()>(g_sym_FMOD_System_update))();
}

FMOD_RESULT F_API FMOD::System::set3DSettings(float dopplerscale, float distancefactor, float rolloffscale)
{
	if (!g_sym_FMOD_System_set3DSettings)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_set3DSettings =
		    GetFmodExSymbol("?set3DSettings@System@FMOD@@QEAA?AW4FMOD_RESULT@@MMM@Z");
#else
		g_sym_FMOD_System_set3DSettings =
		    GetFmodExSymbol("?set3DSettings@System@FMOD@@QAG?AW4FMOD_RESULT@@MMM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(float, float, float)>(
			   g_sym_FMOD_System_set3DSettings))(dopplerscale, distancefactor, rolloffscale);
}

FMOD_RESULT F_API FMOD::System::get3DSettings(float* dopplerscale, float* distancefactor, float* rolloffscale)
{
	if (!g_sym_FMOD_System_get3DSettings)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_get3DSettings =
		    GetFmodExSymbol("?get3DSettings@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM00@Z");
#else
		g_sym_FMOD_System_get3DSettings =
		    GetFmodExSymbol("?get3DSettings@System@FMOD@@QAG?AW4FMOD_RESULT@@PAM00@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(float*, float*, float*)>(
			   g_sym_FMOD_System_get3DSettings))(dopplerscale, distancefactor, rolloffscale);
}

FMOD_RESULT F_API FMOD::System::set3DNumListeners(int numlisteners)
{
	if (!g_sym_FMOD_System_set3DNumListeners)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_set3DNumListeners =
		    GetFmodExSymbol("?set3DNumListeners@System@FMOD@@QEAA?AW4FMOD_RESULT@@H@Z");
#else
		g_sym_FMOD_System_set3DNumListeners =
		    GetFmodExSymbol("?set3DNumListeners@System@FMOD@@QAG?AW4FMOD_RESULT@@H@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int)>(
			   g_sym_FMOD_System_set3DNumListeners))(numlisteners);
}

FMOD_RESULT F_API FMOD::System::get3DNumListeners(int* numlisteners)
{
	if (!g_sym_FMOD_System_get3DNumListeners)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_get3DNumListeners =
		    GetFmodExSymbol("?get3DNumListeners@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_System_get3DNumListeners =
		    GetFmodExSymbol("?get3DNumListeners@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int*)>(
			   g_sym_FMOD_System_get3DNumListeners))(numlisteners);
}

FMOD_RESULT F_API FMOD::System::set3DListenerAttributes(int listener, const FMOD_VECTOR* pos, const FMOD_VECTOR* vel,
                                                        const FMOD_VECTOR* forward, const FMOD_VECTOR* up)
{
	if (!g_sym_FMOD_System_set3DListenerAttributes)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_set3DListenerAttributes = GetFmodExSymbol(
		    "?set3DListenerAttributes@System@FMOD@@QEAA?AW4FMOD_RESULT@@HPEBUFMOD_VECTOR@@000@Z");
#else
		g_sym_FMOD_System_set3DListenerAttributes =
		    GetFmodExSymbol("?set3DListenerAttributes@System@FMOD@@QAG?AW4FMOD_RESULT@@HPBUFMOD_VECTOR@@000@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(
			   int, const FMOD_VECTOR*, const FMOD_VECTOR*, const FMOD_VECTOR*, const FMOD_VECTOR*)>(
			   g_sym_FMOD_System_set3DListenerAttributes))(listener, pos, vel, forward, up);
}

FMOD_RESULT F_API FMOD::System::get3DListenerAttributes(int listener, FMOD_VECTOR* pos, FMOD_VECTOR* vel,
                                                        FMOD_VECTOR* forward, FMOD_VECTOR* up)
{
	if (!g_sym_FMOD_System_get3DListenerAttributes)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_get3DListenerAttributes = GetFmodExSymbol(
		    "?get3DListenerAttributes@System@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAUFMOD_VECTOR@@000@Z");
#else
		g_sym_FMOD_System_get3DListenerAttributes =
		    GetFmodExSymbol("?get3DListenerAttributes@System@FMOD@@QAG?AW4FMOD_RESULT@@HPAUFMOD_VECTOR@@000@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int, FMOD_VECTOR*, FMOD_VECTOR*,
	                                                                     FMOD_VECTOR*, FMOD_VECTOR*)>(
			   g_sym_FMOD_System_get3DListenerAttributes))(listener, pos, vel, forward, up);
}

FMOD_RESULT F_API FMOD::System::set3DRolloffCallback(FMOD_3D_ROLLOFFCALLBACK callback)
{
	if (!g_sym_FMOD_System_set3DRolloffCallback)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_set3DRolloffCallback = GetFmodExSymbol(
		    "?set3DRolloffCallback@System@FMOD@@QEAA?AW4FMOD_RESULT@@P6AMPEAUFMOD_CHANNEL@@M@Z@Z");
#else
		g_sym_FMOD_System_set3DRolloffCallback = GetFmodExSymbol(
		    "?set3DRolloffCallback@System@FMOD@@QAG?AW4FMOD_RESULT@@P6GMPAUFMOD_CHANNEL@@M@Z@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_3D_ROLLOFFCALLBACK)>(
			   g_sym_FMOD_System_set3DRolloffCallback))(callback);
}

FMOD_RESULT F_API FMOD::System::set3DSpeakerPosition(FMOD_SPEAKER speaker, float x, float y, bool active)
{
	if (!g_sym_FMOD_System_set3DSpeakerPosition)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_set3DSpeakerPosition =
		    GetFmodExSymbol("?set3DSpeakerPosition@System@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_SPEAKER@@MM_N@Z");
#else
		g_sym_FMOD_System_set3DSpeakerPosition =
		    GetFmodExSymbol("?set3DSpeakerPosition@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_SPEAKER@@MM_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_SPEAKER, float, float, bool)>(
			   g_sym_FMOD_System_set3DSpeakerPosition))(speaker, x, y, active);
}

FMOD_RESULT F_API FMOD::System::get3DSpeakerPosition(FMOD_SPEAKER speaker, float* x, float* y, bool* active)
{
	if (!g_sym_FMOD_System_get3DSpeakerPosition)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_get3DSpeakerPosition = GetFmodExSymbol(
		    "?get3DSpeakerPosition@System@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_SPEAKER@@PEAM1PEA_N@Z");
#else
		g_sym_FMOD_System_get3DSpeakerPosition = GetFmodExSymbol(
		    "?get3DSpeakerPosition@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_SPEAKER@@PAM1PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_SPEAKER, float*, float*, bool*)>(
			   g_sym_FMOD_System_get3DSpeakerPosition))(speaker, x, y, active);
}

FMOD_RESULT F_API FMOD::System::setStreamBufferSize(unsigned int filebuffersize, FMOD_TIMEUNIT filebuffersizetype)
{
	if (!g_sym_FMOD_System_setStreamBufferSize)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setStreamBufferSize =
		    GetFmodExSymbol("?setStreamBufferSize@System@FMOD@@QEAA?AW4FMOD_RESULT@@II@Z");
#else
		g_sym_FMOD_System_setStreamBufferSize =
		    GetFmodExSymbol("?setStreamBufferSize@System@FMOD@@QAG?AW4FMOD_RESULT@@II@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(unsigned int, FMOD_TIMEUNIT)>(
			   g_sym_FMOD_System_setStreamBufferSize))(filebuffersize, filebuffersizetype);
}

FMOD_RESULT F_API FMOD::System::getStreamBufferSize(unsigned int* filebuffersize, FMOD_TIMEUNIT* filebuffersizetype)
{
	if (!g_sym_FMOD_System_getStreamBufferSize)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getStreamBufferSize =
		    GetFmodExSymbol("?getStreamBufferSize@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAI0@Z");
#else
		g_sym_FMOD_System_getStreamBufferSize =
		    GetFmodExSymbol("?getStreamBufferSize@System@FMOD@@QAG?AW4FMOD_RESULT@@PAI0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(unsigned int*, FMOD_TIMEUNIT*)>(
			   g_sym_FMOD_System_getStreamBufferSize))(filebuffersize, filebuffersizetype);
}

FMOD_RESULT F_API FMOD::System::getVersion(unsigned int* version)
{
	if (!g_sym_FMOD_System_getVersion)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getVersion = GetFmodExSymbol("?getVersion@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAI@Z");
#else
		g_sym_FMOD_System_getVersion = GetFmodExSymbol("?getVersion@System@FMOD@@QAG?AW4FMOD_RESULT@@PAI@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(unsigned int*)>(
			   g_sym_FMOD_System_getVersion))(version);
}

FMOD_RESULT F_API FMOD::System::getOutputHandle(void** handle)
{
	if (!g_sym_FMOD_System_getOutputHandle)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getOutputHandle =
		    GetFmodExSymbol("?getOutputHandle@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAX@Z");
#else
		g_sym_FMOD_System_getOutputHandle =
		    GetFmodExSymbol("?getOutputHandle@System@FMOD@@QAG?AW4FMOD_RESULT@@PAPAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(void**)>(
			   g_sym_FMOD_System_getOutputHandle))(handle);
}

FMOD_RESULT F_API FMOD::System::getChannelsPlaying(int* channels)
{
	if (!g_sym_FMOD_System_getChannelsPlaying)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getChannelsPlaying =
		    GetFmodExSymbol("?getChannelsPlaying@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_System_getChannelsPlaying =
		    GetFmodExSymbol("?getChannelsPlaying@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int*)>(
			   g_sym_FMOD_System_getChannelsPlaying))(channels);
}

FMOD_RESULT F_API FMOD::System::getHardwareChannels(int* num2d, int* num3d, int* total)
{
	if (!g_sym_FMOD_System_getHardwareChannels)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getHardwareChannels =
		    GetFmodExSymbol("?getHardwareChannels@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH00@Z");
#else
		g_sym_FMOD_System_getHardwareChannels =
		    GetFmodExSymbol("?getHardwareChannels@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH00@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int*, int*, int*)>(
			   g_sym_FMOD_System_getHardwareChannels))(num2d, num3d, total);
}

FMOD_RESULT F_API FMOD::System::getCPUUsage(float* dsp, float* stream, float* update, float* total)
{
	if (!g_sym_FMOD_System_getCPUUsage)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getCPUUsage =
		    GetFmodExSymbol("?getCPUUsage@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM000@Z");
#else
		g_sym_FMOD_System_getCPUUsage =
		    GetFmodExSymbol("?getCPUUsage@System@FMOD@@QAG?AW4FMOD_RESULT@@PAM000@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(float*, float*, float*, float*)>(
			   g_sym_FMOD_System_getCPUUsage))(dsp, stream, update, total);
}

FMOD_RESULT F_API FMOD::System::getSoundRAM(int* currentalloced, int* maxalloced, int* total)
{
	if (!g_sym_FMOD_System_getSoundRAM)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getSoundRAM =
		    GetFmodExSymbol("?getSoundRAM@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH00@Z");
#else
		g_sym_FMOD_System_getSoundRAM =
		    GetFmodExSymbol("?getSoundRAM@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH00@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int*, int*, int*)>(
			   g_sym_FMOD_System_getSoundRAM))(currentalloced, maxalloced, total);
}

FMOD_RESULT F_API FMOD::System::getNumCDROMDrives(int* numdrives)
{
	if (!g_sym_FMOD_System_getNumCDROMDrives)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getNumCDROMDrives =
		    GetFmodExSymbol("?getNumCDROMDrives@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_System_getNumCDROMDrives =
		    GetFmodExSymbol("?getNumCDROMDrives@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int*)>(
			   g_sym_FMOD_System_getNumCDROMDrives))(numdrives);
}

FMOD_RESULT F_API FMOD::System::getCDROMDriveName(int drive, char* drivename, int drivenamelen, char* scsiname,
                                                  int scsinamelen, char* devicename, int devicenamelen)
{
	if (!g_sym_FMOD_System_getCDROMDriveName)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getCDROMDriveName =
		    GetFmodExSymbol("?getCDROMDriveName@System@FMOD@@QEAA?AW4FMOD_RESULT@@HPEADH0H0H@Z");
#else
		g_sym_FMOD_System_getCDROMDriveName =
		    GetFmodExSymbol("?getCDROMDriveName@System@FMOD@@QAG?AW4FMOD_RESULT@@HPADH0H0H@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int, char*, int, char*, int, char*, int)>(
			   g_sym_FMOD_System_getCDROMDriveName))(drive, drivename, drivenamelen, scsiname, scsinamelen,
	                                                         devicename, devicenamelen);
}

FMOD_RESULT F_API FMOD::System::getSpectrum(float* spectrumarray, int numvalues, int channeloffset,
                                            FMOD_DSP_FFT_WINDOW windowtype)
{
	if (!g_sym_FMOD_System_getSpectrum)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getSpectrum =
		    GetFmodExSymbol("?getSpectrum@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAMHHW4FMOD_DSP_FFT_WINDOW@@@Z");
#else
		g_sym_FMOD_System_getSpectrum =
		    GetFmodExSymbol("?getSpectrum@System@FMOD@@QAG?AW4FMOD_RESULT@@PAMHHW4FMOD_DSP_FFT_WINDOW@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(float*, int, int, FMOD_DSP_FFT_WINDOW)>(
			   g_sym_FMOD_System_getSpectrum))(spectrumarray, numvalues, channeloffset, windowtype);
}

FMOD_RESULT F_API FMOD::System::getWaveData(float* wavearray, int numvalues, int channeloffset)
{
	if (!g_sym_FMOD_System_getWaveData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getWaveData =
		    GetFmodExSymbol("?getWaveData@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAMHH@Z");
#else
		g_sym_FMOD_System_getWaveData =
		    GetFmodExSymbol("?getWaveData@System@FMOD@@QAG?AW4FMOD_RESULT@@PAMHH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(float*, int, int)>(
			   g_sym_FMOD_System_getWaveData))(wavearray, numvalues, channeloffset);
}

FMOD_RESULT F_API FMOD::System::createSound(const char* name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO* exinfo,
                                            Sound** sound)
{
	if (!g_sym_FMOD_System_createSound)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_createSound = GetFmodExSymbol("?createSound@System@FMOD@@QEAA?AW4FMOD_RESULT@@"
		                                                "PEBDIPEAUFMOD_CREATESOUNDEXINFO@@PEAPEAVSound@2@@Z");
#else
		g_sym_FMOD_System_createSound = GetFmodExSymbol(
		    "?createSound@System@FMOD@@QAG?AW4FMOD_RESULT@@PBDIPAUFMOD_CREATESOUNDEXINFO@@PAPAVSound@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(
			   const char*, FMOD_MODE, FMOD_CREATESOUNDEXINFO*, Sound**)>(g_sym_FMOD_System_createSound))(
	    name_or_data, mode, exinfo, sound);
}

FMOD_RESULT F_API FMOD::System::createStream(const char* name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO* exinfo,
                                             Sound** sound)
{
	if (!g_sym_FMOD_System_createStream)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_createStream = GetFmodExSymbol("?createStream@System@FMOD@@QEAA?AW4FMOD_RESULT@@"
		                                                 "PEBDIPEAUFMOD_CREATESOUNDEXINFO@@PEAPEAVSound@2@@Z");
#else
		g_sym_FMOD_System_createStream = GetFmodExSymbol(
		    "?createStream@System@FMOD@@QAG?AW4FMOD_RESULT@@PBDIPAUFMOD_CREATESOUNDEXINFO@@PAPAVSound@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(
			   const char*, FMOD_MODE, FMOD_CREATESOUNDEXINFO*, Sound**)>(g_sym_FMOD_System_createStream))(
	    name_or_data, mode, exinfo, sound);
}

FMOD_RESULT F_API FMOD::System::createDSP(FMOD_DSP_DESCRIPTION* description, DSP** dsp)
{
	if (!g_sym_FMOD_System_createDSP)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_createDSP = GetFmodExSymbol(
		    "?createDSP@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_DSP_DESCRIPTION@@PEAPEAVDSP@2@@Z");
#else
		g_sym_FMOD_System_createDSP = GetFmodExSymbol(
		    "?createDSP@System@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_DSP_DESCRIPTION@@PAPAVDSP@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_DSP_DESCRIPTION*, DSP**)>(
			   g_sym_FMOD_System_createDSP))(description, dsp);
}

FMOD_RESULT F_API FMOD::System::createDSPByType(FMOD_DSP_TYPE type, DSP** dsp)
{
	if (!g_sym_FMOD_System_createDSPByType)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_createDSPByType = GetFmodExSymbol(
		    "?createDSPByType@System@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_DSP_TYPE@@PEAPEAVDSP@2@@Z");
#else
		g_sym_FMOD_System_createDSPByType =
		    GetFmodExSymbol("?createDSPByType@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_DSP_TYPE@@PAPAVDSP@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_DSP_TYPE, DSP**)>(
			   g_sym_FMOD_System_createDSPByType))(type, dsp);
}

FMOD_RESULT F_API FMOD::System::createDSPByIndex(int index, DSP** dsp)
{
	if (!g_sym_FMOD_System_createDSPByIndex)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_createDSPByIndex =
		    GetFmodExSymbol("?createDSPByIndex@System@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAPEAVDSP@2@@Z");
#else
		g_sym_FMOD_System_createDSPByIndex =
		    GetFmodExSymbol("?createDSPByIndex@System@FMOD@@QAG?AW4FMOD_RESULT@@HPAPAVDSP@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int, DSP**)>(
			   g_sym_FMOD_System_createDSPByIndex))(index, dsp);
}

FMOD_RESULT F_API FMOD::System::createChannelGroup(const char* name, ChannelGroup** channelgroup)
{
	if (!g_sym_FMOD_System_createChannelGroup)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_createChannelGroup = GetFmodExSymbol(
		    "?createChannelGroup@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEBDPEAPEAVChannelGroup@2@@Z");
#else
		g_sym_FMOD_System_createChannelGroup =
		    GetFmodExSymbol("?createChannelGroup@System@FMOD@@QAG?AW4FMOD_RESULT@@PBDPAPAVChannelGroup@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(const char*, ChannelGroup**)>(
			   g_sym_FMOD_System_createChannelGroup))(name, channelgroup);
}

FMOD_RESULT F_API FMOD::System::createSoundGroup(const char* name, SoundGroup** soundgroup)
{
	if (!g_sym_FMOD_System_createSoundGroup)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_createSoundGroup =
		    GetFmodExSymbol("?createSoundGroup@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEBDPEAPEAVSoundGroup@2@@Z");
#else
		g_sym_FMOD_System_createSoundGroup =
		    GetFmodExSymbol("?createSoundGroup@System@FMOD@@QAG?AW4FMOD_RESULT@@PBDPAPAVSoundGroup@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(const char*, SoundGroup**)>(
			   g_sym_FMOD_System_createSoundGroup))(name, soundgroup);
}

FMOD_RESULT F_API FMOD::System::createReverb(Reverb** reverb)
{
	if (!g_sym_FMOD_System_createReverb)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_createReverb =
		    GetFmodExSymbol("?createReverb@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVReverb@2@@Z");
#else
		g_sym_FMOD_System_createReverb =
		    GetFmodExSymbol("?createReverb@System@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVReverb@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(Reverb**)>(
			   g_sym_FMOD_System_createReverb))(reverb);
}

FMOD_RESULT F_API FMOD::System::playSound(FMOD_CHANNELINDEX channelid, Sound* sound, bool paused, Channel** channel)
{
	if (!g_sym_FMOD_System_playSound)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_playSound = GetFmodExSymbol("?playSound@System@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_"
		                                              "CHANNELINDEX@@PEAVSound@2@_NPEAPEAVChannel@2@@Z");
#else
		g_sym_FMOD_System_playSound = GetFmodExSymbol(
		    "?playSound@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_CHANNELINDEX@@PAVSound@2@_NPAPAVChannel@2@@Z");
#endif
	}

	return (
	    this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_CHANNELINDEX, Sound*, bool, Channel**)>(
		       g_sym_FMOD_System_playSound))(channelid, sound, paused, channel);
}

FMOD_RESULT F_API FMOD::System::playDSP(FMOD_CHANNELINDEX channelid, DSP* dsp, bool paused, Channel** channel)
{
	if (!g_sym_FMOD_System_playDSP)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_playDSP = GetFmodExSymbol(
		    "?playDSP@System@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_CHANNELINDEX@@PEAVDSP@2@_NPEAPEAVChannel@2@@Z");
#else
		g_sym_FMOD_System_playDSP = GetFmodExSymbol(
		    "?playDSP@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_CHANNELINDEX@@PAVDSP@2@_NPAPAVChannel@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_CHANNELINDEX, DSP*, bool, Channel**)>(
			   g_sym_FMOD_System_playDSP))(channelid, dsp, paused, channel);
}

FMOD_RESULT F_API FMOD::System::getChannel(int channelid, Channel** channel)
{
	if (!g_sym_FMOD_System_getChannel)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getChannel =
		    GetFmodExSymbol("?getChannel@System@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAPEAVChannel@2@@Z");
#else
		g_sym_FMOD_System_getChannel =
		    GetFmodExSymbol("?getChannel@System@FMOD@@QAG?AW4FMOD_RESULT@@HPAPAVChannel@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int, Channel**)>(
			   g_sym_FMOD_System_getChannel))(channelid, channel);
}

FMOD_RESULT F_API FMOD::System::getMasterChannelGroup(ChannelGroup** channelgroup)
{
	if (!g_sym_FMOD_System_getMasterChannelGroup)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getMasterChannelGroup = GetFmodExSymbol(
		    "?getMasterChannelGroup@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVChannelGroup@2@@Z");
#else
		g_sym_FMOD_System_getMasterChannelGroup =
		    GetFmodExSymbol("?getMasterChannelGroup@System@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVChannelGroup@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(ChannelGroup**)>(
			   g_sym_FMOD_System_getMasterChannelGroup))(channelgroup);
}

FMOD_RESULT F_API FMOD::System::getMasterSoundGroup(SoundGroup** soundgroup)
{
	if (!g_sym_FMOD_System_getMasterSoundGroup)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getMasterSoundGroup =
		    GetFmodExSymbol("?getMasterSoundGroup@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVSoundGroup@2@@Z");
#else
		g_sym_FMOD_System_getMasterSoundGroup =
		    GetFmodExSymbol("?getMasterSoundGroup@System@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVSoundGroup@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(SoundGroup**)>(
			   g_sym_FMOD_System_getMasterSoundGroup))(soundgroup);
}

FMOD_RESULT F_API FMOD::System::setReverbProperties(const FMOD_REVERB_PROPERTIES* prop)
{
	if (!g_sym_FMOD_System_setReverbProperties)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setReverbProperties = GetFmodExSymbol(
		    "?setReverbProperties@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEBUFMOD_REVERB_PROPERTIES@@@Z");
#else
		g_sym_FMOD_System_setReverbProperties = GetFmodExSymbol(
		    "?setReverbProperties@System@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_REVERB_PROPERTIES@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(const FMOD_REVERB_PROPERTIES*)>(
			   g_sym_FMOD_System_setReverbProperties))(prop);
}

FMOD_RESULT F_API FMOD::System::getReverbProperties(FMOD_REVERB_PROPERTIES* prop)
{
	if (!g_sym_FMOD_System_getReverbProperties)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getReverbProperties = GetFmodExSymbol(
		    "?getReverbProperties@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_REVERB_PROPERTIES@@@Z");
#else
		g_sym_FMOD_System_getReverbProperties = GetFmodExSymbol(
		    "?getReverbProperties@System@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_REVERB_PROPERTIES@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_REVERB_PROPERTIES*)>(
			   g_sym_FMOD_System_getReverbProperties))(prop);
}

FMOD_RESULT F_API FMOD::System::setReverbAmbientProperties(FMOD_REVERB_PROPERTIES* prop)
{
	if (!g_sym_FMOD_System_setReverbAmbientProperties)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setReverbAmbientProperties = GetFmodExSymbol(
		    "?setReverbAmbientProperties@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_REVERB_PROPERTIES@@@Z");
#else
		g_sym_FMOD_System_setReverbAmbientProperties = GetFmodExSymbol(
		    "?setReverbAmbientProperties@System@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_REVERB_PROPERTIES@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_REVERB_PROPERTIES*)>(
			   g_sym_FMOD_System_setReverbAmbientProperties))(prop);
}

FMOD_RESULT F_API FMOD::System::getReverbAmbientProperties(FMOD_REVERB_PROPERTIES* prop)
{
	if (!g_sym_FMOD_System_getReverbAmbientProperties)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getReverbAmbientProperties = GetFmodExSymbol(
		    "?getReverbAmbientProperties@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_REVERB_PROPERTIES@@@Z");
#else
		g_sym_FMOD_System_getReverbAmbientProperties = GetFmodExSymbol(
		    "?getReverbAmbientProperties@System@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_REVERB_PROPERTIES@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(FMOD_REVERB_PROPERTIES*)>(
			   g_sym_FMOD_System_getReverbAmbientProperties))(prop);
}

FMOD_RESULT F_API FMOD::System::getDSPHead(DSP** dsp)
{
	if (!g_sym_FMOD_System_getDSPHead)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getDSPHead =
		    GetFmodExSymbol("?getDSPHead@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVDSP@2@@Z");
#else
		g_sym_FMOD_System_getDSPHead =
		    GetFmodExSymbol("?getDSPHead@System@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVDSP@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(DSP**)>(g_sym_FMOD_System_getDSPHead))(
	    dsp);
}

FMOD_RESULT F_API FMOD::System::addDSP(DSP* dsp, DSPConnection** connection)
{
	if (g_hasC1Fmod)
	{
		if (!g_sym_FMOD_System_addDSP)
		{
#ifdef BUILD_64BIT
			g_sym_FMOD_System_addDSP =
			    GetFmodExSymbol("?addDSP@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAVDSP@2@@Z");
#else
			g_sym_FMOD_System_addDSP =
			    GetFmodExSymbol("?addDSP@System@FMOD@@QAG?AW4FMOD_RESULT@@PAVDSP@2@@Z");
#endif
		}

		// no connection parameter
		return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(DSP*)>(g_sym_FMOD_System_addDSP))(
		    dsp);
	}

	if (!g_sym_FMOD_System_addDSP)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_addDSP =
		    GetFmodExSymbol("?addDSP@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAVDSP@2@PEAPEAVDSPConnection@2@@Z");
#else
		g_sym_FMOD_System_addDSP =
		    GetFmodExSymbol("?addDSP@System@FMOD@@QAG?AW4FMOD_RESULT@@PAVDSP@2@PAPAVDSPConnection@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(DSP*, DSPConnection**)>(
			   g_sym_FMOD_System_addDSP))(dsp, connection);
}

FMOD_RESULT F_API FMOD::System::lockDSP()
{
	if (!g_sym_FMOD_System_lockDSP)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_lockDSP = GetFmodExSymbol("?lockDSP@System@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_System_lockDSP = GetFmodExSymbol("?lockDSP@System@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)()>(g_sym_FMOD_System_lockDSP))();
}

FMOD_RESULT F_API FMOD::System::unlockDSP()
{
	if (!g_sym_FMOD_System_unlockDSP)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_unlockDSP = GetFmodExSymbol("?unlockDSP@System@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_System_unlockDSP = GetFmodExSymbol("?unlockDSP@System@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)()>(g_sym_FMOD_System_unlockDSP))();
}

FMOD_RESULT F_API FMOD::System::getDSPClock(unsigned int* hi, unsigned int* lo)
{
	if (!g_sym_FMOD_System_getDSPClock)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getDSPClock =
		    GetFmodExSymbol("?getDSPClock@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAI0@Z");
#else
		g_sym_FMOD_System_getDSPClock = GetFmodExSymbol("?getDSPClock@System@FMOD@@QAG?AW4FMOD_RESULT@@PAI0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(unsigned int*, unsigned int*)>(
			   g_sym_FMOD_System_getDSPClock))(hi, lo);
}

FMOD_RESULT F_API FMOD::System::setRecordDriver(int driver)
{
	if (!g_sym_FMOD_System_setRecordDriver)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setRecordDriver =
		    GetFmodExSymbol("?setRecordDriver@System@FMOD@@QEAA?AW4FMOD_RESULT@@H@Z");
#else
		g_sym_FMOD_System_setRecordDriver =
		    GetFmodExSymbol("?setRecordDriver@System@FMOD@@QAG?AW4FMOD_RESULT@@H@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int)>(g_sym_FMOD_System_setRecordDriver))(
	    driver);
}

FMOD_RESULT F_API FMOD::System::getRecordDriver(int* driver)
{
	if (!g_sym_FMOD_System_getRecordDriver)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getRecordDriver =
		    GetFmodExSymbol("?getRecordDriver@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_System_getRecordDriver =
		    GetFmodExSymbol("?getRecordDriver@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int*)>(g_sym_FMOD_System_getRecordDriver))(
	    driver);
}

FMOD_RESULT F_API FMOD::System::getRecordNumDrivers(int* numdrivers)
{
	if (!g_sym_FMOD_System_getRecordNumDrivers)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getRecordNumDrivers =
		    GetFmodExSymbol("?getRecordNumDrivers@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_System_getRecordNumDrivers =
		    GetFmodExSymbol("?getRecordNumDrivers@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int*)>(
			   g_sym_FMOD_System_getRecordNumDrivers))(numdrivers);
}

FMOD_RESULT F_API FMOD::System::getRecordDriverInfo(int id, char* name, int namelen, FMOD_GUID* guid)
{
	if (g_hasC1Fmod)
	{
		if (!g_sym_FMOD_System_getRecordDriverInfo)
		{
#ifdef BUILD_64BIT
			g_sym_FMOD_System_getRecordDriverInfo =
			    GetFmodExSymbol("?getRecordDriverName@System@FMOD@@QEAA?AW4FMOD_RESULT@@HPEADH@Z");
#else
			g_sym_FMOD_System_getRecordDriverInfo =
			    GetFmodExSymbol("?getRecordDriverName@System@FMOD@@QAG?AW4FMOD_RESULT@@HPADH@Z");
#endif
		}

		// no guid parameter
		return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int, char*, int)>(
				   g_sym_FMOD_System_getRecordDriverInfo))(id, name, namelen);
	}

	if (!g_sym_FMOD_System_getRecordDriverInfo)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getRecordDriverInfo =
		    GetFmodExSymbol("?getRecordDriverInfo@System@FMOD@@QEAA?AW4FMOD_RESULT@@HPEADHPEAUFMOD_GUID@@@Z");
#else
		g_sym_FMOD_System_getRecordDriverInfo =
		    GetFmodExSymbol("?getRecordDriverInfo@System@FMOD@@QAG?AW4FMOD_RESULT@@HPADHPAUFMOD_GUID@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int, char*, int, FMOD_GUID*)>(
			   g_sym_FMOD_System_getRecordDriverInfo))(id, name, namelen, guid);
}

FMOD_RESULT F_API FMOD::System::getRecordDriverCaps(int id, FMOD_CAPS* caps, int* minfrequency, int* maxfrequency)
{
	if (!g_sym_FMOD_System_getRecordDriverCaps)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getRecordDriverCaps =
		    GetFmodExSymbol("?getRecordDriverCaps@System@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAIPEAH1@Z");
#else
		g_sym_FMOD_System_getRecordDriverCaps =
		    GetFmodExSymbol("?getRecordDriverCaps@System@FMOD@@QAG?AW4FMOD_RESULT@@HPAIPAH1@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int, FMOD_CAPS*, int*, int*)>(
			   g_sym_FMOD_System_getRecordDriverCaps))(id, caps, minfrequency, maxfrequency);
}

FMOD_RESULT F_API FMOD::System::getRecordPosition(unsigned int* position)
{
	if (!g_sym_FMOD_System_getRecordPosition)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getRecordPosition =
		    GetFmodExSymbol("?getRecordPosition@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAI@Z");
#else
		g_sym_FMOD_System_getRecordPosition =
		    GetFmodExSymbol("?getRecordPosition@System@FMOD@@QAG?AW4FMOD_RESULT@@PAI@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(unsigned int*)>(
			   g_sym_FMOD_System_getRecordPosition))(position);
}

FMOD_RESULT F_API FMOD::System::recordStart(Sound* sound, bool loop)
{
	if (!g_sym_FMOD_System_recordStart)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_recordStart =
		    GetFmodExSymbol("?recordStart@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAVSound@2@_N@Z");
#else
		g_sym_FMOD_System_recordStart =
		    GetFmodExSymbol("?recordStart@System@FMOD@@QAG?AW4FMOD_RESULT@@PAVSound@2@_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(Sound*, bool)>(
			   g_sym_FMOD_System_recordStart))(sound, loop);
}

FMOD_RESULT F_API FMOD::System::recordStop()
{
	if (!g_sym_FMOD_System_recordStop)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_recordStop = GetFmodExSymbol("?recordStop@System@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_System_recordStop = GetFmodExSymbol("?recordStop@System@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)()>(g_sym_FMOD_System_recordStop))();
}

FMOD_RESULT F_API FMOD::System::isRecording(bool* recording)
{
	if (!g_sym_FMOD_System_isRecording)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_isRecording =
		    GetFmodExSymbol("?isRecording@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEA_N@Z");
#else
		g_sym_FMOD_System_isRecording = GetFmodExSymbol("?isRecording@System@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(bool*)>(g_sym_FMOD_System_isRecording))(
	    recording);
}

FMOD_RESULT F_API FMOD::System::createGeometry(int maxpolygons, int maxvertices, Geometry** geometry)
{
	if (!g_sym_FMOD_System_createGeometry)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_createGeometry =
		    GetFmodExSymbol("?createGeometry@System@FMOD@@QEAA?AW4FMOD_RESULT@@HHPEAPEAVGeometry@2@@Z");
#else
		g_sym_FMOD_System_createGeometry =
		    GetFmodExSymbol("?createGeometry@System@FMOD@@QAG?AW4FMOD_RESULT@@HHPAPAVGeometry@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int, int, Geometry**)>(
			   g_sym_FMOD_System_createGeometry))(maxpolygons, maxvertices, geometry);
}

FMOD_RESULT F_API FMOD::System::setGeometrySettings(float maxworldsize)
{
	if (!g_sym_FMOD_System_setGeometrySettings)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setGeometrySettings =
		    GetFmodExSymbol("?setGeometrySettings@System@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_System_setGeometrySettings =
		    GetFmodExSymbol("?setGeometrySettings@System@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(float)>(
			   g_sym_FMOD_System_setGeometrySettings))(maxworldsize);
}

FMOD_RESULT F_API FMOD::System::getGeometrySettings(float* maxworldsize)
{
	if (!g_sym_FMOD_System_getGeometrySettings)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getGeometrySettings =
		    GetFmodExSymbol("?getGeometrySettings@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_System_getGeometrySettings =
		    GetFmodExSymbol("?getGeometrySettings@System@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(float*)>(
			   g_sym_FMOD_System_getGeometrySettings))(maxworldsize);
}

FMOD_RESULT F_API FMOD::System::loadGeometry(const void* data, int datasize, Geometry** geometry)
{
	if (!g_sym_FMOD_System_loadGeometry)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_loadGeometry =
		    GetFmodExSymbol("?loadGeometry@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEBXHPEAPEAVGeometry@2@@Z");
#else
		g_sym_FMOD_System_loadGeometry =
		    GetFmodExSymbol("?loadGeometry@System@FMOD@@QAG?AW4FMOD_RESULT@@PBXHPAPAVGeometry@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(const void*, int, Geometry**)>(
			   g_sym_FMOD_System_loadGeometry))(data, datasize, geometry);
}

FMOD_RESULT F_API FMOD::System::setNetworkProxy(const char* proxy)
{
	if (!g_sym_FMOD_System_setNetworkProxy)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setNetworkProxy =
		    GetFmodExSymbol("?setNetworkProxy@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEBD@Z");
#else
		g_sym_FMOD_System_setNetworkProxy =
		    GetFmodExSymbol("?setNetworkProxy@System@FMOD@@QAG?AW4FMOD_RESULT@@PBD@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(const char*)>(
			   g_sym_FMOD_System_setNetworkProxy))(proxy);
}

FMOD_RESULT F_API FMOD::System::getNetworkProxy(char* proxy, int proxylen)
{
	if (!g_sym_FMOD_System_getNetworkProxy)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getNetworkProxy =
		    GetFmodExSymbol("?getNetworkProxy@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEADH@Z");
#else
		g_sym_FMOD_System_getNetworkProxy =
		    GetFmodExSymbol("?getNetworkProxy@System@FMOD@@QAG?AW4FMOD_RESULT@@PADH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(char*, int)>(
			   g_sym_FMOD_System_getNetworkProxy))(proxy, proxylen);
}

FMOD_RESULT F_API FMOD::System::setNetworkTimeout(int timeout)
{
	if (!g_sym_FMOD_System_setNetworkTimeout)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setNetworkTimeout =
		    GetFmodExSymbol("?setNetworkTimeout@System@FMOD@@QEAA?AW4FMOD_RESULT@@H@Z");
#else
		g_sym_FMOD_System_setNetworkTimeout =
		    GetFmodExSymbol("?setNetworkTimeout@System@FMOD@@QAG?AW4FMOD_RESULT@@H@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int)>(
			   g_sym_FMOD_System_setNetworkTimeout))(timeout);
}

FMOD_RESULT F_API FMOD::System::getNetworkTimeout(int* timeout)
{
	if (!g_sym_FMOD_System_getNetworkTimeout)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getNetworkTimeout =
		    GetFmodExSymbol("?getNetworkTimeout@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_System_getNetworkTimeout =
		    GetFmodExSymbol("?getNetworkTimeout@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(int*)>(
			   g_sym_FMOD_System_getNetworkTimeout))(timeout);
}

FMOD_RESULT F_API FMOD::System::setUserData(void* userdata)
{
	if (!g_sym_FMOD_System_setUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_setUserData =
		    GetFmodExSymbol("?setUserData@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAX@Z");
#else
		g_sym_FMOD_System_setUserData = GetFmodExSymbol("?setUserData@System@FMOD@@QAG?AW4FMOD_RESULT@@PAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(void*)>(g_sym_FMOD_System_setUserData))(
	    userdata);
}

FMOD_RESULT F_API FMOD::System::getUserData(void** userdata)
{
	if (!g_sym_FMOD_System_getUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_System_getUserData =
		    GetFmodExSymbol("?getUserData@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAX@Z");
#else
		g_sym_FMOD_System_getUserData =
		    GetFmodExSymbol("?getUserData@System@FMOD@@QAG?AW4FMOD_RESULT@@PAPAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::System::*&)(void**)>(g_sym_FMOD_System_getUserData))(
	    userdata);
}

FMOD_RESULT F_API FMOD::Sound::release()
{
	if (!g_sym_FMOD_Sound_release)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_release = GetFmodExSymbol("?release@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_Sound_release = GetFmodExSymbol("?release@Sound@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)()>(g_sym_FMOD_Sound_release))();
}

FMOD_RESULT F_API FMOD::Sound::getSystemObject(System** system)
{
	if (!g_sym_FMOD_Sound_getSystemObject)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getSystemObject =
		    GetFmodExSymbol("?getSystemObject@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVSystem@2@@Z");
#else
		g_sym_FMOD_Sound_getSystemObject =
		    GetFmodExSymbol("?getSystemObject@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVSystem@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(System**)>(
			   g_sym_FMOD_Sound_getSystemObject))(system);
}

FMOD_RESULT F_API FMOD::Sound::lock(unsigned int offset, unsigned int length, void** ptr1, void** ptr2,
                                    unsigned int* len1, unsigned int* len2)
{
	if (!g_sym_FMOD_Sound_lock)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_lock = GetFmodExSymbol("?lock@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@IIPEAPEAX0PEAI1@Z");
#else
		g_sym_FMOD_Sound_lock = GetFmodExSymbol("?lock@Sound@FMOD@@QAG?AW4FMOD_RESULT@@IIPAPAX0PAI1@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(unsigned int, unsigned int, void**, void**,
	                                                                    unsigned int*, unsigned int*)>(
			   g_sym_FMOD_Sound_lock))(offset, length, ptr1, ptr2, len1, len2);
}

FMOD_RESULT F_API FMOD::Sound::unlock(void* ptr1, void* ptr2, unsigned int len1, unsigned int len2)
{
	if (!g_sym_FMOD_Sound_unlock)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_unlock = GetFmodExSymbol("?unlock@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAX0II@Z");
#else
		g_sym_FMOD_Sound_unlock = GetFmodExSymbol("?unlock@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAX0II@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(void*, void*, unsigned int, unsigned int)>(
			   g_sym_FMOD_Sound_unlock))(ptr1, ptr2, len1, len2);
}

FMOD_RESULT F_API FMOD::Sound::setDefaults(float frequency, float volume, float pan, int priority)
{
	if (!g_sym_FMOD_Sound_setDefaults)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_setDefaults = GetFmodExSymbol("?setDefaults@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@MMMH@Z");
#else
		g_sym_FMOD_Sound_setDefaults = GetFmodExSymbol("?setDefaults@Sound@FMOD@@QAG?AW4FMOD_RESULT@@MMMH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(float, float, float, int)>(
			   g_sym_FMOD_Sound_setDefaults))(frequency, volume, pan, priority);
}

FMOD_RESULT F_API FMOD::Sound::getDefaults(float* frequency, float* volume, float* pan, int* priority)
{
	if (!g_sym_FMOD_Sound_getDefaults)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getDefaults =
		    GetFmodExSymbol("?getDefaults@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM00PEAH@Z");
#else
		g_sym_FMOD_Sound_getDefaults =
		    GetFmodExSymbol("?getDefaults@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAM00PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(float*, float*, float*, int*)>(
			   g_sym_FMOD_Sound_getDefaults))(frequency, volume, pan, priority);
}

FMOD_RESULT F_API FMOD::Sound::setVariations(float frequencyvar, float volumevar, float panvar)
{
	if (!g_sym_FMOD_Sound_setVariations)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_setVariations =
		    GetFmodExSymbol("?setVariations@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@MMM@Z");
#else
		g_sym_FMOD_Sound_setVariations =
		    GetFmodExSymbol("?setVariations@Sound@FMOD@@QAG?AW4FMOD_RESULT@@MMM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(float, float, float)>(
			   g_sym_FMOD_Sound_setVariations))(frequencyvar, volumevar, panvar);
}

FMOD_RESULT F_API FMOD::Sound::getVariations(float* frequencyvar, float* volumevar, float* panvar)
{
	if (!g_sym_FMOD_Sound_getVariations)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getVariations =
		    GetFmodExSymbol("?getVariations@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM00@Z");
#else
		g_sym_FMOD_Sound_getVariations =
		    GetFmodExSymbol("?getVariations@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAM00@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(float*, float*, float*)>(
			   g_sym_FMOD_Sound_getVariations))(frequencyvar, volumevar, panvar);
}

FMOD_RESULT F_API FMOD::Sound::set3DMinMaxDistance(float min, float max)
{
	if (!g_sym_FMOD_Sound_set3DMinMaxDistance)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_set3DMinMaxDistance =
		    GetFmodExSymbol("?set3DMinMaxDistance@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@MM@Z");
#else
		g_sym_FMOD_Sound_set3DMinMaxDistance =
		    GetFmodExSymbol("?set3DMinMaxDistance@Sound@FMOD@@QAG?AW4FMOD_RESULT@@MM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(float, float)>(
			   g_sym_FMOD_Sound_set3DMinMaxDistance))(min, max);
}

FMOD_RESULT F_API FMOD::Sound::get3DMinMaxDistance(float* min, float* max)
{
	if (!g_sym_FMOD_Sound_get3DMinMaxDistance)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_get3DMinMaxDistance =
		    GetFmodExSymbol("?get3DMinMaxDistance@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM0@Z");
#else
		g_sym_FMOD_Sound_get3DMinMaxDistance =
		    GetFmodExSymbol("?get3DMinMaxDistance@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAM0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(float*, float*)>(
			   g_sym_FMOD_Sound_get3DMinMaxDistance))(min, max);
}

FMOD_RESULT F_API FMOD::Sound::set3DConeSettings(float insideconeangle, float outsideconeangle, float outsidevolume)
{
	if (!g_sym_FMOD_Sound_set3DConeSettings)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_set3DConeSettings =
		    GetFmodExSymbol("?set3DConeSettings@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@MMM@Z");
#else
		g_sym_FMOD_Sound_set3DConeSettings =
		    GetFmodExSymbol("?set3DConeSettings@Sound@FMOD@@QAG?AW4FMOD_RESULT@@MMM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(float, float, float)>(
			   g_sym_FMOD_Sound_set3DConeSettings))(insideconeangle, outsideconeangle, outsidevolume);
}

FMOD_RESULT F_API FMOD::Sound::get3DConeSettings(float* insideconeangle, float* outsideconeangle, float* outsidevolume)
{
	if (!g_sym_FMOD_Sound_get3DConeSettings)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_get3DConeSettings =
		    GetFmodExSymbol("?get3DConeSettings@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM00@Z");
#else
		g_sym_FMOD_Sound_get3DConeSettings =
		    GetFmodExSymbol("?get3DConeSettings@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAM00@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(float*, float*, float*)>(
			   g_sym_FMOD_Sound_get3DConeSettings))(insideconeangle, outsideconeangle, outsidevolume);
}

FMOD_RESULT F_API FMOD::Sound::set3DCustomRolloff(FMOD_VECTOR* points, int numpoints)
{
	if (!g_sym_FMOD_Sound_set3DCustomRolloff)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_set3DCustomRolloff =
		    GetFmodExSymbol("?set3DCustomRolloff@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_VECTOR@@H@Z");
#else
		g_sym_FMOD_Sound_set3DCustomRolloff =
		    GetFmodExSymbol("?set3DCustomRolloff@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_VECTOR@@H@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(FMOD_VECTOR*, int)>(
			   g_sym_FMOD_Sound_set3DCustomRolloff))(points, numpoints);
}

FMOD_RESULT F_API FMOD::Sound::get3DCustomRolloff(FMOD_VECTOR** points, int* numpoints)
{
	if (!g_sym_FMOD_Sound_get3DCustomRolloff)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_get3DCustomRolloff =
		    GetFmodExSymbol("?get3DCustomRolloff@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAUFMOD_VECTOR@@PEAH@Z");
#else
		g_sym_FMOD_Sound_get3DCustomRolloff =
		    GetFmodExSymbol("?get3DCustomRolloff@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAPAUFMOD_VECTOR@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(FMOD_VECTOR**, int*)>(
			   g_sym_FMOD_Sound_get3DCustomRolloff))(points, numpoints);
}

FMOD_RESULT F_API FMOD::Sound::setSubSound(int index, Sound* subsound)
{
	if (!g_sym_FMOD_Sound_setSubSound)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_setSubSound =
		    GetFmodExSymbol("?setSubSound@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAV12@@Z");
#else
		g_sym_FMOD_Sound_setSubSound =
		    GetFmodExSymbol("?setSubSound@Sound@FMOD@@QAG?AW4FMOD_RESULT@@HPAV12@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(int, Sound*)>(
			   g_sym_FMOD_Sound_setSubSound))(index, subsound);
}

FMOD_RESULT F_API FMOD::Sound::getSubSound(int index, Sound** subsound)
{
	if (!g_sym_FMOD_Sound_getSubSound)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getSubSound =
		    GetFmodExSymbol("?getSubSound@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAPEAV12@@Z");
#else
		g_sym_FMOD_Sound_getSubSound =
		    GetFmodExSymbol("?getSubSound@Sound@FMOD@@QAG?AW4FMOD_RESULT@@HPAPAV12@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(int, Sound**)>(
			   g_sym_FMOD_Sound_getSubSound))(index, subsound);
}

FMOD_RESULT F_API FMOD::Sound::setSubSoundSentence(int* subsoundlist, int numsubsounds)
{
	if (!g_sym_FMOD_Sound_setSubSoundSentence)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_setSubSoundSentence =
		    GetFmodExSymbol("?setSubSoundSentence@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAHH@Z");
#else
		g_sym_FMOD_Sound_setSubSoundSentence =
		    GetFmodExSymbol("?setSubSoundSentence@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAHH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(int*, int)>(
			   g_sym_FMOD_Sound_setSubSoundSentence))(subsoundlist, numsubsounds);
}

FMOD_RESULT F_API FMOD::Sound::getName(char* name, int namelen)
{
	if (!g_sym_FMOD_Sound_getName)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getName = GetFmodExSymbol("?getName@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEADH@Z");
#else
		g_sym_FMOD_Sound_getName = GetFmodExSymbol("?getName@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PADH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(char*, int)>(g_sym_FMOD_Sound_getName))(
	    name, namelen);
}

FMOD_RESULT F_API FMOD::Sound::getLength(unsigned int* length, FMOD_TIMEUNIT lengthtype)
{
	if (!g_sym_FMOD_Sound_getLength)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getLength = GetFmodExSymbol("?getLength@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAII@Z");
#else
		g_sym_FMOD_Sound_getLength = GetFmodExSymbol("?getLength@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAII@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(unsigned int*, FMOD_TIMEUNIT)>(
			   g_sym_FMOD_Sound_getLength))(length, lengthtype);
}

FMOD_RESULT F_API FMOD::Sound::getFormat(FMOD_SOUND_TYPE* type, FMOD_SOUND_FORMAT* format, int* channels, int* bits)
{
	if (!g_sym_FMOD_Sound_getFormat)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getFormat = GetFmodExSymbol("?getFormat@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAW4FMOD_"
		                                             "SOUND_TYPE@@PEAW4FMOD_SOUND_FORMAT@@PEAH2@Z");
#else
		g_sym_FMOD_Sound_getFormat = GetFmodExSymbol(
		    "?getFormat@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAW4FMOD_SOUND_TYPE@@PAW4FMOD_SOUND_FORMAT@@PAH2@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(FMOD_SOUND_TYPE*, FMOD_SOUND_FORMAT*, int*,
	                                                                    int*)>(g_sym_FMOD_Sound_getFormat))(
	    type, format, channels, bits);
}

FMOD_RESULT F_API FMOD::Sound::getNumSubSounds(int* numsubsounds)
{
	if (!g_sym_FMOD_Sound_getNumSubSounds)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getNumSubSounds =
		    GetFmodExSymbol("?getNumSubSounds@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_Sound_getNumSubSounds =
		    GetFmodExSymbol("?getNumSubSounds@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(int*)>(g_sym_FMOD_Sound_getNumSubSounds))(
	    numsubsounds);
}

FMOD_RESULT F_API FMOD::Sound::getNumTags(int* numtags, int* numtagsupdated)
{
	if (!g_sym_FMOD_Sound_getNumTags)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getNumTags = GetFmodExSymbol("?getNumTags@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH0@Z");
#else
		g_sym_FMOD_Sound_getNumTags = GetFmodExSymbol("?getNumTags@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAH0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(int*, int*)>(g_sym_FMOD_Sound_getNumTags))(
	    numtags, numtagsupdated);
}

FMOD_RESULT F_API FMOD::Sound::getTag(const char* name, int index, FMOD_TAG* tag)
{
	if (!g_sym_FMOD_Sound_getTag)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getTag =
		    GetFmodExSymbol("?getTag@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEBDHPEAUFMOD_TAG@@@Z");
#else
		g_sym_FMOD_Sound_getTag =
		    GetFmodExSymbol("?getTag@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PBDHPAUFMOD_TAG@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(const char*, int, FMOD_TAG*)>(
			   g_sym_FMOD_Sound_getTag))(name, index, tag);
}

FMOD_RESULT F_API FMOD::Sound::getOpenState(FMOD_OPENSTATE* openstate, unsigned int* percentbuffered, bool* starving)
{
	if (!g_sym_FMOD_Sound_getOpenState)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getOpenState =
		    GetFmodExSymbol("?getOpenState@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAW4FMOD_OPENSTATE@@PEAIPEA_N@Z");
#else
		g_sym_FMOD_Sound_getOpenState =
		    GetFmodExSymbol("?getOpenState@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAW4FMOD_OPENSTATE@@PAIPA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(FMOD_OPENSTATE*, unsigned int*, bool*)>(
			   g_sym_FMOD_Sound_getOpenState))(openstate, percentbuffered, starving);
}

FMOD_RESULT F_API FMOD::Sound::readData(void* buffer, unsigned int lenbytes, unsigned int* read)
{
	if (!g_sym_FMOD_Sound_readData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_readData = GetFmodExSymbol("?readData@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAXIPEAI@Z");
#else
		g_sym_FMOD_Sound_readData = GetFmodExSymbol("?readData@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAXIPAI@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(void*, unsigned int, unsigned int*)>(
			   g_sym_FMOD_Sound_readData))(buffer, lenbytes, read);
}

FMOD_RESULT F_API FMOD::Sound::seekData(unsigned int pcm)
{
	if (!g_sym_FMOD_Sound_seekData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_seekData = GetFmodExSymbol("?seekData@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@I@Z");
#else
		g_sym_FMOD_Sound_seekData = GetFmodExSymbol("?seekData@Sound@FMOD@@QAG?AW4FMOD_RESULT@@I@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(unsigned int)>(g_sym_FMOD_Sound_seekData))(
	    pcm);
}

FMOD_RESULT F_API FMOD::Sound::setSoundGroup(SoundGroup* soundgroup)
{
	if (!g_sym_FMOD_Sound_setSoundGroup)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_setSoundGroup =
		    GetFmodExSymbol("?setSoundGroup@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAVSoundGroup@2@@Z");
#else
		g_sym_FMOD_Sound_setSoundGroup =
		    GetFmodExSymbol("?setSoundGroup@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAVSoundGroup@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(SoundGroup*)>(
			   g_sym_FMOD_Sound_setSoundGroup))(soundgroup);
}

FMOD_RESULT F_API FMOD::Sound::getSoundGroup(SoundGroup** soundgroup)
{
	if (!g_sym_FMOD_Sound_getSoundGroup)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getSoundGroup =
		    GetFmodExSymbol("?getSoundGroup@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVSoundGroup@2@@Z");
#else
		g_sym_FMOD_Sound_getSoundGroup =
		    GetFmodExSymbol("?getSoundGroup@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVSoundGroup@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(SoundGroup**)>(
			   g_sym_FMOD_Sound_getSoundGroup))(soundgroup);
}

FMOD_RESULT F_API FMOD::Sound::getNumSyncPoints(int* numsyncpoints)
{
	if (!g_sym_FMOD_Sound_getNumSyncPoints)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getNumSyncPoints =
		    GetFmodExSymbol("?getNumSyncPoints@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_Sound_getNumSyncPoints =
		    GetFmodExSymbol("?getNumSyncPoints@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(int*)>(g_sym_FMOD_Sound_getNumSyncPoints))(
	    numsyncpoints);
}

FMOD_RESULT F_API FMOD::Sound::getSyncPoint(int index, FMOD_SYNCPOINT** point)
{
	if (!g_sym_FMOD_Sound_getSyncPoint)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getSyncPoint =
		    GetFmodExSymbol("?getSyncPoint@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAPEAUFMOD_SYNCPOINT@@@Z");
#else
		g_sym_FMOD_Sound_getSyncPoint =
		    GetFmodExSymbol("?getSyncPoint@Sound@FMOD@@QAG?AW4FMOD_RESULT@@HPAPAUFMOD_SYNCPOINT@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(int, FMOD_SYNCPOINT**)>(
			   g_sym_FMOD_Sound_getSyncPoint))(index, point);
}

FMOD_RESULT F_API FMOD::Sound::getSyncPointInfo(FMOD_SYNCPOINT* point, char* name, int namelen, unsigned int* offset,
                                                FMOD_TIMEUNIT offsettype)
{
	if (!g_sym_FMOD_Sound_getSyncPointInfo)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getSyncPointInfo = GetFmodExSymbol(
		    "?getSyncPointInfo@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_SYNCPOINT@@PEADHPEAII@Z");
#else
		g_sym_FMOD_Sound_getSyncPointInfo =
		    GetFmodExSymbol("?getSyncPointInfo@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_SYNCPOINT@@PADHPAII@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(FMOD_SYNCPOINT*, char*, int, unsigned int*,
	                                                                    FMOD_TIMEUNIT)>(
			   g_sym_FMOD_Sound_getSyncPointInfo))(point, name, namelen, offset, offsettype);
}

FMOD_RESULT F_API FMOD::Sound::addSyncPoint(unsigned int offset, FMOD_TIMEUNIT offsettype, const char* name,
                                            FMOD_SYNCPOINT** point)
{
	if (!g_sym_FMOD_Sound_addSyncPoint)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_addSyncPoint =
		    GetFmodExSymbol("?addSyncPoint@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@IIPEBDPEAPEAUFMOD_SYNCPOINT@@@Z");
#else
		g_sym_FMOD_Sound_addSyncPoint =
		    GetFmodExSymbol("?addSyncPoint@Sound@FMOD@@QAG?AW4FMOD_RESULT@@IIPBDPAPAUFMOD_SYNCPOINT@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(
			   unsigned int, FMOD_TIMEUNIT, const char*, FMOD_SYNCPOINT**)>(g_sym_FMOD_Sound_addSyncPoint))(
	    offset, offsettype, name, point);
}

FMOD_RESULT F_API FMOD::Sound::deleteSyncPoint(FMOD_SYNCPOINT* point)
{
	if (!g_sym_FMOD_Sound_deleteSyncPoint)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_deleteSyncPoint =
		    GetFmodExSymbol("?deleteSyncPoint@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_SYNCPOINT@@@Z");
#else
		g_sym_FMOD_Sound_deleteSyncPoint =
		    GetFmodExSymbol("?deleteSyncPoint@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_SYNCPOINT@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(FMOD_SYNCPOINT*)>(
			   g_sym_FMOD_Sound_deleteSyncPoint))(point);
}

FMOD_RESULT F_API FMOD::Sound::setMode(FMOD_MODE mode)
{
	if (!g_sym_FMOD_Sound_setMode)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_setMode = GetFmodExSymbol("?setMode@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@I@Z");
#else
		g_sym_FMOD_Sound_setMode = GetFmodExSymbol("?setMode@Sound@FMOD@@QAG?AW4FMOD_RESULT@@I@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(FMOD_MODE)>(g_sym_FMOD_Sound_setMode))(
	    mode);
}

FMOD_RESULT F_API FMOD::Sound::getMode(FMOD_MODE* mode)
{
	if (!g_sym_FMOD_Sound_getMode)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getMode = GetFmodExSymbol("?getMode@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAI@Z");
#else
		g_sym_FMOD_Sound_getMode = GetFmodExSymbol("?getMode@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAI@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(FMOD_MODE*)>(g_sym_FMOD_Sound_getMode))(
	    mode);
}

FMOD_RESULT F_API FMOD::Sound::setLoopCount(int loopcount)
{
	if (!g_sym_FMOD_Sound_setLoopCount)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_setLoopCount = GetFmodExSymbol("?setLoopCount@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@H@Z");
#else
		g_sym_FMOD_Sound_setLoopCount = GetFmodExSymbol("?setLoopCount@Sound@FMOD@@QAG?AW4FMOD_RESULT@@H@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(int)>(g_sym_FMOD_Sound_setLoopCount))(
	    loopcount);
}

FMOD_RESULT F_API FMOD::Sound::getLoopCount(int* loopcount)
{
	if (!g_sym_FMOD_Sound_getLoopCount)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getLoopCount =
		    GetFmodExSymbol("?getLoopCount@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_Sound_getLoopCount = GetFmodExSymbol("?getLoopCount@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(int*)>(g_sym_FMOD_Sound_getLoopCount))(
	    loopcount);
}

FMOD_RESULT F_API FMOD::Sound::setLoopPoints(unsigned int loopstart, FMOD_TIMEUNIT loopstarttype, unsigned int loopend,
                                             FMOD_TIMEUNIT loopendtype)
{
	if (!g_sym_FMOD_Sound_setLoopPoints)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_setLoopPoints =
		    GetFmodExSymbol("?setLoopPoints@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@IIII@Z");
#else
		g_sym_FMOD_Sound_setLoopPoints =
		    GetFmodExSymbol("?setLoopPoints@Sound@FMOD@@QAG?AW4FMOD_RESULT@@IIII@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(
			   unsigned int, FMOD_TIMEUNIT, unsigned int, FMOD_TIMEUNIT)>(g_sym_FMOD_Sound_setLoopPoints))(
	    loopstart, loopstarttype, loopend, loopendtype);
}

FMOD_RESULT F_API FMOD::Sound::getLoopPoints(unsigned int* loopstart, FMOD_TIMEUNIT loopstarttype,
                                             unsigned int* loopend, FMOD_TIMEUNIT loopendtype)
{
	if (!g_sym_FMOD_Sound_getLoopPoints)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getLoopPoints =
		    GetFmodExSymbol("?getLoopPoints@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAII0I@Z");
#else
		g_sym_FMOD_Sound_getLoopPoints =
		    GetFmodExSymbol("?getLoopPoints@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAII0I@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(unsigned int*, FMOD_TIMEUNIT, unsigned int*,
	                                                                    FMOD_TIMEUNIT)>(
			   g_sym_FMOD_Sound_getLoopPoints))(loopstart, loopstarttype, loopend, loopendtype);
}

FMOD_RESULT F_API FMOD::Sound::getMusicNumChannels(int* numchannels)
{
	if (!g_sym_FMOD_Sound_getMusicNumChannels)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getMusicNumChannels =
		    GetFmodExSymbol("?getMusicNumChannels@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_Sound_getMusicNumChannels =
		    GetFmodExSymbol("?getMusicNumChannels@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(int*)>(
			   g_sym_FMOD_Sound_getMusicNumChannels))(numchannels);
}

FMOD_RESULT F_API FMOD::Sound::setMusicChannelVolume(int channel, float volume)
{
	if (!g_sym_FMOD_Sound_setMusicChannelVolume)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_setMusicChannelVolume =
		    GetFmodExSymbol("?setMusicChannelVolume@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@HM@Z");
#else
		g_sym_FMOD_Sound_setMusicChannelVolume =
		    GetFmodExSymbol("?setMusicChannelVolume@Sound@FMOD@@QAG?AW4FMOD_RESULT@@HM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(int, float)>(
			   g_sym_FMOD_Sound_setMusicChannelVolume))(channel, volume);
}

FMOD_RESULT F_API FMOD::Sound::getMusicChannelVolume(int channel, float* volume)
{
	if (!g_sym_FMOD_Sound_getMusicChannelVolume)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getMusicChannelVolume =
		    GetFmodExSymbol("?getMusicChannelVolume@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAM@Z");
#else
		g_sym_FMOD_Sound_getMusicChannelVolume =
		    GetFmodExSymbol("?getMusicChannelVolume@Sound@FMOD@@QAG?AW4FMOD_RESULT@@HPAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(int, float*)>(
			   g_sym_FMOD_Sound_getMusicChannelVolume))(channel, volume);
}

FMOD_RESULT F_API FMOD::Sound::setUserData(void* userdata)
{
	if (!g_sym_FMOD_Sound_setUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_setUserData = GetFmodExSymbol("?setUserData@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAX@Z");
#else
		g_sym_FMOD_Sound_setUserData = GetFmodExSymbol("?setUserData@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(void*)>(g_sym_FMOD_Sound_setUserData))(
	    userdata);
}

FMOD_RESULT F_API FMOD::Sound::getUserData(void** userdata)
{
	if (!g_sym_FMOD_Sound_getUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Sound_getUserData =
		    GetFmodExSymbol("?getUserData@Sound@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAX@Z");
#else
		g_sym_FMOD_Sound_getUserData = GetFmodExSymbol("?getUserData@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAPAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Sound::*&)(void**)>(g_sym_FMOD_Sound_getUserData))(
	    userdata);
}

FMOD_RESULT F_API FMOD::Channel::getSystemObject(System** system)
{
	if (!g_sym_FMOD_Channel_getSystemObject)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getSystemObject =
		    GetFmodExSymbol("?getSystemObject@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVSystem@2@@Z");
#else
		g_sym_FMOD_Channel_getSystemObject =
		    GetFmodExSymbol("?getSystemObject@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVSystem@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(System**)>(
			   g_sym_FMOD_Channel_getSystemObject))(system);
}

FMOD_RESULT F_API FMOD::Channel::stop()
{
	if (!g_sym_FMOD_Channel_stop)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_stop = GetFmodExSymbol("?stop@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_Channel_stop = GetFmodExSymbol("?stop@Channel@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)()>(g_sym_FMOD_Channel_stop))();
}

FMOD_RESULT F_API FMOD::Channel::setPaused(bool paused)
{
	if (!g_sym_FMOD_Channel_setPaused)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setPaused = GetFmodExSymbol("?setPaused@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@_N@Z");
#else
		g_sym_FMOD_Channel_setPaused = GetFmodExSymbol("?setPaused@Channel@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(bool)>(g_sym_FMOD_Channel_setPaused))(
	    paused);
}

FMOD_RESULT F_API FMOD::Channel::getPaused(bool* paused)
{
	if (!g_sym_FMOD_Channel_getPaused)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getPaused = GetFmodExSymbol("?getPaused@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEA_N@Z");
#else
		g_sym_FMOD_Channel_getPaused = GetFmodExSymbol("?getPaused@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(bool*)>(g_sym_FMOD_Channel_getPaused))(
	    paused);
}

FMOD_RESULT F_API FMOD::Channel::setVolume(float volume)
{
	if (!g_sym_FMOD_Channel_setVolume)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setVolume = GetFmodExSymbol("?setVolume@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_Channel_setVolume = GetFmodExSymbol("?setVolume@Channel@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float)>(g_sym_FMOD_Channel_setVolume))(
	    volume);
}

FMOD_RESULT F_API FMOD::Channel::getVolume(float* volume)
{
	if (!g_sym_FMOD_Channel_getVolume)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getVolume = GetFmodExSymbol("?getVolume@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_Channel_getVolume = GetFmodExSymbol("?getVolume@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*)>(g_sym_FMOD_Channel_getVolume))(
	    volume);
}

FMOD_RESULT F_API FMOD::Channel::setFrequency(float frequency)
{
	if (!g_sym_FMOD_Channel_setFrequency)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setFrequency =
		    GetFmodExSymbol("?setFrequency@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_Channel_setFrequency =
		    GetFmodExSymbol("?setFrequency@Channel@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float)>(g_sym_FMOD_Channel_setFrequency))(
	    frequency);
}

FMOD_RESULT F_API FMOD::Channel::getFrequency(float* frequency)
{
	if (!g_sym_FMOD_Channel_getFrequency)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getFrequency =
		    GetFmodExSymbol("?getFrequency@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_Channel_getFrequency =
		    GetFmodExSymbol("?getFrequency@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*)>(
			   g_sym_FMOD_Channel_getFrequency))(frequency);
}

FMOD_RESULT F_API FMOD::Channel::setPan(float pan)
{
	if (!g_sym_FMOD_Channel_setPan)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setPan = GetFmodExSymbol("?setPan@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_Channel_setPan = GetFmodExSymbol("?setPan@Channel@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float)>(g_sym_FMOD_Channel_setPan))(pan);
}

FMOD_RESULT F_API FMOD::Channel::getPan(float* pan)
{
	if (!g_sym_FMOD_Channel_getPan)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getPan = GetFmodExSymbol("?getPan@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_Channel_getPan = GetFmodExSymbol("?getPan@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*)>(g_sym_FMOD_Channel_getPan))(pan);
}

FMOD_RESULT F_API FMOD::Channel::setDelay(FMOD_DELAYTYPE delaytype, unsigned int delayhi, unsigned int delaylo)
{
	if (!g_sym_FMOD_Channel_setDelay)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setDelay =
		    GetFmodExSymbol("?setDelay@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_DELAYTYPE@@II@Z");
#else
		g_sym_FMOD_Channel_setDelay =
		    GetFmodExSymbol("?setDelay@Channel@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_DELAYTYPE@@II@Z");
#endif
	}

	return (
	    this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(FMOD_DELAYTYPE, unsigned int, unsigned int)>(
		       g_sym_FMOD_Channel_setDelay))(delaytype, delayhi, delaylo);
}

FMOD_RESULT F_API FMOD::Channel::getDelay(FMOD_DELAYTYPE delaytype, unsigned int* delayhi, unsigned int* delaylo)
{
	if (!g_sym_FMOD_Channel_getDelay)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getDelay =
		    GetFmodExSymbol("?getDelay@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_DELAYTYPE@@PEAI1@Z");
#else
		g_sym_FMOD_Channel_getDelay =
		    GetFmodExSymbol("?getDelay@Channel@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_DELAYTYPE@@PAI1@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(
			   FMOD_DELAYTYPE, unsigned int*, unsigned int*)>(g_sym_FMOD_Channel_getDelay))(
	    delaytype, delayhi, delaylo);
}

FMOD_RESULT F_API FMOD::Channel::setSpeakerMix(float frontleft, float frontright, float center, float lfe,
                                               float backleft, float backright, float sideleft, float sideright)
{
	if (!g_sym_FMOD_Channel_setSpeakerMix)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setSpeakerMix =
		    GetFmodExSymbol("?setSpeakerMix@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@MMMMMMMM@Z");
#else
		g_sym_FMOD_Channel_setSpeakerMix =
		    GetFmodExSymbol("?setSpeakerMix@Channel@FMOD@@QAG?AW4FMOD_RESULT@@MMMMMMMM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(
			   float, float, float, float, float, float, float, float)>(g_sym_FMOD_Channel_setSpeakerMix))(
	    frontleft, frontright, center, lfe, backleft, backright, sideleft, sideright);
}

FMOD_RESULT F_API FMOD::Channel::getSpeakerMix(float* frontleft, float* frontright, float* center, float* lfe,
                                               float* backleft, float* backright, float* sideleft, float* sideright)
{
	if (!g_sym_FMOD_Channel_getSpeakerMix)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getSpeakerMix =
		    GetFmodExSymbol("?getSpeakerMix@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM0000000@Z");
#else
		g_sym_FMOD_Channel_getSpeakerMix =
		    GetFmodExSymbol("?getSpeakerMix@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAM0000000@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*, float*, float*, float*, float*,
	                                                                      float*, float*, float*)>(
			   g_sym_FMOD_Channel_getSpeakerMix))(frontleft, frontright, center, lfe, backleft, backright,
	                                                      sideleft, sideright);
}

FMOD_RESULT F_API FMOD::Channel::setSpeakerLevels(FMOD_SPEAKER speaker, float* levels, int numlevels)
{
	if (!g_sym_FMOD_Channel_setSpeakerLevels)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setSpeakerLevels =
		    GetFmodExSymbol("?setSpeakerLevels@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_SPEAKER@@PEAMH@Z");
#else
		g_sym_FMOD_Channel_setSpeakerLevels =
		    GetFmodExSymbol("?setSpeakerLevels@Channel@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_SPEAKER@@PAMH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(FMOD_SPEAKER, float*, int)>(
			   g_sym_FMOD_Channel_setSpeakerLevels))(speaker, levels, numlevels);
}

FMOD_RESULT F_API FMOD::Channel::getSpeakerLevels(FMOD_SPEAKER speaker, float* levels, int numlevels)
{
	if (!g_sym_FMOD_Channel_getSpeakerLevels)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getSpeakerLevels =
		    GetFmodExSymbol("?getSpeakerLevels@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_SPEAKER@@PEAMH@Z");
#else
		g_sym_FMOD_Channel_getSpeakerLevels =
		    GetFmodExSymbol("?getSpeakerLevels@Channel@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_SPEAKER@@PAMH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(FMOD_SPEAKER, float*, int)>(
			   g_sym_FMOD_Channel_getSpeakerLevels))(speaker, levels, numlevels);
}

FMOD_RESULT F_API FMOD::Channel::setInputChannelMix(float* levels, int numlevels)
{
	if (!g_sym_FMOD_Channel_setInputChannelMix)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setInputChannelMix =
		    GetFmodExSymbol("?setInputChannelMix@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAMH@Z");
#else
		g_sym_FMOD_Channel_setInputChannelMix =
		    GetFmodExSymbol("?setInputChannelMix@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAMH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*, int)>(
			   g_sym_FMOD_Channel_setInputChannelMix))(levels, numlevels);
}

FMOD_RESULT F_API FMOD::Channel::getInputChannelMix(float* levels, int numlevels)
{
	if (!g_sym_FMOD_Channel_getInputChannelMix)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getInputChannelMix =
		    GetFmodExSymbol("?getInputChannelMix@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAMH@Z");
#else
		g_sym_FMOD_Channel_getInputChannelMix =
		    GetFmodExSymbol("?getInputChannelMix@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAMH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*, int)>(
			   g_sym_FMOD_Channel_getInputChannelMix))(levels, numlevels);
}

FMOD_RESULT F_API FMOD::Channel::setMute(bool mute)
{
	if (!g_sym_FMOD_Channel_setMute)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setMute = GetFmodExSymbol("?setMute@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@_N@Z");
#else
		g_sym_FMOD_Channel_setMute = GetFmodExSymbol("?setMute@Channel@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(bool)>(g_sym_FMOD_Channel_setMute))(mute);
}

FMOD_RESULT F_API FMOD::Channel::getMute(bool* mute)
{
	if (!g_sym_FMOD_Channel_getMute)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getMute = GetFmodExSymbol("?getMute@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEA_N@Z");
#else
		g_sym_FMOD_Channel_getMute = GetFmodExSymbol("?getMute@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(bool*)>(g_sym_FMOD_Channel_getMute))(
	    mute);
}

FMOD_RESULT F_API FMOD::Channel::setPriority(int priority)
{
	if (!g_sym_FMOD_Channel_setPriority)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setPriority = GetFmodExSymbol("?setPriority@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@H@Z");
#else
		g_sym_FMOD_Channel_setPriority = GetFmodExSymbol("?setPriority@Channel@FMOD@@QAG?AW4FMOD_RESULT@@H@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(int)>(g_sym_FMOD_Channel_setPriority))(
	    priority);
}

FMOD_RESULT F_API FMOD::Channel::getPriority(int* priority)
{
	if (!g_sym_FMOD_Channel_getPriority)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getPriority =
		    GetFmodExSymbol("?getPriority@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_Channel_getPriority =
		    GetFmodExSymbol("?getPriority@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(int*)>(g_sym_FMOD_Channel_getPriority))(
	    priority);
}

FMOD_RESULT F_API FMOD::Channel::setPosition(unsigned int position, FMOD_TIMEUNIT postype)
{
	if (!g_sym_FMOD_Channel_setPosition)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setPosition =
		    GetFmodExSymbol("?setPosition@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@II@Z");
#else
		g_sym_FMOD_Channel_setPosition = GetFmodExSymbol("?setPosition@Channel@FMOD@@QAG?AW4FMOD_RESULT@@II@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(unsigned int, FMOD_TIMEUNIT)>(
			   g_sym_FMOD_Channel_setPosition))(position, postype);
}

FMOD_RESULT F_API FMOD::Channel::getPosition(unsigned int* position, FMOD_TIMEUNIT postype)
{
	if (!g_sym_FMOD_Channel_getPosition)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getPosition =
		    GetFmodExSymbol("?getPosition@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAII@Z");
#else
		g_sym_FMOD_Channel_getPosition =
		    GetFmodExSymbol("?getPosition@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAII@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(unsigned int*, FMOD_TIMEUNIT)>(
			   g_sym_FMOD_Channel_getPosition))(position, postype);
}

FMOD_RESULT F_API FMOD::Channel::setReverbProperties(const FMOD_REVERB_CHANNELPROPERTIES* prop)
{
	if (!g_sym_FMOD_Channel_setReverbProperties)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setReverbProperties = GetFmodExSymbol(
		    "?setReverbProperties@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEBUFMOD_REVERB_CHANNELPROPERTIES@@@Z");
#else
		g_sym_FMOD_Channel_setReverbProperties = GetFmodExSymbol(
		    "?setReverbProperties@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_REVERB_CHANNELPROPERTIES@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(const FMOD_REVERB_CHANNELPROPERTIES*)>(
			   g_sym_FMOD_Channel_setReverbProperties))(prop);
}

FMOD_RESULT F_API FMOD::Channel::getReverbProperties(FMOD_REVERB_CHANNELPROPERTIES* prop)
{
	if (!g_sym_FMOD_Channel_getReverbProperties)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getReverbProperties = GetFmodExSymbol(
		    "?getReverbProperties@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_REVERB_CHANNELPROPERTIES@@@Z");
#else
		g_sym_FMOD_Channel_getReverbProperties = GetFmodExSymbol(
		    "?getReverbProperties@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_REVERB_CHANNELPROPERTIES@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(FMOD_REVERB_CHANNELPROPERTIES*)>(
			   g_sym_FMOD_Channel_getReverbProperties))(prop);
}

FMOD_RESULT F_API FMOD::Channel::setChannelGroup(ChannelGroup* channelgroup)
{
	if (!g_sym_FMOD_Channel_setChannelGroup)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setChannelGroup =
		    GetFmodExSymbol("?setChannelGroup@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAVChannelGroup@2@@Z");
#else
		g_sym_FMOD_Channel_setChannelGroup =
		    GetFmodExSymbol("?setChannelGroup@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAVChannelGroup@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(ChannelGroup*)>(
			   g_sym_FMOD_Channel_setChannelGroup))(channelgroup);
}

FMOD_RESULT F_API FMOD::Channel::getChannelGroup(ChannelGroup** channelgroup)
{
	if (!g_sym_FMOD_Channel_getChannelGroup)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getChannelGroup =
		    GetFmodExSymbol("?getChannelGroup@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVChannelGroup@2@@Z");
#else
		g_sym_FMOD_Channel_getChannelGroup =
		    GetFmodExSymbol("?getChannelGroup@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVChannelGroup@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(ChannelGroup**)>(
			   g_sym_FMOD_Channel_getChannelGroup))(channelgroup);
}

FMOD_RESULT F_API FMOD::Channel::setCallback(FMOD_CHANNEL_CALLBACKTYPE type, FMOD_CHANNEL_CALLBACK callback,
                                             int command)
{
	if (!g_sym_FMOD_Channel_setCallback)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setCallback =
		    GetFmodExSymbol("?setCallback@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_CHANNEL_CALLBACKTYPE@@P6A?"
		                    "AW43@PEAUFMOD_CHANNEL@@0HII@ZH@Z");
#else
		g_sym_FMOD_Channel_setCallback =
		    GetFmodExSymbol("?setCallback@Channel@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_CHANNEL_CALLBACKTYPE@@P6G?"
		                    "AW43@PAUFMOD_CHANNEL@@0HII@ZH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(
			   FMOD_CHANNEL_CALLBACKTYPE, FMOD_CHANNEL_CALLBACK, int)>(g_sym_FMOD_Channel_setCallback))(
	    type, callback, command);
}

FMOD_RESULT F_API FMOD::Channel::set3DAttributes(const FMOD_VECTOR* pos, const FMOD_VECTOR* vel)
{
	if (!g_sym_FMOD_Channel_set3DAttributes)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_set3DAttributes =
		    GetFmodExSymbol("?set3DAttributes@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEBUFMOD_VECTOR@@0@Z");
#else
		g_sym_FMOD_Channel_set3DAttributes =
		    GetFmodExSymbol("?set3DAttributes@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_VECTOR@@0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(const FMOD_VECTOR*, const FMOD_VECTOR*)>(
			   g_sym_FMOD_Channel_set3DAttributes))(pos, vel);
}

FMOD_RESULT F_API FMOD::Channel::get3DAttributes(FMOD_VECTOR* pos, FMOD_VECTOR* vel)
{
	if (!g_sym_FMOD_Channel_get3DAttributes)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_get3DAttributes =
		    GetFmodExSymbol("?get3DAttributes@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_VECTOR@@0@Z");
#else
		g_sym_FMOD_Channel_get3DAttributes =
		    GetFmodExSymbol("?get3DAttributes@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_VECTOR@@0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(FMOD_VECTOR*, FMOD_VECTOR*)>(
			   g_sym_FMOD_Channel_get3DAttributes))(pos, vel);
}

FMOD_RESULT F_API FMOD::Channel::set3DMinMaxDistance(float mindistance, float maxdistance)
{
	if (!g_sym_FMOD_Channel_set3DMinMaxDistance)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_set3DMinMaxDistance =
		    GetFmodExSymbol("?set3DMinMaxDistance@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@MM@Z");
#else
		g_sym_FMOD_Channel_set3DMinMaxDistance =
		    GetFmodExSymbol("?set3DMinMaxDistance@Channel@FMOD@@QAG?AW4FMOD_RESULT@@MM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float, float)>(
			   g_sym_FMOD_Channel_set3DMinMaxDistance))(mindistance, maxdistance);
}

FMOD_RESULT F_API FMOD::Channel::get3DMinMaxDistance(float* mindistance, float* maxdistance)
{
	if (!g_sym_FMOD_Channel_get3DMinMaxDistance)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_get3DMinMaxDistance =
		    GetFmodExSymbol("?get3DMinMaxDistance@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM0@Z");
#else
		g_sym_FMOD_Channel_get3DMinMaxDistance =
		    GetFmodExSymbol("?get3DMinMaxDistance@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAM0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*, float*)>(
			   g_sym_FMOD_Channel_get3DMinMaxDistance))(mindistance, maxdistance);
}

FMOD_RESULT F_API FMOD::Channel::set3DConeSettings(float insideconeangle, float outsideconeangle, float outsidevolume)
{
	if (!g_sym_FMOD_Channel_set3DConeSettings)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_set3DConeSettings =
		    GetFmodExSymbol("?set3DConeSettings@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@MMM@Z");
#else
		g_sym_FMOD_Channel_set3DConeSettings =
		    GetFmodExSymbol("?set3DConeSettings@Channel@FMOD@@QAG?AW4FMOD_RESULT@@MMM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float, float, float)>(
			   g_sym_FMOD_Channel_set3DConeSettings))(insideconeangle, outsideconeangle, outsidevolume);
}

FMOD_RESULT F_API FMOD::Channel::get3DConeSettings(float* insideconeangle, float* outsideconeangle,
                                                   float* outsidevolume)
{
	if (!g_sym_FMOD_Channel_get3DConeSettings)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_get3DConeSettings =
		    GetFmodExSymbol("?get3DConeSettings@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM00@Z");
#else
		g_sym_FMOD_Channel_get3DConeSettings =
		    GetFmodExSymbol("?get3DConeSettings@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAM00@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*, float*, float*)>(
			   g_sym_FMOD_Channel_get3DConeSettings))(insideconeangle, outsideconeangle, outsidevolume);
}

FMOD_RESULT F_API FMOD::Channel::set3DConeOrientation(FMOD_VECTOR* orientation)
{
	if (!g_sym_FMOD_Channel_set3DConeOrientation)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_set3DConeOrientation =
		    GetFmodExSymbol("?set3DConeOrientation@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_VECTOR@@@Z");
#else
		g_sym_FMOD_Channel_set3DConeOrientation =
		    GetFmodExSymbol("?set3DConeOrientation@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_VECTOR@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(FMOD_VECTOR*)>(
			   g_sym_FMOD_Channel_set3DConeOrientation))(orientation);
}

FMOD_RESULT F_API FMOD::Channel::get3DConeOrientation(FMOD_VECTOR* orientation)
{
	if (!g_sym_FMOD_Channel_get3DConeOrientation)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_get3DConeOrientation =
		    GetFmodExSymbol("?get3DConeOrientation@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_VECTOR@@@Z");
#else
		g_sym_FMOD_Channel_get3DConeOrientation =
		    GetFmodExSymbol("?get3DConeOrientation@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_VECTOR@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(FMOD_VECTOR*)>(
			   g_sym_FMOD_Channel_get3DConeOrientation))(orientation);
}

FMOD_RESULT F_API FMOD::Channel::set3DCustomRolloff(FMOD_VECTOR* points, int numpoints)
{
	if (!g_sym_FMOD_Channel_set3DCustomRolloff)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_set3DCustomRolloff =
		    GetFmodExSymbol("?set3DCustomRolloff@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_VECTOR@@H@Z");
#else
		g_sym_FMOD_Channel_set3DCustomRolloff =
		    GetFmodExSymbol("?set3DCustomRolloff@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_VECTOR@@H@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(FMOD_VECTOR*, int)>(
			   g_sym_FMOD_Channel_set3DCustomRolloff))(points, numpoints);
}

FMOD_RESULT F_API FMOD::Channel::get3DCustomRolloff(FMOD_VECTOR** points, int* numpoints)
{
	if (!g_sym_FMOD_Channel_get3DCustomRolloff)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_get3DCustomRolloff = GetFmodExSymbol(
		    "?get3DCustomRolloff@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAUFMOD_VECTOR@@PEAH@Z");
#else
		g_sym_FMOD_Channel_get3DCustomRolloff =
		    GetFmodExSymbol("?get3DCustomRolloff@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAPAUFMOD_VECTOR@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(FMOD_VECTOR**, int*)>(
			   g_sym_FMOD_Channel_get3DCustomRolloff))(points, numpoints);
}

FMOD_RESULT F_API FMOD::Channel::set3DOcclusion(float directocclusion, float reverbocclusion)
{
	if (!g_sym_FMOD_Channel_set3DOcclusion)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_set3DOcclusion =
		    GetFmodExSymbol("?set3DOcclusion@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@MM@Z");
#else
		g_sym_FMOD_Channel_set3DOcclusion =
		    GetFmodExSymbol("?set3DOcclusion@Channel@FMOD@@QAG?AW4FMOD_RESULT@@MM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float, float)>(
			   g_sym_FMOD_Channel_set3DOcclusion))(directocclusion, reverbocclusion);
}

FMOD_RESULT F_API FMOD::Channel::get3DOcclusion(float* directocclusion, float* reverbocclusion)
{
	if (!g_sym_FMOD_Channel_get3DOcclusion)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_get3DOcclusion =
		    GetFmodExSymbol("?get3DOcclusion@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM0@Z");
#else
		g_sym_FMOD_Channel_get3DOcclusion =
		    GetFmodExSymbol("?get3DOcclusion@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAM0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*, float*)>(
			   g_sym_FMOD_Channel_get3DOcclusion))(directocclusion, reverbocclusion);
}

FMOD_RESULT F_API FMOD::Channel::set3DSpread(float angle)
{
	if (!g_sym_FMOD_Channel_set3DSpread)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_set3DSpread = GetFmodExSymbol("?set3DSpread@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_Channel_set3DSpread = GetFmodExSymbol("?set3DSpread@Channel@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float)>(g_sym_FMOD_Channel_set3DSpread))(
	    angle);
}

FMOD_RESULT F_API FMOD::Channel::get3DSpread(float* angle)
{
	if (!g_sym_FMOD_Channel_get3DSpread)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_get3DSpread =
		    GetFmodExSymbol("?get3DSpread@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_Channel_get3DSpread =
		    GetFmodExSymbol("?get3DSpread@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*)>(g_sym_FMOD_Channel_get3DSpread))(
	    angle);
}

FMOD_RESULT F_API FMOD::Channel::set3DPanLevel(float level)
{
	if (!g_sym_FMOD_Channel_set3DPanLevel)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_set3DPanLevel =
		    GetFmodExSymbol("?set3DPanLevel@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_Channel_set3DPanLevel =
		    GetFmodExSymbol("?set3DPanLevel@Channel@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float)>(
			   g_sym_FMOD_Channel_set3DPanLevel))(level);
}

FMOD_RESULT F_API FMOD::Channel::get3DPanLevel(float* level)
{
	if (!g_sym_FMOD_Channel_get3DPanLevel)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_get3DPanLevel =
		    GetFmodExSymbol("?get3DPanLevel@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_Channel_get3DPanLevel =
		    GetFmodExSymbol("?get3DPanLevel@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*)>(
			   g_sym_FMOD_Channel_get3DPanLevel))(level);
}

FMOD_RESULT F_API FMOD::Channel::set3DDopplerLevel(float level)
{
	if (!g_sym_FMOD_Channel_set3DDopplerLevel)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_set3DDopplerLevel =
		    GetFmodExSymbol("?set3DDopplerLevel@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_Channel_set3DDopplerLevel =
		    GetFmodExSymbol("?set3DDopplerLevel@Channel@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float)>(
			   g_sym_FMOD_Channel_set3DDopplerLevel))(level);
}

FMOD_RESULT F_API FMOD::Channel::get3DDopplerLevel(float* level)
{
	if (!g_sym_FMOD_Channel_get3DDopplerLevel)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_get3DDopplerLevel =
		    GetFmodExSymbol("?get3DDopplerLevel@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_Channel_get3DDopplerLevel =
		    GetFmodExSymbol("?get3DDopplerLevel@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*)>(
			   g_sym_FMOD_Channel_get3DDopplerLevel))(level);
}

FMOD_RESULT F_API FMOD::Channel::getDSPHead(DSP** dsp)
{
	if (!g_sym_FMOD_Channel_getDSPHead)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getDSPHead =
		    GetFmodExSymbol("?getDSPHead@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVDSP@2@@Z");
#else
		g_sym_FMOD_Channel_getDSPHead =
		    GetFmodExSymbol("?getDSPHead@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVDSP@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(DSP**)>(g_sym_FMOD_Channel_getDSPHead))(
	    dsp);
}

FMOD_RESULT F_API FMOD::Channel::addDSP(DSP* dsp, DSPConnection** connection)
{
	if (!g_sym_FMOD_Channel_addDSP)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_addDSP =
		    GetFmodExSymbol("?addDSP@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAVDSP@2@PEAPEAVDSPConnection@2@@Z");
#else
		g_sym_FMOD_Channel_addDSP =
		    GetFmodExSymbol("?addDSP@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAVDSP@2@PAPAVDSPConnection@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(DSP*, DSPConnection**)>(
			   g_sym_FMOD_Channel_addDSP))(dsp, connection);
}

FMOD_RESULT F_API FMOD::Channel::isPlaying(bool* isplaying)
{
	if (!g_sym_FMOD_Channel_isPlaying)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_isPlaying = GetFmodExSymbol("?isPlaying@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEA_N@Z");
#else
		g_sym_FMOD_Channel_isPlaying = GetFmodExSymbol("?isPlaying@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(bool*)>(g_sym_FMOD_Channel_isPlaying))(
	    isplaying);
}

FMOD_RESULT F_API FMOD::Channel::isVirtual(bool* isvirtual)
{
	if (!g_sym_FMOD_Channel_isVirtual)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_isVirtual = GetFmodExSymbol("?isVirtual@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEA_N@Z");
#else
		g_sym_FMOD_Channel_isVirtual = GetFmodExSymbol("?isVirtual@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(bool*)>(g_sym_FMOD_Channel_isVirtual))(
	    isvirtual);
}

FMOD_RESULT F_API FMOD::Channel::getAudibility(float* audibility)
{
	if (!g_sym_FMOD_Channel_getAudibility)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getAudibility =
		    GetFmodExSymbol("?getAudibility@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_Channel_getAudibility =
		    GetFmodExSymbol("?getAudibility@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*)>(
			   g_sym_FMOD_Channel_getAudibility))(audibility);
}

FMOD_RESULT F_API FMOD::Channel::getCurrentSound(Sound** sound)
{
	if (!g_sym_FMOD_Channel_getCurrentSound)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getCurrentSound =
		    GetFmodExSymbol("?getCurrentSound@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVSound@2@@Z");
#else
		g_sym_FMOD_Channel_getCurrentSound =
		    GetFmodExSymbol("?getCurrentSound@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVSound@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(Sound**)>(
			   g_sym_FMOD_Channel_getCurrentSound))(sound);
}

FMOD_RESULT F_API FMOD::Channel::getSpectrum(float* spectrumarray, int numvalues, int channeloffset,
                                             FMOD_DSP_FFT_WINDOW windowtype)
{
	if (!g_sym_FMOD_Channel_getSpectrum)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getSpectrum =
		    GetFmodExSymbol("?getSpectrum@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAMHHW4FMOD_DSP_FFT_WINDOW@@@Z");
#else
		g_sym_FMOD_Channel_getSpectrum =
		    GetFmodExSymbol("?getSpectrum@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAMHHW4FMOD_DSP_FFT_WINDOW@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*, int, int, FMOD_DSP_FFT_WINDOW)>(
			   g_sym_FMOD_Channel_getSpectrum))(spectrumarray, numvalues, channeloffset, windowtype);
}

FMOD_RESULT F_API FMOD::Channel::getWaveData(float* wavearray, int numvalues, int channeloffset)
{
	if (!g_sym_FMOD_Channel_getWaveData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getWaveData =
		    GetFmodExSymbol("?getWaveData@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAMHH@Z");
#else
		g_sym_FMOD_Channel_getWaveData =
		    GetFmodExSymbol("?getWaveData@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAMHH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(float*, int, int)>(
			   g_sym_FMOD_Channel_getWaveData))(wavearray, numvalues, channeloffset);
}

FMOD_RESULT F_API FMOD::Channel::getIndex(int* index)
{
	if (!g_sym_FMOD_Channel_getIndex)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getIndex = GetFmodExSymbol("?getIndex@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_Channel_getIndex = GetFmodExSymbol("?getIndex@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(int*)>(g_sym_FMOD_Channel_getIndex))(
	    index);
}

FMOD_RESULT F_API FMOD::Channel::setMode(FMOD_MODE mode)
{
	if (!g_sym_FMOD_Channel_setMode)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setMode = GetFmodExSymbol("?setMode@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@I@Z");
#else
		g_sym_FMOD_Channel_setMode = GetFmodExSymbol("?setMode@Channel@FMOD@@QAG?AW4FMOD_RESULT@@I@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(FMOD_MODE)>(g_sym_FMOD_Channel_setMode))(
	    mode);
}

FMOD_RESULT F_API FMOD::Channel::getMode(FMOD_MODE* mode)
{
	if (!g_sym_FMOD_Channel_getMode)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getMode = GetFmodExSymbol("?getMode@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAI@Z");
#else
		g_sym_FMOD_Channel_getMode = GetFmodExSymbol("?getMode@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAI@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(FMOD_MODE*)>(g_sym_FMOD_Channel_getMode))(
	    mode);
}

FMOD_RESULT F_API FMOD::Channel::setLoopCount(int loopcount)
{
	if (!g_sym_FMOD_Channel_setLoopCount)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setLoopCount =
		    GetFmodExSymbol("?setLoopCount@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@H@Z");
#else
		g_sym_FMOD_Channel_setLoopCount =
		    GetFmodExSymbol("?setLoopCount@Channel@FMOD@@QAG?AW4FMOD_RESULT@@H@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(int)>(g_sym_FMOD_Channel_setLoopCount))(
	    loopcount);
}

FMOD_RESULT F_API FMOD::Channel::getLoopCount(int* loopcount)
{
	if (!g_sym_FMOD_Channel_getLoopCount)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getLoopCount =
		    GetFmodExSymbol("?getLoopCount@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_Channel_getLoopCount =
		    GetFmodExSymbol("?getLoopCount@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(int*)>(g_sym_FMOD_Channel_getLoopCount))(
	    loopcount);
}

FMOD_RESULT F_API FMOD::Channel::setLoopPoints(unsigned int loopstart, FMOD_TIMEUNIT loopstarttype,
                                               unsigned int loopend, FMOD_TIMEUNIT loopendtype)
{
	if (!g_sym_FMOD_Channel_setLoopPoints)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setLoopPoints =
		    GetFmodExSymbol("?setLoopPoints@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@IIII@Z");
#else
		g_sym_FMOD_Channel_setLoopPoints =
		    GetFmodExSymbol("?setLoopPoints@Channel@FMOD@@QAG?AW4FMOD_RESULT@@IIII@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(unsigned int, FMOD_TIMEUNIT, unsigned int,
	                                                                      FMOD_TIMEUNIT)>(
			   g_sym_FMOD_Channel_setLoopPoints))(loopstart, loopstarttype, loopend, loopendtype);
}

FMOD_RESULT F_API FMOD::Channel::getLoopPoints(unsigned int* loopstart, FMOD_TIMEUNIT loopstarttype,
                                               unsigned int* loopend, FMOD_TIMEUNIT loopendtype)
{
	if (!g_sym_FMOD_Channel_getLoopPoints)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getLoopPoints =
		    GetFmodExSymbol("?getLoopPoints@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAII0I@Z");
#else
		g_sym_FMOD_Channel_getLoopPoints =
		    GetFmodExSymbol("?getLoopPoints@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAII0I@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(unsigned int*, FMOD_TIMEUNIT,
	                                                                      unsigned int*, FMOD_TIMEUNIT)>(
			   g_sym_FMOD_Channel_getLoopPoints))(loopstart, loopstarttype, loopend, loopendtype);
}

FMOD_RESULT F_API FMOD::Channel::setUserData(void* userdata)
{
	if (!g_sym_FMOD_Channel_setUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_setUserData =
		    GetFmodExSymbol("?setUserData@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAX@Z");
#else
		g_sym_FMOD_Channel_setUserData =
		    GetFmodExSymbol("?setUserData@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(void*)>(g_sym_FMOD_Channel_setUserData))(
	    userdata);
}

FMOD_RESULT F_API FMOD::Channel::getUserData(void** userdata)
{
	if (!g_sym_FMOD_Channel_getUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Channel_getUserData =
		    GetFmodExSymbol("?getUserData@Channel@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAX@Z");
#else
		g_sym_FMOD_Channel_getUserData =
		    GetFmodExSymbol("?getUserData@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAPAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Channel::*&)(void**)>(g_sym_FMOD_Channel_getUserData))(
	    userdata);
}

FMOD_RESULT F_API FMOD::ChannelGroup::release()
{
	if (!g_sym_FMOD_ChannelGroup_release)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_release =
		    GetFmodExSymbol("?release@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_ChannelGroup_release = GetFmodExSymbol("?release@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (
	    this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)()>(g_sym_FMOD_ChannelGroup_release))();
}

FMOD_RESULT F_API FMOD::ChannelGroup::getSystemObject(System** system)
{
	if (!g_sym_FMOD_ChannelGroup_getSystemObject)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getSystemObject =
		    GetFmodExSymbol("?getSystemObject@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVSystem@2@@Z");
#else
		g_sym_FMOD_ChannelGroup_getSystemObject =
		    GetFmodExSymbol("?getSystemObject@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVSystem@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(System**)>(
			   g_sym_FMOD_ChannelGroup_getSystemObject))(system);
}

FMOD_RESULT F_API FMOD::ChannelGroup::setVolume(float volume)
{
	if (!g_sym_FMOD_ChannelGroup_setVolume)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_setVolume =
		    GetFmodExSymbol("?setVolume@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_ChannelGroup_setVolume =
		    GetFmodExSymbol("?setVolume@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(float)>(
			   g_sym_FMOD_ChannelGroup_setVolume))(volume);
}

FMOD_RESULT F_API FMOD::ChannelGroup::getVolume(float* volume)
{
	if (!g_sym_FMOD_ChannelGroup_getVolume)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getVolume =
		    GetFmodExSymbol("?getVolume@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_ChannelGroup_getVolume =
		    GetFmodExSymbol("?getVolume@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(float*)>(
			   g_sym_FMOD_ChannelGroup_getVolume))(volume);
}

FMOD_RESULT F_API FMOD::ChannelGroup::setPitch(float pitch)
{
	if (!g_sym_FMOD_ChannelGroup_setPitch)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_setPitch =
		    GetFmodExSymbol("?setPitch@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_ChannelGroup_setPitch =
		    GetFmodExSymbol("?setPitch@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(float)>(
			   g_sym_FMOD_ChannelGroup_setPitch))(pitch);
}

FMOD_RESULT F_API FMOD::ChannelGroup::getPitch(float* pitch)
{
	if (!g_sym_FMOD_ChannelGroup_getPitch)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getPitch =
		    GetFmodExSymbol("?getPitch@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_ChannelGroup_getPitch =
		    GetFmodExSymbol("?getPitch@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(float*)>(
			   g_sym_FMOD_ChannelGroup_getPitch))(pitch);
}

FMOD_RESULT F_API FMOD::ChannelGroup::set3DOcclusion(float directocclusion, float reverbocclusion)
{
	if (!g_sym_FMOD_ChannelGroup_set3DOcclusion)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_set3DOcclusion =
		    GetFmodExSymbol("?set3DOcclusion@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@MM@Z");
#else
		g_sym_FMOD_ChannelGroup_set3DOcclusion =
		    GetFmodExSymbol("?set3DOcclusion@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@MM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(float, float)>(
			   g_sym_FMOD_ChannelGroup_set3DOcclusion))(directocclusion, reverbocclusion);
}

FMOD_RESULT F_API FMOD::ChannelGroup::get3DOcclusion(float* directocclusion, float* reverbocclusion)
{
	if (!g_sym_FMOD_ChannelGroup_get3DOcclusion)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_get3DOcclusion =
		    GetFmodExSymbol("?get3DOcclusion@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM0@Z");
#else
		g_sym_FMOD_ChannelGroup_get3DOcclusion =
		    GetFmodExSymbol("?get3DOcclusion@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAM0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(float*, float*)>(
			   g_sym_FMOD_ChannelGroup_get3DOcclusion))(directocclusion, reverbocclusion);
}

FMOD_RESULT F_API FMOD::ChannelGroup::setPaused(bool paused)
{
	if (!g_sym_FMOD_ChannelGroup_setPaused)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_setPaused =
		    GetFmodExSymbol("?setPaused@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@_N@Z");
#else
		g_sym_FMOD_ChannelGroup_setPaused =
		    GetFmodExSymbol("?setPaused@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(bool)>(
			   g_sym_FMOD_ChannelGroup_setPaused))(paused);
}

FMOD_RESULT F_API FMOD::ChannelGroup::getPaused(bool* paused)
{
	if (!g_sym_FMOD_ChannelGroup_getPaused)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getPaused =
		    GetFmodExSymbol("?getPaused@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEA_N@Z");
#else
		g_sym_FMOD_ChannelGroup_getPaused =
		    GetFmodExSymbol("?getPaused@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(bool*)>(
			   g_sym_FMOD_ChannelGroup_getPaused))(paused);
}

FMOD_RESULT F_API FMOD::ChannelGroup::setMute(bool mute)
{
	if (!g_sym_FMOD_ChannelGroup_setMute)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_setMute =
		    GetFmodExSymbol("?setMute@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@_N@Z");
#else
		g_sym_FMOD_ChannelGroup_setMute =
		    GetFmodExSymbol("?setMute@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(bool)>(
			   g_sym_FMOD_ChannelGroup_setMute))(mute);
}

FMOD_RESULT F_API FMOD::ChannelGroup::getMute(bool* mute)
{
	if (!g_sym_FMOD_ChannelGroup_getMute)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getMute =
		    GetFmodExSymbol("?getMute@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEA_N@Z");
#else
		g_sym_FMOD_ChannelGroup_getMute =
		    GetFmodExSymbol("?getMute@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(bool*)>(
			   g_sym_FMOD_ChannelGroup_getMute))(mute);
}

FMOD_RESULT F_API FMOD::ChannelGroup::stop()
{
	if (!g_sym_FMOD_ChannelGroup_stop)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_stop = GetFmodExSymbol("?stop@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_ChannelGroup_stop = GetFmodExSymbol("?stop@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)()>(g_sym_FMOD_ChannelGroup_stop))();
}

FMOD_RESULT F_API FMOD::ChannelGroup::overrideVolume(float volume)
{
	if (!g_sym_FMOD_ChannelGroup_overrideVolume)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_overrideVolume =
		    GetFmodExSymbol("?overrideVolume@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_ChannelGroup_overrideVolume =
		    GetFmodExSymbol("?overrideVolume@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(float)>(
			   g_sym_FMOD_ChannelGroup_overrideVolume))(volume);
}

FMOD_RESULT F_API FMOD::ChannelGroup::overrideFrequency(float frequency)
{
	if (!g_sym_FMOD_ChannelGroup_overrideFrequency)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_overrideFrequency =
		    GetFmodExSymbol("?overrideFrequency@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_ChannelGroup_overrideFrequency =
		    GetFmodExSymbol("?overrideFrequency@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(float)>(
			   g_sym_FMOD_ChannelGroup_overrideFrequency))(frequency);
}

FMOD_RESULT F_API FMOD::ChannelGroup::overridePan(float pan)
{
	if (!g_sym_FMOD_ChannelGroup_overridePan)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_overridePan =
		    GetFmodExSymbol("?overridePan@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_ChannelGroup_overridePan =
		    GetFmodExSymbol("?overridePan@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(float)>(
			   g_sym_FMOD_ChannelGroup_overridePan))(pan);
}

FMOD_RESULT F_API FMOD::ChannelGroup::overrideReverbProperties(const FMOD_REVERB_CHANNELPROPERTIES* prop)
{
	if (!g_sym_FMOD_ChannelGroup_overrideReverbProperties)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_overrideReverbProperties =
		    GetFmodExSymbol("?overrideReverbProperties@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEBUFMOD_REVERB_"
		                    "CHANNELPROPERTIES@@@Z");
#else
		g_sym_FMOD_ChannelGroup_overrideReverbProperties =
		    GetFmodExSymbol("?overrideReverbProperties@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_REVERB_"
		                    "CHANNELPROPERTIES@@@Z");
#endif
	}

	return (
	    this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(const FMOD_REVERB_CHANNELPROPERTIES*)>(
		       g_sym_FMOD_ChannelGroup_overrideReverbProperties))(prop);
}

FMOD_RESULT F_API FMOD::ChannelGroup::override3DAttributes(const FMOD_VECTOR* pos, const FMOD_VECTOR* vel)
{
	if (!g_sym_FMOD_ChannelGroup_override3DAttributes)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_override3DAttributes = GetFmodExSymbol(
		    "?override3DAttributes@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEBUFMOD_VECTOR@@0@Z");
#else
		g_sym_FMOD_ChannelGroup_override3DAttributes =
		    GetFmodExSymbol("?override3DAttributes@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_VECTOR@@0@Z");
#endif
	}

	return (
	    this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(const FMOD_VECTOR*, const FMOD_VECTOR*)>(
		       g_sym_FMOD_ChannelGroup_override3DAttributes))(pos, vel);
}

FMOD_RESULT F_API FMOD::ChannelGroup::overrideSpeakerMix(float frontleft, float frontright, float center, float lfe,
                                                         float backleft, float backright, float sideleft,
                                                         float sideright)
{
	if (!g_sym_FMOD_ChannelGroup_overrideSpeakerMix)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_overrideSpeakerMix =
		    GetFmodExSymbol("?overrideSpeakerMix@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@MMMMMMMM@Z");
#else
		g_sym_FMOD_ChannelGroup_overrideSpeakerMix =
		    GetFmodExSymbol("?overrideSpeakerMix@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@MMMMMMMM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(float, float, float, float, float,
	                                                                           float, float, float)>(
			   g_sym_FMOD_ChannelGroup_overrideSpeakerMix))(frontleft, frontright, center, lfe, backleft,
	                                                                backright, sideleft, sideright);
}

FMOD_RESULT F_API FMOD::ChannelGroup::addGroup(ChannelGroup* group)
{
	if (!g_sym_FMOD_ChannelGroup_addGroup)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_addGroup =
		    GetFmodExSymbol("?addGroup@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAV12@@Z");
#else
		g_sym_FMOD_ChannelGroup_addGroup =
		    GetFmodExSymbol("?addGroup@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAV12@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(ChannelGroup*)>(
			   g_sym_FMOD_ChannelGroup_addGroup))(group);
}

FMOD_RESULT F_API FMOD::ChannelGroup::getNumGroups(int* numgroups)
{
	if (!g_sym_FMOD_ChannelGroup_getNumGroups)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getNumGroups =
		    GetFmodExSymbol("?getNumGroups@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_ChannelGroup_getNumGroups =
		    GetFmodExSymbol("?getNumGroups@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(int*)>(
			   g_sym_FMOD_ChannelGroup_getNumGroups))(numgroups);
}

FMOD_RESULT F_API FMOD::ChannelGroup::getGroup(int index, ChannelGroup** group)
{
	if (!g_sym_FMOD_ChannelGroup_getGroup)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getGroup =
		    GetFmodExSymbol("?getGroup@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAPEAV12@@Z");
#else
		g_sym_FMOD_ChannelGroup_getGroup =
		    GetFmodExSymbol("?getGroup@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@HPAPAV12@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(int, ChannelGroup**)>(
			   g_sym_FMOD_ChannelGroup_getGroup))(index, group);
}

FMOD_RESULT F_API FMOD::ChannelGroup::getParentGroup(ChannelGroup** group)
{
	if (!g_sym_FMOD_ChannelGroup_getParentGroup)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getParentGroup =
		    GetFmodExSymbol("?getParentGroup@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAV12@@Z");
#else
		g_sym_FMOD_ChannelGroup_getParentGroup =
		    GetFmodExSymbol("?getParentGroup@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAPAV12@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(ChannelGroup**)>(
			   g_sym_FMOD_ChannelGroup_getParentGroup))(group);
}

FMOD_RESULT F_API FMOD::ChannelGroup::getDSPHead(DSP** dsp)
{
	if (!g_sym_FMOD_ChannelGroup_getDSPHead)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getDSPHead =
		    GetFmodExSymbol("?getDSPHead@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVDSP@2@@Z");
#else
		g_sym_FMOD_ChannelGroup_getDSPHead =
		    GetFmodExSymbol("?getDSPHead@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVDSP@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(DSP**)>(
			   g_sym_FMOD_ChannelGroup_getDSPHead))(dsp);
}

FMOD_RESULT F_API FMOD::ChannelGroup::addDSP(DSP* dsp, DSPConnection** connection)
{
	if (g_hasC1Fmod)
	{
		if (!g_sym_FMOD_ChannelGroup_addDSP)
		{
#ifdef BUILD_64BIT
			g_sym_FMOD_ChannelGroup_addDSP =
			    GetFmodExSymbol("?addDSP@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAVDSP@2@@Z");
#else
			g_sym_FMOD_ChannelGroup_addDSP =
			    GetFmodExSymbol("?addDSP@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAVDSP@2@@Z");
#endif
		}

		// no connection parameter
		return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(DSP*)>(
				   g_sym_FMOD_ChannelGroup_addDSP))(dsp);
	}

	if (!g_sym_FMOD_ChannelGroup_addDSP)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_addDSP = GetFmodExSymbol(
		    "?addDSP@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAVDSP@2@PEAPEAVDSPConnection@2@@Z");
#else
		g_sym_FMOD_ChannelGroup_addDSP =
		    GetFmodExSymbol("?addDSP@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAVDSP@2@PAPAVDSPConnection@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(DSP*, DSPConnection**)>(
			   g_sym_FMOD_ChannelGroup_addDSP))(dsp, connection);
}

FMOD_RESULT F_API FMOD::ChannelGroup::getName(char* name, int namelen)
{
	if (!g_sym_FMOD_ChannelGroup_getName)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getName =
		    GetFmodExSymbol("?getName@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEADH@Z");
#else
		g_sym_FMOD_ChannelGroup_getName =
		    GetFmodExSymbol("?getName@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PADH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(char*, int)>(
			   g_sym_FMOD_ChannelGroup_getName))(name, namelen);
}

FMOD_RESULT F_API FMOD::ChannelGroup::getNumChannels(int* numchannels)
{
	if (!g_sym_FMOD_ChannelGroup_getNumChannels)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getNumChannels =
		    GetFmodExSymbol("?getNumChannels@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_ChannelGroup_getNumChannels =
		    GetFmodExSymbol("?getNumChannels@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(int*)>(
			   g_sym_FMOD_ChannelGroup_getNumChannels))(numchannels);
}

FMOD_RESULT F_API FMOD::ChannelGroup::getChannel(int index, Channel** channel)
{
	if (!g_sym_FMOD_ChannelGroup_getChannel)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getChannel =
		    GetFmodExSymbol("?getChannel@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAPEAVChannel@2@@Z");
#else
		g_sym_FMOD_ChannelGroup_getChannel =
		    GetFmodExSymbol("?getChannel@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@HPAPAVChannel@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(int, Channel**)>(
			   g_sym_FMOD_ChannelGroup_getChannel))(index, channel);
}

FMOD_RESULT F_API FMOD::ChannelGroup::getSpectrum(float* spectrumarray, int numvalues, int channeloffset,
                                                  FMOD_DSP_FFT_WINDOW windowtype)
{
	if (!g_sym_FMOD_ChannelGroup_getSpectrum)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getSpectrum = GetFmodExSymbol(
		    "?getSpectrum@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAMHHW4FMOD_DSP_FFT_WINDOW@@@Z");
#else
		g_sym_FMOD_ChannelGroup_getSpectrum = GetFmodExSymbol(
		    "?getSpectrum@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAMHHW4FMOD_DSP_FFT_WINDOW@@@Z");
#endif
	}

	return (
	    this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(float*, int, int, FMOD_DSP_FFT_WINDOW)>(
		       g_sym_FMOD_ChannelGroup_getSpectrum))(spectrumarray, numvalues, channeloffset, windowtype);
}

FMOD_RESULT F_API FMOD::ChannelGroup::getWaveData(float* wavearray, int numvalues, int channeloffset)
{
	if (!g_sym_FMOD_ChannelGroup_getWaveData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getWaveData =
		    GetFmodExSymbol("?getWaveData@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAMHH@Z");
#else
		g_sym_FMOD_ChannelGroup_getWaveData =
		    GetFmodExSymbol("?getWaveData@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAMHH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(float*, int, int)>(
			   g_sym_FMOD_ChannelGroup_getWaveData))(wavearray, numvalues, channeloffset);
}

FMOD_RESULT F_API FMOD::ChannelGroup::setUserData(void* userdata)
{
	if (!g_sym_FMOD_ChannelGroup_setUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_setUserData =
		    GetFmodExSymbol("?setUserData@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAX@Z");
#else
		g_sym_FMOD_ChannelGroup_setUserData =
		    GetFmodExSymbol("?setUserData@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(void*)>(
			   g_sym_FMOD_ChannelGroup_setUserData))(userdata);
}

FMOD_RESULT F_API FMOD::ChannelGroup::getUserData(void** userdata)
{
	if (!g_sym_FMOD_ChannelGroup_getUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_ChannelGroup_getUserData =
		    GetFmodExSymbol("?getUserData@ChannelGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAX@Z");
#else
		g_sym_FMOD_ChannelGroup_getUserData =
		    GetFmodExSymbol("?getUserData@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAPAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::ChannelGroup::*&)(void**)>(
			   g_sym_FMOD_ChannelGroup_getUserData))(userdata);
}

FMOD_RESULT F_API FMOD::SoundGroup::release()
{
	if (!g_sym_FMOD_SoundGroup_release)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_release = GetFmodExSymbol("?release@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_SoundGroup_release = GetFmodExSymbol("?release@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)()>(g_sym_FMOD_SoundGroup_release))();
}

FMOD_RESULT F_API FMOD::SoundGroup::getSystemObject(System** system)
{
	if (!g_sym_FMOD_SoundGroup_getSystemObject)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_getSystemObject =
		    GetFmodExSymbol("?getSystemObject@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVSystem@2@@Z");
#else
		g_sym_FMOD_SoundGroup_getSystemObject =
		    GetFmodExSymbol("?getSystemObject@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVSystem@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(System**)>(
			   g_sym_FMOD_SoundGroup_getSystemObject))(system);
}

FMOD_RESULT F_API FMOD::SoundGroup::setMaxAudible(int maxaudible)
{
	if (!g_sym_FMOD_SoundGroup_setMaxAudible)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_setMaxAudible =
		    GetFmodExSymbol("?setMaxAudible@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@H@Z");
#else
		g_sym_FMOD_SoundGroup_setMaxAudible =
		    GetFmodExSymbol("?setMaxAudible@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@H@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(int)>(
			   g_sym_FMOD_SoundGroup_setMaxAudible))(maxaudible);
}

FMOD_RESULT F_API FMOD::SoundGroup::getMaxAudible(int* maxaudible)
{
	if (!g_sym_FMOD_SoundGroup_getMaxAudible)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_getMaxAudible =
		    GetFmodExSymbol("?getMaxAudible@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_SoundGroup_getMaxAudible =
		    GetFmodExSymbol("?getMaxAudible@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(int*)>(
			   g_sym_FMOD_SoundGroup_getMaxAudible))(maxaudible);
}

FMOD_RESULT F_API FMOD::SoundGroup::setMaxAudibleBehavior(FMOD_SOUNDGROUP_BEHAVIOR behavior)
{
	if (!g_sym_FMOD_SoundGroup_setMaxAudibleBehavior)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_setMaxAudibleBehavior = GetFmodExSymbol(
		    "?setMaxAudibleBehavior@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_SOUNDGROUP_BEHAVIOR@@@Z");
#else
		g_sym_FMOD_SoundGroup_setMaxAudibleBehavior = GetFmodExSymbol(
		    "?setMaxAudibleBehavior@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_SOUNDGROUP_BEHAVIOR@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(FMOD_SOUNDGROUP_BEHAVIOR)>(
			   g_sym_FMOD_SoundGroup_setMaxAudibleBehavior))(behavior);
}

FMOD_RESULT F_API FMOD::SoundGroup::getMaxAudibleBehavior(FMOD_SOUNDGROUP_BEHAVIOR* behavior)
{
	if (!g_sym_FMOD_SoundGroup_getMaxAudibleBehavior)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_getMaxAudibleBehavior = GetFmodExSymbol(
		    "?getMaxAudibleBehavior@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAW4FMOD_SOUNDGROUP_BEHAVIOR@@@Z");
#else
		g_sym_FMOD_SoundGroup_getMaxAudibleBehavior = GetFmodExSymbol(
		    "?getMaxAudibleBehavior@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAW4FMOD_SOUNDGROUP_BEHAVIOR@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(FMOD_SOUNDGROUP_BEHAVIOR*)>(
			   g_sym_FMOD_SoundGroup_getMaxAudibleBehavior))(behavior);
}

FMOD_RESULT F_API FMOD::SoundGroup::setMuteFadeSpeed(float speed)
{
	if (!g_sym_FMOD_SoundGroup_setMuteFadeSpeed)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_setMuteFadeSpeed =
		    GetFmodExSymbol("?setMuteFadeSpeed@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_SoundGroup_setMuteFadeSpeed =
		    GetFmodExSymbol("?setMuteFadeSpeed@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(float)>(
			   g_sym_FMOD_SoundGroup_setMuteFadeSpeed))(speed);
}

FMOD_RESULT F_API FMOD::SoundGroup::getMuteFadeSpeed(float* speed)
{
	if (!g_sym_FMOD_SoundGroup_getMuteFadeSpeed)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_getMuteFadeSpeed =
		    GetFmodExSymbol("?getMuteFadeSpeed@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_SoundGroup_getMuteFadeSpeed =
		    GetFmodExSymbol("?getMuteFadeSpeed@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(float*)>(
			   g_sym_FMOD_SoundGroup_getMuteFadeSpeed))(speed);
}

FMOD_RESULT F_API FMOD::SoundGroup::setVolume(float volume)
{
	if (!g_sym_FMOD_SoundGroup_setVolume)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_setVolume =
		    GetFmodExSymbol("?setVolume@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_SoundGroup_setVolume =
		    GetFmodExSymbol("?setVolume@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(float)>(
			   g_sym_FMOD_SoundGroup_setVolume))(volume);
}

FMOD_RESULT F_API FMOD::SoundGroup::getVolume(float* volume)
{
	if (!g_sym_FMOD_SoundGroup_getVolume)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_getVolume =
		    GetFmodExSymbol("?getVolume@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_SoundGroup_getVolume =
		    GetFmodExSymbol("?getVolume@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(float*)>(
			   g_sym_FMOD_SoundGroup_getVolume))(volume);
}

FMOD_RESULT F_API FMOD::SoundGroup::stop()
{
	if (!g_sym_FMOD_SoundGroup_stop)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_stop = GetFmodExSymbol("?stop@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_SoundGroup_stop = GetFmodExSymbol("?stop@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)()>(g_sym_FMOD_SoundGroup_stop))();
}

FMOD_RESULT F_API FMOD::SoundGroup::getName(char* name, int namelen)
{
	if (!g_sym_FMOD_SoundGroup_getName)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_getName =
		    GetFmodExSymbol("?getName@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEADH@Z");
#else
		g_sym_FMOD_SoundGroup_getName = GetFmodExSymbol("?getName@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@PADH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(char*, int)>(
			   g_sym_FMOD_SoundGroup_getName))(name, namelen);
}

FMOD_RESULT F_API FMOD::SoundGroup::getNumSounds(int* numsounds)
{
	if (!g_sym_FMOD_SoundGroup_getNumSounds)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_getNumSounds =
		    GetFmodExSymbol("?getNumSounds@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_SoundGroup_getNumSounds =
		    GetFmodExSymbol("?getNumSounds@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(int*)>(
			   g_sym_FMOD_SoundGroup_getNumSounds))(numsounds);
}

FMOD_RESULT F_API FMOD::SoundGroup::getSound(int index, Sound** sound)
{
	if (!g_sym_FMOD_SoundGroup_getSound)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_getSound =
		    GetFmodExSymbol("?getSound@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAPEAVSound@2@@Z");
#else
		g_sym_FMOD_SoundGroup_getSound =
		    GetFmodExSymbol("?getSound@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@HPAPAVSound@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(int, Sound**)>(
			   g_sym_FMOD_SoundGroup_getSound))(index, sound);
}

FMOD_RESULT F_API FMOD::SoundGroup::getNumPlaying(int* numplaying)
{
	if (!g_sym_FMOD_SoundGroup_getNumPlaying)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_getNumPlaying =
		    GetFmodExSymbol("?getNumPlaying@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_SoundGroup_getNumPlaying =
		    GetFmodExSymbol("?getNumPlaying@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(int*)>(
			   g_sym_FMOD_SoundGroup_getNumPlaying))(numplaying);
}

FMOD_RESULT F_API FMOD::SoundGroup::setUserData(void* userdata)
{
	if (!g_sym_FMOD_SoundGroup_setUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_setUserData =
		    GetFmodExSymbol("?setUserData@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAX@Z");
#else
		g_sym_FMOD_SoundGroup_setUserData =
		    GetFmodExSymbol("?setUserData@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(void*)>(
			   g_sym_FMOD_SoundGroup_setUserData))(userdata);
}

FMOD_RESULT F_API FMOD::SoundGroup::getUserData(void** userdata)
{
	if (!g_sym_FMOD_SoundGroup_getUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_SoundGroup_getUserData =
		    GetFmodExSymbol("?getUserData@SoundGroup@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAX@Z");
#else
		g_sym_FMOD_SoundGroup_getUserData =
		    GetFmodExSymbol("?getUserData@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAPAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::SoundGroup::*&)(void**)>(
			   g_sym_FMOD_SoundGroup_getUserData))(userdata);
}

FMOD_RESULT F_API FMOD::DSP::release()
{
	if (!g_sym_FMOD_DSP_release)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_release = GetFmodExSymbol("?release@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_DSP_release = GetFmodExSymbol("?release@DSP@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)()>(g_sym_FMOD_DSP_release))();
}

FMOD_RESULT F_API FMOD::DSP::getSystemObject(System** system)
{
	if (!g_sym_FMOD_DSP_getSystemObject)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_getSystemObject =
		    GetFmodExSymbol("?getSystemObject@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVSystem@2@@Z");
#else
		g_sym_FMOD_DSP_getSystemObject =
		    GetFmodExSymbol("?getSystemObject@DSP@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVSystem@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(System**)>(g_sym_FMOD_DSP_getSystemObject))(
	    system);
}

FMOD_RESULT F_API FMOD::DSP::addInput(DSP* target, DSPConnection** connection)
{
	if (!g_sym_FMOD_DSP_addInput)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_addInput =
		    GetFmodExSymbol("?addInput@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@PEAV12@PEAPEAVDSPConnection@2@@Z");
#else
		g_sym_FMOD_DSP_addInput =
		    GetFmodExSymbol("?addInput@DSP@FMOD@@QAG?AW4FMOD_RESULT@@PAV12@PAPAVDSPConnection@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(DSP*, DSPConnection**)>(
			   g_sym_FMOD_DSP_addInput))(target, connection);
}

FMOD_RESULT F_API FMOD::DSP::disconnectFrom(DSP* target)
{
	if (!g_sym_FMOD_DSP_disconnectFrom)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_disconnectFrom =
		    GetFmodExSymbol("?disconnectFrom@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@PEAV12@@Z");
#else
		g_sym_FMOD_DSP_disconnectFrom =
		    GetFmodExSymbol("?disconnectFrom@DSP@FMOD@@QAG?AW4FMOD_RESULT@@PAV12@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(DSP*)>(g_sym_FMOD_DSP_disconnectFrom))(
	    target);
}

FMOD_RESULT F_API FMOD::DSP::disconnectAll(bool inputs, bool outputs)
{
	if (!g_sym_FMOD_DSP_disconnectAll)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_disconnectAll = GetFmodExSymbol("?disconnectAll@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@_N0@Z");
#else
		g_sym_FMOD_DSP_disconnectAll = GetFmodExSymbol("?disconnectAll@DSP@FMOD@@QAG?AW4FMOD_RESULT@@_N0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(bool, bool)>(g_sym_FMOD_DSP_disconnectAll))(
	    inputs, outputs);
}

FMOD_RESULT F_API FMOD::DSP::remove()
{
	if (!g_sym_FMOD_DSP_remove)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_remove = GetFmodExSymbol("?remove@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_DSP_remove = GetFmodExSymbol("?remove@DSP@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)()>(g_sym_FMOD_DSP_remove))();
}

FMOD_RESULT F_API FMOD::DSP::getNumInputs(int* numinputs)
{
	if (!g_sym_FMOD_DSP_getNumInputs)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_getNumInputs = GetFmodExSymbol("?getNumInputs@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_DSP_getNumInputs = GetFmodExSymbol("?getNumInputs@DSP@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(int*)>(g_sym_FMOD_DSP_getNumInputs))(
	    numinputs);
}

FMOD_RESULT F_API FMOD::DSP::getNumOutputs(int* numoutputs)
{
	if (!g_sym_FMOD_DSP_getNumOutputs)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_getNumOutputs = GetFmodExSymbol("?getNumOutputs@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_DSP_getNumOutputs = GetFmodExSymbol("?getNumOutputs@DSP@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(int*)>(g_sym_FMOD_DSP_getNumOutputs))(
	    numoutputs);
}

FMOD_RESULT F_API FMOD::DSP::getInput(int index, DSP** input, DSPConnection** inputconnection)
{
	if (!g_sym_FMOD_DSP_getInput)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_getInput =
		    GetFmodExSymbol("?getInput@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAPEAV12@PEAPEAVDSPConnection@2@@Z");
#else
		g_sym_FMOD_DSP_getInput =
		    GetFmodExSymbol("?getInput@DSP@FMOD@@QAG?AW4FMOD_RESULT@@HPAPAV12@PAPAVDSPConnection@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(int, DSP**, DSPConnection**)>(
			   g_sym_FMOD_DSP_getInput))(index, input, inputconnection);
}

FMOD_RESULT F_API FMOD::DSP::getOutput(int index, DSP** output, DSPConnection** outputconnection)
{
	if (!g_sym_FMOD_DSP_getOutput)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_getOutput =
		    GetFmodExSymbol("?getOutput@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAPEAV12@PEAPEAVDSPConnection@2@@Z");
#else
		g_sym_FMOD_DSP_getOutput =
		    GetFmodExSymbol("?getOutput@DSP@FMOD@@QAG?AW4FMOD_RESULT@@HPAPAV12@PAPAVDSPConnection@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(int, DSP**, DSPConnection**)>(
			   g_sym_FMOD_DSP_getOutput))(index, output, outputconnection);
}

FMOD_RESULT F_API FMOD::DSP::setActive(bool active)
{
	if (!g_sym_FMOD_DSP_setActive)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_setActive = GetFmodExSymbol("?setActive@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@_N@Z");
#else
		g_sym_FMOD_DSP_setActive = GetFmodExSymbol("?setActive@DSP@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(bool)>(g_sym_FMOD_DSP_setActive))(active);
}

FMOD_RESULT F_API FMOD::DSP::getActive(bool* active)
{
	if (!g_sym_FMOD_DSP_getActive)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_getActive = GetFmodExSymbol("?getActive@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@PEA_N@Z");
#else
		g_sym_FMOD_DSP_getActive = GetFmodExSymbol("?getActive@DSP@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(bool*)>(g_sym_FMOD_DSP_getActive))(active);
}

FMOD_RESULT F_API FMOD::DSP::setBypass(bool bypass)
{
	if (!g_sym_FMOD_DSP_setBypass)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_setBypass = GetFmodExSymbol("?setBypass@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@_N@Z");
#else
		g_sym_FMOD_DSP_setBypass = GetFmodExSymbol("?setBypass@DSP@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(bool)>(g_sym_FMOD_DSP_setBypass))(bypass);
}

FMOD_RESULT F_API FMOD::DSP::getBypass(bool* bypass)
{
	if (!g_sym_FMOD_DSP_getBypass)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_getBypass = GetFmodExSymbol("?getBypass@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@PEA_N@Z");
#else
		g_sym_FMOD_DSP_getBypass = GetFmodExSymbol("?getBypass@DSP@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(bool*)>(g_sym_FMOD_DSP_getBypass))(bypass);
}

FMOD_RESULT F_API FMOD::DSP::reset()
{
	if (!g_sym_FMOD_DSP_reset)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_reset = GetFmodExSymbol("?reset@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_DSP_reset = GetFmodExSymbol("?reset@DSP@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)()>(g_sym_FMOD_DSP_reset))();
}

FMOD_RESULT F_API FMOD::DSP::setParameter(int index, float value)
{
	if (!g_sym_FMOD_DSP_setParameter)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_setParameter = GetFmodExSymbol("?setParameter@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@HM@Z");
#else
		g_sym_FMOD_DSP_setParameter = GetFmodExSymbol("?setParameter@DSP@FMOD@@QAG?AW4FMOD_RESULT@@HM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(int, float)>(g_sym_FMOD_DSP_setParameter))(
	    index, value);
}

FMOD_RESULT F_API FMOD::DSP::getParameter(int index, float* value, char* valuestr, int valuestrlen)
{
	if (!g_sym_FMOD_DSP_getParameter)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_getParameter =
		    GetFmodExSymbol("?getParameter@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAMPEADH@Z");
#else
		g_sym_FMOD_DSP_getParameter = GetFmodExSymbol("?getParameter@DSP@FMOD@@QAG?AW4FMOD_RESULT@@HPAMPADH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(int, float*, char*, int)>(
			   g_sym_FMOD_DSP_getParameter))(index, value, valuestr, valuestrlen);
}

FMOD_RESULT F_API FMOD::DSP::getNumParameters(int* numparams)
{
	if (!g_sym_FMOD_DSP_getNumParameters)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_getNumParameters =
		    GetFmodExSymbol("?getNumParameters@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_DSP_getNumParameters =
		    GetFmodExSymbol("?getNumParameters@DSP@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(int*)>(g_sym_FMOD_DSP_getNumParameters))(
	    numparams);
}

FMOD_RESULT F_API FMOD::DSP::getParameterInfo(int index, char* name, char* label, char* description, int descriptionlen,
                                              float* min, float* max)
{
	if (!g_sym_FMOD_DSP_getParameterInfo)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_getParameterInfo =
		    GetFmodExSymbol("?getParameterInfo@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAD00HPEAM1@Z");
#else
		g_sym_FMOD_DSP_getParameterInfo =
		    GetFmodExSymbol("?getParameterInfo@DSP@FMOD@@QAG?AW4FMOD_RESULT@@HPAD00HPAM1@Z");
#endif
	}

	return (
	    this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(int, char*, char*, char*, int, float*, float*)>(
		       g_sym_FMOD_DSP_getParameterInfo))(index, name, label, description, descriptionlen, min, max);
}

FMOD_RESULT F_API FMOD::DSP::showConfigDialog(void* hwnd, bool show)
{
	if (!g_sym_FMOD_DSP_showConfigDialog)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_showConfigDialog =
		    GetFmodExSymbol("?showConfigDialog@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@PEAX_N@Z");
#else
		g_sym_FMOD_DSP_showConfigDialog =
		    GetFmodExSymbol("?showConfigDialog@DSP@FMOD@@QAG?AW4FMOD_RESULT@@PAX_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(void*, bool)>(
			   g_sym_FMOD_DSP_showConfigDialog))(hwnd, show);
}

FMOD_RESULT F_API FMOD::DSP::getInfo(char* name, unsigned int* version, int* channels, int* configwidth,
                                     int* configheight)
{
	if (!g_sym_FMOD_DSP_getInfo)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_getInfo = GetFmodExSymbol("?getInfo@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@PEADPEAIPEAH22@Z");
#else
		g_sym_FMOD_DSP_getInfo = GetFmodExSymbol("?getInfo@DSP@FMOD@@QAG?AW4FMOD_RESULT@@PADPAIPAH22@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(char*, unsigned int*, int*, int*, int*)>(
			   g_sym_FMOD_DSP_getInfo))(name, version, channels, configwidth, configheight);
}

FMOD_RESULT F_API FMOD::DSP::getType(FMOD_DSP_TYPE* type)
{
	if (!g_sym_FMOD_DSP_getType)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_getType =
		    GetFmodExSymbol("?getType@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@PEAW4FMOD_DSP_TYPE@@@Z");
#else
		g_sym_FMOD_DSP_getType =
		    GetFmodExSymbol("?getType@DSP@FMOD@@QAG?AW4FMOD_RESULT@@PAW4FMOD_DSP_TYPE@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(FMOD_DSP_TYPE*)>(g_sym_FMOD_DSP_getType))(
	    type);
}

FMOD_RESULT F_API FMOD::DSP::setDefaults(float frequency, float volume, float pan, int priority)
{
	if (!g_sym_FMOD_DSP_setDefaults)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_setDefaults = GetFmodExSymbol("?setDefaults@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@MMMH@Z");
#else
		g_sym_FMOD_DSP_setDefaults = GetFmodExSymbol("?setDefaults@DSP@FMOD@@QAG?AW4FMOD_RESULT@@MMMH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(float, float, float, int)>(
			   g_sym_FMOD_DSP_setDefaults))(frequency, volume, pan, priority);
}

FMOD_RESULT F_API FMOD::DSP::getDefaults(float* frequency, float* volume, float* pan, int* priority)
{
	if (!g_sym_FMOD_DSP_getDefaults)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_getDefaults =
		    GetFmodExSymbol("?getDefaults@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM00PEAH@Z");
#else
		g_sym_FMOD_DSP_getDefaults = GetFmodExSymbol("?getDefaults@DSP@FMOD@@QAG?AW4FMOD_RESULT@@PAM00PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(float*, float*, float*, int*)>(
			   g_sym_FMOD_DSP_getDefaults))(frequency, volume, pan, priority);
}

FMOD_RESULT F_API FMOD::DSP::setUserData(void* userdata)
{
	if (!g_sym_FMOD_DSP_setUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_setUserData = GetFmodExSymbol("?setUserData@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@PEAX@Z");
#else
		g_sym_FMOD_DSP_setUserData = GetFmodExSymbol("?setUserData@DSP@FMOD@@QAG?AW4FMOD_RESULT@@PAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(void*)>(g_sym_FMOD_DSP_setUserData))(
	    userdata);
}

FMOD_RESULT F_API FMOD::DSP::getUserData(void** userdata)
{
	if (!g_sym_FMOD_DSP_getUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSP_getUserData = GetFmodExSymbol("?getUserData@DSP@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAX@Z");
#else
		g_sym_FMOD_DSP_getUserData = GetFmodExSymbol("?getUserData@DSP@FMOD@@QAG?AW4FMOD_RESULT@@PAPAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSP::*&)(void**)>(g_sym_FMOD_DSP_getUserData))(
	    userdata);
}

FMOD_RESULT F_API FMOD::DSPConnection::getInput(DSP** input)
{
	if (!g_sym_FMOD_DSPConnection_getInput)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSPConnection_getInput =
		    GetFmodExSymbol("?getInput@DSPConnection@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVDSP@2@@Z");
#else
		g_sym_FMOD_DSPConnection_getInput =
		    GetFmodExSymbol("?getInput@DSPConnection@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVDSP@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSPConnection::*&)(DSP**)>(
			   g_sym_FMOD_DSPConnection_getInput))(input);
}

FMOD_RESULT F_API FMOD::DSPConnection::getOutput(DSP** output)
{
	if (!g_sym_FMOD_DSPConnection_getOutput)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSPConnection_getOutput =
		    GetFmodExSymbol("?getOutput@DSPConnection@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVDSP@2@@Z");
#else
		g_sym_FMOD_DSPConnection_getOutput =
		    GetFmodExSymbol("?getOutput@DSPConnection@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVDSP@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSPConnection::*&)(DSP**)>(
			   g_sym_FMOD_DSPConnection_getOutput))(output);
}

FMOD_RESULT F_API FMOD::DSPConnection::setMix(float volume)
{
	if (!g_sym_FMOD_DSPConnection_setMix)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSPConnection_setMix =
		    GetFmodExSymbol("?setMix@DSPConnection@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_DSPConnection_setMix =
		    GetFmodExSymbol("?setMix@DSPConnection@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSPConnection::*&)(float)>(
			   g_sym_FMOD_DSPConnection_setMix))(volume);
}

FMOD_RESULT F_API FMOD::DSPConnection::getMix(float* volume)
{
	if (!g_sym_FMOD_DSPConnection_getMix)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSPConnection_getMix =
		    GetFmodExSymbol("?getMix@DSPConnection@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_DSPConnection_getMix =
		    GetFmodExSymbol("?getMix@DSPConnection@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSPConnection::*&)(float*)>(
			   g_sym_FMOD_DSPConnection_getMix))(volume);
}

FMOD_RESULT F_API FMOD::DSPConnection::setLevels(FMOD_SPEAKER speaker, float* levels, int numlevels)
{
	if (!g_sym_FMOD_DSPConnection_setLevels)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSPConnection_setLevels =
		    GetFmodExSymbol("?setLevels@DSPConnection@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_SPEAKER@@PEAMH@Z");
#else
		g_sym_FMOD_DSPConnection_setLevels =
		    GetFmodExSymbol("?setLevels@DSPConnection@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_SPEAKER@@PAMH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSPConnection::*&)(FMOD_SPEAKER, float*, int)>(
			   g_sym_FMOD_DSPConnection_setLevels))(speaker, levels, numlevels);
}

FMOD_RESULT F_API FMOD::DSPConnection::getLevels(FMOD_SPEAKER speaker, float* levels, int numlevels)
{
	if (!g_sym_FMOD_DSPConnection_getLevels)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSPConnection_getLevels =
		    GetFmodExSymbol("?getLevels@DSPConnection@FMOD@@QEAA?AW4FMOD_RESULT@@W4FMOD_SPEAKER@@PEAMH@Z");
#else
		g_sym_FMOD_DSPConnection_getLevels =
		    GetFmodExSymbol("?getLevels@DSPConnection@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_SPEAKER@@PAMH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSPConnection::*&)(FMOD_SPEAKER, float*, int)>(
			   g_sym_FMOD_DSPConnection_getLevels))(speaker, levels, numlevels);
}

FMOD_RESULT F_API FMOD::DSPConnection::setUserData(void* userdata)
{
	if (!g_sym_FMOD_DSPConnection_setUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSPConnection_setUserData =
		    GetFmodExSymbol("?setUserData@DSPConnection@FMOD@@QEAA?AW4FMOD_RESULT@@PEAX@Z");
#else
		g_sym_FMOD_DSPConnection_setUserData =
		    GetFmodExSymbol("?setUserData@DSPConnection@FMOD@@QAG?AW4FMOD_RESULT@@PAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSPConnection::*&)(void*)>(
			   g_sym_FMOD_DSPConnection_setUserData))(userdata);
}

FMOD_RESULT F_API FMOD::DSPConnection::getUserData(void** userdata)
{
	if (!g_sym_FMOD_DSPConnection_getUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_DSPConnection_getUserData =
		    GetFmodExSymbol("?getUserData@DSPConnection@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAX@Z");
#else
		g_sym_FMOD_DSPConnection_getUserData =
		    GetFmodExSymbol("?getUserData@DSPConnection@FMOD@@QAG?AW4FMOD_RESULT@@PAPAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::DSPConnection::*&)(void**)>(
			   g_sym_FMOD_DSPConnection_getUserData))(userdata);
}

FMOD_RESULT F_API FMOD::Geometry::release()
{
	if (!g_sym_FMOD_Geometry_release)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_release = GetFmodExSymbol("?release@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_Geometry_release = GetFmodExSymbol("?release@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)()>(g_sym_FMOD_Geometry_release))();
}

FMOD_RESULT F_API FMOD::Geometry::addPolygon(float directocclusion, float reverbocclusion, bool doublesided,
                                             int numvertices, const FMOD_VECTOR* vertices, int* polygonindex)
{
	if (!g_sym_FMOD_Geometry_addPolygon)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_addPolygon =
		    GetFmodExSymbol("?addPolygon@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@MM_NHPEBUFMOD_VECTOR@@PEAH@Z");
#else
		g_sym_FMOD_Geometry_addPolygon =
		    GetFmodExSymbol("?addPolygon@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@MM_NHPBUFMOD_VECTOR@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(
			   float, float, bool, int, const FMOD_VECTOR*, int*)>(g_sym_FMOD_Geometry_addPolygon))(
	    directocclusion, reverbocclusion, doublesided, numvertices, vertices, polygonindex);
}

FMOD_RESULT F_API FMOD::Geometry::getNumPolygons(int* numpolygons)
{
	if (!g_sym_FMOD_Geometry_getNumPolygons)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_getNumPolygons =
		    GetFmodExSymbol("?getNumPolygons@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_Geometry_getNumPolygons =
		    GetFmodExSymbol("?getNumPolygons@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(int*)>(
			   g_sym_FMOD_Geometry_getNumPolygons))(numpolygons);
}

FMOD_RESULT F_API FMOD::Geometry::getMaxPolygons(int* maxpolygons, int* maxvertices)
{
	if (!g_sym_FMOD_Geometry_getMaxPolygons)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_getMaxPolygons =
		    GetFmodExSymbol("?getMaxPolygons@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH0@Z");
#else
		g_sym_FMOD_Geometry_getMaxPolygons =
		    GetFmodExSymbol("?getMaxPolygons@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@PAH0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(int*, int*)>(
			   g_sym_FMOD_Geometry_getMaxPolygons))(maxpolygons, maxvertices);
}

FMOD_RESULT F_API FMOD::Geometry::getPolygonNumVertices(int index, int* numvertices)
{
	if (!g_sym_FMOD_Geometry_getPolygonNumVertices)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_getPolygonNumVertices =
		    GetFmodExSymbol("?getPolygonNumVertices@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAH@Z");
#else
		g_sym_FMOD_Geometry_getPolygonNumVertices =
		    GetFmodExSymbol("?getPolygonNumVertices@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@HPAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(int, int*)>(
			   g_sym_FMOD_Geometry_getPolygonNumVertices))(index, numvertices);
}

FMOD_RESULT F_API FMOD::Geometry::setPolygonVertex(int index, int vertexindex, const FMOD_VECTOR* vertex)
{
	if (!g_sym_FMOD_Geometry_setPolygonVertex)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_setPolygonVertex =
		    GetFmodExSymbol("?setPolygonVertex@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@HHPEBUFMOD_VECTOR@@@Z");
#else
		g_sym_FMOD_Geometry_setPolygonVertex =
		    GetFmodExSymbol("?setPolygonVertex@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@HHPBUFMOD_VECTOR@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(int, int, const FMOD_VECTOR*)>(
			   g_sym_FMOD_Geometry_setPolygonVertex))(index, vertexindex, vertex);
}

FMOD_RESULT F_API FMOD::Geometry::getPolygonVertex(int index, int vertexindex, FMOD_VECTOR* vertex)
{
	if (!g_sym_FMOD_Geometry_getPolygonVertex)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_getPolygonVertex =
		    GetFmodExSymbol("?getPolygonVertex@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@HHPEAUFMOD_VECTOR@@@Z");
#else
		g_sym_FMOD_Geometry_getPolygonVertex =
		    GetFmodExSymbol("?getPolygonVertex@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@HHPAUFMOD_VECTOR@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(int, int, FMOD_VECTOR*)>(
			   g_sym_FMOD_Geometry_getPolygonVertex))(index, vertexindex, vertex);
}

FMOD_RESULT F_API FMOD::Geometry::setPolygonAttributes(int index, float directocclusion, float reverbocclusion,
                                                       bool doublesided)
{
	if (!g_sym_FMOD_Geometry_setPolygonAttributes)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_setPolygonAttributes =
		    GetFmodExSymbol("?setPolygonAttributes@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@HMM_N@Z");
#else
		g_sym_FMOD_Geometry_setPolygonAttributes =
		    GetFmodExSymbol("?setPolygonAttributes@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@HMM_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(int, float, float, bool)>(
			   g_sym_FMOD_Geometry_setPolygonAttributes))(index, directocclusion, reverbocclusion,
	                                                              doublesided);
}

FMOD_RESULT F_API FMOD::Geometry::getPolygonAttributes(int index, float* directocclusion, float* reverbocclusion,
                                                       bool* doublesided)
{
	if (!g_sym_FMOD_Geometry_getPolygonAttributes)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_getPolygonAttributes =
		    GetFmodExSymbol("?getPolygonAttributes@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAM0PEA_N@Z");
#else
		g_sym_FMOD_Geometry_getPolygonAttributes =
		    GetFmodExSymbol("?getPolygonAttributes@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@HPAM0PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(int, float*, float*, bool*)>(
			   g_sym_FMOD_Geometry_getPolygonAttributes))(index, directocclusion, reverbocclusion,
	                                                              doublesided);
}

FMOD_RESULT F_API FMOD::Geometry::setActive(bool active)
{
	if (!g_sym_FMOD_Geometry_setActive)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_setActive = GetFmodExSymbol("?setActive@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@_N@Z");
#else
		g_sym_FMOD_Geometry_setActive = GetFmodExSymbol("?setActive@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(bool)>(g_sym_FMOD_Geometry_setActive))(
	    active);
}

FMOD_RESULT F_API FMOD::Geometry::getActive(bool* active)
{
	if (!g_sym_FMOD_Geometry_getActive)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_getActive =
		    GetFmodExSymbol("?getActive@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@PEA_N@Z");
#else
		g_sym_FMOD_Geometry_getActive = GetFmodExSymbol("?getActive@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(bool*)>(g_sym_FMOD_Geometry_getActive))(
	    active);
}

FMOD_RESULT F_API FMOD::Geometry::setRotation(const FMOD_VECTOR* forward, const FMOD_VECTOR* up)
{
	if (!g_sym_FMOD_Geometry_setRotation)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_setRotation =
		    GetFmodExSymbol("?setRotation@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@PEBUFMOD_VECTOR@@0@Z");
#else
		g_sym_FMOD_Geometry_setRotation =
		    GetFmodExSymbol("?setRotation@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_VECTOR@@0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(const FMOD_VECTOR*, const FMOD_VECTOR*)>(
			   g_sym_FMOD_Geometry_setRotation))(forward, up);
}

FMOD_RESULT F_API FMOD::Geometry::getRotation(FMOD_VECTOR* forward, FMOD_VECTOR* up)
{
	if (!g_sym_FMOD_Geometry_getRotation)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_getRotation =
		    GetFmodExSymbol("?getRotation@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_VECTOR@@0@Z");
#else
		g_sym_FMOD_Geometry_getRotation =
		    GetFmodExSymbol("?getRotation@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_VECTOR@@0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(FMOD_VECTOR*, FMOD_VECTOR*)>(
			   g_sym_FMOD_Geometry_getRotation))(forward, up);
}

FMOD_RESULT F_API FMOD::Geometry::setPosition(const FMOD_VECTOR* position)
{
	if (!g_sym_FMOD_Geometry_setPosition)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_setPosition =
		    GetFmodExSymbol("?setPosition@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@PEBUFMOD_VECTOR@@@Z");
#else
		g_sym_FMOD_Geometry_setPosition =
		    GetFmodExSymbol("?setPosition@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_VECTOR@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(const FMOD_VECTOR*)>(
			   g_sym_FMOD_Geometry_setPosition))(position);
}

FMOD_RESULT F_API FMOD::Geometry::getPosition(FMOD_VECTOR* position)
{
	if (!g_sym_FMOD_Geometry_getPosition)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_getPosition =
		    GetFmodExSymbol("?getPosition@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_VECTOR@@@Z");
#else
		g_sym_FMOD_Geometry_getPosition =
		    GetFmodExSymbol("?getPosition@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_VECTOR@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(FMOD_VECTOR*)>(
			   g_sym_FMOD_Geometry_getPosition))(position);
}

FMOD_RESULT F_API FMOD::Geometry::setScale(const FMOD_VECTOR* scale)
{
	if (!g_sym_FMOD_Geometry_setScale)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_setScale =
		    GetFmodExSymbol("?setScale@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@PEBUFMOD_VECTOR@@@Z");
#else
		g_sym_FMOD_Geometry_setScale =
		    GetFmodExSymbol("?setScale@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_VECTOR@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(const FMOD_VECTOR*)>(
			   g_sym_FMOD_Geometry_setScale))(scale);
}

FMOD_RESULT F_API FMOD::Geometry::getScale(FMOD_VECTOR* scale)
{
	if (!g_sym_FMOD_Geometry_getScale)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_getScale =
		    GetFmodExSymbol("?getScale@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_VECTOR@@@Z");
#else
		g_sym_FMOD_Geometry_getScale =
		    GetFmodExSymbol("?getScale@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_VECTOR@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(FMOD_VECTOR*)>(
			   g_sym_FMOD_Geometry_getScale))(scale);
}

FMOD_RESULT F_API FMOD::Geometry::save(void* data, int* datasize)
{
	if (!g_sym_FMOD_Geometry_save)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_save = GetFmodExSymbol("?save@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@PEAXPEAH@Z");
#else
		g_sym_FMOD_Geometry_save = GetFmodExSymbol("?save@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@PAXPAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(void*, int*)>(g_sym_FMOD_Geometry_save))(
	    data, datasize);
}

FMOD_RESULT F_API FMOD::Geometry::setUserData(void* userdata)
{
	if (!g_sym_FMOD_Geometry_setUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_setUserData =
		    GetFmodExSymbol("?setUserData@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@PEAX@Z");
#else
		g_sym_FMOD_Geometry_setUserData =
		    GetFmodExSymbol("?setUserData@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@PAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(void*)>(
			   g_sym_FMOD_Geometry_setUserData))(userdata);
}

FMOD_RESULT F_API FMOD::Geometry::getUserData(void** userdata)
{
	if (!g_sym_FMOD_Geometry_getUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Geometry_getUserData =
		    GetFmodExSymbol("?getUserData@Geometry@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAX@Z");
#else
		g_sym_FMOD_Geometry_getUserData =
		    GetFmodExSymbol("?getUserData@Geometry@FMOD@@QAG?AW4FMOD_RESULT@@PAPAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Geometry::*&)(void**)>(
			   g_sym_FMOD_Geometry_getUserData))(userdata);
}

FMOD_RESULT F_API FMOD::Reverb::release()
{
	if (!g_sym_FMOD_Reverb_release)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Reverb_release = GetFmodExSymbol("?release@Reverb@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_Reverb_release = GetFmodExSymbol("?release@Reverb@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Reverb::*&)()>(g_sym_FMOD_Reverb_release))();
}

FMOD_RESULT F_API FMOD::Reverb::set3DAttributes(const FMOD_VECTOR* position, float mindistance, float maxdistance)
{
	if (!g_sym_FMOD_Reverb_set3DAttributes)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Reverb_set3DAttributes =
		    GetFmodExSymbol("?set3DAttributes@Reverb@FMOD@@QEAA?AW4FMOD_RESULT@@PEBUFMOD_VECTOR@@MM@Z");
#else
		g_sym_FMOD_Reverb_set3DAttributes =
		    GetFmodExSymbol("?set3DAttributes@Reverb@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_VECTOR@@MM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Reverb::*&)(const FMOD_VECTOR*, float, float)>(
			   g_sym_FMOD_Reverb_set3DAttributes))(position, mindistance, maxdistance);
}

FMOD_RESULT F_API FMOD::Reverb::get3DAttributes(FMOD_VECTOR* position, float* mindistance, float* maxdistance)
{
	if (!g_sym_FMOD_Reverb_get3DAttributes)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Reverb_get3DAttributes =
		    GetFmodExSymbol("?get3DAttributes@Reverb@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_VECTOR@@PEAM1@Z");
#else
		g_sym_FMOD_Reverb_get3DAttributes =
		    GetFmodExSymbol("?get3DAttributes@Reverb@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_VECTOR@@PAM1@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Reverb::*&)(FMOD_VECTOR*, float*, float*)>(
			   g_sym_FMOD_Reverb_get3DAttributes))(position, mindistance, maxdistance);
}

FMOD_RESULT F_API FMOD::Reverb::setProperties(const FMOD_REVERB_PROPERTIES* properties)
{
	if (!g_sym_FMOD_Reverb_setProperties)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Reverb_setProperties =
		    GetFmodExSymbol("?setProperties@Reverb@FMOD@@QEAA?AW4FMOD_RESULT@@PEBUFMOD_REVERB_PROPERTIES@@@Z");
#else
		g_sym_FMOD_Reverb_setProperties =
		    GetFmodExSymbol("?setProperties@Reverb@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_REVERB_PROPERTIES@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Reverb::*&)(const FMOD_REVERB_PROPERTIES*)>(
			   g_sym_FMOD_Reverb_setProperties))(properties);
}

FMOD_RESULT F_API FMOD::Reverb::getProperties(FMOD_REVERB_PROPERTIES* properties)
{
	if (!g_sym_FMOD_Reverb_getProperties)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Reverb_getProperties =
		    GetFmodExSymbol("?getProperties@Reverb@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_REVERB_PROPERTIES@@@Z");
#else
		g_sym_FMOD_Reverb_getProperties =
		    GetFmodExSymbol("?getProperties@Reverb@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_REVERB_PROPERTIES@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Reverb::*&)(FMOD_REVERB_PROPERTIES*)>(
			   g_sym_FMOD_Reverb_getProperties))(properties);
}

FMOD_RESULT F_API FMOD::Reverb::setActive(bool active)
{
	if (!g_sym_FMOD_Reverb_setActive)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Reverb_setActive = GetFmodExSymbol("?setActive@Reverb@FMOD@@QEAA?AW4FMOD_RESULT@@_N@Z");
#else
		g_sym_FMOD_Reverb_setActive = GetFmodExSymbol("?setActive@Reverb@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Reverb::*&)(bool)>(g_sym_FMOD_Reverb_setActive))(
	    active);
}

FMOD_RESULT F_API FMOD::Reverb::getActive(bool* active)
{
	if (!g_sym_FMOD_Reverb_getActive)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Reverb_getActive = GetFmodExSymbol("?getActive@Reverb@FMOD@@QEAA?AW4FMOD_RESULT@@PEA_N@Z");
#else
		g_sym_FMOD_Reverb_getActive = GetFmodExSymbol("?getActive@Reverb@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Reverb::*&)(bool*)>(g_sym_FMOD_Reverb_getActive))(
	    active);
}

FMOD_RESULT F_API FMOD::Reverb::setUserData(void* userdata)
{
	if (!g_sym_FMOD_Reverb_setUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Reverb_setUserData =
		    GetFmodExSymbol("?setUserData@Reverb@FMOD@@QEAA?AW4FMOD_RESULT@@PEAX@Z");
#else
		g_sym_FMOD_Reverb_setUserData = GetFmodExSymbol("?setUserData@Reverb@FMOD@@QAG?AW4FMOD_RESULT@@PAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Reverb::*&)(void*)>(g_sym_FMOD_Reverb_setUserData))(
	    userdata);
}

FMOD_RESULT F_API FMOD::Reverb::getUserData(void** userdata)
{
	if (!g_sym_FMOD_Reverb_getUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Reverb_getUserData =
		    GetFmodExSymbol("?getUserData@Reverb@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAX@Z");
#else
		g_sym_FMOD_Reverb_getUserData =
		    GetFmodExSymbol("?getUserData@Reverb@FMOD@@QAG?AW4FMOD_RESULT@@PAPAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Reverb::*&)(void**)>(g_sym_FMOD_Reverb_getUserData))(
	    userdata);
}

FMOD_RESULT F_API FMOD::Event::start()
{
	if (!g_sym_FMOD_Event_start)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_start = GetFmodEventSymbol("?start@Event@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_Event_start = GetFmodEventSymbol("?start@Event@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)()>(g_sym_FMOD_Event_start))();
}

FMOD_RESULT F_API FMOD::Event::stop(bool immediate)
{
	if (!g_sym_FMOD_Event_stop)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_stop = GetFmodEventSymbol("?stop@Event@FMOD@@QEAA?AW4FMOD_RESULT@@_N@Z");
#else
		g_sym_FMOD_Event_stop = GetFmodEventSymbol("?stop@Event@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(bool)>(g_sym_FMOD_Event_stop))(immediate);
}

FMOD_RESULT F_API FMOD::Event::getInfo(int* index, char** name, FMOD_EVENT_INFO* info)
{
	if (!g_sym_FMOD_Event_getInfo)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getInfo =
		    GetFmodEventSymbol("?getInfo@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAHPEAPEADPEAUFMOD_EVENT_INFO@@@Z");
#else
		g_sym_FMOD_Event_getInfo =
		    GetFmodEventSymbol("?getInfo@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAHPAPADPAUFMOD_EVENT_INFO@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(int*, char**, FMOD_EVENT_INFO*)>(
			   g_sym_FMOD_Event_getInfo))(index, name, info);
}

FMOD_RESULT F_API FMOD::Event::getState(FMOD_EVENT_STATE* state)
{
	if (!g_sym_FMOD_Event_getState)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getState = GetFmodEventSymbol("?getState@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAI@Z");
#else
		g_sym_FMOD_Event_getState = GetFmodEventSymbol("?getState@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAI@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(FMOD_EVENT_STATE*)>(
			   g_sym_FMOD_Event_getState))(state);
}

FMOD_RESULT F_API FMOD::Event::getParentGroup(EventGroup** group)
{
	if (!g_sym_FMOD_Event_getParentGroup)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getParentGroup =
		    GetFmodEventSymbol("?getParentGroup@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVEventGroup@2@@Z");
#else
		g_sym_FMOD_Event_getParentGroup =
		    GetFmodEventSymbol("?getParentGroup@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVEventGroup@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(EventGroup**)>(
			   g_sym_FMOD_Event_getParentGroup))(group);
}

FMOD_RESULT F_API FMOD::Event::getChannelGroup(ChannelGroup** channelgroup)
{
	if (!g_sym_FMOD_Event_getChannelGroup)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getChannelGroup =
		    GetFmodEventSymbol("?getChannelGroup@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVChannelGroup@2@@Z");
#else
		g_sym_FMOD_Event_getChannelGroup =
		    GetFmodEventSymbol("?getChannelGroup@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVChannelGroup@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(ChannelGroup**)>(
			   g_sym_FMOD_Event_getChannelGroup))(channelgroup);
}

FMOD_RESULT F_API FMOD::Event::setCallback(FMOD_EVENT_CALLBACK callback, void* userdata)
{
	if (!g_sym_FMOD_Event_setCallback)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_setCallback =
		    GetFmodEventSymbol("?setCallback@Event@FMOD@@QEAA?AW4FMOD_RESULT@@P6A?AW43@PEAUFMOD_EVENT@@W4FMOD_"
		                       "EVENT_CALLBACKTYPE@@PEAX22@Z2@Z");
#else
		g_sym_FMOD_Event_setCallback =
		    GetFmodEventSymbol("?setCallback@Event@FMOD@@QAG?AW4FMOD_RESULT@@P6G?AW43@PAUFMOD_EVENT@@W4FMOD_"
		                       "EVENT_CALLBACKTYPE@@PAX22@Z2@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(FMOD_EVENT_CALLBACK, void*)>(
			   g_sym_FMOD_Event_setCallback))(callback, userdata);
}

FMOD_RESULT F_API FMOD::Event::getParameter(const char* name, EventParameter** parameter)
{
	if (!g_sym_FMOD_Event_getParameter)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getParameter =
		    GetFmodEventSymbol("?getParameter@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEBDPEAPEAVEventParameter@2@@Z");
#else
		g_sym_FMOD_Event_getParameter =
		    GetFmodEventSymbol("?getParameter@Event@FMOD@@QAG?AW4FMOD_RESULT@@PBDPAPAVEventParameter@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(const char*, EventParameter**)>(
			   g_sym_FMOD_Event_getParameter))(name, parameter);
}

FMOD_RESULT F_API FMOD::Event::getParameterByIndex(int index, EventParameter** parameter)
{
	if (!g_sym_FMOD_Event_getParameterByIndex)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getParameterByIndex = GetFmodEventSymbol(
		    "?getParameterByIndex@Event@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAPEAVEventParameter@2@@Z");
#else
		g_sym_FMOD_Event_getParameterByIndex = GetFmodEventSymbol(
		    "?getParameterByIndex@Event@FMOD@@QAG?AW4FMOD_RESULT@@HPAPAVEventParameter@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(int, EventParameter**)>(
			   g_sym_FMOD_Event_getParameterByIndex))(index, parameter);
}

FMOD_RESULT F_API FMOD::Event::getNumParameters(int* numparameters)
{
	if (!g_sym_FMOD_Event_getNumParameters)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getNumParameters =
		    GetFmodEventSymbol("?getNumParameters@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_Event_getNumParameters =
		    GetFmodEventSymbol("?getNumParameters@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(int*)>(g_sym_FMOD_Event_getNumParameters))(
	    numparameters);
}

FMOD_RESULT F_API FMOD::Event::getProperty(const char* propertyname, void* value, bool this_instance)
{
	if (!g_sym_FMOD_Event_getProperty)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getProperty =
		    GetFmodEventSymbol("?getProperty@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEBDPEAX_N@Z");
#else
		g_sym_FMOD_Event_getProperty =
		    GetFmodEventSymbol("?getProperty@Event@FMOD@@QAG?AW4FMOD_RESULT@@PBDPAX_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(const char*, void*, bool)>(
			   g_sym_FMOD_Event_getProperty))(propertyname, value, this_instance);
}

FMOD_RESULT F_API FMOD::Event::getPropertyByIndex(int propertyindex, void* value, bool this_instance)
{
	if (g_hasC1Fmod)
	{
		propertyindex = AdjustEventPropertyForOldFmod(propertyindex);
		if (propertyindex < 0)
		{
			return FMOD_ERR_INVALID_PARAM;
		}
	}

	if (!g_sym_FMOD_Event_getPropertyByIndex)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getPropertyByIndex =
		    GetFmodEventSymbol("?getPropertyByIndex@Event@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAX_N@Z");
#else
		g_sym_FMOD_Event_getPropertyByIndex =
		    GetFmodEventSymbol("?getPropertyByIndex@Event@FMOD@@QAG?AW4FMOD_RESULT@@HPAX_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(int, void*, bool)>(
			   g_sym_FMOD_Event_getPropertyByIndex))(propertyindex, value, this_instance);
}

FMOD_RESULT F_API FMOD::Event::setProperty(const char* propertyname, void* value, bool this_instance)
{
	if (!g_sym_FMOD_Event_setProperty)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_setProperty =
		    GetFmodEventSymbol("?setProperty@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEBDPEAX_N@Z");
#else
		g_sym_FMOD_Event_setProperty =
		    GetFmodEventSymbol("?setProperty@Event@FMOD@@QAG?AW4FMOD_RESULT@@PBDPAX_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(const char*, void*, bool)>(
			   g_sym_FMOD_Event_setProperty))(propertyname, value, this_instance);
}

FMOD_RESULT F_API FMOD::Event::setPropertyByIndex(int propertyindex, void* value, bool this_instance)
{
	if (g_hasC1Fmod)
	{
		propertyindex = AdjustEventPropertyForOldFmod(propertyindex);
		if (propertyindex < 0)
		{
			return FMOD_ERR_INVALID_PARAM;
		}
	}

	if (!g_sym_FMOD_Event_setPropertyByIndex)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_setPropertyByIndex =
		    GetFmodEventSymbol("?setPropertyByIndex@Event@FMOD@@QEAA?AW4FMOD_RESULT@@HPEAX_N@Z");
#else
		g_sym_FMOD_Event_setPropertyByIndex =
		    GetFmodEventSymbol("?setPropertyByIndex@Event@FMOD@@QAG?AW4FMOD_RESULT@@HPAX_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(int, void*, bool)>(
			   g_sym_FMOD_Event_setPropertyByIndex))(propertyindex, value, this_instance);
}

FMOD_RESULT F_API FMOD::Event::getNumProperties(int* numproperties)
{
	if (!g_sym_FMOD_Event_getNumProperties)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getNumProperties =
		    GetFmodEventSymbol("?getNumProperties@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAH@Z");
#else
		g_sym_FMOD_Event_getNumProperties =
		    GetFmodEventSymbol("?getNumProperties@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(int*)>(g_sym_FMOD_Event_getNumProperties))(
	    numproperties);
}

FMOD_RESULT F_API FMOD::Event::getCategory(EventCategory** category)
{
	if (!g_sym_FMOD_Event_getCategory)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getCategory =
		    GetFmodEventSymbol("?getCategory@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAVEventCategory@2@@Z");
#else
		g_sym_FMOD_Event_getCategory =
		    GetFmodEventSymbol("?getCategory@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVEventCategory@2@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(EventCategory**)>(
			   g_sym_FMOD_Event_getCategory))(category);
}

FMOD_RESULT F_API FMOD::Event::setVolume(float volume)
{
	if (!g_sym_FMOD_Event_setVolume)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_setVolume = GetFmodEventSymbol("?setVolume@Event@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_Event_setVolume = GetFmodEventSymbol("?setVolume@Event@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(float)>(g_sym_FMOD_Event_setVolume))(
	    volume);
}

FMOD_RESULT F_API FMOD::Event::getVolume(float* volume)
{
	if (!g_sym_FMOD_Event_getVolume)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getVolume = GetFmodEventSymbol("?getVolume@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_Event_getVolume = GetFmodEventSymbol("?getVolume@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(float*)>(g_sym_FMOD_Event_getVolume))(
	    volume);
}

FMOD_RESULT F_API FMOD::Event::setPitch(float pitch, FMOD_EVENT_PITCHUNITS units)
{
	if (g_hasC1Fmod)
	{
		if (!g_sym_FMOD_Event_setPitch)
		{
#ifdef BUILD_64BIT
			g_sym_FMOD_Event_setPitch =
			    GetFmodEventSymbol("?setPitch@Event@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
			g_sym_FMOD_Event_setPitch = GetFmodEventSymbol("?setPitch@Event@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
		}

		// no units parameter
		return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(float)>(g_sym_FMOD_Event_setPitch))(
		    pitch);
	}

	if (!g_sym_FMOD_Event_setPitch)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_setPitch =
		    GetFmodEventSymbol("?setPitch@Event@FMOD@@QEAA?AW4FMOD_RESULT@@MW4FMOD_EVENT_PITCHUNITS@@@Z");
#else
		g_sym_FMOD_Event_setPitch =
		    GetFmodEventSymbol("?setPitch@Event@FMOD@@QAG?AW4FMOD_RESULT@@MW4FMOD_EVENT_PITCHUNITS@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(float, FMOD_EVENT_PITCHUNITS)>(
			   g_sym_FMOD_Event_setPitch))(pitch, units);
}

FMOD_RESULT F_API FMOD::Event::getPitch(float* pitch, FMOD_EVENT_PITCHUNITS units)
{
	if (g_hasC1Fmod)
	{
		if (!g_sym_FMOD_Event_getPitch)
		{
#ifdef BUILD_64BIT
			g_sym_FMOD_Event_getPitch =
			    GetFmodEventSymbol("?getPitch@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
			g_sym_FMOD_Event_getPitch =
			    GetFmodEventSymbol("?getPitch@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
		}

		// no units parameter
		return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(float*)>(
				   g_sym_FMOD_Event_getPitch))(pitch);
	}

	if (!g_sym_FMOD_Event_getPitch)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getPitch =
		    GetFmodEventSymbol("?getPitch@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAMW4FMOD_EVENT_PITCHUNITS@@@Z");
#else
		g_sym_FMOD_Event_getPitch =
		    GetFmodEventSymbol("?getPitch@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAMW4FMOD_EVENT_PITCHUNITS@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(float*, FMOD_EVENT_PITCHUNITS)>(
			   g_sym_FMOD_Event_getPitch))(pitch, units);
}

FMOD_RESULT F_API FMOD::Event::setPaused(bool paused)
{
	if (!g_sym_FMOD_Event_setPaused)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_setPaused = GetFmodEventSymbol("?setPaused@Event@FMOD@@QEAA?AW4FMOD_RESULT@@_N@Z");
#else
		g_sym_FMOD_Event_setPaused = GetFmodEventSymbol("?setPaused@Event@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(bool)>(g_sym_FMOD_Event_setPaused))(paused);
}

FMOD_RESULT F_API FMOD::Event::getPaused(bool* paused)
{
	if (!g_sym_FMOD_Event_getPaused)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getPaused = GetFmodEventSymbol("?getPaused@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEA_N@Z");
#else
		g_sym_FMOD_Event_getPaused = GetFmodEventSymbol("?getPaused@Event@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(bool*)>(g_sym_FMOD_Event_getPaused))(
	    paused);
}

FMOD_RESULT F_API FMOD::Event::setMute(bool mute)
{
	if (!g_sym_FMOD_Event_setMute)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_setMute = GetFmodEventSymbol("?setMute@Event@FMOD@@QEAA?AW4FMOD_RESULT@@_N@Z");
#else
		g_sym_FMOD_Event_setMute = GetFmodEventSymbol("?setMute@Event@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(bool)>(g_sym_FMOD_Event_setMute))(mute);
}

FMOD_RESULT F_API FMOD::Event::getMute(bool* mute)
{
	if (!g_sym_FMOD_Event_getMute)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getMute = GetFmodEventSymbol("?getMute@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEA_N@Z");
#else
		g_sym_FMOD_Event_getMute = GetFmodEventSymbol("?getMute@Event@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(bool*)>(g_sym_FMOD_Event_getMute))(mute);
}

FMOD_RESULT F_API FMOD::Event::set3DAttributes(const FMOD_VECTOR* position, const FMOD_VECTOR* velocity,
                                               const FMOD_VECTOR* orientation)
{
	if (!g_sym_FMOD_Event_set3DAttributes)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_set3DAttributes =
		    GetFmodEventSymbol("?set3DAttributes@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEBUFMOD_VECTOR@@00@Z");
#else
		g_sym_FMOD_Event_set3DAttributes =
		    GetFmodEventSymbol("?set3DAttributes@Event@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_VECTOR@@00@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(const FMOD_VECTOR*, const FMOD_VECTOR*,
	                                                                    const FMOD_VECTOR*)>(
			   g_sym_FMOD_Event_set3DAttributes))(position, velocity, orientation);
}

FMOD_RESULT F_API FMOD::Event::get3DAttributes(FMOD_VECTOR* position, FMOD_VECTOR* velocity, FMOD_VECTOR* orientation)
{
	if (!g_sym_FMOD_Event_get3DAttributes)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_get3DAttributes =
		    GetFmodEventSymbol("?get3DAttributes@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_VECTOR@@00@Z");
#else
		g_sym_FMOD_Event_get3DAttributes =
		    GetFmodEventSymbol("?get3DAttributes@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_VECTOR@@00@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(FMOD_VECTOR*, FMOD_VECTOR*, FMOD_VECTOR*)>(
			   g_sym_FMOD_Event_get3DAttributes))(position, velocity, orientation);
}

FMOD_RESULT F_API FMOD::Event::set3DOcclusion(float directocclusion, float reverbocclusion)
{
	if (!g_sym_FMOD_Event_set3DOcclusion)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_set3DOcclusion =
		    GetFmodEventSymbol("?set3DOcclusion@Event@FMOD@@QEAA?AW4FMOD_RESULT@@MM@Z");
#else
		g_sym_FMOD_Event_set3DOcclusion =
		    GetFmodEventSymbol("?set3DOcclusion@Event@FMOD@@QAG?AW4FMOD_RESULT@@MM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(float, float)>(
			   g_sym_FMOD_Event_set3DOcclusion))(directocclusion, reverbocclusion);
}

FMOD_RESULT F_API FMOD::Event::get3DOcclusion(float* directocclusion, float* reverbocclusion)
{
	if (!g_sym_FMOD_Event_get3DOcclusion)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_get3DOcclusion =
		    GetFmodEventSymbol("?get3DOcclusion@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM0@Z");
#else
		g_sym_FMOD_Event_get3DOcclusion =
		    GetFmodEventSymbol("?get3DOcclusion@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAM0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(float*, float*)>(
			   g_sym_FMOD_Event_get3DOcclusion))(directocclusion, reverbocclusion);
}

FMOD_RESULT F_API FMOD::Event::setReverbProperties(const FMOD_REVERB_CHANNELPROPERTIES* prop)
{
	if (!g_sym_FMOD_Event_setReverbProperties)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_setReverbProperties = GetFmodEventSymbol(
		    "?setReverbProperties@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEBUFMOD_REVERB_CHANNELPROPERTIES@@@Z");
#else
		g_sym_FMOD_Event_setReverbProperties = GetFmodEventSymbol(
		    "?setReverbProperties@Event@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_REVERB_CHANNELPROPERTIES@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(const FMOD_REVERB_CHANNELPROPERTIES*)>(
			   g_sym_FMOD_Event_setReverbProperties))(prop);
}

FMOD_RESULT F_API FMOD::Event::getReverbProperties(FMOD_REVERB_CHANNELPROPERTIES* prop)
{
	if (!g_sym_FMOD_Event_getReverbProperties)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getReverbProperties = GetFmodEventSymbol(
		    "?getReverbProperties@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAUFMOD_REVERB_CHANNELPROPERTIES@@@Z");
#else
		g_sym_FMOD_Event_getReverbProperties = GetFmodEventSymbol(
		    "?getReverbProperties@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_REVERB_CHANNELPROPERTIES@@@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(FMOD_REVERB_CHANNELPROPERTIES*)>(
			   g_sym_FMOD_Event_getReverbProperties))(prop);
}

FMOD_RESULT F_API FMOD::Event::setUserData(void* userdata)
{
	if (!g_sym_FMOD_Event_setUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_setUserData =
		    GetFmodEventSymbol("?setUserData@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAX@Z");
#else
		g_sym_FMOD_Event_setUserData = GetFmodEventSymbol("?setUserData@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(void*)>(g_sym_FMOD_Event_setUserData))(
	    userdata);
}

FMOD_RESULT F_API FMOD::Event::getUserData(void** userdata)
{
	if (!g_sym_FMOD_Event_getUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_Event_getUserData =
		    GetFmodEventSymbol("?getUserData@Event@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAX@Z");
#else
		g_sym_FMOD_Event_getUserData =
		    GetFmodEventSymbol("?getUserData@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAPAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::Event::*&)(void**)>(g_sym_FMOD_Event_getUserData))(
	    userdata);
}

FMOD_RESULT F_API FMOD::EventParameter::getInfo(int* index, char** name)
{
	if (!g_sym_FMOD_EventParameter_getInfo)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_EventParameter_getInfo =
		    GetFmodEventSymbol("?getInfo@EventParameter@FMOD@@QEAA?AW4FMOD_RESULT@@PEAHPEAPEAD@Z");
#else
		g_sym_FMOD_EventParameter_getInfo =
		    GetFmodEventSymbol("?getInfo@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@PAHPAPAD@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::EventParameter::*&)(int*, char**)>(
			   g_sym_FMOD_EventParameter_getInfo))(index, name);
}

FMOD_RESULT F_API FMOD::EventParameter::getRange(float* rangemin, float* rangemax)
{
	if (!g_sym_FMOD_EventParameter_getRange)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_EventParameter_getRange =
		    GetFmodEventSymbol("?getRange@EventParameter@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM0@Z");
#else
		g_sym_FMOD_EventParameter_getRange =
		    GetFmodEventSymbol("?getRange@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@PAM0@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::EventParameter::*&)(float*, float*)>(
			   g_sym_FMOD_EventParameter_getRange))(rangemin, rangemax);
}

FMOD_RESULT F_API FMOD::EventParameter::setValue(float value)
{
	if (!g_sym_FMOD_EventParameter_setValue)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_EventParameter_setValue =
		    GetFmodEventSymbol("?setValue@EventParameter@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_EventParameter_setValue =
		    GetFmodEventSymbol("?setValue@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::EventParameter::*&)(float)>(
			   g_sym_FMOD_EventParameter_setValue))(value);
}

FMOD_RESULT F_API FMOD::EventParameter::getValue(float* value)
{
	if (!g_sym_FMOD_EventParameter_getValue)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_EventParameter_getValue =
		    GetFmodEventSymbol("?getValue@EventParameter@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_EventParameter_getValue =
		    GetFmodEventSymbol("?getValue@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::EventParameter::*&)(float*)>(
			   g_sym_FMOD_EventParameter_getValue))(value);
}

FMOD_RESULT F_API FMOD::EventParameter::setVelocity(float value)
{
	if (!g_sym_FMOD_EventParameter_setVelocity)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_EventParameter_setVelocity =
		    GetFmodEventSymbol("?setVelocity@EventParameter@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_EventParameter_setVelocity =
		    GetFmodEventSymbol("?setVelocity@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::EventParameter::*&)(float)>(
			   g_sym_FMOD_EventParameter_setVelocity))(value);
}

FMOD_RESULT F_API FMOD::EventParameter::getVelocity(float* value)
{
	if (!g_sym_FMOD_EventParameter_getVelocity)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_EventParameter_getVelocity =
		    GetFmodEventSymbol("?getVelocity@EventParameter@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_EventParameter_getVelocity =
		    GetFmodEventSymbol("?getVelocity@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::EventParameter::*&)(float*)>(
			   g_sym_FMOD_EventParameter_getVelocity))(value);
}

FMOD_RESULT F_API FMOD::EventParameter::setSeekSpeed(float value)
{
	if (!g_sym_FMOD_EventParameter_setSeekSpeed)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_EventParameter_setSeekSpeed =
		    GetFmodEventSymbol("?setSeekSpeed@EventParameter@FMOD@@QEAA?AW4FMOD_RESULT@@M@Z");
#else
		g_sym_FMOD_EventParameter_setSeekSpeed =
		    GetFmodEventSymbol("?setSeekSpeed@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::EventParameter::*&)(float)>(
			   g_sym_FMOD_EventParameter_setSeekSpeed))(value);
}

FMOD_RESULT F_API FMOD::EventParameter::getSeekSpeed(float* value)
{
	if (!g_sym_FMOD_EventParameter_getSeekSpeed)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_EventParameter_getSeekSpeed =
		    GetFmodEventSymbol("?getSeekSpeed@EventParameter@FMOD@@QEAA?AW4FMOD_RESULT@@PEAM@Z");
#else
		g_sym_FMOD_EventParameter_getSeekSpeed =
		    GetFmodEventSymbol("?getSeekSpeed@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::EventParameter::*&)(float*)>(
			   g_sym_FMOD_EventParameter_getSeekSpeed))(value);
}

FMOD_RESULT F_API FMOD::EventParameter::setUserData(void* userdata)
{
	if (!g_sym_FMOD_EventParameter_setUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_EventParameter_setUserData =
		    GetFmodEventSymbol("?setUserData@EventParameter@FMOD@@QEAA?AW4FMOD_RESULT@@PEAX@Z");
#else
		g_sym_FMOD_EventParameter_setUserData =
		    GetFmodEventSymbol("?setUserData@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@PAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::EventParameter::*&)(void*)>(
			   g_sym_FMOD_EventParameter_setUserData))(userdata);
}

FMOD_RESULT F_API FMOD::EventParameter::getUserData(void** userdata)
{
	if (!g_sym_FMOD_EventParameter_getUserData)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_EventParameter_getUserData =
		    GetFmodEventSymbol("?getUserData@EventParameter@FMOD@@QEAA?AW4FMOD_RESULT@@PEAPEAX@Z");
#else
		g_sym_FMOD_EventParameter_getUserData =
		    GetFmodEventSymbol("?getUserData@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@PAPAX@Z");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::EventParameter::*&)(void**)>(
			   g_sym_FMOD_EventParameter_getUserData))(userdata);
}

FMOD_RESULT F_API FMOD::EventParameter::keyOff()
{
	if (!g_sym_FMOD_EventParameter_keyOff)
	{
#ifdef BUILD_64BIT
		g_sym_FMOD_EventParameter_keyOff =
		    GetFmodEventSymbol("?keyOff@EventParameter@FMOD@@QEAA?AW4FMOD_RESULT@@XZ");
#else
		g_sym_FMOD_EventParameter_keyOff =
		    GetFmodEventSymbol("?keyOff@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
#endif
	}

	return (this->*reinterpret_cast<FMOD_RESULT (F_API FMOD::EventParameter::*&)()>(
			   g_sym_FMOD_EventParameter_keyOff))();
}
