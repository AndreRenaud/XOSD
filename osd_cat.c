#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xosd.h>
#include <unistd.h>
#include <getopt.h>

static struct option long_options[] = {
    {"font",   1, NULL, 'f'},
    {"color",  1, NULL, 'c'},
    {"delay",  1, NULL, 'd'},
    {"offset", 1, NULL, 'o'},
    {"top",    0, NULL, 't'},
    {"bottom", 0, NULL, 'b'},
    {"help",   0, NULL, 'h'},
    {NULL,     0, NULL, 0}
};

int main (int argc, char *argv[])
   {
   FILE *fp;
   xosd *osd;
   char buffer [1024];
   char *newline;

   char *font = "fixed";
   char *color = "red";
   int delay = 5;
   xosd_pos pos = XOSD_top;
   int offset = 0;

   while (1)
   {
       int option_index = 0;
       int c = getopt_long (argc, argv, "f:c:d:o:tbh", long_options, &option_index);
       if (c == -1) break;
       switch (c)
       {
	   case 'f':
	       font = optarg;
	       break;
	   case 'c':
	       color = optarg;
	       break;
	   case 'd':
	       delay = atoi(optarg);
	       break;
	   case 'o':
	       offset = atoi(optarg);
	       break;
	   case 't':
	       pos = XOSD_top;
	       break;
	   case 'b':
	       pos = XOSD_bottom;
	       break;
	   case '?':
	   case 'h':
	       fprintf (stderr, "Usage: %s [OPTION] [FILE]...\n", argv[0]);
	       fprintf (stderr, "Display FILE, or standard input, on top of display.\n\n");
	       fprintf (stderr, "  -t, --top           Display at top of screen\n");
	       fprintf (stderr, "  -b, --bottom        Display at bottom of screen\n");
	       fprintf (stderr, "  -f, --font=FONT     Use font\n");
	       fprintf (stderr, "  -c, --color=COLOR   Use color\n");
	       fprintf (stderr, "  -d, --delay=TIME    Show for specified time\n");
	       fprintf (stderr, "  -o, --offset=OFFSET Display Offset\n");
	       fprintf (stderr, "  -h, --help          Show this help\n");
	       fprintf (stderr, "\nWith no FILE, or when FILE is -, read standard input.\n");
	       return EXIT_SUCCESS;
       }
   }
   
   if ((optind < argc) && strncmp(argv[optind], "-", 2))
      {
      if ((fp = fopen (argv[optind], "r")) == NULL)
	 {
	 fprintf (stderr, "Unable to open: %s\n", argv[optind]);
	 return EXIT_FAILURE;
	 }
      }
   else
      fp = stdin;
   
   osd = xosd_init (font, color, delay, pos, offset, 0);
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
/* vim: ai si sw=4
 */
