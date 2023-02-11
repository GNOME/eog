#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <math.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>
#ifdef HAVE_RSVG
#include <librsvg/rsvg.h>
#endif

#include <glib/gi18n.h>

#include "eog-config-keys.h"
#include "eog-enum-types.h"
#include "eog-scroll-view.h"
#include "eog-debug.h"
#include "zoom.h"

/* Maximum zoom factor */
#define MAX_ZOOM_FACTOR EOG_SCROLL_VIEW_MAX_ZOOM_FACTOR
#define MIN_ZOOM_FACTOR EOG_SCROLL_VIEW_MIN_ZOOM_FACTOR

/* Default increment for zooming.  The current zoom factor is multiplied or
 * divided by this amount on every zooming step.  For consistency, you should
 * use the same value elsewhere in the program.
 */
#define IMAGE_VIEW_ZOOM_MULTIPLIER 1.05

/* Maximum size of delayed repaint rectangles */
#define PAINT_RECT_WIDTH 128
#define PAINT_RECT_HEIGHT 128

/* Scroll step increment */
#define SCROLL_STEP_SIZE 32

#define CHECK_MEDIUM 8
#define CHECK_BLACK "#000000"
#define CHECK_DARK "#555555"
#define CHECK_GRAY "#808080"
#define CHECK_LIGHT "#cccccc"
#define CHECK_WHITE "#ffffff"

/* Time used for the revealing animation of the overlaid buttons */
#define OVERLAY_REVEAL_ANIM_TIME (1000U) /* ms */
#define OVERLAY_FADE_OUT_TIMEOUT_MS (2000U)

/* from cairo-image-surface.c */
#define MAX_IMAGE_SIZE 32767

/* Signal IDs */
enum {
	SIGNAL_ZOOM_CHANGED,
	SIGNAL_ROTATION_CHANGED,
	SIGNAL_NEXT_IMAGE,
	SIGNAL_PREVIOUS_IMAGE,
	SIGNAL_LAST
};
static gint view_signals [SIGNAL_LAST];

typedef enum {
	EOG_SCROLL_VIEW_CURSOR_NORMAL,
	EOG_SCROLL_VIEW_CURSOR_HIDDEN,
	EOG_SCROLL_VIEW_CURSOR_DRAG
} EogScrollViewCursor;

typedef enum {
	EOG_ROTATION_0,
	EOG_ROTATION_90,
	EOG_ROTATION_180,
	EOG_ROTATION_270,
	N_EOG_ROTATIONS
} EogRotationState;

typedef enum {
	EOG_PAN_ACTION_NONE,
	EOG_PAN_ACTION_NEXT,
	EOG_PAN_ACTION_PREV
} EogPanAction;

/* Drag 'n Drop */
static GtkTargetEntry target_table[] = {
        { "text/uri-list", 0, 0},
};

enum {
	PROP_0,
	PROP_ANTIALIAS_IN,
	PROP_ANTIALIAS_OUT,
	PROP_BACKGROUND_COLOR,
	PROP_IMAGE,
	PROP_SCROLLWHEEL_ZOOM,
	PROP_TRANSP_COLOR,
	PROP_TRANSPARENCY_STYLE,
	PROP_USE_BG_COLOR,
	PROP_ZOOM_MODE,
	PROP_ZOOM_MULTIPLIER,
	PROP_HADJUSTMENT,
	PROP_VADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_VSCROLL_POLICY
};

/* Private part of the EogScrollView structure */
struct _EogScrollViewPrivate {
	/* some widgets we rely on */
	GtkWidget *display;
	GtkAdjustment *hadj;
	GtkAdjustment *vadj;
	GtkPolicyType hscroll_policy;
	GtkPolicyType vscroll_policy;
	GtkWidget *menu;

	/* actual image */
	EogImage *image;
	guint image_changed_id;
	guint frame_changed_id;
	GdkPixbuf *pixbuf;
	cairo_surface_t *surface;

	/* zoom mode, either ZOOM_MODE_FIT or ZOOM_MODE_FREE */
	EogZoomMode zoom_mode;

	/* whether to allow zoom > 1.0 on zoom fit */
	gboolean upscale;

	/* the actual zoom factor */
	double zoom;

	/* the minimum possible (reasonable) zoom factor */
	double min_zoom;

	/* Current scrolling offsets */
	int xofs, yofs;

	/* handler ID for paint idle callback */
	guint idle_id;

	/* Interpolation type when zoomed in*/
	cairo_filter_t interp_type_in;

	/* Interpolation type when zoomed out*/
	cairo_filter_t interp_type_out;

	/* Scroll wheel zoom */
	gboolean scroll_wheel_zoom;

	/* Scroll wheel zoom */
	gdouble zoom_multiplier;

	/* dragging stuff */
	int drag_anchor_x, drag_anchor_y;
	int drag_ofs_x, drag_ofs_y;
	guint dragging : 1;

	/* how to indicate transparency in images */
	EogTransparencyStyle transp_style;
	GdkRGBA transp_color;

	/* the type of the cursor we are currently showing */
	EogScrollViewCursor cursor;

	gboolean  use_bg_color;
	GdkRGBA *background_color;
	GdkRGBA *override_bg_color;

	cairo_surface_t *background_surface;

	GtkGesture *pan_gesture;
	GtkGesture *zoom_gesture;
	GtkGesture *rotate_gesture;
	gdouble initial_zoom;
	EogRotationState rotate_state;
	EogPanAction pan_action;

	GtkWidget *left_revealer;
	GtkWidget *right_revealer;
	GtkWidget *bottom_revealer;
	GSource   *overlay_timeout_source;

	/* Two-pass filtering */
	GSource *hq_redraw_timeout_source;
	gboolean force_unfiltered;
};

static void scroll_by (EogScrollView *view, int xofs, int yofs);
static void set_zoom_fit (EogScrollView *view);
/* static void request_paint_area (EogScrollView *view, GdkRectangle *area); */
static void set_minimum_zoom_factor (EogScrollView *view);
static void view_on_drag_begin_cb (GtkWidget *widget, GdkDragContext *context,
                                   gpointer user_data);
static void view_on_drag_data_get_cb (GtkWidget *widget,
                                      GdkDragContext*drag_context,
                                      GtkSelectionData *data, guint info,
                                      guint time, gpointer user_data);
static void _set_zoom_mode_internal (EogScrollView *view, EogZoomMode mode);
static gboolean eog_scroll_view_get_image_coords (EogScrollView *view, gint *x,
                                                  gint *y, gint *width,
                                                  gint *height);
static gboolean _eog_gdk_rgba_equal0 (const GdkRGBA *a, const GdkRGBA *b);
static void eog_scroll_view_set_hadjustment (EogScrollView *view, GtkAdjustment *adjustment);
static void eog_scroll_view_set_vadjustment (EogScrollView *view, GtkAdjustment *adjustment);

G_DEFINE_TYPE_WITH_CODE (EogScrollView, eog_scroll_view, GTK_TYPE_OVERLAY,
                         G_ADD_PRIVATE (EogScrollView)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

/*===================================
    widget size changing handler &
	util functions
  ---------------------------------*/

static cairo_surface_t *
create_surface_from_pixbuf (EogScrollView *view, GdkPixbuf *pixbuf)
{
	cairo_surface_t *surface;
	gint w, h;

	w = gdk_pixbuf_get_width (pixbuf);
	h = gdk_pixbuf_get_height (pixbuf);

	if (w > MAX_IMAGE_SIZE || h > MAX_IMAGE_SIZE) {
		g_warning ("Image dimensions too large to process");
		w = 50;
		h = 50;

		surface = gdk_window_create_similar_image_surface (
				gtk_widget_get_window (view->priv->display),
				CAIRO_FORMAT_ARGB32, w, h, 1.0);
	} else {
		surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, 1.0,
				gtk_widget_get_window (view->priv->display));
	}

	return surface;
}

/* Disconnects from the EogImage and removes references to it */
static void
free_image_resources (EogScrollView *view)
{
	EogScrollViewPrivate *priv;

	priv = view->priv;

	if (priv->image_changed_id > 0) {
		g_signal_handler_disconnect (G_OBJECT (priv->image), priv->image_changed_id);
		priv->image_changed_id = 0;
	}

	if (priv->frame_changed_id > 0) {
		g_signal_handler_disconnect (G_OBJECT (priv->image), priv->frame_changed_id);
		priv->frame_changed_id = 0;
	}

	if (priv->image != NULL) {
		eog_image_data_unref (priv->image);
		priv->image = NULL;
	}

	if (priv->pixbuf != NULL) {
		g_object_unref (priv->pixbuf);
		priv->pixbuf = NULL;
	}

	if (priv->surface != NULL) {
		cairo_surface_destroy (priv->surface);
		priv->surface = NULL;
	}
}

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
	else {
		*xofs = floor (view_cx * new_zoom - zoom_x_anchor * width + 0.5);
		if (*xofs < 0)
			*xofs = 0;
	}

	if (new_scaled_height < height)
		*yofs = 0;
	else {
		*yofs = floor (view_cy * new_zoom - zoom_y_anchor * height + 0.5);
		if (*yofs < 0)
			*yofs = 0;
	}
}

/* Sets the adjustment values based on the current scrolling offset */
static void
update_adjustment_values (EogScrollView *view)
{
	EogScrollViewPrivate *priv;
	int scaled_width, scaled_height;
	gdouble page_size,page_increment,step_increment;
	gdouble lower, upper;
	GtkAllocation allocation;

	priv = view->priv;

	compute_scaled_size (view, priv->zoom, &scaled_width, &scaled_height);
	gtk_widget_get_allocation (GTK_WIDGET (priv->display), &allocation);

	/* Set scroll increments */
	page_size = MIN (scaled_width, allocation.width);
	page_increment = allocation.width / 2;
	step_increment = SCROLL_STEP_SIZE;

	/* Set scroll bounds and new offsets */
	lower = 0;
	upper = scaled_width;
	priv->xofs = CLAMP (priv->xofs, 0, upper - page_size);

	g_signal_handlers_block_matched (
	        priv->hadj, G_SIGNAL_MATCH_DATA,
	        0, 0, NULL, NULL, view);

	gtk_adjustment_configure (priv->hadj, priv->xofs, lower,
	                          upper, step_increment,
	                          page_increment, page_size);

	g_signal_handlers_unblock_matched (
	        priv->hadj, G_SIGNAL_MATCH_DATA,
	        0, 0, NULL, NULL, view);

	page_size = MIN (scaled_height, allocation.height);
	page_increment = allocation.height / 2;
	step_increment = SCROLL_STEP_SIZE;

	lower = 0;
	upper = scaled_height;
	priv->yofs = CLAMP (priv->yofs, 0, upper - page_size);

	g_signal_handlers_block_matched (
	        priv->vadj, G_SIGNAL_MATCH_DATA,
	        0, 0, NULL, NULL, view);

	gtk_adjustment_configure (priv->vadj, priv->yofs, lower,
	                          upper, step_increment,
	                          page_increment, page_size);

	g_signal_handlers_unblock_matched (
	        priv->vadj, G_SIGNAL_MATCH_DATA,
	        0, 0, NULL, NULL, view);
}

