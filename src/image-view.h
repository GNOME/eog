/* Eye of Gnome image viewer - image view widget
 *
 * Copyright (C) 2000 The Free Software Foundation
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

#ifndef IMAGE_VIEW_H
#define IMAGE_VIEW_H

#include <libgnome/gnome-defs.h>
#include <gconf/gconf-client.h>
#include <gtk/gtkwidget.h>
#include "image.h"
#include "preferences.h"

BEGIN_GNOME_DECLS



#define TYPE_IMAGE_VIEW            (image_view_get_type ())
#define IMAGE_VIEW(obj)            (GTK_CHECK_CAST ((obj), TYPE_IMAGE_VIEW, ImageView))
#define IMAGE_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_IMAGE_VIEW, ImageViewClass))
#define IS_IMAGE_VIEW(obj)         (GTK_CHECK_TYPE ((obj), TYPE_IMAGE_VIEW))
#define IS_IMAGE_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_IMAGE_VIEW))

typedef struct _ImageView ImageView;
typedef struct _ImageViewClass ImageViewClass;

struct _ImageView {
	GtkWidget widget;

	/* Private data */
	gpointer priv;
};

struct _ImageViewClass {
	GtkWidgetClass parent_class;

	/* Notification signals */
	void (* zoom_fit) (ImageView *view);

	/* GTK+ scrolling interface */
	void (* set_scroll_adjustments) (GtkWidget *widget,
					 GtkAdjustment *hadj,
					 GtkAdjustment *vadj);
};

GtkType image_view_get_type (void);

GtkWidget *image_view_new (void);

void image_view_set_image (ImageView *view, Image *image);
Image *image_view_get_image (ImageView *view);

void image_view_set_full_screen_zoom (ImageView *view, FullScreenZoom zoom);
FullScreenZoom image_view_get_full_screen_zoom (ImageView *view);

void image_view_set_zoom (ImageView *view, double zoom);
double image_view_get_zoom (ImageView *view);

void image_view_set_interp_type (ImageView *view, GdkInterpType interp_type);
GdkInterpType image_view_get_interp_type (ImageView *view);

void image_view_set_check_type (ImageView *view, CheckType check_type);
CheckType image_view_get_check_type (ImageView *view);

void image_view_set_check_size (ImageView *view, CheckSize check_size);
CheckSize image_view_get_check_size (ImageView *view);

void image_view_set_dither (ImageView *view, GdkRgbDither dither);
GdkRgbDither image_view_get_dither (ImageView *view);

void image_view_set_scroll (ImageView *view, ScrollType scroll);
ScrollType image_view_get_scroll (ImageView *view);

END_GNOME_DECLS

#endif
