#pragma once

#include "PatternDecoder.h"

struct IMusicSystem;
struct ICryPak;

struct SPCMFileInfo final
{
	SPCMFileInfo() { nBytesPerSample = 0; }
	int nBytesPerSample;
};

class CPCMDecoder final : public IMusicPatternDecoder
{
protected:
	IMusicSystem* m_pMusicSystem;
	ICryPak* m_pPak;
	FILE* m_pFile;
	int m_nPosBytes; // current position (in bytes) in the file to reduce seeking-overhead
	SMusicPatternFileInfo m_FileInfo;
	SPCMFileInfo m_PCMFileInfo;
	friend class CPCMDecoderInstance;

private:
	bool m_b44KHz; // keeps track of encoded frequency

	int ReadFile(void* pBuf, int nSize, int nCount, int* pNewFilePos = NULL);
	bool InitStreamWAV();

protected:
	virtual ~CPCMDecoder();

public:
	CPCMDecoder(IMusicSystem* pMusicSystem);
	void Release() { delete this; }
	//! Open a music-pattern-file.
	bool Open(const char* pszFilename);
	//! Close a music-pattern-file.
	bool Close();
	//! Retrieve file-info
	bool GetFileInfo(SMusicPatternFileInfo& FileInfo);
	//! Retrieve mem-info
	void GetMemoryUsage(class ICrySizer* pSizer);
	//! Create instance.
	IMusicPatternDecoderInstance* CreateInstance();
	//! retrieves frequency encoding
	const bool Is44KHz() const { return m_b44KHz; }
};

class CPCMDecoderInstance final : public IMusicPatternDecoderInstance
{
	static const unsigned int scuiEncodedBlockSize = 2048; // block size for static allocation

protected:
	CPCMDecoder* m_pDecoder;
	int m_nPos;                // position in samples
	int m_nPosBytes;           // file position in bytes
	bool m_bCopyFromLastFrame; // indicates some copying from last frame (due to odd sample count request)
	signed long m_lLastSample; // sample to reuse for next frame
private:
	bool SeekBytes(int nBytes);

protected:
	// static allocated encoded block, only needed for 22 kHz mode
	unsigned int m_aEncodedBlock[scuiEncodedBlockSize];
	//! fills a dest buffer with uncompressed data
	const bool FillPCMBuffer22KHz(signed long* pBuffer, int nSamples);

	virtual ~CPCMDecoderInstance();

public:
	CPCMDecoderInstance(CPCMDecoder* pDecoder);

	virtual void Release() { delete this; }

	//! Seek to beginning of pattern (if nDelay is set it will wait nDelay-samples before starting playback).
	virtual bool Seek0(const int nDelay = 0);

	//! Seek to pos in number of samples of pattern.
	virtual bool SeekPos(const int nSamplePos);

	//! Retrieve the current position in the file (in samples)
	virtual int GetPos();

	//! Decode and retrieve pcm-data (stereo/16bit).
	virtual bool GetPCMData(signed long* pDataOut, int nSamples, bool bLoop = true);
};
