#include <config.h>
#include <string.h>
#include <glib/gthread.h>
#include <glib/gqueue.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-thumbnail.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomevfs/gnome-vfs.h>
#include <eel/eel-vfs-extensions.h>
#if HAVE_EXIF
#include <libexif/exif-data.h>
#include <libexif/exif-utils.h>
#include <libexif/exif-loader.h>
#endif

#include "libeog-marshal.h"
#include "eog-image.h"
#include "eog-image-private.h"
#include "eog-pixbuf-util.h"
#include "eog-image-cache.h"
#if HAVE_JPEG
#include "eog-image-jpeg.h"
#endif

static GThread     *thread                     = NULL;
static gboolean     thread_running             = FALSE;
static GQueue      *jobs_waiting               = NULL;
static GQueue      *jobs_done                  = NULL;
static gint         dispatch_callbacks_id      = -1;
static GStaticMutex jobs_mutex                 = G_STATIC_MUTEX_INIT;


enum {
	SIGNAL_LOADING_UPDATE,
	SIGNAL_LOADING_SIZE_PREPARED,
	SIGNAL_LOADING_FINISHED,
	SIGNAL_LOADING_INFO_FINISHED,
	SIGNAL_LOADING_FAILED,
	SIGNAL_LOADING_CANCELLED,
	SIGNAL_PROGRESS,
	SIGNAL_IMAGE_CHANGED,
	SIGNAL_THUMBNAIL_FINISHED,
	SIGNAL_THUMBNAIL_FAILED,
	SIGNAL_THUMBNAIL_CANCELLED,
	SIGNAL_LAST
};

static gint eog_image_signals [SIGNAL_LAST];

#define NO_DEBUG
#define DEBUG_ASYNC 0
#define THUMB_DEBUG 0
#define OBJECT_WATCH 0

#if OBJECT_WATCH
static int n_active_images = 0;
#endif

#define CHECK_LOAD_TIMEOUT 5

/* Chunk size for reading image data */
#define READ_BUFFER_SIZE 65536

/*============================================

  static thumbnail loader for all image objects

  ------------------------------------------*/

static gint
dispatch_image_finished (gpointer data)
{
	EogImage *image;
 
#if DEBUG_ASYNC
	g_print ("*** dispatch callback called ***");
#endif

	image = NULL;

	g_static_mutex_lock (&jobs_mutex);
	if (!g_queue_is_empty (jobs_done)) {
		image = EOG_IMAGE (g_queue_pop_head (jobs_done));
	}
	else {
		g_queue_free (jobs_done);
		jobs_done = NULL;
		dispatch_callbacks_id = -1;
	}
	g_static_mutex_unlock (&jobs_mutex);	

	if (image == NULL) {
#if DEBUG_ASYNC
		g_print (" --- shutdown\n");
#endif
		return FALSE;
	}
		
	if (image->priv->thumbnail != NULL) {
		g_signal_emit (G_OBJECT (image), eog_image_signals [SIGNAL_THUMBNAIL_FINISHED], 0);
	}
	else {
		g_signal_emit (G_OBJECT (image), eog_image_signals [SIGNAL_THUMBNAIL_FAILED], 0);
	}
	g_object_unref (image);

#if DEBUG_ASYNC
	g_print ("\n");
#endif
	
	return TRUE;
}

static gpointer
create_thumbnails (gpointer data)
{
	EogImage *image;
	EogImagePrivate *priv;
	char *uri_str = NULL;
	char *path = NULL;
	gboolean finished = FALSE;
	gboolean create_thumb = FALSE;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	GnomeThumbnailFactory *factory;

#if DEBUG_ASYNC
	g_print ("*** Start thread ***\n");
#endif	

	while (!finished) {
	        create_thumb = FALSE;

		/* get next image to process */
		g_static_mutex_lock (&jobs_mutex);

		image = EOG_IMAGE (g_queue_pop_head (jobs_waiting));
		g_assert (image != NULL);

		g_static_mutex_unlock (&jobs_mutex);

		/* thumbnail loading/creation  */
		priv = image->priv;

		if (priv->thumbnail != NULL) {
			g_object_unref (priv->thumbnail);
			priv->thumbnail = NULL;
		}

		uri_str = gnome_vfs_uri_to_string (priv->uri, GNOME_VFS_URI_HIDE_NONE);
		info = gnome_vfs_file_info_new ();
		result = gnome_vfs_get_file_info_uri (priv->uri, info, 
						      GNOME_VFS_FILE_INFO_DEFAULT |
						      GNOME_VFS_FILE_INFO_GET_MIME_TYPE);

		if (result == GNOME_VFS_OK &&
		    (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME) != 0 &&
		    (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) != 0) 
		{
			path = gnome_thumbnail_path_for_uri (uri_str, GNOME_THUMBNAIL_SIZE_NORMAL);

			if (g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
				priv->thumbnail = gdk_pixbuf_new_from_file (path, NULL);

				if (!gnome_thumbnail_is_valid (priv->thumbnail, uri_str, info->mtime)) {
					g_object_unref (priv->thumbnail);
					priv->thumbnail = NULL;
					create_thumb = TRUE;
#if THUMB_DEBUG
					g_print ("uri: %s, thumbnail is invalid\n", uri_str);
#endif
				}
			}
			else {
#if THUMB_DEBUG
				g_print ("uri: %s, has no thumbnail file\n", uri_str);
#endif
				create_thumb = TRUE;
			}
		}
		else {
#if THUMB_DEBUG
			g_print ("uri: %s vfs errror: %s\n", uri_str, gnome_vfs_result_to_string (result));
#endif
		}

		if (create_thumb) {

			g_assert (path != NULL);
			g_assert (info != NULL);
			g_assert ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME) != 0);
			g_assert ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) != 0);
			g_assert (priv->thumbnail == NULL);
		
#if THUMB_DEBUG
			g_print ("create thumbnail for uri: %s\n -> mtime: %i\n -> mime_type; %s\n -> thumbpath: %s\n", 
				 uri_str, info->mtime, info->mime_type, path);
