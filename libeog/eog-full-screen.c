/* Eye of Gnome image viewer - full-screen view mode
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-macros.h>

#include "eog-full-screen.h"
#include "eog-scroll-view.h"

GNOME_CLASS_BOILERPLATE (EogFullScreen,
			 eog_full_screen,
			 GtkWindow,
			 GTK_TYPE_WINDOW);

struct _EogFullScreenPrivate
{
	GtkWidget *view;

	/* Whether we have a keyboard grab */
	guint have_grab : 1;

	GList *image_list;
	GList *current_node;

	gboolean single_image; 
};

#define ZOOM_FACTOR 1.2

/* Show handler for the full screen view */
static void
eog_full_screen_show (GtkWidget *widget)
{
	EogFullScreen *fs;

	fs = EOG_FULL_SCREEN (widget);

	GNOME_CALL_PARENT (GTK_WIDGET_CLASS, show, (widget));

	fs->priv->have_grab = !gdk_keyboard_grab (widget->window,
			                          TRUE, GDK_CURRENT_TIME);
	gtk_grab_add (widget);

	gtk_widget_grab_focus (fs->priv->view);
}

/* Hide handler for the full screen view */
static void
eog_full_screen_hide (GtkWidget *widget)
{
	EogFullScreen *fs;
	
	fs = EOG_FULL_SCREEN (widget);

	if (fs->priv->have_grab) {
		fs->priv->have_grab = FALSE;
		gdk_keyboard_ungrab (GDK_CURRENT_TIME);
	}

	/* clear image */
	eog_scroll_view_set_image (EOG_SCROLL_VIEW (fs->priv->view), 0); 

	GNOME_CALL_PARENT (GTK_WIDGET_CLASS, hide, (widget));

	gtk_widget_destroy (widget);
}

static void
view_image (EogFullScreen *fs, GList *node)
{
	EogFullScreenPrivate *priv;
	EogImage *image = NULL;

	priv = fs->priv;

	if (node == NULL) {
		priv->current_node = priv->image_list;
	}
	else {
		priv->current_node = node;
	}

	image = EOG_IMAGE (priv->current_node->data);

	eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), image);
}

static void
view_image_next (EogFullScreen *fs)
{
	EogFullScreenPrivate *priv;
	GList *node = NULL;
	EogImage *image;

	priv = fs->priv;

	if (priv->current_node == NULL) {
		node = priv->image_list;
	}
	else {
		if (priv->current_node->next == NULL) {
			gtk_widget_hide (GTK_WIDGET (fs));
		}
		else {
			node = priv->current_node->next;
		}
	}
	
	if (node != NULL) {
		priv->current_node = node;
		image = EOG_IMAGE (node->data);
		eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), image);
	}
}

static void
view_image_previous (EogFullScreen *fs)
{
	EogFullScreenPrivate *priv;
	GList *node = NULL;
	EogImage *image;

	priv = fs->priv;

	if (priv->current_node == NULL) {
		node = priv->image_list;
	}
	else {
		if (priv->current_node->prev == NULL) {
			gtk_widget_hide (GTK_WIDGET (fs));
		}
		else {
			node = priv->current_node->prev;
		}
	}
	
	if (node != NULL) {
		priv->current_node = node;
		image = EOG_IMAGE (node->data);
		eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), image);
	}
}


