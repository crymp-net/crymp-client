#pragma once

#include <map>
#include <set>
#include <memory>
#include <string>
#include <vector>

#include "CryCommon/CryCore/CryArray.h"
#include "CryCommon/CryCore/smartptr.h"
#include "CryCommon/CryCore/STLPoolAllocator.h"

#include "SaltBufferArray.h"
#include "Sound.h"
#include "SoundAssetManager.h"
#include "SoundSystemCommon.h"

struct ISoundMoodManager;
struct SoundBuffer;
struct IStreamEngine;
class CMoodManager;

#define MAX_SOUNDIDS 4096

#define SETSCALEBIT(bit) (1 << bit)

// ParseSoundNameReturnCodes
#define PSNRC_EVENT 0x00000001
#define PSNRC_GROUP 0x00000002
#define PSNRC_WAV 0x00000004
#define PSNRC_CONVERTEDTOWAV 0x00000008
#define PSNRC_VOICE 0x00000010
#define PSNRC_KEY 0x00000020

enum EDebuggingModes
{
	SOUNDSYSTEM_DEBUG_NONE,           //!< no debugging output
	SOUNDSYSTEM_DEBUG_SIMPLE,         //!< simple debugging to console
	SOUNDSYSTEM_DEBUG_DETAIL,         //!< more detail debugging to console
	SOUNDSYSTEM_DEBUG_EVENTLISTENERS, //!< logs event listeners
	SOUNDSYSTEM_DEBUG_RESERVE3,       //!< reserved 3
	SOUNDSYSTEM_DEBUG_FMOD_SIMPLE,    //!< simple FMOD debugging into fmod.log
	SOUNDSYSTEM_DEBUG_FMOD_COMPLEX,   //!< complex FMOD debugging into fmod.log
	SOUNDSYSTEM_DEBUG_FMOD_ALL,       //!< all FMOD debugging into fmod.log
	SOUNDSYSTEM_DEBUG_RECORD_COMMANDS //!< records FMOD calls in SoundCommands.xml
};

struct SSoundContainer final : public std::vector<_smart_ptr<CSound>>
{

public:
	SSoundContainer() {};

	void Push(CSound* pSound) { push_back(pSound); }

	void AddUnique(CSound* pSound)
	{
		if (!Find(pSound))
			Push(pSound);
	}

	bool EraseUnique(CSound* pSound)
	{
		iterator ItEnd = end();
		for (iterator Iter = begin(); Iter != ItEnd; ++Iter)
		{
			CSound* pContainerSound = (*Iter);
			if (pSound == pContainerSound)
			{
				iterator ItLast = end() - 1;
				if (Iter != ItLast)
					(*Iter) = (*ItLast);

				pop_back();
				return true;
			}
		}
		return false;
	}

	bool Find(const CSound* pSound)
	{
		iterator ItEnd = end();
		for (iterator Iter = begin(); Iter != ItEnd; ++Iter)
		{
			CSound* pContainerSound = (*Iter);
			if (pSound == pContainerSound)
				return true;
		}
		return false;
	}
};

class CListener final : public IListener
{
private:
	Matrix34 TransformationNewest;
	Matrix34 TransformationSet;

	float fUnderwater;
	IVisArea* pVisArea;
	Vec3 vVelocity;

	CListener(const CListener&) {}

public:
	ListenerID nListenerID;
	EntityId nEntityID;
	PodArray<IVisArea*>* pVisAreas;
	float fRecordLevel;
	bool bActive;
	bool bMoved;
	bool bRotated;
	bool bDirty;
	bool bInside;

	// Constructor
	CListener()
	{
		nListenerID = LISTENERID_INVALID;
		vVelocity = Vec3(0, 0, 0);
		fRecordLevel = 0.0f;
		bActive = false;
		bMoved = false;
		bRotated = false;
		bDirty = true;
		bInside = false;
		fUnderwater = 1.0f;
		nEntityID = 0;
		pVisArea = NULL;
		TransformationSet.SetIdentity();
		TransformationNewest.SetIdentity();

		pVisAreas = new (PodArray<IVisArea*>);
		// VisAreas.PreAllocate(MAX_VIS_AREAS);
	}
	~CListener()
	{
		if (pVisAreas)
		{
			pVisAreas->clear();
			delete pVisAreas;
		}
	}

	virtual ListenerID GetID() const { return nListenerID; }

