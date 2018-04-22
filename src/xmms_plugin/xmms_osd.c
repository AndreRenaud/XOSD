/* XOSD

Copyright (c) 2001 Andre Renaud (andre@ignavus.net)

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

#define DEBUG(a) /* fprintf (stderr, "%s: %d: %s\n", __FILE__, __LINE__, a) */

static void init(void);
static void cleanup(void);
static gint timeout_func(gpointer);
static void read_config (void);
static void configure (void);
static show_item(GtkWidget* vbox, const char* description, int selected, GtkToggleButton** on);
static void save_previous_title ( gchar * title );

GeneralPlugin gp =
  {
    .handle = NULL,
    .filename = NULL,
    .xmms_session = -1,
    .description = "On Screen Display",
    .init = init,
    .about = NULL,
    .configure = configure,
    .cleanup = cleanup,
  };

static xosd *osd=NULL;
static guint timeout_tag;
static int previous_song, previous_volume, previous_balance;
static gchar * previous_title = 0;
static gboolean previous_playing, previous_paused, previous_repeat, previous_shuffle;
static gchar *font;
static gchar *colour;

static gint timeout;
static gint offset;
static gint shadow_offset;
static gint pos;

static gboolean show_volume;
static gboolean show_balance;
static gboolean show_pause;
static gboolean show_trackname;
static gboolean show_stop;
static gboolean show_repeat;
static gboolean show_shuffle;

static GtkObject *timeout_obj, *offset_obj, *shadow_obj;
static GtkWidget *configure_win, *font_entry, *colour_entry,
  *timeout_spin, *offset_spin, *pos_top, *pos_bottom, *shadow_spin;

static GtkToggleButton
  *vol_on, *bal_on,
  *pause_on,  *trackname_on,
  *stop_on,  *repeat_on,
  *shuffle_on;

/*
 * Return plugin structure.
 */
GeneralPlugin *get_gplugin_info(void)
{
  return &gp;
}


/*
 * Initialize plugin.
 */
static void init(void)
{
  /* font = "-ttf-lucida console-*-r-*-*-60-*-*-*-*-*-*-*"; */
  /* font = "fixed"; */
  /* font = "-misc-fixed-*-*-*-*-40-*-*-*-*-*-*-*"; */
  /* colour = "green"; */

  DEBUG("init");

  if (osd) {
    DEBUG("uniniting osd");
    xosd_uninit(osd);
    osd=NULL;
  }

  read_config ();

  previous_repeat = previous_shuffle = previous_paused = previous_playing =
    FALSE;
  previous_volume = previous_song =
    0;
  previous_title = 0;

  osd = xosd_init (font, colour, timeout, pos, offset, shadow_offset, 2);

  if (osd)
    timeout_tag = gtk_timeout_add (100, timeout_func, NULL);
}

/*
 * Free memory and release resources.
 */
static void cleanup(void)
{
  DEBUG("cleanup");
  assert(osd);
  if (timeout_tag)
    gtk_timeout_remove(timeout_tag);
  timeout_tag = 0;

  if (font) {
    g_free (font);
    font=NULL;
  }

  if (colour) {
    g_free (colour);
    colour=NULL;
  }

  save_previous_title(NULL);

  xosd_hide (osd);
  xosd_uninit (osd);
  osd=NULL;
}

/*
 * Read configuration and initialize variables.
 */
