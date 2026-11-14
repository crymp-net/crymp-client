/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 26:04:2006   18:38 : Created by Márcio Martins

*************************************************************************/
#include "CryCommon/CrySystem/ISystem.h"
#include "Fists.h"
#include "CryGame/Game.h"
#include "CryGame/Actors/Actor.h"
#include "FireModes/Throw.h"
#include "Firemodes/Melee.h"
#include "CryGame/GameCVars.h"
#include "CryCommon/CryAction/IViewSystem.h"
#include "CryCommon/CryAction/IWorldQuery.h"
#include "CryCommon/CryAction/IGameTokens.h"
#include "CryGame/GameActions.h"
#include "CryCommon/CrySystem/ILog.h"

#include "CryGame/HUD/HUD.h"

TActionHandler<CFists>	CFists::s_actionHandler;

#define TIMEOUT			2.5f
//------------------------------------------------------------------------
CFists::CFists()
	:m_underWater(false)
	, m_currentAnimState(eFAS_FIGHT)
	, m_inFreeFall(false)
	, m_timeOut(0.0f)
{
	m_useFPCamSpacePP = true; //Overwritten by OffHand

	if (s_actionHandler.GetNumHandlers() == 0)
	{
#define ADD_HANDLER(action, func) s_actionHandler.AddHandler(actions.action, &CFists::func)
		const CGameActions& actions = g_pGame->Actions();

		ADD_HANDLER(attack1, OnActionAttack);
		ADD_HANDLER(attack2, OnActionAttack);
		ADD_HANDLER(special, OnActionSpecial);
#undef ADD_HANDLER
	}
}


//------------------------------------------------------------------------
CFists::~CFists()
{
}

//-------------------------------------------------------------------------
void CFists::Reset()
{
	CItem::Reset();

	m_currentAnimState = eFAS_FIGHT;
	m_underWater = false;
	m_inFreeFall = false;
}
//------------------------------------------------------------------------
void CFists::Update(SEntityUpdateContext& ctx, int slot)
{
	CWeapon::Update(ctx, slot);

	if (slot == eIUS_General)
		UpdateAnimState(ctx.fFrameTime);
}

//------------------------------------------------------------------------
void CFists::UpdateFPView(float frameTime)
{
	CWeapon::UpdateFPView(frameTime);
}

//------------------------------------------------------------------------
void CFists::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (IsSelected() && !m_underWater)
	{
		if (!s_actionHandler.Dispatch(this, actorId, actionId, activationMode, value))
		{
			CWeapon::OnAction(actorId, actionId, activationMode, value);
		}
	}
}

//------------------------------------------------------------------------
bool CFists::CanSelect() const
{
	return CWeapon::CanSelect();
}

//------------------------------------------------------------------------
void CFists::Select(bool select)
{
	CWeapon::Select(select);
	SetActionSuffix("");

	if (select)
	{
		EnableUpdate(true, eIUS_General);
		RequestAnimState(eFAS_FIGHT);
	}
	else
	{
		EnableUpdate(false, eIUS_General);
		RequestAnimState(eFAS_NOSTATE);
	}
}

//-----------------------------------------------------------------------
void CFists::PostFilterView(SViewParams& viewParams)
{
	CWeapon::PostFilterView(viewParams);

	if (m_camerastats.animating && IsWeaponRaised())
	{
		viewParams.nearplane = 0.1f;
	}

}

//------------------------------------------------------------------------
void CFists::EnterWater(bool enter)
{
	m_underWater = enter;
}

//-----------------------------------------
//Just a timeout to change to "Relaxed" state
void CFists::UpdateAnimState(float frameTime)
{
	//Only if selected
	if (!IsSelected() || m_frozen || IsWeaponRaised() || m_underWater || m_inFreeFall)
		return;

	if (m_timeOut >= 0.0f && m_currentAnimState == eFAS_FIGHT)
		m_timeOut -= frameTime;

	if (m_timeOut < 0.0f)
		RequestAnimState(eFAS_RELAXED);
}

