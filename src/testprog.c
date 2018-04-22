#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "xosd.h"

void printerror() {
  fprintf(stderr, "ERROR: %s\n", xosd_error);
}

int main (int argc, char *argv[])
   {
   xosd *osd;

   osd = xosd_create(2);

   if (!osd) {
     printerror();
     return 1;
   }

   if (0 != xosd_set_timeout(osd, 2)) {
     printerror();
   }

   if (0 != xosd_display (osd, 0, XOSD_string, "Blah")) {
     printerror();
   }
   
   if (0 != xosd_wait_until_no_display(osd)) {
     printerror();
   }
   
   sleep (2);

   if (0 != xosd_display (osd, 0, XOSD_string, "blah2")) {
     printerror();
   }

   sleep (1);

   if (0 != xosd_display (osd, 1, XOSD_string, "wibble")) {
     printerror();
   }

   sleep (1);

   if (0 != xosd_scroll(osd, 1)) {
     printerror();
   }

   if (0 != xosd_display (osd, 1, XOSD_string, "bloggy")) {
     printerror();
   }

   sleep (1);
   
   if (0 != xosd_scroll(osd, 1)) {
     printerror();
   }

   sleep (1);

   if (0 != xosd_scroll(osd, 1)) {
     printerror();
   }

   if (0 != xosd_wait_until_no_display(osd)) {
     printerror();
   }

   if (0 != xosd_uninit (osd)) {
     printerror();
   }


   return EXIT_SUCCESS;
   }
