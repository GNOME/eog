/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * EOG image viewer using the core eog code.
 *
 * Authors:
 *   Michael Meeks (mmeeks@gnu.org)
 *   Martin Baulig (baulig@suse.de)
 *
 * TODO:
 *    Progressive loading.
 *    Do not display more than required
 *    Queue request-resize on image size change/load
 *    Save image
 *
 * Copyright 2000, Helixcode Inc.
 * Copyright 2000, Eazel, Inc.
 * Copyright 2000, SuSE GmbH.
 */
#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <gnome.h>
#include <liboaf/liboaf.h>

#include <bonobo.h>

#include "eog-image-viewer.h"

/*
 * Number of running objects
 */ 
static int running_objects = 0;
static BonoboGenericFactory    *image_factory = NULL;

/*
 * BonoboObject data
 */
typedef struct {
	EogImageViewer *bonobo_object;
} bonobo_object_data_t;

static void
bod_destroy_cb (BonoboObject *object, bonobo_object_data_t *bod)
{
        if (!bod)
		return;

	g_message ("bod_destroy_cb: %p", object);

	g_free (bod);

	running_objects--;
	if (running_objects > 0)
		return;
	/*
	 * When last object has gone unref the factory & quit.
	 */
	bonobo_object_unref (BONOBO_OBJECT (image_factory));
	gtk_main_quit ();
}

static BonoboObject *
eog_image_viewer_component_factory (BonoboGenericFactory *this, void *data)
{
	bonobo_object_data_t *bod;

	g_return_val_if_fail (this != NULL, NULL);
	g_return_val_if_fail (this->goad_id != NULL, NULL);

	bod = g_new0 (bonobo_object_data_t, 1);

	bod->bonobo_object = eog_image_viewer_new ();

	running_objects++;

	gtk_signal_connect (GTK_OBJECT (bod->bonobo_object), "destroy",
			    GTK_SIGNAL_FUNC (bod_destroy_cb), bod);

	return BONOBO_OBJECT (bod->bonobo_object);
}

static void
init_eog_image_viewer_component_factory (void)
{
	image_factory = bonobo_generic_factory_new
		("OAFIID:eog_image_viewer_component_factory:a5c88a13-9021-4f9e-b0c2-faa8a657c40c",
		 eog_image_viewer_component_factory, NULL);
}

static void
init_server_factory (int argc, char **argv)
{
	CORBA_Environment ev;
	CORBA_exception_init (&ev);

        gnome_init_with_popt_table("bonobo-image-generic", VERSION,
				   argc, argv,
				   oaf_popt_options, 0, NULL); 
	oaf_init (argc, argv);

	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("I could not initialize Bonobo"));

	CORBA_exception_free (&ev);
}

int
main (int argc, char *argv [])
{
	GLogLevelFlags fatal_mask;
	      
	fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
	fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
	g_log_set_always_fatal (fatal_mask);

	init_server_factory (argc, argv);

	init_eog_image_viewer_component_factory ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual   (gdk_rgb_get_visual ());

	bonobo_main ();
	
	return 0;
}
