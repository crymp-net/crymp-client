#include <stdio.h>
#include <stdint.h>
#include <string_view>

#include <nlohmann/json.hpp>

#include "CryCommon/CrySystem/ISystem.h"
#include "Library/StringTools.h"

#include "ServerBrowser.h"
#include "ServerConnector.h"
#include "Client.h"

using json = nlohmann::json;

namespace
{
	bool GetBool(const json & object, const std::string_view & name)
	{
		auto it = object.find(name);
		if (it != object.end() && it->is_boolean())
			return it->get_ref<const bool&>();
		else
			return false;
	}

	int GetInt(const json & object, const std::string_view & name)
	{
		auto it = object.find(name);
		if (it != object.end())
		{
			if (it->is_number_integer())
				return static_cast<int>(it->get_ref<const int64_t&>());
			else if (it->is_number_unsigned())
				return static_cast<int>(it->get_ref<const uint64_t&>());
			else if (it->is_number_float())
				return static_cast<int>(it->get_ref<const double&>());
		}

		return 0;
	}

	std::string_view GetString(const json & object, const std::string_view & name)
	{
		auto it = object.find(name);
		if (it != object.end() && it->is_string())
			return it->get_ref<const std::string&>();
		else
			return std::string_view();
	}

	const char *GetCString(const json & object, const std::string_view & name)
	{
		auto it = object.find(name);
		if (it != object.end() && it->is_string())
			return it->get_ref<const std::string&>().c_str();
		else
			return "";
	}

	std::string IPToString(uint32_t ip)
	{
		const uint8_t byte0 = ip;
		const uint8_t byte1 = ip >> 8;
		const uint8_t byte2 = ip >> 16;
		const uint8_t byte3 = ip >> 24;

		return StringTools::Format("%hhu.%hhu.%hhu.%hhu", byte0, byte1, byte2, byte3);
	}

	uint32_t IPFromString(const char *ip)
	{
		uint8_t byte0 = 0;
		uint8_t byte1 = 0;
		uint8_t byte2 = 0;
		uint8_t byte3 = 0;

		sscanf(ip, "%hhu.%hhu.%hhu.%hhu", &byte0, &byte1, &byte2, &byte3);
		return (byte3 << 24) | (byte2 << 16) | (byte1 << 8) | byte0;
	}

	int GetTimeLeft(const json & serverInfo)
	{
		int minutes = 0;
		int seconds = 0;

		if (sscanf(GetCString(serverInfo, "timel"), "%d:%d", &minutes, &seconds) == 2) {
			return (minutes * 60) + seconds;
		} else {
			return 0;
		}
	}

	void ParseServerInfo(const json & serverInfo, ServerInfo & server)
	{
		server.local_host = GetString(serverInfo, "local_ip");
		server.local_port = GetInt(serverInfo, "local_port");
		server.public_host = GetString(serverInfo, "public_ip");
		server.public_port = GetInt(serverInfo, "public_port");
		server.name = GetString(serverInfo, "name");
		server.map = GetString(serverInfo, "map");
		server.SplitMapVersion();
		server.map_url = GetString(serverInfo, "mapdl");
		server.pak_url = GetString(serverInfo, "pak");
		server.FixMapUrl();
		server.FixPakUrl();
	}

	void SetBasicServerInfo(IServerListener *pListener, const json & serverInfo, int serverID, bool isUpdate)
	{
		SBasicServerInfo info = {};

		info.m_hostName   =              GetCString(serverInfo, "name");
		info.m_mapName    =              GetCString(serverInfo, "mapnm");
		info.m_gameType   =              GetCString(serverInfo, "rules");
		info.m_numPlayers =                  GetInt(serverInfo, "numpl");
		info.m_maxPlayers =                  GetInt(serverInfo, "maxpl");
		info.m_publicIP   = IPFromString(GetCString(serverInfo, "public_ip"));
		info.m_privateIP  = IPFromString(GetCString(serverInfo, "local_ip"));
		info.m_publicPort =                  GetInt(serverInfo, "public_port");
		info.m_hostPort   =                  GetInt(serverInfo, "local_port");
		info.m_official   =                  GetInt(serverInfo, "ranked") != 0;
		info.m_private    =               GetString(serverInfo, "pass") != "0";

		const std::string version = "1.1.1." + std::to_string(GetInt(serverInfo, "ver"));
		info.m_gameVersion = version.c_str();

		if (GetBool(serverInfo, "gs"))
		{
			info.m_mapName      = GetCString(serverInfo, "mapdnm");
			info.m_anticheat    =    GetBool(serverInfo, "anticheat");
			info.m_dedicated    =    GetBool(serverInfo, "dedicated");
			info.m_dx10         =    GetBool(serverInfo, "dx10");
			info.m_friendlyfire =    GetBool(serverInfo, "friendlyfire");
			info.m_gamepadsonly =    GetBool(serverInfo, "gamepadsonly");
			info.m_voicecomm    =    GetBool(serverInfo, "voicecomm");
		}

		// not used
		info.m_country = "";

		// TODO: add optional SSM info?
		info.m_modName = "";
		info.m_modVersion = "";

		if (isUpdate)
			pListener->UpdateServer(serverID, &info);
		else
			pListener->NewServer(serverID, &info);

		// custom stuff
		pListener->UpdateValue(serverID, "connectable", GetInt(serverInfo, "available") ? "1" : "0");
	}
}

