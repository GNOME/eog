/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-image-helper.h - Image utility functions
 *
 * Authors:
 *   Iain Holmes (ih@csd.abdn.ac.uk)
 *   Michael Zucchi (zucchi@zedzone.mmc.com.au)
 *   Federico Mena-Quintero (federico@gimp.org)
 *   Michael Fulbright (drmike@redhat.com)
 *   Martin Baulig (baulig@suse.de)
 *   Jens Finke (jens@triq.net)
 *
 * Please refer to the individual image saving sections for information
 * about their authors and copyright.
 *
 * Copyright 1999-2000, Iain Holmes <ih@csd.abdn.ac.uk>
 * Copyright 1999, Michael Zucchi
 * Copyright 1999, The Free Software Foundation
 * Copyright 2000, SuSE GmbH.
 * Copyright 2003, The Free Software Foundation Europe
 */

#ifndef _EOG_IMAGE_HELPER_H_
#define _EOG_IMAGE_HELPER_H_

#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

/* 
 * You may call all this functions with or without the image library
 * being installed; if the image library is not installed, this'll
 * set the Persist::WrongDataType exception and return FALSE.
 */

gboolean eog_image_helper_save_xpm (GdkPixbuf *pixbuf, const char* path, GError **error);

G_END_DECLS

#endif /* _EOG_IMAGE_HELPER_H_ */
