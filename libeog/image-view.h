/* Eye of Gnome image viewer - image view widget
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

#ifndef IMAGE_VIEW_H
#define IMAGE_VIEW_H

#include <glib/gmacros.h>
#include <gconf/gconf-client.h>
#include <gtk/gtkwidget.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS



/* Default increment for zooming.  The current zoom factor is multiplied or
 * divided by this amount on every zooming step.  For consistency, you should
 * use the same value elsewhere in the program.
 */
#define IMAGE_VIEW_ZOOM_MULTIPLIER 1.05

/* Type of checks for views */
typedef enum {
	CHECK_TYPE_DARK,
	CHECK_TYPE_MIDTONE,
	CHECK_TYPE_LIGHT,
	CHECK_TYPE_BLACK,
	CHECK_TYPE_GRAY,
	CHECK_TYPE_WHITE
} CheckType;

/* Check size for views */
typedef enum {
	CHECK_SIZE_SMALL,
	CHECK_SIZE_MEDIUM,
	CHECK_SIZE_LARGE
} CheckSize;



#define TYPE_IMAGE_VIEW            (image_view_get_type ())
#define IMAGE_VIEW(obj)            (GTK_CHECK_CAST ((obj), TYPE_IMAGE_VIEW, ImageView))
#define IMAGE_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_IMAGE_VIEW, ImageViewClass))
#define IS_IMAGE_VIEW(obj)         (GTK_CHECK_TYPE ((obj), TYPE_IMAGE_VIEW))
#define IS_IMAGE_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_IMAGE_VIEW))

typedef struct _ImageView ImageView;
typedef struct _ImageViewClass ImageViewClass;

typedef struct _ImageViewPrivate ImageViewPrivate;

struct _ImageView {
	GtkWidget widget;

	/* Private data */
	ImageViewPrivate *priv;
};

struct _ImageViewClass {
	GtkWidgetClass parent_class;

	/* Notification signals */
	void (* zoom_fit) (ImageView *view);
	void (* zoom_changed) (ImageView *view);

	/* GTK+ scrolling interface */
	void (* set_scroll_adjustments) (GtkWidget *widget,
					 GtkAdjustment *hadj,
					 GtkAdjustment *vadj);
};

GtkType image_view_get_type (void);

GtkWidget *image_view_new (void);

void image_view_set_pixbuf (ImageView *view, GdkPixbuf *pixbuf);
GdkPixbuf *image_view_get_pixbuf (ImageView *view);

void image_view_set_zoom (ImageView *view, double zoomx, double zoomy,
			  gboolean have_anchor, int anchorx, int anchory);
void image_view_get_zoom (ImageView *view, double *zoomx, double *zoomy);

void image_view_set_interp_type (ImageView *view, GdkInterpType interp_type);
GdkInterpType image_view_get_interp_type (ImageView *view);

void image_view_set_check_type (ImageView *view, CheckType check_type);
CheckType image_view_get_check_type (ImageView *view);

void image_view_set_check_size (ImageView *view, CheckSize check_size);
CheckSize image_view_get_check_size (ImageView *view);

void image_view_set_dither (ImageView *view, GdkRgbDither dither);
GdkRgbDither image_view_get_dither (ImageView *view);

void image_view_get_scaled_size (ImageView *view, gint *width, gint *height);

G_END_DECLS

#endif
