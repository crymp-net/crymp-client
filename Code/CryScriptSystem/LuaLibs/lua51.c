#include <stdint.h>

#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>

static int lua51_math_random(lua_State* L) {
    lua_Number r = (lua_Number)(rand() % RAND_MAX) / (lua_Number)RAND_MAX;
    switch (lua_gettop(L)) {
    case 0: {
        lua_pushnumber(L, r);
        break;
    }
    case 1: {
        int u = (int)luaL_checknumber(L, 1);
        luaL_argcheck(L, 1 <= u, 1, "interval is empty");
        lua_pushnumber(L, floor(r * u) + 1);
        break;
    }
    case 2: {
        int l = (int)luaL_checknumber(L, 1);
        int u = (int)luaL_checknumber(L, 2);
        luaL_argcheck(L, l <= u, 2, "interval is empty");
        lua_pushnumber(L, floor(r * (u - l + 1)) + l);
        break;
    }
    default:
        return luaL_error(L, "wrong number of arguments");
    }
    return 1;
}

static const struct luaL_Reg lua51[] = {
    { "random", lua51_math_random },
    { NULL, NULL }
};

int lua51_init(lua_State* L)
{
    lua_newtable(L);
    luaL_setfuncs(L, lua51, 0);
    lua_setglobal(L, "lua51");

    return 1;
}
