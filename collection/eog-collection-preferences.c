/* eog-collection-preferences - helper functions for preference handling
 *
 * Copyright (C) 2001 The Free Software Foundation
 *
 * Author: Jens Finke <jens@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "eog-collection-view.h"
#include "eog-collection-preferences.h"

#include <gnome.h>

static void
ecp_activate_layout_cb (GtkWidget *widget, EogCollectionView *cview)
{
	EogLayoutMode mode;
	
	g_assert (EOG_IS_COLLECTION_VIEW (cview));
	
	mode = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (widget),
						     "number"));
	eog_collection_view_set_layout_mode (cview, mode);
}

static const char* prefs_layout [] = { 
	N_("Vertical"),
	N_("Horizontal"),
	N_("Rectangle")
};

static GtkWidget*
ecp_create_layout_page (EogCollectionView *cview)
{
	GtkWidget *table;
	GtkWidget *omenu, *menu;
 	GSList *group;
	gint i;

	table = gtk_table_new (1, 2, FALSE);
	omenu = gtk_option_menu_new ();
	menu = gtk_menu_new ();
	group = NULL;

	for (i = 0; i < 3; i++) {
		GtkWidget *item;		
		item = gtk_radio_menu_item_new_with_label (group, prefs_layout[i]);
		group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (item));
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);

		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    (GtkSignalFunc) ecp_activate_layout_cb, cview);
		gtk_object_set_data (GTK_OBJECT (item), "number", GINT_TO_POINTER (i));
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);

	gtk_table_attach_defaults (GTK_TABLE (table),
				   gtk_label_new (_("Layout Mode:")),
				   0, 1, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE (table), omenu,
				   1, 2, 0, 1);
	gtk_widget_show_all (table);
	
	return table;
}

static void 
ecp_activate_color_cb (GtkWidget *cp, guint r, guint g, guint b, guint a, gpointer data)
{
	EogCollectionView *cview;
	GdkColor color;
	
	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_VIEW (data));

	cview = EOG_COLLECTION_VIEW (data);

	color.red = r;
	color.green = g;
	color.blue = b;

	eog_collection_view_set_background_color (cview, &color);
}

static GtkWidget*
ecp_create_color_page (EogCollectionView *cview)
{
	GtkWidget *table;
	GtkWidget *cp;
	
	table = gtk_table_new (1, 2, FALSE);
	
	gtk_table_attach_defaults (GTK_TABLE (table),
				   gtk_label_new (_("Background Color:")),
				   0, 1, 0, 1);

	cp = gnome_color_picker_new ();
	gnome_color_picker_set_title (GNOME_COLOR_PICKER (cp), 
				      _("Image Collection Background Color"));
	gnome_color_picker_set_use_alpha (GNOME_COLOR_PICKER (cp), FALSE);
	gtk_signal_connect (GTK_OBJECT (cp), "color_set",
			    GTK_SIGNAL_FUNC (ecp_activate_color_cb), cview);

	gtk_table_attach_defaults (GTK_TABLE (table), cp, 1, 2, 0, 1);

	gtk_widget_show_all (table);

	return table;
}

GtkWidget*
eog_collection_preferences_create_page (EogCollectionView *cview, int page_number)
{
	GtkWidget *widget = NULL;

	g_return_val_if_fail (cview != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_VIEW (cview), NULL);
	
	switch (page_number) {
	case 0: /* layout page */
		widget = ecp_create_layout_page (cview);
		break;
	case 1: /* color */
		widget = ecp_create_color_page (cview);
		break;
	default:
		g_assert_not_reached ();
	}

	return widget;
}
	

