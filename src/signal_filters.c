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
#include "filter_helper.h"
#include "signal.h"
#include <stdlib.h>
#include <lauxlib.h>
#include <fftw3.h>
#include <math.h>

#define FFT_SIZE (2 * SAMPLE_BUFFER_SIZE)
typedef struct filter_info 
{
    fftw_plan     forward;
    fftw_plan     backward;

    fftw_complex* filter;
    double        signal[FFT_SIZE];
    fftw_complex* signal_fft;
    double        filtered[FFT_SIZE];
    double        filtered_old[SAMPLE_BUFFER_SIZE];
} filter_info;

static int signal_filter_closure(lua_State* L)
{
    double t = luaL_checknumber(L, 1);
    int i;
    fftw_complex temp;
    filter_info* info = (filter_info*)lua_touserdata(L, 
                            lua_upvalueindex(2));

    /* call signal */
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushnumber(L, t);
    lua_call(L, 1, 2);

    /* get signal values
     * first half: actual signal
     * second half: zeros
     * */
    for (i = 1; i <= SAMPLE_BUFFER_SIZE; ++i)
    {
        lua_rawgeti(L, -2, i);
        info->signal[i-1] = lua_tonumber(L, -1);
        info->signal[SAMPLE_BUFFER_SIZE + i - 1] = 0;
        lua_pop(L, 1);
    }

    /* convolute signal with filter via fft */
    fftw_execute(info->forward);

    for (i = 0; i < FFT_SIZE; ++i)
    {
        temp[0] = info->signal_fft[i][0];
        temp[1] = info->signal_fft[i][1];
        info->signal_fft[i][0] = 
            info->filter[i][0] * temp[0] - info->filter[i][1] * temp[1];
        info->signal_fft[i][1] = 
            info->filter[i][0] * temp[1] + info->filter[i][1] * temp[0];
    }
    
    fftw_execute(info->backward);

    /* overlap add: add second half from last step to first half
     *              of the current step. save second half for later.
     * Note that fftw's output is not normalized.
     */ 
    for (i = 0; i < SAMPLE_BUFFER_SIZE; ++i)
    {
        lua_pushnumber(L, info->filtered_old[i] +
                info->signal[i] / (double)FFT_SIZE);
        lua_rawseti(L, -3, i+1);
        info->filtered_old[i] = info->signal[i+SAMPLE_BUFFER_SIZE] / (double)FFT_SIZE;
    }

    /* new values in second table, t_new is already there */
    return 2;
}

static int signal_filter_info_gc(lua_State *L)
{
    filter_info* info = (filter_info*)lua_touserdata(L, 1);
    fftw_destroy_plan(info->backward);
    fftw_destroy_plan(info->forward);
    fftw_free(info->signal_fft);
    fftw_free(info->filter);
    return 0;
}

