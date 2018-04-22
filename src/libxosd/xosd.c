/* XOSD

 Copyright (c) 2000 Andre Renaud (andre@ignavus.net)

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <assert.h>
#include <pthread.h>

#include <locale.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>

#include "xosd.h"


//#ifdef X_HAVE_UTF8_STRING
//#define XDRAWSTRING Xutf8DrawString
//#else
#define XDRAWSTRING XmbDrawString
//#endif

typedef enum {LINE_blank, LINE_text, LINE_percentage, LINE_slider} line_type;

typedef struct
{
  line_type type;

  char *text;
  int length;
  int width;
  int percentage;
} xosd_line;

struct xosd
{
  pthread_t event_thread;
  pthread_t timeout_thread;

  pthread_mutex_t mutex;
  pthread_cond_t cond;

  Display *display;
  int screen;
  Window window;
  unsigned int depth;
  Pixmap mask_bitmap;
  Pixmap line_bitmap;
  Visual *visual;

  XFontSet fontset;
  XRectangle *extent;

  GC gc;
  GC mask_gc;
  GC mask_gc_back;

  int width;
  int height;
  int line_height;
  int x;
  int y;
  xosd_pos pos;
  xosd_align align;
  int offset;
  int shadow_offset;

  int mapped;
  int done;

  unsigned int pixel;
  XColor colour;
  Colormap colourmap;

  xosd_line* lines;
  int number_lines;

  int timeout;
  int timeout_time;
};

char* xosd_error;

const char* osd_default_font="-misc-fixed-medium-r-semicondensed--*-*-*-*-c-*-*-*";
//const char* osd_default_font="adobe-helvetica-bold-r-*-*-10-*";
//const char* osd_default_font="-adobe-helvetica-bold-r-*-*-10-*";

static void draw_bar(xosd *osd, Drawable d, GC gc, int x, int y,
                     int percent, int is_slider, int set_color)
{
  int barw, barh;
  int nbars, on, i;
  int so = osd->shadow_offset;

  barh = -osd->extent->y;
  barw = barh / 2;

  nbars = (osd->width * 0.8) / barw;
  on = (nbars - 1) * percent / 100;

  for (i = 0; i < nbars; x += barw, i++) {
    int w = barw, h = barh;
    int yy = y;

    if (is_slider ? i == on : i < on) {
      w *= 0.7;
    }
    else {
      w *= 0.8;
      h /= 3;
      yy += h;
    }
    XFillRectangle(osd->display, d, gc, x, yy, w, h);
    if (so) {
      if (set_color) XSetForeground (osd->display, gc, BlackPixel(osd->display, osd->screen));
      XFillRectangle(osd->display, d, gc, x + w, yy + so, so, h);
      XFillRectangle(osd->display, d, gc, x + so, yy + h, w - so, so);
      if (set_color) XSetForeground (osd->display, gc, osd->pixel);
    }
  }
}

static void expose_line(xosd *osd, int line)
{
  int x = 10;
  int y = osd->line_height * line;
  xosd_line *l = &osd->lines[line];

  XFillRectangle (osd->display, osd->mask_bitmap, osd->mask_gc_back,
                  0, y, osd->width, osd->line_height);

  switch (l->type) {
    case LINE_blank:
      break;

    case LINE_text:
      if (!l->text || !l->length) break;

      if (osd->align) {
        if (osd->align == XOSD_right) x = osd->width - l->width - x;
        else x = (osd->width - l->width) / 2;
      }

      if (osd->shadow_offset) {
        XDRAWSTRING (osd->display, osd->mask_bitmap, osd->fontset, osd->mask_gc,
            x + osd->shadow_offset, y - osd->extent->y + osd->shadow_offset, l->text, l->length);
        XSetForeground (osd->display, osd->gc, BlackPixel(osd->display, osd->screen));
        XDRAWSTRING (osd->display, osd->line_bitmap, osd->fontset, osd->gc,
            x + osd->shadow_offset, -osd->extent->y + osd->shadow_offset, l->text, l->length);
      }
      XDRAWSTRING (osd->display, osd->mask_bitmap, osd->fontset, osd->mask_gc,
                     x, y - osd->extent->y, l->text, l->length);
      XSetForeground (osd->display, osd->gc, osd->pixel);
      XDRAWSTRING (osd->display, osd->line_bitmap, osd->fontset, osd->gc,
          x, -osd->extent->y, l->text, l->length);
      XCopyArea(osd->display, osd->line_bitmap, osd->window, osd->gc, 0, 0,
          osd->width, osd->line_height, 0, y);
      break;

    case LINE_percentage:
    case LINE_slider:
      draw_bar(osd, osd->mask_bitmap, osd->mask_gc, x, y, l->percentage, l->type == LINE_slider, 0);
      draw_bar(osd, osd->window, osd->gc, x, y, l->percentage, l->type == LINE_slider, 1);
      break;
  }
}

static void *event_loop (void *osdv)
{
  xosd *osd = osdv;
  XEvent report;
  int line, y;

  while (!osd->done) {
    pthread_mutex_lock (&osd->mutex);
    if (!XCheckWindowEvent (osd->display, osd->window, ExposureMask, &report)) {
      pthread_mutex_unlock (&osd->mutex);
      usleep (500);
      continue;
    }
    pthread_mutex_unlock (&osd->mutex);

    report.type &= 0x7f; /* remove the sent by server/manual send flag */

    switch (report.type) {
      case Expose:
        if (report.xexpose.count == 0) {
          pthread_mutex_lock (&osd->mutex);
          for (line = 0; line < osd->number_lines; line++) {
            y = osd->line_height * line;
            if (report.xexpose.y >= y + osd->line_height) continue;
            if (report.xexpose.y + report.xexpose.height < y) continue;
            expose_line(osd, line);
          }
          XShapeCombineMask (osd->display, osd->window, ShapeBounding, 0, 0,
                             osd->mask_bitmap, ShapeSet);
          XFlush(osd->display);
          pthread_mutex_unlock (&osd->mutex);
        }
        break;

      case NoExpose:
        break;

      default:
        //printf ("%d\n", report.type);
        break;
    }
  }

  return NULL;
}


