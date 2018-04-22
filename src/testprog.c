#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "xosd.h"


/* #define FONT "-misc-fixed-medium-r-*-*-*-300-*-*-*-*-*-*" */

/* #define FONT "fixed" */

#define FONT "-*-lucidatypewriter-medium-r-normal-*-*-250-*-*-*-*-*-*"
int main (int argc, char *argv[])
   {
   xosd *osd;
   
   osd = xosd_init (FONT, "LawnGreen", 3, XOSD_top, 0, 1, 2);
   
   xosd_display (osd, 0, XOSD_string, "Blah");
   xosd_wait_until_no_display(osd);

   sleep (2);

   xosd_display (osd, 0, XOSD_string, "blah2");   
   sleep (2);
   xosd_display (osd, 1, XOSD_string, "wibble");
   
   xosd_wait_until_no_display(osd);
   
   xosd_uninit (osd);
   
   return EXIT_SUCCESS;
   }
