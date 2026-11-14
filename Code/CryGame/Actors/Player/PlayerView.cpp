/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

Description:
	The view specific code (First person, Third person, followCharacterHead etc).


-------------------------------------------------------------------------
History:
- 21:02:2006: Re-factored from Filippo's CPlayer code by Nick Hesketh

*************************************************************************/

#include "CryCommon/CrySystem/ISystem.h"

#include "CryGame/Game.h"
#include "CryGame/GameCVars.h"

#include "Player.h"
#include "PlayerView.h"
#include "CryCommon/CryGame/GameUtils.h"
#include "CryGame/GameActions.h"

#include "CryGame/Items/Weapons/Weapon.h"
#include "CryGame/Items/Weapons/WeaponSystem.h"
#include "CryGame/Items/Weapons/FireModes/Single.h"

#include "CryCommon/CryAction/IViewSystem.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CryPhysics/IPhysics.h"
#include "CryCommon/CryAnimation/ICryAnimation.h"
#include "CryCommon/CryAISystem/IAISystem.h"
#include "CryCommon/CryAISystem/IAgent.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryNetwork/ISerialize.h"

#include "CryCommon/CryRenderer/IRenderAuxGeom.h"

#include "CryCommon/CryAction/IGameTokens.h"

#include "CryGame/HUD/HUD.h"

#include "CryGame/Items/Weapons/OffHand.h"
#include "CryGame/Actors/Player/PlayerInput.h"

PlayerView::PlayerView(CPlayer& player) : m_player(player)
{
}

PlayerView::~PlayerView()
{
}

void PlayerView::Reset()
{
	m_lastPos = {};
	m_lastQuat = Quat(IDENTITY);
	m_lastSeatId = 0;
	m_ColDistance = 0.0f;
	m_defaultFov = 0.0f;
	m_frameTime = 0.0f;
	m_health = 0.0f;
	m_smoothViewZ = 0.0f;
	m_smoothZType = 0.0f;
	m_bIsGrabbing = false;
	m_bUsePivot = false;
	m_landed = false;
	m_jumped = false;
	m_vehicleReverseView = false;
	m_bobMul = 0.0f;
	m_bobCycle = 0.0f;
	m_standSpeed = 0.0f;
	m_standSpeedMax = 0.0f;
	m_viewQuatForWeapon = Quat(IDENTITY);
	m_eyeOffsetViewGoal = {};
	m_wAngles = {};
	m_viewQuatFinal = Quat(IDENTITY);
	m_viewQuat = Quat(IDENTITY);
	m_baseQuat = Quat(IDENTITY);
	m_eyeOffsetView = {};
	m_localEyePos = {};
	m_worldEyePos = {};
	m_entityWorldPos = {};
	m_vFPWeaponAngleOffset = {};
	m_vFPWeaponLastDirVec = {};
	m_vFPWeaponOffset = {};
	m_bobOffset = {};
	m_angleOffset = {};
	m_viewAngleOffset = {};
	m_entityWorldMatrix = Matrix34(IDENTITY);
	m_headMtxLocal = Matrix33(IDENTITY);
}

void PlayerView::Update(SViewParams& viewParams)
{
	ViewPreProcess(viewParams);
	ViewProcess(viewParams);
	ViewPostProcess(viewParams);
}

void PlayerView::OnExitVehicle()
{
	if (m_player.IsRemote())
	{
		StopVehicleViewUpdates();
	}
}

//--------------------------------------------------------------------------
//--- ViewPreProcess
//--------------------------------------------------------------------------
// Fetch all the data we need to work on.
//--------------------------------------------------------------------------
void PlayerView::ViewPreProcess(SViewParams& viewParams)
{
	SPlayerStats& stats = m_player.m_stats;
	m_lastPos = viewParams.position;
	m_lastQuat = viewParams.rotation;
	m_defaultFov = g_pGameCVars->cl_fov;

	m_frameTime = min(gEnv->pTimer->GetFrameTime(), 0.1f);
	viewParams.frameTime = m_frameTime;

	m_pCharacter = m_player.GetEntity()->GetCharacter(0);
	m_pVehicle = m_player.GetLinkedVehicle();

	m_jumped = m_player.m_stats.jumped;
	m_landed = m_player.m_stats.landed;

	m_bobMul = g_pGameCVars->cl_bob * m_player.m_params.weaponBobbingMultiplier;

	m_entityWorldPos = m_player.GetEntity()->GetWorldPos();

	//if (m_player.IsFpSpectatorTarget())
	//{
		//m_entityWorldPos = m_player.GetEntity()->GetLocalTM().GetTranslation();
	//}

	if (m_player.IsClient())
	{
		CPlayerInput* pPlayerInput = static_cast<CPlayerInput*>(m_player.GetPlayerInput());
		if (pPlayerInput)
		{
			m_vehicleReverseView = pPlayerInput->IsVehicleReverseView();
		}
	}

	m_playerId = m_player.GetEntityId();

	// use absolute entity matrix when frozen
	if (m_player.m_stats.isFrozen.Value())
		m_entityWorldMatrix = Matrix34(m_player.GetEntity()->GetWorldTM());
	else
		m_entityWorldMatrix = Matrix34(m_player.GetEntity()->GetSlotWorldTM(0));

	m_localEyePos = m_player.GetLocalEyePos();
	m_worldEyePos = m_entityWorldMatrix * m_localEyePos;

	m_headMtxLocal.SetIdentity();

	if (stats.followCharacterHead.Value() || m_player.IsFpSpectatorTarget()) //CryMP FP spec
	{
		const int16 joint_id = m_player.GetBoneID(BONE_HEAD);
		if (joint_id >= 0 && m_pCharacter)
		{
			m_headMtxLocal = Matrix33(m_pCharacter->GetISkeletonPose()->GetAbsJointByID(joint_id).q.GetNormalized());
		}
	}

	EStance refStance((m_player.m_stance == STANCE_PRONE) ? STANCE_PRONE : STANCE_STAND);
	m_standSpeedMax = m_player.GetStanceInfo(refStance)->maxSpeed;
	m_standSpeed = m_player.GetStanceMaxSpeed(refStance);

	m_health = m_player.GetHealth();

	m_eyeOffsetViewGoal = m_player.GetStanceViewOffset(m_player.m_stance);
	m_eyeOffsetView = m_player.m_eyeOffsetView;

	m_viewQuatFinal = m_player.m_viewQuatFinal;
	m_viewQuat = m_player.m_viewQuat;

	m_vFPWeaponOffset = m_player.m_FPWeaponOffset;
	m_baseQuat = m_player.m_baseQuat;

	m_vFPWeaponAngleOffset = m_player.m_FPWeaponAngleOffset;
	m_bobCycle = m_player.m_stats.bobCycle;

	m_vFPWeaponLastDirVec = m_player.m_FPWeaponLastDirVec;
	m_bobOffset = m_player.m_bobOffset;
	m_angleOffset = m_player.m_angleOffset;

	m_smoothViewZ = m_player.m_stats.smoothViewZ;
	m_smoothZType = m_player.m_stats.smoothZType;

	// *****
	viewParams.fov = m_defaultFov * m_player.m_params.viewFoVScale * (gf_PI / 180.0f);
	viewParams.nearplane = 0.0f;
}

