#include <math.h>
#include <libgnome/gnome-macros.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>

#include "libeog-marshal.h"
#include "eog-scroll-view.h"
#include "cursors.h"
#include "uta.h"
#include "zoom.h"

/* Maximum size of delayed repaint rectangles */
#define PAINT_RECT_WIDTH 128
#define PAINT_RECT_HEIGHT 128

/* Scroll step increment */
#define SCROLL_STEP_SIZE 32

/* Maximum zoom factor */
#define MAX_ZOOM_FACTOR 20
#define MIN_ZOOM_FACTOR 0.01

#define CHECK_MEDIUM 8
#define CHECK_BLACK 0x00000000
#define CHECK_DARK 0x00555555
#define CHECK_GRAY 0x00808080
#define CHECK_LIGHT 0x00aaaaaa
#define CHECK_WHITE 0x00ffffff

/* Default increment for zooming.  The current zoom factor is multiplied or
 * divided by this amount on every zooming step.  For consistency, you should
 * use the same value elsewhere in the program.
 */
#define IMAGE_VIEW_ZOOM_MULTIPLIER 1.05

#define NO_DEBUG

enum {
	MODUS_ZOOM_FIT,
	MODUS_ZOOM_FREE
};

enum {
	PROGRESSIVE_NONE,
	PROGRESSIVE_LOADING,
	PROGRESSIVE_POLISHING,
};

enum {
	SIGNAL_ZOOM_CHANGED,
	SIGNAL_LAST
};
static gint view_signals [SIGNAL_LAST];

struct _EogScrollViewPrivate {
	/* some widgets we relay on */
	GtkWidget *display;
	GtkAdjustment *hadj;
	GtkAdjustment *vadj;
	GtkWidget *hbar;
	GtkWidget *vbar;

	/* actual image */
	EogImage *image;
	gulong image_cb_ids [5];
	GdkPixbuf *pixbuf;

	/* zoom modus, either ZOOM_FIT or ZOOM_FREE */
	int modus;

	/* wether to allow zoom > 1.0 on zoom fit */
	gboolean upscale;

	/* the actual zoom factor */
	double zoom;

	/* Current scrolling offsets */
	int xofs, yofs;
	
	/* Microtile arrays for dirty region.  This represents the dirty region
	 * for interpolated drawing.
	 */
	ArtUta *uta;

	/* handler ID for paint idle callback */
	guint idle_id;

	/* Interpolation type */
	GdkInterpType interp_type;

	/* dragging stuff */
	int drag_anchor_x, drag_anchor_y;
	int drag_ofs_x, drag_ofs_y;
	guint dragging : 1;

	/* status of progressive loading */
	int progressive_loading;

	/* how to indicate transparency in images */
	TransparencyStyle transp_style;
	guint32 transp_color;
};

static void scroll_by (EogScrollView *view, int xofs, int yofs);
static void set_zoom_fit (EogScrollView *view);
static void request_paint_area (EogScrollView *view, GdkRectangle *area);

GNOME_CLASS_BOILERPLATE (EogScrollView,
			 eog_scroll_view,
			 GtkTable,
			 GTK_TYPE_TABLE);


/*===================================
    widget size changing handler &
        util functions 
  ---------------------------------*/

/* Computes the size in pixels of the scaled image */
static void
compute_scaled_size (EogScrollView *view, double zoom, int *width, int *height)
{
	EogScrollViewPrivate *priv;

	priv = view->priv;

	if (priv->pixbuf) {
		*width = floor (gdk_pixbuf_get_width (priv->pixbuf) * zoom + 0.5);
		*height = floor (gdk_pixbuf_get_height (priv->pixbuf) * zoom + 0.5);
	} else
		*width = *height = 0;
}

/* Computes the offsets for the new zoom value so that they keep the image
 * centered on the view.
 */
static void
compute_center_zoom_offsets (EogScrollView *view,
			     double old_zoom, double new_zoom,
			     int width, int height,
			     double zoom_x_anchor, double zoom_y_anchor,
			     int *xofs, int *yofs)
{
	EogScrollViewPrivate *priv;
	int old_scaled_width, old_scaled_height;
	int new_scaled_width, new_scaled_height;
	double view_cx, view_cy;

	priv = view->priv;

	compute_scaled_size (view, old_zoom,
			     &old_scaled_width, &old_scaled_height);

	if (old_scaled_width < width)
		view_cx = (zoom_x_anchor * old_scaled_width) / old_zoom;
	else
		view_cx = (priv->xofs + zoom_x_anchor * width) / old_zoom;

	if (old_scaled_height < height)
		view_cy = (zoom_y_anchor * old_scaled_height) / old_zoom;
	else
		view_cy = (priv->yofs + zoom_y_anchor * height) / old_zoom;

	compute_scaled_size (view, new_zoom,
			     &new_scaled_width, &new_scaled_height);

	if (new_scaled_width < width)
		*xofs = 0;
	else
		*xofs = floor (view_cx * new_zoom - zoom_x_anchor * width + 0.5);

	if (new_scaled_height < height)
		*yofs = 0;
	else
		*yofs = floor (view_cy * new_zoom - zoom_y_anchor * height + 0.5);
}