	virtual EntityId GetEntityID() const { return nEntityID; }

	virtual bool GetActive() const { return bActive; }

	virtual void SetActive(bool bNewActive) { bActive = bNewActive; }

	virtual void SetRecordLevel(float fRecord) { fRecordLevel = fRecord; }

	virtual float GetRecordLevel() { return fRecordLevel; }

	virtual Vec3 GetPosition() const { return TransformationSet.GetColumn3(); }

	virtual void SetPosition(const Vec3 Position)
	{
		TransformationNewest.SetColumn(3, Position);
		bDirty = true;
	}

	virtual Vec3 GetForward() const
	{
		Vec3 ForwardVec(0, 1, 0); // Forward.
		ForwardVec = TransformationSet.TransformVector(ForwardVec);
		ForwardVec.Normalize();
		return ForwardVec;
	}

	virtual Vec3 GetTop() const
	{
		Vec3 TopVec(0, 0, 1); // Up.
		TopVec = TransformationSet.TransformVector(TopVec);
		TopVec.Normalize();
		return TopVec;
	}

	virtual Vec3 GetVelocity() const { return vVelocity; }

	virtual void SetVelocity(Vec3 vVel) { vVelocity = vVel; }

	virtual void SetMatrix(const Matrix34 newTransformation)
	{
		Vec3 vOldPos = TransformationSet.GetColumn3();
		Vec3 vNewPos = newTransformation.GetColumn3();
		bMoved = !vOldPos.IsEquivalent(vNewPos, 0.01f);
		bRotated = !TransformationSet.IsEquivalent(newTransformation, 0.01f);

		if (bMoved || bRotated)
		{
			TransformationNewest = newTransformation;
			bDirty = true;
		}
	}

	virtual Matrix34 GetMatrix() const { return TransformationSet; }

	virtual float GetUnderwater() const { return fUnderwater; }

	virtual void SetUnderwater(const float fUnder) { fUnderwater = fUnder; }

	virtual IVisArea* GetVisArea() const { return pVisArea; }

	virtual void SetVisArea(IVisArea* pVArea) { pVisArea = pVArea; }

	void MarkAsSet()
	{
		bDirty = false;
		TransformationSet = TransformationNewest;
	}
};

class CSoundProfileInfo final : public ISoundProfileInfo
{
	string m_sSoundName;
	SSoundProfileInfo m_SoundInfo;

public:
	explicit CSoundProfileInfo(const char* sSoundName) { m_sSoundName = sSoundName; }

	const char* GetName() override { return m_sSoundName.c_str(); }
	SSoundProfileInfo* GetInfo() override { return &m_SoundInfo; }

	void AddInfo(SSoundProfileInfo& SoundInfo) override
	{
		m_SoundInfo.nTimesPlayed = max(m_SoundInfo.nTimesPlayed, SoundInfo.nTimesPlayed);
		m_SoundInfo.nTimesPlayedOnChannel =
		    max(m_SoundInfo.nTimesPlayedOnChannel, SoundInfo.nTimesPlayedOnChannel);
		m_SoundInfo.nMemorySize = max(m_SoundInfo.nMemorySize, SoundInfo.nMemorySize);
		m_SoundInfo.nPeakSpawn = max(m_SoundInfo.nPeakSpawn, SoundInfo.nPeakSpawn);
	}
};

typedef std::vector<CSound*> SoundVec;
typedef SoundVec::iterator SoundVecIter;

typedef std::vector<IMicrophone*> MicroVec;
typedef MicroVec::iterator MicroVecIter;
typedef MicroVec::const_iterator MicroVecIterConst;

typedef std::vector<CListener*> tVecListeners;
typedef tVecListeners::iterator tVecListenersIter;
typedef tVecListeners::const_iterator tVecListenersIterConst;

//////////////////////////////////////////////////////////////////////////////////////////////
// Sound system interface
class CSoundSystem final : public CSoundSystemCommon, public ISystemEventListener
{
	friend class CSound;
	friend class CAudioDeviceFmodEx400;

protected:
	virtual ~CSoundSystem();

public:
	CSoundSystem(void* hWnd, IAudioDevice* pAudioDevice);

	IAudioDevice* GetIAudioDevice() const;
	IReverbManager* GetIReverbManager() const;
	ISoundMoodManager* GetIMoodManager() const;
	ISoundAssetManager* GetISoundAssetManager() const;