//--------------------------------------------------------------------------
//--- ViewProcess
//--------------------------------------------------------------------------
// Process our local copy of the data.
// (This is where all the actual work should occur)
//--------------------------------------------------------------------------
void PlayerView::ViewProcess(SViewParams& viewParams)
{
	const SPlayerStats &stats = m_player.m_stats;
	if (gEnv->bMultiplayer && stats.deathCamTarget && g_pGameCVars->g_deathCam != 0 
		&& ((m_player.IsFpSpectatorTarget() && stats.isRagDoll /*CryMP: other clients hp is not serialized*/)
			|| (stats.spectatorMode == CActor::eASM_None && m_health <= 0)))
	{
		ViewFirstThirdSharedPre(viewParams);
		ViewDeathCamTarget(viewParams);
		ViewFirstThirdSharedPost(viewParams);
	}
	// Externally controlled first person view e.g. by animation
	else if ((!stats.isRagDoll || stats.isFrozen.Value() || stats.isStandingUp) 
		&& !stats.isThirdPerson && !stats.isOnLadder.Value() && m_pCharacter && (stats.firstPersonBody.Value() == 1 || stats.followCharacterHead.Value() > 0))
	{
		ViewFollowCharacterFirstPerson(viewParams);
	}
	else if (stats.followCharacterHead.Value() && stats.isThirdPerson)
	{
		ViewFollowCharacterFirstPerson(viewParams);
	}
	else if (m_pVehicle)
	{
		ViewVehicle(viewParams);
	}
	else if (stats.isOnLadder.Value())
	{
		ViewFirstThirdSharedPre(viewParams);

		if (stats.isThirdPerson)
			ViewThirdPersonOnLadder(viewParams);
		else
			ViewFirstPersonOnLadder(viewParams);
	}
	else if (stats.spectatorMode == CActor::eASM_Follow)
	{
		ViewFirstThirdSharedPre(viewParams);

		//New Spectator (TP)
		if (g_pGameCVars->mp_newSpectator)
			ViewSpectatorTarget_CryMP(viewParams);
		else
			ViewSpectatorTarget(viewParams);

		ViewFirstThirdSharedPost(viewParams);
	}
	else
	{
		ViewFirstThirdSharedPre(viewParams);

		if (stats.isThirdPerson || stats.isShattered.Value() || m_bUsePivot)
			ViewThirdPerson(viewParams);
		else
			ViewFirstPerson(viewParams);

		ViewFirstThirdSharedPost(viewParams);
	}
}

//--------------------------------------------------------------------------
//--- ViewPostProcess
//--------------------------------------------------------------------------
// Commit all the changes
//--------------------------------------------------------------------------
void PlayerView::ViewPostProcess(SViewParams& viewParams)
{
	/*IEntity *pLinked = m_player.GetLinkedEntity();
	if (pLinked && m_player.m_stats.linkedFreeLook)
	{
		viewParams.rotation = Quat(pLinked->GetWorldTM()) * viewParams.rotation;
	}*/

	// update the player rotation if view control is taken from somewhere else (e.g. animation or vehicle)
	ViewExternalControlPostProcess(viewParams);

	//set first person weapon position/rotation
	FirstPersonWeaponPostProcess(viewParams);

	ViewShakePostProcess(viewParams);

	//m_player.m_viewQuat = m_viewQuat;
	//FIXME:updating the baseMatrix due being in a vehicle or having a first person animations playing has to be moved somewhere else
	//m_player.m_baseQuat = m_baseQuat;

	if (!m_player.IsTimeDemo())
	{
		//m_player.m_viewQuatFinal = m_viewQuatFinal;
	}

	//m_player.m_stats.FPSecWeaponAngles = m_io.stats_FPSecWeaponAngles;
	//m_player.m_stats.FPSecWeaponPos = m_io.stats_FPSecWeaponPos;
	//m_player.m_viewShake=m_io.viewShake;

	m_player.m_eyeOffsetView = m_eyeOffsetView;
	m_player.m_stats.bobCycle = m_bobCycle;

	m_player.m_FPWeaponAngleOffset = m_vFPWeaponAngleOffset;
	m_player.m_FPWeaponLastDirVec = m_vFPWeaponLastDirVec;
	m_player.m_FPWeaponOffset = m_vFPWeaponOffset;
	m_player.m_bobOffset = m_bobOffset;
	m_player.m_angleOffset = m_angleOffset;

	m_player.m_stats.landed = m_landed;
	m_player.m_stats.jumped = m_jumped;

	m_player.m_stats.smoothViewZ = m_smoothViewZ;
	m_player.m_stats.smoothZType = m_smoothZType;
	//--------------------------

	m_player.m_stats.shakeAmount = viewParams.shakingRatio;

	HandsPostProcess(viewParams);
}


void PlayerView::ViewFirstThirdSharedPre(SViewParams& viewParams)
{
	const SPlayerStats& stats = m_player.m_stats;
	// don't blend view when spectating
	if (stats.spectatorMode >= CActor::eASM_FirstMPMode && stats.spectatorMode <= CActor::eASM_LastMPMode)
	{
		m_player.SupressViewBlending();
	}

	viewParams.viewID = 0;

	m_bUsePivot = (m_player.m_params.viewPivot.len2() > 0) && !stats.isThirdPerson && !stats.isOnLadder.Value();
	viewParams.position = m_bUsePivot ? m_player.m_params.viewPivot : m_entityWorldPos;

	m_viewQuatForWeapon = m_viewQuatFinal;
	m_viewQuatFinal *= Quat::CreateRotationXYZ(m_angleOffset * (gf_PI / 180.0f));

	viewParams.rotation = m_viewQuatFinal;
}

void PlayerView::ViewFirstThirdSharedPost(SViewParams& viewParams)
{
	const SPlayerStats& stats = m_player.m_stats;

	//--- Update the eye offset and apply 
	// Blend towards the goal eye offset
	float stanceTransitionSpeed = g_pGameCVars->g_stanceTransitionSpeed;
	if ((m_player.GetStance() == STANCE_PRONE) ||
		(m_eyeOffsetView.z < (m_eyeOffsetViewGoal.z - 0.2f)))
		stanceTransitionSpeed = g_pGameCVars->g_stanceTransitionSpeedSecondary;

	Interpolate(m_eyeOffsetView, m_eyeOffsetViewGoal, stanceTransitionSpeed, m_frameTime);

	//apply eye offset
	if (!m_bUsePivot)
	{
		//apply some more offset to the view
		viewParams.position += m_baseQuat * m_eyeOffsetView;

		if (stats.firstPersonBody.Value() == 2)
		{
			float lookDown(m_viewQuatFinal.GetColumn1() * m_baseQuat.GetColumn2());
			float forwardOffset(0.0f);
			if (m_player.GetStance() == STANCE_STAND)
				forwardOffset = 0.15f;

			viewParams.position += m_viewQuatFinal.GetColumn1() * max(-lookDown, 0.0f) * forwardOffset + m_viewQuatFinal.GetColumn2() * max(-lookDown, 0.0f) * 0.0f;
			//Grabbed by the Hunter
			if (stats.isGrabbed && !stats.isThirdPerson)
				viewParams.position = m_entityWorldMatrix.TransformPoint(m_eyeOffsetView);
		}
	}

	if (g_pGameCVars->g_detachCamera != 0)
	{
		viewParams.position = m_lastPos;
		viewParams.rotation = m_lastQuat;
	}

	//--- Weapon orientation
	//FIXME: this should be done in the player update anyway.
	//And all the view position update. (because the game may need to know other players eyepos and such)
	//update first person weapon model position/angles
	
	Quat wQuat(m_viewQuatForWeapon * Quat::CreateRotationXYZ(m_vFPWeaponAngleOffset * gf_PI / 180.0f));
	//wQuat *= Quat::CreateSlerp(viewParams.shakeQuat,IDENTITY,0.5f);
	wQuat *= Quat::CreateSlerp(viewParams.currentShakeQuat, IDENTITY, 0.5f);
	wQuat.Normalize();

	//smooth out the view elevation		
	if (stats.inAir < 0.1f && !stats.flyMode && !stats.spectatorMode && !m_bUsePivot)
	{
		if (m_smoothZType != 1)
		{
			m_smoothViewZ = viewParams.position.z;
			m_smoothZType = 1;
		}

		Interpolate(m_smoothViewZ, viewParams.position.z, 15.0f, m_frameTime);
		viewParams.position.z = m_smoothViewZ;
	}
	else
	{
		if (m_smoothZType == 1)
		{
			m_smoothViewZ = m_lastPos.z - m_smoothViewZ;
			m_smoothZType = 2;
		}

		Interpolate(m_smoothViewZ, 0.0f, 15.0f, m_frameTime);
		viewParams.position.z += m_smoothViewZ;
	}
}

Vec3 PlayerView::Project(Vec3 vector, Vec3 onNormal)
{
	const f32 sqrMag = onNormal.Dot(onNormal);
	const f32 dot = vector.Dot(onNormal);
	return Vec3(onNormal.x * dot / sqrMag,
		onNormal.y * dot / sqrMag,
		onNormal.z * dot / sqrMag);
}

