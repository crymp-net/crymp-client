#include "CryCommon/Cry3DEngine/I3DEngine.h"
#include "CryCommon/CryCore/CrySizer.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryCommon/CryMath/Cry_Camera.h"
#include "CryCommon/CryPhysics/IPhysics.h"
#include "CryCommon/CrySoundSystem/ISoundMoodManager.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/ILog.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/ITimer.h"

#include "IAudioDevice.h"
#include "IPlatformSound.h"
#include "Sound.h"
#include "SoundSystem.h"

CSoundSystem* CSound::m_pSSys = NULL;

CSound::CSound(CSoundSystem* pSSys)
{
	assert(pSSys);
	m_pPlatformSound = NULL;
	m_pSoundBuffer = NULL;
	m_pEventGroupBuffer = NULL;
	m_pSSys = pSSys;
	m_nSoundID = INVALID_SOUNDID;

	Reset();
}

CSound::~CSound()
{
	m_bRefCountGuard = true;

	if (m_pSoundBuffer)
		m_pSoundBuffer->RemoveFromLoadReqList(this);

	// stop the sound
	if (m_pPlatformSound && m_pPlatformSound->GetSoundHandle() && m_bPlaying)
		Stop();

	if (m_pSSys->g_nDebugSound == SOUNDSYSTEM_DEBUG_SIMPLE)
	{
		gEnv->pLog->Log("<Sound> Destroying sound %s \n", GetName());
	}

	// tell enities that this sound will not be valid anymore
	OnEvent(SOUND_EVENT_ON_LOAD_FAILED);

	m_pSSys->RemoveReference(this);

	m_pPlatformSound = NULL;
	m_pSoundBuffer = NULL;
}

int CSound::Release()
{
	// reset the reference
	int ref = 0;
	if (!m_bRefCountGuard)
	{
		if ((ref = --m_refCount) == 0)
		{
			// #if defined(WIN64)// && defined(_DEBUG)
			//  Win64 has a special variable to disable deleting all the sounds
			// ICVar* pVar = gEnv->pConsole->GetCVar("s_EnableRelease");
			// if (!pVar || pVar->GetIVal())
			// #endif
			m_pSSys->RemoveSoundObject(this);
		}
	}

	return (ref);
};

void CSound::Reset()
{
	// SoundID is created here and removed by SoundSystem
	if (m_nSoundID == INVALID_SOUNDID)
		m_nSoundID = m_pSSys->CreateSoundID();

	m_RadiusSpecs.fOriginalMinRadius = 1.0f;
	m_RadiusSpecs.fOriginalMaxRadius = 1.0f;
	m_RadiusSpecs.fMaxRadius2 = 0.0f;
	m_RadiusSpecs.fMinRadius2 = 0.0f;
	m_RadiusSpecs.fPreloadRadius2 = 0.0f;
	m_RadiusSpecs.fMinRadius = 1.0f;
	m_RadiusSpecs.fMaxRadius = 10.0f;
	m_RadiusSpecs.fDistanceMultiplier = 1.0f;

	SetMinMaxDistance_i(m_RadiusSpecs.fMinRadius, m_RadiusSpecs.fMaxRadius);

	m_bPlaying = false;
	m_IsPaused = false;
	m_SoundProxyEntityId = 0;

	m_vPosition(0, 0, 0);
	m_vPlaybackPos(0, 0, 0);
	m_vSpeed(0, 0, 0);
	m_vDirection(0, 0, 0);
	m_fInnerAngle = 360.0f;
	m_fOuterAngle = 360.0f;

	m_eState = eSoundState_None;
	m_eFadeState = eFadeState_None;
	// m_fLastPlayTime.SetSeconds(0);

	m_eSemantic = eSoundSemantic_None;

	m_nSoundScaleGroups = SETSCALEBIT(SOUNDSCALE_MASTER) | SETSCALEBIT(SOUNDSCALE_SCALEABLE) |
	                      SETSCALEBIT(SOUNDSCALE_DEAFNESS) | SETSCALEBIT(SOUNDSCALE_MISSIONHINT);
	m_nStartOffset = 0;
	m_fActiveVolume = -1;

	m_fPitching = 0;
	m_nCurrPitch = 0;
	m_nRelativeFreq = 1000;
	m_nFxChannel = -1;
	m_fRatio = 1.0f;
	m_HDRA = 1.0f;
	m_nFadeTimeInMS = 0;
	m_fFadeGoal = 1.0f;
	m_fFadeCurrent = 1.0f;

	m_fMaxVolume = 1.0f;
	// m_fSoundLengthSec = 0;
	m_VoiceSpecs.fVolume = 1.0f;
	m_VoiceSpecs.fDucking = 0.0f;
	m_VoiceSpecs.fRadioRatio = 0.0f;
	m_VoiceSpecs.fRadioBackground = 0.0f;
	m_VoiceSpecs.fRadioSquelch = 0.0f;

	// Timur: Never addref on creation.
	m_refCount = 0;
	m_pVisArea = NULL;
	m_bRecomputeVisArea = true;

	// m_fFadingValue		= 1.0f; ///5.0f; // by default
	// m_fCurrentFade		= 1.0f;
	m_nSoundPriority = 0;
	m_nFlags = 0;

	m_fPostLoadRatio = 0;
	m_bPlayAfterLoad = false;
	m_eStopMode = ESoundStopMode_EventFade;
	m_bPostLoadForceActiveState = false;
	m_bPostLoadSetRatio = false;
	m_bVolumeHasChanged = true;
	m_bFlagsChanged = false;
	m_bPositionHasChanged = true;
	m_bLoopModeHasChanged = false;
	m_bRefCountGuard = false;

	m_bLineSound = false;
	m_bSphereSound = false;
	m_fSphereRadius = 0;

	m_tPlayTime.SetSeconds(-1.0f);
	m_fSyncTimeout = -1.0f;

	// TODO deactivated because m_pPlatformSound is not availible yet

	// Obstruction
	m_fLastObstructionUpdate = 0.0f;
	m_Obstruction.nRaysShot = 0;
	m_Obstruction.bAssigned = false;
	m_Obstruction.bProcessed = false;
	m_Obstruction.bDelayPlayback = true;
	m_Obstruction.bFirstTime = false;
	m_Obstruction.ResetDirect();
	m_Obstruction.ResetReverb();

	if (!m_listeners.empty())
	{
		OnEvent(SOUND_EVENT_ON_STOP);
		m_listeners.clear();
	}

	m_listenersToBeRemoved.clear();

	if (m_pPlatformSound)
		m_pPlatformSound->FreeSoundHandle();

	m_pPlatformSound = NULL;

	if (m_pSoundBuffer)
		m_pSoundBuffer->RemoveFromLoadReqList(this);

	if (m_pEventGroupBuffer)
		m_pEventGroupBuffer->RemoveFromLoadReqList(this);

	m_pSoundBuffer = NULL;
	m_pEventGroupBuffer = NULL;
	m_sSoundName = "";
}

void CSound::SetPhysicsToBeSkipObstruction(EntityId* pSkipEnts, int nSkipEnts)
{
	for (int i = 0; i < nSkipEnts && i < SOUND_MAX_SKIPPED_ENTITIES; ++i)
		m_Obstruction.pOnstructionSkipEntIDs[i] = pSkipEnts[i];

	m_Obstruction.nObstructionSkipEnts = min(nSkipEnts, SOUND_MAX_SKIPPED_ENTITIES);
}

void CSound::CalculateObstruction()
{
	// start with half of before
	float fDirectOld = m_Obstruction.GetDirect();
	float fReverbOld = m_Obstruction.GetReverb();
	m_Obstruction.ResetDirect();
	m_Obstruction.ResetReverb();
	// m_Obstruction.AddDirect(fDirect);
	// m_Obstruction.AddReverb(fReverb);

	// run through and disable those testoftest which hit something
	// check how many valid ray we got
	int nValidRays = m_Obstruction.nRaysShot;
	for (int i = 0; i < m_Obstruction.nRaysShot; ++i)
	{
		SObstructionTest* pObstructionTest = &(m_Obstruction.ObstructionTests[i]);
		if (pObstructionTest->nTestForTest && pObstructionTest->nHits)
		{
			m_Obstruction.ObstructionTests[pObstructionTest->nTestForTest].bResult = false;
			--nValidRays;
		}
	}

	// the direct ray weights 50% of all rays for direct and
	float fAdditionalWeight = 0.5f;
	float fDirectWeight = 1.0f;

	if (m_Obstruction.nRaysShot > 1)
	{
		fAdditionalWeight = (1.0f / ((m_Obstruction.nRaysShot - 1) * 2.0f));
		fDirectWeight = 1.0f - (m_Obstruction.nRaysShot - 1) * fAdditionalWeight;
	}

	for (int i = 0; i < m_Obstruction.nRaysShot; ++i)
	{
		SObstructionTest* pObstructionTest = &(m_Obstruction.ObstructionTests[i]);
		// pObstructionTest->nPierceability = 0;

		if (pObstructionTest->bResult && pObstructionTest->nTestForTest == 0 && pObstructionTest->nHits)
		{
			// float fRatio = (m_RadiusSpecs.fMaxRadius -
			// sqrt(pObstructionTest->fDistance))/m_RadiusSpecs.fMaxRadius;
			float fRatio =
			    (m_RadiusSpecs.fMaxRadius - pObstructionTest->fDistance) / m_RadiusSpecs.fMaxRadius;

			if (m_pSSys->g_fObstructionMaxRadius > 0)
				fRatio *= (m_pSSys->g_fObstructionMaxRadius - m_RadiusSpecs.fMaxRadius) /
				          m_pSSys->g_fObstructionMaxRadius;

			fRatio = clamp(fRatio, 0.0f, 1.0f);

			float fPierecabilityRatio = 1.0f;

			if (m_pSSys->g_fObstructionMaxPierecability > 0)
				fPierecabilityRatio =
				    pObstructionTest->nPierceability / m_pSSys->g_fObstructionMaxPierecability;

			if (pObstructionTest->bDirect)
			{
				// m_Obstruction.AddDirect(fRatio*(pObstructionTest->nHits*0.3f)/(1.0f*(float)nValidRays));
				m_Obstruction.AddDirect(fPierecabilityRatio * fRatio * fDirectWeight);
				// m_Obstruction.AddReverb(fPierecabilityRatio* fRatio * 2.0f * fAdditionalWeight);
			}
			else
			{
				m_Obstruction.AddDirect(fPierecabilityRatio * fRatio * fAdditionalWeight);
				m_Obstruction.AddReverb(fPierecabilityRatio * fRatio * fAdditionalWeight * 2.0f);
			}
		}
	}

	// VisArea based Obstruction
	if (m_pSSys->g_fObstructionVisArea)
	{
		CListener* pListener =
		    (CListener*)m_pSSys->GetListener(m_pSSys->GetClosestActiveListener(GetPlaybackPosition()));
		if (pListener && pListener->bActive)
		{
			int nVisObstruction = 0;

			// both inside
			if (m_pVisArea != pListener->GetVisArea())
			{
				int nSearchDepth = 3;
				if (m_pVisArea && pListener->GetVisArea())
				{
					if (!m_pVisArea->FindVisArea(pListener->GetVisArea(), nSearchDepth, true))
					{
						nSearchDepth = 5;
						if (!m_pVisArea->FindVisArea(pListener->GetVisArea(), nSearchDepth,
						                             true))
						{
							nVisObstruction = 3;
						}
						else
							nVisObstruction = 2;
					}
					else
						nVisObstruction = 1;
				}
			}

			// Listener outside, sound inside
			if (m_pVisArea && !pListener->GetVisArea())
			{
				// just approximate
				if (m_pVisArea->IsConnectedToOutdoor())
					nVisObstruction = 1;
				else
					nVisObstruction = 2;
			}

			// Listener inside, sound outside
			if (!m_pVisArea && pListener->GetVisArea())
			{
				// just approximate
				if (pListener->GetVisArea()->IsConnectedToOutdoor())
					nVisObstruction = 1;
				else
					nVisObstruction = 2;
			}

			m_Obstruction.AddDirect(nVisObstruction * m_pSSys->g_fObstructionVisArea * 0.5f);
			m_Obstruction.AddReverb(nVisObstruction * m_pSSys->g_fObstructionVisArea);
		}
	}

	// m_Obstruction.fObstruction = (nHitsAccum*0.4f)/(1.0f*(float)m_Obstruction.nRaysShot);
	float fDirectNew = min(m_Obstruction.GetAccumulatedDirect(), m_pSSys->g_fObstructionMaxValue);
	float fReverbNew = min(m_Obstruction.GetAccumulatedReverb(), m_pSSys->g_fObstructionMaxValue);

	// first time do not average with old value
	if (m_Obstruction.bDontAveragePrevious)
	{
		fDirectOld = fDirectNew;
		fReverbOld = fReverbNew;
	}

	m_Obstruction.SetDirect((fDirectNew + fDirectOld) * 0.5f);
	m_Obstruction.SetReverb((fReverbNew + fReverbOld) * 0.5f);

	// if ( abs(fDirectOld-m_Obstruction.GetDirect()) > 0.01f || abs(fDirectOld-m_Obstruction.GetDirect()) )
	m_Obstruction.bAssigned = false;

	m_Obstruction.LastUpdateTime = gEnv->pTimer->GetFrameStartTime();
}

