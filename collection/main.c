/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * EOG Collection
 *
 * Authors:
 *   Jens Finke (jens@gnome.org)
 *
 * Copyright 2000, Helixcode Inc.
 * Copyright 2000, SuSE GmbH.
 * Copyright 2002, The Free Software Foundation
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>
/* This must be included _before_ bonobo-generic-factory.h,
 * otherwise BONOBO_ACTIVATION_FACTORY won't initialize the
 * gtk and other gui stuff! */
#include <bonobo/bonobo-ui-main.h> 
#include <bonobo/bonobo-generic-factory.h>

#include "eog-collection-view.h"

static BonoboObject *
eog_collection_factory (BonoboGenericFactory *this,
			const char           *oaf_iid,
			void                 *data)
{
	BonoboObject *retval;

	g_return_val_if_fail (this != NULL, NULL);
	g_return_val_if_fail (oaf_iid != NULL, NULL);

	if (!strcmp (oaf_iid, "OAFIID:GNOME_EOG_CollectionControl")) {
		retval = BONOBO_OBJECT (eog_collection_view_new ());
	} else {
		g_warning ("Unknown IID `%s' requested", oaf_iid);
		return NULL;
	}

	return retval;
}


int main (int argc, char *argv [])					
{									
	CORBA_Object factory;
	int i;
	gboolean nowait = FALSE;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);                       
	bind_textdomain_codeset (PACKAGE, "UTF-8");                     
	textdomain (PACKAGE);                                           
									
	BONOBO_FACTORY_INIT ("eog-collection", VERSION, &argc, argv);		

	for (i = 1; i < argc; i++) {
		if (!g_ascii_strcasecmp (argv[i], "--nowait")) {
			nowait = TRUE;
		}
	}
					
	factory = bonobo_activation_activate_from_id ("OAFIID:GNOME_EOG_CollectionFactory", 
						      Bonobo_ACTIVATION_FLAG_EXISTING_ONLY, NULL, NULL);

	if (!factory) {
		if (nowait) {
			/* this is mostly for debugging purpose */
			return bonobo_generic_factory_main ("OAFIID:GNOME_EOG_CollectionFactory",
							    eog_collection_factory, NULL);
		}
		else {
			return bonobo_generic_factory_main_timeout ("OAFIID:GNOME_EOG_CollectionFactory",
								    eog_collection_factory, NULL,
								    60000);	
		}
	}

	return 0;
}                                                                       