static void
eog_scroll_view_set_cursor (EogScrollView *view, EogScrollViewCursor new_cursor)
{
	GdkCursor *cursor = NULL;
	GdkDisplay *display;
	GtkWidget *widget;

	if (view->priv->cursor == new_cursor) {
		return;
	}

	widget = gtk_widget_get_toplevel (GTK_WIDGET (view));
	display = gtk_widget_get_display (widget);
	view->priv->cursor = new_cursor;

	switch (new_cursor) {
	        case EOG_SCROLL_VIEW_CURSOR_NORMAL:
		        gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);
		        break;
	        case EOG_SCROLL_VIEW_CURSOR_HIDDEN:
		        cursor = gdk_cursor_new_for_display (display, GDK_BLANK_CURSOR);
		        break;
	        case EOG_SCROLL_VIEW_CURSOR_DRAG:
		        cursor = gdk_cursor_new_for_display (display, GDK_FLEUR);
		        break;
	}

	if (cursor) {
		gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
		g_object_unref (cursor);
		gdk_display_flush(display);
	}
}

#define DOUBLE_EQUAL_MAX_DIFF 1e-6
#define DOUBLE_EQUAL(a,b) (fabs (a - b) < DOUBLE_EQUAL_MAX_DIFF)

/* Returns whether the image is zoomed in */
static gboolean
is_zoomed_in (EogScrollView *view)
{
	EogScrollViewPrivate *priv;

	priv = view->priv;
	return priv->zoom - 1.0 > DOUBLE_EQUAL_MAX_DIFF;
}

/* Returns whether the image is zoomed out */
static gboolean
is_zoomed_out (EogScrollView *view)
{
	EogScrollViewPrivate *priv;

	priv = view->priv;
	return DOUBLE_EQUAL_MAX_DIFF + priv->zoom - 1.0 < 0.0;
}

/* Returns wether the image is movable, that means if it is larger then
 * the actual visible area.
 */

gboolean
eog_scroll_view_is_image_movable (EogScrollView *view)
{
	EogScrollViewPrivate *priv;

	priv = view->priv;
	gboolean hmovable = gtk_adjustment_get_page_size (priv->hadj) < gtk_adjustment_get_upper (priv->hadj);
	gboolean vmovable = gtk_adjustment_get_page_size (priv->vadj) < gtk_adjustment_get_upper (priv->vadj);

	return hmovable || vmovable;
}

/*===================================
	  drawing core
  ---------------------------------*/

static void
get_transparency_params (EogScrollView *view, int *size, GdkRGBA *color1, GdkRGBA *color2)
{
	EogScrollViewPrivate *priv;

	priv = view->priv;

	/* Compute transparency parameters */
	switch (priv->transp_style) {
	case EOG_TRANSP_BACKGROUND: {
		/* Simply return fully transparent color */
		color1->red = color1->green = color1->blue = color1->alpha = 0.0;
		color2->red = color2->green = color2->blue = color2->alpha = 0.0;
		break;
	}

	case EOG_TRANSP_CHECKED:
		g_warn_if_fail (gdk_rgba_parse (color1, CHECK_GRAY));
		g_warn_if_fail (gdk_rgba_parse (color2, CHECK_LIGHT));
		break;

	case EOG_TRANSP_COLOR:
		*color1 = *color2 = priv->transp_color;
		break;

	default:
		g_assert_not_reached ();
	};

	*size = CHECK_MEDIUM;
}


static cairo_surface_t *
create_background_surface (EogScrollView *view)
{
	int check_size;
	GdkRGBA check_1;
	GdkRGBA check_2;
	cairo_surface_t *surface;

	get_transparency_params (view, &check_size, &check_1, &check_2);
	surface = gdk_window_create_similar_surface (gtk_widget_get_window (view->priv->display),
	                                             CAIRO_CONTENT_COLOR_ALPHA,
	                                             check_size * 2, check_size * 2);
	cairo_t* cr = cairo_create (surface);

	/* Use source operator to make fully transparent work */
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

	gdk_cairo_set_source_rgba(cr, &check_1);
	cairo_rectangle (cr, 0, 0, check_size, check_size);
	cairo_rectangle (cr, check_size, check_size, check_size, check_size);
	cairo_fill (cr);

	gdk_cairo_set_source_rgba(cr, &check_2);
	cairo_rectangle (cr, 0, check_size, check_size, check_size);
	cairo_rectangle (cr, check_size, 0, check_size, check_size);
	cairo_fill (cr);

	cairo_destroy (cr);

	return surface;
}

/* =======================================

    scrolling stuff

    --------------------------------------*/

/* Scrolls the view to the specified offsets.  */
static void
scroll_to (EogScrollView *view, int x, int y, gboolean change_adjustments)
{
	EogScrollViewPrivate *priv;
	GtkAllocation allocation;
	int xofs, yofs;
	GdkWindow *window;

	priv = view->priv;

	/* Check bounds & Compute offsets */
	x = CLAMP (x, 0, gtk_adjustment_get_upper (priv->hadj)
	                 - gtk_adjustment_get_page_size (priv->hadj));
	xofs = x - priv->xofs;

	y = CLAMP (y, 0, gtk_adjustment_get_upper (priv->vadj)
	                 - gtk_adjustment_get_page_size (priv->vadj));
	yofs = y - priv->yofs;

	if (xofs == 0 && yofs == 0)
		return;

	priv->xofs = x;
	priv->yofs = y;

	if (!gtk_widget_is_drawable (priv->display))
		goto out;

	gtk_widget_get_allocation (GTK_WIDGET (priv->display), &allocation);

	if (abs (xofs) >= allocation.width || abs (yofs) >= allocation.height) {
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));
		goto out;
	}

	window = gtk_widget_get_window (GTK_WIDGET (priv->display));

	if (!gtk_gesture_is_recognized (priv->zoom_gesture)) {
		gdk_window_scroll (window, -xofs, -yofs);
	}

 out:
	if (!change_adjustments)
		return;

	g_signal_handlers_block_matched (
	        priv->hadj, G_SIGNAL_MATCH_DATA,
	        0, 0, NULL, NULL, view);
	g_signal_handlers_block_matched (
	        priv->vadj, G_SIGNAL_MATCH_DATA,
	        0, 0, NULL, NULL, view);

	gtk_adjustment_set_value (priv->hadj, x);
	gtk_adjustment_set_value (priv->vadj, y);

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
	if (gtk_widget_get_realized (GTK_WIDGET (view)))
		scroll_to (view, gtk_adjustment_get_value (priv->hadj),
		           gtk_adjustment_get_value (priv->vadj), FALSE);
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

static void
set_minimum_zoom_factor (EogScrollView *view)
{
	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	view->priv->min_zoom = MAX (1.0 / gdk_pixbuf_get_width (view->priv->pixbuf),
	                            MAX(1.0 / gdk_pixbuf_get_height (view->priv->pixbuf),
	                                MIN_ZOOM_FACTOR) );
	return;
}

/**
 * set_zoom:
 * @view: A scroll view.
 * @zoom: Zoom factor.
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
static void
set_zoom (EogScrollView *view, double zoom,
          gboolean have_anchor, int anchorx, int anchory)
{
	EogScrollViewPrivate *priv;
	GtkAllocation allocation;
	int xofs, yofs;
	double x_rel, y_rel;

	priv = view->priv;

	if (priv->pixbuf == NULL)
		return;

	if (zoom > MAX_ZOOM_FACTOR)
		zoom = MAX_ZOOM_FACTOR;
	else if (zoom < MIN_ZOOM_FACTOR)
		zoom = MIN_ZOOM_FACTOR;

	if (DOUBLE_EQUAL (priv->zoom, zoom))
		return;
	if (DOUBLE_EQUAL (priv->zoom, priv->min_zoom) && zoom < priv->zoom)
		return;

	eog_scroll_view_set_zoom_mode (view, EOG_ZOOM_MODE_FREE);

	gtk_widget_get_allocation (GTK_WIDGET (priv->display), &allocation);

	/* compute new xofs/yofs values */
	if (have_anchor) {
		x_rel = (double) anchorx / allocation.width;
		y_rel = (double) anchory / allocation.height;
	} else {
		x_rel = 0.5;
		y_rel = 0.5;
	}

	compute_center_zoom_offsets (view, priv->zoom, zoom,
	                             allocation.width, allocation.height,
	                             x_rel, y_rel,
	                             &xofs, &yofs);

	/* set new values */
	priv->xofs = xofs; /* (img_width * x_rel * zoom) - anchorx; */
	priv->yofs = yofs; /* (img_height * y_rel * zoom) - anchory; */

	if (priv->dragging) {
		priv->drag_anchor_x = anchorx;
		priv->drag_anchor_y = anchory;
		priv->drag_ofs_x = priv->xofs;
		priv->drag_ofs_y = priv->yofs;
	}
	if (zoom <= priv->min_zoom)
		priv->zoom = priv->min_zoom;
	else
		priv->zoom = zoom;

	/* we make use of the new values here */
	update_adjustment_values (view);

	/* repaint the whole image */
	gtk_widget_queue_draw (GTK_WIDGET (priv->display));

	g_signal_emit (view, view_signals [SIGNAL_ZOOM_CHANGED], 0, priv->zoom);
}

/* Zooms the image to fit the available allocation */
static void
set_zoom_fit (EogScrollView *view)
{
	EogScrollViewPrivate *priv;
	GtkAllocation allocation;
	double new_zoom;

	priv = view->priv;

	priv->zoom_mode = EOG_ZOOM_MODE_SHRINK_TO_FIT;

	if (!gtk_widget_get_mapped (GTK_WIDGET (view)))
		return;

	if (priv->pixbuf == NULL)
		return;

	gtk_widget_get_allocation (GTK_WIDGET(priv->display), &allocation);

	new_zoom = zoom_fit_scale (allocation.width, allocation.height,
	                           gdk_pixbuf_get_width (priv->pixbuf),
	                           gdk_pixbuf_get_height (priv->pixbuf),
	                           priv->upscale);

	if (new_zoom > MAX_ZOOM_FACTOR)
		new_zoom = MAX_ZOOM_FACTOR;
	else if (new_zoom < MIN_ZOOM_FACTOR)
		new_zoom = MIN_ZOOM_FACTOR;

	priv->zoom = new_zoom;
	priv->xofs = 0;
	priv->yofs = 0;

	/* we make use of the new values here */
	update_adjustment_values (view);

	g_signal_emit (view, view_signals [SIGNAL_ZOOM_CHANGED], 0, priv->zoom);
}