void CSound::ProcessObstruction(CListener* pListener, bool bImmediatly)
{
	assert(pListener != NULL);

	if (!m_pSSys->g_nObstruction)
	{
		m_Obstruction.bDelayPlayback = false;
		m_Obstruction.nRaysShot = 0;
		return;
	}

	if (!(m_nFlags & FLAG_SOUND_OBSTRUCTION) && !(m_nFlags & FLAG_SOUND_3D))
	{
		m_Obstruction.bDelayPlayback = false;
		return;
	}

	if (m_Obstruction.bProcessed)
	{
		m_Obstruction.bDelayPlayback = false;
		return;
	}

	Vec3 vSoundDir = m_vPlaybackPos - pListener->GetPosition();
	if (vSoundDir.IsZero() || (vSoundDir.GetLengthSquared() > m_RadiusSpecs.fMaxRadius2) && !bImmediatly)
	{
		m_Obstruction.bDelayPlayback = false;
		m_Obstruction.nRaysShot = 0;
		return;
	}

	CTimeValue currTime = gEnv->pTimer->GetFrameStartTime();
	int ntimeDiffMS = (int)((currTime - m_Obstruction.LastUpdateTime).GetMilliSeconds());

	if (abs(ntimeDiffMS) > 100)
	{

		// evaluate all results gathered till now
		CalculateObstruction();

		// if (m_Obstruction.nObstructionSkipEnts>0)
		// int test = 0;

		IPhysicalEntity* pSkipEnts[SOUND_MAX_SKIPPED_ENTITIES + 1];
		int nSkipEnts = 0;

		// add player skipent if valid, must be *FIRST* of other, ask Anton why
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pListener->nEntityID);
		if (pEntity)
		{
			IPhysicalEntity* pPhysicalEntity = pEntity->GetPhysics();
			if (pPhysicalEntity)
			{
				pSkipEnts[nSkipEnts] = pPhysicalEntity;
				++nSkipEnts;
			}
		}

		// test if SkipEnts still valid and add them
		if (m_Obstruction.nObstructionSkipEnts)
		{
			for (int i = 0; i < m_Obstruction.nObstructionSkipEnts; ++i)
			{
				pEntity = gEnv->pEntitySystem->GetEntity(m_Obstruction.pOnstructionSkipEntIDs[i]);
				if (pEntity)
				{
					IPhysicalEntity* pPhysicalEntity = pEntity->GetPhysics();
					if (pPhysicalEntity)
					{
						pSkipEnts[nSkipEnts] = pPhysicalEntity;
						++nSkipEnts;
					}
				}
			}
		}

		if (SOUND_MAX_OBSTRUCTION_TESTS < 9)
			return;

		int nObInx = 0;

		int nAccuracy = m_pSSys->g_nObstructionAccuracy;

		if (nAccuracy == 0)
		{
			m_Obstruction.bDelayPlayback = false;
			return;
		}

		Vec3 vSoundDirNorm = vSoundDir;
		vSoundDirNorm.normalize();

		if (nAccuracy > 0)
		{
			m_Obstruction.ObstructionTests[nObInx].vOrigin = pListener->GetPosition();
			m_Obstruction.ObstructionTests[nObInx].vDirection =
			    vSoundDir - (vSoundDirNorm * 0.01f); // shorten ray by a cm
			m_Obstruction.ObstructionTests[nObInx].bDirect = true;
			m_Obstruction.ObstructionTests[nObInx].bResult = true;
			++nObInx;
		}

		if (!bImmediatly)
		{
			float fRadius = sqrt(sqrt(m_RadiusSpecs.fMaxRadius2));
			// vSoundDir *= fRadius;
			assert(vSoundDirNorm.GetLength() != 0);

			if (nAccuracy > 1)
			{
				// test the sidetest
				m_Obstruction.ObstructionTests[nObInx].vOrigin = m_vPlaybackPos;
				m_Obstruction.ObstructionTests[nObInx].vDirection.x = vSoundDirNorm.y * fRadius;
				m_Obstruction.ObstructionTests[nObInx].vDirection.y = -vSoundDirNorm.x * fRadius;
				m_Obstruction.ObstructionTests[nObInx].vDirection.z = 0;
				m_Obstruction.ObstructionTests[nObInx].bDirect = false;
				m_Obstruction.ObstructionTests[nObInx].bResult = true;
				++nObInx;
				m_Obstruction.ObstructionTests[nObInx - 1].nTestForTest = nObInx;

				// test to the side
				m_Obstruction.ObstructionTests[nObInx].vOrigin = pListener->GetPosition();
				m_Obstruction.ObstructionTests[nObInx].vDirection =
				    m_Obstruction.ObstructionTests[nObInx - 1].vDirection +
				    m_Obstruction.ObstructionTests[0].vDirection;
				m_Obstruction.ObstructionTests[nObInx].bDirect = false;
				m_Obstruction.ObstructionTests[nObInx].bResult = true;
				++nObInx;
			}

			if (nAccuracy > 2)
			{
				// test the other sidetest
				m_Obstruction.ObstructionTests[nObInx].vOrigin = m_vPlaybackPos;
				m_Obstruction.ObstructionTests[nObInx].vDirection.x = -vSoundDirNorm.y * fRadius;
				m_Obstruction.ObstructionTests[nObInx].vDirection.y = vSoundDirNorm.x * fRadius;
				m_Obstruction.ObstructionTests[nObInx].vDirection.z = 0;
				m_Obstruction.ObstructionTests[nObInx].bDirect = false;
				m_Obstruction.ObstructionTests[nObInx].bResult = true;
				++nObInx;
				m_Obstruction.ObstructionTests[nObInx - 1].nTestForTest = nObInx;

				// test to the other side
				m_Obstruction.ObstructionTests[nObInx].vOrigin = pListener->GetPosition();
				m_Obstruction.ObstructionTests[nObInx].vDirection =
				    m_Obstruction.ObstructionTests[nObInx - 1].vDirection +
				    m_Obstruction.ObstructionTests[0].vDirection;
				m_Obstruction.ObstructionTests[nObInx].bDirect = false;
				m_Obstruction.ObstructionTests[nObInx].bResult = true;
				++nObInx;
			}

			if (nAccuracy > 3)
			{
				// test the uptest
				m_Obstruction.ObstructionTests[nObInx].vOrigin = m_vPlaybackPos;
				m_Obstruction.ObstructionTests[nObInx].vDirection.x = 0;
				m_Obstruction.ObstructionTests[nObInx].vDirection.y = 0;
				m_Obstruction.ObstructionTests[nObInx].vDirection.z = fRadius;
				m_Obstruction.ObstructionTests[nObInx].bDirect = false;
				m_Obstruction.ObstructionTests[nObInx].bResult = true;
				++nObInx;
				m_Obstruction.ObstructionTests[nObInx - 1].nTestForTest = nObInx;

				// test up
				m_Obstruction.ObstructionTests[nObInx].vOrigin = pListener->GetPosition();
				m_Obstruction.ObstructionTests[nObInx].vDirection =
				    m_Obstruction.ObstructionTests[nObInx - 1].vDirection +
				    m_Obstruction.ObstructionTests[0].vDirection;
				m_Obstruction.ObstructionTests[nObInx].bDirect = false;
				m_Obstruction.ObstructionTests[nObInx].bResult = true;
				++nObInx;
			}

			if (nAccuracy > 4)
			{
				// test the downtest
				m_Obstruction.ObstructionTests[nObInx].vOrigin = m_vPlaybackPos;
				m_Obstruction.ObstructionTests[nObInx].vDirection.x = 0;
				m_Obstruction.ObstructionTests[nObInx].vDirection.y = 0;
				m_Obstruction.ObstructionTests[nObInx].vDirection.z = -fRadius * 0.5f;
				m_Obstruction.ObstructionTests[nObInx].bDirect = false;
				m_Obstruction.ObstructionTests[nObInx].bResult = true;
				++nObInx;
				m_Obstruction.ObstructionTests[nObInx - 1].nTestForTest = nObInx;

				// test down
				m_Obstruction.ObstructionTests[nObInx].vOrigin = pListener->GetPosition();
				m_Obstruction.ObstructionTests[nObInx].vDirection =
				    m_Obstruction.ObstructionTests[nObInx - 1].vDirection +
				    m_Obstruction.ObstructionTests[0].vDirection;
				m_Obstruction.ObstructionTests[nObInx].bDirect = false;
				m_Obstruction.ObstructionTests[nObInx].bResult = true;
				++nObInx;
			}
		}

		// float fTempObstruction = m_Obstruction.fObstructionAccu;
		// m_Obstruction.fObstructionAccu *= 0.5f; // start with half of before

		int nHitsAccum = 0;
		m_Obstruction.nRaysShot = nObInx;
		unsigned int nPhysicsFlags = ent_water | ent_static | ent_sleeping_rigid | ent_rigid | ent_terrain;

		if (bImmediatly)
		{
			// taking first guess of obstruction
			// only one RWI on Timurs request

			SObstructionTest* pObstructionTest = &(m_Obstruction.ObstructionTests[0]);

			ray_hit hits[10];
			int rayresult = gEnv->pPhysicalWorld->RayWorldIntersection(
			    pObstructionTest->vOrigin, pObstructionTest->vDirection, nPhysicsFlags, rwi_pierceability0,
			    &hits[0], 10, pSkipEnts, nSkipEnts, NULL, PHYS_FOREIGN_ID_SOUND_OBSTRUCTION);

			pObstructionTest->nHits = rayresult;
			pObstructionTest->bResult = true;
			if (rayresult)
			{
				float fDistance = FLT_MAX;
				for (int j = 0; j < 10; j++)
				{
					if (hits[j].dist > 0)
					{
						IEntity* pTemp2 = NULL;
						IPhysicalEntity* pTemp = hits[j].pCollider;
						if (pTemp)
							pTemp2 =
							    (IEntity*)pTemp->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
						// pTemp->GetForeignData();
						// pTemp->GetiForeignData();
						fDistance = min(fDistance, hits[j].dist);
					}
				}

				pObstructionTest->fDistance = fDistance;
			}
		}

		else
		{
			m_Obstruction.bDelayPlayback = true;

			for (int i = 0; i < m_Obstruction.nRaysShot; ++i)
			{
				SObstructionTest* pObstructionTest = &(m_Obstruction.ObstructionTests[i]);
				pObstructionTest->nPierceability = 0;

				if (pObstructionTest->bResult)
				{
					// AddRef(); // making that sound to live until the result is ready
					//  only new rwi if the last one was computed
					pObstructionTest->SoundID = GetId();
					gEnv->pPhysicalWorld->RayWorldIntersection(
					    pObstructionTest->vOrigin, pObstructionTest->vDirection, nPhysicsFlags,
					    rwi_pierceability0 | rwi_queue, NULL, 10, pSkipEnts, nSkipEnts,
					    pObstructionTest, PHYS_FOREIGN_ID_SOUND_OBSTRUCTION);
					pObstructionTest->bResult = false;
				}
				nHitsAccum += m_Obstruction.ObstructionTests[i].nHits;
			}
		}

		if (bImmediatly)
		{
			// evaluate all results gathered till now
			m_Obstruction.bProcessed = true;
			m_Obstruction.bDelayPlayback = false;
			CalculateObstruction();
		}
		else
			m_Obstruction.bProcessed = false;
	}

	// check for closest listener?? )

	// if(pEntity)
	// myPhysicalEntity = pEntity->GetPhysics();
}

void CSound::Update()
{
	// GUARD_HEAP;
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SOUND);

	assert(m_pPlatformSound);
	assert(m_pSoundBuffer);

	CListener* pListener = (CListener*)m_pSSys->GetListener(m_pSSys->GetClosestActiveListener(m_vPlaybackPos));
	if (!pListener)
		return;

	// stopped by syncpoint
	if (m_pPlatformSound && m_pPlatformSound->GetState() == pssSYNCARRIVED)
		Stop(ESoundStopMode_AtOnce);

	// syncpoint time out
	if (m_fSyncTimeout >= 0.0f)
	{
		m_fSyncTimeout -= gEnv->pTimer->GetFrameTime();
		if (m_fSyncTimeout < 0)
			Stop(ESoundStopMode_AtOnce);
	}

	// TODO Double check? This is only for 3D Sounds
	// if (m_bPositionHasChanged || m_pSSys->GetSoundGroupPropertiesChanged() || FLAG_SOUND_SELFMOVING)
	if (m_bPositionHasChanged || (m_nFlags & FLAG_SOUND_RELATIVE) || (m_nFlags & FLAG_SOUND_SELFMOVING) ||
	    m_bFlagsChanged)
	{
		if (m_bFlagsChanged)
			SetPosition(m_vPosition);

		UpdatePositionAndVelocity();

		if (m_nFlags & FLAG_SOUND_DOPPLER_PARAM || true)
		{
			Vec3 vRelativeVelocity = pListener->GetVelocity() - m_vSpeed;
			float fDoppler = vRelativeVelocity.GetLength() * 3.6f; // Kmh
			m_pPlatformSound->SetParamByName("doppler", fDoppler, false);
		}

		// reset Doppler effect
		m_vSpeed = {};

		// invalidate current obstruction
		if (pListener->bMoved)
			m_Obstruction.bProcessed = false;
	}

	// recompute VisArea if position changed or VisAreas were invalidated
	if (m_bRecomputeVisArea)
	{
		I3DEngine* p3dEngine = gEnv->pSystem->GetI3DEngine();

		if (p3dEngine)
			m_pVisArea = p3dEngine->GetVisAreaFromPos(m_vPlaybackPos);

		m_bRecomputeVisArea = false;
	}

	if (GetFlags() & FLAG_SOUND_3D && GetFlags() & FLAG_SOUND_OBSTRUCTION) // only 3D sounds can have obstruction
	{
		// only update every defined timestep
		float fCurrTime = gEnv->pTimer->GetCurrTime();
		float fSeconds = m_tPlayTime.GetSeconds();
		if (fSeconds > 0 && (fCurrTime - m_fLastObstructionUpdate > m_pSSys->g_fObstructionUpdate))
		{
			ProcessObstruction(pListener, false);
			m_fLastObstructionUpdate = fCurrTime;
		}
	}

	if (GetFlags() & FLAG_SOUND_DAYLIGHT)
	{
		if (gEnv->p3DEngine && m_pPlatformSound)
			m_pPlatformSound->SetParamByName(
			    "daylight", gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_DAY_NIGHT_INDICATOR));
	}

	UpdateSoundProperties();

	if (m_bLoopModeHasChanged && m_pPlatformSound)
	{
		if (IsActive())
		{
			ptParamBOOL NewParam(GetLoopMode());
			if (!m_pPlatformSound->SetParamByType(pspLOOPMODE, &NewParam))
			{
				// throw error
			}
		}
	}

	m_bVolumeHasChanged = false;
	m_bPositionHasChanged = false;
	m_bLoopModeHasChanged = false;
	m_bFlagsChanged = false;

	// touch the sound
	if (m_bPlaying && m_pSoundBuffer)
		m_pSoundBuffer->UpdateTimer(gEnv->pTimer->GetFrameStartTime());
}

