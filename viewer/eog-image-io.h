/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-image-io.h - Image load/saving methods for EogImage.
 *
 * Authors:
 *   Iain Holmes (ih@csd.abdn.ac.uk)
 *   Michael Zucchi (zucchi@zedzone.mmc.com.au)
 *   Federico Mena-Quintero (federico@gimp.org)
 *   Michael Fulbright (drmike@redhat.com)
 *   Martin Baulig (baulig@suse.de)
 *
 * Please refer to the individual image saving sections for information
 * about their authors and copyright.
 *
 * Copyright 1999-2000, Iain Holmes <ih@csd.abdn.ac.uk>
 * Copyright 1999, Michael Zucchi
 * Copyright 1999, The Free Software Foundation
 * Copyright 2000, SuSE GmbH.
 */

#ifndef _EOG_IMAGE_IO_H_
#define _EOG_IMAGE_IO_H_

#include <eog-image.h>

G_BEGIN_DECLS

/* 
 * You may call all these functions with or without the image library
 * being installed; if the image library is not installed, this'll
 * set the Persist::WrongDataType exception and return FALSE.
 */

gboolean eog_image_save_xpm (EogImage           *image,
			     Bonobo_Stream       stream,
			     CORBA_Environment  *ev);

gboolean eog_image_save_png (EogImage           *image,
			     Bonobo_Stream       stream,
			     CORBA_Environment  *ev);

gboolean eog_image_save_jpeg (EogImage          *image,
			      Bonobo_Stream      stream,
			      CORBA_Environment *ev);

G_END_DECLS

#endif /* _EOG_IMAGE_IO_H_ */

