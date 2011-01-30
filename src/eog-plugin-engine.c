/* Eye Of Gnome - EOG Plugin Manager
 *
 * Copyright (C) 2007 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on gedit code (gedit/gedit-module.c) by:
 * 	- Paolo Maggi <paolo@gnome.org>
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
#include <config.h>
#endif

#include "eog-plugin-engine.h"
#include "eog-debug.h"
#include "eog-config-keys.h"
#include "eog-util.h"

#include <glib/gi18n.h>
#include <glib.h>
#include <gio/gio.h>
#include <girepository.h>

#define USER_EOG_PLUGINS_LOCATION "plugins/"

G_DEFINE_TYPE (EogPluginEngine, eog_plugin_engine, PEAS_TYPE_ENGINE)

struct _EogPluginEnginePrivate {
    GSettings *plugins_settings;
};

static void
eog_plugin_engine_dispose (GObject *object)
{
	EogPluginEngine *engine = EOG_PLUGIN_ENGINE (object);

	if (engine->priv->plugins_settings != NULL)
	{
		g_object_unref (engine->priv->plugins_settings);
		engine->priv->plugins_settings = NULL;
	}

	G_OBJECT_CLASS (eog_plugin_engine_parent_class)->dispose (object);
}

static void
eog_plugin_engine_class_init (EogPluginEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (EogPluginEnginePrivate));

  object_class->dispose = eog_plugin_engine_dispose;
}

static void
eog_plugin_engine_init (EogPluginEngine *engine)
{
	eog_debug (DEBUG_PLUGINS);

	engine->priv = G_TYPE_INSTANCE_GET_PRIVATE (engine,
						    EOG_TYPE_PLUGIN_ENGINE,
						    EogPluginEnginePrivate);

	engine->priv->plugins_settings = g_settings_new ("org.gnome.eog.plugins");
}

EogPluginEngine *
eog_plugin_engine_new (void)
{
	EogPluginEngine *engine;
	gchar *user_plugin_path;
	gchar *private_path;
	GError *error = NULL;

	private_path = g_build_filename (LIBDIR, "eog",
					"girepository-1.0", NULL);

	/* This should be moved to libpeas */
	if (g_irepository_require (g_irepository_get_default (),
				   "Peas", "1.0", 0, &error) == NULL)
	{
		g_warning ("Error loading Peas typelib: %s\n",
			   error->message);
		g_clear_error (&error);
	}


	if (g_irepository_require (g_irepository_get_default (),
				   "PeasGtk", "1.0", 0, &error) == NULL)
	{
		g_warning ("Error loading PeasGtk typelib: %s\n",
			   error->message);
		g_clear_error (&error);
	}


	if (g_irepository_require_private (g_irepository_get_default (),
					   private_path, "Eog", "3.0", 0,
					   &error) == NULL)
	{
		g_warning ("Error loading Eog typelib: %s\n",
			   error->message);
		g_clear_error (&error);
	}

	g_free (private_path);

	engine = EOG_PLUGIN_ENGINE (g_object_new (EOG_TYPE_PLUGIN_ENGINE,
						  NULL));

	/* Disable python and seed bindings as they are not working very
	 * well with eog yet (e.g. are having ref counting issues). */
	peas_engine_disable_loader (PEAS_ENGINE (engine), "python");
	peas_engine_disable_loader (PEAS_ENGINE (engine), "seed");

	user_plugin_path = g_build_filename (eog_util_dot_dir (),
					     USER_EOG_PLUGINS_LOCATION, NULL);
	/* Find per-user plugins */
	peas_engine_add_search_path (PEAS_ENGINE (engine),
				     user_plugin_path, user_plugin_path);
	/* Find system-wide plugins */
	peas_engine_add_search_path (PEAS_ENGINE (engine),
				     EOG_PLUGIN_DIR, EOG_PLUGIN_DIR);

	g_settings_bind (engine->priv->plugins_settings,
			 EOG_CONF_PLUGINS_ACTIVE_PLUGINS,
			 engine,
			 "loaded-plugins",
			 G_SETTINGS_BIND_DEFAULT);

	g_free (user_plugin_path);

	return engine;
}
