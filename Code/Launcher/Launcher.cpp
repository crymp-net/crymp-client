#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <tracy/Tracy.hpp>

#include "Cry3DEngine/TimeOfDay.h"
#include "CryAction/GameFramework.h"
#include "CryCommon/CryAction/IGameFramework.h"
#include "CryCommon/CryNetwork/INetworkService.h"
#include "CryCommon/CrySystem/FrameProfiler.h"
#include "CryCommon/CrySystem/gEnv.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/ICryPak.h"
#include "CryMP/Client/Client.h"
#include "CryMP/Server/Server.h"
#include "CryPhysics/CryPhysics.h"
#include "CryScriptSystem/ScriptSystem.h"
#include "CrySoundSystem/CrySoundSystem.h"
#include "CrySystem/CPUInfo.h"
#include "CrySystem/CrashTest.h"
#include "CrySystem/CryMemoryManager.h"
#include "CrySystem/CryPak.h"
#include "CrySystem/GameWindow.h"
#include "CrySystem/HardwareMouse.h"
#include "CrySystem/LocalizationManager.h"
#include "CrySystem/Logger.h"
#include "CrySystem/RandomGenerator.h"
#include "CrySystem/StreamEngine.h"
#include "Library/CodeWall.h"
#include "Library/CrashLogger.h"
#include "Library/StringTools.h"
#include "Library/WinAPI.h"

#include "DsoalDeployer.h"
#include "Launcher.h"
#include "MemoryPatch.h"
#include "Resources.h"
#include "StartupTime.h"

#include "config.h"

std::uintptr_t CRYACTION_BASE = 0;

void* g_pCry3DEngine = nullptr;
void* g_pCryAction = nullptr;
void* g_pCryAISystem = nullptr;
void* g_pCryNetwork = nullptr;
void* g_pCrySystem = nullptr;
void* g_pCryRenderD3D9 = nullptr;
void* g_pCryRenderD3D10 = nullptr;
void* g_pCryRenderNULL = nullptr;
void* g_pCryMovie = nullptr;
void* g_pCryInput = nullptr;
void* g_pCryFont = nullptr;
void* g_pCryAnimation = nullptr;
void* g_pCryEntitySystem = nullptr;
void* g_pCrySoundSystem = nullptr;
void* g_pCryPhysics = nullptr;
void* g_pCompressATI2 = nullptr;
void* g_pFmodEx = nullptr;
void* g_pFmodEvent = nullptr;
void* g_pFmodEventNet = nullptr;
void* g_pD3DCompiler = nullptr;
bool g_hasC1Fmod = false;
bool g_hasWarheadWarsFmod = false;

static void InitCrySystem(void* pCrySystem, SSystemInitParams& params)
{
	using CrySystemEntry = ISystem* (*)(SSystemInitParams&);

	auto entry = static_cast<CrySystemEntry>(WinAPI::DLL::GetSymbol(pCrySystem, "CreateSystemInterface"));
	if (!entry)
	{
		throw StringTools::ErrorFormat("The CrySystem DLL is not valid!");
	}

	if (!entry(params))
	{
		throw StringTools::ErrorFormat("CrySystem initialization failed!");
	}
}

static void LogBytes(const char* message, std::size_t bytes)
{
	const char* unit = "";
	char units[6][2] = { "K", "M", "G", "T", "P", "E" };

	for (int i = 0; i < 6 && bytes >= 1024; i++)
	{
		unit = units[i];
		bytes /= 1024;
	}

	CryLogAlways("%s%zu%s", message, bytes, unit);
}

static void OnD3D9Info(MemoryPatch::CryRenderD3D9::AdapterInfo* info)
{
	CryLogAlways("D3D9 Adapter: %s", info->description);
	CryLogAlways("D3D9 Adapter: PCI %04x:%04x (rev %02x)", info->vendor_id, info->device_id, info->revision);

	// no memory info available
}

static void OnD3D10Info(MemoryPatch::CryRenderD3D10::AdapterInfo* info)
{
	CryLogAlways("D3D10 Adapter: %ls", info->description);
	CryLogAlways("D3D10 Adapter: PCI %04x:%04x (rev %02x)", info->vendor_id, info->device_id, info->revision);

	LogBytes("D3D10 Adapter: Dedicated video memory = ", info->dedicated_video_memory);
	LogBytes("D3D10 Adapter: Dedicated system memory = ", info->dedicated_system_memory);
	LogBytes("D3D10 Adapter: Shared system memory = ", info->shared_system_memory);
}

static bool OnD3D10Init(MemoryPatch::CryRenderD3D10::SystemAPI* api)
{
	void* d3d10 = WinAPI::DLL::Load("d3d10.dll");
	if (!d3d10)
	{
		WinAPI::ErrorBox(StringTools::SysErrorFormat("Failed to load the D3D10 DLL!").what());
		return false;
	}

	api->pD3D10 = d3d10;
	api->pD3D10CreateDevice = WinAPI::DLL::GetSymbol(d3d10, "D3D10CreateDevice");

	void* dxgi = WinAPI::DLL::Load("dxgi.dll");
	if (!dxgi)
	{
		WinAPI::ErrorBox(StringTools::SysErrorFormat("Failed to load the DXGI DLL!").what());
		return false;
	}

	api->pDXGI = dxgi;
	api->pCreateDXGIFactory = WinAPI::DLL::GetSymbol(dxgi, "CreateDXGIFactory");

	return true;
}

static void OnCryWarning(int, int, const char* format, ...)
{
	// the original buffer size
	char buffer[4096];

	va_list args;
	va_start(args, format);
	StringTools::FormatToV(buffer, sizeof(buffer), format, args);
	va_end(args);

	CryLogWarning("%s", buffer);
}

static void OnGameWarning(const char* format, ...)
{
	// the original buffer size
	char buffer[4096];

	va_list args;
	va_start(args, format);
	StringTools::FormatToV(buffer, sizeof(buffer), format, args);
	va_end(args);

	CryLogWarning("%s", buffer);
}

static void EarlyEngineInitHook(ISystem* pSystem, IConsole* pConsole, ISystemUserCallback* pUserCallback)
{
	gEnv = pSystem->GetGlobalEnvironment();
	gEnv->pConsole = pConsole;

	if (pUserCallback)
	{
		// dedicated server console
		pUserCallback->OnInit(pSystem);
	}

	Launcher::OnEarlyEngineInit(pSystem);
}

static void InstallEarlyEngineInitHook(void* pCrySystem)
{
	void* pHook = &EarlyEngineInitHook;

#ifdef BUILD_64BIT
	const std::size_t codeOffset = 0x462F3;
	const std::size_t codeSize = 0x19;

	unsigned char code[] = {
		0x48, 0x8B, 0xD0,                                            // mov rdx, rax
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x00
		0x48, 0x8B, 0xCD,                                            // mov rcx, rbp
		0x4C, 0x8B, 0x85, 0x30, 0x0C, 0x00, 0x00,                    // mov r8, qword ptr ss:[rbp+0xC30]
		0xFF, 0xD0,                                                  // call rax
	};

	std::memcpy(&code[5], &pHook, 8);
#else
	const std::size_t codeOffset = 0x59B3E;
	const std::size_t codeSize = 0xF;

	unsigned char code[] = {
		0x51,                          // push ecx
		0x50,                          // push eax
		0x55,                          // push ebp
		0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0x0
		0xFF, 0xD0,                    // call eax
		0x83, 0xC4, 0x0C,              // add esp, 0xC
		0x90,                          // nop
		0x90,                          // nop
	};

	std::memcpy(&code[4], &pHook, 4);
#endif

	static_assert(sizeof(code) == codeSize);

	WinAPI::FillMem(WinAPI::RVA(pCrySystem, codeOffset), code, sizeof(code));
}

