/* Eye Of Gnome - Main Window 
 *
 * Copyright (C) 2000-2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on code by:
 * 	- Federico Mena-Quintero <federico@gnu.org>
 *	- Jens Finke <jens@gnome.org>
 * Based on evince code (shell/ev-window.c) by: 
 * 	- Martin Kretzschmar <martink@gnome.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "eog-window.h"
#include "eog-scroll-view.h"
#include "eog-file-chooser.h"
#include "eog-thumb-view.h"
#include "eog-list-store.h"
#include "eog-statusbar.h"
#include "eog-preferences.h"
#include "eog-stock-icons.h"
#include "eog-application.h"
#include "eog-config-keys.h"
#include "eog-job-queue.h"
#include "eog-jobs.h"
#include "eog-util.h"

#include "egg-recent.h"

#include <gconf/gconf-client.h>
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#define EOG_WINDOW_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EOG_TYPE_WINDOW, EogWindowPrivate))

G_DEFINE_TYPE (EogWindow, eog_window, GTK_TYPE_WINDOW);

#define EOG_WINDOW_DEFAULT_WIDTH  440
#define EOG_WINDOW_DEFAULT_HEIGHT 350

#define EOG_WINDOW_FULLSCREEN_TIMEOUT 5 * 1000

typedef enum {
	EOG_WINDOW_MODE_UNKNOWN,
	EOG_WINDOW_MODE_NORMAL,
	EOG_WINDOW_MODE_FULLSCREEN,
	EOG_WINDOW_MODE_SLIDESHOW
} EogWindowMode;

struct _EogWindowPrivate {
        GConfClient *client;

        EogListStore        *store;
        EogImage            *image;
	EogWindowMode        mode;

        GtkUIManager        *ui_mgr;
        GtkWidget           *box;
        GtkWidget           *hpane;
        GtkWidget           *vbox;
        GtkWidget           *view;
        GtkWidget           *thumbview;
        GtkWidget           *statusbar;

        GtkActionGroup      *actions_window;
        GtkActionGroup      *actions_image;
        GtkActionGroup      *actions_collection;

	GtkWidget           *fullscreen_popup;
	GSource             *fullscreen_timeout_source;

        EggRecentViewGtk    *recent_view;

        EogJob              *load_job;
        EogJob              *transform_job;

        guint image_info_message_cid;
        guint tip_message_cid;
};

static void eog_window_cmd_fullscreen (GtkAction *action, gpointer user_data);
static void eog_job_load_cb (EogJobLoad *job, gpointer data);
static void eog_job_load_progress_cb (EogJobLoad *job, float progress, gpointer data);
static void eog_job_transform_cb (EogJobTransform *job, gpointer data);

static void
eog_window_interp_type_changed_cb (GConfClient *client,
				   guint       cnxn_id,
				   GConfEntry  *entry,
				   gpointer    user_data)
{
	EogWindowPrivate *priv;
	gboolean interpolate = TRUE;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (priv->view));

	if (entry->value != NULL && entry->value->type == GCONF_VALUE_BOOL) {
		interpolate = gconf_value_get_bool (entry->value);
	}

	eog_scroll_view_set_antialiasing (EOG_SCROLL_VIEW (priv->view), 
					  interpolate);
}

static void
eog_window_transparency_changed_cb (GConfClient *client,
				    guint       cnxn_id,
				    GConfEntry  *entry,
				    gpointer    user_data)
{
	EogWindowPrivate *priv;
	const char *value = NULL;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (priv->view));

	if (entry->value != NULL && entry->value->type == GCONF_VALUE_STRING) {
		value = gconf_value_get_string (entry->value);
	}

	if (g_strcasecmp (value, "COLOR") == 0) {
		GdkColor color;
		char *color_str;

		color_str = gconf_client_get_string (priv->client,
						     EOG_CONF_VIEW_TRANS_COLOR, NULL);
		if (gdk_color_parse (color_str, &color)) {
			eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->view),
							  TRANSP_COLOR, &color);
		}
	} else if (g_strcasecmp (value, "CHECK_PATTERN") == 0) {
		eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->view),
						  TRANSP_CHECKED, 0);
	} else {
		eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->view),
						  TRANSP_BACKGROUND, 0);
	}
}

static void
eog_window_trans_color_changed_cb (GConfClient *client,
				   guint       cnxn_id,
				   GConfEntry  *entry,
				   gpointer    user_data)
{
	EogWindowPrivate *priv;
	GdkColor color;
	const char *color_str;
	char *value;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (priv->view));

	value = gconf_client_get_string (priv->client, 
					 EOG_CONF_VIEW_TRANSPARENCY, 
					 NULL);

	if (g_strcasecmp (value, "COLOR") != 0) {
		g_free (value);
		return;
	}
	
	if (entry->value != NULL && entry->value->type == GCONF_VALUE_STRING) {
		color_str = gconf_value_get_string (entry->value);

		if (gdk_color_parse (color_str, &color)) {
			eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->view),
							  TRANSP_COLOR, &color);
		}
	}
}

static void
update_status_bar (EogWindow *window)
{
	EogWindowPrivate *priv;
	char *str = NULL;
	int n_images;
	int pos;

	g_return_if_fail (EOG_IS_WINDOW (window));
	
	priv = window->priv;

	if (priv->image != NULL) {
		int zoom, width, height;
		GnomeVFSFileSize bytes = 0;

		zoom = floor (100 * eog_scroll_view_get_zoom (EOG_SCROLL_VIEW (priv->view)) + 0.5);

		eog_image_get_size (priv->image, &width, &height);

		bytes = eog_image_get_bytes (priv->image);
		
		if ((width > 0) && (height > 0)) {
			char *size_string;

			size_string = gnome_vfs_format_file_size_for_display (bytes);

			/* [image width] x [image height] pixels  [bytes]    [zoom in percent] */
			str = g_strdup_printf (ngettext("%i x %i pixels  %s    %i%%", 
			                                "%i x %i pixels  %s    %i%%", 
							height), 
					       width, height, size_string, zoom);
			
			g_free (size_string);
		}

		n_images = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->store), NULL);
		if (n_images > 0) {
			pos = eog_list_store_get_pos_by_image (EOG_LIST_STORE (priv->store), 
							       priv->image);

			/* Images: (image pos) / (n_total_images) */
			eog_statusbar_set_image_number (EOG_STATUSBAR (priv->statusbar), 
							pos + 1, 
							n_images);
		} 
	}

	gtk_statusbar_pop (GTK_STATUSBAR (priv->statusbar), 
			   priv->image_info_message_cid);
	
	gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar), 
			    priv->image_info_message_cid, str ? str : "");

	g_free (str);
}

static void 
eog_window_display_image (EogWindow *window, EogImage *image)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (window));
	g_return_if_fail (EOG_IS_IMAGE (image));

	g_assert (eog_image_has_data (image, EOG_IMAGE_DATA_ALL));

	priv = window->priv;

	eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), image);

	if (priv->image != NULL)
		g_object_unref (priv->image);

	if (image != NULL) {
		priv->image = g_object_ref (image);
		gtk_window_set_icon (GTK_WINDOW (window), 
				     eog_image_get_pixbuf_thumbnail (image));
	}

	gtk_window_set_title (GTK_WINDOW (window), eog_image_get_caption (image));

	update_status_bar (window);

	/* update recent files */
	//add_uri_to_recent_files (window, eog_image_get_uri (image));
}

static void
eog_window_clear_load_job (EogWindow *window)
{
	EogWindowPrivate *priv = window->priv;
	
	if (priv->load_job != NULL) {
		if (!priv->load_job->finished)
			eog_job_queue_remove_job (priv->load_job);
	
		g_signal_handlers_disconnect_by_func (priv->load_job, 
						      eog_job_load_progress_cb, 
						      window);

		g_signal_handlers_disconnect_by_func (priv->load_job, 
						      eog_job_load_cb, 
						      window);

		eog_image_cancel_load (EOG_JOB_LOAD (priv->load_job)->image);

		g_object_unref (priv->load_job);
		priv->load_job = NULL;
	}
}

