/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * EOG image viewer.
 *
 * Authors:
 *   Michael Meeks (mmeeks@gnu.org)
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000, Helixcode Inc.
 * Copyright 2000, Eazel, Inc.
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

#include <bonobo.h>

#include <eog-control.h>
#include <eog-embeddable.h>

#define EOG_IMAGE_VIEWER_ID	"OAFIID:eog_image_viewer:a30dc90b-a68f-4ef8-a257-d2f8ab7e6c9f"
#define EOG_EMBEDDABLE_IMAGE_ID	"OAFIID:eog_embeddedable_image:759a2e09-31e1-4741-9ce7-8354d49a16bb"
#define EOG_IMAGE_ID		"OAFIID:eog_image:20fecf27-a66e-4e0d-bb87-d519a5693ba1"

/*
 * Number of running objects
 */ 
static int running_objects = 0;
static BonoboGenericFactory *image_factory = NULL;

/*
 * BonoboObject data
 */
typedef struct {
	BonoboObject       *bonobo_object;

	EogImage           *image;
} bonobo_object_data_t;

static void
bonobo_object_destroy_cb (BonoboObject *object,
			  bonobo_object_data_t *bonobo_object_data)
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_CONTROL (object));
	g_return_if_fail (bonobo_object_data != NULL);

	g_free (bonobo_object_data);

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
eog_image_viewer_factory (BonoboGenericFactory *this, const char *goad_id,
			  void *data)
{
	bonobo_object_data_t *bonobo_object_data;

	g_return_val_if_fail (this != NULL, NULL);
	g_return_val_if_fail (this->goad_id != NULL, NULL);

	bonobo_object_data = g_new0 (bonobo_object_data_t, 1);

	bonobo_object_data->image = eog_image_new ();
	if (bonobo_object_data->image == NULL) {
		g_free (bonobo_object_data);
		return NULL;
	}

	if (!strcmp (goad_id, EOG_IMAGE_VIEWER_ID))
		bonobo_object_data->bonobo_object = (BonoboObject *)
			eog_control_new (bonobo_object_data->image);
	else if (!strcmp (goad_id, EOG_EMBEDDABLE_IMAGE_ID))
		bonobo_object_data->bonobo_object = (BonoboObject *)
			eog_embeddable_new (bonobo_object_data->image);
	else if (!strcmp (goad_id, EOG_IMAGE_ID)) {
		bonobo_object_data->bonobo_object = (BonoboObject *)
			bonobo_object_data->image;

		bonobo_object_ref (BONOBO_OBJECT (bonobo_object_data->image));
	} else {
		g_warning ("Unknown ID `%s' requested", goad_id);
		return NULL;
	}

	running_objects++;

	gtk_signal_connect (GTK_OBJECT (bonobo_object_data->bonobo_object), "destroy",
			    GTK_SIGNAL_FUNC (bonobo_object_destroy_cb), bonobo_object_data);

	return BONOBO_OBJECT (bonobo_object_data->bonobo_object);
}

static void
init_eog_image_viewer_factory (void)
{
	image_factory = bonobo_generic_factory_new_multi
		("OAFIID:eog_image_viewer_factory:8241bcf2-30ca-4c58-ab45-cfb1393e13a4",
		 eog_image_viewer_factory, NULL);
}

static void
init_server_factory (int argc, char **argv)
{
	CORBA_Environment ev;
	CORBA_exception_init (&ev);

        gnome_init_with_popt_table("eog-image-viewer", VERSION,
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
#if 0
	GLogLevelFlags fatal_mask;
	      
	fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
	fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
	g_log_set_always_fatal (fatal_mask);
#endif

	init_server_factory (argc, argv);

	init_eog_image_viewer_factory ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual   (gdk_rgb_get_visual ());

	bonobo_main ();
	
	return 0;
}
