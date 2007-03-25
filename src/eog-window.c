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

#include <math.h>

#include "eog-window.h"
#include "eog-scroll-view.h"
#include "eog-debug.h"
#include "eog-file-chooser.h"
#include "eog-thumb-view.h"
#include "eog-list-store.h"
#include "eog-statusbar.h"
#include "eog-preferences-dialog.h"
#include "eog-properties-dialog.h"
#include "eog-message-area.h"
#include "eog-error-message-area.h"
#include "eog-application.h"
#include "eog-thumb-nav.h"
#include "eog-config-keys.h"
#include "eog-job-queue.h"
#include "eog-jobs.h"
#include "eog-util.h"
#include "eog-thumbnail.h"
#include "eog-print-image-setup.h"
#include "eog-save-as-dialog-helper.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#include <gtk/gtk.h>
#include <gtk/gtkprintunixdialog.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <gconf/gconf-client.h>

#if HAVE_LCMS
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <lcms.h>
#endif

#define EOG_WINDOW_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EOG_TYPE_WINDOW, EogWindowPrivate))

G_DEFINE_TYPE (EogWindow, eog_window, GTK_TYPE_WINDOW);

#define EOG_WINDOW_MIN_WIDTH  440
#define EOG_WINDOW_MIN_HEIGHT 350

#define EOG_WINDOW_DEFAULT_WIDTH  540
#define EOG_WINDOW_DEFAULT_HEIGHT 450

#define EOG_WINDOW_FULLSCREEN_TIMEOUT 5 * 1000

#define EOG_RECENT_FILES_GROUP  "Eye of Gnome"
#define EOG_RECENT_FILES_LIMIT  5

typedef enum {
	EOG_WINDOW_STATUS_UNKNOWN,
	EOG_WINDOW_STATUS_INIT,
	EOG_WINDOW_STATUS_NORMAL
} EogWindowStatus;

typedef enum {
	EOG_WINDOW_MODE_UNKNOWN,
	EOG_WINDOW_MODE_NORMAL,
	EOG_WINDOW_MODE_FULLSCREEN,
	EOG_WINDOW_MODE_SLIDESHOW
} EogWindowMode;

enum {
	PROP_0,
	PROP_STARTUP_FLAGS
};

enum {
	SIGNAL_PREPARED,
	SIGNAL_LAST
};

static gint signals[SIGNAL_LAST];

struct _EogWindowPrivate {
        GConfClient         *client;

        EogListStore        *store;
        EogImage            *image;
	EogWindowMode        mode;
	EogWindowStatus      status;

        GtkUIManager        *ui_mgr;
        GtkWidget           *box;
        GtkWidget           *layout;
        GtkWidget           *cbox;
        GtkWidget           *view;
        GtkWidget           *thumbview;
        GtkWidget           *statusbar;
        GtkWidget           *nav;
	GtkWidget           *message_area;
	GObject             *properties_dlg;

        GtkActionGroup      *actions_window;
        GtkActionGroup      *actions_image;
        GtkActionGroup      *actions_collection;
        GtkActionGroup      *actions_recent;

	GtkWidget           *fullscreen_popup;
	GSource             *fullscreen_timeout_source;

	gboolean             slideshow_loop;
	gint                 slideshow_switch_timeout;
	GSource             *slideshow_switch_source;

        GtkRecentManager    *recent_manager;
        guint		     recent_menu_id;

	GtkPrintSettings    *print_settings;
	GtkPageSetup        *print_page_setup;

        EogJob              *load_job;
        EogJob              *transform_job;
	EogJob              *save_job;

        guint                image_info_message_cid;
        guint                tip_message_cid;

        EogStartupFlags      flags;
	GSList              *uri_list;

	gint                 collection_position;
	gboolean             collection_resizable;

	guint                open_with_menu_id;
 	GList*               mime_application_list;

#ifdef HAVE_LCMS
        cmsHPROFILE         *display_profile;
#endif
};

static void eog_window_cmd_fullscreen (GtkAction *action, gpointer user_data);
static void eog_window_run_fullscreen (EogWindow *window, gboolean slideshow);
static void eog_window_cmd_slideshow (GtkAction *action, gpointer user_data);
static void eog_window_stop_fullscreen (EogWindow *window, gboolean slideshow);
static void eog_job_load_cb (EogJobLoad *job, gpointer data);
static void eog_job_save_progress_cb (EogJobSave *job, float progress, gpointer data);
static void eog_job_progress_cb (EogJobLoad *job, float progress, gpointer data);
static void eog_job_transform_cb (EogJobTransform *job, gpointer data);
static void fullscreen_set_timeout (EogWindow *window);
static void fullscreen_clear_timeout (EogWindow *window);
static void update_action_groups_state (EogWindow *window);
static void open_with_launch_application_cb (GtkAction *action, gpointer callback_data);
static void eog_window_update_openwith_menu (EogWindow *window, EogImage *image);

static GQuark
eog_window_error_quark (void)
{
	static GQuark q = 0;

	if (q == 0)
		q = g_quark_from_static_string ("eog-window-error-quark");
	
	return q;
}

static void
eog_window_interp_type_changed_cb (GConfClient *client,
				   guint       cnxn_id,
				   GConfEntry  *entry,
				   gpointer    user_data)
{
	EogWindowPrivate *priv;
	gboolean interpolate = TRUE;

	eog_debug (DEBUG_PREFERENCES);

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
eog_window_scroll_wheel_zoom_changed_cb (GConfClient *client,
				         guint       cnxn_id,
				         GConfEntry  *entry,
				         gpointer    user_data)
{
	EogWindowPrivate *priv;
	gboolean scroll_wheel_zoom = FALSE;

	eog_debug (DEBUG_PREFERENCES);

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (priv->view));

	if (entry->value != NULL && entry->value->type == GCONF_VALUE_BOOL) {
		scroll_wheel_zoom = gconf_value_get_bool (entry->value);
	}

	eog_scroll_view_set_scroll_wheel_zoom (EOG_SCROLL_VIEW (priv->view), 
					       scroll_wheel_zoom);
}

static void
eog_window_zoom_multiplier_changed_cb (GConfClient *client,
				       guint       cnxn_id,
				       GConfEntry  *entry,
				       gpointer    user_data)
{
	EogWindowPrivate *priv;
	gdouble multiplier = 0.05;

	eog_debug (DEBUG_PREFERENCES);

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (priv->view));

	if (entry->value != NULL && entry->value->type == GCONF_VALUE_FLOAT) {
		multiplier = gconf_value_get_float (entry->value);
	}

	eog_scroll_view_set_zoom_multiplier (EOG_SCROLL_VIEW (priv->view), 
					     multiplier);
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

	eog_debug (DEBUG_PREFERENCES);

	priv = EOG_WINDOW (user_data)->priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (priv->view));

	if (entry->value != NULL && entry->value->type == GCONF_VALUE_STRING) {
		value = gconf_value_get_string (entry->value);
	}

	if (g_ascii_strcasecmp (value, "COLOR") == 0) {
		GdkColor color;
		char *color_str;

		color_str = gconf_client_get_string (priv->client,
						     EOG_CONF_VIEW_TRANS_COLOR, NULL);
		if (gdk_color_parse (color_str, &color)) {
			eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->view),
							  TRANSP_COLOR, &color);
		}
		g_free (color_str);
	} else if (g_ascii_strcasecmp (value, "CHECK_PATTERN") == 0) {
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

	eog_debug (DEBUG_PREFERENCES);

	priv = EOG_WINDOW (user_data)->priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (priv->view));

	value = gconf_client_get_string (priv->client, 
					 EOG_CONF_VIEW_TRANSPARENCY, 
					 NULL);

	if (g_ascii_strcasecmp (value, "COLOR") != 0) {
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
	g_free (value);
}

static void
eog_window_scroll_buttons_changed_cb (GConfClient *client,
				      guint       cnxn_id,
				      GConfEntry  *entry,
				      gpointer    user_data)
{
	EogWindowPrivate *priv;
	gboolean show_buttons = TRUE;

	eog_debug (DEBUG_PREFERENCES);

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	g_return_if_fail (EOG_IS_SCROLL_VIEW (priv->view));

	if (entry->value != NULL && entry->value->type == GCONF_VALUE_BOOL) {
		show_buttons = gconf_value_get_bool (entry->value);
	}

	eog_thumb_nav_set_show_buttons (EOG_THUMB_NAV (priv->nav), 
					show_buttons);
}

static void
eog_window_collection_mode_changed_cb (GConfClient *client,
				       guint       cnxn_id,
				       GConfEntry  *entry,
				       gpointer    user_data)
{
	EogWindowPrivate *priv;
	GConfEntry *mode_entry;
	GtkWidget *frame;
	EogThumbNavMode mode = EOG_THUMB_NAV_MODE_ONE_ROW;
	gint position = 0;
	gboolean resizable = FALSE;

	eog_debug (DEBUG_PREFERENCES);

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	mode_entry = gconf_client_get_entry (priv->client, 
		 			     EOG_CONF_UI_IMAGE_COLLECTION_POSITION, 
					     NULL, TRUE, NULL);

	if (mode_entry->value != NULL && mode_entry->value->type == GCONF_VALUE_INT) {
		position = gconf_value_get_int (mode_entry->value);
	}

	mode_entry = gconf_client_get_entry (priv->client, 
					     EOG_CONF_UI_IMAGE_COLLECTION_RESIZABLE, 
					     NULL, TRUE, NULL);

	if (mode_entry->value != NULL && mode_entry->value->type == GCONF_VALUE_BOOL) {
		resizable = gconf_value_get_bool (mode_entry->value);
	}

	if (priv->collection_position == position && 
	    priv->collection_resizable == resizable)
		return;

	priv->collection_position = position;
	priv->collection_resizable = resizable;

	frame = priv->view->parent;

	g_object_ref (frame);
	g_object_ref (priv->nav);

	gtk_container_remove (GTK_CONTAINER (priv->layout), frame);
	gtk_container_remove (GTK_CONTAINER (priv->layout), priv->nav);

	gtk_widget_destroy (priv->layout);

	switch (position) {
	case 0:
	case 2:
		if (resizable) {
			mode = EOG_THUMB_NAV_MODE_MULTIPLE_ROWS;

			priv->layout = gtk_vpaned_new ();

			if (position == 0) {
				gtk_paned_pack1 (GTK_PANED (priv->layout), frame, TRUE, FALSE);
				gtk_paned_pack2 (GTK_PANED (priv->layout), priv->nav, FALSE, TRUE);
			} else {
				gtk_paned_pack1 (GTK_PANED (priv->layout), priv->nav, FALSE, TRUE);
				gtk_paned_pack2 (GTK_PANED (priv->layout), frame, TRUE, FALSE);
			}
		} else {
			mode = EOG_THUMB_NAV_MODE_ONE_ROW;

			priv->layout = gtk_vbox_new (FALSE, 2);

			if (position == 0) {
				gtk_box_pack_start (GTK_BOX (priv->layout), frame, TRUE, TRUE, 0);
				gtk_box_pack_start (GTK_BOX (priv->layout), priv->nav, FALSE, FALSE, 0);
			} else {
				gtk_box_pack_start (GTK_BOX (priv->layout), priv->nav, FALSE, FALSE, 0);
				gtk_box_pack_start (GTK_BOX (priv->layout), frame, TRUE, TRUE, 0);
			}
		}
		break;

	case 1:
	case 3:
		if (resizable) {
			mode = EOG_THUMB_NAV_MODE_MULTIPLE_COLUMNS;

			priv->layout = gtk_hpaned_new ();

			if (position == 1) {
				gtk_paned_pack1 (GTK_PANED (priv->layout), priv->nav, FALSE, TRUE);
				gtk_paned_pack2 (GTK_PANED (priv->layout), frame, TRUE, FALSE);
			} else {
				gtk_paned_pack1 (GTK_PANED (priv->layout), frame, TRUE, FALSE);
				gtk_paned_pack2 (GTK_PANED (priv->layout), priv->nav, FALSE, TRUE);
			}
		} else {
			mode = EOG_THUMB_NAV_MODE_ONE_COLUMN;

			priv->layout = gtk_hbox_new (FALSE, 2);

			if (position == 1) {
				gtk_box_pack_start (GTK_BOX (priv->layout), priv->nav, FALSE, FALSE, 0);
				gtk_box_pack_start (GTK_BOX (priv->layout), frame, TRUE, TRUE, 0);
			} else {
				gtk_box_pack_start (GTK_BOX (priv->layout), frame, TRUE, TRUE, 0);
				gtk_box_pack_start (GTK_BOX (priv->layout), priv->nav, FALSE, FALSE, 0);
			}
		}

		break;
	}

	gtk_box_pack_end (GTK_BOX (priv->cbox), priv->layout, TRUE, TRUE, 0);

	eog_thumb_nav_set_mode (EOG_THUMB_NAV (priv->nav), mode);

	if (priv->mode != EOG_WINDOW_STATUS_UNKNOWN) {
		update_action_groups_state (EOG_WINDOW (user_data));
	}
}

