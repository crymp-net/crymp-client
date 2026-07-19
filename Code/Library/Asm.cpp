#include "Asm.h"
#include <stdint.h>
#include <vector>

#include <Windows.h>
#include <zydis/Decoder.h>

void* Asm::CloneFunction(const char* moduleName, const char* funcName) {
	// This function clones an existing function by copying original source code
	// into new memory that has executable flag and rewriting all relative CALLs (E8 opcode)
	// to either a new CALL with a proper relative address from cloned function
	// or into a code cave right after cloned function that performs the calling
	HMODULE hModule = GetModuleHandleA(moduleName);
	if (hModule == NULL) {
		return NULL;
	}

	void* pfn = GetProcAddress(hModule, funcName);
	if (pfn == NULL) {
		return NULL;
	}

	uintptr_t needle = reinterpret_cast<uintptr_t>(pfn);
	uintptr_t start = needle;
	for (uintptr_t i = 0; i < 100000; needle++, i++) {
		uint32_t banner = *reinterpret_cast<uint32_t*>(needle);
		if (banner == 0xCCCCCCCC) {
			break;
		}
	}

	uintptr_t end = needle;
	size_t size = needle - start;
	ZydisDecoder decoder;
#ifdef BUILD_64BIT
	const bool is64 = true;
#else
	const bool is64 = false;
#endif
	const ZydisMachineMode machineMode =
		is64
		? ZYDIS_MACHINE_MODE_LONG_64
		: ZYDIS_MACHINE_MODE_LEGACY_32;

	const ZydisStackWidth stackWith =
		is64
		? ZYDIS_STACK_WIDTH_64
		: ZYDIS_STACK_WIDTH_32;

	if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder, machineMode, stackWith)))
		return NULL;

	std::vector<uintptr_t> calls;

	for (needle = start; needle < end;) {
		ZydisDecodedInstruction instr;
		ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
		ZyanStatus status = ZydisDecoderDecodeFull(
			&decoder,
			reinterpret_cast<void*>(needle),
			end - start,
			&instr,
			operands
		);
		if (!ZYAN_SUCCESS(status)) break;
		if (instr.opcode == 0xE8) {
			calls.push_back(needle);
		}
		needle += instr.length;
	}

	size_t extraSpaceNeeded = 0;
	size_t perCave = 0;
#ifdef BUILD_64BIT
	perCave = 16;
	extraSpaceNeeded = calls.size() * perCave;
#endif

	DWORD flags = PAGE_EXECUTE_READWRITE;
	void* pMem = VirtualAlloc(NULL, size + extraSpaceNeeded, MEM_COMMIT, flags);
	if (pMem == NULL) {
		return NULL;
	}

	memcpy(pMem, pfn, size);

#ifdef BUILD_64BIT
	// For 64-bit, we replace every CALL with a JMP to a cave that begins after
	// the function being cloned and construct the cave to call the desired address
	// and return back from cave to the original function
	uintptr_t mem = reinterpret_cast<uintptr_t>(pMem);
	uintptr_t cavesStart = mem + size;
	for (size_t i = 0; i < calls.size(); i++) {
		uintptr_t call = calls[i];
		uintptr_t rel = call - start;

		uint8_t* callSite = reinterpret_cast<uint8_t*>(mem + rel);
		uint8_t* caveStart = reinterpret_cast<uint8_t*>(cavesStart + i * perCave);

		uint32_t originalArg = *reinterpret_cast<uint32_t*>(callSite + 1);
		uintptr_t callDestination = call + originalArg + 5;

		// Replace CALL External with JMP Cave
		callSite[0] = 0xE9;
		uint32_t dest = size + i * perCave;
		uint32_t src = rel;
		uint32_t jmpToCave = dest - src - 5;
		memcpy(callSite + 1, &jmpToCave, 4);

		// Construct the cave
		dest = rel + 5;
		src = (size + i * perCave + 12);
		uint32_t jmpBack = dest - src - 5;
		char cave[] = {
			0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0
			0xff, 0xd0,                                                  // call rax
			0xe9, 0x00, 0x00, 0x00, 0x00								 // jmp back
		};
		memcpy(cave + 2, &callDestination, 8);
		memcpy(cave + 13, &jmpBack, 4);
		memcpy(caveStart, cave, 17);
	}
#else
	// For 32-bit we simply just rewrite CALL operand with a new relative address since
	// all addresses are 32-bit anyway
	uintptr_t mem = reinterpret_cast<uintptr_t>(pMem);
	for (size_t i = 0; i < calls.size(); i++) {
		uintptr_t call = calls[i];
		uintptr_t rel = call - start;
		uint8_t* callSite = reinterpret_cast<uint8_t*>(mem + rel);
		uint32_t originalArg = *reinterpret_cast<uint32_t*>(callSite + 1);
		uintptr_t callDestination = call + originalArg + 5;
		uint32_t src = mem + rel;
		uint32_t callArg = callDestination - src - 5;

		memcpy(callSite + 1, &callArg, 4);
	}
#endif

	if (!VirtualProtect(pMem, size, PAGE_EXECUTE_READ, &flags)) {
		VirtualFree(pMem, 0, MEM_RELEASE);
		return NULL;
	}

	return pMem;
}