static void
update_scrollbar_values (EogScrollView *view)
{
	EogScrollViewPrivate *priv;
	int scaled_width, scaled_height;
	int img_x, img_y;
	int xofs, yofs;
	GtkAllocation *allocation;

	priv = view->priv;

	if (!GTK_WIDGET_VISIBLE (GTK_WIDGET (priv->hbar)) && !GTK_WIDGET_VISIBLE (GTK_WIDGET (priv->vbar))) 
		return;

	g_print ("update scrollbar values\n");

	compute_scaled_size (view, priv->zoom, &scaled_width, &scaled_height);
	allocation = &GTK_WIDGET (priv->display)->allocation;

	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (priv->hbar))) {
		/* Set scroll increments */
		priv->hadj->page_size = MIN (scaled_width, allocation->width);
		priv->hadj->page_increment = allocation->width / 2;
		priv->hadj->step_increment = SCROLL_STEP_SIZE;

		/* Set scroll bounds and new offsets */
		priv->hadj->lower = 0;
		priv->hadj->upper = scaled_width;
		xofs = CLAMP (priv->xofs, 0, priv->hadj->upper - priv->hadj->page_size);
		if (priv->hadj->value != xofs) {
			priv->hadj->value = xofs;
			priv->xofs = xofs;

			g_signal_handlers_block_matched (
				priv->hadj, G_SIGNAL_MATCH_DATA,
				0, 0, NULL, NULL, view);
			
			g_signal_emit_by_name (priv->hadj, "changed");
			
			g_signal_handlers_unblock_matched (
				priv->hadj, G_SIGNAL_MATCH_DATA,
				0, 0, NULL, NULL, view);
		}
	}

	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (priv->vbar))) {
		priv->vadj->page_size = MIN (scaled_height, allocation->height);
		priv->vadj->page_increment = allocation->height / 2;
		priv->vadj->step_increment = SCROLL_STEP_SIZE;

		priv->vadj->lower = 0;
		priv->vadj->upper = scaled_height;
		yofs = CLAMP (priv->yofs, 0, priv->vadj->upper - priv->vadj->page_size);
		
		if (priv->vadj->value != yofs) {
			priv->vadj->value = yofs;
			priv->yofs = yofs;
			
			g_signal_handlers_block_matched (
				priv->vadj, G_SIGNAL_MATCH_DATA,
				0, 0, NULL, NULL, view);
			
			g_signal_emit_by_name (priv->vadj, "changed");
			
			g_signal_handlers_unblock_matched (
				priv->vadj, G_SIGNAL_MATCH_DATA,
				0, 0, NULL, NULL, view);
		}
	}
}


static gboolean
check_scrollbar_visibility (EogScrollView *view, GtkAllocation *alloc)
{
	EogScrollViewPrivate *priv;
	int bar_height;
	int bar_width;
	int img_width;
	int img_height;
	GtkRequisition req;
	int width, height;
	gboolean changed = FALSE;
	gboolean hbar_visible, vbar_visible;
	
	priv = view->priv;

	if (alloc != 0) {
		width = alloc->width;
		height = alloc->height;
	}
	else {
		width = GTK_WIDGET (view)->allocation.width;
		height = GTK_WIDGET (view)->allocation.height;
	}

	compute_scaled_size (view, priv->zoom, &img_width, &img_height);

	/* this should work fairly well in this special case 
	   for scrollbars */
	gtk_widget_size_request (priv->hbar, &req);
	bar_height = req.height;
	gtk_widget_size_request (priv->vbar, &req);
	bar_width = req.width;
#ifdef DEBUG
	g_print ("Widget Size allocate: %i, %i   bar: %i, %i\n", width, height, bar_width, bar_height);
#endif

	hbar_visible = vbar_visible = FALSE;
	if (priv->modus == MODUS_ZOOM_FIT) {
		hbar_visible = vbar_visible = FALSE;
	}
	else if (img_width <= width && img_height <= height) {
		hbar_visible = FALSE;
		vbar_visible = FALSE;
	}
	else if (img_width > width && img_height > height) {
		hbar_visible = vbar_visible = TRUE;
	}
	else if (img_width > width) {
		hbar_visible = TRUE;
		if (img_height <= (height - bar_height))
			vbar_visible = FALSE;
		else
			vbar_visible = TRUE;
	}
        else if (img_height > height) {
		vbar_visible = TRUE;
		if (img_width <= (width - bar_width))
			hbar_visible = FALSE;
		else
			hbar_visible = TRUE;
	}
	
	if (hbar_visible != GTK_WIDGET_VISIBLE (GTK_WIDGET (priv->hbar))) {
		g_object_set (G_OBJECT (priv->hbar), "visible", hbar_visible, NULL);
		changed = TRUE;
	}
	
	if (vbar_visible != GTK_WIDGET_VISIBLE (GTK_WIDGET (priv->vbar))) {
		g_object_set (G_OBJECT (priv->vbar), "visible", vbar_visible, NULL);
		changed = TRUE;
	}

	return changed;
}

#define DOUBLE_EQUAL(a,b) (fabs (a - b) < 1e-6)
static gboolean
is_unity_zoom (EogScrollViewPrivate *priv)
{
	return DOUBLE_EQUAL (priv->zoom, 1.0);
}

static void
get_image_offsets (EogScrollView *view, int *xofs, int *yofs)
{
	EogScrollViewPrivate *priv;
	int scaled_width, scaled_height;
	int width, height;

	priv = view->priv;

	compute_scaled_size (view, priv->zoom, &scaled_width, &scaled_height);

	width = GTK_WIDGET (priv->display)->allocation.width;
	height = GTK_WIDGET (priv->display)->allocation.height;

	/* Compute image offsets with respect to the window */
	if (scaled_width <= width)
		*xofs = (width - scaled_width) / 2;
	else
		*xofs = -priv->xofs;

	if (scaled_height <= height)
		*yofs = (height - scaled_height) / 2;
	else
		*yofs = -priv->yofs;
}


