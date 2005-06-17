/* Eye of Gnome image viewer - main window widget
 *
 * Copyright (C) 2000-2004 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
 *         Jens Finke <jens@gnome.org>
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

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <config.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <gnome.h>
#include <string.h>
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#include <gconf/gconf-client.h>
#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-window-icon.h>
#include <libgnomevfs/gnome-vfs.h>
#include "eog-window.h"
#include "util.h"
#include "zoom.h"
#include "eog-file-selection.h"
#include "eog-preferences.h"
#include "libeog-marshal.h"
#include "egg-recent.h"
#include "eog-config-keys.h"
#include "eog-scroll-view.h"
#include "eog-wrap-list.h"
#include "eog-vertical-splitter.h"
#include "eog-horizontal-splitter.h"
#include "eog-info-view.h"
#include "eog-full-screen.h"
#include "eog-save-dialog-helper.h"
#include "eog-image-save-info.h"
#include "eog-hig-dialog.h"
#include "eog-uri-converter.h"
#include "eog-save-as-dialog-helper.h"
#include "eog-pixbuf-util.h"
#include "eog-job-manager.h"

#ifdef G_OS_WIN32
#define getgid() 0
#define getppid() 0
#define gethostname(buf,size) strncpy(buf,"localhost",size)
#endif

/* Default size for windows */

#define DEFAULT_WINDOW_WIDTH  310
#define DEFAULT_WINDOW_HEIGHT 280

#define RECENT_FILES_GROUP         "Eye of Gnome"
#define EOG_WINDOW_DND_POPUP_PATH  "/popups/dragndrop"

#define EOG_STOCK_ROTATE_90    "eog-stock-rotate-90"
#define EOG_STOCK_ROTATE_270   "eog-stock-rotate-270"
#define EOG_STOCK_ROTATE_180   "eog-stock-rotate-180"
#define EOG_STOCK_FLIP_HORIZONTAL "eog-stock-flip-horizontal"
#define EOG_STOCK_FLIP_VERTICAL   "eog-stock-flip-vertical"

#define NO_DEBUG
#define SAVE_DEBUG

/* Private part of the Window structure */
struct _EogWindowPrivate {
	/* Our GConf client */
	GConfClient *client;

	/* Images we are displaying */
	EogImageList        *image_list;
	EogImage            *displayed_image;

	/* ui/widget stuff */
	GtkUIManager        *ui_mgr;
	GtkWidget           *box;
	GtkWidget           *hpane;
	GtkWidget           *vpane;
	GtkWidget           *scroll_view;
	GtkWidget           *wraplist;
	GtkWidget           *info_view;
	GtkWidget           *statusbar;
	GtkWidget           *n_img_label;

	/* available action groups */
	GtkActionGroup      *actions_window;
	GtkActionGroup      *actions_image;
	GtkActionGroup      *actions_collection;

	/* window geometry */
	guint save_geometry_timeout_id;
	char *last_geometry;	
	int desired_width;
	int desired_height;

	/* recent files stuff */
	EggRecentModel      *recent_model;
	EggRecentViewGtk    *recent_view;

	/* gconf notification ids */
	guint interp_type_notify_id;
	guint transparency_notify_id;
	guint trans_color_notify_id;

	/* signal ids */
	guint sig_id_list_prepared;
	guint sig_id_progress;
	guint sig_id_loading_finished;
	guint sig_id_loading_failed;
};

enum {
	SIGNAL_OPEN_URI_LIST,
	SIGNAL_NEW_WINDOW,
	SIGNAL_LAST
};

typedef enum {
	EOG_WINDOW_MODE_UNKNOWN,
	EOG_WINDOW_MODE_SINGLETON,
	EOG_WINDOW_MODE_COLLECTION
} EogWindowMode;

static int eog_window_signals [SIGNAL_LAST];

static void eog_window_class_init (EogWindowClass *class);
static void eog_window_init (EogWindow *window);

static gint eog_window_delete (GtkWidget *widget, GdkEventAny *event);
static gint eog_window_key_press (GtkWidget *widget, GdkEventKey *event);
static void eog_window_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y, 
				    GtkSelectionData *selection_data, guint info, guint time);
static void adapt_window_size (EogWindow *window, int width, int height);
static void update_status_bar (EogWindow *window);
static void job_default_progress (EogJob *job, gpointer data, float progress);


static GtkWindowClass *parent_class;

/* The list of all open windows */
static GList *window_list = NULL;

/* Drag target types */
enum {
	TARGET_URI_LIST
};

/* Data storage for EogJobs */
typedef struct {
	EogWindow *window;
} EogJobData;

typedef struct {
	EogJobData generic;
	EogImage   *image;
} EogJobImageLoadData;

#define EOG_JOB_DATA(o) ((EogJobData*) o)


static GQuark
eog_window_error_quark (void)
{
	static GQuark q = 0;
	if (q == 0)
		q = g_quark_from_static_string ("eog-window-error-quark");
	
	return q;
}


/* Computes a unique ID string for use as the window role */
static char *
gen_role (void)
{
        char *ret;
	static char *hostname;
	time_t t;
	static int serial;

	t = time (NULL);

	if (!hostname) {
		static char buffer [512];

		if ((gethostname (buffer, sizeof (buffer) - 1) == 0) &&
		    (buffer [0] != 0))
			hostname = buffer;
		else
			hostname = "localhost";
	}

	ret = g_strdup_printf ("eog-window-%d-%d-%d-%ld-%d@%s",
			       getpid (),
			       getgid (),
			       getppid (),
			       (long) t,
			       serial++,
			       hostname);

	return ret;
}

/** 
 * Copied from eel/eel-gtk-extensions.c
 * eel_gtk_window_get_geometry_string:
 * @window: a #GtkWindow
 * 
 * Obtains the geometry string for this window, suitable for
 * set_geometry_string(); assumes the window has NorthWest gravity
 * 
 * Return value: geometry string, must be freed
 **/
static char*
eog_gtk_window_get_geometry_string (GtkWindow *window)
{
	char *str;
	int w, h, x, y;
	
	g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);
	g_return_val_if_fail (gtk_window_get_gravity (window) ==
			      GDK_GRAVITY_NORTH_WEST, NULL);

	gtk_window_get_position (window, &x, &y);
	gtk_window_get_size (window, &w, &h);
	
	str = g_strdup_printf ("%dx%d+%d+%d", w, h, x, y);

	return str;
}

static EogWindowMode
eog_window_get_mode (EogWindow *window)
{
	EogWindowMode mode = EOG_WINDOW_MODE_UNKNOWN;

	g_return_val_if_fail (EOG_IS_WINDOW (window), EOG_WINDOW_MODE_UNKNOWN);

	if (window->priv->image_list != NULL) {
		int n_images = eog_image_list_length (window->priv->image_list);
		
		if (n_images == 1) 
			mode = EOG_WINDOW_MODE_SINGLETON;
		else if (n_images > 1) 
			mode = EOG_WINDOW_MODE_COLLECTION;
	}

	return mode;
}

static gboolean
save_window_geometry_timeout (gpointer callback_data)
{
	EogWindow *window;
	
	window = EOG_WINDOW (callback_data);
	
	eog_window_save_geometry (window);

	window->priv->save_geometry_timeout_id = 0;

	return FALSE;
}

static gboolean
eog_window_configure_event (GtkWidget *widget,
			    GdkEventConfigure *event)
{
	EogWindow *window;
	char *geometry_string;
	
	window = EOG_WINDOW (widget);

	GTK_WIDGET_CLASS (parent_class)->configure_event (widget, event);
	
	/* Only save the geometry if the user hasn't resized the window
	 * for a second. Otherwise delay the callback another second.
	 */
	if (window->priv->save_geometry_timeout_id != 0) {
		g_source_remove (window->priv->save_geometry_timeout_id);
	}
	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (window))) {
		geometry_string = eog_gtk_window_get_geometry_string (GTK_WINDOW (window));
	
		/* If the last geometry is NULL the window must have just
		 * been shown. No need to save geometry to disk since it
		 * must be the same.
		 */
		if (window->priv->last_geometry == NULL) {
			window->priv->last_geometry = geometry_string;
			return FALSE;
		}
	
		/* Don't save geometry if it's the same as before. */
		if (!strcmp (window->priv->last_geometry, 
			     geometry_string)) {
			g_free (geometry_string);
			return FALSE;
		}

		g_free (window->priv->last_geometry);
		window->priv->last_geometry = geometry_string;

		window->priv->save_geometry_timeout_id = 
			g_timeout_add (1000, save_window_geometry_timeout, window);
	}
	
	return FALSE;
}

static void
eog_window_unrealize (GtkWidget *widget)
{
	EogWindow *window;
	
	window = EOG_WINDOW (widget);

	GTK_WIDGET_CLASS (parent_class)->unrealize (widget);

	if (window->priv->save_geometry_timeout_id != 0) {
		g_source_remove (window->priv->save_geometry_timeout_id);
		window->priv->save_geometry_timeout_id = 0;
		eog_window_save_geometry (window);
	}
}

void
eog_window_save_geometry (EogWindow *window)
{
	EogWindowPrivate *priv;
	char *geometry_string;
	EogWindowMode mode;
	char *key = NULL;

	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	mode = eog_window_get_mode (window);
	if (mode == EOG_WINDOW_MODE_SINGLETON)
		key = EOG_CONF_WINDOW_GEOMETRY_SINGLETON;
	else if (mode == EOG_WINDOW_MODE_COLLECTION)
		key = EOG_CONF_WINDOW_GEOMETRY_COLLECTION;
	
	if (GTK_WIDGET(window)->window && key != NULL && 
	    !(gdk_window_get_state (GTK_WIDGET(window)->window) & GDK_WINDOW_STATE_MAXIMIZED)) {
		geometry_string = eog_gtk_window_get_geometry_string (GTK_WINDOW (window));

		gconf_client_set_string (priv->client, key, geometry_string, NULL);

		g_free (geometry_string);
	}
}

static void
verb_FileNewWindow_cb (GtkAction *action, gpointer user_data)
{
	g_signal_emit (G_OBJECT (user_data), eog_window_signals [SIGNAL_NEW_WINDOW], 0);
}

static void
verb_FileOpen_cb (GtkAction *action, gpointer user_data)
{
	EogWindow *window;
	EogWindowPrivate *priv;
	GtkWidget *dlg;
	gint response;
	GSList *list = NULL;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);
	priv = window->priv;

	dlg = eog_file_selection_new (GTK_FILE_CHOOSER_ACTION_OPEN);

	gtk_widget_show_all (dlg);
	response = gtk_dialog_run (GTK_DIALOG (dlg));
	if (response == GTK_RESPONSE_OK) {
		list = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (dlg));
	}

	gtk_widget_destroy (dlg);

	if (list) {
		g_signal_emit (G_OBJECT (window), eog_window_signals[SIGNAL_OPEN_URI_LIST], 0, list);
	}
}

static void
verb_FolderOpen_cb (GtkAction *action, gpointer user_data)
{
	EogWindow *window;
	EogWindowPrivate *priv;
	GtkWidget *dlg;
	gint response;
	GSList *list = NULL;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);
	priv = window->priv;

	dlg = eog_file_selection_new (GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

	gtk_widget_show_all (dlg);
	response = gtk_dialog_run (GTK_DIALOG (dlg));
	if (response == GTK_RESPONSE_OK) {
		list = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (dlg));
	}

	gtk_widget_destroy (dlg);

	if (list) {
		g_signal_emit (G_OBJECT (window), eog_window_signals[SIGNAL_OPEN_URI_LIST], 0, list);
	}
}

static void
verb_FileCloseWindow_cb (GtkAction *action, gpointer user_data)
{
	eog_window_close (EOG_WINDOW (user_data));
}

static void
verb_EditPreferences_cb (GtkAction *action, gpointer data)
{
	GConfClient *client;
	EogWindowPrivate *priv;

	priv = EOG_WINDOW (data)->priv;

	client = priv->client;

	eog_preferences_show (client);
}

