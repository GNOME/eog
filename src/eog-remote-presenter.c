/* Eye Of Gnome - Remote Presenter
 *
 * Copyright (C) 2006 The Free Software Foundation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "eog-remote-presenter.h"
#include "eog-image.h"
#include "eog-util.h"
#include "eog-thumb-view.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#ifdef HAVE_METADATA
#include "eog-metadata-details.h"
#endif

enum {
        PROP_0,
        PROP_THUMBVIEW,
        PROP_NEXT_ACTION,
        PROP_PREV_ACTION
};

struct _EogRemotePresenterPrivate {
        EogThumbView   *thumbview;

        GtkWidget      *next_button;
        GtkWidget      *previous_button;

        GtkWidget      *thumbnail_image;
        GtkWidget      *name_label;
        GtkWidget      *size_label;
        GtkWidget      *type_label;
        GtkWidget      *bytes_label;
        GtkWidget      *folder_button;
        gchar          *folder_button_uri;
};

G_DEFINE_TYPE_WITH_PRIVATE (EogRemotePresenter, eog_remote_presenter, GTK_TYPE_WINDOW);

static void
parent_file_display_name_query_info_cb (GObject *source_object,
                                        GAsyncResult *res,
                                        gpointer user_data)
{
        EogRemotePresenter *remote_presenter = EOG_REMOTE_PRESENTER (user_data);
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
        gtk_button_set_label (GTK_BUTTON (remote_presenter->priv->folder_button),
                              display_name);
        gtk_widget_set_sensitive (remote_presenter->priv->folder_button, TRUE);

        g_free (display_name);
        g_object_unref (remote_presenter);
}

static void
rp_folder_button_clicked_cb (GtkButton *button, gpointer data)
{
        EogRemotePresenterPrivate *priv = EOG_REMOTE_PRESENTER (data)->priv;
        GtkWindow *window;
        guint32 timestamp;

        if (!priv->folder_button_uri)
                return;
        
        timestamp = gtk_get_current_event_time ();

        window = GTK_WINDOW (data);
        gtk_show_uri_on_window (window, priv->folder_button_uri, timestamp, NULL);
}

static void
eog_remote_presenter_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
        EogRemotePresenter *remote_presenter = EOG_REMOTE_PRESENTER (object);

        switch (prop_id) {
                case PROP_THUMBVIEW:
                        remote_presenter->priv->thumbview = g_value_get_object (value);
                        break;
                case PROP_NEXT_ACTION:
                        gtk_actionable_set_action_name (GTK_ACTIONABLE (remote_presenter->priv->next_button),
                                                        g_value_get_string (value));
                        break;
                case PROP_PREV_ACTION:
                        gtk_actionable_set_action_name (GTK_ACTIONABLE (remote_presenter->priv->previous_button),
                                                        g_value_get_string (value));
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id,
                                                           pspec);
                        break;
        }
}

static void
eog_remote_presenter_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
        EogRemotePresenter *remote_presenter = EOG_REMOTE_PRESENTER (object);

        switch (prop_id) {
                case PROP_THUMBVIEW:
                        g_value_set_object (value, remote_presenter->priv->thumbview);
                        break;
                case PROP_NEXT_ACTION:
                {
                        const gchar* action = gtk_actionable_get_action_name (
                                                      GTK_ACTIONABLE (remote_presenter->priv->next_button));
                        g_value_set_string (value, action);
                        break;
                }
                case PROP_PREV_ACTION:
                {
                        const gchar* action = gtk_actionable_get_action_name (
                                                      GTK_ACTIONABLE (remote_presenter->priv->previous_button));
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
eog_remote_presenter_dispose (GObject *object)
{
        EogRemotePresenter *remote_presenter;
        EogRemotePresenterPrivate *priv;

        g_return_if_fail (object != NULL);
        g_return_if_fail (EOG_IS_REMOTE_PRESENTER (object));

        remote_presenter = EOG_REMOTE_PRESENTER (object);
        priv = remote_presenter->priv;

        if (priv->thumbview) {
                g_object_unref (priv->thumbview);
                priv->thumbview = NULL;
        }

        g_free (priv->folder_button_uri);
        priv->folder_button_uri = NULL;

        G_OBJECT_CLASS (eog_remote_presenter_parent_class)->dispose (object);
}

static void
eog_remote_presenter_class_init (EogRemotePresenterClass *klass)
{
        GObjectClass *g_object_class = (GObjectClass *) klass;

        g_object_class->dispose = eog_remote_presenter_dispose;
        g_object_class->set_property = eog_remote_presenter_set_property;
        g_object_class->get_property = eog_remote_presenter_get_property;

        g_object_class_install_property (g_object_class,
                                         PROP_THUMBVIEW,
                                         g_param_spec_object ("thumbview",
                                                              "Thumbview",
                                                              "Thumbview",
                                                              EOG_TYPE_THUMB_VIEW,
                                                              G_PARAM_READWRITE |
                                                              G_PARAM_CONSTRUCT_ONLY |
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

        gtk_widget_class_set_template_from_resource ((GtkWidgetClass *) klass, "/org/gnome/eog/ui/eog-remote-presenter.ui");

        GtkWidgetClass *wklass = (GtkWidgetClass*) klass;
        gtk_widget_class_bind_template_child_private(wklass,
                                                     EogRemotePresenter,
                                                     previous_button);
        gtk_widget_class_bind_template_child_private(wklass,
                                                     EogRemotePresenter,
                                                     next_button);
        gtk_widget_class_bind_template_child_private(wklass,
                                                     EogRemotePresenter,
                                                     thumbnail_image);
        gtk_widget_class_bind_template_child_private(wklass,
                                                     EogRemotePresenter,
                                                     name_label);
        gtk_widget_class_bind_template_child_private(wklass,
                                                     EogRemotePresenter,
                                                     size_label);
        gtk_widget_class_bind_template_child_private(wklass,
                                                     EogRemotePresenter,
                                                     type_label);
        gtk_widget_class_bind_template_child_private(wklass,
                                                     EogRemotePresenter,
                                                     bytes_label);
        gtk_widget_class_bind_template_child_private(wklass,
                                                     EogRemotePresenter,
                                                     folder_button);

        gtk_widget_class_bind_template_callback(wklass,
                                                rp_folder_button_clicked_cb);
}

static void
eog_remote_presenter_init (EogRemotePresenter *remote_presenter)
{
        EogRemotePresenterPrivate *priv;

        remote_presenter->priv = eog_remote_presenter_get_instance_private (remote_presenter);

        priv = remote_presenter->priv;

        gtk_widget_init_template (GTK_WIDGET (remote_presenter));


        g_signal_connect (remote_presenter,
                          "delete-event",
                          G_CALLBACK (gtk_widget_hide_on_delete),
                          remote_presenter);

        priv->folder_button_uri = NULL;
}

/**
 * eog_remote_presenter_new:
 * @parent: the parent window
 * @thumbview: 
 * @next_image_action: 
 * @previous_image_action: 
 *
 * If %parent implements #GActionMap its actions will be automatically
 * inserted in the "win" namespace.
 *
 * Returns: (transfer full) (type EogRemotePresenter): a new #EogRemotePresenter
 **/
