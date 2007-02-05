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
#include "eog-util.h"
#include "eog-thumb-view.h"
#include "eog-exif-details.h"

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

	GtkWidget      *general_box;
	GtkWidget      *thumbnail_image;
	GtkWidget      *name_label;
	GtkWidget      *width_label;
	GtkWidget      *height_label;
	GtkWidget      *type_label;
	GtkWidget      *bytes_label;
	GtkWidget      *location_label;
	GtkWidget      *created_label;
	GtkWidget      *modified_label;
#ifdef HAVE_EXIF
	GtkWidget      *exif_box;
	GtkWidget      *exif_aperture_label;
	GtkWidget      *exif_exposure_label;
	GtkWidget      *exif_focal_label;
	GtkWidget      *exif_flash_label;
	GtkWidget      *exif_iso_label;
	GtkWidget      *exif_metering_label;
	GtkWidget      *exif_model_label;
	GtkWidget      *exif_date_label;
	GtkWidget      *exif_details_expander;
	GtkWidget      *exif_details;
#endif
};

static void
pd_update_general_tab (EogPropertiesDialog *prop_dlg, 
		       EogImage            *image)
{
	const gchar *type_str; 
	gchar *bytes_str, *dir_str, *mime_str, *uri_str;
	gint width, height, bytes;

	uri_str = eog_image_get_uri_for_display (image);

	g_object_set (G_OBJECT (prop_dlg->priv->thumbnail_image),
		      "pixbuf", eog_image_get_thumbnail (image),
		      NULL);

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->name_label), 
			    eog_image_get_caption (image));

	eog_image_get_size (image, &width, &height);

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->width_label), 
			    g_strdup_printf ("%d pixels", width));

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->height_label), 
			    g_strdup_printf ("%d pixels", height));

	mime_str = gnome_vfs_get_mime_type (uri_str);
	type_str = gnome_vfs_mime_get_description (mime_str);

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->type_label), type_str);

	bytes = eog_image_get_bytes (image);
	bytes_str = gnome_vfs_format_file_size_for_display (bytes);

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->bytes_label), bytes_str);

	dir_str = g_path_get_dirname (uri_str);
	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->location_label), 
			    dir_str);

	g_free (uri_str);
	g_free (mime_str);
	g_free (bytes_str);
	g_free (dir_str);
}

#ifdef HAVE_EXIF
static const gchar * 
pd_get_exif_value (ExifData *exif_data, gint tag_id)
{
	ExifEntry *exif_entry;
	const gchar *exif_value;
	gchar buffer[1024];

        exif_entry = exif_data_get_entry (exif_data, tag_id);

	exif_value = exif_entry_get_value (exif_entry, buffer, sizeof (buffer));

	return exif_value;
}

static void
pd_update_exif_tab (EogPropertiesDialog *prop_dlg, 
		    EogImage            *image)
{
	GtkNotebook *notebook;
	ExifData *exif_data;
	const gchar *exif_value = NULL;

	notebook = GTK_NOTEBOOK (prop_dlg->priv->notebook);

	if (!eog_image_has_data (image, EOG_IMAGE_DATA_EXIF)) {
		if (gtk_notebook_get_current_page (notebook) ==	EOG_PROPERTIES_DIALOG_PAGE_EXIF) {
			gtk_notebook_prev_page (notebook);
		}

		if (GTK_WIDGET_VISIBLE (prop_dlg->priv->exif_box)) {
			gtk_widget_hide_all (prop_dlg->priv->exif_box);
		}

		return;
	} else if (!GTK_WIDGET_VISIBLE (prop_dlg->priv->exif_box)) {
		gtk_widget_show_all (prop_dlg->priv->exif_box);
	}

	exif_data = (ExifData *) eog_image_get_exif_info (image);

	exif_value = pd_get_exif_value (exif_data, EXIF_TAG_APERTURE_VALUE);
	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->exif_aperture_label), 
			    eog_util_make_valid_utf8 (exif_value));

	exif_value = pd_get_exif_value (exif_data, EXIF_TAG_EXPOSURE_TIME);
	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->exif_exposure_label),
			    eog_util_make_valid_utf8 (exif_value));

	exif_value = pd_get_exif_value (exif_data, EXIF_TAG_FOCAL_LENGTH);
	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->exif_focal_label), 
			    eog_util_make_valid_utf8 (exif_value));

	exif_value = pd_get_exif_value (exif_data, EXIF_TAG_FLASH);
	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->exif_flash_label),
			    eog_util_make_valid_utf8 (exif_value));

	exif_value = pd_get_exif_value (exif_data, EXIF_TAG_ISO_SPEED_RATINGS);
	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->exif_iso_label), 
			    eog_util_make_valid_utf8 (exif_value));

	exif_value = pd_get_exif_value (exif_data, EXIF_TAG_METERING_MODE);
	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->exif_metering_label),
			    eog_util_make_valid_utf8 (exif_value));

	exif_value = pd_get_exif_value (exif_data, EXIF_TAG_MODEL);
	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->exif_model_label),
			    eog_util_make_valid_utf8 (exif_value));

	exif_value = pd_get_exif_value (exif_data, EXIF_TAG_DATE_TIME);
	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->exif_date_label),
			    eog_util_format_exif_date (exif_value));

	eog_exif_details_update (EOG_EXIF_DETAILS (prop_dlg->priv->exif_details), 
				 exif_data);
}
#endif

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
#ifdef HAVE_EXIF
	GtkWidget *sw;
#endif

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
			         "general_box", &priv->general_box,
			         "name_label", &priv->name_label,
			         "width_label", &priv->width_label,
			         "height_label", &priv->height_label,
			         "type_label", &priv->type_label,
			         "bytes_label", &priv->bytes_label,
			         "location_label", &priv->location_label,
			         "created_label", &priv->created_label,
			         "modified_label", &priv->modified_label,
#ifdef HAVE_EXIF
			         "exif_box", &priv->exif_box,
			         "exif_aperture_label", &priv->exif_aperture_label,
			         "exif_exposure_label", &priv->exif_exposure_label,
			         "exif_focal_label", &priv->exif_focal_label,
			         "exif_flash_label", &priv->exif_flash_label,
			         "exif_iso_label", &priv->exif_iso_label,
			         "exif_metering_label", &priv->exif_metering_label,
			         "exif_model_label", &priv->exif_model_label,
			         "exif_date_label", &priv->exif_date_label,
			         "exif_details_expander", &priv->exif_details_expander,
#endif
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

#ifdef HAVE_EXIF
	sw = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), 
					     GTK_SHADOW_IN);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	priv->exif_details = eog_exif_details_new ();
	gtk_widget_set_size_request (priv->exif_details, -1, 170);

	gtk_container_add (GTK_CONTAINER (sw), priv->exif_details);
	gtk_widget_show_all (sw);

	gtk_container_add (GTK_CONTAINER (priv->exif_details_expander), sw);
#else
        gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook), 
				  EOG_PROPERTIES_DIALOG_PAGE_EXIF);
#endif
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
	g_return_if_fail (EOG_IS_PROPERTIES_DIALOG (prop_dlg));

	pd_update_general_tab (prop_dlg, image);

#ifdef HAVE_EXIF
	pd_update_exif_tab (prop_dlg, image);
#endif
}

void
eog_properties_dialog_set_page (EogPropertiesDialog *prop_dlg,
			        EogPropertiesDialogPage page)
{
	g_return_if_fail (EOG_IS_PROPERTIES_DIALOG (prop_dlg));
	
	gtk_notebook_set_current_page (GTK_NOTEBOOK (prop_dlg->priv->notebook),
				       page);
}
