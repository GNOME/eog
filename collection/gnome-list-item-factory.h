/* GNOME libraries - abstract list item factory
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

#ifndef GNOME_LIST_ITEM_FACTORY_H
#define GNOME_LIST_ITEM_FACTORY_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-canvas.h>
#include "eog-collection-model.h"

BEGIN_GNOME_DECLS



#define GNOME_TYPE_LIST_ITEM_FACTORY            (gnome_list_item_factory_get_type ())
#define GNOME_LIST_ITEM_FACTORY(obj)            (GTK_CHECK_CAST ((obj),			\
						 GNOME_TYPE_LIST_ITEM_FACTORY, GnomeListItemFactory))
#define GNOME_LIST_ITEM_FACTORY_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass),		\
						 GNOME_TYPE_LIST_ITEM_FACTORY,		\
						 GnomeListItemFactoryClass))
#define GNOME_IS_LIST_ITEM_FACTORY(obj)         (GTK_CHECK_TYPE ((obj),			\
						 GNOME_TYPE_LIST_ITEM_FACTORY))
#define GNOME_IS_LIST_ITEM_FACTORY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass),		\
						 GNOME_TYPE_LIST_ITEM_FACTORY))


typedef struct _GnomeListItemFactory GnomeListItemFactory;
typedef struct _GnomeListItemFactoryClass GnomeListItemFactoryClass;

struct _GnomeListItemFactory {
	GtkObject object;
};

struct _GnomeListItemFactoryClass {
	GtkObjectClass parent_class;

	/* Item mutation signals */

	GnomeCanvasItem *(* create_item) (GnomeListItemFactory *factory, GnomeCanvasGroup *parent, 
					  guint id);
	void (* update_item) (GnomeListItemFactory *factory, 
			      EogCollectionModel *model, 
			      GnomeCanvasItem *item);

	/* Item query signals */

	void (* get_item_size) (GnomeListItemFactory *factory, GnomeCanvasItem *item,
				EogCollectionModel *model, guint n,
				gint *width, gint *height);
};


GtkType gnome_list_item_factory_get_type (void);

GnomeCanvasItem *gnome_list_item_factory_create_item (GnomeListItemFactory *factory,
						      GnomeCanvasGroup *parent,
						      guint id);

void gnome_list_item_factory_update_item (GnomeListItemFactory *factory,
					  EogCollectionModel *model,
					  GnomeCanvasItem *item);

void gnome_list_item_factory_get_item_size (GnomeListItemFactory *factory, GnomeCanvasItem *item,
					    EogCollectionModel *model, guint n,
					    int *width, int *height);



END_GNOME_DECLS

#endif
