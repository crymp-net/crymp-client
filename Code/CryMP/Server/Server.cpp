#include <tracy/Tracy.hpp>

#include "CryMP/Common/Executor.h"
#include "CryMP/Common/HTTPClient.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryGame/Game.h"
#include "CrySystem/Logger.h"
#include "Library/StringTools.h"
#include "Library/WinAPI.h"

#include "Server.h"

#include "CryMP/Common/ScriptBind_CPPAPI.h"

#include "CryMP/Server/SSM.h"
#include "CryMP/Server/SafeWriting/SafeWriting.h"

Server::Server()
{
}

Server::~Server()
{
}

void Server::Init(IGameFramework* pGameFramework)
{
	this->pGameFramework = pGameFramework;

	this->pExecutor = std::make_unique<Executor>();
	this->pHttpClient = std::make_unique<HTTPClient>(*this->pExecutor);

	pGameFramework->RegisterListener(this, "crymp-server", FRAMEWORKLISTENERPRIORITY_DEFAULT);

	CGame* cGame = NULL;

	if (WinAPI::CmdLine::HasArg("-oldgame"))
	{
		void* pCryGame = WinAPI::DLL::Load("CryGame.dll");
		if (!pCryGame)
		{
			throw StringTools::SysErrorFormat("Failed to load the CryGame DLL!");
		}

		auto entry = static_cast<IGame::TEntryFunction>(WinAPI::DLL::GetSymbol(pCryGame, "CreateGame"));
		if (!entry)
		{
			throw StringTools::ErrorFormat("The CryGame DLL is not valid!");
		}

		this->pGame = entry(pGameFramework);
	}
	else
	{
		cGame = new CGame();
		this->pGame = cGame;
	}

	// initialize the game
	// mods are not supported
	this->pGame->Init(pGameFramework);

	if (cGame) {
		if (WinAPI::CmdLine::HasArg("-ssm")) {
			std::string ssm{ WinAPI::CmdLine::GetArgValue("-ssm") };
			CryLogAlways("$6[CryMP] Detected SSM: %s", ssm.c_str());
			if (ssm == "SafeWriting") {
				cGame->SetSSM(new CSafeWriting(pGameFramework, pGameFramework->GetISystem()));
			}
		}
	}

	m_pScriptBind_CPPAPI = std::make_unique<ScriptBind_CPPAPI>();
}

void Server::UpdateLoop()
{
	gEnv->pConsole->ExecuteString("exec autoexec.cfg");

	const bool haveFocus = true;
	const unsigned int updateFlags = 0;

	while (this->pGame->Update(haveFocus, updateFlags))
	{
		Logger::GetInstance().OnUpdate();

		FrameMark;
	}
}

void Server::OnPostUpdate(float deltaTime)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	this->pExecutor->OnUpdate();
	if (g_pGame && g_pGame->GetSSM()) {
		g_pGame->GetSSM()->Update(deltaTime);
	}
}

void Server::OnSaveGame(ISaveGame* saveGame)
{
}

void Server::OnLoadGame(ILoadGame* loadGame)
{
}

void Server::OnLevelEnd(const char* nextLevel)
{
}

void Server::OnActionEvent(const SActionEvent& event)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	switch (event.m_event)
	{
		case eAE_channelCreated:
		case eAE_channelDestroyed:
		case eAE_connectFailed:
		case eAE_connected:
		case eAE_disconnected:
		case eAE_clientDisconnected:
		case eAE_resetBegin:
		case eAE_resetEnd:
		case eAE_resetProgress:
		case eAE_preSaveGame:
		case eAE_postSaveGame:
		case eAE_inGame:
		case eAE_serverName:
		case eAE_serverIp:
		case eAE_earlyPreUpdate:
		{
			break;
		}
	}
}

void Server::HttpRequest(HTTPClientRequest&& request)
{
	pHttpClient->Request(std::move(request));
}
