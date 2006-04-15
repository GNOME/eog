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

#include <gdkconfig.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#endif
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-macros.h>
#include <gconf/gconf-client.h>

#include "eog-full-screen.h"
#include "eog-scroll-view.h"
#include "cursors.h"
#include "eog-config-keys.h"
#include "eog-job-manager.h"
#include "eog-image-cache.h"

G_DEFINE_TYPE (EogFullScreen, eog_full_screen, GTK_TYPE_WINDOW)

#define POINTER_HIDE_DELAY  2   /* hide the pointer after 2 seconds */
static GQuark JOB_ID_QUARK = 0; 

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
	EogListStore *store;

	EogDirection direction;
	gboolean first_image;
	
	/* current visible image iterator */
	GtkTreeIter current;
	/* iter of currently preloaded image */
	GtkTreeIter preload;
	EogImageCache *cache;

	/* if the mouse pointer is hidden */
	gboolean cursor_hidden;
	/* seconds since last mouse movement */
	int mouse_move_counter;
	/* timeout id which checks for cursor hiding */
	guint hide_timeout_id;

	/* wether we should loop the sequence */
	GtkTreeIter first_iter;
	gboolean loop;

	/* timeout for switching to the next image */
	gint   switch_time_counter;
	gint   switch_timeout;      /* in seconds */
	guint  switch_timeout_id;
	gboolean switch_pause;      
	gboolean slide_show;      

	/* timeout for screensaver blocking */
	gint   activity_timeout_id;

	/* skip loading on fast subsequent key presses */
	guint32  last_key_press_time;
	guint    display_id;
};

static guint prepare_load_image (EogFullScreen *fs, GtkTreeIter *iter);

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

	if (GTK_WIDGET_CLASS (eog_full_screen_parent_class)->motion_notify_event)
		result = GTK_WIDGET_CLASS (eog_full_screen_parent_class)->motion_notify_event (widget, event);
	else
		result = FALSE;

	return result;
}

static void
cancel_load_by_iter (EogListStore *store, GtkTreeIter *iter)
{
	EogImage *image;
	guint job_id = 0;
	
	gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
			    EOG_LIST_STORE_EOG_IMAGE, &image,
			    -1);
	if (image != NULL) {
		job_id = GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (image), JOB_ID_QUARK));
		g_object_unref (image);
	}
	
	if (job_id > 0) {
		eog_job_manager_cancel_job (job_id);
	}
}

static gboolean
get_next_iter_in_direction (EogListStore *store, GtkTreeIter *iter, GtkTreeIter *next, EogDirection direction)
{
	gboolean success = FALSE;

	*next = *iter;

	if (direction == EOG_DIRECTION_FORWARD) {
		success = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), next);
	}
	else {
		GtkTreePath *path;
		gtk_tree_model_get_path (GTK_TREE_MODEL (store), next);
		gtk_tree_path_prev (path);
		success = gtk_tree_model_get_iter (GTK_TREE_MODEL (store), next, path);
		gtk_tree_path_free (path);
	}
	return success;
}

/* yes, I know this is ugly, and moreover, it doesn't belong here */
#define GTK_TREE_ITER_EQUAL(a,b) ((a).stamp == (b).stamp &&		\
				  (a).user_data  == (b).user_data &&	\
				  (a).user_data2 == (b).user_data2 &&	\
				  (a).user_data3 == (b).user_data3)

static void
display_image_on_screen (EogFullScreen *fs, GtkTreeIter *iter) 
{
	EogFullScreenPrivate *priv;
	EogImage *image;
	GtkTreeIter next_preload;

	priv = fs->priv;

	if (!GTK_TREE_ITER_EQUAL (priv->current, *iter)) {
/*		if (priv->current != NULL) {
			g_free (priv->current);
		}
*/
		priv->current = *iter;
	}

	/* get image */
	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), iter,
			    EOG_LIST_STORE_EOG_IMAGE, &image,
			    -1);
	
	if (eog_image_has_data (image, EOG_IMAGE_DATA_IMAGE)) {
		eog_image_cache_add (priv->cache, image);

		/* image is already loaded */
		eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), image);
		
		/* preload next image in current direction */
		
		if (get_next_iter_in_direction (priv->store, &(priv->current), &next_preload, priv->direction) && 
		    (!GTK_TREE_ITER_EQUAL (priv->preload, next_preload)))
		{
			cancel_load_by_iter (priv->store, &(priv->preload));
			
/* 			if (priv->preload != NULL) */
/* 				g_free (priv->preload); */
			
			priv->preload = next_preload;
			prepare_load_image (fs, &(priv->preload));
		}