#endif
	
			factory = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);
	
			if (!gnome_thumbnail_factory_has_valid_failed_thumbnail (factory, uri_str, info->mtime) &&
			    gnome_thumbnail_factory_can_thumbnail (factory, uri_str, info->mime_type, info->mtime)) 
			{
				priv->thumbnail = gnome_thumbnail_factory_generate_thumbnail (factory, uri_str, info->mime_type);
				
				if (priv->thumbnail != NULL) {
					gnome_thumbnail_factory_save_thumbnail (factory, priv->thumbnail, uri_str, info->mtime);
				}
			}
			
			g_object_unref (factory);
			
		}
		
		gnome_vfs_file_info_unref (info);
		g_free (uri_str);
		g_free (path);
		

		/* check for thread shutdown */
		g_static_mutex_lock (&jobs_mutex);

		if (jobs_done == NULL) {
			jobs_done = g_queue_new ();
		}
		g_queue_push_tail (jobs_done, image);
		
		if (dispatch_callbacks_id == -1) {
			dispatch_callbacks_id = g_idle_add (dispatch_image_finished, NULL);
		}

		if (g_queue_is_empty (jobs_waiting)) {
			g_queue_free (jobs_waiting);
			jobs_waiting = NULL;
			thread_running = FALSE;
			finished = TRUE;
		}
			
		g_static_mutex_unlock (&jobs_mutex);
	}

#if DEBUG_ASYNC
	g_print ("*** Finish thread ***\n");
#endif	


	return NULL;
}

static void
add_image_to_queue (EogImage *image)
{
	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	g_static_mutex_lock (&jobs_mutex);

	if (jobs_waiting == NULL) {
		jobs_waiting = g_queue_new ();
	}

	g_object_ref (image);
	g_queue_push_tail (jobs_waiting, image);

	if (!thread_running) {
		thread = g_thread_create (create_thumbnails, NULL, TRUE, NULL);
		thread_running = TRUE;
	}

	g_static_mutex_unlock (&jobs_mutex);
}


/*======================================

   EogImage implementation 

   ------------------------------------*/

GNOME_CLASS_BOILERPLATE (EogImage,
			 eog_image,
			 GObject,
			 G_TYPE_OBJECT);

static void
eog_image_dispose (GObject *object)
{
	EogImagePrivate *priv;
	GList *it;

	priv = EOG_IMAGE (object)->priv;

	eog_image_free_mem (EOG_IMAGE (object));

	if (priv->uri) {
		gnome_vfs_uri_unref (priv->uri);
		priv->uri = NULL;
	}

	if (priv->caption) {
		g_free (priv->caption);
		priv->caption = NULL;
	}

	if (priv->caption_key) {
		g_free (priv->caption_key);
		priv->caption_key = NULL;
	}

	if (priv->file_type) {
		g_free (priv->file_type);
		priv->file_type = NULL;
	}
	
	if (priv->status_mutex) {
		g_mutex_free (priv->status_mutex);
		priv->status_mutex = NULL;
	}

	if (priv->trans) {
		g_object_unref (priv->trans);
		priv->trans = NULL;
	}

	if (priv->undo_stack) {
		for (it = priv->undo_stack; it != NULL; it = it->next){
			g_object_unref (G_OBJECT (it->data));
		}

		g_list_free (priv->undo_stack);
		priv->undo_stack = NULL;
	}
}

static void
eog_image_finalize (GObject *object)
{
	EogImagePrivate *priv;

	priv = EOG_IMAGE (object)->priv;

	g_free (priv);

#if OBJECT_WATCH
	n_active_images--;
	if (n_active_images == 0) {
		g_message ("All image objects destroyed.");
	}
	else {
		g_message ("active image objects: %i", n_active_images);
	}
#endif
}

