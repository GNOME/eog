/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-image-data.h.
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000, SuSE GmbH.
 */

#ifndef _EOG_IMAGE_DATA_H_
#define _EOG_IMAGE_DATA_H_

#include <eog-util.h>
#include <image.h>

BEGIN_GNOME_DECLS
 
#define EOG_IMAGE_DATA_TYPE          (eog_image_data_get_type ())
#define EOG_IMAGE_DATA(o)            (GTK_CHECK_CAST ((o), EOG_IMAGE_DATA_TYPE, EogImageData))
#define EOG_IMAGE_DATA_CLASS(k)      (GTK_CHECK_CLASS_CAST((k), EOG_IMAGE_DATA_TYPE, EogImageDataClass))

#define EOG_IS_IMAGE_DATA(o)         (GTK_CHECK_TYPE ((o), EOG_IMAGE_DATA_TYPE))
#define EOG_IS_IMAGE_DATA_CLASS(k)   (GTK_CHECK_CLASS_TYPE ((k), EOG_IMAGE_DATA_TYPE))

typedef struct _EogImageData         EogImageData;
typedef struct _EogImageDataClass    EogImageDataClass;
typedef struct _EogImageDataPrivate  EogImageDataPrivate;

struct _EogImageData {
        BonoboObject object;

        EogImageDataPrivate *priv;
};

struct _EogImageDataClass {
        BonoboObjectClass parent_class;

        void (*set_image)               (EogImageData            *image_data);
};

POA_GNOME_EOG_ImageData__epv *
eog_image_data_get_epv                  (void);

EogImageData *
eog_image_data_new                      (void);

GtkType
eog_image_data_get_type                 (void) G_GNUC_CONST;

GNOME_EOG_ImageData
eog_image_data_corba_object_create      (BonoboObject            *object);

EogImageData
*eog_image_data_construct               (EogImageData            *image_data,
                                         GNOME_EOG_ImageData      corba_object);

Image *
eog_image_data_get_image                (EogImageData            *image_data);

END_GNOME_DECLS

#endif _EOG_EOG_IMAGE_DATA
