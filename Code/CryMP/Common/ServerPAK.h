#pragma once

#include <string>

struct ILevelInfo;

class ServerPAK
{
	std::string m_path;
	bool m_reloadResources = false;

public:
	ServerPAK();
	~ServerPAK();

	bool Load(const std::string & path);
	bool Unload();
	void OnLoadingStart(ILevelInfo* pLevel);
	void OnConnect();
	void OnInGame();
	void OnDisconnect(int reason, const char* message);
	void ResetSubSystems();
};