/*===================================
          drawing core
  ---------------------------------*/


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
paint_background (EogScrollView *view, ArtIRect *r, ArtIRect *rect)
{
	EogScrollViewPrivate *priv;
	ArtIRect d;

	priv = view->priv;

	art_irect_intersect (&d, r, rect);
	if (!art_irect_empty (&d))
		gdk_draw_rectangle (GTK_WIDGET (priv->display)->window,
				    GTK_WIDGET (priv->display)->style->bg_gc[GTK_STATE_NORMAL],
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
paint_rectangle (EogScrollView *view, ArtIRect *rect, GdkInterpType interp_type)
{
	EogScrollViewPrivate *priv;
	int scaled_width, scaled_height;
	int width, height;
	int xofs, yofs;
	ArtIRect r, d;
	GdkPixbuf *tmp;
	int check_size;
	guint32 check_1, check_2;

	priv = view->priv;

	compute_scaled_size (view, priv->zoom, &scaled_width, &scaled_height);

	width = GTK_WIDGET (priv->display)->allocation.width;
	height = GTK_WIDGET (priv->display)->allocation.height;

	/* Compute image offsets with respect to the window */

	if (scaled_width <= width)
		xofs = (width - scaled_width) / 2;
	else
		xofs = -priv->xofs;

	if (scaled_height <= height)
		yofs = (height - scaled_height) / 2;
	else
		yofs = -priv->yofs;

#ifdef DEBUG
	g_print ("paint_rectangle: zoom %.2f, xofs: %i, yofs: %i scaled w: %i h: %i\n", priv->zoom, xofs, yofs, scaled_width, scaled_height);
#endif
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

#ifdef R_DEBUG
 {
	 char *str;
	 switch (interp_type) {
	 case GDK_INTERP_NEAREST:
		 str = "NEAREST";
		 break;
	 default:
		 str = "ALIASED";
	 }
	g_print ("%s: x0: %i,\t y0: %i,\t x1: %i,\t y1: %i\n", str, d.x0, d.y0, d.x1, d.y1); 
 }
#endif

	/* Short-circuit the fast case to avoid a memcpy() */

	if (is_unity_zoom (priv)
	    && gdk_pixbuf_get_colorspace (priv->pixbuf) == GDK_COLORSPACE_RGB
	    && !gdk_pixbuf_get_has_alpha (priv->pixbuf)
	    && gdk_pixbuf_get_bits_per_sample (priv->pixbuf) == 8) {
		guchar *pixels;
		int rowstride;

		rowstride = gdk_pixbuf_get_rowstride (priv->pixbuf);

		pixels = (gdk_pixbuf_get_pixels (priv->pixbuf)
			  + (d.y0 - yofs) * rowstride
			  + 3 * (d.x0 - xofs));

		gdk_draw_rgb_image_dithalign (GTK_WIDGET (priv->display)->window,
					      GTK_WIDGET (priv->display)->style->black_gc,
					      d.x0, d.y0,
					      d.x1 - d.x0, d.y1 - d.y0,
					      GDK_RGB_DITHER_MAX /* FIXME: priv->dither */,
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

	/* Compute transparency  parameters */
	switch (priv->transp_style) {
	case TRANSP_BACKGROUND:
	        {
			GdkColor *color = &GTK_WIDGET (priv->display)->style->bg[GTK_STATE_NORMAL];
			guint32 red_part = (color->red >> 8) << 16;
			guint32 green_part = (color->green >> 8) << 8;
			guint32 blue_part = (color->blue >> 8);

			check_1 = check_2 = red_part + green_part + blue_part;
		}
		break;

	case TRANSP_CHECKEDPATTERN:
		check_1 = CHECK_DARK;
		check_2 = CHECK_LIGHT;
		break;

	case TRANSP_COLOR:
		check_1 = check_2 = priv->transp_color;
		break;
	};
	check_size = CHECK_MEDIUM;

	/* Draw! */
	gdk_pixbuf_composite_color (priv->pixbuf,
				    tmp,
				    0, 0,
				    d.x1 - d.x0, d.y1 - d.y0,
				    -(d.x0 - xofs), -(d.y0 - yofs),
				    priv->zoom, priv->zoom,
				    is_unity_zoom (priv) ? GDK_INTERP_NEAREST : interp_type,
				    255,
				    d.x0 - xofs, d.y0 - yofs,
				    check_size,
				    check_1, check_2);

#ifdef PACK_RGBA
	pack_pixbuf (tmp);
#endif

	gdk_draw_rgb_image_dithalign (GTK_WIDGET (priv->display)->window,
				      GTK_WIDGET (priv->display)->style->black_gc,
				      d.x0, d.y0,
				      d.x1 - d.x0, d.y1 - d.y0,
				      GDK_RGB_DITHER_MAX /* FIXME: priv->dither */,
				      gdk_pixbuf_get_pixels (tmp),
				      gdk_pixbuf_get_rowstride (tmp),
				      d.x0 - xofs, d.y0 - yofs);

	g_object_unref (tmp);
}


/* Idle handler for the drawing process.  We pull a rectangle from the dirty
 * region microtile array, paint it, and leave the rest to the next idle
 * iteration.
 */
static gboolean
paint_iteration_idle (gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;
	ArtIRect rect;

	view = EOG_SCROLL_VIEW (data);
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
request_paint_area (EogScrollView *view, GdkRectangle *area)
{
	EogScrollViewPrivate *priv;
	ArtIRect r;

	priv = view->priv;

#ifdef R_DEBUG
	g_print ("request_paint area: ...  x: %i, y: %i, width: %i, height: %i\n", area->x, area->y, area->width, area->height);
#endif

	if (!GTK_WIDGET_DRAWABLE (priv->display))
		return;

	r.x0 = MAX (0, area->x);
	r.y0 = MAX (0, area->y);
	r.x1 = MIN (GTK_WIDGET (priv->display)->allocation.width, area->x + area->width);
	r.y1 = MIN (GTK_WIDGET (priv->display)->allocation.height, area->y + area->height);

#ifdef DEBUG
	g_print ("request_paint r: %i, %i, %i, %i\n", r.x0, r.y0, r.x1, r.y1);
#endif

	if (r.x0 >= r.x1 || r.y0 >= r.y1)
		return;

	/* Do nearest neighbor, 1:1 zoom or active progressive loading synchronously for speed.  */
	if (priv->interp_type == GDK_INTERP_NEAREST || 
	    is_unity_zoom (priv) || 
	    priv->progressive_loading == PROGRESSIVE_LOADING) 
	{
		paint_rectangle (view, &r, GDK_INTERP_NEAREST);
		return;
	}

	if (priv->progressive_loading == PROGRESSIVE_POLISHING) {
		/* We have already a complete image with nearest neighbor mode. 
		 * It's sufficient to add only a antitaliased idle update
		 */
		priv->progressive_loading = PROGRESSIVE_NONE;
	}
	else {
		/* do nearest neigbor before anti aliased version */
		paint_rectangle (view, &r, GDK_INTERP_NEAREST);
	}

	/* All other interpolation types are delayed.  */
	if (priv->uta)
		g_assert (priv->idle_id != 0);
	else {
		g_assert (priv->idle_id == 0);
		priv->idle_id = g_idle_add (paint_iteration_idle, view);
	}

	priv->uta = uta_add_rect (priv->uta, r.x0, r.y0, r.x1, r.y1);
}


/* =======================================

    scrolling stuff 

    --------------------------------------*/



/* Scrolls the view to the specified offsets.  */
static void
scroll_to (EogScrollView *view, int x, int y, gboolean change_adjustments)
{
	EogScrollViewPrivate *priv;
	int xofs, yofs;
	GdkWindow *window;
	int width, height;
	int src_x, src_y;
	int dest_x, dest_y;
	int twidth, theight;

	priv = view->priv;

	/* Check bounds & Compute offsets */
	if (GTK_WIDGET_VISIBLE (priv->hbar)) {
		x = CLAMP (x, 0, priv->hadj->upper - priv->hadj->page_size);
		xofs = x - priv->xofs;
	}
	else {
		xofs = 0;
	}
	
	if (GTK_WIDGET_VISIBLE (priv->vbar)) {
		y = CLAMP (y, 0, priv->vadj->upper - priv->vadj->page_size);
		yofs = y - priv->yofs;
	}
	else {
		yofs = 0;
	}

	if (xofs == 0 && yofs == 0)
		return;

	priv->xofs = x;
	priv->yofs = y;

	if (!GTK_WIDGET_DRAWABLE (priv->display))
		goto out;

	width = GTK_WIDGET (priv->display)->allocation.width;
	height = GTK_WIDGET (priv->display)->allocation.height;

	if (abs (xofs) >= width || abs (yofs) >= height) {
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));
		goto out;
	}

	window = GTK_WIDGET (priv->display)->window;

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
scroll_by (EogScrollView *view, int xofs, int yofs)
{
	EogScrollViewPrivate *priv;

	priv = view->priv;

	scroll_to (view, priv->xofs + xofs, priv->yofs + yofs, TRUE);
}


/* Callback used when an adjustment is changed */
static void
adjustment_changed_cb (GtkAdjustment *adj, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;

	scroll_to (view, priv->hadj->value, priv->vadj->value, FALSE);
}


/* Drags the image to the specified position */
static void
drag_to (EogScrollView *view, int x, int y)
{
	EogScrollViewPrivate *priv;
	int dx, dy;

	priv = view->priv;

	dx = priv->drag_anchor_x - x;
	dy = priv->drag_anchor_y - y;

	x = priv->drag_ofs_x + dx;
	y = priv->drag_ofs_y + dy;

	scroll_to (view, x, y, TRUE);
}


/**
 * set_zoom:
 * @view: A scroll view.
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
set_zoom (EogScrollView *view, double zoom,
	  gboolean have_anchor, int anchorx, int anchory)
{
	EogScrollViewPrivate *priv;
	GdkRectangle rect;

	g_return_if_fail (view != NULL);
	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));
	g_return_if_fail (zoom > 0.0);

	priv = view->priv;

	if (priv->pixbuf == NULL) return;

	if (zoom > MAX_ZOOM_FACTOR)
		zoom = MAX_ZOOM_FACTOR;
	else if (zoom < MIN_ZOOM_FACTOR)
		zoom = MIN_ZOOM_FACTOR;

	if (!DOUBLE_EQUAL (priv->zoom, zoom)) {
		int xofs, yofs; 
		int disp_width, disp_height;
		double x_rel, y_rel;

		priv->modus = MODUS_ZOOM_FREE;

		disp_width = GTK_WIDGET (priv->display)->allocation.width;
		disp_height = GTK_WIDGET (priv->display)->allocation.height;

		/* compute new xofs/yofs values */
		if (have_anchor) {
			x_rel = (double) anchorx / disp_width;
			y_rel = (double) anchory / disp_height;
		}
		else {
			x_rel = 0.5;
			y_rel = 0.5;
		}

		compute_center_zoom_offsets (view, priv->zoom, zoom,
					     disp_width, disp_height,
					     x_rel, y_rel,
					     &xofs, &yofs);
		
		/* set new values */
		priv->xofs = xofs; // (img_width * x_rel * zoom) - anchorx;
		priv->yofs = yofs; // (img_height * y_rel * zoom) - anchory;
#if 0
		g_print ("xofs: %i  yofs: %i\n", priv->xofs, priv->yofs);
#endif
		priv->zoom = zoom;

		/* we make use of the new values here */
		check_scrollbar_visibility (view, 0);
		update_scrollbar_values (view);

		/* repaint the whole image */
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));

		g_signal_emit (view, view_signals [SIGNAL_ZOOM_CHANGED], 0, priv->zoom);
	}
}




