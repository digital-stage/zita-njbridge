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


#ifndef __NETTX_H
#define __NETTX_H


#include <sys/types.h>
#include <pthread.h>
#include "pxthread.h"
#include "lfqueue.h"
#include "netdata.h"


class Nettx : public Pxthread
{
public:

    Nettx (void);
    virtual ~Nettx (void);
    
    void start (Lfq_packdata *packq,
		Lfq_timedata *timeq, 
		Netdata      *descpack,
		int           sockfd,
	        int           rtprio);

    void stop (void)
    {
	_stop = true;
	trigger ();
    }

    void trigger (void);

private:

    virtual void thr_main (void);

    Lfq_packdata    *_packq;
    Lfq_timedata    *_timeq; 
    Netdata         *_descpack;
    int              _sockfd;
    bool             _stop;
    Pxsema           _sema;
};


#endif