/* 		else if (next_preload != NULL) { */
/* 			g_free (next_preload);	 */
/* 		} */
	}
	else if (g_object_get_qdata (G_OBJECT (image), JOB_ID_QUARK) == NULL) { 
		/* if the image is not loading already */
		prepare_load_image (fs, &(priv->current));
	}

	/* FIXME: pause auto advance */

	priv->switch_time_counter = 0;
	g_object_unref (image);
}

static gboolean
delayed_display_image_on_screen (gpointer data) 
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;

	fs = EOG_FULL_SCREEN (data);
	priv = fs->priv;

	display_image_on_screen (fs, &(priv->current));

	priv->display_id = 0;
	return FALSE;
}

static gboolean
is_loop_end (EogListStore *store, GtkTreeIter *current, EogDirection direction)
{
	gboolean is_end = FALSE;

	int pos = eog_list_store_get_pos_by_iter (store, current);

	if (direction == EOG_DIRECTION_FORWARD) {
		is_end = (pos == (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) - 1));
	}
	else {
		is_end = (pos == 0);
	}
	
	return is_end;
}

static gboolean
check_automatic_switch (gpointer data)
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;
	GtkTreeIter next;

	fs = EOG_FULL_SCREEN (data);
	priv = fs->priv;

	if (priv->switch_pause)
		return TRUE;

	priv->switch_time_counter++;

	if (priv->switch_time_counter == priv->switch_timeout) {
		priv->direction = EOG_DIRECTION_FORWARD;

		if (priv->loop || !is_loop_end (priv->store, &(priv->current), priv->direction)) {
			if (get_next_iter_in_direction (priv->store, &(priv->current), &next, priv->direction)) {
				display_image_on_screen (fs, &next);
			}
			else {
				gtk_widget_hide (GTK_WIDGET (fs));
			}
		}
	}

	return TRUE;
}

static gboolean
disable_screen_saver (gpointer data)
{
	EogFullScreenPrivate *priv;
	gboolean success = TRUE;
	gchar **argv;

	if (g_find_program_in_path ("gnome-screensaver-command") != NULL) {
		g_shell_parse_argv ("gnome-screensaver-command --poke", NULL, &argv, NULL);
	}
	else {
		g_shell_parse_argv ("xscreensaver-command -deactivate", NULL, &argv, NULL);
	}

	priv = EOG_FULL_SCREEN (data)->priv;
	
	if (!priv->switch_pause) {
		GSpawnFlags flags = G_SPAWN_SEARCH_PATH | 
		                    G_SPAWN_STDOUT_TO_DEV_NULL | 
		                    G_SPAWN_STDERR_TO_DEV_NULL;
		
		success = g_spawn_sync (NULL,  /* working directory */
					argv,  /* command */
					NULL,  /* environment */
					flags, /* flags */
					NULL,  /* child setup func */
					NULL,  /* function data */
					NULL,  /* standard output */
		                        NULL,  /* standard error */
					NULL,  /* exit status */
					NULL); /* GError */
	}
	
	if (!success) {
		priv->activity_timeout_id = 0;
		g_print ("Disable screensaver failed.\n");		
	}

	g_strfreev (argv);
	
	return success;
}

