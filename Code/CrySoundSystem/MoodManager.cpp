#include "CryCommon/CryNetwork/ISerialize.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CrySystem/ITimer.h"

#include "IAudioDevice.h"
#include "ISoundAssetManager.h"
#include "ISoundBuffer.h"
#include "MoodManager.h"
#include "Sound.h"
#include "SoundSystem.h"

static float round_near(float f, int precision)
{
	float r = powf(10.f, (float)precision);
	int n = int_round(f * r);
	return (float)n / r;
}

//////////////////////////////////////////////////////////////////////////
// CCategory Definition
//////////////////////////////////////////////////////////////////////////

CCategory::CCategory(CMoodManager* pGroupManager, ICategory* pParent, CMood* pMood)
{
	assert(pGroupManager);

	m_ExResult = FMOD_OK;
	m_pDSPLowPass = 0;
	m_pDSPHighPass = 0;
	m_pPlatformCategory = NULL;
	m_fMaxCategoryVolume = -1.0f;

	m_pMoodManager = pGroupManager;
	m_pMood = pMood;
	m_nFlags = 0;
	m_ParentCategory = NULL;

	// Attributes
	m_nAttributes = 0;
	m_fVolume = 1.0f;
	m_fPitch = 0.0f;
	m_fLowPass = 22000.0f;
	m_fHighPass = 10.0f;
}

CCategory::~CCategory()
{
	FMOD::System* pFMODSystem = NULL;

	if (gEnv->pSoundSystem)
	{
		pFMODSystem = (FMOD::System*)gEnv->pSoundSystem->GetIAudioDevice();
	}

	if (pFMODSystem && m_pDSPLowPass)
	{
		if (m_pDSPLowPass)
		{
			m_ExResult = m_pDSPLowPass->remove();
			m_ExResult = m_pDSPLowPass->release();
		}
		if (m_pDSPHighPass)
		{
			m_ExResult = m_pDSPHighPass->remove();
			m_ExResult = m_pDSPHighPass->release();
		}
	}
}