static void *timeout_loop (void *osdv)
{
  xosd *osd = osdv;

  if (osdv == NULL) return NULL;

  while (!osd->done) {
    usleep (1000);
    pthread_mutex_lock (&osd->mutex);
    if (osd->timeout != -1 && osd->mapped && osd->timeout_time <= time(NULL)) {
      pthread_mutex_unlock (&osd->mutex);
      //printf ("timeout_loop: hiding\n");
      xosd_hide (osd);
    }
    else
      pthread_mutex_unlock (&osd->mutex);
  }

  return NULL;
}

static int display_string (xosd *osd, xosd_line *l, char *string)
{
  XRectangle rect;

  if (osd == NULL) return -1;

  l->type = LINE_text;

  if (string) {
    l->length = strlen(string);
    l->text = realloc (l->text, l->length + 1);
    strcpy (l->text, string);
  }
  else {
    l->text = realloc (l->text, 1);
    l->text[0] = '\0';
    l->length = 0;
  }
  XmbTextExtents(osd->fontset, l->text, l->length, NULL, &rect);
  l->width = rect.width;

  return 0;
}

static int display_percentage (xosd *osd, xosd_line *l, int percentage)
{
  if (osd == NULL) return -1;

  if (percentage < 0) percentage = 0;
  if (percentage > 100) percentage = 100;

  l->type = LINE_percentage;
  l->percentage = percentage;

  return 0;
}

static int display_slider (xosd *osd, xosd_line *l, int percentage)
{
  if (osd == NULL) return -1;

  if (percentage < 0) percentage = 0;
  if (percentage > 100) percentage = 100;

  l->type = LINE_slider;
  l->percentage = percentage;

  return 0;
}

static int force_redraw (xosd *osd, int line)
{
  if (osd == NULL) return -1;
  pthread_mutex_lock (&osd->mutex);
  for (line = 0; line < osd->number_lines; line++) {
    expose_line (osd, line);
  }
  XShapeCombineMask (osd->display, osd->window, ShapeBounding, 0, 0, osd->mask_bitmap, ShapeSet);
  XFlush(osd->display);
  pthread_mutex_unlock (&osd->mutex);

  if (!osd->mapped) {
    pthread_mutex_lock (&osd->mutex);
    XMapRaised (osd->display, osd->window);
    osd->mapped = 1;
    pthread_mutex_unlock (&osd->mutex);
  }

  return 0;
}

