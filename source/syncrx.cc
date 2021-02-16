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


#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <jack/jack.h>
#include "zsockets.h"
#include "syncrx.h"
#include "timers.h"


Syncrx::Syncrx (void) :
    _state (INIT)
{
}


Syncrx::~Syncrx (void)
{
    _state = TERM;
    usleep(100000);
}


int Syncrx::start (Lfq_timedata  *timeq,
		   int            rtprio,
		   int            sockfd)
{
    _timeq = timeq;
    _sockfd = sockfd;
    thr_start (SCHED_FIFO, 80, 0);
    return 0;
}


void Syncrx::thr_main (void)
{
    char     d [32];		
    int      rv, s, u;
    double   tr; 

    _state = PROC;
    while (_state < TERM)
    {
	rv = recv (_sockfd, d, 32, 0);
	tr = tjack (jack_get_time ());
	if (rv <= 0)
	{
	    _state = FAIL;
	    break;
	}
        if ((rv == 20) && !strcmp (d, "/time"))
	{
	    s = ntohl (* (int32_t *)(d + 12));	
	    u = ntohl (* (int32_t *)(d + 16));	
            if (s) send (_state, tr, s, u);
	}
    }
    _state = INIT;
}


void Syncrx::send (int32_t flags, double tjack, uint32_t tsecs, uint32_t tfrac)
{
    Timedata *D;

    if (_timeq->wr_avail () > 0)
    {
        D = _timeq->wr_datap (); 
        D->_flags = flags;
        D->_tjack = tjack;
        D->_tsecs = tsecs;
	D->_tfrac = tfrac;
	_timeq->wr_commit ();
    }
}



