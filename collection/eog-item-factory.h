/* Eye Of Gnome - abstract item factory
 *
 * Copyright (C) 2000-2001 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
 *         Jens Finke <jens@gnome.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EOG_ITEM_FACTORY_H
#define EOG_ITEM_FACTORY_H

#include <libgnomecanvas/gnome-canvas.h>
#include "eog-collection-model.h"

G_BEGIN_DECLS



#define EOG_TYPE_ITEM_FACTORY            (eog_item_factory_get_type ())
#define EOG_ITEM_FACTORY(obj)            (GTK_CHECK_CAST ((obj),			\
					  EOG_TYPE_ITEM_FACTORY, EogItemFactory))
#define EOG_ITEM_FACTORY_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass),		\
					  EOG_TYPE_ITEM_FACTORY,		\
					  EogItemFactoryClass))
#define EOG_IS_ITEM_FACTORY(obj)         (GTK_CHECK_TYPE ((obj),			\
					  EOG_TYPE_ITEM_FACTORY))
#define EOG_IS_ITEM_FACTORY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass),		\
					  EOG_TYPE_ITEM_FACTORY))


typedef struct _EogItemFactory EogItemFactory;
typedef struct _EogItemFactoryClass EogItemFactoryClass;

typedef enum {
	EOG_ITEM_UPDATE_NONE = 0,
	EOG_ITEM_UPDATE_IMAGE = 1 << 0,
	EOG_ITEM_UPDATE_CAPTION = 1 << 1,
	EOG_ITEM_UPDATE_SELECTION_STATE = 1 << 3
} EogItemUpdateHint;

#define EOG_ITEM_UPDATE_ALL  EOG_ITEM_UPDATE_IMAGE | EOG_ITEM_UPDATE_CAPTION | EOG_ITEM_UPDATE_SELECTION_STATE

struct _EogItemFactory {
	GObject object;
};

struct _EogItemFactoryClass {
	GObjectClass parent_class;

	/* Item mutation signals */
	GnomeCanvasItem *(* create_item) (EogItemFactory *factory, 
					  GnomeCanvasGroup *parent, 
					  guint id);
	void (* update_item) (EogItemFactory *factory, 
			      EogCollectionModel *model, 
			      GnomeCanvasItem *item, EogItemUpdateHint hint);

	/* Item query signals */
	void (* get_item_size) (EogItemFactory *factory, 
				gint *width, gint *height);
	
	/* client signals */
	void (* configuration_changed) (EogItemFactory *factory);
};


GType eog_item_factory_get_type (void);

GnomeCanvasItem *eog_item_factory_create_item (EogItemFactory *factory,
					       GnomeCanvasGroup *parent,
					       guint id);

void eog_item_factory_update_item (EogItemFactory *factory,
				   EogCollectionModel *model,
				   GnomeCanvasItem *item,
				   EogItemUpdateHint hint);

void eog_item_factory_get_item_size (EogItemFactory *factory, 
				     gint *width, gint *height);

/* only for internal use */
void eog_item_factory_configuration_changed (EogItemFactory *factory);



G_END_DECLS

#endif
