#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkobject.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs-types.h>

#ifndef _COLLECTION_IMAGE_H_
#define _COLLECTION_IMAGE_H_

BEGIN_GNOME_DECLS

#define TYPE_CIMAGE            (cimage_get_type ())
#define CIMAGE(obj)            (GTK_CHECK_CAST ((obj), TYPE_CIMAGE, CImage))
#define CIMAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_CIMAGE, CImageClass))
#define IS_CIMAGE(obj)         (GTK_CHECK_TYPE ((obj), TYPE_CIMAGE))
#define IS_CIMAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_CIMAGE))


typedef struct _CImageClass CImageClass;
typedef struct _CImage CImage;
typedef struct _CImagePrivate CImagePrivate;

struct _CImageClass {
	GtkObjectClass parent_class;
};

struct _CImage {
	GtkObject parent;
	
	CImagePrivate *priv;
};

GtkType    cimage_get_type (void);
CImage    *cimage_new (gchar *text_uri);
CImage    *cimage_new_uri (GnomeVFSURI *uri);

/* collection image operations */
void       cimage_set_thumbnail (CImage *img, GdkPixbuf *thumbnail);
void       cimage_set_loading_failed (CImage *img);
void       cimage_toggle_select_status (CImage *img);
void       cimage_set_select_status (CImage *img, gboolean status);
void       cimage_set_caption (CImage *img, gchar *caption);
void       cimage_set_image_dimensions (CImage *img, guint widht, guint height);

/* collection image attributes */
guint      cimage_get_unique_id (CImage *img);
GnomeVFSURI *cimage_get_uri (CImage *img);
GdkPixbuf *cimage_get_thumbnail (CImage *img);
gchar     *cimage_get_caption (CImage *img);
guint      cimage_get_width (CImage *img);
guint      cimage_get_height (CImage *img);

/* collection image queries */
gboolean   cimage_is_directory (CImage *img);
gboolean   cimage_is_selected (CImage *img);
gboolean   cimage_has_thumbnail (CImage *img);
gboolean   cimage_has_caption (CImage *img);
gboolean   cimage_has_loading_failed (CImage *img);

END_GNOME_DECLS

#endif /* _COLLECTION_IMAGE_H_ */