//------------------------------------------
//CFists::RequestAnimState(EFistAnimState eFAS)
//
//This method changes (if possible) the current animation for the fists
// eFAS - Requested anim state
void CFists::RequestAnimState(EFistAnimState eFAS, bool force /*=false*/)
{

	//Only if selected
	if (!IsSelected() || m_frozen || IsWeaponRaised())
		return;

	if (!m_underWater)
	{
		switch (eFAS)
		{
		case eFAS_NOSTATE:	m_currentAnimState = eFAS_NOSTATE;
			m_timeOut = TIMEOUT;
			break;

		case eFAS_RELAXED:
			if (m_currentAnimState != eFAS_NOSTATE && m_currentAnimState != eFAS_RELAXED)
			{
				m_currentAnimState = eFAS_RELAXED;
				m_timeOut = TIMEOUT;
				PlayAction(g_pItemStrings->deselect);
				SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle_relaxed);
			}
			break;

		case eFAS_FIGHT:		//if(m_currentAnimState!=eFAS_RUNNING)
		{
			SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
			m_currentAnimState = eFAS_FIGHT;
			m_timeOut = TIMEOUT;
		}
		break;

		case eFAS_RUNNING:
			if (m_currentAnimState == eFAS_RELAXED || CanMeleeAttack())
			{
				PlayAction(g_pItemStrings->run_forward, 0, true);
				m_currentAnimState = eFAS_RUNNING;
			}
			break;

		case eFAS_JUMPING:
			if (m_currentAnimState == eFAS_RUNNING)
			{
				PlayAction(g_pItemStrings->jump_start);
				SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->jump_idle);
				m_currentAnimState = eFAS_JUMPING;
			}
			break;

		case eFAS_LANDING:	 if (m_currentAnimState == eFAS_JUMPING)
		{
			PlayAction(g_pItemStrings->jump_end);
			SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle_relaxed);
			m_currentAnimState = eFAS_RELAXED;
		}
						 break;

		case eFAS_CRAWL:		if (m_currentAnimState != eFAS_CRAWL)
		{
			PlayAction(g_pItemStrings->crawl, 0, true);
			//SetDefaultIdleAnimation(CItem::eIGS_FirstPerson,g_pItemStrings->crawl);
			m_currentAnimState = eFAS_CRAWL;
		}
					   break;


		}
	}
	else
	{
		switch (eFAS)
		{
		case eFAS_SWIM_IDLE:		if (m_currentAnimState != eFAS_SWIM_IDLE)
		{
			m_currentAnimState = eFAS_SWIM_IDLE;
			PlayAction(g_pItemStrings->swim_idle, 0, true, eIPAF_Default | eIPAF_CleanBlending);
		}
						   break;

		case eFAS_SWIM_IDLE_UW:	if (m_currentAnimState != eFAS_SWIM_IDLE_UW)
		{
			m_currentAnimState = eFAS_SWIM_IDLE_UW;
			PlayAction(g_pItemStrings->swim_idle_2, 0, true, eIPAF_Default | eIPAF_CleanBlending);
		}
							  break;

		case eFAS_SWIM_FORWARD:	if (m_currentAnimState != eFAS_SWIM_FORWARD)
		{
			m_currentAnimState = eFAS_SWIM_FORWARD;
			PlayAction(g_pItemStrings->swim_forward, 0, true, eIPAF_Default | eIPAF_CleanBlending);
		}
							  break;

		case eFAS_SWIM_FORWARD_UW:	if (m_currentAnimState != eFAS_SWIM_FORWARD_UW)
		{
			m_currentAnimState = eFAS_SWIM_FORWARD_UW;
			PlayAction(g_pItemStrings->swim_forward_2, 0, true, eIPAF_Default | eIPAF_CleanBlending);
		}
								 break;

		case eFAS_SWIM_BACKWARD: if (m_currentAnimState != eFAS_SWIM_BACKWARD)
		{
			m_currentAnimState = eFAS_SWIM_BACKWARD;
			PlayAction(g_pItemStrings->swim_backward, 0, true, eIPAF_Default | eIPAF_CleanBlending);
		}
							   break;

		case eFAS_SWIM_SPEED:		if (m_currentAnimState != eFAS_SWIM_SPEED)
		{
			m_currentAnimState = eFAS_SWIM_SPEED;
			PlayAction(g_pItemStrings->speed_swim, 0, true, eIPAF_Default | eIPAF_CleanBlending);
		}
							break;

		case eFAS_SWIM_FORWARD_SPEED: if (m_currentAnimState != eFAS_SWIM_FORWARD_SPEED)
		{
			m_currentAnimState = eFAS_SWIM_FORWARD_SPEED;
			PlayAction(g_pItemStrings->swim_forward_2, 0, true, eIPAF_Default | eIPAF_CleanBlending);
		}
									break;
		}
	}

}

