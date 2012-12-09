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

#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "player.h"

static const char *INTERNAL_NAME = "lhc.player-instance";
static const char *CLEANUP_NAME  = "lhc.player.cleanup";
static const char *BUFFERS_NAME  = "lhc.player.cleanup";

static int pa_stream_callback(const void* inputBuffer, void* outputBuffer,
		unsigned long frames, const PaStreamCallbackTimeInfo* timeinfo,
		PaStreamCallbackFlags status, void* udata)
{
	(void)inputBuffer;
	(void)timeinfo;
	(void)status;

	PlayerInstance* pi = (PlayerInstance*)udata;

	float *in  = pi->buffer + (pi->sample_pos * pi->nchannels);
	float *out = (float*)outputBuffer;

	int i, c;
	for (i = 0; i < (int)frames; ++i, ++pi->sample_pos)
	{
		if (pi->sample_pos > pi->nsamples)
		{
			if (pi->is_looping)
			{
				pi->sample_pos = 0;
				in = pi->buffer;
			}
			else
				return paComplete;
		}

		for (c = 0; c < pi->nchannels; ++c)
			*out++ = *in++;
	}

	return paContinue;
}
PlayerInstance *lhc_checkplayer(lua_State* L, int idx)
{
	return (PlayerInstance *)luaL_checkudata(L, idx, INTERNAL_NAME);
}

static int lhc_player_seekTo(lua_State* L)
{
	PlayerInstance* pi = lhc_checkplayer(L, 1);
	int pos = luaL_checkint(L, 2);

	if (pos < 0 || pos > (int)pi->nsamples)
		return luaL_error(L, "Cannot seek to sample %d: out of bounds", pos);
	pi->sample_pos = (size_t)pos;

	lua_settop(L, 1);
	return 1;
}

static int lhc_player_seek(lua_State* L)
{
	PlayerInstance* pi = lhc_checkplayer(L, 1);
	int delta = luaL_checkint(L, 2);

	lua_pushinteger(L, pi->sample_pos + delta);
	lua_replace(L, 2);
	return lhc_player_seekTo(L);
}

int lhc_player_play(lua_State* L)
{
	PlayerInstance* pi = lhc_checkplayer(L, 1);
	if (NULL == pi->stream)
		return luaL_error(L, "Stream not properly initialized.");

	PaError err = Pa_StartStream(pi->stream);
	if (err != paNoError)
		return luaL_error(L, "Cannot play stream: %s", Pa_GetErrorText(err));

	lua_settop(L, 1);
	return 1;
}

static int lhc_player_pause(lua_State* L)
{
	PlayerInstance* pi = lhc_checkplayer(L, 1);
	if (NULL == pi->stream)
		return luaL_error(L, "Stream not properly initialized.");

	PaError err = Pa_AbortStream(pi->stream);
	if (err != paNoError)
		return luaL_error(L, "Cannot pause stream: %s", Pa_GetErrorText(err));

	lua_settop(L, 1);
	return 1;
}

static int lhc_player_stop(lua_State* L)
{
	lhc_player_pause(L);
	lua_pushinteger(L, 0);
	return lhc_player_seekTo(L);
}

static int lhc_player_rewind(lua_State *L)
{
	lua_settop(L, 1);
	lua_pushinteger(L, 0);
	return lhc_player_seekTo(L);
}

static int lhc_player_load(lua_State* L)
{
	PlayerInstance* pi = lhc_checkplayer(L, 1);
	if (NULL == pi->stream)
		return luaL_error(L, "Stream not properly initialized.");
	lua_pushnumber(L, Pa_GetStreamCpuLoad(pi->stream));
	return 1;
}

