#include "signal.h"
#include <lauxlib.h>
#include <math.h>

static int signal_closure(lua_State *L)
{
    double t = luaL_checknumber(L, -1);
    double freq  = lua_tonumber(L, lua_upvalueindex(2));
    double amp   = lua_tonumber(L, lua_upvalueindex(3));
    double phase = lua_tonumber(L, lua_upvalueindex(4));
    
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushnumber(L, fmod(t * freq + phase, 1.));
    lua_call(L, 1, 1);
    if (lua_isnil(L, -1))
        return luaL_error(L, "Generator returned 'nil' when number was expected.");

    t = lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_pushnumber(L, t * amp);
    return 1;
}

#define GET_NUMBER_OR_DEFAULT(index, name) do { lua_getfield(L, (index), name); \
    if (!lua_isnumber(L, -1)) { \
        lua_pop(L, 1); \
        lua_getglobal(L, "defaults"); \
        lua_getfield(L, -1, name); \
        lua_remove(L, -2); \
    } } while(0)

static int l_signal(lua_State *L)
{
    if (!lua_istable(L, -1))
        return luaL_error(L, "error: expected table as argument.");

    lua_getfield(L, -1, "g");
    if (!lua_isfunction(L, -1)) { 
        lua_pop(L, 1);
        lua_pushnumber(L, 1);
        lua_gettable(L, -2);
        if (!lua_isfunction(L, -1))
            return luaL_error(L, "error: expected generator to be a function.");
    }

    GET_NUMBER_OR_DEFAULT(-2, "freq");
    GET_NUMBER_OR_DEFAULT(-3, "amp");
    GET_NUMBER_OR_DEFAULT(-4, "phase");

    lua_pushcclosure(L, &signal_closure, 4); 
    lua_remove(L, -2);
    return 1;
}

int luaopen_signal(lua_State *L)
{
    lua_register(L, "signal", l_signal);
    return 0;
}
