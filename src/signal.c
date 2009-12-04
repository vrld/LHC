/*********************************************************************
 *  This file is part of LHC
 *
 *  Copyright (c) 2009 Matthias Richter
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
#include <math.h>
#include <AL/alut.h>
#include <assert.h>

#include "thread.h"
#include "config.h"

extern lhc_mutex lock_lua_state;

enum { SIGNAL_PLAYING = 0, SIGNAL_STOPPED, SIGNAL_UNDEFINED };

typedef struct Signal__
{
    ALuint buffers[SAMPLE_BUFFER_COUNT];
    ALuint source;
    lhc_thread thread;
    lua_State *L;
    int status;
    size_t time;
} Signal;

typedef short PCMSample;

/* Maximum/Minumum value of the PCM sample */
static const int INTERVAL = (1 << (8 * sizeof(PCMSample) - 1)) - 1;

static void signal_new_from_closure(lua_State *L);

/*
 * return true if stack value at index is a signal userdata
 */
static int signal_userdata_is_signal(lua_State* L, int index)
{
    int is_signal = 0;
    void* p = lua_touserdata(L, index);
    if (p == NULL)
        return 0;

    if (!lua_getmetatable(L, index))
        return 0;

    lua_getfield(L, -1, "__index");
    lua_getfield(L, LUA_REGISTRYINDEX, "lhc.signal");
    is_signal = lua_rawequal(L, -1, -2);
    lua_pop(L, 3);
    return is_signal;
}

/*
 * check if stack-value at index is a signal userdata and return it
 * does a luaL_typerror on failure (which does not return!)
 */
static Signal* signal_checkudata(lua_State *L, int index)
{
    Signal* p = lua_touserdata(L, index);
    if (p != NULL) {
        if (lua_getmetatable(L, index)) {
            lua_getfield(L, -1, "__index");
            lua_getfield(L, LUA_REGISTRYINDEX, "lhc.signal");
            if (lua_rawequal(L, -1, -2)) {
                lua_pop(L, 3);
                return p;
            }
        }
    }
    luaL_typerror(L, index, "signal");
    return NULL;
}

#define GET_FUNCTION_OR_NUMBER(var, L, uvindex, t)                  \
    lua_pushvalue(L, lua_upvalueindex( (uvindex) ));            \
    if (!lua_isnumber(L, lua_upvalueindex( (uvindex) ))) {      \
        lua_pushnumber(L, (t) );                                \
        lua_call(L, 1, 1);                                      \
    }                                                           \
    var = lua_tonumber(L, -1);                                  \
    lua_pop(L, 1)
/*
 * signal closure for generators - basic signals
 */
static int signal_closure(lua_State *L)
{
    double t = luaL_checknumber(L, -1);
    /* TODO:freq, amp, phase may also be functions! */
    /* TODO: Stereo signal - channel upvalue */
    double freq, amp, phase; /* channel */

    GET_FUNCTION_OR_NUMBER(freq,  L, 2, t);
    GET_FUNCTION_OR_NUMBER(amp,   L, 3, t);
    GET_FUNCTION_OR_NUMBER(phase, L, 4, t);

    /* call generator function */
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushnumber(L, t * freq + phase);
    lua_call(L, 1, 1);
    if (lua_isnil(L, -1))
        return luaL_error(L, "Generator returned 'nil' when number was expected.");

    t = lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_pushnumber(L, t * amp);
    return 1;
}

/*
 * replaces signal at arg with its closure (at meta(arg).signal)
 */
static void signal_replace_udata_with_closure(lua_State* L, int arg)
{
    /* check if item at arg is a signal and put its metatable on the stack */
    if (!lua_getmetatable(L, arg)) {
        luaL_typerror(L, arg, "signal");
        return; /* just to clarify that we stop here */
    }

    lua_getfield(L, -1, "__index");
    lua_getfield(L, LUA_REGISTRYINDEX, "lhc.signal");
    if (!lua_rawequal(L, -1, -2)) {
        luaL_typerror(L, arg, "signal");
        return;
    }
    lua_pop(L, 2);

    /* get signal function */
    lua_getfield(L, -1, "signal");
    lua_remove(L, -2); /* remove metatable */
    if (arg < 0) --arg;
    lua_replace(L, arg);
}