#ifdef HAVE_LCMS
static cmsHPROFILE *
eog_window_get_display_profile (GdkScreen *screen)
{
	Display *dpy;
	Atom icc_atom, type;
	int format;
	gulong nitems;
	gulong bytes_after;
	guchar *str;
	int result;
	cmsHPROFILE *profile;

	dpy = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));

	icc_atom = gdk_x11_get_xatom_by_name_for_display (gdk_screen_get_display (screen), 
							  "_ICC_PROFILE");
	
	result = XGetWindowProperty (dpy, 
				     GDK_WINDOW_XID (gdk_screen_get_root_window (screen)),
				     icc_atom, 
				     0, 
				     G_MAXLONG,
				     False, 
				     XA_CARDINAL, 
				     &type, 
				     &format, 
				     &nitems,
				     &bytes_after, 
                                     (guchar **)&str);

	/* TODO: handle bytes_after != 0 */
	
	if (nitems) {
		profile = cmsOpenProfileFromMem(str, nitems);

		XFree (str);

		return profile;
	} else {
		eog_debug_message (DEBUG_LCMS, "No profile, not correcting");

		return NULL;
	}
}
#endif

static void
update_status_bar (EogWindow *window)
{
	EogWindowPrivate *priv;
	char *str = NULL;
	int n_images;
	int pos;

	g_return_if_fail (EOG_IS_WINDOW (window));
	
	eog_debug (DEBUG_WINDOW);

	priv = window->priv;

	if (priv->image != NULL &&
	    eog_image_has_data (priv->image, EOG_IMAGE_DATA_ALL)) {
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

	}

	n_images = eog_list_store_length (EOG_LIST_STORE (priv->store));

	if (n_images > 0) {
		pos = eog_list_store_get_pos_by_image (EOG_LIST_STORE (priv->store), 
						       priv->image);

		/* Images: (image pos) / (n_total_images) */
		eog_statusbar_set_image_number (EOG_STATUSBAR (priv->statusbar), 
						pos + 1, 
						n_images);
	}
 
	gtk_statusbar_pop (GTK_STATUSBAR (priv->statusbar), 
			   priv->image_info_message_cid);
	
	gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar), 
			    priv->image_info_message_cid, str ? str : "");

	g_free (str);
}

static void
eog_window_set_message_area (EogWindow *window,
		             GtkWidget *message_area)
{
	if (window->priv->message_area == message_area)
		return;

	if (window->priv->message_area != NULL)
		gtk_widget_destroy (window->priv->message_area);

	window->priv->message_area = message_area;

	if (message_area == NULL) return;

	gtk_box_pack_start (GTK_BOX (window->priv->cbox),
			    window->priv->message_area,
			    FALSE,
			    FALSE,
			    0);

	g_object_add_weak_pointer (G_OBJECT (window->priv->message_area), 
				   (gpointer *) &window->priv->message_area);
}

static void
update_action_groups_state (EogWindow *window)
{
	EogWindowPrivate *priv;
	GtkAction *action_collection;
	GtkAction *action_fscreen;
	GtkAction *action_sshow;
	GtkAction *action_save;
	GtkAction *action_save_as;
	GtkAction *action_print;
	GtkAction *action_page_setup;
	gboolean save_disabled = FALSE;
	gboolean print_disabled = FALSE;
	gboolean page_setup_disabled = FALSE;
	gboolean show_image_collection = FALSE;
	gboolean fullscreen_mode = FALSE;
	int n_images = 0;

	g_return_if_fail (EOG_IS_WINDOW (window));

	eog_debug (DEBUG_WINDOW);

	priv = window->priv;

	fullscreen_mode = priv->mode == EOG_WINDOW_MODE_FULLSCREEN ||
			  priv->mode == EOG_WINDOW_MODE_SLIDESHOW;

	action_collection = 
		gtk_action_group_get_action (priv->actions_window, 
					     "ViewImageCollection");

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

	action_print = 
		gtk_action_group_get_action (priv->actions_image, 
					     "FilePrint");

	action_page_setup = 
		gtk_action_group_get_action (priv->actions_image, 
					     "FilePageSetup");

	g_assert (action_collection != NULL);
	g_assert (action_fscreen != NULL);
	g_assert (action_sshow != NULL);
	g_assert (action_save != NULL);
	g_assert (action_save_as != NULL);
	g_assert (action_print != NULL);
	g_assert (action_page_setup != NULL);

	if (priv->store != NULL) {
		n_images = eog_list_store_length (EOG_LIST_STORE (priv->store));
	}

	if (n_images == 0) {
		gtk_widget_hide_all (priv->layout);

		gtk_action_group_set_sensitive (priv->actions_window,      TRUE);
		gtk_action_group_set_sensitive (priv->actions_image,       FALSE);
		gtk_action_group_set_sensitive (priv->actions_collection,  FALSE);

		gtk_action_set_sensitive (action_fscreen, FALSE);
		gtk_action_set_sensitive (action_sshow,   FALSE);

		/* If there are no images on model, initialization
 		   stops here. */
		if (priv->status == EOG_WINDOW_STATUS_INIT) {
			priv->status = EOG_WINDOW_STATUS_NORMAL;
		}
	} else {
		if (priv->flags & EOG_STARTUP_DISABLE_COLLECTION) {
			gconf_client_set_bool (priv->client, 
					       EOG_CONF_UI_IMAGE_COLLECTION, 
					       FALSE,
					       NULL);

			show_image_collection = FALSE;
		} else {
			show_image_collection = 
				gconf_client_get_bool (priv->client, 
						       EOG_CONF_UI_IMAGE_COLLECTION, 
						       NULL);
		}

		show_image_collection = show_image_collection &&
					n_images > 1 &&
					!fullscreen_mode;

		gtk_widget_show (priv->layout);
		gtk_widget_show_all (priv->view->parent);

		if (show_image_collection) 
			gtk_widget_show (priv->nav);

		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action_collection),
					      show_image_collection);

		gtk_action_group_set_sensitive (priv->actions_window, TRUE);
		gtk_action_group_set_sensitive (priv->actions_image,  TRUE);
		
		gtk_action_set_sensitive (action_fscreen, TRUE);

		if (n_images == 1) {
			gtk_action_group_set_sensitive (priv->actions_collection,  FALSE);
			gtk_action_set_sensitive (action_collection, FALSE);
			gtk_action_set_sensitive (action_sshow, FALSE);
		} else {
			gtk_action_group_set_sensitive (priv->actions_collection,  TRUE);
			gtk_action_set_sensitive (action_sshow, TRUE);
		}

		gtk_widget_grab_focus (priv->view);
	}

	save_disabled = gconf_client_get_bool (priv->client, 
					       EOG_CONF_DESKTOP_CAN_SAVE, 
					       NULL);

	if (save_disabled) {
		gtk_action_set_sensitive (action_save, FALSE);
		gtk_action_set_sensitive (action_save_as, FALSE);
	}

	print_disabled = gconf_client_get_bool (priv->client, 
						EOG_CONF_DESKTOP_CAN_PRINT, 
						NULL);

	if (print_disabled) {
		gtk_action_set_sensitive (action_print, FALSE);
	}

	page_setup_disabled = gconf_client_get_bool (priv->client, 
						     EOG_CONF_DESKTOP_CAN_SETUP_PAGE, 
						     NULL);

	if (page_setup_disabled) {
		gtk_action_set_sensitive (action_page_setup, FALSE);
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
add_uri_to_recent_files (EogWindow *window, GnomeVFSURI *uri)
{
	gchar *text_uri;
	GtkRecentData *recent_data;
	static gchar *groups[2] = { EOG_RECENT_FILES_GROUP , NULL }; 

	g_return_if_fail (EOG_IS_WINDOW (window));
	if (uri == NULL) return;

	/* The password gets stripped here because ~/.recently-used.xbel is
	 * readable by everyone (chmod 644). It also makes the workaround
	 * for the bug with gtk_recent_info_get_uri_display() easier
	 * (see the comment in eog_window_update_recent_files_menu()). */
	text_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);

	if (text_uri == NULL)
		return;

	recent_data = g_slice_new (GtkRecentData);
	recent_data->display_name = NULL;
	recent_data->description = NULL;
	recent_data->mime_type = (gchar *) gnome_vfs_get_mime_type_from_uri (uri);
	recent_data->app_name = (gchar *) g_get_application_name ();
	recent_data->app_exec = g_strjoin(" ", g_get_prgname (), "%u", NULL);
	recent_data->groups = groups;
	recent_data->is_private = FALSE;

	gtk_recent_manager_add_full (GTK_RECENT_MANAGER (window->priv->recent_manager), 
				     text_uri, 
				     recent_data);

	g_free (recent_data->app_exec);
	g_free (text_uri);

	g_slice_free (GtkRecentData, recent_data);
}

static void
image_thumb_changed_cb (EogImage *image, gpointer data)
{
	EogWindow *window;
	EogWindowPrivate *priv;
	GdkPixbuf *thumb;

	g_return_if_fail (EOG_IS_WINDOW (data));

	window = EOG_WINDOW (data);
	priv = window->priv;

	thumb = eog_image_get_thumbnail (image);

	if (thumb != NULL) {
		gtk_window_set_icon (GTK_WINDOW (window), thumb);

		if (window->priv->properties_dlg != NULL) {
			eog_properties_dialog_update (EOG_PROPERTIES_DIALOG (priv->properties_dlg), 
						      image);
		}

		g_object_unref (thumb);
	} else if (!GTK_WIDGET_VISIBLE (window->priv->nav)) {
		gint img_pos = eog_list_store_get_pos_by_image (window->priv->store, image);
		GtkTreePath *path = gtk_tree_path_new_from_indices (img_pos,-1);
		GtkTreeIter iter;

		gtk_tree_model_get_iter (GTK_TREE_MODEL (window->priv->store), &iter, path);
		eog_list_store_thumbnail_set (window->priv->store, &iter);
		gtk_tree_path_free (path);
	}
}

