/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8 c-style: "K&R" -*- */
/* Eye of Gnome image viewer - EoG image list model implementation
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Author: Jens Finke <jens@gnome.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EOG_IMAGE_LIST_MODEL_H
#define EOG_IMAGE_LIST_MODEL_H

#include <libgnome/gnome-defs.h>
#include <gconf/gconf-client.h>
#include "gnome-icon-list-model.h"

BEGIN_GNOME_DECLS



#define TYPE_EOG_IMAGE_LIST_MODEL            (eog_image_list_model_get_type ())
#define EOG_IMAGE_LIST_MODEL(obj)            (GTK_CHECK_CAST ((obj), TYPE_EOG_IMAGE_LIST_MODEL, EogImageListModel))
#define EOG_IMAGE_LIST_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_EOG_IMAGE_LIST_MODEL, EogImageListModelClass))
#define IS_EOG_IMAGE_LIST_MODEL(obj)         (GTK_CHECK_TYPE ((obj), TYPE_EOG_IMAGE_LIST_MODEL))
#define IS_EOG_IMAGE_LIST_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_EOG_IMAGE_LIST_MODEL))

typedef struct _EogImageListModel EogImageListModel;
typedef struct _EogImageListModelClass EogImageListModelClass;

typedef struct _EogImageListModelPrivate EogImageListModelPrivate;

struct _EogImageListModel {
	GnomeIconListModel parent_obj;

	/* Private data */
	EogImageListModelPrivate *priv;
};

struct _EogImageListModelClass {
	GnomeIconListModelClass parent_class;
};

GtkType eog_image_list_model_get_type(void);

GtkObject* eog_image_list_model_new(void);

/* add single Image* to the model */
void eog_image_list_model_add_image(EogImageListModel *model, CImage *image);

/* add list of Image* objets */
void eog_image_list_model_add_images(EogImageListModel *model, GList *images);

/* checks if any images are available */
gboolean eog_image_list_model_has_images (EogImageListModel *model);

CImage *eog_image_list_model_next_image_to_load (EogImageListModel *model);

END_GNOME_DECLS

#endif /* EOG_IMAGE_LIST_MODEL_H */
