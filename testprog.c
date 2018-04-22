#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "xosd.h"


/* #define FONT "-misc-fixed-medium-r-*-*-*-300-*-*-*-*-*-*" */

/* #define FONT "fixed" */

#define FONT "-*-lucidatypewriter-medium-r-normal-*-*-250-*-*-*-*-*-*"

int main (int argc, char *argv[])
   {
   xosd *osd, *osd2;
 /*  xosd_pos pos2; */


  
   osd = xosd_init (FONT, "LawnGreen", 3, XOSD_top, 0);
   osd2 = xosd_init (FONT, "Violet", 3, XOSD_bottom, 0);


   xosd_display (osd, 0, XOSD_string, "          *** HP-UX Liverpool Archive ***");   

/*
 pos2=XOSD_bottom;
   xosd_set_pos (osd2, pos2);
*/
   xosd_display (osd2, 1, XOSD_string, "**********************************************************");

   sleep (2);

   xosd_display (osd, 0, XOSD_string, "blah-de-blah");
   
   sleep (2);
 
   xosd_uninit (osd);

   return EXIT_SUCCESS;
   }
