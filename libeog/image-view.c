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

#include <config.h>
#include <math.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include "cursors.h"
#include "image-view.h"
#include "uta.h"

/* Checks */

#define CHECK_SMALL 4
#define CHECK_MEDIUM 8
#define CHECK_LARGE 16
#define CHECK_BLACK 0x00000000
#define CHECK_DARK 0x00555555
#define CHECK_GRAY 0x00808080
#define CHECK_LIGHT 0x00aaaaaa
#define CHECK_WHITE 0x00ffffff

/* Maximum size of repaint rectangles */

#define PAINT_FAST_RECT_WIDTH 256
#define PAINT_FAST_RECT_HEIGHT 256

#define PAINT_INTERP_RECT_WIDTH 128
#define PAINT_INTERP_RECT_HEIGHT 128

/* Scroll step increment */

#define SCROLL_STEP_SIZE 32

/* Maximum zoom factor */

#define MAX_ZOOM_FACTOR 128

/* Private part of the ImageView structure */
struct _ImageViewPrivate {
	/* Image being displayed */
	Image *image;

	/* Current zoom factor */
	double zoom;

	/* Previous zoom factor and zoom anchor point stored for size_allocate */
	double old_zoom;
	double zoom_x_anchor;
	double zoom_y_anchor;

	/* Full screen zoom type */
	FullScreenZoom full_screen_zoom;

	/* Adjustments for scrolling */
	GtkAdjustment *hadj;
	GtkAdjustment *vadj;

	/* Current scrolling offsets */
	int xofs, yofs;

	/* Microtile arrays for dirty region.  The first one is for immediate
	 * drawing, the second one is for interpolated drawing.
	 */
	ArtUta *uta1;
	ArtUta *uta2;

	/* Idle handler ID */
	guint idle_id;

	/* Anchor point and offsets for dragging */
	int drag_anchor_x, drag_anchor_y;
	int drag_ofs_x, drag_ofs_y;

	/* Interpolation type */
	GdkInterpType interp_type;

	/* Check type and size */
	CheckType check_type;
	CheckSize check_size;

	/* Dither type */
	GdkRgbDither dither;

	/* Scroll type */
	ScrollType scroll;

	/* Whether the image is being dragged */
	guint dragging : 1;

	/* Whether we need to change the zoom factor */
	guint need_zoom_change : 1;
};

/* Signal IDs */
enum {
	ZOOM_FIT,
	LAST_SIGNAL
};

enum {
	ARG_0,
	ARG_INTERP_TYPE,
	ARG_CHECK_TYPE,
	ARG_CHECK_SIZE,
	ARG_DITHER,
	ARG_SCROLL,
	ARG_FULL_SCREEN_ZOOM
};


static void image_view_class_init (ImageViewClass *class);
static void image_view_init (ImageView *view);
static void image_view_destroy (GtkObject *object);
static void image_view_finalize (GtkObject *object);

static void image_view_unmap (GtkWidget *widget);
static void image_view_realize (GtkWidget *widget);
static void image_view_unrealize (GtkWidget *widget);
static void image_view_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void image_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static void image_view_draw (GtkWidget *widget, GdkRectangle *area);
static gint image_view_button_press (GtkWidget *widget, GdkEventButton *event);
static gint image_view_button_release (GtkWidget *widget, GdkEventButton *event);
static gint image_view_motion (GtkWidget *widget, GdkEventMotion *event);
static gint image_view_expose (GtkWidget *widget, GdkEventExpose *event);
static gint image_view_key_press (GtkWidget *widget, GdkEventKey *event);
static void image_view_get_arg (GtkObject* obj, GtkArg* arg, guint arg_id);
static void image_view_set_arg (GtkObject* obj, GtkArg* arg, guint arg_id);


static void image_view_set_scroll_adjustments (GtkWidget *widget,
					       GtkAdjustment *hadj, GtkAdjustment *vadj);

static GtkWidgetClass *parent_class;

static guint image_view_signals[LAST_SIGNAL];



/**
 * image_view_get_type:
 * @void:
 *
 * Registers the #ImageView class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #ImageView class.
 **/
GtkType
image_view_get_type (void)
{
	static GtkType image_view_type = 0;

	if (!image_view_type) {
		static const GtkTypeInfo image_view_info = {
			"ImageView",
			sizeof (ImageView),
			sizeof (ImageViewClass),
			(GtkClassInitFunc) image_view_class_init,
			(GtkObjectInitFunc) image_view_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		image_view_type = gtk_type_unique (GTK_TYPE_WIDGET, &image_view_info);
	}

	return image_view_type;
}

/* Class initialization function for the image view */
static void
image_view_class_init (ImageViewClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_WIDGET);

	gtk_object_add_arg_type ("ImageView::interp_type", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_INTERP_TYPE);
	gtk_object_add_arg_type ("ImageView::check_type", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_CHECK_TYPE);
	gtk_object_add_arg_type ("ImageView::check_size", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_CHECK_SIZE);
	gtk_object_add_arg_type ("ImageView::dither", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_DITHER);
	gtk_object_add_arg_type ("ImageView::scroll", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_SCROLL);
	gtk_object_add_arg_type ("ImageView::full_screen_zoom", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_FULL_SCREEN_ZOOM);

	image_view_signals[ZOOM_FIT] =
		gtk_signal_new ("zoom_fit",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ImageViewClass, zoom_fit),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	object_class->destroy = image_view_destroy;
	object_class->finalize = image_view_finalize;

	class->set_scroll_adjustments = image_view_set_scroll_adjustments;
	widget_class->set_scroll_adjustments_signal =
		gtk_signal_new ("set_scroll_adjustments",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ImageViewClass, set_scroll_adjustments),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_ADJUSTMENT,
				GTK_TYPE_ADJUSTMENT);

	gtk_object_class_add_signals (object_class, image_view_signals, LAST_SIGNAL);

	widget_class->unmap = image_view_unmap;
	widget_class->realize = image_view_realize;
	widget_class->unrealize = image_view_unrealize;
	widget_class->size_request = image_view_size_request;
	widget_class->size_allocate = image_view_size_allocate;
	widget_class->draw = image_view_draw;
	widget_class->button_press_event = image_view_button_press;
	widget_class->button_release_event = image_view_button_release;
	widget_class->motion_notify_event = image_view_motion;
	widget_class->expose_event = image_view_expose;
	widget_class->key_press_event = image_view_key_press;

	object_class->get_arg = image_view_get_arg;
	object_class->set_arg = image_view_set_arg;
}

