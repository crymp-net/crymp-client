/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Item Scheduler

-------------------------------------------------------------------------
History:
- 1:9:2005   11:33 : Created by Márcio Martins

*************************************************************************/
#ifndef __ITEMSCHEDULER_H__
#define __ITEMSCHEDULER_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <vector>
#include "CryCommon/CrySystem/ITimer.h"
#include "CryCommon/CryCore/CrySizer.h"
#include "CryCommon/CryCore/PoolAllocator.h"


class CItem;

// never inherit directly from ISchedulerAction, instead write a class that looks like:
// class Blah { void execute(CItem *) { ... } };
// and use a pAction = CSchedulerAction<Blah>::Create(Blah(...));
struct ISchedulerAction
{
	virtual void execute(CItem *_this) = 0;
	virtual void destroy() = 0;
	virtual void GetMemoryStatistics(ICrySizer * s) = 0;
};

template <class T>
class CSchedulerAction : public ISchedulerAction
{
	// second sizeof(void*) is just for safety!
	typedef stl::PoolAllocator<sizeof(T) + sizeof(void*) + sizeof(void*), stl::PoolAllocatorSynchronizationSinglethreaded> Alloc;
	static Alloc m_alloc;

public:
	static CSchedulerAction * Create()
	{
		return new (m_alloc.Allocate()) CSchedulerAction();
	}
	static CSchedulerAction * Create( const T& from )
	{
		return new (m_alloc.Allocate()) CSchedulerAction(from);
	}

	void execute(CItem * _this) 
	{ 
		m_impl.execute(_this); 
	}
	void destroy() 
	{ 
		this->~CSchedulerAction();
		m_alloc.Deallocate(this);
	}
	void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); }

private:
	T m_impl;

	CSchedulerAction() {}
	CSchedulerAction( const T& from ) : m_impl(from) {}
	~CSchedulerAction() 
	{
	}
};

// --- CryMP: Lambda adapter: turn any callable F into a Scheduler action ---
template<class F>
struct SLambdaAction
{
	F fn;
	void execute(CItem* item) { fn(item); }
};

template<class F>
inline ISchedulerAction* MakeAction(F&& f)
{
	using Decayed = std::decay_t<F>;
	using Wrapper = SLambdaAction<Decayed>;
	return CSchedulerAction<Wrapper>::Create(Wrapper{ std::forward<F>(f) });
}
// --- CryMP: Lambda adapter end ---

template <class T>
typename CSchedulerAction<T>::Alloc CSchedulerAction<T>::m_alloc;

class CItemScheduler
{
	struct SScheduledAction
	{
		ISchedulerAction* action = nullptr;
		bool persist = false;
	};
	struct STimerAction
	{
		ISchedulerAction* action = nullptr;
		float time = 0.0f;
		bool persist = false;
		int id = 0;
	};

	typedef std::vector<STimerAction>									TTimerActionVector;
	typedef std::vector<SScheduledAction>							TScheduledActionVector;

	struct compare_timers
	{
		bool operator() (const STimerAction &lhs, const STimerAction &rhs ) const
		{
			return lhs.time < rhs.time;
		}
	};

public:
	CItemScheduler(CItem *item);
	virtual ~CItemScheduler();
	void Reset(bool keepPersistent=false);
	void Update(float frameTime);
	unsigned int TimerAction(unsigned int time, ISchedulerAction *action, bool persistent=false);
	void KillTimer(unsigned int timerId, bool execute = false);
	bool IsTimerActive(unsigned int timerId) const;
	void ScheduleAction(ISchedulerAction *action, bool persistent=false);
	void GetMemoryStatistics(ICrySizer * s);

	bool ILINE IsBusy() const { return m_busy; };
	void SetBusy(bool busy);

	void Lock(bool lock);
	bool IsLocked();

	int GetActivesCount() const { return m_actives.size(); }
	int GetTimersCount() const { return m_timers.size(); }

private:
	bool m_locked = false;
	bool m_busy = false;
	ITimer* m_pTimer = gEnv->pTimer;
	CItem* m_pItem = nullptr;

	int m_timerIdGen = 0;

	TTimerActionVector				m_timers;
	TTimerActionVector				m_actives;
	TScheduledActionVector		m_schedule;
};



#endif //__ITEMSCHEDULER_H__
