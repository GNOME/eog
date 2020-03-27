/* Eye Of Gnome - General Utilities
 *
 * Copyright (C) 2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on code by:
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* For O_PATH */
#define _GNU_SOURCE

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/time.h>
#ifdef HAVE_STRPTIME
#define _XOPEN_SOURCE
#endif /* HAVE_STRPTIME */

#include <time.h>

#include "eog-util.h"
#include "eog-debug.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib/gi18n.h>
#include <sys/stat.h>
#include <sys/types.h>

void
eog_util_show_help (const gchar *section, GtkWindow *parent)
{
	GError *error = NULL;
	gchar *uri = NULL;

	if (section)
		uri = g_strdup_printf ("help:eog/%s", section);

	gtk_show_uri_on_window (parent, ((uri != NULL) ? uri : "help:eog"),
                                gtk_get_current_event_time (), &error);

	g_free (uri);

	if (error) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (parent,
						 0,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("Could not display help for Image Viewer"));

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  "%s", error->message);

		g_signal_connect_swapped (dialog, "response",
					  G_CALLBACK (gtk_widget_destroy),
					  dialog);
		gtk_widget_show (dialog);

		g_error_free (error);
	}
}

gchar *
eog_util_make_valid_utf8 (const gchar *str)
{
	GString *string;
	const char *remainder, *invalid;
	int remaining_bytes, valid_bytes;

	string = NULL;
	remainder = str;
	remaining_bytes = strlen (str);

	while (remaining_bytes != 0) {
		if (g_utf8_validate (remainder, remaining_bytes, &invalid)) {
			break;
		}

		valid_bytes = invalid - remainder;

		if (string == NULL) {
			string = g_string_sized_new (remaining_bytes);
		}

		g_string_append_len (string, remainder, valid_bytes);
		g_string_append_c (string, '?');

		remaining_bytes -= valid_bytes + 1;
		remainder = invalid + 1;
	}

	if (string == NULL) {
		return g_strdup (str);
	}

	g_string_append (string, remainder);
	g_string_append (string, _(" (invalid Unicode)"));

	g_assert (g_utf8_validate (string->str, -1, NULL));

	return g_string_free (string, FALSE);
}

GSList*
eog_util_parse_uri_string_list_to_file_list (const gchar *uri_list)
{
	GSList* file_list = NULL;
	gsize i = 0;
	gchar **uris;

	uris = g_uri_list_extract_uris (uri_list);

	while (uris[i] != NULL) {
		file_list = g_slist_append (file_list, g_file_new_for_uri (uris[i]));
		i++;
	}

	g_strfreev (uris);

	return file_list;
}

GSList*
eog_util_string_list_to_file_list (GSList *string_list)
{
	GSList *it = NULL;
	GSList *file_list = NULL;

	for (it = string_list; it != NULL; it = it->next) {
		char *uri_str;

		uri_str = (gchar *) it->data;

		file_list = g_slist_prepend (file_list,
					     g_file_new_for_uri (uri_str));
	}

	return g_slist_reverse (file_list);
}

GSList*
eog_util_strings_to_file_list (gchar **strings)
{
	int i;
 	GSList *file_list = NULL;

	for (i = 0; strings[i]; i++) {
 		file_list = g_slist_prepend (file_list,
					      g_file_new_for_uri (strings[i]));
 	}

 	return g_slist_reverse (file_list);
}

