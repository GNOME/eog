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
#include <libgnome/gnome-i18n.h>
#include "image-view.h"
#include "ui-image.h"
#include "zoom.h"



/* Private part of the UIImage structure */

struct _UIImagePrivate {
	/* Image view widget */
	GtkWidget *view;

	/* Idle handler ID for resetting the scrollbar policy; see the comment
	 * in ui_image_zoom_fit().
	 */
	guint idle_id;
};



static void ui_image_class_init (UIImageClass *class);
static void ui_image_init (UIImage *ui);
static void ui_image_finalize (GObject *object);

static GtkScrolledWindowClass *parent_class;



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
	static GType ui_image_type = 0;

	if (!ui_image_type) {
		static const GTypeInfo ui_image_info = {
			sizeof (UIImageClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ui_image_class_init,
			NULL,
			NULL,
			sizeof (UIImage),
			0,
			(GInstanceInitFunc) ui_image_init,
			NULL,
		};

		ui_image_type = g_type_register_static (gtk_scrolled_window_get_type (), 
							"UIImage",
							&ui_image_info, 0);
	}

	return ui_image_type;
}

/* Class initialization function for an image view */
static void
ui_image_class_init (UIImageClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ui_image_finalize;
}

/* Object initialization function for an image view */
static void
ui_image_init (UIImage *ui)
{
	UIImagePrivate *priv;

	priv = g_new0 (UIImagePrivate, 1);
	ui->priv = priv;

	GTK_WIDGET_SET_FLAGS (ui, GTK_CAN_FOCUS);

 	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (ui), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ui),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
}

/* Destroy handler for an image view */
static void
ui_image_finalize (GObject *object)
{
	UIImage *ui;
	UIImagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_UI_IMAGE (object));

	ui = UI_IMAGE (object);
	priv = ui->priv;

	if (priv->idle_id) {
		g_source_remove (priv->idle_id);
		priv->idle_id = 0;
	}

	g_free (priv);
	ui->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/**
 * ui_image_new:
 *
 * Creates a new scrolling user interface for an image view.
 *
 * Return value: A newly-created scroller for an image view.
 **/
GtkWidget *
ui_image_new (void)
{
	UIImage *ui;

	ui = UI_IMAGE (g_object_new (TYPE_UI_IMAGE,
				     "hadjustment", 
				     GTK_ADJUSTMENT (gtk_object_new (GTK_TYPE_ADJUSTMENT, NULL)),
				     "vadjustment",
				     GTK_ADJUSTMENT (gtk_object_new (GTK_TYPE_ADJUSTMENT, NULL)),
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

	g_signal_connect (priv->view, "zoom_fit",
			  G_CALLBACK (zoom_fit_cb), ui);

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

/* Idle handler to reset the scrollbar policy; see the comment in
 *  ui_image_zoom_fit().
 */
static gboolean
set_policy_idle_cb (gpointer data)
{
	UIImage *ui;
	UIImagePrivate *priv;

	ui = UI_IMAGE (data);
	priv = ui->priv;

	priv->idle_id = 0;

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ui),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	return FALSE;
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
	GdkPixbuf *pixbuf;
	int w, h, xthick, ythick;
	int iw, ih;
	double zoom;

	g_return_if_fail (ui != NULL);
	g_return_if_fail (IS_UI_IMAGE (ui));

	priv = ui->priv;

	pixbuf = image_view_get_pixbuf (IMAGE_VIEW (priv->view));
	if (!pixbuf) {
		image_view_set_zoom (IMAGE_VIEW (priv->view), 1.0, 1.0, FALSE, 0, 0);
		return;
	}

	iw = gdk_pixbuf_get_width (pixbuf);
	ih = gdk_pixbuf_get_height (pixbuf);
	g_object_unref (pixbuf);

	w = GTK_WIDGET (ui)->allocation.width;
	h = GTK_WIDGET (ui)->allocation.height;

	if (gtk_scrolled_window_get_shadow_type (GTK_SCROLLED_WINDOW (ui)) == GTK_SHADOW_NONE)
		xthick = ythick = 0;
	else {
		xthick = GTK_WIDGET (ui)->style->xthickness;
		ythick = GTK_WIDGET (ui)->style->ythickness;
	}

	zoom = zoom_fit_scale (w - 2 * xthick, h - 2 * ythick, iw, ih, TRUE);

	/* We have to set the scrollbar policy to NEVER, then change the zoom
	 * factor, and later reset the policy to AUTOMATIC in the idle loop.  If
	 * we just set the zoom factor, we have a bug in the case when there are
	 * visible scrollbars that manifests itself as follows:
	 *
	 * 1. There are scrollbars on the screen because the image doesn't fit.
	 *
	 * 2. The user selects Fit to Screen.
	 *
	 * 3. The image view has an allocation smaller than that of the scrolled
	 *    window: 2 * style->thickness + image_view_allocation + scrollbar_size
	 *
	 * 4. So when the image view sets the new zoom factor, which would fit
	 *    in the scrolled window but *not* in the image view at its current
	 *    size, it naturally sets its adjustments to say, "the image doesn't
	 *    fit".
	 *
	 * 5. So we get scrollbars anyways.
	 *
	 * To fix this, we turn off the scrollbars, set the new zoom factor, and
	 * later, when GTK+ has resized the widgets, we turn the scrollbars back
	 * on --- they will not get displayed as the image *will* fit at that
	 * time.
	 */

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ui),
					GTK_POLICY_NEVER,
					GTK_POLICY_NEVER);

	image_view_set_zoom (IMAGE_VIEW (priv->view), zoom, zoom, FALSE, 0, 0);

	if (!priv->idle_id)
		priv->idle_id = g_idle_add (set_policy_idle_cb, ui);
}
