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

#include <eog-image-data.h>

BEGIN_GNOME_DECLS
 
#define EOG_IMAGE_VIEW_TYPE          (eog_image_view_get_type ())
#define EOG_IMAGE_VIEW(o)            (GTK_CHECK_CAST ((o), EOG_IMAGE_VIEW_TYPE, EogImageView))
#define EOG_IMAGE_VIEW_CLASS(k)      (GTK_CHECK_CLASS_CAST((k), EOG_IMAGE_VIEW_TYPE, EogImageViewClass))

#define EOG_IS_IMAGE_VIEW(o)         (GTK_CHECK_TYPE ((o), EOG_IMAGE_VIEW_TYPE))
#define EOG_IS_IMAGE_VIEW_CLASS(k)   (GTK_CHECK_CLASS_TYPE ((k), EOG_IMAGE_VIEW_TYPE))

typedef struct _EogImageView         EogImageView;
typedef struct _EogImageViewClass    EogImageViewClass;
typedef struct _EogImageViewPrivate  EogImageViewPrivate;

struct _EogImageView {
	BonoboObject object;

	EogImageViewPrivate *priv;
};

struct _EogImageViewClass {
	BonoboObjectClass parent_class;
};

POA_EOG_ImageView__epv *eog_image_view_get_epv       (void);

EogImageView       *eog_image_view_new                   (EogImageData       *image_data);
GtkType             eog_image_view_get_type              (void) G_GNUC_CONST;

EOG_ImageView       eog_image_view_corba_object_create   (BonoboObject       *object);
EogImageView       *eog_image_view_construct             (EogImageView       *image_view,
							  EOG_ImageView       corba_object,
							  EogImageData       *image_data);

EogImageData       *eog_image_view_get_image_data        (EogImageView       *image_view);
BonoboPropertyBag  *eog_image_view_get_property_bag      (EogImageView       *image_view);

BonoboZoomable     *eog_image_view_get_zoomable          (EogImageView       *image_view);

void                eog_image_view_set_ui_container      (EogImageView       *image_view,
							  Bonobo_UIContainer  ui_container);
void                eog_image_view_unset_ui_container    (EogImageView       *image_view);

GtkWidget          *eog_image_view_get_widget            (EogImageView       *image_view);



/* Properties */
void                eog_image_view_set_interpolation     (EogImageView       *image_view,
							  EOG_Interpolation   interpolation);
EOG_Interpolation   eog_image_view_get_interpolation     (EogImageView       *image_view);

void                eog_image_view_set_dither            (EogImageView       *image_view,
							  EOG_Dither          dither);
EOG_Dither          eog_image_view_get_dither            (EogImageView       *image_view);

void                eog_image_view_set_check_type        (EogImageView       *image_view,
							  EOG_CheckType       check_type);
EOG_CheckType       eog_image_view_get_check_type        (EogImageView       *image_view);

void                eog_image_view_set_check_size        (EogImageView       *image_view,
							  EOG_CheckSize       check_size);
EOG_CheckSize       eog_image_view_get_check_size        (EogImageView       *image_view);

END_GNOME_DECLS

#endif _EOG_EOG_IMAGE_VIEW
