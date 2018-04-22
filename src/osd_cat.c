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
  {"colour",  1, NULL, 'c'},
  {"indent",  1, NULL, 'i'},
  {"delay",  1, NULL, 'd'},
  {"offset", 1, NULL, 'o'},
  {"pos",    1, NULL, 'p'},
  {"align",  1, NULL, 'A'},
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

  char *font = (char*) osd_default_font;
  char *colour = "red";
  int delay = 5;
  int forcewait=0;
  xosd_pos pos = XOSD_top;
  int voffset = 0;
  int hoffset = 0;
  int shadow = 0;
  int scroll_age = 0;
  struct timeval old_age,new_age;
  int screen_line = 0;
  int lines=5;
  xosd_align align=XOSD_left;

  while (1)
    {
      int option_index = 0;
      int c = getopt_long (argc, argv, "l:A:a:f:c:d:o:i:s:p:hw", long_options, &option_index);
      if (c == -1) break;
      switch (c)
	{
	case 'a':
	  scroll_age=atoi(optarg);
	  break;
	case 'w':
	  forcewait=1;
	  break;
	case 'A':
	  if (strcasecmp(optarg,"left")==0) {
	    align=XOSD_left;
	  } else if (strcasecmp(optarg,"right")==0) {
	    align=XOSD_right;
	  } else if (strcasecmp(optarg,"center")==0) {
	    align=XOSD_center;
	  } else if (strcasecmp(optarg,"centre")==0) {
	    align=XOSD_center;
	  } else {
	    fprintf (stderr, "Unknown alignment: %s\n", optarg);
	    return EXIT_FAILURE;
	  }
	  break;
	case 'p':
	  if (strcasecmp(optarg,"top")==0) {
	    pos=XOSD_top;
	  } else if (strcasecmp(optarg,"middle")==0) {
	    pos=XOSD_middle;
	  } else if (strcasecmp(optarg,"bottom")==0) {
	    pos=XOSD_bottom;
	  } else {
	    fprintf (stderr, "Unknown alignment: %s\n", optarg);
	    return EXIT_FAILURE;
	  }
	  break;
	case 'f':
	  font = optarg;
	  break;
	case 'c':
	  colour = optarg;
	  break;
	case 'd':
	  delay = atoi(optarg);
	  break;
	case 'o':
	  voffset = atoi(optarg);
	  break;
	case 'i':
	  hoffset = atoi(optarg);
	  break;
	case 's':
	  shadow=atoi(optarg);
	  break;
	case 'l':
	  lines=atoi(optarg);
	  break;
	case '?':
	case 'h':
	default:
	  fprintf (stderr, "Usage: %s [OPTION] [FILE]...\n", argv[0]);
	  fprintf (stderr, "Version: %s \n", XOSD_VERSION);
	  fprintf (stderr, "Display FILE, or standard input, on top of display.\n\n");
	  fprintf (stderr, "  -a, --age           Time in seconds before old scroll lines are discarded\n");
	  fprintf (stderr, "  -p, --pos=(top|middle|bottom)\n");
	  fprintf (stderr, "                      Display at top/middle/bottom of screen. Top is default\n");
	  fprintf (stderr, "  -A, --align=(left|right|center)\n");
	  fprintf (stderr, "                      Display at left/right/center of screen.Left is default\n");
	  fprintf (stderr, "  -f, --font=FONT     Use font (default: %s)\n", osd_default_font);
	  fprintf (stderr, "  -c, --colour=COLOUR Use colour\n");
	  fprintf (stderr, "  -d, --delay=TIME    Show for specified time\n");
	  fprintf (stderr, "  -o, --offset=OFFSET Vertical Offset\n");
	  fprintf (stderr, "  -i, --indent=OFFSET Horizontal Offset\n");
	  fprintf (stderr, "  -h, --help          Show this help\n");
	  fprintf (stderr, "  -s, --shadow=SHADOW Offset of shadow, default is 0 which is no shadow\n");
	  fprintf (stderr, "  -w, --wait          Delay display even when new lines are ready\n");
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

  osd = xosd_create (lines);
  if (!osd)
    {
      fprintf (stderr, "Error initializing osd: %s\n", xosd_error);
      return EXIT_FAILURE;
    }

  if (xosd_set_font(osd, font))
    {
      /* This is critical, because fontset=NULL, will segfault later! */
      fprintf (stderr, "ABORT: %s\n", xosd_error);
      return EXIT_FAILURE;
    }
  xosd_set_colour(osd, colour);
  xosd_set_timeout(osd, delay);
  xosd_set_pos(osd, pos);
  xosd_set_vertical_offset(osd, voffset);
  xosd_set_horizontal_offset(osd, hoffset);
  xosd_set_shadow_offset(osd, shadow);
  xosd_set_align(osd, align);
  /* Not really needed, but at least we aren't throwing around an unknown value */
  old_age.tv_sec=0;

  if (scroll_age)
    gettimeofday(&old_age,0);

  while (!feof (fp))
    {
      if (fgets (buffer, sizeof(buffer)-1, fp))
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

  xosd_destroy (osd);

  return EXIT_SUCCESS;
}
/* vim: ai si sw=4
 */