static int lhc_player___gc(lua_State* L)
{
	PlayerInstance* pi = (PlayerInstance *)lua_touserdata(L, 1);

	PaError err = Pa_CloseStream(pi->stream);
	if (err != paNoError)
		fprintf(stderr, "Unable to close player stream: %s\n", Pa_GetErrorText(err));

	/* buffers[pi] = nil */
	luaL_getmetatable(L, BUFFERS_NAME);
	lua_pushvalue(L, 1);
	lua_pushnil(L);
	lua_rawset(L, -3);
	lua_pop(L, 1);

	return 0;
}

int lhc_player_new(lua_State* L)
{
	float *buffer     = lhc_checkbuffer(L, 1);
	size_t nsamples   = lhc_buffer_nsamples(L, 1);
	double samplerate = luaL_optnumber(L, 2, 44100);
	int    nchannels  = luaL_optint(L, 3, 1);
	int    is_looping = lua_toboolean(L, 4);

	if (nsamples % nchannels != 0)
		return luaL_error(L, "Buffer size mismatches number of requested channels");

	PlayerInstance *pi = lua_newuserdata(L, sizeof(PlayerInstance));

	PaError err = Pa_OpenDefaultStream(&pi->stream, 0, nchannels,
			paFloat32, samplerate, paFramesPerBufferUnspecified,
			pa_stream_callback, pi);

	if (err != paNoError)
	{
		pi->stream = NULL;
		return luaL_error(L, "Cannot create player stream: %s\n", Pa_GetErrorText(err));
	}

	pi->nchannels  = nchannels;
	pi->is_looping = is_looping;
	pi->sample_pos = 0;
	pi->nsamples   = nsamples / nchannels;
	pi->buffer     = buffer;

	/* buffers[pi] = buffer */
	luaL_getmetatable(L, BUFFERS_NAME);
	lua_pushvalue(L, -2);
	lua_pushvalue(L, 1);
	lua_rawset(L, -3);
	lua_pop(L, 1);

	if (luaL_newmetatable(L, INTERNAL_NAME))
	{
		lua_pushcfunction(L, lhc_player___gc);
		lua_setfield(L, -2, "__gc");

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, lhc_player_seekTo);
		lua_setfield(L, -2, "seekTo");

		lua_pushcfunction(L, lhc_player_seek);
		lua_setfield(L, -2, "seek");

		lua_pushcfunction(L, lhc_player_play);
		lua_setfield(L, -2, "play");

		lua_pushcfunction(L, lhc_player_pause);
		lua_setfield(L, -2, "pause");

		lua_pushcfunction(L, lhc_player_rewind);
		lua_setfield(L, -2, "rewind");

		lua_pushcfunction(L, lhc_player_stop);
		lua_setfield(L, -2, "stop");

		lua_pushcfunction(L, lhc_player_load);
		lua_setfield(L, -2, "load");
	}
	lua_setmetatable(L, -2);

	return 1;
}


/* cleanup function of the dummy object created in luaopen_lhc_player */
static int lhc_player_cleanup(lua_State* L)
{
	(void)L;
	PaError err = Pa_Terminate();
	if (paNoError != err)
		fprintf(stderr, "Unable to properly terminate PortAudio: %s\n", Pa_GetErrorText(err));
	return 0;
}

int luaopen_lhc_player(lua_State* L)
{
	PaError err = Pa_Initialize();
	if (paNoError != err)
		return luaL_error(L, "Cannot open PortAudio: %s\n", Pa_GetErrorText(err));

	/* create dummy object that shuts down PA when the program shuts down */
	lua_newuserdata(L, 0);
	luaL_newmetatable(L, CLEANUP_NAME);
	lua_pushcfunction(L, lhc_player_cleanup);
	lua_setfield(L, -2, "__gc");
	lua_setmetatable(L, -2);
	lua_pushvalue(L, -1);
	lua_rawset(L, LUA_REGISTRYINDEX); /* registry[obj] = obj */

	/* table holding references to the buffers to prevent the GC to delete them
	 * while in use by a player instance
	 */
	luaL_newmetatable(L, BUFFERS_NAME);
	lua_pop(L, 1);

	lua_pushcfunction(L, lhc_player_new);

	return 1;
}