	// Register listener to the sound.
	virtual void AddEventListener(ISoundSystemEventListener* pListener, bool bOnlyVoiceSounds);
	virtual void RemoveEventListener(ISoundSystemEventListener* pListener);

	virtual void GetOutputHandle(void** pHandle, EOutputHandle* HandleType) const;

	//! DSP unit callback for sfx-lowpass filter
	void* DSPUnit_SFXFilter_Callback(void* pOriginalBuffer, void* pNewBuffer, int nLength);

	//! retrieve sfx-filter dsp unit
	// CS_DSPUNIT* GetDSPUnitFilter() { return m_pDSPUnitSFXFilter; }

	bool IsOK() { return m_bOK; }

	//!	Initialize the soundsystem after AudioDevice was initialized
	bool Init();

	//!	Release the sound system
	void Release();

	//!	Update the sound system
	void Update(ESoundUpdateMode UpdateMode);

	/*! Create a music-system. You should only create one music-system at a time.
	 */
	IMusicSystem* CreateMusicSystem();

	void SetSoundActiveState(ISound* pSound, ESoundActiveState State);
	virtual void SetMasterPitch(float fPitch);

	//! Register new playing sound that should be auto stopped when it ends.
	// void RegisterOneShotSound( CSound *pSound );
	// void UnregisterOneShotSound( CSound *pSound );

	tSoundID CreateSoundID();              // returns an unused SoundID which also registers it
	bool DestroySoundID(tSoundID SoundID); // unregisters an used SoundID and frees it

	void RemoveSound(tSoundID nSoundID);
	void SetMasterVolume(float fVol);
	void SetMasterVolumeScale(float fScale, bool bForceRecalc = true);

	virtual float GetSFXVolume() { return m_fSFXVolume; }

	virtual void SetMovieFadeoutVolume(const float movieFadeoutVolume)
	{
		m_fMovieFadeoutVolume = movieFadeoutVolume;
	}

	virtual float GetMovieFadeoutVolume() const { return m_fMovieFadeoutVolume; }

	void RemoveReference(CSound*);

	ISound* GetSound(tSoundID nSoundID) const;

	EPrecacheResult Precache(const char* sGroupAndSoundName, uint32 nSoundFlags, uint32 nPrecacheFlags);
	CSoundBuffer* PrecacheSound(const char* sGroupAndSoundName, uint32 nSoundFlags, uint32 nPrecacheFlags);

	virtual ISound* CreateSound(const char* sGroupAndSoundName, uint32 nFlags);
	virtual ISound* CreateLineSound(const char* sGroupAndSoundName, uint32 nFlags, const Vec3& vStart,
	                                const Vec3& VEnd);
	virtual ISound* CreateSphereSound(const char* sGroupAndSoundName, uint32 nFlags, const float fRadius);

	// quick shortcut to get individual Group/Sound Names, return PSNRC flag
	int ParseGroupString(TFixedResourceName& sGroupAndEventName, TFixedResourceName& sProjectPath,
	                     TFixedResourceName& sProjectName, TFixedResourceName& sGroupName,
	                     TFixedResourceName& sEventName, TFixedResourceName& sKeyName);

	bool SetListener(const ListenerID nListenerID, const Matrix34& matOrientation, const Vec3& vVel, bool bActive,
	                 float fRecordLevel);

	virtual void SetListenerEntity(ListenerID nListenerID, EntityId nEntityID);

	// Listener Management
	ListenerID CreateListener();
	bool RemoveListener(ListenerID nListenerID);
	ListenerID GetClosestActiveListener(Vec3 vPosition) const;
	IListener* GetListener(ListenerID nListenerID);
	IListener* GetNextListener(ListenerID nListenerID);
	uint32 GetNumActiveListeners() const { return m_vecListeners.size(); }
	float GetSqDistanceToClosestListener(CSound* pSound);

	//! get the current area the listener is in
	// IVisArea*	GetListenerArea()			{ return (m_pVisArea); }

	//! Sets minimal priority for sound to be played.
	int SetMinSoundPriority(int nPriority);
	int GetMinSoundPriority() { return m_nMinSoundPriority; };

	/*! to be called when something changes in the environment which could affect
	sound occlusion, for example a door closes etc.
	*/
	void RecomputeSoundOcclusion(bool bRecomputeListener, bool bForceRecompute, bool bReset = false);

