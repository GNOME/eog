#ifndef _EOG_IMAGE_LOADER_SIMPLE_H_
#define _EOG_IMAGE_LOADER_SIMPLE_H_

#include <glib-object.h>
#include "eog-image-loader.h"

G_BEGIN_DECLS

#define EOG_TYPE_IMAGE_LOADER_SIMPLE       (eog_image_loader_simple_get_type ())
#define EOG_IMAGE_LOADER_SIMPLE(o)         (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_IMAGE_LOADER_SIMPLE, EogImageLoaderSimple))
#define EOG_IMAGE_LOADER_SIMPLE_CLASS(k)   (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_IMAGE_LOADER_SIMPLE, EogImageLoaderSimpleClass))
#define EOG_IS_IMAGE_LOADER_SIMPLE(o)      (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_IMAGE_LOADER_SIMPLE))
#define EOG_IS_IMAGE_LOADER_SIMPLE_CLASS(k)    (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_IMAGE_LOADER_SIMPLE))

typedef struct _EogImageLoaderSimple  EogImageLoaderSimple;
typedef struct _EogImageLoaderSimpleClass EogImageLoaderSimpleClass;
typedef struct _EogImageLoaderSimplePrivate EogImageLoaderSimplePrivate;

struct _EogImageLoaderSimpleClass {
	EogImageLoaderClass parent_class;
};

struct _EogImageLoaderSimple {
	EogImageLoader loader;
	
	EogImageLoaderSimplePrivate *priv;
};


GType eog_image_loader_simple_get_type (void);

EogImageLoader*
eog_image_loader_simple_new (gint thumb_width, gint thumb_height);

G_END_DECLS

#endif /* _EOG_IMAGE_LOADER_SIMPLE_H_ */
