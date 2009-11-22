#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <AL/alut.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "generators.h"
#include "signal.h"

static const int INTERVAL = (1 << (8 * sizeof(short) - 1)) - 1;

ALuint buffer, source;

int l_play(lua_State* L)
{
    double rate, length, value, t;
    size_t bytes, sample_count, i;
    short* samples;

    if (!lua_isfunction(L, 1))
        return luaL_error(L, "Expected argument #1 to be a function!");

    lua_getglobal(L, "defaults");
    lua_getfield(L, -1, "samplerate");
    rate = luaL_checknumber(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, -1, "length");
    length = luaL_checknumber(L, -1);
    lua_pop(L, 2);

    sample_count = rate * length;
    samples = malloc(sample_count * sizeof(short));
    if (samples == 0) {
        lua_close(L);
        alutExit();
        fprintf(stderr, "Cannot create sample of %lu bytes", sample_count * sizeof(short));
        exit(1);
    }

    for (i = 0; i < sample_count; ++i) {
        lua_pushvalue(L, 1);
        lua_pushnumber(L, i / rate);
        lua_call(L, 1, 1);
        value = lua_tonumber(L, -1);
        lua_pop(L, 1);
        samples[i] = value * INTERVAL;
    }

    alSourceStop(source);
    alSourcei(source, AL_BUFFER, 0);
    alBufferData(buffer, AL_FORMAT_MONO16, samples, sample_count, rate);
    free(samples);

    alSourcei(source, AL_BUFFER, (ALint)buffer);
    alSourcePlay(source);

    return 0;
}

#define SET_DEFAULT(field, value) lua_pushnumber(L, (value)); lua_setfield(L, -2, (field))
int main(int argc, char** argv)
{
    char input_buffer[512];
    int error;
    alutInit(&argc, argv);
    alGetError();

    alGenBuffers(1, &buffer);
    alGenSources(1, &source);

    lua_State *L = lua_open();
    luaL_openlibs(L);
    lua_cpcall(L, luaopen_generators, NULL);
    lua_cpcall(L, luaopen_signal, NULL);

    lua_createtable(L, 0, 5);
    SET_DEFAULT("samplerate", 96000);
    SET_DEFAULT("length", 5);
    SET_DEFAULT("freq", 440);
    SET_DEFAULT("amp", 1);
    SET_DEFAULT("phase", 0);
    lua_setglobal(L, "defaults");

    lua_register(L, "play", l_play);

    printf("lhc> ");
    while (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
        error = luaL_loadbuffer(L, input_buffer, strlen(input_buffer), "line") || lua_pcall(L, 0,0,0);
        if (error) {
            printf("error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        printf("lhc> ");
    }
    lua_close(L);

    alSourceStop(source);
    alSourcei(source, AL_BUFFER, 0);
    alDeleteBuffers(1, &buffer);
    alDeleteSources(1, &source);

    alutExit();
    return 0;
}
