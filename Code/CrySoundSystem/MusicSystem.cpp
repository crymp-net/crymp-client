#include <mutex>

#include "CryCommon/CryCore/CrySizer.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CrySoundSystem/IReverbManager.h"
#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/ITimer.h"

#include "IAudioDevice.h"
#include "PatternDecoder.h"
#include "MusicSystem.h"

static std::recursive_mutex g_mutex;

// streaming-callback called by fmod.
FMOD_RESULT F_CALLBACK _StreamingCallback(FMOD_SOUND* pStream, void* pBuffer, unsigned int nLength)
{
	void* nParam;
	FMOD::Sound* sound = (FMOD::Sound*)pStream;
	sound->getUserData(&nParam);

	if (!nParam)
		return FMOD_ERR_INVALID_PARAM;
	CMusicSystem* pMusicSystem = (CMusicSystem*)nParam;
	if (!pMusicSystem->StreamingCallback(pStream, pBuffer, nLength))
		return FMOD_OK;
	else
		return FMOD_ERR_INVALID_PARAM;
}

/* constructor
 */
CMusicSystem::CMusicSystem(IAudioDevice* pAudioDevice)
{
	m_pAudioDevice = pAudioDevice;

	if (gEnv->pSoundSystem)
		m_fMasterVolume = gEnv->pSoundSystem->GetMusicVolume();
	else
		m_fMasterVolume = 0.0f;

	m_nSampleRate = 44100;
	//	m_fLatency=0.25f;
	m_fLatency = 0.1f; // to let it peak less
	m_pStream = NULL;
	m_pChannel = NULL;
	m_bPlaying = false;
	m_mapThemes.clear();

	m_pTrack1 = NULL;
	m_pTrack2 = NULL;
	m_pNextTrack = NULL;
	m_Track1Type = MUSICLAYER_NONE;
	m_Track2Type = MUSICLAYER_NONE;
	m_NextTrackType = MUSICLAYER_NONE;

	m_nLayeredRhythmicPatterns = 0;
	m_nLayeredIncidentalPatterns = 0;
	m_pMixBuffer = NULL;
	m_pCurrTheme = NULL;
	m_pNextTheme = NULL;
	m_pCurrMood = NULL;
	m_pNextMood = NULL;
	m_bForcePatternChange = false;
	m_fDefaultMoodTime = 0.0f;
	m_sDefaultMood = "";
	m_pSink = NULL;

	m_tLastUpdate.SetMilliSeconds(0);

	m_nDelayThemeInSec = -1;

	m_bDataLoaded = false;
	m_bPause = false;
	m_bBridging = false;
	m_bCurrPatternIsBridge = false;
	m_fCurrCrossfadeTime = DEFAULT_CROSSFADE_TIME;

	m_musicEnable = g_nMusicEnable;

	m_bStreamDelayCalced = false;
	m_nStreamDelay = 28084; // Estimated streaming delay

	Init();
}

/* destructor; make sure everything gets unloaded
 */
CMusicSystem::~CMusicSystem()
{
	Unload();
}

/* destroy system
 */
void CMusicSystem::Release()
{
	Shutdown();
	delete this;
}

/* set up notification-sink
 */
IMusicSystemSink* CMusicSystem::SetSink(IMusicSystemSink* pSink)
{
	IMusicSystemSink* pOldSink = m_pSink;
	m_pSink = pSink;
	return pOldSink;
}

/* initialize system and create fmod-streaming callback
 */
bool CMusicSystem::Init()
{
	std::lock_guard lock(g_mutex);

	EnableEventProcessing(true);

	int nTime = (int)(gEnv->pTimer->GetAsyncCurTime() / 1000.0f);
	m_RandGen.Seed(nTime & 0xfffffffe);
	m_nBytesPerSample = 4; // always use STEREO 16 BIT

	m_pMixBuffer = new char[(int)((float)m_nSampleRate * m_fLatency) * m_nBytesPerSample];

	if (m_pAudioDevice)
	{
		FMOD::System* pFMODEx = (FMOD::System*)m_pAudioDevice->GetSoundLibrary();
		if (pFMODEx)
		{
			FMOD_RESULT result;
			FMOD::Sound* sound = 0;
			FMOD_MODE mode = (FMOD_MODE)(FMOD_2D | FMOD_SOFTWARE | FMOD_DEFAULT | FMOD_OPENUSER |
			                             FMOD_LOOP_NORMAL | FMOD_CREATESTREAM);

			int channels = 2;
			FMOD_CREATESOUNDEXINFO createsoundexinfo{};
			createsoundexinfo.decodebuffersize =
			    16384; // 44100;                                       /* Chunk size of stream update in
			           // samples.  This will be the amount of data passed to the user callback. */
			createsoundexinfo.length =
			    44100 * channels * sizeof(signed short) *
			    10; /* Length of PCM data in bytes of whole song (for Sound::getLength) */
			createsoundexinfo.numchannels = channels;               /* Number of channels in the sound. */
			createsoundexinfo.defaultfrequency = 44100;             /* Default playback rate of sound. */
			createsoundexinfo.format = FMOD_SOUND_FORMAT_PCM16;     /* Data format of sound. */
			createsoundexinfo.pcmreadcallback = _StreamingCallback; /* User callback for reading. */
			createsoundexinfo.userdata = (void*)this;

			result = pFMODEx->createSound(0, mode, &createsoundexinfo, &sound);
			// ERRCHECK(result);
			m_pStream = (tAssetHandle)sound;
		}
	}

	if (!m_pStream)
		return false;

	return true;
}

/* counterpart of init(); destroy stream and cleanup allocated mem.
 */
void CMusicSystem::Shutdown()
{
	StopPlaying(); // must be out of critical section

	std::lock_guard lock(g_mutex);
	Silence();

	if (m_pStream)
	{

		FMOD::Sound* pSound = (FMOD::Sound*)m_pStream;
		FMOD::Channel* pChannel = (FMOD::Channel*)m_pChannel;
		FMOD_RESULT result = FMOD_OK;

		if (m_pChannel != NULL)
			result = pChannel->stop();

		if (pSound)
			result = pSound->release();

		result = result;
	}
	m_pStream = NULL;
	m_pChannel = NULL;
	m_bPlaying = false;
	if (m_pMixBuffer)
		delete[] m_pMixBuffer;
	m_pMixBuffer = NULL;
	FlushPatterns();
}

void CMusicSystem::LogMsg(const int nVerbosity, const char* pszFormat, ...)
{
	if (g_nDebugMusic >= nVerbosity)
	{
		va_list args;
		va_start(args, pszFormat);
		gEnv->pLog->LogV(ILog::eAlways, pszFormat, args);
		va_end(args);
	}
}

/* add (create) pattern to the music-system and create/open its decoder instance
 */
CMusicPattern* CMusicSystem::AddPattern(const char* pszName, const char* pszFilename)
{
	if (!pszName || !*pszName || !pszFilename || !*pszFilename)
	{
		return nullptr;
	}

	std::lock_guard lock(g_mutex);

	const auto [it, added] = m_patterns.emplace(pszName, CMusicPattern(this, pszName, pszFilename));

	return &it->second;
}

CMusicPattern* CMusicSystem::FindPattern(const char* pszName)
{
	if (!pszName || !*pszName)
	{
		return nullptr;
	}

	const auto it = m_patterns.find(pszName);
	if (it == m_patterns.end())
	{
		return nullptr;
	}

	return &it->second;
}

/* release all patterns
 */
bool CMusicSystem::FlushPatterns()
{
	std::lock_guard lock(g_mutex);

	ReleaseData();

	m_mapThemes.clear();
	m_pCurrTheme = NULL;
	m_pNextTheme = NULL;
	m_pCurrMood = NULL;
	m_pNextMood = NULL;
	m_vecPlayingPatterns.clear();
	m_patterns.clear();

	m_pTrack1 = NULL;
	m_pTrack2 = NULL;
	m_pNextTrack = NULL;

	m_nLayeredRhythmicPatterns = 0;
	m_nLayeredIncidentalPatterns = 0;
	m_bForcePatternChange = false;
	m_fDefaultMoodTime = 0.0f;
	m_sDefaultMood = "";
	m_bDataLoaded = false;
	m_bPause = false;
	m_bBridging = false;
	m_bCurrPatternIsBridge = false;

	return true;
}

/* start streaming (so callback will be called)
 */
bool CMusicSystem::StartPlaying()
{
	if (!m_musicEnable)
		return false;

	std::lock_guard lock(g_mutex);

	if (!m_bDataLoaded)
		return false;

	if (m_bPause)
		return true;

	if (m_pChannel != NULL)
		return false;

	if (gEnv->pSoundSystem)
		m_fMasterVolume = gEnv->pSoundSystem->GetMusicVolume();

	if (m_fMasterVolume <= 0)
		return true;

	m_bPlaying = true;

	FMOD::System* pFMODEx = (FMOD::System*)m_pAudioDevice->GetSoundLibrary();
	if (pFMODEx)
	{
		FMOD::Channel* pChannel = (FMOD::Channel*)m_pChannel;
		m_ExResult = pFMODEx->playSound(FMOD_CHANNEL_FREE, (FMOD::Sound*)m_pStream, true, &pChannel);

		if (m_ExResult != FMOD_OK || pChannel == NULL)
		{
			CryLogAlways("Sound - start playing music failed! \n");
			return false;
		}

		// boost Priority
		pChannel->setPriority(0);

		FMOD_SPEAKERMODE SpeakerMode = FMOD_SPEAKERMODE_RAW;
		m_ExResult = pFMODEx->getSpeakerMode(&SpeakerMode);

		if (SpeakerMode > FMOD_SPEAKERMODE_STEREO)
		{
			m_ExResult = pChannel->setSpeakerMix(g_fMusicSpeakerFrontVolume, g_fMusicSpeakerFrontVolume,
			                                     g_fMusicSpeakerCenterVolume, g_fMusicSpeakerLFEVolume,
			                                     g_fMusicSpeakerBackVolume, g_fMusicSpeakerBackVolume,
			                                     g_fMusicSpeakerSideVolume, g_fMusicSpeakerSideVolume);
		}

		FMOD_REVERB_CHANNELPROPERTIES TempReverb = {0};
		m_ExResult = pChannel->getReverbProperties(&TempReverb);
		TempReverb.Room = -10000;
		m_ExResult = pChannel->setReverbProperties(&TempReverb);

		m_ExResult = pChannel->setPaused(false);

		m_pChannel = pChannel;
	}

	if (m_pChannel == NULL)
	{
		FlushPatterns();
		return false;
	}

	FMOD::Channel* pChannel = (FMOD::Channel*)m_pChannel;
	m_ExResult = pChannel->setVolume(m_fMasterVolume);

	return true;
}

/* stop streaming; streaming-callback will not be called anymore
 */
bool CMusicSystem::StopPlaying()
{
	if (!m_musicEnable)
		return false;

	if (m_pChannel == NULL)
		return false;

	FMOD::Channel* pChannel = (FMOD::Channel*)m_pChannel;
	pChannel->setPaused(true);
	pChannel->stop();
	// m_pStream=NULL;

	{
		std::lock_guard lock(g_mutex);

		m_bPlaying = false;
		m_pChannel = NULL;
	}

	return true;
}

/* set music-system data (eg. read from lua-file by gamecode)
        we convert all names to lowercase to make them not case-sensitive (faster lookup)
        we start streaming
*/
bool CMusicSystem::SetData(SMusicInfo::Data* pMusicData)
{
	StopPlaying(); // must be out of critical section

	std::lock_guard lock(g_mutex);

	FlushPatterns();

	if (!pMusicData)
		return true;

	int i;

	//////////////////////////////////////////////////////////////////////////
	// Set themes.
	m_mapThemes.clear();
	for (i = 0; i < pMusicData->nThemes; i++)
	{
		SMusicTheme* pTheme = new SMusicTheme;
		pTheme->fDefaultMoodTimeout = pMusicData->pThemes[i].fDefaultMoodTimeout;
		pTheme->sDefaultMood = pMusicData->pThemes[i].sDefaultMood;
		pTheme->sName = pMusicData->pThemes[i].sName;
		m_mapThemes[pTheme->sName] = pTheme;
	}

	//////////////////////////////////////////////////////////////////////////
	// Set Moods.
	for (i = 0; i < pMusicData->nMoods; i++)
	{
		SMusicInfo::Mood& moodInfo = pMusicData->pMoods[i];
		SMusicTheme* pTheme = stl::find_in_map(m_mapThemes, moodInfo.sTheme, NULL);
		if (!pTheme)
			continue;

		SMusicMood* pMood = new SMusicMood;
		pMood->sName = moodInfo.sName;
		pMood->bPlaySingle = moodInfo.bPlaySingle;
		pMood->fFadeOutTime = moodInfo.fFadeOutTime;
		pMood->nPriority = moodInfo.nPriority;

		pMood->vecPatternSets.resize(moodInfo.nPatternSetsCount);
		for (int j = 0; j < moodInfo.nPatternSetsCount; j++)
		{
			SMusicPatternSet* pPattern = new SMusicPatternSet;
			pMood->vecPatternSets[j] = pPattern;
			pPattern->fIncidentalLayerProbability = moodInfo.pPatternSets[j].fIncidentalLayerProbability;
			pPattern->fMaxTimeout = moodInfo.pPatternSets[j].fMaxTimeout;
			pPattern->fMinTimeout = moodInfo.pPatternSets[j].fMinTimeout;
			pPattern->fRhythmicLayerProbability = moodInfo.pPatternSets[j].fRhythmicLayerProbability;
			pPattern->fTotalIncidentalPatternProbability =
			    moodInfo.pPatternSets[j].fTotalIncidentalPatternProbability;
			pPattern->fTotalStingerPatternProbability =
			    moodInfo.pPatternSets[j].fTotalStingerPatternProbability;
			pPattern->nMaxSimultaneousIncidentalPatterns =
			    moodInfo.pPatternSets[j].nMaxSimultaneousIncidentalPatterns;
			pPattern->nMaxSimultaneousRhythmicPatterns =
			    moodInfo.pPatternSets[j].nMaxSimultaneousRhythmicPatterns;
		}
		pTheme->mapMoods[pMood->sName] = pMood;
	}

	//////////////////////////////////////////////////////////////////////////
	// Set Patterns.
	for (i = 0; i < pMusicData->nPatterns; i++)
	{
		SMusicInfo::Pattern& patternInfo = pMusicData->pPatterns[i];
		SMusicTheme* pTheme = stl::find_in_map(m_mapThemes, patternInfo.sTheme, NULL);
		if (!pTheme)
			continue;

		SMusicMood* pMood = stl::find_in_map(pTheme->mapMoods, patternInfo.sMood, NULL);
		if (!pMood)
			continue;

		SPatternDef* pPatternDef = new SPatternDef;
		pPatternDef->fProbability = patternInfo.fProbability;
		pPatternDef->fLayeringVolume = patternInfo.fLayeringVolume;
		pPatternDef->sFilename = patternInfo.sFilename;
		pPatternDef->sName = patternInfo.sName;
		pPatternDef->nPreFadeIn = patternInfo.nPreFadeIn;
		for (int j = 0; j < patternInfo.nFadePointsCount; j++)
			pPatternDef->vecFadePoints.push_back(patternInfo.pFadePoints[j]);

		assert(patternInfo.nPatternSetIndex >= 0 &&
		       patternInfo.nPatternSetIndex < (int)pMood->vecPatternSets.size());
		SMusicPatternSet* pPatternSet = pMood->vecPatternSets[patternInfo.nPatternSetIndex];
		switch (patternInfo.nPatternType)
		{
			case MUSICLAYER_MAIN:
				pPatternSet->vecMainPatterns.push_back(pPatternDef);
				break;
			case MUSICLAYER_INCIDENTAL:
				pPatternSet->vecIncidentalPatterns.push_back(pPatternDef);
				break;
			case MUSICLAYER_RHYTHMIC:
				pPatternSet->vecRhythmicPatterns.push_back(pPatternDef);
				break;
			case MUSICLAYER_START:
				pPatternSet->vecStartPatterns.push_back(pPatternDef);
				break;
			case MUSICLAYER_END:
				pPatternSet->vecEndPatterns.push_back(pPatternDef);
				break;
			case MUSICLAYER_STINGER:
				pPatternSet->vecStingerPatterns.push_back(pPatternDef);
				pPatternDef->fProbability = 1.0f;
				break;
		}

		CMusicPattern* pPattern = AddPattern(pPatternDef->sName.c_str(), pPatternDef->sFilename.c_str());
		if (!pPattern)
		{
			gEnv->pSystem->Warning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_SOUND,
			                       pPatternDef->sFilename.c_str(), "<Music> File %s failed to load",
			                       pPatternDef->sFilename.c_str());
			continue;
		}
		for (TIntVecIt IntIt = pPatternDef->vecFadePoints.begin(); IntIt != pPatternDef->vecFadePoints.end();
		     ++IntIt)
		{
			pPattern->AddFadePoint(*IntIt);
		}
		pPattern->SetPreFadeIn(pPatternDef->nPreFadeIn);
		pPattern->SetLayeringVolume(pPatternDef->fLayeringVolume);
	}

	m_bDataLoaded = true;

	// start playing the stream...
	if (m_pChannel == NULL)
		StartPlaying();

	return true;
}