struct CryPakConfig
{
};

static ICryPak* CreateNewCryPak(ISystem* pSystem, CryPakConfig* config, bool lvlRes, bool gameFolderWritable)
{
	// earlier than EarlyEngineInitHook
	gEnv = pSystem->GetGlobalEnvironment();

	// dropped because neither log file nor console are available at this point
	CryLogAlways("$3[CryMP] Initializing CryPak");

	const auto internalPak = WinAPI::GetDataResource(nullptr, RESOURCE_INTERNAL_PAK);

	CryPak* pCryPak = &CryPak::GetInstance();
	// TODO: config
	// TODO: lvlRes
	pCryPak->SetGameFolderWritable(gameFolderWritable);
	pCryPak->LoadClientPak(internalPak.data(), internalPak.size());

	return pCryPak;
}

static void ReplaceCryPak(void* pCrySystem)
{
	void* pFactory = CreateNewCryPak;

#ifdef BUILD_64BIT
	const std::size_t codeOffset = 0x446A8;
	const std::size_t codeMaxSize = 0x7E;

	unsigned char code[] = {
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0x48, 0x8B, 0xCB,                                            // mov rcx, rbx
		0x48, 0x8D, 0x93, 0x18, 0x0C, 0x00, 0x00,                    // lea rdx, qword ptr ds:[rbx+0xC18]
		0x44, 0x0F, 0xB6, 0xC7,                                      // movzx r8d, dil
		0x44, 0x0F, 0xB6, 0x8B, 0x0B, 0x06, 0x00, 0x00,              // movzx r9d, byte ptr ds:[rbx+0x60B]
		0xFF, 0xD0,                                                  // call rax
		0x4C, 0x8B, 0xF8,                                            // mov r15, rax
		0x48, 0x89, 0x83, 0x90, 0x00, 0x00, 0x00,                    // mov qword ptr ds:[rbx+0x90], rax
	};

	std::memcpy(&code[2], &pFactory, 8);
#else
	const std::size_t codeOffset = 0x57AE5;
	const std::size_t codeMaxSize = 0x4C;

	unsigned char code[] = {
		0xB8, 0x00, 0x00, 0x00, 0x00,              // mov eax, 0x0
		0x53,                                      // push ebx
		0x0F, 0xB6, 0x96, 0x4B, 0x05, 0x00, 0x00,  // movzx edx, byte ptr ds:[esi+0x54B]
		0x52,                                      // push edx
		0x8D, 0x96, 0xD8, 0x09, 0x00, 0x00,        // lea edx, dword ptr ds:[esi+0x9D8]
		0x52,                                      // push edx
		0x56,                                      // push esi
		0xFF, 0xD0,                                // call eax
		0x83, 0xC4, 0x10,                          // add esp, 0x10
		0x89, 0x44, 0x24, 0x14,                    // mov dword ptr ss:[esp+0x14], eax
		0x89, 0x46, 0x48,                          // mov dword ptr ds:[esi+0x48], eax
	};

	std::memcpy(&code[1], &pFactory, 4);
#endif

	static_assert(sizeof(code) <= codeMaxSize);

	WinAPI::FillMem(WinAPI::RVA(pCrySystem, codeOffset), code, sizeof(code));
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, codeOffset + sizeof(code)), codeMaxSize - sizeof(code));

#ifdef BUILD_64BIT
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, 0x358DE), 0x35);
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, 0x46276), 0x2F);
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, 0x509BA), 0x99);
#else
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, 0x4EDC9), 0x13);
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, 0x59B10), 0xC);
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, 0x624A6), 0x32);
#endif
}

static IStreamEngine* CreateNewStreamEngine()
{
	CryLogAlways("$3[CryMP] Initializing Stream Engine");

	return &StreamEngine::GetInstance();
}

static void UpdateStreamEngine()
{
	StreamEngine::GetInstance().Update();
}

static void ReplaceStreamEngine(void* pCrySystem)
{
	void* pFactory = CreateNewStreamEngine;
	void* pUpdate = UpdateStreamEngine;

#ifdef BUILD_64BIT
	const std::size_t codeA_Offset = 0x46485;
	const std::size_t codeA_MaxSize = 0x94;

	const std::size_t codeB_Offset = 0x36514;
	const std::size_t codeB_MaxSize = 0x170;

	unsigned char codeA[] = {
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0xFF, 0xD0,                                                  // call rax
		0x4C, 0x8B, 0xB4, 0x24, 0x08, 0x02, 0x00, 0x00,              // mov r14, qword ptr ss:[rsp+0x208]
	};

	unsigned char codeB[] = {
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0xFF, 0xD0,                                                  // call rax
		0xE9, 0x5F, 0x01, 0x00, 0x00,                                // jmp crysystem.36536684
	};

	std::memcpy(&codeA[2], &pFactory, 8);
	std::memcpy(&codeB[2], &pUpdate, 8);
#else
	const std::size_t codeA_Offset = 0x59C47;
	const std::size_t codeA_MaxSize = 0x14;

	const std::size_t codeB_Offset = 0x4C75D;
	const std::size_t codeB_MaxSize = 0xA3;

	unsigned char codeA[] = {
		0xB8, 0x00, 0x00, 0x00, 0x00,              // mov eax, 0x0
		0xFF, 0xD0,                                // call eax
		0x3E, 0x89, 0x85, 0xA0, 0x05, 0x00, 0x00,  // mov dword ptr ds:[ebp+0x5A0], eax                              |
	};

	unsigned char codeB[] = {
		0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0x0
		0xFF, 0xD0,                    // call eax
		0xE9, 0x97, 0x00, 0x00, 0x00,  // jmp crysystem.3654C800
	};

	std::memcpy(&codeA[1], &pFactory, 4);
	std::memcpy(&codeB[1], &pUpdate, 4);
#endif

	static_assert(sizeof(codeA) <= codeA_MaxSize);
	static_assert(sizeof(codeB) <= codeB_MaxSize);

	WinAPI::FillMem(WinAPI::RVA(pCrySystem, codeA_Offset), codeA, sizeof(codeA));
	WinAPI::FillMem(WinAPI::RVA(pCrySystem, codeB_Offset), codeB, sizeof(codeB));
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, codeA_Offset + sizeof(codeA)), codeA_MaxSize - sizeof(codeA));
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, codeB_Offset + sizeof(codeB)), codeB_MaxSize - sizeof(codeB));

#ifdef BUILD_64BIT
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, 0x358C0), 0x1E);
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, 0x50988), 0x32);
#else
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, 0x4EDB0), 0x19);
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, 0x62478), 0x2E);
#endif
}

static IPhysicalWorld* CreateNewPhysics(ISystem* pSystem)
{
	CryLogAlways("$3[CryMP] Initializing Physics");

	return CreatePhysicalWorld(pSystem);
}

