/* Eye of Gnome image viewer - user interface for image views
 *
 * Copyright (C) 1999 The Free Software Foundation
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
#include <gtk/gtksignal.h>
#include <gtk/gtkwindow.h>
#include "cursors.h"
#include "image-item.h"
#include "ui-image.h"
#include "zoom.h"



/* Private part of the UIImage structure */

typedef struct {
	/* Canvas used for display */
	GtkWidget *canvas;

	/* Image item for display */
	GnomeCanvasItem *image_item;

	/* Image we are displaying */
	Image *image;

	/* Zoom factor */
	double zoom;

	/* Anchor point for dragging */
	int drag_anchor_x, drag_anchor_y;
	int drag_ofs_x, drag_ofs_y;

	/* Whether we are dragging or not */
	guint dragging : 1;
} UIImagePrivate;



static void ui_image_class_init (UIImageClass *class);
static void ui_image_init (UIImage *ui);
static void ui_image_destroy (GtkObject *object);


static GtkScrollFrameClass *parent_class;



/**
 * ui_image_get_type:
 * @void:
 *
 * Registers the #UIImage class if necessary, and returns the type ID associated
 * to it.
 *
 * Return value: the type ID of the #UIImage class.
 **/
GtkType
ui_image_get_type (void)
{
	static GtkType ui_image_type = 0;

	if (!ui_image_type) {
		static const GtkTypeInfo ui_image_info = {
			"UIImage",
			sizeof (UIImage),
			sizeof (UIImageClass),
			(GtkClassInitFunc) ui_image_class_init,
			(GtkObjectInitFunc) ui_image_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		ui_image_type = gtk_type_unique (gtk_scroll_frame_get_type (), &ui_image_info);
	}

	return ui_image_type;
}

/* Class initialization function for an image view */
static void
ui_image_class_init (UIImageClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (gtk_scroll_frame_get_type ());

	object_class->destroy = ui_image_destroy;
}

/* Object initialization function for an image view */
static void
ui_image_init (UIImage *ui)
{
	UIImagePrivate *priv;

	priv = g_new0 (UIImagePrivate, 1);
	ui->priv = priv;
	priv->zoom = 1.0;

	gtk_scroll_frame_set_shadow_type (GTK_SCROLL_FRAME (ui), GTK_SHADOW_IN);
	gtk_scroll_frame_set_policy (GTK_SCROLL_FRAME (ui),
				     GTK_POLICY_AUTOMATIC,
				     GTK_POLICY_AUTOMATIC);
}

/* Destroy handler for an image view */
static void
ui_image_destroy (GtkObject *object)
{
	UIImage *ui;
	UIImagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_UI_IMAGE (object));

	ui = UI_IMAGE (object);
	priv = ui->priv;

	if (priv->image)
		image_unref (priv->image);

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/**
 * ui_image_new:
 * @void:
 *
 * Creates a new user interface for an image view.
 *
 * Return value: A newly-created image view.
 **/
GtkWidget *
ui_image_new (void)
{
	UIImage *ui;

	ui = UI_IMAGE (gtk_widget_new (TYPE_UI_IMAGE,
				       "hadjustment", NULL,
				       "vadjustment", NULL,
				       NULL));
	ui_image_construct (ui);
	return GTK_WIDGET (ui);
}



/* Signal handlers for the canvas */

/* Called when the canvas in an image view is realized.  We set its background
 * pixmap to NULL so that X won't clear exposed areas and thus be faster.
 */
static void
canvas_realized (GtkWidget *widget, gpointer data)
{
	GdkCursor *cursor;

	gdk_window_set_back_pixmap (GTK_LAYOUT (widget)->bin_window, NULL, FALSE);

	cursor = cursor_get (GTK_LAYOUT (widget)->bin_window, CURSOR_HAND_OPEN);
	gdk_window_set_cursor (GTK_LAYOUT (widget)->bin_window, cursor);
	gdk_cursor_destroy (cursor);
}

/* Button press handler for the canvas.  We simply start dragging. */
static guint
canvas_button_press (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	UIImage *ui;
	UIImagePrivate *priv;
	GdkCursor *cursor;

	ui = UI_IMAGE (data);
	priv = ui->priv;

	if (priv->dragging || event->button != 1)
		return FALSE;

	priv->dragging = TRUE;
	priv->drag_anchor_x = event->x;
	priv->drag_anchor_y = event->y;
	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (priv->canvas),
					 &priv->drag_ofs_x,
					 &priv->drag_ofs_y);

	cursor = cursor_get (GTK_LAYOUT (priv->canvas)->bin_window, CURSOR_HAND_CLOSED);
	gdk_pointer_grab (GTK_LAYOUT (priv->canvas)->bin_window,
			  FALSE,
			  (GDK_POINTER_MOTION_MASK
			   | GDK_POINTER_MOTION_HINT_MASK
			   | GDK_BUTTON_RELEASE_MASK),
			  NULL,
			  cursor,
			  event->time);
	gdk_cursor_destroy (cursor);

	return TRUE;
}

