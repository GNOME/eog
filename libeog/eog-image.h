#ifndef _EOG_IMAGE_H_
#define _EOG_IMAGE_H_

#include <glib-object.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define EOG_TYPE_IMAGE          (eog_image_get_type ())
#define EOG_IMAGE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_IMAGE, EogImage))
#define EOG_IMAGE_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_IMAGE, EogImageClass))
#define EOG_IS_IMAGE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_IMAGE))
#define EOG_IS_IMAGE_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_IMAGE))
#define EOG_IMAGE_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_TYPE_IMAGE, EogImageClass))

typedef struct _EogImage EogImage;
typedef struct _EogImageClass EogImageClass;
typedef struct _EogImagePrivate EogImagePrivate;

typedef enum {
	EOG_IMAGE_LOAD_DEFAULT,
	EOG_IMAGE_LOAD_PROGRESSIVE,
	EOG_IMAGE_LOAD_COMPLETE
} EogImageLoadMode;

typedef enum {
	EOG_IMAGE_ERROR_SAVE_NOT_LOCAL,
	EOG_IMAGE_ERROR_NOT_LOADED,
	EOG_IMAGE_ERROR_VFS,
	EOG_IMAGE_ERROR_UNKNOWN,
} EogImageError;

#define EOG_IMAGE_ERROR eog_image_error_quark ()

struct _EogImage {
	GObject parent;

	EogImagePrivate *priv;
};

struct _EogImageClass {
	GObjectClass parent_klass;

	/* signals */
	void (* loading_size_prepared) (EogImage *img, int width, int height);
	void (* loading_update) (EogImage *img, int x, int y, int width, int height);
	void (* loading_finished) (EogImage *img);
	void (* loading_failed) (EogImage *img, const char* message);
	void (* loading_cancelled) (EogImage *img);
	
	void (* thumbnail_finished) (EogImage *img);
	void (* thumbnail_failed) (EogImage *img);
	void (* thumbnail_cancelled) (EogImage *img);

	void (* changed) (EogImage *img);
};

GType               eog_image_get_type                       (void) G_GNUC_CONST;

/* loading API */
EogImage*           eog_image_new                            (const char *txt_uri, EogImageLoadMode mode);
EogImage*           eog_image_new_uri                        (GnomeVFSURI *uri, EogImageLoadMode mode);
gboolean            eog_image_load                           (EogImage *img);
gboolean            eog_image_load_thumbnail                 (EogImage *img);
void                eog_image_free_mem                       (EogImage *img);

/* saving API */
gboolean            eog_image_save                            (EogImage *img, 
							       const GnomeVFSURI *uri,
							       GError **error);

/* query API */
gboolean            eog_image_is_animation                    (EogImage *img);
GdkPixbuf*          eog_image_get_pixbuf                      (EogImage *img);
GdkPixbuf*          eog_image_get_pixbuf_thumbnail            (EogImage *img);
void                eog_image_get_size                        (EogImage *img, int *width, int *height);
gboolean            eog_image_is_modified                     (EogImage *img);
gchar*              eog_image_get_caption                     (EogImage *img);

/* modification API */
void                eog_image_rotate_clock_wise               (EogImage *img);
void                eog_image_rotate_counter_clock_wise       (EogImage *img);
void                eog_image_rotate_180                      (EogImage *img);
void                eog_image_flip_horizontal                 (EogImage *img);
void                eog_image_flip_vertical                   (EogImage *img);

G_END_DECLS

#endif /* _IMAGE_H_ */