static void
verb_HelpAbout_cb (GtkAction *action, gpointer data)
{
	EogWindow *window;
	GdkPixbuf *pixbuf;
	
	static const char *authors[] = {
		"Jens Finke <jens@triq.net>",
		"",
		"Martin Baulig <martin@home-of-linux.org>",
		"Arik Devens <arik@gnome.org>",
		"Michael Meeks <mmeeks@gnu.org>",
		"Federico Mena-Quintero <federico@gnu.org>",
		"Lutz M\xc3\xbcller <urc8@rz.uni-karlsruhe.de>",
		NULL
	};
	static const char *documenters[] = {
		"Eliot Landrum <eliot@landrum.cx>",
		"Sun GNOME Documentation Team <gdocteam@sun.com>",
		"Federico Mena-Quintero <federico@gnu.org>",
		NULL
	};
	const char *translators;

	/* Translators should localize the following string
	 * which will give them credit in the About box.
	 * E.g. "Fulano de Tal <fulano@detal.com>"
	 */
	translators = _("translator-credits");

	pixbuf = gdk_pixbuf_new_from_file (EOG_ICONDIR "/gnome-eog.png", NULL);

	window = EOG_WINDOW (data);

	gtk_show_about_dialog (GTK_WINDOW(window),
			"name", _("Eye of GNOME"),
			"version", VERSION,
			"copyright", "Copyright \xc2\xa9 2000-2005 Free Software Foundation, Inc.",
			"comments",_("The GNOME image viewing and cataloging program."),
			"authors", authors,
			"documenters", documenters,
			"translator-credits", translators,
			"logo", pixbuf,
			NULL);
	
	if (pixbuf != NULL) 
		g_object_unref (pixbuf);
}

static void
verb_HelpContent_cb (GtkAction *action, gpointer data)
{
	GError *error;
	EogWindow *window;

	window = EOG_WINDOW (data);

	error = NULL;
	gnome_help_display ("eog", NULL, &error);

	if (error) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (GTK_WINDOW (window),
						 0,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("Could not display help for Eye of GNOME.\n"
						   "%s"),
						 error->message);

		g_signal_connect_swapped (dialog, "response",
					  G_CALLBACK (gtk_widget_destroy),
					  dialog);
		gtk_widget_show (dialog);

		g_error_free (error);
	}
}

static void
verb_ZoomIn_cb (GtkAction *action, gpointer data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (data));

	priv = EOG_WINDOW (data)->priv;

	if (priv->scroll_view) {
		eog_scroll_view_zoom_in (EOG_SCROLL_VIEW (priv->scroll_view), FALSE);
	}
}

static void
verb_ZoomOut_cb (GtkAction *action, gpointer data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (data));

	priv = EOG_WINDOW (data)->priv;

	if (priv->scroll_view) {
		eog_scroll_view_zoom_out (EOG_SCROLL_VIEW (priv->scroll_view), FALSE);
	}
}

static void
verb_ZoomNormal_cb (GtkAction *action, gpointer data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (data));

	priv = EOG_WINDOW (data)->priv;

	if (priv->scroll_view) {
		eog_scroll_view_set_zoom (EOG_SCROLL_VIEW (priv->scroll_view), 1.0);
	}
}

static void
verb_ZoomFit_cb (GtkAction *action, gpointer data)
{
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (data));

	priv = EOG_WINDOW (data)->priv;

	if (priv->scroll_view) {
		eog_scroll_view_zoom_fit (EOG_SCROLL_VIEW (priv->scroll_view));
	}
}

static void
slideshow_hide_cb (GtkWidget *widget, gpointer data)
{
	EogImage *last_image;
	EogFullScreen *fs;
	EogWindow *window;
	EogWindowPrivate *priv;

	fs = EOG_FULL_SCREEN (widget);
	window = EOG_WINDOW (data);
	priv = window->priv;

	last_image = eog_full_screen_get_last_image (fs);
	
	if (last_image != NULL) {
		eog_wrap_list_set_current_image (EOG_WRAP_LIST (priv->wraplist), last_image, TRUE);
		g_object_unref (last_image);
	}

	gtk_widget_destroy (widget);
}

static void
verb_FullScreen_cb (GtkAction *action, gpointer data)
{
	EogWindowPrivate *priv;
	GtkWidget *fs;
	EogImageList *list = NULL;
	EogImage *start_image = NULL;
	int n_selected;

	g_return_if_fail (EOG_IS_WINDOW (data));

	priv = EOG_WINDOW (data)->priv;

	n_selected = eog_wrap_list_get_n_selected (EOG_WRAP_LIST (priv->wraplist));
	if (n_selected == 1) {
		list = g_object_ref (priv->image_list);
		start_image = eog_wrap_list_get_first_selected_image (EOG_WRAP_LIST (priv->wraplist));
	}
	else if (n_selected > 1) {
		GList *l;
		l = eog_wrap_list_get_selected_images (EOG_WRAP_LIST (priv->wraplist));

		list = eog_image_list_new_from_glist (l);

		g_list_foreach (l, (GFunc) g_object_unref, NULL);
		g_list_free (l);
	}

	if (list != NULL) {
		fs = eog_full_screen_new (list, start_image);
		g_signal_connect (G_OBJECT (fs), "hide", G_CALLBACK (slideshow_hide_cb), EOG_WINDOW (data));

		gtk_widget_show_all (fs);

		g_object_unref (list);
	}

	if (start_image != NULL) 
		g_object_unref (start_image);
}

static void
verb_ShowHideAnyBar_cb (GtkAction *action, gpointer data)
{
	EogWindow *window;     
	EogWindowPrivate *priv;
	gboolean visible;

	g_return_if_fail(EOG_IS_WINDOW (data));

	window = EOG_WINDOW (data);
	priv = window->priv;

	visible = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	if (g_ascii_strcasecmp (gtk_action_get_name (action), "ViewToolbar") == 0) {
		GtkWidget *widget = gtk_ui_manager_get_widget (priv->ui_mgr, "/ToolBar");
		g_object_set (G_OBJECT (widget), "visible", visible, NULL);
		gconf_client_set_bool (priv->client, EOG_CONF_UI_TOOLBAR, visible, NULL);
	}
	else if (g_ascii_strcasecmp (gtk_action_get_name (action), "ViewStatusbar") == 0) {
		g_object_set (G_OBJECT (priv->statusbar), "visible", visible, NULL);
		gconf_client_set_bool (priv->client, EOG_CONF_UI_STATUSBAR, visible, NULL);
	}
	else if (g_ascii_strcasecmp (gtk_action_get_name (action), "ViewInfo") == 0) {
		const char *key = NULL;
		int n_images = 0;

		g_object_set (G_OBJECT (priv->info_view), "visible", visible, NULL);

		if (priv->image_list != NULL) {
			n_images = eog_image_list_length (priv->image_list);
		}

		if (n_images == 1) {
			key = EOG_CONF_UI_INFO_IMAGE;
		}
		else if (n_images > 1) {
			key = EOG_CONF_UI_INFO_COLLECTION;
		}
		
		if (key != NULL) {
			gconf_client_set_bool (priv->client, key, visible, NULL);
		}
	}
}

/* ================================================
 *
 *             Save functions 
 * 
 * -----------------------------------------------*/

typedef enum {
	EOG_SAVE_RESPONSE_NONE,
	EOG_SAVE_RESPONSE_RETRY,
	EOG_SAVE_RESPONSE_SKIP,
	EOG_SAVE_RESPONSE_OVERWRITE,
	EOG_SAVE_RESPONSE_CANCEL
} EogSaveResponse;

typedef struct {
	/* all images */
	GList     *images;
	int        n_images;
	int        n_processed;

	/* image currently processed */
	EogImage         *current;
	EogImageSaveInfo *source; 
	EogImageSaveInfo *dest; /* destination */
	EogURIConverter  *conv;

	/* dialog handling */
	EogWindow  *window;
	GtkWindow  *dlg;

	gboolean   cancel_save;
	GMutex     *lock;
	guint      job_id;
} SaveData;

typedef struct {
	EogWindow   *window;
	EogImage    *image;
	
	GError      *error;
	EogSaveResponse response;
	GCond       *wait;
	GMutex      *lock;
} SaveErrorData;

static gboolean
save_dialog_update_finished (SaveData *data)
{
	eog_save_dialog_finished_image (data->dlg);
	return FALSE;
}

static gboolean
save_dialog_update_start_image (SaveData *data)
{
	GnomeVFSURI* uri = NULL;
	EogImage *image = NULL;

	g_mutex_lock (data->lock);
	if (data->current != NULL) {
		image = g_object_ref (data->current);
	}
	if (data->dest != NULL) {
		uri = gnome_vfs_uri_ref (data->dest->uri);
	}
	g_mutex_unlock (data->lock);

	eog_save_dialog_start_image (data->dlg,
				     image, uri);
		
	if (image != NULL) 
		g_object_unref (image);
	if (uri != NULL) 
		gnome_vfs_uri_unref (uri);

	return FALSE;
}

static void
save_dialog_cancel_cb (GtkWidget *button, SaveData *data)
{
	eog_job_manager_cancel_job (data->job_id);
}

static gboolean
save_update_image (EogImage *image)
{
	if (EOG_IS_IMAGE (image)) {
		eog_image_modified (image);
		g_object_unref (image);
	}
	
	return FALSE;
}

static gboolean
save_error (SaveErrorData *edata)
{
	GtkWidget *dlg;
	char *header;
	char *detail = NULL;
	int   response;
	gint  err_code = 0;
	
	g_mutex_lock (edata->lock);
	if (edata->error != NULL) {
		detail   = edata->error->message;
		err_code = edata->error->code;
	}
	g_mutex_unlock (edata->lock);

	/* display generic error dialog, except for FILE_EXISTS error */
	if (err_code == EOG_IMAGE_ERROR_FILE_EXISTS) {
		char *str;

		str = eog_image_get_uri_for_display (edata->image); 

		header = g_strdup_printf (_("Overwrite file %s?"), str);
		detail = _("File exists. Do you want to overwrite it?");

		dlg = eog_hig_dialog_new (GTK_WINDOW (edata->window), 
					  GTK_STOCK_DIALOG_ERROR,
					  header, detail,
					  TRUE);

		g_free (str);

		gtk_dialog_add_button (GTK_DIALOG (dlg), _("Skip"), EOG_SAVE_RESPONSE_SKIP);
		gtk_dialog_add_button (GTK_DIALOG (dlg), _("Overwrite"), EOG_SAVE_RESPONSE_OVERWRITE);
		gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_CANCEL, EOG_SAVE_RESPONSE_CANCEL);
		gtk_dialog_set_default_response (GTK_DIALOG (dlg), EOG_SAVE_RESPONSE_SKIP);
	}
	else {
		header = g_strdup_printf (_("Error on saving %s."), eog_image_get_caption (edata->image));
		
		dlg = eog_hig_dialog_new (GTK_WINDOW (edata->window), 
					  GTK_STOCK_DIALOG_ERROR,
					  header, detail,
					  TRUE);
	
		gtk_dialog_add_button (GTK_DIALOG (dlg), _("Skip"), EOG_SAVE_RESPONSE_SKIP);
		gtk_dialog_add_button (GTK_DIALOG (dlg), _("Retry"), EOG_SAVE_RESPONSE_RETRY);
		gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_CANCEL, EOG_SAVE_RESPONSE_CANCEL);
		gtk_dialog_set_default_response (GTK_DIALOG (dlg), EOG_SAVE_RESPONSE_SKIP);
	}
	
	gtk_widget_show_all (dlg);

	response = gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);
	g_free (header);
	
	g_mutex_lock (edata->lock);
	edata->response = (EogSaveResponse) response;
	g_mutex_unlock (edata->lock);
	g_cond_broadcast (edata->wait);

	return FALSE;
}

static void
job_save_image_finished (EogJob *job, gpointer user_data, GError *error)
{
	SaveData *data = (SaveData*) user_data;

	eog_save_dialog_close (data->dlg, !data->cancel_save);

	/* enable image modification functions */
	gtk_action_group_set_sensitive (data->window->priv->actions_image,  TRUE);

}

static SaveData*
save_data_new (EogWindow *window, GList *images)
{
	SaveData *data;

	g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);
	g_return_val_if_fail (images != NULL, NULL);

	data = g_new0 (SaveData, 1);
	g_assert (data != NULL);

	/* initialise data struct */
	data->images      = images; 
	data->n_images    = g_list_length (images);
	data->n_processed = 0;
	
	data->current = NULL;
	data->source  = NULL;
	data->dest    = NULL;
	data->conv    = NULL;

	data->cancel_save = FALSE;

	data->lock = g_mutex_new ();

	data->window = window;
	data->dlg = GTK_WINDOW (eog_save_dialog_new (GTK_WINDOW (window), data->n_images));

	return data;
}

static void
job_save_data_free (gpointer user_data)
{
	SaveData *data = (SaveData*) user_data;

	g_mutex_lock (data->lock);

	g_list_foreach (data->images, (GFunc) g_object_unref, NULL);
	g_list_free (data->images);

	if (data->source)
		g_object_unref (data->source);

	if (data->dest)
		g_object_unref (data->dest);

	if (data->conv)
		g_object_unref (data->conv);
	g_mutex_unlock (data->lock);

	g_mutex_free (data->lock);
	g_free (data);
}

static void
job_save_image_cancel (EogJob *job, gpointer user_data)
{
	SaveData *data = (SaveData*) user_data;

	g_mutex_lock (data->lock);
	data->cancel_save = TRUE;
	if (data->current != NULL) {
		eog_image_cancel_load (EOG_IMAGE (data->current));
	}
	g_mutex_unlock (data->lock);

	eog_save_dialog_cancel (data->dlg);
}

