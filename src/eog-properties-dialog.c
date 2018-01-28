/* Eye Of Gnome - Image Properties Dialog
 *
 * Copyright (C) 2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *         Hubert Figuiere <hub@figuiere.net> (XMP support)
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

#include "eog-properties-dialog.h"
#include "eog-image.h"
#include "eog-util.h"
#include "eog-thumb-view.h"

#if HAVE_EXIF
#include "eog-exif-util.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#if HAVE_EXEMPI
#include <exempi/xmp.h>
#include <exempi/xmpconsts.h>
#endif
#if HAVE_EXIF || HAVE_EXEMPI
#define HAVE_METADATA 1
#endif

#if HAVE_METADATA
#include "eog-metadata-details.h"
#endif

enum {
        PROP_0,
        PROP_THUMBVIEW,
        PROP_NETBOOK_MODE,
        PROP_NEXT_ACTION,
        PROP_PREV_ACTION
};

struct _EogPropertiesDialogPrivate {
	EogThumbView   *thumbview;

	gboolean        update_page;
	EogPropertiesDialogPage current_page;

	GtkWidget      *notebook;
	GtkWidget      *next_button;
	GtkWidget      *previous_button;

	GtkWidget      *general_box;
	GtkWidget      *thumbnail_image;
	GtkWidget      *name_label;
	GtkWidget      *width_label;
	GtkWidget      *height_label;
	GtkWidget      *type_label;
	GtkWidget      *bytes_label;
	GtkWidget      *folder_button;
	gchar          *folder_button_uri;

#ifdef HAVE_EXIF
	GtkWidget      *exif_aperture_label;
	GtkWidget      *exif_exposure_label;
	GtkWidget      *exif_focal_label;
	GtkWidget      *exif_flash_label;
	GtkWidget      *exif_iso_label;
	GtkWidget      *exif_metering_label;
	GtkWidget      *exif_model_label;
	GtkWidget      *exif_date_label;
#endif
#ifdef HAVE_EXEMPI
	GtkWidget      *xmp_location_label;
	GtkWidget      *xmp_description_label;
	GtkWidget      *xmp_keywords_label;
	GtkWidget      *xmp_creator_label;
	GtkWidget      *xmp_rights_label;
#else
	GtkWidget      *xmp_box;
	GtkWidget      *xmp_box_label;
#endif
#if HAVE_METADATA
	GtkWidget      *metadata_box;
	GtkWidget      *metadata_details_expander;
	GtkWidget      *metadata_details;
	GtkWidget      *metadata_details_box;
	GtkWidget      *metadata_details_sw;
#endif

	gboolean        netbook_mode;
};

G_DEFINE_TYPE_WITH_PRIVATE (EogPropertiesDialog, eog_properties_dialog, GTK_TYPE_DIALOG);

static void
parent_file_display_name_query_info_cb (GObject *source_object,
					GAsyncResult *res,
					gpointer user_data)
{
	EogPropertiesDialog *prop_dlg = EOG_PROPERTIES_DIALOG (user_data);
	GFile *parent_file = G_FILE (source_object);
	GFileInfo *file_info;
	gchar *display_name;


	file_info = g_file_query_info_finish (parent_file, res, NULL);
	if (file_info == NULL) {
		display_name = g_file_get_basename (parent_file);
	} else {
		display_name = g_strdup (
			g_file_info_get_display_name (file_info));
		g_object_unref (file_info);
	}
	gtk_button_set_label (GTK_BUTTON (prop_dlg->priv->folder_button),
			      display_name);
	gtk_widget_set_sensitive (prop_dlg->priv->folder_button, TRUE);

	g_free (display_name);
	g_object_unref (prop_dlg);
}

static void
pd_update_general_tab (EogPropertiesDialog *prop_dlg,
		       EogImage            *image)
{
	gchar *bytes_str;
	gchar *width_str, *height_str;
	GFile *file, *parent_file;
	GFileInfo *file_info;
	const char *mime_str;
	char *type_str;
	gint width, height;
	goffset bytes;

	g_object_set (G_OBJECT (prop_dlg->priv->thumbnail_image),
		      "pixbuf", eog_image_get_thumbnail (image),
		      NULL);

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->name_label),
			    eog_image_get_caption (image));

	eog_image_get_size (image, &width, &height);

	width_str = g_strdup_printf ("%d %s", width,
				     ngettext ("pixel", "pixels", width));
	height_str = g_strdup_printf ("%d %s", height,
				      ngettext ("pixel", "pixels", height));

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->width_label), width_str);

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->height_label),
			    height_str);

	g_free (height_str);
	g_free (width_str);

	file = eog_image_get_file (image);
	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				       0, NULL, NULL);
	if (file_info == NULL) {
		type_str = g_strdup (_("Unknown"));
	} else {
		mime_str = g_file_info_get_content_type (file_info);
		type_str = g_content_type_get_description (mime_str);
		g_object_unref (file_info);
	}

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->type_label), type_str);

	bytes = eog_image_get_bytes (image);
	bytes_str = g_format_size (bytes);

	gtk_label_set_text (GTK_LABEL (prop_dlg->priv->bytes_label), bytes_str);

	parent_file = g_file_get_parent (file);
	if (parent_file == NULL) {
		/* file is root directory itself */
		parent_file = g_object_ref (file);
	}

	gtk_widget_set_sensitive (prop_dlg->priv->folder_button, FALSE);
	gtk_button_set_label (GTK_BUTTON (prop_dlg->priv->folder_button), NULL);
	g_free (prop_dlg->priv->folder_button_uri);
	prop_dlg->priv->folder_button_uri = g_file_get_uri (parent_file);

	g_file_query_info_async (parent_file,
				 G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
				 G_FILE_QUERY_INFO_NONE,
				 G_PRIORITY_DEFAULT,
				 NULL,
				 parent_file_display_name_query_info_cb,
				 g_object_ref (prop_dlg));

	g_object_unref (parent_file);
	g_free (type_str);
	g_free (bytes_str);
}

