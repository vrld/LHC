#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <portaudio.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "reference.h"
#include "inter_stack_tools.h"
#include "mutex.h"
#include "player.h"

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

	mutex_lock(&pi->lock);
	lua_State* L = pi->L;

	/* function callback(out, nsamples, pos, rate, channels) */
	lua_pushvalue(L, 1);
	lua_pushvalue(L, 2);
	lua_pushnumber(L, frames);
	lua_pushnumber(L, pi->pos);
	lua_pushnumber(L, pi->samplerate);
	lua_pushnumber(L, pi->channels);

	if (0 != lua_pcall(L, 5, 0, 0)) {
		fprintf(stderr, "Error calling function: %s\n", lua_tostring(L, -1));
		mutex_unlock(&pi->lock);
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
	mutex_unlock(&pi->lock);

	return paContinue;
}


PlayerInfo* l_checkplayer(lua_State* L, int idx)
{
	Reference* ref = (Reference*)luaL_checkudata(L, idx, reference_names[REF_PLAYER]);
	if (NULL == ref)
		luaL_typerror(L, idx, "Reference");
	if (REF_PLAYER != ref->type)
		luaL_error(L, "Invalid argument %d: Invalid refrence type: %s", idx, reference_names[ref->type]);

	PlayerInfo* pi = (PlayerInfo*)ref->ref;
	if (NULL == pi || 0 >= pi->refcount)
		luaL_error(L, "Invalid argument %d: Invalid reference.", idx);
	if (NULL == pi->stream)
		luaL_error(L, "Invalid argument %d: Stream not properly initialized.", idx);
	return pi;
}

static int l_player_play(lua_State* L)
{
	PlayerInfo* pi = l_checkplayer(L, 1);
	if (NULL == pi->stream)
		return luaL_error(L, "Stream not properly initialized.");

	mutex_lock(&pi->lock);
	PaError err = Pa_StartStream(pi->stream);
	mutex_unlock(&pi->lock);
	if (err != paNoError)
		return luaL_error(L, "Cannot play stream: %s", Pa_GetErrorText(err));

	lua_settop(L, 1);
	return 1;
}

static int l_player_stop(lua_State* L)
{
	PlayerInfo* pi = l_checkplayer(L, 1);
	if (NULL == pi->stream)
		return luaL_error(L, "Stream not properly initialized.");

	mutex_lock(&pi->lock);
	PaError err = Pa_AbortStream(pi->stream);
	mutex_unlock(&pi->lock);
	if (err != paNoError)
		return luaL_error(L, "Cannot stop stream: %s", Pa_GetErrorText(err));

	lua_settop(L, 1);
	return 1;
}

static int l_player_seek(lua_State* L)
{
	PlayerInfo* pi = l_checkplayer(L, 1);
	int pos = luaL_checkint(L, 2);
	PaError err;

	mutex_lock(&pi->lock);
	err = Pa_AbortStream(pi->stream);
	if (err != paNoError) {
		mutex_unlock(&pi->lock);
		return luaL_error(L, "Cannot stop stream: %s", Pa_GetErrorText(err));
	}

	pi->pos = pos;

	err = Pa_StartStream(pi->stream);
	mutex_unlock(&pi->lock);
	if (err != paNoError)
		return luaL_error(L, "Cannot play stream: %s", Pa_GetErrorText(err));

	lua_settop(L, 1);
	return 1;
}

static int l_player_load(lua_State* L)
{
	PlayerInfo* pi = l_checkplayer(L, 1);
	if (NULL == pi->stream)
		return luaL_error(L, "Stream not properly initialized.");
	lua_pushnumber(L, Pa_GetStreamCpuLoad(pi->stream));
	return 1;
}

static int l_player_send(lua_State* L)
{
	PlayerInfo* pi = l_checkplayer(L, 1);
	const char* name = luaL_checkstring(L, 2);
	if (lua_isnone(L, 3))
		return luaL_typerror(L, 3, "any");

	mutex_lock(&pi->lock);
	l_copy_values(L, pi->L, 1);
	lua_setfield(pi->L, LUA_GLOBALSINDEX, name);
	mutex_unlock(&pi->lock);

	lua_settop(L, 1);
	return 1;
}

static int l_player_receive(lua_State* L)
{
	PlayerInfo* pi = l_checkplayer(L, 1);
	const char* name = luaL_checkstring(L, 2);

	mutex_lock(&pi->lock);
	lua_getfield(pi->L, LUA_GLOBALSINDEX, name);
	l_move_values(pi->L, L, 1);
	mutex_unlock(&pi->lock);

	return 1;
}

static int l_player_gc(lua_State* L)
{
	PlayerInfo* pi = l_checkplayer(L, 1);

	if (--pi->refcount <= 0) {
		PaError err = Pa_CloseStream(pi->stream);
		if (err != paNoError)
			fprintf(stderr, "Unable to close player stream: %s\n", Pa_GetErrorText(err));

		mutex_destroy(&pi->lock);
		lua_close(pi->L);
		free(pi);
	}

	return 0;
}

/* check if user queries struct members. otherwise forward to metatable. */
static int l_player_index(lua_State* L)
{
	PlayerInfo* pi = l_checkplayer(L, 1);
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
 * function Player.new(callback, samplerate, channels, buffer_size)
 * function callback(out, nsamples, pos, rate, channels)
 *     ... synthesize! ...
 *     -- postcondition: #out == nSamples
 * end
 */
static int l_player_new(lua_State* L)
{
	double samplerate  = luaL_optnumber(L, 2, 44100);
	int    channels    = luaL_optint(L, 3, 1);
	size_t size_buffer = luaL_optint(L, 4, DEFAULT_PLAYER_BUFFER_SIZE);

	if (!lua_isfunction(L, 1))
		return luaL_typerror(L, 1, "function");

	/* create and init new userdata */
	Reference* ref = (Reference*)lua_newuserdata(L, sizeof(Reference));
	ref->type = REF_PLAYER;

	PlayerInfo* pi = (PlayerInfo*)malloc(sizeof(PlayerInfo));
	pi->refcount   = 1;
	pi->channels   = channels;
	pi->pos        = 0;
	pi->samplerate = samplerate;
	pi->refcount = 1;
	PaError err = Pa_OpenDefaultStream(&(pi->stream), 0, channels, paFloat32,
			samplerate, size_buffer, pa_stream_callback, pi);

	if (err != paNoError) {
		free(pi);
		pi->stream = NULL;
		return luaL_error(L, "Cannot create player stream: %s\n", Pa_GetErrorText(err));
	}

	mutex_init(&pi->lock);

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

	ref->ref = pi;

	/* set metatable for userdata */
	if (luaL_newmetatable(L, reference_names[REF_PLAYER])) {
		lua_pushcfunction(L, &l_player_play);
		lua_setfield(L, -2, "play");

		lua_pushcfunction(L, &l_player_stop);
		lua_setfield(L, -2, "stop");

		lua_pushcfunction(L, &l_player_seek);
		lua_setfield(L, -2, "seek");

		lua_pushcfunction(L, &l_player_load);
		lua_setfield(L, -2, "cpuload");

		lua_pushcfunction(L, &l_player_send);
		lua_setfield(L, -2, "send");

		lua_pushcfunction(L, &l_player_receive);
		lua_setfield(L, -2, "receive");

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