/* Key press handler for the full screen view */
static gint
eog_full_screen_key_press (GtkWidget *widget, GdkEventKey *event)
{
	EogFullScreenPrivate *priv;
	EogFullScreen *fs;
	gint handled;
	gboolean do_hide;

	fs = EOG_FULL_SCREEN (widget);
	priv = fs->priv;

	handled = FALSE;
	do_hide = FALSE;

	switch (event->keyval) {
	case GDK_Q:
	case GDK_q:
	case GDK_Escape:
	case GDK_F11:
		do_hide = handled = TRUE;
		break;

	case GDK_W:
	case GDK_w:
		if (event->state & GDK_CONTROL_MASK) {
			do_hide = handled = TRUE;
		}
		break;

	case GDK_space:
	case GDK_Right:
	case GDK_Down:
		if (!priv->single_image) {
			view_image_next (fs);
			handled = TRUE;
		}
		break;

	case GDK_BackSpace:
	case GDK_Left:
	case GDK_Up:
		if (!priv->single_image) {
			view_image_previous (fs);
			handled = TRUE;
		}
		break;
	};
	
	if (do_hide)
		gtk_widget_hide (widget);
	
	if (!handled) {
		handled = GNOME_CALL_PARENT_WITH_DEFAULT (GTK_WIDGET_CLASS, key_press_event, (widget, event), FALSE);
	}

	return handled;
}

static void
eog_full_screen_destroy (GtkObject *object)
{
	EogFullScreen *fs;
	
	g_return_if_fail (EOG_IS_FULL_SCREEN (object));

	fs = EOG_FULL_SCREEN (object);

	GNOME_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}	

/* Finalize handler for the full screen view */
static void
eog_full_screen_finalize (GObject *object)
{
	EogFullScreen *fs;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_FULL_SCREEN (object));

	fs = EOG_FULL_SCREEN (object);

	g_free (fs->priv);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

/* Class initialization function for the full screen mode */
static void
eog_full_screen_class_init (EogFullScreenClass *class)
{
	GtkObjectClass *object_class;
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	gobject_class = (GObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_WINDOW);

	object_class->destroy = eog_full_screen_destroy;
	gobject_class->finalize = eog_full_screen_finalize;

	widget_class->show = eog_full_screen_show;
	widget_class->hide = eog_full_screen_hide;
	widget_class->key_press_event = eog_full_screen_key_press;
}

/* Object initialization function for the full screen view */
static void
eog_full_screen_instance_init (EogFullScreen *fs)
{
	fs->priv = g_new0 (EogFullScreenPrivate, 1);

	gtk_window_set_default_size (
		GTK_WINDOW (fs),
		gdk_screen_width (),
		gdk_screen_height ());
	gtk_window_move (GTK_WINDOW (fs), 0, 0);
}

#if 0
/* Callback used when the image view requests to be closed */
static void
close_item_activated_cb (EogImageView *image_view, gpointer data)
{
	EogFullScreen *fs;

	fs = EOG_FULL_SCREEN (data);

	gtk_widget_hide (GTK_WIDGET (fs));
}
#endif

GtkWidget *
eog_full_screen_new (GList *image_list, EogImage *start_image)
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;
	GtkWidget     *widget;
	GdkColor      black;

	g_return_val_if_fail (image_list != NULL, NULL);

	fs = g_object_new (EOG_TYPE_FULL_SCREEN, 
			   "type", GTK_WINDOW_POPUP, NULL);
	priv = fs->priv;

	widget = eog_scroll_view_new ();
	priv->view = widget;

#if 0
	/* FIXME: the context menu should have a close item */
	g_signal_connect (priv->image_view, "close_item_activated",
			  G_CALLBACK (close_item_activated_cb), fs);
#endif
	if (gdk_color_black (gdk_colormap_get_system (), &black)) {
		gtk_widget_modify_bg (widget, GTK_STATE_NORMAL, &black);
	}

	eog_scroll_view_set_zoom_upscale (EOG_SCROLL_VIEW (widget), TRUE);

	priv->image_list = image_list;
	priv->single_image = (g_list_length (priv->image_list) == 1);
	if (start_image != NULL) 
		priv->current_node = g_list_find (image_list, start_image);
	else
		priv->current_node = NULL;

	gtk_widget_show (widget);
	gtk_container_add (GTK_CONTAINER (fs), widget);

	view_image (fs, priv->current_node);


	return GTK_WIDGET (fs);
}
