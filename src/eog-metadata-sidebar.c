/*
 * eog-metadata-sidebar.c
 * This file is part of eog
 *
 * Author: Felix Riemann <friemann@gnome.org>
 *
 * Portions based on code by: Lucas Rocha <lucasr@gnome.org>
 *                            Hubert Figuiere <hub@figuiere.net> (XMP support)
 *
 * Copyright (C) 2011 GNOME Foundation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "eog-image.h"
#include "eog-metadata-sidebar.h"
#include "eog-details-dialog.h"
#include "eog-scroll-view.h"
#include "eog-util.h"
#include "eog-window.h"

#ifdef HAVE_EXIF
#include <libexif/exif-data.h>
#include <libexif/exif-tag.h>
#include "eog-exif-util.h"
#endif

#ifdef HAVE_EXEMPI
#include <exempi/xmp.h>
#include <exempi/xmpconsts.h>
#endif

/* There's no exempi support in the sidebar yet */
#if defined(HAVE_EXIF) || defined(HAVE_EXEMPI)
#define HAVE_METADATA
#endif

enum {
	PROP_0,
	PROP_IMAGE,
	PROP_PARENT_WINDOW
};

struct _EogMetadataSidebarPrivate {
	EogWindow *parent_window;
	EogImage *image;

	gulong image_changed_id;
	gulong thumb_changed_id;

	GtkWidget *size_label;
	GtkWidget *type_label;
	GtkWidget *filesize_label;
	GtkWidget *folder_label;

#ifdef HAVE_EXIF
	GtkWidget *aperture_label;
	GtkWidget *exposure_label;
	GtkWidget *focallen_label;
	GtkWidget *iso_label;
	GtkWidget *metering_label;
	GtkWidget *model_label;
	GtkWidget *date_label;
	GtkWidget *time_label;
#else
	GtkWidget *metadata_grid;
#endif

#ifdef HAVE_METADATA
	GtkWidget *show_details_button;
	GtkWidget *details_dialog;
#endif
};

G_DEFINE_TYPE_WITH_PRIVATE(EogMetadataSidebar, eog_metadata_sidebar, GTK_TYPE_SCROLLED_WINDOW)

static void
parent_file_display_name_query_info_cb (GObject *source_object,
					GAsyncResult *res,
					gpointer user_data)
{
	EogMetadataSidebar *sidebar = EOG_METADATA_SIDEBAR (user_data);
	GFile *parent_file = G_FILE (source_object);
	GFileInfo *file_info;
	gchar *baseuri;
	gchar *display_name;
	gchar *str;

	file_info = g_file_query_info_finish (parent_file, res, NULL);
	if (file_info == NULL) {
		display_name = g_file_get_basename (parent_file);
	} else {
		display_name = g_strdup (
			g_file_info_get_display_name (file_info));
		g_object_unref (file_info);
	}
	baseuri = g_file_get_uri (parent_file);
	str = g_markup_printf_escaped ("<a href=\"%s\">%s</a>",
				       baseuri,
				       display_name);
	gtk_label_set_markup (GTK_LABEL (sidebar->priv->folder_label), str);

	g_free (str);
	g_free (baseuri);
	g_free (display_name);
	g_object_unref (sidebar);
}

static void
eog_metadata_sidebar_update_general_section (EogMetadataSidebar *sidebar)
{
	EogMetadataSidebarPrivate *priv = sidebar->priv;
	EogImage *img = priv->image;
	GFile *file, *parent_file;
	GFileInfo *file_info;
	gchar *str;
	goffset bytes;
	gint width, height;

	if (G_UNLIKELY (img == NULL)) {
		gtk_label_set_text (GTK_LABEL (priv->size_label), NULL);
		gtk_label_set_text (GTK_LABEL (priv->type_label), NULL);
		gtk_label_set_text (GTK_LABEL (priv->filesize_label), NULL);
		gtk_label_set_text (GTK_LABEL (priv->folder_label), NULL);
		return;
	}

	eog_image_get_size (img, &width, &height);
	str = eog_util_create_width_height_string(width, height);
	gtk_label_set_text (GTK_LABEL (priv->size_label), str);
	g_free (str);

	file = eog_image_get_file (img);
	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","
				       G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
				       0, NULL, NULL);
	if (file_info == NULL) {
		str = g_strdup (_("Unknown"));
	} else {
		const gchar *mime_str;

		mime_str = eog_util_get_content_type_with_fallback (file_info);
		str = g_content_type_get_description (mime_str);
		g_object_unref (file_info);
	}
	gtk_label_set_text (GTK_LABEL (priv->type_label), str);
	g_free (str);

	bytes = eog_image_get_bytes (img);
	str = g_format_size (bytes);
	gtk_label_set_text (GTK_LABEL (priv->filesize_label), str);
	g_free (str);

	parent_file = g_file_get_parent (file);
	if (parent_file == NULL) {
		/* file is root directory itself */
		parent_file = g_object_ref (file);
	}
	gtk_label_set_markup (GTK_LABEL (sidebar->priv->folder_label), NULL);
	g_file_query_info_async (parent_file,
				 G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
				 G_FILE_QUERY_INFO_NONE,
				 G_PRIORITY_DEFAULT,
				 NULL,
				 parent_file_display_name_query_info_cb,
				 g_object_ref (sidebar));
	g_object_unref (parent_file);
}

