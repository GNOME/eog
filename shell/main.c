#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnome/gnome-config.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-client.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-window-icon.h>
#include <gconf/gconf-client.h>
#include "eog-hig-dialog.h"
#include "eog-window.h"
#include "session.h"
#include "eog-config-keys.h"

static void open_uri_list_cb (EogWindow *window, GSList *uri_list, gpointer data); 
static GtkWidget* create_new_window (void);


typedef struct {
	EogWindow *window;
	char      *iid;
	GList     *uri_list;
	gboolean  single_windows;
} LoadContext;

static void 
free_string_list (GList *list)
{
	GList *it;
	
	for (it = list; it != NULL; it = it->next) {
		g_free (it->data);
	}

	if (list != NULL) 
		g_list_free (list);
}

static void
free_load_context (LoadContext *ctx)
{
	if (ctx == NULL) return;

	free_string_list (ctx->uri_list);

	g_free (ctx);
}

static void
new_window_cb (EogWindow *window, gpointer data)
{
	create_new_window ();
}

static GtkWidget*
create_new_window (void)
{
	GtkWidget *window;
	GError *error = NULL;

	window = eog_window_new (&error);

	if (error != NULL) {
		GtkWidget *dlg;
		dlg = eog_hig_dialog_new (NULL, GTK_STOCK_DIALOG_ERROR,
					  _("Unable to create Eye of GNOME user interface"), error->message, TRUE);
		gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_OK, GTK_RESPONSE_OK);

		g_error_free (error);

		gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (GTK_WIDGET (dlg));

		gtk_main_quit ();
	}
	else {
		g_assert (EOG_IS_WINDOW (window));

		g_signal_connect (G_OBJECT (window), "open_uri_list", G_CALLBACK (open_uri_list_cb), NULL);
		g_signal_connect (G_OBJECT (window), "new_window", G_CALLBACK (new_window_cb), NULL);
		
		gtk_widget_show (window);
	}

	return window;
}

static gboolean
open_window (LoadContext *ctx)
{
	GtkWidget *window;
	GError *error = NULL;
	gboolean new_window;
	GList *it;
	GConfClient *client;

	g_return_val_if_fail (ctx->iid != NULL, FALSE);

	client = gconf_client_get_default ();

	new_window = gconf_client_get_bool (client, EOG_CONF_WINDOW_OPEN_NEW_WINDOW, NULL);
	new_window = new_window && ((ctx->window == NULL) || eog_window_has_contents (ctx->window));

	g_object_unref (client);

	if (ctx->single_windows) 
	{
		for (it = ctx->uri_list; it != NULL; it = it->next) {
			if (new_window || (ctx->window == NULL)) {
				window = create_new_window ();
			}
			else {
				window = GTK_WIDGET (ctx->window);
			}

			if (window != NULL) {
				if (!eog_window_open (EOG_WINDOW (window), ctx->iid, (char*) it->data, &error)) {
					g_print ("error open %s\n", (char*)it->data);
					/* FIXME: handle errors */
				}
			
				new_window = TRUE;
			}
		} 
	}
	else 
	{
		if (new_window || (ctx->window == NULL)) {
			window = create_new_window ();
		}
		else {
			window = GTK_WIDGET (ctx->window);
		}
		
		if (window != NULL) {
			if (!eog_window_open_list (EOG_WINDOW (window), ctx->iid, ctx->uri_list, &error)) {
				g_print ("error");
				/* report error */
			}
		}
	}
		
	free_load_context (ctx);

	return FALSE;
}


static gboolean
create_empty_window (gpointer data)
{
	create_new_window ();

	return FALSE;
}


static GnomeVFSURI*
make_canonical_uri (const char *path)
{
	char *uri_str;
	GnomeVFSURI *uri;

	uri_str = gnome_vfs_make_uri_from_shell_arg (path);

	uri = NULL;

	if (uri_str) {
		uri = gnome_vfs_uri_new (uri_str);
		g_free (uri_str);
	}

	return uri;
}


