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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-macros.h>

#include "eog-full-screen.h"
#include "eog-scroll-view.h"
#include "cursors.h"

GNOME_CLASS_BOILERPLATE (EogFullScreen,
			 eog_full_screen,
			 GtkWindow,
			 GTK_TYPE_WINDOW);

#define POINTER_HIDE_DELAY  2   /* hide the pointer after 2 seconds */
#define MAX_LOAD_SLOTS 5
static GQuark FINISHED_QUARK = 0; 
static GQuark FAILED_QUARK = 0; 

#define DIRECTION_FORWARD  1
#define DIRECTION_BACKWARD  -1

struct _EogFullScreenPrivate
{
	GtkWidget *view;

	/* Whether we have a keyboard grab */
	guint have_grab : 1;

	/* Array of all EogImage objects to show */ 
	EogImage **image_list;
	/* Number of images in image_list */
	int n_images;

	int direction;
	
	/* indices to manage advance loading */
	int visible_index;
	int active_index;

	/* if the mouse pointer is hidden */
	gboolean cursor_hidden;
	/* seconds since last mouse movement */
	int mouse_move_counter;
	/* timeout id which checks for cursor hiding */
	guint hide_timeout_id;
};

static void connect_image_callbacks (EogFullScreen *fs, EogImage *image);
static void disconnect_image_callbacks (EogImage *image);
static void prepare_load_image (EogFullScreen *fs, int image_index);
static void check_advance_loading (EogFullScreen *fs);


static int
get_index_modulo (int index, int offset, int modulo)
{
	int new_index;
	
	new_index = (index + offset) % modulo;
	if (new_index < 0) 
		new_index = modulo + new_index;
	
	return new_index;
}

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

	GNOME_CALL_PARENT (GTK_WIDGET_CLASS, hide, (widget));

	gtk_widget_destroy (widget);
}