void CSound::SetSoundBufferPtr(CSoundBuffer* pNewSoundBuffer)
{
	if (m_pSoundBuffer)
		m_pSoundBuffer->RemoveFromLoadReqList(this);

	m_pSoundBuffer = pNewSoundBuffer;
	m_pSoundBuffer->AddToLoadReqList(this);

	/*if (m_pSoundBuffer->GetProps()->eBufferType = btEVENT)
	{
	        m_pSoundBuffer->Load(NULL);
	        m_pSoundBuffer->GetAssetHandle();

	}*/
}

// load the sound for real
bool CSound::Preload()
{
	assert(m_pSoundBuffer);

	// if (!m_pSSys->IsEnabled())
	//{
	//	OnEvent( SOUND_EVENT_ON_LOADED );
	//	return true;
	// }

	bool bResult = false;

	if (m_pSoundBuffer)
		bResult = m_pSoundBuffer->Load(this);

	return bResult;
}

void CSound::SetSoundPriority(int nSoundPriority)
{
	m_nSoundPriority = nSoundPriority;

	// if (nSoundPriority==255)
	//{
	//  keep in memory all sounds with highest priority
	//  Not needed.
	// m_nFlags|=FLAG_SOUND_LOAD_SYNCHRONOUSLY;
	// Preload();
	//}
}

//! set soundinfo struct for voice sounds
void CSound::SetVoiceSpecs(const ILocalizationManager::SLocalizedSoundInfo* pSoundInfo)
{
	assert(pSoundInfo);

	m_VoiceSpecs.fVolume = pSoundInfo->fVolume;
	m_VoiceSpecs.fDucking = pSoundInfo->fDucking;
	m_VoiceSpecs.fRadioRatio = pSoundInfo->fRadioRatio;
	m_VoiceSpecs.fRadioBackground = pSoundInfo->fRadioBackground;
	m_VoiceSpecs.fRadioSquelch = pSoundInfo->fRadioSquelch;
	m_VoiceSpecs.sCleanEventName = pSoundInfo->sSoundEvent;
}

// calc the sound volume taking into account game sound attenuation,
// reduction ratio, sound system sfx volume, master volume (concentration feature)
float CSound::CalcSoundVolume(float fSoundVolume, float fRatio) const
{
	// FRAME_PROFILER( "CSound:CalcSoundVolume",m_pSSys->GetSystem(),PROFILE_SOUND );

	float fVolume;

	for (int i = 0; i < MAX_SOUNDSCALE_GROUPS; i++)
	{
		if (m_nSoundScaleGroups & SETSCALEBIT(i))
			fRatio *= m_pSSys->m_fSoundScale[i];
	}

	fVolume = fSoundVolume * fRatio * m_HDRA;

	if (m_nFlags & FLAG_SOUND_VOICE)
		fVolume = fVolume * m_pSSys->m_fDialogVolume * m_pSSys->m_fGameDialogVolume * m_VoiceSpecs.fVolume;
	else
		fVolume = fVolume * m_pSSys->m_fSFXVolume * m_pSSys->m_fGameSFXVolume;

	if (!(m_nFlags & FLAG_SOUND_MOVIE))
		fVolume *= m_pSSys->m_fMovieFadeoutVolume;

	return (fVolume);
}

bool CSound::UpdateSoundProperties()
{
	assert(m_pPlatformSound);

	// MasterVolume is set by SoundSystem directly to FMOD
	// int nNewVolume = (int)(m_nMaxVolume * SoundGroupProps.fCurrentVolumeRatio);

	// temporally obstruction effect done by volume
	float fRatio = m_fRatio;

	// This may be also obstruction coming in from soundmoods
	if (m_nFlags & FLAG_SOUND_3D && m_nFlags & FLAG_SOUND_OBSTRUCTION && m_pPlatformSound)
	{

		// real DSP based obstruction
		if ((m_pSSys->g_nObstruction == 1))
		{
			if (!m_Obstruction.bAssigned)
			{
				m_pPlatformSound->SetObstruction(&m_Obstruction);
			}
			// m_Obstruction.bAddObstruction = true;

			// additional volume based Obstruction
			// fRatio = max(1.0f - m_pSSys->g_fObstructionMaxValue, min(1.0f -
			// m_Obstruction.GetDirect()*0.4f - m_Obstruction.GetReverb()*0.1f, 1.0f)) * m_fRatio;
			fRatio = m_fRatio;
		}

		// stronger volume based Obstruction for fallback
		if ((m_pSSys->g_nObstruction == 2))
		{
			fRatio =
			    max(1.0f - m_pSSys->g_fObstructionMaxValue,
			        min(1.0f - m_Obstruction.GetDirect() * 0.6f - m_Obstruction.GetReverb() * 0.7f, 1.0f)) *
			    m_fRatio;
		}

		m_bVolumeHasChanged = true;
		m_Obstruction.bAssigned = true;
	}

	if (m_bVolumeHasChanged || m_fActiveVolume == -1.0f || m_nFadeTimeInMS)
	{
		m_fActiveVolume = CalcSoundVolume(m_fMaxVolume, fRatio);

		if (m_fActiveVolume == 0.0f)
			int a = 0;

		if (m_nFadeTimeInMS > 0)
		{
			int nDifferenceMS = (int)(gEnv->pTimer->GetFrameTime() * 1000);

			float fTimeRatio = (min(nDifferenceMS, m_nFadeTimeInMS)) / float(m_nFadeTimeInMS);

			if (m_nFadeTimeInMS > nDifferenceMS)
				m_nFadeTimeInMS -= nDifferenceMS;

			// float fRatio = (m_fFadeCurrent + m_fFadeGoal)/2.0f;
			float fDiff = m_fFadeGoal - m_fFadeCurrent;

			m_fFadeCurrent += fDiff * fTimeRatio;

			m_fFadeCurrent = min(1.0f, m_fFadeCurrent);
			m_fFadeCurrent = max(0.0f, m_fFadeCurrent);

			if (fDiff < 0.0f)
				m_eFadeState = eFadeState_FadingOut;

			if (fDiff > 0.0f)
				m_eFadeState = eFadeState_FadingIn;

			if (m_fFadeCurrent == m_fFadeGoal)
			{
				m_nFadeTimeInMS = 0;

				if (m_eFadeState != eFadeState_JustFinished)
					m_eFadeState = eFadeState_JustFinished;
			}

			m_fActiveVolume *= m_fFadeCurrent;
		}
		else
		{
			m_eFadeState = eFadeState_None;
		}

		UpdateActiveVolume();

		if (m_pPlatformSound && m_pPlatformSound->GetState() == pssINFOONLY)
			m_fActiveVolume = -1.0f;
	}

	return true;
}

bool CSound::IsPlayingOnChannel() const
{
	if (!IsActive() || !m_pPlatformSound)
		return false;

	// if this sound is not even allocated in the sound library then return false
	if (!m_pPlatformSound->GetSoundHandle())
		return false;

	// GUARD_HEAP;

	bool bIsPlayingOnChannel = false;
	ptParamBOOL NewParam(bIsPlayingOnChannel);
	m_pPlatformSound->GetParamByType(pspISPLAYING, &NewParam);
	NewParam.GetValue(bIsPlayingOnChannel);

	// bool bIsPaused = false;
	// ptParamBOOL NewParam(bIsPlayingOnChannel);
	// m_pPlatformSound->GetParam(pspISPLAYING, &NewParam);
	// NewParam.GetValue(bIsPlayingOnChannel);

	if (bIsPlayingOnChannel || m_IsPaused)
	{
		// gEnv->pLog->LogToConsole("channel %d IS playing!!",m_nChannel);
		return true;
	}

	// gEnv->pLog->LogToConsole("channel %d,not playing!!",m_nChannel);
	return false;
}

bool CSound::IsPlayingVirtual() const
{
	bool bDebug1 = IsPlaying();
	bool bDebug2 = GetLoopMode();
	return (bDebug1 && bDebug2);
}

void CSound::Play(float fRatio, bool bForceActiveState, bool bSetRatio, IEntitySoundProxy* pEntitySoundProxy)
{
	if (!NumberValid(fRatio))
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_SOUNDSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_SOUND, 0,
		                       "<Sound> Invalid input values (possible NaNs) on Sound Play");
		assert(0);
		return;
	}

	int ntest = GetLengthMs();

	// GUARD_HEAP;
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SOUND);

	assert(m_pPlatformSound);
	assert(m_pSoundBuffer);

	if (pEntitySoundProxy) // for soundinfo reason
		m_SoundProxyEntityId = pEntitySoundProxy->GetEntity()->GetId();

	// CTimeValue test = gEnv->pTimer->GetFrameStartTime();
	// gEnv->pLog->Log("DEBUG: Sound started to be played: %s : %f \n", GetName(), test.GetSeconds());

	//	fRatio = 0.0;

	if (!m_pSoundBuffer || m_pSoundBuffer->LoadFailure())
	{
		// load failure proceedure
		OnEvent(SOUND_EVENT_ON_LOAD_FAILED);
		return;
	}

	if (m_pSSys->g_nProfiling > 0 && m_pPlatformSound && (m_pPlatformSound->GetState() == pssNONE))
	{
		const auto it = m_pSSys->m_SoundInfos.find(GetName());
		if (it != m_pSSys->m_SoundInfos.end())
		{
			it->second.get()->GetInfo()->nTimesPlayed++;
		}
	}

	if (m_eState == eSoundState_Stopped)
	{
		int nCatchme = 1;
		return;
	}

	m_bPlaying = true;

	// raise how often the SoundBuffer is used to a reasonable limit
	int nTimesUsed = m_pSoundBuffer->GetInfo()->nTimesUsed;
	if (nTimesUsed < 32000)
		m_pSoundBuffer->GetInfo()->nTimesUsed = ++nTimesUsed;

	if (!m_pSSys->IsEnabled())
	{
		// Simulate that sound is played for disabled sound system.
		OnEvent(SOUND_EVENT_ON_LOADED);
		OnEvent(SOUND_EVENT_ON_START);
		if (!(m_nFlags & FLAG_SOUND_LOOP))
			OnEvent(SOUND_EVENT_ON_STOP);
		return;
	}

	/*
	float fTime;
	if (m_pSSys->m_pCVarDebugSound->GetIVal())
	fTime = gEnv->pTimer->GetAsyncCurTime();
	TRACE("INFO: Synchroneous reading of %s due to Play() call before loading was finished.", GetName());
	if (!m_pSoundBuffer->WaitForLoad())
	return;
	if (m_pSSys->m_pCVarDebugSound->GetIVal())
	gEnv->pLog->Log("Warning: Synchroneous reading of %s due to Play() call before loading was finished stalled for
	%1.3f milliseconds !", GetName(), (gEnv->pTimer->GetAsyncCurTime()-fTime)*1000.0f);
	*/

	// TODO remove this when Play is called only once

	//////////////////////////////////////////////////////////////////////////
	// If sound is already playing, skip until it stops.
	// otherwise it creates a lot of overhead and channel leaks.
	//////////////////////////////////////////////////////////////////////////
	// if (IsPlayingOnChannel())
	//  TODO remove this when Play is called only once
	if (IsActive() && m_pPlatformSound &&
	    !((m_pPlatformSound->GetState() == pssPAUSED) || (m_pPlatformSound->GetState() == pssLOADING)) &&
	    !m_IsPaused)
	{
		int a = 0;
		// TODO Verify with future sound class design
		// if (!bUnstoppable)

		// m_pPlatformSound->StopSound();
		// SetPaused(true);

		Deactivate();
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// For Not looping sounds too far away, stop immediately not even preload sound if too far.
	//////////////////////////////////////////////////////////////////////////

	if (bSetRatio)
		m_fRatio *= fRatio;

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	// load the sound for real the first time is played
	// if (!Preload())
	// return; //the sound buffer cannot be loaded

	// Preloading is also playing.
	// m_bPlaying = true;

	// if there are already too many sounds playing, this one
	// should not be started
	// TODO combine with all active Sound lists

	// Sounds with lower priority then current minimum set by sound system are ignored.
	if (m_nSoundPriority < m_pSSys->GetMinSoundPriority())
		return;

	float fDistanceToListener = 0.0f;
	float fDistSquared = 0.0f;
	CListener* pListener =
	    (CListener*)m_pSSys->GetListener(m_pSSys->GetClosestActiveListener(GetPlaybackPosition()));

	if (CanBeCulled(pListener, fDistanceToListener, fDistSquared))
	{
		CTimeValue currTime = gEnv->pTimer->GetFrameStartTime();
		m_tPlayTime = currTime;

		OnEvent(SOUND_EVENT_ON_LOADED);
		OnEvent(SOUND_EVENT_ON_START);

		if (GetLoopMode())
		{
			Deactivate();
		}
		else
		{
			// OnEvent( SOUND_EVENT_ON_STOP );
			Stop();
		}

		return;
	}

	if (m_nFlags & FLAG_SOUND_START_PAUSED)
	{
		// all sounds that start paused, are culled
		return;
	}

	// todo if the sound is start paused, it wont be culled, but maybe it still should be, when unpaused
	Preload();

	// if the soundbuffer is still loading, initiate the callback to play
	if (m_pSoundBuffer->Loading())
	{
		if (m_pSSys->g_nDebugSound == SOUNDSYSTEM_DEBUG_DETAIL)
			gEnv->pLog->Log("<Sound> Warning: %s, Play() called before loading was finished", GetName());

		// save the state, will be played later
		m_bPlayAfterLoad = true;
		m_fPostLoadRatio = fRatio;
		m_bPostLoadForceActiveState = bForceActiveState;
		m_bPostLoadSetRatio = bSetRatio;
		m_pSoundBuffer->AddToLoadReqList(this);

		if (m_pSSys->g_nDebugSound == SOUNDSYSTEM_DEBUG_DETAIL)
			gEnv->pLog->Log("<Sound> %s Playing delayed until all data loaded!", GetName());
		return;
	}

	if (GetFlags() & FLAG_SOUND_VOICE)
	{
		SetParam("background", m_VoiceSpecs.fRadioBackground, false);
		SetParam("squelch", m_VoiceSpecs.fRadioSquelch, false);

		if (m_VoiceSpecs.fDucking > 0.0f)
		{
			ISoundMoodManager* pMoodManager = m_pSSys->GetIMoodManager();
			if (pMoodManager)
			{
				pMoodManager->RegisterSoundMood("mission_hint");
				float fCurrentFade = pMoodManager->GetSoundMoodFade("mission_hint");
				fCurrentFade = max(fCurrentFade, m_VoiceSpecs.fDucking);
				pMoodManager->UpdateSoundMood("mission_hint", fCurrentFade, 200);
			}
		}
	}

	if (m_pSoundBuffer->Loaded()) // && m_pPlatformSound && m_pPlatformSound->GetState() != pssPLAYING)
		StartPlayback(pListener, fDistanceToListener, fDistSquared);

	// All Sounds turn active now, going in their correct container
	if (m_pPlatformSound &&
	    (m_pPlatformSound->GetState() == pssPLAYING || m_pPlatformSound->GetState() == pssLOADING))
	{
		m_pSSys->SetSoundActiveState(this, eSoundState_Active);
	}

	// if the event or the soundbuffer is still loading, initiate the callback to play
	// if (m_pPlatformSound && m_pPlatformSound->GetState() == pssLOADING)
	//{
	//	if (m_pSSys->g_nDebugSound == SOUNDSYSTEM_DEBUG_DETAIL)
	//		gEnv->pLog->Log("<Sound> Warning: %s, Play() called before loading was finished", GetName());

	//	// save the state, will be played later
	//	m_bPlayAfterLoad						= true;
	//	m_fPostLoadRatio						= fRatio;
	//	m_bPostLoadForceActiveState	= bForceActiveState;
	//	m_bPostLoadSetRatio					= bSetRatio;
	//	//m_pSoundBuffer->AddToLoadReqList(this);
	//	return;
	//}

	// m_fLastPlayTime = currTime;
}

