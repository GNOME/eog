/* Eye of Gnome image viewer - main window widget
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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
#include <gnome.h>
#include "ui-image.h"
#include "window.h"



/* Default size for windows */

#define DEFAULT_WINDOW_WIDTH 400
#define DEFAULT_WINDOW_HEIGHT 300



/* What a window is displaying */
typedef enum {
	WINDOW_MODE_NONE,
	WINDOW_MODE_IMAGE,
	WINDOW_MODE_COLLECTION
} WindowMode;

/* Private part of the Window structure */
typedef struct {
	/* What we are displaying */
	WindowMode mode;

	/* A bin to hold the content.  GnomeApp does not like its contents to be
	 * removed and added again.
	 */
	GtkWidget *bin;

	/* Our contents, which may change depending on the mode */
	GtkWidget *content;

	/* The file selection widget used by this window.  We keep it around so
	 * that it will preserve its cwd.
	 */
	GtkWidget *file_sel;
} WindowPrivate;



static void window_class_init (WindowClass *class);
static void window_init (Window *window);
static void window_destroy (GtkObject *object);

static gint window_delete (GtkWidget *widget, GdkEventAny *event);


static GnomeAppClass *parent_class;



/* What the user may respond to a close window confirmation */
typedef enum {
	CLOSE_SAVE,
	CLOSE_DISCARD,
	CLOSE_DONT_EXIT
} CloseStatus;

/* The list of all open windows */
static GList *window_list;

/* Returns whether a window contains unsaved data */
static gboolean
is_unsaved (Window *window)
{
	WindowPrivate *priv;

	priv = window->priv;

	if (priv->mode == WINDOW_MODE_COLLECTION)
		return TRUE; /* FIXME: is it dirty? */
	else
		return FALSE;
}

/* Brings attention to a window by raising it and giving it focus */
static void
raise_and_focus (GtkWidget *widget)
{
	g_assert (GTK_WIDGET_REALIZED (widget));
	gdk_window_show (widget->window);
	gtk_widget_grab_focus (widget);
}

/* Asks for confirmation for closing an unsaved window */
static CloseStatus
confirm_save (Window *window, gboolean ask_exit)
{
	WindowPrivate *priv;
	GtkWidget *msg;
	char *buf;
	int result;

	priv = window->priv;

	if (!is_unsaved (window))
		return CLOSE_DISCARD;

	g_assert (priv->mode == WINDOW_MODE_COLLECTION);

	raise_and_focus (GTK_WIDGET (window));

	buf = g_strdup_printf (_("Save image collection `%s'?"),
			       "fubari"); /* FIXME: put in title */

	msg = gnome_message_box_new (buf, GNOME_MESSAGE_BOX_QUESTION,
				     _("Save"),
				     _("Don't save"),
				     ask_exit ? _("Don't exit") : NULL,
				     NULL);
	g_free (buf);

	gnome_dialog_set_parent (GNOME_DIALOG (msg), GTK_WINDOW (window));
	gnome_dialog_set_default (GNOME_DIALOG (msg), 0);

	result = gnome_dialog_run (GNOME_DIALOG (msg));

	switch (result) {
	case 0:
		return CLOSE_SAVE;

	case 1:
		return CLOSE_DISCARD;

	case 2:
		return CLOSE_DONT_EXIT;

	default:
		return CLOSE_DONT_EXIT;
	}
}



/* Setting the mode of a window */

static void
set_mode (Window *window, WindowMode mode)
{
	WindowPrivate *priv;

	priv = window->priv;

	if (priv->mode == mode)
		return;

	/* FIXME: this test may be better outside this function */

	switch (confirm_save (window, FALSE)) {
	case CLOSE_SAVE:
		; /* FIXME: save it */
		break;

	default:
		break;
	}

	if (priv->content)
		gtk_widget_destroy (priv->content);

	priv->mode = mode;

	switch (mode) {
	case WINDOW_MODE_NONE:
		break;

	case WINDOW_MODE_IMAGE:
		priv->content = ui_image_new ();
		gtk_container_add (GTK_CONTAINER (priv->bin), priv->content);
		gtk_widget_show (priv->content);
		break;

	case WINDOW_MODE_COLLECTION:
		/* FIXME - create UI, fallthrough */

	default:
		g_assert_not_reached ();
	}
}



/* File opening */

/* Open an image file in a window */
static void
open_image (Window *window, char *filename)
{
	WindowPrivate *priv;
	Image *image;

	g_assert (filename != NULL);

	priv = window->priv;

	set_mode (window, WINDOW_MODE_IMAGE);
	g_assert (priv->content != NULL && IS_UI_IMAGE (priv->content));

	image = image_new ();
	image_load (image, filename);
	ui_image_set_image (UI_IMAGE (priv->content), image);
	image_unref (image);
}



/* Menu callbacks */

/* OK button callback for the open file selection dialog */
static void
open_ok_clicked (GtkWidget *widget, gpointer data)
{
	GtkWidget *fs;
	Window *window;
	char *filename;

	fs = GTK_WIDGET (data);

	window = WINDOW (gtk_object_get_data (GTK_OBJECT (fs), "window"));
	g_assert (window != NULL);

	filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs));
	open_image (window, filename);

	gtk_widget_hide (fs);
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