void CMusicSystem::ReleaseData()
{
	// release themes/moods
	for (TThemeMapIt ThemeIt = m_mapThemes.begin(); ThemeIt != m_mapThemes.end(); ++ThemeIt)
	{
		SMusicTheme* pTheme = ThemeIt->second;
		for (TMoodMapIt MoodIt = pTheme->mapMoods.begin(); MoodIt != pTheme->mapMoods.end(); ++MoodIt)
		{
			SMusicMood* pMood = MoodIt->second;
			for (TPatternSetVecIt PatternSetIt = pMood->vecPatternSets.begin();
			     PatternSetIt != pMood->vecPatternSets.end(); ++PatternSetIt)
			{
				SMusicPatternSet* pPatternSet = (*PatternSetIt);

				TPatternDefVec::iterator PatternIt;
				for (PatternIt = pPatternSet->vecMainPatterns.begin();
				     PatternIt != pPatternSet->vecMainPatterns.end(); ++PatternIt)
				{
					SPatternDef* pPattern = (*PatternIt);
					delete pPattern;
				}
				for (PatternIt = pPatternSet->vecRhythmicPatterns.begin();
				     PatternIt != pPatternSet->vecRhythmicPatterns.end(); ++PatternIt)
				{
					SPatternDef* pPattern = (*PatternIt);
					delete pPattern;
				}
				for (PatternIt = pPatternSet->vecIncidentalPatterns.begin();
				     PatternIt != pPatternSet->vecIncidentalPatterns.end(); ++PatternIt)
				{
					SPatternDef* pPattern = (*PatternIt);
					delete pPattern;
				}
				for (PatternIt = pPatternSet->vecStartPatterns.begin();
				     PatternIt != pPatternSet->vecStartPatterns.end(); ++PatternIt)
				{
					SPatternDef* pPattern = (*PatternIt);
					delete pPattern;
				}
				for (PatternIt = pPatternSet->vecEndPatterns.begin();
				     PatternIt != pPatternSet->vecEndPatterns.end(); ++PatternIt)
				{
					SPatternDef* pPattern = (*PatternIt);
					delete pPattern;
				}
				for (PatternIt = pPatternSet->vecStingerPatterns.begin();
				     PatternIt != pPatternSet->vecStingerPatterns.end(); ++PatternIt)
				{
					SPatternDef* pPattern = (*PatternIt);
					delete pPattern;
				}
				delete pPatternSet;
			}
			delete pMood;
		}
		delete pTheme;
	}
}

/* stop streaming and unload all data from the system
 */
void CMusicSystem::Unload()
{
	StopPlaying(); // must be out of critical section

	// stop the stream...
	std::lock_guard lock(g_mutex);
	Silence();
	FlushPatterns();
}

/* pause/resume streaming
 */
void CMusicSystem::Pause(bool bPause)
{
	if (m_bPause != bPause)
	{
		m_bPause = bPause;
		if (m_bPause)
			StopPlaying(); // must be out of critical section
		else
			StartPlaying();
	}
}

/* get pointer to current mood descriptor
 */
SMusicMood* CMusicSystem::GetMood(SMusicTheme* pTheme, const char* pszMood) const
{
	std::lock_guard lock(g_mutex);
	if (!pTheme)
		return NULL;
	TMoodMapIt It = pTheme->mapMoods.find(CONST_TEMP_STRING(pszMood));
	if (It == pTheme->mapMoods.end())
		return NULL;
	return It->second;
}

/* set the current theme by pointer to descriptor
 */
bool CMusicSystem::SetTheme(SMusicTheme* pNewTheme, bool bForceChange)
{
	std::lock_guard lock(g_mutex);

	if (pNewTheme == m_pCurrTheme)
		return false; // do nothing since we're already in the requested theme

	if (!((m_pCurrTheme == m_pNextTheme) || (pNewTheme))) // only switch to the new theme if there was no switch in
	                                                      // this frame yet OR the new theme is NOT null
		return true;

	if (pNewTheme)
		LogMsg(1, "<Music> Setting theme to %s with%s forcing change.", pNewTheme->sName.c_str(),
		       bForceChange ? "" : "out");
	else
	{
		LogMsg(1, "<Music> Setting theme to NULL");
		SetMood((SMusicMood*)NULL, 1);
	}

	m_pNextTheme = pNewTheme;

	// m_bForcePatternChange = bForceChange;

	m_ThemeTime = gEnv->pTimer->GetFrameStartTime();

	return true;
}

/* set the current theme by name
 */
bool CMusicSystem::SetTheme(const char* pszTheme, bool bForceChange, bool bKeepMood, int nDelayInSec)
{
	std::lock_guard lock(g_mutex);

	// no theme
	if (*pszTheme == 0)
		return SetTheme((SMusicTheme*)NULL, false);

	TThemeMapIt It = m_mapThemes.find(CONST_TEMP_STRING(pszTheme));
	if (It == m_mapThemes.end())
		return false; // theme not found

	if (!m_bPlaying)
		StartPlaying();

	SMusicTheme* pNewTheme = It->second;

	if (nDelayInSec >= 0)
	{
		m_ThemeSetTime = gEnv->pTimer->GetFrameStartTime();
		m_nDelayThemeInSec = nDelayInSec;
	}
	else
		m_nDelayThemeInSec = -1;

	bool bResult = SetTheme(pNewTheme, bForceChange);

	if (bKeepMood && (m_nDelayThemeInSec == -1))
	{
		if (bResult)
		{

			for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != m_vecPlayingPatterns.end();
			     ++It)
			{
				// SMusicPatternPlayInfo *pPlayInfo = GetPlayingPatternInfo(MUSICLAYER_MAIN,
				// EBlend_Once);
				SMusicPatternPlayInfo* pPlayInfo = &(*It);
				if (pPlayInfo->pPatternInstance == m_pTrack1)
				{
					pPlayInfo->fFadeTime = m_pCurrMood ? m_pCurrMood->fFadeOutTime : 1.0f;
					pPlayInfo->eBlendType = EBlend_FadeOut;
					pPlayInfo->ePlayingType = EPlaying_Loop;
					m_bForcePatternChange = false;
					LogMsg(3, "<Music> Start to fade out %s",
					       pPlayInfo->pPatternInstance->GetPattern()->GetName());
				}
			}
		}

		bResult = SetMood(GetMood(), 1, bForceChange | bKeepMood);
		// m_bForcePatternChange = true;
	}

	if (!bResult)
	{
		SetMood(pNewTheme->sDefaultMood.c_str(), 1, bForceChange);
		m_fDefaultMoodTime = -1.0f;
		// m_bForcePatternChange = bForceChange;
	}

	return bResult;
}

/* Brings the current theme to a graceful end
 */
bool CMusicSystem::EndTheme(EThemeFadeType ThemeFade, int nForceEndLimitInSec, bool bEndEverything)
{
	// if (!GetPlayingPatternInfo(MUSICLAYER_END, EBlend_Any))
	{
		if (ThemeFade == EThemeFade_StopAtOnce)
		{
			for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != m_vecPlayingPatterns.end();
			     ++It)
			{
				SMusicPatternPlayInfo& PlayInfo = (*It);
				if (bEndEverything || !(PlayInfo.nLayer & MUSICLAYER_PATTERN_BY_NAME))
				{
					PlayInfo.ePlayingType = EPlaying_Stop;
					PlayInfo.nLayer = MUSICLAYER_END;
				}
			}
			LogMsg(2, "<Music> EndTheme at once");

			if (m_pCurrMood)
				m_pCurrMood->bMainSpawned = false;

			// SetTheme("");
			return true;
		}

		if (ThemeFade == EThemeFade_PlayEnd || ThemeFade == EThemeFade_PlayEndAtFadePoint)
		{
			int nSamplesToEnd = 0;

			if (ThemeFade == EThemeFade_PlayEnd)
			{
				if (m_pTrack1 && m_Track1Type == MUSICLAYER_MAIN)
					nSamplesToEnd = m_pTrack1->GetSamplesToEnd();

				if (m_pTrack2 && m_Track2Type == MUSICLAYER_MAIN)
					nSamplesToEnd = max(nSamplesToEnd, m_pTrack2->GetSamplesToEnd());
			}
			else
			{
				if (m_pTrack1 && m_Track1Type == MUSICLAYER_MAIN)
					nSamplesToEnd = m_pTrack1->GetSamplesToNextFadePoint();

				if (m_pTrack2 && m_Track2Type == MUSICLAYER_MAIN)
					nSamplesToEnd = max(nSamplesToEnd, m_pTrack2->GetSamplesToNextFadePoint());
			}

			int nSecToEnd = nSamplesToEnd / 44100;

			if (nSecToEnd < nForceEndLimitInSec)
			{
				m_pNextTrack = ChoosePattern(m_pCurrMood, MUSICLAYER_END);

				if (m_pNextTrack)
				{
					LogMsg(2, "<Music> EndTheme End found will play in %d seconds", nSecToEnd);
					m_NextTrackType = MUSICLAYER_END;

					if (ThemeFade == EThemeFade_PlayEndAtFadePoint)
						m_pNextTrack->m_bPlayAtOnce = true;

					for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin();
					     It != m_vecPlayingPatterns.end(); ++It)
					{
						SMusicPatternPlayInfo& PlayInfo = (*It);
						if ((bEndEverything &&
						     (PlayInfo.nLayer & MUSICLAYER_PATTERN_BY_NAME)) ||
						    (ThemeFade == EThemeFade_PlayEndAtFadePoint))
						{
							PlayInfo.eBlendType =
							    (ThemeFade == EThemeFade_PlayEndAtFadePoint)
								? EBlend_EndOnFadePoint
								: EBlend_FadeOutOnMainEnd;
							PlayInfo.nLayer = MUSICLAYER_END;
						}
					}
					return true;
				}
			}
		}

		// if none of the above or no end was selected
		LogMsg(2, "<Music> EndTheme FadeOut or no end found within %d seconds", nForceEndLimitInSec);
		for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != m_vecPlayingPatterns.end(); ++It)
		{
			SMusicPatternPlayInfo& PlayInfo = (*It);
			if (bEndEverything || !(PlayInfo.nLayer & MUSICLAYER_PATTERN_BY_NAME))
			{
				PlayInfo.eBlendType = EBlend_FadeOut;
				PlayInfo.nLayer = MUSICLAYER_END;
			}
		}
	}
	return true;
}

/* set the current mood by pointer to descriptor
 */
bool CMusicSystem::SetMood(SMusicMood* pNewMood, const bool bPlayFromStart, const bool bForceChange)
{

	std::lock_guard lock(g_mutex);
	if (!pNewMood)
	{
		m_pCurrMood = pNewMood;
		m_pNextMood = NULL;
		m_pNextTrack = NULL;

		LogMsg(1, "<Music> Setting mood to <none>");
		return true;
	}
	// bool bMainFadeout = GetPlayingPatternInfo(MUSICLAYER_MAIN_ANY, EBlend_FadeOut);
	// bool bMainSpawned = (m_pCurrMood && m_pCurrMood->bMainSpawned);
	if (pNewMood == m_pCurrMood && (m_pTrack1 || m_pTrack2) && (m_pCurrMood && m_pCurrMood->bMainSpawned))
		return true; // do nothing since we're already in the requested mood

	LogMsg(1, "<Music> Setting mood to %s with %s", pNewMood->sName.c_str(),
	       bPlayFromStart ? "queueing" : "crossfading");

	// adjust fading time according to new pattern
	m_fCurrCrossfadeTime = (double)pNewMood->fFadeOutTime;

	pNewMood->bPlayFromStart = bPlayFromStart;

	if (!UpdateCurrentPatternSet(pNewMood, 0, true))
	{
		LogMsg(1, "[Warning] <Music> Unable to find pattern-set in mood %s!", pNewMood->sName.c_str());
		return false;
	}

	m_bForcePatternChange = ((m_pCurrMood != NULL) && (m_pCurrMood != pNewMood)) ||
	                        bForceChange; // try to get into the new mood asap if we're changing the mood
	m_pCurrMood = pNewMood;

	// if there is something playing on the main tracks fade it out
	if (m_pTrack1 || m_pTrack2)
	{
		TPatternPlayInfoVecIt ItEnd = m_vecPlayingPatterns.end();
		for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != ItEnd; ++It)
		{
			SMusicPatternPlayInfo* pPlayInfo = &(*It);

			if ((pPlayInfo->nLayer & MUSICLAYER_MAIN_ANY) && (pPlayInfo->nLayer & ~MUSICLAYER_STINGER) &&
			    (pPlayInfo->ePlayingType != EPlaying_Stop))
			{
				if (bForceChange || pPlayInfo->eBlendType == EBlend_FadeOut ||
				    pPlayInfo->eBlendType == EBlend_FadeIn)
					pPlayInfo->eBlendType = EBlend_FadeOut;
				else
					pPlayInfo->eBlendType = EBlend_FadeOutOnFadePoint;

				pPlayInfo->ePlayingType = EPlaying_Once;
			}
		}
	}

	// no pattern chosen yet, so music should begin with start if available
	bool bStartPlayed = false;

	if (bPlayFromStart)
		bStartPlayed = PlayStart(bForceChange ? EBlend_None : EBlend_PlayOnFadePoint);

	m_pNextTrack = ChoosePattern(pNewMood, MUSICLAYER_MAIN);
	m_NextTrackType = MUSICLAYER_MAIN;

	if (m_pNextTrack)
	{
		m_pNextTrack->m_bPlayAtOnce = (!bStartPlayed && bForceChange);
		m_pNextTrack->m_bPlayFromStart = bPlayFromStart;
	}

	if (!bStartPlayed && m_pNextTrack)
	{
		SMusicPatternPlayInfo PlayInfo;
		PlayInfo.nLayer = m_NextTrackType;
		PlayInfo.pPatternInstance = m_pNextTrack;
		PlayInfo.ePlayingType = EPlaying_Loop;
		PlayInfo.fPhase = 1.0f;
		PlayInfo.fFadeTime = m_pCurrMood->fFadeOutTime;

		if (bPlayFromStart)
		{
			PlayInfo.eBlendType = bForceChange ? EBlend_None : EBlend_PlayOnFadePoint;
		}
		else
		{
			PlayInfo.fPhase = 0.0f;
			PlayInfo.eBlendType = bForceChange ? EBlend_FadeIn : EBlend_FadeInOnFadePoint;
		}

		if (bForceChange)
			PlayInfo.pPatternInstance->SeekPos(m_pTrack1 ? m_pTrack1->GetPos() : 0);

		PushPatternToMixList(PlayInfo);

		m_pTrack2 = m_pNextTrack;
		m_Track2Type = m_NextTrackType;

		m_pNextTrack = NULL;
		m_NextTrackType = 0;

		m_bForcePatternChange = false; // TODO Check, prevent double playback
	}

	// if (!m_pNextTrack)
	//{
	//	LogMsg(1, "[Warning] <Music> Unable to find pattern in mood %s ", pNewMood->sName.c_str() );
	//	return false;
	// }

	m_MoodTime = gEnv->pTimer->GetFrameStartTime();

	return true;
}

