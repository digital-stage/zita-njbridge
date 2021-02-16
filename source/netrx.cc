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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <jack/jack.h>
#include "zsockets.h"
#include "timers.h"
#include "netrx.h"


Netrx::Netrx (void) :
    _state (INIT)
{
}


Netrx::~Netrx (void)
{
}


int Netrx::start (Lfq_audio     *audioq,
                  Lfq_int32     *commq,
                  Lfq_timedata  *timeq,
		  int           *chlist,
		  int            psmax,
		  int            fsamp,
		  int            fsize,
                  int            rtprio,
		  int            sockfd)
{
    _audioq = audioq;
    _commq  = commq;
    _timeq  = timeq;
    _chlist = chlist;
    _fsamp  = fsamp;
    _fsize  = fsize;
    _sockfd = sockfd;
    _packet = new Netdata (psmax);

    // Compute DLL filter coefficients.
    _dt = (double) _fsize / fsamp;
    _w1 = 2 * M_PI * 0.05 * _dt;
    _w2 = _w1 * _w1;
    _w1 *= 3.0;

    // Start the receiver thread.
    if (thr_start (SCHED_FIFO, rtprio, 0x10000)) return 1;
    return 0;
}


void Netrx::thr_main (void)
{
    int     rv, pt, fl, fc, dc;
    double  tr, err;

    _state = WAIT;
    while (_state < TERM)
    {
        // Wait for packet, get timestamp.
	rv = recv (_sockfd, _packet->data (), _packet->size (), 0);
        tr = tjack (jack_get_time ());
	
	// Check socket status.
	if (rv <= 0)
	{
	    _state = FAIL;
	    send (_state, 0, 0.0, 0, 0);
	    break;
	}

	// Basic packet validity check.
	pt = _packet->check_ptype ();
	if (pt < 0) continue;

	// Check for termination or suspend.
        fl = _packet->get_flags ();
	if (fl & Netdata::FL_TERM)
	{
	    _state = TERM;
	    send (_state, 0, 0.0, 0, 0);
	    break;
	}
	if (fl & Netdata::FL_SUSP)
	{
	    _state = WAIT;
	    send (_state, 0, 0.0, 0, 0);
	    continue;
	}

	// // Get time marker if descriptor packet.
	// if ((pt == Netdata::TY_ADESC) && (rv == Netdata::DPEND))
	// {
   	//     send (TNTP, _packet->get_tfcnt (), 0.0,
	// 	  _packet->get_tsecs (), _packet->get_tfrac ());
	// }

	// Ignore packet if not sample data.
	if (pt != Netdata::TY_ADATA) continue;

        // Check for commands from the Jack thread.
        if (_commq->rd_avail ())
	{
	    _state = _commq->rd_int32 ();
	    if (_state == PROC) _first = true;
	}

	// Ignore data if not yet active.
        if (_state != PROC) continue;

        // Apply timing correction from sender.
        tr -= 1e-6 * _packet->get_dtime ();

	dc = 0;
	fc = _packet->get_count ();
	if (_first)
	{
   	    // First packet must be a timed one.
	    if (fl & Netdata::FL_TIMED)
	    {
   	        _first = false;
		_audioq->wr_commit (fc);
	        _t0 = tr;
	    }
	    else continue;
	}
	else
	{
	    // Check frame count continuity.
 	    dc = fc - _audioq->nwr ();
	    if (dc > 0)
	    {
		// Missing frames, replace by silence.
	         write_zeros (dc);
		 _t0 += (double) dc / _fsamp;
	    }
	    if (dc < 0)
	    {
		// Packet out of order, already
		// replaced by silence so ignore.
                continue;
	    }
	    if (fl & Netdata::FL_TIMED)
	    {
  	        // Update the DLL.
                err = tjack_diff (tr, _t0);
	        if (err >  _dt) err =  _dt;
	        if (err < -_dt) err = -_dt;
		_t0 += _w1 * err;
	        _dt += _w2 * err;
	    }
	}

	if (fl & Netdata::FL_TIMED)
	{
	    // Send timing data to Jack thread and update DLL.
	    send (_state, _audioq->nwr (), _t0, 0, 0);
  	    _t0 = tjack_diff (_t0, -_dt);
	}

	// Write samples to queue.
	write_audio (_packet);
    }
    
    delete _packet;
    _state = INIT;
}


void Netrx::send (int flags, int32_t count, double tjack, uint32_t tsecs, uint32_t tfrac)
{
    Timedata *D;

    if (_timeq->wr_avail () > 0)
    {
        D = _timeq->wr_datap (); 
        D->_flags = flags;
        D->_count = count;
        D->_tjack = tjack;
	D->_tsecs = tsecs;
	D->_tfrac = tfrac;
        _timeq->wr_commit ();
    }
}


// The following two functions write data to the audio queue.
// Note that we do *not* check the queue's fill state, and it
// may overrun. This is entirely intentional. The queue keeps
// correct read and write counters even in that case, and the
// main control loop and error recovery depend on it working
// and being used in this way. 


int Netrx::write_audio (Netdata *D)
{
    int    i, j, c, n, k;
    int    nfp, ncp, ncq;
    float  *q;

    nfp = D->get_nfram ();
    ncp = D->get_nchan ();
    ncq = _audioq->nchan (); 
    // This loop takes care of wraparound.
    for (n = nfp; n; n -= k)
    {
	q = _audioq->wr_datap ();   // Audio queue write pointer.
	k = _audioq->wr_linav ();   // Number of frames that can be
	if (k > n) k = n;           // written without wraparound.
	// Loop over all selected channels.
	for (j = 0; j < ncq; j++)
	{
	    c = _chlist [j];
	    if (c < ncp)
	    {
		// Copy from packet to audio queue.
		D->get_audio (c, nfp - n, k, q, ncq);
	    }
	    else
	    {
		// Channel not available, write zeros.
		for (i = 0; i < k; i++) q [i * ncq] = 0;
	    }
	    q++;
	}
	_audioq->wr_commit (k);    // Update audio queue state.
    }
    return nfp;
}


int Netrx::write_zeros (int nfram)
{
    int    n, k;
    float  *q;

    // This loop takes care of wraparound.
    for (n = nfram; n; n -= k)
    {
	q = _audioq->wr_datap ();  // Audio queue write pointer.
	k = _audioq->wr_linav ();  // Number of frames that can be
	if (k > n) k = n;          // written without wraparound.
	memset (q, 0, k * _audioq->nchan () * sizeof (float));
	_audioq->wr_commit (k);    // Update audio queue state.
    }
    return nfram;
}

