/* Eye of Gnome image viewer - main window widget
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
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
#include <math.h>
#include <gnome.h>
#include "commands.h"
#include "full-screen.h"
#include "image-view.h"
#include "preferences.h"
#include "tb-image.h"
#include "ui-image.h"
#include "util.h"
#include "window.h"
#include "zoom.h"

/* Default size for windows */

#define DEFAULT_WINDOW_WIDTH 400
#define DEFAULT_WINDOW_HEIGHT 300

/* Private part of the Window structure */
struct _WindowPrivate {
	/* UIImage widget for our contents */
	GtkWidget *ui;

	/* The file selection widget used by this window.  We keep it around so
	 * that it will preserve its cwd.
	 */
	GtkWidget *file_sel;

	/* Our GConf client */
	GConfClient *client;

	/* View menu */
	GtkWidget *view_menu;

	/* Zoom toolbar items */
	GtkWidget **zoom_tb_items;

	/* Window scrolling policy type */
	GtkPolicyType sb_policy;

	/* GConf client notify id's */
	guint sb_policy_notify_id;
};

static void window_class_init (WindowClass *class);
static void window_init (Window *window);
static void window_destroy (GtkObject *object);

static gint window_delete (GtkWidget *widget, GdkEventAny *event);
static gint window_key_press (GtkWidget *widget, GdkEventKey *event);
static void window_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y, 
				       GtkSelectionData *selection_data, guint info, guint time);

static GnomeAppClass *parent_class;

/* The list of all open windows */
static GList *window_list;

/* Drag target types */
enum {
	TARGET_URI_LIST
};



/* Brings attention to a window by raising it and giving it focus */
static void
raise_and_focus (GtkWidget *widget)
{
	g_assert (GTK_WIDGET_REALIZED (widget));
	gdk_window_show (widget->window);
	gtk_widget_grab_focus (widget);
}

/* Creating a new window */

/* Creates a new window */
static void
create_window (void)
{
	GtkWidget *window;

	window = window_new ();
	gtk_widget_show (window);
}

/* File/New Window */

/* File/New Window callback */
static void
new_window_cmd (GtkWidget *widget, gpointer data)
{
	create_window ();
}

/* File/Close and Exit */

/* File/Exit callback */
static void
exit_cmd (GtkWidget *widget, gpointer data)
{
	GList *l;

	/* Destroy windows and exit */

	l = window_list;
	while (l) {
		Window *w;

		w = l->data;
		l = l->next;
		gtk_widget_destroy (GTK_WIDGET (w));
	}

	gtk_main_quit ();
}

/* Settings/Preferences callback */
static void
preferences_cmd (GtkWidget *widget, gpointer data)
{
	prefs_dialog ();
}

/* Help/About */

/* Help/About callback */
static void
about_cmd (GtkWidget *widget, gpointer data)
{
	static GtkWidget *about;
	static const char *authors[] = {
		"Federico Mena-Quintero (federico@gnu.org)",
		"Arik Devens (arik@helixcode.com)",
		NULL
	};

	if (!about) {
		about = gnome_about_new (
			_("Eye of Gnome"),
			VERSION,
			_("Copyright (C) 2000 The Free Software Foundation"),
			authors,
			_("The GNOME image viewing and cataloging program"),
			NULL);
		gtk_signal_connect (GTK_OBJECT (about), "destroy",
				    GTK_SIGNAL_FUNC (gtk_widget_destroyed),
				    &about);
	}

	gtk_widget_show_now (about);
	raise_and_focus (about);
}

/* Main menu */

