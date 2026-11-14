#pragma once

#include "CryCommon/CrySystem/ILog.h"

#include "FMOD.h"
#include "SoundBuffer.h"

class CSoundBufferFmodEx400Network final : public CSoundBuffer
{
public:
	CSoundBufferFmodEx400Network(const SSoundBufferProps& Props, FMOD::System* pCSEX);
	INetworkSoundListener* GetINetworkStreamListener() { return m_pINetworkSoundListener; }

protected:
	~CSoundBufferFmodEx400Network();

	//////////////////////////////////////////////////////////////////////////
	// Inherited Method by IStreamCallBack
	//////////////////////////////////////////////////////////////////////////
	virtual void StreamOnComplete(IReadStream* pStream, unsigned nError);

	//////////////////////////////////////////////////////////////////////////
	//	platform dependent calls
	//////////////////////////////////////////////////////////////////////////

	virtual uint32 GetMemoryUsed() { return 0; }
	virtual uint32 GetMemoryUsage(class ICrySizer* pSizer); // compute memory-consumption, returns size in Bytes
	virtual void LogDependencies() {}

	// loads as network stream
	virtual tAssetHandle LoadAsNetworkStream(INetworkSoundListener* pINetworkSoundListener);

	// Gets and Sets Parameter defined in the enumAssetParam list
	virtual bool GetParam(enumAssetParamSemantics eSemantics, ptParam* pParam) const;
	virtual bool SetParam(enumAssetParamSemantics eSemantics, ptParam* pParam);

	// Closes down Stream or frees memory of the Sample
	virtual bool UnloadData(const eUnloadDataOptions UnloadOption);

private:
	FMOD::System* m_pCSEX;
	FMOD_RESULT m_ExResult;
	INetworkSoundListener* m_pINetworkSoundListener;
	unsigned int m_nLastRecordPos;
	void* m_TempBuffer;

	void FmodErrorOutput(const char* sDescription, ILog::ELogType LogType = ILog::eMessage);
};
