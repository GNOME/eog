/* Eye of Gnome image viewer - main window widget
 *
 * Copyright (C) 2000 The Free Software Foundation
 *               2000 SuSE GmbH
 *
 * Authors: Federico Mena-Quintero <federico@gnu.org>
 *          Martin Baulig <baulig@suse.de>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <gnome.h>
#include "gconf.h"

typedef struct _ImageViewGConfData ImageViewGConfData;

/* Private part of the ImageView structure */
struct _ImageViewGConfData {
	/* GConf client for monitoring changes to the preferences */
	GConfClient *client;

	/* GConf client notify IDs */
	guint interp_type_notify_id;
	guint check_type_notify_id;
	guint check_size_notify_id;
	guint dither_notify_id;
	guint scroll_notify_id;
};

/* Handler for changes on the interp_type value */
static void
interp_type_changed_cb (GConfClient *client, guint notify_id, GConfEntry *entry, gpointer data)
{
	image_view_set_interp_type (IMAGE_VIEW (data), gconf_value_get_int (entry->value));
}

/* Handler for changes on the check_type value */
static void
check_type_changed_cb (GConfClient *client, guint notify_id, GConfEntry *entry, gpointer data)
{
	image_view_set_check_type (IMAGE_VIEW (data), gconf_value_get_int (entry->value));
}

/* Handler for changes on the check_size value */
static void
check_size_changed_cb (GConfClient *client, guint notify_id, GConfEntry *entry, gpointer data)
{
	image_view_set_check_size (IMAGE_VIEW (data), gconf_value_get_int (entry->value));
}

/* Handler for changes on the dither value */
static void
dither_changed_cb (GConfClient *client, guint notify_id, GConfEntry *entry, gpointer data)
{
	image_view_set_dither (IMAGE_VIEW (data), gconf_value_get_int (entry->value));
}

/* Handler for changes on the scroll value */
static void
scroll_changed_cb (GConfClient *client, guint notify_id, GConfEntry *entry, gpointer data)
{
	image_view_set_scroll (IMAGE_VIEW (data), gconf_value_get_int (entry->value));
}

/* Destroy the GConf stuff */
static void
image_view_destroy_gconf_data (gpointer object)
{
	ImageViewGConfData *priv = object;

	g_return_if_fail (priv != NULL);
	g_return_if_fail (priv->client != NULL);
	g_return_if_fail (GCONF_IS_CLIENT (priv->client));

	/* Remove notification handlers */

	gconf_client_notify_remove (priv->client, priv->interp_type_notify_id);
	gconf_client_notify_remove (priv->client, priv->check_type_notify_id);
	gconf_client_notify_remove (priv->client, priv->check_size_notify_id);
	gconf_client_notify_remove (priv->client, priv->dither_notify_id);
	gconf_client_notify_remove (priv->client, priv->scroll_notify_id);

	priv->interp_type_notify_id = 0;
	priv->check_type_notify_id = 0;
	priv->check_size_notify_id = 0;
	priv->dither_notify_id = 0;
	priv->scroll_notify_id = 0;

	gconf_client_remove_dir (priv->client, "/apps/eog", NULL);

	gtk_object_unref (GTK_OBJECT (priv->client));

	g_free (priv);
}

void
image_view_add_gconf_client (ImageView *image_view, GConfClient *client)
{
	ImageViewGConfData *priv;

	g_return_if_fail (image_view != NULL);
	g_return_if_fail (IS_IMAGE_VIEW (image_view));
	g_return_if_fail (client != NULL);
	g_return_if_fail (GCONF_IS_CLIENT (client));

	g_assert (gtk_object_get_data (GTK_OBJECT (image_view),
				       "image_view:gconf_data") == NULL);

	priv = g_new0 (ImageViewGConfData, 1);

	/* Add the GConf client and notification handlers */

	gtk_object_ref (GTK_OBJECT (client));
	priv->client = client;

	gconf_client_add_dir (priv->client, "/apps/eog",
			      GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);

	priv->interp_type_notify_id = gconf_client_notify_add (
		priv->client, "/apps/eog/view/interp_type",
		interp_type_changed_cb, image_view,
		NULL, NULL);
	priv->check_type_notify_id = gconf_client_notify_add (
		priv->client, "/apps/eog/view/check_type",
		check_type_changed_cb, image_view,
		NULL, NULL);
	priv->check_size_notify_id = gconf_client_notify_add (
		priv->client, "/apps/eog/view/check_size",
		check_size_changed_cb, image_view,
		NULL, NULL);
	priv->dither_notify_id = gconf_client_notify_add (
		priv->client, "/apps/eog/view/dither",
		dither_changed_cb, image_view,
		NULL, NULL);
	priv->scroll_notify_id = gconf_client_notify_add (
		priv->client, "/apps/eog/view/scroll",
		scroll_changed_cb, image_view,
		NULL, NULL);

	/* Set the default values */

	image_view_set_interp_type (image_view,
		gconf_client_get_int (priv->client, "/apps/eog/view/interp_type", NULL));
	image_view_set_check_type (image_view,
		gconf_client_get_int (priv->client, "/apps/eog/view/check_type", NULL));
	image_view_set_check_size (image_view,
		gconf_client_get_int (priv->client, "/apps/eog/view/check_size", NULL));
	image_view_set_dither (image_view,
		gconf_client_get_int (priv->client, "/apps/eog/view/dither", NULL));
	image_view_set_scroll (image_view,
		gconf_client_get_int (priv->client, "/apps/eog/view/scroll", NULL));
	image_view_set_full_screen_zoom (image_view,
		gconf_client_get_int (priv->client, "/apps/eog/full_screen/zoom", NULL));

	gtk_object_set_data_full (GTK_OBJECT (image_view), "image_view:gconf_data",
				  priv, image_view_destroy_gconf_data);
}
