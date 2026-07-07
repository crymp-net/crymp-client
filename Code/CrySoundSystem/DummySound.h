#pragma once

#include "CryCommon/CrySoundSystem/IReverbManager.h"

#include "SoundSystemCommon.h"
#include "DummyMusic.h"

class CDummyListener final : public IListener
{
private:
	CDummyListener(const CDummyListener&) {}

public:
	Matrix34 TransformationNewest;
	Matrix34 TransformationSet;

	float fUnderwater;
	IVisArea* pVisArea;
	Vec3 vVelocity;
	ListenerID nListenerID;
	EntityId nEntityID;
	PodArray<IVisArea*>* pVisAreas;
	float fRecordLevel;
	bool bActive;
	bool bMoved;
	bool bRotated;
	bool bDirty;
	bool bInside;

	CDummyListener()
	{
		nListenerID = LISTENERID_INVALID;
		vVelocity = Vec3(0, 0, 0);
		fRecordLevel = 0.0f;
		bActive = true;
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

	~CDummyListener()
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

	virtual void SetMatrix(const Matrix34 newTransformation) { TransformationNewest = newTransformation; }

	virtual Matrix34 GetMatrix() const { return TransformationSet; }

	virtual float GetUnderwater() const { return fUnderwater; }

	virtual void SetUnderwater(const float fUnder) { fUnderwater = fUnder; }

	virtual IVisArea* GetVisArea() const { return pVisArea; }

	virtual void SetVisArea(IVisArea* pVArea) { pVisArea = pVArea; }
};

class CDummySound final : public ISound
{
public:
	CDummySound() { m_refCount = 0; }

	~CDummySound() {}

	//// ISound //////////////////////////////////////////////////////
	bool IsPlaying() const { return false; }
	bool IsPlayingVirtual() const { return false; }
	bool IsLoaded() const { return true; }
	bool IsLoading() const { return false; }
	bool UnloadBuffer() { return true; }
	void Play(float fVolumeScale = 1.0f, bool bForceActiveState = true, bool bSetRatio = true,
	          IEntitySoundProxy* pEntitySoundProxy = NULL)
	{
	}
	void Stop(ESoundStopMode eStopMode = ESoundStopMode_EventFade) {}
	void SetPaused(bool bPaused) {}
	bool GetPaused() const { return false; }

	//! Fading In/Out - 0.0f is Out 1.0f is In
	virtual void SetFade(const float fFadeGoal, const int nFadeTimeInMS) {}
	virtual EFadeState GetFadeState() const { return eFadeState_None; }

	virtual void SetSemantic(ESoundSemantic eSemantic) {}
	virtual ESoundSemantic GetSemantic() { return eSoundSemantic_None; }

	// virtual void SetFadeTime(const int nFadeTimeInMS) {}
	// virtual int  GetFadeTime() const { return 0; }

	void SetName(const char* szName) {}
	const char* GetName() { return "dummy sound"; }
	const tSoundID GetId() const { return INVALID_SOUNDID; }
	void SetId(tSoundID SoundID) {}
	void SetLoopMode(bool bLoop) {}
	// unsigned int GetCurrentSamplePos(){return 0;}
	void SetPitching(float fPitching) {}
	// void	SetBaseFrequency(int nFreq){}
	int GetBaseFrequency() { return 44100; }
	void SetFrequency(int nFreq) {}
	void SetMinMaxDistance(float fMinDist, float fMaxDist) {}
	float GetMaxDistance() const { return 1.0f; }
	void SetAttrib(float fVolume, int nFreq = 1000, bool bSetRatio = true) {}
	void SetAttrib(const Vec3& pos, const Vec3& speed) {}
	void SetRatio(float fRatio) {}
	void SetPosition(const Vec3& pos) {}
	Vec3 GetPosition() const { return {}; }

	virtual void SetLineSpec(const Vec3& vStart, const Vec3& vEnd) {}
	virtual bool GetLineSpec(Vec3& vStart, Vec3& vEnd)
	{
		vStart = {};
		vEnd = {};
		return false;
	}

	virtual void SetSphereSpec(const float fRadius) {}
	void SetSpeed(const Vec3& speed) {}
	const Vec3& GetSpeed() { return m_vDummy; }
	// group stuff
	void AddToScaleGroup(int nGroup) {}
	void RemoveFromScaleGroup(int nGroup) {}
	void SetScaleGroup(unsigned int nGroupBits) {}

