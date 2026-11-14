#pragma once

#include <map>
#include <vector>

#include "CryCommon/CrySystem/ILog.h"

#include "IPlatformSound.h"
#include "FMOD.h"

class CAudioDeviceFmodEx400;

// queued parameter

typedef std::map<string, int> tmapParameterNames;
typedef tmapParameterNames::iterator tmapParameterNamesIter;
typedef tmapParameterNames::const_iterator tmapParameterNamesIterConst;

typedef struct SQueuedParameter
{
public:
	int nNameIndex;
	enumPlatformSoundParamSemantics eSemantic;
	int nEventIndex;
	float fValue;
	int nValue;

	// Constructor
	SQueuedParameter()
	{
		nNameIndex = -1;
		eSemantic = pspNONE;
		nEventIndex = -1;
		fValue = 0.0f;
		nValue = 0;
	}

} SQueuedParameter;

typedef std::vector<SQueuedParameter> tvecQueuedParameter;
typedef tvecQueuedParameter::iterator tvecQueuedParameterIter;
typedef tvecQueuedParameter::const_iterator tvecQueuedParameterIterConst;

typedef std::vector<int> tvecLocalParameterIndex;
typedef tvecLocalParameterIndex::iterator tvecLocalParameterIndexIter;
typedef tvecLocalParameterIndex::const_iterator tvecLocalParameterIndexIterConst;

class CPlatformSoundFmodEx400Event final : public IPlatformSound
{
public:
	CPlatformSoundFmodEx400Event(CSound* pSound, FMOD::System* pCSEX, const char* sEventName);
	~CPlatformSoundFmodEx400Event(void);

	//////////////////////////////////////////////////////////////////////////
	// Management
	//////////////////////////////////////////////////////////////////////////
	virtual void SetSoundPtr(CSound* pSound) { m_pSound = pSound; }
	virtual int32 AddRef();
	virtual int32 Release();
	virtual bool CreateSound(tAssetHandle AssetHandle, SSoundSettings SoundSettings);
	virtual bool PlaySound(bool bStartPaused); // should or could be done through SetParam PlayModes
	virtual bool StopSound();                  // should or could be done through SetParam PlayModes
	virtual bool FreeSoundHandle();
	virtual bool Set3DPosition(Vec3* pvPosition, Vec3* pvVelocity, Vec3* pvOrientation);
	virtual void SetObstruction(const SObstruction* pObstruction);
	virtual void SetPlatformSoundName(const char* sPlatformSoundName) { m_sEventName = sPlatformSoundName; };

	virtual enumPlatformSoundStates GetState() { return m_State; }

	//////////////////////////////////////////////////////////////////////////
	// Information
	//////////////////////////////////////////////////////////////////////////

	virtual enumPlatformSoundClass GetClass() const { return pscEVENT; }
	virtual void Reset(CSound* pSound, const char* sEventName);

	// Gets and Sets Parameter defined in the enumAssetParam list
	virtual bool GetParamByType(enumPlatformSoundParamSemantics eSemantics, ptParam* pParam);
	virtual bool SetParamByType(enumPlatformSoundParamSemantics eSemantics, ptParam* pParam);

	// Gets and Sets Parameter defined by string and float value, returns the index of that parameter
	virtual int GetParamByName(const char* sParameter, float* fValue, bool bOutputWarning = true);
	virtual int SetParamByName(const char* sParameter, float fValue, bool bOutputWarning = true);

	// Gets and Sets Parameter defined by index and float value
	virtual bool GetParamByIndex(int nIndex, float* fValue, bool bOutputWarning = true);
	virtual bool SetParamByIndex(int nIndex, float fValue, bool bOutputWarning = true);

	virtual tSoundHandle GetSoundHandle() const;

	virtual bool IsInCategory(const char* sCategory);

	// Memory
	virtual void GetMemoryUsage(ICrySizer* pSizer);

private:
	CSound* m_pSound;  // Ptr to the sound this implementation belongs to
	int32 m_nRefCount; // Refcounting, although it does not really make sense, but who knows

	enumPlatformSoundStates m_State;

	// int			m_nChannel;		// the actual FMOD Sound Handle is a channel
	FMOD::System* m_pCSEX;
	FMOD::Channel* m_pExChannel;
	FMOD::Event* m_pEvent;
	FMOD_RESULT m_ExResult;
	float m_fMaxRadius;
	float m_fLoudness;
	float m_fMaxVolume;
	string m_sEventName;
	int32 m_nLenghtInMs;
	int m_nCurrentSpawns;
	int m_nMaxSpawns;
	int m_nDistanceIndex;
	CAudioDeviceFmodEx400* m_pAudioDevice;

	tvecQueuedParameter m_QueuedParameter;

	static tmapParameterNames ParameterNames;
	tvecLocalParameterIndex m_ParameterIndex;

	FMOD_REVERB_CHANNELPROPERTIES m_ReverbProps;

	void FmodErrorOutput(const char* sDescription, ILog::ELogType LogType = ILog::eMessage);
	static FMOD_RESULT F_CALLBACK OnCallBack(FMOD_EVENT* event, FMOD_EVENT_CALLBACKTYPE type, void* param1,
	                                         void* param2, void* userdata);
};
