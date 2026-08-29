////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   ThreadTask.h
//  Version:     v1.00
//  Created:     19/09/2006 by Timur.
//  Compilers:   Visual Studio 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////


#ifndef __thread_task_h__
#define __thread_task_h__

#include "CryCommon/CrySystem/IThreadTask.h"
#include "CryThread.h"
#include "CritSection.h"

#define MAIN_THREAD_INDEX 0

class CThreadTask_Thread;

struct SThreadTaskInfo : public _i_reference_target_t
{
	IThreadTask *pTask;
	CThreadTask_Thread *pThread;
	SThreadTaskParams params;

	SThreadTaskInfo() : pTask(0),pThread(0) { params.nFlags = 0; params.nPreferedThread = -1; }
};

class CThreadTaskManager;
///
struct IThreadTaskThread
{
	virtual void Run() = 0;
	virtual void Cancel() = 0;
};
//////////////////////////////////////////////////////////////////////////
class CThreadTask_Thread : public CryThread<IThreadTaskThread>
{
public:
	CThreadTask_Thread( CThreadTaskManager *pTaskMgr,const char *sName,int nThreadIndex );
	
	virtual void Run();
	virtual void Cancel() {};

	void AddTask( SThreadTaskInfo *pTaskInfo );
	void RemoveTask( SThreadTaskInfo *pTaskInfo );
	void RemoveAllTasks();
	void Stop();
	void SingleUpdate();

public:
	CThreadTaskManager *m_pTaskManager;
	string m_sThreadName;
	int m_nThreadIndex;

	THREAD_HANDLE m_hThreadHandle;

	// Tasks running on this thread.
	std::vector<_smart_ptr<SThreadTaskInfo> > tasks;

	CCritSection m_tasksLock;
	typedef CryCondLock<CRYLOCK_FAST> MyLock;
	MyLock m_waitForTasksLock;
	CryCond<MyLock> m_waitForTasks;

	// Set to true when thread must stop.
	volatile bool bStopThread;
	volatile bool bRunning;
};

//////////////////////////////////////////////////////////////////////////
class CThreadTaskManager : public IThreadTaskManager
{
public:
	CThreadTaskManager();
	~CThreadTaskManager();

	void InitThreads();
	void CloseThreads();

	void StopAllThreads();

	//////////////////////////////////////////////////////////////////////////
	// IThreadTaskManager
	//////////////////////////////////////////////////////////////////////////
	virtual void RegisterTask( IThreadTask *pTask,const SThreadTaskParams &options );
	virtual void UnregisterTask( IThreadTask *pTask );
	virtual void SetMaxThreadCount( int nMaxThreads );
	virtual void SetThreadName( unsigned int dwThreadId,const char *sThreadName );
	virtual const char* GetThreadName( unsigned int dwThreadId );
	//////////////////////////////////////////////////////////////////////////

	// This is on update function of the main thread.
	void OnUpdate();

private:
	SThreadTaskInfo* FindTaskInfo( IThreadTask *pTask );

	void ScheduleTask( SThreadTaskInfo *pTaskInfo );
	void RescheduleTasks();

private:
	typedef std::vector<CThreadTask_Thread*> Threads;
	// Physical threads available to system.
	Threads m_threads;

	typedef std::vector<_smart_ptr<SThreadTaskInfo> > Tasks;
	// All active tasks.
	Tasks m_tasks;

	typedef std::map<uint32,string> ThreadNames;
	ThreadNames m_threadNames;


	// Max threads that can be executed at same time.
	int m_nMaxThreads;
};


#endif __thread_task_h__