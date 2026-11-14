#include "CryCommon/CryCore/CrySizer.h"
#include "CryCommon/CrySoundSystem/IMusicSystem.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/ICryPak.h"

#include "ADPCMDecoder.h"

template<typename T, typename S>
static void lsbshortldi(T& x, S& p)
{
	x = (T)((int)p[0] + ((int)p[1] << 8));
	p += 2;
}

static const int stepAdjustTable[] = {230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230};

CADPCMDecoder::CADPCMDecoder(IMusicSystem* pMusicSystem) : m_pMusicSystem(pMusicSystem), m_bOpen(false)
{
}

CADPCMDecoder::~CADPCMDecoder()
{
	Close();
}

const bool CADPCMDecoder::IsOpen() const
{
	return m_bOpen;
}

FILE* CADPCMDecoder::GetNewFileHandle()
{
	// x for opening with direct streaming.
	FILE* pNewFile = gEnv->pCryPak->FOpen(m_FileInfo.sFilename.c_str(), "rbx");
	if (!pNewFile)
		return NULL;
	return pNewFile;
}

void CADPCMDecoder::CloseFileHandle(FILE*& pFile)
{
	if (pFile)
	{
		gEnv->pCryPak->FClose(pFile);
		pFile = NULL;
	}
}

void CADPCMDecoder::SetFileInfo(const unsigned int cuiSampleCount, const bool cbIs44KHz)
{
	m_FileInfo.nSamples = cuiSampleCount; // already doubled up
	m_b44KHz = cbIs44KHz;
	m_bOpen = true;
}

bool CADPCMDecoder::Open(const char* pszFilename)
{
	m_FileInfo.sFilename = pszFilename;
	m_FileInfo.nHeaderSize = sizeof(SWaveHdr);
	return m_bOpen;
}

bool CADPCMDecoder::Close()
{
	m_bOpen = false;
	return true;
}

const char* CADPCMDecoder::FileName() const
{
	return m_FileInfo.sFilename.c_str();
}

bool CADPCMDecoder::GetFileInfo(SMusicPatternFileInfo& FileInfo)
{
	FileInfo = m_FileInfo;
	return true;
}

void CADPCMDecoder::GetMemoryUsage(class ICrySizer* pSizer)
{
	if (!pSizer->Add(*this))
		return;
}

int CADPCMDecoder::AdpcmDecode(int c, MsState& rState, int sample1, int sample2)
{
	// Compute next step value
	int step(rState.step);

	int nstep((stepAdjustTable[c] * step) >> 8);
	rState.step = (nstep < 16) ? 16 : nstep;

	// make linear prediction for next sample
	int vlin(((sample1 * rState.sCoef0) + (sample2 * rState.sCoef1)) >> 8);

	// then add the code * step adjustment
	c -= (c & 0x08) << 1;
	int sample((c * step) + vlin);

	if (sample > 0x7fff)
		sample = 0x7fff;
	else if (sample < -0x8000)
		sample = -0x8000;

	return (sample);
}

void CADPCMDecoder::AdpcmBlockExpandI(int nCoef, const short* iCoef, const unsigned char* ibuff, short* obuff, int n)
{
	MsState state[2]; // One decompressor state for each channel

	// Read the four-byte header for each channel
	const unsigned char* ip(ibuff);
	unsigned char bpred(*ip++);
	state[0].sCoef0 = iCoef[(int)bpred * 2 + 0];
	state[0].sCoef1 = iCoef[(int)bpred * 2 + 1];
	bpred = (*ip++);
	state[1].sCoef0 = iCoef[(int)bpred * 2 + 0];
	state[1].sCoef1 = iCoef[(int)bpred * 2 + 1];

	lsbshortldi(state[0].step, ip);
	lsbshortldi(state[1].step, ip);
	// sample1's directly into obuff

	lsbshortldi(obuff[2], ip);
	lsbshortldi(obuff[2 + 1], ip);

	// sample2's directly into obuff
	lsbshortldi(obuff[0], ip);
	lsbshortldi(obuff[1], ip);

	// already have 1st 2 samples from block-header
	short* op(obuff + 2 * 2);
	short* top(obuff + n * 2);
	{
		while (op < top)
		{
			unsigned char b(*ip++);
			short* tmp(op);
			*op++ = AdpcmDecode(b >> 4, state[0], tmp[-2], tmp[-2 * 2]);
			tmp = op;
			*op++ = AdpcmDecode(b & 0x0f, state[1], tmp[-2], tmp[-2 * 2]);
		}
	}
}