static void 
eog_window_display_image (EogWindow *window, EogImage *image)
{
	EogWindowPrivate *priv;
	GnomeVFSURI *uri;

	g_return_if_fail (EOG_IS_WINDOW (window));
	g_return_if_fail (EOG_IS_IMAGE (image));

	eog_debug (DEBUG_WINDOW);

	g_assert (eog_image_has_data (image, EOG_IMAGE_DATA_ALL));

	priv = window->priv;

	if (image != NULL) {
		g_signal_connect (image, 
				  "thumbnail_changed", 
				  G_CALLBACK (image_thumb_changed_cb), 
				  window);

		image_thumb_changed_cb (image, window);
	}

	eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), image);

	gtk_window_set_title (GTK_WINDOW (window), eog_image_get_caption (image));

	update_status_bar (window);

	uri = eog_image_get_uri (image);
	add_uri_to_recent_files (window, uri);
	gnome_vfs_uri_unref (uri);

	eog_window_update_openwith_menu (window, image);
}

static void
open_with_launch_application_cb (GtkAction *action, gpointer data){
	EogImage *image;
	GnomeVFSMimeApplication *app;
	GnomeVFSURI *uri;
	gchar *uri_string;
	GList *uris = NULL;
	
	image = EOG_IMAGE (data);
	uri = eog_image_get_uri (image);
	uri_string = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	uris = g_list_prepend (NULL, uri_string);
	app = g_object_get_data (G_OBJECT (action), "app");
	gnome_vfs_mime_application_launch (app, uris);
	gnome_vfs_uri_unref (uri);
	g_free (uri_string);
	g_list_free (uris);
}

static void
eog_window_update_openwith_menu (EogWindow *window, EogImage *image)
{
	GnomeVFSURI *uri;
	GList *iter;
	gchar *label, *tip, *string_uri, *mime_type;
	GtkAction *action;
	EogWindowPrivate *priv;
	
	uri = eog_image_get_uri (image);
	string_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	mime_type = gnome_vfs_get_mime_type (string_uri);
	g_free (string_uri);
	gnome_vfs_uri_unref (uri);
	
	priv = window->priv;
	
	gtk_ui_manager_remove_ui (priv->ui_mgr, priv->open_with_menu_id);
	if (priv->mime_application_list != NULL) {
		gnome_vfs_mime_application_list_free (priv->mime_application_list);
		priv->mime_application_list = NULL;
	}
	
	if (mime_type != NULL) {
		GList *apps = gnome_vfs_mime_get_all_applications (mime_type);
		GList *next;
		
		for (iter = apps; iter; iter = next) {
			next = iter->next;
			
			GnomeVFSMimeApplication *app = iter->data;

			/* do not include eog itself */
			if (g_ascii_strcasecmp (gnome_vfs_mime_application_get_binary_name (app), 
						g_get_prgname ()) == 0) {
 				apps = g_list_remove_link (apps, iter);
				g_list_free1 (iter);
 				gnome_vfs_mime_application_free (app);
				continue;
			}
			
			label = g_strdup_printf (_("Open with \"%s\""), app->name);
			/* FIXME: use the tip once all the actions have tips */
			/* tip = g_strdup_printf (_("Use \"%s\" to open the selected item"), app->name); */
			tip = NULL;
			action = gtk_action_new (app->name, label, tip, NULL);
			g_free (label);
			/* g_free (tip); */
			
			g_object_set_data (G_OBJECT (action), "app", app);

			g_signal_connect (G_OBJECT (action),
					  "activate",
					  G_CALLBACK (open_with_launch_application_cb),
					  image);
			
			gtk_action_group_add_action (priv->actions_image, action);
			
			gtk_ui_manager_add_ui (priv->ui_mgr,
			       		priv->open_with_menu_id,
			       		"/MainMenu/File/FileOpenWith/Applications Placeholder",
			       		app->name,
			       		app->name,
			       		GTK_UI_MANAGER_MENUITEM,
			       		FALSE);

			gtk_ui_manager_add_ui (priv->ui_mgr,
			       		priv->open_with_menu_id,
			       		"/ThumbnailPopup/FileOpenWith/Applications Placeholder",
			       		app->name,
			       		app->name,
			       		GTK_UI_MANAGER_MENUITEM,
			       		FALSE);
		}
		priv->mime_application_list = apps;
		g_free (mime_type);
	}
}

static void
eog_window_clear_load_job (EogWindow *window)
{
	EogWindowPrivate *priv = window->priv;
	
	if (priv->load_job != NULL) {
		if (!priv->load_job->finished)
			eog_job_queue_remove_job (priv->load_job);
	
		g_signal_handlers_disconnect_by_func (priv->load_job, 
						      eog_job_progress_cb, 
						      window);

		g_signal_handlers_disconnect_by_func (priv->load_job, 
						      eog_job_load_cb, 
						      window);

		eog_image_cancel_load (EOG_JOB_LOAD (priv->load_job)->image);

		g_object_unref (priv->load_job);
		priv->load_job = NULL;

		/* Hide statusbar */
		eog_statusbar_set_progress (EOG_STATUSBAR (priv->statusbar), 0);
	}
}

static void
eog_job_progress_cb (EogJobLoad *job, float progress, gpointer user_data) 
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));
	
	EogWindow *window = EOG_WINDOW (user_data);

	eog_statusbar_set_progress (EOG_STATUSBAR (window->priv->statusbar), 
				    progress);
}

static void
eog_job_save_progress_cb (EogJobSave *job, float progress, gpointer user_data) 
{
	EogWindowPrivate *priv;
	EogWindow *window;

	static EogImage *image = NULL;

	g_return_if_fail (EOG_IS_WINDOW (user_data));
	
	window = EOG_WINDOW (user_data);
	priv = window->priv;

	eog_statusbar_set_progress (EOG_STATUSBAR (priv->statusbar), 
				    progress);

	if (image != job->current_image) {
		gchar *str_image, *str_position, *status_message;
		guint n_images;
 
		image = job->current_image;

		n_images = g_list_length (job->images);

		str_image = eog_image_get_uri_for_display (image);

		str_position = g_strdup_printf ("(%d/%d)", 
						job->current_pos + 1, 
						n_images);

		status_message = g_strdup_printf (_("Saving image \"%s\" %s"), 
					          str_image,
						  n_images > 1 ? str_position : "");

		g_free (str_image);
		g_free (str_position);
	
		gtk_statusbar_pop (GTK_STATUSBAR (priv->statusbar), 
				   priv->image_info_message_cid);

		gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar),
				    priv->image_info_message_cid, 
				    status_message);
	
		g_free (status_message);
	}

	if (progress == 1.0)
		image = NULL;
}

static void
eog_window_obtain_desired_size (EogImage  *image, 
				gint       width, 
				gint       height,
				EogWindow *window)
{
	GdkScreen *screen;
	GdkRectangle monitor;
	gint final_width, final_height;
	gint screen_width, screen_height;
	gint window_width, window_height;
	gint img_width, img_height;
	gint view_width, view_height;
	gint deco_width, deco_height;

	gdk_threads_enter ();

	update_action_groups_state (window);

	img_width = width;
	img_height = height;

	if (!GTK_WIDGET_REALIZED (window->priv->view)) {
		gtk_widget_realize (window->priv->view);
	}

	view_width  = window->priv->view->allocation.width;
	view_height = window->priv->view->allocation.height;

	if (!GTK_WIDGET_REALIZED (GTK_WIDGET (window))) {
		gtk_widget_realize (GTK_WIDGET (window));
	}

	window_width  = GTK_WIDGET (window)->allocation.width;
	window_height = GTK_WIDGET (window)->allocation.height;

	screen = gtk_window_get_screen (GTK_WINDOW (window));

	gdk_screen_get_monitor_geometry (screen,
			gdk_screen_get_monitor_at_window (screen,
				GTK_WIDGET (window)->window),
			&monitor);

	screen_width  = monitor.width;
	screen_height = monitor.height;

	deco_width = window_width - view_width; 
	deco_height = window_height - view_height; 

	if (img_width > 0 && img_height > 0) {
		if ((img_width + deco_width > screen_width) ||
		    (img_height + deco_height > screen_height))
		{
			double factor;

			if (img_width > img_height) {
				factor = (screen_width * 0.75 - deco_width) / (double) img_width;
			} else {
				factor = (screen_height * 0.75 - deco_height) / (double) img_height;
			}

			img_width = img_width * factor;
			img_height = img_height * factor;
		}
	}

	final_width = MAX (EOG_WINDOW_MIN_WIDTH, img_width + deco_width);
	final_height = MAX (EOG_WINDOW_MIN_HEIGHT, img_height + deco_height);

	eog_debug_message (DEBUG_WINDOW, "Setting window size: %d x %d", final_width, final_height);

	gtk_window_set_default_size (GTK_WINDOW (window), final_width, final_height);

	gdk_threads_leave ();

	g_signal_emit (window, signals[SIGNAL_PREPARED], 0);
}

static void 
eog_window_error_message_area_response (EogMessageArea   *message_area,
					gint              response_id,
					EogWindow        *window)
{
	if (response_id != GTK_RESPONSE_OK) {
		eog_window_set_message_area (window, NULL);
	}
}

static void
eog_job_load_cb (EogJobLoad *job, gpointer data)
{
	EogWindow *window;
	EogWindowPrivate *priv;

        g_return_if_fail (EOG_IS_WINDOW (data));
	
	eog_debug (DEBUG_WINDOW);

	window = EOG_WINDOW (data);
	priv = window->priv;

	eog_statusbar_set_progress (EOG_STATUSBAR (priv->statusbar), 0.0);

	gtk_statusbar_pop (GTK_STATUSBAR (window->priv->statusbar), 
			   priv->image_info_message_cid);

	if (priv->image != NULL) {
		g_signal_handlers_disconnect_by_func (priv->image, 
						      image_thumb_changed_cb, 
						      window);

		g_object_unref (priv->image);
	}

	priv->image = g_object_ref (job->image);

	if (EOG_JOB (job)->error == NULL) {
#ifdef HAVE_LCMS
		eog_image_apply_display_profile (job->image, 
						 priv->display_profile);
#endif

		eog_window_display_image (window, job->image);
	} else {
		GtkWidget *message_area;

		message_area = eog_image_load_error_message_area_new (
					eog_image_get_caption (job->image),
					EOG_JOB (job)->error);

		g_signal_connect (message_area,
				  "response",
				  G_CALLBACK (eog_window_error_message_area_response),
				  window);

		gtk_window_set_icon (GTK_WINDOW (window), NULL);
		gtk_window_set_title (GTK_WINDOW (window), 
				      eog_image_get_caption (job->image));

		eog_window_set_message_area (window, message_area);

		eog_message_area_set_default_response (EOG_MESSAGE_AREA (message_area),
						       GTK_RESPONSE_CANCEL);

		gtk_widget_show (message_area);

		update_status_bar (window);

		eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), NULL);

        	if (window->priv->status == EOG_WINDOW_STATUS_INIT) {
			update_action_groups_state (window);
			g_signal_emit (window, signals[SIGNAL_PREPARED], 0);
		}
	}

	eog_window_clear_load_job (window);

        if (window->priv->status == EOG_WINDOW_STATUS_INIT) {
		window->priv->status = EOG_WINDOW_STATUS_NORMAL;

		g_signal_handlers_disconnect_by_func
			(job->image, 
			 G_CALLBACK (eog_window_obtain_desired_size), 
			 window);
	}

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

	g_signal_connect (priv->transform_job,
			  "progress",
			  G_CALLBACK (eog_job_progress_cb),
			  window);

	eog_job_queue_add_job (priv->transform_job);
}