	void FadeOutSound(CSound* pSound);

	//! Stop all sounds and music
	bool Silence(bool bStopLoopingSounds, bool bUnloadData);

	bool DeactivateAudioDevice();
	bool ActivateAudioDevice();

	//! pause all sounds
	virtual void Pause(bool bPause, bool bResetVolume = false);
	virtual bool IsPaused() { return (m_bSoundSystemPause); }

	//! Mute/unmute all sounds
	void Mute(bool bMute);

	//! reset the sound system (between loading levels)
	void Reset();

	//! Check for EAX support.
	bool IsEAX(void);

	//! Set EAX listener environment.
	// bool SetEaxListenerEnvironment(SOUND_REVERB_PRESETS nPreset, CRYSOUND_REVERB_PROPERTIES *pProps=NULL, int
	// nFlags=0);

	//! Registers an EAX Preset Area with the current blending weight (0-1) as being active
	//! morphing of several EAX Preset Areas is done internally
	//! Registering the same Preset multiple time will only overwrite the old one
	// bool RegisterWeightedEaxEnvironment(const char *sPreset=NULL, CRYSOUND_REVERB_PROPERTIES *pProps=NULL, bool
	// bFullEffectWhenInside=false, int nFlags=0);

	//! Updates an EAX Preset Area with the current blending ratio (0-1)
	// bool UpdateWeightedEaxEnvironment(const char *sPreset=NULL, float fWeight=0.0);

	//! Unregisters an active EAX Preset Area
	// bool UnregisterWeightedEaxEnvironment(const char *sPreset=NULL);

	//! Gets current EAX listener environment.
	// bool GetCurrentEaxEnvironment(int &nPreset, CRYSOUND_REVERB_PROPERTIES &Props);

	bool SetGroupScale(int nGroup, float fScale);

	//! Sets the options for a group of sounds to animate its behavior
	// bool SetSoundGroupProperties(size_t eSoundGroup, SSoundGroupProperties *pSoundGroupProperties);

	//! Gets the options for a group of sounds to animate its behavior
	// bool GetSoundGroupProperties(size_t eSoundGroup, SSoundGroupProperties &pSoundGroupProperties);
	// bool GetSoundGroupPropertiesChanged() {return m_bSoundGroupPropertiesChanged;}

	//! Will set speaker config
	void SetSpeakerConfig();

	//! get memory usage info
	void GetSoundMemoryUsageInfo(int* nCur, int* nMax) const;

	//! get number of voices playing
	int GetUsedVoices() const;

	//! get cpu-usuage
	float GetCPUUsage() const;

	//! get music-volume
	float GetMusicVolume() const;

	//! sets parameters for directional attenuation (for directional microphone effect); set fConeInDegree to 0 to
	//! disable the effect
	void CalcDirectionalAttenuation(const Vec3& Pos, const Vec3& Dir, const float fConeInRadians);

	//! returns the maximum sound-enhance-factor to use it in the binoculars as "graphical-equalizer"...
	float GetDirectionalAttenuationMaxScale() { return m_fDirAttMaxScale; }

	//! returns if directional attenuation is used
	bool UsingDirectionalAttenuation() { return (m_fDirAttCone != 0.0f); }

	//! remove a sound
	void RemoveBuffer(SSoundBufferProps& sn);

	//! sets the weather condition that affect acoustics
	bool SetWeatherCondition(float fWeatherTemperatureInCelsius, float fWeatherHumidityAsPercent,
	                         float fWeatherInversion);

	//! gets the weather condition that affect acoustics
	bool GetWeatherCondition(float& fWeatherTemperatureInCelsius, float& fWeatherHumidityAsPercent,
	                         float& fWeatherInversion);

	//! compute memory-consumption
	virtual void GetMemoryUsage(class ICrySizer* pSizer) const;
	virtual int GetMemoryUsageInMB();

	// set special music effects volume
	void SetMusicEffectsVolume(float fVolume) { m_fMusicEffectVolume = fVolume; }

	void BufferLoaded() { ++m_nBuffersLoaded; }
	void BufferUnloaded() { --m_nBuffersLoaded; }

	void TraceMemoryUsage(int nMemUsage) { m_MemUsage += nMemUsage; }