/*===================================

   internal signal callbacks

  ---------------------------------*/

/* Button press event handler for the image view */
static gboolean
eog_scroll_view_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;

	if (!gtk_widget_has_focus (priv->display))
		gtk_widget_grab_focus (GTK_WIDGET (priv->display));

	if (priv->dragging)
		return FALSE;

	switch (event->button) {
	        case 1:
	        case 2:
		        if (event->button == 1 && !priv->scroll_wheel_zoom &&
			    !(event->state & GDK_CONTROL_MASK))
				break;

			if (eog_scroll_view_is_image_movable (view)) {
				eog_scroll_view_set_cursor (view, EOG_SCROLL_VIEW_CURSOR_DRAG);

				priv->dragging = TRUE;
				priv->drag_anchor_x = event->x;
				priv->drag_anchor_y = event->y;

				priv->drag_ofs_x = priv->xofs;
				priv->drag_ofs_y = priv->yofs;

				return TRUE;
			}
	        default:
		        break;
	}

	return FALSE;
}

/* Button release event handler for the image view */
static gboolean
eog_scroll_view_button_release_event (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;

	if (!priv->dragging)
		return FALSE;

	switch (event->button) {
	        case 1:
	        case 2:
		        drag_to (view, event->x, event->y);
			priv->dragging = FALSE;

			eog_scroll_view_set_cursor (view, EOG_SCROLL_VIEW_CURSOR_NORMAL);
		        break;

	        default:
		        break;
	}

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
	double min_zoom_factor;
	int xofs, yofs;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;

	/* Compute zoom factor and scrolling offsets; we'll only use either of them */
	/* same as in gtkscrolledwindow.c */
	xofs = gtk_adjustment_get_page_increment (priv->hadj) / 2;
	yofs = gtk_adjustment_get_page_increment (priv->vadj) / 2;

	/* Make sure the user visible zoom factor changes */
	min_zoom_factor = (priv->zoom + 0.01L) / priv->zoom;

	switch (event->direction) {
	case GDK_SCROLL_UP:
		zoom_factor = fmax(priv->zoom_multiplier, min_zoom_factor);

		xofs = 0;
		yofs = -yofs;
		break;

	case GDK_SCROLL_LEFT:
		zoom_factor = 1.0 / fmax(priv->zoom_multiplier,
					 min_zoom_factor);
		xofs = -xofs;
		yofs = 0;
		break;

	case GDK_SCROLL_DOWN:
		zoom_factor = 1.0 / fmax(priv->zoom_multiplier,
					 min_zoom_factor);
		xofs = 0;
		yofs = yofs;
		break;

	case GDK_SCROLL_RIGHT:
		zoom_factor = fmax(priv->zoom_multiplier, min_zoom_factor);
		xofs = xofs;
		yofs = 0;
		break;

	default:
		g_assert_not_reached ();
		return FALSE;
	}

	if (priv->scroll_wheel_zoom) {
		if (event->state & GDK_SHIFT_MASK)
			scroll_by (view, yofs, xofs);
		else if (event->state & GDK_CONTROL_MASK)
			scroll_by (view, xofs, yofs);
		else
			set_zoom (view, priv->zoom * zoom_factor,
			          TRUE, event->x, event->y);
	} else {
		if (event->state & GDK_SHIFT_MASK)
			scroll_by (view, yofs, xofs);
		else if (event->state & GDK_CONTROL_MASK)
			set_zoom (view, priv->zoom * zoom_factor,
			          TRUE, event->x, event->y);
		else
			scroll_by (view, xofs, yofs);
	}

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

	if (gtk_gesture_is_recognized (priv->zoom_gesture))
		return TRUE;

	if (!priv->dragging)
		return FALSE;

	if (event->is_hint)
		gdk_window_get_device_position (gtk_widget_get_window (GTK_WIDGET (priv->display)), event->device, &x, &y, &mods);
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

	eog_debug (DEBUG_WINDOW);

	set_zoom_fit (view);
	gtk_widget_queue_draw (GTK_WIDGET (priv->display));
}

static void
display_size_change (GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;

	if (priv->zoom_mode == EOG_ZOOM_MODE_SHRINK_TO_FIT) {
		set_zoom_fit (view);
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));
	} else {
		int scaled_width, scaled_height;
		int x_offset = 0;
		int y_offset = 0;

		compute_scaled_size (view, priv->zoom, &scaled_width, &scaled_height);

		if (priv->xofs + event->width > scaled_width)
			x_offset = scaled_width - event->width - priv->xofs;

		if (priv->yofs + event->height > scaled_height)
			y_offset = scaled_height - event->height - priv->yofs;

		scroll_by (view, x_offset, y_offset);
	}

	update_adjustment_values (view);
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

static gboolean _hq_redraw_cb (gpointer user_data)
{
	EogScrollViewPrivate *priv = EOG_SCROLL_VIEW (user_data)->priv;

	priv->force_unfiltered = FALSE;
	gtk_widget_queue_draw (GTK_WIDGET (priv->display));

	priv->hq_redraw_timeout_source = NULL;
	return G_SOURCE_REMOVE;
}

static void
_clear_hq_redraw_timeout (EogScrollView *view)
{
	EogScrollViewPrivate *priv = view->priv;

	if (priv->hq_redraw_timeout_source != NULL) {
		g_source_unref (priv->hq_redraw_timeout_source);
		g_source_destroy (priv->hq_redraw_timeout_source);
	}

	priv->hq_redraw_timeout_source = NULL;
}

static void
_set_hq_redraw_timeout (EogScrollView *view)
{
	GSource *source;

	_clear_hq_redraw_timeout (view);

	source = g_timeout_source_new (200);
	g_source_set_callback (source, &_hq_redraw_cb, view, NULL);

	g_source_attach (source, NULL);

	view->priv->hq_redraw_timeout_source = source;
}

static gboolean
display_draw (GtkWidget *widget, cairo_t *cr, gpointer data)
{
	const GdkRGBA *background_color = NULL;
	EogScrollView *view;
	EogScrollViewPrivate *priv;
	GtkAllocation allocation;
	int scaled_width, scaled_height;
	int xofs, yofs;

	g_return_val_if_fail (GTK_IS_DRAWING_AREA (widget), FALSE);
	g_return_val_if_fail (EOG_IS_SCROLL_VIEW (data), FALSE);

	view = EOG_SCROLL_VIEW (data);

	priv = view->priv;

	if (priv->pixbuf == NULL)
		return TRUE;

	eog_scroll_view_get_image_coords (view, &xofs, &yofs,
	                                  &scaled_width, &scaled_height);

	eog_debug_message (DEBUG_WINDOW, "zoom %.2f, xofs: %i, yofs: %i scaled w: %i h: %i\n",
	                   priv->zoom, xofs, yofs, scaled_width, scaled_height);

	/* Paint the background */
	gtk_widget_get_allocation (GTK_WIDGET (priv->display), &allocation);
	cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
	if (priv->transp_style != EOG_TRANSP_BACKGROUND)
		cairo_rectangle (cr, MAX (0, xofs), MAX (0, yofs),
		                 scaled_width, scaled_height);
	if (priv->override_bg_color != NULL)
		background_color = priv->override_bg_color;
	else if (priv->use_bg_color)
		background_color = priv->background_color;
	if (background_color != NULL)
		cairo_set_source_rgba (cr,
		                       background_color->red,
		                       background_color->green,
		                       background_color->blue,
		                       background_color->alpha);
	else {
		GtkStyleContext *context;
		GdkRGBA *pattern_rgba;
		GtkStateFlags state;

		context = gtk_widget_get_style_context (priv->display);
		state = gtk_style_context_get_state (context);

		gtk_style_context_get (context, state, GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &pattern_rgba, NULL);
		gdk_cairo_set_source_rgba (cr, pattern_rgba);

		gdk_rgba_free (pattern_rgba);
	}
	cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
	cairo_fill (cr);

	if (gdk_pixbuf_get_has_alpha (priv->pixbuf)) {
		if (priv->background_surface == NULL) {
			priv->background_surface = create_background_surface (view);
		}
		cairo_set_source_surface (cr, priv->background_surface, xofs, yofs);
		cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
		cairo_rectangle (cr, xofs, yofs, scaled_width, scaled_height);
		cairo_fill (cr);
	}

	/* Make sure the image is only drawn as large as needed.
	 * This is especially necessary for SVGs where there might
	 * be more image data available outside the image boundaries.
	 */
	cairo_rectangle (cr, xofs, yofs, scaled_width, scaled_height);
	cairo_clip (cr);

#ifdef HAVE_RSVG
	if (eog_image_is_svg (view->priv->image)) {
		cairo_matrix_t matrix, translate, scale, original;
		EogTransform *transform = eog_image_get_transform (priv->image);
		cairo_matrix_init_identity (&matrix);
		if (transform) {
			cairo_matrix_t affine;
			double image_offset_x = 0., image_offset_y = 0.;

			eog_transform_get_affine (transform, &affine);
			cairo_matrix_multiply (&matrix, &affine, &matrix);

			switch (eog_transform_get_transform_type (transform)) {
			case EOG_TRANSFORM_ROT_90:
			case EOG_TRANSFORM_FLIP_HORIZONTAL:
				image_offset_x = (double) gdk_pixbuf_get_width (priv->pixbuf);
				break;
			case EOG_TRANSFORM_ROT_270:
			case EOG_TRANSFORM_FLIP_VERTICAL:
				image_offset_y = (double) gdk_pixbuf_get_height (priv->pixbuf);
				break;
			case EOG_TRANSFORM_ROT_180:
			case EOG_TRANSFORM_TRANSPOSE:
			case EOG_TRANSFORM_TRANSVERSE:
				image_offset_x = (double) gdk_pixbuf_get_width (priv->pixbuf);
				image_offset_y = (double) gdk_pixbuf_get_height (priv->pixbuf);
				break;
			case EOG_TRANSFORM_NONE:
			default:
				break;
			}
			cairo_matrix_init_translate (&translate, image_offset_x, image_offset_y);
			cairo_matrix_multiply (&matrix, &matrix, &translate);
		}
		cairo_matrix_init_scale (&scale, priv->zoom, priv->zoom);
		cairo_matrix_multiply (&matrix, &matrix, &scale);
		cairo_matrix_init_translate (&translate, xofs, yofs);
		cairo_matrix_multiply (&matrix, &matrix, &translate);

		cairo_get_matrix (cr, &original);
		cairo_matrix_multiply (&matrix, &matrix, &original);
		cairo_set_matrix (cr, &matrix);

		rsvg_handle_render_cairo (eog_image_get_svg (priv->image), cr);

	} else
#endif /* HAVE_RSVG */
	{
		cairo_filter_t interp_type;

		if(!DOUBLE_EQUAL(priv->zoom, 1.0) && priv->force_unfiltered)
		{
			interp_type = CAIRO_FILTER_NEAREST;
			_set_hq_redraw_timeout(view);
		}
		else
		{
			if (is_zoomed_in (view))
				interp_type = priv->interp_type_in;
			else
				interp_type = priv->interp_type_out;

			_clear_hq_redraw_timeout (view);
			priv->force_unfiltered = TRUE;
		}
		cairo_scale (cr, priv->zoom, priv->zoom);
		cairo_set_source_surface (cr, priv->surface, xofs/priv->zoom, yofs/priv->zoom);
		cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_PAD);
		if (is_zoomed_in (view) || is_zoomed_out (view))
			cairo_pattern_set_filter (cairo_get_source (cr), interp_type);

		cairo_paint (cr);
	}

	return TRUE;
}