static int set_font (xosd *osd, char *font)
{
  char **missing;
  int nmissing;
  char *defstr;
  int line;

  XFontSetExtents *extents;

  if (osd == NULL) return -1;

  pthread_mutex_lock (&osd->mutex);
  if (osd->fontset) {
    XFreeFontSet (osd->display, osd->fontset);
    osd->fontset = NULL;
  }

  osd->fontset = XCreateFontSet (osd->display, font, &missing, &nmissing, &defstr);
  if (osd->fontset == NULL) {
    pthread_mutex_unlock (&osd->mutex);
    xosd_error="Requested font not found";
    return -1;
  }

  extents = XExtentsOfFontSet(osd->fontset);
  osd->extent = &extents->max_logical_extent;

  osd->width = XDisplayWidth (osd->display, osd->screen);
  osd->line_height = osd->extent->height + osd->shadow_offset;
  osd->height = osd->line_height * osd->number_lines;
  for (line = 0; line < osd->number_lines; line++) {
    xosd_line *l = &osd->lines[line];

    if (l->type == LINE_text) {
      XRectangle rect;

      XmbTextExtents(osd->fontset, l->text, l->length, NULL, &rect);
      l->width = rect.width;
    }
  }
  pthread_mutex_unlock (&osd->mutex);

  return 0;
}

static void resize(xosd *osd)
{
  pthread_mutex_lock (&osd->mutex);
  XResizeWindow (osd->display, osd->window, osd->width, osd->height);
  XFreePixmap (osd->display, osd->mask_bitmap);
  osd->mask_bitmap = XCreatePixmap (osd->display, osd->window, osd->width, osd->height, 1);
  XFreePixmap (osd->display, osd->line_bitmap);
  osd->line_bitmap = XCreatePixmap (osd->display, osd->window, osd->width,
                                    osd->line_height, osd->depth);
  pthread_mutex_unlock (&osd->mutex);
}

static int set_colour (xosd *osd, char *colour)
{
  int retval = 0;

  if (osd == NULL) return -1;

  pthread_mutex_lock (&osd->mutex);

  osd->colourmap = DefaultColormap (osd->display, osd->screen);

  if (XParseColor (osd->display, osd->colourmap, colour, &osd->colour)) {
    if (XAllocColor(osd->display, osd->colourmap, &osd->colour)) {
      osd->pixel = osd->colour.pixel;
    }
    else {
      osd->pixel = WhitePixel(osd->display, osd->screen);
      retval = -1;
    }
  }
  else {
    osd->pixel = WhitePixel(osd->display, osd->screen);
    retval = -1;
  }

  XSetForeground (osd->display, osd->gc, osd->pixel);
  XSetBackground (osd->display, osd->gc, WhitePixel (osd->display, osd->screen));

  pthread_mutex_unlock (&osd->mutex);

  return retval;
}


static void set_timeout (xosd *osd, int timeout)
{
  osd->timeout = timeout;
  osd->timeout_time = time (NULL) + timeout;
}

static Atom net_wm;
static Atom net_wm_state;
static Atom net_wm_top;

#define _NET_WM_STATE_ADD           1    /* add/set property */

/* tested with kde */
static void net_wm_stay_on_top(Display *dpy, Window win)
{
  XEvent e;

  e.xclient.type = ClientMessage;
  e.xclient.message_type = net_wm_state;
  e.xclient.display = dpy;
  e.xclient.window = win;
  e.xclient.format = 32;
  e.xclient.data.l[0] = _NET_WM_STATE_ADD;
  e.xclient.data.l[1] = net_wm_top;
  e.xclient.data.l[2] = 0l;
  e.xclient.data.l[3] = 0l;
  e.xclient.data.l[4] = 0l;

  XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureRedirectMask, &e);
}

/* ------------------------------------------------------------------------ */

static Atom gnome;
static Atom gnome_layer;

#define WIN_LAYER_ONTOP                  6

