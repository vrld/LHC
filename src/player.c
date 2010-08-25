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
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <portaudio.h>

#include "player.h"
#include "pa_assert.h"

static int pa_play_callback(const void *inputBuffer, void* outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo* timeinfo,
		PaStreamCallbackFlags statusFlags,
		void *userdata)
{
	(void)inputBuffer;
	(void)timeinfo;
	(void)statusFlags;

	PlayerInfo* pi = (PlayerInfo*)(userdata);
	float *out = (float*)outputBuffer;

	size_t i;
	int c;
	for (i = 0; i < framesPerBuffer; ++i) {
		if (pi->pos >= pi->data->sample_count)
			pi->pos = pi->looping ? 0 : pi->data->sample_count - 1;

		/* multichannel output: samples are interleaved, e.g. stereo:
		 * c1,1|c2,1|c1,2|c2,2|... */
		for (c = 1; c <= pi->data->channels; ++c)
			*out++ = pi->data->samples[pi->pos * pi->data->channels + c - 1];
		pi->pos++;
	}

	if (pi->pos >= pi->data->sample_count)
		return paComplete;

	return paContinue;
}

PlayerInfo* l_player_checkplayer(lua_State* L, int idx)
{
	PlayerInfo* pi = (PlayerInfo*)luaL_checkudata(L, idx, "lhc.PlayerInfo");
	if (pi == NULL)
		luaL_typerror(L, idx, "Player");

	return pi;
}

int l_player_start(lua_State* L)
{
	l_player_stop(L);

	PlayerInfo* pi = l_player_checkplayer(L, 1);
	pi->pos = 0;
	PA_ASSERT_CMD( Pa_StartStream(pi->stream) );

	/* return player */
	return 1;
}

int l_player_stop(lua_State *L)
{
	PlayerInfo* pi = l_player_checkplayer(L, 1);
	if (Pa_IsStreamStopped(pi->stream) == 0) {
		PA_ASSERT_CMD( Pa_StopStream(pi->stream) );
	}

	/* return player */
	return 1;
}

int l_player_set_loop(lua_State* L)
{
	PlayerInfo* pi = l_player_checkplayer(L, 1);
	pi->looping = lua_toboolean(L, 2);

	/* return player */
	return 1;
}

int l_player_is_looping(lua_State* L)
{
	PlayerInfo* pi = l_player_checkplayer(L, 1);
	lua_pushboolean(L, pi->looping);
	return 1;
}

int l_player_gc(lua_State* L)
{
	l_player_stop(L);

	PlayerInfo* pi = l_player_checkplayer(L, 1);
	PA_ASSERT_CMD(Pa_CloseStream(pi->stream));

	/* possibly free associated sounddata  */
	/* TODO: mutex here? */
	pi->data->refcount--;
	if (pi->data->refcount <= 0)
		free(pi->data->samples);
	/* TODO: unmutex here? */

	return 0;
}

#define SETFUNCTION(L, idx, name, func) \
	lua_pushcfunction((L), (func)); \
	lua_setfield((L), (idx)-1, (name))
int l_player_new(lua_State* L)
{
	SoundData *data = l_sounddata_checksounddata(L, -1);
	lua_pop(L,1);

	PlayerInfo* pi = (PlayerInfo*)lua_newuserdata(L, sizeof(PlayerInfo));
	pi->data = data;
	pi->data->refcount++;
	pi->pos = 0;
	pi->looping = 0;

	PA_ASSERT_CMD( Pa_OpenDefaultStream(&pi->stream, 0, pi->data->channels,
				paFloat32, pi->data->rate, paFramesPerBufferUnspecified,
				pa_play_callback, pi) );

	if (luaL_newmetatable(L, "lhc.PlayerInfo")) {
		SETFUNCTION(L, -1, "play", l_player_start);
		SETFUNCTION(L, -1, "stop", l_player_stop);
		SETFUNCTION(L, -1, "set_loop", l_player_set_loop);
		SETFUNCTION(L, -1, "is_looping", l_player_is_looping);
		SETFUNCTION(L, -1, "__gc", l_player_gc);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);

	return 1;
}

int luaopen_player(lua_State* L)
{
	lua_register(L, "Player", l_player_new);
	return 0;
}
