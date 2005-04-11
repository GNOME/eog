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
#include "eog-job-manager.h"
#include "eog-image-cache.h"

GNOME_CLASS_BOILERPLATE (EogFullScreen,
			 eog_full_screen,
			 GtkWindow,
			 GTK_TYPE_WINDOW);

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
	EogImageList *list;

	EogDirection direction;
	gboolean first_image;
	
	/* current visible image iterator */
	EogIter *current;
	/* iter of currently preloaded image */
	EogIter *preload;
	EogImageCache *cache;

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

	/* timeout for screensaver blocking */
	gint   activity_timeout_id;

	/* skip loading on fast subsequent key presses */
	guint32  last_key_press_time;
	guint    display_id;
};

static guint prepare_load_image (EogFullScreen *fs, EogIter *iter);

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

static void
cancel_load_by_iter (EogImageList *list, EogIter *iter)
{
	EogImage *image;
	guint job_id = 0;
	
	image = eog_image_list_get_img_by_iter (list, iter);
	if (image != NULL) {
		job_id = GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (image), JOB_ID_QUARK));
		g_object_unref (image);
	}
	
	if (job_id > 0) {
		eog_job_manager_cancel_job (job_id);
	}
}

static EogIter*
get_next_iter_in_direction (EogImageList *list, EogIter *iter, EogDirection direction)
{
	EogIter *next = NULL;
	gboolean success = FALSE;

	next = eog_image_list_iter_copy (list, iter);

	if (direction == EOG_DIRECTION_FORWARD) {
		success = eog_image_list_iter_next (list, next, TRUE);
	}
	else {
		success = eog_image_list_iter_prev (list, next, TRUE);
	}

	if (!success) {
		g_free (next);
		next = NULL;
	}

	return next;
}

static void
display_image_on_screen (EogFullScreen *fs, EogIter *iter) 
{
	EogFullScreenPrivate *priv;
	EogImage *image;
	EogIter *next_preload;

	priv = fs->priv;

	if (priv->current != iter) {
		if (priv->current != NULL) {
			g_free (priv->current);
		}
		priv->current = eog_image_list_iter_copy (priv->list, iter);
	}

	/* get image */
	image = eog_image_list_get_img_by_iter (priv->list, priv->current);

	if (eog_image_has_data (image, EOG_IMAGE_DATA_IMAGE)) {
		eog_image_cache_add (priv->cache, image);

		/* image is already loaded */
		eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), image);
		
		/* preload next image in current direction */
		next_preload = get_next_iter_in_direction (priv->list, priv->current, priv->direction);

		if (next_preload != NULL && 
		    !eog_image_list_iter_equal (priv->list, priv->preload, next_preload))
		{
			cancel_load_by_iter (priv->list, priv->preload);
			
			if (priv->preload != NULL)
				g_free (priv->preload);
			
			priv->preload = next_preload;
			prepare_load_image (fs, priv->preload);
		}
		else if (next_preload != NULL) {
			g_free (next_preload);	
		}
	}
	else if (g_object_get_qdata (G_OBJECT (image), JOB_ID_QUARK) == NULL) { 
		/* if the image is not loading already */
		prepare_load_image (fs, priv->current);
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

	display_image_on_screen (fs, priv->current);

	priv->display_id = 0;
	return FALSE;
}