/**
 * sort_files:
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
sort_files (GSList *files, GList **file_list, GList **dir_list, GList **error_list)
{
	GSList *it;
	GnomeVFSFileInfo *info;
	
	info = gnome_vfs_file_info_new ();

	for (it = files; it != NULL; it = it->next) {
		GnomeVFSURI *uri;
		GnomeVFSResult result = GNOME_VFS_OK;
		char *filename;
		
		uri = make_canonical_uri ((char*)it->data);

		if (uri != NULL) {
			result = gnome_vfs_get_file_info_uri (uri, info,
							      GNOME_VFS_FILE_INFO_DEFAULT |
							      GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

			filename = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
		}
		else {
			filename = g_strdup ((char*) it->data);
		}

		if (result != GNOME_VFS_OK || uri == NULL)
			*error_list = g_list_append (*error_list, filename);
		else {
			if (info->type == GNOME_VFS_FILE_TYPE_REGULAR)
				*file_list = g_list_append (*file_list, filename);
			else if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
				*dir_list = g_list_append (*dir_list, filename);
			else
				*error_list = g_list_append (*error_list, filename);
		}

		if (uri != NULL) {
			gnome_vfs_uri_unref (uri);
		}
		gnome_vfs_file_info_clear (info);
	}

	gnome_vfs_file_info_unref (info);
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
	gchar body[128];
	int ret;

	g_snprintf (body, 128,
		    _("You are about to open %i windows simultaneously. Do you want to open them in a collection instead?"), 
		    n_windows);

	dlg = eog_hig_dialog_new (NULL, GTK_STOCK_DIALOG_WARNING,
				  _("Open multiple single windows?"),
				  body,
				  TRUE);

	gtk_dialog_add_button (GTK_DIALOG (dlg), _("Single Windows"), COLLECTION_NO);
	gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_CANCEL, COLLECTION_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dlg), _("Collection"), COLLECTION_YES);
       	gtk_dialog_set_default_response (GTK_DIALOG (dlg), COLLECTION_YES);

	gtk_widget_show_all (dlg);

	ret = gtk_dialog_run (GTK_DIALOG (dlg));

	if (ret != COLLECTION_NO && ret != COLLECTION_YES) 
		ret = COLLECTION_CANCEL;

	gtk_widget_destroy (dlg);

	return ret;
}

/* Shows an error dialog for files that do not exist */
static void
show_nonexistent_files (GList *error_list)
{
	GtkWidget *dlg;
	GList *it;
	int n = 0;
	int len;
	GString *detail;

	g_assert (error_list != NULL);

	len = g_list_length (error_list);
	detail = g_string_new ("");

	/* build string of newline separated filepaths */
	for (it = error_list; it != NULL; it = it->next) {
		char *str;

		if (n > 9) {
			/* don't display more than 10 files */
			detail = g_string_append (detail, "\n...");
			break;
		}

		str = gnome_vfs_format_uri_for_display ((char*) it->data);

		if (it != error_list) {
			detail = g_string_append (detail, "\n");
		}
		detail = g_string_append (detail, str);

		g_free (str);
		n++;
	}
	
	dlg = eog_hig_dialog_new (NULL, GTK_STOCK_DIALOG_ERROR, 
				  ngettext ("File not found.", "Files not found.", len),
				  detail->str, TRUE);
	gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_OK, GTK_RESPONSE_OK);
	
	gtk_widget_show (dlg);
	gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);

	g_string_free (detail, TRUE);
}

static GSList*
string_array_to_list (const gchar **files)
{
	gint i;
	GSList *list = NULL;

	if (files == NULL) return list;

	for (i = 0; files [i]; i++) {
		list = g_slist_prepend (list, g_strdup (files[i]));
	}

	return g_slist_reverse (list);
}

