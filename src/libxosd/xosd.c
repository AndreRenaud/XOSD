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
#include <time.h>

#include <assert.h>
#include <pthread.h>

#include <locale.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>
#ifdef HAVE_XINERAMA
#  include <X11/extensions/Xinerama.h>
#endif

#include "xosd.h"

#if 0
#define DEBUG(args...) fprintf (stderr, "%s: %s: %d: ", __FILE__, __PRETTY_FUNCTION__, __LINE__); fprintf(stderr, args); fprintf(stderr, "\n")
#else
#define DEBUG(args...)
#endif

//#ifdef X_HAVE_UTF8_STRING
//#define XDRAWSTRING Xutf8DrawString
//#else
#define XDRAWSTRING XmbDrawString
//#endif
#define SLIDER_WIDTH 0.8

	const char* osd_default_font="-misc-fixed-medium-r-semicondensed--*-*-*-*-c-*-*-*";
static const char* osd_default_colour="green";

//const char* osd_default_font="adobe-helvetica-bold-r-*-*-10-*";
//const char* osd_default_font="-adobe-helvetica-bold-r-*-*-10-*";



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
	pthread_t event_thread; /* handles X events */
	pthread_t timeout_thread; /* handle automatic hide after timeout */

	pthread_mutex_t mutex; /* mutual exclusion to protect struct */
	pthread_cond_t cond_hide; /* signal hide events */
	pthread_cond_t cond_time; /* signal timeout */

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

	int screen_width;
	int screen_height;
	int screen_xpos;
	int height;
	int line_height;
	int x;
	int y;
	xosd_pos pos;
	xosd_align align;
	int hoffset;
	int voffset;
	int shadow_offset;
	XColor shadow_colour;
	unsigned int shadow_pixel;
	int outline_offset;
	XColor outline_colour;
	unsigned int outline_pixel;
	int bar_length; 

	int mapped;
	int done;

	unsigned int pixel;
	XColor colour;
	Colormap colourmap;

	xosd_line* lines;
	int number_lines;

	int timeout; /* delta time */
	struct timespec timeout_time; /* Next absolute timeout */
};

/** Global error string. */
char* xosd_error;

/* Forward declarations of internal functions. */
static void set_timeout (xosd *);
static void show (xosd *);
static void hide (xosd *);
static void update_pos (xosd *osd);