static void
eog_job_load_progress_cb (EogJobLoad *job, float progress, gpointer user_data) 
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));
	
	EogWindow *window = EOG_WINDOW (user_data);

	eog_statusbar_set_progress (EOG_STATUSBAR (window->priv->statusbar), 
				    progress);
}
	
static void
eog_job_load_cb (EogJobLoad *job, gpointer data)
{
	EogWindow *window;
	
        g_return_if_fail (EOG_IS_WINDOW (data));
	
	window = EOG_WINDOW (data);

	eog_statusbar_set_progress (EOG_STATUSBAR (window->priv->statusbar), 
				    0.0);
	
	gtk_statusbar_pop (GTK_STATUSBAR (window->priv->statusbar), 
			   window->priv->image_info_message_cid);

	if (!EOG_JOB (job)->error) {
		eog_window_display_image (window, job->image);
	}

	eog_window_clear_load_job (window);

	g_object_unref (job->image);
}

static void
eog_window_clear_transform_job (EogWindow *window)
{
	EogWindowPrivate *priv = window->priv;
	
	if (priv->transform_job != NULL) {
		if (!priv->transform_job->finished)
			eog_job_queue_remove_job (priv->transform_job);
	
		g_signal_handlers_disconnect_by_func (priv->transform_job, 
						      eog_job_transform_cb, 
						      window);
		g_object_unref (priv->transform_job);
		priv->transform_job = NULL;
	}
}

static void
eog_job_transform_cb (EogJobTransform *job, gpointer data)
{
	EogWindow *window;
	
        g_return_if_fail (EOG_IS_WINDOW (data));
	
	window = EOG_WINDOW (data);

	eog_window_clear_transform_job (window);
}
	
static void
update_action_groups_state (EogWindow *window)
{
	EogWindowPrivate *priv;
	int n_images = 0;
	gboolean save_disabled = FALSE;
	gboolean show_image_collection = FALSE;
	GtkAction *action_fscreen;
	GtkAction *action_sshow;
	GtkAction *action_save;
	GtkAction *action_save_as;

	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	action_fscreen = 
		gtk_action_group_get_action (priv->actions_image, 
					     "ViewFullscreen");
	action_sshow = 
		gtk_action_group_get_action (priv->actions_collection, 
					     "ViewSlideshow");

	action_save = 
		gtk_action_group_get_action (priv->actions_image, 
					     "FileSave");

	action_save_as = 
		gtk_action_group_get_action (priv->actions_image, 
					     "FileSaveAs");

	g_assert (action_fscreen != NULL);
	g_assert (action_sshow != NULL);
	g_assert (action_save != NULL);
	g_assert (action_save_as != NULL);
	
	if (priv->store != NULL) {
		n_images = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->store), NULL);
	}

	if (n_images == 0) {
		gtk_widget_hide_all (priv->vbox);

		gtk_action_group_set_sensitive (priv->actions_window,      TRUE);
		gtk_action_group_set_sensitive (priv->actions_image,       FALSE);
		gtk_action_group_set_sensitive (priv->actions_collection,  FALSE);

		gtk_action_set_sensitive (action_fscreen, FALSE);
		gtk_action_set_sensitive (action_sshow,   FALSE);
	} else if (n_images == 1) {
		show_image_collection = 
			gconf_client_get_bool (priv->client, 
					       EOG_CONF_UI_IMAGE_COLLECTION, 
					       NULL);

		gtk_widget_show_all (priv->vbox);

		if (!show_image_collection) 
			gtk_widget_hide_all (gtk_widget_get_parent (priv->thumbview));

		gtk_action_group_set_sensitive (priv->actions_window,      TRUE);
		gtk_action_group_set_sensitive (priv->actions_image,       TRUE);
		gtk_action_group_set_sensitive (priv->actions_collection,  FALSE);
		
		gtk_action_set_sensitive (action_fscreen, TRUE);
		gtk_action_set_sensitive (action_sshow,   FALSE);

		gtk_widget_grab_focus (priv->view);
	} else {
		show_image_collection = 
			gconf_client_get_bool (priv->client, 
					       EOG_CONF_UI_IMAGE_COLLECTION, 
					       NULL);

		gtk_widget_show_all (priv->vbox);

		if (!show_image_collection) 
			gtk_widget_hide_all (gtk_widget_get_parent (priv->thumbview));

		gtk_action_group_set_sensitive (priv->actions_window,      TRUE);
		gtk_action_group_set_sensitive (priv->actions_image,       TRUE);
		gtk_action_group_set_sensitive (priv->actions_collection,  TRUE);
		
		gtk_action_set_sensitive (action_fscreen, TRUE);
		gtk_action_set_sensitive (action_sshow,   TRUE);

		gtk_widget_grab_focus (priv->view);
	}

	save_disabled = gconf_client_get_bool (priv->client, EOG_CONF_DESKTOP_CAN_SAVE, NULL);

	if (save_disabled) {
		gtk_action_set_sensitive (action_save, FALSE);
		gtk_action_set_sensitive (action_save_as, FALSE);
	}
}

static void
update_selection_ui_visibility (EogWindow *window)
{
	EogWindowPrivate *priv;
	GtkAction *wallpaper_action;
	gint n_selected;

	priv = window->priv;

	n_selected = eog_thumb_view_get_n_selected (EOG_THUMB_VIEW (priv->thumbview));

	wallpaper_action = 
		gtk_action_group_get_action (priv->actions_image, 
					     "SetAsWallpaper");

	if (n_selected == 1) {
		gtk_action_set_sensitive (wallpaper_action, TRUE);
	} else {
		gtk_action_set_sensitive (wallpaper_action, FALSE);
	} 
}

static void 
apply_transformation (EogWindow *window, EogTransform *trans)
{
	EogWindowPrivate *priv;
	GList *images;

	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;
	
	images = eog_thumb_view_get_selected_images (EOG_THUMB_VIEW (priv->thumbview));

	eog_window_clear_transform_job (window);
	
	priv->transform_job = eog_job_transform_new (images, trans);

	g_signal_connect (priv->transform_job,
			  "finished",
			  G_CALLBACK (eog_job_transform_cb),
			  window);

	eog_job_queue_add_job (priv->transform_job);
}

static void
handle_image_selection_changed_cb (EogThumbView *thumbview, EogWindow *window) 
{
	EogWindowPrivate *priv;
	EogImage *image;
	gchar *status_message;

	priv = window->priv;

	if (eog_thumb_view_get_n_selected (EOG_THUMB_VIEW (priv->thumbview)) == 0)
		return;

	update_selection_ui_visibility (window);
	
	image = eog_thumb_view_get_first_selected_image (EOG_THUMB_VIEW (priv->thumbview));

	g_assert (EOG_IS_IMAGE (image));

	if (eog_image_has_data (image, EOG_IMAGE_DATA_ALL)) {
		eog_window_display_image (window, image);
		return;
	}

	eog_window_clear_load_job (window);
	
	priv->load_job = eog_job_load_new (image);

	g_signal_connect (priv->load_job,
			  "finished",
			  G_CALLBACK (eog_job_load_cb),
			  window);

	g_signal_connect (priv->load_job,
			  "progress",
			  G_CALLBACK (eog_job_load_progress_cb),
			  window);

	eog_job_queue_add_job (priv->load_job);

	status_message = g_strdup_printf (_("Loading image \"%s\""), 
				          eog_image_get_uri_for_display (image));
	
	gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar),
			    priv->tip_message_cid, status_message);

	g_free (status_message);
}
	