#ifdef HAVE_METADATA
static void
eog_metadata_sidebar_update_metadata_section (EogMetadataSidebar *sidebar)
{
	EogMetadataSidebarPrivate *priv = sidebar->priv;
	EogImage *image = priv->image;
	gboolean has_metadata = FALSE;

#ifdef HAVE_EXIF
	ExifData *exif_data = NULL;

	if (image) {
		exif_data = eog_image_get_exif_info (image);
		has_metadata |= (exif_data != NULL);
	}

	eog_exif_util_set_label_text (GTK_LABEL (priv->aperture_label),
				      exif_data, EXIF_TAG_FNUMBER);
	eog_exif_util_set_label_text (GTK_LABEL (priv->exposure_label),
				      exif_data,
				      EXIF_TAG_EXPOSURE_TIME);
	eog_exif_util_set_focal_length_label_text (
				       GTK_LABEL (priv->focallen_label),
				       exif_data);
	eog_exif_util_set_label_text (GTK_LABEL (priv->iso_label),
				      exif_data,
				      EXIF_TAG_ISO_SPEED_RATINGS);
	eog_exif_util_set_label_text (GTK_LABEL (priv->metering_label),
				      exif_data,
				      EXIF_TAG_METERING_MODE);
	eog_exif_util_set_label_text (GTK_LABEL (priv->model_label),
				      exif_data, EXIF_TAG_MODEL);
	eog_exif_util_format_datetime_label (GTK_LABEL (priv->date_label),
					     exif_data,
					     EXIF_TAG_DATE_TIME_ORIGINAL,
					     _("%a, %d %B %Y"));
	eog_exif_util_format_datetime_label (GTK_LABEL (priv->time_label),
					     exif_data,
					     EXIF_TAG_DATE_TIME_ORIGINAL,
					     _("%X"));

	/* exif_data_unref can handle NULL-values */
	exif_data_unref(exif_data);
#endif /* HAVE_EXIF */

#ifdef HAVE_EXEMPI
	if (image) {
		has_metadata |= eog_image_has_xmp_info (image);
	}
#endif

	gtk_widget_set_visible (priv->show_details_button, has_metadata);

	if (priv->details_dialog != NULL) {
		eog_details_dialog_update (EOG_DETAILS_DIALOG (priv->details_dialog), priv->image);
	}
}
#endif /* HAVE_METADATA */

static void
eog_metadata_sidebar_update (EogMetadataSidebar *sidebar)
{
	g_return_if_fail (EOG_IS_METADATA_SIDEBAR (sidebar));

	eog_metadata_sidebar_update_general_section (sidebar);
#ifdef HAVE_METADATA
	eog_metadata_sidebar_update_metadata_section (sidebar);
#endif
}

static void
_thumbnail_changed_cb (EogImage *image, gpointer user_data)
{
	eog_metadata_sidebar_update (EOG_METADATA_SIDEBAR (user_data));
}

