#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "eog-image-loader.h"
#include "cimage.h"
#include "eog-image-list-model.h"
#include <gdk-pixbuf/gdk-pixbuf-loader.h>

struct _EogImageLoaderPrivate {
	EogImageListModel *model;
	
	gint thumb_width;
	gint thumb_height;

	CImage *image;

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

	obj_class->destroy = eog_image_loader_destroy;
}

void 
eog_image_loader_init (EogImageLoader *loader)
{
	EogImageLoaderPrivate *priv;

	priv = g_new0 (EogImageLoaderPrivate, 1);
	priv->model = NULL;
	priv->image = NULL;
	priv->active = FALSE;
	priv->stop_loading = FALSE;

	loader->priv = priv;
}

void 
eog_image_loader_destroy (GtkObject *obj)
{
	EogImageLoader *loader;

	g_return_if_fail (obj != NULL);
	g_return_if_fail (IS_EOG_IMAGE_LOADER (obj));

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
eog_image_loader_set_model (EogImageLoader *loader, EogImageListModel *model)
{
	g_return_if_fail (loader != NULL);
	g_return_if_fail (IS_EOG_IMAGE_LOADER (loader));
	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_EOG_IMAGE_LIST_MODEL (model));

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
	EogImageLoaderPrivate *priv;
	GdkPixbufLoader *pbf_loader;
	int fd;
	int size;
	guchar *buffer;
	gchar *path;
	
	g_return_val_if_fail (loader != NULL, FALSE);
	g_assert (loader->priv->model != NULL);

	priv = loader->priv;

	/* remove idle handler */
	gtk_idle_remove (priv->idle_handler_id);
	priv->idle_handler_id = 0;
		
	while ((priv->image = eog_image_list_model_next_image_to_load (priv->model)) &&
	       !priv->stop_loading) {
		/* create loader */
		pbf_loader = gdk_pixbuf_loader_new ();
		
		/* open file and allocate buffer */
		path = cimage_get_path (priv->image);
#ifdef DEBUG	       
		g_print ("loading: %s ... ", path);
#endif
		fd = open (path, O_RDONLY);
		g_free (path);
		
		if (fd) {
#ifdef DEBUG
			g_print ("ok\n");
#endif
		} else {
#ifdef DEBUG
			g_print ("failed\n");
#endif
			cimage_set_loading_failed (priv->image);
			gdk_pixbuf_loader_close (pbf_loader);
			continue;
		}

		/* loading image from file */
		buffer = g_new0 (guchar, 1024);
		while (!priv->stop_loading) {
			size = read (fd, buffer, 1024);
			if (size == 0) break;
			
			if(!gdk_pixbuf_loader_write(pbf_loader,
						    buffer, size)) {
				cimage_set_loading_failed (priv->image);
				break;
			}

			/* execute gui events if neccessary */
			while (gtk_events_pending ())
				gtk_main_iteration ();
		}
		
		if (!cimage_has_loading_failed (priv->image) &&
		    !priv->stop_loading) {
			/* scale image and set it as thumbnail */
			GdkPixbuf *pbf = gdk_pixbuf_loader_get_pixbuf (pbf_loader);
			GdkPixbuf *thumb = scale_image (loader, pbf);
			
			cimage_set_thumbnail (priv->image, thumb);
			
			gdk_pixbuf_unref (thumb);
			gdk_pixbuf_unref (pbf);
			
			gnome_list_model_interval_changed (GNOME_LIST_MODEL (priv->model), 
							   cimage_get_unique_id (priv->image),
							   1);
		}
		
		/* clean up */
		gdk_pixbuf_loader_close (pbf_loader);
		gtk_object_unref (GTK_OBJECT (priv->image));
		priv->image = NULL;
		g_free (buffer);
		close (fd);	
		
	}
	
	priv->active = FALSE;
	priv->stop_loading = FALSE;
	
	return TRUE;
}

void 
eog_image_loader_start (EogImageLoader *loader)
{
	g_return_if_fail (loader != NULL);

	if (loader->priv->model == NULL) return;

	if (loader->priv->active == FALSE) {
		/* start the loading process */
		loader->priv->idle_handler_id = 
			gtk_idle_add ((GtkFunction)real_image_loading, loader);
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
