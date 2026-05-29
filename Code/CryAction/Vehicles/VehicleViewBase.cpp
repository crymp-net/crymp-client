/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a base class for the vehicle views

-------------------------------------------------------------------------
History:
- 06:07:2006: Created by Mathieu Pinard

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CryAction/IActorSystem.h"
#include "CryCommon/CryAnimation/ICryAnimation.h"
#include "CryCommon/CryAction/IViewSystem.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "VehicleViewBase.h"
#include "VehicleSeat.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"

#define SEND_ACTION(aamVar, actionId, value) \
	{ \
	pMovement->OnAction(actionId, eAAM_OnPress, value); \
	aamVar = eAAM_OnPress; \
	}

#define RELEASE_ACTION(aamVar, actionId) \
	{ \
	if (aamVar == eAAM_OnPress) \
		{ \
		pMovement->OnAction(actionId, eAAM_OnRelease, 0.0f); \
		aamVar = eAAM_OnRelease; \
		} \
	}

ICVar* CVehicleViewBase::m_pSensitivity = NULL;


//------------------------------------------------------------------------
CVehicleViewBase::CVehicleViewBase()
{
  m_isDebugView = false;    
  m_pVehicle = NULL;
  m_pSeat = NULL;
  m_isAvailableRemotely = false;

  if (!m_pSensitivity)
    m_pSensitivity = gEnv->pConsole->GetCVar("cl_sensitivity"); // fixme. game cvar
}

