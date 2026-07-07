#pragma once

#include <vector>

#include "CryCommon/CrySoundSystem/ISoundMoodManager.h"
#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/CrySystem/TimeValue.h"

#include "FMOD.h"

enum SAVETYPE
{
	eDONOTSAVE,
	eRESTORE_AT_START,
	eRESTORE_AT_SAVED_LOCATION
};

#define FLAG_SOUNDMOOD_VOLUME 0x000001
#define FLAG_SOUNDMOOD_PITCH 0x000002
#define FLAG_SOUNDMOOD_LOWPASS 0x000004
#define FLAG_SOUNDMOOD_HIGHPASS 0x000008

typedef struct Prop
{
	enumGroupParamSemantics eSemantics;
	ptParam* pParam; // TODO ptr, besser reference

} Prop;

// forward declaration
class CCategory;
class CMoodManager;
struct ISoundAssetManager;
class CMood;
class XmlNodeRef;
struct IRenderer;

// old stuff
typedef std::vector<_smart_ptr<ICategory>> tvecCategory;
typedef tvecCategory::iterator tvecCategoryIter;
typedef tvecCategory::const_iterator tvecCategoryIterConst;

typedef std::vector<_smart_ptr<IMood>> tvecMood;
typedef tvecMood::iterator tvecMoodIter;
typedef tvecMood::const_iterator tvecMoodIterConst;

typedef std::vector<Prop*> tvecProps;
typedef tvecProps::iterator tvecPropsIter;
typedef tvecProps::const_iterator tvecPropsIterConst;

struct SSoundMoodProp
{
	string sName;
	float fFadeCurrent;
	float fFadeTarget;
	int32 nFadeTimeInMS;
	bool bUnregisterOnFadeOut;
	bool bForceUpdate;
	bool bReadyToRemove;
};

typedef std::vector<SSoundMoodProp> tvecActiveMoods;
typedef tvecActiveMoods::iterator tvecActiveMoodsIter;
typedef tvecActiveMoods::const_iterator tvecActiveMoodsIterConst;

class CCategory final : ICategory
{
public:
	CCategory(CMoodManager* pMoodManager, ICategory* pParent, CMood* pMood);
	~CCategory();

	// Name
	virtual const char* GetName() const;
	virtual void SetName(const char* sCategoryName) { m_sName = sCategoryName; }
	virtual const char* GetMoodName() const;

	// Platform Category
	virtual void* GetPlatformCategory() const;
	virtual void SetPlatformCategory(void* pPlatformCategory);

	// Muted
	virtual bool GetMuted() const { return (m_nFlags & FLAG_SOUNDMOOD_MUTED) != 0; }
	virtual void SetMuted(bool bMuted);

	// Parent
	virtual ICategory* GetParentCategory() const { return m_ParentCategory; }
	virtual void SetParentCategory(ICategory* pParent) { m_ParentCategory = pParent; }

	// Children Categories
	virtual uint32 GetChildCount();
	virtual ICategory* GetCategoryByIndex(uint32 nCategoryCount) const;
	virtual ICategory* GetCategory(const char* sCategoryName) const;
	virtual ICategory* GetCategoryByPtr(void* pPlatformCategory) const;
	virtual ICategory* AddCategory(const char* sCategoryName);
	virtual bool RemoveCategory(const char* sCategoryName);

	// Parameter
	virtual bool GetParam(enumGroupParamSemantics eSemantics, ptParam* pParam) const;
	virtual bool SetParam(enumGroupParamSemantics eSemantics, ptParam* pParam);

	virtual bool InterpolateCategories(ICategory* pCat1, float fFade1, ICategory* pCat2, float fFade2,
	                                   bool bForceDefault);

	// Import Export
	virtual bool Serialize(XmlNodeRef& node, bool bLoading);
	virtual bool RefreshCategories(const bool bAddNew);

	// Functionality
	virtual ISoundMoodManager* GetIMoodManager() { return (ISoundMoodManager*)m_pMoodManager; }

	// Other
	virtual bool ApplyCategory(float fFade);

	// writes output to screen in debug
	virtual void DrawInformation(IRenderer* pRenderer, float& xpos, float& ypos, bool bAppliedValues);

	void* GetDSP(const int nDSPType);
	void SetDSP(const int nDSPType, void* pDSP);

	void GetDSPFromAll(const int nDSPType);
	void SetDSPForAll(const int nDSPType, void* pDSP);

private:
	bool ImportProps(XmlNodeRef& xmlNode);
	bool ExportProps(XmlNodeRef& xmlNode);

	ICategory* m_ParentCategory;
	int32 m_nFlags;
	CMoodManager* m_pMoodManager;
	CMood* m_pMood;

	FMOD::EventCategory* m_pPlatformCategory;
	FMOD_RESULT m_ExResult;
	FMOD::DSP* m_pDSPLowPass;
	FMOD::DSP* m_pDSPHighPass;
	float m_fMaxCategoryVolume;

	void FmodErrorOutput(const char* sDescription, ILog::ELogType LogType = ILog::eMessage);

	// Attributes
	string m_sName;
	uint32 m_nAttributes;
	float m_fVolume;
	float m_fPitch;
	float m_fLowPass;
	float m_fHighPass;
};

class CMood final : IMood
{
public:
	CMood(CMoodManager* pMoodManager);
	~CMood();

	// Name
	virtual const char* GetName() const;
	virtual void SetName(const char* sMoodName) { m_sMoodName = sMoodName; }

