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
};

#define ZOOM_FACTOR 1.2

/* Show handler for the full screen view */
static void
eog_full_screen_show (GtkWidget *widget)
{
	EogFullScreen *fs;
	GtkWidget *ui_image;
	GtkWidget *image_view;

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

/* Key press handler for the full screen view */
static gint
eog_full_screen_key_press (GtkWidget *widget, GdkEventKey *event)
{
	gint result;
	gboolean do_hide;

	result = FALSE;

	if (GTK_WIDGET_CLASS (parent_class)->key_press_event)
		result = (* GTK_WIDGET_CLASS (parent_class)->key_press_event) (widget, event);

	if (result)
		return result;

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

	default:
		return FALSE;
	}

	if (do_hide)
		gtk_widget_hide (widget);

	return TRUE;
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
eog_full_screen_new (EogImage *image)
{
	EogFullScreen *fs;
	GtkWidget     *widget;
	GtkStyle      *style;
	GdkColor      black;

	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	fs = g_object_new (EOG_TYPE_FULL_SCREEN, 
			   "type", GTK_WINDOW_POPUP, NULL);

	widget = eog_scroll_view_new ();
	fs->priv->view = widget;

#if 0
	/* FIXME: the context menu should have a close item */
	g_signal_connect (fs->priv->image_view, "close_item_activated",
			  G_CALLBACK (close_item_activated_cb), fs);
#endif
	if (gdk_color_black (gdk_colormap_get_system (), &black)) {
		gtk_widget_modify_bg (widget, GTK_STATE_NORMAL, &black);
	}

	eog_scroll_view_set_zoom_upscale (EOG_SCROLL_VIEW (widget), TRUE);
	eog_scroll_view_set_image (EOG_SCROLL_VIEW (widget), image);

	gtk_widget_show (widget);
	gtk_container_add (GTK_CONTAINER (fs), widget);

	return GTK_WIDGET (fs);
}
