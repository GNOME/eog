/* Eye of Gnome image viewer - full-screen view mode
 *
 * Copyright (C) 2003 The Free Software Foundation
 *
 * Author: Jens Finke <jens@triq.net>
 *
 * Based on code by: Federico Mena-Quintero <federico@gimp.org>
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
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-macros.h>
#include <gconf/gconf-client.h>

#include "eog-full-screen.h"
#include "eog-scroll-view.h"
#include "cursors.h"
#include "eog-config-keys.h"

GNOME_CLASS_BOILERPLATE (EogFullScreen,
			 eog_full_screen,
			 GtkWindow,
			 GTK_TYPE_WINDOW);

#define POINTER_HIDE_DELAY  2   /* hide the pointer after 2 seconds */
static GQuark FINISHED_QUARK = 0; 
static GQuark FAILED_QUARK = 0; 

typedef enum {
	EOG_DIRECTION_FORWARD,
	EOG_DIRECTION_BACKWARD
} EogDirection;

struct _EogFullScreenPrivate
{
	GtkWidget *view;

	/* Whether we have a keyboard grab */
	guint have_grab : 1;

	/* list of images to show */
	EogImageList *list;

	EogDirection direction;
	gboolean first_image;
	
	/* current visible image iterator */
	EogIter *current;

	/* if the mouse pointer is hidden */
	gboolean cursor_hidden;
	/* seconds since last mouse movement */
	int mouse_move_counter;
	/* timeout id which checks for cursor hiding */
	guint hide_timeout_id;

	/* wether we should loop the sequence */
	EogIter *first_iter;
	gboolean loop;

	/* timeout for switching to the next image */
	gint   switch_time_counter;
	gint   switch_timeout;      /* in seconds */
	guint  switch_timeout_id;
	gboolean switch_pause;      
};

static void connect_image_callbacks (EogFullScreen *fs, EogImage *image);
static void disconnect_image_callbacks (EogImage *image);
static void prepare_load_image (EogFullScreen *fs, EogIter *iter);
static void show_next_image (EogFullScreen *fs);

static gboolean
check_cursor_hide (gpointer data)
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;

	fs = EOG_FULL_SCREEN (data);
	priv = fs->priv;

	priv->mouse_move_counter++;

	if (!priv->cursor_hidden && priv->mouse_move_counter >= POINTER_HIDE_DELAY) {
		/* hide the pointer  */
		cursor_set (GTK_WIDGET (fs), CURSOR_INVISIBLE);

		/* don't call timeout again */
		priv->cursor_hidden = TRUE;
		priv->hide_timeout_id = 0;
	}

	return (!priv->cursor_hidden);
}

static gboolean
eog_full_screen_motion (GtkWidget *widget, GdkEventMotion *event)
{
	EogFullScreenPrivate *priv;
	GdkCursor *cursor;
	gboolean result;
	int x, y;
	GdkModifierType mods;

	priv = EOG_FULL_SCREEN (widget)->priv;
	
	priv->mouse_move_counter = 0;

	if (priv->cursor_hidden) {
		/* show cursor */
		cursor = gdk_cursor_new (GDK_LEFT_PTR);
		gdk_window_set_cursor (GTK_WIDGET (widget)->window, cursor);
		gdk_cursor_unref (cursor);
		priv->cursor_hidden = FALSE;
	}
	
	if (priv->hide_timeout_id == 0) {
		priv->hide_timeout_id = g_timeout_add (1000 /* every second */,
						       check_cursor_hide,
						       widget);
	}

	/* we call so to get also the next motion event */
	gdk_window_get_pointer (GTK_WIDGET (widget)->window, &x, &y, &mods);

	result = GNOME_CALL_PARENT_WITH_DEFAULT (GTK_WIDGET_CLASS, motion_notify_event, (widget, event), FALSE);

	return result;
}

static gboolean
check_automatic_switch (gpointer data)
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;

	fs = EOG_FULL_SCREEN (data);
	priv = fs->priv;

	if (priv->switch_pause)
		return TRUE;

	priv->switch_time_counter++;

	if (priv->switch_time_counter == priv->switch_timeout) {
		priv->direction = EOG_DIRECTION_FORWARD;
		show_next_image (fs);
	}

	return TRUE;
}

