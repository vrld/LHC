#ifndef FILTER_HELPER_C
#define FILTER_HELPER_C

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
#include <stdlib.h>
#include <math.h>

static const double PI = 3.14159265358979323844;

int get_filter_width(double bw)
{ 
    int M = 4. / bw;
    if (M % 2 != 1) return ++M;
    return M;
}

static void sinc(double* out, int size, double f, double rate)
{
    double m2 = (double)(size - 1) / 2.;
    double pfc = 2. * PI * f / rate;
    int i;
    double im2;

    for (i = 0; i < size; ++i) 
    {
        im2 = i - m2;
        if (im2 == 0.)
            out[i] = pfc;
        else
            out[i] = sin( pfc * im2) / im2;
    }
}

#if 0 
/* von Hann window */
static void apply_window(double* sinc, int size)
{
    double pim = 2. * PI / (double)(size - 1);
    int i;
    for (i = 0; i < size; ++i)
        sinc[i] *= .5 - .5 * cos(pim * (double)i);
}
/* Blackman window */
static void apply_window(double* sinc, int size)
{
    double pim = 2. * PI / (double)(size - 1);
    int i;
    static const double a0 = .42;
    static const double a1 = .5;
    static const double a2 = .08;
    for (i = 0; i < size; ++i)
        sinc[i] *= a0 - a1 * cos(pim * (double)i) 
                      + a2 * cos(2. * pim * (double)i);
}
#endif
/* Blackman-Harris window */
static void apply_window(double* sinc, int size)
{
    double pim = 2. * PI / (double)(size - 1) * (double)(size-1)/2.;
    int i;
    static const double a0 = .35875;
    static const double a1 = .48829;
    static const double a2 = .14128;
    static const double a3 = .01168;
    for (i = 0; i < size; ++i) {
        sinc[i] *= a0 - a1 * cos(pim * (double)i) 
                      + a2 * cos(2. * pim * (double)i)
                      - a3 * cos(3. * pim * (double)i);
    }
}

static void normalize(double* out, int size)
{
    double K = 0;
    int i;
    for (i = 0; i < size; ++i)
        K += out[i];
    for (i = 0; i < size; ++i)
        out[i] /= K;
}

void spectral_inversion(double* filter, int size)
{
    int i;
    for (i = 0; i < size; ++i)
        filter[i] = -filter[i];

    filter[(size-1)/2] += 1.;
}

int filter_lowpass(double* filter, int size, double f, double rate)
{
    if (size % 2 != 1)
        return -1;

    sinc(filter, size, f, rate);
    apply_window(filter, size);
    normalize(filter, size);
    return 0;
}

int filter_highpass(double* filter, int size, double f, double rate)
{
    if (filter_lowpass(filter, size, f, rate) != 0)
        return -1;

    spectral_inversion(filter, size);
    return 0;
}

int filter_bandpass(double* filter, int size, double f_low, double f_high, double rate)
{
    if (size % 2 != 1)
        return -1;

    if (f_high < f_low)
    {
        double t = f_high;
        f_high = f_low;
        f_low = t;
    }

    int i;
    double* temp = malloc(size * sizeof(double));
    if (!temp) 
        return -2;

    if (size % 2 != 1)
        return -1;

    filter_lowpass(filter, size, f_high, rate);
    filter_lowpass(temp, size, f_low, rate);

    for (i = 0; i < size; ++i)
        filter[i] -= temp[i];
    free(temp);

    return 0;
}

int filter_bandreject(double* filter, int size, double f_low, double f_high, double rate)
{
    int s = filter_bandpass(filter, size, f_low, f_high, rate);
    if (s != 0)
        return s;
    spectral_inversion(filter, size);

    return 0;
}

#endif /* FILTER_HELPER_C */