static void
zoom_gesture_begin_cb (GtkGestureZoom   *gesture,
                       GdkEventSequence *sequence,
                       EogScrollView    *view)
{
	gdouble center_x, center_y;
	EogScrollViewPrivate *priv;

	priv = view->priv;

	/* Displace dragging point to gesture center */
	gtk_gesture_get_bounding_box_center (GTK_GESTURE (gesture),
	                                     &center_x, &center_y);
	priv->drag_anchor_x = center_x;
	priv->drag_anchor_y = center_y;
	priv->drag_ofs_x = priv->xofs;
	priv->drag_ofs_y = priv->yofs;
	priv->dragging = TRUE;
	priv->initial_zoom = priv->zoom;

	gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
zoom_gesture_update_cb (GtkGestureZoom   *gesture,
                        GdkEventSequence *sequence,
                        EogScrollView    *view)
{
	gdouble center_x, center_y, scale;
	EogScrollViewPrivate *priv;

	priv = view->priv;
	scale = gtk_gesture_zoom_get_scale_delta (gesture);
	gtk_gesture_get_bounding_box_center (GTK_GESTURE (gesture),
	                                     &center_x, &center_y);

	drag_to (view, center_x, center_y);
	set_zoom (view, priv->initial_zoom * scale, TRUE,
	          center_x, center_y);
}

static void
zoom_gesture_end_cb (GtkGestureZoom   *gesture,
                     GdkEventSequence *sequence,
                     EogScrollView    *view)
{
	EogScrollViewPrivate *priv;

	priv = view->priv;
	priv->dragging = FALSE;
	eog_scroll_view_set_cursor (view, EOG_SCROLL_VIEW_CURSOR_NORMAL);
}

static void
rotate_gesture_begin_cb (GtkGesture       *gesture,
                         GdkEventSequence *sequence,
                         EogScrollView    *view)
{
	EogScrollViewPrivate *priv;

	priv = view->priv;
	priv->rotate_state = EOG_ROTATION_0;
}

static void
pan_gesture_pan_cb (GtkGesturePan   *gesture,
                    GtkPanDirection  direction,
                    gdouble          offset,
                    EogScrollView   *view)
{
	EogScrollViewPrivate *priv;
	const gboolean is_rtl = gtk_widget_get_direction (GTK_WIDGET (view)) == GTK_TEXT_DIR_RTL;

	if (eog_scroll_view_is_image_movable (view)) {
		gtk_gesture_set_state (GTK_GESTURE (gesture),
		                       GTK_EVENT_SEQUENCE_DENIED);
		return;
	}

#define PAN_ACTION_DISTANCE 200

	priv = view->priv;
	priv->pan_action = EOG_PAN_ACTION_NONE;
	gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

	if (offset > PAN_ACTION_DISTANCE) {
		if (direction == GTK_PAN_DIRECTION_LEFT)
			priv->pan_action = is_rtl ? EOG_PAN_ACTION_PREV
			                          : EOG_PAN_ACTION_NEXT;
		else
			priv->pan_action = is_rtl ? EOG_PAN_ACTION_NEXT
			                          : EOG_PAN_ACTION_PREV;

	}
#undef PAN_ACTION_DISTANCE
}

static void
pan_gesture_end_cb (GtkGesture       *gesture,
                    GdkEventSequence *sequence,
                    EogScrollView    *view)
{
	EogScrollViewPrivate *priv;

	if (!gtk_gesture_handles_sequence (gesture, sequence))
		return;

	priv = view->priv;

	if (priv->pan_action == EOG_PAN_ACTION_PREV)
		g_signal_emit (view, view_signals [SIGNAL_PREVIOUS_IMAGE], 0);
	else if (priv->pan_action == EOG_PAN_ACTION_NEXT)
		g_signal_emit (view, view_signals [SIGNAL_NEXT_IMAGE], 0);

	priv->pan_action = EOG_PAN_ACTION_NONE;
}

static gboolean
scroll_view_check_angle (gdouble angle,
                         gdouble min,
                         gdouble max,
                         gdouble threshold)
{
	if (min < max) {
		return (angle > min - threshold &&
		        angle < max + threshold);
	} else {
		return (angle < max + threshold ||
		        angle > min - threshold);
	}
}

static EogRotationState
scroll_view_get_rotate_state (EogScrollView *view,
                              gdouble        delta)
{
	EogScrollViewPrivate *priv;

	priv = view->priv;

#define THRESHOLD (G_PI / 16)
	switch (priv->rotate_state) {
	case EOG_ROTATION_0:
		if (scroll_view_check_angle (delta, G_PI * 7 / 4,
		                             G_PI / 4, THRESHOLD))
			return priv->rotate_state;
		break;
	case EOG_ROTATION_90:
		if (scroll_view_check_angle (delta, G_PI / 4,
		                             G_PI * 3 / 4, THRESHOLD))
			return priv->rotate_state;
		break;
	case EOG_ROTATION_180:
		if (scroll_view_check_angle (delta, G_PI * 3 / 4,
		                             G_PI * 5 / 4, THRESHOLD))
			return priv->rotate_state;
		break;
	case EOG_ROTATION_270:
		if (scroll_view_check_angle (delta, G_PI * 5 / 4,
		                             G_PI * 7 / 4, THRESHOLD))
			return priv->rotate_state;
		break;
	default:
		g_assert_not_reached ();
	}

#undef THRESHOLD

	if (scroll_view_check_angle (delta, G_PI / 4, G_PI * 3 / 4, 0))
		return EOG_ROTATION_90;
	else if (scroll_view_check_angle (delta, G_PI * 3 / 4, G_PI * 5 / 4, 0))
		return EOG_ROTATION_180;
	else if (scroll_view_check_angle (delta, G_PI * 5 / 4, G_PI * 7 / 4, 0))
		return EOG_ROTATION_270;

	return EOG_ROTATION_0;
}

static void
rotate_gesture_angle_changed_cb (GtkGestureRotate *rotate,
                                 gdouble           angle,
                                 gdouble           delta,
                                 EogScrollView    *view)
{
	EogRotationState rotate_state;
	EogScrollViewPrivate *priv;
	gint angle_diffs [N_EOG_ROTATIONS][N_EOG_ROTATIONS] = {
	        { 0,   90,  180, 270 },
	        { 270, 0,   90,  180 },
	        { 180, 270, 0,   90 },
	        { 90,  180, 270, 0 }
	};
	gint rotate_angle;

	priv = view->priv;
	rotate_state = scroll_view_get_rotate_state (view, delta);

	if (priv->rotate_state == rotate_state)
		return;

	rotate_angle = angle_diffs[priv->rotate_state][rotate_state];
	g_signal_emit (view, view_signals [SIGNAL_ROTATION_CHANGED], 0, (gdouble) rotate_angle);
	priv->rotate_state = rotate_state;
}

/*==================================

   image loading callbacks

   -----------------------------------*/

/* Use when the pixbuf in the view is changed, to keep a
   reference to it and create its cairo surface. */
static void
update_pixbuf (EogScrollView *view, GdkPixbuf *pixbuf)
{
	EogScrollViewPrivate *priv;

	priv = view->priv;

	if (priv->pixbuf != NULL) {
		g_object_unref (priv->pixbuf);
		priv->pixbuf = NULL;
	}

	priv->pixbuf = pixbuf;

	if (priv->surface) {
		cairo_surface_destroy (priv->surface);
	}
	priv->surface = create_surface_from_pixbuf (view, priv->pixbuf);
}

static void
image_changed_cb (EogImage *img, gpointer data)
{
	update_pixbuf (EOG_SCROLL_VIEW (data), eog_image_get_pixbuf (img));

	_set_zoom_mode_internal (EOG_SCROLL_VIEW (data),
	                         EOG_ZOOM_MODE_SHRINK_TO_FIT);
}

/*===================================
	 public API
  ---------------------------------*/

void
eog_scroll_view_hide_cursor (EogScrollView *view)
{
       eog_scroll_view_set_cursor (view, EOG_SCROLL_VIEW_CURSOR_HIDDEN);
}

void
eog_scroll_view_show_cursor (EogScrollView *view)
{
       eog_scroll_view_set_cursor (view, EOG_SCROLL_VIEW_CURSOR_NORMAL);
}

/* general properties */
void
eog_scroll_view_set_zoom_upscale (EogScrollView *view, gboolean upscale)
{
	EogScrollViewPrivate *priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	priv = view->priv;

	if (priv->upscale != upscale) {
		priv->upscale = upscale;

		if (priv->zoom_mode == EOG_ZOOM_MODE_SHRINK_TO_FIT) {
			set_zoom_fit (view);
			gtk_widget_queue_draw (GTK_WIDGET (priv->display));
		}
	}
}

void
eog_scroll_view_set_antialiasing_in (EogScrollView *view, gboolean state)
{
	EogScrollViewPrivate *priv;
	cairo_filter_t new_interp_type;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	priv = view->priv;

	new_interp_type = state ? CAIRO_FILTER_GOOD : CAIRO_FILTER_NEAREST;

	if (priv->interp_type_in != new_interp_type) {
		priv->interp_type_in = new_interp_type;
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));
		g_object_notify (G_OBJECT (view), "antialiasing-in");
	}
}

void
eog_scroll_view_set_antialiasing_out (EogScrollView *view, gboolean state)
{
	EogScrollViewPrivate *priv;
	cairo_filter_t new_interp_type;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	priv = view->priv;

	new_interp_type = state ? CAIRO_FILTER_GOOD : CAIRO_FILTER_NEAREST;

	if (priv->interp_type_out != new_interp_type) {
		priv->interp_type_out = new_interp_type;
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));
		g_object_notify (G_OBJECT (view), "antialiasing-out");
	}
}

static void
_transp_background_changed (EogScrollView *view)
{
	EogScrollViewPrivate *priv = view->priv;

	if (priv->pixbuf != NULL && gdk_pixbuf_get_has_alpha (priv->pixbuf)) {
		if (priv->background_surface) {
			cairo_surface_destroy (priv->background_surface);
			/* Will be recreated if needed during redraw */
			priv->background_surface = NULL;
		}
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));
	}

}