static void
image_view_get_arg (GtkObject* obj, GtkArg* arg, guint arg_id)
{
	ImageView *image_view = IMAGE_VIEW (obj);
	ImageViewPrivate *priv = image_view->priv;

	switch (arg_id) {
	case ARG_INTERP_TYPE:
		GTK_VALUE_INT(*arg) = priv->interp_type;
		break;
	case ARG_CHECK_TYPE:
		GTK_VALUE_INT(*arg) = priv->check_type;
		break;
	case ARG_CHECK_SIZE:
		GTK_VALUE_INT(*arg) = priv->check_size;
		break;
	case ARG_DITHER:
		GTK_VALUE_INT(*arg) = priv->dither;
		break;
	case ARG_SCROLL:
		GTK_VALUE_INT(*arg) = priv->scroll;
		break;
	case ARG_FULL_SCREEN_ZOOM:
		GTK_VALUE_INT(*arg) = priv->full_screen_zoom;
		break;
	default:
		g_warning ("unknown arg id `%d'", arg_id);
		break;
	}
}

static void
image_view_set_arg (GtkObject* obj, GtkArg* arg, guint arg_id)
{
	ImageView *image_view = IMAGE_VIEW (obj);

	switch (arg_id) {
	case ARG_INTERP_TYPE:
		image_view_set_interp_type (image_view, GTK_VALUE_INT(*arg));
		break;
	case ARG_CHECK_TYPE:
		image_view_set_check_type (image_view, GTK_VALUE_INT(*arg));
		break;
	case ARG_CHECK_SIZE:
		image_view_set_check_size (image_view, GTK_VALUE_INT(*arg));
		break;
	case ARG_DITHER:
		image_view_set_dither (image_view, GTK_VALUE_INT(*arg));
		break;
	case ARG_SCROLL:
		image_view_set_scroll (image_view, GTK_VALUE_INT(*arg));
		break;
	case ARG_FULL_SCREEN_ZOOM:
		image_view_set_full_screen_zoom (image_view, GTK_VALUE_INT(*arg));
		break;
	default:
		g_warning ("unknown arg id `%d'", arg_id);
		break;
	}
}

/* Object initialization function for the image view */
static void
image_view_init (ImageView *view)
{
	ImageViewPrivate *priv;

	priv = g_new0 (ImageViewPrivate, 1);
	view->priv = priv;

	GTK_WIDGET_UNSET_FLAGS (view, GTK_NO_WINDOW);
	GTK_WIDGET_SET_FLAGS (view, GTK_CAN_FOCUS);

	priv->zoom = 1.0;
}

/* Frees the dirty region uta and removes the idle handler */
static void
remove_dirty_region (ImageView *view)
{
	ImageViewPrivate *priv;

	priv = view->priv;

	if (priv->uta1 || priv->uta2) {
		g_assert (priv->idle_id != 0);

		if (priv->uta1) {
			art_uta_free (priv->uta1);
			priv->uta1 = NULL;
		}

		if (priv->uta2) {
			art_uta_free (priv->uta2);
			priv->uta2 = NULL;
		}

		g_source_remove (priv->idle_id);
		priv->idle_id = 0;
	} else
		g_assert (priv->idle_id == 0);
}

/* Destroy handler for the image view */
static void
image_view_destroy (GtkObject *object)
{
	ImageView *view;
	ImageViewPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (object));

	view = IMAGE_VIEW (object);
	priv = view->priv;

	gtk_signal_disconnect_by_data (GTK_OBJECT (priv->hadj), view);
	gtk_signal_disconnect_by_data (GTK_OBJECT (priv->vadj), view);

	/* Clean up */

	remove_dirty_region (view);

	if (priv->image) {
		image_unref (priv->image);
		priv->image = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* Finalize handler for the image view */
static void
image_view_finalize (GtkObject *object)
{
	ImageView *view;
	ImageViewPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (object));

	view = IMAGE_VIEW (object);
	priv = view->priv;

	gtk_object_unref (GTK_OBJECT (priv->hadj));
	priv->hadj = NULL;

	gtk_object_unref (GTK_OBJECT (priv->vadj));
	priv->vadj = NULL;

	g_free (priv);
	view->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}



/* Drawing core */

/* Computes the size in pixels of the scaled image */
static void
compute_scaled_size (ImageView *view, double zoom, int *width, int *height)
{
	ImageViewPrivate *priv;

	priv = view->priv;

	if (priv->image && priv->image->pixbuf) {
		*width = floor (gdk_pixbuf_get_width (priv->image->pixbuf) * zoom + 0.5);
		*height = floor (gdk_pixbuf_get_height (priv->image->pixbuf) * zoom + 0.5);
	} else
		*width = *height = 0;
}

/* Pulls a rectangle from the specified microtile array.  The rectangle is the
 * first one that would be glommed together by art_rect_list_from_uta(), and its
 * size is bounded by max_width and max_height.  The rectangle is also removed
 * from the microtile array.
 */
static void
pull_rectangle (ArtUta *uta, ArtIRect *rect, int max_width, int max_height)
{
	uta_find_first_glom_rect (uta, rect, max_width, max_height);
	uta_remove_rect (uta, rect->x0, rect->y0, rect->x1, rect->y1);
}

/* Paints a rectangle with the background color if the specified rectangle
 * intersects the dirty rectangle.
 */
static void
paint_background (ImageView *view, ArtIRect *r, ArtIRect *rect)
{
	ArtIRect d;

	art_irect_intersect (&d, r, rect);
	if (!art_irect_empty (&d))
		gdk_draw_rectangle (GTK_WIDGET (view)->window,
				    GTK_WIDGET (view)->style->bg_gc[GTK_STATE_NORMAL],
				    TRUE,
				    d.x0, d.y0,
				    d.x1 - d.x0, d.y1 - d.y0);
}

#if 0
#define PACK_RGBA
#endif

#ifdef PACK_RGBA

/* Packs an RGBA pixbuf into RGB scanlines.  The rowstride is preserved.  NOTE:
 * This will produce a pixbuf that is NOT usable with any other normal function!
 * This is just a hack to accommodate the lack of a
 * gdk_draw_rgb_image_32_dithalign(); the provided
 * gdk_draw_rgb_image_dithalign() does not take in 32-bit pixels.
 */
