#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <portaudio.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "inter_stack_tools.h"

struct PlayerInfo
{
	int channels;
	int pos;
	double samplerate;
	PaStream *stream;
	lua_State* L;
};
typedef struct PlayerInfo PlayerInfo;

static int l_panic(lua_State* L)
{
	lua_Debug ar;
	lua_getstack(L, 1, &ar);
	fprintf(stderr, "Error in callback function (%s) at line %d:\n%s", ar.source, ar.currentline, lua_tostring(L, -1));
	return 0;
}

static int pa_stream_callback(const void* inputBuffer, void* outputBuffer,
		unsigned long frames, const PaStreamCallbackTimeInfo* timeinfo,
		PaStreamCallbackFlags status, void* udata)
{
	(void)inputBuffer;
	(void)timeinfo;
	(void)status;

	float* out = (float*)outputBuffer;
	PlayerInfo* pi = (PlayerInfo*)udata;
	lua_State* L = pi->L;

	/* function callback(out, nsamples, pos, channels, rate) */
	lua_pushvalue(L, 1);
	lua_pushvalue(L, 2);
	lua_pushnumber(L, frames);
	lua_pushnumber(L, pi->pos);
	lua_pushnumber(L, pi->channels);
	lua_pushnumber(L, pi->samplerate);

	if (0 != lua_pcall(L, 5, 0, 0)) {
		fprintf(stderr, "Error calling function: %s\n", lua_tostring(L, -1));
		return paAbort;
	}

	/* fill buffer */
	for (size_t i = 1; i <= frames*pi->channels; ++i, ++out) {
		lua_rawgeti(L, 2, i);
		*out = lua_tonumber(L, -1);
		if (i % (LUA_MINSTACK - 2)) lua_settop(L, 2);
	}

	/* reset stack */
	lua_settop(L, 2);

	pi->pos += frames;

	return paContinue;
}


PlayerInfo* l_checkplayer(lua_State* L, int idx)
{
	PlayerInfo* pi = (PlayerInfo*)luaL_checkudata(L, idx, "lhc.player.player");
	if (NULL == pi)
		luaL_typerror(L, idx, "Player");
	return pi;
}

static int l_player_play(lua_State* L)
{
	PlayerInfo* pi = l_checkplayer(L, 1);
	if (NULL == pi->stream)
		return luaL_error(L, "Stream not properly initialized.");

	PaError err = Pa_StartStream(pi->stream);
	if (err != paNoError)
		return luaL_error(L, "Cannot play stream: %s", Pa_GetErrorText(err));

	return 0;
}

static int l_player_stop(lua_State* L)
{
	PlayerInfo* pi = l_checkplayer(L, 1);
	if (NULL == pi->stream)
		return luaL_error(L, "Stream not properly initialized.");

	PaError err = Pa_AbortStream(pi->stream);
	if (err != paNoError)
		return luaL_error(L, "Cannot stop stream: %s", Pa_GetErrorText(err));

	return 0;
}

static int l_player_seek(lua_State* L)
{
	PlayerInfo* pi = l_checkplayer(L, 1);
	int pos = luaL_checkint(L, 2);

	l_player_stop(L);
	pi->pos = pos;
	l_player_play(L);

	return 0;
}

static int l_player_load(lua_State* L)
{
	PlayerInfo* pi = l_checkplayer(L, 1);
	if (NULL == pi->stream)
		return luaL_error(L, "Stream not properly initialized.");
	lua_pushnumber(L, Pa_GetStreamCpuLoad(pi->stream));
	return 1;
}

static int l_player_gc(lua_State* L)
{
	PlayerInfo* pi = l_checkplayer(L, 1);
	if (NULL == pi->stream)
		return 0;

	PaError err = Pa_CloseStream(pi->stream);
	if (err != paNoError)
		fprintf(stderr, "Unable to close player stream: %s\n", Pa_GetErrorText(err));

	lua_close(pi->L);

	return 0;
}

