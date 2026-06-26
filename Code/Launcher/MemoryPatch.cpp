#include <cstring>

#include "Library/WinAPI.h"

#include "MemoryPatch.h"

static void FillNop(void* base, std::size_t offset, std::size_t size)
{
	void* address = static_cast<unsigned char*>(base) + offset;

	WinAPI::FillNOP(address, size);
}

static void FillMem(void* base, std::size_t offset, const void* data, std::size_t dataSize)
{
	void* address = static_cast<unsigned char*>(base) + offset;

	WinAPI::FillMem(address, data, dataSize);
}

////////////////////////////////////////////////////////////////////////////////
// Cry3DEngine
////////////////////////////////////////////////////////////////////////////////

void MemoryPatch::Cry3DEngine::EnableBigDecalsOnDynamicObjects(void* pCry3DEngine)
{
	const unsigned char code[] = {
#ifdef BUILD_64BIT
		0xC6, 0x44, 0x24, 0x30, 0x00,  // mov byte ptr ss:[rsp+0x30], 0x0
#else
		0x6A, 0x00,                    // push 0x0
#endif
	};

#ifdef BUILD_64BIT
	FillMem(pCry3DEngine, 0xF6835, code, sizeof(code));
	FillMem(pCry3DEngine, 0xF6867, code, sizeof(code));
#else
	FillMem(pCry3DEngine, 0xBAE29, code, sizeof(code));
	FillMem(pCry3DEngine, 0xBAE43, code, sizeof(code));
#endif
}

void MemoryPatch::Cry3DEngine::FixGetObjectsByType(void* pCry3DEngine)
{
	const unsigned char code[] = {
		0x74, 0x4E,  // je [ip+0x4E]
	};

#ifdef BUILD_64BIT
	FillMem(pCry3DEngine, 0x19CCB7, code, sizeof(code));
#else
	FillMem(pCry3DEngine, 0x134AFA, code, sizeof(code));
#endif
}

////////////////////////////////////////////////////////////////////////////////
// CryAction
////////////////////////////////////////////////////////////////////////////////

/**
 * Allows connecting to DX10 servers with game running in DX9 mode.
 */
void MemoryPatch::CryAction::AllowDX9ImmersiveMultiplayer(void* pCryAction)
{
#ifdef BUILD_64BIT
	FillNop(pCryAction, 0x2B394D, 0x1E);
	FillNop(pCryAction, 0x2B6860, 0x1A);
#else
	FillNop(pCryAction, 0x1D698A, 0x1A);
	FillNop(pCryAction, 0x1D89FC, 0x15);
#endif
}

/**
 * This makes the AI.RegisterWithAI Lua function work in multiplayer.
 */
void MemoryPatch::CryAction::AllowMultiplayerRegisterWithAI(void* pCryAction)
{
#ifdef BUILD_64BIT
	FillNop(pCryAction, 0xEF401, 0x1F);
#else
	FillNop(pCryAction, 0xA79BA, 0x1A);
#endif
}

/**
 * Disables useless "times out" log messages from break replicator.
 */
void MemoryPatch::CryAction::DisableBreakLog(void* pCryAction)
{
#ifdef BUILD_64BIT
	FillNop(pCryAction, 0x2D183F, 0x5);
#else
	FillNop(pCryAction, 0x1E987D, 0x5);
#endif
}

/**
 * Disables the lower limit of ToD length.
 */
void MemoryPatch::CryAction::DisableTimeOfDayLengthLowerLimit(void* pCryAction)
{
#ifdef BUILD_64BIT
	FillNop(pCryAction, 0x302F28, 0x1A);
#else
	FillNop(pCryAction, 0x20C9E4, 0x23);
#endif
}

/**
 * Used to fix the GameWarning format string vulnerability.
 */
void MemoryPatch::CryAction::HookGameWarning(void* pCryAction, void (*handler)(const char* format, ...))
{
#ifdef BUILD_64BIT
	unsigned char code[] = {
		0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0
		0xff, 0xe0,                                                  // jmp rax
		0x90,                                                        // nop
		0x90,                                                        // nop
		0x90,                                                        // nop
	};

	std::memcpy(&code[2], &handler, 8);
#else
	unsigned char code[] = {
		0xb8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0
		0xff, 0xe0,                    // jmp eax
		0x90,                          // nop
		0x90,                          // nop
		0x90,                          // nop
	};

	std::memcpy(&code[1], &handler, 4);
#endif

#ifdef BUILD_64BIT
	FillMem(pCryAction, 0x4230, &code, sizeof(code));
#else
	FillMem(pCryAction, 0xd9c0, &code, sizeof(code));
#endif
}

/**
 * Used to fix the CryWarning format string vulnerability.
 */
