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

static const int INTERVAL = (1 << (8 * sizeof(short) - 1)) - 1;

static int signal_closure(lua_State *L)
{
    double t = luaL_checknumber(L, -1);
    double freq  = lua_tonumber(L, lua_upvalueindex(2));
    double amp   = lua_tonumber(L, lua_upvalueindex(3));
    double phase = lua_tonumber(L, lua_upvalueindex(4));
    
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushnumber(L, fmod(t * freq + phase, 1.));
    lua_call(L, 1, 1);
    if (lua_isnil(L, -1))
        return luaL_error(L, "Generator returned 'nil' when number was expected.");

    t = lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_pushnumber(L, t * amp);
    return 1;
}

/* TODO: error checking on OpenAL functions */
int l_play(lua_State* L)
{
    double rate, length, value, t;
    size_t bytes, sample_count, i;
    short* samples;
    Signal* signal = lua_touserdata(L, 1);
    if (signal == NULL)
        return luaL_error(L, "signal argument expected in signal.play!");

    if (lua_getmetatable(L, 1) == 0)
        return luaL_error(L, "signal argument expected in signal.play!");

    if (!signal->is_generated) {
        lua_getglobal(L, "defaults");
        lua_getfield(L, -1, "samplerate");
        rate = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, -1, "length");
        length = luaL_checknumber(L, -1);
        lua_pop(L, 2);

        /* TODO: continus play by double buffering - do so by starting a new thread */
        sample_count = rate * length;
        samples = malloc(sample_count * sizeof(short));
        if (samples == 0) {
            lua_close(L);
            alutExit();
            fprintf(stderr, "Cannot create sample of %lu bytes", sample_count * sizeof(short));
            exit(1);
        }

        /* create samples */
        for (i = 0; i < sample_count; ++i) {
            lua_getfield(L, 2, "signal");
            assert(lua_gettop(L) == 3);
            assert(lua_isfunction(L, -1));
            lua_pushnumber(L, i / rate);
            assert(lua_gettop(L) == 4);
            lua_call(L, 1, 1);
            assert(lua_gettop(L) == 3);
            value = lua_tonumber(L, -1);
            lua_pop(L, 1);
            assert(lua_gettop(L) == 2);
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

int l_stop(lua_State *L)
{
    Signal* signal = lua_touserdata(L, 1);
    if (signal == NULL || lua_getmetatable(L, 1) == 0)
        return luaL_error(L, "signal argument expected in signal.stop!");
    alSourceStop(signal->source);
    return 0;
}

int signal_gc(lua_State *L)
{
    Signal* signal = lua_touserdata(L, 1);

    alSourceStop(signal->source);
    alSourcei(signal->source, AL_BUFFER, 0);
    
    alDeleteBuffers(1, &(signal->buffer));
    alDeleteSources(1, &(signal->source));
    return 0;
}

#define GET_NUMBER_OR_DEFAULT(index, sname, name) do { lua_getfield(L, (index), sname); \
    if (!lua_isnumber(L, -1)) { \
        lua_pop(L, 1); \
        lua_getglobal(L, "defaults"); \
        lua_getfield(L, -1, name); \
        lua_remove(L, -2); \
    } } while(0)

static int l_signal(lua_State *L)
{
    if (!lua_istable(L, 1))
        return luaL_error(L, "error: expected table as argument.");

    lua_pushnumber(L, 1);
    lua_gettable(L, 1);
    if (!lua_isfunction(L, -1))
        return luaL_error(L, "error: expected generator to be a function.");

    GET_NUMBER_OR_DEFAULT(-2, "f", "freq");
    GET_NUMBER_OR_DEFAULT(-3, "a", "amp");
    GET_NUMBER_OR_DEFAULT(-4, "p", "phase");
    /* stack content: args, generator, freq, amp, phase */

    /* create metatable for signal userdatum */
    lua_pushcclosure(L, &signal_closure, 4); 
    lua_newtable(L);
    lua_replace(L, 1); /* stack: table closure */
    lua_setfield(L, 1, "signal"); /* {signal = closure} */
    lua_pushcfunction(L, l_play);
    lua_setfield(L, 1, "play"); /* {signal = closure, play = l_play} */
    lua_pushcfunction(L, l_stop);
    lua_setfield(L, 1, "stop"); /* {signal = closure, play = l_play, stop = l_stop} */
    lua_pushcfunction(L, signal_gc);
    lua_setfield(L, 1, "__gc"); /* {signal = closure, play = l_play, stop = l_stop, __gc = signal_gc } */
    lua_pushvalue(L, 1);
    lua_setfield(L, 1, "__index"); /* this provides OO-access for the userdata */

    Signal* s = lua_newuserdata(L, sizeof(Signal)); /* stack: { signal, play, stop, __gc, __index } Signal */
    lua_insert(L, 1); /* stack: Signal { signal, play, stop, __gc, __index }*/
    lua_setmetatable(L, 1);
    s->is_generated = 0;
    return 1; /* Signal userdata */
}

int luaopen_signal(lua_State *L)
{
    lua_register(L, "signal", l_signal);
    lua_register(L, "sig", l_signal);
    return 0;
}
