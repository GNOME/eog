/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * sample-container.c
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000 SuSE GmbH.
 */
#include <config.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>

#include <eog-util.h>

int
main (int argc, char *argv [])
{
	Bonobo_Unknown x, y, z;
	CORBA_Environment ev;
	CORBA_ORB orb;
	gchar *url;

	CORBA_exception_init (&ev);

	if (argc != 2)
		g_error ("Usage: %s url", argv[0]);

	gnome_init ("sample-container", "1.0", argc, (char **) argv);

	orb = oaf_init (argc, argv);

	if (bonobo_init (orb, NULL, NULL) == FALSE)
		g_error ("Cannot init bonobo");

	url = argv [1];

	x = bonobo_get_object (url, "IDL:GNOME/EOG/Image:1.0", &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_warning ("Failed to get object: %s",
			   bonobo_exception_get_text (&ev));

	g_message ("x = %p", x);
	if (x)
		g_message ("x is a %s", x->object_id);

	y = bonobo_get_object (url, "IDL:Bonobo/Control:1.0", &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_warning ("Failed to get object: %s",
			   bonobo_exception_get_text (&ev));

	g_message ("y = %p", y);
	if (y)
		g_message ("y is a %s", y->object_id);

	z = bonobo_get_object (url, "IDL:Bonobo/Embeddable:1.0", &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_warning ("Failed to get object: %s",
			   bonobo_exception_get_text (&ev));

	g_message ("z = %p", z);
	if (z)
		g_message ("z is a %s", z->object_id);

	return 0;
}

