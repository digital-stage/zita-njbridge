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
#include <signal.h>
#include <getopt.h>
#include <math.h>
#include <sys/mman.h>
#include "jacktx.h"
#include "nettx.h"
#include "lfqueue.h"
#include "netdata.h"
#include "zsockets.h"


#define APPNAME "zita-j2n"


static Lfq_packdata  *packq = 0;
static Lfq_timedata  *timeq = 0;
static Lfq_int32     *infoq = 0;
static Netdata        descpack (40);
static volatile bool  stop = false;


static const char   *name_arg  = APPNAME;
static const char   *serv_arg  = 0;
static int           chan_arg  = 2;
static const char   *addr_arg  = 0;
static int           port_arg  = 0;
static const char   *dev_arg   = 0;
static int           mtu_arg   = 1500;
static int           hops_arg  = 1;
static int           form_arg  = Netdata::FM_24BIT;


static void help (void)
{
    fprintf (stderr, "\n%s-%s\n", APPNAME, VERSION);
    fprintf (stderr, "(C) 2013-2016 Fons Adriaensen  <fons@linuxaudio.org>\n");
    fprintf (stderr, "Send audio to zita-n2j.\n\n");
    fprintf (stderr, "Usage: %s <options> ip-address ip-port \n", APPNAME);
    fprintf (stderr, "       %s <options> ip-address ip-port interface\n", APPNAME);
    fprintf (stderr, "Options:\n");
    fprintf (stderr, "  --help              Display this text\n");
    fprintf (stderr, "  --jname <name>      Jack client name [%s]\n", APPNAME);
    fprintf (stderr, "  --jserv <name>      Jack server name\n");
    fprintf (stderr, "  --chan  <nchan>     Number of channels [%d]\n", chan_arg);
    fprintf (stderr, "  --16bit             Send 16-bit samples\n");
    fprintf (stderr, "  --24bit             Send 24-bit samples (default)\n");
    fprintf (stderr, "  --float             Send floating point samples\n");
    fprintf (stderr, "  --mtu   <size>      Maximum packet size [%d]\n", mtu_arg);
    fprintf (stderr, "  --hops  <hops>      Number of hops for multicast [%d]\n", hops_arg);
    exit (1);
}


enum { HELP, NAME, SERV, CHAN, BIT16, BIT24, FLT32, MTU, HOPS };


static struct option options [] = 
{
    { "help",  0, 0, HELP  },
    { "jname", 1, 0, NAME  },
    { "jserv", 1, 0, SERV  },
    { "chan",  1, 0, CHAN  },
    { "mtu",   1, 0, MTU   },
    { "hops",  1, 0, HOPS  },
    { "16bit", 0, 0, BIT16 },
    { "24bit", 0, 0, BIT24 },
    { "float", 0, 0, FLT32 },
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
	    chan_arg = getint ("chan");
	    break;
	case MTU:
	    mtu_arg = getint ("mtu");
	    break;
	case HOPS:
	    hops_arg = getint ("hops");
	    break;
	case BIT16:
	    form_arg = Netdata::FM_16BIT;
	    break;
	case BIT24:
	    form_arg = Netdata::FM_24BIT;
	    break;
        case FLT32:
	    form_arg = Netdata::FM_FLOAT;
	    break;
 	}
    }
    if (ac < optind + 2) help ();
    if (ac > optind + 3) help ();
    addr_arg = av [optind++];
    port_arg = atoi (av [optind++]);
    if (ac == optind + 1) dev_arg = av [optind];
}


static void siginthandler (int)
{
    signal (SIGINT, SIG_IGN);
    stop = true;
}


static void checkstatus (void)
{
    int state;

    while (infoq->rd_avail ())
    {
	state = infoq->rd_int32 ();
	switch (state)
	{
	case Jacktx::TERM:
	    printf ("Fatal error condition, terminating.\n");
	    stop = true;
	    return;
	}
    }
}


static int opensocket (Sockaddr *A)
{
    int fd = -1;

    if (A->is_multicast ())
    {
	if (dev_arg) fd = sock_open_mcsend (A, dev_arg, 1, hops_arg);
        else
	{
	    fprintf (stderr, "Multicast requires a network device.\n");
	    exit (1);
	}
    }
    else
    {
	if (dev_arg) fprintf (stderr, "Ignored extra argument '%s'.\n", dev_arg);
	fd = sock_open_dgram (A, 0);
    }
    if (fd < 0)
    {
	fprintf (stderr, "Failed to open socket.\n");
	exit (1);
    }
    return fd;
}



int main (int ac, char *av [])
{
    Sockaddr        A;
    int             sockfd, psize, ppper, npack;
    Jacktx         *jacktx = 0;
    Nettx          *nettx = 0;

    procoptions (ac, av);

    if ((chan_arg < 1) || (chan_arg > Netdata::MAXCHAN))
    {
	fprintf (stderr, "Number of channels is out of range.\n");
	exit (1);
    }
    if (A.set_addr (AF_UNSPEC, SOCK_DGRAM, 0, addr_arg))
    {
	fprintf (stderr, "Address resolution failed.\n");
	exit (1);
    }
    if ((port_arg < 1) || (port_arg > 65535))
    {
	fprintf (stderr, "Port number is out of range.\n");
	exit (1);
    }
    A.set_port (port_arg);

    if (mlockall (MCL_CURRENT | MCL_FUTURE))
    {
        fprintf (stderr, "Warning: memory lock failed.\n");
    }

    jacktx = new Jacktx (name_arg, serv_arg, chan_arg);
    nettx  = new Nettx;
    usleep (100000);

    sockfd = opensocket (&A);
    psize = mtu_arg - ((A.family () == AF_INET6) ? 48 : 28);
    ppper = Netdata::packetsperperiod (psize, jacktx->bsize (), form_arg, chan_arg);
    npack = ppper * (int)(ceil (0.05 * jacktx->fsamp () / jacktx->bsize ()));
    packq = new Lfq_packdata (npack, psize);
    timeq = new Lfq_timedata (4);
    infoq = new Lfq_int32 (16);

    descpack.init_audio_desc (0, form_arg, chan_arg, psize, jacktx->fsamp (), jacktx->bsize ());
    nettx->start (packq, timeq, &descpack, sockfd, jacktx->rprio () + 5);
    jacktx->start (packq, timeq, infoq, nettx, form_arg, ppper);

    signal (SIGINT, siginthandler);
    while (! stop)
    {
	usleep (500000);
        nettx->trigger ();
	checkstatus ();
    }

    nettx->stop ();
    usleep (100000);
    delete jacktx;
    delete nettx;
    delete packq;
    delete timeq;
    delete infoq;

    return 0;
}
