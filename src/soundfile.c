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
#include "soundfile.h"
#include "sounddata.h"

#include <sndfile.h>

int l_readFile(lua_State* L)
{
	/* open file and get info */
	const char* filename = luaL_checkstring(L, 1);
	SF_INFO info;
	info.format = 0;
	SNDFILE *file = sf_open(filename, SFM_READ, &info);
	if (file == NULL)
		return luaL_error(L, "cannot open file '%s' (format may not be supported)", filename);

	/* create new sounddata object based on info */
	lua_createtable(L, 0, 3);
	lua_pushnumber(L, info.samplerate);
	lua_setfield(L, -2, "rate");
	lua_pushnumber(L, (double)info.frames / (double)info.samplerate);
	lua_setfield(L, -2, "len");
	lua_pushnumber(L, info.channels);
	lua_setfield(L, -2, "ch");
	l_sounddata_new(L);

	/* read file to sounddata object */
	SoundData* sd = l_sounddata_checksounddata(L, -1);
	assert(sd->sample_count == (size_t)info.frames);

	sf_count_t read_frames = sf_read_float(file, sd->samples, sd->sample_count * sd->channels);
	sf_close(file);
	if ((size_t)read_frames != sd->sample_count * sd->channels) {
		return luaL_error(L, "file read failed (%lu samples requested, %lu samples read)",
				sd->sample_count, read_frames);
	}

	return 1;
}

int l_writeFile(lua_State* L)
{
	return luaL_error(L, "not yet implemented");
}

int luaopen_soundfile(lua_State* L)
{
	lua_register(L, "readFile", l_readFile);
	lua_register(L, "writeFile", l_writeFile);
	return 0;
}
