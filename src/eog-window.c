/* Eye Of Gnome - Main Window
 *
 * Copyright (C) 2000-2008 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on code by:
 * 	- Federico Mena-Quintero <federico@gnome.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
#include "eog-sidebar.h"
#include "eog-statusbar.h"
#include "eog-preferences-dialog.h"
#include "eog-remote-presenter.h"
#include "eog-print.h"
#include "eog-error-message-area.h"
#include "eog-application.h"
#include "eog-application-internal.h"
#include "eog-thumb-nav.h"
#include "eog-config-keys.h"
#include "eog-job-scheduler.h"
#include "eog-jobs.h"
#include "eog-util.h"
#include "eog-save-as-dialog-helper.h"
#include "eog-plugin-engine.h"
#include "eog-close-confirmation-dialog.h"
#include "eog-clipboard-handler.h"
#include "eog-window-activatable.h"
#include "eog-metadata-sidebar.h"
#include "eog-zoom-entry.h"

#include "eog-enum-types.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>
#ifndef __APPLE__
#include <gio/gdesktopappinfo.h>
#endif
#include <gtk/gtk.h>

#include <libpeas/peas-extension-set.h>
#include <libpeas/peas-activatable.h>

#if defined(HAVE_LCMS) && defined(GDK_WINDOWING_X11)
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <lcms2.h>
#endif

#define EOG_WINDOW_MIN_WIDTH  360
#define EOG_WINDOW_MIN_HEIGHT 350

#define EOG_WINDOW_DEFAULT_WIDTH  540
#define EOG_WINDOW_DEFAULT_HEIGHT 450

#define EOG_WINDOW_FULLSCREEN_TIMEOUT 2 * 1000
#define EOG_WINDOW_FULLSCREEN_POPUP_THRESHOLD 5

#define EOG_RECENT_FILES_GROUP  "Graphics"
#define EOG_RECENT_FILES_APP_NAME "Eye of GNOME"

#define EOG_WALLPAPER_FILENAME "eog-wallpaper"

#define is_rtl (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)

typedef enum {
	EOG_WINDOW_STATUS_UNKNOWN,
	EOG_WINDOW_STATUS_INIT,
	EOG_WINDOW_STATUS_NORMAL
} EogWindowStatus;

enum {
	PROP_0,
	PROP_GALLERY_POS,
	PROP_GALLERY_RESIZABLE,
	PROP_STARTUP_FLAGS
};

enum {
	SIGNAL_PREPARED,
	SIGNAL_LAST
};

static gint signals[SIGNAL_LAST];

struct _EogWindowPrivate {
	GSettings           *fullscreen_settings;
	GSettings           *ui_settings;
	GSettings           *view_settings;
	GSettings           *lockdown_settings;

        EogListStore        *store;
        EogImage            *image;
	EogWindowMode        mode;
	EogWindowStatus      status;

        GtkWidget           *headerbar;
        GtkWidget           *overlay;
        GtkWidget           *box;
        GtkWidget           *layout;
        GtkWidget           *cbox;
        GtkWidget           *scroll_view_container;
        GtkWidget           *view;
        GtkWidget           *sidebar;
        GtkWidget           *thumbview;
        GtkWidget           *statusbar;
        GtkWidget           *nav;
	GtkWidget           *message_area;
	GtkWidget           *remote_presenter;

	GtkBuilder          *gear_menu_builder;

	GtkWidget           *fullscreen_popup;
	GSource             *fullscreen_timeout_source;

	gboolean             slideshow_loop;
	gint                 slideshow_switch_timeout;
	GSource             *slideshow_switch_source;

	guint                fullscreen_idle_inhibit_cookie;

        EogJob              *load_job;
        EogJob              *transform_job;
	EogJob              *save_job;
	GFile               *last_save_as_folder;
	EogJob              *copy_job;

        guint                image_info_message_cid;
        guint                tip_message_cid;
	guint                copy_file_cid;

        EogStartupFlags      flags;
	GSList              *file_list;

	EogWindowGalleryPos  gallery_position;
	gboolean             gallery_resizable;

	gboolean	     save_disabled;
	gboolean             needs_reload_confirmation;

	GtkPageSetup        *page_setup;

	PeasExtensionSet    *extensions;

#ifdef HAVE_LCMS
        cmsHPROFILE          display_profile;
#endif
};

G_DEFINE_TYPE_WITH_PRIVATE (EogWindow, eog_window, HDY_TYPE_APPLICATION_WINDOW);

static void eog_window_action_toggle_fullscreen (GSimpleAction *action, GVariant *state, gpointer user_data);
static void eog_window_run_fullscreen (EogWindow *window, gboolean slideshow);
static void eog_window_action_save (GSimpleAction *action, GVariant *variant, gpointer user_data);
static void eog_window_action_save_as (GSimpleAction *action, GVariant *variant, gpointer user_data);
static void eog_window_action_toggle_slideshow (GSimpleAction *action, GVariant *state, gpointer user_data);
static void eog_window_action_pause_slideshow (GSimpleAction *action, GVariant *variant, gpointer user_data);
static void eog_window_stop_fullscreen (EogWindow *window, gboolean slideshow);
static void eog_job_load_cb (EogJobLoad *job, gpointer data);
static void eog_job_save_progress_cb (EogJobSave *job, float progress, gpointer data);
static void eog_job_progress_cb (EogJobLoad *job, float progress, gpointer data);
static void eog_job_transform_cb (EogJobTransform *job, gpointer data);
static void fullscreen_set_timeout (EogWindow *window);
static void fullscreen_clear_timeout (EogWindow *window);
static void slideshow_set_timeout (EogWindow *window);
static void update_action_groups_state (EogWindow *window);
static void eog_window_list_store_image_added (GtkTreeModel *tree_model,
					       GtkTreePath  *path,
					       GtkTreeIter  *iter,
					       gpointer      user_data);
static void eog_window_list_store_image_removed (GtkTreeModel *tree_model,
                 				 GtkTreePath  *path,
						 gpointer      user_data);
static void eog_window_set_wallpaper (EogWindow *window, const gchar *filename, const gchar *visible_filename);
static gboolean eog_window_save_images (EogWindow *window, GList *images);
static void eog_window_finish_saving (EogWindow *window);
static void eog_window_error_message_area_response (GtkInfoBar *message_area,
						    gint        response_id,
						    EogWindow  *window);

static GQuark
eog_window_error_quark (void)
{
	static GQuark q = 0;

	if (q == 0)
		q = g_quark_from_static_string ("eog-window-error-quark");

	return q;
}

static gboolean
_eog_zoom_shrink_to_boolean (GBinding *binding, const GValue *source,
			     GValue *target, gpointer user_data)
{
	EogZoomMode mode = g_value_get_enum (source);
	gboolean is_fit;

	is_fit = (mode == EOG_ZOOM_MODE_SHRINK_TO_FIT);
	g_value_set_variant (target, g_variant_new_boolean (is_fit));

	return TRUE;
}

static void
eog_window_set_gallery_mode (EogWindow           *window,
			     EogWindowGalleryPos  position,
			     gboolean             resizable)
{
	EogWindowPrivate *priv;
	GtkWidget *hpaned;
	EogThumbNavMode mode = EOG_THUMB_NAV_MODE_ONE_ROW;

	eog_debug (DEBUG_PREFERENCES);

	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	/* If layout is not set, ignore similar parameters
	 * to make sure a layout is set eventually */
	if (priv->layout
	    && priv->gallery_position == position
	    && priv->gallery_resizable == resizable)
		return;

	priv->gallery_position = position;
	priv->gallery_resizable = resizable;

	hpaned = gtk_widget_get_parent (priv->sidebar);

	g_object_ref (hpaned);
	g_object_ref (priv->nav);

	if (priv->layout)
	{
		gtk_container_remove (GTK_CONTAINER (priv->layout), hpaned);
		gtk_container_remove (GTK_CONTAINER (priv->layout), priv->nav);

		gtk_widget_destroy (priv->layout);
	}

	switch (position) {
	case EOG_WINDOW_GALLERY_POS_BOTTOM:
	case EOG_WINDOW_GALLERY_POS_TOP:
		if (resizable) {
			mode = EOG_THUMB_NAV_MODE_MULTIPLE_ROWS;

			priv->layout = gtk_paned_new (GTK_ORIENTATION_VERTICAL);

			if (position == EOG_WINDOW_GALLERY_POS_BOTTOM) {
				gtk_paned_pack1 (GTK_PANED (priv->layout), hpaned, TRUE, FALSE);
				gtk_paned_pack2 (GTK_PANED (priv->layout), priv->nav, FALSE, TRUE);
			} else {
				gtk_paned_pack1 (GTK_PANED (priv->layout), priv->nav, FALSE, TRUE);
				gtk_paned_pack2 (GTK_PANED (priv->layout), hpaned, TRUE, FALSE);
			}
		} else {
			mode = EOG_THUMB_NAV_MODE_ONE_ROW;

			priv->layout = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

			if (position == EOG_WINDOW_GALLERY_POS_BOTTOM) {
				gtk_box_pack_start (GTK_BOX (priv->layout), hpaned, TRUE, TRUE, 0);
				gtk_box_pack_start (GTK_BOX (priv->layout), priv->nav, FALSE, FALSE, 0);
			} else {
				gtk_box_pack_start (GTK_BOX (priv->layout), priv->nav, FALSE, FALSE, 0);
				gtk_box_pack_start (GTK_BOX (priv->layout), hpaned, TRUE, TRUE, 0);
			}
		}
		break;

	case EOG_WINDOW_GALLERY_POS_LEFT:
	case EOG_WINDOW_GALLERY_POS_RIGHT:
		if (resizable) {
			mode = EOG_THUMB_NAV_MODE_MULTIPLE_COLUMNS;

			priv->layout = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);

			if (position == EOG_WINDOW_GALLERY_POS_LEFT) {
				gtk_paned_pack1 (GTK_PANED (priv->layout), priv->nav, FALSE, TRUE);
				gtk_paned_pack2 (GTK_PANED (priv->layout), hpaned, TRUE, FALSE);
			} else {
				gtk_paned_pack1 (GTK_PANED (priv->layout), hpaned, TRUE, FALSE);
				gtk_paned_pack2 (GTK_PANED (priv->layout), priv->nav, FALSE, TRUE);
			}
		} else {
			mode = EOG_THUMB_NAV_MODE_ONE_COLUMN;

			priv->layout = gtk_box_new (GTK_ORIENTATION_HORIZONTAL,
						    2);

			if (position == EOG_WINDOW_GALLERY_POS_LEFT) {
				gtk_box_pack_start (GTK_BOX (priv->layout), priv->nav, FALSE, FALSE, 0);
				gtk_box_pack_start (GTK_BOX (priv->layout), hpaned, TRUE, TRUE, 0);
			} else {
				gtk_box_pack_start (GTK_BOX (priv->layout), hpaned, TRUE, TRUE, 0);
				gtk_box_pack_start (GTK_BOX (priv->layout), priv->nav, FALSE, FALSE, 0);
			}
		}

		break;
	}

	gtk_box_pack_end (GTK_BOX (priv->cbox), priv->layout, TRUE, TRUE, 0);
	gtk_widget_show (priv->layout);

	eog_thumb_nav_set_mode (EOG_THUMB_NAV (priv->nav), mode);

	if (priv->mode != EOG_WINDOW_MODE_UNKNOWN) {
		update_action_groups_state (window);
	}

	g_object_unref (priv->nav);
	g_object_unref (hpaned);
}

static void
eog_window_can_save_changed_cb (GSettings   *settings,
				const gchar *key,
				gpointer     user_data)
{
	EogWindowPrivate *priv;
	EogWindow *window;
	gboolean save_disabled = FALSE;
	GAction *action_save, *action_save_as;

	eog_debug (DEBUG_PREFERENCES);

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);
	priv = EOG_WINDOW (user_data)->priv;

	save_disabled = g_settings_get_boolean (settings, key);

	priv->save_disabled = save_disabled;

	action_save =
		g_action_map_lookup_action (G_ACTION_MAP (window),
									"save");
	action_save_as =
		g_action_map_lookup_action (G_ACTION_MAP (window),
									"save-as");

	if (priv->save_disabled) {
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action_save), FALSE);
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action_save_as), FALSE);
	} else {
		EogImage *image = eog_window_get_image (window);

		if (EOG_IS_IMAGE (image)) {
			g_simple_action_set_enabled (G_SIMPLE_ACTION (action_save),
						  eog_image_is_modified (image));

			g_simple_action_set_enabled (G_SIMPLE_ACTION (action_save_as), TRUE);
		}
	}
}

#if defined(HAVE_LCMS) && defined(GDK_WINDOWING_X11)
static cmsHPROFILE
eog_window_get_display_profile (GtkWidget *window)
{
	GdkScreen *screen;
	Display *dpy;
	Atom icc_atom, type;
	int format;
	gulong nitems;
	gulong bytes_after;
	gulong length;
	guchar *str;
	int result;
	cmsHPROFILE profile = NULL;
	char *atom_name;

	screen = gtk_widget_get_screen (window);

	if (GDK_IS_X11_SCREEN (screen)) {
		dpy = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));

		if (gdk_x11_screen_get_screen_number (screen) > 0)
			atom_name = g_strdup_printf ("_ICC_PROFILE_%d",
			                             gdk_x11_screen_get_screen_number (screen));
		else
			atom_name = g_strdup ("_ICC_PROFILE");

		icc_atom = gdk_x11_get_xatom_by_name_for_display (gdk_screen_get_display (screen), atom_name);

		g_free (atom_name);

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

		if ((result == Success) && (type == XA_CARDINAL) && (nitems > 0)) {
			switch (format)
			{
				case 8:
					length = nitems;
					break;
				case 16:
					length = sizeof(short) * nitems;
					break;
				case 32:
					length = sizeof(long) * nitems;
					break;
				default:
					eog_debug_message (DEBUG_LCMS,
					                   "Unable to read profile, not correcting");

					XFree (str);
					return NULL;
			}

			profile = cmsOpenProfileFromMem (str, length);

			if (G_UNLIKELY (profile == NULL)) {
				eog_debug_message (DEBUG_LCMS,
						   "Invalid display profile set, "
						   "not using it");
			}

			XFree (str);
		}
	} else {
		/* ICC profiles cannot be queried on Wayland yet */
		eog_debug_message (DEBUG_LCMS,
		                   "Not an X11 screen. "
		                   "Cannot fetch display profile.");
	}

	if (profile == NULL) {
		profile = cmsCreate_sRGBProfile ();
		eog_debug_message (DEBUG_LCMS,
		                   "No valid display profile set, assuming sRGB");
	}

	return profile;
}
#endif

static void
update_image_pos (EogWindow *window)
{
	EogWindowPrivate *priv;
	GAction* action;
	gint pos = -1, n_images = 0;

	priv = window->priv;

	n_images = eog_list_store_length (EOG_LIST_STORE (priv->store));

	/* priv->image may be NULL even if n_images > 0 */
	if (n_images > 0 && priv->image) {
		pos = eog_list_store_get_pos_by_image (EOG_LIST_STORE (priv->store),
						       priv->image);
	}
	/* Images: (image pos) / (n_total_images) */
	eog_statusbar_set_image_number (EOG_STATUSBAR (priv->statusbar),
					pos + 1,
					n_images);

	action = g_action_map_lookup_action (G_ACTION_MAP (window),
	                                     "current-image");

	g_return_if_fail (action != NULL);
	g_simple_action_set_state (G_SIMPLE_ACTION (action),
	                           g_variant_new ("(ii)", pos + 1, n_images));

}

static void
update_status_bar (EogWindow *window)
{
	EogWindowPrivate *priv;
	char *str = NULL;

	g_return_if_fail (EOG_IS_WINDOW (window));

	eog_debug (DEBUG_WINDOW);

	priv = window->priv;

	if (priv->image != NULL) {
		if (eog_image_has_data (priv->image, EOG_IMAGE_DATA_DIMENSION)) {
			int zoom, width, height;
			goffset bytes = 0;

			zoom = floor (100 * eog_scroll_view_get_zoom (EOG_SCROLL_VIEW (priv->view)) + 0.5);

			eog_image_get_size (priv->image, &width, &height);

			bytes = eog_image_get_bytes (priv->image);

			if ((width > 0) && (height > 0)) {
				gchar *size_string;

				size_string = g_format_size (bytes);

				/* Translators: This is the string displayed in the statusbar
				 * The tokens are from left to right:
				 * - image width
				 * - image height
				 * - image size in bytes
				 * - zoom in percent */
				str = g_strdup_printf (ngettext("%i × %i pixel  %s    %i%%",
								"%i × %i pixels  %s    %i%%", height),
							width,
							height,
							size_string,
							zoom);

				g_free (size_string);
			}
		}

		update_image_pos (window);
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
				   (void *) &window->priv->message_area);
}

static void
_eog_window_enable_action_group (GActionMap   *map,
				 const gchar **group,
				 gboolean      enable)
{
	GAction *action;
	const gchar **it = group;

	for (it = group; *it != NULL; it++) {
		action = g_action_map_lookup_action (map, *it);
		if (G_LIKELY (action))
			g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enable);
		else
			g_warning ("Action not found in action group: %s", *it);
	}
}

static void
_eog_window_enable_window_actions (EogWindow *window, gboolean enable)
{
	static const gchar *window_actions[] = {
		"open",
		"close",
		"close-all",
		"preferences",
		"manual",
		"about",
		"view-gallery",
		"view-sidebar",
		"view-statusbar",
		"view-fullscreen",
		NULL
	};

	_eog_window_enable_action_group (G_ACTION_MAP (window),
					 window_actions,
					 enable);
}

static void
_eog_window_enable_image_actions (EogWindow *window, gboolean enable)
{
	static const gchar *image_actions[] = {
		"save",
		"open-with",
		"save-as",
		"open-folder",
		"print",
		"properties",
		"undo",
		"flip-horizontal",
		"flip-vertical",
		"rotate-90",
		"rotate-270",
		"set-wallpaper",
		"show-remote",
		"move-trash",
		"delete",
		"copy",
		"zoom-in",
		"zoom-in-smooth",
		"zoom-out",
		"zoom-out-smooth",
		"zoom-normal",
		NULL
	};

	_eog_window_enable_action_group (G_ACTION_MAP (window),
					 image_actions,
					 enable);
}

static void
_eog_window_enable_gallery_actions (EogWindow *window, gboolean enable)
{

	static const gchar *gallery_actions[] = {
		"go-previous",
		"go-next",
		"go-first",
		"go-last",
		"go-random",
		NULL
	};

	_eog_window_enable_action_group (G_ACTION_MAP (window),
					 gallery_actions,
					 enable);
}

