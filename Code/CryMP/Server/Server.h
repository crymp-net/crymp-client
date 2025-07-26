#pragma once

#include <memory>
#include <optional>
#include <string>

#include "CryCommon/CryAction/IGameFramework.h"

class Executor;
class HTTPClient;

class ScriptBind_CPPAPI;

class Server : public IGameFrameworkListener
{
public:
	IGame *pGame = nullptr;
	IGameFramework* pGameFramework = nullptr;

	std::unique_ptr<Executor> pExecutor;
	std::unique_ptr<HTTPClient> pHttpClient;

	std::unique_ptr<ScriptBind_CPPAPI> m_pScriptBind_CPPAPI;

	Server();
	~Server();

	void Init(IGameFramework* pGameFramework);
	void UpdateLoop();
	
	void HttpRequest(HTTPClientRequest&& request);

private:
	// IGameFrameworkListener
	void OnPostUpdate(float deltaTime) override;
	void OnSaveGame(ISaveGame* saveGame) override;
	void OnLoadGame(ILoadGame* loadGame) override;
	void OnLevelEnd(const char* nextLevel) override;
	void OnActionEvent(const SActionEvent& event) override;
};

///////////////////////
inline Server* gServer;
///////////////////////
