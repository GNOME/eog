/* GNOME libraries - abstract icon list model
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

#ifndef GNOME_ICON_LIST_MODEL_H
#define GNOME_ICON_LIST_MODEL_H

#include <libgnome/gnome-defs.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "gnome-list-model.h"

BEGIN_GNOME_DECLS



#define GNOME_TYPE_ICON_LIST_MODEL            (gnome_icon_list_model_get_type ())
#define GNOME_ICON_LIST_MODEL(obj)            (GTK_CHECK_CAST ((obj),			\
					       GNOME_TYPE_ICON_LIST_MODEL, GnomeIconListModel))
#define GNOME_ICON_LIST_MODEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass),		\
					       GNOME_TYPE_ICON_LIST_MODEL, GnomeIconListModelClass))
#define GNOME_IS_ICON_LIST_MODEL(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_ICON_LIST_MODEL))
#define GNOME_IS_ICON_LIST_MODEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass),		\
					       GNOME_TYPE_ICON_LIST_MODEL))


typedef struct _GnomeIconListModel GnomeIconListModel;
typedef struct _GnomeIconListModelClass GnomeIconListModelClass;

struct _GnomeIconListModel {
	GnomeListModel list_model;
};

struct _GnomeIconListModelClass {
	GnomeListModelClass parent_class;

	/* Query signals */

	void (* get_icon) (GnomeIconListModel *model, guint n,
			   GdkPixBuf **pixbuf, const char **caption);
};


GtkType gnome_icon_list_model_get_type (void);

void gnome_icon_list_model_get_icon (GnomeIconListModel *model, guint n,
				     GdkPixBuf **pixbuf, const char **caption);



END_GNOME_DECLS

#endif