static void
job_save_image_progress (EogJob *job, gpointer user_data, float progress)
{
	SaveData *data = (SaveData*) user_data;

	eog_save_dialog_set_progress (data->dlg, progress);
}

static SaveErrorData*
save_error_data_new (GError *error, EogWindow *window, EogImage *image)
{
	SaveErrorData *edata;

	g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	edata = g_new0 (SaveErrorData, 1);
	edata->error    = NULL;
	edata->response = EOG_SAVE_RESPONSE_NONE;
	edata->wait     = g_cond_new ();
	edata->lock     = g_mutex_new ();
	edata->window   = window;
	edata->image    = g_object_ref (image);

	if (error != NULL) {
		edata->error    = g_error_copy (error); 
	}
	
	return edata;
}

static void
save_error_data_free (SaveErrorData *edata)
{
	if (edata == NULL)
		return;

	if (edata->error != NULL) {
		g_error_free (edata->error);
	}
	g_cond_free  (edata->wait);
	g_mutex_free (edata->lock);
	g_object_unref (edata->image);
	g_free (edata);
}

static gboolean
job_save_handle_error (SaveData *data, EogImage *image, GError *error)
{
	SaveErrorData *edata;
	gboolean success = FALSE;

	edata = save_error_data_new (error, data->window, image);
	
	g_mutex_lock (edata->lock);
	g_idle_add ((GSourceFunc) save_error, edata);
	/* wait for error dialog response */
	g_cond_wait (edata->wait, edata->lock);
	g_mutex_unlock (edata->lock);
	
	/* handle results */
	switch (edata->response) {
	case EOG_SAVE_RESPONSE_SKIP:
		success = TRUE;
		break;
	case EOG_SAVE_RESPONSE_CANCEL:
		data->cancel_save = TRUE;
		break;
	case EOG_SAVE_RESPONSE_OVERWRITE:
		g_assert (data->dest != NULL);
		if (data->dest != NULL) {
			data->dest->overwrite = TRUE;
		}
		break;
	case EOG_SAVE_RESPONSE_RETRY:
		success = FALSE;
		break;
	default:
		g_assert_not_reached ();
	}
	save_error_data_free (edata);

	return success;
}

static gboolean
job_save_image_single (EogJob *job, SaveData *data, EogImage *image, GError **error)
{
	EogImageSaveInfo *info = NULL;
	gboolean success = FALSE;

	g_return_val_if_fail (EOG_IS_IMAGE (image), FALSE);
	g_return_val_if_fail (EOG_IS_JOB (job), FALSE);

	/* increase object and data ref count */
	eog_image_data_ref (image);

	while (!success && !data->cancel_save) {
		if (*error != NULL) {
			g_error_free (*error);
			*error = NULL;
		}

		success = eog_image_has_data (image, EOG_IMAGE_DATA_ALL);
		if (!success) {
			success = eog_image_load (image, EOG_IMAGE_DATA_ALL, job, error);
		}

		if (info == NULL) 
			info = eog_image_save_info_from_image (image);
#ifdef SAVE_DEBUG
		g_print ("Save image at %s ", 
			 gnome_vfs_uri_to_string (info->uri, GNOME_VFS_URI_HIDE_NONE));
#endif

		if (success) {
			success = eog_image_save_by_info (image, info, job, error);
		}
#ifdef SAVE_DEBUG
		g_print ("successful: %i\n", success);
#endif
		
		/* handle errors, let user decide how to proceede */
		if (!success && !data->cancel_save) {
			success = job_save_handle_error (data, image, *error);
		}
	}

	if (info != NULL)
		g_object_unref (info);
	
	eog_image_data_unref (image);

	return (success && !data->cancel_save);
}

/* this runs in its own thread */
static void
job_save_image_list (EogJob *job, gpointer user_data, GError **error)
{
	SaveData *data; 
	GList *it;
	EogImage *image;
	gboolean success;

	data = (SaveData*) user_data;

	for (it = data->images; it != NULL && !data->cancel_save; it = it->next) {
		image = EOG_IMAGE (it->data);

		g_mutex_lock (data->lock);
		data->current = image;
		g_mutex_unlock (data->lock);

		g_idle_add ((GSourceFunc) save_dialog_update_start_image, data);

		success = job_save_image_single (job, data, image, error);

		g_mutex_lock (data->lock);
		data->current = NULL;
		g_mutex_unlock (data->lock);

		g_idle_add ((GSourceFunc) save_dialog_update_finished, data);
		eog_job_part_finished (job);
	}
}


static void
verb_Save_cb (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;
	int n_images;
	SaveData *data;
	EogJob *job;
	GtkWidget *button;

	priv = EOG_WINDOW (user_data)->priv;

	n_images = eog_wrap_list_get_n_selected (EOG_WRAP_LIST (priv->wraplist));
	if (n_images == 0) return;

	/* init save data */
	data = save_data_new (EOG_WINDOW (user_data), 
			      eog_wrap_list_get_selected_images (EOG_WRAP_LIST (priv->wraplist)));
	g_assert (data != NULL);
	g_assert (GTK_IS_WINDOW (data->dlg));

	/* connect to cancel button */
	button = eog_save_dialog_get_button (GTK_WINDOW (data->dlg));
	g_signal_connect (G_OBJECT (button), "clicked", (GCallback) save_dialog_cancel_cb, data);

	/* disable image modification functions */
	gtk_action_group_set_sensitive (priv->actions_image,  FALSE);

	/* show dialog */
	gtk_widget_show_all (GTK_WIDGET (data->dlg));
	gtk_widget_show_now (GTK_WIDGET (data->dlg));

	/* start job */
	job = eog_job_new_full (data,
				job_save_image_list,
				job_save_image_finished,
				job_save_image_cancel,
				job_save_image_progress,
				job_save_data_free);
	g_object_set (G_OBJECT (job), "progress-n-parts", data->n_images, NULL);

	data->job_id = eog_job_manager_add (job);
	g_object_unref (G_OBJECT (job));
}

static gboolean
job_save_as_image_single (EogJob *job, EogImage *image, SaveData *data, GError **error)
{
	EogImageSaveInfo *source = NULL;
	EogImageSaveInfo *dest;
	gboolean success;
	
	g_return_val_if_fail (EOG_IS_JOB (job), FALSE);
	g_return_val_if_fail (EOG_IS_IMAGE (image), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (error != NULL, FALSE);

	dest = data->dest;
	g_return_val_if_fail (EOG_IS_IMAGE_SAVE_INFO (dest), FALSE);

	eog_image_data_ref (image);

	/* try to save image source to destination */
	success = FALSE;
	while (!success && !data->cancel_save) {
		if (*error != NULL) {
			g_error_free (*error);
			*error = NULL;
		}

		success = eog_image_has_data (image, EOG_IMAGE_DATA_ALL);
		if (!success) {
			success = eog_image_load (image, EOG_IMAGE_DATA_ALL, job, error);
		}
		
		if (success && source == NULL) {
			source = eog_image_save_info_from_image (image);
			success = (source != NULL);
		}

#ifdef SAVE_DEBUG
		g_print ("Saving image from: %s to: %s\n", 
			 (source != NULL) ?
			 gnome_vfs_uri_to_string (source->uri, GNOME_VFS_URI_HIDE_NONE) : "NULL",
			 (dest != NULL) ? 
			 gnome_vfs_uri_to_string (dest->uri, GNOME_VFS_URI_HIDE_NONE) : "NULL");
#endif

		if (success) {
			success = eog_image_save_as_by_info (image, source, dest, job, error);
		}
#ifdef SAVE_DEBUG
		g_print ("successful: %i\n", success);
#endif
		if (source != NULL) {
			g_object_unref (source);
			source = NULL;
		}

		if (!success && !data->cancel_save) {
			/* handle error case */
			success = job_save_handle_error (data, image, *error);
		}
	}
	
	eog_image_data_unref (image);
	eog_job_set_progress (job, 1.0);

	return success;
}

/* this runs in its own thread */
/* does the actual work */
static void
job_save_as_image_list (EogJob *job, gpointer user_data, GError **error)
{
	SaveData *data;
	GList *it;
	EogImage *image;

	data = (SaveData*) user_data;
	
	g_assert (data->conv != NULL || data->dest != NULL);
	
	for (it = data->images; it != NULL && !data->cancel_save; it = it->next) {
		
		image = EOG_IMAGE (it->data);

		g_mutex_lock (data->lock);
		data->current = image;
		g_mutex_unlock (data->lock);

		g_idle_add ((GSourceFunc) save_dialog_update_start_image, data);
	
		/* obtain destination information */
		if (data->conv != NULL) {
			GdkPixbufFormat *format;
			GnomeVFSURI *dest_uri;
			gboolean result;

			result = eog_uri_converter_do (data->conv,
						       image, 
						       &dest_uri,
						       &format,
						       error);

			if (result == FALSE) {
				g_set_error (error, 
					     EOG_WINDOW_ERROR, EOG_WINDOW_ERROR_GENERIC,
					     _("Couldn't determine destination uri."));
				break; /* will leave for-loop */
			}
#ifdef SAVE_DEBUG
			g_print ("convert uri: %s\n", gnome_vfs_uri_to_string (dest_uri, GNOME_VFS_URI_HIDE_NONE));
#endif
			
			g_mutex_lock (data->lock);
			/* set destination info for current image */
			if (data->dest != NULL) {
				g_object_unref (data->dest);
				data->dest = NULL;
			}

			data->dest = eog_image_save_info_from_vfs_uri (dest_uri, format);
			g_mutex_unlock (data->lock);
				
			if (dest_uri)
				gnome_vfs_uri_unref (dest_uri);
		}

		g_assert (data->dest != NULL);

		/* try to save image */
		if (!job_save_as_image_single (job, image, data, error)) {
			g_warning ("no save_as success\n");
		}

		/* update job status */
		g_idle_add ((GSourceFunc) save_dialog_update_finished, data);
		eog_job_part_finished (job);

		g_mutex_lock (data->lock);
		data->current = NULL;
		g_mutex_unlock (data->lock);
	}
}

static char* 
get_folder_uri_from_image (EogImage *image)
{
	GnomeVFSURI *img_uri;
	GnomeVFSURI *parent;
	char *folder_uri = NULL;

	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	img_uri = eog_image_get_uri (image);

	if (gnome_vfs_uri_has_parent (img_uri)) {
		parent = gnome_vfs_uri_get_parent (img_uri);
		folder_uri = gnome_vfs_uri_to_string (parent, GNOME_VFS_URI_HIDE_NONE);
		gnome_vfs_uri_unref (parent);
	}

	gnome_vfs_uri_unref (img_uri);
	return folder_uri;
}

/* Asks user for a file location to save an image there. Returns the save target uri
 * and the image format. The uri parameter is set to NULL if the user canceled the
 * dialog.
 */
static void
save_as_uri_selection_dialog (EogWindow *window, EogImage *image, char **uri, GdkPixbufFormat **format)
{
	GtkWidget *dlg;
	gboolean success = FALSE;
	gint response;
	char *folder_uri = NULL;

	g_return_if_fail (uri != NULL);
	g_return_if_fail (format != NULL);
	
	*uri = NULL;
	*format = NULL;

	g_return_if_fail (EOG_IS_IMAGE (image));
	g_return_if_fail (EOG_IS_WINDOW (window));

	folder_uri = get_folder_uri_from_image (image);
	if (folder_uri == NULL) {
		g_warning ("Parent uri for %s not available.\n", eog_image_get_caption (image));
		return;
	}

	dlg = eog_file_selection_new (GTK_FILE_CHOOSER_ACTION_SAVE);
	gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dlg),
						 folder_uri);
	while (!success) {
		success = TRUE;
		if (*uri != NULL) {
			g_free (*uri);
			*uri = NULL;
		}

		gtk_widget_show_all (dlg);
		response = gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_hide (dlg);

		if (response == GTK_RESPONSE_OK) {
			/* try to determine uri and image format */
			*format = eog_file_selection_get_format (EOG_FILE_SELECTION (dlg));
			*uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dlg));

			if (*format == NULL && *uri != NULL) {
				*format = eog_pixbuf_get_format_by_uri (*uri);
			}
				
			success = (*format != NULL && *uri != NULL);
		}
		
		if (!success) {
			GtkWidget *err_dlg;
			char *header;
			char *detail;
			char *short_name;
			char *uesc_uri;

			uesc_uri = gnome_vfs_unescape_string_for_display (*uri);
			short_name = g_path_get_basename (uesc_uri);

			header = g_strdup_printf (_("Couldn't determine file format of %s"), short_name);
			detail = _("Please use an appropriate filename suffix or select a file format.");
			
			g_free (uesc_uri);
			g_free (short_name);

			err_dlg = eog_hig_dialog_new (GTK_WINDOW (window),
						      GTK_STOCK_DIALOG_ERROR,
						      header, detail,
						      TRUE);
			
			gtk_dialog_add_button (GTK_DIALOG (err_dlg), _("Retry"), GTK_RESPONSE_OK);
			gtk_dialog_add_button (GTK_DIALOG (err_dlg), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
			gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);
			gtk_widget_show_all (err_dlg);
			
			response = gtk_dialog_run (GTK_DIALOG (err_dlg));
			gtk_widget_destroy (err_dlg);
			g_free (header);

			if (response == GTK_RESPONSE_CANCEL) {
				if (*uri != NULL) {
					g_free (*uri);
					*uri = NULL;
				}
				success = TRUE;
			}
		}
	}

	gtk_widget_destroy (dlg);
}


