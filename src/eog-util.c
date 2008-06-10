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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/time.h>
#ifdef HAVE_STRPTIME
#define _XOPEN_SOURCE
#endif /* HAVE_STRPTIME */

#include <time.h>

#include "eog-util.h"

#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-help.h>
#include <libgnome/gnome-init.h>
#include <libgnome/gnome-desktop-item.h>

void 
eog_util_show_help (const gchar *section, GtkWindow *parent)
{
	GError *error = NULL;

	gnome_help_display ("eog.xml", section, &error);

	if (error) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (parent,
						 0,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("Could not display help for Eye of GNOME"));

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
	const char *start, *end;
	char *uri;

	start = uri_list;
	end = strchr (uri_list, '\r');
	uri = g_strndup (start, end - start);
	file_list = g_slist_append (file_list, g_file_new_for_uri (uri));
	g_free (uri);

	while (strlen (end) > 2)
	{
		start = (end + 2);
		end = strchr (start, '\r');
		uri = g_strndup (start, end - start);
		file_list = g_slist_append (file_list, g_file_new_for_uri (uri));
		g_free (uri);
	}
	
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

#ifdef HAVE_DBUS
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
#endif

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

static int
launch_desktop_item (const char *desktop_file,
                     guint32 user_time,
                     GError **error)
{
	GnomeDesktopItem *item = NULL;
	GList *uris = NULL;
	int ret = -1;
	
	item = gnome_desktop_item_new_from_file (desktop_file, 0, NULL);

	if (item == NULL) return FALSE;
	
	gnome_desktop_item_set_launch_time (item, user_time);

	ret = gnome_desktop_item_launch (item, uris, 0, error);
	
	g_list_foreach (uris, (GFunc) g_free, NULL);
	g_list_free (uris);
	gnome_desktop_item_unref (item);
	
	return ret;
}

gboolean
eog_util_launch_desktop_file (const gchar *filename,
                              guint32      user_time)
{
	GError *error = NULL;
	const char * const *dirs;
	char *path = NULL;
	int i, ret = -1;
	
	dirs = g_get_system_data_dirs ();

	if (dirs == NULL) return FALSE;
	
	for (i = 0; dirs[i] != NULL; i++) {
	        path = g_build_filename (dirs[i], 
					 "applications", 
					 filename, 
					 NULL);

	        if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) break;
	
	        g_free (path);
		path = NULL;
	}
	
	if (path != NULL) {
	        ret = launch_desktop_item (path, user_time, &error);
	
	        if (ret == -1 || error != NULL) {
	                g_warning ("Cannot launch desktop item '%s': %s\n",
	                           path, 
				   error ? error->message : "unknown error");

	                g_clear_error (&error);
	        }
	
	        g_free (path);
	}
	
	return ret >= 0;
}

/* Above code courtesy of Evince hackers */

static gchar *dot_dir = NULL;

static gboolean
ensure_dir_exists (const char *dir)
{
	if (g_file_test (dir, G_FILE_TEST_IS_DIR))
		return TRUE;
	
	if (g_mkdir (dir, 488) == 0)
		return TRUE;

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
		
		dot_dir = g_build_filename (gnome_user_dir_get (),
					    "eog",
					    NULL);
		
		exists = ensure_dir_exists (dot_dir);

		if (G_UNLIKELY (!exists)) {
			static gboolean printed_warning = FALSE;

			if (!printed_warning) {
				g_warning ("EOG could not save some of your preferences in its settings directory due to a file with the same name (%s) blocking its creation. Please remove that file, or move it away.", dot_dir);
				printed_warning = TRUE;
			}
			dot_dir = NULL;
			return NULL;
		}
	}

	return dot_dir;
}
