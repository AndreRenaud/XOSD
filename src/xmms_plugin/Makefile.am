# XMMS library

CFLAGS=@CFLAGS@ ${GTK_CFLAGS} -I..

xmmsplugindir = ${XMMS_PLUGIN_DIR}/General/

xmmsplugin_LTLIBRARIES  = libxmms_osd.la
libxmms_osd_la_SOURCES  = xmms_osd.c
libxmms_osd_la_LIBADD   = ${XMMS_LIBS} ${GTK_LIBS} ../libxosd/libxosd.la
libxmms_osd_la_LDFLAGS = -module -avoid-version
