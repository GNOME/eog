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
#include "libeog-marshal.h"
#include "egg-recent.h"

/* Default size for windows */

#define DEFAULT_WINDOW_WIDTH  310
#define DEFAULT_WINDOW_HEIGHT 280

#define RECENT_FILES_GROUP         "Eye of Gnome"
#define EOG_WINDOW_DND_POPUP_PATH  "/popups/dragndrop"

#define PROPERTY_WINDOW_STATUS "window/status"
#define PROPERTY_WINDOW_TITLE  "window/title"
#define PROPERTY_WINDOW_WIDTH  "window/width"
#define PROPERTY_WINDOW_HEIGHT "window/height"
#define PROPERTY_IMAGE_PROGRESS "image/progress"

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

	int desired_width;
	int desired_height;

	EggRecentModel      *recent_model;
	EggRecentViewBonobo *recent_view;
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
static void adapt_window_size (EogWindow *window);

static BonoboWindowClass *parent_class;

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
verb_FileNewWindow_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	g_signal_emit (G_OBJECT (user_data), eog_window_signals [SIGNAL_NEW_WINDOW], 0);
}

static void
verb_FileOpen_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	EogWindow *window;
	EogWindowPrivate *priv;
	GtkWidget *dlg;
	gint response;
	GList *list = NULL;

	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);
	priv = window->priv;

	dlg = eog_file_selection_new (EOG_FILE_SELECTION_LOAD);
	gtk_file_selection_set_select_multiple (GTK_FILE_SELECTION (dlg), TRUE);

	gtk_widget_show_all (dlg);
	response = gtk_dialog_run (GTK_DIALOG (dlg));
	if (response == GTK_RESPONSE_OK) {
		char **filenames;
		int i;

		filenames = gtk_file_selection_get_selections (GTK_FILE_SELECTION (dlg));
		if (filenames != NULL) {
			for (i = 0; filenames[i] != NULL; i++) {
				list = g_list_prepend (list, g_strdup (filenames[i]));
			}
			
			list = g_list_reverse (list);
		}
	}

	gtk_widget_destroy (dlg);

	if (list) {
		g_signal_emit (G_OBJECT (window), eog_window_signals[SIGNAL_OPEN_URI_LIST], 0, list);
	}
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
			"Copyright \xc2\xa9 2000-2003 Free Software Foundation, Inc.",
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
activate_uri_cb (BonoboControlFrame *control_frame, const char *uri, gboolean relative, gpointer data)
{

	EogWindow *window;
	GList *list = NULL;
	
	g_return_if_fail (uri != NULL);

	window = EOG_WINDOW (eog_window_new ());
	
	list = g_list_prepend (list, g_strdup (uri));

	g_signal_emit (G_OBJECT (window), eog_window_signals[SIGNAL_OPEN_URI_LIST], 0, list);
}

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
open_uri_list_cleanup (EogWindow *window, GList *txt_uri_list)
{
	GList *it;

	if (txt_uri_list != NULL) {

		for (it = txt_uri_list; it != NULL; it = it->next) {
			g_free ((char*)it->data);
		}
		
		g_list_free (txt_uri_list);
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
	
	priv->uri = NULL;

	priv->client = gconf_client_get_default ();

	gconf_client_add_dir (priv->client, "/apps/eog",
			      GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);

	window_list = g_list_prepend (window_list, window);
	priv->ctrl_widget = NULL;

	priv->desired_width = -1;
	priv->desired_height = -1;

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
gboolean
eog_window_has_contents (EogWindow *window)
{
	g_return_val_if_fail (EOG_IS_WINDOW (window), FALSE);

	return (window->priv->ctrl_frame != NULL);
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
	GList *str_list = NULL;
	GList *it;
	EogWindow *window;

	if (info != TARGET_URI_LIST) 
		return;

	if (context->suggested_action == GDK_ACTION_COPY) { 

		window = EOG_WINDOW (widget);
		
		uri_list = gnome_vfs_uri_list_parse (selection_data->data);
		
		for (it = uri_list; it != NULL; it = it->next) {
			char *filename = gnome_vfs_uri_to_string (it->data, GNOME_VFS_URI_HIDE_NONE);
			str_list = g_list_prepend (str_list, filename);
		}
		
		gnome_vfs_uri_list_free (uri_list);
		/* FIXME: free string list */
		str_list = g_list_reverse (str_list);

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


static BonoboUIVerb eog_app_verbs[] = {
	BONOBO_UI_VERB ("FileNewWindow", verb_FileNewWindow_cb),
	BONOBO_UI_VERB ("FileOpen",      verb_FileOpen_cb),
	BONOBO_UI_VERB ("FileCloseWindow", verb_FileCloseWindow_cb),
	BONOBO_UI_VERB ("FileExit",      verb_FileExit_cb),
	BONOBO_UI_VERB ("EditPreferences", verb_EditPreferences_cb),
	BONOBO_UI_VERB ("HelpAbout",     verb_HelpAbout_cb),
	BONOBO_UI_VERB ("Help",          verb_HelpContent_cb),
	BONOBO_UI_VERB_END
};

static void
widget_realized_cb (GtkWidget *widget, gpointer data)
{
	adapt_window_size (EOG_WINDOW (data));
}


/**
 * window_construct:
 * @window: A window widget.
 *
 * Constructs the window widget.
 **/
static void
eog_window_construct (EogWindow *window)
{
	EogWindowPrivate *priv;
	BonoboUIContainer *ui_container;

	g_return_if_fail (window != NULL);
	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	ui_container = bonobo_window_get_ui_container (BONOBO_WINDOW (window));

#if 0
	/* FIXME: it would be nice to allow toolbar
	   configuration/visibility changes from within eog. But this
	   has some issues at the moment, see bug #114231. Needs
	   fixing for 2.4.x.
	*/

	bonobo_ui_engine_config_set_path (
		bonobo_window_get_ui_engine (BONOBO_WINDOW (window)),
		"/apps/eog/shell/ui"); 
#endif
						   
	priv->box = GTK_WIDGET (gtk_vbox_new (FALSE, 0));

	gtk_widget_show (priv->box);
	bonobo_window_set_contents (BONOBO_WINDOW (window), priv->box);

	/* add menu and toolbar */
	priv->ui_comp = bonobo_ui_component_new ("eog");
	bonobo_ui_component_set_container (priv->ui_comp, 
					   BONOBO_OBJREF (ui_container), NULL);

	bonobo_ui_util_set_ui (priv->ui_comp, DATADIR, "eog-shell-ui.xml", "EOG", NULL);
	bonobo_ui_component_add_verb_list_with_data (priv->ui_comp, eog_app_verbs, window);

	/* recent files support */
	priv->recent_model = egg_recent_model_new (EGG_RECENT_MODEL_SORT_MRU);
	egg_recent_model_set_filter_groups (priv->recent_model, RECENT_FILES_GROUP, NULL);
	egg_recent_model_set_limit (priv->recent_model, 5);

	priv->recent_view = egg_recent_view_bonobo_new (priv->ui_comp,
							"/menu/File/EggRecentDocuments/");
	egg_recent_view_bonobo_show_icons (priv->recent_view, TRUE);
	egg_recent_view_set_model (EGG_RECENT_VIEW (priv->recent_view), priv->recent_model);
	g_signal_connect (G_OBJECT (priv->recent_view), "activate",
			  G_CALLBACK (open_recent_cb), window);

	/* add statusbar */
	priv->statusbar = gnome_appbar_new (TRUE, TRUE, GNOME_PREFERENCES_NEVER);
	gtk_box_pack_end (GTK_BOX (priv->box), GTK_WIDGET (priv->statusbar),
			  FALSE, FALSE, 0);
	/* We connect to the realize signal here, because we must know
	 * when all the child widgets of an eog window have determined
	 * their size in order to get a working adapt_window_size
	 * function. It should be sufficient to connect to the
	 * statusbar here, because it is one of the deepest widgets in
	 * the widget hierarchy tree.  FIXME: if we allow hideable
	 * status-/toolbars then this must be reworked.
	 */
	g_signal_connect_after (G_OBJECT (priv->statusbar),
				"realize", G_CALLBACK (widget_realized_cb), window);
	gtk_widget_show (GTK_WIDGET (priv->statusbar));

	/* add control frame interface */
	priv->ctrl_frame = NULL;

	set_drag_dest (window);

	/* set default geometry */
	gtk_window_set_default_size (GTK_WINDOW (window), 
				     DEFAULT_WINDOW_WIDTH,
				     DEFAULT_WINDOW_HEIGHT);
	gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
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

static void
adapt_window_size (EogWindow *window)
{
	int xthick, ythick;
	int req_height, req_width;
	EogWindowPrivate *priv;

	priv = window->priv;

	if ((priv->desired_width > 0) && (priv->desired_height > 0) &&
	    GTK_WIDGET_REALIZED (priv->statusbar) &&
	    GTK_WIDGET_REALIZED (priv->box) &&
	    GTK_WIDGET_REALIZED (GTK_WIDGET (window)))
	{
		/* this is the size of the frame around the vbox */
		xthick = priv->box->style->xthickness;
		ythick = priv->box->style->ythickness;
		req_width = req_height = -1;
		
		req_height = 
			priv->desired_height + 
			(GTK_WIDGET(window)->allocation.height - priv->box->allocation.height) +
			priv->statusbar->allocation.height + 
			2 * ythick;
		
		req_width = 
			priv->desired_width + 
			(GTK_WIDGET(window)->allocation.width - priv->box->allocation.width) +
			2 * xthick;

		gtk_window_resize (GTK_WINDOW (window), req_width, req_height);
	}
}

static void
property_changed_cb (BonoboListener    *listener,
		     char              *event_name, 
		     CORBA_any         *any,
		     CORBA_Environment *ev,
		     gpointer           user_data)
{
	EogWindow *window;
	EogWindowPrivate *priv;

	window = EOG_WINDOW (user_data);
	priv = window->priv;

	if (any == NULL) return;

	if (!g_ascii_strcasecmp (event_name, PROPERTY_IMAGE_PROGRESS)) {
		gnome_appbar_set_progress_percentage (GNOME_APPBAR (window->priv->statusbar),
						      BONOBO_ARG_GET_FLOAT (any));
	}
	else if (!g_ascii_strcasecmp (event_name, PROPERTY_WINDOW_TITLE)) {
		char *title;

		title = BONOBO_ARG_GET_STRING (any);
		if (title == NULL) {
			gtk_window_set_title (GTK_WINDOW (window), _("Eye of Gnome"));
		}
		else {
			gtk_window_set_title (GTK_WINDOW (window), title);
		}
	}
	else if (!g_ascii_strcasecmp (event_name, PROPERTY_WINDOW_STATUS)) {
		gnome_appbar_set_status (GNOME_APPBAR (window->priv->statusbar),
					 BONOBO_ARG_GET_STRING (any));	
	}
	else if (!g_ascii_strcasecmp (event_name, PROPERTY_WINDOW_WIDTH)) {
		priv->desired_width = BONOBO_ARG_GET_INT (any);
		adapt_window_size (window);
	}
	else if (!g_ascii_strcasecmp (event_name, PROPERTY_WINDOW_HEIGHT)) {
		priv->desired_height = BONOBO_ARG_GET_INT (any);
		adapt_window_size (window);
	}
}


static void
check_for_control_properties (EogWindow *window)
{
        EogWindowPrivate *priv;
	Bonobo_PropertyBag pb;
	CORBA_any *any;
	CORBA_Environment ev;
	GList *properties = NULL;
	GList *it = NULL;
	gint width, height;

	priv = window->priv;
	width = height = -1;

	CORBA_exception_init (&ev);

	pb = bonobo_control_frame_get_control_property_bag (priv->ctrl_frame, &ev);
	if (pb == CORBA_OBJECT_NIL) {
		gtk_window_set_title (GTK_WINDOW (window), _("Eye of Gnome"));
		return;
	}

	/* check for exisiting properties */
	properties = bonobo_pbclient_get_keys (pb, &ev);
	for (it = properties; it != NULL; it = it->next) {
		if (g_ascii_strcasecmp ((char*) it->data, PROPERTY_WINDOW_TITLE) == 0) {
			bonobo_event_source_client_add_listener (pb, 
								 (BonoboListenerCallbackFn) property_changed_cb,
								 PROPERTY_WINDOW_TITLE, &ev, window);

			/* set window title */
			any = bonobo_pbclient_get_value (pb, PROPERTY_WINDOW_TITLE, TC_CORBA_string, &ev);
			property_changed_cb (0, PROPERTY_WINDOW_TITLE, any, &ev, window);
		}
		else if (g_ascii_strcasecmp ((char*) it->data, PROPERTY_WINDOW_STATUS) == 0) {
			bonobo_event_source_client_add_listener (pb, 
								 (BonoboListenerCallbackFn) property_changed_cb,
								 PROPERTY_WINDOW_STATUS, &ev, window);

			/* set status bar text */
			any = bonobo_pbclient_get_value (pb, PROPERTY_WINDOW_STATUS, TC_CORBA_string, &ev);
			property_changed_cb (0, PROPERTY_WINDOW_STATUS, any, &ev, window);
		}
		else if (g_ascii_strcasecmp ((char*) it->data, PROPERTY_WINDOW_WIDTH) == 0) {
			bonobo_event_source_client_add_listener (pb, 
								 (BonoboListenerCallbackFn) property_changed_cb,
								 PROPERTY_WINDOW_WIDTH, &ev, window);

			/* query window size */
			priv->desired_width = bonobo_pbclient_get_long (pb, PROPERTY_WINDOW_WIDTH, &ev);
		}
		else if (g_ascii_strcasecmp ((char*) it->data, PROPERTY_WINDOW_HEIGHT) == 0) {
			bonobo_event_source_client_add_listener (pb, 
								 (BonoboListenerCallbackFn) property_changed_cb,
								 PROPERTY_WINDOW_HEIGHT, &ev, window);

			/* query window size */
			priv->desired_height = bonobo_pbclient_get_long (pb, PROPERTY_WINDOW_HEIGHT, &ev);
		}
		else if (g_ascii_strcasecmp ((char*) it->data, PROPERTY_IMAGE_PROGRESS) == 0) {
			bonobo_event_source_client_add_listener (pb, 
								 (BonoboListenerCallbackFn) property_changed_cb,
								 PROPERTY_IMAGE_PROGRESS, &ev, window);
		}
	}

	adapt_window_size (window);

	bonobo_object_release_unref (pb, &ev);
	CORBA_exception_free (&ev);
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
eog_window_open (EogWindow *win, const char *iid, const char *text_uri, GError **error)
{
	GList *list = NULL;
	gboolean result;

	g_return_val_if_fail (EOG_IS_WINDOW (win), FALSE);
	g_return_val_if_fail (iid != NULL, FALSE);
	g_return_val_if_fail (text_uri != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	
	list = g_list_prepend (list, g_strdup (text_uri));
	
	result = eog_window_open_list (win, iid, list, error);

	g_free (list->data);
	g_list_free (list);

	return result;
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
	BonoboUIContainer *ui_container;
	Bonobo_Control control;
	Bonobo_PersistFile pfile;
	CORBA_Environment ev;
	GList *it;
	EogWindowPrivate *priv;
	char *uri;
	EggRecentItem *recent_item;

	g_return_val_if_fail (EOG_IS_WINDOW (window), FALSE);
	g_return_val_if_fail (iid != NULL, FALSE);
	g_return_val_if_fail (text_uri_list != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	priv = window->priv;

	CORBA_exception_init (&ev);

	/* remove previously loaded control */
	if (priv->ctrl_frame != NULL) {
		bonobo_control_frame_control_deactivate (priv->ctrl_frame);
		bonobo_object_unref (priv->ctrl_frame);
		gtk_widget_destroy (priv->ctrl_widget);
		priv->ctrl_widget = NULL;
		priv->ctrl_frame = NULL;
	}

	/* create control frame */
	ui_container = bonobo_window_get_ui_container (BONOBO_WINDOW (window));
	priv->ctrl_frame = bonobo_control_frame_new (BONOBO_OBJREF (ui_container));
	bonobo_control_frame_set_autoactivate (priv->ctrl_frame, FALSE);
	g_signal_connect (G_OBJECT (priv->ctrl_frame), "activate_uri",
			  (GtkSignalFunc) activate_uri_cb, NULL);

	priv->desired_width = -1;
	priv->desired_height = -1;

	/* get control component */
	control = bonobo_get_object (iid, "Bonobo/Control", &ev);
	if (BONOBO_EX (&ev) || (control == CORBA_OBJECT_NIL)) {
		g_set_error (error, EOG_WINDOW_ERROR,
			     EOG_WINDOW_ERROR_CONTROL_NOT_FOUND,
			     bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		return FALSE;
	}


	/* get PersistFile interface */
	pfile = Bonobo_Unknown_queryInterface (control, "IDL:Bonobo/PersistFile:1.0", &ev);
	if (BONOBO_EX (&ev) || (pfile == CORBA_OBJECT_NIL)) {
		bonobo_object_release_unref (control, NULL);
		g_set_error (error, EOG_WINDOW_ERROR,
			     EOG_WINDOW_ERROR_NO_PERSIST_FILE_INTERFACE,
			     bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		return FALSE;
	}

	/* add control to UI */
	bonobo_control_frame_bind_to_control (priv->ctrl_frame, control, &ev);
	bonobo_control_frame_control_activate (priv->ctrl_frame);
	priv->ctrl_widget = bonobo_control_frame_get_widget (priv->ctrl_frame);
	if (!priv->ctrl_widget) {
		g_assert_not_reached ();
	}
	
	gtk_box_pack_start (GTK_BOX (priv->box), priv->ctrl_widget, TRUE, TRUE, 0);
	gtk_widget_show (priv->ctrl_widget);
	
	/* load the files */
	for (it = text_uri_list; it != NULL; it = it->next) {
		uri = (char*) it->data;

		Bonobo_PersistFile_load (pfile, uri, &ev);

		if (BONOBO_EX (&ev)) {
			g_set_error (error, EOG_WINDOW_ERROR,
				     EOG_WINDOW_ERROR_IO,
				     bonobo_exception_get_text (&ev));
			continue;
		}
		
		if (priv->uri == NULL) {
			/* FIXME: if we really open several URI's in one window
			 * than we save only the first uri. 
			 */
			priv->uri = g_strdup ((char*) it->data);
		}

		recent_item = egg_recent_item_new_from_uri (uri);
		egg_recent_item_add_group (recent_item, RECENT_FILES_GROUP);
		egg_recent_model_add_full (priv->recent_model, recent_item);
		egg_recent_item_unref (recent_item);		
	}

	bonobo_object_release_unref (pfile, &ev);
	bonobo_object_release_unref (control, &ev);

	/* register and check for existing properties */
	check_for_control_properties (window);

	/* clean up */
	CORBA_exception_free (&ev);

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