static void
handle_image_selection_changed_cb (EogThumbView *thumbview, EogWindow *window) 
{
	EogWindowPrivate *priv;
	EogImage *image;
	gchar *status_message;
	gchar *str_image;

	priv = window->priv;

	if (eog_thumb_view_get_n_selected (EOG_THUMB_VIEW (priv->thumbview)) == 0)
		return;

	update_selection_ui_visibility (window);
	
	image = eog_thumb_view_get_first_selected_image (EOG_THUMB_VIEW (priv->thumbview));

	g_assert (EOG_IS_IMAGE (image));

	eog_window_clear_load_job (window);

	eog_window_set_message_area (window, NULL);

	if (eog_image_has_data (image, EOG_IMAGE_DATA_ALL)) {
		eog_window_display_image (window, image);
		return;
	}

	if (priv->status == EOG_WINDOW_STATUS_INIT) {
		g_signal_connect (image,
				  "size-prepared",
				  G_CALLBACK (eog_window_obtain_desired_size),
				  window);
	}

	priv->load_job = eog_job_load_new (image);

	g_signal_connect (priv->load_job,
			  "finished",
			  G_CALLBACK (eog_job_load_cb),
			  window);

	g_signal_connect (priv->load_job,
			  "progress",
			  G_CALLBACK (eog_job_progress_cb),
			  window);

	eog_job_queue_add_job (priv->load_job);

	str_image = eog_image_get_uri_for_display (image);

	status_message = g_strdup_printf (_("Loading image \"%s\""), 
				          str_image);

	g_free (str_image);
	
	gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar),
			    priv->image_info_message_cid, status_message);

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
eog_window_open_recent_cb (GtkAction *action, EogWindow *window)
{
	GtkRecentInfo *info;
	const gchar *uri;
	GSList *list = NULL;
	
	info = g_object_get_data (G_OBJECT (action), "gtk-recent-info");
	g_return_if_fail (info != NULL);

	uri = gtk_recent_info_get_uri (info);
	list = g_slist_prepend (list, g_strdup (uri));

	eog_application_open_uri_list (EOG_APP, 
				       list, 
				       GDK_CURRENT_TIME, 
				       0,
				       NULL);

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);
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
					       0,
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

	action = gtk_action_group_get_action (window->priv->actions_image, 
					      "ViewFullscreen");

	g_signal_handlers_block_by_func
		(action, G_CALLBACK (eog_window_cmd_fullscreen), window);
	
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      window->priv->mode == EOG_WINDOW_MODE_FULLSCREEN);

	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (eog_window_cmd_fullscreen), window);
}

static void
eog_window_update_slideshow_action (EogWindow *window)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->actions_collection, 
					      "ViewSlideshow");

	g_signal_handlers_block_by_func
		(action, G_CALLBACK (eog_window_cmd_slideshow), window);
	
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      window->priv->mode == EOG_WINDOW_MODE_SLIDESHOW);

	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (eog_window_cmd_slideshow), window);
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

static gboolean
fullscreen_timeout_cb (gpointer data)
{
	EogWindow *window = EOG_WINDOW (data);

	gtk_widget_hide_all (window->priv->fullscreen_popup);

	eog_scroll_view_hide_cursor (EOG_SCROLL_VIEW (window->priv->view));

	fullscreen_clear_timeout (window);

	return FALSE;
}

static gboolean
slideshow_is_loop_end (EogWindow *window)
{
	EogWindowPrivate *priv = window->priv;
	EogImage *image = NULL;
	gint pos;
	
	image = eog_thumb_view_get_first_selected_image (EOG_THUMB_VIEW (priv->thumbview));

	pos = eog_list_store_get_pos_by_image (priv->store, image);

	return (pos == (eog_list_store_length (priv->store) - 1));
}

static gboolean
slideshow_switch_cb (gpointer data)
{
	EogWindow *window = EOG_WINDOW (data);
	EogWindowPrivate *priv = window->priv;

	eog_debug (DEBUG_WINDOW);
	
	if (!priv->slideshow_loop && slideshow_is_loop_end (window)) {
		eog_window_stop_fullscreen (window, TRUE);
		return FALSE;
	}

	eog_thumb_view_select_single (EOG_THUMB_VIEW (priv->thumbview), 
				      EOG_THUMB_VIEW_SELECT_RIGHT);

	return TRUE;
}

static void
fullscreen_clear_timeout (EogWindow *window)
{
	eog_debug (DEBUG_WINDOW);
	
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

	eog_debug (DEBUG_WINDOW);
	
	fullscreen_clear_timeout (window);

	source = g_timeout_source_new (EOG_WINDOW_FULLSCREEN_TIMEOUT);
	g_source_set_callback (source, fullscreen_timeout_cb, window, NULL);
	
	g_source_attach (source, NULL);

	window->priv->fullscreen_timeout_source = source;

	eog_scroll_view_show_cursor (EOG_SCROLL_VIEW (window->priv->view));
}

static void
slideshow_clear_timeout (EogWindow *window)
{
	eog_debug (DEBUG_WINDOW);
	
	if (window->priv->slideshow_switch_source != NULL) {
		g_source_unref (window->priv->slideshow_switch_source);
		g_source_destroy (window->priv->slideshow_switch_source);
	}

	window->priv->slideshow_switch_source = NULL;
}

static void
slideshow_set_timeout (EogWindow *window)
{
	GSource *source;

	eog_debug (DEBUG_WINDOW);
	
	slideshow_clear_timeout (window);

	source = g_timeout_source_new (window->priv->slideshow_switch_timeout * 1000);
	g_source_set_callback (source, slideshow_switch_cb, window, NULL);

	g_source_attach (source, NULL);

	window->priv->slideshow_switch_source = source;
}

static void
show_fullscreen_popup (EogWindow *window)
{
	eog_debug (DEBUG_WINDOW);
	
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

	eog_debug (DEBUG_WINDOW);
	
	show_fullscreen_popup (window);

	return FALSE;
}

static gboolean
fullscreen_leave_notify_cb (GtkWidget *widget,
			    GdkEventCrossing *event,
			    gpointer user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);

	eog_debug (DEBUG_WINDOW);
	
	fullscreen_clear_timeout (window);

	return FALSE;
}