#if HAVE_EXEMPI
static void
eog_xmp_set_label (XmpPtr xmp,
		   const char *ns,
		   const char *propname,
		   GtkWidget *w)
{
	uint32_t options;

	XmpStringPtr value = xmp_string_new ();

	if (xmp_get_property (xmp, ns, propname, value, &options)) {
		if (XMP_IS_PROP_SIMPLE (options)) {
			gtk_label_set_text (GTK_LABEL (w), xmp_string_cstr (value));
		} else if (XMP_IS_PROP_ARRAY (options)) {
			XmpIteratorPtr iter = xmp_iterator_new (xmp,
							        ns,
								propname,
								XMP_ITER_JUSTLEAFNODES);

			GString *string = g_string_new ("");

			if (iter) {
				gboolean first = TRUE;

				while (xmp_iterator_next (iter, NULL, NULL, value, &options)
				       && !XMP_IS_PROP_QUALIFIER (options)) {

					if (!first) {
						g_string_append_printf(string, ", ");
					} else {
						first = FALSE;
					}

					g_string_append_printf (string,
								"%s",
								xmp_string_cstr (value));
				}

				xmp_iterator_free (iter);
			}

			gtk_label_set_text (GTK_LABEL (w), string->str);
			g_string_free (string, TRUE);
		}
	} else {
		/* Property was not found */
		/* Clear label so it won't show bogus data */
		gtk_label_set_text (GTK_LABEL (w), NULL);
	}

	xmp_string_free (value);
}
#endif

