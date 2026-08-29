////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   ThreadTask.cpp
//  Version:     v1.00
//  Created:     19/09/2006 by Timur.
//  Compilers:   Visual Studio 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ThreadTask.h"
#include "CPUDetect.h"
#include "System.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif //WIN32

namespace
{

};


//////////////////////////////////////////////////////////////////////////
CThreadTask_Thread::CThreadTask_Thread( CThreadTaskManager *pTaskMgr,const char *sName,int nIndex )
{
	m_pTaskManager = pTaskMgr;
	m_sThreadName = sName;
	bStopThread = false;
	bRunning = false;
	m_hThreadHandle = 0;
	m_nThreadIndex = nIndex;
}

//////////////////////////////////////////////////////////////////////////
void CThreadTask_Thread::SingleUpdate()
{
	for (size_t i = 0,numTasks = tasks.size(); i < numTasks; i++)
	{
		tasks[i]->pTask->OnUpdate();
		
		if (bStopThread)
			break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CThreadTask_Thread::Run()
{
// Name this thread.
#ifdef WIN32
	HANDLE hThread = GetCurrentThread();
	m_hThreadHandle = hThread;
	
	CryThreadSetName( GetCurrentThreadId(),m_sThreadName.c_str() );

	if (m_nThreadIndex > 0)
	{
		//SetThreadAffinityMask( GetCurrentThread(),(1 << m_nThreadIndex) );
		DWORD_PTR mask1,mask2;
		GetProcessAffinityMask( GetCurrentProcess(),&mask1,&mask2 );
		// Reserve CPU 1 for main thread.
		SetThreadAffinityMask( GetCurrentThread(),(mask1 & (~1)) );
	}
#endif

	bRunning = true;
	while (!bStopThread)
	{
		AUTO_LOCK(m_tasksLock);
		m_waitForTasksLock.Lock();
		while (tasks.empty() && !bStopThread)
			m_waitForTasks.Wait(m_waitForTasksLock);
		
		if (!bStopThread)
		{
			SingleUpdate();
		}

		m_waitForTasksLock.Unlock();
	}
	bRunning = false;
}

//////////////////////////////////////////////////////////////////////////
void CThreadTask_Thread::Stop()
{
	bStopThread = true;
	m_waitForTasks.Notify();
}

//////////////////////////////////////////////////////////////////////////
void CThreadTask_Thread::AddTask( SThreadTaskInfo *pTaskInfo )
{
	AUTO_LOCK(m_tasksLock);
	pTaskInfo->pThread = this;

	m_waitForTasksLock.Lock();
	tasks.push_back(pTaskInfo);
	m_waitForTasks.Notify();
	m_waitForTasksLock.Unlock();
}

//////////////////////////////////////////////////////////////////////////
void CThreadTask_Thread::RemoveTask( SThreadTaskInfo *pTaskInfo )
{
	AUTO_LOCK(m_tasksLock);

	for (int j = 0,numtasks = (int)tasks.size(); j < numtasks; j++)
	{
		if (tasks[j] == pTaskInfo)
		{
			pTaskInfo->pThread = 0;
			tasks.erase( tasks.begin()+j );
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CThreadTask_Thread::RemoveAllTasks()
{
	AUTO_LOCK(m_tasksLock);
	for (int j = 0,numtasks = (int)tasks.size(); j < numtasks; j++)
	{
		tasks[j]->pThread = 0;
	}
	tasks.clear();
}

//////////////////////////////////////////////////////////////////////////
CThreadTaskManager::CThreadTaskManager()
{
	m_nMaxThreads = MAX_TASK_THREADS_COUNT;
	
	SetThreadName( GetCurrentThreadId(),"Main" );
}

//////////////////////////////////////////////////////////////////////////
CThreadTaskManager::~CThreadTaskManager()
{
	CloseThreads();
}

//////////////////////////////////////////////////////////////////////////
void CThreadTaskManager::StopAllThreads()
{
	if (m_threads.empty())
		return;

	size_t i;
	// Start from 2nd thread, 1st is main thread.
	for (i = 1; i < m_threads.size(); i++)
	{
		CThreadTask_Thread *pThread  = m_threads[i];
		pThread->Stop();
	}
	bool bAllStoped = true;
	do
	{
		bAllStoped = true;
		CrySleep(10);
		for (i = 1; i < m_threads.size(); i++)
		{
			CThreadTask_Thread *pThread = m_threads[i];
			if (pThread->bRunning)
				bAllStoped = false;
		}
	}
	while (!bAllStoped);
}


//////////////////////////////////////////////////////////////////////////
void CThreadTaskManager::CloseThreads()
{
	if (m_threads.size() > 0)
		StopAllThreads();
	for (size_t i = MAIN_THREAD_INDEX,numThreads = m_threads.size(); i < numThreads; i++)
	{
		delete m_threads[i];
	}
	m_threads.clear();
}

//////////////////////////////////////////////////////////////////////////
void CThreadTaskManager::InitThreads()
{
	CloseThreads();

	// Create a dummy thread that is used for main thread.
	m_threads.resize(1);
	m_threads[0] = new CThreadTask_Thread(this,"Main Thread",0);

	//Ignore for now.
	return;

	CCpuFeatures *pCPU = ((CSystem*)gEnv->pSystem)->GetCPUFeatures();

	int nThreads = min( (int)m_nMaxThreads,(int)pCPU->GetCPUCount() );
	if (nThreads < 1)
		nThreads = 1;
	int nAddThreads = nThreads-1;
	
	char str[32];
	m_threads.resize(1+nAddThreads);
	for (int i = 0; i < nAddThreads; i++)
	{
		int nIndex = i+1;
		sprintf( str,"TaskThread%d",i );
		m_threads[nIndex] = new CThreadTask_Thread(this,str,nIndex);
		m_threads[nIndex]->Start();
	}
	RescheduleTasks();
}

//////////////////////////////////////////////////////////////////////////
void CThreadTaskManager::SetMaxThreadCount( int nMaxThreads )
{
	if (nMaxThreads == m_nMaxThreads)
		return;

	m_nMaxThreads = nMaxThreads;

	bool bReallocateThreads = false;
	if (m_nMaxThreads < m_threads.size())
	{
		bReallocateThreads = true;
	}
	if (m_nMaxThreads > m_threads.size())
	{
		CCpuFeatures *pCPU = ((CSystem*)gEnv->pSystem)->GetCPUFeatures();
		if (m_threads.size() < pCPU->GetCPUCount())
			bReallocateThreads = true;
	}
	if (bReallocateThreads)
	{
		CloseThreads();
		InitThreads();
	}
}

//////////////////////////////////////////////////////////////////////////
SThreadTaskInfo* CThreadTaskManager::FindTaskInfo( IThreadTask *pTask )
{
	for (int i = 0,num = (int)m_tasks.size(); i < num; i++)
	{
		if (m_tasks[i]->pTask == pTask)
			return m_tasks[i];
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CThreadTaskManager::RegisterTask( IThreadTask *pTask,const SThreadTaskParams &options )
{
	if (FindTaskInfo(pTask) != NULL)
	{
		// already registered.
		return;
	}
	SThreadTaskInfo *pTaskInfo = new SThreadTaskInfo;
	pTaskInfo->pTask = pTask;
	pTaskInfo->params = options;
	m_tasks.push_back(pTaskInfo);
}


//////////////////////////////////////////////////////////////////////////
void CThreadTaskManager::UnregisterTask( IThreadTask *pTask )
{
	SThreadTaskInfo *pTaskInfo = FindTaskInfo(pTask);
	if (!pTaskInfo)
	{
		// not registered.
		return;
	}
	// Remove from thread.
	if (pTaskInfo->pThread)
	{
		pTaskInfo->pThread->RemoveTask( pTaskInfo );
	}
	{
		// Remove from tasks.
		for (int i = 0,num = (int)m_threads.size(); i < num; i++)
		{
			if (m_tasks[i] == pTaskInfo)
			{
				m_tasks.erase( m_tasks.begin()+i );
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CThreadTaskManager::ScheduleTask( SThreadTaskInfo *pTaskInfo )
{
	int i;

	if (pTaskInfo->pThread)
		pTaskInfo->pThread->RemoveTask(pTaskInfo);

	CThreadTask_Thread *pGoodThread = 0;

	if (pTaskInfo->params.nPreferedThread >= 0 && pTaskInfo->params.nPreferedThread < m_threads.size())
	{
		// Assign task to desired thread.
		pGoodThread = m_threads[pTaskInfo->params.nPreferedThread];
	}
	else
	{
		// Find available thread for the task.
		for (i = MAIN_THREAD_INDEX+1; i < (int)m_threads.size(); i++)
		{
			CThreadTask_Thread *pThread = m_threads[i];
			if (pThread->tasks.size() == 0)
			{
				pGoodThread = pThread;
				break;
			}
		}
	}
	if (!pGoodThread)
	{
		// Assign to last thread.
		pGoodThread = m_threads[m_threads.size()-1];
	}
	
	pGoodThread->AddTask( pTaskInfo );
}

//////////////////////////////////////////////////////////////////////////
void CThreadTaskManager::RescheduleTasks()
{
	int i;

	// Un-schedule all tasks.
	for (i = 0; i < (int)m_tasks.size(); i++)
	{
		if (m_tasks[i]->pThread)
			m_tasks[i]->pThread->RemoveTask( m_tasks[i] );
	}
	for (i = 0; i < (int)m_tasks.size(); i++)
	{
		ScheduleTask( m_tasks[i] );
	}
}

//////////////////////////////////////////////////////////////////////////
void CThreadTaskManager::OnUpdate()
{
	FUNCTION_PROFILER_FAST( GetISystem(),PROFILE_SYSTEM,g_bProfilerEnabled );

	// Emulate single update of the main thread.
	if (m_threads[0])
		m_threads[0]->SingleUpdate();
}

struct THREADNAME_INFO
{
	DWORD dwType;
	LPCSTR szName;
	DWORD dwThreadID;
	DWORD dwFlags;
};

//////////////////////////////////////////////////////////////////////////
void CThreadTaskManager::SetThreadName( unsigned int dwThreadId,const char *sThreadName )
{
#if defined(WIN32) || defined(XENON)
	if (dwThreadId == -1)
	{
		dwThreadId = GetCurrentThreadId();
	}

	//////////////////////////////////////////////////////////////////////////
	// Rise exception to set thread name for debugger.
	//////////////////////////////////////////////////////////////////////////
	THREADNAME_INFO threadName;
	threadName.dwType = 0x1000;
	threadName.szName = sThreadName;
	threadName.dwThreadID = dwThreadId;
	threadName.dwFlags = 0;

	__try
	{
		RaiseException( 0x406D1388, 0, sizeof(threadName)/sizeof(DWORD), (ULONG_PTR*)&threadName );
	}
	__except (EXCEPTION_CONTINUE_EXECUTION)
	{
	}
#endif

	m_threadNames[dwThreadId] = sThreadName;
}

//////////////////////////////////////////////////////////////////////////
const char* CThreadTaskManager::GetThreadName( unsigned int dwThreadId )
{
	ThreadNames::const_iterator it = m_threadNames.find(dwThreadId);
	if (it != m_threadNames.end())
		return it->second.c_str();
	return "";
}