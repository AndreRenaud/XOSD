#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xosd.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/time.h>

static struct option long_options[] = {
  {"font",   1, NULL, 'f'},
  {"color",  1, NULL, 'c'},
  {"delay",  1, NULL, 'd'},
  {"offset", 1, NULL, 'o'},
  {"top",    0, NULL, 't'},
  {"bottom", 0, NULL, 'b'},
  {"help",   0, NULL, 'h'},
  {"shadow", 1, NULL, 's'}, 
  {"age",    1, NULL, 'a'},
  {"lines",  1, NULL, 'l'},
  {"wait", 0, NULL, 'w'},
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
  int forcewait=0;
  xosd_pos pos = XOSD_top;
  int offset = 0;
  int shadow = 0;
  int scroll_age = 0;
  struct timeval old_age,new_age;
  int screen_line = 0;
  int lines=5;

  while (1)
    {
      int option_index = 0;
      int c = getopt_long (argc, argv, "l:a:f:c:d:o:s:tbhw", long_options, &option_index);
      if (c == -1) break;
      switch (c)
	{
	case 'a':
	  scroll_age=atoi(optarg);
	  break;
	case 'w':
	  forcewait=1;
	  break;

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
	case 's':
	  shadow=atoi(optarg);
	  break;
	case 'l':
	  lines=atoi(optarg);
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
	  fprintf (stderr, "  -a, --age           Time in seconds before old scroll lines are discarded\n");
	  fprintf (stderr, "  -t, --top           Display at top of screen\n");
	  fprintf (stderr, "  -b, --bottom        Display at bottom of screen\n");
	  fprintf (stderr, "  -f, --font=FONT     Use font\n");
	  fprintf (stderr, "  -c, --color=COLOR   Use color\n");
	  fprintf (stderr, "  -d, --delay=TIME    Show for specified time\n");
	  fprintf (stderr, "  -o, --offset=OFFSET Display Offset\n");
	  fprintf (stderr, "  -h, --help          Show this help\n");
	  fprintf (stderr, "  -s, --shadow=SHADOW Offset of shadow, default is 0 which is no shadow\n");
	  fprintf (stderr, "  -i, --immediate     Immediatly display new data when ready\n");
	  fprintf (stderr, "  -l, --lines=n       Scroll using n lines. Default is 5.\n");
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
   
  osd = xosd_init (font, color, delay, pos, offset, shadow, lines);
  if (!osd)
    {
      fprintf (stderr, "Error initializing osd\n");
      return EXIT_FAILURE;
    }
   
  /* Not really needed, but at least we aren't throwing around an unknown value */
  old_age.tv_sec=0;

  if (scroll_age)
    gettimeofday(&old_age,0);

  while (!feof (fp))
    {
      if (fgets (buffer, 1023, fp))
	{
	  /* Should we age the display? */
	  if (scroll_age)
	    {
	      gettimeofday(&new_age,0);
	      if ((new_age.tv_sec - old_age.tv_sec) > scroll_age)
		{
		  xosd_scroll(osd,xosd_get_number_lines(osd));
		  screen_line=0;
		}
	    }
	      
	  if (screen_line >= xosd_get_number_lines(osd))
	    {
	      xosd_scroll(osd,1);
	      screen_line = xosd_get_number_lines(osd)-1;
	    }
	  if ((newline = strchr (buffer, '\n')))
	    newline[0] = '\0';
	  
	  if (forcewait && xosd_is_onscreen(osd)) {
	    xosd_wait_until_no_display(osd);
	  }

	  xosd_display (osd, screen_line, XOSD_string, buffer);
	  screen_line++;
	}
      else
	{
	  if (!feof(fp))
	    {
	      fprintf(stderr,"Error occured reading input file: %s\n",strerror(errno));
	      exit(1);
	    }
	}
      old_age.tv_sec = new_age.tv_sec;
    }
  fclose (fp);

  if (xosd_is_onscreen(osd)) {
    xosd_wait_until_no_display(osd);
  }
   
  xosd_uninit (osd);
   
  return EXIT_SUCCESS;
}
/* vim: ai si sw=4
 */