void MemoryPatch::CryAction::HookCryWarning(void* pCryAction, void (*handler)(int, int, const char* format, ...))
{
#ifdef BUILD_64BIT
	unsigned char code[] = {
		0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0
		0xff, 0xe0,                                                  // jmp rax
	};

	std::memcpy(&code[2], &handler, 8);
#else
	unsigned char code[] = {
		0xb8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0
		0xff, 0xe0,                    // jmp eax
		0x90,                          // nop
		0x90,                          // nop
		0x90,                          // nop
	};

	std::memcpy(&code[1], &handler, 4);
#endif

#ifdef BUILD_64BIT
	FillMem(pCryAction, 0x1000, &code, sizeof(code));
#else
	FillMem(pCryAction, 0x3240, &code, sizeof(code));
#endif
}

////////////////////////////////////////////////////////////////////////////////
// CryAISystem
////////////////////////////////////////////////////////////////////////////////

/**
 * Disables the multiplayer check in AI system.
 */
void MemoryPatch::CryAISystem::AllowMultiplayerAI(void* pCryAISystem)
{
	const unsigned char code[] = {
#ifdef BUILD_64BIT
		0x90,        // nop
		0x90,        // nop
		0x90,        // nop
		0x90,        // nop
		0x90,        // nop
		0x90,        // nop
		0x90,        // nop
		0xEB, 0x0E,  // jmp [rip+0x0E]
#else
		0x90,        // nop
		0x90,        // nop
		0x90,        // nop
		0x90,        // nop
		0xEB, 0x1F,  // jmp [eip+0x1F]
#endif
	};

#ifdef BUILD_64BIT
	FillMem(pCryAISystem, 0x1C8740, code, sizeof(code));
#else
	FillMem(pCryAISystem, 0x17504B, code, sizeof(code));
#endif
}

////////////////////////////////////////////////////////////////////////////////
// CryNetwork
////////////////////////////////////////////////////////////////////////////////

/**
 * Prevents server from kicking players with the same CD-Key.
 *
 * This is a server-side patch.
 */
void MemoryPatch::CryNetwork::AllowSameCDKeys(void* pCryNetwork)
{
#ifdef BUILD_64BIT
	FillNop(pCryNetwork, 0xE0328, 0x47);
#else
	FillNop(pCryNetwork, 0x606A5, 0x4);
#endif
}

/**
 * Disables creation of "server_profile.txt" file.
 */
void MemoryPatch::CryNetwork::DisableServerProfile(void* pCryNetwork)
{
#ifdef BUILD_64BIT
	// already disabled in 64-bit version
#else
	FillNop(pCryNetwork, 0x9BE2E, 0x5);
#endif
}

/**
 * Unlocks advantages of pre-ordered version for everyone.
 *
 * This is both server-side and client-side patch.
 */
void MemoryPatch::CryNetwork::EnablePreordered(void* pCryNetwork)
{
	const unsigned char code[] = {
#ifdef BUILD_64BIT
		0xC6, 0x83, 0x70, 0xFA, 0x00, 0x00, 0x01  // mov byte ptr ds:[rbx+0xFA70], 0x1
#else
		0xC6, 0x83, 0xC8, 0xF3, 0x00, 0x00, 0x01  // mov byte ptr ds:[ebx+0xF3C8], 0x1
#endif
	};

#ifdef BUILD_64BIT
	FillMem(pCryNetwork, 0x17C377, code, sizeof(code));
#else
	FillMem(pCryNetwork, 0x43188, code, sizeof(code));
#endif
}

/**
 * Fixes the sporadic crash when file check (sv_cheatProtection) is enabled.
 *
 * Both client and server are affected. Although server is much less prone to crashing. This patch fixes both.
 */