static GnomeUIInfo file_menu[] = {
	GNOMEUIINFO_MENU_NEW_WINDOW_ITEM (new_window_cmd, NULL),
	{ GNOME_APP_UI_ITEM, N_("_Open Image..."), N_("Open an image file"),
	  cmd_cb_image_open, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN,
	  'o', GDK_CONTROL_MASK, NULL },
	GNOMEUIINFO_SEPARATOR,
	{ GNOME_APP_UI_ITEM, N_("_Close This Window"), N_("Close the current window"),
	  cmd_cb_window_close, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CLOSE,
	  'w', GDK_CONTROL_MASK, NULL },

#define FILE_EXIT_INDEX 4
	GNOMEUIINFO_MENU_EXIT_ITEM (exit_cmd, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo zoom_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("2:1"), NULL, cmd_cb_zoom_2_1),
	GNOMEUIINFO_ITEM_NONE (N_("3:1"), NULL, cmd_cb_zoom_3_1),
	GNOMEUIINFO_ITEM_NONE (N_("4:1"), NULL, cmd_cb_zoom_4_1),
	GNOMEUIINFO_ITEM_NONE (N_("5:1"), NULL, cmd_cb_zoom_5_1),
	GNOMEUIINFO_ITEM_NONE (N_("6:1"), NULL, cmd_cb_zoom_6_1),
	GNOMEUIINFO_ITEM_NONE (N_("7:1"), NULL, cmd_cb_zoom_7_1),
	GNOMEUIINFO_ITEM_NONE (N_("8:1"), NULL, cmd_cb_zoom_8_1),
	GNOMEUIINFO_ITEM_NONE (N_("9:1"), NULL, cmd_cb_zoom_9_1),
	GNOMEUIINFO_ITEM_NONE (N_("10:1"), NULL, cmd_cb_zoom_10_1),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("1:2"), NULL, cmd_cb_zoom_1_2),
	GNOMEUIINFO_ITEM_NONE (N_("1:3"), NULL, cmd_cb_zoom_1_3),
	GNOMEUIINFO_ITEM_NONE (N_("1:4"), NULL, cmd_cb_zoom_1_4),
	GNOMEUIINFO_ITEM_NONE (N_("1:5"), NULL, cmd_cb_zoom_1_5),
	GNOMEUIINFO_ITEM_NONE (N_("1:6"), NULL, cmd_cb_zoom_1_6),
	GNOMEUIINFO_ITEM_NONE (N_("1:7"), NULL, cmd_cb_zoom_1_7),
	GNOMEUIINFO_ITEM_NONE (N_("1:8"), NULL, cmd_cb_zoom_1_8),
	GNOMEUIINFO_ITEM_NONE (N_("1:9"), NULL, cmd_cb_zoom_1_9),
	GNOMEUIINFO_ITEM_NONE (N_("1:10"), NULL, cmd_cb_zoom_1_10),

	GNOMEUIINFO_END
};

static GnomeUIInfo view_menu[] = {
	{ GNOME_APP_UI_ITEM, N_("Zoom In"), N_("Increase zoom factor by 5%%"),
	  cmd_cb_zoom_in, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL,
	  '=', 0, NULL },
	{ GNOME_APP_UI_ITEM, N_("Zoom Out"), N_("Decrease zoom factor by 5%%"),
	  cmd_cb_zoom_out, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL,
	  '-', 0, NULL },
	{ GNOME_APP_UI_ITEM, N_("Zoom _1:1"), N_("Display the image at 1:1 scale"),
	  cmd_cb_zoom_1, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL,
	  '1', 0, NULL },
	GNOMEUIINFO_SUBTREE (N_("_Zoom factor"), zoom_menu),
	GNOMEUIINFO_SEPARATOR,
	{ GNOME_APP_UI_ITEM, N_("_Fit to Window"), N_("Zoom the image to fit in the window"),
	  cmd_cb_zoom_fit, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL,
	  'f', 0, NULL },
	{ GNOME_APP_UI_ITEM, N_("Full _Screen"), N_("Use the whole screen for display"),
	  cmd_cb_full_screen, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL,
	  's', 0, NULL },
	GNOMEUIINFO_END
};