static void
update_action_groups_state (EogWindow *window)
{
	EogWindowPrivate *priv;
	GAction *action_gallery;
	GAction *action_sidebar;
	GAction *action_fscreen;
	GAction *action_sshow;
	GAction *action_print;
	gboolean print_disabled = FALSE;
	gboolean show_image_gallery = FALSE;
	gint n_images = 0;

	g_return_if_fail (EOG_IS_WINDOW (window));

	eog_debug (DEBUG_WINDOW);

	priv = window->priv;

	action_gallery =
		g_action_map_lookup_action (G_ACTION_MAP (window),
					     "view-gallery");

	action_sidebar =
		g_action_map_lookup_action (G_ACTION_MAP (window),
					     "view-sidebar");

	action_fscreen =
		g_action_map_lookup_action (G_ACTION_MAP (window),
					     "view-fullscreen");

	action_sshow =
		g_action_map_lookup_action (G_ACTION_MAP (window),
					     "view-slideshow");

	action_print =
		g_action_map_lookup_action (G_ACTION_MAP (window),
					     "print");

	g_assert (action_gallery != NULL);
	g_assert (action_sidebar != NULL);
	g_assert (action_fscreen != NULL);
	g_assert (action_sshow != NULL);
	g_assert (action_print != NULL);

	if (priv->store != NULL) {
		n_images = eog_list_store_length (EOG_LIST_STORE (priv->store));
	}

	if (priv->flags & EOG_STARTUP_DISABLE_GALLERY) {
		g_settings_set_boolean (priv->ui_settings,
					EOG_CONF_UI_IMAGE_GALLERY,
					FALSE);

		show_image_gallery = FALSE;
	} else {
		show_image_gallery =
			g_settings_get_boolean (priv->ui_settings,
					EOG_CONF_UI_IMAGE_GALLERY);
	}

	show_image_gallery &= n_images > 1 &&
			      priv->mode != EOG_WINDOW_MODE_SLIDESHOW;

	gtk_widget_set_visible (priv->nav, show_image_gallery);

	g_simple_action_set_state (G_SIMPLE_ACTION (action_gallery),
				   g_variant_new_boolean (show_image_gallery));

	if (show_image_gallery)
		gtk_widget_grab_focus (priv->thumbview);
	else
		gtk_widget_grab_focus (priv->view);

	if (n_images == 0) {
		_eog_window_enable_window_actions (window, TRUE);
		_eog_window_enable_image_actions (window, FALSE);
		_eog_window_enable_gallery_actions (window, FALSE);

		g_simple_action_set_enabled (G_SIMPLE_ACTION (action_fscreen), FALSE);
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action_sshow), FALSE);

		/* If there are no images on model, initialization
 		   stops here. */
		if (priv->status == EOG_WINDOW_STATUS_INIT) {
			priv->status = EOG_WINDOW_STATUS_NORMAL;
		}
	} else {
		_eog_window_enable_window_actions (window, TRUE);
		_eog_window_enable_image_actions (window, TRUE);

		g_simple_action_set_enabled (G_SIMPLE_ACTION (action_fscreen), TRUE);

		if (n_images == 1) {
			_eog_window_enable_gallery_actions (window, FALSE);
			g_simple_action_set_enabled (G_SIMPLE_ACTION (action_gallery), FALSE);
			g_simple_action_set_enabled (G_SIMPLE_ACTION (action_sshow), FALSE);
		} else {
			_eog_window_enable_gallery_actions (window, TRUE);
			g_simple_action_set_enabled (G_SIMPLE_ACTION (action_sshow), TRUE);
		}
	}

	print_disabled = g_settings_get_boolean (priv->lockdown_settings,
						EOG_CONF_DESKTOP_CAN_PRINT);

	if (print_disabled) {
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action_print), FALSE);
	}

	if (eog_sidebar_is_empty (EOG_SIDEBAR (priv->sidebar))) {
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action_sidebar), FALSE);
		gtk_widget_hide (priv->sidebar);
	}
}

static void
update_selection_ui_visibility (EogWindow *window)
{
	EogWindowPrivate *priv;
	GAction *wallpaper_action;
	gint n_selected;

	priv = window->priv;

	n_selected = eog_thumb_view_get_n_selected (EOG_THUMB_VIEW (priv->thumbview));

	wallpaper_action =
		g_action_map_lookup_action (G_ACTION_MAP (window),
					     "set-wallpaper");

	if (n_selected == 1) {
		g_simple_action_set_enabled (G_SIMPLE_ACTION (wallpaper_action), TRUE);
	} else {
		g_simple_action_set_enabled (G_SIMPLE_ACTION (wallpaper_action), FALSE);
	}
}

static gboolean
add_file_to_recent_files (GFile *file)
{
	gchar *text_uri;
	GFileInfo *file_info;
	GtkRecentData *recent_data;
	static gchar *groups[2] = { EOG_RECENT_FILES_GROUP , NULL };

	if (file == NULL) return FALSE;

	/* The password gets stripped here because ~/.recently-used.xbel is
	 * readable by everyone (chmod 644). It also makes the workaround
	 * for the bug with gtk_recent_info_get_uri_display() easier
	 * (see the comment in eog_window_update_recent_files_menu()). */
	text_uri = g_file_get_uri (file);

	if (text_uri == NULL)
		return FALSE;

	file_info = g_file_query_info (file,
	                               G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","
				       G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
	                               0, NULL, NULL);
	if (file_info == NULL)
		return FALSE;

	recent_data = g_slice_new (GtkRecentData);
	recent_data->display_name = NULL;
	recent_data->description = NULL;
	recent_data->mime_type = (gchar *) eog_util_get_content_type_with_fallback (file_info);
	recent_data->app_name = EOG_RECENT_FILES_APP_NAME;
	recent_data->app_exec = g_strjoin(" ", g_get_prgname (), "%u", NULL);
	recent_data->groups = groups;
	recent_data->is_private = FALSE;

	gtk_recent_manager_add_full (gtk_recent_manager_get_default (),
	                             text_uri,
	                             recent_data);

	g_free (recent_data->app_exec);
	g_free (text_uri);
	g_object_unref (file_info);

	g_slice_free (GtkRecentData, recent_data);

	return FALSE;
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

		if (window->priv->remote_presenter != NULL) {
			eog_remote_presenter_update (EOG_REMOTE_PRESENTER (priv->remote_presenter),
						      image);
		}

		g_object_unref (thumb);
	} else if (!gtk_widget_get_visible (window->priv->nav)) {
		gint img_pos = eog_list_store_get_pos_by_image (window->priv->store, image);
		GtkTreePath *path = gtk_tree_path_new_from_indices (img_pos,-1);
		GtkTreeIter iter;

		gtk_tree_model_get_iter (GTK_TREE_MODEL (window->priv->store), &iter, path);
		eog_list_store_thumbnail_set (window->priv->store, &iter);
		gtk_tree_path_free (path);
	}
}

static void
file_changed_info_bar_response (GtkInfoBar *info_bar,
				gint response,
				EogWindow *window)
{
	if (response == GTK_RESPONSE_YES) {
		eog_window_reload_image (window);
	}

	window->priv->needs_reload_confirmation = TRUE;

	eog_window_set_message_area (window, NULL);
}
static void
image_file_changed_cb (EogImage *img, EogWindow *window)
{
	GtkWidget *info_bar;
	gchar *text, *markup;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *hbox;

	if (window->priv->needs_reload_confirmation == FALSE)
		return;

	if (!eog_image_is_modified (img)) {
		/* Auto-reload when image is unmodified (bug #555370) */
		eog_window_reload_image (window);
		return;
	}

	window->priv->needs_reload_confirmation = FALSE;

	info_bar = gtk_info_bar_new_with_buttons (_("_Reload"),
						  GTK_RESPONSE_YES,
						  C_("MessageArea", "Hi_de"),
						  GTK_RESPONSE_NO, NULL);
	gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar),
				       GTK_MESSAGE_QUESTION);
	image = gtk_image_new_from_icon_name ("dialog-question",
					      GTK_ICON_SIZE_DIALOG);
	label = gtk_label_new (NULL);

	text = g_strdup_printf (_("The image “%s” has been modified by an external application."
				  " Would you like to reload it?"), eog_image_get_caption (img));
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	gtk_label_set_markup (GTK_LABEL (label), markup);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	g_free (text);
	g_free (markup);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
	gtk_widget_set_valign (image, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar))), hbox, TRUE, TRUE, 0);
	gtk_widget_show_all (hbox);
	gtk_widget_show (info_bar);

	eog_window_set_message_area (window, info_bar);
	g_signal_connect (info_bar, "response",
			  G_CALLBACK (file_changed_info_bar_response), window);
}

static void
eog_window_display_image (EogWindow *window, EogImage *image)
{
	EogWindowPrivate *priv;
	GFile *file;

	g_return_if_fail (EOG_IS_WINDOW (window));
	g_return_if_fail (EOG_IS_IMAGE (image));

	eog_debug (DEBUG_WINDOW);

	g_assert (eog_image_has_data (image, EOG_IMAGE_DATA_IMAGE));

	priv = window->priv;

	if (image != NULL) {
		g_signal_connect (image,
				  "thumbnail_changed",
				  G_CALLBACK (image_thumb_changed_cb),
				  window);
		g_signal_connect (image, "file-changed",
				  G_CALLBACK (image_file_changed_cb),
				  window);

		image_thumb_changed_cb (image, window);
	}

	priv->needs_reload_confirmation = TRUE;

	eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), image);

	hdy_header_bar_set_title (HDY_HEADER_BAR (priv->headerbar), eog_image_get_caption (image));
	gtk_window_set_title (GTK_WINDOW (window), eog_image_get_caption (image));

	update_status_bar (window);

	file = eog_image_get_file (image);
	g_idle_add_full (G_PRIORITY_LOW,
			 (GSourceFunc) add_file_to_recent_files,
			 file,
			 (GDestroyNotify) g_object_unref);

	if (eog_image_is_multipaged (image)) {
		GtkWidget *info_bar;

		eog_debug_message (DEBUG_IMAGE_DATA, "Image is multipaged");

		info_bar = eog_multipage_error_message_area_new ();
		g_signal_connect (info_bar,
				  "response",
				  G_CALLBACK (eog_window_error_message_area_response),
				  window);
		gtk_widget_show (info_bar);
		eog_window_set_message_area (window, info_bar);
	}

	slideshow_set_timeout (window);
}

static void
_eog_window_launch_appinfo_with_files (EogWindow *window,
				       GAppInfo *appinfo,
				       GList *files)
{
	GdkAppLaunchContext *context;

	context = gdk_display_get_app_launch_context (
	  gtk_widget_get_display (GTK_WIDGET (window)));
	gdk_app_launch_context_set_screen (context,
	  gtk_widget_get_screen (GTK_WIDGET (window)));
	gdk_app_launch_context_set_icon (context,
	  g_app_info_get_icon (appinfo));
	gdk_app_launch_context_set_timestamp (context,
	  gtk_get_current_event_time ());

	g_app_info_launch (appinfo, files, G_APP_LAUNCH_CONTEXT (context), NULL);

	g_object_unref (context);
}

static void
app_chooser_dialog_response_cb (GtkDialog *dialog,
                                gint response_id,
                                gpointer data)
{
	EogWindow *window;
	GAppInfo *app;
	GFile *file;
	GList *files = NULL;

	g_return_if_fail (EOG_IS_WINDOW (data));

	window = EOG_WINDOW (data);

	if (response_id != GTK_RESPONSE_OK) {
		goto out;
	}

	app = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (dialog));
	file = eog_image_get_file (window->priv->image);
	files = g_list_append (files, file);

	_eog_window_launch_appinfo_with_files (window, app, files);

	g_list_free (files);
	g_object_unref (file);
out:
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
eog_window_open_file_chooser_dialog (EogWindow *window)
{
	GtkWidget *dialog;
	GFileInfo *file_info;
	GFile *file;
	const gchar *mime_type = NULL;

	file = eog_image_get_file (window->priv->image);
	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","
				       G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
				       0, NULL, NULL);
	mime_type = g_content_type_get_mime_type (
			eog_util_get_content_type_with_fallback (file_info));
	g_object_unref (file_info);

	dialog = gtk_app_chooser_dialog_new_for_content_type (GTK_WINDOW (window),
							      GTK_DIALOG_MODAL |
							      GTK_DIALOG_DESTROY_WITH_PARENT |
							      GTK_DIALOG_USE_HEADER_BAR,
							      mime_type);
	gtk_widget_show (dialog);

	g_signal_connect_object (dialog, "response",
				 G_CALLBACK (app_chooser_dialog_response_cb),
				 window, 0);

	g_object_unref (file);
}

static void
eog_window_action_open_with (GSimpleAction *action,
                            GVariant       *parameter,
                            gpointer        user_data)
{
	EogWindow *window;

	g_return_if_fail (EOG_IS_WINDOW (user_data));
	window = EOG_WINDOW (user_data);

#ifdef HAVE_LIBPORTAL
	if (eog_util_is_running_inside_flatpak ()) {
		GFile *file = eog_image_get_file (window->priv->image);

		eog_util_open_file_with_flatpak_portal (file, GTK_WINDOW (window));
		g_object_unref (file);

		return;
	}
#endif
	eog_window_open_file_chooser_dialog (window);
}

static void
eog_window_clear_load_job (EogWindow *window)
{
	EogWindowPrivate *priv = window->priv;

	if (priv->load_job != NULL) {
		if (!priv->load_job->finished)
			eog_job_cancel (priv->load_job);

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
	EogWindow *window;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);

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
		gchar *str_image, *status_message;
		guint n_images;

		image = job->current_image;

		n_images = g_list_length (job->images);

		str_image = eog_image_get_uri_for_display (image);

		/* Translators: This string is displayed in the statusbar
		 * while saving images. The tokens are from left to right:
		 * - the original filename
		 * - the current image's position in the queue
		 * - the total number of images queued for saving */
		status_message = g_strdup_printf (_("Saving image “%s” (%u/%u)"),
					          str_image,
						  job->current_position + 1,
						  n_images);
		g_free (str_image);

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
	GdkMonitor *monitor;
	GdkRectangle monitor_rect;
	GtkAllocation allocation;
	gint final_width, final_height;
	gint screen_width, screen_height;
	gint window_width, window_height;
	gint img_width, img_height;
	gint view_width, view_height;
	gint deco_width, deco_height;

	update_action_groups_state (window);

	img_width = width;
	img_height = height;

	if (!gtk_widget_get_realized (window->priv->view)) {
		gtk_widget_realize (window->priv->view);
	}

	eog_debug_message (DEBUG_WINDOW, "Initial Image Size: %d x %d", img_width, img_height);

	gtk_widget_get_allocation (window->priv->view, &allocation);
	view_width  = allocation.width;
	view_height = allocation.height;

	eog_debug_message (DEBUG_WINDOW, "Initial View Size: %d x %d", view_width, view_height);


	if (!gtk_widget_get_realized (GTK_WIDGET (window))) {
		gtk_widget_realize (GTK_WIDGET (window));
	}

	gtk_widget_get_allocation (GTK_WIDGET (window), &allocation);
	window_width  = allocation.width;
	window_height = allocation.height;

	eog_debug_message (DEBUG_WINDOW, "Initial Window Size: %d x %d", window_width, window_height);


	monitor = gdk_display_get_monitor_at_window (
				gtk_widget_get_display (GTK_WIDGET (window)),
				gtk_widget_get_window (GTK_WIDGET (window)));

	gdk_monitor_get_geometry (monitor, &monitor_rect);

	screen_width  = monitor_rect.width;
	screen_height = monitor_rect.height;

	eog_debug_message (DEBUG_WINDOW, "Screen Size: %d x %d", screen_width, screen_height);

	deco_width = window_width - view_width;
	deco_height = window_height - view_height;

	eog_debug_message (DEBUG_WINDOW, "Decoration Size: %d x %d", deco_width, deco_height);

	if (img_width > 0 && img_height > 0) {
		if ((img_width + deco_width > screen_width) ||
		    (img_height + deco_height > screen_height))
		{
			double width_factor, height_factor, factor;

			width_factor = (screen_width * 0.85 - deco_width) / (double) img_width;
			height_factor = (screen_height * 0.85 - deco_height) / (double) img_height;
			factor = MIN (width_factor, height_factor);

			eog_debug_message (DEBUG_WINDOW, "Scaling Factor: %.2lf", factor);


			img_width = img_width * factor;
			img_height = img_height * factor;
		}
	}

	final_width = MAX (EOG_WINDOW_MIN_WIDTH, img_width + deco_width);
	final_height = MAX (EOG_WINDOW_MIN_HEIGHT, img_height + deco_height);

	eog_debug_message (DEBUG_WINDOW, "Setting window size: %d x %d", final_width, final_height);

	gtk_window_set_default_size (GTK_WINDOW (window), final_width, final_height);

	g_signal_emit (window, signals[SIGNAL_PREPARED], 0);
}

static void
eog_window_error_message_area_response (GtkInfoBar       *message_area,
					gint              response_id,
					EogWindow        *window)
{
	GAction *action_save_as;

	g_return_if_fail (GTK_IS_INFO_BAR (message_area));
	g_return_if_fail (EOG_IS_WINDOW (window));

	/* remove message area */
	eog_window_set_message_area (window, NULL);

	/* evaluate message area response */
	switch (response_id) {
	case EOG_ERROR_MESSAGE_AREA_RESPONSE_NONE:
	case EOG_ERROR_MESSAGE_AREA_RESPONSE_CANCEL:
	case GTK_RESPONSE_CLOSE:
		/* nothing to do in this case */
		break;
	case EOG_ERROR_MESSAGE_AREA_RESPONSE_RELOAD:
		/* TODO: trigger loading for current image again */
		break;
	case EOG_ERROR_MESSAGE_AREA_RESPONSE_SAVEAS:
		/* trigger save as command for current image */
		action_save_as =
			g_action_map_lookup_action (G_ACTION_MAP (window),
							      "save-as");
		eog_window_action_save_as (G_SIMPLE_ACTION (action_save_as), NULL, window);
		break;
	case EOG_ERROR_MESSAGE_AREA_RESPONSE_OPEN_WITH_EVINCE:
	{
#ifndef __APPLE__
		GDesktopAppInfo *app_info;
		GFile *img_file;
		GList *img_files = NULL;

		app_info = g_desktop_app_info_new ("org.gnome.Evince.desktop");

		if (app_info) {
			img_file = eog_image_get_file (window->priv->image);
			if (img_file) {
				img_files = g_list_append (img_files, img_file);
			}
			_eog_window_launch_appinfo_with_files (window,
							       G_APP_INFO (app_info),
							       img_files);
			g_list_free_full (img_files, g_object_unref);
		}
#endif
	}
		break;
	}
}

