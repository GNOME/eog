/* Eye Of Gnome - default item factory for icons
 *
 * Copyright (C) 2002 The Free Software Foundation
 *
 * Author: Jens Finke <jens@gnome.org>
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

#ifndef EOG_ITEM_FACTORY_CLEAN_H
#define EOG_ITEM_FACTORY_CLEAN_H

#include "eog-item-factory.h"
#include "eog-image-loader.h"

G_BEGIN_DECLS



#define EOG_TYPE_ITEM_FACTORY_CLEAN            (eog_item_factory_clean_get_type ())
#define EOG_ITEM_FACTORY_CLEAN(obj)            (GTK_CHECK_CAST ((obj),			\
						 EOG_TYPE_ITEM_FACTORY_CLEAN, EogItemFactoryClean))
#define EOG_ITEM_FACTORY_CLEAN_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass),		\
						 EOG_TYPE_ITEM_FACTORY_CLEAN,		\
						 EogItemFactoryCleanClass))
#define EOG_IS_ITEM_FACTORY_CLEAN(obj)         (GTK_CHECK_TYPE ((obj), EOG_TYPE_ITEM_FACTORY_CLEAN))
#define EGO_IS_ITEM_FACTORY_CLEAN_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass),		\
						 EOG_TYPE_ITEM_FACTORY_CLEAN))


typedef struct _EogItemFactoryClean EogItemFactoryClean;
typedef struct _EogItemFactoryCleanPrivate EogItemFactoryCleanPrivate;
typedef struct _EogItemFactoryCleanClass EogItemFactoryCleanClass;


struct _EogItemFactoryClean {
	EogItemFactory parent_object;

	/* Private data */
	EogItemFactoryCleanPrivate *priv;
};

struct _EogItemFactoryCleanClass {
	EogItemFactoryClass parent_class;
};

typedef struct {
	/* maximum thumbnail width */
	gint twidth;

	/* maximum thumbnail height */
	gint theight;

	/* caption spacing between image and selection rect */
	gint cspace;

	/* padding between selection rect and text */
	gint cpadding;

	/* font height in pixels */
	gint font_height;
} EogCleanMetrics;


GType eog_item_factory_clean_get_type (void);

EogItemFactoryClean *eog_item_factory_clean_new (EogImageLoader *loader);

void eog_item_factory_clean_set_metrics (EogItemFactoryClean *factory,
					  EogCleanMetrics *metrics);

EogCleanMetrics* eog_item_factory_clean_get_metrics (EogItemFactoryClean *factory);



G_END_DECLS

#endif
