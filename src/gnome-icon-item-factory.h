/* GNOME libraries - default item factory for icon lists
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

#ifndef GNOME_ICON_ITEM_FACTORY_H
#define GNOME_ICON_ITEM_FACTORY_H

#include "gnome-list-item-factory.h"

G_BEGIN_DECLS



#define GNOME_TYPE_ICON_ITEM_FACTORY            (gnome_icon_item_factory_get_type ())
#define GNOME_ICON_ITEM_FACTORY(obj)            (GTK_CHECK_CAST ((obj),			\
						 GNOME_TYPE_ICON_ITEM_FACTORY, GnomeIconItemFactory))
#define GNOME_ICON_ITEM_FACTORY_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass),		\
						 GNOME_TYPE_ICON_ITEM_FACTORY,		\
						 GnomeIconItemFactoryClass))
#define GNOME_IS_ICON_ITEM_FACTORY(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_ICON_ITEM_FACTORY))
#define GNOME_IS_ICON_ITEM_FACTORY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass),		\
						 GNOME_TYPE_ICON_ITEM_FACTORY))


typedef struct _GnomeIconItemFactory GnomeIconItemFactory;
typedef struct _GnomeIconItemFactoryClass GnomeIconItemFactoryClass;

struct _GnomeIconItemFactory {
	GnomeListItemFactory li_factory;

	/* Private data */
	gpointer priv;
};

struct _GnomeIconItemFactoryClass {
	GnomeListItemFactoryClass parent_class;
};


GtkType gnome_icon_item_factory_get_type (void);

GnomeIconItemFactory *gnome_icon_item_factory_new (void);

void gnome_icon_item_factory_set_item_metrics (GnomeIconItemFactory *factory,
					       int item_width, int item_height,
					       int image_width, int image_height);
void gnome_icon_item_factory_get_item_metrics (GnomeIconItemFactory *factory,
					       int *item_width, int *item_height,
					       int *image_width, int *image_height);



G_END_DECLS

#endif