/* prepare load for a single image */
static void
save_as_single_image (EogWindow *window, EogImage *image)
{
	EogWindowPrivate *priv;
	char *uri = NULL;
	SaveData *data;
	GdkPixbufFormat *format = NULL;
	GtkWidget *button;
	EogJob *job;

	g_return_if_fail (EOG_IS_WINDOW (window));
	g_return_if_fail (EOG_IS_IMAGE (image));

	priv = window->priv;

	save_as_uri_selection_dialog (window, image, &uri, &format);
	if (uri == NULL)
		return;

	g_assert (uri != NULL && format != NULL);

	data = save_data_new (window, g_list_append (NULL, image));
	g_assert (data != NULL);
	g_assert (GTK_IS_WINDOW (data->dlg));

	data->dest = eog_image_save_info_from_uri (uri, format);
	g_free (uri);

	button = eog_save_dialog_get_button (GTK_WINDOW (data->dlg));
	g_signal_connect (G_OBJECT (button), "clicked", (GCallback) save_dialog_cancel_cb, data);

	/* disable image modification functions */
	gtk_action_group_set_sensitive (priv->actions_image,  FALSE);

	gtk_widget_show_all (GTK_WIDGET (data->dlg));
	gtk_widget_show_now (GTK_WIDGET (data->dlg));

	/* start job */
	job = eog_job_new_full (data,
				job_save_as_image_list, /* Save As for multiple images */
				job_save_image_finished,
				job_save_image_cancel,
				job_save_image_progress,
				job_save_data_free);
	g_object_set (G_OBJECT (job), "progress-n-parts", data->n_images, NULL);

	data->job_id = eog_job_manager_add (job);
	g_object_unref (G_OBJECT (job));
}

static EogURIConverter*
save_as_uri_converter_dialog (EogWindow *window, GList *images)
{
	EogURIConverter *conv = NULL;
	GtkWidget *dlg;
	char *base_dir;
	GnomeVFSURI *base_uri;
	gint response; 
	gboolean success = FALSE;

	g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);
	g_return_val_if_fail (images != NULL, NULL);
	
	if (g_list_length (images) < 2) return NULL;

	base_dir = g_get_current_dir ();
	base_uri = gnome_vfs_uri_new (base_dir);
	g_free (base_dir);

	/* function defined in ../libeog/eog-save-as-dialog-helper.h */
	dlg = eog_save_as_dialog_new (GTK_WINDOW (window), images, 
				      base_uri);

	while (!success) {
		GError *error = NULL;

		if (conv != NULL) {
			g_object_unref (conv);
			conv = NULL;
		}

		gtk_widget_show_all (dlg);
		response = gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_hide (dlg);
		
		if (response != GTK_RESPONSE_OK) {
			break;
		}

		conv = eog_save_as_dialog_get_converter (dlg);
		g_assert (conv != NULL);

		success = eog_uri_converter_check (conv, images, &error);

		if (!success) {
			GtkWidget *error_dlg;

			error_dlg = eog_hig_dialog_new (GTK_WINDOW (window),
							GTK_STOCK_DIALOG_ERROR,
							_("Error on saving images."),
							error->message,
							TRUE);
			gtk_dialog_add_button (GTK_DIALOG (error_dlg), GTK_STOCK_OK, GTK_RESPONSE_OK);
			gtk_dialog_run (GTK_DIALOG (error_dlg));
			gtk_widget_destroy (error_dlg);

			g_object_unref (conv);
			conv = NULL;
		}
	}

	gtk_widget_destroy (dlg);
	gnome_vfs_uri_unref (base_uri);

	return conv;
}

/* prepare load for multiple images */
static void
save_as_multiple_images (EogWindow *window, GList *images)
{
	EogURIConverter *conv = NULL;
	GtkWidget *button;
	SaveData *data;
	EogJob *job;
	
	g_return_if_fail (EOG_IS_WINDOW (window));
	g_return_if_fail (images != NULL);
	
	/* obtain uri converter for translating source uris
	 *  to target uris 
	 */
	conv = save_as_uri_converter_dialog (window, images);
	if (conv == NULL) {
		return;
	}
#ifdef SAVE_DEBUG
	eog_uri_converter_print_list (conv);
#endif

	/* prepare data */
	data = save_data_new (window, images); 
	data->conv = conv;
	
	g_assert (GTK_IS_WINDOW (data->dlg));
	button = eog_save_dialog_get_button (GTK_WINDOW (data->dlg));
	g_signal_connect (G_OBJECT (button), "clicked", (GCallback) save_dialog_cancel_cb, data);
	
	/* disable image modification functions */
	gtk_action_group_set_sensitive (window->priv->actions_image,  FALSE);
	
	gtk_widget_show_all (GTK_WIDGET (data->dlg));
	gtk_widget_show_now (GTK_WIDGET (data->dlg));
	
	/* start job */
	job = eog_job_new_full (data,
				job_save_as_image_list, /* Save As for multiple images */
				job_save_image_finished,
				job_save_image_cancel,
				job_save_image_progress,
				job_save_data_free);
	g_object_set (G_OBJECT (job), "progress-n-parts", data->n_images, NULL);

	data->job_id = eog_job_manager_add (job);
	g_object_unref (G_OBJECT (job));
}

static void
verb_SaveAs_cb (GtkAction *action, gpointer data)
{
	EogWindowPrivate *priv;
	EogWindow *window;
	int n_images;

	window = EOG_WINDOW (data);
	priv = window->priv;

	n_images = eog_wrap_list_get_n_selected (EOG_WRAP_LIST (priv->wraplist));
	if (n_images <= 0) return;

	if (n_images == 1) {
		EogImage *image;

		image = eog_wrap_list_get_first_selected_image (EOG_WRAP_LIST (priv->wraplist));
		save_as_single_image (window, image);
	}
	else {
		GList *images;

		images = eog_wrap_list_get_selected_images (EOG_WRAP_LIST (priv->wraplist));
		save_as_multiple_images (window, images);
	}
}

/* ================================================
 *
 *     Transformation functions 
 * 
 * -----------------------------------------------*/

typedef struct {
	EogJobData    generic;
	GList        *image_list;
	EogTransform *trans;
} EogJobTransformData;

static gboolean
job_transform_image_modified (gpointer data)
{
	g_return_val_if_fail (EOG_IS_IMAGE (data), FALSE);

	eog_image_modified (EOG_IMAGE (data));
	g_object_unref (G_OBJECT (data));

	return FALSE;
}

/* runs in its own thread */
/* If there is no EogTransform object, an Undo function is performed */
static void
job_transform_action (EogJob *job, gpointer data, GError **error)
{
	GList *it;
	EogJobTransformData *td;

	td = (EogJobTransformData*) data;

	for (it = td->image_list; it != NULL; it = it->next) {
		EogImage *image = EOG_IMAGE (it->data);
		
		if (td->trans == NULL) {
			eog_image_undo (image);
		}
		else {
			eog_image_transform (image, td->trans, job);
		}
		
		eog_job_part_finished (job);
		
		if (eog_image_is_modified (image) || td->trans == NULL) {
			g_object_ref (image);
			g_idle_add (job_transform_image_modified, image);
		}
	}
}

static void 
job_transform_free_data (gpointer data)
{
	EogJobTransformData *td = (EogJobTransformData*) data;

	g_list_foreach (td->image_list, (GFunc) g_object_unref, NULL);
	g_list_free (td->image_list);

	if (td->trans != NULL) 
		g_object_unref (td->trans);	

	g_free (td);
}

static void 
apply_transformation (EogWindow *window, EogTransform *trans)
{
	GList *images;
	EogJobTransformData *data;
	EogJob *job;

	g_return_if_fail (EOG_IS_WINDOW (window));

	images = eog_wrap_list_get_selected_images (EOG_WRAP_LIST (window->priv->wraplist));	
	if (images == NULL)
		return;

	/* setup job data */
	data = g_new0 (EogJobTransformData, 1);
	EOG_JOB_DATA (data)->window = window;
	data->image_list = images;
	data->trans = trans;
	
	/* create job */
	job = eog_job_new_full (data, 
				job_transform_action,
				NULL, /* FIXME: Finished func should call save dialog */
				NULL, /* FIXME: Let this be cancelable */
				job_default_progress,
				job_transform_free_data);
	g_object_set (G_OBJECT (job), "progress-n-parts", 
		      (guint) g_list_length (data->image_list),
		      NULL);

	/* run job */
	eog_job_manager_add (job);
	g_object_unref (G_OBJECT (job));
}

static void
verb_Undo_cb (GtkAction *action, gpointer data)
{
	g_return_if_fail (EOG_IS_WINDOW (data));

	apply_transformation (EOG_WINDOW (data), NULL);
}

static void
verb_FlipHorizontal_cb (GtkAction *action, gpointer data)
{
	g_return_if_fail (EOG_IS_WINDOW (data));

	apply_transformation (EOG_WINDOW (data), eog_transform_flip_new (EOG_TRANSFORM_FLIP_HORIZONTAL));
}

static void
verb_FlipVertical_cb (GtkAction *action, gpointer data)
{
	g_return_if_fail (EOG_IS_WINDOW (data));

	apply_transformation (EOG_WINDOW (data), eog_transform_flip_new (EOG_TRANSFORM_FLIP_VERTICAL));
}

static void
verb_Rotate90_cb (GtkAction *action, gpointer data)
{
	g_return_if_fail (EOG_IS_WINDOW (data));

	apply_transformation (EOG_WINDOW (data), eog_transform_rotate_new (90));
}

static void
verb_Rotate270_cb (GtkAction *action, gpointer data)
{
	g_return_if_fail (EOG_IS_WINDOW (data));

	apply_transformation (EOG_WINDOW (data), eog_transform_rotate_new (270));
}

static void
verb_Rotate180_cb (GtkAction *action, gpointer data)
{
	g_return_if_fail (EOG_IS_WINDOW (data));

	apply_transformation (EOG_WINDOW (data), eog_transform_rotate_new (180));
}

/* ========================================================================= */

static int
show_delete_confirm_dialog (EogWindow *window, int n_images)
{
	GtkWidget *dlg;
	char *header;
	int response;

	header = g_strdup_printf (ngettext ("Do you really want to move %i image to trash?", 
					    "Do you really want to move %i images to trash?", n_images), n_images);

	dlg = eog_hig_dialog_new (GTK_WINDOW (window), GTK_STOCK_DIALOG_WARNING,
				  header, NULL, TRUE);
	g_free (header);

	gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_DELETE, GTK_RESPONSE_OK);
	gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);
	gtk_widget_show_all (dlg);

	response = gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);

	return response;
}

static gboolean
delete_image_real (EogImage *image, GError **error)
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
					   &trash_dir, FALSE, FALSE, 0777);
	if (result != GNOME_VFS_OK) {
		gnome_vfs_uri_unref (uri);
		g_set_error (error, EOG_WINDOW_ERROR, 
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
		g_set_error (error, EOG_WINDOW_ERROR,
			     EOG_WINDOW_ERROR_UNKNOWN,
			     gnome_vfs_result_to_string (result));
	}

	return (result == GNOME_VFS_OK);
}