static void draw_bar(xosd *osd, Drawable d, GC gc, int x, int y,
                     int percent, int is_slider, int set_color)
{
	int barw, barh;
	int nbars, on, i;
	int so = osd->shadow_offset;
	assert (osd);

	barh = -osd->extent->y;
	barw = barh / 2;

	//check how to behave
	if (osd->bar_length == -1) {
		nbars = (osd->screen_width * SLIDER_WIDTH) / barw;
		on    = nbars * percent / 100;
	} else {
		nbars = osd->bar_length;
		on    = (nbars * percent) / 100 ;

		DEBUG("percent=%d, nbars==%d, on == %d", percent, nbars, on);

		//fix x coord
		switch (osd->align) {
			case XOSD_left:
				break;
			case XOSD_center:
				x = (osd->screen_width - (nbars*barw)) / 2;
				break;
			case XOSD_right:
				x = osd->screen_width - (nbars*barw) - x;
				break;
			default:
				break;
		}
	}


	for (i = 0; i < nbars; x += barw, i++) {
		int w = barw, h = barh;
		int yy = y;

		if (is_slider ? i == on : i < on) {
			w *= SLIDER_WIDTH-0.1;
		}
		else {
			w *= SLIDER_WIDTH;
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

static void draw_with_mask(xosd* osd, xosd_line* l, int inX, int inPlace, int inY) 
{
	   XDRAWSTRING (osd->display,
					osd->mask_bitmap,
					osd->fontset,
					osd->mask_gc,
					inX,
					inPlace + inY,
					l->text,
					l->length);
	   


		XDRAWSTRING (osd->display,
					 osd->line_bitmap,
					 osd->fontset,
					 osd->gc,
					 inX,
					 inY,
					 l->text,
					 l->length);				

}

static void expose_line(xosd *osd, int line)
{
	int x = 10;
	int y = osd->line_height * line;
	xosd_line *l = &osd->lines[line];
	assert (osd);

	/* don't need to lock here because functions that call expose_line should
	   have already locked the mutex */
	XFillRectangle (osd->display, osd->mask_bitmap, osd->mask_gc_back,
					0, y, osd->screen_width, osd->line_height);

	switch (l->type) {
		case LINE_blank:
			break;

		case LINE_text:
			if (!l->text || !l->length) break;
			if (!osd->fontset) { DEBUG("CRITICAL: No fontset"); return; }

			switch (osd->align) {
				case XOSD_left:
					break;
				case XOSD_center:
					x = (osd->screen_width - l->width) / 2;
					break;
				case XOSD_right:
					x = osd->screen_width - l->width - x;
					break;
				default:
					break;
			}

			if (osd->shadow_offset) {

				XSetForeground (osd->display, osd->gc, osd->shadow_pixel);
		 
				draw_with_mask(osd, l,
							   x + osd->shadow_offset,
							   y,
							   osd->shadow_offset - osd->extent->y );
				
			}
			
			if (osd->outline_offset) {
				XSetForeground (osd->display, osd->gc, osd->outline_pixel);


				draw_with_mask(osd, l, x + osd->outline_offset, y,
							   osd->outline_offset - osd->extent->y);
				
				draw_with_mask(osd, l, x + osd->outline_offset, y,
							   - osd->outline_offset - osd->extent->y);

				
				draw_with_mask(osd, l, x - osd->outline_offset, y,
							   - osd->outline_offset - osd->extent->y);
				
				draw_with_mask(osd, l, x - osd->outline_offset, y,
							   osd->outline_offset - osd->extent->y);
			}

			XSetForeground (osd->display, osd->gc, osd->pixel);

			draw_with_mask(osd, l, x, y, -osd->extent->y);
			
			XCopyArea(osd->display, osd->line_bitmap, osd->window, osd->gc, 0, 0,
					  osd->screen_width, osd->line_height, 0, y);
			break;

		case LINE_percentage:
		case LINE_slider:
      
			switch (osd->align) {
				case XOSD_left:
					break;
				case XOSD_center:
					x=osd->screen_width*((1-SLIDER_WIDTH)/2);
					break;
				case XOSD_right:
					x=osd->screen_width*(1-SLIDER_WIDTH);
					break;
				default:
					break;
			}
      
			draw_bar(osd,osd->mask_bitmap,osd->mask_gc,x,y,l->percentage, l->type==LINE_slider,0);
			draw_bar(osd,osd->window,osd->gc,x,y,l->percentage,l->type==LINE_slider,1);

			/*
			  draw_bar(osd, osd->mask_bitmap, osd->mask_gc, x, y, l->percentage, l->type == LINE_slider, 0);
			  draw_bar(osd, osd->window, osd->gc, x, y, l->percentage, l->type == LINE_slider, 1);
			*/

			break;
	}
}

/** Handle X11 events
 * This is running in it's own thread for Expose-events.
 */
static void *event_loop (void *osdv)
{
	xosd *osd = osdv;
	XEvent report;
	int line, y;

	DEBUG("event thread started");
	assert (osd);
	usleep (500);

	while (!osd->done) {
		//DEBUG("checking window event");
		XWindowEvent (osd->display, osd->window, ExposureMask, &report);
		if (osd->done) break;

		report.type &= 0x7f; /* remove the sent by server/manual send flag */

		switch (report.type) {
			case Expose:
				DEBUG("expose");
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
				//fprintf (stderr, "%d\n", report.type);
				break;
		}
	}

	return NULL;
}


/** Handle timeout events to auto hide output.
 * This is running in it's own thread and waits for timeout notifications.
 */
static void *timeout_loop (void *osdv)
{
	xosd *osd = osdv;
	assert (osd);

	pthread_mutex_lock (&osd->mutex);
	while (!osd->done) {
		/* Wait for timeout or change of timeout */
		int cond = osd->timeout_time.tv_sec
			? pthread_cond_timedwait (&osd->cond_time, &osd->mutex, &osd->timeout_time)
			: pthread_cond_wait (&osd->cond_time, &osd->mutex);
		/* If it was a timeout, hide output */
		if (cond && osd->timeout_time.tv_sec && osd->mapped) {
			//printf ("timeout_loop: hiding\n");
			osd->timeout_time.tv_sec = 0;
			hide (osd);
		}
	}
	pthread_mutex_unlock (&osd->mutex);

	return NULL;
}

static int display_string (xosd *osd, xosd_line *l, char *string)
{
	XRectangle rect;

	assert (osd);
	if (!osd->fontset) { DEBUG("CRITICAL: No fontset"); return -1; }

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
	pthread_mutex_lock (&osd->mutex);
	XmbTextExtents(osd->fontset, l->text, l->length, NULL, &rect);
	pthread_mutex_unlock (&osd->mutex);
	l->width = rect.width;

	return 0;
}

static int display_percentage (xosd *osd, xosd_line *l, int percentage)
{
	assert (osd);

	if (percentage < 0) percentage = 0;
	if (percentage > 100 ) percentage = 100;

	l->type = LINE_percentage;
	l->percentage = percentage;

	return 0;
}

static int display_slider (xosd *osd, xosd_line *l, int percentage)
{
	assert (osd);

	if (percentage < 0) percentage = 0;
	if (percentage > 100 ) percentage = 100;

	l->type = LINE_slider;
	l->percentage = percentage;

	return 0;
}

static void resize(xosd *osd) /* Requires mutex lock. */
{
	assert (osd);
	XResizeWindow (osd->display, osd->window, osd->screen_width, osd->height);
	XFreePixmap (osd->display, osd->mask_bitmap);
	osd->mask_bitmap = XCreatePixmap (osd->display, osd->window, osd->screen_width, osd->height, 1);
	XFreePixmap (osd->display, osd->line_bitmap);
	osd->line_bitmap = XCreatePixmap (osd->display, osd->window, osd->screen_width,
									  osd->line_height, osd->depth);
}

static int force_redraw (xosd *osd, int line) /* Requires mutex lock. */
{
	assert (osd);
	resize(osd);
	for (line = 0; line < osd->number_lines; line++) {
		expose_line (osd, line);
	}
	XShapeCombineMask (osd->display, osd->window, ShapeBounding, 0, 0, osd->mask_bitmap, ShapeSet);
	XFlush(osd->display);
	if (!osd->mapped)
		show (osd);

	return 0;
}

static int set_font (xosd *osd, const char *font) /* Requires mutex lock. */
{
	char **missing;
	int nmissing;
	char *defstr;
	int line;

	XFontSetExtents *extents;

	assert (osd);
	if (osd->fontset) {
		XFreeFontSet (osd->display, osd->fontset);
		osd->fontset = NULL;
	}

	osd->fontset = XCreateFontSet (osd->display, font, &missing, &nmissing, &defstr);
	if (osd->fontset == NULL) {
		xosd_error="Requested font not found";
		return -1;
	}
	XFreeStringList (missing);

	extents = XExtentsOfFontSet(osd->fontset);
	osd->extent = &extents->max_logical_extent;

	osd->line_height = osd->extent->height + osd->shadow_offset;
	osd->height = osd->line_height * osd->number_lines;
	for (line = 0; line < osd->number_lines; line++) {
		xosd_line *l = &osd->lines[line];

		if (l->type == LINE_text && l->text != NULL) {
			XRectangle rect;

			XmbTextExtents(osd->fontset, l->text, l->length, NULL, &rect);
			l->width = rect.width;
		}
	}

	return 0;
}

static int parse_colour(xosd* osd, XColor* col, int* pixel, const char* colour) 
{
	int retval=0;
	
	DEBUG("getting colourmap");
	osd->colourmap = DefaultColormap (osd->display, osd->screen);

	DEBUG("parsing colour");
	if (XParseColor (osd->display, osd->colourmap, colour, col)) {

		DEBUG("attempting to allocate colour");
		if (XAllocColor(osd->display, osd->colourmap, col)) {
			DEBUG("allocation sucessful");
			*pixel = col->pixel;
		}
		else {
			DEBUG("defaulting to white. could not allocate colour");
			*pixel = WhitePixel(osd->display, osd->screen);
			retval = -1;
		}
	}
	else {
		DEBUG("could not poarse colour. defaulting to white");
		*pixel = WhitePixel(osd->display, osd->screen);
		retval = -1;
	}
	
	return retval;
}


static int set_colour (xosd *osd, const char *colour) /* Requires mutex lock. */
{
	int retval = 0;

	assert (osd);
	
	retval=parse_colour(osd, & osd->colour, & osd->pixel, colour);
	
	DEBUG("setting foreground");
	XSetForeground (osd->display, osd->gc, osd->pixel);
	DEBUG("setting background");
	XSetBackground (osd->display, osd->gc, WhitePixel (osd->display, osd->screen));

	DEBUG("done");

	return retval;
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

/** Deprecated init. Use xosd_create. */
xosd *xosd_init (const char *font, const char *colour, int timeout, xosd_pos pos, int voffset, int shadow_offset, int number_lines)
{
	xosd *osd = xosd_create(number_lines);
  
	if (osd == NULL) {
		return NULL;
	}

	if (xosd_set_font(osd, font) == -1) {
		if (xosd_set_font(osd, osd_default_font) == -1) {
			xosd_destroy (osd);
			/* 
			   we do not set xosd_error, as set_font has already
			   set it to a sensible error message. 
			*/
			return NULL;
		} 
	}
	xosd_set_colour(osd, colour);
	xosd_set_timeout(osd, timeout);
	xosd_set_pos(osd, pos);
	xosd_set_vertical_offset(osd, voffset);
	xosd_set_shadow_offset(osd, shadow_offset);
  
	resize(osd);

	return osd;
}

/** New init. */
xosd *xosd_create (int number_lines) 
{
	xosd *osd;
	int event_basep, error_basep, i;
	char *display;
	XSetWindowAttributes setwinattr;
#ifdef HAVE_XINERAMA
	int screens;
	int dummy_a, dummy_b;
	XineramaScreenInfo *screeninfo = NULL;
#endif

	DEBUG("X11 thread support");
	if (!XInitThreads ()) {
		xosd_error = "xlib is not thread-safe";
		return NULL;
	} 

	DEBUG("getting display");
	display = getenv ("DISPLAY");
	if (!display) {
		xosd_error= "No display";
		return NULL;
	}

	DEBUG("setting locale");
	setlocale(LC_ALL, "");

	DEBUG("Mallocing osd");
	osd = malloc (sizeof (xosd));
	memset (osd, 0, sizeof (xosd));
	if (osd == NULL) {
		xosd_error = "Out of memory";
		goto error0;
	}

	DEBUG("initializing mutex");
	pthread_mutex_init (&osd->mutex, NULL);
	DEBUG("initializing condition");
	pthread_cond_init (&osd->cond_hide, NULL);
	pthread_cond_init (&osd->cond_time, NULL);

	DEBUG("initializing number lines");
	osd->number_lines=number_lines;
	osd->lines=malloc(sizeof(xosd_line) * osd->number_lines);
	if (osd->lines == NULL) {
		xosd_error = "Out of memory";
		goto error1;
	}

	for (i = 0; i < osd->number_lines; i++) {
		osd->lines[i].type = LINE_text;
		osd->lines[i].text = NULL;
	}

	DEBUG("misc osd variable initialization");
	osd->mapped = 0;
	osd->done = 0;
	osd->pos = XOSD_top;
	osd->hoffset = 0;
	osd->align = XOSD_left;
	osd->voffset = 0;
	osd->timeout = -1;
	osd->timeout_time.tv_sec = 0;
	osd->fontset = NULL;

	DEBUG("Display query");
	osd->display = XOpenDisplay (display);
	if (!osd->display) {
		xosd_error = "Cannot open display";
		goto error2;
	}
	osd->screen = XDefaultScreen (osd->display);

	DEBUG("x shape extension query");
	if (!XShapeQueryExtension (osd->display, &event_basep, &error_basep)) {
		xosd_error = "X-Server does not support shape extension";
		goto error3;
	}

	osd->visual = DefaultVisual (osd->display, osd->screen);
	osd->depth = DefaultDepth (osd->display, osd->screen);

	DEBUG("font selection info");
	set_font(osd, osd_default_font);
	if (osd->fontset == NULL) {
		/* if we still don't have a fontset, then abort */
		xosd_error="Default font not found";
		goto error3;
	}

	DEBUG("width and height initialization"); 
#ifdef HAVE_XINERAMA
	if (XineramaQueryExtension(osd->display, &dummy_a, &dummy_b) &&
		(screeninfo = XineramaQueryScreens(osd->display, &screens)) &&
		XineramaIsActive(osd->display)) {
		osd->screen_width = screeninfo[0].width;
		osd->screen_height = screeninfo[0].height;
		osd->screen_xpos = screeninfo[0].x_org;
	} else
#endif
	{
		osd->screen_width = XDisplayWidth (osd->display, osd->screen); 
		osd->screen_height = XDisplayHeight (osd->display, osd->screen); 
		osd->screen_xpos = 0;
	}
#ifdef HAVE_XINERAMA
	if (screeninfo) XFree(screeninfo);
#endif
	osd->height = osd->line_height * osd->number_lines;
	osd->bar_length = -1; //init bar_length with -1: draw_bar behaves like unpached

	DEBUG("creating X Window"); 
	setwinattr.override_redirect = 1; 

	osd->window = XCreateWindow (osd->display, 
								 XRootWindow (osd->display, osd->screen), 
								 0, 0,
								 osd->screen_width, osd->height,
								 0,
								 osd->depth,
								 CopyFromParent,
								 osd->visual,
								 CWOverrideRedirect,
								 &setwinattr);
	XStoreName (osd->display, osd->window, "XOSD");

	osd->mask_bitmap = XCreatePixmap (osd->display, osd->window, osd->screen_width, osd->height, 1);
	osd->line_bitmap = XCreatePixmap (osd->display, osd->window, osd->screen_width,
									  osd->line_height, osd->depth);

	osd->gc = XCreateGC (osd->display, osd->window, 0, NULL);
	osd->mask_gc = XCreateGC (osd->display, osd->mask_bitmap, 0, NULL);
	osd->mask_gc_back = XCreateGC (osd->display, osd->mask_bitmap, 0, NULL);

	XSetForeground (osd->display, osd->mask_gc_back, BlackPixel (osd->display, osd->screen));
	XSetBackground (osd->display, osd->mask_gc_back, WhitePixel (osd->display, osd->screen));

	XSetForeground (osd->display, osd->mask_gc, WhitePixel (osd->display, osd->screen));
	XSetBackground (osd->display, osd->mask_gc, BlackPixel (osd->display, osd->screen));


	DEBUG("setting colour");
	set_colour (osd, osd_default_colour); 

	DEBUG("Request exposure events");
	XSelectInput (osd->display, osd->window, ExposureMask);

	DEBUG("stay on top");
	stay_on_top(osd->display, osd->window);
  
	DEBUG("finale resize");
	update_pos (osd); /* Shoule be inside lock, but no threads yet */
	resize(osd); /* Shoule be inside lock, but no threads yet */

	DEBUG("initializing event thread");
	pthread_create (&osd->event_thread, NULL, event_loop, osd);

	DEBUG("initializing timeout thread");
	pthread_create (&osd->timeout_thread, NULL, timeout_loop, osd);

	return osd;

  error3:
	XCloseDisplay (osd->display);
  error2:
	free (osd->lines);
  error1:
	pthread_cond_destroy (&osd->cond_time);
	pthread_cond_destroy (&osd->cond_hide);
	pthread_mutex_destroy (&osd->mutex);
	free (osd);
  error0:
	return NULL;
}

/** Deprecated uninit. Use xosd_destroy. */
int xosd_uninit (xosd *osd) {
	return xosd_destroy(osd);
}

int xosd_destroy (xosd *osd)
{
	int i;
  
	DEBUG("start");

	if (osd == NULL) return -1;

	DEBUG("waiting for threads to exit");
	pthread_mutex_lock (&osd->mutex);
	osd->done = 1;
	/* Send signal to timeout-thread, will quit. */
	pthread_cond_signal (&osd->cond_time);
	pthread_mutex_unlock (&osd->mutex);

	/* Send fake XExpose-event to event-thread, will quit. */
	DEBUG("Send fake expose");
	{
		XEvent event = {
			.xexpose = {
				.type = Expose,
				.send_event = True,
				.display = osd->display,
				.window = osd->window,
				.count = 0,
			}
		};
		XSendEvent (osd->display, osd->window, False, ExposureMask, &event);
		XFlush (osd->display);
	}

	DEBUG("join threads");
	pthread_join (osd->event_thread, NULL);
	pthread_join (osd->timeout_thread, NULL);

	DEBUG("freeing X resources");
	XFreeGC (osd->display, osd->gc);
	XFreeGC (osd->display, osd->mask_gc);
	XFreeGC (osd->display, osd->mask_gc_back);
	XFreePixmap (osd->display, osd->line_bitmap);
	XFreeFontSet (osd->display, osd->fontset);
	XFreePixmap (osd->display, osd->mask_bitmap);
	XDestroyWindow (osd->display, osd->window);

	XCloseDisplay (osd->display);

	DEBUG("freeing lines");
	for (i = 0; i < osd->number_lines; i++) {
		if (osd->lines[i].text)
			free (osd->lines[i].text);
	}
	free(osd->lines);

	DEBUG("destroying condition and mutex");
	pthread_cond_destroy (&osd->cond_time);
	pthread_cond_destroy (&osd->cond_hide);
	pthread_mutex_destroy (&osd->mutex);

	DEBUG("freeing osd structure");
	free (osd);

	DEBUG("done");
	return 0;
}


int xosd_set_bar_length(xosd *osd, int length)
{
	if (osd == NULL) return -1;

	if (length==0) return -1;
	if (length<-1) return -1;

	osd->bar_length = length;
  
	return 0;
}

int xosd_display (xosd *osd, int line, xosd_command command, ...)
{
	int len;
	va_list a;
	char *string;
	int percent;
	xosd_line *l = &osd->lines[line];

	if (osd == NULL) return -1;

	if (line < 0 || line >= osd->number_lines) {
		xosd_error="xosd_display: Invalid Line Number";
		return -1;
	}

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

	pthread_mutex_lock (&osd->mutex);
	force_redraw (osd, line);
	set_timeout (osd);
	pthread_mutex_unlock (&osd->mutex);

	return len;
}

/** Return, if anything is displayed. Beware race conditions! **/
int xosd_is_onscreen(xosd* osd)
{
	if (osd == NULL) return -1;
	return osd->mapped;
}

/** Wait until nothing is displayed anymore. **/
int xosd_wait_until_no_display(xosd* osd)
{
	if (osd == NULL) return -1;

	pthread_mutex_lock (&osd->mutex);
	while (osd->mapped)
		pthread_cond_wait(&osd->cond_hide, &osd->mutex);
	pthread_mutex_unlock (&osd->mutex);

	return 0;
}

/** Set colour and force redraw. **/
int xosd_set_colour (xosd *osd, const char *colour)
{
	int retval = 0;

	if (osd == NULL) return -1;

	pthread_mutex_lock (&osd->mutex);
	retval = set_colour (osd, colour);
	force_redraw (osd, -1);
	pthread_mutex_unlock (&osd->mutex);

	return retval;
}


/** Set shadow colour and force redraw. **/
int xosd_set_shadow_colour (xosd *osd, const char *colour)
{
	int retval = 0;

	if (osd == NULL) return -1;

	pthread_mutex_lock (&osd->mutex);
	retval = parse_colour (osd, &osd->shadow_colour, &osd->shadow_pixel, colour);
	force_redraw (osd, -1);
	pthread_mutex_unlock (&osd->mutex);

	return retval;
}

int xosd_set_outline_colour (xosd *osd, const char *colour)
{
	int retval = 0;

	if (osd == NULL) return -1;

	pthread_mutex_lock (&osd->mutex);
	retval = parse_colour (osd, &osd->outline_colour, &osd->outline_pixel, colour);
	force_redraw (osd, -1);
	pthread_mutex_unlock (&osd->mutex);

	return retval;
}


/** Set font. Might return error if fontset can't be created. **/
int xosd_set_font (xosd *osd, const char *font)
{
	int ret = 0;

	if (font == NULL)  return -1;
	if (osd == NULL) return -1;

	pthread_mutex_lock (&osd->mutex);
	ret = set_font (osd, font);
	if (ret == 0) resize(osd);
	pthread_mutex_unlock (&osd->mutex);

	return ret;
}

static void update_pos (xosd *osd) /* Requires mutex lock. */
{
	assert (osd);
	switch (osd->pos) {
		case XOSD_bottom:
			osd->y = osd->screen_height - osd->height - osd->voffset;
			break;
		case XOSD_middle:
			osd->y = osd->screen_height/2 - osd->height - osd->voffset;
			break;
		case XOSD_top:
		default:
			osd->y = osd->voffset;
			break;
	}

	switch (osd->align) {
		case XOSD_left:
			osd->x = osd->hoffset + osd->screen_xpos;
			break;
		case XOSD_center:
			osd->x = osd->hoffset + osd->screen_xpos;
			/* which direction should this default to, left or right offset */
			break;
		case XOSD_right:
			// osd->x = XDisplayWidth (osd->display, osd->screen) - osd->width - osd->hoffset; 
			osd->x = -(osd->hoffset) + osd->screen_xpos; 
			/* neither of these work right, I want the offset to flip so
			 * +offset is to the left instead of to the right when aligned right */
			break;
		default:
			osd->x = 0;
			break;
	}

	XMoveWindow (osd->display, osd->window, osd->x, osd->y);
}

int xosd_set_shadow_offset (xosd *osd, int shadow_offset)
{
	if (osd == NULL) return -1;

	pthread_mutex_lock (&osd->mutex);
	osd->shadow_offset = shadow_offset;
	force_redraw (osd, -1);
	pthread_mutex_unlock (&osd->mutex);

	return 0;
}

int xosd_set_outline_offset (xosd *osd, int outline_offset)
{
	if (osd == NULL) return -1;

	pthread_mutex_lock (&osd->mutex);
	osd->outline_offset = outline_offset;
	force_redraw (osd, -1);
	pthread_mutex_unlock (&osd->mutex);

	return 0;
}

int xosd_set_vertical_offset (xosd *osd, int voffset)
{
	if (osd == NULL) return -1;

	pthread_mutex_lock (&osd->mutex);
	osd->voffset = voffset;
	update_pos (osd);
	pthread_mutex_unlock (&osd->mutex);

	return 0;
}

int xosd_set_horizontal_offset (xosd *osd, int hoffset)
{
	if (osd == NULL) return -1;

	pthread_mutex_lock (&osd->mutex);
	osd->hoffset = hoffset;
	update_pos (osd);
	pthread_mutex_unlock (&osd->mutex);

	return 0;
}

int xosd_set_pos (xosd *osd, xosd_pos pos)
{
	if (osd == NULL) return -1;

	pthread_mutex_lock (&osd->mutex);
	osd->pos = pos;
	update_pos (osd);
	pthread_mutex_unlock (&osd->mutex);

	return 0;
}

int xosd_set_align (xosd *osd, xosd_align align)
{
	if (osd == NULL) return -1;

	pthread_mutex_lock (&osd->mutex);
	osd->align = align;
	force_redraw (osd, -1);
	pthread_mutex_unlock (&osd->mutex);

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

/** Change automatic timeout. **/
static void set_timeout (xosd *osd) /* Requires mutex lock. */
{
	assert (osd);
	osd->timeout_time.tv_sec = (osd->timeout > 0)
		? osd->timeout_time.tv_sec = time (NULL) + osd->timeout
		: 0;
	pthread_cond_signal (&osd->cond_time);
}
int xosd_set_timeout (xosd *osd, int timeout)
{
	if (osd == NULL) return -1;
	pthread_mutex_lock (&osd->mutex);
	osd->timeout = timeout;
	set_timeout (osd);
	pthread_mutex_unlock (&osd->mutex);
	return 0;
}


/** Hide current lines. **/
static void hide (xosd *osd) /* Requires mutex lock. */
{
	assert (osd);
	osd->mapped = 0;
	XUnmapWindow (osd->display, osd->window);
	XFlush (osd->display);
	pthread_cond_broadcast(&osd->cond_hide);
}

int xosd_hide (xosd *osd)
{
	if (osd == NULL) return -1;
	if (osd->mapped) {
		pthread_mutex_lock (&osd->mutex);
		hide (osd);
		pthread_mutex_unlock (&osd->mutex);
		return 0;
	}
	return -1;
}

/** Show current lines (again). **/
static void show (xosd *osd) /* Requires mutex lock. */
{
	assert (osd);
	osd->mapped = 1;
	XMapRaised (osd->display, osd->window);
	XFlush (osd->display);
}
int xosd_show (xosd *osd)
{
	if (osd == NULL) return -1;
	if (!osd->mapped) {
		pthread_mutex_lock (&osd->mutex);
		show (osd);
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

	pthread_mutex_lock (&osd->mutex);
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
	pthread_mutex_unlock (&osd->mutex);
	return 0;
}

int xosd_get_number_lines(xosd* osd)
{
	if (osd == NULL) return -1;

	return osd->number_lines;
}
