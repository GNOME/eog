#include <gnome.h>
#include <glade/glade.h>
#include <liboaf/liboaf.h>
#include <gconf/gconf-client.h>
#include <bonobo.h>
#include "eog-window.h"
#include "../config.h"

static guint
create_app (gpointer data)
{
	GtkWidget *win;

	win = eog_window_new ();

	gtk_widget_show (win);

	return FALSE;
}

int
main (int argc, char **argv)
{
	CORBA_Environment ev;
	CORBA_ORB orb;
	GError *error;

	gnome_init ("Eye of Gnome", VERSION,
		    argc, argv);
	glade_gnome_init ();

	CORBA_exception_init (&ev);
	orb = oaf_init (argc, argv);

	error = NULL;
	if (!gconf_init (argc, argv, &error)) {
		g_assert (error != NULL);
		g_message ("GConf init failed: %s", error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}


	if (bonobo_init (orb, NULL, NULL) == FALSE)
		g_error (_("Could not initialize Bonobo!\n"));

	gtk_idle_add ((GtkFunction) create_app, NULL);

	bonobo_main ();

	CORBA_exception_free (&ev);
	
	return 0;
}