void CCategory::FmodErrorOutput(const char* sDescription, ILog::ELogType LogType)
{
	switch (LogType)
	{
		case ILog::eWarning:
			gEnv->pLog->LogWarning("<Sound> MoodManager: %s (%d) %s\n", sDescription, m_ExResult,
			                       FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eError:
			gEnv->pLog->LogError("<Sound> MoodManager: %s (%d) %s\n", sDescription, m_ExResult,
			                     FMOD_ErrorString(m_ExResult));
			break;
		case ILog::eMessage:
			gEnv->pLog->Log("<Sound> MoodManager: %s (%d) %s\n", sDescription, m_ExResult,
			                FMOD_ErrorString(m_ExResult));
			break;
		default:
			break;
	}
}

const char* CCategory::GetName() const
{
	return m_sName.c_str();
	// return (m_pMoodManager->GetCategoryName(m_Handle));
}

const char* CCategory::GetMoodName() const
{
	return m_pMood->GetName();
}

void* CCategory::GetPlatformCategory() const
{
	return m_pPlatformCategory;
}

void CCategory::SetPlatformCategory(void* pPlatformCategory)
{
	m_pPlatformCategory = (FMOD::EventCategory*)pPlatformCategory;

	if (m_pPlatformCategory)
	{
		if (m_fMaxCategoryVolume < 0)
			m_ExResult = m_pPlatformCategory->getVolume(&m_fMaxCategoryVolume);
	}
}

void CCategory::SetMuted(bool bMuted)
{
	if (bMuted)
		m_nFlags |= FLAG_SOUNDMOOD_MUTED;
	else
		m_nFlags &= ~FLAG_SOUNDMOOD_MUTED;
}

uint32 CCategory::GetChildCount()
{
	FMOD_RESULT Result;
	int nNum = 0;

	if (m_pPlatformCategory)
		Result = m_pPlatformCategory->getNumCategories(&nNum);

	return nNum;
}

ICategory* CCategory::GetCategoryByIndex(uint32 nCategoryCount) const
{
	if (m_pPlatformCategory)
	{
		FMOD::EventCategory* pFMODCat = nullptr;
		FMOD_RESULT result = m_pPlatformCategory->getCategoryByIndex(nCategoryCount, &pFMODCat);
		if (result == FMOD_OK && pFMODCat)
		{
			return GetCategoryByPtr(pFMODCat);
		}
	}

	return nullptr;
}

ICategory* CCategory::GetCategory(const char* sCategoryName) const
{
	ICategory* pElement = NULL;

	if (m_pPlatformCategory)
	{
		FMOD::EventCategory* pSearch = NULL;
		FMOD_RESULT Result;
		Result = m_pPlatformCategory->getCategory(sCategoryName, &pSearch);
		if (pSearch && Result == FMOD_OK)
		{
			pElement = m_pMood->GetCategoryByPtr((void*)pSearch);
		}
	}

	return pElement;
}

ICategory* CCategory::GetCategoryByPtr(void* pPlatformCategory) const
{
	return m_pMood->GetCategoryByPtr(pPlatformCategory);
}

ICategory* CCategory::AddCategory(const char* sCategoryName)
{
	ICategory* pChild = m_pMood->AddCategory(sCategoryName);

	if (pChild)
		pChild->SetParentCategory(this);

	return pChild;
}

bool CCategory::RemoveCategory(const char* sCategoryName)
{
	bool bResult = false;

	if (m_pPlatformCategory)
	{
		FMOD::EventCategory* pSearch = NULL;
		m_pPlatformCategory->getCategory(sCategoryName, &pSearch);

		if (pSearch)
			bResult = m_pMood->RemoveCategory(sCategoryName);
	}

	return bResult;
}

bool CCategory::GetParam(enumGroupParamSemantics eSemantics, ptParam* pParam) const
{
	bool bResult = true;

	switch (eSemantics)
	{
		case gspNONE:
			break;
		case gspFLAGS:
		{
			int32 nTemp = 0;

			if (!(pParam->GetValue(nTemp)))
				return false;

			nTemp = m_nFlags;
			pParam->SetValue(nTemp);
			break;
		}
		case gspFVOLUME:
		{
			f32 fTemp = 1.0f;
			if (!(pParam->GetValue(fTemp)))
				return false;

			if (!(m_nAttributes & FLAG_SOUNDMOOD_VOLUME))
			{
				pParam->SetValue(1.0f);
				return false;
			}

			pParam->SetValue(m_fVolume);

			break;
		}
		case gspFPITCH:
		{
			f32 fTemp = 0.0f;
			if (!(pParam->GetValue(fTemp)))
				return false;

			if (!(m_nAttributes & FLAG_SOUNDMOOD_PITCH))
			{
				pParam->SetValue(0.0f);
				return false;
			}

			pParam->SetValue(m_fPitch);

			break;
		}
		case gspFLOWPASS:
		{
			f32 fTemp = 22000.0f;
			if (!(pParam->GetValue(fTemp)))
				return false;

			if (!(m_nAttributes & FLAG_SOUNDMOOD_LOWPASS))
			{
				pParam->SetValue(22000.0f);
				return false;
			}

			pParam->SetValue(m_fLowPass);

			break;
		}
		case gspFHIGHPASS:
		{
			f32 fTemp = 10.0f;

			if (!(pParam->GetValue(fTemp)))
				return false;

			if (!(m_nAttributes & FLAG_SOUNDMOOD_HIGHPASS))
			{
				pParam->SetValue(10.0f);
				return false;
			}

			pParam->SetValue(m_fHighPass);

			break;
		}
		case gspMUTED:
		{
			bool bTemp = false;
			if (!(pParam->GetValue(bTemp)))
				return false;

			bTemp = (m_nFlags & FLAG_SOUNDMOOD_MUTED) != 0;
			pParam->SetValue(bTemp);
			break;
		}

		default:
			break;
	}

	return bResult;
}

bool CCategory::SetParam(enumGroupParamSemantics eSemantics, ptParam* pParam)
{
	bool bResult = true;

	switch (eSemantics)
	{
		case gspNONE:
			break;

		case gspFLAGS:
		{
			int32 nTemp = 0;
			if (!(pParam->GetValue(nTemp)))
				return false;
			m_nFlags = nTemp;
			break;
		}
		break;

		case gspMUTED:
		{
			bool bTemp = false;
			if (!(pParam->GetValue(bTemp)))
				return false;

			if (bTemp)
				m_nFlags |= FLAG_SOUNDMOOD_MUTED;
			else
				m_nFlags &= ~FLAG_SOUNDMOOD_MUTED;

			break;
		}

		case gspFVOLUME:
		{
			f32 fTemp = 1.0f;
			if (!(pParam->GetValue(fTemp)))
				return false;

			m_fVolume = clamp(fTemp, 0.0f, 1.0f);

			if (m_fVolume < 1.0f)
				m_nAttributes |= FLAG_SOUNDMOOD_VOLUME;
			else
				m_nAttributes &= ~FLAG_SOUNDMOOD_VOLUME;

			break;
		}

		case gspFPITCH:
		{
			f32 fTemp = 0.0f;
			if (!(pParam->GetValue(fTemp)))
				return false;

			m_fPitch = fTemp;

			if (m_fPitch != 0.0f)
				m_nAttributes |= FLAG_SOUNDMOOD_PITCH;
			else
				m_nAttributes &= ~FLAG_SOUNDMOOD_PITCH;

			break;
		}

		case gspFLOWPASS:
		{
			f32 fTemp = 22000.0f;
			if (!(pParam->GetValue(fTemp)))
				return false;

			m_fLowPass = clamp(fTemp, 10.0f, 22000.0f);

			if (m_fLowPass < 22000.0f)
				m_nAttributes |= FLAG_SOUNDMOOD_LOWPASS;
			else
				m_nAttributes &= ~FLAG_SOUNDMOOD_LOWPASS;

			break;
		}
		case gspFHIGHPASS:
		{
			f32 fTemp = 10.0f;
			if (!(pParam->GetValue(fTemp)))
				return false;

			m_fHighPass = clamp(fTemp, 10.0f, 22000.0f);

			if (m_fHighPass > 10.0f)
				m_nAttributes |= FLAG_SOUNDMOOD_HIGHPASS;
			else
				m_nAttributes &= ~FLAG_SOUNDMOOD_HIGHPASS;

			break;
		}
		default:
			break;
	}

	return bResult;
}

bool CCategory::ImportProps(XmlNodeRef& xmlNode)
{
	bool bResult = true;
	f32 fTemp = 0.0f;
	ptParamF32 Param(fTemp);

	// Volume
	if (xmlNode->getAttr("Volume", m_fVolume))
		m_nAttributes |= FLAG_SOUNDMOOD_VOLUME;

	// Pitch
	if (xmlNode->getAttr("Pitch", m_fPitch))
		m_nAttributes |= FLAG_SOUNDMOOD_PITCH;

	// Lowpass
	if (xmlNode->getAttr("LowPass", m_fLowPass))
		m_nAttributes |= FLAG_SOUNDMOOD_LOWPASS;

	// Highpass
	if (xmlNode->getAttr("HighPass", m_fHighPass))
		m_nAttributes |= FLAG_SOUNDMOOD_HIGHPASS;

	return bResult;
}

bool CCategory::ExportProps(XmlNodeRef& xmlNode)
{
	bool bResult = true;

	// Save the Flags

	// save other Properties
	if (m_nAttributes & FLAG_SOUNDMOOD_VOLUME && m_fVolume < 1.0f)
		xmlNode->setAttr("Volume", m_fVolume);

	if (m_nAttributes & FLAG_SOUNDMOOD_PITCH && m_fPitch != 0.0f)
		xmlNode->setAttr("Pitch", m_fPitch);

	if (m_nAttributes & FLAG_SOUNDMOOD_LOWPASS && m_fLowPass < 22000.0f)
		xmlNode->setAttr("LowPass", m_fLowPass);

	if (m_nAttributes & FLAG_SOUNDMOOD_HIGHPASS && m_fHighPass > 10.0f)
		xmlNode->setAttr("HighPass", m_fHighPass);

	return bResult;
}

bool CCategory::InterpolateCategories(ICategory* pCat1, float fFade1, ICategory* pCat2, float fFade2,
                                      bool bForceDefault)
{
	if (pCat1 && pCat2)
	{
		// PlatformCategory
		// SetPlatformCategory(pCat1->GetPlatformCategory());

		// both Cats
		bool bProp1 = false;
		bool bProp2 = false;
		float fValue1 = 0.0f;
		float fValue2 = 0.0f;
		ptParamF32 fTemp1(fValue1);
		ptParamF32 fTemp2(fValue2);

		// Volume
		bProp1 = pCat1->GetParam(gspFVOLUME, &fTemp1);
		bProp2 = pCat2->GetParam(gspFVOLUME, &fTemp2);

		fTemp1.GetValue(fValue1);
		fTemp2.GetValue(fValue2);
		float fResult = (fValue1 * fFade1 + fValue2 * fFade2);
		fResult = round_near(fResult, 2);
		fTemp1.SetValue(fResult);
		SetParam(gspFVOLUME, &fTemp1);
		m_nAttributes |= (bForceDefault || (bProp1 || bProp2)) ? FLAG_SOUNDMOOD_VOLUME : 0;

		// Pitch
		bProp1 = pCat1->GetParam(gspFPITCH, &fTemp1);
		bProp2 = pCat2->GetParam(gspFPITCH, &fTemp2);

		fTemp1.GetValue(fValue1);
		fTemp2.GetValue(fValue2);
		fResult = (fValue1 * fFade1 + fValue2 * fFade2);
		fResult = round_near(fResult, 2);
		fTemp1.SetValue(fResult);
		SetParam(gspFPITCH, &fTemp1);
		m_nAttributes |= (bForceDefault || (bProp1 || bProp2)) ? FLAG_SOUNDMOOD_PITCH : 0;

		// Lowpass
		bProp1 = pCat1->GetParam(gspFLOWPASS, &fTemp1);
		bProp2 = pCat2->GetParam(gspFLOWPASS, &fTemp2);

		fTemp1.GetValue(fValue1);
		fTemp2.GetValue(fValue2);
		fResult = (fValue1 * fFade1 + fValue2 * fFade2);
		fResult = round_near(fResult, 2);
		fTemp1.SetValue(fResult);
		SetParam(gspFLOWPASS, &fTemp1);
		m_nAttributes |= (bForceDefault || (bProp1 || bProp2)) ? FLAG_SOUNDMOOD_LOWPASS : 0;

		// Highpass
		bProp1 = pCat1->GetParam(gspFHIGHPASS, &fTemp1);
		bProp2 = pCat2->GetParam(gspFHIGHPASS, &fTemp2);

		fTemp1.GetValue(fValue1);
		fTemp2.GetValue(fValue2);
		fResult = (fValue1 * fFade1 + fValue2 * fFade2);
		fResult = round_near(fResult, 2);
		fTemp1.SetValue(fResult);
		SetParam(gspFHIGHPASS, &fTemp1);
		m_nAttributes |= (bForceDefault || (bProp1 || bProp2)) ? FLAG_SOUNDMOOD_HIGHPASS : 0;
	}

	return true;
}

bool CCategory::Serialize(XmlNodeRef& node, bool bLoading)
{
	bool bResult = true;

	if (bLoading)
	{
		// import from node

		// quick test
		if (strcmp("Category", node->getTag()) != 0)
			return false; // no known Tag

		// parse Properties
		if (!ImportProps(node))
			bResult = false;

		// go through Childs and parse them
		int nGroupChildCount = node->getChildCount();
		for (int i = 0; i < nGroupChildCount; ++i)
		{
			XmlNodeRef GroupChildNode = node->getChild(i);

			// read the name of the child
			const char* sChildName = GroupChildNode->getAttr("Name");
			ICategory* pNewChild = AddCategory(sChildName);

			if (!pNewChild)
				return false; // something wrong !

			bool bChildResult = pNewChild->Serialize(GroupChildNode, true);

			if (!bChildResult)
			{
				RemoveCategory(sChildName);
				bResult = false;
			}
		} // done with all children

		// RefreshCategories
		if (m_pMoodManager)
			m_pMoodManager->m_bNeedUpdate = true;
	}
	else
	{
		node->setTag("Category");
		node->setAttr("Name", GetName());

		// parse Properties
		if (!ExportProps(node))
			bResult = false;
	}

	return true;
}

bool CCategory::RefreshCategories(const bool bAddNew)
{
	// if (m_pPlatformCategory)
	{
		FMOD::EventCategory* pCat = NULL;

		int i = 0;

		if (m_pPlatformCategory)
			m_ExResult = m_pPlatformCategory->getCategoryByIndex(i, &pCat);

		while (pCat)
		{
			int nIndex = 0;
			char* sName = 0;

			m_ExResult = pCat->getInfo(&nIndex, &sName);
			if (m_ExResult == FMOD_OK)
			{
				ICategory* pChild = GetCategory(sName);
				if (!pChild && bAddNew)
				{
					pChild = AddCategory(sName);
				}

				if (pChild)
				{
					pChild->SetParentCategory(this);
					pChild->SetPlatformCategory(pCat);
					pChild->RefreshCategories(bAddNew);
				}
			}
			else
			{
				int a = 0; // very bad
			}

			++i;
			// get next category
			pCat = NULL;
			m_ExResult = m_pPlatformCategory->getCategoryByIndex(i, &pCat);
		}
		return true;
	}

	return false;
}

bool CCategory::ApplyCategory(float fFade)
{

	if (m_pPlatformCategory)
	{
		float fNewValue = 0.0f;
		float fOldValue = 0.0f;
		ptParamF32 fTemp(fNewValue);

		if (m_nAttributes & FLAG_SOUNDMOOD_VOLUME)
		{
			fNewValue = m_fVolume;
			m_pPlatformCategory->getVolume(&fOldValue);

			if (m_fMaxCategoryVolume > 0.0f)
				fOldValue = fOldValue / m_fMaxCategoryVolume;

			fNewValue = (fOldValue * (1.0f - fFade) + fNewValue * fFade) * m_fMaxCategoryVolume;

			m_pPlatformCategory->setVolume(fNewValue);
		}

		if (m_nAttributes & FLAG_SOUNDMOOD_PITCH)
		{
			fNewValue = m_fPitch;
			m_pPlatformCategory->getPitch(&fOldValue);
			m_pPlatformCategory->setPitch(fOldValue * (1.0f - fFade) + fNewValue * fFade);
		}

		if (m_nAttributes & FLAG_SOUNDMOOD_LOWPASS)
		{
			// See if others have it
			if (!m_pDSPLowPass)
				GetDSPFromAll(FLAG_SOUNDMOOD_LOWPASS);

			// create DSP
			if (!m_pDSPLowPass && (m_nAttributes & FLAG_SOUNDMOOD_LOWPASS))
			{
				FMOD::System* pFMODSystem = NULL;

				if (gEnv->pSoundSystem && ((CSoundSystem*)gEnv->pSoundSystem)->g_nSoundMoodsDSP)
					pFMODSystem =
					    (FMOD::System*)gEnv->pSoundSystem->GetIAudioDevice()->GetSoundLibrary();

				if (pFMODSystem && (m_nAttributes & FLAG_SOUNDMOOD_LOWPASS))
				{
					m_ExResult =
					    pFMODSystem->createDSPByType(FMOD_DSP_TYPE_LOWPASS_SIMPLE, &m_pDSPLowPass);
					if (m_ExResult == FMOD_OK)
					{
						FMOD::ChannelGroup* pCGroup;
						m_ExResult = m_pPlatformCategory->getChannelGroup(&pCGroup);
						if (m_ExResult == FMOD_OK)
						{
							m_ExResult = pCGroup->addDSP(m_pDSPLowPass, 0);
						}
					}
					SetDSPForAll(FLAG_SOUNDMOOD_LOWPASS, m_pDSPLowPass);
				}
			}

			fNewValue = m_fLowPass;

			if (m_pDSPLowPass)
			{
				m_ExResult =
				    m_pDSPLowPass->getParameter(FMOD_DSP_LOWPASS_SIMPLE_CUTOFF, &fOldValue, 0, 0);
				// fOldValue = 1 - (fOldValue-10)/21990;
				// fOldValue = (fOldValue*(1.0f-fFade) + fNewValue*fFade);
				// fOldValue = (1 - fOldValue)*21990 + 10;
				fOldValue = (fOldValue * (1.0f - fFade) + fNewValue * fFade);
				m_ExResult = m_pDSPLowPass->setParameter(FMOD_DSP_LOWPASS_SIMPLE_CUTOFF, fOldValue);

				if (fOldValue == 22000.0f)
					m_ExResult = m_pDSPLowPass->setBypass(true);
				else
					m_ExResult = m_pDSPLowPass->setBypass(false);
			}
		}

		if (m_nAttributes & FLAG_SOUNDMOOD_HIGHPASS)
		{
			// See if others have it
			if (!m_pDSPHighPass)
				GetDSPFromAll(FLAG_SOUNDMOOD_HIGHPASS);

			// create DSP
			if (!m_pDSPHighPass && (m_nAttributes & FLAG_SOUNDMOOD_HIGHPASS))
			{
				FMOD::System* pFMODSystem = NULL;

				if (gEnv->pSoundSystem && ((CSoundSystem*)gEnv->pSoundSystem)->g_nSoundMoodsDSP)
					pFMODSystem =
					    (FMOD::System*)gEnv->pSoundSystem->GetIAudioDevice()->GetSoundLibrary();

				if (pFMODSystem && (m_nAttributes & FLAG_SOUNDMOOD_HIGHPASS))
				{
					m_ExResult =
					    pFMODSystem->createDSPByType(FMOD_DSP_TYPE_HIGHPASS, &m_pDSPHighPass);
					if (m_ExResult == FMOD_OK)
					{
						FMOD::ChannelGroup* pCGroup;
						m_ExResult = m_pPlatformCategory->getChannelGroup(&pCGroup);
						if (m_ExResult == FMOD_OK)
						{
							m_ExResult = pCGroup->addDSP(m_pDSPHighPass, 0);
						}
					}
					SetDSPForAll(FLAG_SOUNDMOOD_HIGHPASS, m_pDSPHighPass);
				}
			}

			fNewValue = m_fHighPass;

			if (m_pDSPHighPass)
			{
				m_ExResult = m_pDSPHighPass->getParameter(FMOD_DSP_HIGHPASS_CUTOFF, &fOldValue, 0, 0);
				// fOldValue = 1 - (fOldValue-10)/21990;
				// fOldValue = (fOldValue*(1.0f-fFade) + fNewValue*fFade);
				// fOldValue = 21990 - (1 - fOldValue)*21990 + 10;
				fOldValue = (fOldValue * (1.0f - fFade) + fNewValue * fFade);
				m_ExResult = m_pDSPHighPass->setParameter(FMOD_DSP_HIGHPASS_CUTOFF, fOldValue);

				if (fOldValue == 10.0f)
				{
					// m_ExResult = m_pDSPHighPass->remove();
					m_ExResult = m_pDSPHighPass->setBypass(true);
					// m_pDSPHighPass = NULL;
				}
				else
					m_ExResult = m_pDSPHighPass->setBypass(false);

				// m_ExResult = m_pDSPHighPass->setBypass(true);
			}
		}
	}

	return true;
}

// writes output to screen in debug
void CCategory::DrawInformation(IRenderer* pRenderer, float& xpos, float& ypos, bool bAppliedValues)
{
	assert(pRenderer);

	float fColor[4] = {1.0f, 1.0f, 1.0f, 0.7f};
	float fColorRed[4] = {1.0f, 0.0f, 0.0f, 0.7f};
	float* pColor = fColor;

	char sText[512];

	sprintf(sText, "%s", GetName());

	if (m_nAttributes & FLAG_SOUNDMOOD_VOLUME || bAppliedValues)
	{
		float fVolume = m_fVolume;
		size_t idLen = strlen(sText);

		if (bAppliedValues)
		{
			if (m_pPlatformCategory)
			{
				m_ExResult = m_pPlatformCategory->getVolume(&fVolume);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("category get volume failed! ", ILog::eMessage);
				}
			}
		}

		sprintf(sText + idLen, ", Vol=%1.2f", fVolume);

		if (m_fVolume != fVolume)
			pColor = fColorRed;
	}

	if (m_nAttributes & FLAG_SOUNDMOOD_PITCH || bAppliedValues)
	{
		float fPitch = m_fPitch;
		size_t idLen = strlen(sText);

		if (bAppliedValues)
		{
			if (m_pPlatformCategory)
			{
				m_ExResult = m_pPlatformCategory->getPitch(&fPitch);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("category get pitch failed! ", ILog::eMessage);
				}
			}
		}

		sprintf(sText + idLen, ", Pitch=%1.2f", fPitch);

		if (m_fPitch != fPitch)
			pColor = fColorRed;
	}

	if (m_nAttributes & FLAG_SOUNDMOOD_LOWPASS || bAppliedValues)
	{
		float fLowPass = m_fLowPass;
		size_t idLen = strlen(sText);

		if (bAppliedValues)
		{
			if (m_pDSPLowPass)
			{
				m_ExResult = m_pDSPLowPass->setParameter(FMOD_DSP_LOWPASS_SIMPLE_CUTOFF, fLowPass);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("category get lowpass cutoff failed! ", ILog::eMessage);
				}
			}
		}

		sprintf(sText + idLen, ", LowPass=%1.2f", fLowPass);

		if (m_fLowPass != fLowPass)
			pColor = fColorRed;
	}

	if (m_nAttributes & FLAG_SOUNDMOOD_HIGHPASS || bAppliedValues)
	{
		float fHighPass = m_fHighPass;
		size_t idLen = strlen(sText);

		if (bAppliedValues)
		{
			if (m_pDSPHighPass)
			{
				m_ExResult = m_pDSPHighPass->setParameter(FMOD_DSP_HIGHPASS_CUTOFF, fHighPass);
				if (m_ExResult != FMOD_OK)
				{
					FmodErrorOutput("category get highpass cutoff failed! ", ILog::eMessage);
				}
			}
		}

		sprintf(sText + idLen, ", HighPass=%1.2f", fHighPass);

		if (m_fHighPass != fHighPass)
			pColor = fColorRed;
	}

	if (!m_pPlatformCategory)
		pColor = fColorRed;

	pRenderer->Draw2dLabel(xpos, ypos, 1, pColor, false, "%s", sText);

	ypos += 10;
	xpos += 10;

	// go through Children
	FMOD::EventCategory* pCat = NULL;

	int i = 0;

	if (m_pPlatformCategory)
	{
		while (m_pPlatformCategory->getCategoryByIndex(i, &pCat) == FMOD_OK)
		{
			{
				ICategory* pChild = GetCategoryByPtr((void*)pCat);

				if (pChild)
					pChild->DrawInformation(pRenderer, xpos, ypos, bAppliedValues);
			}

			++i;
			pCat = NULL;
		}
	}
	xpos -= 10;
}