static void
view_zoom_changed_cb (GtkWidget *widget, double zoom, gpointer user_data)
{
	EogWindow *window;
	GtkAction *action_zoom_in;
	GtkAction *action_zoom_out;

	g_return_if_fail (EOG_IS_WINDOW (user_data));
	
	window = EOG_WINDOW (user_data);

	update_status_bar (window);

	action_zoom_in = 
		gtk_action_group_get_action (window->priv->actions_image, 
					     "ViewZoomIn");
	
	action_zoom_out = 
		gtk_action_group_get_action (window->priv->actions_image, 
					     "ViewZoomOut");

	gtk_action_set_sensitive (action_zoom_in,
			!eog_scroll_view_get_zoom_is_max (EOG_SCROLL_VIEW (window->priv->view)));
	gtk_action_set_sensitive (action_zoom_out,
			!eog_scroll_view_get_zoom_is_min (EOG_SCROLL_VIEW (window->priv->view)));
}

static void
eog_window_open_recent_cb (GtkWidget *widget, EggRecentItem *item, gpointer data)
{
	g_print ("Not implemented yet!\n");
}

static void
file_open_dialog_response_cb (GtkWidget *chooser,
			      gint       response_id,
			      EogWindow  *ev_window)
{
	if (response_id == GTK_RESPONSE_OK) {
		GSList *uris;

		uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser));

		eog_application_open_uri_list (EOG_APP, 
					       uris, 
					       GDK_CURRENT_TIME, 
					       NULL);
	
		g_slist_foreach (uris, (GFunc) g_free, NULL);	
		g_slist_free (uris);
	}

	gtk_widget_destroy (chooser);
}

static void
eog_window_update_fullscreen_action (EogWindow *window)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->actions_image, "ViewFullscreen");

	g_signal_handlers_block_by_func
		(action, G_CALLBACK (eog_window_cmd_fullscreen), window);
	
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      window->priv->mode == EOG_WINDOW_MODE_FULLSCREEN);

	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (eog_window_cmd_fullscreen), window);
}

static void
eog_window_update_fullscreen_popup (EogWindow *window)
{
	GtkWidget *popup = window->priv->fullscreen_popup;
	GdkRectangle screen_rect;
	GdkScreen *screen;

	g_return_if_fail (popup != NULL);

	if (GTK_WIDGET (window)->window == NULL) return;

	screen = gtk_widget_get_screen (GTK_WIDGET (window));

	gdk_screen_get_monitor_geometry (screen,
			gdk_screen_get_monitor_at_window
                        (screen,
                         GTK_WIDGET (window)->window),
                         &screen_rect);

	gtk_widget_set_size_request (popup,
				     screen_rect.width,
				     -1);

	gtk_window_move (GTK_WINDOW (popup), screen_rect.x, screen_rect.y);
}

static void
screen_size_changed_cb (GdkScreen *screen, EogWindow *window)
{
	eog_window_update_fullscreen_popup (window);
}

static void
fullscreen_popup_size_request_cb (GtkWidget      *popup, 
				  GtkRequisition *req, 
				  EogWindow      *window)
{
	eog_window_update_fullscreen_popup (window);
}

static void fullscreen_clear_timeout (EogWindow *window);

static gboolean
fullscreen_timeout_cb (gpointer data)
{
	EogWindow *window = EOG_WINDOW (data);

	gtk_widget_hide_all (window->priv->fullscreen_popup);
	
	fullscreen_clear_timeout (window);

	return FALSE;
}

static void
fullscreen_clear_timeout (EogWindow *window)
{
	if (window->priv->fullscreen_timeout_source != NULL) {
		g_source_unref (window->priv->fullscreen_timeout_source);
		g_source_destroy (window->priv->fullscreen_timeout_source);
	}

	window->priv->fullscreen_timeout_source = NULL;
}

static void
fullscreen_set_timeout (EogWindow *window)
{
	GSource *source;

	fullscreen_clear_timeout (window);

	source = g_timeout_source_new (EOG_WINDOW_FULLSCREEN_TIMEOUT);
	g_source_set_callback (source, fullscreen_timeout_cb, window, NULL);
	
	g_source_attach (source, NULL);

	window->priv->fullscreen_timeout_source = source;
}

static void
show_fullscreen_popup (EogWindow *window)
{
	if (!GTK_WIDGET_VISIBLE (window->priv->fullscreen_popup)) {
		gtk_widget_show_all (GTK_WIDGET (window->priv->fullscreen_popup));
	}

	fullscreen_set_timeout (window);
}

static gboolean
fullscreen_motion_notify_cb (GtkWidget      *widget,
			     GdkEventMotion *event,
			     gpointer       user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);

	show_fullscreen_popup (window);

	return FALSE;
}

static gboolean
fullscreen_leave_notify_cb (GtkWidget *widget,
			    GdkEventCrossing *event,
			    gpointer user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);

	fullscreen_clear_timeout (window);

	return FALSE;
}

static void
exit_fullscreen_button_clicked_cb (GtkWidget *button, EogWindow *window)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->actions_image, "ViewFullscreen");
	g_return_if_fail (action != NULL);

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
}

static GtkWidget *
eog_window_get_exit_fullscreen_button (EogWindow *window)
{
	GtkWidget *button;

	button = gtk_button_new_from_stock (GTK_STOCK_LEAVE_FULLSCREEN);

	g_signal_connect (button, "clicked",
			  G_CALLBACK (exit_fullscreen_button_clicked_cb),
			  window);

	return button;
}

static GtkWidget *
eog_window_create_fullscreen_popup (EogWindow *window)
{
	GtkWidget *popup;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *toolbar;
	GdkScreen *screen;

	popup = gtk_window_new (GTK_WINDOW_POPUP);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (popup), hbox);
	
	toolbar = gtk_ui_manager_get_widget (window->priv->ui_mgr, "/FullscreenToolbar");
	g_assert (GTK_IS_WIDGET (toolbar));
	gtk_box_pack_start (GTK_BOX (hbox), toolbar, TRUE, TRUE, 0);

	button = eog_window_get_exit_fullscreen_button (window);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	gtk_window_set_resizable (GTK_WINDOW (popup), FALSE);

	screen = gtk_widget_get_screen (GTK_WIDGET (window));

	g_signal_connect_object (screen, "size-changed",
			         G_CALLBACK (screen_size_changed_cb),
				 window, 0);

	g_signal_connect_object (popup, "size_request",
			         G_CALLBACK (fullscreen_popup_size_request_cb),
				 window, 0);

	gtk_window_set_screen (GTK_WINDOW (popup), screen);

	return popup;
}

static void
update_ui_visibility (EogWindow *window)
{
	EogWindowPrivate *priv;
	
	GtkAction *action;
	GtkWidget *menubar;
	GtkWidget *toolbar;
	
	gboolean fullscreen_mode, visible;

	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	fullscreen_mode = priv->mode != EOG_WINDOW_MODE_NORMAL;
	
	menubar = gtk_ui_manager_get_widget (priv->ui_mgr, "/MainMenu");
	g_assert (GTK_IS_WIDGET (menubar));

	toolbar = gtk_ui_manager_get_widget (priv->ui_mgr, "/Toolbar");
	g_assert (GTK_IS_WIDGET (toolbar));

	visible = gconf_client_get_bool (priv->client, EOG_CONF_UI_TOOLBAR, NULL);
	visible = visible && !fullscreen_mode;
	action = gtk_ui_manager_get_action (priv->ui_mgr, "/MainMenu/View/ToolbarToggle");
	g_assert (action != NULL);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);
	g_object_set (G_OBJECT (toolbar), "visible", visible, NULL);

	visible = gconf_client_get_bool (priv->client, EOG_CONF_UI_STATUSBAR, NULL);
	visible = visible && !fullscreen_mode;
	action = gtk_ui_manager_get_action (priv->ui_mgr, "/MainMenu/View/StatusbarToggle");
	g_assert (action != NULL);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);
	g_object_set (G_OBJECT (priv->statusbar), "visible", visible, NULL);

	visible = gconf_client_get_bool (priv->client, EOG_CONF_UI_IMAGE_COLLECTION, NULL);
	visible = visible && !fullscreen_mode;
	action = gtk_ui_manager_get_action (priv->ui_mgr, "/MainMenu/View/ImageCollectionToggle");
	g_assert (action != NULL);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);
	if (visible) {
		gtk_widget_show_all (gtk_widget_get_parent (priv->thumbview));
	} else {
		gtk_widget_hide_all (gtk_widget_get_parent (priv->thumbview));
	}

	if (priv->fullscreen_popup != NULL) {
		if (fullscreen_mode) {
			show_fullscreen_popup (window);
		} else {
			gtk_widget_hide_all (GTK_WIDGET (priv->fullscreen_popup));
		}
	}
}

