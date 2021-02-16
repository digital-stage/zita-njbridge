// ----------------------------------------------------------------------------
//
//  Copyright (C) 2013-2018 Fons Adriaensen <fons@linuxaudio.org>
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
#include <math.h>
#include "netdata.h"
#include "jackrx.h"
#include "timers.h"
#include "netrx.h"


Jackrx::Jackrx (const char *jname, const char*jserv, int nchan, const int *clist) :
    _client (0),
    _nchan (nchan),     
    _state (INIT),
    _freew (false)
{
    init (jname, jserv, clist);
}


Jackrx::~Jackrx (void)
{
    fini ();
}


void Jackrx::init (const char *jname, const char *jserv, const int *clist)
{
    int                 i, opts, spol, flags;
    char                s [16];
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
    jack_set_latency_callback (_client, jack_static_latency, (void *) this);
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
        sprintf (s, "out_%d", clist [i] + 1);
        _ports [i] = jack_port_register (_client, s, JACK_DEFAULT_AUDIO_TYPE,
                                         flags | JackPortIsOutput, 0);
    }
    pthread_getschedparam (jack_client_thread_id (_client), &spol, &spar);
    _rprio = spar.sched_priority;
    _buff = new float [_bsize * _nchan];
    _state = IDLE;
}


void Jackrx::fini (void)
{
    if (_client)
    {
        jack_deactivate (_client);
        jack_client_close (_client);
    }
    delete[] _buff;
}


void Jackrx::jack_static_shutdown (void *arg)
{
    ((Jackrx *) arg)->sendinfo (FATAL, 0, 0, 0);
}


int Jackrx::jack_static_buffsize (jack_nframes_t bsize, void *arg)
{
    ((Jackrx *) arg)->jack_buffsize (bsize);
    return 0;
}


void Jackrx::jack_static_freewheel (int state, void *arg)
{
    ((Jackrx *) arg)->jack_freewheel (state);
}


void Jackrx::jack_static_latency (jack_latency_callback_mode_t jlcm, void *arg)
{
}


int Jackrx::jack_static_process (jack_nframes_t nframes, void *arg)
{
    return ((Jackrx *) arg)->jack_process (nframes);
}


void Jackrx::start (Lfq_audio      *audioq,
                    Lfq_int32      *commq, 
                    Lfq_timedata   *timeq,
                    Lfq_timedata   *syncq,
                    Lfq_infodata   *infoq,
                    double         ratio,
                    int            delay,
                    int            rqual)
{
    _audioq = audioq;
    _commq = commq;
    _timeq = timeq;
    _syncq = syncq;
    _infoq = infoq;
    _ratio = ratio;
    _rcorr = 1.0;
    _resamp.setup (_ratio, _nchan, rqual);
    _resamp.set_rrfilt (100);
    _delay = delay;
    _ppsec = (_fsamp + _bsize / 2) / _bsize;
    _first = true;
    _tnext = 0;
    _limit = (int)(_fsamp / _ratio);
    initwait (_ppsec / 2);
}


void Jackrx::initwait (int nwait)
{
    _count = -nwait;
    _commq->wr_int32 (Netrx::WAIT);
    _state = WAIT;
    if (nwait > _ppsec) sendinfo (_state, (double) nwait / _ppsec, 0, 0);
}


void Jackrx::initsync (void)
{
//  Reset all lock-free queues.
    _commq->reset ();
    _timeq->reset ();
    _audioq->reset ();
    // Reset and prefill the resampler.
    _resamp.reset ();
    _resamp.inp_count = _resamp.inpsize () / 2 - 1;
    _resamp.out_count = 10000;
    _resamp.process ();
    // Initiliase state variables.
    _first = true;
    _t_a0 = _t_a1 = 0;
    _k_a0 = _k_a1 = 0;
    // Initialise loop filter state.
    _z1 = _z2 = _z3 = 0;
    // Activate the netrx thread,
    _commq->wr_int32 (Netrx::PROC);
    _state = SYNC0;
    _syncnt = 0;
    _syndel = 0.0;
    sendinfo (_state, 0, 0, 0);
}


void Jackrx::setloop (double bw)
{
    double w;

    // Set the loop bandwidth to bw Hz.
    w = 6.28 * bw * _bsize / _fsamp;
    _w0 = 1.0 - exp (-20.0 * w);
    _w1 = w * 2.0 * _ratio / _bsize;
    _w2 = w / 2.0; 
}