void* CCategory::GetDSP(const int nDSPType)
{
	switch (nDSPType)
	{
		case FLAG_SOUNDMOOD_LOWPASS:
			return m_pDSPLowPass;
			break;
		case FLAG_SOUNDMOOD_HIGHPASS:
			return m_pDSPHighPass;
			break;
		default:
			break;
	}

	return NULL;
}

void CCategory::SetDSP(const int nDSPType, void* pDSP)
{
	switch (nDSPType)
	{
		case FLAG_SOUNDMOOD_LOWPASS:
			m_pDSPLowPass = (FMOD::DSP*)pDSP;
			break;
		case FLAG_SOUNDMOOD_HIGHPASS:
			m_pDSPHighPass = (FMOD::DSP*)pDSP;
			break;
		default:
			break;
	}
}

void CCategory::GetDSPFromAll(const int nDSPType)
{
	int nCount = 0;
	IMood* pMood = m_pMoodManager->GetMoodPtr(nCount);
	void* pDSP = NULL;

	while (pMood)
	{
		CCategory* pCat = (CCategory*)pMood->GetCategory(GetName());

		if (pCat)
			pDSP = pCat->GetDSP(nDSPType);

		if (pDSP)
		{
			SetDSP(nDSPType, pDSP);
			break;
		}

		++nCount;
		pMood = m_pMoodManager->GetMoodPtr(nCount);
	}
}

