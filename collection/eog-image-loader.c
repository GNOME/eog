#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <bonobo/Bonobo.h>
#include <bonobo/bonobo-moniker-util.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnome/gnome-macros.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "eog-image-loader.h"
#include "cimage.h"
#include "eog-collection-marshal.h"

typedef struct {
	EogImageLoader *loader;
	CImage *cimg;
	GdkPixbufLoader *pbf_loader;
} EILContext;

enum {
	LOADING_FINISHED,
	LOADING_CANCELED,
	LOADING_FAILED,
	LAST_SIGNAL
};

static guint eog_image_loader_signals [LAST_SIGNAL];

struct _EogImageLoaderPrivate {
	gint thumb_width;
	gint thumb_height;

	GList *queue;
	gint idle_handler_id; 
	
	gboolean active;
	gboolean cancel_loading;
};

static void eog_image_loader_class_init (EogImageLoaderClass *klass);
static void eog_image_loader_instance_init (EogImageLoader *loader);
static void eog_image_loader_dispose (GObject *obj);

static gint setup_next_uri (EogImageLoader *loader);

GNOME_CLASS_BOILERPLATE (EogImageLoader, eog_image_loader,
			 GObject, G_TYPE_OBJECT);

void 
eog_image_loader_class_init (EogImageLoaderClass *klass)
{
	GObjectClass *obj_class = (GObjectClass*) klass;

	eog_image_loader_signals [LOADING_FINISHED] = 
		g_signal_new ("loading_finished",
			      G_TYPE_FROM_CLASS (obj_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogImageLoaderClass, loading_finished),
			      NULL, NULL,
			      eog_collection_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
	eog_image_loader_signals [LOADING_CANCELED] = 
		g_signal_new ("loading_canceled",
			      G_TYPE_FROM_CLASS (obj_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogImageLoaderClass, loading_canceled),
			      NULL, NULL,
			      eog_collection_marshal_VOID__POINTER, 
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
	eog_image_loader_signals [LOADING_FAILED] = 
		g_signal_new ("loading_failed",
			      G_TYPE_FROM_CLASS (obj_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogImageLoaderClass, loading_failed),
			      NULL, NULL,
			      eog_collection_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	obj_class->dispose = eog_image_loader_dispose;
}

void 
eog_image_loader_instance_init (EogImageLoader *loader)
{
	EogImageLoaderPrivate *priv;

	priv = g_new0 (EogImageLoaderPrivate, 1);
	priv->queue = NULL;
	priv->idle_handler_id = -1;
	priv->cancel_loading = FALSE;

	loader->priv = priv;
}

void 
eog_image_loader_dispose (GObject *obj)
{
	EogImageLoader *loader;

	g_return_if_fail (obj != NULL);
	g_return_if_fail (EOG_IS_IMAGE_LOADER (obj));

	loader = EOG_IMAGE_LOADER (obj);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (obj));
}

EogImageLoader* 
eog_image_loader_new (gint thumb_width, gint thumb_height)
{
	EogImageLoader *loader;

	loader = EOG_IMAGE_LOADER (g_object_new (EOG_TYPE_IMAGE_LOADER, NULL));
	loader->priv->thumb_width = thumb_width;
	loader->priv->thumb_height = thumb_height;

	return loader;
}

/* Scales a width/height pair to fit in a specific size */
static void
scale_to_fit (int src_w, int src_h, int fit_w, int fit_h, int *dest_w, int *dest_h)
{
	*dest_w = fit_w;
	*dest_h = (src_h * *dest_w) / src_w;

	if (*dest_h > fit_h) {
		*dest_h = fit_h;
		*dest_w = (src_w * *dest_h) / src_h;
	}

	g_assert (*dest_w <= fit_w);
	g_assert (*dest_h <= fit_h);
}

static GdkPixbuf*
scale_image (EogImageLoader *loader, GdkPixbuf *image) 
{
	EogImageLoaderPrivate *priv;
	GdkPixbuf *thumb;
	gint pixbuf_w, pixbuf_h;
	gint thumb_w, thumb_h;
	
	priv = loader->priv;

	if (image) {
		pixbuf_w = gdk_pixbuf_get_width (image);
		pixbuf_h = gdk_pixbuf_get_height (image);
	} else
		pixbuf_w = pixbuf_h = 0;
	
	if (pixbuf_w > priv->thumb_width || pixbuf_h > priv->thumb_height) {
		scale_to_fit (pixbuf_w, pixbuf_h,
			      priv->thumb_width, priv->thumb_height,
			      &thumb_w, &thumb_h);
		thumb = gdk_pixbuf_scale_simple (image, 
						 thumb_w,
						 thumb_h,
						 GDK_INTERP_NEAREST);
	} else {
		thumb_w = pixbuf_w;
		thumb_h = pixbuf_h;
		thumb = image;
		g_object_ref (thumb);
	}			      

	return thumb;
}

static void
loading_canceled (EILContext *ctx)
{
	EogImageLoader *loader;

	g_return_if_fail (ctx != NULL);
	g_return_if_fail (ctx->loader != NULL);

	loader = ctx->loader;
	
#ifdef COLLECTION_DEBUG
	g_message ("Loading canceled\n");
#endif
	g_signal_emit_by_name (G_OBJECT (ctx->loader),
			       "loading_canceled",
			       ctx->cimg);
	if (ctx->pbf_loader)
		gdk_pixbuf_loader_close (ctx->pbf_loader, NULL);
	g_free (ctx);

	loader->priv->cancel_loading = FALSE;
	g_list_free (loader->priv->queue);
	loader->priv->queue = NULL;
	loader->priv->active = FALSE;
}

static void
loading_finished (EILContext *ctx)
{
	EogImageLoader *loader;
	gboolean loading_failed = FALSE;

	loader = ctx->loader;

	if (cimage_has_loading_failed (ctx->cimg))
		loading_failed = TRUE;
	else {
		GdkPixbuf *pbf;
		GdkPixbuf *thumb;
		
		/* scale loaded image */
		pbf = gdk_pixbuf_loader_get_pixbuf (ctx->pbf_loader);
		
		if (pbf != NULL) {
			thumb = scale_image (loader, pbf);
			
#ifdef COLLECTION_DEBUG
			g_message ("Successfully finished loading for: %s\n", cimage_get_uri (ctx->cimg));
#endif
			
			cimage_set_thumbnail (ctx->cimg, thumb);
			cimage_set_image_dimensions (ctx->cimg,
						     gdk_pixbuf_get_width (pbf),
						     gdk_pixbuf_get_height (pbf));
			
			g_object_unref (thumb);
			g_object_unref (pbf);
			
			g_signal_emit_by_name (G_OBJECT (ctx->loader),
					       "loading_finished",
					       ctx->cimg);
		} else {
			loading_failed = TRUE;
		}
	}
	
	if (loading_failed) {
#ifdef COLLECTION_DEBUG
 		g_message ("Loading failed for: %s\n", cimage_get_uri (ctx->cimg)->text);
#endif
		cimage_set_loading_failed (ctx->cimg);
		g_signal_emit_by_name (G_OBJECT (ctx->loader),
				       "loading_failed",
				       ctx->cimg);
	}
	g_free (ctx);

	setup_next_uri (loader);
}

static gint
load_uri (EILContext *ctx)
{
	EogImageLoaderPrivate *priv;
	guchar *buffer;
	gint p = 0;
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	GnomeVFSFileSize bytes_read;

	priv = ctx->loader->priv;

	/* remove idle handler */
	gtk_idle_remove (priv->idle_handler_id);
	priv->idle_handler_id = -1;

	/* try to obtain BonoboStream */
	result = gnome_vfs_open_uri (&handle, cimage_get_uri (ctx->cimg),
				     GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		cimage_set_loading_failed (ctx->cimg);
		loading_finished (ctx);
		return FALSE;
	}
	g_assert (handle != NULL);

	/* create loader */
	ctx->pbf_loader = gdk_pixbuf_loader_new ();
		
	/* loading image from stream */
	buffer = g_new0 (guchar, 4096);
	while (!priv->cancel_loading) {

		result = gnome_vfs_read (handle, buffer, 
					 4096, &bytes_read);

		if (result == GNOME_VFS_ERROR_EOF) break;

		if (result != GNOME_VFS_OK)
			goto loading_error;

		if (bytes_read == 0) break; /* reached end of stream */
			
		if(!gdk_pixbuf_loader_write(ctx->pbf_loader,
					    buffer, bytes_read, NULL)) {
			goto loading_error;
		}
		
		/* update gui every 50th time */
		if (p++ % 50 == 0)
			while (gtk_events_pending ())
				gtk_main_iteration ();
	}

	g_free (buffer);
	gnome_vfs_close (handle);
	gdk_pixbuf_loader_close (ctx->pbf_loader, NULL);
		
	if (priv->cancel_loading) 
		loading_canceled (ctx);
	else 
		loading_finished (ctx);

	return TRUE;

loading_error: 
	cimage_set_loading_failed (ctx->cimg);
	g_free (buffer);
	gnome_vfs_close (handle);
	loading_finished (ctx);

	return FALSE;
}

static gint
setup_next_uri (EogImageLoader *loader)
{		
	EILContext *ctx = NULL; 
	EogImageLoaderPrivate *priv;

	g_return_val_if_fail (loader != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_IMAGE_LOADER (loader), FALSE);
	
	priv = loader->priv;
	if (priv->idle_handler_id != -1)
		gtk_idle_remove (priv->idle_handler_id);
	priv->idle_handler_id = -1;

	if (priv->queue != NULL) {
		ctx = g_new0 (EILContext, 1);
		ctx->loader = loader;
		ctx->pbf_loader = NULL;
		ctx->cimg = (CImage*) priv->queue->data;
		
		priv->queue = g_list_remove (priv->queue, ctx->cimg);
	
#ifdef COLLECTION_DEBUG
		g_message ("Open image: %s\n", cimage_get_uri (ctx->cimg));
#endif

		priv->idle_handler_id = gtk_idle_add ((GtkFunction) load_uri, ctx);
	} else {
		priv->active = FALSE;
	}

	return TRUE;
}

void 
eog_image_loader_start (EogImageLoader *loader, CImage *img)
{
	EogImageLoaderPrivate *priv;

	g_return_if_fail (loader != NULL);
	g_return_if_fail (EOG_IS_IMAGE_LOADER (loader));

	priv = loader->priv;

	priv->queue = g_list_append (priv->queue, img);

	if (!priv->active) {
		priv->active = TRUE;
		priv->idle_handler_id = gtk_idle_add ((GtkFunction) setup_next_uri, loader);
	}
}

void 
eog_image_loader_stop (EogImageLoader *loader)
{
	g_return_if_fail (loader != NULL);
	g_return_if_fail (EOG_IS_IMAGE_LOADER (loader));

	loader->priv->cancel_loading = TRUE;
}


