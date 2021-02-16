// ----------------------------------------------------------------------------
//
//  Copyright (C) 2013-2016 Fons Adriaensen <fons@linuxaudio.org>
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


#ifdef __APPLE__
#include <machine/endian.h>
#else
#include <endian.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "netdata.h"


Netdata::Netdata (int size)
{
    _size = size;
    _dlen = 0;
    _data = new unsigned char [size];
}


Netdata::~Netdata (void)
{
    delete[] _data;
}


int Netdata::packetsperperiod (int maxsize, int period, int sform, int nchan)
{
    int b, n;

    switch (sform)
    {
    case FM_16BIT: b = 2; break;          // Bytes per sample.
    case FM_24BIT: b = 3; break;
    case FM_FLOAT: b = 4; break;
    default: return -1; 
    }
    n = (maxsize - ADATA) / (b * nchan);  // Number of frames per packet.
    return (period + n - 1) / n;          // Number of packets per period.
}


// Initialise an ADESC packet.
//
void Netdata::init_audio_desc (int flags, int sform, int nchan,
                               int psmax, int fsamp, int fsize)
{
    init_header (TY_ADESC, flags, sform, nchan);
    putint (PSMAX, psmax);
    putint (FSAMP, fsamp);
    putint (FSIZE, fsize);
    putint (TFCNT, 0);
    putint (TSECS, 0);
    putint (TFRAC, 0);
    _dlen = DPEND;
}


// Initialise an ADATA packet.
//
void Netdata::init_audio_data (int flags, int sform, int nchan,
                               int count, int nfram, int dtime)
{
    int b;

    switch (sform)
    {
    case FM_16BIT: b = 2; break;
    case FM_24BIT: b = 3; break;
    case FM_FLOAT: b = 4; break;
    default: b = 0; 
    }
    init_header (TY_ADATA, flags, sform, nchan);
    putint (COUNT, count);
    putint (NFRAM, nfram);
    putint (DTIME, dtime);
    _dlen = ADATA + b * nchan * nfram;
}


void Netdata::init_header (int ptype, int flags, int sform, int nchan)
{
    _data [0] = 'z';
    _data [1] = 'n';
    _data [2] = 'j';
    _data [3] = 'b';
    _data [PTYPE] = ptype;
    _data [FLAGS] = flags;
    _data [SFORM] = sform;
    _data [NCHAN] = nchan;
}


void Netdata::set_tmark (int32_t tfcnt, uint32_t tsecs, uint32_t tfrac)
{
    putint (TFCNT, tfcnt);
    putint (TSECS, tsecs);
    putint (TFRAC, tfrac);
}


int Netdata::check_ptype (void) const
{
    if (   (_data [0] != 'z')
        || (_data [1] != 'n')
        || (_data [2] != 'j')
        || (_data [3] != 'b')) return -1;
    return _data [PTYPE];
}


#define R16 32767
#define R24 8388607


// Put audio samples from float array into network packet.
//
void Netdata::put_audio (int chan, int offs, int nsamp, const float *adata, int astep)
{
    int            i, v, nch, fmt;
    unsigned char  *q, *p;

    nch = _data [NCHAN];	
    fmt = _data [SFORM];
    switch (fmt)
    {
    case FM_16BIT:
	q = _data + ADATA + 2 * (nch * offs + chan);
	for (i = 0; i < nsamp; i++)
	{
	    v = (int)(R16 * adata [i * astep] + 0.5f);
	    if (v >  R16) v =  R16;
	    if (v < -R16) v = -R16;
	    q [0] = v >> 8;
	    q [1] = v;
	    q += 2 * nch;
	}
	break;
    
    case FM_24BIT:	
        q = _data + ADATA + 3 * (nch * offs + chan);
        for (i = 0; i < nsamp; i++)
        {
            v = (int)(R24 * adata [i * astep] + 0.5f);
            if (v >  R24) v =  R24;
            if (v < -R24) v = -R24;
            q [0] = v >> 16;
            q [1] = v >> 8;
            q [2] = v;
            q += 3 * nch;
	}
	break;

#if __BYTE_ORDER == __BIG_ENDIAN
    case FM_FLOAT:
	float *f = (float *)(_data + ADATA);
	f += nch * offs + chan;
	for (i = 0; i < nsamp; i++)
	{
	    f [i * nch] = adata [i * astep];
	}
	break;
#else
    case FM_FLOAT:
	p = (unsigned char *) adata;
	q = _data + ADATA + 4 * (nch * offs + chan);
	for (i = 0; i < nsamp; i++)
	{
	    q [0] = p [3];
	    q [1] = p [2];
	    q [2] = p [1];
	    q [3] = p [0];
	    p += 4 * astep;
	    q += 4 * nch;
	}
	break;
#endif
    }
}


// Get audio samples from network packet into float array.
//
void Netdata::get_audio (int chan, int offs, int nsamp, float *adata, int astep) const
{
    int            i, v, nch, fmt;
    unsigned char  *p, *q;

    nch = _data [NCHAN];	
    fmt = _data [SFORM];
    switch (fmt)
    {
    case FM_16BIT:
	p = _data + ADATA + 2 * (nch * offs + chan);
	for (i = 0; i < nsamp; i++)
	{
	    v = p [0];
	    if (v & 128) v -= 256;
	    adata [i * astep] = (float)((v << 8) + p [1]) / R16;
	    p += 2 * nch;
	}
	break;
    
    case FM_24BIT:	
        p = _data + ADATA + 3 * (nch * offs + chan);
	for (i = 0; i < nsamp; i++)
	{
	    v = p [0];
	    if (v & 128) v -= 256;
	    adata [i * astep] = (float)((v << 16) + (p [1] << 8) + p [2]) / R24;
	    p += 3 * nch;
	}
	break;

#if __BYTE_ORDER == __BIG_ENDIAN
    case FM_FLOAT:
	float *f = (float *)(_data + ADATA);
	f += nch * offs + chan;
	for (i = 0; i < nsamp; i++)
	{
	    adata [i * astep] = f [i * nch];
	}
	break;
#else
    case FM_FLOAT:
	p = _data + ADATA + 4 * (nch * offs + chan);
	q = (unsigned char *) adata;
	for (i = 0; i < nsamp; i++)
	{
	    q [0] = p [3];
	    q [1] = p [2];
	    q [2] = p [1];
	    q [3] = p [0];
	    p += 4 * nch;
	    q += 4 * astep;
	}
	break;
#endif
    }
}

