#ifndef _EOG_IMAGE_JPEG_H_
#define _EOG_IMAGE_JPEG_H_

#include <config.h>

#if HAVE_JPEG

#include <glib.h>
#include "eog-image.h"

/* This will include some specialized routines for JPEG images, to support
   saving and transformations better for this format. */

gboolean eog_image_jpeg_save (EogImage *image, const char *path, GError **error);

/* This assumes that image represents a jpeg file on the local filesystem! */
gboolean eog_image_jpeg_save_lossless (EogImage *image, const char *path, GError **error);

#endif

#endif /* _EOG_IMAGE_JPEG_H_ */
