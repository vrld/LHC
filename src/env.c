/***
 * Copyright (c) 2012 Matthias Richter
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written authorization.
 *
 * If you find yourself in a situation where you can safe the author's life
 * without risking your own safety, you are obliged to do so.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <math.h>

#include "env.h"

static const char *CURVES_NAME = "lhc.env.curves";

static int lhc_env_inter_step(lua_State *L)
{
	lua_pushnumber(L, 0);
	return 1;
}

static int lhc_env_inter_lin(lua_State *L)
{
	// assume lua_top(L) == 1
	(void)L;
	return 1;
}

static int lhc_env_inter_exp(lua_State *L)
{
	lua_Number s = lua_tonumber(L, 1);
	if (s == 0 || s == 1)
		/* nothing */;
	else if (s < .5)
		lua_pushnumber(L, .5 * pow(2., 20.*s - 10.) - .001);
	else
		lua_pushnumber(L, 1.0005 * (1. - pow(2., 9.-20.*s)));
	return 1;
}

static int lhc_env_inter_sin(lua_State *L)
{
	lua_Number s = lua_tonumber(L, 1);
	lua_pushnumber(L, .5 - .5 * cos(s * 3.1415926535898));
	return 1;
}

static int lhc_env_inter_sqr(lua_State *L)
{
	lua_Number s = lua_tonumber(L, 1);
	if (s < .5)
		lua_pushnumber(L, 2. * s*s);
	else
	{
		s -= 1.;
		lua_pushnumber(L, 1. - 2 * s*s);
	}
	return 1;
}

static int lhc_env_inter_cub(lua_State *L)
{
	lua_Number s = lua_tonumber(L, 1);
	if (s < .5)
		lua_pushnumber(L, 4. * s*s*s);
	else
	{
		s -= 1.;
		lua_pushnumber(L, 1. + 4 * s*s*s);
	}
	return 1;
}

static int lhc_env_inter_curved_closure_pos(lua_State *L)
{
	lua_Number s = lua_tonumber(L, 1);
	lua_Number a = lua_tonumber(L, lua_upvalueindex(1));
	lua_pushnumber(L, pow(s, a));
	return 1;
}

static int lhc_env_inter_curved_closure_neg(lua_State *L)
{
	lua_Number s = lua_tonumber(L, 1);
	lua_Number a = lua_tonumber(L, lua_upvalueindex(1));
	lua_pushnumber(L, 1. - pow(1.-s, a));
	return 1;
}

static int lhc_env_inter_curved(lua_State *L)
{
	lua_Number a = lua_tonumber(L, 1);
	if (a >= 0)
	{
		lua_pushnumber(L, 1.+a);
		lua_pushcclosure(L, lhc_env_inter_curved_closure_pos, 1);
	}
	else
	{
		lua_pushnumber(L, 1.-a);
		lua_pushcclosure(L, lhc_env_inter_curved_closure_neg, 1);
	}
	return 1;
}