Vec3 PlayerView::GetCollision(CActor *pActor, const Vec3 transformPos, const Vec3 forward, const Vec3 vector, float radius, bool pushByNormal)
{
	int farEnough = 1;

	Vec3 origin = vector;
	Vec3 checkDir = origin - transformPos;

	const auto dt = forward.Dot(checkDir);
	if (dt < 0)
	{
		checkDir *= -1;
	}

	IPhysicalEntity* pSkipEnts[2] = {};
	int nSkip = 0;
	if (IPhysicalEntity *pActorPhysics = pActor->GetEntity()->GetPhysics())
	{
		pSkipEnts[nSkip++] = pActorPhysics;

		if (pActor->GetHeldObjectId())
		{
			IEntity* pObject = gEnv->pEntitySystem->GetEntity(pActor->GetHeldObjectId());
			if (pObject && pObject->GetPhysics())
			{
				pSkipEnts[nSkip++] = pObject->GetPhysics();
			}
		}
	}

	geom_contact* pContact = nullptr;
	primitives::sphere sphere;
	sphere.center = origin;
	sphere.r = radius;
	const float hitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, checkDir.normalized() * farEnough,
		ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid /* |ent_living*/, &pContact, 0, (geom_colltype_player << rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEnts, nSkip);

	if (hitDist > 0.0f && pContact)
	{
		origin = origin + checkDir.normalized() * hitDist;
	}
	else
	{
		origin += checkDir.normalized() * farEnough;
	}

	checkDir = origin - transformPos;

	geom_contact* pSecContact = nullptr;
	sphere.center = origin;
	sphere.r = radius;
	
	Vec3 newDir = -checkDir.normalized() * checkDir.GetLength();
	const float hitDist2 = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, newDir,
		ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid /* |ent_living*/, &pSecContact, 0, (geom_colltype_player << rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEnts, nSkip);

	if (hitDist2 > 0.0f && pSecContact)
	{
		return pushByNormal ? pSecContact->pt + pSecContact->n * radius : pSecContact->pt;
	}
	else
	{
		return transformPos;
	}
}

void PlayerView::ViewThirdPerson(SViewParams& viewParams)
{
	Vec3 viewOffset(g_pGameCVars->goc_targetx, g_pGameCVars->goc_targety, g_pGameCVars->goc_targetz);
	const SPlayerStats& stats = m_player.m_stats;
	const EStance stance = m_player.GetStance();
	const bool freeFall = stats.inFreefall.Value() == 1;
	//const bool parachute = m_player.GetPlayerStats() ? m_player.GetPlayerStats()->inFreefall == 2 : false;
	const bool inFreeFalllOrPara = (stats.inFreefall.Value() > 0 || m_player.IsParachuteActive());

	float distanceOffsetPara = -8.f;

	if (inFreeFalllOrPara)
	{
		viewOffset.x = 0.0f;
		viewOffset.y = distanceOffsetPara;
		viewOffset.z = 1.0f;
	}
	else if (stats.isRagDoll)
	{
		viewOffset.x = 0.0f;
	}

	CWeapon* pWeapon = m_player.GetCurrentWeapon(false);
	if (pWeapon)
	{
		if (pWeapon->IsModifyingNoTS())
		{
			viewParams.rotation *= Quat::CreateRotationX(-0.8f);

			viewOffset.x = 0.3f;
			viewOffset.y = -0.3f;
			viewOffset.z = 0.6f;
		}
	}

	Vec3 offsetX = m_viewQuatFinal.GetColumn0() * viewOffset.x;
	Vec3 offsetY = m_viewQuatFinal.GetColumn1() * viewOffset.y;
	Vec3 offsetZ = m_viewQuatFinal.GetColumn2() * viewOffset.z;

	float noLean = 0.0f;
	Vec3 origin = m_player.GetEntity()->GetWorldPos() + m_baseQuat * m_player.GetStanceViewOffset(m_player.GetStance(), &noLean);
	Vec3 transformPos = origin + offsetX + offsetY + offsetZ;
	Vec3 occRay = transformPos - origin;

	const float thinRadius = 0.1f;
	const float thickRadius = 0.5;

	const Vec3 forward = viewParams.rotation.GetColumn1();

	CActor* pActor = static_cast<CActor*>(&m_player);
	const Vec3 colPoint = GetCollision(pActor, transformPos, forward, origin, thinRadius, true);
	const Vec3 colPointThick = GetCollision(pActor, transformPos, forward, origin, thickRadius, false);

	const Vec3 projection = Project((colPointThick - origin), occRay.normalized());
	const Vec3 colPointThickProjectedOnRay = projection + origin;
	const Vec3 vecToProjected = (colPointThickProjectedOnRay - colPointThick).normalized();
	const Vec3 colPointThickProjectedOnThinCapsule = colPointThickProjectedOnRay - vecToProjected * thinRadius;

	const auto thin2ThickDist = MIN(0.15, colPointThickProjectedOnThinCapsule.GetDistance(colPointThick)); 
	const auto thin2ThickDistNorm = thin2ThickDist / (thickRadius - thinRadius); 
	auto currentColDist = origin.GetDistance(colPoint);
	const auto currentColDistThick = origin.GetDistance(colPointThickProjectedOnRay);

	currentColDist = LERP(currentColDistThick, currentColDist, thin2ThickDistNorm);

	const auto previous = m_ColDistance;

	const float deltaTime = gEnv->pTimer->GetFrameTime();

	const float speedSmooth = 8.0f;
	const float speedFast = 15.0f;
	const float speedSuperFast = 22.0f;

	if (m_fastCameraCorrectionMode > 0.0f) //If we switch view or move mouse quickly, we want fast corrections to avoid clipping
	{
		Interpolate(m_ColDistance, currentColDist, speedSuperFast, deltaTime);
		m_fastCameraCorrectionMode -= deltaTime;
	}
	else if (currentColDist < previous)
	{
		Interpolate(m_ColDistance, currentColDist, speedFast, deltaTime);
	}
	else
	{
		Interpolate(m_ColDistance, currentColDist, speedSmooth, deltaTime);
	}

	IEntityRenderProxy* pRenderProxy = static_cast<IEntityRenderProxy*>(pActor->GetEntity()->GetProxy(ENTITY_PROXY_RENDER));
	if (pRenderProxy)
	{
		if (m_ColDistance < 1.0f)
		{
			m_player.m_targetOpacity = std::clamp(m_player.m_targetOpacity, 0.0f, 0.3f); //0.3 -> 1.0 flickers too much
		}
		else
		{
			m_player.m_targetOpacity = m_player.m_smoothedOpacity = 1.0f;
		}

		Interpolate(m_player.m_smoothedOpacity, m_player.m_targetOpacity, 5.f, deltaTime);

		pRenderProxy->SetOpacity(m_player.m_smoothedOpacity);
	}

	Vec3 customOrigin = origin;
	if (!freeFall)
	{
		if (stance == EStance::STANCE_CROUCH || stance == EStance::STANCE_PRONE)
		{
			customOrigin.z -= 0.6f;
		}
		else
		{
			customOrigin.z -= 1.6f;
		}
	}

	const Vec3 customCamPos = customOrigin + offsetX + offsetY + offsetZ;
	const Vec3 ray = customCamPos - customOrigin;
	const float safeBuffer = 0.2f;
	const float minDistance = 0.2f;
	const float maxDistance = inFreeFalllOrPara ? std::fabs(distanceOffsetPara) : 4.0f;

	m_ColDistance = std::max(minDistance, std::min(m_ColDistance, maxDistance));

	viewParams.position = customOrigin + ray.normalized() * (m_ColDistance - safeBuffer);
}

// jump/land spring effect. Adjust the eye and weapon pos as required.
void PlayerView::FirstPersonJump(SViewParams& viewParams, Vec3& weaponOffset, Ang3& weaponAngleOffset)
{
	const SPlayerStats& stats = m_player.m_stats;
	if (stats.onGround)
	{
		if (m_landed)
		{
			m_eyeOffsetViewGoal.z -= (m_player.GetStance() == STANCE_PRONE) ? 0.1f : 0.3f;
			weaponOffset -= m_baseQuat.GetColumn2() * 0.125f;
			weaponAngleOffset.x += 3.0f;
		}

		if (stats.onGround > 0.1f)
			m_landed = false;
	}
}

