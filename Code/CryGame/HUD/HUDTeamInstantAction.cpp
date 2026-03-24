/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: PowerStruggle mode HUD code (refactored from old HUD code)

-------------------------------------------------------------------------
History:
- 21:02:20067  20:00 : Created by Jan Müller

*************************************************************************/

#include "HUDTeamInstantAction.h"

#include "HUD.h"
#include "GameFlashAnimation.h"
#include "CryGame/Game.h"
#include "CryGame/GameCVars.h"
#include "CryGame/GameRules.h"

CHUDTeamInstantAction::CHUDTeamInstantAction(CHUD *pHUD) : 
g_pHUD(pHUD)
{
	m_animTIAScore.Load("Libs/UI/HUD_TIAScore.gfx", eFD_Center, eFAF_ManualRender|eFAF_Visible);
	m_animTIAScore.SetVisible(true);
}

CHUDTeamInstantAction::~CHUDTeamInstantAction()
{
	m_animTIAScore.Unload();
}

void CHUDTeamInstantAction::Reset()
{
	m_ownTeamScore = 0;
	m_enemyTeamScore = 0;
	m_ownScore = 0;
	m_roundTime = 0;
	m_scoreLimit = 100;
	m_ownTeam = 0;
}

void CHUDTeamInstantAction::SetHUDColor()
{
	g_pHUD->SetFlashColor(&m_animTIAScore);
}

void CHUDTeamInstantAction::UpdateStats()
{

	CGameRules *pGameRules=g_pGame->GetGameRules();
	if(!pGameRules)
		return;

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if(!pClientActor)
		return;

	IScriptTable *pGameRulesScript=pGameRules->GetEntity()->GetScriptTable();
	if(!pGameRulesScript)
		return;


	int ownTeam = pGameRules->GetTeam(pClientActor->GetEntityId());

	int ownTeamScore = 0;
	int enemyTeamScore = 0;
	int ownScore = 0;
	int roundTime = 0;
	int scoreLimit = 100;
	int firstTeam = 1;

	if(ownTeam!=0) //no spectator
	{
		firstTeam = ownTeam;
	}
	int team0ScoreKey = 0;
	if(pGameRulesScript->GetValue("TEAMSCORE_TEAM0_KEY", team0ScoreKey))
	{
		for(int i=1; i<3; ++i)
		{
			int points = 0;
			pGameRules->GetSynchedGlobalValue(team0ScoreKey+i, points);
			if(firstTeam==i)
			{
				ownTeamScore = points;
			}
			else
			{
				enemyTeamScore = points;
			}
		}
	}

	HSCRIPTFUNCTION pfnGetScoreFlags=0;
	if (pGameRulesScript->GetValue("GetPlayerScore", pfnGetScoreFlags))
	{
		ScriptHandle actorId(pClientActor->GetEntityId());
		Script::CallReturn(gEnv->pScriptSystem, pfnGetScoreFlags, pGameRulesScript, actorId, ownScore);
		gEnv->pScriptSystem->ReleaseFunc(pfnGetScoreFlags);
	}

	roundTime = floor(pGameRules->GetRemainingGameTime());

	if (pGameRules->GetCurrentStateId() != 3 /*InGame*/ || !pGameRules->IsTimeLimited())
	{
		roundTime = -1.0f; //hides the timer
	}

	scoreLimit = g_pGameCVars->mp_tiaScoreLimit;

	if(	ownTeamScore!=m_ownTeamScore ||
			enemyTeamScore!=m_enemyTeamScore ||
			ownScore!=m_ownScore ||
			roundTime!=m_roundTime ||
			scoreLimit!=m_scoreLimit ||
			ownTeam!=m_ownTeam)
	{
		m_ownTeamScore = ownTeamScore;
		m_enemyTeamScore = enemyTeamScore;
		m_ownScore = ownScore;
		m_roundTime = roundTime;
		m_scoreLimit = scoreLimit;
		m_ownTeam = ownTeam;
		PushToFlash();
	}

}

void CHUDTeamInstantAction::Show(bool show)
{
	m_animTIAScore.SetVisible(show);
}

void CHUDTeamInstantAction::SetTIAScoreHidden(ETIAScoreHideReason reason, bool hide)
{
	if (hide)
		m_tiaScoreHideFlags |= reason;
	else
		m_tiaScoreHideFlags &= ~reason;

	const bool shouldBeVisible = (m_tiaScoreHideFlags == 0);

	if (m_animTIAScore.GetVisible() != shouldBeVisible)
	{
		m_animTIAScore.SetVisible(shouldBeVisible);
	}
}

void CHUDTeamInstantAction::PushToFlash()
{
	SFlashVarValue args[6] = {m_scoreLimit, m_ownTeamScore, m_enemyTeamScore, m_ownScore, m_roundTime, m_ownTeam};
	m_animTIAScore.Invoke("setValues", args, 6);
	if(m_ownTeam==0)
		g_pHUD->SetTeamDisplay("");
	else
		g_pHUD->SetTeamDisplay(m_ownTeam==1?"NK":"US");
}

void CHUDTeamInstantAction::Update(float fDeltaTime)
{
	if(!m_animTIAScore.IsLoaded() || !m_animTIAScore.GetVisible())
		return;

	UpdateStats();

	m_animTIAScore.GetFlashPlayer()->Advance(fDeltaTime);
	m_animTIAScore.GetFlashPlayer()->Render();
}