/* set the current mood by name
 */
bool CMusicSystem::SetMood(const char* pszMood, const bool bPlayFromStart, const bool bForceChange)
{
	std::lock_guard lock(g_mutex);

	SMusicTheme* pTheme = m_pCurrTheme;

	if (m_pNextTheme)
		pTheme = m_pNextTheme;

	if (!pTheme)
		return false; // no theme set

	if (*pszMood == 0)
	{
		SetMood((SMusicMood*)NULL);
		return true;
	}

	TMoodMapIt It = pTheme->mapMoods.find(CONST_TEMP_STRING(pszMood));

	if (It == pTheme->mapMoods.end())
		return false; // mood not found

	SMusicMood* pNewMood = It->second;
	pNewMood->pSpawnedFromTheme = pTheme;

	if (m_pNextTheme)
		m_pNextMood = pNewMood;

	return SetMood(pNewMood, bPlayFromStart, bForceChange);
}

/* set the current default mood by name
 */
bool CMusicSystem::SetDefaultMood(const char* pszMood)
{
	std::lock_guard lock(g_mutex);
	m_sDefaultMood = pszMood;
	SMusicMood* pDefaultMood = GetDefaultMood(m_pCurrTheme);

	// force reselection of mood if we're currently playing the default mood
	if (m_fDefaultMoodTime < 0.0f)
	{
		// we're currently playing the default mood so lets set the new default-mood
		EnterDefaultMood();
		// m_fDefaultMoodTime = 0.0f;
	}
	else
	{
		// we're currently playing some non-default mood, lets check if the new default mood-priority is higher
		// than the currently playing one
		if (pDefaultMood && m_pCurrMood)
		{
			if (pDefaultMood->nPriority > m_pCurrMood->nPriority)
				m_fDefaultMoodTime = 0.0f; // default-mood priority is higher so lets choose it
		}
	}
	if (m_sDefaultMood.empty())
		LogMsg(1, "<Music> Setting theme-specific default mood");
	else
		LogMsg(1, "<Music> Setting default mood to %s", pszMood);

	return true;
}

/* get a vector of strings containing all available themes
 */
IStringItVec* CMusicSystem::GetThemes() const
{
	std::lock_guard lock(g_mutex);
	TStringVec Vec;
	for (TThemeMapItConst It = m_mapThemes.begin(); It != m_mapThemes.end(); ++It)
	{
		Vec.push_back(It->first.c_str());
	}
	return new CStringItVec(Vec);
}

CTimeValue CMusicSystem::GetThemeTime() const
{
	return gEnv->pTimer->GetFrameStartTime() - m_ThemeTime;
}

/* get a vector of strings containing all available moods in the theme pszTheme
 */
IStringItVec* CMusicSystem::GetMoods(const char* pszTheme) const
{
	std::lock_guard lock(g_mutex);
	TStringVec Vec;
	TThemeMapItConst It = m_mapThemes.find(CONST_TEMP_STRING(pszTheme));

	if (It == m_mapThemes.end())
		return NULL;

	SMusicTheme* pTheme = It->second;
	for (TMoodMapIt Iter = pTheme->mapMoods.begin(); Iter != pTheme->mapMoods.end(); ++Iter)
	{
		Vec.push_back(Iter->first.c_str());
	}
	return new CStringItVec(Vec);
}

/* add a mood-event
 */
bool CMusicSystem::AddMusicMoodEvent(const char* pszMood, float fTimeout)
{
	std::lock_guard lock(g_mutex);
	SMoodEventInfo EventInfo;
	EventInfo.sMood = pszMood;
	EventInfo.fTime = fTimeout;
	m_setFrameMoodEvents.insert(TMoodEventSetIt::value_type(EventInfo));
	return true;
}

CTimeValue CMusicSystem::GetMoodTime() const
{
	return gEnv->pTimer->GetFrameStartTime() - m_MoodTime;
}

/* remove all patterns from the play-lists
 */
void CMusicSystem::Silence()
{
	std::lock_guard lock(g_mutex);

	m_pTrack1 = NULL;
	m_pTrack2 = NULL;
	m_pNextTrack = NULL;

	m_pCurrMood = NULL;
	m_pCurrTheme = NULL;
	m_pNextTheme = NULL;

	m_nLayeredRhythmicPatterns = 0;
	m_nLayeredIncidentalPatterns = 0;
	m_vecPlayingPatterns.clear();
}

/* main update-loop; check comments in function for details
 */
void CMusicSystem::Update()
{
	// Update the MusicSystem every 250ms
	CTimeValue tFrameTime = gEnv->pTimer->GetFrameStartTime(ITimer::ETIMER_UI);
	CTimeValue tTimeDif = tFrameTime - m_tLastUpdate;
	float fMS = tTimeDif.GetMilliSeconds();

	if (abs(fMS) < 250)
		return;

	m_tLastUpdate = tFrameTime;

	if (!m_pCurrTheme || !m_musicEnable)
	{
		m_MoodTime = gEnv->pTimer->GetFrameStartTime();
		m_ThemeTime = gEnv->pTimer->GetFrameStartTime();
	}

	int nThemeInSecs = GetThemeTime().GetSeconds();
	int nMoodInSecs = GetMoodTime().GetSeconds();

	if (!m_bDataLoaded)
		return; // no data loaded

	FRAME_PROFILER("Update", GetISystem(), PROFILE_MUSIC);

	if (g_nMusicEnable != m_musicEnable)
	{
		if (!g_nMusicEnable)
		{
			// Pause(true);
			StopPlaying(); // must be out of critical section
			Silence();
			m_musicEnable = g_nMusicEnable;
		}
		else
		{
			// Pause(false);
			m_musicEnable = g_nMusicEnable;
			StartPlaying();
		}

		// m_musicEnable = m_pCVarMusicEnable->GetIVal();
	}
	if (!m_musicEnable)
		return;

	// get deltatime
	float fDeltaTime = gEnv->pTimer->GetFrameTime();

	// check if the volume has changed...
	if (gEnv->pSoundSystem)
	{
		if (m_fMasterVolume != gEnv->pSoundSystem->GetMusicVolume())
		{
			m_fMasterVolume = gEnv->pSoundSystem->GetMusicVolume();
			if (m_fMasterVolume > 0.0f)
			{
				StartPlaying(); // start playing in case we're stopped (volume=0)

				if (m_pChannel != NULL)
				{
					FMOD::Channel* pChannel = (FMOD::Channel*)m_pChannel;
					m_ExResult = pChannel->setVolume(m_fMasterVolume);
				}
			}
			else
			{
				// must be out of critical section
				StopPlaying(); // volume=0, lets stop playing the stream to reduce overhead
			}
		}
	}

	std::lock_guard lock(g_mutex);

	if (m_nDelayThemeInSec > 0 &&
	    (m_ThemeSetTime.GetSeconds() + m_nDelayThemeInSec <= gEnv->pTimer->GetFrameStartTime().GetSeconds()))
	{
		m_ThemeSetTime.SetSeconds(0.0f);
		m_nDelayThemeInSec = -1;
		m_bForcePatternChange = true;
	}

	// check for theme change
	if (m_pCurrTheme != m_pNextTheme)
	{
		SMusicTheme* pNext = NULL;

		pNext = m_pNextTheme;

		m_bForcePatternChange =
		    ((m_pCurrTheme != NULL) && (m_pCurrTheme != pNext) && m_nDelayThemeInSec == -1) ||
		    m_bForcePatternChange; // try to get into the new theme asap if we're changing the theme
		if (m_pCurrTheme && pNext &&
		    m_bForcePatternChange) // if we are already playing a theme we need to use a bridge
		{
			CMusicPatternInstance* pPattern = ChooseBridge(m_pCurrTheme, pNext);
			// m_pNextTrack = ChooseBridge(m_pCurrTheme, pNext);

			if (!pPattern)
			{
				LogMsg(1,
				       "[Warning] <Music> Unable to find bridging pattern or no bridge defined from %s "
				       "to %s !",
				       m_pCurrTheme->sName.c_str(), m_pNextTheme->sName.c_str());
				// No bridge pattern,
			}
			else
			{
				m_pNextTrack = pPattern;
			}
		}

		m_pCurrTheme = pNext;

		if (m_pCurrTheme)
		{
			if (m_bForcePatternChange)
			{
				if (m_pNextMood) // reenter next mood to get it from the correct theme
					SetMood(m_pNextMood);
				else
					m_pCurrMood = NULL;
			}
		}
		else
		{
			for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != m_vecPlayingPatterns.end();
			     ++It)
			{
				SMusicPatternPlayInfo& PlayInfo = (*It);
				if (!(PlayInfo.nLayer & MUSICLAYER_PATTERN_BY_NAME))
				{
					PlayInfo.ePlayingType = EPlaying_Stop;
				}
			}
		}

		if (!m_pCurrMood)
			EnterDefaultMood(); // if we are in no mood right now, use default mood
	}
	else
	{
		// m_bForcePatternChange = false;
	}

	m_pNextMood = NULL;

	// event-handling
	if (m_setFrameMoodEvents.empty())
	{
		// no events occured this frame so lets delete the events-list
		if (!m_setMoodEvents.empty())
			m_setMoodEvents.clear();
	}
	// lets add all new events
	for (TMoodEventSetIt It = m_setFrameMoodEvents.begin(); It != m_setFrameMoodEvents.end(); ++It)
	{
		TMoodEventSetIt EventIt = m_setMoodEvents.find(TMoodEventSetIt::value_type(*It));
		if (EventIt == m_setMoodEvents.end()) // event not in list yet
			m_setMoodEvents.insert(TMoodEventSetIt::value_type(*It));
	}
	// lets not choose any mood which has a lower priority than the currently playing one...
	int nNewMoodPriority = -1;

	if (m_pCurrMood)
		nNewMoodPriority = m_pCurrMood->nPriority;

	SMusicMood* pNewMood = NULL;
	for (TMoodEventSetIt It = m_setMoodEvents.begin(); It != m_setMoodEvents.end();)
	{
		// lets remove all events which didnt occur this frame...
		TMoodEventSetIt FrameIt = m_setFrameMoodEvents.find(TMoodEventSetIt::value_type(*It));
		if (FrameIt == m_setFrameMoodEvents.end()) // event not in list of last frame
		{
			// the event didn't occur, so we remove it here
			TMoodEventSetIt itnext = It;
			itnext++;
			m_setMoodEvents.erase(It);
			It = itnext;
		}
		else
		{
			// the event occured, so lets check if we reached the timeout
			SMoodEventInfo& EventInfo = const_cast<SMoodEventInfo&>(*It);
			EventInfo.fTime -= fDeltaTime;
			if (EventInfo.fTime <= 0.0f)
			{
				// time to switch to this mood, so we store its priority so we can choose the one with
				// the highest priority later
				SMusicMood* pMood = GetMood(m_pCurrTheme, EventInfo.sMood.c_str());
				if (pMood)
				{
					// does it have a higher priority than the one before ?
					if (pMood->nPriority > nNewMoodPriority)
					{
						// ...yes, store name and new priority
						nNewMoodPriority = pMood->nPriority;
						pNewMood = pMood;
					}
				}
				else
				{
					if (m_pCurrTheme)
						LogMsg(1, "[Warning] <Music> Invalid music mood '%s' (Theme: %s)",
						       EventInfo.sMood.c_str(), m_pCurrTheme->sName.c_str());
					else
						LogMsg(1, "[Warning] <Music> Invalid music mood '%s' (Theme: NULL)",
						       EventInfo.sMood.c_str());
				}
			}
			++It;
		}
	}
	m_setFrameMoodEvents.clear();
	// now we need to check if the current default mood has a higher priority than the current mood
	SMusicMood* pDefMood = GetDefaultMood(m_pCurrTheme);
	// select default mood, if...
	if (pDefMood                                    // ...there is a default mood
	    && (pDefMood->nPriority > nNewMoodPriority) // ...it has a higher priority than the new mood
	    && (m_fDefaultMoodTime > 0.0f))             // ...we are not in the default mood already
	{
		// update default mood timing
		m_fDefaultMoodTime -= fDeltaTime;
		// is it time to enter the default mood ?
		if ((m_fDefaultMoodTime < 0.0f) && (m_pCurrMood && (!m_pCurrMood->bPlaySingle)) &&
		    m_bEnableEventProcessing)
			EnterDefaultMood(); // ...yes
	}
	else
	{
		// one of the above criteria failed, so lets see if we can choose the new mood
		// do we have a new mood ?
		if (pNewMood)
		{
			// ...yes
			bool bEnterNewMood = true;
			// do we also have a default mood ?
			if (pDefMood && pNewMood)
			{
				// ...yes; does the default mood has a higher priority than the new one ?
				if (pDefMood->nPriority > pNewMood->nPriority)
					bEnterNewMood = false; // ...yes, so we dont enter the new mood
			}
			if (m_pCurrMood && pNewMood)
			{
				// If currently playing mood is Play Single and have higher priority, doesnt switch to
				// new mood.
				if (m_pCurrMood->bPlaySingle && m_pCurrMood->nPriority > pNewMood->nPriority)
					bEnterNewMood = false;
			}
			// should we enter the new mood ?
			if (bEnterNewMood && m_bEnableEventProcessing)
			{
				// ...yes
				SetMood(pNewMood);
				if (m_pCurrTheme)
					m_fDefaultMoodTime = m_pCurrTheme->fDefaultMoodTimeout;
			}
		}
	}
}

/* get system-status of music-system; returning pointer only valid until next call to GetStatus()
 */
SMusicSystemStatus* CMusicSystem::GetStatus()
{
	std::lock_guard lock(g_mutex);

	m_Status.m_vecPlayingPatterns.clear();
	m_Status.bPlaying = (m_pChannel != NULL);

	if (m_pCurrTheme)
		m_Status.sTheme = m_pCurrTheme->sName.c_str();
	else
		m_Status.sTheme = "<none>";

	if (m_pCurrMood)
		m_Status.sMood = m_pCurrMood->sName.c_str();
	else
		m_Status.sMood = "<none>";

	SPlayingPatternsStatus PatternStatus;
	// push current main-pattern
	// if (m_pCurrPattern)
	//{
	//	PatternStatus.nLayer=MUSICLAYER_MAIN;
	//	PatternStatus.sName=m_pCurrPattern->GetPattern()->GetName();
	//	PatternStatus.nVolume=255;
	//	m_Status.m_vecPlayingPatterns.push_back(PatternStatus);
	//}
	for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != m_vecPlayingPatterns.end(); ++It)
	{
		SMusicPatternPlayInfo& PlayInfo = (*It);
		PatternStatus.nLayer = PlayInfo.nLayer;
		PatternStatus.eBlendType = PlayInfo.eBlendType;
		PatternStatus.sName = PlayInfo.pPatternInstance->GetPattern()->GetName();
		PatternStatus.fVolume = PlayInfo.pPatternInstance->GetPattern()->GetLayeringVolume();
		PatternStatus.fPhase = PlayInfo.fPhase;
		m_Status.m_vecPlayingPatterns.push_back(PatternStatus);
	}
	return &m_Status;
}

/* get pointer to mood-descriptor of current default mood (either theme specific or overriden default mood; whatever is
 * currently set)
 */
SMusicMood* CMusicSystem::GetDefaultMood(SMusicTheme* pTheme) const
{
	std::lock_guard lock(g_mutex);

	if (!pTheme)
	{
		return nullptr;
	}

	if (m_sDefaultMood.empty())
	{
		return GetMood(pTheme, pTheme->sDefaultMood.c_str());
	}
	else
	{
		SMusicMood* pMood = GetMood(pTheme, m_sDefaultMood.c_str());
		if (!pMood)
			return GetMood(pTheme, pTheme->sDefaultMood.c_str()); // return theme-specific mood if user
			                                                      // defined default mood is not available
		else
			return pMood;
	}
}

/* enter the default mood (either theme specific or overriden default mood; whatever is currently set)
 */