void CCategory::SetDSPForAll(const int nDSPType, void* pDSP)
{
	int nCount = 0;
	IMood* pMood = m_pMoodManager->GetMoodPtr(nCount);

	while (pMood)
	{
		CCategory* pCat = (CCategory*)pMood->GetCategory(GetName());

		if (pCat)
			pCat->SetDSP(nDSPType, pDSP);

		++nCount;
		pMood = m_pMoodManager->GetMoodPtr(nCount);
	}
}

//////////////////////////////////////////////////////////////////////////
// CMood Definition
//////////////////////////////////////////////////////////////////////////

CMood::CMood(CMoodManager* pMoodManager)
{
	m_pMoodManager = pMoodManager;
	m_vecCategories.clear();
	m_fPriority = 1.0f;
	m_fMusicVolume = 0.8f;
	m_bIsMixMood = false;

	m_ExResult = FMOD_OK;
}

CMood::~CMood()
{
	m_vecCategories.clear();
}

const char* CMood::GetName() const
{
	return m_sMoodName.c_str();
}

ICategory* CMood::GetMasterCategory() const
{
	ICategory* pCat = GetCategory("master");

	return pCat;
}

ICategory* CMood::AddCategory(const char* sCategoryName)
{
	ICategory* pNewChild = GetCategory(sCategoryName);

	if (!pNewChild)
	{
		bool bNewEntry = true;

		// fill up holes in the array first
		uint32 i = 0;
		for (i = 0; i < m_vecCategories.size(); ++i)
			if (m_vecCategories[i] == NULL)
			{
				bNewEntry = false;
				break;
			}

		pNewChild = (ICategory*)new CCategory(m_pMoodManager, NULL, this);

		if (!pNewChild)
		{
			// delete pNewChild;
			return 0;
		}

		((CCategory*)pNewChild)->SetName(sCategoryName);

		if (bNewEntry)
			m_vecCategories.push_back(pNewChild);
		else
			m_vecCategories[i] = pNewChild;

		m_pMoodManager->UpdateMemUsage(sizeof(CCategory));
	}

	return pNewChild;
}

bool CMood::RemoveCategory(const char* sCategoryName)
{
	bool bResult = false;

	uint32 i = 0;
	for (i = 0; i < m_vecCategories.size(); ++i)
	{
		ICategory* pCat = m_vecCategories[i];
		if (pCat)
		{
			if (strcmp(sCategoryName, pCat->GetName()) == 0)
			{
				m_vecCategories[i] = NULL;
				bResult = true;
			}
		}
	}

	return bResult;
}