static void
verb_Delete_cb (GtkAction *action, gpointer data)
{
	GList *images;
	GList *it;
	EogWindowPrivate *priv;
	EogImageList *list;
	int pos;
	EogImage *img;
	EogWindow *window;
	int n_images;
	int response; 
	gboolean success;

	g_return_if_fail (EOG_IS_WINDOW (data));

	window = EOG_WINDOW (data);
	priv = window->priv;
	list = priv->image_list;
	
	/* let user confirm this action */
	n_images = eog_wrap_list_get_n_selected (EOG_WRAP_LIST (priv->wraplist));
	if (n_images < 1) return;

	response = show_delete_confirm_dialog (window, n_images);
	if (response != GTK_RESPONSE_OK) return;

	/* save position of selected image after the deletion */
	images = eog_wrap_list_get_selected_images (EOG_WRAP_LIST (priv->wraplist));	
	g_assert (images != NULL);
	pos = eog_image_list_get_pos_by_img (list, EOG_IMAGE (images->data));

	/* FIXME: make a nice progress dialog */
	/* Do the work actually. First try to delete the image from the disk. If this
	 * is successfull, remove it from the screen. Otherwise show error dialog.
	 */
	for (it = images; it != NULL; it = it->next) {
		GError *error = NULL;
		EogImage *image;

		image = EOG_IMAGE (it->data);

		success = delete_image_real (image, &error);
		if (success) {
			/* EogWrapList gets notified by the EogImageList */
			eog_image_list_remove_image (list, image);
		}
		else {
			char *header;
			GtkWidget *dlg;
			
			header = g_strdup_printf (_("Error on deleting image %s"), eog_image_get_caption (image));
			
			dlg = eog_hig_dialog_new (GTK_WINDOW (window), 
						  GTK_STOCK_DIALOG_WARNING, header, 
						  error->message, TRUE);
			
			gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_OK, GTK_RESPONSE_OK);
			gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);
			gtk_widget_show_all (dlg);
			
			gtk_dialog_run (GTK_DIALOG (dlg));
			gtk_widget_destroy (dlg);
		}
	}

	/* free list */
	g_list_foreach (images, (GFunc) g_object_unref, NULL);
	g_list_free (images);

	/* select image at previously saved position */
	pos = MIN (pos, eog_image_list_length (list));
	img = eog_image_list_get_img_by_pos (list, pos);

	eog_wrap_list_set_current_image (EOG_WRAP_LIST (priv->wraplist), img, TRUE);
	if (img != NULL) {
		g_object_unref (img);
	}
}

/* ========================================================================= */

static void
interp_type_changed_cb (GConfClient *client,
			guint        cnxn_id,
			GConfEntry  *entry,
			gpointer     user_data)
{
	EogWindowPrivate *priv;
	gboolean interpolate = TRUE;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;

	if (!EOG_IS_SCROLL_VIEW (priv->scroll_view)) return;

	if (entry->value != NULL && entry->value->type == GCONF_VALUE_BOOL) {
		interpolate = gconf_value_get_bool (entry->value);
	}

	eog_scroll_view_set_antialiasing (EOG_SCROLL_VIEW (priv->scroll_view), interpolate);
}

static void
transparency_changed_cb (GConfClient *client,
			 guint        cnxn_id,
			 GConfEntry  *entry,
			 gpointer     user_data)
{
	EogWindowPrivate *priv;
	const char *value = NULL;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;
	if (!EOG_IS_SCROLL_VIEW (priv->scroll_view)) return;

	if (entry->value != NULL && entry->value->type == GCONF_VALUE_STRING) {
		value = gconf_value_get_string (entry->value);
	}

	if (g_strcasecmp (value, "COLOR") == 0) {
		GdkColor color;
		char *color_str;

		color_str = gconf_client_get_string (priv->client,
						     EOG_CONF_VIEW_TRANS_COLOR, NULL);
		if (gdk_color_parse (color_str, &color)) {
			eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->scroll_view),
							  TRANSP_COLOR, &color);
		}
	}
	else if (g_strcasecmp (value, "CHECK_PATTERN") == 0) {
		eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->scroll_view),
						  TRANSP_CHECKED, 0);
	}
	else {
		eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->scroll_view),
						  TRANSP_BACKGROUND, 0);
	}
}

static void
trans_color_changed_cb (GConfClient *client,
			guint        cnxn_id,
			GConfEntry  *entry,
			gpointer     user_data)
{
	EogWindowPrivate *priv;
	GdkColor color;
	char *value;
	const char *color_str;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	priv = EOG_WINDOW (user_data)->priv;
	if (!EOG_IS_SCROLL_VIEW (priv->scroll_view)) return;

	value = gconf_client_get_string (priv->client, EOG_CONF_VIEW_TRANSPARENCY, NULL);

	if (g_strcasecmp (value, "COLOR") != 0) return;

	if (entry->value != NULL && entry->value->type == GCONF_VALUE_STRING) {
		color_str = gconf_value_get_string (entry->value);

		if (gdk_color_parse (color_str, &color)) {
			eog_scroll_view_set_transparency (EOG_SCROLL_VIEW (priv->scroll_view),
							  TRANSP_COLOR, &color);
		}
	}
}

/* ========================================================================= */

#if 0
static void
activate_uri_cb (BonoboControlFrame *control_frame, const char *uri, gboolean relative, gpointer data)
{

	EogWindow *window;
	GList *list = NULL;
	
	g_return_if_fail (uri != NULL);

	window = EOG_WINDOW (eog_window_new ());
	
	list = g_list_prepend (list, g_strdup (uri));

	g_signal_emit (G_OBJECT (window), eog_window_signals[SIGNAL_OPEN_URI_LIST], 0, list);
}
#endif

static void
open_recent_cb (GtkWidget *widget, const EggRecentItem *item, gpointer data)
{
	EogWindow *window;
	char *uri;
	GList *list = NULL;

	window = EOG_WINDOW (data);

	uri = egg_recent_item_get_uri (item);
	
	list = g_list_prepend (list, uri);
	
	g_signal_emit (G_OBJECT (window), eog_window_signals[SIGNAL_OPEN_URI_LIST], 0, list);
}

static void
open_uri_list_cleanup (EogWindow *window, GSList *txt_uri_list)
{
	GSList *it;

	if (txt_uri_list != NULL) {

		for (it = txt_uri_list; it != NULL; it = it->next) {
			g_free ((char*)it->data);
		}
		
		g_slist_free (txt_uri_list);
	}
}


GType
eog_window_get_type (void) 
{
	static GType eog_window_type = 0;
	
	if (!eog_window_type) {
		static const GTypeInfo eog_window_info =
		{
			sizeof (EogWindowClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) eog_window_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (EogWindow),
			0,		/* n_preallocs */
			(GInstanceInitFunc) eog_window_init,
		};
		
		eog_window_type = g_type_register_static (GTK_TYPE_WINDOW, 
							  "EogWindow", 
							  &eog_window_info, 0);
	}

	return eog_window_type;
}