static void read_config (void)
{

  ConfigFile *cfgfile;

  show_volume = 1;
  show_balance = 1;
  show_pause = 1;
  show_trackname = 1;
  show_stop = 1;
  show_repeat = 1;
  show_shuffle = 1;

  g_free (colour);
  g_free (font);
  colour = NULL;
  font = NULL;
  timeout = 3;
  offset = 50;
  shadow_offset = 1;
  pos = XOSD_bottom;

  DEBUG("read config");
  if ((cfgfile = xmms_cfg_open_default_file ()) != NULL)
    {
      xmms_cfg_read_string (cfgfile, "osd", "font", &font);
      xmms_cfg_read_string (cfgfile, "osd", "colour", &colour);
      xmms_cfg_read_int (cfgfile, "osd", "timeout", &timeout);
      xmms_cfg_read_int (cfgfile, "osd", "offset", &offset);
      xmms_cfg_read_int (cfgfile, "osd", "pos", &pos);
      xmms_cfg_read_int (cfgfile, "osd", "shadow_offset", &shadow_offset);
      xmms_cfg_read_int (cfgfile, "osd", "show_volume", &show_volume );
      xmms_cfg_read_int (cfgfile, "osd", "show_balance", &show_balance );
      xmms_cfg_read_int (cfgfile, "osd", "show_pause", &show_pause );
      xmms_cfg_read_int (cfgfile, "osd", "show_trackname", &show_trackname );
      xmms_cfg_read_int (cfgfile, "osd", "show_stop", &show_stop );
      xmms_cfg_read_int (cfgfile, "osd", "show_repeat", &show_repeat );
      xmms_cfg_read_int (cfgfile, "osd", "show_shuffle", &show_shuffle );
      xmms_cfg_free(cfgfile);
    }

  if (font == NULL)
    font = g_strdup ("fixed");
  if (colour == NULL)
    colour = g_strdup ("green");
}

/*
 * Return state of check button.
 */
static gboolean isactive(GtkToggleButton *item) {
  return gtk_toggle_button_get_active (item)? 1 : 0;
}


/*
 * Apply changed from configuration dialog.
 */
static void configure_apply_cb (gpointer data)

{

  ConfigFile *cfgfile;

  show_volume=isactive(vol_on);
  show_balance=isactive(bal_on);
  show_pause=isactive(pause_on);
  show_trackname=isactive(trackname_on);
  show_stop=isactive(stop_on);
  show_repeat=isactive(repeat_on);
  show_shuffle=isactive(shuffle_on);


  if (colour)
    g_free (colour);
  if (font)
    g_free (font);

  colour = g_strdup (gtk_entry_get_text (GTK_ENTRY (colour_entry)));
  font = g_strdup (gtk_entry_get_text (GTK_ENTRY (font_entry)));
  timeout = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (timeout_spin));
  offset = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (offset_spin));
  shadow_offset = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (shadow_spin));
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pos_top)))
    pos = XOSD_top;
  else
    pos = XOSD_bottom;

  if (osd)
    {
      xosd_set_colour (osd, colour);
      if (xosd_set_font (osd, font) == -1) {
	DEBUG("invalid font");
	DEBUG(font);
      }
      xosd_set_timeout (osd, timeout);
      xosd_set_offset (osd, offset);
      xosd_set_shadow_offset (osd, shadow_offset);
      xosd_set_pos (osd, pos);
    }

  cfgfile = xmms_cfg_open_default_file();
  xmms_cfg_write_string(cfgfile, "osd", "colour", colour);
  xmms_cfg_write_string(cfgfile, "osd", "font", font);
  xmms_cfg_write_int(cfgfile, "osd", "timeout", timeout);
  xmms_cfg_write_int(cfgfile, "osd", "offset", offset);
  xmms_cfg_write_int(cfgfile, "osd", "shadow_offset", shadow_offset);
  xmms_cfg_write_int(cfgfile, "osd", "pos", pos);

  xmms_cfg_write_int (cfgfile, "osd", "show_volume", show_volume );
  xmms_cfg_write_int (cfgfile, "osd", "show_balance", show_balance );
  xmms_cfg_write_int (cfgfile, "osd", "show_pause", show_pause );
  xmms_cfg_write_int (cfgfile, "osd", "show_trackname", show_trackname );
  xmms_cfg_write_int (cfgfile, "osd", "show_stop", show_stop );
  xmms_cfg_write_int (cfgfile, "osd", "show_repeat", show_repeat );
  xmms_cfg_write_int (cfgfile, "osd", "show_shuffle", show_shuffle );

  xmms_cfg_write_default_file(cfgfile);
  xmms_cfg_free(cfgfile);
}

