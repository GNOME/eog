/* Eye Of Gnome - EOG Preferences Dialog
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

#include "eog-preferences-dialog.h"
#include "eog-scroll-view.h"
#include "eog-util.h"
#include "eog-config-keys.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <libpeas-gtk/peas-gtk-plugin-manager.h>

#define EOG_PREFERENCES_DIALOG_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EOG_TYPE_PREFERENCES_DIALOG, EogPreferencesDialogPrivate))

G_DEFINE_TYPE (EogPreferencesDialog, eog_preferences_dialog, EOG_TYPE_DIALOG);

#define GCONF_OBJECT_KEY	"GCONF_KEY"
#define GCONF_OBJECT_VALUE	"GCONF_VALUE"

struct _EogPreferencesDialogPrivate {
	GSettings     *view_settings;
	GSettings     *fullscreen_settings;
};

static GObject *instance = NULL;

static gboolean
pd_string_to_color_mapping (GValue   *value,
			    GVariant *variant,
			    gpointer  user_data)
{
	GdkColor color;

	g_return_val_if_fail (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING), FALSE);

	if (gdk_color_parse (g_variant_get_string (variant, NULL), &color)) {
		g_value_set_boxed (value, &color);
		return TRUE;
	}

	return FALSE;
}

static GVariant*
pd_color_to_string_mapping (const GValue       *value,
			    const GVariantType *expected_type,
			    gpointer            user_data)
{
	GVariant *variant = NULL;
	GdkColor *color;
	gchar *hex_val;

	g_return_val_if_fail (G_VALUE_TYPE (value) == GDK_TYPE_COLOR, NULL);
	g_return_val_if_fail (g_variant_type_equal (expected_type, G_VARIANT_TYPE_STRING), NULL);

	color = g_value_get_boxed (value);
	hex_val = g_strdup_printf ("#%02X%02X%02X",
				   color->red / 256,
				   color->green / 256,
				   color->blue / 256);
	variant = g_variant_new_string (hex_val);
	g_free (hex_val);

	return variant;
}

static void
pd_transp_radio_toggle_cb (GtkWidget *widget, gpointer data)
{
	gpointer value = NULL;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
	    return;

	value = g_object_get_data (G_OBJECT (widget), GCONF_OBJECT_VALUE);

	g_settings_set_enum (G_SETTINGS (data), EOG_CONF_VIEW_TRANSPARENCY,
			     GPOINTER_TO_INT (value));
}

static gchar*
pd_seconds_scale_format_value_cb (GtkScale *scale, gdouble value, gpointer ptr)
{
	gulong int_val = (gulong) value;

	return g_strdup_printf (ngettext("%lu second", "%lu seconds", int_val),
	                        int_val);
}

static void
eog_preferences_response_cb (GtkDialog *dlg, gint res_id, gpointer data)
{
	switch (res_id) {
		case GTK_RESPONSE_HELP:
			eog_util_show_help ("preferences", NULL);
			break;
		default:
			gtk_widget_destroy (GTK_WIDGET (dlg));
			instance = NULL;
	}
}

static GObject *
eog_preferences_dialog_constructor (GType type,
				    guint n_construct_properties,
				    GObjectConstructParam *construct_params)

{
	EogPreferencesDialogPrivate *priv;
	GtkWidget *dlg;
	GtkWidget *interpolate_check;
	GtkWidget *extrapolate_check;
	GtkWidget *autorotate_check;
	GtkWidget *bg_color_check;
	GtkWidget *bg_color_button;
	GtkWidget *color_radio;
	GtkWidget *checkpattern_radio;
	GtkWidget *background_radio;
	GtkWidget *color_button;
	GtkWidget *upscale_check;
	GtkWidget *loop_check;
	GtkWidget *seconds_scale;
	GtkWidget *plugin_manager;
	GtkWidget *plugin_manager_container;
	GtkAdjustment *scale_adjustment;
	GObject *object;

	object = G_OBJECT_CLASS (eog_preferences_dialog_parent_class)->constructor
			(type, n_construct_properties, construct_params);

	priv = EOG_PREFERENCES_DIALOG (object)->priv;

	priv->view_settings = g_settings_new (EOG_CONF_VIEW);
	priv->fullscreen_settings = g_settings_new (EOG_CONF_FULLSCREEN);

	eog_dialog_construct (EOG_DIALOG (object),
			      "eog-preferences-dialog.ui",
			      "eog_preferences_dialog");

	eog_dialog_get_controls (EOG_DIALOG (object),
			         "eog_preferences_dialog", &dlg,
			         "interpolate_check", &interpolate_check,
			         "extrapolate_check", &extrapolate_check,
			         "autorotate_check", &autorotate_check,
				 "bg_color_check", &bg_color_check,
				 "bg_color_button", &bg_color_button,
			         "color_radio", &color_radio,
			         "checkpattern_radio", &checkpattern_radio,
			         "background_radio", &background_radio,
			         "color_button", &color_button,
			         "upscale_check", &upscale_check,
			         "loop_check", &loop_check,
			         "seconds_scale", &seconds_scale,
			         "plugin_manager_container", &plugin_manager_container,
			         NULL);

	g_signal_connect (G_OBJECT (dlg),
			  "response",
			  G_CALLBACK (eog_preferences_response_cb),
			  dlg);

	g_settings_bind (priv->view_settings, EOG_CONF_VIEW_INTERPOLATE,
			 interpolate_check, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (priv->view_settings, EOG_CONF_VIEW_EXTRAPOLATE,
			 extrapolate_check, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (priv->view_settings, EOG_CONF_VIEW_AUTOROTATE,
			 autorotate_check, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (priv->view_settings, EOG_CONF_VIEW_USE_BG_COLOR,
			 bg_color_check, "active", G_SETTINGS_BIND_DEFAULT);

	g_settings_bind_with_mapping (priv->view_settings,
				      EOG_CONF_VIEW_BACKGROUND_COLOR,
				      bg_color_button, "color",
				      G_SETTINGS_BIND_DEFAULT,
				      pd_string_to_color_mapping,
				      pd_color_to_string_mapping,
				      NULL, NULL);

	g_object_set_data (G_OBJECT (color_radio),
			   GCONF_OBJECT_VALUE,
			   GINT_TO_POINTER (EOG_TRANSP_COLOR));

	g_signal_connect (G_OBJECT (color_radio),
			  "toggled",
			  G_CALLBACK (pd_transp_radio_toggle_cb),
			  priv->view_settings);

	g_object_set_data (G_OBJECT (checkpattern_radio),
			   GCONF_OBJECT_VALUE,
			   GINT_TO_POINTER (EOG_TRANSP_CHECKED));

	g_signal_connect (G_OBJECT (checkpattern_radio),
			  "toggled",
			  G_CALLBACK (pd_transp_radio_toggle_cb),
			  priv->view_settings);

	g_object_set_data (G_OBJECT (background_radio),
			   GCONF_OBJECT_VALUE,
			   GINT_TO_POINTER (EOG_TRANSP_BACKGROUND));

	g_signal_connect (G_OBJECT (background_radio),
			  "toggled",
			  G_CALLBACK (pd_transp_radio_toggle_cb),
			  priv->view_settings);

	g_signal_connect (G_OBJECT (seconds_scale), "format-value",
			  G_CALLBACK (pd_seconds_scale_format_value_cb),
			  NULL);

	switch (g_settings_get_enum (priv->view_settings,
				     EOG_CONF_VIEW_TRANSPARENCY))
	{
	case EOG_TRANSP_COLOR:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (color_radio), TRUE);
		break;
	case EOG_TRANSP_CHECKED:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkpattern_radio), TRUE);
		break;
	default:
		// Log a warning and use EOG_TRANSP_BACKGROUND as fallback
		g_warn_if_reached();
	case EOG_TRANSP_BACKGROUND:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (background_radio), TRUE);
		break;
	}

	g_settings_bind_with_mapping (priv->view_settings,
				      EOG_CONF_VIEW_TRANS_COLOR,
				      color_button, "color",
				      G_SETTINGS_BIND_DEFAULT,
				      pd_string_to_color_mapping,
				      pd_color_to_string_mapping,
				      NULL, NULL);

	g_settings_bind (priv->fullscreen_settings, EOG_CONF_FULLSCREEN_UPSCALE,
			 upscale_check, "active", G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (priv->fullscreen_settings, EOG_CONF_FULLSCREEN_LOOP,
			 loop_check, "active", G_SETTINGS_BIND_DEFAULT);

	scale_adjustment = gtk_range_get_adjustment (GTK_RANGE (seconds_scale));

	g_settings_bind (priv->fullscreen_settings, EOG_CONF_FULLSCREEN_SECONDS,
			 scale_adjustment, "value", G_SETTINGS_BIND_DEFAULT);

        plugin_manager = peas_gtk_plugin_manager_new (NULL);

        g_assert (plugin_manager != NULL);

        gtk_box_pack_start (GTK_BOX (plugin_manager_container),
                            plugin_manager,
                            TRUE,
                            TRUE,
                            0);

        gtk_widget_show_all (plugin_manager);

	return object;
}

static void
eog_preferences_dialog_class_init (EogPreferencesDialogClass *class)
{
	GObjectClass *g_object_class = (GObjectClass *) class;

	g_object_class->constructor = eog_preferences_dialog_constructor;

	g_type_class_add_private (g_object_class, sizeof (EogPreferencesDialogPrivate));
}

static void
eog_preferences_dialog_init (EogPreferencesDialog *pref_dlg)
{
	pref_dlg->priv = EOG_PREFERENCES_DIALOG_GET_PRIVATE (pref_dlg);
}

GObject *
eog_preferences_dialog_get_instance (GtkWindow *parent)
{
	if (instance == NULL) {
		instance = g_object_new (EOG_TYPE_PREFERENCES_DIALOG,
				 	 "parent-window", parent,
				 	 NULL);
	}

	return instance;
}
