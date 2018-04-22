#include <gtk/gtk.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <xmms/plugin.h>
#include <xmms/xmmsctrl.h>
#include <xmms/configfile.h>

#include <xosd.h>

#define DEBUG(a) fprintf (stderr, "%s: %d: %s\n", __FILE__, __LINE__, a)

static void init(void);
static void cleanup(void);
static gint timeout_func(gpointer);
static void read_config (void);
static void configure (void);

GeneralPlugin gp =
   {
   NULL,		/* handle */
   NULL,		/* filename */
   -1,			/* xmms_session */
   "On Screen Display",	/* Description */
   init,
   NULL,
   configure,                /* Configure */
   cleanup,
   };

xosd *osd;
guint timeout_tag;
int previous_song, previous_length, previous_volume;
gboolean previous_playing, previous_paused, previous_repeat, previous_shuffle;
gchar *font;
gchar *colour;
gint timeout;
gint offset;
gint pos;
GtkObject *timeout_obj, *offset_obj;
GtkWidget *configure_win, *font_entry, *colour_entry, 
   *timeout_spin, *offset_spin, *pos_top, *pos_bottom;

GeneralPlugin *get_gplugin_info(void)
   {
   return &gp;
   }

static void init(void)
   {
   /* font = "-ttf-lucida console-*-r-*-*-60-*-*-*-*-*-*-*"; */
   /* font = "fixed"; */
   /* font = "-misc-fixed-*-*-*-*-40-*-*-*-*-*-*-*"; */
   /* colour = "green"; */

   read_config ();
   
   previous_repeat = previous_shuffle = previous_paused = previous_playing = 
      FALSE;
   previous_volume = previous_length = previous_song = 
      0;   

   osd = xosd_init (font, colour, timeout, pos, offset);
   if (osd)
      timeout_tag = gtk_timeout_add (100, timeout_func, NULL);
   }

static void cleanup(void)
   {
   if (timeout_tag)
      gtk_timeout_remove(timeout_tag);

   timeout_tag = 0;
   
   if (font)
      g_free (font);
   
   if (colour)
      g_free (colour);
   
   xosd_uninit (osd);
   }

static void read_config (void)
   {
   ConfigFile *cfgfile;
   
   g_free (colour);
   g_free (font);
   colour = NULL;
   font = NULL;
   timeout = 3;
   offset = 50;
   pos = XOSD_bottom;
   
   if ((cfgfile = xmms_cfg_open_default_file ()) != NULL)
      {
      xmms_cfg_read_string (cfgfile, "osd", "font", &font);
      xmms_cfg_read_string (cfgfile, "osd", "colour", &colour);
      xmms_cfg_read_int (cfgfile, "osd", "timeout", &timeout);
      xmms_cfg_read_int (cfgfile, "osd", "offset", &offset);
      xmms_cfg_read_int (cfgfile, "osd", "pos", &pos);
      xmms_cfg_free(cfgfile);
      }
   
   if (font == NULL)
      font = g_strdup ("fixed");
   if (colour == NULL)
      colour = g_strdup ("green");
   }

static void configure_ok_cb (gpointer data)
   {
   ConfigFile *cfgfile;

   if (colour)
      g_free (colour);
   if (font)
      g_free (font);
   
   colour = g_strdup (gtk_entry_get_text (GTK_ENTRY (colour_entry)));
   font = g_strdup (gtk_entry_get_text (GTK_ENTRY (font_entry)));
   timeout = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (timeout_spin));
   offset = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (offset_spin));
   if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pos_top)))
      pos = XOSD_top;
   else
      pos = XOSD_bottom;
   
   if (osd)
      {
      xosd_set_colour (osd, colour);
      xosd_set_font (osd, font);
      xosd_set_timeout (osd, timeout);
      xosd_set_offset (osd, offset);
      xosd_set_pos (osd, pos);
      }

   cfgfile = xmms_cfg_open_default_file();
   xmms_cfg_write_string(cfgfile, "osd", "colour", colour);
   xmms_cfg_write_string(cfgfile, "osd", "font", font);
   xmms_cfg_write_int(cfgfile, "osd", "timeout", timeout);
   xmms_cfg_write_int(cfgfile, "osd", "offset", offset);
   xmms_cfg_write_int(cfgfile, "osd", "pos", pos);
   xmms_cfg_write_default_file(cfgfile);
   xmms_cfg_free(cfgfile);
   
   gtk_widget_destroy (configure_win);
   }

