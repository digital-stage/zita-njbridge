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


#include "lfqueue.h"


Lfq_timedata::Lfq_timedata (int nelm) :
    _nwr (0),
    _nrd (0)
{
    int k;
    for (k = 1; k < nelm; k <<= 1);
    _nelm = k;
    _mask = k - 1;
    _data = new Timedata [k];
}

Lfq_timedata::~Lfq_timedata (void)
{
    delete[] _data;
} 



Lfq_infodata::Lfq_infodata (int nelm) :
    _nwr (0),
    _nrd (0)
{
    int k;
    for (k = 1; k < nelm; k <<= 1);
    _nelm = k;
    _mask = k - 1;
    _data = new Infodata [k];
}

Lfq_infodata::~Lfq_infodata (void)
{
    delete[] _data;
} 



Lfq_packdata::Lfq_packdata (int nelm, int size) :
    _nwr (0),
    _nrd (0)
{
    int k;
    for (k = 1; k < nelm; k <<= 1);
    _nelm = k;
    _mask = k - 1;
    _data = new Netdata* [k];
    for (int i = 0; i < k; i++) _data [i] = new Netdata (size);
}

Lfq_packdata::~Lfq_packdata (void)
{
    for (int i = 0; i < _nelm; i++) delete _data [i];
    delete[] _data;
} 



Lfq_int32::Lfq_int32 (int nelm) :
    _nwr (0),
    _nrd (0)
{
    int k;
    for (k = 1; k < nelm; k <<= 1);
    _nelm = k;
    _mask = k - 1;
    _data = new int32_t [k];
}

Lfq_int32::~Lfq_int32 (void)
{
    delete[] _data;
} 



Lfq_audio::Lfq_audio (int nfram, int nchan) :
    _nwr (0),
    _nrd (0)
{
    int k;
    for (k = 16; k < nfram; k <<= 1);
    _nfram = k;
    _nchan = nchan;
    _mask = k - 1;
    _data = new float [_nchan * k];
}

Lfq_audio::~Lfq_audio (void)
{
    delete[] _data;
} 


