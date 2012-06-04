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
#include "eog-util.h"

#include "totem-scrsaver.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#define APPLICATION_SERVICE_NAME "org.gnome.eog.ApplicationService"

static void eog_application_load_accelerators (void);
static void eog_application_save_accelerators (void);

#define EOG_APPLICATION_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EOG_TYPE_APPLICATION, EogApplicationPrivate))

G_DEFINE_TYPE (EogApplication, eog_application, GTK_TYPE_APPLICATION);

static void
eog_application_activate (GApplication *application)
{
	eog_application_open_window (EOG_APPLICATION (application),
				     GDK_CURRENT_TIME,
				     EOG_APPLICATION (application)->flags,
				     NULL);
}

static void
eog_application_open (GApplication *application,
		      GFile       **files,
		      gint          n_files,
		      const gchar  *hint)
{
	GSList *list = NULL;

	while (n_files--)
		list = g_slist_prepend (list, files[n_files]);

	eog_application_open_file_list (EOG_APPLICATION (application),
					list, GDK_CURRENT_TIME,
					EOG_APPLICATION (application)->flags,
					NULL);
}

static void
eog_application_finalize (GObject *object)
{
	EogApplication *application = EOG_APPLICATION (object);

	if (application->toolbars_model) {
		g_object_unref (application->toolbars_model);
		application->toolbars_model = NULL;
		g_free (application->toolbars_file);
		application->toolbars_file = NULL;
	}
	if (application->plugin_engine) {
		g_object_unref (application->plugin_engine);
		application->plugin_engine = NULL;
	}
	eog_application_save_accelerators ();
}

static void
eog_application_add_platform_data (GApplication *application,
				   GVariantBuilder *builder)
{
	EogApplication *app = EOG_APPLICATION (application);

	G_APPLICATION_CLASS (eog_application_parent_class)->add_platform_data (application,
									       builder);

	if (app->flags) {
		g_variant_builder_add (builder, "{sv}",
				       "eog-application-startup-flags",
				       g_variant_new_byte (app->flags));
	}
}

static void
eog_application_before_emit (GApplication *application,
			     GVariant *platform_data)
{
	GVariantIter iter;
	const gchar *key;
	GVariant *value;

	EOG_APPLICATION (application)->flags = 0;
	g_variant_iter_init (&iter, platform_data);
	while (g_variant_iter_loop (&iter, "{&sv}", &key, &value)) {
		if (strcmp (key, "eog-application-startup-flags") == 0) {
			EOG_APPLICATION (application)->flags = g_variant_get_byte (value);
		}
	}

	G_APPLICATION_CLASS (eog_application_parent_class)->before_emit (application,
									 platform_data);
}

static void
eog_application_class_init (EogApplicationClass *eog_application_class)
{
	GApplicationClass *application_class;
	GObjectClass *object_class;

	application_class = (GApplicationClass *) eog_application_class;
	object_class = (GObjectClass *) eog_application_class;

	object_class->finalize = eog_application_finalize;

	application_class->activate = eog_application_activate;
	application_class->open = eog_application_open;
	application_class->add_platform_data = eog_application_add_platform_data;
	application_class->before_emit = eog_application_before_emit;
}

static void
eog_application_init (EogApplication *eog_application)
{
	const gchar *dot_dir = eog_util_dot_dir ();

	eog_session_init (eog_application);

	eog_application->toolbars_model = egg_toolbars_model_new ();
	eog_application->plugin_engine = eog_plugin_engine_new ();
	eog_application->flags = 0;

	egg_toolbars_model_load_names (eog_application->toolbars_model,
				       EOG_DATA_DIR "/eog-toolbar.xml");

	if (G_LIKELY (dot_dir != NULL))
		eog_application->toolbars_file = g_build_filename
			(dot_dir, "eog_toolbar.xml", NULL);

	if (!dot_dir || !egg_toolbars_model_load_toolbars (eog_application->toolbars_model,
					       eog_application->toolbars_file)) {

		egg_toolbars_model_load_toolbars (eog_application->toolbars_model,
						  EOG_DATA_DIR "/eog-toolbar.xml");
	}

	egg_toolbars_model_set_flags (eog_application->toolbars_model, 0,
				      EGG_TB_MODEL_NOT_REMOVABLE);

	eog_application_load_accelerators ();
}

/**
 * eog_application_get_instance:
 *
 * Returns a singleton instance of #EogApplication currently running.
 * If not running yet, it will create one.
 *
 * Returns: (transfer none): a running #EogApplication.
 **/
EogApplication *
eog_application_get_instance (void)
{
	static EogApplication *instance;

	if (!instance) {
		instance = EOG_APPLICATION (g_object_new (EOG_TYPE_APPLICATION,
							  "application-id", APPLICATION_SERVICE_NAME,
							  "flags", G_APPLICATION_HANDLES_OPEN,
							  NULL));
	}

	return instance;
}