	//! Profiling Sounds
	virtual ISoundProfileInfo* GetSoundInfo(int nIndex);
	virtual ISoundProfileInfo* GetSoundInfo(const char* sSoundName);
	virtual int GetSoundInfoCount() { return static_cast<int>(m_SoundInfos.size()); }

	//! Returns FALSE if sound can be stopped.
	bool ProcessActiveSound(CSound* pSound);
	bool ProcessReverb();

	void LockResources();
	void UnlockResources();

	bool IsEnabled() const { return m_bSoundSystemEnabled; }

	void RenderAudioInformation();

	virtual bool GetRecordDeviceInfo(const int nRecordDevice, char* sName, int nNameLength);

	virtual IMicrophone* CreateMicrophone(const unsigned int nRecordDevice, const unsigned int nBitsPerSample,
	                                      const unsigned int nSamplesPerSecond,
	                                      const unsigned int nBufferSizeInSamples);

	virtual bool RemoveMicrophone(IMicrophone* pMicrophone);

	virtual ISound* CreateNetworkSound(INetworkSoundListener* pNetworkSoundListener,
	                                   const unsigned int nBitsPerSample, const unsigned int nSamplesPerSecond,
	                                   const unsigned int nBufferSizeInSamples, EntityId PlayerID);
	virtual void RemoveNetworkSound(ISound* pSound);

	// interface ISystemEventListener -------------------------------------------------------
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	// ------------------------------------------------------------------------------------

	// for serialization
	virtual void Serialize(TSerialize ser);

	bool IsInPauseCall() const { return m_bIsInPauseCall; }

	bool IsEventUsedInPlatformSound(void* pHandle); // hack for looping weapon sound MP bug

public:
	// IMicrophone * m_pMicrophone;
	// ISound * m_pNetworkSound;
	// uint32* m_pData;

	// A network sound listener
	// bool FillDataBuffer(	unsigned int nBitsPerSample, unsigned int nSamplesPerSecond,
	//		unsigned int nNumSamples, void* pData )
	//{
	//	memcpy(pData, m_pData, nNumSamples);
	//	return true;
	//}

	//// A single microphone stream
	// bool ReadDataBuffer(	const unsigned int nBitsPerSample, const unsigned int nSamplesPerSecond,
	//		const unsigned int nNumSamples, void* pData )
	//{
	//	//memset(m_pData, 1, nNumSamples);
	//	memcpy(m_pData, pData, nNumSamples);
	//	return true;
	// }

	IStreamEngine* m_pStreamEngine; // is used by SoundBuffer also

	// for debugging
	std::map<FILE*, string> m_FMODFilesMap;

	// static ISoundAssetManager* m_pSoundAssetManager;

	static TFixedResourceName g_sProjectPath;
	static TFixedResourceName g_sProjectName;
	static TFixedResourceName g_sGroupName;
	static TFixedResourceName g_sEventName;
	static TFixedResourceName g_sFileName;
	static TFixedResourceName g_sKeyName;
	// static TFixedResourceName g_sStringLocal;

private:
	// platform dependent implementation of the AudioDevice
	IAudioDevice* m_pAudioDevice;
	CTimeValue m_tUpdateAudioDevice;

	bool m_bSoundSystemPause;
	bool m_bOK;
	// bool  m_bInside;
	bool m_bIsInPauseCall;

	int m_nBuffersLoaded;

	IReverbManager* m_pReverbManager;
	bool m_bNotCalledYetEAX;
	int m_nEaxStatus;

	//! if sound is potentially hear able
	bool IsSoundPH(CSound* pSound);
	void AddToSoundSystem(CSound* pSound, int nFlags);
	void DeactivateSound(CSound* pSound);
	void ProcessInactiveSounds();
	void ProcessOneShotSounds();
	void ProcessLoopedSounds();
	void ProcessHDRS();

	ISound* LoadSound(const char* szFile, uint32 nFlags, uint32 nPrecacheFlags);

	// CSound class generation and removal
	CSound* GetNewSoundObject();
	bool RemoveSoundObject(CSound* pSound);

	void SetMinMaxRadius(CSound& Sound, float fMin, float fMax);

	SoundVec m_UnusedSoundObjects;

	// AssetManager
	ISoundAssetManager* m_pSoundAssetManager;
	CTimeValue m_tUpdateSoundAssetManager;

	string m_sGroupPath;

	SoundVec m_AllSoundsArray;

