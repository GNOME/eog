/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  GNOME::EOG::CollectionView - C-implementation of the bonobo
 *  interface.
 *
 *  Authors: Jens Finke <jens@triq.net>
 *
 *  Copyright (C) 2004 Jens Finke
 */

#ifndef _EOG_COLLECTION_VIEW_IFACE_H_
#define _EOG_COLLECTION_VIEW_IFACE_H_

#include <bonobo/bonobo-object.h>
#include "Eog.h"

G_BEGIN_DECLS

#define EOG_TYPE_COLLECTION_VIEW_IFACE        (eog_collection_view_iface_get_type ())
#define EOG_COLLECTION_VIEW_IFACE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_COLLECTION_VIEW_IFACE, EogCollectionViewIface))
#define EOG_COLLECTION_VIEW_IFACE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_COLLECTION_VIEW_IFACE, EogCollectionViewIfaceClass))
#define EOG_IS_COLLECTION_VIEW_IFACE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_COLLECTION_VIEW_IFACE))
#define EOG_IS_COLLECTION_VIEW_IFACE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_COLLECTION_VIEW_IFACE))
#define EOG_COLLECTION_VIEW_IFACE_GET_CLASS(o)(G_TYPE_INSTANCE_GET_CLASS ((o), EOG_TYPE_COLLECTION_VIEW_IFACE, EogCollectionViewIfaceClass))

typedef struct {
        BonoboObject		object;

} EogCollectionViewIface;

typedef struct {
	BonoboObjectClass	parent;

	POA_GNOME_EOG_CollectionView__epv epv;

	void (*load_uri_list)   (EogCollectionViewIface *view_iface, GList *uri_list);
} EogCollectionViewIfaceClass;

GType		 eog_collection_view_iface_get_type                       (void) G_GNUC_CONST;

EogCollectionViewIface	*eog_collection_view_iface_new				(void);

G_END_DECLS

#endif /* _EOG_COLLECTION_VIEW_IFACE_H_ */
