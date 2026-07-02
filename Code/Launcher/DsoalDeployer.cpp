#ifdef CLIENT_LAUNCHER

#include <cstddef>

#include <windows.h>
#include <dsound.h>

#include "CrySystem/CryLog.h"
#include "Library/WinAPI.h"

#include "DsoalDeployer.h"

extern "C" HRESULT WINAPI DSOAL_DirectSoundCreate8(const GUID* deviceId, IDirectSound8** ds, IUnknown* outer);
extern "C" HRESULT WINAPI DSOAL_DirectSoundEnumerateA(LPDSENUMCALLBACKA callback, void* userPtr);
extern "C" HRESULT WINAPI DSOAL_DirectSoundCaptureEnumerateA(LPDSENUMCALLBACKA callback, void* userPtr);
extern "C" HRESULT WINAPI DSOAL_DirectSoundCaptureCreate(const GUID* deviceId, IDirectSoundCapture** ds, IUnknown* outer);

struct FmodContext
{
	bool something;
	unsigned int reserved2;
	unsigned int someFlags;
	void* dsound;
	void* dsound3d;
	void* pDirectSoundCreate8;
	void* pDirectSoundEnumerateA;
	void* reserved3;
	void* reserved4;
	void* pDirectSoundCaptureEnumerateA;
	void* pDirectSoundCaptureCreate;
};

#ifdef BUILD_64BIT
static_assert(offsetof(FmodContext, something) == 0x0);
static_assert(offsetof(FmodContext, someFlags) == 0x8);
static_assert(offsetof(FmodContext, dsound) == 0x10);
static_assert(offsetof(FmodContext, dsound3d) == 0x18);
static_assert(offsetof(FmodContext, pDirectSoundCreate8) == 0x20);
static_assert(offsetof(FmodContext, pDirectSoundEnumerateA) == 0x28);
static_assert(offsetof(FmodContext, pDirectSoundCaptureEnumerateA) == 0x40);
static_assert(offsetof(FmodContext, pDirectSoundCaptureCreate) == 0x48);
#else
static_assert(offsetof(FmodContext, something) == 0x0);
static_assert(offsetof(FmodContext, someFlags) == 0x8);
static_assert(offsetof(FmodContext, dsound) == 0xC);
static_assert(offsetof(FmodContext, dsound3d) == 0x10);
static_assert(offsetof(FmodContext, pDirectSoundCreate8) == 0x14);
static_assert(offsetof(FmodContext, pDirectSoundEnumerateA) == 0x18);
static_assert(offsetof(FmodContext, pDirectSoundCaptureEnumerateA) == 0x24);
static_assert(offsetof(FmodContext, pDirectSoundCaptureCreate) == 0x28);
#endif

static void OnDirectSoundInit(FmodContext* context)
{
	CryLogAlways("$3[CryMP] Using DSOAL and OpenAL for sound");

	// no dsound.dll
	context->dsound = nullptr;
	context->dsound3d = nullptr;

	context->pDirectSoundCreate8 = &DSOAL_DirectSoundCreate8;
	context->pDirectSoundEnumerateA = &DSOAL_DirectSoundEnumerateA;
	context->pDirectSoundCaptureEnumerateA = &DSOAL_DirectSoundCaptureEnumerateA;
	context->pDirectSoundCaptureCreate = &DSOAL_DirectSoundCaptureCreate;

	context->someFlags = 0x9;
	context->something = true;
}

void DsoalDeployer::Init(void* pFmodEx, bool isOldFmod)
{
	void* pHook = &OnDirectSoundInit;

#ifdef BUILD_64BIT
	constexpr std::size_t codeOffsetOld = 0x2FE82;
	constexpr std::size_t codeOffsetNew = 0x38DFA;
	constexpr std::size_t codeMaxSizeOld = 0x18F;
	constexpr std::size_t codeMaxSizeNew = 0x1A2;

	const unsigned int contextBaseSize = isOldFmod ? 0x320 : 0x558;

	unsigned char code[] = {
		0x48, 0x81, 0xC1, 0x00, 0x00, 0x00, 0x00,                    // add rcx, 0x0
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0x0
		0xFF, 0xD0,                                                  // call rax
	};

	std::memcpy(&code[3], &contextBaseSize, 4);
	std::memcpy(&code[9], &pHook, 8);
#else
	constexpr std::size_t codeOffsetOld = 0x2ABA6;
	constexpr std::size_t codeOffsetNew = 0x2E856;
	constexpr std::size_t codeMaxSizeOld = 0x145;
	constexpr std::size_t codeMaxSizeNew = 0x156;

	const unsigned int contextBaseSize = isOldFmod ? 0x22C : 0x3C0;

	unsigned char code[] = {
		0x8B, 0xC6,                    // mov eax, esi
		0x05, 0x00, 0x00, 0x00, 0x00,  // add eax, 0x0
		0x50,                          // push eax
		0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0x0
		0xFF, 0xD0,                    // call eax
		0x83, 0xC4, 0x04,              // add esp, 0x4
		0x33, 0xC0,                    // xor eax, eax
	};

	std::memcpy(&code[3], &contextBaseSize, 4);
	std::memcpy(&code[9], &pHook, 4);
#endif

	static_assert(sizeof(code) <= codeMaxSizeOld);
	static_assert(sizeof(code) <= codeMaxSizeNew);

	const std::size_t codeOffset = isOldFmod ? codeOffsetOld : codeOffsetNew;
	const std::size_t codeMaxSize = isOldFmod ? codeMaxSizeOld : codeMaxSizeNew;

	WinAPI::FillMem(WinAPI::RVA(pFmodEx, codeOffset), code, sizeof(code));
	WinAPI::FillNOP(WinAPI::RVA(pFmodEx, codeOffset + sizeof(code)), codeMaxSize - sizeof(code));
}

#endif  // CLIENT_LAUNCHER
