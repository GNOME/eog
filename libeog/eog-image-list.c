#include <string.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-directory.h>

#include "eog-image-list.h"
#include "libeog-marshal.h"

struct  _EogIter {
	/* age of the image list when the iterator was created */
	unsigned int   age;

	/* pointer to the list */
	EogImageList  *list;

	/* the position of the iterator in the list */
	unsigned int   position;

	/* the node the iterator points into the image list */
	GList         *node;
};

struct _EogImageListPrivate {
	/* The age is incremented everytime the list is altered. 
	   Iterators are only valid if they have the same age */
	unsigned int    age;
	
	/* We hold our own references to the images in the list here */
	GList          *store;

	/* Number of images. Same as g_list_length (store) */
	int             n_images;

	/* number of images to load before emitting a 'prepared' signal */
	int             load_n_files;

	/* the largest common part of all image uris */
	GnomeVFSURI     *base_uri;
	gboolean         no_common_base;
};

typedef struct {
	EogImageList *list;
	GnomeVFSURI *uri;
	GnomeVFSFileInfo *info;
	GnomeVFSHandle *handle;
} DirLoadingContext;

/* Signal IDs */
enum {
	LIST_PREPARED,
	IMAGE_ADDED,
	IMAGE_REMOVED,
	LAST_SIGNAL
};
static guint eog_image_list_signals[LAST_SIGNAL];

static GSList *supported_mime_types = NULL;

/* ====================================================== */
/*               Cache Area                               */



/* ====================================================== */
/*              Eog Iter                                  */

/* Inits a new iter, only for internal use */
static EogIter*            
eog_iter_new (EogImageList *list)
{
	EogIter *iter;
	
	g_return_val_if_fail (EOG_IS_IMAGE_LIST (list), NULL);

	iter = g_new0 (EogIter, 1);
	iter->age      = list->priv->age;
	iter->list     = list;
	iter->position = 0;
	iter->node     = NULL;

	return iter;
}

/* Checks if the iter is valid. Only for internal use */
static gboolean
eog_iter_is_valid_private (EogImageList *list, EogIter *iter)
{
	return (iter->list == list && iter->age == list->priv->age);
}

/* ====================================================== */
/*               Eog Image List                           */

static void
eog_image_list_finalize (GObject *object)
{
	EogImageList *instance = EOG_IMAGE_LIST (object);
	
	if (instance->priv != NULL) {
		g_free (instance->priv);
		instance->priv = NULL;
	}
}

static void
eog_image_list_dispose (GObject *object)
{
	EogImageList *list;
	EogImageListPrivate *priv;
	GList *it;

	list = EOG_IMAGE_LIST (object);
	priv = list->priv;

	if (priv->store != NULL) {
		it = priv->store;
		for (; it != NULL; it = it->next) {
			g_object_unref (G_OBJECT (it->data));
		}
		
		g_list_free (priv->store);
		priv->store = NULL;
		priv->age++;
		priv->n_images = 0;
	}

	if (priv->base_uri != NULL) {
		gnome_vfs_uri_unref (priv->base_uri);
		priv->base_uri = NULL;
	}
}

static void
eog_image_list_instance_init (EogImageList *obj)
{
	EogImageListPrivate *priv;

	priv = g_new0 (EogImageListPrivate, 1);

	obj->priv = priv;

	priv->age = 0;
	priv->store = NULL;
	priv->n_images = 0;
	priv->base_uri = NULL;
	priv->no_common_base = FALSE;
}

static void 
eog_image_list_class_init (EogImageListClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->finalize = eog_image_list_finalize;
	object_class->dispose = eog_image_list_dispose;

	eog_image_list_signals[LIST_PREPARED] = 
		g_signal_new ("list-prepared",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogImageListClass, list_prepared),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	eog_image_list_signals[IMAGE_ADDED] =
		g_signal_new ("image-added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogImageListClass, image_added),
			      NULL,
			      NULL,
			      libeog_marshal_VOID__OBJECT_INT,
			      G_TYPE_NONE,
			      2,
			      EOG_TYPE_IMAGE,
			      G_TYPE_INT);
	eog_image_list_signals[IMAGE_REMOVED] =
		g_signal_new ("image-removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogImageListClass, image_removed),
			      NULL,
			      NULL,
			      libeog_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_INT);
}


