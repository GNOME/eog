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
#if HAVE_EXIF
#include <libexif/exif-data.h>
#endif

#include "libeog-marshal.h"
#include "eog-image.h"
#include "eog-pixbuf-util.h"

static GThread     *thread                     = NULL;
static gboolean     thread_running             = FALSE;
static GQueue      *jobs_waiting               = NULL;
static GQueue      *jobs_done                  = NULL;
static gint         dispatch_callbacks_id      = -1;
static GStaticMutex jobs_mutex                 = G_STATIC_MUTEX_INIT;

enum {
	EOG_IMAGE_LOAD_STATUS_NONE     = 0,
	EOG_IMAGE_LOAD_STATUS_PREPARED = 1 << 0,
	EOG_IMAGE_LOAD_STATUS_UPDATED  = 1 << 1,
	EOG_IMAGE_LOAD_STATUS_DONE     = 1 << 2,
	EOG_IMAGE_LOAD_STATUS_FAILED   = 1 << 3,
	EOG_IMAGE_LOAD_STATUS_CANCELLED = 1 << 4,
	EOG_IMAGE_LOAD_STATUS_INFO_DONE = 1 << 5
};

struct _EogImagePrivate {
	GnomeVFSURI *uri;
	EogImageLoadMode mode;

	GdkPixbuf *image;
	GdkPixbuf *thumbnail;
	
	gint width;
	gint height;
#if HAVE_EXIF
	ExifData *exif;
#endif

	gint thumbnail_id;
	
	gboolean modified;

	gchar *caption;
	gchar *caption_key;

	GThread *load_thread;
	gint load_id;
	GMutex *status_mutex;
	gint load_status;
	gboolean cancel_loading;

	/* data which depends on the load status */
	int update_x1;
	int update_y1;
	int update_x2;
	int update_y2;
	char *error_message;
};

enum {
	SIGNAL_LOADING_UPDATE,
	SIGNAL_LOADING_SIZE_PREPARED,
	SIGNAL_LOADING_FINISHED,
	SIGNAL_LOADING_INFO_FINISHED,
	SIGNAL_LOADING_FAILED,
	SIGNAL_LOADING_CANCELLED,
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

#if DEBUG_ASYNC
	g_print ("*** Start thread ***\n");
#endif	