static void
exit_fullscreen_button_clicked_cb (GtkWidget *button, EogWindow *window)
{
	GtkAction *action;

	eog_debug (DEBUG_WINDOW);
	
	if (window->priv->mode == EOG_WINDOW_MODE_SLIDESHOW) {
		action = gtk_action_group_get_action (window->priv->actions_collection, 
						      "ViewSlideshow");
	} else {
		action = gtk_action_group_get_action (window->priv->actions_image, 
						      "ViewFullscreen");
	}
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

	eog_debug (DEBUG_WINDOW);
	
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

	g_signal_connect (popup,
			  "enter-notify-event",
			  G_CALLBACK (fullscreen_leave_notify_cb),
			  window);

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

	eog_debug (DEBUG_WINDOW);
	
	priv = window->priv;

	fullscreen_mode = priv->mode == EOG_WINDOW_MODE_FULLSCREEN ||
			  priv->mode == EOG_WINDOW_MODE_SLIDESHOW;

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

	if (priv->status != EOG_WINDOW_STATUS_INIT) {
		visible = gconf_client_get_bool (priv->client, EOG_CONF_UI_IMAGE_COLLECTION, NULL);
		visible = visible && !fullscreen_mode;
		action = gtk_ui_manager_get_action (priv->ui_mgr, "/MainMenu/View/ImageCollectionToggle");
		g_assert (action != NULL);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);
		if (visible) {
			gtk_widget_show (priv->nav);
		} else {
			gtk_widget_hide (priv->nav);
		}
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
eog_window_run_fullscreen (EogWindow *window, gboolean slideshow)
{
	EogWindowPrivate *priv;
	GtkWidget *menubar;
	gboolean upscale;

	eog_debug (DEBUG_WINDOW);
	
	priv = window->priv;

	if (slideshow) {
		priv->mode = EOG_WINDOW_MODE_SLIDESHOW;
	} else {
		priv->mode = EOG_WINDOW_MODE_FULLSCREEN;
	}

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

	if (slideshow) {
		window->priv->slideshow_loop = 
				gconf_client_get_bool (window->priv->client, 
						       EOG_CONF_FULLSCREEN_LOOP, 
						       NULL);

		window->priv->slideshow_switch_timeout = 
				gconf_client_get_int (window->priv->client, 
						      EOG_CONF_FULLSCREEN_SECONDS, 
						      NULL);

		slideshow_set_timeout (window);
	}

	upscale = gconf_client_get_bool (window->priv->client, 
					 EOG_CONF_FULLSCREEN_UPSCALE, 
					 NULL);

	eog_scroll_view_set_zoom_upscale (EOG_SCROLL_VIEW (priv->view), 
					  upscale);

	gtk_widget_grab_focus (window->priv->view);

	gtk_window_fullscreen (GTK_WINDOW (window));
	eog_window_update_fullscreen_popup (window);

	if (slideshow) {
		eog_window_update_slideshow_action (window);
	} else {
		eog_window_update_fullscreen_action (window);
	}
}

static void
eog_window_stop_fullscreen (EogWindow *window, gboolean slideshow)
{
	EogWindowPrivate *priv;
	GtkWidget *menubar;
	
	eog_debug (DEBUG_WINDOW);

	priv = window->priv;

	if (priv->mode != EOG_WINDOW_MODE_SLIDESHOW &&
	    priv->mode != EOG_WINDOW_MODE_FULLSCREEN) return;

	priv->mode = EOG_WINDOW_MODE_NORMAL;

	fullscreen_clear_timeout (window);

	if (slideshow) {
		slideshow_clear_timeout (window);
	}
	
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

	gtk_window_unfullscreen (GTK_WINDOW (window));

	if (slideshow) {
		eog_window_update_slideshow_action (window);
	} else {
		eog_window_update_fullscreen_action (window);
	}

	eog_scroll_view_show_cursor (EOG_SCROLL_VIEW (priv->view));
}

static void
eog_window_page_setup (EogWindow *window)
{
	GtkPageSetup *new_page_setup;

	eog_debug (DEBUG_PRINTING);
	
	if (window->priv->print_settings == NULL) {
		window->priv->print_settings = gtk_print_settings_new ();
	}
	
	new_page_setup = gtk_print_run_page_setup_dialog (GTK_WINDOW (window),
							  window->priv->print_page_setup, 
							  window->priv->print_settings);
	if (window->priv->print_page_setup) {
		g_object_unref (window->priv->print_page_setup);
	}
	
	window->priv->print_page_setup = new_page_setup;
}

static void
eog_window_print_draw_page (GtkPrintOperation *operation,
			    GtkPrintContext   *context,
			    gint               page_nr,
			    gpointer           user_data) 
{
	cairo_t *cr;
	gdouble dpi_x, dpi_y;
	gdouble x0, y0;
	gdouble scale_factor;
	gdouble p_width, p_height;
	gint width, height;
	GdkPixbuf *pixbuf;
	EogPrintData *data;
	GtkPageSetup *page_setup;
	
	eog_debug (DEBUG_PRINTING);
	
	data = (EogPrintData *) user_data;

	scale_factor = data->scale_factor/100;
	pixbuf = eog_image_get_pixbuf (data->image);

	dpi_x = gtk_print_context_get_dpi_x (context);
	dpi_y = gtk_print_context_get_dpi_y (context);
	
	switch (data->unit) {
	case GTK_UNIT_INCH:
		x0 = data->left_margin * dpi_x;
		y0 = data->top_margin  * dpi_y;
		break;
	case GTK_UNIT_MM:
		x0 = data->left_margin * dpi_x/25.4;
		y0 = data->top_margin  * dpi_y/25.4;
		break;
	default:
		g_assert_not_reached ();
	}

	cr = gtk_print_context_get_cairo_context (context);

	cairo_translate (cr, x0, y0);

	page_setup = gtk_print_context_get_page_setup (context);
	p_width =  gtk_page_setup_get_page_width (page_setup, GTK_UNIT_POINTS);
	p_height = gtk_page_setup_get_page_height (page_setup, GTK_UNIT_POINTS);

	width  = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	/* this is both a workaround for a bug in cairo's PDF backend, and
	   a way to ensure we are not printing outside the page margins */
	cairo_rectangle (cr, 0, 0, MIN (width*scale_factor, p_width), MIN (height*scale_factor, p_height));
	cairo_clip (cr);

	cairo_scale (cr, scale_factor, scale_factor);
	gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
 	cairo_paint (cr);

	g_object_unref (pixbuf);
}

static GObject *
eog_window_print_create_custom_widget (GtkPrintOperation *operation, 
				       gpointer user_data)
{
	GtkPageSetup *page_setup;
	EogPrintData *data;
	
	eog_debug (DEBUG_PRINTING);
	
	data = (EogPrintData *)user_data;
	
	page_setup = gtk_print_operation_get_default_page_setup (operation);
	
	g_assert (page_setup != NULL);
	
	return G_OBJECT (eog_print_image_setup_new (data->image, page_setup));
}

static void
eog_window_print_custom_widget_apply (GtkPrintOperation *operation,
				      GtkWidget         *widget,
				      gpointer           user_data)
{
	EogPrintData *data;
	gdouble left_margin, top_margin, scale_factor;
	GtkUnit unit;
	
	eog_debug (DEBUG_PRINTING);
	
	data = (EogPrintData *)user_data;
	
	eog_print_image_setup_get_options (EOG_PRINT_IMAGE_SETUP (widget), 
					   &left_margin, &top_margin, 
					   &scale_factor, &unit);
	
	data->left_margin = left_margin;
	data->top_margin = top_margin;
	data->scale_factor = scale_factor;
	data->unit = unit;
}

static void
eog_window_print_end_print (GtkPrintOperation *operation,
			    GtkPrintContext   *context,
			    gpointer           user_data)
{
	EogPrintData *data = (EogPrintData*) user_data;

	eog_debug (DEBUG_PRINTING);
	
	g_object_unref (data->image);
	g_free (data);
}
static void
eog_window_print (EogWindow *window)
{
	GtkWidget *dialog;
	GError *error = NULL;
	GtkPrintOperation *print;
	GtkPrintOperationResult res;
	EogPrintData *data;
	
	eog_debug (DEBUG_PRINTING);
	
	if (!window->priv->print_settings)
		window->priv->print_settings = gtk_print_settings_new ();
	if (!window->priv->print_page_setup)
		window->priv->print_page_setup = gtk_page_setup_new ();
	
	print = gtk_print_operation_new ();

	data = g_new0 (EogPrintData, 1);

	data->left_margin = 0;
	data->top_margin = 0;
	data->scale_factor = 100;
	data->image = g_object_ref (window->priv->image);
	data->unit = GTK_UNIT_INCH;

	gtk_print_operation_set_print_settings (print, window->priv->print_settings);
	gtk_print_operation_set_default_page_setup (print, 
						    window->priv->print_page_setup);
	gtk_print_operation_set_n_pages (print, 1);
	gtk_print_operation_set_job_name (print,
					  eog_image_get_caption (window->priv->image));

	g_signal_connect (print, "draw_page", 
			  G_CALLBACK (eog_window_print_draw_page), 
			  data);
	g_signal_connect (print, "create-custom-widget", 
			  G_CALLBACK (eog_window_print_create_custom_widget),
			  data);
	g_signal_connect (print, "custom-widget-apply", 
			  G_CALLBACK (eog_window_print_custom_widget_apply), 
			  data);
	g_signal_connect (print, "end-print", 
			  G_CALLBACK (eog_window_print_end_print),
			  data);

	gtk_print_operation_set_custom_tab_label (print, _("Image Settings"));

	/* Make sure the window stays valid while printing */
	g_object_ref (window);

	res = gtk_print_operation_run (print,
				       GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				       GTK_WINDOW (window), &error);

	if (res == GTK_PRINT_OPERATION_RESULT_ERROR) {
		dialog = gtk_message_dialog_new (GTK_WINDOW (window),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("Error printing file:\n%s"),
						 error->message);
		g_signal_connect (dialog, "response", 
				  G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_widget_show (dialog);
		g_error_free (error);
	} else if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		if (window->priv->print_settings != NULL)
			g_object_unref (window->priv->print_settings);
		window->priv->print_settings = g_object_ref (gtk_print_operation_get_print_settings (print));
	}

	g_object_unref (window);
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

	if (current != NULL) {
		gchar *dirname, *filename;

		filename = eog_image_get_uri_for_display (current);
		dirname = g_path_get_dirname (filename);
		
	        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg), 
						     dirname);
		g_free (filename);
		g_free (dirname);
		g_object_unref (current);
	}

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
	GObject *pref_dlg;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);

	pref_dlg = eog_preferences_dialog_get_instance (GTK_WINDOW (window), 
							window->priv->client);

	eog_dialog_show (EOG_DIALOG (pref_dlg));
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
		"Claudio Saavedra <csaavedra@alumnos.utalca.cl>",
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
			       "website", "http://www.gnome.org/projects/eog",
			       "logo-icon-name", "eog",
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
			gtk_widget_show (priv->nav);
		} else {
			gtk_widget_hide (priv->nav);
		}
		gconf_client_set_bool (priv->client, EOG_CONF_UI_IMAGE_COLLECTION, visible, NULL);
	}
}

static void
eog_job_save_cb (EogJobSave *job, gpointer user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);

	g_signal_handlers_disconnect_by_func (job, 
					      eog_job_save_cb, 
					      window);

	g_signal_handlers_disconnect_by_func (job, 
					      eog_job_save_progress_cb, 
					      window);

	g_object_unref (window->priv->save_job);
	window->priv->save_job = NULL;

	update_status_bar (window);
}

static void
eog_window_cmd_save (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;
	EogWindow *window;
	GList *images;

	window = EOG_WINDOW (user_data);
	priv = window->priv;

	if (window->priv->save_job != NULL)
		return;

	images = eog_thumb_view_get_selected_images (EOG_THUMB_VIEW (priv->thumbview));

	priv->save_job = eog_job_save_new (images);

	g_signal_connect (priv->save_job, 
			  "finished",
			  G_CALLBACK (eog_job_save_cb), 
			  window);

	g_signal_connect (priv->save_job, 
			  "progress",
			  G_CALLBACK (eog_job_save_progress_cb), 
			  window);

	eog_job_queue_add_job (priv->save_job);
}

static GnomeVFSURI*
eog_window_retrieve_save_as_uri (EogWindow *window, EogImage *image)
{
	GtkWidget *dialog;
	GnomeVFSURI *save_uri = NULL;
	GnomeVFSURI *parent_uri;
	GnomeVFSURI *image_uri;
	gchar *folder_uri;
	gint response;

	g_assert (image != NULL);

	image_uri = eog_image_get_uri (image);
	parent_uri = gnome_vfs_uri_get_parent (image_uri);
	folder_uri = gnome_vfs_uri_to_string (parent_uri, GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (parent_uri);
	gnome_vfs_uri_unref (image_uri);

	dialog = eog_file_chooser_new (GTK_FILE_CHOOSER_ACTION_SAVE);
	gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dialog),
						 folder_uri);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_hide (dialog);

	if (response == GTK_RESPONSE_OK) {
		gchar *new_uri;

		new_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

		g_assert (new_uri != NULL);

		save_uri = gnome_vfs_uri_new (new_uri);
		g_free (new_uri);
	}
	gtk_widget_destroy (dialog);
	if (folder_uri)
		g_free (folder_uri);
	
	return save_uri;
}

static void
eog_window_cmd_save_as (GtkAction *action, gpointer user_data)
{
        EogWindowPrivate *priv;
        EogWindow *window;
	GList *images;

        window = EOG_WINDOW (user_data);
	priv = window->priv;

	if (window->priv->save_job != NULL)
		return;

	images = eog_thumb_view_get_selected_images (EOG_THUMB_VIEW (priv->thumbview));

	if (g_list_length (images) == 1) {
		GnomeVFSURI *uri;
		
		uri = eog_window_retrieve_save_as_uri (window, images->data);

		if (!uri) {
			g_list_free (images);
			return;
		}

		priv->save_job = eog_job_save_as_new (images, NULL, uri); 

		gnome_vfs_uri_unref (uri);
	} else {
		GnomeVFSURI *baseuri;
		GtkWidget *dialog;
		gchar *basedir;
		EogURIConverter *converter;
		
		basedir = g_get_current_dir ();
		baseuri = gnome_vfs_uri_new (basedir);
		g_free (basedir);

		dialog = eog_save_as_dialog_new (GTK_WINDOW (window),
						 images, 
						 baseuri);

		gtk_widget_show_all (dialog);

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
			gnome_vfs_uri_unref (baseuri);

			gtk_widget_destroy (dialog);

			return;
		}

		converter = eog_save_as_dialog_get_converter (dialog);

		g_assert (converter != NULL);

		priv->save_job = eog_job_save_as_new (images, converter, NULL);

		gtk_widget_destroy (dialog);

		g_object_unref (converter);
		gnome_vfs_uri_unref (baseuri);
	}
	
	g_signal_connect (priv->save_job, 
			  "finished",
			  G_CALLBACK (eog_job_save_cb), 
			  window);

	g_signal_connect (priv->save_job, 
			  "progress",
			  G_CALLBACK (eog_job_save_progress_cb), 
			  window);

	eog_job_queue_add_job (priv->save_job);
}

static void
eog_window_cmd_page_setup (GtkAction *action, gpointer user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);

	eog_window_page_setup (window);
}

static void
eog_window_cmd_print (GtkAction *action, gpointer user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);

	eog_window_print (window);
}

