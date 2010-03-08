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
#include "signal_player.h"
#include "signal.h"

#include "thread.h"
#include "pa_assert.h"

#include <lauxlib.h>
#include <portaudio.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern lhc_mutex lock_lua_state;

static int signal_play_buffer(const void *in, void *out,
        unsigned long fpb, const PaStreamCallbackTimeInfo *time,
        PaStreamCallbackFlags flags, void *udata)
{
    Signal *s = (Signal*)udata;
    (void)in;
    (void)flags;
    (void)time;

    assert(fpb == SAMPLE_BUFFER_SIZE);
    memcpy(out, s->buffers[ s->current_buffer ], fpb * sizeof(float));
    s->read_buffer_empty = 1;

    return 0;
}

static void* signal_fill_buffer_thread(void* arg)
{
    unsigned int i, k;
    double rate;
    Signal* s = (Signal*)arg;
    float *buffer;
    PaStream *stream;
    float val;

    lua_State* L = s->L;
    lhc_mutex_lock(&lock_lua_state) ;
    {
        lua_getglobal(L, "defaults");
        lua_getfield(L, -1, "samplerate");
        rate = luaL_checknumber(L, -1);
        lua_settop(L, 1);
    }
    lhc_mutex_unlock(&lock_lua_state) ;

    /* initially fill all buffers */
    for (i = 0; i < SAMPLE_BUFFER_COUNT; ++i) 
    {
        buffer = s->buffers[i];

        lhc_mutex_lock(&lock_lua_state) ;
        {
            lua_pushvalue(L, 1);
            signal_replace_udata_with_closure(L, -1);
            lua_pushnumber(L, s->t);
            lua_pushnumber(L, rate);
            lua_call(L, 2, 2);
            /* lua tables start at index 1, but arrays dont. */
            for (k = 1; k <= SAMPLE_BUFFER_SIZE; ++k) 
            {
                lua_rawgeti(L, -2, k);
                val = lua_tonumber(L, -1);
                if (val > 1.) val = 1.;
                else if (val < -1.) val = -1.;
                buffer[k-1] = val;
                lua_pop(L, 1);
            }
            s->t = lua_tonumber(L, -1);
            lua_pop(L, 2);
        }
        lhc_mutex_unlock(&lock_lua_state) ;
    }
    s->current_buffer = 0;
    s->read_buffer_empty = 0;

    /* start portaudio stream */
    PA_ASSERT_CMD(Pa_OpenDefaultStream(&stream,
                0, 1, paFloat32, rate, SAMPLE_BUFFER_SIZE,
                signal_play_buffer, s));

    PA_ASSERT_CMD(Pa_StartStream(stream));

    /* TODO: this is essentially just double buffering. do more */
    /* fill buffers if needed */
    s->status = SIGNAL_PLAYING;
    while (s->status == SIGNAL_PLAYING) 
    {
        /* see if portaudio has finished reading the buffer */
        if (s->read_buffer_empty) 
        {
            buffer = s->buffers[ s->current_buffer ];
            /* advance to next buffer before filling the empty one */
            s->current_buffer = (s->current_buffer + 1) % SAMPLE_BUFFER_COUNT;
            s->read_buffer_empty = 0;

            lhc_mutex_lock(&lock_lua_state) ;
            {
                lua_pushvalue(L, 1);
                signal_replace_udata_with_closure(L, -1);
                lua_pushnumber(L, s->t);
                lua_pushnumber(L, rate);
                lua_call(L, 2, 2);
                for (k = 1; k <= SAMPLE_BUFFER_SIZE; ++k) 
                {
                    lua_rawgeti(L, -2, k);
                    val = lua_tonumber(L, -1);
                    if (val > 1.) val = 1.;
                    else if (val < -1.) val = -1.;
                    buffer[k-1] = val;
                    lua_pop(L, 1);
                }
                s->t = lua_tonumber(L, -1);
            }
            lhc_mutex_unlock(&lock_lua_state) ;
        }
        lhc_thread_yield();
    }

    PA_ASSERT_CMD(Pa_StopStream(stream));
    PA_ASSERT_CMD(Pa_CloseStream(stream));
    s->status = SIGNAL_STOPPED;

    return NULL;
}

int signal_play(lua_State *L)
{
    Signal *s = signal_checkudata(L, 1);
    
    if (s->status != SIGNAL_STOPPED)
        return 0;

    lua_settop(L, 1);
    lua_getglobal(L, "signals");
    lua_getfield(L, -1, "threads");
    lua_pushvalue(L, 1);
    s->L = lua_newthread(L);
    lua_settable(L, -3);

    /* push userdatum on top of new thread stack */
    lua_pushvalue(L, 1);
    lua_xmove(L, s->L, 1);

    if (lhc_thread_create(&(s->thread), signal_fill_buffer_thread, s))
        fprintf(stderr, "Cannot start signal thread!\n");

    return 0;
}

int signal_stop(lua_State *L)
{
    Signal *s = signal_checkudata(L, 1);
    if (s->status == SIGNAL_PLAYING) {
        s->status = SIGNAL_STOPPED;
        s->t = 0;
        if (s->thread)
            lhc_thread_join(s->thread, NULL);
    }
    return 0;
}
