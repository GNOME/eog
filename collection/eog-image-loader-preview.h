#ifndef _EOG_IMAGE_LOADER_PREVIEW_H_
#define _EOG_IMAGE_LOADER_PREVIEW_H_

#if HAVE_LIBPREVIEW

#include <glib-object.h>
#include "eog-image-loader.h"

G_BEGIN_DECLS

#define EOG_TYPE_IMAGE_LOADER_PREVIEW       (eog_image_loader_preview_get_type ())
#define EOG_IMAGE_LOADER_PREVIEW(o)         (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_IMAGE_LOADER_PREVIEW, EogImageLoaderPreview))
#define EOG_IMAGE_LOADER_PREVIEW_CLASS(k)   (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_IMAGE_LOADER_PREVIEW, EogImageLoaderPreviewClass))
#define EOG_IS_IMAGE_LOADER_PREVIEW(o)      (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_IMAGE_LOADER_PREVIEW))
#define EOG_IS_IMAGE_LOADER_PREVIEW_CLASS(k)    (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_IMAGE_LOADER_PREVIEW))

typedef struct _EogImageLoaderPreview  EogImageLoaderPreview;
typedef struct _EogImageLoaderPreviewClass EogImageLoaderPreviewClass;
typedef struct _EogImageLoaderPreviewPrivate EogImageLoaderPreviewPrivate;

struct _EogImageLoaderPreviewClass {
	EogImageLoaderClass parent_class;
};

struct _EogImageLoaderPreview {
	EogImageLoader loader;
	
	EogImageLoaderPreviewPrivate *priv;
};


GType eog_image_loader_preview_get_type (void);

EogImageLoader*
eog_image_loader_preview_new (gint thumb_width, gint thumb_height);

G_END_DECLS

#endif

#endif /* _EOG_IMAGE_LOADER_PREVIEW_H_ */
