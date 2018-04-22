#
# Makefile for X-on-screen display
#

LIBRARY_VERSION=0
VERSION=0.7.0

PREFIX=/usr/local
EXEC_PREFIX=$(PREFIX)
BINDIR=$(EXEC_PREFIX)/bin
LIBDIR=$(EXEC_PREFIX)/lib
MANDIR=$(PREFIX)/man
INCLUDEDIR=$(PREFIX)/include
XMMS_PLUGINDIR=$(HOME)/.xmms

CC=gcc
INSTALL=/usr/bin/install -c
INSTALL_DATA=$(INSTALL) -m 644

CFLAGS=-O2 -Wall -pipe -I. -DXOSD_VERSION=\"$(VERSION)\" -I/usr/X11R6/include
LDFLAGS=-L. -L/usr/X11R6/lib -lX11 -lXext -lpthread -lXt

XOSDLIBS=-lxosd

SOURCES=NEWS AUTHORS ChangeLog README COPYING Makefile testprog.c xosd.c \
	xosd.h xmms_osd.c osd_cat.c xosd.3 osd_cat.1

ARFLAGS=cru

all: testprog libxosd.a libxosd.so libxmms_osd.so osd_cat

%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

%.o.pic: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@ -fPIC

libxosd.so: xosd.o.pic
	$(CC) -shared -fPIC -o $@ $+ $(LDFLAGS) \
		-Wl,-soname,libxosd.so.$(LIBRARY_VERSION)

libxosd.a: xosd.o
	$(AR) $(ARFLAGS) libxosd.a  $+
	ranlib libxosd.a

xmms_osd.o: xmms_osd.c
	$(CC) -c -o $@ xmms_osd.c `xmms-config --cflags` -I/usr/include/gtk-1.2 -I/usr/include/glib-1.2 $(CFLAGS)

libxmms_osd.so: xmms_osd.o 
	$(CC) -shared -o $@ $+ $(LDFLAGS) `gtk-config --libs` $(XOSDLIBS)

testprog: testprog.o libxosd.so
	$(CC) -o $@ testprog.o $(LDFLAGS) $(XOSDLIBS)

osd_cat: libxosd.so osd_cat.o
	$(CC) -o $@ osd_cat.o $(LDFLAGS) $(XOSDLIBS)

tar: xosd-$(VERSION).tar.gz

xosd-$(VERSION).tar.gz:
	ln -s . xosd-$(VERSION)
	tar cfz $@ $(patsubst %, xosd-$(VERSION)/%, $(SOURCES))
	rm xosd-$(VERSION)

install: all
	$(INSTALL) libxosd.so $(LIBDIR)/libxosd.so.$(LIBRARY_VERSION)
	rm -f $(PREFIX)/lib/libxosd.so
	ln -s $(PREFIX)/lib/libxosd.so.$(LIBRARY_VERSION) $(PREFIX)/lib/libxosd.so
	mkdir -p $(XMMS_PLUGINDIR)/Plugins/General
	rm -f $(XMMS_PLUGINDIR)/Plugins/General/libxmms_osd.so
	cp -a libxmms_osd.so $(XMMS_PLUGINDIR)/Plugins/General
	$(INSTALL) osd_cat $(BINDIR)
	mkdir -p $(MANDIR)/man1 $(MANDIR)/man3
	$(INSTALL_DATA) osd_cat.1 $(MANDIR)/man1/
	$(INSTALL_DATA) xosd.3 $(MANDIR)/man3/

clean:
	rm -f *~ *.o *.o.pic xosd testprog libxosd.so libxmms_osd.so libxosd.a osd_cat
	rm -f xosd-$(VERSION).tar.gz

.PHONY: all tar clean install
# vim: noexpandtab
