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
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-i18n.h>
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

/* Maximum size of delayed repaint rectangles */

#define PAINT_RECT_WIDTH 128
#define PAINT_RECT_HEIGHT 128

/* Scroll step increment */

#define SCROLL_STEP_SIZE 32

/* Maximum zoom factor */

#define MAX_ZOOM_FACTOR 20
#define MIN_ZOOM_FACTOR 0.01

/* Private part of the ImageView structure */
struct _ImageViewPrivate {
	/* Pixbuf being displayed */
	GdkPixbuf *pixbuf;

	/* Current zoom factors */
	double zoomx;
	double zoomy;

	/* Previous zoom factor and zoom anchor point stored for size_allocate */
	double old_zoomx;
	double old_zoomy;
	double zoom_x_anchor;
	double zoom_y_anchor;

	/* Adjustments for scrolling */
	GtkAdjustment *hadj;
	GtkAdjustment *vadj;

	/* Current scrolling offsets */
	int xofs, yofs;

	/* Microtile arrays for dirty region.  This represents the dirty region
	 * for interpolated drawing.
	 */
	ArtUta *uta;

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

	/* Whether the image is being dragged */
	guint dragging : 1;

	/* Whether we need to change the zoom factor */
	guint need_zoom_change : 1;
};

/* Signal IDs */
enum {
	ZOOM_FIT,
	ZOOM_CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_INTERP_TYPE,
	PROP_CHECK_TYPE,
	PROP_CHECK_SIZE,
	PROP_DITHER
};

static guint image_view_signals[LAST_SIGNAL];

GNOME_CLASS_BOILERPLATE (ImageView,
			 image_view,
			 GtkWidget,
			 GTK_TYPE_WIDGET);

/* VOID:OBJECT,OBJECT (gtkmarshalers.list:69) */
void
marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
			     GValue       *return_value,
			     guint         n_param_values,
			     const GValue *param_values,
			     gpointer      invocation_hint,
			     gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__OBJECT_OBJECT) (gpointer     data1,
                                                    gpointer     arg_1,
                                                    gpointer     arg_2,
                                                    gpointer     data2);
  register GMarshalFunc_VOID__OBJECT_OBJECT callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__OBJECT_OBJECT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_value_get_object (param_values + 1),
            g_value_get_object (param_values + 2),
            data2);
}

static void
image_view_get_property (GObject    *object,
			 guint       property_id,
			 GValue     *value,
			 GParamSpec *pspec)
{
	ImageView *image_view = IMAGE_VIEW (object);
	ImageViewPrivate *priv = image_view->priv;

	switch (property_id) {
	case PROP_INTERP_TYPE:
		g_value_set_int (value, priv->interp_type);
		break;
	case PROP_CHECK_TYPE:
		g_value_set_int (value, priv->check_type);
		break;
	case PROP_CHECK_SIZE:
		g_value_set_int (value, priv->check_size);
		break;
	case PROP_DITHER:
		g_value_set_int (value, priv->dither);
		break;
	default:
		g_warning ("unknown property id `%d'", property_id);
		break;
	}
}

static void
image_view_set_property (GObject      *object,
			 guint         property_id,
			 const GValue *value,
			 GParamSpec   *pspec)
{
	ImageView *image_view = IMAGE_VIEW (object);

	switch (property_id) {
	case PROP_INTERP_TYPE:
		image_view_set_interp_type (image_view, g_value_get_int (value));
		break;
	case PROP_CHECK_TYPE:
		image_view_set_check_type (image_view, g_value_get_int (value));
		break;
	case PROP_CHECK_SIZE:
		image_view_set_check_size (image_view, g_value_get_int (value));
		break;
	case PROP_DITHER:
		image_view_set_dither (image_view, g_value_get_int (value));
		break;
	default:
		g_warning ("unknown property id `%d'", property_id);
		break;
	}
}

