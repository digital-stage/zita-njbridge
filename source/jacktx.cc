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


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "jacktx.h"
#include "timers.h"


Jacktx::Jacktx (const char *jname, const char*jserv, int nchan) :
    _client (0),
    _nchan (nchan),     
    _state (INIT),
    _freew (false),
    _packq (0),
    _timeq (0),
    _infoq (0),
    _nettx (0)
{
    init (jname, jserv);
}


Jacktx::~Jacktx (void)
{
    fini ();
}


void Jacktx::init (const char *jname, const char *jserv)
{
    int                 i, opts, spol, flags;
    char                s [64];
    jack_status_t       stat;
    struct sched_param  spar;

    opts = JackNoStartServer;
    if (jserv) opts |= JackServerName;
    _client = jack_client_open (jname, (jack_options_t) opts, &stat, jserv);
    if (_client == 0)
    {
        fprintf (stderr, "Can't connect to Jack, is the server running ?\n");
        exit (1);
    }
    jack_set_process_callback (_client, jack_static_process, (void *) this);
    jack_set_freewheel_callback (_client, jack_static_freewheel, (void *) this);
    jack_set_buffer_size_callback (_client, jack_static_buffsize, (void *) this);
    jack_on_shutdown (_client, jack_static_shutdown, (void *) this);

    _bsize = 0;
    _fsamp = 0;
    if (jack_activate (_client))
    {
        fprintf(stderr, "Can't activate Jack");
        exit (1);
    }
    _jname = jack_get_client_name (_client);
    _bsize = jack_get_buffer_size (_client);
    _fsamp = jack_get_sample_rate (_client);

    flags = JackPortIsTerminal | JackPortIsPhysical;
    if (_nchan > Netdata::MAXCHAN) _nchan = Netdata::MAXCHAN;
    for (i = 0; i < _nchan; i++)
    {
        sprintf (s, "in_%d", i + 1);
        _ports [i] = jack_port_register (_client, s, JACK_DEFAULT_AUDIO_TYPE,
                                         flags | JackPortIsInput, 0);
    }

    pthread_getschedparam (jack_client_thread_id (_client), &spol, &spar);
    _rprio = spar.sched_priority;
}


void Jacktx::fini (void)
{
    if (_client)
    {
        jack_deactivate (_client);
        jack_client_close (_client);
    }
}


void Jacktx::jack_static_shutdown (void *arg)
{
    ((Jacktx *) arg)->report (TERM);
}


int Jacktx::jack_static_buffsize (jack_nframes_t bsize, void *arg)
{
    ((Jacktx *) arg)->jack_buffsize (bsize);
    return 0;
}


void Jacktx::jack_static_freewheel (int state, void *arg)
{
    ((Jacktx *) arg)->jack_freewheel (state);
}


int Jacktx::jack_static_process (jack_nframes_t nframes, void *arg)
{
    return ((Jacktx *) arg)->jack_process (nframes);
}


void Jacktx::start (Lfq_packdata   *packq, 
                    Lfq_timedata   *timeq, 
                    Lfq_int32      *infoq, 
		    Nettx          *nettx,
		    int             sform,
                    int             npack)
{
    _packq = packq;
    _timeq = timeq;
    _infoq = infoq;
    _nettx = nettx;
    _sform = sform;
    _npack = npack;
    _count = 0;
    _first = true;
    _tnext = 0;
    _state = SEND;
    report (_state);
}


void Jacktx::report (int state)
{
    if (_infoq->wr_avail () > 0) _infoq->wr_int32 (state);
}


void Jacktx::jack_freewheel (int freew)
{
    _freew = freew;
}


void Jacktx::jack_buffsize (int bsize)
{
    if (_bsize == 0) _bsize = bsize;
    else if (_bsize != bsize) _state = Jacktx::TERM;
}


