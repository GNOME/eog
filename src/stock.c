/* Eye of Gnome image viewer - stock icons
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
#include "stock.h"

#include "stock/stock-zoom-1.xpm"
#include "stock/stock-zoom-fit.xpm"
#include "stock/stock-zoom-in.xpm"
#include "stock/stock-zoom-out.xpm"



/**
 * stock_init:
 * @void: 
 * 
 * Initializes the stock icons by registering them against gnome-stock.
 **/
void
stock_init (void)
{
	static GnomeStockPixmapEntry entries[4];

	entries[0].data.type = GNOME_STOCK_PIXMAP_TYPE_DATA;
	entries[0].data.width = 24;
	entries[0].data.height = 24;
	entries[0].data.xpm_data = stock_zoom_1_xpm;

	entries[1].data.type = GNOME_STOCK_PIXMAP_TYPE_DATA;
	entries[1].data.width = 24;
	entries[1].data.height = 24;
	entries[1].data.xpm_data = stock_zoom_fit_xpm;

	entries[2].data.type = GNOME_STOCK_PIXMAP_TYPE_DATA;
	entries[2].data.width = 24;
	entries[2].data.height = 24;
	entries[2].data.xpm_data = stock_zoom_in_xpm;

	entries[3].data.type = GNOME_STOCK_PIXMAP_TYPE_DATA;
	entries[3].data.width = 24;
	entries[3].data.height = 24;
	entries[3].data.xpm_data = stock_zoom_out_xpm;

	gnome_stock_pixmap_register (STOCK_ZOOM_1, GNOME_STOCK_PIXMAP_REGULAR, &entries[0]);
	gnome_stock_pixmap_register (STOCK_ZOOM_FIT, GNOME_STOCK_PIXMAP_REGULAR, &entries[1]);
	gnome_stock_pixmap_register (STOCK_ZOOM_IN, GNOME_STOCK_PIXMAP_REGULAR, &entries[2]);
	gnome_stock_pixmap_register (STOCK_ZOOM_OUT, GNOME_STOCK_PIXMAP_REGULAR, &entries[3]);
}