/* Key press event handler for the image view */
static gboolean
display_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;
	gboolean handled;
	gboolean do_zoom;
	double zoom;
	gboolean do_scroll;
	int xofs, yofs;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;

	handled = FALSE;

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
	case GDK_plus:
	case GDK_KP_Add:
		do_zoom = TRUE;
		zoom = priv->zoom * IMAGE_VIEW_ZOOM_MULTIPLIER;
		break;

	case GDK_minus:
	case GDK_KP_Subtract:
		do_zoom = TRUE;
		zoom = priv->zoom / IMAGE_VIEW_ZOOM_MULTIPLIER;
		break;

	case GDK_1:
		do_zoom = TRUE;
		zoom = 1.0;
		break;

	case GDK_F:
	case GDK_f:
		set_zoom_fit (view);
		check_scrollbar_visibility (view, 0);
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));
		break;

	default:
		return FALSE;
	}

	if (do_zoom) {
		gint x, y;

		gdk_window_get_pointer (widget->window, &x, &y, NULL);
		set_zoom (view, zoom, TRUE, x, y);
	}

	if (do_scroll)
		scroll_by (view, xofs, yofs);

	return TRUE;
}


static void
set_zoom_fit (EogScrollView *view)
{
	EogScrollViewPrivate *priv;
	double new_zoom;
	int scaled_width, scaled_height;
	int width, height;
	
	priv = view->priv;

	priv->modus = MODUS_ZOOM_FIT;

	if (!GTK_WIDGET_MAPPED (GTK_WIDGET (view)))
		return;

	if (priv->pixbuf == NULL)
		return;

	width = GTK_WIDGET (priv->display)->allocation.width;
	height = GTK_WIDGET (priv->display)->allocation.height;

	new_zoom = zoom_fit_scale (width, height, 
				   gdk_pixbuf_get_width (priv->pixbuf),
				   gdk_pixbuf_get_height (priv->pixbuf), 
				   priv->upscale);

	if (DOUBLE_EQUAL (new_zoom, priv->zoom)) {
		return;
	}

	priv->zoom = new_zoom;
	g_signal_emit (view, view_signals [SIGNAL_ZOOM_CHANGED], 0, priv->zoom);

	priv->xofs = 0;
	priv->yofs = 0;
}


