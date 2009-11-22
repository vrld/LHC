#include "generators.h"
#include <lauxlib.h>
#include <math.h>
#include <stdlib.h>

static const double PI = 3.1415926535897932384626433832795028841968;

static int generator_sin(lua_State *L)
{
    double t = luaL_checknumber(L, -1);
    lua_pushnumber(L, sin(t * 2 * PI));
    return 1;
}

static int generator_triangle(lua_State *L)
{
    double t = luaL_checknumber(L, -1);
    if (t <= .5) 
        lua_pushnumber(L, 4.*t - 1.);
    else 
        lua_pushnumber(L, 3. - 4*t);
    return 1;
}

static int generator_saw(lua_State *L)
{
    double t = luaL_checknumber(L, -1);
    lua_pushnumber(L, 2.*t - 1.);
    return 1;
}

static int generator_rect(lua_State *L)
{
    double t = luaL_checknumber(L, -1);
    if (t <= .5) 
        lua_pushnumber(L, 1.);
    else 
        lua_pushnumber(L, -1.);
    return 1;
}

static int generator_whiteNoise(lua_State *L)
{
    double r = (rand() % (1 << 15)) / (double)(1 << 14) - 1.;
    lua_pushnumber(L, r);
    return 1;
}

static int generator_brownNoise_closure(lua_State *L)
{
    double last = lua_tonumber(L, lua_upvalueindex(1));
    double r = (rand() % (1 << 15)) / (double)(1 << 14) - 1. + last;
    if (r < -1.) r = -1.;
    if (r > 1.)  r =  1.;
    lua_pushnumber(L, r);
    lua_pushvalue(L, -1);
    lua_replace(L, lua_upvalueindex(1));
    return 1;
}

static const struct luaL_Reg generators[] = {
    {"sin", generator_sin},
    {"triangle", generator_triangle},
    {"saw", generator_saw},
    {"rect", generator_rect},
    {"whiteNoise", generator_whiteNoise},
    {NULL, NULL}
};

int luaopen_generators(lua_State *L)
{
    luaL_register(L, "gen", generators);
    lua_pushnumber(L, 0);
    lua_pushcclosure(L, &generator_brownNoise_closure, 1);
    lua_setfield(L, -2, "brownNoise");
    return 1;
}