	int GetFrequency() const { return (0); }
	void SetPitch(int nValue) {}

	//! Set panning values (-1.0 left, 0.0 center, 1.0 right)
	virtual void SetPan(float fPan) {}
	virtual float GetPan() const { return 0.0f; }

	//! Set 3d panning values (0.0 no directional, 1.0 full 3d directional effect)
	virtual void Set3DPan(float f3DPan) {}
	virtual float Get3DPan() const { return 0.0f; }

	void SetMaxSoundDistance(float fMaxSoundDistance) {}
	void SetMinMaxDistance(float, float, float) {}
	void SetDistanceMultiplier(const float) {}
	void SetConeAngles(const float, const float) {}
	void GetConeAngles(float&, float&) {}
	void SetVolume(const float) {}
	float GetVolume() const { return (0); }
	void SetVelocity(const Vec3& vVel) {}
	Vec3 GetVelocity() const { return (Vec3(0, 0, 0)); }
	// void	SetObstruction(const float fObstruction) {}			//! Set sound obstruction level.
	SObstruction* GetObstruction(void) { return NULL; } //! Get sound obstruction struct.
	void SetPhysicsToBeSkipObstruction(EntityId* pSkipEnts, int nSkipEnts) {}

	void SetDirection(const Vec3& vDir) {}
	Vec3 GetDirection() const { return (Vec3(0, 0, 0)); }
	// void  SetLoopPoints(const int iLoopStart, const int iLoopEnd) { };
	bool IsRelative() const { return false; };
	bool RegisterInIndoor() { return false; };
	void SetSoundPriority(int nSoundPriority) {};
	bool Preload() { return true; }
	// Sets certain sound properties
	// void SetSoundProperties(float fFadingValue) {} ;

	void SetFlags(uint32 nFlags) {};
	uint32 GetFlags() const { return 0; };

	void AddFlags(uint32 nFlags) {}

	// Add/remove sounds. // This is used in the smart_ptr
	int AddRef() { return ++m_refCount; };
	int Release()
	{
		int ref = --m_refCount;
		return ref;
	};

	// put in to give music an effects volume
	// void SetMusicEffectsVolume(float v ){float s=v;}
	void SetMusicEffectsVolume(float v) {}

	//! enable fx effects for this sound
	//! must be called after each play
	void FXEnable(int nEffectNumber) {}
	void FXSetParamEQ(float fCenter, float fBandwidth, float fGain) {}

	//! retrieves the currently played sample-pos, in milliseconds or bytes
	unsigned int GetCurrentSamplePos(bool bMilliSeconds = false) const { return (0); }
	//! set the currently played sample-pos in bytes or milliseconds
	void SetCurrentSamplePos(unsigned int nPos, bool bMilliSeconds) {}

	//! returns the size of the stream in ms
	int GetLengthMs() const { return (0); }

	//! returns the size of the stream in bytes
	int GetLength() const { return (0); }
	void AddEventListener(ISoundEventListener* pListener, const char* sWho) {};
	void RemoveEventListener(ISoundEventListener* pListener) {};

	virtual bool IsInCategory(const char* sCategory) { return false; }

	//////////////////////////////////////////////////////////////////////////
	// Information
	//////////////////////////////////////////////////////////////////////////

	// Gets and Sets Parameter defined in the enumAssetParam list
	bool GetParam(enumSoundParamSemantics eSemantics, ptParam* pParam) const { return false; }
	bool SetParam(enumSoundParamSemantics eSemantics, ptParam* pParam) { return false; }

	// Gets and Sets Parameter defined by string and float value, returns the index of that parameter
	virtual int GetParam(const char* sParameter, float* fValue, bool bOutputWarning) const { return (-1); }
	virtual int SetParam(const char* sParameter, float fValue, bool bOutputWarning) { return (-1); }

	// Gets and Sets Parameter defined by index and float value
	virtual bool GetParam(int nIndex, float* fValue, bool bOutputWarning) const { return false; }
	virtual bool SetParam(int nIndex, float fValue, bool bOutputWarning) { return false; }

private:
	Vec3 m_vDummy;
	int m_refCount;
};

class CDummySoundSystem final : public CSoundSystemCommon
{
public:
	CDummySoundSystem(void* hWnd) : CSoundSystemCommon()
	{
		g_nDummySound = 1;
		m_DummyListener.bActive = false;
	}
	~CDummySoundSystem() {}

