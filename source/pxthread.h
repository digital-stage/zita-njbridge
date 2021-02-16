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


#ifndef __PXTHREAD_H
#define __PXTHREAD_H


// ----------------------------------------------------------------------------


#if defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__GNU__) || defined(__APPLE__)

// NOTE: __FreeBSD_kernel__  and __GNU__ were added by the Debian maintainers
// (the latter for the HURD version of Debian). Things are reported to work
// with some applications but probably have not been tested in depth.


#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>


class Pxthread
{
public:

    Pxthread (void);
    virtual ~Pxthread (void);
    Pxthread (const Pxthread&);
    Pxthread& operator=(const Pxthread&);

    virtual void thr_main (void) = 0;
    virtual int  thr_start (int policy, int priority, size_t stacksize = 0);

private:
  
    pthread_t  _thrid;
};


#endif


// ----------------------------------------------------------------------------


#if defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__GNU__)

class Pxsema
{
public:

    Pxsema (void) { init (0, 0); }
    ~Pxsema (void) { sem_destroy (&_sema); }

    Pxsema (const Pxsema&); // disabled
    Pxsema& operator= (const Pxsema&); // disabled

    int init (int s, int v) { return sem_init (&_sema, s, v); }
    int post (void) { return sem_post (&_sema); }
    int wait (void) { return sem_wait (&_sema); }
    int trywait (void) { return sem_trywait (&_sema); }

private:

    sem_t  _sema;
};

#define PXSEMA_IS_IMPLEMENTED
#endif


// ----------------------------------------------------------------------------


#ifdef __APPLE__


// NOTE:  ***** I DO NOT REPEAT NOT PROVIDE SUPPORT FOR OSX *****
// 
// The following code partially emulates the POSIX sem_t for which
// OSX has only a crippled implementation. It may or may not compile,
// and if it compiles it may or may not work correctly. Blame APPLE
// for not following POSIX standards.


#include <unistd.h>
#include <pthread.h>


class Pxsema
{
public:

    Pxsema (void) : _count (0)
    {
        init (0, 0);
    }

    ~Pxsema (void)
    {
        pthread_mutex_destroy (&_mutex);
        pthread_cond_destroy (&_cond);
    }

    Pxsema (const Pxsema&); // disabled
    Pxsema& operator= (const Pxsema&); // disabled

    int init (int s, int v)
    {
	_count = v;
        return pthread_mutex_init (&_mutex, 0) || pthread_cond_init (&_cond, 0);
    }

    int post (void)
    {
	pthread_mutex_lock (&_mutex);
	_count++;
	if (_count == 1) pthread_cond_signal (&_cond);
	pthread_mutex_unlock (&_mutex);
	return 0;
    }

    int wait (void)
    {
	pthread_mutex_lock (&_mutex);
	while (_count < 1) pthread_cond_wait (&_cond, &_mutex);
	_count--;
	pthread_mutex_unlock (&_mutex);
	return 0;
    }

    int trywait (void)
    {
	if (pthread_mutex_trylock (&_mutex)) return -1;
	if (_count < 1)
	{
	    pthread_mutex_unlock (&_mutex);
	    return -1;
	}
        _count--;
        pthread_mutex_unlock (&_mutex);
        return 0;
    }

private:

    int              _count;
    pthread_mutex_t  _mutex;
    pthread_cond_t   _cond;
};

#define PXSEMA_IS_IMPLEMENTED
#endif


// ----------------------------------------------------------------------------


#if defined(_WIN32) || defined(WIN32)

#include <windows.h>


#define usleep(t) Sleep ((t) / 1000);

#define SCHED_OTHER 0
#define SCHED_FIFO  1
#define SCHED_RR    2


// ---------------------------------------------------------------------
//
// Mapping of policy and priority pararmeters on Windows
//
// If the policy parameter is SCHED_FIFO or SCHED_RR then
//
//   priority parameter       Windows thread priority
//   ------------------------------------------------
//     < 60                   THREAD_PRIORITY_TIME_CRITICAL 
//     60..69                 MMCSS Pro Audio, LOW      
//     70..79                 MMCSS Pro Audio, NORMAL      
//     80..89                 MMCSS Pro Audio, HIGH      
//     >= 90                  MMCSS Pro Audio, CRITICAL      
//
// If the policy parameter is not SCHED_FIFO or SCHED_RR then
// the default thread priority is used.
//
// ---------------------------------------------------------------------


class Pxthread
{
public:

    Pxthread (void);
    virtual ~Pxthread (void);
    Pxthread (const Pxthread&);
    Pxthread& operator=(const Pxthread&);

    void *handle (void) { return _handle; }
    int priority (void) { return _prior; } 

    virtual void thr_main (void) = 0;
    int thr_start (int policy, int priority, size_t stacksize = 0);

    void mmcssprio (void);

private:
 
    void          *_handle;
    unsigned long  _thrid;
    int            _prior;


    static unsigned long  _index;
};


class Pxsema
{
public:

    Pxsema (void) { _handle = CreateSemaphore (0, 0, 999999999, 0); }
    ~Pxsema (void) { CloseHandle (_handle); }

    Pxsema (const Pxsema&); // disabled
    Pxsema& operator= (const Pxsema&); // disabled

//    int init (void) { return 0; }
    int post (void) { return ReleaseSemaphore (_handle, 1, 0) ? 0 : -1; }
    int wait (void) { return WaitForSingleObject (_handle, INFINITE); }
    int trywait (void) { return WaitForSingleObject (_handle, 0); }

private:

    void  *_handle;	
};


#define PXSEMA_IS_IMPLEMENTED
#endif


// ----------------------------------------------------------------------------


#ifndef PXSEMA_IS_IMPLEMENTED
#error "The Pxsema class is not implemented."
#endif


#endif
