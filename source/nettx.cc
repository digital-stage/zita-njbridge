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


#include <stdio.h>
#include "nettx.h"
#include "zsockets.h"


Nettx::Nettx (void) :
    _stop (false)
{
}


Nettx::~Nettx (void)
{
}


void Nettx::start (Lfq_packdata   *packq, 
		   Lfq_timedata   *timeq,
		   Netdata        *descpack,
 	           int             sockfd,
		   int             rtprio)
{
    _packq = packq;
    _timeq = timeq;
    _descpack = descpack;
    _sockfd = sockfd;
    thr_start (SCHED_FIFO, rtprio, 0);
}


void Nettx::trigger (void)
{
    _sema.post ();
}


void Nettx::thr_main (void)
{
    Netdata  *D;
    Timedata *M;
    
    while (true)
    {
        _sema.wait ();
	
	if (_stop)
	{
	    _descpack->set_flags (Netdata::FL_TERM);
            send (_sockfd, (char *) _descpack->data (), _descpack->dlen (), 0);
            sock_close (_sockfd);
 	    return;
	}
        if (_packq->rd_avail () > 0)
	{
	    D = _packq->rd_datap ();
	    send (_sockfd, (char *) D->data (), D->dlen (), 0);
	    _packq->rd_commit ();
	}
	else
	{
	    if (_timeq->rd_avail () > 0)
	    {
		M = _timeq->rd_datap ();
		_descpack->set_tmark (M->_count, M->_tsecs, M->_tfrac);
		_timeq->rd_commit ();
	    }
            send (_sockfd, (char *) _descpack->data (), _descpack->dlen (), 0);
	}
    }
}
