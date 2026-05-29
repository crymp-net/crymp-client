/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a standard seat for vehicles

-------------------------------------------------------------------------
History:
- 14:04:2005: Created by Mathieu Pinard

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/gEnv.h"
#include "CryCommon/CryAction/IActorSystem.h"
#include "CryCommon/CryAction/IAnimatedCharacter.h"
#include "CryCommon/CryAction/IAnimationGraph.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryAction/IViewSystem.h"
#include "CryCommon/CryAction/IGameObject.h"
#include "CryCommon/CryAISystem/IAgent.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CryRenderer/IRenderAuxGeom.h"
#include "CryCommon/CryAction/IGameRulesSystem.h"

#include "CryCommon/CrySoundSystem/ISoundMoodManager.h"
#include "CryCommon/CryAnimation/ICryAnimation.h"
#include "CryCommon/CryAction/IGameplayRecorder.h"
#include "CryCommon/CryInput/IInput.h"
#include "CryCommon/CryMath/Cry_Camera.h"

#include "VehicleSeat.h"
#include "VehicleSeatGroup.h"
#include "VehiclePartBase.h"
#include "VehiclePartAnimated.h"
#include "Vehicle.h"

#include "VehicleSeatActionMovement.h"
#include "VehicleSeatActionRotateTurret.h"
#include "VehicleSeatActionWeapons.h"
#include "VehicleSeatSerializer.h"

#include "VehicleViewThirdPerson.h"

/*
#include "../Network/GameContext.h"
#include "../Network/GameChannel.h"
#include "../Network/GameClientNub.h"
#include "../Network/GameClientChannel.h"
#include "../Network/GameServerNub.h"
#include "../Network/GameServerChannel.h"
#include "../Network/GameServerNub.h"
*/

// MusicLogic
#include "CryCommon/CrySoundSystem/IMusicSystem.h"

IGameFramework* CVehicleSeat::m_pGameFramework = NULL;

//------------------------------------------------------------------------
CVehicleSeat::CVehicleSeat()
	: m_aiWeaponId(0),
	m_currentView(InvalidVehicleViewId),
	m_pEnterHelper(NULL),
	m_pExitHelper(NULL),
	m_pSitHelper(NULL),
	m_pAimPart(NULL),
	m_pVehicle(NULL),
	m_pRemoteSeat(NULL)
{
	m_isPassengerShielded = false;
	m_isPassengerHidden = false;
	m_isPassengerExposed = false;
	m_pSerializer = 0;
	m_serializerId = 0;
	m_agStartEnterQueryId = m_agDeathQueryId = m_agSeatQueryId = m_agVehicleQueryId = 0;
	m_pWeaponAction = NULL;
	m_pTurretAction = NULL;
	m_blurPrevVal = 0.0f;
	m_isLocked = false;
	m_adjustedExitPos.zero();
	m_passengerId = 0;
	m_pSeatGroup = NULL;
	m_pAimPart = NULL;
	m_serializerId = 0;
	m_pSerializer = 0;
	m_currentView = InvalidVehicleViewId;
	m_isDriver = false;
	m_isPassengerShielded = false;
	m_isPassengerHidden = false;
	m_movePassengerOnExit = true;
	m_isRagdollingOnDeath = false;
	m_disableStopAllAnimationsOnEnter = false;
	m_agSeatNumber = 0;
	m_blurMult = 1.0f;
	m_explosionShakeMult = 1.f;
	m_seatGroupIndex = 1;
	m_transitionType = eVT_None;
	m_prevTransitionType = eVT_None;
	m_queuedTransition = eVT_None;
	m_isFiring = false;
	m_exitOffsetPlayer.zero();
	m_exitWorldPos.zero();
	m_postEnterId = 0;
	m_postViewId = 0;
	m_skipViewBlending = false;
}

//------------------------------------------------------------------------
CVehicleSeat::~CVehicleSeat()
{
	if (m_passengerId)
		Exit(false, true);

	while (!m_stateList.empty())
		RemoveStateListener(m_stateList.back());

	for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
	{
		SSeatActionData& seatActionData = *ite;
		assert(seatActionData.pSeatAction);
		seatActionData.pSeatAction->Release();
		seatActionData.pSeatAction = NULL;
	}

	for (TVehicleViewVector::iterator it = m_views.begin(); it != m_views.end(); ++it)
		(*it)->Release();

	if (gEnv->bMultiplayer && gEnv->bServer && gEnv->pEntitySystem->GetEntity(m_serializerId))
	{
		gEnv->pEntitySystem->RemoveEntity(m_serializerId, true /*Remove Now*/);
		m_serializerId = 0;
		m_pSerializer = 0;
	}
}