static void
pack_pixbuf (GdkPixbuf *pixbuf)
{
	int x, y;
	int width, height, rowstride;
	guchar *pixels, *p, *q;

	g_assert (gdk_pixbuf_get_n_channels (pixbuf) == 4);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	for (y = 0; y < height; y++) {
		p = pixels;
		q = pixels;

		for (x = 0; x < width; x++) {
			*p++ = *q++;
			*p++ = *q++;
			*p++ = *q++;
			q++;
		}

		pixels += rowstride;
	}
}

#endif

/* Paints a rectangle of the dirty region */
static void
paint_rectangle (ImageView *view, ArtIRect *rect, GdkInterpType interp_type)
{
	ImageViewPrivate *priv;
	int scaled_width, scaled_height;
	int width, height;
	int xofs, yofs;
	ArtIRect r, d;
	GdkPixbuf *tmp;
	int check_size;
	guint32 check_1, check_2;

	priv = view->priv;

	compute_scaled_size (view, priv->zoom, &scaled_width, &scaled_height);

	width = GTK_WIDGET (view)->allocation.width;
	height = GTK_WIDGET (view)->allocation.height;

	/* Compute image offsets with respect to the window */

	if (scaled_width < width)
		xofs = (width - scaled_width) / 2;
	else
		xofs = -priv->xofs;

	if (scaled_height < height)
		yofs = (height - scaled_height) / 2;
	else
		yofs = -priv->yofs;

	/* Draw background if necessary, in four steps */

	/* Top */
	if (yofs > 0) {
		r.x0 = 0;
		r.y0 = 0;
		r.x1 = width;
		r.y1 = yofs;
		paint_background (view, &r, rect);
	}

	/* Left */
	if (xofs > 0) {
		r.x0 = 0;
		r.y0 = yofs;
		r.x1 = xofs;
		r.y1 = yofs + scaled_height;
		paint_background (view, &r, rect);
	}

	/* Right */
	if (xofs >= 0) {
		r.x0 = xofs + scaled_width;
		r.y0 = yofs;
		r.x1 = width;
		r.y1 = yofs + scaled_height;
		if (r.x0 < r.x1)
			paint_background (view, &r, rect);
	}

	/* Bottom */
	if (yofs >= 0) {
		r.x0 = 0;
		r.y0 = yofs + scaled_height;
		r.x1 = width;
		r.y1 = height;
		if (r.y0 < r.y1)
			paint_background (view, &r, rect);
	}

	/* Draw the scaled image
	 *
	 * FIXME: this is not using the color correction tables!
	 */

	if (!(priv->image && priv->image->pixbuf))
		return;

	r.x0 = xofs;
	r.y0 = yofs;
	r.x1 = xofs + scaled_width;
	r.y1 = yofs + scaled_height;

	art_irect_intersect (&d, &r, rect);
	if (art_irect_empty (&d))
		return;

	/* Short-circuit the fast case to avoid a memcpy() */

	if (priv->zoom == 1.0
	    && gdk_pixbuf_get_colorspace (priv->image->pixbuf) == GDK_COLORSPACE_RGB
	    && !gdk_pixbuf_get_has_alpha (priv->image->pixbuf)
	    && gdk_pixbuf_get_bits_per_sample (priv->image->pixbuf) == 8) {
		guchar *pixels;
		int rowstride;

		rowstride = gdk_pixbuf_get_rowstride (priv->image->pixbuf);

		pixels = (gdk_pixbuf_get_pixels (priv->image->pixbuf)
			  + (d.y0 - yofs) * rowstride
			  + 3 * (d.x0 - xofs));

		gdk_draw_rgb_image_dithalign (GTK_WIDGET (view)->window,
					      GTK_WIDGET (view)->style->black_gc,
					      d.x0, d.y0,
					      d.x1 - d.x0, d.y1 - d.y0,
					      priv->dither,
					      pixels,
					      rowstride,
					      d.x0 - xofs, d.y0 - yofs);
		return;
	}

	/* For all other cases, create a temporary pixbuf */

#ifdef PACK_RGBA
	tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, d.x1 - d.x0, d.y1 - d.y0);
#else
	tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, d.x1 - d.x0, d.y1 - d.y0);
#endif

	if (!tmp) {
		g_message ("paint_rectangle(): Could not allocate temporary pixbuf of "
			   "size (%d, %d); skipping", d.x1 - d.x0, d.y1 - d.y0);
		return;
	}

	/* Compute check parameters */

	switch (priv->check_type) {
	case CHECK_TYPE_DARK:
		check_1 = CHECK_BLACK;
		check_2 = CHECK_DARK;
		break;

	case CHECK_TYPE_MIDTONE:
		check_1 = CHECK_DARK;
		check_2 = CHECK_LIGHT;
		break;

	case CHECK_TYPE_LIGHT:
		check_1 = CHECK_LIGHT;
		check_2 = CHECK_WHITE;
		break;

	case CHECK_TYPE_BLACK:
		check_1 = check_2 = CHECK_BLACK;
		break;

	case CHECK_TYPE_GRAY:
		check_1 = check_2 = CHECK_GRAY;
		break;

	case CHECK_TYPE_WHITE:
		check_1 = check_2 = CHECK_WHITE;
		break;

	default:
		check_1 = CHECK_DARK;
		check_2 = CHECK_LIGHT;
	}

	switch (priv->check_size) {
	case CHECK_SIZE_SMALL:
		check_size = CHECK_SMALL;
		break;

	case CHECK_SIZE_MEDIUM:
		check_size = CHECK_MEDIUM;
		break;

	case CHECK_SIZE_LARGE:
		check_size = CHECK_LARGE;
		break;

	default:
		check_size = CHECK_LARGE;
		break;
	}

	/* Draw! */

	gdk_pixbuf_composite_color (priv->image->pixbuf,
				    tmp,
				    0, 0,
				    d.x1 - d.x0, d.y1 - d.y0,
				    -(d.x0 - xofs), -(d.y0 - yofs),
				    priv->zoom, priv->zoom,
				    (priv->zoom == 1.0) ? GDK_INTERP_NEAREST : interp_type,
				    255,
				    d.x0 - xofs, d.y0 - yofs,
				    check_size,
				    check_1, check_2);

#ifdef PACK_RGBA
	pack_pixbuf (tmp);
#endif

	gdk_draw_rgb_image_dithalign (GTK_WIDGET (view)->window,
				      GTK_WIDGET (view)->style->black_gc,
				      d.x0, d.y0,
				      d.x1 - d.x0, d.y1 - d.y0,
				      priv->dither,
				      gdk_pixbuf_get_pixels (tmp),
				      gdk_pixbuf_get_rowstride (tmp),
				      d.x0 - xofs, d.y0 - yofs);

	gdk_pixbuf_unref (tmp);

