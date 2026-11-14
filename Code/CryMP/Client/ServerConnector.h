#pragma once

#include <string>

#include "ServerInfo.h"

struct HTTPClientResult;

class ServerConnector
{
	ServerInfo m_server;
	unsigned int m_contractID = 0;

	bool ParseServerInfo(HTTPClientResult & result);
	void SetLoadingDialogText(const char *text);
	void SetLoadingDialogText(const char *label, const char *param);  // for localized stuff
	void ShowErrorBox(const char *text);
	void ResetCVars();

	void Step1_RequestServerInfo();
	void Step2_DownloadMap();
	void Step3_DownloadPAK();
	void Step4_TryConnect();

public:
	ServerConnector();
	~ServerConnector();

	void Connect(const ServerInfo& server);
	void Reconnect();
	void Disconnect();
    void OnDisconnect();

	const ServerInfo& GetLastServer() const
	{
		return m_server;
	}
};
