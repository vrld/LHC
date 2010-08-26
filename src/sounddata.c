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
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "sounddata.h"

SoundData* l_sounddata_checksounddata(lua_State* L, int idx)
{
	SoundData* data = (SoundData*)luaL_checkudata(L, idx, "lhc.SoundData");
	if (data == NULL)
		luaL_typerror(L, idx, "SoundData");

	return data;
}

int l_sounddata_samplerate(lua_State* L)
{
	SoundData* data = l_sounddata_checksounddata(L, 1);
	lua_pushnumber(L, data->rate);
	return 1;
}

int l_sounddata_length(lua_State* L)
{
	SoundData* data = l_sounddata_checksounddata(L, 1);
	lua_pushnumber(L, data->len);
	return 1;
}

int l_sounddata_channels(lua_State* L)
{
	SoundData* data = l_sounddata_checksounddata(L, 1);
	lua_pushnumber(L, data->channels);
	return 1;
}

int l_sounddata_samplecount(lua_State* L)
{
	SoundData* data = l_sounddata_checksounddata(L, 1);
	lua_pushnumber(L, data->sample_count);
	return 1;
}

int l_sounddata_to_index(lua_State* L)
{
	SoundData* data = l_sounddata_checksounddata(L, 1);
	double t = luaL_checknumber(L, 2);
	lua_pushinteger(L, (int)(t * data->rate));
	return 1;
}

int l_sounddata_to_time(lua_State* L)
{
	SoundData* data = l_sounddata_checksounddata(L, 1);
	double idx = luaL_checknumber(L, 2);
	lua_pushnumber(L, idx / data->rate);
	return 1;
}

int l_sounddata_gc(lua_State* L)
{
	SoundData* data = l_sounddata_checksounddata(L, 1);
	/* TODO: mutex here */
	data->refcount--;
	if (data->refcount <= 0)
		free(data->samples);
	/* TODO: unmutex here */
	return 0;
}

int l_sounddata_tostring(lua_State* L)
{
	SoundData* data = l_sounddata_checksounddata(L, 1);
	lua_pushstring(L, "sounddata{samplerate=");
	lua_pushnumber(L, data->rate);
	lua_pushstring(L, ", length=");
	lua_pushnumber(L, data->len);
	lua_pushstring(L, ", channels=");
	lua_pushnumber(L, data->channels);
	lua_pushstring(L, ", samples=");
	lua_pushnumber(L, data->sample_count);
	lua_pushstring(L, "}");
	lua_concat(L, 9);
	return 1;
}

int l_sounddata_clone(lua_State* L)
{
	(void)L;
	return 0;
}

int l_sounddata_get(lua_State* L)
{
	SoundData* data = l_sounddata_checksounddata(L, 1);
	int idx = luaL_checkint(L, 2);
	int channel = luaL_checkint(L, 3);

	size_t sample_idx = idx * data->channels + channel - 1;
	if (sample_idx >= data->sample_count)
		return luaL_error(L, "requested sample number out of bounds");
	lua_pushnumber(L, data->samples[sample_idx]);

	return 1;
}

int l_sounddata_set(lua_State* L)
{
	SoundData* data = l_sounddata_checksounddata(L, 1);
	int idx = luaL_checkint(L, 2);
	int channel = luaL_checkint(L, 3);

	size_t sample_idx = idx * data->channels + channel - 1;
	if (sample_idx >= data->sample_count)
		return luaL_error(L, "requested sample number out of bounds");
	data->samples[sample_idx] = luaL_checknumber(L, 4);

	/* return sounddata */
	lua_settop(L, 1);
	return 1;
}

int l_sounddata_map(lua_State* L)
{
	SoundData* data = l_sounddata_checksounddata(L, 1);
	if (lua_type(L, 2) != LUA_TFUNCTION)
		return luaL_typerror(L, 2, "function");

	size_t i;
	int c;
	for (i = 0; i < data->sample_count; ++i) {
		for (c = 1; c <= data->channels; ++c) {
			lua_pushvalue(L, 2);
			lua_pushinteger(L, i);
			lua_pushinteger(L, c);
			lua_pushnumber(L, data->samples[i * data->channels + c - 1]);
			lua_call(L, 3, 1); /* TODO: error checking? */
			data->samples[i * data->channels + c - 1] = lua_tonumber(L, -1);
			lua_pop(L, 1);
		}
	}

	/* return sounddata */
	lua_settop(L, 1);
	return 1;
}

int l_sounddata_maptime(lua_State* L)
{
	SoundData* data = l_sounddata_checksounddata(L, 1);
	if (lua_type(L, 2) != LUA_TFUNCTION)
		return luaL_typerror(L, 2, "function");

	size_t i;
	int c;
	for (i = 0; i < data->sample_count; ++i) {
		for (c = 1; c <= data->channels; ++c) {
			lua_pushvalue(L, 2);
			lua_pushnumber(L, (double)i / data->rate);
			lua_pushinteger(L, c);
			lua_pushnumber(L, data->samples[i * data->channels + c - 1]);
			lua_call(L, 3, 1); /* TODO: error checking? */
			data->samples[i * data->channels + c - 1] = lua_tonumber(L, -1);
			lua_pop(L, 1);
		}
	}

	/* return sounddata */
	lua_settop(L, 1);
	return 1;
}

static int l_sounddata_push_result_data(lua_State*L, SoundData* d1, SoundData* d2, double len)
{
	if (d1->rate != d2->rate)
		return luaL_error(L, "sample rate does not match");

	if (d1->channels != d2->channels) /* TODO: channel multiplying */
		return luaL_error(L, "channel count does not match");

	lua_createtable(L, 0, 2);
	lua_pushnumber(L, d1->rate);
	lua_setfield(L, -2, "rate");
	lua_pushnumber(L, len);
	lua_setfield(L, -2, "len");
	lua_pushinteger(L, d2->channels);
	lua_setfield(L, -2, "channels");

	l_sounddata_new(L);
	return 0;
}

