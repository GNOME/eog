/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-image-data.h.
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000, SuSE GmbH.
 */

#ifndef _EOG_IMAGE_H_
#define _EOG_IMAGE_H_

#include <Eog.h>
#include <bonobo.h>

#include <eog-util.h>
#include <image.h>

BEGIN_GNOME_DECLS
 
#define EOG_IMAGE_TYPE          (eog_image_get_type ())
#define EOG_IMAGE(o)            (GTK_CHECK_CAST ((o), EOG_IMAGE_TYPE, EogImage))
#define EOG_IMAGE_CLASS(k)      (GTK_CHECK_CLASS_CAST((k), EOG_IMAGE_TYPE, EogImageClass))

#define EOG_IS_IMAGE(o)         (GTK_CHECK_TYPE ((o), EOG_IMAGE_TYPE))
#define EOG_IS_IMAGE_CLASS(k)   (GTK_CHECK_CLASS_TYPE ((k), EOG_IMAGE_TYPE))

typedef struct _EogImage         EogImage;
typedef struct _EogImageClass    EogImageClass;
typedef struct _EogImagePrivate  EogImagePrivate;

struct _EogImage {
        BonoboObject object;

        EogImagePrivate *priv;
};

struct _EogImageClass {
        BonoboObjectClass parent_class;

        void (*set_image)                       (EogImage       *image);
};

POA_GNOME_EOG_Image__epv *eog_image_get_epv     (void);

EogImage       *eog_image_new                   (void);
GtkType         eog_image_get_type              (void);
GNOME_EOG_Image eog_image_corba_object_create   (BonoboObject   *object);
BonoboObject   *eog_image_add_interfaces        (EogImage       *image,
						 BonoboObject   *to_aggregate);
EogImage       *eog_image_construct             (EogImage       *image,
						 GNOME_EOG_Image corba_object);
Image          *eog_image_get_image             (EogImage       *image);
GdkPixbuf      *eog_image_get_pixbuf            (EogImage       *image);


END_GNOME_DECLS

#endif _EOG_EOG_IMAGE