/* Button press event handler for the image view */
static gboolean
eog_scroll_view_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;
	GdkCursor *cursor;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;

	if (!GTK_WIDGET_HAS_FOCUS (priv->display))
		gtk_widget_grab_focus (GTK_WIDGET (priv->display));

	if (priv->dragging)
		return FALSE;

	switch (event->button) {
	case 1:
		cursor = cursor_get (GTK_WIDGET (priv->display), CURSOR_HAND_CLOSED);
		gdk_window_set_cursor (GTK_WIDGET (priv->display)->window, cursor);
		gdk_cursor_unref (cursor);

		priv->dragging = TRUE;
		priv->drag_anchor_x = event->x;
		priv->drag_anchor_y = event->y;

		priv->drag_ofs_x = priv->xofs;
		priv->drag_ofs_y = priv->yofs;

		return TRUE;
	default:
		break;
	}

	return FALSE;
}


/*===================================
 
   internal signal callbacks

  ---------------------------------*/


/* Button release event handler for the image view */
static gboolean
eog_scroll_view_button_release_event (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;
	GdkCursor *cursor;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;

	if (!priv->dragging || event->button != 1)
		return FALSE;

	drag_to (view, event->x, event->y);
	priv->dragging = FALSE;

	cursor = cursor_get (GTK_WIDGET (priv->display), CURSOR_HAND_OPEN);
	gdk_window_set_cursor (GTK_WIDGET (priv->display)->window, cursor);
	gdk_cursor_unref (cursor);

	return TRUE;
}

/* Scroll event handler for the image view.  We zoom with an event without
 * modifiers rather than scroll; we use the Shift modifier to scroll.
 * Rationale: images are not primarily vertical, and in EOG you scan scroll by
 * dragging the image with button 1 anyways.
 */
static gboolean
eog_scroll_view_scroll_event (GtkWidget *widget, GdkEventScroll *event, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;
	double zoom_factor;
	int xofs, yofs;

	view = EOG_SCROLL_VIEW (data);
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
		set_zoom (view, priv->zoom * zoom_factor,
			  TRUE, event->x, event->y);
	else if ((event->state & GDK_CONTROL_MASK) == 0)
		scroll_by (view, xofs, yofs);
	else
		scroll_by (view, yofs, xofs);

	return TRUE;
}

/* Motion event handler for the image view */
static gboolean
eog_scroll_view_motion_event (GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;
	gint x, y;
	GdkModifierType mods;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;

	if (!priv->dragging)
		return FALSE;

	if (event->is_hint)
		gdk_window_get_pointer (GTK_WIDGET (priv->display)->window, &x, &y, &mods);
	else {
		x = event->x;
		y = event->y;
	}

	drag_to (view, x, y);
	return TRUE;
}



static void
display_map_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;

	g_print ("display_map_event ...\n");

	set_zoom_fit (view);
	check_scrollbar_visibility (view, 0);
	gtk_widget_queue_draw (GTK_WIDGET (priv->display));
}

static void
display_map_cb (GtkWidget *widget, gpointer data)
{
	g_print ("display_map cb ...\n");
}


static void
eog_scroll_view_size_allocate (GtkWidget *widget, GtkAllocation *alloc)
{
	EogScrollView *view;

	view = EOG_SCROLL_VIEW (widget);
	check_scrollbar_visibility (view, alloc);

	GNOME_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate, (widget, alloc)); 
}

static void
display_size_change (GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;

	if (priv->modus == MODUS_ZOOM_FIT) {
		GtkAllocation alloc;
		alloc.width = event->width;
		alloc.height = event->height;
		set_zoom_fit (view);
		check_scrollbar_visibility (view, &alloc);
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));
	}
	else {
		int scaled_width, scaled_height;
		int x_offset = 0;
		int y_offset = 0;

		compute_scaled_size (view, priv->zoom, &scaled_width, &scaled_height);
		if (priv->xofs + event->width > scaled_width) {
			x_offset = scaled_width - event->width - priv->xofs; 
		}
		if (priv->yofs + event->height > scaled_height) {
			y_offset = scaled_height - event->height - priv->yofs;
		}
		
		scroll_by (view, x_offset, y_offset);
	}

	update_scrollbar_values (view);
}


static gboolean
eog_scroll_view_focus_in_event (GtkWidget     *widget,
			    GdkEventFocus *event,
			    gpointer data)
{
	g_signal_stop_emission_by_name (G_OBJECT (widget), "focus_in_event");
	return FALSE;
}