static void
eog_window_run_fullscreen (EogWindow *window)
{
	EogWindowPrivate *priv;
	GtkWidget *menubar;
	gboolean upscale;
	
	priv = window->priv;

	priv->mode = EOG_WINDOW_MODE_FULLSCREEN;

	if (window->priv->fullscreen_popup == NULL)
		window->priv->fullscreen_popup
			= eog_window_create_fullscreen_popup (window);

	update_ui_visibility (window);

	menubar = gtk_ui_manager_get_widget (priv->ui_mgr, "/MainMenu");
	g_assert (GTK_IS_WIDGET (menubar));
	gtk_widget_hide (menubar);

	g_object_set (G_OBJECT (gtk_widget_get_parent (priv->view)),
		      "shadow-type", GTK_SHADOW_NONE,
		      NULL);

	g_signal_connect (window->priv->view,
			  "motion-notify-event",
			  G_CALLBACK (fullscreen_motion_notify_cb),
			  window);

	g_signal_connect (window->priv->view,
			  "leave-notify-event",
			  G_CALLBACK (fullscreen_leave_notify_cb),
			  window);
	
	fullscreen_set_timeout (window);

	upscale = gconf_client_get_bool (window->priv->client, 
					 EOG_CONF_FULLSCREEN_UPSCALE, 
					 NULL);

	eog_scroll_view_set_zoom_upscale (EOG_SCROLL_VIEW (priv->view), 
					  upscale);
	
	gtk_widget_grab_focus (window->priv->view);
	eog_window_update_fullscreen_action (window);
	gtk_window_fullscreen (GTK_WINDOW (window));
	eog_window_update_fullscreen_popup (window);
}

static void
eog_window_stop_fullscreen (EogWindow *window)
{
	EogWindowPrivate *priv;
	GtkWidget *menubar;
	
	priv = window->priv;

	if (priv->mode != EOG_WINDOW_MODE_FULLSCREEN) return;
	
	priv->mode = EOG_WINDOW_MODE_NORMAL;

	fullscreen_clear_timeout (window);

	g_object_set (G_OBJECT (gtk_widget_get_parent (priv->view)),
		      "shadow-type", GTK_SHADOW_IN,
		      NULL);

	g_signal_handlers_disconnect_by_func (priv->view,
					      (gpointer) fullscreen_motion_notify_cb,
					      window);

	g_signal_handlers_disconnect_by_func (priv->view,
					      (gpointer) fullscreen_leave_notify_cb,
					      window);
	
	update_ui_visibility (window);

	menubar = gtk_ui_manager_get_widget (priv->ui_mgr, "/MainMenu");
	g_assert (GTK_IS_WIDGET (menubar));
	gtk_widget_show (menubar);
	
	eog_scroll_view_set_zoom_upscale (EOG_SCROLL_VIEW (priv->view), FALSE);

	eog_window_update_fullscreen_action (window);
	gtk_window_unfullscreen (GTK_WINDOW (window));
}

static void
eog_window_cmd_file_open (GtkAction *action, gpointer user_data)
{
	EogWindow *window;
	EogWindowPrivate *priv;
        EogImage *current;
	GtkWidget *dlg;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);

        priv = window->priv;

	dlg = eog_file_chooser_new (GTK_FILE_CHOOSER_ACTION_OPEN);

	current = eog_thumb_view_get_first_selected_image (EOG_THUMB_VIEW (priv->thumbview));

	if (current != NULL)
	        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg), 
						     g_path_get_dirname (eog_image_get_uri_for_display (current)));

	g_signal_connect (dlg, "response",
			  G_CALLBACK (file_open_dialog_response_cb),
			  window);
	
	gtk_widget_show_all (dlg);
}

static void
eog_window_cmd_close_window (GtkAction *action, gpointer user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	gtk_widget_destroy (GTK_WIDGET (user_data));
}

static void
eog_window_cmd_preferences (GtkAction *action, gpointer user_data)
{
	EogWindow *window;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);

	eog_preferences_show (GTK_WINDOW (window), window->priv->client);
}

static void
eog_window_cmd_help (GtkAction *action, gpointer user_data)
{
	EogWindow *window;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);

	eog_util_show_help (NULL, GTK_WINDOW (window));
}

static void
eog_window_cmd_about (GtkAction *action, gpointer user_data)
{
	EogWindow *window;
	
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	static const char *authors[] = {
		"Lucas Rocha <lucasr@cvs.gnome.org> (maintainer)",
		"Tim Gerla <tim+gnomebugs@gerla.net> (maintainer)",
		"",
		"Philip Van Hoof <pvanhoof@gnome.org>",
                "Paolo Borelli <pborelli@katamail.com>",
		"Jens Finke <jens@triq.net>",
		"Martin Baulig <martin@home-of-linux.org>",
		"Arik Devens <arik@gnome.org>",
		"Michael Meeks <mmeeks@gnu.org>",
		"Federico Mena-Quintero <federico@gnu.org>",
		"Lutz M\xc3\xbcller <urc8@rz.uni-karlsruhe.de>",
		NULL
	};
	static const char *documenters[] = {
		"Eliot Landrum <eliot@landrum.cx>",
		"Federico Mena-Quintero <federico@gnu.org>",
		"Sun GNOME Documentation Team <gdocteam@sun.com>",
		NULL
	};

	const char *translators;

	translators = _("translator-credits");

	const char *license[] = {
		N_("This program is free software; you can redistribute it and/or modify "
		   "it under the terms of the GNU General Public License as published by "
		   "the Free Software Foundation; either version 2 of the License, or "
		   "(at your option) any later version.\n"),
		N_("This program is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details.\n"),
		N_("You should have received a copy of the GNU General Public License "
		   "along with this program; if not, write to the Free Software "
		   "Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.")
	};

	char *license_trans;

	license_trans = g_strconcat (_(license[0]), "\n", _(license[1]), "\n",
				     _(license[2]), "\n", NULL);

	window = EOG_WINDOW (user_data);

	gtk_show_about_dialog (GTK_WINDOW (window),
			       "name", _("Eye of GNOME"),
			       "version", VERSION,
			       "copyright", "Copyright \xc2\xa9 2000-2006 Free Software Foundation, Inc.",
			       "comments",_("The GNOME image viewing and cataloging program."),
			       "authors", authors,
			       "documenters", documenters,
			       "translator-credits", translators,
			       "website", "http://live.gnome.org/EyeOfGnome",
			       "logo-icon-name", "image-viewer",
			       "wrap-license", TRUE,
			       "license", license_trans,
			       NULL);

	g_free (license_trans);
}

static void
eog_window_cmd_show_hide_bar (GtkAction *action, gpointer user_data)
{
	EogWindow *window;     
	EogWindowPrivate *priv;
	gboolean visible;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);
	priv = window->priv;

	if (priv->mode != EOG_WINDOW_MODE_NORMAL) return;
	
	visible = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	if (g_ascii_strcasecmp (gtk_action_get_name (action), "ViewToolbar") == 0) {
		GtkWidget *widget = gtk_ui_manager_get_widget (priv->ui_mgr, "/Toolbar");
		g_object_set (G_OBJECT (widget), "visible", visible, NULL);
		gconf_client_set_bool (priv->client, EOG_CONF_UI_TOOLBAR, visible, NULL);
	} else if (g_ascii_strcasecmp (gtk_action_get_name (action), "ViewStatusbar") == 0) {
		g_object_set (G_OBJECT (priv->statusbar), "visible", visible, NULL);
		gconf_client_set_bool (priv->client, EOG_CONF_UI_STATUSBAR, visible, NULL);
	} else if (g_ascii_strcasecmp (gtk_action_get_name (action), "ViewImageCollection") == 0) {
		if (visible) {
			gtk_widget_show_all (gtk_widget_get_parent (priv->thumbview));
		} else {
			gtk_widget_hide_all (gtk_widget_get_parent (priv->thumbview));
		}
		gconf_client_set_bool (priv->client, EOG_CONF_UI_IMAGE_COLLECTION, visible, NULL);
	}
}

