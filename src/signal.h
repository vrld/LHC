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
#ifndef SIGNAL_H
#define SIGNAL_H

#include <lua.h>
#include <lauxlib.h>
#include "config.h"
#include "thread.h"

enum { SIGNAL_PLAYING, SIGNAL_STOPPED };

typedef struct {
    double t;
    lua_State *L;
    float buffers[SAMPLE_BUFFER_COUNT][SAMPLE_BUFFER_SIZE];
    int current_buffer;
    int read_buffer_empty;
    int status;

    lhc_thread thread;
} Signal;

int luaopen_signal(lua_State *L);
int signal_userdata_is_signal(lua_State* L, int idx);
Signal* signal_checkudata(lua_State* L, int idx);
void signal_new_from_closure(lua_State *L);
void signal_register(lua_State* L, luaL_Reg* lib);

/*
 * replaces userdata at given index with associated
 * signal closure. does NO error checking!
 */
#define signal_replace_udata_with_closure(L, idx)   \
    lua_pushvalue(L, idx);                          \
    lua_gettable(L, LUA_REGISTRYINDEX);             \
    if (idx != -1 && idx != lua_gettop(L)) lua_replace(L, idx)

#endif /* SIGNAL_H */
