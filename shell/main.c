#include <config.h>
#include <string.h>
#include <bonobo-activation/bonobo-activation.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnome/gnome-config.h>
#include <libgnomeui/gnome-client.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-window-icon.h>
#include <gconf/gconf-client.h>
#include <bonobo.h>
#include <bonobo/bonobo-ui-main.h>
#include "eog-window.h"
#include "session.h"

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
sort_startup_files (const gchar **files, GList **file_list, GList **dir_list, GList **error_list)
{
	gint i;
	GnomeVFSFileInfo *info;
	
	info = gnome_vfs_file_info_new ();

	for (i = 0; files [i]; i++) {
		GnomeVFSURI *uri;
		GnomeVFSResult result;
		char *filename;
		
		uri = make_canonical_uri (files[i]);

		result = gnome_vfs_get_file_info_uri (uri, info,
						      GNOME_VFS_FILE_INFO_DEFAULT |
						      GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

		filename = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);

		if (result != GNOME_VFS_OK)
			*error_list = g_list_append (*error_list, filename);
		else {
			if (info->type == GNOME_VFS_FILE_TYPE_REGULAR)
				*file_list = g_list_append (*file_list, filename);
			else if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
				*dir_list = g_list_append (*dir_list, filename);
			else
				*error_list = g_list_append (*error_list, filename);
		}

		gnome_vfs_uri_unref (uri);
		gnome_vfs_file_info_clear (info);
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
#ifdef EOG_COLLECTION_WORKS

static gint
user_wants_collection (gint n_windows)
{
	GtkWidget *dlg;
	gint ret;

	dlg = gtk_message_dialog_new (NULL,
				      GTK_DIALOG_MODAL,
				      GTK_MESSAGE_QUESTION,
				      GTK_BUTTONS_CANCEL,
				      _("You are about to open %i windows\n"
					"simultanously. Do you want to open\n"
					"them in a collection instead?"),
				      n_windows);
	
	gtk_dialog_add_button (GTK_DIALOG (dlg), _("Single Windows"), COLLECTION_NO);
	gtk_dialog_add_button (GTK_DIALOG (dlg), _("Collection"), COLLECTION_YES);

	ret = gtk_dialog_run (GTK_DIALOG (dlg));

	if (ret != COLLECTION_NO && ret != COLLECTION_YES) 
		ret = COLLECTION_CANCEL;

	gtk_widget_destroy (dlg);

	return ret;
}

#else

static gint
user_wants_collection (gint n_windows)
{
	GtkWidget *dlg;
	gint ret;
	
	dlg = gtk_message_dialog_new (NULL,
				      GTK_DIALOG_MODAL,
				      GTK_MESSAGE_QUESTION,
				      GTK_BUTTONS_CANCEL,
				      _("You are about to open %i windows\n"
					"simultanously. Do you want to continue?"),
				      n_windows);
	gtk_dialog_add_button (GTK_DIALOG (dlg), _("Open"), COLLECTION_NO);

	ret = gtk_dialog_run (GTK_DIALOG (dlg));

	if (ret != COLLECTION_NO)
		ret = COLLECTION_CANCEL;

	gtk_widget_destroy (dlg);

	return ret;
}
#endif /* EOG_COLLECTION WORKS */

/* Concatenates the strings in a list and separates them with newlines.  If the
 * list is empty, returns the empty string.  Returns the number of list elements in n.
 */
static char *
concat_string_list_with_newlines (GList *strings, int *n)
{
	int len;
	GList *l;
	char *str, *p;

	*n = 0;

	if (!strings)
		return g_strdup ("");

	len = 0;

	for (l = strings; l; l = l->next) {
		char *s;

		(*n)++;

		s = l->data;
		len += strlen (s) + 1; /* Add 1 for newline */
	}

	str = g_new (char, len);
	p = str;
	for (l = strings; l; l = l->next) {
		char *s;

		s = l->data;
		p = g_stpcpy (p, s);
		*p++ = '\n';
	}

	p--;
	*p = '\0';

	return str;
}

/* Callback used to process the response from the error dialog box */
static void
error_dialog_response_cb (GtkDialog *dialog, gint response_id)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));

	/* Terminate if there are no more windows open */
	if (!eog_get_window_list ())
		bonobo_main_quit ();
}

