#include "CryCommon/CryCore/CrySizer.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/CrySystem/ITimer.h"

#include "IAudioDevice.h"
#include "SoundAssetManager.h"
#include "SoundSystem.h" // CSoundSystem should be ISoundSystem
#include "Sound.h"

CSoundAssetManager::CSoundAssetManager(CSoundSystem* pSoundSystem)
{
	assert(pSoundSystem != NULL);

	m_pSoundSystem = pSoundSystem;
	m_bUseMemoryBlock = false; // toggles if internal Memory Block should be used or not
	m_MemoryBlockStart = NULL;
	m_nMemoryBlockSize = 0;
	m_nUsedMemorySize = 0;
	m_nTotalSoundMemory = 0;
}

CSoundAssetManager::~CSoundAssetManager(void)
{
	ShutDown();
}

//////////////////////////////////////////////////////////////////////////
// Management
//////////////////////////////////////////////////////////////////////////

// gives a big memory block in Bytes to the Manager
bool CSoundAssetManager::SetMemoryBlock(void* pBlockStart, unsigned int nBlockSize)
{
	assert(pBlockStart);

	// delete the old Memory Block
	if (m_bUseMemoryBlock)
	{
		delete[] (char*)m_MemoryBlockStart;
		m_nMemoryBlockSize = 0;
		m_bUseMemoryBlock = false;
	}

	// set the new one
	if (pBlockStart && nBlockSize)
	{
		m_MemoryBlockStart = pBlockStart;
		m_nMemoryBlockSize = nBlockSize;
		m_bUseMemoryBlock = true;
	}

	return (m_bUseMemoryBlock);
}

// Let the Manager allocate a big memory block in Bytes to deal with
bool CSoundAssetManager::AllocMemoryBlock(unsigned int nBlockSize)
{
	// delete the old Memory Block
	if (m_bUseMemoryBlock)
	{
		delete[] (char*)m_MemoryBlockStart;
		m_nMemoryBlockSize = 0;
		m_bUseMemoryBlock = false;
	}

	// create the new one
	if (nBlockSize)
	{
		m_MemoryBlockStart = new char[nBlockSize];
		m_nMemoryBlockSize = nBlockSize;
		m_bUseMemoryBlock = true;
	}

	return (m_bUseMemoryBlock);
}

// returns a Ptr to a free chunk of memory to be used
void* CSoundAssetManager::GetMemoryChunk(SSoundBufferProps BufferProps, unsigned int nChunkSize)
{
	// only alloc memory for a SoundBuffer that already exist
	SoundBufferPropsMapItorConst It = m_soundBuffers.find(BufferProps);
	if (It == m_soundBuffers.end())
		return (NULL);

	CSoundBuffer* pBuf = It->second;

	// remove all old data
	if (pBuf->m_BufferData)
	{
		delete[] (char*)pBuf->m_BufferData;
		pBuf->m_BufferData = NULL;
	}

	void* pMemChunk = new char[nChunkSize];

	// out of memory
	if (!pMemChunk)
		return NULL;

	m_nUsedMemorySize += nChunkSize;
	memset(pMemChunk, 0, nChunkSize);

	return (pMemChunk);
}

// frees a chunk of memory that was used in a SoundBuffer
bool CSoundAssetManager::FreeMemoryChunk(SSoundBufferProps BufferProps)
{
	// only free memory of a SoundBuffer that already exist
	SoundBufferPropsMapItorConst It = m_soundBuffers.find(BufferProps);
	if (It == m_soundBuffers.end())
		return false;

	CSoundBuffer* pBuf = It->second;
	uint32 nMemorySize = pBuf->GetInfo()->nLengthInBytes;

	// remove all old data
	if (pBuf->m_BufferData)
	{
		delete[] (char*)pBuf->m_BufferData;
		pBuf->m_BufferData = NULL;
		m_nUsedMemorySize -= nMemorySize;
	}

	return true;
}

// gives a big memory block to the Manager
// bool CSoundAssetManager::FreeMemoryBlock()
//{
// return false;
//}

// frees all SoundBuffers, deletes all data;
bool CSoundAssetManager::ShutDown()
{
	// SoundBufferPropsMapItor It = m_soundBuffers.begin();
	// while (It!=m_soundBuffers.end())
	//{
	//	CSoundBuffer *pBuf = It->second;
	//	//if (pBuf->Loaded())
	//	if (pBuf)
	//	{
	//		RemoveSoundBuffer(pBuf->GetProps());
	//	}
	//	else
	//		++It;
	// } //it

	m_lockedResources.resize(0);
	m_soundBuffers.clear();
	m_nUsedMemorySize = 0;

	// delete the old Memory Block
	if (m_bUseMemoryBlock)
	{
		delete[] (char*)m_MemoryBlockStart;
		m_nMemoryBlockSize = 0;
		m_bUseMemoryBlock = false;
	}

	return true;
}

