
//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
//	
//	File:Timer.cpp
//  Description: Implementation of the timer class: timing functions
//
//	History:
//	-Jan 31,2001:Created by Marco Corbetta
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Timer.h"
#include <time.h>
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/CryNetwork/ISerialize.h"
/////////////////////////////////////////////////////

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "Mmsystem.h"
#endif

//this should not be included here
#include "CryCommon/CryRenderer/IRenderer.h"									// needed for m_pSystem->GetIRenderer()->EF_Query(EFQ_RecurseLevel)

//#define PROFILING 1
#ifdef PROFILING
static int64 g_lCurrentTime = 0;
#endif

//! Smoothing time in seconds (original default was .8 / log(10) ~= .35 s)
#define SMOOTHING_TIME	0.35f

/////////////////////////////////////////////////////
// get the timer in microseconds. using QueryPerformanceCounter
static int64 GetPerformanceCounterTime()
{
#ifdef PROFILING
	return g_lCurrentTime;
#endif

	LARGE_INTEGER lNow;

	QueryPerformanceCounter(&lNow);

	// for the following calculation even 64bit integer isn't enough
	//	int64 lCurTime=(lNow.QuadPart*TIMEVALUE_PRECISION) / m_lTicksPerSec;
	//	int64 lCurTime = (int64)((double)lNow.QuadPart / m_lTicksPerMicroSec);

	return lNow.QuadPart;
}



#ifdef WIN32
// get the timer in microseconds. using winmm
static int64 GetMMTime()
{		
	int64 lNow=timeGetTime();

	return lNow;
}
#endif // WIN32


#define DEFAULT_FRAME_SMOOTHING 1
/////////////////////////////////////////////////////
CTimer::CTimer() 
{
	m_pfnUpdate=NULL;
	m_lBaseTime=0;
	m_lLastTime=0;
	m_lTicksPerSec=0;
	m_lCurrentTime=0;
	m_fFrameTime=0;
	m_fRealFrameTime=0;
	m_lOffsetTime=0;

	m_fixed_time_step = 0;
	m_max_time_step = 0.25f;
  m_time_scale = 1.0f;

	// note: frame numbers (old version - commented out) are not used but is based on time
	m_e_time_smoothing = DEFAULT_FRAME_SMOOTHING; 
	
// smoothing
	m_timecount=0;
	for (int k=0;k<FPS_FRAMES;k++) 
   m_previousTimes[k]=0;

	m_fAverageFrameTime=1.0f/30.0f;	//lets hope the game will run with 30 frames on average
	for (int i=0; i<MAX_FRAME_AVERAGE; i++) 
		m_arrFrameTimes[i]=m_fAverageFrameTime; 
	
	m_profile_smooth_time = SMOOTHING_TIME;
	m_profile_weighting = 0;
	m_fAvgFrameTime = 0;

	m_bEnabled = true;
	m_debug = 0;
	m_lGameTimerPausedTime = 0;
	m_bGameTimerPaused = false;
	m_lForcedGameTime = -1;

	m_nFrameCounter=0;

	LARGE_INTEGER TTicksPerSec;

	if(QueryPerformanceFrequency(&TTicksPerSec))
	{ 
		// performance counter is available, use it instead of multimedia timer
		LARGE_INTEGER t;
		QueryPerformanceCounter( &t );
		m_lTicksPerSec=TTicksPerSec.QuadPart;
		m_pfnUpdate = &GetPerformanceCounterTime;
		ResetTimer();
	}
	else
	{ 
#ifdef WIN32
		//Use MM timer if unable to use the High Frequency timer
		m_lTicksPerSec=1000;
		m_pfnUpdate = &GetMMTime;
		ResetTimer();
#endif // WIN32
	}
}

