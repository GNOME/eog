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
#include <gnome.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <liboaf/liboaf.h>
#include <bonobo/Bonobo.h>
#include "eog-preferences.h"
#include "eog-window.h"
#include "util.h"
#include "preferences.h"
#include "Eog.h"

/* Default size for windows */

#define DEFAULT_WINDOW_WIDTH 400
#define DEFAULT_WINDOW_HEIGHT 300

/* Private part of the Window structure */
struct _EogWindowPrivate {
	/* UIImage widget for our contents */
	GtkWidget *ui;

	/* The file selection widget used by this window.  We keep it around so
	 * that it will preserve its cwd.
	 */
	GtkWidget *file_sel;

	/* Our GConf client */

	GConfClient *client;

	/* ui container */
	BonoboUIContainer  *ui_container;

	/* Property Control */
	Bonobo_PropertyControl prop_control;

	/* vbox */
	GtkWidget           *box;

	/* the control embedded in the container */
	GtkWidget           *control;

	/* Window scrolling policy type */
	GtkPolicyType sb_policy;

	/* GConf client notify id's */
	guint sb_policy_notify_id;
};

static void eog_window_class_init (EogWindowClass *class);
static void eog_window_init (EogWindow *window);
static void eog_window_destroy (GtkObject *object);

static gint eog_window_delete (GtkWidget *widget, GdkEventAny *event);
static gint eog_window_key_press (GtkWidget *widget, GdkEventKey *event);
static void eog_window_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y, 
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

/* Settings/Preferences callback */
static void
verb_Preferences_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	EogWindow *window;
	EogPreferences *preferences;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);

	preferences = eog_preferences_new (window);

	if (preferences != NULL)
		gnome_dialog_run_and_close (GNOME_DIALOG (preferences));
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
	GList *l;

	/* Destroy windows and exit */

	l = window_list;
	while (l) {
		EogWindow *w;

		w = (EogWindow*) l->data;
		l = l->next;
		gtk_widget_unref (GTK_WIDGET (w));
	}

	gtk_main_quit ();
}