// Returns current Memory Usage
unsigned int CSoundAssetManager::GetMemUsage()
{
	uint32 nSize = 0;
	SoundBufferPropsMapItorConst ItEnd = m_soundBuffers.end();
	for (SoundBufferPropsMapItorConst It = m_soundBuffers.begin(); It != ItEnd; ++It)
	{
		CSoundBuffer* pBuf = It->second;
		nSize += pBuf->GetMemoryUsed();
	} // it

	return nSize;
}

// compute memory-consumption, returns rough estimate in MB
int CSoundAssetManager::GetMemoryUsage(class ICrySizer* pSizer)
{

	if (pSizer)
	{
		if (!pSizer->Add(*this))
			return 0;
	}

	uint32 nSize = 0;
	SoundBufferPropsMapItorConst ItEnd = m_soundBuffers.end();
	for (SoundBufferPropsMapItorConst It = m_soundBuffers.begin(); It != ItEnd; ++It)
	{
		CSoundBuffer* pBuf = It->second;
		nSize += pBuf->GetMemoryUsage(pSizer);
	} // it

	return nSize;
}

unsigned int CSoundAssetManager::GetNumberBuffersLoaded()
{
	return static_cast<unsigned int>(m_soundBuffers.size());
}

// Cleans up dead bufffers, defrags or kicks out low priority Buffers
bool CSoundAssetManager::Update()
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SOUND);

	if (!m_pSoundSystem->IsEnabled())
		return true;

	// Lets see how much memory is used and if the cache is too full, remove some buffers.
	m_nTotalSoundMemory = (m_pSoundSystem->GetIAudioDevice()->GetMemoryStats() / 1024) / 1024;
	float nThreshold = m_pSoundSystem->g_nCacheSize * 0.9f;

	bool bInBudget = (m_nTotalSoundMemory < nThreshold);
	bool bRemoved = false;

	std::vector<CSoundBuffer*> ToRemove;

	// while (It != m_soundBuffers.end())
	SoundBufferPropsMapItorConst ItEnd = m_soundBuffers.end();
	for (SoundBufferPropsMapItorConst It = m_soundBuffers.begin(); It != ItEnd; ++It)
	{
		bRemoved = false;
		CSoundBuffer* pBuf = It->second;

		if (pBuf)
		{
			// pBuf is valid
			if (pBuf->LoadFailure())
				bRemoved = true; // false;//true; // pBuf is invalid so get rid of it

			float fBufTime = pBuf->GetTimer().GetSeconds();
			float fNowTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
			float fDiff = fNowTime - fBufTime;

			if (fDiff > 20.0f)
			{
				++pBuf->GetInfo()->nTimedOut;

				float fNeeded = AssetStillNeeded(pBuf); // Calls the evaluation Algorithm
				pBuf->UpdateTimer(gEnv->pTimer->GetFrameStartTime());

				if (!fNeeded)
				{
					if (!pBuf->Loaded() || bRemoved)
					{
						ToRemove.push_back(pBuf);
					}
					else
					{
						if (pBuf->GetProps()->nPrecacheFlags & FLAG_SOUND_PRECACHE_UNLOAD_NOW &&
						    m_pSoundSystem->g_nUnloadData != 0)
						{
							if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_DETAIL)
								gEnv->pLog->Log("<Sound> unloading (now) data on: %s",
								                pBuf->GetName());

							bool bResult =
							    pBuf->UnloadData(sbUNLOAD_ALL_DATA); // unload completely
						}

						// if needed, maybe we can unload some data partially
						// disable until FMOD bug is fixed
						if (!bInBudget &&
						    !(pBuf->GetProps()->nPrecacheFlags &
						      FLAG_SOUND_PRECACHE_STAY_IN_MEMORY) &&
						    m_pSoundSystem->g_nUnloadData != 0)
						{
							if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_DETAIL)
								gEnv->pLog->Log("<Sound> unloading data on: %s",
								                pBuf->GetName());

							bool bResult = pBuf->UnloadData(
							    sbUNLOAD_UNNEEDED_DATA); // try to free some memory

							if (bResult)
								It = ItEnd; // lets only free one group per frame
						}
					}
				}
			}

			// int nNumReferencedSounds = 0;
			// ptParamINT32 TempParam(nNumReferencedSounds);
			// pBuf->GetParam(apNUMREFERENCEDSOUNDS, &TempParam);
			// TempParam.GetValue(nNumReferencedSounds);

			// NICK warning precache routine loading buffers that are not associated with any sounds
			// DO NOT DESTROY BUFFERS CREATED BY precache system
			// if (nNumReferencedSounds < 0) // TODO move up !?
			//				bRemoveIt = true;
		}

		// if (!bRemoved)  // TODO Activate this again, when Scripts dont hold buffers anymore
		//++It;
	}

	std::vector<CSoundBuffer*>::iterator IterEnd = ToRemove.end();
	for (std::vector<CSoundBuffer*>::iterator Iter = ToRemove.begin(); Iter != IterEnd; ++Iter)
	{
		CSoundBuffer* pBuf = (*Iter);

		// unload this SoundBuffer
		pBuf->UnloadData(sbUNLOAD_ALL_DATA);

		bRemoved = RemoveSoundBuffer(*(pBuf->GetProps()));
		// m_pSoundSystem->GetSystem()->GetILog()->LogToFile("Removing sound [%s]", It->first.sName.c_str());
	}

	return false;
}

