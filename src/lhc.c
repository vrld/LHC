#include <string.h>
#include <AL/alut.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "generators.h"
#include "signal.h"


#define SET_DEFAULT(field, value) lua_pushnumber(L, (value)); lua_setfield(L, -2, (field))
int main(int argc, char** argv)
{
    char input_buffer[512];
    int error;
    alutInit(&argc, argv);
    alGetError();

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

    alutExit();
    return 0;
}
