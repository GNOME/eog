#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-i18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomevfs/gnome-vfs.h>

#include "libeog-marshal.h"
#include "eog-image.h"
#include "eog-pixbuf-util.h"

#define NO_DEBUG

struct _EogImagePrivate {
	char *txt_uri;
	GnomeVFSURI *uri;
	EogImageLoadMode mode;

	GdkPixbuf *image;
	GdkPixbuf *thumbnail;
	
	gint width;
	gint height;

	gint load_idle_id;

	gboolean modified;
};

enum {
	SIGNAL_LOADING_UPDATE,
	SIGNAL_LOADING_SIZE_PREPARED,
	SIGNAL_LOADING_FINISHED,
	SIGNAL_LOADING_FAILED,
	SIGNAL_LOADING_CANCELLED,
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static gint eog_image_signals [SIGNAL_LAST];

GNOME_CLASS_BOILERPLATE (EogImage,
			 eog_image,
			 GObject,
			 G_TYPE_OBJECT);

static void
eog_image_dispose (GObject *object)
{
	EogImagePrivate *priv;

	priv = EOG_IMAGE (object)->priv;

	if (priv->txt_uri) {
		g_free (priv->txt_uri);
		priv->txt_uri = NULL;
	}
	
	if (priv->uri) {
		gnome_vfs_uri_unref (priv->uri);
		priv->uri = NULL;
	}

	if (priv->image) {
		g_object_unref (priv->image);
		priv->image = NULL;
	}
}

static void
eog_image_finalize (GObject *object)
{
	EogImagePrivate *priv;

	priv = EOG_IMAGE (object)->priv;

	g_free (priv);
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
	eog_image_signals [SIGNAL_CHANGED] = 
		g_signal_new ("changed",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogImageClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
eog_image_instance_init (EogImage *img)
{
	EogImagePrivate *priv;

	priv = g_new0 (EogImagePrivate, 1);

	priv->txt_uri = NULL;
	priv->uri = NULL;
	priv->image = NULL;
	priv->thumbnail = NULL;
	priv->width = priv->height = -1;
	priv->modified = FALSE;

	img->priv = priv;
}

EogImage* 
eog_image_new (const char *txt_uri, EogImageLoadMode mode)
{
	EogImage *img;
	EogImagePrivate *priv;
	
	img = EOG_IMAGE (g_object_new (EOG_TYPE_IMAGE, NULL));
	priv = img->priv;

	priv->txt_uri = g_strdup (txt_uri);
	priv->uri = gnome_vfs_uri_new (txt_uri);
	priv->mode = mode;
	priv->modified = FALSE;
	
	return img;
}

GQuark
eog_image_error_quark (void)
{
	static GQuark q = 0;
	if (q == 0)
		q = g_quark_from_static_string ("eog-image-error-quark");
	
	return q;
}


static void 
load_area_updated (GdkPixbufLoader *loader, gint x, gint y, gint width, gint height, gpointer data)
{
	EogImage *img;
	EogImagePrivate *priv;

	img = EOG_IMAGE (data);

#ifdef DEBUG
	g_print ("load_area_updated\n");
#endif

	priv = img->priv;

	if (priv->image == NULL) {
		g_print ("get pixbuf.\n");
		priv->image = gdk_pixbuf_loader_get_pixbuf (loader);
		g_object_ref (priv->image);
	}

#ifdef DEBUG
	g_print ("area_updated: x: %i, y: %i, width: %i, height: %i\n", x, y, width, height);
#endif

	g_signal_emit (img, eog_image_signals [SIGNAL_LOADING_UPDATE], 0, x, y, width, height);
}

static void
load_size_prepared (GdkPixbufLoader *loader, gint width, gint height, gpointer data)
{
	EogImage *img;

	g_return_if_fail (EOG_IS_IMAGE (data));
	
	img = EOG_IMAGE (data);

	img->priv->width = width;
	img->priv->height = height;

	g_signal_emit (img, eog_image_signals [SIGNAL_LOADING_SIZE_PREPARED], 0, width, height);
}

static gboolean
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

	img = EOG_IMAGE (data);
	priv = img->priv;

#ifdef DEBUG
	g_print ("real image load\n");
#endif

	g_assert (priv->image == NULL);

	result = gnome_vfs_open_uri (&handle, priv->uri, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_LOADING_FAILED], 0, gnome_vfs_result_to_string (result));
		g_print ("VFS Error: %s\n", gnome_vfs_result_to_string (result));
		return FALSE;
	}
	