static void
eog_image_class_init (EogImageClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->dispose = eog_image_dispose;
	object_class->finalize = eog_image_finalize;

	eog_image_signals [SIGNAL_LOADING_UPDATE] = 
		g_signal_new ("loading_update",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogImageClass, loading_update),
			      NULL, NULL,
			      libeog_marshal_VOID__INT_INT_INT_INT,
			      G_TYPE_NONE, 4,
			      G_TYPE_INT,
			      G_TYPE_INT,
			      G_TYPE_INT,
			      G_TYPE_INT);
	eog_image_signals [SIGNAL_LOADING_SIZE_PREPARED] = 
		g_signal_new ("loading_size_prepared",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogImageClass, loading_size_prepared),
			      NULL, NULL,
			      libeog_marshal_VOID__INT_INT,
			      G_TYPE_NONE, 2,
			      G_TYPE_INT,
			      G_TYPE_INT);
	eog_image_signals [SIGNAL_LOADING_FINISHED] = 
		g_signal_new ("loading_finished",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogImageClass, loading_finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);			     
	eog_image_signals [SIGNAL_LOADING_INFO_FINISHED] = 
		g_signal_new ("loading_info_finished",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogImageClass, loading_info_finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);			     
	eog_image_signals [SIGNAL_LOADING_FAILED] = 
		g_signal_new ("loading_failed",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogImageClass, loading_failed),
			      NULL, NULL,
			      libeog_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
	eog_image_signals [SIGNAL_LOADING_CANCELLED] = 
		g_signal_new ("loading_cancelled",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogImageClass, loading_cancelled),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	eog_image_signals [SIGNAL_PROGRESS] = 
		g_signal_new ("progress",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogImageClass, progress),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__FLOAT,
			      G_TYPE_NONE, 1,
			      G_TYPE_FLOAT);
	eog_image_signals [SIGNAL_IMAGE_CHANGED] = 
		g_signal_new ("image_changed",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogImageClass, image_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	eog_image_signals [SIGNAL_THUMBNAIL_FINISHED] = 
		g_signal_new ("thumbnail_finished",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogImageClass, thumbnail_finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);			     
	eog_image_signals [SIGNAL_THUMBNAIL_FAILED] = 
		g_signal_new ("thumbnail_failed",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogImageClass, thumbnail_failed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	eog_image_signals [SIGNAL_THUMBNAIL_CANCELLED] = 
		g_signal_new ("thumbnail_cancelled",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogImageClass, thumbnail_cancelled),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
eog_image_instance_init (EogImage *img)
{
	EogImagePrivate *priv;

	priv = g_new0 (EogImagePrivate, 1);

	priv->uri = NULL;
	priv->image = NULL;
	priv->thumbnail = NULL;
	priv->width = priv->height = -1;
	priv->modified = FALSE;
	priv->status_mutex = g_mutex_new ();
#if HAVE_EXIF
	priv->exif = NULL;
#endif

	img->priv = priv;
}

EogImage* 
eog_image_new_uri (GnomeVFSURI *uri)
{
	EogImage *img;
	EogImagePrivate *priv;
	
	img = EOG_IMAGE (g_object_new (EOG_TYPE_IMAGE, NULL));
	priv = img->priv;

	priv->uri = gnome_vfs_uri_ref (uri);
	priv->mode = EOG_IMAGE_LOAD_DEFAULT;
	priv->status = EOG_IMAGE_STATUS_UNKNOWN;
	priv->modified = FALSE;
	priv->load_thread = NULL;

	priv->undo_stack = NULL;
	priv->trans = NULL;

#if OBJECT_WATCH
	n_active_images++;
	g_message ("active image objects: %i", n_active_images);
#endif
	
	return img;
}

EogImage* 
eog_image_new (const char *txt_uri)
{
	GnomeVFSURI *uri;
	EogImage *image;

	uri = gnome_vfs_uri_new (txt_uri);
	image = eog_image_new_uri (uri);
	gnome_vfs_uri_unref (uri);

	return image;
}

GQuark
eog_image_error_quark (void)
{
	static GQuark q = 0;
	if (q == 0)
		q = g_quark_from_static_string ("eog-image-error-quark");
	
	return q;
}

static gboolean
check_load_status (gpointer data)
{
	EogImage *img;
	EogImagePrivate *priv;
	int load_status;
	int x, y, width, height;
	gboolean call_again = TRUE;
	float progress;

	img = EOG_IMAGE (data);
	priv = img->priv;
	
	g_mutex_lock (priv->status_mutex);

	g_source_remove (priv->load_id);
	priv->load_id = -1;

	load_status = priv->load_status;
	x = priv->update_x1;
	y = priv->update_y1;
	width = priv->update_x2 - x;
	height = priv->update_y2 - y;
	progress = priv->progress;

	priv->load_status = 0;
	priv->update_x1 = 10000;
	priv->update_y1 = 10000;
	priv->update_x2 = 0;
	priv->update_y2 = 0;

	g_mutex_unlock (priv->status_mutex);

	if ((load_status & EOG_IMAGE_LOAD_STATUS_FAILED) > 0) {
		g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_LOADING_FAILED], 0, priv->error_message);
		call_again = FALSE;
	}
	
	if ((load_status & EOG_IMAGE_LOAD_STATUS_CANCELLED) > 0) {
		g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_LOADING_CANCELLED], 0);
		call_again = FALSE;
	}
	
	if ((load_status & EOG_IMAGE_LOAD_STATUS_PREPARED) > 0) {
		g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_LOADING_SIZE_PREPARED], 0, priv->width, priv->height);
	}
	
	if ((load_status & EOG_IMAGE_LOAD_STATUS_UPDATED) > 0) {
		g_signal_emit (img, eog_image_signals [SIGNAL_LOADING_UPDATE], 0, 
			       x, y, width, height);
	}

	if ((load_status & EOG_IMAGE_LOAD_STATUS_INFO_DONE) > 0) {
		g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_LOADING_INFO_FINISHED], 0);
	}
	
 	if ((load_status & EOG_IMAGE_LOAD_STATUS_DONE) > 0) {
		if ((load_status & EOG_IMAGE_LOAD_STATUS_TRANSFORMED) > 0) {
			g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_IMAGE_CHANGED], 0);
		}
		else {
			g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_LOADING_FINISHED], 0);
		}

		call_again = FALSE;
	}

 	if ((load_status & EOG_IMAGE_LOAD_STATUS_PROGRESS) > 0) {
		g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_PROGRESS], 0, progress);
	}

	if (call_again) {
		priv->load_id = g_timeout_add (CHECK_LOAD_TIMEOUT, check_load_status, img);
	}
	else {
		g_object_unref (img);
	}

	return FALSE;
}


static void
transform_progress_hook_async (EogTransform *trans, float progress, gpointer hook_data)
{
	EogImage *img;
	EogImagePrivate *priv;

	img = EOG_IMAGE(hook_data);
	priv = img->priv;

	if (progress != priv->progress) {
		g_mutex_lock (priv->status_mutex);
		priv->progress = progress;
		priv->load_status |= EOG_IMAGE_LOAD_STATUS_PROGRESS;
		g_mutex_unlock (priv->status_mutex);		
	}
}

static void 
load_area_updated (GdkPixbufLoader *loader, gint x, gint y, gint width, gint height, gpointer data)
{
	EogImage *img;
	EogImagePrivate *priv;
	int x2, y2;

	img = EOG_IMAGE (data);

#ifdef DEBUG
	g_print ("load_area_updated\n");
#endif

	priv = img->priv;

	g_mutex_lock (priv->status_mutex);
	priv->load_status |= EOG_IMAGE_LOAD_STATUS_UPDATED;

	if (priv->image == NULL) {
		priv->image = gdk_pixbuf_loader_get_pixbuf (loader);
		g_object_ref (priv->image);
	}
	
	x2 = x + width;
	y2 = y + height;

	priv->update_x1 = MIN (priv->update_x1, x);
	priv->update_y1 = MIN (priv->update_y1, y);
	priv->update_x2 = MAX (priv->update_x2, x2);
	priv->update_y2 = MAX (priv->update_y2, y2);

	g_mutex_unlock (priv->status_mutex);

#ifdef DEBUG
	g_print ("area_updated: x: %i, y: %i, width: %i, height: %i\n", x, y, width, height);
#endif
}

