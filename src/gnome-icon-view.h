/* GNOME libraries - icon view for an icon list model
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

#ifndef GNOME_ICON_VIEW_H
#define GNOME_ICON_VIEW_H

#include <libgnome/gnome-defs.h>
#include "gnome-icon-list-model.h"
#include "gnome-wrap-list.h"

BEGIN_GNOME_DECLS



#define GNOME_TYPE_ICON_VIEW            (gnome_icon_view_get_type ())
#define GNOME_ICON_VIEW(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_ICON_VIEW, GnomeIconView))
#define GNOME_ICON_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_ICON_VIEW,	\
					 GnomeIconViewClass))
#define GNOME_IS_ICON_VIEW(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_ICON_VIEW))
#define GNOME_IS_ICON_VIEW_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_ICON_VIEW))


typedef struct _GnomeIconView GnomeIconView;
typedef struct _GnomeIconViewClass GnomeIconViewClass;

struct _GnomeIconView {
	GnomeWrapList wlist;
};

struct _GnomeIconViewClass {
	GnomeWrapListClass parent_class;
};


GtkType gnome_icon_view_get_type (void);

GtkWidget *gnome_icon_view_new (void);

void gnome_icon_view_set_model (GnomeIconView *iview, GnomeIconListModel *model);



END_GNOME_DECLS

#endif