uint32 CMood::GetCategoryCount() const
{
	int nNum = m_vecCategories.size();

	return nNum;
}

ICategory* CMood::GetCategoryByIndex(uint32 nCategoryCount) const
{
	ICategory* pElement = NULL;

	if (m_vecCategories.size() > nCategoryCount)
		pElement = m_vecCategories[nCategoryCount];

	return pElement;
}

ICategory* CMood::GetCategory(const char* sCategoryName) const
{
	ICategory* pElement = NULL;

	tvecCategoryIterConst IterEnd = m_vecCategories.end();
	for (tvecCategoryIterConst Iter = m_vecCategories.begin(); Iter != IterEnd; ++Iter)
	{
		ICategory* pCat = (*Iter);
		if (pCat)
		{
			if (strcmp(sCategoryName, pCat->GetName()) == 0)
			{
				pElement = pCat;
				break;
			}
		}
	}

	return pElement;
}

ICategory* CMood::GetCategoryByPtr(void* pPlatformCategory) const
{
	ICategory* pElement = NULL;

	if (pPlatformCategory)
	{
		tvecCategoryIterConst ItEnd = m_vecCategories.end();
		for (tvecCategoryIterConst It = m_vecCategories.begin(); It != ItEnd; ++It)
		{
			ICategory* pCat = (*It);
			if (pCat->GetPlatformCategory() == pPlatformCategory)
			{
				pElement = pCat;
				break;
			}
		}
	}

	return pElement;
}

bool CMood::Serialize(XmlNodeRef& node, bool bLoading)
{
	bool bResult = true;

	if (bLoading)
	{
		// import from node
		if (strcmp("Mood", node->getTag()) != 0)
			return false; // no known Tag

		node->getAttr("Priority", m_fPriority);
		node->getAttr("MusicVolume", m_fMusicVolume);

		// go through Childs and parse them
		int nGroupChildCount = node->getChildCount();
		for (int i = 0; i < nGroupChildCount; ++i)
		{
			XmlNodeRef GroupChildNode = node->getChild(i);

			// read the name of the child
			const char* sChildName = GroupChildNode->getAttr("Name");
			ICategory* pNewChild = AddCategory(sChildName);

			if (!pNewChild)
				return false; // something wrong !

			bool bChildResult = pNewChild->Serialize(GroupChildNode, true);

			if (!bChildResult)
			{
				RemoveCategory(sChildName);
				bResult = false;
			}
		} // done with all children
	}
	else
	{

		node->setTag("Mood");
		node->setAttr("Name", GetName());
		node->setAttr("Priority", m_fPriority);
		node->setAttr("MusicVolume", m_fMusicVolume);

		// sorting Category names
		std::vector<string> CategoryNames;

		tvecCategoryIter IterEnd = m_vecCategories.end();
		for (tvecCategoryIter Iter = m_vecCategories.begin(); Iter != IterEnd; ++Iter)
		{
			ICategory* pCat = (*Iter);
			if (pCat)
				CategoryNames.push_back(pCat->GetName());
		}

		// CategoryNames.sort();
		std::sort(CategoryNames.begin(), CategoryNames.end());

		int nSize = CategoryNames.size();
		for (int i = 0; i < nSize; ++i)
		{
			ICategory* pCat = GetCategory(CategoryNames[i].c_str());
			if (pCat)
			{
				XmlNodeRef ChildNode = node->newChild("Export");
				bool bChildResult = pCat->Serialize(ChildNode, false);

				if (bChildResult)
				{
					// node->addChild(ChildNode);
				}
				else
				{
					node->removeChild(ChildNode);
					bResult = false;
				}
			} // done with all children
		}
	}

	return bResult;
}

bool CMood::InterpolateMoods(const IMood* pMood1, float fFade1, const IMood* pMood2, float fFade2, bool bForceDefault)
{

	// priority
	float fPriorityRatio = pMood1->GetPriority() * fFade1 + pMood2->GetPriority() * fFade2;
	float fNewFade1 = (pMood1->GetPriority() * fFade1) / fPriorityRatio;
	float fNewFade2 = 1.0f - fNewFade1;

	m_fPriority = (fPriorityRatio / 2.0f);

	int nNum1 = pMood1->GetCategoryCount();
	int nNum2 = pMood1->GetCategoryCount();

	assert(nNum1 == nNum2);

	int nNum3 = GetCategoryCount();

	if (nNum3 < nNum1)
	{
		// insert all categories from Mood1 without hierarchy
		for (uint32 i = 0; i < pMood1->GetCategoryCount(); ++i)
		{
			ICategory* pCat1 = pMood1->GetCategoryByIndex(i);
			ICategory* pNewCat = NULL;

			if (pCat1)
			{
				pNewCat = GetCategoryByPtr((void*)pCat1->GetPlatformCategory());

				if (!pNewCat)
					pNewCat = AddCategory(pCat1->GetName());

				if (pNewCat && !pNewCat->GetPlatformCategory())
					pNewCat->SetPlatformCategory(pCat1->GetPlatformCategory());
			}
		}
	}

	// interpolate MusicVolume
	float fMusic1 = pMood1->GetMusicVolume();
	float fMusic2 = pMood2->GetMusicVolume();
	float fNewMusic = fMusic1 * fNewFade1 + fMusic2 * fNewFade2;
	m_fMusicVolume = fNewMusic;

	// check if both Moods have the same num of categories
	assert(pMood1->GetCategoryCount() == pMood2->GetCategoryCount());
	if (CSound::m_pSSys && CSound::m_pSSys->g_nDebugSound == SOUNDSYSTEM_DEBUG_DETAIL &&
	    pMood1->GetCategoryCount() != pMood2->GetCategoryCount())
	{
		gEnv->pLog->Log("<Sound> incompatible SoundMoods  %s and %s\n", pMood1->GetName(), pMood2->GetName());
	}

	nNum3 = GetCategoryCount();

	assert(nNum1 == nNum3);

	// go through all Categories
	for (uint32 i = 0; i < GetCategoryCount(); ++i)
	{
		ICategory* pCat = GetCategoryByIndex(i);
		ICategory* pCat1 = pMood1->GetCategoryByIndex(i);
		ICategory* pCat2 = pMood2->GetCategoryByIndex(i);
		pCat->InterpolateCategories(pCat1, fNewFade1, pCat2, fNewFade2, bForceDefault);
	}

	return true;
}

bool CMood::ApplyMood(float fFade)
{
	FMOD::EventSystem* pEventSystem = NULL;

	if (gEnv->pSoundSystem)
		pEventSystem = (FMOD::EventSystem*)gEnv->pSoundSystem->GetIAudioDevice()->GetEventSystem();

	if (!pEventSystem)
		return false;

	for (uint32 i = 0; i < GetCategoryCount(); ++i)
	{
		ICategory* pCat = GetCategoryByIndex(i);

		if (pCat)
			pCat->ApplyCategory(fFade);
		else
			int verybad = 1;
	}

	if (gEnv->pSoundSystem)
		gEnv->pSoundSystem->SetMusicEffectsVolume(m_fMusicVolume);

	return true;
}

// writes output to screen in debug
void CMood::DrawInformation(IRenderer* pRenderer, float& xpos, float& ypos, bool bAppliedValues)
{
	assert(pRenderer);

	float fColor[4] = {1.0f, 1.0f, 1.0f, 0.7f};

	pRenderer->Draw2dLabel(xpos, ypos, 1, fColor, false, "<%s>", GetName());
	ypos += 10;
	xpos += 10;

	ICategory* pCategory = GetCategory("master");

	if (pCategory)
		pCategory->DrawInformation(pRenderer, xpos, ypos, bAppliedValues);

	xpos -= 10;
}

