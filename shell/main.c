/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors:
 *   Federico Mena-Quintero (federico@gnu.org)
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright (C) 2000-2001 The Free Software Foundation.
 * Copyright (C) 2001 SuSE GmbH.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <bonobo.h>

#include <eog-window.h>

static void
app_destroy_cb (GtkWidget *app, gpointer user_data)
{
	gtk_main_quit ();
}

static void
create_app (void)
{
	EogWindow *app;

	app = eog_window_new ("EOG Shell");

	gtk_signal_connect (GTK_OBJECT (app), "destroy",
			    GTK_SIGNAL_FUNC (app_destroy_cb), NULL);

	eog_window_launch_component (app, "OAFIID:GNOME_EOG_Control");

	gtk_widget_show_all (GTK_WIDGET (app));
}

int
main (int argc, char **argv)
{
	CORBA_ORB orb;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
	
        gnome_init_with_popt_table (PACKAGE, VERSION, argc, argv,
				    oaf_popt_options, 0, NULL); 
	gdk_rgb_init ();

	orb = oaf_init (argc, argv);

	if (bonobo_init (orb, NULL, NULL) == FALSE)
		g_error ("Could not initialize Bonobo");

	/* Do this now since we need it before the main loop. */
	bonobo_activate ();

	create_app ();

	bonobo_main ();

	return 0;
}
