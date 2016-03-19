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
#define MAX_ZOOM_FACTOR 20
#define MIN_ZOOM_FACTOR 0.02

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
#define CHECK_GRAY "#808080"
#define CHECK_LIGHT "#cccccc"

/* Time used for the realing animation of the overlaid buttons */
#define OVERLAY_REVEAL_ANIM_TIME (500U) /* ms */

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
	PROP_ZOOM_MULTIPLIER
};

/* Private part of the EogScrollView structure */
struct _EogScrollViewPrivate {
	/* some widgets we rely on */
	GtkWidget *display;
	GtkWidget *scrolled_window;
	GtkAdjustment *hadj;
	GtkAdjustment *vadj;
	GtkWidget *menu;

	/* actual image */
	EogImage *image;
	guint image_changed_id;
	guint frame_changed_id;

	/* zoom mode, either ZOOM_MODE_FIT or ZOOM_MODE_FREE */
	EogZoomMode zoom_mode;

	/* whether to allow zoom > 1.0 on zoom fit */
	gboolean upscale;

	/* the actual zoom factor */
	double zoom;

	/* the minimum possible (reasonable) zoom factor */
	double min_zoom;

	/* Interpolation type when zoomed in*/
	cairo_filter_t interp_type_in;

	/* Interpolation type when zoomed out*/
	cairo_filter_t interp_type_out;

	/* Scroll wheel zoom */
	gboolean scroll_wheel_zoom;

	/* Scroll wheel zoom */
	gdouble zoom_multiplier;

	/* dragging stuff */
	double drag_anchor_x;
	double drag_anchor_y;

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
	GtkGesture *drag_gesture;

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


G_DEFINE_TYPE_WITH_PRIVATE (EogScrollView, eog_scroll_view, GTK_TYPE_OVERLAY)

/*===================================
    widget size changing handler &
        util functions
  ---------------------------------*/

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
}

/* Computes the size in pixels of the scaled image */
static void
compute_scaled_size (EogScrollView *view, int *width, int *height)
{
	EogScrollViewPrivate *priv = view->priv;

	if (priv->image) {
		GtkAbstractImage *image = GTK_ABSTRACT_IMAGE (priv->image);
		double scale = gtk_image_view_get_scale (GTK_IMAGE_VIEW (priv->display));
		*width  = floor (gtk_abstract_image_get_width  (image) * scale + 0.5);
		*height = floor (gtk_abstract_image_get_height (image) * scale + 0.5);
	} else {
		*width  = 0;
		*height = 0;
	}
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
		gdk_flush();
	}
}

#define DOUBLE_EQUAL_MAX_DIFF 1e-6
#define DOUBLE_EQUAL(a,b) (fabs (a - b) < DOUBLE_EQUAL_MAX_DIFF)

static inline gboolean
is_zoomed_in (EogScrollView *view)
{
	EogScrollViewPrivate *priv = view->priv;
	double scale = gtk_image_view_get_scale (GTK_IMAGE_VIEW (priv->display));

	return scale - 1.0 > DOUBLE_EQUAL_MAX_DIFF;
}

static inline gboolean
is_zoomed_out (EogScrollView *view)
{
	EogScrollViewPrivate *priv = view->priv;
	double scale = gtk_image_view_get_scale (GTK_IMAGE_VIEW (priv->display));

	return DOUBLE_EQUAL_MAX_DIFF + scale - 1.0 < 0.0;
}

/* Returns wether the image is movable, that means if it is larger than
 * the actual visible area.
 */

static gboolean
is_image_movable (EogScrollView *view)
{
	EogScrollViewPrivate *priv = view->priv;
	GtkAllocation image_view_alloc;

	gtk_widget_get_allocation (priv->display, &image_view_alloc);

	return gtk_adjustment_get_upper (priv->hadj) > image_view_alloc.width ||
	       gtk_adjustment_get_upper (priv->vadj) > image_view_alloc.height;
}

/*===================================
          drawing core
  ---------------------------------*/

static void
get_transparency_params (EogScrollView *view, GdkRGBA *color1, GdkRGBA *color2)
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
}