static void
eog_window_cmd_properties (GtkAction *action, gpointer user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);
	EogWindowPrivate *priv;

	priv = window->priv;

	if (window->priv->properties_dlg == NULL) {
		window->priv->properties_dlg = 
			eog_properties_dialog_new (GTK_WINDOW (window),
						   EOG_THUMB_VIEW (priv->thumbview));

		eog_properties_dialog_update (EOG_PROPERTIES_DIALOG (priv->properties_dlg),
					      priv->image);
	}

	eog_dialog_show (EOG_DIALOG (window->priv->properties_dlg));
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
eog_window_cmd_wallpaper (GtkAction *action, gpointer user_data)
{
	EogWindow *window;
	EogWindowPrivate *priv;
	EogImage *image;
	guint32 user_time;
	char *filename = NULL;
	
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);
	priv = window->priv;

	image = eog_thumb_view_get_first_selected_image (EOG_THUMB_VIEW (priv->thumbview));

	g_return_if_fail (EOG_IS_IMAGE (image));

	user_time = gtk_get_current_event_time();

	filename = eog_image_get_uri_for_display (image);

	gconf_client_set_string (priv->client, 
				 EOG_CONF_DESKTOP_WALLPAPER, 
				 filename, 
				 NULL);

	eog_util_launch_desktop_file ("background.desktop", user_time);
}

static int
show_move_to_trash_confirm_dialog (EogWindow *window, GList *images)
{
	GtkWidget *dlg;
	char *prompt;
	int response;
	int n_images;
	EogImage *image;

	n_images = g_list_length (images);
	
	if (n_images == 1) {
		image = EOG_IMAGE (images->data);
		prompt = g_strdup_printf (_("Are you sure you want to move\n\"%s\" to the trash?"), 
                                          eog_image_get_caption (image));		
	} else {
		prompt = g_strdup_printf (ngettext("Are you sure you want to move\n" 
					           "the selected image to the trash?",
						   "Are you sure you want to move\n"
						   "the %d selected images to the trash?", n_images), n_images);
	}

	dlg = gtk_message_dialog_new_with_markup (GTK_WINDOW (window),
						  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						  GTK_MESSAGE_QUESTION,
						  GTK_BUTTONS_NONE,
						  "<span weight=\"bold\" size=\"larger\">%s</span>", 
						  prompt);
	g_free (prompt);

	gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dlg), _("Move to Trash"), GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);
	gtk_window_set_title (GTK_WINDOW (dlg), "");
	gtk_widget_show_all (dlg);

	response = gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);

	return response;
}

static gboolean
move_to_trash_real (EogImage *image, GError **error)
{
	GnomeVFSURI *uri;
	GnomeVFSURI *trash_dir;
	GnomeVFSURI *trash_uri;
	gint result;
	char *name;

	g_return_val_if_fail (EOG_IS_IMAGE (image), FALSE);

	uri = eog_image_get_uri (image);

        result = gnome_vfs_find_directory (uri,
					   GNOME_VFS_DIRECTORY_KIND_TRASH,
					   &trash_dir, 
					   FALSE, 
					   FALSE, 
					   0777);

	if (result != GNOME_VFS_OK) {
		gnome_vfs_uri_unref (uri);

		g_set_error (error, 
			     EOG_WINDOW_ERROR, 
			     EOG_WINDOW_ERROR_TRASH_NOT_FOUND,
			     _("Couldn't access trash."));

		return FALSE;
	}

	name = gnome_vfs_uri_extract_short_name (uri);
	trash_uri = gnome_vfs_uri_append_file_name (trash_dir, name);
	g_free (name);
	
	result = gnome_vfs_move_uri (uri, trash_uri, TRUE);

	gnome_vfs_uri_unref (uri);
	gnome_vfs_uri_unref (trash_uri);
	gnome_vfs_uri_unref (trash_dir);
	
	if (result != GNOME_VFS_OK) {
		g_set_error (error, 
			     EOG_WINDOW_ERROR,
			     EOG_WINDOW_ERROR_UNKNOWN,
			     gnome_vfs_result_to_string (result));
	}

	return (result == GNOME_VFS_OK);
}

static void
eog_window_cmd_move_to_trash (GtkAction *action, gpointer user_data)
{
	GList *images;
	GList *it;
	EogWindowPrivate *priv;
	EogListStore *list;
	int pos;
	EogImage *img;
	EogWindow *window;
	int response;
	int n_images;
	gboolean success;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);
	priv = window->priv;
	list = priv->store;
	
	n_images = eog_thumb_view_get_n_selected (EOG_THUMB_VIEW (priv->thumbview));

	if (n_images < 1) return;

	/* save position of selected image after the deletion */
	images = eog_thumb_view_get_selected_images (EOG_THUMB_VIEW (priv->thumbview));

	g_assert (images != NULL);

	/* HACK: eog_list_store_get_n_selected return list in reverse order */
	images = g_list_reverse (images);

	if (g_ascii_strcasecmp (gtk_action_get_name (action), "Delete") == 0) {
		response = show_move_to_trash_confirm_dialog (window, images);

		if (response != GTK_RESPONSE_OK) return;
	}
	
	pos = eog_list_store_get_pos_by_image (list, EOG_IMAGE (images->data));

	/* FIXME: make a nice progress dialog */
	/* Do the work actually. First try to delete the image from the disk. If this
	 * is successfull, remove it from the screen. Otherwise show error dialog.
	 */
	for (it = images; it != NULL; it = it->next) {
		GError *error = NULL;
		EogImage *image;

		image = EOG_IMAGE (it->data);

		success = move_to_trash_real (image, &error);

		if (success) {
			eog_list_store_remove_image (list, image);
		} else {
			char *header;
			GtkWidget *dlg;
			
			header = g_strdup_printf (_("Error on deleting image %s"), 
						  eog_image_get_caption (image));

			dlg = gtk_message_dialog_new (GTK_WINDOW (window),
						      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						      GTK_MESSAGE_ERROR,
						      GTK_BUTTONS_OK,
						      header);

			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dlg),
								  error->message);

			gtk_dialog_run (GTK_DIALOG (dlg));

			gtk_widget_destroy (dlg);

			g_free (header);
		}
	}

	/* free list */
	g_list_foreach (images, (GFunc) g_object_unref, NULL);
	g_list_free (images);

	/* select image at previously saved position */
	pos = MIN (pos, eog_list_store_length (list) - 1);

	if (pos >= 0) {
		img = eog_list_store_get_image_by_pos (list, pos);

		eog_thumb_view_set_current_image (EOG_THUMB_VIEW (priv->thumbview), 
						  img, 
						  TRUE);

		if (img != NULL) {
			g_object_unref (img);
		}
	}
}

static void
eog_window_cmd_fullscreen (GtkAction *action, gpointer user_data)
{
	EogWindow *window;
	gboolean fullscreen;
	
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	window = EOG_WINDOW (user_data);

	fullscreen = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	if (fullscreen) {
		eog_window_run_fullscreen (window, FALSE);
	} else {
		eog_window_stop_fullscreen (window, FALSE);
	}
}

static void
eog_window_cmd_slideshow (GtkAction *action, gpointer user_data)
{
	EogWindow *window;
	gboolean slideshow;
	
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	window = EOG_WINDOW (user_data);

	slideshow = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	if (slideshow) {
		eog_window_run_fullscreen (window, TRUE);
	} else {
		eog_window_stop_fullscreen (window, TRUE);
	}
}

static void
eog_window_cmd_zoom_in (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

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

	eog_debug (DEBUG_WINDOW);

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

	eog_debug (DEBUG_WINDOW);

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

	eog_debug (DEBUG_WINDOW);

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

	eog_debug (DEBUG_WINDOW);

	priv = EOG_WINDOW (user_data)->priv;
	
	eog_thumb_view_select_single (EOG_THUMB_VIEW (priv->thumbview), 
				      EOG_THUMB_VIEW_SELECT_LEFT);
}

static void
eog_window_cmd_go_next (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	priv = EOG_WINDOW (user_data)->priv;

	eog_thumb_view_select_single (EOG_THUMB_VIEW (priv->thumbview), 
				      EOG_THUMB_VIEW_SELECT_RIGHT);
}

static void
eog_window_cmd_go_first (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	priv = EOG_WINDOW (user_data)->priv;

	eog_thumb_view_select_single (EOG_THUMB_VIEW (priv->thumbview), 
				      EOG_THUMB_VIEW_SELECT_FIRST);
}

static void
eog_window_cmd_go_last (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

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
	{ "FileOpenWith", NULL, N_("Open _with"), NULL,
	  NULL,
	  NULL},
	{ "FileSaveAs", GTK_STOCK_SAVE_AS, N_("Save _As..."), "<control><shift>s", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_save_as) },
	{ "FilePageSetup", "stock_print-setup", N_("Page Set_up..."), NULL, 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_page_setup) },
	{ "FilePrint", GTK_STOCK_PRINT, N_("Print..."), "<control>p", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_print) },
	{ "FileProperties", GTK_STOCK_PROPERTIES, N_("Properties"), "<alt>Return", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_properties) },
	{ "EditUndo", GTK_STOCK_UNDO, N_("_Undo"), "<control>z", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_undo) },
	{ "EditFlipHorizontal", "object-flip-horizontal", N_("Flip _Horizontal"), NULL, 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_flip_horizontal) },
	{ "EditFlipVertical", "object-flip-vertical", N_("Flip _Vertical"), NULL, 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_flip_vertical) },
	{ "EditRotate90",  "object-rotate-right",  N_("_Rotate Clockwise"), "<control>r", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_rotate_90) },
	{ "EditRotate270", "object-rotate-left", N_("Rotate Counterc_lockwise"), NULL, 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_rotate_270) },
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

