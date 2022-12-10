/* Eye Of Gnome - Error Message Area
 *
 * Copyright (C) 2007 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on gedit code (gedit/gedit-message-area.h) by:
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eog-error-message-area.h"
#include "eog-image.h"
#include "eog-util.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#ifndef __APPLE__
#include <gio/gdesktopappinfo.h>
#endif

/* gboolean <-> gpointer conversion macros taken from gedit */
#ifndef GBOOLEAN_TO_POINTER
#define GBOOLEAN_TO_POINTER(i) (GINT_TO_POINTER ((i) ? 2 : 1))
#endif
#ifndef GPOINTER_TO_BOOLEAN
#define GPOINTER_TO_BOOLEAN(i) ((gboolean) ((GPOINTER_TO_INT (i) == 2) ? TRUE : FALSE))
#endif

typedef enum {
	EOG_ERROR_MESSAGE_AREA_NO_BUTTONS    = 0,
	EOG_ERROR_MESSAGE_AREA_CANCEL_BUTTON = 1 << 0,
	EOG_ERROR_MESSAGE_AREA_RELOAD_BUTTON = 1 << 1,
	EOG_ERROR_MESSAGE_AREA_SAVEAS_BUTTON = 1 << 2,
	EOG_ERROR_MESSAGE_AREA_OPEN_WITH_EVINCE_BUTTON = 1 << 3
} EogErrorMessageAreaButtons;

static void
set_message_area_text_and_icon (GtkInfoBar   *message_area,
				const gchar  *icon_name,
				const gchar  *primary_text,
				const gchar  *secondary_text)
{
	GtkWidget *hbox_content;
	GtkWidget *image;
	GtkWidget *vbox;
	gchar *primary_markup;
	gchar *secondary_markup;
	GtkWidget *primary_label;
	GtkWidget *secondary_label;

	hbox_content = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox_content);

	image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_DIALOG);
	gtk_widget_show (image);
	gtk_box_pack_start (GTK_BOX (hbox_content), image, FALSE, FALSE, 0);
	gtk_widget_set_valign (image, GTK_ALIGN_START);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (hbox_content), vbox, TRUE, TRUE, 0);

	primary_markup = g_markup_printf_escaped ("<b>%s</b>", primary_text);
	primary_label = gtk_label_new (primary_markup);
	g_free (primary_markup);

	gtk_widget_show (primary_label);

	gtk_box_pack_start (GTK_BOX (vbox), primary_label, TRUE, TRUE, 0);
	gtk_label_set_use_markup (GTK_LABEL (primary_label), TRUE);
	gtk_label_set_line_wrap (GTK_LABEL (primary_label), FALSE);
	gtk_widget_set_halign (primary_label, GTK_ALIGN_START);

	gtk_widget_set_can_focus (primary_label, TRUE);

	gtk_label_set_selectable (GTK_LABEL (primary_label), TRUE);

  	if (secondary_text != NULL) {
		secondary_markup = g_markup_printf_escaped ("<small>%s</small>",
						            secondary_text);
		secondary_label = gtk_label_new (secondary_markup);
		g_free (secondary_markup);

		gtk_widget_show (secondary_label);

		gtk_box_pack_start (GTK_BOX (vbox), secondary_label, TRUE, TRUE, 0);

		gtk_widget_set_can_focus (secondary_label, TRUE);

		gtk_label_set_use_markup (GTK_LABEL (secondary_label), TRUE);
		gtk_label_set_line_wrap (GTK_LABEL (secondary_label), TRUE);
		gtk_label_set_selectable (GTK_LABEL (secondary_label), TRUE);
		gtk_widget_set_halign (secondary_label, GTK_ALIGN_START);
	}

	gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (GTK_INFO_BAR (message_area))), hbox_content, TRUE, TRUE, 0);
}