static void
eog_window_cmd_save (GtkAction *action, gpointer user_data)
{
	g_print ("Not implemented yet!\n");
}

static void
eog_window_cmd_save_as (GtkAction *action, gpointer user_data)
{
	g_print ("Not implemented yet!\n");
}

static void
eog_window_cmd_print (GtkAction *action, gpointer user_data)
{
	g_print ("Not implemented yet!\n");
}

static void
eog_window_cmd_undo (GtkAction *action, gpointer user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	apply_transformation (EOG_WINDOW (user_data), NULL);
}

static void
eog_window_cmd_flip_horizontal (GtkAction *action, gpointer user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	apply_transformation (EOG_WINDOW (user_data), 
			      eog_transform_flip_new (EOG_TRANSFORM_FLIP_HORIZONTAL));
}

static void
eog_window_cmd_flip_vertical (GtkAction *action, gpointer user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	apply_transformation (EOG_WINDOW (user_data), 
			      eog_transform_flip_new (EOG_TRANSFORM_FLIP_VERTICAL));
}

static void
eog_window_cmd_rotate_90 (GtkAction *action, gpointer user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	apply_transformation (EOG_WINDOW (user_data), 
			      eog_transform_rotate_new (90));
}

static void
eog_window_cmd_rotate_270 (GtkAction *action, gpointer user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	apply_transformation (EOG_WINDOW (user_data), 
			      eog_transform_rotate_new (270));
}

static void
eog_window_cmd_rotate_180 (GtkAction *action, gpointer user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	apply_transformation (EOG_WINDOW (user_data), 
			      eog_transform_rotate_new (180));
}

static void
eog_window_cmd_wallpaper (GtkAction *action, gpointer user_data)
{
	g_print ("Not implemented yet!\n");
}

static void
eog_window_cmd_move_to_trash (GtkAction *action, gpointer user_data)
{
	g_print ("Not implemented yet!\n");
}

static void
eog_window_cmd_fullscreen (GtkAction *action, gpointer user_data)
{
	EogWindow *window;
	gboolean fullscreen;
	
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);

	fullscreen = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	if (fullscreen) {
		eog_window_run_fullscreen (window);
	} else {
		eog_window_stop_fullscreen (window);
	}
}

static void
eog_window_cmd_slideshow (GtkAction *action, gpointer user_data)
{
	g_print ("Not implemented yet!\n");
}

static void
eog_window_cmd_zoom_in (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	if (priv->view) {
		eog_scroll_view_zoom_in (EOG_SCROLL_VIEW (priv->view), FALSE);
	}
}

static void
eog_window_cmd_zoom_out (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	if (priv->view) {
		eog_scroll_view_zoom_out (EOG_SCROLL_VIEW (priv->view), FALSE);
	}
}

static void
eog_window_cmd_zoom_normal (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	if (priv->view) {
		eog_scroll_view_set_zoom (EOG_SCROLL_VIEW (priv->view), 1.0);
	}
}

static void
eog_window_cmd_zoom_fit (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	if (priv->view) {
		eog_scroll_view_zoom_fit (EOG_SCROLL_VIEW (priv->view));
	}
}

static void
eog_window_cmd_go_prev (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;
	
	eog_thumb_view_select_single (EOG_THUMB_VIEW (priv->thumbview), 
				      EOG_THUMB_VIEW_SELECT_LEFT);
}

static void
eog_window_cmd_go_next (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	eog_thumb_view_select_single (EOG_THUMB_VIEW (priv->thumbview), 
				      EOG_THUMB_VIEW_SELECT_RIGHT);
}

static void
eog_window_cmd_go_first (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	eog_thumb_view_select_single (EOG_THUMB_VIEW (priv->thumbview), 
				      EOG_THUMB_VIEW_SELECT_FIRST);
}

static void
eog_window_cmd_go_last (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	eog_thumb_view_select_single (EOG_THUMB_VIEW (priv->thumbview), 
				      EOG_THUMB_VIEW_SELECT_LAST);
}

static const GtkActionEntry action_entries_window[] = {
	{ "File",  NULL, N_("_File") },
	{ "Edit",  NULL, N_("_Edit") },
	{ "View",  NULL, N_("_View") },
	{ "Image", NULL, N_("_Image") },
	{ "Go",    NULL, N_("_Go") },
	{ "Help",  NULL, N_("_Help") },

	{ "FileOpen", GTK_STOCK_OPEN,  N_("_Open..."), "<control>O", 
	  N_("Open a file"),
	  G_CALLBACK (eog_window_cmd_file_open) },
	{ "FileClose", GTK_STOCK_CLOSE, N_("_Close"), "<control>W",  
	  N_("Close window"),
	  G_CALLBACK (eog_window_cmd_close_window) },
	{ "EditPreferences", GTK_STOCK_PREFERENCES, N_("Prefere_nces"), NULL, 
	  N_("Preferences for Eye of GNOME"), 
	  G_CALLBACK (eog_window_cmd_preferences) },
	{ "HelpManual", GTK_STOCK_HELP, N_("_Contents"), "F1", 
	  N_("Help On this application"),
	  G_CALLBACK (eog_window_cmd_help) },
	{ "HelpAbout", GTK_STOCK_ABOUT, N_("_About"), NULL, 
	  N_("About this application"),
	  G_CALLBACK (eog_window_cmd_about) }
};

static const GtkToggleActionEntry toggle_entries_window[] = {
	{ "ViewToolbar", NULL, N_("_Toolbar"), NULL, 
	  N_("Changes the visibility of the toolbar in the current window"),
	  G_CALLBACK (eog_window_cmd_show_hide_bar), TRUE },
	{ "ViewStatusbar", NULL, N_("_Statusbar"), NULL, 
	  N_("Changes the visibility of the statusbar in the current window"), 
	  G_CALLBACK (eog_window_cmd_show_hide_bar), TRUE },
	{ "ViewImageCollection", NULL, N_("_Image Collection"), "F9",
	  N_("Changes the visibility of the image collection pane in the current window"), 
	  G_CALLBACK (eog_window_cmd_show_hide_bar), TRUE },
};