/* File/Open callback */
static void
open_cmd (GtkWidget *widget, gpointer data)
{
	Window *window;
	WindowPrivate *priv;
	GtkWidget *fs;

	window = WINDOW (data);
	priv = window->priv;

	if (!priv->file_sel) {
		priv->file_sel = gtk_file_selection_new (_("Open Image"));
		gtk_window_set_transient_for (GTK_WINDOW (priv->file_sel), GTK_WINDOW (window));
		gtk_object_set_data (GTK_OBJECT (priv->file_sel), "window", window);

		gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (priv->file_sel)->ok_button),
				    "clicked",
				    GTK_SIGNAL_FUNC (open_ok_clicked),
				    priv->file_sel);
		gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (priv->file_sel)->cancel_button),
				    "clicked",
				    GTK_SIGNAL_FUNC (open_cancel_clicked),
				    fs);
		gtk_signal_connect (GTK_OBJECT (priv->file_sel), "delete_event",
				    GTK_SIGNAL_FUNC (open_delete_event),
				    window);
	}

	gtk_widget_show_now (priv->file_sel);
	raise_and_focus (priv->file_sel);
}

/* Closes a window with confirmation, and exits the main loop if this was the
 * last window in the list.
 */
static void
close_window (Window *window)
{
	switch (confirm_save (window, FALSE)) {
	case CLOSE_SAVE:
		; /* FIXME: save it */
		break;

	case CLOSE_DISCARD:
		break;

	default:
		return;
	}

	gtk_widget_destroy (GTK_WIDGET (window));

	if (!window_list)
		gtk_main_quit ();
}

/* File/Close callback */
static void
close_cmd (GtkWidget *widget, gpointer data)
{
	Window *window;

	window = WINDOW (data);
	close_window (window);
}

/* File/Exit callback */
static void
exit_cmd (GtkWidget *widget, gpointer data)
{
	gboolean do_exit;
	GList *l;
	Window *w;

	/* Ask for confirmation on unsaved windows */

	do_exit = TRUE;

	for (l = window_list; l && do_exit; l = l->next) {
		w = l->data;

		switch (confirm_save (w, TRUE)) {
		case CLOSE_SAVE:
			; /* FIXME: save it */
			break;

		case CLOSE_DONT_EXIT:
			do_exit = FALSE;
			break;

		default:
			break;
		}
	}

	if (!do_exit)
		return;

	/* Destroy windows and exit */

	l = window_list;
	while (l) {
		w = l->data;
		l = l->next;
		gtk_widget_destroy (GTK_WIDGET (w));
	}

	gtk_main_quit ();
}

/* Help/About callback */
static void
about_cmd (GtkWidget *widget, gpointer data)
{
	static GtkWidget *about;
	static const char *authors[] = {
		"Federico Mena-Quintero (federico@gimp.org)",
		NULL
	};

	if (!about) {
		about = gnome_about_new (
			_("Eye of Gnome"),
			VERSION,
			_("Copyright (C) 1999 The Free Software Foundation"),
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
	{ GNOME_APP_UI_ITEM, N_("_Open Image..."), N_("Open an image file"),
	  open_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN,
	  'o', GDK_CONTROL_MASK, NULL },
	GNOMEUIINFO_SEPARATOR,
	{ GNOME_APP_UI_ITEM, N_("_Close This Window"), N_("Close the current window"),
	  close_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CLOSE,
	  'w', GDK_CONTROL_MASK, NULL },
	GNOMEUIINFO_MENU_EXIT_ITEM (exit_cmd, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo help_menu[] = {
	GNOMEUIINFO_MENU_ABOUT_ITEM (about_cmd, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo main_menu[] = {
	GNOMEUIINFO_MENU_FILE_TREE (file_menu),
	GNOMEUIINFO_MENU_HELP_TREE (help_menu),
	GNOMEUIINFO_END
};



/**
 * window_get_type:
 * @void:
 *
 * Registers the &Window class if necessary, and returns the type ID associated
 * to it.
 *
 * Return value: the type ID of the &Window class.
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
}

/* Object initialization function for windows */
static void
window_init (Window *window)
{
	WindowPrivate *priv;

	priv = g_new0 (WindowPrivate, 1);
	window->priv = priv;

	window_list = g_list_prepend (window_list, window);
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

	if (priv->file_sel)
		gtk_widget_destroy (priv->file_sel);

	g_free (priv);

	window_list = g_list_remove (window_list, window);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* delete_event handler for windows */
static gint
window_delete (GtkWidget *widget, GdkEventAny *event)
{
	Window *window;

	window = WINDOW (widget);
	close_window (window);
	return TRUE;
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

	g_return_if_fail (window != NULL);
	g_return_if_fail (IS_WINDOW (window));

	priv = window->priv;

	gnome_app_construct (GNOME_APP (window), "eog", _("Eye of Gnome"));
	gnome_app_create_menus_with_data (GNOME_APP (window), main_menu, window);

	priv->bin = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gnome_app_set_contents (GNOME_APP (window), priv->bin);
	gtk_widget_show (priv->bin);

	gtk_window_set_default_size (GTK_WINDOW (window),
				     DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
}
