/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a seat action for the steering wheel

-------------------------------------------------------------------------
History:
- 28:11:2005: Created by Mathieu Pinard

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
//#include "CryAction.h"
#include "CryCommon/CryAction/IActorSystem.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehiclePartAnimatedJoint.h"
#include "VehiclePartSubPartWheel.h"
#include "VehicleSeatActionSteeringWheel.h"


CVehicleSeatActionSteeringWheel::CVehicleSeatActionSteeringWheel()
: m_pVehicleAnim(NULL),
  m_isBeingUsed(false)
{
	m_steeringActions	=	Vec3();
	m_steeringValues	=	Vec3();
}

//------------------------------------------------------------------------
bool CVehicleSeatActionSteeringWheel::Init(IVehicle* pVehicle, TVehicleSeatId seatId, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;
	m_pFirstWheel = NULL;

	m_userId = 0;

	CVehicleParams steeringWheelTable = table.findChild("SteeringWheel");
	if (!steeringWheelTable)
		return false;

	if (CVehicleParams subTable = steeringWheelTable.findChild("Actions"))
	{
		m_steeringClass = eSC_Generic;

		subTable.getAttr("anglesMax", m_steeringMaxAngles);
		m_steeringMaxAngles = DEG2RAD(m_steeringMaxAngles);

		m_steeringRelaxMult = 1.0f;
		subTable.getAttr("steeringRelaxMult", m_steeringRelaxMult);

		m_steeringForce = 1.0f;
		subTable.getAttr("steeringForce", m_steeringForce);
	}
	else if (CVehicleParams subTableCar = steeringWheelTable.findChild("Car"))
	{
		m_steeringClass = eSC_Car;

		if (!subTableCar.getAttr("wheelRotationMax", m_wheelRotationMax) || iszero(m_wheelRotationMax))
			m_wheelRotationMax = 0.1f;

		TVehiclePartVector& vehicleParts = static_cast<CVehicle*>(m_pVehicle)->GetParts();
		for (TVehiclePartVector::const_iterator ite = vehicleParts.begin(),end = vehicleParts.end(); ite!=end; ++ite)
		{
			IVehiclePart* pPart = ite->second;

			if (m_pFirstWheel = CAST_VEHICLEOBJECT(CVehiclePartSubPartWheel, pPart))
				break;
		}

		if (m_pFirstWheel)
		{
			const Matrix34& initialTM = m_pFirstWheel->GetLocalTM(false);
			m_initialRotation = Ang3::GetAnglesXYZ(Matrix33(initialTM)).z;
		}
		else
			return false;
	}

	m_pVehicleAnim = m_pVehicle->GetAnimation(steeringWheelTable.getAttr("vehicleAnimation"));
	return true;
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::Reset()
{
	if (m_userId)
		StopUsing();
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::StartUsing(EntityId passengerId)
{
	m_userId = passengerId;
	m_isBeingUsed = false;

	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();

	if (IActor* pActor = pActorSystem->GetActor(m_userId))
	{
		pActor->GetAnimationGraphState()->AddListener("wheel", this);
	}

	if (m_steeringClass == eSC_Generic)
	{
		m_steeringValues.zero();
		m_steeringActions.zero();
	}

	if (m_pVehicleAnim)
	{
		m_pVehicleAnim->StartAnimation();
		m_pVehicleAnim->ToggleManualUpdate(true);
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::StopUsing()
{
	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();

	if (IActor* pActor = pActorSystem->GetActor(m_userId))
	{
		pActor->GetAnimationGraphState()->RemoveListener(this);
	}

	if (m_isBeingUsed)
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);

	if (m_pVehicleAnim)
	{
		m_pVehicleAnim->StopAnimation();
	}

	m_userId = 0;
  m_isBeingUsed = false;
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	if (m_steeringClass == eSC_Generic)
	{
		if (actionId == eVAI_TurnLeft)
		{
			/*if (activationMode == eAAM_OnPress || activationMode == eAAM_OnHold)
				m_steeringActions.y -= 1.0f;
			else if (activationMode == eAAM_OnRelease)
				//m_steeringActions.y = max(m_steeringActions.y, 0.0f);
				m_steeringActions.y += 1.0f;
      */
      m_steeringActions.y = -value;
		}
		else if (actionId == eVAI_TurnRight)
		{
			/*if (activationMode == eAAM_OnPress || activationMode == eAAM_OnHold)
				m_steeringActions.y += 1.0f;
			else if (activationMode == eAAM_OnRelease)
				m_steeringActions.y -= 1.0f;
				//m_steeringActions.y = min(m_steeringActions.y, 0.0f);
      */
      m_steeringActions.y = value;
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::Update(float frameTime)
{
	if (!m_userId || !m_isBeingUsed)
		return;

	if (m_steeringClass == eSC_Car)
	{
		Matrix33 wheelMat = Matrix33(m_pFirstWheel->GetLocalTM(false));
		wheelMat.OrthonormalizeFast();

		Ang3 angles = Ang3::GetAnglesXYZ(wheelMat);

		float value = angles.z + DEG2RAD(180.0f);
		value /= DEG2RAD(360.0f);

		value -= 0.5f - m_wheelRotationMax;
		value /= m_wheelRotationMax * 2.0f;

		value = max(0.0f, min(1.0f, value));
		value = 1.0f - value;

		if (ISkeletonAnim* pSkeletonAnim = GetActorSkeleton(m_userId))
			SetAnimManualUpdate(pSkeletonAnim, value);

		if (m_pVehicleAnim)    
      m_pVehicleAnim->SetTime(value, true);
    
    //float color[] = {1,1,1,1};
    //gEnv->pRenderer->Draw2dLabel(100,230,1.5,color,false,"action: %.2f", m_steeringActions.y);
    //gEnv->pRenderer->Draw2dLabel(100,250,1.5,color,false,"value: %.2f", value);
	}
	else if (m_steeringClass == eSC_Generic)
	{
		m_steeringValues -= m_steeringValues * m_steeringRelaxMult * frameTime;
		m_steeringValues += m_steeringActions * m_steeringForce * frameTime;

		m_steeringValues.y = min(1.0f, max(-1.0f, m_steeringValues.y));

		float value = m_steeringValues.y + 1.0f;
		value /= 2.0f;

		if (ISkeletonAnim* pSkeletonAnim = GetActorSkeleton(m_userId))
			SetAnimManualUpdate(pSkeletonAnim, value);

		if (m_pVehicleAnim)
			m_pVehicleAnim->SetTime(value, true);

    //float color[] = {1,1,1,1};
    //gEnv->pRenderer->Draw2dLabel(100,230,1.5,color,false,"action: %.2f", m_steeringActions.y);
    //gEnv->pRenderer->Draw2dLabel(100,250,1.5,color,false,"value: %.2f", value);
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::Serialize(TSerialize ser, unsigned aspects)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.Value("steeringValues", m_steeringValues);
		ser.Value("steeringActions", m_steeringActions);
    
    m_isUsedSerialization = m_isBeingUsed;
    ser.Value("isUsed", m_isUsedSerialization);
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::PostSerialize()
{
	if (m_isUsedSerialization != m_isBeingUsed)
	{
		m_pVehicle->SetObjectUpdate(this, m_isUsedSerialization ? IVehicle::eVOU_PassengerUpdate : IVehicle::eVOU_NoUpdate);
		m_isBeingUsed = m_isUsedSerialization;
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::SetOutput(const char * output, const char * value)
{
	if (!m_userId)
		return;

	if (!strcmp("VehicleSeatAction", output) && !strcmp("SteeringWheel", value))
	{
		m_isBeingUsed = true;
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_PassengerUpdate);
	}
}

//------------------------------------------------------------------------
ISkeletonAnim* CVehicleSeatActionSteeringWheel::GetActorSkeleton(EntityId actorId)
{
	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	assert(pActorSystem);

	if (IActor* pActor = pActorSystem->GetActor(m_userId))
	{
		if (ICharacterInstance* pCharInstance = pActor->GetEntity()->GetCharacter(0))
			return pCharInstance->GetISkeletonAnim();
	}

	return NULL;
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::SetAnimManualUpdate(ISkeletonAnim* pSkeletonAnim, float value)
{
	assert(pSkeletonAnim);

	if (!pSkeletonAnim)
		return;

	if (uint32 numAnimsLayer = pSkeletonAnim->GetNumAnimsInFIFO(0))
	{
		CAnimation& animation= pSkeletonAnim->GetAnimFromFIFO(0, 0);
		if (animation.m_AnimParams.m_nFlags & CA_MANUAL_UPDATE)
			animation.m_fAnimTime = value;

		assert(animation.m_fAnimTime>=0.0f && animation.m_fAnimTime<=1.0f);
	}
}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionSteeringWheel);
