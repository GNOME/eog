/* Eye Of Gnome - default item factory for icons
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

#ifndef EOG_ITEM_FACTORY_SIMPLE_H
#define EOG_ITEM_FACTORY_SIMPLE_H

#include <libgnome/gnome-defs.h>
#include "eog-item-factory.h"

BEGIN_GNOME_DECLS



#define EOG_TYPE_ITEM_FACTORY_SIMPLE            (eog_item_factory_simple_get_type ())
#define EOG_ITEM_FACTORY_SIMPLE(obj)            (GTK_CHECK_CAST ((obj),			\
						 EOG_TYPE_ITEM_FACTORY_SIMPLE, EogItemFactorySimple))
#define EOG_ITEM_FACTORY_SIMPLE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass),		\
						 EOG_TYPE_ITEM_FACTORY_SIMPLE,		\
						 EogItemFactorySimpleClass))
#define EOG_IS_ITEM_FACTORY_SIMPLE(obj)         (GTK_CHECK_TYPE ((obj), EOG_TYPE_ITEM_FACTORY_SIMPLE))
#define EGO_IS_ITEM_FACTORY_SIMPLE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass),		\
						 EOG_TYPE_ITEM_FACTORY_SIMPLE))


typedef struct _EogItemFactorySimple EogItemFactorySimple;
typedef struct _EogItemFactorySimplePrivate EogItemFactorySimplePrivate;
typedef struct _EogItemFactorySimpleClass EogItemFactorySimpleClass;


struct _EogItemFactorySimple {
	EogItemFactory parent_object;

	/* Private data */
	EogItemFactorySimplePrivate *priv;
};

struct _EogItemFactorySimpleClass {
	EogItemFactoryClass parent_class;
};

typedef struct {
	/* maximum thumbnail width */
	gint twidth;

	/* maximum thumbnail height */
	gint theight;

	/* caption spacing */
	gint cspace;

	/* item border */
	gint border;
} EogSimpleMetrics;


GtkType eog_item_factory_simple_get_type (void);

EogItemFactorySimple *eog_item_factory_simple_new (void);

void eog_item_factory_simple_set_metrics (EogItemFactorySimple *factory,
					  EogSimpleMetrics *metrics);

EogSimpleMetrics* eog_item_factory_simple_get_metrics (EogItemFactorySimple *factory);



END_GNOME_DECLS

#endif