static int check_al_error(int line)
{
    int error = alGetError();

    if (error != AL_NO_ERROR) {
        fprintf(stderr, "OpenAL error in signal.c:%d: (%d) ", line, error);
        switch (error) {
            case AL_INVALID_NAME: printf("invalid name\n"); break;
            case AL_INVALID_ENUM: printf("invalid enum\n"); break;
            case AL_INVALID_VALUE: printf("invalid value\n"); break;
            case AL_INVALID_OPERATION: printf("invalid operation\n"); break;
            case AL_OUT_OF_MEMORY: printf("out of memory\n"); break;
            default: printf("I don't know\n");
        }
    }
    return error;
}
#define CHECK_AL_ERROR() check_al_error(__LINE__ - 1)

static void signal_fill_buffer(Signal* s, lua_State* L, size_t buf, double rate)
{
    size_t i;
    double value;
    PCMSample samples[SAMPLE_CHUNK_LENGTH];
    CRITICAL_SECTION(&lock_lua_state) 
    {
        for (i = 0; i < SAMPLE_CHUNK_LENGTH; ++i, ++s->time) 
        {
            lua_getfield(L, 1, "signal");
            lua_pushnumber(L, s->time / rate);
            lua_call(L, 1, 1);
            value = lua_tonumber(L, -1);
            lua_pop(L, 1);
            if (value > 1.) value = 1.;
            else if (value < -1.) value = -1.;
            samples[i] = value * INTERVAL;
        }
    }

    alBufferData(buf, AL_FORMAT_MONO16, samples, SAMPLE_CHUNK_LENGTH * sizeof(PCMSample), rate);
    CHECK_AL_ERROR();
}

/*
 * create samples and play them
 */
static void* signal_play_thread(void* arg)
{
    Signal* signal = (Signal*)arg;

    lua_State* L = signal->L;
    ALenum source_status;
    ALuint buffer;
    double rate;
    size_t i;

    CRITICAL_SECTION(&lock_lua_state) 
    {
        lua_getglobal(L, "defaults");
        lua_getfield(L, -1, "samplerate");
        rate = luaL_checknumber(L, -1);
        lua_pop(L, 2);
    }

    alGenBuffers(SAMPLE_BUFFER_COUNT, signal->buffers);
    CHECK_AL_ERROR();
    alGenSources(1, &(signal->source));
    CHECK_AL_ERROR();

    for (i = 0; i < SAMPLE_BUFFER_COUNT; ++i) 
        signal_fill_buffer(signal, L, signal->buffers[i], rate);

    alSourceQueueBuffers(signal->source, SAMPLE_BUFFER_COUNT, signal->buffers);
    CHECK_AL_ERROR();
    alSourcePlay(signal->source);
    CHECK_AL_ERROR();

    while (signal->status == SIGNAL_PLAYING) 
    {
        alGetSourcei(signal->source, AL_BUFFERS_PROCESSED, &source_status);
        CHECK_AL_ERROR();
        if (source_status > 0) 
        {
            alSourceUnqueueBuffers(signal->source, 1, &buffer);
            CHECK_AL_ERROR();
            signal_fill_buffer(signal, L, buffer, rate);
            alSourceQueueBuffers(signal->source, 1, &buffer);
            CHECK_AL_ERROR();
        }

        alGetSourcei(signal->source, AL_SOURCE_STATE, &source_status);
        CHECK_AL_ERROR();
        if (source_status != AL_PLAYING) {
            alSourcePlay(signal->source);
            CHECK_AL_ERROR();
        }

        alGetSourcei(signal->source, AL_BUFFERS_PROCESSED, &source_status);
        CHECK_AL_ERROR();
    }

    return NULL;
}

/*
 * create thread to play sample
 */
static int signal_play(lua_State* L)
{
    Signal* signal = signal_checkudata(L, 1);

    if (signal->status == SIGNAL_PLAYING)
        return 0;

    lua_getglobal(L, "signal_threads");
    lua_pushvalue(L, 1);
    signal->L = lua_newthread(L);
    lua_settable(L, -3); 

    /* move signal metatable to signal stack */
    lua_getmetatable(L, 1);
    lua_xmove(L, signal->L, 1);

    signal->status = SIGNAL_PLAYING;

    if (lhc_thread_create(&(signal->thread), signal_play_thread, signal)) 
    {
        fprintf(stderr, "Cannot start signal thread!\n");
    }

    lua_pop(L, 1);

    return 0;
}