static int font_dialog_ok (GtkButton *button, gpointer user_data)
   {
   GtkWidget *font_dialog = user_data;
   char *tmp_font;
   
   assert (GTK_IS_FONT_SELECTION_DIALOG (font_dialog));
   
   tmp_font = gtk_font_selection_dialog_get_font_name 
      (GTK_FONT_SELECTION_DIALOG (font_dialog));
   
   gtk_entry_set_text (GTK_ENTRY (font_entry), tmp_font);
   
   gtk_widget_destroy (font_dialog);
   
   return 0;
   }

static int font_dialog_apply (GtkButton *button, gpointer user_data)
   {
   GtkWidget *font_dialog = user_data;
   char *tmp_font;
   
   assert (GTK_IS_FONT_SELECTION_DIALOG (font_dialog));
   
   tmp_font = gtk_font_selection_dialog_get_font_name 
      (GTK_FONT_SELECTION_DIALOG (font_dialog));
   
   gtk_entry_set_text (GTK_ENTRY (font_entry), tmp_font);
   
   return 0;
   }

static int font_dialog_window (GtkButton *button, gpointer user_data)
   {
   GtkWidget *font_dialog;
   GtkWidget *vbox;
   GtkWidget *cancel_button, *apply_button, *ok_button;
   GList *children;

   font_dialog = gtk_font_selection_dialog_new ("XOSD Font");
   
   assert (font_dialog);
   
   if (font)
      gtk_font_selection_dialog_set_font_name 
                 (GTK_FONT_SELECTION_DIALOG (font_dialog), font);
   
   children = gtk_container_children (GTK_CONTAINER (font_dialog));

   vbox = GTK_WIDGET (children->data);
   
   children = gtk_container_children (GTK_CONTAINER (vbox));
   
   vbox = GTK_WIDGET (children->next->data);
   children = gtk_container_children (GTK_CONTAINER (vbox));
   ok_button = GTK_WIDGET (children->data);

   apply_button = GTK_WIDGET (children->next->data);
   
   cancel_button = GTK_WIDGET (children->next->next->data);

   gtk_signal_connect_object (GTK_OBJECT (cancel_button), "clicked",
			      GTK_SIGNAL_FUNC (gtk_widget_destroy), 
			      GTK_OBJECT (font_dialog));
   
   gtk_signal_connect (GTK_OBJECT (ok_button), "clicked",
		       GTK_SIGNAL_FUNC (font_dialog_ok), font_dialog);
   gtk_signal_connect (GTK_OBJECT (apply_button), "clicked",
		       GTK_SIGNAL_FUNC (font_dialog_apply), font_dialog);


   gtk_widget_show_all (font_dialog);
   return 0;
   }

static int colour_dialog_ok (GtkButton *button, gpointer user_data)
   {
   GtkWidget *colour_dialog = user_data;
   char tmp_colour[8];
   double colour[4];
   
   assert (GTK_IS_COLOR_SELECTION_DIALOG (colour_dialog));
   
   gtk_color_selection_get_color
      (GTK_COLOR_SELECTION (GTK_COLOR_SELECTION_DIALOG
			        (colour_dialog)->colorsel), colour);
   
   sprintf (tmp_colour, "#%2.2x%2.2x%2.2x", 
	    (int)(colour[0] * 255), (int)(colour[1] * 255),(int)(colour[2] * 255));
   
   gtk_entry_set_text (GTK_ENTRY (colour_entry), tmp_colour);
   
   gtk_widget_destroy (colour_dialog);
   
   return 0;
   }

static int colour_dialog_window (GtkButton *button, gpointer user_data)
   {
   GtkWidget *colour_dialog;
   GtkWidget *cancel_button, *ok_button, *colour_widget;
   gdouble colour[4];
   int red, green, blue;

   colour_dialog = gtk_color_selection_dialog_new ("XOSD Colour");
   
   assert (colour_dialog);
   
   colour_widget = GTK_COLOR_SELECTION_DIALOG (colour_dialog)->colorsel;
   if (osd)
      {
      xosd_get_colour (osd, &red, &green, &blue);
   
      colour[0] = (float)red / (float)USHRT_MAX;
      colour[1] = (float)green / (float)USHRT_MAX;
      colour[2] = (float)blue / (float)USHRT_MAX;
   
      gtk_color_selection_set_color (GTK_COLOR_SELECTION 
				     (GTK_COLOR_SELECTION_DIALOG 
				      (colour_dialog)->colorsel), colour);
      }
   
   ok_button = GTK_COLOR_SELECTION_DIALOG (colour_dialog)->ok_button;
   cancel_button = GTK_COLOR_SELECTION_DIALOG (colour_dialog)->cancel_button;

   gtk_signal_connect_object (GTK_OBJECT (cancel_button), "clicked",
			      GTK_SIGNAL_FUNC (gtk_widget_destroy), 
			      GTK_OBJECT (colour_dialog));
   
   gtk_signal_connect (GTK_OBJECT (ok_button), "clicked",
		       GTK_SIGNAL_FUNC (colour_dialog_ok), colour_dialog);

   gtk_widget_show_all (colour_dialog);
   return 0;
   }