	int m_nLastInactiveSoundsIndex;
	SSoundContainer m_InactiveSounds;
	SSoundContainer m_LoopedSounds;
	SSoundContainer m_OneShotSounds;
	SSoundContainer m_stoppedSoundToBeDeleted;

	MicroVec m_Microphones;

	CTimeValue m_tUpdateSounds;

	CMoodManager* m_pMoodManager;

	CSaltBufferArray<uint16, uint16, MAX_SOUNDIDS>
	    m_SoundSaltBuffer; // used to create new entity ids (with uniqueid=salt)

	typedef std::set<tSoundID, std::less<tSoundID>,
	                 stl::STLPoolAllocator<tSoundID, stl::PoolAllocatorSynchronizationSinglethreaded>>
	    TSoundIDSet;
	TSoundIDSet m_currentSoundIDs;

	// Listener Management
	tVecListeners m_vecListeners;

	// sfx-filter stuff //////////////////////////////////////////////////////
	//	CS_DSPUNIT *m_pDSPUnitSFXFilter;
	float m_fDSPUnitSFXFilterCutoff;
	float m_fDSPUnitSFXFilterResonance;
	float m_fDSPUnitSFXFilterLowLVal;
	float m_fDSPUnitSFXFilterBandLVal;
	float m_fDSPUnitSFXFilterLowRVal;
	float m_fDSPUnitSFXFilterBandRVal;
	//////////////////////////////////////////////////////////////////////////

	// group stuff
	float m_fSoundScale[MAX_SOUNDSCALE_GROUPS];

	// new SoundGroup
	// SoundGroupVec m_vecSoundGroups;
	// bool					m_bSoundGroupPropertiesChanged;

	//

	int m_nMuteRefCnt;
	int m_nDialogRefCnt;

	// temp array for sound visareas to prevent runtime allocs
	PodArray<IVisArea*> m_SoundTempVisAreas;

	char m_szEmptyName[32];

	float m_fSFXVolume;
	float m_fGameSFXVolume;
	float m_fMusicVolume;
	float m_fGameMusicVolume;
	float m_fDialogVolume;
	float m_fGameDialogVolume;
	float m_fMovieFadeoutVolume;

	float m_fSFXResetVolume;
	float m_fMusicEffectVolume;
	int m_nSampleRate;
	int m_MemUsage;

	// this is used for the binocular-feature where sounds are heard according to their screen-projection
	Vec3 m_DirAttPos;
	Vec3 m_DirAttDir;
	float m_fDirAttCone;
	float m_fDirAttMaxScale;
	bool m_bNeedSoundZoomUpdate;

	// profiling
	std::map<std::string, std::unique_ptr<CSoundProfileInfo>, std::less<>> m_SoundInfos;

	// int		m_nSpeakerConfig;
	int m_nReverbType;
	int m_nMinSoundPriority;
	bool m_bResetVolume;

	bool m_bUpdateNeededMoodManager;
	bool m_bSoundSystemEnabled;

	float m_fWeatherTemperatureInCelsius;
	float m_fWeatherHumidityAsPercent;
	float m_fWeatherInversion;
	float m_fWeatherAirAbsorptionMultiplyer;

	SSoundBufferProps m_TempBufferProps;

	bool m_bDelayPrecaching;
	// std::vector<SSoundBufferProps> m_DelayedPrecaches;
	SoundBufferPropsMap m_DelayedPrecaches;

	// bool SoundGroupChanged(ESOUNDGROUP eSoundGroup);

	// Out of convenience and because this is rarely used map is used
	struct SEventListener
	{
		SEventListener(ISoundSystemEventListener* pListener, bool bOnlyVoiceSounds)
		    : pListener(pListener), bOnlyVoiceSounds(bOnlyVoiceSounds)
		{
		}
		bool operator==(const SEventListener& other) const { return other.pListener == pListener; }
		bool operator==(const ISoundSystemEventListener* pOtherListener) const
		{
			return pListener == pOtherListener;
		}

		ISoundSystemEventListener* pListener;
		bool bOnlyVoiceSounds;
	};
	typedef std::vector<SEventListener> TSoundSystenEventListenerVector;

	TSoundSystenEventListenerVector m_EventListeners;
	TSoundSystenEventListenerVector m_EventListenersTemp;

	//! Fires event for all listeners to this sound.
	void OnEvent(ESoundSystemCallbackEvent event, ISound* pSound);

	// int m_nMemTest;
};