/* Destroy handler for windows */
static void
eog_window_destroy (GtkObject *object)
{
	EogWindow *window;
	EogWindowPrivate *priv;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_WINDOW (object));

	window = EOG_WINDOW (object);
	priv = window->priv;

	window_list = g_list_remove (window_list, window);

	if (priv->displayed_image != NULL) {
		g_object_unref (priv->displayed_image);
		priv->displayed_image = NULL;
	}

	if (priv->image_list != NULL) {
		g_object_unref (priv->image_list);
		priv->image_list = NULL;
	}

	if (priv->recent_view != NULL) {
		g_object_unref (priv->recent_view);
		priv->recent_view = NULL;
	}

	if (priv->recent_model != NULL) {
		g_object_unref (priv->recent_model);
		priv->recent_model = NULL;
	}

	if (priv->last_geometry != NULL) {
		g_free (priv->last_geometry);
		priv->last_geometry = NULL;
	}

	/* Clean up GConf-related stuff */
	if (priv->client) {
		gconf_client_notify_remove (priv->client, priv->interp_type_notify_id);
		gconf_client_notify_remove (priv->client, priv->transparency_notify_id);
		gconf_client_notify_remove (priv->client, priv->trans_color_notify_id);
		priv->interp_type_notify_id = 0;
		priv->transparency_notify_id = 0;
		priv->trans_color_notify_id = 0;

		gconf_client_remove_dir (priv->client, EOG_CONF_DIR, NULL);
		g_object_unref (G_OBJECT (priv->client));
		priv->client = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
eog_window_finalize (GObject *object)
{
	EogWindow *window;
	
	g_return_if_fail (EOG_IS_WINDOW (object));

	window = EOG_WINDOW (object);
	g_free (window->priv);
	window->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Class initialization function for windows */
static void
eog_window_class_init (EogWindowClass *class)
{
	GObjectClass   *gobject_class;
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	gobject_class = (GObjectClass *) class;
	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = g_type_class_peek_parent (class);
	
	eog_window_signals [SIGNAL_OPEN_URI_LIST] = 
		g_signal_new ("open_uri_list",
			      G_TYPE_FROM_CLASS(gobject_class),
			      G_SIGNAL_RUN_CLEANUP,
			      G_STRUCT_OFFSET (EogWindowClass, open_uri_list),
			      NULL,
			      NULL,
			      libeog_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);

	eog_window_signals [SIGNAL_NEW_WINDOW] = 
		g_signal_new ("new_window",
			      G_TYPE_FROM_CLASS(gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogWindowClass, new_window),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	object_class->destroy = eog_window_destroy;
	gobject_class->finalize = eog_window_finalize;

	widget_class->delete_event = eog_window_delete;
	widget_class->key_press_event = eog_window_key_press;
	widget_class->drag_data_received = eog_window_drag_data_received;
        widget_class->configure_event = eog_window_configure_event;
	widget_class->unrealize = eog_window_unrealize;

	class->open_uri_list = open_uri_list_cleanup;
}

/* Object initialization function for windows */
static void
eog_window_init (EogWindow *window)
{
	EogWindowPrivate *priv;
	char *role;

	priv = g_new0 (EogWindowPrivate, 1);
	window->priv = priv;

	role = gen_role ();
	gtk_window_set_role (GTK_WINDOW (window), role);
	g_free(role);
	
	priv->client = gconf_client_get_default ();

	gconf_client_add_dir (priv->client, EOG_CONF_DIR,
			      GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);

	priv->interp_type_notify_id =
		gconf_client_notify_add (priv->client,
					 EOG_CONF_VIEW_INTERPOLATE,
					 interp_type_changed_cb,
					 window, NULL, NULL);
	priv->transparency_notify_id =
		gconf_client_notify_add (priv->client,
					 EOG_CONF_VIEW_TRANSPARENCY,
					 transparency_changed_cb,
					 window, NULL, NULL);
	priv->trans_color_notify_id =
		gconf_client_notify_add (priv->client,
					 EOG_CONF_VIEW_TRANS_COLOR,
					 trans_color_changed_cb,
					 window, NULL, NULL);

	window_list = g_list_prepend (window_list, window);

	priv->desired_width = -1;
	priv->desired_height = -1;

	priv->image_list = NULL;
	priv->displayed_image = NULL;
	priv->sig_id_list_prepared = 0;

	g_object_set (G_OBJECT (window), "allow_shrink", TRUE, NULL);
}

/* delete_event handler for windows */
static gint
eog_window_delete (GtkWidget *widget, GdkEventAny *event)
{
	eog_window_close (EOG_WINDOW (widget));
	return TRUE;
}

/* Key press handler for windows */
static gint
eog_window_key_press (GtkWidget *widget, GdkEventKey *event)
{
	gint result;

	result = FALSE;

	if (GTK_WIDGET_CLASS (parent_class)->key_press_event)
		result = (* GTK_WIDGET_CLASS (parent_class)->key_press_event) (widget, event);

	if (result)
		return result;

	switch (event->keyval) {
	case GDK_Q:
	case GDK_q:
		eog_window_close (EOG_WINDOW (widget));
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

/* Returns whether a window has an image loaded in it */
gboolean
eog_window_has_contents (EogWindow *window)
{
	EogWindowPrivate *priv;
	gboolean has_contents = FALSE;

	g_return_val_if_fail (EOG_IS_WINDOW (window), FALSE);
	
	priv = window->priv;

	if (priv->image_list != NULL) {
		has_contents = (eog_image_list_length (priv->image_list) > 0);
	}
	
	return has_contents;
}

/* Drag_data_received handler for windows */
static void
eog_window_drag_data_received (GtkWidget *widget, 
			       GdkDragContext *context, 
			       gint x, gint y, 
			       GtkSelectionData *selection_data, 
			       guint info, guint time)
{
	GList *uri_list;
	GSList *str_list = NULL;
	GList *it;
	EogWindow *window;

	if (info != TARGET_URI_LIST) 
		return;

	if (context->suggested_action == GDK_ACTION_COPY) { 

		window = EOG_WINDOW (widget);
		
		uri_list = gnome_vfs_uri_list_parse (selection_data->data);
		
		for (it = uri_list; it != NULL; it = it->next) {
			char *filename = gnome_vfs_uri_to_string (it->data, GNOME_VFS_URI_HIDE_NONE);
			str_list = g_slist_prepend (str_list, filename);
		}
		
		gnome_vfs_uri_list_free (uri_list);
		/* FIXME: free string list */
		str_list = g_slist_reverse (str_list);

		g_signal_emit (G_OBJECT (window), eog_window_signals[SIGNAL_OPEN_URI_LIST], 0, str_list);
	}
}

/* Sets the window as a drag destination */
static void
set_drag_dest (EogWindow *window)
{
	static const GtkTargetEntry drag_types[] = {
		{ "text/uri-list", 0, TARGET_URI_LIST }
	};

	gtk_drag_dest_set (GTK_WIDGET (window),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
			   drag_types,
			   sizeof (drag_types) / sizeof (drag_types[0]),
			   GDK_ACTION_COPY | GDK_ACTION_ASK);
}

#if 0
static void
widget_realized_cb (GtkWidget *widget, gpointer data)
{
	adapt_window_size (EOG_WINDOW (data), 250, 250);
}
#endif

static void
update_ui_visibility (EogWindow *window)
{
	EogWindowPrivate *priv;
	int n_images = 0;
	gboolean show_info_pane = TRUE;
	GtkAction *action_fscreen;
	GtkAction *action_sshow;

	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	if (priv->image_list != NULL) {
		n_images = eog_image_list_length (priv->image_list);
	}

	action_fscreen = gtk_action_group_get_action (priv->actions_image, "ViewFullscreen");
	action_sshow = gtk_action_group_get_action (priv->actions_image, "ViewSlideshow");
	g_assert (action_fscreen != NULL);
	g_assert (action_sshow != NULL);
	
	if (n_images == 0) {
		/* update window content */
		gtk_widget_hide_all (priv->vpane);

		gtk_action_group_set_sensitive (priv->actions_window, TRUE);
		gtk_action_group_set_sensitive (priv->actions_image, FALSE);
		gtk_action_set_sensitive (action_fscreen, FALSE);
		gtk_action_set_sensitive (action_sshow, FALSE);
	}
	else if (n_images == 1) {
		/* update window content for single image mode */
		show_info_pane = gconf_client_get_bool (priv->client, EOG_CONF_UI_INFO_IMAGE, NULL);

		gtk_widget_show_all (priv->vpane);
		gtk_widget_hide_all (gtk_widget_get_parent (priv->wraplist));
		if (!show_info_pane) 
			gtk_widget_hide (priv->info_view);

		gtk_action_group_set_sensitive (priv->actions_window, TRUE);
		gtk_action_group_set_sensitive (priv->actions_image,  TRUE);
		
		/* Show Fullscreen option for single images only, and disable Slideshow */
		gtk_action_set_sensitive (action_fscreen, TRUE);
		gtk_action_set_sensitive (action_sshow, FALSE);
	}
	else {
		/* update window content for collection mode */
		show_info_pane = gconf_client_get_bool (priv->client, EOG_CONF_UI_INFO_COLLECTION, NULL);
		gtk_widget_show_all (priv->vpane);
		if (!show_info_pane)
			gtk_widget_hide (priv->info_view);

		gtk_action_group_set_sensitive (priv->actions_window, TRUE);
		gtk_action_group_set_sensitive (priv->actions_image,  TRUE);
		
		/* Show Slideshow option for collections only, and disable Fullscreen */
		gtk_action_set_sensitive (action_fscreen, FALSE);
		gtk_action_set_sensitive (action_sshow, TRUE);
	}

	/* update the toggle menu item for image information pane too */
	if (n_images > 0) {
		GtkAction *action;

		action = gtk_ui_manager_get_action (priv->ui_mgr, "/MainMenu/ViewMenu/InfoToggle");
		g_assert (action != NULL);

		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), show_info_pane);
	}
}

static void
get_window_decoration_size (EogWindow *window, int *width, int *height)
{
	int window_width, window_height;
	int view_width, view_height;
	GtkWidget *view;
	
	g_return_if_fail (EOG_IS_WINDOW (window));
	
	view = GTK_WIDGET (window->priv->scroll_view);
	
	if (!GTK_WIDGET_REALIZED (view))
		gtk_widget_realize (view);
	view_width  = view->allocation.width;
	view_height = view->allocation.height;

	if (!GTK_WIDGET_REALIZED (GTK_WIDGET (window))) 
		gtk_widget_realize (GTK_WIDGET (window));
	window_width  = GTK_WIDGET (window)->allocation.width;
	window_height = GTK_WIDGET (window)->allocation.height;
	
	*width = window_width - view_width;
	*height = window_height - view_height;
}

static void
obtain_desired_size (EogWindow *window, int screen_width, 
					 int screen_height, int *x11_flags, 
					 int *x, int *y, int *width, int *height)
{
	char *key;
	char *geometry_string;
	EogImageList *list;
	EogWindowMode mode;
	gboolean finished = FALSE;
	EogImage *img;
	
	list = window->priv->image_list;
	
	if (list != NULL && eog_image_list_length (list) == 1) {
		int img_width, img_height;
		int deco_width, deco_height;
		
		img = eog_image_list_get_img_by_pos (list, 0);
		g_assert (EOG_IS_IMAGE (img));
		eog_image_get_size (img, &img_width, &img_height);
		
		get_window_decoration_size (window, &deco_width, &deco_height);
		
		if (img_width > 0 && (img_height > 0)) {
			if ((img_width + deco_width > screen_width) ||
				(img_height + deco_height > screen_height))
			{
				double factor;
				if (img_width > img_height) {
					factor = (screen_width * 0.75 - deco_width) / (double) img_width;
				}
				else {
					factor = (screen_height * 0.75 - deco_height) / (double) img_height;
				}
				img_width = img_width * factor;
				img_height = img_height * factor;				
			}
			
			/* determine size of whole window */
			*width = img_width + deco_width;
			*height = img_height + deco_height;
#ifdef GDK_WINDOWING_X11
			*x11_flags = *x11_flags | WidthValue | HeightValue;
#endif
			finished = TRUE;			
		}

		g_object_unref (img);
	}
	
	if (!finished) {
		/* retrieve last saved geometry */
		mode = eog_window_get_mode (window);
		if (mode == EOG_WINDOW_MODE_COLLECTION)
			key = EOG_CONF_WINDOW_GEOMETRY_COLLECTION;
		else
			key = EOG_CONF_WINDOW_GEOMETRY_SINGLETON;
		
		geometry_string = gconf_client_get_string (window->priv->client,
												   key, NULL);
		/* parse resulting string */
		if (geometry_string == NULL) {
			*width = DEFAULT_WINDOW_WIDTH;
			*height = DEFAULT_WINDOW_HEIGHT;
#ifdef GDK_WINDOWING_X11
			*x11_flags = *x11_flags | WidthValue | HeightValue;
#endif
		}
		else {
#ifdef GDK_WINDOWING_X11
			*x11_flags = XParseGeometry (geometry_string,
									   x, y,
									   width, height);
#endif
		}
	}
}

static void
setup_initial_geometry (EogWindow *window)
{
	int x11_flags = 0;
	guint width, height;
	int x, y;
	GdkScreen *screen;
	int screen_width, screen_height;
	
	g_assert (EOG_IS_WINDOW (window));

	screen = gtk_window_get_screen (GTK_WINDOW (window));
	g_assert (screen != NULL);
	screen_width  = gdk_screen_get_width  (screen);
	screen_height = gdk_screen_get_height (screen);

	obtain_desired_size (window, screen_width, screen_height, &x11_flags,
						 &x, &y, &width, &height);
	
#ifdef GDK_WINDOWING_X11
	/* set position first */
	if ((x11_flags & XValue) && (x11_flags & YValue)) {
		int real_x = x;
		int real_y = y;
		
		/* This is sub-optimal. GDK doesn't allow us to set win_gravity
		* to South/East types, which should be done if using negative
		* positions (so that the right or bottom edge of the window
		* appears at the specified position, not the left or top).
		* However it does seem to be consistent with other GNOME apps.
		*/
		if (x11_flags & XNegative) {
			real_x = screen_width - real_x;
		}
		if (x11_flags & YNegative) {
			real_y = screen_height - real_y;
		}
		
		/* sanity check */
		real_x = CLAMP (real_x, 0, screen_width - 100);
		real_y = CLAMP (real_y, 0, screen_height - 100);
		
		gtk_window_move (GTK_WINDOW (window), real_x, real_y);
	}
	
	if ((x11_flags & WidthValue) && (x11_flags & HeightValue)) {
		/* make sure window is usable */
		width  = CLAMP (width, 100, screen_width);
		height = CLAMP (height, 100, screen_height);
	
		g_print ("setting window size: %i/%i\n", width, height);
		
		gtk_window_set_default_size (GTK_WINDOW (window), width, height);
	}
#endif
}

static void
update_status_bar (EogWindow *window)
{
	EogWindowPrivate *priv;
	int nimg;
	int nsel;
	char *n_img_str = NULL;
	char *str = NULL;

	priv = window->priv;

	/* update number of selected images */	
	nimg = eog_image_list_length (priv->image_list);
	if (nimg > 0) {
		nsel = eog_wrap_list_get_n_selected (EOG_WRAP_LIST (priv->wraplist));
		/* Images: (n_selected_images) / (n_total_images) */
		n_img_str = g_strdup_printf ("%i / %i", nsel, nimg);
	}
	gtk_label_set_text (GTK_LABEL (priv->n_img_label), n_img_str);
	
	if (priv->displayed_image != NULL) {
		int zoom, width, height;
		GnomeVFSFileSize bytes = 0;

		zoom = floor (100 * eog_scroll_view_get_zoom (EOG_SCROLL_VIEW (priv->scroll_view)));
		
		eog_image_get_size (priv->displayed_image, &width, &height);
		bytes = eog_image_get_bytes (priv->displayed_image);
		
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

	if (str != NULL) {
		gnome_appbar_set_status (GNOME_APPBAR (priv->statusbar), str);
		g_free (str);
	}
	else {
		gnome_appbar_set_status (GNOME_APPBAR (priv->statusbar), "");
	}
}

static void
view_zoom_changed_cb (GtkWidget *widget, double zoom, gpointer data)
{
	update_status_bar (EOG_WINDOW (data));
}

static void
show_error_dialog (EogWindow *window, char *header, GError *error) 
{
	GtkWidget *dlg;
	char *detail = NULL;

	g_return_if_fail (EOG_IS_WINDOW (window));

	if (header == NULL)
		return;
	
	if (error != NULL) {
		detail = g_strdup_printf (_("Reason: %s"), error->message);
	}

	dlg = eog_hig_dialog_new (GTK_WINDOW (window), 
				  GTK_STOCK_DIALOG_ERROR,
				  header, detail,
				  TRUE);

	gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);
	g_signal_connect_swapped (G_OBJECT (dlg), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  dlg);
	gtk_widget_show_all (dlg);
}

static void 
display_image_data (EogWindow *window, EogImage *image)
{
	EogWindowPrivate *priv;
	char *title = NULL;

	g_return_if_fail (EOG_IS_WINDOW (window));
	g_return_if_fail (EOG_IS_IMAGE (image));

	g_assert (eog_image_has_data (image, EOG_IMAGE_DATA_ALL));

	priv = window->priv;

	eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->scroll_view), image);
	eog_info_view_set_image (EOG_INFO_VIEW (priv->info_view), image);
	update_status_bar (window);

	if (priv->displayed_image != NULL)
		g_object_unref (priv->displayed_image);

	if (image != NULL) {
		priv->displayed_image = g_object_ref (image);
		title = eog_image_get_caption (image);
	}
	else {
		title = _("Eye of GNOME");		
	}

	gtk_window_set_title (GTK_WINDOW (window), title);		
}

/* this runs in its own thread */
static void
job_image_load_action (EogJob *job, gpointer data, GError **error)
{
	EogJobImageLoadData *job_data;

	job_data = (EogJobImageLoadData*) data;

	eog_image_load (EOG_IMAGE (job_data->image),
			EOG_IMAGE_DATA_ALL,
			job,
			error);
}

static void
job_image_load_finished (EogJob *job, gpointer data, GError *error)
{
	EogJobImageLoadData *job_data;
	EogWindow *window;
	EogImage *image;
	
	job_data = (EogJobImageLoadData*) data;
	window = EOG_JOB_DATA (job_data)->window;
	image = job_data->image;

	if (eog_job_get_status (job) == EOG_JOB_STATUS_FINISHED) {
		if (eog_job_get_success (job)) { 
			/* successfull */
			display_image_data (window, image);
		}
		else {  /* failed */
			char *header; 

			display_image_data (window, NULL);			

			header = g_strdup_printf (_("Image loading failed for %s"),
						  eog_image_get_caption (image));
			show_error_dialog (window, header, error);
			g_free (header);
		}
	}
	else if (eog_job_get_status (job) == EOG_JOB_STATUS_CANCELED) {
		/* actually nothing to do */
	}
	else {
		g_assert_not_reached ();
	}

	g_object_unref (image);
}

static void
job_image_load_cancel_job (EogJob *job, gpointer data)
{
	EogJobImageLoadData *job_data;

	job_data = (EogJobImageLoadData*) data;
	
	eog_image_cancel_load (EOG_IMAGE (job_data->image));
}

static void 
job_default_progress (EogJob *job, gpointer data, float progress)
{
	EogJobData *job_data;
	EogWindow *window;
	
	job_data = EOG_JOB_DATA (data);
	window = job_data->window;
	
	gnome_appbar_set_progress_percentage (GNOME_APPBAR (EOG_WINDOW (window)->priv->statusbar), progress);
}

static void
handle_image_selection_changed (EogWrapList *list, EogWindow *window) 
{
	EogWindowPrivate *priv;
	EogImage *image;
	EogJob *job;
	EogJobImageLoadData *data;

	priv = window->priv;

	image = eog_wrap_list_get_first_selected_image (EOG_WRAP_LIST (priv->wraplist));
	g_assert (EOG_IS_IMAGE (image));

	if (eog_image_has_data (image, EOG_IMAGE_DATA_ALL)) {
		display_image_data (window, image);
		return;
	}

	data = g_new0 (EogJobImageLoadData, 1);
	EOG_JOB_DATA (data)->window = window;
	data->image = image; /* no additional ref required, since 
			      * its already increased by 
			      * eog_wrap_lsit_get_first_selected_image
			      */
	
	job = eog_job_new_full (data,
				job_image_load_action,
				job_image_load_finished,
				job_image_load_cancel_job,
				job_default_progress,
				(EogJobFreeDataFunc) g_free);
	g_object_set (G_OBJECT (job), 
		      "progress-threshold", 0.25,
		      "priority", EOG_JOB_PRIORITY_HIGH, 
		      NULL);

	eog_job_manager_add (job); 
	g_object_unref (G_OBJECT (job));
}

typedef struct {
	char *stock_id;
	char *path;
} EogStockItems;

static EogStockItems eog_stock_items [] = {
	{ EOG_STOCK_ROTATE_90, "eog/stock-rotate-90-16.png" },
	{ EOG_STOCK_ROTATE_180, "eog/stock-rotate-180-16.png" },
	{ EOG_STOCK_ROTATE_270, "eog/stock-rotate-270-16.png" },
	{ EOG_STOCK_FLIP_VERTICAL, "eog/stock-flip-vertical-16.png" },
	{ EOG_STOCK_FLIP_HORIZONTAL, "eog/stock-flip-horizontal-16.png" },
	{ NULL, NULL }
};

static void
add_eog_icon_factory (void)
{
	GdkPixbuf *pixbuf;
	GtkIconFactory *factory;
	GtkIconSet *set;
	int i = 0; 
	GnomeProgram *program;

	factory = gtk_icon_factory_new ();
	program = gnome_program_get (); /* don't free this it's static */
	g_assert (program != NULL);

	while (eog_stock_items[i].stock_id != NULL) {
		EogStockItems item;
		char *filepath;

		item = eog_stock_items[i++];
		
		filepath = gnome_program_locate_file (program,
						      GNOME_FILE_DOMAIN_APP_PIXMAP,
						      item.path,
						      FALSE, NULL);
		if (filepath != NULL) {
			pixbuf = gdk_pixbuf_new_from_file (filepath, NULL);
			
			if (pixbuf != NULL) {
				set = gtk_icon_set_new_from_pixbuf (pixbuf);
				gtk_icon_factory_add (factory, item.stock_id, set);
				
				gtk_icon_set_unref (set);
				gdk_pixbuf_unref (pixbuf);
			}
			
			g_free (filepath);
		}
	}

	gtk_icon_factory_add_default (factory);
	g_object_unref (factory);
}

/* FIXME: The action and ui creation stuff should be moved to a separate file */
/* UI<->function mapping */
/* Normal items */
static GtkActionEntry action_entries_window[] = {
  { "FileMenu", NULL, N_("_File") },
  { "EditMenu", NULL, N_("_Edit") },
  { "ViewMenu", NULL, N_("_View") },
  { "HelpMenu", NULL, N_("_Help") },
  { "FileNewWindow",   GTK_STOCK_NEW,   N_("_New Window"),      "<control>N",  N_("Open a new window"),            G_CALLBACK (verb_FileNewWindow_cb) },
  { "FileOpen",        GTK_STOCK_OPEN,  N_("_Open..."),  "<control>O",  N_("Open a file"),                  G_CALLBACK (verb_FileOpen_cb) },
  { "FileFolderOpen",     GTK_STOCK_OPEN,  N_("Open _Folder..."), "<control><shift>O", N_("Open a folder"), G_CALLBACK (verb_FolderOpen_cb) },
  { "FileCloseWindow", GTK_STOCK_CLOSE, N_("_Close"),    "<control>W",  N_("Close window"),                 G_CALLBACK (verb_FileCloseWindow_cb) },
  { "EditPreferences", GTK_STOCK_PREFERENCES, N_("Prefere_nces"), NULL, N_("Preferences for Eye of GNOME"), G_CALLBACK (verb_EditPreferences_cb) },
  { "HelpManual",      GTK_STOCK_HELP,  N_("_Contents"), "F1",          N_("Help On this application"),     G_CALLBACK (verb_HelpContent_cb) },
  { "HelpAbout",       GTK_STOCK_ABOUT, N_("_About"),	NULL, N_("About this application"),       G_CALLBACK (verb_HelpAbout_cb) }
};

/* Toggle items */
static GtkToggleActionEntry toggle_entries_window[] = {
  { "ViewToolbar",   NULL, N_("_Toolbar"),   NULL, N_("Changes the visibility of the toolbar in the current window"),   G_CALLBACK (verb_ShowHideAnyBar_cb), TRUE },
  { "ViewStatusbar", NULL, N_("_Statusbar"), NULL, N_("Changes the visibility of the statusbar in the current window"), G_CALLBACK (verb_ShowHideAnyBar_cb), TRUE },
};


static GtkActionEntry action_entries_image[] = {
  { "FileSave", GTK_STOCK_SAVE, N_("_Save"), "<control>s", NULL, G_CALLBACK (verb_Save_cb) },
  { "FileSaveAs", GTK_STOCK_SAVE_AS, N_("Save _As"), "<control><shift>s", NULL, G_CALLBACK (verb_SaveAs_cb) },

  { "EditUndo", NULL, N_("_Undo"), "<control>z", NULL, G_CALLBACK (verb_Undo_cb) },

  { "EditFlipHorizontal", EOG_STOCK_FLIP_HORIZONTAL, N_("Flip _Horizontal"), NULL, NULL, G_CALLBACK (verb_FlipHorizontal_cb) },
  { "EditFlipVertical", EOG_STOCK_FLIP_VERTICAL, N_("Flip _Vertical"), NULL, NULL, G_CALLBACK (verb_FlipVertical_cb) },

  { "EditRotate90",  EOG_STOCK_ROTATE_90,  N_("_Rotate Clockwise"), "<control>r", NULL, G_CALLBACK (verb_Rotate90_cb) },
  { "EditRotate270", EOG_STOCK_ROTATE_270, N_("Rotate Counter C_lockwise"), NULL, NULL, G_CALLBACK (verb_Rotate270_cb) },
  { "EditRotate180", EOG_STOCK_ROTATE_180, N_("Rotat_e 180\xC2\xB0"), "<control><shift>r", NULL, G_CALLBACK (verb_Rotate180_cb) },

  { "EditDelete", GTK_STOCK_DELETE, N_("Delete"), "Delete", NULL, G_CALLBACK (verb_Delete_cb) },
 
  { "ViewFullscreen", NULL, N_("_Full Screen"), "F11", NULL, G_CALLBACK (verb_FullScreen_cb) },
  { "ViewSlideshow", NULL, N_("_Slideshow"), "F11", NULL, G_CALLBACK (verb_FullScreen_cb) },
  { "ViewZoomIn", GTK_STOCK_ZOOM_IN, N_("_Zoom In"), "<control>plus", NULL, G_CALLBACK (verb_ZoomIn_cb) },
  { "ViewZoomOut", GTK_STOCK_ZOOM_OUT, N_("Zoom _Out"), "<control>minus", NULL, G_CALLBACK (verb_ZoomOut_cb) },
  { "ViewZoomNormal", GTK_STOCK_ZOOM_100, N_("_Normal Size"), "<control>equal", NULL, G_CALLBACK (verb_ZoomNormal_cb) },
  { "ViewZoomFit", GTK_STOCK_ZOOM_FIT, N_("Best _Fit"), NULL, NULL, G_CALLBACK (verb_ZoomFit_cb) }
};


static GtkToggleActionEntry toggle_entries_image[] = {
  { "ViewInfo",      NULL, N_("Image _Information"), NULL, N_("Change the visibility of the information pane in the current window"), G_CALLBACK (verb_ShowHideAnyBar_cb), TRUE }
};

typedef struct {
	char *action_name;
	char *short_label;
} ShortLabelMap;

/* mapping for short-labels, used for toolbar buttons */
static ShortLabelMap short_label_map[] = {
	{ "FileNewWindow", N_("New") },
	{ "FileOpen",  N_("Open") },
	{ "FileFolderOpen",  N_("Open") },
	{ "FileCloseWindow", N_("Close") },
	{ "FileSave", N_("Save") },
	{ "FileSaveAs", N_("Save As") },
	{ "EditUndo",  N_("Undo") },
	{ "EditFlipHorizontal", NULL },
	{ "EditFlipVertical", NULL },
	{ "EditRotate90", N_("Right") },
	{ "EditRotate270", N_("Left") },
	{ "ViewFullscreen", NULL },
	{ "ViewSlideshow", NULL },
	{ "ViewZoomIn", N_("In") },
	{ "ViewZoomOut", N_("Out") },
	{ "ViewZoomNormal", N_("Normal") },
	{ "ViewZoomFit", N_("Fit") },
	{ NULL, NULL }
};

static void
add_short_labels (GtkActionGroup *group) 
{
	GtkAction *action;
	char *translated_string;	
	int i;

	for (i = 0; short_label_map[i].action_name != NULL; i++) {
		action = gtk_action_group_get_action (group,
						      short_label_map[i].action_name);
		if (action != NULL) {
			translated_string = gettext (short_label_map[i].short_label);
			
			g_object_set (G_OBJECT (action), "short-label",
				          translated_string, NULL);
		}
	}
}

static char*
get_ui_description_file () {
	/* For development purpose only: Use ui file in current 
	 * directory if its exists.
	 */
	char *cd = g_get_current_dir ();
	char *filename = g_build_filename (cd, "eog-gtk-ui.xml", NULL);
	g_free (cd);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		g_free (filename);
	
		/* find and setup UI description */
		filename = gnome_program_locate_file (NULL,
											  GNOME_FILE_DOMAIN_APP_DATADIR,
											  "eog/eog-gtk-ui.xml",
											  FALSE, NULL);
	}
	
	return filename;
}

