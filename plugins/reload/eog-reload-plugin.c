/* Reload Image -- Allows to reload an image from disk
 *
 * Copyright (C) 2007-2012 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
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
#include <config.h>
#endif

#include "eog-reload-plugin.h"

#include <gmodule.h>
#include <glib/gi18n-lib.h>

#include <libpeas/peas.h>

#include <eog-application.h>
#include <eog-debug.h>
#include <eog-thumb-view.h>
#include <eog-window.h>
#include <eog-window-activatable.h>

enum {
  PROP_0,
  PROP_WINDOW
};

#define EOG_RELOAD_PLUGIN_MENU_ID "EogPluginRunReload"
#define EOG_RELOAD_PLUGIN_ACTION "reload"

static void eog_window_activatable_iface_init (EogWindowActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EogReloadPlugin,
                                eog_reload_plugin,
                                PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (EOG_TYPE_WINDOW_ACTIVATABLE,
                                                               eog_window_activatable_iface_init))

static void
reload_cb (GSimpleAction *simple,
	   GVariant      *parameter,
	   gpointer       user_data)
{
	eog_window_reload_image (EOG_WINDOW (user_data));
}

static void
eog_reload_plugin_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	EogReloadPlugin *plugin = EOG_RELOAD_PLUGIN (object);

	switch (prop_id)
	{
	case PROP_WINDOW:
		plugin->window = EOG_WINDOW (g_value_dup_object (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
eog_reload_plugin_get_property (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
	EogReloadPlugin *plugin = EOG_RELOAD_PLUGIN (object);

	switch (prop_id)
	{
	case PROP_WINDOW:
		g_value_set_object (value, plugin->window);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
eog_reload_plugin_init (EogReloadPlugin *plugin)
{
	eog_debug_message (DEBUG_PLUGINS, "EogReloadPlugin initializing");
}

static void
eog_reload_plugin_dispose (GObject *object)
{
	EogReloadPlugin *plugin = EOG_RELOAD_PLUGIN (object);

	eog_debug_message (DEBUG_PLUGINS, "EogReloadPlugin disposing");

	if (plugin->window != NULL) {
		g_object_unref (plugin->window);
		plugin->window = NULL;
	}

	G_OBJECT_CLASS (eog_reload_plugin_parent_class)->dispose (object);
}

static void
eog_reload_plugin_update_action_state (EogReloadPlugin *plugin)
{
	GAction *action;
	EogThumbView *thumbview;
	gboolean enable = FALSE;

	thumbview = EOG_THUMB_VIEW (eog_window_get_thumb_view (plugin->window));

	if (G_LIKELY (thumbview))
	{
		enable = (eog_thumb_view_get_n_selected (thumbview) != 0);
	}

	action = g_action_map_lookup_action (G_ACTION_MAP (plugin->window),
					     EOG_RELOAD_PLUGIN_ACTION);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enable);
}

static void
_selection_changed_cb (EogThumbView *thumbview, gpointer user_data)
{
	EogReloadPlugin *plugin = EOG_RELOAD_PLUGIN (user_data);

	if (G_LIKELY (plugin))
		eog_reload_plugin_update_action_state (plugin);
}

static void
eog_reload_plugin_activate (EogWindowActivatable *activatable)
{
	const gchar * const accel_keys[] = { "R", NULL };
	EogReloadPlugin *plugin = EOG_RELOAD_PLUGIN (activatable);
	GMenu *model, *menu;
	GMenuItem *item;
	GSimpleAction *action;

	eog_debug (DEBUG_PLUGINS);

	model= eog_window_get_gear_menu_section (plugin->window,
						 "plugins-section");

	g_return_if_fail (G_IS_MENU (model));

	/* Setup and inject action */
	action = g_simple_action_new (EOG_RELOAD_PLUGIN_ACTION, NULL);
	g_signal_connect(action, "activate",
			 G_CALLBACK (reload_cb), plugin->window);
	g_action_map_add_action (G_ACTION_MAP (plugin->window),
				 G_ACTION (action));
	g_object_unref (action);

	g_signal_connect (G_OBJECT (eog_window_get_thumb_view (plugin->window)),
			  "selection-changed",
			  G_CALLBACK (_selection_changed_cb),
			  plugin);
	eog_reload_plugin_update_action_state (plugin);

	/* Append entry to the window's gear menu */
	menu = g_menu_new ();
	g_menu_append (menu, _("Reload Image"),
		       "win." EOG_RELOAD_PLUGIN_ACTION);

	item = g_menu_item_new_section (NULL, G_MENU_MODEL (menu));
	g_menu_item_set_attribute (item, "id",
				   "s", EOG_RELOAD_PLUGIN_MENU_ID);
	g_menu_item_set_attribute (item, G_MENU_ATTRIBUTE_ICON,
				   "s", "view-refresh-symbolic");
	g_menu_append_item (model, item);
	g_object_unref (item);

	g_object_unref (menu);

	/* Define accelerator keys */
	gtk_application_set_accels_for_action (GTK_APPLICATION (EOG_APP),
					       "win." EOG_RELOAD_PLUGIN_ACTION,
					       accel_keys);
}

static void
eog_reload_plugin_deactivate (EogWindowActivatable *activatable)
{
	const gchar * const empty_accels[1] = { NULL };
	EogReloadPlugin *plugin = EOG_RELOAD_PLUGIN (activatable);
	GMenu *menu;
	GMenuModel *model;
	gint i;

	eog_debug (DEBUG_PLUGINS);

	menu = eog_window_get_gear_menu_section (plugin->window,
						 "plugins-section");

	g_return_if_fail (G_IS_MENU (menu));

	/* Remove menu entry */
	model = G_MENU_MODEL (menu);
	for (i = 0; i < g_menu_model_get_n_items (model); i++) {
		gchar *id;
		if (g_menu_model_get_item_attribute (model, i, "id", "s", &id)) {
			const gboolean found =
				(g_strcmp0 (id, EOG_RELOAD_PLUGIN_MENU_ID) == 0);
			g_free (id);

			if (found) {
				g_menu_remove (menu, i);
				break;
			}
		}
	}

	/* Unset accelerator */
	gtk_application_set_accels_for_action(GTK_APPLICATION (EOG_APP),
					      "win." EOG_RELOAD_PLUGIN_ACTION,
					      empty_accels);

	/* Disconnect selection-changed handler as the thumbview would
	 * otherwise still cause callbacks during its own disposal */
	g_signal_handlers_disconnect_by_func (eog_window_get_thumb_view (plugin->window),
					      _selection_changed_cb, plugin);

	/* Finally remove action */
	g_action_map_remove_action (G_ACTION_MAP (plugin->window),
				    EOG_RELOAD_PLUGIN_ACTION);
}

static void
eog_reload_plugin_class_init (EogReloadPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose= eog_reload_plugin_dispose;
	object_class->set_property = eog_reload_plugin_set_property;
	object_class->get_property = eog_reload_plugin_get_property;

	g_object_class_override_property (object_class, PROP_WINDOW, "window");
}

static void
eog_reload_plugin_class_finalize (EogReloadPluginClass *klass)
{
}

static void
eog_window_activatable_iface_init (EogWindowActivatableInterface *iface)
{
	iface->activate = eog_reload_plugin_activate;
	iface->deactivate = eog_reload_plugin_deactivate;
}

G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
	eog_reload_plugin_register_type (G_TYPE_MODULE (module));
	peas_object_module_register_extension_type (module,
						    EOG_TYPE_WINDOW_ACTIVATABLE,
						    EOG_TYPE_RELOAD_PLUGIN);
}