/* Drags the canvas to the specified position */
static void
drag_to (UIImage *ui, int x, int y)
{
	UIImagePrivate *priv;
	int dx, dy;

	priv = ui->priv;

	dx = priv->drag_anchor_x - x;
	dy = priv->drag_anchor_y - y;

	/* Right now this will suck for diagonal movement.  GtkLayout does not
	 * have a way to scroll itself diagonally, i.e. you have to change the
	 * vertical and horizontal adjustments independently, leading to ugly
	 * visual results.  The canvas freezes and thaws the layout in case of
	 * diagonal movement, forcing it to repaint everything.
	 *
	 * I will put in an ugly hack to circumvent this later.
	 */

	gnome_canvas_scroll_to (GNOME_CANVAS (priv->canvas),
				priv->drag_ofs_x + dx,
				priv->drag_ofs_y + dy);
}

/* Button release handler for the canvas.  We terminate dragging. */
static guint
canvas_button_release (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	UIImage *ui;
	UIImagePrivate *priv;

	ui = UI_IMAGE (data);
	priv = ui->priv;

	if (!priv->dragging || event->button != 1)
		return FALSE;

	drag_to (ui, event->x, event->y);
	priv->dragging = FALSE;
	gdk_pointer_ungrab (event->time);

	return TRUE;
}

/* Motion handler for the canvas.  We update the drag offset. */
static guint
canvas_motion (GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	UIImage *ui;
	UIImagePrivate *priv;
	gint x, y;
	GdkModifierType mods;

	ui = UI_IMAGE (data);
	priv = ui->priv;

	if (!priv->dragging)
		return FALSE;

	if (event->is_hint)
		gdk_window_get_pointer (GTK_LAYOUT (priv->canvas)->bin_window, &x, &y, &mods);
	else {
		x = event->x;
		y = event->y;
	}

	drag_to (ui, event->x, event->y);
	return TRUE;
}



/**
 * ui_image_construct:
 * @ui: An image view.
 *
 * Constructs an image view.
 **/
void
ui_image_construct (UIImage *ui)
{
	UIImagePrivate *priv;

	g_return_if_fail (ui != NULL);
	g_return_if_fail (IS_UI_IMAGE (ui));

	priv = ui->priv;

	/* Create the canvas and the image item */

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	priv->canvas = gnome_canvas_new ();
	gtk_signal_connect (GTK_OBJECT (priv->canvas), "realize",
			    GTK_SIGNAL_FUNC (canvas_realized),
			    ui);
	gtk_signal_connect (GTK_OBJECT (priv->canvas), "button_press_event",
			    GTK_SIGNAL_FUNC (canvas_button_press),
			    ui);
	gtk_signal_connect (GTK_OBJECT (priv->canvas), "button_release_event",
			    GTK_SIGNAL_FUNC (canvas_button_release),
			    ui);
	gtk_signal_connect (GTK_OBJECT (priv->canvas), "motion_notify_event",
			    GTK_SIGNAL_FUNC (canvas_motion),
			    ui);

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

	gtk_container_add (GTK_CONTAINER (ui), priv->canvas);
	gtk_widget_show (priv->canvas);

	/* Sigh, set the step_increments by hand */

	g_assert (GTK_LAYOUT (priv->canvas)->vadjustment != NULL);
	g_assert (GTK_LAYOUT (priv->canvas)->hadjustment != NULL);
	GTK_LAYOUT (priv->canvas)->vadjustment->step_increment = 10;
	GTK_LAYOUT (priv->canvas)->hadjustment->step_increment = 10;

	priv->image_item = gnome_canvas_item_new (
		gnome_canvas_root (GNOME_CANVAS (priv->canvas)),
		TYPE_IMAGE_ITEM,
		NULL);
}