// writes output to screen in debug
void CSoundAssetManager::DrawInformation(IRenderer* pRenderer, float xpos, float ypos)
{
	uint32 nUsedMem = m_nUsedMemorySize;
	int nBufferCount = 0;

	if (!(m_soundBuffers.empty()) && m_nUsedMemorySize == 0)
	{
		SoundBufferPropsMapItorConst ItEnd = m_soundBuffers.end();
		for (SoundBufferPropsMapItorConst It = m_soundBuffers.begin(); It != ItEnd; ++It)
		{
			++nBufferCount;
			CSoundBuffer* pBuf = It->second;

			if (pBuf->m_BufferData != NULL && pBuf->GetProps()->eBufferType != btSTREAM)
				nUsedMem += pBuf->GetMemoryUsed();
			// nUsedMem += pBuf->GetInfo()->nLengthInBytes;
		}
	}

	float fColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	float fWhite[4] = {1.0f, 1.0f, 1.0f, 0.7f};
	float fBlue[4] = {0.0f, 0.0f, 1.0f, 0.7f};
	float fCyan[4] = {0.0f, 1.0f, 1.0f, 0.7f};
	float fRed[4] = {1.0f, 0.0f, 0.0f, 1.0f};

	TFixedResourceName sMemUnit("(KB)");
	// string sMemUnit = "(KB)";
	float fUsedMem = nUsedMem / 1024;
	if (fUsedMem > 1024)
	{
		sMemUnit = "(MB)";
		fUsedMem /= 1024;
	}

	pRenderer->Draw2dLabel(xpos, ypos, 1, fColor, false, "Total Buffers: %d", m_soundBuffers.size());
	ypos += 10;

	pRenderer->Draw2dLabel(xpos, ypos, 1, fColor, false, "Used Memory: %f %s", fUsedMem, sMemUnit.c_str());
	ypos += 10;

	SoundBufferPropsMapItorConst ItEnd = m_soundBuffers.end();
	for (SoundBufferPropsMapItorConst It = m_soundBuffers.begin(); It != ItEnd; ++It)
	{
		CSoundBuffer* pBuf = It->second;
		memcpy(fColor, fWhite, 4 * sizeof(float));

		if (pBuf->GetProps()->nPrecacheFlags & FLAG_SOUND_PRECACHE_STAY_IN_MEMORY)
			memcpy(fColor, fCyan, 4 * sizeof(float));

		// if (pBuf->GetSample())
		// if (pBuf->GetAssetHandle())
		//		if (!pBuf->Loaded())
		{
			// float nThreshold = m_pSoundSystem->g_nCacheSize * 0.9f;
			// if (m_nTotalSoundMemory < nThreshold)
			//{
			// memcpy(fColor,fWhite,4*sizeof(float));

			//}
			// else
			//{
			//				if (pBuf->GetProps()->nPrecacheFlags &
			// FLAG_SOUND_PRECACHE_STAY_IN_MEMORY)
			// memcpy(fColor,fCyan,4*sizeof(float));

			if (!pBuf->Loaded())
				memcpy(fColor, fRed, 4 * sizeof(float));
			else
				fColor[3] = AssetStillNeeded(pBuf) + 0.3f;
			//}

			pRenderer->Draw2dLabel(xpos, ypos, 1, fColor, false, "%s Mem: %dKB", pBuf->GetName(),
			                       pBuf->GetMemoryUsed() / 1024);
			ypos += 10;
		}
	} // it
}