GNOME_CLASS_BOILERPLATE (EogImageList,
			 eog_image_list,
			 GObject,
			 G_TYPE_OBJECT);


/* ================== Private EogImageList functions  ===================*/

static int
compare_filename_cb (gconstpointer a, gconstpointer b)
{
	EogImage *img_a;
	EogImage *img_b;

	img_a = EOG_IMAGE (a);
	img_b = EOG_IMAGE (b);

	return strcmp (eog_image_get_collate_key (img_a), eog_image_get_collate_key (img_b));
}

static void
free_loading_context (DirLoadingContext *ctx)
{
	if (ctx == NULL)
		return;
	
	if (ctx->uri)
		gnome_vfs_uri_unref (ctx->uri);
	ctx->uri = NULL;

	if (ctx->info)
		gnome_vfs_file_info_unref (ctx->info);
	ctx->info = NULL;
	
	if (ctx->handle)
		gnome_vfs_close (ctx->handle);
	ctx->handle = NULL;
	
	g_free (ctx);
}

/* Increases the refcount of the image and adds the image to the
 * internal list. If sorted is true the image is sorted in with regard
 * to the filename. Otherwise the image is prepended to the list.
 */
static void 
add_image_private (EogImageList *list, EogImage *image, gboolean sorted) 
{ 
	EogImageListPrivate *priv;

	g_return_if_fail (EOG_IS_IMAGE_LIST (list));	
	g_return_if_fail (EOG_IS_IMAGE (image));

	priv = list->priv;

	g_object_ref (image);

	if (sorted) {
		priv->store = g_list_insert_sorted (priv->store, image, 
						    (GCompareFunc) compare_filename_cb);
	}
	else {
		priv->store = g_list_prepend (priv->store, image);
	}

	priv->age++;
	priv->n_images++;
}

static gint
compare_quarks (gconstpointer a, gconstpointer b)
{
	return GPOINTER_TO_INT (a) - GPOINTER_TO_INT (b);
}

/* Creates a list of Quarks which represent all mime types supported
 * by GdkPixbuf at the moment */
static GSList*
get_supported_mime_types (void)
{
	GSList *format_list;
	GSList *it;
	GSList *list = NULL;
	gchar **mime_types;
	int i;
	
	format_list = gdk_pixbuf_get_formats ();

	for (it = format_list; it != NULL; it = it->next) {
		mime_types = gdk_pixbuf_format_get_mime_types ((GdkPixbufFormat *) it->data);

		for (i = 0; mime_types[i] != NULL; i++) {
			GQuark quark;

			quark = g_quark_from_string (mime_types[i]);
			list = g_slist_prepend (list,
						GINT_TO_POINTER (quark));
		}

		g_strfreev (mime_types);
	}

	list = g_slist_sort (list, (GCompareFunc) compare_quarks);
	
	g_slist_free (format_list);

	return list;
}

/* checks if the mime type is in our static list of 
 * supported mime types.
 */
static gboolean 
is_supported_mime_type (const char *mime_type) 
{
	GQuark quark;
	GSList *result;

	if (supported_mime_types == NULL) {
		supported_mime_types = get_supported_mime_types ();
	}

	quark = g_quark_from_string (mime_type);
	
	result = g_slist_find (supported_mime_types, GINT_TO_POINTER (quark));

	return (result != NULL);
}

/* ================== Directory Loading stuff ===================*/

/* Called for each file in a directory. Checks if the file is some
 * sort of image. If so, it creates an image object and adds it to the
 * list.
 */
