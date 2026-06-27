#pragma once

#include "CryCommon/CryMath/Cry_Math.h"
#include "CryCommon/CryMath/Cry_Geo.h"

#include "IPlatformSound.h"
#include "SoundBuffer.h"

struct IVisArea;
class CListener;
class CSoundSystem;

// start preloading this amount of meters before the sound is potentially hearable (max_radius)
#define SOUND_PRELOAD_DISTANCE 10

class CSound final : public ISound
{
	struct RadiusSpecs
	{
		float fOriginalMinRadius;
		float fOriginalMaxRadius;
		float fMinRadius;
		float fMaxRadius;
		float fMaxRadius2;         // used to skip playing of sounds if using sound's sphere
		float fMinRadius2;         // used to skip playing of sounds if using sound's sphere
		float fPreloadRadius2;     // used to start preloading
		float fDistanceMultiplier; // used to tweak distance updating on sound events;
	};

	struct VoiceSpecs
	{
		string sCleanEventName;
		float fVolume;
		float fDucking;
		float fRadioRatio;
		float fRadioBackground;
		float fRadioSquelch;
	};

public:
	CSound(CSoundSystem* pSSys);
	virtual ~CSound();

	//// ISound //////////////////////////////////////////////////////
	void Play(float fVolumeScale = 1.0f, bool bForceActiveState = true, bool bSetRatio = true,
	          IEntitySoundProxy* pEntitySoundProxy = NULL);

	bool IsPlaying() const { return (m_bPlaying); } // returns true if the timer has started to play the sound
	bool
	IsPlayingOnChannel() const; // returns true if a channel is used to play the sound and have channel allocated.
	bool IsPlayingVirtual()
	    const;              // returns true if the sound is looping and not stopped.
	                        // it might be too far (clipped) so it will not use a physical channel nor is it audible
	                        // TODO get rid of loop-dependecy. An inactive sound can started playback virtually.
	bool IsLoading() const; // returns true if the Asset is still being loaded
	bool IsLoaded() const;  // returns true if Asset is loaded
	bool IsActive() const;  // returns true if a sound is allocated in the sound library's "audio device"
	bool UnloadBuffer();    // Unloads the soundbuffer of this sound.

	bool IsPlayLengthExpired(float fCurrTime) const;

	void Stop(ESoundStopMode eStopMode = ESoundStopMode_EventFade);
	void SetPaused(bool bPaused);
	bool GetPaused() const;
	bool GetPausedState() const { return m_IsPaused; }

	void Reset(); // completely resets the sound, all information is lost

	//! Fading In/Out - 0.0f is Out 1.0f is In
	virtual void SetFade(const float fFadeGoal, const int nFadeTimeInMS);
	virtual EFadeState GetFadeState() const { return m_eFadeState; }

	virtual void SetSemantic(ESoundSemantic eSemantic) { m_eSemantic = eSemantic; }
	virtual ESoundSemantic GetSemantic() { return m_eSemantic; }

	// virtual void SetFadeTime(const int nFadeTimeInMS) { m_nFadeTimeInMS = nFadeTimeInMS; }
	// virtual int  GetFadeTime() const									{ return
	// m_nFadeTimeInMS; } EFadeState GetFadeState() const                   { return m_eFadeState; }

	void Update(); // central call to update changed of a sound to the platform sound library

	void SetState(ESoundActiveState eState) { m_eState = eState; }
	ESoundActiveState GetState() const { return (m_eState); }

	void SetSoundBufferPtr(CSoundBuffer* pNewSoundBuffer);
	CSoundBuffer* GetSoundBufferPtr() const { return (m_pSoundBuffer); }

	void SetPlatformSoundPtr(IPlatformSound* pPlatformSound) { m_pPlatformSound = pPlatformSound; }
	IPlatformSound* GetPlatformSoundPtr() const { return (m_pPlatformSound); }

	CSoundSystem* GetSoundSystemPtr() const { return (m_pSSys); }

	//! Frees sound library allocation, also stop hear able sound playback
	void Deactivate();

	//! Get name of sound file.
	const char* GetName();

	//! Get unique id of sound.
	const tSoundID GetId() const;
	void SetId(tSoundID SoundID);