static void
eog_job_load_cb (EogJobLoad *job, gpointer data)
{
	EogWindow *window;
	EogWindowPrivate *priv;
	GAction *action_undo, *action_save;

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
		g_signal_handlers_disconnect_by_func (priv->image,
						      image_file_changed_cb,
						      window);

		g_object_unref (priv->image);
	}

	priv->image = g_object_ref (job->image);

	if (EOG_JOB (job)->error == NULL) {
#ifdef HAVE_LCMS
		eog_image_apply_display_profile (job->image,
						 priv->display_profile);
#endif

		_eog_window_enable_image_actions (window, TRUE);

		/* Make sure the window is really realized
		 *  before displaying the image. The ScrollView needs that.  */
        	if (!gtk_widget_get_realized (GTK_WIDGET (window))) {
			gint width = -1, height = -1;

			eog_image_get_size (job->image, &width, &height);
			eog_window_obtain_desired_size (job->image, width,
			                                height, window);

		}

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
		hdy_header_bar_set_title (HDY_HEADER_BAR (priv->headerbar),
					  eog_image_get_caption (job->image));
		gtk_window_set_title (GTK_WINDOW (window),
				      eog_image_get_caption (job->image));

		eog_window_set_message_area (window, message_area);

		gtk_info_bar_set_default_response (GTK_INFO_BAR (message_area),
						   GTK_RESPONSE_CANCEL);

		gtk_widget_show (message_area);

		update_status_bar (window);

		eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view), NULL);

        	if (window->priv->status == EOG_WINDOW_STATUS_INIT) {
			update_action_groups_state (window);

			g_signal_emit (window, signals[SIGNAL_PREPARED], 0);
		}

		_eog_window_enable_image_actions (window, FALSE);
	}

	eog_window_clear_load_job (window);

        if (window->priv->status == EOG_WINDOW_STATUS_INIT) {
		window->priv->status = EOG_WINDOW_STATUS_NORMAL;

		g_signal_handlers_disconnect_by_func
			(job->image,
			 G_CALLBACK (eog_window_obtain_desired_size),
			 window);
	}

	action_save = g_action_map_lookup_action (G_ACTION_MAP (window),
											  "save");
	action_undo = g_action_map_lookup_action (G_ACTION_MAP (window),
											  "undo");

	/* Set Save and Undo sensitive according to image state.
	 * Respect lockdown in case of Save.*/
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action_save),
			(!priv->save_disabled && eog_image_is_modified (job->image)));
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action_undo),
			eog_image_is_modified (job->image));

	g_object_unref (job->image);
}

static void
eog_window_clear_transform_job (EogWindow *window)
{
	EogWindowPrivate *priv = window->priv;

	if (priv->transform_job != NULL) {
		if (!priv->transform_job->finished)
			eog_job_cancel (priv->transform_job);

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
	GAction *action_undo, *action_save;
	EogImage *image;

        g_return_if_fail (EOG_IS_WINDOW (data));

	window = EOG_WINDOW (data);

	eog_window_clear_transform_job (window);

	action_undo =
		g_action_map_lookup_action (G_ACTION_MAP (window),
									"undo");
	action_save =
		g_action_map_lookup_action (G_ACTION_MAP (window),
									"save");

	image = eog_window_get_image (window);

	g_simple_action_set_enabled (G_SIMPLE_ACTION (action_undo),
								 eog_image_is_modified (image));

	if (!window->priv->save_disabled)
	{
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action_save),
									 eog_image_is_modified (image));
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

	g_signal_connect (priv->transform_job,
			  "progress",
			  G_CALLBACK (eog_job_progress_cb),
			  window);

	eog_job_scheduler_add_job_with_priority (priv->transform_job,
						 EOG_JOB_PRIORITY_MEDIUM);
}

static void
handle_image_selection_changed_cb (EogThumbView *thumbview, EogWindow *window)
{
	EogWindowPrivate *priv;
	EogImage *image;
	gchar *status_message;
	gchar *str_image;

	priv = window->priv;

	if (eog_list_store_length (EOG_LIST_STORE (priv->store)) == 0) {
		hdy_header_bar_set_title (HDY_HEADER_BAR (priv->headerbar),
					  g_get_application_name());
		gtk_window_set_title (GTK_WINDOW (window),
				      g_get_application_name());
		gtk_statusbar_remove_all (GTK_STATUSBAR (priv->statusbar),
					  priv->image_info_message_cid);
		eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->view),
					   NULL);
	}
	if (eog_thumb_view_get_n_selected (EOG_THUMB_VIEW (priv->thumbview)) == 0)
		return;

	update_selection_ui_visibility (window);

	image = eog_thumb_view_get_first_selected_image (EOG_THUMB_VIEW (priv->thumbview));

	g_assert (EOG_IS_IMAGE (image));

	eog_window_clear_load_job (window);

	eog_window_set_message_area (window, NULL);

	gtk_statusbar_pop (GTK_STATUSBAR (priv->statusbar),
			   priv->image_info_message_cid);

	if (image == priv->image) {
		update_status_bar (window);
		return;
	}

	if (eog_image_has_data (image, EOG_IMAGE_DATA_IMAGE)) {
		if (priv->image != NULL)
			g_object_unref (priv->image);
		priv->image = image;
		eog_window_display_image (window, image);
		return;
	}

	if (priv->status == EOG_WINDOW_STATUS_INIT) {
		g_signal_connect (image,
				  "size-prepared",
				  G_CALLBACK (eog_window_obtain_desired_size),
				  window);
	}

	priv->load_job = eog_job_load_new (image, EOG_IMAGE_DATA_ALL);

	g_signal_connect (priv->load_job,
			  "finished",
			  G_CALLBACK (eog_job_load_cb),
			  window);

	g_signal_connect (priv->load_job,
			  "progress",
			  G_CALLBACK (eog_job_progress_cb),
			  window);

	eog_job_scheduler_add_job_with_priority (priv->load_job,
						 EOG_JOB_PRIORITY_MEDIUM);

	str_image = eog_image_get_uri_for_display (image);

	status_message = g_strdup_printf (_("Opening image “%s”"),
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
	GAction *action_zoom_in;
	GAction *action_zoom_out;
	GAction *action_zoom_in_smooth;
	GAction *action_zoom_out_smooth;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);

	update_status_bar (window);

	action_zoom_in =
		g_action_map_lookup_action (G_ACTION_MAP (window),
					     "zoom-in");
	action_zoom_in_smooth =
		g_action_map_lookup_action (G_ACTION_MAP (window),
					     "zoom-in-smooth");

	action_zoom_out =
		g_action_map_lookup_action (G_ACTION_MAP (window),
					     "zoom-out");
	action_zoom_out_smooth =
		g_action_map_lookup_action (G_ACTION_MAP (window),
					     "zoom-out-smooth");

	g_simple_action_set_enabled (G_SIMPLE_ACTION (action_zoom_in),
			!eog_scroll_view_get_zoom_is_max (EOG_SCROLL_VIEW (window->priv->view)));
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action_zoom_in_smooth),
			!eog_scroll_view_get_zoom_is_max (EOG_SCROLL_VIEW (window->priv->view)));
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action_zoom_out),
			!eog_scroll_view_get_zoom_is_min (EOG_SCROLL_VIEW (window->priv->view)));
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action_zoom_out_smooth),
			!eog_scroll_view_get_zoom_is_min (EOG_SCROLL_VIEW (window->priv->view)));
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
	GAction *action;

	action = g_action_map_lookup_action (G_ACTION_MAP (window),
					      "view-fullscreen");

	g_simple_action_set_state (G_SIMPLE_ACTION (action),
			g_variant_new_boolean (window->priv->mode == EOG_WINDOW_MODE_FULLSCREEN));
}

static void
eog_window_update_slideshow_action (EogWindow *window)
{
	GAction *action;

	action = g_action_map_lookup_action (G_ACTION_MAP (window),
					      "view-slideshow");

	g_simple_action_set_state (G_SIMPLE_ACTION (action),
			g_variant_new_boolean (window->priv->mode == EOG_WINDOW_MODE_SLIDESHOW));
}

static void
eog_window_update_pause_slideshow_action (EogWindow *window)
{
	GAction *action;

	action = g_action_map_lookup_action (G_ACTION_MAP (window),
					      "pause-slideshow");

	g_simple_action_set_state (G_SIMPLE_ACTION (action),
			g_variant_new_boolean (window->priv->mode != EOG_WINDOW_MODE_SLIDESHOW));
}

static gboolean
fullscreen_timeout_cb (gpointer data)
{
	EogWindow *window = EOG_WINDOW (data);

	eog_debug (DEBUG_WINDOW);

	gtk_revealer_set_reveal_child (
		    GTK_REVEALER (window->priv->fullscreen_popup), FALSE);
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
		return G_SOURCE_REMOVE;
	}

	eog_thumb_view_select_single (EOG_THUMB_VIEW (priv->thumbview),
				      EOG_THUMB_VIEW_SELECT_RIGHT);

	return G_SOURCE_REMOVE;
}

static void
fullscreen_clear_timeout (EogWindow *window)
{
	eog_debug (DEBUG_WINDOW);

	if (window->priv->fullscreen_timeout_source != NULL) {
		g_source_destroy (window->priv->fullscreen_timeout_source);
		g_source_unref (window->priv->fullscreen_timeout_source);
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
		g_source_destroy (window->priv->slideshow_switch_source);
		g_source_unref (window->priv->slideshow_switch_source);
	}

	window->priv->slideshow_switch_source = NULL;
}

static void
slideshow_set_timeout (EogWindow *window)
{
	GSource *source;

	eog_debug (DEBUG_WINDOW);

	slideshow_clear_timeout (window);

	if (window->priv->mode != EOG_WINDOW_MODE_SLIDESHOW)
		return;

	if (window->priv->slideshow_switch_timeout <= 0)
		return;

	source = g_timeout_source_new (window->priv->slideshow_switch_timeout * 1000);
	g_source_set_callback (source, slideshow_switch_cb, window, NULL);

	g_source_attach (source, NULL);

	window->priv->slideshow_switch_source = source;
}

static void
show_fullscreen_popup (EogWindow *window)
{
	eog_debug (DEBUG_WINDOW);

	if (!gtk_widget_get_visible (window->priv->fullscreen_popup)) {
		gtk_widget_show_all (GTK_WIDGET (window->priv->fullscreen_popup));
	}

	gtk_revealer_set_reveal_child (
		    GTK_REVEALER (window->priv->fullscreen_popup), TRUE);

	fullscreen_set_timeout (window);
}

static gboolean
fullscreen_motion_notify_cb (GtkWidget      *widget,
			     GdkEventMotion *event,
			     gpointer       user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);

	eog_debug (DEBUG_WINDOW);

	if (event->y < EOG_WINDOW_FULLSCREEN_POPUP_THRESHOLD) {
		show_fullscreen_popup (window);
	} else {
		fullscreen_set_timeout (window);
	}

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
	GAction *action;

	eog_debug (DEBUG_WINDOW);

	if (window->priv->mode == EOG_WINDOW_MODE_SLIDESHOW) {
		action = g_action_map_lookup_action (G_ACTION_MAP (window),
						      "view-slideshow");
	} else {
		action = g_action_map_lookup_action (G_ACTION_MAP (window),
						      "view-fullscreen");
	}
	g_return_if_fail (action != NULL);

	g_action_change_state (action, g_variant_new_boolean (FALSE));
}

static GtkWidget *
eog_window_create_fullscreen_popup (EogWindow *window)
{
	GtkWidget *revealer;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *toolbar;
	GtkBuilder *builder;

	eog_debug (DEBUG_WINDOW);

	revealer = gtk_revealer_new();
	gtk_widget_add_events (revealer, GDK_ENTER_NOTIFY_MASK);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_valign (revealer, GTK_ALIGN_START);
	gtk_widget_set_halign (revealer, GTK_ALIGN_FILL);
	gtk_container_add (GTK_CONTAINER (revealer), hbox);

	builder = gtk_builder_new_from_resource ("/org/gnome/eog/ui/fullscreen-toolbar.ui");
	toolbar = GTK_WIDGET (gtk_builder_get_object (builder, "fullscreen_toolbar"));
	g_assert (GTK_IS_TOOLBAR (toolbar));

	gtk_box_pack_start (GTK_BOX (hbox), toolbar, TRUE, TRUE, 0);

	button = GTK_WIDGET (gtk_builder_get_object (builder, "exit_fullscreen_button"));
	g_signal_connect (button, "clicked",
			  G_CALLBACK (exit_fullscreen_button_clicked_cb),
			  window);

	/* Disable timer when the pointer enters the toolbar window. */
	g_signal_connect (revealer,
			  "enter-notify-event",
			  G_CALLBACK (fullscreen_leave_notify_cb),
			  window);

	g_object_unref (builder);
	return revealer;
}

static void
update_ui_visibility (EogWindow *window)
{
	EogWindowPrivate *priv;

	GAction *action;

	gboolean fullscreen_mode, visible;

	g_return_if_fail (EOG_IS_WINDOW (window));

	eog_debug (DEBUG_WINDOW);

	priv = window->priv;

	fullscreen_mode = priv->mode == EOG_WINDOW_MODE_FULLSCREEN ||
			  priv->mode == EOG_WINDOW_MODE_SLIDESHOW;

	visible = g_settings_get_boolean (priv->ui_settings,
					  EOG_CONF_UI_STATUSBAR);
	visible = visible && !fullscreen_mode;
	action = g_action_map_lookup_action (G_ACTION_MAP (window), "view-statusbar");
	g_assert (action != NULL);
	g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (visible));
	gtk_widget_set_visible (priv->statusbar, visible);

	if (priv->status != EOG_WINDOW_STATUS_INIT) {
		visible = g_settings_get_boolean (priv->ui_settings,
						  EOG_CONF_UI_IMAGE_GALLERY);
		visible &= gtk_widget_get_visible (priv->nav);
		visible &= (priv->mode != EOG_WINDOW_MODE_SLIDESHOW);
		action = g_action_map_lookup_action (G_ACTION_MAP (window), "view-gallery");
		g_assert (action != NULL);
		g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (visible));
		gtk_widget_set_visible (priv->nav, visible);
	}

	visible = g_settings_get_boolean (priv->ui_settings,
					  EOG_CONF_UI_SIDEBAR);
	visible = visible && !fullscreen_mode;
	action = g_action_map_lookup_action (G_ACTION_MAP (window), "view-sidebar");
	g_assert (action != NULL);
	g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (visible));
	gtk_widget_set_visible (priv->sidebar, visible);

  gtk_widget_set_visible (priv->headerbar, !fullscreen_mode);

	if (priv->fullscreen_popup != NULL) {
		gtk_widget_hide (priv->fullscreen_popup);
	}
}

static void
eog_window_inhibit_screensaver (EogWindow *window)
{
	EogWindowPrivate *priv = window->priv;

	/* Already inhibited */
	if (G_UNLIKELY (priv->fullscreen_idle_inhibit_cookie != 0))
		return;

	eog_debug (DEBUG_WINDOW);

	window->priv->fullscreen_idle_inhibit_cookie =
		gtk_application_inhibit (GTK_APPLICATION (EOG_APP),
		                         GTK_WINDOW (window),
		                         GTK_APPLICATION_INHIBIT_IDLE,
	/* L10N: This the reason why the screensaver is inhibited. */
		                         _("Viewing a slideshow"));
}

static void
eog_window_uninhibit_screensaver (EogWindow *window)
{
	EogWindowPrivate *priv = window->priv;

	if (G_UNLIKELY (priv->fullscreen_idle_inhibit_cookie == 0))
		return;

	eog_debug (DEBUG_WINDOW);

	gtk_application_uninhibit (GTK_APPLICATION (EOG_APP),
	                           priv->fullscreen_idle_inhibit_cookie);
	priv->fullscreen_idle_inhibit_cookie = 0;
}

static void
eog_window_run_fullscreen (EogWindow *window, gboolean slideshow)
{
	static const GdkRGBA black = { 0., 0., 0., 1.};
	EogWindowPrivate *priv;
	gboolean upscale;

	eog_debug (DEBUG_WINDOW);

	priv = window->priv;

	if (slideshow) {
		priv->mode = EOG_WINDOW_MODE_SLIDESHOW;
	} else {
		/* Stop the timer if we come from slideshowing */
		if (priv->mode == EOG_WINDOW_MODE_SLIDESHOW) {
			slideshow_clear_timeout (window);
			eog_window_uninhibit_screensaver (window);
		}

		priv->mode = EOG_WINDOW_MODE_FULLSCREEN;
	}

	if (window->priv->fullscreen_popup == NULL)
	{
		priv->fullscreen_popup
			= eog_window_create_fullscreen_popup (window);
		gtk_overlay_add_overlay (GTK_OVERLAY(priv->overlay),
					 priv->fullscreen_popup);
	}

	update_ui_visibility (window);

	g_signal_connect (priv->view,
			  "motion-notify-event",
			  G_CALLBACK (fullscreen_motion_notify_cb),
			  window);

	g_signal_connect (priv->view,
			  "leave-notify-event",
			  G_CALLBACK (fullscreen_leave_notify_cb),
			  window);

	g_signal_connect (priv->thumbview,
			  "motion-notify-event",
			  G_CALLBACK (fullscreen_motion_notify_cb),
			  window);

	g_signal_connect (priv->thumbview,
			  "leave-notify-event",
			  G_CALLBACK (fullscreen_leave_notify_cb),
			  window);

	fullscreen_set_timeout (window);

	if (slideshow) {
		priv->slideshow_loop =
			g_settings_get_boolean (priv->fullscreen_settings,
						EOG_CONF_FULLSCREEN_LOOP);

		priv->slideshow_switch_timeout =
			g_settings_get_int (priv->fullscreen_settings,
					    EOG_CONF_FULLSCREEN_SECONDS);

		slideshow_set_timeout (window);
	}

	upscale = g_settings_get_boolean (priv->fullscreen_settings,
					  EOG_CONF_FULLSCREEN_UPSCALE);

	eog_scroll_view_set_zoom_upscale (EOG_SCROLL_VIEW (priv->view),
					  upscale);

	gtk_widget_grab_focus (priv->view);

	eog_scroll_view_override_bg_color (EOG_SCROLL_VIEW (window->priv->view),
					   &black);

	gtk_window_fullscreen (GTK_WINDOW (window));

	if (slideshow)
		eog_window_inhibit_screensaver (window);

	/* Update both actions as we could've already been in one those modes */
	eog_window_update_slideshow_action (window);
	eog_window_update_fullscreen_action (window);
	eog_window_update_pause_slideshow_action (window);
}

