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

#include <bonobo/bonobo-generic-factory.h>

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

BONOBO_OAF_FACTORY_MULTI ("OAFIID:GNOME_EOG_Factory", "eog-image-viewer", VERSION, eog_image_viewer_factory, NULL)
