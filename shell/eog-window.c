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
#include <gnome.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/Bonobo.h>
#include <bonobo/bonobo-window.h>
#include <libgnomeui/gnome-window-icon.h>
#include <libgnomevfs/gnome-vfs.h>
#include "eog-window.h"
#include "util.h"
#include "zoom.h"
#include "Eog.h"
#include "eog-file-selection.h"
#include "eog-preferences.h"

/* Default size for windows */

#define DEFAULT_WINDOW_WIDTH 310
#define DEFAULT_WINDOW_HEIGHT 280

#define EOG_VIEWER_CONTROL_IID "OAFIID:GNOME_EOG_Control"
#define EOG_VIEWER_COLLECTION_IID "OAFIID:GNOME_EOG_CollectionControl"
#define EOG_WINDOW_DND_POPUP_PATH  "/popups/dragndrop"

/* Private part of the Window structure */
struct _EogWindowPrivate {
	/* Our GConf client */
	GConfClient *client;

	/* URI we are displaying */
	char *uri;

	/* control frame */
	BonoboControlFrame  *ctrl_frame;

	/* UI component - arn't these comments banal */
	BonoboUIComponent   *ui_comp;

	/* vbox */
	GtkWidget           *box;
	GtkWidget           *ctrl_widget;

	/* statusbar */
	GtkWidget *statusbar;

	/* Window scrolling policy type */
	GtkPolicyType sb_policy;

	/* GConf client notify id's */
	guint sb_policy_notify_id;

	/* list of files recieved from a drag'n'drop action */
	GList *dnd_files;
};

static void eog_window_class_init (EogWindowClass *class);
static void eog_window_init (EogWindow *window);

static gint eog_window_delete (GtkWidget *widget, GdkEventAny *event);
static gint eog_window_key_press (GtkWidget *widget, GdkEventKey *event);
static void eog_window_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y, 
				    GtkSelectionData *selection_data, guint info, guint time);
static void open_dnd_files (EogWindow *window, gboolean need_new_window);


static BonoboWindowClass *parent_class;

/* The list of all open windows */
static GList *window_list = NULL;

/* Drag target types */
enum {
	TARGET_URI_LIST
};



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
verb_FileNewWindow_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	GtkWidget *win;

	win = eog_window_new ();

	gtk_widget_show (win);
}

static void
verb_FileOpen_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	eog_window_open_dialog (EOG_WINDOW (user_data));
}

static void
verb_FileCloseWindow_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	eog_window_close (EOG_WINDOW (user_data));
}

static void
verb_FileExit_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	eog_window_close_all ();
}

static void
verb_EditPreferences_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	GConfClient *client;

	client = EOG_WINDOW (user_data)->priv->client;

	eog_preferences_show (client);
}

static void
verb_HelpAbout_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	static GtkWidget *about;
	static const char *authors[] = {
		"Federico Mena-Quintero <federico@gnu.org>",
		"Jens Finke <jens@triq.net>",
		"Lutz Mueller <urc8@rz.uni-karlsruhe.de>",
		"Martin Baulig <martin@home-of-linux.org>",
		"Arik Devens <arik@gnome.org>",
		"Michael Meeks <mmeeks@gnu.org>",
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
		translators = _("translator_credits-PLEASE_ADD_YOURSELF_HERE");

		pixbuf = gdk_pixbuf_new_from_file (EOG_ICONDIR "/gnome-eog.png", NULL);

		about = gnome_about_new (
			_("Eye of Gnome"),
			VERSION,
			_("Copyright (C) 2000-2002 The Free Software Foundation"),
			_("The GNOME image viewing and cataloging program."),
			authors,
			documenters,
			(strcmp (translators, "translator_credits-PLEASE_ADD_YOURSELF_HERE")
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
verb_HelpContent_cb (BonoboUIComponent *uic, gpointer data, const char *cname)
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
verb_DnDNewWindow_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	open_dnd_files (EOG_WINDOW (user_data), TRUE);
}

static void
verb_DnDSameWindow_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	open_dnd_files (EOG_WINDOW (user_data), FALSE);
}


static void
verb_DnDCancel_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	gnome_vfs_uri_list_free (EOG_WINDOW (user_data)->priv->dnd_files);
	EOG_WINDOW (user_data)->priv->dnd_files = NULL;	
}

