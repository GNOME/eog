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
#include <bonobo/bonobo-object.h>

#include <eog-util.h>

G_BEGIN_DECLS
 
#define EOG_IMAGE_TYPE          (eog_image_get_type ())
#define EOG_IMAGE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_IMAGE_TYPE, EogImage))
#define EOG_IMAGE_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), EOG_IMAGE_TYPE, EogImageClass))

#define EOG_IS_IMAGE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_IMAGE_TYPE))
#define EOG_IS_IMAGE_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_IMAGE_TYPE))

typedef struct _EogImage         EogImage;
typedef struct _EogImageClass    EogImageClass;
typedef struct _EogImagePrivate  EogImagePrivate;

struct _EogImage {
        BonoboObject base;

        EogImagePrivate *priv;
};

struct _EogImageClass {
        BonoboObjectClass parent_class;

	POA_GNOME_EOG_Image__epv epv;

        void (*set_image)                       (EogImage       *image);
};

POA_GNOME_EOG_Image__epv *eog_image_get_epv     (void);

EogImage       *eog_image_new                   (void);
GType          eog_image_get_type              (void);
BonoboObject   *eog_image_add_interfaces        (EogImage       *image,
						 BonoboObject   *to_aggregate);
EogImage       *eog_image_construct             (EogImage       *image);
GdkPixbuf      *eog_image_get_pixbuf            (EogImage       *image);
const gchar    *eog_image_get_filename          (EogImage       *image);

void	eog_image_save_to_stream	(EogImage	           *image, 
					 Bonobo_Stream	            stream, 
					 Bonobo_Persist_ContentType type, 
					 CORBA_Environment         *ev);
void	eog_image_load_from_stream	(EogImage                  *image,
					 Bonobo_Stream              stream,
					 CORBA_Environment         *ev);

G_END_DECLS

#endif /* _EOG_EOG_IMAGE */
