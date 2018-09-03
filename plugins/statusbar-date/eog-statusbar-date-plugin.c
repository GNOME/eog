/* Statusbar Date -- Shows the EXIF date in EOG's statusbar
 *
 * Copyright (C) 2008-2010 The Free Software Foundation
 *
 * Author: Claudio Saavedra  <csaavedra@gnome.org>
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

#include "eog-statusbar-date-plugin.h"

#include <gmodule.h>
#include <glib/gi18n-lib.h>

#include <libpeas/peas.h>

#include <eog-debug.h>
#include <eog-scroll-view.h>
#include <eog-image.h>
#include <eog-exif-util.h>
#include <eog-window.h>
#include <eog-window-activatable.h>

static void eog_window_activatable_iface_init (EogWindowActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EogStatusbarDatePlugin,
		eog_statusbar_date_plugin,
		PEAS_TYPE_EXTENSION_BASE,
		0,
		G_IMPLEMENT_INTERFACE_DYNAMIC (EOG_TYPE_WINDOW_ACTIVATABLE,
					       eog_window_activatable_iface_init))

enum {
	PROP_0,
	PROP_WINDOW
};

static void
statusbar_set_date (GtkLabel *label, EogScrollView *view)
{
	EogImage *image;
	ExifData *exif_data = NULL;

	image = eog_scroll_view_get_image (view);

	if (image && eog_image_has_data (image, EOG_IMAGE_DATA_EXIF))
		exif_data = eog_image_get_exif_info (image);

	if (exif_data) {
		/* A strftime-formatted string, to display the date the image was taken.  */
		eog_exif_util_format_datetime_label (label, exif_data,
		                                     EXIF_TAG_DATE_TIME_ORIGINAL,
		                                     _("%a, %d %B %Y  %X"));
		eog_exif_data_free (exif_data);
	} else {
		gtk_label_set_text (label, NULL);
	}

	if (image)
		g_object_unref (image);
}

static void
image_changed_cb (EogScrollView *view,
		  GParamSpec *spec,
		  EogStatusbarDatePlugin *plugin)
{
	g_return_if_fail (EOG_IS_STATUSBAR_DATE_PLUGIN (plugin));

	statusbar_set_date (GTK_LABEL (plugin->statusbar_date), view);
}

static void
eog_statusbar_date_plugin_set_property (GObject      *object,
					guint         prop_id,
					const GValue *value,
					GParamSpec   *pspec)
{
	EogStatusbarDatePlugin *plugin = EOG_STATUSBAR_DATE_PLUGIN (object);

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
eog_statusbar_date_plugin_get_property (GObject    *object,
					guint       prop_id,
					GValue     *value,
					GParamSpec *pspec)
{
	EogStatusbarDatePlugin *plugin = EOG_STATUSBAR_DATE_PLUGIN (object);

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
eog_statusbar_date_plugin_init (EogStatusbarDatePlugin *plugin)
{
	eog_debug_message (DEBUG_PLUGINS, "EogStatusbarDatePlugin initializing");
}

static void
eog_statusbar_date_plugin_dispose (GObject *object)
{
	EogStatusbarDatePlugin *plugin = EOG_STATUSBAR_DATE_PLUGIN (object);

	eog_debug_message (DEBUG_PLUGINS, "EogStatusbarDatePlugin disposing");

	if (plugin->window != NULL) {
		g_object_unref (plugin->window);
		plugin->window = NULL;
	}

	G_OBJECT_CLASS (eog_statusbar_date_plugin_parent_class)->dispose (object);
}

static void
eog_statusbar_date_plugin_activate (EogWindowActivatable *activatable)
{
	EogStatusbarDatePlugin *plugin = EOG_STATUSBAR_DATE_PLUGIN (activatable);
	EogWindow *window = plugin->window;
	GtkWidget *statusbar = eog_window_get_statusbar (window);
	GtkWidget *view = eog_window_get_view (window);

	eog_debug (DEBUG_PLUGINS);

	plugin->statusbar_date = gtk_label_new (NULL);
	gtk_widget_set_size_request (plugin->statusbar_date, 200, 10);
	gtk_box_pack_end (GTK_BOX (statusbar),
			  plugin->statusbar_date,
			  FALSE, FALSE, 0);

	plugin->signal_id = g_signal_connect (G_OBJECT (view), "notify::image",
					      G_CALLBACK (image_changed_cb),
					      plugin);

	statusbar_set_date (GTK_LABEL (plugin->statusbar_date),
			    EOG_SCROLL_VIEW (view));
	gtk_widget_show (plugin->statusbar_date);
}

static void
eog_statusbar_date_plugin_deactivate (EogWindowActivatable *activatable)
{
	EogStatusbarDatePlugin *plugin = EOG_STATUSBAR_DATE_PLUGIN (activatable);
	EogWindow *window = plugin->window;
	GtkWidget *statusbar = eog_window_get_statusbar (window);
	GtkWidget *view = eog_window_get_view (window);

	g_signal_handler_disconnect (view, plugin->signal_id);

	gtk_container_remove (GTK_CONTAINER (statusbar),
			      plugin->statusbar_date);
}

static void
eog_statusbar_date_plugin_class_init (EogStatusbarDatePluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = eog_statusbar_date_plugin_dispose;
	object_class->set_property = eog_statusbar_date_plugin_set_property;
	object_class->get_property = eog_statusbar_date_plugin_get_property;
	
	g_object_class_override_property (object_class, PROP_WINDOW, "window");
 }

static void
eog_statusbar_date_plugin_class_finalize (EogStatusbarDatePluginClass *klass)
{
}

static void
eog_window_activatable_iface_init (EogWindowActivatableInterface *iface)
{
	iface->activate = eog_statusbar_date_plugin_activate;
	iface->deactivate = eog_statusbar_date_plugin_deactivate;
}

G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
	eog_statusbar_date_plugin_register_type (G_TYPE_MODULE (module));
	peas_object_module_register_extension_type (module,
						    EOG_TYPE_WINDOW_ACTIVATABLE,
						    EOG_TYPE_STATUSBAR_DATE_PLUGIN);
}