//////////////////////////////////////////////////////////////////////////
// Asset Management
//////////////////////////////////////////////////////////////////////////

// // gets or creates a SoundBuffer for a referred sound on a specific platform and returns the pointer to it
// NICK moved pRefSound test for NULL further down so precache routine can call this
CSoundBuffer* CSoundAssetManager::GetOrCreateSoundBuffer(const SSoundBufferProps& BufferProps, CSound* pRefSound)
{
	CSoundBuffer* pBuf = GetSoundBufferPtr(BufferProps);

	if (!pBuf)
	{
		pBuf = m_pSoundSystem->GetIAudioDevice()->CreateSoundBuffer(BufferProps);
	}

	if (pBuf)
	{
		m_soundBuffers[BufferProps] = pBuf;
		// pBuf->GetProps()->nPrecacheFlags = BufferProps.nPrecacheFlags;
		// pBuf->GetProps()->nFlags = BufferProps.nFlags;
		// gEnv->pLog->Log("Sound-AssetManager: SoundBuffer %s %s \n", BufferProps.sName.c_str(), "created
		// successfully.");

		// moved this test to here so pSound ptr can be null when precache called
		if (pRefSound)
			pRefSound->SetSoundBufferPtr(pBuf);

		// pBuf->Load(pRefSound);
	}

	return (pBuf);
}

// removes a SoundBuffer from the AssetManager
bool CSoundAssetManager::RemoveSoundBuffer(const SSoundBufferProps& BufferProps)
{
	// TODO move this into AssetManager which has to be designed yet

	SoundBufferPropsMapItor itor = m_soundBuffers.find(BufferProps);

	if (itor != m_soundBuffers.end())
	{
		CSoundBuffer* pBuf = (itor->second);
		assert(pBuf->m_nRef == 1);

		if (pBuf->m_nRef == 1)
		{
			if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_DETAIL)
				gEnv->pLog->Log("<Sound> AssetManager: Removing sound [%s]", itor->first.sName.c_str());

			m_soundBuffers.erase(itor);
			return true;
		}
	}
	else
	{
		if (m_pSoundSystem->g_nDebugSound == SOUNDSYSTEM_DEBUG_DETAIL)
			gEnv->pLog->LogWarning("<Sound> AssetManager: Trying to remove sound [%s] NOT FOUND!!!",
			                       BufferProps.sName.c_str());
	}

	return false;
}

// bool CSoundAssetManager::RemoveSoundBuffer(SoundBufferPropsMapRevItor Iter)
//{
//	if (Iter != m_soundBuffers.rend())
//	{
//		m_pSoundSystem->GetSystem()->GetILog()->LogToFile("Removing sound [%s]", Iter->first.sName.c_str());
//		m_soundBuffers.erase(Iter);
//		return true;
//	}
//
//	return false;
// }

// locks the SoundBuffers so they don't get unloaded
void CSoundAssetManager::LockResources()
{
	m_lockedResources.resize(0);
	m_lockedResources.reserve(m_soundBuffers.size());

	SoundBufferPropsMapItorConst ItEnd = m_soundBuffers.end();
	for (SoundBufferPropsMapItorConst It = m_soundBuffers.begin(); It != ItEnd; ++It)
	{
		CSoundBuffer* pBuf = It->second;
		m_lockedResources.push_back(pBuf);
	}
}

// unlocks the SoundBuffers so they could be unload if needed
void CSoundAssetManager::UnlockResources()
{
	m_lockedResources.clear();
}

