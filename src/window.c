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
	/* FIXME */
}

/* File/Exit callback */
static void
exit_cmd (GtkWidget *widget, gpointer data)
{
	/* FIXME */
}

static void
meept (GtkWidget *widget, gpointer data)
{
	fprintf (stderr, "meept!\n");
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
		about = gnome_about_new (_("Retina"),
					 VERSION,
					 _("Copyright (C) 1999 The Free Software Foundation"),
					 authors,
					 _("The GNOME image viewer and cataloging program"),
					 NULL);
		gtk_signal_connect (GTK_OBJECT (about), "destroy",
				    GTK_SIGNAL_FUNC (gtk_widget_destroyed),
				    &about);
		gtk_signal_connect (GTK_OBJECT (about), "destroy",
				    GTK_SIGNAL_FUNC (meept),
				    &about);
	}

	gtk_widget_show (about);
}



/* Main menu */

static GnomeUIInfo file_menu[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("_Open Image..."),
				N_("Open an image file"),
				open_cmd, GNOME_STOCK_MENU_OPEN),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CLOSE_ITEM (close_cmd, NULL),
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
#if 0
	gnome_app_create_menus_with_data (
#endif
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
