#include "CryCommon/CrySystem/gEnv.h"
#include "CryCommon/CrySystem/IConsole.h"

#include "MusicCVars.h"

CMusicCVars::CMusicCVars()
{
	IConsole* pConsole = gEnv->pConsole;

	pConsole->Register("s_DebugMusic", &g_nDebugMusic, 0, VF_DUMPTODISK,
	                   "Changes music-debugging verbosity level.\n"
	                   "Usage: s_DebugMusic [0/4]\n"
	                   "Default is 0 (off). Set to 1 (up to 4) to debug music.");

	pConsole->Register("s_MusicEnable", &g_nMusicEnable, 1, VF_DUMPTODISK, "enable/disable music");

	pConsole->Register("s_MusicMaxPatterns", &g_nMusicMaxPatterns, 12, VF_DUMPTODISK,
	                   "Max simultaniously playing music patterns.\n");

	pConsole->Register("s_MusicStreamedData", &g_nMusicStreamedData, 1, VF_DUMPTODISK,
	                   "Data used for streaming music data.\n0 - (AD)PCM\n1 - OGG");

	pConsole->Register("s_MusicSpeakerFrontVolume", &g_fMusicSpeakerFrontVolume, 1.0f, VF_DUMPTODISK,
	                   "Sets the volume of the front speakers.\n"
	                   "Usage: s_MusicSpeakerFrontVolume 1.0"
	                   "Default is 1.0.");

	pConsole->Register("s_MusicSpeakerBackVolume", &g_fMusicSpeakerBackVolume, 0.0f, VF_DUMPTODISK,
	                   "Sets the volume of the back speakers.\n"
	                   "Usage: s_MusicSpeakerBackVolume 0.3"
	                   "Default is 0.0.");

	pConsole->Register("s_MusicSpeakerCenterVolume", &g_fMusicSpeakerCenterVolume, 0.0f, VF_DUMPTODISK,
	                   "Sets the volume of the center speakers (front and back).\n"
	                   "Usage: s_MusicSpeakerCenterVolume 0.0"
	                   "Default is 0.0.");

	pConsole->Register("s_MusicSpeakerSideVolume", &g_fMusicSpeakerSideVolume, 0.5f, VF_DUMPTODISK,
	                   "Sets the volume of the side speakers.\n"
	                   "Usage: s_MusicSpeakerSideVolume 0.2"
	                   "Default is 0.5.");

	pConsole->Register("s_MusicSpeakerLFEVolume", &g_fMusicSpeakerLFEVolume, 0.5f, VF_DUMPTODISK,
	                   "Sets the volume of the LFE speaker.\n"
	                   "Usage: s_MusicSpeakerLFEVolume 0.2"
	                   "Default is 0.5.");
}

CMusicCVars::~CMusicCVars()
{
	IConsole* pConsole = gEnv->pConsole;

	pConsole->UnregisterVariable("s_DebugMusic");
	pConsole->UnregisterVariable("s_MusicEnable");
	pConsole->UnregisterVariable("s_MusicMaxPatterns");
	pConsole->UnregisterVariable("s_MusicStreamedData");

	pConsole->UnregisterVariable("s_MusicSpeakerFrontVolume");
	pConsole->UnregisterVariable("s_MusicSpeakerBackVolume");
	pConsole->UnregisterVariable("s_MusicSpeakerCenterVolume");
	pConsole->UnregisterVariable("s_MusicSpeakerSideVolume");
	pConsole->UnregisterVariable("s_MusicSpeakerLFEVolume");
}