static int lhc_env_closure(lua_State *L)
{
	static const int UP_IDX    = 1;
	static const int UP_LEVELS = 2;
	static const int UP_L0     = 3;
	static const int UP_L1     = 4;
	static const int UP_TIMES  = 5;
	static const int UP_T0     = 6;
	static const int UP_T1     = 7;
	static const int UP_CURVES = 8;
	static const int UP_INTER  = 9;
	static const int UP_RATE   = 10;

	/* if idx > #times then return l1 end */
	size_t idx = lua_tointeger(L, lua_upvalueindex(UP_IDX));
	if (idx > lua_objlen(L, lua_upvalueindex(UP_TIMES)))
	{
		lua_pushvalue(L, lua_upvalueindex(UP_L0));
		return 1;
	}

	lua_Number t = lua_tonumber(L, 1) / lua_tonumber(L, lua_upvalueindex(UP_RATE));
	lua_Number t1 = lua_tonumber(L, lua_upvalueindex(UP_T1));
	lua_Number l1 = lua_tonumber(L, lua_upvalueindex(UP_L1));

	if (t >= t1)
	{
		/* idx = idx + 1 */
		++idx;
		lua_pushinteger(L, idx);
		lua_replace(L, lua_upvalueindex(UP_IDX));

		/* t0 = t1 */
		lua_pushnumber(L, t1);
		lua_replace(L, lua_upvalueindex(UP_T0));

		/* t1 = times[idx] + t1 */
		lua_rawgeti(L, lua_upvalueindex(UP_TIMES), idx);
		t1 += lua_tonumber(L, -1);
		lua_pop(L, 1);
		lua_pushnumber(L, t1);
		lua_replace(L, lua_upvalueindex(UP_T1));

		/* l0 = l1 */
		lua_pushnumber(L, l1);
		lua_replace(L, lua_upvalueindex(UP_L0));

		/* l1 = levels[idx+1] */
		lua_rawgeti(L, lua_upvalueindex(UP_LEVELS), idx+1);
		lua_replace(L, lua_upvalueindex(UP_L1));

		/* inter = curves[idx] */
		lua_rawgeti(L, lua_upvalueindex(UP_CURVES), idx);
		lua_replace(L, lua_upvalueindex(UP_INTER));
	}

	lua_Number l0 = lua_tonumber(L, lua_upvalueindex(UP_L0));
	if (l0 == l1)
	{
		lua_pushnumber(L, l1);
		return 1;
	}

	lua_Number t0 = lua_tonumber(L, lua_upvalueindex(UP_T0));
	lua_pushvalue(L, lua_upvalueindex(UP_INTER));
	lua_pushnumber(L, fmin(1., (t-t0)/(t1-t0)));
	lua_call(L, 1, 1);

	lua_Number c = lua_tonumber(L, -1);
	lua_pushnumber(L, l0 + (l1-l0) * c);
	return 1;
}

#define lhc_argcheck(L, narg, check, tname) \
	if (!(check(L, narg))) luaL_typerror(L, narg, tname)
static int lhc_env_new(lua_State *L)
{
	lua_settop(L, 4);
	lhc_argcheck(L, 1, lua_istable, "table");
	lhc_argcheck(L, 2, lua_istable, "table");

	int n_levels = lua_objlen(L, 1);
	int n_times  = lua_objlen(L, 2);

	if (n_levels <= n_times)
		return luaL_argerror(L, 2, "Number of levels must be greater than number of delays");

	/* curve = {curve, ..., curve} if curve is not a table */
	if (!lua_istable(L, 3))
	{
		lua_createtable(L, n_times, 0);
		for (int i = 0; i < n_times; ++i)
		{
			lua_pushvalue(L, 3);
			lua_rawseti(L, -2, i+1);
		}
		lua_replace(L, 3);
	}

	/* resolve curve names to functions */
	lua_getfield(L, LUA_REGISTRYINDEX, CURVES_NAME);
	for (int i = 1; i <= n_times; ++i)
	{
		lua_rawgeti(L, 3, i);
		if (lua_isfunction(L, -1) || lua_isnil(L, -1))
		{
			lua_pushvalue(L, -1);
		}
		else if (LUA_TNUMBER == lua_type(L, -1))
		{
			lua_pushcfunction(L, lhc_env_inter_curved);
			lua_insert(L, -2);
			lua_call(L, 1, 1);
		}
		else
		{
			lua_rawget(L, -2);
		}

		if (lua_isnil(L, -1))
		{
			lua_pop(L, 1);
			lua_pushcfunction(L, lhc_env_inter_lin);
		}

		lua_rawseti(L, 3, i);
	}

	/* idx = 1 */
	lua_pushinteger(L, 1);

	/* levels, l0 = levels[1], l1 = levels[2] */
	lua_pushvalue(L, 1);
	lua_rawgeti(L, 1, 1);
	lua_rawgeti(L, 1, 2);

	/* times, t0 = 0, t1 = times[1] */
	lua_pushvalue(L, 2);
	lua_pushnumber(L, .0);
	lua_rawgeti(L, 2, 1);

	/* curves, inter = curves[1] */
	lua_pushvalue(L, 3);
	lua_rawgeti(L, 3, 1);

	/* samplerate = samplerate or 44100. */
	lua_Number rate = luaL_optnumber(L, 4, 44100.);
	lua_pushnumber(L, rate);

	lua_pushcclosure(L, lhc_env_closure, 10);
	return 1;
}

