/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-embeddable.h.
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000, SuSE GmbH.
 */

#ifndef _EOG_EMBEDDABLE_H_
#define _EOG_EMBEDDABLE_H_

#include <eog-image-view.h>
#include <bonobo/Bonobo.h>

G_BEGIN_DECLS
 
#define EOG_EMBEDDABLE_TYPE          (eog_embeddable_get_type ())
#define EOG_EMBEDDABLE(o)            (GTK_CHECK_CAST ((o), EOG_EMBEDDABLE_TYPE, EogEmbeddable))
#define EOG_EMBEDDABLE_CLASS(k)      (GTK_CHECK_CLASS_CAST((k), EOG_EMBEDDABLE_TYPE, EogEmbeddableClass))

#define EOG_IS_EMBEDDABLE(o)         (GTK_CHECK_TYPE ((o), EOG_EMBEDDABLE_TYPE))
#define EOG_IS_EMBEDDABLE_CLASS(k)   (GTK_CHECK_CLASS_TYPE ((k), EOG_EMBEDDABLE_TYPE))

typedef struct _EogEmbeddable         EogEmbeddable;
typedef struct _EogEmbeddableClass    EogEmbeddableClass;
typedef struct _EogEmbeddablePrivate  EogEmbeddablePrivate;

struct _EogEmbeddable {
	// FIXME: changed from parent as BonobEmbeddable, is this right??
	BonoboObject embeddable;

	EogEmbeddablePrivate *priv;
};

struct _EogEmbeddableClass {
	// FIXME: changed from parent as BonobEmbeddableClass, is this right??
	BonoboObjectClass parent_class;
};

EogEmbeddable *
eog_embeddable_new                      (EogImage                *image_data);

GtkType
eog_embeddable_get_type                 (void) G_GNUC_CONST;

EogEmbeddable *
eog_embeddable_construct                (EogEmbeddable           *embeddable,
                                         EogImage                *image);

G_END_DECLS

#endif /* _EOG_EOG_EMBEDDABLE */
