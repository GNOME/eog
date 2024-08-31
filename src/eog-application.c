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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "eog-config-keys.h"
#include "eog-debug.h"
#include "eog-image.h"
#include "eog-job-scheduler.h"
#include "eog-session.h"
#include "eog-thumbnail.h"
#include "eog-window.h"
#include "eog-application.h"
#include "eog-application-activatable.h"
#include "eog-application-internal.h"
#include "eog-util.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <handy.h>

#ifdef HAVE_EXEMPI
#include <exempi/xmp.h>
#endif

#define is_rtl (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)

static void eog_application_load_accelerators (void);
static void eog_application_save_accelerators (void);

G_DEFINE_TYPE_WITH_PRIVATE (EogApplication, eog_application, GTK_TYPE_APPLICATION)

static void
action_about (GSimpleAction *action,
	      GVariant      *parameter,
	      gpointer       user_data)
{
	GtkApplication *application = GTK_APPLICATION (user_data);
	GtkWindow *window = gtk_application_get_active_window (application);

	g_return_if_fail (EOG_IS_WINDOW (window));

	eog_window_show_about_dialog (EOG_WINDOW (window));
}

static void
action_help (GSimpleAction *action,
	     GVariant      *parameter,
	     gpointer       user_data)
{
	GtkApplication *application = GTK_APPLICATION (user_data);
	GtkWindow *window = gtk_application_get_active_window (application);

	g_return_if_fail (window != NULL);

	eog_util_show_help (NULL, window);
}

static void
action_preferences (GSimpleAction *action,
	            GVariant      *parameter,
	            gpointer       user_data)
{
	GtkApplication *application = GTK_APPLICATION (user_data);
	GtkWindow *window = gtk_application_get_active_window (application);

	g_return_if_fail (EOG_IS_WINDOW (window));

	eog_window_show_preferences_dialog (EOG_WINDOW (window));
}

static void
action_toggle_state (GSimpleAction *action,
                     GVariant *parameter,
                     gpointer user_data)
{
	GVariant *state;
	gboolean new_state;

	state = g_action_get_state (G_ACTION (action));
	new_state = !g_variant_get_boolean (state);
	g_action_change_state (G_ACTION (action),
	                       g_variant_new_boolean (new_state));
	g_variant_unref (state);
}

static void
action_quit (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
	GList *windows;

	windows = gtk_application_get_windows (GTK_APPLICATION (user_data));

	g_list_foreach (windows, (GFunc) eog_window_close, NULL);
}

static GActionEntry app_entries[] = {
	{ "view-statusbar", action_toggle_state, NULL, "true", NULL },
	{ "view-gallery", action_toggle_state, NULL, "true",  NULL },
	{ "view-sidebar", action_toggle_state, NULL, "true",  NULL },
	{ "preferences", action_preferences, NULL, NULL, NULL },
	{ "about", action_about, NULL, NULL, NULL },
	{ "help", action_help, NULL, NULL, NULL },
	{ "quit", action_quit, NULL, NULL, NULL },
};

static gboolean
_settings_map_get_bool_variant (GValue   *value,
                               GVariant *variant,
                               gpointer  user_data)
{
	g_return_val_if_fail (g_variant_is_of_type (variant,
	                                            G_VARIANT_TYPE_BOOLEAN),
	                      FALSE);

	g_value_set_variant (value, variant);

	return TRUE;
}

static GVariant*
_settings_map_set_variant (const GValue       *value,
                           const GVariantType *expected_type,
                           gpointer            user_data)
{
	g_return_val_if_fail (g_variant_is_of_type (g_value_get_variant (value), expected_type), NULL);

	return g_value_dup_variant (value);
}

static void
eog_application_init_app_menu (EogApplication *application)
{
	EogApplicationPrivate *priv = application->priv;
	GAction *action;

	g_action_map_add_action_entries (G_ACTION_MAP (application),
					 app_entries, G_N_ELEMENTS (app_entries),
					 application);

	action = g_action_map_lookup_action (G_ACTION_MAP (application),
	                                     "view-gallery");
	g_settings_bind_with_mapping (priv->ui_settings,
	                              EOG_CONF_UI_IMAGE_GALLERY, action,
	                              "state", G_SETTINGS_BIND_DEFAULT,
	                              _settings_map_get_bool_variant,
	                              _settings_map_set_variant,
	                              NULL, NULL);

	action = g_action_map_lookup_action (G_ACTION_MAP (application),
	                                     "view-sidebar");
	g_settings_bind_with_mapping (priv->ui_settings,
	                              EOG_CONF_UI_SIDEBAR, action, "state",
                                      G_SETTINGS_BIND_DEFAULT,
	                              _settings_map_get_bool_variant,
	                              _settings_map_set_variant,
	                              NULL, NULL);
	action = g_action_map_lookup_action (G_ACTION_MAP (application),
	                                     "view-statusbar");
	g_settings_bind_with_mapping (priv->ui_settings,
	                              EOG_CONF_UI_STATUSBAR, action, "state",
                                      G_SETTINGS_BIND_DEFAULT,
	                              _settings_map_get_bool_variant,
	                              _settings_map_set_variant,
	                              NULL, NULL);
}