static void
eog_window_stop_fullscreen (EogWindow *window, gboolean slideshow)
{
	EogWindowPrivate *priv;

	eog_debug (DEBUG_WINDOW);

	priv = window->priv;

	if (priv->mode != EOG_WINDOW_MODE_SLIDESHOW &&
	    priv->mode != EOG_WINDOW_MODE_FULLSCREEN) return;

	priv->mode = EOG_WINDOW_MODE_NORMAL;

	fullscreen_clear_timeout (window);
	gtk_revealer_set_reveal_child (GTK_REVEALER(window->priv->fullscreen_popup), FALSE);

	if (slideshow) {
		slideshow_clear_timeout (window);
	}

	g_signal_handlers_disconnect_by_func (priv->view,
					      (gpointer) fullscreen_motion_notify_cb,
					      window);

	g_signal_handlers_disconnect_by_func (priv->view,
					      (gpointer) fullscreen_leave_notify_cb,
					      window);

	g_signal_handlers_disconnect_by_func (priv->thumbview,
					      (gpointer) fullscreen_motion_notify_cb,
					      window);

	g_signal_handlers_disconnect_by_func (priv->thumbview,
					      (gpointer) fullscreen_leave_notify_cb,
					      window);

	update_ui_visibility (window);

	eog_scroll_view_set_zoom_upscale (EOG_SCROLL_VIEW (priv->view), FALSE);

	eog_scroll_view_override_bg_color (EOG_SCROLL_VIEW (window->priv->view),
					   NULL);
	gtk_window_unfullscreen (GTK_WINDOW (window));

	if (slideshow) {
		eog_window_update_slideshow_action (window);
		eog_window_uninhibit_screensaver (window);
	} else {
		eog_window_update_fullscreen_action (window);
	}

	eog_scroll_view_show_cursor (EOG_SCROLL_VIEW (priv->view));
}

static void
set_basename_for_print_settings (GtkPrintSettings *print_settings, EogWindow *window)
{
	const char *basename = NULL;

	if(G_LIKELY (window->priv->image != NULL))
		basename = eog_image_get_caption (window->priv->image);

	if (G_LIKELY(basename))
		gtk_print_settings_set (print_settings,
		                        GTK_PRINT_SETTINGS_OUTPUT_BASENAME,
		                        basename);
}

static void
eog_window_print (EogWindow *window)
{
	GtkWidget *dialog;
	GError *error = NULL;
	GtkPrintOperation *print;
	GtkPrintOperationResult res;
	GtkPageSetup *page_setup;
	GtkPrintSettings *print_settings;
	gboolean page_setup_disabled = FALSE;

	eog_debug (DEBUG_PRINTING);

	print_settings = eog_print_get_print_settings ();
	set_basename_for_print_settings (print_settings, window);

	/* Make sure the window stays valid while printing */
	g_object_ref (window);

	if (window->priv->page_setup != NULL)
		page_setup = g_object_ref (window->priv->page_setup);
	else
		page_setup = NULL;

	print = eog_print_operation_new (window->priv->image,
					 print_settings,
					 page_setup);


	// Disable page setup options if they are locked down
	page_setup_disabled = g_settings_get_boolean (window->priv->lockdown_settings,
						      EOG_CONF_DESKTOP_CAN_SETUP_PAGE);
	if (page_setup_disabled)
		gtk_print_operation_set_embed_page_setup (print, FALSE);


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
		GtkPageSetup *new_page_setup;
		eog_print_set_print_settings (gtk_print_operation_get_print_settings (print));
		new_page_setup = gtk_print_operation_get_default_page_setup (print);
		if (window->priv->page_setup != NULL)
			g_object_unref (window->priv->page_setup);
		window->priv->page_setup = g_object_ref (new_page_setup);
	}

	if (page_setup != NULL)
		g_object_unref (page_setup);
	g_object_unref (print_settings);
	g_object_unref (window);
}

static void
eog_window_action_file_open (GSimpleAction *action,
			     GVariant      *parameter,
			     gpointer       user_data)
{
	EogWindow *window;
	EogWindowPrivate *priv;
	EogImage *current;
	GtkWidget *dlg;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);

	priv = window->priv;

	dlg = eog_file_chooser_new (GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (window));

	current = eog_thumb_view_get_first_selected_image (EOG_THUMB_VIEW (priv->thumbview));

	if (current != NULL) {
		gchar *dir_uri, *file_uri;

		file_uri = eog_image_get_uri_for_display (current);
		dir_uri = g_path_get_dirname (file_uri);

		gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dlg),
							 dir_uri);
		g_free (file_uri);
		g_free (dir_uri);
		g_object_unref (current);
	} else {
		/* If desired by the user,
		   fallback to the XDG_PICTURES_DIR (if available) */
		const gchar *pics_dir;
		gboolean use_fallback;

		use_fallback = g_settings_get_boolean (priv->ui_settings,
					EOG_CONF_UI_FILECHOOSER_XDG_FALLBACK);
		pics_dir = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
		if (use_fallback && pics_dir) {
			gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg),
							     pics_dir);
		}
	}

	g_signal_connect (dlg, "response",
			  G_CALLBACK (file_open_dialog_response_cb),
			  window);

	gtk_widget_show_all (dlg);
}

static void
eog_job_close_save_cb (EogJobSave *job, gpointer user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);
	GAction *action_save;

	g_signal_handlers_disconnect_by_func (job,
					      eog_job_close_save_cb,
					      window);

	/* clean the last save job */
	g_clear_object (&window->priv->save_job);

	/* recover save action from actions group */
	action_save = g_action_map_lookup_action (G_ACTION_MAP (window),
						   "save");

	/* check if job contains any error */
	if (EOG_JOB (job)->error == NULL) {
		gtk_widget_destroy (GTK_WIDGET (window));
	} else {
		GtkWidget *message_area;

		eog_thumb_view_set_current_image (EOG_THUMB_VIEW (window->priv->thumbview),
						  job->current_image,
						  TRUE);

		message_area = eog_image_save_error_message_area_new (
					eog_image_get_caption (job->current_image),
					EOG_JOB (job)->error);

		g_signal_connect (message_area,
				  "response",
				  G_CALLBACK (eog_window_error_message_area_response),
				  window);

		gtk_window_set_icon (GTK_WINDOW (window), NULL);
		hdy_header_bar_set_title (HDY_HEADER_BAR (window->priv->headerbar),
					  eog_image_get_caption (job->current_image));
		gtk_window_set_title (GTK_WINDOW (window),
				      eog_image_get_caption (job->current_image));

		eog_window_set_message_area (window, message_area);

		gtk_info_bar_set_default_response (GTK_INFO_BAR (message_area),
						   GTK_RESPONSE_CANCEL);

		gtk_widget_show (message_area);

		update_status_bar (window);

		g_simple_action_set_enabled (G_SIMPLE_ACTION (action_save), TRUE);
	}
}

static void
close_confirmation_dialog_response_handler (EogCloseConfirmationDialog *dlg,
					    gint                        response_id,
					    EogWindow                  *window)
{
	GList            *selected_images;
	EogWindowPrivate *priv;
	GAction          *action_save_as;

	priv = window->priv;

	switch (response_id) {
	case EOG_CLOSE_CONFIRMATION_DIALOG_RESPONSE_SAVE:
		selected_images = eog_close_confirmation_dialog_get_selected_images (dlg);
		gtk_widget_destroy (GTK_WIDGET (dlg));

		if (eog_window_save_images (window, selected_images)) {
			g_signal_connect (priv->save_job,
					  "finished",
					  G_CALLBACK (eog_job_close_save_cb),
					  window);

			eog_job_scheduler_add_job (priv->save_job);
		}

		break;

	case EOG_CLOSE_CONFIRMATION_DIALOG_RESPONSE_SAVEAS:
		selected_images = eog_close_confirmation_dialog_get_selected_images (dlg);
		gtk_widget_destroy (GTK_WIDGET (dlg));

		eog_thumb_view_set_current_image (EOG_THUMB_VIEW (priv->thumbview),
						  g_list_first (selected_images)->data,
						  TRUE);

		action_save_as = g_action_map_lookup_action (G_ACTION_MAP (window),
							      "save-as");
		eog_window_action_save_as (G_SIMPLE_ACTION (action_save_as), NULL, window);
		break;

	case EOG_CLOSE_CONFIRMATION_DIALOG_RESPONSE_CLOSE:
		gtk_widget_destroy (GTK_WIDGET (window));
		break;

	case EOG_CLOSE_CONFIRMATION_DIALOG_RESPONSE_CANCEL:
		gtk_widget_destroy (GTK_WIDGET (dlg));
		break;
	}
}

static gboolean
eog_window_unsaved_images_confirm (EogWindow *window)
{
	EogWindowPrivate *priv;
	gboolean disabled;
	GtkWidget *dialog;
	GList *list;
	EogImage *image;
	GtkTreeIter iter;

	priv = window->priv;

	disabled = g_settings_get_boolean(priv->ui_settings,
					EOG_CONF_UI_DISABLE_CLOSE_CONFIRMATION);
	disabled |= window->priv->save_disabled;

	if (disabled || !priv->store) {
		return FALSE;
	}

	list = NULL;
	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &iter)) {
		do {
			gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
					    EOG_LIST_STORE_EOG_IMAGE, &image,
					    -1);
			if (!image)
				continue;

			if (eog_image_is_modified (image)) {
				list = g_list_prepend (list, image);
			} else {
				g_object_unref (image);
			}
		} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &iter));
	}

	if (list) {
		list = g_list_reverse (list);
		dialog = eog_close_confirmation_dialog_new (GTK_WINDOW (window),
							    list);
		g_list_free (list);
		g_signal_connect (dialog,
				  "response",
				  G_CALLBACK (close_confirmation_dialog_response_handler),
				  window);
		gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

		gtk_widget_show (dialog);
		return TRUE;

	}
	return FALSE;
}

static void
eog_window_action_close_window (GSimpleAction *action,
				GVariant      *variant,
				gpointer       user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_window_close (EOG_WINDOW (user_data));
}

static void
eog_window_action_close_all_windows (GSimpleAction *action,
				     GVariant      *variant,
				     gpointer       user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_application_close_all_windows (EOG_APP);
}

static void
eog_window_action_preferences (GSimpleAction *action,
			       GVariant      *variant,
			       gpointer       user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_window_show_preferences_dialog (EOG_WINDOW (user_data));
}

static void
eog_window_action_help (GSimpleAction *action,
			GVariant      *variant,
			gpointer       user_data)
{
	EogWindow *window;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);

	eog_util_show_help (NULL, GTK_WINDOW (window));
}

static void
eog_window_action_about (GSimpleAction *action,
			 GVariant      *variant,
			 gpointer       user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_window_show_about_dialog (EOG_WINDOW (user_data));
}

static void
eog_window_action_toggle_properties (GSimpleAction *action,
				     GVariant      *variant,
				     gpointer       user_data)
{
	static gint PROPERTIES_PAGE = 0;

	EogWindow *window;
	EogWindowPrivate *priv;
	gboolean was_visible;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);
	priv = window->priv;

	if (priv->mode != EOG_WINDOW_MODE_NORMAL &&
            priv->mode != EOG_WINDOW_MODE_FULLSCREEN) return;

	was_visible = gtk_widget_get_visible (priv->sidebar) &&
	              eog_sidebar_get_page_nr (EOG_SIDEBAR (priv->sidebar)) == PROPERTIES_PAGE;

	if  (!was_visible) {
		eog_sidebar_set_page_nr (EOG_SIDEBAR (priv->sidebar), PROPERTIES_PAGE);
	}
	gtk_widget_set_visible (priv->sidebar, !was_visible);
	g_settings_set_boolean (priv->ui_settings, EOG_CONF_UI_SIDEBAR, !was_visible);
}

static void
eog_window_action_show_hide_bar (GSimpleAction *action,
				 GVariant      *state,
				 gpointer       user_data)
{
	EogWindow *window;
	EogWindowPrivate *priv;
	gboolean visible;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);
	priv = window->priv;

	if (priv->mode != EOG_WINDOW_MODE_NORMAL &&
            priv->mode != EOG_WINDOW_MODE_FULLSCREEN) return;

	visible = g_variant_get_boolean (state);

	if (g_ascii_strcasecmp (g_action_get_name (G_ACTION (action)), "view-statusbar") == 0) {
		gtk_widget_set_visible (priv->statusbar, visible);
		g_simple_action_set_state (action, state);

		if (priv->mode == EOG_WINDOW_MODE_NORMAL)
			g_settings_set_boolean (priv->ui_settings,
						EOG_CONF_UI_STATUSBAR, visible);

	} else if (g_ascii_strcasecmp (g_action_get_name (G_ACTION (action)), "view-gallery") == 0) {
		if (visible) {
			/* Make sure the focus widget is realized to
			 * avoid warnings on keypress events */
			if (!gtk_widget_get_realized (window->priv->thumbview))
				gtk_widget_realize (window->priv->thumbview);

			gtk_widget_show (priv->nav);
		} else {
			/* Make sure the focus widget is realized to
			 * avoid warnings on keypress events.
			 * Don't do it during init phase or the view
			 * will get a bogus allocation. */
			if (!gtk_widget_get_realized (priv->view)
			    && priv->status == EOG_WINDOW_STATUS_NORMAL)
				gtk_widget_realize (priv->view);

			gtk_widget_hide (priv->nav);
		}
		g_simple_action_set_state (action, state);
		g_settings_set_boolean (priv->ui_settings,
					EOG_CONF_UI_IMAGE_GALLERY, visible);

	} else if (g_ascii_strcasecmp (g_action_get_name (G_ACTION (action)), "view-sidebar") == 0) {
		gtk_widget_set_visible (priv->sidebar, visible);
		g_simple_action_set_state (action, state);
		g_settings_set_boolean (priv->ui_settings, EOG_CONF_UI_SIDEBAR,
					visible);
	}
}

static gboolean
in_desktop (const gchar *name)
{
	const gchar *desktop_name_list;
	gchar **names;
	gboolean in_list = FALSE;
	gint i;

	desktop_name_list = g_getenv ("XDG_CURRENT_DESKTOP");
	if (!desktop_name_list)
		return FALSE;

	names = g_strsplit (desktop_name_list, ":", -1);
	for (i = 0; names[i] && !in_list; i++)
		if (strcmp (names[i], name) == 0) {
			in_list = TRUE;
			break;
		}
	g_strfreev (names);

	return in_list;
}

static void
wallpaper_info_bar_response (GtkInfoBar *bar, gint response, EogWindow *window)
{
	if (response == GTK_RESPONSE_YES) {
		GAppInfo *app_info;
		gchar *path;
		GError *error = NULL;

		path = g_find_program_in_path ("unity-control-center");
		if (path && in_desktop ("Unity"))
			app_info = g_app_info_create_from_commandline ("unity-control-center appearance",
								       "System Settings",
								       G_APP_INFO_CREATE_NONE,
								       &error);
		else
			app_info = g_app_info_create_from_commandline ("gnome-control-center background",
								       "System Settings",
								       G_APP_INFO_CREATE_NONE,
								       &error);
		g_free (path);

		if (error != NULL) {
			g_warning ("%s%s", _("Error launching System Settings: "),
				   error->message);
			g_error_free (error);
			error = NULL;
		}

		if (app_info != NULL) {
			GdkAppLaunchContext *context;
			GdkDisplay *display;

			display = gtk_widget_get_display (GTK_WIDGET (window));
			context = gdk_display_get_app_launch_context (display);
			g_app_info_launch (app_info, NULL, G_APP_LAUNCH_CONTEXT (context), &error);

			if (error != NULL) {
				g_warning ("%s%s", _("Error launching System Settings: "),
					   error->message);
				g_error_free (error);
				error = NULL;
			}

			g_object_unref (context);
			g_object_unref (app_info);
		}
	}

	/* Close message area on every response */
	eog_window_set_message_area (window, NULL);
}

static void
eog_window_set_wallpaper (EogWindow *window, const gchar *filename, const gchar *visible_filename)
{
	GSettings *settings;
	GtkWidget *info_bar;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *hbox;
	gchar *markup;
	gchar *text;
	gchar *basename;
	gchar *uri;

	uri = g_filename_to_uri (filename, NULL, NULL);
	settings = g_settings_new (EOG_CONF_DESKTOP_WALLPAPER_SCHEMA);
	g_settings_set_string (settings, EOG_CONF_DESKTOP_WALLPAPER, uri);
	g_settings_set_string (settings, EOG_CONF_DESKTOP_WALLPAPER_DARK, uri);
	g_object_unref (settings);
	g_free (uri);

	info_bar = gtk_info_bar_new_with_buttons (_("_Open Background Preferences"),
						  GTK_RESPONSE_YES,
						  C_("MessageArea","Hi_de"),
						  GTK_RESPONSE_NO, NULL);
	gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar),
				       GTK_MESSAGE_QUESTION);

	image = gtk_image_new_from_icon_name ("dialog-question",
					      GTK_ICON_SIZE_DIALOG);
	label = gtk_label_new (NULL);

	if (!visible_filename)
		basename = g_path_get_basename (filename);

	text = g_strdup_printf (_("The image “%s” has been set as Desktop Background."
				  " Would you like to modify its appearance?"),
				visible_filename ? visible_filename : basename);
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	gtk_label_set_markup (GTK_LABEL (label), markup);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	g_free (markup);
	g_free (text);
	if (!visible_filename)
		g_free (basename);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
	gtk_widget_set_valign (image, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar))), hbox, TRUE, TRUE, 0);
	gtk_widget_show_all (hbox);
	gtk_widget_show (info_bar);


	eog_window_set_message_area (window, info_bar);
	gtk_info_bar_set_default_response (GTK_INFO_BAR (info_bar),
					   GTK_RESPONSE_YES);
	g_signal_connect (info_bar, "response",
			  G_CALLBACK (wallpaper_info_bar_response), window);
}