static void
activate_uri_cb (BonoboControlFrame *control_frame, const char *uri, gboolean relative, gpointer data)
{
	EogWindow *window;

	g_return_if_fail (uri != NULL);

	window = EOG_WINDOW (eog_window_new ());

	eog_window_open (window, uri);
	gtk_widget_show (GTK_WIDGET (window));
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
		
		eog_window_type = g_type_register_static (BONOBO_TYPE_WINDOW, 
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

	if (priv->uri) {
		g_free (priv->uri);
		priv->uri = NULL;
	}

	if (priv->ctrl_frame != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (priv->ctrl_frame));
		priv->ctrl_frame = NULL;
	}

	/* Clean up GConf-related stuff */
	if (priv->client) {
		gconf_client_notify_remove (priv->client, priv->sb_policy_notify_id);
		gconf_client_remove_dir (priv->client, "/apps/eog", NULL);
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

	object_class->destroy = eog_window_destroy;
	gobject_class->finalize = eog_window_finalize;

	widget_class->delete_event = eog_window_delete;
	widget_class->key_press_event = eog_window_key_press;
	widget_class->drag_data_received = eog_window_drag_data_received;
}

/* Handler for changes on the window sb policy */
static void
sb_policy_changed_cb (GConfClient *client, guint notify_id, GConfEntry *entry, gpointer data)
{
/*     EogWindow *window;
       EogWindowPrivate *priv;

       window = EOG_WINDOW (data);
       priv = window->priv;

       priv->sb_policy = gconf_value_get_int (entry->value);

       gtk_scroll_frame_set_policy (GTK_SCROLL_FRAME (priv->ui), priv->sb_policy, priv->sb_policy);*/
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
	
	priv->uri = NULL;

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
	priv->ctrl_widget = NULL;

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
		verb_FileExit_cb (NULL, widget, NULL);
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

/* Returns whether a window has an image loaded in it */
static gboolean
eog_window_has_contents (EogWindow *window)
{
	g_return_val_if_fail (EOG_IS_WINDOW (window), FALSE);

	return (bonobo_control_frame_get_control (window->priv->ctrl_frame) != CORBA_OBJECT_NIL);
}

/* Drag_data_received handler for windows */
static void
eog_window_drag_data_received (GtkWidget *widget, 
			       GdkDragContext *context, 
			       gint x, gint y, 
			       GtkSelectionData *selection_data, 
			       guint info, guint time)
{
	EogWindow *window;
	EogWindowPrivate *priv;
	gboolean need_new_window = TRUE;

	window = EOG_WINDOW (widget);
	priv = window->priv;

	if (info != TARGET_URI_LIST)
		return;

	if (priv->dnd_files != NULL)
		gnome_vfs_uri_list_free (priv->dnd_files);
	priv->dnd_files = gnome_vfs_uri_list_parse (selection_data->data);

	if (context->suggested_action == GDK_ACTION_ASK) {
		GtkWidget *menu = gtk_menu_new ();
		
		bonobo_window_add_popup (BONOBO_WINDOW (window), 
					 GTK_MENU (menu), 
					 EOG_WINDOW_DND_POPUP_PATH);
		gtk_menu_popup (GTK_MENU (menu),
				NULL,
				NULL,
				NULL,
				NULL,
				0,
				GDK_CURRENT_TIME);
	} else {
 		/* The first image is opened in the same window only if the
		* current window has no image in it.
		*/
		need_new_window = eog_window_has_contents (window);		
		open_dnd_files (window, need_new_window);
	}
}

static void
open_dnd_files (EogWindow *window, gboolean need_new_window)
{
	GtkWidget *new_window;
	GList *l;
	char *filename;

	g_return_if_fail (EOG_IS_WINDOW (window));

	if (window->priv->dnd_files == NULL)
		return;

	for (l = window->priv->dnd_files; l; l = l->next) {
		g_assert (l->data != NULL);
		filename = gnome_vfs_uri_to_string (l->data, GNOME_VFS_URI_HIDE_NONE);

		if (need_new_window)
			new_window = eog_window_new ();
		else
			new_window = GTK_WIDGET (window);

		if (eog_window_open (EOG_WINDOW (new_window), filename)) {
			gtk_widget_show_now (new_window);
			need_new_window = TRUE;
		} else {
			open_failure_dialog (GTK_WINDOW (new_window), filename);
			
			if (new_window != GTK_WIDGET (window))
				gtk_widget_destroy (new_window);
		}
	}

	gnome_vfs_uri_list_free (window->priv->dnd_files);
	window->priv->dnd_files = NULL;
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

	window = EOG_WINDOW (g_object_new (EOG_TYPE_WINDOW, "win_name", "eog", "title", _("Eye of Gnome"), NULL));

	eog_window_construct (window);

	return GTK_WIDGET (window);
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


static BonoboUIVerb eog_app_verbs[] = {
	BONOBO_UI_VERB ("FileNewWindow", verb_FileNewWindow_cb),
	BONOBO_UI_VERB ("FileOpen",      verb_FileOpen_cb),
	BONOBO_UI_VERB ("FileCloseWindow", verb_FileCloseWindow_cb),
	BONOBO_UI_VERB ("FileExit",      verb_FileExit_cb),
	BONOBO_UI_VERB ("EditPreferences", verb_EditPreferences_cb),
	BONOBO_UI_VERB ("HelpAbout",     verb_HelpAbout_cb),
	BONOBO_UI_VERB ("Help",          verb_HelpContent_cb),
	BONOBO_UI_VERB ("DnDNewWindow",  verb_DnDNewWindow_cb),
	BONOBO_UI_VERB ("DnDSameWindow", verb_DnDSameWindow_cb),
	BONOBO_UI_VERB ("DnDCancel",     verb_DnDCancel_cb),
	BONOBO_UI_VERB_END
};


/**
 * window_construct:
 * @window: A window widget.
 *
 * Constructs the window widget.
 **/
void
eog_window_construct (EogWindow *window)
{
	EogWindowPrivate *priv;
	BonoboUIContainer *ui_container;

	g_return_if_fail (window != NULL);
	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	ui_container = bonobo_window_get_ui_container (BONOBO_WINDOW (window));

	bonobo_ui_engine_config_set_path (
		bonobo_window_get_ui_engine (BONOBO_WINDOW (window)),
		"/eog-shell/UIConf/kvps");

	priv->box = GTK_WIDGET (gtk_vbox_new (FALSE, 0));
	gtk_widget_show (priv->box);
	bonobo_window_set_contents (BONOBO_WINDOW (window), priv->box);

	/* add menu and toolbar */
	priv->ui_comp = bonobo_ui_component_new ("eog");
	bonobo_ui_component_set_container (priv->ui_comp, 
					   BONOBO_OBJREF (ui_container), NULL);

	bonobo_ui_util_set_ui (priv->ui_comp, DATADIR, "eog-shell-ui.xml", "EOG", NULL);
	bonobo_ui_component_add_verb_list_with_data (priv->ui_comp, eog_app_verbs, window);

	/* add statusbar */
	priv->statusbar = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_NEVER);
	gtk_box_pack_end (GTK_BOX (priv->box), GTK_WIDGET (priv->statusbar),
			  FALSE, FALSE, 0);
	gtk_widget_show (GTK_WIDGET (priv->statusbar));

	/* add control frame interface */
	priv->ctrl_frame = bonobo_control_frame_new (BONOBO_OBJREF (ui_container));
	bonobo_control_frame_set_autoactivate (priv->ctrl_frame, FALSE);
	g_signal_connect (G_OBJECT (priv->ctrl_frame), "activate_uri",
			  (GtkSignalFunc) activate_uri_cb, NULL);

	set_drag_dest (window);

	/* set default geometry */
	gtk_window_set_default_size (GTK_WINDOW (window), 
				     DEFAULT_WINDOW_WIDTH,
				     DEFAULT_WINDOW_HEIGHT);
	gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
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
		bonobo_main_quit ();
}

/* Open image dialog */

/* Opens an image in a new window; takes in an escaped URI */
static void
open_new_window (EogWindow *window, const char *text_uri)
{
	EogWindowPrivate *priv;
	GtkWidget *new_window;

	priv = window->priv;

	if (!eog_window_has_contents (window))
		new_window = GTK_WIDGET (window);
	else
		new_window = eog_window_new ();

	if (eog_window_open (EOG_WINDOW (new_window), text_uri)) {
		gtk_widget_show_now (new_window);
		raise_and_focus (new_window);
	} else {
		open_failure_dialog (GTK_WINDOW (new_window), text_uri);

		if (new_window != GTK_WIDGET (window))
			gtk_widget_destroy (new_window);
	}
}

/**
 * window_open_dialog:
 * @window: A window.
 *
 * Creates an "open image" dialog for a window.
 **/
void
eog_window_open_dialog (EogWindow *window)
{
	EogWindowPrivate *priv;
	char *filename = NULL;
	GtkWidget *dlg;
	gint response;

	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	dlg = eog_file_selection_new (EOG_FILE_SELECTION_LOAD);
	gtk_widget_show_all (dlg);
	response = gtk_dialog_run (GTK_DIALOG (dlg));
	if (response == GTK_RESPONSE_OK)
		filename = g_strdup (gtk_file_selection_get_filename (GTK_FILE_SELECTION (dlg)));

	gtk_widget_destroy (dlg);

	if (response == GTK_RESPONSE_OK) {
		char *escaped;

		escaped = gnome_vfs_escape_path_string (filename);

		if (gconf_client_get_bool (priv->client, "/apps/eog/window/open_new_window", NULL))
			open_new_window (window, escaped);
		else if (!eog_window_open (window, escaped))
			open_failure_dialog (GTK_WINDOW (window), escaped);

		g_free (escaped);
	}

	if (filename)
		g_free (filename);
}

static void
property_changed_cb (BonoboListener    *listener,
		     char              *event_name, 
		     CORBA_any         *any,
		     CORBA_Environment *ev,
		     gpointer           user_data)
{
	EogWindow *window;

	window = EOG_WINDOW (user_data);

	if (!g_ascii_strcasecmp (event_name, "window/title"))
		gtk_window_set_title (GTK_WINDOW (window),
				      BONOBO_ARG_GET_STRING (any));
	else if (!g_ascii_strcasecmp (event_name, "window/status"))
		gnome_appbar_set_status (GNOME_APPBAR (window->priv->statusbar),
					 BONOBO_ARG_GET_STRING (any));		
}

static void
check_for_control_properties (EogWindow *window)
{
        EogWindowPrivate *priv;
	Bonobo_PropertyBag pb;
	gchar *title;
	CORBA_Environment ev;
	gchar *mask = NULL;

	priv = window->priv;

	CORBA_exception_init (&ev);

	pb = bonobo_control_frame_get_control_property_bag (priv->ctrl_frame, &ev);
	if (pb == CORBA_OBJECT_NIL)
		goto on_error;

	/* set window title */
	title = bonobo_pbclient_get_string (pb, "window/title", &ev);
	if (title != NULL) {
		gtk_window_set_title (GTK_WINDOW (window), title);
		g_free (title);
		mask = g_strdup ("window/title");
	} else {
		g_warning ("Control doesn't support window_title property.");
		gtk_window_set_title (GTK_WINDOW (window), "Eye of Gnome");
	}

	/* set status bar text */
	title = bonobo_pbclient_get_string (pb, "window/status", &ev);
	if (title != NULL) {
		gchar *temp;
		gnome_appbar_set_status (GNOME_APPBAR (priv->statusbar),
					 title);
		g_free (title);
		/* append status_text property to list of properties to listen */
		temp = g_strjoin (",", mask, "window/status", NULL);
		g_free (mask);
		mask = temp;
	} else {
		g_warning ("Control doesn't support status_text property.");
	}

	/* register for further changes */
	if (mask) {
		bonobo_event_source_client_add_listener (pb, 
							 (BonoboListenerCallbackFn)property_changed_cb,
							 mask, &ev, window);
		if (BONOBO_EX (&ev))
			g_warning ("add_listener failed '%s'",
				   bonobo_exception_get_text (&ev));
		g_free (mask);
	} 

	bonobo_object_release_unref (pb, &ev);
	CORBA_exception_free (&ev);
	return;

on_error:
	g_warning ("Control doesn't have properties");
	gtk_window_set_title (GTK_WINDOW (window), "Eye of Gnome");
}

static Bonobo_Control
get_viewer_control (GnomeVFSURI *uri, GnomeVFSFileInfo *info)
{
	Bonobo_Control control;
	Bonobo_PersistFile pfile;
	CORBA_Environment ev;
	gchar *text_uri;

	g_return_val_if_fail (uri != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (info != NULL, CORBA_OBJECT_NIL);
	
	/* check for valid mime_type */
	if ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) == 0) {
		g_warning ("Couldn't retrieve mime type for file.");
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_init (&ev);

	/* get control component */
	control = bonobo_get_object (EOG_VIEWER_CONTROL_IID,
				     "Bonobo/Control", &ev);
	if (BONOBO_EX (&ev) || (control == CORBA_OBJECT_NIL))
		goto ctrl_error;

	/* get PersistFile interface */
	pfile = Bonobo_Unknown_queryInterface (control, "IDL:Bonobo/PersistFile:1.0", &ev);
	if (BONOBO_EX (&ev) || (pfile == CORBA_OBJECT_NIL))
		goto persist_file_error;
	
	/* load the file */
	text_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	Bonobo_PersistFile_load (pfile, text_uri, &ev);
	bonobo_object_release_unref (pfile, NULL);
	g_free (text_uri);
	if (BONOBO_EX (&ev))
		goto persist_file_error;

	/* clean up */
	CORBA_exception_free (&ev);

	return control;
	
	/* error handling */
 persist_file_error:
	bonobo_object_release_unref (control, NULL);
	
 ctrl_error:
	if (BONOBO_EX (&ev))
		g_warning ("%s", bonobo_exception_get_text (&ev));

	CORBA_exception_free (&ev);

	return CORBA_OBJECT_NIL;
}	

#ifdef HAVE_COLLECTION
static Bonobo_Control
get_collection_control (GnomeVFSURI *uri, GnomeVFSFileInfo *info)
{
	Bonobo_Unknown unknown_obj;
	Bonobo_Control control;
	GNOME_EOG_ImageCollection collection;
	CORBA_Environment ev;
	GNOME_EOG_URI eog_uri;

	g_return_val_if_fail (uri != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (info != NULL, CORBA_OBJECT_NIL);
	
	/* activate component */
 	CORBA_exception_init (&ev);
	control = bonobo_get_object (EOG_VIEWER_COLLECTION_IID,
				     "Bonobo/Control", &ev);
	if (BONOBO_EX (&ev) || (control == CORBA_OBJECT_NIL))
		goto coll_ctrl_error;

	/* get PersistFile interface */
	collection = Bonobo_Unknown_queryInterface (control, "IDL:GNOME/EOG/ImageCollection:1.0", &ev);
	if (BONOBO_EX (&ev) || (collection == CORBA_OBJECT_NIL))
		goto coll_error;

	/* set uri */
	eog_uri = (CORBA_char*) gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	GNOME_EOG_ImageCollection_openURI (collection, eog_uri, &ev);
	g_free (eog_uri);
	bonobo_object_release_unref (collection, &ev);

	CORBA_exception_free (&ev);

	return control;

 coll_error:
	bonobo_object_release_unref (control, NULL);

 coll_ctrl_error:
	if (BONOBO_EX (&ev))
		g_warning ("%s", bonobo_exception_get_text (&ev));

	CORBA_exception_free (&ev);

	return CORBA_OBJECT_NIL;
}

static Bonobo_Control
get_collection_control_list (GList *text_uri_list)
{
	Bonobo_Unknown unknown_obj;
	Bonobo_Control control;
	GNOME_EOG_ImageCollection collection;
	CORBA_Environment ev;
	GNOME_EOG_URIList *uri_list;
	GList *uri;
	gint length, i;

	g_return_val_if_fail (text_uri_list != NULL, CORBA_OBJECT_NIL);
	
	/* activate component */
 	CORBA_exception_init (&ev);
	unknown_obj = (Bonobo_Unknown) bonobo_activation_activate 
		("repo_ids.has_all(['IDL:GNOME/EOG/ImageCollection:1.0', 'IDL:Bonobo/Control:1.0'])",
		 NULL, 0, NULL, &ev);
	if (unknown_obj == CORBA_OBJECT_NIL) return CORBA_OBJECT_NIL;
	
	/* get collection image interface */
        collection = Bonobo_Unknown_queryInterface (unknown_obj, "IDL:GNOME/EOG/ImageCollection:1.0", &ev);
	if (collection == CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (unknown_obj, &ev);
		CORBA_Object_release (unknown_obj, &ev);
		return CORBA_OBJECT_NIL;		
	}

	/* create string sequence */
	length = g_list_length (text_uri_list);
	uri_list = GNOME_EOG_URIList__alloc ();
	uri_list->_maximum = length;
	uri_list->_length = length;
	uri_list->_buffer = CORBA_sequence_GNOME_EOG_URI_allocbuf (length);
	uri = text_uri_list;
	for (i = 0; i < length; i++) {
		g_print ("List uri: %s\n", (gchar*) uri->data);
		uri_list->_buffer[i] = CORBA_string_dup ((gchar*)uri->data);
		uri = uri->next;
	}
	CORBA_sequence_set_release (uri_list, CORBA_TRUE);

	/* set uris */
	GNOME_EOG_ImageCollection_openURIList (collection, uri_list, &ev);

	Bonobo_Unknown_unref (collection, &ev);
	CORBA_Object_release (collection, &ev);

	/* get Control interface */
	control = Bonobo_Unknown_queryInterface (unknown_obj, "IDL:Bonobo/Control:1.0", &ev);
	if (control == CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (unknown_obj, &ev);
		CORBA_Object_release (unknown_obj, &ev);
		return CORBA_OBJECT_NIL;		
	}

	/* clean up */ 
	Bonobo_Unknown_unref (unknown_obj, &ev);
        CORBA_Object_release (unknown_obj, &ev);
	CORBA_exception_free (&ev);

	return control;
}
#else

static Bonobo_Control
get_collection_control (GnomeVFSURI *uri, GnomeVFSFileInfo *info)
{
	return CORBA_OBJECT_NIL;
}

static Bonobo_Control
get_collection_control_list (GList *text_uri_list)
{
	return CORBA_OBJECT_NIL;
}

#endif /* HAVE_COLLECTION */


void
adapt_shell_size_to_control (EogWindow *window, Bonobo_Control control)
{
	CORBA_Environment ev;
	EogWindowPrivate *priv;
	Bonobo_PropertyBag pb;
	gint32 width, height;
	gint32 image_width, image_height;
	int sw, sh;
	Bonobo_Zoomable zi;
	gboolean need_zoom;
	int req_width, req_height;
	int xthick, ythick;

	g_return_if_fail (EOG_IS_WINDOW (window));

	CORBA_exception_init (&ev);
	priv = window->priv;

	/* calculate initial eog window size */
	pb = bonobo_control_frame_get_control_property_bag (priv->ctrl_frame, &ev);
	if (pb == CORBA_OBJECT_NIL) return;

		
	/* FIXME: We try to obtain the desired size of the component
	 * here. The image/width image/height properties aren't
	 * generally available in controls and work only with the
	 * eog-image-viewer component!  
	 */
	image_width = bonobo_pbclient_get_long (pb, "image/width", &ev);
	image_height = bonobo_pbclient_get_long (pb, "image/height", &ev);
	
	sw = gdk_screen_width ();
	sh = gdk_screen_height ();
	need_zoom = FALSE;

	if (image_width >= sw) {
		width = 0.75 * sw;
		need_zoom = TRUE;
	}
	else {
		width = image_width;
	}
	if (image_height >= sh) {
		height = 0.75 * sh;
		need_zoom = TRUE;
	}
	else {
		height = image_height;
	}

	if (!width  || !height) {
		bonobo_object_release_unref (pb, &ev);
		return;
	}		

	/* this is the size of the frame around the vbox */
	xthick = priv->box->style->xthickness;
	ythick = priv->box->style->ythickness;
	
	req_height = 
		height + 
		(GTK_WIDGET(window)->allocation.height - priv->box->allocation.height) +
		priv->statusbar->allocation.height + 
		2 * ythick;
	
	req_width = 
		width + 
		(GTK_WIDGET(window)->allocation.width - priv->box->allocation.width) +
		2 * xthick;

	gtk_window_resize (GTK_WINDOW (window), req_width, req_height);

	if (need_zoom) {
		zi = Bonobo_Unknown_queryInterface (control, "IDL:Bonobo/Zoomable:1.0", &ev);
		if (zi != CORBA_OBJECT_NIL) {
			double zoom_level;
			zoom_level = zoom_fit_scale (width, height, 
						     image_width, image_height, TRUE);
			Bonobo_Zoomable_setLevel (zi, zoom_level, &ev);
			bonobo_object_release_unref (zi, &ev);
		}
	}
	
	bonobo_object_release_unref (pb, &ev);
	CORBA_exception_free (&ev);
}

static void 
add_control_to_ui (EogWindow *window, Bonobo_Control control)
{
	EogWindowPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (window != NULL);
	g_return_if_fail (EOG_IS_WINDOW (window));
	
	priv = window->priv;
	CORBA_exception_init (&ev);

	/* bind and view new control widget */
	bonobo_control_frame_bind_to_control (priv->ctrl_frame, control, &ev);
	bonobo_control_frame_control_activate (priv->ctrl_frame);
	if (control != CORBA_OBJECT_NIL && priv->ctrl_widget == NULL) {
		priv->ctrl_widget = bonobo_control_frame_get_widget (priv->ctrl_frame);
		if (!priv->ctrl_widget)
			g_assert_not_reached ();

		gtk_box_pack_start (GTK_BOX (priv->box), priv->ctrl_widget, TRUE, TRUE, 0);
		gtk_widget_show (priv->ctrl_widget);
	}
	else if (control == CORBA_OBJECT_NIL && priv->ctrl_widget != NULL) {
		gtk_container_remove (GTK_CONTAINER (priv->box), 
				      GTK_WIDGET (priv->ctrl_widget));
		priv->ctrl_widget = NULL;
	}

	adapt_shell_size_to_control (window, control);

	CORBA_exception_free (&ev);

	/* retrieve control properties and install listeners */
	check_for_control_properties (window);
}


/**
 * window_open:
 * @window: A window.
 * @filename: An escaped text URI for the object to load.
 *
 * Opens an image file and puts it into a window.  Even if loading fails, the
 * image structure will be created and put in the window.
 *
 * Return value: TRUE on success, FALSE otherwise.
 **/
gboolean
eog_window_open (EogWindow *window, const char *text_uri)
{
	EogWindowPrivate *priv;
	Bonobo_Control control;
	GnomeVFSResult result;
	GnomeVFSFileInfo *info;
	GnomeVFSURI *uri;
	gchar *uri_str;

	g_return_val_if_fail (window != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_WINDOW (window), FALSE);
	g_return_val_if_fail (text_uri != NULL, FALSE);

	priv = window->priv;

	uri = gnome_vfs_uri_new (text_uri);

	/* obtain file infos */
	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info_uri (uri, info,
					      GNOME_VFS_FILE_INFO_DEFAULT |
					      GNOME_VFS_FILE_INFO_FOLLOW_LINKS |
					      GNOME_VFS_FILE_INFO_GET_MIME_TYPE);
	if (result != GNOME_VFS_OK)
		return FALSE;
	
	control = CORBA_OBJECT_NIL;

	/* get appropriate control */
	if (info->type == GNOME_VFS_FILE_TYPE_REGULAR) {
		control = get_viewer_control (uri, info);
	}
	else if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
		control = get_collection_control (uri, info);
	}

	if (control != CORBA_OBJECT_NIL) {
		/* add it to the user interface */
		add_control_to_ui (window, control);
		bonobo_object_release_unref (control, NULL);
	}
	else {
		uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
		/* FIXME: The error message should be more specific 
		 *        (eg. "Unknown file format").
		 */
		open_failure_dialog (GTK_WINDOW (window), uri_str);
		g_free (uri_str);
	}

	/* clean up */
	gnome_vfs_file_info_unref (info);
	gnome_vfs_uri_unref (uri);

	if (priv->uri)
		g_free (priv->uri);

	priv->uri = g_strdup (text_uri);

	return TRUE;
}


gboolean 
eog_window_open_list (EogWindow *window, GList *text_uri_list)
{
	EogWindowPrivate *priv;
	Bonobo_Control control;

	g_return_val_if_fail (window != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_WINDOW (window), FALSE);
	g_return_val_if_fail (text_uri_list != NULL, FALSE);

	priv = window->priv;

      	/* get appropriate control */
	control = get_collection_control_list (text_uri_list);

	/* add it to the user interface */
	add_control_to_ui (window, control);

	return TRUE;
}

Bonobo_PropertyControl
eog_window_get_property_control (EogWindow *window, CORBA_Environment *ev)
{
	Bonobo_Control control;
	Bonobo_PropertyControl prop_control;

	g_return_val_if_fail (window != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (EOG_IS_WINDOW (window), CORBA_OBJECT_NIL);

	control = bonobo_control_frame_get_control (window->priv->ctrl_frame);
	if (control == CORBA_OBJECT_NIL) return CORBA_OBJECT_NIL;
	
	prop_control = Bonobo_Unknown_queryInterface (control, 
						      "IDL:Bonobo/PropertyControl:1.0", ev);
	return prop_control;
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
 * window is not displaying a single image or directory, this will return NULL.
 * 
 * Return value: The URI that is being displayed.
 **/
const char *
eog_window_get_uri (EogWindow *eog_window)
{
	EogWindowPrivate *priv;

	g_return_val_if_fail (EOG_IS_WINDOW (eog_window), NULL);

	priv = eog_window->priv;
	return priv->uri;
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
