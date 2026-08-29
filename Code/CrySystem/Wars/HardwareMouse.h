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
#ifndef __HARDWAREMOUSE_H__
#define __HARDWAREMOUSE_H__

//-----------------------------------------------------------------------------------------------------

#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CrySystem/IHardwareMouse.h"

//-----------------------------------------------------------------------------------------------------

class CHardwareMouse : public IRendererEventListener, public IInputEventListener, public IHardwareMouse, public ISystemEventListener
{
public:

	// IRendererEventListener
	virtual void OnPostCreateDevice	();
	virtual void OnPostResetDevice	();
	// ~IRendererEventListener

	// IInputEventListener
	virtual bool OnInputEvent(const SInputEvent &rInputEvent);
	// ~IInputEventListener

	// ISystemEventListener
	virtual void OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam );
	// ~ISystemEventListener

	// IHardwareMouse
	virtual void Release();
	virtual void OnPreInitRenderer();
	virtual void OnPostInitInput();
	virtual void Event(int iX,int iY,EHARDWAREMOUSEEVENT eHardwareMouseEvent);
	virtual void AddListener		(IHardwareMouseEventListener *pHardwareMouseEventListener);
	virtual void RemoveListener	(IHardwareMouseEventListener *pHardwareMouseEventListener);
	virtual void SetGameMode(bool bGameMode);
	virtual void IncrementCounter();
	virtual void DecrementCounter();
	virtual void GetHardwareMousePosition(float *pfX,float *pfY);
	virtual void SetHardwareMousePosition(float fX,float fY);
	virtual void GetHardwareMouseClientPosition(float *pfX,float *pfY);
	virtual void SetHardwareMouseClientPosition(float fX,float fY);
	virtual void Reset(bool bVisibleByDefault);
	virtual void ConfineCursor(bool	confine);
	// ~IHardwareMouse

		CHardwareMouse(bool bVisibleByDefault);
	~	CHardwareMouse();

private:
	void ShowHardwareMouse(bool bShow);
	static bool IsFullscreen();

	typedef std::list<IHardwareMouseEventListener *> TListHardwareMouseEventListeners;
	TListHardwareMouseEventListeners m_listHardwareMouseEventListeners;

	int m_iReferenceCounter;
	float m_fCursorX;
	float m_fCursorY;
	float m_fIncX;
	float m_fIncY;
	bool	m_bFocus;
	bool	m_recapture;
};

//-----------------------------------------------------------------------------------------------------

#endif

//-----------------------------------------------------------------------------------------------------