/*
 * Apply changes and close configuration dialog.
 */
static void configure_ok_cb (gpointer data)
{
  DEBUG("configure_ok_cb");
  configure_apply_cb (data);

  gtk_widget_destroy (configure_win);
  configure_win = NULL;
}

/*
 * Apply font change and close dialog.
 */
static int font_dialog_ok (GtkButton *button, gpointer user_data)
{
  GtkWidget *font_dialog = user_data;
  char *tmp_font;
  DEBUG("font_dialog_ok");

  assert (GTK_IS_FONT_SELECTION_DIALOG (font_dialog));

  tmp_font = gtk_font_selection_dialog_get_font_name
    (GTK_FONT_SELECTION_DIALOG (font_dialog));

  gtk_entry_set_text (GTK_ENTRY (font_entry), tmp_font);

  gtk_widget_destroy (font_dialog);

  return 0;
}

/*
 * Apply font change and close dialog.
 */
static int font_dialog_apply (GtkButton *button, gpointer user_data)
{
  GtkWidget *font_dialog = user_data;
  char *tmp_font;
  DEBUG("font_dialog_apply");

  assert (GTK_IS_FONT_SELECTION_DIALOG (font_dialog));

  tmp_font = gtk_font_selection_dialog_get_font_name
    (GTK_FONT_SELECTION_DIALOG (font_dialog));

  gtk_entry_set_text (GTK_ENTRY (font_entry), tmp_font);

  return 0;
}

/*
 * Create dialog window for font selection.
 */
