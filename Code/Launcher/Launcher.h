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

#ifdef EDITOR_LAUNCHER
    void LoadEditor();
    void PatchEditor();
    int StartEditor(char* cmdLine);
#endif

public:
    Launcher() = default;

    int Run(char* cmdLine = nullptr);

    static void OnEarlyEngineInit(ISystem* pSystem);
};
