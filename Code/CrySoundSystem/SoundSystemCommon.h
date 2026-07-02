#pragma once

#include "CryCommon/CrySoundSystem/ISound.h"

struct ISystem;
struct ICVar;

class CSoundSystemCommon : public ISoundSystem
{
public:
	CSoundSystemCommon();
	virtual ~CSoundSystemCommon();
	bool DebuggingSound();

	// System
	int g_nDummySound;
	int g_nSoundEnable;
	int g_nSpeakerConfig;
	int g_nOutputConfig;
	int g_nReverbType;
	int g_nReverbInstances;

	// Format
	int g_nFormatSampleRate;
	int g_nFormatResampler;
	int g_nFormatType;

	// Tweaking
	int g_nCacheSize;
	int g_nCompressedDialog;
	int g_nSoftwareChannels;
	int g_nMPEGDecoders;
	int g_nXMADecoders;
	int g_nADPCMDecoders;
	float g_fInactiveSoundIterationTimeout;

	int g_nMaxChannels;
	int g_nMaxActiveSounds;
	int g_nPriorityThreshold;
	int g_nMinHWChannels; // not used in Crysis
	int g_nMaxHWChannels; // not used in Crysis

	float g_fMinRepeatSoundTimeout; // implement ?

	int g_nVisAreaPropagation;
	int g_nPrecacheData;
	int g_nUnloadData;
	int g_nLoadNonBlocking;

	float g_fVariationLimiter;

	// Features

	// ICVar	*m_pCVarEnableSoundFX; // toggles DSP
	int g_nSoundMoods;
	int g_nSoundMoodsDSP;

	int g_nObstruction;
	float g_fObstructionMaxValue;
	int g_nObstructionAccuracy;
	float g_fObstructionUpdate;
	float g_fObstructionVisArea;
	float g_fObstructionMaxRadius;
	float g_fObstructionMaxPierecability;

	// ICVar	*m_pCVarEnableHDRA; // implement
	int g_nNetworkAudition;
	int g_nMPEGCompression; // prefer mpeg or wav
	int g_nLanguagesConversion;

	int g_nDoppler;
	float g_fDopplerScale;

	int g_nHRTF_DSP;
	int g_nVol0TurnsVirtual;

	// Volume
	float g_fSFXVolume;
	float g_fGameSFXVolume;
	float g_fMusicVolume;
	float g_fGameMusicVolume;
	float g_fDialogVolume;
	float g_fGameDialogVolume;

	// Debug
	int g_nSoundInfo;
	int g_nDebugSound;
	int g_nDrawSounds;
	int g_nProfiling;
	int g_nPlaybackFilter;
	int g_nStopSoundsImmediately;
	int g_nErrorSound;
	int g_nDrawObstruction;
	int g_nDumpEventStructure;

	// old
	// ICVar *m_pCVarReverbPresetMix; // remove
	// ICVar *m_pCVarCapsCheck;
};