	while (!finished) {

		/* get next image to process */
		g_static_mutex_lock (&jobs_mutex);

		image = EOG_IMAGE (g_queue_pop_head (jobs_waiting));
		g_assert (image != NULL);

		g_static_mutex_unlock (&jobs_mutex);

		/* thumbnail loading/creation  */

		priv = image->priv;

		uri_str = gnome_vfs_uri_to_string (priv->uri, GNOME_VFS_URI_HIDE_NONE);
#if THUMB_DEBUG
		g_message ("uri:  %s", uri_str);
#endif
		path = gnome_thumbnail_path_for_uri (uri_str, GNOME_THUMBNAIL_SIZE_NORMAL);

#if THUMB_DEBUG
		g_message ("thumb path: %s", path);
#endif
		
		if (g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			priv->thumbnail = gdk_pixbuf_new_from_file (path, NULL);
		}
		else {
			GnomeThumbnailFactory *factory;
			GnomeVFSFileInfo *info;
			GnomeVFSResult result;
			
			info = gnome_vfs_file_info_new ();
			result = gnome_vfs_get_file_info_uri (priv->uri, info, 
							      GNOME_VFS_FILE_INFO_DEFAULT |
							      GNOME_VFS_FILE_INFO_GET_MIME_TYPE);
			
			if (result == GNOME_VFS_OK &&
			    (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME) != 0 &&
			    (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) != 0) 
			{
#if THUMB_DEBUG
				g_print ("uri: %s, mtime: %i, mime_type %s\n", uri_str, info->mtime, info->mime_type);
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
			else {
#if THUMB_DEBUG
				g_print ("uri: %s vfs errror: %s\n", uri_str, gnome_vfs_result_to_string (result));
#endif
			}
			
			gnome_vfs_file_info_unref (info);
		}
		
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

	priv = EOG_IMAGE (object)->priv;

	if (priv->uri) {
		gnome_vfs_uri_unref (priv->uri);
		priv->uri = NULL;
	}

	if (priv->image) {
		g_object_unref (priv->image);
		priv->image = NULL;
	}

	if (priv->caption) {
		g_free (priv->caption);
		priv->caption = NULL;
	}

	if (priv->caption_key) {
		g_free (priv->caption_key);
		priv->caption_key = NULL;
	}

	if (priv->status_mutex) {
		g_mutex_free (priv->status_mutex);
		priv->status_mutex = NULL;
	}

#if HAVE_EXIF
	if (priv->exif) {
		exif_data_unref (priv->exif);
		priv->exif = NULL;
	}
#endif
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
eog_image_new_uri (GnomeVFSURI *uri, EogImageLoadMode mode)
{
	EogImage *img;
	EogImagePrivate *priv;
	
	img = EOG_IMAGE (g_object_new (EOG_TYPE_IMAGE, NULL));
	priv = img->priv;

	priv->uri = gnome_vfs_uri_ref (uri);
	priv->mode = mode;
	priv->modified = FALSE;
	priv->load_thread = NULL;

#if OBJECT_WATCH
	n_active_images++;
	g_message ("active image objects: %i", n_active_images);
#endif
	
	return img;
}

EogImage* 
eog_image_new (const char *txt_uri, EogImageLoadMode mode)
{
	GnomeVFSURI *uri;
	EogImage *image;

	uri = gnome_vfs_uri_new (txt_uri);
	image = eog_image_new_uri (uri, mode);
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
		g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_LOADING_FINISHED], 0);
		call_again = FALSE;
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
#define JPEG_MARKER_SOI  0xd8
#define	JPEG_MARKER_APP0 0xe0
#define JPEG_MARKER_APP1 0xe1

typedef enum {
	EXIF_LOADER_READ,
	EXIF_LOADER_READ_SIZE_HIGH_BYTE,
	EXIF_LOADER_READ_SIZE_LOW_BYTE,
	EXIF_LOADER_SKIP_BYTES,
	EXIF_LOADER_EXIF_FOUND,
	EXIF_LOADER_FAILED
} ExifLoaderState;

typedef struct {
	ExifLoaderState state;
	ExifData *data;
	int      size;
	int      last_marker;
	guchar  *buffer_data;
	int      bytes_read;
} ExifLoaderData;

/* This function imitates code from libexif, written by Lutz
 * Müller. See libexif/exif-data.c:exif_data_new_from_file. Here, it
 * can cope with a sequence of data chunks.
 */
static gboolean
exif_loader_write (ExifLoaderData *eld, guchar *buffer, int len)
{
	int i;
	int len_remain;
	
	if (eld->state == EXIF_LOADER_FAILED) return FALSE;
	if (eld->state == EXIF_LOADER_EXIF_FOUND && eld->data != NULL) return FALSE;

	for (i = 0; (i < len) && eld->state != EXIF_LOADER_EXIF_FOUND && eld->state != EXIF_LOADER_FAILED; i++) {

		switch (eld->state) {
		case EXIF_LOADER_SKIP_BYTES:
			eld->size--;
			if (eld->size == 0) {
				eld->state = EXIF_LOADER_READ;
			}
			break;
			
		case EXIF_LOADER_READ_SIZE_HIGH_BYTE:
			eld->size = buffer [i] << 8;
			eld->state = EXIF_LOADER_READ_SIZE_LOW_BYTE;
			break;
			
		case EXIF_LOADER_READ_SIZE_LOW_BYTE:
			eld->size |= buffer [i];
			
			switch (eld->last_marker) {
			case JPEG_MARKER_APP0:
				eld->state = EXIF_LOADER_SKIP_BYTES;
				break;

			case JPEG_MARKER_APP1:
				eld->state = EXIF_LOADER_EXIF_FOUND;
				break;

			default:
				g_assert_not_reached ();
			}

			eld->last_marker = 0;
			break;

		default:
			if (buffer[i] != 0xff) {
				if (buffer [i] == JPEG_MARKER_APP0 ||
				    buffer [i] == JPEG_MARKER_APP1) 
				{
					eld->state = EXIF_LOADER_READ_SIZE_HIGH_BYTE;
					eld->last_marker = buffer [i];
				}
				else if (buffer [i] != JPEG_MARKER_SOI) {
					eld->state = EXIF_LOADER_FAILED;
				}
			}
		}
	}
	
	len_remain = len - i;

	if (eld->state == EXIF_LOADER_EXIF_FOUND && len_remain > 0) {

		if (eld->buffer_data == NULL) {
			eld->buffer_data = g_new0 (guchar, eld->size);
			eld->bytes_read = 0;
		}

		if (eld->bytes_read < eld->size) {
			int cp_len;
			int rest;

			/* the number of bytes we need to copy */
			rest = eld->size - eld->bytes_read;
			cp_len = MIN (rest, len_remain);
			
			g_assert ((cp_len + eld->bytes_read) <= eld->size);

			/* copy memory */
			memcpy (eld->buffer_data + eld->bytes_read, &buffer[i], cp_len);

			eld->bytes_read += cp_len;
		}

		if (eld->bytes_read == eld->size) {
			eld->data = exif_data_new_from_data (eld->buffer_data, eld->size);
		}
	}

	return (eld->data != NULL);
}

#endif 

/* this function runs in it's own thread */
static gpointer
real_image_load (gpointer data)
{
	EogImage *img;
	EogImagePrivate *priv;
	GdkPixbufLoader *loader;
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	guchar *buffer;
	GnomeVFSFileSize bytes_read;
	gboolean failed;
#if HAVE_EXIF
	ExifLoaderData *eld;
#endif

	img = EOG_IMAGE (data);
	priv = img->priv;

#ifdef DEBUG
	g_print ("real image load %s\n", gnome_vfs_uri_to_string (priv->uri, GNOME_VFS_URI_HIDE_NONE));
#endif

	g_assert (priv->image == NULL);
	g_assert (priv->exif == NULL);

	result = gnome_vfs_open_uri (&handle, priv->uri, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		g_mutex_lock (priv->status_mutex);
		priv->load_status |= EOG_IMAGE_LOAD_STATUS_FAILED;
		priv->error_message = (char*) gnome_vfs_result_to_string (result);
		g_mutex_unlock (priv->status_mutex);

		return NULL;
	}
	
	buffer = g_new0 (guchar, 4096);
	loader = gdk_pixbuf_loader_new ();
	failed = FALSE;
#if HAVE_EXIF
	eld = g_new0 (ExifLoaderData, 1);
	eld->state = EXIF_LOADER_READ;
#endif

	if (priv->mode == EOG_IMAGE_LOAD_PROGRESSIVE) {
		g_signal_connect_object (G_OBJECT (loader), "area-updated", (GCallback) load_area_updated, img, 0);
		g_signal_connect_object (G_OBJECT (loader), "size-prepared", (GCallback) load_size_prepared, img, 0);
	}
	
	while (!priv->cancel_loading) {
		result = gnome_vfs_read (handle, buffer, 4096, &bytes_read);
		if (result == GNOME_VFS_ERROR_EOF || bytes_read == 0) {
			break;
		}
		else if (result != GNOME_VFS_OK) {
			failed = TRUE;
			break;
		}
		
		if (!gdk_pixbuf_loader_write (loader, buffer, bytes_read, NULL)) {
			failed = TRUE;
			break;
		}
#if HAVE_EXIF
		if (exif_loader_write (eld, buffer, 4096)) {
			g_mutex_lock (img->priv->status_mutex);
			exif_data_ref (eld->data);
			priv->exif = eld->data;
			priv->load_status |= EOG_IMAGE_LOAD_STATUS_INFO_DONE;
			g_mutex_unlock (img->priv->status_mutex);
		}
#endif
	}

	g_free (buffer);
	gnome_vfs_close (handle);
	
	g_mutex_lock (priv->status_mutex);

	if (failed) {
		if (priv->image != NULL) {
			g_object_unref (priv->image);
			priv->image = NULL;
		}
		priv->load_status |= EOG_IMAGE_LOAD_STATUS_FAILED;
		priv->error_message = NULL; /* FIXME: add descriptive error message */
	}
	else if (priv->cancel_loading) {
		if (priv->image != NULL) {
			g_object_unref (priv->image);
			priv->image = NULL;
		}
		priv->cancel_loading = TRUE;

		priv->load_status |= EOG_IMAGE_LOAD_STATUS_CANCELLED;
	}
	else {
		if (priv->image == NULL) {
			priv->image = gdk_pixbuf_loader_get_pixbuf (loader);
			g_object_ref (priv->image);

			priv->width = gdk_pixbuf_get_width (priv->image);
			priv->height = gdk_pixbuf_get_height (priv->image);
			priv->load_status |= EOG_IMAGE_LOAD_STATUS_PREPARED;
		}

		priv->load_status |= EOG_IMAGE_LOAD_STATUS_DONE;
	}

#if HAVE_EXIF
	if (eld->buffer_data != NULL)
		g_free (eld->buffer_data);
	if (eld->data)
		exif_data_unref (eld->data);
	g_free (eld);
#endif

	priv->load_thread = NULL;
	g_mutex_unlock (priv->status_mutex);
	
	gdk_pixbuf_loader_close (loader, NULL);	
	g_object_unref (loader);

	return NULL;
}

gboolean 
eog_image_load (EogImage *img)
{
	EogImagePrivate *priv;
	gboolean image_loaded;
	gboolean thread_running;

	priv = EOG_IMAGE (img)->priv;

	g_return_val_if_fail (priv->uri != NULL, FALSE);

	g_mutex_lock (priv->status_mutex);
	image_loaded = priv->image != NULL;
	thread_running = priv->load_thread != NULL;
	g_mutex_unlock (priv->status_mutex);

	if (!image_loaded && !thread_running)
	{
		if (priv->mode == EOG_IMAGE_LOAD_DEFAULT) {
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
					return FALSE;
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
		
		g_object_ref (img); /* make sure the object isn't destroyed while we try to load it */
		priv->load_status = EOG_IMAGE_LOAD_STATUS_NONE;
		priv->update_x1 = 10000;
		priv->update_y1 = 10000;
		priv->update_x2 = 0;
		priv->update_y2 = 0;
		priv->error_message = NULL;
		priv->cancel_loading = FALSE;
		priv->load_id = g_timeout_add (CHECK_LOAD_TIMEOUT, check_load_status, img);
		priv->load_thread = g_thread_create (real_image_load, img, TRUE, NULL);
	}

	return (image_loaded && !thread_running);
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


void    
eog_image_rotate_clock_wise (EogImage *img)
{
	EogImagePrivate *priv;
	GdkPixbuf *rotated;

	g_return_if_fail (EOG_IS_IMAGE (img));

	priv = img->priv;
	if (priv->image == NULL) return;

	rotated = eog_pixbuf_rotate_90_cw (priv->image);
	g_object_unref (priv->image);
	priv->image = rotated;

	priv->modified = TRUE;
	g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_IMAGE_CHANGED], 0);
}

void    
eog_image_rotate_counter_clock_wise (EogImage *img)
{
	EogImagePrivate *priv;
	GdkPixbuf *rotated;

	g_return_if_fail (EOG_IS_IMAGE (img));

	priv = img->priv;
	if (priv->image == NULL) return;

	rotated = eog_pixbuf_rotate_90_ccw (priv->image);
	g_object_unref (priv->image);
	priv->image = rotated;

	priv->modified = TRUE;
	g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_IMAGE_CHANGED], 0);	
}

