#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <bonobo/Bonobo.h>
#include <bonobo/bonobo-moniker-util.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>

#include "eog-image-loader.h"
#include "cimage.h"
#include <gdk-pixbuf/gdk-pixbuf-loader.h>

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

GtkObjectClass *parent_class;

static void eog_image_loader_class_init (EogImageLoaderClass *klass);
static void eog_image_loader_init (EogImageLoader *loader);
static void eog_image_loader_destroy (GtkObject *obj);

static gint setup_next_uri (EogImageLoader *loader);

GtkType 
eog_image_loader_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"EogImageLoader",
			sizeof (EogImageLoader),
			sizeof (EogImageLoaderClass),
			(GtkClassInitFunc)  eog_image_loader_class_init,
			(GtkObjectInitFunc) eog_image_loader_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (
			gtk_object_get_type (), &info);
	}

	return type;
}

void 
eog_image_loader_class_init (EogImageLoaderClass *klass)
{
	GtkObjectClass *obj_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (gtk_object_get_type ());

	eog_image_loader_signals [LOADING_FINISHED] = 
		gtk_signal_new ("loading_finished",
				GTK_RUN_FIRST,
				obj_class->type,
				GTK_SIGNAL_OFFSET (EogImageLoaderClass, loading_finished),
				gtk_marshal_NONE__POINTER, 
				GTK_TYPE_NONE, 1,
				TYPE_CIMAGE);
	eog_image_loader_signals [LOADING_CANCELED] = 
		gtk_signal_new ("loading_canceled",
				GTK_RUN_FIRST,
				obj_class->type,
				GTK_SIGNAL_OFFSET (EogImageLoaderClass, loading_canceled),
				gtk_marshal_NONE__POINTER, 
				GTK_TYPE_NONE, 1,
				TYPE_CIMAGE);
	eog_image_loader_signals [LOADING_FAILED] = 
		gtk_signal_new ("loading_failed",
				GTK_RUN_FIRST,
				obj_class->type,
				GTK_SIGNAL_OFFSET (EogImageLoaderClass, loading_failed),
				gtk_marshal_NONE__POINTER, 
				GTK_TYPE_NONE, 1,
				TYPE_CIMAGE);

	gtk_object_class_add_signals (obj_class, eog_image_loader_signals, LAST_SIGNAL);

	obj_class->destroy = eog_image_loader_destroy;
}

void 
eog_image_loader_init (EogImageLoader *loader)
{
	EogImageLoaderPrivate *priv;

	priv = g_new0 (EogImageLoaderPrivate, 1);
	priv->queue = NULL;
	priv->idle_handler_id = -1;
	priv->cancel_loading = FALSE;

	loader->priv = priv;
}

void 
eog_image_loader_destroy (GtkObject *obj)
{
	EogImageLoader *loader;

	g_return_if_fail (obj != NULL);
	g_return_if_fail (EOG_IS_IMAGE_LOADER (obj));

	loader = EOG_IMAGE_LOADER (obj);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}

EogImageLoader* 
eog_image_loader_new (gint thumb_width, gint thumb_height)
{
	EogImageLoader *loader;

	loader = gtk_type_new (eog_image_loader_get_type ());
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
		gdk_pixbuf_ref (thumb);
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
	gtk_signal_emit (GTK_OBJECT (ctx->loader),
				 eog_image_loader_signals [LOADING_CANCELED],
				 ctx->cimg);
	if (ctx->pbf_loader)
		gdk_pixbuf_loader_close (ctx->pbf_loader);
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
			
			gdk_pixbuf_unref (thumb);
			gdk_pixbuf_unref (pbf);
			
			gtk_signal_emit (GTK_OBJECT (ctx->loader),
					 eog_image_loader_signals [LOADING_FINISHED],
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
		gtk_signal_emit (GTK_OBJECT (ctx->loader),
				 eog_image_loader_signals [LOADING_FAILED],
				 ctx->cimg);
	}

	if (ctx->pbf_loader)
		gdk_pixbuf_loader_close (ctx->pbf_loader);
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
					    buffer, bytes_read)) {
			goto loading_error;
		}
		
		/* update gui every 50th time */
		if (p++ % 50 == 0)
			while (gtk_events_pending ())
				gtk_main_iteration ();
	}

	g_free (buffer);
	gnome_vfs_close (handle);
		
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


