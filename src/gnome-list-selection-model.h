/* GNOME libraries - abstract list selection model
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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

#ifndef GNOME_LIST_SELECTION_MODEL_H
#define GNOME_LIST_SELECTION_MODEL_H

#include <libgnome/gnome-defs.h>
#include "gnome-list-model.h"

BEGIN_GNOME_DECLS



#define GNOME_TYPE_LIST_SELECTION_MODEL            (gnome_list_selection_model_get_type ())
#define GNOME_LIST_SELECTION_MODEL(obj)            (GTK_CHECK_CAST ((obj),		\
						    GNOME_TYPE_LIST_SELECTION_MODEL,	\
						    GnomeListSelectionModel))
#define GNOME_LIST_SELECTION_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass),	\
						    GNOME_TYPE_LIST_SELECTION_MODEL,	\
						    GnomeListSelectionModelClass))
#define GNOME_IS_LIST_SELECTION_MODEL(obj)         (GTK_CHECK_TYPE ((obj),		\
						    GNOME_TYPE_LIST_SELECTION_MODEL))
#define GNOME_IS_LIST_SELECTION_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass),	\
						    GNOME_TYPE_LIST_SELECTION_MODEL))


typedef struct _GnomeListSelectionModel GnomeListSelectionModel;
typedef struct _GnomeListSelectionModelClass GnomeListSelectionModelClass;

struct _GnomeListSelectionModel {
	GnomeListModel list_model;
};

struct _GnomeListSelectionModelClass {
	GnomeListModelClass parent_class;

	/* Query signals */

	gboolean (* is_selected) (GnomeListSelectionModel *model, guint n);
	gint (* get_min_selected) (GnomeListSelectionModel *model);
	gint (* get_max_selected) (GnomeListSelectionModel *model);
};


GtkType gnome_list_selection_model_get_type (void);

gboolean gnome_list_selection_model_is_selected (GnomeListSelectionModel *model, guint n);
gint gnome_list_selection_model_get_min_selected (GnomeListSelectionModel *model);
gint gnome_list_selection_model_get_max_selected (GnomeListSelectionModel *model);



END_GNOME_DECLS

#endif
