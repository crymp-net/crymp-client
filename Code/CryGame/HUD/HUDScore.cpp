/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: HUD score board for multiplayer

-------------------------------------------------------------------------
History:
- 27:03:2006: Created by Jan Müller

*************************************************************************/

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryInput/IInput.h"
#include "CryCommon/CrySystem/ITimer.h"
#include "HUDObject.h"
#include "HUDScore.h"
#include "HUD.h"
#include "HUDRadar.h"
#include "CryCommon/CryAction/IGameFramework.h"
#include "CryCommon/CryAction/IUIDraw.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "GameFlashAnimation.h"
#include "HUDCommon.h"
#include "CryGame/Actors/Actor.h"
#include "Library/StringTools.h"

CHUDScore::ScoreEntry::ScoreEntry(EntityId id, int kills, int deaths, int ping, int teamKills, int playerScore, bool prioTeamKills)
{
	m_entityId = id;
	m_team = -1;
	m_kills = kills;
	m_deaths = deaths;
	m_ping = ping;
	m_currentRank = 0;
	m_score = playerScore;
	m_teamKills = teamKills;
	m_prioritizeTeamKills = prioTeamKills;

	if (int team = g_pGame->GetGameRules()->GetTeam(id))
		m_team = team;
}

void CHUDScore::ScoreEntry::UpdateLiveStats()
{
	if (CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_entityId)))
	{
		m_alive = pActor->GetHealth() > 0;
		if (pActor->GetSpectatorMode() && !g_pGame->GetGameRules()->IsPlayerActivelyPlaying(pActor->GetEntityId()))
			m_spectating = true;
		else
			m_spectating = false;

		//update rank
		m_currentRank = 0;
		CGameRules* pGameRules = g_pGame->GetGameRules();
		if (!m_spectating && pGameRules)
		{
			const TSynchedKey RANK_KEY(202);
			pGameRules->GetSynchedEntityValue(m_entityId, RANK_KEY, m_currentRank);
			m_currentRank = MAX(m_currentRank, 1);
		}
	}
}

bool CHUDScore::ScoreEntry::operator<(const ScoreEntry& entry) const
{
	if(m_spectating)
		return false;
	else if(entry.m_spectating)
		return true;
	else if(m_team >= 0 && m_team != entry.m_team)
	{
		if(m_team > entry.m_team)
			return false;
		return true;
	}
	else
	{
		if(entry.m_currentRank > m_currentRank)
			return false;
		else if(m_currentRank > entry.m_currentRank)
			return true;
		else if(entry.m_score > m_score)
			return false;
		else if(m_score > entry.m_score)
			return true;
		else
		{
			if(entry.m_prioritizeTeamKills)
			{
				if(entry.m_teamKills < m_teamKills)
					return false;
				else if(m_teamKills < entry.m_teamKills)
					return true;
				else if(entry.m_kills > m_kills)
					return false;
				else if(m_kills > entry.m_kills)
					return true;
				else
				{
					if(m_deaths < entry.m_deaths)
						return true;
					else if (m_deaths > entry.m_deaths)
						return false;
					else
					{
						IEntity *pEntity0=gEnv->pEntitySystem->GetEntity(m_entityId);
						IEntity *pEntity1=gEnv->pEntitySystem->GetEntity(entry.m_entityId);

						const char *name0=pEntity0?pEntity0->GetName():"";
						const char *name1=pEntity1?pEntity1->GetName():"";

						if (strcmp(name0, name1)<0)
							return true;
						else
							return false;
					}
				}
			}
			else
			{
				if(entry.m_kills > m_kills)
					return false;
				else if(m_kills > entry.m_kills)
					return true;
				else
				{
					if(m_deaths < entry.m_deaths)
						return true;
					else if (m_deaths > entry.m_deaths)
						return false;
					else
					{
						IEntity *pEntity0=gEnv->pEntitySystem->GetEntity(m_entityId);
						IEntity *pEntity1=gEnv->pEntitySystem->GetEntity(entry.m_entityId);

						const char *name0=pEntity0?pEntity0->GetName():"";
						const char *name1=pEntity1?pEntity1->GetName():"";

						if (strcmp(name0, name1)<0)
							return true;
						else
							return false;
					}
				}
			}
		}
		return true;
	}
}