#if HAVE_METADATA
static void
pd_update_metadata_tab (EogPropertiesDialog *prop_dlg,
			EogImage            *image)
{
	EogPropertiesDialogPrivate *priv;
	GtkNotebook *notebook;
#if HAVE_EXIF
	ExifData    *exif_data;
#endif
#if HAVE_EXEMPI
	XmpPtr      xmp_data;
#endif

	g_return_if_fail (EOG_IS_PROPERTIES_DIALOG (prop_dlg));

	priv = prop_dlg->priv;

	notebook = GTK_NOTEBOOK (priv->notebook);

	if (TRUE
#if HAVE_EXIF
	    && !eog_image_has_data (image, EOG_IMAGE_DATA_EXIF)
#endif
#if HAVE_EXEMPI
	    && !eog_image_has_data (image, EOG_IMAGE_DATA_XMP)
#endif
	    ) {
		if (gtk_notebook_get_current_page (notebook) ==	EOG_PROPERTIES_DIALOG_PAGE_EXIF) {
			gtk_notebook_prev_page (notebook);
		} else if (gtk_notebook_get_current_page (notebook) == EOG_PROPERTIES_DIALOG_PAGE_DETAILS) {
			gtk_notebook_set_current_page (notebook, EOG_PROPERTIES_DIALOG_PAGE_GENERAL);
		}

		if (gtk_widget_get_visible (priv->metadata_box)) {
			gtk_widget_hide (priv->metadata_box);
		}
		if (gtk_widget_get_visible (priv->metadata_details_box)) {
			gtk_widget_hide (priv->metadata_details_box);
		}

		return;
	} else {
		if (!gtk_widget_get_visible (priv->metadata_box))
			gtk_widget_show_all (priv->metadata_box);
		if (priv->netbook_mode &&
		    !gtk_widget_get_visible (priv->metadata_details_box)) {
			gtk_widget_show_all (priv->metadata_details_box);
			gtk_widget_hide (priv->metadata_details_expander);
		}
	}

#if HAVE_EXIF
	exif_data = (ExifData *) eog_image_get_exif_info (image);

	eog_exif_util_set_label_text (GTK_LABEL (priv->exif_aperture_label),
				      exif_data, EXIF_TAG_FNUMBER);

	eog_exif_util_set_label_text (GTK_LABEL (priv->exif_exposure_label),
				      exif_data, EXIF_TAG_EXPOSURE_TIME);

	eog_exif_util_set_focal_length_label_text (GTK_LABEL (priv->exif_focal_label), exif_data);

	eog_exif_util_set_label_text (GTK_LABEL (priv->exif_flash_label),
				      exif_data, EXIF_TAG_FLASH);

	eog_exif_util_set_label_text (GTK_LABEL (priv->exif_iso_label),
				      exif_data, EXIF_TAG_ISO_SPEED_RATINGS);


	eog_exif_util_set_label_text (GTK_LABEL (priv->exif_metering_label),
				      exif_data, EXIF_TAG_METERING_MODE);

	eog_exif_util_set_label_text (GTK_LABEL (priv->exif_model_label),
				      exif_data, EXIF_TAG_MODEL);

	eog_exif_util_set_label_text (GTK_LABEL (priv->exif_date_label),
				      exif_data, EXIF_TAG_DATE_TIME_ORIGINAL);

	eog_metadata_details_update (EOG_METADATA_DETAILS (priv->metadata_details),
				 exif_data);

	/* exif_data_unref can handle NULL-values */
	exif_data_unref(exif_data);
#endif

#if HAVE_EXEMPI
	xmp_data = (XmpPtr) eog_image_get_xmp_info (image);

 	if (xmp_data != NULL) {
		eog_xmp_set_label (xmp_data,
				   NS_IPTC4XMP,
				   "Location",
				   priv->xmp_location_label);

		eog_xmp_set_label (xmp_data,
				   NS_DC,
				   "description",
				   priv->xmp_description_label);

		eog_xmp_set_label (xmp_data,
				   NS_DC,
				   "subject",
				   priv->xmp_keywords_label);

		eog_xmp_set_label (xmp_data,
				   NS_DC,
        	                   "creator",
				   priv->xmp_creator_label);

		eog_xmp_set_label (xmp_data,
				   NS_DC,
				   "rights",
				   priv->xmp_rights_label);

		eog_metadata_details_xmp_update (EOG_METADATA_DETAILS (priv->metadata_details), xmp_data);

		xmp_free (xmp_data);
	} else {
		/* Image has no XMP data */

		/* Clear the labels so they won't display foreign data.*/

		gtk_label_set_text (GTK_LABEL (priv->xmp_location_label), NULL);
		gtk_label_set_text (GTK_LABEL (priv->xmp_description_label),
				    NULL);
		gtk_label_set_text (GTK_LABEL (priv->xmp_keywords_label), NULL);
		gtk_label_set_text (GTK_LABEL (priv->xmp_creator_label), NULL);
		gtk_label_set_text (GTK_LABEL (priv->xmp_rights_label), NULL);
	}
#endif
}