	//! Set looping mode of sound.
	void SetLoopMode(bool bLoopMode);
	bool GetLoopMode() const { return (GetFlags() & FLAG_SOUND_LOOP); }

	//! retrieves the currently played sample-pos, in milliseconds or bytes
	unsigned int GetCurrentSamplePos(bool bMilliSeconds) const;

	//! set the currently played sample-pos in bytes or milliseconds
	void SetCurrentSamplePos(unsigned int nPos, bool bMilliSeconds);

	//! sets automatic pitching amount (0-1000)
	void SetPitching(float fPitching);

	// void SetBaseFrequency(int nFreq);

	//! Return frequency of sound.
	int GetBaseFrequency() const;

	void SetFrequency(int nFreq);

	//! Set Minimal/Maximal distances for sound.
	//! Sound is not attenuated below minimal distance and not heared outside of max distance.
	void SetMinMaxDistance(float fMinDist, float fMaxDist);

	//! Sets a distance multiplier so sound event's distance can be tweak (sadly pretty hack feature)
	virtual void SetDistanceMultiplier(const float fMultiplier);

	float GetMinRadius() const { return (m_RadiusSpecs.fMinRadius); }
	float GetMaxDistance() const { return (m_RadiusSpecs.fMaxRadius); }
	float GetSquardedMinRadius() const { return (m_RadiusSpecs.fMinRadius2); }
	float GetSquardedMaxRadius() const { return (m_RadiusSpecs.fMaxRadius2); }
	float GetSquardedPreloadRadius() const { return (m_RadiusSpecs.fPreloadRadius2); }

	void SetAttrib(float fVolume, float fRatio, int nFreq = 1000, bool bSetRatio = true);
	void SetAttrib(const Vec3& pos, const Vec3& speed);

	float GetRatioByDistance(const float fDistance);   // return ratio
	void SetVolume(const float fVolume);               // Sets sound volume. Range: 0-1
	float GetVolume() const { return (m_fMaxVolume); } // Gets sound volume. Range: 0-1
	// int  GetMaxVolume()			{ return (m_nMaxVolume); }
	float GetActiveVolume() const { return (m_fActiveVolume); }

	//! Set panning values (-1.0 left, 0.0 center, 1.0 right)
	virtual void SetPan(float fPan);
	virtual float GetPan() const;

	//! Set 3d panning values (0.0 no directional, 1.0 full 3d directional effect)
	virtual void Set3DPan(float f3DPan);
	virtual float Get3DPan() const;

	void SetRatio(float fRatio);
	float GetRatio() const { return (m_fRatio); }

	//! Set sound source position.
	void SetPosition(const Vec3& vPos);

	//! Get sound source position.
	Vec3 GetPosition() const { return (m_vPosition); }
	Vec3 GetPlaybackPosition() const { return (m_vPlaybackPos); }

	// modify a line sound
	virtual void SetLineSpec(const Vec3& vStart, const Vec3& vEnd);
	virtual bool GetLineSpec(Vec3& vStart, Vec3& vEnd);

	// modify a sphere sound
	virtual void SetSphereSpec(const float fRadius);

	int GetFrequency() const
	{
		if (m_pSoundBuffer)
			return (m_pSoundBuffer->GetInfo()->nBaseFreq);
		return (0);
	}

	//! Set sound pitch. 1000 is default pitch.
	void SetPitch(int nValue);

	//! Define sound code. Angles are in degrees, in range 0-360.
	void SetConeAngles(const float fInnerAngle, const float fOuterAngle);
	void GetConeAngles(float& fInnerAngle, float& fOuterAngle);

	void SetVelocity(const Vec3& vVel);                 //! Set sound source velocity.
	Vec3 GetVelocity(void) const { return (m_vSpeed); } //! Get sound source velocity.

	// void	SetObstruction(const float fObstruction) {m_Obstruction = fObstruction; m_bPropertiesHaveChanged =
	// true;}		//! Set sound obstruction level.
	SObstruction* GetObstruction(void) { return (&m_Obstruction); } //! Get sound obstruction level.
	// void	SetObstructionAccum(const float fObstruction) {m_fObstructionAccumulate = fObstruction;}
	// //! Set accum. sound obstruction level. float GetObstructionAccum( void ) const { return
	// (m_fObstructionAccumulate); }
	// //! Get accum. sound obstruction level.
	void SetPhysicsToBeSkipObstruction(EntityId* pSkipEnts, int nSkipEnts);