static const GtkToggleActionEntry toggle_entries_collection[] = {
	{ "ViewSlideshow", NULL, N_("_Slideshow"), "F5", 
	  NULL, 
	  G_CALLBACK (eog_window_cmd_slideshow), FALSE },
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

static gint
sort_recents_mru (GtkRecentInfo *a, GtkRecentInfo *b)
{
	return (gtk_recent_info_get_modified (a) < gtk_recent_info_get_modified (b));
}

static void
eog_window_update_recent_files_menu (EogWindow *window)
{
	EogWindowPrivate *priv;
	GList *actions = NULL, *li = NULL, *items = NULL;
	guint count_recent = 0;

	priv = window->priv;

	if (priv->recent_menu_id != 0)
		gtk_ui_manager_remove_ui (priv->ui_mgr, priv->recent_menu_id);
	
	actions = gtk_action_group_list_actions (priv->actions_recent);

	for (li = actions; li != NULL; li = li->next) {
		g_signal_handlers_disconnect_by_func (GTK_ACTION (li->data),
						      G_CALLBACK(eog_window_open_recent_cb),
						      window);
		
		gtk_action_group_remove_action (priv->actions_recent,
						GTK_ACTION (li->data));
	}

	g_list_free (actions);

	priv->recent_menu_id = gtk_ui_manager_new_merge_id (priv->ui_mgr);
	items = gtk_recent_manager_get_items (priv->recent_manager);
	items = g_list_sort (items, (GCompareFunc) sort_recents_mru);

	for (li = items; li != NULL && count_recent < EOG_RECENT_FILES_LIMIT; li = li->next) {
		gchar *action_name;
		gchar *label;
		gchar *tip;
		gchar **display_name;
		gchar *label_filename;
		GtkAction *action;
		GtkRecentInfo *info = li->data;

		if (!gtk_recent_info_has_group (info, EOG_RECENT_FILES_GROUP))
			continue;

		count_recent++;

		action_name = g_strdup_printf ("recent-info-%d", count_recent);
		display_name = g_strsplit (gtk_recent_info_get_display_name (info), "_", -1);
		label_filename = g_strjoinv ("__", display_name);
		label = g_strdup_printf ("_%d. %s", count_recent, label_filename);
		g_free (label_filename);
		g_strfreev (display_name);

		tip = gtk_recent_info_get_uri_display (info);

		/* This is a workaround for a bug (#351945) regarding 
		 * gtk_recent_info_get_uri_display() and remote URIs. 
		 * gnome_vfs_format_uri_for_display is sufficient here
		 * since the password gets stripped when adding the 
		 * file to the recently used list. */
		if (tip == NULL)
			tip = gnome_vfs_format_uri_for_display (gtk_recent_info_get_uri (info));
		
		action = gtk_action_new (action_name, label, tip, NULL);
		
		g_object_set_data_full (G_OBJECT (action), "gtk-recent-info", 
					gtk_recent_info_ref (info),
					(GDestroyNotify) gtk_recent_info_unref);
		
		g_object_set (G_OBJECT (action), "icon-name", "gnome-mime-image", NULL);
		
		g_signal_connect (action, "activate",
				  G_CALLBACK (eog_window_open_recent_cb),
				  window);
		
		gtk_action_group_add_action (priv->actions_recent, action);
		
		g_object_unref (action);

		gtk_ui_manager_add_ui (priv->ui_mgr, priv->recent_menu_id,
				       "/MainMenu/File/RecentDocuments",
				       action_name, action_name,
				       GTK_UI_MANAGER_AUTO, FALSE);

		g_free (action_name);
		g_free (label);
		g_free (tip);
	}

	g_list_foreach (items, (GFunc) gtk_recent_info_unref, NULL);
	g_list_free (items);
}

static void
eog_window_recent_manager_changed_cb (GtkRecentManager *manager, EogWindow *window)
{
	eog_window_update_recent_files_menu (window);
}

static void 
eog_window_construct_ui (EogWindow *window)
{
	EogWindowPrivate *priv;

	GError *error = NULL;
	
	GtkWidget *menubar;
	GtkWidget *toolbar;
	GtkWidget *popup;
	GtkWidget *frame;

	GConfEntry *entry;

	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	priv->box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), priv->box);
	gtk_widget_show (priv->box);

	priv->ui_mgr = gtk_ui_manager_new ();

	priv->open_with_menu_id = gtk_ui_manager_new_merge_id (priv->ui_mgr);

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

	gtk_action_group_add_toggle_actions (priv->actions_collection, 
					     toggle_entries_collection, 
					     G_N_ELEMENTS (toggle_entries_collection), 
					     window);

	set_action_properties (priv->actions_image, priv->actions_collection);

	gtk_ui_manager_insert_action_group (priv->ui_mgr, priv->actions_collection, 0);

	if (!gtk_ui_manager_add_ui_from_file (priv->ui_mgr, 
					      EOG_DATADIR"/eog-ui.xml", 
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

	priv->actions_recent = gtk_action_group_new ("RecentFilesActions");
	gtk_action_group_set_translation_domain (priv->actions_recent, 
						 GETTEXT_PACKAGE);

	g_signal_connect (priv->recent_manager, "changed",
			  G_CALLBACK (eog_window_recent_manager_changed_cb),
			  window);

	eog_window_update_recent_files_menu (window);

	gtk_ui_manager_insert_action_group (priv->ui_mgr, priv->actions_recent, 0);

	priv->cbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start_defaults (GTK_BOX (priv->box), priv->cbox);
	gtk_widget_show (priv->cbox);

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

	priv->layout = gtk_vbox_new (FALSE, 2);

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

	gtk_box_pack_start_defaults (GTK_BOX (priv->layout), frame);

	priv->thumbview = eog_thumb_view_new ();

	/* giving shape to the view */
	gtk_icon_view_set_margin (GTK_ICON_VIEW (priv->thumbview), 0);
	gtk_icon_view_set_row_spacing (GTK_ICON_VIEW (priv->thumbview), 0);
	gtk_icon_view_set_item_width (GTK_ICON_VIEW (priv->thumbview), 110);

	g_signal_connect (G_OBJECT (priv->thumbview), "selection_changed",
			  G_CALLBACK (handle_image_selection_changed_cb), window);

	priv->nav = eog_thumb_nav_new (priv->thumbview,
				       EOG_THUMB_NAV_MODE_ONE_ROW,
				       gconf_client_get_bool (priv->client,
							      EOG_CONF_UI_SCROLL_BUTTONS,
							      NULL));

	popup = gtk_ui_manager_get_widget (priv->ui_mgr, "/ThumbnailPopup");
	eog_thumb_view_set_thumbnail_popup (EOG_THUMB_VIEW (priv->thumbview), 
					    GTK_MENU (popup));

	gtk_box_pack_start (GTK_BOX (priv->layout), priv->nav, FALSE, FALSE, 0);

	gtk_box_pack_end (GTK_BOX (priv->cbox), priv->layout, TRUE, TRUE, 0);

	//set_drag_dest (window);

	entry = gconf_client_get_entry (priv->client, 
					EOG_CONF_VIEW_INTERPOLATE, 
					NULL, TRUE, NULL);
	if (entry != NULL) {
		eog_window_interp_type_changed_cb (priv->client, 0, entry, window);
		gconf_entry_unref (entry);
		entry = NULL;
	}

	entry = gconf_client_get_entry (priv->client, 
					EOG_CONF_VIEW_SCROLL_WHEEL_ZOOM, 
					NULL, TRUE, NULL);
	if (entry != NULL) {
		eog_window_scroll_wheel_zoom_changed_cb (priv->client, 0, entry, window);
		gconf_entry_unref (entry);
		entry = NULL;
	}

	entry = gconf_client_get_entry (priv->client, 
					EOG_CONF_VIEW_ZOOM_MULTIPLIER, 
					NULL, TRUE, NULL);
	if (entry != NULL) {
		eog_window_zoom_multiplier_changed_cb (priv->client, 0, entry, window);
		gconf_entry_unref (entry);
		entry = NULL;
	}

	entry = gconf_client_get_entry (priv->client, 
					EOG_CONF_VIEW_TRANSPARENCY, 
					NULL, TRUE, NULL);
	if (entry != NULL) {
		eog_window_transparency_changed_cb (priv->client, 0, entry, window);
		gconf_entry_unref (entry);
		entry = NULL;
	}

	entry = gconf_client_get_entry (priv->client, 
					EOG_CONF_VIEW_TRANS_COLOR, 
					NULL, TRUE, NULL);
	if (entry != NULL) {
		eog_window_trans_color_changed_cb (priv->client, 0, entry, window);
		gconf_entry_unref (entry);
		entry = NULL;
	}

	entry = gconf_client_get_entry (priv->client, 
					EOG_CONF_UI_IMAGE_COLLECTION_POSITION, 
					NULL, TRUE, NULL);
	if (entry != NULL) {
		eog_window_collection_mode_changed_cb (priv->client, 0, entry, window);
		gconf_entry_unref (entry);
		entry = NULL;
	}

	if ((priv->flags & EOG_STARTUP_FULLSCREEN) || 
	    (priv->flags & EOG_STARTUP_SLIDE_SHOW)) {
		eog_window_run_fullscreen (window, (priv->flags & EOG_STARTUP_SLIDE_SHOW));
	} else {
		priv->mode = EOG_WINDOW_MODE_NORMAL;
		update_ui_visibility (window);
	}
}

static void
eog_window_init (EogWindow *window)
{
	GdkGeometry hints;
	GdkScreen *screen;

	eog_debug (DEBUG_WINDOW);

	hints.min_width  = EOG_WINDOW_MIN_WIDTH;
	hints.min_height = EOG_WINDOW_MIN_HEIGHT;

	screen = gtk_widget_get_screen (GTK_WIDGET (window));

	window->priv = EOG_WINDOW_GET_PRIVATE (window);

	window->priv->client = gconf_client_get_default ();

	gconf_client_add_dir (window->priv->client, EOG_CONF_DIR,
			      GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);

	gconf_client_notify_add (window->priv->client,
				 EOG_CONF_VIEW_INTERPOLATE,
				 eog_window_interp_type_changed_cb,
				 window, NULL, NULL);
	
	gconf_client_notify_add (window->priv->client,
				 EOG_CONF_VIEW_SCROLL_WHEEL_ZOOM,
				 eog_window_scroll_wheel_zoom_changed_cb,
				 window, NULL, NULL);
	
	gconf_client_notify_add (window->priv->client,
				 EOG_CONF_VIEW_ZOOM_MULTIPLIER,
				 eog_window_zoom_multiplier_changed_cb,
				 window, NULL, NULL);
	
	gconf_client_notify_add (window->priv->client,
				 EOG_CONF_VIEW_TRANSPARENCY,
				 eog_window_transparency_changed_cb,
				 window, NULL, NULL);

	gconf_client_notify_add (window->priv->client,
				 EOG_CONF_VIEW_TRANS_COLOR,
				 eog_window_trans_color_changed_cb,
				 window, NULL, NULL);

	gconf_client_notify_add (window->priv->client,
				 EOG_CONF_UI_SCROLL_BUTTONS,
				 eog_window_scroll_buttons_changed_cb,
				 window, NULL, NULL);

	gconf_client_notify_add (window->priv->client,
				 EOG_CONF_UI_IMAGE_COLLECTION_POSITION,
				 eog_window_collection_mode_changed_cb,
				 window, NULL, NULL);

	gconf_client_notify_add (window->priv->client,
				 EOG_CONF_UI_IMAGE_COLLECTION_RESIZABLE,
				 eog_window_collection_mode_changed_cb,
				 window, NULL, NULL);

	window->priv->store = NULL;
	window->priv->image = NULL;

	window->priv->fullscreen_popup = NULL;
	window->priv->fullscreen_timeout_source = NULL;
	window->priv->slideshow_loop = FALSE;
	window->priv->slideshow_switch_timeout = 0;
	window->priv->slideshow_switch_source = NULL;

	gtk_window_set_geometry_hints (GTK_WINDOW (window),
				       GTK_WIDGET (window),
				       &hints,
				       GDK_HINT_MIN_SIZE);

	gtk_window_set_default_size (GTK_WINDOW (window), 
				     EOG_WINDOW_DEFAULT_WIDTH,
				     EOG_WINDOW_DEFAULT_HEIGHT);

	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);

	window->priv->mode = EOG_WINDOW_MODE_UNKNOWN;
	window->priv->status = EOG_WINDOW_STATUS_UNKNOWN;

	window->priv->recent_manager = 
		gtk_recent_manager_get_for_screen (screen);

#ifdef HAVE_LCMS
	window->priv->display_profile = 
		eog_window_get_display_profile (screen);
#endif

	window->priv->recent_menu_id = 0;

	window->priv->print_page_setup = NULL;
	window->priv->print_settings = NULL;
	
	window->priv->collection_position = 0;
	window->priv->collection_resizable = FALSE;

	window->priv->mime_application_list = NULL;
}

