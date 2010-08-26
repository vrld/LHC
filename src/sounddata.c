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

int l_sounddata_get(lua_State* L)
{
	SoundData* data = l_sounddata_checksounddata(L, 1);
	int idx = luaL_checkint(L, 2);
	int channel = luaL_checkint(L, 3);

	if (channel <= 0 || channel > data->channels)
		return luaL_error(L, "channel %d out of bounds (has to be 0 < channel < %d)", channel, data->channels);

	size_t sample_idx = idx * data->channels + channel - 1;
	if (sample_idx >= data->sample_count * data->channels)
		return luaL_error(L, "requested sample number out of bounds");
	lua_pushnumber(L, data->samples[sample_idx]);

	return 1;
}

int l_sounddata_set(lua_State* L)
{
	SoundData* data = l_sounddata_checksounddata(L, 1);
	int idx = luaL_checkint(L, 2);
	int channel = luaL_checkint(L, 3);

	if (channel <= 0 || channel > data->channels)
		return luaL_error(L, "channel %d out of bounds (has to be 0 < channel < %d)", channel, data->channels);

	size_t sample_idx = idx * data->channels + channel - 1;
	if (sample_idx >= data->sample_count * data->channels)
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

		SETFUNCTION(L, -1, "set", l_sounddata_set);
		SETFUNCTION(L, -1, "get", l_sounddata_get);
		SETFUNCTION(L, -1, "map", l_sounddata_map);

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
