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

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <xmms/plugin.h>
#include <xmms/xmmsctrl.h>
#include <xmms/configfile.h>

#include <xosd.h>

#define DEBUG(a) /*fprintf (stderr, "%s: %s: %d: %s\n", __FILE__, __PRETTY_FUNCTION__, __LINE__, a)*/

static void init(void);
static void cleanup(void);
static gint timeout_func(gpointer);
static void read_config (void);
static void configure (void);
static void show_item(GtkWidget* vbox, const char* description, int selected, GtkToggleButton** on);
static void save_previous_title ( gchar * title );
GtkWidget **position_icons_new(void);

GeneralPlugin gp =
  {
    .handle = NULL,
    .filename = NULL,
    .xmms_session = -1,
    .description = "On Screen Display " XOSD_VERSION,
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
static gint h_offset;
static gint shadow_offset;
static gint pos;
static gint align;

static gboolean show_volume;
static gboolean show_balance;
static gboolean show_pause;
static gboolean show_trackname;
static gboolean show_stop;
static gboolean show_repeat;
static gboolean show_shuffle;

static GtkObject *timeout_obj, *offset_obj, *h_offset_obj, *shadow_obj;
static GtkWidget *configure_win, *font_entry, *colour_entry,
  *timeout_spin,*offset_spin, *h_offset_spin,  *shadow_spin;
static GtkWidget *positions[3][3];

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
  /* font = osd_default_font; */
  /* font = "-misc-fixed-*-*-*-*-40-*-*-*-*-*-*-*"; */
  /* colour = "green"; */

  DEBUG("init");

  if (osd) {
    DEBUG("uniniting osd");
    xosd_destroy(osd);
    osd=NULL;
  }

  read_config ();

  previous_repeat = previous_shuffle = previous_paused = previous_playing =
    FALSE;
  previous_volume = previous_song =
    0;
  previous_title = 0;

  DEBUG("calling osd init function");

  osd = xosd_create (2);
  xosd_set_font(osd, font);
  xosd_set_colour(osd, colour);
  xosd_set_timeout(osd, timeout);
  xosd_set_pos(osd, pos);
  xosd_set_align(osd, align);
  xosd_set_vertical_offset(osd, offset);
  xosd_set_horizontal_offset(osd, h_offset);
  xosd_set_shadow_offset(osd, shadow_offset);
 DEBUG("osd initialized");
  if (osd)
    timeout_tag = gtk_timeout_add (100, timeout_func, NULL);
}

/*
 * Free memory and release resources.
 */
static void cleanup(void)
{
  DEBUG("cleanup");
  if (osd && timeout_tag)
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

  if (osd) {
    DEBUG("hide");
    xosd_hide (osd);
    DEBUG("uninit");
    xosd_destroy (osd);
    DEBUG("done with osd");
    osd=NULL;
  }
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

  DEBUG("read_config");

  g_free (colour);
  g_free (font);
  colour = NULL;
  font = NULL;
  timeout = 3;
  offset = 50;
  h_offset = 0;
  shadow_offset = 1;
  pos = XOSD_bottom;
  align = XOSD_left;

  DEBUG("read config");

  if ((cfgfile = xmms_cfg_open_default_file ()) != NULL)
    {
      DEBUG("reading configuration data");
      xmms_cfg_read_string (cfgfile, "osd", "font", &font);
      xmms_cfg_read_string (cfgfile, "osd", "colour", &colour);
      xmms_cfg_read_int (cfgfile, "osd", "timeout", &timeout);
      xmms_cfg_read_int (cfgfile, "osd", "offset", &offset);
      xmms_cfg_read_int (cfgfile, "osd", "h_offset", &h_offset);
      xmms_cfg_read_int (cfgfile, "osd", "shadow_offset", &shadow_offset);
      xmms_cfg_read_int (cfgfile, "osd", "pos", &pos);
      xmms_cfg_read_int (cfgfile, "osd", "align", &align);
      xmms_cfg_read_int (cfgfile, "osd", "show_volume", &show_volume );
      xmms_cfg_read_int (cfgfile, "osd", "show_balance", &show_balance );
      xmms_cfg_read_int (cfgfile, "osd", "show_pause", &show_pause );
      xmms_cfg_read_int (cfgfile, "osd", "show_trackname", &show_trackname );
      xmms_cfg_read_int (cfgfile, "osd", "show_stop", &show_stop );
      xmms_cfg_read_int (cfgfile, "osd", "show_repeat", &show_repeat );
      xmms_cfg_read_int (cfgfile, "osd", "show_shuffle", &show_shuffle );
      xmms_cfg_free(cfgfile);
    }

  DEBUG("getting default font");
  if (font == NULL)
    font = g_strdup (osd_default_font);

  DEBUG("default colour");
  if (colour == NULL)
    colour = g_strdup ("green");

  DEBUG("done");
}

