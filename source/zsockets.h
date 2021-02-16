// ----------------------------------------------------------------------------
//
//  Copyright (C) 2011-2016 Fons Adriaensen <fons@linuxaudio.org>
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


#ifndef __ZSOCKETS_H
#define __ZSOCKETS_H


#include <sys/types.h>
#include <netdb.h>


class Sockaddr
{
public:

    Sockaddr (int family = AF_UNSPEC);

    void reset (int family = AF_UNSPEC);
    int  set_addr (int family, int socktype, int protocol, const char *address);
    void set_port (int port);
    int  get_addr (char *address, int len) const;
    int  get_port (void) const;

    int   family  (void) const;
    bool  is_multicast (void) const; 

    // For use by socket library calls.
    sockaddr *sa_ptr (void) const { return (sockaddr *) _data; }
    int       sa_len (void) const;

private:

    char _data [sizeof (struct sockaddr_storage)];
};


extern int sock_open_active (Sockaddr *remote, Sockaddr *local); 
extern int sock_open_passive (Sockaddr *local, int qlen);
extern int sock_open_dgram (Sockaddr *remote, Sockaddr *local);
extern int sock_open_mcsend (Sockaddr *addr, const char *iface, int loop, int hops);
extern int sock_open_mcrecv (Sockaddr *addr, const char *iface);
extern int sock_accept (int fd, Sockaddr *remote, Sockaddr *local);
extern int sock_close (int fd);

extern int sock_set_close_on_exec (int fd, bool flag);
extern int sock_set_no_delay (int fd, bool flag);
extern int sock_set_write_buffer (int fd, size_t size);
extern int sock_set_read_buffer (int fd, size_t size);

extern int sock_write (int fd, void* data, size_t size, size_t min);
extern int sock_read (int fd, void* data, size_t size, size_t min);
extern int sock_sendto (int fd, void* data, size_t size, Sockaddr *addr);
extern int sock_recvfm (int fd, void* data, size_t size, Sockaddr *addr);


#endif