//////////////////////////////////////////////////////////////////////////
// CMoodManager Definition
//////////////////////////////////////////////////////////////////////////

CMoodManager::CMoodManager()
{
	m_ExResult = FMOD_OK;

	m_nTotalSoundsListed = 0;
	m_nTotalMemUsedByManager = sizeof(CMoodManager);

	m_tLastUpdate = gEnv->pTimer->GetFrameStartTime();

	m_MoodArray.clear();
	m_pFinalMix = NULL;
	// m_mapHandleNames.clear();

	Load(SOUNDMOOD_FILENAME);
	// RefreshCategories();
	m_bNeedUpdate = true;
	m_bForceFinalMix = false;

	// make sure to create the "default" mood
	Reset();
}

CMoodManager::~CMoodManager()
{
	// TODO free all that memory
	m_MoodArray.clear();
	// m_mapHandleNames.clear();
}

bool CMoodManager::RefreshCategories(const char* sMoodName)
{
	FMOD::EventSystem* pEventSystem = NULL;

	if (gEnv->pSoundSystem)
		pEventSystem = (FMOD::EventSystem*)gEnv->pSoundSystem->GetIAudioDevice()->GetEventSystem();

	IMood* pMood = AddMood("default");

	// maybe a category was selected
	pMood = GetMoodPtr(sMoodName);
	if (!pMood)
		return false;

	pMood = AddMood(sMoodName);

	// reset all Categories of that mood
	int nNum = pMood->GetCategoryCount();
	for (int i = 0; i < nNum; ++i)
	{
		ICategory* pCat = pMood->GetCategoryByIndex(i);
		if (pCat)
			pCat->SetPlatformCategory(NULL);
	}

	if (!pEventSystem)
		return false;

	FMOD::EventCategory* pPlatformCategory;
	m_ExResult = pEventSystem->getCategory("master", &pPlatformCategory);

	ICategory* pCategory = pMood->AddCategory("master");
	pCategory->SetPlatformCategory(pPlatformCategory ? pPlatformCategory : NULL);
	pCategory->RefreshCategories(true);

	return true;
}

bool CMoodManager::RefreshPlatformCategories()
{
	FMOD::EventSystem* pEventSystem = NULL;

	if (gEnv->pSoundSystem)
		pEventSystem = (FMOD::EventSystem*)gEnv->pSoundSystem->GetIAudioDevice()->GetEventSystem();

	if (!pEventSystem)
		return false;

	FMOD::EventCategory* pPlatformCategory;

	// update platform category handle
	tvecActiveMoodsIter IterEnd = m_ActiveMoods.end();
	for (tvecActiveMoodsIter It = m_ActiveMoods.begin(); It != IterEnd; ++It)
	{
		IMood* pMood = GetMoodPtr((*It).sName.c_str());
		if (pMood)
		{
			// uint32 nNum = pMood->GetCategoryCount();
			// for (uint32 i=0; i<nNum; ++i)
			{
				// ICategory *pCategory = pMood->GetCategoryByIndex(i);
				ICategory* pCategory = pMood->GetMasterCategory();
				if (pCategory)
				{
					m_ExResult =
					    pEventSystem->getCategory(pCategory->GetName(), &pPlatformCategory);

					if (m_ExResult != FMOD_OK)
						break;

					pCategory->SetPlatformCategory(pPlatformCategory);
					pCategory->RefreshCategories(false);
				}
				else
				{
					int a = 0;
				}
			}
		}
	}

	return true;
}

void CMoodManager::Release()
{
	delete this;
}

void CMoodManager::Reset()
{
	if (!m_pFinalMix)
	{
		m_pFinalMix = AddMood("final");
		m_pFinalMix->SetIsMixMood(true);
	}

	assert(m_pFinalMix);

	// set the mixing mood back to default values
	m_pFinalMix->SetName("final");
	IMood* pDefault = GetMoodPtr("default");

	if (pDefault)
	{
		m_pFinalMix->InterpolateMoods(pDefault, 1.0f, pDefault, 1.0f, true);
		m_pFinalMix->ApplyMood(1.0f);
		m_pFinalMix->InterpolateMoods(pDefault, 1.0f, pDefault, 1.0f, false);
	}

	// remove all active moods
	m_ActiveMoods.clear();

	// re-enter the default mood
	RegisterSoundMood("default");
	UpdateSoundMood("default", 1.0f, 0);
}

//////////////////////////////////////////////////////////////////////////
// Management
//////////////////////////////////////////////////////////////////////////

IMood* CMoodManager::AddMood(const char* sMoodName)
{
	IMood* pNewMood = GetMoodPtr(sMoodName);

	// add a new Mood if there is not already one with that name
	if (!pNewMood)
	{
		bool bNewEntry = true;

		// fill up holes in the array first
		uint32 i = 0;
		for (i = 0; i < m_MoodArray.size(); ++i)
		{
			if (m_MoodArray[i] == NULL)
			{
				bNewEntry = false;
				break;
			}
		}

		pNewMood = (IMood*)new CMood(this);

		// problem with new()
		if (!pNewMood)
			return (IMood*)pNewMood;

		pNewMood->SetName(sMoodName);

		if (bNewEntry)
			m_MoodArray.push_back(pNewMood);
		else
			m_MoodArray[i] = pNewMood;

		UpdateMemUsage(sizeof(CCategory));
	}

	return pNewMood;
}

bool CMoodManager::RemoveMood(const char* sMoodName)
{
	bool bFound = false;

	for (uint32 i = 0; i < m_MoodArray.size(); ++i)
	{
		IMood* pMood = m_MoodArray[i];

		if (pMood)
			if (strcmp(pMood->GetName(), sMoodName) == 0)
			{
				m_MoodArray[i] = NULL;
				bFound = true;
				break;
			}
	}

	return (bFound);
}

IMood* CMoodManager::GetMoodPtr(const uint32 nGroupCount) const
{
	IMood* pMood = NULL;

	// if (nGroupCount < 0 || nGroupCount > m_MoodArray.size())
	// return pMood;

	uint32 nCount = 0;
	for (uint32 i = 0; i < m_MoodArray.size(); ++i)
	{
		if (m_MoodArray[i] != NULL)
			++nCount;
		if (nCount == nGroupCount + 1)
		{
			pMood = m_MoodArray[i];
			break;
		}
	}

	return pMood;
}

IMood* CMoodManager::GetMoodPtr(const char* sMoodName) const
{
	IMood* pMood = NULL;

	int i = 0;
	pMood = GetMoodPtr(i);

	while (pMood)
	{
		if (strcmp(pMood->GetName(), sMoodName) == 0)
		{
			break;
		}
		++i;
		pMood = GetMoodPtr(i);
	}
	return pMood;
}

//////////////////////////////////////////////////////////////////////////
// Import/Export
//////////////////////////////////////////////////////////////////////////

unsigned int CMoodManager::GetNumberSoundsLoaded() const
{
	return m_nTotalSoundsListed;
}

//////////////////////////////////////////////////////////////////////////
// Real Time Manipulation
//////////////////////////////////////////////////////////////////////////