static void
eog_window_dispose (GObject *object)
{
	EogWindow *window;
	EogWindowPrivate *priv;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_WINDOW (object));

	eog_debug (DEBUG_WINDOW);

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

	if (priv->actions_window != NULL) {
		g_object_unref (priv->actions_window);
		priv->actions_window = NULL;
	}

	if (priv->actions_image != NULL) {
		g_object_unref (priv->actions_image);
		priv->actions_image = NULL;
	}

	if (priv->actions_collection != NULL) {
		g_object_unref (priv->actions_collection);
		priv->actions_collection = NULL;
	}

	if (priv->actions_recent != NULL) {
		g_object_unref (priv->actions_recent);
		priv->actions_recent = NULL;
	}

	fullscreen_clear_timeout (window);

	if (window->priv->fullscreen_popup != NULL) {
		gtk_widget_destroy (priv->fullscreen_popup);
		priv->fullscreen_popup = NULL;
	}

	slideshow_clear_timeout (window);

	if (priv->recent_manager) {
		g_signal_handlers_disconnect_by_func (priv->recent_manager, 
						      G_CALLBACK (eog_window_recent_manager_changed_cb), 
						      window);
		priv->recent_manager = NULL;
	}
	
	priv->recent_menu_id = 0;
		
	eog_window_clear_load_job (window);

	eog_window_clear_transform_job (window);

	if (priv->client) {
		gconf_client_remove_dir (priv->client, EOG_CONF_DIR, NULL);
		g_object_unref (priv->client);
		priv->client = NULL;
	}
		
	if (priv->print_settings != NULL) {
		g_object_unref (priv->print_settings);
		priv->print_settings = NULL;
	}
	
	if (priv->print_page_setup != NULL) {
		g_object_unref (priv->print_page_setup);
		priv->print_page_setup = NULL;
	}

	if (priv->uri_list != NULL) {
		g_slist_foreach (priv->uri_list, (GFunc) gnome_vfs_uri_unref, NULL);	
		g_slist_free (priv->uri_list);
		priv->uri_list = NULL;
	}

	if (priv->mime_application_list != NULL) {
		gnome_vfs_mime_application_list_free (priv->mime_application_list);
		priv->mime_application_list = NULL;
	}

#ifdef HAVE_LCMS
	if (priv->display_profile != NULL) {
		cmsCloseProfile (priv->display_profile);
		priv->display_profile = NULL;
	}
#endif

	G_OBJECT_CLASS (eog_window_parent_class)->dispose (object);
}

static void
eog_window_finalize (GObject *object)
{
        GList *windows = eog_application_get_windows (EOG_APP);

	g_return_if_fail (EOG_IS_WINDOW (object));
	
	eog_debug (DEBUG_WINDOW);

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
	gint result = FALSE;

	switch (event->keyval) {
	case GDK_Escape:
		if (EOG_WINDOW (widget)->priv->mode == EOG_WINDOW_MODE_FULLSCREEN) {
			eog_window_stop_fullscreen (EOG_WINDOW (widget), FALSE);
		} else if (EOG_WINDOW (widget)->priv->mode == EOG_WINDOW_MODE_SLIDESHOW) {
			eog_window_stop_fullscreen (EOG_WINDOW (widget), TRUE);
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

		if (EOG_WINDOW (widget)->priv->mode == EOG_WINDOW_MODE_SLIDESHOW) {
			slideshow_set_timeout (EOG_WINDOW (widget));
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

		if (EOG_WINDOW (widget)->priv->mode == EOG_WINDOW_MODE_SLIDESHOW) {
			slideshow_set_timeout (EOG_WINDOW (widget));
		}

		break;
	}

	if (result == FALSE && GTK_WIDGET_CLASS (eog_window_parent_class)->key_press_event) {
		result = (* GTK_WIDGET_CLASS (eog_window_parent_class)->key_press_event) (widget, event);
	}

	return result;
}

static gint
eog_window_button_press (GtkWidget *widget, GdkEventButton *event)
{
	EogWindow *window = EOG_WINDOW (widget);
	gint result = FALSE;

	if (event->type == GDK_BUTTON_PRESS) {
		switch (event->button) {
		case 6:
			eog_thumb_view_select_single (EOG_THUMB_VIEW (window->priv->thumbview),
						      EOG_THUMB_VIEW_SELECT_LEFT);
			result = TRUE;
		       	break;
		case 7:
			eog_thumb_view_select_single (EOG_THUMB_VIEW (window->priv->thumbview),
						      EOG_THUMB_VIEW_SELECT_RIGHT);
			result = TRUE;
		       	break;
		}
	}

	if (result == FALSE && GTK_WIDGET_CLASS (eog_window_parent_class)->button_press_event) {
		result = (* GTK_WIDGET_CLASS (eog_window_parent_class)->button_press_event) (widget, event);
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

		gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (window->priv->statusbar),
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

	eog_debug (DEBUG_WINDOW);

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

	eog_debug (DEBUG_WINDOW);

	fullscreen = priv->mode == EOG_WINDOW_MODE_FULLSCREEN ||
		     priv->mode == EOG_WINDOW_MODE_SLIDESHOW;

	if (fullscreen) {
		gtk_widget_hide_all (priv->fullscreen_popup);
	}

	return GTK_WIDGET_CLASS (eog_window_parent_class)->focus_out_event (widget, event);
}

static void
eog_window_screen_changed (GtkWidget *widget, GdkScreen *prev_screen)
{
	EogWindowPrivate *priv;
	GdkScreen *new_screen;

	g_return_if_fail (EOG_IS_WINDOW (widget));

	eog_debug (DEBUG_WINDOW);

	priv = EOG_WINDOW (widget)->priv;

	new_screen = gtk_widget_get_screen (widget);

	if (prev_screen != NULL)
		g_signal_handlers_disconnect_by_func (gtk_recent_manager_get_for_screen (prev_screen), 
						      G_CALLBACK (eog_window_recent_manager_changed_cb), 
						      widget);
	
	priv->recent_manager = gtk_recent_manager_get_for_screen (new_screen);

	g_signal_connect (priv->recent_manager, "changed", 
			  G_CALLBACK (eog_window_recent_manager_changed_cb), 
			  widget);

	if (GTK_WIDGET_CLASS (eog_window_parent_class)->screen_changed)
		GTK_WIDGET_CLASS (eog_window_parent_class)->screen_changed (widget, prev_screen);
}

static void
eog_window_set_property (GObject      *object,
			 guint         property_id,
			 const GValue *value,
			 GParamSpec   *pspec)
{
	EogWindow *window;
	EogWindowPrivate *priv;

        g_return_if_fail (EOG_IS_WINDOW (object));

        window = EOG_WINDOW (object);
	priv = window->priv;

        switch (property_id) {
	case PROP_STARTUP_FLAGS:
		priv->flags = (gint8) g_value_get_uchar (value);
		break;

        default:
                g_assert_not_reached ();
        }
}

static void
eog_window_get_property (GObject    *object,
			 guint       property_id,
			 GValue     *value,
			 GParamSpec *pspec)
{
	EogWindow *window;
	EogWindowPrivate *priv;

        g_return_if_fail (EOG_IS_WINDOW (object));

        window = EOG_WINDOW (object);
	priv = window->priv;

        switch (property_id) {
	case PROP_STARTUP_FLAGS:
		g_value_set_uchar (value, priv->flags);

        default:
                g_assert_not_reached ();
	}
}

static GObject *
eog_window_constructor (GType type,
			guint n_construct_properties,
			GObjectConstructParam *construct_params)
{
	GObject *object;

	object = G_OBJECT_CLASS (eog_window_parent_class)->constructor
			(type, n_construct_properties, construct_params);

	eog_window_construct_ui (EOG_WINDOW (object));

	return object;
}

static void
eog_window_class_init (EogWindowClass *class)
{
	GObjectClass *g_object_class = (GObjectClass *) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) class;

	g_object_class->constructor = eog_window_constructor;
	g_object_class->dispose = eog_window_dispose;
	g_object_class->finalize = eog_window_finalize;
	g_object_class->set_property = eog_window_set_property;
	g_object_class->get_property = eog_window_get_property;

	widget_class->delete_event = eog_window_delete;
	widget_class->key_press_event = eog_window_key_press;
	widget_class->button_press_event = eog_window_button_press;
        widget_class->configure_event = eog_window_configure_event;
        widget_class->window_state_event = eog_window_window_state_event;
	widget_class->unrealize = eog_window_unrealize;
	widget_class->focus_in_event = eog_window_focus_in_event;
	widget_class->focus_out_event = eog_window_focus_out_event;
	//widget_class->drag_data_received = eog_window_drag_data_received;
	widget_class->screen_changed = eog_window_screen_changed;

	g_object_class_install_property (g_object_class,
					 PROP_STARTUP_FLAGS,
					 g_param_spec_uchar ("startup-flags", 
							     NULL, 
							     NULL,
							     0,
					 		     G_MAXUINT8,
					 		     0,
					 		     G_PARAM_READWRITE |
							     G_PARAM_CONSTRUCT_ONLY));

	signals [SIGNAL_PREPARED] = 
		g_signal_new ("prepared",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogWindowClass, prepared),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (g_object_class, sizeof (EogWindowPrivate));
}

GtkWidget*
eog_window_new (EogStartupFlags flags)
{
	EogWindow *window;

	eog_debug (DEBUG_WINDOW);

	window = EOG_WINDOW (g_object_new (EOG_TYPE_WINDOW, 
					   "type", GTK_WINDOW_TOPLEVEL,
					   "startup-flags", flags,
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
	gint n_images;

	eog_debug (DEBUG_WINDOW);

#ifdef HAVE_EXIF
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

	n_images = eog_list_store_length (EOG_LIST_STORE (priv->store));

#ifdef HAVE_EXIF 
	if (gconf_client_get_bool (priv->client, EOG_CONF_VIEW_AUTOROTATE, NULL)) {
		for (i = 0; i < n_images; i++) {
			eog_image_autorotate (
				eog_list_store_get_image_by_pos (priv->store, i));
		}
	}
#endif

	eog_thumb_view_set_model (EOG_THUMB_VIEW (priv->thumbview), priv->store);

	if (n_images == 0) {
		gint n_uris;

		priv->status = EOG_WINDOW_STATUS_NORMAL;
		update_action_groups_state (window);

		n_uris = g_slist_length (priv->uri_list);

		if (n_uris > 0) {
			GtkWidget *message_area;
			GnomeVFSURI *uri = NULL;

			if (n_uris == 1) {
				uri = (GnomeVFSURI *) priv->uri_list->data;
			}

			message_area = eog_no_images_error_message_area_new (uri);

			eog_window_set_message_area (window, message_area);

			gtk_widget_show (message_area);
		}

		g_signal_emit (window, signals[SIGNAL_PREPARED], 0);
	}
}

void
eog_window_open_uri_list (EogWindow *window, GSList *uri_list)
{
	EogJob *job;

	eog_debug (DEBUG_WINDOW);

	window->priv->status = EOG_WINDOW_STATUS_INIT;

	g_slist_foreach (uri_list, (GFunc) gnome_vfs_uri_ref, NULL);
	window->priv->uri_list = uri_list;

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

	eog_debug (DEBUG_WINDOW);

        g_return_val_if_fail (EOG_IS_WINDOW (window), FALSE);

        priv = window->priv;

        if (priv->store != NULL) {
                empty = (eog_list_store_length (EOG_LIST_STORE (priv->store)) == 0);
        }

        return empty;
}