//---------------------------------------------------------------
//This is not "properly" RaiseWeapon(), but it works in a similar way
//as for the other ones.

struct CFists::EndRaiseWeaponAction
{
	EndRaiseWeaponAction(CFists* _fists) : fists(_fists) {};
	CFists* fists;

	void execute(CItem* _this)
	{
		fists->SetWeaponRaised(false);
		fists->m_currentAnimState = eFAS_RELAXED;
	}
};

void CFists::RaiseWeapon(bool raise, bool faster /*= false*/)
{
	CActor* pActor = GetOwnerActor();
	if (!pActor)
		return;

	const bool bIsClient = pActor->IsClient();
	if (!bIsClient && !IsOwnerFP()) //FP spec support
		return;

	//Only when colliding something while running
	if (raise && g_pGameCVars->mp_wallJump > 0.01f)
	{
		//Skip these checks for other clients
		if (bIsClient)
		{
			const bool bAllow = ((GetCurrentAnimState() == eFAS_RUNNING || GetCurrentAnimState() == eFAS_JUMPING) && !IsWeaponRaised());

			if (!bAllow)
				return;

			if ((m_fm && m_fm->IsFiring()) || (m_melee && m_melee->IsFiring()))
				return;
		}

		//If NANO speed selected...
		float speedOverride = -1.0f;

		if (CNanoSuit* pSuit = CPlayer::GetNanoSuit(pActor))
		{
			if (pSuit->GetMode() == NANOMODE_SPEED)
			{
				speedOverride = 1.75f;
			}
		}

		if (speedOverride > 0.0f)
			PlayAction(g_pItemStrings->raise, 0, false, CItem::eIPAF_Default, speedOverride);
		else
			PlayAction(g_pItemStrings->raise);

		SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle_relaxed);
		SetWeaponRaised(true);

		//Also give the player some impulse into the opposite direction
		Vec3 pos;

		IPhysicalEntity* playerPhysics = pActor->GetEntity()->GetPhysics();
		if (playerPhysics)
		{
			IMovementController* pMC = pActor->GetMovementController();
			if (pMC)
			{
				SMovementState state;
				pMC->GetMovementState(state);

				if (bIsClient)
				{
					pe_status_dynamics dynamics;
					float dt = gEnv->pTimer->GetFrameTime();
					if (playerPhysics->GetStatus(&dynamics)) {

						float  w = g_pGameCVars->mp_wallJump;
						float  k = 1.0f;
						Vec3   i = -state.eyeDirection * 600.0f;

						if (w <= 1.0f) {
							// if mp_wallJump is <0; 1>, use the multiplier
							i *= w;
						}

						Vec3 idt = i * dt;
						Vec3& v0 = dynamics.v;                  // current velocity
						Vec3  v1 = v0 + dynamics.a * dt;        // temporary velocity, without impulse
						Vec3  v2 = v1 + idt;                    // final velocity

						// if mp_wallJump is (1; +inf) and new velocity > mp_wallJump and new velocity > previous velocity
						// then dampen the impulse such that new velocity is capped at mp_wallJump value
						if (w > (1.0f + FLT_EPSILON) && v2.len() >= w && v2.len() > v1.len()) {
							if (v1.len() > w) {
								k = 0.0f;
							}
							else {
								float A = idt.len2();
								float B = 2 * v1.Dot(idt);
								float C = v1.len2() - powf(w, 2.0f);
								k = (-B + sqrt(B * B - 4 * A * C)) / (2 * A);
							}

							if (isnan(k)) {
								// it shouldn't happen, but just in case B^2 - 4AC yields a value below zero, 
								// making sqrt NaN, assume k to be zero to prevent any random behaviors
								k = 0.0f;
							}
						}

						// v2 = v1 + k * idt;	                // recomputed final velocity
						// gEnv->pLog->Log("v1: %f -> v2: %f (k: %f, dt: %f)", v1.len(), v2.len(), k, dt);

						pe_action_impulse impulse;
						impulse.iApplyTime = 1;
						impulse.impulse = k * i;
						playerPhysics->Action(&impulse);
						//CryMP: Send walljump request to server
						RequestWeaponRaised(true);
					}
				}

				pos = state.eyePosition + state.eyeDirection * 0.5f;
			}

		}

		GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson), CSchedulerAction<EndRaiseWeaponAction>::Create(EndRaiseWeaponAction(this)), true);

		//Sound and FX feedback
		CollisionFeeback(pos, m_currentAnimState);
	}
	else if (!raise)
	{
		SetWeaponRaised(false);
	}

}

