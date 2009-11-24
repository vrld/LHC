#include "signal.h"
#include <lauxlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <AL/alut.h>
#include <assert.h>

#define GET_NUMBER_OR_DEFAULT(index, sname, name) do { lua_getfield(L, (index), sname); \
    if (!lua_isnumber(L, -1)) { \
        lua_pop(L, 1); \
        lua_getglobal(L, "defaults"); \
        lua_getfield(L, -1, name); \
        lua_remove(L, -2); \
    } } while(0)


typedef struct Signal__
{
    ALuint buffer;
    ALuint source;
    int is_generated;
    /* TODO: player thread */
} Signal;

static const int INTERVAL = (1 << (8 * sizeof(short) - 1)) - 1;

static void signal_new_from_closure(lua_State *L);
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
static int signal_play(lua_State* L)
{
    double rate, length, value, t;
    size_t bytes, sample_count, i;
    short* samples;
    Signal* signal = signal_checkudata(L, 1);

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

static int signal_stop(lua_State *L)
{
    Signal* signal = signal_checkudata(L, 1);
    alSourceStop(signal->source);
    return 0;
}

static int signal_gc(lua_State *L)
{
    Signal* signal = lua_touserdata(L, 1);

    alSourceStop(signal->source);
    alSourcei(signal->source, AL_BUFFER, 0);
    
    alDeleteBuffers(1, &(signal->buffer));
    alDeleteSources(1, &(signal->source));
    return 0;
}

static int signal_add_number_signal_closure(lua_State *L)
{
    double sval = lua_tonumber(L, lua_upvalueindex(1));
    lua_pushvalue(L, lua_upvalueindex(2));
    lua_insert(L, 1);
    lua_call(L, 1, 1);
    sval += lua_tonumber(L, -1);
    lua_pop(L, 1);

    if (sval > 1.)  sval = 1.;
    if (sval < -1.) sval = -1.;
    lua_pushnumber(L, sval);
    return 1;
}

static int signal_add_signal_signal_closure(lua_State *L)
{
    double sval;
    lua_pushvalue(L, 1); /* st: t t */
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_pushvalue(L, lua_upvalueindex(2));
    lua_insert(L, 3); /* st: f1 t f2 t */
    
    lua_call(L, 1, 1);
    sval = lua_tonumber(L, -1); 
    lua_pop(L, 1);

    lua_call(L, 1, 1);
    sval += lua_tonumber(L, -1);
    lua_pop(L,1); 

    if (sval > 1.)  sval = 1.;
    if (sval < -1.) sval = -1.;
    lua_pushnumber(L, sval);
    
    return 1;
}

/* push metatable to stack and check for type */
#define GET_SIGNAL_FUNC(L, arg) \
        if (!lua_getmetatable(L, arg)) \
            return luaL_typerror(L, arg, "signal"); \
        lua_getfield(L, -1, "__index"); \
        lua_getfield(L, LUA_REGISTRYINDEX, "lhc.signal"); \
        if (!lua_rawequal(L, -1, -2)) \
            return luaL_typerror(L, arg, "signal"); \
        lua_pop(L, 2); \
        lua_getfield(L, -1, "signal"); \
        lua_replace(L, arg); \
        lua_pop(L, 1); \

static int signal_add(lua_State *L)
{
    if (lua_isnumber(L, 2)) { /* transform: sig + a -> a + sig */
        lua_insert(L, 1);
    }

    GET_SIGNAL_FUNC(L, 2);
    if (lua_isnumber(L, 1)) {
        lua_pushcclosure(L, &signal_add_number_signal_closure, 2);
    } else {
        GET_SIGNAL_FUNC(L, 1);
        lua_pushcclosure(L, &signal_add_signal_signal_closure, 2);
    } /* stack at this point: closure */

    signal_new_from_closure(L);

    return 1;
}

static int signal_sub(lua_State *L)
{
    double t;
    if (!signal_userdata_is_signal(L,1))
        return luaL_typerror(L, 1, "signal");

    t = luaL_checknumber(L, 2);
    lua_pushnumber(L, -t);
    lua_replace(L, 2);

    return signal_add(L);
}

static int signal_mul_number_signal_closure(lua_State *L)
{
    double sval = lua_tonumber(L, lua_upvalueindex(1));
    lua_pushvalue(L, lua_upvalueindex(2));
    lua_insert(L, 1);
    lua_call(L, 1, 1);
    sval *= lua_tonumber(L, -1);
    lua_pop(L, 1);

    if (sval > 1.)  sval = 1.;
    if (sval < -1.) sval = -1.;
    lua_pushnumber(L, sval);
    return 1;
}

static int signal_mul_signal_signal_closure(lua_State *L)
{
    double sval;
    lua_pushvalue(L, 1); /* st: t t */
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_pushvalue(L, lua_upvalueindex(2));
    lua_insert(L, 3); /* st: f1 t f2 t */
    
    lua_call(L, 1, 1);
    sval = lua_tonumber(L, -1); 
    lua_pop(L, 1);

    lua_call(L, 1, 1);
    sval *= lua_tonumber(L, -1);
    lua_pop(L,1); 

    if (sval > 1.)  sval = 1.;
    if (sval < -1.) sval = -1.;
    lua_pushnumber(L, sval);
    
    return 1;
}

static int signal_mul(lua_State *L)
{
    if (lua_isnumber(L, 2)) { /* transform: sig * a -> a * sig */
        lua_insert(L, 1);
    }

    GET_SIGNAL_FUNC(L, 2);
    if (lua_isnumber(L, 1)) {
        lua_pushcclosure(L, &signal_mul_number_signal_closure, 2);
    } else {
        GET_SIGNAL_FUNC(L, 1);
        lua_pushcclosure(L, &signal_mul_signal_signal_closure, 2);
    }
    signal_new_from_closure(L);

    return 1;
}

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

    /* create metatable for signal userdatum */
    lua_pushcclosure(L, &signal_closure, 4); 
    lua_replace(L, 1);
    signal_new_from_closure(L);
    return 1; /* Signal userdata */
}

static void signal_new_from_closure(lua_State *L) 
{
    lua_newtable(L);
    lua_insert(L, -2);
    lua_setfield(L, -2, "signal");

    luaL_newmetatable(L, "lhc.signal");
    lua_pushcfunction(L, signal_play);
    lua_setfield(L, -2, "play");
    lua_pushcfunction(L, signal_stop);
    lua_setfield(L, -2, "stop");
   
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, signal_gc);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, signal_mul);
    lua_setfield(L, -2, "__mul");
    lua_pushcfunction(L, signal_add);
    lua_setfield(L, -2, "__add");
    lua_pushcfunction(L, signal_sub);
    lua_setfield(L, -2, "__sub");

    Signal* s = lua_newuserdata(L, sizeof(Signal));
    lua_insert(L, -2);
    lua_setmetatable(L, -2);
    s->is_generated = 0;
}

int luaopen_signal(lua_State *L)
{
    lua_register(L, "signal", l_signal);
    lua_register(L, "sig", l_signal);
    return 0;
}
