/* Eye of Gnome image viewer - image view widget
 *
 * Copyright (C) 2002 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnome/gnome-config.h>
#include "eog-window.h"
#include "session.h"



static void
load_uri_with_role (const char *uri, const char *role)
{
	GtkWidget *window;

	window = eog_window_new ();
	eog_window_open (EOG_WINDOW (window), uri);
	gtk_window_set_role (GTK_WINDOW (window), role);
	gtk_widget_show (window);
}

void
session_save (const char *config_prefix)
{
	GList *l;
	int i;

	gnome_config_push_prefix (config_prefix);

	i = 0;

	for (l = eog_get_window_list (); l; l++) {
		EogWindow *window;
		const char *uri;
		const char *role;
		char *key;

		window = EOG_WINDOW (l->data);

		uri = eog_window_get_uri (window);
		if (!uri)
			continue;

		role = gtk_window_get_role (GTK_WINDOW (window));
		g_assert (role != NULL);

		key = g_strdup_printf ("role_%d", i);
		gnome_config_set_string (key, role);
		g_free (key);

		key = g_strdup_printf ("uri_%d", i);
		gnome_config_set_string (key, uri);
		g_free (key);

		i++;
	}

	gnome_config_pop_prefix ();
}

void
session_load (const char *config_prefix)
{
	int i;

	gnome_config_push_prefix (config_prefix);

	i = 0;

	while (1) {
		char *key;
		char *role_value;
		char *uri_value;

		key = g_strdup_printf ("role_%d", i);
		role_value = gnome_config_get_string (key);
		g_free (key);

		if (!role_value)
			break;

		key = g_strdup_printf ("uri_%d", i);
		uri_value = gnome_config_get_string (key);
		g_free (key);

		if (!uri_value) {
			g_message ("Got role/ID %s but no URI key!  Stopping session load.",
				   role_value);
			g_free (role_value);
			break;
		}

		load_uri_with_role (uri_value, role_value);

		g_free (role_value);
		g_free (uri_value);

		i++;
	}

	gnome_config_pop_prefix ();
}