//---------------------------------------------------------------
void CFists::CollisionFeeback(Vec3& pos, int eFAS)
{
	CPlayer* pPlayer = CPlayer::FromActor(GetOwnerActor());
	if (pPlayer)
	{
		switch (eFAS)
		{
		case eFAS_RUNNING:
			pPlayer->PlaySound(CPlayer::ESound_Hit_Wall, true, true, "speed", 0.4f);
			break;

		case eFAS_JUMPING:
			pPlayer->PlaySound(CPlayer::ESound_Hit_Wall, true, true, "speed", 0.8f);
			break;

		default:
			break;
		}
	}

	//FX feedback
	IParticleEffect* pEffect = gEnv->p3DEngine->FindParticleEffect("collisions.footsteps.dirt");
	if (pEffect)
	{
		Matrix34 tm = IParticleEffect::ParticleLoc(pos);
		pEffect->Spawn(true, tm);
	}
}

//---------------------------------------------------------------
bool CFists::OnActionAttack(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	//SetCurrentFireMode(0);
	CWeapon::OnAction(actorId, actionId, activationMode, value);
	if (m_fm && m_fm->IsFiring())
		RequestAnimState(eFAS_FIGHT);
	return false;
}

bool CFists::OnActionSpecial(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	CWeapon::OnAction(actorId, actionId, activationMode, value);
	if (m_melee && m_melee->IsFiring())
		RequestAnimState(eFAS_FIGHT);
	return false;
}

//------------------------------------------------------------------------
void CFists::NetStartMeleeAttack(bool weaponMelee)
{
	RequestAnimState(eFAS_FIGHT);
	CWeapon::NetStartMeleeAttack(weaponMelee);
}

//------------------------------------------------------------------------
void CFists::FullSerialize(TSerialize ser)
{
	CWeapon::FullSerialize(ser);

	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.Value("underWater", m_underWater);
		ser.Value("inFreeFall", m_inFreeFall);
		ser.Value("m_timeOut", m_timeOut);
	}
}

//-----------------------------------------------------
tSoundID CFists::PlayAction(const ItemString& action, int layer /* =0  */, bool loop /* =false  */, unsigned int flags /* = eIPAF_Default  */, float speedOverride /* = -1.0f  */)
{
	if (action == g_pItemStrings->offhand_on && m_currentAnimState == eFAS_RELAXED)
		return CWeapon::PlayAction(g_pItemStrings->offhand_on_akimbo, layer, loop, flags, speedOverride);
	else if (action == g_pItemStrings->offhand_off && m_currentAnimState == eFAS_RELAXED)
		return CWeapon::PlayAction(g_pItemStrings->offhand_off_akimbo, layer, loop, flags, speedOverride);
	else
		return CWeapon::PlayAction(action, layer, loop, flags, speedOverride);
}
