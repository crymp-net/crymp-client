#include <stdio.h>
#include <string.h>
#include <algorithm>

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <tracy/Tracy.hpp>

#include "CryCommon/CryCore/CryMalloc.h"
#include "CryCommon/CrySystem/CryFile.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/ITimer.h"
#include "CryCommon/CryAISystem/IAISystem.h"

#include "ScriptSystem.h"
#include "ScriptTable.h"
#include "ScriptUtil.h"

static lua_State *g_L = nullptr;

static HSCRIPTFUNCTION RefToFunctionHandle(int ref)
{
	return reinterpret_cast<HSCRIPTFUNCTION>(static_cast<std::uintptr_t>(ref));
}

static int FunctionHandleToRef(HSCRIPTFUNCTION handle)
{
	return static_cast<int>(reinterpret_cast<std::uintptr_t>(handle));
}

extern "C"
{
	int bitlib_init(lua_State *L);
}

ScriptSystem::ScriptSystem()
{
	m_scripts.reserve(128);
}

ScriptSystem::~ScriptSystem()
{
	LuaClose();
}

void ScriptSystem::Init()
{
	LuaInit();

	m_timers.Init(this);
	m_bindings.Init(this);

	SetGlobalValue("_time", 0);
	SetGlobalValue("_frametime", 0);
	SetGlobalValue("_aitick", 0);

	IConsole *pConsole = gEnv->pConsole;

	pConsole->AddCommand("lua_dump_state", OnDumpStateCmd, 0, "Dumps the current state into a file.");
	pConsole->AddCommand("lua_dump_scripts", OnDumpScriptsCmd, 0, "Dumps loaded scripts to the log.");
	pConsole->AddCommand("lua_garbage_collect", OnGarbageCollectCmd, 0, "Forces garbage collection.");

	ExecuteFile("Scripts/common.lua");
}

void *ScriptSystem::Allocate(size_t size)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SCRIPT);

	// TODO: optimized memory allocator
	void *block = CryMalloc(size);
	TracyAllocN(block, size, "ScriptSystem");

	// we never fail
	if (!block)
	{
		// quit
		gEnv->pSystem->Error("[ScriptSystem] Out of memory");
	}

	return block;
}

void ScriptSystem::Deallocate(void *block)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SCRIPT);

	TracyFreeN(block, "ScriptSystem");
	CryFree(block);
}

void ScriptSystem::PushAny(const ScriptAnyValue & any)
{
	switch (any.type)
	{
		case ANY_ANY:
		case ANY_TNIL:
		{
			lua_pushnil(m_L);
			break;
		}
		case ANY_TBOOLEAN:
		{
			lua_pushboolean(m_L, any.b);
			break;
		}
		case ANY_THANDLE:
		{
			lua_pushlightuserdata(m_L, const_cast<void*>(any.ptr));
			break;
		}
		case ANY_TNUMBER:
		{
			lua_pushnumber(m_L, any.number);
			break;
		}
		case ANY_TSTRING:
		{
			lua_pushstring(m_L, any.str);
			break;
		}
		case ANY_TTABLE:
		{
			if (any.table)
				static_cast<ScriptTable*>(any.table)->PushRef();
			else
				lua_pushnil(m_L);
			break;
		}
		case ANY_TFUNCTION:
		{
			if (any.function)
				lua_getref(m_L, FunctionHandleToRef(any.function));
			else
				lua_pushnil(m_L);
			break;
		}
		case ANY_TUSERDATA:
		{
			if (any.ud.ptr)
				lua_getref(m_L, static_cast<int>(reinterpret_cast<std::uintptr_t>(any.ud.ptr)));
			else
				lua_pushnil(m_L);
			break;
		}
		case ANY_TVECTOR:
		{
			PushVec3(Vec3(any.vec3.x, any.vec3.y, any.vec3.z));
			break;
		}
	}
}

