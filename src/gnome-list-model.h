/* GNOME libraries - abstract list model
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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

#ifndef GNOME_LIST_MODEL_H
#define GNOME_LIST_MODEL_H

#include <libgnome/gnome-defs.h>
#include <gtk/gtkobject.h>

BEGIN_GNOME_DECLS



#define GNOME_TYPE_LIST_MODEL            (gnome_list_model_get_type ())
#define GNOME_LIST_MODEL(obj)            (GTK_CHECK_CAST ((obj),			\
					  GNOME_TYPE_LIST_MODEL, GnomeListModel))
#define GNOME_LIST_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass),		\
					  GNOME_TYPE_LIST_MODEL, GnomeListModelClass))
#define GNOME_IS_LIST_MODEL(obj)         (GTK_CHECK_TYPE ((obj),			\
					  GNOME_TYPE_LIST_MODEL))
#define GNOME_IS_LIST_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass),		\
					  GNOME_TYPE_LIST_MODEL))


typedef struct _GnomeListModel GnomeListModel;
typedef struct _GnomeListModelClass GnomeListModelClass;

struct _GnomeListModel {
	GtkObject object;
};

struct _GnomeListModelClass {
	GtkObjectClass parent_class;

	/* Query signals */

	guint (* get_length) (GnomeListModel *model);

	/* Notification signals */

	void (* interval_changed) (GnomeListModel *model, guint start, guint length);
	void (* interval_added) (GnomeListModel *model, guint start, guint length);
	void (* interval_removed) (GnomeListModel *model, guint start, guint length);
};


GtkType gnome_list_model_get_type (void);

guint gnome_list_model_get_length (GnomeListModel *model);



END_GNOME_DECLS

#endif
