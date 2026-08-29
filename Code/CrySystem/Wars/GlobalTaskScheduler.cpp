#include "StdAfx.h"
#include "GlobalTaskScheduler.h"
#include <float.h>										// FLT_MAX
#include <cmath>											// floor

CGlobalTaskScheduler::CGlobalTaskScheduler()
{
	Reset();
}

bool CGlobalTaskScheduler::AddTask( void *pObjectIdentifier, const char *szTaskTypeName, 
	const float fMinDelayTime, const float fMaxDelayTime, const uint32 dwCount, const float fInitalTimePerUnitEstimate )
{
	assert(szTaskTypeName);
	assert(fMinDelayTime<=fMaxDelayTime);
	assert(fMinDelayTime>=0);
	assert(fMaxDelayTime>0);
	assert(dwCount);

	STaskEntry *pTask = GetTask(FindTaskIndex(pObjectIdentifier));

	if(pTask)		// was already registered
	{
		pTask->m_bWasQueriedThisFrame=true;
		pTask->m_dwUnitsLeft+=dwCount;
//		assert(pTask->m_szTaskTypeName==szTaskTypeName);
		return false;
	}

	m_GlobalTasks.push_back(STaskEntry(pObjectIdentifier,szTaskTypeName,fMinDelayTime,fMaxDelayTime,dwCount));

	std::map<const char *, STimeEstimater>::iterator it = m_TimeEstimater.find(szTaskTypeName);

	if(it==m_TimeEstimater.end())					// wasn't registered yet?
		m_TimeEstimater.insert(std::pair<const char *, STimeEstimater>(szTaskTypeName,STimeEstimater(fInitalTimePerUnitEstimate)));
	else
	{
		// szTaskTypeName was already known
		STimeEstimater &ref = it->second;

		// todo: this might change
		assert(ref.m_fTimeEstimate==fInitalTimePerUnitEstimate);
	}

	return true;
}

uint32 CGlobalTaskScheduler::GetTaskStatus( void *pObjectIdentifier )
{
	uint32 dwTaskIndex = FindTaskIndex(pObjectIdentifier);
	
	if(dwTaskIndex == 0xffffffff)
		return 0;

	STaskEntry *pTask = GetTask(dwTaskIndex);			assert(pTask);

	return (pTask->m_fMinDelayTime>0) ? 1 : 2;
}


float CGlobalTaskScheduler::GetEstimatedTimePerUnit( void *pObjectIdentifier )
{
	uint32 dwTaskIndex = FindTaskIndex(pObjectIdentifier);
	
	if(dwTaskIndex == 0xffffffff)
		return 0;

	STaskEntry *pTask = GetTask(dwTaskIndex);			assert(pTask);

	return pTask->ComputeCurrentWorkloadPerUnit(*this);
}


uint32 CGlobalTaskScheduler::StartQuery( void *pObjectIdentifier )
{
	assert(m_dwStartedQueryIndex==0xffffffff);			// you need to call FinishQuery() before

	uint32 dwRet=0;
	
	uint32 dwTaskIndex = FindTaskIndex(pObjectIdentifier);

	STaskEntry *pTask = GetTask(dwTaskIndex);
	
	if(!pTask)
		return 0;		// not registered - this shouldn't happen too open - this means redundant queries - just for convenience

	pTask->m_bWasQueriedThisFrame=true;

	if(!pTask->ConsiderExecution())
		return 0;

	float fUnits = pTask->m_fScheduledUnitsThisFrame;

	dwRet = (uint32)fUnits;

	if(dwRet>pTask->m_dwUnitsLeft)
	{
		dwRet = pTask->m_dwUnitsLeft;
		pTask->m_fError = 0.5f;
	}
	else pTask->m_fError = fUnits-(float)dwRet;


	// todo: start measure time

	if(dwRet)
		m_dwStartedQueryIndex=dwTaskIndex;

	pTask->m_dwUnitsLeft -= dwRet;

	return dwRet;
}





void CGlobalTaskScheduler::FinishQuery()
{
	assert(m_dwStartedQueryIndex!=0xffffffff);

	STaskEntry *pTask = GetTask(m_dwStartedQueryIndex);

	if(!pTask->m_dwUnitsLeft)
		RemoveTask(m_dwStartedQueryIndex);


	m_dwStartedQueryIndex=0xffffffff;
/*
	// todo: end measure time with m_TimeEstimater
	float fMeasuredTime=0.1f;

	std::map<const char *, STimeEstimater>::iterator it = m_TimeEstimater.find(szTaskTypeName);

	assert(it!=rScheduler.m_TimeEstimater.end());

	const STimeEstimater &rTimeEstimater =  it->second;

	rTimeEstimater.UpdateSample(fMeasuredTime);
	*/
}