static void
load_size_prepared (GdkPixbufLoader *loader, gint width, gint height, gpointer data)
{
	EogImage *img;

	g_return_if_fail (EOG_IS_IMAGE (data));
	
	img = EOG_IMAGE (data);

	g_mutex_lock (img->priv->status_mutex);
	img->priv->load_status |= EOG_IMAGE_LOAD_STATUS_PREPARED;

	img->priv->width = width;
	img->priv->height = height;
	g_mutex_unlock (img->priv->status_mutex);
}

#if HAVE_EXIF

static void
update_exif_data (EogImage *image)
{
	EogImagePrivate *priv;
	ExifEntry *entry;
	ExifByteOrder bo;

	g_return_if_fail (EOG_IS_IMAGE (image));
	
	priv = image->priv;
	
#ifdef DEBUG
	g_message ("update exif data");
#endif
	if (priv->exif == NULL) return;

	/* FIXME: Must we update more properties here? */
	bo = exif_data_get_byte_order (priv->exif);

	entry = exif_content_get_entry (priv->exif->ifd [EXIF_IFD_EXIF], EXIF_TAG_PIXEL_X_DIMENSION);
	if (entry != NULL) {
		exif_set_long (entry->data, bo, priv->width);
	}

	entry = exif_content_get_entry (priv->exif->ifd [EXIF_IFD_EXIF], EXIF_TAG_PIXEL_Y_DIMENSION);
	if (entry != NULL) {
		exif_set_long (entry->data, bo, priv->height);
	}	
}


#endif 

/* this function runs in it's own thread */
static gpointer
real_image_load (gpointer data)
{
	EogImage *img;
	EogImagePrivate *priv;
	GdkPixbufLoader *loader;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	guchar *buffer;
	GnomeVFSFileSize bytes_read;
	GnomeVFSFileSize bytes_read_total;
	gboolean failed;
	GError *error = NULL;
#if HAVE_EXIF
	ExifLoader *exif_loader;
#endif

	img = EOG_IMAGE (data);
	priv = img->priv;

#ifdef DEBUG
	g_print ("real image load %s\n", gnome_vfs_uri_to_string (priv->uri, GNOME_VFS_URI_HIDE_NONE));
#endif

	g_assert (priv->image == NULL);
#if HAVE_EXIF
	if (priv->exif != NULL) {
		/* FIXME: this shouldn't happen but do so sometimes. */
		g_warning ("exif data not freed\n");
	}
#endif
	if (priv->file_type != NULL) {
		g_free (priv->file_type);
		priv->file_type = NULL;
	}

	result = gnome_vfs_open_uri (&handle, priv->uri, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		g_mutex_lock (priv->status_mutex);
		priv->load_status |= EOG_IMAGE_LOAD_STATUS_FAILED;
		priv->error_message = (char*) gnome_vfs_result_to_string (result);
		g_mutex_unlock (priv->status_mutex);

		return NULL;
	}

	/* determine file size */
	/* FIXME: we should reuse the values gained in eog_image_load here */
	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info_uri (priv->uri,
					      info,
					      GNOME_VFS_FILE_INFO_DEFAULT);
	if ((result != GNOME_VFS_OK) || (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) == 0) {
		g_mutex_lock (priv->status_mutex);
		priv->load_status |= EOG_IMAGE_LOAD_STATUS_FAILED;
		priv->error_message = (char*) gnome_vfs_result_to_string (result);
		g_mutex_unlock (priv->status_mutex);

		gnome_vfs_file_info_unref (info);

		return NULL;
	}
	bytes_read_total = 0;
	priv->progress = 0.0;
	priv->bytes = info->size;
	
	buffer = g_new0 (guchar, READ_BUFFER_SIZE);
	loader = gdk_pixbuf_loader_new ();
	failed = FALSE;
#if HAVE_EXIF
	exif_loader = exif_loader_new ();
#endif

	g_signal_connect_object (G_OBJECT (loader), "size-prepared", (GCallback) load_size_prepared, img, 0);
	if (priv->mode == EOG_IMAGE_LOAD_PROGRESSIVE) {
		g_signal_connect_object (G_OBJECT (loader), "area-updated", (GCallback) load_area_updated, img, 0);
	}

	while (!priv->cancel_loading) {
		float new_progress;
		
		result = gnome_vfs_read (handle, buffer, READ_BUFFER_SIZE, &bytes_read);
		if (result == GNOME_VFS_ERROR_EOF || bytes_read == 0) {
			break;
		}
		else if (result != GNOME_VFS_OK) {
			failed = TRUE;
			break;
		}
		
		if (!gdk_pixbuf_loader_write (loader, buffer, bytes_read, &error)) {
			failed = TRUE;
			break;
		}

		bytes_read_total += bytes_read;
		new_progress = (float) bytes_read_total / (float) info->size;
		if (priv->progress != new_progress) {
			priv->progress = new_progress;
			priv->load_status |= EOG_IMAGE_LOAD_STATUS_PROGRESS;
		}

#if HAVE_EXIF
		if (exif_loader != NULL && (exif_loader_write (exif_loader, buffer, bytes_read) == 0)) {
			g_mutex_lock (img->priv->status_mutex);
			priv->exif = exif_loader_get_data (exif_loader);
			priv->load_status |= EOG_IMAGE_LOAD_STATUS_INFO_DONE;
			g_mutex_unlock (img->priv->status_mutex);
			
			exif_loader_unref (exif_loader);
			exif_loader = NULL;
		}
#endif
	}

	gdk_pixbuf_loader_close (loader, NULL);	

	g_free (buffer);
	gnome_vfs_close (handle);
	gnome_vfs_file_info_unref (info);
	
	if (failed || (bytes_read_total == 0)) {
		g_mutex_lock (priv->status_mutex);
		if (priv->image != NULL) {
			g_object_unref (priv->image);
			priv->image = NULL;
		}
		priv->load_status |= EOG_IMAGE_LOAD_STATUS_FAILED;
		priv->progress = 0.0;
		
		if (error != NULL) {
			priv->error_message = g_strdup (error->message);
		}
		else if (bytes_read_total == 0) {
			priv->error_message = g_strdup (_("empty file"));
		}
		else {
			priv->error_message = NULL;
		}
		priv->status = EOG_IMAGE_STATUS_FAILED;
		g_mutex_unlock (priv->status_mutex);
	}
	else if (priv->cancel_loading) {
		g_mutex_lock (priv->status_mutex);
		if (priv->image != NULL) {
			g_object_unref (priv->image);
			priv->image = NULL;
		}
#if HAVE_EXIF
		if (priv->exif != NULL) {
			exif_data_unref (priv->exif);
			priv->exif = NULL;
		}
#endif
		priv->cancel_loading = FALSE;
		priv->progress = 0.0;

		priv->load_status |= EOG_IMAGE_LOAD_STATUS_CANCELLED;
		priv->status = EOG_IMAGE_STATUS_UNKNOWN;
		g_mutex_unlock (priv->status_mutex);
	}
	else {
		GdkPixbuf *image;
		GdkPixbuf *transformed = NULL;
		EogTransform *trans;
		GdkPixbufFormat *format;

		g_mutex_lock (priv->status_mutex);
		image = priv->image;
		trans = priv->trans;
		g_mutex_unlock (priv->status_mutex);
		
		if (image == NULL) {
			image = gdk_pixbuf_loader_get_pixbuf (loader);
			g_object_ref (image);
		}
		g_assert (image != NULL);
		
		if (trans != NULL) {
			transformed = eog_transform_apply (trans, image, transform_progress_hook_async, img);

			g_object_unref (image);
			image = transformed;
		}

		g_mutex_lock (priv->status_mutex);
		priv->progress = 1.0;
		priv->image = image;
		priv->width = gdk_pixbuf_get_width (priv->image);
		priv->height = gdk_pixbuf_get_height (priv->image);
		format = gdk_pixbuf_loader_get_format (loader);
		if (format != NULL) {
			priv->file_type = g_strdup (gdk_pixbuf_format_get_name (format));
		}
#if HAVE_EXIF
		update_exif_data (img);
#endif 
		eog_image_cache_add (img);

		if (trans != NULL && priv->mode == EOG_IMAGE_LOAD_PROGRESSIVE) {
			priv->load_status |= EOG_IMAGE_LOAD_STATUS_TRANSFORMED;
		}

		priv->load_status |= EOG_IMAGE_LOAD_STATUS_DONE;
		priv->status = EOG_IMAGE_STATUS_LOADED;
		g_mutex_unlock (priv->status_mutex);
	}

	if (error != NULL) {
		g_error_free (error);
	}
	g_object_unref (loader);

	g_mutex_lock (priv->status_mutex);
	priv->load_thread = NULL;
	g_mutex_unlock (priv->status_mutex);

	return NULL;
}

