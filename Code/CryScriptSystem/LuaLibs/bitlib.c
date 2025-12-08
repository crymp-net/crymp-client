#include <stdint.h>

#include <lua.h>
#include <lauxlib.h>

static int64_t get_number(lua_State *L, int index)
{
	return (int64_t) lua_tonumber(L, index);
}

static int int_not(lua_State *L)
{
	int64_t result = ~get_number(L, 1);

	lua_pushnumber(L, (lua_Number) result);

	return 1;
}

static int int_mod(lua_State *L)
{
	int64_t result = get_number(L, 1) % get_number(L, 2);

	lua_pushnumber(L, (lua_Number) result);

	return 1;
}

static int int_and(lua_State *L)
{
	int count = lua_gettop(L);
	int64_t result = get_number(L, 1);

	for (int i = 2; i <= count; i++)
	{
		result &= get_number(L, i);
	}

	lua_pushnumber(L, (lua_Number) result);

	return 1;
}

static int int_or(lua_State *L)
{
	int count = lua_gettop(L);
	int64_t result = get_number(L, 1);

	for (int i = 2; i <= count; i++)
	{
		result |= get_number(L, i);
	}

	lua_pushnumber(L, (lua_Number) result);

	return 1;
}

static int int_xor(lua_State *L)
{
	int count = lua_gettop(L);
	int64_t result = get_number(L, 1);

	for (int i = 2; i <= count; i++)
	{
		result ^= get_number(L, i);
	}

	lua_pushnumber(L, (lua_Number) result);

	return 1;
}

static int int_lshift(lua_State *L)
{
	int64_t result = get_number(L, 1) << (uint64_t) get_number(L, 2);

	lua_pushnumber(L, (lua_Number) result);

	return 1;
}

static int int_rshift(lua_State *L)
{
	int64_t result = (uint64_t) get_number(L, 1) >> (uint64_t) get_number(L, 2);

	lua_pushnumber(L, (lua_Number) result);

	return 1;
}

static int int_arshift(lua_State *L)
{
	int64_t result = get_number(L, 1) >> (uint64_t) get_number(L, 2);

	lua_pushnumber(L, (lua_Number) result);

	return 1;
}

static const struct luaL_Reg bitlib[] = {
	{ "bnot",    int_not     },
	{ "imod",    int_mod     },  // "mod" already in Lua math library
	{ "band",    int_and     },
	{ "bor",     int_or      },
	{ "bxor",    int_xor     },
	{ "lshift",  int_lshift  },
	{ "rshift",  int_rshift  },
	{ "arshift", int_arshift },
	{ NULL, NULL }
};

int bitlib_init(lua_State* L)
{
	lua_pushglobaltable(L);
	luaL_setfuncs(L, bitlib, 0);
	return 1;
}
