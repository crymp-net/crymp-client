#ifndef __SYSTEMEVENTDISPATCHER_H__
#define __SYSTEMEVENTDISPATCHER_H__

#include "CryCommon/CrySystem/ISystem.h"

class CSystemEventDispatcher : public ISystemEventDispatcher
{
public:
	virtual ~CSystemEventDispatcher(){}

	// ISystemEventDispatcher
	virtual bool RegisterListener(ISystemEventListener *pListener);
	virtual bool RemoveListener(ISystemEventListener *pListener);

	virtual void OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam );
	// ~ISystemEventDispatcher
private:
	typedef std::list<ISystemEventListener*>	TSystemEventListeners;
	TSystemEventListeners	m_listeners;
};

#endif //__SYSTEMEVENTDISPATCHER_H__