void
eog_image_load (EogImage *img, EogImageLoadMode mode)
{
	EogImagePrivate *priv;

	g_return_if_fail (EOG_IS_IMAGE (img));

	priv = EOG_IMAGE (img)->priv;

	if (priv->status == EOG_IMAGE_STATUS_LOADED) {
		g_assert (priv->image != NULL);
		eog_image_cache_reload (img);
		g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_LOADING_FINISHED], 0);
		g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_LOADING_INFO_FINISHED], 0);
	}
	else if (priv->status == EOG_IMAGE_STATUS_FAILED) {
		g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_LOADING_FAILED], 0, "");		
	}
	else if (priv->status == EOG_IMAGE_STATUS_UNKNOWN) { 
		g_assert (priv->image == NULL);
		
                /* make sure the object isn't destroyed while we try to load it */
		g_object_ref (img);

		/* initialize data fields for progressive loading */
		priv->load_status = EOG_IMAGE_LOAD_STATUS_NONE;
		priv->update_x1 = 10000;
		priv->update_y1 = 10000;
		priv->update_x2 = 0;
		priv->update_y2 = 0;
		priv->error_message = NULL;
		priv->cancel_loading = FALSE;
		priv->mode = mode;

		if (priv->mode == EOG_IMAGE_LOAD_DEFAULT) {
			/* determine if the image should be loaded progressively or not */
			if (gnome_vfs_uri_is_local (priv->uri)) {
				GnomeVFSFileInfo *info;
				GnomeVFSResult result;
				info = gnome_vfs_file_info_new ();
				
				result = gnome_vfs_get_file_info_uri (priv->uri,
								      info,
								      GNOME_VFS_FILE_INFO_DEFAULT);

				if (result != GNOME_VFS_OK) {
					g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_LOADING_FAILED], 
						       0, gnome_vfs_result_to_string (result));
					g_print ("VFS Error: %s\n", gnome_vfs_result_to_string (result));
					return;
				}

				priv->mode = EOG_IMAGE_LOAD_PROGRESSIVE;
				if (((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) != 0) && 
				    (info->size < 1000000))
				{
					priv->mode = EOG_IMAGE_LOAD_COMPLETE;
				}

				gnome_vfs_file_info_unref (info);
			}
			else {
				priv->mode = EOG_IMAGE_LOAD_PROGRESSIVE;
				
			}
		}

		/* start the thread machinery */
		priv->status      = EOG_IMAGE_STATUS_LOADING;
		priv->load_id     = g_timeout_add (CHECK_LOAD_TIMEOUT, check_load_status, img);
		priv->load_thread = g_thread_create (real_image_load, img, TRUE, NULL);
	}
}

