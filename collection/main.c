/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * EOG image viewer.
 *
 * Authors:
 *   Michael Meeks (mmeeks@gnu.org)
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000, Helixcode Inc.
 * Copyright 2000, SuSE GmbH.
 */

#include <config.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <gnome.h>
#include <liboaf/liboaf.h>
#include <libgnomevfs/gnome-vfs-init.h>

#include <bonobo.h>

#include "eog-collection-control.h"

/* Number of running objects */ 
static int running_objects = 0;
static BonoboGenericFactory *list_factory = NULL;

static void
bonobo_object_destroy_cb (BonoboObject *object,
			  gpointer      user_data)
{
	g_return_if_fail (object != NULL);

	running_objects--;

	if (running_objects > 0)
		return;

	/* When last object has gone unref the factory & quit. */
	bonobo_object_unref (BONOBO_OBJECT (list_factory));
	gtk_main_quit ();
}

static BonoboObject *
eog_list_viewer_factory (BonoboGenericFactory *this,
			  const char           *oaf_iid,
			  void                 *data)
{
	BonoboObject *retval;

	g_return_val_if_fail (this != NULL, NULL);
	g_return_val_if_fail (oaf_iid != NULL, NULL);

	if (!strcmp (oaf_iid, "OAFIID:GNOME_EOG_CollectionControl")) {
		retval = BONOBO_OBJECT (eog_collection_control_new ());
	} else {
		g_warning ("Unknown IID `%s' requested", oaf_iid);
		return NULL;
	}

	running_objects++;

	gtk_signal_connect (GTK_OBJECT (retval), "destroy",
			    GTK_SIGNAL_FUNC (bonobo_object_destroy_cb), NULL);

	return retval;
}

static void
init_eog_list_viewer_factory (void)
{
	list_factory = bonobo_generic_factory_new_multi (
		"OAFIID:GNOME_EOG_CollectionFactory",
		eog_list_viewer_factory, NULL);
}

static void
init_server_factory (int argc, char **argv)
{
	CORBA_Environment ev;
	CORBA_exception_init (&ev);

        gnome_init_with_popt_table ("eog-collection", VERSION,
				    argc, argv,
				    oaf_popt_options, 0, NULL); 
	oaf_init (argc, argv);

	if (gnome_vfs_init () == FALSE)
		g_error (_("I could not initialize GnomeVFS!\n"));
		
	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("I could not initialize Bonobo"));

	CORBA_exception_free (&ev);
}

int
main (int argc, char *argv [])
{
#if 0
	GLogLevelFlags fatal_mask;
	      
	fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
	fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
	g_log_set_always_fatal (fatal_mask);
#endif

	init_server_factory (argc, argv);

	init_eog_list_viewer_factory ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual   (gdk_rgb_get_visual ());

	bonobo_main ();
	
	return 0;
}
