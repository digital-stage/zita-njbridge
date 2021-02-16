// ----------------------------------------------------------------------------
//
//  Copyright (C) 2013-2015 Fons Adriaensen <fons@linuxaudio.org>
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
#include "zsockets.h"
#include "timers.h"
#include "pxthread.h"


#define APPNAME "synctest"


int main (int ac, char *av [])
{
    Sockaddr        A;
    int             pn, fd, del;
    char            data [32];
    uint32_t        s, f;
    
    if (ac < 4) exit (1);
    
    if (A.set_addr (AF_UNSPEC, SOCK_DGRAM, 0, av [1]))
    {
	fprintf (stderr, "Address resolution failed.\n");
	exit (1);
    }
    pn = atoi (av [2]);
    if ((pn < 1000) || (pn > 65535))
    {
	fprintf (stderr, "Port number is out of range.\n");
	exit (1);
    }
    A.set_port (pn);

    fd = sock_open_dgram (&A, 0);
    if (fd < 0)
    {
	fprintf (stderr, "Failed to open socket.\n");
	exit (1);
    }

    del = atoi (av [3]);
    while (true)
    {
	tntp_now (&s, &f, -1e-3 * del);
	strcpy (data + 0, "/time");
	strcpy (data + 8, ",ii");
	(* (uint32_t *)(data + 12)) = htonl (s);	
	(* (uint32_t *)(data + 16)) = htonl (f);	
	send (fd, data, 20, 0);
	usleep (250000);
    }

    sock_close (fd);
    return 0;
}
