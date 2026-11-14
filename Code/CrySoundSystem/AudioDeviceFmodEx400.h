#pragma once

#include <map>
#include <memory>
#include <string>

#include "CryCommon/CrySoundSystem/ISound.h"
#include "CryCommon/CrySystem/ILog.h"

#include "FMOD.h"
#include "IAudioDevice.h"
#include "SoundSystemCommon.h"

class CSoundSystem;
class CSoundBuffer;
class CSound;
class CPlatformSoundFmodEx400Event;
class CPlatformSoundFmodEx400;

class CWavebankFmodEx400 final : public IWavebank
{
	std::string m_name;
	std::string m_path;
	SWavebankInfo m_info{};

public:
	explicit CWavebankFmodEx400(const char* name) : m_name(name ? name : "") {}

	const char* GetName() override { return m_name.c_str(); }
	const char* GetPath() override { return m_path.c_str(); }
	void SetPath(const char* path) override { m_path = path ? path : ""; }
	SWavebankInfo* GetInfo() override { return &m_info; }

	void AddInfo(SWavebankInfo& otherInfo) override
	{
		m_info.nFileSize = max(m_info.nFileSize, otherInfo.nFileSize);
		m_info.nTimesAccessed = max(m_info.nTimesAccessed, otherInfo.nTimesAccessed);
		m_info.nMemCurrentlyInByte = otherInfo.nMemCurrentlyInByte;
		m_info.nMemPeakInByte = max(m_info.nMemPeakInByte, otherInfo.nMemPeakInByte);
	}
};

class CAudioDeviceFmodEx400 final : public IAudioDevice
{
public:
	CAudioDeviceFmodEx400(void* hWnd);
	~CAudioDeviceFmodEx400();

	bool InitDevice(CSoundSystem* pSoundSystem) override;
	bool ShutDownDevice() override;

	void* GetSoundLibrary() override { return m_pAPISystem; }
	void* GetEventSystem() override { return m_pEventSystem; }

	void GetOutputHandle(void** pHandle, EOutputHandle* HandleType) override;

	void GetInitSettings(AudioDeviceSettings* InitSettings) override;
	void SetInitSettings(AudioDeviceSettings* InitSettings) override;

	// major system attrib has been changed del all sounds clean system start
	bool ResetAudio();
	// this is called every frame to update all listeners and must be called *before* SubSystemUpdate()
	bool UpdateListeners();
	// example would be CS_update()
	bool Update();
	int GetMemoryStats();
	int GetNumberSoundsPlaying();
	// returns percent of cpu usage 1.0 is 100%
	float GetCpuUsage() override;
	// sets sound system global frequency
	bool SetFrequency(int newFreq);
	// compute memory-consumption, returns rough estimate in MB
	int GetMemoryUsage(class ICrySizer* pSizer);

	// accesses wavebanks
	IWavebank* GetWavebank(int nIndex) override;
	IWavebank* GetWavebank(const char* sWavebankName) override;
	int GetWavebankCount() override { return m_wavebanks.size(); }

	CSoundBuffer* CreateSoundBuffer(const SSoundBufferProps& BufferProps);
	IPlatformSound* CreatePlatformSound(CSound* pSound, const char* sEventName);
	bool RemovePlatformSound(IPlatformSound* pPlatformSound);

	// check for multiple record devices and expose them, write name to sName pointer
	bool GetRecordDeviceInfo(const int nRecordDevice, char* sName, const int nNameLength);

	// sound system settings
	// get enum for type of info and ptr type of info gotton
	bool GetParam(enumAudioDeviceParamSemantics enumtype, ptParam* pParam);
	// the bool returns if this info is available on this type
	bool SetParam(enumAudioDeviceParamSemantics enumtype, ptParam* pParam);

	bool IsEax(void);

	// return handle if Project is available
	FMOD::EventProject* LoadProjectFile(const string& sProjectPath, const string& sProjectName);

	// writes output to screen in debug
	void DrawInformation(IRenderer* pRenderer, float xpos, float ypos, int nSoundInfo) override;

	void FindLostEvent(); // hack for looping weapon sound MP bug

private:
	FMOD::System* m_pAPISystemDSound;
	FMOD::System* m_pAPISystem;
	FMOD::EventSystem* m_pEventSystem;
	FMOD_RESULT m_ExResult;
	CSoundSystem* m_pSoundSystem;

	CTimeValue m_tLastUpdate;

	int m_nfModDriverType;
	int m_nCurrentMemAlloc;
	int m_nMaxMemAlloc;
	unsigned int m_nMemoryStatInc;
	FMOD_CAPS m_nFMODDriverCaps;
	// int m_nEaxStatusFMOD;

	// init vars
	AudioDeviceSettings m_InitSettings;

	int m_nHardware3DChannels;
	int m_nHardware2DChannels;
	int m_nTotalHardwareChannelsAvail;
	// int m_nSoftwareChannels;
	FMOD_SPEAKERMODE m_nSpeakerMode;
	int m_nTotalActiveSounds; // for fmod total hardware and software channels -- for xennon total voices
	int m_fMasterVolume;      // volume between 0-1, 1 being max
	bool m_bSystemPaused;
	bool m_bGetSystemStats;
	bool m_bHaveListenerAttributes;
	int m_nSystemFrequency;
	bool m_bMuteStatus;
	bool m_bDopplerStatus;
	int m_nMixingBuffSize;

	struct SProjectFile
	{
		string sProjectName;
		FMOD::EventProject* ProjectHandle;
	};

	typedef std::vector<SProjectFile> VecProjectFiles;
	typedef VecProjectFiles::iterator VecProjectFilesIter;
	VecProjectFiles m_VecLoadedProjectFiles;

	std::map<std::string, std::unique_ptr<CWavebankFmodEx400>, std::less<>> m_wavebanks;
	TFixedResourceName m_sFullWaveBankName;

	int32 m_nCountProject;
	int32 m_nCountGroup;
	int32 m_nCountEvent;

	void UpdateGroupEventCount();
	void GroupEventCount(FMOD::EventGroup* pGroup, FMOD::EventProject* pProject);

	std::vector<CPlatformSoundFmodEx400Event*> m_UnusedPlatformSoundEvents;
	std::vector<CPlatformSoundFmodEx400*> m_UnusedPlatformSounds;

	void* m_pHWnd; // for windows which window is active
	// float m_fpctCpuUsed;

	void FmodErrorOutput(const char* sDescription, ILog::ELogType LogType = ILog::eMessage);
};