/**
 * ui_image_set_image:
 * @ui: An image view.
 * @image: An image structure, or NULL if none.
 *
 * Sets the image to be displayed in an image view.
 **/
void
ui_image_set_image (UIImage *ui, Image *image)
{
	UIImagePrivate *priv;
	GtkWidget *toplevel;
	int w, h;

	g_return_if_fail (ui != NULL);
	g_return_if_fail (IS_UI_IMAGE (ui));

	priv = ui->priv;

	if (priv->image == image)
		return;

	if (image)
		image_ref (image);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (ui));
	if (toplevel && image->filename)
		gtk_window_set_title (GTK_WINDOW (toplevel), g_basename (image->filename));
	
	if (priv->image)
		image_unref (priv->image);

	priv->image = image;

	gnome_canvas_item_set (priv->image_item,
			       "image", image,
			       NULL);

	if (image->pixbuf) {
		w = image->pixbuf->art_pixbuf->width;
		h = image->pixbuf->art_pixbuf->height;
	} else
		w = h = 0;

	gnome_canvas_item_set (priv->image_item,
			       "width", (double) w,
			       "height", (double) h,
			       NULL);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (priv->canvas), 0.0, 0.0, w - 1, h - 1);
}

/**
 * ui_image_set_zoom:
 * @ui: An image view.
 * @zoom: Desired zoom factor.
 *
 * Sets the zoom factor of an image view.
 **/
void
ui_image_set_zoom (UIImage *ui, double zoom)
{
	UIImagePrivate *priv;

	g_return_if_fail (ui != NULL);
	g_return_if_fail (IS_UI_IMAGE (ui));
	g_return_if_fail (zoom > 0.0);

	priv = ui->priv;
	priv->zoom = zoom;
	gnome_canvas_set_pixels_per_unit (GNOME_CANVAS (priv->canvas), zoom);
}

/**
 * ui_image_get_zoom:
 * @ui: An image view.
 *
 * Gets the current zoom factor of an image view.
 *
 * Return value: Current zoom factor.
 **/
double
ui_image_get_zoom (UIImage *ui)
{
	UIImagePrivate *priv;

	g_return_val_if_fail (ui != NULL, -1.0);
	g_return_val_if_fail (IS_UI_IMAGE (ui), -1.0);

	priv = ui->priv;
	return priv->zoom;
}

/**
 * ui_image_zoom_fit:
 * @ui: An image view.
 *
 * Sets the zoom factor to fit the size of an image view.
 **/
void
ui_image_zoom_fit (UIImage *ui)
{
	UIImagePrivate *priv;
	int w, h, xthick, ythick;
	int iw, ih;
	double zoom;

	g_return_if_fail (ui != NULL);
	g_return_if_fail (IS_UI_IMAGE (ui));

	priv = ui->priv;

	w = GTK_WIDGET (ui)->allocation.width;
	h = GTK_WIDGET (ui)->allocation.height;
	xthick = GTK_WIDGET (ui)->style->klass->xthickness;
	ythick = GTK_WIDGET (ui)->style->klass->ythickness;

	if (priv->image->pixbuf) {
		iw = priv->image->pixbuf->art_pixbuf->width;
		ih = priv->image->pixbuf->art_pixbuf->height;
	} else
		iw = ih = 0;

	zoom = zoom_fit_scale (w - 2 * xthick, h - 2 * ythick, iw, ih, TRUE);
	ui_image_set_zoom (ui, zoom);
}
