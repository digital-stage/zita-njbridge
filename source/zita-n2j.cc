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


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <getopt.h>
#include <sys/mman.h>
#include "lfqueue.h"
#include "netdata.h"
#include "zsockets.h"
#include "netrx.h"
#include "syncrx.h"
#include "jackrx.h"


#define APPNAME "zita-n2j"


static Lfq_audio      *audioq = 0;
static Lfq_int32      *commq = 0;
static Lfq_timedata   *timeq = 0;
static Lfq_timedata   *syncq = 0;
static Lfq_infodata   *infoq = 0;
static bool stop = false;

static const char   *name_arg  = APPNAME;
static const char   *serv_arg  = 0;
static const char   *chan_arg  = "1,2";
static const char   *addr_arg  = 0;
static int           port_arg  = 0;
static const char   *dev_arg   = 0;
static int           buff_arg  = 10;
static int           sync_arg  = 0;
static int           filt_arg  = 0;
static bool          info_opt  = false;


static void help (void)
{
    fprintf (stderr, "\n%s-%s\n", APPNAME, VERSION);
    fprintf (stderr, "(C) 2013-2016 Fons Adriaensen  <fons@linuxaudio.org>\n");
    fprintf (stderr, "Receive audio from zita-j2n.\n\n");
    fprintf (stderr, "Usage: %s <options> ip-address ip-port \n", APPNAME);
    fprintf (stderr, "       %s <options> ip-address ip-port interface\n", APPNAME);
    fprintf (stderr, "Options:\n");
    fprintf (stderr, "  --help              Display this text\n");
    fprintf (stderr, "  --jname <name>      Jack client name [%s]\n", APPNAME);
    fprintf (stderr, "  --jserv <name>      Jack server name\n");
    fprintf (stderr, "  --chan  <list>      List of channels [%s]\n", chan_arg);
    fprintf (stderr, "  --buff  <time>      Additional buffering (ms) [%d]\n", buff_arg);
//    fprintf (stderr, "  --sync  <time>      Sync delay (ms) [%d]\n", sync_arg);
    fprintf (stderr, "  --filt  <delay>     Resampler filter delay [16..96]\n");
    fprintf (stderr, "  --info              Print additional info\n");
    exit (1);
}


enum { HELP, NAME, SERV, CHAN, BUFF, SYNC, FILT, INFO };


static struct option options [] = 
{
    { "help",  0, 0, HELP  },
    { "jname", 1, 0, NAME  },
    { "jserv", 1, 0, SERV  },
    { "chan",  1, 0, CHAN  },
    { "buff",  1, 0, BUFF  },
    { "sync",  1, 0, SYNC  },
    { "filt",  1, 0, FILT  },
    { "info",  0, 0, INFO  },
    { 0, 0, 0, 0 }
};


static int getint (const char *optname)
{
    int v;

    if (sscanf (optarg, "%d", &v) != 1)
    {
	fprintf (stderr, "Bad option argument: --%s %s\n", optname, optarg);
	exit (1);
    }
    return v;
}


static void procoptions (int ac, char *av [])
{
    int k;

    while ((k = getopt_long (ac, av, "", options, 0)) != -1)
    {
	switch (k)
	{
        case '?':
	case HELP:
	    help ();
	    break;
	case NAME:
	    name_arg = optarg;
	    break;
	case SERV:
	    serv_arg = optarg;
	    break;
	case CHAN:
	    chan_arg = optarg;
	    break;
	case BUFF:
	    buff_arg = getint ("buff");
	    break;
//	case SYNC:
//	    sync_arg = getint ("sync");
//	    break;
	case FILT:
	    filt_arg = getint ("filt");
	    break;
	case INFO:
	    info_opt = true;
	    break;
 	}
    }
    if (ac < optind + 2) help ();
    if (ac > optind + 3) help ();
    addr_arg = av [optind++];
    port_arg = atoi (av [optind++]);
    if (ac == optind + 1) dev_arg = av [optind];
}