//
// Extract the various modifiers out into their own function.
//
// Aim is to make the code easier to understand and modify, as 
// well as to ease the addition of new modifiers.
//
void PlayerView::ViewFirstPerson(SViewParams& viewParams)
{
	const SPlayerStats& stats = m_player.m_stats;
	//headbob
	Ang3 angOffset(0, 0, 0);
	Vec3 weaponOffset(0, 0, 0);
	Ang3 weaponAngleOffset(0, 0, 0);

	// jump/land spring effect. Adjust the eye and weapon pos as required.
	FirstPersonJump(viewParams, weaponOffset, weaponAngleOffset);

	//float standSpeed(GetStanceMaxSpeed(STANCE_STAND));

	Vec3 vSpeed(0, 0, 0);
	if (m_standSpeed > 0.001f)
		vSpeed = (stats.velocity / m_standSpeed);

	float vSpeedLen(vSpeed.len());
	if (vSpeedLen > 1.5f)
		vSpeed = vSpeed / vSpeedLen * 1.5f;

	float speedMul(0);
	if (m_standSpeed > 0.001f)
		speedMul = (stats.speedFlat / m_standSpeed * 1.1f);

	speedMul = min(1.5f, speedMul);

	bool crawling(m_player.GetStance() == STANCE_PRONE /*&& stats.speedFlat>0.1f*/ && stats.onGround > 0.1f);
	bool weaponZoomed = false;
	bool weaponZooming = false;

	//Not crawling while in zoom mode
	CWeapon* pWeapon = m_player.GetCurrentWeapon(false);
	if (pWeapon)
	{
		weaponZoomed = pWeapon->IsZoomed();
		weaponZooming = pWeapon->IsZooming();
		if (weaponZoomed || weaponZooming || pWeapon->IsModifying())
			crawling = false;
	}
	
	// On the ground.
	if (stats.inAir < 0.1f /*&& stats.inWaterTimer < 0.1f*/)
	{
		//--- Bobbing.
		// bobCycle is a speed varying time step running (looping) from 0 to 1 
		// this feeds into a sin eqn creating a double horizontal figure of 8.
		// ( a lissajous figure with the vertical freq twice the horz freq ).

		// To tweak the total speed of the curve:

		// To tweak the effect speed has on the curve:
		float kSpeedToBobFactor = 1.15f;//0.9f
		// To tweak the width of the bob:
		float kBobWidth = 0.1f;
		// To tweak the height of the bob:
		float kBobHeight = 0.05f;
		// To tweak the scale of strafing lag: (may need to manually adjust the strafing angle offsets as well.)
		const float kStrafeHorzScale = 0.05f;

		kBobWidth = 0.15f;
		kBobHeight = 0.06f;

		m_bobCycle += m_frameTime * kSpeedToBobFactor * speedMul;// * (stats.bSprinting?1.25f:1.0f);

		//if player is standing set the bob to rest. (bobCycle reaches 1.0f within 1 second)
		if (speedMul < 0.1f)
			m_bobCycle = min(m_bobCycle + m_frameTime * 1.0f, 1.0f);

		// bobCycle loops between 0 and 1
		if (m_bobCycle > 1.0f)
			m_bobCycle = m_bobCycle - 1.0f;

		if (crawling)
			kBobWidth *= 2.0f * speedMul;
		else if (stats.bSprinting)
			kBobWidth *= 1.25f * speedMul;

		//set the bob offset
		Vec3 bobDir(cry_sinf(m_bobCycle * gf_PI * 2.0f) * kBobWidth * speedMul, 0, cry_sinf(m_bobCycle * gf_PI * 4.0f) * kBobHeight * speedMul);

		//not the bob offset for the weapon
		bobDir *= 0.25f;
		//if player is strafing shift a bit the weapon on left/right
		if (speedMul > 0.01f)
		{
			// right vector dot speed vector
			float dot(m_viewQuatFinal.GetColumn0() * vSpeed);

			bobDir.x -= dot * kStrafeHorzScale;	// the faster we move right, the more the gun lags to the left and vice versa

			//tweak the right strafe for weapon laser
			if (dot > 0.0f)
				weaponAngleOffset.z += dot * 1.5f;	// kStrafeHorzScale
			else
				weaponAngleOffset.z -= dot * 2.0f;	// kStrafeHorzScale

			weaponAngleOffset.y += dot * 5.0f;		// kStrafeHorzScale
		}
		//CryLogAlways("bobDir.z: %f", bobDir.z);
		if (bobDir.z < 0.0f)
		{
			bobDir.x *= 1.0f;
			bobDir.y *= 1.0f;
			bobDir.z *= 0.35f;
			speedMul *= 0.65f;
		}
		else
			bobDir.z *= 1.85f;

		//CryLogAlways("bobDir.z: %f after", bobDir.z);
		weaponOffset += m_viewQuatFinal * bobDir;
		weaponOffset -= m_baseQuat.GetColumn2() * 0.035f * speedMul;

		weaponAngleOffset.y += cry_sinf(m_bobCycle * gf_PI * 2.0f) * speedMul * -1.5f;
		if (crawling)
			weaponAngleOffset.y *= 3.0f;

		weaponAngleOffset.x += speedMul * 1.5f;
		if (crawling)
			weaponAngleOffset.z += cry_sinf(m_bobCycle * gf_PI * 2.0f) * speedMul * 3.0f;

		//FIXME: viewAngles must include all the view offsets, otherwise aiming wont be precise.
		angOffset.x += cry_sinf(m_bobCycle * gf_PI * 4.0f) * 0.7f * speedMul;

		if (crawling)
		{
			angOffset.x *= 2.5f;
			angOffset.y += cry_sinf(m_bobCycle * gf_PI * 2.0f) * 1.25f * speedMul;
			angOffset.z -= cry_sinf(m_bobCycle * gf_PI * 2.0f) * 2.5f * speedMul;

		}
		else if (stats.bSprinting)
		{
			angOffset.x *= 2.5f;
			angOffset.y += cry_sinf(m_bobCycle * gf_PI * 2.0f) * 1.0f * speedMul;
			angOffset.z -= cry_sinf(m_bobCycle * gf_PI * 2.0f) * 2.25f * speedMul;
		}
		else if (m_player.GetStance() == STANCE_CROUCH && !weaponZoomed && !weaponZooming)
		{
			weaponOffset.z += 0.035f;
			weaponOffset.y -= m_viewQuatFinal.GetColumn1().y * 0.03f;
		}
		else if (m_player.GetStance() == STANCE_CROUCH && weaponZooming)
		{
			weaponOffset.z -= 0.07f;
			weaponOffset.y += m_viewQuatFinal.GetColumn1().y * 0.06f;
		}
		else
		{
			//angOffset.x *= 2.25f;
			//angOffset.y += cry_sinf(m_bobCycle*gf_PI*2.0f)*0.5f*speedMul;
			//angOffset.z -= cry_sinf(m_bobCycle*gf_PI*2.0f)*1.125f*speedMul;
		}
	}
	else
	{
		m_bobCycle = 0;

		//while flying offset a bit the weapon model by the player speed	
		if (stats.velocity.len2() > 0.001f)
		{
			float dotFwd(m_viewQuatFinal.GetColumn1() * vSpeed);
			float dotSide(m_viewQuatFinal.GetColumn0() * vSpeed);
			float dotUp(m_viewQuatFinal.GetColumn2() * vSpeed);

			weaponOffset += m_viewQuatFinal * Vec3(dotSide * -0.05f, dotFwd * -0.035f, dotUp * -0.05f);

			weaponAngleOffset.x += dotUp * 2.0f;
			weaponAngleOffset.y += dotSide * 5.0f;
			weaponAngleOffset.z -= dotSide * 2.0f;
		}
	}

	//add some inertia to weapon due view direction change.
	float deltaDotSide(m_vFPWeaponLastDirVec * m_viewQuatFinal.GetColumn0());
	float deltaDotUp(m_vFPWeaponLastDirVec * m_viewQuatFinal.GetColumn2());

	weaponOffset += m_viewQuatFinal * Vec3(deltaDotSide * 0.1f + stats.leanAmount * 0.05f, 0, deltaDotUp * 0.1f - fabs(stats.leanAmount) * 0.05f) * m_player.m_params.weaponInertiaMultiplier;

	weaponAngleOffset.x -= deltaDotUp * 5.0f * m_player.m_params.weaponInertiaMultiplier;
	weaponAngleOffset.z += deltaDotSide * 5.0f * m_player.m_params.weaponInertiaMultiplier;
	weaponAngleOffset.y += deltaDotSide * 5.0f * m_player.m_params.weaponInertiaMultiplier;

	if (stats.leanAmount < 0.0f)
		weaponAngleOffset.y += stats.leanAmount * 5.0f;

	//the weapon model tries to stay parallel to the terrain when the player is freefalling/parachuting

	if (stats.inWaterTimer > 0.0f)
		weaponOffset -= m_viewQuat.GetColumn2() * 0.15f;

	if (stats.inWaterTimer > 0.1f && !(stats.headUnderWaterTimer > 0.0f))
	{
		Ang3 offset(m_viewQuatFinal);
		offset.z = 0;
		if (offset.x < 0.0f)
			offset.x = 0;

		weaponAngleOffset -= offset * (180.0f / gf_PI) * 0.75f;
	}
	else if (stats.inFreefall.Value())
	{
		Ang3 offset(m_viewQuatFinal);
		offset.z = 0;

		weaponAngleOffset -= offset * (180.0f / gf_PI) * 0.5f;
	}
	//same thing with crawling
	else if (crawling)
	{
		//FIXME:to optimize, looks like a bit too expensive
		Vec3 forward(m_viewQuatFinal.GetColumn1());
		Vec3 up(m_baseQuat.GetColumn2());
		Vec3 right(-(up % forward));

		Matrix33 mat;
		mat.SetFromVectors(right, up % right, up);
		mat.OrthonormalizeFast();

		Ang3 offset(m_viewQuatFinal.GetInverted() * Quat(mat));

		weaponAngleOffset += offset * (180.0f / gf_PI) * 0.5f;

		float lookDown(m_viewQuatFinal.GetColumn1() * m_baseQuat.GetColumn2());
		weaponOffset += m_baseQuat * Vec3(0, -0.5f * max(-lookDown, 0.0f), -0.05f);

		float scale = 0.5f;
		if (weaponAngleOffset.x > 0.0f)
		{
			scale = min(0.5f, weaponAngleOffset.x / 15.0f);
			weaponAngleOffset.x *= scale;
		}
		else
		{
			scale = min(0.5f, -weaponAngleOffset.x / 20.0f);
			weaponAngleOffset *= (1.0f - scale);
			weaponOffset *= scale;
		}
		//if(vSpeedLen>0.1f)
			//weaponAngleOffset += Ang3(-8.0f,0,-12.5f);

	}
	else if (stats.bSprinting && vSpeedLen > 0.5f)
	{
		weaponAngleOffset += Ang3(-20.0f, 0, 10.0f);
		weaponOffset += m_viewQuatFinal * Vec3(0.0f, -.01f, .1f);
	}
	else if (stats.bLookingAtFriendlyAI && !weaponZooming && !weaponZoomed)
	{
		weaponAngleOffset += Ang3(-15.0f, 0, 8.0f);
		weaponOffset += m_viewQuatFinal * Vec3(0.0f, -.01f, .05f);
	}

	//apply some multipliers
	weaponOffset *= m_player.m_params.weaponBobbingMultiplier;
	angOffset *= m_bobMul * 0.25f;
	if (m_bobMul * m_bobMul != 1.0f)
	{
		weaponOffset *= m_bobMul;
		weaponAngleOffset *= m_bobMul;
	}

	float bobSpeedMult(1.0f);
	if (stats.inWaterTimer > 0.1)
	{
		bobSpeedMult = 0.75f;
	}
	
	Interpolate(m_vFPWeaponOffset, weaponOffset, 3.95f * bobSpeedMult, m_frameTime);
	Interpolate(m_vFPWeaponAngleOffset, weaponAngleOffset, 10.0f * bobSpeedMult, m_frameTime);
	Interpolate(m_vFPWeaponLastDirVec, m_viewQuatFinal.GetColumn1(), 5.0f * bobSpeedMult, m_frameTime);

	Interpolate(m_angleOffset, angOffset, 10.0f, m_frameTime, 0.002f);
	if (weaponZooming)
	{
		m_vFPWeaponLastDirVec = m_viewQuatFinal.GetColumn1();
		m_vFPWeaponOffset.Set(0.0f, 0.0f, 0.0f);
		m_vFPWeaponAngleOffset.Set(0.0f, 0.0f, 0.0f);
		m_bobOffset.Set(0.0f, 0.0f, 0.0f);
	}

	if (stats.bSprinting)
	{
		float headBobScale = (stats.speedFlat / m_standSpeed);
		headBobScale = min(1.0f, headBobScale);

		m_bobOffset = m_vFPWeaponOffset * 2.5f * g_pGameCVars->cl_headBob * headBobScale;
		float bobLenSq = m_bobOffset.GetLengthSquared();
		float bobLenLimit = g_pGameCVars->cl_headBobLimit;
		if (bobLenSq > bobLenLimit * bobLenLimit)
		{
			float bobLen = sqrt_tpl(bobLenSq);
			m_bobOffset *= bobLenLimit / bobLen;
		}
		viewParams.position += m_bobOffset;
	}
}

