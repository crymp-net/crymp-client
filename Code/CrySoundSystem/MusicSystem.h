#pragma once

#include <map>
#include <vector>

#include "CryCommon/CryCore/smartptr.h"
#include "CryCommon/CrySoundSystem/IMusicSystem.h"
#include "CryCommon/CrySystem/IXml.h"
#include "CryCommon/CrySystem/TimeValue.h"
#include "Library/StringTools.h"

#include "MusicPattern.h"
#include "RandGen.h"
#include "IPlatformSound.h" // tSoundHandle
#include "MusicCVars.h"
#include "FMOD.h"

// Forward declarations
struct ISystem;
struct IAudioDevice;

// Smart Pattern-Ptr
typedef _smart_ptr<CMusicPatternInstance> TPatternInstancePtr;

// Info-Structure for automated fading
struct SMusicPatternPlayInfo
{
	SMusicPatternPlayInfo()
	{
		nLayer = MUSICLAYER_MAIN;
		pPatternInstance = NULL;
		eBlendType = EBlend_None;
		ePlayingType = EPlaying_Loop;
		pRefCount = NULL;
		fPhase = 1.0;
		fFadeTime = 0.0; // use default fade time...
		nSamplesToWait = 0;
		bRefCountAdjusted = false;
		bJustStarted = true;
	}
	~SMusicPatternPlayInfo() { pPatternInstance = NULL; }
	unsigned int nLayer;
	TPatternInstancePtr pPatternInstance;
	EBlendingType eBlendType;
	EPlayingType ePlayingType;
	int* pRefCount;
	// internal
	double fPhase;    // phase for fading (0-1)
	double fFadeTime; // time in seconds to fade
	int nSamplesToWait;
	bool bRefCountAdjusted;
	CTimeValue startTime;
	bool bJustStarted;
};

//////////////////////////////////////////////////////////////////////////
// A couple of typedefs for various stl-containers
//////////////////////////////////////////////////////////////////////////

typedef std::vector<string> TStringVec;
typedef TStringVec::iterator TStringVecIt;
typedef TStringVec::const_iterator TStringVecItConst;

typedef std::vector<SMusicPatternPlayInfo> TPatternPlayInfoVec;
typedef TPatternPlayInfoVec::iterator TPatternPlayInfoVecIt;
typedef TPatternPlayInfoVec::const_iterator TPatternPlayInfoVecItConst;

//////////////////////////////////////////////////////////////////////////
// Structure to collect/handle mood-events
//////////////////////////////////////////////////////////////////////////

struct SMoodEventInfo
{
	bool operator<(const SMoodEventInfo& b) const
	{
		if (_stricmp(sMood.c_str(), b.sMood.c_str()) < 0)
			return true;
		return false;
	}
	string sMood; // name of mood
	float fTime;  // time of consecutive event-occurance
};

typedef std::set<SMoodEventInfo> TMoodEventSet;
typedef TMoodEventSet::iterator TMoodEventSetIt;

//////////////////////////////////////////////////////////////////////////
// The MusicSystem itself
//////////////////////////////////////////////////////////////////////////

class CMusicSystem final : public IMusicSystem, public CMusicCVars
{
protected:
	// random generator
	CPseudoRandGen m_RandGen;

	// notification sink
	IMusicSystemSink* m_pSink;

	// sound-playback params
	float m_fMasterVolume;
	int m_nSampleRate;
	float m_fLatency;

	// bytes/sample in output stream
	int m_nBytesPerSample;

	// fmod stream data
	tAssetHandle m_pStream;
	IAudioDevice* m_pAudioDevice;
	void* m_pChannel;

	// stream playback delay
	bool m_bStreamDelayCalced;
	unsigned int m_nStreamDelay;

	FMOD_RESULT m_ExResult;

	// states
	bool m_bDataLoaded;
	bool m_bPause;
	bool m_bPlaying;
	bool m_bBridging; // bridging in progress
	bool m_bCurrPatternIsBridge;
	bool m_bForcePatternChange; // change pattern asap (next fadepoint), do not play to the end (last fadepoint)

	// status
	SMusicSystemStatus m_Status;

	// all patterns
	std::map<std::string, CMusicPattern, StringTools::ComparatorNoCase> m_patterns;

