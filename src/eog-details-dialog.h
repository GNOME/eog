/* Eye Of Gnome - Details Dialog
 *
 * Copyright (C) 2021 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_EXIF) || defined(HAVE_EXEMPI)
#define HAVE_METADATA

#include "eog-image.h"

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _EogDetailsDialog EogDetailsDialog;
typedef struct _EogDetailsDialogClass EogDetailsDialogClass;
typedef struct _EogDetailsDialogPrivate EogDetailsDialogPrivate;

#define EOG_TYPE_DETAILS_DIALOG            (eog_details_dialog_get_type ())
#define EOG_DETAILS_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), EOG_TYPE_DETAILS_DIALOG, EogDetailsDialog))
#define EOG_DETAILS_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  EOG_TYPE_DETAILS_DIALOG, EogDetailsDialogClass))
#define EOG_IS_DETAILS_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), EOG_TYPE_DETAILS_DIALOG))
#define EOG_IS_DETAILS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  EOG_TYPE_DETAILS_DIALOG))
#define EOG_DETAILS_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  EOG_TYPE_DETAILS_DIALOG, EogDetailsDialogClass))

struct _EogDetailsDialog {
    GtkWindow window;

    EogDetailsDialogPrivate *priv;
};

struct _EogDetailsDialogClass {
    GtkWindowClass parent_class;
};

GType	    eog_details_dialog_get_type	(void) G_GNUC_CONST;

GtkWidget  *eog_details_dialog_new		(GtkWindow            *parent);

void	    eog_details_dialog_update  	(EogDetailsDialog     *details_dialog,
                                         EogImage             *image);

G_END_DECLS

#endif /* defined(HAVE_EXIF) || defined(HAVE_EXEMPI) */
