/* Eye Of Gnome - Preferences Dialog 
 *
 * Copyright (C) 2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on code by:
 *	- Jens Finke <jens@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "eog-preferences.h"
#include "eog-util.h"
#include "eog-config-keys.h"

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-help.h>

#define GCONF_OBJECT_KEY	"GCONF_KEY"
#define GCONF_OBJECT_VALUE	"GCONF_VALUE"

static void
check_toggle_cb (GtkWidget *widget, gpointer data)
{
	char *key = NULL;

	key = g_object_get_data (G_OBJECT (widget), GCONF_OBJECT_KEY);
	if (key == NULL) return;

	gconf_client_set_bool (GCONF_CLIENT (data),
			       key,
			       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)),
			       NULL);
}

static void
spin_button_changed_cb (GtkWidget *widget, gpointer data)
{
	char *key = NULL;

	key = g_object_get_data (G_OBJECT (widget), GCONF_OBJECT_KEY);
	if (key == NULL) return;

	gconf_client_set_int (GCONF_CLIENT (data),
			      key,
			      gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget)),
			      NULL);
}

static void
color_change_cb (GtkColorButton *button, gpointer data)
{
	GdkColor color;
	char *key = NULL;
	char *value = NULL;

	gtk_color_button_get_color (button, &color);

	value = g_strdup_printf ("#%02X%02X%02X",
				 color.red / 256,
				 color.green / 256,
				 color.blue / 256);

	key = g_object_get_data (G_OBJECT (button), GCONF_OBJECT_KEY);
	if (key == NULL || value == NULL) 
		return;

	gconf_client_set_string (GCONF_CLIENT (data),
				 key,
				 value,
				 NULL);
}

static void
radio_toggle_cb (GtkWidget *widget, gpointer data)
{
	char *key = NULL;
	char *value = NULL;
	

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) 
	    return;

	key = g_object_get_data (G_OBJECT (widget), GCONF_OBJECT_KEY);
	value = g_object_get_data (G_OBJECT (widget), GCONF_OBJECT_VALUE);
	if (key == NULL || value == NULL) 
		return;

	gconf_client_set_string (GCONF_CLIENT (data),
				 key,
				 value,
				 NULL);
}

static void
eog_preferences_response_cb (GtkDialog *dlg, gint res_id, gpointer data)
{
	switch (res_id) {
	case GTK_RESPONSE_HELP:
		eog_util_show_help ("eog-prefs", NULL);
		break;
	default:
		gtk_widget_destroy (GTK_WIDGET (dlg));
	}
}

void
eog_preferences_show (GtkWindow *parent, GConfClient *client)
{
	GtkWidget *dlg;
	GladeXML  *xml;
	GtkWidget *widget;
	char *value, *filename;
	GdkColor color;

	filename = g_build_filename (DATADIR, "eog.glade", NULL);
	xml = glade_xml_new (filename, "eog_preferences_dialog", "eog");
	g_free (filename);

	g_assert (xml != NULL);

	dlg = glade_xml_get_widget (xml, "eog_preferences_dialog");

	g_signal_connect (G_OBJECT (dlg), "response",
			  G_CALLBACK (eog_preferences_response_cb), dlg);

	/* Interpolate Flag */
	widget = glade_xml_get_widget (xml, "interpolate_check");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), 
				      gconf_client_get_bool (client, EOG_CONF_VIEW_INTERPOLATE, NULL));
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_VIEW_INTERPOLATE);
	g_signal_connect (G_OBJECT (widget), 
			  "toggled", 
			  G_CALLBACK (check_toggle_cb), 
			  client);

	/* Transparency Radio Group */
	widget = glade_xml_get_widget (xml, "color_radio");
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_VIEW_TRANSPARENCY);
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_VALUE, "COLOR");
	g_signal_connect (G_OBJECT (widget), 
			  "toggled", 
			  G_CALLBACK (radio_toggle_cb), 
			  client);

	widget = glade_xml_get_widget (xml, "checkpattern_radio");
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_VIEW_TRANSPARENCY);
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_VALUE, "CHECK_PATTERN");
	g_signal_connect (G_OBJECT (widget), 
			  "toggled", 
			  G_CALLBACK (radio_toggle_cb), 
			  client);

	widget = glade_xml_get_widget (xml, "background_radio");
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_VIEW_TRANSPARENCY);
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_VALUE, "NONE");
	g_signal_connect (G_OBJECT (widget), 
			  "toggled", 
			  G_CALLBACK (radio_toggle_cb), 
			  client);
	
	value = gconf_client_get_string (client, EOG_CONF_VIEW_TRANSPARENCY, NULL);
	if (g_ascii_strcasecmp (value, "COLOR") == 0) {
		widget = glade_xml_get_widget (xml, "color_radio");
	}
	else if (g_ascii_strcasecmp (value, "CHECK_PATTERN") == 0) {
		widget = glade_xml_get_widget (xml, "checkpattern_radio");
	}
	else {
		widget = glade_xml_get_widget (xml, "background_radio");
	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

	/* Color Picker */
	g_free (value);
	value = gconf_client_get_string (client, EOG_CONF_VIEW_TRANS_COLOR, NULL);
	widget = glade_xml_get_widget (xml, "colorbutton");
	if (gdk_color_parse (value, &color)) {
		gtk_color_button_set_color (GTK_COLOR_BUTTON (widget), &color);
	}
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_VIEW_TRANS_COLOR);
	g_signal_connect (G_OBJECT (widget),
			  "color-set",
			  G_CALLBACK (color_change_cb),
			  client);
	g_free (value);

	/* Slideshow Page */
	widget = glade_xml_get_widget (xml, "upscale_check");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), 
				      gconf_client_get_bool (client, EOG_CONF_FULLSCREEN_UPSCALE, NULL));
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_FULLSCREEN_UPSCALE);
	g_signal_connect (G_OBJECT (widget), 
			  "toggled", 
			  G_CALLBACK (check_toggle_cb), 
			  client);

	widget = glade_xml_get_widget (xml, "loop_check");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), 
				      gconf_client_get_bool (client, EOG_CONF_FULLSCREEN_LOOP, NULL));
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_FULLSCREEN_LOOP);
	g_signal_connect (G_OBJECT (widget), 
			  "toggled", 
			  G_CALLBACK (check_toggle_cb), 
			  client);

	widget = glade_xml_get_widget (xml, "seconds_spin");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 
				   gconf_client_get_int (client, EOG_CONF_FULLSCREEN_SECONDS, NULL));
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_FULLSCREEN_SECONDS);
	g_signal_connect (G_OBJECT (widget), 
			  "changed", 
			  G_CALLBACK (spin_button_changed_cb), 
			  client);
			  
	gtk_window_set_transient_for (GTK_WINDOW (dlg), parent);
	gtk_widget_show_all (dlg);
}
