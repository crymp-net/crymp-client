/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a seat action to rotate a turret

-------------------------------------------------------------------------
History:
- 14:12:2005: Created by Mathieu Pinard

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"

#include "CryCommon/CryAction/IGameObject.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryAction/IActorSystem.h"
#include "CryCommon/CryGame/GameUtils.h"
#include "Vehicle.h"
#include "VehicleSeat.h"
#include "VehiclePartBase.h"
#include "VehiclePartAnimated.h"
#include "VehicleSeatActionRotateTurret.h"
#include "VehicleSeatActionWeapons.h"

#include "CryCommon/CryRenderer/IRenderAuxGeom.h"

#define TURNSPEED_COEFF 0.002f

//------------------------------------------------------------------------
bool CVehicleSeatActionRotateTurret::Init(IVehicle* pVehicle, TVehicleSeatId seatId, const CVehicleParams& table)
{
  m_pUserEntity = NULL;
  m_aimGoal.zero();
  m_aimGoalPriority = 0;

	m_rotTestHelpers[0] = NULL;
	m_rotTestHelpers[1] = NULL;
	m_rotTestRadius = 0.0f;

  m_pVehicle = static_cast<CVehicle*>(pVehicle);
  m_seatId = seatId;

	CVehicleParams rotationTable = table.findChild("RotateTurret");
	if(!rotationTable)
		return false;

	if (rotationTable.haveAttr("pitchPart"))
	{
		std::string pitchPart = rotationTable.getAttr("pitchPart");
		CVehiclePartBase* pPichtPart = static_cast<CVehiclePartBase*>(m_pVehicle->GetPart(pitchPart.c_str()));
		if (pPichtPart)
		{
			InitRotation(pPichtPart, eVTRT_Pitch);
			InitRotationSounds(pPichtPart, eVTRT_Pitch);
		}
	}

	if (rotationTable.haveAttr("yawPart"))
	{
		std::string yawPart = rotationTable.getAttr("yawPart");
		CVehiclePartBase* pYawPart = static_cast<CVehiclePartBase*>(m_pVehicle->GetPart(yawPart.c_str()));
		if (pYawPart)
		{
			InitRotation(pYawPart, eVTRT_Yaw);
			InitRotationSounds(pYawPart, eVTRT_Yaw);
		}
	}

	/* //CryMP: not supported 
	if (CVehicleParams rotationTestTable = rotationTable.findChild("RotationTest"))
	{
		if (rotationTestTable.haveAttr("helper1"))
		{
			m_rotTestHelperNames[0] = rotationTestTable.getAttr("helper1");

			if(IVehicleHelper* pHelper = m_pVehicle->GetHelper(m_rotTestHelperNames[0].c_str()))
				m_rotTestHelpers[0] = pHelper;
		}

		if (rotationTestTable.haveAttr("helper2"))
		{
			m_rotTestHelperNames[1] = rotationTestTable.getAttr("helper2");

			if(IVehicleHelper* pHelper = m_pVehicle->GetHelper(m_rotTestHelperNames[1].c_str()))
				m_rotTestHelpers[1] = pHelper;
		}

		rotationTestTable.getAttr("radius", m_rotTestRadius);
	}*/

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::Reset()
{
	m_aimGoal.zero();

	for(int i=0; i < eVTRT_NumRotationTypes; ++i)
	{
		if(m_rotations[i].m_pPart)
			m_rotations[i].m_prevBaseTM = m_rotations[i].m_pPart->GetLocalBaseTM();
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::StartUsing(EntityId passengerId)
{
	m_pUserEntity = gEnv->pEntitySystem->GetEntity(passengerId);
  m_aimGoal.zero();
  
  CVehicleSeat* pSeat = (CVehicleSeat*)m_pVehicle->GetSeatById(m_seatId);

  IVehiclePart::SVehiclePartEvent partEvent;
  partEvent.type = IVehiclePart::eVPE_StartUsing;

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

	for(int i=0; i < eVTRT_NumRotationTypes; ++i)
	{
		if(m_rotations[i].m_pPart)
		{
			m_rotations[i].m_relSpeed = 0.0f;
			m_rotations[i].m_prevBaseTM = m_rotations[i].m_pPart->GetLocalBaseTM();
			m_rotations[i].m_prevWorldQuat = Quat(m_rotations[i].m_pPart->GetWorldTM());

			m_rotations[i].m_pPart->OnEvent(partEvent);
			m_rotations[i].m_pPart->SetSeat(pSeat);
		}
	}

	// fetch helpers (was in PostInit but that doesn't exist for seat actions...)
	if(m_rotTestHelpers[0] == NULL && !m_rotTestHelperNames[0].empty())
	{
		m_rotTestHelpers[0] = m_pVehicle->GetHelper(m_rotTestHelperNames[0].c_str());
	}
	if(m_rotTestHelpers[1] == NULL && !m_rotTestHelperNames[1].empty())
	{
		m_rotTestHelpers[1] = m_pVehicle->GetHelper(m_rotTestHelperNames[1].c_str());
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::StopUsing()
{
	m_pUserEntity = NULL;
	m_aimGoal.zero();

  IVehiclePart::SVehiclePartEvent partEvent;
  partEvent.type = IVehiclePart::eVPE_StopUsing;

	for(int i=0; i < eVTRT_NumRotationTypes; ++i)
	{
		if(m_rotations[i].m_pPart)
		{
			m_rotations[i].m_pPart->OnEvent(partEvent);
			m_rotations[i].m_relSpeed = 0.f;
			m_pVehicle->StopSound(m_rotations[i].m_turnSoundId);
			m_pVehicle->StopSound(m_rotations[i].m_damageSoundId);
		}
	}
    
	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	CVehicleSeat* pSeat = (CVehicleSeat*)m_pVehicle->GetSeatById(m_seatId);

	if (!m_aimGoal.IsZero())
  {
    // if seat action is used remotely, rotation is set through aim goal, thus return here
    if (pSeat && pSeat->GetCurrentTransition() == CVehicleSeat::eVT_RemoteUsage)
      return;
  }
  
	if (actionId == eVAI_RotatePitch && m_rotations[eVTRT_Pitch].m_pPart)
	{
		m_rotations[eVTRT_Pitch].m_action = value * -1.0f;
		m_rotations[eVTRT_Pitch].m_hasReceivedAction = true;
	}
	else if (actionId == eVAI_RotateYaw && m_rotations[eVTRT_Yaw].m_pPart)
	{
		m_rotations[eVTRT_Yaw].m_action = value * -1.0f;
		m_rotations[eVTRT_Yaw].m_hasReceivedAction = true;
	}

	if (pSeat)
		pSeat->ChangedNetworkState(CVehicle::ASPECT_PART_MATRIX);
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::Serialize(TSerialize ser, unsigned aspects)
{
	CVehicleSeat *pSeat = (CVehicleSeat*)m_pVehicle->GetSeatById(m_seatId);

  // MR: for network, only turret parts are serialized
  // for savegame, all parts are serialized (by CVehicle)
  if (ser.GetSerializationTarget() == eST_Network)
  {
		for(int i=0; i<eVTRT_NumRotationTypes; ++i)
		{
			if (m_rotations[i].m_pPart)
			{
				m_rotations[i].m_pPart->SetSeat(pSeat);		
				m_rotations[i].m_pPart->Serialize(ser, aspects);

				if(ser.IsReading())
					m_rotations[i].m_hasReceivedAction = true;
			}
		}
	}
	else
	{
		// save rotation details
		for(int i=0; i<eVTRT_NumRotationTypes; ++i)
		{
			if(m_rotations[i].m_pPart)
			{
				Quat q;
				Matrix34 currentTM = m_rotations[i].m_pPart->GetLocalBaseTM();
				if (ser.IsWriting())
					q=Quat(currentTM);

				string tag;
				tag.Format("rotation_%s", i==eVTRT_Pitch ? "pitch" : "yaw");
				ser.Value(tag.c_str(), q, 'ori1');

				if (ser.IsReading())
				{			
					Matrix34 newTM(q);
					newTM.SetTranslation(currentTM.GetTranslation());
					m_rotations[i].m_pPart->SetLocalBaseTM(newTM);
					m_rotations[i].m_orientation.Set(q);

					if(CVehiclePartAnimated* pPart = CAST_VEHICLEOBJECT(CVehiclePartAnimated, m_rotations[i].m_pPart))
						pPart->ProcessAnimatedJointRotations();
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::Update(float frameTime)
{
	//CryMP: commented out
	//if (gEnv->bClient && m_pVehicle->GetGameObject()->IsProbablyDistant() && !m_pVehicle->GetGameObject()->IsProbablyVisible())
	//	return;

	if (!m_aimGoal.IsZero())
	{
		IVehiclePart* pPitchPart = m_rotations[eVTRT_Pitch].m_pPart;
		IVehiclePart* pYawPart = m_rotations[eVTRT_Yaw].m_pPart;
		CVehicleSeat* pSeat = (CVehicleSeat*)m_pVehicle->GetSeatById(m_seatId);

		if (pPitchPart)
		{
      const Matrix34& vehicleWorldTM = m_pVehicle->GetEntity()->GetWorldTM();
      Matrix34 pitchPartVehicleTM = pPitchPart->GetLocalTM(false);
      const Matrix34& yawPartVehicleTM = pYawPart->GetLocalTM(false);
      
      Vec3 pitchPartWorldPos = vehicleWorldTM * pitchPartVehicleTM.GetTranslation();
      Vec3 firingPos = pitchPartWorldPos;
      Vec3 vFiringDir = m_aimGoal - firingPos;
			
      if (CVehicleSeatActionWeapons* pWeaponAction = pSeat->GetSeatActionWeapons())
			{
				EntityId weaponId = pWeaponAction->GetWeaponEntityId(pWeaponAction->GetCurrentWeapon());
				pWeaponAction->GetFiringPos(weaponId, 0, firingPos);
				pWeaponAction->GetActualWeaponDir(weaponId, 0, vFiringDir, Vec3(), Vec3() );
			}

			// Just in case, fix the angle error between parts angle and weapon angle.

			Vec3 vDir = m_aimGoal - firingPos;//m_pPitchPart->GetWorldTM().GetTranslation();

			Vec3 vDirNormal	= (m_aimGoal - firingPos).GetNormalizedSafe();
			Vec3 vDirNear	= (m_aimGoal - pitchPartWorldPos).GetNormalizedSafe();

			if ( vDirNormal.Dot(vDirNear) < cosf( DEG2RAD(30.0f) ) )
				vDir = m_aimGoal - pitchPartWorldPos;// this is more stable, but not precise.

			vDir.NormalizeSafe();

/*			Vec3 v1 = pitchPartWorldPos;      
			Vec3 v2 = vehicleWorldTM * yawPartVehicleTM.GetTranslation(); 

			ColorB white(255, 255, 255);
			IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();
			pRenderAux->DrawLine(v1, white, m_aimGoal, white);
			pRenderAux->DrawLine(v2, white, m_aimGoal, white);
			pRenderAux->DrawLine(firingPos, white, m_aimGoal, white);
*/
			//setups of matrixes - globals

			Matrix33 rotWorld(Matrix33(pPitchPart->GetEntity()->GetWorldTM()));
			Matrix33 rotWorldInvert(rotWorld);
			rotWorldInvert.Invert();

			//setups of matrixes - locals

			Matrix33 rotMatX,rotMatZ;

			IVehiclePart* pParent = pPitchPart->GetParent();
			Matrix33 initTM = Matrix33(pPitchPart->GetLocalInitialTM());
			Matrix33 initialLocalLocalMat( initTM );			// initial local
			Matrix33 initialLocalLocalMatInvert( initTM );		// initial local
			initialLocalLocalMatInvert.Invert();
			while (pParent)
			{
				initTM = Matrix33(pParent->GetLocalInitialTM()) * initTM;
				pParent = pParent->GetParent();
			}

			Matrix33 initialLocalMat( initTM );					// initial local->vehicle
			Matrix33 initialLocalMatInvert( initialLocalMat );
			initialLocalMatInvert.Invert();

			Matrix33 localMat(pitchPartVehicleTM); // local->vehicle
			Matrix33 localMatYaw(pYawPart->GetLocalInitialTM());

			// get a default direction in vehicle space
			Vec3 defaultFireDir(0.0f,1.0f,0.0f);
			Vec3 defaultFireDirRot;
			Vec3 defaultFireDirRot2;

			// CHECK!!!: defaultFireDir should be always (0.0f,1.0f,0.0f);
			// but it seems to need painful assumption that if default yaw is -180, it means a model has (0,-1,0);
			defaultFireDir		= localMatYaw * defaultFireDir;

			defaultFireDir		= initialLocalMat * defaultFireDir;			// default turret direction in vehicle
			defaultFireDirRot	= initialLocalMatInvert * defaultFireDir;
			defaultFireDirRot	= localMat * defaultFireDirRot;

			float yaw = cry_atan2f(defaultFireDirRot.y,defaultFireDirRot.x);
			rotMatZ.SetRotationZ(-yaw);		
			defaultFireDirRot = rotMatZ * defaultFireDirRot;
			float pitch2 =cry_atan2f(defaultFireDirRot.z,defaultFireDirRot.x);

			// ideal direction in vehicle space
			Vec3 vecDir = rotWorldInvert * vDir;
			vecDir.NormalizeSafe();

			//Convert the direction to angles
			yaw = cry_atan2f(vecDir.y,vecDir.x);
			rotMatZ.SetRotationZ(-yaw);		
			vecDir = rotMatZ * vecDir;
			float pitch	=cry_atan2f(vecDir.z,vecDir.x);

			//get a difference between current angle and idal angle.
			pitch -= pitch2;

			// this pitch should be converted in the parts space;
			rotMatX.SetRotationX(pitch);	
			Vec3 defaultFireDir2(0.0f,1.0f,0.0f);

			defaultFireDir2 = rotMatX *initialLocalMat * initialLocalLocalMatInvert * defaultFireDir2;
			yaw = cry_atan2f(defaultFireDir2.y,defaultFireDir2.x);
			rotMatZ.SetRotationZ(-yaw);		
			defaultFireDir2 = rotMatZ * defaultFireDir2;
			pitch	=cry_atan2f(defaultFireDir2.z,defaultFireDir2.x);

			// clamp and finalize

			pitch = fmod(pitch,DEG2RAD(360.0f));
			if (pitch > DEG2RAD(180.0f))  pitch-=DEG2RAD(360.0f);
			if (pitch < DEG2RAD(-180.0f)) pitch+=DEG2RAD(360.0f);
			pitch *= -1.0f;

			Limit( pitch, DEG2RAD(-15.0f), DEG2RAD(15.0f) );
			m_rotations[eVTRT_Pitch].m_action = pitch * 100.0f * -1.0f;
			
			if (pYawPart)
			{
				rotWorld = Matrix33(pYawPart->GetEntity()->GetWorldTM());
				rotWorldInvert = rotWorld;
				rotWorldInvert.Invert();

				//setups of matrixes - locals

				pParent = pYawPart->GetParent();
				initTM = Matrix33(pYawPart->GetLocalInitialTM());
				initialLocalLocalMat = initTM;			// initial local
				initialLocalLocalMatInvert = initTM;		// initial local
				initialLocalLocalMatInvert.Invert();
				while (pParent)
				{
					initTM = Matrix33(pParent->GetLocalInitialTM()) * initTM;
					pParent = pParent->GetParent();
				}

				initialLocalMat = initTM;					// initial local->vehicle
				initialLocalMatInvert = initialLocalMat;
				initialLocalMatInvert.Invert();

				localMat = Matrix33(yawPartVehicleTM);	// local->vehicle
				localMatYaw = Matrix33(pYawPart->GetLocalInitialTM());

				// get a default direction in vehicle space
				defaultFireDir.Set(0.0f,1.0f,0.0f);
				
				// CHECK!!!: defaultFireDir should be always (0.0f,1.0f,0.0f);
				// but it seems to need painful assumption that if default yaw is -180, it means a model has (0,-1,0);
				defaultFireDir		= localMatYaw * defaultFireDir;

				defaultFireDir		= initialLocalMat * defaultFireDir;			// default turret direction in vehicle
				defaultFireDirRot	= initialLocalMatInvert * defaultFireDir;
				defaultFireDirRot	= localMat * defaultFireDirRot;

				float yaw2 = cry_atan2f(defaultFireDirRot.y,defaultFireDirRot.x);

				// ideal direction in vehicle space
				vecDir = rotWorldInvert * vDir;
				vecDir.NormalizeSafe();

				//Convert the direction to angles
				yaw = cry_atan2f(vecDir.y,vecDir.x);

				//if there is yawlimit
				if ( m_rotations[eVTRT_Yaw].m_minLimit != 0.f && m_rotations[eVTRT_Yaw].m_maxLimit != 0.f)
				{
					if ( ( yaw >DEG2RAD(90.0f) || yaw <DEG2RAD(-90.0f) ) && ( yaw2 <DEG2RAD(90.0f) && yaw2 >DEG2RAD(-90.0f) ) )
						yaw =DEG2RAD(15.0f);
					else if ( ( yaw2 >DEG2RAD(90.0f) || yaw2 <DEG2RAD(-90.0f) ) && ( yaw <DEG2RAD(90.0f) && yaw >DEG2RAD(-90.0f) ) )
						yaw =DEG2RAD(-15.0f);
					else
						yaw -=yaw2;
				}
				else
				{
					yaw -=yaw2;
				}

				// this yaw should be converted in the parts space;
				rotMatZ.SetRotationZ(yaw);	
				defaultFireDir2 = Vec3(1.0f,0.0f,0.0f);

				defaultFireDir2 = rotMatZ *initialLocalMat * initialLocalLocalMatInvert * defaultFireDir2;
				yaw = cry_atan2f(defaultFireDir2.y,defaultFireDir2.x);

				yaw =fmod(yaw,DEG2RAD(360.0f));
				if (yaw>DEG2RAD(180.0f))  yaw-=DEG2RAD(360.0f);
				if (yaw<DEG2RAD(-180.0f)) yaw+=DEG2RAD(360.0f);
				yaw *= -1.0f;

				Limit( yaw, DEG2RAD(-15.0f), DEG2RAD(15.0f) );
				m_rotations[eVTRT_Yaw].m_action = yaw * 100.0f * -1.0f;
			}
		}
	}

	// now update each rotation type
	for(int i=0; i<eVTRT_NumRotationTypes; ++i)
	{
		if(m_rotations[i].m_pPart)
		{
			EVehicleTurretRotationType type = (EVehicleTurretRotationType)i;
			UpdatePartRotation(type, frameTime);
			UpdateRotationSound(type, frameTime);
		}
	}

	m_aimGoalPriority = 0;
}

//------------------------------------------------------------------------
inline bool ComputeRotation(float &actual, float goal, float speed, float frameTime, float slowMult=1.f)
{
	if (actual == goal)
		return false;

	if (speed > 0.f)
	{
		bool slowDown = sgn(goal)*sgn(actual) == -1 || abs(goal) < abs(actual);

		float delta(goal - actual);
		actual += sgn(delta) * min(abs(delta), frameTime*speed*(slowDown ? slowMult : 1.f));

		return slowDown;
	}
	else
	{
		actual = goal;
	}  

	return false;
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::UpdatePartRotation(EVehicleTurretRotationType eType, float frameTime)
{
	//CryMP: commented out
	//if (gEnv->bClient && m_pVehicle->GetGameObject()->IsProbablyDistant() && !m_pVehicle->GetGameObject()->IsProbablyVisible())
	//	return;

	assert( eType < eVTRT_NumRotationTypes );

	const float threshold = 0.05f;	
	m_rotations[eType].m_rotationChanged = false;

	CVehiclePartBase* pPart = m_rotations[eType].m_pPart;
	IVehiclePart* pParent = pPart->GetParent();
	CVehicleSeat* pSeat = static_cast<CVehicleSeat*>(m_pVehicle->GetSeatById(m_seatId));
	IActor* pActor = pSeat ? pSeat->GetPassengerActor() : 0;

	bool remote = pSeat && pSeat->GetCurrentTransition() == IVehicleSeat::eVT_RemoteUsage;
	bool worldSpace = m_rotations[eType].m_rotWorldSpace > 0.f && VehicleCVars().v_independentMountedGuns != 0;

	bool checkRotation = (m_rotTestHelpers[0] && m_rotTestHelpers[1] && pActor );

	if (worldSpace && pParent && pActor && pActor->IsClient() && !remote)
	{ 
		// we want to keep the old worldspace rotation
		// therefore we're updatink the local transform from it

		Matrix34 localTM = pParent->GetWorldTM().GetInverted() * Matrix34(m_rotations[eType].m_prevWorldQuat);
		localTM.OrthonormalizeFast(); // precision issue

		const Matrix34& baseTM = pPart->GetLocalBaseTM();

		if (!baseTM.IsEquivalent(localTM))
		{
			Ang3 anglesCurr(baseTM);
			Ang3 angles(localTM);

			if (eType == eVTRT_Pitch)
			{ 
				if (m_rotations[eType].m_maxLimit != 0.f || m_rotations[eType].m_minLimit != 0.f)
					Limit(angles.x, m_rotations[eType].m_minLimit, m_rotations[eType].m_maxLimit);

				angles.y = anglesCurr.y;
				angles.z = anglesCurr.z;
			}    
			else if (eType == eVTRT_Yaw)
			{ 
				angles.x = anglesCurr.x;
				angles.y = anglesCurr.y;      

				if (m_rotations[eType].m_maxLimit != 0.f || m_rotations[eType].m_minLimit != 0.f)
					Limit(angles.x, m_rotations[eType].m_minLimit, m_rotations[eType].m_maxLimit);
			}

			localTM.SetRotationXYZ(angles);
			localTM.SetTranslation(baseTM.GetTranslation());
			pPart->SetLocalBaseTM(localTM);

			pSeat->ChangedNetworkState(CVehicle::ASPECT_PART_MATRIX);
		}

		if (VehicleCVars().v_debugdraw == eVDB_Parts)
		{
			float color[] = {1,1,1,1};
			Ang3 a(localTM), aBase(baseTM);    
			gEnv->pRenderer->Draw2dLabel(200,200,1.4f,color,false,"localAng: %.1f (real: %.1f)", RAD2DEG(a.z), RAD2DEG(aBase.z));        
		}    
	}

	m_rotations[eType].m_prevBaseTM = m_rotations[eType].m_pPart->GetLocalBaseTM();
	float updatedAngle = m_rotations[eType].m_orientation.Update(frameTime);

	float convertedPitchMax = m_rotations[eVTRT_Pitch].m_maxLimit;
	float convertedPitchMin = m_rotations[eVTRT_Pitch].m_minLimit;

	Matrix34 tm = pPart->GetLocalBaseTM();

	if (!(pActor && pActor->IsPlayer()))
	{
		// needed for AI (ask Tetsuji)
		Matrix33 initTM = Matrix33(pPart->GetLocalInitialTM());
		Matrix33 initialLocalLocalMatInvert( initTM );
		initialLocalLocalMatInvert.Invert();

		while (pParent)
		{
			initTM = Matrix33(pParent->GetLocalInitialTM()) * initTM;
			pParent = pParent->GetParent();
		}

		Matrix33 initialLocalMat( initTM );
		Matrix33 rotMatZ,rotMatXMin,rotMatXMax;

		float yaw;

		// this pitch should be converted in the parts space;
		rotMatXMax.SetRotationX(convertedPitchMax);	
		rotMatXMin.SetRotationX(convertedPitchMin);	
		Vec3 defaultFireDir(0.0f,1.0f,0.0f);
		Vec3 defaultFireDirRot;

		defaultFireDirRot = rotMatXMax *initialLocalMat * initialLocalLocalMatInvert * defaultFireDir;
		yaw = cry_atan2f(defaultFireDirRot.y,defaultFireDirRot.x);
		rotMatZ.SetRotationZ(-yaw);		
		defaultFireDirRot = rotMatZ * defaultFireDirRot;
		convertedPitchMax	=cry_atan2f(defaultFireDirRot.z,defaultFireDirRot.x);

		defaultFireDirRot = rotMatXMin *initialLocalMat * initialLocalLocalMatInvert * defaultFireDir;
		yaw = cry_atan2f(defaultFireDirRot.y,defaultFireDirRot.x);
		rotMatZ.SetRotationZ(-yaw);		
		defaultFireDirRot = rotMatZ * defaultFireDirRot;
		convertedPitchMin	=cry_atan2f(defaultFireDirRot.z,defaultFireDirRot.x);

		if ( convertedPitchMin > convertedPitchMax )
			std::swap( convertedPitchMin, convertedPitchMax );
	}

	{
		m_rotations[eType].m_previousValue = m_rotations[eType].m_currentValue;
		bool slowed = ComputeRotation(m_rotations[eType].m_currentValue, m_rotations[eType].m_action, m_rotations[eType].m_acceleration, frameTime, 1.8f);
		
		if (IsDebugParts())
		{
			float color[] = {1,1,1,1};
			const auto &info = eType == eVTRT_Yaw ? m_rotations[eVTRT_Yaw] : m_rotations[eVTRT_Pitch];
			if(eType == eVTRT_Yaw)
				gEnv->pRenderer->Draw2dLabel(300, 250, 1.5f, color, false,
					"z: %.1f - limit %.1f / %.1f  %s", 
					info.m_currentValue,
					info.m_speed, 
					info.m_acceleration), 
					(slowed ? "(slowdown)" : "");
			else
				gEnv->pRenderer->Draw2dLabel(300, 280, 1.5f, color, false,
					"z: %.1f - limit %.1f / %.1f  %s",
					info.m_currentValue,
					info.m_speed,
					info.m_acceleration),
				(slowed ? "(slowdown)" : "");
		}
	}

	Ang3 deltaAngles;

	if (m_rotations[eType].m_pPart && abs(m_rotations[eType].m_currentValue) > threshold)
	{      
		float delta = TURNSPEED_COEFF * max(min(m_rotations[eType].m_currentValue, m_rotations[eType].m_speed), -m_rotations[eType].m_speed) * GetDamageSpeedMul(pPart);
		m_rotations[eType].m_rotationChanged = true;

		if(eType == eVTRT_Pitch)
			deltaAngles.x = delta;
		else if(eType == eVTRT_Yaw)
			deltaAngles.z = delta;
		else
			assert(false && "Unknown turret rotation");
	}

	if (m_rotations[eType].m_pPart->IsRotationBlocked())
	{
		deltaAngles.x = m_rotations[eType].m_speed * TURNSPEED_COEFF * 2.0f;
		deltaAngles.z = 0.0f;
		m_rotations[eType].m_rotationChanged = true;
	}

	if (m_rotations[eType].m_rotationChanged)
	{ 
		m_rotations[eType].m_orientation.Set(Quat(Matrix33(tm) * Matrix33::CreateRotationXYZ(deltaAngles)));
		Ang3 angles = Ang3::GetAnglesXYZ(m_rotations[eType].m_orientation.Get());

		bool clamped = false;

		// clamp to limits
		if(eType == eVTRT_Pitch)
		{
			if (m_rotations[eType].m_minLimit != 0.0f || m_rotations[eType].m_maxLimit != 0.0f)
			{ 
				if (angles.x > convertedPitchMax || angles.x < convertedPitchMin)
				{
					angles.x = max(min(angles.x, convertedPitchMax), convertedPitchMin);
					m_rotations[eType].m_currentValue = 0.f;
					clamped = true;
				}
			}
		}
		else
		{
			if (m_rotations[eType].m_maxLimit != 0.0f || m_rotations[eType].m_minLimit != 0.0f)
			{
				if (angles.z > m_rotations[eType].m_maxLimit || angles.z < m_rotations[eType].m_maxLimit)
				{
					angles.z = max(min(angles.z, m_rotations[eType].m_maxLimit), m_rotations[eType].m_minLimit);
					m_rotations[eType].m_currentValue = 0.f;
					clamped = true;
				}
			}
		}

		if (clamped)
			m_rotations[eType].m_orientation.Set(Quat::CreateRotationXYZ(angles));
	}

	m_rotations[eType].m_action = 0.0f;

	bool updated = updatedAngle > 0.f || m_rotations[eType].m_rotationChanged || m_rotations[eType].m_orientation.IsInterpolating();

	Vec3 oldHelper2Pos;
	if(checkRotation)
	{
		oldHelper2Pos = m_rotTestHelpers[1]->GetWorldTM().GetTranslation();
	}

	if (updated)
	{ 
		Matrix34 newTM(m_rotations[eType].m_orientation.Get().GetNormalized());
		newTM.SetTranslation(tm.GetTranslation());

		pPart->SetLocalBaseTM(newTM);
	}

	const Matrix34& worldTM = pPart->GetWorldTM();
	m_rotations[eType].m_prevWorldQuat = Quat(worldTM);
	assert(m_rotations[eType].m_prevWorldQuat.IsValid());

	if(updated && checkRotation)
	{
		// force recalculation based on the new matrix set above.
		m_rotTestHelpers[0]->Invalidate();
		m_rotTestHelpers[1]->Invalidate();

		// need to test the new rotations before applying them. Sweep a sphere between the two helpers and check for collisions...
		static IPhysicalEntity* pSkipEntities[10];
		int numSkip = m_pVehicle->GetSkipEntities(pSkipEntities, 10);
		primitives::sphere sphere;
		sphere.center = m_rotTestHelpers[0]->GetWorldTM().GetTranslation();
		sphere.r = m_rotTestRadius;

		geom_contact* pContact = NULL;
		Vec3 dir = m_rotTestHelpers[1]->GetWorldTM().GetTranslation() - sphere.center;
		float hit = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, dir, ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid, &pContact, 0, (geom_colltype_player<<rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEntities, numSkip);
		if(hit > 0.001f && pContact)
		{
			// ignore contacts with breakable things, as moving the barrel into them should break them.
			bool ignore = false;
			if(pContact)
			{
				ISurfaceType* pST = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceType(pContact->id[1]);
				if(pST && pST->GetBreakability() > 0)
					ignore = true;
			}

			if(!ignore)
			{
				// there was a collision. check whether the barrel is moving towards the collision point or not... if not, ignore the collision.
				if(VehicleCVars().v_debugdraw > 0)
				{
					/*
					CPersistantDebug* pPD = gEnv->pGame->GetIGameFramework()->GetPersistantDebug();
					pPD->Begin("VehicleCannon", false);

					ColorF col(1.0f, 0.0f, 0.0f, 1.0f);
					if(pContact && hit > 0.0f)
					{
						pPD->AddSphere(pContact->pt, 0.1f, col, 30.0f);
					}*/
				}

				Vec3 endPos = m_rotTestHelpers[1]->GetWorldTM().GetTranslation();
				Vec3 moveDir = (endPos - oldHelper2Pos).GetNormalizedSafe();
				Vec3 hitDir = (pContact->pt - oldHelper2Pos).GetNormalizedSafe();

				//CryLog("Collision dot=%.2f", moveDir.Dot(hitDir));
				if(moveDir.Dot(hitDir) > 0.0f)
				{
					// reset as though the rotation never happened.
					pPart->SetLocalBaseTM(m_rotations[eType].m_prevBaseTM);
					updatedAngle = 0.0f;
					m_rotations[eType].m_orientation.Set(Quat(m_rotations[eType].m_prevBaseTM));
					m_rotations[eType].m_prevWorldQuat = Quat(m_rotations[eType].m_prevBaseTM);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::SetAimGoal(Vec3 aimPos, int priority)
{
	if (m_aimGoalPriority <= priority)
	{
		m_aimGoal = aimPos;
		m_aimGoalPriority = priority;    
	}
}


//------------------------------------------------------------------------
float CVehicleSeatActionRotateTurret::GetDamageSpeedMul(CVehiclePartBase* pPart)
{
	// slowdown by max 50%, starting from 75% damage    
	return 1.0f - 2.f*max(min(1.f,pPart->m_damageRatio)-0.75f, 0.f);
}

//------------------------------------------------------------------------
bool CVehicleSeatActionRotateTurret::InitRotation(CVehiclePartBase* pPart, EVehicleTurretRotationType eType)
{
	if (!pPart)
		return false;

	const CVehiclePartBase::RotationParams& params = pPart->GetRotationParams();

	m_rotations[eType].m_pPart = pPart;

	m_rotations[eType].m_speed = params.speed;
	m_rotations[eType].m_acceleration = params.accel;
	m_rotations[eType].m_minLimit = DEG2RAD(params.limitMin);
	m_rotations[eType].m_maxLimit = DEG2RAD(params.limitMax);
	m_rotations[eType].m_rotWorldSpace = params.worldSpace;

	pPart->SetMoveable();

	//CryLogAlways("[%s] Inited rotation for part %s, speed: %.2f, accel: %.2f, limits: %.2f - %.2f",
	//	pPart->GetParent()->GetEntity()->GetName(),
	//	pPart->GetName(), m_rotations[eType].m_speed, m_rotations[eType].m_acceleration, m_rotations[eType].m_minLimit, m_rotations[eType].m_maxLimit);

	return true;
}

//------------------------------------------------------------------------
bool CVehicleSeatActionRotateTurret::InitRotationSounds(CVehiclePartBase* pPart, EVehicleTurretRotationType eType)
{
	const CVehiclePartBase::RotationParams& params = pPart->GetRotationParams();

	if (!params.soundEvent.empty())
	{
		if (!params.soundHelper.empty())
		{
			if (IVehicleHelper* pHelper = m_pVehicle->GetHelper(params.soundHelper.c_str()))
			{
				SVehicleSoundInfo info;
				info.name = params.soundEvent.c_str();
				info.pHelper = pHelper;
				m_rotations[eType].m_turnSoundId = m_pVehicle->AddSoundEvent(info);  

				if (!params.soundEventDamage.empty())
				{
					SVehicleSoundInfo dmgInfo;
					info.name = params.soundEventDamage.c_str();
					info.pHelper = pHelper;
					m_rotations[eType].m_damageSoundId = m_pVehicle->AddSoundEvent(info);
				}

				//CryLogAlways("[%s] Inited rotation sound for part %s, event: %s, helper: %s",
				//	pPart->GetParent()->GetEntity()->GetName(),
				//	pPart->GetName(), params.soundEvent.c_str(), params.soundHelper.c_str());

				return true;
			}
		}

		return false;
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::UpdateRotationSound(EVehicleTurretRotationType type, float deltaTime)
{
	// update rotation sound, if available
	if (m_rotations[type].m_turnSoundId == InvalidSoundEventId)
		return;

	if (m_pVehicle->GetGameObject()->IsProbablyDistant())
		return;

	const static float minSpeed = 0.0002f;
	const static float soundDamageRatio = 0.2f;

	float speed = abs(m_rotations[type].m_currentValue - m_rotations[type].m_previousValue);
	speed *= GetDamageSpeedMul(m_rotations[type].m_pPart);

	bool bDamage = m_rotations[type].m_pPart->m_damageRatio > soundDamageRatio && m_pVehicle->IsPlayerPassenger();

	if (speed != m_rotations[type].m_relSpeed)
	{      
		if (speed < 1e-4 && m_rotations[type].m_relSpeed < 1e-4)
			m_rotations[type].m_relSpeed = 0.f;
		else
			Interpolate(m_rotations[type].m_relSpeed, speed, 8.f, deltaTime);

		float speedParam = (speed != 0) ? min(1.f, m_rotations[type].m_relSpeed/speed) : 0;
		m_pVehicle->SetSoundParam(m_rotations[type].m_turnSoundId, "speed", speedParam, true);    

		if (bDamage)
			m_pVehicle->SetSoundParam(m_rotations[type].m_damageSoundId, "speed", speedParam, true);   

		if (m_rotations[type].m_relSpeed < minSpeed)
		{
			m_pVehicle->StopSound(m_rotations[type].m_turnSoundId);
			m_pVehicle->StopSound(m_rotations[type].m_damageSoundId);
		}
	}   

	float inout = 1.f;
	if (m_pVehicle->IsPlayerPassenger())
	{ 
		if (IVehicleSeat* pSeat = m_pVehicle->GetSeatForPassenger(gEnv->pGame->GetIGameFramework()->GetClientActor()->GetEntityId()))
		{
			if (IVehicleView* pView = pSeat->GetView(pSeat->GetCurrentView()))
			{
				if (!pView->IsThirdPerson())
					inout = pSeat->GetSoundParams().inout;
			}
		}
	}        

	if (ISound* pSound = m_pVehicle->GetSound(m_rotations[type].m_turnSoundId, false))
		pSound->SetParam("in_out", inout, false); 

	if (bDamage) 
	{
		if (ISound* pSound = m_pVehicle->GetSound(m_rotations[type].m_damageSoundId, false))
		{
			pSound->SetParam("in_out", inout, false);     
			pSound->SetParam("damage", min(1.f, m_rotations[type].m_pPart->m_damageRatio), false);
		}      
	} 
}

//------------------------------------------------------------------------
bool CVehicleSeatActionRotateTurret::GetRotationLimits(int axis, float& min, float& max)
{
	EVehicleTurretRotationType type;
	if(axis == 0)
		type = eVTRT_Pitch;
	else if(axis == 2)
		type = eVTRT_Yaw;
	else 
		return false;

	min = m_rotations[type].m_minLimit;
	max = m_rotations[type].m_maxLimit;
	return true;
}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionRotateTurret);
