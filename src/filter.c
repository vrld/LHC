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

#include "filter.h"
#include "signal.h"
#include "config.h"
#include <lauxlib.h>
#include <fftw3.h>

#define UVINDEX_SIGNAL 1
#define UVINDEX_FFT_INFO 2

typedef struct {
    fftw_plan forward;
    fftw_plan backward;
    double buffer[FILTER_WINDOW_SIZE];
    double real[FILTER_WINDOW_SIZE];
    fftw_complex *complex;
} fft_info;

static fft_info* filter_setup(lua_State* L)
{
    fft_info *fft = (fft_info*)lua_touserdata(L, lua_upvalueindex(UVINDEX_FFT_INFO));
    size_t i;

    /* shift fft->buffer to the left */
    for (i = 0; i < FILTER_WINDOW_SIZE - SAMPLE_BUFFER_SIZE; ++i)
        fft->buffer[i] = fft->buffer[i+SAMPLE_BUFFER_SIZE];

    /* call signal with t, rate */
    lua_pushvalue(L, lua_upvalueindex(UVINDEX_SIGNAL));
    lua_insert(L, 1);
    lua_call(L, 2, 2);

    /* fill back of the window */
    for (i = 1; i <= SAMPLE_BUFFER_SIZE; ++i)
    {
        lua_rawgeti(L, -2, i);
        fft->buffer[FILTER_WINDOW_SIZE - SAMPLE_BUFFER_SIZE + i-1] = lua_tonumber(L, -1);
        lua_pop(L,1);
    }
    fftw_execute(fft->forward);

    return fft;
}

static int filter_end(lua_State *L, fft_info *fft)
{
    size_t i;

    fftw_execute(fft->backward);
    for (i = 1; i <= SAMPLE_BUFFER_SIZE; ++i)
    {
        lua_pushnumber(L, fft->real[i-1] / (double)FILTER_WINDOW_SIZE);
        lua_rawseti(L, -3, i);
    }
    return 2;
}

/* Note that since we calculate the frequency
 * of an index using
 *   freq = i / BUFSIZE * samplerate, 
 * the index corrosponding to freq is:
 *   i = freq * BUFSIZE / samplerate
 */
#define to_index(f,r) (size_t)((f) * FILTER_WINDOW_SIZE / (r))
#define to_freq(i, r) (double)(i) / (double)FILTER_WINDOW_SIZE * (r)

static int filter_lowpass(lua_State *L)
{
    double rate = luaL_checknumber(L, 2);
    fft_info *fft = filter_setup(L);
    size_t i;

    size_t max = to_index(lua_tonumber(L, lua_upvalueindex(3)), rate);
    for (i = 0; i < FILTER_WINDOW_SIZE/2; ++i) 
    {
        if (i > max)
            fft->complex[i][0] = fft->complex[i][1] = .0;
    }

    return filter_end(L, fft);
}

static int filter_highpass(lua_State *L)
{
    double rate = luaL_checknumber(L, 2);
    fft_info *fft = filter_setup(L);
    size_t i;

    size_t min = to_index(lua_tonumber(L, lua_upvalueindex(3)), rate);
    for (i = 0; i < FILTER_WINDOW_SIZE/2; ++i) 
    {
        if (i < min)
            fft->complex[i][0] = fft->complex[i][1] = .0;
    }

    return filter_end(L, fft);
}

static int filter_bandpass(lua_State *L)
{
    double rate = luaL_checknumber(L, 2);
    fft_info *fft = filter_setup(L);
    size_t i;
    
    size_t low  = to_index(lua_tonumber(L, lua_upvalueindex(3)), rate);
    size_t high = to_index(lua_tonumber(L, lua_upvalueindex(4)), rate);
    for (i = 0; i < FILTER_WINDOW_SIZE/2; ++i) 
    {
        if (i < low || i > high)
            fft->complex[i][0] = fft->complex[i][1] = .0;
    }

    return filter_end(L, fft);
}