/* Show handler for the full screen view */
static void
eog_full_screen_show (GtkWidget *widget)
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;

	fs = EOG_FULL_SCREEN (widget);
	priv = fs->priv;

	GNOME_CALL_PARENT (GTK_WIDGET_CLASS, show, (widget));

	fs->priv->have_grab = !gdk_keyboard_grab (widget->window,
			                          TRUE, GDK_CURRENT_TIME);
	gtk_grab_add (widget);

	gtk_widget_grab_focus (fs->priv->view);

	priv->cursor_hidden = FALSE;
	priv->mouse_move_counter = 0;
        priv->hide_timeout_id = g_timeout_add (1000 /* every second */,
					       check_cursor_hide,
					       fs);

	priv->switch_timeout_id = 0;
	if (priv->switch_timeout > 0) {
		priv->switch_timeout_id = g_timeout_add (1000 /* every second */,
							 check_automatic_switch,
							 fs);
	}
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

	if (fs->priv->switch_timeout_id > 0) {
		g_source_remove (fs->priv->switch_timeout_id);
	}

	GNOME_CALL_PARENT (GTK_WIDGET_CLASS, hide, (widget));
}

static void
show_next_image (EogFullScreen *fs)
{
	EogFullScreenPrivate *priv;
	EogImage *image;
	EogIter *iter_copy;
	gboolean success = FALSE;
	
	priv = fs->priv;

	/* point the current iterator to the next image in the choosen direction */
	if (priv->direction == EOG_DIRECTION_FORWARD) {
		success = eog_image_list_iter_next (priv->list, priv->current, TRUE);

		/* quit the diashow, if the user don't want to loop
		 * and we are pointing to the first image again */
		if (!priv->loop && eog_image_list_iter_equal (priv->list, priv->first_iter, priv->current)) {
			success = FALSE;
		}
	}
	else {
		/* quit the diashow, if the user don't want to loop
		 * and we are pointing currently to the first image */
		if (!priv->loop && eog_image_list_iter_equal (priv->list, priv->first_iter, priv->current)) {
			success = FALSE;
		}
		else {
			success = eog_image_list_iter_prev (priv->list, priv->current, TRUE);
		}
	}

	if (success) {
		/* view next image */
		image = eog_image_list_get_img_by_iter (priv->list, priv->current);
		eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), image);
		priv->switch_time_counter = 0;
		g_object_unref (image);
	}
	else {
		/* quit diashow */
		gtk_widget_hide (GTK_WIDGET (fs));
		return;
	}

	/* preload next image in current direction */
	iter_copy = eog_image_list_iter_copy (priv->list, priv->current);
	if (priv->direction == EOG_DIRECTION_FORWARD) {
		success = eog_image_list_iter_next (priv->list, iter_copy, TRUE);
	}
	else {
		success = eog_image_list_iter_prev (priv->list, iter_copy, TRUE);
	}

	if (success) {
		prepare_load_image (fs, iter_copy);
	}

	g_free (iter_copy);
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
	case SunXK_F36:
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
		if (eog_image_list_length (priv->list) > 1) {
			priv->direction = EOG_DIRECTION_FORWARD;

			show_next_image (fs);
			handled = TRUE;
		}
		break;

	case GDK_P:
	case GDK_p:
		priv->switch_pause = !priv->switch_pause;
		break;

	case GDK_BackSpace:
	case GDK_Left:
	case GDK_Up:
		if (eog_image_list_length (priv->list) > 1) {
			priv->direction = EOG_DIRECTION_BACKWARD;

			show_next_image (fs);
			handled = TRUE;
		}
		break;
	};
	
	if (do_hide) {
		gtk_widget_hide (widget);
	}

	if (!handled) {
		handled = GNOME_CALL_PARENT_WITH_DEFAULT (GTK_WIDGET_CLASS, key_press_event, (widget, event), FALSE);
	}

	return handled;
}