static gboolean
eog_scroll_view_focus_out_event (GtkWidget     *widget,
			     GdkEventFocus *event,
			     gpointer data)
{
	g_signal_stop_emission_by_name (G_OBJECT (widget), "focus_out_event");
	return FALSE;
}

/* Expose event handler for the drawing area.  First we process the whole dirty
 * region by drawing a non-interpolated version, which is "instantaneous", and
 * we do this synchronously.  Then, if we are set to use interpolation, we queue
 * an idle handler to handle interpolated drawing there.
 */
static gboolean
display_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	EogScrollView *view;
	GdkRectangle *rects;
	gint n_rects;
	int i;

	g_return_val_if_fail (GTK_IS_DRAWING_AREA (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_SCROLL_VIEW (data), FALSE);

	view = EOG_SCROLL_VIEW (data);

	gdk_region_get_rectangles (event->region, &rects, &n_rects);

	for (i = 0; i < n_rects; i++) {
		request_paint_area (view, rects + i);
	}

	g_free (rects);

	return TRUE;
}

static void
style_set_event (GtkWidget *widget, GtkStyle *old_style, gpointer data)
{
	EogScrollView *view; 
	EogScrollViewPrivate *priv;
	GtkStyle *style;
	GdkColor *color;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;
	
	/* adapt background of display */
	style = gtk_widget_get_style (widget);
	color = &(style->bg[GTK_STATE_NORMAL]);
	gtk_widget_modify_bg (GTK_WIDGET (priv->display), GTK_STATE_NORMAL, color);
}

/*==================================

   image loading callbacks

   -----------------------------------*/

static void
image_loading_update_cb (EogImage *img, int x, int y, int width, int height, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;
	GdkRectangle area;
	int xofs, yofs;
	int sx0, sy0, sx1, sy1;

	view = (EogScrollView*) data;
	priv = view->priv;

#ifdef DEBUG
	g_print ("image_update_cb: x: %i, y: %i, width: %i, height: %i\n", x, y, width, height);
#endif

	if (priv->pixbuf == NULL) {
		priv->pixbuf = eog_image_get_pixbuf (img);
		set_zoom_fit (view);
		check_scrollbar_visibility (view, 0);
	}

	get_image_offsets (view, &xofs, &yofs);
	
	sx0 = floor (x * priv->zoom + xofs);
	sy0 = floor (y * priv->zoom + yofs);
	sx1 = ceil ((x + width) * priv->zoom + xofs);
	sy1 = ceil ((y + height) * priv->zoom + yofs);
	
	area.x = sx0;
	area.y = sy0;
	area.width = sx1 - sx0;
	area.height = sy1 - sy0;
		
	if (GTK_WIDGET_DRAWABLE (priv->display)) {
		gdk_window_invalidate_rect (GTK_WIDGET (priv->display)->window, &area, FALSE);
	}
}


static void
image_loading_finished_cb (EogImage *img, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;

	view = (EogScrollView*) data;
	priv = view->priv;

	if (priv->pixbuf == NULL) {
		priv->pixbuf = eog_image_get_pixbuf (img);
		priv->progressive_loading = PROGRESSIVE_NONE;
		set_zoom_fit (view);
		check_scrollbar_visibility (view, 0);
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));
		
	} 
	else if (priv->interp_type != GDK_INTERP_NEAREST &&
		 !is_unity_zoom (priv))
	{
		/* paint antialiased image version */
		priv->progressive_loading = PROGRESSIVE_POLISHING;
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));	
	}
}

static void
image_loading_failed_cb (EogImage *img, char *msg, gpointer data)
{
	EogScrollViewPrivate *priv;

	priv = EOG_SCROLL_VIEW (data)->priv;

	g_print ("loading failed.\n");

	if (priv->pixbuf != 0) {
		g_object_unref (priv->pixbuf);
		priv->pixbuf = 0;
	}

	if (GTK_WIDGET_DRAWABLE (priv->display)) {
		gdk_window_clear (GTK_WIDGET (priv->display)->window);
	}
}

static void
image_loading_cancelled_cb (EogImage *img, gpointer data)
{
	EogScrollViewPrivate *priv;

	priv = EOG_SCROLL_VIEW (data)->priv;

	if (priv->pixbuf != NULL) {
		g_object_unref (priv->pixbuf);
		priv->pixbuf = NULL;
	}

	if (GTK_WIDGET_DRAWABLE (priv->display)) {
		gdk_window_clear (GTK_WIDGET (priv->display)->window);
	}
}

static void
image_changed_cb (EogImage *img, gpointer data)
{
	EogScrollViewPrivate *priv;

	priv = EOG_SCROLL_VIEW (data)->priv;

	if (priv->pixbuf != NULL) {
		g_object_unref (priv->pixbuf);
		priv->pixbuf = NULL;
	}

	priv->pixbuf = eog_image_get_pixbuf (img);
	g_object_ref (priv->pixbuf);
	
	set_zoom_fit (EOG_SCROLL_VIEW (data));
	check_scrollbar_visibility (EOG_SCROLL_VIEW (data), 0);

	gtk_widget_queue_draw (GTK_WIDGET (priv->display));
}


/*===================================
         public API
  ---------------------------------*/

/* general properties */
void 
eog_scroll_view_set_zoom_upscale (EogScrollView *view, gboolean upscale)
{
	EogScrollViewPrivate *priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));
	
	priv = view->priv;

	if (priv->upscale != upscale) {
		priv->upscale = upscale;

		if (priv->modus == MODUS_ZOOM_FIT) {
			set_zoom_fit (view);
			gtk_widget_queue_draw (GTK_WIDGET (priv->display));
		}
	}
}

void 
eog_scroll_view_set_antialiasing (EogScrollView *view, gboolean state)
{
	EogScrollViewPrivate *priv;
	GdkInterpType new_interp_type;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));
	
	priv = view->priv;

	new_interp_type = state ? GDK_INTERP_BILINEAR : GDK_INTERP_NEAREST;

	if (priv->interp_type != new_interp_type) {
		priv->interp_type = new_interp_type;
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));
	}
}