bool CSound::CanBeCulled(CListener* pListener, float& fDistanceToListener, float& fDistSquared)
{
	// bool bAllocatedChannel = false;
	// bool bStarted = false;
	uint32 nPlaybackFilter = m_pSSys->g_nPlaybackFilter;
	if (nPlaybackFilter)
	{
		bool bFilter = false;

		if (m_nFlags & FLAG_SOUND_VOICE && nPlaybackFilter == 2)
			bFilter = true;

		if (!(m_nFlags & FLAG_SOUND_VOICE) && nPlaybackFilter == 1)
			bFilter = true;

		if (nPlaybackFilter > 32 && !(m_eSemantic & nPlaybackFilter) && (m_eSemantic != eSoundSemantic_None))
			bFilter = true;

		if (bFilter && !(m_nFlags & FLAG_SOUND_START_PAUSED))
		{
			return true;
		}
	}

	bool bCullOnDistance = false;

	int nActiveSounds = (int)m_pSSys->m_OneShotSounds.size() + (int)m_pSSys->m_LoopedSounds.size();
	bool bTooManyActiveSounds = (nActiveSounds > m_pSSys->g_nMaxActiveSounds);
	bool bTooManyChannels = false;

	int nThreshold = m_pSSys->g_nPriorityThreshold;
	if (nThreshold)
	{
		int nPlaying = m_pSSys->GetIAudioDevice()->GetNumberSoundsPlaying();
		float fFloodRate = nPlaying / (float)m_pSSys->g_nMaxChannels;

		if (fFloodRate > 0.8f && m_nSoundPriority > nThreshold) // high prio means lower prio !
			// if (m_pSSys->m_pCVarMaxChannels->GetIVal() < nPlaying)// && m_nSoundPriority < nThreshold)
			bTooManyChannels = true;
	}

	if (bTooManyActiveSounds || bTooManyChannels)
		return true;

	if (m_nFlags & FLAG_SOUND_RADIUS)
	{
		if (!pListener)
			return true;

		fDistSquared = pListener->GetPosition().GetSquaredDistance(GetPlaybackPosition());
		bCullOnDistance = ((fDistSquared > 0) && (fDistSquared > m_RadiusSpecs.fMaxRadius2));
		fDistanceToListener = sqrt_tpl(fDistSquared);

		// switch to radio if needed
		// do that before Culling by distance
		if (m_nFlags & FLAG_SOUND_VOICE)
		{
			float fRadioRadius =
			    m_RadiusSpecs.fMaxRadius2 * (m_VoiceSpecs.fRadioRatio * m_VoiceSpecs.fRadioRatio);
			if (fRadioRadius > 0 && fDistSquared > fRadioRadius)
			{
				m_pPlatformSound->SetPlatformSoundName("radio3d");
				m_nFlags &= ~FLAG_SOUND_RADIUS;
				m_nFlags &= ~FLAG_SOUND_3D;
				m_nFlags |= FLAG_SOUND_2D;
				// TODO set reverblevel to 0
			}
		}
	}

	if (m_nFlags & FLAG_SOUND_RADIUS)
	{
		if (bCullOnDistance) // && !(m_nFlags & FLAG_SOUND_START_PAUSED))
		{
			return true; // too far
		}
		else
		{
			m_fRatio = GetRatioByDistance(fDistanceToListener);
		}
	}

	if (m_nFlags & FLAG_SOUND_3D) // && !(m_nFlags & FLAG_SOUND_START_PAUSED))
	{
		if (!m_pSSys->IsSoundPH(this))
			return true; // not hearable
	}

	// sound is marked to play outdoor but the listener is inside a visarea
	if (m_nFlags & FLAG_SOUND_OUTDOOR && pListener->GetVisArea())
	{
		return true;
	}

	// sound is marked to play indoor but the listener is not inside a visarea
	if (m_nFlags & FLAG_SOUND_INDOOR && !pListener->GetVisArea())
	{
		return true;
	}

	// slider for SFX sounds on mute, so all non voice sounds can be culled
	if (m_pSSys->m_fSFXVolume == 0.0f && !(m_nFlags & FLAG_SOUND_VOICE))
	{
		return true;
	}

	// slider for voice sounds on mute, so all voice sounds can be culled
	//	if( m_pSSys->m_fDialogVolume == 0.0f && (m_nFlags & FLAG_SOUND_VOICE) )
	//	{
	//		return true;
	//	}

	return false;
}

