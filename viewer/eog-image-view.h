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
#include "eog-image.h"

G_BEGIN_DECLS
 
#define EOG_IMAGE_VIEW_TYPE          (eog_image_view_get_type ())
#define EOG_IMAGE_VIEW(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_IMAGE_VIEW_TYPE, EogImageView))
#define EOG_IMAGE_VIEW_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), EOG_IMAGE_VIEW_TYPE, EogImageViewClass))

#define EOG_IS_IMAGE_VIEW(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_IMAGE_VIEW_TYPE))
#define EOG_IS_IMAGE_VIEW_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_IMAGE_VIEW_TYPE))
#define EOG_IMAGE_VIEW_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_IMAGE_VIEW_TYPE, EogImageViewClass))

#define GCONF_EOG_VIEW_DIR               "/apps/eog/view"
#define GCONF_EOG_VIEW_INTERP_TYPE       "/apps/eog/view/interp_type"
#define GCONF_EOG_VIEW_CHECK_TYPE        "/apps/eog/view/check_type"
#define GCONF_EOG_VIEW_CHECK_SIZE        "/apps/eog/view/check_size"
#define GCONF_EOG_VIEW_DITHER            "/apps/eog/view/dither"

typedef struct _EogImageView         EogImageView;
typedef struct _EogImageViewClass    EogImageViewClass;
typedef struct _EogImageViewPrivate  EogImageViewPrivate;

struct _EogImageView {
	BonoboObject base;

	EogImageViewPrivate *priv;
};

struct _EogImageViewClass {
	BonoboObjectClass parent_class;

	POA_GNOME_EOG_ImageView__epv epv;

	/* Signals */

	void (* close_item_activated) (EogImageView *image_view);
};

GtkType             eog_image_view_get_type            (void);

EogImageView       *eog_image_view_new                 (EogImage           *image,
							gboolean            zoom_fit,
							gboolean            need_close_item);
EogImageView       *eog_image_view_construct           (EogImageView       *image_view,
							EogImage           *image,
							gboolean            zoom_fit,
							gboolean            need_close_item);
EogImage           *eog_image_view_get_image           (EogImageView       *image_view);
BonoboPropertyBag  *eog_image_view_get_property_bag    (EogImageView       *image_view);
BonoboPropertyControl *eog_image_view_get_property_control (EogImageView   *image_view);
void                eog_image_view_set_ui_container    (EogImageView       *image_view,
							Bonobo_UIContainer  ui_container);
void                eog_image_view_unset_ui_container  (EogImageView       *image_view);
GtkWidget          *eog_image_view_get_widget          (EogImageView       *image_view);

void  eog_image_view_print (EogImageView *image_view, gboolean preview, 
			    const gchar *paper_size, gboolean landscape, 
			    gdouble bottom, gdouble top, gdouble right, 
			    gdouble left, gboolean vertically, 
			    gboolean horizontally, gboolean down_right, 
			    gboolean cut, gboolean fit_to_page, gint adjust_to,
			    gdouble overlap_x, gdouble overlap_y, 
			    gboolean overlap);

/* Zooming */
void  eog_image_view_get_zoom_factor (EogImageView *image_view,
				      double       *zoomx,
				      double       *zoomy);
void  eog_image_view_set_zoom_factor (EogImageView *image_view,
				      double        zoom_factor);
void  eog_image_view_zoom_to_fit     (EogImageView *image_view,
				      gboolean      keep_aspect_ratio);
void  eog_image_view_set_zoom        (EogImageView *image_view,
				      double        zoomx,
				      double        zoomy);

GConfClient* eog_image_view_get_client (EogImageView *image_view);
			 
G_END_DECLS

#endif /* _EOG_EOG_IMAGE_VIEW */
