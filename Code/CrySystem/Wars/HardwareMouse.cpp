/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:

System "Hardware mouse" cursor with reference counter.
This is needed because Menus / HUD / Profiler / or whatever
can use the cursor not at the same time be successively
=> We need to know when to enable/disable the cursor.

-------------------------------------------------------------------------
History:
- 18:12:2006   Created by Julien Darré

*************************************************************************/

#include "StdAfx.h"

#ifdef WIN32
#include <windows.h>
#endif

#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CryInput/IInput.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/ITimer.h"
#include "HardwareMouse.h"

const static bool g_debugHardwareMouse = false;

#ifdef WIN32
void ReleaseCursor()
{
	::ClipCursor(NULL);
}
#endif
//-----------------------------------------------------------------------------------------------------

CHardwareMouse::CHardwareMouse(bool bVisibleByDefault)
{
#ifdef WIN32
	atexit(ReleaseCursor);
#endif
	Reset(bVisibleByDefault);

	if (gEnv->pSystem)
		gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this);
}

//-----------------------------------------------------------------------------------------------------

CHardwareMouse::~CHardwareMouse()
{
	if(gEnv)
	{
		if(gEnv->pRenderer)
			gEnv->pRenderer->RemoveListener(this);
		if(gEnv->pInput)
			gEnv->pInput->RemoveEventListener(this);
		
		if (gEnv->pSystem)
			gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::ShowHardwareMouse(bool bShow)
{
	if (g_debugHardwareMouse)
		CryLogAlways("HM: ShowHardwareMouse = %d", bShow);

	if(bShow)
	{
		SetHardwareMousePosition(m_fCursorX,m_fCursorY);
	}
	else
	{
		GetHardwareMousePosition(&m_fCursorX,&m_fCursorY);
	}

#ifdef WIN32
	::ShowCursor(bShow);
#endif
	bool bConfine = !bShow;

	if (IsFullscreen() || !gEnv->pSystem->IsDevMode())
		bConfine = true;

	ConfineCursor(bConfine);

	if (gEnv->pInput) gEnv->pInput->SetExclusiveMode(eDI_Mouse,false);
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::ConfineCursor(bool confine)
{
	if (g_debugHardwareMouse)
		CryLogAlways("HM: ConfineCursor = %d", confine);
#ifdef WIN32
	if (gEnv == NULL || gEnv->pRenderer == NULL)
		return;

	HWND hWnd = 0;
	
	if (gEnv->pSystem->IsEditor())
		hWnd = (HWND) gEnv->pRenderer->GetCurrentContextHWND();
	else
		hWnd = (HWND) gEnv->pRenderer->GetHWND();

	if (hWnd)
	{
		bool active = false;//(::GetActiveWindow() == hWnd);
		bool focused = (::GetFocus() == hWnd);
		bool foreground = (::GetForegroundWindow() == hWnd) || gEnv->pSystem->IsEditor();

		confine = confine && (active || (focused && foreground));
		// It's necessary to call ClipCursor AFTER the calls to
		// CreateDevice/ResetDevice otherwise the clip area is reseted.
		if(confine && !gEnv->pSystem->IsEditorMode())
		{
			if (g_debugHardwareMouse)
				gEnv->pLog->Log("HM:   Confining cursor");
			RECT rcClient;
			::GetClientRect(hWnd, &rcClient);
			::ClientToScreen(hWnd, (LPPOINT)&rcClient.left);
			::ClientToScreen(hWnd, (LPPOINT)&rcClient.right);
			::ClipCursor(&rcClient);
		}
		else
		{
			if (g_debugHardwareMouse)
				gEnv->pLog->Log("HM:   Releasing cursor");
			::ClipCursor(NULL);
		}
	}
#endif
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::OnPostCreateDevice()
{
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::OnPostResetDevice()
{
}

//-----------------------------------------------------------------------------------------------------

bool CHardwareMouse::OnInputEvent(const SInputEvent &rInputEvent)
{
	static float s_fAcceleration = 1.0f;

	if(0 == m_iReferenceCounter)
	{
		// Do not emulate if mouse is not present on screen
	}
	else if(eDI_XI == rInputEvent.deviceId)
	{
		if(rInputEvent.keyName == "xi_thumbrx")
		{
			m_fIncX = rInputEvent.value;
		}
		else if(rInputEvent.keyName == "xi_thumbry")
		{
			m_fIncY = -rInputEvent.value;
		}
		else if(rInputEvent.keyName == "xi_a")
		{
			// This emulation was just not right, A-s meaning is context sensitive
			/*if(eIS_Pressed == rInputEvent.state)
			{
				Event((int)m_fCursorX,(int)m_fCursorY,HARDWAREMOUSEEVENT_LBUTTONDOWN);
			}
			else if(eIS_Released == rInputEvent.state)
			{
				Event((int)m_fCursorX,(int)m_fCursorY,HARDWAREMOUSEEVENT_LBUTTONUP);
			}*/
			// TODO: do we simulate double-click?
		}
	}
	else if(rInputEvent.keyId == eKI_SYS_Commit)
	{
		const float fSensitivity = 100.0f;
		const float fDeadZone = 0.3f;

		if(	m_fIncX < -fDeadZone || m_fIncX > +fDeadZone ||
				m_fIncY < -fDeadZone || m_fIncY > +fDeadZone)
		{
			float fFrameTime = gEnv->pTimer->GetFrameTime(ITimer::ETIMER_UI);
			if(s_fAcceleration < 10.0f)
			{
				s_fAcceleration += fFrameTime * 5.0f;
			}
			m_fCursorX += m_fIncX * fSensitivity * s_fAcceleration * fFrameTime;
			m_fCursorY += m_fIncY * fSensitivity * s_fAcceleration * fFrameTime;
			SetHardwareMousePosition(m_fCursorX,m_fCursorY);
		}
		else
		{
			GetHardwareMousePosition(&m_fCursorX,&m_fCursorY);
			s_fAcceleration = 1.0f;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------
void CHardwareMouse::OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
{
	if (event == ESYSTEM_EVENT_CHANGE_FOCUS)
	{
		//gEnv->pLog->Log("Change focus - %d", wparam);
		m_bFocus = wparam != 0;

		if (m_bFocus)
		{
			if (!gEnv->pSystem->IsEditorMode() && m_recapture)
			{
				m_recapture = false;
				DecrementCounter();
			}

			if (IsFullscreen() || !gEnv->pSystem->IsDevMode())
				ConfineCursor(true);
		}
		else
		{
			if (!gEnv->pSystem->IsEditorMode())
			{
				if (m_iReferenceCounter == 0)
				{
					m_recapture = true;
					IncrementCounter();
				}
				ConfineCursor(false);
			}
		}
	}
	else if (event == ESYSTEM_EVENT_MOVE)
	{
		if (IsFullscreen() || m_iReferenceCounter==0)
			ConfineCursor(true);
	}
	else if (event == ESYSTEM_EVENT_RESIZE)
	{
		if (IsFullscreen() || m_iReferenceCounter==0)
			ConfineCursor(true);
	}
	else if (event == ESYSTEM_EVENT_TOGGLE_FULLSCREEN)
	{
		if (wparam || m_iReferenceCounter==0)
			ConfineCursor(true);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::Release()
{
	delete this;
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::OnPreInitRenderer()
{
	CRY_ASSERT(gEnv->pRenderer);

	if (gEnv->pRenderer)
		gEnv->pRenderer->AddListener(this);
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::OnPostInitInput()
{
	CRY_ASSERT(gEnv->pInput);

	if (gEnv->pInput) 
		gEnv->pInput->AddEventListener(this);
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::Event(int iX,int iY,EHARDWAREMOUSEEVENT eHardwareMouseEvent)
{
	// Ignore events while console is active
	if(gEnv->pConsole->GetStatus())
	{
		return;
	}

	for(TListHardwareMouseEventListeners::iterator iter=m_listHardwareMouseEventListeners.begin(); iter!=m_listHardwareMouseEventListeners.end(); ++iter)
	{
		(*iter)->OnHardwareMouseEvent(iX,iY,eHardwareMouseEvent);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::AddListener(IHardwareMouseEventListener *pHardwareMouseEventListener)
{
	stl::push_back_unique(m_listHardwareMouseEventListeners,pHardwareMouseEventListener);
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::RemoveListener(IHardwareMouseEventListener *pHardwareMouseEventListener)
{
	stl::find_and_erase(m_listHardwareMouseEventListeners,pHardwareMouseEventListener);
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::SetGameMode(bool bGameMode)
{
	if(bGameMode)
	{
		DecrementCounter();
	}
	else
	{
		IncrementCounter();
	}
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::IncrementCounter()
{
	m_iReferenceCounter++;

	if (g_debugHardwareMouse)
		CryLogAlways("HM: IncrementCounter = %d", m_iReferenceCounter);
	CRY_ASSERT(m_iReferenceCounter >= 0);

	if(1 == m_iReferenceCounter)
	{
		ShowHardwareMouse(true);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::DecrementCounter()
{
	m_iReferenceCounter--;

	if (g_debugHardwareMouse)
		CryLogAlways("HM: DecrementCounter = %d", m_iReferenceCounter);
	CRY_ASSERT(m_iReferenceCounter >= 0);

	if(0 == m_iReferenceCounter)
	{
		ShowHardwareMouse(false);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::GetHardwareMousePosition(float *pfX,float *pfY)
{
#ifdef WIN32
	POINT pointCursor;
	GetCursorPos(&pointCursor);
	*pfX = (float) pointCursor.x;
	*pfY = (float) pointCursor.y;
#else
	*pfX = 0.0f;
	*pfY = 0.0f;
#endif
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::SetHardwareMousePosition(float fX,float fY)
{
#ifdef WIN32
	SetCursorPos((int) fX,(int) fY);
#endif
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::GetHardwareMouseClientPosition(float *pfX,float *pfY)
{
#ifdef WIN32
	if (gEnv == NULL || gEnv->pRenderer == NULL)
		return;

	HWND hWnd = (HWND) gEnv->pRenderer->GetHWND();
	CRY_ASSERT_MESSAGE(hWnd,"Impossible to get client coordinates from a non existing window!");

	if(hWnd)
	{
		POINT pointCursor;
		GetCursorPos(&pointCursor);
		ScreenToClient(hWnd,&pointCursor);
		*pfX = (float) pointCursor.x;
		*pfY = (float) pointCursor.y;
	}
	else
	{
		*pfX = 0.0f;
		*pfY = 0.0f;
	}
#else
	*pfX = 0.0f;
	*pfY = 0.0f;
#endif
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::SetHardwareMouseClientPosition(float fX,float fY)
{
#ifdef WIN32
	HWND hWnd = (HWND) gEnv->pRenderer->GetHWND();
	CRY_ASSERT_MESSAGE(hWnd,"Impossible to set position of the mouse relative to client coordinates from a non existing window!");

	if(hWnd)
	{
		POINT pointCursor;
		pointCursor.x = fX;
		pointCursor.y = fY;
		ClientToScreen(hWnd,&pointCursor);
		SetCursorPos(pointCursor.x,pointCursor.y);
	}
#endif
}

//-----------------------------------------------------------------------------------------------------

void CHardwareMouse::Reset(bool bVisibleByDefault)
{
	m_iReferenceCounter = bVisibleByDefault ? 1 : 0;
	//ShowHardwareMouse(bVisibleByDefault);
	GetHardwareMousePosition(&m_fCursorX,&m_fCursorY);

	if (!bVisibleByDefault)
		ConfineCursor(true);

	m_fIncX = 0.0f;
	m_fIncY = 0.0f;
	m_bFocus = true;
	m_recapture = false;
}

//-----------------------------------------------------------------------------------------------------

bool CHardwareMouse::IsFullscreen()
{
	if (gEnv && gEnv->pConsole)
	{
		ICVar *pVar = gEnv->pConsole->GetCVar("r_Fullscreen");
		if (pVar)
			return pVar->GetIVal() != 0;
	}
	return false;
}
//-----------------------------------------------------------------------------------------------------