/*
 * Return state of check button.
 */
static gboolean isactive(GtkToggleButton *item) {
  DEBUG("is active");
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
  h_offset = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (h_offset_spin));
  shadow_offset = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (shadow_spin));

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (positions[XOSD_top][XOSD_left])))
    {
      pos = XOSD_top;
      align = XOSD_left;
    }
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (positions[XOSD_top][XOSD_center])))
    {
      pos = XOSD_top;
      align = XOSD_center;
    }
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (positions[XOSD_top][XOSD_right])))
    {
      pos = XOSD_top;
      align = XOSD_right;
    }
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (positions[XOSD_middle][XOSD_left])))
    {
      pos = XOSD_middle;
      align = XOSD_left;
    }
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (positions[XOSD_middle][XOSD_center])))
    {
      pos = XOSD_middle;
      align = XOSD_center;
    }
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (positions[XOSD_middle][XOSD_right])))
    {
      pos = XOSD_middle;
      align = XOSD_right;
    }
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (positions[XOSD_bottom][XOSD_left])))
    {
      pos = XOSD_bottom;
      align = XOSD_left;
    }
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (positions[XOSD_bottom][XOSD_center])))
    {
      pos = XOSD_bottom;
      align = XOSD_center;
    }
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (positions[XOSD_bottom][XOSD_right])))
    {
      pos = XOSD_bottom;
      align = XOSD_right;
    }

  if (osd)
    {
      xosd_set_colour (osd, colour);
      if (xosd_set_font (osd, font) == -1) {
	DEBUG("invalid font");
	DEBUG(font);
      }
      xosd_set_timeout (osd, timeout);
      xosd_set_vertical_offset (osd, offset);
      xosd_set_horizontal_offset (osd, h_offset);
      xosd_set_shadow_offset (osd, shadow_offset);
      xosd_set_pos (osd, pos);
      xosd_set_align(osd, align);
    }

  cfgfile = xmms_cfg_open_default_file();
  xmms_cfg_write_string(cfgfile, "osd", "colour", colour);
  xmms_cfg_write_string(cfgfile, "osd", "font", font);
  xmms_cfg_write_int(cfgfile, "osd", "timeout", timeout);
  xmms_cfg_write_int(cfgfile, "osd", "offset", offset);
  xmms_cfg_write_int(cfgfile, "osd", "h_offset", h_offset);
  xmms_cfg_write_int(cfgfile, "osd", "shadow_offset", shadow_offset);
  xmms_cfg_write_int(cfgfile, "osd", "pos", pos);
  xmms_cfg_write_int(cfgfile, "osd", "align", align);

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
  GtkWidget *table, **position_icons, *position_table;
  
  xosd_pos curr_pos;
  xosd_align curr_align;

  GSList *group = NULL;

  DEBUG("configure");
  if (configure_win)
    return;

  read_config ();

  configure_win = gtk_window_new (GTK_WINDOW_DIALOG);

  gtk_signal_connect (GTK_OBJECT (configure_win), "destroy",
		      GTK_SIGNAL_FUNC (gtk_widget_destroyed), &configure_win);

  gtk_window_set_title (GTK_WINDOW (configure_win),
			"OSD " XOSD_VERSION " Configuration");

  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_add (GTK_CONTAINER (configure_win), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (configure_win), 12);

  /* --=mjs=-- The Main table to pack everything into */
  table = gtk_table_new (7, 3, FALSE);
  gtk_table_set_row_spacings(GTK_TABLE(table), 12);
  gtk_table_set_col_spacings(GTK_TABLE(table), 12);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

  /* Font selector. */
  label = gtk_label_new ("Font:");
  gtk_misc_set_alignment(GTK_MISC (label), 0.0, 0.0);
  gtk_label_set_justify(GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, 
		    GTK_FILL, GTK_FILL, 0, 0);
  font_entry = gtk_entry_new ();
  if (font)
    gtk_entry_set_text (GTK_ENTRY (font_entry), font);
  gtk_table_attach (GTK_TABLE (table), font_entry, 1, 2, 0, 1, 
		    GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  button = gtk_button_new_with_label ("Set...");
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (font_dialog_window), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 2, 3, 0, 1, 
		    GTK_FILL,  GTK_FILL, 0, 0);

  /* Colour Selector */
  label = gtk_label_new ("Colour:");
  gtk_misc_set_alignment(GTK_MISC (label), 0.0, 0.0);
  gtk_label_set_justify(GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, 
		    GTK_FILL, GTK_FILL, 0, 0);
  colour_entry = gtk_entry_new ();
  if (colour)
    gtk_entry_set_text (GTK_ENTRY (colour_entry), colour);
  gtk_table_attach (GTK_TABLE (table), colour_entry, 1, 2, 1, 2, 
		    GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  button = gtk_button_new_with_label ("Set...");
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (colour_dialog_window), NULL);
  gtk_table_attach (GTK_TABLE (table), button, 2, 3, 1, 2, 
		    GTK_FILL, GTK_FILL, 0, 0);

  /* Timeout */
  label = gtk_label_new ("Timeout:");
  gtk_misc_set_alignment(GTK_MISC (label), 0.0, 0.0);
  gtk_label_set_justify(GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
		    GTK_FILL, GTK_FILL, 0, 0);
  hbox = gtk_hbox_new (FALSE, 6);
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 2, 3,
		    GTK_FILL, GTK_FILL, 0, 0);
  timeout_obj = gtk_adjustment_new (timeout, -1, 60, 1, 1, 1);
  timeout_spin = gtk_spin_button_new (GTK_ADJUSTMENT (timeout_obj), 1.0, 0);
  if (timeout)
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (timeout_spin),
			       (gfloat) timeout);
  gtk_box_pack_start (GTK_BOX (hbox), timeout_spin, FALSE, FALSE, 0);
  unit_label = gtk_label_new ("seconds");
  gtk_misc_set_alignment(GTK_MISC (unit_label), 0.0, 0.0);
  gtk_label_set_justify(GTK_LABEL (unit_label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox), unit_label, FALSE, FALSE, 0);

  /* Shadow Offset */
  label = gtk_label_new ("Shadow Offset:");
  gtk_misc_set_alignment(GTK_MISC (label), 0.0, 0.0);
  gtk_label_set_justify(GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
		    GTK_FILL, GTK_FILL, 0, 0);
  hbox = gtk_hbox_new (FALSE, 6);
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 3, 4,
		    GTK_FILL, GTK_FILL, 0, 0);
  shadow_obj = gtk_adjustment_new (timeout, 0, 60, 1, 1, 1);
  shadow_spin = gtk_spin_button_new (GTK_ADJUSTMENT (shadow_obj), 1.0, 0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (shadow_spin),
			     (gfloat) shadow_offset);
  gtk_box_pack_start (GTK_BOX (hbox), shadow_spin, FALSE, FALSE, 0);
  unit_label = gtk_label_new ("pixels");
  gtk_misc_set_alignment(GTK_MISC (unit_label), 0.0, 0.0);
  gtk_label_set_justify(GTK_LABEL (unit_label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox), unit_label, FALSE, FALSE, 0);

  /* Position */
  label = gtk_label_new ("Position:");
  gtk_misc_set_alignment(GTK_MISC (label), 0.0, 0.0);
  gtk_label_set_justify(GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 4, 5,
		    GTK_FILL, GTK_FILL, 0, 0);

  position_icons = position_icons_new();
  position_table = gtk_table_new(3, 3, FALSE);
  gtk_table_set_row_spacings(GTK_TABLE(position_table), 6);
  gtk_table_set_col_spacings(GTK_TABLE(position_table), 6);
  gtk_table_attach (GTK_TABLE (table), position_table, 1, 2, 4, 5,
		    GTK_FILL, GTK_FILL, 0, 0);

  curr_pos = XOSD_top;
  for (curr_align = XOSD_left ; curr_align <= XOSD_right; curr_align++)
    {
      positions[curr_pos][curr_align] = gtk_radio_button_new(group);
      gtk_container_add( GTK_CONTAINER (positions[curr_pos][curr_align]),
			 position_icons[(curr_pos*3) + curr_align]);
      assert(positions[curr_pos][curr_align] != NULL);

      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (positions[curr_pos][curr_align]), FALSE);
      group = gtk_radio_button_group (GTK_RADIO_BUTTON (positions[curr_pos][curr_align]));

      if (pos == curr_pos && align == curr_align)
	{
	  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON (positions[curr_pos][curr_align]), TRUE);
	}

      gtk_table_attach ( GTK_TABLE (position_table),
			 positions[curr_pos][curr_align],
			 curr_align, curr_align + 1,
			 curr_pos, curr_pos + 1,
			 GTK_FILL, GTK_FILL, 0, 0);
    }

  curr_pos = XOSD_middle;
  for (curr_align = XOSD_left ; curr_align <= XOSD_right; curr_align++)
    {
      positions[curr_pos][curr_align] = gtk_radio_button_new (group);
      gtk_container_add( GTK_CONTAINER (positions[curr_pos][curr_align]),
			 position_icons[(curr_pos*3) + curr_align]);
      assert(positions[curr_pos][curr_align] != NULL);

      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (positions[curr_pos][curr_align]), FALSE);
      group = gtk_radio_button_group (GTK_RADIO_BUTTON (positions[curr_pos][curr_align]));

      if (pos == curr_pos && align == curr_align)
	{
	  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON (positions[curr_pos][curr_align]), TRUE);
	}


      gtk_table_attach ( GTK_TABLE (position_table),
			 positions[curr_pos][curr_align],
			 curr_align, curr_align + 1,
			 1, 2,
			 GTK_FILL, GTK_FILL, 0, 0);
    }

  curr_pos = XOSD_bottom;
  for (curr_align = XOSD_left ; curr_align <= XOSD_right; curr_align++)
    {
      positions[curr_pos][curr_align] = gtk_radio_button_new (group);
      gtk_container_add( GTK_CONTAINER (positions[curr_pos][curr_align]),
			 position_icons[(curr_pos*3) + curr_align]);

      assert(positions[curr_pos][curr_align] != NULL);

      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (positions[curr_pos][curr_align]), FALSE);
      group = gtk_radio_button_group (GTK_RADIO_BUTTON (positions[curr_pos][curr_align]));

      if (pos == curr_pos && align == curr_align)
	{
	  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON (positions[curr_pos][curr_align]), TRUE);
	}

      gtk_table_attach ( GTK_TABLE (position_table),
			 positions[curr_pos][curr_align],
			 curr_align, curr_align + 1,
			 2, 3,
			 GTK_FILL, GTK_FILL, 0, 0);
    }

  /* Vertical Offset */
  label = gtk_label_new ("Vertical Offset:");
  gtk_misc_set_alignment(GTK_MISC (label), 0.0, 0.0);
  gtk_label_set_justify(GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 6, 7,
		    GTK_FILL, GTK_FILL, 0, 0);
  hbox = gtk_hbox_new (FALSE, 6);
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 6, 7,
		    GTK_FILL, GTK_FILL, 0, 0);
  offset_obj = gtk_adjustment_new (timeout, 0, 60, 1, 1, 1);
  offset_spin = gtk_spin_button_new (GTK_ADJUSTMENT (offset_obj), 1.0, 0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (offset_spin), offset);
  gtk_box_pack_start (GTK_BOX (hbox), offset_spin, FALSE, FALSE, 0);
  unit_label = gtk_label_new ("pixels");
  gtk_misc_set_alignment(GTK_MISC (unit_label), 0.0, 0.0);
  gtk_label_set_justify(GTK_LABEL (unit_label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox), unit_label, FALSE, FALSE, 0);

  // Horizontal Offset
  label = gtk_label_new ("Horizontal Offset:");
  gtk_misc_set_alignment(GTK_MISC (label), 0.0, 0.0);
  gtk_label_set_justify(GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 7, 8,
		    GTK_FILL, GTK_FILL, 0, 0);
  hbox = gtk_hbox_new (FALSE, 6);
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 7, 8,
		    GTK_FILL, GTK_FILL, 0, 0);
  h_offset_obj = gtk_adjustment_new (timeout, 0, 60, 1, 1, 1);
  h_offset_spin = gtk_spin_button_new (GTK_ADJUSTMENT (h_offset_obj), 1.0, 0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (h_offset_spin), h_offset);
  gtk_box_pack_start (GTK_BOX (hbox), h_offset_spin, FALSE, FALSE, 0);
  unit_label = gtk_label_new ("pixels");
  gtk_misc_set_alignment(GTK_MISC (unit_label), 0.0, 0.0);
  gtk_label_set_justify(GTK_LABEL (unit_label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (hbox), unit_label, FALSE, FALSE, 0);

  sep=gtk_hseparator_new();
  gtk_box_pack_start (GTK_BOX (vbox), sep, FALSE, FALSE, 0);
  
  // What data should be shown
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

  // Command Buttons
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
  DEBUG("enter");
  if ( previous_title ) {
    DEBUG("freeing");
    g_free( previous_title );
  }
  previous_title = title;
  DEBUG("exit");
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
      DEBUG("replace_hexcodes loop");
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

/*
 * Callback funtion to handle delayed display.
 */
static gint timeout_func(gpointer data)
{
  gint pos, volume, balance;
  gboolean playing, paused, repeat, shuffle;
  gchar *text;

  DEBUG("timeout func");

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
void show_item(GtkWidget* vbox, const char* description, int selected, GtkToggleButton** on)
{
  /* GtkWidget  *hbox, *label;*/
  /*GSList *group = NULL; */

  /*hbox = gtk_hbox_new (FALSE, 5);*/

  /*gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);*/

  DEBUG("show_item");

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


/* "position_icons_new" -- Load the display position icons
 * 
 * DESCRIPTION
 *    There are nine icons used to set the position of the XOSD
 *    window: one for each compass point and one for the center.
 *    "position_icons_new" loads these icons and returns them as an
 *    array.  The directory that holds the PNG image files used for
 *    the icons is defined by the CPP definition PIXMAPDIR
 *
 * ARGUMENTS
 *    None.
 *
 * RETURNS
 *    An array of nine (9, 00001011, IX) GtkPixmap widgets.
 *
 * DEPENDS
 *    Libraries: gtk, gdk-pixbuf, stdlib, stdio
 *    CPP Definitions: PIXMAPDIR
 */
GtkWidget **position_icons_new(void)
{
  GtkWidget **icons = NULL;
  int i = 0;
  int j = 0;
  // Curse TNW13 and his "TOP, BOTTOM, MIDDLE" enumerations :)
  const char *icon_names[3][3] = {{"top-left.png", "top.png", "top-right.png"},
				  {"bottom-left.png", "bottom.png", 
				   "bottom-right.png"},
				  {"left.png", "centre.png", "right.png"}};
  // PIXMAPDIR should be defined elsewhere, such as the command line
  // (i.e. "-DPIXMAPDIR=...") but I assign it to a string because I'm
  // as wimp and I like type-checking.  I miss Modula-2\ldots
  const char *pixmap_path = PIXMAPDIR;

  int pixmap_path_len = strlen(pixmap_path);
  int icon_name_len = 0;
  char *icon_file_name = NULL;

  GdkPixbuf *icon_pixbuf = NULL;
  GdkPixmap *icon_pixmap = NULL;
  GdkBitmap *icon_mask = NULL;
  GtkWidget *icon_widget = NULL;

  // Create the array to hold the icons
  icons = calloc(9, sizeof(GtkWidget *));
  if (icons == NULL)
    {
      perror ("Could not create \"icons\"");
      exit (20432);
    }
  
  for (i = 0; i < 3; i++)
    {
      for (j = 0; j < 3; j++)
	{
	  // Calculate the length of the complete file name: length of
	  // the filename + length of path + slash + \0.  We need this
	  // value twice, so it is assigned to a variable.
	  icon_name_len = strlen(icon_names[i][j]) + pixmap_path_len + 2;
	  icon_file_name = calloc(icon_name_len, sizeof(char));
	  if (icon_file_name == NULL)
	    {
	      perror("Could not create \"icon_file_name\"");
	      exit(20433);
	    }
	  snprintf (icon_file_name, icon_name_len, "%s/%s", 
		    pixmap_path, icon_names[i][j]);

	  // Load the file, render it, and create the widget.
	  icon_pixbuf = gdk_pixbuf_new_from_file(icon_file_name);
	  gdk_pixbuf_render_pixmap_and_mask(icon_pixbuf, &icon_pixmap, 
					    &icon_mask, 128);
	  icon_widget = gtk_pixmap_new(icon_pixmap, icon_mask);
	  // Add the widget to the array of pixmaps
	  icons[(3*i) + j] = icon_widget;
	  
	  free(icon_file_name);
	}
    }
  return icons;
}
