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
#include <portaudio.h>

#include "generators.h"
#include "signal.h"
#include "thread.h"
#include "commandline.h"
#include "pa_assert.h"
#include "queue.h"

lhc_mutex lock_lua_state;

Queue* command_queue;
int commandline_active = 0;

void exec_file(lua_State* L, const char* file);
void* fetch_command(void*);

#define SET_DEFAULT(field, value) lua_pushnumber(L, (value)); lua_setfield(L, -2, (field))
int main(int argc, char** argv)
{
    const char* command;
    int error;

    lhc_mutex_init(&lock_lua_state);
    command_queue = queue_init();

    lua_State *L = lua_open();
    luaL_openlibs(L);
    lua_cpcall(L, luaopen_generators, NULL);
    lua_cpcall(L, luaopen_signal, NULL);

    lua_createtable(L, 0, 5);
    SET_DEFAULT("samplerate", 44100);
    SET_DEFAULT("freq", 440);
    lua_setglobal(L, "defaults");

    lua_newtable(L);
        lua_newtable(L);
        lua_setfield(L, -2, "threads");

        const char* stop_all_signals = "for _s_, _ in pairs(signals.threads) do"
                                            "_s_:stop() "
                                        "end";
        luaL_loadbuffer(L, stop_all_signals, strlen(stop_all_signals), "signals.stop()");
        lua_setfield(L, -2, "stop");

        const char* clear_all_signals = "for _s_, _ in pairs(signals.threads) do"
                                            "_s_:stop() "
                                            "signals.threads[_s_] = nil "
                                        "end";
        luaL_loadbuffer(L, clear_all_signals, strlen(clear_all_signals), "signals.clear()");
        lua_setfield(L, -2, "clear");
    lua_setglobal(L, "signals");

    /* load file  */
    PA_ASSERT_CMD(Pa_Initialize());
    if (argc > 1)
        exec_file(L, argv[1]);

    commandline_active = 1;
    lhc_thread cmdline_thread;
    if (lhc_thread_create(&cmdline_thread, fetch_command, NULL))
    {
        lua_close(L);
        fprintf(stderr, "Cannot start commandline thread!");
        return 1;
    }

    while (commandline_active) 
    {
        if (!queue_empty(command_queue))
        {
            command = queue_front_nocopy_dont_use_this_if_not_sure(command_queue);
            lhc_mutex_lock(&lock_lua_state);
            {
                error = luaL_loadbuffer(L, command, strlen(command), "line") || lua_pcall(L, 0,0,0);
                if (error)
                {
                    fprintf(stderr, "\rerror: %s\nlhc> ", lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            }
            lhc_mutex_unlock(&lock_lua_state);
            queue_pop(command_queue);
        }

        /* TODO: schedule timers */ 
        lhc_thread_yield();
    }

    lua_close(L);

    PA_ASSERT_CMD(Pa_Terminate());
    return 0;
}

void exec_file(lua_State* L, const char* file)
{
    FILE* f = fopen(file, "r");
    int error = 0;
    if (f != NULL)
    {
        fclose(f);
        lhc_mutex_lock(&lock_lua_state);
        {
            error = luaL_dofile(L, file);
        }
        lhc_mutex_unlock(&lock_lua_state);
        if (error)
            fprintf(stderr, "error: %s\n", lua_tostring(L, -1));
        else
            printf("loaded file '%s'!\n", file);
    }
    else
    {
        fprintf(stderr, "cannot open '%s' for reading!\n", file);
    }
}

void* fetch_command(void* _)
{
    (void)_;
    char in_buffer[LHC_CMDLINE_INPUT_BUFFER_SIZE];
    char *input = in_buffer;
#ifdef WITH_GNU_READLINE
    rl_bind_key ('\t', rl_insert); /* TODO: tab completion on globals */
#endif

    while (read_line(input, "lhc> "))
    {
        if (strstr(input, "exit") == input)
            break;
        if (input[0] != 0 && input[0] != '\n') 
        {
            save_line(input);
            queue_push(command_queue, input);
        }
    }
    /* prevent  while (queue_empty(command_queue))  to run forever
     * and give the user a nice farwell message
     */
    queue_push(command_queue, "print 'Bye'");
    commandline_active = 0;
    return NULL;
}
