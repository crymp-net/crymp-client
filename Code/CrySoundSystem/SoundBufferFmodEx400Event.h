#pragma once

#include "CryCommon/CrySystem/ILog.h"

#include "FMOD.h"
#include "SoundBuffer.h"

class CSoundBufferFmodEx400Event final : public CSoundBuffer
{
public:
	CSoundBufferFmodEx400Event(const SSoundBufferProps& Props, FMOD::System* pCSEX);

protected:
	~CSoundBufferFmodEx400Event();

	//////////////////////////////////////////////////////////////////////////
	// Inherited Method by IStreamCallBack
	//////////////////////////////////////////////////////////////////////////
	virtual void StreamOnComplete(IReadStream* pStream, unsigned nError) {};

	//////////////////////////////////////////////////////////////////////////
	//	platform dependent calls
	//////////////////////////////////////////////////////////////////////////

	virtual uint32 GetMemoryUsed();
	virtual uint32 GetMemoryUsage(class ICrySizer* pSizer); // compute memory-consumption, returns size in Bytes
	virtual void LogDependencies();

	// loads a event sound
	virtual tAssetHandle LoadAsEvent(const char* AssetName);

	// Gets and Sets Parameter defined in the enumAssetParam list
	virtual bool GetParam(enumAssetParamSemantics eSemantics, ptParam* pParam) const;
	virtual bool SetParam(enumAssetParamSemantics eSemantics, ptParam* pParam);

	// Closes down Stream or frees memory of the Sample
	virtual bool UnloadData(const eUnloadDataOptions UnloadOption);

private:
	FMOD::System* m_pCSEX;
	FMOD_RESULT m_ExResult;
	void FmodErrorOutput(const char* sDescription, ILog::ELogType LogType = ILog::eMessage);

	std::vector<CTimeValue> m_EventTimeouts;
};
