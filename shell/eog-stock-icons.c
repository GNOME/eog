/* Eye Of Gnome - Stock Icons Handling 
 *
 * Copyright (C) 2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on evince code (shell/ev-stock-icons.c) by: 
 * 	- Martin Kretzschmar <martink@gnome.org>
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

#include <config.h>

#include "eog-stock-icons.h"

#include <gtk/gtkiconfactory.h>
#include <gtk/gtkstock.h>
#include <gdk/gdkpixbuf.h>

typedef struct {
	char *stock_id;
	char *icon;
} EogStockIcon;

/* Evince stock icons from gnome-icon-theme */
static const EogStockIcon stock_icons [] = {
	{ EOG_STOCK_ROTATE_90,       "stock-rotate-90-16.png" },
	{ EOG_STOCK_ROTATE_270,      "stock-rotate-270-16.png" },
	{ EOG_STOCK_ROTATE_180,      "stock-rotate-180-16.png" },
	{ EOG_STOCK_FLIP_HORIZONTAL, "stock-flip-horizontal-16.png" },
	{ EOG_STOCK_FLIP_VERTICAL,   "stock-flip-vertical-16.png" }
};

void
eog_stock_icons_init (void)
{
	GtkIconFactory *factory;
	GtkIconSource *source;
	gint i;

        factory = gtk_icon_factory_new ();
        gtk_icon_factory_add_default (factory);

	source = gtk_icon_source_new ();

	for (i = 0; i < G_N_ELEMENTS (stock_icons); i++) {
		GtkIconSet *set;
		gchar *icon_path;

		gtk_icon_source_set_icon_name (source, stock_icons[i].stock_id);

		icon_path = g_build_filename (DATADIR, "icons", stock_icons[i].icon, NULL);
		gtk_icon_source_set_filename (source, icon_path);

		set = gtk_icon_set_new ();
		gtk_icon_set_add_source (set, source);

		gtk_icon_factory_add (factory, stock_icons[i].stock_id, set);

		gtk_icon_set_unref (set);
		g_free (icon_path);
	}

	gtk_icon_source_free (source);

	g_object_unref (factory);
}