static gboolean
is_loop_end (EogImageList *list, EogIter *current, EogDirection direction)
{
	gboolean is_end = FALSE;

	int pos = eog_image_list_get_pos_by_iter (list, current);

	if (direction == EOG_DIRECTION_FORWARD) {
		is_end = (pos == (eog_image_list_length (list) - 1));
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
	EogIter *next = NULL;

	fs = EOG_FULL_SCREEN (data);
	priv = fs->priv;

	if (priv->switch_pause)
		return TRUE;

	priv->switch_time_counter++;

	if (priv->switch_time_counter == priv->switch_timeout) {
		priv->direction = EOG_DIRECTION_FORWARD;

		if (priv->loop || !is_loop_end (priv->list, priv->current, priv->direction)) {
			next = get_next_iter_in_direction (priv->list, priv->current, priv->direction);
		}

		if (next != NULL){
			display_image_on_screen (fs, next);
			g_free (next);
		}
		else {
			gtk_widget_hide (GTK_WIDGET (fs));
		}
	}

	return TRUE;
}

/* Works only for xscreensaver */
static gboolean
disable_screen_saver (gpointer data)
{
	EogFullScreenPrivate *priv;
	gboolean success = TRUE;
	char *argv[] = { "xscreensaver-command", "-deactivate", NULL };

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
		g_print ("Disable xscreensaver failed.\n");		
	}
	
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
	priv->activity_timeout_id = 0;
	if (priv->switch_timeout > 0) {
		priv->switch_timeout_id = g_timeout_add (1000 /* every second */,
							 check_automatic_switch,
							 fs);

		/* disable screen saver */
		priv->activity_timeout_id = g_timeout_add (6000 /* every minute */,
											 disable_screen_saver,
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
		fs->priv->switch_timeout_id = 0;
	}
	
	if (fs->priv->activity_timeout_id > 0) {
		g_source_remove (fs->priv->activity_timeout_id);
		fs->priv->activity_timeout_id = 0;
	}

	GNOME_CALL_PARENT (GTK_WIDGET_CLASS, hide, (widget));
}


/* Key press handler for the full screen view */
static gint
eog_full_screen_key_press (GtkWidget *widget, GdkEventKey *event)
{
	EogFullScreenPrivate *priv;
	EogFullScreen *fs;
	gint handled;
	gboolean do_hide;
	EogIter *next = NULL;

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
	case GDK_Page_Down:
		if (eog_image_list_length (priv->list) > 1) {
			priv->direction = EOG_DIRECTION_FORWARD;
			
			next = get_next_iter_in_direction (priv->list, priv->current, priv->direction);
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
		if (eog_image_list_length (priv->list) > 1) {
			priv->direction = EOG_DIRECTION_BACKWARD;

			next = get_next_iter_in_direction (priv->list, priv->current, priv->direction);
			handled = TRUE;
		}
		break;

	case GDK_Home:
		if (eog_image_list_length (priv->list) > 1) {
			next = eog_image_list_get_iter_by_pos (priv->list, 0);
			handled = TRUE;
		}
		break;

	case GDK_End:
		if (eog_image_list_length (priv->list) > 1) {
			next = eog_image_list_get_iter_by_pos (priv->list, eog_image_list_length (priv->list) - 1);
			handled = TRUE;
		}
	};
	
	if (next != NULL) {  
		if (!priv->loop && is_loop_end (priv->list, priv->current, priv->direction)) {
			do_hide = TRUE;
		}
		else if ((event->time - priv->last_key_press_time) < 1000) {
			/* just update iterator and delay loading of current iter */
			if (priv->current != NULL) {
				g_free (priv->current);
			}
			priv->current = eog_image_list_iter_copy (priv->list, next);

			if (priv->display_id != 0) {
				g_source_remove (priv->display_id);
			}
			priv->display_id = g_timeout_add (500, delayed_display_image_on_screen, fs);
		}
		else {
			display_image_on_screen (fs, next);
		}

		priv->last_key_press_time = event->time;
		g_free (next);
	}

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
		/* free list */
		g_object_unref (priv->list);
		priv->list = NULL;
	}

	if (priv->first_iter != NULL) {
		g_free (priv->first_iter);
		priv->first_iter = NULL;
	}

	if (priv->cache != NULL) {
		g_object_unref (priv->cache);
		priv->cache = NULL;
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
	GdkScreen *screen;
	GdkRectangle geometry;
	int monitor;

	priv = g_new0 (EogFullScreenPrivate, 1);
	fs->priv = priv;

	screen = gtk_window_get_screen (GTK_WINDOW (fs));
	monitor = gdk_screen_get_monitor_at_window (screen, GTK_WIDGET (fs)->window);
	gdk_screen_get_monitor_geometry (screen, monitor, &geometry);

	gtk_window_set_default_size (GTK_WINDOW (fs),
				     geometry.width,
				     geometry.height);
	gtk_window_move (GTK_WINDOW (fs), geometry.x, geometry.y);

	priv->loop = TRUE;
	priv->first_iter = NULL;

	priv->switch_timeout = 0;
	priv->switch_timeout_id = 0;
	priv->switch_time_counter = 0;
	priv->switch_pause = FALSE;

	priv->preload = NULL;
	priv->cache = eog_image_cache_new (3); /* maximum number of cached images */

	priv->last_key_press_time = 0;
	priv->display_id = 0;
}

typedef struct {
	EogFullScreen *fs;
	EogImage      *image;
} JobImageLoadData;

static void
job_image_load_do (EogJob *job, gpointer data, GError **error)
{
	EogImage *image = ((JobImageLoadData*) data)->image;

	eog_image_load (image, EOG_IMAGE_DATA_IMAGE, job, error);
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
	current_img = eog_image_list_get_img_by_iter (priv->list, priv->current);
	if (current_img == image) {
		if (eog_job_get_success (job))
			eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), image);
		else 
			eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), NULL);
	}
	if (current_img != NULL) 
		g_object_unref (current_img);
	
	/* check if the finished image is the one we want to preload */
	preload_img = eog_image_list_get_img_by_iter (priv->list, priv->preload);
	if (preload_img == image) {
		g_free (priv->preload);
		priv->preload = NULL;
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
prepare_load_image (EogFullScreen *fs, EogIter *iter)
{
	EogFullScreenPrivate *priv;
	EogImage *image;
	EogJob *job;
	guint job_id = 0;
	JobImageLoadData *ld;

	priv = fs->priv;
	
	image = eog_image_list_get_img_by_iter (priv->list, iter);
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
		priv->switch_timeout = 0; /* disable automatic switching always */
	}

	priv->direction = EOG_DIRECTION_FORWARD;
	priv->first_iter = eog_image_list_get_first_iter (image_list);

	display_image_on_screen (fs, priv->current);
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
	gboolean       antialiasing = TRUE;

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

	if (JOB_ID_QUARK == 0)
		JOB_ID_QUARK = g_quark_from_static_string ("EogFullScreen Load Job ID");
	
	/* read configuration */
	client = gconf_client_get_default ();
	priv->loop = gconf_client_get_bool (client, EOG_CONF_FULLSCREEN_LOOP, NULL);
	if (gconf_client_get_bool (client, EOG_CONF_FULLSCREEN_AUTO_ADVANCE, NULL)) {
		priv->switch_timeout = gconf_client_get_int (client, EOG_CONF_FULLSCREEN_SECONDS, NULL);
	}
	else {
		priv->switch_timeout = 0;
	}
	upscale = gconf_client_get_bool (client, EOG_CONF_FULLSCREEN_UPSCALE, NULL);
	antialiasing = gconf_client_get_bool (client, EOG_CONF_VIEW_INTERPOLATE, NULL);
	eog_scroll_view_set_zoom_upscale (EOG_SCROLL_VIEW (widget), upscale);
	eog_scroll_view_set_antialiasing (EOG_SCROLL_VIEW (widget), antialiasing);
	
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