#if 0
	gdk_draw_line (GTK_WIDGET (view)->window,
		       GTK_WIDGET (view)->style->black_gc,
		       d.x0, d.y0,
		       d.x1 - 1, d.y1 - 1);
	gdk_draw_line (GTK_WIDGET (view)->window,
		       GTK_WIDGET (view)->style->black_gc,
		       d.x1 - 1, d.y0,
		       d.x0, d.y1 - 1);
#endif
}

#include <stdio.h>

/* Idle handler for the drawing process.  We pull a rectangle from the dirty
 * region microtile array, paint it, and leave the rest to the next idle
 * iteration.
 */
static gboolean
paint_iteration_idle (gpointer data)
{
	ImageView *view;
	ImageViewPrivate *priv;
	ArtIRect rect;

	view = IMAGE_VIEW (data);
	priv = view->priv;

	g_assert (priv->uta1 != NULL || priv->uta2 != NULL);

	if (priv->uta1) {
		/* Paint the no-interpolation cases as fast as possible */

		if (priv->scroll == SCROLL_TWO_PASS || priv->interp_type == GDK_INTERP_NEAREST)
			while (1) {
				pull_rectangle (priv->uta1, &rect,
						PAINT_FAST_RECT_WIDTH, PAINT_FAST_RECT_HEIGHT);

				if (art_irect_empty (&rect)) {
					art_uta_free (priv->uta1);
					priv->uta1 = NULL;
					break;
				}

				paint_rectangle (view, &rect, GDK_INTERP_NEAREST);

				if (priv->scroll == SCROLL_TWO_PASS)
					priv->uta2 = uta_add_rect (priv->uta2,
								   rect.x0, rect.y0,
								   rect.x1, rect.y1);
			}
		else {
			pull_rectangle (priv->uta1, &rect,
					PAINT_INTERP_RECT_WIDTH, PAINT_INTERP_RECT_HEIGHT);

			if (art_irect_empty (&rect)) {
				art_uta_free (priv->uta1);
				priv->uta1 = NULL;
			} else
				paint_rectangle (view, &rect, priv->interp_type);
		}
	} else if (priv->uta2) {
		pull_rectangle (priv->uta2, &rect,
				PAINT_INTERP_RECT_WIDTH, PAINT_INTERP_RECT_HEIGHT);

		if (art_irect_empty (&rect)) {
			art_uta_free (priv->uta2);
			priv->uta2 = NULL;
		} else
			paint_rectangle (view, &rect, priv->interp_type);
	}

	if (!priv->uta1 && !priv->uta2) {
		priv->idle_id = 0;
		return FALSE;
	}

	return TRUE;
}

/* Queues a repaint of the specified area in window coordinates */
static void
request_paint_area (ImageView *view, GdkRectangle *area, gboolean asynch)
{
	ImageViewPrivate *priv;
	ArtIRect r;

	priv = view->priv;

	if (!GTK_WIDGET_DRAWABLE (view))
		return;

	r.x0 = MAX (0, area->x);
	r.y0 = MAX (0, area->y);
	r.x1 = MIN (GTK_WIDGET (view)->allocation.width, area->x + area->width);
	r.y1 = MIN (GTK_WIDGET (view)->allocation.height, area->y + area->height);

	if (r.x0 >= r.x1 || r.y0 >= r.y1)
		return;

	/* Unless told not to, do nearest neighbor or 1:1 zoom synchronously for
         * speed.
	 */

	if (!asynch && (priv->interp_type == GDK_INTERP_NEAREST || priv->zoom == 1.0)) {
		paint_rectangle (view, &r, priv->interp_type);
		return;
	}

	/* All other interpolation types are delayed */

	if (priv->uta1 || priv->uta2)
		g_assert (priv->idle_id != 0);
	else {
		g_assert (priv->idle_id == 0);
		priv->idle_id = g_idle_add (paint_iteration_idle, view);
	}

	if (asynch || priv->scroll != SCROLL_TWO_PASS)
		priv->uta1 = uta_add_rect (priv->uta1, r.x0, r.y0, r.x1, r.y1);
	else {
		paint_rectangle (view, &r, GDK_INTERP_NEAREST);
		priv->uta2 = uta_add_rect (priv->uta2, r.x0, r.y0, r.x1, r.y1);
	}
}

/* Scrolls the view to the specified offsets.  Does not perform range checking!  */
static void
scroll_to (ImageView *view, int x, int y)
{
	ImageViewPrivate *priv;
	int xofs, yofs;
	GdkWindow *window;
	GdkGC *gc;
	int width, height;
	int src_x, src_y;
	int dest_x, dest_y;
	int twidth, theight;
	GdkEvent *event;

	priv = view->priv;

	/* Compute offsets and check bounds */

	xofs = x - priv->xofs;
	yofs = y - priv->yofs;

	if (xofs == 0 && yofs == 0)
		return;

	priv->xofs = x;
	priv->yofs = y;

	if (!GTK_WIDGET_DRAWABLE (view))
		return;

	width = GTK_WIDGET (view)->allocation.width;
	height = GTK_WIDGET (view)->allocation.height;

	if (abs (xofs) >= width || abs (yofs) >= height) {
		GdkRectangle area;

		area.x = 0;
		area.y = 0;
		area.width = width;
		area.height = height;

		request_paint_area (view, &area, FALSE);
		return;
	}

	window = GTK_WIDGET (view)->window;

	/* Ensure that the uta has the full size */

	twidth = (width + ART_UTILE_SIZE - 1) >> ART_UTILE_SHIFT;
	theight = (height + ART_UTILE_SIZE - 1) >> ART_UTILE_SHIFT;

	if (priv->uta1 || priv->uta2)
		g_assert (priv->idle_id != 0);
	else
		priv->idle_id = g_idle_add (paint_iteration_idle, view);

	priv->uta1 = uta_ensure_size (priv->uta1, 0, 0, twidth, theight);

	/* Copy the uta area */

	src_x = xofs < 0 ? 0 : xofs;
	src_y = yofs < 0 ? 0 : yofs;
	dest_x = xofs < 0 ? -xofs : 0;
	dest_y = yofs < 0 ? -yofs : 0;

	uta_copy_area (priv->uta1,
		       src_x, src_y,
		       dest_x, dest_y,
		       width - abs (xofs), height - abs (yofs));

	if (priv->scroll == SCROLL_TWO_PASS && priv->uta2) {
		priv->uta2 = uta_ensure_size (priv->uta2, 0, 0, twidth, theight);

		uta_copy_area (priv->uta2,
			       src_x, src_y,
			       dest_x, dest_y,
			       width - abs (xofs), height - abs (yofs));
	}

	/* Copy the window area */

	gc = gdk_gc_new (window);
	gdk_gc_set_exposures (gc, TRUE);

	gdk_window_copy_area (window,
			      gc,
			      dest_x, dest_y,
			      window,
			      src_x, src_y,
			      width - abs (xofs),
			      height - abs (yofs));

	gdk_gc_destroy (gc);

	/* Add the scrolled-in region */

	if (xofs) {
		GdkRectangle r;

		r.x = xofs < 0 ? 0 : width - xofs;
		r.y = 0;
		r.width = abs (xofs);
		r.height = height;

		request_paint_area (view, &r, FALSE);
	}

	if (yofs) {
		GdkRectangle r;

		r.x = 0;
		r.y = yofs < 0 ? 0 : height - yofs;
		r.width = width;
		r.height = abs (yofs);

		request_paint_area (view, &r, FALSE);
	}

	/* Process graphics exposures */

	while ((event = gdk_event_get_graphics_expose (window)) != NULL) {
		gtk_widget_event (GTK_WIDGET (view), event);
		if (event->expose.count == 0) {
			gdk_event_free (event);
			break;
		}
		gdk_event_free (event);
	}
}