gboolean 
eog_image_load_thumbnail (EogImage *img)
{
	EogImagePrivate *priv;

	g_return_val_if_fail (EOG_IS_IMAGE (img), FALSE);

	priv = img->priv;

	if (priv->thumbnail == NULL)
	{
		add_image_to_queue (img);
	}
	
	return (priv->thumbnail != NULL);
}

gboolean 
eog_image_is_animation (EogImage *img)
{
	return FALSE;
}

GdkPixbuf* 
eog_image_get_pixbuf (EogImage *img)
{
	GdkPixbuf *image = NULL;

	g_return_val_if_fail (EOG_IS_IMAGE (img), NULL);

	g_mutex_lock (img->priv->status_mutex);
	image = img->priv->image;
	g_mutex_unlock (img->priv->status_mutex);

	if (image != NULL) {
		g_object_ref (image);
	}
	
	return image;
}

GdkPixbuf* 
eog_image_get_pixbuf_thumbnail (EogImage *img)
{
	g_return_val_if_fail (EOG_IS_IMAGE (img), NULL);

	if (img->priv->thumbnail != 0) {
		g_object_ref (img->priv->thumbnail);
		return img->priv->thumbnail;
	}

	return NULL;
}

void 
eog_image_get_size (EogImage *img, int *width, int *height)
{
	EogImagePrivate *priv;

	g_return_if_fail (EOG_IS_IMAGE (img));

	priv = img->priv;

	*width = priv->width; 
	*height = priv->height;
}

static gboolean
check_progress_sync (gpointer data)
{
	g_signal_emit (G_OBJECT (data), eog_image_signals [SIGNAL_PROGRESS], 0, EOG_IMAGE (data)->priv->progress);
	
	return TRUE;
}

static void
transform_progress_hook_sync (EogTransform *trans, float progress, gpointer hook_data)
{
	EogImagePrivate *priv;

	priv = EOG_IMAGE (hook_data)->priv;

	if (progress != priv->progress) {
		
		priv->progress = progress;

		if (gtk_events_pending ()) {
			gtk_main_iteration ();
		}
	}
}

static void
image_transform (EogImage *img, EogTransform *trans, gboolean is_undo)
{
	EogImagePrivate *priv;
	GdkPixbuf *transformed;
	gboolean modified = FALSE;

	g_return_if_fail (EOG_IS_IMAGE (img));
	g_return_if_fail (EOG_IS_TRANSFORM (trans));

	priv = img->priv;

	if (priv->image != NULL) {
		transformed = eog_transform_apply (trans, priv->image, transform_progress_hook_sync, img);
		
		g_object_unref (priv->image);
		priv->image = transformed;
		priv->width = gdk_pixbuf_get_width (transformed);
		priv->height = gdk_pixbuf_get_height (transformed);
       
		modified = TRUE;
	}

	if (priv->thumbnail != NULL) {
		transformed = eog_transform_apply (trans, priv->thumbnail, transform_progress_hook_sync, img);

		g_object_unref (priv->thumbnail);
		priv->thumbnail = transformed;
       
		modified = TRUE;
	}

	if (modified) {
		priv->modified = TRUE;
#if HAVE_EXIF
		update_exif_data (img);
#endif 
		g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_IMAGE_CHANGED], 0);
	}

	if (priv->trans == NULL) {
		g_object_ref (trans);
		priv->trans = trans;
	}
	else {
		EogTransform *composition;

		composition = eog_transform_compose (priv->trans, trans);

		g_object_unref (priv->trans);
		priv->trans = composition;
	}
	
	if (!is_undo) {
		g_object_ref (trans);
		priv->undo_stack = g_list_prepend (priv->undo_stack, trans);
	}
}


void                
eog_image_transform (EogImage *img, EogTransform *trans)
{
	gint signal_id;

	signal_id = g_timeout_add (CHECK_LOAD_TIMEOUT, check_progress_sync, img);

	image_transform (img, trans, FALSE);

	g_source_remove (signal_id);

	g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_PROGRESS], 0, 1.0);
}

void 
eog_image_undo (EogImage *img)
{
	EogImagePrivate *priv;
	EogTransform *trans;
	EogTransform *inverse;

	g_return_if_fail (EOG_IS_IMAGE (img));

	priv = img->priv;

	if (priv->undo_stack != NULL) {
		trans = EOG_TRANSFORM (priv->undo_stack->data);

		inverse = eog_transform_reverse (trans);

		image_transform (img, inverse, TRUE);

		priv->undo_stack = g_list_delete_link (priv->undo_stack, priv->undo_stack);
		g_object_unref (trans);
		g_object_unref (inverse);

		if (eog_transform_is_identity (priv->trans)) {
			g_object_unref (priv->trans);
			priv->trans = NULL;
		}
	}

	priv->modified = (priv->undo_stack != NULL);
}

/* is_local_uri: 
 * 
 * Checks if the URI points to a local file system. This tests simply
 * if the URI scheme is 'file'. This function is used to ensure that
 * we can write to the path-part of the URI with non-VFS aware
 * filesystem calls.
 */
static gboolean 
is_local_uri (const GnomeVFSURI* uri)
{
	const char *scheme;

	g_return_val_if_fail (uri != NULL, FALSE);

	scheme = gnome_vfs_uri_get_scheme (uri);
	return (g_strcasecmp (scheme, "file") == 0);
}

static char*
get_save_file_type_by_suffix (const char *local_path)
{
	GSList *savable_formats = NULL;
	GSList *it;
	char *file_type = NULL;
	char *suffix;
	GdkPixbufFormat *format;
	char **extension;
	int i;

	/* FIXME: this is probably not unicode friendly */
	suffix = g_strrstr (local_path, ".");
	if (suffix == NULL) {
		return NULL;
	}

	/* skip '.' from the suffix string */
	if (strlen (suffix) > 1) {
		suffix++;
	}

	savable_formats = eog_pixbuf_get_savable_formats ();

	/* iterate over the availabe formats and check for every
	 * possible format suffix if it matches the file suffix */
	for (it = savable_formats; it != NULL && file_type == NULL; it = it->next) {

		format = (GdkPixbufFormat*) it->data;
		extension = gdk_pixbuf_format_get_extensions (format);
		
		for (i = 0; extension[i] != NULL && file_type == NULL; i++) {
			if (g_ascii_strcasecmp (extension[i], suffix) == 0) {
				file_type = gdk_pixbuf_format_get_name (format);
			}
		}

		g_strfreev (extension);
	}
	g_slist_free (savable_formats);

	/* file_type is either NULL or contains the name of the format */
	return file_type;
}