/* tested with icewm + WindowMaker */
static void gnome_stay_on_top(Display *dpy, Window win)
{
  XClientMessageEvent xev;

  memset(&xev, 0, sizeof(xev));
  xev.type = ClientMessage;
  xev.window = win;
  xev.message_type = gnome_layer;
  xev.format = 32;
  xev.data.l[0] = WIN_LAYER_ONTOP;
  XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureNotifyMask, (XEvent *)&xev);
}

/* ------------------------------------------------------------------------ */

static void stay_on_top(Display *dpy, Window win)
{
  Atom            type;
  int             format;
  unsigned long   nitems, bytesafter;
  unsigned char  *args = NULL;
  Window root = DefaultRootWindow(dpy);

  /* build atoms */
  net_wm       = XInternAtom(dpy, "_NET_SUPPORTED", False);
  net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
  net_wm_top   = XInternAtom(dpy, "_NET_WM_STATE_STAYS_ON_TOP", False);
  gnome        = XInternAtom(dpy, "_WIN_SUPPORTING_WM_CHECK", False);
  gnome_layer  = XInternAtom(dpy, "_WIN_LAYER", False);

  /* gnome-compilant */
  if (Success == XGetWindowProperty
      (dpy, root, gnome, 0, (65536 / sizeof(long)), False,
       AnyPropertyType, &type, &format, &nitems, &bytesafter, &args) &&
      nitems > 0) {
    /* FIXME: check capabilities */
    gnome_stay_on_top(dpy, win);
    XFree(args);
  }
  /* netwm compliant */
  else if (Success == XGetWindowProperty
      (dpy, root, net_wm, 0, (65536 / sizeof(long)), False,
       AnyPropertyType, &type, &format, &nitems, &bytesafter, &args) &&
      nitems > 0) {
    net_wm_stay_on_top(dpy, win);
    XFree(args);
  }
  XRaiseWindow(dpy, win);
}

xosd *xosd_init (char *font, char *colour, int timeout, xosd_pos pos, int offset, int shadow_offset, int number_lines)
{
  xosd *osd;
  int event_basep, error_basep, inputmask, i;
  char *display;
  XSetWindowAttributes setwinattr;
  long data;
  Atom a;

  display = getenv ("DISPLAY");
  if (!display) {
    xosd_error= "No display";
    return NULL;
  }

  setlocale(LC_ALL, "");

  osd = malloc (sizeof (xosd));
  if (osd == NULL)
    {
      xosd_error = "Out of memory";
      return NULL;
    }

  pthread_mutex_init (&osd->mutex, NULL);
  pthread_cond_init (&osd->cond, NULL);

  osd->number_lines=number_lines;
  osd->lines=malloc(sizeof(xosd_line) * osd->number_lines);
  if (osd->lines == NULL)
    {
      xosd_error = "Out of memory";
      free (osd);
      return NULL;
    }

  osd->mapped = 0;
  osd->done = 0;
  osd->align = XOSD_left;
  osd->shadow_offset = shadow_offset;
  osd->display = XOpenDisplay (display);
  osd->screen = XDefaultScreen (osd->display);

  if (!osd->display) {
    xosd_error="Cannot open display";
    free(osd);
    return NULL;
  }

  if (!XShapeQueryExtension (osd->display, &event_basep, &error_basep)) {
    xosd_error="X-Server does not support shape extension";
    free(osd);
    return NULL;
  }

  osd->visual = DefaultVisual (osd->display, osd->screen);
  osd->depth = DefaultDepth (osd->display, osd->screen);

  osd->fontset=NULL;
  if (set_font (osd, font)) {
    /* If we didn't get a fontset, default to default font */
    font = osd_default_font;
    set_font(osd, font);
  }

  if (osd->fontset == NULL) {
    /* if we still don't have a fontset, then abort */
    xosd_error="Requested font not found";
    return NULL;
  }

  osd->width = XDisplayWidth (osd->display, osd->screen);
  osd->height = osd->line_height * osd->number_lines;

  setwinattr.override_redirect = 1;
  osd->window = XCreateWindow (osd->display,
      XRootWindow (osd->display, osd->screen),
      0, 0,
      osd->width, osd->height,
      0,
      osd->depth,
      CopyFromParent,
      osd->visual,
      CWOverrideRedirect,
      &setwinattr);
  XStoreName (osd->display, osd->window, "XOSD");
  osd->pos = pos;
  xosd_set_offset (osd, offset);

  osd->mask_bitmap = XCreatePixmap (osd->display, osd->window, osd->width, osd->height, 1);
  osd->line_bitmap = XCreatePixmap (osd->display, osd->window, osd->width,
     osd->line_height, osd->depth);

  osd->gc = XCreateGC (osd->display, osd->window, 0, NULL);
  osd->mask_gc = XCreateGC (osd->display, osd->mask_bitmap, 0, NULL);
  osd->mask_gc_back = XCreateGC (osd->display, osd->mask_bitmap, 0, NULL);
  XSetForeground (osd->display, osd->mask_gc_back, BlackPixel (osd->display, osd->screen));
  XSetBackground (osd->display, osd->mask_gc_back, WhitePixel (osd->display, osd->screen));

  XSetForeground (osd->display, osd->mask_gc, WhitePixel (osd->display, osd->screen));
  XSetBackground (osd->display, osd->mask_gc, BlackPixel (osd->display, osd->screen));

  set_colour (osd, colour);
  set_timeout (osd, timeout);

  inputmask = ExposureMask ;
  XSelectInput (osd->display, osd->window, inputmask);

  stay_on_top(osd->display, osd->window);

  for (i = 0; i < osd->number_lines; i++) {
    osd->lines[i].type = LINE_text;
    osd->lines[i].text = NULL;
  }

  pthread_create (&osd->event_thread, NULL, event_loop, osd);
  pthread_create (&osd->timeout_thread, NULL, timeout_loop, osd);

  return osd;
}