static void
show_next_image (EogFullScreen *fs)
{
	EogFullScreenPrivate *priv;
	int free_index;
	
	priv = fs->priv;

	priv->visible_index = get_index_modulo (priv->visible_index, priv->direction, priv->n_images);

	eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), priv->image_list [priv->visible_index]);

	if (priv->n_images > 3) {
		free_index = get_index_modulo (priv->visible_index, 2*-priv->direction, priv->n_images);
		g_print ("free: %i\n", free_index);
		eog_image_cancel_load (priv->image_list [free_index]);
	}

	check_advance_loading (fs);
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
		if (priv->n_images > 1 && priv->visible_index >= 0) {
			priv->direction = DIRECTION_FORWARD;

			show_next_image (fs);
			handled = TRUE;
		}
		break;

	case GDK_BackSpace:
	case GDK_Left:
	case GDK_Up:
		if (priv->n_images > 1 && priv->visible_index >= 0) {
			priv->direction = DIRECTION_BACKWARD;

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
	int i;
	
	g_return_if_fail (EOG_IS_FULL_SCREEN (object));

	fs = EOG_FULL_SCREEN (object);
	priv = fs->priv;

	if (priv->hide_timeout_id > 0) {
		g_source_remove (priv->hide_timeout_id);
		priv->hide_timeout_id = 0;
	}

	if (priv->image_list != NULL) {
		for (i = 0; i < priv->n_images; i++) {
			disconnect_image_callbacks (priv->image_list [i]);
		}
		g_free (priv->image_list);
		priv->image_list = NULL;
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
	fs->priv = g_new0 (EogFullScreenPrivate, 1);

	gtk_window_set_default_size (
		GTK_WINDOW (fs),
	        gdk_screen_width (),
		gdk_screen_height ()); 
	gtk_window_move (GTK_WINDOW (fs), 0, 0);
}


static void
check_advance_loading (EogFullScreen *fs)
{
	EogFullScreenPrivate *priv;
	int diff = 0;

	g_return_if_fail (EOG_IS_FULL_SCREEN (fs));

	priv = fs->priv;

	if (priv->direction == DIRECTION_FORWARD) {
		if (priv->active_index < priv->visible_index) {
			diff = priv->n_images - priv->visible_index + priv->active_index;
		}
		else {
			diff = priv->active_index - priv->visible_index;
		}
	}
	else if (priv->direction == DIRECTION_BACKWARD) {
		if (priv->active_index > priv->visible_index) {
			diff = priv->n_images - priv->active_index + priv->visible_index;
		}
		else {
			diff = priv->visible_index - priv->active_index;
		}
	}
	g_assert (diff >= 0);

	g_print ("visible: %i .... active: %i  - diff: %i\n", priv->visible_index, priv->active_index, diff);

	if (diff < 2) {
		/* load maximal one image in advance */
		if (!eog_image_is_loaded (EOG_IMAGE (priv->image_list [priv->active_index]))) {
			prepare_load_image (fs, priv->active_index);
		}
	}
	else if (diff > 2) {
		priv->active_index = get_index_modulo (priv->visible_index, priv->direction, priv->n_images);
		if (!eog_image_is_loaded (EOG_IMAGE (priv->image_list [priv->active_index]))) {
			prepare_load_image (fs, priv->active_index);
		}
	}
}

static void
image_loading_finished_cb (EogImage *image, gpointer data)
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;

	g_return_if_fail (EOG_IS_FULL_SCREEN (data));
	
	fs = EOG_FULL_SCREEN (data);

	priv = fs->priv;

	g_print ("image loading finished: %s\n", eog_image_get_caption (image));

	disconnect_image_callbacks (image);

	if (priv->visible_index == -1) {
		/* happens if we load the first image */
		eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), priv->image_list[priv->active_index]);
		priv->visible_index = priv->active_index;

		g_print ("initiale visible index: %i\n", priv->visible_index);
	}

	priv->active_index = get_index_modulo (priv->active_index, priv->direction, priv->n_images);

	check_advance_loading (fs);
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
prepare_load_image (EogFullScreen *fs, int image_index)
{
	EogFullScreenPrivate *priv;
	EogImage *image;

	priv = fs->priv;
	
	image = priv->image_list [image_index];
	connect_image_callbacks (fs, image);

	g_print ("start image loading: %s\n", eog_image_get_caption (image));
	eog_image_load (image, EOG_IMAGE_LOAD_COMPLETE);
}

static void
prepare_data (EogFullScreen *fs, GList *image_list, EogImage *start_image)
{
	EogFullScreenPrivate *priv;
	int i;
	GList *it;

	priv = fs->priv;
	priv->n_images = g_list_length (image_list);
       
	g_assert (priv->n_images > 0);

	priv->image_list = g_new0 (EogImage*, priv->n_images);

	/* init array of images */
	for (it = image_list, i = 0; it != NULL; it = it->next, i++) {
		priv->image_list [i] = EOG_IMAGE (it->data);
	}

	/* determine first image to show */
	priv->active_index = -1;
	priv->visible_index = -1;
	priv->direction = DIRECTION_FORWARD;
	if (start_image != NULL) 
		priv->active_index = g_list_index (image_list, start_image);

	if (priv->active_index == -1) 
		priv->active_index = 0;

	if (priv->n_images == 1) {
		eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), 
					   priv->image_list [0]);
		return;
	}

	prepare_load_image (fs, priv->active_index);
}

GtkWidget *
eog_full_screen_new (GList *image_list, EogImage *start_image)
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;
	GtkWidget     *widget;
	GtkStyle      *style;

	g_return_val_if_fail (image_list != NULL, NULL);

	fs = g_object_new (EOG_TYPE_FULL_SCREEN, 
			   "type", GTK_WINDOW_POPUP, NULL);
	priv = fs->priv;

	widget = eog_scroll_view_new ();
	priv->view = widget;
	eog_scroll_view_set_zoom_upscale (EOG_SCROLL_VIEW (widget), TRUE);

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

	prepare_data (fs, image_list, start_image);

	return GTK_WIDGET (fs);
}
