#pragma once

struct SMusicPatternFileInfo
{
	string sFilename;
	int nHeaderSize = 0;
	int nSamples = 0;
};

struct SWaveHdr
{
	char RIFF[4];         // "RIFF" tag
	uint32 dwSize;        // Size of data to follow
	char WAVE[4];         // "WAVE" tag
	char fmt_[4];         // "fmt " tag
	uint32 dw16;          // 16
	uint16 wOne_0;        // 1
	uint16 wChnls;        // Number of Channels
	uint32 dwSRate;       // Sample Rate
	uint32 BytesPerSec;   // Bytes per second
	uint16 wBlkAlign;     // Block align
	uint16 BitsPerSample; // Sample size
	char DATA[4];         // "DATA"
	uint32 dwDSize;       // Number of Samples
};

enum EWaveFormatIDs
{
	WAVE_FORMAT_ID_PCM = 0x0001,
	WAVE_FORMAT_ID_ADPCM = 0x0002,
};

struct IMusicPatternDecoderInstance
{
	virtual void Release() = 0;

	//! Seek to beginning of pattern (if nDelay is set it will wait nDelay-samples before starting playback).
	virtual bool Seek0(const int nDelay = 0) = 0;

	//! Seek to pos in number of samples of pattern.
	virtual bool SeekPos(const int nSamplePos) = 0;

	//! Retrieve the current position in the file (in samples).
	virtual int GetPos() = 0;

	//! Decode and retrieve pcm-data (stereo/16bit).
	virtual bool GetPCMData(signed long* pDataOut, int nSamples, bool bLoop = true) = 0;
};

struct IMusicPatternDecoder
{
	virtual void Release() = 0;
	//! Open a music-pattern-file.
	virtual bool Open(const char* pszFilename) = 0;
	//! Close a music-pattern-file.
	virtual bool Close() = 0;
	//! Retrieve file-info.
	virtual bool GetFileInfo(SMusicPatternFileInfo& FileInfo) = 0;
	//! Retrieve mem-info.
	virtual void GetMemoryUsage(class ICrySizer* pSizer) = 0;
	//! Create instance.
	virtual IMusicPatternDecoderInstance* CreateInstance() = 0;
};
