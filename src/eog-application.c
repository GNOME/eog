/* Eye Of Gnome - Application Facade 
 *
 * Copyright (C) 2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on evince code (shell/ev-application.h) by: 
 * 	- Martin Kretzschmar <martink@gnome.org>
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

#include "eog-image.h"
#include "eog-session.h"
#include "eog-window.h"
#include "eog-application.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#ifdef HAVE_LEAFTAG
#include <libleaftag/leaftag.h>
#endif

#define EOG_APPLICATION_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EOG_TYPE_APPLICATION, EogApplicationPrivate))

G_DEFINE_TYPE (EogApplication, eog_application, G_TYPE_OBJECT);

static void
eog_application_class_init (EogApplicationClass *eog_application_class)
{
}

static void
eog_application_init (EogApplication *eog_application)
{
	eog_session_init (eog_application);
}

EogApplication *
eog_application_get_instance (void)
{
	static EogApplication *instance;

	if (!instance) {
		instance = EOG_APPLICATION (g_object_new (EOG_TYPE_APPLICATION, NULL));
	}

	return instance;
}

gboolean
eog_application_open_window (EogApplication  *application,
			     guint32         timestamp,
			     GError         **error,
			     EogStartupFlags flags)
{
	GtkWidget *new_window = eog_window_new (flags);
  
	g_return_val_if_fail (EOG_IS_APPLICATION (application), FALSE);

	gtk_window_present_with_time (GTK_WINDOW (new_window),
				      timestamp);

	return TRUE;
}

static EogWindow *
eog_application_get_empty_window (EogApplication *application)
{
	EogWindow *empty_window = NULL;
	GList *windows;
	GList *l;

	g_return_val_if_fail (EOG_IS_APPLICATION (application), NULL);

	windows = eog_application_get_windows (application);

	for (l = windows; l != NULL; l = l->next) {
		EogWindow *window = EOG_WINDOW (l->data);

		if (eog_window_is_empty (window)) {
			empty_window = window;
			break;
		}
	}

	g_list_free (windows);
	
	return empty_window;
}

static EogWindow *
eog_application_get_uri_window (EogApplication *application, const char *uri)
{
	EogWindow *uri_window = NULL;
	GList *windows;
	GList *l;

	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (EOG_IS_APPLICATION (application), NULL);

	windows = gtk_window_list_toplevels ();

	for (l = windows; l != NULL; l = l->next) {
		if (EOG_IS_WINDOW (l->data)) {
			EogWindow *window = EOG_WINDOW (l->data);
			const char *window_uri = eog_window_get_uri (window);

			if (window_uri && strcmp (window_uri, uri) == 0 && 
			    !eog_window_is_empty (window)) {
				uri_window = window;
				break;
			}
		}
	}

	g_list_free (windows);
	
	return uri_window;
}

static GSList*
string_list_to_uri_list (GSList *string_list)
{
	GSList *it = NULL;
	GSList *uri_list = NULL;

	for (it = string_list; it != NULL; it = it->next) {
		char *uri_str;

		uri_str = (gchar *) it->data;
		
		uri_list = g_slist_prepend (uri_list, 
					    gnome_vfs_uri_new (uri_str));
	}

	return g_slist_reverse (uri_list);
}

static void
eog_application_show_window (EogWindow *window, gpointer user_data)
{
	gdk_threads_enter ();

	gtk_window_present_with_time (GTK_WINDOW (window),
				      (guint) user_data);

	gdk_threads_leave ();
}

gboolean
eog_application_open_uri_list (EogApplication  *application,
			       GSList          *files,
			       guint           timestamp,
			       GError         **error,
			       EogStartupFlags flags)
{
	EogWindow *new_window = NULL;
	GSList *uri_list = NULL;

	g_return_val_if_fail (EOG_IS_APPLICATION (application), FALSE);

	uri_list = string_list_to_uri_list (files);
	
	//new_window = eog_application_get_uri_window (application, (const char *) uri_list->data);

	if (new_window != NULL) {
		gtk_window_present_with_time (GTK_WINDOW (new_window),
					      timestamp);
		return TRUE;
	}

	new_window = eog_application_get_empty_window (application);

	if (new_window == NULL) {
		new_window = EOG_WINDOW (eog_window_new (flags));
	}

	g_signal_connect (new_window, 
			  "prepared", 
			  G_CALLBACK (eog_application_show_window), 
			  GINT_TO_POINTER (timestamp));

	eog_window_open_uri_list (new_window, uri_list);

	return TRUE;
}

#ifdef HAVE_LEAFTAG
gboolean
eog_application_open_tag_list (EogApplication  *application,
			       GSList          *tags,
			       guint           timestamp,
			       GError         **error,
			       EogStartupFlags flags)
{
	GSList *files = NULL;
	GList *sources, *l;
	GList *mimes = NULL;

	mimes = eog_image_get_supported_mime_types ();

	sources = lt_get_sources_with_tag_names ((GList *) tags, NULL, mimes);

	for (l = sources; l != NULL; l = l->next) {
		LtSource *source = LT_SOURCE (l->data);
		files = g_slist_prepend (files, g_strdup (lt_source_get_filename (source)));
	}

	eog_application_open_uri_list (EOG_APP, 
				       files,
				       GDK_CURRENT_TIME,
				       NULL,
				       flags);

	g_list_foreach(sources, (GFunc) g_object_unref, NULL);
	g_list_free (sources);

	g_slist_foreach (files, (GFunc) g_free, NULL);	
	g_slist_free (files);
	
	return TRUE;
}
#endif

void
eog_application_shutdown (EogApplication *application)
{
	g_return_if_fail (EOG_IS_APPLICATION (application));

	g_object_unref (application);
	
	gtk_main_quit ();
}

GList *
eog_application_get_windows (EogApplication *application)
{
	GList *l, *toplevels;
	GList *windows = NULL;

	g_return_val_if_fail (EOG_IS_APPLICATION (application), NULL);

	toplevels = gtk_window_list_toplevels ();

	for (l = toplevels; l != NULL; l = l->next) {
		if (EOG_IS_WINDOW (l->data)) {
			windows = g_list_append (windows, l->data);
		}
	}

	g_list_free (toplevels);

	return windows;
}

