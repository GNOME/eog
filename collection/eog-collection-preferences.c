/* eog-collection-preferences - helper functions for preference handling
 *
 * Copyright (C) 2001 The Free Software Foundation
 *
 * Author: Jens Finke <jens@gnome.org>
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
#include "eog-collection-view.h"
#include "eog-collection-preferences.h"

#include <gnome.h>

enum {
	PREF_LAYOUT,
	PREF_COLOR,
	PREF_LAST
};

static const gchar *pref_key[] = {
	"/apps/eog/collection/layout",
	"/apps/eog/collection/color"
};

static const char* prefs_layout [] = { 
	N_("Vertical"),
	N_("Horizontal"),
	N_("Rectangle")
};

static void
ecp_activate_layout_cb (GtkWidget *widget, GConfClient *client)
{
	EogLayoutMode mode;
	
	g_assert (GCONF_IS_CLIENT (client));
	
	mode = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
						   "number"));
	if(!gconf_client_set_int (client, pref_key[PREF_LAYOUT],
				  mode, NULL))
	{
		g_warning ("Couldn't set layout value in gconf.\n");
	}
}

static void 
ecp_activate_color_cb (GtkWidget *cp, guint r, guint g, guint b, guint a, gpointer data)
{
	GConfClient *client;
	GSList *l = NULL;

	g_return_if_fail (data != NULL);
	g_return_if_fail (GCONF_IS_CLIENT (data));

	client = GCONF_CLIENT (data);	
	
	l = g_slist_append (l, GUINT_TO_POINTER (r));
	l = g_slist_append (l, GUINT_TO_POINTER (g));
	l = g_slist_append (l, GUINT_TO_POINTER (b));
	
	gconf_client_set_list (client, pref_key[PREF_COLOR],
			       GCONF_VALUE_INT, l, NULL);
}

static GtkWidget*
ecp_create_view_page (GConfClient *client)
{
	GtkWidget *table;
	GtkWidget *omenu, *menu;
	GtkWidget *cp;
 	GSList *group = NULL;
	GSList *l = NULL;
	gint i;

	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 5);
	gtk_table_set_col_spacings (GTK_TABLE (table), 5);
	gtk_container_set_border_width (GTK_CONTAINER (table), 5);

	/* create layout mode option menu */
	omenu = gtk_option_menu_new ();
	menu = gtk_menu_new ();

	for (i = 0; i < 3; i++) {
		GtkWidget *item;		
		item = gtk_radio_menu_item_new_with_label (group, prefs_layout[i]);
		group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);

		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (ecp_activate_layout_cb), client);
		g_object_set_data (G_OBJECT (item), "number", GINT_TO_POINTER (i));
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), 
				     gconf_client_get_int (client, 
							   pref_key[PREF_LAYOUT],
							   NULL));
	gtk_table_attach (GTK_TABLE (table),
			  gtk_label_new (_("Layout Mode:")),
			  0, 1, 0, 1,
			  GTK_FILL, 0,
			  0, 0);
	gtk_table_attach (GTK_TABLE (table), omenu,
			  1, 2, 0, 1,
			  GTK_FILL, 0,
			  0, 0);

	/* create color picker */
	cp = gnome_color_picker_new ();
	gnome_color_picker_set_title (GNOME_COLOR_PICKER (cp), 
				      _("Image Collection Background Color"));
	gnome_color_picker_set_use_alpha (GNOME_COLOR_PICKER (cp), FALSE);
	
	l = gconf_client_get_list (client, pref_key[PREF_COLOR],
				   GCONF_VALUE_INT, NULL);
	if (l && (g_slist_length (l) == 3)) {
		gushort r, g, b;
		r = GPOINTER_TO_UINT (l->data);
		g = GPOINTER_TO_UINT (l->next->data);
		b = GPOINTER_TO_UINT (l->next->next->data);
		
		gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (cp),
					   r, g, b, 65535);
		g_slist_free (l);
	} else {
		gnome_color_picker_set_i8 (GNOME_COLOR_PICKER (cp),
					   222, 222, 222, 255);
	}

	g_signal_connect (G_OBJECT (cp), "color_set",
			  G_CALLBACK (ecp_activate_color_cb), client);

	gtk_table_attach (GTK_TABLE (table),
			  gtk_label_new (_("Background Color:")),
			  0, 1, 1, 2,
			  GTK_FILL, 0,
			  0, 0);
	gtk_table_attach (GTK_TABLE (table), 
			  cp, 
			  1, 2, 1, 2,
			  GTK_FILL, 0, 
			  0, 0);
	
	/* show all widgets */
	gtk_widget_show_all (table);
	
	return table;
}

GtkWidget*
eog_collection_preferences_create_page (GConfClient *client, int page_number)
{
	GtkWidget *widget = NULL;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (GCONF_IS_CLIENT (client), NULL);
	
	switch (page_number) {
	case 0: /* view page */
		widget = ecp_create_view_page (client);
		break;
	default:
		g_assert_not_reached ();
	}

	return widget;
}
	