	// current main patterns

	TPatternInstancePtr m_pTrack1;
	int m_Track1Type;

	TPatternInstancePtr m_pTrack2;
	int m_Track2Type;

	TPatternInstancePtr m_pNextTrack;
	int m_NextTrackType;

	int m_nLayeredRhythmicPatterns;
	int m_nLayeredIncidentalPatterns;

	// temporary mix-buffer
	void* m_pMixBuffer;

	// array of currently playing patterns
	TPatternPlayInfoVec m_vecPlayingPatterns;

	// all themes (moods inside)
	TThemeMap m_mapThemes;

	// current theme/mood
	SMusicTheme* m_pCurrTheme;
	SMusicTheme* m_pNextTheme; // in case a theme change occured, this is the new theme... NULL otherwise
	SMusicMood* m_pCurrMood;
	SMusicMood* m_pNextMood;

	// Timer
	CTimeValue m_MoodTime;
	CTimeValue m_ThemeTime;
	CTimeValue m_ThemeSetTime;
	int m_nDelayThemeInSec;

	CTimeValue m_tLastUpdate;

	// mood event arrays
	TMoodEventSet m_setMoodEvents;      // active mood events
	TMoodEventSet m_setFrameMoodEvents; // mood events arrived last frame

	// default mood
	// SMusicMood *m_pDefaultMood;
	float m_fDefaultMoodTime; // current time 'till default-mood-timeout
	string m_sDefaultMood;    // override of theme-specific default-mood (set by SetDefaultMood); set to "" to use
	                          // theme-specific

	// fading
	double m_fCurrCrossfadeTime;

	bool m_bEnableEventProcessing;
	int m_musicEnable;

	XmlNodeRef m_musicStateNode;

private:
protected:
	virtual ~CMusicSystem();
	bool Init();
	void Shutdown();
	bool StartPlaying();
	bool StopPlaying();
	void Silence();
	void FadeOutAllSecondaryLayers(double fFadeTime);

	// Pattern
	bool FlushPatterns();
	void PushPatternToMixList(SMusicPatternPlayInfo& PlayInfo);
	bool UpdateCurrentPatternSet(SMusicMood* pMood, int nSamples, bool bAllowChange);
	bool ChoosePatternSet(SMusicMood* pMood, const bool bUseSameIndex = false);
	CMusicPattern* AddPattern(const char* pszName, const char* pszFilename);
	CMusicPattern* FindPattern(const char* pszName);
	CMusicPatternInstance* GetPatternInstance(const char* pszPattern);
	CMusicPatternInstance* ChooseBridge(SMusicTheme* pCurrTheme, SMusicTheme* pNewTheme);
	CMusicPatternInstance* ChoosePattern(SMusicMood* pMood, const int nLayer);

	// Mood
	void EnterDefaultMood();
	bool SetMood(SMusicMood* pNewMood, const bool bPlayFromStart = true, const bool bForceChange = true);
	SMusicMood* GetMood(SMusicTheme* pTheme, const char* pszMood) const;
	SMusicMood* GetDefaultMood(SMusicTheme* pTheme) const;

	// Theme
	bool SetTheme(SMusicTheme* pNewTheme, bool bForceChange);

	// Stream
	void AdjustMixStreamsRefCount(int nSamples);
	void MixStreams(void* pBuffer, int nLength);
	bool FadeStream(SMusicPatternPlayInfo& PlayInfo, void* pBuffer, int nSamples);

	// Callbacks
	signed char StreamingCallback(tAssetHandle pStream, void* pBuffer, int nLength);

	friend FMOD_RESULT F_CALLBACK _StreamingCallback(FMOD_SOUND* sound, void* data, unsigned int datalen);

public:
	CMusicSystem(IAudioDevice* pAudioDevice);

	// System
	void Release();
	void ReleaseData();
	void Unload();
	IMusicSystemSink* SetSink(IMusicSystemSink* pSink);
	bool SetData(struct SMusicInfo::Data* pMusicData);
	void Update();

	// Information
	int GetBytesPerSample() { return m_nBytesPerSample; }
	SMusicSystemStatus* GetStatus(); // retrieve status of music-system... dont keep returning pointer !

