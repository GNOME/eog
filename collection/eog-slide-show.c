/* Eye of Gnome collection view - slide show class
 *
 * Copyright (C) 2002 The Free Software Foundation
 *
 * Author: Jens Finke <jens@gnome.org>
 *         Federico Mena-Quintero <federico@gimp.org>
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>

#include "eog-slide-show.h"

#include <image-view.h>
#include <ui-image.h>
#include <libgnome/gnome-macros.h>

GNOME_CLASS_BOILERPLATE (EogSlideShow,
			 eog_slide_show,
			 GtkWindow,
			 GTK_TYPE_WINDOW);

struct _EogSlideShowPrivate
{
	EogCollectionModel *model;	
	GtkWidget *ui_image;

	/* whole list of CImage objects */
	GList *list;  
	/* Iterator, pointer into the list of the image
	 *  displayed currently.
	 */
	GList *it;

	/* Whether we have a keyboard grab */
	guint have_grab : 1;
};

#define ZOOM_FACTOR 1.2

static void
display_image (EogSlideShow *show, CImage *image)
{
	EogSlideShowPrivate *priv;
	GnomeVFSURI *uri;
	GdkPixbuf *pixbuf;
	GtkWidget *image_view;
	
	priv = show->priv;
	
	uri = cimage_get_uri (image);
	 
	pixbuf = gdk_pixbuf_new_from_file (gnome_vfs_uri_get_path (uri), NULL);
	
	image_view = ui_image_get_image_view (UI_IMAGE (priv->ui_image));

	if (pixbuf != NULL) {
		image_view_set_pixbuf (IMAGE_VIEW (image_view), pixbuf);

		if (gdk_pixbuf_get_width (pixbuf) > GTK_WIDGET (show)->allocation.width ||
		    gdk_pixbuf_get_height (pixbuf) > GTK_WIDGET (show)->allocation.height)
		{
		    
			ui_image_zoom_fit (UI_IMAGE (priv->ui_image));
		}
		else {
			image_view_set_zoom (IMAGE_VIEW (image_view), 1.0, 1.0, FALSE, 0, 0);
		}
		g_object_unref (pixbuf);
	}

	gnome_vfs_uri_unref (uri);
}

static void
set_next_image (EogSlideShow *show)
{
	EogSlideShowPrivate *priv;
	CImage *image;

	priv = show->priv;

	if (priv->it == NULL || priv->it->next == NULL) {
		gtk_widget_hide (GTK_WIDGET (show));
		return;
	}

	priv->it = priv->it->next;

	image = (CImage*) priv->it->data;

	display_image (show, image);
}

static void
set_previous_image (EogSlideShow *show)
{
	EogSlideShowPrivate *priv;
	CImage *image;

	priv = show->priv;

	if (priv->it == NULL || priv->it->prev == NULL) {
		gtk_widget_hide (GTK_WIDGET (show));
		return;
	}

	priv->it = priv->it->prev;

	image = (CImage*) priv->it->data;

	display_image (show, image);
}

/* Show handler for the full screen view */
static void
eog_slide_show_show (GtkWidget *widget)
{
	EogSlideShow *show;
	EogSlideShowPrivate *priv;
	GtkWidget *image_view;
	GdkPixbuf *pixbuf;
	CImage *image;

	show = EOG_SLIDE_SHOW (widget);
	priv = show->priv;

	if (GTK_WIDGET_CLASS (parent_class)->show)
		(* GTK_WIDGET_CLASS (parent_class)->show) (widget);

	priv->have_grab = !gdk_keyboard_grab (widget->window,
					      TRUE, GDK_CURRENT_TIME);
	gtk_grab_add (widget);

	if (priv->it != NULL && priv->it->data != NULL) {
		image = (CImage*) priv->it->data;
		display_image (show, image);
	}

	image_view = ui_image_get_image_view (UI_IMAGE (priv->ui_image));

	gtk_widget_grab_focus (image_view);
}

/* Hide handler for the full screen view */
static void
eog_slide_show_hide (GtkWidget *widget)
{
	EogSlideShow *fs;
	
	fs = EOG_SLIDE_SHOW (widget);

	if (fs->priv->have_grab) {
		fs->priv->have_grab = FALSE;
		gdk_keyboard_ungrab (GDK_CURRENT_TIME);
	}

	GNOME_CALL_PARENT (GTK_WIDGET_CLASS, show, (widget));

	gtk_widget_destroy (widget);
}

