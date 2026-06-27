#include <algorithm>

#include "CryCommon/CryCore/CrySizer.h"
#include "CryCommon/CrySystem/gEnv.h"
#include "CryCommon/CrySystem/ICryPak.h"
#include "CryCommon/CrySoundSystem/IMusicSystem.h"

#include "MusicPattern.h"
#include "PatternDecoder.h"
#include "PCMDecoder.h"
#include "OGGDecoder.h"
#include "ADPCMDecoder.h"

CMusicPattern::CMusicPattern(IMusicSystem* pMusicSystem, const char* pszName, const char* pszFilename)
{
	m_pMusicSystem = pMusicSystem;
	m_pDecoder = NULL;
	m_vecFadePoints.clear();
	m_fLayeringVolume = 1.0f;
	m_nPreFadeIn = 0;
	m_sName = pszName;
	m_sFilename = pszFilename;
	m_numPatternInstances = 0;
	m_nSamples = 0;
}

CMusicPattern::~CMusicPattern()
{
	Close();
}

static bool FindFile(ICryPak* pPak, const char* pszFilename)
{
	bool ret = false;
	FILE* pFile(pPak->FOpen(pszFilename, "rb"));
	if (0 != pFile)
	{
		ret = true;
		pPak->FClose(pFile);
	}
	return ret;
}

bool CMusicPattern::Open(const char* pszFilename)
{
	// no valid filename
	if (0 == pszFilename || strlen(pszFilename) < 5)
	{
		return false;
	}

	if (0 != m_pDecoder)
	{
		Close();
	}

	ICryPak* pPak = gEnv->pCryPak;
	assert(0 != pPak);

	string strFilename(pszFilename);
	if (0 != _stricmp(strFilename.c_str() + strFilename.size() - 4, ".ogg"))
	{
		strFilename = strFilename.substr(0, strFilename.size() - 4) + ".ogg";
	}

	bool bWaveFallback(false != m_pMusicSystem->StreamOGG() && false == FindFile(pPak, strFilename.c_str()));
	if (false == m_pMusicSystem->StreamOGG() || false != bWaveFallback)
	{
		if (false != bWaveFallback)
		{
			m_pMusicSystem->LogMsg(1, "[Error] <Music> Pattern: OGG file %s not found... trying .snd",
			                       strFilename.c_str());
		}

		// use (AD)PCM files for music streaming
		strFilename = strFilename.substr(0, strFilename.size() - 4) + ".snd"; // was .wav

		// x for opening with direct streaming.
		FILE* pFile(pPak->FOpen(strFilename.c_str(), "rbx"));

		if (0 != pFile)
		{
			// check encoding and spawn PCM or ADPCM decoder accordingly
			SWaveHdr header;
			pPak->FRead(&header, 1, pFile);

			if (header.dwSize < (90 - 8 + 12))
			{
				// not enough data, not even one sample
				return false;
			}

			if (WAVE_FORMAT_ID_ADPCM == header.wOne_0)
			{
				m_pDecoder = new CADPCMDecoder(m_pMusicSystem);
				m_pDecoder->Open(
				    strFilename.c_str()); // returns always true, just use here to really open it
				// since we actually do not know in advance how many samples are contained, we need to
				// create a temp instance
				IMusicPatternDecoderInstance* pTempInstance(m_pDecoder->CreateInstance());
				pTempInstance->Release();
				m_pMusicSystem->LogMsg(2, "<Music> Pattern: Initialized ADPCM Decoder for %s",
				                       strFilename.c_str());
			}
			else if (WAVE_FORMAT_ID_PCM == header.wOne_0)
			{
				m_pDecoder = new CPCMDecoder(m_pMusicSystem);
				m_pMusicSystem->LogMsg(2, "<Music> Pattern: Initialized PCM Decoder for %s",
				                       strFilename.c_str());
			}
			else
			{
				m_pMusicSystem->LogMsg(
				    1, "[Warning] <Music> Pattern: AD(PCM) expected... failed to initialize %s",
				    strFilename.c_str());
				return false;
			}
			pPak->FClose(pFile);
		}
		else
		{
			return false;
		}
	}
	else
	{
		// use OGG files for music streaming
		if (0 != _stricmp(strFilename.c_str() + strFilename.size() - 4, ".ogg"))
		{
			strFilename = strFilename.substr(0, strFilename.size() - 4) + ".ogg";
		}

		m_pDecoder = new COGGDecoder(m_pMusicSystem);
		m_pMusicSystem->LogMsg(2, "<Music> Pattern: Initialized OGG Decoder for %s", strFilename.c_str());
	}

	if (0 != m_pDecoder)
	{
		if (false == m_pDecoder->Open(strFilename.c_str()))
		{
			m_pDecoder->Release();
			m_pDecoder = NULL;
			return false;
		}
	}
	else
	{
		return false;
	}

	SMusicPatternFileInfo fileInfo;
	if (false != m_pDecoder->GetFileInfo(fileInfo))
	{
		m_nSamples = fileInfo.nSamples;
	}

	m_sFilename = strFilename;

	return true;
}