void CHUDScore::SRankStats::Update(IScriptTable* pGameRulesScript, IActor* pActor, CGameFlashAnimation* pFlashBoard)
{
	HSCRIPTFUNCTION pfnFuHelper = 0;
	if (pGameRulesScript->GetValue("GetPlayerRank", pfnFuHelper) && pfnFuHelper)
	{
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, ScriptHandle(pActor->GetEntityId()), currentRank);
		gEnv->pScriptSystem->ReleaseFunc(pfnFuHelper);
	}
	if (!currentRank)
		return;

	if (pGameRulesScript->GetValue("GetRankName", pfnFuHelper) && pfnFuHelper)
	{
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, currentRank, true, false, currentRankName);
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, currentRank + 1, true, false, nextRankName);
		gEnv->pScriptSystem->ReleaseFunc(pfnFuHelper);
	}

	if (!nextRankName)
		return;

	if (pGameRulesScript->GetValue("GetPlayerCP", pfnFuHelper) && pfnFuHelper)
	{
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, ScriptHandle(pActor->GetEntityId()), playerCP);
		gEnv->pScriptSystem->ReleaseFunc(pfnFuHelper);
	}

	if (pGameRulesScript->GetValue("GetRankCP", pfnFuHelper) && pfnFuHelper)
	{
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, currentRank, currentRankCP);
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, currentRank + 1, nextRankCP);
		gEnv->pScriptSystem->ReleaseFunc(pfnFuHelper);

		//set values to flash
		if (pFlashBoard)
		{
			SFlashVarValue args[4] = { nextRankName, 100.0f * ((playerCP - currentRankCP) / float(nextRankCP - currentRankCP)), currentRank, currentRankName };
			pFlashBoard->Invoke("setClientRankProgress", args, 4);
		}
	}

	if (pGameRulesScript->GetValue("GetRankPP", pfnFuHelper) && pfnFuHelper)
	{
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, currentRank, currentRankPP);
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, currentRank + 1, nextRankPP);
		gEnv->pScriptSystem->ReleaseFunc(pfnFuHelper);

		//set values to flash
		if (pFlashBoard)
		{
			SFlashVarValue args[2] = { currentRankPP, nextRankPP };
			pFlashBoard->Invoke("setMinimumPrestige", args, 2);
		}
	}
}

void CHUDScore::Update(float fDeltaTime)
{
	if (m_bShow && gEnv->bMultiplayer)
		Render();
	else
		m_currentClientTeam = -1;
}

void CHUDScore::Reset()
{
	m_scoreBoard.clear();
}

void CHUDScore::AddEntry(EntityId player, int kills, int deaths, int ping)
{
	//doesn't check for existing entries atm. (scoreboard deleted every frame)
	//m_scoreBoard.push_back(ScoreEntry(player, kills, deaths, ping));
}

