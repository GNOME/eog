/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-image-view.h.
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *   Jens Finke (jens@gnome.org)
 *
 * Copyright 2000, SuSE GmbH.
 */

#ifndef _EOG_COLLECTION_VIEW_H_
#define _EOG_COLLECTION_VIEW_H_

#include <bonobo.h>
#include <Eog.h>

BEGIN_GNOME_DECLS
 
#define EOG_COLLECTION_VIEW_TYPE          (eog_collection_view_get_type ())
#define EOG_COLLECTION_VIEW(o)            (GTK_CHECK_CAST ((o), EOG_COLLECTION_VIEW_TYPE, EogCollectionView))
#define EOG_COLLECTION_VIEW_CLASS(k)      (GTK_CHECK_CLASS_CAST((k), EOG_COLLECTION_VIEW_TYPE, EogCollectionViewClass))

#define EOG_IS_COLLECTION_VIEW(o)         (GTK_CHECK_TYPE ((o), EOG_COLLECTION_VIEW_TYPE))
#define EOG_IS_COLLECTION_VIEW_CLASS(k)   (GTK_CHECK_CLASS_TYPE ((k), EOG_COLLECTION_VIEW_TYPE))

typedef struct _EogCollectionView         EogCollectionView;
typedef struct _EogCollectionViewClass    EogCollectionViewClass;
typedef struct _EogCollectionViewPrivate  EogCollectionViewPrivate;

struct _EogCollectionView {
	BonoboXObject base;

	EogCollectionViewPrivate *priv;
};

struct _EogCollectionViewClass {
	BonoboXObjectClass parent_class;

	POA_GNOME_EOG_ImageCollection__epv epv;
};

GtkType                  eog_collection_view_get_type  (void);

EogCollectionView       *eog_collection_view_new                 (void);
GNOME_EOG_ImageCollection eog_collection_view_corba_object_create (BonoboObject       *object);
EogCollectionView       *eog_collection_view_construct      (EogCollectionView       *list_view);

void                eog_collection_view_unset_ui_container  (EogCollectionView       *list_view);
void                eog_collection_view_set_ui_container  (EogCollectionView       *list_view,
							   Bonobo_UIContainer       ui_container);
GtkWidget          *eog_collection_view_get_widget          (EogCollectionView       *list_view);



END_GNOME_DECLS

#endif _EOG_COLLECTION_VIEW_H_
