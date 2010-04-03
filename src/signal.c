/*********************************************************************
 *  This file is part of LHC
 *
 *  Copyright (c) 2010 Matthias Richter
 * 
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use,
 *  copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following
 *  conditions:
 * 
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *  OTHER DEALINGS IN THE SOFTWARE.
 */
#include "signal.h"
#include "signal_operators.h"
#include "signal_player.h"

#include <lauxlib.h>
#include <math.h>

/*
 * returns true if userdata at idx is a signal
 */
int signal_userdata_is_signal(lua_State* L, int idx)
{
    int is_signal = 0;
    void *p = lua_touserdata(L, idx);
    if (p == NULL)
        return 0;

    if (!lua_getmetatable(L, idx))
        return 0;

    lua_getfield(L, LUA_REGISTRYINDEX, "lhc.signal");
    is_signal = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    return is_signal;
}

/*
 * returns userdata at index if it is a signal. yields a
 * type error if it is not (never returns!)
 */
Signal* signal_checkudata(lua_State* L, int idx)
{
    Signal* p = lua_touserdata(L, idx);
    if (p != NULL) {
        if (lua_getmetatable(L, idx)) {
            lua_getfield(L, LUA_REGISTRYINDEX, "lhc.signal");
            if (lua_rawequal(L, -1, -2)) {
                lua_pop(L, 2);
                return p;
            }
        }
    }
    luaL_typerror(L, idx, "signal");
    return NULL;
}

/*
 * generates SAMPLE_BUFFER_SIZE next values.
 * arguments are current time and sample rate
 * returns table with values and new time
 */
static int signal_closure(lua_State *L)
{
    double t = luaL_checknumber(L, 1);
    double timestep = 1. / SAMPLERATE;
    double freq;
    size_t i;

    lua_createtable(L, SAMPLE_BUFFER_SIZE, 0);
    for (i = 1; i <= SAMPLE_BUFFER_SIZE; ++i, t += timestep) 
    {
        lua_pushvalue(L, lua_upvalueindex(2));
        if (lua_isfunction(L, -1)) 
        {
            lua_pushnumber(L, t);
            lua_call(L, 1, 1);
        }
        freq = lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_pushvalue(L, lua_upvalueindex(1));
        /* t should be in [0:1] for the generator, as the slope of
         * t * freq gets bigger with growing t:
         * If the frequency is a function(t) return 440 + 220 * (t%1) end
         * (t * freq) will result in t * 440 + 220 * t * (t%1) which is
         * not linear anymore! (plot this if you want to)
         */
        lua_pushnumber(L, fmod(t,1) * freq);
        lua_call(L, 1, 1);
        lua_rawseti(L, -2, i);
    }
    lua_pushnumber(L, t);

    return 2; /* table and new time */
}

static int signal_gc(lua_State *L)
{
    Signal *s = signal_checkudata(L, 1);
    if (s->status == SIGNAL_PLAYING) {
        s->status = SIGNAL_STOPPED;
        if (s->thread)
            lhc_thread_join(s->thread, NULL);
        /* TODO: more work here? if not, delete this and use signal_stop instead */
    }

    return 0;
}

/* 
 * expects a C-Closure on top of the stack.
 * leaves the associated userdata on the stack.
 */
void signal_new_from_closure(lua_State *L)
{
    Signal *s = lua_newuserdata(L, sizeof(Signal));
    s->t = 0;
    s->read_buffer_empty = 1;
    s->status = SIGNAL_STOPPED;

    /* lua_settable() needs stack to be '... udata udata closure' */
    lua_pushvalue(L, -1);
    lua_pushvalue(L, -3);
    lua_remove(L, -4);
    lua_settable(L, LUA_REGISTRYINDEX); /* registry[udata] = closure */

    /* set metatable */
    luaL_getmetatable(L, "lhc.signal");
    lua_setmetatable(L, -2);

    /* userdata is left on the stack */
}

void signal_register(lua_State* L, luaL_Reg* lib)
{
    luaL_newmetatable(L, "lhc.signal");
    while (lib->name && lib->func)
    {
        lua_pushcfunction(L, lib->func); 
        lua_setfield(L, -2, lib->name);
        lib++;
    }
}

/*
 * looks for tbl[tbl_name] where tbl is the value at the given index
 * if the value is not a number or a function, get defaults[def_name]
 */
static void push_default_if_not_valid(lua_State* L, const char* def_name)
{
    if (!lua_isnumber(L, -1) && !lua_isfunction(L, -1)) 
    {
        lua_pop(L, 1);
        lua_getglobal(L, "defaults");
        lua_getfield(L, -1, def_name);
        lua_remove(L, -2);
        if (lua_isnil(L, -1)) 
            luaL_error(L, "Panic: Black hole emerged.");
    }
}

/*
 * create new signal userdata
 */
static int l_signal(lua_State* L)
{
    if (!lua_istable(L, 1))
        return luaL_error(L, "expected table argument");

    lua_rawgeti(L, 1, 1);
    push_default_if_not_valid(L, "generator");

    lua_getfield(L, 1, "f");
    push_default_if_not_valid(L, "freq");

    /* stack contains: table generator freq */
    lua_pushcclosure(L, &signal_closure, 2);
    signal_new_from_closure(L);

    /* remove argument table */
    lua_remove(L, -2);
    return 1;
}

/* 
 * registers signal function with lua 
 */
int luaopen_signal(lua_State *L)
{
    lua_register(L, "sig", l_signal);
    lua_register(L, "signal", l_signal);

    luaL_Reg basics[] = {
        {"__gc",  signal_gc},
        {"__add", signal_add},
        {"__mul", signal_mul},
        {"play",  signal_play},
        {"stop",  signal_stop},
        {NULL, NULL}};
    signal_register(L, basics);
    luaL_getmetatable(L, "lhc.signal");
    /* set metatable as index table for oo access*/
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    return 0;
}
