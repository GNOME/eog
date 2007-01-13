#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <libgnome/gnome-config.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-client.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-authentication-manager.h>
#include <libgnomeui/gnome-app-helper.h>
#include <gconf/gconf-client.h>
#include "eog-window.h"
#include "eog-thumbnail.h"
#include "session.h"
#include "eog-config-keys.h"
#include "eog-image-list.h"
#include "eog-job-manager.h"

static char **startup_files = NULL;

static const GOptionEntry goption_options[] =
{
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &startup_files, NULL, N_("[FILE...]") },
	{ NULL }
};

static void open_uri_list_cb (EogWindow *window, GSList *uri_list, gpointer data); 

typedef struct {
	EogWindow *window;
	GList     *uri_list;
	EogImageList  *img_list;
} LoadContext;

#ifdef G_OS_WIN32

#include <windows.h>
#include "util.h"

static const char *
get_installation_subdir (const char *configure_time_path)
{
  char *full_prefix, *cp_prefix;
  const char *retval;

  gnome_win32_get_prefixes (GetModuleHandle (NULL), &full_prefix, &cp_prefix);

  retval = g_build_filename (full_prefix, configure_time_path + strlen (EOG_PREFIX), NULL);

  g_free (full_prefix);
  g_free (cp_prefix);

  return retval;
}

const char *
eog_get_datadir (void)
{
  static const char *datadir = NULL;

  if (datadir == NULL)
    datadir = get_installation_subdir (EOG_DATADIR);

  return datadir;
}

#undef EOG_DATADIR
#define EOG_DATADIR eog_get_datadir ()

const char *
eog_get_icondir (void)
{
  static const char *icondir = NULL;

  if (icondir == NULL)
    icondir = get_installation_subdir (EOG_ICONDIR);

  return icondir;
}

#undef EOG_ICONDIR
#define EOG_ICONDIR eog_get_icondir ()

static const char *
eog_get_localedir (void)
{
  char *full_prefix, *cp_prefix;
  const char *localedir;

  gnome_win32_get_prefixes (GetModuleHandle (NULL), &full_prefix, &cp_prefix);

  localedir = g_build_filename (cp_prefix, GNOMELOCALEDIR + strlen (EOG_PREFIX), NULL);

  g_free (full_prefix);
  g_free (cp_prefix);

  return localedir;
}

#undef GNOMELOCALEDIR
#define GNOMELOCALEDIR eog_get_localedir ()



#endif	/* G_OS_WIN32 */


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
load_context_free (LoadContext *ctx)
{
	if (ctx == NULL) return;

	gnome_vfs_uri_list_free (ctx->uri_list);
	if (ctx->img_list != NULL)
			g_object_unref (ctx->img_list);
	ctx->img_list = NULL;
	
	g_free (ctx);
}

static LoadContext*
load_context_new (EogWindow *window, GList *uri_list)
{
	LoadContext *ctx;
	
	ctx = g_new0 (LoadContext, 1);
	ctx->window = window;
	ctx->uri_list = uri_list;
	ctx->img_list = NULL;

	return ctx;
}

static GtkWidget*
create_new_window (void)
{
	GtkWidget *window;
	GError *error = NULL;

	window = eog_window_new (&error);

	if (error != NULL) {
		GtkWidget *dlg;

		dlg = gtk_message_dialog_new (NULL,
					      GTK_DIALOG_MODAL,
					      GTK_MESSAGE_ERROR,
					      GTK_BUTTONS_OK,
					      _("Unable to create Eye of GNOME user interface"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dlg),
							  error->message);

		gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (GTK_WIDGET (dlg));

		g_error_free (error);

		gtk_main_quit ();
	}
	else {
		g_assert (EOG_IS_WINDOW (window));

		g_signal_connect (G_OBJECT (window), "open_uri_list", G_CALLBACK (open_uri_list_cb), NULL);
	}

	return window;
}

static void
assign_model_to_window (EogWindow *window, EogImageList *list)
{
	GError *error = NULL;

	if (window == NULL)
		window = EOG_WINDOW (create_new_window ());
	
	if (window == NULL) return;
	
	if (list != NULL) {
		eog_image_list_print_debug (list);
	}
	
	if (!eog_window_open (window, list, &error)) {
		/* FIXME: show error, free image list */
		g_print ("error open %s\n", (char*) error->message);
	}
	
	gtk_widget_show (GTK_WIDGET (window));
}

