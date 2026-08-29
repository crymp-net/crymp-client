#include "CryCommon/CrySystem/IGlobalTaskScheduler.h"
#pragma once

#include <vector>					// STL vector<>
#include <map>						// STL map<>
#include <cmath>					// ceil()

class CGlobalTaskScheduler :public IGlobalTaskScheduler
{
public:
	// constructor
	CGlobalTaskScheduler();

	// interface IGlobalTaskScheduler ---------------------------------------------

	virtual bool AddTask( void *pObjectIdentifier, const char *szTaskTypeName,
		const float fMinDelayTime, const float fMaxDelayTime, const uint32 dwUnits, const float fInitalTimePerUnitEstimate );
	virtual uint32 StartQuery( void *pObjectIdentifier );
	virtual void FinishQuery();
	virtual void Update( const float fFrameTime );
	virtual void Reset();
	virtual float GetEstimatedTimePerUnit( void *pObjectIdentifier );
	virtual uint32 GetTaskStatus( void *pObjectIdentifier );
	virtual float GetCurrentScheduledWorkload();

private: // ---------------------------------------------------------------------

	struct STaskEntry
	{
		// constructor
		STaskEntry( void *pObjectIdentifier, const char *szTaskTypeName, const float fMinDelayTime,
			const float fMaxDelayTime, const uint32 dwUnits )
			:m_pObjectIdentifier(pObjectIdentifier), m_szTaskTypeName(szTaskTypeName),
			m_fMinDelayTime(fMinDelayTime), m_fMaxDelayTime(fMaxDelayTime),
			m_dwUnitsLeft(dwUnits), m_bWasQueriedThisFrame(true), m_fError(0.5f),
			m_fScheduledUnitsThisFrame(0), m_fRequestedUnitsThisFrame(0)
		{
			assert(m_fMinDelayTime>=0.0f);
			assert(m_fMaxDelayTime>0.0f);
		}

		// called every frame
		// Returns:
		//   scheduled units * EstimatedTime
		float Update( const CGlobalTaskScheduler &rScheduler )
		{
			if(!m_bWasQueriedThisFrame)
			{
				assert(0);		// in each frame AddTask() needs to be called after StartQuery(), for open task StartQuery() needs to be called every frame

				// todo: error printout

				return 0;				// task is kept but not updated -> acceptable behaviour in non debug
			}

			// update timing
			{
				m_fMinDelayTime-=rScheduler.m_fFrameTime;

				if(m_fMinDelayTime<0.0f)
					m_fMinDelayTime=0.0f;

				m_fMaxDelayTime-=rScheduler.m_fFrameTime;

				if(m_fMaxDelayTime<0.0f)
					m_fMaxDelayTime=0.0f;
			}
		
			m_bWasQueriedThisFrame=false;

			float fCurrentWorkloadPerUnit = ComputeCurrentWorkloadPerUnit(rScheduler);
			float fCurrentWorkload = m_dwUnitsLeft * fCurrentWorkloadPerUnit;

			// schedule task itself
			{
				float fFrames = m_fMaxDelayTime/rScheduler.m_fFrameTime;
				
				m_fRequestedUnitsThisFrame = (fFrames>1.0f) ? m_dwUnitsLeft/fFrames : m_dwUnitsLeft;

				m_fScheduledUnitsThisFrame = m_fRequestedUnitsThisFrame + m_fError;			assert(m_fScheduledUnitsThisFrame>=0.0f);
			}

			return fCurrentWorkloadPerUnit * (uint32)m_fScheduledUnitsThisFrame;
		}

		bool ConsiderExecution() const
		{
			return m_dwUnitsLeft!=0 && m_fMinDelayTime<=0;
		}

		float ComputeCurrentWorkloadPerUnit( const CGlobalTaskScheduler &rScheduler ) const
		{
			if(m_fMinDelayTime>0.0f)
				return 0;

			std::map<const char *, STimeEstimater>::const_iterator it = rScheduler.m_TimeEstimater.find(m_szTaskTypeName);

			assert(it!=rScheduler.m_TimeEstimater.end());

			const STimeEstimater &rTimeEstimater = it->second;

			return rTimeEstimater.m_fTimeEstimate;
		}

		float ComputeFullWorkload( const CGlobalTaskScheduler &rScheduler ) const
		{
			return m_dwUnitsLeft * ComputeCurrentWorkloadPerUnit(rScheduler);
		}


		// ---------------------------------------------------------------

		void *					m_pObjectIdentifier;				// to identify the task
		const char *		m_szTaskTypeName;						// this class only keeps the pointer as long the instance exist, there is no deallocation
		float						m_fMinDelayTime;						// in seconds
		float						m_fMaxDelayTime;						// in seconds
		uint32					m_dwUnitsLeft;							//
		float						m_fError;										// as result needs to discreet (count) we make an error we need to accumulate to compensite later
		bool						m_bWasQueriedThisFrame;			// to detect if the system wasn't used the right way, QueryUpdateCount should be called for every task every frame
		float						m_fScheduledUnitsThisFrame;	// updated after each Update()
		float						m_fRequestedUnitsThisFrame;	//
	};

	// helper class to have smooth (over n frames) time estimates
	struct STimeEstimater
	{
		// constructor
		STimeEstimater( float fInitalTimeEstimate ) :m_fTimeEstimate(fInitalTimeEstimate), m_dwTimeSamples(0),m_fCurrentSampleSum(0.0f)
		{
		}

		void UpdateSample( const float fMeasuredSample )
		{
			m_fCurrentSampleSum+=fMeasuredSample;
			++m_dwTimeSamples;

			const uint32 dwSmoothRange=8;

			if(m_dwTimeSamples>=dwSmoothRange)
			{
				m_fTimeEstimate=m_fCurrentSampleSum/dwSmoothRange;
				m_fCurrentSampleSum=0;
				m_dwTimeSamples=0;
			}
		}

		float				m_fTimeEstimate;							//

	private: // ----------------------------

		uint32			m_dwTimeSamples;							//
		float				m_fCurrentSampleSum;					//
	};

	// ------------------------------------------------------------------------
	
	std::vector<STaskEntry>											m_GlobalTasks;								//
	std::map<const char *, STimeEstimater>			m_TimeEstimater;							//
	uint32																			m_dwStartedQueryIndex;				// index into m_GlobalTasks, 0xffffffff if no query is started
	float																				m_fFrameTime;									// in seconds
	float																				m_fCurrentScheduledWorkload;	// for this frame

	// ------------------------------------------------------------------------

	// O(n), n=m_GlobalTasks.size()
	// Return:
	//   0xffffffff is the task wasn't found, index otherwise
	uint32 FindTaskIndex( void *pObjectIdentifier );

	// Arguments:
	//   dwIndex - can be even 0xffffffff, then result is 0
	// Returns:
	//   0 in the case the index is out of range
	STaskEntry *GetTask( const uint32 dwIndex );

	// Arguments:
	//   fOutTimeScheduled - 
	float ComputeFullWorkload( float &fOutTimeScheduled ) const;

	void RemoveTask( const uint32 dwIndex );

	friend struct STaskEntry;
};