void MemoryPatch::CryNetwork::FixFileCheckCrash(void* pCryNetwork)
{
#ifdef BUILD_64BIT
	const unsigned char codeA[] = {
		0x48, 0x89, 0x0A,  // mov qword ptr ds:[rdx], rcx
		0x90               // nop
	};

	const unsigned char codeB[] = {
		0x48, 0x89, 0x4A, 0x08  // mov qword ptr ds:[rdx+0x8], rcx
	};
#else
	const unsigned char clientCode[] = {
		0x8B, 0x4D, 0xC0,  // mov ecx, dword ptr ss:[ebp-0x40]
		0xFF, 0x49, 0xF4,  // dec dword ptr ds:[ecx-0xC]
		0x8B, 0x4D, 0xBC,  // mov ecx, dword ptr ss:[ebp-0x44]
		0x89, 0x4D, 0xC0   // mov dword ptr ss:[ebp-0x40], ecx
	};

	const unsigned char serverCode[] = {
		0x90,              // nop
		0x90,              // nop
		0xEB, 0x02,        // jmp -------------------------------+
		0x33, 0xC0,        // xor eax, eax                       |
		0x8B, 0x4F, 0x04,  // mov ecx, dword ptr ds:[edi+0x4] <--+
		0xFF, 0x49, 0xF4,  // dec dword ptr ds:[ecx-0xC]
		0x8B, 0x0F,        // mov ecx, dword ptr ds:[edi]
		0x89, 0x4F, 0x04,  // mov dword ptr ds:[edi+0x4], ecx
		0x90,              // nop
		0x90,              // nop
		0x90               // nop
	};
#endif

#ifdef BUILD_64BIT
	// client
	FillMem(pCryNetwork, 0x14F5B1, codeA, sizeof(codeA));
	FillMem(pCryNetwork, 0x14F5C9, codeB, sizeof(codeB));
	// server
	FillMem(pCryNetwork, 0x14F8E1, codeA, sizeof(codeA));
	FillMem(pCryNetwork, 0x14F8F9, codeB, sizeof(codeB));
#else
	// client
	FillNop(pCryNetwork, 0x4A34F, 0xC);
	FillMem(pCryNetwork, 0x4A39E, clientCode, sizeof(clientCode));
	// server
	FillNop(pCryNetwork, 0x49F68, 0xC);
	FillMem(pCryNetwork, 0x30E7B, serverCode, sizeof(serverCode));
#endif
}

/**
 * Allows connecting to Internet servers without GameSpy account.
 */
void MemoryPatch::CryNetwork::FixInternetConnect(void* pCryNetwork)
{
#ifdef BUILD_64BIT
	FillNop(pCryNetwork, 0x189896, 0x18);
#else
	FillNop(pCryNetwork, 0x3FB7C, 0xD);
#endif
}

/**
 * Fixes LAN server browser unable to find any servers when GameSpy is only used for LAN lobby.
 */
void MemoryPatch::CryNetwork::FixLanServerBrowser(void* pCryNetwork)
{
#ifdef BUILD_64BIT
	const unsigned char code[] = {
		0x40, 0x3A, 0xF8,  // cmp dil, al
	};
#else
	const unsigned char code[] = {
		0x38, 0x5D, 0x08,  // cmp byte ptr ss:[ebp+0x8], bl
	};
#endif

#ifdef BUILD_64BIT
	FillMem(pCryNetwork, 0x110D8A, &code, sizeof(code));
#else
	FillMem(pCryNetwork, 0x53936, &code, sizeof(code));
#endif
}

/**
 * Removes pointless GameSpy master server availability check.
 */
void MemoryPatch::CryNetwork::RemoveGameSpyAvailableCheck(void* pCryNetwork)
{
#ifdef BUILD_64BIT
	char* gameName = static_cast<char*>(pCryNetwork) + 0x1FCE50;
	int* gsAvailable = static_cast<int*>(pCryNetwork) + (0x1FCE48 / sizeof(int));
#else
	char* gameName = static_cast<char*>(pCryNetwork) + 0xF17C0;
	int* gsAvailable = static_cast<int*>(pCryNetwork) + (0xF17BC / sizeof(int));
#endif

	std::strcpy(gameName, "crysis");
	*gsAvailable = 1;

#ifdef BUILD_64BIT
	const unsigned char code[] = {
		0xC7, 0x43, 0x28, 0x01, 0x00, 0x00, 0x00,  // mov dword ptr ds:[rbx+0x28], 0x1
		0xC7, 0x43, 0x2C, 0x00, 0x00, 0x00, 0x00,  // mov dword ptr ds:[rbx+0x2C], 0x0
	};
#else
	const unsigned char code[] = {
		0x83, 0xC4, 0x0C,                          // add esp, 0xC
		0xC7, 0x47, 0x14, 0x01, 0x00, 0x00, 0x00,  // mov dword ptr ds:[edi+0x14], 0x1
		0xC7, 0x47, 0x18, 0x00, 0x00, 0x00, 0x00,  // mov dword ptr ds:[edi+0x18], 0x0
	};
#endif

#ifdef BUILD_64BIT
	FillNop(pCryNetwork, 0xF3C5E, 0x5);
	FillNop(pCryNetwork, 0xF3C8E, 0x8B);
	FillMem(pCryNetwork, 0xF3C8E, &code, sizeof(code));
#else
	FillNop(pCryNetwork, 0x586EC, 0x5);
	FillNop(pCryNetwork, 0x5871A, 0x96);
	FillMem(pCryNetwork, 0x5871A, &code, sizeof(code));
#endif
}

/**
 * Used to fix the CryWarning format string vulnerability.
 */
