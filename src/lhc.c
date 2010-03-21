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
#include "thread.h"
#include "commandline.h"
#include "pa_assert.h"
#include "queue.h"
#include "timer.h"

lhc_mutex lock_lua_state;

Queue* command_queue;
int commandline_active = 0;

void* fetch_command(void*);
void schedule_timers(lua_State* L);

int main()
{
    const char* command;
    int error;

    lhc_mutex_init(&lock_lua_state);
    command_queue = queue_init();

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_cpcall(L, luaopen_generators, NULL);
    lua_cpcall(L, luaopen_signal, NULL);
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
    "timer = { "
    "    new = function(eta, func) "
    "        timer[#timer+1] = {eta + time(), func} "
    "    end "
    "}";
    luaL_loadbuffer(L, lhc_std, strlen(lhc_std), "lhc std");
    lua_pcall(L, 0,0,0);


    /* run commandline */
    commandline_active = 1;
    lhc_thread cmdline_thread;
    if (lhc_thread_create(&cmdline_thread, fetch_command, NULL))
    {
        lua_close(L);
        fprintf(stderr, "Cannot start commandline thread!");
        return 1;
    }

    PA_ASSERT_CMD(Pa_Initialize());
    while (commandline_active) 
    {
        if (!queue_empty(command_queue))
        {
            command = queue_front_nocopy_dont_use_this_if_not_sure(command_queue);
            lhc_mutex_lock(&lock_lua_state);
            {
                error = luaL_loadbuffer(L, command, strlen(command), "input") || lua_pcall(L, 0,0,0);
                if (error)
                {
                    fprintf(stderr, "\r/o\\ OH NOES! /o\\ %s\n", lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            }
            lhc_mutex_unlock(&lock_lua_state);
            queue_pop(command_queue);
        }

        schedule_timers(L);
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

void* fetch_command(void* _)
{
    (void)_;
    char in_buffer[LHC_CMDLINE_INPUT_BUFFER_SIZE];
    char *input = in_buffer;
    const char* prompt1 = ">> ";
    const char* prompt2 = ".. ";
    const char* prompt = prompt1;
#ifdef WITH_GNU_READLINE
    rl_bind_key ('\t', rl_insert); /* TODO: tab completion on globals? */
#endif

    /* create new state for testing multiline input */
    lua_State* L = luaL_newstate();
    int status;

    lua_pushliteral(L, "");
    while (read_line(input, prompt))
    {
        if (strstr(input, "exit") == input)
            break;
        if (input[0] == '\0')
            continue;
        save_line(input);

        /* multiline input -- take a loo at the original lua interpreter */
        lua_pushstring(L, input);
        lua_pushliteral(L, "\n");
        lua_insert(L, -2);
        lua_concat(L, 3);
        status = luaL_loadbuffer(L, lua_tostring(L, 1), lua_strlen(L, 1), "=stdin");
        prompt = prompt2;
        if (!incomplete(L, status)) {
            queue_push(command_queue, lua_tostring(L, 1));
            lua_settop(L, 0);
            lua_pushliteral(L, "");
            prompt = prompt1;
        }
    }
    commandline_active = 0;
    return NULL;
}

void schedule_timers(lua_State* L)
{
    int status;
    lhc_mutex_lock(&lock_lua_state);
    lua_getglobal(L, "timer");
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) 
    {
        /* avoid timer.new */
        if (lua_istable(L, -1))
        {
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
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    lhc_mutex_unlock(&lock_lua_state);
}