void PlayerView::ViewVehicle(SViewParams& viewParams)
{
	const SPlayerStats& stats = m_player.m_stats;
	if (m_pVehicle)
	{
		if (m_player.IsFpSpectatorTarget())
		{
			if (IVehicleSeat* pSeat = m_pVehicle->GetSeatForPassenger(m_playerId))
			{
				const int currSeatViewId = 1;

				//CryMP: Check if player is switching seat
				if (m_currentViewUpdateSeatId != pSeat->GetSeatId())
				{	
					StopVehicleViewUpdates();

					IVehicleView* pNewView = pSeat->GetView(currSeatViewId);
					if (pNewView)
					{
						pNewView->OnStartUsing(m_player.GetEntityId());

						m_currentViewUpdateSeatId = pSeat->GetSeatId();
						m_currentViewUpdateVehicleId = m_pVehicle->GetEntityId();
					}
					if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
					{
						SVehicleMovementEventParams params;
						params.fValue = 1.0f;
						pMovement->OnEvent(IVehicleMovement::eVME_PlayerSwitchView, params);
					}
				}
				if (IVehicleView* pView = m_currentViewUpdateSeatId ? pSeat->GetView(currSeatViewId) : nullptr)
				{
					//CryMP: This probably needs a fix in CryAction
					//so that we see the view direction changes in non driver/gunner seats
					pView->UpdateView(viewParams, m_playerId); 

					//viewParams.viewID = 2;
				}
				else
				{
					ViewFollowCharacterFirstPerson(viewParams);
				}
			}
		}
		else
		{
			// CryMP: Reverse View
			if (m_vehicleReverseView && stats.isThirdPerson && !m_pVehicle->GetCurrentWeaponId(m_playerId))
			{
				AABB vehicleBox;
				m_pVehicle->GetEntity()->GetLocalBounds(vehicleBox);

				const float distance = 3.0f + vehicleBox.GetRadius();
				const Matrix34 worldTM = m_pVehicle->GetEntity()->GetWorldTM();
				const Vec3 forwardDir = worldTM.GetColumn1().normalized();
				const Vec3 revdir = forwardDir * -1;
				const Matrix33 rotation = Matrix33::CreateRotationVDir(revdir);

				Vec3 worldPos = worldTM.GetTranslation();
				worldPos.z += 2.5f;

				const Vec3 newPos = worldPos + forwardDir * distance;

				viewParams.position = newPos;
				viewParams.rotation = GetQuatFromMat33(rotation);
			}
			else
			{
				m_pVehicle->UpdateView(viewParams, m_playerId);
				viewParams.viewID = 2;
			}
		}
	}
}

