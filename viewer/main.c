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

/* This must be included _before_ bonobo-generic-factory.h,
 * otherwise BONOBO_ACTIVATION_FACTORY won't initialize the
 * gtk and other gui stuff! */
#include <bonobo/bonobo-ui-main.h> 

#include <bonobo/bonobo-generic-factory.h>
#include <eog-control.h>

static BonoboObject *
eog_image_viewer_factory (BonoboGenericFactory *this,
			  const char           *oaf_iid,
			  void                 *data)
{
	EogImage     *image;
	BonoboObject *retval;

	g_return_val_if_fail (this != NULL, NULL);
	g_return_val_if_fail (oaf_iid != NULL, NULL);

	if (getenv ("DEBUG_EOG"))
		g_message ("Trying to produce a '%s'...", oaf_iid);

	image = eog_image_new ();
	if (!image)
		return NULL;

	if (!strcmp (oaf_iid, "OAFIID:GNOME_EOG_Control"))
		retval = BONOBO_OBJECT (eog_control_new (image));

#if NEED_GNOME2_PORTING
	else if (!strcmp (oaf_iid, "OAFIID:GNOME_EOG_Embeddable"))
		retval = BONOBO_OBJECT (eog_embeddable_new (image));
#endif

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

BONOBO_ACTIVATION_FACTORY("OAFIID:GNOME_EOG_Factory", "eog-image-viewer", 
			  VERSION, eog_image_viewer_factory, NULL);
