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

G_DEFINE_TYPE (EogPropertiesDialog, eog_properties_dialog, G_TYPE_OBJECT);

struct _EogPropertiesDialogPrivate {
	GtkWidget      *dlg;
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
pd_update_general_tab (EogPropertiesDialog *prop, 
		       EogImage            *image)
{
	const gchar *type_str; 
	gchar *bytes_str, *uri_str;
	gint width, height, bytes;

	uri_str = eog_image_get_uri_for_display (image);

	g_object_set (G_OBJECT (prop->priv->thumbnail_image),
		      "pixbuf", eog_image_get_pixbuf_thumbnail (image),
		      NULL);

	gtk_label_set_text (GTK_LABEL (prop->priv->name_label), 
			    eog_image_get_caption (image));

	eog_image_get_size (image, &width, &height);

	gtk_label_set_text (GTK_LABEL (prop->priv->size_label), 
			    g_strdup_printf ("%dx%d", width, height));

	type_str = gnome_vfs_mime_get_description (gnome_vfs_get_mime_type (uri_str));

	gtk_label_set_text (GTK_LABEL (prop->priv->type_label), type_str);

	bytes = eog_image_get_bytes (image);
	bytes_str = gnome_vfs_format_file_size_for_display (bytes);

	gtk_label_set_text (GTK_LABEL (prop->priv->bytes_label), bytes_str);

	gtk_label_set_text (GTK_LABEL (prop->priv->location_label), 
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
	eog_properties_dialog_hide (EOG_PROPERTIES_DIALOG (user_data));
}

static gint
eog_properties_dialog_delete (GtkWidget   *widget, 
			      GdkEventAny *event, 
			      gpointer     user_data)
{
	g_return_val_if_fail (EOG_IS_PROPERTIES_DIALOG (user_data), FALSE);

	eog_properties_dialog_hide (EOG_PROPERTIES_DIALOG (user_data));

	return TRUE;
}

static void
eog_properties_dialog_dispose (GObject *object)
{
	EogPropertiesDialog *prop;
	EogPropertiesDialogPrivate *priv;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_PROPERTIES_DIALOG (object));

	prop = EOG_PROPERTIES_DIALOG (object);
	priv = prop->priv;

	if (priv->dlg) {
		gtk_widget_destroy (priv->dlg);
		priv->dlg = NULL;
	}

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

	g_type_class_add_private (g_object_class, sizeof (EogPropertiesDialogPrivate));
}

static void
eog_properties_dialog_init (EogPropertiesDialog *prop)
{
	GladeXML *xml;
	char *filename;

	prop->priv = EOG_PROPERTIES_DIALOG_GET_PRIVATE (prop);

	filename = g_build_filename (DATADIR, "eog.glade", NULL);
	xml = glade_xml_new (filename, "eog_image_properties_dialog", "eog");

	g_free (filename);

	g_assert (xml != NULL);

	prop->priv->dlg = glade_xml_get_widget (xml, 
						"eog_image_properties_dialog");

	g_signal_connect (prop->priv->dlg,
			  "delete-event",
			  G_CALLBACK (eog_properties_dialog_delete),
			  prop);

	prop->priv->notebook = glade_xml_get_widget (xml, "notebook");

	prop->priv->previous_button = 
			glade_xml_get_widget (xml, "previous_button");

	g_signal_connect (prop->priv->previous_button,
			  "clicked",
			  G_CALLBACK (pd_previous_button_clicked_cb),
			  prop);

	prop->priv->next_button = 
			glade_xml_get_widget (xml, "next_button");

	g_signal_connect (prop->priv->next_button,
			  "clicked",
			  G_CALLBACK (pd_next_button_clicked_cb),
			  prop);

	prop->priv->close_button = 
			glade_xml_get_widget (xml, "close_button");

	g_signal_connect (prop->priv->close_button,
			  "clicked",
			  G_CALLBACK (pd_close_button_clicked_cb),
			  prop);

	prop->priv->thumbnail_image = 
			glade_xml_get_widget (xml, "thumbnail_image");

	gtk_widget_set_size_request (prop->priv->thumbnail_image, 100, 100);

	prop->priv->name_label = 
			glade_xml_get_widget (xml, "name_label");

	prop->priv->size_label = 
			glade_xml_get_widget (xml, "size_label");

	prop->priv->type_label = 
			glade_xml_get_widget (xml, "type_label");

	prop->priv->bytes_label = 
			glade_xml_get_widget (xml, "bytes_label");

	prop->priv->location_label = 
			glade_xml_get_widget (xml, "location_label");
}

GObject *
eog_properties_dialog_new (GtkWindow *parent, EogThumbView *thumbview)
{
	GObject *prop;

	prop = g_object_new (EOG_TYPE_PROPERTIES_DIALOG, NULL);

	EOG_PROPERTIES_DIALOG (prop)->priv->thumbview = g_object_ref (thumbview);

	gtk_window_set_transient_for (GTK_WINDOW (EOG_PROPERTIES_DIALOG (prop)->priv->dlg), 
				      parent);

	return prop;
}

void
eog_properties_dialog_show (EogPropertiesDialog *prop)
{
	gtk_widget_show_all (prop->priv->dlg);
}

void
eog_properties_dialog_hide (EogPropertiesDialog *prop)
{
	gtk_widget_hide_all (prop->priv->dlg);
}

void
eog_properties_dialog_update (EogPropertiesDialog *properties,
			      EogImage            *image)
{
	pd_update_general_tab (properties, image);
}
