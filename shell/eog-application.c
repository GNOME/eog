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

#include "eog-session.h"
#include "eog-window.h"
#include "eog-application.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkmain.h>
#include <string.h>

#define EOG_RECENT_FILES_GROUP		"Eye of GNOME"

G_DEFINE_TYPE (EogApplication, eog_application, G_TYPE_OBJECT);

#define EOG_APPLICATION_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EOG_TYPE_APPLICATION, EogApplicationPrivate))

static void
eog_application_class_init (EogApplicationClass *eog_application_class)
{
}

static void
eog_application_init (EogApplication *eog_application)
{
	eog_session_init (eog_application);

	eog_application->recent_model = egg_recent_model_new (EGG_RECENT_MODEL_SORT_MRU);
        egg_recent_model_set_limit (eog_application->recent_model, 5);
        egg_recent_model_set_filter_groups (eog_application->recent_model, 
					    EOG_RECENT_FILES_GROUP, NULL);
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
			     GError         **error)
{
	//GtkWidget *new_window = eog_window_new (NULL);

	g_return_val_if_fail (EOG_IS_APPLICATION (application), FALSE);

	//gtk_widget_show (new_window);
	
#ifdef HAVE_GTK_WINDOW_PRESENT_WITH_TIME
	//gtk_window_present_with_time (GTK_WINDOW (new_window),
	//			      timestamp);
#else
	//gtk_window_present (GTK_WINDOW (new_window));
#endif

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

		if (!eog_window_has_contents (window)) {
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

			if (window_uri && strcmp (window_uri, uri) == 0 && eog_window_has_contents (window)) {
				uri_window = window;
				break;
			}
		}
	}

	g_list_free (windows);
	
	return uri_window;
}

gboolean
eog_application_open_uri_list (EogApplication  *application,
			       GSList          *uri_list,
			       guint           timestamp,
			       GError         **error)
{
	EogWindow *new_window = NULL;

	g_return_val_if_fail (EOG_IS_APPLICATION (application), FALSE);

	//new_window = eog_application_get_uri_window (application, (const char *) uri_list->data);
	if (new_window != NULL) {
#ifdef HAVE_GTK_WINDOW_PRESENT_WITH_TIME
		//gtk_window_present_with_time (GTK_WINDOW (new_window),
		//			      timestamp);
#else
		//gtk_window_present (GTK_WINDOW (new_window));
#endif	
		return TRUE;
	}

	//new_window = eog_application_get_empty_window (application);

	//if (new_window == NULL) {
	//	new_window = EOG_WINDOW (eog_window_new (NULL));
	//}

	eog_window_open_uri_list (new_window, uri_list, error);

	//gtk_widget_show (GTK_WIDGET (new_window));

#ifdef HAVE_GTK_WINDOW_PRESENT_WITH_TIME
	//gtk_window_present_with_time (GTK_WINDOW (new_window),
	//			      timestamp);
#else
	//gtk_window_present (GTK_WINDOW (new_window));
#endif

	return TRUE;
}

void
eog_application_shutdown (EogApplication *application)
{
	g_return_if_fail (EOG_IS_APPLICATION (application));

	if (application->recent_model) {
		g_object_unref (application->recent_model);
		application->recent_model = NULL;
	}

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

EggRecentModel *
eog_application_get_recent_model (EogApplication *application)
{
	g_return_val_if_fail (EOG_IS_APPLICATION (application), NULL);

	return application->recent_model;
}
