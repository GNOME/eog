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

#include <config.h>
#include <math.h>
#include <gtk/gtksignal.h>
#include "cursors.h"
#include "image-view.h"
#include "uta.h"



/* Checks */

#define CHECK_SIZE 16
#define CHECK_DARK 0x00555555
#define CHECK_LIGHT 0x00aaaaaa

/* Maximum size of repaint rectangles */

#define PAINT_RECT_WIDTH 256
#define PAINT_RECT_HEIGHT 64



/* Private part of the ImageView structure */
typedef struct {
	/* Image being displayed */
	Image *image;

	/* Zoom factor */
	double zoom;

	/* Adjustments for scrolling */
	GtkAdjustment *hadj;
	GtkAdjustment *vadj;

	/* Microtile array for dirty region */
	ArtUta *uta;

	/* Idle handler ID */
	guint idle_id;
} ImageViewPrivate;



static void image_view_class_init (ImageViewClass *class);
static void image_view_init (ImageView *view);
static void image_view_destroy (GtkObject *object);

static void image_view_unmap (GtkWidget *widget);
static void image_view_realize (GtkWidget *widget);
static void image_view_unrealize (GtkWidget *widget);
static void image_view_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void image_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static void image_view_draw (GtkWidget *widget, GdkRectangle *area);
static gint image_view_expose (GtkWidget *widget, GdkEventExpose *event);

static void image_view_set_scroll_adjustments (GtkWidget *widget,
					       GtkAdjustment *hadj, GtkAdjustment *vadj);

static GtkWidgetClass *parent_class;



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

	object_class->destroy = image_view_destroy;

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

	widget_class->unmap = image_view_unmap;
	widget_class->realize = image_view_realize;
	widget_class->unrealize = image_view_unrealize;
	widget_class->size_request = image_view_size_request;
	widget_class->size_allocate = image_view_size_allocate;
	widget_class->draw = image_view_draw;
	widget_class->expose_event = image_view_expose;
}

/* Object initialization function for the image view */
static void
image_view_init (ImageView *view)
{
	ImageViewPrivate *priv;

	priv = g_new0 (ImageViewPrivate, 1);
	view->priv = priv;

	priv->zoom = 1.0;

	GTK_WIDGET_UNSET_FLAGS (view, GTK_NO_WINDOW);
}

