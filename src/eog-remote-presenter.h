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

typedef struct _EogRemotePresenter EogRemotePresenter;
typedef struct _EogRemotePresenterClass EogRemotePresenterClass;
typedef struct _EogRemotePresenterPrivate EogRemotePresenterPrivate;

#define EOG_TYPE_REMOTE_PRESENTER            (eog_remote_presenter_get_type ())
#define EOG_REMOTE_PRESENTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), EOG_TYPE_REMOTE_PRESENTER, EogRemotePresenter))
#define EOG_REMOTE_PRESENTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  EOG_TYPE_REMOTE_PRESENTER, EogRemotePresenterClass))
#define EOG_IS_REMOTE_PRESENTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), EOG_TYPE_REMOTE_PRESENTER))
#define EOG_IS_REMOTE_PRESENTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  EOG_TYPE_REMOTE_PRESENTER))
#define EOG_REMOTE_PRESENTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  EOG_TYPE_REMOTE_PRESENTER, EogRemotePresenterClass))
        
struct _EogRemotePresenter {
        GtkWindow window;

        EogRemotePresenterPrivate *priv;
};

struct _EogRemotePresenterClass {
        GtkWindowClass parent_class;
};

GType	    eog_remote_presenter_get_type	(void) G_GNUC_CONST;

GtkWidget  *eog_remote_presenter_new		(GtkWindow               *parent,
                                                 EogThumbView            *thumbview,
                                                 const gchar             *next_image_action,
                                                 const gchar             *previous_image_action);

void	    eog_remote_presenter_update  	(EogRemotePresenter     *prop,
                                                 EogImage                *image);
G_END_DECLS
