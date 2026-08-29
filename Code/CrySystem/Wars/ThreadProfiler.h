////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ThreadProfiler.h
//  Version:     v1.00
//  Created:     24/6/2003 by Timur,Sergey,Wouter.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ThreadProfiler_h__
#define __ThreadProfiler_h__
#pragma once

#include "CryCommon/CryCore/platform.h"
#include "CryCommon/CrySystem/FrameProfiler.h"

#if defined(USE_FRAME_PROFILER) && defined(WIN32) && !defined(WIN64)
#define THREAD_SAMPLER
#endif

#ifdef THREAD_SAMPLER
//////////////////////////////////////////////////////////////////////////
//! the system which does the gathering of stats
class CThreadProfiler
{
public:
	CThreadProfiler();
	~CThreadProfiler();

	void Start();
	void Stop();
	void Render();

private:
	bool m_bStarted;
	class TThreadSampler *m_pSampler;
	std::vector<float> m_lastActive;
	int m_nDisplayedThreads;
};

#else // THREAD_SAMPLER

class CThreadProfiler
{
public:
	void Start() {};
	void Stop() {};
	void Render() {};
};

#endif

#endif // __ThreadProfiler_h__
