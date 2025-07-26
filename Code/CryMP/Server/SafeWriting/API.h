#pragma once

struct IGameFramework;

typedef void (*SafeWritingAPIFunc)(void);

struct SafeWritingAPI {
public:
    virtual ~SafeWritingAPI() = default;

    virtual void RegisterLuaFunc(const char* name, SafeWritingAPIFunc func) = 0;

    virtual void RegisterLuaGlobal(const char* name, int num) = 0;
    virtual void RegisterLuaGlobal(const char* name, bool num) = 0;
    virtual void RegisterLuaGlobal(const char* name, const char* str) = 0;
    virtual void RegisterLuaGlobal(const char* name, float num) = 0;

    virtual void GetLuaGlobal(const char* name, int& num) = 0;
    virtual void GetLuaGlobal(const char* name, bool& num) = 0;
    virtual void GetLuaGlobal(const char* name, const  char*& str) = 0;
    virtual void GetLuaGlobal(const char* name, float& num) = 0;


    virtual void GetArg(int idx, int& num) = 0;
    virtual void GetArg(int idx, bool& num) = 0;
    virtual void GetArg(int idx, const  char*& str) = 0;
    virtual void GetArg(int idx, float& num) = 0;

    virtual void StartReturn() = 0;
    virtual void EndReturn() = 0;
    virtual void ReturnToLua(int num) = 0;
    virtual void ReturnToLua(bool num) = 0;
    virtual void ReturnToLua(const char* str) = 0;
    virtual void ReturnToLua(float num) = 0;

    virtual void PushFuncParam(int p) = 0;
    virtual void PushFuncParam(bool p) = 0;
    virtual void PushFuncParam(float p) = 0;
    virtual void PushFuncParam(const char* p) = 0;

    virtual void BeginFuncCall(const char* name) = 0;
    virtual void BeginFuncCall(const char* table, const char* name) = 0;
    virtual void CallFunction() = 0;

    virtual IGameFramework* GetIGameFramework() = 0;
    virtual SafeWritingAPIFunc FindFunc(const char* name) = 0;
};