static void
create_empty_window (void)
{
       GtkWidget *window;

       window = create_new_window ();
       if (window != NULL) {
               eog_window_open (EOG_WINDOW (window), NULL, NULL);
               gtk_widget_show (window);
       }
}

static GnomeVFSURI*
make_canonical_uri (const char *path)
{
	char *uri_str;
	GnomeVFSURI *uri = NULL;

	uri_str = gnome_vfs_make_uri_from_input_with_dirs (path, GNOME_VFS_MAKE_URI_DIR_CURRENT);

	if (uri_str) {
		uri = gnome_vfs_uri_new (uri_str);
		g_free (uri_str);
	}

	return uri;
}

static GnomeVFSFileType
check_uri_file_type (GnomeVFSURI *uri, GnomeVFSFileInfo *info)
{
	GnomeVFSFileType type = GNOME_VFS_FILE_TYPE_UNKNOWN;
	GnomeVFSResult result;

	g_return_val_if_fail (uri != NULL, GNOME_VFS_FILE_TYPE_UNKNOWN);
	g_return_val_if_fail (info != NULL, GNOME_VFS_FILE_TYPE_UNKNOWN);

	gnome_vfs_file_info_clear (info);
	
	result = gnome_vfs_get_file_info_uri (uri, info,
					      GNOME_VFS_FILE_INFO_DEFAULT |
					      GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	
	if (result == GNOME_VFS_OK &&
	    ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) != 0))
	{
		type = info->type;
	}

	return type;
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
		GnomeVFSFileType type = GNOME_VFS_FILE_TYPE_UNKNOWN;
		char *argument;

		argument = (char*) it->data;
		uri = make_canonical_uri (argument);

		if (uri != NULL) {
			type = check_uri_file_type (uri, info);
		}

		switch (type) {
		case GNOME_VFS_FILE_TYPE_REGULAR:
			*file_list = g_list_prepend (*file_list, gnome_vfs_uri_ref (uri));
			break;
		case GNOME_VFS_FILE_TYPE_DIRECTORY:
			*dir_list = g_list_prepend (*dir_list, gnome_vfs_uri_ref (uri));
			break;
		default:
			*error_list = g_list_prepend (*error_list, 
						      gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE));
			break;
		}

		if (uri != NULL) {
			gnome_vfs_uri_unref (uri);
		}
	}

	/* reverse lists for correct order */
	*file_list  = g_list_reverse (*file_list);
	*dir_list   = g_list_reverse (*dir_list);
	*error_list = g_list_reverse (*error_list);

	gnome_vfs_file_info_unref (info);
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

	dlg = gtk_message_dialog_new (NULL,
				      GTK_DIALOG_MODAL,
				      GTK_MESSAGE_ERROR,
				      GTK_BUTTONS_OK,
				      ngettext ("File not found.", "Files not found.", len));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dlg), detail->str);

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
job_prepare_model_do (EogJob *job, gpointer data, GError **error)
{
	LoadContext *ctx = (LoadContext*) data;
	int initial_pos;
	
	ctx->img_list = eog_image_list_new ();
	
	/* prepare the image list */
	eog_image_list_add_uris (ctx->img_list, ctx->uri_list);

	initial_pos = eog_image_list_get_initial_pos (ctx->img_list);

	if (initial_pos != -1) {
		EogImage *img;

		img = eog_image_list_get_img_by_pos (ctx->img_list, initial_pos);
		if (EOG_IS_IMAGE (img)) {
			eog_image_load (img, EOG_IMAGE_DATA_ALL, job, error);
			g_object_unref (img);
		}
	}
}

static void
job_prepare_model_finished (EogJob *job, gpointer data, GError *error)
{
	LoadContext *ctx = (LoadContext*) data;
	
	if (eog_job_get_status (job) == EOG_JOB_STATUS_FINISHED &&
		eog_job_get_success (job))
	{
		g_object_ref (ctx->img_list);
		
		assign_model_to_window (ctx->window, ctx->img_list);
	} 
	else {
		g_error_free (error);
		gtk_main_quit ();
	}
}

