/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: TeamInstantAction specific HUD object

-------------------------------------------------------------------------
History:
- 03/18/2008: Created by Jan Neugebauer

*************************************************************************/

#ifndef HUD_TEAMINSTANTACTION_H
#define HUD_TEAMINSTANTACTION_H

# pragma once


#include "HUDObject.h"

#include "HUD.h"

class CGameFlashAnimation;

class CHUDTeamInstantAction : public CHUDObject
{
	friend class CHUD;
public:

	CHUDTeamInstantAction(CHUD *pHUD);
	~CHUDTeamInstantAction();

	enum ETIAScoreHideReason
	{
		eTIAScoreHideReason_None = 0,
		eTIAScoreHideReason_PDA = BIT(0),
		eTIAScoreHideReason_Binoculars = BIT(1),
	};

	void Update(float fDeltaTime);
	void Reset();
	void SetHUDColor();
	void UpdateStats();

	void Show(bool show);
	void SetTIAScoreHidden(ETIAScoreHideReason reason, bool show);

//	virtual bool IsFactoryType(EntityId entity, EBuyMenuPage type);

//	bool IsPlayerSpecial();

	int m_ownTeamScore;
	int m_enemyTeamScore;
	int m_ownScore;
	int m_ownTeam;

private:

	void PushToFlash();
	CGameFlashAnimation m_animTIAScore;
	CHUD *g_pHUD;

	int m_roundTime;
	int m_scoreLimit;

	int m_tiaScoreHideFlags = 0;
};

#endif
