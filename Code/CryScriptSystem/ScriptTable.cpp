#include <cstdio>
#include <cstring>
#include <new>
#include <cstddef>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
}

#include "CryCommon/CrySystem/ISystem.h"
#include "ScriptTable.h"
#include "ScriptSystem.h"
#include "ScriptUtil.h"
#include "FunctionHandler.h"

struct FunctionData
{
    ScriptSystem *pSS;
    char signature[256];
    int paramIdOffset;
    IScriptTable::FunctionFunctor functor;
    IScriptTable::UserDataFunction userDataFunc;
    int userDataSize;
    unsigned char userDataBuffer[1];  // this must be at the end of this structure !!
    
    void SetSignature(const IScriptTable::SUserFunctionDesc & fd)
    {
        if (*fd.sGlobalName)
            std::snprintf(signature, sizeof signature, "%s.%s(%s)",
                          fd.sGlobalName, fd.sFunctionName, fd.sFunctionParams);
        else
            std::snprintf(signature, sizeof signature, "%s(%s)",
                          fd.sFunctionName, fd.sFunctionParams);
    }
};

ScriptTable *ScriptTable::Create(ScriptSystem *pSS, lua_State *L, bool empty)
{
    ScriptTable *pTable = AllocateTable(pSS);

    pTable->m_L = L;
    pTable->m_pSS = pSS;

    if (!empty)
    {
        pTable->Init();
    }

    return pTable;
}

void ScriptTable::Init()
{
    lua_newtable(m_L);
    Attach();
}

void ScriptTable::Attach()
{
    if (m_ref > 0)
    {
        luaL_unref(m_L, LUA_REGISTRYINDEX, m_ref);
    }

    m_ref = luaL_ref(m_L, LUA_REGISTRYINDEX);
}

void ScriptTable::PushRef()
{
    if (m_ref > 0)
    {
        lua_rawgeti(m_L, LUA_REGISTRYINDEX, m_ref);
    }
    else
    {
        CryLogWarningAlways("[Script] Pushing nil table");
        lua_pushnil(m_L);
    }
}

void ScriptTable::SetMetatable(IScriptTable *pMetatable)
{
    // -2
    PushRef();
    // -1
    static_cast<ScriptTable*>(pMetatable)->PushRef();

    lua_setmetatable(m_L, -2);

    // pop table
    lua_pop(m_L, 1);
}

//////////////////
// IScriptTable //
//////////////////

IScriptSystem *ScriptTable::GetScriptSystem() const
{
    return m_pSS;
}

void ScriptTable::AddRef()
{
    m_refCount++;
}

void ScriptTable::Release()
{
    m_refCount--;

    if (m_refCount <= 0)
    {
        if (m_ref > 0)
        {
            luaL_unref(m_L, LUA_REGISTRYINDEX, m_ref);
            m_ref = 0;
        }

        DeallocateTable(this, m_pSS);
    }
}

void ScriptTable::Delegate(IScriptTable *pMetatable)
{
    if (!pMetatable)
        return;

    static_cast<ScriptTable*>(pMetatable)->PushRef();
    lua_pushstring(m_L, "__index");  // push key
    static_cast<ScriptTable*>(pMetatable)->PushRef();
    lua_rawset(m_L, -3);  // set metatable.__index = metatable
    lua_pop(m_L, 1);      // pop metatable from stack

    SetMetatable(pMetatable);
}

void *ScriptTable::GetUserDataValue()
{
    PushRef();

    void *data = lua_touserdata(m_L, -1);

    lua_pop(m_L, 1);

    return data;
}

