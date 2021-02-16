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


#ifndef __LFQUEUE_H
#define __LFQUEUE_H


#include <stdint.h>
#include <string.h>
#include "netdata.h"


class Timedata
{
public:

    int32_t  _flags;
    int32_t  _count;  // Frame count
    double   _tjack;  // Jack time modulo 2^28 microseconds
    uint32_t _tsecs;  // Seconds of NTP time
    uint32_t _tfrac;  // Fraction of NTP time
};


class Infodata
{
public:

    int32_t  _state;
    double   _error;
    double   _ratio;
    int      _nfram;
    int      _syncc;
};


// Queue of timing info.
// Single element read/write.
// Nelm will be rounded up to a power of 2.
//
class Lfq_timedata
{
public:

    Lfq_timedata (int nelm);
    ~Lfq_timedata (void); 

    void reset (void) { _nwr = _nrd = 0; }
    int  nelm (void) const { return _nelm; }

    int       wr_avail (void) const { return _nelm - _nwr + _nrd; } 
    Timedata *wr_datap (void) { return _data + (_nwr & _mask); }
    void      wr_commit (void) { _nwr++; }

    int       rd_avail (void) const { return _nwr - _nrd; } 
    Timedata *rd_datap (void) { return _data + (_nrd & _mask); }
    void      rd_commit (void) { _nrd++; }

private:

    Timedata   *_data;
    int         _nelm;
    int         _mask;
    int         _nwr;
    int         _nrd;
};


// Queue of Infodata, from jack TX/RX threads to main.
// Single element read/write.
// Nelm will be rounded up to a power of 2.
//
class Lfq_infodata
{
public:

    Lfq_infodata (int nelm);
    ~Lfq_infodata (void); 

    void reset (void) { _nwr = _nrd = 0; }
    int  nelm (void) const { return _nelm; }

    int       wr_avail (void) const { return _nelm - _nwr + _nrd; } 
    Infodata *wr_datap (void) { return _data + (_nwr & _mask); }
    void      wr_commit (void) { _nwr++; }

    int       rd_avail (void) const { return _nwr - _nrd; } 
    Infodata *rd_datap (void) { return _data + (_nrd & _mask); }
    void      rd_commit (void) { _nrd++; }

private:

    Infodata   *_data;
    int         _nelm;
    int         _mask;
    int         _nwr;
    int         _nrd;
};


// Queue of Packdata objects, from jack TX thread to network TX.
// Single element read/write.
// Nelm will be rounded up to a power of 2.
//
class Lfq_packdata
{
public:

    Lfq_packdata (int nelm, int size);
    ~Lfq_packdata (void); 

    void reset (void) {_nwr = _nrd = 0; }
    int  nelm (void) const { return _nelm; }

    int       wr_avail (void) const { return _nelm - _nwr + _nrd; } 
    Netdata  *wr_datap (void) { return _data [_nwr & _mask]; }
    void      wr_commit (void) { _nwr++; }

    int       rd_avail (void) const { return _nwr - _nrd; } 
    Netdata  *rd_datap (void) { return _data [_nrd & _mask]; }
    void      rd_commit (void) { _nrd++; }

private:

    Netdata   **_data;
    int         _nelm;
    int         _mask;
    int         _nwr;
    int         _nrd;
};


// Queue of 32-bit elements.
// Single element read/write.
// Nelm will be rounded up to a power of 2.
//
class Lfq_int32
{
public:

    Lfq_int32 (int nelm);
    ~Lfq_int32 (void); 

    int  nelm (void) const { return _nelm; }
    void reset (void) { _nwr = _nrd = 0; }

    int      wr_avail (void) const { return _nelm - _nwr + _nrd; } 
    void     wr_commit (void) { _nwr++; }

    int      rd_avail (void) const { return _nwr - _nrd; } 
    void     rd_commit (void) { _nrd++; }

    // These include the commit() call.
    void     wr_int32 (int32_t v) { _data [_nwr++ & _mask] = v; }
    void     wr_float (float v) { *(float *)(_data + (_nwr++ & _mask)) = v; }
    int32_t  rd_int32 (void) { return  _data [_nrd++ & _mask]; }
    float    rd_float (void) { return *(float *)(_data + (_nrd++ & _mask)); }

private:

    int32_t  *_data;
    int       _nelm;
    int       _mask;
    int       _nwr;
    int       _nrd;
};


// Queue of multichannel audio frames, from net RX to jack RX thread.
// Supports block copy and random access.
// Nfram will be rounded up to a power of 2.
// 
class Lfq_audio
{
public:

    Lfq_audio (int nfram, int nchan);
    ~Lfq_audio (void); 

    void reset (void)
    {
        _nwr = _nrd = 0;
	memset (_data, 0, _nfram * _nchan * sizeof (float));
    }

    int     nfram (void) const { return _nfram; } 
    int     nchan (void) const { return _nchan; } 
    int     nwr (void) const { return _nwr; };
    int     nrd (void) const { return _nrd; };

    int     wr_avail (void) const { return _nfram - _nwr + _nrd; } 
    int     wr_linav (void) const { return _nfram - (_nwr & _mask); }
    float  *wr_datap (void) { return _data + _nchan * (_nwr & _mask); }
    void    wr_commit (int k) { _nwr += k; }

    int     rd_avail (void) const { return _nwr - _nrd; } 
    int     rd_linav (void) const { return _nfram - (_nrd & _mask); }
    float  *rd_datap (void) { return _data + _nchan * (_nrd & _mask); }
    void    rd_commit (int k) { _nrd += k; }

private:

    float    *_data;
    int       _nfram;
    int       _nchan;
    int       _mask;
    int       _nwr;
    int       _nrd;
};


#endif