void Jackrx::capture (int nframes)
{
    int    i, j, k1, k2;
    float  *p, *q;

    // Read from audio queue and resample.
    // The while loop takes care of wraparound.
    _resamp.out_count = _bsize;
    _resamp.out_data  = _buff;
    while (_resamp.out_count)
    {
        // Allow the audio queue to underrun, but
        // use zero valued samples in that case.
        // This will happen when the sender skips
        // some cycles and the receiver is not
        // configured for additional latency.
        k1 = _audioq->rd_avail ();
        k2 = _audioq->rd_linav ();
        if (k1 > 0)
        {
            _resamp.inp_count = (k1 < k2) ? k1 : k2;
            _resamp.inp_data  = _audioq->rd_datap ();
        }
        else
        {
            _resamp.inp_count = 999999;
            _resamp.inp_data = 0;
        }
        // Resample up to a full output buffer.
        k1 = _resamp.inp_count;
        _resamp.process ();
        k1 -= _resamp.inp_count;
        // Adjust audio queue and state by the
        // number of frames consumed.
        _audioq->rd_commit (k1);
    }
    // Deinterleave _buff to outputs.
    for (j = 0; j < _nchan; j++)
    {
        p = _buff + j;
        q = (float *)(jack_port_get_buffer (_ports [j], nframes));
        for (i = 0; i < _bsize; i++) q [i] = p [i * _nchan];
    }       
}


void Jackrx::silence (int nframes)
{
    int    i;
    float  *q;

    // Write silence to all jack ports.
    for (i = 0; i < _nchan; i++)
    {
        q = (float *)(jack_port_get_buffer (_ports [i], nframes));
        memset (q, 0, nframes * sizeof (float));
    }
}


void Jackrx::sendinfo (int state, double error, double ratio, int nfram)
{
    Infodata *I;

    if (_infoq->wr_avail ())
    {
        I = _infoq->wr_datap ();
        I->_state = state;
        I->_error = error;
        I->_ratio = ratio;
        I->_nfram = nfram;
        I->_syncc = _syncnt;
        _infoq->wr_commit ();
    }
}


void Jackrx::jack_freewheel (int yesno)
{
    _freew = yesno ? true : false;
    if (_freew) initwait (_ppsec / 4);
}


void Jackrx::jack_buffsize (int bsize)
{
    if (_bsize == 0) _bsize = bsize;
    else if (_bsize != bsize) _state = Jackrx::FATAL;
}