static const GtkActionEntry action_entries_image[] = {
	{ "FileSave", GTK_STOCK_SAVE, N_("_Save"), "<control>s", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_save) },
	{ "FileSaveAs", GTK_STOCK_SAVE_AS, N_("Save _As..."), "<control><shift>s", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_save_as) },
	{ "FilePrint", GTK_STOCK_PRINT, N_("Print..."), "<control>p", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_print) },
	{ "EditUndo", NULL, N_("_Undo"), "<control>z", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_undo) },
	{ "EditFlipHorizontal", EOG_STOCK_FLIP_HORIZONTAL, N_("Flip _Horizontal"), NULL, 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_flip_horizontal) },
	{ "EditFlipVertical", EOG_STOCK_FLIP_VERTICAL, N_("Flip _Vertical"), NULL, 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_flip_vertical) },
	{ "EditRotate90",  EOG_STOCK_ROTATE_90,  N_("_Rotate Clockwise"), "<control>r", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_rotate_90) },
	{ "EditRotate270", EOG_STOCK_ROTATE_270, N_("Rotate Counter C_lockwise"), NULL, 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_rotate_270) },
	{ "EditRotate180", EOG_STOCK_ROTATE_180, N_("Rotat_e 180\xC2\xB0"), "<control><shift>r", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_rotate_180) },
	{ "SetAsWallpaper", NULL, N_("Set As _Wallpaper"), NULL, 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_wallpaper) },
	{ "EditMoveToTrash", GTK_STOCK_DELETE, N_("Move to Trash"), NULL, 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_move_to_trash) },
	{ "ViewZoomIn", GTK_STOCK_ZOOM_IN, N_("_Zoom In"), "<control>plus", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_zoom_in) },
	{ "ViewZoomOut", GTK_STOCK_ZOOM_OUT, N_("Zoom _Out"), "<control>minus", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_zoom_out) },
	{ "ViewZoomNormal", GTK_STOCK_ZOOM_100, N_("_Normal Size"), "<control>0", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_zoom_normal) },
	{ "ViewZoomFit", GTK_STOCK_ZOOM_FIT, N_("Best _Fit"), NULL, 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_zoom_fit) },
	{ "ControlEqual", GTK_STOCK_ZOOM_IN, N_("_Zoom In"), "<control>equal", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_zoom_in) },
	{ "ControlKpAdd", GTK_STOCK_ZOOM_IN, N_("_Zoom In"), "<control>KP_Add",
	  NULL, 
	  G_CALLBACK (eog_window_cmd_zoom_in) },
	{ "ControlKpSub", GTK_STOCK_ZOOM_OUT, N_("Zoom _Out"), "<control>KP_Subtract", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_zoom_out) },
	{ "Delete", NULL, N_("Move to _Trash"), "Delete", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_move_to_trash) },
};

static const GtkToggleActionEntry toggle_entries_image[] = {
	{ "ViewFullscreen", GTK_STOCK_FULLSCREEN, N_("_Full Screen"), "F11", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_fullscreen), FALSE },
};
	
static const GtkActionEntry action_entries_collection[] = {
	{ "GoPrevious", GTK_STOCK_GO_BACK, N_("_Previous Image"), "<Alt>Left", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_go_prev) },
	{ "GoNext", GTK_STOCK_GO_FORWARD, N_("_Next Image"), "<Alt>Right", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_go_next) },
	{ "GoFirst", GTK_STOCK_GOTO_FIRST, N_("_First Image"), "<Alt>Home", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_go_first) },
	{ "GoLast", GTK_STOCK_GOTO_LAST, N_("_Last Image"), "<Alt>End", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_go_last) },
	{ "ViewSlideshow", NULL, N_("_Slideshow"), "F5", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_slideshow) },
	{ "SpaceBar", NULL, N_("_Next Image"), "space", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_go_next) },
	{ "ShiftSpaceBar", NULL, N_("_Previous Image"), "<shift>space", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_go_prev) },
	{ "Return", NULL, N_("_Next Image"), "Return", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_go_next) },
	{ "ShiftReturn", NULL, N_("_Previous Image"), "<shift>Return", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_go_prev) },
	{ "BackSpace", NULL, N_("_Previous Image"), "BackSpace", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_go_prev) },
	{ "Home", NULL, N_("_First Image"), "Home", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_go_first) },
	{ "End", NULL, N_("_Last Image"), "End", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_go_last) },
};

static void
menu_item_select_cb (GtkMenuItem *proxy, EogWindow *window)
{
	GtkAction *action;
	char *message;

	action = g_object_get_data (G_OBJECT (proxy), "gtk-action");

	g_return_if_fail (action != NULL);

	g_object_get (G_OBJECT (action), "tooltip", &message, NULL);

	if (message) {
		gtk_statusbar_push (GTK_STATUSBAR (window->priv->statusbar),
				    window->priv->tip_message_cid, message);
		g_free (message);
	}
}

static void
menu_item_deselect_cb (GtkMenuItem *proxy, EogWindow *window)
{
	gtk_statusbar_pop (GTK_STATUSBAR (window->priv->statusbar),
			   window->priv->tip_message_cid);
}

static void
connect_proxy_cb (GtkUIManager *manager,
                  GtkAction *action,
                  GtkWidget *proxy,
                  EogWindow *window)
{
	if (GTK_IS_MENU_ITEM (proxy)) {
		g_signal_connect (proxy, "select",
				  G_CALLBACK (menu_item_select_cb), window);
		g_signal_connect (proxy, "deselect",
				  G_CALLBACK (menu_item_deselect_cb), window);
	}
}

static void
disconnect_proxy_cb (GtkUIManager *manager,
                     GtkAction *action,
                     GtkWidget *proxy,
                     EogWindow *window)
{
	if (GTK_IS_MENU_ITEM (proxy)) {
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_select_cb), window);
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_deselect_cb), window);
	}
}

static void
set_action_properties (GtkActionGroup *image_group, 
		       GtkActionGroup *collection_group)
{
        GtkAction *action;

        action = gtk_action_group_get_action (collection_group, "GoPrevious");
        g_object_set (action, "short_label", _("Previous"), NULL);
        g_object_set (action, "is-important", TRUE, NULL);

        action = gtk_action_group_get_action (collection_group, "GoNext");
        g_object_set (action, "short_label", _("Next"), NULL);
        g_object_set (action, "is-important", TRUE, NULL);

        action = gtk_action_group_get_action (image_group, "EditRotate90");
        g_object_set (action, "short_label", _("Right"), NULL);

        action = gtk_action_group_get_action (image_group, "EditRotate270");
        g_object_set (action, "short_label", _("Left"), NULL);

        action = gtk_action_group_get_action (image_group, "ViewZoomIn");
        g_object_set (action, "short_label", _("In"), NULL);

        action = gtk_action_group_get_action (image_group, "ViewZoomOut");
        g_object_set (action, "short_label", _("Out"), NULL);

        action = gtk_action_group_get_action (image_group, "ViewZoomNormal");
        g_object_set (action, "short_label", _("Normal"), NULL);

        action = gtk_action_group_get_action (image_group, "ViewZoomFit");
        g_object_set (action, "short_label", _("Fit"), NULL);
}

