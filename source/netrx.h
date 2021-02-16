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


#ifndef __NETRX_H
#define __NETRX_H

#include <stdint.h>
#include "pxthread.h"
#include "lfqueue.h"


class Netrx : public Pxthread
{
public:

    enum { INIT, WAIT, PROC, TNTP, TERM, FAIL };

    Netrx (void);
    virtual ~Netrx (void);

    int start (Lfq_audio     *audioq,
               Lfq_int32     *commq,
               Lfq_timedata  *timeq,
	       int           *chlist,
	       int            psmax,
	       int            fsamp,
	       int            fsize,
               int            rtprio,
	       int            sockfd);

private:

    virtual void thr_main (void);

    void send (int flags, int32_t count, double tjack, uint32_t tsecs, uint32_t tfrac);
    int write_audio (Netdata *D);
    int write_zeros (int nfram);

    int            _state;
    bool           _first;
    double         _tq;
    double         _t0;
    double         _dt;
    double         _w1;
    double         _w2;
    Lfq_audio     *_audioq;
    Lfq_int32     *_commq;
    Lfq_timedata  *_timeq;
    int           *_chlist;
    int            _fsamp;
    int            _fsize;
    int            _sockfd;
    Netdata       *_packet;
};


#endif