/* Key press handler for the full screen view */
static gint
eog_slide_show_key_press (GtkWidget *widget, GdkEventKey *event)
{
	gboolean do_hide;
	gboolean result;
	EogSlideShowPrivate *priv;
	GdkPixbuf *pixbuf;

	priv = EOG_SLIDE_SHOW (widget)->priv;

	do_hide = FALSE;

	switch (event->keyval) {
	case GDK_Q:
	case GDK_q:
	case GDK_Escape:
	case GDK_F11:
		do_hide = TRUE;
		break;

	case GDK_W:
	case GDK_w:
		if (event->state & GDK_CONTROL_MASK)
			do_hide = TRUE;
		break;

	case GDK_space:
	case GDK_Right:
	case GDK_Down:
		set_next_image (EOG_SLIDE_SHOW (widget));
		break;

	case GDK_BackSpace:
	case GDK_Left:
	case GDK_Up:
		set_previous_image (EOG_SLIDE_SHOW (widget));
		break;

	default:
		result = FALSE;
		if (GTK_WIDGET_CLASS (parent_class)->key_press_event)
			result = (* GTK_WIDGET_CLASS (parent_class)->key_press_event) (widget, event);
		return result;
	}

	if (do_hide)
		gtk_widget_hide (widget);

	return TRUE;
}

static void
eog_slide_show_dispose (GObject *object)
{
	EogSlideShow *show;
	
	g_return_if_fail (EOG_IS_SLIDE_SHOW (object));

	show = EOG_SLIDE_SHOW (object);

	if (show->priv->model) {
		g_object_unref (G_OBJECT (show->priv->model));
		show->priv->model = NULL;
	}

	if (show->priv->list)
		g_list_free (show->priv->list);
	show->priv->list = NULL;

	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}	

/* Finalize handler for the full screen view */
static void
eog_slide_show_finalize (GObject *object)
{
	EogSlideShow *show;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_SLIDE_SHOW (object));

	show = EOG_SLIDE_SHOW (object);

	g_free (show->priv);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

/* Class initialization function for the full screen mode */
static void
eog_slide_show_class_init (EogSlideShowClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_WINDOW);

	object_class->dispose = eog_slide_show_dispose;
	object_class->finalize = eog_slide_show_finalize;

	widget_class->show = eog_slide_show_show;
	widget_class->hide = eog_slide_show_hide;
	widget_class->key_press_event = eog_slide_show_key_press;
}

/* Object initialization function for the full screen view */
static void
eog_slide_show_instance_init (EogSlideShow *show)
{
	show->priv = g_new0 (EogSlideShowPrivate, 1);
	show->priv->list = NULL;

	gtk_window_set_default_size ( GTK_WINDOW (show),
				      gdk_screen_width (),
				      gdk_screen_height ());
	gtk_window_move (GTK_WINDOW (show), 0, 0);
}

static gboolean
list_cb (EogCollectionModel *model, CImage *image, gpointer data)
{
	EogSlideShowPrivate *priv;
	
	priv = (EogSlideShowPrivate*) data;

	priv->list = g_list_append (priv->list, image);

	return TRUE;
}

static gboolean
list_selected_cb (EogCollectionModel *model, CImage *image, gpointer data)
{
	EogSlideShowPrivate *priv;
	
	priv = (EogSlideShowPrivate*) data;

	if (cimage_is_selected (image))
		priv->list = g_list_append (priv->list, image);

	return TRUE;	
}

GtkWidget *
eog_slide_show_new (EogCollectionModel *model)
{
	EogSlideShow  *show;
	EogSlideShowPrivate *priv;

	g_return_val_if_fail (EOG_IS_COLLECTION_MODEL (model), NULL);

	show = g_object_new (EOG_TYPE_SLIDE_SHOW, 
			     "type", GTK_WINDOW_POPUP, NULL);

	priv = show->priv;

	priv->model = model;
	g_object_ref (G_OBJECT (model));

	if (eog_collection_model_get_selected_length (model) > 0) {
		eog_collection_model_foreach (model, list_selected_cb, priv);
	} 
	else {
		eog_collection_model_foreach (model, list_cb, priv);
	}
	priv->it = priv->list;

	priv->ui_image = ui_image_new ();
	gtk_widget_show (priv->ui_image);
	gtk_container_add (GTK_CONTAINER (show), priv->ui_image);

	return GTK_WIDGET (show);
}