static void
verb_HelpAbout_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	static GtkWidget *about;
	static const char *authors[] = {
		"Federico Mena-Quintero (federico@gnu.org)",
		"Arik Devens (arik@gnome.org)",
		"Jens Finke (jens@gnome.org)",
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
eog_window_get_type (void)
{
	static GtkType eog_window_type = 0;

	if (!eog_window_type) {
		static const GtkTypeInfo eog_window_info = {
			"EogWindow",
			sizeof (EogWindow),
			sizeof (EogWindowClass),
			(GtkClassInitFunc)  eog_window_class_init,
			(GtkObjectInitFunc) eog_window_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		eog_window_type = gtk_type_unique (bonobo_window_get_type (), &eog_window_info);
	}

	return eog_window_type;
}

/* Class initialization function for windows */
static void
eog_window_class_init (EogWindowClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (bonobo_window_get_type ());

	object_class->destroy = eog_window_destroy;

	widget_class->delete_event = eog_window_delete;
	widget_class->key_press_event = eog_window_key_press;
	widget_class->drag_data_received = eog_window_drag_data_received;
}

/* Handler for changes on the window sb policy */
static void
sb_policy_changed_cb (GConfClient *client, guint notify_id, GConfEntry *entry, gpointer data)
{
/*	EogWindow *window;
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

	priv = g_new0 (EogWindowPrivate, 1);
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
eog_window_destroy (GtkObject *object)
{
	EogWindow *window;
	EogWindowPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_WINDOW (object));

	window = EOG_WINDOW (object);
	priv = window->priv;

	if (priv->file_sel) {
		gtk_widget_destroy (priv->file_sel);
		priv->file_sel = NULL;
	}

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
eog_window_delete (GtkWidget *widget, GdkEventAny *event)
{
	EogWindow *window;

	window = EOG_WINDOW (widget);
	eog_window_close (window);
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
	EogWindowPrivate *priv;

	priv = window->priv;

	return (priv->control != NULL);
}


/* Drag_data_received handler for windows */
static void
eog_window_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y, 
			       GtkSelectionData *selection_data, guint info, guint time)
{
	EogWindow *window;
	EogWindowPrivate *priv;
	GList *filenames;
	GList *l;
	gboolean need_new_window;

	window = EOG_WINDOW (widget);
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
	need_new_window = eog_window_has_contents (window);

	for (l = filenames; l; l = l->next) {
		GtkWidget *new_window;
		char *filename;

		g_assert (l->data != NULL);
		filename = l->data;

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

	gnome_uri_list_free_strings (filenames);
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

	window = EOG_WINDOW (gtk_type_new (TYPE_EOG_WINDOW));

	bonobo_window_construct (BONOBO_WINDOW (window), "eog", _("Eye Of Gnome"));
	eog_window_construct (window);

	return GTK_WIDGET (window);
}

#if 0
/* Sets the sensitivity of all the items */
static void
sensitize_zoom_items (GtkWidget **widgets, gboolean sensitive)
{
	g_assert (widgets != NULL);

	for (; *widgets != NULL; widgets++)
		gtk_widget_set_sensitive (*widgets, sensitive);
}
#endif

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
			   GDK_ACTION_COPY);
}


static BonoboUIVerb eog_app_verbs[] = {
	BONOBO_UI_VERB ("FileNewWindow", verb_FileNewWindow_cb),
	BONOBO_UI_VERB ("FileOpen", verb_FileOpen_cb),
	BONOBO_UI_VERB ("FileCloseWindow", verb_FileCloseWindow_cb),
	BONOBO_UI_VERB ("FileExit",      verb_FileExit_cb),
	BONOBO_UI_VERB ("Preferences", verb_Preferences_cb),
	BONOBO_UI_VERB ("HelpAbout",     verb_HelpAbout_cb),
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
	Bonobo_UIContainer corba_container;
	BonoboUIComponent *ui_comp;
	BonoboUIContainer *ui_cont;
	gchar *fname;

	g_return_if_fail (window != NULL);
	g_return_if_fail (EOG_IS_WINDOW (window));

	priv = window->priv;

	priv->ui_container = ui_cont = bonobo_ui_container_new ();
	bonobo_ui_container_set_win (ui_cont, BONOBO_WINDOW (window));

	priv->box = GTK_WIDGET (gtk_vbox_new (TRUE, 2));
	bonobo_window_set_contents (BONOBO_WINDOW (window), priv->box);

	priv->control = NULL;

	/* add menu and toolbar */
	ui_comp = bonobo_ui_component_new ("eog");
	corba_container = bonobo_object_corba_objref (BONOBO_OBJECT (ui_cont));
	bonobo_ui_component_set_container (ui_comp, corba_container);

	fname = bonobo_ui_util_get_ui_fname (NULL, "eog-shell-ui.xml");
	if (fname && g_file_exists (fname)) {
		bonobo_ui_util_set_ui (ui_comp, NULL, "eog-shell-ui.xml", "EOG");
		bonobo_ui_component_add_verb_list_with_data (ui_comp, eog_app_verbs, window);
	} else {
		g_error ("Can't find eog-shell-ui.xml.\n");
	}
	g_free (fname);

	
	gtk_signal_connect (GTK_OBJECT(window), "delete_event", 
			    (GtkSignalFunc) eog_window_close, NULL);

	gtk_widget_set_usize (GTK_WIDGET (window), 
			      DEFAULT_WINDOW_WIDTH,
			      DEFAULT_WINDOW_HEIGHT);

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
eog_window_close (EogWindow *window)
{
	g_return_if_fail (window != NULL);
	g_return_if_fail (EOG_IS_WINDOW (window));

	gtk_widget_destroy (GTK_WIDGET (window));

	if (!window_list)
		gtk_main_quit ();
}

/* Open image dialog */

/* Opens an image in a new window */
static void
open_new_window (EogWindow *window, const char *filename)
{
	EogWindowPrivate *priv;
	GtkWidget *new_window;

	priv = window->priv;

	if (!eog_window_has_contents (window))
		new_window = GTK_WIDGET (window);
	else
		new_window = eog_window_new ();

	if (eog_window_open (EOG_WINDOW (new_window), filename)) {
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
	EogWindow *window;
	EogWindowPrivate *priv;
	char *filename;

	fs = GTK_WIDGET (data);

	window = EOG_WINDOW (gtk_object_get_data (GTK_OBJECT (fs), "window"));
	g_assert (window != NULL);

	priv = window->priv;

	filename = g_strdup (gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
	gtk_widget_hide (fs);
	
	if (gconf_client_get_bool (priv->client, "/apps/eog/window/open_new_window", NULL))
		open_new_window (window, filename);
	else if (!eog_window_open (window, filename))
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
 * window_open_dialog:
 * @window: A window.
 *
 * Creates an "open image" dialog for a window.
 **/
void
eog_window_open_dialog (EogWindow *window)
{
	EogWindowPrivate *priv;

	g_return_if_fail (window != NULL);
	g_return_if_fail (EOG_IS_WINDOW (window));

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

#if 0
/* Picks a reasonable size for the window and zoom factor based on the image size */
static void
auto_size (EogWindow *window)
{
/*	EogWindowPrivate *priv;
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
*/
}
#endif

static void
remove_component (EogWindow *window)
{
	EogWindowPrivate *priv;

	priv = window->priv;

	if (priv->control == NULL) return;

	gtk_container_remove (GTK_CONTAINER (priv->box), priv->control);
	gtk_widget_unref (priv->control);

	priv->control = NULL;
}

static GNOME_EOG_Files*
get_dir_images (const char *path)
{
	DIR *dir;
	CORBA_Environment ev;
	GList *filenames = NULL;
	GList *tmp;
	GNOME_EOG_Files *corba_files;
	gint length = 0;
	gint i = 0;

	g_return_val_if_fail (path != NULL, CORBA_OBJECT_NIL);

	/* Open directory and retrieve all 
	 * image filenames.
	 */
	errno = 0;
	dir = opendir(path);
	if(dir)
	{
		struct dirent *dent = NULL;
		char *filepath;

		while((dent = readdir(dir)) != NULL)
		{
			filepath = g_concat_dir_and_file(path, dent->d_name);
			
			if(g_strncasecmp(gnome_mime_type_of_file(filepath), "image/", 6) == 0)
			{
				filenames = g_list_append (filenames, g_strdup (filepath));
				length++;
			}

			free(filepath);
		}
	       
		closedir(dir);
	}
	else
	{
		g_warning(_("Couldn't open directory. Error: %s.\n"), g_unix_error_string(errno));
		return CORBA_OBJECT_NIL;
	}

	corba_files = GNOME_EOG_Files__alloc ();
	corba_files->_length = length;
	if (length == 0) return corba_files;

 	CORBA_exception_init (&ev);

	corba_files->_buffer = CORBA_sequence_CORBA_string_allocbuf (length);

	while (filenames) {

		corba_files->_buffer [i++] = CORBA_string_dup (filenames->data);

		/* free memory */
		g_free (filenames->data);
		tmp = filenames->next;
		g_list_free_1 (filenames);
		filenames = tmp;		
	}

	CORBA_exception_free (&ev);

	return corba_files;
}

static Bonobo_Control
get_file_control (const gchar *filename)
{
	Bonobo_Unknown unknown_obj;
	Bonobo_Control control;
	Bonobo_PersistFile pfile;
	CORBA_Environment ev;
	const char *mimetype;
	gchar *oaf_query;

	g_return_val_if_fail (filename != NULL, CORBA_OBJECT_NIL);

	/* check if it's really a file */
	if (!g_file_test (filename, G_FILE_TEST_ISFILE)) return CORBA_OBJECT_NIL;
	if (!g_file_exists (filename)) return CORBA_OBJECT_NIL;
	
	/* discover mimetype */
	mimetype = gnome_mime_type (filename); /* do not free mimetype */
	
	/* assemble requirements for the component*/
	oaf_query = g_new0(gchar, 255);
	g_snprintf (oaf_query, 255,
		    "repo_ids.has_all(['IDL:Bonobo/Control:1.0', " 
		    "	               'IDL:Bonobo/PersistFile:1.0']) AND " 
		    "bonobo:supported_mime_types.has('%s')", 
		    mimetype);
	  
	/* activate component */
	CORBA_exception_init (&ev);
	unknown_obj = (Bonobo_Unknown) oaf_activate (oaf_query, NULL, 0, NULL, &ev);
	g_free (oaf_query);	
	if (unknown_obj == CORBA_OBJECT_NIL) return CORBA_OBJECT_NIL;
	
	/* get PersistFile interface */
	pfile = Bonobo_Unknown_queryInterface (unknown_obj, "IDL:Bonobo/PersistFile:1.0", &ev);
	if (pfile == CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (unknown_obj, &ev);
		CORBA_Object_release (unknown_obj, &ev);
		return CORBA_OBJECT_NIL;
	}
	
	/* load the file */
	Bonobo_PersistFile_load (pfile, filename, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		Bonobo_Unknown_unref (pfile, &ev);
		CORBA_Object_release (pfile, &ev);
		Bonobo_Unknown_unref (unknown_obj, &ev);
		CORBA_Object_release (unknown_obj, &ev);
		return CORBA_OBJECT_NIL;
	}	

	Bonobo_Unknown_unref (pfile, &ev);
	CORBA_Object_release (pfile, &ev);

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

static Bonobo_Control
get_directory_control (const gchar *path)
{
	Bonobo_Unknown unknown_obj;
	Bonobo_Control control;
	GNOME_EOG_ImageCollection ilv;
	GNOME_EOG_Files *files;
	CORBA_Environment ev;

	g_return_val_if_fail (path != NULL, CORBA_OBJECT_NIL);

	if(!g_file_test (path, G_FILE_TEST_ISDIR)) return CORBA_OBJECT_NIL;
	
	/* activate component */
 	CORBA_exception_init (&ev);
	unknown_obj = (Bonobo_Unknown) oaf_activate 
		("repo_ids.has_all(['IDL:GNOME/EOG/ImageCollection:1.0', 'IDL:Bonobo/Control:1.0'])",
		 NULL, 0, NULL, &ev);
	if (unknown_obj == CORBA_OBJECT_NIL) return CORBA_OBJECT_NIL;
	
	/* get ImageListView interface */
	ilv = Bonobo_Unknown_queryInterface (unknown_obj, "IDL:GNOME/EOG/ImageCollection:1.0", &ev);
	if (ilv == CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (unknown_obj, &ev);
		CORBA_Object_release (unknown_obj, &ev);
		return CORBA_OBJECT_NIL;		
	}

	/* add image file names to the list view */
	files = get_dir_images (path);
	GNOME_EOG_ImageCollection_addImages (ilv, files, &ev);
	CORBA_free (files);
	
	Bonobo_Unknown_unref (ilv, &ev);
	CORBA_Object_release (ilv, &ev);

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


/**
 * window_open:
 * @window: A window.
 * @filename: An path to the object to load (image/directory).
 *
 * Opens an image file and puts it into a window.  Even if loading fails, the
 * image structure will be created and put in the window.
 *
 * Return value: TRUE on success, FALSE otherwise.
 **/
gboolean
eog_window_open (EogWindow *window, const char *path)
{
	EogWindowPrivate *priv;       
	Bonobo_Control control;
	CORBA_Environment ev;

	g_return_val_if_fail (window != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_WINDOW (window), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	priv = window->priv;
 	CORBA_exception_init (&ev);

	/* remove current component if neccessary */
	if (priv->control != NULL) {
		remove_component (window);
	}
	g_assert (priv->control == NULL);


	/* get appropriate control */
	control = CORBA_OBJECT_NIL;
	if (g_file_test (path, G_FILE_TEST_ISFILE)) {

		control = get_file_control (path);

	} else if (g_file_test (path, G_FILE_TEST_ISDIR)) {

		control = get_directory_control (path);

	} else {

		g_warning ("Couldn't handle: %s.\n", path);
		return FALSE;
	}
	if (control == CORBA_OBJECT_NIL) return FALSE;

	/* add control to the application window */
	priv->control = bonobo_widget_new_control_from_objref  
		(control, bonobo_object_corba_objref(BONOBO_OBJECT(priv->ui_container)));
	gtk_object_ref (GTK_OBJECT (priv->control));

	/* view and activate it */
	gtk_container_add (GTK_CONTAINER (priv->box), priv->control);
	gtk_widget_show (priv->control);
	gtk_widget_show (priv->box);

	Bonobo_Control_activate (control, TRUE, &ev);

	/* Get property control */
	priv->prop_control = Bonobo_Unknown_queryInterface
		(control, "IDL:Bonobo/PropertyControl:1.0", &ev);

	g_message ("Property control: %p", priv->prop_control);

	/* clean up */
	CORBA_exception_free (&ev);

	return TRUE;
}

Bonobo_PropertyControl
eog_window_get_property_control (EogWindow *window, CORBA_Environment *ev)
{
	g_return_val_if_fail (window != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (EOG_IS_WINDOW (window), CORBA_OBJECT_NIL);

	return CORBA_Object_duplicate (window->priv->prop_control, ev);
}

Bonobo_UIContainer
eog_window_get_ui_container (EogWindow *window, CORBA_Environment *ev)
{
	Bonobo_UIContainer corba_container;

	g_return_val_if_fail (window != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (EOG_IS_WINDOW (window), CORBA_OBJECT_NIL);
	g_return_val_if_fail (window->priv->ui_container != NULL, CORBA_OBJECT_NIL);

	corba_container = bonobo_object_corba_objref (BONOBO_OBJECT (window->priv->ui_container));

	return CORBA_Object_duplicate (corba_container, ev);
}
