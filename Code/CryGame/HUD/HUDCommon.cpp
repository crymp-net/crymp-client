/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
	Common functionality for all HUDs using Flash player
	Code which is not game-specific should go here
	Shared by G02 and G04

-------------------------------------------------------------------------
History:
- 22:02:2006: Created by Matthew Jack from original HUD class

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "HUDCommon.h"
#include "HUD.h"

#include "GameFlashAnimation.h"
#include "GameFlashLogic.h"
#include "CryGame/Menus/FlashMenuObject.h"

#include "CryGame/Actors/Player/Player.h"
#include "CryGame/Items/Weapons/Weapon.h"
#include "CryGame/Game.h"
#include "CryGame/GameCVars.h"
#include "CryGame/GameActions.h"

//-----------------------------------------------------------------------------------------------------
//-- IConsoleArgs
//-----------------------------------------------------------------------------------------------------

void CHUDCommon::HUD(ICVar* pVar)
{
	SAFE_HUD_FUNC(Show(pVar->GetIVal() != 0));
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::ShowGODMode(IConsoleCmdArgs* pConsoleCmdArgs)
{
	CHUD* pHUD = g_pGame->GetHUD();

	if (pHUD && 2 == pConsoleCmdArgs->GetArgCount())
	{
		if (gEnv->pSystem->IsDevMode())
			pHUD->m_bShowGODMode = false;
		else
		{
			if (0 == strcmp(pConsoleCmdArgs->GetArg(1), "0"))
			{
				pHUD->m_bShowGODMode = false;
			}
			else
			{
				pHUD->m_bShowGODMode = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------
//-- ~ IConsoleArgs
//-----------------------------------------------------------------------------------------------------

void CHUDCommon::UpdateRatio()
{
	// try to resize based on any width and height
	for (TGameFlashAnimationsList::iterator i = m_gameFlashAnimationsList.begin(); i != m_gameFlashAnimationsList.end(); ++i)
	{
		CGameFlashAnimation* pAnim = (*i);
		RepositionFlashAnimation(pAnim);
	}

	m_width = gEnv->pRenderer->GetWidth();
	m_height = gEnv->pRenderer->GetHeight();
}

//-----------------------------------------------------------------------------------------------------

CHUDCommon::CHUDCommon()
{
	m_bShowGODMode = true;
	m_godMode = 0;
	m_iDeaths = 0;

	strcpy(m_strGODMode, "");

	m_width = 0;
	m_height = 0;

	m_bForceInterferenceUpdate = false;
	m_distortionStrength = 0;
	m_displacementStrength = 0;
	m_alphaStrength = 0;
	m_interferenceDecay = 0;

	m_displacementX = 0;
	m_displacementY = 0;
	m_distortionX = 0;
	m_distortionY = 0;
	m_alpha = 100;

	m_bShow = true;

	gEnv->pConsole->AddCommand("ShowGODMode", ShowGODMode);

	if (gEnv->pHardwareMouse)
	{
		gEnv->pHardwareMouse->AddListener(this);
	}
}

//-----------------------------------------------------------------------------------------------------

CHUDCommon::~CHUDCommon()
{
	this->ShowMouseCursor(false);

	if (gEnv->pHardwareMouse)
	{
		gEnv->pHardwareMouse->RemoveListener(this);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::Show(bool bShow)
{
	m_bShow = bShow;
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::SetGODMode(uint8 ucGodMode, bool forceUpdate)
{
	if (forceUpdate || m_godMode != ucGodMode)
	{
		m_godMode = ucGodMode;
		m_fLastGodModeUpdate = gEnv->pTimer->GetAsyncTime().GetSeconds();

		if (gEnv->pSystem->IsDevMode())
		{
			if (0 == ucGodMode)
			{
				strcpy(m_strGODMode, "GOD MODE OFF");
				m_iDeaths = 0;
			}
			else if (1 == ucGodMode)
			{
				strcpy(m_strGODMode, "GOD");
			}
			else if (2 == ucGodMode)
			{
				strcpy(m_strGODMode, "Team GOD");
			}
			else if (3 == ucGodMode)
			{
				strcpy(m_strGODMode, "DEMI GOD");
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------
//-- Cursor handling
//-----------------------------------------------------------------------------------------------------

void CHUDCommon::ShowMouseCursor(bool show)
{
	if (m_isMouseCursorVisible == show)
	{
		return;
	}

	m_isMouseCursorVisible = show;

	if (gEnv->pHardwareMouse)
	{
		CryLogComment("%s: Changing cursor visibility", __FUNCTION__);

		if (show)
		{
			gEnv->pHardwareMouse->IncrementCounter();
		}
		else
		{
			gEnv->pHardwareMouse->DecrementCounter();
		}
	}

	if (g_pGameActions && g_pGameActions->FilterNoMouse())
	{
		g_pGameActions->FilterNoMouse()->Enable(show);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::Register(CGameFlashAnimation* pAnim)
{
	TGameFlashAnimationsList::iterator it = std::find(m_gameFlashAnimationsList.begin(), m_gameFlashAnimationsList.end(), pAnim);

	if (it == m_gameFlashAnimationsList.end())
		m_gameFlashAnimationsList.push_back(pAnim);
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::Remove(CGameFlashAnimation* pAnim)
{
	TGameFlashAnimationsList::iterator it = std::find(m_gameFlashAnimationsList.begin(), m_gameFlashAnimationsList.end(), pAnim);

	if (it != m_gameFlashAnimationsList.end())
		m_gameFlashAnimationsList.erase(it);
}

//-----------------------------------------------------------------------------------------------------
//-- Starting new interference effect 
//-----------------------------------------------------------------------------------------------------

void CHUDCommon::StartInterference(float distortion, float displacement, float alpha, float decay)
{
	m_distortionStrength = distortion;
	m_displacementStrength = displacement;
	m_alphaStrength = alpha;
	m_interferenceDecay = decay;
	m_bForceInterferenceUpdate = true;
}

//-----------------------------------------------------------------------------------------------------
//-- Creating random distortion and displacements
//-----------------------------------------------------------------------------------------------------

void CHUDCommon::CreateInterference()
{
	if (m_distortionStrength || m_displacementStrength || m_alphaStrength || m_bForceInterferenceUpdate)
	{
		float fDistortionStrengthOverTwo = m_distortionStrength * 0.5f;

		m_distortionX = (int)((Random() * m_distortionStrength) - fDistortionStrengthOverTwo);
		m_distortionY = (int)((Random() * m_distortionStrength) - fDistortionStrengthOverTwo);
		m_displacementX = (int)((Random() * m_displacementStrength) - fDistortionStrengthOverTwo);
		m_displacementY = (int)((Random() * m_displacementStrength) - fDistortionStrengthOverTwo);

		m_alpha = 100 - (int)(Random() * m_alphaStrength);

		float fMultiplier = m_interferenceDecay * gEnv->pTimer->GetFrameTime();

		m_distortionStrength -= m_distortionStrength * fMultiplier;
		m_displacementStrength -= m_displacementStrength * fMultiplier;
		m_alphaStrength -= m_alphaStrength * fMultiplier;

		if (m_distortionStrength < 0.5f)
			m_distortionStrength = 0.0f;
		if (m_displacementStrength < 0.5f)
			m_displacementStrength = 0.0f;
		if (m_alphaStrength < 1.0f)
			m_alphaStrength = 0.0f;

		if (!m_distortionStrength && !m_displacementStrength && !m_alphaStrength)
		{
			m_displacementX = 0;
			m_displacementY = 0;
			m_distortionX = 0;
			m_distortionY = 0;
			m_alpha = 100;
		}

		for (TGameFlashAnimationsList::iterator iter = m_gameFlashAnimationsList.begin(); iter != m_gameFlashAnimationsList.end(); ++iter)
		{
			RepositionFlashAnimation(*iter);
		}

		m_bForceInterferenceUpdate = false;
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::ApplyDocking(uint32 dock, int screenW, int screenH, int w, int h, int& x, int& y) const
{
	// Horizontal
	if (dock & eFD_Left)
		x = 0;
	else if (dock & eFD_Right)
		x = screenW - w;
	else
		x = (screenW - w) / 2;

	// Vertical
	if (dock & eFD_Top)
		y = 0;
	else if (dock & eFD_Bottom)
		y = screenH - h;
	else
		y = (screenH - h) / 2;
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::ApplyViewport(IFlashPlayer* pPlayer, int x, int y, int w, int h,
	float dispX, float dispY, float distX, float distY, float alpha) const
{
	int finalX = (int)(x + dispX - distX * 0.5f);
	int finalY = (int)(y + dispY - distY * 0.5f);
	int finalW = max(1, w + (int)distX);
	int finalH = max(1, h + (int)distY);

	pPlayer->SetViewport(finalX, finalY, finalW, finalH);
	pPlayer->SetVariable("_alpha", SFlashVarValue(alpha));
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::RepositionFlashAnimation(CGameFlashAnimation* pAnimation) const
{
	if (!pAnimation)
		return;

	IFlashPlayer* pPlayer = pAnimation->GetFlashPlayer();
	if (!pPlayer)
		return;

	const int screenW = gEnv->pRenderer->GetWidth();
	const int screenH = gEnv->pRenderer->GetHeight();

	if (screenW <= 0 || screenH <= 0)
		return;

	const uint32 dock = pAnimation->GetDock();

	int x = 0, y = 0, w = 0, h = 0;

	// --------------------------------------------------
	// CENTERED FIT
	// --------------------------------------------------
	if (dock & eFD_CenteredFit)
	{
		const int fw = pPlayer->GetWidth();
		const int fh = pPlayer->GetHeight();
		if (fw <= 0 || fh <= 0)
			return;

		float scale = min((float)screenW / fw, (float)screenH / fh);

		if (pAnimation->IsFixedScaleEnabled())
			scale *= pAnimation->GetFixedScale();
		else if (!pAnimation->GetIgnoreHUDScale())
			scale *= CLAMP(g_pGameCVars->hud_scale, pAnimation->GetMinScale(), pAnimation->GetMaxScale());

		w = max(1, (int)(fw * scale));
		h = max(1, (int)(fh * scale));

		ApplyDocking(dock, screenW, screenH, w, h, x, y);
		ApplyViewport(pPlayer, x, y, w, h, m_displacementX, m_displacementY, m_distortionX, m_distortionY, m_alpha);
		return;
	}

	// --------------------------------------------------
	// SCALING
	// --------------------------------------------------
	if (dock & eFD_Scaling)
	{
		pAnimation->RepositionFlashAnimation();

		float aspect = 0.f;
		pPlayer->GetViewport(x, y, w, h, aspect);

		float scale = 1.f;

		if (pAnimation->IsFixedScaleEnabled())
			scale = pAnimation->GetFixedScale();
		else if (!pAnimation->GetIgnoreHUDScale())
			scale = CLAMP(g_pGameCVars->hud_scale, pAnimation->GetMinScale(), pAnimation->GetMaxScale());

		const int oldX = x;
		const int oldY = y;
		const int oldW = w;
		const int oldH = h;

		w = max(1, (int)(w * scale));
		h = max(1, (int)(h * scale));

		if (dock & eFD_Left)
			x = oldX;
		else if (dock & eFD_Right)
			x = oldX + oldW - w;
		else
			x = oldX + ((oldW - w) / 2);

		if (dock & eFD_Top)
			y = oldY;
		else if (dock & eFD_Bottom)
			y = oldY + oldH - h;
		else
			y = oldY + ((oldH - h) / 2);

		ApplyViewport(pPlayer, x, y, w, h, m_displacementX, m_displacementY, m_distortionX, m_distortionY, m_alpha);
		return;
	}

	// --------------------------------------------------
	// DEFAULT
	// --------------------------------------------------
	pAnimation->RepositionFlashAnimation();

	float aspect = 0.f;
	pPlayer->GetViewport(x, y, w, h, aspect);

	ApplyViewport(pPlayer, x, y, w, h, m_displacementX, m_displacementY, m_distortionX, m_distortionY, m_alpha);
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::Serialize(TSerialize ser)
{
	ser.Value("distortionStrength", m_distortionStrength);
	ser.Value("displacementStrength", m_displacementStrength);
	ser.Value("alphaStrength", m_alphaStrength);
	ser.Value("interferenceDecay", m_interferenceDecay);

	m_bForceInterferenceUpdate = true;
}

//-----------------------------------------------------------------------------------------------------
