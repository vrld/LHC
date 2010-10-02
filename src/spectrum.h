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
#ifndef SPECTRUM_H
#define SPECTRUM_H

#include <lua.h>
#include <fftw3.h>

typedef struct _Spectrum
{
	int channels;

	size_t datapoints;
	fftw_complex** data;
} Spectrum;

Spectrum* l_spectrum_checkspectrum(lua_State* L, int idx);
int l_spectrum_get(lua_State* L);
int l_spectrum_set(lua_State* L);
int l_spectrum_map(lua_State* L);
int l_spectrum_dft(lua_State* L);
int l_spectrum_idft(lua_State* L);
int l_spectrum_gc(lua_State* L);
int l_spectrum_tostring(lua_State* L);
int luaopen_spectrum(lua_State* L);

#endif
