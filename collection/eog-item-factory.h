/* Eye Of Gnome - abstract item factory
 *
 * Copyright (C) 2000-2001 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
 *         Jens Finke <jens@gnome.org>
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

#ifndef EOG_ITEM_FACTORY_H
#define EOG_ITEM_FACTORY_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-canvas.h>
#include "eog-collection-model.h"

BEGIN_GNOME_DECLS



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

struct _EogItemFactory {
	GtkObject object;
};

struct _EogItemFactoryClass {
	GtkObjectClass parent_class;

	/* Item mutation signals */
	GnomeCanvasItem *(* create_item) (EogItemFactory *factory, 
					  GnomeCanvasGroup *parent, 
					  guint id);
	void (* update_item) (EogItemFactory *factory, 
			      EogCollectionModel *model, 
			      GnomeCanvasItem *item);

	/* Item query signals */
	void (* get_item_size) (EogItemFactory *factory, 
				gint *width, gint *height);
	
	/* client signals */
	void (* configuration_changed) (EogItemFactory *factory);
};


GtkType eog_item_factory_get_type (void);

GtkObject* eog_item_factory_new (void);

GnomeCanvasItem *eog_item_factory_create_item (EogItemFactory *factory,
					       GnomeCanvasGroup *parent,
					       guint id);

void eog_item_factory_update_item (EogItemFactory *factory,
				   EogCollectionModel *model,
				   GnomeCanvasItem *item);

void eog_item_factory_get_item_size (EogItemFactory *factory, 
				     gint *width, gint *height);

/* only for internal use */
void eog_item_factory_configuration_changed (EogItemFactory *factory);



END_GNOME_DECLS

#endif