static EogWindow *
eog_application_get_empty_window (EogApplication *application)
{
	EogWindow *empty_window = NULL;
	GList *windows;
	GList *l;

	g_return_val_if_fail (EOG_IS_APPLICATION (application), NULL);

	windows = gtk_application_get_windows (GTK_APPLICATION (application));

	for (l = windows; l != NULL; l = l->next) {
		EogWindow *window = EOG_WINDOW (l->data);

		/* Make sure the window is empty and not initializing */
		if (eog_window_is_empty (window) && eog_window_was_initialized (window)) {
			empty_window = window;
			break;
		}
	}

	return empty_window;
}

/**
 * eog_application_open_window:
 * @application: An #EogApplication.
 * @timestamp: The timestamp of the user interaction which triggered this call
 * (see gtk_window_present_with_time()).
 * @flags: A set of #EogStartupFlags influencing a new windows' state.
 * @error: Return location for a #GError, or NULL to ignore errors.
 *
 * Opens and presents an empty #EogWindow to the user. If there is
 * an empty window already open, this will be used. Otherwise, a
 * new one will be instantiated.
 *
 * Returns: %FALSE if @application is invalid, %TRUE otherwise
 **/
gboolean
eog_application_open_window (EogApplication  *application,
			     guint32         timestamp,
			     EogStartupFlags flags,
			     GError        **error)
{
	GtkWidget *new_window = NULL;

	new_window = GTK_WIDGET (eog_application_get_empty_window (application));

	if (new_window == NULL) {
		new_window = eog_window_new (flags);
	}

	g_return_val_if_fail (EOG_IS_APPLICATION (application), FALSE);

	gtk_window_present_with_time (GTK_WINDOW (new_window),
				      timestamp);

	return TRUE;
}

static EogWindow *
eog_application_get_file_window (EogApplication *application, GFile *file)
{
	EogWindow *file_window = NULL;
	GList *windows;
	GList *l;

	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (EOG_IS_APPLICATION (application), NULL);

	windows = gtk_window_list_toplevels ();

	for (l = windows; l != NULL; l = l->next) {
		if (EOG_IS_WINDOW (l->data)) {
			EogWindow *window = EOG_WINDOW (l->data);

			if (!eog_window_is_empty (window)) {
				EogImage *image = eog_window_get_image (window);
				GFile *window_file;

				window_file = eog_image_get_file (image);
				if (g_file_equal (window_file, file)) {
					file_window = window;
					break;
				}
			}
		}
	}

	g_list_free (windows);

	return file_window;
}

static EogWindow *
eog_application_get_first_window (EogApplication *application)
{
	g_return_val_if_fail (EOG_IS_APPLICATION (application), NULL);

	GList *windows;
	GList *l;
	EogWindow *window = NULL;
	windows = gtk_window_list_toplevels ();
	for (l = windows; l != NULL; l = l->next) {
		if (EOG_IS_WINDOW (l->data)) {
			window = EOG_WINDOW (l->data);
			break;
		}
	}
	g_list_free (windows);

	return window;
}


static void
eog_application_show_window (EogWindow *window, gpointer user_data)
{
	gtk_window_present_with_time (GTK_WINDOW (window),
				      GPOINTER_TO_UINT (user_data));
}

/**
 * eog_application_open_file_list:
 * @application: An #EogApplication.
 * @file_list: (element-type GFile): A list of #GFile<!-- -->s.
 * @timestamp: The timestamp of the user interaction which triggered this call
 * (see gtk_window_present_with_time()).
 * @flags: A set of #EogStartupFlags influencing a new windows' state.
 * @error: Return location for a #GError, or NULL to ignore errors.
 *
 * Opens a list of files in a #EogWindow. If an #EogWindow displaying the first
 * image in the list is already open, this will be used. Otherwise, an empty
 * #EogWindow is used, either already existing or newly created.
 * If the EOG_STARTUP_SINGLE_WINDOW flag is set, the files are opened in the
 * first #EogWindow and no new one is opened.
 *
 * Returns: Currently always %TRUE.
 **/
gboolean
eog_application_open_file_list (EogApplication  *application,
				GSList          *file_list,
				guint           timestamp,
				EogStartupFlags flags,
				GError         **error)
{
	EogWindow *new_window = NULL;

	if (file_list != NULL) {
		if(flags & EOG_STARTUP_SINGLE_WINDOW)
			new_window = eog_application_get_first_window (application);
		else
			new_window = eog_application_get_file_window (application,
								      (GFile *) file_list->data);
	}

	if (new_window != NULL) {
		if(flags & EOG_STARTUP_SINGLE_WINDOW)
		        eog_window_open_file_list (new_window, file_list);
		else
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
			  GUINT_TO_POINTER (timestamp));

	eog_window_open_file_list (new_window, file_list);

	return TRUE;
}