static void
eog_job_save_cb (EogJobSave *job, gpointer user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);
	GAction *action_save;

	g_signal_handlers_disconnect_by_func (job,
					      eog_job_save_cb,
					      window);

	g_signal_handlers_disconnect_by_func (job,
					      eog_job_save_progress_cb,
					      window);

	/* clean the last save job */
	g_clear_object (&window->priv->save_job);

	/* recover save action from actions group */
	action_save = g_action_map_lookup_action (G_ACTION_MAP (window),
						   "save");

	/* check if job contains any error */
	if (EOG_JOB (job)->error == NULL) {
		update_status_bar (window);
		hdy_header_bar_set_title (HDY_HEADER_BAR (window->priv->headerbar),
					  eog_image_get_caption (job->current_image));
		gtk_window_set_title (GTK_WINDOW (window),
				      eog_image_get_caption (job->current_image));

		g_simple_action_set_enabled (G_SIMPLE_ACTION (action_save), FALSE);
	} else {
		GtkWidget *message_area;

		message_area = eog_image_save_error_message_area_new (
					eog_image_get_caption (job->current_image),
					EOG_JOB (job)->error);

		g_signal_connect (message_area,
				  "response",
				  G_CALLBACK (eog_window_error_message_area_response),
				  window);

		gtk_window_set_icon (GTK_WINDOW (window), NULL);
		hdy_header_bar_set_title (HDY_HEADER_BAR (window->priv->headerbar),
					  eog_image_get_caption (job->current_image));
		gtk_window_set_title (GTK_WINDOW (window),
				      eog_image_get_caption (job->current_image));

		eog_window_set_message_area (window, message_area);

		gtk_info_bar_set_default_response (GTK_INFO_BAR (message_area),
						   GTK_RESPONSE_CANCEL);

		gtk_widget_show (message_area);

		update_status_bar (window);

		g_simple_action_set_enabled (G_SIMPLE_ACTION (action_save), TRUE);
	}
}

static void
eog_job_copy_cb (EogJobCopy *job, gpointer user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);
	gchar *filepath, *basename, *filename, *extension;
	GAction *action;
	GFile *source_file, *dest_file;
	gint64 mtime;

	/* Create source GFile */
	basename = g_file_get_basename (job->images->data);
	filepath = g_build_filename (job->destination, basename, NULL);
	source_file = g_file_new_for_path (filepath);
	g_free (filepath);

	/* Create destination GFile */
	extension = eog_util_filename_get_extension (basename);
	filename = g_strdup_printf  ("%s.%s", EOG_WALLPAPER_FILENAME, extension);
	filepath = g_build_filename (job->destination, filename, NULL);
	dest_file = g_file_new_for_path (filepath);
	g_free (filename);
	g_free (extension);

	/* Move the file */
	g_file_move (source_file, dest_file, G_FILE_COPY_OVERWRITE,
		     NULL, NULL, NULL, NULL);

	/* Update mtime, see bug 664747 */
	mtime = g_get_real_time ();
	g_file_set_attribute_uint64 (dest_file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
				     (guint64)(mtime / G_USEC_PER_SEC),
				     G_FILE_QUERY_INFO_NONE, NULL, NULL);
	g_file_set_attribute_uint32 (dest_file,
				     G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC,
				     (guint32)(mtime % G_USEC_PER_SEC),
				     G_FILE_QUERY_INFO_NONE, NULL, NULL);

	/* Set the wallpaper */
	eog_window_set_wallpaper (window, filepath, basename);
	g_free (basename);
	g_free (filepath);

	gtk_statusbar_pop (GTK_STATUSBAR (window->priv->statusbar),
			   window->priv->copy_file_cid);
	action = g_action_map_lookup_action (G_ACTION_MAP (window),
					      "set-wallpaper");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

	window->priv->copy_job = NULL;

	g_object_unref (source_file);
	g_object_unref (dest_file);
	g_object_unref (job);
}

static gboolean
eog_window_save_images (EogWindow *window, GList *images)
{
	EogWindowPrivate *priv;

	priv = window->priv;

	if (window->priv->save_job != NULL)
		return FALSE;

	priv->save_job = eog_job_save_new (images);

	g_signal_connect (priv->save_job,
			  "finished",
			  G_CALLBACK (eog_job_save_cb),
			  window);

	g_signal_connect (priv->save_job,
			  "progress",
			  G_CALLBACK (eog_job_save_progress_cb),
			  window);

	return TRUE;
}

static void
eog_window_action_save (GSimpleAction *action,
			GVariant      *variant,
			gpointer       user_data)
{
	EogWindowPrivate *priv;
	EogWindow *window;
	GList *images;

	window = EOG_WINDOW (user_data);
	priv = window->priv;

	if (window->priv->save_job != NULL)
		return;

	images = eog_thumb_view_get_selected_images (EOG_THUMB_VIEW (priv->thumbview));

	if (eog_window_save_images (window, images)) {
		eog_job_scheduler_add_job (priv->save_job);
	}
}

static GFile*
eog_window_retrieve_save_as_file (EogWindow *window, EogImage *image)
{
	GtkWidget *dialog;
	GFile *save_file = NULL;
	GFile *last_dest_folder;
	gint response;

	g_assert (image != NULL);

	dialog = eog_file_chooser_new (GTK_FILE_CHOOSER_ACTION_SAVE);

	last_dest_folder = window->priv->last_save_as_folder;

	if (last_dest_folder && g_file_query_exists (last_dest_folder, NULL)) {
		gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (dialog), last_dest_folder, NULL);
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog),
						 eog_image_get_caption (image));
	} else {
		GFile *image_file;

		image_file = eog_image_get_file (image);
		/* Setting the file will also navigate to its parent folder */
		gtk_file_chooser_set_file (GTK_FILE_CHOOSER (dialog),
					   image_file, NULL);
		g_object_unref (image_file);
	}

	gtk_window_set_transient_for (GTK_WINDOW(dialog), GTK_WINDOW(window));
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_hide (dialog);

	if (response == GTK_RESPONSE_OK) {
		save_file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
		if (window->priv->last_save_as_folder)
			g_object_unref (window->priv->last_save_as_folder);
		window->priv->last_save_as_folder = g_file_get_parent (save_file);
	}
	gtk_widget_destroy (dialog);

	return save_file;
}

static void
eog_window_action_save_as (GSimpleAction *action,
			   GVariant      *variant,
			   gpointer       user_data)
{
        EogWindowPrivate *priv;
        EogWindow *window;
	GList *images;
	guint n_images;

        window = EOG_WINDOW (user_data);
	priv = window->priv;

	if (window->priv->save_job != NULL)
		return;

	images = eog_thumb_view_get_selected_images (EOG_THUMB_VIEW (priv->thumbview));
	n_images = g_list_length (images);

	if (n_images == 1) {
		GFile *file;

		file = eog_window_retrieve_save_as_file (window, images->data);

		if (!file) {
			g_list_free (images);
			return;
		}

		priv->save_job = eog_job_save_as_new (images, NULL, file);

		g_object_unref (file);
	} else if (n_images > 1) {
		GFile *base_file;
		GtkWidget *dialog;
		gchar *basedir;
		EogURIConverter *converter;

		basedir = g_get_current_dir ();
		base_file = g_file_new_for_path (basedir);
		g_free (basedir);

		dialog = eog_save_as_dialog_new (GTK_WINDOW (window),
						 images,
						 base_file);

		gtk_widget_show_all (dialog);

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
			g_object_unref (base_file);
			g_list_free (images);
			gtk_widget_destroy (dialog);

			return;
		}

		converter = eog_save_as_dialog_get_converter (dialog);

		g_assert (converter != NULL);

		priv->save_job = eog_job_save_as_new (images, converter, NULL);

		gtk_widget_destroy (dialog);

		g_object_unref (converter);
		g_object_unref (base_file);
	} else {
		/* n_images = 0 -- No Image selected */
		return;
	}

	g_signal_connect (priv->save_job,
			  "finished",
			  G_CALLBACK (eog_job_save_cb),
			  window);

	g_signal_connect (priv->save_job,
			  "progress",
			  G_CALLBACK (eog_job_save_progress_cb),
			  window);

	eog_job_scheduler_add_job (priv->save_job);
}

static void
eog_window_action_open_containing_folder (GSimpleAction *action,
					  GVariant      *variant,
					  gpointer       user_data)
{
	EogWindowPrivate *priv;
	GFile *file;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	g_return_if_fail (priv->image != NULL);

	file = eog_image_get_file (priv->image);

	g_return_if_fail (file != NULL);

	eog_util_show_file_in_filemanager (file,
					   GTK_WINDOW (user_data));
}

static void
eog_window_action_print (GSimpleAction *action,
			 GVariant      *variant,
			 gpointer       user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);

	eog_window_print (window);
}

/**
 * eog_window_get_remote_presenter:
 * @window: a #EogWindow
 *
 * Gets the remote presenter dialog. The widget will be built on the first call to this function.
 *
 * Returns: (transfer none): a #GtkWidget.
 */
GtkWidget*
eog_window_get_remote_presenter (EogWindow *window)
{
	EogWindowPrivate *priv;

	g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);

	priv = window->priv;

	if (priv->remote_presenter == NULL) {
		priv->remote_presenter =
			eog_remote_presenter_new (GTK_WINDOW (window),
						  EOG_THUMB_VIEW (priv->thumbview),
						  "win.go-next",
						  "win.go-previous");

		eog_remote_presenter_update (EOG_REMOTE_PRESENTER (priv->remote_presenter),
					     priv->image);
	}

	return priv->remote_presenter;
}

static void
eog_window_action_show_remote (GSimpleAction *action,
			       GVariant      *variant,
			       gpointer       user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);
	GtkWidget *remote_presenter;

	remote_presenter = eog_window_get_remote_presenter (window);
	gtk_widget_show (remote_presenter);
}

static void
eog_window_action_undo (GSimpleAction *action,
			GVariant      *variant,
			gpointer       user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	apply_transformation (EOG_WINDOW (user_data), NULL);
}

static void
eog_window_action_flip_horizontal (GSimpleAction *action,
				   GVariant      *variant,
				   gpointer       user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	apply_transformation (EOG_WINDOW (user_data),
			      eog_transform_flip_new (EOG_TRANSFORM_FLIP_HORIZONTAL));
}

static void
eog_window_action_flip_vertical (GSimpleAction *action,
				 GVariant      *variant,
				 gpointer      user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	apply_transformation (EOG_WINDOW (user_data),
			      eog_transform_flip_new (EOG_TRANSFORM_FLIP_VERTICAL));
}

static void
eog_window_action_rotate_90 (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	apply_transformation (EOG_WINDOW (user_data),
			      eog_transform_rotate_new (90));
}

static void
eog_window_action_rotate_270 (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	apply_transformation (EOG_WINDOW (user_data),
			      eog_transform_rotate_new (270));
}

static void
eog_window_action_wallpaper (GSimpleAction *action,
			     GVariant      *variant,
			     gpointer       user_data)
{
	EogWindow *window;
	EogWindowPrivate *priv;
	EogImage *image;
	g_autoptr(GFile) file = NULL;
	g_autofree gchar *filename = NULL;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);
	priv = window->priv;

	/* If currently copying an image to set it as wallpaper, return. */
	if (priv->copy_job != NULL)
		return;

	image = eog_thumb_view_get_first_selected_image (EOG_THUMB_VIEW (priv->thumbview));

	g_return_if_fail (EOG_IS_IMAGE (image));

	file = eog_image_get_file (image);

	filename = g_file_get_path (file);

	/* Currently only local files can be set as wallpaper */
	if (filename == NULL || !eog_util_file_is_persistent (file))
	{
		GList *files = NULL;

		g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

		priv->copy_file_cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar),
								    "copy_file_cid");
		gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar),
				    priv->copy_file_cid,
				    _("Saving image locally…"));

		files = g_list_append (files, eog_image_get_file (image));
		priv->copy_job = eog_job_copy_new (files, g_get_user_data_dir ());
		g_signal_connect (priv->copy_job,
				  "finished",
				  G_CALLBACK (eog_job_copy_cb),
				  window);
		g_signal_connect (priv->copy_job,
				  "progress",
				  G_CALLBACK (eog_job_progress_cb),
				  window);
		eog_job_scheduler_add_job (priv->copy_job);

		return;
	}

#ifdef HAVE_LIBPORTAL
	if (eog_util_is_running_inside_flatpak ()) {
		eog_util_set_wallpaper_with_portal (file, GTK_WINDOW (window));

		return;
	}
#endif

	eog_window_set_wallpaper (window, filename, NULL);

}

static gboolean
eog_window_all_images_trasheable (GList *images)
{
	GFile *file;
	GFileInfo *file_info;
	GList *iter;
	EogImage *image;
	gboolean can_trash = TRUE;

	for (iter = images; iter != NULL; iter = g_list_next (iter)) {
		image = (EogImage *) iter->data;
		file = eog_image_get_file (image);
		file_info = g_file_query_info (file,
					       G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH,
					       0, NULL, NULL);
		can_trash = g_file_info_get_attribute_boolean (file_info,
							       G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH);

		g_object_unref (file_info);
		g_object_unref (file);

		if (can_trash == FALSE)
			break;
	}

	return can_trash;
}

static gint
show_force_image_delete_confirm_dialog (EogWindow *window,
					GList     *images)
{
	static gboolean dont_ask_again_force_delete = FALSE;

	GtkWidget *dialog;
	GtkWidget *dont_ask_again_button;
	EogImage  *image;
	gchar     *prompt;
	guint      n_images;
	gint       response;

	/* assume agreement, if the user doesn't want to be asked and deletion is available */
	if (dont_ask_again_force_delete)
		return GTK_RESPONSE_OK;

	/* retrieve the selected images count */
	n_images = g_list_length (images);

	/* make the dialog prompt message */
	if (n_images == 1) {
		image = EOG_IMAGE (images->data);

		prompt = g_strdup_printf (_("Are you sure you want to remove\n“%s” permanently?"),
					  eog_image_get_caption (image));
	} else {
		prompt = g_strdup_printf (ngettext ("Are you sure you want to remove\n"
						    "the selected image permanently?",
						    "Are you sure you want to remove\n"
						    "the %d selected images permanently?",
						    n_images),
					  n_images);
	}

	/* create the dialog */
	dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (window),
						     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_MESSAGE_WARNING,
						     GTK_BUTTONS_NONE,
						     "<span weight=\"bold\" size=\"larger\">%s</span>",
						     prompt);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 GTK_RESPONSE_OK);

	/* add buttons to the dialog */
	if (n_images == 1) {
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Delete"), GTK_RESPONSE_OK);
	} else {
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Yes")   , GTK_RESPONSE_OK);
	}

	/* add 'dont ask again' button */
	dont_ask_again_button = gtk_check_button_new_with_mnemonic (_("Do _not ask again during this session"));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dont_ask_again_button),
				      FALSE);

	gtk_box_pack_end (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
			  dont_ask_again_button,
			  TRUE,
			  TRUE,
			  0);

	/* show dialog and get user response */
	gtk_widget_show_all (dialog);
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	/* only update the 'dont ask again' property if the user has accepted */
	if (response == GTK_RESPONSE_OK)
		dont_ask_again_force_delete = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dont_ask_again_button));

	/* free resources */
	g_free (prompt);
	gtk_widget_destroy (dialog);

	return response;
}

static gboolean
force_image_delete_real (EogImage  *image,
			 GError   **error)
{
	GFile     *file;
	GFileInfo *file_info;
	gboolean   can_delete;
	gboolean   result;

	g_return_val_if_fail (EOG_IS_IMAGE (image), FALSE);

	/* retrieve image file */
	file = eog_image_get_file (image);

	if (file == NULL) {
		g_set_error (error,
			     EOG_WINDOW_ERROR,
			     EOG_WINDOW_ERROR_IO,
			     _("Couldn’t retrieve image file"));

		return FALSE;
	}

	/* retrieve some image file information */
	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE,
				       0,
				       NULL,
				       NULL);

	if (file_info == NULL) {
		g_set_error (error,
			     EOG_WINDOW_ERROR,
			     EOG_WINDOW_ERROR_IO,
			     _("Couldn’t retrieve image file information"));

		/* free resources */
		g_object_unref (file);

		return FALSE;
	}

	/* check that image file can be deleted */
	can_delete = g_file_info_get_attribute_boolean (file_info,
							G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE);

	if (!can_delete) {
		g_set_error (error,
			     EOG_WINDOW_ERROR,
			     EOG_WINDOW_ERROR_IO,
			     _("Couldn’t delete file"));

		/* free resources */
		g_object_unref (file_info);
		g_object_unref (file);

		return FALSE;
	}

	/* delete image file */
	result = g_file_delete (file,
				NULL,
				error);

	/* free resources */
	g_object_unref (file_info);
        g_object_unref (file);

	return result;
}

static void
eog_window_force_image_delete (EogWindow *window,
			       GList     *images)
{
	GList    *item;
	gboolean  success;

	g_return_if_fail (EOG_WINDOW (window));

	/* force delete of each image of the list */
	for (item = images; item != NULL; item = item->next) {
		GError   *error;
		EogImage *image;

		error = NULL;
		image = EOG_IMAGE (item->data);

		success = force_image_delete_real (image, &error);

		if (!success) {
			GtkWidget *dialog;
			gchar     *header;

			/* set dialog error message */
			header = g_strdup_printf (_("Error on deleting image %s"),
						  eog_image_get_caption (image));

			/* create dialog */
			dialog = gtk_message_dialog_new (GTK_WINDOW (window),
							 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_OK,
							 "%s", header);

			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
								  "%s",
								  error->message);

			/* show dialog */
			gtk_dialog_run (GTK_DIALOG (dialog));

			/* free resources */
			gtk_widget_destroy (dialog);
			g_free (header);

			return;
		}

		/* remove image from store */
		eog_list_store_remove_image (window->priv->store, image);
	}

	/* free list */
	g_list_foreach (images, (GFunc) g_object_unref, NULL);
	g_list_free    (images);
}

static void
eog_window_action_delete (GSimpleAction *action,
			  GVariant      *variant,
			  gpointer       user_data)
{
	EogWindow *window;
	GList     *images;
	gint       result;

	window = EOG_WINDOW (user_data);
	images = eog_thumb_view_get_selected_images (EOG_THUMB_VIEW (window->priv->thumbview));
	if (G_LIKELY (g_list_length (images) > 0))
	{
		result = show_force_image_delete_confirm_dialog (window, images);

		if (result == GTK_RESPONSE_OK)
			eog_window_force_image_delete (window, images);
	}
}

