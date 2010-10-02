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
#include <math.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>

#include "spectrum.h"
#include "sounddata.h"

Spectrum* l_spectrum_checkspectrum(lua_State* L, int idx)
{
	Spectrum* s = (Spectrum*)luaL_checkudata(L, idx, "lhc.Spectrum");
	if (s == NULL)
		luaL_typerror(L, idx, "Spectrum");

	return s;
}

int l_spectrum_gc(lua_State* L)
{
	Spectrum* s = l_spectrum_checkspectrum(L, 1);

	int c;
	for (c = 0; c < s->channels; ++c)
		fftw_free(s->data[c]);

	fftw_free(s->data);

	return 0;
}

int l_spectrum_tostring(lua_State* L)
{
	Spectrum* s = l_spectrum_checkspectrum(L, 1);
	lua_pushstring(L, "spectrum{channels = ");
	lua_pushinteger(L, s->channels);
	lua_pushstring(L, ", #data = ");
	lua_pushinteger(L, s->datapoints / 2);
	lua_pushstring(L, "}");
	lua_concat(L, 5);
	return 1;
}

int l_spectrum_get(lua_State* L)
{
	Spectrum *s = l_spectrum_checkspectrum(L, 1);
	size_t i = luaL_checkint(L, 2);
	int c = luaL_checkint(L, 3);

	if (c <= 0 || c > s->channels)
		return luaL_error(L, "channel number (%d) out of bounds (%d)", c, s->channels);

	if (i > s->datapoints / 2)
		return luaL_error(L, "data number (%d) out of bounds (%d)", i, s->datapoints / 2);

	fftw_complex* dp = &(s->data[c-1][i]);
	lua_pushnumber(L, *dp[0]);
	lua_pushnumber(L, *dp[1]);
	return 2;
}

int l_spectrum_set(lua_State* L)
{
	Spectrum *s = l_spectrum_checkspectrum(L, 1);
	size_t i = luaL_checkint(L, 2);
	int c = luaL_checkint(L, 3);

	if (c <= 0 || c > s->channels)
		return luaL_error(L, "channel number (%d) out of bounds (%d)", c, s->channels);

	if (i > s->datapoints)
		return luaL_error(L, "data number (%d) out of bounds (%d)", i, s->datapoints);

	fftw_complex* dp = &(s->data[c-1][i]);

	double mag = luaL_checknumber(L, 4);
	double arg = luaL_optnumber(L, 5, *dp[1]);

	*dp[0] = mag; *dp[1] = arg;

	lua_settop(L, 1);
	return 0;
}

int l_spectrum_map(lua_State* L)
{
	Spectrum *s = l_spectrum_checkspectrum(L, 1);
	if (lua_type(L, 2) != LUA_TFUNCTION)
		return luaL_typerror(L, 2, "function");

	size_t i;
	int c;
	for (c = 0; c < s->channels; ++c) {
		fftw_complex* data = s->data[c];
		for (i = 0; i < s->datapoints/2; ++i) {
			lua_pushvalue(L, 2);
			lua_pushinteger(L, i);
			lua_pushinteger(L, c+1);
			lua_pushnumber(L, data[i][0]);
			lua_pushnumber(L, data[i][1]);
			lua_call(L, 4, 2);
			data[i][0] = lua_tonumber(L, -2);
			data[i][1] = lua_tonumber(L, -1);
			lua_pop(L, 2);
		}
	}

	lua_settop(L, 1);
	return 0;
	return luaL_error(L, "not yet implemented");
}

static int fft(lua_State* L, SoundData* data, Spectrum *s)
{
	s->data = (fftw_complex**)fftw_malloc(sizeof(fftw_complex*) * s->channels);
	if (s->data == NULL)
		return luaL_error(L, "Cannot allocate memory for spectrum array (this is serious).");

	int c = 0;
	for (c = 0; c < s->channels; ++c) {
		s->data[c] = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * s->datapoints);
		if (s->data[c] == NULL) {
			for (--c; c >= 0; --c)
				fftw_free(s->data[c]);

			fftw_free(s->data);
			return luaL_error(L, "Cannot allocate memory for spectrum");
		}
	}

	for (c = 0; c < s->channels; ++c) {
		fftw_complex* inout = s->data[c];
		fftw_plan p = fftw_plan_dft_1d(s->datapoints, inout, inout,
				FFTW_FORWARD, FFTW_ESTIMATE);

		/* copy input with window applied */
		size_t i;
		for (i = 0; i < data->sample_count; ++i) {
			inout[i][0] = (double)data->samples[i * data->channels + c];

			lua_pushvalue(L, 2); /* push window function */
			lua_pushnumber(L, (double)i / (double)data->sample_count);
			lua_call(L, 1, 1);
			inout[i][0] *= lua_tonumber(L, -1);
			lua_pop(L, 1);

			inout[i][1] = .0;
		}
		for (; i < s->datapoints; ++i) {
			inout[i][0] = .0;
			inout[i][1] = .0;
		}

		/* perform fft */
		fftw_execute(p);
		fftw_destroy_plan(p);

		/* transform to magnitude/argument */
		fftw_complex* dp = inout;
		for (i = 0; i < s->datapoints; ++i, ++dp) {
			double mag = sqrt(*dp[0] * *dp[0] + *dp[1] * *dp[1]);
			double arg = atan(*dp[1] / *dp[0]);
			*dp[0] = mag; *dp[1] = arg;
		}
	}

	return 0;
}

#define SETFUNCTION(L, idx, name, func) \
	lua_pushcfunction((L), (func)); \
	lua_setfield((L), (idx)-1, (name))
int l_spectrum_dft(lua_State* L)
{
	SoundData *data = l_sounddata_checksounddata(L, 1);
	if (!lua_isfunction(L, 2)) {
		lua_getglobal(L, "window");
		lua_getfield(L, -1, "blackman");
		lua_replace(L, 2);
		lua_settop(L, 2);
	}

	Spectrum* s = (Spectrum*)lua_newuserdata(L, sizeof(Spectrum));
	s->channels = data->channels;
	s->datapoints = data->rate / 2;
	if (s->datapoints < data->sample_count)
		s->datapoints = data->sample_count;

	fft(L, data, s);

	/* create/assign metatable to userdata */
	if (luaL_newmetatable(L, "lhc.Spectrum")) {
		SETFUNCTION(L, -1, "get", l_spectrum_get);
		SETFUNCTION(L, -1, "set", l_spectrum_set);
		SETFUNCTION(L, -1, "map", l_spectrum_map);
		SETFUNCTION(L, -1, "__gc", l_spectrum_gc);
		SETFUNCTION(L, -1, "__tostring", l_spectrum_tostring);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);

	return 1;
}

int l_spectrum_idft(lua_State* L)
{
	Spectrum* s = l_spectrum_checkspectrum(L, 1);
	(void)s;
	return 1;
}

int luaopen_spectrum(lua_State* L)
{
	lua_register(L, "dft", l_spectrum_dft);
	lua_register(L, "idft", l_spectrum_idft);
	return 0;
}
