#ifndef UI_IMAGE_H
#define UI_IMAGE_H

#include <libgnome/gnome-defs.h>
#include <gtk/gtktable.h>
#include "image.h"

BEGIN_GNOME_DECLS


#define TYPE_UI_IMAGE            (ui_image_get_type ())
#define UI_IMAGE(obj)            (GTK_CHECK_CAST (obj), TYPE_UI_IMAGE, UIImage)
#define UI_IMAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_UI_IMAGE, UIImageClass))
#define IS_UI_IMAGE(obj)         (GTK_CHECK_TYPE ((obj), TYPE_UI_IMAGE))
#define IS_UI_IMAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_UI_IMAGE))

typedef struct _UIImage UIImage;
typedef struct _UIImageClass UIImageClass;

struct _UIImage {
	GtkTable table;
	
	/* Image we are displaying */
	Image *image;

	/* Image view area */
	GtkWidget *view;

	/* Vertical and horizontal adjustments */
	GtkAdjustment *vadj;
	GtkAdjustment *hadj;

	/* Vertical and horizontal scrollbars */
	GtkWidget *vsb;
	GtkWidget *hsb;
};

struct _UIImageClass {
	GtkTableClass parent_class;
};


GtkType ui_image_get_type (void);
UIImage *ui_image_new (void);
void ui_image_set_image (UIImage *ui, Image *image);


END_GNOME_DECLS

#endif
