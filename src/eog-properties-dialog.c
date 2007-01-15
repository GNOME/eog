/* Eye Of Gnome - Image Properties Dialog 
 *
 * Copyright (C) 2006 The Free Software Foundation
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "eog-properties-dialog.h"
#include "eog-image.h"
#include "eog-thumb-view.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#define EOG_PROPERTIES_DIALOG_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EOG_TYPE_PROPERTIES_DIALOG, EogPropertiesDialogPrivate))

G_DEFINE_TYPE (EogPropertiesDialog, eog_properties_dialog, EOG_TYPE_DIALOG);

enum {
        PROP_0,
        PROP_THUMBVIEW,
};

struct _EogPropertiesDialogPrivate {
	EogThumbView   *thumbview;

	GtkWidget      *notebook;
	GtkWidget      *close_button;
	GtkWidget      *next_button;
	GtkWidget      *previous_button;

	GtkWidget      *thumbnail_image;
	GtkWidget      *name_label;
	GtkWidget      *size_label;
	GtkWidget      *type_label;
	GtkWidget      *bytes_label;
	GtkWidget      *location_label;
	GtkWidget      *modified_label;
};

static void
pd_update_general_tab (EogPropertiesDialog *prop_dlg, 
		       EogImage            *image)
{
	const gchar *type_str; 
	gchar *bytes_str, *uri_str;
	gint width, height, bytes;

	uri_str = eog_image_get_uri_for_display (image);

	g_object_set (G_OBJECT (prop_dlg->priv->thumbnail_image),
		      "pixbuf", eog_image_get_thumbnail (image),
		      NULL);

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->name_label), 
			    eog_image_get_caption (image));

	eog_image_get_size (image, &width, &height);

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->size_label), 
			    g_strdup_printf ("%d x %d", width, height));

	type_str = gnome_vfs_mime_get_description (gnome_vfs_get_mime_type (uri_str));

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->type_label), type_str);

	bytes = eog_image_get_bytes (image);
	bytes_str = gnome_vfs_format_file_size_for_display (bytes);

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->bytes_label), bytes_str);

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->location_label), 
			    g_path_get_dirname (uri_str));

	g_free (uri_str);
	g_free (bytes_str);
}

static void
pd_previous_button_clicked_cb (GtkButton *button, 
			       gpointer   user_data)
{
	EogPropertiesDialogPrivate *priv;

	g_return_if_fail (EOG_IS_PROPERTIES_DIALOG (user_data));

	priv = EOG_PROPERTIES_DIALOG (user_data)->priv;

	eog_thumb_view_select_single (EOG_THUMB_VIEW (priv->thumbview), 
				      EOG_THUMB_VIEW_SELECT_LEFT);
}

static void
pd_next_button_clicked_cb (GtkButton *button, 
			   gpointer   user_data)
{
	EogPropertiesDialogPrivate *priv;

	g_return_if_fail (EOG_IS_PROPERTIES_DIALOG (user_data));

	priv = EOG_PROPERTIES_DIALOG (user_data)->priv;

	eog_thumb_view_select_single (EOG_THUMB_VIEW (priv->thumbview), 
				      EOG_THUMB_VIEW_SELECT_RIGHT);
}

static void
pd_close_button_clicked_cb (GtkButton *button, 
			    gpointer   user_data)
{
	eog_dialog_hide (EOG_DIALOG (user_data));
}

static gint
eog_properties_dialog_delete (GtkWidget   *widget, 
			      GdkEventAny *event, 
			      gpointer     user_data)
{
	g_return_val_if_fail (EOG_IS_PROPERTIES_DIALOG (user_data), FALSE);

	eog_dialog_hide (EOG_DIALOG (user_data));

	return TRUE;
}

static void
eog_properties_dialog_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
	EogPropertiesDialog *prop_dlg = EOG_PROPERTIES_DIALOG (object);

	switch (prop_id) {
		case PROP_THUMBVIEW:
			prop_dlg->priv->thumbview = g_value_get_object (value);
			break;
	}
}

static void
eog_properties_dialog_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
	EogPropertiesDialog *prop_dlg = EOG_PROPERTIES_DIALOG (object);

	switch (prop_id) {
		case PROP_THUMBVIEW:
			g_value_set_object (value, prop_dlg->priv->thumbview);
			break;
	}
}

static void
eog_properties_dialog_dispose (GObject *object)
{
	EogPropertiesDialog *prop_dlg;
	EogPropertiesDialogPrivate *priv;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_PROPERTIES_DIALOG (object));

	prop_dlg = EOG_PROPERTIES_DIALOG (object);
	priv = prop_dlg->priv;

	if (priv->thumbview) {
		g_object_unref (priv->thumbview);
		priv->thumbview = NULL;
	}

	G_OBJECT_CLASS (eog_properties_dialog_parent_class)->dispose (object);
}

static void
eog_properties_dialog_class_init (EogPropertiesDialogClass *class)
{
	GObjectClass *g_object_class = (GObjectClass *) class;

	g_object_class->dispose = eog_properties_dialog_dispose;
	g_object_class->set_property = eog_properties_dialog_set_property;
	g_object_class->get_property = eog_properties_dialog_get_property;

	g_object_class_install_property (g_object_class,
					 PROP_THUMBVIEW,
					 g_param_spec_object ("thumbview",
							      "Thumbview",
							      "Thumbview",
							      EOG_TYPE_THUMB_VIEW,
							      G_PARAM_READWRITE | 
							      G_PARAM_CONSTRUCT_ONLY | 
							      G_PARAM_STATIC_NAME | 
							      G_PARAM_STATIC_NICK | 
							      G_PARAM_STATIC_BLURB));

	g_type_class_add_private (g_object_class, sizeof (EogPropertiesDialogPrivate));
}

static void
eog_properties_dialog_init (EogPropertiesDialog *prop_dlg)
{
	EogPropertiesDialogPrivate *priv;
	GtkWidget *dlg;

	prop_dlg->priv = EOG_PROPERTIES_DIALOG_GET_PRIVATE (prop_dlg);

	priv = prop_dlg->priv;

	eog_dialog_construct (EOG_DIALOG (prop_dlg),
			      "eog.glade",
			      "eog_image_properties_dialog");
 
	eog_dialog_get_controls (EOG_DIALOG (prop_dlg), 
			         "eog_image_properties_dialog", &dlg,
			         "notebook", &priv->notebook,
			         "previous_button", &priv->previous_button,
			         "next_button", &priv->next_button,
			         "close_button", &priv->close_button,
			         "thumbnail_image", &priv->thumbnail_image,
			         "name_label", &priv->name_label,
			         "size_label", &priv->size_label,
			         "type_label", &priv->type_label,
			         "bytes_label", &priv->bytes_label,
			         "location_label", &priv->location_label,
			         NULL);

	g_signal_connect (dlg,
			  "delete-event",
			  G_CALLBACK (eog_properties_dialog_delete),
			  prop_dlg);

	g_signal_connect (priv->previous_button,
			  "clicked",
			  G_CALLBACK (pd_previous_button_clicked_cb),
			  prop_dlg);

	g_signal_connect (priv->next_button,
			  "clicked",
			  G_CALLBACK (pd_next_button_clicked_cb),
			  prop_dlg);

	g_signal_connect (priv->close_button,
			  "clicked",
			  G_CALLBACK (pd_close_button_clicked_cb),
			  prop_dlg);

	gtk_widget_set_size_request (priv->thumbnail_image, 100, 100);
}

GObject *
eog_properties_dialog_new (GtkWindow *parent, EogThumbView *thumbview)
{
	GObject *prop_dlg;

	prop_dlg = g_object_new (EOG_TYPE_PROPERTIES_DIALOG, 
				 "parent-window", parent,
			     	 "thumbview", thumbview,
			     	 NULL);

	return prop_dlg;
}

void
eog_properties_dialog_update (EogPropertiesDialog *prop_dlg,
			      EogImage            *image)
{
	pd_update_general_tab (prop_dlg, image);
}
