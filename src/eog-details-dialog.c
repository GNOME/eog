/* Eye Of Gnome - Details Dialog
 *
 * Copyright (C) 2021 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *         Hubert Figuiere <hub@figuiere.net> (XMP support)
 *         Peter Eisenmann <p3732@getgoogleoff.me> (from eog-properties-dialog)
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

#include "eog-details-dialog.h"

#ifdef HAVE_METADATA

#include "eog-image.h"
#include "eog-util.h"

#ifdef HAVE_EXIF
#include "eog-exif-util.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#ifdef HAVE_EXEMPI
#include <exempi/xmp.h>
#include <exempi/xmpconsts.h>
#endif

#include "eog-metadata-details.h"

struct _EogDetailsDialogPrivate {
    GtkStack       *details_stack;

    GtkWidget      *metadata_details;
    GtkWidget      *metadata_details_box;
};

G_DEFINE_TYPE_WITH_PRIVATE (EogDetailsDialog, eog_details_dialog, GTK_TYPE_WINDOW);

#ifdef HAVE_EXIF
static gboolean
eog_details_dialog_update_exif (EogDetailsDialog *details_dialog,
                                EogImage         *image)
{
    ExifData *exif_data;

    if (eog_image_has_data (image, EOG_IMAGE_DATA_EXIF)) {
        exif_data = (ExifData *) eog_image_get_exif_info (image);

        eog_metadata_details_update (EOG_METADATA_DETAILS (details_dialog->priv->metadata_details),
                    exif_data);

        /* exif_data_unref can handle NULL-values */
        exif_data_unref(exif_data);

        return TRUE;
    } else {
        return FALSE;
    }
}
#endif

#ifdef HAVE_EXEMPI
static gboolean
eog_details_dialog_update_exempi (EogDetailsDialog *details_dialog,
                                  EogImage         *image)
{
    XmpPtr xmp_data;

    if (eog_image_has_data (image, EOG_IMAGE_DATA_XMP)) {
        xmp_data = (XmpPtr) eog_image_get_xmp_info (image);

        if (xmp_data != NULL) {
            eog_metadata_details_xmp_update (EOG_METADATA_DETAILS (details_dialog->priv->metadata_details), xmp_data);

            xmp_free (xmp_data);
        }

        return TRUE;
    } else {
        return FALSE;
    }
}
#endif

static void
eog_details_dialog_class_init (EogDetailsDialogClass *klass)
{
    gtk_widget_class_set_template_from_resource ((GtkWidgetClass *) klass, "/org/gnome/eog/ui/eog-details-dialog.ui");

    GtkWidgetClass *wklass = (GtkWidgetClass*) klass;
    gtk_widget_class_bind_template_child_private(wklass,
                             EogDetailsDialog,
                             details_stack);
    gtk_widget_class_bind_template_child_private(wklass,
                             EogDetailsDialog,
                             metadata_details_box);
}

static void
eog_details_dialog_init (EogDetailsDialog *details_dialog)
{
    EogDetailsDialogPrivate *priv;

    priv = eog_details_dialog_get_instance_private (details_dialog);

    details_dialog->priv = priv;

    gtk_widget_init_template (GTK_WIDGET (details_dialog));

    g_signal_connect (details_dialog,
              "delete-event",
              G_CALLBACK (gtk_widget_hide_on_delete),
              details_dialog);

    gtk_stack_set_visible_child_name(priv->details_stack, "no_details");

	priv->metadata_details = eog_metadata_details_new ();
	gtk_widget_set_vexpand (priv->metadata_details, TRUE);

	gtk_container_add (GTK_CONTAINER (priv->metadata_details_box), priv->metadata_details);
	gtk_widget_show_all (priv->metadata_details_box);
}

/**
 * eog_details_dialog_new:
 * @parent: the dialog's parent window
 *
 * If %parent implements #GActionMap its actions will be automatically
 * inserted in the "win" namespace.
 *
 * Returns: (transfer full) (type EogDetailsDialog): a new #EogDetailsDialog
 **/
GtkWidget *
eog_details_dialog_new (GtkWindow *parent)
{
    GObject *details_dialog;

    g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);

    details_dialog = g_object_new (EOG_TYPE_DETAILS_DIALOG, NULL);

    gtk_window_set_transient_for (GTK_WINDOW (details_dialog), parent);

    if (G_LIKELY (G_IS_ACTION_GROUP (parent))) {
        gtk_widget_insert_action_group (GTK_WIDGET (details_dialog),
                                        "win",
                                        G_ACTION_GROUP (parent));
    }

    return GTK_WIDGET (details_dialog);
}

void
eog_details_dialog_update (EogDetailsDialog *details_dialog,
                           EogImage         *image)
{
    g_return_if_fail (EOG_IS_DETAILS_DIALOG (details_dialog));

    if (FALSE
#ifdef HAVE_EXIF
        | eog_details_dialog_update_exif(details_dialog, image)
#endif
#ifdef HAVE_EXEMPI
        | eog_details_dialog_update_exempi(details_dialog, image)
#endif
        ) {
        gtk_stack_set_visible_child_name(details_dialog->priv->details_stack, "show_details");
    } else {
        gtk_stack_set_visible_child_name(details_dialog->priv->details_stack, "no_details");
    }
}

#endif // HAVE_METADATA