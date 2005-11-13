#ifndef _EOG_IMAGE_LIST_H_
#define _EOG_IMAGE_LIST_H_

#include <glib-object.h>
#include <glib/glist.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include "eog-image.h"

G_BEGIN_DECLS


#define EOG_TYPE_IMAGE_LIST            (eog_image_list_get_type ())
#define EOG_IMAGE_LIST(o)         (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_IMAGE_LIST, EogImageList))
#define EOG_IMAGE_LIST_CLASS(k)   (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_IMAGE_LIST, EogImageListClass))
#define EOG_IS_IMAGE_LIST(o)         G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_IMAGE_LIST)
#define EOG_IS_IMAGE_LIST_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_IMAGE_LIST))
#define EOG_IMAGE_LIST_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_TYPE_IMAGE_LIST, EogImageListClass))

typedef struct _EogImageList EogImageList;
typedef struct _EogImageListClass EogImageListClass;
typedef struct _EogImageListPrivate EogImageListPrivate;

typedef struct _EogIter EogIter;

struct _EogImageList {
	GObject parent;

	EogImageListPrivate *priv;
};

struct _EogImageListClass {
	GObjectClass parent_klass;

	/* Notification signals */
	void (* list_prepared) (EogImageList *list);
	void (* reordered)     (EogImageList *list);
	void (* image_added)   (EogImageList *list, int pos);
	void (* image_removed) (EogImageList *list, int pos);
};

GType               eog_image_list_get_type                       (void) G_GNUC_CONST;

EogImageList*       eog_image_list_new                            (void);
EogImageList*       eog_image_list_new_from_glist                 (GList *list);

void                eog_image_list_add_uris                       (EogImageList *list, GList *uri_list);

int                 eog_image_list_get_initial_pos                (EogImageList *list);
GnomeVFSURI*        eog_image_list_get_base_uri                   (EogImageList *list);
int                 eog_image_list_length                         (EogImageList *list);

void                eog_image_list_add_image                      (EogImageList *list, EogImage *image);
void                eog_image_list_remove_image                   (EogImageList *list, EogImage *image);

EogImage*           eog_image_list_get_img_by_iter                (EogImageList *list, EogIter *iter);
EogImage*           eog_image_list_get_img_by_pos                 (EogImageList *list, unsigned int position);

int                 eog_image_list_get_pos_by_iter                (EogImageList *list, EogIter *iter);
int                 eog_image_list_get_pos_by_img                 (EogImageList *list, EogImage *image);

EogIter*            eog_image_list_get_first_iter                 (EogImageList *list);
EogIter*            eog_image_list_get_iter_by_img                (EogImageList *list, EogImage *image);
EogIter*            eog_image_list_get_iter_by_pos                (EogImageList *list, unsigned int position);

EogIter*            eog_image_list_iter_copy                      (EogImageList *list, EogIter *iter);
gboolean            eog_image_list_iter_valid                     (EogImageList *list, EogIter *iter); 
gboolean            eog_image_list_iter_prev                      (EogImageList *list, EogIter *iter, gboolean loop);
gboolean            eog_image_list_iter_next                      (EogImageList *list, EogIter *iter, gboolean loop);
gboolean            eog_image_list_iter_equal                     (EogImageList *list, EogIter *a, EogIter *b);

/* debug functions */
void                eog_image_list_print_debug                    (EogImageList *list);

#if 0 
/* FIXME: allow discrimination function, which decides if an image
 * gets into the list or not */
EogImageList*       eog_image_list_get_by_criteria                (EogImageList *list, );
#endif

G_END_DECLS

#endif /* _EOG_IMAGE_LIST_H_ */
