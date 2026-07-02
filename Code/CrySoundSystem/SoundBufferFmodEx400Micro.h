#pragma once

#include <mutex>

#include "CryCommon/CrySystem/ILog.h"

#include "FMOD.h"
#include "SoundBuffer.h"

struct IMicrophoneStream;

class CSoundBufferFmodEx400Micro final : public CSoundBuffer
{
public:
	CSoundBufferFmodEx400Micro(const SSoundBufferProps& Props, FMOD::System* pCSEX);

protected:
	~CSoundBufferFmodEx400Micro();

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

	// loads as microphone stream
	virtual tAssetHandle LoadAsMicro(IMicrophoneStream* pIMicrophoneStream);

	virtual void Update();

	// Gets and Sets Parameter defined in the enumAssetParam list
	virtual bool GetParam(enumAssetParamSemantics eSemantics, ptParam* pParam) const;
	virtual bool SetParam(enumAssetParamSemantics eSemantics, ptParam* pParam);

	// Closes down Stream or frees memory of the Sample
	virtual bool UnloadData(const eUnloadDataOptions UnloadOption);

private:
	FMOD::System* m_pCSEX;
	FMOD_RESULT m_ExResult;
	IMicrophoneStream* m_pMicrophoneStream;
	unsigned int m_nLastRecordPos;
	std::recursive_mutex m_mutex;

	void FmodErrorOutput(const char* sDescription, ILog::ELogType LogType = ILog::eMessage);
};