void CMusicSystem::EnterDefaultMood()
{
	std::lock_guard lock(g_mutex);

	if (m_pCurrTheme)
	{
		SetMood(GetDefaultMood(m_pCurrTheme));
		m_fDefaultMoodTime = -1.0f;
	}
	else
	{
		m_fDefaultMoodTime = 0.0f; // no theme is set yet, so lets try it again in the next update...
	}
}

/* add a pattern to the play-list
 */
void CMusicSystem::PushPatternToMixList(SMusicPatternPlayInfo& PlayInfo)
{
	std::lock_guard lock(g_mutex);

	if (!m_musicEnable)
		return;

	// clean up pattern which are not updated through streaming callback because FMOD doesnt call it on Vol 0
	if (m_fMasterVolume == 0)
	{
		m_vecPlayingPatterns.clear();
	}

	LogMsg(2, "<Music> Pushing pattern %s in mix-list.", PlayInfo.pPatternInstance->GetPattern()->GetName());
	// MTRACE("Pushing pattern %s in mix-list.", PlayInfo.pPatternInstance->GetPattern()->GetName());
	m_vecPlayingPatterns.push_back(PlayInfo);
}

SMusicPatternPlayInfo* CMusicSystem::GetPlayingPatternInfo(const int nLayer, const EBlendingType BlendType)
{

	TPatternPlayInfoVecIt ItEnd = m_vecPlayingPatterns.end();
	for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != ItEnd; ++It)
	{
		SMusicPatternPlayInfo* pPlayInfo = &(*It);

		if ((pPlayInfo->nLayer & nLayer || nLayer == MUSICLAYER_ANY) &&
		    (pPlayInfo->eBlendType == BlendType || BlendType == EBlend_Any))
			return pPlayInfo;
	}
	return NULL;
}

bool CMusicSystem::IsPatternPlaying(const TPatternInstancePtr pPattern)
{

	TPatternPlayInfoVecIt ItEnd = m_vecPlayingPatterns.end();
	for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != ItEnd; ++It)
	{
		SMusicPatternPlayInfo* pPlayInfo = &(*It);

		if (pPlayInfo->pPatternInstance == pPattern)
			return true;
	}
	return false;
}

/* main streaming callback
        this is the main state-machine; check comments in function for details
        */
signed char CMusicSystem::StreamingCallback(tAssetHandle pStream, void* pBuffer, int nLength)
{
	FRAME_PROFILER("Callback", GetISystem(), PROFILE_MUSIC);

	std::lock_guard lock(g_mutex);

	// FUNCTION_PROFILER( gEnv->pSystem, PROFILE_SOUND );
	// FRAME_PROFILER( "Callback", GetISystem(), PROFILE_MUSIC );
	// FUNCTION_PROFILER( GetSystem(),PROFILE_MUSIC );

	int nNumPlaying = m_vecPlayingPatterns.size();

	if (nNumPlaying == 0)
		int n0 = 0;

	if (nNumPlaying == 1)
		int n1 = 1;

	if (nNumPlaying == 2)
		int n2 = 2;

	// FMOD Ex calls callback before m_pStream gets set, should not create a error
	if (m_pStream == NULL)
		return 0;

	if (m_pStream != pStream)
	{
		// wrong stream; this should never happen
		gEnv->pSystem->Error("<CrySound> (CMusicSystem::StreamingCallback) Wrong Stream");
		return false;
	}
	// Ignore if not playing.
	if (!m_bPlaying)
		return 1;

	if (!m_musicEnable)
		return 1;

	signed short* pOutput = (signed short*)pBuffer;
	int nOfs = 0;
	int nSamples = nLength / m_nBytesPerSample;
	memset(&(pOutput[nOfs]), 0, nSamples * m_nBytesPerSample);

	// check if theme and mood are valid
	if ((!m_pCurrTheme) || (!m_pCurrMood) || (!UpdateCurrentPatternSet(m_pCurrMood, nSamples, true)))
	{
		MixStreams(&(pOutput[nOfs]), nSamples);
		return 1;
	}

	if (!m_pNextTrack)
	{
		SMusicPatternPlayInfo* pTempMain = GetPlayingPatternInfo(MUSICLAYER_END, EBlend_Any);

		if (!pTempMain)
		{
			m_pNextTrack = ChoosePattern(m_pCurrMood, MUSICLAYER_MAIN);
			m_NextTrackType = MUSICLAYER_MAIN;

			if (m_pNextTrack)
				m_pNextTrack->m_bPlayAtOnce = false;
		}

		memset(&(pOutput[nOfs]), 0, nSamples * m_nBytesPerSample);
		MixStreams(&(pOutput[nOfs]), nSamples);
		return 1;
	}

	// if no patterns are chosen, lets choose one
	if (!m_pTrack1 && !m_pTrack2)
	{
		memset(&(pOutput[nOfs]), 0, nSamples * m_nBytesPerSample);
		MixStreams(&(pOutput[nOfs]), nSamples);

		return 1;
	}

	for (;;)
	{
		if (!GetPlayingPatternInfo(MUSICLAYER_ANY, EBlend_Any))
			break;

		// sorting Patterns if Track1 is invalid and Track2 is fine
		if (!m_pTrack1 && m_pTrack2)
		{
			m_pTrack1 = m_pTrack2;
			m_Track1Type = m_Track2Type;

			m_pTrack2 = NULL;
			m_Track2Type = 0;
		}

		assert(m_pTrack1);

		int nSamplesToNextFadePointOrEnd = m_pTrack1->GetSamplesToNextFadePoint();

		if ((!m_bBridging) && (nSamplesToNextFadePointOrEnd <= nSamples) && m_pCurrMood->bMainSpawned)
		{
			bool bFading = (GetPlayingPatternInfo(MUSICLAYER_MAIN, EBlend_FadeIn) ||
			                GetPlayingPatternInfo(MUSICLAYER_MAIN, EBlend_FadeOut) ||
			                GetPlayingPatternInfo(MUSICLAYER_END, EBlend_FadeOut));

			// adjust streams ref-count, so it will free up patterns which will finish this frame
			AdjustMixStreamsRefCount(nSamplesToNextFadePointOrEnd);
			// ...yes, lets eventually choose some layered patterns
			// rhythmic layer
			if ((!m_pCurrMood->pCurrPatternSet->vecRhythmicPatterns.empty()) &&
			    (m_nLayeredRhythmicPatterns <
			     m_pCurrMood->pCurrPatternSet->nMaxSimultaneousRhythmicPatterns) &&
			    (m_nLayeredRhythmicPatterns + m_nLayeredIncidentalPatterns) < g_nMusicMaxPatterns &&
			    !bFading)
			{
				// should we layer now ?
				uint32 nRand = m_RandGen.Rand(0.0f, 100.0f);
				bool bIsTimeForLayer = nRand < m_pCurrMood->pCurrPatternSet->fRhythmicLayerProbability;
				if (bIsTimeForLayer)
				{
					// ...yes
					CMusicPatternInstance* pLayeredPattern =
					    ChoosePattern(m_pCurrMood, MUSICLAYER_RHYTHMIC);
					if (pLayeredPattern)
					{
						m_nLayeredRhythmicPatterns++;
						pLayeredPattern->Seek0(
						    nSamplesToNextFadePointOrEnd); // lets seek negative so it starts
						                                   // playing at the right time...
						// lets queue the new pattern so it gets automatically played till the
						// end
						SMusicPatternPlayInfo PlayInfo;
						PlayInfo.nLayer = MUSICLAYER_RHYTHMIC;
						PlayInfo.pPatternInstance = pLayeredPattern;
						PlayInfo.eBlendType = EBlend_None;
						PlayInfo.ePlayingType = EPlaying_Once;
						PlayInfo.fPhase = 1.0f;
						PlayInfo.pRefCount = &m_nLayeredRhythmicPatterns;
						PlayInfo.nSamplesToWait = 0;
						PlayInfo.fFadeTime = m_pCurrMood->fFadeOutTime;
						PushPatternToMixList(PlayInfo);
#ifdef _DEBUG
						SMusicPatternFileInfo FileInfo;
						pLayeredPattern->GetPattern()->GetFileInfo(FileInfo);
						LogMsg(3, "<Music> Layering rhythmic-layer %s",
						       FileInfo.sFilename.c_str());
#endif
					}
				}
			}

			int nSamplePos = m_pTrack1->GetPos();

			// incidental layer
			if ((!m_pCurrMood->pCurrPatternSet->vecIncidentalPatterns.empty()) &&
			    (m_nLayeredIncidentalPatterns <
			     m_pCurrMood->pCurrPatternSet->nMaxSimultaneousIncidentalPatterns) &&
			    (m_nLayeredRhythmicPatterns + m_nLayeredIncidentalPatterns) < g_nMusicMaxPatterns &&
			    !bFading)
			{
				// should we layer now ?
				uint32 nRand = m_RandGen.Rand(0.0f, 100.0f);
				bool bIsTimeForLayer =
				    nRand < m_pCurrMood->pCurrPatternSet->fIncidentalLayerProbability;
				if (bIsTimeForLayer)
				{
					// ...yes
					CMusicPatternInstance* pLayeredPattern =
					    ChoosePattern(m_pCurrMood, MUSICLAYER_INCIDENTAL);
					if (pLayeredPattern)
					{
						m_nLayeredIncidentalPatterns++;
						pLayeredPattern->Seek0(
						    nSamplesToNextFadePointOrEnd); // lets seek negative so it starts
						                                   // playing at the right time...
						// lets queue the new pattern so it gets automatically played till the
						// end
						SMusicPatternPlayInfo PlayInfo;
						PlayInfo.nLayer = MUSICLAYER_INCIDENTAL;
						PlayInfo.pPatternInstance = pLayeredPattern;
						PlayInfo.eBlendType = EBlend_None;
						PlayInfo.ePlayingType = EPlaying_Once;
						PlayInfo.fPhase = 1.0f;
						PlayInfo.pRefCount = &m_nLayeredIncidentalPatterns;
						PlayInfo.nSamplesToWait = 0;
						PlayInfo.fFadeTime = m_pCurrMood->fFadeOutTime;
						PushPatternToMixList(PlayInfo);
#ifdef _DEBUG
						SMusicPatternFileInfo FileInfo;
						pLayeredPattern->GetPattern()->GetFileInfo(FileInfo);
						LogMsg(3, "<Music> Layering incidental-layer %s",
						       FileInfo.sFilename.c_str());
#endif
					}
				}
			}
		}

		// new streaming code
		int nSamplesToNextFadePoint = m_pTrack1->GetSamplesToNextFadePoint();
		int nSamplesToEnd = m_pTrack1->GetSamplesToEnd();
		int nCurrentSamplePos = m_pTrack1->GetPos();

		assert(m_pNextTrack);
		if (!m_pNextTrack->GetPattern())
		{
			assert(!"prevent a bug that should not happen");

			m_pNextTrack = NULL;
			return 1;
		}

		int nPreFadeSamples = m_pNextTrack->GetPattern()->GetPreFadeIn();
		int nSamplesToPreFadeIn = max(0, nSamplesToNextFadePoint - nPreFadeSamples);
		SMusicPatternPlayInfo* pTrack1PlayInfo = NULL;

		TPatternPlayInfoVecIt ItEnd = m_vecPlayingPatterns.end();
		for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != ItEnd; ++It)
		{
			SMusicPatternPlayInfo* pPlayInfo = &(*It);

			if (m_pTrack1 == pPlayInfo->pPatternInstance)
				pTrack1PlayInfo = pPlayInfo;

			switch (pPlayInfo->eBlendType)
			{
				case EBlend_FadeInOnFadePoint:
				{
					if (nSamplesToPreFadeIn <= nSamples || m_pTrack1 == pPlayInfo->pPatternInstance)
					{
						// if (!m_pNextTrack->m_bPlayFromStart)
						{
							// Go to the position
							// prepare the m_pMixBuffer with the correct number of samples
							// to read
							SMusicPatternFileInfo FileInfoNext;
							pPlayInfo->pPatternInstance->GetPattern()->GetFileInfo(
							    FileInfoNext);
							int nSamplesToRead = nCurrentSamplePos % FileInfoNext.nSamples;

							// try if that decoder can seek directly to the position
							if (!pPlayInfo->pPatternInstance->SeekPos(nSamplesToRead))
							{
								// if not, do a fallback and read/seek to the position
								int nSamplesTurn = nSamples;
								while (nSamplesToRead > 0)
								{
									pPlayInfo->pPatternInstance->GetPCMData(
									    (signed long*)m_pMixBuffer, nSamplesTurn);
									nSamplesToRead -= nSamples;
									nSamplesTurn =
									    min(nSamplesToRead, nSamplesTurn);
								}
							}
						}

						pPlayInfo->nSamplesToWait = nSamplesToPreFadeIn;
						pPlayInfo->eBlendType = EBlend_FadeIn;
					}

					break;
				}
				case EBlend_FadeOut:
				{
					if (m_pTrack1 == pPlayInfo->pPatternInstance)
					{
						m_pTrack1 = NULL;
					}
					break;
				}
				case EBlend_FadeOutOnFadePoint:
				{
					if (nSamplesToNextFadePoint <=
					    nSamples) // || m_pTrack1 == pPlayInfo->pPatternInstance)
					{
						pPlayInfo->eBlendType = EBlend_FadeOut;

						if (m_pTrack1 == pPlayInfo->pPatternInstance)
							m_pTrack1 = NULL;
					}

					break;
				}
				case EBlend_FadeOutOnMainEnd:
				{
					if (nSamplesToEnd <= nSamples) // || m_pTrack1 == pPlayInfo->pPatternInstance)
					{
						pPlayInfo->eBlendType = EBlend_FadeOut;

						if (m_pTrack1 == pPlayInfo->pPatternInstance)
							m_pTrack1 = NULL;
					}

					break;
				}
				case EBlend_PlayOnFadePoint:
				{
					if (nSamplesToNextFadePoint <= nSamples ||
					    m_pTrack1 == pPlayInfo->pPatternInstance)
					{
						pPlayInfo->nSamplesToWait = nSamplesToNextFadePoint;
						pPlayInfo->eBlendType = EBlend_None;
					}

					break;
				}
				case EBlend_EndOnFadePoint:
				{
					if (nSamplesToNextFadePoint <= nSamples)
					{
						pPlayInfo->eBlendType = EBlend_None;
						pPlayInfo->ePlayingType = EPlaying_Stop;
					}

					break;
				}
				case EBlend_None:
				{
					if (m_pTrack1 == pPlayInfo->pPatternInstance)
					{
						// speed up any pattern that is waiting for itself
						pPlayInfo->nSamplesToWait = 0;
					}

					break;
				}
				default:
					break;
			}
		}

		// m_pTrack1 was not found in the playing pattern, lets free it and get a new one
		if (!pTrack1PlayInfo)
			m_pTrack1 = NULL;

		// start new pattern on Track1 if its looping
		if (((nSamplesToEnd <= nSamples) ||
		     (nSamplesToNextFadePoint <= nSamples && m_pNextTrack->m_bPlayAtOnce)) &&
		    pTrack1PlayInfo && pTrack1PlayInfo->eBlendType != EBlend_FadeIn)
		{
			// start playing the new pattern

			// check if 2nd track is free
			if (m_pTrack2 && m_pTrack2->GetSamplesToEnd() > nSamples)
				break;

			if (!((m_pNextTrack->GetMood() == m_pCurrMood) || (m_pNextTrack->GetMood() == m_pNextMood)))
			{
				// next track isnt spawned by current mood, so lets get a new next track
				m_pNextTrack = ChoosePattern(m_pCurrMood, MUSICLAYER_MAIN);
			}

			if (!m_pNextTrack->m_bPlayFromStart)
			{
				// Go to the position
				// prepare the m_pMixBuffer with the correct number of samples to read
				SMusicPatternFileInfo FileInfoNext;
				m_pNextTrack->GetPattern()->GetFileInfo(FileInfoNext);
				int nSamplesToRead = nCurrentSamplePos % FileInfoNext.nSamples;

				// try if that decoder can seek directly to the position
				if (!m_pNextTrack->SeekPos(nSamplesToRead))
				{
					// if not, do a fallback and read/seek to the position
					int nSamplesTurn = nSamples;
					while (nSamplesToRead > 0)
					{
						m_pNextTrack->GetPCMData((signed long*)m_pMixBuffer, nSamplesTurn);
						nSamplesToRead -= nSamples;
						nSamplesTurn = min(nSamplesToRead, nSamplesTurn);
					}
				}
				// m_pNextPattern->Seek0(-(nCurrentPos));
			}

			// m_NextPatternType = MUSICLAYER_MAIN;
			SMusicPatternPlayInfo PlayInfo;
			PlayInfo.nLayer = m_NextTrackType;
			PlayInfo.pPatternInstance = m_pNextTrack;
			PlayInfo.fFadeTime = m_pCurrMood->fFadeOutTime;
			PlayInfo.ePlayingType = EPlaying_Loop;
			PlayInfo.fPhase = pTrack1PlayInfo ? pTrack1PlayInfo->fPhase : 1.0f;

			if (m_pNextTrack->m_bPlayFromStart)
			{
				PlayInfo.eBlendType = pTrack1PlayInfo ? pTrack1PlayInfo->eBlendType : EBlend_None;
			}
			else
			{
				PlayInfo.eBlendType = EBlend_FadeIn;
				PlayInfo.fPhase = 0.0f;
			}

			PlayInfo.nSamplesToWait = nSamplesToNextFadePoint;
			PushPatternToMixList(PlayInfo);

			m_pTrack2 = m_pNextTrack;
			m_Track2Type = m_NextTrackType;

			m_pNextTrack = NULL;
			m_NextTrackType = 0;

			m_bForcePatternChange = false;
			m_pCurrMood->bPlayFromStart = true;
		}

		break;
	}

	MixStreams(&(pOutput[nOfs]), nSamples); // mix queued patterns

	return 1;
}