void PlayerView::StopVehicleViewUpdates()
{
	if (!m_currentViewUpdateVehicleId || !m_currentViewUpdateSeatId)
	{
		return;
	}
	IVehicle *pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(m_currentViewUpdateVehicleId);
	if (pVehicle)
	{
		if (IVehicleSeat* pPreviousSeat = pVehicle->GetSeatById(m_currentViewUpdateSeatId))
		{
			if (IVehicleView* pView = pPreviousSeat->GetView(1))
			{
				//CryMP: Called on the FP spectator target whenever client stops spectating
				//or target changes seat/leaves vehicle

				pView->OnStopUsing();
				
				//CryLogAlways("$3Stopping view update for vehicle: %s seat %s", pVehicle->GetEntity()->GetName(),pPreviousSeat->GetSeatName());
			}
		}
	}
	m_currentViewUpdateVehicleId = 0;
	m_currentViewUpdateSeatId = 0;
}

void PlayerView::ViewSpectatorTarget(SViewParams& viewParams)
{
	const SPlayerStats& stats = m_player.m_stats;
	CActor* pTarget = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(stats.spectatorTarget));
	if (!pTarget)
		return;

	IVehicle* pVehicle = pTarget->GetLinkedVehicle();

	static float defaultOffset = 0.3f;
	static float viewHeight = 1.8f;

	Matrix34 worldTM = pTarget->GetEntity()->GetWorldTM();
	Vec3 worldPos = worldTM.GetTranslation();
	if (!pVehicle)
	{
		const SStanceInfo* stanceInfo = pTarget->GetStanceInfo(pTarget->GetStance());
		if (stanceInfo)
		{
			Interpolate(viewHeight, stanceInfo->viewOffset.z, 5.0f, viewParams.frameTime);
			worldPos.z += viewHeight + defaultOffset;
		}
		else
		{
			worldPos.z += 1.8f;
		}
	}
	else
	{
		// use vehicle pos/ori
		worldTM = pVehicle->GetEntity()->GetWorldTM();
		worldPos = pVehicle->GetEntity()->GetWorldPos();
		worldPos.z += 1.5f;
	}

	Ang3 worldAngles = Ang3::GetAnglesXYZ(Matrix33(worldTM));
	float distance = 3;

	// if freelook allowed, get orientation and distance from player entity
	if (g_pGameCVars->g_spectate_FixedOrientation == 0)
	{
		Matrix34 ownOrientation = m_player.GetEntity()->GetWorldTM();

		worldAngles += Ang3::GetAnglesXYZ(Matrix33(ownOrientation));
		distance = m_player.GetSpectatorZoom();
	}

	if (pVehicle)
	{
		distance *= 4.0f;

		// air vehicles need bigger distance
		if (pVehicle->GetMovement() && pVehicle->GetMovement()->GetMovementType() == IVehicleMovement::eVMT_Air)
			distance *= 2.0f;
	}

	Vec3 goal;
	goal.x = distance * cos(worldAngles.z + gf_PI * 1.5f) + worldPos.x;
	goal.y = distance * sin(worldAngles.z - gf_PI / 2.0f) + worldPos.y;

	AABB targetBounds;
	pTarget->GetEntity()->GetLocalBounds(targetBounds);
	goal.z = targetBounds.max.z;
	float offset = defaultOffset;
	if (pVehicle)
	{
		if (pVehicle->GetMovement() && pVehicle->GetMovement()->GetMovementType() == IVehicleMovement::eVMT_Air)
			offset = 3.0f;
		else
			offset = 1.0f;
	}
	goal.z += pTarget->GetEntity()->GetWorldPos().z + offset;

	// store / interpolate the offset, not the world pos (reduces percieved jitter in vehicles)
	static Vec3 viewOffset(goal - worldPos);
	static Vec3 camPos(goal);
	static Vec3 entPos(worldPos);
	static EntityId lastSpectatorTarget(stats.spectatorTarget);

	// do a ray cast to check for camera intersection
	static ray_hit hit;
	IPhysicalEntity* pSkipEntities[10];
	int nSkip = 0;
	if (pVehicle)
	{
		// vehicle drivers don't seem to have current items, so need to add the vehicle itself here
		nSkip = pVehicle->GetSkipEntities(pSkipEntities, 10);
	}
	else
	{
		IItem* pItem = pTarget->GetCurrentItem();
		if (pItem)
		{
			CWeapon* pWeapon = (CWeapon*)pItem->GetIWeapon();
			if (pWeapon)
				nSkip = CSingle::GetSkipEntities(pWeapon, pSkipEntities, 10);
		}
	}

	static float minDist = 0.4f;	// how close we're allowed to get to the target
	static float wallSafeDistance = 0.3f; // how far to keep camera from walls

	Vec3 dir = goal - worldPos;
	primitives::sphere sphere;
	sphere.center = worldPos;
	sphere.r = wallSafeDistance;

	geom_contact* pContact = 0;
	float hitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, dir, ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid,
		&pContact, 0, (geom_colltype_player << rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEntities, nSkip);

	// even when we have contact, keep the camera the same height above the target
	float minHeightDiff = dir.z;

	if (hitDist > 0 && pContact)
	{
		goal = worldPos + (hitDist * dir.GetNormalizedSafe());

		if (goal.z - worldPos.z < minHeightDiff)
		{
			// can't move the camera far enough away from the player in this direction. Try moving it directly up a bit
			int numHits = 0;
			sphere.center = goal;

			// (move back just slightly to avoid colliding with the wall we've already found...)
			sphere.center -= dir.GetNormalizedSafe() * 0.05f;

			float newHitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, Vec3(0, 0, minHeightDiff), ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid,
				&pContact, 0, (geom_colltype_player << rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEntities, nSkip);

			float raiseDist = minHeightDiff - (goal.z - worldPos.z) - wallSafeDistance;
			if (newHitDist != 0)
			{
				raiseDist = MIN(minHeightDiff, newHitDist);
			}

			raiseDist = MAX(0.0f, raiseDist);

			goal.z += raiseDist;
			worldPos.z += raiseDist * 0.8f;
		}
	}

	int thisFrameId = gEnv->pRenderer->GetFrameID();
	static int frameNo(thisFrameId);
	if (thisFrameId - frameNo > 5)
	{
		// reset positions
		viewOffset = goal - worldPos;
		entPos = worldPos;
		camPos = goal;
	}
	if (lastSpectatorTarget != stats.spectatorTarget)
	{
		viewOffset = goal - worldPos;
		entPos = worldPos;
		camPos = goal;
		lastSpectatorTarget = stats.spectatorTarget;
	}
	frameNo = thisFrameId;

	static float interpSpeed = 5.0f;
	static float interpSpeed2 = 5.0f;
	static float interpSpeed3 = 8.0f;

	if (pVehicle)
	{
		Interpolate(viewOffset, goal - worldPos, interpSpeed, viewParams.frameTime);
		entPos = worldPos;
		viewParams.position = worldPos + viewOffset;
		camPos = viewParams.position;
	}
	else
	{
		Vec3 camPosChange = goal - camPos;
		Vec3 entPosChange = worldPos - entPos;

		if (camPosChange.GetLengthSquared() > 100.0f)
			camPos = goal;
		if (entPosChange.GetLengthSquared() > 100.0f)
			entPos = worldPos;

		Interpolate(camPos, goal, interpSpeed2, viewParams.frameTime);
		Interpolate(entPos, worldPos, interpSpeed3, viewParams.frameTime);
		viewParams.position = camPos;
	}

	Matrix33 rotation = Matrix33::CreateRotationVDir((entPos - viewParams.position).GetNormalizedSafe());
	viewParams.rotation = GetQuatFromMat33(rotation);
	m_bUsePivot = true;
	m_bobCycle = 0.0;
}