int Jackrx::jack_process (int nframes)
{
    int             k, nskip;
    double          d1, d2, err;
    jack_time_t     t0, t1;
    jack_nframes_t  ft;
    float           usecs;
    bool            shift; 
    Timedata        *D;

    // Skip cylce if ports may not yet exist.
    if (_state < IDLE) return 0;

    // Buffer size change, no data, or other evil.
    if (_state >= TXEND)
    {
        sendinfo (_state, 0, 0, 0);
        _state = IDLE;
        return 0;
    }
    // Output silence if idle.
    if (_state < WAIT)
    {
        silence (nframes);
        return 0;
    }

    // Start synchronisation 1/2 second after entering
    // the WAIT state. Disabled while freewheeling.
    if (_state == WAIT)
    {
        silence (nframes);
        if (_freew) return 0;
        if (++_count == 0) initsync ();
        else return 0;
    }

    // Get local timing info.
    jack_get_cycle_times (_client, &ft, &t0, &t1, &usecs);
    _t_j0 = tjack (t0);

    if (_first)
    {
        _first = false;
        nskip = 0;
    }
    else
    {
        usecs = (float)(t0 - _tnext);
        nskip = (int)(1e-6f * usecs * _fsamp / _ratio + 0.5f);
    }
    _tnext = t1;
    _audioq->rd_commit (nskip);
    
    // Check if we have info from the netrx thread.
    // If the queue is full restart synchronisation.
    // This can happen e.g. on a jack engine timeout,
    // or when too many cycles have been skipped.
    if (_timeq->rd_avail () >= _timeq->nelm ())
    {
        initwait (_ppsec / 2);
        return 0;
    }
    shift = true;
    while (_timeq->rd_avail ())
    {
        D = _timeq->rd_datap ();
        switch (D->_flags)
        {
        case Netrx::WAIT:
            // Restart synchronisation in case the netrx
            // thread signals a problem. This will happen
            // when the sender goes into freewheeling mode.
            initwait (_ppsec / 2);
            return 0;
        case Netrx::PROC:
            // Frame count and reception time stamp.
            if (shift)
            {
                shift = false;
                _t_a0 = _t_a1;
                _k_a0 = _k_a1;
                if (_state < SYNC2)
                {
                    _state++;
                    sendinfo (_state, 0, 0, 0);
                }
            }
            _k_a1 = D->_count;
            _t_a1 = D->_tjack;
            break;
//      case Netrx::TNTP:
//          // Frame count and system time at sender.
//          procsync (D->_count, D->_tsecs, D->_tfrac);
//          break;
        case Netrx::TERM:
            // Sender terminated.
            _state = TXEND;
            return 0;
        case Netrx::FAIL:
            // Fatal error in netrx thread.
            _state = FATAL;
            return 0;
        }
        _timeq->rd_commit ();
    }

    err = 0;
    if (_state >= SYNC2)
    {
        // Compute the delay error.
        d1 = tjack_diff (_t_j0, _t_a0);
        d2 = tjack_diff (_t_a1, _t_a0);
        // This must be done as integer as both terms will overflow.
        k = _k_a0 - _audioq->nrd ();
        err = k + (_k_a1 - _k_a0) * d1 / d2  + _resamp.inpdist () - _delay;
        if (_state == SYNC2)
        {
            // We have the first delay error value. Adjust the audio queue
            // to obtain the actually wanted delay, and start tracking.
            k = (int)(floor (err + 0.5));
            _audioq->rd_commit (k);
            err -= k;
            setloop (0.5);
            _state = PROC1;
        }    
    }

    // Switch to lower bandwidth after 4 seconds.
    if ((_state == PROC1) && (++_count == 4 * _ppsec))
    {
        _state = PROC2;
        setloop (0.05);
    } 

    if (_state >= PROC1)
    {
        // Run loop filter and set resample ratio.
        _z1 += _w0 * (_w1 * err - _z1);
        _z2 += _w0 * (_z1 - _z2);
        _z3 += _w2 * _z2;
        if (fabs (_z3) > 0.05)
        {
            // Something is really wrong.
            // Wait 10 seconds then restart.
            initwait (10 * _ppsec);
            return 0;
        }
        _rcorr = 1 - (_z2 + _z3);
        if (_rcorr > 1.05) _rcorr = 1.05;
        if (_rcorr < 0.95) _rcorr = 0.95;
        _resamp.set_rratio (_rcorr);

        // Resample and transfer between audio
        // queue and jack ports.
        capture (nframes);
        k = _audioq->rd_avail ();
        sendinfo (_state, err, _rcorr, k);
        if (k < -_limit) _state = TXEND;
    }
    else silence (nframes);

    return 0; 
}


// void Jackrx::procsync (int32_t fc_ref, uint32_t s_ref, uint32_t f_ref)
// {
//     int       k;
//     double    ts_ref, dj, ds, del;
//     Timedata  *D;

//     ts_ref = tntp (s_ref, f_ref);

//     while (_syncq->rd_avail () > 1) _syncq->rd_commit ();
//     if (_syncq->rd_avail () > 0)
//     {
//         D = _syncq->rd_datap ();
//         _tj_ext = D->_tjack;
//         _ts_ext = tntp (D->_tsecs, D->_tfrac);
//         _syncq->rd_commit ();
//     }
//     else
//     {
//         _syndel = 0.0;
//         _syncnt = 0;
//         return;
//     }
    
//     ds = tsyst_diff (ts_ref, _ts_ext);
//     dj = tjack_diff (_tj_ext, _t_a0);
//     del = (ds + dj) * _fsamp / _ratio - fc_ref + _k_a0 + _bsize;
//     if (_syncnt == 0)
//     {
//         _syndel = del;
//         _syncnt++;
//     }
//     else if (_syncnt < 10)
//     {
//         _syndel += 0.25 * (del - _syndel);
//         _syncnt++;
//     }
//     else
//     {
//         _syndel += 0.025 * (del - _syndel);
//     }
//     if (_syncnt == 10)
//     {
//         k = (int)(_syndel - _delay);
//         if (abs (k) > 500) _audioq->rd_commit (-k);
//         _delay = _syndel;
//     }
// }