/* check if user queries struct members. otherwise forward to metatable. */
static int l_player_index(lua_State* L)
{
	PlayerInfo* pi = (PlayerInfo*)lua_touserdata(L, 1);
	assert(NULL != pi);
	const char* key = lua_tostring(L, 2);

	if (0 == strcmp("pos", key)) {
		lua_pushnumber(L, pi->pos);
	} else if (0 == strcmp("samplerate", key)) {
		lua_pushnumber(L, pi->samplerate);
	} else if (0 == strcmp("time", key)) {
		lua_pushnumber(L, (double)pi->pos / pi->samplerate);
	} else {
		if (0 != lua_getmetatable(L, 1))
			lua_getfield(L, -1, key);
		else /* just to be sure */
			lua_pushnil(L);
	}

	return 1;
}

/***
 * function Player.new(callback, channels, samplerate, buffer_size)
 * function callback(out, nsamples, pos, channels, rate)
 *     ... synthesize! ...
 *     -- postcondition: #out == nSamples
 * end
 */
static int l_player_new(lua_State* L)
{
	int    channels    = luaL_optint(L, 2, 2);
	double samplerate  = luaL_optnumber(L, 3, 44100);
	size_t size_buffer = luaL_optint(L, 4, DEFAULT_PLAYER_BUFFER_SIZE);

	if (!lua_isfunction(L, 1))
		return luaL_typerror(L, 1, "function");

	/* create and init new userdata */
	PlayerInfo* pi = (PlayerInfo*)lua_newuserdata(L, sizeof(PlayerInfo));
	pi->channels   = channels;
	pi->pos        = 0;
	pi->samplerate = samplerate;
	PaError err = Pa_OpenDefaultStream(&(pi->stream), 0, channels, paFloat32,
			samplerate, size_buffer, pa_stream_callback, pi);

	if (err != paNoError) {
		pi->stream = NULL;
		return luaL_error(L, "Cannot create player stream: %s\n", Pa_GetErrorText(err));
	}

	/* push function to the new state */
	pi->L = luaL_newstate();
	lua_atpanic(pi->L, &l_panic);
	luaL_openlibs(pi->L);

	lua_pushvalue(L, 1);
	l_move_values(L, pi->L, 1);

	lua_createtable(pi->L, size_buffer * channels, 0); /* out */
	for (size_t i = 1; i <= size_buffer * channels; ++i) {
		lua_pushnumber(pi->L, .0);
		lua_rawseti(pi->L, -2, i);
	}

	/* set metatable for userdata */
	if (luaL_newmetatable(L, "lhc.player.player")) {
		lua_pushcfunction(L, &l_player_play);
		lua_setfield(L, -2, "play");

		lua_pushcfunction(L, &l_player_stop);
		lua_setfield(L, -2, "stop");

		lua_pushcfunction(L, &l_player_seek);
		lua_setfield(L, -2, "seek");

		lua_pushcfunction(L, &l_player_load);
		lua_setfield(L, -2, "cpuload");

		lua_pushcfunction(L, &l_player_gc);
		lua_setfield(L, -2, "__gc");

		lua_pushcfunction(L, &l_player_index);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);

	return 1;
}


/* GC-function of the PA cleanup object created in luaopen_player */
static int l_player_cleanup(lua_State* L)
{
	(void)L;
	PaError err = Pa_Terminate();
	if (paNoError != err)
		fprintf(stderr, "Unable to properly terminate PortAudio: %s\n", Pa_GetErrorText(err));
	return 0;
}

int luaopen_player(lua_State* L)
{
	PaError err = Pa_Initialize();
	if (paNoError != err)
		return luaL_error(L, "Cannot open PortAudio: %s\n", Pa_GetErrorText(err));

	/* create dummy object that shuts down PA upon destroy */
	lua_newuserdata(L, 0);
	luaL_newmetatable(L, "lhc.player.cleanup");
	lua_pushcfunction(L, &l_player_cleanup);
	lua_setfield(L, -2, "__gc");
	lua_setmetatable(L, -2);
	lua_setfield(L, LUA_REGISTRYINDEX, "lhc.player.cleanup.object");

	/* the module */
	lua_createtable(L, 0, 1);
	lua_pushcfunction(L, &l_player_new);
	lua_setfield(L, -2, "new");

	return 1;
}