static void 
eog_window_construct_ui (EogWindow *window)
{
	EogWindowPrivate *priv;

	GError *error = NULL;
	
	GtkWidget *menubar;
	GtkWidget *toolbar;
	GtkWidget *popup;
	GtkWidget *recent_widget;
	GtkWidget *sw;
	GtkWidget *frame;

	EggRecentModel *recent_model;
	
	GConfEntry *entry;

	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	priv->box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), priv->box);
	gtk_widget_show (priv->box);

	priv->ui_mgr = gtk_ui_manager_new ();

	priv->actions_window = gtk_action_group_new ("MenuActionsWindow");
	
	gtk_action_group_set_translation_domain (priv->actions_window, 
						 GETTEXT_PACKAGE);
	
	gtk_action_group_add_actions (priv->actions_window, 
				      action_entries_window, 
				      G_N_ELEMENTS (action_entries_window), 
				      window);

	gtk_action_group_add_toggle_actions (priv->actions_window, 
					     toggle_entries_window, 
					     G_N_ELEMENTS (toggle_entries_window), 
					     window);

	gtk_ui_manager_insert_action_group (priv->ui_mgr, priv->actions_window, 0);

	priv->actions_image = gtk_action_group_new ("MenuActionsImage");
	gtk_action_group_set_translation_domain (priv->actions_image, 
						 GETTEXT_PACKAGE);

	gtk_action_group_add_actions (priv->actions_image, 
				      action_entries_image, 
				      G_N_ELEMENTS (action_entries_image), 
				      window);
	
	gtk_action_group_add_toggle_actions (priv->actions_image, 
					     toggle_entries_image, 
					     G_N_ELEMENTS (toggle_entries_image), 
					     window);

	gtk_ui_manager_insert_action_group (priv->ui_mgr, priv->actions_image, 0);

	priv->actions_collection = gtk_action_group_new ("MenuActionsCollection");
	gtk_action_group_set_translation_domain (priv->actions_collection, 
						 GETTEXT_PACKAGE);
	
	gtk_action_group_add_actions (priv->actions_collection, 
				      action_entries_collection, 
				      G_N_ELEMENTS (action_entries_collection), 
				      window);

	set_action_properties (priv->actions_image, priv->actions_collection);

	gtk_ui_manager_insert_action_group (priv->ui_mgr, priv->actions_collection, 0);

	if (!gtk_ui_manager_add_ui_from_file (priv->ui_mgr, 
					      DATADIR"/eog-ui.xml", 
					      &error)) {
                g_warning ("building menus failed: %s", error->message);
                g_error_free (error);
        }

	g_signal_connect (priv->ui_mgr, "connect_proxy",
			  G_CALLBACK (connect_proxy_cb), window);
	g_signal_connect (priv->ui_mgr, "disconnect_proxy",
			  G_CALLBACK (disconnect_proxy_cb), window);

	menubar = gtk_ui_manager_get_widget (priv->ui_mgr, "/MainMenu");
	g_assert (GTK_IS_WIDGET (menubar));
	gtk_box_pack_start (GTK_BOX (priv->box), menubar, FALSE, FALSE, 0);
	gtk_widget_show (menubar);

	toolbar = gtk_ui_manager_get_widget (priv->ui_mgr, "/Toolbar");
	g_assert (GTK_IS_WIDGET (toolbar));
	gtk_box_pack_start (GTK_BOX (priv->box), toolbar, FALSE, FALSE, 0);
	gtk_widget_show (toolbar);

	gtk_window_add_accel_group (GTK_WINDOW (window), 
				    gtk_ui_manager_get_accel_group (priv->ui_mgr));

	recent_model = eog_application_get_recent_model (EOG_APP);
	
	recent_widget = gtk_ui_manager_get_widget (priv->ui_mgr, 
						   "/MainMenu/File/EggRecentDocuments");
	priv->recent_view = egg_recent_view_gtk_new (gtk_widget_get_parent (recent_widget),
						     recent_widget);
	egg_recent_view_gtk_set_trailing_sep (priv->recent_view, TRUE);
	egg_recent_view_set_model (EGG_RECENT_VIEW (priv->recent_view), recent_model);

	g_signal_connect (G_OBJECT (priv->recent_view), "activate",
			  G_CALLBACK (eog_window_open_recent_cb), window);

	priv->statusbar = eog_statusbar_new ();
	gtk_box_pack_end (GTK_BOX (priv->box), 
			  GTK_WIDGET (priv->statusbar),
			  FALSE, FALSE, 0);
	gtk_widget_show (priv->statusbar);

	priv->image_info_message_cid = 
		gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar), 
					      "image_info_message");
	priv->tip_message_cid = 
		gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar), 
					      "tip_message");

	priv->vbox = gtk_vbox_new (FALSE, 2);

 	priv->view = eog_scroll_view_new ();
	gtk_widget_set_size_request (GTK_WIDGET (priv->view), 100, 100);
	g_signal_connect (G_OBJECT (priv->view),
			  "zoom_changed",
			  G_CALLBACK (view_zoom_changed_cb), 
			  window);

	frame = gtk_widget_new (GTK_TYPE_FRAME, 
				"shadow-type", GTK_SHADOW_IN, 
				NULL);
	gtk_container_add (GTK_CONTAINER (frame), priv->view);

	gtk_box_pack_start_defaults (GTK_BOX (priv->vbox), frame);

	priv->thumbview = eog_thumb_view_new ();

	/* this will arrange the view in one single row */
	gtk_icon_view_set_columns (GTK_ICON_VIEW (priv->thumbview), G_MAXINT);

	/* giving shape to the view */
	gtk_widget_set_size_request (GTK_WIDGET (priv->thumbview), 0, 135);
	gtk_icon_view_set_margin (GTK_ICON_VIEW (priv->thumbview), 0);
	gtk_icon_view_set_row_spacing (GTK_ICON_VIEW (priv->thumbview), 0);
	gtk_icon_view_set_item_width (GTK_ICON_VIEW (priv->thumbview), 120);

	g_signal_connect (G_OBJECT (priv->thumbview), "selection_changed",
			  G_CALLBACK (handle_image_selection_changed_cb), window);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (sw), priv->thumbview);

	popup = gtk_ui_manager_get_widget (priv->ui_mgr, "/ThumbnailPopup");
	eog_thumb_view_set_thumbnail_popup (priv->thumbview, GTK_MENU (popup));

	gtk_box_pack_start (GTK_BOX (priv->vbox), sw, FALSE, 0, 0);

	gtk_box_pack_start (GTK_BOX (priv->box), priv->vbox, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (priv->vbox));

	//set_drag_dest (window);

	update_ui_visibility (window);
	
	entry = gconf_client_get_entry (priv->client, 
					EOG_CONF_VIEW_INTERPOLATE, 
					NULL, TRUE, NULL);
	if (entry != NULL) {
		eog_window_interp_type_changed_cb (priv->client, 0, entry, window);
		gconf_entry_free (entry);
		entry = NULL;
	}

	entry = gconf_client_get_entry (priv->client, 
					EOG_CONF_VIEW_TRANSPARENCY, 
					NULL, TRUE, NULL);
	if (entry != NULL) {
		eog_window_transparency_changed_cb (priv->client, 0, entry, window);
		gconf_entry_free (entry);
		entry = NULL;
	}

	entry = gconf_client_get_entry (priv->client, 
					EOG_CONF_VIEW_TRANS_COLOR, 
					NULL, TRUE, NULL);
	if (entry != NULL) {
		eog_window_trans_color_changed_cb (priv->client, 0, entry, window);
		gconf_entry_free (entry);
		entry = NULL;
	}
}

static void
eog_window_init (EogWindow *window)
{
	GdkGeometry hints;

	hints.min_width  = EOG_WINDOW_DEFAULT_WIDTH;
	hints.min_height = EOG_WINDOW_DEFAULT_HEIGHT;

	window->priv = EOG_WINDOW_GET_PRIVATE (window);

	window->priv->client = gconf_client_get_default ();

	gconf_client_add_dir (window->priv->client, EOG_CONF_DIR,
			      GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);

	gconf_client_notify_add (window->priv->client,
				 EOG_CONF_VIEW_INTERPOLATE,
				 eog_window_interp_type_changed_cb,
				 window, NULL, NULL);
	
	gconf_client_notify_add (window->priv->client,
				 EOG_CONF_VIEW_TRANSPARENCY,
				 eog_window_transparency_changed_cb,
				 window, NULL, NULL);

	gconf_client_notify_add (window->priv->client,
				 EOG_CONF_VIEW_TRANS_COLOR,
				 eog_window_trans_color_changed_cb,
				 window, NULL, NULL);

	window->priv->store = NULL;
	window->priv->image = NULL;

	gtk_window_set_geometry_hints (GTK_WINDOW (window),
				       GTK_WIDGET (window),
				       &hints,
				       GDK_HINT_MIN_SIZE);

	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);

	window->priv->mode = EOG_WINDOW_MODE_NORMAL;
	
	eog_window_construct_ui (window);
}

static void
eog_window_dispose (GObject *object)
{
	EogWindow *window;
	EogWindowPrivate *priv;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_WINDOW (object));

	window = EOG_WINDOW (object);
	priv = window->priv;

	if (priv->store != NULL) {
		g_object_unref (priv->store);
		priv->store = NULL;
	}

	if (priv->image != NULL) {
		g_object_unref (priv->image);
		priv->image = NULL;
	}

	if (window->priv->fullscreen_popup != NULL) {
		gtk_widget_destroy (priv->fullscreen_popup);
		priv->fullscreen_popup = NULL;
	}

	fullscreen_clear_timeout (window);

	if (priv->recent_view != NULL) {
		g_object_unref (priv->recent_view);
		priv->recent_view = NULL;
	}

	if (priv->load_job != NULL) {
		eog_window_clear_load_job (window);
	}

	if (priv->transform_job != NULL) {
		eog_window_clear_transform_job (window);
	}

	if (priv->client) {
		gconf_client_remove_dir (priv->client, EOG_CONF_DIR, NULL);
		g_object_unref (priv->client);
		priv->client = NULL;
	}

	G_OBJECT_CLASS (eog_window_parent_class)->dispose (object);
}

