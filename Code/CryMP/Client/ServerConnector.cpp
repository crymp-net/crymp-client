#include <string_view>

#include <nlohmann/json.hpp>

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CryAction/IGameFramework.h"
#include "CryCommon/CryNetwork/INetwork.h"
#include "CryGame/Game.h"
#include "CryGame/GameCVars.h"
#include "CryGame/Menus/FlashMenuObject.h"
#include "CryGame/Menus/MPHub.h"

#include "ServerConnector.h"
#include "Client.h"
#include "MapDownloader.h"
#include "FileCache.h"
#include "CryMP/Common/ServerPAK.h"

using json = nlohmann::json;

static std::string_view GetString(const json & object, const std::string_view & name)
{
	auto it = object.find(name);
	if (it != object.end() && it->is_string())
		return it->get_ref<const std::string&>();
	else
		return std::string_view();
}

bool ServerConnector::ParseServerInfo(HTTPClientResult & result)
{
	if (!result.error.empty())
	{
		CryLogAlways("$4[CryMP] Server check failed: %s", result.error.c_str());
		return false;
	}

	CryLog("[CryMP] Server info (%d): %s", result.code, result.response.c_str());

	try
	{
		const json serverInfo = json::parse(result.response);

		if (serverInfo.contains("error"))
		{
			CryLogAlways("$4[CryMP] Server check error: %s", GetString(serverInfo, "error").data());
			return false;
		}

		m_server.name = GetString(serverInfo, "name");
		m_server.map = GetString(serverInfo, "map");
		m_server.map_url = GetString(serverInfo, "mapdl");
		m_server.pak_url = GetString(serverInfo, "pak");
	}
	catch (const json::exception & ex)
	{
		CryLogAlways("$4[CryMP] Server info parse error: %s", ex.what());
		return false;
	}

	m_server.SplitMapVersion();
	m_server.FixMapUrl();
	m_server.FixPakUrl();

	return true;
}

void ServerConnector::SetLoadingDialogText(const char *text)
{
	CMPHub *pMPHub = SAFE_MENU_FUNC_RET(GetMPHub());
	if (pMPHub)
	{
		pMPHub->SetLoadingDlgText(text, false);
	}
}

void ServerConnector::SetLoadingDialogText(const char *label, const char *param)
{
	CMPHub *pMPHub = SAFE_MENU_FUNC_RET(GetMPHub());
	if (pMPHub)
	{
		pMPHub->SetLoadingDlgText(label, param);
	}
}

void ServerConnector::ShowErrorBox(const char *text)
{
	CMPHub *pMPHub = SAFE_MENU_FUNC_RET(GetMPHub());
	if (pMPHub)
	{
		pMPHub->CloseLoadingDlg();
		pMPHub->ShowError(text);
	}
}

void ServerConnector::ResetCVars()
{
	SCVars *pGameCVars = g_pGame->GetCVars();

	pGameCVars->mp_crymp = 0;
	pGameCVars->mp_circleJump = 0.0f;
	pGameCVars->mp_wallJump = 1.0f;
	pGameCVars->mp_flyMode = 0;
	pGameCVars->mp_pickupObjects = 0;
	pGameCVars->mp_pickupVehicles = 0;
	pGameCVars->mp_pickupMassLimit = 0;
	pGameCVars->mp_pickupThrowMult = 1.0f;
	pGameCVars->mp_pickupMaxVehicleCollisions = 0;
	pGameCVars->mp_thirdPerson = 1;
	pGameCVars->mp_rpgMod = 0;
	pGameCVars->mp_radioTagging = 0;
	pGameCVars->mp_healthBars = 0;
	pGameCVars->mp_deadPlayersOnMinimap = 0;
	pGameCVars->mp_netSerializeHolsteredItems = 0;
}

void ServerConnector::Step1_RequestServerInfo()
{
	CryLogAlways("$3[CryMP] Checking server at $6%s:%hu$3", m_server.public_host.c_str(), m_server.public_port);

	const std::string url = gClient->GetMasterServerAPI(m_server.master)
		+ "/server?ip=" + m_server.public_host
		+ "&port=" + std::to_string(m_server.public_port)
		+ "&json";

	gClient->HttpGet(url, [contractID = m_contractID, this](HTTPClientResult& result)
	{
		if (contractID == m_contractID)
		{
			if (ParseServerInfo(result))
			{
				// next step
				Step2_DownloadMap();
			}
			else
			{
				// don't give up
				Step4_TryConnect();
			}
		}
	});
}