static cairo_surface_t *
create_background_surface (EogScrollView *view)
{
	int check_size = CHECK_MEDIUM;
	GdkRGBA check_1;
	GdkRGBA check_2;
	cairo_surface_t *surface;

	get_transparency_params (view, &check_1, &check_2);
	surface = gdk_window_create_similar_surface (gtk_widget_get_window (view->priv->display),
						     CAIRO_CONTENT_COLOR_ALPHA,
						     check_size * 2, check_size * 2);
	cairo_t *cr = cairo_create (surface);

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
scroll_to (EogScrollView *view, int x, int y)
{
	EogScrollViewPrivate *priv = view->priv;
	GtkAllocation allocation;
	int xofs, yofs;

	if (!gtk_widget_is_drawable (priv->display))
		goto out;


	/* Check bounds & Compute offsets */
	x = CLAMP (x, 0, gtk_adjustment_get_upper (priv->hadj)
	                 - gtk_adjustment_get_page_size (priv->hadj));
	xofs = x - (int) gtk_adjustment_get_value (priv->hadj);

	y = CLAMP (y, 0, gtk_adjustment_get_upper (priv->vadj)
	                 - gtk_adjustment_get_page_size (priv->vadj));
	yofs = y - (int) gtk_adjustment_get_value (priv->vadj);

	if (xofs == 0 && yofs == 0)
		return;

	gtk_widget_get_allocation (priv->display, &allocation);

	if (abs (xofs) >= allocation.width || abs (yofs) >= allocation.height) {
		gtk_widget_queue_draw (GTK_WIDGET (priv->display));
	}

out:
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
	int _xofs, _yofs;

	priv = view->priv;

	_xofs = (int) gtk_adjustment_get_value (priv->hadj);
	_yofs = (int) gtk_adjustment_get_value (priv->vadj);

	scroll_to (view, _xofs + xofs, _yofs + yofs);
}

static void
set_minimum_zoom_factor (EogScrollView *view)
{
    GtkAbstractImage *image = GTK_ABSTRACT_IMAGE (view->priv->image);
	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));


	view->priv->min_zoom = MAX (1.0 / gtk_abstract_image_get_width (image),
				    MAX(1.0 / gtk_abstract_image_get_height (image),
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
	EogScrollViewPrivate *priv = view->priv;

	zoom = MAX (priv->min_zoom, zoom);
	gtk_image_view_set_scale (GTK_IMAGE_VIEW (priv->display), zoom);
	priv->zoom = zoom;

	g_signal_emit (view, view_signals [SIGNAL_ZOOM_CHANGED], 0, priv->zoom);
}

/* Zooms the image to fit the available allocation */
static void
set_zoom_fit (EogScrollView *view)
{
	EogScrollViewPrivate *priv = view->priv;

	priv->zoom_mode = EOG_ZOOM_MODE_SHRINK_TO_FIT;
	gtk_image_view_set_fit_allocation (GTK_IMAGE_VIEW (priv->display), TRUE);

#if 0

	if (!gtk_widget_get_mapped (GTK_WIDGET (view)))
		return;

	if (priv->image == NULL)
		return;

	gtk_widget_get_allocation (priv->display, &allocation);

	new_zoom = zoom_fit_scale (allocation.width, allocation.height,
                               gtk_abstract_image_get_width (GTK_ABSTRACT_IMAGE (priv->image)),
                               gtk_abstract_image_get_height (GTK_ABSTRACT_IMAGE (priv->image)),
                               priv->upscale);

	if (new_zoom > MAX_ZOOM_FACTOR)
		new_zoom = MAX_ZOOM_FACTOR;
	else if (new_zoom < MIN_ZOOM_FACTOR)
		new_zoom = MIN_ZOOM_FACTOR;

	priv->zoom = new_zoom;
	priv->xofs = 0;
	priv->yofs = 0;

#endif
	g_signal_emit (view, view_signals [SIGNAL_ZOOM_CHANGED], 0, priv->zoom);
}

/*===================================

   internal signal callbacks

  ---------------------------------*/

/* Key press event handler for the image view */
static gboolean
display_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;
	GtkAllocation allocation;
	gboolean do_zoom;
	double zoom;
	gboolean do_scroll;
	int xofs, yofs;
	GdkModifierType modifiers;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;

	do_zoom = FALSE;
	do_scroll = FALSE;
	xofs = yofs = 0;
	zoom = 1.0;

	gtk_widget_get_allocation (priv->display, &allocation);

	modifiers = gtk_accelerator_get_default_mod_mask ();

	switch (event->keyval) {
	case GDK_KEY_Up:
		if ((event->state & modifiers) == GDK_MOD1_MASK) {
			do_scroll = TRUE;
			xofs = 0;
			yofs = -SCROLL_STEP_SIZE;
		}
		break;

	case GDK_KEY_Page_Up:
		if ((event->state & GDK_MOD1_MASK) != 0) {
			do_scroll = TRUE;
			if (event->state & GDK_CONTROL_MASK) {
				xofs = -(allocation.width * 3) / 4;
				yofs = 0;
			} else {
				xofs = 0;
				yofs = -(allocation.height * 3) / 4;
			}
		}
		break;

	case GDK_KEY_Down:
		if ((event->state & modifiers) == GDK_MOD1_MASK) {
			do_scroll = TRUE;
			xofs = 0;
			yofs = SCROLL_STEP_SIZE;
		}
		break;

	case GDK_KEY_Page_Down:
		if ((event->state & GDK_MOD1_MASK) != 0) {
			do_scroll = TRUE;
			if (event->state & GDK_CONTROL_MASK) {
				xofs = (allocation.width * 3) / 4;
				yofs = 0;
			} else {
				xofs = 0;
				yofs = (allocation.height * 3) / 4;
			}
		}
		break;

	case GDK_KEY_Left:
		if ((event->state & modifiers) == GDK_MOD1_MASK) {
			do_scroll = TRUE;
			xofs = -SCROLL_STEP_SIZE;
			yofs = 0;
		}
		break;

	case GDK_KEY_Right:
		if ((event->state & modifiers) == GDK_MOD1_MASK) {
			do_scroll = TRUE;
			xofs = SCROLL_STEP_SIZE;
			yofs = 0;
		}
		break;

	case GDK_KEY_plus:
	case GDK_KEY_equal:
	case GDK_KEY_KP_Add:
		if (!(event->state & modifiers)) {
			do_zoom = TRUE;
			zoom = priv->zoom * priv->zoom_multiplier;
		}
		break;

	case GDK_KEY_minus:
	case GDK_KEY_KP_Subtract:
		if (!(event->state & modifiers)) {
			do_zoom = TRUE;
			zoom = priv->zoom / priv->zoom_multiplier;
		}
		break;

	case GDK_KEY_1:
		if (!(event->state & modifiers)) {
			do_zoom = TRUE;
			zoom = 1.0;
		}
		break;

	default:
		return FALSE;
	}

	if (do_zoom) {
		GdkSeat *seat;
		GdkDevice *device;
		gint x, y;

		seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
		device = gdk_seat_get_pointer (seat);

		gdk_window_get_device_position (gtk_widget_get_window (widget), device,
		                                &x, &y, NULL);
		set_zoom (view, zoom, TRUE, x, y);
	}

	if (do_scroll)
		scroll_by (view, xofs, yofs);

	if(!do_scroll && !do_zoom)
		return FALSE;

	return TRUE;
}