static gboolean
pd_resize_dialog (gpointer user_data)
{
	gint width, height;

	gtk_window_get_size (GTK_WINDOW (user_data),
			     &width,
			     &height);

	gtk_window_resize (GTK_WINDOW (user_data), width, 1);

	return FALSE;
}

static void
pd_exif_details_activated_cb (GtkExpander *expander,
			      GParamSpec *param_spec,
			      GtkWidget *dialog)
{
	gboolean expanded;

	expanded = gtk_expander_get_expanded (expander);

	/*FIXME: this is depending on the expander animation
         * duration. Need to find a safer way for doing that. */
	if (!expanded)
		g_timeout_add (150, pd_resize_dialog, dialog);
}
#endif

static void
pd_folder_button_clicked_cb (GtkButton *button, gpointer data)
{
	EogPropertiesDialogPrivate *priv = EOG_PROPERTIES_DIALOG (data)->priv;
	GtkWindow *window;
	guint32 timestamp;

	if (!priv->folder_button_uri)
		return;
	
	timestamp = gtk_get_current_event_time ();

	window = GTK_WINDOW (data);
	gtk_show_uri_on_window (window, priv->folder_button_uri, timestamp, NULL);
}

static gboolean
eog_properties_dialog_page_switch (GtkNotebook     *notebook,
				   GtkWidget       *page,
				   gint             page_index,
				   EogPropertiesDialog *prop_dlg)
{
	if (prop_dlg->priv->update_page)
		prop_dlg->priv->current_page = page_index;

	return TRUE;
}

void
eog_properties_dialog_set_netbook_mode (EogPropertiesDialog *dlg,
					gboolean enable)
{
	EogPropertiesDialogPrivate *priv;

	g_return_if_fail (EOG_IS_PROPERTIES_DIALOG (dlg));

	priv = dlg->priv;

	if (priv->netbook_mode == enable)
		return;

	priv->netbook_mode = enable;

#ifdef HAVE_METADATA
	if (enable) {
		g_object_ref (priv->metadata_details_sw);
		gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (priv->metadata_details_sw)),
				      priv->metadata_details_sw);
		gtk_container_add (GTK_CONTAINER (priv->metadata_details_box), priv->metadata_details_sw);
		g_object_unref (priv->metadata_details_sw);
		// Only show details box if metadata is being displayed
		if (gtk_widget_get_visible (priv->metadata_box))
			gtk_widget_show_all (priv->metadata_details_box);

		gtk_widget_hide (priv->metadata_details_expander);
	} else {
		g_object_ref (priv->metadata_details_sw);
		gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (priv->metadata_details_sw)),
				      priv->metadata_details_sw);
		gtk_container_add (GTK_CONTAINER (priv->metadata_details_expander), priv->metadata_details_sw);
		g_object_unref (priv->metadata_details_sw);
		gtk_widget_show_all (priv->metadata_details_expander);

		if (gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook)) == EOG_PROPERTIES_DIALOG_PAGE_DETAILS)
			gtk_notebook_prev_page (GTK_NOTEBOOK (priv->notebook));
		gtk_widget_hide (priv->metadata_details_box);
	}