static void
add_message_area_buttons (GtkWidget *message_area,
			  EogErrorMessageAreaButtons buttons)
{
	if (buttons & EOG_ERROR_MESSAGE_AREA_CANCEL_BUTTON)
		gtk_info_bar_add_button (GTK_INFO_BAR (message_area),
					 _("_Cancel"),
					 EOG_ERROR_MESSAGE_AREA_RESPONSE_CANCEL);

	if (buttons & EOG_ERROR_MESSAGE_AREA_RELOAD_BUTTON)
		gtk_info_bar_add_button (GTK_INFO_BAR (message_area),
					 _("_Reload"),
					 EOG_ERROR_MESSAGE_AREA_RESPONSE_RELOAD);

	if (buttons & EOG_ERROR_MESSAGE_AREA_SAVEAS_BUTTON)
		gtk_info_bar_add_button (GTK_INFO_BAR (message_area),
					 _("Save _As…"),
					 EOG_ERROR_MESSAGE_AREA_RESPONSE_SAVEAS);
	if (buttons & EOG_ERROR_MESSAGE_AREA_OPEN_WITH_EVINCE_BUTTON)
		gtk_info_bar_add_button (GTK_INFO_BAR (message_area),
					 _("Open with _Document Viewer"),
					 EOG_ERROR_MESSAGE_AREA_RESPONSE_OPEN_WITH_EVINCE);
}

static GtkWidget *
create_error_message_area (const gchar                 *primary_text,
			   const gchar                 *secondary_text,
			   EogErrorMessageAreaButtons   buttons)
{
	GtkWidget *message_area;

	/* create a new message area */
	message_area = gtk_info_bar_new ();

	/* add requested buttons to the message area */
	add_message_area_buttons (message_area, buttons);

	/* set message type */
	gtk_info_bar_set_message_type (GTK_INFO_BAR (message_area),
				       GTK_MESSAGE_ERROR);

	/* set text and icon */
	set_message_area_text_and_icon (GTK_INFO_BAR (message_area),
					"dialog-error",
					primary_text,
					secondary_text);

	return message_area;
}

static GtkWidget *
create_info_message_area (const gchar                 *primary_text,
			  const gchar                 *secondary_text,
			  EogErrorMessageAreaButtons   buttons)
{
	GtkWidget *message_area;

	/* create a new message area */
	message_area = gtk_info_bar_new ();

	/* add requested buttons to the message area */
	add_message_area_buttons (message_area, buttons);

	/* set message type */
	gtk_info_bar_set_message_type (GTK_INFO_BAR (message_area),
				       GTK_MESSAGE_INFO);

	/* set text and icon */
	set_message_area_text_and_icon (GTK_INFO_BAR (message_area),
					"dialog-information",
					primary_text,
					secondary_text);

	return message_area;
}

/**
 * eog_image_load_error_message_area_new: (skip):
 * @caption:
 * @error:
 *
 *
 *
 * Returns: (transfer full): a new #GtkInfoBar
 **/
GtkWidget *
eog_image_load_error_message_area_new (const gchar  *caption,
				       const GError *error)
{
	GtkWidget *message_area;
	gchar *error_message = NULL;
	gchar *message_details = NULL;
	gchar *pango_escaped_caption = NULL;

	g_return_val_if_fail (caption != NULL, NULL);
	g_return_val_if_fail (error != NULL, NULL);

	/* Escape the caption string with respect to pango markup.
	   This is necessary because otherwise characters like "&" will
	   be interpreted as the beginning of a pango entity inside
	   the message area GtkLabel. */
	pango_escaped_caption = g_markup_escape_text (caption, -1);
	error_message = g_strdup_printf (_("Could not load image “%s”."),
					 pango_escaped_caption);

	message_details = eog_util_make_valid_utf8 (error->message);

	message_area = create_error_message_area (error_message,
						  message_details,
						  EOG_ERROR_MESSAGE_AREA_CANCEL_BUTTON);

	g_free (pango_escaped_caption);
	g_free (error_message);
	g_free (message_details);

	return message_area;
}

/**
 * eog_image_save_error_message_area_new: (skip):
 * @caption:
 * @error:
 *
 *
 *
 * Returns: (transfer full): a new #GtkInfoBar
 **/