void ScriptSystem::PushVec3(const Vec3 & vec)
{
	lua_newtable(m_L);

	lua_pushlstring(m_L, "x", 1);
	lua_pushnumber(m_L, vec.x);
	lua_settable(m_L, -3);

	lua_pushlstring(m_L, "y", 1);
	lua_pushnumber(m_L, vec.y);
	lua_settable(m_L, -3);

	lua_pushlstring(m_L, "z", 1);
	lua_pushnumber(m_L, vec.z);
	lua_settable(m_L, -3);
}

bool ScriptSystem::PopAny(ScriptAnyValue & any)
{
	const bool status = ToAny(any, -1);

	lua_pop(m_L, 1);

	return status;
}

bool ScriptSystem::PopAnys(ScriptAnyValue *anys, int count)
{
	for (int i = 0; i < count; i++)
	{
		if (!PopAny(anys[i]))
		{
			return false;
		}
	}

	return true;
}

bool ScriptSystem::ToAny(ScriptAnyValue & any, int index)
{
	if (!lua_gettop(m_L))
	{
		return false;
	}

	switch (lua_type(m_L, index))
	{
		case LUA_TNIL:
		{
			if (any.type == ANY_ANY || any.type == ANY_TNIL)
			{
				any.type = ANY_TNIL;
				return true;
			}
			else
			{
				return false;
			}
		}
		case LUA_TBOOLEAN:
		{
			if (any.type == ANY_ANY || any.type == ANY_TBOOLEAN)
			{
				any.b = (lua_toboolean(m_L, index) != 0);
				any.type = ANY_TBOOLEAN;
				return true;
			}
			else
			{
				return false;
			}
		}
		case LUA_TLIGHTUSERDATA:
		{
			if (any.type == ANY_ANY || any.type == ANY_THANDLE)
			{
				any.ptr = lua_topointer(m_L, index);
				any.type = ANY_THANDLE;
				return true;
			}
			else
			{
				return false;
			}
		}
		case LUA_TNUMBER:
		{
			if (any.type == ANY_ANY || any.type == ANY_TNUMBER)
			{
				any.number = lua_tonumber(m_L, index);
				any.type = ANY_TNUMBER;
				return true;
			}
			else if (any.type == ANY_TBOOLEAN)
			{
				any.b = (lua_tonumber(m_L, index) != 0);
				return true;
			}
			else
			{
				return false;
			}
		}
		case LUA_TSTRING:
		{
			if (any.type == ANY_ANY || any.type == ANY_TSTRING)
			{
				any.str = lua_tostring(m_L, index);
				any.type = ANY_TSTRING;
				return true;
			}
			else
			{
				return false;
			}
		}
		case LUA_TTABLE:
		case LUA_TUSERDATA:
		{
			if (any.type == ANY_ANY || any.type == ANY_TTABLE)
			{
				if (!any.table)
				{
					any.table = ScriptTable::Create(this, m_L, true);
					any.table->AddRef();
				}

				lua_pushvalue(m_L, index);
				static_cast<ScriptTable*>(any.table)->Attach();

				any.type = ANY_TTABLE;
				return true;
			}
			else if (any.type == ANY_TVECTOR)
			{
				Vec3 v(0, 0, 0);
				if (ToVec3(v, index))
				{
					any.vec3.x = v.x;
					any.vec3.y = v.y;
					any.vec3.z = v.z;
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		case LUA_TFUNCTION:
		{
			if (any.type == ANY_ANY || any.type == ANY_TFUNCTION)
			{
				lua_pushvalue(m_L, index);
				any.function = RefToFunctionHandle(lua_ref(m_L, 1));
				any.type = ANY_TFUNCTION;
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	return false;
}

bool ScriptSystem::ToVec3(Vec3 & vec, int index)
{
	if (index < 0)
		index = lua_gettop(m_L) + index + 1;

	if (lua_type(m_L, index) != LUA_TTABLE)
		return false;

	float x = 0;
	float y = 0;
	float z = 0;

	lua_pushlstring(m_L, "x", 1);
	lua_gettable(m_L, index);
	if (lua_isnumber(m_L, -1))
	{
		x = lua_tonumber(m_L, -1);
		lua_pop(m_L, 1);

		lua_pushlstring(m_L, "y", 1);
		lua_gettable(m_L, index);
		if (lua_isnumber(m_L, -1))
		{
			y = lua_tonumber(m_L, -1);
			lua_pop(m_L, 1);
		}
		else
		{
			lua_pop(m_L, 1);
			return false;
		}

		lua_pushlstring(m_L, "z", 1);
		lua_gettable(m_L, index);
		if (lua_isnumber(m_L, -1))
		{
			z = lua_tonumber(m_L, -1);
			lua_pop(m_L, 1);
		}
		else
		{
			lua_pop(m_L, 1);
			return false;
		}
	}
	else
	{
		lua_pop(m_L, 1);

		// try indexed table instead

		lua_pushnumber(m_L, 1);
		lua_gettable(m_L, index);
		if (lua_isnumber(m_L, -1))
		{
			x = lua_tonumber(m_L, -1);
			lua_pop(m_L, 1);
		}
		else
		{
			lua_pop(m_L, 1);
			return false;
		}

		lua_pushnumber(m_L, 2);
		lua_gettable(m_L, index);
		if (lua_isnumber(m_L, -1))
		{
			y = lua_tonumber(m_L, -1);
			lua_pop(m_L, 1);
		}
		else
		{
			lua_pop(m_L, 1);
			return false;
		}

		lua_pushnumber(m_L, 3);
		lua_gettable(m_L, index);
		if (lua_isnumber(m_L, -1))
		{
			z = lua_tonumber(m_L, -1);
			lua_pop(m_L, 1);
		}
		else
		{
			lua_pop(m_L, 1);
			return false;
		}
	}

	vec.x = x;
	vec.y = y;
	vec.z = z;

	return true;
}

///////////////////
// IScriptSystem //
///////////////////

void ScriptSystem::Update()
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SCRIPT);

	ITimer *pTimer = gEnv->pTimer;
	IAISystem *pAISystem = gEnv->pAISystem;

	const float currentTime = pTimer->GetCurrTime();
	const float frameTime = pTimer->GetFrameTime();
	const int aiTickCount = (pAISystem) ? pAISystem->GetAITickCount() : 0;

	SetGlobalValue("_time", currentTime);
	SetGlobalValue("_frametime", frameTime);
	SetGlobalValue("_aitick", aiTickCount);

	LuaGarbageCollectStep();

	m_timers.Update();
}

void ScriptSystem::SetGCFrequency(const float rate)
{
}

bool ScriptSystem::ExecuteFile(const char *fileName, bool raiseError, bool forceReload)
{
	std::string fileNameCryMP("CryMP/");
	fileNameCryMP += fileName;

	// files in CryMP/Scripts/ override the original ones in Scripts/
	if (!fileNameCryMP.starts_with("CryMP/CryMP/") && gEnv->pCryPak->IsFileExist(fileNameCryMP.c_str()))
	{
		CryLogComment("%s(\"%s\"): Overriding with \"%s\"", __FUNCTION__, fileName, fileNameCryMP.c_str());
		fileName = fileNameCryMP.c_str();
	}

	const bool isNew = this->AddToScripts(fileName);

	if (forceReload)
	{
		m_nestedForceReload++;
	}

	bool success = true;

	if (isNew || forceReload || m_nestedForceReload > 0)
	{
		// try to load and execute the script file
		if (CryFile file(fileName, "rb"); file)
		{
			std::vector<char> content;

			// read script file content
			content.resize(file.GetSize());
			content.resize(file.ReadRaw(content.data(), content.size()));

			// use script file path as its description
			const char *description = fileName;

			// execute the file
			success = ExecuteBuffer(content.data(), content.size(), description);
		}
		else
		{
			success = false;
		}

		if (!success)
		{
			// always log failed script execution
			//if (raiseError)
			{
				CryLogErrorAlways("[Script] Failed to load %s", fileName);
			}

			this->RemoveFromScripts(fileName);
		}
	}

	if (forceReload)
	{
		m_nestedForceReload--;
	}

	return success;
}

bool ScriptSystem::ExecuteBuffer(const char *buffer, size_t bufferSize, const char *bufferDescription)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SCRIPT);

	// compile
	if (luaL_loadbuffer(m_L, buffer, bufferSize, bufferDescription) != 0)
	{
		const char *message = lua_tostring(m_L, -1);

		CryLogErrorAlways("[Script] Parse error (%s): %s", bufferDescription, message);

		return false;
	}

	// execute
	return LuaCall(0, LUA_MULTRET);
}

void ScriptSystem::UnloadScript(const char *fileName)
{
	this->RemoveFromScripts(fileName);
}

void ScriptSystem::UnloadScripts()
{
	m_scripts.clear();
}

bool ScriptSystem::ReloadScript(const char *fileName, bool raiseError)
{
	const bool forceReload = false;

	return ExecuteFile(fileName, raiseError, forceReload);
}

bool ScriptSystem::ReloadScripts()
{
	bool status = true;

	for (const Script& script : m_scripts)
	{
		if (!ReloadScript(script.prettyName.c_str()))
		{
			status = false;
		}
	}

	return status;
}

void ScriptSystem::DumpLoadedScripts()
{
	CryLogAlways("=== LOADED SCRIPTS BEGIN ===");

	for (const Script& script : m_scripts)
	{
		CryLogAlways("%s", script.prettyName.c_str());
	}

	CryLogAlways("=== LOADED SCRIPTS END ===");
}

IScriptTable *ScriptSystem::CreateTable(bool empty)
{
	return ScriptTable::Create(this, m_L, empty);
}

int ScriptSystem::BeginCall(HSCRIPTFUNCTION func)
{
	m_funcParamCount = -1;

	if (!func)
		return 0;

	const int funcRef = FunctionHandleToRef(func);

	lua_getref(m_L, funcRef);
	if (!lua_isfunction(m_L, -1))
	{
		CryLogWarning("ScriptSystem::BeginCall(%d): Not a function", funcRef);
		lua_pop(m_L, 1);
		return 0;
	}

	m_funcParamCount = 0;

	return 1;
}

int ScriptSystem::BeginCall(const char *funcName)
{
	m_funcParamCount = -1;

	lua_getglobal(m_L, funcName);
	if (!lua_isfunction(m_L, -1))
	{
		CryLogWarning("ScriptSystem::BeginCall(%s): Not a function", funcName);
		lua_pop(m_L, 1);
		return 0;
	}

	m_funcParamCount = 0;

	return 1;
}

int ScriptSystem::BeginCall(const char *tableName, const char *funcName)
{
	m_funcParamCount = -1;

	lua_getglobal(m_L, tableName);
	if (!lua_istable(m_L, -1))
	{
		CryLogWarning("ScriptSystem::BeginCall(%s.%s): Not a table", tableName, funcName);
		lua_pop(m_L, 1);
		return 0;
	}

	lua_pushstring(m_L, funcName);
	lua_gettable(m_L, -2);
	lua_remove(m_L, -2);  // remove global table
	if (!lua_isfunction(m_L, -1))
	{
		CryLogWarning("ScriptSystem::BeginCall(%s.%s): Not a function", tableName, funcName);
		lua_pop(m_L, 1);
		return 0;
	}

	m_funcParamCount = 0;

	return 1;
}

int ScriptSystem::BeginCall(IScriptTable *pTable, const char *funcName)
{
	m_funcParamCount = -1;

	static_cast<ScriptTable*>(pTable)->PushRef();

	lua_pushstring(m_L, funcName);
	lua_gettable(m_L, -2);
	lua_remove(m_L, -2);  // remove global table
	if (!lua_isfunction(m_L, -1))
	{
		CryLogWarning("ScriptSystem::BeginCall(0x%p, %s): Not a function", pTable, funcName);
		lua_pop(m_L, 1);
		return 0;
	}

	m_funcParamCount = 0;

	return 1;
}

bool ScriptSystem::EndCall()
{
	return (m_funcParamCount >= 0) ? LuaCall(m_funcParamCount, 0) : false;
}

bool ScriptSystem::EndCallAny(ScriptAnyValue & any)
{
	return (m_funcParamCount >= 0) ? LuaCall(m_funcParamCount, 1) && PopAny(any) : false;
}

bool ScriptSystem::EndCallAnyN(int resultCount, ScriptAnyValue *anys)
{
	return (m_funcParamCount >= 0) ? LuaCall(m_funcParamCount, resultCount) && PopAnys(anys, resultCount) : false;
}

HSCRIPTFUNCTION ScriptSystem::GetFunctionPtr(const char *funcName)
{
	lua_getglobal(m_L, funcName);
	if (!lua_isfunction(m_L, -1))
	{
		CryLogWarning("ScriptSystem::GetFunctionPtr(%s): Not a function", funcName);
		lua_pop(m_L, 1);
		return nullptr;
	}

	HSCRIPTFUNCTION handle = RefToFunctionHandle(lua_ref(m_L, 1));
	lua_pop(m_L, 1);

	return handle;
}

HSCRIPTFUNCTION ScriptSystem::GetFunctionPtr(const char *tableName, const char *funcName)
{
	lua_getglobal(m_L, tableName);
	if (!lua_istable(m_L, -1))
	{
		CryLogWarning("ScriptSystem::GetFunctionPtr(%s.%s): Not a table", tableName, funcName);
		lua_pop(m_L, 1);
		return nullptr;
	}

	lua_pushstring(m_L, funcName);
	lua_gettable(m_L, -2);
	if (!lua_isfunction(m_L, -1))
	{
		CryLogWarning("ScriptSystem::GetFunctionPtr(%s.%s): Not a function", tableName, funcName);
		lua_pop(m_L, 2);
		return nullptr;
	}

	HSCRIPTFUNCTION handle = RefToFunctionHandle(lua_ref(m_L, 1));
	lua_pop(m_L, 2);

	return handle;
}

void ScriptSystem::ReleaseFunc(HSCRIPTFUNCTION func)
{
	if (func)
	{
		lua_unref(m_L, FunctionHandleToRef(func));
	}
}

void ScriptSystem::PushFuncParamAny(const ScriptAnyValue & any)
{
	if (m_funcParamCount >= 0)
	{
		PushAny(any);
		m_funcParamCount++;
	}
}

void ScriptSystem::SetGlobalAny(const char *key, const ScriptAnyValue & any)
{
	const size_t key_length = strlen(key);

	char key_buffer[256];
	if (key_length >= sizeof(key_buffer))
	{
		CryLogError("ScriptSystem::SetGlobalAny(%s): Too long key", key);
		return;
	}

	memcpy(key_buffer, key, key_length + 1);
	key_buffer[255] = 0;
	char* current_key = key_buffer;

	bool success = false;
	bool is_nested = false;
	ScriptAnyValue nested_any;
	for (; char* dot = strchr(current_key, '.'); current_key = dot + 1)
	{
		*dot = '\0';

		if (!is_nested)
		{
			is_nested = true;
			lua_getglobal(m_L, current_key);
			success = PopAny(nested_any);
		}
		else
		{
			ScriptAnyValue temp;
			success = nested_any.table->GetValueAny(current_key, temp);
			nested_any.Swap(temp);
		}

		if (!success || nested_any.type != ANY_TTABLE)
		{
			CryLogWarning("ScriptSystem::SetGlobalAny(%s): %s is not a table", key, current_key);
			return;
		}
	}

	if (is_nested)
	{
		nested_any.table->SetValueAny(current_key, any);
	}
	else
	{
		PushAny(any);
		lua_setglobal(m_L, current_key);
	}
}

bool ScriptSystem::GetGlobalAny(const char *key, ScriptAnyValue & any)
{
	const size_t key_length = strlen(key);

	char key_buffer[256];
	if (key_length >= sizeof(key_buffer))
	{
		CryLogError("ScriptSystem::GetGlobalAny(%s): Too long key", key);
		return false;
	}

	memcpy(key_buffer, key, key_length + 1);
	char* current_key = key_buffer;

	bool success = false;
	bool is_nested = false;
	ScriptAnyValue nested_any;
	for (; char* dot = strchr(current_key, '.'); current_key = dot + 1)
	{
		*dot = '\0';

		if (!is_nested)
		{
			is_nested = true;
			lua_getglobal(m_L, current_key);
			success = PopAny(nested_any);
		}
		else
		{
			ScriptAnyValue temp;
			success = nested_any.table->GetValueAny(current_key, temp);
			nested_any.Swap(temp);
		}

		if (!success || nested_any.type != ANY_TTABLE)
		{
			CryLogWarning("ScriptSystem::GetGlobalAny(%s): %s is not a table", key, current_key);
			return false;
		}
	}

	if (is_nested)
	{
		success = nested_any.table->GetValueAny(current_key, any);
	}
	else
	{
		lua_getglobal(m_L, current_key);
		success = PopAny(any);
	}

	if (!success)
	{
		CryLogWarning("ScriptSystem::GetGlobalAny(%s): Failed", key);
	}
	else if (any.type == ANY_TNIL)
	{
		CryLogWarning("ScriptSystem::GetGlobalAny(%s): Nil value", key);
	}

	return success;
}

void ScriptSystem::SetGlobalToNull(const char *key)
{
	SetGlobalAny(key, ScriptAnyValue(ANY_TNIL));
}

IScriptTable *ScriptSystem::CreateUserData(void *data, size_t dataSize)
{
	void *buffer = lua_newuserdata(m_L, dataSize);
	memcpy(buffer, data, dataSize);

	ScriptTable *pTable = ScriptTable::Create(this, m_L, true);
	pTable->Attach();

	return pTable;
}

void ScriptSystem::ForceGarbageCollection()
{
	const int beforeKBytes = GetCGCount();

	LuaGarbageCollectFull();

	const int afterKBytes = GetCGCount();

	CryLog("[Script] Garbage collect: %i KiB => %i KiB", beforeKBytes, afterKBytes);
}

int ScriptSystem::GetCGCount()
{
	return lua_gc(m_L, LUA_GCCOUNT, 0);
}

void ScriptSystem::SetGCThreshhold(int kilobytes)
{
}

void ScriptSystem::GetMemoryStatistics(ICrySizer *pSizer)
{
}

void ScriptSystem::RaiseError(const char *format, ...)
{
	char buffer[1024];

	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof buffer, format, args);
	va_end(args);

	CryLogErrorAlways("[Script] %s", buffer);

	for (const std::string & line : ScriptUtil::GetCallStack(m_L))
	{
		CryLogErrorAlways("    %s", line.c_str());
	}
}

void ScriptSystem::PostInit()
{
}

void ScriptSystem::SerializeTimers(ISerialize *pSer)
{
	m_timers.Serialize(pSer);
}

void ScriptSystem::ResetTimers()
{
	m_timers.Reset();
}

int ScriptSystem::GetStackSize()
{
	return lua_gettop(m_L);
}

uint32_t ScriptSystem::GetScriptAllocSize()
{
	return 0;
}

///////////////////////
// Private functions //
///////////////////////

void ScriptSystem::LuaInit()
{
	LuaClose();

	m_L = lua_newstate(ScriptSystem::LuaAllocator, this);
	g_L = m_L;

	// set handler for errors in unprotected environment
	lua_atpanic(m_L, ScriptSystem::PanicHandler);

	// standard Lua libraries
	luaL_openlibs(m_L);

	// custom Lua libraries
	bitlib_init(m_L);

	// prepare handler for errors in protected environment
	lua_pushcfunction(m_L, ScriptSystem::ErrorHandler);
	m_errorHandlerRef = lua_ref(m_L, 1);
}

void ScriptSystem::LuaClose()
{
	if (m_L)
	{
		lua_close(m_L);
		m_L = nullptr;
		g_L = nullptr;
	}
}

void ScriptSystem::LuaGarbageCollectStep()
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SCRIPT);

	const int stepSize = 2;

	lua_gc(m_L, LUA_GCSTEP, stepSize);
}