static gboolean
directory_visit_cb (const gchar *rel_uri,
		    GnomeVFSFileInfo *info,
		    gboolean recursing_will_loop,
		    gpointer data,
		    gboolean *recurse)
{
	EogImage *image;
	DirLoadingContext *ctx;
	GnomeVFSURI *uri;
	EogImageList *list;
	EogImageListPrivate *priv;
	gboolean load_uri = FALSE;
	
	ctx = (DirLoadingContext*) data;
	list = ctx->list;
	priv = list->priv;

	if ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) > 0) {
		if (is_supported_mime_type (info->mime_type)) {
			load_uri = TRUE;
		}
	}

	if (load_uri) {
		/* FIXME: escape uri */
		uri = gnome_vfs_uri_append_file_name (ctx->uri, rel_uri);	
		
		image = eog_image_new_uri (uri);
		gnome_vfs_uri_unref (uri);
		
		if (image != NULL) {
			add_image_private (list, image, FALSE);
			g_object_unref (image);
		}
	}

	return TRUE;
}

/* Delayed dir loading function, called when idle. Initiates the
 * examination of each file of an directory pointed by ctx->uri.  If
 * successful (images found in the directory) the 'list-prepared'
 * signal is emitted, otherwise 'list-empty'.
 */
static gint
real_dir_loading (DirLoadingContext *ctx)
{
	EogImageList *list;
	EogImageListPrivate *priv;

	g_return_val_if_fail (ctx->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY, FALSE);

	list = ctx->list;
	priv = list->priv;

	g_assert (ctx->uri != NULL);

	priv->base_uri = gnome_vfs_uri_dup (ctx->uri);

	gnome_vfs_directory_visit_uri (ctx->uri,
				       GNOME_VFS_FILE_INFO_DEFAULT |
				       GNOME_VFS_FILE_INFO_FOLLOW_LINKS |
				       GNOME_VFS_FILE_INFO_GET_MIME_TYPE,
				       GNOME_VFS_DIRECTORY_VISIT_DEFAULT,
				       directory_visit_cb,
				       ctx);

	free_loading_context (ctx);

	priv->store = g_list_sort (priv->store, compare_filename_cb);
	
	/* notify interested objects, that the list is ready for usage */
	g_signal_emit (G_OBJECT (list), eog_image_list_signals[LIST_PREPARED], 0);
	
	return FALSE;
}

/* Delayed file loading function, called when idle. Loads an file
 * pointed by ctx->uri.  
 */
static gint
real_file_loading (DirLoadingContext *ctx)
{
	EogImageList *list;
	EogImageListPrivate *priv;
	EogImage *image = NULL;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;

	info = ctx->info;

	g_return_val_if_fail (info->type == GNOME_VFS_FILE_TYPE_REGULAR, FALSE);


	result = gnome_vfs_get_file_info_uri (ctx->uri, info,
					      GNOME_VFS_FILE_INFO_DEFAULT |
					      GNOME_VFS_FILE_INFO_FOLLOW_LINKS |
					      GNOME_VFS_FILE_INFO_GET_MIME_TYPE);
	if (result != GNOME_VFS_OK) {
		return FALSE;
	}

	list = ctx->list;
	priv = list->priv;

	if ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) > 0 &&
	    is_supported_mime_type (info->mime_type)) 
	{
		image = eog_image_new_uri (ctx->uri);
	}

	if (image != NULL) {
		add_image_private (list, image, TRUE);
		g_object_unref (image);

		/* Retrieve base uri. This may not possible if there
		 * are a lot of single images from different
		 * directories. In this case base_uri is set to NULL
		 * and no_common_base is TRUE.
		 */
		if ((priv->base_uri == NULL) && !priv->no_common_base) {
			priv->base_uri = gnome_vfs_uri_get_parent (ctx->uri);
		}
		else if ((priv->base_uri != NULL) && !gnome_vfs_uri_is_parent (priv->base_uri, ctx->uri, FALSE)) {
			gnome_vfs_uri_unref (priv->base_uri);
			priv->base_uri = NULL;
			priv->no_common_base = TRUE;
		}
	}
		
	priv->load_n_files--;

	if (priv->load_n_files == 0) {
		g_signal_emit (G_OBJECT (list), eog_image_list_signals[LIST_PREPARED], 0);
	}
			
       	free_loading_context (ctx);

	return FALSE;
}
	
