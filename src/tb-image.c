/* Eye of Gnome image viewer - toolbar for image windows
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <gnome.h>
#include "commands.h"
#include "stock.h"
#include "tb-image.h"



/* Toolbar definition */

static GnomeUIInfo toolbar[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("Open"), N_("Open an image file"),
				cmd_cb_image_open, GNOME_STOCK_PIXMAP_OPEN),
	GNOMEUIINFO_ITEM_STOCK (N_("Close"), N_("Close the current window"),
				cmd_cb_window_close, GNOME_STOCK_PIXMAP_CLOSE),
	GNOMEUIINFO_SEPARATOR,

/* Index of the first zoom item and number of zoom items */
#define ZOOM_INDEX 3
#define ZOOM_ITEMS 4

	GNOMEUIINFO_ITEM_STOCK (N_("In"), NULL, cmd_cb_zoom_in, STOCK_ZOOM_IN),
	GNOMEUIINFO_ITEM_STOCK (N_("Out"), NULL, cmd_cb_zoom_out, STOCK_ZOOM_OUT),
	GNOMEUIINFO_ITEM_STOCK (N_("1:1"), NULL, cmd_cb_zoom_1, STOCK_ZOOM_1),
	GNOMEUIINFO_ITEM_STOCK (N_("Fit"), NULL, cmd_cb_zoom_fit, STOCK_ZOOM_FIT),
	GNOMEUIINFO_END
};



/**
 * tb_image_new:
 * @window: An image window.
 * @zoom_items: A NULL-terminated array of widgets for the zoom buttons is
 * returned here.
 *
 * Creates a toolbar suitable for image windows.
 *
 * Return value: A newly-created toolbar.
 **/
GtkWidget *
tb_image_new (Window *window, GtkWidget ***zoom_items)
{
	GtkWidget *tb;
	GtkWidget **items;
	int i;

	g_return_val_if_fail (window != NULL, NULL);
	g_return_val_if_fail (IS_WINDOW (window), NULL);
	g_return_val_if_fail (zoom_items != NULL, NULL);

	tb = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
	gnome_app_fill_toolbar_with_data (GTK_TOOLBAR (tb),
					  toolbar,
					  GNOME_APP (window)->accel_group,
					  window);

	items = g_new (GtkWidget *, ZOOM_ITEMS + 1);
	*zoom_items = items;

	for (i = 0; i < ZOOM_ITEMS; i++)
		items[i] = toolbar[ZOOM_INDEX + i].widget;

	items[i] = NULL;

	return tb;
}
