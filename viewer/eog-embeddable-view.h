/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-embeddable-view.h.
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000, SuSE GmbH.
 */

#ifndef _EOG_EMBEDDABLE_VIEW_H_
#define _EOG_EMBEDDABLE_VIEW_H_

#include <eog-image-view.h>

BEGIN_GNOME_DECLS
 
#define EOG_EMBEDDABLE_VIEW_TYPE          (eog_embeddable_view_get_type ())
#define EOG_EMBEDDABLE_VIEW(o)            (GTK_CHECK_CAST ((o), EOG_EMBEDDABLE_VIEW_TYPE, EogEmbeddableView))
#define EOG_EMBEDDABLE_VIEW_CLASS(k)      (GTK_CHECK_CLASS_CAST((k), EOG_EMBEDDABLE_VIEW_TYPE, EogEmbeddableViewClass))

#define EOG_IS_EMBEDDABLE_VIEW(o)         (GTK_CHECK_TYPE ((o), EOG_EMBEDDABLE_VIEW_TYPE))
#define EOG_IS_EMBEDDABLE_VIEW_CLASS(k)   (GTK_CHECK_CLASS_TYPE ((k), EOG_EMBEDDABLE_VIEW_TYPE))

typedef struct _EogEmbeddableView         EogEmbeddableView;
typedef struct _EogEmbeddableViewClass    EogEmbeddableViewClass;
typedef struct _EogEmbeddableViewPrivate  EogEmbeddableViewPrivate;

struct _EogEmbeddableView {
	BonoboView view;

	EogEmbeddableViewPrivate *priv;
};

struct _EogEmbeddableViewClass {
	BonoboViewClass parent_class;
};

EogEmbeddableView   *eog_embeddable_view_new                   (EogImageData         *image_data);
GtkType              eog_embeddable_view_get_type              (void) G_GNUC_CONST;

EOG_EmbeddableView   eog_embeddable_view_corba_object_create   (BonoboObject         *object);
EogEmbeddableView   *eog_embeddable_view_construct             (EogEmbeddableView    *embeddable_view,
								EOG_EmbeddableView    corba_object,
								EogImageData         *image_data);

END_GNOME_DECLS

#endif _EOG_EOG_EMBEDDABLE_VIEW
