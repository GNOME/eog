/* Eye of Gnome image viewer - main window widget
 *
 * Copyright (C) 2000 The Free Software Foundation
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
#include "eog-image-list.h"
#include "eog-full-screen.h"
#include "eog-save-dialog-helper.h"
#include "eog-image-save-info.h"
#include "eog-hig-dialog.h"
#include "eog-uri-converter.h"
#include "eog-save-as-dialog-helper.h"
#include "eog-pixbuf-util.h"

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
#define NO_SAVE_DEBUG

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

static int eog_window_signals [SIGNAL_LAST];

static void eog_window_class_init (EogWindowClass *class);
static void eog_window_init (EogWindow *window);

static gint eog_window_delete (GtkWidget *widget, GdkEventAny *event);
static gint eog_window_key_press (GtkWidget *widget, GdkEventKey *event);
static void eog_window_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y, 
				    GtkSelectionData *selection_data, guint info, guint time);
static void adapt_window_size (EogWindow *window, int width, int height);
static void update_status_bar (EogWindow *window);

static GtkWindowClass *parent_class;

/* The list of all open windows */
static GList *window_list = NULL;

/* Drag target types */
enum {
	TARGET_URI_LIST
};

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

/* Brings attention to a window by raising it and giving it focus */
static void
raise_and_focus (GtkWidget *widget)
{
	g_assert (GTK_WIDGET_REALIZED (widget));
	gdk_window_show (widget->window);
	gtk_widget_grab_focus (widget);
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
	static GtkWidget *about;
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



	if (!about) {
		GdkPixbuf *pixbuf;

		/* Translators should localize the following string
		 * which will give them credit in the About box.
		 * E.g. "Fulano de Tal <fulano@detal.com>"
		 */
		translators = _("translator-credits");

		pixbuf = gdk_pixbuf_new_from_file (EOG_ICONDIR "/gnome-eog.png", NULL);

		about = gnome_about_new (
			_("Eye of Gnome"),
			VERSION,
			"Copyright \xc2\xa9 2000-2004 Free Software Foundation, Inc.",
			_("The GNOME image viewing and cataloging program."),
			authors,
			documenters,
			(strcmp (translators, "translator-credits")
			 ? translators : NULL),
			pixbuf);

		if (pixbuf)
			g_object_unref (pixbuf);

		g_signal_connect (about, "destroy",
				  G_CALLBACK (gtk_widget_destroyed),
				  &about);
	}

	gtk_widget_show_now (about);
	raise_and_focus (about);
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
						 _("Could not display help for Eye of Gnome.\n"
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
	GList     *current;
	EogImageSaveInfo *source;
	EogImageSaveInfo *target;
	EogURIConverter  *conv;

	/* dialog handling */
	EogWindow  *window;
	GtkWindow  *dlg;
	EogSaveResponse response;

	/* in case of error on current image this
	 * is set. */
	GError    *error;
	gboolean   cancel_save;

	/* thread machinerie */
	GThread   *thread;
	GCond     *wait;
	GMutex    *lock;
} SaveData;

static gboolean
save_dialog_update (SaveData *data)
{
	EogImage *image = NULL;
	EogImageSaveInfo *source = NULL;
	EogImageSaveInfo *target = NULL;
	
	g_mutex_lock (data->lock);
	if (data->current != NULL) {
		image = EOG_IMAGE (data->current->data);
	}
	if (data->source != NULL) {
		source = g_object_ref (data->source);
	}
	if (data->target != NULL) {
		target = g_object_ref (data->target);
	}
	g_mutex_unlock (data->lock);

	eog_save_dialog_update (data->dlg,
				data->n_processed,
				image,
				source,
				target);

	if (source != NULL) 
		g_object_unref (source);
	if (target != NULL) 
		g_object_unref (target);

	return FALSE;
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
save_error (SaveData *data)
{
	GtkWidget *dlg;
	EogImage  *image;
	char *header;
	char *detail = NULL;
	int   response;
	gint  err_code = 0;
	
	g_mutex_lock (data->lock);
	image = EOG_IMAGE (data->current->data);
	if (data->error != NULL) {
		detail   = data->error->message;
		err_code = data->error->code;
	}
	g_mutex_unlock (data->lock);

	/* display generic error dialog, except for FILE_EXISTS error */
	if (err_code == EOG_IMAGE_ERROR_FILE_EXISTS) {
		char *str;

		str = eog_image_get_uri_for_display (image); 

		header = g_strdup_printf (_("Overwrite file %s?"), str);
		detail = _("File exists. Do you want to overwrite it?");

		dlg = eog_hig_dialog_new (GTK_WINDOW (data->window), 
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
		header = g_strdup_printf (_("Error on saving %s."), eog_image_get_caption (image));
		
		dlg = eog_hig_dialog_new (GTK_WINDOW (data->window), 
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
	
	g_mutex_lock (data->lock);
	if (response == EOG_SAVE_RESPONSE_CANCEL)
		data->cancel_save = TRUE;
	if (response == EOG_SAVE_RESPONSE_OVERWRITE && 
	    data->target != NULL)
	{
		data->target->overwrite = TRUE;
	}
	data->response = (EogSaveResponse) response;
	g_mutex_unlock (data->lock);

	g_cond_broadcast (data->wait);

	return FALSE;
}

static gboolean
save_finished (SaveData *data)
{
	eog_save_dialog_close (data->dlg, !data->cancel_save);

	g_mutex_free (data->lock);
	g_cond_free (data->wait);

	g_list_foreach (data->images, (GFunc) g_object_unref, NULL);
	g_list_free (data->images);

	if (data->source)
		g_object_unref (data->source);

	if (data->target)
		g_object_unref (data->target);

	if (data->conv)
		g_object_unref (data->conv);

	/* enable image modification functions */
	gtk_action_group_set_sensitive (data->window->priv->actions_image,  TRUE);

	g_free (data);

	return FALSE;
}

static void
save_cancel (GtkWidget *button, SaveData *data)
{
	g_mutex_lock (data->lock);
	data->cancel_save = TRUE;
	g_mutex_unlock (data->lock);

	eog_save_dialog_cancel (data->dlg);
}

/* this runs in its own thread */
static gpointer
save_image_list (gpointer user_data)
{
	SaveData *data; 

	data = (SaveData*) user_data;

	g_mutex_lock (data->lock);
	data->current = data->images;
	data->n_processed = 0;
	g_mutex_unlock (data->lock);
	
	while (data->current != NULL && !data->cancel_save) {
		EogImageSaveInfo *info;
		EogImage *image;
		GError *error = NULL;
		gboolean success = FALSE;
		
		g_mutex_lock (data->lock);
		image = EOG_IMAGE (data->current->data);
		g_mutex_unlock (data->lock);
		
		info = eog_image_save_info_from_image (image);

		g_mutex_lock (data->lock);
		if (data->source != NULL)
			g_object_unref (data->source);
		data->source = info;
		data->target = NULL;
		g_mutex_unlock (data->lock);

		g_idle_add ((GSourceFunc) save_dialog_update, data);
		
		while (!success && !data->cancel_save) {
#ifdef SAVE_DEBUG
			g_print ("Save image at %s ", 
				 gnome_vfs_uri_to_string (data->source->uri, GNOME_VFS_URI_HIDE_NONE));
#endif
			success = eog_image_load_sync (image, EOG_IMAGE_LOAD_DEFAULT);
			
			if (success) {
				success = eog_image_save_by_info (image, data->source, &error);
			}
#ifdef SAVE_DEBUG
			g_print ("successful: %i\n", success);
#endif
			
			if (!success && !data->cancel_save) {
				g_mutex_lock (data->lock);
				data->error = g_error_copy (error);
				data->response = EOG_SAVE_RESPONSE_NONE;
				
				g_idle_add ((GSourceFunc) save_error, data);
				g_cond_wait (data->wait, data->lock);

				if (data->response == EOG_SAVE_RESPONSE_SKIP)
					success = TRUE;

				g_error_free (data->error);
				data->error = NULL;

				g_mutex_unlock (data->lock);
			}

			if (success) {
				g_idle_add ((GSourceFunc) save_update_image, g_object_ref (image));
			}
		}

		g_mutex_lock (data->lock);
		data->current = data->current->next;
		data->n_processed++;
		g_mutex_unlock (data->lock);
	}

	g_idle_add ((GSourceFunc) save_finished, data);

	return NULL;
}

static void
verb_Save_cb (GtkAction *action, gpointer user_data)
{
	EogWindowPrivate *priv;
	int n_images;
	SaveData *data;
	GtkWidget *button;

	priv = EOG_WINDOW (user_data)->priv;

	n_images = eog_wrap_list_get_n_selected (EOG_WRAP_LIST (priv->wraplist));
	if (n_images == 0) return;

	data = g_new0 (SaveData, 1);
	g_assert (data != NULL);

	/* initialise data struct */
	data->images      = eog_wrap_list_get_selected_images (EOG_WRAP_LIST (priv->wraplist));
	data->n_images    = n_images;
	data->n_processed = 0;
	
	data->current = NULL;
	data->source  = data->target = NULL;
	data->conv = NULL;

	data->response = EOG_SAVE_RESPONSE_NONE;

	data->error       = NULL;
	data->cancel_save = FALSE;

	data->wait = g_cond_new ();
	data->lock = g_mutex_new ();

	data->window = EOG_WINDOW (user_data);
	data->dlg = GTK_WINDOW (eog_save_dialog_new (GTK_WINDOW (user_data), data->n_images));
	button = eog_save_dialog_get_button (GTK_WINDOW (data->dlg));
	g_signal_connect (G_OBJECT (button), "clicked", (GCallback) save_cancel, data);

	/* disable image modification functions */
	gtk_action_group_set_sensitive (priv->actions_image,  FALSE);

	gtk_widget_show_all (GTK_WIDGET (data->dlg));
	gtk_widget_show_now (GTK_WIDGET (data->dlg));

	data->thread = g_thread_create (save_image_list, data, TRUE, NULL);
}

/* this runs in its own thread */
static gpointer
save_as_image_list (gpointer user_data)
{
	SaveData *data; 

	data = (SaveData*) user_data;

	g_mutex_lock (data->lock);
	data->current = data->images;
	data->n_processed = 0;
	g_mutex_unlock (data->lock);
	
	g_assert (data->conv != NULL || data->target != NULL);
	
	while (data->current != NULL && !data->cancel_save) {
		EogImageSaveInfo *sinfo;
		EogImageSaveInfo *tinfo = NULL;
		EogImage *image;
		GError *error = NULL;
		gboolean success = FALSE;
		
		g_mutex_lock (data->lock);
		image = EOG_IMAGE (data->current->data);
		g_mutex_unlock (data->lock);

		/* obtain target information */
		if (data->conv != NULL) {
			GdkPixbufFormat *format;
			GnomeVFSURI *target_uri;
			gboolean result;
			GError *error = NULL;

			result = eog_uri_converter_do (data->conv,
						       image, 
						       &target_uri,
						       &format,
						       &error);
#ifdef SAVE_DEBUG
			g_print ("convert uri: %s\n", gnome_vfs_uri_to_string (target_uri, GNOME_VFS_URI_HIDE_NONE));
#endif

			if (result) {
				tinfo = eog_image_save_info_from_vfs_uri (target_uri,
									  format);
			}
				
			if (target_uri)
				gnome_vfs_uri_unref (target_uri);
		}

		if (tinfo == NULL  && data->target != NULL) {
			tinfo = g_object_ref (data->target);
		}

		/* This may leave sinfo->format_type as NULL, therefor we
		 * repeat this call after actually loading the image.
		 * Here we only need it for updating the dialog.
		 */
		sinfo = eog_image_save_info_from_image (image);

		/* update dialog */
		g_mutex_lock (data->lock);
		if (data->target != NULL) 
			g_object_unref (data->target);
		data->target = tinfo;
		if (data->source != NULL)
			g_object_unref (data->source);
		data->source = sinfo;
		g_mutex_unlock (data->lock);
		g_idle_add ((GSourceFunc) save_dialog_update, data);
		
		/* try to save image source to target */
		success = FALSE;
		while (!success && !data->cancel_save) {
#ifdef SAVE_DEBUG
			g_print ("Save image from: %s to: %s\n", 
				 gnome_vfs_uri_to_string (data->source->uri, GNOME_VFS_URI_HIDE_NONE),
				 gnome_vfs_uri_to_string (data->target->uri, GNOME_VFS_URI_HIDE_NONE));
#endif
			
			/* load source image */
			success = eog_image_load_sync (image, EOG_IMAGE_LOAD_DEFAULT);
			
			if (success) {
				sinfo = eog_image_save_info_from_image (image);
				g_mutex_lock (data->lock);
				if (data->source != NULL)
					g_object_unref (data->source);
				data->source = sinfo;
				g_mutex_unlock (data->lock);
		
				success = eog_image_save_as_by_info (image, data->source, data->target, &error);
			}
#ifdef SAVE_DEBUG
			g_print ("successful: %i\n", success);
#endif
				
			if (!success && !data->cancel_save) {
				g_mutex_lock (data->lock);
				data->error = g_error_copy (error);
				data->response = EOG_SAVE_RESPONSE_NONE;
				
				g_idle_add ((GSourceFunc) save_error, data);
				g_cond_wait (data->wait, data->lock);

				if (data->response == EOG_SAVE_RESPONSE_SKIP)
					success = TRUE;

				g_error_free (data->error);
				data->error = NULL;

				g_mutex_unlock (data->lock);
			}
			
			if (success) {
				g_idle_add ((GSourceFunc) save_update_image,g_object_ref (image));
			}
		}

		g_mutex_lock (data->lock);
		data->current = data->current->next;
		data->n_processed++;
		g_mutex_unlock (data->lock);
	}

	g_idle_add ((GSourceFunc) save_finished, data);

	return NULL;
}

/* Asks user for a file location to save an image there. Returns the save target uri
 * and the image format. The uri parameter is set to NULL if the user canceled the
 * dialog.
 */
static void
save_as_file_selection_dialog (EogWindow *window, char *folder_uri, char **uri, GdkPixbufFormat **format)
{
	GtkWidget *dlg;
	gboolean success = FALSE;
	gint response;

	g_return_if_fail (uri != NULL);
	g_return_if_fail (format != NULL);

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
		}
	}

	gtk_widget_destroy (dlg);
}

static void
save_as_single_image (EogWindow *window, EogImage *image)
{
	EogWindowPrivate *priv;
	char *uri = NULL;
	SaveData *data;
	GnomeVFSURI *img_uri;
	GnomeVFSURI *parent;
	char *folder_uri = NULL;
	GdkPixbufFormat *format = NULL;
	GtkWidget *button;

	g_return_if_fail (EOG_IS_WINDOW (window));
	g_return_if_fail (EOG_IS_IMAGE (image));

	priv = window->priv;

	img_uri = eog_image_get_uri (image);
	g_assert (gnome_vfs_uri_has_parent (img_uri));
	parent = gnome_vfs_uri_get_parent (img_uri);
	folder_uri = gnome_vfs_uri_to_string (parent, GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (img_uri);
	gnome_vfs_uri_unref (parent);

	save_as_file_selection_dialog (window, folder_uri, &uri, &format);
	g_free (folder_uri);

	if (uri == NULL)
		return;

	g_assert (uri != NULL && format != NULL);

	data = g_new0 (SaveData, 1);
	g_assert (data != NULL);

	/* initialise data struct */
	data->images      = g_list_append (NULL, image);
	data->n_images    = 1;
	data->n_processed = 0;
	
	data->current = NULL;
	data->source  = NULL;
	data->target = eog_image_save_info_from_uri (uri, format);
	data->conv   = NULL;

	data->response = EOG_SAVE_RESPONSE_NONE;

	data->error       = NULL;
	data->cancel_save = FALSE;

	data->wait = g_cond_new ();
	data->lock = g_mutex_new ();

	data->window = window;
	data->dlg = GTK_WINDOW (eog_save_dialog_new (GTK_WINDOW (window), data->n_images));
	button = eog_save_dialog_get_button (GTK_WINDOW (data->dlg));
	g_signal_connect (G_OBJECT (button), "clicked", (GCallback) save_cancel, data);

	/* disable image modification functions */
	gtk_action_group_set_sensitive (priv->actions_image,  FALSE);

	gtk_widget_show_all (GTK_WIDGET (data->dlg));
	gtk_widget_show_now (GTK_WIDGET (data->dlg));

	data->thread = g_thread_create (save_as_image_list, data, TRUE, NULL);

	g_free (uri);
}

static void
save_as_multiple_images (EogWindow *window, GList *images)
{
	GtkWidget *dlg;
	char *base_dir;
	GnomeVFSURI *base_uri;
	gint response; 
	EogURIConverter *conv = NULL;
	GtkWidget *button;
	SaveData *data;
	EogWindowPrivate *priv;
	gboolean success = FALSE;
	
	g_return_if_fail (EOG_IS_WINDOW (window));
	
	priv = window->priv;
	
	base_dir = g_get_current_dir ();
	base_uri = gnome_vfs_uri_new (base_dir);

	dlg = eog_save_as_dialog_new (GTK_WINDOW (window), images, 
				      base_uri);

	g_free (base_dir);
	gnome_vfs_uri_unref (base_uri);

	while (!success) {
		GError *error = NULL;

		gtk_widget_show_all (dlg);
		response = gtk_dialog_run (GTK_DIALOG (dlg));
		
		if (response != GTK_RESPONSE_OK) {
			gtk_widget_destroy (GTK_WIDGET (dlg));
			return;
		}

		gtk_widget_hide (dlg);

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

	data = g_new0 (SaveData, 1);
	g_assert (data != NULL);
	g_assert (conv != NULL);
        eog_uri_converter_print_list (conv);

	/* initialise data struct */
	data->images      = images;
	data->n_images    = g_list_length (images);
	data->n_processed = 0;
	
	data->current = NULL;
	data->source  = NULL;
	data->target  = NULL;
	data->conv    = conv;

	data->response = EOG_SAVE_RESPONSE_NONE;

	data->error       = NULL;
	data->cancel_save = FALSE;

	data->wait = g_cond_new ();
	data->lock = g_mutex_new ();

	data->window = window;
	data->dlg = GTK_WINDOW (eog_save_dialog_new (GTK_WINDOW (window), data->n_images));
	button = eog_save_dialog_get_button (GTK_WINDOW (data->dlg));
	g_signal_connect (G_OBJECT (button), "clicked", (GCallback) save_cancel, data);

	/* disable image modification functions */
	gtk_action_group_set_sensitive (priv->actions_image,  FALSE);

	gtk_widget_show_all (GTK_WIDGET (data->dlg));
	gtk_widget_show_now (GTK_WIDGET (data->dlg));

	data->thread = g_thread_create (save_as_image_list, data, TRUE, NULL);
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
	EogWindow *window;
	float max_progress;
	int counter;
} ProgressData;

static void
transformation_progress_cb (EogImage *image, float progress, ProgressData *data)
{
	float total;
	EogWindowPrivate *priv;

	priv = data->window->priv;

	total = ((float) data->counter + progress) / data->max_progress;

	gnome_appbar_set_progress_percentage (GNOME_APPBAR (priv->statusbar), progress);
}

static void
verb_Undo_cb (GtkAction *action, gpointer user_data)
{
	GList *images;
	GList *it;
	gint id;
	ProgressData data;
	EogWindowPrivate *priv;

	priv = EOG_WINDOW (user_data)->priv;
	
	images = eog_wrap_list_get_selected_images (EOG_WRAP_LIST (priv->wraplist));	
	data.window       = EOG_WINDOW (user_data);
	data.max_progress = g_list_length (images);
	data.counter      = 0;
	
	/* block progress changes for actual displayed image */
	if (priv->displayed_image != NULL) {
		g_signal_handler_block (G_OBJECT (priv->displayed_image), priv->sig_id_progress);
	}

	for (it = images; it != NULL; it = it->next) {
		EogImage *image = EOG_IMAGE (it->data);

		id = g_signal_connect (G_OBJECT (image), "progress", (GCallback) transformation_progress_cb, &data);
		
		eog_image_undo (image);
		
		g_signal_handler_disconnect (image, id);
		data.counter++;
	}

	g_list_foreach (images, (GFunc) g_object_unref, NULL);
	g_list_free (images);

	if (priv->displayed_image != NULL) {
		g_signal_handler_unblock (G_OBJECT (priv->displayed_image), priv->sig_id_progress);
	}
}

static void 
apply_transformation (EogWindow *window, EogTransform *trans)
{
	GList *images;
	GList *it;
	gint id;
	ProgressData data;
	EogWindowPrivate *priv;

	priv = window->priv;
	
	images = eog_wrap_list_get_selected_images (EOG_WRAP_LIST (priv->wraplist));	
	data.window       = window;
	data.max_progress = g_list_length (images);
	data.counter      = 0;
	
	/* block progress changes for actual displayed image */
	if (priv->displayed_image != NULL) {
		g_signal_handler_block (G_OBJECT (priv->displayed_image), priv->sig_id_progress);
	}

	for (it = images; it != NULL; it = it->next) {
		EogImage *image = EOG_IMAGE (it->data);

		id = g_signal_connect (G_OBJECT (image), "progress", (GCallback) transformation_progress_cb, &data);
		
		eog_image_transform (image, trans);
		
		g_signal_handler_disconnect (image, id);
		data.counter++;
	}

	g_list_foreach (images, (GFunc) g_object_unref, NULL);
	g_list_free (images);
	g_object_unref (trans);	

	if (priv->displayed_image != NULL) {
		g_signal_handler_unblock (G_OBJECT (priv->displayed_image), priv->sig_id_progress);
	}
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

	priv = window->priv;
	
	if (priv->image_list != NULL) {
		n_images = eog_image_list_length (priv->image_list);
	}

	if (n_images == 0) {
		/* update window content */
		gtk_widget_hide_all (priv->vpane);

		gtk_action_group_set_sensitive (priv->actions_window, TRUE);
		gtk_action_group_set_sensitive (priv->actions_image, FALSE);
	}
	else if (n_images == 1) {
		/* update window content */
		gtk_widget_show_all (priv->vpane);
		gtk_widget_hide_all (gtk_widget_get_parent (priv->wraplist));

		gtk_action_group_set_sensitive (priv->actions_window, TRUE);
		gtk_action_group_set_sensitive (priv->actions_image,  TRUE);
	}
	else {
		/* update window content */
		gtk_widget_show_all (priv->vpane);

		gtk_action_group_set_sensitive (priv->actions_window, TRUE);
		gtk_action_group_set_sensitive (priv->actions_image,  TRUE);
	}
}

static void 
image_progress_cb (EogImage *image, float progress, gpointer data) 
{
	gnome_appbar_set_progress_percentage (GNOME_APPBAR (EOG_WINDOW (data)->priv->statusbar), progress);
}

static void
image_loading_finished_cb (EogImage *image, gpointer data) 
{
	EogWindow *window;
	EogWindowPrivate *priv;
	int n_images = 0;

	window = EOG_WINDOW (data);
	priv = window->priv;

	if (priv->image_list != NULL) {
		n_images = eog_image_list_length (priv->image_list);
	}

	if (n_images == 1) {
		int width, height;
		
		eog_image_get_size (image, &width, &height);
		adapt_window_size (window, width, height);

#ifdef DEBUG
		g_print ("loading finished: %s - (%i|%i)\n", eog_image_get_caption (image), width, height);
#endif
	}

	update_status_bar  (window);
}

static void
image_loading_failed_cb (EogImage *image, const char* message,  gpointer data) 
{
	g_print ("loading failed: %s\n", eog_image_get_caption (image));
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

			/* [image width] x [image height] pixel  [bytes]    [zoom in percent] */
			str = g_strdup_printf (_("%i x %i pixel  %s    %i%%"), 
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
handle_image_selection_changed (EogWrapList *list, EogWindow *window) 
{
	EogWindowPrivate *priv;
	EogImage *image;
	char *title = NULL; 

	priv = window->priv;

	g_assert (priv->image_list != NULL);

	image = eog_wrap_list_get_first_selected_image (EOG_WRAP_LIST (priv->wraplist));

	if (priv->displayed_image != image) {
		if (priv->displayed_image != NULL) {
			g_signal_handler_disconnect (priv->displayed_image, priv->sig_id_progress);
			g_signal_handler_disconnect (priv->displayed_image, priv->sig_id_loading_finished);
			g_signal_handler_disconnect (priv->displayed_image, priv->sig_id_loading_failed);

			g_object_unref (priv->displayed_image);
			priv->displayed_image = NULL;
		}
		
		if (image != NULL) {
			priv->sig_id_progress         = g_signal_connect (image, "progress", 
									  G_CALLBACK (image_progress_cb), window);
			priv->sig_id_loading_finished = g_signal_connect (image, "loading-finished", 
									  G_CALLBACK (image_loading_finished_cb), window);
			priv->sig_id_loading_failed   = g_signal_connect (image, "loading-failed", 
									  G_CALLBACK (image_loading_failed_cb), window);
			priv->displayed_image = image; /* no g_object_ref required, since image has already
							  increased ref count by eog_wrap_list_get_fist_selected_image */
		}
	}
	
        eog_scroll_view_set_image (EOG_SCROLL_VIEW (priv->scroll_view), image);
	eog_info_view_set_image (EOG_INFO_VIEW (priv->info_view), image);

	update_status_bar (window);

	/* update window title */
	if (image != NULL) {
		GnomeVFSURI *uri;
		
		uri = eog_image_get_uri (image);
		title = gnome_vfs_uri_extract_short_name (uri);
		gnome_vfs_uri_unref (uri);
	}
	else {
		title = g_strdup (_("Eye of Gnome"));
	}
	gtk_window_set_title (GTK_WINDOW (window), title);
		
	g_free (title);
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

/* UI<->function mapping */
/* Normal items */
static GtkActionEntry action_entries_window[] = {
  { "FileMenu", NULL, N_("_File") },
  { "EditMenu", NULL, N_("_Edit") },
  { "ViewMenu", NULL, N_("_View") },
  { "HelpMenu", NULL, N_("_Help") },
  { "FileNewWindow",   GTK_STOCK_NEW,   N_("_New"),      "<control>N",  N_("Open a new window"),            G_CALLBACK (verb_FileNewWindow_cb) },
  { "FileOpen",        GTK_STOCK_OPEN,  N_("_Open..."),  "<control>O",  N_("Open a file"),                  G_CALLBACK (verb_FileOpen_cb) },
  { "FileFolderOpen",     GTK_STOCK_OPEN,  N_("Open _Folder..."), "<control><shift>O", N_("Open a folder"), G_CALLBACK (verb_FolderOpen_cb) },
  { "FileCloseWindow", GTK_STOCK_CLOSE, N_("_Close"),    "<control>W",  N_("Close window"),                 G_CALLBACK (verb_FileCloseWindow_cb) },
  { "EditPreferences", GTK_STOCK_PREFERENCES, N_("Prefere_nces"), NULL, N_("Preferences for Eye of Gnome"), G_CALLBACK (verb_EditPreferences_cb) },
  { "HelpManual",      GTK_STOCK_HELP,  N_("_Contents"), "F1",          N_("Help On this application"),     G_CALLBACK (verb_HelpContent_cb) },
  { "HelpAbout",       GNOME_STOCK_ABOUT, N_("_About"),	NULL, N_("About this application"),       G_CALLBACK (verb_HelpAbout_cb) }
};

/* Toggle items */
static GtkToggleActionEntry toggle_entries_window[] = {
  { "ViewToolbar",   NULL, N_("_Toolbar"),   NULL, "Change the visibility of the toolbar in the current window",   G_CALLBACK (verb_ShowHideAnyBar_cb), TRUE },
  { "ViewStatusbar", NULL, N_("_Statusbar"), NULL, "Change the visibility of the statusbar in the current window", G_CALLBACK (verb_ShowHideAnyBar_cb), TRUE }
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
  { "ViewZoomIn", GTK_STOCK_ZOOM_IN, N_("_Zoom In"), "<control>plus", NULL, G_CALLBACK (verb_ZoomIn_cb) },
  { "ViewZoomOut", GTK_STOCK_ZOOM_OUT, N_("Zoom _Out"), "<control>minus", NULL, G_CALLBACK (verb_ZoomOut_cb) },
  { "ViewZoomNormal", GTK_STOCK_ZOOM_100, N_("_Normal Size"), "<control>equal", NULL, G_CALLBACK (verb_ZoomNormal_cb) },
  { "ViewZoomFit", GTK_STOCK_ZOOM_FIT, N_("Best _Fit"), NULL, NULL, G_CALLBACK (verb_ZoomFit_cb) }
};


/**
 * window_construct:
 * @window: A window widget.
 *
 * Constructs the window widget.
 **/
static void
eog_window_construct_ui (EogWindow *window)
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

	g_return_if_fail (window != NULL);
	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	priv->ui_mgr = gtk_ui_manager_new ();
	priv->box = GTK_WIDGET (gtk_vbox_new (FALSE, 0));
	gtk_container_add (GTK_CONTAINER (window), priv->box);

	add_eog_icon_factory ();
	
	/* build menu and toolbar */
	priv->actions_window = gtk_action_group_new ("MenuActionsWindow");
	gtk_action_group_set_translation_domain (priv->actions_window, PACKAGE);
	gtk_action_group_add_actions (priv->actions_window, action_entries_window, G_N_ELEMENTS (action_entries_window), window);
	gtk_action_group_add_toggle_actions (priv->actions_window, toggle_entries_window, G_N_ELEMENTS (toggle_entries_window), window);
	gtk_ui_manager_insert_action_group (priv->ui_mgr, priv->actions_window, 0);

	priv->actions_image = gtk_action_group_new ("MenuActionsImage");
	gtk_action_group_set_translation_domain (priv->actions_image, PACKAGE);
	gtk_action_group_add_actions (priv->actions_image, action_entries_image, G_N_ELEMENTS (action_entries_image), window);
	gtk_ui_manager_insert_action_group (priv->ui_mgr, priv->actions_image, 0);

        filename = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_DATADIR, "gnome-2.0/ui/eog-gtk-ui.xml", TRUE, NULL);
        g_assert (filename);
	gtk_ui_manager_add_ui_from_file (priv->ui_mgr, filename, NULL);
        g_free (filename);

	menubar = gtk_ui_manager_get_widget (priv->ui_mgr, "/MainMenu");
	g_assert (menubar != NULL);
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
	/* g_object_set (G_OBJECT (priv->scroll_view), "height_request", 250, NULL); */
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
	gtk_paned_pack2 (GTK_PANED (priv->hpane), priv->info_view, FALSE, TRUE);

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

	/* set default geometry */
	gtk_window_set_default_size (GTK_WINDOW (window), 
				     DEFAULT_WINDOW_WIDTH,
				     DEFAULT_WINDOW_HEIGHT);
	gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
	gtk_widget_show_all (GTK_WIDGET (window));
	gtk_widget_hide_all (GTK_WIDGET (priv->vpane));

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

	update_ui_visibility (window);
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
eog_window_new (void)
{
	EogWindow *window;

	window = EOG_WINDOW (g_object_new (EOG_TYPE_WINDOW, "title", _("Eye of Gnome"), NULL));

	eog_window_construct_ui (window);
	eog_window_update_properties (window);

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
image_list_prepared_cb (EogImageList *list, EogWindow *window)
{
#ifdef DEBUG
	g_print ("EogWindow: Image list prepared: %i images\n", eog_image_list_length (EOG_IMAGE_LIST (list)));
#endif
	update_ui_visibility (EOG_WINDOW (window));
}

/**
 * eog_window_open:
 * @window: A window.
 * @iid: The object interface id of the bonobo control to load.
 * @text_uri: An escaped text URI for the object to load.
 * @error: An pointer to an error object or NULL.
 *
 * Tries to create an instance of the iid and loads the uri into it.
 *
 * Return value: TRUE on success, FALSE otherwise.
 **/
gboolean
eog_window_open (EogWindow *window, const char *iid, const char *text_uri, GError **error)
{
	EogWindowPrivate *priv;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	EggRecentItem *recent_item;

	priv = window->priv;

#ifdef DEBUG
	g_print ("EogWindow.c: eog_window_open single uri\n");
#endif

	/* create new image list */
	if (priv->image_list != NULL) {
		g_signal_handler_disconnect (G_OBJECT (priv->image_list), priv->sig_id_list_prepared);
		g_object_unref (priv->image_list);
	}
	priv->image_list = eog_image_list_new ();
	priv->sig_id_list_prepared = g_signal_connect (G_OBJECT (priv->image_list), "list_prepared", G_CALLBACK (image_list_prepared_cb),
						       window);

	/* fill list with uris */
	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (text_uri, info,
					  GNOME_VFS_FILE_INFO_DEFAULT |
					  GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	if (result == GNOME_VFS_OK) {
		if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
			eog_image_list_add_directory (priv->image_list, (char*) text_uri);
		}
		else if (info->type == GNOME_VFS_FILE_TYPE_REGULAR) {
			GList *list = NULL;

			list = g_list_prepend (list, (gpointer) text_uri);
			eog_image_list_add_files (priv->image_list, list);

			g_list_free (list);
		}
	}
	gnome_vfs_file_info_unref (info);

	/* attach model to view */
	eog_wrap_list_set_model (EOG_WRAP_LIST (priv->wraplist), EOG_IMAGE_LIST (priv->image_list));

	/* update recent files */
	recent_item = egg_recent_item_new_from_uri (text_uri);
	egg_recent_item_add_group (recent_item, RECENT_FILES_GROUP);
	egg_recent_model_add_full (priv->recent_model, recent_item);
	egg_recent_item_unref (recent_item);		

	/* update ui */
	update_ui_visibility (window);

	return TRUE;
}

/**
 * eog_window_open_list:
 * @window: A window.
 * @iid: The object interface id of the bonobo control to load.
 * @text_uri_list: List of escaped text URIs for the object to load.
 * @error: An pointer to an error object or NULL.
 *
 * Tries to create an instance of the iid and loads all uri's 
 * contained in the list into it.
 *
 * Return value: TRUE on success, FALSE otherwise.
 **/
gboolean
eog_window_open_list (EogWindow *window, const char *iid, GList *text_uri_list, GError **error)
{
	EogWindowPrivate *priv;
	GList *it;

	priv = window->priv;

#ifdef DEBUG
	g_print ("EogWindow.c: eog_window_open  uri list\n");
#endif

	if (priv->image_list != NULL) {
		g_signal_handler_disconnect (G_OBJECT (priv->image_list), priv->sig_id_list_prepared);
		g_object_unref (priv->image_list);
	}

	priv->image_list = eog_image_list_new ();
	priv->sig_id_list_prepared = g_signal_connect (G_OBJECT (priv->image_list), "list_prepared", G_CALLBACK (image_list_prepared_cb),
						       window);
	eog_image_list_add_files (priv->image_list, text_uri_list);
	eog_wrap_list_set_model (EOG_WRAP_LIST (priv->wraplist), EOG_IMAGE_LIST (priv->image_list));

	/* update recent files list */
	for (it = text_uri_list; it != NULL; it = it->next) {
		EggRecentItem *recent_item = egg_recent_item_new_from_uri ((char*) it->data);
		egg_recent_item_add_group (recent_item, RECENT_FILES_GROUP);
		egg_recent_model_add_full (priv->recent_model, recent_item);
		egg_recent_item_unref (recent_item);		
	}

	update_ui_visibility (window);

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