static void ReplacePhysics(void* pCrySystem)
{
	void* pFactory = CreateNewPhysics;

#ifdef BUILD_64BIT
	const std::size_t codeOffset = 0x43392;
	const std::size_t codeMaxSize = 0x53;

	unsigned char code[] = {
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0x48, 0x8B, 0xCE,                                            // mov rcx, rsi
		0xFF, 0xD0,                                                  // call rax
	};

	std::memcpy(&code[2], &pFactory, 8);
#else
	const std::size_t codeOffset = 0x556CF;
	const std::size_t codeMaxSize = 0x49;

	unsigned char code[] = {
		0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0x0
		0x55,                          // push ebp
		0xFF, 0xD0,                    // call eax
		0x83, 0xC4, 0x04,              // add esp, 0x4
	};

	std::memcpy(&code[1], &pFactory, 4);
#endif

	static_assert(sizeof(code) <= codeMaxSize);

	WinAPI::FillMem(WinAPI::RVA(pCrySystem, codeOffset), code, sizeof(code));
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, codeOffset + sizeof(code)), codeMaxSize - sizeof(code));
}

#ifdef CLIENT_LAUNCHER

static void InitNewSoundSystem(ISystem* pSystem, void* hWnd)
{
	CryLogAlways("$3[CryMP] Initializing Sound System");

	if (g_hasC1Fmod)
	{
		CryLogAlways("$3[CryMP] Using C1 FMOD");
	}
	else if (g_hasWarheadWarsFmod)
	{
		CryLogAlways("$3[CryMP] Using Warhead/Wars FMOD");
	}
	else
	{
		CryLogAlways("$3[CryMP] Using unknown FMOD");
	}

	ISoundSystem* pSoundSystem = CreateSoundSystem(pSystem, hWnd);
	gEnv->pSoundSystem = pSoundSystem;
	gEnv->pMusicSystem = pSoundSystem->CreateMusicSystem();
}

static void ReplaceSoundSystem(void* pCrySystem)
{
	void* pInitFunc = InitNewSoundSystem;

#ifdef BUILD_64BIT
	const std::size_t codeOffset = 0x42E29;
	const std::size_t codeMaxSize = 0xD4;

	unsigned char code[] = {
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0x48, 0x8B, 0xCB,                                            // mov rcx, rbx
		0x48, 0x8B, 0xD7,                                            // mov rdx, rdi
		0xFF, 0xD0,                                                  // call rax
	};

	std::memcpy(&code[2], &pInitFunc, 8);
#else
	const std::size_t codeOffset = 0x55229;
	const std::size_t codeMaxSize = 0xA6;

	unsigned char code[] = {
		0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0x0
		0x8B, 0x54, 0x24, 0x08,        // mov edx, dword ptr ss:[esp+0x8]
		0x52,                          // push edx
		0x56,                          // push esi
		0xFF, 0xD0,                    // call eax
		0x83, 0xC4, 0x08,              // add esp, 0x8
	};

	std::memcpy(&code[1], &pInitFunc, 4);
#endif

	static_assert(sizeof(code) <= codeMaxSize);

	WinAPI::FillMem(WinAPI::RVA(pCrySystem, codeOffset), code, sizeof(code));
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, codeOffset + sizeof(code)), codeMaxSize - sizeof(code));
}

#endif // CLIENT_LAUNCHER

static IScriptSystem* CreateNewScriptSystem(ISystem* pSystem, bool)
{
	CryLogAlways("$3[CryMP] Initializing Script System");

	ScriptSystem* pScriptSystem = new ScriptSystem();
	pScriptSystem->Init();

	return pScriptSystem;
}

static void ReplaceScriptSystem(void* pCrySystem)
{
	void* pNewFunc = CreateNewScriptSystem;

#ifdef BUILD_64BIT
	const std::size_t codeOffset = 0x445A2;
	const std::size_t codeMaxSize = 0x4E;

	unsigned char code[] = {
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0x48, 0x8B, 0xCB                                             // mov rcx, rbx
	};

	std::memcpy(&code[2], &pNewFunc, 8);
#else
	const std::size_t codeOffset = 0x56409;
	const std::size_t codeMaxSize = 0x3C;

	unsigned char code[] = {
		0xB8, 0x00, 0x00, 0x00, 0x00  // mov eax, 0x0
	};

	std::memcpy(&code[1], &pNewFunc, 4);
#endif

	static_assert(sizeof(code) <= codeMaxSize);

	WinAPI::FillMem(WinAPI::RVA(pCrySystem, codeOffset), code, sizeof(code));
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, codeOffset + sizeof(code)), codeMaxSize - sizeof(code));
}

static IHardwareMouse* CreateNewHardwareMouse()
{
	if (gEnv->pSystem->IsDedicated())
	{
		return nullptr;
	}

	CryLogAlways("$3[CryMP] Initializing Hardware Mouse");

	HardwareMouse* pHardwareMouse = &HardwareMouse::GetInstance();
	pHardwareMouse->Init();

	return pHardwareMouse;
}

static void ReplaceHardwareMouse(void* pCrySystem)
{
	void* pNewFunc = CreateNewHardwareMouse;

#ifdef BUILD_64BIT
	const std::size_t codeOffset = 0x469A0;
	const std::size_t codeMaxSize = 0x5C;

	unsigned char code[] = {
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0xFF, 0xD0                                                   // call rax
	};

	std::memcpy(&code[2], &pNewFunc, 8);
#else
	const std::size_t codeOffset = 0x59F2D;
	const std::size_t codeMaxSize = 0x22;

	unsigned char code[] = {
		0x83, 0xC4, 0x04,              // add esp, 0x4
		0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0x0
		0xFF, 0xD0,                    // call eax
	};

	std::memcpy(&code[4], &pNewFunc, 4);
#endif

	static_assert(sizeof(code) <= codeMaxSize);

	WinAPI::FillMem(WinAPI::RVA(pCrySystem, codeOffset), code, sizeof(code));
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, codeOffset + sizeof(code)), codeMaxSize - sizeof(code));
}

static bool LanguagePakExists(std::string_view language)
{
	std::filesystem::path path = "Game";
	path /= "Localized";
	path /= language;
	path += ".pak";

	return std::filesystem::exists(path);
}

static std::string_view ChooseLanguage(std::string_view defaultLanguage, ICVar* pLanguageCVar)
{
	std::string_view language = WinAPI::CmdLine::GetArgValue("-language");

	if (!language.empty())
	{
		return language;
	}

	if (pLanguageCVar)
	{
		const std::string_view value = pLanguageCVar->GetString();

		if (!value.empty())
		{
			return value;
		}
	}

	language = defaultLanguage;

	if (language.empty())
	{
		CryLogAlways("$4[CryMP] Missing or invalid Game/Localized/Default.lng file!");
		CryLogAlways("$4[CryMP] Trying to guess language from the system!");
		language = LocalizationManager::GetLanguageFromSystem();

		if (language.empty())
		{
			CryLogAlways("$4[CryMP] Failed to guess language from the system!");
			CryLogAlways("$4[CryMP] Falling back to English language!");
			language = "English";
		}
	}

	bool exists = LanguagePakExists(language);
	if (!exists)
	{
		if (language == "English")
		{
			CryLogAlways("$4[CryMP] Not even English language exists!");
		}
		else
		{
			CryLogAlways("$4[CryMP] %s language does not exist!", language.data());
			CryLogAlways("$4[CryMP] Falling back to English language!");
			language = "English";

			exists = LanguagePakExists(language);
			if (!exists)
			{
				CryLogAlways("$4[CryMP] Not even English language exists!");
			}
		}
	}

	if (!exists)
	{
		CryLogAlways("$4[CryMP] No suitable language found!");

		WinAPI::ErrorBox(
			"No suitable language found!\n"
			"\n"
			"Localization files are incomplete!\n"
			"This is a known issue in the Steam version of Crysis.\n"
			"\n"
			"You can try the following:\n"
			"    1. Go to Game/Localized\n"
			"    2. Choose a suitable *.lng file\n"
			"    3. Make a copy of that file\n"
			"    4. Rename the copy to Default.lng\n"
			"\n"
			"One or more *.pak files of the chosen language must exist!"
		);

		// throwing an exception through the engine is undefined behavior
		std::exit(1);
	}

	return language;
}

