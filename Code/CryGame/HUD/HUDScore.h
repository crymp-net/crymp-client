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
#ifndef __HUDSCORE_H__
#define __HUDSCORE_H__

#include "CryCommon/CryEntitySystem/EntityId.h"

#include "HUDObject.h"

struct IActor;
class CGameFlashAnimation;

class CHUDScore : public CHUDObject
{
	friend class CHUD;

	struct ScoreEntry
	{
		int					m_kills = 0;
		int					m_teamKills = 0;
		int					m_score = 0;
		int					m_deaths = 0;
		int					m_ping = 0;
		int					m_team = -1;
		int					m_currentRank = 0;
		EntityId			m_entityId = 0;
		bool				m_alive = false;
		bool				m_spectating = false; //updating during sorting
		bool				m_prioritizeTeamKills = false;

		ScoreEntry(EntityId id, int kills, int deaths, int ping, int teamKills = 0, int playerScore = 0, bool prioTeamKills = false);
		bool operator<(const ScoreEntry& entry) const;
		void UpdateLiveStats();
	};

public:
	struct SRankStats
	{
		int currentRank;
		const char* nextRankName;
		const char* currentRankName;
		int currentRankCP;
		int nextRankCP;
		int currentRankPP;
		int nextRankPP;
		int playerCP;

		SRankStats()
		{
			currentRank = 0;
			nextRankName = 0;
			currentRankName = 0;
			currentRankCP = 0;
			nextRankCP = 0;
			currentRankPP = 0;
			nextRankPP = 0;
			playerCP = 0;
		}

		void Update(IScriptTable* pGameRulesScript, IActor* pActor, CGameFlashAnimation* pFlashBoard = 0);
	};

	CHUDScore(CHUD* pHUD)
	{
		m_pHUD = pHUD;
		m_bShow = false;
		m_lastUpdate = 0.0f;
		m_lastShowSwitch = 0;
		m_pFlashBoard = NULL;
		m_currentClientTeam = -1;
	}

	~CHUDScore()
	{
	}

	virtual void Update(float fDeltaTime);

	virtual bool OnInputEvent(const SInputEvent &event )
	{
		return false;
	}

	std::vector<ScoreEntry> m_scoreBoard;

	void Reset();
	void AddEntry(EntityId player, int kills, int deaths, int ping);

	void GetMemoryStatistics(ICrySizer * s);

	ILINE void SetVisible(bool visible, CGameFlashAnimation *board = NULL)
	{
		m_bShow = visible;
		m_pFlashBoard = board;
		m_lastShowSwitch = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	}
private:

	CHUD* m_pHUD;
	float			m_lastUpdate;
	int				m_currentClientTeam;
	bool			m_bShow;
	float			m_lastShowSwitch;
	std::string m_currentServer;
	std::string m_currentServerIp;
	CGameFlashAnimation *m_pFlashBoard;
	SRankStats m_rankStats;

	void Render();
};

#endif