void ServerConnector::Step2_DownloadMap()
{
	MapDownloaderRequest request;
	request.map = m_server.map;
	request.mapURL = m_server.map_url;
	request.mapVersion = m_server.map_version;

	request.onProgress = [contractID = m_contractID, this](const std::string & message) -> bool
	{
		if (contractID == m_contractID)
		{
			SetLoadingDialogText(message.c_str());

			// continue download
			return true;
		}
		else
		{
			// cancel download
			return false;
		}
	};

	request.onComplete = [contractID = m_contractID, this](MapDownloaderResult & result)
	{
		if (contractID == m_contractID)
		{
			if (result.success)
			{
				// next step
				Step3_DownloadPAK();
			}
			else
			{
				ShowErrorBox("Map download failed.");
			}
		}
	};

	gClient->GetMapDownloader()->Request(std::move(request));
}

void ServerConnector::Step3_DownloadPAK()
{
	if (m_server.pak_url.empty())
	{
		// next step
		Step4_TryConnect();
	}
	else
	{
		CryLogAlways("$3[CryMP] Obtaining server PAK...");

		FileCacheRequest request;
		request.fileURL = m_server.pak_url;
		request.fileType = "PAK";

		request.onProgress = [contractID = m_contractID, this](const std::string & message) -> bool
		{
			if (contractID == m_contractID)
			{
				SetLoadingDialogText(message.c_str());

				// continue download
				return true;
			}
			else
			{
				// cancel download
				return false;
			}
		};

		request.onComplete = [contractID = m_contractID, this](FileCacheResult & result)
		{
			if (contractID == m_contractID)
			{
				if (result.success)
				{
					if (gClient->GetServerPAK()->Load(result.filePath.string()))
					{
						// next step
						Step4_TryConnect();
					}
					else
					{
						ShowErrorBox("Server PAK load failed.");
					}
				}
				else
				{
					ShowErrorBox("Server PAK download failed.");
				}
			}
		};

		gClient->GetFileCache()->Request(std::move(request));
	}
}

void ServerConnector::Step4_TryConnect()
{
	const std::string endpoint = m_server.CreateEndpointString();

	if (m_server.name.empty())
	{
		CryLogAlways("$3[CryMP] Joining unknown server at $6%s$3", endpoint.c_str());

		SetLoadingDialogText("@ui_connecting_to", endpoint.c_str());
	}
	else
	{
		CryLogAlways("$3[CryMP] Joining $6%s$3 at $6%s$3", m_server.name.c_str(), endpoint.c_str());

		SetLoadingDialogText("@ui_connecting_to", m_server.name.c_str());
	}

	gClient->GetServerPAK()->ResetSubSystems();

	ResetCVars();

	SGameStartParams params;
	params.flags = eGSF_Client | eGSF_NoDelayedStart | eGSF_NoQueries | eGSF_ImmersiveMultiplayer;
	params.hostname = m_server.GetPreferredHost().c_str();
	params.port = m_server.GetPreferredPort();

	gClient->GetGameFramework()->StartGameContext(&params);
}

ServerConnector::ServerConnector()
{
}

ServerConnector::~ServerConnector()
{
}

void ServerConnector::Connect(const ServerInfo& server)
{
	m_server = server;

	Reconnect();
}

void ServerConnector::Reconnect()
{
	Disconnect();

	CMPHub* pMPHub = SAFE_MENU_FUNC_RET(GetMPHub());
	if (pMPHub)
	{
		pMPHub->ShowLoadingDlg("@ui_connecting_to",
			m_server.name.empty() ? m_server.CreateEndpointString().c_str() : m_server.name.c_str());
	}

	gClient->GetServerPAK()->OnConnect();

	IConsole *pConsole = gEnv->pConsole;

	pConsole->GetCVar("cl_serveraddr")->Set(m_server.GetPreferredHost().c_str());
	pConsole->GetCVar("cl_serverport")->Set(static_cast<int>(m_server.GetPreferredPort()));

	Step1_RequestServerInfo();
}

void ServerConnector::Disconnect()
{
	m_contractID++;

	if (!gEnv->bServer)
	{
		INetChannel *pClientChannel = gClient->GetGameFramework()->GetClientChannel();
		if (pClientChannel)
		{
			pClientChannel->Disconnect(eDC_UserRequested, "User left the game");
		}
	}

	gClient->GetGameFramework()->EndGameContext();
}
