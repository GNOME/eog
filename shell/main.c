#include <gnome.h>
#include <glade/glade.h>
#include <liboaf/liboaf.h>
#include <gconf/gconf-client.h>
#include <bonobo.h>
#include "eog-window.h"
#include "../config.h"

static const gchar **startup_files;

poptContext ctx;

static gboolean
create_app (gpointer data)
{
	GtkWidget *win;

	win = eog_window_new ();

	gtk_widget_show (win);

	if (data)
		eog_window_open (EOG_WINDOW (win), data);

	return FALSE;
}

int
main (int argc, char **argv)
{
	CORBA_Environment ev;
	CORBA_ORB orb;
	GError *error;
	gint i;

	gnome_init_with_popt_table ("Eye of Gnome", VERSION,
		    argc, argv, NULL, 0, &ctx);

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

	if (ctx) {
		startup_files = poptGetArgs (ctx);
		if (startup_files) {

			/* Load each file into a separate window */
			for (i = 0; startup_files [i]; i++)
				gtk_idle_add (create_app,
					      (gchar*) startup_files [i]);
			}
		else {

			/* Create an empty window */
			gtk_idle_add (create_app, NULL);
		}

		poptFreeContext (ctx);
	} else
		gtk_idle_add (create_app, NULL);

	bonobo_main ();

	CORBA_exception_free (&ev);
	
	return 0;
}