void ScriptSystem::LuaGarbageCollectFull()
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SCRIPT);

	lua_gc(m_L, LUA_GCCOLLECT, 0);
}

bool ScriptSystem::LuaCall(int paramCount, int resultCount)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_SCRIPT);

	const int errorHandlerIndex = lua_gettop(m_L) - paramCount;
	lua_getref(m_L, m_errorHandlerRef);
	lua_insert(m_L, errorHandlerIndex);

	const int status = lua_pcall(m_L, paramCount, resultCount, errorHandlerIndex);

	lua_remove(m_L, errorHandlerIndex);

	return (status == 0);
}

static std::string SanitizeScriptFileName(const char *fileName)
{
	std::string result;

	if (fileName)
	{
		result = fileName;

		for (char& ch : result)
		{
			if (ch == '\\')
			{
				// convert backslashes to normal slashes
				ch = '/';
			}

			// convert to lowercase
			ch |= (ch >= 'A' && ch <= 'Z') << 5;
		}
	}

	return result;
}

bool ScriptSystem::AddToScripts(const char *fileName)
{
	const std::string sanitizedName = SanitizeScriptFileName(fileName);

	auto it = std::lower_bound(m_scripts.begin(), m_scripts.end(), sanitizedName);
	if (it != m_scripts.end() && it->sanitizedName == sanitizedName)
	{
		return false;
	}

	it = m_scripts.emplace(it);
	it->sanitizedName = sanitizedName;
	it->prettyName = fileName;

	return true;
}

