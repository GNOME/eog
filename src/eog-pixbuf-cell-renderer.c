/* Eye Of Gnome - Pixbuf Cellrenderer
 *
 * Copyright (C) 2007 The GNOME Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on gnome-control-center code (capplets/appearance/wp-cellrenderer.c) by:
 *      - Denis Washington <denisw@svn.gnome.org>
 *      - Jens Granseuer <jensgr@gmx.net>
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

#include "eog-pixbuf-cell-renderer.h"

#include <math.h>

G_DEFINE_TYPE (EogPixbufCellRenderer, eog_pixbuf_cell_renderer, GTK_TYPE_CELL_RENDERER_PIXBUF)

static void eog_pixbuf_cell_renderer_render (GtkCellRenderer *cell,
                                             GdkWindow *window,
                                             GtkWidget *widget,
                                             GdkRectangle *background_area,
                                             GdkRectangle *cell_area,
                                             GdkRectangle *expose_area,
                                             GtkCellRendererState flags);

static void
eog_pixbuf_cell_renderer_class_init (EogPixbufCellRendererClass *klass)
{
	GtkCellRendererClass *renderer_class;

	renderer_class = (GtkCellRendererClass *) klass;
	renderer_class->render = eog_pixbuf_cell_renderer_render;
}

static void
eog_pixbuf_cell_renderer_init (EogPixbufCellRenderer *renderer)
{
}

GtkCellRenderer *
eog_pixbuf_cell_renderer_new (void)
{
	return g_object_new (eog_pixbuf_cell_renderer_get_type (), NULL);
}

static void
eog_pixbuf_cell_renderer_render (GtkCellRenderer *cell,
                                 GdkWindow *window,
                                 GtkWidget *widget,
                                 GdkRectangle *background_area,
                                 GdkRectangle *cell_area,
                                 GdkRectangle *expose_area,
                                 GtkCellRendererState flags)
{
	if ((flags & (GTK_CELL_RENDERER_SELECTED|GTK_CELL_RENDERER_PRELIT)) != 0) {
		cairo_t *cr;
		gint radius = 5;
		gint x, y, w, h;
		GtkStateType state;

		x = background_area->x;
		y = background_area->y;
		w = background_area->width;
		h = background_area->height;

		/* Sometimes width is -1 - not sure what to do here */
		if (w == -1) return;

		if ((flags & GTK_CELL_RENDERER_SELECTED) != 0) {
			if (GTK_WIDGET_HAS_FOCUS (widget))
				state = GTK_STATE_SELECTED;
			else
				state = GTK_STATE_ACTIVE;
		} else
			state = GTK_STATE_PRELIGHT;

		/* draw the selection indicator */
		cr = gdk_cairo_create (GDK_DRAWABLE (window));
		gdk_cairo_set_source_color (cr, &widget->style->base[state]);

		cairo_arc (cr, x + radius, y + radius, radius, M_PI, M_PI * 1.5);
		cairo_arc (cr, x + w - radius, y + radius, radius, M_PI * 1.5, 0);
		cairo_arc (cr, x + w - radius, y + h - radius, radius, 0, M_PI * 0.5);
		cairo_arc (cr, x + radius, y + h - radius, radius, M_PI * 0.5, M_PI);
		cairo_close_path (cr);

		/* FIXME: this should not be hardcoded to 4 */
		cairo_rectangle (cr, x + 4, y + 4, w - 8, h - 8);

		/*cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);*/
		cairo_fill (cr);
		cairo_destroy (cr);
	}

	(* GTK_CELL_RENDERER_CLASS (eog_pixbuf_cell_renderer_parent_class)->render)
	    (cell, window, widget, background_area, cell_area, expose_area, flags);
}