/////////////////////////////////////////////////////
bool CTimer::Init(ISystem *pSystem)
{
	m_pSystem=pSystem;
	m_pSystem->GetIConsole()->Register("e_time_smoothing",&m_e_time_smoothing,DEFAULT_FRAME_SMOOTHING,0,
		"0=no smoothing, 1=smoothing");	// FPS_FRAMES-1=15
	m_pSystem->GetIConsole()->Register("fixed_time_step",&m_fixed_time_step,0,0,
		"Game updated with this fixed frame time" );
	m_pSystem->GetIConsole()->Register("max_time_step",&m_max_time_step,0.25f,0,
		"Game systems clamped to this frame time" );
  m_pSystem->GetIConsole()->Register("time_scale",&m_time_scale,1.0f,0,
		"Game time scaled by this - for variable slow motion" );

	m_pSystem->GetIConsole()->Register("profile_smooth",&m_profile_smooth_time,SMOOTHING_TIME,0,
		"Profiler exponential smoothing interval (seconds).\n" );
	m_pSystem->GetIConsole()->Register("profile_weighting",&m_profile_weighting,0,0,
		"Profiler smoothing mode: 0 = legacy, 1 = average, 2 = peak weighted, 3 = peak hold.\n" );

	// debugging
	m_pSystem->GetIConsole()->Register("e_timer_debug",&m_debug,0,0, "Timer debug" );

	return true;
}


/////////////////////////////////////////////////////
float CTimer::GetCurrTime(ETimer which) const 
{
	return m_CurrTime[(int)which].GetSeconds();
}


/////////////////////////////////////////////////////
float CTimer::GetFrameTime(ETimer which) const 
{ 
	if(!m_bEnabled) 
		return 0.0f; 

	if (which != ETIMER_GAME || m_bGameTimerPaused == false)
		return m_fFrameTime;
	return 0.0f;
} 

/////////////////////////////////////////////////////
CTimeValue CTimer::GetFrameStartTime(ETimer which) const
{
	return m_CurrTime[(int)which];
}

/////////////////////////////////////////////////////
float CTimer::GetRealFrameTime() const 
{ 
	if(!m_bEnabled) 
		return 0.0f; 

	return m_fRealFrameTime;   
} 

/////////////////////////////////////////////////////
float CTimer::GetTimeScale() const 
{ 
	return m_time_scale;   
} 

/////////////////////////////////////////////////////
void CTimer::SetTimeScale(float scale)
{ 
	m_time_scale = scale;
} 

/////////////////////////////////////////////////////
float CTimer::GetAsyncCurTime()  
{ 
	assert(m_pfnUpdate);		// call Init() before

	int64 llNow=(*m_pfnUpdate)()-m_lBaseTime;
	double dVal=(double)llNow;
	float fRet=(float)(dVal/(double)(m_lTicksPerSec));

	return fRet; 
}

/////////////////////////////////////////////////////
float CTimer::GetFrameRate()
{
	// Use real frame time.
	if (m_fRealFrameTime != 0.f)
		return 1.f / m_fRealFrameTime;
	return 0.f;
}

void CTimer::UpdateBlending()
{
	// Accumulate smoothing time up to specified max.
	float fSmoothTime = min(m_profile_smooth_time, m_CurrTime[ETIMER_GAME].GetSeconds());

	// Blend current frame into blended previous.
	m_fProfileBlend = fSmoothTime > 0.f ? 1.f - exp(-m_fRealFrameTime / fSmoothTime) : 1.f;
	if (m_profile_weighting >= 3)
	{
		// Decay avg frame time, set as new peak.
		m_fAvgFrameTime *= 1.f - m_fProfileBlend;
		if (m_fRealFrameTime > m_fAvgFrameTime)
		{
			m_fAvgFrameTime = m_fRealFrameTime;
			m_fProfileBlend = 1.f;
		}
		else
			m_fProfileBlend = 0.f;
	}
	else
	{
		// Track average frame time.
		m_fAvgFrameTime += m_fProfileBlend * (m_fRealFrameTime - m_fAvgFrameTime);
		if (m_profile_weighting == 1)
			// Weight all frames evenly.
			m_fProfileBlend = fSmoothTime > 0.f ? 1.f - exp(-m_fAvgFrameTime / fSmoothTime) : 1.f;
	}
}

float CTimer::GetProfileFrameBlending( float* pfBlendTime, int* piBlendMode )
{
	if (piBlendMode)
		*piBlendMode = m_profile_weighting;
	if (pfBlendTime)
	{
		if (*pfBlendTime < m_profile_smooth_time)
		{
			// Convert blend.
			if (*pfBlendTime <= 0.f || m_fProfileBlend >= 1.f)
				return 1.f;
			float fExp = logf(1.f - m_fProfileBlend);
			return 1.f - exp(fExp * m_profile_smooth_time / *pfBlendTime);
		}
		else
			*pfBlendTime = m_profile_smooth_time;
	}
	return m_fProfileBlend;
}

