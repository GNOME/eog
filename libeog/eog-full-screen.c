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

GNOME_CLASS_BOILERPLATE (EogFullScreen,
			 eog_full_screen,
			 GtkWindow,
			 GTK_TYPE_WINDOW);

#define MAX_LOAD_SLOTS 5
static GQuark FINISHED_QUARK = 0; 
static GQuark FAILED_QUARK = 0; 

struct _EogFullScreenPrivate
{
	GtkWidget *view;

	/* Whether we have a keyboard grab */
	guint have_grab : 1;

	/* Array of all EogImage objects to show */ 
	EogImage **image_list;
	/* Number of images in image_list */
	int n_images;
	/* The index of the displayed image in image_list */ 
	int current_index;
	
	/* Kind of fifo which holds all the EogImage objects which are
	   currently loaded completely into memory and need freeing
	   eventually.
	*/
	EogImage *load_slots [MAX_LOAD_SLOTS];
	/* Number of slots actually used. May be smaller than
	   MAX_LOAD_SLOTS if there are less images to show */
	int n_slots;
	/* The index of the displayed image in load_slots */
	int current_slot;

	/* Number of images we have to load in advance on start up,
	   before we show anyting on the screen */
	gboolean n_prepare_loadings;
	/* Wether or not we are in intialize mode. We ignore all user
	 * input then. */
	gboolean initialize;
};

static void preparation_finished (EogFullScreen *fs);
static void connect_image_callbacks (EogFullScreen *fs, EogImage *image);
static void disconnect_image_callbacks (EogImage *image);

static int
get_index_offset (int index, int offset, int n_indices)
{
	int new_index;
	
	new_index = (index + offset) % n_indices;
	if (new_index < 0) 
		new_index = n_indices + new_index;
	
	return new_index;
}


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

	GNOME_CALL_PARENT (GTK_WIDGET_CLASS, hide, (widget));

	gtk_widget_destroy (widget);
}

static void
show_next_image (EogFullScreen *fs)
{
	EogFullScreenPrivate *priv;
	int load_img_index;
	int load_slot_index;
	EogImage *image;

	priv = fs->priv;

	/* obtain the next image to show directly from the load_slots fifo */
	priv->current_slot = get_index_offset (priv->current_slot, 1, priv->n_slots);
	g_assert (priv->load_slots [priv->current_slot] != NULL);
	image = priv->load_slots [priv->current_slot];
		
	eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), image);
	
        /* adapt current_index variable to point to the position of the visible
	 * image in the global image list */
	priv->current_index = get_index_offset (priv->current_index, 1, priv->n_images);

	/* determine next image to load in advance */
	load_img_index = get_index_offset (priv->current_index, 2, priv->n_images);
	load_slot_index = get_index_offset (priv->current_slot, 2, priv->n_slots);

	image = priv->image_list [load_img_index];
	if (priv->load_slots [load_slot_index] != image) {
		if (priv->load_slots [load_slot_index] != NULL) {
			eog_image_free_mem (priv->load_slots [load_slot_index]);
		}
		priv->load_slots [load_slot_index] = image;
		eog_image_load (image);
	}
}

static void
show_previous_image (EogFullScreen *fs)
{
	EogFullScreenPrivate *priv;
	int load_img_index;
	int load_slot_index;
	EogImage *image;

	priv = fs->priv;

	/* obtain the previous image to show directly from the load_slots fifo */
	priv->current_slot = get_index_offset (priv->current_slot, -1, priv->n_slots);
	g_assert (priv->load_slots [priv->current_slot] != NULL);
	image = priv->load_slots [priv->current_slot];
		
	eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), image);
	
        /* adapt current_index variable to point to the position of the visible
	 * image in the global image list  */
	priv->current_index = get_index_offset (priv->current_index, -1, priv->n_images);

	/* determine next image to load in advance */
	load_img_index = get_index_offset (priv->current_index, -2, priv->n_images);
	load_slot_index = get_index_offset (priv->current_slot, -2, priv->n_slots);

	image = priv->image_list [load_img_index];
	if (priv->load_slots [load_slot_index] != image) {
		if (priv->load_slots [load_slot_index] != NULL) {
			eog_image_free_mem (priv->load_slots [load_slot_index]);
		}
		priv->load_slots [load_slot_index] = image;
		eog_image_load (image);
	}

	load_slot_index = get_index_offset (priv->current_slot, -1, priv->n_slots);
	if (priv->load_slots [load_slot_index] == NULL) { 
		/* This is a special case, which may occur if the user
		 * moves backward through the list. */
		load_img_index = get_index_offset (priv->current_index, -1, priv->n_images);
		image = priv->image_list [load_img_index];
		priv->load_slots [load_slot_index] = image;
		eog_image_load (image);
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
		if (priv->n_images > 1 && !priv->initialize) {
			show_next_image (fs);
			handled = TRUE;
		}
		break;

	case GDK_BackSpace:
	case GDK_Left:
	case GDK_Up:
		if (priv->n_images > 1 && !priv->initialize) {
			show_previous_image (fs);
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
image_loading_finished_cb (EogImage *image, gpointer data)
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;

	g_return_if_fail (EOG_IS_FULL_SCREEN (data));
	
	fs = EOG_FULL_SCREEN (data);

	priv = fs->priv;

	if (priv->initialize) {
		priv->n_prepare_loadings--;
		if (priv->n_prepare_loadings == 0)
		{
			preparation_finished (fs);
		}
	}
}

static void
image_loading_failed_cb (EogImage *image, const char *message, gpointer data)
{
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
preparation_finished (EogFullScreen *fs)
{
	EogFullScreenPrivate *priv;
	int img_index;
	int slot_index;
	EogImage *image;

	g_return_if_fail (EOG_IS_FULL_SCREEN (fs));

	priv = fs->priv;

	if (!priv->initialize) return;

	/* display first image */
	eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), priv->load_slots [priv->current_slot]);

	priv->initialize = FALSE;

	/* prepare the next image */
	if (priv->n_images > 3) {
		img_index = get_index_offset (priv->current_index, 2, priv->n_images);
		slot_index = get_index_offset (priv->current_slot, 2, priv->n_slots);

		image = priv->image_list [img_index];
		connect_image_callbacks (fs, image);
		priv->load_slots [slot_index] = image;
		eog_image_load (image);
	}
}

