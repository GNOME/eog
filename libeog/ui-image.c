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
#include "image-item.h"
#include "ui-image.h"



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
} UIImagePrivate;



static void ui_image_class_init (UIImageClass *class);
static void ui_image_init (UIImage *ui);
static void ui_image_destroy (GtkObject *object);


static GtkScrollFrameClass *parent_class;



/**
 * ui_image_get_type:
 * @void:
 *
 * Registers the &UIImage class if necessary, and returns the type ID associated
 * to it.
 *
 * Return value: the type ID of the &UIImage class.
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

/* Called when the canvas in an image view is realized.  We set its background
 * pixmap to NULL so that X won't clear exposed areas and thus be faster.
 */
static void
canvas_realized (GtkWidget *widget, gpointer data)
{
	gdk_window_set_back_pixmap (GTK_LAYOUT (widget)->bin_window, NULL, FALSE);
}

/**
 * ui_image_constructd:
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
			    NULL);

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

	gtk_container_add (GTK_CONTAINER (ui), priv->canvas);
	gtk_widget_show (priv->canvas);

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

	g_return_if_fail (ui != NULL);
	g_return_if_fail (IS_UI_IMAGE (ui));

	priv = ui->priv;

	if (priv->image == image)
		return;

	if (image)
		image_ref (image);

	if (priv->image)
		image_unref (image);

	priv->image = image;

	gnome_canvas_item_set (priv->image_item,
			       "image", image,
			       NULL);

	/* FIXME: set the scroll region and zoom */
}