GSList*
eog_util_string_array_to_list (const gchar **files, gboolean create_uri)
{
	gint i;
	GSList *list = NULL;

	if (files == NULL) return list;

	for (i = 0; files[i]; i++) {
		char *str;

		if (create_uri) {
			GFile *file;

			file = g_file_new_for_commandline_arg (files[i]);
			str = g_file_get_uri (file);

			g_object_unref (file);
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

gchar **
eog_util_string_array_make_absolute (gchar **files)
{
	int i;
	int size;
	gchar **abs_files;
	GFile *file;

	if (files == NULL)
		return NULL;

	size = g_strv_length (files);

	/* Ensure new list is NULL-terminated */
	abs_files = g_new0 (gchar *, size+1);

	for (i = 0; i < size; i++) {
		file = g_file_new_for_commandline_arg (files[i]);
		abs_files[i] = g_file_get_uri (file);

		g_object_unref (file);
	}

	return abs_files;
}

static gchar *dot_dir = NULL;
static void migrate_config_folder (const gchar* new_folder);

static gboolean
ensure_dir_exists (const char *dir)
{
	if (g_file_test (dir, G_FILE_TEST_IS_DIR))
		return TRUE;

	if (g_mkdir_with_parents (dir, 0700) == 0) {
		/* If the folder is created try migrating from the old folder */
		migrate_config_folder (dir);
		return TRUE;
	}

	if (errno == EEXIST)
		return g_file_test (dir, G_FILE_TEST_IS_DIR);

	g_warning ("Failed to create directory %s: %s", dir, strerror (errno));
	return FALSE;
}

const gchar *
eog_util_dot_dir (void)
{
	if (dot_dir == NULL) {
		gboolean exists;

		dot_dir = g_build_filename (g_get_user_config_dir (),
					    "eog", NULL);

		exists = ensure_dir_exists (dot_dir);
		if (G_UNLIKELY (!exists)) {
			static gboolean printed_warning = FALSE;

			if (!printed_warning) {
				g_warning ("EOG could not save some of your preferences in its settings directory due to a file with the same name (%s) blocking its creation. Please remove that file, or move it away.", dot_dir);
				printed_warning = TRUE;
			}
			g_free (dot_dir);
			dot_dir = NULL;
			return NULL;
		}
	}

	return dot_dir;
}

static void migrate_config_file (const gchar *old_filename, const gchar* new_filename)
{
	GFile *old_file, *new_file;
	GError *error = NULL;

	if (!g_file_test (old_filename, G_FILE_TEST_IS_REGULAR))
		return;

	old_file = g_file_new_for_path (old_filename);
	new_file = g_file_new_for_path (new_filename);

	if (!g_file_move (old_file, new_file, G_FILE_COPY_NONE, NULL, NULL, NULL, &error)) {
		g_warning ("Could not migrate config file %s: %s\n", old_filename, error->message);
		g_error_free (error);
	}
	g_object_unref (new_file);
	g_object_unref (old_file);
}

static void migrate_config_folder (const gchar* new_dir)
{
	gchar* old_dir = g_build_filename (g_get_home_dir (), ".gnome2",
					   "eog", NULL);
	gchar* old_filename = NULL;
	gchar* new_filename = NULL;
	GError *error = NULL;
	GFile *dir_file = NULL;
	gsize i;
	static const gchar *old_files[] = { "eog-print-settings.ini",
					    NULL };

	if(!g_file_test (old_dir, G_FILE_TEST_IS_DIR)) {
		/* Nothing to migrate */
		g_free (old_dir);
		return;
	}

	eog_debug (DEBUG_PREFERENCES);

	for (i = 0; old_files[i] != NULL; i++) {
		old_filename = g_build_filename (old_dir,
						old_files[i], NULL);
		new_filename = g_build_filename (new_dir,
						old_files[i], NULL);

		migrate_config_file (old_filename, new_filename);

		g_free (new_filename);
		g_free (old_filename);
	}

	/* Migrate accels file */
	old_filename = g_build_filename (g_get_home_dir (), ".gnome2",
					 "accels", "eog", NULL);
	/* move file to ~/.config/eog/accels if its not already there */
	new_filename = g_build_filename (new_dir, "accels", NULL);

	migrate_config_file (old_filename, new_filename);

	g_free (new_filename);
	g_free (old_filename);

	dir_file = g_file_new_for_path (old_dir);
	if (!g_file_delete (dir_file, NULL, &error)) {
		g_warning ("An error occurred while deleting the old config folder %s: %s\n", old_dir, error->message);
		g_error_free (error);
	}
	g_object_unref (dir_file);
	g_free(old_dir);
}

/* Based on eel_filename_strip_extension() */

/**
 * eog_util_filename_get_extension:
 * @filename: a filename
 *
 * Returns a reasonably good guess of the file extension of @filename.
 *
 * Returns: a newly allocated string with the file extension of @filename.
 **/
char *
eog_util_filename_get_extension (const char * filename)
{
	char *begin, *begin2;

	if (filename == NULL) {
		return NULL;
	}

	begin = strrchr (filename, '.');

	if (begin && begin != filename) {
		if (strcmp (begin, ".gz") == 0 ||
		    strcmp (begin, ".bz2") == 0 ||
		    strcmp (begin, ".sit") == 0 ||
		    strcmp (begin, ".Z") == 0) {
			begin2 = begin - 1;
			while (begin2 > filename &&
			       *begin2 != '.') {
				begin2--;
			}
			if (begin2 != filename) {
				begin = begin2;
			}
		}
		begin ++;
	} else {
		return NULL;
	}

	return g_strdup (begin);
}


/**
 * eog_util_file_is_persistent:
 * @file: a #GFile
 *
 * Checks whether @file is a non-removable local mount.
 *
 * Returns: %TRUE if @file is in a non-removable mount,
 * %FALSE otherwise or when it is remote.
 **/
gboolean
eog_util_file_is_persistent (GFile *file)
{
	GMount *mount;

	if (!g_file_is_native (file))
		return FALSE;

	mount = g_file_find_enclosing_mount (file, NULL, NULL);
	if (mount) {
		if (g_mount_can_unmount (mount)) {
			return FALSE;
		}
	}

	return TRUE;
}

static void
_eog_util_show_file_in_filemanager_fallback (GFile *file, GtkWindow *toplevel)
{
	gchar *uri = NULL;
	GError *error = NULL;
	guint32 timestamp = gtk_get_current_event_time ();

	if (g_file_query_file_type (file, 0, NULL) == G_FILE_TYPE_DIRECTORY) {
		uri = g_file_get_uri (file);
	} else {
		/* If input file is not a directory we must open it's
		   folder/parent to avoid opening the file itself     */
		GFile *parent_file;

		parent_file = g_file_get_parent (file);
		if (G_LIKELY (parent_file))
			uri = g_file_get_uri (parent_file);
		g_object_unref (parent_file);
	}

	if (uri && !gtk_show_uri_on_window (toplevel, uri, timestamp, &error)) {
		g_warning ("Couldn't show containing folder \"%s\": %s", uri,
			   error->message);
		g_error_free (error);
	}

	g_free (uri);
}

void
eog_util_show_file_in_filemanager (GFile *file, GtkWindow *toplevel)
{
	GDBusProxy *proxy;
	gboolean done = FALSE;

	g_return_if_fail (file != NULL);

	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
				G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS |
				G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
				NULL, "org.freedesktop.FileManager1",
				"/org/freedesktop/FileManager1",
				"org.freedesktop.FileManager1",
				NULL, NULL);

	if (proxy) {
		gchar *uri = g_file_get_uri (file);
		gchar *startup_id;
		GVariant *params, *result;
		GVariantBuilder builder;

		g_variant_builder_init (&builder,
					G_VARIANT_TYPE ("as"));
		g_variant_builder_add (&builder, "s", uri);

		/* This seems to be the expected format, as other values
		   cause the filemanager window not to get focus. */
		startup_id = g_strdup_printf("_TIME%u",
					     gtk_get_current_event_time());

		/* params is floating! */
		params = g_variant_new ("(ass)", &builder, startup_id);

		g_free (startup_id);
		g_variant_builder_clear (&builder);

		/* Floating params-GVariant is consumed here */
		result = g_dbus_proxy_call_sync (proxy, "ShowItems",
						 params, G_DBUS_CALL_FLAGS_NONE,
						 -1, NULL, NULL);

		/* Receiving a non-NULL result counts as a successful call. */
		if (G_LIKELY (result != NULL)) {
			done = TRUE;
			g_variant_unref (result);
		}

		g_free (uri);
		g_object_unref (proxy);
	}

	/* Fallback to gtk_show_uri() if launch over DBus is not possible */
	if (!done)
		_eog_util_show_file_in_filemanager_fallback (file, toplevel);
}

/* Portal */

gboolean
eog_util_is_running_inside_flatpak (void)
{
	return g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
}

static void
response_cb (GDBusConnection *connection, const char *sender_name, const char *object_path, const char *interface_name, const char *signal_name, GVariant *parameters, gpointer user_data)
{
	GFile *file = G_FILE (user_data);
	guint32 response;
	guint signal_id;

	signal_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (file), "signal-id"));
	g_dbus_connection_signal_unsubscribe (connection, signal_id);

	g_variant_get (parameters, "(u@a{sv})", &response, NULL);
	if (response == 0) {
		g_debug ("Opening file");
	} else if (response == 1) {
		g_debug ("User cancelled opening file");
	} else {
		g_warning ("Failed to open file via portal");
	}
}

