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
#include <string.h>

/* This must be included _before_ bonobo-generic-factory.h,
 * otherwise BONOBO_ACTIVATION_FACTORY won't initialize the
 * gtk and other gui stuff! */
#include <bonobo/bonobo-ui-main.h> 

#include <bonobo/bonobo-generic-factory.h>
#include <libgnomevfs/gnome-vfs-init.h>

#include "eog-image-view.h"

static BonoboObject *
eog_image_viewer_factory (BonoboGenericFactory *this,
			  const char           *oaf_iid,
			  void                 *data)
{
	BonoboObject *retval;

	g_return_val_if_fail (this != NULL, NULL);
	g_return_val_if_fail (oaf_iid != NULL, NULL);

	g_message ("EoG2: Trying to produce a '%s'...", oaf_iid);

	if (!strcmp (oaf_iid, "OAFIID:GNOME_EOG_Control")) {
		retval = BONOBO_OBJECT (eog_image_view_new (FALSE));

	} else {
		g_warning ("Unknown IID `%s' requested", oaf_iid);
		return NULL;
	}

	return retval;
}

int main (int argc, char *argv [])					
{									
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);                       
	bind_textdomain_codeset (PACKAGE, "UTF-8");                     
	textdomain (PACKAGE);                                           
									
	BONOBO_FACTORY_INIT ("eog-image-viewer", VERSION, &argc, argv);	

	return bonobo_generic_factory_main ("OAFIID:GNOME_EOG_Factory",
					    eog_image_viewer_factory, NULL);	
}                                                                       
