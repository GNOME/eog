/* Eye Of Gnome - Main 
 *
 * Copyright (C) 2000-2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on code by:
 * 	- Federico Mena-Quintero <federico@gnu.org>
 *	- Jens Finke <jens@gnome.org>
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
#include "config.h"
#endif

#include "eog-session.h"
#include "eog-thumbnail.h"
#include "eog-stock-icons.h"
#include "eog-job-queue.h"
#include "eog-application.h"

#include <string.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-config.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-client.h>
#include <libgnomeui/gnome-authentication-manager.h>
#include <libgnomevfs/gnome-vfs.h>

static gchar **startup_files = NULL;

static const GOptionEntry goption_options[] =
{
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &startup_files, NULL, N_("[FILE...]") },
	{ NULL }
};

static GSList*
string_array_to_list (const gchar **files)
{
	gint i;
	GSList *list = NULL;

	if (files == NULL) return list;

	for (i = 0; files[i]; i++) {
		char *uri_str;

		uri_str = gnome_vfs_make_uri_from_shell_arg (files[i]);
	
		if (uri_str) {
			list = g_slist_prepend (list, g_strdup (uri_str));
			g_free (uri_str);
		}
	}

	return g_slist_reverse (list);
}

static void 
load_files ()
{
	GSList *files = NULL;

	files = string_array_to_list ((const gchar **) startup_files);

	eog_application_open_uri_list (EOG_APP, 
				       files,
				       GDK_CURRENT_TIME,
				       NULL);

	g_slist_foreach (files, (GFunc) g_free, NULL);	
	g_slist_free (files);
}

int
main (int argc, char **argv)
{
	GnomeProgram *program;
	GOptionContext *ctx;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

	ctx = g_option_context_new (NULL);
	g_option_context_add_main_entries (ctx, goption_options, PACKAGE);

	program = gnome_program_init ("eog", VERSION,
				      LIBGNOMEUI_MODULE, argc, argv,
				      GNOME_PARAM_GOPTION_CONTEXT, ctx,
				      GNOME_PARAM_HUMAN_READABLE_NAME, _("Eye of GNOME"),
				      GNOME_PARAM_APP_DATADIR, EOG_DATADIR,
				      NULL);

	gnome_authentication_manager_init ();

	eog_job_queue_init ();
	eog_thumbnail_init ();
	eog_stock_icons_init ();

	gtk_window_set_default_icon_name ("image-viewer");
	g_set_application_name (_("Eye of GNOME Image Viewer"));

	load_files ();

	gtk_main ();

  	if (startup_files)
		g_strfreev (startup_files);

	g_object_unref (program);

	return 0;
}