static void
eog_full_screen_destroy (GtkObject *object)
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;
	EogIter *iter;
	gboolean success;
	
	g_return_if_fail (EOG_IS_FULL_SCREEN (object));

	fs = EOG_FULL_SCREEN (object);
	priv = fs->priv;

	if (priv->hide_timeout_id > 0) {
		g_source_remove (priv->hide_timeout_id);
		priv->hide_timeout_id = 0;
	}

	if (priv->current != NULL) {
		g_free (priv->current);
		priv->current = NULL;
	}

	if (priv->list != NULL) {
		/* remove all callback handlers */
		iter = eog_image_list_get_first_iter (priv->list);
		success = (iter != NULL);
		while (success) {
			EogImage *image;
			image = eog_image_list_get_img_by_iter (priv->list, iter);
			disconnect_image_callbacks (image);
			g_object_unref (image);
			success = eog_image_list_iter_next (priv->list, iter, FALSE);
		}
		
		/* free list */
		g_object_unref (priv->list);
		priv->list = NULL;
	}

	if (priv->first_iter != NULL) {
		g_free (priv->first_iter);
		priv->first_iter = NULL;
	}

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
	widget_class->motion_notify_event = eog_full_screen_motion;
}

/* Object initialization function for the full screen view */
static void
eog_full_screen_instance_init (EogFullScreen *fs)
{
	EogFullScreenPrivate *priv;

	priv = g_new0 (EogFullScreenPrivate, 1);
	fs->priv = priv;

	gtk_window_set_default_size (GTK_WINDOW (fs),
				     gdk_screen_width (),
				     gdk_screen_height ()); 
	gtk_window_move (GTK_WINDOW (fs), 0, 0);

	priv->loop = TRUE;
	priv->first_iter = NULL;

	priv->switch_timeout = 0;
	priv->switch_timeout_id = 0;
	priv->switch_time_counter = 0;
	priv->switch_pause = FALSE;
}

static void
image_loading_finished_cb (EogImage *image, gpointer data)
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;
	EogIter *iter_copy;

	g_return_if_fail (EOG_IS_FULL_SCREEN (data));
	
	fs = EOG_FULL_SCREEN (data);

	priv = fs->priv;

	disconnect_image_callbacks (image);

	if (priv->first_image) {
		eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), image);
		priv->first_image = FALSE;

		iter_copy = eog_image_list_iter_copy (priv->list, priv->current);
		if (eog_image_list_iter_next (priv->list, iter_copy, TRUE)) {
			prepare_load_image (fs, iter_copy);
		}
		g_free (iter_copy);

		iter_copy = eog_image_list_iter_copy (priv->list, priv->current);
		if (eog_image_list_iter_prev (priv->list, iter_copy, TRUE)) {
			prepare_load_image (fs, iter_copy);
		}
		g_free (iter_copy);
		
	}
}

static void
image_loading_failed_cb (EogImage *image, const char *message, gpointer data)
{
	disconnect_image_callbacks (image);

	/* FIXME: What should we do if an image loading failed? */
}

static void
disconnect_image_callbacks (EogImage *image)
{
	int id;

	g_return_if_fail (EOG_IS_IMAGE (image));

	if (g_object_get_qdata (G_OBJECT (image), FINISHED_QUARK) != NULL) { 
		id = GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (image), FINISHED_QUARK));
		g_signal_handler_disconnect (G_OBJECT (image), id);
		g_object_set_qdata (G_OBJECT (image), FINISHED_QUARK, NULL);
	}

	if (g_object_get_qdata (G_OBJECT (image), FAILED_QUARK) != NULL) {
		id = GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (image), FAILED_QUARK));
		g_signal_handler_disconnect (G_OBJECT (image), id);
		g_object_set_qdata (G_OBJECT (image), FAILED_QUARK, NULL);
	}
}

static void 
connect_image_callbacks (EogFullScreen *fs, EogImage *image)
{
	int id;

	g_return_if_fail (EOG_IS_IMAGE (image));

	if (g_object_get_qdata (G_OBJECT (image), FINISHED_QUARK) == NULL) {
		id = g_signal_connect (G_OBJECT (image), "loading_finished", 
				       G_CALLBACK (image_loading_finished_cb), fs);
		g_object_set_qdata (G_OBJECT (image), FINISHED_QUARK, GINT_TO_POINTER (id));
	}

	if (g_object_get_qdata (G_OBJECT (image), FAILED_QUARK) == NULL) {
		id = g_signal_connect (G_OBJECT (image), "loading_failed",
				       G_CALLBACK (image_loading_failed_cb), fs);
		g_object_set_qdata (G_OBJECT (image), FAILED_QUARK, GINT_TO_POINTER (id));
	}
}

