#ifndef _EOG_COLLECTION_ITEM_H_
#define _EOG_COLLECTION_ITEM_H_

#include <libgnomecanvas/gnome-canvas.h>
#include "eog-image.h"

G_BEGIN_DECLS

#define EOG_TYPE_COLLECTION_ITEM            (eog_collection_item_get_type ())
#define EOG_COLLECTION_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EOG_TYPE_COLLECTION_ITEM, EogCollectionItem))
#define EOG_COLLECTION_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EOG_TYPE_COLLECTION_ITEM, EogCollectionItemClass))
#define EOG_IS_COLLECTION_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EOG_TYPE_COLLECTION_ITEM))
#define EOG_IS_COLLECTION_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EOG_TYPE_COLLECTION_ITEM))

typedef struct _EogCollectionItem EogCollectionItem;
typedef struct _EogCollectionItemClass EogCollectionItemClass;
typedef struct _EogCollectionItemPrivate EogCollectionItemPrivate;

#define EOG_COLLECTION_ITEM_THUMB_WIDTH     96    /* maximal thumbnail width */
#define EOG_COLLECTION_ITEM_THUMB_HEIGHT    96    /* maximal thumbnail height */
#define EOG_COLLECTION_ITEM_FRAME_WIDTH      1    /* thickness of the frame around the image */
#define EOG_COLLECTION_ITEM_SPACING          6    /* space between image and caption */
#define EOG_COLLECTION_ITEM_CAPTION_PADDING  2    /* padding between caption text and selection */
#define EOG_COLLECTION_ITEM_CAPTION_FRAME_WIDTH 2 /* thickness of the frame around the caption */

#define EOG_COLLECTION_ITEM_MAX_WIDTH   (EOG_COLLECTION_ITEM_THUMB_WIDTH+2*EOG_COLLECTION_ITEM_CAPTION_PADDING+2*EOG_COLLECTION_ITEM_CAPTION_FRAME_WIDTH)

struct _EogCollectionItem {
	GnomeCanvasGroup parent;

	EogCollectionItemPrivate *priv;
};

struct _EogCollectionItemClass {
	GnomeCanvasGroupClass parent_class;
	
	void (* selection_changed) (EogCollectionItem *item, gboolean selected);
	void (* size_changed)      (EogCollectionItem *item);
};


GType eog_collection_item_get_type (void);

GnomeCanvasItem *eog_collection_item_new (GnomeCanvasGroup *group, EogImage *image);

void eog_collection_item_load (EogCollectionItem *item);

gboolean eog_collection_item_is_selected (EogCollectionItem *item);

void eog_collection_item_set_selected (EogCollectionItem *item, gboolean state);

void eog_collection_item_toggle_selected (EogCollectionItem *item);

EogImage* eog_collection_item_get_image (EogCollectionItem *item);

void eog_collection_item_get_size (EogCollectionItem *item, int *width, int *image_height, int *caption_height);


G_END_DECLS

#endif /* _EOG_COLLECTION_ITEM_H_ */
