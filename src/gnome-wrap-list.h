/* GNOME libraries - abstract wrapped list view
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

#ifndef GNOME_WRAP_LIST_H
#define GNOME_WRAP_LIST_H

#include <libgnome/gnome-defs.h>
#include "gnome-list-view.h"

BEGIN_GNOME_DECLS



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
};

struct _GnomeWrapListClass {
	GnomeListViewClass parent_class;
};


GtkType gnome_wrap_list_get_type (void);




END_GNOME_DECLS

#endif