bool CMusicPattern::Close()
{
	if (!m_pDecoder)
		return false;
	m_pDecoder->Close();
	m_pDecoder->Release();
	m_pDecoder = NULL;
	return true;
}

bool CMusicPattern::AddFadePoint(const int nFadePos)
{
	/*
	SMusicPatternFileInfo FileInfo;
	if (nFadePos<=0)
	{
	        if (!GetFileInfo(FileInfo))
	                return false;
	        nFadePos = FileInfo.nSamples+nFadePos;
	}
	*/
	TMarkerVecIt It = std::find(m_vecFadePoints.begin(), m_vecFadePoints.end(), nFadePos);
	if (It != m_vecFadePoints.end()) // already in list ?
		return true;
	m_vecFadePoints.push_back(nFadePos);
	std::sort(m_vecFadePoints.begin(), m_vecFadePoints.end());
	return true;
}

void CMusicPattern::ClearFadePoints()
{
	m_vecFadePoints.clear();
}

IMusicPatternDecoderInstance* CMusicPattern::CreateDecoderInstance()
{
	if (!m_pDecoder)
		return NULL;
	return m_pDecoder->CreateInstance();
}

CMusicPatternInstance* CMusicPattern::CreateInstance()
{
	if (!m_pDecoder)
	{
		// Make new decoder.
		if (!Open(m_sFilename.c_str()))
		{
			m_pMusicSystem->LogMsg(0, "[Error] <Music> Pattern: Music file %s failed to load",
			                       m_sFilename.c_str());
			return 0;
		}
	}
	if (!m_pDecoder)
		return NULL;
	m_numPatternInstances++;
	return new CMusicPatternInstance(this);
}

void CMusicPattern::ReleaseInstance(CMusicPatternInstance* pInstance)
{
	// TODO check this one
	m_numPatternInstances--;
	if (m_numPatternInstances <= 0)
	{
		// Can release decoder.
		Close();
	}
}

bool CMusicPattern::GetFileInfo(SMusicPatternFileInfo& FileInfo)
{
	if (!m_pDecoder)
	{
		// Make new decoder.
		if (!Open(m_sFilename.c_str()))
		{
			m_pMusicSystem->LogMsg(1, "[Error] <Music> Pattern: Music file %s failed to load",
			                       m_sFilename.c_str());
			return false;
		}
		bool bRes = false;
		if (m_pDecoder)
			bRes = m_pDecoder->GetFileInfo(FileInfo);
		Close();
		return bRes;
	}
	else
		return m_pDecoder->GetFileInfo(FileInfo);
}

void CMusicPattern::GetMemoryUsage(class ICrySizer* pSizer)
{
	if (!pSizer->Add(*this))
		return;
	if (m_pDecoder)
		m_pDecoder->GetMemoryUsage(pSizer);
}
