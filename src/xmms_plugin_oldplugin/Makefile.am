# XMMS library

AM_CFLAGS=@XMMS_CFLAGS@ @GTK_CFLAGS@ -I..

xmmsplugindir = ${XMMS_PLUGIN_DIR}

# Don't build a static library
LIBTOOL = $(top_builddir)/libtool-disable-static
 
xmmsplugin_LTLIBRARIES  = libxmms_osd_oldplugin.la
libxmms_osd_oldplugin_la_SOURCES  = xmms_osd_oldplugin.c
libxmms_osd_oldplugin_la_LIBADD   = $(top_builddir)/src/libxosd/libxosd.la
libxmms_osd_oldplugin_la_LDFLAGS = -module -avoid-version
