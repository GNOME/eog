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
#include "eog-job-queue.h"
#include "eog-application.h"

#include <string.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-config.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-client.h>
#include <libgnomeui/gnome-app-helper.h>
#include <libgnomeui/gnome-authentication-manager.h>
#include <libgnomevfs/gnome-vfs.h>
#ifdef HAVE_LEAFTAG
#include <libleaftag/leaftag.h>
#endif

static gboolean tags = FALSE;
static gchar **startup_files = NULL;

static const GOptionEntry goption_options[] =
{
#ifdef HAVE_LEAFTAG
	{ "tags", 0, 0, G_OPTION_ARG_NONE, &tags, N_("Open images by tag names"), NULL },
#endif
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &startup_files, NULL, N_("[FILE...]") },
	{ NULL }
};

static GSList*
string_array_to_list (const gchar **files, gboolean create_uri)
{
	gint i;
	GSList *list = NULL;

	if (files == NULL) return list;

	for (i = 0; files[i]; i++) {
		char *str;

		if (create_uri) {
			str = gnome_vfs_make_uri_from_input_with_dirs (files[i], 
								       GNOME_VFS_MAKE_URI_DIR_CURRENT);
		} else {
			str = g_strdup (files[i]);
		}

		if (str) {
			list = g_slist_prepend (list, g_strdup (str));
			g_free (str);
		}
	}

	return g_slist_reverse (list);
}

static void 
load_files ()
{
	GSList *files = NULL;

	files = string_array_to_list ((const gchar **) startup_files, TRUE);

	eog_application_open_uri_list (EOG_APP, 
				       files,
				       GDK_CURRENT_TIME,
				       NULL);

	g_slist_foreach (files, (GFunc) g_free, NULL);	
	g_slist_free (files);
}

#ifdef HAVE_LEAFTAG
static void 
load_files_from_tags ()
{
	GSList *tags = NULL;

	tags = string_array_to_list ((const gchar **) startup_files, FALSE);

	eog_application_open_tag_list (EOG_APP, 
				       tags,
				       GDK_CURRENT_TIME,
				       NULL);

	g_slist_foreach (tags, (GFunc) g_free, NULL);	
	g_slist_free (tags);
}
#endif

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

	gtk_window_set_default_icon_name ("image-viewer");
	g_set_application_name (_("Eye of GNOME Image Viewer"));

#ifdef HAVE_LEAFTAG
	if (!tags) {
#endif
		load_files ();
#ifdef HAVE_LEAFTAG
	} else {
		load_files_from_tags();
	}
#endif

	gtk_main ();

	gnome_accelerators_sync ();
	
  	if (startup_files)
		g_strfreev (startup_files);

	g_object_unref (program);

	return 0;
}