void CSound::StartPlayback(CListener* pListener, float& fDistanceToListener, float& fDistSquared)
{
	if (!m_pSoundBuffer || !m_pPlatformSound)
		return;

	m_bVolumeHasChanged = true; // make sure new volume is calculated and set to FMOD

	CTimeValue currTime = gEnv->pTimer->GetFrameStartTime();
	CTimeValue timeSinceLastPlay = currTime - m_tPlayTime;

	bool bStartPlaybackNow = true;
	if ((m_nFlags & FLAG_SOUND_OBSTRUCTION) && (m_nFlags & FLAG_SOUND_3D) && pListener && m_Obstruction.bFirstTime)
	{
		// ProcessObstruction(pListener, false);

		// m_Obstruction.bDelayPlayback = !m_Obstruction.bDelayPlayback;

		ProcessObstruction(pListener, false);
		// this call might change bDelayPlayback immediately
		bStartPlaybackNow = !m_Obstruction.bDelayPlayback;
		m_Obstruction.bFirstTime = m_Obstruction.bDelayPlayback;
	}

	// TODO REview this
	SSoundSettings NewSoundSettings;
	NewSoundSettings.Frequency = 0;
	NewSoundSettings.is3D = false;
	NewSoundSettings.bLoadData =
	    (!bStartPlaybackNow && (m_nFlags & FLAG_SOUND_OBSTRUCTION) && m_Obstruction.bFirstTime)
		? false
		: true; // delay playback for obstruction
	NewSoundSettings.bLoadNonBlocking = (m_nFlags & FLAG_SOUND_LOAD_SYNCHRONOUSLY) ? false : true;
	NewSoundSettings.SoundSettings3D = NULL;
	NewSoundSettings.SpeakerPan = 0;

	// TODO still needed here?
	m_pSoundBuffer->UpdateTimer(gEnv->pTimer->GetFrameStartTime());

	bool bOldLoadData = false;
	bool bCreated = false;

	switch (m_pSoundBuffer->GetProps()->eBufferType)
	{
		case btEVENT:
			// TODO Probably get rid of the Soundsettings at CreateSound and set all we need afterwards

			if ((m_nFlags & FLAG_SOUND_OBSTRUCTION) && !m_Obstruction.bFirstTime)
			{
				// set obstruction value on infoonly instance before creating real instance
				m_Obstruction.bAssigned = false;
				CalculateObstruction();
			}

			if (!m_pPlatformSound->GetSoundHandle())
			{
				bOldLoadData = NewSoundSettings.bLoadData;
				NewSoundSettings.bLoadData = false;
				bCreated =
				    m_pPlatformSound->CreateSound(m_pSoundBuffer->GetAssetHandle(), NewSoundSettings);
				NewSoundSettings.bLoadData = bOldLoadData;
			}

			m_fRatio = GetRatioByDistance(fDistanceToListener); // reset the current distance
			UpdateSoundProperties();
			UpdatePositionAndVelocity();

			bCreated = m_pPlatformSound->CreateSound(m_pSoundBuffer->GetAssetHandle(), NewSoundSettings);
			m_fRatio = GetRatioByDistance(fDistanceToListener); // reset the current distance
			m_tPlayTime = currTime;
			// SetFadeTime(200);
			// bAllocatedChannel		= true;
			// bStarted = true;
			// SetLoopMode(GetLoopMode()); //TODO should be review
			break;
		case btSTREAM:
			if ((!GetLoopMode()) || (!IsPlayingOnChannel()))
			{
				// TODO Probably get rid of the Soundsettings at CreateSound and set all we need
				// afterwards
				bCreated =
				    m_pPlatformSound->CreateSound(m_pSoundBuffer->GetAssetHandle(), NewSoundSettings);
				m_tPlayTime = currTime;
				// bAllocatedChannel		= true;
				// bStarted = true;
			}
			SetLoopMode(GetLoopMode()); // TODO should be review
			break;
		case btSAMPLE:
		{
			if (m_pEventGroupBuffer)
			{
				if ((m_nFlags & FLAG_SOUND_OBSTRUCTION) && !m_Obstruction.bFirstTime)
				{
					// set obstruction value on infoonly instance before creating real instance
					m_Obstruction.bAssigned = false;
					CalculateObstruction();
				}

				m_fRatio = GetRatioByDistance(fDistanceToListener); // reset the current distance
				UpdateSoundProperties();
				UpdatePositionAndVelocity();

				bCreated =
				    m_pPlatformSound->CreateSound(m_pSoundBuffer->GetAssetHandle(), NewSoundSettings);
				m_fRatio = GetRatioByDistance(fDistanceToListener); // reset the current distance
				m_tPlayTime = currTime;
			}
			else
			{
				if (m_nFlags & FLAG_SOUND_3D)
				{
					SSoundSettings3D NewSoundSettings3D;
					NewSoundSettings3D.bRelativetoListener = false;
					NewSoundSettings3D.bUseVelocity = false;
					NewSoundSettings3D.vPosition = m_vPosition;

					NewSoundSettings.is3D = true;
					NewSoundSettings.SoundSettings3D = &NewSoundSettings3D;

					if (!fDistSquared)
					{
						// check the distance from the listener
						if (!pListener)
							return;
						Vec3 vListenerPos = pListener->GetPosition();
						fDistSquared = vListenerPos.GetSquaredDistance(m_vPlaybackPos);
					}

					if (fDistSquared > m_RadiusSpecs.fMaxRadius2) // && bCullOnDistance)
					{
						// listener is beyond the MAX Radius
						if (!GetLoopMode())
						{
							Deactivate(); // free a channel
							return;       // too far
						}
						// bCanPlay = false;
					}

					// todo ask Craig or Timur how frame_profiler works
					// FRAME_PROFILER( "CSound:Create/PlaySoundEX", m_pSSys->GetSystem(),
					// PROFILE_SOUND ); if ((!GetLoopMode()) || (!IsPlayingOnChannel())) // Loopfix
					if (!IsPlayingOnChannel())
					{
						bCreated = m_pPlatformSound->CreateSound(
						    m_pSoundBuffer->GetAssetHandle(), NewSoundSettings);
						// m_fChannelPlayTime	= currTime;
						// bAllocatedChannel		= true;
						// bStarted = true;
					}
					SetLoopMode(GetLoopMode()); // Todo should be reviewed
					SetPosition(m_vPosition);
					// min max distance and loop mode must be set before 'cos it operates
					// on a per-sound basis
					SetMinMaxDistance_i(m_RadiusSpecs.fMinRadius, m_RadiusSpecs.fMaxRadius);
					// SetFrequency(m_nBaseFreq);
				}
				else // 2D Sound
				{
					// if ((!GetLoopMode()) || (!IsPlayingOnChannel())) // loopfix
					if (!IsPlayingOnChannel())
					{
						bCreated = m_pPlatformSound->CreateSound(
						    m_pSoundBuffer->GetAssetHandle(), NewSoundSettings);
						// m_fChannelPlayTime	= currTime;
						// bAllocatedChannel		= true;
						// bStarted = true;
					}
					// loop mode must be set before 'cos it operates
					// on a per-sound basis
					SetLoopMode(GetLoopMode()); // todo should be reviewed
					                            // SetFrequency(m_nRelativeFreq);
				}
			}
			if (m_fPitching != 0.0f)
			{
				float fRand = (float)rand() / (float)RAND_MAX;
				fRand = (fRand * 2.0f) - 1.0f;
				m_nCurrPitch = (int)(fRand * m_fPitching);
				// TODO activate
				//
			}
			else
				m_nCurrPitch = 0;

			SetFrequency(m_nRelativeFreq);
			if (m_nStartOffset)
			{
				ptParamINT32 NewParam(m_nStartOffset);
				m_pPlatformSound->SetParamByType(pspSAMPLEPOSITION, &NewParam);
			}
			m_nStartOffset = 0;
		}

		break;
		case btNETWORK:
			// TODO Probably get rid of the Soundsettings at CreateSound and set all we need afterwards
			bCreated = m_pPlatformSound->CreateSound(m_pSoundBuffer->GetAssetHandle(), NewSoundSettings);
			m_fRatio = GetRatioByDistance(fDistanceToListener); // reset the current distance
			// m_fChannelPlayTime	= currTime;
			// bAllocatedChannel		= true;
			// bStarted = true;
			// SetLoopMode(GetLoopMode()); //TODO should be review
			break;
	}

	if (!bCreated)
	{
		// something is wrong
		Stop();
		return;
	}

	// if (m_pPlatformSound->GetState() == pssLOADING)
	//{
	//  sound event is not ready yet, so we have to wait
	// return;
	//}

	if (m_pPlatformSound->GetSoundHandle())
	{
		ptParamINT32 NewParam(m_nSoundPriority);
		NewParam.SetValue((int32)m_nSoundPriority);
		m_pPlatformSound->SetParamByType(pspPRIORITY, &NewParam);

		m_bPositionHasChanged = true;

		// lets do a Update before unpausing the sound
		// if ((m_nFlags & FLAG_SOUND_OBSTRUCTION) && (m_nFlags & FLAG_SOUND_3D) && pListener &&
		// m_Obstruction.bFirstTime)
		//{
		//	//ProcessObstruction(pListener, false);
		//
		//	//m_Obstruction.bDelayPlayback = !m_Obstruction.bDelayPlayback;
		//
		//	ProcessObstruction(pListener, false);
		//	// this call might change bDelayPlayback immediately
		//	bStartPlaybackNow = !m_Obstruction.bDelayPlayback;
		//	m_Obstruction.bFirstTime = m_Obstruction.bDelayPlayback;
		//}

		if (!(m_nFlags & FLAG_SOUND_START_PAUSED) && bStartPlaybackNow)
		{
			if (m_IsPaused)
				int nMe = 1;

			m_IsPaused = false;

			Update();

			OnEvent(SOUND_EVENT_ON_START);
			m_pPlatformSound->PlaySound(false);

			m_tPlayTime = currTime;
			// UpdateActiveVolume();
		}
		else
		{
			m_IsPaused = true;

			// start event paused so it counts as an active instances and prevents more sounds to play in
			// the same frame
			// m_pPlatformSound->PlaySound(true);

			Update();

			// TODO REVIEW this
			// no Platform sound send this event, so it needs to be send here
			// OnEvent( SOUND_EVENT_ON_PLAY );
		}
	}

	if (m_pSSys->g_nProfiling > 0 && m_pPlatformSound && (m_pPlatformSound->GetState() == pssPLAYING))
	{
		ISoundProfileInfo* pSoundInfo = NULL;

		const auto it = m_pSSys->m_SoundInfos.find(GetName());
		if (it != m_pSSys->m_SoundInfos.end())
		{
			pSoundInfo = it->second.get();
		}

		int nTemp = 0;
		if (pSoundInfo)
		{
			++pSoundInfo->GetInfo()->nTimesPlayedOnChannel;

			ptParamINT32 ptTemp(nTemp);
			if (m_pPlatformSound->GetParamByType(pspLENGTHINBYTES, &ptTemp))
				ptTemp.GetValue(nTemp);
			else
				nTemp = m_pSoundBuffer->GetMemoryUsed();

			pSoundInfo->GetInfo()->nMemorySize = nTemp;

			nTemp = 0;
			if (m_pPlatformSound->GetParamByType(pspSPAWNEDINSTANCES, &ptTemp))
				ptTemp.GetValue(nTemp);

			int nSpawn = pSoundInfo->GetInfo()->nPeakSpawn;
			pSoundInfo->GetInfo()->nPeakSpawn = max(nSpawn, nTemp);

			nTemp = 0;
			if (m_pPlatformSound->GetParamByType(pspCHANNELSUSED, &ptTemp))
				ptTemp.GetValue(nTemp);

			pSoundInfo->GetInfo()->nChannelsUsed = nTemp;
		}
	}

	if (m_pSSys->g_nDebugSound == SOUNDSYSTEM_DEBUG_SIMPLE && !m_IsPaused && m_pPlatformSound->GetSoundHandle())
	{
		gEnv->pLog->Log("<Sound> Start playback: %s,volume=%.2f,priority=%d ID:%d, TimeDiff:%.2f", GetName(),
		                m_fActiveVolume, m_nSoundPriority, GetId(), timeSinceLastPlay.GetSeconds());
	}
}

void CSound::Deactivate()
{
	if (!m_pPlatformSound || m_pPlatformSound->GetSoundHandle() == NULL)
		return;

	m_fActiveVolume = -1;

	OnEvent(SOUND_EVENT_ON_PLAYBACK_STOPPED);
	m_pPlatformSound->FreeSoundHandle();

	m_fLastObstructionUpdate = 0.0f;
	m_Obstruction.nRaysShot = 0;
	m_Obstruction.bAssigned = false;
	m_Obstruction.bProcessed = false;
	m_Obstruction.bDelayPlayback = true;
	m_Obstruction.bFirstTime = false;
	m_Obstruction.ResetDirect();
	m_Obstruction.ResetReverb();

	m_tPlayTime.SetSeconds(-1.0f);
}

void CSound::Stop(ESoundStopMode eStopMode)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SOUND);
	// GUARD_HEAP;
	//  don't play after finishing with loading - if still loading yet...

	// bool bStopAlreadySend = false;
	// bool bAlreadyDeactivated = false;

	// dealing with static sounds
	if (GetISystem() && gEnv->pEntitySystem)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_SoundProxyEntityId);
		if (pEntity)
		{
			IEntitySoundProxy* pSoundProxy = (IEntitySoundProxy*)pEntity->GetProxy(ENTITY_PROXY_SOUND);
			if (pSoundProxy)
			{
				if (pSoundProxy->GetStaticSound(GetId()))
				{
					Deactivate();
					SetState(eSoundState_Inactive);
					m_bPlaying = false;
					OnEvent(SOUND_EVENT_ON_STOP);
					// SetPaused(true);
					return;
				}
				// else
				// OnEvent( SOUND_EVENT_ON_STOP );	// proxy wants to really stop this sound
			}
		}
	}

	m_eStopMode = eStopMode;

	if (!m_bPlaying)
	{
		SetState(eSoundState_Stopped);
		Deactivate();
		// Release(); // revert from above
		return;
	}
	else
		m_bPlaying = false;

	// be sure that it wont stop again
	// AddRef();

	m_bPlayAfterLoad = false;

	if (m_IsPaused)
		int nCatchMe = 1;

	if (m_Obstruction.bFirstTime && (m_nFlags & FLAG_SOUND_OBSTRUCTION) && m_pPlatformSound &&
	    m_pPlatformSound->GetSoundHandle())
		int nMeToo = 1;

	bool bStolen = false;

	if (m_pPlatformSound)
		bStolen = m_pPlatformSound->GetState() == pssSTOLEN;

	if (IsActive() && (m_pSSys->g_nDebugSound == SOUNDSYSTEM_DEBUG_SIMPLE))
		gEnv->pLog->Log("<Sound> Stop%s sound: %s, ID:%d", bStolen ? " stolen" : "", GetName(), GetId());

	m_IsPaused = false;

	if (eStopMode == ESoundStopMode_OnSyncPoint)
	{
		m_pPlatformSound->StopSound();
		m_fSyncTimeout =
		    (m_fSyncTimeout > 0.0f)
			? m_fSyncTimeout
			: 0.5f; // half a second of hard-coded timeout in the case no syncpoints ever arrives
	}
	else
	{
		SetState(eSoundState_Stopped);
		Deactivate();
	}

	m_fActiveVolume = -1.0f;

	// sound never was activated and idles in InactiveSound
	// if (GetState() == eSoundState_Inactive)
	//{
	// int vla = 0;
	// m_pSSys->SetSoundActiveState(this, eSoundState_Active);
	//}

	// tell all listeners this sounds stops now
	// if (!bStopAlreadySend)
	OnEvent(SOUND_EVENT_ON_STOP);

	if (m_pSSys->g_nDebugSound == SOUNDSYSTEM_DEBUG_SIMPLE)
	{
		gEnv->pLog->Log("<Sound> Stop Sound %s, ID:%d", GetName(), GetId());
	}

	// Release(); // revert from above
}

void CSound::SetPaused(bool bPaused)
{
	assert(m_pPlatformSound);

	if (!m_pPlatformSound)
		return;

	// FUNCTION_PROFILER( m_pSSys->GetSystem(),PROFILE_SOUND );
	// GUARD_HEAP;

	m_IsPaused = bPaused;

	// renew the timer
	m_tPlayTime = gEnv->pTimer->GetCurrTime();

	if ((m_nFlags & FLAG_SOUND_EVENT) && !bPaused)
	{
		m_nFlags &= ~FLAG_SOUND_START_PAUSED;
		// Play();
		// m_pPlatformSound
		ptParamBOOL NewParam(bPaused);
		m_pPlatformSound->SetParamByType(pspPAUSEMODE, &NewParam);

		if (!m_pPlatformSound->GetSoundHandle())
		{
			Play();
		}
	}
	else
	{
		ptParamBOOL NewParam(bPaused);
		m_pPlatformSound->SetParamByType(pspPAUSEMODE, &NewParam);
	}

	if (m_pPlatformSound->GetState() != pssINFOONLY)
	{
		OnEvent(bPaused ? SOUND_EVENT_ON_PLAYBACK_PAUSED : SOUND_EVENT_ON_PLAYBACK_UNPAUSED);

		if (!m_pSSys->IsInPauseCall())
			OnEvent(bPaused ? SOUND_EVENT_ON_PLAYBACK_STOPPED : SOUND_EVENT_ON_PLAYBACK_STARTED);
	}

	return;
}

bool CSound::GetPaused() const
{
	if (m_pPlatformSound && m_pPlatformSound->GetState() != pssINFOONLY)
	{
		bool bPaused = false;
		ptParamBOOL NewParam(bPaused);
		m_pPlatformSound->GetParamByType(pspPAUSEMODE, &NewParam);
		NewParam.GetValue(bPaused);
		return bPaused;
	}
	return m_IsPaused;
}

void CSound::SetFade(const float fFadeGoal, const int nFadeTimeInMS)
{
	if (!NumberValid(fFadeGoal))
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_SOUNDSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_SOUND, 0,
		                       "<Sound> Invalid input values (possible NaNs) on Sound SetFade");
		assert(0);
		return;
	}

	if (fFadeGoal != m_fFadeGoal)
	{
		m_fFadeGoal = fFadeGoal;

		if (nFadeTimeInMS != m_nFadeTimeInMS)
			m_nFadeTimeInMS = nFadeTimeInMS;
	}
}

const char* CSound::GetName()
{
	assert(m_pPlatformSound);
	assert(m_pSoundBuffer);

	if (m_sSoundName.empty() && m_pSoundBuffer && m_pPlatformSound)
	{
		if (m_pSoundBuffer->GetProps()->eBufferType == btEVENT)
		{
			string sTemp;
			ptParamCRYSTRING Temp(sTemp);
			m_pPlatformSound->GetParamByType(pspEVENTNAME, &Temp);
			Temp.GetValue(sTemp);
			m_sSoundName = m_pSoundBuffer->GetName();
			m_sSoundName += ":" + sTemp;
		}
		else
			m_sSoundName = m_pSoundBuffer->GetName();
	}

	return m_sSoundName.c_str();
}

const tSoundID CSound::GetId() const
{
	return m_nSoundID;
}

void CSound::SetId(tSoundID SoundID)
{
	m_nSoundID = SoundID;
}