void
eog_scroll_view_set_transparency_color (EogScrollView *view, GdkRGBA *color)
{
	EogScrollViewPrivate *priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	priv = view->priv;

	if (!_eog_gdk_rgba_equal0 (&priv->transp_color, color)) {
		priv->transp_color = *color;
		if (priv->transp_style == EOG_TRANSP_COLOR)
			_transp_background_changed (view);

		g_object_notify (G_OBJECT (view), "transparency-color");
	}
}

void
eog_scroll_view_set_transparency (EogScrollView        *view,
                                  EogTransparencyStyle  style)
{
	EogScrollViewPrivate *priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	priv = view->priv;

	if (priv->transp_style != style) {
		priv->transp_style = style;
		_transp_background_changed (view);
		g_object_notify (G_OBJECT (view), "transparency-style");
	}
}

/* zoom api */

static double preferred_zoom_levels[] = {
        1.0 / 100, 1.0 / 50, 1.0 / 20,
        1.0 / 10.0, 1.0 / 5.0, 1.0 / 3.0, 1.0 / 2.0, 1.0 / 1.5,
        1.0, 1 / 0.75, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
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
		zoom = priv->zoom * priv->zoom_multiplier;
	}
	else {
		int i;
		int index = -1;

		for (i = 0; i < n_zoom_levels; i++) {
			if (preferred_zoom_levels [i] - priv->zoom
			                > DOUBLE_EQUAL_MAX_DIFF) {
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
		zoom = priv->zoom / priv->zoom_multiplier;
	}
	else {
		int i;
		int index = -1;

		for (i = n_zoom_levels - 1; i >= 0; i--) {
			if (priv->zoom - preferred_zoom_levels [i]
			                > DOUBLE_EQUAL_MAX_DIFF) {
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

static void
eog_scroll_view_zoom_fit (EogScrollView *view)
{
	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	set_zoom_fit (view);
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

gboolean
eog_scroll_view_get_zoom_is_min (EogScrollView *view)
{
	g_return_val_if_fail (EOG_IS_SCROLL_VIEW (view), FALSE);

	set_minimum_zoom_factor (view);

	return DOUBLE_EQUAL (view->priv->zoom, MIN_ZOOM_FACTOR) ||
	       DOUBLE_EQUAL (view->priv->zoom, view->priv->min_zoom);
}

gboolean
eog_scroll_view_get_zoom_is_max (EogScrollView *view)
{
	g_return_val_if_fail (EOG_IS_SCROLL_VIEW (view), FALSE);

	return DOUBLE_EQUAL (view->priv->zoom, MAX_ZOOM_FACTOR);
}

static void
display_next_frame_cb (EogImage *image, gint delay, gpointer data)
{
	EogScrollViewPrivate *priv;
	EogScrollView *view;

	if (!EOG_IS_SCROLL_VIEW (data))
		return;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;

	update_pixbuf (view, eog_image_get_pixbuf (image));

	gtk_widget_queue_draw (GTK_WIDGET (priv->display));
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

	if (priv->image != NULL) {
		free_image_resources (view);
	}
	g_assert (priv->image == NULL);
	g_assert (priv->pixbuf == NULL);

	/* priv->progressive_state = PROGRESSIVE_NONE; */
	if (image != NULL) {
		eog_image_data_ref (image);

		if (priv->pixbuf == NULL) {
			update_pixbuf (view, eog_image_get_pixbuf (image));
			/* priv->progressive_state = PROGRESSIVE_NONE; */
			_set_zoom_mode_internal (view,
			                         EOG_ZOOM_MODE_SHRINK_TO_FIT);

		}

		priv->image_changed_id = g_signal_connect (image, "changed",
		                                           (GCallback) image_changed_cb, view);
		if (eog_image_is_animation (image) == TRUE ) {
			eog_image_start_animation (image);
			priv->frame_changed_id = g_signal_connect (image, "next-frame",
			                                            (GCallback) display_next_frame_cb, view);
		}
	} else {
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));
	}

	priv->image = image;

	g_object_notify (G_OBJECT (view), "image");
	update_adjustment_values (view);
}

/**
 * eog_scroll_view_get_image:
 * @view: An #EogScrollView.
 *
 * Gets the currently displayed #EogImage.
 *
 * Returns: (transfer full): An #EogImage.
 **/
EogImage*
eog_scroll_view_get_image (EogScrollView *view)
{
	EogImage *img;

	g_return_val_if_fail (EOG_IS_SCROLL_VIEW (view), NULL);

	img = view->priv->image;

	if (img != NULL)
		g_object_ref (img);

	return img;
}

/*===================================
    object creation/freeing
  ---------------------------------*/

static gboolean
sv_string_to_rgba_mapping (GValue   *value,
                            GVariant *variant,
                            gpointer  user_data)
{
	GdkRGBA color;

	g_return_val_if_fail (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING), FALSE);

	if (gdk_rgba_parse (&color, g_variant_get_string (variant, NULL))) {
		g_value_set_boxed (value, &color);
		return TRUE;
	}

	return FALSE;
}

static GVariant*
sv_rgba_to_string_mapping (const GValue       *value,
                            const GVariantType *expected_type,
                            gpointer            user_data)
{
	GVariant *variant = NULL;
	GdkRGBA *color;
	gchar *hex_val;

	g_return_val_if_fail (G_VALUE_TYPE (value) == GDK_TYPE_RGBA, NULL);
	g_return_val_if_fail (g_variant_type_equal (expected_type, G_VARIANT_TYPE_STRING), NULL);

	color = g_value_get_boxed (value);
	hex_val = gdk_rgba_to_string(color);
	variant = g_variant_new_string (hex_val);
	g_free (hex_val);

	return variant;
}

static void
_clear_overlay_timeout (EogScrollView *view)
{
	EogScrollViewPrivate *priv = view->priv;

	if (priv->overlay_timeout_source != NULL) {
		g_source_unref (priv->overlay_timeout_source);
		g_source_destroy (priv->overlay_timeout_source);
	}

	priv->overlay_timeout_source = NULL;
}
static gboolean
_overlay_timeout_cb (gpointer data)
{
	EogScrollView *view = EOG_SCROLL_VIEW (data);
	EogScrollViewPrivate *priv = view->priv;

	gtk_revealer_set_reveal_child (GTK_REVEALER (priv->left_revealer), FALSE);
	gtk_revealer_set_reveal_child (GTK_REVEALER (priv->right_revealer), FALSE);
	gtk_revealer_set_reveal_child (GTK_REVEALER (priv->bottom_revealer), FALSE);

	_clear_overlay_timeout (view);

	return FALSE;
}
static void
_set_overlay_timeout (EogScrollView *view)
{
	GSource *source;

	_clear_overlay_timeout (view);

	source = g_timeout_source_new (OVERLAY_FADE_OUT_TIMEOUT_MS);
	g_source_set_callback (source, _overlay_timeout_cb, view, NULL);

	g_source_attach (source, NULL);

	view->priv->overlay_timeout_source = source;
}

static gboolean
_enter_overlay_event_cb (GtkWidget *widget,
                         GdkEvent *event,
                         gpointer user_data)
{
	EogScrollView *view = EOG_SCROLL_VIEW (widget);

	_clear_overlay_timeout (view);

	return FALSE;
}

static gboolean
_motion_notify_cb (GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
	EogScrollView *view = EOG_SCROLL_VIEW (user_data);
	EogScrollViewPrivate *priv = view->priv;
	gboolean reveal_child;

	reveal_child = gtk_revealer_get_reveal_child (GTK_REVEALER (priv->left_revealer));

	if (!reveal_child) {
		gtk_revealer_set_reveal_child (GTK_REVEALER (priv->left_revealer), TRUE);
		gtk_revealer_set_reveal_child (GTK_REVEALER (priv->right_revealer), TRUE);
		gtk_revealer_set_reveal_child (GTK_REVEALER (priv->bottom_revealer), TRUE);
	}

	/* reset timeout */
	_set_overlay_timeout(view);

	return FALSE;
}

static void
eog_scroll_view_init (EogScrollView *view)
{
	GSettings *settings;
	EogScrollViewPrivate *priv;

	priv = view->priv = eog_scroll_view_get_instance_private (view);
	settings = g_settings_new (EOG_CONF_VIEW);

	priv->zoom = 1.0;
	priv->min_zoom = MIN_ZOOM_FACTOR;
	priv->zoom_mode = EOG_ZOOM_MODE_SHRINK_TO_FIT;
	priv->upscale = FALSE;
	priv->interp_type_in = CAIRO_FILTER_GOOD;
	priv->interp_type_out = CAIRO_FILTER_GOOD;
	priv->scroll_wheel_zoom = FALSE;
	priv->zoom_multiplier = IMAGE_VIEW_ZOOM_MULTIPLIER;
	priv->image = NULL;
	priv->pixbuf = NULL;
	priv->surface = NULL;
	/* priv->progressive_state = PROGRESSIVE_NONE; */
	priv->transp_style = EOG_TRANSP_BACKGROUND;
	g_warn_if_fail (gdk_rgba_parse(&priv->transp_color, CHECK_BLACK));
	priv->cursor = EOG_SCROLL_VIEW_CURSOR_NORMAL;
	priv->menu = NULL;
	priv->override_bg_color = NULL;
	priv->background_surface = NULL;

	priv->display = g_object_new (GTK_TYPE_DRAWING_AREA,
	                              "can-focus", TRUE,
	                              NULL);

	gtk_widget_add_events (GTK_WIDGET (priv->display),
	                       GDK_EXPOSURE_MASK
	                       | GDK_TOUCHPAD_GESTURE_MASK
	                       | GDK_BUTTON_PRESS_MASK
	                       | GDK_BUTTON_RELEASE_MASK
	                       | GDK_POINTER_MOTION_MASK
	                       | GDK_POINTER_MOTION_HINT_MASK
	                       | GDK_TOUCH_MASK
	                       | GDK_SCROLL_MASK);
	g_signal_connect (G_OBJECT (priv->display), "configure_event",
	                  G_CALLBACK (display_size_change), view);
	g_signal_connect (G_OBJECT (priv->display), "draw",
	                  G_CALLBACK (display_draw), view);
	g_signal_connect (G_OBJECT (priv->display), "map_event",
	                  G_CALLBACK (display_map_event), view);
	g_signal_connect (G_OBJECT (priv->display), "button_press_event",
	                  G_CALLBACK (eog_scroll_view_button_press_event),
	                  view);
	g_signal_connect (G_OBJECT (priv->display), "motion_notify_event",
	                  G_CALLBACK (eog_scroll_view_motion_event), view);
	g_signal_connect (G_OBJECT (priv->display), "button_release_event",
	                  G_CALLBACK (eog_scroll_view_button_release_event),
	                  view);
	g_signal_connect (G_OBJECT (priv->display), "scroll_event",
	                  G_CALLBACK (eog_scroll_view_scroll_event), view);
	g_signal_connect (G_OBJECT (priv->display), "focus_in_event",
	                  G_CALLBACK (eog_scroll_view_focus_in_event), NULL);
	g_signal_connect (G_OBJECT (priv->display), "focus_out_event",
	                  G_CALLBACK (eog_scroll_view_focus_out_event), NULL);

	gtk_drag_source_set (priv->display, GDK_BUTTON1_MASK,
	                     target_table, G_N_ELEMENTS (target_table),
	                     GDK_ACTION_COPY | GDK_ACTION_MOVE |
	                     GDK_ACTION_LINK | GDK_ACTION_ASK);
	g_signal_connect (G_OBJECT (priv->display), "drag-data-get",
	                  G_CALLBACK (view_on_drag_data_get_cb), view);
	g_signal_connect (G_OBJECT (priv->display), "drag-begin",
	                  G_CALLBACK (view_on_drag_begin_cb), view);

	gtk_container_add (GTK_CONTAINER (view), priv->display);

	gtk_widget_set_hexpand (priv->display, TRUE);
	gtk_widget_set_vexpand (priv->display, TRUE);

	g_settings_bind (settings, EOG_CONF_VIEW_USE_BG_COLOR, view,
	                 "use-background-color", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind_with_mapping (settings, EOG_CONF_VIEW_BACKGROUND_COLOR,
	                              view, "background-color",
	                              G_SETTINGS_BIND_DEFAULT,
	                              sv_string_to_rgba_mapping,
	                              sv_rgba_to_string_mapping, NULL, NULL);
	g_settings_bind_with_mapping (settings, EOG_CONF_VIEW_TRANS_COLOR,
	                              view, "transparency-color",
	                              G_SETTINGS_BIND_GET,
	                              sv_string_to_rgba_mapping,
	                              sv_rgba_to_string_mapping, NULL, NULL);
	g_settings_bind (settings, EOG_CONF_VIEW_TRANSPARENCY, view,
	                 "transparency-style", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, EOG_CONF_VIEW_EXTRAPOLATE, view,
	                 "antialiasing-in", G_SETTINGS_BIND_GET);
	g_settings_bind (settings, EOG_CONF_VIEW_INTERPOLATE, view,
	                 "antialiasing-out", G_SETTINGS_BIND_GET);

	g_object_unref (settings);

	priv->zoom_gesture = gtk_gesture_zoom_new (GTK_WIDGET (view));
	g_signal_connect (priv->zoom_gesture, "begin",
	                  G_CALLBACK (zoom_gesture_begin_cb), view);
	g_signal_connect (priv->zoom_gesture, "update",
	                  G_CALLBACK (zoom_gesture_update_cb), view);
	g_signal_connect (priv->zoom_gesture, "end",
	                  G_CALLBACK (zoom_gesture_end_cb), view);
	g_signal_connect (priv->zoom_gesture, "cancel",
	                  G_CALLBACK (zoom_gesture_end_cb), view);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->zoom_gesture),
	                                            GTK_PHASE_CAPTURE);

	priv->rotate_gesture = gtk_gesture_rotate_new (GTK_WIDGET (view));
	gtk_gesture_group (priv->rotate_gesture, priv->zoom_gesture);
	g_signal_connect (priv->rotate_gesture, "angle-changed",
	                  G_CALLBACK (rotate_gesture_angle_changed_cb), view);
	g_signal_connect (priv->rotate_gesture, "begin",
	                  G_CALLBACK (rotate_gesture_begin_cb), view);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->rotate_gesture),
	                                            GTK_PHASE_CAPTURE);

	priv->pan_gesture = gtk_gesture_pan_new (GTK_WIDGET (view),
	                                         GTK_ORIENTATION_HORIZONTAL);
	g_signal_connect (priv->pan_gesture, "pan",
	                  G_CALLBACK (pan_gesture_pan_cb), view);
	g_signal_connect (priv->pan_gesture, "end",
	                  G_CALLBACK (pan_gesture_end_cb), view);
	gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->pan_gesture),
	                                   TRUE);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->pan_gesture),
	                                            GTK_PHASE_CAPTURE);

	/* left revealer */
	priv->left_revealer = gtk_revealer_new ();
	gtk_revealer_set_transition_type (GTK_REVEALER (priv->left_revealer),
	                                  GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
	gtk_revealer_set_transition_duration (GTK_REVEALER (priv->left_revealer),
	                                      OVERLAY_REVEAL_ANIM_TIME);
	gtk_widget_set_halign (priv->left_revealer, GTK_ALIGN_START);
	gtk_widget_set_valign (priv->left_revealer, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_start(priv->left_revealer, 12);
	gtk_widget_set_margin_end(priv->left_revealer, 12);
	gtk_overlay_add_overlay (GTK_OVERLAY (view),
	                         priv->left_revealer);

	/* right revealer */
	priv->right_revealer = gtk_revealer_new ();
	gtk_revealer_set_transition_type (GTK_REVEALER (priv->right_revealer),
	                                  GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
	gtk_revealer_set_transition_duration (GTK_REVEALER (priv->right_revealer),
	                                      OVERLAY_REVEAL_ANIM_TIME);
	gtk_widget_set_halign (priv->right_revealer, GTK_ALIGN_END);
	gtk_widget_set_valign (priv->right_revealer, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_start (priv->right_revealer, 12);
	gtk_widget_set_margin_end (priv->right_revealer, 12);
	gtk_overlay_add_overlay(GTK_OVERLAY (view),
	                        priv->right_revealer);

	/* bottom revealer */
	priv->bottom_revealer = gtk_revealer_new ();
	gtk_revealer_set_transition_type (GTK_REVEALER (priv->bottom_revealer),
	                                  GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
	gtk_revealer_set_transition_duration (GTK_REVEALER (priv->bottom_revealer),
	                                      OVERLAY_REVEAL_ANIM_TIME);
	gtk_widget_set_halign (priv->bottom_revealer, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (priv->bottom_revealer, GTK_ALIGN_END);
	gtk_widget_set_margin_bottom (priv->bottom_revealer, 12);
	gtk_overlay_add_overlay (GTK_OVERLAY (view),
	                         priv->bottom_revealer);

	/* overlaid buttons */
	GtkWidget *button = gtk_button_new_from_icon_name ("go-next-symbolic",
	                                                   GTK_ICON_SIZE_BUTTON);

	gtk_container_add(GTK_CONTAINER (priv->right_revealer), button);
	gtk_actionable_set_action_name(GTK_ACTIONABLE (button), "win.go-next");
	gtk_widget_set_tooltip_text (button,
	                             _("Go to the next image of the gallery"));
	gtk_style_context_add_class (gtk_widget_get_style_context (button),
	                             GTK_STYLE_CLASS_OSD);


	button = gtk_button_new_from_icon_name("go-previous-symbolic",
	                                       GTK_ICON_SIZE_BUTTON);

	gtk_container_add(GTK_CONTAINER (priv->left_revealer), button);
	gtk_actionable_set_action_name (GTK_ACTIONABLE(button),
	                                "win.go-previous");
	gtk_widget_set_tooltip_text (button,
	                             _("Go to the previous image of the gallery"));
	gtk_style_context_add_class (gtk_widget_get_style_context (button),
	                             GTK_STYLE_CLASS_OSD);


	/* group rotate buttons into a box */
	GtkWidget* bottomBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_style_context_add_class (gtk_widget_get_style_context (bottomBox),
	                             GTK_STYLE_CLASS_LINKED);

	button = gtk_button_new_from_icon_name ("object-rotate-left-symbolic",
	                                        GTK_ICON_SIZE_BUTTON);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
	                                "win.rotate-270");
	gtk_widget_set_tooltip_text (button,
	                             _("Rotate the image 90 degrees to the left"));
	gtk_style_context_add_class (gtk_widget_get_style_context (button),
	                             GTK_STYLE_CLASS_OSD);

	gtk_container_add (GTK_CONTAINER (bottomBox), button);

	button = gtk_button_new_from_icon_name ("object-rotate-right-symbolic",
	                                        GTK_ICON_SIZE_BUTTON);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
	                                "win.rotate-90");
	gtk_widget_set_tooltip_text (button,
	                             _("Rotate the image 90 degrees to the right"));
	gtk_style_context_add_class (gtk_widget_get_style_context (button),
	                             GTK_STYLE_CLASS_OSD);
	gtk_container_add (GTK_CONTAINER (bottomBox), button);

	gtk_container_add (GTK_CONTAINER (priv->bottom_revealer), bottomBox);

	/* Display overlay buttons on mouse movement */
	g_signal_connect (priv->display,
	                  "motion-notify-event",
	                  G_CALLBACK (_motion_notify_cb),
	                  view);

	/* Don't hide overlay buttons when above */
	gtk_widget_add_events (GTK_WIDGET (view),
	                       GDK_ENTER_NOTIFY_MASK);
	g_signal_connect (view,
	                  "enter-notify-event",
	                  G_CALLBACK (_enter_overlay_event_cb),
	                  NULL);
}

static void
eog_scroll_view_set_hadjustment (EogScrollView   *view,
                               GtkAdjustment *adjustment)
{
	EogScrollViewPrivate *priv = view->priv;

	if (adjustment && priv->hadj == adjustment)
		return;

	if (priv->hadj != NULL) {
		g_signal_handlers_disconnect_by_func (priv->hadj,
		                                      adjustment_changed_cb,
		                                      view);
		g_object_unref (priv->hadj);
	}

	if (adjustment == NULL)
		adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	priv->hadj = g_object_ref_sink (adjustment);

	g_signal_connect (adjustment, "value-changed",
	                  G_CALLBACK (adjustment_changed_cb),
	                  view);
	adjustment_changed_cb (adjustment, view);
	g_object_notify (G_OBJECT (view), "hadjustment");
}

static void
eog_scroll_view_set_vadjustment (EogScrollView   *view,
                               GtkAdjustment *adjustment)
{
	EogScrollViewPrivate *priv = view->priv;

	if (adjustment && priv->vadj == adjustment)
		return;

	if (priv->vadj != NULL) {
		g_signal_handlers_disconnect_by_func (priv->vadj,
		                                      adjustment_changed_cb,
		                                      view);
		g_object_unref (priv->vadj);
	}

	if (adjustment == NULL)
		adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	priv->vadj = g_object_ref_sink (adjustment);

	g_signal_connect (adjustment, "value-changed",
	                  G_CALLBACK (adjustment_changed_cb),
	                  view);
	adjustment_changed_cb (adjustment, view);
	g_object_notify (G_OBJECT (view), "vadjustment");
}

static void
eog_scroll_view_dispose (GObject *object)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (object));

	view = EOG_SCROLL_VIEW (object);
	priv = view->priv;

	_clear_overlay_timeout (view);
	_clear_hq_redraw_timeout (view);

	if (priv->idle_id != 0) {
		g_source_remove (priv->idle_id);
		priv->idle_id = 0;
	}

	if (priv->background_color != NULL) {
		gdk_rgba_free (priv->background_color);
		priv->background_color = NULL;
	}

	if (priv->override_bg_color != NULL) {
		gdk_rgba_free (priv->override_bg_color);
		priv->override_bg_color = NULL;
	}

	if (priv->background_surface != NULL) {
		cairo_surface_destroy (priv->background_surface);
		priv->background_surface = NULL;
	}

	free_image_resources (view);

	if (priv->zoom_gesture) {
		g_object_unref (priv->zoom_gesture);
		priv->zoom_gesture = NULL;
	}

	if (priv->rotate_gesture) {
		g_object_unref (priv->rotate_gesture);
		priv->rotate_gesture = NULL;
	}

	if (priv->pan_gesture) {
		g_object_unref (priv->pan_gesture);
		priv->pan_gesture = NULL;
	}

	G_OBJECT_CLASS (eog_scroll_view_parent_class)->dispose (object);
}

static void
eog_scroll_view_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (object));

	view = EOG_SCROLL_VIEW (object);
	priv = view->priv;

	switch (property_id) {
	case PROP_ANTIALIAS_IN:
	{
		gboolean filter = (priv->interp_type_in != CAIRO_FILTER_NEAREST);
		g_value_set_boolean (value, filter);
		break;
	}
	case PROP_ANTIALIAS_OUT:
	{
		gboolean filter = (priv->interp_type_out != CAIRO_FILTER_NEAREST);
		g_value_set_boolean (value, filter);
		break;
	}
	case PROP_USE_BG_COLOR:
		g_value_set_boolean (value, priv->use_bg_color);
		break;
	case PROP_BACKGROUND_COLOR:
		//FIXME: This doesn't really handle the NULL color.
		g_value_set_boxed (value, priv->background_color);
		break;
	case PROP_SCROLLWHEEL_ZOOM:
		g_value_set_boolean (value, priv->scroll_wheel_zoom);
		break;
	case PROP_TRANSPARENCY_STYLE:
		g_value_set_enum (value, priv->transp_style);
		break;
	case PROP_ZOOM_MODE:
		g_value_set_enum (value, priv->zoom_mode);
		break;
	case PROP_ZOOM_MULTIPLIER:
		g_value_set_double (value, priv->zoom_multiplier);
		break;
	case PROP_IMAGE:
		g_value_set_object (value, priv->image);
		break;
	case PROP_HADJUSTMENT:
		g_value_set_object (value, priv->hadj);
		break;
	case PROP_VADJUSTMENT:
		g_value_set_object (value, priv->vadj);
		break;
	case PROP_HSCROLL_POLICY:
		g_value_set_enum (value, priv->hscroll_policy);
		break;
	case PROP_VSCROLL_POLICY:
		g_value_set_enum (value, priv->vscroll_policy);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
eog_scroll_view_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
	EogScrollView *view;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (object));

	view = EOG_SCROLL_VIEW (object);

	switch (property_id) {
	case PROP_ANTIALIAS_IN:
		eog_scroll_view_set_antialiasing_in (view, g_value_get_boolean (value));
		break;
	case PROP_ANTIALIAS_OUT:
		eog_scroll_view_set_antialiasing_out (view, g_value_get_boolean (value));
		break;
	case PROP_USE_BG_COLOR:
		eog_scroll_view_set_use_bg_color (view, g_value_get_boolean (value));
		break;
	case PROP_BACKGROUND_COLOR:
	{
		const GdkRGBA *color = g_value_get_boxed (value);
		eog_scroll_view_set_background_color (view, color);
		break;
	}
	case PROP_SCROLLWHEEL_ZOOM:
		eog_scroll_view_set_scroll_wheel_zoom (view, g_value_get_boolean (value));
		break;
	case PROP_TRANSP_COLOR:
		eog_scroll_view_set_transparency_color (view, g_value_get_boxed (value));
		break;
	case PROP_TRANSPARENCY_STYLE:
		eog_scroll_view_set_transparency (view, g_value_get_enum (value));
		break;
	case PROP_ZOOM_MODE:
		eog_scroll_view_set_zoom_mode (view, g_value_get_enum (value));
		break;
	case PROP_ZOOM_MULTIPLIER:
		eog_scroll_view_set_zoom_multiplier (view, g_value_get_double (value));
		break;
	case PROP_IMAGE:
		eog_scroll_view_set_image (view, g_value_get_object (value));
		break;
	case PROP_HADJUSTMENT:
		eog_scroll_view_set_hadjustment (view, g_value_get_object(value));
		break;
	case PROP_VADJUSTMENT:
		eog_scroll_view_set_vadjustment (view, g_value_get_object(value));
		break;
	case PROP_HSCROLL_POLICY:
		if (view->priv->hscroll_policy != g_value_get_enum (value)) {
			view->priv->hscroll_policy = g_value_get_enum (value);
			gtk_widget_queue_resize (GTK_WIDGET (view));
			g_object_notify_by_pspec (object, pspec);
		}
		break;
	case PROP_VSCROLL_POLICY:
		if (view->priv->vscroll_policy != g_value_get_enum (value)) {
			view->priv->vscroll_policy = g_value_get_enum (value);
			gtk_widget_queue_resize (GTK_WIDGET (view));
			g_object_notify_by_pspec (object, pspec);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}


static void
eog_scroll_view_class_init (EogScrollViewClass *klass)
{
	GObjectClass *gobject_class;
	gobject_class = (GObjectClass*) klass;

	gobject_class->dispose = eog_scroll_view_dispose;
	gobject_class->set_property = eog_scroll_view_set_property;
	gobject_class->get_property = eog_scroll_view_get_property;

	/**
	 * EogScrollView:antialiasing-in:
	 *
	 * If %TRUE the displayed image will be filtered in a second pass
	 * while being zoomed in.
	 */
	g_object_class_install_property (
	        gobject_class, PROP_ANTIALIAS_IN,
	        g_param_spec_boolean ("antialiasing-in", NULL, NULL, TRUE,
	                              G_PARAM_READWRITE | G_PARAM_STATIC_NAME));
	/**
	 * EogScrollView:antialiasing-out:
	 *
	 * If %TRUE the displayed image will be filtered in a second pass
	 * while being zoomed out.
	 */
	g_object_class_install_property (
	        gobject_class, PROP_ANTIALIAS_OUT,
	        g_param_spec_boolean ("antialiasing-out", NULL, NULL, TRUE,
	                              G_PARAM_READWRITE | G_PARAM_STATIC_NAME));

	/**
	 * EogScrollView:background-color:
	 *
	 * This is the default background color used for painting the background
	 * of the image view. If set to %NULL the color is determined by the
	 * active GTK theme.
	 */
	g_object_class_install_property (
	        gobject_class, PROP_BACKGROUND_COLOR,
	        g_param_spec_boxed ("background-color", NULL, NULL,
	                            GDK_TYPE_RGBA,
	                            G_PARAM_READWRITE | G_PARAM_STATIC_NAME));

	g_object_class_install_property (
	        gobject_class, PROP_USE_BG_COLOR,
	        g_param_spec_boolean ("use-background-color", NULL, NULL, FALSE,
	                              G_PARAM_READWRITE | G_PARAM_STATIC_NAME));

	/**
	 * EogScrollView:zoom-multiplier:
	 *
	 * The current zoom factor is multiplied with this value + 1.0 when
	 * scrolling with the scrollwheel to determine the next zoom factor.
	 */
	g_object_class_install_property (
	        gobject_class, PROP_ZOOM_MULTIPLIER,
	        g_param_spec_double ("zoom-multiplier", NULL, NULL,
	                             -G_MAXDOUBLE, G_MAXDOUBLE - 1.0, 0.05,
	                             G_PARAM_READWRITE | G_PARAM_STATIC_NAME));

	/**
	 * EogScrollView:scrollwheel-zoom:
	 *
	 * If %TRUE the scrollwheel will zoom the view, otherwise it will be
	 * used for scrolling a zoomed image.
	 */
	g_object_class_install_property (
	        gobject_class, PROP_SCROLLWHEEL_ZOOM,
	        g_param_spec_boolean ("scrollwheel-zoom", NULL, NULL, TRUE,
	                              G_PARAM_READWRITE | G_PARAM_STATIC_NAME));

	/**
	 * EogScrollView:image:
	 *
	 * This is the currently display #EogImage.
	 */
	g_object_class_install_property (
	        gobject_class, PROP_IMAGE,
	        g_param_spec_object ("image", NULL, NULL, EOG_TYPE_IMAGE,
	                             G_PARAM_READWRITE | G_PARAM_STATIC_NAME));

	/**
	 * EogScrollView:transparency-color:
	 *
	 * This is the color used to fill the transparent parts of an image
	 * if #EogScrollView:transparency-style is set to %EOG_TRANSP_COLOR.
	 */
	g_object_class_install_property (
	        gobject_class, PROP_TRANSP_COLOR,
	        g_param_spec_boxed ("transparency-color", NULL, NULL,
	                            GDK_TYPE_RGBA,
	                            G_PARAM_WRITABLE | G_PARAM_STATIC_NAME));

	/**
	 * EogScrollView:transparency-style:
	 *
	 * Determines how to fill the shown image's transparent areas.
	 */
	g_object_class_install_property (
	        gobject_class, PROP_TRANSPARENCY_STYLE,
	        g_param_spec_enum ("transparency-style", NULL, NULL,
	                           EOG_TYPE_TRANSPARENCY_STYLE,
	                           EOG_TRANSP_CHECKED,
	                           G_PARAM_READWRITE | G_PARAM_STATIC_NAME));

	g_object_class_install_property (
	        gobject_class, PROP_ZOOM_MODE,
	        g_param_spec_enum ("zoom-mode", NULL, NULL,
	                           EOG_TYPE_ZOOM_MODE,
	                           EOG_ZOOM_MODE_SHRINK_TO_FIT,
	                           G_PARAM_READWRITE | G_PARAM_STATIC_NAME));

	/* GtkScrollable implementation */
	g_object_class_override_property (gobject_class, PROP_HADJUSTMENT, "hadjustment");
	g_object_class_override_property (gobject_class, PROP_VADJUSTMENT, "vadjustment");
	g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
	g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");

	view_signals [SIGNAL_ZOOM_CHANGED] =
	        g_signal_new ("zoom_changed",
	                      EOG_TYPE_SCROLL_VIEW,
	                      G_SIGNAL_RUN_LAST,
	                      G_STRUCT_OFFSET (EogScrollViewClass, zoom_changed),
	                      NULL, NULL,
	                      g_cclosure_marshal_VOID__DOUBLE,
	                      G_TYPE_NONE, 1,
	                      G_TYPE_DOUBLE);
	view_signals [SIGNAL_ROTATION_CHANGED] =
	        g_signal_new ("rotation-changed",
	                      EOG_TYPE_SCROLL_VIEW,
	                      G_SIGNAL_RUN_LAST,
	                      G_STRUCT_OFFSET (EogScrollViewClass, rotation_changed),
	                      NULL, NULL,
	                      g_cclosure_marshal_VOID__DOUBLE,
	                      G_TYPE_NONE, 1,
	                      G_TYPE_DOUBLE);

	view_signals [SIGNAL_NEXT_IMAGE] =
	        g_signal_new ("next-image",
	                      EOG_TYPE_SCROLL_VIEW,
	                      G_SIGNAL_RUN_LAST,
	                      G_STRUCT_OFFSET (EogScrollViewClass, next_image),
	                      NULL, NULL,
	                      g_cclosure_marshal_VOID__VOID,
	                      G_TYPE_NONE, 0);
	view_signals [SIGNAL_PREVIOUS_IMAGE] =
	        g_signal_new ("previous-image",
	                      EOG_TYPE_SCROLL_VIEW,
	                      G_SIGNAL_RUN_LAST,
	                      G_STRUCT_OFFSET (EogScrollViewClass, previous_image),
	                      NULL, NULL,
	                      g_cclosure_marshal_VOID__VOID,
	                      G_TYPE_NONE, 0);
}

static void
view_on_drag_begin_cb (GtkWidget        *widget,
                       GdkDragContext   *context,
                       gpointer          user_data)
{
	EogScrollView *view;
	EogImage *image;
	GdkPixbuf *thumbnail;
	gint width, height;

	view = EOG_SCROLL_VIEW (user_data);
	image = view->priv->image;

	if (!image)
		return;

	thumbnail = eog_image_get_thumbnail (image);

	if  (thumbnail) {
		width = gdk_pixbuf_get_width (thumbnail);
		height = gdk_pixbuf_get_height (thumbnail);
		gtk_drag_set_icon_pixbuf (context, thumbnail, width/2, height/2);
		g_object_unref (thumbnail);
	}
}

static void
view_on_drag_data_get_cb (GtkWidget        *widget,
                          GdkDragContext   *drag_context,
                          GtkSelectionData *data,
                          guint             info,
                          guint             time,
                          gpointer          user_data)
{
	EogScrollView *view;
	EogImage *image;
	gchar *uris[2];
	GFile *file;

	view = EOG_SCROLL_VIEW (user_data);

	image = view->priv->image;

	if (!image)
		return;

	file = eog_image_get_file (image);
	uris[0] = g_file_get_uri (file);
	uris[1] = NULL;

	gtk_selection_data_set_uris (data, uris);

	g_free (uris[0]);
	g_object_unref (file);
}

GtkWidget*
eog_scroll_view_new (void)
{
	GtkWidget *widget;

	widget = g_object_new (EOG_TYPE_SCROLL_VIEW,
	                       "can-focus", TRUE,
	                       NULL);

	return widget;
}

static void
eog_scroll_view_popup_menu (EogScrollView *view, GdkEventButton *event)
{
	gtk_menu_popup_at_pointer (GTK_MENU (view->priv->menu),
	                           (const GdkEvent*) event);
}

static gboolean
view_on_button_press_event_cb (GtkWidget *view, GdkEventButton *event,
                               gpointer user_data)
{
	/* Ignore double-clicks and triple-clicks */
	if (gdk_event_triggers_context_menu ((const GdkEvent*) event)
	    && event->type == GDK_BUTTON_PRESS)
	{
		eog_scroll_view_popup_menu (EOG_SCROLL_VIEW (view), event);

		return TRUE;
	}

	return FALSE;
}

static gboolean
eog_scroll_view_popup_menu_handler (GtkWidget *widget, gpointer user_data)
{
	eog_scroll_view_popup_menu (EOG_SCROLL_VIEW (widget), NULL);
	return TRUE;
}

void
eog_scroll_view_set_popup (EogScrollView *view,
                           GtkMenu *menu)
{
	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));
	g_return_if_fail (view->priv->menu == NULL);

	view->priv->menu = g_object_ref (GTK_WIDGET (menu));

	gtk_menu_attach_to_widget (GTK_MENU (view->priv->menu),
	                           GTK_WIDGET (view),
	                           NULL);

	g_signal_connect (G_OBJECT (view), "button_press_event",
	                  G_CALLBACK (view_on_button_press_event_cb), NULL);
	g_signal_connect (G_OBJECT (view), "popup-menu",
	                  G_CALLBACK (eog_scroll_view_popup_menu_handler), NULL);
}