static int
show_move_to_trash_confirm_dialog (EogWindow *window, GList *images, gboolean can_trash)
{
	GtkWidget *dlg;
	char *prompt;
	int response;
	int n_images;
	EogImage *image;
	static gboolean dontaskagain = FALSE;
	gboolean neverask = FALSE;
	GtkWidget* dontask_cbutton = NULL;

	/* Check if the user never wants to be bugged. */
	neverask = g_settings_get_boolean (window->priv->ui_settings,
					   EOG_CONF_UI_DISABLE_TRASH_CONFIRMATION);

	/* Assume agreement, if the user doesn't want to be
	 * asked and the trash is available */
	if (can_trash && (dontaskagain || neverask))
		return GTK_RESPONSE_OK;

	n_images = g_list_length (images);

	if (n_images == 1) {
		image = EOG_IMAGE (images->data);
		if (can_trash) {
			prompt = g_strdup_printf (_("Are you sure you want to move\n“%s” to the trash?"),
						  eog_image_get_caption (image));
		} else {
			prompt = g_strdup_printf (_("A trash for “%s” couldn’t be found. Do you want to remove "
						    "this image permanently?"), eog_image_get_caption (image));
		}
	} else {
		if (can_trash) {
			prompt = g_strdup_printf (ngettext("Are you sure you want to move\n"
							   "the selected image to the trash?",
							   "Are you sure you want to move\n"
							   "the %d selected images to the trash?", n_images), n_images);
		} else {
			prompt = g_strdup (_("Some of the selected images can’t be moved to the trash "
					     "and will be removed permanently. Are you sure you want "
					     "to proceed?"));
		}
	}

	dlg = gtk_message_dialog_new_with_markup (GTK_WINDOW (window),
						  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						  GTK_MESSAGE_WARNING,
						  GTK_BUTTONS_NONE,
						  "<span weight=\"bold\" size=\"larger\">%s</span>",
						  prompt);
	g_free (prompt);

	gtk_dialog_add_button (GTK_DIALOG (dlg), _("_Cancel"), GTK_RESPONSE_CANCEL);

	if (can_trash) {
		gtk_dialog_add_button (GTK_DIALOG (dlg), _("Move to _Trash"), GTK_RESPONSE_OK);

		dontask_cbutton = gtk_check_button_new_with_mnemonic (_("Do _not ask again during this session"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dontask_cbutton), FALSE);

		gtk_box_pack_end (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))), dontask_cbutton, TRUE, TRUE, 0);
	} else {
		if (n_images == 1) {
			gtk_dialog_add_button (GTK_DIALOG (dlg), _("_Delete"), GTK_RESPONSE_OK);
		} else {
			gtk_dialog_add_button (GTK_DIALOG (dlg), _("_Yes"), GTK_RESPONSE_OK);
		}
	}

	gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);
	gtk_window_set_title (GTK_WINDOW (dlg), "");
	gtk_widget_show_all (dlg);

	response = gtk_dialog_run (GTK_DIALOG (dlg));

	/* Only update the property if the user has accepted */
	if (can_trash && response == GTK_RESPONSE_OK)
		dontaskagain = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dontask_cbutton));

	/* The checkbutton is destroyed together with the dialog */
	gtk_widget_destroy (dlg);

	return response;
}

static gboolean
move_to_trash_real (EogImage *image, GError **error)
{
	GFile *file;
	GFileInfo *file_info;
	gboolean can_trash, result;

	g_return_val_if_fail (EOG_IS_IMAGE (image), FALSE);

	file = eog_image_get_file (image);
	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH,
				       0, NULL, NULL);
	if (file_info == NULL) {
		g_set_error (error,
			     EOG_WINDOW_ERROR,
			     EOG_WINDOW_ERROR_TRASH_NOT_FOUND,
			     _("Couldn’t access trash."));
		return FALSE;
	}

	can_trash = g_file_info_get_attribute_boolean (file_info,
						       G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH);
	g_object_unref (file_info);
	if (can_trash)
	{
		result = g_file_trash (file, NULL, NULL);
		if (result == FALSE) {
			g_set_error (error,
				     EOG_WINDOW_ERROR,
				     EOG_WINDOW_ERROR_TRASH_NOT_FOUND,
				     _("Couldn’t access trash."));
		}
	} else {
		result = g_file_delete (file, NULL, NULL);
		if (result == FALSE) {
			g_set_error (error,
				     EOG_WINDOW_ERROR,
				     EOG_WINDOW_ERROR_IO,
				     _("Couldn’t delete file"));
		}
	}

        g_object_unref (file);

	return result;
}

static void
eog_window_action_copy_image (GSimpleAction *action,
			      GVariant      *variant,
			      gpointer       user_data)
{
	GtkClipboard *clipboard;
	EogWindow *window;
	EogWindowPrivate *priv;
	EogImage *image;
	EogClipboardHandler *cbhandler;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);
	priv = window->priv;

	image = eog_thumb_view_get_first_selected_image (EOG_THUMB_VIEW (priv->thumbview));

	g_return_if_fail (EOG_IS_IMAGE (image));

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	cbhandler = eog_clipboard_handler_new (image);
	// cbhandler will self-destruct when it's not needed anymore
	eog_clipboard_handler_copy_to_clipboard (cbhandler, clipboard);
}

static void
eog_window_action_move_to_trash (GSimpleAction *action,
				 GVariant      *variant,
				 gpointer       user_data)
{
	GList *images;
	GList *it;
	EogWindowPrivate *priv;
	EogListStore *list;
	EogWindow *window;
	int response;
	int n_images;
	gboolean success;
	gboolean can_trash;

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

	can_trash = eog_window_all_images_trasheable (images);

	if (g_ascii_strcasecmp (g_action_get_name (G_ACTION (action)), "Delete") == 0 ||
	    can_trash == FALSE) {
		response = show_move_to_trash_confirm_dialog (window, images, can_trash);

		if (response != GTK_RESPONSE_OK) return;
	}

	/* FIXME: make a nice progress dialog */
	/* Do the work actually. First try to delete the image from the disk. If this
	 * is successful, remove it from the screen. Otherwise show error dialog.
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
						      "%s", header);

			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dlg),
								  "%s", error->message);

			gtk_dialog_run (GTK_DIALOG (dlg));

			gtk_widget_destroy (dlg);

			g_free (header);
		}
	}

	/* free list */
	g_list_foreach (images, (GFunc) g_object_unref, NULL);
	g_list_free (images);
}

static void
eog_window_action_toggle_fullscreen (GSimpleAction *action,
				     GVariant      *state,
				     gpointer       user_data)
{
	EogWindow *window;
	gboolean fullscreen;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	window = EOG_WINDOW (user_data);

	fullscreen = g_variant_get_boolean (state);

	if (fullscreen) {
		eog_window_run_fullscreen (window, FALSE);
	} else {
		eog_window_stop_fullscreen (window, FALSE);
	}
}

static void
eog_window_action_toggle_slideshow (GSimpleAction *action,
				    GVariant      *state,
				    gpointer       user_data)
{
	EogWindow *window;
	gboolean slideshow;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	window = EOG_WINDOW (user_data);

	slideshow = g_variant_get_boolean (state);

	if (slideshow) {
		eog_window_run_fullscreen (window, TRUE);
	} else {
		eog_window_stop_fullscreen (window, TRUE);
	}
}

static void
eog_window_action_pause_slideshow (GSimpleAction *action,
				   GVariant      *variant,
				   gpointer       user_data)
{
	EogWindow *window;
	gboolean slideshow;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	window = EOG_WINDOW (user_data);

	slideshow = window->priv->mode == EOG_WINDOW_MODE_SLIDESHOW;

	if (!slideshow && window->priv->mode != EOG_WINDOW_MODE_FULLSCREEN)
		return;

	eog_window_run_fullscreen (window, !slideshow);
}

static void
eog_window_action_zoom_in (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
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
eog_window_action_zoom_in_smooth (GSimpleAction *action,
                                  GVariant      *parameter,
                                  gpointer       user_data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	priv = EOG_WINDOW (user_data)->priv;

	if (priv->view) {
		eog_scroll_view_zoom_in (EOG_SCROLL_VIEW (priv->view), TRUE);
	}
}

static void
eog_window_action_zoom_out (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
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
eog_window_action_zoom_out_smooth (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	priv = EOG_WINDOW (user_data)->priv;

	if (priv->view) {
		eog_scroll_view_zoom_out (EOG_SCROLL_VIEW (priv->view), TRUE);
	}
}

static void
eog_window_action_zoom_normal (GSimpleAction *action,
			       GVariant      *variant,
			       gpointer       user_data)
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
eog_window_action_toggle_zoom_fit (GSimpleAction *action,
				   GVariant      *state,
				   gpointer       user_data)
{
	EogWindowPrivate *priv;
	EogZoomMode mode;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	priv = EOG_WINDOW (user_data)->priv;

	mode = g_variant_get_boolean (state)
	       ? EOG_ZOOM_MODE_SHRINK_TO_FIT : EOG_ZOOM_MODE_FREE;

	if (priv->view) {
		eog_scroll_view_set_zoom_mode (EOG_SCROLL_VIEW (priv->view),
					       mode);
	}
}

static void
eog_window_action_go_prev (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
	EogWindow *window;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	window = EOG_WINDOW (user_data);

	eog_thumb_view_select_single (EOG_THUMB_VIEW (window->priv->thumbview),
				      EOG_THUMB_VIEW_SELECT_LEFT);

	slideshow_set_timeout (window);
}

static void
eog_window_action_go_next (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
	EogWindow *window;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	window = EOG_WINDOW (user_data);

	eog_thumb_view_select_single (EOG_THUMB_VIEW (window->priv->thumbview),
				      EOG_THUMB_VIEW_SELECT_RIGHT);

	slideshow_set_timeout (window);
}

static void
eog_window_action_go_first (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
	EogWindow *window;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	window = EOG_WINDOW (user_data);

	eog_thumb_view_select_single (EOG_THUMB_VIEW (window->priv->thumbview),
				      EOG_THUMB_VIEW_SELECT_FIRST);

	slideshow_set_timeout (window);
}

static void
eog_window_action_go_last (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
	EogWindow *window;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	window = EOG_WINDOW (user_data);

	eog_thumb_view_select_single (EOG_THUMB_VIEW (window->priv->thumbview),
				      EOG_THUMB_VIEW_SELECT_LAST);

	slideshow_set_timeout (window);
}

static void
eog_window_action_go_random (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
	EogWindow *window;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	eog_debug (DEBUG_WINDOW);

	window = EOG_WINDOW (user_data);

	eog_thumb_view_select_single (EOG_THUMB_VIEW (window->priv->thumbview),
				      EOG_THUMB_VIEW_SELECT_RANDOM);

	slideshow_set_timeout (window);
}

static void
eog_window_action_set_zoom (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
	EogWindow *window;
	double zoom;

	g_return_if_fail (EOG_IS_WINDOW (user_data));
	g_return_if_fail (g_variant_is_of_type (parameter, G_VARIANT_TYPE_DOUBLE));

	window = EOG_WINDOW (user_data);

	zoom = g_variant_get_double (parameter);

	eog_debug_message (DEBUG_WINDOW, "Set zoom factor to %.4lf", zoom);

	if (window->priv->view) {
		eog_scroll_view_set_zoom (EOG_SCROLL_VIEW (window->priv->view),
		                          zoom);
	}
}

static void
readonly_state_handler (GSimpleAction *action,
                        GVariant      *value,
                        gpointer       user_data)
{
	g_warning ("The state of action \"%s\" is read-only! Ignoring request!",
	           g_action_get_name (G_ACTION (action)));
}

static const GActionEntry window_actions[] = {
	/* Stateless actions on the window. */
	{ "open",          eog_window_action_file_open },
	{ "open-with",     eog_window_action_open_with },
	{ "open-folder",   eog_window_action_open_containing_folder },
	{ "save",          eog_window_action_save },
	{ "save-as",       eog_window_action_save_as },
	{ "close",         eog_window_action_close_window },
	{ "close-all",     eog_window_action_close_all_windows },
	{ "print",         eog_window_action_print },
	{ "properties",    eog_window_action_toggle_properties },
	{ "show-remote",   eog_window_action_show_remote },
	{ "set-wallpaper", eog_window_action_wallpaper },
	{ "preferences",   eog_window_action_preferences },
	{ "manual",        eog_window_action_help },
	{ "about",         eog_window_action_about },

	/* Stateless actions on the image. */
	{ "go-previous",     eog_window_action_go_prev },
	{ "go-next",         eog_window_action_go_next },
	{ "go-first",        eog_window_action_go_first },
	{ "go-last",         eog_window_action_go_last },
	{ "go-random",       eog_window_action_go_random },
	{ "rotate-90",       eog_window_action_rotate_90 },
	{ "rotate-270",      eog_window_action_rotate_270 },
	{ "flip-horizontal", eog_window_action_flip_horizontal },
	{ "flip-vertical",   eog_window_action_flip_vertical },
	{ "move-trash",      eog_window_action_move_to_trash },
	{ "delete",          eog_window_action_delete },
	{ "copy",            eog_window_action_copy_image },
	{ "undo",            eog_window_action_undo },
	{ "zoom-in",         eog_window_action_zoom_in },
	{ "zoom-in-smooth",  eog_window_action_zoom_in_smooth },
	{ "zoom-out",        eog_window_action_zoom_out },
	{ "zoom-out-smooth", eog_window_action_zoom_out_smooth },
	{ "zoom-normal",     eog_window_action_zoom_normal },
	{ "zoom-set",        eog_window_action_set_zoom, "d" },

	/* Stateful actions. */
	{ "current-image",   NULL, NULL, "@(ii) (0, 0)", readonly_state_handler },
	{ "view-statusbar",  NULL, NULL, "true",  eog_window_action_show_hide_bar },
	{ "view-gallery",    NULL, NULL, "true",  eog_window_action_show_hide_bar },
	{ "view-sidebar",    NULL, NULL, "true",  eog_window_action_show_hide_bar },
	{ "view-slideshow",  NULL, NULL, "false", eog_window_action_toggle_slideshow },
	{ "view-fullscreen", NULL, NULL, "false", eog_window_action_toggle_fullscreen },
	{ "pause-slideshow", NULL, NULL, "false", eog_window_action_pause_slideshow },
	{ "toggle-zoom-fit", NULL, NULL, "true",  eog_window_action_toggle_zoom_fit },
};

static void
eog_window_ui_settings_changed_cb (GSettings *settings,
				   gchar     *key,
				   gpointer   user_data)
{
	GVariant *new_state = NULL;
	GVariant *old_state;
	GAction *action;

	g_return_if_fail (G_IS_ACTION (user_data));

	action = G_ACTION (user_data);

	new_state = g_settings_get_value (settings, key);
	g_assert (new_state != NULL);

	old_state = g_action_get_state (action);

	if (g_variant_get_boolean (new_state) != g_variant_get_boolean (old_state))
		g_action_change_state (action, new_state);

	g_variant_unref (new_state);
}

static void
eog_window_drag_data_received (GtkWidget *widget,
                               GdkDragContext *context,
                               gint x, gint y,
                               GtkSelectionData *selection_data,
                               guint info, guint time)
{
        GSList *file_list;
        EogWindow *window;
	GdkAtom target;
	GtkWidget *src;

	target = gtk_selection_data_get_target (selection_data);

        if (!gtk_targets_include_uri (&target, 1))
                return;

	/* if the request is from another process this will return NULL */
	src = gtk_drag_get_source_widget (context);

	/* if the drag request originates from the current eog instance, ignore
	   the request if the source window is the same as the dest window */
	if (src &&
	    gtk_widget_get_toplevel (src) == gtk_widget_get_toplevel (widget))
	{
		gdk_drag_status (context, 0, time);
		return;
	}

        if (gdk_drag_context_get_suggested_action (context) == GDK_ACTION_COPY)
        {
                window = EOG_WINDOW (widget);

                file_list = eog_util_parse_uri_string_list_to_file_list ((const gchar *) gtk_selection_data_get_data (selection_data));

		eog_window_open_file_list (window, file_list);
        }
}

static void
eog_window_set_drag_dest (EogWindow *window)
{
        gtk_drag_dest_set (GTK_WIDGET (window),
                           GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                           NULL, 0,
                           GDK_ACTION_COPY | GDK_ACTION_ASK);
	gtk_drag_dest_add_uri_targets (GTK_WIDGET (window));
}

static void
eog_window_sidebar_visibility_changed (GtkWidget *widget, EogWindow *window)
{
	GAction *action;
	GVariant *state;
	gboolean visible;
	gboolean active;

	visible = gtk_widget_get_visible (window->priv->sidebar);

	action = g_action_map_lookup_action (G_ACTION_MAP (window),
					      "view-sidebar");

	state = g_action_get_state (action);
	active = g_variant_get_boolean (state);
	if (active != visible)
		g_action_change_state (action,
				       g_variant_new_boolean (visible));
	g_variant_unref (state);

	/* Focus the image */
	if (!visible && window->priv->image != NULL)
		gtk_widget_grab_focus (window->priv->view);
}

static void
eog_window_sidebar_page_added (EogSidebar  *sidebar,
			       GtkWidget   *main_widget,
			       EogWindow   *window)
{
	if (eog_sidebar_get_n_pages (sidebar) == 1) {
		GAction *action;
		GVariant *state;
		gboolean show;

		action = g_action_map_lookup_action (G_ACTION_MAP (window),
						      "view-sidebar");

		g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

		state = g_action_get_state (action);
		show = g_variant_get_boolean (state);

		if (show)
			gtk_widget_show (GTK_WIDGET (sidebar));

		g_variant_unref (state);
	}
}

static void
eog_window_sidebar_page_removed (EogSidebar  *sidebar,
			         GtkWidget   *main_widget,
			         EogWindow   *window)
{
	if (eog_sidebar_is_empty (sidebar)) {
		GAction *action;

		gtk_widget_hide (GTK_WIDGET (sidebar));

		action = g_action_map_lookup_action (G_ACTION_MAP (window),
						      "view-sidebar");

		g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
	}
}

static void
eog_window_finish_saving (EogWindow *window)
{
	EogWindowPrivate *priv = window->priv;

	gtk_widget_set_sensitive (GTK_WIDGET (window), FALSE);

	do {
		gtk_main_iteration ();
	} while (priv->save_job != NULL);
}

static void
eog_window_view_rotation_changed_cb (EogScrollView *view,
				     gdouble        degrees,
				     EogWindow     *window)
{
	apply_transformation (window, eog_transform_rotate_new (degrees));
}

static void
eog_window_view_next_image_cb (EogScrollView *view,
			       EogWindow     *window)
{
	eog_window_action_go_next (NULL, NULL, window);
}

static void
eog_window_view_previous_image_cb (EogScrollView *view,
				   EogWindow     *window)
{
	eog_window_action_go_prev (NULL, NULL, window);
}

static void
eog_window_construct_ui (EogWindow *window)
{
	EogWindowPrivate *priv;

	GtkBuilder *builder;
	GAction *action = NULL;
	GObject *builder_object;

	GtkWidget *popup_menu;
	GtkWidget *hpaned;
	GtkWidget *zoom_entry;
	GtkWidget *menu_button;
	GtkWidget *menu_image;
	GtkWidget *fullscreen_button;

	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	priv->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (window), priv->box);
	gtk_widget_show (priv->box);

	priv->headerbar = hdy_header_bar_new ();
	hdy_header_bar_set_show_close_button (HDY_HEADER_BAR (priv->headerbar), TRUE);
	hdy_header_bar_set_title (HDY_HEADER_BAR (priv->headerbar),
				  g_get_application_name ());
	gtk_window_set_title (GTK_WINDOW (window), g_get_application_name ());
	gtk_box_pack_start (GTK_BOX (priv->box), priv->headerbar, FALSE, FALSE, 0);
	gtk_widget_show (priv->headerbar);

	menu_button = gtk_menu_button_new ();
	menu_image = gtk_image_new_from_icon_name ("open-menu-symbolic",
						   GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (menu_button), menu_image);

	builder = gtk_builder_new_from_resource ("/org/gnome/eog/ui/eog-gear-menu.ui");
	builder_object = gtk_builder_get_object (builder, "gear-menu");
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_button),
					G_MENU_MODEL (builder_object));

	hdy_header_bar_pack_end (HDY_HEADER_BAR (priv->headerbar), menu_button);
	gtk_widget_show (menu_button);

	action = G_ACTION (g_property_action_new ("toggle-gear-menu",
						  menu_button, "active"));
	g_action_map_add_action (G_ACTION_MAP (window), action);
	g_object_unref (action);

	fullscreen_button = gtk_button_new_from_icon_name ("view-fullscreen-symbolic",
							   GTK_ICON_SIZE_BUTTON);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (fullscreen_button),
					"win.view-fullscreen");
	gtk_widget_set_tooltip_text(fullscreen_button,
				    _("Show the current image in fullscreen mode"));
	hdy_header_bar_pack_end (HDY_HEADER_BAR (priv->headerbar), fullscreen_button);
	gtk_widget_show (fullscreen_button);

	priv->gear_menu_builder = builder;
	builder = NULL;

	priv->cbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (priv->box), priv->cbox, TRUE, TRUE, 0);
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

	hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);

	priv->sidebar = eog_sidebar_new ();
	/* The sidebar shouldn't be shown automatically on show_all(),
	   but only when the user actually wants it. */
	gtk_widget_set_no_show_all (priv->sidebar, TRUE);

	gtk_widget_set_size_request (priv->sidebar, 210, -1);

	g_signal_connect_after (priv->sidebar,
				"show",
				G_CALLBACK (eog_window_sidebar_visibility_changed),
				window);

	g_signal_connect_after (priv->sidebar,
				"hide",
				G_CALLBACK (eog_window_sidebar_visibility_changed),
				window);

	g_signal_connect_after (priv->sidebar,
				"page-added",
				G_CALLBACK (eog_window_sidebar_page_added),
				window);

	g_signal_connect_after (priv->sidebar,
				"page-removed",
				G_CALLBACK (eog_window_sidebar_page_removed),
				window);
	priv->overlay = gtk_overlay_new();

 	priv->view = eog_scroll_view_new ();
	g_signal_connect (priv->view,
			  "rotation-changed",
			  G_CALLBACK (eog_window_view_rotation_changed_cb),
			  window);
	g_signal_connect (priv->view,
			  "next-image",
			  G_CALLBACK (eog_window_view_next_image_cb),
			  window);
	g_signal_connect (priv->view,
			  "previous-image",
			  G_CALLBACK (eog_window_view_previous_image_cb),
			  window);

	priv->scroll_view_container = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add (GTK_CONTAINER(priv->scroll_view_container), priv->view);
	gtk_container_add (GTK_CONTAINER(priv->overlay), priv->scroll_view_container);

	eog_sidebar_add_page (EOG_SIDEBAR (priv->sidebar),
			      _("Properties"),
			      GTK_WIDGET (eog_metadata_sidebar_new (window)));

	gtk_widget_set_size_request (GTK_WIDGET (priv->view), 100, 100);
	g_signal_connect (G_OBJECT (priv->view),
			  "zoom_changed",
			  G_CALLBACK (view_zoom_changed_cb),
			  window);
	action = g_action_map_lookup_action (G_ACTION_MAP (window),
					     "toggle-zoom-fit");
	if (action != NULL) {
		/* Binding will be destroyed when the objects finalize */
		g_object_bind_property_full (priv->view, "zoom-mode",
					     action, "state",
					     G_BINDING_SYNC_CREATE,
					     _eog_zoom_shrink_to_boolean,
					     NULL, NULL, NULL);
	}
	g_settings_bind (priv->view_settings, EOG_CONF_VIEW_SCROLL_WHEEL_ZOOM,
			 priv->view, "scrollwheel-zoom", G_SETTINGS_BIND_GET);
	g_settings_bind (priv->view_settings, EOG_CONF_VIEW_ZOOM_MULTIPLIER,
			 priv->view, "zoom-multiplier", G_SETTINGS_BIND_GET);

	builder = gtk_builder_new_from_resource ("/org/gnome/eog/ui/popup-menus.ui");
	builder_object = gtk_builder_get_object (builder, "view-popup-menu");

	popup_menu = gtk_menu_new_from_model (G_MENU_MODEL (builder_object));

	eog_scroll_view_set_popup (EOG_SCROLL_VIEW (priv->view),
				   GTK_MENU (popup_menu));

	g_object_unref (popup_menu);

	gtk_paned_pack1 (GTK_PANED (hpaned),
			 priv->overlay,
			 TRUE,
			 FALSE);
	gtk_paned_pack2 (GTK_PANED (hpaned),
			 priv->sidebar,
			 FALSE,
			 FALSE);

	gtk_widget_show_all (hpaned);

	zoom_entry = eog_zoom_entry_new (EOG_SCROLL_VIEW (priv->view),
	                                 G_MENU (gtk_builder_get_object (builder,
	                                                                 "zoom-menu")));
	hdy_header_bar_pack_start (HDY_HEADER_BAR (priv->headerbar), zoom_entry);

	priv->thumbview = g_object_ref (eog_thumb_view_new ());

	/* giving shape to the view */
	gtk_icon_view_set_margin (GTK_ICON_VIEW (priv->thumbview), 4);
	gtk_icon_view_set_row_spacing (GTK_ICON_VIEW (priv->thumbview), 0);

	g_signal_connect (G_OBJECT (priv->thumbview), "selection_changed",
			  G_CALLBACK (handle_image_selection_changed_cb), window);

	priv->nav = eog_thumb_nav_new (priv->thumbview,
				       EOG_THUMB_NAV_MODE_ONE_ROW,
				       g_settings_get_boolean (priv->ui_settings,
							       EOG_CONF_UI_SCROLL_BUTTONS));

	// Bind the scroll buttons to their GSettings key
	g_settings_bind (priv->ui_settings, EOG_CONF_UI_SCROLL_BUTTONS,
			 priv->nav, "show-buttons", G_SETTINGS_BIND_GET);

	// Re-use the scroll view's popup menu model for the thumb view
	popup_menu = gtk_menu_new_from_model (G_MENU_MODEL(builder_object));
	eog_thumb_view_set_thumbnail_popup (EOG_THUMB_VIEW (priv->thumbview),
					    GTK_MENU (popup_menu));

	g_object_unref (popup_menu);
	g_clear_object (&builder);

	// Setup priv->layout
	eog_window_set_gallery_mode (window, priv->gallery_position, priv->gallery_resizable);

	g_settings_bind (priv->ui_settings, EOG_CONF_UI_IMAGE_GALLERY_POSITION,
			 window, "gallery-position", G_SETTINGS_BIND_GET);
	g_settings_bind (priv->ui_settings, EOG_CONF_UI_IMAGE_GALLERY_RESIZABLE,
			 window, "gallery-resizable", G_SETTINGS_BIND_GET);

	g_signal_connect (priv->lockdown_settings,
			  "changed::" EOG_CONF_DESKTOP_CAN_SAVE,
			  G_CALLBACK (eog_window_can_save_changed_cb), window);
	// Call callback once to have the value set
	eog_window_can_save_changed_cb (priv->lockdown_settings,
					EOG_CONF_DESKTOP_CAN_SAVE, window);

	update_action_groups_state (window);

	if ((priv->flags & EOG_STARTUP_FULLSCREEN) ||
	    (priv->flags & EOG_STARTUP_SLIDE_SHOW)) {
		eog_window_run_fullscreen (window, (priv->flags & EOG_STARTUP_SLIDE_SHOW));
	} else {
		priv->mode = EOG_WINDOW_MODE_NORMAL;
		update_ui_visibility (window);
	}

	eog_window_set_drag_dest (window);
}