static void
open_file_complete_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
	GDBusProxy *proxy = G_DBUS_PROXY (source);
	GFile *file = G_FILE (user_data);
	GVariant *return_value = NULL;
	const char *handle;
	char *object_path = NULL;
	GError *error = NULL;

	return_value = g_dbus_proxy_call_with_unix_fd_list_finish (proxy, NULL, result, &error);
	if (!return_value) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Failed to open file via portal: %s", error->message);

		goto out;
	}

	g_variant_get (return_value, "(o)", &object_path);
	handle = (const char *)g_object_get_data (G_OBJECT (file), "handle");
	if (strcmp (handle, object_path) != 0) {
		GDBusConnection *connection;
		guint signal_id;

		connection = g_dbus_proxy_get_connection (proxy);
		signal_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (file), "signal-id"));
		g_dbus_connection_signal_unsubscribe (connection, signal_id);

		signal_id = g_dbus_connection_signal_subscribe (connection,
			"org.freedesktop.portal.Desktop",
			"org.freedesktop.portal.Request",
			"Response",
			handle,
			NULL,
			G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
			response_cb,
			file,
			NULL);
		g_object_set_data (G_OBJECT (file), "signal-id", GUINT_TO_POINTER (signal_id));
	}

out:
	if (return_value)
		g_variant_unref (return_value);
	if (object_path)
		g_free (object_path);
}

