/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-image-control.h.
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000, SuSE GmbH.
 */

#ifndef _EOG_IMAGE_CONTROL_H_
#define _EOG_IMAGE_CONTROL_H

#include <eog-image-view.h>

BEGIN_GNOME_DECLS
 
#define EOG_IMAGE_CONTROL_TYPE          (eog_image_control_get_type ())
#define EOG_IMAGE_CONTROL(o)            (GTK_CHECK_CAST ((o), EOG_IMAGE_CONTROL_TYPE, EogImageControl))
#define EOG_IMAGE_CONTROL_CLASS(k)      (GTK_CHECK_CLASS_CAST((k), EOG_IMAGE_CONTROL_TYPE, EogImageControlClass))

#define EOG_IS_IMAGE_CONTROL(o)         (GTK_CHECK_TYPE ((o), EOG_IMAGE_CONTROL_TYPE))
#define EOG_IS_IMAGE_CONTROL_CLASS(k)   (GTK_CHECK_CLASS_TYPE ((k), EOG_IMAGE_CONTROL_TYPE))

typedef struct _EogImageControl         EogImageControl;
typedef struct _EogImageControlClass    EogImageControlClass;
typedef struct _EogImageControlPrivate  EogImageControlPrivate;

struct _EogImageControl {
	BonoboControl control;

	EogImageControlPrivate *priv;
};

struct _EogImageControlClass {
	BonoboControlClass parent_class;
};

EogImageControl   *eog_image_control_new                   (EogImageData       *image_data);
GtkType            eog_image_control_get_type              (void) G_GNUC_CONST;

EOG_ImageControl   eog_image_control_corba_object_create   (BonoboObject       *object);
EogImageControl   *eog_image_control_construct             (EogImageControl    *image_control,
							    EOG_ImageControl    corba_object,
							    EogImageData       *image_data);

END_GNOME_DECLS

#endif _EOG_EOG_IMAGE_CONTROL