/* Show handler for the full screen view */
static void
eog_full_screen_show (GtkWidget *widget)
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;

	fs = EOG_FULL_SCREEN (widget);
	priv = fs->priv;

	GTK_WIDGET_CLASS (eog_full_screen_parent_class)->show (widget);

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
	priv->activity_timeout_id = 0;
	if (priv->slide_show) {
		priv->switch_timeout_id = g_timeout_add (1000 /* every second */,
							 check_automatic_switch,
							 fs);
	}

	/* disable screen saver */
	priv->activity_timeout_id = g_timeout_add (6000 /* every minute */,
						   disable_screen_saver,
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

	if (fs->priv->slide_show) {
		g_source_remove (fs->priv->switch_timeout_id);
		fs->priv->switch_timeout_id = 0;
	}
	
	if (fs->priv->activity_timeout_id > 0) {
		g_source_remove (fs->priv->activity_timeout_id);
		fs->priv->activity_timeout_id = 0;
	}

	GTK_WIDGET_CLASS (eog_full_screen_parent_class)->hide (widget);
}


/* Key press handler for the full screen view */
static gint
eog_full_screen_key_press (GtkWidget *widget, GdkEventKey *event)
{
	EogFullScreenPrivate *priv;
	EogFullScreen *fs;
	gint handled;
	gboolean do_hide, got_iter;
	GtkTreeIter next;

	fs = EOG_FULL_SCREEN (widget);
	priv = fs->priv;

	handled = FALSE;
	do_hide = FALSE;
	got_iter = FALSE;

	switch (event->keyval) {
	case GDK_F11:
		if (!priv->slide_show) {
			do_hide = handled = TRUE;
		}
		break;

	case GDK_F5:
		if (priv->slide_show) {
			do_hide = handled = TRUE;
		}
		break;
		
	case GDK_Q:
	case GDK_q:
	case GDK_Escape:
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
	case GDK_Page_Down:
		if (!priv->slide_show && gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->store), NULL) > 1) {
			priv->direction = EOG_DIRECTION_FORWARD;
			
			got_iter = get_next_iter_in_direction (priv->store, &(priv->current), 
						    &next, priv->direction);
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
	case GDK_Page_Up:
		if (!priv->slide_show && gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->store), NULL) > 1) {
			priv->direction = EOG_DIRECTION_BACKWARD;

			got_iter = get_next_iter_in_direction (priv->store, &(priv->current), &next, priv->direction);
			handled = TRUE;
		}
		break;

	case GDK_Home:
		if (!priv->slide_show && gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->store), NULL) > 1) {
			got_iter = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &next);
			handled = TRUE;
		}
		break;

	case GDK_End:
		if (!priv->slide_show && gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->store), NULL) > 1) {
			got_iter = gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (priv->store),
								  &next, NULL, 
								  gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->store), NULL) - 1);
			handled = TRUE;
		}
	};
	
	if (got_iter) {  
		if (!priv->loop && is_loop_end (priv->store, &(priv->current), priv->direction)) {
			do_hide = TRUE;
		}
		else if ((event->time - priv->last_key_press_time) < 1000) {
			/* just update iterator and delay loading of current iter */
			priv->current = next;

			if (priv->display_id != 0) {
				g_source_remove (priv->display_id);
			}
			priv->display_id = g_timeout_add (500, delayed_display_image_on_screen, fs);
		}
		else {
			display_image_on_screen (fs, &next);
		}

		priv->last_key_press_time = event->time;
	}

	if (do_hide) {
		gtk_widget_hide (widget);
	}

	if (!handled) {
		if (GTK_WIDGET_CLASS (eog_full_screen_parent_class)->key_press_event)
			handled = GTK_WIDGET_CLASS (eog_full_screen_parent_class)->key_press_event (widget, event);
		else
			handled = FALSE;
	}

	return handled;
}

static void
eog_full_screen_destroy (GtkObject *object)
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;
	
	g_return_if_fail (EOG_IS_FULL_SCREEN (object));

	fs = EOG_FULL_SCREEN (object);
	priv = fs->priv;

	if (priv->hide_timeout_id > 0) {
		g_source_remove (priv->hide_timeout_id);
		priv->hide_timeout_id = 0;
	}

/* 	if (priv->current != NULL) { */
/* 		g_free (priv->current); */
/* 		priv->current = NULL; */
/* 	} */

	if (priv->store != NULL) {
		/* free list */
		g_object_unref (priv->store);
		priv->store = NULL;
	}