GtkWidget *
eog_remote_presenter_new (GtkWindow    *parent,
                          EogThumbView *thumbview,
                          const gchar  *next_image_action,
                          const gchar  *previous_image_action)
{
        GObject *remote_presenter;

        g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
        g_return_val_if_fail (EOG_IS_THUMB_VIEW (thumbview), NULL);

        remote_presenter = g_object_new (EOG_TYPE_REMOTE_PRESENTER,
                                         "thumbview", thumbview,
                                         "next-action", next_image_action,
                                         "prev-action", previous_image_action,
                                         NULL);

        gtk_window_set_transient_for (GTK_WINDOW (remote_presenter), parent);

        if (G_LIKELY (G_IS_ACTION_GROUP (parent))) {
                gtk_widget_insert_action_group (GTK_WIDGET (remote_presenter),
                                                "win",
                                                G_ACTION_GROUP (parent));
        }

        return GTK_WIDGET (remote_presenter);
}

void
eog_remote_presenter_update (EogRemotePresenter *remote_presenter,
                             EogImage           *image)
{
        gchar *bytes_str;
        gchar *size_str;
        GFile *file, *parent_file;
        GFileInfo *file_info;
        const char *mime_str;
        char *type_str;
        gint width, height;
        goffset bytes;

        g_return_if_fail (EOG_IS_REMOTE_PRESENTER (remote_presenter));

        g_object_set (G_OBJECT (remote_presenter->priv->thumbnail_image),
                      "pixbuf", eog_image_get_thumbnail (image),
                      NULL);

        gtk_label_set_text (GTK_LABEL (remote_presenter->priv->name_label),
                            eog_image_get_caption (image));

        eog_image_get_size (image, &width, &height);

        size_str = eog_util_create_width_height_string(width, height);

        gtk_label_set_text (GTK_LABEL (remote_presenter->priv->size_label), size_str);

        g_free (size_str);

        file = eog_image_get_file (image);
        file_info = g_file_query_info (file,
                                       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","
                                       G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
                                       0, NULL, NULL);
        if (file_info == NULL) {
                type_str = g_strdup (_("Unknown"));
        } else {
                mime_str = eog_util_get_content_type_with_fallback (file_info);
                type_str = g_content_type_get_description (mime_str);
                g_object_unref (file_info);
        }

        gtk_label_set_text (GTK_LABEL (remote_presenter->priv->type_label), type_str);

        bytes = eog_image_get_bytes (image);
        bytes_str = g_format_size (bytes);

        gtk_label_set_text (GTK_LABEL (remote_presenter->priv->bytes_label), bytes_str);

        parent_file = g_file_get_parent (file);
        if (parent_file == NULL) {
                /* file is root directory itself */
                parent_file = g_object_ref (file);
        }

        gtk_widget_set_sensitive (remote_presenter->priv->folder_button, FALSE);
        gtk_button_set_label (GTK_BUTTON (remote_presenter->priv->folder_button), NULL);
        g_free (remote_presenter->priv->folder_button_uri);
        remote_presenter->priv->folder_button_uri = g_file_get_uri (parent_file);

        g_file_query_info_async (parent_file,
                                 G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                 G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_DEFAULT,
                                 NULL,
                                 parent_file_display_name_query_info_cb,
                                 g_object_ref (remote_presenter));

        g_object_unref (parent_file);
        g_free (type_str);
        g_free (bytes_str);
}