// needs to be updated regularly
bool CMoodManager::Update()
{
	if (gEnv->pSoundSystem)
	{
		CSoundSystem* pSoundSystem = (CSoundSystem*)gEnv->pSoundSystem;
		if (!pSoundSystem->g_nSoundMoods)
			return false;
	}

	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SOUND);

	// update timer
	CTimeValue tTimeDif = gEnv->pTimer->GetFrameStartTime() - m_tLastUpdate;
	float fMS = abs(tTimeDif.GetMilliSeconds());

	if (fMS == 0.0f)
		return false; // cannot do anything if last update was same frame;

	std::vector<tvecActiveMoodsIter> ToBeRemoved;

	// update time based fading
	tvecActiveMoodsIter IterEnd = m_ActiveMoods.end();
	for (tvecActiveMoodsIter It = m_ActiveMoods.begin(); It != IterEnd; ++It)
	{

		if ((*It).fFadeTarget != (*It).fFadeCurrent && (*It).nFadeTimeInMS > 0)
		{
			float fFadeDiff =
			    (((*It).fFadeTarget - (*It).fFadeCurrent) * min(fMS, (float)(*It).nFadeTimeInMS)) /
			    (float)(*It).nFadeTimeInMS;
			(*It).fFadeCurrent += fFadeDiff;
			(*It).fFadeCurrent = ((*It).fFadeCurrent > 0.00001f) ? (*It).fFadeCurrent : 0.0f;
			fFadeDiff = (fFadeDiff > 0.00001f) ? fFadeDiff : 0.0f;

			(*It).fFadeCurrent = max(0.0f, (*It).fFadeCurrent);
			(*It).fFadeCurrent = min(1.0f, (*It).fFadeCurrent);
			(*It).nFadeTimeInMS -= (int)fMS;
			(*It).nFadeTimeInMS = max(0, (*It).nFadeTimeInMS);
			m_bNeedUpdate = true;

			if ((*It).fFadeCurrent == 0 && (*It).bUnregisterOnFadeOut)
			{
				(*It).bReadyToRemove = true;
				ToBeRemoved.push_back(It);
			}
		}
		else
		{
			if (((*It).fFadeCurrent == 0 || (*It).fFadeTarget == 0) && (*It).bUnregisterOnFadeOut)
				ToBeRemoved.push_back(It);
		}
	}

	if (!m_bNeedUpdate)
	{
		m_tLastUpdate = gEnv->pTimer->GetFrameStartTime();
		return false;
	}

	if (!m_pFinalMix)
	{
		m_pFinalMix = AddMood("final");
		m_pFinalMix->SetIsMixMood(true);
	}
	assert(m_pFinalMix);

	m_bNeedUpdate = false;

	bool Checkresults = true;
	float SumOfWeight = 0;

	// if there is only one Mood just interpolate directly with its weight
	if (m_ActiveMoods.size() == 1)
	{
		tvecActiveMoodsIter It = m_ActiveMoods.begin();

		IMood* pSingleMood = GetMoodPtr((*It).sName.c_str());

		if (pSingleMood)
		{
			m_pFinalMix->InterpolateMoods(pSingleMood, 1.0f, pSingleMood, 1.0f, false);

			m_pFinalMix->SetName((*It).sName.c_str());
		}

		SumOfWeight = (*It).fFadeTarget;
	}
	else
	{
		float fMixFade = 0.0f;
		int i = 0;

		tvecActiveMoodsIter ItEnd = m_ActiveMoods.end();
		for (tvecActiveMoodsIter It = m_ActiveMoods.begin(); It != ItEnd; ++It)
		{
			// Copy first element to the temp preset
			if (It == m_ActiveMoods.begin())
			{
				IMood* pFirstMood = GetMoodPtr((*It).sName.c_str());
				fMixFade = (*It).fFadeTarget;

				if (pFirstMood)
				{
					m_pFinalMix->InterpolateMoods(pFirstMood, 1.0f, pFirstMood, 1.0f, false);

					m_pFinalMix->SetName((*It).sName.c_str());
				}

				++i;
			}
			else // start interpolating at the 2nd element
			{
				IMood* pNextMood = GetMoodPtr((*It).sName.c_str());

				if (!pNextMood)
					continue;

				CryFixedStringT<256> sResultMood;
				sResultMood += m_pFinalMix->GetName();
				sResultMood += "-";
				sResultMood += pNextMood->GetName();
				m_pFinalMix->SetName(sResultMood.c_str());

				if (i == 2)
					int j = 0;

				++i;

				if ((*It).bReadyToRemove)
					m_bForceFinalMix = true;
				else
					m_bForceFinalMix = false;

				m_pFinalMix->InterpolateMoods(m_pFinalMix, fMixFade, pNextMood, (*It).fFadeCurrent,
				                              m_bForceFinalMix);
			}
		}
	}

	assert(m_pFinalMix);

	m_pFinalMix->ApplyMood(1.0f);

	// Unregister Moods with current fade == 0;
	std::vector<tvecActiveMoodsIter>::const_reverse_iterator IEnd = ToBeRemoved.rend();
	for (std::vector<tvecActiveMoodsIter>::const_reverse_iterator I = ToBeRemoved.rbegin(); I != IEnd; ++I)
	{
		m_ActiveMoods.erase((*I));
		m_bNeedUpdate = true;
	}

	ToBeRemoved.clear();

	// update timer
	m_tLastUpdate = gEnv->pTimer->GetFrameStartTime();

	return true;
}

bool CMoodManager::InterpolateMoods(const IMood* pMood1, const float fFade1, const IMood* pMood2, const float fFade2,
                                    IMood* pResultMood, float fResultFade)
{
	CryFixedStringT<256> sResultMood;
	sResultMood += pMood1->GetName();
	sResultMood += pMood2->GetName();

	if (!m_pFinalMix)
	{
		m_pFinalMix = AddMood(sResultMood.c_str());
	}
	else
	{
		m_pFinalMix->SetName(sResultMood.c_str());
	}

	if (m_pFinalMix)
		m_pFinalMix->InterpolateMoods(pMood1, fFade1, pMood2, fFade2, false);

	return true;
}

// registers a Mood to be actively processes (starts with 0 fade value)
bool CMoodManager::RegisterSoundMood(const char* sMoodName)
{
	if (gEnv->pSoundSystem)
	{
		CSoundSystem* pSoundSystem = (CSoundSystem*)gEnv->pSoundSystem;
		if (!pSoundSystem->g_nSoundMoods)
			return false;
	}

	assert(sMoodName[0]);
	if (sMoodName[0])
	{
		// verify if this Mood is available in the library
		IMood* pMood = GetMoodPtr(sMoodName);

		if (!pMood)
			return false;

		// add this Preset to the vector of active Presets or update it if its already in there
		tvecActiveMoodsIterConst IterEnd = m_ActiveMoods.end();
		for (tvecActiveMoodsIterConst It = m_ActiveMoods.begin(); It != IterEnd; ++It)
		{
			if ((*It).sName == sMoodName)
			{
				// cant register a 2nd time
				return false;
				break;
			}
		}

		SSoundMoodProp MP;
		MP.sName = sMoodName;
		MP.fFadeCurrent = 0.0f;
		MP.fFadeTarget = 0.0f;
		MP.nFadeTimeInMS = 0;
		MP.bUnregisterOnFadeOut = false;
		MP.bForceUpdate = false;
		MP.bReadyToRemove = false;

		m_ActiveMoods.push_back(MP);
		m_bNeedUpdate = true;

		return true;
	}

	return false;
}

// updates the fade value of a registered mood
bool CMoodManager::UpdateSoundMood(const char* sMoodName, float fFade, uint32 nFadeTimeInMS, bool bUnregistedOnFadeOut)
{
	assert(sMoodName[0]);

	// update this Mood to the vector of active Moodss or add if its not in there yet
	bool bMoodFound = false;

	tvecActiveMoodsIter IterEnd = m_ActiveMoods.end();
	for (tvecActiveMoodsIter It = m_ActiveMoods.begin(); It != IterEnd; ++It)
	{
		if ((*It).sName == sMoodName)
		{
			if (fFade < 0)
			{
				bool bStillUpdating = ((*It).fFadeTarget != (*It).fFadeCurrent);
				return bStillUpdating;
			}

			assert(fFade >= 0 && fFade <= 1);

			if (nFadeTimeInMS == 0)
				(*It).fFadeCurrent = fFade;

			(*It).fFadeTarget = fFade;
			(*It).nFadeTimeInMS = nFadeTimeInMS;
			(*It).bUnregisterOnFadeOut = bUnregistedOnFadeOut;

			bMoodFound = true;
			break;
		}
	}

	if (!bMoodFound)
		return false;

	m_bNeedUpdate = true;
	return true;
}

