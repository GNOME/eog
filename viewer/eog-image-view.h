/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-image-view.h.
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000, SuSE GmbH.
 */

#ifndef _EOG_IMAGE_VIEW_H_
#define _EOG_IMAGE_VIEW_H_

#include <bonobo/bonobo-object.h>
#include <gconf/gconf-client.h>
#include <bonobo/bonobo-persist-file.h>

G_BEGIN_DECLS
 
#define EOG_IMAGE_VIEW_TYPE          (eog_image_view_get_type ())
#define EOG_IMAGE_VIEW(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_IMAGE_VIEW_TYPE, EogImageView))
#define EOG_IMAGE_VIEW_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), EOG_IMAGE_VIEW_TYPE, EogImageViewClass))

#define EOG_IS_IMAGE_VIEW(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_IMAGE_VIEW_TYPE))
#define EOG_IS_IMAGE_VIEW_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_IMAGE_VIEW_TYPE))
#define EOG_IMAGE_VIEW_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_IMAGE_VIEW_TYPE, EogImageViewClass))

#define GCONF_EOG_VIEW_DIR               "/apps/eog/view"
#define GCONF_EOG_VIEW_INTERP_TYPE       "/apps/eog/view/interpolate"
#define GCONF_EOG_VIEW_TRANSPARENCY      "/apps/eog/view/transparency"
#define GCONF_EOG_VIEW_TRANS_COLOR       "/apps/eog/view/trans_color"

#define GCONF_EOG_VIEW_CHECK_TYPE        "/apps/eog/view/check_type"
#define GCONF_EOG_VIEW_CHECK_SIZE        "/apps/eog/view/check_size"
#define GCONF_EOG_VIEW_DITHER            "/apps/eog/view/dither"

typedef struct _EogImageView         EogImageView;
typedef struct _EogImageViewClass    EogImageViewClass;
typedef struct _EogImageViewPrivate  EogImageViewPrivate;

struct _EogImageView {
	BonoboPersistFile base;

	EogImageViewPrivate *priv;
};

struct _EogImageViewClass {
	BonoboPersistFileClass parent_class;

	/* Signals */
	void (* close_item_activated) (EogImageView *image_view);
	void (* zoom_changed) (EogImageView *image_view);
};

GType               eog_image_view_get_type            (void);

EogImageView       *eog_image_view_new                 (gboolean            need_close_item);

GConfClient*        eog_image_view_get_client          (EogImageView       *image_view);

G_END_DECLS

#endif /* _EOG_EOG_IMAGE_VIEW */