void MemoryPatch::CryNetwork::HookCryWarning(void* pCryNetwork, void (*handler)(int, int, const char* format, ...))
{
#ifdef BUILD_64BIT
	unsigned char code[] = {
		0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0
		0xff, 0xe0,                                                  // jmp rax
	};

	std::memcpy(&code[2], &handler, 8);
#else
	unsigned char code[] = {
		0xb8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0
		0xff, 0xe0,                    // jmp eax
		0x90,                          // nop
	};

	std::memcpy(&code[1], &handler, 4);
#endif

#ifdef BUILD_64BIT
	FillMem(pCryNetwork, 0x24490, &code, sizeof(code));
#else
	FillMem(pCryNetwork, 0xcba2, &code, sizeof(code));
#endif
}

////////////////////////////////////////////////////////////////////////////////
// CryRenderD3D9
////////////////////////////////////////////////////////////////////////////////

#ifdef BUILD_64BIT
static void DoFixUseAfterFreeInShaderParser(void* pCryRender, std::size_t codeOffset,
	std::size_t secondMovOffset,
	std::size_t secondMovSize,
	std::size_t atofRegionOffset,
	std::size_t atofRegionSize)
{
	const std::size_t firstMovOffset = 0x0;
	const std::size_t firstMovSize = secondMovOffset;
	const std::size_t stringDtorRegionOffset = secondMovOffset + secondMovSize;
	const std::size_t stringDtorRegionSize = atofRegionOffset - stringDtorRegionOffset;

	const unsigned char* oldCode = static_cast<unsigned char*>(pCryRender) + codeOffset;
	unsigned char newCode[256];

	// copy and reorder old code
	std::size_t newCodeSize = 0;
	std::memcpy(newCode + newCodeSize, oldCode + secondMovOffset, secondMovSize);
	newCodeSize += secondMovSize;
	std::memcpy(newCode + newCodeSize, oldCode + atofRegionOffset, atofRegionSize);
	newCodeSize += atofRegionSize;
	std::memcpy(newCode + newCodeSize, oldCode + firstMovOffset, firstMovSize);
	newCodeSize += firstMovSize;
	std::memcpy(newCode + newCodeSize, oldCode + stringDtorRegionOffset, stringDtorRegionSize);
	newCodeSize += stringDtorRegionSize;

	const std::size_t newAtofRegionOffset = secondMovSize;

	// patch offsets relative to instruction pointer
	int offset;

	std::memcpy(&offset, newCode + newAtofRegionOffset + 5, 4);
	offset += static_cast<int>(firstMovSize + stringDtorRegionSize);
	std::memcpy(newCode + newAtofRegionOffset + 5, &offset, 4);

	std::memcpy(&offset, newCode + (newCodeSize - 4), 4);
	offset -= static_cast<int>(atofRegionSize);
	std::memcpy(newCode + (newCodeSize - 4), &offset, 4);

	std::memcpy(&offset, newCode + (newCodeSize - 9), 4);
	offset -= static_cast<int>(atofRegionSize);
	std::memcpy(newCode + (newCodeSize - 9), &offset, 4);

	// inject new code
	FillMem(pCryRender, codeOffset, newCode, newCodeSize);
}
#endif

/**
 * Fixes use-after-free bugs in shader parser.
 *
 * The original code first destroys a string and then calls atof on it. This patch simply changes the order.
 */
void MemoryPatch::CryRenderD3D9::FixUseAfterFreeInShaderParser(void* pCryRenderD3D9)
{
#ifdef BUILD_64BIT
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x127C87, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x127D0D, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x12B4D7, 0x5, 0x3, 0x2C, 0xD);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x12C05F, 0x8, 0x3, 0x32, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x12C128, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x12C182, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x12C1DF, 0x8, 0x3, 0x32, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x12C23C, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x12C296, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x12C2F0, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x12C34A, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x12C3A4, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x12C3FB, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x19280C, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D9, 0x192FA7, 0x5, 0x3, 0x2F, 0x12);
#else
	// TODO: 32-bit
#endif
}

static void SetWindowName(char* buffer, const char* name)
{
	constexpr std::size_t BUFFER_SIZE = 80;

	std::size_t length = std::strlen(name);
	if (length >= BUFFER_SIZE)
	{
		length = BUFFER_SIZE - 1;
	}

	std::memcpy(buffer, name, length);
	buffer[length] = '\0';
}

