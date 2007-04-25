#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eog-image-cache.h"

struct _EogImageCachePrivate {
	int      capacity;
	GQueue  *queue;
};

#define CACHE_DEBUG  0

G_DEFINE_TYPE (EogImageCache, eog_image_cache, G_TYPE_OBJECT)

static void
eog_image_cache_finalize (GObject *object)
{
	EogImageCache *instance = EOG_IMAGE_CACHE (object);
	
	g_free (instance->priv);
	instance->priv = NULL;

	G_OBJECT_CLASS (eog_image_cache_parent_class)->finalize (object);
}

static void
unref_image (gpointer data, gpointer user_data)
{
	eog_image_data_unref (EOG_IMAGE (data));
}

static void
eog_image_cache_dispose (GObject *object)
{
	EogImageCachePrivate *priv;

	priv = EOG_IMAGE_CACHE (object)->priv;

	if (priv->queue != NULL) {
		g_queue_foreach (priv->queue, (GFunc) unref_image, NULL);
		g_queue_free (priv->queue);
		priv->queue = NULL;
	}

	G_OBJECT_CLASS (eog_image_cache_parent_class)->dispose (object);
}

static void
eog_image_cache_init (EogImageCache *obj)
{
	EogImageCachePrivate *priv;

	priv = g_new0 (EogImageCachePrivate, 1);
	priv->capacity = 0;
	priv->queue = g_queue_new ();

	obj->priv = priv;
}

static void 
eog_image_cache_class_init (EogImageCacheClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->finalize = eog_image_cache_finalize;
	object_class->dispose = eog_image_cache_dispose;
}

EogImageCache*      
eog_image_cache_new (int n)
{
	EogImageCache *cache; 

	cache = g_object_new (EOG_TYPE_IMAGE_CACHE, NULL);
	cache->priv->capacity = MAX (1,n);

	return cache;
}

void 
eog_image_cache_add (EogImageCache *cache, EogImage *image)
{
	EogImageCachePrivate *priv;
	GList *it;

	g_return_if_fail (EOG_IS_IMAGE_CACHE (cache));
	g_return_if_fail (EOG_IS_IMAGE (image));
	
	if (!eog_image_has_data (image, EOG_IMAGE_DATA_IMAGE)) {
		g_warning ("Try to add unloaded image to cache.");
		return;
	}

	priv = cache->priv;

	it = g_queue_find (priv->queue, image);
	if (it != NULL) {
		if (priv->queue->tail != it) { 
			/* image already cached, move it to the end again */
			g_queue_delete_link (priv->queue, it);
			g_queue_push_tail (priv->queue, image);
		}

#if CACHE_DEBUG
		g_print ("Cache re-add: %s\n", eog_image_get_caption (image));
#endif
	}
	else {  /* image is not yet cached */
		eog_image_data_ref (image);
		g_queue_push_tail (priv->queue, image);

#if CACHE_DEBUG
		g_print ("Cache add: %s\n", eog_image_get_caption (image));
#endif

		/* free first image in queue, if capacity is reached */
		if (g_queue_get_length (priv->queue) > priv->capacity) {
			EogImage *head_img;

			head_img = EOG_IMAGE (g_queue_pop_head (priv->queue));
#if CACHE_DEBUG
			g_print ("Cache release: %s\n", eog_image_get_caption (head_img));
#endif
			eog_image_data_unref (head_img);
		}
	}
}

void
eog_image_cache_remove (EogImageCache *cache, EogImage *image)
{
	EogImageCachePrivate *priv;
	GList *it;

	g_return_if_fail (EOG_IS_IMAGE_CACHE (cache));
	g_return_if_fail (EOG_IS_IMAGE (image));

	priv = cache->priv;

	it = g_queue_find (priv->queue, image);
	if (it != NULL) {
#if CACHE_DEBUG
		g_print ("Cache release: %s\n", eog_image_get_caption (image));
#endif
		eog_image_data_unref (image);
		g_queue_delete_link (priv->queue, it);
	}
	else {
		g_warning ("Can't remove image, image not cached.");
	}
}