static void ReplaceLocalizationManager(void* pCrySystem)
{
	struct DummyCSystem
	{
		ILocalizationManager* GetLocalizationManager()
		{
			return &LocalizationManager::GetInstance();
		}

		static void InitLocalizationManager(const char* defaultLanguage)
		{
			CryLogAlways("$3[CryMP] Initializing Localization Manager");

			ICVar* pLanguageCVar = gEnv->pConsole->GetCVar("g_language");

			const std::string_view language = ChooseLanguage(defaultLanguage, pLanguageCVar);
			CryLogAlways("$3[CryMP] Using %s language", language.data());

			if (pLanguageCVar)
			{
				pLanguageCVar->Set(language.data());
			}

			LocalizationManager::GetInstance().SetLanguage(language.data());
		}
	};

	using TGetLocalizationManager = decltype(&DummyCSystem::GetLocalizationManager);

	void* pNewFunc = &DummyCSystem::InitLocalizationManager;

#ifdef BUILD_64BIT
	const std::size_t vtableOffset = 0x26ACF8;

	unsigned char code[] = {
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0x48, 0x8B, 0x4C, 0x24, 0x30,                                // mov rcx, qword ptr ss:[rsp+0x30]
		0xFF, 0xD0                                                   // call rax
	};

	std::memcpy(&code[2], &pNewFunc, 8);

	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, 0x453A7), 0x1A8);
	WinAPI::FillMem(WinAPI::RVA(pCrySystem, 0x453A7), code, sizeof(code));
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, 0x50A5C), 0x28);
#else
	const std::size_t vtableOffset = 0x1BC5F8;

	unsigned char code[] = {
		0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0x0
		0xFF, 0x74, 0x24, 0x1C,        // push dword ptr ss:[esp+0x1C]
		0xFF, 0xD0,                    // call eax
		0x83, 0xC4, 0x04               // add esp, 0x4
	};

	std::memcpy(&code[1], &pNewFunc, 4);

	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, 0x56B1D), 0xA4);
	WinAPI::FillMem(WinAPI::RVA(pCrySystem, 0x56B1D), code, sizeof(code));
	WinAPI::FillNOP(WinAPI::RVA(pCrySystem, 0x624E1), 0x23);
#endif
	void** vtable = static_cast<void**>(WinAPI::RVA(pCrySystem, vtableOffset));

	// vtable hook
	TGetLocalizationManager newFunc = &DummyCSystem::GetLocalizationManager;
	WinAPI::FillMem(&vtable[105], &reinterpret_cast<void*&>(newFunc), sizeof(void*), false);
}

static ITimeOfDay* CreateTimeOfDay()
{
	CryLogAlways("$3[CryMP] Initializing Time Of Day");

	return new TimeOfDay;
}

static void DestroyTimeOfDay(void* pTimeOfDay)
{
	delete static_cast<TimeOfDay*>(pTimeOfDay);
}

static float GetTimeOfDayHDRMultiplier()
{
	return static_cast<TimeOfDay*>(gEnv->p3DEngine->GetTimeOfDay())->GetHDRMultiplier();
}

static void ReplaceTimeOfDay(void* pCry3DEngine)
{
	void* ctorFunc = &CreateTimeOfDay;
	void* dtorFunc = &DestroyTimeOfDay;
	void* getHDRMultiplierFunc = &GetTimeOfDayHDRMultiplier;

#ifdef BUILD_64BIT
	unsigned char ctorCode[] = {
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0xFF, 0xD0,                                                  // call rax
		0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,              // nop...
		0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
	};

	unsigned char dtorCode[] = {
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0x48, 0x8B, 0xCB,                                            // mov rcx, rbx
		0xFF, 0xD0,                                                  // call rax
		0x90                                                         // nop
	};

	unsigned char getHDRMultiplierCode[] = {
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0xFF, 0xD0,                                                  // call rax
		0x0F, 0x28, 0xD0,                                            // movaps xmm2, xmm0
		0x48, 0x8D, 0x4C, 0x24, 0x20,                                // lea rcx, qword ptr ss:[rsp+0x20]
		0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90         // nop...
	};

	unsigned char getHDRMultiplierCode2[] = {
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0xFF, 0xD0,                                                  // call rax
		0x0F, 0x29, 0xB4, 0x24, 0x80, 0x00, 0x00, 0x00,              // movaps xmmword ptr ss:[rsp+0x80], xmm6
		0x0F, 0x28, 0xF0,                                            // movaps xmm6, xmm0
		0x33, 0xF6,                                                  // xor esi, esi
		0x41, 0x39, 0xB4, 0x24, 0x80, 0x00, 0x00, 0x00,              // cmp dword ptr ds:[r12+0x80], esi
		0x90, 0x90, 0x90, 0x90, 0x90, 0x90                           // nop...
	};

	std::memcpy(&ctorCode[2], &ctorFunc, 8);
	std::memcpy(&dtorCode[2], &dtorFunc, 8);
	std::memcpy(&getHDRMultiplierCode[2], &getHDRMultiplierFunc, 8);
	std::memcpy(&getHDRMultiplierCode2[2], &getHDRMultiplierFunc, 8);

	WinAPI::FillMem(WinAPI::RVA(pCry3DEngine, 0xFB81A), ctorCode, sizeof(ctorCode));
	WinAPI::FillMem(WinAPI::RVA(pCry3DEngine, 0xFC505), dtorCode, sizeof(dtorCode));
	WinAPI::FillMem(WinAPI::RVA(pCry3DEngine, 0xF38CC), getHDRMultiplierCode, sizeof(getHDRMultiplierCode));
	WinAPI::FillMem(WinAPI::RVA(pCry3DEngine, 0x11E2C1), getHDRMultiplierCode2, sizeof(getHDRMultiplierCode2));
#else
	unsigned char ctorCode[] = {
		0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0x0
		0xFF, 0xD0,                    // call eax
		0x90, 0x90, 0x90, 0x90,        // nop...
		0x90, 0x90, 0x90, 0x90,
		0x90, 0x90, 0x90, 0x90,
		0x90, 0x90, 0x90, 0x90,
		0x90, 0x90
	};

	unsigned char dtorCode[] = {
		0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0x0
		0x57,                          // push edi
		0xFF, 0xD0,                    // call eax
		0x83, 0xC4, 0x04,              // add esp, 0x4
		0x90, 0x90, 0x90, 0x90, 0x90   // nop...
	};

	unsigned char getHDRMultiplierCode[] = {
		0xB8, 0x00, 0x00, 0x00, 0x00,        // mov eax, 0x0
		0xFF, 0xD0,                          // call eax
		0xD9, 0x5C, 0x24, 0xFC,              // fstp dword ptr ss:[esp-0x4], st(0)
		0xF3, 0x0F, 0x10, 0x44, 0x24, 0xFC,  // movss xmm0, dword ptr ss:[esp-0x4]
		0x90, 0x90, 0x90, 0x90               // nop...
	};

	unsigned char getHDRMultiplierCode2[] = {
		0x55,                                // push ebp
		0x89, 0x5C, 0x24, 0x0C,              // mov dword ptr ss:[esp+0xC], ebx
		0xB8, 0x00, 0x00, 0x00, 0x00,        // mov eax, 0x0
		0xFF, 0xD0,                          // call eax
		0xD9, 0x5C, 0x24, 0xFC,              // fstp dword ptr ss:[esp-0x4], st(0)
		0xF3, 0x0F, 0x10, 0x44, 0x24, 0xFC,  // movss xmm0, dword ptr ss:[esp-0x4]
		0x90, 0x90, 0x90, 0x90               // nop...
	};

	std::memcpy(&ctorCode[1], &ctorFunc, 4);
	std::memcpy(&dtorCode[1], &dtorFunc, 4);
	std::memcpy(&getHDRMultiplierCode[1], &getHDRMultiplierFunc, 4);
	std::memcpy(&getHDRMultiplierCode2[6], &getHDRMultiplierFunc, 4);

	WinAPI::FillMem(WinAPI::RVA(pCry3DEngine, 0xBE70B), ctorCode, sizeof(ctorCode));
	WinAPI::FillMem(WinAPI::RVA(pCry3DEngine, 0xBF0D6), dtorCode, sizeof(dtorCode));
	WinAPI::FillMem(WinAPI::RVA(pCry3DEngine, 0xB8D7D), getHDRMultiplierCode, sizeof(getHDRMultiplierCode));
	WinAPI::FillMem(WinAPI::RVA(pCry3DEngine, 0xDC5B6), getHDRMultiplierCode2, sizeof(getHDRMultiplierCode2));
#endif
}

