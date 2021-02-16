// ----------------------------------------------------------------------------
//
//  Copyright (C) 2010-2016 Fons Adriaensen <fons@linuxaudio.org>
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
#include "pxthread.h"


// ----------------------------------------------------------------------------


#if defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__GNU__) || defined(__APPLE__)


Pxthread::Pxthread (void)
{
}


Pxthread::~Pxthread (void)
{
}


extern "C" void *Pxthread_entry_point (void *arg)
{
    Pxthread *T = (Pxthread *) arg;
    T->thr_main ();
    return NULL;
}


int Pxthread::thr_start (int policy, int priority, size_t stacksize)
{
    int                min, max, rc;
    pthread_attr_t     attr;
    struct sched_param parm;

    min = sched_get_priority_min (policy);
    max = sched_get_priority_max (policy);
    if (priority > max) priority = max;
    if (priority < min) priority = min;
    parm.sched_priority = priority;

    pthread_attr_init (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setschedpolicy (&attr, policy);
    pthread_attr_setschedparam (&attr, &parm);
    pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setinheritsched (&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setstacksize (&attr, stacksize);

    rc = pthread_create (&_thrid,
			 &attr,
			 Pxthread_entry_point,
			 this);

    pthread_attr_destroy (&attr);

    return rc;
}


#endif


// ----------------------------------------------------------------------------


#if defined(_WIN32) || defined(WIN32)


#include <windows.h>
#include <Avrt.h>


unsigned long Pxthread::_index = 0; // for MMCSS


Pxthread::Pxthread (void):
    _handle (0),	 
    _thrid (0),
    _prior (0)
{
}


Pxthread::~Pxthread (void)
{
}


void Pxthread::mmcssprio (void)
{
    int    p;
    void   *H;

    H = AvSetMmThreadCharacteristics ("Pro Audio", &_index);
    if (H == 0)
    {
        fprintf (stderr, "AvSetMmThreadCharacteristics() failed\n");
        return;
    }
    p = (_prior - 60) / 10;
    if (p > 3) p =  3;
    if (AvSetMmThreadPriority (H, (AVRT_PRIORITY)(p - 1)) == 0)
    {
        fprintf (stderr, "AvSetMmThreadPriority() failed\n");
    }
}


extern "C" unsigned long __stdcall Pxthread_entry_point (void *arg)
{
    Pxthread *T = (Pxthread *) arg;
    if (T->priority () >= 60) T->mmcssprio ();
    T->thr_main ();
    CloseHandle (T->handle ());
    return 0;
}


int Pxthread::thr_start (int policy, int priority, size_t stacksize)
{
    _thrid = 0; 
    if (policy == SCHED_FIFO || policy == SCHED_RR)
    {
        _prior = priority;
        if (_prior < 0) _prior = 0;
    }
    else _prior = -1;
    _handle = CreateThread (0,
                            stacksize,
                            Pxthread_entry_point, 
                            this,
                            0,
                            &_thrid);
    if (_handle && (_prior >= 0) && (_prior < 60))
    {
        SetThreadPriority (_handle, THREAD_PRIORITY_TIME_CRITICAL);
        return 0;
    }
    return 1;
}


#endif


// ----------------------------------------------------------------------------