void ScriptTable::SetValueAny(const char *key, const ScriptAnyValue & any, bool isChain)
{
    const int top = lua_gettop(m_L);

    if (!isChain)
        PushRef();

    if (any.type == ANY_TVECTOR)
    {
        // check if we can reuse Vec3 value already in the table
        lua_pushstring(m_L, key);
        lua_gettable(m_L, -2);

        if (lua_type(m_L, -1) == LUA_TTABLE)
        {
            lua_pushstring(m_L, "x");
            lua_gettable(m_L, -2);

            const bool isNumber = (lua_isnumber(m_L, -1) != 0);

            // pop x value
            lua_pop(m_L, 1);

            if (isNumber)
            {
                // assume it's a vector, just fill it with new vector values
                lua_pushstring(m_L, "x");
                lua_pushnumber(m_L, any.vec3.x);
                lua_settable(m_L, -3);

                lua_pushstring(m_L, "y");
                lua_pushnumber(m_L, any.vec3.y);
                lua_settable(m_L, -3);

                lua_pushstring(m_L, "z");
                lua_pushnumber(m_L, any.vec3.z);
                lua_settable(m_L, -3);

                lua_settop(m_L, top);
                return;
            }
        }

        // pop key value
        lua_pop(m_L, 1);
    }

    lua_pushstring(m_L, key);
    m_pSS->PushAny(any);
    lua_rawset(m_L, -3);

    lua_settop(m_L, top);
}

bool ScriptTable::GetValueAny(const char *key, ScriptAnyValue & any, bool isChain)
{
    const int top = lua_gettop(m_L);

    if (!isChain)
        PushRef();

    lua_pushstring(m_L, key);
    lua_gettable(m_L, -2);

    const bool status = m_pSS->PopAny(any);

    lua_settop(m_L, top);

    return status;
}

bool ScriptTable::BeginSetGetChain()
{
    PushRef();
    return true;
}

void ScriptTable::EndSetGetChain()
{
    if (lua_istable(m_L, -1))
    {
        lua_pop(m_L, 1);
    }
    else
    {
        CryLogErrorAlways("[ScriptTable] Mismatch in Set/Get Chain");
    }
}

ScriptVarType ScriptTable::GetValueType(const char *key)
{
    PushRef();
    lua_pushstring(m_L, key);
    lua_gettable(m_L, -2);

    const ScriptVarType type = ScriptUtil::LuaTypeToScriptVarType(lua_type(m_L, -1));

    lua_pop(m_L, 2);

    return type;
}

ScriptVarType ScriptTable::GetAtType(int index)
{
    ScriptVarType type = svtNull;

    PushRef();

    // Use luaL_len/lua_objlen for table length, assuming an array-like table
    if (luaL_len(m_L, -1) < index)
    {
        lua_pop(m_L, 1);
    }
    else
    {
        lua_rawgeti(m_L, -1, index);
        type = ScriptUtil::LuaTypeToScriptVarType(lua_type(m_L, -1));
        lua_pop(m_L, 2);
    }

    return type;
}

void ScriptTable::SetAtAny(int index, const ScriptAnyValue & any)
{
    PushRef();
    m_pSS->PushAny(any);
    lua_rawseti(m_L, -2, index);
    lua_pop(m_L, 1);
}

bool ScriptTable::GetAtAny(int index, ScriptAnyValue & any)
{
    PushRef();
    lua_rawgeti(m_L, -1, index);

    const bool status = m_pSS->PopAny(any);

    lua_pop(m_L, 1);

    return status;
}

ScriptTable::Iterator ScriptTable::BeginIteration()
{
    Iterator it;
    it.nKey = -1;
    it.sKey = nullptr;
    it.nInternal = lua_gettop(m_L) + 1;

    PushRef();
    lua_pushnil(m_L);

    return it;
}