/* lookahead for all patterns in play-list to see which ones exceed in the next nSamples
 */
void CMusicSystem::AdjustMixStreamsRefCount(int nSamples)
{
	for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != m_vecPlayingPatterns.end(); ++It)
	{
		SMusicPatternPlayInfo& PlayInfo = (*It);
		if (!PlayInfo.pRefCount)
			continue;
		int nSamplesToEnd = PlayInfo.pPatternInstance->GetSamplesToEnd();
		if (nSamplesToEnd <= nSamples)
		{
			(*(PlayInfo.pRefCount))--;
			PlayInfo.bRefCountAdjusted = true;
		}
	}
}

/* mix all streams in the play-list into pBuffer; mixlength is nSamples
the data is processed before mixing if needed (eg. ramping)
*/
void CMusicSystem::MixStreams(void* pBuffer, int nSamples)
{
	if (m_bStreamDelayCalced == false)
	{
		if (nSamples != 4096)
		{
			m_bStreamDelayCalced = true;

			FMOD::Channel* pChannel = (FMOD::Channel*)m_pChannel;
			pChannel->getPosition(&m_nStreamDelay, FMOD_TIMEUNIT_PCM);

			m_nStreamDelay = 44100 * 10 - m_nStreamDelay - nSamples;
		}
	}

	TPatternPlayInfoVecIt ItEnd = m_vecPlayingPatterns.end();
	for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != ItEnd;)
	{
		SMusicPatternPlayInfo& PlayInfo = (*It);
		int nSamplesToRead = nSamples;
		bool bStreamEnd = false;

		if (!(PlayInfo.pPatternInstance))
		{
			++It;
			// It = m_vecPlayingPatterns.erase(It);
			continue;
		}

		if (PlayInfo.ePlayingType == EPlaying_Stop)
		{
			bStreamEnd = true;
		}
		else
		{

			// filter out patters that are on hold
			switch (PlayInfo.eBlendType)
			{
				case EBlend_FadeInOnFadePoint:
				{
					++It;
					continue;
					break;
				}
				case EBlend_PlayOnFadePoint:
				{
					++It;
					continue;
					break;
				}
					// case EBlend_EndOnFadePoint:
					//	{
					//		++It;
					//		continue;
					//		break;
					//	}
			}

			if (PlayInfo.bJustStarted == true)
			{
				int currentPos = 0;

				if (PlayInfo.pPatternInstance)
				{
					currentPos = PlayInfo.pPatternInstance->GetPos();
				}

				CTimeValue adjust;
				adjust.SetSeconds(((float)m_nStreamDelay) / m_nSampleRate);

				PlayInfo.startTime =
				    gEnv->pTimer->GetAsyncTime() - ((float)currentPos) / m_nSampleRate + adjust;
				PlayInfo.bJustStarted = false;
			}

			// GUARD_HEAP; // AMD64 note: here was a Memory Corruption
			// int nLength = PlayInfo.fFadeTime;
			int nLength = PlayInfo.pPatternInstance->GetSamplesToEnd();
			if (nSamplesToRead >= nLength)
				nSamplesToRead = nLength;

			nSamplesToRead = max(0, nSamplesToRead - PlayInfo.nSamplesToWait);

			// prepare the m_pMixBuffer with the correct number of samples to read
			if (nSamplesToRead > 0 &&
			    !PlayInfo.pPatternInstance->GetPCMData((signed long*)m_pMixBuffer, nSamplesToRead))
			{
				// something went wrong in the decoder, lets stop that pattern
				bStreamEnd = true;
				PlayInfo.fPhase = 0.0f;
				PlayInfo.eBlendType = EBlend_FadeOut;
				PlayInfo.ePlayingType = EPlaying_Stop;
				break;
			}

			switch (PlayInfo.eBlendType)
			{
				case EBlend_FadeIn:
				{
					if (!FadeStream(PlayInfo, m_pMixBuffer, nSamplesToRead) || nSamplesToRead == 0)
					{
						TPatternPlayInfoVecIt ItEnd2 = m_vecPlayingPatterns.end();
						for (TPatternPlayInfoVecIt It2 = m_vecPlayingPatterns.begin();
						     It2 != ItEnd2; ++It2)
						{
							SMusicPatternPlayInfo& PlayInfo2 = (*It2);
							if ((PlayInfo2.pPatternInstance == m_pTrack1 ||
							     PlayInfo2.pPatternInstance == m_pTrack2) &&
							    PlayInfo2.pPatternInstance != PlayInfo.pPatternInstance)
							{
								// PlayInfo2.eBlendType = EBlend_FadeOut;
								break;
							}
						}
						if (nSamplesToRead == 0)
							bStreamEnd = true;
						// PlayInfo.eBlendType = EBlend_Once;
					}

					break;
				}
				case EBlend_FadeOut:
				{
					if (!FadeStream(PlayInfo, m_pMixBuffer, nSamplesToRead) || nSamplesToRead == 0)
						bStreamEnd = true;

					break;
				}
			}

			switch (PlayInfo.ePlayingType)
			{
				case EPlaying_Loop:
				{
					if (nSamplesToRead >= nLength)
					{
						bStreamEnd = true; // shouldnt go here
					}
					break;
				}
				case EPlaying_Once:
				{
					if (nSamplesToRead >= nLength)
					{
						bStreamEnd = true;
					}
					break;
				}
			}

			// TODO multiply pattern volume
			// TODO convert int volume to float

			// const int nSize = 1024*1024*4;
			// static int arrBuffer[nSize];
			// memset (arrBuffer, 0xCE, sizeof(arrBuffer));
			// memcpy (pBuffer, m_pMixBuffer, nSamplesToRead*4);
			// memset(pBuffer, 0, nSamplesToRead*4);

			signed short* destMix = (signed short*)pBuffer;       // thats where all is stored to
			signed short* srcMix = (signed short*)pBuffer;        // thats where accumulated data is
			signed short* srcTrack = (signed short*)m_pMixBuffer; // thats where the new data is

			// jump to the position when pattern got delayed
			destMix += 2 * PlayInfo.nSamplesToWait;
			srcMix += 2 * PlayInfo.nSamplesToWait;

			float fDest, fMix, fTrack;
			int iDest;
			float fFade = 1.0f;

			if (PlayInfo.pPatternInstance->GetPattern())
				fFade = PlayInfo.pPatternInstance->GetPattern()->GetLayeringVolume();
			else
				int f = 2; // no good

			// quick logarithmic conversion
			float fLog = cry_logf(fFade * 1.71f + 1.0f);

			for (int count = 0; count < nSamplesToRead; count++)
			{
				fMix = *srcMix++;
				fTrack = *srcTrack++;
				fDest = (fMix + fTrack * fLog);
				iDest = (signed int)(fDest);
				*destMix++ = (signed short)(iDest < SHRT_MIN   ? SHRT_MIN
				                            : iDest > SHRT_MAX ? SHRT_MAX
				                                               : iDest);

				fMix = *srcMix++;
				fTrack = *srcTrack++;
				fDest = (fMix + fTrack * fLog);
				iDest = (signed int)(fDest);
				*destMix++ = (signed short)(iDest < SHRT_MIN   ? SHRT_MIN
				                            : iDest > SHRT_MAX ? SHRT_MAX
				                                               : iDest);
			}

			/**/

			/*assert (arrBuffer[nSize-1] == 0xCECECECE);
			assert (IsHeapValid());
			memcpy (pBuffer, arrBuffer, nSamplesToRead*4);
			assert (IsHeapValid());*/
			PlayInfo.nSamplesToWait = max(0, PlayInfo.nSamplesToWait - nSamples);
		}

		if (bStreamEnd)
		{
			unsigned int nTempLayer = PlayInfo.nLayer;
			int bTempPlaySingle = PlayInfo.pPatternInstance->m_bPlaySingle;
			TPatternInstancePtr pTempPattern = PlayInfo.pPatternInstance;

			switch (nTempLayer)
			{
				case MUSICLAYER_MAIN:
				{
					// if the pattern was fading before it could fade out, lets restart it and
					// continue the fade
					if (((PlayInfo.eBlendType == EBlend_FadeIn && PlayInfo.fPhase < 1.0f) ||
					     (PlayInfo.eBlendType == EBlend_FadeOut && PlayInfo.fPhase > 0.0f)) &&
					    PlayInfo.ePlayingType != EPlaying_Stop)
					{
						PlayInfo.pPatternInstance->Seek0();
					}
					else
					{
						if (PlayInfo.pRefCount && (!PlayInfo.bRefCountAdjusted))
							(*(PlayInfo.pRefCount))--;

						It = m_vecPlayingPatterns.erase(It);

						// PlayInfo.pPatternInstance->Seek0();

						if (m_pNextTrack && m_pTrack1 &&
						    m_pNextTrack->GetMood() == m_pTrack1->GetMood())
						{
							m_pNextTrack = NULL;
							m_NextTrackType = 0;
						}

						if (m_pTrack1 == pTempPattern)
						{
							m_pTrack1 = NULL;
							m_Track1Type = 0;
						}
						if (m_pTrack2 == pTempPattern)
						{
							m_pTrack2 = NULL;
							m_Track2Type = 0;
						}

						if (bTempPlaySingle)
							EnterDefaultMood();
					}

					break;
				}
				case MUSICLAYER_START:
				{
					if (PlayInfo.pRefCount && (!PlayInfo.bRefCountAdjusted))
						(*(PlayInfo.pRefCount))--;

					It = m_vecPlayingPatterns.erase(It);

					if (m_pTrack1 == pTempPattern)
					{
						m_pTrack1 = NULL;
						m_Track1Type = 0;
					}
					if (m_pTrack2 == pTempPattern)
					{
						m_pTrack2 = NULL;
						m_Track2Type = 0;
					}
					break;
				}
				case MUSICLAYER_END:
				{
					{
						if (PlayInfo.pRefCount && (!PlayInfo.bRefCountAdjusted))
							(*(PlayInfo.pRefCount))--;

						It = m_vecPlayingPatterns.erase(It);

						if (m_pTrack1 == pTempPattern)
						{
							m_pTrack1 = NULL;
							m_Track1Type = 0;
						}
						if (m_pTrack2 == pTempPattern)
						{
							m_pTrack2 = NULL;
							m_Track2Type = 0;
						}

						break;
					}
				}
				default:
				{
					if (m_pTrack1 == pTempPattern)
					{
						m_pTrack1 = NULL;
						m_Track1Type = 0;
					}
					if (m_pTrack2 == pTempPattern)
					{
						m_pTrack2 = NULL;
						m_Track2Type = 0;
					}

					if (PlayInfo.pRefCount && (!PlayInfo.bRefCountAdjusted))
						(*(PlayInfo.pRefCount))--;

					It = m_vecPlayingPatterns.erase(It);

					break;
				}
			}
		}
		else
			++It;

		if (!m_pTrack1 && !m_pTrack2)
			int a = 0;
	}
}

/* volume ramping for a pattern
 */
// returns if fading is ongoing
bool CMusicSystem::FadeStream(SMusicPatternPlayInfo& PlayInfo, void* pBuffer, int nSamples)
{
	signed short* pDataOut = (signed short*)pBuffer;

	double fFadeTime = PlayInfo.fFadeTime;

	if (!fFadeTime)
		fFadeTime = m_fCurrCrossfadeTime;

	double fPhaseAdder = (1.0 / (double)m_nSampleRate) / fFadeTime;

	for (int i = 0; i < nSamples; i++)
	{
		if (PlayInfo.eBlendType == EBlend_FadeOut)
		{
			if (PlayInfo.fPhase <= 0.0f)
			{
				memset(pDataOut, 0, (nSamples - i) * m_nBytesPerSample);
				PlayInfo.fPhase = 0.0f;
				PlayInfo.ePlayingType = EPlaying_Stop;
				return false;
			}
		}
		else if (PlayInfo.eBlendType == EBlend_FadeIn)
		{
			if (PlayInfo.fPhase >= 1.0f)
			{
				PlayInfo.fPhase = 1.0f;
				PlayInfo.eBlendType = EBlend_None;
				return false;
			}
		}

		// quick logarithmic conversion
		float fLog = cry_logf(PlayInfo.fPhase * 1.71f + 1.0f);

		*pDataOut = (signed short)((float)(*pDataOut) * fLog);
		pDataOut++;
		*pDataOut = (signed short)((float)(*pDataOut) * fLog);
		pDataOut++;

		if (PlayInfo.eBlendType == EBlend_FadeOut)
			PlayInfo.fPhase -= fPhaseAdder;
		else if (PlayInfo.eBlendType == EBlend_FadeIn)
			PlayInfo.fPhase += fPhaseAdder;
	}
	return true;
}

/* all secondary layers (rhythmic, incidental) will be faded out over fFadeTime seconds
 */
void CMusicSystem::FadeOutAllSecondaryLayers(double fFadeTime)
{
	for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != m_vecPlayingPatterns.end(); ++It)
	{
		SMusicPatternPlayInfo& PlayInfo = (*It);

		if (PlayInfo.ePlayingType == EPlaying_Stop)
			continue;

		PlayInfo.eBlendType = EBlend_FadeOut;
		PlayInfo.fFadeTime = fFadeTime;
	}
}

/* create a new instance of pattern pszPattern
 */
CMusicPatternInstance* CMusicSystem::GetPatternInstance(const char* pszPattern)
{
	std::lock_guard lock(g_mutex);

	CMusicPattern* pPattern = FindPattern(pszPattern);
	if (!pPattern)
	{
		LogMsg(1, "[Warning] <Music> Pattern %s could not be found !", pszPattern);
		return nullptr;
	}

	return pPattern->CreateInstance();
}

/* check if a new pattern-set needs to be chosen
 */
bool CMusicSystem::UpdateCurrentPatternSet(SMusicMood* pMood, int nSamples, bool bAllowChange)
{
	std::lock_guard lock(g_mutex);

	if (!pMood)
		return false;

	pMood->fCurrPatternSetTime += (float)nSamples / (float)m_nSampleRate; // update timing

	if (bAllowChange)
	{
		bool bChooseNewPatternSet =
		    ((pMood->pCurrPatternSet == NULL) || (pMood->fCurrPatternSetTime >= pMood->fCurrPatternSetTimeout));

		if (bChooseNewPatternSet)
		{
			// yes... lets choose a new pattern-set
			ChoosePatternSet(pMood, !pMood->bPlayFromStart);
			return (pMood->pCurrPatternSet != NULL);
		}
	}
	return true;
}

