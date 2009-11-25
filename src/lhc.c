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
#include <string.h>
#include <AL/alut.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>

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

    if (argc > 0)
    {
        FILE* f = fopen(argv[1], "r");
        if (f != NULL)
        {
            fclose(f);
            error = luaL_dofile(L, argv[1]);
            if (error) {
                fprintf(stderr, "error: %s\n", lua_tostring(L, -1));
            } else {
                char tmp;
                fprintf(stdin, "type anything to exit...");
                fread(&tmp, 1, 1, stdin);
            }
        }
        else
        {
            fprintf(stderr, "cannot open '%s' for reading!\n", argv[1]);
        }
    }
    else
    {
        printf("lhc> ");
        while (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
            error = luaL_loadbuffer(L, input_buffer, strlen(input_buffer), "line") || lua_pcall(L, 0,0,0);
            if (error) {
                fprintf(stderr, "error: %s\n", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
            printf("lhc> ");
        }
    }
    lua_close(L);

    alutExit();
    return 0;
}