void CSound::SetLoopMode(bool bLoopMode)
{
	// GUARD_HEAP;
	// FRAME_PROFILER( "CSound:SetLoopMode",m_pSSys->GetSystem(),PROFILE_SOUND );

	//  Bugsearch

	// if (IsActive())
	//{
	//	ptParamBOOL NewParam(bLoopMode);
	//	if (!m_pPlatformSound->SetParam(pspLOOPMODE, &NewParam))
	//	{
	//		// throw error
	//	}
	// }

	// only set the flag if the value really will change
	// m_bLoopModeHasChanged = ((m_nFlags & FLAG_SOUND_LOOP) != bLoopMode);
	m_bLoopModeHasChanged = true;

	if (bLoopMode)
		m_nFlags |= FLAG_SOUND_LOOP;
	else
		m_nFlags &= ~FLAG_SOUND_LOOP;
}

void CSound::SetPitching(float fPitching)
{
	if (!NumberValid(fPitching))
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_SOUNDSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_SOUND, 0,
		                       "<Sound> Invalid input values (possible NaNs) on Sound SetPitching");
		assert(0);
		return;
	}

	m_fPitching = fPitching;
}

// void CSound::SetBaseFrequency(int nFreq)
//{
//	//m_nBaseFreq = nFreq;
// }

int CSound::GetBaseFrequency() const
{
	if (!m_pSoundBuffer)
		return 0;

	return (m_pSoundBuffer->GetInfo()->nBaseFreq);
}

void CSound::SetPan(float fPan)
{
	if (m_pPlatformSound)
	{
		ptParamF32 NewParam(fPan);
		m_pPlatformSound->SetParamByType(pspSPEAKERPAN, &NewParam);
	}

	m_fPan = fPan;
}

float CSound::GetPan() const
{
	float fPan = 0.0f;

	if (m_pPlatformSound)
	{
		ptParamF32 NewParam(fPan);
		m_pPlatformSound->GetParamByType(pspSPEAKERPAN, &NewParam);
		NewParam.GetValue(fPan);
	}

	return fPan;
}

void CSound::Set3DPan(float f3DPan)
{
	if (m_pPlatformSound)
	{
		ptParamF32 NewParam(f3DPan);
		m_pPlatformSound->SetParamByType(pspSPEAKER3DPAN, &NewParam);
	}
}

float CSound::Get3DPan() const
{
	float f3DPan = 0.0f;

	if (m_pPlatformSound)
	{
		ptParamF32 NewParam(f3DPan);
		m_pPlatformSound->GetParamByType(pspSPEAKER3DPAN, &NewParam);
		NewParam.GetValue(f3DPan);
	}

	return f3DPan;
}

void CSound::SetFrequency(int nFreq)
{
	// FRAME_PROFILER( "CSound:SetFrequency",m_pSSys->GetSystem(),PROFILE_SOUND );

	assert(m_pPlatformSound);
	assert(m_pSoundBuffer);

	// TODO
	// rework the whole pitch thing

	m_nRelativeFreq = nFreq;

	if (!m_pSoundBuffer || !m_pPlatformSound)
		return;

	float fFreq = (float)m_pSoundBuffer->GetInfo()->nBaseFreq * ((float)(m_nRelativeFreq + m_nCurrPitch) / 1000.0f);
	ptParamINT32 NewParam((int)fFreq);
	m_pPlatformSound->SetParamByType(pspFREQUENCY, &NewParam);

	int32 nTestFreq = 0;
	ptParamINT32 NewParam2(nTestFreq);
	m_pPlatformSound->GetParamByType(pspFREQUENCY, &NewParam2);
	NewParam2.GetValue(nTestFreq);
}

void CSound::SetMinMaxDistance(float fMinDist, float fMaxDist)
{
	if (!NumberValid(fMinDist) || !NumberValid(fMaxDist))
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_SOUNDSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_SOUND, 0,
		                       "<Sound> Invalid input values (possible NaNs) on Sound SetMinMaxDistance");
		assert(0);
		return;
	}

	if (m_nFlags & FLAG_SOUND_EVENT)
	{
		// TODO enable gate on shipping
		// if (m_pSSys->g_nDebugSound == SOUNDSYSTEM_DEBUG_DETAIL)
		{
			gEnv->pLog->Log("[Warning] <Sound> Setting min/max radius on an event (%s) prevented",
			                GetName());
		}
	}
	else
	{
		SetMinMaxDistance_i(fMinDist, fMaxDist);
	}
}

void CSound::SetMinMaxDistance_i(float fMinDist, float fMaxDist)
{
	// FRAME_PROFILER( "CSound:SetMinMaxDistance",m_pSSys->GetSystem(),PROFILE_SOUND );

	assert(m_pSSys);

	m_RadiusSpecs.fOriginalMinRadius = fMinDist;
	m_RadiusSpecs.fOriginalMaxRadius = fMaxDist;

	m_RadiusSpecs.fMinRadius = fMinDist;
	m_RadiusSpecs.fMinRadius2 = fMinDist * fMinDist;

	m_RadiusSpecs.fMaxRadius = fMaxDist * m_RadiusSpecs.fDistanceMultiplier;
	m_RadiusSpecs.fMaxRadius2 = m_RadiusSpecs.fMaxRadius * m_RadiusSpecs.fMaxRadius;

	float fPreloadRadius = m_RadiusSpecs.fMaxRadius + SOUND_PRELOAD_DISTANCE;
	m_RadiusSpecs.fPreloadRadius2 = fPreloadRadius * fPreloadRadius;

#ifdef _DEBUG
	if (fMinDist < 0.1f)
	{
		gEnv->pLog->LogToConsole("<Sound> [%s] min out of range %f", GetName(), fMinDist);
	}
	if (fMaxDist > 1000000000.0f)
	{
		gEnv->pLog->LogToConsole("<Sound> [%s] max out of range %f", GetName(), fMaxDist);
	}
#endif

	if (m_pPlatformSound && !(m_nFlags & FLAG_SOUND_EVENT))
	{
		ptParamF32 NewParam(fMinDist);
		m_pPlatformSound->SetParamByType(pspMINRADIUS, &NewParam);
	}
}

//! Sets a distance multiplier so sound event's distance can be tweak (sadly pretty hack feature)
void CSound::SetDistanceMultiplier(const float fMultiplier)
{
	if (!NumberValid(fMultiplier))
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_SOUNDSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_SOUND, 0,
		                       "<Sound> Invalid input values (possible NaNs) on Sound SetDistanceMultiplier");
		assert(0);
		return;
	}

	m_RadiusSpecs.fDistanceMultiplier = fMultiplier;
	SetMinMaxDistance_i(m_RadiusSpecs.fOriginalMinRadius, m_RadiusSpecs.fOriginalMaxRadius);
}

void CSound::SetAttrib(float fMaxVolume, float fRatio, int nFreq, bool bSetRatio)
{
	assert(m_pPlatformSound);
	assert(nFreq == 1000); // TODO why is that so?

	m_bVolumeHasChanged = true;

	if (bSetRatio)
		m_fRatio = fRatio;

	m_fMaxVolume = fMaxVolume;

	// TODO Optimize all the Change/Set/Calc-Volume stuff
	if (m_pPlatformSound && m_pPlatformSound->GetSoundHandle())
	{
		CalcSoundVolume(m_fMaxVolume, fRatio);
	}
}

void CSound::SetAttrib(const Vec3& pos, const Vec3& speed)
{
	m_bPositionHasChanged = true;
	m_vPosition = pos;
	m_vSpeed = speed;
}

void CSound::SetRatio(float fRatio)
{
	if (!NumberValid(fRatio))
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_SOUNDSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_SOUND, 0,
		                       "<Sound> Invalid input values (possible NaNs) on Sound SetRatio");
		assert(0);
		return;
	}

	if (abs(m_fRatio - fRatio) > 0.01f)
	{
		m_bVolumeHasChanged = true;
		m_fRatio = fRatio;
	}
}

void CSound::UpdatePositionAndVelocity()
{
	assert(m_pPlatformSound);

	if (m_nFlags & FLAG_SOUND_SELFMOVING)
	{
		float fTimeDelta = gEnv->pTimer->GetFrameTime();
		Vec3 vNewPos = (m_vPosition + m_vDirection * fTimeDelta);
		SetPosition(vNewPos);
	}

	if (m_nFlags & FLAG_SOUND_RELATIVE)
		SetPosition(m_vPosition);

	if (IsActive() && (m_nFlags & FLAG_SOUND_3D) && m_pPlatformSound)
	// if (m_nFlags & FLAG_SOUND_RELATIVE)
	//{
	//	m_pPlatformSound->Set3DPosition(&m_vPosition, &m_vSpeed);
	// }
	// else
	{
		m_pPlatformSound->Set3DPosition(&m_vPlaybackPos, &m_vSpeed, m_vDirection.IsZero() ? 0 : &m_vDirection);
	}
}

void CSound::SetPosition(const Vec3& vPos)
{
	if (!vPos.IsValid())
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_SOUNDSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_SOUND, 0,
		                       "<Sound> Invalid input values (possible NaNs) on Sound SetPosition()");
		assert(0);
		return;
	}

	// for now both 2D and 3D can have a position
	// if (!(m_nFlags & FLAG_SOUND_3D))
	//	return;
	assert(m_pSSys);

	// vPos == m_vPlaybackPos

	if (vPos.IsEquivalent(m_vPosition) && !(GetFlags() & FLAG_SOUND_RELATIVE) && (m_pSSys->m_fDirAttCone == 0.0f))
		return;

	m_vPlaybackPos = m_vPosition;

	// Setting the Velocity for Doppler
	if (m_pSSys->g_nDoppler && (m_nFlags & FLAG_SOUND_DOPPLER) && m_bPlaying)
	{
		m_vSpeed = (vPos - m_vPosition);
		float fTimeDelta = gEnv->pTimer->GetFrameTime();
		m_vSpeed = iszero(fTimeDelta) ? Vec3() : m_vSpeed * (m_pSSys->g_fDopplerScale / fTimeDelta);

		if (m_vSpeed.GetLengthSquared() > 0)
			int a = 0;

		// GUARD_HEAP;
	}

	m_bPositionHasChanged = true;
	IListener* pListener = m_pSSys->GetListener(LISTENERID_STANDARD);
	Vec3 vListenerPos;

	if (pListener)
		vListenerPos = pListener->GetPosition();

	bool bSnapSoundToListener = (vListenerPos.IsEquivalent(vPos, 0.3f));

	if (bSnapSoundToListener)
		m_vPosition = vListenerPos;
	else
		m_vPosition = vPos;

	if (GetFlags() & FLAG_SOUND_RELATIVE)
	{
		if (!pListener)
			return;

		// Matrix34 mTemp = pListener->GetMatrix();
		// mTemp.SetTranslation(Vec3(0.0f));
		// Vec3 vTemp = mTemp.GetInvertedFast() * vPos; //(pListener->GetPosition());

		m_vPlaybackPos = /* vTemp + */ vListenerPos;
		// gEnv->pLog->Log("## Set rel. Sound Pos: %.2f,%.2f,%.2f", m_vPlaybackPos.x, m_vPlaybackPos.y,
		// m_vPlaybackPos.z);
	}
	else
	{
		m_vPlaybackPos = m_vPosition;
	}

	// if (!(m_nFlags & FLAG_SOUND_3D))
	//	return;

	/*
	if (m_pSSys->m_pCVarDebugSound->GetIVal()==1)
	{
	        gEnv->pLog->Log("Sound %s, pos+%f,%f,%f",m_strName.c_str(),m_position.x,m_position.y,m_position.z);
	}
	*/

	// m_pSSys->m_fDirAttCone = 0.8f;
	// m_pSSys->m_fDirAttMaxScale = 0.0f;
	// m_pSSys->m_DirAttPos = pListener->GetPosition();
	// m_pSSys->m_DirAttDir = pListener->GetForward();

	if ((m_pSSys->m_fDirAttCone > 0.0f) && (m_pSSys->m_fDirAttCone < 1.0f))
	{
		Vec3 DirToPlayer = m_vPosition - m_pSSys->m_DirAttPos;
		DirToPlayer.Normalize();
		float fScale = ((1.0f - (DirToPlayer * m_pSSys->m_DirAttDir)) / (1.0f - m_pSSys->m_fDirAttCone));

		if ((fScale > 0.0f) && (fScale <= 1.0f)) // inside cone
		{
			if ((1.0f - fScale) > m_pSSys->m_fDirAttMaxScale)
				m_pSSys->m_fDirAttMaxScale = (1.0f - fScale);

			// not move the position anymore, but increase the distance range instead
			float fReverseDistanceMultiplier = 1.0f / fScale;
			if (m_RadiusSpecs.fDistanceMultiplier != fReverseDistanceMultiplier)
				SetDistanceMultiplier(fReverseDistanceMultiplier);

			// m_vPlaybackPos = (m_vPosition * fScale) + (m_pSSys->m_DirAttPos + (m_pSSys->m_DirAttDir *
			// 0.1f))*(1.0f - fScale); TRACE("Sound-Proj: %s, %1.3f, %1.3f, %1.3f, (%5.3f, %5.3f, %5.3f)",
			// m_strName.c_str(), fScale, DirToPlayer*m_pSSys->m_DirAttDir, m_pSSys->m_fDirAttCone,
			// m_RealPos.x, m_RealPos.y, m_RealPos.z);
		}
		else
		{
			if (m_RadiusSpecs.fDistanceMultiplier != 1.0f)
				SetDistanceMultiplier(1.0f);
		}
	}
	else
	{
		if (m_pSSys->m_bNeedSoundZoomUpdate)
			SetDistanceMultiplier(1.0f);
	}

	// check if the sound must be considered
	// for sound occlusion
	if (m_nFlags & FLAG_SOUND_CULLING)
	{
		m_bRecomputeVisArea = true;
	}

	// line sound position
	if (m_bLineSound)
	{
		if (!pListener)
			return;

		float fT = 0.0f;
		float fDistance = Distance::Point_Lineseg(vListenerPos, m_Line, fT);
		m_vPlaybackPos = m_Line.start + fT * (m_Line.end - m_Line.start);
	}

	// sphere sound position
	if (m_bSphereSound)
	{
		if (!pListener)
			return;

		Vec3 vTemp = vListenerPos - m_vPosition;
		if ((vTemp.GetLengthSquared()) < m_fSphereRadius * m_fSphereRadius)
		{
			// inside
			m_vPlaybackPos = vListenerPos;
		}
		else
		{
			// outside
			vTemp = vTemp.normalize() * m_fSphereRadius;
			m_vPlaybackPos = m_vPosition + vTemp;
		}
	}

	// gEnv->pLog->Log("**** Set Sound Pos: %.2f,%.2f,%.2f", m_vPlaybackPos.x, m_vPlaybackPos.y, m_vPlaybackPos.z);
}