#endif
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
		case PROP_NETBOOK_MODE:
			eog_properties_dialog_set_netbook_mode (prop_dlg,
						   g_value_get_boolean (value));
			break;
		case PROP_NEXT_ACTION:
			gtk_actionable_set_action_name (GTK_ACTIONABLE (prop_dlg->priv->next_button),
							g_value_get_string (value));
			gtk_button_set_always_show_image(GTK_BUTTON(prop_dlg->priv->next_button), TRUE);
			break;
		case PROP_PREV_ACTION:
			gtk_actionable_set_action_name (GTK_ACTIONABLE (prop_dlg->priv->previous_button),
							g_value_get_string (value));
			gtk_button_set_always_show_image(GTK_BUTTON(prop_dlg->priv->previous_button), TRUE);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id,
							   pspec);
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
		case PROP_NETBOOK_MODE:
			g_value_set_boolean (value,
					     prop_dlg->priv->netbook_mode);
			break;
		case PROP_NEXT_ACTION:
		{
			const gchar* action = gtk_actionable_get_action_name (
						      GTK_ACTIONABLE (prop_dlg->priv->next_button));
			g_value_set_string (value, action);
			break;
		}
		case PROP_PREV_ACTION:
		{
			const gchar* action = gtk_actionable_get_action_name (
						      GTK_ACTIONABLE (prop_dlg->priv->previous_button));
			g_value_set_string (value, action);
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id,
							   pspec);
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

	g_free (priv->folder_button_uri);
	priv->folder_button_uri = NULL;

	G_OBJECT_CLASS (eog_properties_dialog_parent_class)->dispose (object);
}

static void
eog_properties_dialog_class_init (EogPropertiesDialogClass *klass)
{
	GObjectClass *g_object_class = (GObjectClass *) klass;

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
							      G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class, PROP_NETBOOK_MODE,
					 g_param_spec_boolean ("netbook-mode",
					 		      "Netbook Mode",
							      "Netbook Mode",
							      FALSE,
							      G_PARAM_READWRITE |
							      G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_NEXT_ACTION,
					 g_param_spec_string ("next-action",
							      "Next Action",
							      "Action for Next button",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_PREV_ACTION,
					 g_param_spec_string ("prev-action",
							      "Prev Action",
							      "Action for Prev button",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));

	gtk_widget_class_set_template_from_resource ((GtkWidgetClass *) klass, "/org/gnome/eog/ui/eog-image-properties-dialog.ui");

	GtkWidgetClass *wklass = (GtkWidgetClass*) klass;
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     notebook);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     previous_button);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     next_button);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     thumbnail_image);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     general_box);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     name_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     width_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     height_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     type_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     bytes_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     folder_button);

#if HAVE_EXIF
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     exif_aperture_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     exif_exposure_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     exif_focal_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     exif_flash_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     exif_iso_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     exif_metering_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     exif_model_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     exif_date_label);
#endif
#if HAVE_EXEMPI
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     xmp_location_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     xmp_description_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     xmp_keywords_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     xmp_creator_label);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     xmp_rights_label);
#else
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     xmp_box);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     xmp_box_label);
#endif
#ifdef HAVE_METADATA
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     metadata_box);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     metadata_details_expander);
	gtk_widget_class_bind_template_child_private(wklass,
						     EogPropertiesDialog,
						     metadata_details_box);

	gtk_widget_class_bind_template_callback(wklass,
						pd_exif_details_activated_cb);
#endif
	gtk_widget_class_bind_template_callback(wklass,
						eog_properties_dialog_page_switch);
	gtk_widget_class_bind_template_callback(wklass,
						pd_folder_button_clicked_cb);

}