/* choose a new pattern-set and calc its timeout
 */
bool CMusicSystem::ChoosePatternSet(SMusicMood* pMood, const bool bUseSameIndex)
{
	std::lock_guard lock(g_mutex);

	if (!pMood)
		return false;

	SMusicPatternSet* pPrevPatternSet = pMood->pCurrPatternSet;
	int nPatternSets = (int)pMood->vecPatternSets.size();
	SMusicPatternSet* pPatternSet = NULL;
	int nCount = 0;

	switch (nPatternSets)
	{
		case 0:
			LogMsg(1, "[Warning] <Music> Choosing NULL pattern-set.");
			return false;
		case 1:
			pPatternSet = *(pMood->vecPatternSets.begin());
			pMood->nPatternRandIndex = 1;
			break;
		default:
			if (bUseSameIndex)
				pMood->nPatternRandIndex = m_pCurrMood->nPatternRandIndex;
			else
				pMood->nPatternRandIndex =
				    (int)m_RandGen.Rand(0.0f, (float)nPatternSets) % nPatternSets;

			TPatternSetVecIt ItEnd = pMood->vecPatternSets.end();
			for (TPatternSetVecIt It = pMood->vecPatternSets.begin(); It != ItEnd; ++It)
			{
				if (nCount == pMood->nPatternRandIndex)
				{
					pPatternSet = (*It);
					while (pPatternSet == pPrevPatternSet) // make sure we select a different set
					{
						++It;
						nCount++;
						if (It == pMood->vecPatternSets.end())
						{
							It = pMood->vecPatternSets.begin();
							nCount = 0;
						}
						pPatternSet = (*It);
					}
					break;
				}
				nCount++;
			}
			// FadeOutAllSecondaryLayers(2.0);
			break;
	}

	assert(pPatternSet);
	if (!pPatternSet)
		return false;

	pMood->fCurrPatternSetTime = 0.0f;
	pMood->fCurrPatternSetTimeout = m_RandGen.Rand(pPatternSet->fMinTimeout, pPatternSet->fMaxTimeout);
	LogMsg(1, "<Music> Choosing pattern-set %d with a timeout of %3.1f seconds.", nCount,
	       pMood->fCurrPatternSetTimeout);
	pMood->pCurrPatternSet = pPatternSet;

	// we have to choose a new next pattern in order to reflect the new pattern set
	// m_pNextPattern=ChoosePattern(pMood, MUSICLAYER_MAIN);
	// m_NextPatternType = MUSICLAYER_MAIN;

	return true;
}

/* return bridge from pCurrTheme to pNewTheme; NULL if no such bridge exist
 */
CMusicPatternInstance* CMusicSystem::ChooseBridge(SMusicTheme* pCurrTheme, SMusicTheme* pNewTheme)
{
	std::lock_guard lock(g_mutex);

	if ((!pCurrTheme) || (!pNewTheme))
		return NULL;

	TThemeBridgeMapIt BridgeIt = pCurrTheme->mapBridges.find(pNewTheme->sName);

	if (BridgeIt == pCurrTheme->mapBridges.end())
		return NULL; // no bridge defined

	CMusicPatternInstance* pBridgePattern = GetPatternInstance(BridgeIt->second.c_str());

	if (pBridgePattern)
		m_bBridging = true; // set bridging flag

	// if (m_bBridging)
	// MTRACE("Setting bridging-flag.");

	return pBridgePattern;
}

/* choose a pattern and return its instance for pMood in nLayer
 */
CMusicPatternInstance* CMusicSystem::ChoosePattern(SMusicMood* pMood, int nLayer)
{
	std::lock_guard lock(g_mutex);

	if (!pMood)
		return NULL;

	if (m_patterns.empty())
		return NULL;

	TPatternDefVec* pPatternInfo = NULL;
	float fTotalProbability;
	bool bPlayAtOnce = true;

	switch (nLayer)
	{
		default:
		case MUSICLAYER_MAIN:
			if (m_bBridging)
			{
				// MTRACE("Returning bridging pattern blindly...");
				return m_pNextTrack; // if we're bridging we return the "NextPattern" since we want to
				                     // continue the bridging
			}
			// if (!m_pCurrPattern && !pMood->pCurrPatternSet->vecStartPatterns.empty())
			//{
			//	// if no pattern is playing and start is available
			//	pPatternInfo=&(pMood->pCurrPatternSet->vecStartPatterns);
			//	fTotalProbability=0;
			// }
			// else
			//{
			pPatternInfo = &(pMood->pCurrPatternSet->vecMainPatterns);
			fTotalProbability = pMood->pCurrPatternSet->fTotalMainPatternProbability;
			//			}
			break;
		case MUSICLAYER_RHYTHMIC:
			pPatternInfo = &(pMood->pCurrPatternSet->vecRhythmicPatterns);
			fTotalProbability = pMood->pCurrPatternSet->fTotalRhythmicPatternProbability;
			break;
		case MUSICLAYER_INCIDENTAL:
			pPatternInfo = &(pMood->pCurrPatternSet->vecIncidentalPatterns);
			fTotalProbability = pMood->pCurrPatternSet->fTotalIncidentalPatternProbability;
			break;
		case MUSICLAYER_STINGER:
			pPatternInfo = &(pMood->pCurrPatternSet->vecStingerPatterns);
			fTotalProbability = pMood->pCurrPatternSet->fTotalStingerPatternProbability;
			break;
		case MUSICLAYER_START:
			pPatternInfo = &(pMood->pCurrPatternSet->vecStartPatterns);
			fTotalProbability = 0;
			break;
		case MUSICLAYER_END:
			pPatternInfo = &(pMood->pCurrPatternSet->vecEndPatterns);
			fTotalProbability = 0;
			bPlayAtOnce = false;
			break;
	}
	if ((!pPatternInfo) || (pPatternInfo->empty()))
		return NULL; // no pattern could be found in the requested layer
	// lets choose one accourding to their probability
	//	float fProb = fTotalProbability;
	float fRand = m_RandGen.Rand(0.0f, fTotalProbability);
	fTotalProbability = 0.0f;

	const char* sPattern = "";

	// iterate through patterns and retrieve the one which was chosen by fTotalProbability
	TPatternDefVecIt InfoIt;
	for (InfoIt = pPatternInfo->begin(); InfoIt != pPatternInfo->end(); ++InfoIt)
	{
		SPatternDef* pPattern = (*InfoIt);
		fTotalProbability += pPattern->fProbability;
		if (fRand <= fTotalProbability)
		{
			sPattern = pPattern->sName.c_str();
			break;
		}
	}

	assert(sPattern[0]);
	if (nLayer == MUSICLAYER_INCIDENTAL) // make sure we dont choose a pattern which is already played
	{
		bool bPatternFound;
		int nTries = 0;
		do
		{
			bPatternFound = true;
			for (SMusicPatternPlayInfo& info : m_vecPlayingPatterns)
			{
				if (_stricmp(info.pPatternInstance->GetPattern()->GetName(), sPattern) == 0)
				{
					bPatternFound = false;
					break;
				}
			}
			if (!bPatternFound)
			{
				InfoIt++;
				if (InfoIt == pPatternInfo->end())
					InfoIt = pPatternInfo->begin();

				SPatternDef* pPattern = (*InfoIt);
				sPattern = pPattern->sName.c_str();
				nTries++;

				if (nTries == (int)pPatternInfo->size())
					return NULL; // nothing found
			}
		}
		while (!bPatternFound);
	}
	CMusicPatternInstance* pInstance = GetPatternInstance(sPattern);

	if (pInstance)
	{
		// pInstance->m_bPlayFromStart = pMood->bPlayFromStart;
		pInstance->m_bPlaySingle = pMood->bPlaySingle;
		pInstance->m_bPlayAtOnce = bPlayAtOnce;
		pInstance->SetMood(pMood);
		pMood->bMainSpawned = pMood->bMainSpawned || (nLayer == MUSICLAYER_MAIN);
	}

	return pInstance;
}

void CMusicSystem::GetMemoryUsage(class ICrySizer* pSizer) const
{
	if (!pSizer->Add(*this))
		return;

	if (m_pMixBuffer)
		pSizer->AddObject(m_pMixBuffer, (int)((float)m_nSampleRate * m_fLatency) * m_nBytesPerSample);

	{
		SIZER_COMPONENT_NAME(pSizer, "Patterns");
		for (auto& [name, pattern] : m_patterns)
		{
			const_cast<CMusicPattern&>(pattern).GetMemoryUsage(pSizer);
		}

		TPatternPlayInfoVecItConst ItEnd2 = m_vecPlayingPatterns.end();
		for (TPatternPlayInfoVecItConst PatternIt2 = m_vecPlayingPatterns.begin(); PatternIt2 != ItEnd2;
		     ++PatternIt2)
		{
			const SMusicPatternPlayInfo& PlayInfo = (*PatternIt2);
			PlayInfo.pPatternInstance->GetPattern()->GetMemoryUsage(pSizer);
		}
	}

	{
		// SIZER_COMPONENT_NAME(pSizer, "Meta Data");
		TThemeMapItConst ItEnd3 = m_mapThemes.end();
		for (TThemeMapItConst ThemeIt = m_mapThemes.begin(); ThemeIt != ItEnd3; ++ThemeIt)
		{
			SMusicTheme* pTheme = ThemeIt->second;
			pSizer->AddObject(pTheme, sizeof(SMusicTheme));

			for (TMoodMapItConst MoodIt = pTheme->mapMoods.begin(); MoodIt != pTheme->mapMoods.end();
			     ++MoodIt)
			{
				SMusicMood* pMood = MoodIt->second;
				pSizer->AddObject(pMood, sizeof(SMusicMood));
			}
		}
	}

	// m_musicStateNode
}

void CMusicSystem::RenamePattern(const char* sOldName, const char* sNewName)
{
	if (!sOldName || !*sOldName || !sNewName || !*sNewName)
	{
		return;
	}

	std::lock_guard lock(g_mutex);

	const auto oldIt = m_patterns.find(sOldName);
	if (oldIt != m_patterns.end())
	{
		const auto [newIt, added] = m_patterns.emplace(sNewName, std::move(oldIt->second));
		if (added)
		{
			m_patterns.erase(oldIt);
		}
	}
}

void CMusicSystem::UpdatePatternDef(SPatternDef* pPatternDef)
{
	std::lock_guard lock(g_mutex);

	// Update pattern.
	CMusicPattern* pPattern = FindPattern(pPatternDef->sName.c_str());
	if (pPattern)
	{
		if (!m_bPlaying) // only update if music was stopped before
		{
			// Update existing pattern.
			if (_stricmp(pPatternDef->sFilename.c_str(), pPattern->GetFilename()) != 0)
			{
				LogMsg(1,
				       "[Warning] <Music> Pattern %s already found with references to different file "
				       "name. Better stop music.",
				       pPatternDef->sName.c_str());
				pPattern->SetFilename(pPatternDef->sFilename.c_str());
			}
			pPattern->ClearFadePoints();

			for (TIntVecIt IntIt = pPatternDef->vecFadePoints.begin();
			     IntIt != pPatternDef->vecFadePoints.end(); ++IntIt)
			{
				pPattern->AddFadePoint(*IntIt);
			}
			pPattern->SetPreFadeIn(pPatternDef->nPreFadeIn);
			pPattern->SetLayeringVolume(pPatternDef->fLayeringVolume);
		}
	}
	else
	{
		// Add this pattern.
		pPattern = AddPattern(pPatternDef->sName.c_str(), pPatternDef->sFilename.c_str());
		if (pPattern)
		{
			for (TIntVecIt IntIt = pPatternDef->vecFadePoints.begin();
			     IntIt != pPatternDef->vecFadePoints.end(); ++IntIt)
			{
				pPattern->AddFadePoint(*IntIt);
			}
			pPattern->SetPreFadeIn(pPatternDef->nPreFadeIn);
			pPattern->SetLayeringVolume(pPatternDef->fLayeringVolume);
		}
	}
}

void CMusicSystem::UpdatePattern(SMusicInfo::Pattern* pPatternInfo)
{
	std::lock_guard lock(g_mutex);

	// Update pattern.
	CMusicPattern* pPattern = FindPattern(pPatternInfo->sName);
	if (pPattern)
	{
		if (!m_bPlaying) // only update if music was stopped before
		{
			// Update existing pattern.
			if (_stricmp(pPatternInfo->sFilename, pPattern->GetFilename()) != 0)
			{
				pPattern->SetFilename(pPatternInfo->sFilename);
				/*
				// Filename changed.
				const char *pszFilename = pPatternDef->sFilename.c_str();
				if (!pPattern->Open(pszFilename))
				{
				delete pPattern;
				LogMsg(1, "[Error] <Music> Cannot load music-pattern %s (%s) !",
				pPatternDef->sName.c_str(), pszFilename); return;
				}
				*/
			}
			pPattern->ClearFadePoints();
			for (int i = 0; i < pPatternInfo->nFadePointsCount; i++)
			{
				pPattern->AddFadePoint(pPatternInfo->pFadePoints[i]);
			}
			pPattern->SetPreFadeIn(pPatternInfo->nPreFadeIn);
			pPattern->SetLayeringVolume(pPatternInfo->fLayeringVolume);
		}
	}
	else
	{
		// Add this pattern.
		pPattern = AddPattern(pPatternInfo->sName, pPatternInfo->sFilename);
		if (pPattern)
		{
			for (int i = 0; i < pPatternInfo->nFadePointsCount; i++)
			{
				pPattern->AddFadePoint(pPatternInfo->pFadePoints[i]);
			}
			pPattern->SetPreFadeIn(pPatternInfo->nPreFadeIn);
			pPattern->SetLayeringVolume(pPatternInfo->fLayeringVolume);
		}
	}
}

void CMusicSystem::PlayPattern(const char* sPattern, bool bStopPrevious, bool bPlaySynched)
{
	if (!m_musicEnable)
		return;

	std::lock_guard lock(g_mutex);

	CMusicPattern* pPattern = FindPattern(sPattern);
	if (!pPattern)
	{
		return;
	}

	// m_bForcePatternChange = true;
	if (bStopPrevious)
	{
		for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != m_vecPlayingPatterns.end(); ++It)
		{
			SMusicPatternPlayInfo& PlayInfo = (*It);
			PlayInfo.eBlendType = EBlend_FadeOut;
		}
		m_pCurrTheme = m_pNextTheme = 0;
		m_pCurrMood = 0;
		m_pNextTrack = pPattern->CreateInstance();
		// m_pNextPattern = m_pCurrPattern;
	}
	else
	{
		CMusicPatternInstance* pLayeredPattern = pPattern->CreateInstance();
		if (pLayeredPattern)
		{
			m_nLayeredIncidentalPatterns++;
			// pLayeredPattern->Seek0(nSamplesToNextFade);	// lets seek negative so it starts
			// playing at the right time...
			//  lets queue the new pattern so it gets automatically played till the end
			SMusicPatternPlayInfo PlayInfo;
			PlayInfo.nLayer = MUSICLAYER_PATTERN_BY_NAME;
			PlayInfo.pPatternInstance = pLayeredPattern;
			PlayInfo.eBlendType = EBlend_None;
			PlayInfo.ePlayingType = EPlaying_Once;
			PlayInfo.fPhase = 1.0f;
			PlayInfo.pRefCount = &m_nLayeredIncidentalPatterns;

			if (m_pCurrMood)
				PlayInfo.fFadeTime = m_pCurrMood->fFadeOutTime;

			if (bPlaySynched)
			{
				// int nSamplesToNextFadePoint = 0;

				// if (m_pTrack2)
				//	nSamplesToNextFadePoint = m_pTrack2->GetSamplesToNextFadePoint();

				// if (m_pTrack1)
				//	nSamplesToNextFadePoint = m_pTrack1->GetSamplesToNextFadePoint();

				// PlayInfo.nSamplesToWait = nSamplesToNextFadePoint;
				PlayInfo.eBlendType = EBlend_PlayOnFadePoint;
			}

			PushPatternToMixList(PlayInfo);
		}
		// m_pNextTrack = pPattern->CreateInstance();
	}
	StartPlaying(); // if music system is not turned on yet, do so
}