/* Widget methods */

/* Unmap handler for the image view */
static void
image_view_unmap (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (widget));

	remove_dirty_region (IMAGE_VIEW (widget));

	if (GTK_WIDGET_CLASS (parent_class)->unmap)
		(* GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
}

/* Realize handler for the image view */
static void
image_view_realize (GtkWidget *widget)
{
	GdkWindowAttr attr;
	int attr_mask;
	GdkCursor *cursor;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (widget));

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	attr.window_type = GDK_WINDOW_CHILD;
	attr.x = widget->allocation.x;
	attr.y = widget->allocation.y;
	attr.width = widget->allocation.width;
	attr.height = widget->allocation.height;
	attr.wclass = GDK_INPUT_OUTPUT;
	attr.visual = gdk_rgb_get_visual ();
	attr.colormap = gdk_rgb_get_cmap ();
	attr.event_mask = (gtk_widget_get_events (widget)
			   | GDK_EXPOSURE_MASK
			   | GDK_BUTTON_PRESS_MASK
			   | GDK_KEY_PRESS_MASK);

	attr_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attr, attr_mask);
	gdk_window_set_user_data (widget->window, widget);

	cursor = cursor_get (widget->window, CURSOR_HAND_OPEN);
	gdk_window_set_cursor (widget->window, cursor);
	gdk_cursor_destroy (cursor);

	widget->style = gtk_style_attach (widget->style, widget->window);

	gdk_window_set_back_pixmap (widget->window, NULL, FALSE);
}

/* Unrealize handler for the image view */
static void
image_view_unrealize (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (widget));

	remove_dirty_region (IMAGE_VIEW (widget));

	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

/* Size_request handler for the image view */
static void
image_view_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	ImageView *view;
	ImageViewPrivate *priv;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (widget));
	g_return_if_fail (requisition != NULL);

	view = IMAGE_VIEW (widget);
	priv = view->priv;

	requisition->width = requisition->height = 0;
}

/* Sets the zoom anchor point with respect to the specified window position */
static void
set_zoom_anchor (ImageView *view, int x, int y)
{
	ImageViewPrivate *priv;

	priv = view->priv;
	priv->zoom_x_anchor = (double) x / GTK_WIDGET (view)->allocation.width;
	priv->zoom_y_anchor = (double) y / GTK_WIDGET (view)->allocation.height;
}

/* Sets the zoom anchor point to be the middle of the visible area */
static void
set_default_zoom_anchor (ImageView *view)
{
	ImageViewPrivate *priv;

	priv = view->priv;
	priv->zoom_x_anchor = priv->zoom_y_anchor = 0.5;
}

/* Computes the offsets for the new zoom value so that they keep the image
 * centered on the view.
 */
static void
compute_center_zoom_offsets (ImageView *view,
			     int old_width, int old_height,
			     int new_width, int new_height,
			     int *xofs, int *yofs)
{
	ImageViewPrivate *priv;
	int old_scaled_width, old_scaled_height;
	int new_scaled_width, new_scaled_height;
	double view_cx, view_cy;

	priv = view->priv;
	g_assert (priv->need_zoom_change);

	compute_scaled_size (view, priv->old_zoom, &old_scaled_width, &old_scaled_height);

	if (old_scaled_width < old_width)
		view_cx = (priv->zoom_x_anchor * old_scaled_width) / priv->old_zoom;
	else
		view_cx = (priv->xofs + priv->zoom_x_anchor * old_width) / priv->old_zoom;

	if (old_scaled_height < old_height)
		view_cy = (priv->zoom_y_anchor * old_scaled_height) / priv->old_zoom;
	else
		view_cy = (priv->yofs + priv->zoom_y_anchor * old_height) / priv->old_zoom;

	compute_scaled_size (view, priv->zoom, &new_scaled_width, &new_scaled_height);

	if (new_scaled_width < new_width)
		*xofs = 0;
	else
		*xofs = floor (view_cx * priv->zoom - priv->zoom_x_anchor * new_width + 0.5);

	if (new_scaled_height < new_height)
		*yofs = 0;
	else
		*yofs = floor (view_cy * priv->zoom - priv->zoom_y_anchor * new_height + 0.5);
}

