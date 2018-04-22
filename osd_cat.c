#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xosd.h>

int main (int argc, char *argv[])
   {
   FILE *fp;
   xosd *osd;
   char buffer [1024];
   char *newline;
   
   if (argc != 1)
      {
      if ((fp = fopen (argv[1], "r")) == NULL)
	 {
	 fprintf (stderr, "Unable to open: %s\n", argv[1]);
	 return EXIT_FAILURE;
	 }
      }
   else
      fp = stdin;
   
   osd = xosd_init ("fixed", "red", 5, XOSD_top, 0);
   if (!osd)
      {
      fprintf (stderr, "Error initializing osd\n");
      return EXIT_FAILURE;
      }
   
   while (!feof (fp))
      {
      fgets (buffer, 1023, fp);
      if ((newline = strchr (buffer, '\n')))
	 newline[0] = '\0';
      xosd_display (osd, 1, XOSD_string, buffer);
      }

   fclose (fp);

   sleep(delay);
   
   xosd_uninit (osd);
   
   return EXIT_SUCCESS;
   }
