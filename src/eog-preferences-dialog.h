/* Eye Of Gnome - EOG Preferences Dialog
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#pragma once

#include "eog-image.h"
#include "eog-thumb-view.h"

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _EogPreferencesDialog EogPreferencesDialog;
typedef struct _EogPreferencesDialogClass EogPreferencesDialogClass;
typedef struct _EogPreferencesDialogPrivate EogPreferencesDialogPrivate;

#define EOG_TYPE_PREFERENCES_DIALOG            (eog_preferences_dialog_get_type ())
#define EOG_PREFERENCES_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), EOG_TYPE_PREFERENCES_DIALOG, EogPreferencesDialog))
#define EOG_PREFERENCES_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  EOG_TYPE_PREFERENCES_DIALOG, EogPreferencesDialogClass))
#define EOG_IS_PREFERENCES_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), EOG_TYPE_PREFERENCES_DIALOG))
#define EOG_IS_PREFERENCES_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  EOG_TYPE_PREFERENCES_DIALOG))
#define EOG_PREFERENCES_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  EOG_TYPE_PREFERENCES_DIALOG, EogPreferencesDialogClass))

struct _EogPreferencesDialog {
	GtkDialog dialog;

	EogPreferencesDialogPrivate *priv;
};

struct _EogPreferencesDialogClass {
	GtkDialogClass parent_class;
};

G_GNUC_INTERNAL
GType	    eog_preferences_dialog_get_type	  (void) G_GNUC_CONST;

G_GNUC_INTERNAL
GtkWidget  *eog_preferences_dialog_get_instance	  (GtkWindow   *parent);

G_END_DECLS