struct DummyCNetwork
{
	static inline INetworkServicePtr (DummyCNetwork::*s_pOriginalGetService)(const char* name);

	INetworkServicePtr GetService(const char* name)
	{
		// log every access to the GameSpy service
		// we want to eventually get rid of GameSpy completely
		CryLogWarning("INetwork::GetService(\"%s\")", name);

		return (this->*s_pOriginalGetService)(name);
	}
};

static void HookNetworkGetService(void* pCryNetwork)
{
	void** pCNetworkVTable = static_cast<void**>(WinAPI::RVA(pCryNetwork,
#ifdef BUILD_64BIT
		0x19BEE8
#else
		0xC0C90
#endif
	));

	std::memcpy(&DummyCNetwork::s_pOriginalGetService, &pCNetworkVTable[7], sizeof(void*));

	// vtable hook
	auto pNewGetService = &DummyCNetwork::GetService;
	WinAPI::FillMem(&pCNetworkVTable[7], &reinterpret_cast<void*&>(pNewGetService), sizeof(void*), false);
}

struct DummyCSystem
{
	void Warning(int subsys, int severity, int flags, const char* file, const char* format, ...)
	{
		std::string msg;

		// severity is ignored because the message is always supposed to be a warning

		switch (subsys)
		{
			case VALIDATOR_MODULE_UNKNOWN:      msg += "[Unknown] ";      break;
			case VALIDATOR_MODULE_RENDERER:     msg += "[Renderer] ";     break;
			case VALIDATOR_MODULE_3DENGINE:     msg += "[3DEngine] ";     break;
			case VALIDATOR_MODULE_AI:           msg += "[AI] ";           break;
			case VALIDATOR_MODULE_ANIMATION:    msg += "[Animation] ";    break;
			case VALIDATOR_MODULE_ENTITYSYSTEM: msg += "[EntitySystem] "; break;
			case VALIDATOR_MODULE_SCRIPTSYSTEM: msg += "[ScriptSystem] "; break;
			case VALIDATOR_MODULE_SYSTEM:       msg += "[System] ";       break;
			case VALIDATOR_MODULE_SOUNDSYSTEM:  msg += "[SoundSystem] ";  break;
			case VALIDATOR_MODULE_GAME:         msg += "[Game] ";         break;
			case VALIDATOR_MODULE_MOVIE:        msg += "[Movie] ";        break;
			case VALIDATOR_MODULE_EDITOR:       msg += "[Editor] ";       break;
			case VALIDATOR_MODULE_NETWORK:      msg += "[Network] ";      break;
			case VALIDATOR_MODULE_PHYSICS:      msg += "[Physics] ";      break;
			case VALIDATOR_MODULE_FLOWGRAPH:    msg += "[FlowGraph] ";    break;
			default:                            msg += "[?] ";            break;
		}

		if (flags & VALIDATOR_FLAG_FILE)    msg += "[File] ";
		if (flags & VALIDATOR_FLAG_TEXTURE) msg += "[Texture] ";
		if (flags & VALIDATOR_FLAG_SCRIPT)  msg += "[Script] ";
		if (flags & VALIDATOR_FLAG_SOUND)   msg += "[Sound] ";
		if (flags & VALIDATOR_FLAG_AI)      msg += "[AI] ";

		va_list args;
		va_start(args, format);
		StringTools::FormatToV(msg, format, args);
		va_end(args);

		if (file)
		{
			msg += " [";
			msg += file;
			msg += "]";
		}

		CryLogWarning("%s", msg.c_str());
	}
};

static void HookSystemWarning(void* pCrySystem)
{
	void** pCSystemVTable = static_cast<void**>(WinAPI::RVA(pCrySystem,
#ifdef BUILD_64BIT
		0x26ACF8
#else
		0x1BC5F8
#endif
	));

	// vtable hook
	auto pNewWarning = &DummyCSystem::Warning;
	WinAPI::FillMem(&pCSystemVTable[21], &reinterpret_cast<void*&>(pNewWarning), sizeof(void*), false);
}

static void LogRealWindowsBuild(Logger& logger)
{
	void* kernel32 = WinAPI::DLL::Get("kernel32.dll");
	if (!kernel32)
	{
		return;
	}

	WinAPI::VersionResource ver;
	if (!WinAPI::GetVersionResource(kernel32, ver))
	{
		return;
	}

	logger.LogAlways("Windows build: %hu.%hu.%hu (real)", ver.major, ver.minor, ver.patch);
}

static void EnableHiddenProfilerSubsystems(ISystem* pSystem)
{
	struct Subsystem
	{
		const char* name;
		float reserved;
	};

#ifdef BUILD_64BIT
	static_assert(sizeof(Subsystem) == 16);
#else
	static_assert(sizeof(Subsystem) == 8);
#endif

	IFrameProfileSystem* pProfiler = pSystem->GetIProfileSystem();

#ifdef BUILD_64BIT
	void* subsystemsBegin = reinterpret_cast<unsigned char*>(pProfiler) + 0xAC8;
#else
	void* subsystemsBegin = reinterpret_cast<unsigned char*>(pProfiler) + 0x634;
#endif
	Subsystem* subsystems = static_cast<Subsystem*>(subsystemsBegin);

	subsystems[PROFILE_ANY].name = "Unknown";
	subsystems[PROFILE_MOVIE].name = "Movie";
	subsystems[PROFILE_FONT].name = "Font";
	subsystems[PROFILE_SCRIPT].name = "Script";
}

#ifdef CRYMP_TRACY_ENABLED
static void TracyProfilerStartSection(CFrameProfilerSection* section)
{
	using namespace tracy;

	const char* name = section->m_pFrameProfiler->m_name;

	TracyQueuePrepare(QueueType::ZoneBeginAllocSrcLoc);
	const auto srcloc = Profiler::AllocSourceLocation(0, nullptr, 0, nullptr, 0, name, std::strlen(name));
	MemWrite(&item->zoneBegin.time, Profiler::GetTime());
	MemWrite(&item->zoneBegin.srcloc, srcloc);
	TracyQueueCommit(zoneBeginThread);
}

