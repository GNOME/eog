/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-image-view.h.
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *   Jens Finke (jens@gnome.org)
 *
 * Copyright 2000, SuSE GmbH.
 * Copyright 2002, The Free Software Foundation
 */

#ifndef _EOG_COLLECTION_VIEW_H_
#define _EOG_COLLECTION_VIEW_H_

#include <bonobo/bonobo-persist-file.h>

G_BEGIN_DECLS
 
#define EOG_TYPE_COLLECTION_VIEW          (eog_collection_view_get_type ())
#define EOG_COLLECTION_VIEW(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_COLLECTION_VIEW, EogCollectionView))
#define EOG_COLLECTION_VIEW_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_COLLECTION_VIEW, EogCollectionViewClass))

#define EOG_IS_COLLECTION_VIEW(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_COLLECTION_VIEW))
#define EOG_IS_COLLECTION_VIEW_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_COLLECTION_VIEW))
#define EOG_COLLECTION_VIEW_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_COLLECTION_VIEW_TYPE, EogCollectionViewClass))

typedef struct _EogCollectionView         EogCollectionView;
typedef struct _EogCollectionViewClass    EogCollectionViewClass;
typedef struct _EogCollectionViewPrivate  EogCollectionViewPrivate;

struct _EogCollectionView {
	BonoboPersistFile base;

	EogCollectionViewPrivate *priv;
};

struct _EogCollectionViewClass {
	BonoboPersistFileClass parent_class;
};

GType                   eog_collection_view_get_type  (void);

EogCollectionView       *eog_collection_view_new                 (void);

G_END_DECLS

#endif /* _EOG_COLLECTION_VIEW_H_ */