uint32 CGlobalTaskScheduler::FindTaskIndex( void *pObjectIdentifier )
{
	std::vector<STaskEntry>::iterator it, end=m_GlobalTasks.end();
	uint32 dwI=0;

	for(it=m_GlobalTasks.begin();it!=end;++it,++dwI)
	{
		STaskEntry &ref = *it;

		if(ref.m_pObjectIdentifier==pObjectIdentifier)
			return dwI;
	}

	return 0xffffffff;		// not found
}

CGlobalTaskScheduler::STaskEntry *CGlobalTaskScheduler::GetTask( const uint32 dwIndex )
{
	uint32 dwSize = (uint32)m_GlobalTasks.size();

	if(dwIndex<dwSize)
		return &m_GlobalTasks[dwIndex];
	 else
		return 0;
}

static float frac( const float f )
{
	return f-floor(f);
}



float CGlobalTaskScheduler::GetCurrentScheduledWorkload()
{
	return m_fCurrentScheduledWorkload;
}


void CGlobalTaskScheduler::Update( const float fFrameTime )
{
	assert(fFrameTime>0);

	m_fFrameTime=fFrameTime;

	float fTimeScheduled;
	float fCurrentWorkload = ComputeFullWorkload(fTimeScheduled);

	float fWorkloadThisFrame = (fTimeScheduled>0.0f) ? fFrameTime/fTimeScheduled*fCurrentWorkload : fCurrentWorkload;

	// update all tasks
	m_fCurrentScheduledWorkload = 0.0f;
	{
		std::vector<STaskEntry>::iterator it, end=m_GlobalTasks.end();

		for(it=m_GlobalTasks.begin();it!=end;++it)
		{
			STaskEntry &ref = *it;

			m_fCurrentScheduledWorkload += ref.Update(*this);
		}
	}

	// m_fCurrentScheduledWorkload might be different from fCurrentWorkload because of task execution is atomic

	// add more tasks if they are in the budget
	for(;;)
	{
		// find one to add
		STaskEntry *pBest=0;
		float fBestValue=FLT_MAX;
		float fUnitWorkload=0.0f;

		{
			std::vector<STaskEntry>::iterator it, end=m_GlobalTasks.end();

			for(it=m_GlobalTasks.begin();it!=end;++it)
			{
				STaskEntry &ref = *it;

				if(!ref.ConsiderExecution())
					continue;

				if(ref.m_fRequestedUnitsThisFrame<0.001f)
					continue;

				float fLocalUnit = ref.ComputeCurrentWorkloadPerUnit(*this);

				float fAlpha = frac(ref.m_fScheduledUnitsThisFrame);
				float fTimeTillLastUnit = fAlpha / ref.m_fRequestedUnitsThisFrame;				assert(fTimeTillLastUnit>=0.0f);

				if(fTimeTillLastUnit<fBestValue)
				{
					fBestValue=fTimeTillLastUnit;
					fUnitWorkload=fLocalUnit;
					pBest=&ref;
				}
			}
		}

		if(!pBest)
			break;
		
		// only if that bring is nearer to the optimum
		float fCurrentDist = abs(m_fCurrentScheduledWorkload-fWorkloadThisFrame);
		float fPossibleDist = abs((m_fCurrentScheduledWorkload-fUnitWorkload)-fWorkloadThisFrame);
		
		if(fPossibleDist>=fCurrentDist)
			return;		// not betting better so better to stop

		pBest->m_fScheduledUnitsThisFrame -= 1.0f;

		if(pBest->m_fScheduledUnitsThisFrame<0.0f)
			pBest->m_fScheduledUnitsThisFrame=0.0f;

		m_fCurrentScheduledWorkload -= fUnitWorkload;
	}
}

float CGlobalTaskScheduler::ComputeFullWorkload( float &fOutTimeScheduled ) const
{
	float fRet=0.0f;
	std::vector<STaskEntry>::const_iterator it, end=m_GlobalTasks.end();

	fOutTimeScheduled=0.0f;

	for(it=m_GlobalTasks.begin();it!=end;++it)
	{
		const STaskEntry &ref = *it;

		fRet += ref.ComputeFullWorkload(*this);
		fOutTimeScheduled += ref.m_fMaxDelayTime-ref.m_fMinDelayTime;
	}

	if(!m_GlobalTasks.empty())
		fOutTimeScheduled/=(uint32)m_GlobalTasks.size();

	return fRet;
}


void CGlobalTaskScheduler::Reset()
{
	m_GlobalTasks.clear();
	m_dwStartedQueryIndex=0xffffffff;
	m_fFrameTime=0.0f;
	m_fCurrentScheduledWorkload=0.0f;
}

void CGlobalTaskScheduler::RemoveTask( const uint32 dwIndex )
{
	uint32 dwSize = (uint32)m_GlobalTasks.size();

	assert(dwIndex<dwSize);

	if(dwIndex!=dwSize-1)
	{
		// copy last over this one

		m_GlobalTasks[dwIndex]=m_GlobalTasks[dwSize-1];
	}

	m_GlobalTasks.pop_back();
}