/* Object initialization function for the image view */
static void
image_view_instance_init (ImageView *view)
{
	ImageViewPrivate *priv;

	priv = g_new0 (ImageViewPrivate, 1);
	view->priv = priv;

	GTK_WIDGET_UNSET_FLAGS (view, GTK_NO_WINDOW);
	GTK_WIDGET_SET_FLAGS (view, GTK_CAN_FOCUS);

	priv->pixbuf = NULL;
	priv->zoomx = priv->zoomy = 1.0;
	priv->hadj = NULL;
	priv->vadj = NULL;
	priv->uta = NULL;

	/* Defaults for rendering */
	priv->interp_type = GDK_INTERP_BILINEAR;
	priv->check_type = CHECK_TYPE_MIDTONE;
	priv->check_size = CHECK_SIZE_LARGE;
	priv->dither = GDK_RGB_DITHER_MAX;

	/* We don't want to be double-buffered as we are SuperSmart(tm) */
	gtk_widget_set_double_buffered (GTK_WIDGET (view), FALSE);
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
		priv->uta = NULL;

		g_source_remove (priv->idle_id);
		priv->idle_id = 0;
	} else
		g_assert (priv->idle_id == 0);
}

/* Destroy handler for the image view */
static void
image_view_dispose (GObject *object)
{
	ImageView *view;
	ImageViewPrivate *priv;

	view = IMAGE_VIEW (object);
	priv = view->priv;

	g_signal_handlers_disconnect_matched (
		priv->hadj, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, view);

	g_signal_handlers_disconnect_matched (
		priv->vadj, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, view);

	/* Clean up */
	if (view->priv->pixbuf)
		g_object_unref (G_OBJECT (view->priv->pixbuf));
	view->priv->pixbuf = NULL;

	remove_dirty_region (view);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

/* Finalize handler for the image view */
static void
image_view_finalize (GObject *object)
{
	ImageView *view;
	ImageViewPrivate *priv;

	view = IMAGE_VIEW (object);
	priv = view->priv;

	g_object_unref (priv->hadj);
	g_object_unref (priv->vadj);

	g_free (priv);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}



/* Drawing core */

/* Computes the size in pixels of the scaled image */
static void
compute_scaled_size (ImageView *view, double zoomx, double zoomy, int *width, int *height)
{
	ImageViewPrivate *priv;

	priv = view->priv;

	if (priv->pixbuf) {
		*width = floor (gdk_pixbuf_get_width (priv->pixbuf) * zoomx + 0.5);
		*height = floor (gdk_pixbuf_get_height (priv->pixbuf) * zoomy + 0.5);
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

#define DOUBLE_EQUAL(a,b) (fabs (a - b) < 1e-6)
static gboolean
unity_zoom (ImageViewPrivate *priv)
{
	return (DOUBLE_EQUAL (priv->zoomx, 1.0) &&
		DOUBLE_EQUAL (priv->zoomy, 1.0));
}

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

	compute_scaled_size (view, priv->zoomx, priv->zoomy, &scaled_width, &scaled_height);

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

	if (!priv->pixbuf)
		return;

	r.x0 = xofs;
	r.y0 = yofs;
	r.x1 = xofs + scaled_width;
	r.y1 = yofs + scaled_height;

	art_irect_intersect (&d, &r, rect);
	if (art_irect_empty (&d))
		return;

	/* Short-circuit the fast case to avoid a memcpy() */

	if (unity_zoom (priv)
	    && gdk_pixbuf_get_colorspace (priv->pixbuf) == GDK_COLORSPACE_RGB
	    && !gdk_pixbuf_get_has_alpha (priv->pixbuf)
	    && gdk_pixbuf_get_bits_per_sample (priv->pixbuf) == 8) {
		guchar *pixels;
		int rowstride;

		rowstride = gdk_pixbuf_get_rowstride (priv->pixbuf);

		pixels = (gdk_pixbuf_get_pixels (priv->pixbuf)
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
		g_assert_not_reached ();
		return;
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
		g_assert_not_reached ();
		return;
	}

	/* Draw! */

	gdk_pixbuf_composite_color (priv->pixbuf,
				    tmp,
				    0, 0,
				    d.x1 - d.x0, d.y1 - d.y0,
				    -(d.x0 - xofs), -(d.y0 - yofs),
				    priv->zoomx, priv->zoomy,
				    unity_zoom (priv) ? GDK_INTERP_NEAREST : interp_type,
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

	g_object_unref (tmp);

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

	g_assert (priv->uta != NULL);

	pull_rectangle (priv->uta, &rect, PAINT_RECT_WIDTH, PAINT_RECT_HEIGHT);

	if (art_irect_empty (&rect)) {
		art_uta_free (priv->uta);
		priv->uta = NULL;
	} else
		paint_rectangle (view, &rect, priv->interp_type);

	if (!priv->uta) {
		priv->idle_id = 0;
		return FALSE;
	}

	return TRUE;
}

/* Paints the requested area in non-interpolated mode.  Then, if we are
 * configured to use interpolation, we queue an idle handler to redraw the area
 * with interpolation.  The area is in window coordinates.
 */
static void
request_paint_area (ImageView *view, GdkRectangle *area)
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

	/* Do nearest neighbor or 1:1 zoom synchronously for speed.  */

	if (priv->interp_type == GDK_INTERP_NEAREST || unity_zoom (priv)) {
		paint_rectangle (view, &r, priv->interp_type);
		return;
	}

	/* All other interpolation types are delayed.  */

	if (priv->uta)
		g_assert (priv->idle_id != 0);
	else {
		g_assert (priv->idle_id == 0);
		priv->idle_id = g_idle_add (paint_iteration_idle, view);
	}

	paint_rectangle (view, &r, GDK_INTERP_NEAREST);
	priv->uta = uta_add_rect (priv->uta, r.x0, r.y0, r.x1, r.y1);
}

/* Scrolls the view to the specified offsets.  */
static void
scroll_to (ImageView *view, int x, int y, gboolean change_adjustments)
{
	ImageViewPrivate *priv;
	int xofs, yofs;
	GdkWindow *window;
	int width, height;
	int src_x, src_y;
	int dest_x, dest_y;
	int twidth, theight;

	priv = view->priv;

	/* Check bounds */

	x = CLAMP (x, 0, priv->hadj->upper - priv->hadj->page_size);
	y = CLAMP (y, 0, priv->vadj->upper - priv->vadj->page_size);

	/* Compute offsets */

	xofs = x - priv->xofs;
	yofs = y - priv->yofs;

	if (xofs == 0 && yofs == 0)
		return;

	priv->xofs = x;
	priv->yofs = y;

	if (!GTK_WIDGET_DRAWABLE (view))
		goto out;

	width = GTK_WIDGET (view)->allocation.width;
	height = GTK_WIDGET (view)->allocation.height;

	if (abs (xofs) >= width || abs (yofs) >= height) {
		GdkRectangle area;

		area.x = 0;
		area.y = 0;
		area.width = width;
		area.height = height;

		request_paint_area (view, &area);
		goto out;
	}

	window = GTK_WIDGET (view)->window;

	/* Ensure that the uta has the full size */

	twidth = (width + ART_UTILE_SIZE - 1) >> ART_UTILE_SHIFT;
	theight = (height + ART_UTILE_SIZE - 1) >> ART_UTILE_SHIFT;

	if (priv->uta)
		g_assert (priv->idle_id != 0);
	else
		priv->idle_id = g_idle_add (paint_iteration_idle, view);

	priv->uta = uta_ensure_size (priv->uta, 0, 0, twidth, theight);

	/* Copy the uta area.  Our synchronous handling of expose events, below,
	 * will queue the new scrolled-in areas.
	 */

	src_x = xofs < 0 ? 0 : xofs;
	src_y = yofs < 0 ? 0 : yofs;
	dest_x = xofs < 0 ? -xofs : 0;
	dest_y = yofs < 0 ? -yofs : 0;

	uta_copy_area (priv->uta,
		       src_x, src_y,
		       dest_x, dest_y,
		       width - abs (xofs), height - abs (yofs));

	/* Scroll the window area and process exposure synchronously. */

	gdk_window_scroll (window, -xofs, -yofs);
	gdk_window_process_updates (window, TRUE);

 out:
	if (!change_adjustments)
		return;

	g_signal_handlers_block_matched (
		priv->hadj, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, view);
	g_signal_handlers_block_matched (
		priv->vadj, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, view);

	priv->hadj->value = x;
	priv->vadj->value = y;

	g_signal_emit_by_name (priv->hadj, "value_changed");
	g_signal_emit_by_name (priv->vadj, "value_changed");

	g_signal_handlers_unblock_matched (
		priv->hadj, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, view);
	g_signal_handlers_unblock_matched (
		priv->vadj, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, view);
}

/* Scrolls the image view by the specified offsets.  Notifies the adjustments
 * about their new values.
 */
static void
scroll_by (ImageView *view, int xofs, int yofs)
{
	ImageViewPrivate *priv;

	priv = view->priv;

	scroll_to (view, priv->xofs + xofs, priv->yofs + yofs, TRUE);
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
	attr.colormap = gdk_rgb_get_colormap ();
	attr.event_mask = (gtk_widget_get_events (widget)
			   | GDK_EXPOSURE_MASK
			   | GDK_BUTTON_PRESS_MASK
			   | GDK_BUTTON_RELEASE_MASK
			   | GDK_POINTER_MOTION_MASK
			   | GDK_POINTER_MOTION_HINT_MASK
			   | GDK_SCROLL_MASK
			   | GDK_KEY_PRESS_MASK);

	attr_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attr, attr_mask);
	gdk_window_set_user_data (widget->window, widget);

	cursor = cursor_get (widget, CURSOR_HAND_OPEN);
	gdk_window_set_cursor (widget->window, cursor);
	gdk_cursor_unref (cursor);

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

	compute_scaled_size (view, priv->old_zoomx, priv->old_zoomy,
			     &old_scaled_width, &old_scaled_height);

	if (old_scaled_width < old_width)
		view_cx = (priv->zoom_x_anchor * old_scaled_width) / priv->old_zoomx;
	else
		view_cx = (priv->xofs + priv->zoom_x_anchor * old_width) / priv->old_zoomx;

	if (old_scaled_height < old_height)
		view_cy = (priv->zoom_y_anchor * old_scaled_height) / priv->old_zoomy;
	else
		view_cy = (priv->yofs + priv->zoom_y_anchor * old_height) / priv->old_zoomy;

	compute_scaled_size (view, priv->zoomx, priv->zoomy,
			     &new_scaled_width, &new_scaled_height);

	if (new_scaled_width < new_width)
		*xofs = 0;
	else
		*xofs = floor (view_cx * priv->zoomx - priv->zoom_x_anchor * new_width + 0.5);

	if (new_scaled_height < new_height)
		*yofs = 0;
	else
		*yofs = floor (view_cy * priv->zoomy - priv->zoom_y_anchor * new_height + 0.5);
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

	compute_scaled_size (view, priv->zoomx, priv->zoomy, &scaled_width, &scaled_height);

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

	g_signal_emit_by_name (priv->hadj, "changed");
	g_signal_emit_by_name (priv->vadj, "changed");

	if (priv->hadj->value != xofs) {
		priv->hadj->value = xofs;
		priv->xofs = xofs;

		g_signal_handlers_block_matched (
			priv->hadj, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, view);

		g_signal_emit_by_name (priv->hadj, "value_changed");

		g_signal_handlers_unblock_matched (
			priv->hadj, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, view);
	}

	if (priv->vadj->value != yofs) {
		priv->vadj->value = yofs;
		priv->yofs = yofs;

		g_signal_handlers_block_matched (
			priv->vadj, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, view);

		g_signal_emit_by_name (priv->vadj, "value_changed");

		g_signal_handlers_unblock_matched (
			priv->vadj, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, view);

	}
}

/* Button press event handler for the image view */
static gboolean
image_view_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	ImageView *view;
	ImageViewPrivate *priv;
	GdkCursor *cursor;

	view = IMAGE_VIEW (widget);
	priv = view->priv;

	if (!GTK_WIDGET_HAS_FOCUS (widget))
		gtk_widget_grab_focus (widget);

	if (priv->dragging)
		return FALSE;

	switch (event->button) {
	case 1:
		cursor = cursor_get (widget, CURSOR_HAND_CLOSED);
		gdk_window_set_cursor (widget->window, cursor);
		gdk_cursor_unref (cursor);

		priv->dragging = TRUE;
		priv->drag_anchor_x = event->x;
		priv->drag_anchor_y = event->y;

		priv->drag_ofs_x = priv->xofs;
		priv->drag_ofs_y = priv->yofs;

		return TRUE;

	case 4:
		image_view_set_zoom (view,
				     priv->zoomx * IMAGE_VIEW_ZOOM_MULTIPLIER,
				     priv->zoomy * IMAGE_VIEW_ZOOM_MULTIPLIER,
				     TRUE, event->x, event->y);
		return TRUE;

	case 5:
		image_view_set_zoom (view,
				     priv->zoomx / IMAGE_VIEW_ZOOM_MULTIPLIER,
				     priv->zoomy / IMAGE_VIEW_ZOOM_MULTIPLIER,
				     TRUE, event->x, event->y);
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

	x = priv->drag_ofs_x + dx;
	y = priv->drag_ofs_y + dy;

	scroll_to (view, x, y, TRUE);
}

/* Button release event handler for the image view */
static gboolean
image_view_button_release_event (GtkWidget *widget, GdkEventButton *event)
{
	ImageView *view;
	ImageViewPrivate *priv;
	GdkCursor *cursor;

	view = IMAGE_VIEW (widget);
	priv = view->priv;

	if (!priv->dragging || event->button != 1)
		return FALSE;

	drag_to (view, event->x, event->y);
	priv->dragging = FALSE;

	cursor = cursor_get (widget, CURSOR_HAND_OPEN);
	gdk_window_set_cursor (widget->window, cursor);
	gdk_cursor_unref (cursor);

	return TRUE;
}

/* Scroll event handler for the image view.  We zoom with an event without
 * modifiers rather than scroll; we use the Shift modifier to scroll.
 * Rationale: images are not primarily vertical, and in EOG you scan scroll by
 * dragging the image with button 1 anyways.
 */
static gboolean
image_view_scroll_event (GtkWidget *widget, GdkEventScroll *event)
{
	ImageView *view;
	ImageViewPrivate *priv;
	double zoom_factor;
	int xofs, yofs;

	view = IMAGE_VIEW (widget);
	priv = view->priv;

	/* Compute zoom factor and scrolling offsets; we'll only use either of them */

	xofs = priv->hadj->page_increment / 2; /* same as in gtkscrolledwindow.c */
	yofs = priv->vadj->page_increment / 2;

	switch (event->direction) {
	case GDK_SCROLL_UP:
		zoom_factor = IMAGE_VIEW_ZOOM_MULTIPLIER;
		xofs = 0;
		yofs = -yofs;
		break;

	case GDK_SCROLL_LEFT:
		zoom_factor = 1.0 / IMAGE_VIEW_ZOOM_MULTIPLIER;
		xofs = -xofs;
		yofs = 0;
		break;

	case GDK_SCROLL_DOWN:
		zoom_factor = 1.0 / IMAGE_VIEW_ZOOM_MULTIPLIER;
		xofs = 0;
		yofs = yofs;
		break;

	case GDK_SCROLL_RIGHT:
		zoom_factor = IMAGE_VIEW_ZOOM_MULTIPLIER;
		xofs = xofs;
		yofs = 0;
		break;

	default:
		g_assert_not_reached ();
		return FALSE;
	}

	if ((event->state & GDK_SHIFT_MASK) == 0)
		image_view_set_zoom (view, priv->zoomx * zoom_factor, priv->zoomy * zoom_factor,
				     TRUE, event->x, event->y);
	else
		scroll_by (view, xofs, yofs);

	return TRUE;
}

/* Motion event handler for the image view */
static gboolean
image_view_motion_event (GtkWidget *widget, GdkEventMotion *event)
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

/* Expose event handler for the image view.  First we process the whole dirty
 * region by drawing a non-interpolated version, which is "instantaneous", and
 * we do this synchronously.  Then, if we are set to use interpolation, we queue
 * an idle handler to handle interpolated drawing there.
 */
static gboolean
image_view_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
	ImageView *view;
	GdkRectangle *rects;
	gint n_rects;
	int i;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (IS_IMAGE_VIEW (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	view = IMAGE_VIEW (widget);

	gdk_region_get_rectangles (event->region, &rects, &n_rects);

	for (i = 0; i < n_rects; i++)
		request_paint_area (view, rects + i);

	g_free (rects);

	return TRUE;
}

/* Key press event handler for the image view */
static gboolean
image_view_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
	ImageView *view;
	ImageViewPrivate *priv;
	gboolean do_zoom;
	double zoomx, zoomy;
	gboolean do_scroll;
	int xofs, yofs;

	view = IMAGE_VIEW (widget);
	priv = view->priv;

	do_zoom = FALSE;
	do_scroll = FALSE;
	xofs = yofs = 0;
	zoomx = zoomy = 1.0;

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

	case GDK_plus:
	case GDK_KP_Add:
		do_zoom = TRUE;
		zoomx = priv->zoomx * IMAGE_VIEW_ZOOM_MULTIPLIER;
		zoomy = priv->zoomy * IMAGE_VIEW_ZOOM_MULTIPLIER;
		break;

	case GDK_minus:
	case GDK_KP_Subtract:
		do_zoom = TRUE;
		zoomx = priv->zoomx / IMAGE_VIEW_ZOOM_MULTIPLIER;
		zoomy = priv->zoomy / IMAGE_VIEW_ZOOM_MULTIPLIER;
		break;

	case GDK_1:
		do_zoom = TRUE;
		zoomx = zoomy = 1.0;
		break;

	case GDK_F:
	case GDK_f:
		g_signal_emit (view, image_view_signals [ZOOM_FIT], 0);
		break;

	default:
		return FALSE;
	}

	if (do_zoom) {
		gint x, y;

		gdk_window_get_pointer (widget->window, &x, &y, NULL);
		image_view_set_zoom (view, zoomx, zoomy, TRUE, x, y);
	}

	if (do_scroll)
		scroll_by (view, xofs, yofs);

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

	scroll_to (view, priv->hadj->value, priv->vadj->value, FALSE);
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
		g_signal_handlers_disconnect_matched (
			priv->hadj, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, view);
		g_object_unref (priv->hadj);
	}

	if (priv->vadj && priv->vadj != vadj) {
		g_signal_handlers_disconnect_matched (
			priv->vadj, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, view);
		g_object_unref (priv->vadj);
	}

	need_adjust = FALSE;

	if (priv->hadj != hadj) {
		priv->hadj = hadj;
		g_object_ref (priv->hadj);
		gtk_object_sink (GTK_OBJECT (priv->hadj));

		g_signal_connect (priv->hadj, "value_changed",
				  G_CALLBACK (adjustment_changed_cb),
				  view);

		need_adjust = TRUE;
	}

	if (priv->vadj != vadj) {
		priv->vadj = vadj;
		g_object_ref (priv->vadj);
		gtk_object_sink (GTK_OBJECT (priv->vadj));

		g_signal_connect (priv->vadj, "value_changed",
				  G_CALLBACK (adjustment_changed_cb),
				  view);

		need_adjust = TRUE;
	}

	if (need_adjust)
		adjustment_changed_cb (NULL, view);
}

