#if HAVE_LIBPREVIEW

#include <libpreview/libpreview.h>

#include "eog-image-loader-preview.h"
#include "cimage.h"
#include "eog-collection-marshal.h"

struct _EogImageLoaderPreviewPrivate {
	gint thumb_width;
	gint thumb_height;
};

typedef struct {
	CImage          *image;
	EogImageLoaderPreview  *loader;
} LoadingContext;

static void eog_image_loader_preview_class_init (EogImageLoaderPreviewClass *klass);
static void eog_image_loader_preview_instance_init (EogImageLoaderPreview *loader);
static void eog_image_loader_preview_dispose (GObject *obj);
static void eog_image_loader_preview_finalize (GObject *obj);

static void eog_image_loader_preview_start (EogImageLoader *loader, CImage *img);
static void eog_image_loader_preview_stop (EogImageLoader *loader);

GNOME_CLASS_BOILERPLATE (EogImageLoaderPreview, eog_image_loader_preview,
			 EogImageLoader, EOG_TYPE_IMAGE_LOADER);

#define USE_ASYNC 1

void 
eog_image_loader_preview_class_init (EogImageLoaderPreviewClass *klass)
{
	GObjectClass *obj_class;
	EogImageLoaderClass *eil_class;
	
	obj_class = (GObjectClass*) klass;
	eil_class = (EogImageLoaderClass*) klass;
	
	obj_class->dispose = eog_image_loader_preview_dispose;
	obj_class->finalize = eog_image_loader_preview_finalize;
	
	eil_class->start = eog_image_loader_preview_start;
	eil_class->stop = eog_image_loader_preview_stop;
}

void 
eog_image_loader_preview_instance_init (EogImageLoaderPreview *loader)
{
	EogImageLoaderPreviewPrivate *priv;
	
	priv = g_new0 (EogImageLoaderPreviewPrivate, 1);

	loader->priv = priv;
}

void 
eog_image_loader_preview_dispose (GObject *obj)
{

	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (obj));
}

void 
eog_image_loader_preview_finalize (GObject *obj)
{
	EogImageLoaderPreview *loader;
	
	preview_cache_async_shutdown ();
	
	loader = EOG_IMAGE_LOADER_PREVIEW (obj);
	
	if (loader->priv != NULL)
		g_free (loader->priv);
	loader->priv = NULL;

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (obj));
}

EogImageLoader*
eog_image_loader_preview_new (gint thumb_width, gint thumb_height)
{
	EogImageLoaderPreview *loader;

	preview_cache_init ();
#if USE_ASYNC
	preview_cache_async_init ();
#endif
	
	loader = EOG_IMAGE_LOADER_PREVIEW (g_object_new (EOG_TYPE_IMAGE_LOADER_PREVIEW, NULL));
	loader->priv->thumb_width = thumb_width;
	loader->priv->thumb_height = thumb_height;

	return EOG_IMAGE_LOADER (loader);
}

static void
loading_finished (PreviewThumbnail *thumb, GError **err, gpointer data)
{
	LoadingContext *context;
	EogImageLoaderPreview *loader;
	PreviewThumbnailStatus status;
	GdkPixbuf *pixbuf;
	CImage *image;
	
	context = (LoadingContext*) data;
	image = context->image;
	loader = context->loader;
	g_free (context);

	status = preview_thumbnail_get_status (thumb);

	if (status == PREVIEW_THUMBNAIL_LOADED) {
		pixbuf = preview_thumbnail_get_pixbuf (thumb);
		cimage_set_thumbnail (image, pixbuf);
		cimage_set_image_dimensions (image, 
					     gdk_pixbuf_get_width (pixbuf),
					     gdk_pixbuf_get_height (pixbuf));
		g_object_unref (G_OBJECT (pixbuf));
		g_object_unref (G_OBJECT (thumb));

		_eog_image_loader_loading_finished (EOG_IMAGE_LOADER (loader), image);
	}
	else {
		cimage_set_loading_failed (image);

		g_object_unref (G_OBJECT (thumb));
		
		_eog_image_loader_loading_failed (EOG_IMAGE_LOADER (loader), image);
	}
}

static void
loading_canceled (PreviewThumbnail *thumb, GError **err, gpointer data)
{
	LoadingContext *context;
	EogImageLoaderPreview *loader;
	CImage *image;
	
	context = (LoadingContext*) data;
	image = context->image;
	loader = context->loader;

	g_free (context);
	g_object_unref (G_OBJECT (thumb));

	_eog_image_loader_loading_canceled (EOG_IMAGE_LOADER (loader), image);
}

static void 
eog_image_loader_preview_start (EogImageLoader *loader, CImage *image)
{
	EogImageLoaderPreviewPrivate *priv;
	PreviewThumbnail *thumb;
	GnomeVFSURI *uri;
	LoadingContext *context;

	g_return_if_fail (EOG_IS_IMAGE_LOADER_PREVIEW (loader));
	g_return_if_fail (IS_CIMAGE (image));

	priv = EOG_IMAGE_LOADER_PREVIEW (loader)->priv;

	context = g_new0 (LoadingContext, 1);
	context->loader = EOG_IMAGE_LOADER_PREVIEW (loader);
	context->image = image;

	uri = cimage_get_uri (image);
	thumb = preview_thumbnail_new_uri (uri, priv->thumb_width, priv->thumb_height);
#if USE_ASYNC
	preview_cache_thumbnail_request_async (thumb,
					       loading_finished,
					       loading_canceled,
					       NULL,
					       context);
#else
	preview_cache_thumbnail_request (thumb, NULL);

	loading_finished (thumb, NULL, context); 
#endif

	gnome_vfs_uri_unref (uri);
}					       

static void 
eog_image_loader_preview_stop (EogImageLoader *loader)
{
#if USE_ASYNC
	preview_cache_cancel_existing_jobs ();
#endif
}


#endif
