#ifndef _EOG_IMAGE_CACHE_H_
#define _EOG_IMAGE_CACHE_H_

#include "eog-image.h"

G_BEGIN_DECLS

void eog_image_cache_add (EogImage *image);

void eog_image_cache_remove (EogImage *image);

void eog_image_cache_reload (EogImage *image);


G_END_DECLS

#endif /* _EGO_IMAGE_CACHE_H_ */
