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

#include <bonobo/bonobo-control.h>
#include "eog-image-view.h"

G_BEGIN_DECLS
 
#define EOG_CONTROL_TYPE           (eog_control_get_type ())
#define EOG_CONTROL(o)             (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_CONTROL_TYPE, EogControl))
#define EOG_CONTROL_CLASS(k)       (G_TYPE_CHECK_CLASS_CAST((k), EOG_CONTROL_TYPE, EogControlClass))

#define EOG_IS_CONTROL(o)          (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_CONTROL_TYPE))
#define EOG_IS_CONTROL_CLASS(k)    (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_CONTROL_TYPE))
#define EOG_CONTROL_GET_CLASS(o)   (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_CONTROL_TYPE, EogControlClass))

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

GType         eog_control_get_type                    (void);
EogControl    *eog_control_new                         (EogImage      *image);
EogControl    *eog_control_construct                   (EogControl    *control,
							EogImage      *image);

G_END_DECLS

#endif /* _EOG_EOG_CONTROL */
