#include "eog-image-list-model.h"

#ifndef _EOG_IMAGE_LOADER_H_
#define _EOG_IMAGE_LOADER_H_

BEGIN_GNOME_DECLS

#define EOG_IMAGE_LOADER_TYPE       (eog_image_loader_get_type ())
#define EOG_IMAGE_LOADER(o)         (GTK_CHECK_CAST ((o), EOG_IMAGE_LOADER_TYPE, EogImageLoader))
#define EOG_IMAGE_LOADER_CLASS(k)   (GTK_CHECK_CLASS_CAST((k), EOG_IMAGE_LOADER_TYPE, EogImageLoaderClass))
#define IS_EOG_IMAGE_LOADER(o)      (GTK_CHECK_TYPE ((o), EOG_IMAGE_LOADER_TYPE))
#define IS_EOG_IMAGE_LOADER_CLASS(k)    (GTK_CHECK_CLASS_TYPE ((k), EOG_IMAGE_LOADER_TYPE))

typedef struct _EogImageLoader  EogImageLoader;
typedef struct _EogImageLoaderClass EogImageLoaderClass;
typedef struct _EogImageLoaderPrivate EogImageLoaderPrivate;

struct _EogImageLoaderClass {
	GtkObjectClass parent_class;
};

struct _EogImageLoader {
	GtkObject parent;
	
	EogImageLoaderPrivate *priv;
};


GtkType eog_image_loader_get_type (void);

EogImageLoader* eog_image_loader_new (gint thumb_width, gint thumb_height);

void eog_image_loader_set_model (EogImageLoader *loader, EogImageListModel *model);
void eog_image_loader_start (EogImageLoader *loader);
void eog_image_loader_stop (EogImageLoader *loader);

END_GNOME_DECLS

#endif /* _EOG_IMAGE_LOADER_H_ */
