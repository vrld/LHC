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

#include "generators.h"
#include "signal.h"
#include "signal_filters.h"
#include "signal_tools.h"
#include "thread.h"
#include "commandline.h"
#include "pa_assert.h"
#include "timer.h"

lhc_mutex lock_lua_state;

int get_command(lua_State* L);
void* schedule_timer_thread(void* udata);

int main()
{
    int error;
    lhc_thread timer_thread;

    lhc_mutex_init(&lock_lua_state);

    lua_State *L = luaL_newstate(); /* worker (signals, timer, ...) */
    lua_State *L_input = luaL_newstate(); /* input parser */

    luaL_openlibs(L);
    lua_cpcall(L, luaopen_generators, NULL);
    lua_cpcall(L, luaopen_signal, NULL);
    lua_cpcall(L, luaopen_signal_filter, NULL);
    lua_cpcall(L, luaopen_signal_tools, NULL);
    lua_pushcfunction(L, &l_time);
    lua_setglobal(L, "time");

    const char* lhc_std =
    "defaults = { "
    "     generator = gen.sin, "
    "     freq = 440, "
    "} "
    "signals = { "
    "    threads = {}, "
    "    stop = function() "
    "        for _s_, _ in pairs(signals.threads) do "
    "            _s_:stop() "
    "        end "
    "    end, "
    "    clear = function() "
    "        for _s_, _ in pairs(signals.threads) do "
    "            _s_:stop() "
    "            signals.threads[_s_] = nil "
    "        end "
    "    end "
    "} "
    "timer = {} "
    "setmetatable(timer, {__index = { "
    "    new = function(eta, func) "
    "        timer[#timer+1] = {eta + time(), func} "
    "        return timer[#timer] "
    "    end "
    "}})";
    luaL_loadbuffer(L, lhc_std, strlen(lhc_std), "lhc std");
    lua_pcall(L, 0,0,0);


    PA_ASSERT_CMD(Pa_Initialize());
    /* run commandline */
    while (get_command(L_input)) 
    {
        lhc_mutex_lock(&lock_lua_state);
   
        error = luaL_loadbuffer(L, lua_tostring(L_input, 1), lua_strlen(L_input, 1), "input");
        if (error)
            fprintf(stderr, "/o\\ OH NOES! /o\\ %s\n", lua_tostring(L, -1));
        error = lua_pcall(L, 0,0,0);
        if (error)
            fprintf(stderr, "/o\\ OH NOES! /o\\ %s\n", lua_tostring(L, -1));

        /* start timer thread if necessary */
        lua_getglobal(L, "timer");
        if (lua_objlen(L, -1) > 0) {
            error = lhc_thread_create(&timer_thread, schedule_timer_thread, L);
            if (error)
                fprintf(stderr, "There is no time! (cannot start timer scheduler)\n");
        }
        lua_pop(L, 1);
    
        lhc_mutex_unlock(&lock_lua_state);
    }

    lua_close(L);
    PA_ASSERT_CMD(Pa_Terminate());

    return 0;
}

/* from the lua interpreter */
static int incomplete(lua_State* L, int status)
{
    if (status == LUA_ERRSYNTAX) {
        size_t lmsg;
        const char *msg = lua_tolstring(L, -1, &lmsg);
        const char *tp = msg + lmsg - (sizeof("'<eof>'") - 1);
        if (strstr(msg, "'<eof>'") == tp) {
            lua_pop(L, 1);
            return 1;
        }
    }
    return 0;  /* else... */
}

int get_command(lua_State* L)
{
    static char in_buffer[LHC_CMDLINE_INPUT_BUFFER_SIZE];
    static char *input = in_buffer;
    static const char* prompt1 = ">> ";
    static const char* prompt2 = ".. ";
    const char* prompt = prompt1;
#ifdef WITH_GNU_READLINE
    rl_bind_key ('\t', rl_insert); /* TODO: tab completion on globals? */
#endif

    int status;

    lua_settop(L, 0);
    while (read_line(input, prompt))
    {
        if (strstr(input, "exit") == input)
            return 0;

        save_line(input);

        /* multiline input -- take a loo at the original lua interpreter */
        lua_pushstring(L, input);
        if (lua_gettop(L) > 1)
        {
            lua_pushliteral(L, "\n");
            lua_insert(L, -2);
            lua_concat(L, 3);
        }

        status = luaL_loadbuffer(L, lua_tostring(L, 1), lua_strlen(L, 1), "=stdin");
        prompt = prompt2;
        if (!incomplete(L, status)) 
            return 1;
    }
    return 0;
}

void* schedule_timer_thread(void* udata)
{
    lua_State* L = (lua_State*)udata;
    int status, timer_active;
    do {
        timer_active = 0;
        lhc_mutex_lock(&lock_lua_state);
        lua_getglobal(L, "timer");
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) 
        {
            if (!lua_istable(L, -1)) {
                lhc_mutex_unlock(&lock_lua_state);
                luaL_error(L, "Don't mess with me timers! This is only for tables!");
                return NULL; /* never reached! */
            }

            timer_active = 1;
            lua_rawgeti(L, -1, 1);
            if ((long long)lua_tonumber(L, -1) <= time_ms()) 
            {
                lua_rawgeti(L, -2, 2);
                status = lua_pcall(L, 0, 0, 0);
                if (status != 0) {
                    fprintf(stderr, "\rI accidentally the whole timer: %s\n", lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
                /* remove timed function */
                lua_pushvalue(L, -3);
                lua_pushnil(L);
                lua_rawset(L, -6);
            }
            lua_pop(L, 2);
        }
        lua_pop(L, 1);
        lhc_mutex_unlock(&lock_lua_state);
    } while (timer_active);

    return NULL;
}
