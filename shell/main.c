#include "../config.h"

#include <gnome.h>
#include <bonobo-activation/bonobo-activation.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomeui/gnome-window-icon.h>
#include <gconf/gconf-client.h>
#include <bonobo.h>
#include <bonobo/bonobo-ui-main.h>
#include "eog-window.h"

static gboolean
create_app (gpointer data)
{
	GtkWidget *win;
	gchar *uri;

	win = eog_window_new ();
	uri = (gchar*) data;

	gtk_widget_show (win);

	if (uri) {
		eog_window_open (EOG_WINDOW (win), uri);
		g_free (uri);
	}
	
	return FALSE;
}

static gboolean
create_app_list (gpointer data)
{
	GtkWidget *win;
	GList *list;
	GList *node;

	win = eog_window_new ();
	list = (GList*) data;

	gtk_widget_show (win);

	if (list) {
		eog_window_open_list (EOG_WINDOW (win), data);
		for (node=list; node!=NULL; node=node->next) g_free ((gchar*)node->data);
		g_list_free (list);
	}

	return FALSE;
}

static GnomeVFSURI*
make_canonical_uri (const gchar *path)
{
	GnomeVFSURI *uri;
	gchar *current_dir;
	gchar *canonical;
	gchar *concat_path;

	g_return_val_if_fail (path != NULL, NULL);

	if (strchr (path, ':') != NULL)
		return gnome_vfs_uri_new (path);

	if (path[0] == '/')
		return gnome_vfs_uri_new (path);

	current_dir = g_get_current_dir ();
	/* g_get_current_dir returns w/o trailing / */
	concat_path = g_strconcat (current_dir, "/", path, NULL);
	canonical = gnome_vfs_make_path_name_canonical (concat_path);
	
	uri = gnome_vfs_uri_new (canonical);

	g_free (current_dir);
	g_free (canonical);
	g_free (concat_path);

	return uri;
}


/**
 * sort_startup_files:
 * @files: Input list of additional command line arguments.
 * @file_list: Return value, contains all the files.
 * @dir_list: Return value, contains all the directories given 
 *            on the command line.
 *
 * Sorts all the command line arguments into two lists, according to 
 * their GNOME_VFS_FILE_TYPE. This results in one list with all 
 * regular files and one list with all directories.
 **/
static void
sort_startup_files (const gchar **files, GList **file_list, GList **dir_list)
{
	gint i;
	GnomeVFSFileInfo *info;
	
	info = gnome_vfs_file_info_new ();

	for (i = 0; files [i]; i++) {
		GnomeVFSURI *uri;
		
		uri = make_canonical_uri (files[i]);

		gnome_vfs_get_file_info_uri (uri, info,
					     GNOME_VFS_FILE_INFO_DEFAULT |
					     GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

		if (info->type == GNOME_VFS_FILE_TYPE_REGULAR)
			*file_list = g_list_append (*file_list, gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE));
		else if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
			*dir_list = g_list_append (*dir_list, gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE));
		else g_warning ("%s has file type: %i\n", 
				gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE),
				info->type);
		gnome_vfs_uri_unref (uri);
	}

	gnome_vfs_file_info_unref (info);
}


static void
open_in_single_windows (GList *list)
{
	for (; list; list = list->next) {
		gtk_idle_add (create_app,
			      (gchar*) list->data);
	}
}

enum {
	COLLECTION_CANCEL,
	COLLECTION_NO,
	COLLECTION_YES
};

/**
 * user_wants_collection:
 * @n_windows: The number of windows eog will open.
 * 
 * Pop ups a dialog which asks the user if he wants to open n_windows or if he
 * prefers a single window with a collection in it.
 *
 * @Return value: TRUE, if a collection should be used, else FALSE.
 * */
