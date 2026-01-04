// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "CryCommon/CrySystem/ISystem.h"
#include "NetPlayerInput.h"
#include "Player.h"
#include "CryGame/Game.h"
#include "CryGame/GameCVars.h"

CNetPlayerInput::CNetPlayerInput(CPlayer* pPlayer) : m_pPlayer(pPlayer)
{
}

void CNetPlayerInput::PreUpdate()
{
	const IPhysicalEntity *pPhysEnt = m_pPlayer->GetEntity()->GetPhysics();
	if (!pPhysEnt)
		return;

	CMovementRequest moveRequest;
	SMovementState moveState;
	m_pPlayer->GetMovementController()->GetMovementState(moveState);
	Quat worldRot = m_pPlayer->GetBaseQuat(); 
	Vec3 deltaMovement = worldRot.GetInverted().GetNormalized() * m_curInput.deltaMovement;
	// absolutely ensure length is correct
	deltaMovement = deltaMovement.GetNormalizedSafe(ZERO) * m_curInput.deltaMovement.GetLength();
	moveRequest.AddDeltaMovement(deltaMovement);

	Vec3 lookDirection(m_curInput.lookDirection);
	m_pPlayer->m_netAimDir = lookDirection;

	//CryMP: FP Spectator uses interpolated lookDirection
	if (m_pPlayer->IsFpSpectatorTarget())
	{
		lookDirection = m_pPlayer->GetNetAimDirSmooth();
	}

	const Vec3 distantTarget = moveState.eyePosition + 1000.0f * lookDirection;
	moveRequest.SetLookTarget(distantTarget);
	moveRequest.SetAimTarget(distantTarget);

	if (m_curInput.deltaMovement.GetLengthSquared() > sqr(0.02f)) // 0.2f is almost stopped
		moveRequest.SetBodyTarget(distantTarget);
	else
		moveRequest.ClearBodyTarget();

	moveRequest.SetAllowStrafing(true);

	float pseudoSpeed = 0.0f;
	if (m_curInput.deltaMovement.len2() > 0.0f)
	{
		pseudoSpeed = m_pPlayer->CalculatePseudoSpeed(m_curInput.sprint);
	}
	moveRequest.SetPseudoSpeed(pseudoSpeed);

	float lean = 0.0f;
	if (m_curInput.leanl)
		lean -= 1.0f;
	if (m_curInput.leanr)
		lean += 1.0f;

	if (fabsf(lean) > 0.01f)
		moveRequest.SetLean(lean);
	else
		moveRequest.ClearLean();

	m_pPlayer->GetMovementController()->RequestMovement(moveRequest);

	if (m_curInput.sprint)
		m_pPlayer->m_actions |= ACTION_SPRINT;
	else
		m_pPlayer->m_actions &= ~ACTION_SPRINT;

	if (m_curInput.leanl)
		m_pPlayer->m_actions |= ACTION_LEANLEFT;
	else
		m_pPlayer->m_actions &= ~ACTION_LEANLEFT;

	if (m_curInput.leanr)
		m_pPlayer->m_actions |= ACTION_LEANRIGHT;
	else
		m_pPlayer->m_actions &= ~ACTION_LEANRIGHT;
}

void CNetPlayerInput::Update()
{
	if (gEnv->bServer && (g_pGameCVars->sv_input_timeout > 0) && ((gEnv->pTimer->GetFrameStartTime() - m_lastUpdate).GetMilliSeconds() >= g_pGameCVars->sv_input_timeout))
	{
		m_curInput.deltaMovement.zero();
		m_curInput.sprint = m_curInput.leanl = m_curInput.leanr = false;
		m_curInput.stance = STANCE_NULL;

		m_pPlayer->GetGameObject()->ChangedNetworkState(INPUT_ASPECT);
	}
}

void CNetPlayerInput::PostUpdate()
{
}

void CNetPlayerInput::SetState(const SSerializedPlayerInput& input)
{
	DoSetState(input);

	m_lastUpdate = gEnv->pTimer->GetCurrTime();
}

void CNetPlayerInput::GetState(SSerializedPlayerInput& input)
{
	input = m_curInput;
}

void CNetPlayerInput::Reset()
{
	m_curInput = SSerializedPlayerInput();  

	m_curInput.leanl = m_curInput.leanr = m_curInput.sprint = false;
	m_curInput.deltaMovement.zero();
	m_curInput.stance = STANCE_STAND; 

	DoSetState(m_curInput);
	m_pPlayer->GetGameObject()->ChangedNetworkState(IPlayerInput::INPUT_ASPECT);
}

void CNetPlayerInput::DisableXI(bool disabled)
{
}


void CNetPlayerInput::DoSetState(const SSerializedPlayerInput& input)
{
	m_curInput = input;
	m_pPlayer->GetGameObject()->ChangedNetworkState(INPUT_ASPECT);

	CMovementRequest moveRequest;

	const int stanceInput = static_cast<int>(m_curInput.stance);
	if (m_pPlayer->IsStanceInputValid(stanceInput))
	{
		moveRequest.SetStance(static_cast<EStance>(stanceInput));
	}
	//else
	//{
	//	CryLogAlways("Ignoring invalid net stance %d for %s (chan %d)",
	//		stanceInput, m_pPlayer->GetEntity()->GetName(), m_pPlayer->GetChannelId());
	//}

	if (IsDemoPlayback())
	{
		Vec3 localVDir(m_pPlayer->GetViewQuatFinal().GetInverted() * m_curInput.lookDirection);
		Ang3 deltaAngles(asin(localVDir.z), 0, cry_atan2f(-localVDir.x, localVDir.y));
		moveRequest.AddDeltaRotation(deltaAngles * gEnv->pTimer->GetFrameTime());
	}
	//else
	{
		moveRequest.SetLookTarget(m_pPlayer->GetEntity()->GetWorldPos() + 10.0f * m_curInput.lookDirection);
		moveRequest.SetAimTarget(moveRequest.GetLookTarget());
	}

	float pseudoSpeed = 0.0f;
	if (m_curInput.deltaMovement.len2() > 0.0f)
	{
		pseudoSpeed = m_pPlayer->CalculatePseudoSpeed(m_curInput.sprint);
	}
	moveRequest.SetPseudoSpeed(pseudoSpeed);
	moveRequest.SetAllowStrafing(true);

	float lean = 0.0f;
	if (m_curInput.leanl)
		lean -= 1.0f;
	if (m_curInput.leanr)
		lean += 1.0f;
	moveRequest.SetLean(lean);

	m_pPlayer->GetMovementController()->RequestMovement(moveRequest);

	// debug..
	if (g_pGameCVars->g_debugNetPlayerInput & 1)
	{
		IPersistantDebug* pPD = gEnv->pGame->GetIGameFramework()->GetIPersistantDebug();
		pPD->Begin((string("net_player_input_") + m_pPlayer->GetEntity()->GetName()).c_str(), true);
		pPD->AddSphere(moveRequest.GetLookTarget(), 0.5f, ColorF(1, 0, 1, 1), 1.0f);
		//			pPD->AddSphere( moveRequest.GetMoveTarget(), 0.5f, ColorF(1,1,0,1), 1.0f );

		Vec3 wp(m_pPlayer->GetEntity()->GetWorldPos() + Vec3(0, 0, 2));
		pPD->AddDirection(wp, 1.5f, m_curInput.deltaMovement, ColorF(1, 0, 0, 1), 1.0f);
		pPD->AddDirection(wp, 1.5f, m_curInput.lookDirection, ColorF(0, 1, 0, 1), 1.0f);
	}
}