// unloads ALL SoundBuffers from the AssetManager
void CSoundAssetManager::UnloadAllSoundBuffers()
{
	// GUARD_HEAP;

	// first iterate through all buffers and unload events to prevent dialog event access invalid memory
	SoundBufferPropsMapItorConst IterEnd = m_soundBuffers.end();
	for (SoundBufferPropsMapItorConst Iter = m_soundBuffers.begin(); Iter != IterEnd; ++Iter)
	{
		// m_pSoundSystem->GetSystem()->GetILog()->LogToFile("Removing sound [%s]", itor->first.sName.c_str());
		CSoundBuffer* pBuf = (Iter->second);
		if (pBuf && m_pSoundSystem->g_nUnloadData && pBuf->GetProps()->eBufferType == btEVENT)
			pBuf->UnloadData(sbUNLOAD_ALL_DATA);
	}

	// all other buffers 2nd iteration
	for (SoundBufferPropsMapItorConst Iter = m_soundBuffers.begin(); Iter != IterEnd; ++Iter)
	{
		// m_pSoundSystem->GetSystem()->GetILog()->LogToFile("Removing sound [%s]", itor->first.sName.c_str());
		CSoundBuffer* pBuf = (Iter->second);
		if (pBuf && m_pSoundSystem->g_nUnloadData && pBuf->GetProps()->eBufferType != btEVENT)
			pBuf->UnloadData(sbUNLOAD_ALL_DATA);
	}
}

// MVD
// which one? Both? what parameter?
// Checks and returns a Ptr to a loaded Asset or NULL if its not there
CSoundBuffer* CSoundAssetManager::GetSoundBufferPtr(const SSoundBufferProps& BufferProps) const
{
	CSoundBuffer* pBuf = NULL;

	// float f0 = 0.0f;
	// float f1 = 0.0f;
	// float f2 = 0.0f;
	// f0 = gEnv->pTimer->GetAsyncCurTime();

	SoundBufferPropsMapItorConst It = m_soundBuffers.find(BufferProps);
	if (It != m_soundBuffers.end())
	{
		pBuf = (It->second);
		CTimeValue TimeNow = gEnv->pTimer->GetFrameStartTime();
		pBuf->UpdateTimer(TimeNow);

		// f1 = gEnv->pTimer->GetAsyncCurTime();

		pBuf->Load(NULL);
	}

	// f2 = gEnv->pTimer->GetAsyncCurTime();

	// TOMAS Profiling
	//{
	//	float fStep1 = (f1-f0)*1000.0f;
	//	float fStep2 = (f2-f1)*1000.0f;
	//	float fTotal = fStep1+fStep2;
	//	CryLog("<---------> SoundSystem Profile GetOrCreateSoundBuffer> %s %.2f", BufferProps.sName.c_str(),
	// fTotal);

	//	if (fTotal > 0.1f)
	//		int catchme = 0;

	//}

	return pBuf;
}

bool CSoundAssetManager::IsLoaded(const SSoundBufferProps& BufferProps)
{
	return (GetSoundBufferPtr(BufferProps) != NULL);
}

float CSoundAssetManager::AssetStillNeeded(CSoundBuffer* pBuf)
{
	// pBuf was tested before and is valid!

	if (m_pSoundSystem->g_nUnloadData == 0)
		return (1.0f);

	if (pBuf->GetProps()->nPrecacheFlags & FLAG_SOUND_PRECACHE_STAY_IN_MEMORY)
		return (1.0f);

	int nNumReferencedSounds = 0;
	ptParamINT32 TempParam(nNumReferencedSounds);
	pBuf->GetParam(apNUMREFERENCEDSOUNDS, &TempParam);
	TempParam.GetValue(nNumReferencedSounds);

	if (nNumReferencedSounds)
		return (1.0f);

	if (pBuf->GetProps()->nPrecacheFlags & FLAG_SOUND_PRECACHE_UNLOAD_NOW)
		return (0.0f);

	int nNumRef2 = nNumReferencedSounds * nNumReferencedSounds + 2;
	CTimeValue tDeltaTime = gEnv->pTimer->GetFrameStartTime() - pBuf->GetTimer();

	// right now that would be sqrt(nTimesUsed) plus some minimal of 10, that divided through the
	// seconds of that last Update (+1 to rule out div(0)), and that multiplied by the squared number of references
	// to the Buffer
	// float fCacheThreshold = ((sqrt((float)pBuf->GetInfo()->nTimesUsed)+20) / (1.0f + tDeltaTime.GetSeconds()));

	uint32 nUsedOverTime = 0;
	if (pBuf->GetInfo()->nTimesUsed > pBuf->GetInfo()->nTimedOut)
		nUsedOverTime = pBuf->GetInfo()->nTimesUsed - pBuf->GetInfo()->nTimedOut;

	float fCacheThreshold = nUsedOverTime / (1.0f + tDeltaTime.GetSeconds());

	// bool bAssetStillNeeded = (nCacheThreshold >= 1 );
	// bAssetStillNeeded = false;
	return (max(0.0f, fCacheThreshold - 1.0f));
}
