#pragma once

#include "CryCommon/CrySoundSystem/ISound.h"
#include "CryCommon/CrySystem/IStreamEngine.h"

#include "ISoundBuffer.h"

class CSound;
class CSoundAssetManager;
class CSoundSystem;

class CSoundBuffer : public IStreamCallback, public ISoundBuffer, public ISoundEventListener
{
	friend class CSoundAssetManager;

public:
	CSoundBuffer(const SSoundBufferProps& Props);
	virtual ~CSoundBuffer();

	// ISoundBuffer Interface:

	//////////////////////////////////////////////////////////////////////////
	// Management
	//////////////////////////////////////////////////////////////////////////
	void SetSoundSystemPtr(CSoundSystem* pSoundSystem) { m_pSoundSystem = pSoundSystem; }
	int AddRef();
	int Release();
	void RemoveFromLoadReqList(CSound* pSound);
	void AddToLoadReqList(CSound* pSound);
	bool Load(CSound* pSound);
	bool WaitForLoad();
	void AbortLoading();
	void SetAssetHandle(tAssetHandle AssetHandle,
	                    enumBufferType eBufferType); // Sets the AssetHandle/AssetType and does some managment work
	void SetProps(SSoundBufferProps NewProps);       // Sets new Properties (especially used for the AssetType)
	void Update() {}
	void UpdateTimer(CTimeValue tNewTime) { m_tLastTimeUsed = tNewTime; }
	CTimeValue GetTimer() { return (m_tLastTimeUsed); }

	//////////////////////////////////////////////////////////////////////////
	// Information
	//////////////////////////////////////////////////////////////////////////
	tAssetHandle GetAssetHandle() const { return (m_BufferData); } // returns the Ptr to the real Asset
	SSoundBufferProps* GetProps() { return (&m_Props); }
	SSoundBufferInfo* GetInfo() { return (&m_Info); }
	const char* GetName() const { return m_Props.sName.c_str(); }
	bool NotLoaded() const { return (m_BufferData == NULL) && (m_pReadStream == NULL); }
	bool Loaded() const { return (m_BufferData != NULL) && (m_pReadStream == NULL); }
	bool Loading() const { return (m_BufferData == NULL) && (m_pReadStream != NULL); }
	bool LoadFailure() const { return (m_Info.bLoadFailure); }
	virtual uint32 GetMemoryUsed() { return 0; }
	virtual void LogDependencies() {}

	//////////////////////////////////////////////////////////////////////////
	//	platform dependent calls
	//////////////////////////////////////////////////////////////////////////

	// Loads a file or memory block into memory of the used sound library
	// TODO change interface to (void* Data, void* DataPros) to make it very generic
	virtual tAssetHandle LoadAsSample(const char* AssetDataPtrOrName, int nLength) { return 0; }

	// Opens a file or memory block as a stream
	virtual tAssetHandle LoadAsStream(const char* AssetName, int nLength) { return 0; }

	// loads a event sound
	virtual tAssetHandle LoadAsEvent(const char* AssetName) { return 0; }

	// loads as microphone stream
	virtual tAssetHandle LoadAsMicro(IMicrophoneStream* pIMicrophoneStream) { return 0; }

	// loads as network stream
	virtual tAssetHandle LoadAsNetworkStream(INetworkSoundListener* pINetworkSoundListener) { return 0; }

	// Gets and Sets Parameter defined in the enumAssetParam list
	virtual bool GetParam(enumAssetParamSemantics eSemantics, ptParam* pParam) const = 0;
	virtual bool SetParam(enumAssetParamSemantics eSemantics, ptParam* pParam) = 0;

	// Closes down Stream or frees memory of the Sample
	virtual bool UnloadData(const eUnloadDataOptions UnloadOption) = 0;

	//////////////////////////////////////////////////////////////////////////
	// ISoundEventListener implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void OnSoundEvent(ESoundCallbackEvent event, ISound* pSound);
	//////////////////////////////////////////////////////////////////////////

protected:
	// internal methods for handling callback and debug warninges
	// (not that good to have crucial code in there, but well...)

	virtual void SoundBufferLoaded(); // Called after successful loading
	virtual void LoadFailed();        // Called after failed loading
	virtual void UpdateCallbacks();   // Called after changing states

	CSoundSystem* m_pSoundSystem;             // Ptr to the global SoundSystem
	CSoundAssetManager* m_pSoundAssetManager; // Ptr to the global AssetManager
	int m_nRef;                               // Reference Counter
	tAssetHandle m_BufferData;                // Ptr to AssetHandle
	IReadStreamPtr m_pReadStream;             // used Stream Ptr of StreamingEngine
	bool m_bCallbackGuard;                    // used to prevent recursive Callback loop

	SSoundBufferInfo m_Info;   // Holds passive information about the asset
	SSoundBufferProps m_Props; // Holds active parameters about the asset (name/flags/type)

	TBufferLoadReqVec m_vecLoadReq;         // SoundPtrs which reference to this asset
	TBufferLoadReqVec m_vecLoadReqToRemove; // SoundPtrs which dont need this asset anymore
	TBufferLoadReqVec m_vecLoadReqToAdd;    // SoundPtrs which want to use this asset

	// add Timer
	CTimeValue m_tLastTimeUsed;
	unsigned int m_nPriority;
	CSoundAssetManager* m_pManager;
};