static void TracyProfilerEndSection(CFrameProfilerSection* section)
{
	using namespace tracy;

	TracyQueuePrepare(QueueType::ZoneEnd);
	MemWrite(&item->zoneEnd.time, Profiler::GetTime());
	TracyQueueCommit(zoneEndThread);
}

static void TracyHookEngineProfiler()
{
	gEnv->callbackStartSection = &TracyProfilerStartSection;
	gEnv->callbackEndSection = &TracyProfilerEndSection;
	gEnv->bProfilerEnabled = true;
}
#endif

static std::FILE* ProvideLogFile()
{
	return Logger::GetInstance().GetFileHandle();
}

void Launcher::SetCmdLine()
{
	std::string cmdLine = WinAPI::CmdLine::GetFull();

#ifdef SERVER_LAUNCHER
	const std::filesystem::path defaultServerRoot = std::filesystem::absolute("CryMP-Server");
	if (!WinAPI::CmdLine::HasArg("-root") && std::filesystem::is_directory(defaultServerRoot))
	{
		cmdLine += " -root \"";
		cmdLine += defaultServerRoot.string();
		cmdLine += "\" +exec server.cfg";
	}
#endif

	if (cmdLine.length() >= sizeof(m_params.cmdLine))
	{
		throw StringTools::ErrorFormat("Command line is too long!");
	}

	std::memcpy(m_params.cmdLine, cmdLine.c_str(), cmdLine.length() + 1);
}

void Launcher::InitWorkingDirectory()
{
	std::filesystem::path dir;

	if (const std::string_view dirArgValue = WinAPI::CmdLine::GetArgValue("-dir"); !dirArgValue.empty())
	{
		dir = dirArgValue;
	}
	else
	{
		dir = WinAPI::GetApplicationPath().remove_filename();
	}

	// sanitize the path
	dir = dir.lexically_normal();

	// remove trailing slash
	if (!dir.has_filename())
	{
		dir = dir.parent_path();
	}

#ifdef BUILD_64BIT
	constexpr std::string_view BIN_DIR = "Bin64";
#else
	constexpr std::string_view BIN_DIR = "Bin32";
#endif

	if (dir.filename() != BIN_DIR && std::filesystem::is_directory(dir / "Game"))
	{
		// we are in Crysis directory, so add Bin32 or Bin64 to the DLL search path
		WinAPI::DLL::AddSearchDirectory(dir / BIN_DIR);
	}
	else
	{
		// we are in Crysis/Bin32 or Crysis/Bin64 directory, so remove Bin32 or Bin64 from the path
		dir = dir.parent_path();
	}

	WinAPI::SetWorkingDirectory(dir);
}

void Launcher::LoadEngine()
{
	void* msvcr80 = WinAPI::DLL::Load("msvcr80.dll");
	if (!msvcr80)
	{
		if (WinAPI::GetCurrentErrorCode() == 126)  // ERROR_MOD_NOT_FOUND
		{
			throw StringTools::ErrorFormat("Failed to load the MSVCR80 DLL!\n\n"
				"Crysis requires the following to be installed:\n\n"
				"- Microsoft Visual C++ 2005 Service Pack 1 Redistributable\n"
				"- DirectX End-User Runtime\n"
			);
		}
		else
		{
			throw StringTools::SysErrorFormat("Failed to load the MSVCR80 DLL!");
		}
	}

	g_pCrySystem = WinAPI::DLL::Load("CrySystem.dll");
	if (!g_pCrySystem)
	{
		if (WinAPI::GetCurrentErrorCode() == 193)  // ERROR_BAD_EXE_FORMAT
		{
			throw StringTools::ErrorFormat("Failed to load CrySystem.dll\n\n"
#ifdef BUILD_64BIT
				"It seems you have 32-bit DLLs in your Bin64 directory! Please fix it."
#else
				"It seems you have 64-bit DLLs in your Bin32 directory! Please fix it."
#endif
			);
		}
		else
		{
			throw StringTools::SysErrorFormat("Failed to load the CrySystem DLL!");
		}
	}

	WinAPI::VersionResource version;
	if (!WinAPI::GetVersionResource(g_pCrySystem, version))
	{
		throw StringTools::SysErrorFormat("Failed to get the game version!");
	}

	const int gameBuild = version.tweak;

	switch (gameBuild)
	{
		case 5767:
		{
			throw StringTools::ErrorFormat(
				"Crysis 1.0 is not supported!\n\n"
				"Install 1.2 and 1.2.1 official patch."
			);
		}
		case 5879:
		{
			throw StringTools::ErrorFormat(
				"Crysis 1.1 is not supported!\n\n"
				"Install 1.2 and 1.2.1 official patch."
			);
		}
		case 6115:
		{
			throw StringTools::ErrorFormat(
				"Crysis 1.2 is not supported!\n\n"
				"Install 1.2.1 official patch."
			);
		}
		case 6156:
		{
			// only the latest Crysis 1.2.1 is supported
			break;
		}
		case 6527:
		case 6566:
		case 6586:
		case 6627:
		case 6670:
		case 6729:
		{
			throw StringTools::ErrorFormat("Crysis Wars is not supported!");
		}
		case 687:
		case 710:
		case 711:
		{
			throw StringTools::ErrorFormat("Crysis Warhead is not supported!");
		}
		default:
		{
			throw StringTools::ErrorFormat("Unknown game build %d!", gameBuild);
		}
	}

	struct SLoadEntry
	{
		const char* dllName;
		void** outPtr;
	};

	std::vector<SLoadEntry> loadList =
	{
		{ "CryAction.dll",       &g_pCryAction },
		{ "CryAISystem.dll",     &g_pCryAISystem },
		{ "CryNetwork.dll",      &g_pCryNetwork },
		{ "Cry3DEngine.dll",     &g_pCry3DEngine },
		{ "CryFont.dll",         &g_pCryFont },
		{ "CryAnimation.dll",    &g_pCryAnimation },
		{ "CryEntitySystem.dll", &g_pCryEntitySystem },
	};

	if (WinAPI::CmdLine::HasArg("-oldphysics"))
	{
		loadList.push_back({ "CryPhysics.dll", &g_pCryPhysics });
	}

	if (m_params.isDedicatedServer)
	{
		loadList.push_back({ "CryRenderNULL.dll", &g_pCryRenderNULL });
	}
	else
	{
		loadList.push_back({ "CryMovie.dll", &g_pCryMovie });
		loadList.push_back({ "CryInput.dll", &g_pCryInput });
		if (WinAPI::CmdLine::HasArg("-oldsound")) {
			loadList.push_back({ "CrySoundSystem.dll", &g_pCrySoundSystem });
		}
		if (!WinAPI::CmdLine::HasArg("-dx9") &&
			(WinAPI::CmdLine::HasArg("-dx10") || WinAPI::IsVistaOrLater()))
		{
			loadList.push_back({ "CryRenderD3D10.dll", &g_pCryRenderD3D10 });
			loadList.push_back({ "D3DCompiler_34.dll", &g_pD3DCompiler });
		}
		else
		{
			loadList.push_back({ "CryRenderD3D9.dll", &g_pCryRenderD3D9 });
			loadList.push_back({ "d3dx9_34.dll", &g_pD3DCompiler });
		}

		loadList.push_back({ "CompressATI2.dll", &g_pCompressATI2 });

	#ifdef BUILD_64BIT
		loadList.push_back({ "fmodex64.dll", &g_pFmodEx });
		loadList.push_back({ "fmod_event64.dll", &g_pFmodEvent });
		loadList.push_back({ "fmod_event_net64.dll", &g_pFmodEventNet });
	#else
		loadList.push_back({ "fmodex.dll", &g_pFmodEx });
		loadList.push_back({ "fmod_event.dll", &g_pFmodEvent });
		loadList.push_back({ "fmod_event_net.dll", &g_pFmodEventNet });
	#endif
	}

	for (const SLoadEntry& entry : loadList)
	{
		*entry.outPtr = WinAPI::DLL::Load(entry.dllName);

		if (!*entry.outPtr)
		{
			throw StringTools::SysErrorFormat("Failed to load %s", entry.dllName);
		}
	}

	CRYACTION_BASE = reinterpret_cast<std::uintptr_t>(g_pCryAction);

	WinAPI::VersionResource fmodVer;
	if (!WinAPI::GetVersionResource(g_pFmodEx, fmodVer))
	{
		throw StringTools::SysErrorFormat("Failed to get FMOD version!");
	}

	g_hasC1Fmod = (
		fmodVer.major == 0 &&
		fmodVer.minor == 4 &&
		fmodVer.patch == 7 &&
		fmodVer.tweak == 23
	);
	g_hasWarheadWarsFmod = (!g_hasC1Fmod &&
		fmodVer.major == 0 &&
		fmodVer.minor == 4 &&
		fmodVer.patch == 14 &&
		fmodVer.tweak == 3
	);
}