static gboolean
image_view_focus_in_event (GtkWidget     *widget,
			   GdkEventFocus *event)
{
	return FALSE;
}

static gboolean
image_view_focus_out_event (GtkWidget     *widget,
			    GdkEventFocus *event)
{
	return FALSE;
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
	return g_object_new (TYPE_IMAGE_VIEW, NULL);
}

GdkPixbuf *
image_view_get_pixbuf (ImageView *view)
{
	g_return_val_if_fail (IS_IMAGE_VIEW (view), NULL);

	if (view->priv->pixbuf)
		g_object_ref (view->priv->pixbuf);

	return view->priv->pixbuf;
}

/**
 * image_view_set_pixbuf:
 * @view: An image view.
 * @pixbuf: A pixbuf.
 *
 * Sets the pixbuf that an image view will display.
 **/
void
image_view_set_pixbuf (ImageView *view, GdkPixbuf *pixbuf)
{
	ImageViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (view));

	priv = view->priv;

	if (pixbuf) {
		g_object_ref (pixbuf);
		if (view->priv->pixbuf)
			g_object_unref (view->priv->pixbuf);
	}

	view->priv->pixbuf = pixbuf;

	remove_dirty_region (view);
	scroll_to (view, 0, 0, TRUE);

	gtk_widget_queue_draw (GTK_WIDGET (view));
}