void CMusicSystem::DeletePattern(const char* sPattern)
{
	m_patterns.erase(sPattern);
}

bool CMusicSystem::StopLastPattern(const char* sPattern)
{
	std::lock_guard lock(g_mutex);

	CMusicPattern* pPattern = FindPattern(sPattern);
	SMusicPatternPlayInfo* pLastPlayInfo = NULL;
	for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != m_vecPlayingPatterns.end(); ++It)
	{
		SMusicPatternPlayInfo& PlayInfo = (*It);

		if (PlayInfo.pPatternInstance->GetPattern() == pPattern)
		{
			pLastPlayInfo = &PlayInfo;
		}
	}

	if (pLastPlayInfo != NULL)
	{
		pLastPlayInfo->eBlendType = EBlend_FadeOut;
		pLastPlayInfo->fFadeTime = 0.25f;

		return true;
	}

	return false;
}

bool CMusicSystem::SeekLastPattern(const char* sPattern, const float position)
{
	std::lock_guard lock(g_mutex);

	CMusicPattern* pPattern = FindPattern(sPattern);
	SMusicPatternPlayInfo* pLastPlayInfo = NULL;
	for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != m_vecPlayingPatterns.end(); ++It)
	{
		SMusicPatternPlayInfo& PlayInfo = (*It);

		if (PlayInfo.pPatternInstance->GetPattern() == pPattern)
		{
			pLastPlayInfo = &PlayInfo;
		}
	}

	if (pLastPlayInfo != NULL)
	{
		if (pLastPlayInfo->pPatternInstance->SeekPos(position * 44100))
		{
			pLastPlayInfo->bJustStarted = true;
			pLastPlayInfo->startTime.SetValue(0);
		}
		else
		{
			pLastPlayInfo->eBlendType = EBlend_FadeOut;
			pLastPlayInfo->fFadeTime = 0.25f;
		}

		return true;
	}

	return false;
}

bool CMusicSystem::GetLastPatternStartTime(const char* sPattern, CTimeValue& startTime)
{
	if (!m_bPlaying)
		return false;

	std::lock_guard lock(g_mutex);

	CMusicPattern* pPattern = FindPattern(sPattern);
	SMusicPatternPlayInfo* pLastPlayInfo = NULL;
	for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != m_vecPlayingPatterns.end(); ++It)
	{
		SMusicPatternPlayInfo& PlayInfo = (*It);

		if (PlayInfo.pPatternInstance->GetPattern() == pPattern)
		{
			pLastPlayInfo = &PlayInfo;
		}
	}

	if (pLastPlayInfo != NULL)
	{
		startTime = pLastPlayInfo->startTime;

		return true;
	}

	return false;
}

bool CMusicSystem::PlayStart(EBlendingType eBlendType)
{
	if (!m_musicEnable)
		return false;

	// if (!GetPlayingPatternInfo(MUSICLAYER_START, EBlend_Any))
	{
		// pick one
		TPatternInstancePtr pNewPattern = ChoosePattern(m_pCurrMood, MUSICLAYER_START);

		// CMusicPatternInstance *m_pStinger=ChoosePattern(m_pCurrMood, MUSICLAYER_STINGER);
		if (pNewPattern)
		{
			// lets queue the new pattern so it gets automatically played till the end
			SMusicPatternPlayInfo PlayInfo;
			PlayInfo.nLayer = MUSICLAYER_START;
			PlayInfo.pPatternInstance = pNewPattern;
			PlayInfo.eBlendType = eBlendType;
			PlayInfo.ePlayingType = EPlaying_Once;
			PlayInfo.fPhase = 1.0f;

			PushPatternToMixList(PlayInfo);
			m_Track2Type = MUSICLAYER_START;
			m_pTrack2 = pNewPattern;

			m_bForcePatternChange = false;

			return true;
		}
	}
	return false;
}

void CMusicSystem::PlayStinger()
{
	if (!m_musicEnable)
		return;

	if (!GetPlayingPatternInfo(MUSICLAYER_STINGER, EBlend_Any))
	{
		// pick one
		TPatternInstancePtr pNewPattern = ChoosePattern(m_pCurrMood, MUSICLAYER_STINGER);

		// CMusicPatternInstance *m_pStinger=ChoosePattern(m_pCurrMood, MUSICLAYER_STINGER);
		if (pNewPattern)
		{
			// m_nLayeredRhythmicPatterns++;
			// pLayeredPattern->Seek0(nSamplesToNextFade);	// lets seek negative so it starts playing at
			// the right time...
			//  lets queue the new pattern so it gets automatically played till the end
			SMusicPatternPlayInfo PlayInfo;
			PlayInfo.nLayer = MUSICLAYER_STINGER;
			PlayInfo.pPatternInstance = pNewPattern;
			PlayInfo.eBlendType = EBlend_None;
			PlayInfo.ePlayingType = EPlaying_Once;
			PlayInfo.fPhase = 1.0f;
			PushPatternToMixList(PlayInfo);
			// m_CurrPatternType = MUSICLAYER_STINGER;
			// m_pCurrPattern = pNewPattern;

			// PlayPattern()
		}
	}
}

void CMusicSystem::LoadPatternFromXML(XmlNodeRef node, SPatternDef* pPattern)
{
	// Loading.
	pPattern->sName = node->getAttr("Name");
	node->getAttr("Probability", pPattern->fProbability);
	node->getAttr("LayeringVolume", pPattern->fLayeringVolume);
	pPattern->fLayeringVolume = min(1.0f, pPattern->fLayeringVolume);
	pPattern->sFilename = node->getAttr("Filename");
	node->getAttr("PreFadeIn", pPattern->nPreFadeIn);

	const char* sFadePoints = node->getAttr("FadePoints");
	if (sFadePoints[0])
	{
		char sPoints[4096];
		strncpy(sPoints, sFadePoints, sizeof(sPoints));
		sPoints[sizeof(sPoints) - 1] = '\0';

		char* token = strtok(sPoints, ",");
		while (token != NULL)
		{
			pPattern->vecFadePoints.push_back(atoi(token));
			token = strtok(NULL, ",");
		}
	}

	UpdatePatternDef(pPattern);
}

void CMusicSystem::LoadMoodFromXML(XmlNodeRef& node, SMusicMood* pMood)
{
	pMood->sName = node->getAttr("Name");
	node->getAttr("PlaySingle", pMood->bPlaySingle);
	node->getAttr("Priority", pMood->nPriority);
	node->getAttr("FadeOutTime", pMood->fFadeOutTime);
	if (node->getChildCount() > 0)
	{
		// Save pattern sets.
		for (int i = 0; i < node->getChildCount(); i++)
		{
			XmlNodeRef nodePtrnSet = node->getChild(i);

			SMusicPatternSet* pPatternSet = new SMusicPatternSet;
			pMood->vecPatternSets.push_back(pPatternSet);

			nodePtrnSet->getAttr("MaxTimeout", pPatternSet->fMaxTimeout);
			nodePtrnSet->getAttr("MinTimeout", pPatternSet->fMinTimeout);
			nodePtrnSet->getAttr("IncidentalLayerProbability", pPatternSet->fIncidentalLayerProbability);
			nodePtrnSet->getAttr("RhythmicLayerProbability", pPatternSet->fRhythmicLayerProbability);
			nodePtrnSet->getAttr("MaxSimultaneousIncidentalPatterns",
			                     pPatternSet->nMaxSimultaneousIncidentalPatterns);
			nodePtrnSet->getAttr("MaxSimultaneousRhythmicPatterns",
			                     pPatternSet->nMaxSimultaneousRhythmicPatterns);

			// Save patterns.
			XmlNodeRef nodeMainLayer = nodePtrnSet->findChild("MainLayer");
			XmlNodeRef nodeRhythmicLayer = nodePtrnSet->findChild("RhythmicLayer");
			XmlNodeRef nodeIncidentalLayer = nodePtrnSet->findChild("IncidentalLayer");
			XmlNodeRef nodeStartLayer = nodePtrnSet->findChild("StartLayer");
			XmlNodeRef nodeEndLayer = nodePtrnSet->findChild("EndLayer");
			XmlNodeRef nodeStingerLayer = nodePtrnSet->findChild("StingerLayer");

			if (nodeMainLayer)
			{
				for (int j = 0; j < nodeMainLayer->getChildCount(); j++)
				{
					XmlNodeRef childNode = nodeMainLayer->getChild(j);
					SPatternDef* pPattern = new SPatternDef;
					pPatternSet->vecMainPatterns.push_back(pPattern);
					LoadPatternFromXML(childNode, pPattern);
				}
			}
			if (nodeRhythmicLayer)
			{
				for (int j = 0; j < nodeRhythmicLayer->getChildCount(); j++)
				{
					XmlNodeRef childNode = nodeRhythmicLayer->getChild(j);
					SPatternDef* pPattern = new SPatternDef;
					pPatternSet->vecRhythmicPatterns.push_back(pPattern);
					LoadPatternFromXML(childNode, pPattern);
				}
			}
			if (nodeIncidentalLayer)
			{
				for (int j = 0; j < nodeIncidentalLayer->getChildCount(); j++)
				{
					XmlNodeRef childNode = nodeIncidentalLayer->getChild(j);
					SPatternDef* pPattern = new SPatternDef;
					pPatternSet->vecIncidentalPatterns.push_back(pPattern);
					LoadPatternFromXML(childNode, pPattern);
				}
			}
			if (nodeStartLayer)
			{
				for (int j = 0; j < nodeStartLayer->getChildCount(); j++)
				{
					XmlNodeRef childNode = nodeStartLayer->getChild(j);
					SPatternDef* pPattern = new SPatternDef;
					pPatternSet->vecStartPatterns.push_back(pPattern);
					LoadPatternFromXML(childNode, pPattern);
				}
			}
			if (nodeEndLayer)
			{
				for (int j = 0; j < nodeEndLayer->getChildCount(); j++)
				{
					XmlNodeRef childNode = nodeEndLayer->getChild(j);
					SPatternDef* pPattern = new SPatternDef;
					pPatternSet->vecEndPatterns.push_back(pPattern);
					LoadPatternFromXML(childNode, pPattern);
				}
			}
			if (nodeStingerLayer)
			{
				for (int j = 0; j < nodeStingerLayer->getChildCount(); j++)
				{
					XmlNodeRef childNode = nodeStingerLayer->getChild(j);
					SPatternDef* pPattern = new SPatternDef;
					pPatternSet->vecStingerPatterns.push_back(pPattern);
					LoadPatternFromXML(childNode, pPattern);
				}
			}

			//////////////////////////////////////////////////////////////////////////
			pPatternSet->fTotalMainPatternProbability = 0.0f;
			pPatternSet->fTotalRhythmicPatternProbability = 0.0f;
			pPatternSet->fTotalIncidentalPatternProbability = 0.0f;
			pPatternSet->fTotalStingerPatternProbability = 0.0f;
			for (int j = 0; j < (int)pPatternSet->vecMainPatterns.size(); j++)
			{
				pPatternSet->fTotalMainPatternProbability +=
				    pPatternSet->vecMainPatterns[j]->fProbability;
			}
			for (int j = 0; j < (int)pPatternSet->vecRhythmicPatterns.size(); j++)
			{
				pPatternSet->fTotalRhythmicPatternProbability +=
				    pPatternSet->vecRhythmicPatterns[j]->fProbability;
			}
			for (int j = 0; j < (int)pPatternSet->vecIncidentalPatterns.size(); j++)
			{
				pPatternSet->fTotalIncidentalPatternProbability +=
				    pPatternSet->vecIncidentalPatterns[j]->fProbability;
			}
			for (int j = 0; j < (int)pPatternSet->vecStingerPatterns.size(); j++)
			{
				pPatternSet->fTotalStingerPatternProbability +=
				    pPatternSet->vecStingerPatterns[j]->fProbability;
			}
		}
	}
}

void CMusicSystem::LoadThemeFromXML(XmlNodeRef& node, SMusicTheme* pTheme)
{
	// Loading.
	pTheme->sName = node->getAttr("Name");
	pTheme->sDefaultMood = node->getAttr("DefaultMood");
	node->getAttr("DefaultMoodTimeout", pTheme->fDefaultMoodTimeout);

	XmlNodeRef nodeMoods = node->findChild("Moods");
	if (nodeMoods)
	{
		for (int i = 0; i < nodeMoods->getChildCount(); i++)
		{
			XmlNodeRef childNode = nodeMoods->getChild(i);
			SMusicMood* pMood = new SMusicMood;
			LoadMoodFromXML(childNode, pMood);
			pTheme->mapMoods[pMood->sName.c_str()] = pMood;
		}
	}

	XmlNodeRef nodeBridges = node->findChild("Bridges");
	if (nodeBridges)
	{
		for (int i = 0; i < nodeBridges->getChildCount(); i++)
		{
			XmlNodeRef nodeBridge = nodeBridges->getChild(i);
			const char* sPattern = nodeBridge->getAttr("Pattern");
			if (sPattern[0])
			{
				pTheme->mapBridges[nodeBridges->getTag()] = (const char*)sPattern;
			}
		}
	}

	// Add this pattern to music data, if not already there
	m_mapThemes.emplace(pTheme->sName, pTheme);
}

bool CMusicSystem::LoadFromXML(const char* sFilename, bool bAddData, bool bReplaceData)
{
	if (bReplaceData || !bAddData)
	{
		StopPlaying(); // this needs to be called out of critical section
		Silence();
	}

	std::lock_guard lock(g_mutex);

	// Loading.
	if (!bAddData)
	{
		Unload();
	}

	XmlNodeRef root = gEnv->pSystem->LoadXmlFile(sFilename);
	if (!root)
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_SOUND, sFilename,
		                       "<Music> Description XML file %s failed to load", sFilename);
		return false;
	}

	// Iterate music themes
	for (int i = 0; i < root->getChildCount(); i++)
	{
		XmlNodeRef nodeTheme = root->getChild(i);
		if (nodeTheme->isTag("Theme"))
		{
			SMusicTheme* pTheme = new SMusicTheme;
			LoadThemeFromXML(nodeTheme, pTheme);
		}
	}

	m_bDataLoaded = true;

	// start playing the stream...
	if (m_pChannel == NULL)
		StartPlaying();

	return true;
}

