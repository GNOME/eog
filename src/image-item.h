#ifndef IMAGE_ITEM_H
#define IMAGE_ITEM_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-canvas.h>

BEGIN_GNOME_DECLS



#define TYPE_IMAGE_ITEM            (image_item_get_type ())
#define IMAGE_ITEM(obj)            (GTK_CHECK_CAST ((obj), TYPE_IMAGE_ITEM, ImageItem))
#define IMAGE_ITEM_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_IMAGE_ITEM, ImageItemClass))
#define IS_IMAGE_ITEM(obj)         (GTK_CHECK_TYPE ((obj), TYPE_IMAGE_ITEM))
#define IS_IMAGE_ITEM_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_IMAGE_ITEM))


typedef struct _ImageItem ImageItem;
typedef struct _ImageItemClass ImageItemClass;


struct _ImageItem {
	GnomeCanvasItem item;

	/* Private data */
	gpointer priv;
};

struct _ImageItemClass {
	GnomeCanvasItemClass parent_class;
};


GtkType image_item_get_type (void);



END_GNOME_DECLS

#endif