/**
 * eog_application_open_uri_list:
 * @application: An #EogApplication.
 * @uri_list: (element-type utf8): A list of URIs.
 * @timestamp: The timestamp of the user interaction which triggered this call
 * (see gtk_window_present_with_time()).
 * @flags: A set of #EogStartupFlags influencing a new windows' state.
 * @error: Return location for a #GError, or NULL to ignore errors.
 *
 * Opens a list of images, from a list of URIs. See
 * eog_application_open_file_list() for details.
 *
 * Returns: Currently always %TRUE.
 **/
gboolean
eog_application_open_uri_list (EogApplication  *application,
 			       GSList          *uri_list,
 			       guint           timestamp,
 			       EogStartupFlags flags,
 			       GError         **error)
{
 	GSList *file_list = NULL;

 	g_return_val_if_fail (EOG_IS_APPLICATION (application), FALSE);

 	file_list = eog_util_string_list_to_file_list (uri_list);

 	return eog_application_open_file_list (application,
					       file_list,
					       timestamp,
					       flags,
					       error);
}

/**
 * eog_application_open_uris:
 * @application: an #EogApplication
 * @uris:  A #GList of URI strings.
 * @timestamp: The timestamp of the user interaction which triggered this call
 * (see gtk_window_present_with_time()).
 * @flags: A set of #EogStartupFlags influencing a new windows' state.
 * @error: Return location for a #GError, or NULL to ignore errors.
 *
 * Opens a list of images, from a list of URI strings. See
 * eog_application_open_file_list() for details.
 *
 * Returns: Currently always %TRUE.
 **/
gboolean
eog_application_open_uris (EogApplication  *application,
 			   gchar          **uris,
 			   guint           timestamp,
 			   EogStartupFlags flags,
 			   GError        **error)
{
 	GSList *file_list = NULL;

 	file_list = eog_util_strings_to_file_list (uris);

 	return eog_application_open_file_list (application, file_list, timestamp,
						    flags, error);
}


/**
 * eog_application_get_toolbars_model:
 * @application: An #EogApplication.
 *
 * Retrieves the #EggToolbarsModel for the toolbar in #EogApplication.
 *
 * Returns: (transfer none): An #EggToolbarsModel.
 **/
EggToolbarsModel *
eog_application_get_toolbars_model (EogApplication *application)
{
	g_return_val_if_fail (EOG_IS_APPLICATION (application), NULL);

	return application->toolbars_model;
}

/**
 * eog_application_save_toolbars_model:
 * @application: An #EogApplication.
 *
 * Causes the saving of the model of the toolbar in #EogApplication to a file.
 **/
void
eog_application_save_toolbars_model (EogApplication *application)
{
	if (G_LIKELY(application->toolbars_file != NULL))
        	egg_toolbars_model_save_toolbars (application->toolbars_model,
				 	          application->toolbars_file,
						  "1.0");
}

/**
 * eog_application_reset_toolbars_model:
 * @app: an #EogApplication
 *
 * Restores the toolbars model to the defaults.
 **/
void
eog_application_reset_toolbars_model (EogApplication *app)
{
	g_return_if_fail (EOG_IS_APPLICATION (app));

	g_object_unref (app->toolbars_model);

	app->toolbars_model = egg_toolbars_model_new ();

	egg_toolbars_model_load_names (app->toolbars_model,
				       EOG_DATA_DIR "/eog-toolbar.xml");
	egg_toolbars_model_load_toolbars (app->toolbars_model,
					  EOG_DATA_DIR "/eog-toolbar.xml");
	egg_toolbars_model_set_flags (app->toolbars_model, 0,
				      EGG_TB_MODEL_NOT_REMOVABLE);
}

/**
 * eog_application_screensaver_enable:
 * @application: an #EogApplication.
 *
 * Enables the screensaver. Usually necessary after a call to
 * eog_application_screensaver_disable().
 **/
void
eog_application_screensaver_enable (EogApplication *application)
{
        if (application->scr_saver)
                totem_scrsaver_enable (application->scr_saver);
}

/**
 * eog_application_screensaver_disable:
 * @application: an #EogApplication.
 *
 * Disables the screensaver. Useful when the application is in fullscreen or
 * similar mode.
 **/
void
eog_application_screensaver_disable (EogApplication *application)
{
        if (application->scr_saver)
                totem_scrsaver_disable (application->scr_saver);
}

static void
eog_application_load_accelerators (void)
{
	gchar *accelfile = g_build_filename (eog_util_dot_dir (), "accels", NULL);

	/* gtk_accel_map_load does nothing if the file does not exist */
	gtk_accel_map_load (accelfile);

	g_free (accelfile);
}

static void
eog_application_save_accelerators (void)
{
	/* save to XDG_CONFIG_HOME/eog/accels */
	gchar *accelfile = g_build_filename (eog_util_dot_dir (), "accels", NULL);

	gtk_accel_map_save (accelfile);
	g_free (accelfile);
}
