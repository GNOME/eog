/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-image-control.h.
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000, SuSE GmbH.
 */

#ifndef _EOG_CONTROL_H_
#define _EOG_CONTROL_H_

#include <eog-image-view.h>

BEGIN_GNOME_DECLS
 
#define EOG_CONTROL_TYPE           (eog_control_get_type ())
#define EOG_CONTROL(o)             (GTK_CHECK_CAST ((o), EOG_CONTROL_TYPE, EogControl))
#define EOG_CONTROL_CLASS(k)       (GTK_CHECK_CLASS_CAST((k), EOG_CONTROL_TYPE, EogControlClass))

#define EOG_IS_CONTROL(o)          (GTK_CHECK_TYPE ((o), EOG_CONTROL_TYPE))
#define EOG_IS_CONTROL_CLASS(k)    (GTK_CHECK_CLASS_TYPE ((k), EOG_CONTROL_TYPE))

typedef struct _EogControl         EogControl;
typedef struct _EogControlClass    EogControlClass;
typedef struct _EogControlPrivate  EogControlPrivate;

struct _EogControl {
	BonoboControl control;

	EogControlPrivate *priv;
};

struct _EogControlClass {
	BonoboControlClass parent_class;
};

EogControl   *eog_control_new                   (EogImageData       *image_data);
GtkType       eog_control_get_type              (void) G_GNUC_CONST;

EOG_Control   eog_control_corba_object_create   (BonoboObject       *object);
EogControl   *eog_control_construct             (EogControl         *control,
						 EOG_Control         corba_object,
						 EogImageData       *image_data);

END_GNOME_DECLS

#endif _EOG_EOG_CONTROL