/////////////////////////////////////////////////////
void CTimer::RefreshGameTime(int64 curTime)
{
	double dVal=(double) ( curTime + m_lOffsetTime );
	
	//	m_fCurrTime=(float)(dVal/(double)(m_lTicksPerSec));

	// can be done much better

	m_CurrTime[ETIMER_GAME].SetSeconds((float)(dVal/(double)(m_lTicksPerSec)));

	assert(dVal>=0);
}

/////////////////////////////////////////////////////
void CTimer::RefreshUITime(int64 curTime)
{
	double dVal=(double) ( curTime );

	//	m_fCurrTime=(float)(dVal/(double)(m_lTicksPerSec));

	// can be done much better

	m_CurrTime[ETIMER_UI].SetSeconds((float)(dVal/(double)(m_lTicksPerSec)));

	assert(dVal>=0);
}


/////////////////////////////////////////////////////
void CTimer::UpdateOnFrameStart()
{
	if(!m_bEnabled)
		return;

	assert(m_pfnUpdate);		// call Init() before

	if ((m_nFrameCounter & 127)==0)
	{
		// every bunch of frames, check frequency to adapt to
		// CPU power management clock rate changes
		LARGE_INTEGER TTicksPerSec;
		if (QueryPerformanceFrequency(&TTicksPerSec))
		{ 
			// if returns false, no performance counter is available
			m_lTicksPerSec=TTicksPerSec.QuadPart;
		}
	}

	m_nFrameCounter++;

#ifdef PROFILING
	m_fFrameTime = 0.020f; // 20ms = 50fps
	g_lCurrentTime += (int)(m_fFrameTime*(float)(CTimeValue::TIMEVALUE_PRECISION));
	m_lCurrentTime = g_lCurrentTime;
	m_lLastTime = m_lCurrentTime;
	RefreshGameTime(m_lCurrentTime);
	RefreshUITime(m_lCurrentTime);
	return;
#endif
 
	int64 now = (*m_pfnUpdate)();

	if (m_lForcedGameTime >= 0)
	{
		// m_lCurrentTime contains the time, which should be current
		// but time has passed since Serialize until UpdateOnFrameStart
		// so we have to make sure to compensate!
		m_lOffsetTime = m_lForcedGameTime - now + m_lBaseTime;
		if (m_debug)
			CryLogAlways("[CTimer] PostSerialization: Frame=%d Cur=%lld Now=%lld Off=%lld Async=%f CurrTime=%f UI=%f", gEnv->pRenderer->GetFrameID(false), (long long)m_lCurrentTime, (long long)now, (long long)m_lOffsetTime, GetAsyncCurTime(), GetCurrTime(ETIMER_GAME), GetCurrTime(ETIMER_UI));
		m_lLastTime = now - m_lBaseTime;
		m_lForcedGameTime = -1;
	}

	// Real time. 
	m_lCurrentTime = now - m_lBaseTime;

	//m_fRealFrameTime = m_fFrameTime = (float)(m_lCurrentTime-m_lLastTime) / (float)(m_lTicksPerSec);
	m_fRealFrameTime = m_fFrameTime = (float)((double)(m_lCurrentTime-m_lLastTime) / (double)(m_lTicksPerSec));

	if( 0 != m_fixed_time_step) // Absolute zero used as a switch. looks wrong, but runs right ;-)
	{
		if (m_fixed_time_step < 0)
		{
			// Enforce real framerate by sleeping.
			float sleep = -m_fixed_time_step - m_fFrameTime;
			if (sleep > 0)
			{
				CrySleep((unsigned int)(sleep*1000.f));
				m_lCurrentTime = (*m_pfnUpdate)() - m_lBaseTime;
				//m_fRealFrameTime = (float)(m_lCurrentTime-m_lLastTime) / (float)(m_lTicksPerSec);
				m_fRealFrameTime = (float)((double)(m_lCurrentTime-m_lLastTime) / (double)(m_lTicksPerSec));				
			}
		}
		m_fFrameTime = abs(m_fixed_time_step);
	}
	else
	{
		// Clamp it
		m_fFrameTime = min(m_fFrameTime, m_max_time_step);

		// Dilate it.
		m_fFrameTime *= m_time_scale;
	}

	if (m_fFrameTime < 0) 
		m_fFrameTime = 0;



//-----------------------------------------------

	if(m_e_time_smoothing>0)
	{
	/*
		if(m_e_time_smoothing>FPS_FRAMES-1)
			m_e_time_smoothing=FPS_FRAMES-2;

		if(m_fFrameTime < 0.0000001f)
			m_fFrameTime = 0.0000001f;

		m_previousTimes[m_timecount] = m_fFrameTime;

		m_timecount++;
		if(m_timecount>=m_e_time_smoothing)
			m_timecount=0;

		// average multiple frames together to smooth changes out a bit
		float total = 0;
		for(int i = 0 ; i < m_e_time_smoothing+1; i++ ) 
			total += m_previousTimes[i];	
	    
		if(!total) 
			total = 1;

		m_fFrameTime=total/(float)(m_e_time_smoothing+1);
*/
		m_fAverageFrameTime = GetAverageFrameTime( 0.25f, m_fFrameTime, m_fAverageFrameTime); 
		m_fFrameTime=m_fAverageFrameTime;
	}

	//else

	{	
		// Adjust.
		if (m_fFrameTime != m_fRealFrameTime)
		{
			float fBefore = GetAsyncCurTime();
			int64 nAdjust = (int64)((m_fFrameTime - m_fRealFrameTime) * (double)(m_lTicksPerSec));
			m_lCurrentTime += nAdjust;
			m_lBaseTime -= nAdjust;
			if (m_debug)
				CryLogAlways("[CTimer]: Frame=%d Adj: FrameTime=%f Real=%f (delta=%.8f) -> AsyncCurTime=%f (was %f)", gEnv->pRenderer->GetFrameID(false), m_fFrameTime, m_fRealFrameTime, (m_fFrameTime - m_fRealFrameTime), GetAsyncCurTime(), fBefore);
		}
	}

	RefreshUITime(m_lCurrentTime);
	if (m_bGameTimerPaused == false)
		RefreshGameTime(m_lCurrentTime);

	m_lLastTime = m_lCurrentTime;

	if (m_fRealFrameTime < 0.f)
		// Can happen after loading a saved game and 
		// at any time on dual core AMD machines and laptops :)
		m_fRealFrameTime = 0.0f;

	UpdateBlending();

	/* Check for AsyncCurTime. Should never jump back! */
#if defined(USER_alexl)
	static float sAsync = GetAsyncCurTime();
	static int64 sOldBaseTime = m_lBaseTime;
	const float async = GetAsyncCurTime();
	if (async < sAsync)
	{
		assert (false);
		CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,"[CTimer] Frame=%d AsyncCurTime: Time Jump Detected! LastTime=%f CurTime=%f", gEnv->pRenderer->GetFrameID(false), sAsync, async );
		CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,"[CTimer]   OldBase=%lld  NewBase=%lld", (long long)sOldBaseTime, (long long)m_lBaseTime);
	}
	sAsync = async;