static void
eog_properties_dialog_init (EogPropertiesDialog *prop_dlg)
{
	EogPropertiesDialogPrivate *priv;
#if HAVE_METADATA
	GtkWidget *sw;
#endif

	prop_dlg->priv = eog_properties_dialog_get_instance_private (prop_dlg);

	priv = prop_dlg->priv;

	priv->update_page = FALSE;

	gtk_widget_init_template (GTK_WIDGET (prop_dlg));


	g_signal_connect (prop_dlg,
			  "delete-event",
			  G_CALLBACK (gtk_widget_hide_on_delete),
			  prop_dlg);

	gtk_widget_set_tooltip_text (GTK_WIDGET (priv->folder_button),
				     _("Show the folder which contains this "
				       "file in the file manager"));
	priv->folder_button_uri = NULL;

	gtk_widget_set_size_request (priv->thumbnail_image, 100, 100);

#ifdef HAVE_METADATA
 	sw = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
					     GTK_SHADOW_IN);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	priv->metadata_details = eog_metadata_details_new ();
	gtk_widget_set_size_request (priv->metadata_details, -1, 170);
	gtk_widget_set_vexpand (priv->metadata_details, TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (sw), 6);

	gtk_container_add (GTK_CONTAINER (sw), priv->metadata_details);
	gtk_widget_show_all (sw);

	priv->metadata_details_sw = sw;

	if (priv->netbook_mode) {
		gtk_widget_hide (priv->metadata_details_expander);
		gtk_box_pack_start (GTK_BOX (priv->metadata_details_box),
				    sw, TRUE, TRUE, 6);
	} else {
		gtk_container_add (GTK_CONTAINER (priv->metadata_details_expander),
				   sw);
	}

#ifndef HAVE_EXEMPI
	gtk_widget_hide (priv->xmp_box);
	gtk_widget_hide (priv->xmp_box_label);
#endif

#else
	/* Remove pages from back to front. Otherwise the page index
	 * needs to be adjusted when deleting the next page. */
	gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook),
				  EOG_PROPERTIES_DIALOG_PAGE_DETAILS);
	gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook),
				  EOG_PROPERTIES_DIALOG_PAGE_EXIF);
#endif
}

/**
 * eog_properties_dialog_new:
 * @parent: the dialog's parent window
 * @thumbview: 
 * @next_image_action: 
 * @previous_image_action: 
 *
 * If %parent implements #GActionMap its actions will be automatically
 * inserted in the "win" namespace.
 *
 * Returns: (transfer full) (type EogPropertiesDialog): a new #EogPropertiesDialog
 **/
GtkWidget *
eog_properties_dialog_new (GtkWindow    *parent,
			   EogThumbView *thumbview,
			   const gchar  *next_image_action,
			   const gchar  *previous_image_action)
{
	GObject *prop_dlg;

	g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
	g_return_val_if_fail (EOG_IS_THUMB_VIEW (thumbview), NULL);

	prop_dlg = g_object_new (EOG_TYPE_PROPERTIES_DIALOG,
			     	 "thumbview", thumbview,
				 "next-action", next_image_action,
				 "prev-action", previous_image_action,
				 "use-header-bar", TRUE,
			     	 NULL);

	gtk_window_set_transient_for (GTK_WINDOW (prop_dlg), parent);

	if (G_LIKELY (G_IS_ACTION_GROUP (parent))) {
		gtk_widget_insert_action_group (GTK_WIDGET (prop_dlg),
						"win",
						G_ACTION_GROUP (parent));
	}

	return GTK_WIDGET (prop_dlg);
}

void
eog_properties_dialog_update (EogPropertiesDialog *prop_dlg,
			      EogImage            *image)
{
	g_return_if_fail (EOG_IS_PROPERTIES_DIALOG (prop_dlg));

	prop_dlg->priv->update_page = FALSE;

	pd_update_general_tab (prop_dlg, image);

#ifdef HAVE_METADATA
	pd_update_metadata_tab (prop_dlg, image);
#endif
	gtk_notebook_set_current_page (GTK_NOTEBOOK (prop_dlg->priv->notebook),
				       prop_dlg->priv->current_page);

	prop_dlg->priv->update_page = TRUE;
}

void
eog_properties_dialog_set_page (EogPropertiesDialog *prop_dlg,
			        EogPropertiesDialogPage page)
{
	g_return_if_fail (EOG_IS_PROPERTIES_DIALOG (prop_dlg));

	prop_dlg->priv->current_page = page;

	gtk_notebook_set_current_page (GTK_NOTEBOOK (prop_dlg->priv->notebook),
				       page);
}
