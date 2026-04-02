/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Multiplayer lobby

-------------------------------------------------------------------------
History:
- 12/2006: Created by Stas Spivakov

*************************************************************************/
#ifndef __MULTIPLAYERMENU_H__
#define __MULTIPLAYERMENU_H__

#pragma once

#include "CryCommon/CrySystem/IFlashPlayer.h"
#include "MPHub.h"

struct  IServerBrowser;
struct  INetworkChat;
class   CGameNetworkProfile;
#include "MPLobbyUI.h"
struct  SStoredServer;

class CMultiPlayerMenu
{
private:
	struct SGSBrowser;
	struct SGSNetworkProfile;
	struct SChat;
	struct SCreateGame;

	class  CUI;
public:
	CMultiPlayerMenu(bool lan, IFlashPlayer* plr, CMPHub* hub);
	~CMultiPlayerMenu();
	void SetFlashPlayer(IFlashPlayer* pPlayer);
	bool HandleFSCommand(EGsUiCommand cmd, const char* pArgs);
	void OnUIEvent(const SUIEvent& event);
	void UpdateButtonModes();
	void UpdateServerList();
	bool IsConnectingToPopulatedServer() const
	{
		return m_connectingToPopulatedServer;
	}
		
private:
	void    DisplayServerList();
	void    SetServerListPos(double sb_pos);
	void    ChangeServerListPos(int dir);//either +1 or -1
	void    SetResumeEnabled(bool enable);
	void    StopServerListUpdate();
	void    SelectServer(int id);
	void    JoinServer();
	void    ServerListUpdated();
	void    OnRefreshComplete(bool ok);

	IServerBrowser* m_browser;
	CGameNetworkProfile* m_profile;
	INetworkChat* m_chat;
	std::unique_ptr<CUI> m_ui;
	std::unique_ptr<SGSBrowser> m_serverlist;
	std::unique_ptr<SChat> m_chatlist;
	std::unique_ptr<SGSNetworkProfile> m_buddylist;
	std::unique_ptr<SCreateGame> m_creategame;
	bool                        m_lan;
	std::vector<SStoredServer>  m_favouriteServers;
	std::vector<SStoredServer>  m_recentServers;
	CMPHub* m_hub;
	bool												m_joiningServer;

	EChatCategory               m_selectedCat;
	int                         m_selectedId;

	bool m_connectingToPopulatedServer = false;
};
#endif /*__MULTIPLAYERMENU_H__*/
