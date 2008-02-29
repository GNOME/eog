/* Eye Of Gnome - Session Handler 
 *
 * Copyright (C) 2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on gedit code (gedit/gedit-session.h) by: 
 * 	- Gedit Team
 * 	- Federico Mena-Quintero <federico@ximian.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eog-session.h"
#include "eog-window.h"
#include "eog-application.h"

#include <libgnomeui/gnome-client.h>

static void
removed_from_session (GnomeClient *client, EogApplication *application)
{
	eog_application_shutdown (application);
}

static gint
save_session (GnomeClient *client, gint phase, GnomeSaveStyle save_style, gint shutdown,
	      GnomeInteractStyle interact_style, gint fast, EogApplication *application)
{
	return TRUE;
}

void
eog_session_init (EogApplication *application)
{
	GnomeClient *client;

	g_return_if_fail (EOG_IS_APPLICATION (application));

	client = gnome_master_client ();

	g_signal_connect (client, "save_yourself",
			  G_CALLBACK (save_session), application);	
	g_signal_connect (client, "die",
			  G_CALLBACK (removed_from_session), application);
}

gboolean
eog_session_is_restored	(void)
{
	return FALSE;
}

gboolean
eog_session_load (void)
{
	return TRUE;
}