#define lhc_setnumber(L, i, number) \
	lua_pushnumber(L, number); \
	lua_rawseti(L, -2, i)
static int lhc_env_triangle(lua_State *L)
{
	lua_Number dur   = luaL_optnumber(L, 1, 1.) * .5;
	lua_Number level = luaL_optnumber(L, 2, 1.);

	/* levels = {0, level, 0} */
	lua_createtable(L, 3, 0);
	lhc_setnumber(L, 1, .0);
	lhc_setnumber(L, 2, level);
	lhc_setnumber(L, 3, .0);
	lua_replace(L, 1);

	/* times = {dur, dur} */
	lua_createtable(L, 2, 0);
	lhc_setnumber(L, 1, dur);
	lhc_setnumber(L, 2, dur);
	lua_replace(L, 2);

	lua_pushcfunction(L, lhc_env_inter_lin);
	lua_insert(L, 3);

	return lhc_env_new(L);
}

static int lhc_env_sine(lua_State *L)
{
	lua_Number dur   = luaL_optnumber(L, 1, 1.) * .5;
	lua_Number level = luaL_optnumber(L, 2, 1.);

	/* levels = {0, level, 0} */
	lua_createtable(L, 3, 0);
	lhc_setnumber(L, 1, .0);
	lhc_setnumber(L, 2, level);
	lhc_setnumber(L, 3, .0);
	lua_replace(L, 1);

	/* times = {dur, dur} */
	lua_createtable(L, 2, 0);
	lhc_setnumber(L, 1, dur);
	lhc_setnumber(L, 2, dur);
	lua_replace(L, 2);

	lua_pushcfunction(L, lhc_env_inter_sin);
	lua_insert(L, 3);
	return lhc_env_new(L);
}

static int lhc_env_perc(lua_State *L)
{
	lua_Number attack  = luaL_optnumber(L, 1, .1);
	lua_Number release = luaL_optnumber(L, 2, .9);
	lua_Number level   = luaL_optnumber(L, 3, 1.);

	/* levels = {0, level, 0} */
	lua_createtable(L, 3, 0);
	lhc_setnumber(L, 1, .0);
	lhc_setnumber(L, 2, level);
	lhc_setnumber(L, 3, .0);
	lua_replace(L, 1);

	/* times = {dur, dur} */
	lua_createtable(L, 2, 0);
	lhc_setnumber(L, 1, attack);
	lhc_setnumber(L, 2, release);
	lua_replace(L, 2);

	/* curve = curve or -4 */
	if (lua_isnoneornil(L, 4))
	{
		lua_pushcfunction(L, lhc_env_inter_curved);
		lua_pushnumber(L, -4);
		lua_call(L, 1, 1);
		lua_replace(L, 4);
	}
	lua_remove(L, 3); /* remove level */

	return lhc_env_new(L);
}

static int lhc_env_linen(lua_State *L)
{
	lua_Number attack  = luaL_optnumber(L, 1, .1);
	lua_Number sustain = luaL_optnumber(L, 2, 1.);
	lua_Number release = luaL_optnumber(L, 3, 1.);
	lua_Number level   = luaL_optnumber(L, 4, 1.);

	/* levels = {0, level, 0} */
	lua_createtable(L, 4, 0);
	lhc_setnumber(L, 1, .0);
	lhc_setnumber(L, 2, level);
	lhc_setnumber(L, 3, level);
	lhc_setnumber(L, 4, .0);
	lua_replace(L, 1);

	/* times = {dur, dur} */
	lua_createtable(L, 3, 0);
	lhc_setnumber(L, 1, attack);
	lhc_setnumber(L, 2, sustain);
	lhc_setnumber(L, 3, release);
	lua_replace(L, 2);

	/* curve = curve or 'linear' */
	if (lua_isnoneornil(L, 5))
	{
		lua_pushcfunction(L, lhc_env_inter_lin);
		lua_replace(L, 5);
	}
	lua_remove(L, 4); /* remove release */
	lua_remove(L, 3); /* remove level */
	return lhc_env_new(L);
}

