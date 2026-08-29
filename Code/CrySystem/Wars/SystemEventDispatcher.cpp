#include "StdAfx.h"
#include "SystemEventDispatcher.h"

bool CSystemEventDispatcher::RegisterListener(ISystemEventListener *pListener)
{
	if (pListener && std::find(m_listeners.begin(), m_listeners.end(), pListener) == m_listeners.end())
	{
		m_listeners.push_back(pListener);
		return true;
	}
	return false;
}

bool CSystemEventDispatcher::RemoveListener(ISystemEventListener *pListener)
{
	TSystemEventListeners::iterator i = std::find(m_listeners.begin(), m_listeners.end(), pListener);
	if (i != m_listeners.end())
	{
		m_listeners.erase(i);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSystemEventDispatcher::OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
{
	for (TSystemEventListeners::iterator i = m_listeners.begin(); i != m_listeners.end(); ++i)
	{
		(*i)->OnSystemEvent(event,wparam,lparam);
	}
}