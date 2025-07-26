#pragma once
#include "CryCommon/CryScriptSystem/IScriptSystem.h"

#include "CryMP/Server/SafeWriting/API.h"
#include <map>
#include <string>


struct IFunctionHandler;
struct ISystem;
struct IScriptSystem;

class FunctionRegisterer : public CScriptableBase {
public:
    FunctionRegisterer(ISystem*, IGameFramework*, SafeWritingAPI*);
    int RunFunc(IFunctionHandler* pH, const char* name);
    int LoadDLL(IFunctionHandler* pH, const char* path);
    int Is64Bit(IFunctionHandler* pH);
protected:
    void RegisterMethods();
    ISystem* m_pSystem;
    IScriptSystem* m_pSS;
    IGameFramework* m_pGameFW;
    SafeWritingAPI* m_pAPI;
};

class CSafeWritingAPI : public SafeWritingAPI {
public:
    CSafeWritingAPI();
    ~CSafeWritingAPI() override;

    void RegisterLuaFunc(const char* name, SafeWritingAPIFunc func) override;

    void RegisterLuaGlobal(const char* name, int num) override;
    void RegisterLuaGlobal(const char* name, bool num) override;
    void RegisterLuaGlobal(const char* name, const char* str) override;
    void RegisterLuaGlobal(const char* name, float num) override;

    void GetLuaGlobal(const char* name, int& num) override;
    void GetLuaGlobal(const char* name, bool& num) override;
    void GetLuaGlobal(const char* name, const char*& str) override;
    void GetLuaGlobal(const char* name, float& num) override;

    void GetArg(int idx, int& num) override;
    void GetArg(int idx, bool& num) override;
    void GetArg(int idx, const char*& str) override;
    void GetArg(int idx, float& num) override;

    void StartReturn() override;
    void EndReturn() override;
    void ReturnToLua(int num) override;
    void ReturnToLua(bool num) override;
    void ReturnToLua(const char* str) override;
    void ReturnToLua(float num) override;

    void PushFuncParam(int p) override;
    void PushFuncParam(bool p) override;
    void PushFuncParam(float p) override;
    void PushFuncParam(const char* p) override;

    void BeginFuncCall(const char* name) override;
    void BeginFuncCall(const char* table, const  char* name) override;
    void CallFunction() override;

    IGameFramework* GetIGameFramework() override;
    SafeWritingAPIFunc FindFunc(const char* name) override;
private:
    std::map<std::string, SafeWritingAPIFunc> funcMap;
    int retCtr;
};