	bool IsOK() { return true; }

	//// ISoundSystem ///////////////////////////////////////////////
	bool Init() { return true; }
	void Release() {}
	void Update(ESoundUpdateMode UpdateMode);

	IMusicSystem* CreateMusicSystem() { return new CDummyMusicSystem(); }
	IAudioDevice* GetIAudioDevice() const { return NULL; }
	ISoundMoodManager* GetIMoodManager() const { return NULL; }
	IReverbManager* GetIReverbManager() const { return NULL; }

	// Register listener to the sound.
	virtual void AddEventListener(ISoundSystemEventListener* pListener, bool bOnlyVoiceSounds) {}
	virtual void RemoveEventListener(ISoundSystemEventListener* pListener) {}

	// ISound* LoadSound(const char *szFile, uint32 nFlags) { return &m_sndDummy; }
	void RemoveSound(int nSoundID) {}
	ISound* GetSound(tSoundID nSoundID) const { return (ISound*)&m_sndDummy; }
	void SetMasterVolume(float fVol) {}
	void SetMasterVolumeScale(float fScale, bool bForceRecalc = false) {}
	virtual float GetSFXVolume() { return 1.0f; }
	virtual void SetMovieFadeoutVolume(const float movieFadeoutVolume) {}
	virtual float GetMovieFadeoutVolume() const { return 1.0f; }
	void SetSoundActiveState(ISound* pSound, ESoundActiveState State) {}
	void SetMasterPitch(float fPitch) {}
	void AddSoundFlags(int nSoundID, uint32 nFlags) {}

	void GetOutputHandle(void** pHandle, EOutputHandle* HandleType) const
	{
		*pHandle = NULL;
		*HandleType = eOUTPUT_MAX;
	}
	// void		PlaySound(tSoundID nSoundID) {}

	EPrecacheResult Precache(const char* sGroupAndSoundName, uint32 nSoundFlags, uint32 nPrecacheFlags)
	{
		return ePrecacheResult_OK;
	}
	virtual ISound* CreateSound(const char* sGroupAndSoundName, uint32 nFlags) { return NULL; }
	virtual ISound* CreateLineSound(const char* sGroupAndSoundName, uint32 nFlags, const Vec3& vStart,
	                                const Vec3& VEnd)
	{
		return NULL;
	}
	virtual ISound* CreateSphereSound(const char* sGroupAndSoundName, uint32 nFlags, const float fRadius)
	{
		return NULL;
	}

	bool SetListener(const ListenerID nListenerID, const Matrix34& matOrientation, const Vec3& vVel, bool bActive,
	                 float fRecordLevel)
	{
		return true;
	}
	virtual void SetListenerEntity(ListenerID nListenerID, EntityId nEntityID) {}
	ListenerID CreateListener() { return LISTENERID_INVALID; }
	bool RemoveListener(ListenerID nListenerID) { return true; }
	ListenerID GetClosestActiveListener(Vec3 vPosition) const { return LISTENERID_INVALID; }
	IListener* GetListener(ListenerID nListenerID) { return &m_DummyListener; }
	IListener* GetNextListener(ListenerID nListenerID) { return NULL; }
	uint32 GetNumActiveListeners() const { return 1; }

	bool PlayMusic(const char* szFileName) { return false; }
	void StopMusic() {}
	bool Silence(bool bStopLoopingSounds, bool bUnloadData) { return true; }
	bool DeactivateAudioDevice() { return true; }
	bool ActivateAudioDevice() { return true; }

	virtual void Pause(bool bPause, bool bResetVolume = false) {}
	virtual bool IsPaused() { return false; }
	void Mute(bool bMute) {}

	//! Check for EAX support.
	bool IsEAX(void) { return false; }

	//! Set EAX listener environment.
	bool SetEaxListenerEnvironment(int nPreset, CRYSOUND_REVERB_PROPERTIES* pProps = NULL, uint32 nFlags = 0)
	{
		return false;
	}