static void HookWindowName(void* pCryRender, std::size_t offset, const char* name)
{
	void* pSetFunc = &SetWindowName;

#ifdef BUILD_64BIT
	unsigned char code[] = {
		0x48, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rdx, 0x0
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0xFF, 0xD0                                                   // call rax
	};

	std::memcpy(&code[2], &name, 8);
	std::memcpy(&code[12], &pSetFunc, 8);
#else
	unsigned char code[] = {
		0xB8, 0x00, 0x00, 0x00, 0x00,        // mov eax, 0x0
		0x50,                                // push eax
		0x8D, 0x86, 0x84, 0x2C, 0x02, 0x00,  // lea eax, dword ptr ds:[esi+0x22C84]
		0x50,                                // push eax
		0xB8, 0x00, 0x00, 0x00, 0x00,        // mov eax, 0x0
		0xFF, 0xD0,                          // call eax
		0x33, 0xC0,                          // xor eax, eax
		0x50,                                // push eax
		0x50,                                // push eax
		0x90,                                // nop
		0x90,                                // nop
		0x90                                 // nop
	};

	std::memcpy(&code[1], &name, 4);
	std::memcpy(&code[14], &pSetFunc, 4);
#endif

	FillMem(pCryRender, offset, code, sizeof(code));
}

/**
 * Sets a new string to be used as title during game window creation.
 *
 * Note that the name pointer must remain valid until the game window is created.
 */
void MemoryPatch::CryRenderD3D9::HookWindowNameD3D9(void* pCryRenderD3D9, const char* name)
{
#ifdef BUILD_64BIT
	HookWindowName(pCryRenderD3D9, 0xD0FF0, name);
#else
	HookWindowName(pCryRenderD3D9, 0x9A8AA, name);
#endif
}

/**
 * Hooks D3D9 adapter information logging.
 */
void MemoryPatch::CryRenderD3D9::HookAdapterInfo(void* pCryRenderD3D9, void (*handler)(AdapterInfo* info))
{
#ifdef BUILD_64BIT
	unsigned char code[] = {
		0x48, 0x8B, 0xCE,                                            // mov rcx, rsi
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0xFF, 0xD0,                                                  // call rax
		0x48, 0x8B, 0xAC, 0x24, 0x30, 0x05, 0x00, 0x00               // mov rbp, qword ptr ss:[rsp+0x530]
	};

	std::memcpy(&code[5], &handler, 8);
#else
	unsigned char code[] = {
		0x55,                               // push ebp
		0xB8, 0x00, 0x00, 0x00, 0x00,       // mov eax, 0x0
		0xFF, 0xD0,                         // call eax
		0x83, 0xC4, 0x04,                   // add esp, 0x4
		0x8B, 0x85, 0x28, 0x04, 0x00, 0x00  // mov eax, dword ptr ss:[ebp+0x428]
	};

	std::memcpy(&code[2], &handler, 4);
#endif

#ifdef BUILD_64BIT
	FillNop(pCryRenderD3D9, 0xC909E, 0x18B);
	FillMem(pCryRenderD3D9, 0xC909E, &code, sizeof(code));
#else
	FillNop(pCryRenderD3D9, 0x95F76, 0x137);
	FillMem(pCryRenderD3D9, 0x95F76, &code, sizeof(code));
#endif
}

////////////////////////////////////////////////////////////////////////////////
// CryRenderD3D10
////////////////////////////////////////////////////////////////////////////////

/**
 * Prevents DX10 renderer from switching to the lowest refresh rate available.
 *
 * Thanks to Guzz and Vladislav for this patch.
 */
void MemoryPatch::CryRenderD3D10::FixLowRefreshRateBug(void* pCryRenderD3D10)
{
#ifdef BUILD_64BIT
	FillNop(pCryRenderD3D10, 0x1C8F45, 0x4);
#else
	FillNop(pCryRenderD3D10, 0x16F3E0, 0x6);
#endif
}

/**
 * Fixes use-after-free bugs in shader parser.
 *
 * The original code first destroys a string and then calls atof on it. This patch simply changes the order.
 */
void MemoryPatch::CryRenderD3D10::FixUseAfterFreeInShaderParser(void* pCryRenderD3D10)
{
#ifdef BUILD_64BIT
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x11D787, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x11D80D, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x120FD7, 0x5, 0x3, 0x2C, 0xD);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x121BAF, 0x8, 0x3, 0x32, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x121C78, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x121CD2, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x121D2F, 0x8, 0x3, 0x32, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x121D8C, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x121DE6, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x121E40, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x121E9A, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x121EF4, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x121F4B, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x18888C, 0x5, 0x3, 0x2F, 0x12);
	DoFixUseAfterFreeInShaderParser(pCryRenderD3D10, 0x189027, 0x5, 0x3, 0x2F, 0x12);
#else
	// TODO: 32-bit
#endif
}

/**
 * Sets a new string to be used as title during game window creation.
 *
 * Note that the name pointer must remain valid until the game window is created.
 */
