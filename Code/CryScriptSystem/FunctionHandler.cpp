#include <algorithm>

extern "C"
{
#include <lua.h>
}

#include "CryCommon/CrySystem/ISystem.h"

#include "FunctionHandler.h"
#include "ScriptSystem.h"
#include "ScriptUtil.h"

//////////////////////
// IFunctionHandler //
//////////////////////

IScriptSystem *FunctionHandler::GetIScriptSystem()
{
	return m_pSS;
}

void *FunctionHandler::GetThis()
{
	m_pSS->CheckThread(__FUNCTION__);
	void *self = nullptr;

	// get implicit self table
	if (m_paramIdOffset > 0 && lua_type(m_L, 1) == LUA_TTABLE)
	{
		// index "__this" member
		lua_pushstring(m_L, "__this");
		lua_rawget(m_L, 1);

		if (lua_type(m_L, -1) == LUA_TLIGHTUSERDATA)
		{
			self = const_cast<void*>(lua_topointer(m_L, -1));
		}

		// pop result
		lua_pop(m_L, 1);
	}

	return self;
}

bool FunctionHandler::GetSelfAny(ScriptAnyValue & any)
{
	m_pSS->CheckThread(__FUNCTION__);
	if (m_paramIdOffset > 0)
	{
		return m_pSS->ToAny(any, 1);
	}
	else
	{
		return false;
	}
}

const char *FunctionHandler::GetFuncName()
{
	m_pSS->CheckThread(__FUNCTION__);
	return m_funcName;
}

int FunctionHandler::GetParamCount()
{
	m_pSS->CheckThread(__FUNCTION__);
	return std::max(lua_gettop(m_L) - m_paramIdOffset, 0);
}

ScriptVarType FunctionHandler::GetParamType(int index)
{
	m_pSS->CheckThread(__FUNCTION__);
	const int realIndex = index + m_paramIdOffset;
	const int type = lua_type(m_L, realIndex);

	return ScriptUtil::LuaTypeToScriptVarType(type);
}

bool FunctionHandler::GetParamAny(int index, ScriptAnyValue & any)
{
	m_pSS->CheckThread(__FUNCTION__);
	const int realIndex = index + m_paramIdOffset;

	if (m_pSS->ToAny(any, realIndex))
	{
		return true;
	}
	else
	{
		m_pSS->RaiseError("Function %s expects parameter %d to be type %s and not %s",
			m_funcName,
			index,
			ScriptUtil::ScriptAnyTypeToString(any.type),
			ScriptUtil::ScriptVarTypeToString(GetParamType(index))
		);

		return false;
	}
}

int FunctionHandler::EndFunctionAny(const ScriptAnyValue & any)
{
	m_pSS->CheckThread(__FUNCTION__);
	m_pSS->PushAny(any);

	if (any.type == ANY_TNIL || any.type == ANY_ANY)
		return 0;
	else
		return 1;
}

int FunctionHandler::EndFunctionAny(const ScriptAnyValue & any1, const ScriptAnyValue & any2)
{
	m_pSS->CheckThread(__FUNCTION__);
	m_pSS->PushAny(any1);
	m_pSS->PushAny(any2);

	return 2;
}

int FunctionHandler::EndFunctionAny(const ScriptAnyValue & any1, const ScriptAnyValue & any2, const ScriptAnyValue & any3)
{
	m_pSS->CheckThread(__FUNCTION__);
	m_pSS->PushAny(any1);
	m_pSS->PushAny(any2);
	m_pSS->PushAny(any3);

	return 3;
}

int FunctionHandler::EndFunction()
{
	m_pSS->CheckThread(__FUNCTION__);
	return 0;
}