// writes output to screen in debug
void CMusicSystem::DrawInformation(IRenderer* pRenderer, float xpos, float ypos)
{
	float fColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	float fWhite[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	float fGreen[4] = {0.0f, 0.8f, 0.0f, 1.0f};
	float fLightGreen[4] = {0.0f, 1.0f, 0.0f, 1.0f};
	float fDarkGreen[4] = {0.0f, 0.5f, 0.0f, 1.0f};
	float fRed[4] = {1.0f, 0.0f, 0.0f, 1.0f};
	float fBlue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
	float fCyan[4] = {0.0f, 1.0f, 1.0f, 1.0f};

	// uint32 nUsedMem = 0;
	ypos += 20;

	SMusicSystemStatus* pStatus = GetStatus();

	pRenderer->Draw2dLabel(xpos, ypos, 1.5f, fWhite, false, "Music %s %s",
	                       pStatus->bPlaying ? "playing" : "not playing",
	                       m_bForcePatternChange ? "Force Pattern Change" : "");
	ypos += 15;
	pRenderer->Draw2dLabel(xpos, ypos, 1.5f, fWhite, false, "Theme: %s Mood: %s", pStatus->sTheme.c_str(),
	                       pStatus->sMood.c_str());
	ypos += 15;

	pRenderer->Draw2dLabel(xpos, ypos, 1.5f, fWhite, false, "Track 1: %s",
	                       m_pTrack1 ? m_pTrack1->GetPattern()->GetName() : "");
	ypos += 15;
	pRenderer->Draw2dLabel(xpos, ypos, 1.5f, fWhite, false, "Track 2: %s",
	                       m_pTrack2 ? m_pTrack2->GetPattern()->GetName() : "");
	ypos += 15;
	pRenderer->Draw2dLabel(xpos, ypos, 1.5f, fWhite, false, "Next Pattern: %s",
	                       m_pNextTrack ? m_pNextTrack->GetPattern()->GetName() : "");
	ypos += 15;

	std::lock_guard lock(g_mutex);

	TPatternPlayInfoVecIt ItEnd = m_vecPlayingPatterns.end();
	for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != ItEnd; ++It)
	{
		SMusicPatternPlayInfo& PatternStatus = (*It);
		string sLayer;

		switch (PatternStatus.nLayer)
		{
			case MUSICLAYER_MAIN:
				memcpy(fColor, fGreen, 4 * sizeof(float));
				sLayer = "Main";
				break;
			case MUSICLAYER_START:
				memcpy(fColor, fLightGreen, 4 * sizeof(float));
				sLayer = "Start";
				break;
			case MUSICLAYER_END:
				memcpy(fColor, fDarkGreen, 4 * sizeof(float));
				sLayer = "End";
				break;
			case MUSICLAYER_STINGER:
				memcpy(fColor, fRed, 4 * sizeof(float));
				sLayer = "Stinger";
				break;
			case MUSICLAYER_INCIDENTAL:
				memcpy(fColor, fCyan, 4 * sizeof(float));
				sLayer = "Incidental";
				break;
			case MUSICLAYER_RHYTHMIC:
				memcpy(fColor, fBlue, 4 * sizeof(float));
				sLayer = "Rythmic";
				break;
			case MUSICLAYER_PATTERN_BY_NAME:
				memcpy(fColor, fBlue, 4 * sizeof(float));
				sLayer = "Pattern by Name";
				break;
			default:
				memcpy(fColor, fWhite, 4 * sizeof(float));
				sLayer = "None";
				break;
		}

		// TPatternInstancePtr pTempPattern = PatternStatus.pPatternInstance;

		// assert (pTempPattern);

		// float fVol = pTempPattern->GetPattern()->GetLayeringVolume();
		// fColor[3] = PatternStatus.fPhase+.2f;
		// uint32 nPos = pTempPattern->GetPos();
		// uint32 nLength = pTempPattern->GetPattern()->GetNumSamples();
		// float fProgress = (nLength>0)?(nPos / (float)nLength):1;
		// float fNextFade = (nLength>0)?((nPos + pTempPattern->GetSamplesToNextFadePoint()) /
		// (float)nLength):1; pRenderer->Draw2dLabel(xpos, ypos, 1.5f, fColor, false, "%s Pattern: %s Volume:
		// %1.2f Progress: %1.2f NextFade: %1.2f", (PatternStatus.nLayer==MUSICLAYER_MAIN) ? "Main" :
		// ((PatternStatus.nLayer==MUSICLAYER_RHYTHMIC) ? "Rhythmic" : "Incidental"),
		// pTempPattern->GetPattern()->GetFilename(), fVol, fProgress, fNextFade);
		////pLog->LogToConsole("    %s (Layer: %s; Volume: %s)", PatternStatus.sName.c_str(),
		///(PatternStatus.nLayer==MUSICLAYER_MAIN) ? "Main" : ((PatternStatus.nLayer==MUSICLAYER_RHYTHMIC) ?
		///"Rhythmic" : "Incidental"), PatternStatus.fVolume);
		// ypos += 15;

		float fVol = PatternStatus.pPatternInstance->GetPattern()->GetLayeringVolume();
		fColor[3] = PatternStatus.fPhase + .2f;
		uint32 nPos = PatternStatus.pPatternInstance->GetPos();
		uint32 nLength = PatternStatus.pPatternInstance->GetPattern()->GetNumSamples();
		float fProgress = (nLength > 0) ? (nPos / (float)nLength) : 1;
		float fNextFade =
		    (nLength > 0)
			? ((nPos + PatternStatus.pPatternInstance->GetSamplesToNextFadePoint()) / (float)nLength)
			: 1;

		pRenderer->Draw2dLabel(xpos, ypos, 1.5f, fColor, false,
		                       "%s Pattern: %s Volume: %1.2f Progress: %1.2f NextFade: %1.2f", sLayer.c_str(),
		                       PatternStatus.pPatternInstance->GetPattern()->GetFilename(), fVol, fProgress,
		                       fNextFade);
		// pRenderer->Draw2dLabel(xpos, ypos, 1.5f, fColor, false, "%s Pattern: %s Volume: %1.2f Progress: %1.2f
		// NextFade: %1.2f", (PatternStatus.nLayer==MUSICLAYER_MAIN) ? "Main" :
		// ((PatternStatus.nLayer==MUSICLAYER_RHYTHMIC) ? "Rhythmic" : "Incidental"),
		// PatternStatus.pPatternInstance->GetPattern()->GetFilename(), fVol, fProgress, fNextFade);
		// pLog->LogToConsole("    %s (Layer: %s; Volume: %s)", PatternStatus.sName.c_str(),
		// (PatternStatus.nLayer==MUSICLAYER_MAIN) ? "Main" : ((PatternStatus.nLayer==MUSICLAYER_RHYTHMIC) ?
		// "Rhythmic" : "Incidental"), PatternStatus.fVolume);
		ypos += 15;
	}
}

bool CMusicSystem::StreamOGG()
{
	return (1 == g_nMusicStreamedData);
}

void CMusicSystem::Serialize(TSerialize ser)
{

	string sTempName;
	string sTrack1Name;
	string sTrack2Name;
	string sNextTrackName;
	bool bWasPlaying = false;

	ser.Value("ForcePatternChange", m_bForcePatternChange);
	ser.Value("Playing", bWasPlaying);
	ser.Value("Pause", m_bPause);
	ser.Value("Enable", m_musicEnable);

	ser.Value("MasterVolume", m_fMasterVolume);
	ser.Value("DelayTheme", m_nDelayThemeInSec);

	if (ser.IsWriting())
	{
		ser.Value("Playing", m_bPlaying);

		// themes
		sTempName = m_pCurrTheme ? m_pCurrTheme->sName : "";
		ser.Value("CurrTheme", sTempName);

		sTempName = m_pNextTheme ? m_pNextTheme->sName : "";
		ser.Value("NextTheme", sTempName);

		// moods
		sTempName = m_pCurrMood ? m_pCurrMood->sName : "";
		ser.Value("CurrMood", sTempName);
		sTempName =
		    (m_pCurrMood && m_pCurrMood->pSpawnedFromTheme) ? m_pCurrMood->pSpawnedFromTheme->sName : "";
		ser.Value("CurrMoodTheme", sTempName);

		sTempName = m_pNextMood ? m_pNextMood->sName : "";
		ser.Value("NextMood", sTempName);
		sTempName =
		    (m_pNextMood && m_pNextMood->pSpawnedFromTheme) ? m_pNextMood->pSpawnedFromTheme->sName : "";
		ser.Value("NextMoodTheme", sTempName);

		ser.Value("DefaultMood", m_sDefaultMood);
		ser.Value("DefaultMoodTime", m_fDefaultMoodTime);

		// patterns
		int nNumPatterns = m_vecPlayingPatterns.size();
		ser.Value("PatternCount", nNumPatterns);

		TPatternPlayInfoVecIt ItEnd = m_vecPlayingPatterns.end();
		for (TPatternPlayInfoVecIt It = m_vecPlayingPatterns.begin(); It != ItEnd; ++It)
		{
			SMusicPatternPlayInfo* pPlayInfo = &(*It);
			ser.BeginGroup("Pattern");

			ser.Value("PatternName", pPlayInfo->pPatternInstance->GetPattern()->GetName());

			ser.Value("Layer", pPlayInfo->nLayer);
			int nBlendType = (int)pPlayInfo->eBlendType;
			ser.Value("BlendType", nBlendType);
			int nPlayingType = (int)pPlayInfo->ePlayingType;
			ser.Value("PlayingType", nPlayingType);
			float fPhase = (float)pPlayInfo->fPhase;
			ser.Value("Phase", fPhase);
			float fFadeTime = (float)pPlayInfo->fFadeTime;
			ser.Value("FadeTime", fFadeTime);
			ser.Value("SamplesToWait", pPlayInfo->nSamplesToWait);

			ser.Value("PlayFromStart", pPlayInfo->pPatternInstance->m_bPlayFromStart);
			ser.Value("PlaySingle", pPlayInfo->pPatternInstance->m_bPlaySingle);
			ser.Value("PlayAtonce", pPlayInfo->pPatternInstance->m_bPlayAtOnce);

			int nPos = pPlayInfo->pPatternInstance->GetPos();
			ser.Value("Pos", nPos);

			if (pPlayInfo->pPatternInstance == m_pTrack1)
				sTrack1Name = pPlayInfo->pPatternInstance->GetPattern()->GetName();

			if (pPlayInfo->pPatternInstance == m_pTrack2)
				sTrack2Name = pPlayInfo->pPatternInstance->GetPattern()->GetName();

			// pRefCount					=	NULL;
			// bRefCountAdjusted	=	false;

			ser.EndGroup();
		}

		if (m_pNextTrack)
			sNextTrackName = m_pNextTrack->GetPattern()->GetName();

		// tracks
		ser.Value("Track1Name", sTrack1Name);
		ser.Value("Track1Type", m_Track1Type);
		ser.Value("Track2Name", sTrack2Name);
		ser.Value("Track2Type", m_Track2Type);
		ser.Value("NextTrackName", sNextTrackName);
		ser.Value("NextTrackType", m_NextTrackType);
		if (m_pNextTrack)
		{
			ser.Value("NextTrackAtOnce", m_pNextTrack->m_bPlayAtOnce);
			ser.Value("NextTrackFromStart", m_pNextTrack->m_bPlayFromStart);
			ser.Value("NextTrackPlaySingle", m_pNextTrack->m_bPlaySingle);
		}
	}
	else
	{
		if (bWasPlaying != m_bPlaying)
			StopPlaying();

		// if more than 4 pattern play already and have stacked up by going in/out menu, just remove them and
		// restore freshly
		if (m_vecPlayingPatterns.size() > 4)
			EndTheme(EThemeFade_StopAtOnce, 0, true);

		// tracks
		ser.Value("Track1Name", sTrack1Name);
		ser.Value("Track1Type", m_Track1Type);
		ser.Value("Track2Name", sTrack2Name);
		ser.Value("Track2Type", m_Track2Type);
		ser.Value("NextTrackName", sNextTrackName);
		ser.Value("NextTrackType", m_NextTrackType);
		m_pNextTrack = NULL;

		CMusicPattern* pNextTrack = FindPattern(sNextTrackName.c_str());
		if (pNextTrack)
		{
			CMusicPatternInstance* pNextTrackInstance = pNextTrack->CreateInstance();
			if (pNextTrackInstance)
			{
				m_pNextTrack = pNextTrackInstance;
				ser.Value("NextTrackAtOnce", m_pNextTrack->m_bPlayAtOnce);
				ser.Value("NextTrackFromStart", m_pNextTrack->m_bPlayFromStart);
				ser.Value("NextTrackPlaySingle", m_pNextTrack->m_bPlaySingle);
			}
		}

		// themes
		ser.Value("CurrTheme", sTempName);
		TThemeMapIt ThemeIt = m_mapThemes.find(CONST_TEMP_STRING(sTempName.c_str()));
		if (ThemeIt != m_mapThemes.end())
			m_pCurrTheme = ThemeIt->second;
		else
			m_pCurrTheme = NULL;

		ser.Value("NextTheme", sTempName);
		ThemeIt = m_mapThemes.find(CONST_TEMP_STRING(sTempName.c_str()));
		if (ThemeIt != m_mapThemes.end())
			m_pNextTheme = ThemeIt->second;
		else
			m_pNextTheme = NULL;

		// moods
		m_pCurrMood = NULL;
		ser.Value("CurrMoodTheme", sTempName);
		ThemeIt = m_mapThemes.find(CONST_TEMP_STRING(sTempName.c_str()));
		if (ThemeIt != m_mapThemes.end())
		{
			SMusicTheme* pTheme = ThemeIt->second;
			ser.Value("CurrMood", sTempName);

			TMoodMapIt MoodIt = pTheme->mapMoods.find(CONST_TEMP_STRING(sTempName.c_str()));

			if (MoodIt != pTheme->mapMoods.end())
				m_pCurrMood = MoodIt->second;
		}

		m_pNextMood = NULL;
		ser.Value("NextMoodTheme", sTempName);
		ThemeIt = m_mapThemes.find(CONST_TEMP_STRING(sTempName.c_str()));
		if (ThemeIt != m_mapThemes.end())
		{
			SMusicTheme* pTheme = ThemeIt->second;
			ser.Value("NextMood", sTempName);

			TMoodMapIt MoodIt = pTheme->mapMoods.find(CONST_TEMP_STRING(sTempName.c_str()));

			if (MoodIt != pTheme->mapMoods.end())
				m_pNextMood = MoodIt->second;
		}

		ser.Value("DefaultMood", m_sDefaultMood);
		ser.Value("DefaultMoodTime", m_fDefaultMoodTime);

		// patterns
		int nNumPatterns = 0;
		ser.Value("PatternCount", nNumPatterns);

		for (int i = 0; i < nNumPatterns; ++i)
		{
			SMusicPatternPlayInfo NewPlayInfo;
			ser.BeginGroup("Pattern");

			ser.Value("PatternName", sTempName);
			CMusicPattern* pPattern = FindPattern(sTempName.c_str());
			if (pPattern)
			{
				CMusicPatternInstance* pPatternInstance = pPattern->CreateInstance();
				if (pPatternInstance)
				{
					ser.Value("Layer", NewPlayInfo.nLayer);
					int nBlendType = 0;
					ser.Value("BlendType", nBlendType);
					NewPlayInfo.eBlendType = (EBlendingType)nBlendType;
					int nPlayingType = 0;
					ser.Value("PlayingType", nPlayingType);
					NewPlayInfo.ePlayingType = (EPlayingType)nPlayingType;
					float fPhase = 0.0f;
					ser.Value("Phase", fPhase);
					NewPlayInfo.fPhase = fPhase;
					float fFadeTime = 0.0f;
					ser.Value("FadeTime", fFadeTime);
					NewPlayInfo.fFadeTime = fFadeTime;
					ser.Value("SamplesToWait", NewPlayInfo.nSamplesToWait);

					NewPlayInfo.pPatternInstance = pPatternInstance;

					ser.Value("PlayFromStart", pPatternInstance->m_bPlayFromStart);
					ser.Value("PlaySingle", pPatternInstance->m_bPlaySingle);
					ser.Value("PlayAtonce", pPatternInstance->m_bPlayAtOnce);

					unsigned int nPos = 0;
					ser.Value("Pos", nPos);
					pPatternInstance->SeekPos(nPos);

					PushPatternToMixList(NewPlayInfo);

					if (sTrack1Name == sTempName)
						m_pTrack1 = pPatternInstance;

					if (sTrack2Name == sTempName)
						m_pTrack2 = pPatternInstance;
				}

				//	//pRefCount					=	NULL;
				//	//bRefCountAdjusted	=	false;
			}
			ser.EndGroup();
		}
	}
}

bool CMusicSystem::SerializeInternal(bool bSave)
{
	IXmlSerializer* pSerializer = gEnv->pSystem->GetXmlUtils()->CreateXmlSerializer();
	if (pSerializer == 0)
		return false;

	ISerialize* pSer = 0;
	if (bSave)
	{
		// if (m_musicStateNode == 0)
		{
			m_musicStateNode = gEnv->pSystem->CreateXmlNode("MusicState");
			pSer = pSerializer->GetWriter(m_musicStateNode);
		}
	}
	else
	{
		if (m_musicStateNode != 0)
			pSer = pSerializer->GetReader(m_musicStateNode);
	}

	bool bSerialize = pSer != 0;
	if (bSerialize)
	{
		TSerialize ser = TSerialize(pSer);
		Serialize(ser);
	}

	pSerializer->Release();

	// if (bSerialize)
	//{
	//	if (bSave)
	//	{
	//		StopPlaying();
	//		Silence();
	//	}
	//	else
	//	{
	//		m_musicStateNode = 0;
	//		StartPlaying();
	//	}
	// }
	return true;
}