/* Size_allocate handler for the image view */
static void
image_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	ImageView *view;
	ImageViewPrivate *priv;
	int xofs, yofs;
	int scaled_width, scaled_height;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (widget));
	g_return_if_fail (allocation != NULL);

	view = IMAGE_VIEW (widget);
	priv = view->priv;

	/* Compute new scroll offsets */

	if (priv->need_zoom_change) {
		compute_center_zoom_offsets (view,
					     widget->allocation.width, widget->allocation.height,
					     allocation->width, allocation->height,
					     &xofs, &yofs);

		set_default_zoom_anchor (view);
		priv->need_zoom_change = FALSE;
	} else {
		xofs = priv->xofs;
		yofs = priv->yofs;
	}

	/* Resize the window */

	widget->allocation = *allocation;

	if (GTK_WIDGET_REALIZED (widget))
		gdk_window_move_resize (widget->window,
					allocation->x,
					allocation->y,
					allocation->width,
					allocation->height);

	/* Set scroll increments */

	compute_scaled_size (view, priv->zoom, &scaled_width, &scaled_height);

	priv->hadj->page_size = MIN (scaled_width, allocation->width);
	priv->hadj->page_increment = allocation->width / 2;
	priv->hadj->step_increment = SCROLL_STEP_SIZE;

	priv->vadj->page_size = MIN (scaled_height, allocation->height);
	priv->vadj->page_increment = allocation->height / 2;
	priv->vadj->step_increment = SCROLL_STEP_SIZE;

	/* Set scroll bounds and new offsets */

	priv->hadj->lower = 0;
	priv->hadj->upper = scaled_width;
	xofs = CLAMP (xofs, 0, priv->hadj->upper - priv->hadj->page_size);

	priv->vadj->lower = 0;
	priv->vadj->upper = scaled_height;
	yofs = CLAMP (yofs, 0, priv->vadj->upper - priv->vadj->page_size);

	gtk_signal_emit_by_name (GTK_OBJECT (priv->hadj), "changed");
	gtk_signal_emit_by_name (GTK_OBJECT (priv->vadj), "changed");

	if (priv->hadj->value != xofs) {
		priv->hadj->value = xofs;
		priv->xofs = xofs;

		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->hadj), view);
		gtk_signal_emit_by_name (GTK_OBJECT (priv->hadj), "value_changed");
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->hadj), view);
	}

	if (priv->vadj->value != yofs) {
		priv->vadj->value = yofs;
		priv->yofs = yofs;

		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->vadj), view);
		gtk_signal_emit_by_name (GTK_OBJECT (priv->vadj), "value_changed");
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->vadj), view);
	}
}

/* Draw handler for the image view */
static void
image_view_draw (GtkWidget *widget, GdkRectangle *area)
{
	ImageView *view;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (widget));
	g_return_if_fail (area != NULL);

	view = IMAGE_VIEW (widget);

	request_paint_area (view, area, TRUE);
}

/* Button press handler for the image view */
static gint
image_view_button_press (GtkWidget *widget, GdkEventButton *event)
{
	ImageView *view;
	ImageViewPrivate *priv;
	GdkCursor *cursor;
	int retval;

	view = IMAGE_VIEW (widget);
	priv = view->priv;

	if (!GTK_WIDGET_HAS_FOCUS (widget))
		gtk_widget_grab_focus (widget);

	if (priv->dragging)
		return FALSE;

	switch (event->button) {
	case 1:
		cursor = cursor_get (widget->window, CURSOR_HAND_CLOSED);
		retval = gdk_pointer_grab (widget->window,
					   FALSE,
					   (GDK_POINTER_MOTION_MASK
					    | GDK_POINTER_MOTION_HINT_MASK
					    | GDK_BUTTON_RELEASE_MASK),
					   NULL,
					   cursor,
					   event->time);
		gdk_cursor_destroy (cursor);

		if (retval != 0)
			return FALSE;

		priv->dragging = TRUE;
		priv->drag_anchor_x = event->x;
		priv->drag_anchor_y = event->y;

		priv->drag_ofs_x = priv->xofs;
		priv->drag_ofs_y = priv->yofs;

		return TRUE;

	case 4:
		set_zoom_anchor (view, event->x, event->y);
		image_view_set_zoom (view, priv->zoom * 1.05);
		return TRUE;

	case 5:
		set_zoom_anchor (view, event->x, event->y);
		image_view_set_zoom (view, priv->zoom / 1.05);
		return TRUE;

	default:
		break;
	}

	return FALSE;
}

/* Drags the image to the specified position */
static void
drag_to (ImageView *view, int x, int y)
{
	ImageViewPrivate *priv;
	int dx, dy;

	priv = view->priv;

	dx = priv->drag_anchor_x - x;
	dy = priv->drag_anchor_y - y;

	x = CLAMP (priv->drag_ofs_x + dx, 0, priv->hadj->upper - priv->hadj->page_size);
	y = CLAMP (priv->drag_ofs_y + dy, 0, priv->vadj->upper - priv->vadj->page_size);

	scroll_to (view, x, y);

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->hadj), view);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->vadj), view);

	priv->hadj->value = x;
	priv->vadj->value = y;

	gtk_signal_emit_by_name (GTK_OBJECT (priv->hadj), "value_changed");
	gtk_signal_emit_by_name (GTK_OBJECT (priv->vadj), "value_changed");

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->hadj), view);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->vadj), view);
}

/* Button release handler for the image view */
static gint
image_view_button_release (GtkWidget *widget, GdkEventButton *event)
{
	ImageView *view;
	ImageViewPrivate *priv;

	view = IMAGE_VIEW (widget);
	priv = view->priv;

	if (!priv->dragging || event->button != 1)
		return FALSE;

	drag_to (view, event->x, event->y);
	priv->dragging = FALSE;
	gdk_pointer_ungrab (event->time);

	return TRUE;
}

/* Motion handler for the image view */
static gint
image_view_motion (GtkWidget *widget, GdkEventMotion *event)
{
	ImageView *view;
	ImageViewPrivate *priv;
	gint x, y;
	GdkModifierType mods;

	view = IMAGE_VIEW (widget);
	priv = view->priv;

	if (!priv->dragging)
		return FALSE;

	if (event->is_hint)
		gdk_window_get_pointer (widget->window, &x, &y, &mods);
	else {
		x = event->x;
		y = event->y;
	}

	drag_to (view, x, y);
	return TRUE;
}