static void
prepare_load_image (EogFullScreen *fs, int image_index, int slot)
{
	EogFullScreenPrivate *priv;
	EogImage *image;

	priv = fs->priv;
	
	image = priv->image_list [image_index];
	connect_image_callbacks (fs, image);
	priv->load_slots [slot] = image;
	if (eog_image_load (image)) {
		priv->n_prepare_loadings--;
	}
}

static void
prepare_data (EogFullScreen *fs, GList *image_list, EogImage *start_image)
{
	EogFullScreenPrivate *priv;
	int i;
	GList *it;
	int next_index;
	int prev_index;
	int slot;

	priv = fs->priv;
	priv->n_images = g_list_length (image_list);
       
	g_assert (priv->n_images > 0);

	priv->image_list = g_new0 (EogImage*, priv->n_images);

	/* init array of images */
	for (it = image_list, i = 0; it != NULL; it = it->next, i++) {
		priv->image_list [i] = EOG_IMAGE (it->data);
	}

	/* determine first image to show */
	priv->current_index = -1;
	if (start_image != NULL) 
		priv->current_index = g_list_index (image_list, start_image);

	if (priv->current_index == -1) 
		priv->current_index = 0;

	if (priv->n_images == 1) {
		eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), 
					   priv->image_list [0]);
		return;
	}

	/* prepare load slots */
	for (i = 0; i < MAX_LOAD_SLOTS; i++) {
		priv->load_slots [i] = NULL;
	}

	/* setup some variables */
	priv->n_slots = MIN (priv->n_images, MAX_LOAD_SLOTS);
	priv->n_prepare_loadings = MIN (3, priv->n_images);
	priv->initialize = TRUE;
	slot = 0;

	if (priv->n_prepare_loadings == 3) {
		/* load previous image only if we have more than two images
		 * in the list
		 */
		prev_index = get_index_offset (priv->current_index, -1, priv->n_images);
		prepare_load_image (fs, prev_index, slot++);
		
		priv->current_slot = 1;
	}
	else {
		priv->current_slot = 0;
	}

	/* load 'current' image always */
	prepare_load_image (fs, priv->current_index, slot++);

	/* load next image always (we have at least 2 images always at this point) */
	next_index = get_index_offset (priv->current_index, 1, priv->n_images);
	prepare_load_image (fs, next_index, slot++);

	if (priv->n_prepare_loadings == 0) {
		preparation_finished (fs);
	}
}

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

	if (gdk_color_black (gdk_colormap_get_system (), &black)) {
		gtk_widget_modify_bg (widget, GTK_STATE_NORMAL, &black);
	}
	eog_scroll_view_set_zoom_upscale (EOG_SCROLL_VIEW (widget), TRUE);

	gtk_widget_show (widget);
	gtk_container_add (GTK_CONTAINER (fs), widget);

	if (FINISHED_QUARK == 0)
		FINISHED_QUARK = g_quark_from_static_string ("EogFullScreen Finished Callback");
	
	if (FAILED_QUARK == 0) 
		FAILED_QUARK = g_quark_from_static_string ("EogFullScreen Failed Callback");

	prepare_data (fs, image_list, start_image);

	return GTK_WIDGET (fs);
}
