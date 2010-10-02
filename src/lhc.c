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
#include <stdio.h>
#include <stdlib.h>
#include <portaudio.h>

#include "commandline.h"
#include "sounddata.h"
#include "soundfile.h"
#include "spectrum.h"
#include "player.h"

static int get_command(lua_State* L);
static int incomplete(lua_State* L);

int main()
{
	int error;

	lua_State *L = luaL_newstate();

	luaL_openlibs(L);
	lua_cpcall(L, luaopen_sounddata, NULL);
	lua_cpcall(L, luaopen_soundfile, NULL);
	lua_cpcall(L, luaopen_spectrum, NULL);
	lua_cpcall(L, luaopen_player, NULL);

	luaL_getmetatable(L, "lhc.SoundData");
	lua_setglobal(L, "SD_meta");
	error = luaL_dofile(L, "stdlib.lua");
	if (error) {
		fprintf(stderr, "error opening stdlib: %s\n", lua_tostring(L, -1));
		return -1;
	}
	lua_pushnil(L);
	lua_setglobal(L, "SD_meta");

	PaError pa_error = Pa_Initialize();
	if (pa_error != paNoError) {
		fprintf(stderr, "Cannot initialize PortAudio: %s\n", Pa_GetErrorText(pa_error));
		return -1;
	}

	/* run commandline */
	while (get_command(L))
	{
		error = luaL_loadbuffer(L, lua_tostring(L, 1), lua_strlen(L, 1), "input");
		if (error) {
			fprintf(stderr, "syntax-error: %s\n", lua_tostring(L, -1));
			continue;
		}
		error = lua_pcall(L, 0,0,0);
		if (error)
			fprintf(stderr, "%s\n", lua_tostring(L, -1));
	}

	lua_close(L);
	pa_error = Pa_Terminate();
	if (pa_error != paNoError) {
		fprintf(stderr, "Cannot terminate PortAudio: %s\n", Pa_GetErrorText(pa_error));
		return -1;
	}

	return 0;
}

/* from the lua interpreter */
static int incomplete(lua_State* L)
{
	int status = luaL_loadbuffer(L, lua_tostring(L, 1), lua_strlen(L, 1), "=stdin");
	if (status == LUA_ERRSYNTAX) {
		size_t lmsg;
		const char *msg = lua_tolstring(L, -1, &lmsg);
		const char *tp = msg + lmsg - (sizeof("'<eof>'") - 1);
		if (strstr(msg, "'<eof>'") == tp) {
			lua_pop(L, 1);
			return 1;
		}
	}
	lua_pop(L,1);
	return 0;  /* else... */
}

static int get_command(lua_State* L)
{
	static char in_buffer[LHC_CMDLINE_INPUT_BUFFER_SIZE];
	static char *input = in_buffer;
	static const char* prompt1 = ">> ";
	static const char* prompt2 = ".. ";
	const char* prompt = prompt1;
#ifdef WITH_GNU_READLINE
	rl_bind_key ('\t', rl_insert); /* TODO: tab completion on globals? */
#endif

	lua_settop(L, 0);
	while (read_line(input, prompt))
	{
		if (strstr(input, "exit") == input)
			return 0;

		/* multiline input -- take a look at the original lua interpreter */
		lua_pushstring(L, input);
		if (lua_gettop(L) > 1)
		{
			lua_pushliteral(L, "\n");
			lua_insert(L, -2);
			lua_concat(L, 3);
		}

		prompt = prompt2;
		if (!incomplete(L)) {
			save_line( lua_tostring(L, -1) );
			return 1;
		}
	}
	return 0;
}