IMusicPatternDecoderInstance* CADPCMDecoder::CreateInstance()
{
	return new CADPCMDecoderInstance(this);
}

//////////////////////////////////////////////////////////////////////////
/*** INSTANCE ***/
//////////////////////////////////////////////////////////////////////////

CADPCMDecoderInstance::CADPCMDecoderInstance(CADPCMDecoder* pDecoder)
    : m_iFilePos(0), m_pFile(NULL), m_nPos(0), m_uiDataStartPos(0), m_uiNumSamples(0), m_sCoefs(7), m_psSamplePtr(NULL),
      m_bInitialized(false), m_bCopyFromLastFrame(false), m_uiCurrentBlockSize(scuiBlockSize),
      m_uiCurrentSamplesPerBlock(scuiSamplesPerBlock)
{
	m_pDecoder = pDecoder;
	// init wav stream
	if (!InitStreamWAV())
	{
		m_bInitialized = false;
	}
	else
	{
		m_bInitialized = true;
	}
}

CADPCMDecoderInstance::~CADPCMDecoderInstance()
{
	Close(); // let the filehandle be released
}

void CADPCMDecoderInstance::Close()
{
	if (m_pFile)
	{
		m_pDecoder->CloseFileHandle(m_pFile);
	}
	m_bInitialized = false;
}

//////////////////////////////////////////////////////////////////////////
bool CADPCMDecoderInstance::Seek0(const int nDelay)
{
	FRAME_PROFILER("Decoder", GetISystem(), PROFILE_MUSIC);

	m_nPos = -nDelay; // new sample pos
	m_bCopyFromLastFrame = false;
	if (!m_pFile || m_uiDataStartPos == 0)
		return false;
	gEnv->pCryPak->FSeek(m_pFile, m_uiDataStartPos, SEEK_SET);
	m_iFilePos = 0;
	return true;
}

//! Seek to pos in number of samples of pattern.
bool CADPCMDecoderInstance::SeekPos(const int nSamplePos)
{
	// todo implement a fast seek
	return false;
}

const unsigned int CADPCMDecoderInstance::Samples()
{
	return m_uiNumSamples;
}

//////////////////////////////////////////////////////////////////////////
int CADPCMDecoderInstance::GetPos()
{
	if (!m_pFile)
		return -1;
	return m_nPos;
}