/*
 * stop playing of source
 */
static int signal_stop(lua_State *L)
{
    Signal* signal = signal_checkudata(L, 1);
    if (signal->status != SIGNAL_PLAYING)
        return 0;

    signal->status = SIGNAL_STOPPED;
    lhc_thread_join(signal->thread, NULL);
    alSourceStop(signal->source);
    return 0;
}

/*
 * stop playing and delete OpenAL resources
 */
static int signal_gc(lua_State *L)
{
    Signal* signal = lua_touserdata(L, 1);
    if (signal->status == SIGNAL_PLAYING) 
    {
        signal->status = SIGNAL_STOPPED;
        lhc_thread_join(signal->thread, NULL);

        alSourceStop(signal->source);
        alSourcei(signal->source, AL_BUFFER, 0);
    }

    alDeleteBuffers(SAMPLE_BUFFER_COUNT, signal->buffers);
    alDeleteSources(1, &(signal->source));
    return 0;
}

/* Generic programming magic follows: */
#define MAKE_NUMBER_SIGNAL_ARIT_CLOSURE(OP) \
    double sval = lua_tonumber(L, lua_upvalueindex(1)); \
lua_pushvalue(L, lua_upvalueindex(2));  \
lua_insert(L, 1);                       \
lua_call(L, 1, 1);                      \
sval OP##= lua_tonumber(L, -1);         \
lua_pop(L, 1);                          \
lua_pushnumber(L, sval);                \
return 1                               

#define MAKE_SIGNAL_SIGNAL_ARIT_CLOSURE(OP) \
    double sval;                            \
/* double function argument and put signal functions
 * on stack so we can call them. Stack contents after this:
 * function1 t function2 t
 */                                     \
    lua_pushvalue(L, 1);                    \
    lua_pushvalue(L, lua_upvalueindex(1));  \
    lua_insert(L, -3);                      \
    lua_pushvalue(L, lua_upvalueindex(2));  \
    lua_insert(L, -2);                      \
    \
    lua_call(L, 1, 1);                      \
    sval = lua_tonumber(L, -1);             \
    lua_pop(L, 1);                          \
    \
    lua_call(L, 1, 1);                      \
    sval OP##= lua_tonumber(L, -1);         \
    lua_pop(L,1);                           \
    lua_pushnumber(L, sval);                \
    return 1

static int signal_add_number_signal_closure(lua_State *L)
{
    MAKE_NUMBER_SIGNAL_ARIT_CLOSURE(+);
}

static int signal_add_signal_signal_closure(lua_State *L)
{
    MAKE_SIGNAL_SIGNAL_ARIT_CLOSURE(+);
}

static int signal_mul_number_signal_closure(lua_State *L)
{
    MAKE_NUMBER_SIGNAL_ARIT_CLOSURE(*);
}

static int signal_mul_signal_signal_closure(lua_State *L)
{
    MAKE_SIGNAL_SIGNAL_ARIT_CLOSURE(*);
}

/* 
 * sum of signals. s + s -> s, s + number -> s 
 */
static int signal_add(lua_State *L)
{
    /* if the call was signal + number, swap the two top elements,
     * so it appears the call was number + signal
     */ 
    if (lua_isnumber(L, -1)) {
        lua_insert(L, -2);
    }

    signal_replace_udata_with_closure(L, -1);
    if (lua_isnumber(L, -2)) {
        lua_pushcclosure(L, &signal_add_number_signal_closure, 2);
    } else {
        signal_replace_udata_with_closure(L, -2);
        lua_pushcclosure(L, &signal_add_signal_signal_closure, 2);
    } 

    signal_new_from_closure(L);
    return 1;
}

/*
 * signal - number = signal + (-number)
 */
