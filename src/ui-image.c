/* Eye of Gnome image viewer - scrolling user interface for image views
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
#include <gtk/gtksignal.h>
#include <gtk/gtkwindow.h>
#include "image-view.h"
#include "ui-image.h"
#include "zoom.h"



/* Private part of the UIImage structure */

typedef struct {
	/* Image view widget */
	GtkWidget *view;
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

	GTK_WIDGET_SET_FLAGS (ui, GTK_CAN_FOCUS);

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

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/**
 * ui_image_new:
 * @void:
 *
 * Creates a new scrolling user interface for an image view.
 *
 * Return value: A newly-created scroller for an image view.
 **/
GtkWidget *
ui_image_new (void)
{
	UIImage *ui;

	ui = UI_IMAGE (gtk_widget_new (TYPE_UI_IMAGE,
				       "hadjustment", NULL,
				       "vadjustment", NULL,
				       NULL));
	return ui_image_construct (ui);
}

/* Callback for the zoom_fit signal of the image view */
static void
zoom_fit_cb (ImageView *view, gpointer data)
{
	UIImage *ui;

	ui = UI_IMAGE (data);
	ui_image_zoom_fit (ui);
}

/**
 * ui_image_construct:
 * @ui: An image view scroller.
 * 
 * Constructs a scrolling user interface for an image view by creating the
 * actual image view and inserting it into the scroll frame.
 * 
 * Return value: The same value as @ui.
 **/
GtkWidget *
ui_image_construct (UIImage *ui)
{
	UIImagePrivate *priv;

	g_return_val_if_fail (ui != NULL, NULL);
	g_return_val_if_fail (IS_UI_IMAGE (ui), NULL);

	priv = ui->priv;

	priv->view = image_view_new ();
	gtk_signal_connect (GTK_OBJECT (priv->view), "zoom_fit",
			    GTK_SIGNAL_FUNC (zoom_fit_cb), ui);
	gtk_container_add (GTK_CONTAINER (ui), priv->view);
	gtk_widget_show (priv->view);

	return GTK_WIDGET (ui);
}

/**
 * ui_image_get_image_view:
 * @ui: An image view scroller.
 * 
 * Queries the image view widget that is inside an image view scroller.
 * 
 * Return value: An image view widget.
 **/
GtkWidget *
ui_image_get_image_view (UIImage *ui)
{
	UIImagePrivate *priv;

	g_return_val_if_fail (ui != NULL, NULL);
	g_return_val_if_fail (IS_UI_IMAGE (ui), NULL);

	priv = ui->priv;
	return priv->view;
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
	Image *image;
	int w, h, xthick, ythick;
	int iw, ih;
	double zoom;

	g_return_if_fail (ui != NULL);
	g_return_if_fail (IS_UI_IMAGE (ui));

	priv = ui->priv;

	image = image_view_get_image (IMAGE_VIEW (priv->view));
	if (!image) {
		image_view_set_zoom (IMAGE_VIEW (priv->view), 1.0);
		return;
	}

	w = GTK_WIDGET (ui)->allocation.width;
	h = GTK_WIDGET (ui)->allocation.height;

	if (gtk_scroll_frame_get_shadow_type (GTK_SCROLL_FRAME (ui)) == GTK_SHADOW_NONE)
		xthick = ythick = 0;
	else {
		xthick = GTK_WIDGET (ui)->style->klass->xthickness;
		ythick = GTK_WIDGET (ui)->style->klass->ythickness;
	}

	if (image->pixbuf) {
		iw = gdk_pixbuf_get_width (image->pixbuf);
		ih = gdk_pixbuf_get_height (image->pixbuf);
	} else
		iw = ih = 0;

	zoom = zoom_fit_scale (w - 2 * xthick, h - 2 * ythick, iw, ih, TRUE);
	image_view_set_zoom (IMAGE_VIEW (priv->view), zoom);
}