int xosd_uninit (xosd *osd)
{
  int i;

  if (osd == NULL) return -1;

  pthread_mutex_lock (&osd->mutex);
  osd->done = 1;
  pthread_mutex_unlock (&osd->mutex);

  pthread_join (osd->event_thread, NULL);
  pthread_join (osd->timeout_thread, NULL);

  XFreePixmap (osd->display, osd->mask_bitmap);
  XFreePixmap (osd->display, osd->line_bitmap);
  XFreeFontSet (osd->display, osd->fontset);
  XDestroyWindow (osd->display, osd->window);


  for (i = 0; i < osd->number_lines; i++) {
    if (osd->lines[i].text)
      free (osd->lines[i].text);
  }

  free(osd->lines);

  pthread_cond_destroy (&osd->cond);
  pthread_mutex_destroy (&osd->mutex);

  free (osd);

  return 0;
}

int xosd_display (xosd *osd, int line, xosd_command command, ...)
{
  int len;
  va_list a;
  char *string;
  int percent;
  xosd_line *l = &osd->lines[line];

  assert (line >= 0 && line < osd->number_lines);

  if (osd == NULL) return -1;

  osd->timeout_time = time(NULL) + osd->timeout;

  va_start (a, command);
  switch (command) {
    case XOSD_string: {
        string = va_arg (a, char *);
        len = display_string (osd, l, string);
        break;
      }

    case XOSD_printf: {
        char buf[2000];

        string = va_arg (a, char *);
        if (vsnprintf(buf, sizeof(buf), string, a) >= sizeof(buf)) {
          return -1;
        }
        len = display_string (osd, l, buf);
        break;
      }

    case XOSD_percentage: {
        percent = va_arg (a, int);

        display_percentage (osd, l, percent);

        len = percent;
        break;
      }

    case XOSD_slider: {
        percent = va_arg (a, int);

        display_slider (osd, l, percent);

        len = percent;
        break;
      }

    default: {
        len = -1;
        xosd_error="xosd_display: Unknown command";
      }
  }
  va_end (a);

  force_redraw (osd, line);

  return len;
}

int xosd_is_onscreen(xosd* osd)
{
  if (osd == NULL) return -1;
  return osd->mapped;
}

int xosd_wait_until_no_display(xosd* osd)
{
  if (osd == NULL) return -1;

  while (xosd_is_onscreen(osd)) {
    pthread_mutex_lock (&osd->mutex);
    pthread_cond_wait(&osd->cond, &osd->mutex);
    pthread_mutex_unlock (&osd->mutex);
  }

  return 0;
}