static void 
job_prepare_model_free (gpointer data)
{
	load_context_free ((LoadContext*) data);
	data = NULL;
}
	
static void
load_uris_in_single_model (EogWindow *window, GList *uri_list)
{
	GList *it;
	LoadContext *ctx;
	
	for (it = uri_list; it != NULL; it = it->next) {
		GList *singleton = NULL;
		EogJob *job;
		
		singleton = g_list_prepend (singleton, it->data);		
		ctx = load_context_new (window, singleton);
	
		job = eog_job_new_full (ctx, job_prepare_model_do, job_prepare_model_finished,
						NULL,
				        NULL, job_prepare_model_free);
		eog_job_manager_add (job);
		g_object_unref (job);
		
		window = NULL;
	}
}

static EogWindow*
check_window_reuse (EogWindow *window)
{
	gboolean new_window = FALSE;
	GConfClient *client;
	EogWindow *use_window = NULL;

	client = gconf_client_get_default ();

	new_window = gconf_client_get_bool (client, EOG_CONF_WINDOW_OPEN_NEW_WINDOW, NULL);
	new_window = new_window && (window == NULL || eog_window_has_contents (window));
	
	if (!new_window)
		use_window = window;
	
	g_object_unref (client);
	
	return use_window;
}

static void 
open_uri_list_cb (EogWindow *window, GSList *uri_list, gpointer data)
{
	GList *file_list = NULL;
	GList *dir_list = NULL;
	GList *error_list = NULL;
	gboolean quit_program = FALSE;
	
	if (uri_list == NULL) {
		if (window == NULL) {
			create_empty_window ();
		}

		return;
	}

	sort_files (uri_list, &file_list, &dir_list, &error_list);

	/* check if we can/should reuse the window */
	window = check_window_reuse (window);
	
	/* open regular files */
	if (file_list) {
		LoadContext *ctx;				
		EogJob *job;
		ctx = load_context_new (window, file_list);
		window = NULL;
	
		job = eog_job_new_full (ctx, job_prepare_model_do, job_prepare_model_finished, 
					NULL, NULL, job_prepare_model_free);

		eog_job_manager_add (job);
		g_object_unref (job);
	}
		
	/* open every directory in an own window */
	if (dir_list) {
		quit_program = FALSE;
		load_uris_in_single_model (window, dir_list);		
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
 * @data: NULL
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

	startup_file_list = string_array_to_list ((const gchar **) startup_files);
	
	open_uri_list_cb (NULL, startup_file_list, NULL);

	g_slist_foreach (startup_file_list, (GFunc) g_free, NULL);
	g_slist_free (startup_file_list);
	
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
	GOptionContext *ctx;
	GnomeClient *client;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

	ctx = g_option_context_new (NULL);
	g_option_context_add_main_entries (ctx, goption_options, PACKAGE);

	program = gnome_program_init ("eog", VERSION,
				      LIBGNOMEUI_MODULE, argc, argv,
				      GNOME_PARAM_GOPTION_CONTEXT, ctx,
				      GNOME_PARAM_HUMAN_READABLE_NAME, _("Eye of GNOME"),
				      GNOME_PARAM_APP_DATADIR,EOG_DATADIR,NULL);

	if (gnome_vfs_init () == FALSE) {
		g_error ("Could not initialize GnomeVFS!");
		exit (EXIT_FAILURE);
	}

#ifndef G_OS_WIN32
	gnome_authentication_manager_init ();
#endif
	eog_thumbnail_init ();

	gtk_window_set_default_icon_name ("eog");

	client = gnome_master_client ();

	g_signal_connect (client, "save_yourself", G_CALLBACK (client_save_yourself_cb), NULL);
	g_signal_connect (client, "die", G_CALLBACK (client_die_cb), NULL);

	if (gnome_client_get_flags (client) & GNOME_CLIENT_RESTORED) {
		session_load (gnome_client_get_config_prefix (client));
	}
	else  {
		g_idle_add (handle_cmdline_args, NULL);
	}

	gtk_main ();

	gnome_accelerators_sync();
	
	g_object_unref (program);

	return 0;
}