static int signal_sub(lua_State *L)
{
    double t;
    if (!signal_userdata_is_signal(L, -2))
        return luaL_typerror(L, 1, "signal");

    t = luaL_checknumber(L, -1);
    lua_pushnumber(L, -t);
    lua_replace(L, -2);

    return signal_add(L);
}

/* 
 * multiplication of signals. s * s -> s, s * number -> s 
 */
static int signal_mul(lua_State *L)
{
    /* if the call was signal * number, swap the two top elements,
     * so it appears the call was number * signal
     */ 
    if (lua_isnumber(L, -1)) {
        lua_insert(L, -2);
    }

    signal_replace_udata_with_closure(L, -1);
    if (lua_isnumber(L, -2)) {
        lua_pushcclosure(L, &signal_mul_number_signal_closure, 2);
    } else {
        signal_replace_udata_with_closure(L, -2);
        lua_pushcclosure(L, &signal_mul_signal_signal_closure, 2);
    }
    signal_new_from_closure(L);

    return 1;
}

/* 
 * get table[sname], where table is at index index. 
 * if table[sname] is not a number, get defaults[name].
 */
#define GET_NUMBER_OR_DEFAULT(index, sname, name) do { lua_getfield(L, (index), sname); \
    if (!lua_isnumber(L, -1) && !lua_isfunction(L, -1)) { \
        lua_pop(L, 1); \
        lua_getglobal(L, "defaults"); \
        lua_getfield(L, -1, name); \
        lua_remove(L, -2); \
    } } while(0)

/* 
 * create new signal from table argument:
 * { generator, f = freq, a = amp, p = phase }
 * All arguments but generator are optional
 */
static int l_signal(lua_State *L)
{
    if (!lua_istable(L, -1))
        return luaL_error(L, "error: expected table as argument.");

    lua_pushnumber(L, 1);
    lua_rawget(L, -2);
    if (!lua_isfunction(L, -1))
        return luaL_error(L, "error: expected generator to be a function.");

    GET_NUMBER_OR_DEFAULT(-2, "f", "freq");
    GET_NUMBER_OR_DEFAULT(-3, "a", "amp");
    GET_NUMBER_OR_DEFAULT(-4, "p", "phase");
    lua_pushcclosure(L, &signal_closure, 4); 
    signal_new_from_closure(L);
    /* remove argument table to leave stack as it was before */
    lua_remove(L, -2);
    return 1;
}

/* 
 * transform closure on top of the stack to Signal userdata 
 * with the following metatable:
 * { signal  = closure,
 *   __index = global metatable { play, stop }
 *   __gc    = delete resources,
 *   __add   = signal + signal|number,
 *   __sub   = signal - number,
 *   __mul   = signal * signal|number }
 */
static void signal_new_from_closure(lua_State *L) 
{
    size_t i;
    /* create metatable for userdata and put signal function in it */
    lua_newtable(L);
    lua_insert(L, -2);
    lua_setfield(L, -2, "signal");

    /* arithmetics and destructor */
    lua_pushcfunction(L, signal_gc);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, signal_mul);
    lua_setfield(L, -2, "__mul");
    lua_pushcfunction(L, signal_add);
    lua_setfield(L, -2, "__add");
    lua_pushcfunction(L, signal_sub);
    lua_setfield(L, -2, "__sub");

    /* get global metatable and set as __index for oo-access*/
    if (luaL_newmetatable(L, "lhc.signal")) {
        /* create new table only if needed */
        lua_pushcfunction(L, signal_play);
        lua_setfield(L, -2, "play");
        lua_pushcfunction(L, signal_stop);
        lua_setfield(L, -2, "stop");
    }
    lua_setfield(L, -2, "__index");

    /* finally, create the userdata and set the above table as its meta */
    Signal* s = lua_newuserdata(L, sizeof(Signal));
    for (i = 0; i < SAMPLE_BUFFER_COUNT; ++i)
        s->buffers[i] = 0;
    s->source = 0;
    s->time = 0;
    s->status = SIGNAL_UNDEFINED;
    lua_insert(L, -2);
    lua_setmetatable(L, -2);
}

/* 
 * registers signal function with lua 
 */
int luaopen_signal(lua_State *L)
{
    lua_register(L, "signal", l_signal);
    lua_register(L, "sig", l_signal);
    return 0;
}