bool ScriptTable::MoveNext(Iterator & it)
{
    if (!it.nInternal)
        return false;

    const int top = it.nInternal - 1;

    // leave only the index into the stack
    while ((lua_gettop(m_L) - (top + 1)) > 1)
    {
        lua_pop(m_L, 1);
    }

    bool status = (lua_next(m_L, top + 1) != 0);

    if (status)
    {
        it.value.Clear();
        status = m_pSS->PopAny(it.value);

        if (lua_type(m_L, -1) == LUA_TSTRING)
        {
            // string key
            it.sKey = lua_tostring(m_L, -1);
            it.nKey = -1;
        }
        else if (lua_type(m_L, -1) == LUA_TNUMBER)
        {
            // number key
            it.sKey = nullptr;
            it.nKey = static_cast<int>(lua_tonumber(m_L, -1));
        }
        else
        {
            it.sKey = nullptr; // Corrected: Use nullptr for C++ string pointer
            it.nKey = -1;
        }
    }

    if (!status)
    {
        EndIteration(it);
    }

    return status;
}

void ScriptTable::EndIteration(const Iterator & it)
{
    if (it.nInternal)
    {
        lua_settop(m_L, it.nInternal - 1);

        // make the iterator invalid
        const_cast<Iterator&>(it).nInternal = 0;
    }
}

void ScriptTable::Clear()
{
    PushRef();

    const int tableIndex = lua_gettop(m_L);

    // first key
    lua_pushnil(m_L);

    while (lua_next(m_L, tableIndex))
    {
        lua_pop(m_L, 1);         // pop value, leave index
        lua_pushvalue(m_L, -1);  // push again index
        lua_pushnil(m_L);
        lua_rawset(m_L, tableIndex);
    }

    lua_pop(m_L, 1);
}

int ScriptTable::Count()
{
    PushRef();

    int count = (int)luaL_len(m_L, -1);

    lua_pop(m_L, 1);

    return count;
}

bool ScriptTable::Clone(IScriptTable *pSourceTable, bool isDeepCopy)
{
    const int top = lua_gettop(m_L);

    static_cast<ScriptTable*>(pSourceTable)->PushRef();
    PushRef();

    const int srcTable = top + 1;
    const int dstTable = top + 2;

    if (isDeepCopy)
    {
        CloneTableRecursive(srcTable, dstTable);
    }
    else
    {
        CloneTable(srcTable, dstTable);
    }

    // restore stack
    lua_settop(m_L, top);

    return true;
}

void ScriptTable::Dump(IScriptTableDumpSink *pSink)
{
    if (!pSink)
        return;

    const int top = lua_gettop(m_L);

    PushRef();

    DumpTable(pSink);

    if (lua_getmetatable(m_L, -1))
    {
        lua_pushstring(m_L, "__index");
        lua_rawget(m_L, -2);

        if (lua_type(m_L, -1) == LUA_TTABLE)
        {
            DumpTable(pSink);
        }
    }

    lua_settop(m_L, top + 1);

    // pop table ref
    lua_pop(m_L, 1);
}

bool ScriptTable::AddFunction(const SUserFunctionDesc & fd)
{
    PushRef();
    lua_pushstring(m_L, fd.sFunctionName);

    if (fd.pFunctor)
    {
        FunctionData *pData = static_cast<FunctionData*>(lua_newuserdata(m_L, sizeof (FunctionData)));

        pData->pSS = m_pSS;
        pData->SetSignature(fd);
        pData->paramIdOffset = fd.nParamIdOffset;
        pData->functor = fd.pFunctor;

        lua_pushcclosure(m_L, StdCFunction, 1);
    }
    else
    {
        // user data buffer at the end of function data
        const std::size_t totalDataSize = sizeof (FunctionData) + fd.nDataSize;

        FunctionData *pData = static_cast<FunctionData*>(lua_newuserdata(m_L, totalDataSize));

        pData->pSS = m_pSS;
        pData->SetSignature(fd);
        pData->paramIdOffset = fd.nParamIdOffset;
        pData->userDataFunc = fd.pUserDataFunc;
        pData->userDataSize = fd.nDataSize;
        std::memcpy(pData->userDataBuffer, fd.pDataBuffer, fd.nDataSize);

        lua_pushcclosure(m_L, StdCUserDataFunction, 1);
    }

    lua_rawset(m_L, -3);
    lua_pop(m_L, 1);  // pop table

    return true;
}