#endif

	if (m_debug)
		CryLogAlways("[CTimer]: Frame=%d Cur=%lld Now=%lld Off=%lld Async=%f CurrTime=%f UI=%f", gEnv->pRenderer->GetFrameID(false), (long long)m_lCurrentTime, (long long)now, (long long)m_lOffsetTime, GetAsyncCurTime(), GetCurrTime(ETIMER_GAME), GetCurrTime(ETIMER_UI));
}

//------------------------------------------------------------------------
//--  average frame-times to avoid stalls and peaks in framerate
//--    note that is is time-base averaging and not frame-based
//------------------------------------------------------------------------
f32 CTimer::GetAverageFrameTime(f32 sec, f32 FrameTime, f32 LastAverageFrameTime) 
{ 
	uint32 numFT=	MAX_FRAME_AVERAGE;
	for (int32 i=(numFT-2); i>-1; i--)
		m_arrFrameTimes[i+1] = m_arrFrameTimes[i];

	if (FrameTime>0.4f) FrameTime = 0.4f;
	if (FrameTime<0.0f) FrameTime = 0.0f;
	m_arrFrameTimes[0] = FrameTime;

	//get smoothed frame
	uint32 avrg_ftime = 1;
	if (LastAverageFrameTime)
	{
		avrg_ftime = uint32(sec/LastAverageFrameTime+0.5f); //average the frame-times for a certain time-period (sec)
		if (avrg_ftime>numFT)	avrg_ftime=numFT;
		if (avrg_ftime<1)	avrg_ftime=1;
	}

	f32 AverageFrameTime=0;
	for (uint32 i=0; i<avrg_ftime; i++)	
		AverageFrameTime += m_arrFrameTimes[i];
	AverageFrameTime /= avrg_ftime;

	//don't smooth if we pause the game
	if (FrameTime<0.0001f)
		AverageFrameTime=FrameTime;
	
//	g_YLine+=66.0f;
//	float fColor[4] = {1,0,1,1};
//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"AverageFrameTime: keys:%d   time:%f %f", avrg_ftime ,AverageFrameTime,sec/LastAverageFrameTime);	
//	g_YLine+=16.0f;

	return AverageFrameTime;
}