/**
 * window_construct:
 * @window: A window widget.
 *
 * Constructs the window widget.
 **/
static gboolean
eog_window_construct_ui (EogWindow *window, GError **error)
{
	EogWindowPrivate *priv;
	gboolean visible;
	GtkWidget *menubar;
	GtkWidget *toolbar;
	GtkWidget *recent_widget;
	GtkAction *action;
	GtkWidget *sw;
	GtkWidget *frame;
	char *filename;
	guint merge_id;

	g_return_val_if_fail (window != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_WINDOW (window), FALSE);

	priv = window->priv;

	priv->ui_mgr = gtk_ui_manager_new ();
	priv->box = GTK_WIDGET (gtk_vbox_new (FALSE, 0));
	gtk_container_add (GTK_CONTAINER (window), priv->box);

	add_eog_icon_factory ();
	
	/* build menu and toolbar */
	priv->actions_window = gtk_action_group_new ("MenuActionsWindow");
	gtk_action_group_set_translation_domain (priv->actions_window, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (priv->actions_window, action_entries_window, G_N_ELEMENTS (action_entries_window), window);
	gtk_action_group_add_toggle_actions (priv->actions_window, toggle_entries_window, G_N_ELEMENTS (toggle_entries_window), window);
	add_short_labels (priv->actions_window);
	gtk_ui_manager_insert_action_group (priv->ui_mgr, priv->actions_window, 0);

	priv->actions_image = gtk_action_group_new ("MenuActionsImage");
	gtk_action_group_set_translation_domain (priv->actions_image, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (priv->actions_image, action_entries_image, G_N_ELEMENTS (action_entries_image), window);
	gtk_action_group_add_toggle_actions (priv->actions_image, toggle_entries_image, G_N_ELEMENTS (toggle_entries_image), window);
	add_short_labels (priv->actions_image);
	gtk_ui_manager_insert_action_group (priv->ui_mgr, priv->actions_image, 0);

	filename = get_ui_description_file ();

	if (filename == NULL) {
		g_set_error (error, EOG_WINDOW_ERROR, 
			     EOG_WINDOW_ERROR_UI_NOT_FOUND,
			     _("User interface description not found."));
		return FALSE;
	}

	merge_id = gtk_ui_manager_add_ui_from_file (priv->ui_mgr, filename, error);
	g_free (filename);
	if (merge_id == 0)
		return FALSE;

	menubar = gtk_ui_manager_get_widget (priv->ui_mgr, "/MainMenu");
	g_assert (GTK_IS_WIDGET (menubar));
	gtk_box_pack_start (GTK_BOX (priv->box), menubar, FALSE, FALSE, 0);

	toolbar = gtk_ui_manager_get_widget (priv->ui_mgr, "/ToolBar");
	g_assert (toolbar != NULL);
	gtk_box_pack_start (GTK_BOX (priv->box), toolbar, FALSE, FALSE, 0);

	gtk_window_add_accel_group (GTK_WINDOW (window), gtk_ui_manager_get_accel_group (priv->ui_mgr));

	/* recent files support */
	priv->recent_model = egg_recent_model_new (EGG_RECENT_MODEL_SORT_MRU);
	egg_recent_model_set_filter_groups (priv->recent_model, RECENT_FILES_GROUP, NULL);
	egg_recent_model_set_limit (priv->recent_model, 5);
	
	recent_widget = gtk_ui_manager_get_widget (priv->ui_mgr, "/MainMenu/FileMenu/EggRecentDocuments");
	priv->recent_view = egg_recent_view_gtk_new (gtk_widget_get_parent (recent_widget),
						     recent_widget);
	egg_recent_view_gtk_set_trailing_sep (priv->recent_view, TRUE);

	egg_recent_view_set_model (EGG_RECENT_VIEW (priv->recent_view), priv->recent_model);
	g_signal_connect (G_OBJECT (priv->recent_view), "activate",
			  G_CALLBACK (open_recent_cb), window);

	/* add statusbar */
	priv->statusbar = gnome_appbar_new (TRUE, TRUE, GNOME_PREFERENCES_NEVER);
	{
		GtkWidget *frame;
		frame = g_object_new (GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_IN, NULL);
		priv->n_img_label = gtk_label_new ("       ");
		gtk_container_add (GTK_CONTAINER (frame), priv->n_img_label);
		gtk_box_pack_start (GTK_BOX (priv->statusbar), frame, FALSE, TRUE, 0);
	}

	gtk_box_pack_end (GTK_BOX (priv->box), GTK_WIDGET (priv->statusbar),
			  FALSE, FALSE, 0);

	/* content display */
	priv->vpane = gtk_vpaned_new (); /* eog_vertical_splitter_new (); */

	/* the image view for the full size image */
 	priv->scroll_view = eog_scroll_view_new ();
	gtk_widget_set_usize (GTK_WIDGET (priv->scroll_view), 100, 100);
	g_signal_connect (G_OBJECT (priv->scroll_view),
			  "zoom_changed",
			  (GCallback) view_zoom_changed_cb, window);

	frame = gtk_widget_new (GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_IN, NULL);
	gtk_container_add (GTK_CONTAINER (frame), priv->scroll_view);

	/* Create an info view widget to view additional image information and 
	 * put it to left of the image view. Using an eog_horizontal_splitter for this. 
	 */
	priv->info_view = gtk_widget_new (EOG_TYPE_INFO_VIEW, NULL);

	/* left side holds the image view, right side the info view */
	priv->hpane = gtk_hpaned_new (); /* eog_horizontal_splitter_new ();  */
	gtk_paned_pack1 (GTK_PANED (priv->hpane), frame, TRUE, TRUE);
	gtk_paned_pack2 (GTK_PANED (priv->hpane), priv->info_view, TRUE, TRUE);

	gtk_paned_pack1 (GTK_PANED (priv->vpane), priv->hpane, TRUE, TRUE);

	/* the wrap list for all the thumbnails */
	priv->wraplist = eog_wrap_list_new ();
	/* g_object_set (G_OBJECT (priv->wraplist), 
		      "height_request", 200, 
		      "width_request", 500,
		      NULL);*/
	eog_wrap_list_set_col_spacing (EOG_WRAP_LIST (priv->wraplist), 20);
	eog_wrap_list_set_row_spacing (EOG_WRAP_LIST (priv->wraplist), 20);
	g_signal_connect (G_OBJECT (priv->wraplist), "selection_changed",
			  G_CALLBACK (handle_image_selection_changed), window);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), priv->wraplist);
	
	gtk_paned_pack2 (GTK_PANED (priv->vpane), sw, TRUE, TRUE);

	/* by default make the wrap list keyboard active */
	/* gtk_widget_grab_focus (priv->wraplist); */

	gtk_box_pack_start (GTK_BOX (priv->box), priv->vpane, TRUE, TRUE, 0);

	set_drag_dest (window);

	gtk_widget_show_all (GTK_WIDGET (priv->box));

	/* show/hide toolbar? */
	visible = gconf_client_get_bool (priv->client, EOG_CONF_UI_TOOLBAR, NULL);
	action = gtk_ui_manager_get_action (priv->ui_mgr, "/MainMenu/ViewMenu/ToolbarToggle");
	g_assert (action != NULL);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);
	g_object_set (G_OBJECT (toolbar), "visible", visible, NULL);

	/* show/hide statusbar? */
	visible = gconf_client_get_bool (priv->client, EOG_CONF_UI_STATUSBAR, NULL);
	action = gtk_ui_manager_get_action (priv->ui_mgr, "/MainMenu/ViewMenu/StatusbarToggle");
	g_assert (action != NULL);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);
	g_object_set (G_OBJECT (priv->statusbar), "visible", visible, NULL);

	return TRUE;
}