	buffer = g_new0 (guchar, 4096);
	loader = gdk_pixbuf_loader_new ();
	failed = FALSE;

	if (priv->mode == EOG_IMAGE_LOAD_PROGRESSIVE) {
		g_signal_connect (G_OBJECT (loader), "area-updated", (GCallback) load_area_updated, img);
		g_signal_connect (G_OBJECT (loader), "size-prepared", (GCallback) load_size_prepared, img);
	}
	
	while (TRUE) {
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

		if (priv->mode == EOG_IMAGE_LOAD_PROGRESSIVE) {
			while (gtk_events_pending ()) {
				gtk_main_iteration ();
			}
		}
	}

	g_free (buffer);
	gnome_vfs_close (handle);
	
	if (failed) {
		if (priv->image != NULL) {
			g_object_unref (priv->image);
			priv->image = NULL;
		}

		g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_LOADING_FAILED], 0);
	}
	else {
		if (priv->image == NULL) {
			priv->image = gdk_pixbuf_loader_get_pixbuf (loader);
			g_object_ref (priv->image);

			priv->width = gdk_pixbuf_get_width (priv->image);
			priv->height = gdk_pixbuf_get_height (priv->image);
			g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_LOADING_SIZE_PREPARED], 
				       0, priv->width, priv->height);
		}
		
		g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_LOADING_FINISHED], 0);
	}

	gdk_pixbuf_loader_close (loader, NULL);	
	priv->load_idle_id = 0;

	return FALSE;
}

gboolean 
eog_image_load (EogImage *img)
{
	EogImagePrivate *priv;

	priv = EOG_IMAGE (img)->priv;

	g_return_val_if_fail (priv->uri != NULL, FALSE);

	if (priv->image == NULL && priv->load_idle_id == 0)
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
				if ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE != 0) && 
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
		
		priv->load_idle_id = g_idle_add (real_image_load, img);
	}
	
	return (priv->image != NULL);
}

void 
eog_image_load_thumbnail (EogImage *img)
{
}

gboolean 
eog_image_is_animation (EogImage *img)
{
	return FALSE;
}

GdkPixbuf* 
eog_image_get_pixbuf (EogImage *img)
{
	g_return_val_if_fail (EOG_IS_IMAGE (img), NULL);

	if (img->priv->image != 0) {
		g_object_ref (img->priv->image);
		return img->priv->image;
	}
	else {
		return 0;
	}
}

GdkPixbuf* 
eog_image_get_thumbnail_pixbuf (EogImage *img)
{
	g_return_val_if_fail (EOG_IS_IMAGE (img), NULL);

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
	g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_CHANGED], 0);
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
	g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_CHANGED], 0);	
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
	g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_CHANGED], 0);
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
	g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_CHANGED], 0);	
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
	g_signal_emit (G_OBJECT (img), eog_image_signals [SIGNAL_CHANGED], 0);	
}

gboolean
eog_image_save (EogImage *img, const GnomeVFSURI *uri, GError **error)
{
	EogImagePrivate *priv;
	char *file;
	char *file_type = NULL;

	g_return_if_fail (EOG_IS_IMAGE (img));
	g_return_if_fail (uri != NULL);
	g_return_if_fail (error == NULL || *error == NULL);

	priv = img->priv;

	if (priv->image == NULL) {
		g_set_error (error, EOG_IMAGE_ERROR,
			     EOG_IMAGE_ERROR_NOT_LOADED,
			     _("No image loaded."));
		return FALSE;
	}
	
	if (!gnome_vfs_uri_is_local (uri)) {
		g_set_error (error, EOG_IMAGE_ERROR, 
			     EOG_IMAGE_ERROR_SAVE_NOT_LOCAL,
			     _("Can't save non local files."));
		return FALSE;
	}

	file = (char*) gnome_vfs_uri_get_path (uri); /* don't free file */
	
	if (g_str_has_suffix (file, ".png")) {
		file_type = "png";
	}
	else if (g_str_has_suffix (file, ".jpg") ||
		 g_str_has_suffix (file, ".jpeg"))
	{
		file_type = "jpeg";
	}
	else if (g_str_has_suffix (file, ".xpm")) {
		return eog_image_helper_save_xpm (priv->image, file, error);
	}

	if (file_type == NULL) {
		g_set_error (error, GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
			     _("Unsupported image type for saving."));
		return FALSE;
	}
	else {
		return gdk_pixbuf_save (priv->image, file, file_type, error, NULL);
	}

	return FALSE;
}
