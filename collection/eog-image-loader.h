#ifndef _EOG_IMAGE_LOADER_H_
#define _EOG_IMAGE_LOADER_H_

#include <glib-object.h>
#include "cimage.h"

G_BEGIN_DECLS

#define EOG_TYPE_IMAGE_LOADER       (eog_image_loader_get_type ())
#define EOG_IMAGE_LOADER(o)         (GTK_CHECK_CAST ((o), EOG_TYPE_IMAGE_LOADER, EogImageLoader))
#define EOG_IMAGE_LOADER_CLASS(k)   (GTK_CHECK_CLASS_CAST((k), EOG_TYPE_IMAGE_LOADER, EogImageLoaderClass))
#define EOG_IS_IMAGE_LOADER(o)      (GTK_CHECK_TYPE ((o), EOG_TYPE_IMAGE_LOADER))
#define EOG_IS_IMAGE_LOADER_CLASS(k)    (GTK_CHECK_CLASS_TYPE ((k), EOG_TYPE_IMAGE_LOADER))

typedef struct _EogImageLoader  EogImageLoader;
typedef struct _EogImageLoaderClass EogImageLoaderClass;
typedef struct _EogImageLoaderPrivate EogImageLoaderPrivate;

struct _EogImageLoaderClass {
	GObjectClass parent_class;

	/* signals */
	void (* loading_finished) (EogImageLoader *loader, CImage *img);
	void (* loading_canceled) (EogImageLoader *loader, CImage *img);
	void (* loading_failed) (EogImageLoader *loader, CImage *img);
	
	/* virtuell functions */
	void (* start) (EogImageLoader *loader, CImage *img);
	void (* stop)  (EogImageLoader *loader);
};

struct _EogImageLoader {
	GObject parent;
};


GType eog_image_loader_get_type (void);

void eog_image_loader_start (EogImageLoader *loader, CImage *img);

void eog_image_loader_stop (EogImageLoader *loader);

/* only for internal use */
void _eog_image_loader_loading_finished (EogImageLoader *loader, CImage *img);
void _eog_image_loader_loading_canceled (EogImageLoader *loader, CImage *img);
void _eog_image_loader_loading_failed (EogImageLoader *loader, CImage *img);

G_END_DECLS

#endif /* _EOG_IMAGE_LOADER_H_ */
