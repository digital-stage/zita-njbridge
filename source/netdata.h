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


#ifndef __NETDATA_H
#define __NETDATA_H


#include <stdint.h>
 
// This class defines the encoding of network packets.
// All data uses network byte order. 
//
class Netdata
{
public:

    Netdata (int size);
    ~Netdata (void);

    friend class Netrx;
    
    enum { MAXCHAN = 64 };
    enum
    {
        FM_16BIT,
        FM_24BIT,
        FM_FLOAT,
    };
    enum
    {
        TY_ADESC,  // Audio descriptor packet.
        TY_ADATA   // Audio sample data packet.
    };
    enum
    {
        FL_TIMED  = 0x01, // Valid dtime field, start of period.
        FL_SUSP   = 0x02, // Transmission is suspended.
	FL_SKIP   = 0x04, // Token packet for skipped frames.
        FL_TERM   = 0x80  // Sender terminates.
    };

    unsigned char *data (void) const { return _data; }
    int size (void) const { return _size; } // Allocated size, normally MTU.
    int dlen (void) const { return _dlen; } // Used size in bytes.

    void init_audio_desc (int flags, int sform, int nchan, int psmax, int fsamp, int fsize);
    void init_audio_data (int flags, int sform, int nchan, int count, int nfram, int dtime);
    void set_flags (int flags) { _data [FLAGS] = flags; }
    void set_tmark (int32_t tfcnt, uint32_t tsecs, uint32_t tfrac);

    int check_ptype (void) const;
    int get_ptype (void) const { return _data [PTYPE]; }   // Packet type (TY_xxx)
    int get_flags (void) const { return _data [FLAGS]; }   // Various flags (FL_xxx)
    int get_sform (void) const { return _data [SFORM]; }   // Sample format (FM_xxx)
    int get_nchan (void) const { return _data [NCHAN]; }   // Number of channels.
    int get_psmax (void) const { return getint (PSMAX); }  // Maximum packet size.
    int get_fsamp (void) const { return getint (FSAMP); }  // Sender sample frequency.
    int get_fsize (void) const { return getint (FSIZE); }  // Sender period size.
    int get_tfcnt (void) const { return getint (TFCNT); }  // Timestamp frame count.
    int get_tsecs (void) const { return getint (TSECS); }  // Timestamp NTP seconds.
    int get_tfrac (void) const { return getint (TFRAC); }  // Timestamp NTP fraction.
    int get_count (void) const { return getint (COUNT); }  // Frame count, used to check continuity.
    int get_nfram (void) const { return getint (NFRAM); }  // Number of frames in this packet.
    int get_dtime (void) const { return getint (DTIME); }  // Transmit delay in usecs.  

    void put_audio (int chan, int offs, int nsamp, const float *adata, int astep);
    void get_audio (int chan, int offs, int nsamp, float *adata, int astep) const;

    static int packetsperperiod (int maxsize, int period, int sform, int nchan);

private:

    // Byte offsets.
    enum
    {
	// All packets
	PTYPE = 4,
	FLAGS = 5,

	// All audio packets
	SFORM = 6,
	NCHAN = 7,

	// Descriptor packet
	PSMAX = 8,          
	FSAMP = 12,
	FSIZE = 16,
	TFCNT = 20,
	TSECS = 24,
	TFRAC = 28,
	DPEND = 32,

	// Sample data packet
	COUNT = 8,
	NFRAM = 12,
	DTIME = 16,
	ADATA = 20
    };

    void init_header (int ptype, int flags, int sform, int nchan);

    // Used for header fields, always big-endian.
    // Put a 32-bit integer.
    void putint (int k, int32_t v)
    {
	_data [k++] = v >> 24;
	_data [k++] = v >> 16;
	_data [k++] = v >> 8;
	_data [k++] = v;
    }
    // Get a 32-bit integer.
    int32_t getint (int k) const
    {
	int32_t v;
	v  = _data [k++] << 24;
	v += _data [k++] << 16;
	v += _data [k++] << 8;
	v += _data [k++];
	return v;
    }

    int             _size;  // Allocated size.
    int             _dlen;  // Used size.
    unsigned char  *_data;
};


#endif