GtkWidget *
eog_image_save_error_message_area_new (const gchar  *caption,
				       const GError *error)
{
	GtkWidget *message_area;
	gchar *error_message = NULL;
	gchar *message_details = NULL;
	gchar *pango_escaped_caption = NULL;

	g_return_val_if_fail (caption != NULL, NULL);
	g_return_val_if_fail (error != NULL, NULL);

	/* Escape the caption string with respect to pango markup.
	   This is necessary because otherwise characters like "&" will
	   be interpreted as the beginning of a pango entity inside
	   the message area GtkLabel. */
	pango_escaped_caption = g_markup_escape_text (caption, -1);
	error_message = g_strdup_printf (_("Could not save image “%s”."),
					 pango_escaped_caption);

	message_details = eog_util_make_valid_utf8 (error->message);

	message_area = create_error_message_area (error_message,
						  message_details,
						  EOG_ERROR_MESSAGE_AREA_CANCEL_BUTTON |
						  EOG_ERROR_MESSAGE_AREA_SAVEAS_BUTTON);

	g_free (pango_escaped_caption);
	g_free (error_message);
	g_free (message_details);

	return message_area;
}

/**
 * eog_no_images_error_message_area_new: (skip):
 * @file:
 *
 *
 *
 * Returns: (transfer full): a new #GtkInfoBar
 **/
GtkWidget *
eog_no_images_error_message_area_new (GFile *file)
{
	GtkWidget *message_area;
	gchar *error_message = NULL;

	if (file != NULL) {
		gchar *uri_str, *unescaped_str, *pango_escaped_str;

		uri_str = g_file_get_uri (file);
		/* Unescape URI with respect to rules defined in RFC 3986. */
		unescaped_str = g_uri_unescape_string (uri_str, NULL);

		/* Escape the URI string with respect to pango markup.
		   This is necessary because the URI string can contain
		   for example "&" which will otherwise be interpreted
		   as a pango markup entity when inserted into a GtkLabel. */
		pango_escaped_str = g_markup_escape_text (unescaped_str, -1);
		error_message = g_strdup_printf (_("No images found in “%s”."),
						 pango_escaped_str);

		g_free (pango_escaped_str);
		g_free (uri_str);
		g_free (unescaped_str);
	} else {
		error_message = g_strdup (_("The given locations contain no images."));
	}

	message_area = create_error_message_area (error_message,
						  NULL,
						  EOG_ERROR_MESSAGE_AREA_NO_BUTTONS);

	g_free (error_message);

	return message_area;
}

static gpointer
_check_evince_availability(gpointer data)
{
	gboolean result = FALSE;
#ifndef __APPLE__
	GDesktopAppInfo *app_info;

	app_info = g_desktop_app_info_new ("org.gnome.Evince.desktop");
	if (app_info) {
		result = TRUE;
		g_object_unref (app_info);
	}
#endif
	return GBOOLEAN_TO_POINTER(result);
}

GtkWidget *
eog_multipage_error_message_area_new(void)
{
	static GOnce evince_is_available = G_ONCE_INIT;
	EogErrorMessageAreaButtons buttons = EOG_ERROR_MESSAGE_AREA_NO_BUTTONS;
	GtkWidget *message_area;
	const gchar *info_message;

	g_once (&evince_is_available, _check_evince_availability, NULL);

	if (GPOINTER_TO_BOOLEAN (evince_is_available.retval))
	{
		buttons = EOG_ERROR_MESSAGE_AREA_OPEN_WITH_EVINCE_BUTTON;
		info_message = N_("This image contains multiple pages. "
				  "Image Viewer displays only the first page.\n"
				  "Do you want to open the image with the Document Viewer to see all pages?");
	} else {
		buttons = EOG_ERROR_MESSAGE_AREA_NO_BUTTONS;
		info_message = N_("This image contains multiple pages. "
				  "Image Viewer displays only the first page.\n"
				  "You may want to install the Document Viewer to see all pages.");
	}

	message_area = create_info_message_area (gettext (info_message),
						 NULL,
						 buttons);
	gtk_info_bar_set_show_close_button (GTK_INFO_BAR (message_area),
					    TRUE);

	return message_area;
}