static int font_dialog_window (GtkButton *button, gpointer user_data)
{
  GtkWidget *font_dialog;
  GtkWidget *vbox;
  GtkWidget *cancel_button, *apply_button, *ok_button;
  GList *children;

  DEBUG("font_dialog_window");
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

/*
 * Apply colour changes and close window.
 */
static int colour_dialog_ok (GtkButton *button, gpointer user_data)
{
  GtkWidget *colour_dialog = user_data;
  char tmp_colour[8];
  double colour[4];

  DEBUG("colour_dialog_ok");
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

/*
 * Create dialog window for colour selection.
 */
static int colour_dialog_window (GtkButton *button, gpointer user_data)
{
  GtkWidget *colour_dialog;
  GtkWidget *cancel_button, *ok_button, *colour_widget;
  gdouble colour[4];
  int red, green, blue;

  DEBUG("colour_dialog_window");
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

/*
 * Create dialog window for configuration.
 */
static void configure (void)
{
  GtkWidget *vbox, *bbox, *ok, *cancel, *apply, *hbox, *label,
    *button, *unit_label, *hbox2, *vbox2, *sep;


  GSList *group = NULL;

  DEBUG("configure");
  if (configure_win)
    return;

  read_config ();

  configure_win = gtk_window_new (GTK_WINDOW_DIALOG);

  gtk_signal_connect (GTK_OBJECT (configure_win), "destroy",
		      GTK_SIGNAL_FUNC (gtk_widget_destroyed), &configure_win);

  gtk_window_set_title (GTK_WINDOW (configure_win),
			"On Screen Display Configuration - " XOSD_VERSION);

  vbox = gtk_vbox_new (FALSE, 10);
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
  unit_label = gtk_label_new ("pixels");
  gtk_box_pack_start (GTK_BOX (hbox), unit_label, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  label = gtk_label_new ("Shadow Offset:");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  shadow_obj = gtk_adjustment_new (timeout, 0, 60, 1, 1, 1);
  shadow_spin = gtk_spin_button_new (GTK_ADJUSTMENT (shadow_obj), 1.0, 0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (shadow_spin),
			     (gfloat) shadow_offset);
  gtk_box_pack_start (GTK_BOX (hbox), shadow_spin, FALSE, FALSE, 0);
  unit_label = gtk_label_new ("pixels");
  gtk_box_pack_start (GTK_BOX (hbox), unit_label, FALSE, FALSE, 0);


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


  /*
  hbox = gtk_hbox_new (FALSE, 5);

  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  label = gtk_label_new ("Volume :");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  vol_on = gtk_radio_button_new_with_label (NULL, "Yes");
  group = gtk_radio_button_group (GTK_RADIO_BUTTON (vol_on));
  vol_off = gtk_radio_button_new_with_label (group, "No");
  gtk_box_pack_start (GTK_BOX (hbox), vol_on, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vol_off, FALSE, FALSE, 0);

  if (show_volume == 1)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vol_on), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vol_off), TRUE);
  */

  sep=gtk_hseparator_new();
  gtk_box_pack_start (GTK_BOX (vbox), sep, FALSE, FALSE, 0);

  hbox2 = gtk_hbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 0);
  label=gtk_label_new("Show:");
  gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 0);

  hbox2 = gtk_hbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 0);

  vbox2 = gtk_vbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (hbox2), vbox2, FALSE, FALSE, 0);

  show_item(vbox2, "Volume", show_volume, &vol_on);
  show_item(vbox2, "Balance", show_balance, &bal_on);
  show_item(vbox2, "Pause", show_pause, &pause_on);
  show_item(vbox2, "Track Name", show_trackname, &trackname_on);
  vbox2 = gtk_vbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (hbox2), vbox2, FALSE, FALSE, 0);
  show_item(vbox2, "Stop", show_stop, &stop_on);
  show_item(vbox2, "Repeat", show_repeat, &repeat_on);
  show_item(vbox2, "Shuffle", show_shuffle, &shuffle_on);

  sep=gtk_hseparator_new();
  gtk_box_pack_start (GTK_BOX (vbox), sep, FALSE, FALSE, 0);

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

  apply = gtk_button_new_with_label ("Apply");
  gtk_signal_connect (GTK_OBJECT (apply), "clicked",
		      GTK_SIGNAL_FUNC (configure_apply_cb), NULL);
  GTK_WIDGET_SET_FLAGS (apply, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (bbox), apply, TRUE, TRUE, 0);

  cancel = gtk_button_new_with_label ("Cancel");
  gtk_signal_connect_object (GTK_OBJECT (cancel), "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_destroy),
			     GTK_OBJECT (configure_win));
  GTK_WIDGET_SET_FLAGS (cancel, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (bbox), cancel, TRUE, TRUE, 0);


  gtk_widget_show_all (configure_win);
}

/**
 * DTM: save_previous_title() assumes ownership of the
 * memory pointed to by 'title', and will g_free()
 * it when deleting it - as such 'title' must be
 * gotten through glib allocations.
 */
static void save_previous_title ( gchar * title ) {
  DEBUG("save_previous_title");
  if ( previous_title )
    g_free( previous_title );
  previous_title = title;
}

/*
 * Convert hexcode to ASCII.
 */
