
//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
//	
//	File:Timer.h
//
//	History:
//	01/31/2001 Created by Marco Corbetta
//  08/03/2003 MartinM improved precision (intern from float to int64)
//
//////////////////////////////////////////////////////////////////////

#ifndef TIMER_H
#define TIMER_H

#if _MSC_VER > 1000
# pragma once
#endif

#include "CryCommon/CrySystem/ITimer.h"						// ITimer

#define FPS_FRAMES 16
#define MAX_FRAME_AVERAGE 100


#define TIME_PROFILE_PARAMS 256

// Implements all common timing routines
class CTimer : public ITimer
{
public:
	// constructor
	CTimer();
	// destructor
	~CTimer() {};

	bool Init( ISystem *pSystem );

	// interface ITimer ----------------------------------------------------------

	// TODO: Review m_time usage in System.cpp / SystemRender.cpp
	//       if it wants Game Time / UI Time or a new Render Time?

	virtual void ResetTimer();		
	virtual void UpdateOnFrameStart();
	virtual float GetCurrTime(ETimer which = ETIMER_GAME) const;
	virtual CTimeValue GetFrameStartTime(ETimer which = ETIMER_GAME) const;
	virtual CTimeValue GetAsyncTime() const;
	virtual float GetAsyncCurTime();
	virtual float GetFrameTime(ETimer which = ETIMER_GAME) const;
	virtual float GetRealFrameTime() const;
	virtual float GetTimeScale() const;
	virtual void SetTimeScale(float scale);
	virtual void EnableTimer( const bool bEnable );
	virtual float	GetFrameRate();
	virtual float GetProfileFrameBlending( float* pfBlendTime = 0, int* piBlendMode = 0 );
	virtual void Serialize(TSerialize ser);
	virtual bool IsTimerEnabled() const;

	//! try to pause/unpause a timer
	//  returns true if successfully paused/unpaused, false otherwise
	virtual bool PauseTimer(ETimer which, bool bPause);

	//! determine if a timer is paused
	//  returns true if paused, false otherwise
	virtual bool IsTimerPaused(ETimer which);

	//! try to set a timer
	//  return true if successful, false otherwise
	virtual bool SetTimer(ETimer which, float timeInSeconds);

	//! make a tm struct from a time_t in UTC (like gmtime)
	virtual void SecondsToDateUTC(time_t time, struct tm& outDateUTC);

	//! make a UTC time from a tm (like timegm, but not available on all platforms)
	virtual time_t DateToSecondsUTC(struct tm& timePtr);

private: // ---------------------------------------------------------------------
	
	typedef int64 (*TimeUpdateFunc) ();				//  absolute, in microseconds,

	// ---------------------------------------------------------------------------

	// updates m_CurrTime (either pass m_lCurrentTime or custom curTime)
	void RefreshGameTime(int64 curTime);
	void RefreshUITime(int64 curTime);
	void UpdateBlending();
	f32 GetAverageFrameTime(f32 sec, f32 FrameTime, f32 LastAverageFrameTime); 
		
	TimeUpdateFunc	m_pfnUpdate;								// pointer to the timer function (performance counter or timegettime)
	int64						m_lBaseTime;								// absolute in ticks, 1 sec = m_lTicksPerSec units
	int64						m_lLastTime;								// absolute in ticks, 1 sec = m_lTicksPerSec units, needed to compute frame time
	int64           m_lOffsetTime;              // relative in ticks; offsets frame time by some amount so we can reset it independently of AsyncTime

	int64						m_lTicksPerSec;							// units per sec

	int64						m_lCurrentTime;							// absolute, in microseconds, at the CTimer:Update() call
	float						m_fFrameTime;								// in game seconds since the last update
	float						m_fRealFrameTime;						// in real seconds since the last update

	CTimeValue			m_CurrTime[ETIMER_LAST+1];  // absolute time at UpdateOnFrameStart()
	int64           m_lForcedGameTime;
	bool						m_bEnabled;									//
	// smoothing

	float						m_previousTimes[FPS_FRAMES];//
	float						m_arrFrameTimes[MAX_FRAME_AVERAGE];
	float						m_fAverageFrameTime;
	int							m_timecount;								//

	float						m_fProfileBlend;						// current blending amount for profile.
	float						m_fAvgFrameTime;						// for weighting.

	//////////////////////////////////////////////////////////////////////////
	// Console vars.
	//////////////////////////////////////////////////////////////////////////
	float						m_fixed_time_step;					// in seconds
	float						m_max_time_step;
  float           m_time_scale;               // slow down time (or theoretically speed it up)
  int							m_e_time_smoothing;					// [0..FPS_FRAMES-2] optional smoothing, 0=no, x=x additional steps,  

	// Profile averaging help.
	float						m_profile_smooth_time;			// seconds to exponentially smooth profile results.
	int							m_profile_weighting;				// weighting mode (see RegisterVar desc).

	ISystem *				m_pSystem;									// 	
	int             m_debug;

	unsigned	int		m_nFrameCounter;

	bool            m_bGameTimerPaused;
	int64           m_lGameTimerPausedTime;
};





#endif //timer