void MemoryPatch::CryRenderD3D10::HookWindowNameD3D10(void* pCryRenderD3D10, const char* name)
{
#ifdef BUILD_64BIT
	HookWindowName(pCryRenderD3D10, 0xC95DD, name);
#else
	HookWindowName(pCryRenderD3D10, 0x99C57, name);
#endif
}

/**
 * Hooks D3D10 adapter information logging.
 *
 * It also fixes crash of 64-bit DX10 renderer on nVidia driver version 545.92 and possibly others.
 */
void MemoryPatch::CryRenderD3D10::HookAdapterInfo(void* pCryRenderD3D10, void (*handler)(AdapterInfo* info))
{
#ifdef BUILD_64BIT
	unsigned char codeA[] = {
		0x48, 0x8B, 0xF0,                                            // mov rsi, rax
		0x48, 0x8B, 0xC8,                                            // mov rcx, rax
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0xFF, 0xD0,                                                  // call rax
		0x4C, 0x8B, 0x9E, 0x18, 0x01, 0x00, 0x00,                    // mov r11, qword ptr ds:[rsi+0x118]
		0x4C, 0x89, 0x9F, 0x78, 0x8A, 0x02, 0x00                     // mov qword ptr ds:[rdi+0x28A78], r11
	};

	// avoid using RSI register with value corrupted by nVidia driver
	unsigned char codeB[] = {
		0x4C, 0x8B, 0x9F, 0x78, 0x8A, 0x02, 0x00,  // mov r11, qword ptr ds:[rdi+0x28A78]
		0x90,                                      // nop
		0x90,                                      // nop
		0x90,                                      // nop
		0x90,                                      // nop
		0x90,                                      // nop
		0x90,                                      // nop
		0x90                                       // nop
	};

	std::memcpy(&codeA[8], &handler, 8);
#else
	unsigned char code[] = {
		0x50,                          // push eax
		0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0x0
		0xFF, 0xD0,                    // call eax
		// normally, we would do "add esp, 0x4" here, but after this comes "add esp, 0xC"
		0x83, 0xEC, 0x08               // sub esp, 0x8
	};

	std::memcpy(&code[2], &handler, 4);
#endif

#ifdef BUILD_64BIT
	FillNop(pCryRenderD3D10, 0xC71F7, 0xFF);
	FillMem(pCryRenderD3D10, 0xC71F7, &codeA, sizeof(codeA));
	FillMem(pCryRenderD3D10, 0xC7442, &codeB, sizeof(codeB));
#else
	FillNop(pCryRenderD3D10, 0x98268, 0xC8);
	FillMem(pCryRenderD3D10, 0x98268, &code, sizeof(code));
#endif
}

////////////////////////////////////////////////////////////////////////////////
// CryRenderNULL
////////////////////////////////////////////////////////////////////////////////

/**
 * Disables the debug renderer in CryRenderNULL DLL.
 *
 * This patch gets rid of the wasteful debug renderer with its hidden window and OpenGL context.
 *
 * The 1st FillNop disables debug renderer stuff in CNULLRenderAuxGeom constructor.
 * The 2nd FillNop disables debug renderer stuff in CNULLRenderAuxGeom destructor.
 * The 3rd FillMem disables the CNULLRenderAuxGeom::BeginFrame call in CNULLRenderer::BeginFrame.
 * The 4th FillMem disables the CNULLRenderAuxGeom::EndFrame call in CNULLRenderer::EndFrame.
 */
void MemoryPatch::CryRenderNULL::DisableDebugRenderer(void* pCryRenderNULL)
{
	const unsigned char code[] = {
		0xC3,  // ret
#ifdef BUILD_64BIT
		0x90,  // nop
#endif
		0x90,  // nop
		0x90,  // nop
		0x90,  // nop
		0x90,  // nop
		0x90   // nop
	};

	unsigned int renderAuxGeomVTableOffset = 0;

#ifdef BUILD_64BIT
	FillNop(pCryRenderNULL, 0xD379, 0x175);
	FillNop(pCryRenderNULL, 0xD533, 0x35);
	FillMem(pCryRenderNULL, 0x16CE, code, sizeof(code));
	FillMem(pCryRenderNULL, 0x16E0, code, sizeof(code));
	renderAuxGeomVTableOffset = 0x97588;
#else
	FillNop(pCryRenderNULL, 0x1CEE6, 0x101);
	FillNop(pCryRenderNULL, 0x1CFF9, 0xE);
	FillMem(pCryRenderNULL, 0x1895, code, sizeof(code));
	FillMem(pCryRenderNULL, 0x18A9, code, sizeof(code));
	renderAuxGeomVTableOffset = 0xA778C;
#endif

	if (renderAuxGeomVTableOffset)
	{
		void** oldVTable = reinterpret_cast<void**>(
			static_cast<unsigned char*>(pCryRenderNULL) + renderAuxGeomVTableOffset
		);

		// CNULLRenderAuxGeom::SetRenderFlags is empty and returns nothing
		void* emptyFunc = oldVTable[0];

		// create a new CNULLRenderAuxGeom vtable
		void* newVTable[27] = {};

		// keep CNULLRenderAuxGeom::SetRenderFlags
		// keep CNULLRenderAuxGeom::GetRenderFlags
		newVTable[0] = oldVTable[0];
		newVTable[1] = oldVTable[1];

		// make the rest of CNULLRenderAuxGeom functions empty
		// note that all the functions return nothing
		for (int i = 2; i < 27; i++)
		{
			newVTable[i] = emptyFunc;
		}

		// install the new vtable
		FillMem(pCryRenderNULL, renderAuxGeomVTableOffset, newVTable, sizeof(newVTable));
	}
}