int Jacktx::jack_process (int nframes)
{
    int             i, j, bdiff, bstep;
    int             dtime, nskip, flags, nfram;
    jack_time_t     t0, t1;
    jack_nframes_t  ft;
    float           usecs;
    float           *inp [Netdata::MAXCHAN];
    Netdata         *D;
    Timedata        *M;
    
    if (_state == TERM)
    {
	report (_state);
	return 0;
    }

    // Check Jack freewheeling.
    if (_freew)
    {
	if (_state == SEND)
	{
	    // Jack entered freewheeling state.
	    // Send a 'suspend' packet.
            if (_packq->wr_avail () > 0)
	    {
   	        D = _packq->wr_datap ();
	        D->init_audio_data (Netdata::FL_SUSP, _sform, _nchan, 0, 0, 0); 
	        _packq->wr_commit ();
		_nettx->trigger ();
	    }
	    else
	    {
	        // Transmit queue is full.
                _state = TERM;
 	        report (_state);
	        return 0;
	    }
	    _state = SUSP;
	    report (_state);
	}
    }
    else
    {
	if (_state == SUSP)
	{
	    // Resume after freewheeling.
	    _first = true;
	    _state = SEND;
	    report (_state);
	}
    }

    if (_state != SEND) return 0;

    // Get cycle timings. These are used in two ways.
    // The first is to detect discontinuities after xruns,
    // or skipped cycles, so we can provide a correct frame
    // count to the receivers. In current Jack releases the
    // frame time for the start of the current cycle does
    // not correctly indicate such gaps. A patch to fix this
    // has been submitted, doing essentially the same as the
    // simple code computing 'nskip' below.
    // The second use is include the number of microseconds
    // since cycle start in the transmitted audio packets.
    // By doing this the receiver has timing data that depends
    // only on the network delay and not on the position of the
    // transmitter in the Jack graph.
    jack_get_cycle_times (_client, &ft, &t0, &t1, &usecs);
    dtime = (int)(jack_get_time () - t0);
    if (_first)
    {
	_first = false;
	nskip = 0;
    }
    else
    {
	usecs = (float)(t0 - _tnext);
	nskip = (int)(_fsamp * usecs * 1e-6f + 0.5f);
    }
    _tnext = t1;
    _count += nskip;

    // Send periodic timestamp.
    _tscnt += _bsize;
    if (_tscnt >= _fsamp)
    {
	_tscnt -= _fsamp;
	M = _timeq->wr_datap ();
	M->_count = _count + _bsize;
	tntp_now (&(M->_tsecs), &(M->_tfrac), -1e-6 * dtime);
	_timeq->wr_commit ();
    }	

    // Get port data pointers.
    for (i = 0; i < _nchan; i++)
    {
	inp [i] = (float *)(jack_port_get_buffer (_ports [i], nframes));
    }

    // Bresenham algo to divide period in packets.
    // The first packet of a period has valid time.
    bdiff = 0;
    bstep = _bsize / _npack;
    flags = Netdata::FL_TIMED;
    for (j = 0; j < _npack; j++)
    {
	if (_packq->wr_avail () > 0)
	{
	    // Create and send an audio data packet.
	    D = _packq->wr_datap ();
	    nfram = bstep;
    	    if (bdiff < 0) nfram++; // Bresenham algo.
	    D->init_audio_data (flags, _sform, _nchan, _count, nfram, dtime);
	    for (i = 0; i < _nchan; i++)
  	    { 
                D->put_audio (i, 0, nfram, inp [i], 1);
		inp [i] += nfram;
	    }
	    _packq->wr_commit ();
	    _nettx->trigger ();
	    _count += nfram;
	    // Only used in first packet of each period.
	    dtime = 0;
	    flags = 0;
	}
	else
	{
	    // Transmit queue is full.
            _state = TERM;
 	    report (_state);
	    return 0;
	}
	// Update Bresenham algo.
	bdiff += nfram * _npack - _bsize;
    }

    return 0; 
}