static filter_info* create_filter_userdata(lua_State* L)
{
    filter_info* info;
    info             = (filter_info*)lua_newuserdata(L,
                            sizeof(filter_info)); 
    info->filter     = (fftw_complex*)fftw_malloc(
                            FFT_SIZE*sizeof(fftw_complex));
    info->signal_fft = (fftw_complex*)fftw_malloc(
                            FFT_SIZE*sizeof(fftw_complex));
    info->forward    = fftw_plan_dft_r2c_1d(FFT_SIZE, info->signal,
                            info->signal_fft, FFTW_ESTIMATE);
    info->backward   = fftw_plan_dft_c2r_1d(FFT_SIZE, info->signal_fft,
                            info->signal, FFTW_ESTIMATE);
    if (luaL_newmetatable(L, "lhc.signal.filter"))
    {
        lua_pushcfunction(L, signal_filter_info_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);

    return info;
}

static void filter_to_freq_domain(filter_info* info, int kernel_size)
{
    int i;

    for (i = kernel_size; i < FFT_SIZE; ++i)
        info->signal[i] = 0.;

    fftw_execute(info->forward);
    /* copy transformed data. neccessary as forward will overwrite
     * this every time it is executed.
     * also fill 'last' filtered values with 0
     */
    for (i = 0; i < FFT_SIZE; ++i) 
    {
        info->filter[i][0] = info->signal_fft[i][0];
        info->filter[i][1] = info->signal_fft[i][1];
        info->filtered_old[i % SAMPLE_BUFFER_SIZE] = 0.;
    }
}

static int signal_filter_onefreq(lua_State* L, int(*filter_func)(double*,int,double,double))
{
    filter_info *info;
    double freq, bw = 4. / (double)(SAMPLE_BUFFER_SIZE);
    int kernel_size;

    if (!filter_func)
        return luaL_error(L, "panic: pants on fire");

    if (!signal_userdata_is_signal(L, 1))
        return luaL_typerror(L, 1, "signal");
    signal_replace_udata_with_closure(L, 1);
    lua_pushvalue(L, 1);

    freq = luaL_checknumber(L, 2);

    info = create_filter_userdata(L);
    /* generate filter kernel. 
     * put it into info->signal so we can use the plans created 
     * in create_filter
     */
    kernel_size = get_filter_width(bw);
    if (kernel_size > FFT_SIZE - 1)
        kernel_size = FFT_SIZE - 1;

    filter_func(info->signal, kernel_size, freq, SAMPLERATE);
    filter_to_freq_domain(info, kernel_size);

    /* stack: signal, f, signal, filter info */
    lua_pushcclosure(L, &signal_filter_closure, 2);
    signal_new_from_closure(L);
    lua_replace(L, 1); /* move new signal to bottom of stack */
    lua_settop(L, 1);
    return 1;
}

static int signal_filter_multifreq(lua_State* L, int(*filter_func)(double*,int,double,double,double))
{
    filter_info *info;
    double freq1, freq2, bw = 4. / (double)(SAMPLE_BUFFER_SIZE);
    double filter_temp[FFT_SIZE];
    int kernel_size, i;

    if (!signal_userdata_is_signal(L, 1))
        return luaL_typerror(L, 1, "signal");
    signal_replace_udata_with_closure(L, 1);
    lua_pushvalue(L, 1);

    /* make non-table argument a table argument */
    if (!lua_istable(L, 2)) 
    {
        lua_createtable(L, 1, 0);

        lua_createtable(L, 2, 0);
        lua_pushvalue(L, 2);
        lua_rawseti(L, -2, 1);
        lua_pushvalue(L, 3);
        lua_rawseti(L, -2, 2);
        
        lua_rawseti(L, -2, 1);
        lua_replace(L, 2);
    }
    /* check arguments for goodness */
    if (lua_objlen(L, 2) < 1)
        return luaL_error(L, "Very funny, dude...");

    lua_pushnil(L);
    while (lua_next(L, 2) != 0)
    {
        if (lua_objlen(L, -1) < 2)
            return luaL_error(L, "Filter needs frequency pairs");
        lua_rawgeti(L, -1, 1);
        lua_rawgeti(L, -2, 2);
        if (!lua_isnumber(L, -2) || !lua_isnumber(L, -1))
            return luaL_error(L, "Frequencies must be numbers");

        lua_pop(L, 3);
    }

    /* finally, the create filter part */
    info = create_filter_userdata(L);
    kernel_size = get_filter_width(bw);
    if (kernel_size > FFT_SIZE - 1)
        kernel_size = FFT_SIZE - 1;

    for (i = 0; i < FFT_SIZE; ++i)
        info->signal[i] = 0;

    /* make multiple bandpass (multipass anyone?) */
    lua_pushnil(L);
    while (lua_next(L, 2) != 0) 
    {
        lua_rawgeti(L, -1, 1);
        lua_rawgeti(L, -2, 2);
        freq1 = lua_tonumber(L, -2);
        freq2 = lua_tonumber(L, -1);

        filter_func(filter_temp, kernel_size, freq1, freq2, SAMPLERATE);
        for (i = 0; i < FFT_SIZE; ++i)
            info->signal[i] += filter_temp[i];

        lua_pop(L, 3);
    }
    filter_to_freq_domain(info, kernel_size);

    lua_pushcclosure(L, &signal_filter_closure, 2);
    signal_new_from_closure(L);
    lua_replace(L, 1);
    lua_settop(L, 1);
    return 1;
}

int signal_filter_lowpass(lua_State* L)
{
    return signal_filter_onefreq(L, &filter_lowpass);
}

int signal_filter_highpass(lua_State* L)
{
    return signal_filter_onefreq(L, &filter_highpass);
}
    
int signal_filter_bandpass(lua_State* L)
{
    return signal_filter_multifreq(L, &filter_bandpass);
}

int signal_filter_bandreject(lua_State* L)
{
    return signal_filter_multifreq(L, &filter_bandreject);
}