static void
eog_window_update_properties (EogWindow *window)
{
	GConfEntry *entry;
	EogWindowPrivate *priv;

	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	entry = gconf_client_get_entry (priv->client, EOG_CONF_VIEW_INTERPOLATE, NULL, TRUE, NULL);
	if (entry != NULL) {
		interp_type_changed_cb (priv->client, 0, entry, window);
		gconf_entry_free (entry);
		entry = NULL;
	}

	entry = gconf_client_get_entry (priv->client, EOG_CONF_VIEW_TRANSPARENCY, NULL, TRUE, NULL);
	if (entry != NULL) {
		transparency_changed_cb (priv->client, 0, entry, window);
		gconf_entry_free (entry);
		entry = NULL;
	}

	entry = gconf_client_get_entry (priv->client, EOG_CONF_VIEW_TRANS_COLOR, NULL, TRUE, NULL);
	if (entry != NULL) {
		trans_color_changed_cb (priv->client, 0, entry, window);
		gconf_entry_free (entry);
		entry = NULL;
	}
}

/**
 * eog_window_new:
 * @void:
 *
 * Creates a new main window.
 *
 * Return value: A newly-created main window.
 **/
GtkWidget *
eog_window_new (GError **error)
{
	EogWindow *window;

	window = EOG_WINDOW (g_object_new (EOG_TYPE_WINDOW, "title", _("Eye of GNOME"), NULL));

	if (eog_window_construct_ui (window, error)) {
		eog_window_update_properties (window);
	}

	return GTK_WIDGET (window);
}

/**
 * window_close:
 * @window: A window.
 *
 * Closes a window with confirmation, and exits the main loop if this was the
 * last window in the list.
 **/
void
eog_window_close (EogWindow *window)
{
	g_return_if_fail (window != NULL);
	g_return_if_fail (EOG_IS_WINDOW (window));

	gtk_widget_destroy (GTK_WIDGET (window));

	if (!window_list)
		gtk_main_quit ();
}

static void
adapt_window_size (EogWindow *window, int width, int height)
{
#if 0
	int xthick, ythick;
	int req_height, req_width;
	int sb_height;
	gboolean sb_visible;
#endif

	EogWindowPrivate *priv;
	int new_height;
	int new_width;
	int sw, sh;

	priv = window->priv;
	
	new_width = GTK_WIDGET (window)->allocation.width - 
		GTK_WIDGET (window->priv->scroll_view)->allocation.width +
		width;

	new_height = GTK_WIDGET (window)->allocation.height - 
		GTK_WIDGET (window->priv->scroll_view)->allocation.height + 
		height;

	sw = gdk_screen_width (); /* FIXME: Multihead issue? */
	sh = gdk_screen_height ();
	
	if ((new_width >= sw) || (new_height >= sh)) {
		double factor;
		if (new_width > new_height) {
			factor = (sw * 0.75) / (double) new_width;
		}
		else {
			factor = (sh * 0.75) / (double) new_height;
		}
		new_width = new_width * factor;
		new_height = new_height * factor;
	}


	gtk_window_resize (GTK_WINDOW (window), new_width, new_height);

#if 0
	/* check if the statusbar is visible */
	g_object_get (G_OBJECT (priv->statusbar), "visible", &sb_visible, NULL);

	if ((priv->desired_width > 0) && (priv->desired_height > 0) &&
	    (!sb_visible || GTK_WIDGET_REALIZED (priv->statusbar)) &&
	    GTK_WIDGET_REALIZED (priv->box) &&
	    GTK_WIDGET_REALIZED (GTK_WIDGET (window)))
	{
		/* this is the size of the frame around the vbox */
		xthick = priv->box->style->xthickness;
		ythick = priv->box->style->ythickness;
		req_width = req_height = -1;
		
		if (sb_visible) {
			sb_height = priv->statusbar->allocation.height;
		}
		else {
			sb_height = 0;
		}

		req_height = 
			priv->desired_height + 
			(GTK_WIDGET(window)->allocation.height - priv->box->allocation.height) +
		        sb_height + 2 * ythick;
		
		req_width = 
			priv->desired_width + 
			(GTK_WIDGET(window)->allocation.width - priv->box->allocation.width) +
			2 * xthick;

		gtk_window_resize (GTK_WINDOW (window), req_width, req_height);
	}
#endif
}

static void
add_uri_to_recent_files (EogWindow *window, GnomeVFSURI *uri)
{
	EggRecentItem *recent_item;
	char *text_uri;

	g_return_if_fail (EOG_IS_WINDOW (window));
	if (uri == NULL) return;

	text_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	if (text_uri == NULL)
		return;

	recent_item = egg_recent_item_new_from_uri (text_uri);
	egg_recent_item_add_group (recent_item, RECENT_FILES_GROUP);
	egg_recent_model_add_full (window->priv->recent_model, recent_item);
	egg_recent_item_unref (recent_item);

	g_free (text_uri);
}

/**
 * eog_window_open:
 * @window: A window.
 * @model: A list of EogImage objects.
 * @error: An pointer to an error object or NULL.
 *
 * Loads the uri into the window.
 *
 * Return value: TRUE on success, FALSE otherwise.
 **/
gboolean
eog_window_open (EogWindow *window, EogImageList *model, GError **error)
{
	EogWindowPrivate *priv;

	g_return_val_if_fail (EOG_IS_WINDOW (window), FALSE);
	
	priv = window->priv;

#ifdef DEBUG
	g_print ("EogWindow.c: eog_window_open\n");
#endif
	
	/* attach image list */
	if (priv->image_list != NULL) {
		g_signal_handler_disconnect (G_OBJECT (priv->image_list), priv->sig_id_list_prepared);
		g_object_unref (priv->image_list);
		priv->image_list = NULL;
	}
	
	if (model != NULL) {
		g_object_ref (model);
		priv->image_list = model;
	}

	/* update ui */
	update_ui_visibility (window);
	
	if (!GTK_WIDGET_MAPPED (GTK_WIDGET (window))) {
		setup_initial_geometry (window);
	}
		
	/* attach model to view */
	eog_wrap_list_set_model (EOG_WRAP_LIST (priv->wraplist), EOG_IMAGE_LIST (priv->image_list));
	
	/* update recent files */
#if 0
	add_uri_to_recent_files (window, uri);
#endif

	return TRUE;
}

/**
 * eog_get_window_list:
 *
 * Gets a list of all #EogWindow objects.  You should not modify this list at
 * all; it should be considered read-only.
 * 
 * Return value: A list with all the active #EogWindow objects.
 **/
GList *
eog_get_window_list (void)
{
	return window_list;
}

/**
 * eog_window_get_uri:
 * @eog_window: A shell window.
 * 
 * Queries the URI that is being displayed by the specified window.  If the
 * window is not displaying a single image or folder, this will return NULL.
 * 
 * Return value: The URI that is being displayed.
 **/
const char *
eog_window_get_uri (EogWindow *eog_window)
{
#if 0
	EogWindowPrivate *priv;

	g_return_val_if_fail (EOG_IS_WINDOW (eog_window), NULL);

	priv = eog_window->priv;
	return priv->uri;
#else
	return NULL;
#endif
}

/**
 * eog_window_close_all:
 * 
 * Closes all EOG windows, causing the program to exit.
 **/
void
eog_window_close_all (void)
{
	while (1) {
		GList *l;
		EogWindow *window;

		l = window_list;
		if (!l)
			break;

		window = EOG_WINDOW (l->data);
		eog_window_close (window);
	}
}
