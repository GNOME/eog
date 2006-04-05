#include "config.h"

#include "eog-session.h"
#include "eog-thumbnail.h"
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
		char *uri;

		uri = gnome_vfs_make_uri_from_shell_arg (files[i]);
	
		list = g_slist_prepend (list, g_strdup (uri));

		g_free (uri);
	}

	return g_slist_reverse (list);
}

static void 
load_files ()
{
	GSList *startup_list = NULL;

	startup_list = string_array_to_list ((const gchar **) startup_files);

	eog_application_open_uri_list (EOG_APP, 
				       startup_list,
				       GDK_CURRENT_TIME,
				       NULL);
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

	eog_thumbnail_init ();

	gtk_window_set_default_icon_name ("image-viewer");
	g_set_application_name (_("Eye of GNOME Image Viewer"));

	load_files ();

	gtk_main ();

	g_object_unref (program);

	return 0;
}
