# ----------------------------------------------------------------------------
#
#  Copyright (C) 2013-2018 Fons Adriaensen <fons@linuxaudio.org>
#    
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# ----------------------------------------------------------------------------


PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= /usr/share/man/man1

VERSION = 0.4.4
CPPFLAGS +=  -MMD -MP -DVERSION=\"$(VERSION)\"
CXXFLAGS += -O2 -Wall


all:	zita-j2n zita-n2j zita-njbridge.1.gz zita-j2n.1.gz zita-n2j.1.gz


ZITA-J2N_O = zita-j2n.o netdata.o jacktx.o nettx.o pxthread.o lfqueue.o zsockets.o
$(ZITA-J2N_O):
-include $(ZITA-J2N_O:%.o=%.d)
zita-j2n:	LDLIBS += -ljack -lpthread -lm
zita-j2n:	$(ZITA-J2N_O)
	$(CXX) $(LDFLAGS) -o $@ $(ZITA-J2N_O) $(LDLIBS)


ZITA-N2J_O = zita-n2j.o netdata.o jackrx.o netrx.o pxthread.o lfqueue.o zsockets.o syncrx.o
$(ZITA-N2J_O):
-include $(ZITA-N2J_O:%.o=%.d)
zita-n2j:	LDLIBS += -lzita-resampler -ljack -lpthread -lm
zita-n2j:	$(ZITA-N2J_O)
	$(CXX) $(LDFLAGS) -o $@ $(ZITA-N2J_O) $(LDLIBS)


zita-njbridge.1.gz:	zita-njbridge.1
	gzip -c zita-njbridge.1 > zita-njbridge.1.gz

zita-n2j.1.gz:	zita-n2j.1
	gzip -c zita-n2j.1 > zita-n2j.1.gz

zita-j2n.1.gz:	zita-j2n.1
	gzip -c zita-j2n.1 > zita-j2n.1.gz



install:	all
	install -d $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(MANDIR)
	install -m 755 zita-j2n $(DESTDIR)$(BINDIR)
	install -m 755 zita-n2j $(DESTDIR)$(BINDIR)
	install -m 644 zita-njbridge.1.gz $(DESTDIR)$(MANDIR)
	install -m 644 zita-j2n.1.gz $(DESTDIR)$(MANDIR)
	install -m 644 zita-n2j.1.gz $(DESTDIR)$(MANDIR)


uninstall:
	/bin/rm -f  $(DESTDIR)$(BINDIR)/zita-j2n
	/bin/rm -f  $(DESTDIR)$(BINDIR)/zita-n2j
	/bin/rm -f  $(DESTDIR)$(MANDIR)/zita-njbridge.1.gz
	/bin/rm -f  $(DESTDIR)$(MANDIR)/zita-j2n.1.gz
	/bin/rm -f  $(DESTDIR)$(MANDIR)/zita-n2j.1.gz


clean:
	/bin/rm -f *~ *.o *.a *.d *.so *.gz
	/bin/rm -f zita-n2j zita-j2n