gboolean
eog_image_save (EogImage *img, GnomeVFSURI *uri, GdkPixbufFormat *format, GError **error)
{
	EogImagePrivate *priv;
	char *file;
	char *target_type = NULL;
	char *source_type = NULL;
	gboolean result = FALSE;
	gboolean source_is_local;

	g_return_val_if_fail (EOG_IS_IMAGE (img), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	priv = img->priv;

	/* fail if there is no image to save */
	if (priv->image == NULL) {
		g_set_error (error, EOG_IMAGE_ERROR,
			     EOG_IMAGE_ERROR_NOT_LOADED,
			     _("No image loaded."));
		return FALSE;
	}

	/* Since we rely on some local system calls resp. non-VFS aware libs, 
	 * we can write only to local files.
	 */
	if (!is_local_uri (uri)) {
		g_set_error (error, EOG_IMAGE_ERROR,
			     EOG_IMAGE_ERROR_NOT_LOADED,
			     _("Images can only be saved as local files."));
		return FALSE;
	}

	file = (char*) gnome_vfs_uri_get_path (uri); /* don't free file, it's a static string */

	/* determine type of file to write (eg. 'png') */
	if (format == NULL) {
		if (gnome_vfs_uri_equal (priv->uri, uri)) {
			/* we overwrite the same file, therefore we
			 * can reuse the file type saved in the EogImage object. */
			target_type = g_strdup (priv->file_type);
		}
		else {
			/* find file type for saving, according to filename suffix */
			target_type = get_save_file_type_by_suffix (file);
		}
	}
	else {
		target_type = gdk_pixbuf_format_get_name (format);
	}

	if (target_type == NULL) {
		g_set_error (error, GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
			     _("Unsupported image type for saving."));
		return FALSE;
	}
	
	source_is_local = (priv->uri != NULL && is_local_uri (priv->uri));
	source_type = priv->file_type;
	
	/* Check for some special cases, so that we use always the
	 * least intrusive method for saving the image.
	 */
	if ((g_ascii_strcasecmp (target_type, source_type) == 0) &&
	    !eog_image_is_modified (img))
	{
		/* If image source and target have the same type and
		 * source is not modified in any way => copy file.
		 */
		GnomeVFSResult vfs_result;
		
		vfs_result = gnome_vfs_xfer_uri (priv->uri,
						 uri, 
						 GNOME_VFS_XFER_DEFAULT                 /* copy the data */,
						 GNOME_VFS_XFER_ERROR_MODE_ABORT,       /* abort on all errors */
						 GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE, /* we checked for existing 
											   file already */
						 NULL,                                  /* no progress callback */
						 NULL);
		result = (vfs_result == GNOME_VFS_OK);
		if (!result) {
			g_set_error (error, EOG_IMAGE_ERROR,
				     EOG_IMAGE_ERROR_VFS, 
				     gnome_vfs_result_to_string (vfs_result));
		}
		g_print ("copy image file\n");
	}
#if HAVE_JPEG
	else if ((g_ascii_strcasecmp (source_type, "jpeg") == 0) &&
		 (g_ascii_strcasecmp (target_type, "jpeg") == 0) && 
		 source_is_local) 
	{
		/* If source is a local file and both are jpeg files,
		 * then use lossless transformation through libjpeg.
		 */
		result = eog_image_jpeg_save_lossless (img, uri, error);

		g_print ("loseless saving of %s to %s\n", 
			 gnome_vfs_uri_to_string (priv->uri, GNOME_VFS_URI_HIDE_NONE), file);
	}
#if HAVE_EXIF
	else if ((g_ascii_strcasecmp (target_type, "jpeg") == 0) && priv->exif != NULL) {
		/* If the target is a jpeg file and the image has EXIF
		 * information preserve these by saving through
		 * libjpeg.
		 */
		g_print ("Save through jpeg library.\n");
		result = eog_image_jpeg_save (img, uri, error);
	}
#endif
#endif
	else {
		/* In all other cases: Use default save method
		 * provided by gdk-pixbuf library.
		 */
		g_print ("default save method.\n");
		result = gdk_pixbuf_save (priv->image, file, target_type, error, NULL);
	}
	
	if (result) {
		/* free the transformation since it's not needed anymore */
		/* FIXME: You can't undo prev transformations then anymore. */
		GList *it = priv->undo_stack;
		for (; it != NULL; it = it->next) 
			g_object_unref (G_OBJECT (it->data));
		
		g_list_free (priv->undo_stack);
		priv->undo_stack = NULL;
		if (priv->trans != NULL) {
			g_object_unref (priv->trans);
			priv->trans = NULL;
		}
		priv->modified = FALSE;

		/* update file properties */
		if (priv->uri != NULL) {
			gnome_vfs_uri_unref (priv->uri);
		}
		priv->uri = gnome_vfs_uri_ref (uri);
		if (priv->caption != NULL) {
			g_free (priv->caption);
			priv->caption = NULL;
		}
		if (priv->caption_key != NULL) {
			g_free (priv->caption_key);
			priv->caption_key = NULL;
		}
		if (priv->file_type != NULL) {
			g_free (priv->file_type);
		}
		priv->file_type = g_strdup (target_type);

		g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_IMAGE_CHANGED], 0);
	}

	g_free (target_type);

	return result;
}

/*
 * This function is extracted from 
 * File: nautilus/libnautilus-private/nautilus-file.c
 * Revision: 1.309
 * Author: Darin Adler <darin@bentspoon.com>
 */