bool ServerBrowser::OnPublicAddress(HTTPClientResult & result)
{
	CryLog("[CryMP] Public address (%d): %s", result.code, result.response.c_str());

	if (!result.error.empty())
	{
		return false;
	}

	try
	{
		const json publicAddress = json::parse(result.response);

		m_clientPublicAddress = GetString(publicAddress, "ip");
	}
	catch(const json::exception & ex)
	{
		CryLogAlways("$4[CryMP] Public address parse error: %s", ex.what());
		return false;
	}

	return true;
}

bool ServerBrowser::OnServerList(HTTPClientResult & result, const std::string & master)
{
	CryLog("[CryMP] Server list (%d): %s", result.code, result.response.c_str());

	try
	{
		const json serverList = json::parse(result.response);

		if (serverList.contains("error"))
		{
			CryLogAlways("$4[CryMP] Server list update error: %s", GetCString(serverList, "error"));
			return false;
		}

		for (const json & serverInfo : serverList)
		{
			const int serverID = static_cast<int>(m_servers.size());

			ServerInfo& server = m_servers.emplace_back();
			ParseServerInfo(serverInfo, server);
			server.master = master;
			server.is_local = server.public_host == m_clientPublicAddress;

			SetBasicServerInfo(m_pListener, serverInfo, serverID, false);
		}
	}
	catch (const json::exception & ex)
	{
		CryLogAlways("$4[CryMP] Server list parse error: %s", ex.what());
		return false;
	}

	return true;
}

bool ServerBrowser::OnServerInfo(HTTPClientResult & result, int serverID)
{
	CryLog("[CryMP] Server info (%d): %s", result.code, result.response.c_str());

	try
	{
		const json serverInfo = json::parse(result.response);

		if (serverInfo.contains("error"))
		{
			CryLogAlways("$4[CryMP] Server update error: %s", GetCString(serverInfo, "error"));
			return false;
		}

		ParseServerInfo(serverInfo, m_servers[serverID]);

		SetBasicServerInfo(m_pListener, serverInfo, serverID, true);

		m_pListener->UpdateValue(serverID, "hostname",              GetCString(serverInfo, "name"));
		m_pListener->UpdateValue(serverID, "mapname",               GetCString(serverInfo, "mapnm"));
		m_pListener->UpdateValue(serverID, "gametype",              GetCString(serverInfo, "rules"));
		m_pListener->UpdateValue(serverID, "numplayers", std::to_string(GetInt(serverInfo, "numpl")).c_str());
		m_pListener->UpdateValue(serverID, "maxplayers", std::to_string(GetInt(serverInfo, "maxpl")).c_str());
		m_pListener->UpdateValue(serverID, "password",              GetCString(serverInfo, "pass"));
		m_pListener->UpdateValue(serverID, "official",                  GetInt(serverInfo, "ranked") ? "1" : "0");

		const std::string version = "1.1.1." + std::to_string(GetInt(serverInfo, "ver"));
		m_pListener->UpdateValue(serverID, "gamever", version.c_str());

		const int timeLeft = GetTimeLeft(serverInfo);
		m_pListener->UpdateValue(serverID, "timelimit", timeLeft ? "1" : "0");
		m_pListener->UpdateValue(serverID, "timeleft", std::to_string(timeLeft).c_str());

		if (GetBool(serverInfo, "gs"))
		{
			m_pListener->UpdateValue(serverID, "mapname",   GetCString(serverInfo, "mapdnm"));
			m_pListener->UpdateValue(serverID, "anticheat",    GetBool(serverInfo, "anticheat")    ? "1" : "0");
			m_pListener->UpdateValue(serverID, "dedicated",    GetBool(serverInfo, "dedicated")    ? "1" : "0");
			m_pListener->UpdateValue(serverID, "dx10",         GetBool(serverInfo, "dx10")         ? "1" : "0");
			m_pListener->UpdateValue(serverID, "friendlyfire", GetBool(serverInfo, "friendlyfire") ? "1" : "0");
			m_pListener->UpdateValue(serverID, "gamepadsonly", GetBool(serverInfo, "gamepadsonly") ? "1" : "0");
			m_pListener->UpdateValue(serverID, "voicecomm",    GetBool(serverInfo, "voicecomm")    ? "1" : "0");

			int playerID = 0;

			for (const json & player : serverInfo["players"])
			{
				const char *name = GetCString(player, "name");

				const std::string team   = std::to_string(GetInt(player, "team"));
				const std::string rank   = std::to_string(GetInt(player, "rank"));
				const std::string kills  = std::to_string(GetInt(player, "kills"));
				const std::string deaths = std::to_string(GetInt(player, "deaths"));

				m_pListener->UpdatePlayerValue(serverID, playerID, "player", name);
				m_pListener->UpdatePlayerValue(serverID, playerID, "team", team.c_str());
				m_pListener->UpdatePlayerValue(serverID, playerID, "rank", rank.c_str());
				m_pListener->UpdatePlayerValue(serverID, playerID, "kills", kills.c_str());
				m_pListener->UpdatePlayerValue(serverID, playerID, "deaths", deaths.c_str());

				playerID++;
			}
		}
	}
	catch (const json::exception & ex)
	{
		CryLogAlways("$4[CryMP] Server info parse error: %s", ex.what());
		return false;
	}

	return true;
}

