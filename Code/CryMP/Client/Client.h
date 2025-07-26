#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "CryCommon/CryAction/IGameFramework.h"
#include "CryCommon/CryAction/ILevelSystem.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryMP/Common/HTTPClient.h"

struct IConsoleCmdArgs;

class Executor;
class FileDownloader;
class FileCache;
class MapDownloader;
class GSMasterHook;
class ScriptCommands;
class ScriptCallbacks;
class ScriptBind_CPPAPI;
class ServerBrowser;
class ServerConnector;
class ServerPAK;
class EngineCache;
class ParticleManager;
class DrawTools;

class Client : public IGameFrameworkListener, public ILevelSystemListener, public IEntitySystemSink
{
	IGameFramework *m_pGameFramework = nullptr;
	IGame *m_pGame = nullptr;

	std::unique_ptr<Executor> m_pExecutor;
	std::unique_ptr<HTTPClient> m_pHTTPClient;
	std::unique_ptr<FileDownloader> m_pFileDownloader;
	std::unique_ptr<FileCache> m_pFileCache;
	std::unique_ptr<MapDownloader> m_pMapDownloader;
	std::unique_ptr<GSMasterHook> m_pGSMasterHook;
	std::unique_ptr<ScriptCommands> m_pScriptCommands;
	std::unique_ptr<ScriptCallbacks> m_pScriptCallbacks;
	std::unique_ptr<ScriptBind_CPPAPI> m_pScriptBind_CPPAPI;
	std::unique_ptr<ServerBrowser> m_pServerBrowser;
	std::unique_ptr<ServerConnector> m_pServerConnector;
	std::unique_ptr<ServerPAK> m_pServerPAK;
	std::unique_ptr<EngineCache> m_pEngineCache;
	std::unique_ptr<ParticleManager> m_pParticleManager;
	std::unique_ptr<DrawTools> m_pDrawTools;

	std::string m_hwid;
	std::string m_locale;
	std::string m_timezone;

	std::vector<std::string> m_masters;

	struct KeyBind
	{
		std::string key;
		std::string command;
		SmartScriptFunction function;
		bool createdInGame = false;
	};

	std::vector<KeyBind> m_keyBinds;

	EntityId m_lastSpawnId = 0;

	void InitMasters();
	void SetVersionInLua();

	std::string GenerateRandomCDKey();
	void SetRandomCDKey();

	static void OnConnectCmd(IConsoleCmdArgs *pArgs);
	static void OnReconnectCmd(IConsoleCmdArgs *pArgs);
	static void OnDisconnectCmd(IConsoleCmdArgs* pArgs);
	static void OnKeyBindCmd(IConsoleCmdArgs* pArgs);
	static void OnDumpKeyBindsCmd(IConsoleCmdArgs* pArgs);

public:
	Client();
	~Client();

	void Init(IGameFramework *pGameFramework);

	void UpdateLoop();

	void HttpGet(const std::string_view& url, std::function<void(HTTPClientResult&)> callback);
	void HttpRequest(HTTPClientRequest&& request);

	std::string GetMasterServerAPI(const std::string & master);

	void AddKeyBind(const std::string_view& key, const std::string_view& command);
	void AddKeyBind(const std::string_view& key, HSCRIPTFUNCTION function);
	void OnKeyPress(const std::string_view& key);
	void OnKeyRelease(const std::string_view& key);
	void ClearKeyBinds();

	// IGameFrameworkListener
	void OnPostUpdate(float deltaTime) override;
	void OnSaveGame(ISaveGame *pSaveGame) override;
	void OnLoadGame(ILoadGame *pLoadGame) override;
	void OnLevelEnd(const char *nextLevel) override;
	void OnActionEvent(const SActionEvent & event) override;

	// ILevelSystemListener
	void OnLevelNotFound(const char *levelName) override;
	void OnLoadingStart(ILevelInfo *pLevel) override;
	void OnLoadingComplete(ILevel *pLevel) override;
	void OnLoadingError(ILevelInfo *pLevel, const char *error) override;
	void OnLoadingProgress(ILevelInfo *pLevel, int progressAmount) override;

	// IEntitySystemSink
	bool OnBeforeSpawn(SEntitySpawnParams & params) override;
	void OnSpawn(IEntity *pEntity, SEntitySpawnParams & params) override;
	bool OnRemove(IEntity *pEntity) override;
	void OnEvent(IEntity *pEntity, SEntityEvent & event) override;

	IGameFramework *GetGameFramework()
	{
		return m_pGameFramework;
	}

	Executor *GetExecutor()
	{
		return m_pExecutor.get();
	}

	HTTPClient *GetHTTPClient()
	{
		return m_pHTTPClient.get();
	}

	FileDownloader *GetFileDownloader()
	{
		return m_pFileDownloader.get();
	}

	FileCache *GetFileCache()
	{
		return m_pFileCache.get();
	}

	MapDownloader *GetMapDownloader()
	{
		return m_pMapDownloader.get();
	}

	GSMasterHook *GetGSMasterHook()
	{
		return m_pGSMasterHook.get();
	}

	ScriptCommands *GetScriptCommands()
	{
		return m_pScriptCommands.get();
	}

	ScriptCallbacks *GetScriptCallbacks()
	{
		return m_pScriptCallbacks.get();
	}

	ScriptBind_CPPAPI *GetScriptBind_CPPAPI()
	{
		return m_pScriptBind_CPPAPI.get();
	}

	ServerBrowser *GetServerBrowser()
	{
		return m_pServerBrowser.get();
	}

	ServerConnector *GetServerConnector()
	{
		return m_pServerConnector.get();
	}

	ServerPAK *GetServerPAK()
	{
		return m_pServerPAK.get();
	}

	EngineCache* GetEngineCache()
	{
		return m_pEngineCache.get();
	}

	DrawTools* GetDrawTools()
	{
		return m_pDrawTools.get();
	}

	const std::vector<std::string> & GetMasters() const
	{
		return m_masters;
	}

	EntityId GetLastSpawnId() const
	{
		return m_lastSpawnId;
	}

private:

	enum Timers
	{
		PHYSICS_POS_SYNC = 7777,
	};

	void SynchWithPhysicsPosition(IEntity* pEntity);
};

///////////////////////
inline Client *gClient;
///////////////////////