const bool CADPCMDecoderInstance::InitStreamWAV()
{
	memset(m_aEncodedBlock, 0, scuiBlockSize);
	SADPCMWaveHdr adpcmHeader; // adpcm secific header
	// get file handle
	if (!m_pFile)
	{
		m_pFile = m_pDecoder->GetNewFileHandle();
		if (!m_pFile)
			return false;
	}
	ICryPak* pPak = gEnv->pCryPak;
	// retrieve total size of file
	pPak->FRead(&adpcmHeader, 1, m_pFile);
	pPak->FRead(&m_sCoefs, 1, m_pFile);
	if (adpcmHeader.FormatTag == WAVE_FORMAT_ID_PCM)
	{
		m_pDecoder->m_pMusicSystem->LogMsg(2, "<Music> Decoder: Uncompressed pcm data, use pcm decoder %s\n",
		                                   m_pDecoder->FileName());
		// TRACE("uncompressed pcm data, use pcm decoder %s\n", m_pDecoder->FileName());
		return false;
	}
	if (adpcmHeader.FormatTag != WAVE_FORMAT_ID_ADPCM || adpcmHeader.wChnls != 2)
	{
		// #ifdef LOG_MUSICFILES
		m_pDecoder->m_pMusicSystem->LogMsg(2, "[Warning] <Music> Decoder: No stereo adpcm file:  %s\n",
		                                   m_pDecoder->FileName());
		// #else
		// TRACE("no stereo adpcm file:  %s\n", m_pDecoder->FileName());
		// #endif
		return false;
	}
	// set block alignments
	m_uiCurrentBlockSize = adpcmHeader.wBlkAlign;
	m_uiCurrentSamplesPerBlock = adpcmHeader.SamplesPerBlock;
	if (m_sCoefs != scuiNumberOfCoefs || m_uiCurrentSamplesPerBlock > scuiSamplesPerBlock ||
	    m_uiCurrentBlockSize > scuiBlockSize)
	{
		m_pDecoder->m_pMusicSystem->LogMsg(2, "[Warning] <Music> Decoder: Unsupported adpcm format %s\n",
		                                   m_pDecoder->FileName());
		//		TRACE("unsupported adpcm format %s\n", m_pDecoder->FileName());
		return false;
	}
	pPak->FReadRaw(m_aCoefs, 1, m_sCoefs /*7*/ * 2 /*stereo->2 channels*/ * sizeof(short), m_pFile);
	// now seek to start position (next to "data")
	unsigned int uiDataLeft = 0;
	for (;;)
	{
		char pcMagic[4];
		pPak->FRead(pcMagic, 4, m_pFile);

		if (0 != pPak->FEof(m_pFile))
			return false;
		pPak->FRead(&uiDataLeft, 1, m_pFile);
		if (0 == strncmp("data", pcMagic, 4))
			break;
		pPak->FSeek(m_pFile, uiDataLeft, SEEK_CUR);
	}
	m_uiDataStartPos = pPak->FTell(m_pFile); // to make seek0 work properly, data start position
	// compute number of samples contained
	int m = (uiDataLeft % m_uiCurrentBlockSize);
	m_uiNumSamples =
	    (uiDataLeft / m_uiCurrentBlockSize) * m_uiCurrentSamplesPerBlock; // usually block aligned and not necessary
	m -= scuiNumberOfCoefs * 2;                                           // bytes beyond block-header
	m += 2;                                                               // nibbles / channels + 2 in header
	if (m > 0) // there are some samples contained in an unaligned block
	{
		if (m > (int)m_uiCurrentSamplesPerBlock)
		{
			m = (int)m_uiCurrentSamplesPerBlock;
		}
		m_uiNumSamples += m;
	}
	Seek0();
	// since the total sample count is not known by the decoder, tell him
	const bool cbIs44KHz = (adpcmHeader.dwSRate == 44100);
	if (!cbIs44KHz)
		m_uiNumSamples *= 2;
	m_pDecoder->SetFileInfo(m_uiNumSamples, cbIs44KHz);
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CADPCMDecoderInstance::GetPCMData(signed long* pDataOut, int nSamples, bool bLoop)
{
	// prevent any asynchonisation in terms of file opening
	if (!m_pFile || !m_bInitialized)
	{
		// try it again
		if (m_pDecoder->IsOpen())
		{
			if (!InitStreamWAV())
				return false;
		}
		else
			return false;
	}
	int nOfs = 0;
	if (m_nPos < 0)
	{
		if (-m_nPos >= nSamples)
		{
			memset(pDataOut, 0, nSamples * m_pDecoder->m_pMusicSystem->GetBytesPerSample());
			m_nPos += nSamples;
			return true;
		}
		else
		{
			nOfs = -m_nPos;
			m_nPos = 0;
			Seek0(); // set back to start
			nSamples -= nOfs;
			memset(pDataOut, 0, nOfs * m_pDecoder->m_pMusicSystem->GetBytesPerSample());
		}
	}
	int nSamplesToRead;
	for (;;)
	{
		if ((m_nPos + nSamples) > (int)m_uiNumSamples)
		{
			nSamplesToRead = m_uiNumSamples - m_nPos;
		}
		else
			nSamplesToRead = nSamples;
		if (m_pDecoder->Is44KHz())
		{
			if (!FillPCMBuffer(&(pDataOut[nOfs]), nSamplesToRead))
				return false;
		}
		else
		{
			if (!FillPCMBuffer22KHz(&(pDataOut[nOfs]), nSamplesToRead))
				return false;
		}
		m_nPos += nSamplesToRead;
		if (nSamplesToRead == nSamples)
			break;
		nOfs += nSamplesToRead;
		if (!bLoop)
		{
			memset(&(pDataOut[nOfs]), 0,
			       (nSamples - nSamplesToRead) * m_pDecoder->m_pMusicSystem->GetBytesPerSample());
			break;
		}
		nSamples -= nSamplesToRead;
		Seek0();
	}
	return true;
}

// AdpcmReadBlock - Grab and decode complete block of samples
unsigned short CADPCMDecoderInstance::AdpcmReadBlock(short* pDest)
{
	// read block
	unsigned short bytesRead((unsigned short)gEnv->pCryPak->FRead(m_aEncodedBlock, m_uiCurrentBlockSize, m_pFile));
	m_iFilePos += bytesRead; // internal file pos counter, counted from m_uiDataStartPos on
	unsigned int samplesThisBlock = m_uiCurrentSamplesPerBlock;
	if (bytesRead < m_uiCurrentBlockSize)
	{
		// this shouldnt be the case, but handle it just in case (most programs do block align)
		samplesThisBlock = 0;
		unsigned int m = bytesRead;
		// more data found than just coefs
		if (m >= (unsigned int)(scuiNumberOfCoefs * 2))
		{
			samplesThisBlock = m - scuiNumberOfCoefs * 2 + 2; // bytes beyond block-header
		}
		else
			return (0);
	}
	// now decode read stuff
	if (pDest == NULL)
		CADPCMDecoder::AdpcmBlockExpandI(m_sCoefs, m_aCoefs, m_aEncodedBlock, m_aDecodedBlock,
		                                 samplesThisBlock);
	else
		CADPCMDecoder::AdpcmBlockExpandI(m_sCoefs, m_aCoefs, m_aEncodedBlock, pDest, samplesThisBlock);
	return (samplesThisBlock);
}

const bool CADPCMDecoderInstance::FillPCMBuffer(signed long* pBuffer, int nSamples)
{
	// since there get always complete blocks read, fill buffer with already compressed samples
	const unsigned int cuiModulo = m_nPos % m_uiCurrentSamplesPerBlock;
	if (cuiModulo != 0 && m_iFilePos > 0) // to not let break anything
	{                                     // we are not on a block boundary
		unsigned int uiLeft = min((unsigned int)nSamples, (m_uiCurrentSamplesPerBlock - cuiModulo));
		if (uiLeft != 0) // read as many sampels as left decompressed and not more than requested
		{
			// copy decompressed data
			signed long* pslDecompressed = reinterpret_cast<signed long*>(m_aDecodedBlock) + cuiModulo;
			for (unsigned int i = 0; i < uiLeft; i++)
			{
				*pBuffer++ = *pslDecompressed++;
			}
			// set new number of remaining samples
			nSamples -= uiLeft;
		}
	}
	short* pCurrentDest = reinterpret_cast<short*>(
	    pBuffer); // need a short buffer to decode each channel indivually (remember: 16 bit)
	if (nSamples > 0)
	{
		while (nSamples > 0)
		{
			// read as many blocks as necessary
			// write straight to output buffer if complete block has to read in
			if (nSamples >= (int)m_uiCurrentSamplesPerBlock)
			{
				bool bEndOfFile = (AdpcmReadBlock(pCurrentDest) != m_uiCurrentSamplesPerBlock);
				if (bEndOfFile) // shouldnt happen
					return false;
				nSamples -= m_uiCurrentSamplesPerBlock;
				pCurrentDest += m_uiCurrentSamplesPerBlock * 2; // two channels decoded
			}
			else
			{
				// decompress to static buffer
				unsigned int uiReadSamples = AdpcmReadBlock();
				if (uiReadSamples != m_uiCurrentSamplesPerBlock)
				{
					// we are supposed to be on the end of the file, fill with 0's
					memset(m_aDecodedBlock + uiReadSamples * 2 /*stereo*/, 0,
					       m_uiCurrentSamplesPerBlock - uiReadSamples);
				}
				// read requested samples
				pBuffer = reinterpret_cast<signed long*>(pCurrentDest);
				signed long* pDecoded = reinterpret_cast<signed long*>(m_aDecodedBlock);
				// if for bad reason not enough samples have been read, it will get automatically filled
				// with 0's
				for (int i = 0; i < nSamples; i++)
				{
					*pBuffer++ = *pDecoded++;
				}
				nSamples = 0;
			}
		}
	}
	return true;
}

const bool CADPCMDecoderInstance::FillPCMBuffer22KHz(signed long* pBuffer, int nSamples)
{
	unsigned uStartPos = (m_nPos / 2 /*22KHz*/);
	// first check for copying some data from last frame (if not at starting position)
	if (m_bCopyFromLastFrame && m_iFilePos > 0) /*22KHz*/
	{
		*pBuffer++ = m_lLastSample; // contains sample to start with from last encoding
		nSamples -= 1;
		m_bCopyFromLastFrame = false;
		uStartPos++;
	}
	// since there get always complete blocks read, fill buffer with already compressed samples
	const unsigned int cuiModulo = uStartPos % m_uiCurrentSamplesPerBlock;
	unsigned int uiLeft = 0;
	if (cuiModulo != 0 && m_iFilePos > 0) // to not let break anything
	{                                     // we are not on a block boundary
		uiLeft = min((unsigned int)nSamples / 2, (m_uiCurrentSamplesPerBlock - cuiModulo)); /*22KHz*/
		signed long* pslDecompressed = reinterpret_cast<signed long*>(m_aDecodedBlock) + cuiModulo;
		if (uiLeft != 0) // read as many sampels as left decompressed and not more than requested
		{
			// copy decompressed data
			for (unsigned int i = 0; i < uiLeft; i++)
			{
				*pBuffer++ = *pslDecompressed; /*22KHz*/
				*pBuffer++ = *pslDecompressed++;
			}
			// set new number of remaining samples
			nSamples -= 2 * uiLeft;
		}
		if (nSamples == 1) /*22KHz*/
		{
			// there is one sample left
			m_bCopyFromLastFrame = true;
			*pBuffer++ = m_lLastSample =
			    *pslDecompressed; // remember last sample, has to start with in next request
			nSamples = 0;
		}
	}
	short* pCurrentDest = reinterpret_cast<short*>(
	    pBuffer); // need a short buffer to decode each channel indivually (remember: 16 bit)
	if (nSamples > 0)
	{
		while (nSamples > 0)
		{
			// read as many blocks as necessary
			// write straight to output buffer if complete block has to read in
			if (nSamples >= (int)m_uiCurrentSamplesPerBlock * 2)
			{
				bool bEndOfFile = (AdpcmReadBlock() != m_uiCurrentSamplesPerBlock);
				if (bEndOfFile) // shouldnt happen
				{
					memset(pBuffer, 0, nSamples * 4); // fill with 0's
					return false;
				}
				pBuffer = reinterpret_cast<signed long*>(pCurrentDest);
				signed long* pDecoded = reinterpret_cast<signed long*>(m_aDecodedBlock);
				// if for bad reason not enough samples have been read, it will get automatically filled
				// with 0's
				for (unsigned int i = 0; i < m_uiCurrentSamplesPerBlock; i++)
				{
					*pBuffer++ = *pDecoded;
					*pBuffer++ = *pDecoded++;
				}
				nSamples -= 2 * m_uiCurrentSamplesPerBlock; /*22KHz*/
				pCurrentDest = reinterpret_cast<short*>(pBuffer);
			}
			else
			{
				// decompress to static buffer
				unsigned int uiReadSamples = AdpcmReadBlock();
				if (uiReadSamples != m_uiCurrentSamplesPerBlock)
				{
					// we are supposed to be on the end of the file, fill with 0's
					memset(m_aDecodedBlock + uiReadSamples * 2 /*stereo*/, 0,
					       m_uiCurrentSamplesPerBlock - uiReadSamples);
				}
				// read requested samples
				pBuffer = reinterpret_cast<signed long*>(pCurrentDest);
				signed long* pDecoded = reinterpret_cast<signed long*>(m_aDecodedBlock);
				// if for some bad reason not enough samples have been read, it will get automatically
				// filled with 0's
				for (int i = 0; i < nSamples / 2; i++)
				{
					*pBuffer++ = *pDecoded;
					*pBuffer++ = *pDecoded++;
				}
				m_bCopyFromLastFrame = false; // just to make sure
				if (nSamples & 0x1)
				{
					m_bCopyFromLastFrame = true;
					// read one sample more
					*pBuffer++ = m_lLastSample =
					    *pDecoded; // remember last sample, has to start with in next request
				}
				nSamples = 0;
			}
		}
	}
	return true;
}
