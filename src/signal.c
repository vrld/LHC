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
#include "signal.h"
#include <lauxlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <portaudio.h>

#include "thread.h"
#include "config.h"
#include "pa_assert.h"

extern lhc_mutex lock_lua_state;

enum { SIGNAL_UNDEFINED, SIGNAL_PLAYING, SIGNAL_STOPPED };

typedef struct {
    double t;
    lua_State *L;
    float buffers[SAMPLE_BUFFER_COUNT][SAMPLE_BUFFER_SIZE];
    int current_buffer;
    int read_buffer_empty;
    int status;

    lhc_thread thread;
} Signal;

/*
 * returns true if userdata at idx is a signal
 */
static int signal_userdata_is_signal(lua_State* L, int idx)
{
    int is_signal = 0;
    void *p = lua_touserdata(L, idx);
    if (p == NULL)
        return 0;

    if (!lua_getmetatable(L, idx))
        return 0;

    lua_getfield(L, LUA_REGISTRYINDEX, "lhc.signal");
    is_signal = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    return is_signal;
}

/*
 * returns userdata at index if it is a signal. yields a
 * type error if it is not (never returns!)
 */
static Signal* signal_checkudata(lua_State* L, int idx)
{
    Signal* p = lua_touserdata(L, idx);
    if (p != NULL) {
        if (lua_getmetatable(L, idx)) {
            lua_getfield(L, LUA_REGISTRYINDEX, "lhc.signal");
            if (lua_rawequal(L, -1, -2)) {
                lua_pop(L, 2);
                return p;
            }
        }
    }
    luaL_typerror(L, idx, "signal");
    return NULL;
}

/*
 * replaces userdata at given index with associated
 * signal closure. does NO error checking!
 */
#define signal_replace_udata_with_closure(L, idx)   \
    lua_pushvalue(L, idx);                          \
    lua_gettable(L, LUA_REGISTRYINDEX);             \
    if (idx != -1 && idx != lua_gettop(L)) lua_replace(L, idx)

/*
 * generates SAMPLE_BUFFER_SIZE next values.
 * arguments are current time and sample rate
 * returns table with values and new time
 */
static int signal_closure(lua_State *L)
{
    double t = luaL_checknumber(L, 1);
    double rate = luaL_checknumber(L, 2);
    double timestep = 1. / rate;
    double freq;
    size_t i;

    lua_createtable(L, SAMPLE_BUFFER_SIZE, 0);
    for (i = 1; i <= SAMPLE_BUFFER_SIZE; ++i, t += timestep) 
    {
        lua_pushvalue(L, lua_upvalueindex(2));
        if (lua_isfunction(L, -1)) {
            lua_pushnumber(L, t);
            lua_call(L, 1, 1);
        }
        freq = lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_pushvalue(L, lua_upvalueindex(1));
        lua_pushnumber(L, t * freq);
        lua_call(L, 1, 1);
        lua_rawseti(L, -2, i);
    }
    lua_pushnumber(L, t);

    return 2; /* table and new time */
}

/*
 * creates 3 functions:
 *  signal_[name]_closure         combines 2 signals using OP
 *  signal_[name]_number_closure  combines signal and number using OP
 *  signal_[name]                 inspects arguments on the stack and
 *                                creates signal with corrosponding closure
 *  only signal_[name] will be exported to lua
 */
#define SIGNAL_OPERATOR(name, OP)                                \
static int signal_##name##_closure(lua_State* L)                 \
{                                                                \
    double t = luaL_checknumber(L, 1);                           \
    double rate = luaL_checknumber(L, 2);                        \
    double val;                                                  \
    size_t i;                                                    \
                                                                 \
    /* call signal 1, omit returned new t */                     \
    lua_pushvalue(L, lua_upvalueindex(1));                       \
    lua_pushnumber(L, t);                                        \
    lua_pushnumber(L, rate);                                     \
    lua_call(L, 2, 1);                                           \
                                                                 \
    /* call signal 2 */                                          \
    lua_pushvalue(L, lua_upvalueindex(2));                       \
    lua_pushnumber(L, t);                                        \
    lua_pushnumber(L, rate);                                     \
    lua_call(L, 2, 2);                                           \
                                                                 \
    /* for i=1,N do tbl2[i] = tbl1[i] <OP> tbl2[i] end */        \
    for (i = 1; i <= SAMPLE_BUFFER_SIZE; ++i)                    \
    {                                                            \
        lua_rawgeti(L, -3, i);                                   \
        val = lua_tonumber(L, -1);                               \
        lua_rawgeti(L, -3, i);                                   \
        val OP##= lua_tonumber(L, -1);                           \
        lua_pop(L, 2); /* remove signal1 and signal2 values*/    \
                                                                 \
        if (val >  1.) val =  1.;                                \
        if (val < -1.) val = -1.;                                \
        lua_pushnumber(L, val);                                  \
        lua_rawseti(L, -3, i);                                   \
    }                                                            \
    /* new values in second table, t_new is already there */     \
    return 2;                                                    \
}                                                                \
                                                                 \
