#ifndef _EOG_IMAGE_CACHE_H_
#define _EOG_IMAGE_CACHE_H_

#include <glib-object.h>
#include "eog-image.h"


G_BEGIN_DECLS

#define EOG_TYPE_IMAGE_CACHE            (eog_image_cache_get_type ())
#define EOG_IMAGE_CACHE(o)         (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_IMAGE_CACHE, EogImageCache))
#define EOG_IMAGE_CACHE_CLASS(k)   (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_IMAGE_CACHE, EogImageCacheClass))
#define EOG_IS_IMAGE_CACHE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_IMAGE_CACHE))
#define EOG_IS_IMAGE_CACHE_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_IMAGE_CACHE))
#define EOG_IMAGE_CACHE_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_TYPE_IMAGE_CACHE, EogImageCacheClass))

typedef struct _EogImageCache EogImageCache;
typedef struct _EogImageCacheClass EogImageCacheClass;
typedef struct _EogImageCachePrivate EogImageCachePrivate;

struct _EogImageCache {
	GObject parent;

	EogImageCachePrivate *priv;
};

struct _EogImageCacheClass {
	GObjectClass parent_klass;
};

GType               eog_image_cache_get_type                       (void) G_GNUC_CONST;

EogImageCache*      eog_image_cache_new    (int n);

void                eog_image_cache_add    (EogImageCache *cache, EogImage *image);
void                eog_image_cache_remove (EogImageCache *cache, EogImage *image);



G_END_DECLS

#endif /* _EOG_IMAGE_CACHE_H_ */
