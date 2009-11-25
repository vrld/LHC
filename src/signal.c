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

typedef struct Signal__
{
    ALuint buffer;
    ALuint source;
    int is_generated;
    /* TODO: player thread */
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

/* TODO: error checking on OpenAL functions */
/*
 * create sample from closure and play it
 */
static int signal_play(lua_State* L)
{
    double rate, length, value, t;
    size_t bytes, sample_count, i;
    PCMSample* samples;
    Signal* signal = signal_checkudata(L, 1);

    if (lua_getmetatable(L, 1) == 0)
        return luaL_error(L, "signal argument expected in signal.play!");

    if (!signal->is_generated) {
        lua_getglobal(L, "defaults");
        lua_getfield(L, -1, "samplerate");
        rate = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, -1, "length");
        length = luaL_checknumber(L, -1) * 2; /* Nyquist-Shannon sampling theorem */
        lua_pop(L, 2);

        /* TODO: continus play by double buffering - do so by starting a new thread */
        sample_count = rate * length;
        samples = malloc(sample_count * sizeof(PCMSample));
        if (samples == 0) {
            lua_close(L);
            alutExit();
            fprintf(stderr, "Cannot create sample of %lu bytes", sample_count * sizeof(PCMSample));
            exit(1);
        }

        /* TODO: Stereo signal */
        /* create samples */
        for (i = 0; i < sample_count; ++i) {
            lua_getfield(L, 2, "signal");
            lua_pushnumber(L, i / rate);
            lua_call(L, 1, 1);
            value = lua_tonumber(L, -1);
            lua_pop(L, 1);
            if (value > 1.) value = 1.;
            else if (value < -1.) value = -1.;
            samples[i] = value * INTERVAL;
        }
        lua_pop(L, 1);

        /* play samples */
        alGenBuffers(1, &(signal->buffer));
        alGenSources(1, &(signal->source));
        alBufferData(signal->buffer, AL_FORMAT_MONO16, samples, sample_count, rate);
        signal->is_generated = 1;
        free(samples);
        alSourcei(signal->source, AL_BUFFER, (ALint)signal->buffer);
    }
    lua_pop(L, 1);
    alSourcePlay(signal->source);

    return 0;
}

/*
 * stop playing of source
 */
static int signal_stop(lua_State *L)
{
    Signal* signal = signal_checkudata(L, 1);
    alSourceStop(signal->source);
    return 0;
}

/*
 * stop playing and delete OpenAL resources
 */
static int signal_gc(lua_State *L)
{
    Signal* signal = lua_touserdata(L, 1);

    alSourceStop(signal->source);
    alSourcei(signal->source, AL_BUFFER, 0);

    alDeleteBuffers(1, &(signal->buffer));
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

static int signal_mul_number_signal_closure(lua_State *L)
{
    MAKE_NUMBER_SIGNAL_ARIT_CLOSURE(*);
}

static int signal_mul_signal_signal_closure(lua_State *L)
{
    MAKE_SIGNAL_SIGNAL_ARIT_CLOSURE(*);
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
    lua_gettable(L, -2);
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
    lua_insert(L, -2);
    lua_setmetatable(L, -2);
    s->is_generated = 0;
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
