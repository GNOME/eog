/* GNOME libraries - abstract position list model
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef GNOME_POSITION_LIST_MODEL_H
#define GNOME_POSITION_LIST_MODEL_H

#include "gnome-list-model.h"

G_BEGIN_DECLS



#define GNOME_TYPE_POSITION_LIST_MODEL            (gnome_position_list_model_get_type ())
#define GNOME_POSITION_LIST_MODEL(obj)            (GTK_CHECK_CAST ((obj),			\
						   GNOME_TYPE_POSITION_LIST_MODEL,		\
						   GnomePositionListModel))
#define GNOME_POSITION_LIST_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass),		\
						   GNOME_TYPE_POSITION_LIST_MODEL,		\
						   GnomePositionListModelClass))
#define GNOME_IS_POSITION_LIST_MODEL(obj)         (GTK_CHECK_TYPE ((obj),			\
						   GNOME_TYPE_POSITION_LIST_MODEL))
#define GNOME_IS_POSITION_LIST_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass),		\
						   GNOME_TYPE_POSITION_LIST_MODEL))


typedef struct _GnomePositionListModel GnomePositionListModel;
typedef struct _GnomePositionListModelClass GnomePositionListModelClass;

struct _GnomePositionListModel {
	GnomeListModel list_model;
};

struct _GnomePositionListModelClass {
	GnomeListModelClass parent_class;

	/* Mutation signals */

	void (* set_position) (GnomePositionListModel *model, guint n, gint x, gint y);

	/* Query signals */

	gboolean (* get_position) (GnomePositionListModel *model, guint n, gint *x, gint *y);
};


GtkType gnome_position_list_model_get_type (void);

void gnome_position_list_model_set_position (GnomePositionListModel *model, guint n,
					     gint x, gint y);

gboolean gnome_position_list_model_get_position (GnomePositionListModel *model, guint n,
						 gint *x, gint *y);



G_END_DECLS

#endif
