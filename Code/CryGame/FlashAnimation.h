/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Flash animation base class

-------------------------------------------------------------------------
History:
- 02:27:2007: Created by Marco Koegler

*************************************************************************/
#ifndef __FLASHANIMATION_H__
#define __FLASHANIMATION_H__

//-----------------------------------------------------------------------------------------------------

struct IFlashPlayer;
struct SFlashVarValue;

enum EFlashDock
{
	eFD_Stretch = BIT(0),
	eFD_Center = BIT(1),
	eFD_Left = BIT(3),
	eFD_Right = BIT(4),

	eFD_Top = BIT(5),
	eFD_Bottom = BIT(6),

	eFD_CenteredFit = BIT(9),
	eFD_Scaling = BIT(10),
	eFD_BottomFit = BIT(11),
	eFD_AnchorFitScale = BIT(12),
};

class CFlashAnimation
{
public:
	CFlashAnimation();
	virtual ~CFlashAnimation();

	IFlashPlayer*	GetFlashPlayer() const;

	void SetDock(uint32 eFDock);
	uint32 GetDock() const
	{
		return m_dock;
	}

	bool	LoadAnimation(const char* name);
	virtual void	Unload();
	bool	IsLoaded() const;
	void RepositionFlashAnimation();

	// these functions act on the flash player
	void SetVisible(bool visible);
	bool GetVisible() const;
	bool IsAvailable(const char* pPathToVar) const;
	bool SetVariable(const char* pPathToVar, const SFlashVarValue& value);
	bool CheckedSetVariable(const char* pPathToVar, const SFlashVarValue& value);
	bool Invoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult = 0);
	bool CheckedInvoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult = 0);
	// invoke helpers
	bool Invoke(const char* pMethodName, SFlashVarValue* pResult = 0)
	{
		return Invoke(pMethodName, 0, 0, pResult);
	}
	bool Invoke(const char* pMethodName, const SFlashVarValue& arg, SFlashVarValue* pResult = 0)
	{
		return Invoke(pMethodName, &arg, 1, pResult);
	}
	bool CheckedInvoke(const char* pMethodName, SFlashVarValue* pResult = 0)
	{
		return CheckedInvoke(pMethodName, 0, 0, pResult);
	}
	bool CheckedInvoke(const char* pMethodName, const SFlashVarValue& arg, SFlashVarValue* pResult = 0)
	{
		return CheckedInvoke(pMethodName, &arg, 1, pResult);
	}

	void ApplyScale(float hudScale);
	bool NeedsHUDScaleApply() const;
	void ForceHUDScaleApply();

	void SetIgnoreHUDScale(bool ignore)
	{
		m_ignoreHUDScale = ignore;
	}

	bool GetIgnoreHUDScale() const
	{
		return m_ignoreHUDScale;
	}

	void SetFixedScale(float scale)
	{
		m_fixedScale = scale;
	}

	void SetMaxScale(float scale)
	{
		m_maxScale = scale;
	}

	void SetMinScale(float scale)
	{
		m_minScale = scale;
	}

	float GetMaxScale() const
	{
		return m_maxScale;
	}

	float GetMinScale() const
	{
		return m_minScale;
	}

	float GetFixedScale() const
	{
		return m_fixedScale;
	}

	void EnableFixedScale(bool enable)
	{
		m_useFixedScale = enable;
	}

	bool IsFixedScaleEnabled() const
	{
		return m_useFixedScale;
	}

private:

	IFlashPlayer*	m_pFlashPlayer;
	uint32	m_dock;

	// shared null player
	static IFlashPlayer*	s_pFlashPlayerNull;

	bool m_ignoreHUDScale = false;
	bool m_useFixedScale = false;
	float m_fixedScale = 1.0f;
	float m_maxScale = 1.2f;
	float m_minScale = 0.4f;
	bool m_needsHUDScaleApply = true;
};

#endif //__FLASHANIMATION_H__