static void configure (void)
   {
   GtkWidget *vbox, *bbox, *ok, *cancel, *apply, *hbox, *label, 
      *button, *unit_label;
   GSList *group = NULL;
   
   if (configure_win)
      return;
   
   read_config ();

   configure_win = gtk_window_new (GTK_WINDOW_DIALOG);
   
   gtk_signal_connect (GTK_OBJECT (configure_win), "destroy", 
		       GTK_SIGNAL_FUNC (gtk_widget_destroyed), &configure_win);

   gtk_window_set_title (GTK_WINDOW (configure_win), 
			 "On Screen Display Configuration - " XOSD_VERSION);
   
   vbox = gtk_vbox_new (TRUE, 10);
   gtk_container_add (GTK_CONTAINER (configure_win), vbox);
   gtk_container_set_border_width (GTK_CONTAINER (configure_win), 5);
   
   hbox = gtk_hbox_new (FALSE, 5);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
   label = gtk_label_new ("Font:");
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
   font_entry = gtk_entry_new ();
   if (font)
      gtk_entry_set_text (GTK_ENTRY (font_entry), font);
   gtk_box_pack_start (GTK_BOX (hbox), font_entry, TRUE, TRUE, 0);
   button = gtk_button_new_with_label ("Set");
   gtk_signal_connect (GTK_OBJECT (button), "clicked",
		       GTK_SIGNAL_FUNC (font_dialog_window), NULL);
   gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);


   hbox = gtk_hbox_new (FALSE, 5);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
   label = gtk_label_new ("Colour:");
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
   colour_entry = gtk_entry_new ();
   if (colour)
      gtk_entry_set_text (GTK_ENTRY (colour_entry), colour);
   gtk_box_pack_start (GTK_BOX (hbox), colour_entry, TRUE, TRUE, 0);
   button = gtk_button_new_with_label ("Set");
   gtk_signal_connect (GTK_OBJECT (button), "clicked",
		       GTK_SIGNAL_FUNC (colour_dialog_window), NULL);
   gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);


   hbox = gtk_hbox_new (FALSE, 5);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
   label = gtk_label_new ("Timeout:");
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
   timeout_obj = gtk_adjustment_new (timeout, -1, 60, 1, 1, 1);
   timeout_spin = gtk_spin_button_new (GTK_ADJUSTMENT (timeout_obj), 1.0, 0);
   if (timeout)
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (timeout_spin), 
				 (gfloat) timeout);
   gtk_box_pack_start (GTK_BOX (hbox), timeout_spin, FALSE, FALSE, 0);
   unit_label = gtk_label_new ("seconds");
   gtk_box_pack_start (GTK_BOX (hbox), unit_label, FALSE, FALSE, 0);

   
   hbox = gtk_hbox_new (FALSE, 5);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
   label = gtk_label_new ("Vertical Offset:");
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
   offset_obj = gtk_adjustment_new (timeout, 0, 60, 1, 1, 1);
   offset_spin = gtk_spin_button_new (GTK_ADJUSTMENT (offset_obj), 1.0, 0);
   if (offset)
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (offset_spin),
				 (gfloat) offset);
   gtk_box_pack_start (GTK_BOX (hbox), offset_spin, FALSE, FALSE, 0);   
   
   hbox = gtk_hbox_new (FALSE, 5);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
   label = gtk_label_new ("Position:");
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
   pos_top = gtk_radio_button_new_with_label (NULL, "Top");
   group = gtk_radio_button_group (GTK_RADIO_BUTTON (pos_top));
   pos_bottom = gtk_radio_button_new_with_label (group, "Bottom");
   gtk_box_pack_start (GTK_BOX (hbox), pos_top, FALSE, FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox), pos_bottom, FALSE, FALSE, 0);

   if (pos == XOSD_top)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pos_top), TRUE);
   else
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pos_bottom), TRUE);

   bbox = gtk_hbutton_box_new ();
   gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
   gtk_button_box_set_spacing (GTK_BUTTON_BOX (bbox), 5);
   gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);
   
   ok = gtk_button_new_with_label ("Ok");
   gtk_signal_connect (GTK_OBJECT (ok), "clicked", 
		       GTK_SIGNAL_FUNC (configure_ok_cb), NULL);
   GTK_WIDGET_SET_FLAGS (ok, GTK_CAN_DEFAULT);
   gtk_box_pack_start (GTK_BOX (bbox), ok, TRUE, TRUE, 0);
   gtk_widget_grab_default (ok);
   
   cancel = gtk_button_new_with_label ("Cancel");
   gtk_signal_connect_object (GTK_OBJECT (cancel), "clicked",
			      GTK_SIGNAL_FUNC (gtk_widget_destroy), 
			      GTK_OBJECT (configure_win));
   GTK_WIDGET_SET_FLAGS (cancel, GTK_CAN_DEFAULT);
   gtk_box_pack_start (GTK_BOX (bbox), cancel, TRUE, TRUE, 0);
   
   
   gtk_widget_show_all (configure_win);
   }