void 
eog_scroll_view_set_transparency (EogScrollView *view, TransparencyStyle style, GdkColor *color)
{
	EogScrollViewPrivate *priv;
	GdkInterpType new_interp_type;
	guint32 col = 0;
	guint32 red, green, blue;
	gboolean changed = FALSE;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));
	
	priv = view->priv;

	if (color != NULL) {
		red = (color->red >> 8) << 16;
		green = (color->green >> 8) << 8;
		blue = (color->blue >> 8);
		col = red + green + blue;
	}

	if (priv->transp_style != style) {
		priv->transp_style = style;
		changed = TRUE;
	}

	if (priv->transp_style == TRANSP_COLOR && priv->transp_color != col) {
		priv->transp_color = col;
		changed = TRUE;
	}

	if (changed && priv->pixbuf != NULL && gdk_pixbuf_get_has_alpha (priv->pixbuf)) {
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));
	}
}

/* zoom api */

static double preferred_zoom_levels[] = {
	1.0 / 100, 1.0 / 50, 1.0 / 20,
	1.0 / 10.0, 1.0 / 5.0, 1.0 / 3.0, 1.0 / 2.0, 1.0 / 1.5, 
        1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
        11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0
};
static const gint n_zoom_levels = (sizeof (preferred_zoom_levels) / sizeof (double));

void 
eog_scroll_view_zoom_in (EogScrollView *view, gboolean smooth)
{
	EogScrollViewPrivate *priv;
	double zoom;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));
	
	priv = view->priv;

	if (smooth) {
		zoom = priv->zoom * IMAGE_VIEW_ZOOM_MULTIPLIER;
	}
	else {
		int i;
		int index = -1;

		for (i = 0; i < n_zoom_levels; i++) {
			if (preferred_zoom_levels [i] > priv->zoom) {
				index = i;
				break;
			}
		}

		if (index == -1) {
			zoom = priv->zoom;
		}
		else {
			zoom = preferred_zoom_levels [i];
		}
	}
	set_zoom (view, zoom, FALSE, 0, 0);
	
}

void 
eog_scroll_view_zoom_out (EogScrollView *view, gboolean smooth)
{
	EogScrollViewPrivate *priv;
	double zoom;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));
	
	priv = view->priv;

	if (smooth) {
		zoom = priv->zoom / IMAGE_VIEW_ZOOM_MULTIPLIER;
	}
	else {
		int i;
		int index = -1;

		for (i = n_zoom_levels - 1; i >= 0; i--) {
			if (preferred_zoom_levels [i] < priv->zoom) {
				index = i;
				break;
			}
		}
		if (index == -1) {
			zoom = priv->zoom;
		}
		else {
			zoom = preferred_zoom_levels [i];
		}
	}
	set_zoom (view, zoom, FALSE, 0, 0);
}

void
eog_scroll_view_zoom_fit (EogScrollView *view)
{
	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	set_zoom_fit (view);
	check_scrollbar_visibility (view, 0);
	gtk_widget_queue_draw (GTK_WIDGET (view->priv->display));
}

void
eog_scroll_view_set_zoom (EogScrollView *view, double zoom)
{
	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	set_zoom (view, zoom, FALSE, 0, 0);
}

double
eog_scroll_view_get_zoom (EogScrollView *view)
{
	g_return_val_if_fail (EOG_IS_SCROLL_VIEW (view), 0.0);

	return view->priv->zoom;
}


void 
eog_scroll_view_set_image (EogScrollView *view, EogImage *image)
{
	EogScrollViewPrivate *priv;
	
	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	priv = view->priv;

	if (priv->image == image) {
		return;
	}

	if (image != NULL) {
		g_object_ref (image);
	}
	
	if (priv->image != NULL) {
		int i;
		for (i = 0; i < 5; i++) {
			if (priv->image_cb_ids[i] > 0 ) {
				g_signal_handler_disconnect (G_OBJECT (priv->image), priv->image_cb_ids[i]);
			}
		}
		g_object_unref (priv->image);
		priv->image = NULL;
		
		if (priv->pixbuf != NULL) {
			g_object_unref (priv->pixbuf);
			priv->pixbuf = NULL;
		}

		if (GTK_WIDGET_DRAWABLE (priv->display) && image == NULL) {
			gdk_window_clear (GTK_WIDGET (priv->display)->window);
		}
	}
	g_assert (priv->image == NULL);
	g_assert (priv->pixbuf == NULL);


	priv->progressive_loading = PROGRESSIVE_NONE;
	if (image != 0) {
		priv->image = image;

		priv->image_cb_ids[0] = g_signal_connect (priv->image, "loading_update", 
							  (GCallback) image_loading_update_cb, view);
		priv->image_cb_ids[1] = g_signal_connect (priv->image, "loading_finished", 
							  (GCallback) image_loading_finished_cb, view);
		priv->image_cb_ids[2] = g_signal_connect (priv->image, "loading_failed", 
							  (GCallback) image_loading_failed_cb, view);
		priv->image_cb_ids[3] = g_signal_connect (priv->image, "loading_cancelled", 
							  (GCallback) image_loading_cancelled_cb, view);
		priv->image_cb_ids[4] = g_signal_connect (priv->image, "changed", 
							  (GCallback) image_changed_cb, view);

		if (eog_image_load (priv->image)) {
			priv->pixbuf = eog_image_get_pixbuf (priv->image);
		}
		else {
			priv->progressive_loading = PROGRESSIVE_LOADING;
		}
	}

	if (priv->progressive_loading == PROGRESSIVE_NONE) { 
		set_zoom_fit (view);
		check_scrollbar_visibility (view, 0);
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));
	}
}