//------------------------------------------------------------------------
bool CVehicleViewBase::Init(CVehicleSeat* pSeat, const CVehicleParams& table)
{
	m_pSeat = pSeat;
	m_pVehicle = pSeat->GetVehicle();
  m_rotationInit.zero();
	 
	if (table.getAttr("rotationBoundsActionMult", m_rotationBoundsActionMult) && !iszero(m_rotationBoundsActionMult))
		m_isSendingActionOnRotation = true;
	else
	{
		m_isSendingActionOnRotation = false;
		m_rotationBoundsActionMult = 0.0f;
	}

	if (table.getAttr("rotationMax", m_rotationMax))
	{
		m_rotationMax = DEG2RAD(m_rotationMax);

		if (table.getAttr("rotationMin", m_rotationMin))
			m_rotationMin = DEG2RAD(m_rotationMin);
		else
			m_rotationMin = -m_rotationMax;
	}
	else
	{
		m_rotationMax.zero();
		m_rotationMin.zero();
	}

	if (!table.getAttr("delayTimeMax", m_relaxDelayMax))
		m_relaxDelayMax = 0.3f;

	if (!table.getAttr("relaxTimeMax", m_relaxTimeMax))
		m_relaxTimeMax = 0.4f;

    if (table.getAttr("rotationInit", m_rotationInit))
      m_rotationInit = DEG2RAD(m_rotationInit);

	if (!table.getAttr("canRotate", m_isRotating))
		m_isRotating = false;

	if (table.getAttr("rotationVelMax", m_velLenMax))
	{
		if (!table.getAttr("rotationVelMin", m_velLenMin))
			m_velLenMin = m_velLenMax;
	}
	else
	{
		m_velLenMax = 1.0f;
		m_velLenMin = 1.0f;
	}

	if (m_velLenMax == m_velLenMin)
		m_velLenMax += 1.0f;

	m_rotation=Ang3();

	if (!IsThirdPerson())
	{
		if (m_rotationMax.IsZero())
		{
			m_rotationMax.x = DEG2RAD(85.0f);
			m_rotationMax.y = DEG2RAD(0.0f);
			m_rotationMax.z = DEG2RAD(85.0f);
		}
	}

	if (m_rotationMin.IsZero() && !m_rotationMax.IsZero())
		m_rotationMin = -m_rotationMax;

  m_viewAngleOffset.Set(0.f, 0.f, 0.f);
  
  if (!table.getAttr("hidePlayer", m_hidePlayer))
    m_hidePlayer = false;

  table.getAttr("isAvailableRemotely", m_isAvailableRemotely);
	m_isRelaxEnabled = false;

	if (CVehicleParams hideTable = table.findChild("HideParts"))
	{
		int i = 0;
		int c = hideTable.getChildCount();

		for (; i < c; i++)
		{
			CVehicleParams partRef = hideTable.getChild(i);
			m_hideParts.push_back(partRef.getAttr("value"));
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CVehicleViewBase::Init(CVehicleSeat* pSeat)
{
	m_pSeat = pSeat;
	m_pVehicle = pSeat->GetVehicle();

	m_isSendingActionOnRotation = false;
	m_rotationBoundsActionMult = 0.0f;

	m_isRotating = true;
	m_isRelaxEnabled = false;
	m_relaxDelayMax = 0.0f;
	m_relaxTimeMax = 0.0f;

	m_rotationMax.zero();
	m_rotationMin.zero();

	m_velLenMax = 2.0f;
	m_velLenMin = 1.0f;

	m_rotation=Ang3();
  m_rotationInit.zero();

	m_viewAngleOffset.Set(0.f, 0.f, 0.f);
	m_hidePlayer = false;

	return true;
}

//------------------------------------------------------------------------
void CVehicleViewBase::Reset()
{
	m_rotatingAction.zero();
	m_rotation = Ang3(m_rotationInit);
  m_viewAngleOffset.Set(0.f, 0.f, 0.f);

	m_relaxTime = 0.0f;
	m_relaxDelay = 0.0f;
}

//------------------------------------------------------------------------
void CVehicleViewBase::OnStartUsing(EntityId playerId)
{
  //if (VehicleCVars().v_debugdraw == eVDB_View)
    //CryLog("StartUsing: %s %s", m_pSeat->GetName().c_str(), IsThirdPerson()?"(tp)":"");

  Reset();
	
  m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);


  IActor* pActor = m_pSeat->GetPassengerActor(true);
  if (pActor)
  {
    m_playerViewThirdOnExit = pActor->IsThirdPerson();
    if (pActor->IsThirdPerson() != this->IsThirdPerson())
      pActor->ToggleThirdPerson();
  }  


	// hide vehicle parts we don't want to see. NB there is a hide-count in vehicle part base, so we can safely
	// hide and unhide from here without upsetting anything
	for(int i=0; i<m_hideParts.size(); ++i)
	{
		if(CVehiclePartBase* pPart = static_cast<CVehiclePartBase*>(m_pVehicle->GetPart(m_hideParts[i].c_str())))
		{
			pPart->Hide(true);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleViewBase::OnStopUsing()
{
  //if (VehicleCVars().v_debugdraw == eVDB_View)
    //CryLog("StopUsing: %s %s", m_pSeat->GetName().c_str(), IsThirdPerson()?"(tp)":"");
	
  m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);

	IVehicleMovement* pMovement = m_pVehicle->GetMovement();

	#define CLEAR_ACTION(aamvar, actionid) \
		if (aamvar == eAAM_OnPress) \
		{ \
			aamvar = eAAM_OnRelease;\
			if (pMovement) pMovement->OnAction(actionid, aamvar, 0.0f); \
		}

	CLEAR_ACTION(m_yawLeftActionOnBorderAAM, eVAI_TurnLeft);
	CLEAR_ACTION(m_yawRightActionOnBorderAAM, eVAI_TurnRight);
	CLEAR_ACTION(m_pitchUpActionOnBorderAAM, eVAI_PitchUp);
	CLEAR_ACTION(m_pitchDownActionOnBorderAAM, eVAI_PitchDown);

	m_pitchVal = 0.0f;
	m_yawVal = 0.0f;

	// unhide vehicle parts we hid while using
	for(int i=0; i<m_hideParts.size(); ++i)
	{
		if(CVehiclePartBase* pPart = static_cast<CVehiclePartBase*>(m_pVehicle->GetPart(m_hideParts[i].c_str())))
		{
			pPart->Hide(false);
		}
	}

	// Reset the player's view mode to one used, when he got in the vehicle
	IActor* pActor = m_pSeat->GetPassengerActor(true);
	if (pActor)
	{
		if (m_playerViewThirdOnExit != pActor->IsThirdPerson())
		{
			pActor->ToggleThirdPerson();
		}
	}
}

//------------------------------------------------------------------------
void CVehicleViewBase::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	if (actionId == eVAI_RotateYaw)
	{
		m_rotatingAction.z += value;
	}
	else if (actionId == eVAI_RotatePitch)
	{		
		m_rotatingAction.x += value;
	}
	else if (actionId == eVAI_ViewOption)
	{
		m_isRotating = !m_isRotating;
		m_isSendingActionOnRotation = !m_isSendingActionOnRotation;

		if (!m_isSendingActionOnRotation)
		{
			m_yawVal = 0.0f;
			m_pitchVal = 0.0f;

			if (m_pSeat->IsDriver())
			{
				if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
				{
					RELEASE_ACTION(m_pitchDownActionOnBorderAAM, eVAI_PitchDown);
					RELEASE_ACTION(m_pitchUpActionOnBorderAAM, eVAI_PitchUp);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleViewBase::Update(const float frameTime)
{
	const float recenterSpeed = 4.25f;

  IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_pSeat->GetPassenger());
/*
  if (false && pActor)  
    m_viewAngleOffset = Ang3(pActor->GetViewAngleOffset());
*/
	bool isRelaxWanted = (!iszero(m_rotationBoundsActionMult));
	float rotMaxVelMult = 1.0f;

	if (!iszero(m_velLenMax) && !iszero(m_velLenMin))
	{
		if (IPhysicalEntity* pPhysEntity = m_pVehicle->GetEntity()->GetPhysics())
		{
			pe_status_dynamics dyn;
			pPhysEntity->GetStatus(&dyn);

			float velLen = dyn.v.GetLength2D();

			if (velLen > m_velLenMin)
			{
				rotMaxVelMult = (min(m_velLenMax, velLen) + (m_velLenMin - min(m_velLenMax, velLen))) 
					/ (m_velLenMax - m_velLenMin);

				rotMaxVelMult = min(1.0f, rotMaxVelMult);

				isRelaxWanted = true;
			}
		}
	}

	Vec3 rotationMin;
	Vec3 rotationMax;

	if (m_isRotating)
	{
		rotationMin = m_rotationMin * rotMaxVelMult;
		rotationMax = m_rotationMax * rotMaxVelMult;
	}
	else
	{
		rotationMin.zero();
		rotationMax.zero();

		if (!Vec3(m_rotation).IsZero())
		{
			m_rotation.x -= m_rotation.x * recenterSpeed * frameTime;
			m_rotation.y -= m_rotation.y * recenterSpeed * frameTime;
			m_rotation.z -= m_rotation.z * recenterSpeed * frameTime;
		}
	}

	if (m_isRotating && !m_rotatingAction.IsZero())
	{
		// todo: would like to use players view code here as far as possible
//		const static float coeff = 0.05f;		
    float sensitivity = (m_pSensitivity) ? m_pSensitivity->GetFVal() : 1.0f;
		
		// get rotation into a manageable form
		float mouseSensitivity = 0.005f * max(1.0f, sensitivity);
		mouseSensitivity *= gf_PI / 180.0f;

		m_rotation.x -= m_rotatingAction.x * mouseSensitivity;
		m_rotation.z -= m_rotatingAction.z * mouseSensitivity;

		if (!rotationMax.IsZero() && !rotationMin.IsZero())
		{
			m_rotation.x = min(max(m_rotation.x, rotationMin.x), rotationMax.x);
			m_rotation.z = min(max(m_rotation.z, rotationMin.z), rotationMax.z);
		}

		m_relaxTime = 0.0f;
		m_relaxDelay = 0.0f;
	}
	else
	{
		if (m_isRelaxEnabled && !Vec3(m_rotation).IsZero() && isRelaxWanted)
		{
			m_relaxDelay += frameTime;

			if (m_relaxDelay > m_relaxDelayMax)
			{
				m_relaxTime += frameTime;

				Ang3 m_subStraction = m_rotation;
				float mult = min(2.0f, (m_relaxTime / m_relaxTimeMax));
				m_subStraction *= mult;
				m_subStraction *= frameTime;
				m_rotation -= m_subStraction;
			}
		}
	}

	IVehicleMovement* pMovement = NULL;
	if (m_pSeat->IsDriver())
		pMovement = m_pVehicle->GetMovement();
	
	if (pMovement && m_isSendingActionOnRotation)
	{
		if (m_rotation.z >= rotationMax.z && m_rotatingAction.z > 0.0f)
		{
			if (m_yawRightActionOnBorderAAM != eAAM_OnPress)
				m_yawVal = 0.0f;

			m_yawVal += m_rotationBoundsActionMult;
			if (m_rotation.z != 0.0f)
				m_yawVal *= m_rotation.z / (m_rotation.x + m_rotation.z);

			m_yawVal = min(m_yawVal, 1.0f);

			RELEASE_ACTION(m_yawLeftActionOnBorderAAM, eVAI_TurnLeft);
			SEND_ACTION(m_yawRightActionOnBorderAAM, eVAI_TurnRight, m_yawVal);
		}
		else if (m_rotation.z <= rotationMin.z && m_rotatingAction.z < 0.0f)
		{
			if (m_yawLeftActionOnBorderAAM != eAAM_OnPress)
				m_yawVal = 0.0f;

			m_yawVal += m_rotationBoundsActionMult;
			if (m_rotation.z != 0.0f)
				m_yawVal *= m_rotation.z / (m_rotation.x + m_rotation.z);

			m_yawVal = min(m_yawVal, 1.0f);

			RELEASE_ACTION(m_yawRightActionOnBorderAAM, eVAI_TurnRight);
			SEND_ACTION(m_yawLeftActionOnBorderAAM, eVAI_TurnLeft, m_yawVal);
		}
		else
		{
			m_yawVal = 0.0f;

			RELEASE_ACTION(m_yawRightActionOnBorderAAM, eVAI_TurnRight);
			RELEASE_ACTION(m_yawLeftActionOnBorderAAM, eVAI_TurnLeft);
		}

		if (m_rotation.x >= rotationMax.x && m_rotatingAction.x > 0.0f)
		{
			if (m_pitchDownActionOnBorderAAM != eAAM_OnPress)
				m_pitchVal = 0.0f;

			m_pitchVal += m_rotationBoundsActionMult;
			if (m_rotation.x != 0.0f)
				m_pitchVal *= m_rotation.x / (m_rotation.x + m_rotation.z);

			m_pitchVal = min(m_pitchVal, 1.0f);

			RELEASE_ACTION(m_pitchUpActionOnBorderAAM, eVAI_PitchUp);
			SEND_ACTION(m_pitchDownActionOnBorderAAM, eVAI_PitchDown, m_pitchVal);
		}
		else if (m_rotation.x <= rotationMin.x && m_rotatingAction.x < 0.0f)
		{
			if (m_pitchUpActionOnBorderAAM != eAAM_OnPress)
				m_pitchVal = 0.0f;

			m_pitchVal += m_rotationBoundsActionMult;
			if (m_rotation.x != 0.0f)
				m_pitchVal *= m_rotation.x / (m_rotation.x + m_rotation.z);

			m_pitchVal = min(m_pitchVal, 1.0f);

			RELEASE_ACTION(m_pitchDownActionOnBorderAAM, eVAI_PitchDown);
			SEND_ACTION(m_pitchUpActionOnBorderAAM, eVAI_PitchUp, m_pitchVal);
		}
		else
		{
			m_yawVal = 0.0f;
			m_pitchVal = 0.0f;

			RELEASE_ACTION(m_pitchDownActionOnBorderAAM, eVAI_PitchDown);
			RELEASE_ACTION(m_pitchUpActionOnBorderAAM, eVAI_PitchUp);
		}
	}

	m_rotatingAction.zero();
}

//------------------------------------------------------------------------
void CVehicleViewBase::Serialize(TSerialize serialize, unsigned int aspects)
{
	if (serialize.GetSerializationTarget() != eST_Network)
	{
		serialize.Value("rotation", m_rotation);
	}
}