static int filter_custom(lua_State *L)
{
    double rate = luaL_checknumber(L, 2);
    fft_info *fft = filter_setup(L);

    size_t i = 0;
    /* filter function takes 3 arguments (freq, real, imag) 
     * and returns 2 values (real, imag)
     * a lowpass could be written like this:
     *   s:filter(function(f,r,i) if f < 440 then return r,i else return 0,0 end)
     */
    for (i = 0; i < FILTER_WINDOW_SIZE/2; ++i) 
    {
        lua_pushvalue(L, lua_upvalueindex(3));
        lua_pushnumber(L, to_freq(i, rate));
        lua_pushnumber(L, fft->complex[i][0]);
        lua_pushnumber(L, fft->complex[i][1]);
        lua_call(L, 3, 2);
        fft->complex[i][0] = lua_tonumber(L, -2);
        fft->complex[i][1] = lua_tonumber(L, -1);
        lua_pop(L, 2);
    }

    return filter_end(L, fft);
}

static int filter_delete_fft_info(lua_State *L)
{
    fft_info *fft = (fft_info*)lua_touserdata(L,1);
    fftw_destroy_plan(fft->forward);
    fftw_destroy_plan(fft->backward);
    fftw_free(fft->complex);
    return 0;
}

static void filter_create_fft_info(lua_State *L)
{
    size_t i;
    fft_info *fft = lua_newuserdata(L, sizeof(fft_info));

    fft->complex = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * FILTER_WINDOW_SIZE);

    fft->forward = fftw_plan_dft_r2c_1d(FILTER_WINDOW_SIZE, 
            fft->buffer, fft->complex, FFTW_ESTIMATE);
    fft->backward = fftw_plan_dft_c2r_1d(FILTER_WINDOW_SIZE,
            fft->complex, fft->real, FFTW_ESTIMATE);

    for (i = 0; i < FILTER_WINDOW_SIZE; ++i)
        fft->buffer[i] = .0;
    
    /* set destructor function to ensure the plans are freed */
    if (luaL_newmetatable(L, "lhc.signal.filter")) 
    {
        lua_pushcfunction(L, filter_delete_fft_info);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);

    /* fft_info is left on the stack*/
}

int signal_lowpass(lua_State *L)
{
    lua_settop(L, 2);

    if (!signal_userdata_is_signal(L, 1))
        return luaL_typerror(L, 1, "signal");

    if (!lua_isnumber(L, 2))
        return luaL_typerror(L, 2, "number");

    signal_replace_udata_with_closure(L, 1);
    filter_create_fft_info(L);
    lua_insert(L, 2);
    /* upvalues: 1 -> signal, 2 -> fft_info, 3 -> low */
    lua_pushcclosure(L, &filter_lowpass, 3);

    signal_new_from_closure(L);
    return 1;
}

int signal_highpass(lua_State *L)
{
    lua_settop(L, 2);

    if (!signal_userdata_is_signal(L, 1))
        return luaL_typerror(L, 1, "signal");

    if (!lua_isnumber(L, 2))
        return luaL_typerror(L, 2, "number");

    signal_replace_udata_with_closure(L, 1);
    filter_create_fft_info(L);
    lua_insert(L, 2);
    /* upvalues: 1 -> signal, 2 -> fft_info, 3 -> high */
    lua_pushcclosure(L, &filter_highpass, 3);

    signal_new_from_closure(L);
    return 1;
}

int signal_bandpass(lua_State *L)
{
    lua_settop(L, 3);

    if (!signal_userdata_is_signal(L, 1))
        return luaL_typerror(L, 1, "signal");

    if (!lua_isnumber(L, 2))
        return luaL_typerror(L, 2, "number");

    if (!lua_isnumber(L, 3))
        return luaL_typerror(L, 3, "number");

    signal_replace_udata_with_closure(L, 1);
    filter_create_fft_info(L);
    lua_insert(L, 2);
    /* upvalues: 1 -> signal, 2 -> fft_info, 3 -> low, 4 -> high */
    lua_pushcclosure(L, &filter_bandpass, 4);

    signal_new_from_closure(L);
    return 1;
}

int signal_filter(lua_State *L)
{
    if (!signal_userdata_is_signal(L, 1))
        return luaL_typerror(L, 1, "signal");
    if (!lua_isfunction(L, 2))
        return luaL_typerror(L, 2, "function");

    signal_replace_udata_with_closure(L, 1);
    filter_create_fft_info(L);
    lua_insert(L, 2);
    /* upvalues: 1 -> signal, 2 -> fft_info, 3 -> function */
    lua_pushcclosure(L, &filter_custom, 3);

    signal_new_from_closure(L);
    return 1;
}
