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

#include "eog-image-loader-simple.h"
#include "cimage.h"
#include "eog-collection-marshal.h"

typedef struct {
	EogImageLoaderSimple *loader;
	CImage *cimg;
	GdkPixbufLoader *pbf_loader_simple;
} EILContext;

struct _EogImageLoaderSimplePrivate {
	gint thumb_width;
	gint thumb_height;

	GList *queue;
	gint idle_handler_id; 
	
	gboolean active;
	gboolean cancel_loading;
};

static void eog_image_loader_simple_class_init (EogImageLoaderSimpleClass *klass);
static void eog_image_loader_simple_instance_init (EogImageLoaderSimple *loader);
static void eog_image_loader_simple_dispose (GObject *obj);
static void eog_image_loader_simple_finalize (GObject *obj);

static void eog_image_loader_simple_start (EogImageLoader *loader, CImage *img);
static void eog_image_loader_simple_stop (EogImageLoader *loader);

static gint setup_next_uri (EogImageLoaderSimple *loader);

GNOME_CLASS_BOILERPLATE (EogImageLoaderSimple, eog_image_loader_simple,
			 EogImageLoader, EOG_TYPE_IMAGE_LOADER);

void 
eog_image_loader_simple_class_init (EogImageLoaderSimpleClass *klass)
{
	GObjectClass *obj_class;
	EogImageLoaderClass *eil_class;

	obj_class = (GObjectClass*) klass;
	eil_class = (EogImageLoaderClass*) klass;

	obj_class->dispose = eog_image_loader_simple_dispose;
	obj_class->finalize = eog_image_loader_simple_finalize;

	eil_class->start = eog_image_loader_simple_start;
	eil_class->stop = eog_image_loader_simple_stop;
}

void 
eog_image_loader_simple_instance_init (EogImageLoaderSimple *loader)
{
	EogImageLoaderSimplePrivate *priv;

	priv = g_new0 (EogImageLoaderSimplePrivate, 1);
	priv->queue = NULL;
	priv->idle_handler_id = -1;
	priv->cancel_loading = FALSE;

	loader->priv = priv;
}

void 
eog_image_loader_simple_dispose (GObject *obj)
{
	EogImageLoaderSimple *loader;

	g_return_if_fail (EOG_IS_IMAGE_LOADER_SIMPLE (obj));

	loader = EOG_IMAGE_LOADER_SIMPLE (obj);

	/* FIXME: Free resources */

	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (obj));
}

void 
eog_image_loader_simple_finalize (GObject *obj)
{
	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (obj));
}


EogImageLoader*
eog_image_loader_simple_new (gint thumb_width, gint thumb_height)
{
	EogImageLoaderSimple *loader;

	loader = EOG_IMAGE_LOADER_SIMPLE (g_object_new (EOG_TYPE_IMAGE_LOADER_SIMPLE, NULL));
	loader->priv->thumb_width = thumb_width;
	loader->priv->thumb_height = thumb_height;

	return EOG_IMAGE_LOADER (loader);
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
scale_image (EogImageLoaderSimple *loader, GdkPixbuf *image) 
{
	EogImageLoaderSimplePrivate *priv;
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
						 GDK_INTERP_BILINEAR);
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
	EogImageLoaderSimple *loader;

	g_return_if_fail (ctx != NULL);
	g_return_if_fail (ctx->loader != NULL);

	loader = ctx->loader;
	
#ifdef COLLECTION_DEBUG
	g_message ("Loading canceled\n");
#endif
	g_signal_emit_by_name (G_OBJECT (ctx->loader),
			       "loading_canceled",
			       ctx->cimg);
	if (ctx->pbf_loader_simple)
		gdk_pixbuf_loader_close (ctx->pbf_loader_simple, NULL);
	g_free (ctx);

	loader->priv->cancel_loading = FALSE;
	g_list_free (loader->priv->queue);
	loader->priv->queue = NULL;
	loader->priv->active = FALSE;
}

static void
loading_finished (EILContext *ctx)
{
	EogImageLoaderSimple *loader;
	gboolean loading_failed = FALSE;

	loader = ctx->loader;

	if (cimage_has_loading_failed (ctx->cimg))
		loading_failed = TRUE;
	else {
		GdkPixbuf *pbf;
		GdkPixbuf *thumb;
		
		/* scale loaded image */
		pbf = gdk_pixbuf_loader_get_pixbuf (ctx->pbf_loader_simple);
		
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
	EogImageLoaderSimplePrivate *priv;
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
	ctx->pbf_loader_simple = gdk_pixbuf_loader_new ();
		
	/* loading image from stream */
	buffer = g_new0 (guchar, 4096);
	while (!priv->cancel_loading) {

		result = gnome_vfs_read (handle, buffer, 
					 4096, &bytes_read);

		if (result == GNOME_VFS_ERROR_EOF) break;

		if (result != GNOME_VFS_OK)
			goto loading_error;

		if (bytes_read == 0) break; /* reached end of stream */
			
		if(!gdk_pixbuf_loader_write(ctx->pbf_loader_simple,
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
	gdk_pixbuf_loader_close (ctx->pbf_loader_simple, NULL);
		
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
setup_next_uri (EogImageLoaderSimple *loader)
{		
	EILContext *ctx = NULL; 
	EogImageLoaderSimplePrivate *priv;

	g_return_val_if_fail (loader != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_IMAGE_LOADER_SIMPLE (loader), FALSE);
	
	priv = loader->priv;
	if (priv->idle_handler_id != -1)
		gtk_idle_remove (priv->idle_handler_id);
	priv->idle_handler_id = -1;

	if (priv->queue != NULL) {
		ctx = g_new0 (EILContext, 1);
		ctx->loader = loader;
		ctx->pbf_loader_simple = NULL;
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

static void 
eog_image_loader_simple_start (EogImageLoader *loader, CImage *img)
{
	EogImageLoaderSimplePrivate *priv;

	g_return_if_fail (EOG_IS_IMAGE_LOADER_SIMPLE (loader));
	g_return_if_fail (IS_CIMAGE (img));

	priv = EOG_IMAGE_LOADER_SIMPLE (loader)->priv;

	priv->queue = g_list_append (priv->queue, img);

	if (!priv->active) {
		priv->active = TRUE;
		priv->idle_handler_id = gtk_idle_add ((GtkFunction) setup_next_uri, loader);
	}
}

static void 
eog_image_loader_simple_stop (EogImageLoader *loader)
{
	g_return_if_fail (EOG_IS_IMAGE_LOADER_SIMPLE (loader));

	EOG_IMAGE_LOADER_SIMPLE (loader)->priv->cancel_loading = TRUE;
}