//------------------------------------------------------------------------
bool CVehicleSeat::Init(IVehicle* pVehicle, TVehicleSeatId seatId, const CVehicleParams& seatTable)
{
	m_pVehicleSystem = static_cast<CVehicleSystem*>(gEnv->pGame->GetIGameFramework()->GetIVehicleSystem());

	if (!m_pGameFramework)
		m_pGameFramework = gEnv->pGame->GetIGameFramework();

	m_pVehicle = (CVehicle*)pVehicle;
	m_seatId = seatId;

	if (seatTable.haveAttr("AimPart"))
		m_pAimPart = m_pVehicle->GetPart(seatTable.getAttr("AimPart"));

	m_name = seatTable.getAttr("name");

	if (seatTable.haveAttr("sitHelper"))
		m_pSitHelper = m_pVehicle->GetHelper(seatTable.getAttr("sitHelper"));

	if (seatTable.haveAttr("enterHelper"))
		m_pEnterHelper = m_pVehicle->GetHelper(seatTable.getAttr("enterHelper"));

	if (seatTable.haveAttr("exitHelper"))
		m_pExitHelper = m_pVehicle->GetHelper(seatTable.getAttr("exitHelper"));

	seatTable.getAttr("exitOffsetPlayer", m_exitOffsetPlayer);
	seatTable.getAttr("locked", m_isLocked);

	// Init views
	if (CVehicleParams viewsTable = seatTable.findChild("Views"))
	{
		for (int i = 0; i < viewsTable.getChildCount(); i++)
		{
			if (CVehicleParams viewTable = viewsTable.getChild(i))
			{
				string className = viewTable.getAttr("class");
				if (!className.empty())
				{
					if (CVehicleParams viewParamsTable = viewTable.findChild(className.c_str()))
					{
						IVehicleView* view = m_pVehicleSystem->CreateVehicleView(className);
						if (view)
						{
							if (view->Init(this, viewTable))
							{
								m_views.push_back(view);
							}
							else
							{
								delete view;
							}
						}
					}
				}
			}
		}
	}

	CVehicleViewThirdPerson* pDebugView = new CVehicleViewThirdPerson;
	if (pDebugView->Init(this))
	{
		pDebugView->SetDebugView(true);
		m_views.push_back(pDebugView);
	}
	else
		delete pDebugView;

	seatTable.getAttr("isDriver", m_isDriver);
	seatTable.getAttr("isPassengerShielded", m_isPassengerShielded);
	seatTable.getAttr("isPassengerHidden", m_isPassengerHidden);
	seatTable.getAttr("movePassengerOnExit", m_movePassengerOnExit);
	seatTable.getAttr("ragdollOnDeath", m_isRagdollingOnDeath);
	seatTable.getAttr("isPassengerExposed", m_isPassengerExposed);

	m_seatNameToUseForEntering = seatTable.getAttr("usesSeatForEntering");
	m_useActionsRemotelyOfSeat = seatTable.getAttr("remotelyUseActionsFromSeat");
	seatTable.getAttr("disableStopAllAnimationsOnEnter", m_disableStopAllAnimationsOnEnter);

	m_agVehicleName = seatTable.getAttr("agVehicleName");
	if (m_agVehicleName.empty())
		m_agVehicleName = pVehicle->GetEntity()->GetClass()->GetName();

	if (!seatTable.getAttr("agSeatNumber", m_agSeatNumber))
		m_agSeatNumber = m_seatId;

	seatTable.getAttr("blurMult", m_blurMult);
	seatTable.getAttr("explosionShakeMult", m_explosionShakeMult);
	seatTable.getAttr("seatGroupIndex", m_seatGroupIndex);

	if (CVehicleParams soundsTable = seatTable.findChild("Sounds"))
	{
		soundsTable.getAttr("inout", m_soundParams.inout);
		soundsTable.getAttr("mood", m_soundParams.mood);
	}

	InitSeatActions(seatTable);

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeat::PostInit(IVehicle* pVehicle)
{
	if (!m_useActionsRemotelyOfSeat.empty())
	{
		TVehicleSeatId seatId = m_pVehicle->GetSeatId(m_useActionsRemotelyOfSeat.c_str());

		if (seatId != InvalidVehicleSeatId)
			m_pRemoteSeat = (CVehicleSeat*)m_pVehicle->GetSeatById(seatId);

		m_useActionsRemotelyOfSeat.clear();
	}
}

//------------------------------------------------------------------------
bool CVehicleSeat::InitSeatActions(const CVehicleParams& seatTable)
{
	if (IsDriver())
	{
		CVehicleSeatActionMovement* pActionMovement = new CVehicleSeatActionMovement;
		if (pActionMovement && pActionMovement->Init(m_pVehicle, GetSeatId()))
		{
			m_seatActions.resize(m_seatActions.size() + 1);
			SSeatActionData& seatActionData = m_seatActions.back();

			seatActionData.pSeatAction = pActionMovement;
			seatActionData.isAvailableRemotely = true;
		}
		else
		{
			assert(0 && "[CVehicleSeat::InitSeatActions] failed to init movement action");
			delete pActionMovement;
		}
	}

	CVehicleParams seatActionsTable = seatTable.findChild("SeatActions");
	if (!seatActionsTable)
		return false;

	int i = 0;
	int c = seatActionsTable.getChildCount();

	for (; i < c; i++)
	{
		if (CVehicleParams seatActionTable = seatActionsTable.getChild(i))
		{
			string className = seatActionTable.getAttr("class");
			if (!className.empty())
			{
				IVehicleSeatAction* pSeatAction = m_pVehicleSystem->CreateVehicleSeatAction(className);
				if (pSeatAction && pSeatAction->Init(m_pVehicle, m_seatId, seatActionTable))
				{
					// add the seat action to the vector
					m_seatActions.resize(m_seatActions.size() + 1);
					SSeatActionData& seatActionData = m_seatActions.back();

					seatActionData.pSeatAction = pSeatAction;

					// store first weapon/turret action for quick access
					CVehicleSeatActionWeapons* pActionWeapons = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, pSeatAction);
					if (pActionWeapons && !m_pWeaponAction)
						m_pWeaponAction = pActionWeapons;

					CVehicleSeatActionRotateTurret* pActionTurret = CAST_VEHICLEOBJECT(CVehicleSeatActionRotateTurret, pSeatAction);
					if (pActionTurret && !m_pTurretAction)
						m_pTurretAction = pActionTurret;

					if (!seatActionTable.getAttr("isAvailableRemotely", seatActionData.isAvailableRemotely))
						seatActionData.isAvailableRemotely = false;
				}
			}
		}
	}

	bool needsSynch = !m_seatActions.empty();

	if (gEnv->bMultiplayer && gEnv->bServer && needsSynch && !IsDemoPlayback())
	{
		string name(m_pVehicle->GetEntity()->GetName());
		name.append("_serializer_");

		char a[128] = { 0 };
		name.append(ltoa(m_seatId, a, 10));

		SEntitySpawnParams params;
		params.sName = name.c_str();
		params.nFlags = ENTITY_FLAG_NEVER_NETWORK_STATIC | ENTITY_FLAG_NO_SAVE;
		params.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("VehicleSeatSerializer");

		if (!params.pClass)
		{
			CryLogWarningAlways("VehicleSeatSerializer not registered in the entity system!");
			return false;
		}


		static_cast<CVehicleSystem*>(gEnv->pGame->GetIGameFramework()->GetIVehicleSystem())->SetInitializingSeat(this);

		IEntity* pSerializerEntity = gEnv->pEntitySystem->SpawnEntity(params);

		static_cast<CVehicleSystem*>(gEnv->pGame->GetIGameFramework()->GetIVehicleSystem())->SetInitializingSeat(0);

		CGameObject* pSerializerGameObject = (CGameObject*)pSerializerEntity->GetProxy(ENTITY_PROXY_USER);
		if (!pSerializerGameObject)
			return false;
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeat::Reset()
{
	if (m_passengerId)
		Exit(false, true);

	for (TVehicleSeatActionVector::iterator it = m_seatActions.begin(), end = m_seatActions.end(); it != end; ++it)
	{
		SSeatActionData& data = *it;
		data.pSeatAction->Reset();
	}

	if (IVehicleView* pView = GetView(GetCurrentView()))
		pView->Reset();

	m_isFiring = false;
	m_transitionType = eVT_None;
	m_queuedTransition = eVT_None;

	if (gEnv->bEditor)
	{
		while (!m_stateList.empty())
		{
			RemoveStateListener(m_stateList.back());
		}
	}
	m_stateList.clear();

	m_isLocked = 0;
	m_passengerId = 0;
	//CHANGED_NETWORK_STATE(m_pVehicle, CVehicle::ASPECT_SEAT_PASSENGER);
	m_pVehicle->GetGameObject()->ChangedNetworkState(CVehicle::ASPECT_SEAT_PASSENGER);
}

//------------------------------------------------------------------------
void CVehicleSeat::SerializeActions(TSerialize ser, EEntityAspects aspects)
{
	for (TVehicleSeatActionVector::iterator it = m_seatActions.begin(); it != m_seatActions.end(); ++it)
		it->pSeatAction->Serialize(ser, aspects);
}

// this is now implemented in CAnimatedCharacter. this would override that
/*
//------------------------------------------------------------------------
int animEventCallback(ICharacterInstance* pCharInstance, void* pData)
{
	CVehicleSeat* pSeat = (CVehicleSeat*) pData;
	assert(pSeat);

	IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pSeat->GetPassenger());

	ISkeleton* pSkeleton = pCharInstance->GetISkeleton();
	assert(pSkeleton);

	LastAnimEvent lastEvent = pSkeleton->GetLastAnimEvent();

	if (lastEvent.m_SoundPathName && strcmp("", lastEvent.m_SoundPathName) != 0)
	{
		if (pActor)
		{
			IEntity* pEntity = pActor->GetEntity();
			assert(pEntity);

			IEntitySoundProxy* pSoundProxy = (IEntitySoundProxy*) pEntity->GetProxy(ENTITY_PROXY_SOUND);
			assert(pSoundProxy);

			Vec3 soundPos;
			soundPos.zero();

			int16 jointId = pSkeleton->GetIDByName(lastEvent.m_BonePathName);
			if (jointId > -1)
			{
				QuatT q = pSkeleton->GetAbsJointByID(jointId);
				soundPos = q.t;
			}

			pSoundProxy->PlaySound(lastEvent.m_SoundPathName, soundPos, FORWARD_DIRECTION, FLAG_SOUND_3D);
		}

	}

	return 0;
}
*/

//------------------------------------------------------------------------
bool CVehicleSeat::IsAnimGraphStateExisting(EntityId actorId)
{
	IAnimationGraphState* pState = GetAnimGraphState(actorId);
	if (!pState)
		return false;

	bool res = false;

	if (IAnimationGraphExistanceQuery* pQuery = pState->CreateExistanceQuery()) // assumes vehicle states are defined in layer 0
	{
		pQuery->SetInput("Vehicle", m_agVehicleName.c_str());
		pQuery->SetInput("VehicleSeat", m_agSeatNumber);

		res = pQuery->Complete();
		pQuery->Release();
	}

	return res;
}


//------------------------------------------------------------------------
bool CVehicleSeat::Enter(EntityId actorId, bool isTransitionEnabled)
{
	if (m_pVehicle->IsDestroyed())
		return false;

	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	assert(pActorSystem);

	IActor* pActor = pActorSystem->GetActor(actorId);
	if (!pActor)
		return false;

	if (IPhysicalEntity* pPE = m_pVehicle->GetEntity()->GetPhysics())
	{
		pe_action_awake awakeParams;
		awakeParams.bAwake = true;
		pPE->Action(&awakeParams);
	}

	if (!IsFree())
	{
		if (m_passengerId != pActor->GetEntityId())
			return false;
		else if (m_transitionType == eVT_Entering && !m_postEnterId)
			return true;
	}

	if (m_passengerId == pActor->GetEntityId() && m_transitionType == eVT_None && !m_postEnterId)
		return false;

	// exit previous seat if any  
	if (IVehicle* pPrevVehicle = pActor->GetLinkedVehicle())
	{
		if (IVehicleSeat* pPrevSeat = pPrevVehicle->GetSeatForPassenger(pActor->GetEntityId()))
			if (pPrevSeat != this)
				pPrevSeat->Exit(false, true);
	}

	// in case there's just a corpse on the seat, we simply push it to the ground
	if (m_passengerId && m_passengerId != pActor->GetEntityId())
	{
		if (m_transitionType == eVT_RemoteUsage)
			ExitRemotely();

		if (IActor* pCurrentPassenger = pActorSystem->GetActor(m_passengerId))
		{
			if ((pCurrentPassenger != NULL) && (pCurrentPassenger->GetHealth() <= 0))
				Exit(false);
			else
				return false;
		}
	}

	bool isAnimGraphStateExists = IsAnimGraphStateExisting(pActor->GetEntityId());

	if (isTransitionEnabled && isAnimGraphStateExists && !pActor->IsPlayer() && VehicleCVars().v_transitionAnimations == 1)
	{
		pActor->HolsterItem(true);

		m_passengerId = actorId;
		m_transitionType = eVT_Entering;

		m_aiWeaponId = 0;

		SActorTargetParams target;
		CVehicleSeat* pSeatToEnter = NULL;

		if (!m_seatNameToUseForEntering.empty())
		{
			pSeatToEnter = (CVehicleSeat*)m_pVehicle->GetSeatById(m_pVehicle->GetSeatId(m_seatNameToUseForEntering.c_str()));
			if (pSeatToEnter)
			{
				if (!pSeatToEnter->IsFree())
					return false;

				pSeatToEnter->m_transitionType = eVT_SeatIsBorrowed;
			}
		}

		if (!pSeatToEnter)
			pSeatToEnter = this;

		IVehicleHelper* pHelper = pSeatToEnter->GetEnterHelper();
		if (pHelper)
		{
			target.location = pHelper->GetWorldTM().GetTranslation();
			target.direction = pHelper->GetWorldTM().GetColumn(1);
		}
		else
		{
			const Matrix34& worldTM = m_pVehicle->GetEntity()->GetWorldTM();
			target.location = worldTM.GetTranslation();
			target.direction = worldTM.GetColumn(1);
		}

		//CPersistantDebug* pPD = gEnv->pGame->GetIGameFramework()->GetPersistantDebug();
		//pPD->Begin("VehicleSeat", false);
			//pPD->AddSphere(target.location, 0.2f, ColorF(0,0,1,1), 3.f);

		target.vehicleSeat = pSeatToEnter->GetSeatId();
		//target.directionTolerance = 0.0f; //CryMP: fixme?
		//target.startArcAngle = 0.0f;
		target.speed = 0;
		target.vehicleName = m_agVehicleName;
		target.pQueryStart = &m_agStartEnterQueryId;
		target.pQueryEnd = &m_agStartEndQueryId;
		//target.triggerUser = eAGTU_VehicleSystem;
		//target.startWidth = 0.0f;

		// don't set actor target here! instead tell AIObject to do that after passing thru all smart objects
		IAIObject* pAIObject = pActor->GetEntity()->GetAI();
		IPipeUser* pPipeUser = CastToIPipeUserSafe(pAIObject);
		if (pPipeUser)
		{
			// Setup the actor target request for AI.
			// After the request is set, the AI goalpipe can approach to it by locating "animtarget"
			// and it will automatically trigger the exact positioning request in AG.
			SAIActorTargetRequest	req;

			// Set the location the AI should approach to. Try to use the enter helper if possible.
			if (pSeatToEnter->GetEnterHelper())
			{
				req.approachLocation = pSeatToEnter->GetEnterHelper()->GetWorldTM().GetTranslation();
				req.approachDirection = pSeatToEnter->GetEnterHelper()->GetWorldTM().GetColumn(1);
			}
			else
			{
				req.approachLocation = target.location;
				req.approachDirection = target.direction;
			}
			// The location to do the exact positioning.
			req.animLocation = target.location;
			req.animDirection = target.direction;
			req.vehicleSeat = target.vehicleSeat;
			//req.directionTolerance = target.directionTolerance;
			//req.startArcAngle = target.startArcAngle;
			req.speed = target.speed;
			req.vehicleName = target.vehicleName;
			//req.startWidth = target.startWidth;
			// Pass the local ids so that it is possible to listen to the query results.
			req.pQueryStart = target.pQueryStart;
			req.pQueryEnd = target.pQueryEnd;
			req.signalAnimation = false;

			pPipeUser->SetActorTargetRequest(req);

			// Set animation graph listener to check when the actor target request is being executed.
			IAnimationGraphState* pGraphState = pActor->GetAnimationGraphState();
			AddStateListener(pGraphState);
		}
		else
		{
			CMovementRequest mr;
			mr.SetActorTarget(target);
			bool requestResult = false;

			requestResult = pActor->GetMovementController()->RequestMovement(mr);

			if (requestResult)
			{
				IAnimationGraphState* pGraphState = pActor->GetAnimationGraphState();
				AddStateListener(pGraphState);

				// this is now implemented in CAnimatedCharacter. this would override that
				/*
				if (ICharacterInstance* pCharInstance = pActor->GetEntity()->GetCharacter(0))
				{
					ISkeleton* pSkeleton = pCharInstance->GetISkeleton();
					assert(pSkeleton);

					pSkeleton->SetEventCallback(animEventCallback, (void*)this);
				}
				*/
			}
			else
			{
				//pActor->HolsterItem(true);

				m_passengerId = actorId;
				pActor->LinkToVehicle(m_pVehicle->GetEntityId());

				if (!m_disableStopAllAnimationsOnEnter)
					pActor->GetEntity()->GetCharacter(0)->GetISkeletonAnim()->StopAnimationsAllLayers();

				SetAnimGraphInputs(actorId, true, false);
				SitDown();
			}
		}

	}
	else
	{
		pActor->HolsterItem(true);

		m_passengerId = actorId;
		if (pActor != NULL)
			pActor->LinkToVehicle(m_pVehicle->GetEntityId());

		if (pActor->IsPlayer() && !m_postEnterId)
		{
			if (!m_disableStopAllAnimationsOnEnter)
			{
				if (ICharacterInstance* pCharInstance = pActor->GetEntity()->GetCharacter(0))
					pCharInstance->GetISkeletonAnim()->StopAnimationsAllLayers();
			}

			m_isPlayerViewBlending = (isTransitionEnabled && VehicleCVars().v_transitionAnimations == 1);
		}

		if (!m_postEnterId)
			SetAnimGraphInputs(actorId, true, false);
		SitDown();
	}

	//CHANGED_NETWORK_STATE(m_pVehicle, CVehicle::ASPECT_SEAT_PASSENGER);
	m_pVehicle->GetGameObject()->ChangedNetworkState(CVehicle::ASPECT_SEAT_PASSENGER);

	// adjust the vehicle update slot to start calling the seat update function
	m_pVehicle->GetGameObject()->EnableUpdateSlot(m_pVehicle, IVehicle::eVUS_PassengerIn);

	if (gEnv->bMultiplayer && gEnv->bServer)
		m_pVehicle->KillAbandonedTimer();

	return true;
}

//------------------------------------------------------------------------
bool CVehicleSeat::EnterRemotely(EntityId actorId)
{
	if (m_passengerId != 0 && actorId != m_passengerId && m_transitionType != eVT_Dying)
		return false;

	IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(actorId);
	if (!(pActor && pActor->IsPlayer()))
		return false;

	m_passengerId = actorId;
	m_transitionType = eVT_RemoteUsage;

	GivesActorSeatFeatures(true, true);

	m_pVehicle->GetGameObject()->ChangedNetworkState(CVehicle::ASPECT_SEAT_PASSENGER);

	return true;
}

//------------------------------------------------------------------------
bool CVehicleSeat::ExitRemotely()
{
	if (m_passengerId == 0 || m_transitionType != eVT_RemoteUsage)
		return false;

	GivesActorSeatFeatures(false, true);

	m_passengerId = 0;
	m_pVehicle->GetGameObject()->ChangedNetworkState(CVehicle::ASPECT_SEAT_PASSENGER);

	m_transitionType = eVT_None;

	return true;
}

//------------------------------------------------------------------------
CVehicleSeat* CVehicleSeat::GetSeatUsedRemotely(bool onlyIfBeingUsed)
{
	if (m_pRemoteSeat)
	{
		if (onlyIfBeingUsed)
		{
			if (m_pRemoteSeat->GetCurrentTransition() != eVT_RemoteUsage || m_pRemoteSeat->GetPassenger(true) != m_passengerId)
				return NULL;
		}

		if (VehicleCVars().v_driverControlledMountedGuns == 0 && IsDriver() && m_pRemoteSeat->IsGunner())
			return NULL;
	}

	return m_pRemoteSeat;
}

//------------------------------------------------------------------------
bool CVehicleSeat::SitDown()
{
	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	IActor* pActor = pActorSystem->GetActor(m_passengerId);
	if (!pActor)
		return false;

	if (pActor->IsPlayer())
		// Just for the player, we keep him in the entered state until the next frame.
		m_transitionType = eVT_Entering;
	else
		m_transitionType = eVT_None;

	if (IAISystem* pAISystem = gEnv->pAISystem)
	{
		//CryMP
		pAISystem->AddSmartObjectState(pActor->GetEntity(), "InVehicle");
	}

	IEntity* pPassengerEntity = pActor->GetEntity();

	// MR: moved this to before GiveActorSeatFeatures, as the latter sets the vehicle view, 
	// which needs the remote seat to check for potentially available remote views
	if (CVehicleSeat* pSeat = GetSeatUsedRemotely(false))
		pSeat->EnterRemotely(m_passengerId);

	GivesActorSeatFeatures(true);

	if (pActor->IsClient())
	{
		//gEnv->pInput->RetriggerKeyState();
		m_pVehicleSystem->RetriggerKeyStateNextFrame(); //CryMP: Workaround
	}

	// allow lua side of the seat implementation to do its business
	HSCRIPTFUNCTION scriptFunction(0);
	if (IScriptTable* pScriptTable = m_pVehicle->GetEntity()->GetScriptTable())
	{
		if (pScriptTable->GetValue("OnActorSitDown", scriptFunction))
		{
			assert(scriptFunction);
			ScriptHandle passengerHandle(pPassengerEntity->GetId());
			IScriptSystem* pIScriptSystem = gEnv->pScriptSystem;
			Script::Call(pIScriptSystem, scriptFunction, pScriptTable, GetSeatId(), passengerHandle);
			pIScriptSystem->ReleaseFunc(scriptFunction);
		}
	}

	// broadcast an event about the situation
	SVehicleEventParams eventParams;
	eventParams.iParam = m_seatId;
	eventParams.entityId = m_passengerId;
	m_pVehicle->BroadcastVehicleEvent(eVE_PassengerEnter, eventParams);

	m_pVehicle->GetGameObject()->ChangedNetworkState(CVehicle::ASPECT_SEAT_PASSENGER);

	if (pActor->IsClient())
	{
		if (ICVar* pBlurVar = gEnv->pConsole->GetCVar("cl_motionBlur"))
		{
			m_blurPrevVal = pBlurVar->GetFVal();
			pBlurVar->Set(m_blurPrevVal * m_blurMult);
		}

		if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
		{
			SVehicleMovementEventParams params;
			params.bValue = true;
			pMovement->OnEvent(IVehicleMovement::eVME_PlayerEnterLeaveVehicle, params);
		}

		// Send an event to MusicLogic
		m_pGameFramework->GetMusicLogic()->SetEvent(eMUSICLOGICEVENT_VEHICLE_ENTER, 0.0f);

	}

	UpdatePassengerLocalTM(pActor);

	if (m_pSeatGroup)
		m_pSeatGroup->OnPassengerEnter(this, m_passengerId);

	//gEnv->pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(pActor->GetEntity(), GameplayEvent(eGE_EnteredVehicle, 0, 0, (void*)m_pVehicle->GetEntityId()));

	return true;
}

//------------------------------------------------------------------------
bool CVehicleSeat::QueueTransition()
{
	// check if transition is prohibited by other blocking seats
	const STransitionInfo& info = m_pVehicle->GetTransitionInfoForSeat(m_seatId);

	if (!info.waitFor.empty())
	{
		for (int i = 0, nWait = info.waitFor.size(); i < nWait; ++i)
		{
			CVehicleSeat* pSeat = (CVehicleSeat*)m_pVehicle->GetSeatById(info.waitFor[i]);
			assert(pSeat);

			int trans = pSeat->GetCurrentTransition();

			if (trans == eVT_Entering || trans == eVT_Exiting)
				return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicleSeat::Exit(bool isTransitionEnabled, bool force/*=false*/)
{
	if (m_transitionType == eVT_RemoteUsage)
		return ExitRemotely();

	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	IActor* pActor = pActorSystem->GetActor(m_passengerId);

	if (!exitPos.IsEquivalent(ZERO))
		m_exitWorldPos = exitPos;

	// check the player can exit the vehicle from somewhere...
	// This just ensures that when we reach StandUp there will be a valid place to exit from.
	// Changed this for MP: only local client should decide whether it can get out or not (since it
	//	will also decide where to place itself).
	if (pActor && pActor->IsClient())
	{
		Matrix34 worldTM = GetExitTM(pActor, false);
		m_exitWorldPos = worldTM.GetTranslation();

		EntityId blockerId = 0;
		bool canExit = TestExitPosition(pActor, m_exitWorldPos, &blockerId);

		// if our own seat exit is blocked, ask the vehicle for a free one...
		if (!canExit && !force)
		{
			canExit = m_pVehicle->GetExitPositionForActor(pActor, m_exitWorldPos, false);
		}

		if (!canExit && !force)
		{
			m_exitWorldPos.zero();
			return false;
		}
	}

	if (pActor)
	{
		if (!pActor->IsPlayer())
		{
			if (isTransitionEnabled && !force)
			{
				if (QueueTransition())
				{
					m_queuedTransition = eVT_Exiting;
					return true;
				}
			}

			if (m_transitionType == eVT_Entering)
			{
				// reset actor target request
				/*			IAIObject* pAIObject = pActor->GetEntity()->GetAI();
				if ( pAIObject )
				{
				CAIProxy* pAIProxy = (CAIProxy*) pAIObject->GetProxy();
				pAIProxy->SetActorTarget( this, NULL );
				}*/
			}
			else if (m_transitionType == eVT_None)
			{
				m_adjustedExitPos.zero();

				if (IAIObject* pAIObject = pActor->GetEntity()->GetAI())
				{
					Matrix34 worldTM = GetExitTM(pActor);
					Vec3 adjustedPos;

					//CryMP:
					if (IAIActor* aiactor = pAIObject->CastToIAIActor())
					{
						if (pAIObject->GetValidPositionNearby(worldTM.GetTranslation(), adjustedPos))
						{
							if (worldTM.GetTranslation() != adjustedPos)
								m_adjustedExitPos = adjustedPos;
						}
					}
				}
			}
		}
		else
		{
			/*m_adjustedExitPos.zero();
			Matrix34 worldTM = GetExitTM(pActor);
			Vec3 pos = worldTM.GetTranslation();
			int i = 0;
			bool valid = false;

			while (!valid)
			valid = TestExitPos(pActor, pos, i++);

			if (pos != worldTM.GetTranslation())
			m_adjustedExitPos = pos;
			*/
		}
	}

	m_transitionType = pActor && (m_transitionType != eVT_Entering) ? eVT_Exiting : eVT_None;
	m_queuedTransition = eVT_None;

	GivesActorSeatFeatures(false);

	if (pActor)
	{
		if (!pActor->IsPlayer())
		{
			if (m_transitionType == eVT_Exiting)
			{
				if (m_pSitHelper && IsRotatable() && !gEnv->pSystem->IsSerializingFile())
				{
					Matrix34 mat;
					mat.SetIdentity();
					mat.SetTranslation(m_pSitHelper->GetVehicleTM().GetTranslation());
					pActor->GetEntity()->SetLocalTM(mat);
				}

				if (!SetAnimGraphInputs(m_passengerId, false, isTransitionEnabled))
					StandUp();

				if (!isTransitionEnabled || force || IsDemoPlayback())
					StandUp();
			}
			else
			{
				bool temp = m_movePassengerOnExit;
				m_movePassengerOnExit = false;
				StandUp();
				m_movePassengerOnExit = temp;
			}
		}
		else
		{
			SetAnimGraphInputs(m_passengerId, false, false);
			StandUp();
		}

		if (m_pSeatGroup)
			m_pSeatGroup->OnPassengerExit(this, m_passengerId);

		if (!m_exitWorldPos.IsZero())
		{
			Matrix34 worldTM = pActor->GetEntity()->GetWorldTM();
			worldTM.SetTranslation(m_exitWorldPos);
			pActor->GetEntity()->SetWorldTM(worldTM);
		}
	}

	m_adjustedExitPos.zero();

	if (pActor && pActor->IsClient())
	{
		if (ICVar* pBlurVar = gEnv->pConsole->GetCVar("cl_motionBlur"))
		{
			pBlurVar->Set(m_blurPrevVal);
			m_blurPrevVal = 0.0f;
		}

		if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
		{
			SVehicleMovementEventParams params;
			params.bValue = false;
			pMovement->OnEvent(IVehicleMovement::eVME_PlayerEnterLeaveVehicle, params);
		}

		// Send an event to MusicLogic
		m_pGameFramework->GetMusicLogic()->SetEvent(eMUSICLOGICEVENT_VEHICLE_LEAVE, 0.0f);
	}

	if (!pActor)
	{
		// seems to be necessary - had a crash on sv_restart, possibly because the actor has already been
		//	deleted, so m_passenger id is never reset, so ~CVehicleSeat() tries to call Exit again. This ends up calling
		// m_pVehicle->IsEmpty() below on a half-destructed vehicle
		m_passengerId = 0;
		return true;
	}

	if (gEnv->bMultiplayer && gEnv->bServer && m_pVehicle->IsEmpty())
		m_pVehicle->StartAbandonedTimer();

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeat::SetAnimGraphInput(EntityId actorId, EAIAGInput input, const char* value)
{
	IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(actorId);
	IAnimationGraphState* pState = GetAnimGraphState(actorId);

	if (pActor && pState)
	{
		if (!pActor->IsPlayer())
		{
			if (IAIObject* pAIObject = pActor->GetEntity()->GetAI())
			{
				//CryMP
				if (IUnknownProxy* pAIProxy = pAIObject->GetProxy())
				{
					pAIProxy->SetAGInput(input, value);
				}
			}
		}
		else
		{
			const char* inputName = input == AIAG_ACTION ? "Action" : "Signal";
			IAnimationGraphState::InputID inputId = pState->GetInputId(inputName);
			pState->SetInput(inputId, value);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::ResetAnimGraphInput(EntityId actorId, EAIAGInput input)
{
	IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(actorId);
	IAnimationGraphState* pState = GetAnimGraphState(actorId);

	if (pActor && pState)
	{
		if (!pActor->IsPlayer())
		{
			if (IAIObject* pAIObject = pActor->GetEntity()->GetAI())
			{
				//CryMP
				if (IUnknownProxy* pAIProxy = pAIObject->GetProxy())
				{
					pAIProxy->ResetAGInput(input);
				}
			}
		}
		else
		{
			const char* inputName = input == AIAG_ACTION ? "Action" : "Signal";
			IAnimationGraphState::InputID inputId = pState->GetInputId(inputName);
			pState->SetInput(inputId, "idle");
		}
	}
}

//------------------------------------------------------------------------
bool CVehicleSeat::StandUp()
{
	if (m_passengerId == 0)
		return false;

	IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_passengerId);
	if (!pActor)
		return false;

	IAnimatedCharacter* pAnimChar = (IAnimatedCharacter*)pActor->GetGameObject()->QueryExtension("AnimatedCharacter");
	if (pAnimChar)
	{
		SAnimatedCharacterParams params = pAnimChar->GetParams();
		params.flags |= eACF_EnableMovementProcessing;
		pAnimChar->SetParams(params);
	}

	IEntity* pPassengerEntity = pActor->GetEntity();

	if (IAISystem* pAISystem = gEnv->pAISystem)
	{
		//CryMP
		pAISystem->RemoveSmartObjectState(pActor->GetEntity(), "InVehicle");
	}

	// allow lua side of the seat implementation to do its business
	HSCRIPTFUNCTION scriptFunction(0);
	if (IScriptTable* pScriptTable = m_pVehicle->GetEntity()->GetScriptTable())
	{
		if (pScriptTable->GetValue("OnActorStandUp", scriptFunction))
		{
			assert(scriptFunction);
			ScriptHandle passengerHandle(pPassengerEntity->GetId());
			IScriptSystem* pIScriptSystem = gEnv->pScriptSystem;
			Script::Call(pIScriptSystem, scriptFunction, pScriptTable, passengerHandle, true);
			pIScriptSystem->ReleaseFunc(scriptFunction);
		}
	}

	pActor->LinkToVehicle(0);

	if (pActor->IsClient())
	{
		//gEnv->pInput->RetriggerKeyState();
		m_pVehicleSystem->RetriggerKeyStateNextFrame(); //CryMP: Workaround
	}

	// current world transform of passenger
	// we always want to use the rotational part of this
	// and we want to use the position if he played an exit animation
	Matrix34 passengerTM = pPassengerEntity->GetWorldTM();
	Vec3 exitPos = passengerTM.GetTranslation();
	// this only gets set to true if the world transform should really be updated
	bool	dirtyPos = false;

	if (m_movePassengerOnExit)
	{
		if (!m_adjustedExitPos.IsZero())
		{
			// something external is trying to adjust the exit position ... so we will just let it do it
			exitPos = m_adjustedExitPos;
			dirtyPos = true;
		}
		else
		{
			// ok, we get to choose the exit position ourselves
			// we assume that the exit animation will get us reasonably close to the exit helper (0.5m)
			// if not, we do a fallback teleport
			// the teleport is there to solve two case
			//		- we don't have an exit animation
			//		- something messed up playing the exit animation
			static float teleportDistanceSq = 0.25f;

			// by this point m_exitWorldPos should be valid (non-zero)
			if (m_exitWorldPos.GetLengthSquared() > 0.01f)
			{
				exitPos = m_exitWorldPos;
				dirtyPos = true;
			}
			else
			{
				// if not, fallback to the old method and just exit this seat
				Matrix34 exitHelperTM = GetExitTM(pActor);
				Vec3 delta = exitHelperTM.GetTranslation() - exitPos;
				if (delta.GetLengthSquared() > teleportDistanceSq)
				{
					// teleport
					exitPos = exitHelperTM.GetTranslation();
					dirtyPos = true;
				}
			}
		}
	}

	// sanity check the exit position
	{
		float elevation = gEnv->p3DEngine->GetTerrainElevation(exitPos.x, exitPos.y);

		if (exitPos.z < elevation && (elevation - exitPos.z) < 5.0f)
		{
			// we are under the terrain ... but this can be fine if we are inside a VisArea (e.g. a tunnel inside the terrain)
			Vec3 vehiclePos = m_pVehicle->GetEntity()->GetWorldPos();
			IVisArea* pVehicleVisArea = gEnv->p3DEngine->GetVisAreaFromPos(vehiclePos);
			IVisArea* pExitVisArea = gEnv->p3DEngine->GetVisAreaFromPos(exitPos);

			if (!(pVehicleVisArea && pExitVisArea))
			{
				dirtyPos = true;
				exitPos.z = max(exitPos.z, elevation);
			}
		}
	}

	if (dirtyPos)
	{
		// set the position, but keep the orientation
		passengerTM.SetTranslation(exitPos);
		// move the entity( during serialize, don't change the postiin
		if (!gEnv->pSystem->IsSerializingFile())
			pPassengerEntity->SetWorldTM(passengerTM, ENTITY_XFORM_USER);
	}

	if (VehicleCVars().v_debugdraw > 0)
	{
		IPersistantDebug* pDB = gEnv->pGame->GetIGameFramework()->GetIPersistantDebug();
		pDB->Begin("Seat", false);
		pDB->AddDirection(passengerTM.GetTranslation() + Vec3(0, 0, 0.5f), 0.25f, passengerTM.GetColumn(1), ColorF(1, 1, 0, 1), 5.f);
	}

	// downgrade the vehicle update slot
	m_pVehicle->GetGameObject()->DisableUpdateSlot(m_pVehicle, IVehicle::eVUS_PassengerIn);

	// broadcast an event about the passenger exiting
	SVehicleEventParams eventParams;
	eventParams.iParam = m_seatId;
	eventParams.entityId = pActor->GetEntityId();
	m_pVehicle->BroadcastVehicleEvent(eVE_PassengerExit, eventParams);

	// this is now implemented in CAnimatedCharacter. this would override that
	//pActor->GetEntity()->GetCharacter(0)->GetISkeleton()->SetEventCallback(NULL, NULL);

	m_passengerId = 0;

	m_pVehicle->GetGameObject()->ChangedNetworkState(CVehicle::ASPECT_SEAT_PASSENGER);

	if (!gEnv->pSystem->IsSerializingFile()) //serialization will fix that automatically
	{
		if (pActor->GetHealth() > 0)
		{
			if (IInventory* pInventory = pActor->GetInventory())
			{
				if (!pInventory->GetCurrentItem())
					pActor->HolsterItem(false);

				IItem* currentItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pInventory->GetCurrentItem());
				IMaterial* currentReplacementMat = pActor->GetReplacementMaterial();
				if (currentItem && currentReplacementMat)
				{
					currentItem->Cloak(false);
					currentItem->Cloak(true, currentReplacementMat);
				}
			}
		}
		else
		{
			pActor->GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);
		}

		if (pActor->IsPlayer())
		{
			pActor->GetEntity()->SetRotation(m_pVehicle->GetEntity()->GetRotation());
			pActor->GetEntity()->Hide(false);
		}
	}

	m_transitionType = eVT_None;
	RemoveStateListener(pActor->GetAnimationGraphState());

	if (pActor->IsClient())
	{
		StopSounds();
	}

	eventParams.entityId = pActor->GetEntityId();
	eventParams.iParam = m_seatId;
	m_pVehicle->BroadcastVehicleEvent(eVE_SeatFreed, eventParams);

	//gEnv->pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(pActor->GetEntity(), GameplayEvent(eGE_LeftVehicle, 0, 0, (void*)m_pVehicle->GetEntityId()));

	m_exitWorldPos.zero();

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeat::OnSeatFreed(TVehicleSeatId seatId, EntityId passengerId)
{
	if (seatId == m_seatId || !m_passengerId || m_passengerId == passengerId)
		return;

	// if remote seat freed by someone else, try to occupy it
	if (!gEnv->pSystem->IsSerializingFile())
	{
		if (CVehicleSeat* pSeat = GetSeatUsedRemotely(false))
		{
			if (pSeat->GetSeatId() == seatId)
			{
				pSeat->EnterRemotely(m_passengerId);
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::GivesActorSeatFeatures(bool enabled, bool isRemote)
{
	IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(m_passengerId);
	INetContext* pNetContext = m_pGameFramework->GetNetContext();

	if (enabled && !m_pVehicle->IsDestroyed() && (pActor && pActor->GetHealth() > 0 || gEnv->pSystem->IsSerializingFile())) // move this to PostSerialize (after alpha build..)
	{
		if (m_isDriver)
		{
			SetAnimGraphInput(m_passengerId, AIAG_ACTION, "drive");

			// give authority of the vehicle entity to whoever just sat down
			if (gEnv->bServer && pActor->IsPlayer())
			{
				INetChannel* pNetChannel = m_pGameFramework->GetNetChannel(pActor->GetChannelId());
				if (pNetChannel && pNetContext)
					pNetContext->DelegateAuthority(m_pVehicle->GetEntityId(), pNetChannel);
			}
		}

		// enable the seat actions
		for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
		{
			SSeatActionData& seatActionData = *ite;

			if (!isRemote || isRemote && seatActionData.isAvailableRemotely)
				seatActionData.pSeatAction->StartUsing(m_passengerId);
		}

		if (!isRemote && pActor->IsClient())
		{
			bool isThirdPerson = true;
			if (IVehicleView* pView = GetView(GetCurrentView()))
				isThirdPerson = pView->IsThirdPerson();
			/*
						else
							isThirdPerson = pActor->IsThirdPerson();
			*/

			if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
			{
				SVehicleMovementEventParams params;
				params.fValue = isThirdPerson ? 0.f : 1.f;
				params.bValue = true;
				pMovement->OnEvent(IVehicleMovement::eVME_PlayerEnterLeaveSeat, params);
			}

			if (IVehicleClient* pVehicleClient = GetVehicleClient())
				pVehicleClient->OnEnterVehicleSeat(this);

			EnableVehicleActionMaps(true);
		}

		// give authority of the seat serializer to whoever just sat down
		if (gEnv->bServer && pActor->IsPlayer() && gEnv->pEntitySystem->GetEntity(m_serializerId))
		{
			INetChannel* pNetChannel = m_pGameFramework->GetNetChannel(pActor->GetChannelId());

			if (pNetChannel && pNetContext)
				pNetContext->DelegateAuthority(m_serializerId, pNetChannel);
		}

		if (!isRemote)
			UpdateHiddenState(true);
	}
	else if (!enabled)
	{
		if (!isRemote)
			UpdateHiddenState(false);

		if (CVehicleSeat* pSeat = GetSeatUsedRemotely(true))
			pSeat->ExitRemotely();

		if (pActor)
		{
			if (m_isDriver)
				ResetAnimGraphInput(m_passengerId, AIAG_ACTION);

			if (!isRemote && pActor->IsClient())
			{
				if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
				{
					SVehicleMovementEventParams params;
					params.fValue = 0.0f;
					params.bValue = false;
					pMovement->OnEvent(IVehicleMovement::eVME_PlayerEnterLeaveSeat, params);
				}

				if (IVehicleClient* pVehicleClient = GetVehicleClient())
					pVehicleClient->OnExitVehicleSeat(this);

				if (IVehicleView* pView = GetView(GetCurrentView()))
					pView->OnStopUsing();

				StopSounds();

				EnableVehicleActionMaps(false);
			}

			if (m_isDriver && pNetContext && gEnv->bServer)
				pNetContext->DelegateAuthority(m_pVehicle->GetEntityId(), 0);
		}

		// give authority of the seat serializer to server
		if (pNetContext && gEnv->bServer && gEnv->pEntitySystem->GetEntity(m_serializerId))
			pNetContext->DelegateAuthority(m_serializerId, 0);

		// disable the seat actions
		for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
		{
			SSeatActionData& seatActionData = *ite;

			if (!isRemote || seatActionData.isAvailableRemotely)
				seatActionData.pSeatAction->StopUsing();
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::EnableVehicleActionMaps(bool enable)
{
	// illegal to change action maps in demo playback - Lin
	if (IsDemoPlayback())
		return;

	IActionMapManager* pActionMapMan = gEnv->pGame->GetIGameFramework()->GetIActionMapManager();

	if (IActionMap* pActionMap = pActionMapMan->GetActionMap("vehicle"))
	{
		pActionMapMan->EnableActionMap("player", !enable); //CryMP
		pActionMap->SetActionListener(enable ? m_passengerId : 0);
		pActionMapMan->EnableActionMap("vehicle", enable);
	}

	const string& actionMap = m_pVehicle->GetActionMap();
	if (!actionMap.empty())
	{
		if (IActionMap* pActionMap = pActionMapMan->GetActionMap(actionMap.c_str()))
		{
			pActionMap->SetActionListener(enable ? m_passengerId : 0);
			pActionMapMan->EnableActionMap(actionMap.c_str(), enable);
		}
	}
}

//------------------------------------------------------------------------
EntityId CVehicleSeat::GetPassenger(bool remoteUser/*=false*/)
{
	if (!remoteUser && m_transitionType == eVT_RemoteUsage)
		return 0;

	return m_passengerId;
}

//------------------------------------------------------------------------
IActor* CVehicleSeat::GetPassengerActor(bool remoteUser/*=false*/)
{
	EntityId id = GetPassenger(remoteUser);

	if (0 == id)
		return NULL;

	return gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(id);
}

//------------------------------------------------------------------------
bool CVehicleSeat::IsPassengerClientActor() const
{
	if (m_passengerId)
	{
		IActor* pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
		if (pActor && pActor->GetEntityId() == m_passengerId)
			return true;
	}

	return false;
}

//------------------------------------------------------------------------
int CVehicleSeat::GetSeatActionId(IVehicleSeatAction* pAction)
{
	int action = 0;
	for (TVehicleSeatActionVector::const_iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite, ++action)
	{
		const SSeatActionData& seatActionData = *ite;
		if (seatActionData.pSeatAction == pAction)
			return action;
	}

	return -1;
}

//------------------------------------------------------------------------
IVehicleSeatAction* CVehicleSeat::GetSeatActionById(int id)
{
	if (id >= 0)
	{
		int action = 0;
		for (TVehicleSeatActionVector::const_iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite, ++action)
		{
			const SSeatActionData& seatActionData = *ite;
			if (id == action)
				return seatActionData.pSeatAction;
		}
	}
	return 0;
}


//------------------------------------------------------------------------
void CVehicleSeat::ChangedNetworkState(EEntityAspects aspects)
{
	if (m_pSerializer && gEnv->pEntitySystem->GetEntity(m_serializerId))
	{
		//CHANGED_NETWORK_STATE(m_pSerializer, aspects);
		m_pSerializer->GetGameObject()->ChangedNetworkState(aspects);
	}
}

//------------------------------------------------------------------------
bool CVehicleSeat::IsFree()
{
	if (m_isLocked)
		return false;

	if (!m_passengerId)
		return true;

	IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_passengerId);
	if (!pActor)
		return true;

	if (m_transitionType == eVT_RemoteUsage)
		return true;

	if (m_transitionType == eVT_Entering || m_transitionType == eVT_Exiting
		|| m_transitionType == eVT_SeatIsBorrowed)
	{
		if (pActor->GetHealth() > 0)
			return false;
		else
			return true;
	}

	if (pActor->GetHealth() <= 0)
		return true;

	return false;
}

//------------------------------------------------------------------------
void CVehicleSeat::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	assert(pActorSystem);

	IActor* pActor = pActorSystem->GetActor(m_passengerId);

	if (!pActor)
	{
		return;
	}

	if (pActor->GetHealth() <= 0)
		return;

	if (!m_pVehicle->IsDestroyed())
	{
		for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
		{
			SSeatActionData& seatActionData = *ite;

			if (m_transitionType == eVT_None ||
				(m_transitionType == eVT_RemoteUsage && seatActionData.isAvailableRemotely) ||
				(m_transitionType == eVT_Entering && pActor->IsClient()))
			{
				seatActionData.pSeatAction->OnAction(actionId, activationMode, value);
			}
		}

		if (CVehicleSeat* pRemoteSeat = GetSeatUsedRemotely(true))
			pRemoteSeat->OnAction(actionId, activationMode, value);
	}

	if (actionId == eVAI_ChangeView)
		SetView(GetNextView(GetCurrentView()));

	if (IVehicleView* pView = GetView(GetCurrentView()))
		pView->OnAction(actionId, activationMode, value);
}

//------------------------------------------------------------------------
void CVehicleSeat::Update(float deltaTime)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if (m_passengerId)
	{
		// update passenger TM
		if (IActor* pActor = GetPassengerActor())
		{
			if (m_transitionType == eVT_Entering && pActor->IsPlayer())
			{
				m_transitionType = eVT_None;
			}

			if (m_transitionType == eVT_None || m_transitionType == eVT_Dying)
			{
				UpdatePassengerLocalTM(pActor);

				if (m_pSitHelper && VehicleCVars().v_draw_passengers == 1)
					VehicleUtils::DrawTM(m_pSitHelper->GetWorldTM(), "VehicleSeat::Update", true);

				if (pActor->IsClient())
				{
					m_pVehicle->UpdateHUD(deltaTime);
					UpdateRemote(pActor, deltaTime);

					//if (VehicleCVars().v_debugdraw)
					  //GetExitTM(pActor);
				}

				if (pActor && (m_transitionType == eVT_Dying) && (!pActor->IsPlayer()))
				{
					if (ICharacterInstance* pCharacter = pActor->GetEntity()->GetCharacter(0))
						if (ISkeletonPose* pSkeli = pCharacter->GetISkeletonPose())
							if (IPhysicalEntity* pPE = pSkeli->GetCharacterPhysics())
							{
								pe_params_part ppp;
								ppp.flagsAND = ~(geom_colltype_explosion | geom_colltype_ray | geom_colltype_foliage_proxy);
								pPE->SetParams(&ppp);
							}
				}
			}
			else if (m_transitionType == eVT_RemoteUsage)
			{
				for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
				{
					SSeatActionData& seatActionData = *ite;

					if (seatActionData.isAvailableRemotely)
						seatActionData.pSeatAction->Update(deltaTime);
				}
			}
			else if (m_transitionType == eVT_ExitingWarped)
			{
				Vec3 warpToPos;
				AABB worldBounds;

				pActor->GetEntity()->GetWorldBounds(worldBounds);

				if (!gEnv->pRenderer->GetCamera().IsAABBVisible_F(worldBounds))
				{
					if (IAIObject* pAIObject = pActor->GetEntity()->GetAI())
					{
						if (IAIActor* aiactor = pAIObject->CastToIAIActor())
						{
							if (pAIObject->GetTeleportPosition(m_adjustedExitPos))
							{
								Exit(false);
								StandUp();
							}
						}
					}
				}
			}

			if (m_queuedTransition == eVT_Exiting)
			{
				if (!QueueTransition())
					Exit(true, false);
			}

			UpdateSounds(deltaTime);
		}
	}
}

void CVehicleSeat::UpdateRemote(IActor* pActor, float deltaTime)
{
	CVehicleSeat* pSeat = GetSeatUsedRemotely(true);
	if (!pSeat)
		return;

	if (pSeat->IsGunner() && VehicleCVars().v_driverControlledMountedGuns)
	{
		// merge this with vehicle weapon raycast (after alpha)
		CVehicleSeatActionRotateTurret* pTurretAction = NULL;

		TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();
		for (TVehicleSeatActionVector::const_iterator it = seatActions.begin(); it != seatActions.end(); ++it)
		{
			if (!it->isAvailableRemotely)
				continue;

			if (pTurretAction = CAST_VEHICLEOBJECT(CVehicleSeatActionRotateTurret, it->pSeatAction))
				break;
		}

		IMovementController* pMC = (pActor != NULL) ? pActor->GetMovementController() : NULL;
		if (pMC && pTurretAction)
		{
			IVehicleView* pView = GetView(GetCurrentView());
			if (pView && pView->IsAvailableRemotely() && pView->IsThirdPerson())
			{
				// remote thirdperson view uses AimPart as usual
				pTurretAction->SetAimGoal(Vec3());
			}
			else
			{
				// when using fp view, set AimGoal on Turret action
				SMovementState info;
				pMC->GetMovementState(info);

				ray_hit rayhit;
				static IPhysicalEntity* pSkipEnts[10];
				pSkipEnts[0] = m_pVehicle->GetEntity()->GetPhysics();
				int nskip = 0;

				for (int i = 0, count = m_pVehicle->GetEntity()->GetChildCount(); i < count && nskip < 9; ++i)
				{
					IEntity* pChild = m_pVehicle->GetEntity()->GetChild(i);
					IPhysicalEntity* pPhysics = pChild->GetPhysics();
					if (pPhysics)
					{
						// todo: shorter way to handle this?
						if (pPhysics->GetType() == PE_LIVING)
							if (ICharacterInstance* pCharacter = pChild->GetCharacter(0))
								if (IPhysicalEntity* pCharPhys = pCharacter->GetISkeletonPose()->GetCharacterPhysics())
									pPhysics = pCharPhys;

						pSkipEnts[++nskip] = pPhysics;
					}
				}

				int hits = gEnv->pPhysicalWorld->RayWorldIntersection(info.weaponPosition, info.aimDirection * 200.f, ent_all,
					rwi_stop_at_pierceable | rwi_colltype_any, &rayhit, 1, pSkipEnts, nskip + 1);

				if (hits)
					pTurretAction->SetAimGoal(rayhit.pt);
				else
					pTurretAction->SetAimGoal(info.weaponPosition + 1000.f * info.aimDirection);
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::UpdatePassengerLocalTM(IActor* pActor)
{
	if (0 == VehicleCVars().v_set_passenger_tm)
		return;

	if (!m_pSitHelper)
		return;

	// todo: optimize this to only update after invalidation   
	pActor->GetEntity()->SetLocalTM(m_pSitHelper->GetVehicleTM());
}

//------------------------------------------------------------------------
IVehicleView* CVehicleSeat::GetView(TVehicleViewId viewId)
{
	if (viewId < 1 || viewId > m_views.size())
		return NULL;

	IVehicleView* pView = m_views[viewId - 1];

	if (CVehicleSeat* pSeat = GetSeatUsedRemotely(true))
	{
		if (IVehicleView* pRemoteView = pSeat->GetView(viewId))
		{
			if (pRemoteView->IsAvailableRemotely() && pRemoteView->IsThirdPerson() == pView->IsThirdPerson())
				pView = pRemoteView;
		}
	}

	return pView;
}

//------------------------------------------------------------------------
bool CVehicleSeat::SetView(TVehicleViewId viewId)
{
	if (viewId < InvalidVehicleViewId || viewId > m_views.size() || viewId == GetCurrentView())
		return false;

	if (IVehicleView* pOldView = GetView(GetCurrentView()))
		pOldView->OnStopUsing();

	if (viewId != InvalidVehicleViewId)
	{
		IActor* pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
		if (!pActor || pActor->GetEntityId() != m_passengerId)
			return false;

		IVehicleView* pNewView = GetView(viewId);
		if (!pNewView)
			return false;

		pNewView->OnStartUsing(m_passengerId);

		if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
		{
			SVehicleMovementEventParams params;
			params.fValue = pNewView->IsThirdPerson() ? 0.0f : 1.0f;
			pMovement->OnEvent(IVehicleMovement::eVME_PlayerSwitchView, params);
		}
	}

	m_currentView = viewId;

	return true;
}

//------------------------------------------------------------------------
TVehicleViewId CVehicleSeat::GetNextView(TVehicleViewId viewId)
{
	if (viewId == InvalidVehicleViewId && m_views.empty())
		return InvalidVehicleViewId;

	TVehicleViewId next = viewId + 1;
	if (next > m_views.size())
		next = 1;

	if (0 == VehicleCVars().v_debugView && GetView(next)->IsDebugView())
		++next;

	if (next > m_views.size())
		next = 1;

	return next;
}

//------------------------------------------------------------------------
void CVehicleSeat::UpdateView(SViewParams& viewParams)
{
	assert(m_passengerId);

	if (m_transitionType == eVT_None)
	{
		if (IVehicleView* pView = GetView(GetCurrentView()))
			pView->UpdateView(viewParams, m_passengerId);
	}
	else
	{
		if (IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(m_passengerId))
		{
			IEntity* pEntity = pActor->GetEntity();
			assert(pEntity);

			if (ICharacterInstance* pCharInstance = pEntity->GetCharacter(0))
			{
				ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose();
				assert(pSkeletonPose);

				int16 headId = pSkeletonPose->GetJointIDByName("Bip01 Head");
				if (headId > -1)
				{
					Matrix34 worldTM = pEntity->GetSlotWorldTM(0) * Matrix34(pSkeletonPose->GetAbsJointByID(headId));
					worldTM.OrthonormalizeFast();

					viewParams.position = worldTM.GetTranslation();
					viewParams.rotation = GetQuatFromMat33(Matrix33(worldTM)).GetNormalized() * Quat::CreateRotationY(gf_PI * 0.5f);

					if (ICVar* pVar = gEnv->pConsole->GetCVar("cl_fov"))
						viewParams.fov = DEG2RAD(pVar->GetFVal());
					else
						viewParams.fov = 0.0f;

					viewParams.nearplane = 0.0f;
				}
			}
		}
	}

	if (!m_isPlayerViewBlending || m_skipViewBlending)
	{
		viewParams.SetViewID(2, false);
		m_skipViewBlending = false;
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::UpdateSounds(float deltaTime)
{
	if (!(m_transitionType != eVT_RemoteUsage && IsPassengerClientActor()))
		return;

	ISoundMoodManager* pMoodMan = gEnv->pSoundSystem->GetIMoodManager();
	if (!pMoodMan)
		return;

	static const char* moodName = "vehicle";
	SSeatSoundParams& params = GetSoundParams();

	float mood = params.mood;

	if (IVehicleView* pView = GetView(GetCurrentView()))
		if (pView->IsThirdPerson())
			mood = 0.f;

	if (mood != params.moodCurr)
	{
		if (params.moodCurr == 0.f)
		{
			pMoodMan->RegisterSoundMood(moodName);
			pMoodMan->UpdateSoundMood(moodName, mood, 100);
		}
		else if (mood == 0.f)
		{
			pMoodMan->UnregisterSoundMood(moodName);
		}
		else
		{
			pMoodMan->UpdateSoundMood(moodName, mood, 100);
		}
		params.moodCurr = mood;
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::StopSounds()
{
	ISoundMoodManager* pMoodMan = gEnv->pSoundSystem->GetIMoodManager();
	if (!pMoodMan)
		return;

	pMoodMan->UnregisterSoundMood("vehicle");
	GetSoundParams().moodCurr = 0.f;
}

//------------------------------------------------------------------------
EntityId CVehicleSeat::NetGetPassengerId() const
{
	return m_passengerId;
}

//------------------------------------------------------------------------
void CVehicleSeat::NetSetPassengerId(EntityId passengerId)
{
	if (passengerId != m_passengerId)
	{
		//CryLog("%s::NetSetPassenger: <%s> %i -> %i", m_pVehicle->GetEntity()->GetClass()->GetName(), m_name.c_str(), m_passengerId, passengerId);

		IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
		assert(pActorSystem);

		IActor* pActor = pActorSystem->GetActor(passengerId ? passengerId : m_passengerId);
		if (!pActor)
			return;

		// resolve the previous seat situation if any,
		// because passengers need to leave their current seat before entering the new one
		CVehicleSeat* pPrevSeat = (CVehicleSeat*)m_pVehicle->GetSeatForPassenger(passengerId);
		if (pPrevSeat)
		{
			if (pPrevSeat->m_transitionType == eVT_RemoteUsage)
				pPrevSeat->ExitRemotely();
			else
			{
				pActor->GetEntity()->GetCharacter(0)->GetISkeletonAnim()->StopAnimationsAllLayers();

				pPrevSeat->GivesActorSeatFeatures(false);

				pPrevSeat->SetAnimGraphInputs(passengerId, false, false);
				pPrevSeat->m_passengerId = 0;

				SVehicleEventParams ep;
				ep.entityId = passengerId;
				ep.iParam = m_seatId;
				m_pVehicle->BroadcastVehicleEvent(eVE_SeatFreed, ep);
			}
		}

		EntityId exitId = m_passengerId ? m_passengerId : 0;
		EntityId enterId = ((pActor->GetHealth() > 0) && !m_pVehicle->IsDestroyed() && passengerId) ? passengerId : 0;

		if (exitId)
		{
			if (m_prevTransitionType == eVT_RemoteUsage)
				ExitRemotely();
			else
			{
				if (!pActor->IsClient())
				{
					Exit(false, true);
					//CryMP: Unfortunately no leave seat rmi so we have to catch NetSetPassengerId(0) in NetSerialize
				}
				else
				{
					pActor->GetEntity()->GetCharacter(0)->GetISkeletonAnim()->StopAnimationsAllLayers();

					GivesActorSeatFeatures(false);

					SetAnimGraphInputs(exitId, false, false);
				}

				m_passengerId = 0;

				SVehicleEventParams ep;
				ep.entityId = exitId;
				ep.iParam = m_seatId;
				m_pVehicle->BroadcastVehicleEvent(eVE_SeatFreed, ep);
			}

			m_prevTransitionType = eVT_None;
		}

		if (enterId)
		{
			if (pActor->GetLinkedEntity() != m_pVehicle->GetEntity())
			{
				Enter(enterId, false);
			}
			else
			{
				m_passengerId = passengerId;
				if (m_transitionType != eVT_RemoteUsage)
				{
					SetAnimGraphInputs(enterId, true, false);
					SitDown();
				}
				else
				{
					EnterRemotely(enterId);
				}
			}
			m_prevTransitionType = m_transitionType;
		}

		//CHANGED_NETWORK_STATE(m_pVehicle, CVehicle::ASPECT_SEAT_PASSENGER);
		m_pVehicle->GetGameObject()->ChangedNetworkState(CVehicle::ASPECT_SEAT_PASSENGER);
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		if (ser.IsWriting())
		{
			ser.Value("passengerId", m_passengerId, 'eid');
			ser.Value("isLocked", m_isLocked);
			ser.Value("transition", m_transitionType);
			ser.Value("queueTransition", m_queuedTransition);
		}
		else if (ser.IsReading())
		{
			EntityId passengerId;
			int transitionType = 0, queueTransitionType = 0;

			ser.Value("passengerId", passengerId, 'eid');
			ser.Value("isLocked", m_isLocked);
			ser.Value("transition", transitionType);
			ser.Value("queueTransition", queueTransitionType);

			IActor* pActor = NULL;
			if (passengerId)
				pActor = m_pGameFramework->GetIActorSystem()->GetActor(passengerId);

			IActor* pActorBefore = NULL;
			if (m_passengerId)
			{
				pActorBefore = m_pGameFramework->GetIActorSystem()->GetActor(m_passengerId);
				if ((pActorBefore != NULL) && (pActorBefore->GetHealth() <= 0.0f))
					Exit(false, true);
			}

			// remove current passenger if necessary
			if (m_passengerId && (!pActor || passengerId != m_passengerId))
				Exit(false, true);

			if (pActor)
			{
				bool remote = (transitionType == eVT_RemoteUsage);

				if (passengerId != GetPassenger(remote))
				{
					// exit previous seat if any
					if (!remote)
					{
						if (IVehicle* pPrevVehicle = pActor->GetLinkedVehicle())
						{
							if (IVehicleSeat* pPrevSeat = pPrevVehicle->GetSeatForPassenger(pActor->GetEntityId()))
								pPrevSeat->Exit(false, true);
						}
					}

					m_postEnterId = passengerId;

					/*if (remote)
			EnterRemotely(passengerId);
		  else
			Enter(passengerId, false);
		  */
				}
			}

			m_transitionType = transitionType;
			m_queuedTransition = queueTransitionType;
			m_passengerId = passengerId;
			m_pVehicle->GetGameObject()->EnableUpdateSlot(m_pVehicle, IVehicle::eVUS_PassengerIn);
		}

		TVehicleViewId view = m_currentView;
		ser.Value("currentViewId", view);

		if (ser.IsReading() && view != m_currentView && m_transitionType != eVT_RemoteUsage)
		{
			m_postViewId = view;
		}

		if (IVehicleView* pView = GetView(view))
			pView->Serialize(ser, aspects);

		int i = 0;
		char pSeatActionString[256];

		for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
		{
			_snprintf(pSeatActionString, 256, "seatAction_%d", ++i);
			pSeatActionString[sizeof(pSeatActionString) - 1] = '\0';

			ser.BeginGroup(pSeatActionString);
			ite->pSeatAction->Serialize(ser, aspects);
			ser.EndGroup();
		}
	}
	// network
	else
	{
		if (aspects & CVehicle::ASPECT_SEAT_PASSENGER)
		{
			bool remotelyUsed = m_transitionType == eVT_RemoteUsage;
			ser.Value("remotelyUsed", remotelyUsed, 'bool');
			if (ser.IsReading())
				m_transitionType = remotelyUsed ? eVT_RemoteUsage : eVT_None;

			ser.Value("passengerId", this, &CVehicleSeat::NetGetPassengerId, &CVehicleSeat::NetSetPassengerId, 'eid');
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::PostSerialize()
{
	IActor* pActor = m_passengerId ? gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_passengerId) : 0;
	bool remote = (m_transitionType == eVT_RemoteUsage);

	if (pActor)
	{
		if (m_postEnterId)
		{
			if (m_transitionType != eVT_Exiting)
			{
				if (remote)
					EnterRemotely(m_passengerId);
				else
				{
					bool needUpdateTM = false;
					if (m_isRagdollingOnDeath == false)
					{
						if (pActor->GetGameObject()->GetAspectProfile(eEA_Physics) != eAP_Alive)
						{
							pActor->GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
							needUpdateTM = true;
						}
					}
					Enter(m_passengerId, false);
					if (needUpdateTM)
						UpdatePassengerLocalTM(pActor);

					if (pActor->GetHealth() <= 0)
						OnPassengerDeath();
				}
			}

			m_postEnterId = 0;
		}

		if (!remote)
		{
			pActor->HolsterItem(true);

			if (pActor->IsClient())
				EnableVehicleActionMaps(true);

			if (m_postViewId)
			{
				SetView(m_postViewId);
				m_postViewId = 0;
			}
		}

		if (pActor->IsClient())
		{
			// don't interpolate view in this case
			IVehicleView* pView = GetView(m_currentView);
			if (pView)
			{
				pView->ResetPosition();
			}
			m_skipViewBlending = true;
		}
	}

	if (m_transitionType == eVT_Exiting)
		StandUp();

	for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
	{
		ite->pSeatAction->PostSerialize();
	}
}

//------------------------------------------------------------------------
CVehicleSeatSerializer* CVehicleSeat::GetSerializer()
{
	return m_pSerializer;
}

//------------------------------------------------------------------------
void CVehicleSeat::SetSerializer(CVehicleSeatSerializer* pSerializer)
{
	if (m_serializerId)
		return;

	m_pSerializer = pSerializer;
	m_serializerId = m_pSerializer->GetEntityId();
}

//------------------------------------------------------------------------
float CVehicleSeat::ProcessPassengerDamage(float actorHealth, float damage, const char* pDamageClass, bool explosion)
{
	// todo: use hit type ids
	bool fire = !strcmp(pDamageClass, "fire");
	bool punish = !strcmp(pDamageClass, "punish");

	if (!punish && m_isPassengerShielded && !m_pVehicle->IsDestroyed())
	{
		if (explosion || fire)
			return 0.0f;
	}

	if (fire)
		return damage * m_soundParams.inout;

	return damage;
}

//------------------------------------------------------------------------
void CVehicleSeat::OnPassengerDeath()
{
	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	assert(pActorSystem);

	IActor* pActor = pActorSystem->GetActor(m_passengerId);
	if (!pActor)
		return;

	if (m_transitionType == eVT_RemoteUsage)
	{
		assert(0 && "OnPassengerDeath called on remotely used seat");
		return;
	}

	if (m_pSeatGroup)
		m_pSeatGroup->OnPassengerExit(this, m_passengerId);

	if (m_isRagdollingOnDeath || m_transitionType == eVT_Entering || m_transitionType == eVT_Exiting ||
		VehicleCVars().v_ragdollPassengers == 1 || m_pVehicle->IsDestroyed() || pActor->IsPlayer())
	{
		UnlinkPassenger(true);

		if (pActor->IsPlayer() && m_pEnterHelper)
			pActor->GetEntity()->SetWorldTM(m_pEnterHelper->GetWorldTM());

		if (VehicleCVars().v_debugdraw)
			CryLog("unlinked %s", pActor->GetEntity()->GetName());
	}
	else
	{
		m_transitionType = eVT_Dying;
		GivesActorSeatFeatures(false);

		if (VehicleCVars().v_debugdraw)
			CryLog("%s: setting transition type eVT_Dying", pActor->GetEntity()->GetName());
	}

	// hide non-players when died from vehicle explosion
	if (!pActor->IsPlayer() && (m_pVehicle->IsDestroyed() || m_isPassengerHidden))
	{
		if (VehicleCVars().v_debugdraw)
			CryLog("hiding %s during destruction of vehicle %s", pActor->GetEntity()->GetName(), m_pVehicle->GetEntity()->GetName());

		pActor->GetEntity()->Hide(true);
	}

	SVehicleEventParams eventParams;
	eventParams.entityId = pActor->GetEntityId();
	eventParams.iParam = m_seatId;
	m_pVehicle->BroadcastVehicleEvent(eVE_SeatFreed, eventParams);

	if (m_pVehicle->IsEmpty())
		m_pVehicle->StartAbandonedTimer();
}

//------------------------------------------------------------------------
void CVehicleSeat::UnlinkPassenger(bool ragdoll)
{
	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	assert(pActorSystem);

	IActor* pActor = pActorSystem->GetActor(m_passengerId);
	if (!pActor)
		return;

	GivesActorSeatFeatures(false);

	IAnimatedCharacter* pAnimChar = (IAnimatedCharacter*)pActor->GetGameObject()->QueryExtension("AnimatedCharacter");

	if (pAnimChar)
	{
		SAnimatedCharacterParams params = pAnimChar->GetParams();
		params.flags |= eACF_EnableMovementProcessing;
		pAnimChar->SetParams(params);
	}

	IAnimationGraphState* pGraphState = GetAnimGraphState(m_passengerId);
	if (pGraphState)
	{
		IAnimationGraphState::InputID vehicleInputId = pGraphState->GetInputId("Vehicle");
		IAnimationGraphState::InputID SeatInputId = pGraphState->GetInputId("VehicleSeat");

		pGraphState->SetInput(vehicleInputId, "none", &m_agVehicleQueryId);
		pGraphState->SetInput(SeatInputId, 0, &m_agSeatQueryId);
	}

	pActor->LinkToVehicle(0);

	if (ragdoll && gEnv->bServer && !pActor->IsGod())
	{
		if (VehicleCVars().v_debugdraw)
			CryLog("%s: setting ragdoll physicalization profle", pActor->GetEntity()->GetName());

		pActor->GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);
	}

	m_passengerId = 0;
	m_transitionType = eVT_None;

	if (pActor->IsClient())
		StopSounds();

	//CHANGED_NETWORK_STATE(m_pVehicle, CVehicle::ASPECT_SEAT_PASSENGER);
	m_pVehicle->GetGameObject()->ChangedNetworkState(CVehicle::ASPECT_SEAT_PASSENGER);
}

//------------------------------------------------------------------------
void CVehicleSeat::ForceAnimGraphInputs()
{
	IAnimationGraphState* pGraphState = GetAnimGraphState(m_passengerId);

	if (!pGraphState)
		return;

	IAnimationGraphState::InputID vehicleInputId = pGraphState->GetInputId("Vehicle");
	IAnimationGraphState::InputID SeatInputId = pGraphState->GetInputId("VehicleSeat");

	pGraphState->SetInput(vehicleInputId, "none");
	pGraphState->Update();
	pGraphState->SetInput(vehicleInputId, m_agVehicleName.c_str());
	pGraphState->SetInput(SeatInputId, m_agSeatNumber);
	pGraphState->ForceTeleportToQueriedState();
}

//------------------------------------------------------------------------
bool CVehicleSeat::SetAnimGraphInputs(EntityId actorId, bool writeOrClear, bool requestQuery, bool teleport)
{
	IAnimationGraphState* pGraphState = GetAnimGraphState(actorId);

	if (!pGraphState)
		return false;

	IAnimationGraphState::InputID vehicleInputId = pGraphState->GetInputId("Vehicle");
	IAnimationGraphState::InputID SeatInputId = pGraphState->GetInputId("VehicleSeat");

	if (requestQuery)
	{
		AddStateListener(pGraphState);

		if (writeOrClear)
		{
			pGraphState->SetInput(vehicleInputId, m_agVehicleName.c_str(), &m_agVehicleQueryId);
			pGraphState->SetInput(SeatInputId, m_agSeatNumber, &m_agSeatQueryId);
		}
		else
		{
			//pGraphState->SetInput(SeatInputId, 0);
			pGraphState->SetInput(vehicleInputId, "none", &m_agVehicleQueryId);
		}
	}
	else
	{
		RemoveStateListener(pGraphState);

		if (writeOrClear)
		{
			pGraphState->SetInput(vehicleInputId, m_agVehicleName.c_str());
			pGraphState->SetInput(SeatInputId, m_agSeatNumber);
		}
		else
		{
			//pGraphState->SetInput(SeatInputId, 0);
			pGraphState->SetInput(vehicleInputId, "none");
		}
	}

	if (teleport || m_pGameFramework->GetIActorSystem()->GetActor(actorId)->IsPlayer())
	{
		pGraphState->ForceTeleportToQueriedState();
	}

	return true;
}

//------------------------------------------------------------------------
bool CVehicleSeat::RequestMovement(CMovementRequest& movementRequest)
{
	if (m_passengerId == 0)
		return false;

	IActor* pActor = GetPassengerActor();
	if (pActor && pActor->IsPlayer())
		return false;

	if (!(m_transitionType == eVT_None || m_transitionType == eVT_RemoteUsage))
		return false;

	if (movementRequest.HasLookTarget() || movementRequest.HasAimTarget())
	{
		Vec3 vGoal;

		if (movementRequest.HasLookTarget())
		{
			vGoal = movementRequest.GetLookTarget();
		}

		if (movementRequest.HasAimTarget())
		{
			vGoal = movementRequest.GetAimTarget();
		}

		for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
		{
			IVehicleSeatAction* pSeatAction = ite->pSeatAction;
			if (CVehicleSeatActionRotateTurret* pActionRotateTurret =
				CAST_VEHICLEOBJECT(CVehicleSeatActionRotateTurret, pSeatAction))
			{
				pActionRotateTurret->SetAimGoal(vGoal, 1);
			}
			if (CVehicleSeatActionWeapons* pActionWeapons =
				CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, pSeatAction))
			{
				if (movementRequest.HasFireTarget())
					pActionWeapons->SetFireTarget(movementRequest.GetFireTarget());
				else if (movementRequest.HasAimTarget())
					pActionWeapons->SetFireTarget(movementRequest.GetAimTarget());
				else
					pActionWeapons->SetFireTarget(Vec3(0, 0, 0));
			}
		}
	}

	if (m_isDriver)
	{
		if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
			pMovement->RequestMovement(movementRequest);
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeat::GetMovementState(SMovementState& movementState)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	IEntity* pVehicleEntity = m_pVehicle->GetEntity();
	if (!pVehicleEntity)
		return;

	IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_passengerId);

	const Matrix34& worldTM = pVehicleEntity->GetWorldTM();

	// todo: is this really intended?
	movementState.pos = worldTM.GetTranslation();
	movementState.upDirection = worldTM.GetColumn(2);
	// ~todo

	if (m_pAimPart)
	{
		const Matrix34& partWorldTM = m_pAimPart->GetWorldTM();
		movementState.eyePosition = partWorldTM.GetTranslation();
		movementState.eyeDirection = partWorldTM.GetColumn(1);
	}
	else
	{
		if (!pActor || !m_pSitHelper) // || !pActor->IsPlayer())
		{
			movementState.eyeDirection = worldTM.GetColumn(1);
			movementState.eyePosition = worldTM.GetTranslation();
		}
		else
		{
			const Matrix34& seatWorldTM = m_pSitHelper->GetWorldTM();
			movementState.eyeDirection = seatWorldTM.GetColumn1();
			if (pActor != NULL)
				movementState.eyePosition = seatWorldTM.GetTranslation() + seatWorldTM.TransformVector(pActor->GetLocalEyePos(0));
			else
				movementState.eyePosition = seatWorldTM.GetTranslation();
		}
	}

	// this is needed for AI LookAt to work correctly
	movementState.animationEyeDirection = movementState.eyeDirection;
	//	pActor->GetLocalEyePos(0);

	movementState.stance = STANCE_NULL;
	movementState.atMoveTarget = false;
	pVehicleEntity->GetWorldBounds(movementState.m_StanceSize);
	movementState.m_StanceSize.Move(-movementState.pos);
	movementState.m_ColliderSize = movementState.m_StanceSize;

	movementState.weaponPosition = movementState.eyePosition;
	movementState.fireDirection = movementState.eyeDirection;
	movementState.aimDirection = movementState.eyeDirection;
	movementState.bodyDirection = movementState.eyeDirection;
	movementState.movementDirection = movementState.bodyDirection;

	if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
		pMovement->GetMovementState(movementState);

	if (pActor && !pActor->IsPlayer())
	{
		if (m_pWeaponAction)
		{
			if (m_isDriver)
			{
				movementState.fireTarget = movementState.eyePosition + 10.f * movementState.eyeDirection;
			}
			else
			{
				EntityId weaponId = m_pWeaponAction->GetWeaponEntityId(m_pWeaponAction->GetCurrentWeapon());
				m_pWeaponAction->GetActualWeaponDir(weaponId, 0, movementState.aimDirection, Vec3(), Vec3());
				m_pWeaponAction->GetFiringPos(weaponId, 0, movementState.weaponPosition);
				movementState.aimDirection.Normalize();
			}
		}
	}
	else
	{
		if (pActor && pActor->IsClient())
		{
			// only overwrite when client
			movementState.weaponPosition = GetISystem()->GetViewCamera().GetPosition();
			movementState.fireDirection = GetISystem()->GetViewCamera().GetViewdir();
			movementState.aimDirection = movementState.fireDirection;
		}
	}

	movementState.isAiming = true;
}

//------------------------------------------------------------------------
bool CVehicleSeat::GetStanceState(EStance stance, float lean, bool defaultPose, SStanceState& state)
{
	IEntity* pEntity = m_pVehicle->GetEntity();
	// Returns one stance.
	if (stance != STANCE_STAND || !pEntity)
		return false;


	// TODO: This information is very approximate at the moment.
	if (false) //(query.defaultPose) //CryMP: Fixme?
	{
		AABB	aabb;
		pEntity->GetLocalBounds(aabb);

		state.pos.Set(0, 0, 0);
		state.bodyDirection = FORWARD_DIRECTION;
		state.upDirection(0, 0, 1);
		state.weaponPosition.Set(0, 0, 0);
		state.aimDirection = FORWARD_DIRECTION;
		state.fireDirection = FORWARD_DIRECTION;
		state.eyePosition.Set(0, 0, 0);
		state.eyeDirection = FORWARD_DIRECTION;
		state.m_StanceSize = aabb;
		state.m_ColliderSize = aabb;
	}
	else
	{
		Vec3	entityPos = pEntity->GetWorldPos();
		Vec3	forward = pEntity->GetWorldTM().TransformVector(FORWARD_DIRECTION);
		AABB	aabb;
		pEntity->GetWorldBounds(aabb);
		aabb.Move(-entityPos);

		state.pos = entityPos;
		state.bodyDirection = forward;
		state.upDirection(0, 0, 1);
		state.weaponPosition = entityPos;
		state.aimDirection = forward;
		state.fireDirection = forward;
		state.eyePosition = entityPos;
		state.eyeDirection = forward;
		state.m_StanceSize = aabb;
		state.m_ColliderSize = aabb;
	}

	return true;
}

//------------------------------------------------------------------------
bool CVehicleSeat::RequestFire(bool bFire)
{
	//IActor* pActor = GetPassengerActor();
	//CryComment("RequestFire <%s /%s> (passenger %s)", m_pVehicle->GetEntity()->GetName(), m_name.c_str(), pActor ? pActor->GetEntity()->GetName() : "null");

	  // we need a passenger to fire
	if (!m_passengerId)
		return false;

	for (TVehicleSeatActionVector::iterator ite = m_seatActions.begin(); ite != m_seatActions.end(); ++ite)
	{
		IVehicleSeatAction* pSeatAction = ite->pSeatAction;
		if (CVehicleSeatActionWeapons* pActionWeapons = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, pSeatAction))
		{
			if (m_aiWeaponId == 0 || m_aiWeaponId == pActionWeapons->GetWeaponEntityId(0))
			{
				if (bFire)
					pActionWeapons->StartFire();
				else
					pActionWeapons->StopFire();

				return true;
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------
TVehicleSeatId CVehicleSeat::GetNextSeatId(const TVehicleSeatId currentSeatId)
{
	return m_pVehicle->GetNextSeatId(currentSeatId);
}

//------------------------------------------------------------------------
CGameChannel* CVehicleSeat::GetGameChannel(EntityId actorId)
{
	IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(actorId);
	if (pActor)
	{
		int channelId = pActor->GetChannelId();
		if (channelId)
		{
			//CryMP: Fixme? not used now
			//CGameServerNub* pServerNub = gEnv->pGame->GetIGameFramework()->GetGameServerNub();
			//return pServerNub->GetChannel(channelId);
		}
	}

	return 0;
}

//------------------------------------------------------------------------
void CVehicleSeat::SetOutput(const char* output, const char* value)
{
	//vehicle_animation
	if (!strcmp("VehicleAnimation", output) && strcmp("", value) != 0)
	{
		if (CVehiclePartAnimated* pPartAnimated = m_pVehicle->GetVehiclePartAnimated())
		{
			if (ICharacterInstance* pCharInstance =
				pPartAnimated->GetEntity()->GetCharacter(pPartAnimated->GetSlot()))
			{
				ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
				assert(pSkeletonAnim);

				CryCharAnimationParams animParams;
				animParams.m_nLayerID = pPartAnimated->GetNextFreeLayer();
				pSkeletonAnim->StartAnimation(value, nullptr, nullptr, nullptr, animParams); //CryMP
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeat::QueryComplete(TAnimationGraphQueryID queryID, bool succeeded)
{
	IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(m_passengerId);
	if (!pActor)
		return;

	IAnimationGraphState* pGraphState = pActor->GetAnimationGraphState();
	assert(pGraphState);

	pGraphState->GetAnimationTarget();
	SAnimationTarget target;

	if (m_transitionType == eVT_Entering)
	{
		if (m_agStartEnterQueryId == queryID && succeeded)
		{
			const Matrix34& tmStart = pActor->GetEntity()->GetWorldTM();

			pActor->LinkToVehicle(m_pVehicle->GetEntityId());
			pActor->GetEntity()->SetWorldTM(tmStart);

			/*if (!m_seatNameToUseForEntering.empty())
			{
				IAnimationGraphState::InputID SeatInputId = pGraph->LookupInputId("VehicleSeat");
				pGraphState->SetInput(SeatInputId, m_agSeatNumber);
			}*/
		}
		else if (m_agStartEndQueryId == queryID && succeeded)
		{
			if (!m_seatNameToUseForEntering.empty())
			{
				IAnimationGraphState::InputID SeatInputId = pGraphState->GetInputId("VehicleSeat");
				pGraphState->SetInput(SeatInputId, m_agSeatNumber);
			}

			SitDown();
		}
		else if (m_agStartEndQueryId == queryID && !succeeded && m_transitionType == eVT_Entering)
		{
			m_transitionType = eVT_None;
			m_passengerId = 0;

			//CHANGED_NETWORK_STATE(m_pVehicle, CVehicle::ASPECT_SEAT_PASSENGER);
			m_pVehicle->GetGameObject()->ChangedNetworkState(CVehicle::ASPECT_SEAT_PASSENGER);

			if (!m_seatNameToUseForEntering.empty())
			{
				if (CVehicleSeat* pSeat = (CVehicleSeat*)m_pVehicle->GetSeatById(m_pVehicle->GetSeatId(m_seatNameToUseForEntering.c_str())))
					pSeat->m_transitionType = eVT_None;
			}

			pActor->HolsterItem(false);
		}
		if ((m_agSeatQueryId == queryID && succeeded)
			|| (m_agVehicleQueryId == queryID && !succeeded))
		{
			SitDown();
		}
	}
	else if (m_transitionType == eVT_Exiting)
	{
		if (m_agVehicleQueryId == queryID)
		{
			StandUp();
			RemoveStateListener(pGraphState);
		}
	}
}

void CVehicleSeat::DestroyedState(IAnimationGraphState* pState)
{
	// remove state 
	bool removed = stl::find_and_erase(m_stateList, pState);

	//CryLog("<%s> seat %i state %s destroyed, removing..: [%i]", m_pVehicle->GetEntity()->GetName(), GetSeatId(), pState->GetCurrentStateName(), removed);
	  // TODO: check here for any pending queries and abort them
}

//------------------------------------------------------------------------
IAnimationGraphState* CVehicleSeat::GetAnimGraphState(EntityId actorId)
{
	IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(actorId);
	if (!pActor)
		return NULL;

	return pActor->GetAnimationGraphState();

	return NULL;
}

//------------------------------------------------------------------------
void CVehicleSeat::AddStateListener(IAnimationGraphState* pState)
{
	bool inserted = stl::push_back_unique(m_stateList, pState);

	if (inserted)
		pState->AddListener("seat", this);

	//CryLog("<%s> seat %i adding listener on %s: [%i]", m_pVehicle->GetEntity()->GetName(), GetSeatId(), pState->GetCurrentStateName(), inserted);
}

//------------------------------------------------------------------------
void CVehicleSeat::RemoveStateListener(IAnimationGraphState* pState)
{
	pState->RemoveListener(this);
	bool removed = stl::find_and_erase(m_stateList, pState);

	//CryLog("<%s> seat %i removing listener from %s: [%i]", m_pVehicle->GetEntity()->GetName(), GetSeatId(), pState->GetCurrentStateName(), removed);
}

//------------------------------------------------------------------------
void CVehicleSeat::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	for (TVehicleSeatActionVector::iterator it = m_seatActions.begin(); it != m_seatActions.end(); ++it)
	{
		it->pSeatAction->OnVehicleEvent(event, params);
	}

	switch (event)
	{
	case eVE_Destroyed:
	{
		if (m_passengerId)
		{
			// kill non-dead passengers
			IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(m_passengerId);
			if (!pActor)
				return;

			if (gEnv->bServer && (pActor->GetHealth() > 0))
			{
				IGameRules* pGameRules = m_pGameFramework->GetIGameRulesSystem()->GetCurrentGameRules();
				if (pGameRules)
				{
					HitInfo hit;

					EntityId shooterId = params.entityId;
					if (shooterId == m_pVehicle->GetEntityId())
					{
						if (m_pVehicle->GetDriver())
							shooterId = m_pVehicle->GetDriver()->GetEntityId();
					}

					hit.targetId = m_passengerId;
					hit.shooterId = shooterId;
					hit.weaponId = m_pVehicle->GetEntityId();
					hit.damage = 1000.f;
					hit.type = pGameRules->GetHitTypeId("normal");
					hit.pos = pActor->GetEntity()->GetWorldPos();

					pGameRules->ServerHit(hit);
				}
			}
			else
			{
				if (!pActor->IsPlayer())
				{
					Exit(false, true);
					if (m_pVehicle->GetMovement() && m_pVehicle->GetMovement()->GetMovementType() == IVehicleMovement::eVMT_Air)
					{
						pActor->HolsterItem(false);
						pActor->GetEntity()->Hide(true);
						IInventory* pInventory = pActor->GetInventory();
						if (pInventory != NULL)
						{
							IItem* currentItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pInventory->GetCurrentItem());
							IMaterial* currentReplacementMat = pActor->GetReplacementMaterial();
							if (currentItem && currentReplacementMat)
							{
								currentItem->Cloak(false);
								currentItem->Cloak(true, currentReplacementMat);
							}
						}
					}
				}

			}
		}
	}
	break;
	case eVE_ToggleDebugView:
	{
		IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(m_passengerId);
		if (pActor && pActor->IsClient())
		{
			if (params.bParam)
			{
				if (m_views.size() > 0 && m_views.back()->IsDebugView())
					SetView(m_views.size());
			}
		}
	}
	break;
	case eVE_ToggleDriverControlledGuns:
	{
		if (!params.bParam)
		{
			if (m_passengerId && m_transitionType == eVT_RemoteUsage && IsGunner())
			{
				ExitRemotely();

				if (GetCurrentView() > 1)
					SetView(1);
			}
		}
		else
		{
			if (m_passengerId && IsDriver())
			{
				if (CVehicleSeat* pSeat = GetSeatUsedRemotely(false))
				{
					if (!pSeat->GetPassenger(true))
					{
						pSeat->EnterRemotely(m_passengerId);

						if (GetCurrentView() > 1)
							SetView(1);
					}
				}
			}
		}
	}
	break;
	case eVE_SeatFreed:
		OnSeatFreed(params.iParam, params.entityId);
		break;
	default:
		break;
	}
}

//------------------------------------------------------------------------
bool CVehicleSeat::IsGunner()
{
	if (m_pWeaponAction)
		return true;

	if (CVehicleSeat* pSeat = GetSeatUsedRemotely(true))
		return pSeat->IsGunner();

	return false;
}

//------------------------------------------------------------------------
bool CVehicleSeat::IsRotatable()
{
	if (m_pTurretAction)
		return true;

	return false;
}

//------------------------------------------------------------------------
bool CVehicleSeat::IsPassengerHidden()
{
	return m_isPassengerHidden;
}

//------------------------------------------------------------------------
void CVehicleSeat::UpdateHiddenState(bool hide)
{
	if (!m_isPassengerHidden)
		return;

	IEntity* pEnt = gEnv->pEntitySystem->GetEntity(m_passengerId);
	if (!pEnt)
		return;

	pEnt->Invisible(hide);
}

//------------------------------------------------------------------------
void CVehicleSeat::OnCameraShake(float& angleAmount, float& shiftAmount, const Vec3& pos, const char* source) const
{
	if (!strcmp(source, "explosion"))
	{
		angleAmount *= m_explosionShakeMult;
		shiftAmount *= m_explosionShakeMult;
	}
}

//------------------------------------------------------------------------
Matrix34 CVehicleSeat::GetExitTM(IActor* pActor, bool opposite/*=false*/)
{
	Matrix34 worldTM;
	IEntity* pEntity = m_pVehicle->GetEntity();
	const Matrix34& vehicleWorldTM = pEntity->GetWorldTM();

	AABB bounds;
	pEntity->GetLocalBounds(bounds);

	if (m_pExitHelper)
	{
		if (!opposite)
			worldTM = m_pExitHelper->GetWorldTM();
		else
			worldTM = m_pExitHelper->GetReflectedWorldTM();
	}
	else if (m_pEnterHelper)
	{
		if (!opposite)
			worldTM = m_pEnterHelper->GetWorldTM();
		else
			worldTM = m_pEnterHelper->GetReflectedWorldTM();
	}
	else
	{
		worldTM.SetIdentity();
		Vec3 pos = vehicleWorldTM * Vec3(bounds.min.x, bounds.min.y, 0.f);
		AABB worldBounds = AABB::CreateTransformedAABB(vehicleWorldTM, bounds);
		pos.z = min(pos.z + bounds.max.z, worldBounds.max.z);
		worldTM.SetTranslation(pos);
	}

	if (pActor && pActor->IsPlayer())
	{
		Matrix34 worldTMInv = vehicleWorldTM.GetInverted();
		Vec3 localPos = worldTMInv.TransformPoint(worldTM.GetTranslation());

		// for players, we add a sideways safety gap 
		// as helpers are usually close enough to generate collisions when exiting while driving fast & not straight
		// default 0.5m
		Vec3 offset(m_exitOffsetPlayer);
		if (offset.IsZero())
		{
			offset.x = sgn(localPos.x) * 0.5f;
		}

		// additional offset to account for angular velocity   
		// possibly replace by physical bounds check (and integrate with general intersection checks) after alpha 
		if (m_pVehicle->GetStatus().speed > 0.1f)
		{
			pe_status_dynamics dyn;
			if (IPhysicalEntity* pEntityPhysics = pEntity->GetPhysics())
			{
				if (pEntityPhysics->GetStatus(&dyn))
				{
					if (dyn.w.GetLengthSquared() > sqr(0.2f))
					{
						Vec3 localW = worldTMInv.TransformVector(dyn.w);
						if (localW.z * localPos.x > 0.f || localW.y > 0.25f)
							offset.x += sgn(offset.x) * min(4.f * max(abs(localW.z), abs(localW.y)), bounds.max.x);
					}
				}
			}
		}

		worldTM.SetTranslation(worldTM.GetTranslation() + vehicleWorldTM.TransformVector(offset));

		/*pe_status_pos pos;
		pEntity->GetPhysics()->GetStatus(&pos);

		// estimate local bounds after vehicle moved about half its length along its current velocity
		Vec3 dp = dyn.v.GetNormalized() * 0.5f*bounds.GetSize().y;
		float dt = 0.5f*bounds.GetSize().y / m_pVehicle->GetStatus().speed;
		Vec3 posNew = vehicleWorldTM.GetTranslation() + dp;
		Quat rotNew = vehicleWorldRotation;
		if (dyn.w.GetLengthSquared()>0.01f)
		{
		  rotNew *= Quat(dyn.w*dt);
		  rotNew.Normalize();
		}

		OBB obbNew = OBB::CreateOBBfromAABB(rotNew, bounds);

		Matrix34 newWorldTM(IDENTITY);
		newWorldTM.SetTranslation(posNew);

		IRenderAuxGeom* pGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		pGeom->SetRenderFlags(e_Def3DPublicRenderflags);
		pGeom->DrawOBB(obbNew, vehicleWorldTM.GetTranslation(), false, ColorB(255,0,0,200), eBBD_Faceted);
		*/

	}

	return worldTM;
}

//------------------------------------------------------------------------
bool CVehicleSeat::TestExitPosition(IActor* pActor, Vec3& testPos, EntityId* pBlockingEntity)
{
	if (!pActor)
		return false;

	Vec3 startPos = pActor->GetEntity()->GetWorldPos();
	IVehicleHelper* pSeatHelper = GetSitHelper();
	if (pSeatHelper)
		startPos = pSeatHelper->GetWorldTM().GetTranslation();

	Vec3 dir = testPos - startPos;
	dir.z += 0.3f;

	// first test vehicle velocity. if the vehicle is moving in the general direction of this helper
	//	(most likely when sliding to a halt) then disallow this exit pos to prevent running yourself over
	IEntity* pEntity = m_pVehicle->GetEntity();
	const Matrix34& vehicleWorldTM = pEntity->GetWorldTM();
	Matrix34 worldTMInv = vehicleWorldTM.GetInverted();
	if (m_pVehicle->GetStatus().speed > 1.0f)
	{
		pe_status_dynamics dyn;
		if (IPhysicalEntity* pEntityPhysics = pEntity->GetPhysics())
		{
			if (pEntityPhysics->GetStatus(&dyn))
			{
				Vec3 localV = worldTMInv.TransformVector(dyn.v);
				Vec3 localDir = worldTMInv.TransformVector(dir);

				if (abs(localV.x) > 2.0f && sgn(localV.x) == sgn(localDir.x))
				{
					//CryLog("Disallowing vehicle exit due to sliding");
					return false;
				}
			}
		}
	}

	static IPhysicalEntity* pSkipEnts[11];
	int nskip = m_pVehicle->GetSkipEntities(pSkipEnts, 10);
	// add the actor specifically
	pSkipEnts[nskip++] = pActor->GetEntity()->GetPhysics();
	return m_pVehicle->ExitSphereTest(pSkipEnts, nskip, startPos, testPos, pBlockingEntity);
}

//------------------------------------------------------------------------
IVehicleClient* CVehicleSeat::GetVehicleClient()
{
	return m_pVehicleSystem->GetVehicleClient();
}

//------------------------------------------------------------------------
void CVehicleSeat::GetMemoryStatistics(ICrySizer* s)
{
	s->Add(*this);
	{
		SIZER_COMPONENT_NAME(s, "Views");
		s->AddContainer(m_views);
		for (TVehicleViewVector::iterator iter = m_views.begin(); iter != m_views.end(); ++iter)
			(*iter)->GetMemoryStatistics(s);
		s->AddContainer(m_viewNames);
		for (TStringVector::iterator iter = m_viewNames.begin(); iter != m_viewNames.end(); ++iter)
			s->Add(*iter);
	}
	{
		SIZER_COMPONENT_NAME(s, "Actions");
		s->AddContainer(m_seatActions);
		for (TVehicleSeatActionVector::iterator iter = m_seatActions.begin(); iter != m_seatActions.end(); ++iter)
		{
			if (iter->pSeatAction)
				iter->pSeatAction->GetMemoryStatistics(s);
		}
	}
	s->AddContainer(m_stateList);
	s->Add(m_seatNameToUseForEntering);
	s->Add(m_useActionsRemotelyOfSeat);
	s->Add(m_agVehicleName);
}

//#include UNIQUE_VIRTUAL_WRAPPER(IVehicleSeat)