/**
 * image_view_set_zoom:
 * @view: An image view.
 * @zoomx: Horizontal zoom factor.
 * @zoomy: Vertical zoom factor.
 * @have_anchor: Whether the anchor point specified by (@anchorx, @anchory)
 * should be used.
 * @anchorx: Horizontal anchor point in pixels.
 * @anchory: Vertical anchor point in pixels.
 *
 * Sets the zoom factor for an image view.  The anchor point can be used to
 * specify the point that stays fixed when the image is zoomed.  If @have_anchor
 * is %TRUE, then (@anchorx, @anchory) specify the point relative to the image
 * view widget's allocation that will stay fixed when zooming.  If @have_anchor
 * is %FALSE, then the center point of the image view will be used.
 **/
void
image_view_set_zoom (ImageView *view, double zoomx, double zoomy,
		     gboolean have_anchor, int anchorx, int anchory)
{
	ImageViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (view));
	g_return_if_fail (zoomx > 0.0);
	g_return_if_fail (zoomy > 0.0);

	priv = view->priv;

	if (zoomx > MAX_ZOOM_FACTOR)
		zoomx = MAX_ZOOM_FACTOR;
	else if (zoomx < MIN_ZOOM_FACTOR)
		zoomx = MIN_ZOOM_FACTOR;
	if (zoomy > MAX_ZOOM_FACTOR)
		zoomy = MAX_ZOOM_FACTOR;
	else if (zoomy < MIN_ZOOM_FACTOR)
		zoomy = MIN_ZOOM_FACTOR;

	if (DOUBLE_EQUAL (priv->zoomx, zoomx) &&
	    DOUBLE_EQUAL (priv->zoomy, zoomy))
		goto out;

	if (!priv->need_zoom_change) {
		priv->old_zoomx = priv->zoomx;
		priv->old_zoomy = priv->zoomy;
		priv->need_zoom_change = TRUE;
	}

	priv->zoomx = zoomx;
	priv->zoomy = zoomy;

	g_signal_emit (view, image_view_signals [ZOOM_CHANGED], 0);

	if (have_anchor) {
		anchorx = CLAMP (anchorx, 0, GTK_WIDGET (view)->allocation.width);
		anchory = CLAMP (anchory, 0, GTK_WIDGET (view)->allocation.height);
		set_zoom_anchor (view, anchorx, anchory);
	} else
		set_default_zoom_anchor (view);

 out:

	gtk_widget_queue_resize (GTK_WIDGET (view));
}