static void
eog_window_init (EogWindow *window)
{
	GdkGeometry hints;
	EogWindowPrivate *priv;
	GAction* action;

	eog_debug (DEBUG_WINDOW);

	hints.min_width  = EOG_WINDOW_MIN_WIDTH;
	hints.min_height = EOG_WINDOW_MIN_HEIGHT;

	priv = window->priv = eog_window_get_instance_private (window);

	priv->fullscreen_settings = g_settings_new (EOG_CONF_FULLSCREEN);
	priv->ui_settings = g_settings_new (EOG_CONF_UI);
	priv->view_settings = g_settings_new (EOG_CONF_VIEW);
	priv->lockdown_settings = g_settings_new (EOG_CONF_DESKTOP_LOCKDOWN_SCHEMA);

	window->priv->file_list = NULL;
	window->priv->store = NULL;
	window->priv->image = NULL;

	window->priv->fullscreen_popup = NULL;
	window->priv->fullscreen_timeout_source = NULL;
	window->priv->slideshow_loop = FALSE;
	window->priv->slideshow_switch_timeout = 0;
	window->priv->slideshow_switch_source = NULL;
	window->priv->fullscreen_idle_inhibit_cookie = 0;

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

#if defined(HAVE_LCMS) && defined(GDK_WINDOWING_X11)
	window->priv->display_profile =
		eog_window_get_display_profile (GTK_WIDGET (window));
#endif

	window->priv->gallery_position = EOG_WINDOW_GALLERY_POS_BOTTOM;
	window->priv->gallery_resizable = FALSE;

	window->priv->save_disabled = FALSE;

	window->priv->page_setup = NULL;

	gtk_window_set_application (GTK_WINDOW (window), GTK_APPLICATION (EOG_APP));

	g_action_map_add_action_entries (G_ACTION_MAP (window),
	                                 window_actions, G_N_ELEMENTS (window_actions),
	                                 window);

	/* Creating a binding between the ui settings and the related GActions does
	 * not trigger the state changed handler since the state is updated directly
	 * via the "state" property. Requesting a state change via these callbacks,
	 * however, works. */
	g_signal_connect_object (priv->ui_settings, "changed::"EOG_CONF_UI_IMAGE_GALLERY,
				 G_CALLBACK (eog_window_ui_settings_changed_cb),
				 g_action_map_lookup_action (G_ACTION_MAP (window), "view-gallery"),
				 G_CONNECT_DEFAULT);

	g_signal_connect_object (priv->ui_settings, "changed::"EOG_CONF_UI_SIDEBAR,
				 G_CALLBACK (eog_window_ui_settings_changed_cb),
				 g_action_map_lookup_action (G_ACTION_MAP (window), "view-sidebar"),
				 G_CONNECT_DEFAULT);

	g_signal_connect_object (priv->ui_settings, "changed::"EOG_CONF_UI_STATUSBAR,
				 G_CALLBACK (eog_window_ui_settings_changed_cb),
				 g_action_map_lookup_action (G_ACTION_MAP (window), "view-statusbar"),
				 G_CONNECT_DEFAULT);

	action = g_action_map_lookup_action (G_ACTION_MAP (window),
	                                     "current-image");
	if (G_LIKELY (action != NULL))
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

	if (g_strcmp0 (PROFILE, "") != 0) {
		GtkStyleContext *style_context;

		style_context = gtk_widget_get_style_context (GTK_WIDGET (window));
		gtk_style_context_add_class (style_context, "devel");
	}
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

	peas_engine_garbage_collect (PEAS_ENGINE (EOG_APP->priv->plugin_engine));

	if (priv->extensions != NULL) {
		g_object_unref (priv->extensions);
		priv->extensions = NULL;
		peas_engine_garbage_collect (PEAS_ENGINE (EOG_APP->priv->plugin_engine));
	}

	if (priv->store != NULL) {
		g_signal_handlers_disconnect_by_func (priv->store,
					      eog_window_list_store_image_added,
					      window);
		g_signal_handlers_disconnect_by_func (priv->store,
					    eog_window_list_store_image_removed,
					    window);
		g_object_unref (priv->store);
		priv->store = NULL;
	}

	if (priv->image != NULL) {
	  	g_signal_handlers_disconnect_by_func (priv->image,
						      image_thumb_changed_cb,
						      window);
		g_signal_handlers_disconnect_by_func (priv->image,
						      image_file_changed_cb,
						      window);
		g_object_unref (priv->image);
		priv->image = NULL;
	}

	fullscreen_clear_timeout (window);

	if (window->priv->fullscreen_popup != NULL) {
		gtk_widget_destroy (priv->fullscreen_popup);
		priv->fullscreen_popup = NULL;
	}

	slideshow_clear_timeout (window);
	eog_window_uninhibit_screensaver (window);

	eog_window_clear_load_job (window);

	eog_window_clear_transform_job (window);

	if (priv->view_settings) {
		g_object_unref (priv->view_settings);
		priv->view_settings = NULL;
	}

	if (priv->ui_settings) {
		g_object_unref (priv->ui_settings);
		priv->ui_settings = NULL;
	}

	if (priv->fullscreen_settings) {
		g_object_unref (priv->fullscreen_settings);
		priv->fullscreen_settings = NULL;
	}

	if (priv->lockdown_settings) {
		g_object_unref (priv->lockdown_settings);
		priv->lockdown_settings = NULL;
	}

	if (priv->file_list != NULL) {
		g_slist_foreach (priv->file_list, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->file_list);
		priv->file_list = NULL;
	}

#ifdef HAVE_LCMS
	if (priv->display_profile != NULL) {
		cmsCloseProfile (priv->display_profile);
		priv->display_profile = NULL;
	}
#endif

	if (priv->last_save_as_folder != NULL) {
		g_object_unref (priv->last_save_as_folder);
		priv->last_save_as_folder = NULL;
	}

	if (priv->page_setup != NULL) {
		g_object_unref (priv->page_setup);
		priv->page_setup = NULL;
	}

	if (priv->thumbview)
	{
		/* Disconnect so we don't get any unwanted callbacks
		 * when the thumb view is disposed. */
		g_signal_handlers_disconnect_by_func (priv->thumbview,
		                 G_CALLBACK (handle_image_selection_changed_cb),
		                 window);
		g_clear_object (&priv->thumbview);
	}

	g_clear_object (&priv->gear_menu_builder);

	peas_engine_garbage_collect (PEAS_ENGINE (EOG_APP->priv->plugin_engine));

	G_OBJECT_CLASS (eog_window_parent_class)->dispose (object);
}

static gint
eog_window_delete (GtkWidget *widget, GdkEventAny *event)
{
	EogWindow *window;
	EogWindowPrivate *priv;

	g_return_val_if_fail (EOG_IS_WINDOW (widget), FALSE);

	window = EOG_WINDOW (widget);
	priv = window->priv;

	if (priv->save_job != NULL) {
		eog_window_finish_saving (window);
	}

	if (eog_window_unsaved_images_confirm (window)) {
		return TRUE;
	}

	gtk_widget_destroy (widget);

	return TRUE;
}

/*
 * Imported from gedit-window.c:
 * GtkWindow catches keybindings for the menu items _before_ passing them to
 * the focused widget. This is unfortunate and means that trying to use
 * Return to activate a menu entry will instead skip to the next image.
 * Here we override GtkWindow's handler to do the same things that it
 * does, but in the opposite order and then we chain up to the grand
 * parent handler, skipping gtk_window_key_press_event.
 */
static gint
eog_window_key_press (GtkWidget *widget, GdkEventKey *event)
{
	gint result = FALSE;
	gboolean handle_selection = FALSE;
	GdkModifierType modifiers;

	/* handle focus widget key events */
	if (!handle_selection) {
		handle_selection = gtk_window_propagate_key_event (GTK_WINDOW (widget), event);
	}

	/* handle mnemonics and accelerators */
	if (!handle_selection) {
		handle_selection = gtk_window_activate_key (GTK_WINDOW (widget), event);
	}

	if (handle_selection)
		return TRUE;

	modifiers = gtk_accelerator_get_default_mod_mask ();

	switch (event->keyval) {
	case GDK_KEY_Escape:
		if (EOG_WINDOW (widget)->priv->mode == EOG_WINDOW_MODE_FULLSCREEN) {
			eog_window_stop_fullscreen (EOG_WINDOW (widget), FALSE);
		} else if (EOG_WINDOW (widget)->priv->mode == EOG_WINDOW_MODE_SLIDESHOW) {
			eog_window_stop_fullscreen (EOG_WINDOW (widget), TRUE);
		} else {
			eog_window_action_close_window (NULL, NULL, EOG_WINDOW (widget));
		}
		return TRUE;
		break;
	case GDK_KEY_Page_Up:
		if ((event->state & modifiers) == 0) {
			if (!eog_scroll_view_is_image_movable (EOG_SCROLL_VIEW (EOG_WINDOW (widget)->priv->view))) {
				if (!gtk_widget_get_visible (EOG_WINDOW (widget)->priv->nav)) {
					/* If the iconview is not visible skip to the
					 * previous image manually as it won't handle
					 * the keypress then. */
					eog_window_action_go_prev (NULL, NULL, EOG_WINDOW (widget));
					result = TRUE;
				} else
					handle_selection = TRUE;
			}
		}
		break;
	case GDK_KEY_Page_Down:
		if ((event->state & modifiers) == 0) {
			if (!eog_scroll_view_is_image_movable (EOG_SCROLL_VIEW (EOG_WINDOW (widget)->priv->view))) {
				if (!gtk_widget_get_visible (EOG_WINDOW (widget)->priv->nav)) {
					/* If the iconview is not visible skip to the
					 * next image manually as it won't handle
					 * the keypress then. */
					eog_window_action_go_next (NULL, NULL, EOG_WINDOW (widget));
					result = TRUE;
				} else
					handle_selection = TRUE;
			}
		}
		break;
	}

	if (handle_selection && !result) {
		gtk_widget_grab_focus (GTK_WIDGET (EOG_WINDOW (widget)->priv->thumbview));

		result = gtk_widget_event (GTK_WIDGET (EOG_WINDOW (widget)->priv->thumbview),
					   (GdkEvent *) event);
	}

	/* If we still haven't handled the event, give the scrolled window a chance to do it.  */
	if (!result &&
		gtk_widget_get_realized (GTK_WIDGET (EOG_WINDOW (widget)->priv->scroll_view_container))) {
			result = gtk_widget_event (GTK_WIDGET (EOG_WINDOW (widget)->priv->scroll_view_container),
						   (GdkEvent *) event);
	}

	if (!result && GTK_WIDGET_CLASS (eog_window_parent_class)->key_press_event) {
		result = GTK_WIDGET_CLASS (eog_window_parent_class)->key_press_event (widget, event);
	}

	return result;
}