static void
eog_application_init_accelerators (GtkApplication *application)
{
	/* Based on a similar construct in Evince (src/ev-application.c).
	 * Setting multiple accelerators at once for an action
	 * is not very straight forward in a static way.
	 *
	 * This gchar* array simulates an action<->accels mapping.
	 * Enter the action name followed by the accelerator strings
	 * and terminate the entry with a NULL-string. */
	static const gchar * const accelmap[] = {
		"win.open",		"<Ctrl>o", NULL ,
		"win.save",		"<Ctrl>s", NULL ,
		"win.save-as",		"<Ctrl><shift>s", NULL,
		"win.close",		"<Ctrl>w", NULL,
		"win.close-all",	"<Ctrl>q", NULL,
		"win.print",		"<Ctrl>p", NULL,
		"win.properties",	"<Alt>Return", NULL,
		"win.show-remote",	"<Ctrl><Shift>p", NULL,
		"win.set-wallpaper",	"<Ctrl>F8", NULL,
		"win.manual",		"F1", NULL,
		"win.preferences",	"<Ctrl>comma", NULL,

		"win.go-first",		"<Alt>Home", "Home", NULL,
		"win.go-last",		"<Alt>End", "End", NULL,
		"win.go-random",	"<Ctrl>m", NULL,
		"win.rotate-90",	"<Ctrl>r", NULL,
		"win.rotate-270",	"<Ctrl><Shift>r", NULL,
		"win.move-trash",	"Delete", NULL,
		"win.delete",		"<Shift>Delete", NULL,
		"win.copy",		"<Ctrl>c", NULL,
		"win.undo",		"<Ctrl>z", NULL,
		"win.zoom-in",		"<Ctrl>equal", "<Ctrl>KP_Add",
					"<Ctrl>plus", NULL,
		"win.zoom-in-smooth",		"equal", "KP_Add",
					"plus", NULL,
		"win.zoom-out",		"<Ctrl>minus",
					"<Ctrl>KP_Subtract", NULL,
		"win.zoom-out-smooth",		"minus",
					"KP_Subtract", NULL,
		"win.zoom-normal",	"<Ctrl>0", "<Ctrl>KP_0",
					"1", "KP_1", NULL,

		"win.view-gallery",	"<Ctrl>F9", NULL,
		"win.view-sidebar",	"F9", NULL,
		"win.view-fullscreen",	"F11", NULL,
		"win.view-slideshow",	"F5", NULL,
		"win.toggle-zoom-fit",	"F", NULL,
		"win.toggle-gear-menu",	"F10", NULL,
		"win.pause-slideshow",	"p", NULL,

		NULL /* Terminating NULL */
	};

	const gchar * const *it = accelmap;

	for (it = accelmap; it[0]; it += g_strv_length ((gchar **)it) + 1) {
		gtk_application_set_accels_for_action (GTK_APPLICATION (application),
						       it[0], &it[1]);
	}

	static const gchar * const accels_left[] = {
		"Left", "<Alt>Left", NULL
	};
	static const gchar * const accels_right[] = {
		"Right", "<Alt>Right", NULL
	};

	if (is_rtl) {
		gtk_application_set_accels_for_action (GTK_APPLICATION (application),
					       "win.go-previous", accels_right);
		gtk_application_set_accels_for_action (GTK_APPLICATION (application),
					       "win.go-next", accels_left);
	} else {
		gtk_application_set_accels_for_action (GTK_APPLICATION (application),
					       "win.go-previous", accels_left);
		gtk_application_set_accels_for_action (GTK_APPLICATION (application),
					       "win.go-next", accels_right);
	}
}

static void
on_extension_added (PeasExtensionSet *set,
		    PeasPluginInfo   *info,
		    PeasExtension    *exten,
		    EogApplication   *app)
{
	eog_application_activatable_activate (EOG_APPLICATION_ACTIVATABLE (exten));
}

static void
on_extension_removed (PeasExtensionSet *set,
		      PeasPluginInfo   *info,
		      PeasExtension    *exten,
		      EogApplication   *app)
{
	eog_application_activatable_deactivate (EOG_APPLICATION_ACTIVATABLE (exten));
}

