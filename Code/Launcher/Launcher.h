#pragma once

#include "CryCommon/CrySystem/ISystem.h"

class Launcher
{
	struct DLLs
	{
		// do not try to unload these DLLs as it might crash
		void* pCry3DEngine = nullptr;
		void* pCryAction = nullptr;
		void* pCryAISystem = nullptr;
		void* pCryNetwork = nullptr;
		void* pCryPhysics = nullptr;
		void* pCrySystem = nullptr;
		void* pCryRenderD3D9 = nullptr;
		void* pCryRenderD3D10 = nullptr;
		void* pCryRenderNULL = nullptr;
		void* pFmodEx = nullptr;
	};

	DLLs m_dlls;
	SSystemInitParams m_params = {};

	void SetCmdLine();
	void InitWorkingDirectory();
	void LoadEngine();
	void PatchEngine();
	void StartEngine();

public:
	Launcher();
	~Launcher();

	void OnEarlyEngineInit(ISystem* pSystem);

	const DLLs& GetDLLs()
	{
		return m_dlls;
	}

	const SSystemInitParams& GetParams() const
	{
		return m_params;
	}

	void Run();
};

///////////////////////////
inline Launcher* gLauncher;
///////////////////////////