static gboolean
_eog_gdk_rgba_equal0 (const GdkRGBA *a, const GdkRGBA *b)
{
	if (a == NULL || b == NULL)
		return (a == b);

	return gdk_rgba_equal (a, b);
}

static gboolean
_eog_replace_gdk_rgba (GdkRGBA **dest, const GdkRGBA *src)
{
	GdkRGBA *old = *dest;

	if (_eog_gdk_rgba_equal0 (old, src))
		return FALSE;

	if (old != NULL)
		gdk_rgba_free (old);

	*dest = (src) ? gdk_rgba_copy (src) : NULL;

	return TRUE;
}

static void
_eog_scroll_view_update_bg_color (EogScrollView *view)
{
	EogScrollViewPrivate *priv = view->priv;

	if (priv->transp_style == EOG_TRANSP_BACKGROUND
	    && priv->background_surface != NULL) {
		/* Delete the SVG background to have it recreated with
		 * the correct color during the next SVG redraw */
		cairo_surface_destroy (priv->background_surface);
		priv->background_surface = NULL;
	}

	gtk_widget_queue_draw (priv->display);
}

void
eog_scroll_view_set_background_color (EogScrollView *view,
                                      const GdkRGBA *color)
{
	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	if (_eog_replace_gdk_rgba (&view->priv->background_color, color))
		_eog_scroll_view_update_bg_color (view);
}

