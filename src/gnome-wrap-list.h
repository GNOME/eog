/* GNOME libraries - abstract wrapped list view
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

#ifndef GNOME_WRAP_LIST_H
#define GNOME_WRAP_LIST_H

#include "gnome-list-view.h"
#include "gnome-position-list-model.h"

G_BEGIN_DECLS



#define GNOME_TYPE_WRAP_LIST            (gnome_wrap_list_get_type ())
#define GNOME_WRAP_LIST(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_WRAP_LIST, GnomeWrapList))
#define GNOME_WRAP_LIST_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_WRAP_LIST,	\
					 GnomeWrapListClass))
#define GNOME_IS_WRAP_LIST(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_WRAP_LIST))
#define GNOME_IS_WRAP_LIST_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_WRAP_LIST))


typedef struct _GnomeWrapList GnomeWrapList;
typedef struct _GnomeWrapListClass GnomeWrapListClass;

struct _GnomeWrapList {
	GnomeListView view;

	/* Private data */
	gpointer priv;
};

struct _GnomeWrapListClass {
	GnomeListViewClass parent_class;

	/* GTK+ scrolling interface */
	void (* set_scroll_adjustments) (GnomeWrapList *wlist,
					 GtkAdjustment *hadj, GtkAdjustment *vadj);
};


/* Layout modes */
typedef enum {
	GNOME_WRAP_LIST_ROW_MAJOR,
	GNOME_WRAP_LIST_COL_MAJOR,
	GNOME_WRAP_LIST_MANUAL
} GnomeWrapListMode;


GtkType gnome_wrap_list_get_type (void);

void gnome_wrap_list_set_mode (GnomeWrapList *wlist, GnomeWrapListMode mode);
GnomeWrapListMode gnome_wrap_list_get_mode (GnomeWrapList *wlist);

void gnome_wrap_list_set_position_model (GnomeWrapList *wlist, GnomePositionListModel *pos_model);
GnomePositionListModel *gnome_wrap_list_get_position_model (GnomeWrapList *wlist);

void gnome_wrap_list_set_item_size (GnomeWrapList *wlist, int width, int height);
void gnome_wrap_list_get_item_size (GnomeWrapList *wlist, int *width, int *height);

void gnome_wrap_list_set_row_spacing (GnomeWrapList *wlist, int spacing);
int gnome_wrap_list_get_row_spacing (GnomeWrapList *wlist);

void gnome_wrap_list_set_col_spacing (GnomeWrapList *wlist, int spacing);
int gnome_wrap_list_get_col_spacing (GnomeWrapList *wlist);

void gnome_wrap_list_set_use_unit_scrolling (GnomeWrapList *wlist, gboolean use_unit_scrolling);
gboolean gnome_wrap_list_get_use_unit_scrolling (GnomeWrapList *wlist);

void gnome_wrap_list_set_shadow_type (GnomeWrapList *wlist, GtkShadowType shadow_type);
GtkShadowType gnome_wrap_list_get_shadow_type (GnomeWrapList *wlist);



G_END_DECLS

#endif
