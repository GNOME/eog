#include <config.h>
#include <gnome.h>
#include "window.h"



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

	/* Our contents, which may change depending on the mode */
	GtkWidget *content;
} WindowPrivate;



static void window_class_init (WindowClass *class);
static void window_init (Window *window);
static void window_destroy (GtkObject *object);

static gint window_delete (GtkWidget *widget, GdkEventAny *event);


static GnomeAppClass *parent_class;



/* Menu callbacks */

/* File/Open callback */
static void
open_cmd (GtkWidget *widget, gpointer data)
{
	/* FIXME */
}

/* File/Close callback */
static void
close_cmd (GtkWidget *widget, gpointer data)
{
	Window *window;

	window = WINDOW (data);

	/* FIXME */
	gtk_widget_destroy (GTK_WIDGET (window));
}

/* File/Exit callback */
static void
exit_cmd (GtkWidget *widget, gpointer data)
{
	Window *window;

	window = WINDOW (data);

	/* FIXME */

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
			_("The GNOME image viewer and image cataloging program"),
			NULL);
		gtk_signal_connect (GTK_OBJECT (about), "destroy",
				    GTK_SIGNAL_FUNC (gtk_widget_destroyed),
				    &about);
	}

	gtk_widget_show_now (about);
	g_assert (GTK_WIDGET_REALIZED (about));
	gdk_window_show (about->window);
	gtk_widget_grab_focus (about);
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

	/* FIXME: destroy the rest */

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* delete_event handler for windows.  We prompt the user if he has an unsaved
 * image collection and exit the program if this was the last open window.
 */
static gint
window_delete (GtkWidget *widget, GdkEventAny *event)
{
	/* FIXME */
	return FALSE;
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
	GtkWidget *window;

	window = gtk_type_new (TYPE_WINDOW);
	window_construct (WINDOW (window));

	return window;
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
	g_return_if_fail (window != NULL);
	g_return_if_fail (IS_WINDOW (window));

	gnome_app_construct (GNOME_APP (window), "eog", _("Eye of Gnome"));
	gnome_app_create_menus_with_data (GNOME_APP (window), main_menu, window);
}