	// Master Category
	virtual ICategory* GetMasterCategory() const;

	// Categories
	virtual uint32 GetCategoryCount() const;
	virtual ICategory* GetCategoryByIndex(uint32 nCategoryCount) const;
	virtual ICategory* GetCategory(const char* sCategoryName) const;
	virtual ICategory* GetCategoryByPtr(void* pPlatformCategory) const;
	virtual ICategory* AddCategory(const char* sCategoryName);
	virtual bool RemoveCategory(const char* sCategoryName);

	// Priority
	virtual float GetPriority() const { return m_fPriority; }
	virtual void SetPriority(float fPriority) { m_fPriority = fPriority; }
	// Music Volume
	virtual float GetMusicVolume() const { return m_fMusicVolume; }
	virtual void SetMusicVolume(float fMusicVolume) { m_fMusicVolume = fMusicVolume; }

	// Import Export
	virtual bool Serialize(XmlNodeRef& node, bool bLoading);
	virtual void SetIsMixMood(const bool bIsMixMood) { m_bIsMixMood = bIsMixMood; }
	virtual bool GetIsMixMood() { return m_bIsMixMood; }

	// Functionality
	virtual bool InterpolateMoods(const IMood* pMood1, float fFade1, const IMood* pMood2, float fFade2,
	                              bool bForceDefault);
	virtual bool ApplyMood(float fFade);

	// Other
	virtual ISoundMoodManager* GetIMoodManager() { return (ISoundMoodManager*)m_pMoodManager; }

	// writes output to screen in debug
	virtual void DrawInformation(IRenderer* pRenderer, float& xpos, float& ypos, bool bAppliedValues);

	tvecCategory m_vecCategories;

private:
	CMoodManager* m_pMoodManager;
	string m_sMoodName;
	float m_fPriority;
	float m_fMusicVolume;
	bool m_bIsMixMood;

	FMOD_RESULT m_ExResult;
};

class CMoodManager final : public ISoundMoodManager
{
	friend class CCategory;
	friend class CMood;

public:
	//////////////////////////////////////////////////////////////////////////
	// Initialization
	//////////////////////////////////////////////////////////////////////////

	CMoodManager();
	~CMoodManager();

	virtual bool RefreshCategories(const char* sMoodName);
	virtual bool RefreshPlatformCategories();
	virtual void Release();
	virtual void Reset();

	//////////////////////////////////////////////////////////////////////////
	// Memory Usage
	//////////////////////////////////////////////////////////////////////////

	// Returns current Memory Usage
	virtual uint32 GetMemUsage(void) const { return m_nTotalMemUsedByManager; }

	//////////////////////////////////////////////////////////////////////////
	// Information
	//////////////////////////////////////////////////////////////////////////

	virtual uint32 GetNumberSoundsLoaded(void) const;

	// writes output to screen in debug
	virtual void DrawInformation(IRenderer* pRenderer, float& xpos, float& ypos, int nSoundInfo);

	//////////////////////////////////////////////////////////////////////////
	// Management
	//////////////////////////////////////////////////////////////////////////

	// adds a Mood to the MoodManager database
	virtual IMood* AddMood(const char* sMoodName);
	// removes a Mood (and all Category settings) defined by sMoodName from the MoodManager
	virtual bool RemoveMood(const char* sMoodName);

	//////////////////////////////////////////////////////////////////////////
	// Real Time Manipulation
	//////////////////////////////////////////////////////////////////////////

	// needs to be updated regularly
	virtual bool Update();

	// registers a Mood to be actively processes (starts with 0 fade value)
	virtual bool RegisterSoundMood(const char* sMoodName);

	// updates the fade value of a registered mood
	virtual bool UpdateSoundMood(const char* sMoodName, float fFade, uint32 nFadeTimeInMS = 0,
	                             bool bUnregistedOnFadeOut = true);

	// get current fade value of a registered mood
	virtual float GetSoundMoodFade(const char* sMoodName);

	// unregisters a Mood and removes it from the active ones
	virtual bool UnregisterSoundMood(const char* sMoodName);

	//////////////////////////////////////////////////////////////////////////
	// Access
	//////////////////////////////////////////////////////////////////////////

	virtual IMood* GetMoodPtr(uint32 nGroupCount) const;    // may be NULL
	virtual IMood* GetMoodPtr(const char* sMoodName) const; // may be NULL

	//////////////////////////////////////////////////////////////////////////
	// Import/Export
	//////////////////////////////////////////////////////////////////////////

	virtual bool Serialize(XmlNodeRef& node, bool bLoading);
	virtual void SerializeState(TSerialize ser);

	virtual bool RefreshCategories();

	//////////////////////////////////////////////////////////////////////////
private:
	void UpdateMemUsage(int nAddToTotal) { m_nTotalMemUsedByManager += nAddToTotal; }
	bool InterpolateMoods(const IMood* pMood1, const float fFade1, const IMood* pMood2, const float fFade2,
	                      IMood* pResultMood, float fResultFade);
	bool Load(const char* sFilename);

	FMOD_RESULT m_ExResult;

	uint32 m_nTotalMemUsedByManager;
	uint32 m_nTotalSoundsListed;

	tvecMood m_MoodArray; // array of all Mood-Ptrs.

	tvecActiveMoods m_ActiveMoods; // vec of all active Moods
	IMood* m_pFinalMix;
	bool m_bForceFinalMix;
	bool m_bNeedUpdate;

	CTimeValue m_tLastUpdate;
};