/**
 * image_view_get_zoom:
 * @view: An image view.
 * @zoomx: If non-NULL, the horizontal zoom factor is returned here.
 * @zoomy: If non-NULL, the vertical zoom factor is returned here.
 *
 * Queries the zoom factor of an image view.
 **/
void
image_view_get_zoom (ImageView *view, double *zoomx, double *zoomy)
{
	ImageViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (view));

	priv = view->priv;

	if (zoomx)
		*zoomx = priv->zoomx;

	if (zoomy)
		*zoomy = priv->zoomy;
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
	gtk_widget_queue_draw (GTK_WIDGET (view));
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

	gtk_widget_queue_draw (GTK_WIDGET (view));
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

	gtk_widget_queue_draw (GTK_WIDGET (view));
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

	gtk_widget_queue_draw (GTK_WIDGET (view));
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
 * image_view_get_scaled_size
 * @view: An image view.
 * @width: Image width result.
 * @height: Image height result.
 *
 * Returns the size of the image after applying the zoom factor.
 *
 * Return value: Image size according to zoom factor.
 **/
void
image_view_get_scaled_size (ImageView *view, gint *width, gint *height)
{
	ImageViewPrivate *priv;

	*width = *height = 0;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (view));

	priv = view->priv;

	compute_scaled_size (view, priv->zoomx, priv->zoomy, width, height);
}