////////////////////////////////////////////////////////////////////////////////
// CrySystem
////////////////////////////////////////////////////////////////////////////////

/**
 * Allows Very High settings in DX9 mode.
 */
void MemoryPatch::CrySystem::AllowDX9VeryHighSpec(void* pCrySystem)
{
#ifdef BUILD_64BIT
	FillNop(pCrySystem, 0x4674C, 0x54);
#else
	FillNop(pCrySystem, 0x59DA8, 0x4B);
#endif
}

/**
 * Allows running multiple instances of Crysis at once.
 *
 * Note that the first check if any instance is already running is normally done in launcher.
 */
void MemoryPatch::CrySystem::AllowMultipleInstances(void* pCrySystem)
{
#ifdef BUILD_64BIT
	FillNop(pCrySystem, 0x42BFF, 0x68);
#else
	FillNop(pCrySystem, 0x5794F, 0x58);
#endif
}

/**
 * Prevents out-of-bounds access of the CPUInfo::cores array.
 */
void MemoryPatch::CrySystem::FixCPUInfoOverflow(void* pCrySystem)
{
#ifdef BUILD_64BIT
	FillNop(pCrySystem, 0x3801D, 0x1A);
#else
	FillNop(pCrySystem, 0x4B4A0, 0x9);
#endif
}

/**
 * Prevents Flash memory allocator from causing buffer underflow.
 *
 * This is normally harmless as it's read-only buffer underflow, but it annoys debug allocator.
 */
void MemoryPatch::CrySystem::FixFlashAllocatorUnderflow(void* pCrySystem)
{
#ifdef BUILD_64BIT
	FillNop(pCrySystem, 0xDEE82, 0x10);
	FillNop(pCrySystem, 0xDEF0F, 0x10);
#else
	// TODO: 32-bit
#endif
}

/**
 * Hooks CryEngine CPU detection.
 */
void MemoryPatch::CrySystem::HookCPUDetect(void* pCrySystem, void (*handler)(CPUInfo* info))
{
#ifdef BUILD_64BIT
	unsigned char code[] = {
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0xFF, 0xE0                                                   // jmp rax
	};

	std::memcpy(&code[2], &handler, 8);
#else
	unsigned char code[] = {
		0x51,                          // push ecx
		0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0x0
		0xFF, 0xD0,                    // call eax
		0x83, 0xC4, 0x04,              // add esp, 0x4
		0xC3                           // ret
	};

	std::memcpy(&code[2], &handler, 4);
#endif

#ifdef BUILD_64BIT
	FillMem(pCrySystem, 0xA7E0, &code, sizeof(code));
#else
	FillMem(pCrySystem, 0xA380, &code, sizeof(code));
#endif
}

/**
 * Hooks CryEngine fatal error handler.
 */
