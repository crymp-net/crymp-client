/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 1:9:2005   11:32 : Created by Márcio Martins

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "ItemScheduler.h"
#include "Item.h"
#include "CryGame/Game.h"
#include "CryCommon/CryAction/IGameObject.h"


//------------------------------------------------------------------------
CItemScheduler::CItemScheduler(CItem *item)
: m_pItem(item)
{
}

//------------------------------------------------------------------------
CItemScheduler::~CItemScheduler()
{
	Reset();
}

//------------------------------------------------------------------------
void CItemScheduler::Reset(bool keepPersistent)
{
	m_timers.erase(
		std::remove_if(m_timers.begin(), m_timers.end(),
			[keepPersistent](auto& timer) 
			{
				if (!timer.persist || !keepPersistent) 
				{
					timer.action->destroy();
					return true;
				}
				return false;
			}),
		m_timers.end()
	);

	m_schedule.erase(
		std::remove_if(m_schedule.begin(), m_schedule.end(),
			[keepPersistent](auto& scheduled) 
			{
				if (!scheduled.persist || !keepPersistent)
				{
					scheduled.action->destroy();
					return true;
				}
				return false;
			}),
		m_schedule.end()
	);

	if (m_timers.empty() && m_schedule.empty())
	{
		m_pItem->EnableUpdate(false, eIUS_Scheduler);
	}

	SetBusy(false);
}

//------------------------------------------------------------------------
void CItemScheduler::Update(float frameTime)
{
	if (frameTime > 0.2f)
		frameTime = 0.2f;

	if (!m_schedule.empty())
	{
		while(!m_schedule.empty() && !m_busy)
		{
			SScheduledAction &action = *m_schedule.begin();
			ISchedulerAction *pAction = action.action;
			m_schedule.erase(m_schedule.begin());

			pAction->execute(m_pItem);
			pAction->destroy();
		}
	}

	if (!m_timers.empty())
	{
		unsigned int count=0;
		m_actives.swap(m_timers);

		for (auto &active : m_actives)
		{
			ISchedulerAction* pAction = active.action;
			active.time -= frameTime;
			if (active.time <= 0.0f)
			{
				pAction->execute(m_pItem);
				pAction->destroy();
				++count;
			}
		}

		if (count)
			m_actives.erase(m_actives.begin(), m_actives.begin()+count);

		if (!m_timers.empty())
		{
			for (const auto &timer : m_timers) 
				m_actives.push_back(timer);

			std::sort(m_actives.begin(), m_actives.end(), compare_timers());
		}

		m_actives.swap(m_timers);
		m_actives.resize(0);
	}

	if (m_timers.empty() && m_schedule.empty())
	{
		m_pItem->EnableUpdate(false, eIUS_Scheduler);
	}
	else
	{
		m_pItem->RequireUpdate(eIUS_Scheduler);
	}
}

//------------------------------------------------------------------------
void CItemScheduler::ScheduleAction(ISchedulerAction *action, bool persistent)
{
	if (m_locked)
		return;

	if (!m_busy)
	{
		action->execute(m_pItem);
		return;
	}

	SScheduledAction scheduleAction;
	scheduleAction.action = action;
	scheduleAction.persist = persistent;

	m_schedule.push_back(scheduleAction);

	m_pItem->EnableUpdate(true, eIUS_Scheduler);
}

//------------------------------------------------------------------------
unsigned int CItemScheduler::TimerAction(unsigned int time, ISchedulerAction *action, bool persistent)
{
	if (m_locked)
		return 0;

	STimerAction timerAction;
	timerAction.action = action;
	timerAction.time = (float)time/1000.0f;
	timerAction.persist = persistent;

	++m_timerIdGen;
	if (m_timerIdGen > 1000)
		m_timerIdGen = 1;

	timerAction.id = m_timerIdGen;

	m_timers.push_back(timerAction);
	std::sort(m_timers.begin(), m_timers.end(), compare_timers());

	m_pItem->EnableUpdate(true, eIUS_Scheduler);

	return timerAction.id;
}

//------------------------------------------------------------------------
void CItemScheduler::KillTimer(unsigned int timerId, bool execute /*=false*/)
{
	std::erase_if(m_timers,
		[this, timerId, execute](auto& timer)   // capture this + execute + timerId
		{
			if (static_cast<unsigned int>(timer.id) == timerId)
			{
				if (execute)
				{
					timer.action->execute(m_pItem);
				}
				timer.action->destroy();
				return true;
			}
			return false;
		}
	);
}

//------------------------------------------------------------------------
bool CItemScheduler::IsTimerActive(unsigned int timerId) const
{
	for (const auto& timer : m_timers)
	{
		if (static_cast<unsigned int>(timer.id) == timerId)
		{
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------
void CItemScheduler::SetBusy(bool busy)
{
	if (m_locked)
		return;

	m_busy = busy;
}

//------------------------------------------------------------------------
void CItemScheduler::Lock(bool lock)
{
	m_locked=lock;
}

//------------------------------------------------------------------------
bool CItemScheduler::IsLocked()
{
	return m_locked;
}

void CItemScheduler::GetMemoryStatistics(ICrySizer * s)
{
	s->AddContainer(m_timers);
	s->AddContainer(m_actives);
	s->AddContainer(m_schedule);
	for (size_t i=0; i<m_timers.size(); i++)
		m_timers[i].action->GetMemoryStatistics(s);
	for (size_t i=0; i<m_actives.size(); i++)
		m_actives[i].action->GetMemoryStatistics(s);
	for (size_t i=0; i<m_schedule.size(); i++)
		m_schedule[i].action->GetMemoryStatistics(s);
}