// modify a line sound
void CSound::SetLineSpec(const Vec3& vStart, const Vec3& vEnd)
{
	m_Line.start = vStart;
	m_Line.end = vEnd;
	m_bLineSound = true;
	m_bFlagsChanged = true;
}

// retrieve a line sound
bool CSound::GetLineSpec(Vec3& vStart, Vec3& vEnd)
{
	vStart = m_Line.start;
	vEnd = m_Line.end;

	return m_bLineSound;
}

// modify a sphere sound
void CSound::SetSphereSpec(const float fRadius)
{
	m_fSphereRadius = fRadius;
	m_bSphereSound = true;
	m_bFlagsChanged = true;
}

void CSound::SetVelocity(const Vec3& speed)
{
	m_vSpeed = speed;
}

void CSound::SetDirection(const Vec3& vDir)
{
	m_vDirection = vDir;
}
float CSound::GetRatioByDistance(const float fDistance)
{
	assert(m_pPlatformSound);

	// Sounds without a radius always play at full volume
	if (!(m_nFlags & FLAG_SOUND_RADIUS) || !m_pPlatformSound)
		return 1.0f;

	// Events attenuate using their distance parameter
	if (m_nFlags & FLAG_SOUND_EVENT)
	{
		if (m_pPlatformSound->GetSoundHandle())
		{
			ptParamF32 fParam(fDistance / m_RadiusSpecs.fDistanceMultiplier);
			m_pPlatformSound->SetParamByType(pspDISTANCE, &fParam);
		}

		if (m_nFlags & FLAG_SOUND_SPREAD)
		{
			ptParamF32 fParam((fDistance / m_RadiusSpecs.fDistanceMultiplier) / m_RadiusSpecs.fMaxRadius);
			m_pPlatformSound->SetParamByType(pspSPREAD, &fParam);
		}

		return (1.0f);
	}
	else
	{
		if (!(m_nFlags & FLAG_SOUND_NO_SW_ATTENUATION))
		{
			float fSquaredDistance = fDistance * fDistance;
			float fMinFactor =
			    __min(fSquaredDistance, (m_RadiusSpecs.fMaxRadius2 - m_RadiusSpecs.fMinRadius2));
			float fMaxFactor =
			    __max(0.f, ((fMinFactor - m_RadiusSpecs.fMinRadius2) / m_RadiusSpecs.fMaxRadius2));
			// fRatio = 1.0f - max(0,((fDist2-cs->GetSquardedMinRadius())/cs->GetSquardedMaxRadius()));
			float fRatio = 1.0f - fMaxFactor;
			return fRatio;
		}
		else
			return (1.0f);
	}
}

void CSound::UpdateActiveVolume()
{
	assert(m_pPlatformSound);

	// Tomas
	// if (!(m_nFlags & FLAG_SOUND_VOICE))
	//{
	//	m_fActiveVolume = 0.01f;
	//	m_fMaxVolume = 0.01f;
	//}

	if (m_pPlatformSound && m_pPlatformSound->GetSoundHandle()) // && nVolume != m_nActiveVolume)
	{
		ptParamF32 NewParam(m_fActiveVolume);
		m_pPlatformSound->SetParamByType(pspVOLUME, &NewParam);
		//		m_nActiveVolume = nVolume;
	}
}

void CSound::SetVolume(const float fMaxVolume)
{
	if (!NumberValid(fMaxVolume))
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_SOUNDSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_SOUND, 0,
		                       "<Sound> Invalid input values (possible NaNs) on Sound SetVolume");
		assert(0);
		return;
	}

	assert(fMaxVolume < 5);

	m_bVolumeHasChanged = true;

	m_fMaxVolume = max(0.0f, fMaxVolume);
	m_fMaxVolume = min(1.0f, m_fMaxVolume);

	// if (m_pSSys->m_pCVarDebugSound->GetIVal()==5)
	//	gEnv->pLog->LogToConsole("VOL:%s,ch=%d,Set vol=%d",GetName(),m_nChannel,nVolume);
	// ChangeVolume(nVolume);
}

void CSound::SetConeAngles(const float fInnerAngle, const float fOuterAngle)
{
	m_fInnerAngle = fInnerAngle;
	m_fOuterAngle = fOuterAngle;
}

void CSound::GetConeAngles(float& fInnerAngle, float& fOuterAngle)
{
	fInnerAngle = m_fInnerAngle;
	fOuterAngle = m_fOuterAngle;
}

void CSound::SetPitch(int nValue)
{
	assert(m_pPlatformSound);
	assert(m_pSoundBuffer);

	if (!m_pSoundBuffer || !m_pPlatformSound)
		return;

	// freq = The frequency to set. Valid ranges are from 100 to 705600.
	// Scale from 0 - 1000 to FMOD range
	int iReScaledValue;

	iReScaledValue = (int)((float)nValue / 1000.0f * (float)m_pSoundBuffer->GetInfo()->nBaseFreq);
	iReScaledValue = __max(iReScaledValue, 100);

	// if (m_pPlatformSound->GetSoundHandle() && (m_pSoundBuffer->GetType() == btSAMPLE)) // Was restrictied to
	// sanple only
	if (m_pPlatformSound->GetSoundHandle() && (m_pSoundBuffer->GetProps()->eBufferType == btSAMPLE))
	{
		// GUARD_HEAP;

		ptParamINT32 NewParam(iReScaledValue);
		m_pPlatformSound->SetParamByType(pspFREQUENCY, &NewParam);
	}
}

/*
void CSound::SetLoopPoints(const int iLoopStart, const int iLoopEnd)
{
        // if (m_pSoundBuffer->GetSoundBuffer()) was this, changed it TN TODO
        if (!m_pSoundBuffer)
                return;

        if (m_pSoundBuffer->Loaded())
        {
                // TODO Solve this. what about a INTpair
                //ptParamINT32 NewParam(iLoopStart);
                //m_pSoundBuffer->SetParam(pspLOOPPOINTS, &NewParam);  // FIX THIS
                //CS_Sample_SetLoopPoints((CS_SAMPLE*)m_pSoundBuffer->GetAssetHandle(), iLoopStart, iLoopEnd);
        }
}
*/

void CSound::FXEnable(int nFxNumber)
{
	assert(m_pPlatformSound);
	assert(m_pSoundBuffer);
	assert(m_pSSys);

	// if (!m_pSSys->g_nSoundFX->GetIVal())
	// return;
	// TODO REVIEW: Restricted to Samples only ?!

	if (!m_pSoundBuffer)
		return;

	if (!m_pPlatformSound)
		return;

	if ((m_pSoundBuffer->GetProps()->eBufferType == btSAMPLE) && m_pPlatformSound->GetSoundHandle())
	{
		// GUARD_HEAP;
		ptParamINT32 NewParam(nFxNumber);
		// m_pSoundBuffer->SetParam(pspFXENABLE, &NewParam); // FIX THIS

		// m_nFxChannel=CS_FX_Enable(m_nChannel, nFxNumber);
	}
}

void CSound::FXSetParamEQ(float fCenter, float fBandwidth, float fGain)
{
	assert(m_pSoundBuffer);
	assert(m_pSSys);

	// if (!m_pSSys->m_pCVarEnableSoundFX->GetIVal())
	// return;

	if (!m_pSoundBuffer)
		return;

	if ((m_pSoundBuffer->GetProps()->eBufferType == btSAMPLE) && (m_nFxChannel >= 0))
	{
		// deactivated for Xenon, TODO reactivate and make FX stuff platformindependent
		// GUARD_HEAP;
		// CS_FX_SetParamEQ(m_nFxChannel, fCenter, fBandwidth, fGain);
	}
}

//! returns the size of the stream in ms
int CSound::GetLengthMs() const
{
	assert(m_pSoundBuffer);
	assert(m_pPlatformSound);

	int nLengthInMs = 1;
	ptParamINT32 NewParam(nLengthInMs);

	if (m_pSoundBuffer && m_pPlatformSound)
	{
		if (m_pSoundBuffer->GetParam(apLENGTHINMS, &NewParam))
			NewParam.GetValue(nLengthInMs);
		else if (m_pPlatformSound->GetParamByType(pspLENGTHINMS, &NewParam))
			NewParam.GetValue(nLengthInMs);
	}

	return (nLengthInMs);
}

//! returns the size of the stream in bytes
int CSound::GetLength() const
{
	assert(m_pSoundBuffer);
	assert(m_pPlatformSound);

	int nLengthInMs = 1;
	ptParamINT32 NewParam(nLengthInMs);

	if (m_pSoundBuffer && m_pPlatformSound)
	{
		if (m_pSoundBuffer->GetParam(apLENGTHINBYTES, &NewParam))
			NewParam.GetValue(nLengthInMs);
		else if (m_pPlatformSound->GetParamByType(pspLENGTHINBYTES, &NewParam))
			NewParam.GetValue(nLengthInMs);
	}

	return (nLengthInMs);
}

//! retrieves the currently played sample-pos, in milliseconds or bytes
unsigned int CSound::GetCurrentSamplePos(bool bMilliSeconds) const
{
	assert(m_pPlatformSound);
	assert(m_pSoundBuffer);

	if (!m_pSoundBuffer || !m_pPlatformSound)
		return 0;

	// GUARD_HEAP;

	if (m_pSoundBuffer->NotLoaded())
	{
		// if (!Preload())
		return (0); // the sound buffer cannot be loaded
	}

	int32 nLength = 0;
	ptParamINT32 NewParam(nLength);

	switch (m_pSoundBuffer->GetProps()->eBufferType)
	{
		case btSAMPLE:
			if (bMilliSeconds)
				m_pPlatformSound->GetParamByType(pspTIMEPOSITION, &NewParam);
			else
				m_pPlatformSound->GetParamByType(pspSAMPLEPOSITION, &NewParam);
			break;
		case btSTREAM:
			if (bMilliSeconds) // TODO Not LENGTH but POSITION
				m_pSoundBuffer->GetParam(apTIMEPOSITION, &NewParam);
			else
				m_pSoundBuffer->GetParam(apBYTEPOSITION, &NewParam);
	}
	NewParam.GetValue(nLength);
	return (nLength);
}

//! set the currently played sample-pos in bytes or milliseconds
void CSound::SetCurrentSamplePos(unsigned int nPos, bool bMilliSeconds)
{
	assert(m_pPlatformSound);
	assert(m_pSoundBuffer);

	if (!m_pSoundBuffer || !m_pPlatformSound)
		return;

	// GUARD_HEAP;

	if (m_pSoundBuffer->NotLoaded())
	{
		// if (!Preload())
		return; // the sound buffer cannot be loaded
	}
	switch (m_pSoundBuffer->GetProps()->eBufferType)
	{
		case btEVENT:
		case btSAMPLE:
		{
			bool bResult = false;

			if (bMilliSeconds)
			{
				float fMS = nPos;
				ptParamF32 fParam(fMS);
				bResult = m_pPlatformSound->SetParamByType(pspTIMEPOSITION, &fParam);
			}
			else
			{
				ptParamINT32 NewParam(nPos);
				bResult = m_pPlatformSound->SetParamByType(pspSAMPLEPOSITION, &NewParam);
			}

			if (!bResult && !bMilliSeconds)
				m_nStartOffset =
				    nPos; // this causes a bug later at playback position if pos was specified in MS

			break;
		}
		case btSTREAM:
			ptParamINT32 NewParam(nPos);
			if (bMilliSeconds)
				m_pSoundBuffer->SetParam(apTIMEPOSITION, &NewParam);
			// CS_Stream_SetTime((CS_STREAM*)m_pSoundBuffer->GetAssetHandle(),nPos);
			else
				m_pSoundBuffer->SetParam(apBYTEPOSITION, &NewParam);
			// CS_Stream_SetPosition((CS_STREAM*)m_pSoundBuffer->GetAssetHandle(),nPos);
			break;
	}
}

// bool CSound::FadeIn()
//{
//	if (m_fCurrentFade >= 1)
//	{
//		m_fCurrentFade = 1;
//		return true;
//	}
//
//	// this type of fading is purely frame rate dependent
//	m_fCurrentFade += m_fFadingValue*m_pSSys->m_pISystem->GetITimer()->GetFrameTime();
//
//	//gEnv->pLog->LogToConsole("Fade IN, current fade=%f",m_fCurrentFade);
//	if (m_fCurrentFade<0)
//		m_fCurrentFade=0;
//	else
//	{
//		if (m_fCurrentFade>1.0f)
//		{
//			m_fCurrentFade=1.0f;
//			//m_nFadeType=0;
//			return true;
//		}
//	}
//	return false;
// }