static void
open_with_flatpak_portal_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
	GVariantBuilder builder;
	GUnixFDList *fd_list;
	GFile *file;
	GDBusProxy *proxy;
	GDBusConnection *connection;
	GError  *error = NULL;
	guint signal_id;
	char *sender, *token, *handle;
	int fd;

	file = G_FILE (user_data);
	fd = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (file), "fd"));

	proxy = g_dbus_proxy_new_for_bus_finish (result, &error);
	if (!proxy) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Failed to create D-Bus proxy for OpenURI portal: %s", error->message);

		close (fd);
		return;
	}

	connection = g_dbus_proxy_get_connection (proxy);
	sender = g_strdup (g_dbus_connection_get_unique_name (connection) + 1);
	for (guint i = 0; sender[i] != '\0'; i++) {
		if (sender[i] == '.') {
			sender[i] = '_';
		}
	}

	token = g_strdup_printf ("eog%u", g_random_int ());
	handle = g_strdup_printf ("/org/freedesktop/portal/desktop/request/%s/%s", sender, token);

	g_object_set_data_full (G_OBJECT (file), "handle", handle, g_free);
	g_free (sender);

	signal_id = g_dbus_connection_signal_subscribe (connection,
			"org.freedesktop.portal.Desktop",
			"org.freedesktop.portal.Request",
			"Response",
			handle,
			NULL,
			G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
			response_cb,
			file,
			NULL);
	g_object_set_data (G_OBJECT (file), "signal-id", GUINT_TO_POINTER (signal_id));

	g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
	g_variant_builder_add (&builder, "{sv}", "handle_token", g_variant_new_string (token));
	g_variant_builder_add (&builder, "{sv}", "ask", g_variant_new ("b", TRUE));
	g_free (token);

	fd_list = g_unix_fd_list_new_from_array (&fd, 1);
	g_dbus_proxy_call_with_unix_fd_list (proxy,
			"OpenFile",
			g_variant_new ("(s@h@a{sv})",
			               "",
			              g_variant_new ("h", 0),
				            g_variant_builder_end (&builder)),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			fd_list,
			NULL,
			open_file_complete_cb,
			file);
	g_object_unref (fd_list);
}

void
eog_util_open_file_with_flatpak_portal (GFile *file)
{
	const gchar *path;
	int fd;

	path = g_file_get_path (file);
	fd = open (path, O_PATH | O_CLOEXEC);
	if (fd == -1) {
		g_warning ("Failed to open %s: %s", path, g_strerror (errno));
		return;
	}

	g_object_set_data (G_OBJECT (file), "fd", GINT_TO_POINTER (fd));
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
			G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
			NULL,
			"org.freedesktop.portal.Desktop",
			"/org/freedesktop/portal/desktop",
			"org.freedesktop.portal.OpenURI",
			NULL,
			open_with_flatpak_portal_cb,
			file);
}