void MemoryPatch::CrySystem::HookError(void* pCrySystem, void (*handler)(const char* format, va_list args))
{
	// convert thiscall into a normal function call
	// and call our handler
#ifdef BUILD_64BIT
	unsigned char code[] = {
		0x48, 0x89, 0x54, 0x24, 0x10,                                // mov qword ptr ss:[rsp+0x10], rdx
		0x4C, 0x89, 0x44, 0x24, 0x18,                                // mov qword ptr ss:[rsp+0x18], r8
		0x4C, 0x89, 0x4C, 0x24, 0x20,                                // mov qword ptr ss:[rsp+0x20], r9
		0x48, 0x83, 0xEC, 0x28,                                      // sub rsp, 0x28
		0x48, 0x8B, 0xCA,                                            // mov rcx, rdx
		0x48, 0x8D, 0x54, 0x24, 0x40,                                // lea rdx, qword ptr ss:[rsp+0x40]
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0xFF, 0xD0,                                                  // call rax
		0x48, 0x83, 0xC4, 0x28,                                      // add rsp, 0x28
		0xC3                                                         // ret
	};

	std::memcpy(&code[29], &handler, 8);
#else
	unsigned char code[] = {
		0x8B, 0x4C, 0x24, 0x08,        // mov ecx, dword ptr ss:[esp+0x8]
		0x8D, 0x44, 0x24, 0x0C,        // lea eax, dword ptr ss:[esp+0xC]
		0x50,                          // push eax
		0x51,                          // push ecx
		0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0x0
		0xFF, 0xD0,                    // call eax
		0x83, 0xC4, 0x08,              // add esp, 0x8
		0xC3                           // ret
	};

	std::memcpy(&code[11], &handler, 4);
#endif

#ifdef BUILD_64BIT
	FillMem(pCrySystem, 0x52D00, &code, sizeof(code));
#else
	FillMem(pCrySystem, 0x63290, &code, sizeof(code));
#endif
}

/**
 * Makes DX9 mode the default.
 *
 * It is still possible to use the "-dx10" command line parameter to run the game in DX10 mode.
 */
void MemoryPatch::CrySystem::MakeDX9Default(void* pCrySystem)
{
#ifdef BUILD_64BIT
	FillNop(pCrySystem, 0x46719, 0x1F);
#else
	FillNop(pCrySystem, 0x59D86, 0x5);
#endif
}

/**
 * Disables the SecuROM crap in 64-bit CrySystem DLL.
 */
void MemoryPatch::CrySystem::RemoveSecuROM(void* pCrySystem)
{
#ifdef BUILD_64BIT
	FillNop(pCrySystem, 0x470B9, 0x16);
#endif
}

/**
 * Prevents the engine from installing its own broken unhandled exceptions handler.
 */
void MemoryPatch::CrySystem::UnhandledExceptions(void* pCrySystem)
{
#ifdef BUILD_64BIT
	FillNop(pCrySystem, 0x22946, 0x6);
	FillNop(pCrySystem, 0x22952, 0x7);
	FillNop(pCrySystem, 0x467A5, 0x16);
#else
	FillNop(pCrySystem, 0x17D67, 0x5);
	FillNop(pCrySystem, 0x17D72, 0xC);
	FillNop(pCrySystem, 0x59DF8, 0x13);
#endif
}

/**
 * Enables physics thread on server.
 */
void MemoryPatch::CrySystem::EnableServerPhysicsThread(void* pCrySystem)
{
#ifdef BUILD_64BIT
	FillNop(pCrySystem, 0x36CD6, 0x11);
#else
	FillNop(pCrySystem, 0x4CBC1, 0xD);
#endif
}

/**
 * Used to fix the CryWarning format string vulnerability.
 */
void MemoryPatch::CrySystem::HookCryWarning(void* pCrySystem, void (*handler)(int, int, const char* format, ...))
{
#ifdef BUILD_64BIT
	unsigned char code[] = {
		0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0
		0xff, 0xe0,                                                  // jmp rax
	};

	std::memcpy(&code[2], &handler, 8);
#else
	unsigned char code[] = {
		0xb8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0
		0xff, 0xe0,                    // jmp eax
		0x90,                          // nop
		0x90,                          // nop
		0x90,                          // nop
	};

	std::memcpy(&code[1], &handler, 4);
#endif

#ifdef BUILD_64BIT
	FillMem(pCrySystem, 0x16e0, &code, sizeof(code));
#else
	FillMem(pCrySystem, 0x3980, &code, sizeof(code));
#endif
}

////////////////////////////////////////////////////////////////////////////////
// FMODEx
////////////////////////////////////////////////////////////////////////////////

/**
 * Fixes truncation of 64-bit heap buffer addresses inside FMOD.
 */
void MemoryPatch::FMODEx::Fix64BitHeapAddressTruncation(void* pFMODEx)
{
#ifdef BUILD_64BIT
	const unsigned char code[] = {
		0x48, 0x8D, 0x40, 0x0F,              // lea rax, qword ptr ds:[rax+0xF]
		0x48, 0x83, 0xE0, 0xF0,              // and rax, 0xFFFFFFFFFFFFFFF0
		0x90,                                // nop
		0x90,                                // nop
		0x90,                                // nop
		0x41, 0xB9, 0x3C, 0x00, 0x00, 0x00,  // mov r9d, 0x3C
	};

	FillMem(pFMODEx, 0x482DA, &code, sizeof(code) - 6);
	FillMem(pFMODEx, 0x486B7, &code, sizeof(code));
#endif
}