void
eog_image_rotate_180 (EogImage *img)
{
	EogImagePrivate *priv;

	g_return_if_fail (EOG_IS_IMAGE (img));
	
	priv = img->priv;
	if (priv->image == NULL) return;

	eog_pixbuf_rotate_180 (priv->image);
	
	priv->modified = TRUE;
	g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_IMAGE_CHANGED], 0);
}

void
eog_image_flip_horizontal (EogImage *img)
{
	EogImagePrivate *priv;
	
	g_return_if_fail (EOG_IS_IMAGE (img));

	priv = img->priv;
	if (priv->image == NULL) return;

	eog_pixbuf_flip_horizontal (priv->image);

	priv->modified = TRUE;
	g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_IMAGE_CHANGED], 0);	
}

void
eog_image_flip_vertical (EogImage *img)
{
	EogImagePrivate *priv;
	
	g_return_if_fail (EOG_IS_IMAGE (img));

	priv = img->priv;
	if (priv->image == NULL) return;

	eog_pixbuf_flip_vertical (priv->image);

	priv->modified = TRUE;
	g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_IMAGE_CHANGED], 0);	
}

gboolean
eog_image_save (EogImage *img, const GnomeVFSURI *uri, GError **error)
{
	EogImagePrivate *priv;
	char *file;
	char *file_type = NULL;
	GSList *savable_formats = NULL;
	GSList *it;

	g_return_val_if_fail (EOG_IS_IMAGE (img), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	priv = img->priv;

	if (priv->image == NULL) {
		g_set_error (error, EOG_IMAGE_ERROR,
			     EOG_IMAGE_ERROR_NOT_LOADED,
			     _("No image loaded."));
		return FALSE;
	}
	
	/* find file type for saving, according to filename suffix */
	file = (char*) gnome_vfs_uri_get_path (uri); /* don't free file */
	
	savable_formats = eog_pixbuf_get_savable_formats ();
	for (it = savable_formats; it != NULL && file_type == NULL; it = it->next) {
		GdkPixbufFormat *format;
		char **extension;
		int i;

		format = (GdkPixbufFormat*) it->data;
		extension = gdk_pixbuf_format_get_extensions (format);
		
		for (i = 0; extension[i] != NULL && file_type == NULL; i++) {
			char *suffix;
			
			suffix = g_strconcat (".", extension[i], NULL);
			if (g_str_has_suffix (file, suffix)) {
				file_type = gdk_pixbuf_format_get_name (format);
			}

			g_free (suffix);
		}

		g_strfreev (extension);
	}
	g_slist_free (savable_formats);
	
	if (file_type == NULL) {
		g_set_error (error, GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
			     _("Unsupported image type for saving."));
		return FALSE;
	}
	else {
		gboolean result = gdk_pixbuf_save (priv->image, file, file_type, error, NULL);
		g_free (file_type);
		return result;
	}

	return FALSE;
}

gchar*               
eog_image_get_caption (EogImage *img)
{
	EogImagePrivate *priv;

	g_return_val_if_fail (EOG_IS_IMAGE (img), NULL);

	priv = img->priv;

	if (priv->uri == NULL) return NULL;

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
eog_image_free_mem (EogImage *img)
{
	EogImagePrivate *priv;
	
	g_return_if_fail (EOG_IS_IMAGE (img));
	
	priv = img->priv;

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
	gboolean join_thread = FALSE;

	g_return_if_fail (EOG_IS_IMAGE (img));
	
	priv = img->priv;

	g_mutex_lock (priv->status_mutex);
	if (priv->load_thread != NULL) {
		priv->cancel_loading = TRUE;
		join_thread = TRUE;
	}
	g_mutex_unlock (priv->status_mutex);

	if (join_thread)
		g_thread_join (priv->load_thread);
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
