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

#include <eog-control.h>
#include <eog-embeddable.h>

static BonoboObject *
eog_image_viewer_factory (BonoboGenericFactory *this,
			  const char           *oaf_iid,
			  void                 *data)
{
	EogImage     *image;
	BonoboObject *retval;

	g_return_val_if_fail (this != NULL, NULL);
	g_return_val_if_fail (oaf_iid != NULL, NULL);

	image = eog_image_new ();
	if (!image)
		return NULL;

	if (!strcmp (oaf_iid, "OAFIID:GNOME_EOG_Control"))
		retval = BONOBO_OBJECT (eog_control_new (image));

	else if (!strcmp (oaf_iid, "OAFIID:GNOME_EOG_Embeddable"))
		retval = BONOBO_OBJECT (eog_embeddable_new (image));

	else if (!strcmp (oaf_iid, "OAFIID:GNOME_EOG_Image")) {
		retval = BONOBO_OBJECT (image);
		bonobo_object_ref (BONOBO_OBJECT (image));
	} else {
		g_warning ("Unknown IID `%s' requested", oaf_iid);
		return NULL;
	}

	bonobo_object_unref (BONOBO_OBJECT (image));

	return retval;
}

static void
init_eog_image_viewer_factory (void)
{
	BonoboGenericFactory *factory;

	factory = bonobo_generic_factory_new_multi (
		"OAFIID:GNOME_EOG_Factory",
		eog_image_viewer_factory, NULL);

	bonobo_running_context_auto_exit_unref (
		BONOBO_OBJECT (factory));
}

static void
init_server_factory (int argc, char **argv)
{
	CORBA_Environment ev;
	CORBA_exception_init (&ev);

        gnome_init_with_popt_table ("eog-image-viewer", VERSION,
				    argc, argv,
				    oaf_popt_options, 0, NULL); 
	oaf_init (argc, argv);

	if (gnome_vfs_init () == FALSE)
		g_error (_("Couldn't initialize GnomeVFS!\n"));

	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("I could not initialize Bonobo!\n"));

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

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	init_server_factory (argc, argv);

	init_eog_image_viewer_factory ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual   (gdk_rgb_get_visual ());

	bonobo_main ();
	
	return 0;
}
