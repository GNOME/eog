/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * sample-container.c
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000 SuSE GmbH.
 */
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>

static void
test_url (const gchar *url, const gchar *interface) 
{
	Bonobo_Unknown object;
	CORBA_Environment ev;

	g_message ("Querying %s", url);
	
	CORBA_exception_init (&ev);

	object = bonobo_get_object (url, interface, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Failed to get object: %s",
			   bonobo_exception_get_text (&ev));
	} else {
		g_message ("object = %p", object);
		g_message ("object is a %s", object->object_id);
		bonobo_object_release_unref (object, NULL);
	}

	CORBA_exception_free (&ev);
}

int
main (int argc, char *argv [])
{
	CORBA_ORB orb;
	gchar *url;

	if (argc != 2) {
		g_print ("Usage: %s path_to_file\n", argv [0]);
		exit (1);
	}

	gnome_init ("sample-container", "1.0", argc, (char **) argv);

	orb = oaf_init (argc, argv);

	if (bonobo_init (orb, NULL, NULL) == FALSE)
		g_error ("Cannot init bonobo");

	url = g_strdup_printf ("file:%s", argv [1]);
	test_url (url, "IDL:GNOME/EOG/Image:1.0");
	g_free (url);

	url = g_strdup_printf ("file:%s!control", argv [1]);
	test_url (url, "IDL:Bonobo/Control:1.0");
	g_free (url);

	url = g_strdup_printf ("file:%s!embeddable", argv [1]);
	test_url (url, "IDL:Bonobo/Embeddable:1.0");
	g_free (url);

	return 0;
}