// bool CSound::FadeOut()
//{
//	// this type of fading is purely frame rate dependent
//	m_fCurrentFade-=m_fFadingValue*m_pSSys->m_pISystem->GetITimer()->GetFrameTime();
//
//	//gEnv->pLog->LogToConsole("Fade OUT, current fade=%f",m_fCurrentFade);
//
//	if (m_fCurrentFade<0)
//	{
//		m_fCurrentFade=0;
//		if (IsActive())
//			Deactivate();
//		return true;
//	}
//	else
//		if (m_fCurrentFade>1.0f)
//			m_fCurrentFade=1.0f;
//
//	return false;
// }

void CSound::OnBufferLoaded()
{
	// if (!m_bAlreadyLoaded)
	{
		AddRef();
		OnEvent(SOUND_EVENT_ON_LOADED);

		if (m_bPlayAfterLoad)
		{
			if (m_pSSys->g_nDebugSound == SOUNDSYSTEM_DEBUG_DETAIL)
				gEnv->pLog->Log("<Sound> %s All data arrived, startin playback again!", GetName());

			m_bPlayAfterLoad = false;
			Play(m_fPostLoadRatio, m_bPostLoadForceActiveState, m_bPostLoadSetRatio);

			// if (m_IsPaused)
			{
				// SetPaused(false);
				// OnEvent( SOUND_EVENT_ON_PLAY );
			}
		}
		Release();
	}
	// m_bAlreadyLoaded = true;
}

void CSound::OnAssetUnloaded()
{
	if (m_pPlatformSound)
		m_pPlatformSound->FreeSoundHandle();

	// m_bAlreadyLoaded = false;
}
void CSound::OnBufferLoadFailed()
{
	AddRef();
	OnEvent(SOUND_EVENT_ON_LOAD_FAILED);
	Release();
}

void CSound::OnBufferDelete()
{
	if (m_pSoundBuffer)
		m_pSoundBuffer->RemoveFromLoadReqList(this);

	Stop();
	m_pSoundBuffer = NULL;
	// OnEvent( SOUND_EVENT_ON_LOAD_FAILED );
}

void CSound::GetMemoryUsage(class ICrySizer* pSizer) const
{
	// TODO MAJOR REVIEW HERE !
	if (!pSizer->Add(*this))
		return;

	if (m_pPlatformSound && !m_pSoundBuffer)
		m_pPlatformSound->GetMemoryUsage(pSizer);

	if (m_pSoundBuffer)
	{
		// CSoundBuffer *pBuffer = m_pSoundBuffer;
		// pSizer->Add(*pBuffer);

		uint32 dwSize = 0xffffffff;

		if (pSizer->GetResourceCollector().AddResource(m_pSoundBuffer->GetName(), dwSize))
		{
			pSizer->GetResourceCollector().OpenDependencies(m_pSoundBuffer->GetName());

			pSizer->GetResourceCollector().AddResource(m_pSoundBuffer->GetProps()->sResourceFile.c_str(),
			                                           0xffffffff);

			if (m_pPlatformSound)
				m_pPlatformSound->GetMemoryUsage(pSizer);

			pSizer->GetResourceCollector().CloseDependencies();
		}
	}
}

bool CSound::IsInCategory(const char* sCategory)
{
	bool bResult = false;

	if (m_pPlatformSound)
		bResult = m_pPlatformSound->IsInCategory(sCategory);

	return bResult;
}

bool CSound::IsLoading() const
{
	if (m_pSoundBuffer)
		return m_pSoundBuffer->Loading();

	return false;
}

bool CSound::IsLoaded() const
{
	if (m_pSoundBuffer)
		return m_pSoundBuffer->Loaded();

	return false;
}

bool CSound::UnloadBuffer()
{
	assert(m_pSoundBuffer);

	if (m_pSoundBuffer)
		return m_pSoundBuffer->UnloadData(sbUNLOAD_ALL_DATA);

	return false;
}

bool CSound::IsActive() const
{
	if (m_pPlatformSound && m_pPlatformSound->GetSoundHandle() && m_pPlatformSound->GetState() != pssINFOONLY)
		return true;

	return false;
}

void CSound::AddEventListener(ISoundEventListener* pListener, const char* sWho)
{
	TEventListenerInfoVector::iterator ItEnd = m_listeners.end();
	for (TEventListenerInfoVector::iterator it = m_listeners.begin(); it != ItEnd; ++it)
	{
		if (it->pListener == pListener)
			return;
	}

	SSoundEventListenerInfo ListenerInfo;
	ListenerInfo.pListener = pListener;

	// #ifdef _DEBUG
	memset(ListenerInfo.sWho, 0, sizeof(ListenerInfo.sWho));
	strncpy(ListenerInfo.sWho, sWho, 64);
	ListenerInfo.sWho[63] = 0;

	if (m_pSSys->g_nDebugSound == SOUNDSYSTEM_DEBUG_EVENTLISTENERS)
	{
		gEnv->pLog->Log("<Sound> Adding Listener %s on %p", ListenerInfo.sWho, pListener);
	}
	// #endif

	m_listeners.push_back(ListenerInfo);
	// stl::binary_insert_unique( m_listeners, ListenerInfo );
}

void CSound::RemoveEventListener(ISoundEventListener* pListener)
{
	TEventListenerInfoVector::iterator ItEnd = m_listenersToBeRemoved.end();
	for (TEventListenerInfoVector::iterator it = m_listenersToBeRemoved.begin(); it != ItEnd; ++it)
	{
		if (it->pListener == pListener)
			return;
	}

	SSoundEventListenerInfo ListenerInfo;
	ListenerInfo.pListener = pListener;

	m_listenersToBeRemoved.push_back(ListenerInfo);
	// stl::binary_insert_unique( m_listenersToBeRemoved, ListenerInfo );
}

void CSound::OnEvent(ESoundCallbackEvent event)
{
	// remove accumulated listeners that unregistered so far
	TEventListenerInfoVector::iterator ItREnd = m_listenersToBeRemoved.end();
	for (TEventListenerInfoVector::iterator ItR = m_listenersToBeRemoved.begin(); ItR != ItREnd; ++ItR)
	{
		// SSoundEventListenerInfo* pListenerInfo = (ItR);
		// stl::binary_erase(m_listeners, *pListenerInfo );
		TEventListenerInfoVector::iterator ItLEnd = m_listeners.end();
		for (TEventListenerInfoVector::iterator itL = m_listeners.begin(); itL != ItLEnd; ++itL)
		{
			if (itL->pListener == ItR->pListener)
			{
				m_listeners.erase(itL);
				break;
			}
		}
	}

	// send the event to listeners
	if (!m_listeners.empty())
	{
		TEventListenerInfoVector::iterator ItEnd = m_listeners.end();
		for (TEventListenerInfoVector::iterator It = m_listeners.begin(); It != ItEnd; ++It)
		{
			// SSoundEventListenerInfo* pListenerInfo = (It);
			// assert (pListenerInfo);
			// #ifdef _DEBUG
			if (m_pSSys->g_nDebugSound == SOUNDSYSTEM_DEBUG_EVENTLISTENERS)
			{
				gEnv->pLog->Log("<Sound> Calling Listener %s on %p", It->sWho, It->pListener);
			}
			// #endif

			It->pListener->OnSoundEvent(event, this);
		}
	}

	// Tell the Soundsystem about this
	m_pSSys->OnEvent((ESoundSystemCallbackEvent)event, this);
}

bool CSound::IsPlayLengthExpired(float fCurrTime) const
{
	assert(m_pSSys);

	if (m_pPlatformSound && (m_pPlatformSound->GetState() == pssWAITINGFORSYNC) && (m_fSyncTimeout < 0))
		return true;

	if (m_tPlayTime.GetSeconds() < 0 || GetLoopMode() || m_pSSys->m_bSoundSystemPause)
		return false;
	// Take few additional seconds precaution, (10 secs).
	// if (fCurrTime > m_tPlayTime + m_fSoundLengthSec + 10)
	float fSeconds = m_tPlayTime.GetSeconds();
	float fDuration = (GetLengthMs() + 1) / 1000.0f;

	// 1 extra second for sounds that are paused but got never unpaused (maybe because of missing obstruction
	// callback
	if (m_IsPaused)
		fDuration += 1;

	if (fCurrTime > fSeconds + fDuration + 0.5f) // half a second longer to give the callback a chance to catch it
		return true;

	return false;
}

// Information

// Gets and Sets Parameter defined in the enumAssetParam list
bool CSound::GetParam(enumSoundParamSemantics eSemantics, ptParam* pParam) const
{
	switch (eSemantics)
	{
		case spNONE:
			return false;
			break;
		case spSOUNDID:
		{
			int32 nTemp;
			if (!(pParam->GetValue(nTemp)))
				return false;
			nTemp = m_nSoundID;
			pParam->SetValue(nTemp);
			break;
		}
		case spMINRADIUS:
		{
			float fTemp;
			if (!(pParam->GetValue(fTemp)))
				return false;
			fTemp = m_RadiusSpecs.fMinRadius;
			pParam->SetValue(fTemp);
			break;
		}
		case spMAXRADIUS:
		{
			float fTemp;
			if (!(pParam->GetValue(fTemp)))
				return false;
			fTemp = m_RadiusSpecs.fMaxRadius;
			pParam->SetValue(fTemp);
			break;
		}
		case spPITCH:
		{
			float fTemp;
			if (!(pParam->GetValue(fTemp)))
				return false;
			fTemp = m_fPitching;
			pParam->SetValue(fTemp);
			break;
		}
		case spSYNCTIMEOUTINSEC:
		{
			float fTemp;
			if (!(pParam->GetValue(fTemp)))
				return false;
			fTemp = m_fSyncTimeout;
			pParam->SetValue(fTemp);
			break;
		}
		default:
			return false;
			break;
	}

	return false;
}

bool CSound::SetParam(enumSoundParamSemantics eSemantics, ptParam* pParam)
{
	float fNanTest = 0.0f;
	if (pParam->GetValue(fNanTest) && !NumberValid(fNanTest))
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_SOUNDSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_SOUND, 0,
		                       "<Sound> Invalid input values (possible NaNs) on Sound SetParam by Type");
		assert(0);
		return false;
	}

	switch (eSemantics)
	{
		case spNONE:
			return false;
			break;
		case spPITCH:
		{
			float fTemp;
			if (!(pParam->GetValue(fTemp)))
				return false;
			m_fPitching = fTemp;
			break;
		}
		case spSYNCTIMEOUTINSEC:
		{
			float fTemp;
			if (!(pParam->GetValue(fTemp)))
				return false;
			m_fSyncTimeout = fTemp;
			break;
		}
		case spREVERBWET:
		{
			float fTemp;
			if (!(pParam->GetValue(fTemp)))
				return false;

			if (m_pPlatformSound)
				return m_pPlatformSound->SetParamByType(pspREVERBWET, pParam);

			break;
		}
		case spREVERBDRY:
		{
			float fTemp;
			if (!(pParam->GetValue(fTemp)))
				return false;

			if (m_pPlatformSound)
				return m_pPlatformSound->SetParamByType(pspREVERBDRY, pParam);

			break;
		}
		default:
			return false;
			break;
	}
	return false;
}

// Gets and Sets Parameter defined by string and float value
int CSound::GetParam(const char* sParameter, float* fValue, bool bOutputWarning) const
{
	assert(m_pPlatformSound);
	assert(fValue);

	if (_stricmp(sParameter, "HDRA") == 0)
	{
		*fValue = m_HDRA;
		return true;
	}

	int nIndex = -1;

	if (m_pPlatformSound)
		return m_pPlatformSound->GetParamByName(sParameter, fValue, bOutputWarning);

	// gEnv->pLog->Log("Error: Sound: Parameter <%s> couldn't be get on %s\n", sParameter, GetName().c_str());
	return nIndex;
}

int CSound::SetParam(const char* sParameter, float fValue, bool bOutputWarning)
{
	if (!NumberValid(fValue))
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_SOUNDSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_SOUND, 0,
		                       "<Sound> Invalid input values (possible NaNs) on Sound SetParam by Name %s",
		                       sParameter);
		assert(0);
		return false;
	}

	assert(m_pPlatformSound);

	// if (stricmp(sParameter,"HDRA")==0)
	//{
	//	m_HDRA = fValue;
	//	return true;
	// }

	int nIndex = -1;

	if (m_pPlatformSound)
		return m_pPlatformSound->SetParamByName(sParameter, fValue, bOutputWarning);

	// gEnv->pLog->Log("Error: Sound: Parameter <%s> (%f) couldn't be set on %s\n", sParameter, fValue,
	// GetName().c_str());
	return nIndex;
}

// Gets and Sets Parameter defined by index and float value
bool CSound::GetParam(int nIndex, float* fValue, bool bOutputWarning) const
{
	assert(fValue);
	assert(m_pPlatformSound);

	if (m_pPlatformSound)
		return m_pPlatformSound->GetParamByIndex(nIndex, fValue, bOutputWarning);

	// gEnv->pLog->Log("Error: Sound: Parameter <%s> couldn't be get on %s\n", sParameter, GetName().c_str());
	return false;
}

bool CSound::SetParam(int nIndex, float fValue, bool bOutputWarning)
{
	if (!NumberValid(fValue))
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_SOUNDSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_SOUND, 0,
		                       "<Sound> Invalid input values (possible NaNs) on Sound SetParam by Index");
		assert(0);
		return false;
	}

	assert(m_pPlatformSound);

	if (m_pPlatformSound)
		return m_pPlatformSound->SetParamByIndex(nIndex, fValue, bOutputWarning);

	// gEnv->pLog->Log("Error: Sound: Parameter <%s> (%f) couldn't be set on %s\n", sParameter, fValue,
	// GetName().c_str());
	return false;
}