bool ScriptSystem::RemoveFromScripts(const char *fileName)
{
	const std::string sanitizedName = SanitizeScriptFileName(fileName);

	auto it = std::lower_bound(m_scripts.begin(), m_scripts.end(), sanitizedName);
	if (it == m_scripts.end() || it->sanitizedName != sanitizedName)
	{
		return false;
	}

	m_scripts.erase(it);

	return true;
}

int ScriptSystem::ErrorHandler(lua_State *L)
{
	const char *message = lua_tostring(L, 1);

	CryLogErrorAlways("[Script] %s", message);

	for (const std::string & line : ScriptUtil::GetCallStack(L, 1))
	{
		CryLogErrorAlways("    %s", line.c_str());
	}

	return 0;
}

int ScriptSystem::PanicHandler(lua_State *L)
{
	const char *message = lua_tostring(L, -1);

	CryLogErrorAlways("[Script] Unprotected error: %s", message);

	for (const std::string & line : ScriptUtil::GetCallStack(L, 1))
	{
		CryLogErrorAlways("    %s", line.c_str());
	}

	// quit
	gEnv->pSystem->Error("[Script] Unprotected error: %s", message);

	// Lua always calls exit() after this handler
	return 0;
}

void* ScriptSystem::LuaAllocator(void* userData, void* originalBlock, size_t originalSize, size_t newSize)
{
	ScriptSystem *self = static_cast<ScriptSystem*>(userData);

	if (!newSize)
	{
		self->Deallocate(originalBlock);
		return nullptr;
	}

	if (newSize <= originalSize)
	{
		return originalBlock;
	}

	// always succeeds
	void *newBlock = self->Allocate(newSize);

	memcpy(newBlock, originalBlock, originalSize);

	self->Deallocate(originalBlock);

	return newBlock;
}

void ScriptSystem::OnDumpStateCmd(IConsoleCmdArgs *pArgs)
{
	// TODO
}

void ScriptSystem::OnDumpScriptsCmd(IConsoleCmdArgs *pArgs)
{
	static_cast<ScriptSystem*>(gEnv->pScriptSystem)->DumpLoadedScripts();
}

void ScriptSystem::OnGarbageCollectCmd(IConsoleCmdArgs *pArgs)
{
	gEnv->pScriptSystem->ForceGarbageCollection();
}

void DumpLuaStackTrace(std::FILE* file) {
	if (g_L) {
		lua_Debug ar = {};
		int level = 0;

		std::fprintf(file, "Lua stack:\n");

		while (lua_getstack(g_L, level++, &ar) && lua_getinfo(g_L, "Sln", &ar))
		{
			std::fprintf(file, " %s:%d %s %s %s\n",
				ar.source,
				ar.currentline,
				ar.what,
				ar.namewhat,
				ar.name);
		}
		std::fflush(file);
	} else {
		std::fprintf(file, "Lua was already closed\n");
	}
}