int l_sounddata_add(lua_State* L)
{
	lua_settop(L, 2);
	SoundData* d1 = l_sounddata_checksounddata(L, 1);
	SoundData* d2 = l_sounddata_checksounddata(L, 2);

	l_sounddata_push_result_data(L, d1, d2, d1->len > d2->len ? d1->len : d2->len);
	SoundData* res = l_sounddata_checksounddata(L, 3);

	size_t i;
	double v1, v2;
	for (i = 0; i < res->sample_count * res->channels; ++i) {
		v1 = i < d1->sample_count * res->channels ? d1->samples[i] : .0;
		v2 = i < d2->sample_count * res->channels ? d2->samples[i] : .0;
		res->samples[i] = v1 + v2;
	}

	return 1;
}

int l_sounddata_mul(lua_State* L)
{
	lua_settop(L, 2);
	SoundData* d1 = l_sounddata_checksounddata(L, 1);
	SoundData* d2 = l_sounddata_checksounddata(L, 2);

	l_sounddata_push_result_data(L, d1, d2, d1->len > d2->len ? d1->len : d2->len);
	SoundData* res = l_sounddata_checksounddata(L, 3);

	size_t i;
	double v1, v2;
	for (i = 0; i < res->sample_count * res->channels; ++i) {
		v1 = i < d1->sample_count * res->channels ? d1->samples[i] : 1.;
		v2 = i < d2->sample_count * res->channels ? d2->samples[i] : 1.;
		res->samples[i] = v1 * v2;
	}

	return 1;
}

int l_sounddata_append(lua_State* L)
{
	lua_settop(L, 2);
	SoundData* d1 = l_sounddata_checksounddata(L, 1);
	SoundData* d2 = l_sounddata_checksounddata(L, 2);

	l_sounddata_push_result_data(L, d1, d2, d1->len + d2->len);
	SoundData* res = l_sounddata_checksounddata(L, 3);
	assert(d1->sample_count + d2->sample_count == res->sample_count);

	size_t i;
	for (i = 0; i < d1->sample_count * res->channels; ++i) {
		res->samples[i] = d1->samples[i];
	}
	for (i = d1->sample_count; i < res->sample_count * res->channels; ++i) {
		res->samples[i] = d2->samples[i - d1->sample_count * res->channels];
	}

	return 1;
}

#define SETFUNCTION(L, idx, name, func) \
	lua_pushcfunction((L), (func)); \
	lua_setfield((L), (idx)-1, (name))

static void l_sounddata_push_metatable(lua_State* L)
{
	/* maybe create metatable */
	if (luaL_newmetatable(L, "lhc.SoundData")) {
		SETFUNCTION(L, -1, "samplerate", l_sounddata_samplerate);
		SETFUNCTION(L, -1, "length", l_sounddata_length);
		SETFUNCTION(L, -1, "channels", l_sounddata_channels);
		SETFUNCTION(L, -1, "samplecount", l_sounddata_samplecount);

		SETFUNCTION(L, -1, "to_index", l_sounddata_to_index);
		SETFUNCTION(L, -1, "to_time", l_sounddata_to_time);

		SETFUNCTION(L, -1, "clone", l_sounddata_clone);
		SETFUNCTION(L, -1, "set", l_sounddata_set);
		SETFUNCTION(L, -1, "get", l_sounddata_get);
		SETFUNCTION(L, -1, "map", l_sounddata_map);
		SETFUNCTION(L, -1, "maptime", l_sounddata_maptime);

		SETFUNCTION(L, -1, "append", l_sounddata_append);
		SETFUNCTION(L, -1, "__add", l_sounddata_add);
		SETFUNCTION(L, -1, "__mul", l_sounddata_mul);
		SETFUNCTION(L, -1, "__gc", l_sounddata_gc);
		SETFUNCTION(L, -1, "__tostring", l_sounddata_tostring);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
}

int l_sounddata_new(lua_State* L)
{
	size_t i;
	if (!lua_istable(L, -1))
		return luaL_typerror(L, 1, "table");

	lua_getfield(L, -1, "rate");
	double rate = luaL_optnumber(L, -1, 44100);
	if (rate <= 0)
		return luaL_error(L, "invalid sample rate");

	lua_getfield(L, -2, "len");
	double len = luaL_optnumber(L, -1, 1);
	if (len <= .0)
		return luaL_error(L, "invalid length");

	lua_getfield(L, -3, "channels");
	int channels = luaL_optint(L, -1, 1);
	if (channels < 1)
		return luaL_error(L, "Need at least one channel");

	/* clear stack */
	lua_pop(L, 4);

	SoundData* data = (SoundData*)lua_newuserdata(L, sizeof(SoundData));
	data->rate = rate;
	data->len = len;
	data->channels = channels;
	data->refcount = 1;
	data->sample_count = (size_t)(ceil(rate * len));

	data->samples = (float*)malloc(data->sample_count * channels * sizeof(float));
	for (i = 0; i < data->sample_count * channels; ++i)
		data->samples[i] = .0;

	l_sounddata_push_metatable(L);
	lua_setmetatable(L, -2);

	return 1;
}

int luaopen_sounddata(lua_State* L)
{
	lua_register(L, "SoundData", l_sounddata_new);
	lua_register(L, "SD", l_sounddata_new);
	return 0;
}