	// added by Tomas
	//! Registers an EAX Preset Area as being active
	//! morphing of several EAX Preset Areas is done internally
	//! Registering the same Preset multiple time will only overwrite the old one
	// bool RegisterWeightedEaxEnvironment(const char *sPreset=NULL, CRYSOUND_REVERB_PROPERTIES *pProps=NULL, bool
	// bFullEffectWhenInside=false, int nFlags=0) { return(false); }

	//! Updates an EAX Preset Area with the current blending ratio (0-1)
	// bool UpdateWeightedEaxEnvironment(const char *sPreset=NULL, float fWeight=0.0) { return(false); }

	//! Unregisters an active EAX Preset Area
	// bool UnregisterWeightedEaxEnvironment(const char *sPreset=NULL) { return(false); }

	//! Gets current EAX listener environment.
	// bool GetCurrentEaxEnvironment(int &nPreset, CRYSOUND_REVERB_PROPERTIES &Props) { nPreset=0;return true;}

	//! sets the weather condition that affect acoustics
	bool SetWeatherCondition(float fWeatherTemperatureInCelsius, float fWeatherHumidityAsPercent,
	                         float fWeatherInversion)
	{
		return false;
	}

	//! gets the weather condition that affect acoustics
	bool GetWeatherCondition(float& fWeatherTemperatureInCelsius, float& fWeatherHumidityAsPercent,
	                         float& fWeatherInversion)
	{
		return false;
	}

	//! Sets the options for a group of sounds to animate its behavior
	// bool SetSoundGroupProperties(size_t eSoundGroup, SSoundGroupProperties *pSoundGroupProperties) {return
	// false;}

	//! Sets the options for a group of sounds to animate its behavior
	// bool GetSoundGroupProperties(size_t eSoundGroup, SSoundGroupProperties &pSoundGroupProperties) {return
	// false;}

	bool SetGroupScale(int nGroup, float fScale) { return true; }
	void RecomputeSoundOcclusion(bool bRecomputeListener, bool bForceRecompute, bool bReset) {}

	//! get memory usage info
	void GetSoundMemoryUsageInfo(int* nCurrentMemory, int* nMaxMemory) const { *nCurrentMemory = *nMaxMemory = 0; }

	void SetMusicEffectsVolume(float) {}

	int GetUsedVoices() const { return 0; }
	float GetCPUUsage() const { return 0.0f; }
	float GetMusicVolume() const { return 0.0f; }
	void CalcDirectionalAttenuation(const Vec3& Pos, const Vec3& Dir, const float fConeInRadians) {}
	float GetDirectionalAttenuationMaxScale() { return 0.0f; }
	bool UsingDirectionalAttenuation() { return false; }
	virtual void GetMemoryUsage(class ICrySizer* pSizer) const {}
	virtual int GetMemoryUsageInMB() { return 0; }

	//! get the current area the listener is in
	// virtual IVisArea	*GetListenerArea() { return(NULL); }

	int SetMinSoundPriority(int nPriority) { return 0; };

	void LockResources() {};
	void UnlockResources() {};

	void TraceMemoryUsage(int nMemUsage) {};

	//! Profiling Sounds
	virtual ISoundProfileInfo* GetSoundInfo(int nIndex) { return NULL; }
	virtual ISoundProfileInfo* GetSoundInfo(const char* sSoundName) { return NULL; }
	virtual int GetSoundInfoCount() { return 0; }

	virtual bool GetRecordDeviceInfo(const int nRecordDevice, char* sName, int nNameLength) { return false; }
	virtual IMicrophone* CreateMicrophone(const unsigned int nRecordDevice, const unsigned int nBitsPerSample,
	                                      const unsigned int nSamplesPerSecond,
	                                      const unsigned int nBufferSizeInSamples)
	{
		return NULL;
	}
	virtual bool RemoveMicrophone(IMicrophone* pMicrophone) { return false; }
	virtual ISound* CreateNetworkSound(INetworkSoundListener* pNetworkSoundListener,
	                                   const unsigned int nBitsPerSample, const unsigned int nSamplesPerSecond,
	                                   const unsigned int nBufferSizeInSamples, EntityId PlayerID)
	{
		return NULL;
	}
	virtual void RemoveNetworkSound(ISound* pSound) {}

	virtual void Serialize(TSerialize ser) {}

private:
	CDummySound m_sndDummy;
	CDummyListener m_DummyListener;
};
