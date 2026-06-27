#pragma once

#include "CryCommon/CrySystem/ILog.h"

#include "IPlatformSound.h"
#include "FMOD.h"

class CAudioDeviceFmodEx400;

class CPlatformSoundFmodEx400 final : public IPlatformSound
{
public:
	CPlatformSoundFmodEx400(CSound* pSound, FMOD::System* pCSEX);
	~CPlatformSoundFmodEx400(void);

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
	virtual void SetObstruction(const SObstruction* pObstruction) {};
	virtual void SetPlatformSoundName(const char* sPlatformSoundName) {};

	virtual enumPlatformSoundStates GetState() { return m_State; }

	//////////////////////////////////////////////////////////////////////////
	// Information
	//////////////////////////////////////////////////////////////////////////

	virtual enumPlatformSoundClass GetClass() const { return pscSOUND; }
	virtual void Reset(CSound* pSound, const char* sEventName);

	// Gets and Sets Parameter defined in the enumAssetParam list
	virtual bool GetParamByType(enumPlatformSoundParamSemantics eSemantics, ptParam* pParam);
	virtual bool SetParamByType(enumPlatformSoundParamSemantics eSemantics, ptParam* pParam);

	// Gets and Sets Parameter defined by string and float value, returns the index of that parameter
	virtual int GetParamByName(const char* sParameter, float* fValue, bool bOutputWarning = true) { return (-1); }
	virtual int SetParamByName(const char* sParameter, float fValue, bool bOutputWarning = true) { return (-1); }

	// Gets and Sets Parameter defined by index and float value
	virtual bool GetParamByIndex(int nIndex, float* fValue, bool bOutputWarning = true) { return false; }
	virtual bool SetParamByIndex(int nIndex, float fValue, bool bOutputWarning = true) { return false; }

	virtual tSoundHandle GetSoundHandle() const;

	virtual bool IsInCategory(const char* sCategory) { return false; }

	// Memory
	virtual void GetMemoryUsage(ICrySizer* pSizer);

private:
	CSound* m_pSound;  // Ptr to the sound this implementation belongs to
	int32 m_nRefCount; // Refcounting, although it does not really make sense, but who knows

	enumPlatformSoundStates m_State;

	FMOD::System* m_pCSEX;
	FMOD::Channel* m_pExChannel;
	FMOD_RESULT m_ExResult;
	CAudioDeviceFmodEx400* m_pAudioDevice;

	void FmodErrorOutput(const char* sDescription, ILog::ELogType LogType = ILog::eMessage);
	static FMOD_RESULT F_CALLBACK OnCallBack(FMOD_CHANNEL* channel, FMOD_CHANNEL_CALLBACKTYPE type, int command,
	                                         unsigned int commanddata1, unsigned int commanddata2);
};