static gboolean
have_broken_filenames (void)
{
	static gboolean initialized = FALSE;
	static gboolean broken;
	
	if (initialized) {
		return broken;
	}
	
	broken = g_getenv ("G_BROKEN_FILENAMES") != NULL;
  
	initialized = TRUE;
  
	return broken;
}

/* 
 * This function is inspired by
 * nautilus/libnautilus-private/nautilus-file.c:nautilus_file_get_display_name_nocopy
 * Revision: 1.309
 * Author: Darin Adler <darin@bentspoon.com>
 */
gchar*               
eog_image_get_caption (EogImage *img)
{
	EogImagePrivate *priv;
	char *name;
	char *utf8_name;
	gboolean validated = FALSE;
	gboolean broken_filenames;

	g_return_val_if_fail (EOG_IS_IMAGE (img), NULL);

	priv = img->priv;

	if (priv->uri == NULL) return NULL;

	if (priv->caption != NULL) 
		/* Use cached caption string */
		return priv->caption;

	name = gnome_vfs_uri_extract_short_name (priv->uri);
	
	if (name != NULL && gnome_vfs_uri_is_local (priv->uri)) {
		/* Support the G_BROKEN_FILENAMES feature of
		 * glib by using g_filename_to_utf8 to convert
		 * local filenames to UTF-8. Also do the same
		 * thing with any local filename that does not
		 * validate as good UTF-8.
		 */
		broken_filenames = have_broken_filenames ();
		if (broken_filenames || !g_utf8_validate (name, -1, NULL)) {
			utf8_name = g_locale_to_utf8 (name, -1, NULL, NULL, NULL);
			if (utf8_name != NULL) {
				g_free (name);
				name = utf8_name;
				/* Guaranteed to be correct utf8 here */
				validated = TRUE;
			}
		} 
		else if (!broken_filenames) {
			/* name was valid, no need to re-validate */
			validated = TRUE;
		}
	}
	
	if (!validated && !g_utf8_validate (name, -1, NULL)) {
		if (name == NULL) {
			name = g_strdup ("[Invalid Unicode]");
		}
		else {
			utf8_name = eel_make_valid_utf8 (name);
			g_free (name);
			name = utf8_name;
		}
	}

	priv->caption = name;

	if (priv->caption == NULL) {
		char *short_str;

		short_str = gnome_vfs_uri_extract_short_name (priv->uri);
		if (g_utf8_validate (short_str, -1, NULL)) {
			priv->caption = g_strdup (short_str);
		}
		else {
			priv->caption = g_filename_to_utf8 (short_str, -1, NULL, NULL, NULL);
		}
		g_free (short_str);
	}
	
	return priv->caption;
}

void 
eog_image_free_mem_private (EogImage *image)
{
	EogImagePrivate *priv;
	
	priv = image->priv;

	if (priv->status == EOG_IMAGE_STATUS_LOADING) {
		eog_image_cancel_load (image);
	}
	else {
		if (priv->image != NULL) {
			gdk_pixbuf_unref (priv->image);
			priv->image = NULL;
		}
		
#if HAVE_EXIF
		if (priv->exif != NULL) {
			exif_data_unref (priv->exif);
			priv->exif = NULL;
		}
#endif

		priv->load_status = EOG_IMAGE_LOAD_STATUS_NONE;
		priv->status = EOG_IMAGE_STATUS_UNKNOWN;
	}
}

void
eog_image_free_mem (EogImage *image)
{
	g_return_if_fail (EOG_IS_IMAGE (image));

	if (image->priv->image != NULL) {
		eog_image_cache_remove (image);
		eog_image_free_mem_private (image);
	}
}


const gchar*        
eog_image_get_collate_key (EogImage *img)
{
	EogImagePrivate *priv;

	g_return_val_if_fail (EOG_IS_IMAGE (img), NULL);
	
	priv = img->priv;

	if (priv->caption_key == NULL) {
		char *caption;

		caption = eog_image_get_caption (img);
		priv->caption_key = g_utf8_collate_key (caption, -1);
	}

	return priv->caption_key;
}

void
eog_image_cancel_load (EogImage *img)
{
	EogImagePrivate *priv;

	g_return_if_fail (EOG_IS_IMAGE (img));
	
	priv = img->priv;

	g_mutex_lock (priv->status_mutex);
	if (priv->status == EOG_IMAGE_STATUS_LOADING) {
		priv->cancel_loading = TRUE;
	}
	g_mutex_unlock (priv->status_mutex);
}

gpointer
eog_image_get_exif_information (EogImage *img)
{
	EogImagePrivate *priv;
	gpointer data = NULL;
	
	g_return_val_if_fail (EOG_IS_IMAGE (img), NULL);
	
	priv = img->priv;

#if HAVE_EXIF
	g_mutex_lock (priv->status_mutex);
	exif_data_ref (priv->exif);
	data = priv->exif;
	g_mutex_unlock (priv->status_mutex);
#endif

	return data;
}

gboolean            
eog_image_is_loaded (EogImage *img)
{
	EogImagePrivate *priv;
	gboolean result;

	g_return_val_if_fail (EOG_IS_IMAGE (img), FALSE);

	priv = img->priv;

	g_mutex_lock (priv->status_mutex);
	result = (priv->status == EOG_IMAGE_STATUS_LOADED);
	g_mutex_unlock (priv->status_mutex);
	
	return result;
}

GnomeVFSURI*
eog_image_get_uri (EogImage *img)
{
	g_return_val_if_fail (EOG_IS_IMAGE (img), NULL);
	
	return gnome_vfs_uri_ref (img->priv->uri);
}

gboolean
eog_image_is_modified (EogImage *img)
{
	g_return_val_if_fail (EOG_IS_IMAGE (img), FALSE);
	
	return img->priv->modified;
}

GnomeVFSFileSize
eog_image_get_bytes (EogImage *img)
{
	g_return_val_if_fail (EOG_IS_IMAGE (img), 0);
	
	return img->priv->bytes;
}