static void
eog_metadata_sidebar_set_image (EogMetadataSidebar *sidebar, EogImage *image)
{
	EogMetadataSidebarPrivate *priv = sidebar->priv;

	if (image == priv->image)
		return;


	if (priv->thumb_changed_id != 0) {
		g_signal_handler_disconnect (priv->image,
					     priv->thumb_changed_id);
		priv->thumb_changed_id = 0;
	}

	if (priv->image)
		g_object_unref (priv->image);

	priv->image = image;

	if (priv->image) {
		g_object_ref (priv->image);
		priv->thumb_changed_id = 
			g_signal_connect (priv->image, "thumbnail-changed",
					  G_CALLBACK (_thumbnail_changed_cb),
					  sidebar);
		eog_metadata_sidebar_update (sidebar);
	}
	
	g_object_notify (G_OBJECT (sidebar), "image");
}

static void
_notify_image_cb (GObject *gobject, GParamSpec *pspec, gpointer user_data)
{
	EogImage *image;

	g_return_if_fail (EOG_IS_METADATA_SIDEBAR (user_data));
	g_return_if_fail (EOG_IS_SCROLL_VIEW (gobject));

	image = eog_scroll_view_get_image (EOG_SCROLL_VIEW (gobject));

	eog_metadata_sidebar_set_image (EOG_METADATA_SIDEBAR (user_data),
					image);

	if (image)
		g_object_unref (image);
}

static void
_folder_label_clicked_cb (GtkLabel *label, const gchar *uri, gpointer user_data)
{
	EogMetadataSidebarPrivate *priv = EOG_METADATA_SIDEBAR(user_data)->priv;
	EogImage *img;
	GtkWidget *toplevel;
	GtkWindow *window;
	GFile *file;

	g_return_if_fail (priv->parent_window != NULL);

	img = eog_window_get_image (priv->parent_window);
	file = eog_image_get_file (img);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (label));
	if (GTK_IS_WINDOW (toplevel))
		window = GTK_WINDOW (toplevel);
	else
		window = NULL;

	eog_util_show_file_in_filemanager (file, window);

	g_object_unref (file);
}

#ifdef HAVE_METADATA
static void
eog_metadata_sidebar_show_details_cb (GtkButton *button, EogMetadataSidebar *sidebar)
{
	EogMetadataSidebarPrivate *priv = EOG_METADATA_SIDEBAR(sidebar)->priv;

	g_return_if_fail (priv->parent_window != NULL);
	
	if (priv->details_dialog == NULL) {
		priv->details_dialog = eog_details_dialog_new (GTK_WINDOW (priv->parent_window));

		eog_details_dialog_update (EOG_DETAILS_DIALOG (priv->details_dialog), priv->image);
	}

	gtk_widget_show (priv->details_dialog);
}
#endif /* HAVE_METADATA */

static void
eog_metadata_sidebar_set_parent_window (EogMetadataSidebar *sidebar,
					EogWindow *window)
{
	EogMetadataSidebarPrivate *priv;
	GtkWidget *view;

	g_return_if_fail (EOG_IS_METADATA_SIDEBAR (sidebar));
	priv = sidebar->priv;
	g_return_if_fail (priv->parent_window == NULL);

	priv->parent_window = g_object_ref (window);
	eog_metadata_sidebar_update (sidebar);
	view = eog_window_get_view (window);
	priv->image_changed_id = g_signal_connect (view, "notify::image",
						  G_CALLBACK (_notify_image_cb),
						  sidebar);

	g_object_notify (G_OBJECT (sidebar), "parent-window");
	
}

static void
eog_metadata_sidebar_init (EogMetadataSidebar *sidebar)
{
	EogMetadataSidebarPrivate *priv;

	priv = sidebar->priv = eog_metadata_sidebar_get_instance_private (sidebar);

	gtk_widget_init_template (GTK_WIDGET (sidebar));

	g_signal_connect (priv->folder_label, "activate-link",
			  G_CALLBACK (_folder_label_clicked_cb), sidebar);

#ifndef HAVE_EXIF
	{
		/* Remove the lower 8 lines as they are empty without libexif*/
		guint i;

		for (i = 11; i > 3; i--)
		{
			gtk_grid_remove_row (GTK_GRID (priv->metadata_grid), i);
		}
	}
#endif /* !HAVE_EXIF */
}

