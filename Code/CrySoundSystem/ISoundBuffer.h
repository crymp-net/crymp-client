#pragma once

#include "CryCommon/CrySystem/ISystem.h" // CryLogAlways
#include "CryCommon/CryCore/CryFixedString.h"

#define MAXCHARBUFFERSIZE 512

enum enumBufferType
{
	btNONE,
	btSAMPLE,
	btSTREAM,
	btEVENT,
	btMICRO,
	btNETWORK
};

enum enumAssetLoadingStates
{
	alsSTILLLOADING,
	alsFINISHEDLOADING,
	alsNOTLOADED,
	alsLOADFAILURE
};

typedef CryFixedStringT<MAXCHARBUFFERSIZE> TFixedResourceName;

// active properties with copy constructor and overloaded operators for comparing sName
struct SSoundBufferProps
{
	SSoundBufferProps()
	{
		nFlags = 0;
		nPrecacheFlags = 0;
		eBufferType = btNONE;
	}

	SSoundBufferProps(const char* _pszName, uint32 _nFlags, enumBufferType _eBufferType, uint32 _nPrecacheFlags)
	{
		sName.assign(_pszName);

		nFlags = _nFlags;
		eBufferType = _eBufferType;
		nPrecacheFlags = _nPrecacheFlags;
	}
	SSoundBufferProps(const SSoundBufferProps& Props)
	{
		sName = Props.sName;
		sFilePath = Props.sFilePath;
		sResourceFile = Props.sResourceFile;

		nFlags = Props.nFlags;
		eBufferType = Props.eBufferType;
		nPrecacheFlags = Props.nPrecacheFlags;
	}
	bool operator<(const SSoundBufferProps& Props) const
	{
		if ((uint32)eBufferType < (uint32)Props.eBufferType)
			return true;

		if ((uint32)eBufferType > (uint32)Props.eBufferType)
			return false;

		int iCmp;

		iCmp = sName.compare(Props.sName);

		if (iCmp)
			return iCmp == -1;

		// if (eBufferType == btEVENT)
		//	return false;

		// uint32 nMyFlags = nFlags & SOUNDBUFFER_FLAG_MASK;
		// uint32 nOtherFlags = Props.nFlags & SOUNDBUFFER_FLAG_MASK;

		// if (nMyFlags<nOtherFlags)
		//	return true;

		// if (nMyFlags>nOtherFlags)
		//	return false;

		/*		if (Props.eBufferType == btEVENT)
		                        return (stricmp(sName.c_str(), Props.sName.c_str())<0);
		                else
		                {
		                        if (nFlags<Props.nFlags)
		                                return true;
		                        if (nFlags!=Props.nFlags)
		                                return false;
		                        return (stricmp(sName.c_str(), Props.sName.c_str())<0);
		                }
		*/
		return false;
	}
	// actual properties
	TFixedResourceName sName;
	TFixedResourceName sFilePath;
	TFixedResourceName sResourceFile;
	uint32 nFlags;
	uint32 nPrecacheFlags;
	enumBufferType eBufferType;
};

// information of that asset and the loading state
struct SSoundBufferInfo
{
	int32 nBaseFreq;
	int32 nBitsPerSample;
	int32 nChannels;
	uint32 nTimesUsed;
	uint32 nTimedOut;
	uint32 nLengthInBytes;
	uint32 nLengthInMs;
	uint32 nLengthInSamples;
	bool bLoadFailure;
};

class CSound;
class CSoundSystem;
class CSoundBuffer;
struct IMicrophoneStream;
struct ptParam;

typedef void* tAssetHandle;
typedef _smart_ptr<CSoundBuffer> CSoundBufferPtr;

typedef std::vector<CSound*> TBufferLoadReqVec;
typedef TBufferLoadReqVec::iterator TBufferLoadReqVecIt;
typedef TBufferLoadReqVec::const_iterator TBufferLoadReqVecItConst;

// special asset ParamSemantics
enum enumAssetParamSemantics
{
	apASSETNAME,
	apASSETTYPE,
	apASSETFREQUENCY,
	apLENGTHINBYTES,
	apLENGTHINMS,
	apLENGTHINSAMPLES,
	apBYTEPOSITION,
	apTIMEPOSITION,
	apLOADINGSTATE,
	apNUMREFERENCEDSOUNDS
};

enum eUnloadDataOptions
{
	sbUNLOAD_ALL_DATA,
	sbUNLOAD_UNNEEDED_DATA
};

//////////////////////////////
// A single microphone stream
struct IMicrophoneStream
{
	// ask for butter to be read
	virtual bool ReadDataBuffer(const void* ptr1, const unsigned int len1, const void* ptr2,
	                            const unsigned int len2) = 0;
	virtual void OnError(const char*) = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// SoundBuffer interface
//////////////////////////////////////////////////////////////////////////////////////////////
struct ISoundBuffer
{
	//////////////////////////////////////////////////////////////////////////
	// Management
	//////////////////////////////////////////////////////////////////////////
	virtual void SetSoundSystemPtr(CSoundSystem* pSoundSystem) = 0;
	virtual int AddRef() = 0;
	virtual int Release() = 0;
	virtual void RemoveFromLoadReqList(CSound* pSound) = 0;
	virtual void AddToLoadReqList(CSound* pSound) = 0;
	virtual bool Load(CSound* pSound) = 0;
	virtual bool WaitForLoad() = 0;
	virtual void AbortLoading() = 0;
	virtual void SetAssetHandle(tAssetHandle AssetHandle, enumBufferType eBufferType) = 0;
	virtual void SetProps(SSoundBufferProps NewProps) = 0;
	virtual void Update() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Information
	//////////////////////////////////////////////////////////////////////////

	// Gets and Sets Parameter defined in the enumAssetParam list
	virtual bool GetParam(enumAssetParamSemantics eSemantics, ptParam* pParam) const = 0;
	virtual bool SetParam(enumAssetParamSemantics eSemantics, ptParam* pParam) = 0;

	// TODO Clean up here!
	virtual tAssetHandle GetAssetHandle() const = 0;
	virtual SSoundBufferProps* GetProps() = 0;
	virtual SSoundBufferInfo* GetInfo() = 0;
	virtual const char* GetName() const = 0;
	virtual bool NotLoaded() const = 0;
	virtual bool Loaded() const = 0;
	virtual bool Loading() const = 0;
	virtual bool LoadFailure() const = 0;
	virtual uint32 GetMemoryUsed() = 0;
	virtual uint32 GetMemoryUsage(class ICrySizer* pSizer) = 0; // compute memory-consumption, returns size in Bytes
	virtual void LogDependencies() = 0;

	//////////////////////////////////////////////////////////////////////////
	//	platform dependent calls
	//////////////////////////////////////////////////////////////////////////
	virtual void* LoadAsSample(const char* AssetName, int nLength) = 0;
	virtual void* LoadAsStream(const char* AssetName, int nLength) = 0;
	virtual void* LoadAsEvent(const char* AssetName) = 0;
	virtual void* LoadAsMicro(IMicrophoneStream* pIMicrophoneStream) = 0;

	virtual bool UnloadData(const eUnloadDataOptions UnloadOption) = 0;
};