void
eog_scroll_view_override_bg_color (EogScrollView *view,
                                   const GdkRGBA *color)
{
	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	if (_eog_replace_gdk_rgba (&view->priv->override_bg_color, color))
		_eog_scroll_view_update_bg_color (view);
}

void
eog_scroll_view_set_use_bg_color (EogScrollView *view, gboolean use)
{
	EogScrollViewPrivate *priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	priv = view->priv;

	if (use != priv->use_bg_color) {
		priv->use_bg_color = use;

		_eog_scroll_view_update_bg_color (view);

		g_object_notify (G_OBJECT (view), "use-background-color");
	}
}

void
eog_scroll_view_set_scroll_wheel_zoom (EogScrollView *view,
                                       gboolean       scroll_wheel_zoom)
{
	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	if (view->priv->scroll_wheel_zoom != scroll_wheel_zoom) {
		view->priv->scroll_wheel_zoom = scroll_wheel_zoom;
		g_object_notify (G_OBJECT (view), "scrollwheel-zoom");
	}
}

void
eog_scroll_view_set_zoom_multiplier (EogScrollView *view,
                                     gdouble        zoom_multiplier)
{
	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	view->priv->zoom_multiplier = 1.0 + zoom_multiplier;

	g_object_notify (G_OBJECT (view), "zoom-multiplier");
}

