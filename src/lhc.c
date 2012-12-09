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

#include "buffer.h"
#include "player.h"
#include "soundfile.h"

static int lhc_play(lua_State *L)
{
	lhc_player_new(L);
	lua_replace(L, 1);
	lua_settop(L, 1);
	return lhc_player_play(L);
}

int luaopen_lhc(lua_State *L)
{
	lua_createtable(L, 0, 4);

	luaopen_lhc_buffer(L);
	lua_setfield(L, -2, "buffer");

	luaopen_lhc_player(L);
	lua_setfield(L, -2, "player");

	lua_pushcfunction(L, lhc_play);
	lua_setfield(L, -2, "play");

	luaopen_lhc_soundfile(L);
	lua_setfield(L, -2, "soundfile");

	return 1;
}
