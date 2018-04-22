# XMMS library

AM_CFLAGS = @XMMS_CFLAGS@ @GTK_CFLAGS@ @GDK_PIXBUF_CFLAGS@ \
	-I$(top_srcdir)/src -DXMMS_PIXMAPDIR=\"$(XMMS_PIXMAPDIR)\"

xmmsplugindir = @XMMS_PLUGIN_DIR@

# Don't build a static library
LIBTOOL = $(top_builddir)/libtool-disable-static
 
xmmsplugin_LTLIBRARIES =
if BUILD_NEW_PLUGIN
xmmsplugin_LTLIBRARIES += libxmms_osd.la
endif
if BUILD_OLD_PLUGIN
xmmsplugin_LTLIBRARIES += libxmms_osd_old.la
endif

libxmms_osd_la_SOURCES = xmms_osd.c dlg_config.c dlg_font.c dlg_colour.c xmms_osd.h
libxmms_osd_la_LIBADD  = $(top_builddir)/src/libxosd/libxosd.la
libxmms_osd_la_LDFLAGS = -module -avoid-version @GDK_PIXBUF_LIBS@
 
libxmms_osd_old_la_SOURCES = xmms_osd.c dlg_config_old.c dlg_font.c dlg_colour.c xmms_osd.h
libxmms_osd_old_la_LIBADD  = $(top_builddir)/src/libxosd/libxosd.la
libxmms_osd_old_la_LDFLAGS = -module -avoid-version
