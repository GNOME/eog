/* Eye of Gnome image viewer - toolbar for image windows
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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
#include "tb-image.h"



/* Toolbar definition */

static GnomeUIInfo toolbar[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("Open"), N_("Open an image file"),
				cmd_cb_image_open, GNOME_STOCK_PIXMAP_OPEN),
	GNOMEUIINFO_ITEM_STOCK (N_("Close"), N_("Close the current window"),
				cmd_cb_window_close, GNOME_STOCK_PIXMAP_CLOSE),
	GNOMEUIINFO_END
};



/**
 * tb_image_new:
 * @window: An image window.
 *
 * Creates a toolbar suitable for image windows.
 *
 * Return value: A newly-created toolbar.
 **/
GtkWidget *
tb_image_new (Window *window)
{
	GtkWidget *tb;

	tb = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
	gnome_app_fill_toolbar_with_data (GTK_TOOLBAR (tb),
					  toolbar,
					  GNOME_APP (window)->accel_group,
					  window);

	return tb;
}
