#pragma once

#include "ISoundAssetManager.h"

class CSoundAssetManager final : ISoundAssetManager
{
	friend class CSoundSystem;

public:
	//////////////////////////////////////////////////////////////////////////
	// Initialization
	//////////////////////////////////////////////////////////////////////////

	CSoundAssetManager(CSoundSystem* pSoundSystem);
	virtual ~CSoundAssetManager(void);

	// Cleans up dead bufffers, defrags or kicks out low priority Buffers
	bool Update();

	//////////////////////////////////////////////////////////////////////////
	// Memory Usage
	//////////////////////////////////////////////////////////////////////////

	// gives a big memory block in Bytes to the Manager
	bool SetMemoryBlock(void* pBlockStart, unsigned int nBlockSize);

	// Let the Manager allocate a big memory block in Bytes to deal with
	bool AllocMemoryBlock(unsigned int nBlockSize);

	// returns a Ptr to a free chunk of memory to be used
	void* GetMemoryChunk(SSoundBufferProps BufferProps, unsigned int nChunkSize);

	// frees a chunk of memory that was used in a SoundBuffer
	bool FreeMemoryChunk(SSoundBufferProps BufferProps);

	// frees all SoundBuffers, deletes all data;
	bool ShutDown();

	// Returns current Memory Usage
	unsigned int GetMemUsage();
	virtual int GetMemoryUsage(class ICrySizer* pSizer); // compute memory-consumption, returns rough estimate in MB

	//////////////////////////////////////////////////////////////////////////

	unsigned int GetNumberBuffersLoaded();

	// writes output to screen in debug
	void DrawInformation(IRenderer* pRenderer, float xpos, float ypos);

	//////////////////////////////////////////////////////////////////////////
	// Asset Management

	// gets or creates a SoundBuffer for a referred sound on a specific platform and returns the pointer to it
	CSoundBuffer* GetOrCreateSoundBuffer(const SSoundBufferProps& AssetProps, CSound* pRefSound);

	// removes a SoundBuffer from the AssetManager
	bool RemoveSoundBuffer(const SSoundBufferProps& BufferProps);

	// locks the SoundBuffers so they don't get unloaded
	void LockResources();

	// unlocks the SoundBuffers so they could be unload if needed
	void UnlockResources();

	// unloads ALL SoundBuffers
	void UnloadAllSoundBuffers();

	// Checks and returns a Ptr to a loaded Buffer or NULL if its not there
	CSoundBuffer* GetSoundBufferPtr(const SSoundBufferProps& BufferProps) const;

	// Check if a Buffer is loaded
	bool IsLoaded(const SSoundBufferProps& BufferProps);

private:
	CSoundSystem* m_pSoundSystem;
	bool m_bUseMemoryBlock; // toggles if internal Memory Block should be used or not
	void* m_MemoryBlockStart;
	uint32 m_nMemoryBlockSize;
	uint32 m_nUsedMemorySize;

	uint32 m_nTotalSoundMemory;

	SoundBufferPropsMap m_soundBuffers;             // Map of CSoundBuffer* with the filename as the key
	std::vector<CSoundBufferPtr> m_lockedResources; // vector of SmartPtrs
	// or before playing each sound has to verify existence with a IsLoaded()

	// what is the strategy to prevent memory defragmentation?
	bool DefragMemoryBlock();

	// This one decides if an asset should be kept in memory or kicked out ( == 0)
	float AssetStillNeeded(CSoundBuffer* pBuf);

	// internally removes a SoundBuffer from the AssetManager using reverse iterator
	// bool	RemoveSoundBuffer(SoundBufferPropsMapRevItor Iter);
};