	//! Set orientation of sound.
	//! Only relevant when cone angles are specified.
	void SetDirection(const Vec3& vDir);
	Vec3 GetDirection() const { return (m_vDirection); }

	// void	SetLoopPoints(const int iLoopStart, const int iLoopEnd);

	//! compute fmod flags from crysound-flags
	// int GetFModFlags();

	//! load the sound only when it is played
	bool Preload();

	void SetFlags(uint32 nFlags)
	{
		m_bFlagsChanged = true;
		m_nFlags = nFlags;
	}
	uint32 GetFlags() const { return (m_nFlags); }
	bool IsRelative() const { return (m_nFlags & FLAG_SOUND_RELATIVE) ? (true) : (false); }

	// Sets certain sound properties
	// void	SetSoundProperties(float fFadingValue);

	// Add/remove sounds. // This is used in the smart_ptr
	int AddRef() { return ++m_refCount; };
	int Release();

	//! enable fx effects for this sound
	//! _ust be called after each play
	void FXEnable(int nEffectNumber);

	void FXSetParamEQ(float fCenter, float fBandwidth, float fGain);

	// group stuff
	void AddToScaleGroup(int nGroup) { m_nSoundScaleGroups |= (1 << nGroup); }
	void RemoveFromScaleGroup(int nGroup) { m_nSoundScaleGroups &= ~(1 << nGroup); }
	void SetScaleGroup(unsigned int nGroupBits) { m_nSoundScaleGroups = nGroupBits; }

	//! set the maximum distance / the sound will be stopped if the
	//! distance from the listener and this sound is bigger than this max distance
	//	void	SetMaxSoundDistance(float fMaxSoundDistance);

	int GetLengthMs() const; //! returns the size of the stream in ms
	int GetLength() const;   //! returns the size of the stream

	//! set sound priority (0-255)
	void SetSoundPriority(int nSoundPriority);

	//! set soundinfo struct for voice sounds
	void SetVoiceSpecs(const ILocalizationManager::SLocalizedSoundInfo* pSoundInfo);
	VoiceSpecs* GetVoiceSpecs() { return (&m_VoiceSpecs); }

	// bool	FadeIn();
	// bool	FadeOut();
	// float GetCurrentFade() { return (m_fCurrentFade); }

	void AddEventListener(ISoundEventListener* pListener, const char* sWho);
	void RemoveEventListener(ISoundEventListener* pListener);

	//! Fires event for all listeners to this sound.
	void OnEvent(ESoundCallbackEvent event);

	void OnBufferLoaded();
	void OnBufferLoadFailed();
	void OnAssetUnloaded();
	void OnBufferDelete();

	void GetMemoryUsage(class ICrySizer* pSizer) const;

	IVisArea* GetSoundVisArea() const { return (m_pVisArea); }
	void SetSoundVisArea(IVisArea* pVisArea) { m_pVisArea = pVisArea; }
	void InvalidateVisArea()
	{
		m_pVisArea = NULL;
		m_bRecomputeVisArea = true;
	}

	virtual bool IsInCategory(const char* sCategory);

	//////////////////////////////////////////////////////////////////////////
	// Information
	//////////////////////////////////////////////////////////////////////////

	// Gets and Sets Parameter defined in the enumAssetParam list
	bool GetParam(enumSoundParamSemantics eSemantics, ptParam* pParam) const;
	bool SetParam(enumSoundParamSemantics eSemantics, ptParam* pParam);

	// Gets and Sets Parameter defined by string and float value, returns the index of that parameter
	virtual int GetParam(const char* sParameter, float* fValue, bool bOutputWarning = true) const;
	virtual int SetParam(const char* sParameter, float fValue, bool bOutputWarning = true);

	// Gets and Sets Parameter defined by index and float value
	virtual bool GetParam(int nIndex, float* fValue, bool bOutputWarning = true) const;
	virtual bool SetParam(int nIndex, float fValue, bool bOutputWarning = true);

