/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * EOG image viewer control.
 *
 * Authors:
 *   Michael Meeks (mmeeks@gnu.org)
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000, Helixcode Inc.
 * Copyright 2000, Eazel, Inc.
 * Copyright 2000, SuSE GmbH.
 */

#ifndef _EOG_IMAGE_VIEWER_H_
#define _EOG_IMAGE_VIEWER_H

#include <bonobo.h>

BEGIN_GNOME_DECLS
 
#define EOG_IMAGE_VIEWER_TYPE        (eog_image_viewer_get_type ())
#define EOG_IMAGE_VIEWER(o)          (GTK_CHECK_CAST ((o), EOG_IMAGE_VIEWER_TYPE, EogImageViewer))
#define EOG_IMAGE_VIEWER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), EOG_IMAGE_VIEWER_TYPE, EogImageViewerClass))

#define EOG_IS_IMAGE_VIEWER(o)       (GTK_CHECK_TYPE ((o), EOG_IMAGE_VIEWER_TYPE))
#define EOG_IS_IMAGE_VIEWER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), EOG_IMAGE_VIEWER_TYPE))

typedef struct _EogImageViewer EogImageViewer;
typedef struct _EogImageViewerClass EogImageViewerClass;
typedef struct _EogImageViewerPrivate EogImageViewerPrivate;

struct _EogImageViewer {
	BonoboControl control;

	EogImageViewerPrivate *priv;
};

struct _EogImageViewerClass {
	BonoboControlClass parent_class;
};

EogImageViewer   *eog_image_viewer_new          (void);
GtkType           eog_image_viewer_get_type     (void) G_GNUC_CONST;
EogImageViewer   *eog_image_viewer_construct    (EogImageViewer *image_viewer,
						 Bonobo_Control corba_control);

END_GNOME_DECLS

#endif _EOG_EOG_IMAGE_VIEWER