	//! compute memory-consumption
	void GetMemoryUsage(class ICrySizer* pSizer) const;
	bool StreamOGG();
	void LogMsg(const int nVerbosity, const char* pszFormat, ...) PRINTF_PARAMS(1, 2);

	// Playback
	void Pause(bool bPause);
	void EnableEventProcessing(bool bEnable) { m_bEnableEventProcessing = bEnable; }

	// Theme
	virtual bool SetTheme(const char* pszTheme, bool bForceChange = true, bool bKeepMood = true,
	                      int nDelayInSec = -1);
	virtual bool EndTheme(EThemeFadeType ThemeFade = EThemeFade_FadeOut, int nForceEndLimitInSec = 10,
	                      bool bEndEverything = false);
	const char* GetTheme() const
	{
		if (m_pCurrTheme)
			return m_pCurrTheme->sName.c_str();
		return "";
	}
	IStringItVec* GetThemes() const;
	virtual CTimeValue GetThemeTime() const;

	// Mood
	bool SetMood(const char* pszMood, const bool bPlayFromStart = true, const bool bForceChange = false);
	bool SetDefaultMood(const char* pszMood);
	const char* GetMood() const
	{
		if (m_pCurrMood)
			return m_pCurrMood->sName.c_str();
		return "";
	}
	IStringItVec* GetMoods(const char* pszTheme) const;
	bool AddMusicMoodEvent(const char* pszMood, float fTimeout);
	virtual CTimeValue GetMoodTime() const;

	//////////////////////////////////////////////////////////////////////////
	//! Load music data from XML.
	//! @param bAddAdata if true data from XML will be added to currently loaded music data.
	bool LoadFromXML(const char* sFilename, bool bAddData, bool bReplaceData);

	//////////////////////////////////////////////////////////////////////////
	// Editing support.
	//////////////////////////////////////////////////////////////////////////
	virtual void UpdatePattern(SMusicInfo::Pattern* pPattern);
	virtual void RenamePattern(const char* sOldName, const char* sNewName);
	virtual void PlayPattern(const char* sPattern, bool bStopPrevious, bool bPlaySynched);
	virtual void DeletePattern(const char* sPattern);

	virtual bool StopLastPattern(const char* sPattern);
	virtual bool SeekLastPattern(const char* sPattern, const float position);
	virtual bool GetLastPatternStartTime(const char* sPattern, CTimeValue& startTime);

	virtual void PlayStinger();

	// writes output to screen in debug
	virtual void DrawInformation(IRenderer* pRenderer, float xpos, float ypos);

	// for serialization
	virtual void Serialize(TSerialize ser);
	virtual bool SerializeInternal(bool bSave);

private:
	//////////////////////////////////////////////////////////////////////////
	// Loading.
	//////////////////////////////////////////////////////////////////////////
	void LoadPatternFromXML(XmlNodeRef node, SPatternDef* pPattern);
	void LoadMoodFromXML(XmlNodeRef& node, SMusicMood* pMood);
	void LoadThemeFromXML(XmlNodeRef& node, SMusicTheme* pTheme);
	void UpdatePatternDef(SPatternDef* pPattern);

	SMusicPatternPlayInfo* GetPlayingPatternInfo(const int nLayer, const EBlendingType BlendType);
	bool IsPatternPlaying(const TPatternInstancePtr pPattern);

	bool PlayStart(EBlendingType eBlendType);
};

//////////////////////////////////////////////////////////////////////////
// String-vector-class to iterate data from other DLLs
//////////////////////////////////////////////////////////////////////////

class CStringItVec final : public IStringItVec
{
public:
	CStringItVec(TStringVec& Vec)
	{
		m_nRefCount = 0;
		m_Vec = Vec;
		MoveFirst();
	};
	bool IsEnd() { return (m_itVec == m_Vec.end()); };
	const char* Next() { return IsEnd() ? "" : (*m_itVec++).c_str(); };
	void MoveFirst() { m_itVec = m_Vec.begin(); };
	void AddRef() { m_nRefCount++; }
	void Release()
	{
		--m_nRefCount;
		if (m_nRefCount <= 0)
		{
			delete this;
		}
	};

protected:
	int m_nRefCount;
	TStringVec m_Vec;
	TStringVecIt m_itVec;
};