	void StartPlayback(CListener* pListener, float& fDistanceToListener, float& fDistSquared);
	bool CanBeCulled(CListener* pListener, float& fDistanceToListener, float& fDistSquared);

private:
	//! Update the position (called when directional-attenuation-parameters change)
	void UpdatePositionAndVelocity(); // just calls 	SetPosition(m_vPosition);

	//! Sets the ActiveVolume to the PlatformSound
	void UpdateActiveVolume();

	//! updates the obstruction level of that sound, will be called from within Update()
	void ProcessObstruction(CListener* pListener, bool bImmediatly);
	void CalculateObstruction(); // evaluates the RWIs and changes the obstruction value

	// group stuff
	float CalcSoundVolume(float fSoundVolume, float fVolumeScale) const;

	bool UpdateSoundProperties();

	friend class CSoundSystem;
	void SetMinMaxDistance_i(float fMinDist, float fMaxDist);

public:
	static CSoundSystem* m_pSSys;
	EntityId m_SoundProxyEntityId;

	SObstruction m_Obstruction;

	_smart_ptr<CSoundBuffer> m_pEventGroupBuffer;

	ESoundStopMode m_eStopMode;

private:
	//_smart_ptr<ISGElement>	m_pSGSound;

	tSoundID m_nSoundID;
	_smart_ptr<CSoundBuffer> m_pSoundBuffer;
	_smart_ptr<IPlatformSound> m_pPlatformSound;

	ESoundActiveState m_eState;
	EFadeState m_eFadeState;
	ESoundSemantic m_eSemantic;

	string m_sSoundName;

	// old group stuff
	unsigned int m_nSoundScaleGroups;

	int m_nFxChannel;
	int m_nStartOffset;

	CTimeValue m_tPlayTime;
	float m_fSyncTimeout;
	float m_fLastObstructionUpdate;

	uint32 m_nFlags;
	int m_refCount;

	// Volume related
	float m_fMaxVolume;    // theoretical maximum volume
	float m_fActiveVolume; // volume that is currently set to the platform sound
	float m_fRatio;        // additional scale factor used for software distance rollof
	float m_fFadeGoal;
	float m_fFadeCurrent;
	int m_nFadeTimeInMS;

	float m_fPan;
	int m_nCurrPitch;
	int m_nRelativeFreq;

	float m_HDRA;      // additional attenuation based on louder sounds nearby
	float m_fPitching; // relative random pitching of sound (to avoid flanging if multiple sounds are played
	                   // simultaneously)
	// float m_fFadingValue;
	// float m_fCurrentFade;
	float m_fPostLoadRatio;

	// Radius Specs
	RadiusSpecs m_RadiusSpecs;

	// Direction
	Vec3 m_vDirection;
	float m_fInnerAngle;
	float m_fOuterAngle;

	// Voice Specs
	VoiceSpecs m_VoiceSpecs;

	// Vectors
	Vec3 m_vPosition;    // position of the sound object, maybe relative coordinates
	Vec3 m_vPlaybackPos; // this is the playback position (may differ from m_position on binocular/portals/relative
	                     // sounds)
	Vec3 m_vSpeed;

	// Line Sound quick feature
	Lineseg m_Line;
	bool m_bLineSound;

	// Sphere Sound quick feature
	Vec3 m_vCenter;
	float m_fSphereRadius;
	bool m_bSphereSound;

	// short		m_nSectorId,m_nBuildingId;
	IVisArea* m_pVisArea;

	// update flags // TODO put these in a struct or bitfield
	bool m_bVolumeHasChanged;
	bool m_bPositionHasChanged;
	bool m_bPlayerPositionHasChanged;
	bool m_bLoopModeHasChanged;
	bool m_bFlagsChanged;

	bool m_bPlaying;
	bool m_IsPaused;
	bool m_bPlayAfterLoad;
	bool m_bPostLoadForceActiveState;
	bool m_bPostLoadSetRatio;
	bool m_bRecomputeVisArea;
	bool m_bRefCountGuard;

	int m_nSoundPriority;

	typedef std::vector<SSoundEventListenerInfo> TEventListenerInfoVector;
	// typedef std::vector<ISoundEventListener*>	TEventListenerVector;

	// typedef std::set<ISoundEventListener*> Listeners;
	TEventListenerInfoVector m_listeners;
	TEventListenerInfoVector m_listenersToBeRemoved;
};
