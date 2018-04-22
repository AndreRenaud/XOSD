PREFIX=/opt/tmp/xosd
CC=gcc
VERSION=0.3.0
CFLAGS=$(GTK) $(XMMS) -fPIC -Wall -pipe -I. -DXOSD_VERSION=\"$(VERSION)\" -I/usr/X11R6/include
LFLAGS=-L. -lxosd
SOLFLAGS=-L/usr/X11R6/lib -lXext -lpthread -lXt -lX11
SOURCES=AUTHORS CHANGELOG README LICENSE Makefile testprog.c xosd.c xosd.h xmms_osd.c

#default: testprog libxmms_osd.so osd_cat
default:  all libxosd.sl libxmms_osd.sl 

%.o: %.c
	$(CC) -c -o $@ $<  $(CFLAGS)


all:	
	SYS=`uname -r`; echo $$SYS;\
	if test "$$SYS" = "B.10.20";\
	then \
        $(CC)  $(CFLAGS) -c -o xosd1020.o xosd1020.c; \
        ar cr libxosd.a xosd1020.o; \
        ld -b -s -o libxosd.sl xosd1020.o; \
        echo "10.20 Libs done"; \
        exit;\
        else \
        echo "Normal Pthread build for 11.00 ...";\
        fi      



#	echo "No Build";exit;\
 	

libxosd.sl: 
	SYS=`uname -r`; echo $$SYS;\
	if test "$$SYS" = "B.11.00";\
	then \
	echo "Making 11.00 Libs";\
	$(CC)  $(CFLAGS) -c -o xosd.o xosd.c;ar cr libxosd.a xosd.o; ld -b -s -o libxosd.sl xosd.o;\
	else \
	echo "No 10.20 Libs";\
	fi 

GTK =-I/opt/gtk+/include -I/opt/glib/lib/glib/include -I/opt/glib/include -I/usr/include/X11R6
XMMS =-I/opt/xmms/include -I/opt/xmms/include/xmms -I/opt/gtk+/include -I/opt/glib/lib/glib/include -I/opt/glib/include -D_REENTRANT -I/usr/include/X11R6

#Only go through with this on 11.00 Systems ...
libxmms_osd.sl:
	SYS=`uname -r`; echo $$SYS;\
	if test "$$SYS" = "B.11.00";\
	then \
	echo "Making XMMS Plugin Library";\
	$(CC)  $(CFLAGS) -c -o xmms_osd.o xmms_osd.c;ar cr libxmms_osd.a xmms_osd.o;ld -b -s -o libxmms_osd.sl xmms_osd.o;\
	else \
	echo "No XMMS Plugin";\
	fi
	
testprog: testprog.o libxosd.sl
	$(CC) -o $@ testprog.o $(SOLFLAGS) -lrt libxosd.a

#You still need to link libxosd.a for xosd_init(), etc.
#<stephie>
testprog2: testprog.o libxmms_osd.sl
	$(CC) -o $@ testprog.o libxmms_osd.a libxosd.a $(SOLFLAGS) -lrt




#This just seems rather confused to me ...
#<stephie>
#xmms_osd: xmms_osd.o 
#	$(CC) -shared -o $@ $+ `/opt/gtk+/bin/gtk-config --libs` $(LFLAGS)
#	ld -b -s -o libxmms_xosd.sl xmms_osd.o  $(SOLFLAGS) $+

#You dont include this obj in either of the libs (you get duplicate symbols)
#<stephie>
#osd_cat.o: xosd.c
#	$(CC) -c xosd.c -o osd_cat.o  $(CFLAGS)

#This Supposed to produced a binary?? No main() found!?
#<stephie>
#osd_cat:  osd_cat.o
#	$(CC) -o $@ osd_cat.o $(LFLAGS) -lrt

tar: xosd-$(VERSION).tar.gz

xosd-$(VERSION).tar.gz:
	ln -s . xosd-$(VERSION)
	tar cfz $@ $(patsubst %, xosd-$(VERSION)/%, $(SOURCES))
	rm xosd-$(VERSION)

bindir=/opt/xosd/bin
libdir=/opt/xosd/lib

install:
	test -d $(bindir) || mkdirhier $(bindir)
	bsdinst -c  -m 0644 testprog.c $(bindir)
	test -d $(libdir) || mkdirhier $(libdir)
	bsdinst -c  -m 0644 libxosd.a $(libdir)/lib
	bsdinst -c  -m 0755 libxosd.sl $(libdir)/lib
	test -d $(libdir)/Plugins/General || mkdirhier $(libdir)/Plugins/General
	test -z libxmms_osd.a || touch  libxmms_osd.a libxmms_osd.sl
	bsdinst -c  -m 0644 libxmms_osd.a $(libdir)/Plugins/General
	bsdinst -c  -m 0755 libxmms_osd.sl $(libdir)/Plugins/General
	@echo "Copy libxmms_osd to $(HOME)/.xmms/Plugins/General"
#	rm -f $(HOME)/.xmms/Plugins/General/libxmms_osd.so
#	cp -a libxmms_osd.so $(HOME)/.xmms/Plugins/General
#	cp -a osd_cat $(PREFIX)/bin

clean:
	rm -f *~ *.o xosd testprog libxosd.sl libxmms_osd.a libxmms_osd.sl libxosd.a osd_cat
	rm -f xosd-$(VERSION).tar.gz
	