int xosd_set_colour (xosd *osd, char *colour)
{
  int retval = 0;

  if (osd == NULL) return -1;

  retval = set_colour (osd, colour);

  force_redraw (osd, -1);
  return retval;
}


int xosd_set_font (xosd *osd, char *font)
{
  int ret = 0;

  if (font==NULL)  return -1;

  if (osd == NULL) return -1;

  ret = set_font (osd, font);
  if (ret == 0) resize(osd);

  return ret;
}

static void xosd_update_pos (xosd *osd)
{
  osd->x = 0;
  if (osd->pos == XOSD_bottom)
    osd->y = XDisplayHeight (osd->display, osd->screen) - osd->height - osd->offset;
  else
    osd->y = osd->offset;

  XMoveWindow (osd->display, osd->window, osd->x, osd->y);
}

int xosd_set_shadow_offset (xosd *osd, int shadow_offset)
{
  if (osd == NULL) return -1;

  osd->shadow_offset = shadow_offset;
  force_redraw (osd, -1);

  return 0;
}

int xosd_set_offset (xosd *osd, int offset)
{
  if (osd == NULL) return -1;

  osd->offset = offset;
  xosd_update_pos (osd);

  return 0;
}

int xosd_set_pos (xosd *osd, xosd_pos pos)
{
  if (osd == NULL) return -1;

  osd->pos = pos;
  xosd_update_pos (osd);

  return 0;
}

int xosd_set_align (xosd *osd, xosd_align align)
{
  if (osd == NULL) return -1;

  osd->align = align;
  force_redraw (osd, -1);

  return 0;
}

int xosd_get_colour (xosd *osd, int *red, int *green, int *blue)
{
  if (osd == NULL) return -1;

  if (red) *red = osd->colour.red;
  if (blue) *blue = osd->colour.blue;
  if (green) *green = osd->colour.green;

  return 0;

}

int xosd_set_timeout (xosd *osd, int timeout)
{
  if (osd == NULL) return -1;

  set_timeout (osd, timeout);
  return 0;
}


int xosd_hide (xosd *osd)
{
  if (osd == NULL) return -1;
  if (osd->mapped) {
    pthread_mutex_lock (&osd->mutex);
    osd->mapped = 0;
    XUnmapWindow (osd->display, osd->window);
    XFlush (osd->display);
    pthread_cond_broadcast(&osd->cond);
    pthread_mutex_unlock (&osd->mutex);
    return 0;
  }
  return -1;
}

int xosd_show (xosd *osd)
{
  if (osd == NULL) return -1;

  if (!osd->mapped) {
    pthread_mutex_lock (&osd->mutex);
    osd->mapped = 1;
    XMapRaised (osd->display, osd->window);
    XFlush (osd->display);
    pthread_mutex_unlock (&osd->mutex);
    return 0;
  }
  return -1;
}

/* This function will scroll the display up "lines" number of lines */
int xosd_scroll(xosd *osd, int lines)
{
  int new_line;

  if (osd == NULL) return -1;

  assert(lines > 0 && lines <= osd->number_lines);

  /* First free everything no longer needed */
  for (new_line = 0; new_line < lines; new_line++) {
    if ((osd->lines[new_line].type == LINE_text) && (osd->lines[new_line].text != NULL)) {
      free(osd->lines[new_line].text);
      osd->lines[new_line].text = NULL;
      osd->lines[new_line].type = LINE_blank;
    }
  }

  /* Do the scroll */
  for (new_line = 0; new_line < osd->number_lines - lines; new_line++) {
    osd->lines[new_line] = osd->lines[new_line + lines];
  }

  /* Clear the lines opened up by scrolling, need because of the use of realloc in display string */
  while (new_line < osd->number_lines) {
    osd->lines[new_line].text = NULL;
    osd->lines[new_line].type = LINE_blank;
    new_line++;
  }

  force_redraw (osd, -1);
  return 0;
}

int xosd_get_number_lines(xosd* osd) {

  if ( (osd) == NULL) { return -1; };

  return osd->number_lines;
}
