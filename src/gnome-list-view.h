/* GNOME libraries - abstract list view
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

#ifndef GNOME_LIST_VIEW_H
#define GNOME_LIST_VIEW_H

#include <libgnome/gnome-defs.h>
#include "gnome-list-item-factory.h"
#include "gnome-list-model.h"
#include "gnome-list-selection-model.h"

BEGIN_GNOME_DECLS



#define GNOME_TYPE_LIST_VIEW            (gnome_list_view_get_type ())
#define GNOME_LIST_VIEW(obj)            (GTK_CHECK_CAST ((obj),		\
					 GNOME_TYPE_LIST_VIEW, GnomeListView))
#define GNOME_LIST_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass),	\
					 GNOME_TYPE_LIST_VIEW, GnomeListViewClass))
#define GNOME_IS_LIST_VIEW(obj)         (GTK_CHECK_TYPE ((obj),		\
					 GNOME_TYPE_LIST_VIEW))
#define GNOME_IS_LIST_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass),	\
					 GNOME_TYPE_LIST_VIEW))


typedef struct _GnomeListView GnomeListView;
typedef struct _GnomeListViewClass GnomeListViewClass;

struct _GnomeListView {
	GtkContainer container;

	/* Private data */
	gpointer priv;
};

struct _GnomeListViewClass {
	GtkContainerClass parent_class;

	/* Notification signals */

	void (* model_set) (GnomeListView *view, GnomeListModel *old_model);
	void (* selection_model_set) (GnomeListView *view, GnomeListSelectionModel *old_sel_model);
	void (* list_item_factory_set) (GnomeListView *view, GnomeListItemFactory *old_factory);
};


GtkType gnome_list_view_get_type (void);

void gnome_list_view_set_model (GnomeListView *view, GnomeListModel *model);
GnomeListModel *gnome_list_view_get_model (GnomeListView *view);

void gnome_list_view_set_selection_model (GnomeListView *view, GnomeListSelectionModel *sel_model);
GnomeListSelectionModel *gnome_list_view_get_selection_model (GnomeListView *view);

void gnome_list_view_set_list_item_factory (GnomeListView *view, GnomeListItemFactory *factory);
GnomeListItemFactory *gnome_list_view_get_list_item_factory (GnomeListView *view);



END_GNOME_DECLS

#endif