bool ScriptTable::GetValueRecursive(const char *path, IScriptTable *pTable)
{
    CryLogErrorAlways("[ScriptTable::GetValueRecursive] Should not be used");
    return false;
}

///////////////////////
// Private functions //
///////////////////////

void ScriptTable::CloneTable(int srcTableIndex, int dstTableIndex)
{
    const int top = lua_gettop(m_L);

    // first key
    lua_pushnil(m_L);

    while (lua_next(m_L, srcTableIndex))
    {
        // 'key' is at index -2 and 'value' at index -1
        lua_pushvalue(m_L, -2);  // push again index
        lua_pushvalue(m_L, -2);  // push value

        lua_rawset(m_L, dstTableIndex);

        // pop value, leave index
        lua_pop(m_L, 1);
    }

    // restore stack
    lua_settop(m_L, top);
}

void ScriptTable::CloneTableRecursive(int srcTableIndex, int dstTableIndex)
{
    const int top = lua_gettop(m_L);

    // first key
    lua_pushnil(m_L);

    while (lua_next(m_L, srcTableIndex))
    {
        if (lua_type(m_L, -1) == LUA_TTABLE)
        {
            const int innerSrcTableIndex = lua_gettop(m_L);

            lua_pushvalue(m_L, -2);  // push again index
            lua_newtable(m_L);       // make value

            const int innerDstTableIndex = lua_gettop(m_L);

            CloneTableRecursive(innerSrcTableIndex, innerDstTableIndex);

            lua_rawset(m_L, dstTableIndex);  // set new table to dstTableIndex
        }
        else
        {
            // 'key' is at index -2 and 'value' at index -1
            lua_pushvalue(m_L, -2);  // push again index
            lua_pushvalue(m_L, -2);  // push value

            lua_rawset(m_L, dstTableIndex);
        }

        // pop value, leave index
        lua_pop(m_L, 1);
    }

    // restore stack
    lua_settop(m_L, top);
}

void ScriptTable::DumpTable(IScriptTableDumpSink *pSink)
{
    // first key
    lua_pushnil(m_L);

    while (lua_next(m_L, -2))
    {
        const ScriptVarType valueType = ScriptUtil::LuaTypeToScriptVarType(lua_type(m_L, -1));

        if (lua_type(m_L, -2) == LUA_TSTRING)  // key type
        {
            const char *key = lua_tostring(m_L, -2);

            if (!(valueType == svtObject && std::strcmp(key, "__index") == 0))
            {
                pSink->OnElementFound(key, valueType);
            }
        }
        else
        {
            const int index = static_cast<int>(lua_tonumber(m_L, -2));

            pSink->OnElementFound(index, valueType);
        }

        // pop value, keep key
        lua_pop(m_L, 1);
    }
}

ScriptTable *ScriptTable::AllocateTable(ScriptSystem *pSS)
{
    // allocate memory
    // always succeeds
    void *buffer = pSS->Allocate(sizeof (ScriptTable));

    // call constructor
    return new (buffer) ScriptTable();
}

void ScriptTable::DeallocateTable(ScriptTable *pTable, ScriptSystem *pSS)
{
    // call destructor
    pTable->~ScriptTable();

    // deallocate memory
    pSS->Deallocate(pTable);
}

int ScriptTable::StdCFunction(lua_State *L)
{
    FunctionData *pData = static_cast<FunctionData*>(lua_touserdata(L, lua_upvalueindex(1)));

    FunctionHandler handler(L, pData->pSS, pData->signature, pData->paramIdOffset);

    return pData->functor(&handler);
}

int ScriptTable::StdCUserDataFunction(lua_State *L)
{
    FunctionData *pData = static_cast<FunctionData*>(lua_touserdata(L, lua_upvalueindex(1)));

    FunctionHandler handler(L, pData->pSS, pData->signature, pData->paramIdOffset);

    return pData->userDataFunc(&handler, pData->userDataBuffer, pData->userDataSize);
}