/* Helper to cause a redraw even if the zoom mode is unchanged */
static void
_set_zoom_mode_internal (EogScrollView *view, EogZoomMode mode)
{
	gboolean notify = (mode != view->priv->zoom_mode);


	if (mode == EOG_ZOOM_MODE_SHRINK_TO_FIT)
		eog_scroll_view_zoom_fit (view);
	else
		view->priv->zoom_mode = mode;

	if (notify)
		g_object_notify (G_OBJECT (view), "zoom-mode");
}


void
eog_scroll_view_set_zoom_mode (EogScrollView *view, EogZoomMode mode)
{
	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	if (view->priv->zoom_mode == mode)
		return;

	_set_zoom_mode_internal (view, mode);
}

EogZoomMode
eog_scroll_view_get_zoom_mode (EogScrollView *view)
{
	g_return_val_if_fail (EOG_IS_SCROLL_VIEW (view),
	                      EOG_ZOOM_MODE_SHRINK_TO_FIT);

	return view->priv->zoom_mode;
}

static gboolean
eog_scroll_view_get_image_coords (EogScrollView *view, gint *x, gint *y,
                                  gint *width, gint *height)
{
	EogScrollViewPrivate *priv = view->priv;
	GtkAllocation allocation;
	gint scaled_width, scaled_height, xofs, yofs;

	compute_scaled_size (view, priv->zoom, &scaled_width, &scaled_height);

	if (G_LIKELY (width))
		*width = scaled_width;
	if (G_LIKELY (height))
		*height = scaled_height;

	/* If only width and height are needed stop here. */
	if (x == NULL && y == NULL)
		return TRUE;

	gtk_widget_get_allocation (GTK_WIDGET (priv->display), &allocation);

	/* Compute image offsets with respect to the window */

	if (scaled_width <= allocation.width)
		xofs = (allocation.width - scaled_width) / 2;
	else
		xofs = -priv->xofs;

	if (scaled_height <= allocation.height)
		yofs = (allocation.height - scaled_height) / 2;
	else
		yofs = -priv->yofs;

	if (G_LIKELY (x))
		*x = xofs;
	if (G_LIKELY (y))
		*y = yofs;

	return TRUE;
}

/**
 * eog_scroll_view_event_is_over_image:
 * @view: An #EogScrollView that has an image loaded.
 * @ev: A #GdkEvent which must have window-relative coordinates.
 *
 * Tells if @ev's originates from inside the image area. @view must be
 * realized and have an image set for this to work.
 *
 * It only works with #GdkEvent<!-- -->s that supply coordinate data,
 * i.e. #GdkEventButton.
 *
 * Returns: %TRUE if @ev originates from over the image, %FALSE otherwise.
 */
gboolean
eog_scroll_view_event_is_over_image (EogScrollView *view, const GdkEvent *ev)
{
	EogScrollViewPrivate *priv;
	GdkWindow *window;
	gdouble evx, evy;
	gint x, y, width, height;

	g_return_val_if_fail (EOG_IS_SCROLL_VIEW (view), FALSE);
	g_return_val_if_fail (gtk_widget_get_realized(GTK_WIDGET(view)), FALSE);
	g_return_val_if_fail (ev != NULL, FALSE);

	priv = view->priv;
	window = gtk_widget_get_window (GTK_WIDGET (priv->display));

	if (G_UNLIKELY (priv->pixbuf == NULL
	    || window != ((GdkEventAny*) ev)->window))
		return FALSE;

	if (G_UNLIKELY (!gdk_event_get_coords (ev, &evx, &evy)))
		return FALSE;

	if (!eog_scroll_view_get_image_coords (view, &x, &y, &width, &height))
		return FALSE;

	if (evx < x || evy < y || evx > (x + width) || evy > (y + height))
		return FALSE;

	return TRUE;
}