/* Frees the dirty region uta and removes the idle handler */
static void
remove_dirty_region (ImageView *view)
{
	ImageViewPrivate *priv;

	priv = view->priv;

	if (priv->uta) {
		g_assert (priv->idle_id != 0);

		art_uta_free (priv->uta);
		g_source_remove (priv->idle_id);

		priv->uta = NULL;
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

	remove_dirty_region (view);

	if (priv->image) {
		image_unref (priv->image);
		priv->image = NULL;
	}

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Drawing core */

/* Computes the size in pixels of the scaled image */
static void
compute_scaled_size (ImageView *view, int *width, int *height)
{
	ImageViewPrivate *priv;

	priv = view->priv;

	if (priv->image && priv->image->pixbuf) {
		*width = floor (gdk_pixbuf_get_width (priv->image->pixbuf) * priv->zoom + 0.5);
		*height = floor (gdk_pixbuf_get_height (priv->image->pixbuf) * priv->zoom + 0.5);
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

/* Paints a rectangle of the dirty region */
static void
paint_rectangle (ImageView *view, ArtIRect *rect)
{
	ImageViewPrivate *priv;
	int scaled_width, scaled_height;
	int width, height;
	int xofs, yofs;
	ArtIRect r, d;
	GdkPixbuf *tmp;

	priv = view->priv;

	compute_scaled_size (view, &scaled_width, &scaled_height);

	width = GTK_WIDGET (view)->allocation.width;
	height = GTK_WIDGET (view)->allocation.height;

	/* Compute image offsets with respect to the window */

	if (scaled_width < width)
		xofs = (width - scaled_width) / 2;
	else
		xofs = priv->hadj->value;

	if (scaled_height < height)
		yofs = (height - scaled_height) / 2;
	else
		yofs = priv->vadj->value;

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

	tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, d.x1 - d.x0, d.y1 - d.y0);

	gdk_pixbuf_composite_color (priv->image->pixbuf,
				    tmp,
				    0, 0,
				    d.x1 - d.x0, d.y1 - d.y0,
				    -(d.x0 - xofs), -(d.y0 - yofs),
				    priv->zoom, priv->zoom,
				    GDK_INTERP_BILINEAR,
				    255,
				    d.x0 - xofs, d.y0 - yofs,
				    CHECK_SIZE,
				    CHECK_DARK, CHECK_LIGHT);

	gdk_draw_rgb_image_dithalign (GTK_WIDGET (view)->window,
				      GTK_WIDGET (view)->style->black_gc,
				      d.x0, d.y0,
				      d.x1 - d.x0, d.y1 - d.y0,
				      GDK_RGB_DITHER_NORMAL,
				      gdk_pixbuf_get_pixels (tmp),
				      gdk_pixbuf_get_rowstride (tmp),
				      d.x0 - xofs, d.y0 - yofs);

	gdk_pixbuf_unref (tmp);
}

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

	g_assert (priv->uta != NULL);

	pull_rectangle (priv->uta, &rect, PAINT_RECT_WIDTH, PAINT_RECT_HEIGHT);

	if (art_irect_empty (&rect)) {
		/* This means there is nothing else to repaint, so we remove the
		 * dirty region.
		 */

		art_uta_free (priv->uta);
		priv->uta = NULL;
		priv->idle_id = 0;

		return FALSE;
	}

	paint_rectangle (view, &rect);
	return TRUE;
}

/* Queues a repaint of the specified area in window coordinates */
static void
request_paint_area (ImageView *view, GdkRectangle *area)
{
	ImageViewPrivate *priv;
	int x1, y1, x2, y2;

	priv = view->priv;

	if (!GTK_WIDGET_DRAWABLE (view))
		return;

	x1 = MAX (0, area->x);
	y1 = MAX (0, area->y);
	x2 = MIN (GTK_WIDGET (view)->allocation.width, area->x + area->width);
	y2 = MIN (GTK_WIDGET (view)->allocation.height, area->y + area->height);

	if (x1 >= x2 || y1 >= y2)
		return;

	if (priv->uta) {
		g_assert (priv->idle_id != 0);

		priv->uta = uta_add_rect (priv->uta, x1, y1, x2, y2);
	} else {
		g_assert (priv->idle_id == 0);

		priv->uta = uta_add_rect (NULL, x1, y1, x2, y2);
		priv->idle_id = g_idle_add (paint_iteration_idle, view);
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
			   | GDK_BUTTON_RELEASE_MASK);

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

	if (GTK_WIDGET_CLASS (parent_class)->unmap)
		(* GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
}

/* Size_request handler for the image view */
static void
image_view_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	ImageView *view;
	ImageViewPrivate *priv;
	int scaled_width, scaled_height;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (widget));
	g_return_if_fail (requisition != NULL);

	view = IMAGE_VIEW (widget);
	priv = view->priv;

	compute_scaled_size (view, &scaled_width, &scaled_height);

	requisition->width = scaled_width ? scaled_width : 1;
	requisition->height = scaled_height ? scaled_height : 1;
}

/* Size_allocate handler for the image view */
static void
image_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	ImageView *view;
	ImageViewPrivate *priv;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (widget));
	g_return_if_fail (allocation != NULL);

	view = IMAGE_VIEW (widget);
	priv = view->priv;

	widget->allocation = *allocation;

	if (GTK_WIDGET_REALIZED (widget))
		gdk_window_move_resize (widget->window,
					allocation->x,
					allocation->y,
					allocation->x + allocation->width,
					allocation->y + allocation->height);
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

	request_paint_area (view, area);
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

	request_paint_area (view, &event->area);
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

	/* FIXME */
}

/* Set_scroll_adjustments handler for the image view */
static void
image_view_set_scroll_adjustments (GtkWidget *widget,
				   GtkAdjustment *hadj, GtkAdjustment *vadj)
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

	/* FIXME: resize adjustments, redraw */
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

	priv->zoom = zoom;

	/* FIXME: resize adjustments, redraw */
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
