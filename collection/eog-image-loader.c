#include <libgnome/gnome-macros.h>
#include <glib-object.h>

#include "eog-image-loader.h"
#include "eog-collection-marshal.h"

enum {
	LOADING_FINISHED,
	LOADING_CANCELED,
	LOADING_FAILED,
	START,
	STOP,
	LAST_SIGNAL
};

static guint eog_image_loader_signals [LAST_SIGNAL];

static void eog_image_loader_class_init (EogImageLoaderClass *klass);
static void eog_image_loader_instance_init (EogImageLoader *loader);

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
	eog_image_loader_signals [START] = 
		g_signal_new ("start",
			      G_TYPE_FROM_CLASS (obj_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogImageLoaderClass, start),
			      NULL, NULL,
			      eog_collection_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
	eog_image_loader_signals [STOP] = 
		g_signal_new ("stop",
			      G_TYPE_FROM_CLASS (obj_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogImageLoaderClass, start),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

void 
eog_image_loader_instance_init (EogImageLoader *loader)
{
}

void 
_eog_image_loader_loading_finished (EogImageLoader *loader, CImage *img)
{
	g_return_if_fail (EOG_IS_IMAGE_LOADER (loader));
	g_return_if_fail (IS_CIMAGE (img));

	g_signal_emit_by_name (G_OBJECT (loader), "loading_finished", img);
}

void 
_eog_image_loader_loading_canceled (EogImageLoader *loader, CImage *img)
{
	g_return_if_fail (EOG_IS_IMAGE_LOADER (loader));
	g_return_if_fail (IS_CIMAGE (img));

	g_signal_emit_by_name (G_OBJECT (loader), "loading_canceled", img);
}

void 
_eog_image_loader_loading_failed (EogImageLoader *loader, CImage *img)
{
	g_return_if_fail (EOG_IS_IMAGE_LOADER (loader));
	g_return_if_fail (IS_CIMAGE (img));

	g_signal_emit_by_name (G_OBJECT (loader), "loading_failed", img);
}

void 
eog_image_loader_start (EogImageLoader *loader, CImage *img)
{
	g_return_if_fail (EOG_IS_IMAGE_LOADER (loader));
	g_return_if_fail (IS_CIMAGE (img));

	g_signal_emit_by_name (G_OBJECT (loader), "start", img);
}

void 
eog_image_loader_stop (EogImageLoader *loader)
{
	g_return_if_fail (EOG_IS_IMAGE_LOADER (loader));

	g_signal_emit_by_name (G_OBJECT (loader), "stop");
}