static int signal_##name##_number_closure(lua_State* L)          \
{                                                                \
    double t = luaL_checknumber(L, 1);                           \
    double rate = luaL_checknumber(L, 2);                        \
    double val, c;                                               \
    size_t i;                                                    \
                                                                 \
    c = lua_tonumber(L, lua_upvalueindex(1));                    \
                                                                 \
    /* call signal */                                            \
    lua_pushvalue(L, lua_upvalueindex(2));                       \
    lua_pushnumber(L, t);                                        \
    lua_pushnumber(L, rate);                                     \
    lua_call(L, 2, 2);                                           \
                                                                 \
    /* for i=1,N do tbl[i] = tbl[i] + c end */                   \
    for (i = 1; i <= SAMPLE_BUFFER_SIZE; ++i)                    \
    {                                                            \
        lua_rawgeti(L, -2, i);                                   \
        val = lua_tonumber(L, -1) OP c;                          \
        lua_pop(L, 1);                                           \
                                                                 \
        if (val >  1.) val =  1.;                                \
        if (val < -1.) val = -1.;                                \
        lua_pushnumber(L, val);                                  \
        lua_rawseti(L, -3, i);                                   \
    }                                                            \
    /* new values in second table, t_new is already there */     \
    return 2;                                                    \
}                                                                \
                                                                 \
static int signal_##name(lua_State *L)                           \
{                                                                \
    lua_settop(L, 2);                                            \
    if (lua_isnumber(L, 2)) /* swap 1st with 2nd element */      \
        lua_insert(L, 1);                                        \
                                                                 \
    if (!signal_userdata_is_signal(L, 2))                        \
        return luaL_typerror(L, 2, "signal");                    \
    signal_replace_udata_with_closure(L, 2);                     \
                                                                 \
    if (lua_isnumber(L, 1)) {                                    \
        lua_pushcclosure(L, &signal_##name##_number_closure, 2); \
    } else if (signal_userdata_is_signal(L, 1)) {                \
        signal_replace_udata_with_closure(L, 1);                 \
        lua_pushcclosure(L, &signal_##name##_closure, 2);        \
    } else {                                                     \
        return luaL_typerror(L, 1, "signal or number");          \
    }                                                            \
    signal_new_from_closure(L);                                  \
    return 1;                                                    \
}

static void signal_new_from_closure(lua_State *L);
SIGNAL_OPERATOR(add, +);
SIGNAL_OPERATOR(mul, *);


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

    lua_State* L = s->L;
    CRITICAL_SECTION(&lock_lua_state) 
    {
        lua_getglobal(L, "defaults");
        lua_getfield(L, -1, "samplerate");
        rate = luaL_checknumber(L, -1);
        lua_settop(L, 1);
    }

    /* initially fill all buffers */
    for (i = 0; i < SAMPLE_BUFFER_COUNT; ++i) 
    {
        buffer = s->buffers[i];

        CRITICAL_SECTION(&lock_lua_state) 
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
                buffer[k-1] = lua_tonumber(L, -1);
                lua_pop(L, 1);
            }
            s->t = lua_tonumber(L, -1);
            lua_pop(L, 2);
        }
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
            s->read_buffer_empty = 0;
            s->current_buffer = (s->current_buffer + 1) % SAMPLE_BUFFER_COUNT;

            CRITICAL_SECTION(&lock_lua_state) 
            {
                lua_pushvalue(L, 1);
                signal_replace_udata_with_closure(L, -1);
                lua_pushnumber(L, s->t);
                lua_pushnumber(L, rate);
                lua_call(L, 2, 2);
                for (k = 1; k <= SAMPLE_BUFFER_SIZE; ++k) 
                {
                    lua_rawgeti(L, -2, k);
                    buffer[k-1] = lua_tonumber(L, -1);
                    lua_pop(L, 1);
                }
                s->t = lua_tonumber(L, -1);
            }
        }
        lhc_thread_yield();
    }

    PA_ASSERT_CMD(Pa_StopStream(stream));
    PA_ASSERT_CMD(Pa_CloseStream(stream));
    s->status = SIGNAL_UNDEFINED;

    return NULL;
}

