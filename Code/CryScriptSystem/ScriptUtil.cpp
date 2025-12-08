extern "C"
{
#include <lua.h>
}

#include "Library/StringTools.h"

#include "ScriptUtil.h"

std::vector<std::string> ScriptUtil::GetCallStack(lua_State *L, int level)
{
	std::vector<std::string> result;

	lua_Debug ar = {};

	while (lua_getstack(L, level++, &ar) && lua_getinfo(L, "Sln", &ar))
	{
		result.emplace_back(StringTools::Format("%s:%d %s %s %s",
			ar.source,
			ar.currentline,
			ar.what,
			ar.namewhat,
			ar.name
		));
	}

	return result;
}

ScriptVarType ScriptUtil::LuaTypeToScriptVarType(int type)
{
	switch (type)
	{
		case LUA_TNIL:           return svtNull;
		case LUA_TBOOLEAN:       return svtBool;
		case LUA_TLIGHTUSERDATA: return svtPointer;
		case LUA_TNUMBER:        return svtNumber;
		case LUA_TSTRING:        return svtString;
		case LUA_TTABLE:         return svtObject;
		case LUA_TFUNCTION:      return svtFunction;
		case LUA_TUSERDATA:      return svtUserData;
	}

	return svtNull;
}

const char *ScriptUtil::ScriptAnyTypeToString(ScriptAnyType type)
{
	switch (type)
	{
		case ANY_ANY:       return "Any";
		case ANY_TNIL:      return "Null";
		case ANY_TBOOLEAN:  return "Boolean";
		case ANY_TSTRING:   return "String";
		case ANY_TNUMBER:   return "Number";
		case ANY_TFUNCTION: return "Function";
		case ANY_TTABLE:    return "Table";
		case ANY_TUSERDATA: return "UserData";
		case ANY_THANDLE:   return "Pointer";
		case ANY_TVECTOR:   return "Vec3";
	}

	return "Unknown";
}

const char *ScriptUtil::ScriptVarTypeToString(ScriptVarType type)
{
	switch (type)
	{
		case svtNull:     return "Null";
		case svtBool:     return "Boolean";
		case svtString:   return "String";
		case svtNumber:   return "Number";
		case svtFunction: return "Function";
		case svtObject:   return "Table";
		case svtUserData: return "UserData";
		case svtPointer:  return "Pointer";
	}

	return "Unknown";
}
