LIBRARY_VERSION=0
PREFIX=/usr/local
CC=gcc
VERSION=0.5.0
CFLAGS=-g -Wall -pipe -I. -DXOSD_VERSION=\"$(VERSION)\" -I/usr/X11R6/include -fPIC
LFLAGS=-L. -lxosd -fpic
SOLFLAGS=-L/usr/X11R6/lib -lX11 -lXext -lpthread -lXt -fPIC
SOURCES=AUTHORS CHANGELOG README LICENSE Makefile testprog.c xosd.c xosd.h \
	xmms_osd.c osd_cat.c xosd.3

default: testprog libxmms_osd.so osd_cat

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
	
libxosd.so: xosd.o
	$(CC) -shared -o $@ xosd.o $(SOLFLAGS) \
		-Wl,-soname,libxosd.so.$(LIBRARY_VERSION)
	
testprog: testprog.o libxosd.so
	$(CC) -o $@ testprog.o $(LFLAGS)

xmms_osd.o: xmms_osd.c
	$(CC) -c -o $@ xmms_osd.c `gtk-config --cflags` `xmms-config --cflags` $(CFLAGS)

libxmms_osd.so: xmms_osd.o 
	$(CC) -shared -o $@ $+ `gtk-config --libs` $(LFLAGS)

osd_cat: libxosd.so osd_cat.o
	$(CC) -o $@ osd_cat.o $(LFLAGS)

tar: xosd-$(VERSION).tar.gz

xosd-$(VERSION).tar.gz:
	ln -s . xosd-$(VERSION)
	tar cfz $@ $(patsubst %, xosd-$(VERSION)/%, $(SOURCES))
	rm xosd-$(VERSION)

install: default
	cp -a libxosd.so $(PREFIX)/lib/libxosd.so.$(LIBRARY_VERSION)
	rm $(PREFIX)/lib/libxosd.so
	ln -s $(PREFIX)/lib/libxosd.so.$(LIBRARY_VERSION) $(PREFIX)/lib/libxosd.so
	mkdir -p $(HOME)/.xmms/Plugins/Visualization
	rm -f $(HOME)/.xmms/Plugins/Visualization/libxmms_osd.so
	cp -a libxmms_osd.so $(HOME)/.xmms/Plugins/Visualization
	cp -a osd_cat $(PREFIX)/bin
	mkdir -p $(PREFIX)/man/man3
	cp -a xosd.3 $(PREFIX)/man/man3

clean:
	rm -f *~ *.o xosd testprog libxosd.so libxmms_osd.so libxosd.a osd_cat
	rm -f xosd-$(VERSION).tar.gz
	