/* Expose handler for the image view */
static gint
image_view_expose (GtkWidget *widget, GdkEventExpose *event)
{
	ImageView *view;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (IS_IMAGE_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	view = IMAGE_VIEW (widget);

	request_paint_area (view, &event->area, TRUE);
	return TRUE;
}

/* Key press handler for the image view */
static gint
image_view_key_press (GtkWidget *widget, GdkEventKey *event)
{
	ImageView *view;
	ImageViewPrivate *priv;
	gboolean do_zoom;
	double zoom;
	gboolean do_scroll;
	int xofs, yofs;

	view = IMAGE_VIEW (widget);
	priv = view->priv;

	do_zoom = FALSE;
	do_scroll = FALSE;
	xofs = yofs = 0;
	zoom = 1.0;

	if ((event->state & (GDK_MODIFIER_MASK & ~GDK_LOCK_MASK)) != 0)
		return FALSE;

	switch (event->keyval) {
	case GDK_Up:
		do_scroll = TRUE;
		xofs = 0;
		yofs = -SCROLL_STEP_SIZE;
		break;

	case GDK_Down:
		do_scroll = TRUE;
		xofs = 0;
		yofs = SCROLL_STEP_SIZE;
		break;

	case GDK_Left:
		do_scroll = TRUE;
		xofs = -SCROLL_STEP_SIZE;
		yofs = 0;
		break;

	case GDK_Right:
		do_scroll = TRUE;
		xofs = SCROLL_STEP_SIZE;
		yofs = 0;
		break;

	case GDK_equal:
	case GDK_KP_Add:
		do_zoom = TRUE;
		zoom = priv->zoom * 1.05;
		break;

	case GDK_minus:
	case GDK_KP_Subtract:
		do_zoom = TRUE;
		zoom = priv->zoom / 1.05;
		break;

	case GDK_1:
		do_zoom = TRUE;
		zoom = 1.0;
		break;

	case GDK_F:
	case GDK_f:
		gtk_signal_emit (GTK_OBJECT (view), image_view_signals[ZOOM_FIT]);
		break;

	default:
		return FALSE;
	}

	if (do_zoom) {
		gint x, y;

		gdk_window_get_pointer (widget->window, &x, &y, NULL);

		if (x >= 0 && x < widget->allocation.width
		    && y >= 0 && y < widget->allocation.height)
			set_zoom_anchor (view, x, y);
		else
			set_default_zoom_anchor (view);

		image_view_set_zoom (view, zoom);
	}

	if (do_scroll) {
		int x, y;

		x = CLAMP (priv->xofs + xofs, 0, priv->hadj->upper - priv->hadj->page_size);
		y = CLAMP (priv->yofs + yofs, 0, priv->vadj->upper - priv->vadj->page_size);

		scroll_to (view, x, y);

		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->hadj), view);
		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->vadj), view);

		priv->hadj->value = x;
		priv->vadj->value = y;

		gtk_signal_emit_by_name (GTK_OBJECT (priv->hadj), "value_changed");
		gtk_signal_emit_by_name (GTK_OBJECT (priv->vadj), "value_changed");

		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->hadj), view);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->vadj), view);
	}

	return TRUE;
}

/* Callback used when an adjustment is changed */
static void
adjustment_changed_cb (GtkAdjustment *adj, gpointer data)
{
	ImageView *view;
	ImageViewPrivate *priv;

	view = IMAGE_VIEW (data);
	priv = view->priv;

	scroll_to (view, priv->hadj->value, priv->vadj->value);
}

/* Set_scroll_adjustments handler for the image view */
static void
image_view_set_scroll_adjustments (GtkWidget *widget,
				   GtkAdjustment *hadj,
				   GtkAdjustment *vadj)
{
	ImageView *view;
	ImageViewPrivate *priv;
	gboolean need_adjust;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (widget));

	view = IMAGE_VIEW (widget);
	priv = view->priv;

	if (hadj)
		g_return_if_fail (GTK_IS_ADJUSTMENT (hadj));
	else
		hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

	if (vadj)
		g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
	else
		vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

	if (priv->hadj && priv->hadj != hadj) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->hadj), view);
		gtk_object_unref (GTK_OBJECT (priv->hadj));
	}

	if (priv->vadj && priv->vadj != vadj) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->vadj), view);
		gtk_object_unref (GTK_OBJECT (priv->vadj));
	}

	need_adjust = FALSE;

	if (priv->hadj != hadj) {
		priv->hadj = hadj;
		gtk_object_ref (GTK_OBJECT (priv->hadj));
		gtk_object_sink (GTK_OBJECT (priv->hadj));

		gtk_signal_connect (GTK_OBJECT (priv->hadj), "value_changed",
				    GTK_SIGNAL_FUNC (adjustment_changed_cb),
				    view);

		need_adjust = TRUE;
	}

	if (priv->vadj != vadj) {
		priv->vadj = vadj;
		gtk_object_ref (GTK_OBJECT (priv->vadj));
		gtk_object_sink (GTK_OBJECT (priv->vadj));

		gtk_signal_connect (GTK_OBJECT (priv->vadj), "value_changed",
				    GTK_SIGNAL_FUNC (adjustment_changed_cb),
				    view);

		need_adjust = TRUE;
	}

	if (need_adjust)
		adjustment_changed_cb (NULL, view);
}



/**
 * image_view_new:
 * @void:
 *
 * Creates a new empty image view widget.
 *
 * Return value: A newly-created image view.
 **/
GtkWidget *
image_view_new (void)
{
	return GTK_WIDGET (gtk_type_new (TYPE_IMAGE_VIEW));
}

/* Requests a full redraw of the image view */
static void
redraw_all (ImageView *view)
{
	GdkRectangle r;

	r.x = 0;
	r.y = 0;
	r.width = GTK_WIDGET (view)->allocation.width;
	r.height = GTK_WIDGET (view)->allocation.height;

	request_paint_area (view, &r, TRUE);
}

/**
 * image_view_set_image:
 * @view: An image view.
 * @image: An image.
 *
 * Sets the image that an image view will display.
 **/
void
image_view_set_image (ImageView *view, Image *image)
{
	ImageViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (view));

	priv = view->priv;

	if (priv->image == image)
		return;

	if (image)
		image_ref (image);

	if (priv->image)
		image_unref (priv->image);

	priv->image = image;

	/* FIXME: adjust zoom / image offsets; maybe just offsets here */

	redraw_all (view);
}

/**
 * image_view_get_image:
 * @view:
 *
 * Queries the image that an image view is displaying.
 *
 * Return value: The current image, or NULL if no image is being displayed.
 **/
Image *
image_view_get_image (ImageView *view)
{
	ImageViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (IS_IMAGE_VIEW (view), NULL);

	priv = view->priv;
	return priv->image;
}

/**
 * image_view_set_zoom:
 * @view: An image view.
 * @zoom: Zoom factor.
 *
 * Sets the zoom factor for an image view.
 **/