static void
display_size_change (GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
	EogScrollView *view;
	EogScrollViewPrivate *priv;

	view = EOG_SCROLL_VIEW (data);
	priv = view->priv;

	if (gtk_image_view_get_fit_allocation (GTK_IMAGE_VIEW (priv->display))) {
		set_zoom_fit (view);
	} else {
		int scaled_width, scaled_height;
		int xofs = (int) gtk_adjustment_get_value (priv->hadj);
		int yofs = (int) gtk_adjustment_get_value (priv->vadj);
		int x_offset = 0;
		int y_offset = 0;

		compute_scaled_size (view, &scaled_width, &scaled_height);

		if (xofs + event->width > scaled_width)
			x_offset = scaled_width - event->width - xofs;

		if (yofs + event->height > scaled_height)
			y_offset = scaled_height - event->height - yofs;

		scroll_by (view, x_offset, y_offset);
	}
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

	g_return_val_if_fail (EOG_IS_SCROLL_VIEW (data), FALSE);

	view = EOG_SCROLL_VIEW (data);

	priv = view->priv;

	if (priv->image == NULL)
		return TRUE;


	scaled_width  = (int)gtk_adjustment_get_upper (priv->hadj);
	scaled_height = (int)gtk_adjustment_get_upper (priv->vadj);

	gtk_widget_get_allocation (priv->display, &allocation);

	xofs = (allocation.width / 2)  - (scaled_width / 2);
	yofs = (allocation.height / 2) - (scaled_height / 2);

	/* Paint the background */
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
	else
		cairo_set_source (cr, gdk_window_get_background_pattern (gtk_widget_get_window (priv->display)));

	cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
	cairo_fill (cr);

    if (eog_image_has_alpha (priv->image)) {
		if (priv->background_surface == NULL) {
			priv->background_surface = create_background_surface (view);
		}
		cairo_set_source_surface (cr, priv->background_surface, xofs, yofs);
		cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
		cairo_rectangle (cr, xofs, yofs, scaled_width, scaled_height);
		cairo_fill (cr);
	}

	return GDK_EVENT_PROPAGATE;
}