static GnomeUIInfo settings_menu[] = {
	GNOMEUIINFO_MENU_PREFERENCES_ITEM (preferences_cmd, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo help_menu[] = {
	GNOMEUIINFO_MENU_ABOUT_ITEM (about_cmd, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo main_menu[] = {
	GNOMEUIINFO_MENU_FILE_TREE (file_menu),
	GNOMEUIINFO_MENU_VIEW_TREE (view_menu),
	GNOMEUIINFO_MENU_SETTINGS_TREE (settings_menu),
	GNOMEUIINFO_MENU_HELP_TREE (help_menu),
	GNOMEUIINFO_END
};

/**
 * window_get_type:
 * @void:
 *
 * Registers the #Window class if necessary, and returns the type ID associated
 * to it.
 *
 * Return value: the type ID of the #Window class.
 **/
GtkType
window_get_type (void)
{
	static GtkType window_type = 0;

	if (!window_type) {
		static const GtkTypeInfo window_info = {
			"Window",
			sizeof (Window),
			sizeof (WindowClass),
			(GtkClassInitFunc) window_class_init,
			(GtkObjectInitFunc) window_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		window_type = gtk_type_unique (gnome_app_get_type (), &window_info);
	}

	return window_type;
}

/* Class initialization function for windows */
static void
window_class_init (WindowClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (gnome_app_get_type ());

	object_class->destroy = window_destroy;

	widget_class->delete_event = window_delete;
	widget_class->key_press_event = window_key_press;
	widget_class->drag_data_received = window_drag_data_received;
}

/* Handler for changes on the window sb policy */
static void
sb_policy_changed_cb (GConfClient *client, guint notify_id, const gchar *key,
		      GConfValue *value, gboolean is_default, gpointer data)
{
	Window *window;
	WindowPrivate *priv;

	window = WINDOW (data);
	priv = window->priv;

	priv->sb_policy = gconf_value_int (value);

	gtk_scroll_frame_set_policy (GTK_SCROLL_FRAME (priv->ui), priv->sb_policy, priv->sb_policy);
}

/* Object initialization function for windows */
static void
window_init (Window *window)
{
	WindowPrivate *priv;

	priv = g_new0 (WindowPrivate, 1);
	window->priv = priv;

	priv->client = gconf_client_get_default ();

	gconf_client_add_dir (priv->client, "/apps/eog",
			      GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);

	priv->sb_policy_notify_id = gconf_client_notify_add (
		priv->client, "/apps/eog/window/sb_policy",
		sb_policy_changed_cb, window,
		NULL, NULL);

	priv->sb_policy = gconf_client_get_int (
		priv->client, "/apps/eog/window/sb_policy",
		NULL);

	window_list = g_list_prepend (window_list, window);

	gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, FALSE);
}

/* Destroy handler for windows */
static void
window_destroy (GtkObject *object)
{
	Window *window;
	WindowPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_WINDOW (object));

	window = WINDOW (object);
	priv = window->priv;

	if (priv->file_sel) {
		gtk_widget_destroy (priv->file_sel);
		priv->file_sel = NULL;
	}

	g_free (priv->zoom_tb_items);
	priv->zoom_tb_items = NULL;

	window_list = g_list_remove (window_list, window);

	/* Remove notification handlers */

	gconf_client_notify_remove (priv->client, priv->sb_policy_notify_id);

	priv->sb_policy_notify_id = 0;

	gconf_client_remove_dir (priv->client, "/apps/eog", NULL);

	gtk_object_unref (GTK_OBJECT (priv->client));
	priv->client = NULL;

	/* Clean up */

	g_free (priv);
	window->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* delete_event handler for windows */
static gint
window_delete (GtkWidget *widget, GdkEventAny *event)
{
	Window *window;

	window = WINDOW (widget);
	window_close (window);
	return TRUE;
}

/* Key press handler for windows */
static gint
window_key_press (GtkWidget *widget, GdkEventKey *event)
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
		exit_cmd (NULL, widget);
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

/* Returns whether a window has an image loaded in it */
static gboolean
window_has_image (Window *window)
{
	WindowPrivate *priv;
	ImageView *view;
	Image *image;

	priv = window->priv;

	view = IMAGE_VIEW (ui_image_get_image_view (UI_IMAGE (priv->ui)));
	image = image_view_get_image (view);

	return (image && image->pixbuf);
}


/* Drag_data_received handler for windows */
static void
window_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y, 
			   GtkSelectionData *selection_data, guint info, guint time)
{
	Window *window;
	WindowPrivate *priv;
	GList *filenames;
	GList *l;
	gboolean need_new_window;

	window = WINDOW (widget);
	priv = window->priv;

	if (info != TARGET_URI_LIST)
		return;

	/* FIXME: This should use GnomeVFS later and it should not strip the
	 * method prefix.
	 */

	filenames = gnome_uri_list_extract_filenames (selection_data->data);

	/* The first image is opened in the same window only if the current
	 * window has no image in it.
	 */
	need_new_window = window_has_image (window);

	for (l = filenames; l; l = l->next) {
		GtkWidget *new_window;
		char *filename;

		g_assert (l->data != NULL);
		filename = l->data;

		if (need_new_window)
			new_window = window_new ();
		else
			new_window = GTK_WIDGET (window);

		if (window_open_image (WINDOW (new_window), filename)) {
			gtk_widget_show_now (new_window);
			need_new_window = TRUE;
		} else {
			open_failure_dialog (GTK_WINDOW (new_window), filename);
			
			if (new_window != GTK_WIDGET (window))
				gtk_widget_destroy (new_window);
		}
	}

	gnome_uri_list_free_strings (filenames);
}

/**
 * window_new:
 * @void:
 *
 * Creates a new main window in the WINDOW_MODE_NONE mode.
 *
 * Return value: A newly-created main window.
 **/
GtkWidget *
window_new (void)
{
	Window *window;

	window = WINDOW (gtk_type_new (TYPE_WINDOW));
	window_construct (window);

	return GTK_WIDGET (window);
}

/* Sets the sensitivity of all the items */
static void
sensitize_zoom_items (GtkWidget **widgets, gboolean sensitive)
{
	g_assert (widgets != NULL);

	for (; *widgets != NULL; widgets++)
		gtk_widget_set_sensitive (*widgets, sensitive);
}

/* Sets the window as a drag destination */
static void
set_drag_dest (Window *window)
{
	static const GtkTargetEntry drag_types[] = {
		{ "text/uri-list", 0, TARGET_URI_LIST }
	};

	gtk_drag_dest_set (GTK_WIDGET (window),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
			   drag_types,
			   sizeof (drag_types) / sizeof (drag_types[0]),
			   GDK_ACTION_COPY);
}

/**
 * window_construct:
 * @window: A window widget.
 *
 * Constructs the window widget.
 **/
void
window_construct (Window *window)
{
	WindowPrivate *priv;
	GtkWidget *tb;
	ImageView *view;

	g_return_if_fail (window != NULL);
	g_return_if_fail (IS_WINDOW (window));

	priv = window->priv;

	gnome_app_construct (GNOME_APP (window), "eog", _("Eye of Gnome"));
	gnome_app_create_menus_with_data (GNOME_APP (window), main_menu, window);

	priv->view_menu = main_menu[1].widget;

	tb = tb_image_new (window, &priv->zoom_tb_items);
	gnome_app_set_toolbar (GNOME_APP (window), GTK_TOOLBAR (tb));

	priv->ui = ui_image_new ();
	gnome_app_set_contents (GNOME_APP (window), priv->ui);
	gtk_scroll_frame_set_policy (GTK_SCROLL_FRAME (priv->ui), priv->sb_policy, priv->sb_policy);
	gtk_widget_show (priv->ui);

	view = IMAGE_VIEW (ui_image_get_image_view (UI_IMAGE (priv->ui)));

	gtk_widget_grab_focus (GTK_WIDGET (view));

	gtk_widget_set_sensitive (priv->view_menu, TRUE);
	sensitize_zoom_items (priv->zoom_tb_items, TRUE);

	set_drag_dest (window);
}

/**
 * window_close:
 * @window: A window.
 *
 * Closes a window with confirmation, and exits the main loop if this was the
 * last window in the list.
 **/
void
window_close (Window *window)
{
	g_return_if_fail (window != NULL);
	g_return_if_fail (IS_WINDOW (window));

	gtk_widget_destroy (GTK_WIDGET (window));

	if (!window_list)
		gtk_main_quit ();
}

/* Open image dialog */

/* Opens an image in a new window */
static void
open_new_window (Window *window, const char *filename)
{
	WindowPrivate *priv;
	GtkWidget *new_window;

	priv = window->priv;

	if (!window_has_image (window))
		new_window = GTK_WIDGET (window);
	else
		new_window = window_new ();

	if (window_open_image (WINDOW (new_window), filename)) {
		gtk_widget_show_now (new_window);
		raise_and_focus (new_window);
	} else {
		open_failure_dialog (GTK_WINDOW (new_window), filename);

		if (new_window != GTK_WIDGET (window))
			gtk_widget_destroy (new_window);
	}
}

/* OK button callback for the open file selection dialog */
static void
open_ok_clicked (GtkWidget *widget, gpointer data)
{
	GtkWidget *fs;
	Window *window;
	WindowPrivate *priv;
	char *filename;

	fs = GTK_WIDGET (data);

	window = WINDOW (gtk_object_get_data (GTK_OBJECT (fs), "window"));
	g_assert (window != NULL);

	priv = window->priv;

	filename = g_strdup (gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
	gtk_widget_hide (fs);

	if (gconf_client_get_bool (priv->client, "/apps/eog/window/open_new_window", NULL))
		open_new_window (window, filename);
	else if (!window_open_image (window, filename))
		open_failure_dialog (GTK_WINDOW (window), filename);

	g_free (filename);
}

/* Cancel button callback for the open file selection dialog */
static void
open_cancel_clicked (GtkWidget *widget, gpointer data)
{
	gtk_widget_hide (GTK_WIDGET (data));
}

/* Delete_event handler for the open file selection dialog */
static gint
open_delete_event (GtkWidget *widget, gpointer data)
{
	gtk_widget_hide (widget);
	return TRUE;
}

/**
 * window_open_image_dialog:
 * @window: A window.
 *
 * Creates an "open image" dialog for a window.
 **/
void
window_open_image_dialog (Window *window)
{
	WindowPrivate *priv;

	g_return_if_fail (window != NULL);
	g_return_if_fail (IS_WINDOW (window));

	priv = window->priv;

	if (!priv->file_sel) {
		GtkAccelGroup *accel_group;

		priv->file_sel = gtk_file_selection_new (_("Open Image"));
		gtk_window_set_transient_for (GTK_WINDOW (priv->file_sel), GTK_WINDOW (window));
		gtk_window_set_modal (GTK_WINDOW (priv->file_sel), TRUE);
		gtk_object_set_data (GTK_OBJECT (priv->file_sel), "window", window);

		gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (priv->file_sel)->ok_button),
				    "clicked",
				    GTK_SIGNAL_FUNC (open_ok_clicked),
				    priv->file_sel);
		gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (priv->file_sel)->cancel_button),
				    "clicked",
				    GTK_SIGNAL_FUNC (open_cancel_clicked),
				    priv->file_sel);
		gtk_signal_connect (GTK_OBJECT (priv->file_sel), "delete_event",
				    GTK_SIGNAL_FUNC (open_delete_event),
				    window);

		accel_group = gtk_accel_group_new ();
		gtk_window_add_accel_group (GTK_WINDOW (priv->file_sel), accel_group);
		gtk_widget_add_accelerator (GTK_FILE_SELECTION (priv->file_sel)->cancel_button,
					    "clicked",
					    accel_group,
					    GDK_Escape,
					    0, 0);
	}

	gtk_widget_show_now (priv->file_sel);
	raise_and_focus (priv->file_sel);
}