static void replace_hexcodes (gchar *text)
   {
   gchar hex_number[] = "FF";
   gchar *tmp, *tmp2;

   while ((tmp = strchr(text, '%')) != NULL)
      {
      /* Make sure we're not at the end of the string */
      if ((tmp+1) && (tmp+2))
	 {
	 tmp2 = tmp + 3;
	 hex_number[0] = *(tmp+1);
	 hex_number[1] = *(tmp+2);

	 *(tmp++) = (char) strtol(hex_number, NULL, 16);

	 text = tmp;

	 while (*tmp2)
	    *(tmp++) = *(tmp2++);

	 *tmp = '\0';	 
	 }
      }

   }

static gint timeout_func(gpointer data)
   {
   gint pos, length, volume;
   gboolean playing, paused, repeat, shuffle;
   gchar *text;

   if (!osd)
      return FALSE;
   
   GDK_THREADS_ENTER();

   pos = xmms_remote_get_playlist_pos (gp.xmms_session);
   length = xmms_remote_get_playlist_time (gp.xmms_session, pos);
   playing = xmms_remote_is_playing (gp.xmms_session);
   paused = xmms_remote_is_paused (gp.xmms_session);
   volume = xmms_remote_get_main_volume (gp.xmms_session);
   shuffle = xmms_remote_is_shuffle (gp.xmms_session);
   repeat = xmms_remote_is_repeat (gp.xmms_session);
   
   if (pos != previous_song || length != previous_length)
      {
      xosd_display (osd, 0, XOSD_string, playing ? "Play" : "Stopped");
      
      if (xmms_remote_get_playlist_length (gp.xmms_session)) /* otherwise it'll crash */
	 {
	 text = xmms_remote_get_playlist_title (gp.xmms_session, pos);
	 replace_hexcodes (text);
	 xosd_display (osd, 1, XOSD_string, text);		       
	 }

      previous_song = pos;
      previous_length = length;      
      }
   else if (playing != previous_playing)
      {
      if (playing)
	 {
	 xosd_display (osd, 0, XOSD_string, "Play");
	 text = xmms_remote_get_playlist_title (gp.xmms_session, pos);
	 replace_hexcodes (text);
	 xosd_display (osd, 1, XOSD_string, text);		       
	 }
      else
	 {
	 xosd_display (osd, 0, XOSD_string, "Stop");
	 xosd_display (osd, 1, XOSD_string, "");      
	 }
	      
      previous_playing = playing;
      }

   else if (paused != previous_paused)
      {
      if (paused)
	 {
	 xosd_display (osd, 0, XOSD_string, "Paused");
	 xosd_display (osd, 1, XOSD_string, "");
	 }
      else
	 {
	 xosd_display (osd, 0, XOSD_string, "Unpaused");
	 text = xmms_remote_get_playlist_title (gp.xmms_session, pos);
	 replace_hexcodes (text);
	 xosd_display (osd, 1, XOSD_string, text);		       
	 }
      previous_paused = paused;
      }

   else if (volume != previous_volume)
      {
      xosd_display (osd, 0, XOSD_string, "Volume");
      xosd_display (osd, 1, XOSD_percentage, volume);
      previous_volume = volume;
      }

   else if (repeat != previous_repeat)
      {
      xosd_display (osd, 0, XOSD_string, "Repeat");
      xosd_display (osd, 1, XOSD_string, repeat ? "On" : "Off");

      previous_repeat = repeat;
      }
   
   else if (shuffle != previous_shuffle)
      {
      xosd_display (osd, 0, XOSD_string, "Shuffle");
      xosd_display (osd, 1, XOSD_string, shuffle ? "On" : "Off");

      previous_shuffle = shuffle;
      }
   
   GDK_THREADS_LEAVE();

   return TRUE;
   }