int readlist (const char *s, int *list)
{
    // Parse channel list. This must be a string consisting
    // of decimal integers in strictly ascending order and
    // separated by ',' or '-', the latter denoting a range.

    int c, i, j, k, n;

    i = 0;
    k = 0;
    c = ',';
    while (c)
    {
	if (sscanf (s, "%d%n", &j, &n) != 1) return 0;
	if ((j <= i) || (j > Netdata::MAXCHAN)) return 0;  
	if      (c == ',') list [k++] = j - 1;
	else if (c == '-') while (++i <= j) list [k++] = i - 1;
	else return 0;
        i = j;
	s += n;
        do c = *s++; while (c && isblank (c));
    }
    list [k] = -1;
    return k;
}


static void sigint_handler (int)
{
    stop = true;
}


static bool checkstatus (void)
{
    int       c, n, m;
    double    e, r;
    Infodata *I;

    n = 0;
    m = 999999999;
    e = r = 0;
    while (infoq->rd_avail ())
    {
	I = infoq->rd_datap ();
	switch (I->_state)
	{
	case Jackrx::FATAL:
	    printf ("Fatal error, terminating.\n");
	    stop = true;
  	    infoq->rd_commit ();
	    return true;
	case Jackrx::TXEND:
	    printf ("Transmitter terminated.\n");
  	    infoq->rd_commit ();
	    return true;
	case Jackrx::WAIT:
	    printf ("Waiting for %3.1lf seconds...\n", I->_error);
  	    infoq->rd_commit ();
	    return false;
	case Jackrx::SYNC0:
            printf ("Syncing...\n");
	    break;
	case Jackrx::SYNC2:
            printf ("Receiving.\n");
	    break;
	}
	if (info_opt && (I->_state >= Jackrx::PROC1))
	{
	    n++;
	    e += I->_error;
	    r += I->_ratio;
            c = I->_syncc;
            if (m > I->_nfram) m = I->_nfram;
	}
	infoq->rd_commit ();
    }
    if (n) printf ("%3d %8.3lf %9.6lf %8d %3d\n", n, e / n, r / n, m, c);
    return false;
}


static int opensocket (Sockaddr *A)
{
    int fd = -1;

    if (A->is_multicast ())
    {
	if (dev_arg) fd = sock_open_mcrecv (A, dev_arg);
        else
	{
	    fprintf (stderr, "Multicast requires a network device.\n");
	    exit (1);
	}
    }
    else
    {
	if (dev_arg) fprintf (stderr, "Ignored extra argument '%s'.\n", dev_arg);
	fd = sock_open_dgram (0, A);
    }
    if (fd < 0)
    {
	fprintf (stderr, "Failed to open socket.\n");
	perror (0);
	exit (1);
    }
    return fd;
}