static void replace_hexcodes (gchar *text)
{

  gchar hex_number[] = "FF";
  gchar *tmp, *tmp2;
  DEBUG("replace_hexcodes");

  while ((tmp = strchr(text, '%')) != NULL)
    {
      // Make sure we're not at the end of the string
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

/*
 * Callback funtion to handle delayed display.
 */
static gint timeout_func(gpointer data)
{
  gint pos, volume, balance;
  gboolean playing, paused, repeat, shuffle;
  gchar *text;

  if (!osd)
    return FALSE;

  GDK_THREADS_ENTER();

  pos = xmms_remote_get_playlist_pos (gp.xmms_session);
  playing = xmms_remote_is_playing (gp.xmms_session);
  paused = xmms_remote_is_paused (gp.xmms_session);
  volume = xmms_remote_get_main_volume (gp.xmms_session);
  shuffle = xmms_remote_is_shuffle (gp.xmms_session);
  repeat = xmms_remote_is_repeat (gp.xmms_session);
  balance = (xmms_remote_get_balance(gp.xmms_session) + 100) / 2;

  /**
   * Check if the position of the current song has changed.
   * DTM: bugfix
   *        1) 'get_playlist_time' seems "variable" for a song, don't use it
   *        2) we must free the titles we download
   */
  if (pos != previous_song)
    {
      if (xmms_remote_get_playlist_length (gp.xmms_session)) /* otherwise it'll crash */
	{

	  text = xmms_remote_get_playlist_title (gp.xmms_session, pos);
	  if (text)
	    replace_hexcodes (text);

	  /**
	   * Check to see if the title of the song has changed.
	   */
	  if ( !previous_title ||
	       g_strcasecmp(text, previous_title) != 0 ) {
	    if (show_stop) {
	      xosd_display (osd, 0, XOSD_string, playing ? "Play" : "Stopped");
	      xosd_display (osd, 1, XOSD_string, text);
	    }
	    save_previous_title( text );
	  }

	} else {
	  /** No song titles available. */
	  if (show_stop) {
	    xosd_display (osd, 0, XOSD_string, playing ? "Play" : "Stopped");
	  }
	  save_previous_title( 0 );
	}

      previous_song = pos;
    }

  else if (playing != previous_playing )
    {
      if (playing && show_trackname)
	{
	  xosd_display (osd, 0, XOSD_string, "Play");
	  text = xmms_remote_get_playlist_title (gp.xmms_session, pos);
	  if (text) {
	    replace_hexcodes (text);
	    xosd_display (osd, 1, XOSD_string, text);
	    save_previous_title ( text );
	  }
	}
      else if (!playing && show_stop )
	{
	  xosd_display (osd, 0, XOSD_string, "Stop");
	  xosd_display (osd, 1, XOSD_string, "");
	}

      previous_playing = playing;
    }

  else if (paused != previous_paused && show_pause)
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
	  if (text) {
	    replace_hexcodes (text);
	    xosd_display (osd, 1, XOSD_string, text);
	    save_previous_title( text );
	  }
	}
      previous_paused = paused;
    }

  else if (volume != previous_volume && show_volume)
    {
      xosd_display (osd, 0, XOSD_string, "Volume");
      xosd_display (osd, 1, XOSD_percentage, volume);
      previous_volume = volume;
    }

  else if (balance != previous_balance && show_balance)
    {
      xosd_display (osd, 0, XOSD_string, "Balance");
      xosd_display (osd, 1, XOSD_slider, balance);

      previous_balance = balance;
    }

  else if (repeat != previous_repeat && show_repeat )
    {
      xosd_display (osd, 0, XOSD_string, "Repeat");
      xosd_display (osd, 1, XOSD_string, repeat ? "On" : "Off");

      previous_repeat = repeat;
    }

  else if (shuffle != previous_shuffle && show_shuffle )
    {
      xosd_display (osd, 0, XOSD_string, "Shuffle");
      xosd_display (osd, 1, XOSD_string, shuffle ? "On" : "Off");

      previous_shuffle = shuffle;
    }

  GDK_THREADS_LEAVE();

  return TRUE;
}

/*
 * Add item to configuration dialog.
 */
show_item(GtkWidget* vbox, const char* description, int selected, GtkToggleButton** on)
{
  //GtkWidget  *hbox, *label;
  //GSList *group = NULL;

  //hbox = gtk_hbox_new (FALSE, 5);

  //gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  *on = (GtkToggleButton*) gtk_check_button_new_with_label(description);
  gtk_box_pack_start (GTK_BOX (vbox), (GtkWidget*)*on, FALSE, FALSE, 0);

  /*label = gtk_label_new (description);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  *on = gtk_radio_button_new_with_label (NULL, "Yes");
  group = gtk_radio_button_group (GTK_RADIO_BUTTON (*on));
  *off = gtk_radio_button_new_with_label (group, "No");

  gtk_box_pack_start (GTK_BOX (hbox), *on, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), *off, FALSE, FALSE, 0);

  */

  gtk_toggle_button_set_active (*on, selected);


}
