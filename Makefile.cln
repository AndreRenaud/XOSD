PREFIX=/usr/local
CC=gcc
VERSION=0.3.0
CFLAGS=-g -Wall -pipe -I. -DXOSD_VERSION=\"$(VERSION)\" -I/usr/X11R6/include
LFLAGS=-L. -lxosd
SOLFLAGS=-L/usr/X11R6/lib -lX11 -lXext -lpthread -lXt
SOURCES=AUTHORS CHANGELOG README LICENSE Makefile testprog.c xosd.c xosd.h xmms_osd.c

default: testprog libxmms_osd.so osd_cat

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
	
libxosd.so: xosd.o
	$(CC) -shared -o $@ xosd.o $(SOLFLAGS)
	
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
	cp -a libxosd.so $(PREFIX)/lib
	mkdir -p $(HOME)/.xmms/Plugins/General
	rm -f $(HOME)/.xmms/Plugins/General/libxmms_osd.so
	cp -a libxmms_osd.so $(HOME)/.xmms/Plugins/General
	cp -a osd_cat $(PREFIX)/bin

clean:
	rm -f *~ *.o xosd testprog libxosd.so libxmms_osd.so libxosd.a osd_cat
	rm -f xosd-$(VERSION).tar.gz
	