static void
eog_application_startup (GApplication *application)
{
	EogApplication *app = EOG_APPLICATION (application);
	GError *error = NULL;
	GFile *css_file;
	GtkCssProvider *provider;
  HdyStyleManager *style_manager;

	g_application_set_resource_base_path (application, "/org/gnome/eog");
	G_APPLICATION_CLASS (eog_application_parent_class)->startup (application);

  hdy_init ();
#ifdef HAVE_EXEMPI
	xmp_init();
#endif
	eog_job_scheduler_init ();
	eog_thumbnail_init ();

	/* Load special style properties for EogThumbView's scrollbar */
	css_file = g_file_new_for_uri ("resource:///org/gnome/eog/ui/eog.css");
	provider = gtk_css_provider_new ();
	if (G_UNLIKELY (!gtk_css_provider_load_from_file(provider,
							 css_file,
							 &error)))
	{
		g_critical ("Could not load CSS data: %s", error->message);
		g_clear_error (&error);
	} else {
		gtk_style_context_add_provider_for_screen (
				gdk_screen_get_default(),
				GTK_STYLE_PROVIDER (provider),
				GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
	g_object_unref (provider);
	g_object_unref (css_file);

	/* Add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                           EOG_DATA_DIR G_DIR_SEPARATOR_S "icons");

	g_set_prgname (APPLICATION_ID);
	gtk_window_set_default_icon_name (APPLICATION_ID);
	g_set_application_name (_("Eye of GNOME"));

	style_manager = hdy_style_manager_get_default ();
	hdy_style_manager_set_color_scheme (style_manager,
	                                    HDY_COLOR_SCHEME_PREFER_DARK);

	eog_application_init_app_menu (app);
	eog_application_init_accelerators (GTK_APPLICATION (app));

	app->priv->extensions = peas_extension_set_new (
				   PEAS_ENGINE (app->priv->plugin_engine),
				   EOG_TYPE_APPLICATION_ACTIVATABLE,
				   "app", app, NULL);
	g_signal_connect (app->priv->extensions, "extension-added",
			  G_CALLBACK (on_extension_added), app);
	g_signal_connect (app->priv->extensions, "extension-removed",
			  G_CALLBACK (on_extension_removed), app);

	peas_extension_set_call (app->priv->extensions, "activate");
}

static void
eog_application_shutdown (GApplication *application)
{
#ifdef HAVE_EXEMPI
	xmp_terminate();
#endif

	G_APPLICATION_CLASS (eog_application_parent_class)->shutdown (application);
}

static void
eog_application_activate (GApplication *application)
{
	eog_application_open_window (EOG_APPLICATION (application),
				     GDK_CURRENT_TIME,
				     EOG_APPLICATION (application)->priv->flags,
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
					EOG_APPLICATION (application)->priv->flags,
					NULL);
}

static void
eog_application_finalize (GObject *object)
{
	EogApplication *application = EOG_APPLICATION (object);
	EogApplicationPrivate *priv = application->priv;

	g_clear_object (&priv->extensions);

	if (priv->plugin_engine) {
		g_object_unref (priv->plugin_engine);
		priv->plugin_engine = NULL;
	}

	g_clear_object (&priv->ui_settings);

	eog_application_save_accelerators ();
}

static void
eog_application_add_platform_data (GApplication *application,
				   GVariantBuilder *builder)
{
	EogApplication *app = EOG_APPLICATION (application);

	G_APPLICATION_CLASS (eog_application_parent_class)->add_platform_data (application,
									       builder);

	if (app->priv->flags) {
		g_variant_builder_add (builder, "{sv}",
				       "eog-application-startup-flags",
		                       g_variant_new_byte (app->priv->flags));
	}
}

static void
eog_application_before_emit (GApplication *application,
			     GVariant *platform_data)
{
	GVariantIter iter;
	const gchar *key;
	GVariant *value;

	EOG_APPLICATION (application)->priv->flags = 0;
	g_variant_iter_init (&iter, platform_data);
	while (g_variant_iter_loop (&iter, "{&sv}", &key, &value)) {
		if (strcmp (key, "eog-application-startup-flags") == 0) {
			EOG_APPLICATION (application)->priv->flags = g_variant_get_byte (value);
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

	application_class->startup = eog_application_startup;
	application_class->shutdown = eog_application_shutdown;
	application_class->activate = eog_application_activate;
	application_class->open = eog_application_open;
	application_class->add_platform_data = eog_application_add_platform_data;
	application_class->before_emit = eog_application_before_emit;
}

static void
eog_application_init (EogApplication *eog_application)
{
	EogApplicationPrivate *priv;

	eog_debug_init ();
	eog_session_init (eog_application);

	eog_application->priv = eog_application_get_instance_private (eog_application);
	priv = eog_application->priv;

	priv->plugin_engine = eog_plugin_engine_new ();
	priv->flags = 0;

	priv->ui_settings = g_settings_new (EOG_CONF_UI);

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
							  "application-id", APPLICATION_ID,
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
		if (eog_window_is_empty (window) &&
		    eog_window_is_not_initializing (window)) {
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
		/* Filter out fullscreen flags to avoid going fullscreen
		 * with functions to leave fullscreen possibly being
		 * disabled due to the empty model */
		flags &= ~(EOG_STARTUP_FULLSCREEN | EOG_STARTUP_SLIDE_SHOW);
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
			EogImage *image = eog_window_get_image (window);

			if (image) {
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

gboolean
eog_application_close_all_windows (EogApplication *application)
{
	GList *windows;

	windows = gtk_application_get_windows (GTK_APPLICATION (application));

	g_list_foreach (windows, (GFunc) eog_window_close, NULL);

	return TRUE;
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