static void
prepare_load_image (EogFullScreen *fs, EogIter *iter)
{
	EogFullScreenPrivate *priv;
	EogImage *image;

	priv = fs->priv;
	
	image = eog_image_list_get_img_by_iter (priv->list, iter);
	connect_image_callbacks (fs, image);

	eog_image_load (image, EOG_IMAGE_LOAD_COMPLETE);

	g_object_unref (image);
}

static void
prepare_data (EogFullScreen *fs, EogImageList *image_list, EogImage *start_image)
{
	EogFullScreenPrivate *priv;

	priv = fs->priv;
       
	g_assert (eog_image_list_length (image_list) > 0);

	priv->list = g_object_ref (image_list);
	priv->current = NULL;

	/* determine first image to show */
	if (start_image != NULL) 
		priv->current = eog_image_list_get_iter_by_img (image_list, start_image);
	
	if (priv->current == NULL) 
		priv->current = eog_image_list_get_first_iter (image_list);

	/* special case if we only have one image */
	if (eog_image_list_length (image_list) == 1) {
		EogImage *single_image;
		single_image = eog_image_list_get_img_by_iter (image_list, priv->current);
		eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), 
					   single_image);
		g_object_unref (single_image);
		priv->switch_timeout = 0; /* disable automatic switching always */
	}
	else {
		priv->direction = EOG_DIRECTION_FORWARD;
		priv->first_iter = eog_image_list_iter_copy (image_list, priv->current);
		priv->first_image = TRUE;
		prepare_load_image (fs, priv->current);
	}
}

GtkWidget *
eog_full_screen_new (EogImageList *image_list, EogImage *start_image)
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;
	GtkWidget     *widget;
	GtkStyle      *style;
	GConfClient   *client;
	gboolean       upscale = TRUE;

	g_return_val_if_fail (image_list != NULL, NULL);

	fs = g_object_new (EOG_TYPE_FULL_SCREEN, 
			   "type", GTK_WINDOW_POPUP, NULL);
	priv = fs->priv;

	widget = eog_scroll_view_new ();
	priv->view = widget;

	/* ensure black background */
	style = gtk_widget_get_style (widget);
	style->fg    [GTK_STATE_NORMAL] = style->black;
	style->fg_gc [GTK_STATE_NORMAL] = style->black_gc;
	style->bg    [GTK_STATE_NORMAL] = style->black;
	style->bg_gc [GTK_STATE_NORMAL] = style->black_gc;
	if (style->bg_pixmap [GTK_STATE_NORMAL])
		gdk_pixmap_unref (style->bg_pixmap [GTK_STATE_NORMAL]);
	style->bg_pixmap [GTK_STATE_NORMAL] = NULL;

	gtk_widget_set_style (widget, style);
	gtk_widget_ensure_style (widget);

	gtk_widget_show (widget);
	gtk_container_add (GTK_CONTAINER (fs), widget);

	if (FINISHED_QUARK == 0)
		FINISHED_QUARK = g_quark_from_static_string ("EogFullScreen Finished Callback");
	
	if (FAILED_QUARK == 0) 
		FAILED_QUARK = g_quark_from_static_string ("EogFullScreen Failed Callback");

	/* read configuration */
	client = gconf_client_get_default ();
	priv->loop = gconf_client_get_bool (client, EOG_CONF_FULLSCREEN_LOOP, NULL);
	priv->switch_timeout = gconf_client_get_int (client, EOG_CONF_FULLSCREEN_SECONDS, NULL);
	upscale = gconf_client_get_bool (client, EOG_CONF_FULLSCREEN_UPSCALE, NULL);
	eog_scroll_view_set_zoom_upscale (EOG_SCROLL_VIEW (widget), upscale);
	g_object_unref (G_OBJECT (client));

	/* load first image in the background */
	prepare_data (fs, image_list, start_image);

	return GTK_WIDGET (fs);
}

gboolean
eog_full_screen_enable_SunF36 (void)
{
	return (XKeysymToKeycode (GDK_DISPLAY (), SunXK_F36) != 0);
}

EogImage*  
eog_full_screen_get_last_image (EogFullScreen *fs)
{
	EogFullScreenPrivate *priv;
	EogImage *image;

	g_return_val_if_fail (EOG_IS_FULL_SCREEN (fs), NULL);
	
	priv = fs->priv;

	image = eog_image_list_get_img_by_iter (priv->list, priv->current);

	return image;
}