static gint
eog_window_button_press (GtkWidget *widget, GdkEventButton *event)
{
	EogWindow *window = EOG_WINDOW (widget);
	gint result = FALSE;

	/* We currently can't tell whether the old button codes (6, 7) are
	 * still in use. So we keep them in addition to the new ones (8, 9)
	 */
	if (event->type == GDK_BUTTON_PRESS) {
		switch (event->button) {
		case 6:
		case 8:
			eog_thumb_view_select_single (EOG_THUMB_VIEW (window->priv->thumbview),
						      EOG_THUMB_VIEW_SELECT_LEFT);
			result = TRUE;
		       	break;
		case 7:
		case 9:
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
eog_window_focus_out_event (GtkWidget *widget, GdkEventFocus *event)
{
	EogWindow *window = EOG_WINDOW (widget);
	EogWindowPrivate *priv = window->priv;
	gboolean fullscreen;

	eog_debug (DEBUG_WINDOW);

	fullscreen = priv->mode == EOG_WINDOW_MODE_FULLSCREEN ||
		     priv->mode == EOG_WINDOW_MODE_SLIDESHOW;

	if (fullscreen) {
		gtk_widget_hide (priv->fullscreen_popup);
	}

	return GTK_WIDGET_CLASS (eog_window_parent_class)->focus_out_event (widget, event);
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
	case PROP_GALLERY_POS:
		eog_window_set_gallery_mode (window, g_value_get_enum (value),
					     priv->gallery_resizable);
		break;
	case PROP_GALLERY_RESIZABLE:
		eog_window_set_gallery_mode (window, priv->gallery_position,
					     g_value_get_boolean (value));
		break;
	case PROP_STARTUP_FLAGS:
		priv->flags = g_value_get_flags (value);
		break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
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
	case PROP_GALLERY_POS:
		g_value_set_enum (value, priv->gallery_position);
		break;
	case PROP_GALLERY_RESIZABLE:
		g_value_set_boolean (value, priv->gallery_resizable);
		break;
	case PROP_STARTUP_FLAGS:
		g_value_set_flags (value, priv->flags);
		break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
on_extension_added (PeasExtensionSet *set,
		    PeasPluginInfo   *info,
		    PeasExtension    *exten,
		    GtkWindow        *window)
{
	peas_extension_call (exten, "activate", window);
}

static void
on_extension_removed (PeasExtensionSet *set,
		      PeasPluginInfo   *info,
		      PeasExtension    *exten,
		      GtkWindow        *window)
{
	peas_extension_call (exten, "deactivate", window);
}

static GObject *
eog_window_constructor (GType type,
			guint n_construct_properties,
			GObjectConstructParam *construct_params)
{
	GObject *object;
	EogWindowPrivate *priv;

	object = G_OBJECT_CLASS (eog_window_parent_class)->constructor
			(type, n_construct_properties, construct_params);

	priv = EOG_WINDOW (object)->priv;

	eog_window_construct_ui (EOG_WINDOW (object));

	priv->extensions = peas_extension_set_new (PEAS_ENGINE (EOG_APP->priv->plugin_engine),
						   EOG_TYPE_WINDOW_ACTIVATABLE,
						   "window",
						   EOG_WINDOW (object), NULL);
	peas_extension_set_call (priv->extensions, "activate");
	g_signal_connect (priv->extensions, "extension-added",
			  G_CALLBACK (on_extension_added), object);
	g_signal_connect (priv->extensions, "extension-removed",
			  G_CALLBACK (on_extension_removed), object);

	return object;
}

static void
eog_window_class_init (EogWindowClass *class)
{
	GObjectClass *g_object_class = (GObjectClass *) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) class;

	g_object_class->constructor = eog_window_constructor;
	g_object_class->dispose = eog_window_dispose;
	g_object_class->set_property = eog_window_set_property;
	g_object_class->get_property = eog_window_get_property;

	widget_class->delete_event = eog_window_delete;
	widget_class->key_press_event = eog_window_key_press;
	widget_class->button_press_event = eog_window_button_press;
	widget_class->drag_data_received = eog_window_drag_data_received;
	widget_class->focus_out_event = eog_window_focus_out_event;

/**
 * EogWindow:gallery-position:
 *
 * Determines the position of the image gallery in the window
 * relative to the image.
 */
	g_object_class_install_property (
		g_object_class, PROP_GALLERY_POS,
		g_param_spec_enum ("gallery-position", NULL, NULL,
				   EOG_TYPE_WINDOW_GALLERY_POS,
				   EOG_WINDOW_GALLERY_POS_BOTTOM,
				   G_PARAM_READWRITE | G_PARAM_STATIC_NAME));

/**
 * EogWindow:gallery-resizable:
 *
 * If %TRUE the gallery will be resizable by the user otherwise it will be
 * in single column/row mode.
 */
	g_object_class_install_property (
		g_object_class, PROP_GALLERY_RESIZABLE,
		g_param_spec_boolean ("gallery-resizable", NULL, NULL, FALSE,
				      G_PARAM_READWRITE | G_PARAM_STATIC_NAME));

/**
 * EogWindow:startup-flags:
 *
 * A bitwise OR of #EogStartupFlags elements, indicating how the window
 * should behave upon creation.
 */
	g_object_class_install_property (g_object_class,
					 PROP_STARTUP_FLAGS,
					 g_param_spec_flags ("startup-flags",
							     NULL,
							     NULL,
							     EOG_TYPE_STARTUP_FLAGS,
					 		     0,
					 		     G_PARAM_READWRITE |
							     G_PARAM_CONSTRUCT_ONLY));

/**
 * EogWindow::prepared:
 * @window: the object which received the signal.
 *
 * The #EogWindow::prepared signal is emitted when the @window is ready
 * to be shown.
 */
	signals [SIGNAL_PREPARED] =
		g_signal_new ("prepared",
			      EOG_TYPE_WINDOW,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogWindowClass, prepared),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

/**
 * eog_window_new:
 * @flags: the initialization parameters for the new window.
 *
 *
 * Creates a new and empty #EogWindow. Use @flags to indicate
 * if the window should be initialized fullscreen, in slideshow mode,
 * and/or without the thumbnails gallery visible. See #EogStartupFlags.
 *
 * Returns: a newly created #EogWindow.
 **/
GtkWidget*
eog_window_new (EogStartupFlags flags)
{
	EogWindow *window;

	eog_debug (DEBUG_WINDOW);

	window = EOG_WINDOW (g_object_new (EOG_TYPE_WINDOW,
					   "type", GTK_WINDOW_TOPLEVEL,
	                                   "application", EOG_APP,
					   "startup-flags", flags,
					   NULL));

	return GTK_WIDGET (window);
}

static void
eog_window_list_store_image_added (GtkTreeModel *tree_model,
                                   GtkTreePath  *path,
                                   GtkTreeIter  *iter,
                                   gpointer      user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);

	update_image_pos (window);
	update_action_groups_state (window);
}

static void
eog_window_list_store_image_removed (GtkTreeModel *tree_model,
                                     GtkTreePath  *path,
                                     gpointer      user_data)
{
	EogWindow *window = EOG_WINDOW (user_data);
	EogWindowPrivate *priv = window->priv;
	gint n_images = eog_list_store_length (priv->store);

	if (eog_thumb_view_get_n_selected (EOG_THUMB_VIEW (priv->thumbview)) == 0
	    && n_images > 0) {
		gint pos = MIN (gtk_tree_path_get_indices (path)[0],
				n_images - 1);
		EogImage *image = eog_list_store_get_image_by_pos (priv->store, pos);

		if (image != NULL) {
			eog_thumb_view_set_current_image (EOG_THUMB_VIEW (priv->thumbview),
							  image, TRUE);
			g_object_unref (image);
		}
	} else if (!n_images) {
		eog_window_clear_load_job (window);
	}

	update_image_pos (window);
	update_action_groups_state (window);
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
	EogImage *image;
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
	if (g_settings_get_boolean (priv->view_settings, EOG_CONF_VIEW_AUTOROTATE)) {
		for (i = 0; i < n_images; i++) {
			image = eog_list_store_get_image_by_pos (priv->store, i);
			eog_image_autorotate (image);
			g_object_unref (image);
		}
	}
#endif

	eog_thumb_view_set_model (EOG_THUMB_VIEW (priv->thumbview), priv->store);

	g_signal_connect (G_OBJECT (priv->store),
			  "row-inserted",
			  G_CALLBACK (eog_window_list_store_image_added),
			  window);

	g_signal_connect (G_OBJECT (priv->store),
			  "row-deleted",
			  G_CALLBACK (eog_window_list_store_image_removed),
			  window);

	if (n_images == 0) {
		gint n_files;

		/* Avoid starting up fullscreen with an empty model as
		 * fullscreen controls might end up disabled */
		if (priv->status == EOG_WINDOW_STATUS_INIT &&
		    (priv->mode == EOG_WINDOW_MODE_FULLSCREEN
		     || priv->mode == EOG_WINDOW_MODE_SLIDESHOW)) {
			eog_window_stop_fullscreen (window,
				priv->mode == EOG_WINDOW_MODE_SLIDESHOW);
		}
		priv->status = EOG_WINDOW_STATUS_NORMAL;
		update_action_groups_state (window);

		n_files = g_slist_length (priv->file_list);

		if (n_files > 0) {
			GtkWidget *message_area;
			GFile *file = NULL;

			if (n_files == 1) {
				file = (GFile *) priv->file_list->data;
			}

			message_area = eog_no_images_error_message_area_new (file);

			eog_window_set_message_area (window, message_area);

			gtk_widget_show (message_area);
		}

		g_signal_emit (window, signals[SIGNAL_PREPARED], 0);
	}
}

/**
 * eog_window_open_file_list:
 * @window: An #EogWindow.
 * @file_list: (element-type GFile): A %NULL-terminated list of #GFile's.
 *
 * Opens a list of files, adding them to the gallery in @window.
 * Files will be checked to be readable and later filtered according
 * with eog_list_store_add_files().
 **/
void
eog_window_open_file_list (EogWindow *window, GSList *file_list)
{
	EogJob *job;

	eog_debug (DEBUG_WINDOW);

	window->priv->status = EOG_WINDOW_STATUS_INIT;

	/* Free the list to avoid memory leak
	 * when using flag EOG_STARTUP_SINGLE_WINDOW
	 */
	if (window->priv->file_list != NULL) {
		g_slist_foreach (window->priv->file_list, (GFunc) g_object_unref, NULL);
		g_slist_free (window->priv->file_list);
	}

	g_slist_foreach (file_list, (GFunc) g_object_ref, NULL);
	window->priv->file_list = file_list;

	job = eog_job_model_new (file_list);

	g_signal_connect (job,
			  "finished",
			  G_CALLBACK (eog_job_model_cb),
			  window);

	eog_job_scheduler_add_job (job);
	g_object_unref (job);
}

/**
 * eog_window_get_gear_menu_section:
 * @window: an #EogWindow.
 * @id: the ID for the menu section to look up
 *
 * Return value: (transfer none): a #GMenu or %NULL on failure
 **/
GMenu *
eog_window_get_gear_menu_section (EogWindow *window, const gchar *id)
{
	GObject *object;
	g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);

	object = gtk_builder_get_object (window->priv->gear_menu_builder, id);
	if (object == NULL || !G_IS_MENU (object))
		return NULL;

	return G_MENU (object);
}

/**
 * eog_window_get_mode:
 * @window: An #EogWindow.
 *
 * Gets the mode of @window. See #EogWindowMode for details.
 *
 * Returns: An #EogWindowMode.
 **/
EogWindowMode
eog_window_get_mode (EogWindow *window)
{
        g_return_val_if_fail (EOG_IS_WINDOW (window), EOG_WINDOW_MODE_UNKNOWN);

	return window->priv->mode;
}

/**
 * eog_window_set_mode:
 * @window: an #EogWindow.
 * @mode: an #EogWindowMode value.
 *
 * Changes the mode of @window to normal, fullscreen, or slideshow.
 * See #EogWindowMode for details.
 **/
void
eog_window_set_mode (EogWindow *window, EogWindowMode mode)
{
        g_return_if_fail (EOG_IS_WINDOW (window));

	if (window->priv->mode == mode)
		return;

	switch (mode) {
	case EOG_WINDOW_MODE_NORMAL:
		eog_window_stop_fullscreen (window,
					    window->priv->mode == EOG_WINDOW_MODE_SLIDESHOW);
		break;
	case EOG_WINDOW_MODE_FULLSCREEN:
		eog_window_run_fullscreen (window, FALSE);
		break;
	case EOG_WINDOW_MODE_SLIDESHOW:
		eog_window_run_fullscreen (window, TRUE);
		break;
	case EOG_WINDOW_MODE_UNKNOWN:
		break;
	}
}

/**
 * eog_window_get_store:
 * @window: An #EogWindow.
 *
 * Gets the #EogListStore that contains the images in the gallery
 * of @window.
 *
 * Returns: (transfer none): an #EogListStore.
 **/
EogListStore *
eog_window_get_store (EogWindow *window)
{
        g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);

	return EOG_LIST_STORE (window->priv->store);
}

/**
 * eog_window_get_view:
 * @window: An #EogWindow.
 *
 * Gets the #EogScrollView in the window.
 *
 * Returns: (transfer none): the #EogScrollView.
 **/
GtkWidget *
eog_window_get_view (EogWindow *window)
{
        g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);

       return window->priv->view;
}

/**
 * eog_window_get_sidebar:
 * @window: An #EogWindow.
 *
 * Gets the sidebar widget of @window.
 *
 * Returns: (transfer none): the #EogSidebar.
 **/
GtkWidget *
eog_window_get_sidebar (EogWindow *window)
{
        g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);

	return window->priv->sidebar;
}

/**
 * eog_window_get_thumb_view:
 * @window: an #EogWindow.
 *
 * Gets the thumbnails view in @window.
 *
 * Returns: (transfer none): an #EogThumbView.
 **/
GtkWidget *
eog_window_get_thumb_view (EogWindow *window)
{
        g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);

	return window->priv->thumbview;
}

/**
 * eog_window_get_thumb_nav:
 * @window: an #EogWindow.
 *
 * Gets the thumbnails navigation pane in @window.
 *
 * Returns: (transfer none): an #EogThumbNav.
 **/
GtkWidget *
eog_window_get_thumb_nav (EogWindow *window)
{
        g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);

	return window->priv->nav;
}

/**
 * eog_window_get_statusbar:
 * @window: an #EogWindow.
 *
 * Gets the statusbar in @window.
 *
 * Returns: (transfer none): a #EogStatusbar.
 **/
GtkWidget *
eog_window_get_statusbar (EogWindow *window)
{
        g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);

	return window->priv->statusbar;
}

/**
 * eog_window_get_image:
 * @window: an #EogWindow.
 *
 * Gets the image currently displayed in @window or %NULL if
 * no image is being displayed.
 *
 * Returns: (transfer none): an #EogImage.
 **/
EogImage *
eog_window_get_image (EogWindow *window)
{
        g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);

	return window->priv->image;
}

/**
 * eog_window_is_empty:
 * @window: an #EogWindow.
 *
 * Tells whether @window is currently empty or not.
 *
 * Returns: %TRUE if @window has no images, %FALSE otherwise.
 **/
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

void
eog_window_reload_image (EogWindow *window)
{
	GtkWidget *view;

	g_return_if_fail (EOG_IS_WINDOW (window));

	if (window->priv->image == NULL)
		return;

	g_object_unref (window->priv->image);
	window->priv->image = NULL;

	view = eog_window_get_view (window);
	eog_scroll_view_set_image (EOG_SCROLL_VIEW (view), NULL);

	eog_thumb_view_select_single (EOG_THUMB_VIEW (window->priv->thumbview),
				      EOG_THUMB_VIEW_SELECT_CURRENT);
}

gboolean
eog_window_is_not_initializing (const EogWindow *window)
{
	g_return_val_if_fail (EOG_IS_WINDOW (window), FALSE);

	return window->priv->status != EOG_WINDOW_STATUS_INIT;
}

void
eog_window_show_about_dialog (EogWindow *window)
{
	g_return_if_fail (EOG_IS_WINDOW (window));

	static const char *authors[] = {
		"Felix Riemann <friemann@gnome.org> (maintainer)",
		"",
		"Claudio Saavedra <csaavedra@igalia.com>",
		"Lucas Rocha <lucasr@gnome.org>",
		"Tim Gerla <tim+gnomebugs@gerla.net>",
		"Philip Van Hoof <pvanhoof@gnome.org>",
                "Paolo Borelli <pborelli@katamail.com>",
		"Jens Finke <jens@triq.net>",
		"Martin Baulig <martin@home-of-linux.org>",
		"Arik Devens <arik@gnome.org>",
		"Michael Meeks <mmeeks@gnu.org>",
		"Federico Mena-Quintero <federico@gnome.org>",
		"Lutz M\xc3\xbcller <urc8@rz.uni-karlsruhe.de>",
		NULL
	};

	static const char *documenters[] = {
		"Eliot Landrum <eliot@landrum.cx>",
		"Federico Mena-Quintero <federico@gnome.org>",
		"Sun GNOME Documentation Team <gdocteam@sun.com>",
		"Tiffany Antopolski <tiffany@antopolski.com>",
		NULL
	};

	gtk_show_about_dialog (GTK_WINDOW (window),
			       "program-name", _("Eye of GNOME"),
			       "version", VERSION,
			       "copyright", "Copyright \xc2\xa9 2000-2010 Free Software Foundation, Inc.",
			       "comments",_("Image viewer for GNOME"),
			       "authors", authors,
			       "documenters", documenters,
			       "translator-credits", _("translator-credits"),
			       "website", "https://gitlab.gnome.org/GNOME/eog/",
			       "logo-icon-name", APPLICATION_ID,
			       "wrap-license", TRUE,
			       "license-type", GTK_LICENSE_GPL_2_0,
			       NULL);
}

void
eog_window_show_preferences_dialog (EogWindow *window)
{
	GtkWidget *pref_dlg;

	g_return_if_fail (window != NULL);

	pref_dlg = eog_preferences_dialog_get_instance (GTK_WINDOW (window));

	gtk_widget_show (pref_dlg);
}

void
eog_window_close (EogWindow *window)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	if (priv->save_job != NULL) {
		eog_window_finish_saving (window);
	}

	if (!eog_window_unsaved_images_confirm (window)) {
		gtk_widget_destroy (GTK_WIDGET (window));
	}
}
