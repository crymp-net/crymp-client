#include "CryMP/Server/SafeWriting/APIImpl.h"

#include "CryCommon/CryAction/IGameFramework.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryCommon/CrySystem/ISystem.h"
#include <Windows.h>

CSafeWritingAPI::CSafeWritingAPI() { retCtr = 0; }
CSafeWritingAPI::~CSafeWritingAPI() {}

void CSafeWritingAPI::RegisterLuaGlobal(const char* name, int num) {
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->SetGlobalValue(name, num);
}

void CSafeWritingAPI::RegisterLuaGlobal(const char* name, float num) {
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->SetGlobalValue(name, num);
}

void CSafeWritingAPI::RegisterLuaGlobal(const char* name, const char* str) {
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->SetGlobalValue(name, str);
}

void CSafeWritingAPI::RegisterLuaGlobal(const char* name, bool num) {
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->SetGlobalValue(name, num);
}

void CSafeWritingAPI::PushFuncParam(int val) {
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->PushFuncParam(val);
}

void CSafeWritingAPI::PushFuncParam(bool val) {
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->PushFuncParam(val);
}

void CSafeWritingAPI::PushFuncParam(float val) {
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->PushFuncParam(val);
}

void CSafeWritingAPI::PushFuncParam(const char* val) {
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->PushFuncParam(val);
}

void CSafeWritingAPI::BeginFuncCall(const char* name) {
	IScriptSystem* ss = gEnv->pScriptSystem;
	int res = ss->BeginCall("_G", name);
}

void CSafeWritingAPI::BeginFuncCall(const char* table, const char* name) {
	IScriptSystem* ss = gEnv->pScriptSystem;
	int res = ss->BeginCall(table, name);
}

void CSafeWritingAPI::CallFunction() {
	IScriptSystem* ss = gEnv->pScriptSystem;
	int res = ss->EndCall();
}

void CSafeWritingAPI::StartReturn() {
	retCtr = 0;
}

void CSafeWritingAPI::EndReturn() {
	RegisterLuaGlobal("__CPP__CNT__", retCtr);
	retCtr = 0;
}

void CSafeWritingAPI::ReturnToLua(int num) {
	char n[30];
	sprintf(n, "__CPP__RET__%d", retCtr++);
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->SetGlobalValue(n, num);
}

void CSafeWritingAPI::ReturnToLua(float num) {
	char n[30];
	sprintf(n, "__CPP__RET__%d", retCtr++);
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->SetGlobalValue(n, num);
}

void CSafeWritingAPI::ReturnToLua(const char* str) {
	char n[30];
	sprintf(n, "__CPP__RET__%d", retCtr++);
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->SetGlobalValue(n, str);
}

void CSafeWritingAPI::ReturnToLua(bool num) {
	char n[30];
	sprintf(n, "__CPP__RET__%d", retCtr++);
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->SetGlobalValue(n, num);
}

void CSafeWritingAPI::RegisterLuaFunc(const char* name, SafeWritingAPIFunc func) {
	funcMap[name] = func;
}

void CSafeWritingAPI::GetLuaGlobal(const char* name, int& num) {
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->GetGlobalValue(name, num);
}

void CSafeWritingAPI::GetLuaGlobal(const char* name, float& num) {
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->GetGlobalValue(name, num);
}

void CSafeWritingAPI::GetLuaGlobal(const char* name, const char*& str) {
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->GetGlobalValue(name, str);
}

void CSafeWritingAPI::GetLuaGlobal(const char* name, bool& num) {
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->GetGlobalValue(name, num);
}

void CSafeWritingAPI::GetArg(int idx, int& num) {
	char n[30];
	sprintf(n, "__CPP__ARG__%d", idx);
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->GetGlobalValue(n, num);
}

void CSafeWritingAPI::GetArg(int idx, bool& num) {
	char n[30];
	sprintf(n, "__CPP__ARG__%d", idx);
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->GetGlobalValue(n, num);
}

void CSafeWritingAPI::GetArg(int idx, float& num) {
	char n[30];
	sprintf(n, "__CPP__ARG__%d", idx);
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->GetGlobalValue(n, num);
}

void CSafeWritingAPI::GetArg(int idx, const char*& str) {
	char n[30];
	sprintf(n, "__CPP__ARG__%d", idx);
	IScriptSystem* ss = gEnv->pScriptSystem;
	ss->GetGlobalValue(n, str);
}

IGameFramework* CSafeWritingAPI::GetIGameFramework() {
	return gEnv->pGame->GetIGameFramework();
}

SafeWritingAPIFunc CSafeWritingAPI::FindFunc(const char* name) {
	auto it = funcMap.find(name);
	if (it != funcMap.end()) {
		return it->second;
	} else {
		return NULL;
	}
}

FunctionRegisterer::FunctionRegisterer(ISystem* pSystem, IGameFramework* pGameFramework, SafeWritingAPI* pAPI)
	:	m_pSystem(pSystem),
		m_pSS(pSystem->GetIScriptSystem()),
		m_pGameFW(pGameFramework),
		m_pAPI(pAPI)
{
	Init(m_pSS, m_pSystem);
	SetGlobalName("DLLAPI100");
	RegisterMethods();
}

void FunctionRegisterer::RegisterMethods() {
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &FunctionRegisterer::
	SetGlobalName("DLLAPI");
	SCRIPT_REG_TEMPLFUNC(RunFunc, "name");
	SCRIPT_REG_TEMPLFUNC(LoadDLL, "path");
	SCRIPT_REG_FUNC(Is64Bit);
}

int FunctionRegisterer::RunFunc(IFunctionHandler* pH, const char* name) {
	if (auto f = m_pAPI->FindFunc(name)) {
		f();
		return pH->EndFunction(true);
	}
	return pH->EndFunction(false);
}

int FunctionRegisterer::LoadDLL(IFunctionHandler* pH, const char* name) {
	HMODULE lib = LoadLibraryA(name);
	typedef void(*PFINIT)(void*);
	if (lib == NULL) {
		CryLogAlways("$6[CSafeWritingAPI] Failed to load DLL! Error %d (%s)", GetLastError(), name);
		return pH->EndFunction(false);
	}
	PFINIT f = (PFINIT)GetProcAddress(lib, "Init");
	if (!f) {
		CryLogAlways("$6[CSafeWritingAPI] Address of Init inside DLL is unknown! Error %d", GetLastError());
		return pH->EndFunction(false);
	} else {
		f(m_pAPI);
	}
	return pH->EndFunction(true);
}

int FunctionRegisterer::Is64Bit(IFunctionHandler* pH) {
#ifdef BUILD_64BIT
	return pH->EndFunction(true);
#else
	return pH->EndFunction(false);
#endif
}
