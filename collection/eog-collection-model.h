/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8 c-style: "K&R" -*- */
/* Eye of Gnome image viewer - EoG collection model
 *
 * Copyright (C) 2001 The Free Software Foundation
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

#ifndef EOG_COLLECTION_MODEL_H
#define EOG_COLLECTION_MODEL_H

#include <bonobo/bonobo-storage.h>
#include <gtk/gtkobject.h>
#include "cimage.h"

BEGIN_GNOME_DECLS

#define EOG_COLLECTION_MODEL_TYPE            (eog_collection_model_get_type ())
#define EOG_COLLECTION_MODEL(obj)            (GTK_CHECK_CAST ((obj), EOG_COLLECTION_MODEL_TYPE, EogCollectionModel))
#define EOG_COLLECTION_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EOG_COLLECTION_MODEL_TYPE, EogCollectionModelClass))
#define EOG_IS_COLLECTION_MODEL(obj)         (GTK_CHECK_TYPE ((obj), EOG_COLLECTION_MODEL_TYPE))
#define EOG_IS_COLLECTION_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), EOG_COLLECTION_MODEL_TYPE))


typedef struct _EogCollectionModel EogCollectionModel;
typedef struct _EogCollectionModelClass EogCollectionModelClass;
typedef struct _EogCollectionModelPrivate EogCollectionModelPrivate;

#define EOG_MODEL_ID_RANGE -1 /* Random number to indicate that the
                                 following two ids are building a range. */
#define EOG_MODEL_ID_END   -2 /* Random number to indicate that the list
                                 will include all ids until the end. */
#define EOG_MODEL_ID_NONE  -3 /* Random number, same as NULL otherwise.*/

struct _EogCollectionModel {
	GtkObject parent_object;
	
	EogCollectionModelPrivate *priv;
};

struct _EogCollectionModelClass {
	GtkObjectClass parent_class;

	/* Notification signals */
	void (* interval_changed) (EogCollectionModel *model, GList *id_list);
	void (* interval_added)   (EogCollectionModel *model, GList *id_list);
	void (* interval_removed) (EogCollectionModel *model, GList *id_list);

	void (* selection_changed) (EogCollectionModel *model);
};


GtkType 
eog_collection_model_get_type               (void);

EogCollectionModel*
eog_collection_model_new                    (void);

void
eog_collection_model_construct (EogCollectionModel *model);

void
eog_collection_model_set_uri            (EogCollectionModel *model, 
                                         const gchar *uri);

gint
eog_collection_model_get_length (EogCollectionModel *model);


CImage*
eog_collection_model_get_image              (EogCollectionModel *model,
                                             guint unique_id);

gchar*
eog_collection_model_get_uri                (EogCollectionModel *model,
                                             guint unique_id);

GList*
eog_collection_model_get_images             (EogCollectionModel *model,
                                             guint min_id, guint len);

void
eog_collection_model_toggle_select_status   (EogCollectionModel *model,
                                             guint id);

void 
eog_collection_model_set_select_status_all  (EogCollectionModel *model, 
                                             gboolean status);

END_GNOME_DECLS

#endif /* EOG_COLLECTION_MODEL_H */
