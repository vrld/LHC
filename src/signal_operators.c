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
#include <lauxlib.h>

/*
 * creates 3 functions:
 *  signal_[name]_closure         combines 2 signals using OP
 *  signal_[name]_number_closure  combines signal and number using OP
 *  signal_[name]                 inspects arguments on the stack and
 *                                creates signal with corrosponding closure
 *  only signal_[name] will be exported to lua
 */
#define SIGNAL_OPERATOR(name, OP)                                \
static int signal_##name##_closure(lua_State* L)                 \
{                                                                \
    double t = luaL_checknumber(L, 1);                           \
    double val;                                                  \
    size_t i;                                                    \
                                                                 \
    /* call signal 1, omit returned new t */                     \
    lua_pushvalue(L, lua_upvalueindex(1));                       \
    lua_pushnumber(L, t);                                        \
    lua_call(L, 1, 1);                                           \
                                                                 \
    /* call signal 2 */                                          \
    lua_pushvalue(L, lua_upvalueindex(2));                       \
    lua_pushnumber(L, t);                                        \
    lua_call(L, 1, 2);                                           \
                                                                 \
    /* for i=1,N do tbl2[i] = tbl1[i] <OP> tbl2[i] end */        \
    for (i = 1; i <= SAMPLE_BUFFER_SIZE; ++i)                    \
    {                                                            \
        lua_rawgeti(L, -3, i);                                   \
        val = lua_tonumber(L, -1);                               \
        lua_rawgeti(L, -3, i);                                   \
        val OP##= lua_tonumber(L, -1);                           \
        lua_pop(L, 2); /* remove signal1 and signal2 values*/    \
                                                                 \
        lua_pushnumber(L, val);                                  \
        lua_rawseti(L, -3, i);                                   \
    }                                                            \
    /* new values in second table, t_new is already there */     \
    return 2;                                                    \
}                                                                \
                                                                 \
static int signal_##name##_number_closure(lua_State* L)          \
{                                                                \
    double t = luaL_checknumber(L, 1);                           \
    double val, c;                                               \
    size_t i;                                                    \
                                                                 \
    c = lua_tonumber(L, lua_upvalueindex(1));                    \
                                                                 \
    /* call signal */                                            \
    lua_pushvalue(L, lua_upvalueindex(2));                       \
    lua_pushnumber(L, t);                                        \
    lua_call(L, 1, 2);                                           \
                                                                 \
    /* for i=1,N do tbl[i] = tbl[i] + c end */                   \
    for (i = 1; i <= SAMPLE_BUFFER_SIZE; ++i)                    \
    {                                                            \
        lua_rawgeti(L, -2, i);                                   \
        val = lua_tonumber(L, -1) OP c;                          \
        lua_pop(L, 1);                                           \
                                                                 \
        lua_pushnumber(L, val);                                  \
        lua_rawseti(L, -3, i);                                   \
    }                                                            \
    /* new values in second table, t_new is already there */     \
    return 2;                                                    \
}                                                                \
                                                                 \
int signal_##name(lua_State *L)                                  \
{                                                                \
    lua_settop(L, 2);                                            \
    if (lua_isnumber(L, 2)) /* swap 1st with 2nd element */      \
        lua_insert(L, 1);                                        \
                                                                 \
    if (!signal_userdata_is_signal(L, 2))                        \
        return luaL_typerror(L, 2, "signal");                    \
    signal_replace_udata_with_closure(L, 2);                     \
                                                                 \
    if (lua_isnumber(L, 1)) {                                    \
        lua_pushcclosure(L, &signal_##name##_number_closure, 2); \
    } else if (signal_userdata_is_signal(L, 1)) {                \
        signal_replace_udata_with_closure(L, 1);                 \
        lua_pushcclosure(L, &signal_##name##_closure, 2);        \
    } else {                                                     \
        return luaL_typerror(L, 1, "signal or number");          \
    }                                                            \
    signal_new_from_closure(L);                                  \
    return 1;                                                    \
}

SIGNAL_OPERATOR(add, +);
SIGNAL_OPERATOR(mul, *);