/* 	if (priv->first_iter != NULL) { */
/* 		g_free (priv->first_iter); */
/* 		priv->first_iter = NULL; */
/* 	} */

	if (priv->cache != NULL) {
		g_object_unref (priv->cache);
		priv->cache = NULL;
	}

	GTK_OBJECT_CLASS (eog_full_screen_parent_class)->destroy (object);
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

	G_OBJECT_CLASS (eog_full_screen_parent_class)->finalize (object);
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


	object_class->destroy = eog_full_screen_destroy;
	gobject_class->finalize = eog_full_screen_finalize;

	widget_class->show = eog_full_screen_show;
	widget_class->hide = eog_full_screen_hide;
	widget_class->key_press_event = eog_full_screen_key_press;
	widget_class->motion_notify_event = eog_full_screen_motion;
}

/* Object initialization function for the full screen view */
static void
eog_full_screen_init (EogFullScreen *fs)
{
	EogFullScreenPrivate *priv;
	priv = g_new0 (EogFullScreenPrivate, 1);
	fs->priv = priv;

	priv->loop = TRUE;
/* 	priv->first_iter = NULL; */

	priv->switch_timeout = 0;
	priv->switch_timeout_id = 0;
	priv->switch_time_counter = 0;
	priv->switch_pause = FALSE;

/* 	priv->preload = NULL; */
	priv->cache = eog_image_cache_new (3); /* maximum number of cached images */

	priv->last_key_press_time = 0;
	priv->display_id = 0;
}

static void
eog_full_screen_set_geometry (EogFullScreen *fs, GtkWindow *parent)
{
	GdkScreen *screen;
	GdkRectangle geometry;
	int monitor;
	
	screen = gtk_window_get_screen (GTK_WINDOW (fs));
	monitor = gdk_screen_get_monitor_at_window (screen,
			GTK_WIDGET (parent)->window);
	gdk_screen_get_monitor_geometry (screen, monitor, &geometry);

	gtk_window_set_default_size (GTK_WINDOW (fs),
				     geometry.width,
				     geometry.height);
	gtk_window_move (GTK_WINDOW (fs), geometry.x, geometry.y);
}

typedef struct {
	EogFullScreen *fs;
	EogImage      *image;
} JobImageLoadData;

static void
job_image_load_do (EogJob *job, gpointer data, GError **error)
{
	EogImage *image = ((JobImageLoadData*) data)->image;

	eog_image_load (image, EOG_IMAGE_DATA_IMAGE, error);
}

static void
job_image_load_finished  (EogJob *job, gpointer data, GError *error)
{
	EogFullScreen *fs = ((JobImageLoadData*) data)->fs;
	EogFullScreenPrivate *priv = fs->priv;
	EogImage *image = ((JobImageLoadData*) data)->image;
	EogImage *current_img;
	EogImage *preload_img;

	if (eog_job_get_success (job))
		eog_image_cache_add (priv->cache, image);

	/* check if the finished image is the one we want to display now */
	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &(priv->current),
			    EOG_LIST_STORE_EOG_IMAGE, &current_img,
			    -1);
	if (current_img == image) {
		if (eog_job_get_success (job))
			eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), image);
		else 
			eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), NULL);
	}
	if (current_img != NULL) 
		g_object_unref (current_img);
	
	/* check if the finished image is the one we want to preload */
	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &(priv->preload),
			    EOG_LIST_STORE_EOG_IMAGE, &preload_img,
			    -1);
	if (preload_img == image) {
/* 		g_free (priv->preload); */
/* 		priv->preload = NULL; */
	}
	if (preload_img != NULL)
		g_object_unref (preload_img);
		
	/* clean up, see prepare_load_image */
	g_object_set_qdata (G_OBJECT (image), JOB_ID_QUARK, NULL);
	eog_image_data_unref (image);
}

static void
job_image_load_cancel (EogJob *job, gpointer data)
{
	EogImage *image = ((JobImageLoadData*) data)->image;

	eog_image_cancel_load (image);
}

