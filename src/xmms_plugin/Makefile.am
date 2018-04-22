# XMMS library

AM_CFLAGS=@XMMS_CFLAGS@ @GTK_CFLAGS@ @GDK_PIXBUF_CFLAGS@ -I$(top_srcdir)/src

xmmsplugindir = ${XMMS_PLUGIN_DIR}

# Don't build a static library
LIBTOOL = $(top_builddir)/libtool-disable-static
 
xmmsplugin_LTLIBRARIES  = libxmms_osd.la
libxmms_osd_la_SOURCES  = xmms_osd.c
libxmms_osd_la_LIBADD   = $(top_builddir)/src/libxosd/libxosd.la
libxmms_osd_la_LDFLAGS = -module -avoid-version @GDK_PIXBUF_LIBS@
