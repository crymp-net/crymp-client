#pragma once

#include "CryCommon/CrySystem/ISystem.h"

class Launcher
{
	SSystemInitParams m_params = {};

	void SetCmdLine();
	void InitWorkingDirectory();
	void LoadEngine();
	void PatchEngine();
	void StartEngine();

public:
	Launcher() = default;

	void Run();

	static void OnEarlyEngineInit(ISystem* pSystem);
};