void Launcher::PatchEngine()
{
	if (g_pCryAction)
	{
		MemoryPatch::CryAction::AllowDX9ImmersiveMultiplayer(g_pCryAction);
		MemoryPatch::CryAction::AllowMultiplayerRegisterWithAI(g_pCryAction);
		MemoryPatch::CryAction::DisableBreakLog(g_pCryAction);
		MemoryPatch::CryAction::DisableGameplayStats(g_pCryAction);
		MemoryPatch::CryAction::DisableTimeOfDayLengthLowerLimit(g_pCryAction);
		MemoryPatch::CryAction::HookCryWarning(g_pCryAction, &OnCryWarning);
		MemoryPatch::CryAction::HookGameWarning(g_pCryAction, &OnGameWarning);
	}

	if (g_pCryAISystem)
	{
		MemoryPatch::CryAISystem::AllowMultiplayerAI(g_pCryAISystem);
	}

	if (g_pCryNetwork)
	{
		MemoryPatch::CryNetwork::AllowSameCDKeys(g_pCryNetwork);
		MemoryPatch::CryNetwork::DisableServerProfile(g_pCryNetwork);
		MemoryPatch::CryNetwork::EnablePreordered(g_pCryNetwork);
		MemoryPatch::CryNetwork::FixFileCheckCrash(g_pCryNetwork);
		MemoryPatch::CryNetwork::FixInternetConnect(g_pCryNetwork);
		MemoryPatch::CryNetwork::FixLanServerBrowser(g_pCryNetwork);
		MemoryPatch::CryNetwork::RemoveGameSpyAvailableCheck(g_pCryNetwork);
		MemoryPatch::CryNetwork::HookCryWarning(g_pCryNetwork, &OnCryWarning);

		HookNetworkGetService(g_pCryNetwork);
	}

	if (g_pCrySystem)
	{
		MemoryPatch::CrySystem::AllowDX9VeryHighSpec(g_pCrySystem);
		MemoryPatch::CrySystem::AllowMultipleInstances(g_pCrySystem);
		MemoryPatch::CrySystem::FixCPUInfoOverflow(g_pCrySystem);
		MemoryPatch::CrySystem::FixFlashAllocatorUnderflow(g_pCrySystem);
		MemoryPatch::CrySystem::HookCPUDetect(g_pCrySystem, &CPUInfo::Detect);
		MemoryPatch::CrySystem::HookError(g_pCrySystem, &CrashLogger::OnEngineError);
		//MemoryPatch::CrySystem::MakeDX9Default(g_pCrySystem);
		MemoryPatch::CrySystem::RemoveSecuROM(g_pCrySystem);
		MemoryPatch::CrySystem::UnhandledExceptions(g_pCrySystem);
		//MemoryPatch::CrySystem::EnableServerPhysicsThread(g_pCrySystem);
		MemoryPatch::CrySystem::HookCryWarning(g_pCrySystem, &OnCryWarning);

		InstallEarlyEngineInitHook(g_pCrySystem);
		HookSystemWarning(g_pCrySystem);

		ReplaceCryPak(g_pCrySystem);
		ReplaceStreamEngine(g_pCrySystem);
		ReplaceScriptSystem(g_pCrySystem);
		ReplaceHardwareMouse(g_pCrySystem);
		ReplaceLocalizationManager(g_pCrySystem);

		if (!WinAPI::CmdLine::HasArg("-oldphysics"))
		{
			ReplacePhysics(g_pCrySystem);
		}

#ifdef CLIENT_LAUNCHER
		if (!WinAPI::CmdLine::HasArg("-oldsound"))
		{
			ReplaceSoundSystem(g_pCrySystem);
		}
#endif
	}

	if (g_pCry3DEngine)
	{
		MemoryPatch::Cry3DEngine::EnableBigDecalsOnDynamicObjects(g_pCry3DEngine);
		MemoryPatch::Cry3DEngine::FixGetObjectsByType(g_pCry3DEngine);

		ReplaceTimeOfDay(g_pCry3DEngine);
	}

	const char* GAME_WINDOW_NAME = "CryMP Client " CRYMP_VERSION_STRING;

	if (g_pCryRenderD3D9)
	{
		MemoryPatch::CryRenderD3D9::FixUseAfterFreeInShaderParser(g_pCryRenderD3D9);
		MemoryPatch::CryRenderD3D9::HookWindowNameD3D9(g_pCryRenderD3D9, GAME_WINDOW_NAME);
		MemoryPatch::CryRenderD3D9::HookAdapterInfo(g_pCryRenderD3D9, &OnD3D9Info);
	}

	if (g_pCryRenderD3D10)
	{
		MemoryPatch::CryRenderD3D10::FixLowRefreshRateBug(g_pCryRenderD3D10);
		MemoryPatch::CryRenderD3D10::FixUseAfterFreeInShaderParser(g_pCryRenderD3D10);
		MemoryPatch::CryRenderD3D10::HookWindowNameD3D10(g_pCryRenderD3D10, GAME_WINDOW_NAME);
		MemoryPatch::CryRenderD3D10::HookAdapterInfo(g_pCryRenderD3D10, &OnD3D10Info);
		MemoryPatch::CryRenderD3D10::HookInitAPI(g_pCryRenderD3D10, &OnD3D10Init);
	}

	if (g_pCryRenderNULL)
	{
		MemoryPatch::CryRenderNULL::DisableDebugRenderer(g_pCryRenderNULL);
	}

	if (g_pFmodEx)
	{
		if (g_hasC1Fmod)
		{
			// only C1 FMOD is affected
			MemoryPatch::FMODEx::Fix64BitHeapAddressTruncation(g_pFmodEx);
		}

#ifdef CLIENT_LAUNCHER
		if (WinAPI::CmdLine::HasArg("-dsoal") && (g_hasC1Fmod || g_hasWarheadWarsFmod))
		{
			DsoalDeployer::Init(g_pFmodEx, g_hasC1Fmod);
		}
#endif
	}
}

