# Programs.  Don't install testprog.
bin_PROGRAMS 	= osd_cat 
noinst_PROGRAMS = testprog

osd_cat_SOURCES  = osd_cat.c
testprog_SOURCES = testprog.c

osd_cat_LDADD 	= libxosd.la
testprog_LDADD 	= libxosd.la

# Libraries.  Put libxmms_osd into xmms' plugin dir.
lib_LTLIBRARIES 	= libxosd.la 
libxosd_la_SOURCES 	= xosd.c  xosd.h
libxosd_la_LDFLAGS = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

include_HEADERS = xosd.h

CFLAGS=@CFLAGS@ ${GTK_CFLAGS}
xmmsplugindir = ${XMMS_PLUGIN_DIR}/General/
xmmsplugin_LTLIBRARIES 	= libxmms_osd.la
libxmms_osd_la_SOURCES	= xmms_osd.c
libxmms_osd_la_LIBADD	= ${XMMS_LIBS} ${GTK_LIBS} libxosd.la