// get current fade value of a registered mood
float CMoodManager::GetSoundMoodFade(const char* sMoodName)
{
	bool bMoodFound = false;

	tvecActiveMoodsIter IterEnd = m_ActiveMoods.end();
	for (tvecActiveMoodsIter It = m_ActiveMoods.begin(); It != IterEnd; ++It)
	{
		if ((*It).sName == sMoodName)
		{
			return (*It).fFadeCurrent;
		}
	}
	return -1;
}

// unregisters a Mood and removes it from the active ones
bool CMoodManager::UnregisterSoundMood(const char* sMoodName)
{
	assert(sMoodName[0]);
	bool bMoodFound = false;

	tvecActiveMoodsIter IterEnd = m_ActiveMoods.end();
	for (tvecActiveMoodsIter It = m_ActiveMoods.begin(); It != IterEnd; ++It)
	{
		if ((*It).sName == sMoodName)
		{
			// set it to 0 to disable all set effects first
			(*It).fFadeCurrent = -0.1f;
			(*It).fFadeTarget = 0.0f;
			(*It).nFadeTimeInMS = 1; // must be positive to trigger correct removal
			(*It).bUnregisterOnFadeOut = true;
			m_bNeedUpdate = true;
			Update();
			bMoodFound = true;
			break;
		}
	}

	if (!bMoodFound)
		return false;

	m_bNeedUpdate = true;
	return true;
}

// writes output to screen in debug
void CMoodManager::DrawInformation(IRenderer* pRenderer, float& xpos, float& ypos, int nSoundInfo)
{
	assert(pRenderer);

	float fColor[4] = {1.0f, 1.0f, 1.0f, 0.7f};

	pRenderer->Draw2dLabel(xpos, ypos, 1, fColor, false, "Sound Moods: %d", m_ActiveMoods.size());
	ypos += 10;

	int nActiveMoodsNum = m_ActiveMoods.size();

	tvecActiveMoodsIter IterEnd = m_ActiveMoods.end();
	for (tvecActiveMoodsIter It = m_ActiveMoods.begin(); It != IterEnd; ++It)
	{
		float fActiveColor[4] = {0.2f, (*It).fFadeCurrent, 0.2f, 0.7f};
		IMood* pMood = GetMoodPtr((*It).sName.c_str());

		pRenderer->Draw2dLabel(xpos, ypos, 1, fActiveColor, false, "%s : Fade %f, Time %d, Cats %d",
		                       (*It).sName.c_str(), (*It).fFadeCurrent, (*It).nFadeTimeInMS,
		                       pMood->GetCategoryCount());
		ypos += 10;
	}

	pRenderer->Draw2dLabel(xpos, ypos, 1, fColor, false, "Sound Moods Mix: %d", m_ActiveMoods.size());
	ypos += 10;

	if (nSoundInfo == 9)
	{
		if (m_pFinalMix)
			m_pFinalMix->DrawInformation(pRenderer, xpos, ypos, false);
	}

	if (nSoundInfo == 10)
	{
		if (m_pFinalMix)
		{
			m_pFinalMix->DrawInformation(pRenderer, xpos, ypos, true);
		}
	}
}

bool CMoodManager::Serialize(XmlNodeRef& node, bool bLoading)
{
	bool bResult = true;

	if (bLoading)
	{
		// couldn't load or find file
		if (!node)
			return false;

		// quick test for correct tag
		if (strcmp("SoundMoods", node->getTag()) != 0)
			return false;

		// go through Childs and parse them
		int nGroupChildCount = node->getChildCount();
		for (int i = 0; i < nGroupChildCount; ++i)
		{
			XmlNodeRef GroupChildNode = node->getChild(i);

			// read the name of the child
			const char* sChildName = GroupChildNode->getAttr("Name");
			IMood* pNewMood = AddMood(sChildName);

			if (!pNewMood)
				return false; // something wrong !

			bool bChildResult = pNewMood->Serialize(GroupChildNode, true);

			if (!bChildResult)
			{
				RemoveMood(sChildName);
				bResult = false;
			}
		} // done with all children
	}
	else // saving
	{
		// setup a new root
		// XmlNodeRef root = GetISystem()->CreateXmlNode("SoundMood");

		node->setTag("SoundMoods");
		// node->setAttr("Name", GetName());

		int i = 0;
		IMood* pMood = GetMoodPtr(i);
		while (pMood && !pMood->GetIsMixMood())
		{

			XmlNodeRef ChildNode = node->newChild("Export");

			bResult = pMood->Serialize(ChildNode, false);

			if (!bResult)
				node->removeChild(ChildNode);

			++i;
			pMood = GetMoodPtr(i);
		}
	}

	m_bNeedUpdate = true;
	return bResult;
}

bool CMoodManager::Load(const char* sFilename)
{
	XmlNodeRef root = GetISystem()->LoadXmlFile(sFilename);
	Serialize(root, true);

	return true;
}

bool CMoodManager::RefreshCategories()
{
	IMood* pMood = NULL;
	bool bResult = false;

	uint32 nCount = 0;
	for (uint32 i = 0; i < m_MoodArray.size(); ++i)
	{
		if (m_MoodArray[i])
		{
			pMood = m_MoodArray[i];
			bResult |= !RefreshCategories(pMood->GetName());
		}
	}
	return bResult;
}

void CMoodManager::SerializeState(TSerialize ser)
{
	string sTempName;

	ser.Value("NeedUpdate", m_bNeedUpdate);
	ser.Value("ForceFinalMix", m_bForceFinalMix);

	if (ser.IsWriting())
	{
		// active moods
		int nActiveMoodsNum = m_ActiveMoods.size();
		ser.Value("MoodCount", nActiveMoodsNum);

		tvecActiveMoodsIter IterEnd = m_ActiveMoods.end();
		for (tvecActiveMoodsIter It = m_ActiveMoods.begin(); It != IterEnd; ++It)
		{
			ser.BeginGroup("ActiveMood");

			sTempName = (*It).sName;
			ser.Value("MoodName", sTempName);
			ser.Value("ForceUpdate", (*It).bForceUpdate);
			ser.Value("Unregister", (*It).bUnregisterOnFadeOut);
			ser.Value("FadeCurrent", (*It).fFadeCurrent);
			ser.Value("FadeTarget", (*It).fFadeTarget);
			ser.Value("FadeTime", (*It).nFadeTimeInMS);
			ser.Value("ReadyToRemove", (*It).bReadyToRemove);

			ser.EndGroup();
		}
	}
	else
	{
		m_ActiveMoods.clear();
		int nActiveMoodsNum = 0;
		ser.Value("MoodCount", nActiveMoodsNum);

		for (int i = 0; i < nActiveMoodsNum; ++i)
		{
			ser.BeginGroup("ActiveMood");

			ser.Value("MoodName", sTempName);

			if (GetMoodPtr(sTempName.c_str())) // mood exist
			{
				SSoundMoodProp MP;
				MP.sName = sTempName;
				ser.Value("ForceUpdate", MP.bForceUpdate);
				ser.Value("Unregister", MP.bUnregisterOnFadeOut);
				ser.Value("FadeCurrent", MP.fFadeCurrent);
				ser.Value("FadeTarget", MP.fFadeTarget);
				ser.Value("FadeTime", MP.nFadeTimeInMS);
				ser.Value("ReadyToRemove", MP.bReadyToRemove);
				m_ActiveMoods.push_back(MP);
			}
			ser.EndGroup();
		}

		m_bNeedUpdate = true;
		m_tLastUpdate = gEnv->pTimer->GetFrameStartTime();
	}
}