static int lhc_env_xyc(lua_State *L)
{
	lhc_argcheck(L, 1, lua_istable, "table");
	int n_coords = lua_objlen(L, 1);

	lua_createtable(L, n_coords+1, 0);
	lua_pushnumber(L, .0);
	lua_rawseti(L, -2, 1);
	lua_insert(L, 1);
	lua_createtable(L, n_coords, 0);
	lua_insert(L, 1);
	lua_createtable(L, n_coords, 0);
	lua_insert(L, 1);

	lua_Number tlast = .0;
	/* stack: levels times curves coords [rate] */
	for (int i = 1; i <= n_coords; ++i)
	{
		lua_rawgeti(L, 4, i);

		/* times[i] = coords[i][1] */
		lua_rawgeti(L, -1, 1);
		lua_Number t = luaL_checknumber(L, -1);
		if (t < tlast)
			return luaL_error(L, "Invalid coordinate %d: "
					"Times must be ordered ascending.", i);
		lua_pop(L, 1);
		lua_pushnumber(L, t - tlast);
		lua_rawseti(L, 2, i);
		tlast = t;

		/* levels[i+1] = coords[i][2] -  */
		lua_rawgeti(L, -1, 2);
		(void)luaL_checknumber(L, -1);
		lua_rawseti(L, 1, i+1);

		/* curves[i] = coords[i][3] */
		lua_rawgeti(L, -1, 3);
		lua_rawseti(L, 3, i);
	}

	lua_remove(L, 4);
	return lhc_env_new(L);
}

static int lhc_env_pairs(lua_State *L)
{
	lhc_argcheck(L, 1, lua_istable, "table");
	lhc_argcheck(L, 2, !lua_istable, "interpolator");

	int n_coords = lua_objlen(L, 1);
	for (int i = 1; i <= n_coords; ++i)
	{
		lua_rawgeti(L, 1, i);
		if (!lua_istable(L, -1))
			return luaL_typerror(L, 1, "table of tables");
		lua_pushvalue(L, 2);
		lua_rawseti(L, -2, 3);
	}

	lua_remove(L, 2);
	return lhc_env_xyc(L);
}


#define lhc_register_function(L, func, name) \
	lua_pushcfunction(L, func); \
	lua_setfield(L, -2, name)
int luaopen_lhc_env(lua_State* L)
{
	lua_createtable(L, 0, 8);

	/* lhc.env.curves */
	lua_createtable(L, 0, 12);
	lhc_register_function(L, lhc_env_inter_step,   "step");
	lhc_register_function(L, lhc_env_inter_lin,    "lin");
	lhc_register_function(L, lhc_env_inter_lin,    "linear");
	lhc_register_function(L, lhc_env_inter_exp,    "exp");
	lhc_register_function(L, lhc_env_inter_exp,    "exponential");
	lhc_register_function(L, lhc_env_inter_sin,    "sin");
	lhc_register_function(L, lhc_env_inter_sin,    "sine");
	lhc_register_function(L, lhc_env_inter_sqr,    "sqr");
	lhc_register_function(L, lhc_env_inter_sqr,    "squared");
	lhc_register_function(L, lhc_env_inter_cub,    "cub");
	lhc_register_function(L, lhc_env_inter_cub,    "cubed");
	lhc_register_function(L, lhc_env_inter_curved, "curved");
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "curves");
	lua_setfield(L, LUA_REGISTRYINDEX, CURVES_NAME);

	/* lhc.env.new */
	lhc_register_function(L, lhc_env_new,      "new");
	lhc_register_function(L, lhc_env_triangle, "triangle");
	lhc_register_function(L, lhc_env_sine,     "sine");
	lhc_register_function(L, lhc_env_perc,     "perc");
	lhc_register_function(L, lhc_env_linen,    "linen");
	lhc_register_function(L, lhc_env_xyc,      "xyc");
	lhc_register_function(L, lhc_env_pairs,    "pairs");

	return 1;
}