/////////////////////////////////////////////////////
void CTimer::ResetTimer()
{
	assert(m_pfnUpdate);		// call Init() before

	m_lBaseTime = (*m_pfnUpdate)();
	m_lLastTime = m_lCurrentTime = 0;
	m_fFrameTime = 0;
	RefreshGameTime(m_lCurrentTime);
	RefreshUITime(m_lCurrentTime);
	m_lForcedGameTime = -1;
	m_bGameTimerPaused = false;
	m_lGameTimerPausedTime = 0;
}

/////////////////////////////////////////////////////
void CTimer::EnableTimer( const bool bEnable ) 
{ 
	m_bEnabled = bEnable; 
}

bool CTimer::IsTimerEnabled() const
{
	return m_bEnabled;
}

/////////////////////////////////////////////////////
CTimeValue CTimer::GetAsyncTime() const
{
//	assert(m_pfnUpdate);		// call Init() before
	
	if(!m_pfnUpdate)		// call Init() before
		return CTimeValue();

	int64 llNow=(*m_pfnUpdate)();

	// can be done much better
	double fac=(double)CTimeValue::TIMEVALUE_PRECISION/(double)m_lTicksPerSec;

	return CTimeValue(int64(llNow*fac));
}




void CTimer::Serialize( TSerialize ser )
{
	// cannot change m_lBaseTime, as this is used for async time (which shouldn't be affected by save games)
	if (ser.IsWriting())
	{
		// why we have to do this: see comment in PauseTimer
		// (m_lCurrentTime+m_lOffsetTime) might not reflect the current game time
		// if there had been no UpdateOnFrameStart yet
		int64 currentGameTime = 0;
		if (m_lForcedGameTime >= 0)
			currentGameTime = m_lForcedGameTime;
		else
			currentGameTime = m_lCurrentTime + m_lOffsetTime;

		ser.Value("curTime", currentGameTime);
		ser.Value("ticksPerSecond", m_lTicksPerSec);
	}
	else
	{
		int64 ticksPerSecond = 1, curTime = 1;
		ser.Value("curTime", curTime);
		ser.Value("ticksPerSecond", ticksPerSecond);
		curTime = (int64)((long double)curTime * m_lTicksPerSec / ticksPerSecond);
		m_lOffsetTime = 0; // will be corrected in UpdateOnFrameStart
		m_lForcedGameTime = curTime;
		if(m_lGameTimerPausedTime && m_bGameTimerPaused)
			m_lGameTimerPausedTime = curTime;
		RefreshGameTime(m_lForcedGameTime);
		if (m_debug)
		{
			int64 now = m_pfnUpdate ? (*m_pfnUpdate)() : 0;
			CryLogAlways("[CTimer]: Serialize: Frame=%d Cur=%lld Now=%lld Off=%lld Async=%f CurrTime=%f UI=%f", gEnv->pRenderer->GetFrameID(false), (long long)m_lCurrentTime, (long long)now, (long long)m_lOffsetTime, GetAsyncCurTime(), GetCurrTime(ETIMER_GAME), GetCurrTime(ETIMER_UI));
		}
	}
}