static gint
user_wants_collection (gint n_windows)
{
	GtkWidget *dlg;
	gint ret;
	gchar *msg;
	
	msg = g_new0(gchar, 120);
	g_snprintf (msg, 120, 
		    _("You are about to open %i windows\n"
		      "simultanously. Do you want to open\n"
		      "them in a collection instead?"),
		    n_windows);
	
	dlg = gnome_message_box_new (msg,
				     GNOME_MESSAGE_BOX_QUESTION,
				     GNOME_STOCK_BUTTON_CANCEL,
				     _("Single Windows"), _("Collection"), 
				     NULL);		
	ret = gnome_dialog_run (GNOME_DIALOG (dlg));
	g_free (msg);

	if (ret == -1) ret = COLLECTION_CANCEL;

	return ret;
}

/**
 * handle_cmdline_args:
 * @data: Pointer to the popt context provided by gnome_init_with_popt_table.
 *
 * Handles command line arguments. Counts the regular files the user wants to
 * open and if there are more than three, asks him to open them in a single
 * collection instead. This check will only be performed for regular files not
 * for directories.
 *
 * @Return value: Always FALSE, to prevent idle dispatcher from calling this
 * function again.
 **/
static gboolean
handle_cmdline_args (gpointer data)
{
	GList *dir_list = NULL;
	GList *file_list = NULL;
	const gchar **startup_files;
	poptContext *ctx;

	ctx = (poptContext*) data;
	startup_files = poptGetArgs (*ctx);

	/* sort cmdline arguments into file and dir list */
	if (startup_files)
		sort_startup_files (startup_files, &file_list, &dir_list);
	else {
		gtk_idle_add (create_app, NULL);
		return FALSE;
	}

	/* open regular files */
	if (file_list != NULL) {
		if (g_list_length (file_list) > 3) {
			gint ret = user_wants_collection (g_list_length (file_list));
			if (ret == COLLECTION_YES) {
				/* Do not free the file_list, this will be done
				 in the create_app_list function! */
				gtk_idle_add (create_app_list, 
					      file_list);
			} else if (ret == COLLECTION_NO) {
				/* open multiple windows */
				open_in_single_windows (file_list);
				g_list_free (file_list);
			} else { /* quit whole program */
				GList *node;
				poptFreeContext (*ctx);
				g_free (ctx);
				for (node = file_list; node; node = node->next) g_free (node->data);
				for (node = dir_list; node; node = node->next) g_free (node->data);
				g_list_free (file_list);
				g_list_free (dir_list);
				bonobo_main_quit ();
				return FALSE;
			}
		} else {
			open_in_single_windows (file_list);
			g_list_free (file_list);
		}
	}
		
	/* open every directory in an own window */
	if (dir_list != NULL) {
		open_in_single_windows (dir_list);
		g_list_free (dir_list);
	}
	
	/* clean up */
	poptFreeContext (*ctx);
	g_free (ctx);

	return FALSE;
}

int
main (int argc, char **argv)
{
	CORBA_Environment ev;
	CORBA_ORB orb;
	GError *error;
	poptContext *ctx;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

	ctx = g_new0 (poptContext, 1);
	gnome_init_with_popt_table ("Eye of Gnome", VERSION,
		    argc, argv, NULL, 0, ctx);

	CORBA_exception_init (&ev);
	/* FIXME GNOME2 shouldn't be necessary, right?
	   orb = oaf_init (argc, argv);
	*/

	error = NULL;
	if (gconf_init (argc, argv, &error) == FALSE) {
		g_assert (error != NULL);
		g_message ("GConf init failed: %s", error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}

	if(gnome_vfs_init () == FALSE)
		g_error (_("Could not initialize GnomeVFS!\n"));

	if (bonobo_ui_init ("Eye Of GNOME", VERSION, &argc, argv) == FALSE)
		g_error (_("Could not initialize Bonobo!\n"));

	if (*ctx) {
		gtk_idle_add (handle_cmdline_args, ctx);
	} else
		gtk_idle_add (create_app, NULL);

	gnome_window_icon_set_default_from_file (EOG_ICONDIR"/gnome-eog.png");

	bonobo_main ();

	CORBA_exception_free (&ev);
	
	return 0;
}