/* Picks a reasonable size for the window and zoom factor based on the image size */
static void
auto_size (Window *window)
{
	WindowPrivate *priv;
	GtkWidget *view;
	Image *image;
	int iwidth, iheight;
	int swidth, sheight;
	int zwidth, zheight;
	double zoom;

	priv = window->priv;

	if (!window_has_image (window))
		return;

	view = ui_image_get_image_view (UI_IMAGE (priv->ui));
	image = image_view_get_image (IMAGE_VIEW (view));

	iwidth = gdk_pixbuf_get_width (image->pixbuf);
	iheight = gdk_pixbuf_get_height (image->pixbuf);

	swidth = gdk_screen_width () * 0.75;
	sheight = gdk_screen_height () * 0.75;

	zoom = zoom_fit_scale (swidth, sheight, iwidth, iheight, FALSE);
	image_view_set_zoom (IMAGE_VIEW (view), zoom);

	zwidth = floor (iwidth * zoom + 0.5);
	zheight = floor (iheight * zoom + 0.5);

	gtk_widget_set_usize (view, zwidth, zheight);
}

/**
 * window_open_image:
 * @window: A window.
 * @filename: An image filename.
 *
 * Opens an image file and puts it into a window.  Even if loading fails, the
 * image structure will be created and put in the window.
 *
 * Return value: TRUE on success, FALSE otherwise.
 **/