//! try to pause/unpause a timer
//  returns true if successfully paused/unpaused, false otherwise
bool CTimer::PauseTimer(ETimer which, bool bPause)
{
	if (which != ETIMER_GAME)
		return false;

	if (m_bGameTimerPaused == bPause)
		return false;

	if (bPause)
	{
		// if we have not yet had an UpdateOnFrameStart call
		// this can happen
		// Frame I: Pause On
		// Frame I+2: Pause Off
		// Frame I+2: Pause On  (no UpdateOnFrameStart, so m_lForcedGameTime contains the to-be-set time,
		//                       and m_lCurrentTime+m_lOffsetTime is wrong, because m_lOffsetTime is 0
		//                       as m_lOffsetTime was set to 0 during unpausing and was not yet corrected
		//                       in UpdateOnFrameStart.
		//                       Maybe we should call UpdateOnFrameStart internally or at least when we Unpause
		//                       int64 now = (*m_pfnUpdate)();
		//                       m_lOffsetTime = m_lForcedGameTime - now + m_lBaseTime;
		if (m_lForcedGameTime >= 0)
			m_lGameTimerPausedTime = m_lForcedGameTime;
		else
			m_lGameTimerPausedTime = m_lCurrentTime + m_lOffsetTime;
		if (m_debug)
			CryLogAlways("[CTimer]: Pausing ON: Frame=%d Cur=%lld Off=%lld Async=%f CurrTime=%f UI=%f", gEnv->pRenderer->GetFrameID(false), (long long)m_lCurrentTime, (long long)m_lOffsetTime, GetAsyncCurTime(), GetCurrTime(ETIMER_GAME), GetCurrTime(ETIMER_UI));
	}
	else
	{
		if (m_lGameTimerPausedTime > 0)
		{
			m_lOffsetTime = 0;
			RefreshGameTime(m_lGameTimerPausedTime);
			m_lForcedGameTime = m_lGameTimerPausedTime;
			if (m_debug)
				CryLogAlways("[CTimer]: Pausing OFF: Frame=%d Cur=%lld Off=%lld Async=%f CurrTime=%f UI=%f", gEnv->pRenderer->GetFrameID(false), (long long)m_lCurrentTime, (long long)m_lOffsetTime, GetAsyncCurTime(), GetCurrTime(ETIMER_GAME), GetCurrTime(ETIMER_UI));
		}
		m_lGameTimerPausedTime = 0;
	}
	m_bGameTimerPaused = bPause;
	return true;
}

//! determine if a timer is paused
//  returns true if paused, false otherwise
bool CTimer::IsTimerPaused(ETimer which)
{
	if (which != ETIMER_GAME)
		return false;
	return m_bGameTimerPaused;
}

//! try to set a timer
//  return true if successful, false otherwise
bool CTimer::SetTimer(ETimer which, float timeInSeconds)
{
	if (which != ETIMER_GAME)
		return false;
	m_lOffsetTime = 0;
	m_lForcedGameTime = (int64) ((double)(timeInSeconds)*(double)(m_lTicksPerSec));
	RefreshGameTime(m_lForcedGameTime);
	return true;
}

void CTimer::SecondsToDateUTC(time_t inTime, struct tm& outDateUTC)
{
	outDateUTC = *gmtime(&inTime);
}

#if defined (WIN32) || defined(WIN64) || defined(XENON)
time_t gmt_to_local_win32(void)
{
	TIME_ZONE_INFORMATION tzinfo;
	DWORD dwStandardDaylight;
	long bias;

	dwStandardDaylight = GetTimeZoneInformation(&tzinfo);
	bias = tzinfo.Bias;

	if (dwStandardDaylight == TIME_ZONE_ID_STANDARD)
		bias += tzinfo.StandardBias;

	if (dwStandardDaylight == TIME_ZONE_ID_DAYLIGHT)
		bias += tzinfo.DaylightBias;

	return (- bias * 60);
}
#endif

#if defined (PS3)
#include <cell/rtc.h>
#endif

#if !defined __CRYCG__
time_t CTimer::DateToSecondsUTC(struct tm& inDate)
{
#if defined (WIN32) || defined(XENON)
	return mktime(&inDate) + gmt_to_local_win32();
#elif defined (LINUX)
#if defined (HAVE_TIMEGM)
	// return timegm(&inDate);
#else
	// craig: temp disabled the +tm.tm_gmtoff because i can't see the intention here
	// and it doesn't compile anymore
	// alexl: tm_gmtoff is the offset to greenwhich mean time, whereas mktime uses localtime
	//        but not all linux distributions have it...
	return mktime(&inDate) /*+ tm.tm_gmtoff*/;
#endif
#elif defined (PS3)
	CellRtcDateTime dateTime;
	dateTime.year = inDate.tm_year+1900;
	dateTime.month = inDate.tm_mon+1;
	dateTime.day = inDate.tm_mday;
	dateTime.hour = inDate.tm_hour;
	dateTime.minute = inDate.tm_min;
	dateTime.second = inDate.tm_sec;
	dateTime.microsecond = 0;
	time_t t;
	cellRtcGetTime_t(&dateTime, &t);
	return t;
#else
	return mktime(&inDate);
#endif
}
#endif // !__CRYCG__

