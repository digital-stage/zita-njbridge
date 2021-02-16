// ----------------------------------------------------------------------------
//
//  Copyright (C) 2015-2016 Fons Adriaensen <fons@linuxaudio.org>
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


#ifndef __SYNCRX_H
#define __SYNCRX_H

#include <stdint.h>
#include "pxthread.h"
#include "lfqueue.h"


class Syncrx : public Pxthread
{
public:

    enum { INIT, PROC, TERM, FAIL };

    Syncrx (void);
    virtual ~Syncrx (void);

    int start (Lfq_timedata  *timeq,
	       int            rtprio,
	       int            sockfd);

private:

    virtual void thr_main (void);

    void send (int32_t flags, double tjack, uint32_t tsecs, uint32_t tfrac);

    int            _state;
    Lfq_timedata  *_timeq;
    int            _sockfd;
};


#endif