void     
eog_scroll_view_get_image_size   (EogScrollView *view, int *width, int *height, gboolean scaled)
{
	EogScrollViewPrivate *priv;

	width = height = 0;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	priv = view->priv;

	if (priv->pixbuf == 0) return;

	*width = gdk_pixbuf_get_width (priv->pixbuf);
	*height = gdk_pixbuf_get_height (priv->pixbuf);
}


/*===================================
    object creation/freeing
  ---------------------------------*/


static void 
eog_scroll_view_instance_init (EogScrollView *view)
{
	EogScrollViewPrivate *priv;

	priv = g_new0 (EogScrollViewPrivate, 1);
	priv->zoom = 1.0;
	priv->modus = MODUS_ZOOM_FIT;
	priv->upscale = FALSE;
	priv->uta = NULL;
	priv->interp_type = GDK_INTERP_BILINEAR;
	priv->image = NULL;
	priv->pixbuf = NULL;
	priv->progressive_loading = PROGRESSIVE_NONE;
	priv->transp_style = TRANSP_BACKGROUND;
	priv->transp_color = 0;

	view->priv = priv;
}

static void
eog_scroll_view_dispose (GObject *object)
{
	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
eog_scroll_view_finalize (GObject *object)
{
	EogScrollView *view;

	view = EOG_SCROLL_VIEW (object);
	if (view->priv != 0) {
		g_free (view->priv);
		view->priv = 0;
	}

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
eog_scroll_view_class_init (EogScrollViewClass *klass)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;

	gobject_class = (GObjectClass*) klass;
	widget_class = (GtkWidgetClass*) klass;

	gobject_class->dispose = eog_scroll_view_dispose;
  	gobject_class->finalize = eog_scroll_view_finalize;


	view_signals [SIGNAL_ZOOM_CHANGED] = 
		g_signal_new ("zoom_changed",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogScrollViewClass, zoom_changed),
			      NULL, NULL,
			      libeog_marshal_VOID__DOUBLE,
			      G_TYPE_NONE, 1,
			      G_TYPE_DOUBLE);
	
	widget_class->size_allocate = eog_scroll_view_size_allocate;
}


GtkWidget*
eog_scroll_view_new (void)
{
	GtkWidget *widget;
	GtkWidget *vbar;
	GtkWidget *hbar;
	GtkTable *table;
	EogScrollView *view;
	EogScrollViewPrivate *priv;

	widget = g_object_new (EOG_TYPE_SCROLL_VIEW, 
			       "n_rows", 2, 
			       "n_columns", 2, 
			       "homogeneous", FALSE,
			       NULL);

	table = GTK_TABLE (widget);
	view = EOG_SCROLL_VIEW (widget);
	priv = view->priv;
	
	priv->hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0, 100, 0, 10, 10, 100));
	g_signal_connect (priv->hadj, "value_changed",
			  G_CALLBACK (adjustment_changed_cb),
			  view);
	priv->hbar = gtk_hscrollbar_new (priv->hadj);
	priv->vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0, 100, 0, 10, 10, 100));
	g_signal_connect (priv->vadj, "value_changed",
			  G_CALLBACK (adjustment_changed_cb),
			  view);
	priv->vbar = gtk_vscrollbar_new (priv->vadj);
	priv->display = g_object_new (GTK_TYPE_DRAWING_AREA, 
				      "can-focus", TRUE, 
				      NULL);
	/* We don't want to be double-buffered as we are SuperSmart(tm) */
	gtk_widget_set_double_buffered (GTK_WIDGET (priv->display), FALSE);
	gtk_widget_set_double_buffered (GTK_WIDGET (table), FALSE);
	gtk_widget_add_events (GTK_WIDGET (priv->display),
			       GDK_EXPOSURE_MASK
			       | GDK_BUTTON_PRESS_MASK
			       | GDK_BUTTON_RELEASE_MASK
			       | GDK_POINTER_MOTION_MASK
			       | GDK_POINTER_MOTION_HINT_MASK
			       | GDK_SCROLL_MASK
			       | GDK_KEY_PRESS_MASK);
	g_signal_connect (G_OBJECT (priv->display), "configure_event", G_CALLBACK (display_size_change), view);
	g_signal_connect (G_OBJECT (priv->display), "expose_event", G_CALLBACK (display_expose_event), view);
	g_signal_connect (G_OBJECT (priv->display), "map_event", G_CALLBACK (display_map_event), view);
	g_signal_connect (G_OBJECT (priv->display), "map_event", G_CALLBACK (display_map_cb), view);
	g_signal_connect (G_OBJECT (priv->display), "button_press_event", G_CALLBACK (eog_scroll_view_button_press_event), view);
	g_signal_connect (G_OBJECT (priv->display), "motion_notify_event", G_CALLBACK (eog_scroll_view_motion_event), view);
	g_signal_connect (G_OBJECT (priv->display), "button_release_event", G_CALLBACK (eog_scroll_view_button_release_event), view);
	g_signal_connect (G_OBJECT (priv->display), "scroll_event", G_CALLBACK (eog_scroll_view_scroll_event), view);
	g_signal_connect (G_OBJECT (priv->display), "focus_in_event", G_CALLBACK (eog_scroll_view_focus_in_event), NULL);
	g_signal_connect (G_OBJECT (priv->display), "focus_out_event", G_CALLBACK (eog_scroll_view_focus_out_event), NULL);

	g_signal_connect (G_OBJECT (widget), "key_press_event", G_CALLBACK (display_key_press_event), view);
	g_signal_connect (G_OBJECT (widget), "style-set", G_CALLBACK (style_set_event), view);

	gtk_table_attach (table, priv->display, 
			  0, 1, 0, 1, 
			  GTK_EXPAND | GTK_FILL, 
			  GTK_EXPAND | GTK_FILL,
			  0,0);
	gtk_table_attach (table, priv->hbar,
			  0, 1, 1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);
	gtk_table_attach (table, priv->vbar, 
			  1, 2, 0, 1,
			  GTK_FILL, GTK_FILL,
			  0, 0);

	gtk_widget_show_all (widget);

	return widget;
}
