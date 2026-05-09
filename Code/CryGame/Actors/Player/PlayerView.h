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

#if _MSC_VER > 1000
# pragma once
#endif

#include "Player.h"


class PlayerView
{
public:

	explicit PlayerView(CPlayer& player);
	~PlayerView();

	void Reset();
	
	void Update(SViewParams &viewParams);
	void OnExitVehicle();
	void StopVehicleViewUpdates();

private:
	
	CPlayer& m_player;
	EntityId m_playerId = 0;

	Vec3 m_lastPos = {};
	Quat m_lastQuat = Quat(IDENTITY);
	uint8 m_lastSeatId = 0;
	float m_ColDistance = 0.0f; 
	float m_defaultFov = 0.0f;
	float m_frameTime = 0.0f;
	float m_health = 0.0f;
	float m_smoothViewZ = 0.0f;
	float m_smoothZType = 0.0f;
	bool m_bIsGrabbing = false;
	bool m_bUsePivot = false;
	bool m_landed = false;
	bool m_jumped = false;
	bool m_vehicleReverseView = false;
	float m_bobMul = 0.0f;
	float m_bobCycle = 0.0f;
	float m_standSpeed = 0.0f;
	float m_standSpeedMax = 0.0f;
	Quat m_viewQuatForWeapon = Quat(IDENTITY);
	Vec3 m_eyeOffsetViewGoal = {};
	Ang3 m_wAngles = {};
	Quat m_viewQuatFinal = Quat(IDENTITY);
	Quat m_viewQuat = Quat(IDENTITY);
	Quat m_baseQuat = Quat(IDENTITY);
	Vec3 m_eyeOffsetView = {};
	Vec3 m_localEyePos = {};
	Vec3 m_worldEyePos = {};
	Vec3 m_entityWorldPos = {};
	Ang3 m_vFPWeaponAngleOffset = {};
	Vec3 m_vFPWeaponLastDirVec = {};
	Vec3 m_vFPWeaponOffset = {};
	Vec3 m_bobOffset = {};
	Ang3 m_angleOffset = {};
	Ang3 m_viewAngleOffset = {};
	Matrix34 m_entityWorldMatrix = Matrix34(IDENTITY);
	Matrix33 m_headMtxLocal = Matrix33(IDENTITY);
	IVehicle *m_pVehicle = nullptr;
	ICharacterInstance *m_pCharacter = nullptr;
	float m_fastCameraCorrectionMode = 0.0f;

	//Vehicle view update
	int m_currentViewUpdateSeatId = 0;
	EntityId m_currentViewUpdateVehicleId = 0;

protected:

	void ViewFirstThirdSharedPre(SViewParams &viewParams);
	void ViewFirstThirdSharedPost(SViewParams &viewParams);
	Vec3 GetCollision(CActor* pActor, const Vec3 transformPosition, const Vec3 forward, const Vec3 vector, float radius, bool pushByNormal);
	void ViewThirdPerson(SViewParams &viewParams);
	Vec3 Project(Vec3 vector, Vec3 onNormal);
	void ViewFirstPerson(SViewParams &viewParams);
	void FirstPersonJump(SViewParams &viewParams,Vec3 &weaponOffset, Ang3 &weaponAngleOffset);
	void ViewVehicle(SViewParams &viewParams);

	void ViewFollowCharacterFirstPerson(SViewParams &viewParams);
	void ViewFirstPersonOnLadder(SViewParams & viewParams);
	void ViewThirdPersonOnLadder(SViewParams& viewParams);
	void ViewSpectatorTarget(SViewParams &viewParams);
	void ViewSpectatorTarget_CryMP(SViewParams& viewParams);
	void ViewDeathCamTarget(SViewParams &viewParams);

	void ViewExternalControlPostProcess(SViewParams &viewParams);
	public:
	void FirstPersonWeaponPostProcess(SViewParams &viewParams);
	void SetFastCameraCorrectionMode(float seconds)
	{
		m_fastCameraCorrectionMode = seconds;
	}
	protected:
	void ViewShakePostProcess(SViewParams &viewParams);
	void HandsPostProcess(SViewParams &viewParams);

	void ViewProcess(SViewParams &viewParams);
	void ViewPreProcess(SViewParams &viewParams);
	void ViewPostProcess(SViewParams &viewParams);
};