static void
eog_metadata_sidebar_get_property (GObject *object, guint property_id,
				   GValue *value, GParamSpec *pspec)
{
	EogMetadataSidebar *sidebar;

	g_return_if_fail (EOG_IS_METADATA_SIDEBAR (object));

	sidebar = EOG_METADATA_SIDEBAR (object);

	switch (property_id) {
	case PROP_IMAGE:
	{
		g_value_set_object (value, sidebar->priv->image);
		break;
	}
	case PROP_PARENT_WINDOW:
		g_value_set_object (value, sidebar->priv->parent_window);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
eog_metadata_sidebar_set_property (GObject *object, guint property_id,
				   const GValue *value, GParamSpec *pspec)
{
	EogMetadataSidebar *sidebar;

	g_return_if_fail (EOG_IS_METADATA_SIDEBAR (object));

	sidebar = EOG_METADATA_SIDEBAR (object);

	switch (property_id) {
	case PROP_IMAGE:
	{
		break;
	}
	case PROP_PARENT_WINDOW:
	{
		EogWindow *window;

		window = g_value_get_object (value);
		eog_metadata_sidebar_set_parent_window (sidebar, window);
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}

}

static void
eog_metadata_sidebar_dispose (GObject *object)
{
	EogMetadataSidebarPrivate *priv;

	priv = EOG_METADATA_SIDEBAR (object)->priv;

	g_clear_object (&priv->image);

	g_clear_object (&priv->parent_window);

	G_OBJECT_CLASS (eog_metadata_sidebar_parent_class)->dispose (object);
}

static void
eog_metadata_sidebar_class_init (EogMetadataSidebarClass *klass)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	g_obj_class->get_property = eog_metadata_sidebar_get_property;
	g_obj_class->set_property = eog_metadata_sidebar_set_property;
	g_obj_class->dispose = eog_metadata_sidebar_dispose;

	g_object_class_install_property (
		g_obj_class, PROP_PARENT_WINDOW,
		g_param_spec_object ("parent-window", NULL, NULL,
				     EOG_TYPE_WINDOW, G_PARAM_READWRITE
				     | G_PARAM_CONSTRUCT_ONLY
				     | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (
		g_obj_class, PROP_IMAGE,
		g_param_spec_object ("image", NULL, NULL, EOG_TYPE_IMAGE,
				     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
				    );

	gtk_widget_class_set_template_from_resource (widget_class,
						     "/org/gnome/eog/ui/metadata-sidebar.ui");

	gtk_widget_class_bind_template_child_private (widget_class,
						      EogMetadataSidebar,
						      size_label);
	gtk_widget_class_bind_template_child_private (widget_class,
						      EogMetadataSidebar,
						      type_label);
	gtk_widget_class_bind_template_child_private (widget_class,
						      EogMetadataSidebar,
						      filesize_label);
	gtk_widget_class_bind_template_child_private (widget_class,
						      EogMetadataSidebar,
						      folder_label);
#ifdef HAVE_EXIF
	gtk_widget_class_bind_template_child_private (widget_class,
						      EogMetadataSidebar,
						      aperture_label);
	gtk_widget_class_bind_template_child_private (widget_class,
						      EogMetadataSidebar,
						      exposure_label);
	gtk_widget_class_bind_template_child_private (widget_class,
						      EogMetadataSidebar,
						      focallen_label);
	gtk_widget_class_bind_template_child_private (widget_class,
						      EogMetadataSidebar,
						      iso_label);
	gtk_widget_class_bind_template_child_private (widget_class,
						      EogMetadataSidebar,
						      metering_label);
	gtk_widget_class_bind_template_child_private (widget_class,
						      EogMetadataSidebar,
						      model_label);
	gtk_widget_class_bind_template_child_private (widget_class,
						      EogMetadataSidebar,
						      date_label);
	gtk_widget_class_bind_template_child_private (widget_class,
						      EogMetadataSidebar,
						      time_label);
#else
	gtk_widget_class_bind_template_child_private (widget_class,
						      EogMetadataSidebar,
						      metadata_grid);
#endif /* HAVE_EXIF */

#ifdef HAVE_METADATA
	gtk_widget_class_bind_template_child_private (widget_class,
						      EogMetadataSidebar,
						      show_details_button);

	gtk_widget_class_bind_template_callback(widget_class,
						eog_metadata_sidebar_show_details_cb);
#endif /* HAVE_METADATA */
}


GtkWidget*
eog_metadata_sidebar_new (EogWindow *window)
{
	return gtk_widget_new (EOG_TYPE_METADATA_SIDEBAR,
			       "hadjustment", NULL,
			       "vadjustment", NULL,
			       "hscrollbar-policy", GTK_POLICY_NEVER,
			       "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
			       "parent-window", window,
			       NULL);
}