static guint
prepare_load_image (EogFullScreen *fs, GtkTreeIter *iter)
{
	EogFullScreenPrivate *priv;
	EogImage *image;
	EogJob *job;
	guint job_id = 0;
	JobImageLoadData *ld;

	priv = fs->priv;
	
	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), iter, 
			    EOG_LIST_STORE_EOG_IMAGE, &image,
			    -1);
	eog_image_data_ref (image);
	g_object_unref (image); /* eog_image_list_get_img_by_iter ref'ed the image too */

	if (eog_image_has_data (image, EOG_IMAGE_DATA_IMAGE)) {
		eog_image_data_unref (image);
		return 0;
	}

	ld = g_new0 (JobImageLoadData, 1);
	ld->image = image;
	ld->fs = fs;
	
	job = eog_job_new_full (ld,
				job_image_load_do,
				job_image_load_finished,
				job_image_load_cancel,
				NULL,
				g_free);
	
	job_id = eog_job_get_id (job);
	g_object_set_qdata (G_OBJECT (image), JOB_ID_QUARK, GUINT_TO_POINTER (job_id));
	
	eog_job_manager_add (job);

	return job_id;
}

static void
prepare_data (EogFullScreen *fs, EogListStore *store, EogImage *start_image)
{
	EogFullScreenPrivate *priv;

	priv = fs->priv;
       
	g_assert (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) > 0);

	priv->store = g_object_ref (store);

	/* determine first image to show */
	if (start_image != NULL) { 
		gint pos = eog_list_store_get_pos_by_image (store, start_image);
		if (!gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &(priv->current), NULL, pos)) {
			gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &(priv->current));
		}
	}		
	else {
		gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &(priv->current));
	}
	
	/* special case if we only have one image */
	if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) == 1) {
		priv->slide_show = FALSE; /* disable automatic switching always */
	}

	priv->direction = EOG_DIRECTION_FORWARD;
	gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &(priv->first_iter));

	display_image_on_screen (fs, &(priv->current));
}

GtkWidget *
eog_full_screen_new (GtkWindow *parent,
		     EogListStore *store, EogImage *start_image, gboolean slide_show)
{
	EogFullScreen *fs;
	EogFullScreenPrivate *priv;
 	GtkWidget     *widget;
	GtkStyle      *style;
	GConfClient   *client;
	gboolean       upscale = TRUE;
	gboolean       antialiasing = TRUE;

	g_return_val_if_fail (store != NULL, NULL);

	fs = g_object_new (EOG_TYPE_FULL_SCREEN, 
			   "type", GTK_WINDOW_POPUP,
			   NULL);
	priv = fs->priv;
	/* bind to a particular monitor */
	eog_full_screen_set_geometry (fs, parent);

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

	if (JOB_ID_QUARK == 0)
		JOB_ID_QUARK = g_quark_from_static_string ("EogFullScreen Load Job ID");

	priv->slide_show = slide_show;
	
	/* read configuration */
	client = gconf_client_get_default ();
	priv->loop = gconf_client_get_bool (client, EOG_CONF_FULLSCREEN_LOOP, NULL);
	priv->switch_timeout = gconf_client_get_int (client, EOG_CONF_FULLSCREEN_SECONDS, NULL);
	upscale = gconf_client_get_bool (client, EOG_CONF_FULLSCREEN_UPSCALE, NULL);
	antialiasing = gconf_client_get_bool (client, EOG_CONF_VIEW_INTERPOLATE, NULL);
	eog_scroll_view_set_zoom_upscale (EOG_SCROLL_VIEW (widget), upscale);
	eog_scroll_view_set_antialiasing (EOG_SCROLL_VIEW (widget), antialiasing);
	
	g_object_unref (G_OBJECT (client));

	/* load first image in the background */
	prepare_data (fs, store, start_image);

	return GTK_WIDGET (fs);
}

gboolean
eog_full_screen_enable_SunF36 (void)
{
#ifdef GDK_WINDOWING_X11
	return (XKeysymToKeycode (GDK_DISPLAY (), SunXK_F36) != 0);
#else
	return FALSE;
#endif
}

EogImage*  
eog_full_screen_get_last_image (EogFullScreen *fs)
{
	EogFullScreenPrivate *priv;
	EogImage *image;

	g_return_val_if_fail (EOG_IS_FULL_SCREEN (fs), NULL);
	
	priv = fs->priv;

	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &(priv->current),
			    EOG_LIST_STORE_EOG_IMAGE, &image,
			    -1);
	return image;
}