/* Helper function to initialize the DirLoadingContext structure */
static DirLoadingContext*
prepare_loading_context (EogImageList *list, const gchar *text_uri) 
{
	DirLoadingContext *ctx;
	GnomeVFSResult result;

	ctx = g_new0 (DirLoadingContext, 1);
	ctx->uri = gnome_vfs_uri_new (text_uri);

#ifdef IMAGE_LIST_DEBUG
	g_message ("Prepare context for URI: %s", text_uri);
#endif

	ctx->info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info_uri (ctx->uri, ctx->info,
					      GNOME_VFS_FILE_INFO_DEFAULT |
					      GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	if (result != GNOME_VFS_OK) {
		free_loading_context (ctx);
		return NULL;
	}

	ctx->handle = 0;
	ctx->list = list;

	return ctx;
}

/* Public api function, which starts an idle function for looking for
 * image files in the directory pointed by the text_uri
 */
void
eog_image_list_add_directory (EogImageList *list, char *text_uri)
{
	EogImageListPrivate *priv;
	DirLoadingContext *ctx;

	g_return_if_fail (EOG_IS_IMAGE_LIST (list));
	g_return_if_fail (text_uri != NULL);
	
	priv = list->priv;

	ctx = prepare_loading_context (list, text_uri);
	
	if ((ctx != NULL) && (ctx->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)) {
		g_idle_add ((GSourceFunc)real_dir_loading, ctx);
	}
	else {
		if (ctx != NULL) {
			free_loading_context (ctx);
		}
		g_warning ("Can't handle URI: %s", text_uri);
	}	
}

void
eog_image_list_add_files (EogImageList *list, GList *uri_list) 
{
	GList *it;
	EogImageListPrivate *priv;
	DirLoadingContext *ctx;

	priv = list->priv;

	if (uri_list == NULL) {
		return;
	}

	priv->load_n_files = g_list_length (uri_list);

	for (it = uri_list; it != NULL; it = it->next) {
		ctx = prepare_loading_context (list, (char*) it->data);

		if ((ctx != NULL) && (ctx->info->type == GNOME_VFS_FILE_TYPE_REGULAR)) {
			g_idle_add ((GSourceFunc) real_file_loading, ctx);
		}
		else {
			if (ctx != NULL) {
				free_loading_context (ctx);
			}
			priv->load_n_files--;
		}
	}
}

/* ====================================================== */
/*               Public API                               */

/* Creates a new empty EogImageList. Use eog_image_list_add_directory
 * to fill the list.
 */
EogImageList*       
eog_image_list_new (void)
{
	EogImageList *list;
	
	list = EOG_IMAGE_LIST (g_object_new (EOG_TYPE_IMAGE_LIST, NULL));

	return list;
}

/* Creates a new EogImageList object, initially filled with the
 * objects from the submitted GList. The data structure will be
 * completely copied and all EogImage objects ref'ed. Therefore the
 * list l can/should be free'd after calling this function.
 */
EogImageList*
eog_image_list_new_from_glist (GList *l)
{
	EogImageList *list;
	EogImageListPrivate *priv;
	GList *it;

	list = EOG_IMAGE_LIST (g_object_new (EOG_TYPE_IMAGE_LIST, NULL));
	priv = list->priv;

	priv->store = g_list_copy (l);
	
	/* count number of images and ref each in one run */
	it = priv->store;
	for (; it != NULL; it = it->next) {
		priv->n_images++;
		g_object_ref (G_OBJECT (it->data));
	}

	return list;
}


/* ==================== Query functions ================================ */

/* Returns the number of images in the list. */
int
eog_image_list_length (EogImageList *list)
{
	g_return_val_if_fail (EOG_IS_IMAGE_LIST (list), 0);

	return list->priv->n_images;
}

GnomeVFSURI*
eog_image_list_get_base_uri (EogImageList *list)
{
	g_return_val_if_fail (EOG_IS_IMAGE_LIST (list), NULL);
	
	if (list->priv->base_uri != NULL)
		gnome_vfs_uri_ref (list->priv->base_uri);

	return list->priv->base_uri;
}


void
eog_image_list_add_image (EogImageList *list, EogImage *image)
{
	g_return_if_fail (EOG_IS_IMAGE_LIST (list));
	g_return_if_fail (EOG_IS_IMAGE (image));

	add_image_private (list, image, TRUE);
}

#if 0
void                eog_image_list_remove_image                   (EogImageList *list, EogImage *image);
#endif 

/* Returns the EogImage object pointed by iter or NULL if the iter is invalid. */
EogImage*
eog_image_list_get_img_by_iter (EogImageList *list, EogIter *iter)
{
	if (!eog_image_list_iter_valid (list, iter))
		return NULL;

	return g_object_ref (EOG_IMAGE (iter->node->data));
}

/* Returns the EogImage object at position in the list or NULL if the
 * position is not valid. */
EogImage*
eog_image_list_get_img_by_pos (EogImageList *list, unsigned int position)
{
	EogImageListPrivate *priv;
	GList *node;

	g_return_val_if_fail (EOG_IS_IMAGE_LIST (list), NULL);
	
	priv = list->priv;

	node = g_list_nth (priv->store, position);
	if (node == NULL)
		return NULL;

	return g_object_ref (EOG_IMAGE (node->data));
}

/* Returns the position the iter points to or -1 if the iter is invalid. */
int
eog_image_list_get_pos_by_iter (EogImageList *list, EogIter *iter)
{
	g_return_val_if_fail (EOG_IS_IMAGE_LIST (list), -1);
	g_return_val_if_fail (iter != NULL, -1);

	if (!eog_iter_is_valid_private (list, iter)) 
		return -1;
	else
		return iter->position;
}

/* Returns the position where the EogImage object is stored in the
 * list or -1 if the image is not found.
 */
int
eog_image_list_get_pos_by_img (EogImageList *list, EogImage *image)
{
	EogImageListPrivate *priv;
	GList *node;
	unsigned int  position = 0;

	g_return_val_if_fail (EOG_IS_IMAGE_LIST (list), -1);
	g_return_val_if_fail (EOG_IS_IMAGE (image), -1);
	
	priv = list->priv;
	
        node = priv->store;
	while (node != NULL && node->data != image) {
		node = node->next;
		position++;
	}

	if (node == NULL)
		return -1;

	return position;
}

/* Returns an iter which points at the first image object in the list
 * or NULL if the list is empty. The iter must be freed with g_free.
 */
EogIter*
eog_image_list_get_first_iter (EogImageList *list)
{
	EogIter *iter = NULL;
	EogImageListPrivate *priv;

	g_return_val_if_fail (EOG_IS_IMAGE_LIST (list), NULL);

	priv = list->priv;

	if (priv->store != NULL) {
		iter = eog_iter_new (list);
		
		iter->node = priv->store;
		iter->position = 0;
	}
		
	return iter;
}

/* Returns an iter which points at the image in the list. Returns NULL
 * if the image is not the list. The iter must be freed with g_free.
 */
EogIter*
eog_image_list_get_iter_by_img (EogImageList *list, EogImage *image)
{
	EogImageListPrivate *priv;
	GList *node;
	EogIter *iter;
	unsigned int  position = 0;

	g_return_val_if_fail (EOG_IS_IMAGE_LIST (list), NULL);

	if (image == NULL) 
		return NULL;
	
	priv = list->priv;
	
        node = priv->store;
	while (node != NULL && node->data != image) {
		node = node->next;
		position++;
	}

	if (node == NULL)
		return NULL;

	iter = eog_iter_new (list);
	iter->node = node;
	iter->position = position;
	
	return iter;
}

/* Get the iter which points at position in the list or NULL if the
 * position is out of bounds. The iter must be freed with g_free.
 */
EogIter*
eog_image_list_get_iter_by_pos (EogImageList *list, unsigned int position)
{
	EogImageListPrivate *priv;
	GList *node;
	EogIter *iter;

	g_return_val_if_fail (EOG_IS_IMAGE_LIST (list), NULL);
	
	priv = list->priv;
	
	node = g_list_nth (priv->store, position);
	if (node == NULL)
		return NULL;

	iter = eog_iter_new (list);
	iter->node = node;
	iter->position = position;
	
	return iter;
}

/* Returns a copy of the iter. Must be freed with g_free.
 */
EogIter*
eog_image_list_iter_copy (EogImageList *list, EogIter *iter)
{
	EogIter *copy;

	g_return_val_if_fail (EOG_IS_IMAGE_LIST (list), NULL);
	g_return_val_if_fail (iter != NULL, NULL);
	
	copy = eog_iter_new (list);
	copy->age = iter->age;
	copy->list = iter->list;
	copy->position = iter->position;
	copy->node = iter->node;

	return copy;
}

/* Checks if the iter is valid or not. A iter is valid for an list if
 * it points to the same list object and the age of both objects is
 * the same.
 */
gboolean
eog_image_list_iter_valid (EogImageList *list, EogIter *iter)
{
	g_return_val_if_fail (EOG_IS_IMAGE_LIST (list), FALSE);
	
	if (iter == NULL) {
		return FALSE;
	}
	else {
		return eog_iter_is_valid_private (list, iter);
	}
}

/* Points the iter to the previous position in the list. If loop is
 * true it doesn't stop at the first position but points the iter to
 * the end of the list.  Result: TRUE if the iter could be moved or
 * FALSE if not.
 */
gboolean
eog_image_list_iter_prev (EogImageList *list, EogIter *iter, gboolean loop)
{
	g_return_val_if_fail (EOG_IS_IMAGE_LIST (list), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	if (!eog_iter_is_valid_private (list, iter)) 
		return FALSE;

	if (iter->node->prev == NULL) {
		/* handle first list node separately */
		if (loop) {
			iter->node = g_list_last (list->priv->store);
			iter->position = list->priv->n_images - 1;
		}
		else {
			return FALSE;
		}
	}
	else {
		/* all other cases */
		iter->node = iter->node->prev;
		iter->position--;
	}

	return TRUE;
}

/* Points the iter to the next position in the list. If loop is true
 * it doesn't stop at the last position but points the iter to the
 * begining of the list.  Result: TRUE if the iter could be moved or
 * FALSE if not.
 */
gboolean
eog_image_list_iter_next (EogImageList *list, EogIter *iter, gboolean loop)
{
	g_return_val_if_fail (EOG_IS_IMAGE_LIST (list), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	if (!eog_iter_is_valid_private (list, iter)) 
		return FALSE;

	if (iter->node->next == NULL) {
		/* handle last list node separately */
		if (loop) {
			iter->node = list->priv->store;
			iter->position = 0;
		}
		else {
			return FALSE;
		}
	}
	else {
		/* all other cases */
		iter->node = iter->node->next;
		iter->position++;
	}

	return TRUE;
}

gboolean
eog_image_list_iter_equal (EogImageList *list, EogIter *a, EogIter *b)
{
	g_return_val_if_fail (EOG_IS_IMAGE_LIST (list), FALSE);

	if (!eog_iter_is_valid_private (list, a))
		return FALSE;

	if (!eog_iter_is_valid_private (list, b))
		return FALSE;
	
	return (a->list == b->list) && (a->age == b->age) && (a->position == b->position);
}

