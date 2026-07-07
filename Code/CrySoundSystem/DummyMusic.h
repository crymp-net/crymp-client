#pragma once

#include "CryCommon/CrySoundSystem/IMusicSystem.h"
#include "MusicCVars.h"

class CDummyMusicSystem final : public IMusicSystem, public CMusicCVars
{
protected:
	CTimeValue m_TimeValue;

public:
	CDummyMusicSystem() {}
	virtual ~CDummyMusicSystem() {}
	void Release() { delete this; }
	void Update() {}

	int GetBytesPerSample() { return 4; }
	IMusicSystemSink* SetSink(IMusicSystemSink* pSink) { return NULL; }
	bool SetData(SMusicInfo::Data* pMusicData) { return true; };
	void Unload() {}
	void Pause(bool bPause) {}
	void EnableEventProcessing(bool bEnable) {}

	// theme stuff
	bool SetTheme(const char* pszTheme, bool bForceChange = true, bool bKeepMood = true, int nDelayInMS = 0)
	{
		return true;
	}
	virtual bool EndTheme(EThemeFadeType ThemeFade = EThemeFade_FadeOut, int nForceEndLimitInSec = 10,
	                      bool bEndEverything = true)
	{
		return true;
	};
	const char* GetTheme() const { return ""; }
	virtual CTimeValue GetThemeTime() const { return m_TimeValue; }

	// Mood stuff
	bool SetMood(const char* pszMood, const bool bPlayFromStart, const bool bForceChange) { return true; }
	bool SetDefaultMood(const char* pszMood) { return true; }
	const char* GetMood() const { return ""; }
	IStringItVec* GetThemes() const { return NULL; }
	IStringItVec* GetMoods(const char* pszTheme) const { return NULL; }
	bool AddMusicMoodEvent(const char* pszMood, float fTimeout) { return true; }
	virtual CTimeValue GetMoodTime() const { return m_TimeValue; }

	SMusicSystemStatus* GetStatus() { return NULL; }
	void GetMemoryUsage(class ICrySizer* pSizer) const {}
	bool LoadMusicDataFromLUA(IScriptSystem* pScriptSystem, const char* pszFilename) { return true; }
	bool StreamOGG() { return true; }
	void LogMsg(const int nVerbosity, const char* pszFormat, ...) PRINTF_PARAMS(3, 4) {};

	//////////////////////////////////////////////////////////////////////////
	// Editing support.
	//////////////////////////////////////////////////////////////////////////
	virtual void RenamePattern(const char* sOldName, const char* sNewName) {};
	virtual void UpdatePattern(SMusicInfo::Pattern* pPattern) {};
	virtual void PlayPattern(const char* sPattern, bool bStopPrevious, bool bPlaySynched) {};
	virtual void DeletePattern(const char* sPattern) {};

	virtual bool StopLastPattern(const char* sPattern) { return false; }
	virtual bool SeekLastPattern(const char* sPattern, const float position) { return false; }
	virtual bool GetLastPatternStartTime(const char* sPattern, CTimeValue& startTime) { return false; }

	virtual void Silence() {};
	virtual bool LoadFromXML(const char* sFilename, bool bAddData, bool bReplaceData) { return true; };

	virtual void PlayStinger() {};

	// writes output to screen in debug
	virtual void DrawInformation(IRenderer* pRenderer, float xpos, float ypos) {};

	// for serialization
	virtual void Serialize(TSerialize ser) {}
	virtual bool SerializeInternal(bool bSave) { return true; }
};
