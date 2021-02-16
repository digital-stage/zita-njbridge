// ----------------------------------------------------------------------------
//
//  Copyright (C) 2012-2016 Fons Adriaensen <fons@linuxaudio.org>
//    
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ----------------------------------------------------------------------------


#ifndef __TIMERS_H
#define __TIMERS_H


#include <math.h>
#include <sys/time.h>
#include <jack/jack.h>


#define tjack_mod ldexp (1e-6f, 32)
#define tntp_mod 1000



inline double tjack_diff (double a, double b)    
{
    double d, m;

    d = a - b;
    m = tjack_mod;
    while (d < -m / 2) d += m;
    while (d >= m / 2) d -= m;
    return d;
}


inline double tsyst_diff (double a, double b)    
{
    double d, m;

    d = a - b;
    m = tntp_mod;
    while (d < -m / 2) d += m;
    while (d >= m / 2) d -= m;
    return d;
}


inline double tjack (jack_time_t t, double dt = 0)
{
    int32_t u = (int32_t)(t & 0xFFFFFFFFLL);
    return 1e-6 * u;
}


inline double tntp (uint32_t s, uint32_t f)
{
    return (s % tntp_mod) + ldexp ((double) f, -32);
}


inline double tntp_now (void)
{
    struct timeval T;
    
    gettimeofday (&T, 0);
    return ((T.tv_sec + 0x83AA7E80) % tntp_mod) + 1e-6 * T.tv_usec;
}


inline void tntp_now (uint32_t *secs, uint32_t *frac, double dt = 0.0)
{
    struct timeval T;
    int32_t  s;
    double   f;
    
    gettimeofday (&T, 0);
    f = 1e-6 * T.tv_usec + dt;
    s = (int32_t) floor (f);
    *frac = (uint32_t) ldexp (f - s, 32); 
    *secs = T.tv_sec + 0x83AA7E80 + s;
}


#endif
