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
#include "signal_tools.h"
#include "signal.h"
#include <stdlib.h>
#include <lauxlib.h>
#include <math.h>

static int signal_normalize_closure(lua_State* L)
{
    int i;
    double t = luaL_checknumber(L, 1);
    double max, maxidx, lastmax, modulation;
    double top = lua_tonumber(L, lua_upvalueindex(2));
    double values[SAMPLE_BUFFER_SIZE];

    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushnumber(L, t);
    lua_call(L, 1, 2);

    max = 1e-10;
    maxidx = 1;
    for (i = 1; i <= SAMPLE_BUFFER_SIZE; ++i)
    {
        lua_rawgeti(L, -2, i);
        values[i-1] = lua_tonumber(L, -1);
        if (max < fabs(values[i-1])) {
            maxidx = (double)i;
            max = fabs(values[i-1]);
        }
        lua_pop(L, 1);
    }

    if (maxidx < 32)
        maxidx = 32.;

    lua_pushvalue(L, lua_upvalueindex(3));
    lua_rawgeti(L, -1, 1);
    lastmax = lua_tonumber(L, -1);
    lua_pushnumber(L, max); 
    lua_rawseti(L, -3, 1);
    lua_pop(L, 2);

    for (i = 1; i <= SAMPLE_BUFFER_SIZE; ++i)
    {
        modulation = (max - lastmax);
        if (i < maxidx)
            modulation *= (double)i / maxidx;
        modulation += lastmax;
        lua_pushnumber(L, values[i-1] / modulation * top);
        lua_rawseti(L, -3, i);
    }

    return 2;
}

int signal_normalize(lua_State *L)
{
    if (!signal_userdata_is_signal(L, 1))
        return luaL_typerror(L, 1, "signal");
    if (!lua_isnumber(L, 2)) {
        lua_settop(L, 1);
        lua_pushnumber(L, .8); /* TODO: unmagic this number */
    }
    lua_settop(L, 2);

    signal_replace_udata_with_closure(L, 1);
    lua_createtable(L, 1, 0);
    lua_pushnumber(L, 0);
    lua_rawseti(L, -2, 1);
    lua_pushcclosure(L, &signal_normalize_closure, 3);
    signal_new_from_closure(L);
    return 1;
}
