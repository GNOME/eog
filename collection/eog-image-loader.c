#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <bonobo/Bonobo.h>

#include "eog-image-loader.h"
#include "cimage.h"
#include <gdk-pixbuf/gdk-pixbuf-loader.h>

enum {
	LOADING_FINISHED,
	LOADING_CANCELED,
	LOADING_FAILED,
	LAST_SIGNAL
};

static guint eog_image_loader_signals [LAST_SIGNAL];

struct _EogImageLoaderPrivate {
	EogCollectionModel *model;
	
	gint thumb_width;
	gint thumb_height;

	gint idle_handler_id;
	gboolean active;
	gboolean stop_loading;
};

GtkObjectClass *parent_class;

static void eog_image_loader_class_init (EogImageLoaderClass *klass);
static void eog_image_loader_init (EogImageLoader *loader);
static void eog_image_loader_destroy (GtkObject *obj);

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
	priv->model = NULL;
	priv->active = FALSE;
	priv->stop_loading = FALSE;

	loader->priv = priv;
}

void 
eog_image_loader_destroy (GtkObject *obj)
{
	EogImageLoader *loader;

	g_return_if_fail (obj != NULL);
	g_return_if_fail (EOG_IS_IMAGE_LOADER (obj));

	loader = EOG_IMAGE_LOADER (obj);

	if(loader->priv->model)
		gtk_object_unref (GTK_OBJECT (loader->priv->model));

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

void 
eog_image_loader_set_model (EogImageLoader *loader, EogCollectionModel *model)
{
	g_return_if_fail (loader != NULL);
	g_return_if_fail (EOG_IS_IMAGE_LOADER (loader));
	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));

	if (loader->priv->model)
		gtk_object_unref (GTK_OBJECT (loader->priv->model));

	loader->priv->model = model;
	gtk_object_ref (GTK_OBJECT (model));
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

static gint
real_image_loading (EogImageLoader *loader)
{
	CORBA_Environment ev;
	EogImageLoaderPrivate *priv;
	GdkPixbufLoader *pbf_loader;
	LoadingContext *lctx;
	Bonobo_Stream_iobuf *buf;
	gint p = 0;

	g_return_val_if_fail (loader != NULL, FALSE);
	g_assert (loader->priv->model != NULL);

	priv = loader->priv;
	CORBA_exception_init (&ev);

	/* remove idle handler */
	gtk_idle_remove (priv->idle_handler_id);
	priv->idle_handler_id = 0;

	/* create buffer */
	buf = Bonobo_Stream_iobuf__alloc ();
	buf->_length = 1024;
	buf->_buffer = g_new0  (guchar, 1024);
	
	while ((lctx = eog_collection_model_get_next_loading_context (priv->model)) &&
	       !priv->stop_loading) {
		
		g_assert (lctx->image != NULL);
		g_assert (lctx->stream != CORBA_OBJECT_NIL);

		/* create loader */
		pbf_loader = gdk_pixbuf_loader_new ();
		
		/* loading image from stream */
		while (!priv->stop_loading) {

			Bonobo_Stream_read (lctx->stream, 1024, &buf, &ev);

			if (buf->_length == 0) break;
			if (ev._major != CORBA_NO_EXCEPTION) break;
			
			if(!gdk_pixbuf_loader_write(pbf_loader,
						    buf->_buffer, buf->_length)) {
				cimage_set_loading_failed (lctx->image);

				gtk_signal_emit (GTK_OBJECT (loader), 
						 eog_image_loader_signals [LOADING_FAILED],
						 lctx->image);
				break;
			}

			/* update gui every 50th time */
			if (p++ % 50 == 0)
				while (gtk_events_pending ())
					gtk_main_iteration ();
		}
		
		if (!cimage_has_loading_failed (lctx->image) &&
		    !priv->stop_loading) {
			/* scale image and set it as thumbnail */
			GdkPixbuf *pbf = gdk_pixbuf_loader_get_pixbuf (pbf_loader);
			GdkPixbuf *thumb = scale_image (loader, pbf);
			
			cimage_set_thumbnail (lctx->image, thumb);
			
			gdk_pixbuf_unref (thumb);
			gdk_pixbuf_unref (pbf);
			
			gtk_signal_emit (GTK_OBJECT (loader),
					 eog_image_loader_signals [LOADING_FINISHED],
					 lctx->image);
		} else if (priv->stop_loading) {
			gtk_signal_emit (GTK_OBJECT (loader),
					 eog_image_loader_signals [LOADING_CANCELED],
					 lctx->image);
		}
		
		/* clean up */
		gdk_pixbuf_loader_close (pbf_loader);
		gtk_object_unref (GTK_OBJECT (lctx->image));
		Bonobo_Unknown_unref (lctx->stream, &ev);
		g_free (lctx);
	}

	g_free (buf->_buffer);
	CORBA_free (buf);

	priv->active = FALSE;
	priv->stop_loading = FALSE;

	CORBA_exception_free (&ev);
	
	return TRUE;
}

void 
eog_image_loader_start (EogImageLoader *loader)
{
	g_return_if_fail (loader != NULL);

	if (loader->priv->model == NULL) return;

	g_print ("eog-image-loader: start image loading\n");

	if (loader->priv->active == FALSE) {
		/* start the loading process */
		loader->priv->idle_handler_id = 
			gtk_idle_add ((GtkFunction)real_image_loading, loader);

		// real_image_loading (loader);
		loader->priv->active = TRUE;
	}
}

void 
eog_image_loader_stop (EogImageLoader *loader)
{
	g_return_if_fail (loader != NULL);
	
	if (loader->priv->active) {
		loader->priv->stop_loading = TRUE;
	}
}
