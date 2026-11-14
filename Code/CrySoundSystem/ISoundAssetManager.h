#pragma once

#include <map>

#include "SoundBuffer.h"

enum enumAssetManagerParamSemantics
{
	ampNONE
};

typedef std::map<SSoundBufferProps, CSoundBufferPtr> SoundBufferPropsMap;
typedef SoundBufferPropsMap::iterator SoundBufferPropsMapItor;
typedef SoundBufferPropsMap::reverse_iterator SoundBufferPropsMapRevItor;
typedef SoundBufferPropsMap::const_iterator SoundBufferPropsMapItorConst;

struct IRenderer;

//////////////////////////////////////////////////////////////////////////////////////////////
// Sound Asset Manager Interface
//////////////////////////////////////////////////////////////////////////////////////////////
struct ISoundAssetManager
{
	virtual ~ISoundAssetManager() = default;

	//////////////////////////////////////////////////////////////////////////
	// Management
	//////////////////////////////////////////////////////////////////////////

	// gives a big memory block in Bytes to the Manager
	virtual bool SetMemoryBlock(void* pBlockStart, unsigned int nBlockSize) = 0;

	// Let the Manager allocate a big memory block in Bytes to deal with
	virtual bool AllocMemoryBlock(unsigned int nBlockSize) = 0;

	// returns a Ptr to a free chunk of memory to be used
	virtual void* GetMemoryChunk(SSoundBufferProps BufferProps, unsigned int nChunkSize) = 0;

	// frees a chunk of memory that was used in a SoundBuffer
	virtual bool FreeMemoryChunk(SSoundBufferProps BufferProps) = 0;

	// frees the big memory block
	// virtual bool FreeMemoryBlock() = 0;

	// frees all SoundBuffers, deletes all data;
	virtual bool ShutDown() = 0;

	// Returns current Memory Usage
	virtual unsigned int GetMemUsage() = 0;
	virtual int GetMemoryUsage(class ICrySizer* pSizer) = 0;
	virtual unsigned int GetNumberBuffersLoaded() = 0;

	// Cleans up dead bufffers, defrags or kicks out low priority Buffers
	virtual bool Update() = 0;

	// writes output to screen in debug
	virtual void DrawInformation(IRenderer* pRenderer, float xpos, float ypos) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Buffer Management
	//////////////////////////////////////////////////////////////////////////

	// // gets or creates a SoundBuffer for a referred sound on a specific platform and returns the pointer to it
	virtual CSoundBuffer* GetOrCreateSoundBuffer(const SSoundBufferProps& BufferProps, CSound* pRefSound) = 0;

	// removes a SoundBuffer from the AssetManager
	virtual bool RemoveSoundBuffer(const SSoundBufferProps& BufferProps) = 0;

	// locks the SoundBuffers so they don't get unloaded
	virtual void LockResources() = 0;

	// unlocks the SoundBuffers so they could be unload if needed
	virtual void UnlockResources() = 0;

	// unloads ALL SoundBuffers
	virtual void UnloadAllSoundBuffers() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Asset Management
	//////////////////////////////////////////////////////////////////////////

	// Checks and returns a Ptr to a loaded Buffer or NULL if its not there
	virtual CSoundBuffer* GetSoundBufferPtr(const SSoundBufferProps& BufferProps) const = 0;

	// Check if a Buffer is loaded
	virtual bool IsLoaded(const SSoundBufferProps& BufferProps) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Wavebank Management
	//////////////////////////////////////////////////////////////////////////

	// registers a wavebank name with full path
	// virtual void CollectWaveBank(const char* sWaveBankName)

	//////////////////////////////////////////////////////////////////////////
	// Information
	//////////////////////////////////////////////////////////////////////////

	// Gets and Sets Parameter defined in the enumAssetParam list
	// virtual bool	GetParam(enumAssetManagerParamSemantics eSemantics, ptParam* pParam) = 0;
	// virtual bool SetParam(enumAssetManagerParamSemantics eSemantics, ptParam* pParam) = 0;
};
