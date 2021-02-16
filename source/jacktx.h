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


#ifndef __JACKTX_H
#define __JACKTX_H


#include <jack/jack.h>
#include "lfqueue.h"
#include "netdata.h"
#include "nettx.h"


class Jacktx
{
public:

    enum { INIT, SEND, SUSP, TERM };


    Jacktx (const char  *jname, const char *jserv, int nchan);
    virtual ~Jacktx (void);
    
    void start (Lfq_packdata *packq,
		Lfq_timedata *timeq,
                Lfq_int32    *infoq,
		Nettx        *nettx,
		int           sform,
		int           npack);

    const char *jname (void) const { return _jname; }
    int fsamp (void) const { return _fsamp; }
    int bsize (void) const { return _bsize; }
    int rprio (void) const { return _rprio; }

private:

    void init (const char *jname, const char *jserv);
    void fini (void);
    void report (int state);

    virtual void thr_main (void) {}

    void jack_buffsize (int bsize);
    void jack_freewheel (int freew);
    void jack_latency (jack_latency_callback_mode_t jlcm);
    int  jack_process (int nframes);


    jack_client_t  *_client;
    jack_port_t    *_ports [Netdata::MAXCHAN];
    const char     *_jname;
    int             _rprio;
    int             _nchan;
    int             _fsamp;
    int             _bsize;
    int             _state;
    int             _freew;
    int             _sform;
    int             _npack;
    int             _count;
    int             _tscnt;
    bool            _first;
    jack_time_t     _tnext;
    Lfq_packdata   *_packq;
    Lfq_timedata   *_timeq;
    Lfq_int32      *_infoq;
    Nettx          *_nettx;


    static void jack_static_shutdown (void *arg);
    static int  jack_static_buffsize (jack_nframes_t nframes, void *arg);
    static void jack_static_freewheel (int state, void *arg);
    static void jack_static_latency (jack_latency_callback_mode_t jlcm, void *arg);
    static int  jack_static_process (jack_nframes_t nframes, void *arg);
};


#endif
