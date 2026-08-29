////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   frameprofilerender.cpp
//  Version:     v1.00
//  Created:     24/6/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Rendering of ThreadProfile.
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ThreadProfiler.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CryRenderer/IRenderAuxGeom.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryThread.h"
#include "CryCommon/CrySystem/ITimer.h"

#ifdef THREAD_SAMPLER

#include "ThreadSampler.h"
#include "ThreadSampler.cpp"

namespace
{
	int profile_threads = 0;
}

CThreadProfiler::CThreadProfiler()
{
	m_bStarted = false;
	m_pSampler = 0;

	// Register console var.
	gEnv->pConsole->Register("profile_threads",&profile_threads,0,0,
		"Enables Threads Profiler (should be deactivated for final product)\n"
		"The C++ function CryThreadSetName(threadid,\"Name\") needs to be called on thread creation.\n"
		"o=off, 1=only active thready, 2+=show all threads\n"
		"Threads profiling may not work on all combinations of OS and CPUs (does not not on Win64)\n"
		"Usage: profile_threads [0/1/2+]");
}

CThreadProfiler::~CThreadProfiler()
{
	SAFE_DELETE(m_pSampler);
}


//////////////////////////////////////////////////////////////////////////
void CThreadProfiler::Render()
{
	if (profile_threads != 0 && !m_bStarted)
	{
		Start();
		return;
	}
	if (profile_threads == 0 && m_bStarted)
	{
		Stop();
		return;
	}

	if (!m_bStarted)
		return;
	if (!m_pSampler)
		return;

	int width = gEnv->pRenderer->GetWidth();
	int height = gEnv->pRenderer->GetHeight();
	gEnv->pRenderer->Set2DMode(true,width,height);

	IRenderAuxGeom *pAux = gEnv->pRenderer->GetIRenderAuxGeom();

	float x1 = 0;
	float y1 = 0;
	float x2 = 100;
	float y2 = 100;

	float graph_offset = 100;

	ColorB col(255,0,255,255);

	float fTextSize = 1.2f;
	float colThreadName[4] = { 1,1,0,1 };
	float colInfo1[4] = { 0,1,1,1 };

	float base_y = 0;
	float szy = 18;

	DWORD processId = GetCurrentProcessId();

	static TThreadSampler::TSpanList spans;
	spans.resize(0);

	base_y = height - (m_nDisplayedThreads)*szy - 5;

	DWORD contextSwitches;
	if (m_pSampler->MakeSnapshot(&contextSwitches)==true)
	{
		//gEnv->pRenderer->Draw2dLabel( 1,base_y,fTextSize,colInfo1,false,"Context Switches per sec:%d",contextSwitches );
		//base_y += szy*2;
	}

	char str[128];

	float timeNow = gEnv->pTimer->GetAsyncCurTime();

	m_nDisplayedThreads = 0;
	for (int ti = 0; ti < (int)m_pSampler->threads.size()+1; ti++)
	{
		DWORD threadId = TThreadSampler::OTHER_THREADS;
		if (ti < (int)m_pSampler->threads.size())
		{
			threadId = m_pSampler->threads[ti];
		}
		float y = base_y + m_nDisplayedThreads*szy;

		ColorB clr(200,200,0,200);
		ColorB clrSpans[3];
		clrSpans[0] = ColorB(255,0,0,200);
		clrSpans[1] = ColorB(0,255,0,200);
		clrSpans[2] = ColorB(0,255,255,200);

		//ColorB clrSpanThread = clrSpans[ti%3];
		ColorB clrSpanThread = clrSpans[0];

		DWORD totalTime = 0;
		spans.resize(0);
		m_pSampler->CreateSpanListForThread( processId,threadId, spans, width-graph_offset, 1, &totalTime);

		if ((!spans.empty() || totalTime > 0) && ti < (int)m_pSampler->threads.size())
		{
			m_lastActive[ti] = gEnv->pTimer->GetAsyncCurTime();
		}

		//gEnv->pRenderer->Draw2dLabel( 1,y,fTextSize,colInfo1,false,"(%dms)",totalTime );
		if (threadId != TThreadSampler::OTHER_THREADS)
		{
			// Hide threads that are not active for 10 seconds.
			if ((timeNow - m_lastActive[ti]) > 10 && profile_threads == 1)
				continue;

			strcpy( str,CryThreadGetName(threadId) );
			if (str[0] == 0)
			{
				sprintf( str,"Thread%d-%d",ti, threadId );
			}
			gEnv->pRenderer->Draw2dLabel( 1,y,fTextSize,colThreadName,false,"%s, %d(ms)",str,totalTime );
		}
		else
		{
			float clrGray[4] = {180,180,180,255};
			gEnv->pRenderer->Draw2dLabel( 1,y,fTextSize,clrGray,false,"Others, %d(ms)",totalTime );
			
			clr = ColorB(180,180,180,200);
			clrSpanThread = ColorB(180,180,180,200);
		}


		float ly = y + szy/2.0f;
		pAux->DrawLine( Vec3(graph_offset,ly,0),clr,Vec3(width,ly,0),clr );
		pAux->DrawLine( Vec3(graph_offset,ly-8,0),clr,Vec3(graph_offset,ly+8,0),clr );
		for (int i = 0; i < (int)spans.size(); i++)
		{
			float start = graph_offset + spans[i].start;
			float end = graph_offset + spans[i].end;
			Vec3 quad[4];
			quad[0] = Vec3(start,ly-8,1);
			quad[1] = Vec3(end,  ly-8,1);
			quad[2] = Vec3(end,  ly+8,1);
			quad[3] = Vec3(start,ly+8,1);

			ColorB clrSpan = clrSpanThread;
			/*
			int clrstep = i%3;
			ColorB clrSpan = clrSpanThread;
			if (threadId != TThreadSampler::OTHER_THREADS)
			{
				if (clrSpan.r > 0)
					clrSpan.r -= (clrstep)*10;
				if (clrSpan.g > 0)
					clrSpan.g -= (clrstep)*10;
				if (clrSpan.b > 0)
					clrSpan.b -= (clrstep)*10;
			}
			*/

			//pAux->DrawLine( Vec3(spans[i].start,ly+1,0),clr,Vec3(spans[i].end,ly+1,0),clr );
			pAux->DrawTriangle( quad[0],clrSpan,quad[2],clrSpan,quad[1],clrSpan );
			pAux->DrawTriangle( quad[0],clrSpan,quad[3],clrSpan,quad[2],clrSpan );
		}
		m_nDisplayedThreads++;
	}

	gEnv->pRenderer->Set2DMode( false,0,0 );
}

void CThreadProfiler::Start()
{
	m_bStarted = true;
	m_pSampler = new TThreadSampler;
	m_pSampler->EnumerateThreads( GetCurrentProcessId() );
	m_lastActive.resize(m_pSampler->threads.size());
	m_nDisplayedThreads = (int)m_pSampler->threads.size() + 1;
}

void CThreadProfiler::Stop()
{
	m_bStarted = false;
	SAFE_DELETE(m_pSampler);
}


#endif // THREAD_SAMPLER