static void
pan_gesture_pan_cb (GtkGesturePan   *gesture,
                    GtkPanDirection  direction,
                    gdouble          offset,
                    EogScrollView   *view)
{
	EogScrollViewPrivate *priv;

	if (is_image_movable (view)) {
		gtk_gesture_set_state (GTK_GESTURE (gesture),
		                       GTK_EVENT_SEQUENCE_DENIED);
		return;
	}

#define PAN_ACTION_DISTANCE 200

	priv = view->priv;
	priv->pan_action = EOG_PAN_ACTION_NONE;
	gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

	if (offset > PAN_ACTION_DISTANCE) {
		if (direction == GTK_PAN_DIRECTION_LEFT ||
		    gtk_widget_get_direction (GTK_WIDGET (view)) == GTK_TEXT_DIR_RTL)
			priv->pan_action = EOG_PAN_ACTION_NEXT;
		else
			priv->pan_action = EOG_PAN_ACTION_PREV;
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

static void
drag_begin_cb (GtkGesture *gesture,
               double      start_x,
               double      start_y,
               gpointer    user_data)
{
	EogScrollView *view = user_data;
	EogScrollViewPrivate *priv = view->priv;


	g_message (__FUNCTION__);

	if (!is_image_movable (view)) {
		g_message ("not movable");
		gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
		return;
	} else
		g_message ("Movable");

	gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);

	g_message (__FUNCTION__);
	priv->drag_anchor_x = gtk_adjustment_get_value (priv->hadj);
	priv->drag_anchor_y = gtk_adjustment_get_value (priv->vadj);

	eog_scroll_view_set_cursor (view, EOG_SCROLL_VIEW_CURSOR_DRAG);
}

static void
drag_update_cb (GtkGesture *gesture,
                double      offset_x,
                double      offset_y,
                gpointer    user_data)
{
	EogScrollViewPrivate *priv = EOG_SCROLL_VIEW (user_data)->priv;
	double new_value_x;
	double new_value_y;

	new_value_x = priv->drag_anchor_x - offset_x;
	new_value_y = priv->drag_anchor_y - offset_y;

	gtk_adjustment_set_value (priv->hadj, new_value_x);
	gtk_adjustment_set_value (priv->vadj, new_value_y);
}

static void
drag_end_cb (GtkGesture *gesture,
             double      offset_x,
             double      offset_y,
             gpointer    user_data)
{
	eog_scroll_view_set_cursor (user_data, EOG_SCROLL_VIEW_CURSOR_NORMAL);
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

		if (gtk_image_view_get_fit_allocation (GTK_IMAGE_VIEW (priv->display))) {
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

    if (priv->image && eog_image_has_alpha (priv->image)) {
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
	} else {
		int i;
		int index = -1;

		for (i = 0; i < n_zoom_levels; i++) {
			if (preferred_zoom_levels [i] - priv->zoom > DOUBLE_EQUAL_MAX_DIFF) {
				index = i;
				break;
			}
		}

		if (index == -1) {
			zoom = priv->zoom;
		} else {
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
	} else {
		int i;
		int index = -1;

		for (i = n_zoom_levels - 1; i >= 0; i--) {
			if (priv->zoom - preferred_zoom_levels [i] > DOUBLE_EQUAL_MAX_DIFF) {
				index = i;
				break;
			}
		}
		if (index == -1) {
			zoom = priv->zoom;
		} else {
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

	return gtk_image_view_get_scale (GTK_IMAGE_VIEW (view->priv->display));
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

void
eog_scroll_view_set_image (EogScrollView *view, EogImage *image)
{
	EogScrollViewPrivate *priv;
	GtkImageView *display;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (view));

	priv = view->priv;
	display = GTK_IMAGE_VIEW (priv->display);

	if (priv->image == image) {
		return;
	}

	if (priv->image != NULL) {
		free_image_resources (view);
	}
	g_assert (priv->image == NULL);

	if (image != NULL) {
		eog_image_data_ref (image);
	}

	priv->image = image;

	/* Disable the transitions in any case here so we don't see a possible
	 * angle transition */
	gtk_image_view_set_transitions_enabled (display, FALSE);
	gtk_image_view_set_angle (display, 0);
	gtk_image_view_set_fit_allocation (display, TRUE);
	gtk_image_view_set_abstract_image (display,
	                                   GTK_ABSTRACT_IMAGE (image));
	gtk_image_view_set_transitions_enabled (display, TRUE);


	g_object_notify (G_OBJECT (view), "image");
}

/**
 * eog_scroll_view_get_image:
 * @view: An #EogScrollView.
 *
 * Gets the the currently displayed #EogImage.
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

gboolean
eog_scroll_view_scrollbars_visible (EogScrollView *view)
{
	return is_image_movable (view);
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

	source = g_timeout_source_new (1000);
	g_source_set_callback (source, _overlay_timeout_cb, view, NULL);

	g_source_attach (source, NULL);

	view->priv->overlay_timeout_source = source;
}

static gboolean
_enter_overlay_event_cb (GtkWidget *widget,
                         GdkEvent  *event,
                         gpointer   user_data)
{
	EogScrollView *view = EOG_SCROLL_VIEW (user_data);

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
display_scale_changed_cb (GtkImageView *display,
                          GParamSpec   *param_spec,
                          gpointer      user_data)
{
	EogScrollView *view = user_data;
	EogScrollViewPrivate *priv = view->priv;
	double scale = gtk_image_view_get_scale (display);

	eog_image_set_view_scale (priv->image, scale);

	if (is_zoomed_in (view)) {
		eog_image_set_interp_type (priv->image, priv->interp_type_in);
	} else if (is_zoomed_out (view)) {
		eog_image_set_interp_type (priv->image, priv->interp_type_out);
	}
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
	//priv->uta = NULL;
	priv->interp_type_in = CAIRO_FILTER_GOOD;
	priv->interp_type_out = CAIRO_FILTER_GOOD;
	priv->scroll_wheel_zoom = FALSE;
	priv->zoom_multiplier = IMAGE_VIEW_ZOOM_MULTIPLIER;
	priv->image = NULL;
	priv->transp_style = EOG_TRANSP_BACKGROUND;
	g_warn_if_fail (gdk_rgba_parse(&priv->transp_color, CHECK_BLACK));
	priv->cursor = EOG_SCROLL_VIEW_CURSOR_NORMAL;
	priv->menu = NULL;
	priv->override_bg_color = NULL;
	priv->background_surface = NULL;

	priv->hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0, 100, 0, 10, 10, 100));
	priv->vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0, 100, 0, 10, 10, 100));

	priv->scrolled_window = gtk_scrolled_window_new (priv->hadj, priv->vadj);

	priv->display = g_object_new (GTK_TYPE_IMAGE_VIEW,
	                              "can-focus", TRUE,
	                              "hexpand", TRUE,
	                              "vexpand", TRUE,
	                              "fit-allocation", TRUE,
	                              NULL);

	g_signal_connect (priv->display, "notify::scale",
	                  G_CALLBACK (display_scale_changed_cb), view);

	gtk_widget_add_events (priv->display,
	                       GDK_BUTTON_PRESS_MASK
	                     | GDK_BUTTON_RELEASE_MASK
	                     | GDK_POINTER_MOTION_MASK
	                     | GDK_POINTER_MOTION_HINT_MASK
	                     | GDK_KEY_PRESS_MASK);
	g_signal_connect (G_OBJECT (priv->display), "configure_event",
			  G_CALLBACK (display_size_change), view);

	g_signal_connect (G_OBJECT (priv->display), "draw",
	                  G_CALLBACK (display_draw), view);

	g_signal_connect (G_OBJECT (priv->display), "focus_in_event",
			  G_CALLBACK (eog_scroll_view_focus_in_event), NULL);
	g_signal_connect (G_OBJECT (priv->display), "focus_out_event",
			  G_CALLBACK (eog_scroll_view_focus_out_event), NULL);

	g_signal_connect (G_OBJECT (view), "key_press_event",
			  G_CALLBACK (display_key_press_event), view);

	gtk_drag_source_set (priv->display, GDK_BUTTON1_MASK,
			     target_table, G_N_ELEMENTS (target_table),
			     GDK_ACTION_COPY | GDK_ACTION_MOVE |
			     GDK_ACTION_LINK | GDK_ACTION_ASK);
	g_signal_connect (G_OBJECT (priv->display), "drag-data-get",
			  G_CALLBACK (view_on_drag_data_get_cb), view);
	g_signal_connect (G_OBJECT (priv->display), "drag-begin",
			  G_CALLBACK (view_on_drag_begin_cb), view);

	gtk_container_add (GTK_CONTAINER (priv->scrolled_window), priv->display);
	gtk_container_add (GTK_CONTAINER (view), priv->scrolled_window);

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




	priv->pan_gesture = gtk_gesture_pan_new (GTK_WIDGET (view), GTK_ORIENTATION_HORIZONTAL);
	g_signal_connect (priv->pan_gesture, "pan",
	                  G_CALLBACK (pan_gesture_pan_cb), view);
	g_signal_connect (priv->pan_gesture, "end",
	                  G_CALLBACK (pan_gesture_end_cb), view);
	gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->pan_gesture), TRUE);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->pan_gesture),
	                                            GTK_PHASE_CAPTURE);

	priv->drag_gesture = gtk_gesture_drag_new (GTK_WIDGET (view));
	g_signal_connect (priv->drag_gesture, "drag-begin",
	                  G_CALLBACK (drag_begin_cb), view);
	g_signal_connect (priv->drag_gesture, "drag-update",
	                  G_CALLBACK (drag_update_cb), view);
	g_signal_connect (priv->drag_gesture, "drag-end",
	                  G_CALLBACK (drag_end_cb), view);

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
	gtk_overlay_add_overlay (GTK_OVERLAY (view),
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

	gtk_container_add (GTK_CONTAINER (priv->right_revealer), button);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "win.go-next");
	gtk_widget_set_tooltip_text (button,
	                             _("Go to the next image of the gallery"));
	gtk_style_context_add_class (gtk_widget_get_style_context (button),
	                             GTK_STYLE_CLASS_OSD);
	g_signal_connect (button, "enter-notify-event", G_CALLBACK (_enter_overlay_event_cb), view);


	button = gtk_button_new_from_icon_name("go-previous-symbolic",
	                                       GTK_ICON_SIZE_BUTTON);

	gtk_container_add(GTK_CONTAINER (priv->left_revealer), button);
	gtk_actionable_set_action_name (GTK_ACTIONABLE(button),
	                                "win.go-previous");
	gtk_widget_set_tooltip_text (button,
	                             _("Go to the previous image of the gallery"));
	gtk_style_context_add_class (gtk_widget_get_style_context (button),
	                             GTK_STYLE_CLASS_OSD);
	g_signal_connect (button, "enter-notify-event", G_CALLBACK (_enter_overlay_event_cb), view);


	/* group rotate buttons into a box */
	GtkWidget* bottom_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_style_context_add_class (gtk_widget_get_style_context (bottom_box),
	                             GTK_STYLE_CLASS_LINKED);

	button = gtk_button_new_from_icon_name ("object-rotate-left-symbolic",
	                                        GTK_ICON_SIZE_BUTTON);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
	                                "win.rotate-270");
	gtk_widget_set_tooltip_text (button,
	                             _("Rotate the image 90 degrees to the left"));
	gtk_style_context_add_class (gtk_widget_get_style_context (button),
	                             GTK_STYLE_CLASS_OSD);
	g_signal_connect (button, "enter-notify-event", G_CALLBACK (_enter_overlay_event_cb), view);
	gtk_container_add (GTK_CONTAINER (bottom_box), button);


	button = gtk_button_new_from_icon_name ("object-rotate-right-symbolic",
	                                        GTK_ICON_SIZE_BUTTON);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
	                                "win.rotate-90");
	gtk_widget_set_tooltip_text (button,
	                             _("Rotate the image 90 degrees to the right"));
	gtk_style_context_add_class (gtk_widget_get_style_context (button),
	                             GTK_STYLE_CLASS_OSD);
	g_signal_connect (button, "enter-notify-event", G_CALLBACK (_enter_overlay_event_cb), view);
	gtk_container_add (GTK_CONTAINER (bottom_box), button);

	gtk_container_add (GTK_CONTAINER (priv->bottom_revealer), bottom_box);

	/* Display overlay buttons on mouse movement */
	g_signal_connect (priv->display,
			  "motion-notify-event",
			  G_CALLBACK (_motion_notify_cb),
			  view);
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

	g_clear_object (&priv->pan_gesture);
	g_clear_object (&priv->drag_gesture);

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
	EogScrollView *view = user_data;
	EogImage *image;
	GdkPixbuf *thumbnail;
	gint width, height;

	image = view->priv->image;

	if (image == NULL)
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
	EogScrollView *view = user_data;
	EogImage *image;
	gchar *uris[2];
	GFile *file;

	image = view->priv->image;

	if (image == NULL)
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
	GtkWidget *popup;
	int button, event_time;

	popup = view->priv->menu;

	if (event) {
		button = event->button;
		event_time = event->time;
	} else {
		button = 0;
		event_time = gtk_get_current_event_time ();
	}

	gtk_menu_popup (GTK_MENU (popup), NULL, NULL, NULL, NULL,
	                button, event_time);
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

		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
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

	view->priv->menu = g_object_ref (menu);

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
	int scaled_width, scaled_height, xofs, yofs;
	int _xofs = (int) gtk_adjustment_get_value (priv->hadj);
	int _yofs = (int) gtk_adjustment_get_value (priv->vadj);

	compute_scaled_size (view, &scaled_width, &scaled_height);

	*width = scaled_width;
	*height = scaled_height;

	gtk_widget_get_allocation (GTK_WIDGET (priv->display), &allocation);

	/* Compute image offsets with respect to the window */

	if (scaled_width <= allocation.width)
		xofs = (allocation.width - scaled_width) / 2;
	else
		xofs = -_xofs;

	if (scaled_height <= allocation.height)
		yofs = (allocation.height - scaled_height) / 2;
	else
		yofs = -_yofs;

	*x = xofs;
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

	if (G_UNLIKELY (priv->image == NULL
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

double
eog_scroll_view_get_angle (EogScrollView *view)
{
	return gtk_image_view_get_angle (GTK_IMAGE_VIEW (view->priv->display));
}

void
eog_scroll_view_set_angle (EogScrollView *view, double angle)
{
	gtk_image_view_set_angle (GTK_IMAGE_VIEW (view->priv->display), angle);
}