ServerBrowser::ServerBrowser()
{
}

ServerBrowser::~ServerBrowser()
{
}

void ServerBrowser::QueryClientPublicAddress()
{
	const std::string url = gClient->GetMasterServerAPI(gClient->GetMasters()[0]) + "/ip";

	gClient->HttpGet(url, [this](HTTPClientResult& result)
	{
		m_lastRequestSucceeded = this->OnPublicAddress(result);
	});
}

bool ServerBrowser::IsAvailable() const
{
	return true;
}

void ServerBrowser::Start(bool browseLAN)
{
}

void ServerBrowser::SetListener(IServerListener *pListener)
{
	m_pListener = pListener;
}

void ServerBrowser::Stop()
{
	m_servers.clear();
}

void ServerBrowser::Update()
{
	m_servers.clear();
	m_pendingQueryCount = static_cast<unsigned int>(gClient->GetMasters().size());

	unsigned int contractId = ++m_contract;

	for (const std::string& master : gClient->GetMasters())
	{
		const std::string url = gClient->GetMasterServerAPI(master) + "/servers?all&detailed&json";

		gClient->HttpGet(url, [this, master, contractId](HTTPClientResult& result)
		{
			if (contractId != m_contract)
			{
				return;
			}

			m_pendingQueryCount--;

			const bool success = result.error.empty();
			m_lastRequestSucceeded = success;

			if (m_pListener)
			{
				if (success)
				{
					OnServerList(result, master);
				}
				else
				{
					CryLogAlways("$4[CryMP] Server list update failed: %s", result.error.c_str());
				}

				if (m_pendingQueryCount == 0)
				{
					m_pListener->UpdateComplete(false);
				}
			}
		});
	}
}

void ServerBrowser::UpdateServerInfo(int id)
{
	if (id < 0 || id >= m_servers.size())
	{
		return;
	}

	const std::string url = gClient->GetMasterServerAPI(m_servers[id].master)
		+ "/server?ip=" + m_servers[id].public_host
		+ "&port=" + std::to_string(m_servers[id].public_port)
		+ "&json";

	gClient->HttpGet(url, [id, this](HTTPClientResult& result)
	{
		const bool success = result.error.empty();
		m_lastRequestSucceeded = success;

		if (m_pListener)
		{
			if (success)
			{
				if (OnServerInfo(result, id))
				{
					m_pListener->ServerUpdateComplete(id);
				}
				else
				{
					m_pListener->ServerUpdateFailed(id);
				}
			}
			else
			{
				CryLogAlways("$4[CryMP] Server update failed: %s", result.error.c_str());
			}
		}
	});
}

void ServerBrowser::BrowseForServer(unsigned int ip, unsigned short port)
{
}

void ServerBrowser::BrowseForServer(const char *address, unsigned short port)
{
}

void ServerBrowser::SendNatCookie(unsigned int ip, unsigned short port, int cookie)
{
}

void ServerBrowser::CheckDirectConnect(int id, unsigned short port)
{
	if (id < 0 || id >= m_servers.size())
	{
		return;
	}

	gClient->GetServerConnector()->Connect(m_servers[id]);
}

int ServerBrowser::GetServerCount()
{
	return m_servers.size();
}

int ServerBrowser::GetPendingQueryCount()
{
	return 0;
}