void Launcher::StartEngine()
{
	const bool oldAction = WinAPI::CmdLine::HasArg("-oldaction");

	IGameFramework* pGameFramework = nullptr;

	if (oldAction)
	{
		using CryActionEntry = IGameFramework::TEntryFunction;

		auto entry = static_cast<CryActionEntry>(WinAPI::DLL::GetSymbol(g_pCryAction, "CreateGameFramework"));
		if (!entry)
		{
			throw StringTools::ErrorFormat("The CryAction DLL is not valid!");
		}

		pGameFramework = entry();
		if (!pGameFramework)
		{
			throw StringTools::ErrorFormat("Failed to create the GameFramework Interface!");
		}
	}
	else
	{
		pGameFramework = GameFramework::GetInstance();
	}

#ifdef CLIENT_LAUNCHER
	GameWindow::GetInstance().Init();
#endif

	if (oldAction)
	{
		// initialize CryEngine
		// Launcher::OnEarlyEngineInit is called here
		if (!pGameFramework->Init(m_params))
		{
			throw StringTools::ErrorFormat("CryENGINE initialization failed!");
		}
	}
	else
	{
		// initialize CryEngine without CryAction
		// Launcher::OnEarlyEngineInit is called here
		InitCrySystem(g_pCrySystem, m_params);

		// initialize our CryAction
		if (!pGameFramework->Init(m_params))
		{
			throw StringTools::ErrorFormat("CryAction initialization failed!");
		}
	}

	CrashTest::Register();

#ifdef CRYMP_TRACY_ENABLED
	TracyHookEngineProfiler();
#endif

#ifdef CLIENT_LAUNCHER
	if (gClient)
	{
		gClient->Init(pGameFramework);
	}
#endif

	if (gServer)
	{
		gServer->Init(pGameFramework);
	}

	if (!pGameFramework->CompleteInit())
	{
		throw StringTools::ErrorFormat("CryENGINE post-initialization failed!");
	}

	StartupTime::Finish();
	CryLogAlways("Startup finished in %.3f seconds", StartupTime::GetSeconds());

#ifdef SERVER_LAUNCHER
	if (oldAction) {
		CryLogAlways("[CryMP] Using old CryAction");
	} else {
		CryLogAlways("[CryMP] Using new CryAction");
	}
#endif

	gEnv->pSystem->ExecuteCommandLine();
}

void Launcher::OnEarlyEngineInit(ISystem* pSystem)
{
	const std::filesystem::path mainDirPath = std::filesystem::current_path();
	const std::filesystem::path userDirPath = std::filesystem::canonical(gEnv->pCryPak->GetAlias("%USER%"));
	const std::filesystem::path rootDirPath = std::filesystem::canonical(gEnv->pSystem->GetRootFolder());

	const char* defaultVerbosity = "0";
#ifdef CLIENT_LAUNCHER
	const char* defaultLogFileName = "CryMP-Client.log";
#else
	const char* defaultLogFileName = "CryMP-Server.log";
#endif
	const char* defaultLogPrefix = "";

	const int verbosity = std::atoi(WinAPI::CmdLine::GetArgValue("-verbosity", defaultVerbosity));
	const char* logFileName = WinAPI::CmdLine::GetArgValue("-logfile", defaultLogFileName);
	const char* logPrefix = WinAPI::CmdLine::GetArgValue("-logprefix", defaultLogPrefix);

	Logger& logger = Logger::GetInstance();

	logger.SetVerbosity(verbosity);
	logger.OpenFile((rootDirPath.empty() ? userDirPath : rootDirPath) / logFileName);

#ifdef CLIENT_LAUNCHER
	const char* banner = "CryMP Client " CRYMP_VERSION_STRING " " CRYMP_BITS " " CRYMP_BUILD_TYPE;
#else
	const char* banner = "CryMP Server " CRYMP_VERSION_STRING " " CRYMP_BITS " " CRYMP_BUILD_TYPE;
#endif

	CrashLogger::Enable(&ProvideLogFile, &CryMemoryManager::ProvideHeapInfo, banner);

	logger.LogAlways("Log begins at %s", Logger::FormatPrefix("%F %T%z").c_str());

	const std::string exe = WinAPI::GetApplicationPath().string();
	const std::string mainDir = mainDirPath.string();
	const std::string rootDir = rootDirPath.string();
	const std::string userDir = userDirPath.string();

	logger.LogAlways("Executable: %s", exe.c_str());
	logger.LogAlways("Main directory: %s", mainDir.c_str());
	logger.LogAlways("Root directory: %s", rootDir.empty() ? mainDir.c_str() : rootDir.c_str());
	logger.LogAlways("User directory: %s", userDir.c_str());
	logger.LogAlways("");

	const SFileVersion& version = gEnv->pSystem->GetProductVersion();

	logger.LogAlways("Crysis %d.%d.%d.%d " CRYMP_BITS, version[3], version[2], version[1], version[0]);
	logger.LogAlways("%s", banner);
	logger.LogAlways("Compiled by " CRYMP_COMPILER);
	logger.LogAlways("Copyright (C) 2001-2008 Crytek GmbH");
	logger.LogAlways("Copyright (C) 2014-2026 CryMP");
	logger.LogAlways("");

	logger.SetPrefix(logPrefix);

	LogRealWindowsBuild(logger);

	EnableHiddenProfilerSubsystems(pSystem);

	gEnv->pConsole->AddCommand("CryPakInfo", [](IConsoleCmdArgs* args) {
		CryPak::GetInstance().LogInfo();
	});

	gEnv->pConsole->AddCommand("LocalizationManagerInfo", [](IConsoleCmdArgs* args) {
		LocalizationManager::GetInstance().LogInfo();
	});

	gEnv->pConsole->AddCommand("CryMemoryManagerInfo", [](IConsoleCmdArgs* args) {
		CryMemoryManager::LogInfo();
	});
}

struct DummySystemCallback : public ISystemUserCallback
{
	bool OnError(const char*) override { return false; }
	void OnSaveDocument() override {}
	void OnProcessSwitch() override {}
	void OnInitProgress(const char*) override {}
	void OnInit(ISystem*) override {}
	void OnShutdown() override {}
	void OnUpdate() override {}
	void GetMemoryUsage(ICrySizer*) override {}
};

void Launcher::Run()
{
#ifdef CLIENT_LAUNCHER
	CodeWall::InitializeCodeWallExternal();
#endif
	DummySystemCallback dummyCallback;

	m_params.hInstance = WinAPI::DLL::Get(nullptr);  // EXE handle
	m_params.pLog = &Logger::GetInstance();
#ifdef SERVER_LAUNCHER
	m_params.isDedicatedServer = true;
#else
	m_params.isDedicatedServer = WinAPI::CmdLine::HasArg("-dedicated");
#endif

	if (WinAPI::CmdLine::HasArg("-headless"))
	{
		m_params.pUserCallback = &dummyCallback;
		m_params.isDedicatedServer = true;
	}

	if (WinAPI::GetApplicationPath().filename().string().find("CryMP") != 0)
	{
		throw StringTools::ErrorFormat("Invalid name of the executable!");
	}

	if (WinAPI::CmdLine::HasArg("-mod"))
	{
		throw StringTools::ErrorFormat("Mods are not supported!");
	}

	this->InitWorkingDirectory();
	this->SetCmdLine();

	CryMemoryManager::Init();

	this->LoadEngine();
	this->PatchEngine();

	RandomGenerator::Init();

	if (m_params.isDedicatedServer)
	{
		Server server;
		gServer = &server;

		this->StartEngine();

		gServer->UpdateLoop();
	}
	else
	{
#ifdef CLIENT_LAUNCHER
		CodeWall::InitializeCodeWallInternalCIG();
		Client client;
		gClient = &client;

		this->StartEngine();

		CodeWall::InitializeCodeWallInternalACG();
		gClient->UpdateLoop();
#endif
	}
}