int main (int ac, char *av [])
{
    Sockaddr     Arx, Atx, Asy;
    int          sockfd1, sockfd2, nchan, fsamp, filt;
    int          tx_psmax, tx_nchan, tx_fsamp, tx_fsize;
    int          chlist [Netdata::MAXCHAN + 1];
    int          k, k_buf, k_del;
    double       t_tx, t_rx, t_buf, t_del;
    Netdata      *packet = 0;
    Jackrx       *jackrx = 0;
    Syncrx       *syncrx = 0;
    Netrx        *netrx = 0;
    char         s [256];

    procoptions (ac, av);
    nchan = readlist (chan_arg, chlist);
    if (nchan < 1)
    {
	fprintf (stderr, "Format error in channel list\n");
	exit (1);
    }
    if ((buff_arg < 0) || (buff_arg > 4000))
    {
	fprintf (stderr, "Buffer time is out of range.\n");
	exit (1);
    }
    if (sync_arg && (sync_arg > buff_arg))
    {
	fprintf (stderr, "Sync delay too high.\n");
	exit (1);
    }
    if (filt_arg && ((filt_arg < 16) || (filt_arg > 96)))
    {
	fprintf (stderr, "Filter delay is out of range.\n");
	exit (1);
    }
    if (   Arx.set_addr (AF_INET, SOCK_DGRAM, 0, addr_arg)
        || Asy.set_addr (AF_INET, SOCK_DGRAM, 0, addr_arg))
    {
	fprintf (stderr, "Address resolution failed.\n");
	exit (1);
    }
    if ((port_arg < 1) || (port_arg > 65535))
    {
	fprintf (stderr, "Port number is out of range.\n");
	exit (1);
    }

    Arx.set_port (port_arg);
    Asy.set_port (port_arg + 1);

    if (mlockall (MCL_CURRENT | MCL_FUTURE))
    {
        fprintf (stderr, "Warning: memory lock failed.\n");
    }

    packet = new Netdata (1500);
    jackrx = new Jackrx (name_arg, serv_arg, nchan, chlist);
//    syncrx = new Syncrx ();
    netrx  = new Netrx ();
    commq = new Lfq_int32 (16);
    timeq = new Lfq_timedata (256);
//    syncq = new Lfq_timedata (256);
    infoq = new Lfq_infodata (256);
    usleep (100000);

    while (! stop)
    {
        signal (SIGINT, SIG_DFL);
        sockfd1 = opensocket (&Arx);
        sockfd2 = opensocket (&Asy);
	printf ("Waiting for info packet...\n");
        while (true)
        {
	    if (sock_recvfm (sockfd1, packet->data (), packet->size (), &Atx) <= 0)
  	    {
  	        fprintf (stderr, "Fatal error on socket.\n");
	        sock_close (sockfd1);
	        sock_close (sockfd2);
		stop = true;
		break;
	    }
  	    if (packet->check_ptype () == Netdata::TY_ADESC)
	    {
                Atx.get_addr (s, 256);
 	        tx_psmax = packet->get_psmax ();
                tx_nchan = packet->get_nchan ();
                tx_fsamp = packet->get_fsamp ();
                tx_fsize = packet->get_fsize ();
                printf ("From %s : %d chan, %d Hz\n", s, tx_nchan, tx_fsamp);
	        break;
	    }
        }
	if (stop) break;

	fsamp = jackrx->fsamp ();
        t_tx = (double) tx_fsize / tx_fsamp;
        t_rx = (double) jackrx->bsize () / fsamp; 
        t_buf = t_tx + t_rx + 1e-3 * buff_arg;
        if (sync_arg) t_del = 1e-3 * sync_arg;    
        else          t_del = 1e-3 * buff_arg;
        k_buf = (int)(t_buf * tx_fsamp + 0.5);
	k_del = (int)(t_del * tx_fsamp + 0.5);
	for (k = 256; k < 2 * k_buf; k *= 2);
        audioq = new Lfq_audio (k, nchan);
	
	if (filt_arg) filt = filt_arg;
	else
	{
  	    k = (fsamp < tx_fsamp) ? fsamp : tx_fsamp;
	    if (k < 44100) k = 44100;
	    filt = (int)((6.7 * k) / (k - 38000));
	    if (filt < 16) filt = 16;
  	    printf ("Resampler filter delay is %d.\n", filt);
	}

//        if (sync_arg) syncrx->start (syncq, jackrx->rprio() + 5, sockfd2);

        netrx->start (audioq, commq, timeq, chlist, 
	   	      tx_psmax, tx_fsamp, tx_fsize, jackrx->rprio() + 5, sockfd1);

        jackrx->start (audioq, commq, timeq, syncq, infoq,
                       (double) jackrx->fsamp () / tx_fsamp, k_del, filt);

        signal (SIGINT, sigint_handler);
        while (! (stop || checkstatus ())) usleep (250000);

        sock_close (sockfd1);
        sock_close (sockfd2);
	usleep (100000);
        delete audioq;
    }

    delete commq;
    delete timeq;
    delete syncq;
    delete infoq;
    delete netrx;
    delete syncrx;
    delete jackrx;
    delete packet;

    return 0;
}