void
image_view_set_zoom (ImageView *view, double zoom)
{
	ImageViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (view));
	g_return_if_fail (zoom > 0.0);

	priv = view->priv;

	if (zoom > MAX_ZOOM_FACTOR)
		zoom = MAX_ZOOM_FACTOR;

	if (priv->zoom == zoom)
		return;

	if (!priv->need_zoom_change) {
		priv->old_zoom = priv->zoom;
		priv->need_zoom_change = TRUE;
	}

	priv->zoom = zoom;

	gtk_widget_queue_resize (GTK_WIDGET (view));
}

/**
 * image_view_get_zoom:
 * @view: An image view.
 *
 * Queries the zoom factor of an image view.
 *
 * Return value: Current zoom factor.
 **/
double
image_view_get_zoom (ImageView *view)
{
	ImageViewPrivate *priv;

	g_return_val_if_fail (view != NULL, -1.0);
	g_return_val_if_fail (IS_IMAGE_VIEW (view), -1.0);

	priv = view->priv;
	return priv->zoom;
}

/**
 * image_view_set_interp_type:
 * @view: An image view.
 * @interp_type: Interpolation type.
 *
 * Sets the interpolation type on an image view.
 **/
void
image_view_set_interp_type (ImageView *view, GdkInterpType interp_type)
{
	ImageViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (view));

	priv = view->priv;

	if (priv->interp_type == interp_type)
		return;

	priv->interp_type = interp_type;
	redraw_all (view);
}

/**
 * image_view_get_interp_type:
 * @view: An image view.
 *
 * Queries the interpolation type of an image view.
 *
 * Return value: Interpolation type.
 **/
GdkInterpType
image_view_get_interp_type (ImageView *view)
{
	ImageViewPrivate *priv;

	g_return_val_if_fail (view != NULL, GDK_INTERP_NEAREST);
	g_return_val_if_fail (IS_IMAGE_VIEW (view), GDK_INTERP_NEAREST);

	priv = view->priv;
	return priv->interp_type;
}

/**
 * image_view_set_check_type:
 * @view: An image view.
 * @check_type: Check type.
 *
 * Sets the check type on an image view.
 **/
void
image_view_set_check_type (ImageView *view, CheckType check_type)
{
	ImageViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (view));

	priv = view->priv;

	if (priv->check_type == check_type)
		return;

	priv->check_type = check_type;
	redraw_all (view);
}

/**
 * image_view_get_check_type:
 * @view: An image view.
 *
 * Queries the check type of an image view.
 *
 * Return value: Check type.
 **/
CheckType
image_view_get_check_type (ImageView *view)
{
	ImageViewPrivate *priv;

	g_return_val_if_fail (view != NULL, CHECK_TYPE_BLACK);
	g_return_val_if_fail (IS_IMAGE_VIEW (view), CHECK_TYPE_BLACK);

	priv = view->priv;
	return priv->check_type;
}

/**
 * image_view_set_check_size:
 * @view: An image view.
 * @check_size: Check size.
 *
 * Sets the check size on an image view.
 **/
void
image_view_set_check_size (ImageView *view, CheckSize check_size)
{
	ImageViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (view));

	priv = view->priv;

	if (priv->check_size == check_size)
		return;

	priv->check_size = check_size;
	redraw_all (view);
}

/**
 * image_view_get_check_size:
 * @view: An image view.
 *
 * Queries the check size on an image view.
 *
 * Return value: Check size.
 **/
CheckSize
image_view_get_check_size (ImageView *view)
{
	ImageViewPrivate *priv;

	g_return_val_if_fail (view != NULL, CHECK_SIZE_SMALL);
	g_return_val_if_fail (IS_IMAGE_VIEW (view), CHECK_SIZE_SMALL);

	priv = view->priv;
	return priv->check_size;
}

/**
 * image_view_set_dither:
 * @view: An image view.
 * @dither: Dither type.
 *
 * Sets the dither type on an image view.
 **/
void
image_view_set_dither (ImageView *view, GdkRgbDither dither)
{
	ImageViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (view));

	priv = view->priv;

	if (priv->dither == dither)
		return;

	priv->dither = dither;
	redraw_all (view);
}

/**
 * image_view_get_dither:
 * @view: An image view.
 *
 * Queries the dither type of an image view.
 *
 * Return value: Dither type.
 **/
GdkRgbDither
image_view_get_dither (ImageView *view)
{
	ImageViewPrivate *priv;

	g_return_val_if_fail (view != NULL, GDK_RGB_DITHER_NONE);
	g_return_val_if_fail (IS_IMAGE_VIEW (view), GDK_RGB_DITHER_NONE);

	priv = view->priv;
	return priv->dither;
}

/**
 * image_view_set_scroll:
 * @view: An image view.
 * @scroll: Scrolling type.
 *
 * Sets the scrolling type on an image view.
 **/
void
image_view_set_scroll (ImageView *view, ScrollType scroll)
{
	ImageViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (view));

	priv = view->priv;

	if (priv->scroll == scroll)
		return;

	priv->scroll = scroll;
	redraw_all (view);
}

/**
 * image_view_get_scroll:
 * @view: An image view.
 *
 * Queries the scrolling type of an image view.
 *
 * Return value: Scrolling type.
 **/
ScrollType
image_view_get_scroll (ImageView *view)
{
	ImageViewPrivate *priv;

	g_return_val_if_fail (view != NULL, SCROLL_NORMAL);
	g_return_val_if_fail (IS_IMAGE_VIEW (view), SCROLL_NORMAL);

	priv = view->priv;
	return priv->scroll;
}

/**
 * image_view_set_full_screen_zoom:
 * @view: An image view.
 * @full_screen_zoom: Full screen zooming type.
 *
 * Sets the full screen zooming type on an image view.
 **/
void
image_view_set_full_screen_zoom (ImageView *view, FullScreenZoom full_screen_zoom)
{
	ImageViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (view));

	priv = view->priv;

	if (priv->full_screen_zoom == full_screen_zoom)
		return;

	priv->full_screen_zoom = full_screen_zoom;
	redraw_all (view);
}

/**
 * image_view_get_full_screen_zoom:
 * @view: An image view.
 *
 * Queries the full_screen_zooming type of an image view.
 *
 * Return value: full screen zooming type.
 **/
FullScreenZoom
image_view_get_full_screen_zoom (ImageView *view)
{
	ImageViewPrivate *priv;

	g_return_val_if_fail (view != NULL, FULL_SCREEN_ZOOM_1);
	g_return_val_if_fail (IS_IMAGE_VIEW (view), FULL_SCREEN_ZOOM_1);

	priv = view->priv;
	return priv->full_screen_zoom;
}