void PlayerView::ViewSpectatorTarget_CryMP(SViewParams& viewParams)
{
	const SPlayerStats& stats = m_player.m_stats;
	CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(stats.spectatorTarget));
	if (!pActor)
		return;

	IEntity* pEntity = pActor->GetEntity();
	Matrix34 worldTM = pEntity->GetWorldTM();

	Vec3 worldPos = worldTM.GetTranslation();
	worldPos.z += 1.5f;

	Ang3 worldAngles = Ang3::GetAnglesXYZ(Matrix33(worldTM));

	const float defaultOffset = 0.75f;
	float distance = 3.0f;
	float offset = defaultOffset;
	if (pActor)
	{
		if (pActor->GetLinkedVehicle())
			offset = 2.0f;
		else
		{
			SActorStats* pStats = pActor->GetActorStats();
			if (pStats)
			{
				if (pStats->inFreefall.Value() == 2)
				{
					offset -= 2.0f;
					distance += 5.0f;
				}
				else if (pStats->inFreefall.Value() == 1 || pStats->inFreefall.Value() == 3)
				{
					offset = 10.0f;
					worldPos.z -= 10.f;
				}
			}
		}
	}

	float rot = worldAngles.z;// + m_rot;

	if (IVehicle* pVehicle = pActor->GetLinkedVehicle())
	{
		if (pVehicle->GetMovement() && pVehicle->GetMovement()->GetMovementType() == IVehicleMovement::eVMT_Air)
		{
			offset = 5.0f;
			distance = 30.f;
		}
		else
		{
			AABB vehicleBox;
			pVehicle->GetEntity()->GetLocalBounds(vehicleBox);
			distance = 2.0f * vehicleBox.GetRadius();
		}
	}

	//Zooming ability
	distance += (m_player.GetSpectatorZoom() - 1);

	Vec3 goal;
	float zoom = 1.0f;
	goal.x = distance * zoom * cosf(rot + gf_PI * 1.5f) + worldPos.x;
	goal.y = distance * zoom * sinf(rot - gf_PI / 2.0f) + worldPos.y;

	AABB targetBounds;
	pEntity->GetLocalBounds(targetBounds);
	goal.z = targetBounds.max.z;
	goal.z += pEntity->GetWorldPos().z + offset;

	// store / interpolate the offset, not the world pos (reduces percieved jitter in vehicles)
	static Vec3 viewOffset(goal - worldPos);
	static Vec3 position(goal);
	static Vec3 entPos(worldPos);
	static EntityId lastSpectatorTarget(stats.spectatorTarget);

	// do a ray cast to check for camera intersection
	static ray_hit hit;
	IPhysicalEntity* pSkipEntities[10];
	int nSkip = 0;
	if (pActor)
	{
		CWeapon* pWeapon = pActor->GetCurrentWeapon(false);
		if (pWeapon)
		{
			nSkip = CSingle::GetSkipEntities(pWeapon, pSkipEntities, 10);
		}
		else if (IVehicle* pVehicle = pActor->GetLinkedVehicle())
		{
			// vehicle drivers don't seem to have current items, so need to add the vehicle itself here
			nSkip = pVehicle->GetSkipEntities(pSkipEntities, 10);
		}
	}

	const float wallSafeDistance = 0.2f; // how far to keep camera from walls

	Vec3 dir = goal - worldPos;

	primitives::sphere sphere;
	sphere.center = worldPos;
	sphere.r = wallSafeDistance;

	geom_contact* pContact = 0;
	float hitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, dir, ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid,
		&pContact, 0, (geom_colltype_player << rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEntities, nSkip);

	// even when we have contact, keep the camera the same height above the target
	float minHeightDiff = dir.z;

	if (hitDist > 0 && pContact)
	{
		goal = worldPos + (hitDist * dir.GetNormalizedSafe());

		if (goal.z - worldPos.z < minHeightDiff)
		{
			// can't move the camera far enough away from the player in this direction. Try moving it directly up a bit
			sphere.center = goal;

			// (move back just slightly to avoid colliding with the wall we've already found...)
			sphere.center -= dir.GetNormalizedSafe() * 0.05f;

			float newHitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, Vec3(0, 0, minHeightDiff), ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid,
				&pContact, 0, (geom_colltype_player << rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEntities, nSkip);

			float raiseDist = minHeightDiff - (goal.z - worldPos.z) - wallSafeDistance;
			if (newHitDist != 0)
			{
				raiseDist = MIN(minHeightDiff, newHitDist);
			}

			raiseDist = MAX(0.0f, raiseDist);

			goal.z += raiseDist;
			worldPos.z += raiseDist * 0.8f;
		}
	}

	int thisFrameId = gEnv->pRenderer->GetFrameID();
	static int frameNo(thisFrameId);
	if (thisFrameId - frameNo > 5)
	{
		// reset positions
		viewOffset = goal - worldPos;
		entPos = worldPos;
		position = goal;
	}
	if (lastSpectatorTarget != stats.spectatorTarget)
	{
		viewOffset = goal - worldPos;
		entPos = worldPos;
		position = goal;
		lastSpectatorTarget = stats.spectatorTarget;
	}
	frameNo = thisFrameId;
	if (pActor && pActor->GetLinkedVehicle())
	{
		Interpolate(viewOffset, goal - worldPos, 5.0f, viewParams.frameTime);
		entPos = worldPos;
		viewParams.position = worldPos + viewOffset;
		position = viewParams.position;
	}
	else
	{
		Vec3 camPosChange = goal - position;
		Vec3 entPosChange = worldPos - entPos;

		if (camPosChange.GetLengthSquared() > 100.0f)
			position = goal;
		if (entPosChange.GetLengthSquared() > 100.0f)
			entPos = worldPos;

		Interpolate(position, goal, 5.0f, viewParams.frameTime);
		Interpolate(entPos, worldPos, 5.0f, viewParams.frameTime);
		viewParams.position = position;
	}

	Matrix33 rotation = Matrix33::CreateRotationVDir((entPos - viewParams.position).GetNormalizedSafe());

	viewParams.rotation = GetQuatFromMat33(rotation);
	m_bUsePivot = true;
	m_bobCycle = 0.0;
}

void PlayerView::ViewDeathCamTarget(SViewParams& viewParams)
{
	const SPlayerStats& stats = m_player.m_stats;
	CActor* pTarget = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(stats.deathCamTarget));
	if (!pTarget)
		return;

	Matrix34 targetWorldTM = pTarget->GetEntity()->GetWorldTM();

	Vec3 camPos = viewParams.position;
	static float offset = 1.5f;
	camPos.z += offset;

	float heightOffset = 1.5f;
	const SStanceInfo* pSI = pTarget->GetStanceInfo(pTarget->GetStance());
	if (pSI)
	{
		heightOffset = pSI->viewOffset.z;
	}

	Vec3 targetPos = targetWorldTM.GetTranslation();
	targetPos.z += heightOffset;

	int thisFrameId = gEnv->pRenderer->GetFrameID();
	static int frameNo(thisFrameId);
	static Vec3 oldCamPos(camPos);
	static Vec3 oldTargetPos(targetPos);
	static EntityId lastSpectatorTarget(stats.spectatorTarget);
	static float oldFOVScale(1.0f);

	// if more than a few frames have passed since our last update, invalidate the positions
	if (thisFrameId - frameNo > 5)
	{
		oldCamPos = viewParams.position;	// interpolate from current camera pos
		oldTargetPos = targetPos;
		oldFOVScale = 1.0f;
	}
	// if target changed, reset positions
	if (lastSpectatorTarget != stats.spectatorTarget)
	{
		oldCamPos = camPos;
		oldTargetPos = targetPos;
		lastSpectatorTarget = stats.spectatorTarget;
		oldFOVScale = 1.0f;
	}
	frameNo = thisFrameId;

	// slight zoom after 2s
	float timeNow = gEnv->pTimer->GetCurrTime();
	float distSq = (targetPos - camPos).GetLengthSquared();
	float scale = 1.0f;
	if (timeNow - m_player.GetDeathTime() > 1.0f && distSq > 2500.0f)
	{
		// 1.0f at 50m, 0.3f at 100m+
		scale = 1.0f - (distSq - 2500.0f) / 25000.0f;
		scale = CLAMP(scale, 0.3f, 1.0f);
	}

	Interpolate(oldCamPos, camPos, 5.0f, viewParams.frameTime);
	Interpolate(oldTargetPos, targetPos, 5.0f, viewParams.frameTime);
	Interpolate(oldFOVScale, scale, 0.5f, viewParams.frameTime);

	viewParams.position = oldCamPos;
	Vec3 dir = (oldTargetPos - oldCamPos).GetNormalizedSafe();
	Matrix33 rotation = Matrix33::CreateRotationVDir(dir);
	dir.z = 0.0f;

	// quick ray check to make sure there's not a wall in the way...

	static ray_hit hit;
	IPhysicalEntity* pSkipEntities[10];
	int nSkip = 0;

	CWeapon* pWeapon = m_player.GetCurrentWeapon(false);
	if (pWeapon)
	{
		nSkip = CSingle::GetSkipEntities(pWeapon, pSkipEntities, 10);
	}

	if (gEnv->pPhysicalWorld->RayWorldIntersection(viewParams.position, -dir, ent_static | ent_terrain | ent_rigid,
		rwi_ignore_noncolliding | rwi_stop_at_pierceable, &hit, 1, pSkipEntities, nSkip))
	{
		dir.zero();
	}

	viewParams.position -= dir;

	viewParams.fov = m_defaultFov * oldFOVScale * (gf_PI / 180.0f);
	viewParams.rotation = GetQuatFromMat33(rotation);
	m_bUsePivot = true;
	m_bobCycle = 0.0;
}