static void 
open_uri_list_cb (EogWindow *window, GSList *uri_list, gpointer data)
{
	GList *file_list = NULL;
	GList *dir_list = NULL;
	GList *error_list = NULL;
	LoadContext *ctx;
	gboolean quit_program = FALSE;

	if (uri_list == NULL) {
		if (window == NULL) {
			g_idle_add (create_empty_window, NULL);
		}

		return;
	}

	sort_files (uri_list, &file_list, &dir_list, &error_list);

	/* open regular files */
	if (file_list) {
		int result = COLLECTION_NO;
		int n_files = 0;

		n_files = g_list_length (file_list);
		if (n_files > 3 && n_files < 10) {
			result = user_wants_collection (g_list_length (file_list));
		}
		else if (n_files >= 10) {
			result = COLLECTION_YES;
		}
		else {
			result = COLLECTION_NO;
		}
	

		if (result == COLLECTION_YES) 
		{
			/* Do not free the file_list, this will be done
			   in the create_app_list function! */
			ctx = g_new0 (LoadContext, 1);
			ctx->window = window;
			ctx->iid = EOG_COLLECTION_CONTROL_IID;
			ctx->uri_list = file_list;
			ctx->single_windows = FALSE;
			
			g_idle_add ((GSourceFunc)open_window, ctx);
		} 
		else if (result == COLLECTION_NO)
		{
			ctx = g_new0 (LoadContext, 1);
			ctx->window = window;
			ctx->iid = EOG_VIEWER_CONTROL_IID;
			ctx->uri_list = file_list;
			ctx->single_windows = TRUE;
			
			/* open multiple windows */
			g_idle_add ((GSourceFunc)open_window, ctx);
		}
		else if (result == COLLECTION_CANCEL && window == NULL) {
			/* We quit the whole program only if we open the files
			   from the commandline. We would get the signal emitting 
			   window otherwise.
			*/
			quit_program = TRUE;
		}
	}
		
	/* open every directory in an own window */
	if (dir_list) {
		quit_program = FALSE;

		ctx = g_new0 (LoadContext, 1);
		ctx->window = window;
		ctx->iid = EOG_COLLECTION_CONTROL_IID;
		ctx->uri_list = dir_list;
		ctx->single_windows = TRUE;

		g_idle_add ((GSourceFunc)open_window, ctx);
	}

	/* show error for inaccessable files */
	if (error_list) {
		show_nonexistent_files (error_list);
		free_string_list (error_list);

		quit_program = (eog_get_window_list () == NULL);
	}

	if (quit_program) {
		gtk_main_quit ();
	}
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
	GSList *startup_file_list = NULL;
	const gchar **startup_files;
	poptContext ctx;

	ctx = data;
	startup_files = poptGetArgs (ctx);

	startup_file_list = string_array_to_list (startup_files);
	
	open_uri_list_cb (NULL, startup_file_list, NULL);
	
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
	eog_window_close_all ();
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
				      GNOME_PARAM_HUMAN_READABLE_NAME, _("Eye of GNOME"),
				      GNOME_PARAM_APP_DATADIR,DATADIR,NULL);

	error = NULL;
	if (gconf_init (argc, argv, &error) == FALSE) {
		g_assert (error != NULL);
		g_message ("GConf init failed: %s", error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}

	if(gnome_vfs_init () == FALSE)
		g_error ("Could not initialize GnomeVFS!");

	gnome_authentication_manager_init ();

	gnome_window_icon_set_default_from_file (EOG_ICONDIR"/gnome-eog.png");

	client = gnome_master_client ();

	g_signal_connect (client, "save_yourself", G_CALLBACK (client_save_yourself_cb), NULL);
	g_signal_connect (client, "die", G_CALLBACK (client_die_cb), NULL);

	if (gnome_client_get_flags (client) & GNOME_CLIENT_RESTORED) {
		session_load (gnome_client_get_config_prefix (client));
	}
	else  {
		g_value_init (&value, G_TYPE_POINTER);
		g_object_get_property (G_OBJECT (program), GNOME_PARAM_POPT_CONTEXT, &value);
		ctx = g_value_get_pointer (&value);
		g_value_unset (&value);

		g_idle_add (handle_cmdline_args, ctx);
	}

	gtk_main ();

	return 0;
}