void CHUDScore::Render()
{
	if (!m_pFlashBoard)
		return;

	int lastTeam = -1;

	CGameRules* pGameRules = g_pGame->GetGameRules();
	if (!pGameRules)
		return;

	IScriptTable* pGameRulesScript = pGameRules->GetEntity()->GetScriptTable();
	const TSynchedKey PP_AMOUNT_KEY(200);

	m_scoreBoard.clear();

	IActorIteratorPtr actorIt = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
	while (IActor* pActor = actorIt->Next())
	{
		//CryMP: For MisterSD :s
		if (!pActor->IsPlayer())
			continue;

		const EntityId playerId = pActor->GetEntityId();
		const int KEY_KILL = 100;
		const int KEY_DEATH = 101;
		const int KEY_PING = 103;
		const int KEY_TK = 105;

		int kills = 0;
		pGameRules->GetSynchedEntityValue(playerId, KEY_KILL, kills);
		int deaths = 0;
		pGameRules->GetSynchedEntityValue(playerId, KEY_DEATH, deaths);
		int ping = 0;
		pGameRules->GetSynchedEntityValue(playerId, KEY_PING, ping);

		int teamKills = 0;
		bool prioTeamKills = false;

		int playerScore = 0;
		const bool TIA = m_pHUD->GetCurrentGameRules() == EHUD_TEAMINSTANTACTION;
		if (TIA)
		{
			prioTeamKills = true;
			pGameRules->GetSynchedEntityValue(playerId, KEY_TK, teamKills);

			IScriptTable* pGameRulesScript = pGameRules->GetEntity()->GetScriptTable();
			if (pGameRulesScript)
			{
				HSCRIPTFUNCTION pfnGetScoreFlags = 0;
				if (pGameRulesScript->GetValue("GetPlayerScore", pfnGetScoreFlags))
				{
					ScriptHandle actorId(playerId);
					Script::CallReturn(gEnv->pScriptSystem, pfnGetScoreFlags, pGameRulesScript, actorId, playerScore);
					gEnv->pScriptSystem->ReleaseFunc(pfnGetScoreFlags);
				}
			}
		}

		m_scoreBoard.push_back(ScoreEntry(playerId, kills, deaths, ping, teamKills, playerScore, prioTeamKills));
	}

	for (auto& m : m_scoreBoard)
	{
		m.UpdateLiveStats();
	}

	std::sort(m_scoreBoard.begin(), m_scoreBoard.end());

	//clear the teams stats in flash
	//m_pFlashBoard->Invoke("clearTable");

	IActor* pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	string svr_name = "";
	string svr_ip = "";

	pGameRules->GetSynchedGlobalValue(GLOBAL_SERVER_IP_KEY, svr_ip);
	pGameRules->GetSynchedGlobalValue(GLOBAL_SERVER_NAME_KEY, svr_name);

	if (svr_ip != m_currentServerIp || svr_name != m_currentServer)
	{
		m_currentServer = svr_name;
		m_currentServerIp = svr_ip;

		SFlashVarValue args[2] = { svr_name.c_str(), svr_ip.c_str() };
		m_pFlashBoard->CheckedInvoke("setServerInfo", args, 2);
	}

	bool drawTeamScores = false;

	int clientTeam = pGameRules->GetTeam(pClientActor->GetEntityId());
	bool notTeamed = (clientTeam != 1 && clientTeam != 2);
	if (notTeamed)		//Spectate mode
		clientTeam = 2;

	int teamCount = pGameRules->GetTeamCount();
	if (teamCount > 1)
	{
		if (clientTeam != m_currentClientTeam)
		{
			m_pFlashBoard->CheckedInvoke("setClientTeam", (clientTeam == 1) ? "korean" : "us");
			m_currentClientTeam = clientTeam;
		}

		//get the player rank, next rank, CP etc. from gamerules script
		//CryMP: This is still using lua, but needs only few updates, once a second should be enough..
		if (gEnv->pTimer->GetCurrTime() - m_lastUpdate > 1.0f)
		{
			m_rankStats.Update(pGameRulesScript, pClientActor, m_pFlashBoard);
			m_lastUpdate = gEnv->pTimer->GetCurrTime();
		}
		if (m_pHUD->GetCurrentGameRules() == EHUD_POWERSTRUGGLE)
		{
			drawTeamScores = (m_rankStats.currentRank != 0); // if we got here it means we don't want to show the number of kills
		}
		else
		{
			drawTeamScores = true;
		}
	}

	m_pFlashBoard->Invoke("clearEntries");

	std::vector<EntityId>::const_iterator start;
	std::vector<EntityId>::const_iterator end;
	std::vector<EntityId>* alreadySelected = NULL;
	if (m_pHUD->GetRadar())
	{
		alreadySelected = m_pHUD->GetRadar()->GetSelectedTeamMates();
		start = alreadySelected->begin();
		end = alreadySelected->end();
	}

	int usTeamPoints = 0;
	int koreanTeamPoints = 0;

	if (drawTeamScores)
	{
		const int TEAMSCORE_TEAM0_KEY = 10;
		pGameRules->GetSynchedGlobalValue(TEAMSCORE_TEAM0_KEY + 1, koreanTeamPoints);
		pGameRules->GetSynchedGlobalValue(TEAMSCORE_TEAM0_KEY + 2, usTeamPoints);
	}

	for (const auto& player : m_scoreBoard)
	{
		IEntity* pPlayer = gEnv->pEntitySystem->GetEntity(player.m_entityId);
		if (!pPlayer)
			continue;

		if ((teamCount > 1) && (player.m_team != 1 && player.m_team != 2))
			continue;

		/*const char* rank = 0;
		HSCRIPTFUNCTION pfnGetPlayerRank = 0;
		if (pGameRulesScript->GetValue("GetPlayerRankName", pfnGetPlayerRank) && pfnGetPlayerRank)
		{
			Script::CallReturn(gEnv->pScriptSystem, pfnGetPlayerRank, pGameRulesScript, ScriptHandle(player.m_entityId), rank);
			gEnv->pScriptSystem->ReleaseFunc(pfnGetPlayerRank);
		}*/

		const TSynchedKey RANK_KEY(202);
		int currentRank = 1;
		pGameRules->GetSynchedEntityValue(pPlayer->GetId(), RANK_KEY, currentRank);
		currentRank = MAX(currentRank, 1);
		string strRank;
		strRank.Format("@ui_short_rank_%d", currentRank);

		const char* playerName = pPlayer->GetName();
		std::wstring name = StringTools::ToWide(playerName ? playerName : "(null)");
		if (player.m_spectating)
		{
			name += L" (";
			name += m_pHUD->LocalizeWithParams("@ui_SPECTATE");
			name += L")";
		}

		SFlashVarValue pp(0);

		if (!notTeamed)
		{
			int playerPP = 0;
			pGameRules->GetSynchedEntityValue(player.m_entityId, PP_AMOUNT_KEY, playerPP);
			pp = SFlashVarValue(playerPP);
		}
		else
			pp = SFlashVarValue("---");

		bool selected = false;
		if (alreadySelected)
		{
			if (std::find(start, end, player.m_entityId) != end)
				selected = true;
		}

		IVoiceContext* pVoiceContext = gEnv->pGame->GetIGameFramework()->GetNetContext()->GetVoiceContext();
		bool muted = pVoiceContext->IsMuted(pClientActor->GetEntityId(), pPlayer->GetId());

		int team = (player.m_team == clientTeam) ? 1 : 2;
		if (player.m_spectating)
			team = 3;

		if (g_pGame->GetHUD()->GetCurrentGameRules() == EHUD_TEAMINSTANTACTION)
		{
			int playerScore = player.m_score;
			SFlashVarValue args[12] = { name.c_str(),
																	team,
																	(player.m_entityId == pClientActor->GetEntityId()) ? true : false,
																	playerScore,
																	player.m_kills,
																	player.m_teamKills,
																	player.m_deaths,
																	player.m_ping,
																	player.m_entityId,
																	player.m_alive ? 0 : 1,
																	selected,
																	muted };
			m_pFlashBoard->Invoke("addEntry", args, 12);
		}
		else
		{
			SFlashVarValue args[12] = { name.c_str(), team, (player.m_entityId == pClientActor->GetEntityId()) ? true : false, pp, player.m_kills, player.m_deaths, player.m_ping, player.m_entityId, strRank.c_str(), player.m_alive ? 0 : 1,selected, muted };
			m_pFlashBoard->Invoke("addEntry", args, 12);
		}
	}

	if (drawTeamScores)
		//set the teams scores in flash
	{
		SFlashVarValue argsA[2] = { 1, usTeamPoints };
		m_pFlashBoard->CheckedInvoke("setTeamPoints", argsA, 2);
		SFlashVarValue argsB[2] = { 2, koreanTeamPoints };
		m_pFlashBoard->CheckedInvoke("setTeamPoints", argsB, 2);
	}
	else
	{
		SFlashVarValue argsA[2] = { 1, "" };
		m_pFlashBoard->CheckedInvoke("setTeamPoints", argsA, 2);
		SFlashVarValue argsB[2] = { 2, "" };
		m_pFlashBoard->CheckedInvoke("setTeamPoints", argsB, 2);
	}

	EntityId playerBeingKicked = 0;
	m_pFlashBoard->Invoke("drawAllEntries", playerBeingKicked);
}

void CHUDScore::GetMemoryStatistics(ICrySizer* s)
{
	s->Add(*this);
	if (m_pFlashBoard)
		m_pFlashBoard->GetMemoryStatistics(s);
	s->AddContainer(m_scoreBoard);
}