/* Shows an error dialog for files that do not exist */
static void
show_nonexistent_files (GList *error_list)
{
	char *str;
	char *msg;
	int n;
	GtkWidget *dialog;

	g_assert (error_list != NULL);

	str = concat_string_list_with_newlines (error_list, &n);

	if (n == 1)
		msg = g_strdup_printf (_("Could not access %s\n"
					 "Eye of Gnome will not be able to display this file."),
				       str);
	else
		msg = g_strdup_printf (_("The following files cannot be displayed "
					 "because Eye of Gnome was not able to "
					 "access them:\n"
					 "%s"),
				       str);

	g_free (str);

	dialog = gtk_message_dialog_new (
		NULL,
		0,
		GTK_MESSAGE_ERROR,
		GTK_BUTTONS_CANCEL,
		msg);
	g_free (msg);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (error_dialog_response_cb),
			  NULL);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_widget_show (dialog);
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
	GList *file_list;
	GList *dir_list;
	GList *error_list;
	const gchar **startup_files;
	poptContext ctx;

	ctx = data;
	startup_files = poptGetArgs (ctx);

	file_list = dir_list = error_list = NULL;

	/* sort cmdline arguments into file and dir list */
	if (startup_files)
		sort_startup_files (startup_files, &file_list, &dir_list, &error_list);
	else {
		gtk_idle_add (create_app, NULL);
		return FALSE;
	}

	/* open regular files */
	if (file_list) {
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
				poptFreeContext (ctx);
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
	if (dir_list) {
		open_in_single_windows (dir_list);
		g_list_free (dir_list);
	}

	if (error_list) {
		GList *l;

		show_nonexistent_files (error_list);

		for (l = error_list; l; l = l->next) {
			char *uri;

			uri = l->data;
			g_free (uri);
		}

		g_list_free (error_list);
	}
	
	/* clean up */
	poptFreeContext (ctx);

	return FALSE;
}

/* Callback used when the session manager asks us to save our state */
static gboolean
client_save_yourself_cb (GnomeClient *client,
			 gint phase,
			 GnomeSaveStyle save_style,
			 gboolean shutdown,
			 GnomeInteractStyle interact_style,
			 gboolean fast,
			 gpointer data)
{
	const char *prefix;
	char *discard_argv[] = { "rm", "-f", NULL };

	prefix = gnome_client_get_config_prefix (client);
	session_save (prefix);

	discard_argv[2] = gnome_config_get_real_path (prefix);
	gnome_client_set_discard_command (client, 3, discard_argv);
	g_free (discard_argv[2]);

	return TRUE;
}

/* Callback used when the session manager asks us to terminate.  We simply exit
 * the application by destroying all open windows.
 */
static void
client_die_cb (GnomeClient *client, gpointer data)
{
	GList *l;

	do {
		l = eog_get_window_list ();
		if (l) {
			EogWindow *window;

			window = EOG_WINDOW (l->data);
			gtk_widget_destroy (GTK_WIDGET (window));
		}
	} while (l);
}

int
main (int argc, char **argv)
{
	GnomeProgram *program;
	GError *error;
	poptContext ctx;
	GValue value = { 0 };
	GnomeClient *client;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

	program = gnome_program_init ("eog", VERSION,
				      LIBGNOMEUI_MODULE, argc, argv,
				      GNOME_PARAM_HUMAN_READABLE_NAME, _("Eye of Gnome"),
				      GNOME_PARAM_APP_DATADIR,DATADIR,NULL);

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

	gnome_window_icon_set_default_from_file (EOG_ICONDIR"/gnome-eog.png");

	client = gnome_master_client ();

	g_signal_connect (client, "save_yourself", G_CALLBACK (client_save_yourself_cb), NULL);
	g_signal_connect (client, "die", G_CALLBACK (client_die_cb), NULL);

	if (gnome_client_get_flags (client) & GNOME_CLIENT_RESTORED)
		session_load (gnome_client_get_config_prefix (client));
	else {
		g_value_init (&value, G_TYPE_POINTER);
		g_object_get_property (G_OBJECT (program), GNOME_PARAM_POPT_CONTEXT, &value);
		ctx = g_value_get_pointer (&value);
		g_value_unset (&value);

		gtk_idle_add (handle_cmdline_args, ctx);
	}

	bonobo_main ();

	return 0;
}