static void
eog_window_finalize (GObject *object)
{
        GList *windows = eog_application_get_windows (EOG_APP);

	g_return_if_fail (EOG_IS_WINDOW (object));

        if (windows == NULL) {
                eog_application_shutdown (EOG_APP);
        } else {
                g_list_free (windows);
	}
        G_OBJECT_CLASS (eog_window_parent_class)->finalize (object);
}

static gint
eog_window_delete (GtkWidget *widget, GdkEventAny *event)
{
	g_return_val_if_fail (EOG_IS_WINDOW (widget), FALSE);

	gtk_widget_destroy (widget);

	return TRUE;
}

static gint
eog_window_key_press (GtkWidget *widget, GdkEventKey *event)
{
	gint result;

	result = FALSE;

	switch (event->keyval) {
	case GDK_Escape:
		if (EOG_WINDOW (widget)->priv->mode == EOG_WINDOW_MODE_FULLSCREEN) {
			eog_window_stop_fullscreen (EOG_WINDOW (widget));
		}
		break;
	case GDK_F:
	case GDK_f:
		if (EOG_WINDOW (widget)->priv->mode == EOG_WINDOW_MODE_FULLSCREEN) {
			eog_window_stop_fullscreen (EOG_WINDOW (widget));
		} else {
			eog_window_run_fullscreen (EOG_WINDOW (widget));
		}
		break;
	case GDK_Q:
	case GDK_q:
		gtk_widget_destroy (widget);
		result = TRUE;
		break;
	case GDK_Up:
	case GDK_Left:
	case GDK_Page_Up:
		if (!eog_scroll_view_scrollbars_visible (EOG_SCROLL_VIEW (EOG_WINDOW (widget)->priv->view))) {
			eog_thumb_view_select_single (EOG_THUMB_VIEW (EOG_WINDOW (widget)->priv->thumbview), 
						      EOG_THUMB_VIEW_SELECT_LEFT);
			result = TRUE;		
		}
		break;
	case GDK_Down:
	case GDK_Right:
	case GDK_Page_Down:
		if (!eog_scroll_view_scrollbars_visible (EOG_SCROLL_VIEW (EOG_WINDOW (widget)->priv->view))) {
			eog_thumb_view_select_single (EOG_THUMB_VIEW (EOG_WINDOW (widget)->priv->thumbview), 
						      EOG_THUMB_VIEW_SELECT_RIGHT);
			result = TRUE;
		}
		break;
	}

	if (result == FALSE && GTK_WIDGET_CLASS (eog_window_parent_class)->key_press_event) {
		result = (* GTK_WIDGET_CLASS (eog_window_parent_class)->key_press_event) (widget, event);
	}

	return result;
}

static gboolean
eog_window_configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
	EogWindow *window;

	g_return_val_if_fail (EOG_IS_WINDOW (widget), TRUE);
	
	window = EOG_WINDOW (widget);

	GTK_WIDGET_CLASS (eog_window_parent_class)->configure_event (widget, event);

	return FALSE;
}

static gboolean
eog_window_window_state_event (GtkWidget *widget,
			       GdkEventWindowState *event)
{
	EogWindow *window;

	g_return_val_if_fail (EOG_IS_WINDOW (widget), TRUE);

	window = EOG_WINDOW (widget);

	if (event->changed_mask &
	    (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN))	{
		gboolean show;

		show = !(event->new_window_state &
		         (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN));

		eog_statusbar_set_has_resize_grip (EOG_STATUSBAR (window->priv->statusbar),
						   show);
	}

	return FALSE;
}

static void
eog_window_unrealize (GtkWidget *widget)
{
	EogWindow *window;
	
	g_return_if_fail (EOG_IS_WINDOW (widget));

	window = EOG_WINDOW (widget);

	GTK_WIDGET_CLASS (eog_window_parent_class)->unrealize (widget);
}

static gboolean
eog_window_focus_in_event (GtkWidget *widget, GdkEventFocus *event)
{
	EogWindow *window = EOG_WINDOW (widget);
	EogWindowPrivate *priv = window->priv;
	gboolean fullscreen;

	fullscreen = priv->mode == EOG_WINDOW_MODE_FULLSCREEN ||
		     priv->mode == EOG_WINDOW_MODE_SLIDESHOW;
	
	if (fullscreen) {
		show_fullscreen_popup (window);
	}
		
	return GTK_WIDGET_CLASS (eog_window_parent_class)->focus_in_event (widget, event);
}

static gboolean
eog_window_focus_out_event (GtkWidget *widget, GdkEventFocus *event)
{
	EogWindow *window = EOG_WINDOW (widget);
	EogWindowPrivate *priv = window->priv;
	gboolean fullscreen;

	fullscreen = priv->mode == EOG_WINDOW_MODE_FULLSCREEN ||
		     priv->mode == EOG_WINDOW_MODE_SLIDESHOW;

	if (fullscreen) {
		gtk_widget_hide_all (priv->fullscreen_popup);
	}

	return GTK_WIDGET_CLASS (eog_window_parent_class)->focus_out_event (widget, event);
}

static void
eog_window_class_init (EogWindowClass *class)
{
	GObjectClass *g_object_class = (GObjectClass *) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) class;

	g_object_class->dispose = eog_window_dispose;
	g_object_class->finalize = eog_window_finalize;

	widget_class->delete_event = eog_window_delete;
	widget_class->key_press_event = eog_window_key_press;
        widget_class->configure_event = eog_window_configure_event;
        widget_class->window_state_event = eog_window_window_state_event;
	widget_class->unrealize = eog_window_unrealize;
	widget_class->focus_in_event = eog_window_focus_in_event;
	widget_class->focus_out_event = eog_window_focus_out_event;
	//widget_class->drag_data_received = eog_window_drag_data_received;

	g_type_class_add_private (g_object_class, sizeof (EogWindowPrivate));
}

GtkWidget*
eog_window_new ()
{
	EogWindow *window;

	window = EOG_WINDOW (g_object_new (EOG_TYPE_WINDOW, 
					   "type", GTK_WINDOW_TOPLEVEL,
					   NULL));

	return GTK_WIDGET (window);
}

const char*  
eog_window_get_uri (EogWindow *window)
{
	return NULL;
}

static void
eog_job_model_cb (EogJobModel *job, gpointer data)
{
	EogWindow *window;
	EogWindowPrivate *priv;

#ifdef HAVE_LCMS
        int i;
#endif
	
        g_return_if_fail (EOG_IS_WINDOW (data));
	
	window = EOG_WINDOW (data);
	priv = window->priv;

	if (priv->store != NULL) {
		g_object_unref (priv->store);
		priv->store = NULL;
	}

	priv->store = g_object_ref (job->store);

#ifdef HAVE_LCMS
	gint n_images = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->store), NULL);

	/* Colour-correct the images */
	for (i = 0; i < n_images; i++) {
		//eog_image_apply_display_profile (eog_list_store_get_image_by_pos (priv->store, i),
		//				 get_screen_profile (window));
	}
#endif

	eog_thumb_view_set_model (EOG_THUMB_VIEW (priv->thumbview), priv->store);

	update_action_groups_state (window);
}

void
eog_window_open_uri_list (EogWindow *window, GSList *uri_list)
{
	EogJob *job;
	
	job = eog_job_model_new (uri_list);

	g_signal_connect (job,
			  "finished",
			  G_CALLBACK (eog_job_model_cb),
			  window);

	eog_job_queue_add_job (job);
	g_object_unref (job);
}

gboolean
eog_window_is_empty (EogWindow *window)
{
        EogWindowPrivate *priv;
        gboolean empty = TRUE;

        g_return_val_if_fail (EOG_IS_WINDOW (window), FALSE);

        priv = window->priv;

        if (priv->store != NULL) {
                empty = (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->store), NULL) == 0);
        }

        return empty;
}