gboolean
window_open_image (Window *window, const char *filename)
{
	WindowPrivate *priv;
	Image *image;
	gboolean retval;
	char *fname;
	gboolean free_fname;
	GtkWidget *view;

	g_return_val_if_fail (window != NULL, FALSE);
	g_return_val_if_fail (IS_WINDOW (window), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	priv = window->priv;

	image = image_new ();
	retval = image_load (image, filename);
	view = ui_image_get_image_view (UI_IMAGE (priv->ui));
	image_view_set_image (IMAGE_VIEW (view), image);

	free_fname = FALSE;

	if (image->filename) {
		if (image->pixbuf) {
			fname = g_strdup_printf ("%s (%dx%d)",
						 g_basename (image->filename),
						 gdk_pixbuf_get_width (image->pixbuf),
						 gdk_pixbuf_get_height (image->pixbuf));
			free_fname = TRUE;
		} else
			fname = g_basename (image->filename);
	} else
		fname = _("Eye of Gnome");

	gtk_window_set_title (GTK_WINDOW (window), fname);

	if (free_fname)
		g_free (fname);

	if (gconf_client_get_bool (priv->client, "/apps/eog/window/auto_size", NULL))
		auto_size (window);

	image_unref (image);
	return retval;
}

/**
 * window_get_ui_image:
 * @window: A window.
 *
 * Queries the image view scroller inside a window.
 *
 * Return value: An image view scroller.
 **/
GtkWidget *
window_get_ui_image (Window *window)
{
	WindowPrivate *priv;

	g_return_val_if_fail (window != NULL, NULL);
	g_return_val_if_fail (IS_WINDOW (window), NULL);

	priv = window->priv;
	return priv->ui;
}