static int signal_play(lua_State *L)
{
    Signal *s = signal_checkudata(L, 1);
    
    if (s->status != SIGNAL_UNDEFINED)
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

static int signal_stop(lua_State *L)
{
    Signal *s = signal_checkudata(L, 1);
    if (s->status == SIGNAL_PLAYING) {
        s->status = SIGNAL_STOPPED;
        s->t = 0;
        lhc_thread_join(s->thread, NULL);
    }
    return 0;
}

static int signal_gc(lua_State *L)
{
    Signal *s = signal_checkudata(L, 1);
    if (s->status == SIGNAL_PLAYING) {
        s->status = SIGNAL_STOPPED;
        lhc_thread_join(s->thread, NULL);
        /* TODO: more work here? if not, delete this and use signal_stop instead */
    }

    return 0;
}

#define SET_FUNCTION_FIELD(L, func, name) lua_pushcfunction(L, func); lua_setfield(L, -2, name)
/* 
 * expects a C-Closure on top of the stack.
 * leaves the associated userdata on the stack.
 */
static void signal_new_from_closure(lua_State *L)
{
    Signal *s = lua_newuserdata(L, sizeof(Signal));
    s->t = 0;
    s->read_buffer_empty = 1;
    s->status = SIGNAL_UNDEFINED;

    /* lua_settable() needs stack to be '... udata udata closure' */
    lua_pushvalue(L, -1);
    lua_pushvalue(L, -3);
    lua_remove(L, -4);
    lua_settable(L, LUA_REGISTRYINDEX); /* registry[udata] = closure */

    /* set metatable */
    if (luaL_newmetatable(L, "lhc.signal")) 
    {
        SET_FUNCTION_FIELD(L, signal_gc, "__gc");
        SET_FUNCTION_FIELD(L, signal_add, "__add");
        SET_FUNCTION_FIELD(L, signal_mul, "__mul");
        SET_FUNCTION_FIELD(L, signal_play, "play");
        SET_FUNCTION_FIELD(L, signal_stop, "stop");
        /* set metatable as index table */
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);

    /* userdata is left on the stack */
}

/*
 * looks for tbl[tbl_name] where tbl is the value at the given index
 * if the value is not a number or a function, get defaults[def_name]
 */
static void push_arg_or_default(lua_State* L, int index, const char* tbl_name, const char* def_name)
{
    lua_getfield(L, index, tbl_name);
    if (!lua_isnumber(L, -1) && !lua_isfunction(L, -1)) 
    {
        lua_pop(L, 1);
        lua_getglobal(L, "defaults");
        lua_getfield(L, -1, def_name);
        lua_remove(L, -2);
        if (lua_isnil(L, -1)) 
            luaL_error(L, "I hate it when that happens!");
    }
}

/*
 * create new signal userdata
 */
static int l_signal(lua_State* L)
{
    if (!lua_istable(L, 1))
        return luaL_error(L, "expected table argument");

    lua_rawgeti(L, 1, 1);
    if (!lua_isfunction(L, -1))
        return luaL_error(L, "generator has to be a function");

    push_arg_or_default(L, 1, "f", "freq");
    /* stack contains: table generator freq */
    lua_pushcclosure(L, &signal_closure, 2);
    signal_new_from_closure(L);

    /* remove argument table */
    lua_remove(L, -2);
    return 1;
}

/* 
 * registers signal function with lua 
 */
int luaopen_signal(lua_State *L)
{
    lua_register(L, "sig", l_signal);
    return 0;
}
