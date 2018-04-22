#
# Makefile for X-on-screen display
#

LIBRARY_VERSION=0
VERSION=0.6.0

PREFIX=/usr/local
EXEC_PREFIX=$(PREFIX)
BINDIR=$(EXEC_PREFIX)/bin
LIBDIR=$(EXEC_PREFIX)/lib
MANDIR=$(PREFIX)/man
INCLUDEDIR=$(PREFIX)/include
XMMS_PLUGINDIR=$(HOME)/.xmms

CC=gcc
INSTALL=/usr/bin/install -c
INSTALL_DATA=${INSTALL} -m 644

CFLAGS=-g -Wall -pipe -I. -DXOSD_VERSION=\"$(VERSION)\" -I/usr/X11R6/include -fPIC
LDFLAGS=-L. -lxosd -fpic
SOLFLAGS=-L/usr/X11R6/lib -lX11 -lXext -lpthread -lXt -fPIC
SOURCES=AUTHORS CHANGELOG README LICENSE Makefile testprog.c xosd.c xosd.h \
	xmms_osd.c osd_cat.c xosd.3 osd_cat.1

all: testprog libxmms_osd.so osd_cat

libxosd.so: xosd.o
	$(CC) -shared -o $@ xosd.o $(SOLFLAGS) \
		-Wl,-soname,libxosd.so.$(LIBRARY_VERSION)

xmms_osd.o: xmms_osd.c
	$(CC) -c -o $@ xmms_osd.c `gtk-config --cflags` `xmms-config --cflags` $(CFLAGS)

libxmms_osd.so: xmms_osd.o
	$(CC) -shared -o $@ $+ `gtk-config --libs` $(LDFLAGS)

testprog: testprog.o libxosd.so

osd_cat: libxosd.so osd_cat.o

tar: xosd-$(VERSION).tar.gz

xosd-$(VERSION).tar.gz:
	ln -s . xosd-$(VERSION)
	tar cfz $@ $(patsubst %, xosd-$(VERSION)/%, $(SOURCES))
	rm xosd-$(VERSION)

install: all
	$(INSTALL) libxosd.so $(LIBDIR)/libxosd.so.$(LIBRARY_VERSION)
	rm $(PREFIX)/lib/libxosd.so
	ln -s $(PREFIX)/lib/libxosd.so.$(LIBRARY_VERSION) $(PREFIX)/lib/libxosd.so
	mkdir -p $(XMMS_PLUGINDIR)/Plugins/Visualization
	rm -f $(XMMS_PLUGINDIR)/Plugins/Visualization/libxmms_osd.so
	cp -a libxmms_osd.so $(HOME)/.xmms/Plugins/Visualization
	$(INSTALL) osd_cat $(BINDIR)
	mkdir -p $(MANDIR)/man1 $(MANDIR)/man3
	$(INSTALL_DATA) osc_cat.1 $(MANDIR)/man1/
	$(INSTALL_DATA) xosd.3 $(MANDIR)/man3/

clean:
	rm -f *~ *.o xosd testprog libxosd.so libxmms_osd.so libxosd.a osd_cat
	rm -f xosd-$(VERSION).tar.gz

.PHONY: all tar clean install
# vim: noexpandtab