/* Class initialization function for the image view */
static void
image_view_class_init (ImageViewClass *class)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	gobject_class = (GObjectClass *) class;
	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	gobject_class->set_property = image_view_set_property;
	gobject_class->get_property = image_view_get_property;

	/* FIXME: we should use the enum types but that's a pain */
	g_object_class_install_property (
		gobject_class,
		PROP_INTERP_TYPE,
		g_param_spec_int ("interp_type",
				  _("interpolation type"),
				  _("the type of interpolation to use"),
				  0, G_MAXINT, 0, 0));
	g_object_class_install_property (
		gobject_class,
		PROP_CHECK_TYPE,
		g_param_spec_int ("check_type",
				  _("check type"),
				  _("the type of chequering to use"),
				  0, G_MAXINT, 0, 0));
	g_object_class_install_property (
		gobject_class,
		PROP_CHECK_SIZE,
		g_param_spec_int ("check_size",
				  _("check type"),
				  _("the size of chequers to use"),
				  0, G_MAXINT, 0, 0));
	g_object_class_install_property (
		gobject_class,
		PROP_DITHER,
		g_param_spec_int ("dither",
				  _("dither"),
				  _("dither type"),
				  0, G_MAXINT, 0, 0));
  	image_view_signals[ZOOM_FIT] =
 		g_signal_new ("zoom_fit",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (ImageViewClass, zoom_fit),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
  	image_view_signals[ZOOM_CHANGED] =
 		g_signal_new ("zoom_changed",
			      G_TYPE_FROM_CLASS(object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (ImageViewClass, zoom_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	gobject_class->dispose = image_view_dispose;
  	gobject_class->finalize = image_view_finalize;

  	class->set_scroll_adjustments = image_view_set_scroll_adjustments;
  	widget_class->set_scroll_adjustments_signal =
 		g_signal_new ("set_scroll_adjustments",
			      G_TYPE_FROM_CLASS(object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ImageViewClass, set_scroll_adjustments),
			      NULL,
			      NULL,
			      marshal_VOID__OBJECT_OBJECT,
			      G_TYPE_NONE,
			      2,
			      GTK_TYPE_ADJUSTMENT,
			      GTK_TYPE_ADJUSTMENT);

	widget_class->unmap = image_view_unmap;
	widget_class->realize = image_view_realize;
	widget_class->unrealize = image_view_unrealize;
	widget_class->size_request = image_view_size_request;
	widget_class->size_allocate = image_view_size_allocate;
	widget_class->button_press_event = image_view_button_press_event;
	widget_class->button_release_event = image_view_button_release_event;
	widget_class->scroll_event = image_view_scroll_event;
	widget_class->motion_notify_event = image_view_motion_event;
	widget_class->expose_event = image_view_expose_event;
	widget_class->key_press_event = image_view_key_press_event;
	widget_class->focus_in_event  = image_view_focus_in_event;
	widget_class->focus_out_event = image_view_focus_out_event;
}
