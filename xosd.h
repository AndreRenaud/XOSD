#ifndef XOSD_H
#define XOSD_H

typedef struct xosd xosd;

typedef enum
   {
   XOSD_percentage,
   XOSD_string,
   XOSD_printf
   } xosd_command;

typedef enum
   {
   XOSD_top,
   XOSD_bottom
   } xosd_pos;

xosd *xosd_init (char *font, char *colour, int timeout, 
		 xosd_pos pos, int offset);
int xosd_uninit (xosd *osd);
int xosd_display (xosd *osd, int line, xosd_command command, ...);

int xosd_hide (xosd *osd);
int xosd_show (xosd *osd);

int xosd_set_pos (xosd *osd, xosd_pos pos);
int xosd_set_offset (xosd *osd, int offset);	 
int xosd_set_timeout (xosd *osd, int timeout);
int xosd_set_colour (xosd *osd, char *colour);
int xosd_set_font (xosd *osd, char *font);
int xosd_get_colour (xosd *osd, int *red, int *green, int *blue);

#endif