//FIXME:use the animated character view filter for this
void PlayerView::ViewFollowCharacterFirstPerson(SViewParams& viewParams)
{
	const SPlayerStats& stats = m_player.m_stats;

	if (!m_player.IsFpSpectatorTarget())
		viewParams.viewID = 3;

	//to avoid clipping
	viewParams.nearplane = 0.1f;
	viewParams.position = m_entityWorldMatrix * m_localEyePos;

	if (stats.isRagDoll)
		viewParams.position.z += 0.05f;

	if (stats.followCharacterHead.Value())
		viewParams.rotation = Quat(Matrix33(m_entityWorldMatrix) * m_headMtxLocal * Matrix33::CreateRotationY(gf_PI * 0.5f));
}

// View was set from an external source such as a vehicle, or followCharacterHead, so update the players rotation to match.
void PlayerView::ViewExternalControlPostProcess(SViewParams& viewParams)
{
	const SPlayerStats& stats = m_player.m_stats;
	// update the player rotation if view control is taken from somewhere else
	if (m_health > 0 && (/*m_pVehicle || */stats.followCharacterHead.Value()))
	{
		Vec3 forward = viewParams.rotation.GetColumn1().GetNormalizedSafe();
		Vec3 up = Vec3(0, 0, 1);

		m_baseQuat = viewParams.rotation;//Quat(Matrix33::CreateFromVectors(forward % up,forward,up));
		//m_baseQuat.NoScale();

		m_viewQuat = m_viewQuatFinal = viewParams.rotation;
		//m_input.viewVector = viewParams.rotation.GetColumn1();
	}
}

void PlayerView::ViewFirstPersonOnLadder(SViewParams& viewParams)
{
	viewParams.viewID = 0;
	viewParams.nearplane = 0.12f;
	viewParams.position = m_entityWorldMatrix * (m_localEyePos + Vec3(0.02f, 0.0f, 0.05f));// * (m_localEyePos+Vec3(0.1f,0.0f,0.0f));

	//Different camera angles for debugging
	if (g_pGameCVars->pl_debug_ladders != 0)
		viewParams.position = m_entityWorldMatrix * (m_localEyePos + Vec3(0.1f, -g_pGameCVars->cl_tpvDist, -0.3f));

	m_viewQuat = m_viewQuatFinal = viewParams.rotation;

	if (g_pGameCVars->g_detachCamera != 0)
	{
		viewParams.position = m_lastPos;
		viewParams.rotation = m_lastQuat;
	}
}

void PlayerView::ViewThirdPersonOnLadder(SViewParams& viewParams)
{
	ViewThirdPerson(viewParams);

	//lift the camera a little up
	viewParams.position.z += 1.0f;
}

// Position the first person weapons
void PlayerView::FirstPersonWeaponPostProcess(SViewParams& viewParams)
{
	m_player.m_stats.FPWeaponPosOffset = m_vFPWeaponOffset;
	m_player.m_stats.FPWeaponAnglesOffset = m_vFPWeaponAngleOffset;

	m_player.m_stats.FPSecWeaponPosOffset = m_vFPWeaponOffset;
	m_player.m_stats.FPSecWeaponAnglesOffset = m_vFPWeaponAngleOffset;
}

// Shake the view as requested
void PlayerView::ViewShakePostProcess(SViewParams& viewParams)
{
	if (m_player.m_stats.inFreefall.Value())
	{
		IView* pView = g_pGame->GetIGameFramework()->GetIViewSystem()->GetViewByEntityId(m_player.GetEntityId());
		if (pView)
		{
			float dot(0.1f * min(19.0f, 5.0f + max(0.0f, m_player.m_stats.velocity * viewParams.rotation.GetColumn1())));
			if (m_player.m_stats.inFreefall.Value() == 2)
				dot *= 0.5f;

			//float white[4] = {1,1,1,1};
			//gEnv->pRenderer->Draw2dLabel( 100, 50, 2, white, false, "dot:%f vel:%f", dot, m_player.m_stats.velocity.len() );

			pView->SetViewShake(Ang3(0.0005f, 0.001f, 0.0005f) * dot, Vec3(0.003f, 0.0f, 0.003f) * dot, 0.3f, 0.015f, 1.0f, 2);
		}
	}

#if 0
	if (m_io.viewShake.amount > 0.001f)
	{
		viewParams.shakeDuration = m_io.viewShake.duration;
		viewParams.shakeFrequency = m_io.viewShake.frequency;
		viewParams.shakeAmount = m_io.viewShake.angle;
		viewParams.shakeID = 1;

		memset(&m_io.viewShake, 0, sizeof(SViewShake));
	}
	/*else if (IsZeroG() && m_stats.speedFlat > 10.0f)
	{
	viewParams.shakeDuration = 0.1f;
	viewParams.shakeFrequency = 0.1f;
	viewParams.shakeAmount = Ang3(m_stats.speedFlat*0.001f,0,0);
	}*/
	else if (stats.inAir < 0.1f && stats.speedFlat > m_standSpeedMax * 1.1f)
	{
		viewParams.shakeDuration = 0.1f;/// (shake*shake);
		viewParams.shakeFrequency = 0.1f;/// (shake*shake);
		viewParams.shakeAmount = Ang3(stats.speedFlat * 0.0015f, stats.speedFlat * 0.00035f, stats.speedFlat * 0.00035f) * g_pGameCVars->cl_sprintShake;
		viewParams.shakeID = 0;
	}
#endif
	//testing code
	/*if (m_stats.inAir < 0.1f && m_stats.speedFlat > GetStanceInfo(STANCE_STAND)->maxSpeed * 1.1f)
	{
	float shake(g_pGameCVars->cl_sprintShake);

	IView *pView = g_pGame->GetIGameFramework()->GetIViewSystem()->GetViewByEntityId(GetEntityId());
	if (pView)
	pView->SetViewShake({},Vec3(m_stats.speedFlat*0.0035f,0,m_stats.speedFlat*0.0035f) * shake,0.1f,0.05f,0.5f,1);
	}*/
}

// If there are first person hands present, then position and orient them relative to the view
void PlayerView::HandsPostProcess(SViewParams& viewParams)
{
	//for testing sake
	float nearPlaneOverride(g_pGameCVars->cl_nearPlane);
	if (nearPlaneOverride > 0.001f)
		viewParams.nearplane = nearPlaneOverride;

	//FIXME:find a better place?
	// Convention: if Player has a renderable object in slot 2 then it's the first person hands.
	if (m_player.GetEntity()->GetSlotFlags(2) & ENTITY_SLOT_RENDER)
	{
		Matrix34 handsMtx(m_player.m_viewQuatFinal * Quat::CreateRotationZ(gf_PI));
		handsMtx.SetTranslation(viewParams.position);

		m_player.GetEntity()->SetSlotLocalTM(2, m_player.GetEntity()->GetWorldTM().GetInverted() * handsMtx);
		ICharacterInstance* pHands = m_player.GetEntity()->GetCharacter(2);
		if (pHands)
		{
			QuatT renderLocation = QuatT(handsMtx);
			pHands->GetISkeletonPose()->SetForceSkeletonUpdate(5);
			pHands->SkeletonPreProcess(renderLocation, renderLocation, GetISystem()->GetViewCamera(), 0x55);
			pHands->SkeletonPostProcess(renderLocation, renderLocation, 0, 0.0f, 0x55);
		}
	}
}
