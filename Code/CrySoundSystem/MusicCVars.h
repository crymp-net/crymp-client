#pragma once

class CMusicCVars
{
public:
	CMusicCVars();
	virtual ~CMusicCVars();

	int g_nDebugMusic;
	int g_nMusicEnable;
	int g_nMusicMaxPatterns;
	int g_nMusicStreamedData;
	float g_fMusicSpeakerFrontVolume;
	float g_fMusicSpeakerBackVolume;
	float g_fMusicSpeakerCenterVolume;
	float g_fMusicSpeakerSideVolume;
	float g_fMusicSpeakerLFEVolume;
};
